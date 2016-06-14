/* tastyles.h */

/*
 * Copyright (C) 2014-2016 by Werner Lemberg.
 *
 * This file is part of the ttfautohint library, and may only be used,
 * modified, and distributed under the terms given in `COPYING'.  By
 * continuing to use, modify, or distribute this file you indicate that you
 * have read `COPYING' and understand and accept it fully.
 *
 * The file `COPYING' mentioned in the previous paragraph is distributed
 * with the ttfautohint library.
 */


/* originally file `afstyles.h' (2014-Jan-11) from FreeType */

/* heavily modified 2014 by Werner Lemberg <wl@gnu.org> */


/* The following part can be included multiple times. */
/* Define `STYLE' as needed. */


/*
 * Add new styles here.  The first and second arguments are the
 * style name in lowercase and uppercase, respectively, followed
 * by a description string.  The next arguments are the
 * corresponding writing system, script, blue stringset, and
 * coverage.
 *
 * Note that styles using `TA_COVERAGE_DEFAULT' should always
 * come after styles with other coverages.  Also note that
 * fallback scripts only use `TA_COVERAGE_DEFAULT' for its
 * style.
 *
 * Example:
 *
 *   STYLE(cyrl_dflt, CYRL_DFLT,
 *         "Cyrillic default style",
 *         TA_WRITING_SYSTEM_LATIN,
 *         TA_SCRIPT_CYRL,
 *         TA_BLUE_STRINGSET_CYRL,
 *         TA_COVERAGE_DEFAULT)
 */

#undef STYLE_LATIN
#define STYLE_LATIN(s, S, f, F, ds, df, C) \
          STYLE(s ## _ ## f, S ## _ ## F, \
                ds " " df " style", \
                TA_WRITING_SYSTEM_LATIN, \
                TA_SCRIPT_ ## S, \
                TA_BLUE_STRINGSET_ ## S, \
                TA_COVERAGE_ ## C)

#undef META_STYLE_LATIN
#define META_STYLE_LATIN(s, S, ds) \
          STYLE_LATIN(s, S, c2cp, C2CP, ds, \
                      "petite capitals from capitals", \
                      PETITE_CAPITALS_FROM_CAPITALS) \
          STYLE_LATIN(s, S, c2sc, C2SC, ds, \
                      "small capitals from capitals", \
                      SMALL_CAPITALS_FROM_CAPITALS) \
          STYLE_LATIN(s, S, ordn, ORDN, ds, \
                      "ordinals", \
                      ORDINALS) \
          STYLE_LATIN(s, S, pcap, PCAP, ds, \
                      "petite capitals", \
                      PETITE_CAPITALS) \
          STYLE_LATIN(s, S, sinf, SINF, ds, \
                      "scientific inferiors", \
                      SCIENTIFIC_INFERIORS) \
          STYLE_LATIN(s, S, smcp, SMCP, ds, \
                      "small capitals", \
                      SMALL_CAPITALS) \
          STYLE_LATIN(s, S, subs, SUBS, ds, \
                      "subscript", \
                      SUBSCRIPT) \
          STYLE_LATIN(s, S, sups, SUPS, ds, \
                      "superscript", \
                      SUPERSCRIPT) \
          STYLE_LATIN(s, S, titl, TITL, ds, \
                      "titling", \
                      TITLING) \
          STYLE_LATIN(s, S, dflt, DFLT, ds, \
                      "default", \
                      DEFAULT)


STYLE(arab_dflt, ARAB_DFLT,
      "Arabic default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_ARAB,
      TA_BLUE_STRINGSET_ARAB,
      TA_COVERAGE_DEFAULT)

STYLE(armn_dflt, ARMN_DFLT,
      "Armenian default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_ARMN,
      TA_BLUE_STRINGSET_ARMN,
      TA_COVERAGE_DEFAULT)

STYLE(beng_dflt, BENG_DFLT,
      "Bengali default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_BENG,
      TA_BLUE_STRINGSET_BENG,
      TA_COVERAGE_DEFAULT)

STYLE(cher_dflt, CHER_DFLT,
      "Cherokee default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_CHER,
      TA_BLUE_STRINGSET_CHER,
      TA_COVERAGE_DEFAULT)

META_STYLE_LATIN(cyrl, CYRL, "Cyrillic")

STYLE(deva_dflt, DEVA_DFLT,
      "Devanagari default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_DEVA,
      TA_BLUE_STRINGSET_DEVA,
      TA_COVERAGE_DEFAULT)

STYLE(ethi_dflt, ETHI_DFLT,
      "Ethiopic default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_ETHI,
      TA_BLUE_STRINGSET_ETHI,
      TA_COVERAGE_DEFAULT)

STYLE(geor_dflt, GEOR_DFLT,
      "Georgian (Mkhedruli) default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_GEOR,
      TA_BLUE_STRINGSET_GEOR,
      TA_COVERAGE_DEFAULT)

STYLE(geok_dflt, GEOK_DFLT,
      "Georgian (Khutsuri) default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_GEOK,
      TA_BLUE_STRINGSET_GEOK,
      TA_COVERAGE_DEFAULT)

META_STYLE_LATIN(grek, GREK, "Greek")

STYLE(gujr_dflt, GUJR_DFLT,
      "Gujarati default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_GUJR,
      TA_BLUE_STRINGSET_GUJR,
      TA_COVERAGE_DEFAULT)

STYLE(guru_dflt, GURU_DFLT,
      "Gurmukhi default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_GURU,
      TA_BLUE_STRINGSET_GURU,
      TA_COVERAGE_DEFAULT)

STYLE(hebr_dflt, HEBR_DFLT,
      "Hebrew default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_HEBR,
      TA_BLUE_STRINGSET_HEBR,
      TA_COVERAGE_DEFAULT)

STYLE(knda_dflt, KNDA_DFLT,
      "Kannada default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_KNDA,
      TA_BLUE_STRINGSET_KNDA,
      TA_COVERAGE_DEFAULT)

STYLE(khmr_dflt, KHMR_DFLT,
      "Khmer default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_KHMR,
      TA_BLUE_STRINGSET_KHMR,
      TA_COVERAGE_DEFAULT)

STYLE(khms_dflt, KHMS_DFLT,
      "Khmer Symbols default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_KHMS,
      TA_BLUE_STRINGSET_KHMS,
      TA_COVERAGE_DEFAULT)

STYLE(lao_dflt, LAO_DFLT,
      "Lao default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_LAO,
      TA_BLUE_STRINGSET_LAO,
      TA_COVERAGE_DEFAULT)

META_STYLE_LATIN(latn, LATN, "Latin")

STYLE(latb_dflt, LATB_DFLT,
      "Latin subscript fallback default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_LATB,
      TA_BLUE_STRINGSET_LATB,
      TA_COVERAGE_DEFAULT)

STYLE(latp_dflt, LATP_DFLT,
      "Latin superscript fallback default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_LATP,
      TA_BLUE_STRINGSET_LATP,
      TA_COVERAGE_DEFAULT)

#ifdef FT_OPTION_AUTOFIT2
STYLE(ltn2_dflt, LTN2_DFLT,
      "Latin 2 default style",
      TA_WRITING_SYSTEM_LATIN2,
      TA_SCRIPT_LATN,
      TA_BLUE_STRINGSET_LATN,
      TA_COVERAGE_DEFAULT)
#endif

STYLE(mlym_dflt, MLYM_DFLT,
      "Malayalam default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_MLYM,
      TA_BLUE_STRINGSET_MLYM,
      TA_COVERAGE_DEFAULT)

STYLE(mymr_dflt, MYMR_DFLT,
      "Myanmar default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_MYMR,
      TA_BLUE_STRINGSET_MYMR,
      TA_COVERAGE_DEFAULT)

STYLE(sinh_dflt, SINH_DFLT,
      "Sinhala default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_SINH,
      TA_BLUE_STRINGSET_SINH,
      TA_COVERAGE_DEFAULT)

STYLE(taml_dflt, TAML_DFLT,
      "Tamil default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_TAML,
      TA_BLUE_STRINGSET_TAML,
      TA_COVERAGE_DEFAULT)

STYLE(telu_dflt, TELU_DFLT,
      "Telugu default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_TELU,
      TA_BLUE_STRINGSET_TELU,
      TA_COVERAGE_DEFAULT)

STYLE(thai_dflt, THAI_DFLT,
      "Thai default style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_THAI,
      TA_BLUE_STRINGSET_THAI,
      TA_COVERAGE_DEFAULT)

STYLE(none_dflt, NONE_DFLT,
      "no style",
      TA_WRITING_SYSTEM_LATIN,
      TA_SCRIPT_NONE,
      TA_BLUE_STRINGSET_NONE,
      TA_COVERAGE_DEFAULT)

/* end of tastyles.h */
