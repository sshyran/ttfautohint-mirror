/* tabytecode.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#include "ta.h"
#include "tabytecode.h"


/* a simple macro to emit bytecode instructions */
#define BCI(code) *(bufp++) = (code)

/* we increase the stack depth by amount */
#define ADDITIONAL_STACK_ELEMENTS 20


#ifdef TA_DEBUG
int _ta_debug = 1;
int _ta_debug_disable_horz_hints;
int _ta_debug_disable_vert_hints;
int _ta_debug_disable_blue_hints;
void* _ta_debug_hints;
#endif


typedef struct Hints_Record_ {
  FT_UInt size;
  FT_Byte* buf;
} Hints_Record;


static FT_Error
TA_sfnt_compute_global_hints(SFNT* sfnt,
                             FONT* font)
{
  FT_Error error;
  FT_Face face = sfnt->face;
  FT_UInt enc;
  FT_UInt idx;

  static const FT_Encoding latin_encs[] =
  {
    FT_ENCODING_UNICODE,
    FT_ENCODING_APPLE_ROMAN,
    FT_ENCODING_ADOBE_STANDARD,
    FT_ENCODING_ADOBE_LATIN_1,

    FT_ENCODING_NONE /* end of list */
  };


  error = ta_loader_init(font->loader);
  if (error)
    return error;

  /* try to select a latin charmap */
  for (enc = 0; latin_encs[enc] != FT_ENCODING_NONE; enc++)
  {
    error = FT_Select_Charmap(face, latin_encs[enc]);
    if (!error)
      break;
  }

  /* load latin glyph `a' to trigger all initializations */
  idx = FT_Get_Char_Index(face, 'a');
  error = ta_loader_load_glyph(font->loader, face, idx, 0);

  return error;
}


static FT_Error
TA_table_build_cvt(FT_Byte** cvt,
                   FT_ULong* cvt_len,
                   SFNT* sfnt,
                   FONT* font)
{
  TA_LatinAxis haxis;
  TA_LatinAxis vaxis;

  FT_UInt i;
  FT_UInt buf_len;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;

  FT_Error error;


  error = TA_sfnt_compute_global_hints(sfnt, font);
  if (error)
    return error;

  /* XXX check validity of pointers */
  haxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[0];
  vaxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[1];

  buf_len = 2 * (2
                 + haxis->width_count
                 + vaxis->width_count
                 + 2 * vaxis->blue_count);

  /* buffer length must be a multiple of four */
  len = (buf_len + 3) & ~3;
  buf = (FT_Byte*)malloc(len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* pad end of buffer with zeros */
  buf[len - 1] = 0x00;
  buf[len - 2] = 0x00;
  buf[len - 3] = 0x00;

  buf_p = buf;

  if (haxis->width_count > 0)
  {
    *(buf_p++) = HIGH(haxis->widths[0].org);
    *(buf_p++) = LOW(haxis->widths[0].org);
  }
  else
  {
    *(buf_p++) = 0;
    *(buf_p++) = 50;
  }
  if (vaxis->width_count > 0)
  {
    *(buf_p++) = HIGH(vaxis->widths[0].org);
    *(buf_p++) = LOW(vaxis->widths[0].org);
  }
  else
  {
    *(buf_p++) = 0;
    *(buf_p++) = 50;
  }

  for (i = 0; i < haxis->width_count; i++)
  {
    if (haxis->widths[i].org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(haxis->widths[i].org);
    *(buf_p++) = LOW(haxis->widths[i].org);
  }

  for (i = 0; i < vaxis->width_count; i++)
  {
    if (vaxis->widths[i].org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->widths[i].org);
    *(buf_p++) = LOW(vaxis->widths[i].org);
  }

  for (i = 0; i < vaxis->blue_count; i++)
  {
    if (vaxis->blues[i].ref.org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->blues[i].ref.org);
    *(buf_p++) = LOW(vaxis->blues[i].ref.org);
  }

  for (i = 0; i < vaxis->blue_count; i++)
  {
    if (vaxis->blues[i].shoot.org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->blues[i].shoot.org);
    *(buf_p++) = LOW(vaxis->blues[i].shoot.org);
  }

#if 0
  TA_LOG(("--------------------------------------------------\n"));
  TA_LOG(("glyph %d:\n", idx));
  ta_glyph_hints_dump_edges(_ta_debug_hints);
  ta_glyph_hints_dump_segments(_ta_debug_hints);
  ta_glyph_hints_dump_points(_ta_debug_hints);
#endif

  *cvt = buf;
  *cvt_len = buf_len;

  return FT_Err_Ok;

Err:
  free(buf);
  return TA_Err_Hinter_Overflow;
}


FT_Error
TA_sfnt_build_cvt_table(SFNT* sfnt,
                        FONT* font)
{
  FT_Error error;

  FT_Byte* cvt_buf;
  FT_ULong cvt_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    return error;

  error = TA_table_build_cvt(&cvt_buf, &cvt_len, sfnt, font);
  if (error)
    return error;

  /* in case of success, `cvt_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_cvt, cvt_len, cvt_buf);
  if (error)
  {
    free(cvt_buf);
    return error;
  }

  return FT_Err_Ok;
}


/* the horizontal and vertical standard widths */
#define CVT_HORZ_STANDARD_WIDTH_OFFSET(font) 0
#define CVT_VERT_STANDARD_WIDTH_OFFSET(font) \
          CVT_HORZ_STANDARD_WIDTH_OFFSET(font) + 1

/* the horizontal stem widths */
#define CVT_HORZ_WIDTHS_OFFSET(font) \
          CVT_VERT_STANDARD_WIDTH_OFFSET(font) + 1
#define CVT_HORZ_WIDTHS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[0].width_count

/* the vertical stem widths */
#define CVT_VERT_WIDTHS_OFFSET(font) \
          CVT_HORZ_WIDTHS_OFFSET(font) + CVT_HORZ_WIDTHS_SIZE(font)
#define CVT_VERT_WIDTHS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].width_count

/* the number of blue zones */
#define CVT_BLUES_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].blue_count

/* the blue zone values for flat and round edges */
#define CVT_BLUE_REFS_OFFSET(font) \
          CVT_VERT_WIDTHS_OFFSET(font) + CVT_VERT_WIDTHS_SIZE(font)
#define CVT_BLUE_SHOOTS_OFFSET(font) \
          CVT_BLUE_REFS_OFFSET(font) + CVT_BLUES_SIZE(font)


/* symbolic names for storage area locations */

#define sal_counter 0
#define sal_limit sal_counter + 1
#define sal_scale sal_limit + 1
#define sal_0x10000 sal_scale + 1
#define sal_is_extra_light sal_0x10000 + 1
#define sal_segment_offset sal_is_extra_light + 1 /* must be last */


/* we need the following macro */
/* so that `func_name' doesn't get replaced with its #defined value */
/* (as defined in `tabytecode.h') */

#define FPGM(func_name) fpgm_ ## func_name


/* in the comments below, the top of the stack (`s:') */
/* is the rightmost element; the stack is shown */
/* after the instruction on the same line has been executed */

/*
 * bci_compute_stem_width
 *
 *   This is the equivalent to the following code from function
 *   `ta_latin_compute_stem_width':
 *
 *      dist = ABS(width)
 *
 *      if (stem_is_serif
 *          && dist < 3*64)
 *         || is_extra_light:
 *        return width
 *      else if base_is_round:
 *        if dist < 80
 *          dist = 64
 *      else:
 *        dist = MIN(56, dist)
 *
 *      delta = ABS(dist - std_width)
 *
 *      if delta < 40:
 *        dist = MIN(48, std_width)
 *        goto End
 *
 *      if dist < 3*64:
 *        delta = dist
 *        dist = FLOOR(dist)
 *        delta = delta - dist
 *
 *        if delta < 10:
 *          dist = dist + delta
 *        else if delta < 32:
 *          dist = dist + 10
 *        else if delta < 54:
 *          dist = dist + 54
 *        else
 *          dist = dist + delta
 *      else
 *        dist = ROUND(dist)
 *
 *    End:
 *      if width < 0:
 *        dist = -dist
 *      return dist
 *
 *
 * in: width
 *     stem_is_serif
 *     base_is_round
 * out: new_width
 * sal: is_extra_light
 * CVT: std_width
 */

unsigned char FPGM(bci_compute_stem_width_a) [] = {

  PUSHB_1,
    bci_compute_stem_width,
  FDEF,

  DUP,
  ABS, /* s: base_is_round stem_is_serif width dist */

  DUP,
  PUSHB_1,
    3*64,
  LT, /* dist < 3*64 */

  PUSHB_1,
    4,
  MINDEX, /* s: base_is_round width dist (dist<3*64) stem_is_serif */
  AND, /* stem_is_serif && dist < 3*64 */

  PUSHB_1,
    sal_is_extra_light,
  RS,
  OR, /* (stem_is_serif && dist < 3*64) || is_extra_light */

  IF, /* s: base_is_round width dist */
    POP,
    SWAP,
    POP, /* s: width */

  ELSE,
    ROLL, /* s: width dist base_is_round */
    IF, /* s: width dist */
      DUP,
      PUSHB_1,
        80,
      LT, /* dist < 80 */
      IF, /* s: width dist */
        POP,
        PUSHB_1,
          64, /* dist = 64 */
      EIF,

    ELSE,
      PUSHB_1,
        56,
      MIN, /* dist = min(56, dist) */
    EIF,

    DUP, /* s: width dist dist */
    PUSHB_1,

};

/*    %c, index of std_width */

unsigned char FPGM(bci_compute_stem_width_b) [] = {

    RCVT,
    SUB,
    ABS, /* s: width dist delta */

    PUSHB_1,
      40,
    LT, /* delta < 40 */
    IF, /* s: width dist */
      POP,
      PUSHB_2,
        48,

};

/*      %c, index of std_width */

unsigned char FPGM(bci_compute_stem_width_c) [] = {

      RCVT,
      MIN, /* dist = min(48, std_width) */

    ELSE,
      DUP, /* s: width dist dist */
      PUSHB_1,
        3*64,
      LT, /* dist < 3*64 */
      IF,
        DUP, /* s: width delta dist */
        FLOOR, /* dist = FLOOR(dist) */
        DUP, /* s: width delta dist dist */
        ROLL,
        ROLL, /* s: width dist delta dist */
        SUB, /* delta = delta - dist */

        DUP, /* s: width dist delta delta */
        PUSHB_1,
          10,
        LT, /* delta < 10 */
        IF, /* s: width dist delta */
          ADD, /* dist = dist + delta */

        ELSE,
          DUP,
          PUSHB_1,
            32,
          LT, /* delta < 32 */
          IF,
            POP,
            PUSHB_1,
              10,
            ADD, /* dist = dist + 10 */

          ELSE,
            DUP,
            PUSHB_1,
              54,
            LT, /* delta < 54 */
            IF,
              POP,
              PUSHB_1,
                54,
              ADD, /* dist = dist + 54 */

            ELSE,
              ADD, /* dist = dist + delta */

            EIF,
          EIF,
        EIF,

        ELSE,
          PUSHB_1,
            32,
          ADD,
          FLOOR, /* dist = round(dist) */

        EIF,
      EIF,

      SWAP, /* s: dist width */
      PUSHB_1,
        0,
      LT, /* width < 0 */
      IF,
        NEG, /* dist = -dist */
      EIF,
    EIF,
  EIF,

  ENDF,

};


/*
 * bci_loop
 *
 *   Take a range and a function number and apply the function to all
 *   elements of the range.  The called function must not change the
 *   stack.
 *
 * in: func_num
 *     end
 *     start
 *
 * uses: sal_counter (counter initialized with `start')
 *       sal_limit (`end')
 */

unsigned char FPGM(bci_loop) [] = {

  PUSHB_1,
    bci_loop,
  FDEF,

  ROLL,
  ROLL, /* s: func_num start end */
  PUSHB_1,
    sal_limit,
  SWAP,
  WS,

  PUSHB_1,
    sal_counter,
  SWAP,
  WS,

/* start_loop: */
  PUSHB_1,
    sal_counter,
  RS,
  PUSHB_1,
    sal_limit,
  RS,
  LTEQ, /* start <= end */
  IF, /* s: func_num */
    DUP,
    CALL,
    PUSHB_2,
      1,
      sal_counter,
    RS,
    ADD, /* start = start + 1 */
    PUSHB_1,
      sal_counter,
    SWAP,
    WS,

    PUSHB_1,
      22,
    NEG,
    JMPR, /* goto start_loop */
  ELSE,
    POP,
  EIF,

  ENDF,

};


/*
 * bci_cvt_rescale
 *
 *   Rescale CVT value by a given factor.
 *
 * uses: sal_counter (CVT index)
 *       sal_scale (scale in 16.16 format)
 */

unsigned char FPGM(bci_cvt_rescale) [] = {

  PUSHB_1,
    bci_cvt_rescale,
  FDEF,

  PUSHB_1,
    sal_counter,
  RS,
  DUP,
  RCVT,
  PUSHB_1,
    sal_scale,
  RS,
  MUL, /* CVT * scale * 2^10 */
  PUSHB_1,
    sal_0x10000,
  RS,
  DIV, /* CVT * scale */

  WCVTP,

  ENDF,

};


/*
 * bci_loop_sal_assign
 *
 *   Apply the WS instruction repeatedly to stack data.
 *
 * in: counter (N)
 *     offset
 *     data_0
 *     data_1
 *     ...
 *     data_(N-1)
 *
 * uses: bci_sal_assign
 */

unsigned char FPGM(bci_sal_assign) [] = {

  PUSHB_1,
    bci_sal_assign,
  FDEF,

  DUP,
  ROLL, /* s: offset offset data */
  WS,

  PUSHB_1,
    1,
  ADD, /* s: (offset + 1) */

  ENDF,

};

unsigned char FPGM(bci_loop_sal_assign) [] = {

  PUSHB_1,
    bci_loop_sal_assign,
  FDEF,

  /* process the stack, popping off the elements in a loop */
  PUSHB_1,
    bci_sal_assign,
  LOOPCALL,

  /* clean up stack */
  POP,

  ENDF,

};


/*
 * bci_blue_round
 *
 *   Round a blue ref value and adjust its corresponding shoot value.
 *
 * uses: sal_counter (CVT index)
 *
 */

unsigned char FPGM(bci_blue_round_a) [] = {

  PUSHB_1,
    bci_blue_round,
  FDEF,

  PUSHB_1,
    sal_counter,
  RS,
  DUP,
  RCVT, /* s: ref_idx ref */

  DUP,
  PUSHB_1,
    32,
  ADD,
  FLOOR,
  SWAP, /* s: ref_idx round(ref) ref */

  PUSHB_2,

};

/*  %c, blue_count */

unsigned char FPGM(bci_blue_round_b) [] = {

    4,
  CINDEX,
  ADD, /* s: ref_idx round(ref) ref shoot_idx */
  DUP,
  RCVT, /* s: ref_idx round(ref) ref shoot_idx shoot */

  ROLL, /* s: ref_idx round(ref) shoot_idx shoot ref */
  SWAP,
  SUB, /* s: ref_idx round(ref) shoot_idx dist */
  DUP,
  ABS, /* s: ref_idx round(ref) shoot_idx dist delta */

  DUP,
  PUSHB_1,
    32,
  LT, /* delta < 32 */
  IF,
    POP,
    PUSHB_1,
      0, /* delta = 0 */

  ELSE,
    PUSHB_1,
      48,
    LT, /* delta < 48 */
    IF,
      PUSHB_1,
        32, /* delta = 32 */

    ELSE,
      PUSHB_1,
        64, /* delta = 64 */
    EIF,
  EIF,

  SWAP, /* s: ref_idx round(ref) shoot_idx delta dist */
  PUSHB_1,
    0,
  LT, /* dist < 0 */
  IF,
    NEG, /* delta = -delta */
  EIF,

  PUSHB_1,
    3,
  CINDEX,
  ADD, /* s: ref_idx round(ref) shoot_idx (round(ref) + delta) */

  WCVTP,
  WCVTP,

  ENDF,

};


/*
 * bci_hint_glyph
 *
 *   This is the top-level glyph hinting function
 *   which parses the arguments on the stack and calls subroutines.
 *
 * in: num_edges2blues (M)
 *       edge2blue_0.first_segment
 *                  .is_serif
 *                  .is_round
 *                  .num_remaining_segments (N)
 *                  .remaining_segments_0
 *                                     _1
 *                                     ...
 *                                     _(N-1)
 *                _1
 *                ...
 *                _(M-1)
 *
 *     num_edges2links (P)
 *       edge2link_0.first_segment
 *                  .num_remaining_segments (Q)
 *                  .remaining_segments_0
 *                                     _1
 *                                     ...
 *                                     _(Q-1)
 *                _1
 *                ...
 *                _(P-1)
 *
 * uses: bci_edge2blue
 *       bci_edge2link
 */

unsigned char FPGM(bci_remaining_edges) [] = {

  PUSHB_1,
    bci_remaining_edges,
  FDEF,

  POP, /* XXX remaining segment */

  ENDF,

};

unsigned char FPGM(bci_edge2blue) [] = {

  PUSHB_1,
    bci_edge2blue,
  FDEF,

  POP, /* XXX first_segment */
  POP, /* XXX is_serif */
  POP, /* XXX is_round */
  PUSHB_1,
    bci_remaining_edges,
  LOOPCALL,

  ENDF,

};

unsigned char FPGM(bci_edge2link) [] = {

  PUSHB_1,
    bci_edge2link,
  FDEF,

  POP, /* XXX first_segment */
  PUSHB_1,
    bci_remaining_edges,
  LOOPCALL,

  ENDF,

};

unsigned char FPGM(bci_hint_glyph) [] = {

  PUSHB_1,
    bci_hint_glyph,
  FDEF,

  PUSHB_1,
    bci_edge2blue,
  LOOPCALL,

  PUSHB_1,
    bci_edge2link,
  LOOPCALL,

  ENDF,

};


#define COPY_FPGM(func_name) \
          memcpy(buf_p, fpgm_ ## func_name, \
                 sizeof (fpgm_ ## func_name)); \
          buf_p += sizeof (fpgm_ ## func_name) \

static FT_Error
TA_table_build_fpgm(FT_Byte** fpgm,
                    FT_ULong* fpgm_len,
                    FONT* font)
{
  FT_UInt buf_len;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;


  buf_len = sizeof (FPGM(bci_compute_stem_width_a))
            + 1
            + sizeof (FPGM(bci_compute_stem_width_b))
            + 1
            + sizeof (FPGM(bci_compute_stem_width_c))
            + sizeof (FPGM(bci_loop))
            + sizeof (FPGM(bci_cvt_rescale))
            + sizeof (FPGM(bci_sal_assign))
            + sizeof (FPGM(bci_loop_sal_assign))
            + sizeof (FPGM(bci_blue_round_a))
            + 1
            + sizeof (FPGM(bci_blue_round_b))
            + sizeof (FPGM(bci_remaining_edges))
            + sizeof (FPGM(bci_edge2blue))
            + sizeof (FPGM(bci_edge2link))
            + sizeof (FPGM(bci_hint_glyph));
  /* buffer length must be a multiple of four */
  len = (buf_len + 3) & ~3;
  buf = (FT_Byte*)malloc(len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* pad end of buffer with zeros */
  buf[len - 1] = 0x00;
  buf[len - 2] = 0x00;
  buf[len - 3] = 0x00;

  /* copy font program into buffer and fill in the missing variables */
  buf_p = buf;

  COPY_FPGM(bci_compute_stem_width_a);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  COPY_FPGM(bci_compute_stem_width_b);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  COPY_FPGM(bci_compute_stem_width_c);
  COPY_FPGM(bci_loop);
  COPY_FPGM(bci_cvt_rescale);
  COPY_FPGM(bci_sal_assign);
  COPY_FPGM(bci_loop_sal_assign);
  COPY_FPGM(bci_blue_round_a);
  *(buf_p++) = (unsigned char)CVT_BLUES_SIZE(font);
  COPY_FPGM(bci_blue_round_b);
  COPY_FPGM(bci_remaining_edges);
  COPY_FPGM(bci_edge2blue);
  COPY_FPGM(bci_edge2link);
  COPY_FPGM(bci_hint_glyph);

  *fpgm = buf;
  *fpgm_len = buf_len;

  return FT_Err_Ok;
}


FT_Error
TA_sfnt_build_fpgm_table(SFNT* sfnt,
                         FONT* font)
{
  FT_Error error;

  FT_Byte* fpgm_buf;
  FT_ULong fpgm_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    return error;

  error = TA_table_build_fpgm(&fpgm_buf, &fpgm_len, font);
  if (error)
    return error;

  /* in case of success, `fpgm_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_fpgm, fpgm_len, fpgm_buf);
  if (error)
  {
    free(fpgm_buf);
    return error;
  }

  return FT_Err_Ok;
}


/* the `prep' instructions */

#define PREP(snippet_name) prep_ ## snippet_name

/* we often need 0x10000 which can't be pushed directly onto the stack, */
/* thus we provide it in the storage area */

unsigned char PREP(store_0x10000) [] = {

  PUSHB_1,
    sal_0x10000,
  PUSHW_2,
    0x08, /* 0x800 */
    0x00,
    0x08, /* 0x800 */
    0x00,
  MUL, /* 0x10000 */
  WS,

};

unsigned char PREP(align_top_a) [] = {

  /* optimize the alignment of the top of small letters to the pixel grid */

  PUSHB_1,

};

/*  %c, index of alignment blue zone */

unsigned char PREP(align_top_b) [] = {

  RCVT,
  DUP,
  DUP,
  PUSHB_1,
    40,
  ADD,
  FLOOR, /* fitted = FLOOR(scaled + 40) */
  DUP, /* s: scaled scaled fitted fitted */
  ROLL,
  NEQ,
  IF, /* s: scaled fitted */
    PUSHB_1,
      sal_0x10000,
    RS,
    MUL, /* scaled in 16.16 format */
    SWAP,
    DIV, /* (fitted / scaled) in 16.16 format */

    PUSHB_1,
      sal_scale,
    SWAP,
    WS,

};

unsigned char PREP(loop_cvt_a) [] = {

    /* loop over vertical CVT entries */
    PUSHB_4,

};

/*    %c, first vertical index */
/*    %c, last vertical index */

unsigned char PREP(loop_cvt_b) [] = {

      bci_cvt_rescale,
      bci_loop,
    CALL,

    /* loop over blue refs */
    PUSHB_4,

};

/*    %c, first blue ref index */
/*    %c, last blue ref index */

unsigned char PREP(loop_cvt_c) [] = {

      bci_cvt_rescale,
      bci_loop,
    CALL,

    /* loop over blue shoots */
    PUSHB_4,

};

/*    %c, first blue shoot index */
/*    %c, last blue shoot index */

unsigned char PREP(loop_cvt_d) [] = {

      bci_cvt_rescale,
      bci_loop,
    CALL,
  EIF,

};

unsigned char PREP(compute_extra_light_a) [] = {

  /* compute (vertical) `extra_light' flag */
  PUSHB_3,
    sal_is_extra_light,
    40,

};

/*  %c, index of vertical standard_width */

unsigned char PREP(compute_extra_light_b) [] = {

  RCVT,
  GT, /* standard_width < 40 */
  WS,

};

unsigned char PREP(round_blues_a) [] = {

  /* use discrete values for blue zone widths */
  PUSHB_4,

};

/*  %c, first blue ref index */
/*  %c, last blue ref index */

unsigned char PREP(round_blues_b) [] = {

    bci_blue_round,
    bci_loop,
  CALL

};


/* XXX talatin.c: 1671 */
/* XXX talatin.c: 1708 */
/* XXX talatin.c: 2182 */


#define COPY_PREP(snippet_name) \
          memcpy(buf_p, prep_ ## snippet_name, \
                 sizeof (prep_ ## snippet_name)); \
          buf_p += sizeof (prep_ ## snippet_name);

static FT_Error
TA_table_build_prep(FT_Byte** prep,
                    FT_ULong* prep_len,
                    FONT* font)
{
  TA_LatinAxis vaxis;
  TA_LatinBlue blue_adjustment;
  FT_UInt i;

  FT_UInt buf_len;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;


  vaxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[1];
  blue_adjustment = NULL;

  for (i = 0; i < vaxis->blue_count; i++)
  {
    if (vaxis->blues[i].flags & TA_LATIN_BLUE_ADJUSTMENT)
    {
      blue_adjustment = &vaxis->blues[i];
      break;
    }
  }

  buf_len = sizeof (PREP(store_0x10000));

  if (blue_adjustment)
    buf_len += sizeof (PREP(align_top_a))
               + 1
               + sizeof (PREP(align_top_b))
               + sizeof (PREP(loop_cvt_a))
               + 2
               + sizeof (PREP(loop_cvt_b))
               + 2
               + sizeof (PREP(loop_cvt_c))
               + 2
               + sizeof (PREP(loop_cvt_d));

  buf_len += sizeof (PREP(compute_extra_light_a))
             + 1
             + sizeof (PREP(compute_extra_light_b));

  if (CVT_BLUES_SIZE(font))
    buf_len += sizeof (PREP(round_blues_a))
               + 2
               + sizeof (PREP(round_blues_b));

  /* buffer length must be a multiple of four */
  len = (buf_len + 3) & ~3;
  buf = (FT_Byte*)malloc(len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* pad end of buffer with zeros */
  buf[len - 1] = 0x00;
  buf[len - 2] = 0x00;
  buf[len - 3] = 0x00;

  /* copy cvt program into buffer and fill in the missing variables */
  buf_p = buf;

  COPY_PREP(store_0x10000);

  if (blue_adjustment)
  {
    COPY_PREP(align_top_a);
    *(buf_p++) = (unsigned char)(CVT_BLUE_SHOOTS_OFFSET(font)
                                 + blue_adjustment - vaxis->blues);
    COPY_PREP(align_top_b);

    COPY_PREP(loop_cvt_a);
    *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_VERT_WIDTHS_OFFSET(font)
                                 + CVT_VERT_WIDTHS_SIZE(font) - 1);
    COPY_PREP(loop_cvt_b);
    *(buf_p++) = (unsigned char)CVT_BLUE_REFS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_REFS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1);
    COPY_PREP(loop_cvt_c);
    *(buf_p++) = (unsigned char)CVT_BLUE_SHOOTS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_SHOOTS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1);
    COPY_PREP(loop_cvt_d);
  }

  COPY_PREP(compute_extra_light_a);
  *(buf_p++) = (unsigned char)CVT_VERT_STANDARD_WIDTH_OFFSET(font);
  COPY_PREP(compute_extra_light_b);

  if (CVT_BLUES_SIZE(font))
  {
    COPY_PREP(round_blues_a);
    *(buf_p++) = (unsigned char)CVT_BLUE_REFS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_REFS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1);
    COPY_PREP(round_blues_b);
  }

  *prep = buf;
  *prep_len = buf_len;

  return FT_Err_Ok;
}


FT_Error
TA_sfnt_build_prep_table(SFNT* sfnt,
                         FONT* font)
{
  FT_Error error;

  FT_Byte* prep_buf;
  FT_ULong prep_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    return error;

  error = TA_table_build_prep(&prep_buf, &prep_len, font);
  if (error)
    return error;

  /* in case of success, `prep_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_prep, prep_len, prep_buf);
  if (error)
  {
    free(prep_buf);
    return error;
  }

  return FT_Err_Ok;
}


/* we store the segments in the storage area; */
/* each segment record consists of the first and last point */

static FT_Byte*
TA_sfnt_build_glyph_segments(SFNT* sfnt,
                             FONT* font,
                             FT_Byte* bufp)
{
  TA_GlyphHints hints = &font->loader->hints;
  TA_AxisHints axis = &hints->axis[TA_DIMENSION_VERT];
  TA_Point points = hints->points;
  TA_Segment segments = axis->segments;
  TA_Segment seg;
  TA_Segment seg_limit;

  FT_UInt* args;
  FT_UInt* arg;
  FT_UInt num_args;
  FT_UInt nargs;

  FT_Bool need_words = 0;

  FT_UInt i, j;
  FT_UInt num_storage;
  FT_UInt num_stack_elements;


  seg_limit = segments + axis->num_segments;
  num_args = 2 * axis->num_segments + 3;

  /* collect all arguments temporarily in an array (in reverse order) */
  /* so that we can easily split into chunks of 255 args */
  /* as needed by NPUSHB and NPUSHW, respectively */
  args = (FT_UInt*)malloc(num_args * sizeof (FT_UInt));
  if (!args)
    return NULL;

  arg = args + num_args - 1;

  if (axis->num_segments > 0xFF)
    need_words = 1;

  *(arg--) = bci_loop_sal_assign;
  *(arg--) = axis->num_segments * 2;
  *(arg--) = sal_segment_offset;

  for (seg = segments; seg < seg_limit; seg++)
  {
    FT_UInt first = seg->first - points;
    FT_UInt last = seg->last - points;


    *(arg--) = first;
    *(arg--) = last;

    if (first > 0xFF || last > 0xFF)
      need_words = 1;
  }

  /* with most fonts it is very rare */
  /* that any of the pushed arguments is larger than 0xFF, */
  /* thus we refrain from further optimizing this case */

  arg = args;

  if (need_words)
  {
    for (i = 0; i < num_args; i += 255)
    {
      nargs = (num_args - i > 255) ? 255 : num_args - i;

      BCI(NPUSHW);
      BCI(nargs);
      for (j = 0; j < nargs; j++)
      {
        BCI(HIGH(*arg));
        BCI(LOW(*arg));
        arg++;
      }
    }
  }
  else
  {
    for (i = 0; i < num_args; i += 255)
    {
      nargs = (num_args - i > 255) ? 255 : num_args - i;

      BCI(NPUSHB);
      BCI(nargs);
      for (j = 0; j < nargs; j++)
      {
        BCI(*arg);
        arg++;
      }
    }
  }

  BCI(CALL);

  num_storage = sal_segment_offset + axis->num_segments;
  if (num_storage > sfnt->max_storage)
    sfnt->max_storage = num_storage;

  num_stack_elements = ADDITIONAL_STACK_ELEMENTS + num_args;
  if (num_stack_elements > sfnt->max_stack_elements)
    sfnt->max_stack_elements = num_stack_elements;

  free(args);

  return bufp;
}


static FT_Bool
TA_hints_record_is_different(Hints_Record* hints_records,
                             FT_UInt num_hints_records,
                             FT_Byte* start,
                             FT_Byte* end)
{
  Hints_Record last_hints_record;


  if (!hints_records)
    return 1;

  /* we only need to compare with the last hints record */
  last_hints_record = hints_records[num_hints_records - 1];

  if ((FT_UInt)(end - start) != last_hints_record.size)
    return 1;

  if (memcmp(start, last_hints_record.buf, last_hints_record.size))
    return 1;

  return 0;
}


static FT_Error
TA_add_hints_record(Hints_Record** hints_records,
                    FT_UInt* num_hints_records,
                    FT_Byte* start,
                    FT_Byte* end)
{
  Hints_Record* hints_records_new;
  Hints_Record hints_record;
  FT_UInt size;


  size = (FT_UInt)(end - start);

  hints_record.size = size;
  hints_record.buf = (FT_Byte*)malloc(size);
  if (!hints_record.buf)
    return FT_Err_Out_Of_Memory;

  memcpy(hints_record.buf, start, size);

  (*num_hints_records)++;
  hints_records_new =
    (Hints_Record*)realloc(*hints_records, *num_hints_records
                                           * sizeof (Hints_Record));
  if (!hints_records_new)
  {
    free(hints_record.buf);
    (*num_hints_records)--;
    return FT_Err_Out_Of_Memory;
  }
  else
    *hints_records = hints_records_new;

  (*hints_records)[*num_hints_records - 1] = hints_record;

  return FT_Err_Ok;
}


#if 0

static FT_Byte*
TA_sfnt_emit_hinting_set(SFNT* sfnt,
                         Hinting_Set* hinting_set,
                         FT_Byte* bufp)
{
  FT_UInt* args;
  FT_UInt* arg;

  Edge2Blue* edge2blue;
  Edge2Blue* edge2blue_limit;
  Edge2Link* edge2link;
  Edge2Link* edge2link_limit;

  FT_UInt* seg;
  FT_UInt* seg_limit;

  FT_UInt i, j;
  FT_UInt num_args;
  FT_UInt num_stack_elements;


  /* collect all arguments temporarily in an array (in reverse order) */
  /* so that we can easily split into chunks of 255 args */
  /* as needed by NPUSHB and NPUSHW, respectively */
  args = (FT_UInt*)malloc(hinting_set->num_args * sizeof (FT_UInt));
  if (!args)
    return NULL;

  arg = args + hinting_set->num_args - 1;

  *(arg--) = hinting_set->num_edges2blues;

  edge2blue_limit = hinting_set->edges2blues
                    + hinting_set->num_edges2blues;
  for (edge2blue = hinting_set->edges2blues;
       edge2blue < edge2blue_limit;
       edge2blue++)
  {
    *(arg--) = edge2blue->first_segment;
    *(arg--) = edge2blue->is_serif;
    *(arg--) = edge2blue->is_round;
    *(arg--) = edge2blue->num_remaining_segments;

    seg_limit = edge2blue->remaining_segments
                + edge2blue->num_remaining_segments;
    for (seg = edge2blue->remaining_segments; seg < seg_limit; seg++)
      *(arg--) = *seg;
  }

  *(arg--) = hinting_set->num_edges2links;

  edge2link_limit = hinting_set->edges2links
                    + hinting_set->num_edges2links;
  for (edge2link = hinting_set->edges2links;
       edge2link < edge2link_limit;
       edge2link++)
  {
    *(arg--) = edge2link->first_segment;
    *(arg--) = edge2link->num_remaining_segments;

    seg_limit = edge2link->remaining_segments
                + edge2link->num_remaining_segments;
    for (seg = edge2link->remaining_segments; seg < seg_limit; seg++)
      *(arg--) = *seg;
  }

  /* with most fonts it is very rare */
  /* that any of the pushed arguments is larger than 0xFF, */
  /* thus we refrain from further optimizing this case */

  arg = args;

  if (hinting_set->need_words)
  {
    for (i = 0; i < hinting_set->num_args; i += 255)
    {
      num_args = (hinting_set->num_args - i > 255)
                   ? 255
                   : hinting_set->num_args - i;

      BCI(NPUSHW);
      BCI(num_args);
      for (j = 0; j < num_args; j++)
      {
        BCI(HIGH(*arg));
        BCI(LOW(*arg));
        arg++;
      }
    }
  }
  else
  {
    for (i = 0; i < hinting_set->num_args; i += 255)
    {
      num_args = (hinting_set->num_args - i > 255)
                   ? 255
                   : hinting_set->num_args - i;

      BCI(NPUSHB);
      BCI(num_args);
      for (j = 0; j < num_args; j++)
      {
        BCI(*arg);
        arg++;
      }
    }
  }

  free(args);

  num_stack_elements = ADDITIONAL_STACK_ELEMENTS + hinting_set->num_args;
  if (num_stack_elements > sfnt->max_stack_elements)
    sfnt->max_stack_elements = sfnt->max_stack_elements;

  return bufp;
}


static FT_Byte*
TA_sfnt_emit_hinting_sets(SFNT* sfnt,
                          Hinting_Set* hinting_sets,
                          FT_UInt num_hinting_sets,
                          FT_Byte* bufp)
{
  FT_UInt i;
  Hinting_Set* hinting_set;


  hinting_set = hinting_sets;

  /* this instruction is essential for getting correct CVT values */
  /* if horizontal and vertical resolutions differ; */
  /* it assures that the projection vector is set to the y axis */
  /* so that CVT values are handled as being `vertical' */
  BCI(SVTCA_y);

  for (i = 0; i < num_hinting_sets - 1; i++)
  {
    BCI(MPPEM);
    if (hinting_set->size > 0xFF)
    {
      BCI(PUSHW_1);
      BCI(HIGH((hinting_set + 1)->size));
      BCI(LOW((hinting_set + 1)->size));
    }
    else
    {
      BCI(PUSHB_1);
      BCI((hinting_set + 1)->size);
    }
    BCI(LT);
    BCI(IF);
    bufp = TA_sfnt_emit_hinting_set(sfnt, hinting_set, bufp);
    if (!bufp)
      return NULL;
    BCI(ELSE);

    hinting_set++;
  }

  bufp = TA_sfnt_emit_hinting_set(sfnt, hinting_set, bufp);
  if (!bufp)
    return NULL;

  for (i = 0; i < num_hinting_sets - 1; i++)
    BCI(EIF);

  BCI(PUSHB_1);
  BCI(bci_hint_glyph);
  BCI(CALL);

  return bufp;
}

#endif


static void
TA_free_hints_records(Hints_Record* hints_records,
                      FT_UInt num_hints_records)
{
  FT_UInt i;


  for (i = 0; i < num_hints_records; i++)
    free(hints_records[i].buf);

  free(hints_records);
}


static FT_Byte*
TA_hints_recorder_handle_segments(FT_Byte* bufp,
                                  TA_Segment segments,
                                  TA_Edge edge,
                                  FT_Bool* need_words)
{
  TA_Segment seg;
  FT_UInt seg_idx;
  FT_UInt num_segs = 0;
  FT_Bool nw = 0;


  seg_idx = edge->first - segments;
  if (seg_idx > 0xFF)
    nw = 1;

  *(bufp++) = HIGH(seg_idx);
  *(bufp++) = LOW(seg_idx);
  *(bufp++) = edge->flags & TA_EDGE_SERIF;
  *(bufp++) = edge->flags & TA_EDGE_ROUND;

  seg = edge->first->edge_next;
  while (seg != edge->first)
  {
    seg = seg->edge_next;
    num_segs++;
  }
  if (num_segs > 0xFF)
    nw = 1;

  *(bufp++) = HIGH(num_segs);
  *(bufp++) = LOW(num_segs);

  seg = edge->first->edge_next;
  while (seg != edge->first)
  {
    seg_idx = seg - segments;
    seg = seg->edge_next;
    if (seg_idx > 0xFF)
      nw = 1;

    *(bufp++) = HIGH(seg_idx);
    *(bufp++) = LOW(seg_idx);
  }

  if (!*need_words)
    *need_words = nw;

  return bufp;
}


static void
TA_hints_recorder(TA_Action action,
                  TA_GlyphHints hints,
                  TA_Dimension dim,
                  TA_Edge edge1,
                  TA_Edge edge2)
{
  TA_AxisHints axis = &hints->axis[dim];
  TA_Segment segments = axis->segments;

  FT_Byte** buf = (FT_Byte**)hints->user;
  FT_Byte* p = *buf;
  FT_Bool need_words = 0;


  if (dim == TA_DIMENSION_HORZ)
    return;

  *(p++) = (FT_Byte)action;

  switch (action)
  {
  case ta_link:
  case ta_anchor:
  case ta_stem:
    p = TA_hints_recorder_handle_segments(p, segments, edge1, &need_words);
    p = TA_hints_recorder_handle_segments(p, segments, edge2, &need_words);
    break;

  case ta_blue:
  case ta_bound:
  case ta_serif:
  case ta_serif_anchor:
  case ta_serif_link1:
  case ta_serif_link2:
    p = TA_hints_recorder_handle_segments(p, segments, edge1, &need_words);
    break;
  }

  *(p++) = (FT_Byte)need_words;

  *buf = p;
}


static FT_Error
TA_sfnt_build_glyph_instructions(SFNT* sfnt,
                                 FONT* font,
                                 FT_Long idx)
{
  FT_Face face = sfnt->face;
  FT_Error error;

  FT_Byte* ins_buf;
  FT_UInt ins_len;
  FT_Byte* bufp;
  FT_Byte* curp;

  SFNT_Table* glyf_table = &font->tables[sfnt->glyf_idx];
  glyf_Data* data = (glyf_Data*)glyf_table->data;
  GLYPH* glyph = &data->glyphs[idx];

  TA_GlyphHints hints;

  FT_UInt num_hints_records;
  Hints_Record* hints_records;

  FT_UInt size;


  if (idx < 0)
    return FT_Err_Invalid_Argument;

  /* computing the segments is resolution independent, */
  /* thus the pixel size in this call is arbitrary */
  error = FT_Set_Pixel_Sizes(face, 20, 20);
  if (error)
    return error;

  ta_loader_register_hints_recorder(font->loader, NULL, NULL);
  error = ta_loader_load_glyph(font->loader, face, (FT_UInt)idx, 0);
  if (error)
    return error;

  /* do nothing if we have an empty glyph */
  if (!face->glyph->outline.n_contours)
    return FT_Err_Ok;

  hints = &font->loader->hints;

  /* we allocate a buffer which is certainly large enough */
  /* to hold all of the created bytecode instructions; */
  /* later on it gets reallocated to its real size */
  ins_len = hints->num_points * 1000;
  ins_buf = (FT_Byte*)malloc(ins_len);
  if (!ins_buf)
    return FT_Err_Out_Of_Memory;

  /* initialize array with an invalid bytecode */
  /* so that we can easily find the array length at reallocation time */
  memset(ins_buf, INS_A0, ins_len);

  bufp = TA_sfnt_build_glyph_segments(sfnt, font, ins_buf);

  /* now we loop over a large range of pixel sizes */
  /* to find hints records which get pushed onto the bytecode stack */
  num_hints_records = 0;
  hints_records = NULL;

#if DEBUG
  printf("glyph %ld\n", idx);
#endif

  /* we temporarily use `ins_buf' to record the current glyph hints */
  curp = bufp;
  ta_loader_register_hints_recorder(font->loader,
                                    TA_hints_recorder, (void *)&curp);

  for (size = 8; size <= 1000; size++)
  {
    /* rewind buffer pointer for recorder */
    curp = bufp;

    error = FT_Set_Pixel_Sizes(face, size, size);
    if (error)
      goto Err;

    /* calling `ta_loader_load_glyph' uses the */
    /* `TA_hints_recorder' function as a callback, modifying `curp' */
    error = ta_loader_load_glyph(font->loader, face, idx, 0);
    if (error)
      goto Err;

    if (TA_hints_record_is_different(hints_records,
                                     num_hints_records,
                                     bufp, curp))
    {
#if DEBUG
      if (num_hints_records > 0)
      {
        FT_Byte* p;


        printf("  %d:\n", size);
        for (p = bufp; p < curp; p++)
          printf(" %2d", *p);
        printf("\n");
      }
#endif

      error = TA_add_hints_record(&hints_records,
                                  &num_hints_records,
                                  bufp, curp);
      if (error)
        goto Err;
    }
  }

#if 0
  bufp = TA_sfnt_emit_hinting_sets(sfnt,
                                   hinting_sets, num_hinting_sets, bufp);
  if (!bufp)
    return FT_Err_Out_Of_Memory;
#endif

  /* we are done, so reallocate the instruction array to its real size */
  /* (memrchr is a GNU glibc extension, so we do it manually) */
  bufp = ins_buf + ins_len;
  while (*(--bufp) == INS_A0)
    ;
  ins_len = bufp - ins_buf + 1;

  if (ins_len > sfnt->max_instructions)
    sfnt->max_instructions = ins_len;

  glyph->ins_buf = (FT_Byte*)realloc(ins_buf, ins_len);
  glyph->ins_len = ins_len;

  TA_free_hints_records(hints_records, num_hints_records);

  return FT_Err_Ok;

Err:
  TA_free_hints_records(hints_records, num_hints_records);
  free(ins_buf);

  return error;
}


FT_Error
TA_sfnt_build_glyf_hints(SFNT* sfnt,
                         FONT* font)
{
  FT_Face face = sfnt->face;
  FT_Long idx;
  FT_Error error;


  for (idx = 0; idx < face->num_glyphs; idx++)
  {
    error = TA_sfnt_build_glyph_instructions(sfnt, font, idx);
    if (error)
      return error;
  }

  return FT_Err_Ok;
}

/* end of tabytecode.c */
