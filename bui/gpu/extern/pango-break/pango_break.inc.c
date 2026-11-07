/* Pango's text segmentation (pango/break.c, break-arabic.c, break-indic.c,
 * break-thai.c, break-latin.c of Pango 1.58): cursor positions (UAX #29
 * graphemes), words, line breaks (UAX #14 with Pango's tailorings) and
 * hyphenation points, the logical attributes PangoLayout breaks lines,
 * moves the cursor and deletes with. Ported to BUI: the text comes as code
 * points (`cp`, `n`), GLib's Unicode data is BUI's (line_break_table.h,
 * break_props.h, HarfBuzz's general categories and scripts), Thai words
 * are found by libthai when it is installed (loaded at run time, as Pango
 * links it), sentences are left out (nothing asks for them). The rules are
 * Pango's, line for line.
 *
 * Included by gpu/intern/gpu_text_unicode.inc.c, which provides
 * text_gc(), text_script_of(), text_is_emoji(), text_is_ext_pict(),
 * TEXT_FALLTHROUGH and BuiLogAttr.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */

#define PARAGRAPH_SEPARATOR 0x2029

/* See http://www.unicode.org/unicode/reports/tr14/ if you hope
 * to understand the line breaking code.
 */

typedef enum
{
  BREAK_ALREADY_HANDLED,   /* didn't use the table */
  BREAK_PROHIBITED, /* no break, even if spaces intervene */
  BREAK_IF_SPACES,  /* "indirect break" (only if there are spaces) */
  BREAK_ALLOWED     /* "direct break" (can always break here) */
  /* TR 14 has two more break-opportunity classes,
   * "indirect break opportunity for combining marks following a space"
   * and "prohibited break for combining marks"
   * but we handle that inline in the code.
   */
} BreakOpportunity;


/* need to sync the break range to glib/gunicode.h . */
#define LAST_BREAK_TYPE BUI_LB_UNAMBIGUOUS_HYPHEN
#define BREAK_TYPE_SAFE(btype)            \
	 ((btype) <= LAST_BREAK_TYPE ? (btype) : BUI_LB_UNKNOWN)


/*
 * Hangul Conjoining Jamo handling.
 *
 * The way we implement it is just a bit different from TR14,
 * but produces the same results.
 * The same algorithm is also used in TR29 for cluster boundaries.
 *
 */


/* An enum that works as the states of the Hangul syllables system.
 **/
typedef enum
{
  JAMO_L,	/* BUI_LB_HANGUL_L_JAMO */
  JAMO_V,	/* BUI_LB_HANGUL_V_JAMO */
  JAMO_T,	/* BUI_LB_HANGUL_T_JAMO */
  JAMO_LV,	/* BUI_LB_HANGUL_LV_SYLLABLE */
  JAMO_LVT,	/* BUI_LB_HANGUL_LVT_SYLLABLE */
  NO_JAMO	/* Other */
} JamoType;

/* There are Hangul syllables encoded as characters, that act like a
 * sequence of Jamos. For each character we define a JamoType
 * that the character starts with, and one that it ends with.  This
 * decomposes JAMO_LV and JAMO_LVT to simple other JAMOs.  So for
 * example, a character with LineBreak type
 * BUI_LB_HANGUL_LV_SYLLABLE has start=JAMO_L and end=JAMO_V.
 */
typedef struct _CharJamoProps
{
  JamoType start, end;
} CharJamoProps;

/* Map from JamoType to CharJamoProps that hold only simple
 * JamoTypes (no LV or LVT) or none.
 */
static const CharJamoProps HangulJamoProps[] = {
  {JAMO_L, JAMO_L},	/* JAMO_L */
  {JAMO_V, JAMO_V},	/* JAMO_V */
  {JAMO_T, JAMO_T},	/* JAMO_T */
  {JAMO_L, JAMO_V},	/* JAMO_LV */
  {JAMO_L, JAMO_T},	/* JAMO_LVT */
  {NO_JAMO, NO_JAMO}	/* NO_JAMO */
};

/* A character forms a syllable with the previous character if and only if:
 * JamoType(this) is not NO_JAMO and:
 *
 * HangulJamoProps[JamoType(prev)].end and
 * HangulJamoProps[JamoType(this)].start are equal,
 * or the former is one less than the latter.
 */

#define IS_JAMO(btype)              \
	((btype >= BUI_LB_HANGUL_L_JAMO) && \
	 (btype <= BUI_LB_HANGUL_LVT_SYLLABLE))
#define JAMO_TYPE(btype)      \
	(IS_JAMO(btype) ? (btype - BUI_LB_HANGUL_L_JAMO) : NO_JAMO)

/* Types of Japanese characters */
#define JAPANESE(wc) ((wc) >= 0x2F00 && (wc) <= 0x30FF)
#define KANJI(wc)    ((wc) >= 0x2F00 && (wc) <= 0x2FDF)
#define HIRAGANA(wc) ((wc) >= 0x3040 && (wc) <= 0x309F)
#define KATAKANA(wc) ((wc) >= 0x30A0 && (wc) <= 0x30FF)

#define LATIN(wc) (((wc) >= 0x0020 && (wc) <= 0x02AF) || ((wc) >= 0x1E00 && (wc) <= 0x1EFF))
#define CYRILLIC(wc) (((wc) >= 0x0400 && (wc) <= 0x052F))
#define GREEK(wc) (((wc) >= 0x0370 && (wc) <= 0x3FF) || ((wc) >= 0x1F00 && (wc) <= 0x1FFF))
#define KANA(wc) ((wc) >= 0x3040 && (wc) <= 0x30FF)
#define HANGUL(wc) ((wc) >= 0xAC00 && (wc) <= 0xD7A3)
#define EMOJI(wc) (text_is_emoji (wc))
#define MATH(wc) ((wc) >= 0x2200 && (wc) <= 0x22FF)
#define BACKSPACE_DELETES_CHARACTER(wc) (!LATIN (wc) && !CYRILLIC (wc) && !GREEK (wc) && !KANA (wc) && !HANGUL (wc) && !EMOJI (wc) && !MATH (wc))

/* Previously "123foo" was two words. But in UAX 29 of Unicode, 
 * we know don't break words between consecutive letters and numbers
 */
typedef enum
{
  WordNone,
  WordLetters,
  WordNumbers
} WordType;


/* attrs[0..n]: the boundary before each character, the last after the
 * text (a paragraph separator is assumed after it). */
static void
default_break (const uint32_t *cp,
               int             n,
               BuiLogAttr     *attrs)
{
  /* The rationale for all this is in section 5.15 of the Unicode 3.0 book,
   * the line breaking stuff is also in TR14 on unicode.org
   */

  /* This is a default break implementation that should work for nearly all
   * languages. Language engines can override it optionally.
   */

  /* FIXME one cheesy optimization here would be to memset attrs to 0
   * before we start, and then never assign %0 to anything
   */

  int i;

  uint32_t prev_wc;
  uint32_t prev_prev_wc;
  uint32_t next_wc;
  uint32_t next_next_wc;

  JamoType prev_jamo;

  int next_break_type;
  int next_next_break_type;
  int prev_break_type;
  int prev_prev_break_type;

  hb_script_t prev_script;

  /* See Grapheme_Cluster_Break Property Values table of UAX#29 */
  typedef enum
  {
    GB_Other,
    GB_ControlCRLF,
    GB_Extend,
    GB_ZWJ,
    GB_Prepend,
    GB_SpacingMark,
    GB_InHangulSyllable, /* Handles all of L, V, T, LV, LVT rules */
    /* Use state machine to handle emoji sequence */
    /* Rule GB12 and GB13 */
    GB_RI_Odd, /* Meets odd number of RI */
    GB_RI_Even, /* Meets even number of RI */
  } GraphemeBreakType;
  GraphemeBreakType prev_GB_type = GB_Other;
  int met_Extended_Pictographic = 0;

  /* Rule GB9c */
  int met_InCB_Consonant = 0;
  int met_InCB_Consonant_Linker = 0;

  /* See Word_Break Property Values table of UAX#29 */
  typedef enum
  {
    WB_Other,
    WB_NewlineCRLF,
    WB_ExtendFormat,
    WB_Katakana,
    WB_Hebrew_Letter,
    WB_ALetter,
    WB_MidNumLet,
    WB_MidLetter,
    WB_MidNum,
    WB_Numeric,
    WB_ExtendNumLet,
    WB_RI_Odd,
    WB_RI_Even,
    WB_WSegSpace,
  } WordBreakType;
  WordBreakType prev_prev_WB_type = WB_Other, prev_WB_type = WB_Other;
  int prev_WB_i = -1;

  /* Rule LB25 with Example 7 of Customization */
  typedef enum
  {
    LB_Other,
    LB_Numeric,
    LB_Numeric_Close,
    LB_RI_Odd,
    LB_RI_Even,
    LB_Dotted_Circle,
    LB_Hyphen,
  } LineBreakType;
  LineBreakType prev_prev_LB_type = LB_Other;
  LineBreakType prev_LB_type = LB_Other;
  int met_LB15a = 0;
  int prev_LB_i = -1;
  int met_Ext_Pict_Unassigned = 0;

  WordType current_word_type = WordNone;
  uint32_t last_word_letter = 0;
  uint32_t base_character = 0;

  int prev_space_or_hyphen;

  int almost_done = 0;
  int done = 0;

  prev_break_type = BUI_LB_UNKNOWN;
  prev_prev_break_type = BUI_LB_UNKNOWN;
  prev_wc = 0;
  prev_prev_wc = 0;
  prev_script = HB_SCRIPT_COMMON;
  prev_jamo = NO_JAMO;
  prev_space_or_hyphen = 0;
  next_next_wc = 0;
  next_next_break_type = BUI_LB_UNKNOWN;

  if (n == 0)
    {
      next_wc = PARAGRAPH_SEPARATOR;
      almost_done = 1;
    }
  else
    next_wc = cp[0];

  next_break_type = bui_lb_type (next_wc);
  next_break_type = BREAK_TYPE_SAFE (next_break_type);

  for (i = 0; !done ; i++)
    {
      hb_unicode_general_category_t type;
      uint32_t wc;
      int break_type;
      int row_break_type;
      BreakOpportunity break_op;
      JamoType jamo;
      int makes_hangul_syllable;

      /* UAX#29 boundaries */
      int is_grapheme_boundary;
      int is_word_boundary;

      /* Emoji extended pictographics */
      int is_Extended_Pictographic;

      hb_script_t script;

      wc = next_wc;
      break_type = next_break_type;

      if (almost_done)
	{
	  next_wc = 0;
	  next_break_type = BUI_LB_UNKNOWN;
	  done = 1;
	}
      else
	{
	  if (i + 1 >= n)
	    {
	      /* This is how we fill in the last element (end position) of the
	       * attr array - assume there's a paragraph separators off the end
	       * of the text.
	       */
	      next_wc = PARAGRAPH_SEPARATOR;
	      next_next_wc = PARAGRAPH_SEPARATOR;
	      almost_done = 1;
	    }
	  else
	    {
	      next_wc = cp[i + 1];
	      next_next_wc = i + 2 >= n ? PARAGRAPH_SEPARATOR : cp[i + 2];
	    }

	  next_break_type = bui_lb_type (next_wc);
	  next_break_type = BREAK_TYPE_SAFE (next_break_type);

	  next_next_break_type = bui_lb_type (next_next_wc);
	  next_next_break_type = BREAK_TYPE_SAFE (next_next_break_type);
	}

      type = text_gc (wc);
      jamo = JAMO_TYPE (break_type);

      /* Determine wheter this forms a Hangul syllable with prev. */
      if (jamo == NO_JAMO)
	makes_hangul_syllable = 0;
      else
	{
	  JamoType prev_end   = HangulJamoProps[prev_jamo].end  ;
	  JamoType this_start = HangulJamoProps[     jamo].start;

	  /* See comments before IS_JAMO */
	  makes_hangul_syllable = (prev_end == this_start) || (prev_end + 1 == this_start);
	}

      switch ((int)type)
        {
        case HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR:
        case HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR:
        case HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR:
          attrs[i].is_white = 1;
          break;
        case HB_UNICODE_GENERAL_CATEGORY_CONTROL:
          if (wc == '\t' || wc == '\n' || wc == '\r' || wc == '\f')
            attrs[i].is_white = 1;
          else
            attrs[i].is_white = 0;
          break;
        default:
          attrs[i].is_white = 0;
          break;
        }

      /* Just few spaces have variable width. So explicitly mark them.
       */
      attrs[i].is_expandable_space = (0x0020 == wc || 0x00A0 == wc);
      is_Extended_Pictographic =
	text_is_ext_pict (wc);


      /* ---- UAX#29 Grapheme Boundaries ---- */
      {
	GraphemeBreakType GB_type;

        /* Find the GraphemeBreakType of wc */
	GB_type = GB_Other;
	switch ((int)type)
	  {
	  case HB_UNICODE_GENERAL_CATEGORY_FORMAT:
	    if ((wc == 0x200C))
	      {
		GB_type = GB_Extend;
		break;
	      }
	    if ((wc == 0x200D))
	      {
		GB_type = GB_ZWJ;
		break;
	      }
            if (((wc >= 0x600 && wc <= 0x605) ||
                            wc == 0x6DD ||
                            wc == 0x70F ||
                            wc == 0x8E2 ||
                            wc == 0x110BD ||
                            wc == 0x110CD))
              {
                GB_type = GB_Prepend;
                break;
              }
            /* Tag chars */
            if (wc >= 0xE0020 && wc <= 0xE00FF)
              {
                GB_type = GB_Extend;
                break;
              }
            TEXT_FALLTHROUGH;
	  case HB_UNICODE_GENERAL_CATEGORY_CONTROL:
	  case HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR:
	  case HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR:
	  case HB_UNICODE_GENERAL_CATEGORY_SURROGATE:
	    GB_type = GB_ControlCRLF;
	    break;

	  case HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED:
	    /* Unassigned default ignorables */
	    if ((wc >= 0xFFF0 && wc <= 0xFFF8) ||
		(wc >= 0xE0000 && wc <= 0xE0FFF))
	      {
		GB_type = GB_ControlCRLF;
		break;
	      }
            TEXT_FALLTHROUGH;

	  case HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER:
	    if (makes_hangul_syllable)
	      GB_type = GB_InHangulSyllable;

	    if (bui_is_Consonant_Preceding_Repha (wc) ||
		bui_is_Consonant_Prefixed (wc))
	      GB_type = GB_Prepend;
	    break;

	  case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER:
	    if (wc >= 0xFF9E && wc <= 0xFF9F)
	      GB_type = GB_Extend; /* Other_Grapheme_Extend */
	    break;

	  case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
	    if (!(wc == 0x102B || wc == 0x102C || wc == 0x1038||
		  (wc >= 0x1062 && wc <= 0x1064) ||
		  (wc >= 0x1067 && wc <= 0x106D) ||
		  wc == 0x1083 || (wc >= 0x1087 && wc <= 0x108C) ||
		  wc == 0x108F || (wc >= 0x109A && wc <= 0x109C) ||
		  wc == 0x1A61 || wc == 0x1A63 || wc == 0x1A64 ||
		  wc == 0xAA7B || wc == 0xAA7D ||
		  wc == 0x11720 || wc == 0x11721))
	      GB_type = GB_SpacingMark; /* SpacingMark */
	    if (wc >= 0x0900)
	      {
		if (wc == 0x09BE || wc == 0x09D7 ||
		    wc == 0x0B3E || wc == 0x0B57 || wc == 0x0BBE || wc == 0x0BD7 ||
		    wc == 0x0CC2 || wc == 0x0CD5 || wc == 0x0CD6 ||
		    wc == 0x0D3E || wc == 0x0D57 || wc == 0x0DCF || wc == 0x0DDF ||
		    wc == 0x1D165 || (wc >= 0x1D16E && wc <= 0x1D172))
		  GB_type = GB_Extend; /* Other_Grapheme_Extend */
	      }
	    break;

	  case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
	  case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
	    GB_type = GB_Extend; /* Grapheme_Extend */
	    break;

          case HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL:
            if ((wc >=0x1F1E6 && wc <=0x1F1FF))
              {
                if (prev_GB_type == GB_RI_Odd)
                  GB_type = GB_RI_Even;
                else
                  GB_type = GB_RI_Odd;
                break;
              }
            break;

          case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL:
            /* Fitzpatrick modifiers */
            if (wc >= 0x1F3FB && wc <= 0x1F3FF)
              GB_type = GB_Extend;
            break;

          default:
            break;
	  }

	/* Rule GB11 */
	if (met_Extended_Pictographic)
	  {
	    if (GB_type == GB_Extend)
	      met_Extended_Pictographic = 1;
	    else if (text_is_ext_pict (prev_wc) &&
		     GB_type == GB_ZWJ)
	      met_Extended_Pictographic = 1;
	    else if (prev_GB_type == GB_Extend && GB_type == GB_ZWJ)
	      met_Extended_Pictographic = 1;
	    else if (prev_GB_type == GB_ZWJ && is_Extended_Pictographic)
	      met_Extended_Pictographic = 1;
	    else
	      met_Extended_Pictographic = 0;
	  }

	/* Grapheme Cluster Boundary Rules */
	is_grapheme_boundary = 1; /* Rule GB999 */

	if (prev_GB_type == GB_RI_Odd && GB_type == GB_RI_Even)
	  is_grapheme_boundary = 0; /* Rule GB12 and GB13 */

	if (is_Extended_Pictographic)
	  { /* Rule GB11 */
	    if (prev_GB_type == GB_ZWJ && met_Extended_Pictographic)
	      is_grapheme_boundary = 0;
	  }

	if (met_InCB_Consonant || met_InCB_Consonant_Linker)
	  { /* Rule GB9c */
	    if (met_InCB_Consonant_Linker &&
		bui_is_Indic_Conjunct_Break_Consonant(wc))
	      {
	        met_InCB_Consonant = 1;
		met_InCB_Consonant_Linker = 0;
		is_grapheme_boundary = 0;
	      }

	    if (!(bui_is_Indic_Conjunct_Break_Extend(wc) ||
		  bui_is_Indic_Conjunct_Break_Linker(wc)))
	      {
	        met_InCB_Consonant = 0;
		met_InCB_Consonant_Linker = 0;
	      }
	  }

	if (prev_GB_type == GB_Prepend)
	  is_grapheme_boundary = 0; /* Rule GB9b */

	if (GB_type == GB_SpacingMark)
	  is_grapheme_boundary = 0; /* Rule GB9a */

	if (GB_type == GB_Extend || GB_type == GB_ZWJ)
	  is_grapheme_boundary = 0; /* Rule GB9 */

	if (GB_type == GB_InHangulSyllable)
	  is_grapheme_boundary = 0; /* Rules GB6, GB7, GB8 */

	if (prev_GB_type == GB_ControlCRLF || GB_type == GB_ControlCRLF)
	  is_grapheme_boundary = 1; /* Rules GB4 and GB5 */

	if (wc == '\n' && prev_wc == '\r')
          is_grapheme_boundary = 0; /* Rule GB3 */

	/* We apply Rules GB1 and GB2 at the end of the function */

	if (is_Extended_Pictographic)
	  met_Extended_Pictographic = 1;

	if (met_InCB_Consonant && !met_InCB_Consonant_Linker &&
	    bui_is_Indic_Conjunct_Break_Linker(wc))
	      met_InCB_Consonant_Linker = 1; /* Rule GB9c */

	if (!met_InCB_Consonant && !met_InCB_Consonant_Linker &&
	    bui_is_Indic_Conjunct_Break_Consonant(wc))
	      met_InCB_Consonant = 1; /* Rule GB9c */

	attrs[i].is_cursor_position = is_grapheme_boundary;
	/* If this is a grapheme boundary, we have to decide if backspace
	 * deletes a character or the whole grapheme cluster */
	if (is_grapheme_boundary)
          {
	    attrs[i].backspace_deletes_character = BACKSPACE_DELETES_CHARACTER (base_character);

	    /* Dependent Vowels for Indic language */
	    if (bui_is_Virama (prev_wc) ||
		bui_is_Vowel_Dependent (prev_wc))
	      attrs[i].backspace_deletes_character = 1;
          }
	else
	  attrs[i].backspace_deletes_character = 0;

	prev_GB_type = GB_type;
      }

      script = (hb_script_t)text_script_of (wc);
      /* ---- UAX#29 Word Boundaries ---- */
      {
	is_word_boundary = 0;
	if (is_grapheme_boundary ||
	    (wc >=0x1F1E6 && wc <=0x1F1FF)) /* Rules WB3 and WB4 */
	  {
	    WordBreakType WB_type;

	    /* Find the WordBreakType of wc */
	    WB_type = WB_Other;

	    if (script == HB_SCRIPT_KATAKANA)
	      WB_type = WB_Katakana;

	    if (script == HB_SCRIPT_HEBREW && type == HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER)
	      WB_type = WB_Hebrew_Letter;

	    if (WB_type == WB_Other)
	      switch (wc >> 8)
	        {
		case 0x00:
		  if (wc == 0x00b8)
		    WB_type = WB_ALetter; /* For Unicode 17.0.0 */
		  break;
		case 0x30:
		  if (wc == 0x3031 || wc == 0x3032 || wc == 0x3033 || wc == 0x3034 || wc == 0x3035 ||
		      wc == 0x309b || wc == 0x309c || wc == 0x30a0 || wc == 0x30fc)
		    WB_type = WB_Katakana; /* Katakana exceptions */
		  break;
		case 0xFF:
		  if (wc == 0xFF70)
		    WB_type = WB_Katakana; /* Katakana exceptions */
		  else if (wc >= 0xFF9E && wc <= 0xFF9F)
		    WB_type = WB_ExtendFormat; /* Other_Grapheme_Extend */
		  break;
		case 0x05:
		  if (wc == 0x058A)
		    WB_type = WB_ALetter; /* ALetter exceptions */
		  break;
		case 0x07:
		  if (wc == 0x070F)
		    WB_type = WB_ALetter; /* ALetter exceptions */
		  break;
                default:
                  break;
		}

	    if (WB_type == WB_Other)
	      switch ((int) break_type)
	        {
		case BUI_LB_NUMERIC:
		  if (wc != 0x066C)
		    WB_type = WB_Numeric; /* Numeric */
		  break;
		case BUI_LB_INFIX_SEPARATOR:
		  if (wc != 0x003A && wc != 0xFE13 && wc != 0x002E)
		    WB_type = WB_MidNum; /* MidNum */
		  break;
                default:
                  break;
		}

	    if (WB_type == WB_Other)
	      switch ((int) type)
		{
		case HB_UNICODE_GENERAL_CATEGORY_CONTROL:
		  if (wc != 0x000D && wc != 0x000A && wc != 0x000B && wc != 0x000C && wc != 0x0085)
		    break;
                  TEXT_FALLTHROUGH;
		case HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR:
		case HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR:
		  WB_type = WB_NewlineCRLF; /* CR, LF, Newline */
		  break;

		case HB_UNICODE_GENERAL_CATEGORY_FORMAT:
		case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
		case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
		case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
		  WB_type = WB_ExtendFormat; /* Extend, Format */
		  break;

		case HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION:
		  WB_type = WB_ExtendNumLet; /* ExtendNumLet */
		  break;

		case HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION:
		case HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION:
		  if (wc == 0x2018 || wc == 0x2019)
		    WB_type = WB_MidNumLet; /* MidNumLet */
		  break;
		case HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION:
		  if ((wc >= 0x055a && wc <= 0x055c) ||
		      wc == 0x055e || wc == 0x05f3)
		    WB_type = WB_ALetter; /* ALetter */
		  else if (wc == 0x0027 || wc == 0x002e || wc == 0x2024 ||
		      wc == 0xfe52 || wc == 0xff07 || wc == 0xff0e)
		    WB_type = WB_MidNumLet; /* MidNumLet */
		  else if (wc == 0x00b7 || wc == 0x05f4 || wc == 0x2027 ||
			   wc == 0x003a || wc == 0x0387 || wc == 0x055f ||
			   wc == 0xfe13 || wc == 0xfe55 || wc == 0xff1a)
		    WB_type = WB_MidLetter; /* MidLetter */
		  else if (wc == 0x066c ||
			   wc == 0xfe50 || wc == 0xfe54 || wc == 0xff0c || wc == 0xff1b)
		    WB_type = WB_MidNum; /* MidNum */
		  break;

		case HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL:
		  if (wc >= 0x24B6 && wc <= 0x24E9) /* Other_Alphabetic */
		    goto Alphabetic;

		  if ((wc >= 0x1F1E6 && wc <= 0x1F1FF))
		    {
                      if (prev_WB_type == WB_RI_Odd)
                        WB_type = WB_RI_Even;
                      else
                        WB_type = WB_RI_Odd;
		    }

		  break;

		case HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER:
		case HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER:
		  if (wc == 0x3006 || wc == 0x3007 ||
		      (wc >= 0x3021 && wc <= 0x3029) ||
		      (wc >= 0x3038 && wc <= 0x303A) ||
		      (wc >= 0x3400 && wc <= 0x4DB5) ||
		      (wc >= 0x4E00 && wc <= 0x9FC3) ||
		      (wc >= 0xF900 && wc <= 0xFA2D) ||
		      (wc >= 0xFA30 && wc <= 0xFA6A) ||
		      (wc >= 0xFA70 && wc <= 0xFAD9) ||
		      (wc >= 0x20000 && wc <= 0x2A6D6) ||
		      (wc >= 0x2F800 && wc <= 0x2FA1D))
		    break; /* ALetter exceptions: Ideographic */
		  goto Alphabetic;

		case HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER:
		case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER:
		case HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER:
		case HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER:
		Alphabetic:
		  if (break_type != BUI_LB_COMPLEX_CONTEXT && script != HB_SCRIPT_HIRAGANA)
		    WB_type = WB_ALetter; /* ALetter */
		  break;
                default:
                  break;
		}

	    if (WB_type == WB_Other)
	      {
		if (type == HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR &&
		    break_type != BUI_LB_NON_BREAKING_GLUE)
		  WB_type = WB_WSegSpace;
	      }

	    /* Word Cluster Boundary Rules */

	    /* We apply Rules WB1 and WB2 at the end of the function */

	    if (prev_WB_type == WB_NewlineCRLF && prev_WB_i + 1 == i)
	      {
	        /* The extra check for prev_WB_i is to correctly handle sequences like
		 * Newline ÷ Extend × Extend
		 * since we have not skipped ExtendFormat yet.
		 */
	        is_word_boundary = 1; /* Rule WB3a */
	      }
	    else if (WB_type == WB_NewlineCRLF)
	      is_word_boundary = 1; /* Rule WB3b */
	    else if (prev_wc == 0x200D && is_Extended_Pictographic)
	      is_word_boundary = 0; /* Rule WB3c */
	    else if (prev_WB_type == WB_WSegSpace &&
		     WB_type == WB_WSegSpace && prev_WB_i + 1 == i)
	      is_word_boundary = 0; /* Rule WB3d */
	    else if (WB_type == WB_ExtendFormat)
	      is_word_boundary = 0; /* Rules WB4? */
	    else if ((prev_WB_type == WB_ALetter  ||
                  prev_WB_type == WB_Hebrew_Letter ||
                  prev_WB_type == WB_Numeric) &&
                 (WB_type == WB_ALetter  ||
                  WB_type == WB_Hebrew_Letter ||
                  WB_type == WB_Numeric))
	      is_word_boundary = 0; /* Rules WB5, WB8, WB9, WB10 */
	    else if (prev_WB_type == WB_Katakana && WB_type == WB_Katakana)
	      is_word_boundary = 0; /* Rule WB13 */
	    else if ((prev_WB_type == WB_ALetter ||
                  prev_WB_type == WB_Hebrew_Letter ||
                  prev_WB_type == WB_Numeric ||
                  prev_WB_type == WB_Katakana ||
                  prev_WB_type == WB_ExtendNumLet) &&
                 WB_type == WB_ExtendNumLet)
	      is_word_boundary = 0; /* Rule WB13a */
	    else if (prev_WB_type == WB_ExtendNumLet &&
                 (WB_type == WB_ALetter ||
                  WB_type == WB_Hebrew_Letter ||
                  WB_type == WB_Numeric ||
                  WB_type == WB_Katakana))
	      is_word_boundary = 0; /* Rule WB13b */
	    else if (((prev_prev_WB_type == WB_ALetter ||
                   prev_prev_WB_type == WB_Hebrew_Letter) &&
                  (WB_type == WB_ALetter ||
                   WB_type == WB_Hebrew_Letter)) &&
		     (prev_WB_type == WB_MidLetter ||
              prev_WB_type == WB_MidNumLet ||
              prev_wc == 0x0027))
	      {
		attrs[prev_WB_i].is_word_boundary = 0; /* Rule WB6 */
		is_word_boundary = 0; /* Rule WB7 */
	      }
	    else if (prev_WB_type == WB_Hebrew_Letter && wc == 0x0027)
          is_word_boundary = 0; /* Rule WB7a */
	    else if (prev_prev_WB_type == WB_Hebrew_Letter && prev_wc == 0x0022 &&
                 WB_type == WB_Hebrew_Letter) {
          attrs[prev_WB_i].is_word_boundary = 0; /* Rule WB7b */
          is_word_boundary = 0; /* Rule WB7c */
        }
	    else if ((prev_prev_WB_type == WB_Numeric && WB_type == WB_Numeric) &&
                 (prev_WB_type == WB_MidNum || prev_WB_type == WB_MidNumLet ||
                  prev_wc == 0x0027))
	      {
		is_word_boundary = 0; /* Rule WB11 */
		attrs[prev_WB_i].is_word_boundary = 0; /* Rule WB12 */
	      }
	    else if (prev_WB_type == WB_RI_Odd && WB_type == WB_RI_Even)
	      is_word_boundary = 0; /* Rule WB15 and WB16 */
	    else
	      is_word_boundary = 1; /* Rule WB999 */

	    if (WB_type != WB_ExtendFormat)
	      {
		prev_prev_WB_type = prev_WB_type;
		prev_WB_type = WB_type;
		prev_WB_i = i;
	      }
	  }

	attrs[i].is_word_boundary = is_word_boundary;
      }


      /* ---- Line breaking ---- */

      break_op = BREAK_ALREADY_HANDLED;

      row_break_type = prev_break_type == BUI_LB_SPACE ?
	prev_prev_break_type : prev_break_type;

      attrs[i].is_char_break = 0;
      attrs[i].is_line_break = 0;
      attrs[i].is_mandatory_break = 0;

      /* Rule LB1:
	 assign a line breaking class to each code point of the input. */
      switch ((int)break_type)
	{
	case BUI_LB_AMBIGUOUS:
	case BUI_LB_SURROGATE:
	case BUI_LB_UNKNOWN:
	  break_type = BUI_LB_ALPHABETIC;
	  break;

	case BUI_LB_COMPLEX_CONTEXT:
	  if (type == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK ||
	      type == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK)
	    break_type = BUI_LB_COMBINING_MARK;
	  else
	    break_type = BUI_LB_ALPHABETIC;
	  break;

	case BUI_LB_CONDITIONAL_JAPANESE_STARTER:
	  break_type = BUI_LB_NON_STARTER;
	  break;

	default:
          break;
	}

      /* If it's not a grapheme boundary, it's not a line break either */
      if (attrs[i].is_cursor_position ||
	  break_type == BUI_LB_COMBINING_MARK ||
	  break_type == BUI_LB_ZERO_WIDTH_JOINER ||
	  break_type == BUI_LB_NON_BREAKING_GLUE ||
	  break_type == BUI_LB_HANGUL_L_JAMO ||
	  break_type == BUI_LB_HANGUL_V_JAMO ||
	  break_type == BUI_LB_HANGUL_T_JAMO ||
	  break_type == BUI_LB_HANGUL_LV_SYLLABLE ||
	  break_type == BUI_LB_HANGUL_LVT_SYLLABLE ||
	  break_type == BUI_LB_EMOJI_MODIFIER ||
	  break_type == BUI_LB_REGIONAL_INDICATOR ||
	  break_type == BUI_LB_VIRAMA ||
	  break_type == BUI_LB_VIRAMA_FINAL ||
	  0)
	{
	  LineBreakType LB_type;

	  /* Find the LineBreakType of wc */
	  LB_type = LB_Other;

	  /* Rule LB20a */
	  if (wc == 0x2010)
	    LB_type = LB_Hyphen;

	  /* Rule LB28a */
	  if (wc == 0x25CC)
	    LB_type = LB_Dotted_Circle;

	  if (break_type == BUI_LB_NUMERIC)
	    LB_type = LB_Numeric;

	  if (break_type == BUI_LB_SYMBOL ||
	      break_type == BUI_LB_INFIX_SEPARATOR)
	    {
	      if (!(prev_LB_type == LB_Numeric))
		LB_type = LB_Other;
	    }

	  if (break_type == BUI_LB_CLOSE_PUNCTUATION ||
	      break_type == BUI_LB_CLOSE_PARENTHESIS)
	    {
	      if (prev_LB_type == LB_Numeric)
		LB_type = LB_Numeric_Close;
	      else
		LB_type = LB_Other;
	    }

	  if (break_type == BUI_LB_REGIONAL_INDICATOR)
	    {
	      if (prev_LB_type == LB_RI_Odd)
		LB_type = LB_RI_Even;
	      else
		LB_type = LB_RI_Odd;
	    }

	  attrs[i].is_line_break = 1; /* Rule LB31 */
	  /* Unicode doesn't specify char wrap;
	     we wrap around all chars currently. */
	  if (attrs[i].is_cursor_position)
	    attrs[i].is_char_break = 1;

	  /* Make any necessary replacements first */
	  if (row_break_type == BUI_LB_UNKNOWN)
	    row_break_type = BUI_LB_ALPHABETIC;

	  /* add the line break rules in reverse order to override
	     the lower priority rules. */

	  /* Rule LB30b */
	  if (prev_break_type == BUI_LB_EMOJI_BASE &&
	      break_type == BUI_LB_EMOJI_MODIFIER)
	    break_op = BREAK_PROHIBITED;

	  if (met_Ext_Pict_Unassigned &&
	      break_type == BUI_LB_EMOJI_MODIFIER)
	    {
	      met_Ext_Pict_Unassigned = 0;
	      break_op = BREAK_PROHIBITED;
	    }

	  /* Rule LB30a */
	  if (prev_LB_type == LB_RI_Odd && LB_type == LB_RI_Even)
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB30 */
	  if ((prev_break_type == BUI_LB_ALPHABETIC ||
	       prev_break_type == BUI_LB_HEBREW_LETTER ||
	       prev_break_type == BUI_LB_NUMERIC) &&
	      break_type == BUI_LB_OPEN_PUNCTUATION &&
	      !bui_is_EastAsianWide (wc))
	    break_op = BREAK_PROHIBITED;

	  if (prev_break_type == BUI_LB_CLOSE_PARENTHESIS &&
	      !bui_is_EastAsianWide (prev_wc)&&
	      (break_type == BUI_LB_ALPHABETIC ||
	       break_type == BUI_LB_HEBREW_LETTER ||
	       break_type == BUI_LB_NUMERIC))
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB29 */
	  if (prev_break_type == BUI_LB_INFIX_SEPARATOR &&
	      (break_type == BUI_LB_ALPHABETIC ||
	       break_type == BUI_LB_HEBREW_LETTER))
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB28a */
	  if (prev_break_type == BUI_LB_AKSARA_PRE_BASE &&
	      (break_type == BUI_LB_AKSARA ||
	       LB_type == LB_Dotted_Circle ||
	       break_type == BUI_LB_AKSARA_START))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_break_type == BUI_LB_AKSARA ||
	       prev_LB_type == LB_Dotted_Circle ||
	       prev_break_type == BUI_LB_AKSARA_START) &&
	      (break_type == BUI_LB_VIRAMA_FINAL ||
	       break_type == BUI_LB_VIRAMA))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_prev_break_type == BUI_LB_AKSARA ||
	       prev_prev_LB_type == LB_Dotted_Circle ||
	       prev_prev_break_type == BUI_LB_AKSARA_START) &&
	      prev_break_type == BUI_LB_VIRAMA &&
	      (break_type == BUI_LB_AKSARA ||
	       LB_type == LB_Dotted_Circle))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_break_type == BUI_LB_AKSARA ||
	       prev_LB_type == LB_Dotted_Circle ||
	       prev_break_type == BUI_LB_AKSARA_START) &&
	      (break_type == BUI_LB_AKSARA ||
	       LB_type == LB_Dotted_Circle ||
	       break_type == BUI_LB_AKSARA_START) &&
	      next_break_type == BUI_LB_VIRAMA_FINAL)
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB28 */
	  if ((prev_break_type == BUI_LB_ALPHABETIC ||
	       prev_break_type == BUI_LB_HEBREW_LETTER) &&
	      (break_type == BUI_LB_ALPHABETIC ||
	       break_type == BUI_LB_HEBREW_LETTER))
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB27 */
	  if ((prev_break_type == BUI_LB_HANGUL_L_JAMO ||
	       prev_break_type == BUI_LB_HANGUL_V_JAMO ||
	       prev_break_type == BUI_LB_HANGUL_T_JAMO ||
	       prev_break_type == BUI_LB_HANGUL_LV_SYLLABLE ||
	       prev_break_type == BUI_LB_HANGUL_LVT_SYLLABLE) &&
	      break_type == BUI_LB_POSTFIX)
	    break_op = BREAK_PROHIBITED;

	  if (prev_break_type == BUI_LB_PREFIX &&
	      (break_type == BUI_LB_HANGUL_L_JAMO ||
	       break_type == BUI_LB_HANGUL_V_JAMO ||
	       break_type == BUI_LB_HANGUL_T_JAMO ||
	       break_type == BUI_LB_HANGUL_LV_SYLLABLE ||
	       break_type == BUI_LB_HANGUL_LVT_SYLLABLE))
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB26 */
	  if (prev_break_type == BUI_LB_HANGUL_L_JAMO &&
	      (break_type == BUI_LB_HANGUL_L_JAMO ||
	       break_type == BUI_LB_HANGUL_V_JAMO ||
	       break_type == BUI_LB_HANGUL_LV_SYLLABLE ||
	       break_type == BUI_LB_HANGUL_LVT_SYLLABLE))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_break_type == BUI_LB_HANGUL_V_JAMO ||
	       prev_break_type == BUI_LB_HANGUL_LV_SYLLABLE) &&
	      (break_type == BUI_LB_HANGUL_V_JAMO ||
	       break_type == BUI_LB_HANGUL_T_JAMO))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_break_type == BUI_LB_HANGUL_T_JAMO ||
	       prev_break_type == BUI_LB_HANGUL_LVT_SYLLABLE) &&
	      break_type == BUI_LB_HANGUL_T_JAMO)
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB25 */
	  if (prev_prev_LB_type == LB_Numeric &&
	      prev_break_type == BUI_LB_CLOSE_PUNCTUATION &&
	      break_type == BUI_LB_POSTFIX)
	    break_op = BREAK_PROHIBITED; /* NU ( SY | IS )* CL × PO */

	  if (prev_prev_LB_type == LB_Numeric &&
	      prev_break_type == BUI_LB_CLOSE_PARENTHESIS &&
	      break_type == BUI_LB_POSTFIX)
	    break_op = BREAK_PROHIBITED; /* NU ( SY | IS )* CP × PO */

	  if (prev_prev_LB_type == LB_Numeric &&
	      prev_break_type == BUI_LB_CLOSE_PUNCTUATION &&
	      break_type == BUI_LB_PREFIX)
	    break_op = BREAK_PROHIBITED; /* NU ( SY | IS )* CL × PR */

	  if (prev_prev_LB_type == LB_Numeric &&
	      prev_break_type == BUI_LB_CLOSE_PARENTHESIS &&
	      break_type == BUI_LB_PREFIX)
	    break_op = BREAK_PROHIBITED; /* NU ( SY | IS )* CP × PR */

	  if (prev_LB_type == LB_Numeric &&
	      break_type == BUI_LB_POSTFIX)
	    break_op = BREAK_PROHIBITED; /* NU ( SY | IS )* × PO */

	  if (prev_LB_type == LB_Numeric &&
	      break_type == BUI_LB_PREFIX)
	    break_op = BREAK_PROHIBITED; /* NU ( SY | IS )* × PR */

	  if (prev_break_type == BUI_LB_POSTFIX &&
	      break_type == BUI_LB_OPEN_PUNCTUATION &&
	      next_break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* PO × OP NU */

	  if (prev_break_type == BUI_LB_POSTFIX &&
	      break_type == BUI_LB_OPEN_PUNCTUATION &&
	      next_break_type == BUI_LB_INFIX_SEPARATOR &&
	      next_next_break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* PO × OP IS NU */

	  if (prev_break_type == BUI_LB_POSTFIX &&
	      break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* PO × NU */

	  if (prev_break_type == BUI_LB_PREFIX &&
	      break_type == BUI_LB_OPEN_PUNCTUATION &&
	      next_break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* PR × OP NU */

	  if (prev_break_type == BUI_LB_PREFIX &&
	      break_type == BUI_LB_OPEN_PUNCTUATION &&
	      next_break_type == BUI_LB_INFIX_SEPARATOR &&
	      next_next_break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* PR × OP IS NU */

	  if (prev_break_type == BUI_LB_PREFIX &&
	      break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* PR × NU */

	  if (prev_break_type == BUI_LB_HYPHEN &&
	      break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* HY × NU */

	  if (prev_break_type == BUI_LB_INFIX_SEPARATOR &&
	      break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* IS × NU */

	  if (prev_LB_type == LB_Numeric &&
	      break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED; /* NU ( SY | IS )* × NU */

	  /* Rule LB24 */
	  if ((prev_break_type == BUI_LB_PREFIX ||
	       prev_break_type == BUI_LB_POSTFIX) &&
	      (break_type == BUI_LB_ALPHABETIC ||
	       break_type == BUI_LB_HEBREW_LETTER))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_break_type == BUI_LB_ALPHABETIC ||
	       prev_break_type == BUI_LB_HEBREW_LETTER) &&
	      (break_type == BUI_LB_PREFIX ||
	       break_type == BUI_LB_POSTFIX))
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB23a */
	  if (prev_break_type == BUI_LB_PREFIX &&
	      (break_type == BUI_LB_IDEOGRAPHIC ||
	       break_type == BUI_LB_EMOJI_BASE ||
	       break_type == BUI_LB_EMOJI_MODIFIER))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_break_type == BUI_LB_IDEOGRAPHIC ||
	       prev_break_type == BUI_LB_EMOJI_BASE ||
	       prev_break_type == BUI_LB_EMOJI_MODIFIER) &&
	      break_type == BUI_LB_POSTFIX)
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB23 */
	  if ((prev_break_type == BUI_LB_ALPHABETIC ||
	       prev_break_type == BUI_LB_HEBREW_LETTER) &&
	      break_type == BUI_LB_NUMERIC)
	    break_op = BREAK_PROHIBITED;

	  if (prev_break_type == BUI_LB_NUMERIC &&
	      (break_type == BUI_LB_ALPHABETIC ||
	       break_type == BUI_LB_HEBREW_LETTER))
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB22 */
	  if (break_type == BUI_LB_INSEPARABLE)
	    break_op = BREAK_PROHIBITED;

	  if (prev_break_type == BUI_LB_SYMBOL &&
	      break_type == BUI_LB_HEBREW_LETTER)
	    break_op = BREAK_PROHIBITED; /* Rule LB21b */

	  if (prev_prev_break_type == BUI_LB_HEBREW_LETTER &&
	      (prev_break_type == BUI_LB_HYPHEN ||
	       prev_break_type == BUI_LB_UNAMBIGUOUS_HYPHEN) &&
	      break_type != BUI_LB_HEBREW_LETTER)
	    break_op = BREAK_PROHIBITED; /* Rule LB21a */

	  if (break_type == BUI_LB_AFTER ||
	      break_type == BUI_LB_UNAMBIGUOUS_HYPHEN ||
	      break_type == BUI_LB_HYPHEN ||
	      break_type == BUI_LB_NON_STARTER ||
	      prev_break_type == BUI_LB_BEFORE)
	    break_op = BREAK_PROHIBITED; /* Rule LB21 */

	  /* Rule LB20a */
	  if ((prev_LB_i == 0 ||
	       prev_prev_break_type == BUI_LB_MANDATORY ||
	       prev_prev_break_type == BUI_LB_CARRIAGE_RETURN ||
	       prev_prev_break_type == BUI_LB_LINE_FEED ||
	       prev_prev_break_type == BUI_LB_NEXT_LINE ||
	       prev_prev_break_type == BUI_LB_SPACE ||
	       prev_prev_break_type == BUI_LB_ZERO_WIDTH_SPACE ||
	       prev_prev_break_type == BUI_LB_CONTINGENT ||
	       prev_prev_break_type == BUI_LB_NON_BREAKING_GLUE) &&
	      (prev_break_type == BUI_LB_HYPHEN ||
	       prev_break_type == BUI_LB_UNAMBIGUOUS_HYPHEN) &&
	      (break_type == BUI_LB_ALPHABETIC ||
	       break_type == BUI_LB_HEBREW_LETTER))
	     break_op = BREAK_PROHIBITED;

	  if (prev_break_type == BUI_LB_CONTINGENT ||
	      break_type == BUI_LB_CONTINGENT)
	    break_op = BREAK_ALLOWED; /* Rule LB20 */

	  /* Rule LB19a */
	  if (!bui_is_EastAsianWide(prev_wc) &&
	      break_type == BUI_LB_QUOTATION)
	    break_op = BREAK_PROHIBITED;

	  if (break_type == BUI_LB_QUOTATION &&
	      (!bui_is_EastAsianWide(next_wc) || done))
	    break_op = BREAK_PROHIBITED;

	  if (prev_break_type == BUI_LB_QUOTATION &&
	      !bui_is_EastAsianWide(wc))
	    break_op = BREAK_PROHIBITED;

	  if ((prev_LB_i == 0 || !bui_is_EastAsianWide(prev_prev_wc)) &&
	      prev_break_type == BUI_LB_QUOTATION)
	    break_op = BREAK_PROHIBITED;

	  /* Rule LB19 */
	  if (break_type == BUI_LB_QUOTATION &&
	      type != HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION)
	    break_op = BREAK_PROHIBITED;

	  if (prev_break_type == BUI_LB_QUOTATION &&
	      text_gc(prev_wc) != HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION)
	    break_op = BREAK_PROHIBITED;

	  /* handle related rules for Space as state machine here,
	     and override the pair table result. */
	  if (prev_break_type == BUI_LB_SPACE) /* Rule LB18 */
	    break_op = BREAK_ALLOWED;

	  if (row_break_type == BUI_LB_BEFORE_AND_AFTER &&
	      break_type == BUI_LB_BEFORE_AND_AFTER)
	    break_op = BREAK_PROHIBITED; /* Rule LB17 */

	  if ((row_break_type == BUI_LB_CLOSE_PUNCTUATION ||
	       row_break_type == BUI_LB_CLOSE_PARENTHESIS) &&
	      break_type == BUI_LB_NON_STARTER)
	    break_op = BREAK_PROHIBITED; /* Rule LB16 */

	  /* Rule LB15d */
	  if (break_type == BUI_LB_INFIX_SEPARATOR)
	      break_op = BREAK_PROHIBITED;

	  /* Rule LB15c */
	  if (prev_break_type == BUI_LB_SPACE &&
	      break_type == BUI_LB_INFIX_SEPARATOR &&
	      next_break_type == BUI_LB_NUMERIC)
	      break_op = BREAK_ALLOWED;

	  /* Rule LB15b */
	  if (type == HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION &&
	      break_type == BUI_LB_QUOTATION)
	    {
	      if (next_wc == 0 ||
		  next_break_type == BUI_LB_SPACE ||
		  next_break_type == BUI_LB_NON_BREAKING_GLUE ||
		  next_break_type == BUI_LB_WORD_JOINER ||
		  next_break_type == BUI_LB_CLOSE_PUNCTUATION ||
		  next_break_type == BUI_LB_QUOTATION ||
		  next_break_type == BUI_LB_CLOSE_PARENTHESIS ||
		  next_break_type == BUI_LB_EXCLAMATION ||
		  next_break_type == BUI_LB_INFIX_SEPARATOR ||
		  next_break_type == BUI_LB_SYMBOL ||
		  next_break_type == BUI_LB_MANDATORY ||
		  next_break_type == BUI_LB_CARRIAGE_RETURN ||
		  next_break_type == BUI_LB_LINE_FEED ||
		  next_break_type == BUI_LB_NEXT_LINE ||
		  next_break_type == BUI_LB_ZERO_WIDTH_SPACE)
		break_op = BREAK_PROHIBITED;
	    }

	  /* Rule LB15a */
	  if (met_LB15a &&
	      break_type != BUI_LB_SPACE &&
	      /* Rule LB9 */
	      break_type != BUI_LB_COMBINING_MARK &&
	      break_type != BUI_LB_ZERO_WIDTH_JOINER)
	    {
	      met_LB15a = 0;
	      break_op = BREAK_PROHIBITED;
	    }
	  else if (type == HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION &&
	      break_type == BUI_LB_QUOTATION)
	    {
	      if (i == 0 ||
		  prev_break_type == BUI_LB_MANDATORY ||
		  prev_break_type == BUI_LB_CARRIAGE_RETURN ||
		  prev_break_type == BUI_LB_LINE_FEED ||
		  prev_break_type == BUI_LB_NEXT_LINE ||
		  prev_break_type == BUI_LB_OPEN_PUNCTUATION ||
		  prev_break_type == BUI_LB_QUOTATION ||
		  prev_break_type == BUI_LB_NON_BREAKING_GLUE ||
		  prev_break_type == BUI_LB_SPACE ||
		  prev_break_type == BUI_LB_ZERO_WIDTH_SPACE)
		met_LB15a = 1;
	    }

	  if (row_break_type == BUI_LB_OPEN_PUNCTUATION)
	    break_op = BREAK_PROHIBITED; /* Rule LB14 */

	  if (break_type == BUI_LB_CLOSE_PUNCTUATION ||
	      break_type == BUI_LB_CLOSE_PARENTHESIS ||
	      break_type == BUI_LB_EXCLAMATION ||
	      break_type == BUI_LB_SYMBOL)
	    break_op = BREAK_PROHIBITED; /* Rule LB13 */

	  if (break_type == BUI_LB_NON_BREAKING_GLUE &&
	      (prev_break_type != BUI_LB_SPACE &&
	       prev_break_type != BUI_LB_AFTER &&
	       prev_break_type != BUI_LB_HYPHEN &&
	       prev_break_type != BUI_LB_UNAMBIGUOUS_HYPHEN))
	    break_op = BREAK_PROHIBITED; /* Rule LB12a */

	  if (prev_break_type == BUI_LB_NON_BREAKING_GLUE)
	    break_op = BREAK_PROHIBITED; /* Rule LB12 */

	  if (prev_break_type == BUI_LB_WORD_JOINER ||
	      break_type == BUI_LB_WORD_JOINER)
	    break_op = BREAK_PROHIBITED; /* Rule LB11 */

	  /* Rule LB9 */
	  if (break_type == BUI_LB_COMBINING_MARK ||
              break_type == BUI_LB_ZERO_WIDTH_JOINER)
	    {
	      if (!(prev_break_type == BUI_LB_MANDATORY ||
		    prev_break_type == BUI_LB_CARRIAGE_RETURN ||
		    prev_break_type == BUI_LB_LINE_FEED ||
		    prev_break_type == BUI_LB_NEXT_LINE ||
		    prev_break_type == BUI_LB_SPACE ||
		    prev_break_type == BUI_LB_ZERO_WIDTH_SPACE))
		break_op = BREAK_PROHIBITED;

	      if (met_LB15a)
		break_op = BREAK_PROHIBITED;
	    }
	  else
	    {
	      if (text_is_ext_pict (wc) &&
		  text_gc (wc) == HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED)
		met_Ext_Pict_Unassigned = 1;
	    }

	  if (prev_wc == 0x200D)
	    break_op = BREAK_PROHIBITED; /* Rule LB8a */

	  if (row_break_type == BUI_LB_ZERO_WIDTH_SPACE)
	    break_op = BREAK_ALLOWED; /* Rule LB8 */

	  if (break_type == BUI_LB_SPACE ||
	      break_type == BUI_LB_ZERO_WIDTH_SPACE)
	    break_op = BREAK_PROHIBITED; /* Rule LB7 */

	  /* Rule LB6 */
	  if (break_type == BUI_LB_MANDATORY ||
	      break_type == BUI_LB_CARRIAGE_RETURN ||
	      break_type == BUI_LB_LINE_FEED ||
	      break_type == BUI_LB_NEXT_LINE)
	    break_op = BREAK_PROHIBITED;

	  /* Rules LB4 and LB5 */
	  if (prev_break_type == BUI_LB_MANDATORY ||
	      (prev_break_type == BUI_LB_CARRIAGE_RETURN &&
	       wc != '\n') ||
	      prev_break_type == BUI_LB_LINE_FEED ||
	      prev_break_type == BUI_LB_NEXT_LINE)
	    {
	      attrs[i].is_mandatory_break = 1;
	      break_op = BREAK_ALLOWED;
	    }

	  switch (break_op)
	    {
	    case BREAK_PROHIBITED:
	      /* can't break here */
	      attrs[i].is_line_break = 0;
	      break;

	    case BREAK_IF_SPACES:
	      /* break if prev char was space */
	      if (prev_break_type != BUI_LB_SPACE)
		attrs[i].is_line_break = 0;
	      break;

	    case BREAK_ALLOWED:
	      attrs[i].is_line_break = 1;
	      break;

	    case BREAK_ALREADY_HANDLED:
	      break;

	    default:
	      break;
	    }

	  /* Rule LB9 */
	  if (!(break_type == BUI_LB_COMBINING_MARK ||
		break_type == BUI_LB_ZERO_WIDTH_JOINER))
	    {
	      /* Rule LB25 without Example 7 of Customization */
	      if (break_type == BUI_LB_SYMBOL ||
		    break_type == BUI_LB_INFIX_SEPARATOR)
		{
		  if (prev_LB_type != LB_Numeric)
		    {
		      prev_prev_LB_type = prev_LB_type;
		      prev_LB_type = LB_type;
		    }
		  /* else don't change the prev_LB_type */
		}
	      else
		{
		  prev_prev_LB_type = prev_LB_type;
		  prev_LB_type = LB_type;
		}
	    }
	  /* else don't change the prev_LB_type for Rule LB9 */
	}

      if (break_type != BUI_LB_SPACE)
	{
	  /* Rule LB9 */
	  if (break_type == BUI_LB_COMBINING_MARK ||
	      break_type == BUI_LB_ZERO_WIDTH_JOINER)
	    {
	      if (i == 0 /* start of text */ ||
		  prev_break_type == BUI_LB_MANDATORY ||
		  prev_break_type == BUI_LB_CARRIAGE_RETURN ||
		  prev_break_type == BUI_LB_LINE_FEED ||
		  prev_break_type == BUI_LB_NEXT_LINE ||
		  prev_break_type == BUI_LB_SPACE ||
		  prev_break_type == BUI_LB_ZERO_WIDTH_SPACE)
		prev_break_type = BUI_LB_ALPHABETIC; /* Rule LB10 */
	      /* else don't change the prev_break_type for Rule LB9 */
	    }
	  else
	    {
	      prev_prev_break_type = prev_break_type;
	      prev_break_type = break_type;
	      prev_LB_i = i;
	    }

	  prev_jamo = jamo;
	}
      else
	{
	  if (prev_break_type != BUI_LB_SPACE)
	    {
	      prev_prev_break_type = prev_break_type;
	      prev_break_type = break_type;
	      prev_LB_i = i;
	    }
	  /* else don't change the prev_break_type */
	}

      /* ---- Word breaks ---- */

      /* default to not a word start/end */
      attrs[i].is_word_start = 0;
      attrs[i].is_word_end = 0;

      if (current_word_type != WordNone)
	{
	  /* Check for a word end */
	  switch ((int) type)
	    {
	    case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
	    case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
	    case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
	    case HB_UNICODE_GENERAL_CATEGORY_FORMAT:
	      /* nothing, we just eat these up as part of the word */
	      break;

	    case HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER:
	      if (current_word_type == WordLetters)
		{
		  /* Japanese special cases for ending the word */
		  if (JAPANESE (last_word_letter) ||
		      JAPANESE (wc))
		    {
		      if ((HIRAGANA (last_word_letter) &&
			   !HIRAGANA (wc)) ||
			  (KATAKANA (last_word_letter) &&
			   !(KATAKANA (wc) || HIRAGANA (wc))) ||
			  (KANJI (last_word_letter) &&
			   !(HIRAGANA (wc) || KANJI (wc))) ||
			  (JAPANESE (last_word_letter) &&
			   !JAPANESE (wc)) ||
			  (!JAPANESE (last_word_letter) &&
			   JAPANESE (wc)))
			{
			  attrs[i].is_word_start = 1;
			  attrs[i].is_word_end = 1;
			}
		    }
		}
	      last_word_letter = wc;
	      break;

	    case HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER:
	    case HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER:
	    case HB_UNICODE_GENERAL_CATEGORY_OTHER_NUMBER:
	      last_word_letter = wc;
	      break;

	    default:
	      /* Punctuation, control/format chars, etc. all end a word. */
	      attrs[i].is_word_end = 1;
	      current_word_type = WordNone;
	      break;
	    }
	}
      else
	{
	  /* Check for a word start */
	  switch ((int) type)
	    {
	    case HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER:
	    case HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER:
	      current_word_type = WordLetters;
	      last_word_letter = wc;
	      attrs[i].is_word_start = 1;
	      break;

	    case HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER:
	    case HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER:
	    case HB_UNICODE_GENERAL_CATEGORY_OTHER_NUMBER:
	      current_word_type = WordNumbers;
	      last_word_letter = wc;
	      attrs[i].is_word_start = 1;
	      break;

	    default:
	      /* No word here */
	      break;
	    }
	}

      /* --- Hyphens --- */

      {
        int insert_hyphens;
        int space_or_hyphen = 0;

        attrs[i].break_inserts_hyphen = 0;
        attrs[i].break_removes_preceding = 0;

        switch ((int)prev_script)
          {
          case HB_SCRIPT_COMMON:
            insert_hyphens = prev_wc == 0x00ad;
            break;
          case HB_SCRIPT_HAN:
          case HB_SCRIPT_HANGUL:
          case HB_SCRIPT_HIRAGANA:
          case HB_SCRIPT_KATAKANA:
            insert_hyphens = 0;
            break;
          default:
            insert_hyphens = 1;
            break;
          }

        switch ((int)type)
          {
          case HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR:
          case HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR:
          case HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR:
            space_or_hyphen = 1;
            break;
          case HB_UNICODE_GENERAL_CATEGORY_CONTROL:
            if (wc == '\t' || wc == '\n' || wc == '\r' || wc == '\f')
              space_or_hyphen = 1;
            break;
          default:
            break;
          }

        if (!space_or_hyphen)
          {
            if (wc == '-'    || /* Hyphen-minus */
                wc == 0x058a || /* Armenian hyphen */
                wc == 0x1400 || /* Canadian syllabics hyphen */
                wc == 0x1806 || /* Mongolian todo hyphen */
                wc == 0x2010 || /* Hyphen */
                wc == 0x2e17 || /* Double oblique hyphen */
                wc == 0x2e40 || /* Double hyphen */
                wc == 0x30a0 || /* Katakana-Hiragana double hyphen */
                wc == 0xfe63 || /* Small hyphen-minus */
                wc == 0xff0d)   /* Fullwidth hyphen-minus */
              space_or_hyphen = 1;
          }

        if (attrs[i].is_word_boundary)
          attrs[i].break_inserts_hyphen = 0;
        else if (prev_space_or_hyphen)
          attrs[i].break_inserts_hyphen = 0;
        else if (space_or_hyphen)
          attrs[i].break_inserts_hyphen = 0;
        else
          attrs[i].break_inserts_hyphen = insert_hyphens;

        if (prev_wc == 0x2027)     /* Hyphenation point */
          {
            attrs[i].break_inserts_hyphen = 1;
            attrs[i].break_removes_preceding = 1;
          }

        prev_space_or_hyphen = space_or_hyphen;
      }

      prev_prev_wc = prev_wc;
      prev_wc = wc;
      prev_script = script;

      /* wc might not be a valid Unicode base character, but really all we
       * need to know is the last non-combining character */
      if (type != HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK &&
	  type != HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK &&
	  type != HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
	base_character = wc;
    }

  i--;

  attrs[0].is_cursor_position = 1;  /* Rule GB1 */
  attrs[i].is_cursor_position = 1;  /* Rule GB2 */

  attrs[0].is_word_boundary = 1;  /* Rule WB1 */
  attrs[i].is_word_boundary = 1;  /* Rule WB2 */

  attrs[0].is_line_break = 0; /* Rule LB2 */
  attrs[i].is_line_break = 1;  /* Rule LB3 */
  attrs[i].is_mandatory_break = 1;  /* Rule LB3 */
}


#define ALEF_WITH_MADDA_ABOVE	0x0622
#define YEH_WITH_HAMZA_ABOVE	0x0626
#define ALEF			0x0627
#define WAW			0x0648
#define YEH			0x064A

#define MADDAH_ABOVE		0x0653
#define HAMZA_ABOVE		0x0654
#define HAMZA_BELOW		0x0655

/*
 * Arabic characters with canonical decompositions that are not just
 * ligatures.  The characters U+06C0, U+06C2, and U+06D3 are intentionally
 * excluded as they are marked as "not an independent letter" in Unicode
 * Character Database's NamesList.txt
 */
#define IS_COMPOSITE(c) (ALEF_WITH_MADDA_ABOVE <= (c) && (c) <= YEH_WITH_HAMZA_ABOVE)

/* If a character is the second part of a composite Arabic character with an Alef */
#define IS_COMPOSITE_WITH_ALEF(c) (MADDAH_ABOVE <= (c) && (c) <= HAMZA_BELOW)

/* Pango's tailoring runs on one item (one script, font, level...) at a
 * time: `cp`, `n` are its characters, attrs[0..n] the boundaries around
 * them. */
static void
break_arabic (const uint32_t *cp,
	      int             n,
	      BuiLogAttr     *attrs)
{
  int i;
  uint32_t prev_wc, this_wc;

  /* See http://bugzilla.gnome.org/show_bug.cgi?id=350132 for issues this
   * module tries to solve.
   */

  for (i = 0, prev_wc = 0; i < n; i++, prev_wc = this_wc)
    {
      this_wc = cp[i];

      /*
       * Unset backspace_deletes_character for various combinations.
       *
       * A few more combinations may need to be handled here, but are not
       * handled yet, as expectations of users is not known or may differ
       * among different languages or users:
       * some letters combined with U+0658 ARABIC MARK NOON GHUNNA;
       * combinations considered one letter in Azerbaijani (WAW+SUKUN and
       * FARSI_YEH+HAMZA_ABOVE); combinations of YEH and ALEF_MAKSURA with
       * HAMZA_BELOW (Qur'anic); TATWEEL+HAMZA_ABOVE (Qur'anic).
       */
      if (IS_COMPOSITE (this_wc) ||
	  (prev_wc == ALEF && IS_COMPOSITE_WITH_ALEF (this_wc)) ||
	  (this_wc == HAMZA_ABOVE && (prev_wc == WAW || prev_wc == YEH)))
	attrs[i+1].backspace_deletes_character = 0;
    }
}

#define DEV_RRA 0x0931 /* 0930 + 093c */
#define DEV_QA 0x0958 /* 0915 + 093c */
#define DEV_YA 0x095F /* 092f + 003c */
#define DEV_KHHA 0x0959
#define DEV_GHHA 0x095A
#define DEV_ZA 0x095B
#define DEV_DDDHA 0x095C
#define DEV_RHA 0x095D
#define DEV_FA 0x095E
#define DEV_YYA 0x095F

/* Bengali */
/* for split matras in all brahmi based script */
#define BENGALI_SIGN_O 0x09CB  /* 09c7 + 09be */
#define BENGALI_SIGN_AU 0x09CC /* 09c7 + 09d7 */
#define BENGALI_RRA 0x09DC
#define BENGALI_RHA 0x09DD
#define BENGALI_YYA 0x09DF

/* Gurumukhi */
#define GURUMUKHI_LLA 0x0A33
#define GURUMUKHI_SHA 0x0A36
#define GURUMUKHI_KHHA 0x0A59
#define GURUMUKHI_GHHA 0x0A5A
#define GURUMUKHI_ZA 0x0A5B
#define GURUMUKHI_RRA 0x0A5C
#define GURUMUKHI_FA 0x0A5E

/* Oriya */
#define ORIYA_AI 0x0B48
#define ORIYA_O 0x0B4B
#define ORIYA_AU 0x0B4C

/* Telugu */
#define TELUGU_EE 0x0C47
#define TELUGU_AI 0x0C48

/* Tamil */
#define TAMIL_O 0x0BCA
#define TAMIL_OO 0x0BCB
#define TAMIL_AU 0x0BCC

/* Kannada */
#define KNDA_EE 0x0CC7
#define KNDA_AI 0x0CC8
#define KNDA_O 0x0CCA
#define KNDA_OO 0x0CCB

/* Malayalam */
#define MLYM_O 0x0D4A
#define MLYM_OO 0x0D4B
#define MLYM_AU 0x0D4C

#define IS_COMPOSITE_WITH_BRAHMI_NUKTA(c) ( \
	(c >= BENGALI_RRA  && c <= BENGALI_YYA) || \
	(c >= DEV_QA  && c <= DEV_YA) || (c == DEV_RRA) || (c >= DEV_KHHA  && c <= DEV_YYA) || \
	(c >= KNDA_EE  && c <= KNDA_AI) ||(c >= KNDA_O  && c <= KNDA_OO) || \
	(c == TAMIL_O) || (c == TAMIL_OO) || (c == TAMIL_AU) || \
	(c == TELUGU_EE) || (c == TELUGU_AI) || \
	(c == ORIYA_AI) || (c == ORIYA_O) || (c == ORIYA_AU) || \
	(c >= GURUMUKHI_KHHA  && c <= GURUMUKHI_RRA) || (c == GURUMUKHI_FA)|| (c == GURUMUKHI_LLA)|| (c == GURUMUKHI_SHA) || \
	0)
#define IS_SPLIT_MATRA_BRAHMI(c) ( \
	(c == BENGALI_SIGN_O) || (c == BENGALI_SIGN_AU) || \
	(c >= MLYM_O  && c <= MLYM_AU) || \
	0)

static void
not_cursor_position (BuiLogAttr *attr)
{
  if (!attr->is_mandatory_break)
    {
      attr->is_cursor_position = 0;
      attr->is_char_break = 0;
      attr->is_line_break = 0;
      attr->is_mandatory_break = 0;
    }
}

static void
break_indic (const uint32_t *cp,
	     int             n,
	     hb_script_t     script,
	     BuiLogAttr     *attrs)
{
  uint32_t prev_wc, this_wc, next_wc, next_next_wc;
  int is_conjunct = 0;
  int i;

  for (prev_wc = 0, i = 0; i < n; prev_wc = this_wc, i++)
    {
      this_wc = cp[i];

      if (IS_COMPOSITE_WITH_BRAHMI_NUKTA(this_wc) || IS_SPLIT_MATRA_BRAHMI(this_wc))
	attrs[i+1].backspace_deletes_character = 0;

      next_wc = i + 1 < n ? cp[i + 1] : 0;
      next_next_wc = i + 1 < n && i + 2 < n ? cp[i + 2] : 0;

      switch (script)
      {
        case HB_SCRIPT_SINHALA:
	  /*
	   * TODO: The cursor position should be based on the state table.
	   *       This is the wrong place to be doing this.
	   */

	  /*
	   * The cursor should treat as a single glyph:
	   * SINHALA CONS + 0x0DCA + 0x200D + SINHALA CONS
	   * SINHALA CONS + 0x200D + 0x0DCA + SINHALA CONS
	   */
	  if ((this_wc == 0x0DCA && next_wc == 0x200D)
	      || (this_wc == 0x200D && next_wc == 0x0DCA))
	    {
	      not_cursor_position(&attrs[i]);
	      not_cursor_position(&attrs[i + 1]);
	      is_conjunct = 1;
	    }
	  else if (is_conjunct
		   && (prev_wc == 0x200D || prev_wc == 0x0DCA)
		   && this_wc >= 0x0D9A
		   && this_wc <= 0x0DC6)
	    {
	      not_cursor_position(&attrs[i]);
	      is_conjunct = 0;
	    }
	  /*
	   * Consonant clusters do NOT result in implicit conjuncts
	   * in SINHALA orthography.
	   */
	  else if (!is_conjunct && prev_wc == 0x0DCA && this_wc != 0x200D)
	    {
	      attrs[i].is_cursor_position = 1;
	    }

	  break;

	default:

	  if (prev_wc != 0 && (this_wc == 0x200D || this_wc == 0x200C))
	    {
	      not_cursor_position(&attrs[i]);
	      if (next_wc != 0)
		{
		  not_cursor_position(&attrs[i+1]);
		  if ((next_next_wc != 0) &&
		       (next_wc == 0x09CD ||	/* Bengali */
			next_wc == 0x0ACD ||	/* Gujarati */
			next_wc == 0x094D ||	/* Hindi */
			next_wc == 0x0CCD ||	/* Kannada */
			next_wc == 0x0D4D ||	/* Malayalam */
			next_wc == 0x0B4D ||	/* Oriya */
			next_wc == 0x0A4D ||	/* Punjabi */
			next_wc == 0x0BCD ||	/* Tamil */
			next_wc == 0x0C4D))	/* Telugu */
		    {
		      not_cursor_position(&attrs[i+2]);
		    }
		}
	    }

	  break;
      }
    }
}

/* Thai: libthai's dictionary word breaks, as Pango (built with libthai)
 * finds them. text_thai_breaks() (BUI) fills `brk` with the break
 * positions (character indices) of `n` characters and returns how many,
 * or -1 without libthai: Pango built without it adds none. */
static void
break_thai (const uint32_t *cp,
	    int             n,
	    BuiLogAttr     *attrs)
{
  int brk_stack[512];
  int *brk_pnts = brk_stack;
  int len, cnt;

  if (n + 1 > (int) (sizeof (brk_stack) / sizeof (brk_stack[0])))
    {
      brk_pnts = (int *) malloc (sizeof (int) * (size_t) (n + 1));
      if (!brk_pnts)
        return;
    }

  len = text_thai_breaks (cp, n, brk_pnts, n + 1);

  for (cnt = 0; cnt < len; cnt++)
    {
      if (brk_pnts[cnt] < 0 || brk_pnts[cnt] > n)
        continue;
      if (!attrs[brk_pnts[cnt]].is_line_break)
        {
          /* Insert line breaks where there wasn't one.
           * Satisfy invariants by marking it as char break too.
           */
          attrs[brk_pnts[cnt]].is_char_break = 1;
          attrs[brk_pnts[cnt]].is_line_break = 1;
        }
      if (!(attrs[brk_pnts[cnt]].is_word_start ||
            attrs[brk_pnts[cnt]].is_word_end))
        {
          /* If we find a break in the middle of a sequence
           * of characters, end and start a word. We must
           * be careful only to do that if default_break
           * did not already find a word start or end,
           * otherwise we mess up the sequence.
           */
          attrs[brk_pnts[cnt]].is_word_start = 1;
          attrs[brk_pnts[cnt]].is_word_end = 1;
        }
    }

  if (brk_pnts != brk_stack)
    free (brk_pnts);
}

/* `next` is the character after the item (Pango reads past the item's
 * end), 0 at the paragraph's end. */
static void
break_latin (const uint32_t *cp,
             int             n,
             uint32_t        next,
             const char     *lang,
             BuiLogAttr     *attrs)
{
  int i;
  uint32_t wc, prev_wc;

  if (!lang || !((lang[0] | 0x20) == 'c' && (lang[1] | 0x20) == 'a' && lang[2] == '-'))
    return;

  for (i = 0, prev_wc = 0; i < n; i++, prev_wc = wc)
    {
      wc = cp[i];

      /* Catalan middle dot does not break words */
      if (wc == 0x00b7)
        {
          uint32_t middle_next = i + 1 < n ? cp[i + 1] : next;
          if (text_tolower (middle_next) == 'l' && text_tolower (prev_wc) == 'l')
            {
              attrs[i].is_word_end = 0;
              attrs[i+1].is_word_start = 0;
            }
        }
    }
}

static int
break_script (const uint32_t *cp,
	      int             n,
	      uint32_t        next,
	      hb_script_t     script,
	      const char     *lang,
	      BuiLogAttr     *attrs)
{
  switch ((int) script)
    {
    case HB_SCRIPT_ARABIC:
      break_arabic (cp, n, attrs);
      break;

    case HB_SCRIPT_DEVANAGARI:
    case HB_SCRIPT_BENGALI:
    case HB_SCRIPT_GURMUKHI:
    case HB_SCRIPT_GUJARATI:
    case HB_SCRIPT_ORIYA:
    case HB_SCRIPT_TAMIL:
    case HB_SCRIPT_TELUGU:
    case HB_SCRIPT_KANNADA:
    case HB_SCRIPT_MALAYALAM:
    case HB_SCRIPT_SINHALA:
      break_indic (cp, n, script, attrs);
      break;

    case HB_SCRIPT_THAI:
      break_thai (cp, n, attrs);
      break;

    case HB_SCRIPT_LATIN:
      break_latin (cp, n, next, lang, attrs);
      break;

    default:
      return 0;
    }

  return 1;
}

/* pango_default_break: default_break, keeping the line break, mandatory
 * break and cursor position already at attrs[0] (the end of the paragraph
 * before). */
static void
pango_default_break (const uint32_t *cp,
                     int             n,
                     BuiLogAttr     *attrs)
{
  BuiLogAttr before = *attrs;

  default_break (cp, n, attrs);

  attrs->is_line_break      |= before.is_line_break;
  attrs->is_mandatory_break |= before.is_mandatory_break;
  attrs->is_cursor_position |= before.is_cursor_position;
}

/* pango_tailor_break of one item */
static void
pango_tailor_break (const uint32_t *cp,
                    int             n,
                    uint32_t        next,
                    hb_script_t     script,
                    const char     *lang,
                    BuiLogAttr     *attrs)
{
  BuiLogAttr *start = attrs;
  BuiLogAttr attr_before = *start;

  if (break_script (cp, n, next, script, lang, attrs))
    {
      /* if tailored, we enforce some of the attrs from before
       * tailoring at the boundary
       */

     start->backspace_deletes_character  = attr_before.backspace_deletes_character;

     start->is_line_break      |= attr_before.is_line_break;
     start->is_mandatory_break |= attr_before.is_mandatory_break;
     start->is_cursor_position |= attr_before.is_cursor_position;
    }
}
