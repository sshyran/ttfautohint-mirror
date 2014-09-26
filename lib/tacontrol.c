/* tacontrol.c */

/*
 * Copyright (C) 2014 by Werner Lemberg.
 *
 * This file is part of the ttfautohint library, and may only be used,
 * modified, and distributed under the terms given in `COPYING'.  By
 * continuing to use, modify, or distribute this file you indicate that you
 * have read `COPYING' and understand and accept it fully.
 *
 * The file `COPYING' mentioned in the previous paragraph is distributed
 * with the ttfautohint library.
 */

#include "ta.h"

#include <locale.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h> /* for llrb.h */

#include "llrb.h" /* a red-black tree implementation */
#include "tacontrol-bison.h"

Control*
TA_control_new(Control_Type type,
               long font_idx,
               long glyph_idx,
               number_range* point_set,
               double x_shift,
               double y_shift,
               number_range* ppem_set)
{
  Control* control;


  control = (Control*)malloc(sizeof (Control));
  if (!control)
    return NULL;

  control->type = type;

  control->font_idx = font_idx;
  control->glyph_idx = glyph_idx;
  control->points = number_set_reverse(point_set);

  switch (control->type)
  {
  case Control_Delta_before_IUP:
  case Control_Delta_after_IUP:
    /* we round shift values to multiples of 1/(2^CONTROL_DELTA_SHIFT) */
    control->x_shift = (char)(x_shift * CONTROL_DELTA_FACTOR
                              + (x_shift > 0 ? 0.5 : -0.5));
    control->y_shift = (char)(y_shift * CONTROL_DELTA_FACTOR
                              + (y_shift > 0 ? 0.5 : -0.5));
    break;

  case Control_One_Point_Segment:
    /* not implemented yet */
    control->x_shift = 0;
    control->y_shift = 0;
    break;
  }

  control->ppems = number_set_reverse(ppem_set);
  control->next = NULL;

  return control;
}


Control*
TA_control_prepend(Control* list,
                   Control* element)
{
  if (!element)
    return list;

  element->next = list;

  return element;
}


Control*
TA_control_reverse(Control* list)
{
  Control* cur;


  cur = list;
  list = NULL;

  while (cur)
  {
    Control* tmp;


    tmp = cur;
    cur = cur->next;
    tmp->next = list;
    list = tmp;
  }

  return list;
}


void
TA_control_free(Control* control)
{
  while (control)
  {
    Control* tmp;


    number_set_free(control->points);
    number_set_free(control->ppems);

    tmp = control;
    control = control->next;
    free(tmp);
  }
}


sds
control_show_line(FONT* font,
                  Control* control)
{
  char glyph_name_buf[64];
  char* points_buf = NULL;
  char* ppems_buf = NULL;

  sds s;

  FT_Face face;


  s = sdsempty();

  if (!control)
    goto Exit;

  if (control->font_idx >= font->num_sfnts)
    goto Exit;

  face = font->sfnts[control->font_idx].face;
  glyph_name_buf[0] = '\0';
  if (FT_HAS_GLYPH_NAMES(face))
    FT_Get_Glyph_Name(face, control->glyph_idx, glyph_name_buf, 64);

  points_buf = number_set_show(control->points, -1, -1);
  if (!points_buf)
    goto Exit;
  ppems_buf = number_set_show(control->ppems, -1, -1);
  if (!ppems_buf)
    goto Exit;

  switch (control->type)
  {
  case Control_Delta_before_IUP:
    /* not implemented yet */
    break;

  case Control_Delta_after_IUP:
    /* display glyph index if we don't have a glyph name */
    if (*glyph_name_buf)
      s = sdscatprintf(s, "%ld %s p %s x %.20g y %.20g @ %s",
                       control->font_idx,
                       glyph_name_buf,
                       points_buf,
                       (double)control->x_shift / CONTROL_DELTA_FACTOR,
                       (double)control->y_shift / CONTROL_DELTA_FACTOR,
                       ppems_buf);
    else
      s = sdscatprintf(s, "%ld %ld p %s x %.20g y %.20g @ %s",
                       control->font_idx,
                       control->glyph_idx,
                       points_buf,
                       (double)control->x_shift / CONTROL_DELTA_FACTOR,
                       (double)control->y_shift / CONTROL_DELTA_FACTOR,
                       ppems_buf);
    break;

  case Control_One_Point_Segment:
    /* not implemented yet */
    break;
  }

Exit:
  free(points_buf);
  free(ppems_buf);

  return s;
}


char*
TA_control_show(FONT* font)
{
  sds s;
  size_t len;
  char* res;

  Control* control = font->control;


  s = sdsempty();

  while (control)
  {
    sds d;


    /* append current line to buffer, followed by a newline character */
    d = control_show_line(font, control);
    if (!d)
    {
      sdsfree(s);
      return NULL;
    }
    s = sdscatsds(s, d);
    sdsfree(d);
    s = sdscat(s, "\n");

    control = control->next;
  }

  if (!s)
    return NULL;

  /* we return an empty string if there is no data */
  len = sdslen(s) + 1;
  res = (char*)malloc(len);
  if (res)
    memcpy(res, s, len);

  sdsfree(s);

  return res;
}


/* Parse control instructions in `font->control_buf'. */

TA_Error
TA_control_parse_buffer(FONT* font,
                        char** error_string_p,
                        unsigned int* errlinenum_p,
                        char** errline_p,
                        char** errpos_p)
{
  int bison_error;

  Control_Context context;


  /* nothing to do if no data */
  if (!font->control_buf)
  {
    font->control = NULL;
    return TA_Err_Ok;
  }

  TA_control_scanner_init(&context, font);
  if (context.error)
    goto Fail;
  /* this is `yyparse' in disguise */
  bison_error = TA_control_parse(&context);
  TA_control_scanner_done(&context);

  if (bison_error)
  {
    if (bison_error == 2)
      context.error = TA_Err_Control_Allocation_Error;

Fail:
    font->control = NULL;

    if (context.error == TA_Err_Control_Allocation_Error
        || context.error == TA_Err_Control_Flex_Error)
    {
      *errlinenum_p = 0;
      *errline_p = NULL;
      *errpos_p = NULL;
      if (context.errmsg)
        *error_string_p = strdup(context.errmsg);
      else
        *error_string_p = strdup(TA_get_error_message(context.error));
    }
    else
    {
      int i, ret;
      char auxbuf[128];

      char* buf_end;
      char* p_start;
      char* p_end;


      /* construct data for `errline_p' */
      buf_end = font->control_buf + font->control_len;

      p_start = font->control_buf;
      if (context.errline_num > 1)
      {
        i = 1;
        while (p_start < buf_end)
        {
          if (*p_start++ == '\n')
          {
            i++;
            if (i == context.errline_num)
              break;
          }
        }
      }

      p_end = p_start;
      while (p_end < buf_end)
      {
        if (*p_end == '\n')
          break;
        p_end++;
      }
      *errline_p = strndup(p_start, p_end - p_start);

      /* construct data for `error_string_p' */
      if (context.error == TA_Err_Control_Invalid_Font_Index)
        sprintf(auxbuf, " (valid range is [%ld;%ld])",
                0L,
                font->num_sfnts);
      else if (context.error == TA_Err_Control_Invalid_Glyph_Index)
        sprintf(auxbuf, " (valid range is [%ld;%ld])",
                0L,
                font->sfnts[context.font_idx].face->num_glyphs);
      else if (context.error == TA_Err_Control_Invalid_Shift)
        sprintf(auxbuf, " (valid interval is [%g;%g])",
                CONTROL_DELTA_SHIFT_MIN,
                CONTROL_DELTA_SHIFT_MAX);
      else if (context.error == TA_Err_Control_Invalid_Range)
        sprintf(auxbuf, " (values must be within [%ld;%ld])",
                context.number_set_min,
                context.number_set_max);
      else
        auxbuf[0] = '\0';

      ret = asprintf(error_string_p, "%s%s",
                     *context.errmsg ? context.errmsg
                                     : TA_get_error_message(context.error),
                     auxbuf);
      if (ret == -1)
        *error_string_p = NULL;

      if (errline_p)
        *errpos_p = *errline_p + context.errline_pos_left - 1;
      else
        *errpos_p = NULL;

      *errlinenum_p = context.errline_num;
    }
  }
  else
    font->control = context.result;

  return context.error;
}


/* node structure for control instruction data */

typedef struct Node Node;
struct Node
{
  LLRB_ENTRY(Node) entry;
  Ctrl ctrl;
};


/* comparison function for our red-black tree */

static int
nodecmp(Node* e1,
        Node* e2)
{
  long diff;


  /* sort by font index ... */
  diff = e1->ctrl.font_idx - e2->ctrl.font_idx;
  if (diff)
    goto Exit;

  /* ... then by glyph index ... */
  diff = e1->ctrl.glyph_idx - e2->ctrl.glyph_idx;
  if (diff)
    goto Exit;

  /* ... then by ppem ... */
  diff = e1->ctrl.ppem - e2->ctrl.ppem;
  if (diff)
    goto Exit;

  /* ... then by point index */
  diff = e1->ctrl.point_idx - e2->ctrl.point_idx;

Exit:
  /* https://graphics.stanford.edu/~seander/bithacks.html#CopyIntegerSign */
  return (diff > 0) - (diff < 0);
}


/* the red-black tree function body */
typedef struct control_data control_data;

LLRB_HEAD(control_data, Node);

/* no trailing semicolon in the next line */
LLRB_GENERATE_STATIC(control_data, Node, entry, nodecmp)


void
TA_control_free_tree(FONT* font)
{
  control_data* control_data_head = (control_data*)font->control_data_head;

  Node* node;
  Node* next_node;


  if (!control_data_head)
    return;

  for (node = LLRB_MIN(control_data, control_data_head);
       node;
       node = next_node)
  {
    next_node = LLRB_NEXT(control_data, control_data_head, node);
    LLRB_REMOVE(control_data, control_data_head, node);
    free(node);
  }

  free(control_data_head);
}


TA_Error
TA_control_build_tree(FONT* font)
{
  Control* control = font->control;
  control_data* control_data_head;
  int emit_newline = 0;


  /* nothing to do if no data */
  if (!control)
  {
    font->control_data_head = NULL;
    return TA_Err_Ok;
  }

  control_data_head = (control_data*)malloc(sizeof (control_data));
  if (!control_data_head)
    return FT_Err_Out_Of_Memory;

  LLRB_INIT(control_data_head);

  while (control)
  {
    Control_Type type = control->type;
    long font_idx = control->font_idx;
    long glyph_idx = control->glyph_idx;
    char x_shift = control->x_shift;
    char y_shift = control->y_shift;

    number_set_iter ppems_iter;
    int ppem;


    ppems_iter.range = control->ppems;
    ppem = number_set_get_first(&ppems_iter);

    while (ppems_iter.range)
    {
      number_set_iter points_iter;
      int point_idx;


      points_iter.range = control->points;
      point_idx = number_set_get_first(&points_iter);

      while (points_iter.range)
      {
        Node* node;
        Node* val;


        node = (Node*)malloc(sizeof (Node));
        if (!node)
          return FT_Err_Out_Of_Memory;

        node->ctrl.type = type;
        node->ctrl.font_idx = font_idx;
        node->ctrl.glyph_idx = glyph_idx;
        node->ctrl.ppem = ppem;
        node->ctrl.point_idx = point_idx;
        node->ctrl.x_shift = x_shift;
        node->ctrl.y_shift = y_shift;

        val = LLRB_INSERT(control_data, control_data_head, node);
        if (val)
          free(node);
        if (val && font->debug)
        {
          /* entry is already present; we ignore it */
          Control d;
          number_range ppems;
          number_range points;

          sds s;


          /* construct Control entry for debugging output */
          ppems.start = ppem;
          ppems.end = ppem;
          ppems.next = NULL;
          points.start = point_idx;
          points.end = point_idx;
          points.next = NULL;

          d.type = type;
          d.font_idx = font_idx;
          d.glyph_idx = glyph_idx;
          d.points = &points;
          d.x_shift = x_shift;
          d.y_shift = y_shift;
          d.ppems = &ppems;
          d.next = NULL;

          s = control_show_line(font, &d);
          if (s)
          {
            fprintf(stderr, "Control instruction %s ignored.\n", s);
            sdsfree(s);
          }

          emit_newline = 1;
        }

        point_idx = number_set_get_next(&points_iter);
      }

      ppem = number_set_get_next(&ppems_iter);
    }

    control = control->next;
  }

  if (font->debug && emit_newline)
    fprintf(stderr, "\n");

  font->control_data_head = control_data_head;
  font->control_data_cur = LLRB_MIN(control_data, control_data_head);

  return TA_Err_Ok;
}


/* the next functions are intended to restrict the use of LLRB stuff */
/* related to control instructions to this file, */
/* providing a means to access the data sequentially */

void
TA_control_get_next(FONT* font)
{
  Node* node = (Node*)font->control_data_cur;


  if (!node)
    return;

  node = LLRB_NEXT(control_data, /* unused */, node);

  font->control_data_cur = node;
}


const Ctrl*
TA_control_get_ctrl(FONT* font)
{
  Node* node = (Node*)font->control_data_cur;


  return node ? &node->ctrl : NULL;
}

/* end of tacontrol.c */
