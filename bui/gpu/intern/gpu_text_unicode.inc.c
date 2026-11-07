/* BUI-Awra: the Unicode both text engines need, independent of fonts
 * (included by gpu_raster.c):
 *
 *  - scripts (Pango's script iterator) and the languages they imply;
 *  - bidi levels (UAX #9) through FriBidi (extern/fribidi) as Pango
 *    resolves them, and a paragraph's direction from its first strong
 *    character;
 *  - text segmentation as Pango's break.c computes it (extern/pango-break):
 *    cursor positions and backspace (UAX #29), words, line breaks (UAX #14
 *    with Pango's tailorings, Thai words from libthai) and hyphenation;
 *  - canonical decomposition (NFD) through HarfBuzz's Unicode data, for
 *    backspace as GTK does it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "../extern/emoji/emoji_tables.h"
#include "../extern/pango-break/break_props.h"

#include <hb.h>
#include <hb-ot.h>

#include <fribidi-bidi.h>
#include <fribidi-bidi-types.h>
#include <fribidi-brackets.h>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#  include <dlfcn.h>
#endif

/* UTF-8 decode; invalid bytes map to U+FFFD. */
static int utf8_next(const char *s, int len, int *i)
{
  const unsigned char c = (unsigned char)s[*i];
  int cp, n;
  if (c < 0x80) {
    cp = c;
    n = 1;
  }
  else if ((c & 0xE0) == 0xC0) {
    cp = c & 0x1F;
    n = 2;
  }
  else if ((c & 0xF0) == 0xE0) {
    cp = c & 0x0F;
    n = 3;
  }
  else if ((c & 0xF8) == 0xF0) {
    cp = c & 0x07;
    n = 4;
  }
  else {
    (*i)++;
    return 0xFFFD;
  }
  if (*i + n > len) {
    *i = len;
    return 0xFFFD;
  }
  for (int k = 1; k < n; k++) {
    const unsigned char cc = (unsigned char)s[*i + k];
    if ((cc & 0xC0) != 0x80) {
      *i += k;
      return 0xFFFD;
    }
    cp = (cp << 6) | (cc & 0x3F);
  }
  *i += n;
  return cp;
}


/* The length of the UTF-8 sequence starting at s[i]. */
static int utf8_step(const char *s, int i)
{
  const unsigned char c = (unsigned char)s[i];
  return c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
}

/* -------------------------------------------------------------------- */
/** \name Emoji properties
 * \{ */

static int emoji_in(uint32_t c, const EmojiInterval *t, int n)
{
  if (c < t[0].start || c > t[n - 1].end) {
    return 0;
  }
  int lo = 0, hi = n - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (c < t[mid].start) {
      hi = mid - 1;
    }
    else if (c > t[mid].end) {
      lo = mid + 1;
    }
    else {
      return 1;
    }
  }
  return 0;
}

#  define EMOJI_IS(name, c) \
    emoji_in((c), g_emoji_##name, (int)(sizeof(g_emoji_##name) / sizeof(g_emoji_##name[0])))

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scripts and languages
 *
 * Pango's script iterator (pango-script.c: paired punctuation takes the
 * script of its opening, Common and Inherited characters the script
 * around them) and derived languages (itemize.c: a script the user's
 * language doesn't write gets the language Pango samples it with).
 * \{ */

static hb_unicode_funcs_t *text_uf(void)
{
  static hb_unicode_funcs_t *uf = NULL;
  if (!uf) {
    uf = hb_unicode_funcs_get_default();
  }
  return uf;
}

static int text_real_script(hb_script_t s)
{
  return s != HB_SCRIPT_COMMON && s != HB_SCRIPT_INHERITED && s != HB_SCRIPT_UNKNOWN &&
         s != HB_SCRIPT_INVALID;
}

static int text_is_mark(uint32_t c)
{
  const hb_unicode_general_category_t gc = hb_unicode_general_category(text_uf(), c);
  return gc == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK ||
         gc == HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK ||
         gc == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK;
}

static int text_cluster_extender(uint32_t c)
{
  if (c < 128) {
    return 0;
  }
  return text_is_mark(c) || (c >= 0x200C && c <= 0x200D) || (c >= 0xFF9E && c <= 0xFF9F) ||
         (c >= 0x1F3FB && c <= 0x1F3FF) || (c >= 0xE0020 && c <= 0xE007F);
}

static const uint32_t g_paired[] = {
    0x0028, 0x0029, 0x003c, 0x003e, 0x005b, 0x005d, 0x007b, 0x007d, 0x00ab, 0x00bb, 0x0f3a,
    0x0f3b, 0x0f3c, 0x0f3d, 0x169b, 0x169c, 0x2018, 0x2019, 0x201c, 0x201d, 0x2039, 0x203a,
    0x2045, 0x2046, 0x207d, 0x207e, 0x208d, 0x208e, 0x27e6, 0x27e7, 0x27e8, 0x27e9, 0x27ea,
    0x27eb, 0x27ec, 0x27ed, 0x27ee, 0x27ef, 0x2983, 0x2984, 0x2985, 0x2986, 0x2987, 0x2988,
    0x2989, 0x298a, 0x298b, 0x298c, 0x298d, 0x298e, 0x298f, 0x2990, 0x2991, 0x2992, 0x2993,
    0x2994, 0x2995, 0x2996, 0x2997, 0x2998, 0x29fc, 0x29fd, 0x2e02, 0x2e03, 0x2e04, 0x2e05,
    0x2e09, 0x2e0a, 0x2e0c, 0x2e0d, 0x2e1c, 0x2e1d, 0x2e20, 0x2e21, 0x2e22, 0x2e23, 0x2e24,
    0x2e25, 0x2e26, 0x2e27, 0x2e28, 0x2e29, 0x3008, 0x3009, 0x300a, 0x300b, 0x300c, 0x300d,
    0x300e, 0x300f, 0x3010, 0x3011, 0x3014, 0x3015, 0x3016, 0x3017, 0x3018, 0x3019, 0x301a,
    0x301b, 0xfe59, 0xfe5a, 0xfe5b, 0xfe5c, 0xfe5d, 0xfe5e, 0xff08, 0xff09, 0xff3b, 0xff3d,
    0xff5b, 0xff5d, 0xff5f, 0xff60, 0xff62, 0xff63};

static int text_pair_index(uint32_t c)
{
  int lo = 0, hi = (int)(sizeof(g_paired) / sizeof(g_paired[0])) - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (c < g_paired[mid]) {
      hi = mid - 1;
    }
    else if (c > g_paired[mid]) {
      lo = mid + 1;
    }
    else {
      return mid;
    }
  }
  return -1;
}

/* Pango's pango_script_iter: each character's run script. */
static void text_scripts(const uint32_t *cp, int n, hb_script_t *out)
{
  struct {
    int pair;
    hb_script_t sc;
  } stack[128];
  int sp = -1;
  int i = 0;
  while (i < n) {
    int start_sp = sp;
    hb_script_t code = HB_SCRIPT_COMMON;
    const int start = i;
    for (; i < n; i++) {
      const uint32_t ch = cp[i];
      hb_script_t sc = ch < 128 ? (((ch | 0x20) >= 'a' && (ch | 0x20) <= 'z') ? HB_SCRIPT_LATIN : HB_SCRIPT_COMMON) :
                                  hb_unicode_script(text_uf(), ch);
      const int pi = sc == HB_SCRIPT_COMMON ? text_pair_index(ch) : -1;
      if (pi >= 0) {
        if ((pi & 1) == 0) {
          if (++sp >= 128) {
            sp = 0; /* too deep: forget the nesting, as Pango does */
          }
          stack[sp].pair = pi;
          stack[sp].sc = code;
        }
        else if (sp >= 0) {
          const int open = pi & ~1;
          while (sp >= 0 && stack[sp].pair != open) {
            sp--;
          }
          if (sp < start_sp) {
            start_sp = sp;
          }
          if (sp >= 0) {
            sc = stack[sp].sc;
          }
        }
      }
      if (!text_real_script(code) || !text_real_script(sc) || code == sc ||
          text_cluster_extender(ch))
      {
        if (!text_real_script(code) && text_real_script(sc)) {
          code = sc;
          while (start_sp < sp) {
            stack[++start_sp].sc = code;
          }
        }
        if (pi >= 0 && (pi & 1) && sp >= 0) {
          sp--;
          if (sp < start_sp) {
            start_sp = sp;
          }
        }
      }
      else {
        break;
      }
    }
    for (int k = start; k < i; k++) {
      out[k] = code;
    }
  }
}

/* The scripts languages are written in (Pango's language table, for the
 * languages people use most); a language missing here writes any script,
 * as an unknown language does in Pango. */
static const struct LangScripts {
  char lang[4];
  hb_script_t s0, s1, s2;
} g_lang_scripts[] = {
    {"af", HB_SCRIPT_LATIN}, {"az", HB_SCRIPT_LATIN}, {"ca", HB_SCRIPT_LATIN},
    {"cs", HB_SCRIPT_LATIN}, {"cy", HB_SCRIPT_LATIN}, {"da", HB_SCRIPT_LATIN},
    {"de", HB_SCRIPT_LATIN}, {"en", HB_SCRIPT_LATIN}, {"eo", HB_SCRIPT_LATIN},
    {"es", HB_SCRIPT_LATIN}, {"et", HB_SCRIPT_LATIN}, {"eu", HB_SCRIPT_LATIN},
    {"fi", HB_SCRIPT_LATIN}, {"fr", HB_SCRIPT_LATIN}, {"ga", HB_SCRIPT_LATIN},
    {"gl", HB_SCRIPT_LATIN}, {"hr", HB_SCRIPT_LATIN}, {"hu", HB_SCRIPT_LATIN},
    {"id", HB_SCRIPT_LATIN}, {"is", HB_SCRIPT_LATIN}, {"it", HB_SCRIPT_LATIN},
    {"lt", HB_SCRIPT_LATIN}, {"lv", HB_SCRIPT_LATIN}, {"ms", HB_SCRIPT_LATIN},
    {"mt", HB_SCRIPT_LATIN}, {"nb", HB_SCRIPT_LATIN}, {"nl", HB_SCRIPT_LATIN},
    {"nn", HB_SCRIPT_LATIN}, {"no", HB_SCRIPT_LATIN}, {"pl", HB_SCRIPT_LATIN},
    {"pt", HB_SCRIPT_LATIN}, {"ro", HB_SCRIPT_LATIN}, {"sk", HB_SCRIPT_LATIN},
    {"sl", HB_SCRIPT_LATIN}, {"sq", HB_SCRIPT_LATIN}, {"sv", HB_SCRIPT_LATIN},
    {"sw", HB_SCRIPT_LATIN}, {"tl", HB_SCRIPT_LATIN}, {"tr", HB_SCRIPT_LATIN},
    {"uz", HB_SCRIPT_LATIN}, {"vi", HB_SCRIPT_LATIN}, {"be", HB_SCRIPT_CYRILLIC},
    {"bg", HB_SCRIPT_CYRILLIC}, {"kk", HB_SCRIPT_CYRILLIC}, {"ky", HB_SCRIPT_CYRILLIC},
    {"mk", HB_SCRIPT_CYRILLIC}, {"mn", HB_SCRIPT_CYRILLIC}, {"ru", HB_SCRIPT_CYRILLIC},
    {"sr", HB_SCRIPT_CYRILLIC}, {"tg", HB_SCRIPT_CYRILLIC}, {"uk", HB_SCRIPT_CYRILLIC},
    {"el", HB_SCRIPT_GREEK}, {"ar", HB_SCRIPT_ARABIC}, {"fa", HB_SCRIPT_ARABIC},
    {"ps", HB_SCRIPT_ARABIC}, {"sd", HB_SCRIPT_ARABIC}, {"ug", HB_SCRIPT_ARABIC},
    {"ur", HB_SCRIPT_ARABIC}, {"he", HB_SCRIPT_HEBREW}, {"iw", HB_SCRIPT_HEBREW},
    {"yi", HB_SCRIPT_HEBREW}, {"hy", HB_SCRIPT_ARMENIAN}, {"ka", HB_SCRIPT_GEORGIAN},
    {"hi", HB_SCRIPT_DEVANAGARI}, {"mr", HB_SCRIPT_DEVANAGARI}, {"ne", HB_SCRIPT_DEVANAGARI},
    {"sa", HB_SCRIPT_DEVANAGARI}, {"bn", HB_SCRIPT_BENGALI}, {"as", HB_SCRIPT_BENGALI},
    {"pa", HB_SCRIPT_GURMUKHI}, {"gu", HB_SCRIPT_GUJARATI}, {"or", HB_SCRIPT_ORIYA},
    {"ta", HB_SCRIPT_TAMIL}, {"te", HB_SCRIPT_TELUGU}, {"kn", HB_SCRIPT_KANNADA},
    {"ml", HB_SCRIPT_MALAYALAM}, {"si", HB_SCRIPT_SINHALA}, {"th", HB_SCRIPT_THAI},
    {"lo", HB_SCRIPT_LAO}, {"km", HB_SCRIPT_KHMER}, {"my", HB_SCRIPT_MYANMAR},
    {"bo", HB_SCRIPT_TIBETAN}, {"dz", HB_SCRIPT_TIBETAN}, {"am", HB_SCRIPT_ETHIOPIC},
    {"ti", HB_SCRIPT_ETHIOPIC}, {"dv", HB_SCRIPT_THAANA}, {"ko", HB_SCRIPT_HANGUL, HB_SCRIPT_HAN},
    {"ja", HB_SCRIPT_HAN, HB_SCRIPT_HIRAGANA, HB_SCRIPT_KATAKANA}, {"zh", HB_SCRIPT_HAN},
};

/* 1: `lang` writes script `s`, 0: it doesn't, -1: unknown language. */
static int text_lang_writes(const char *lang, hb_script_t s)
{
  char primary[4] = {0};
  for (int i = 0; i < 3 && lang[i] && lang[i] != '-'; i++) {
    primary[i] = lang[i];
  }
  if (lang[0] && lang[1] && lang[2] && lang[2] != '-' && lang[3] && lang[3] != '-') {
    return -1; /* longer primary subtags (fil, chr...): not in the table */
  }
  for (size_t i = 0; i < sizeof(g_lang_scripts) / sizeof(g_lang_scripts[0]); i++) {
    const struct LangScripts *l = &g_lang_scripts[i];
    if (strcmp(l->lang, primary) == 0) {
      return s == l->s0 || (l->s1 && s == l->s1) || (l->s2 && s == l->s2);
    }
  }
  return -1;
}

/* Pango's pango_script_get_sample_language (sample_languages[]). */
static const char *text_sample_lang(hb_script_t s)
{
  switch (s) {
    case HB_SCRIPT_ARABIC: return "ar";
    case HB_SCRIPT_ARMENIAN: return "hy";
    case HB_SCRIPT_BENGALI: return "bn";
    case HB_SCRIPT_CHEROKEE: return "chr";
    case HB_SCRIPT_COPTIC: return "cop";
    case HB_SCRIPT_CYRILLIC: return "ru";
    case HB_SCRIPT_DEVANAGARI: return "hi";
    case HB_SCRIPT_ETHIOPIC: return "am";
    case HB_SCRIPT_GEORGIAN: return "ka";
    case HB_SCRIPT_GREEK: return "el";
    case HB_SCRIPT_GUJARATI: return "gu";
    case HB_SCRIPT_GURMUKHI: return "pa";
    case HB_SCRIPT_HANGUL: return "ko";
    case HB_SCRIPT_HEBREW: return "he";
    case HB_SCRIPT_HIRAGANA: return "ja";
    case HB_SCRIPT_KANNADA: return "kn";
    case HB_SCRIPT_KATAKANA: return "ja";
    case HB_SCRIPT_KHMER: return "km";
    case HB_SCRIPT_LAO: return "lo";
    case HB_SCRIPT_LATIN: return "en";
    case HB_SCRIPT_MALAYALAM: return "ml";
    case HB_SCRIPT_MONGOLIAN: return "mn";
    case HB_SCRIPT_MYANMAR: return "my";
    case HB_SCRIPT_ORIYA: return "or";
    case HB_SCRIPT_SINHALA: return "si";
    case HB_SCRIPT_SYRIAC: return "syr";
    case HB_SCRIPT_TAMIL: return "ta";
    case HB_SCRIPT_TELUGU: return "te";
    case HB_SCRIPT_THAANA: return "dv";
    case HB_SCRIPT_THAI: return "th";
    case HB_SCRIPT_TIBETAN: return "bo";
    case HB_SCRIPT_CANADIAN_SYLLABICS: return "iu";
    case HB_SCRIPT_TAGALOG: return "tl";
    case HB_SCRIPT_HANUNOO: return "hnn";
    case HB_SCRIPT_BUHID: return "bku";
    case HB_SCRIPT_TAGBANWA: return "tbw";
    case HB_SCRIPT_UGARITIC: return "uga";
    case HB_SCRIPT_BUGINESE: return "bug";
    case HB_SCRIPT_SYLOTI_NAGRI: return "syl";
    case HB_SCRIPT_OLD_PERSIAN: return "peo";
    case HB_SCRIPT_NKO: return "nqo";
    default: return NULL;
  }
}

/* The user's language (Pango's default language: the locale's, as GTK sets
 * it from the environment) and the languages they read
 * (PANGO_LANGUAGE / LANGUAGE), lowercased with '-' separators. */
static char g_lang_default[24];
static char g_lang_pref[8][16];
static int g_lang_npref = -1;

static void text_lang_norm(const char *in, char *out, int cap)
{
  int n = 0;
  for (; in[n] && in[n] != '.' && in[n] != '@' && n < cap - 1; n++) {
    const char c = in[n];
    out[n] = c == '_' ? '-' : (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
  }
  out[n] = 0;
  if (strcmp(out, "posix") == 0) {
    strcpy(out, "c");
  }
}

static void text_langs_init(void)
{
  if (g_lang_npref >= 0) {
    return;
  }
  g_lang_npref = 0;
  const char *loc = NULL;
#  ifdef _WIN32
  char name[LOCALE_NAME_MAX_LENGTH * 2];
  wchar_t wname[LOCALE_NAME_MAX_LENGTH];
  if (GetUserDefaultLocaleName(wname, LOCALE_NAME_MAX_LENGTH) > 0 &&
      WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, (int)sizeof(name), NULL, NULL) > 0)
  {
    loc = name;
  }
#  else
  static const char *const vars[] = {"LC_ALL", "LC_CTYPE", "LANG"};
  for (int i = 0; i < 3 && !loc; i++) {
    const char *v = getenv(vars[i]);
    if (v && v[0]) {
      loc = v;
    }
  }
#  endif
  text_lang_norm(loc ? loc : "c", g_lang_default, (int)sizeof(g_lang_default));
  const char *p = getenv("PANGO_LANGUAGE");
  if (!p) {
    p = getenv("LANGUAGE");
  }
  while (p && *p && g_lang_npref < 8) {
    while (*p == ':' || *p == ';' || *p == ',' || *p == ' ' || *p == '\t') {
      p++;
    }
    char item[16];
    int n = 0;
    while (p[n] && !strchr(":;, \t", p[n]) && n < 15) {
      item[n] = p[n];
      n++;
    }
    item[n] = 0;
    p += n;
    while (*p && !strchr(":;, \t", *p)) {
      p++; /* longer than we keep */
    }
    if (n > 0) {
      text_lang_norm(item, g_lang_pref[g_lang_npref], 16);
      g_lang_npref++;
    }
  }
}

/* Pango's compute_derived_language: the language of a run of script `s`. */
static const char *text_script_lang_find(hb_script_t s)
{
  if (!text_real_script(s) || text_lang_writes(g_lang_default, s) != 0) {
    return g_lang_default;
  }
  for (int i = 0; i < g_lang_npref; i++) {
    if (text_lang_writes(g_lang_pref[i], s) != 0) {
      return g_lang_pref[i];
    }
  }
  const char *sample = text_sample_lang(s);
  return sample ? sample : "xx";
}

static const char *text_script_lang(hb_script_t s)
{
  /* the answers, by script (a handful of scripts in any process) */
  static struct {
    hb_script_t script;
    const char *lang;
  } memo[16];
  static int n = 0;
  for (int i = 0; i < n; i++) {
    if (memo[i].script == s) {
      return memo[i].lang;
    }
  }
  text_langs_init();
  const char *lang = text_script_lang_find(s);
  if (n < 16) {
    memo[n].script = s;
    memo[n].lang = lang;
    n++;
  }
  return lang;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bidi (UAX #9 through FriBidi, as Pango resolves it)
 * \{ */

/* Pango's pango_find_base_dir: 1 LTR, 2 RTL, 0 no strong character. */
static int text_strong_dir(const uint32_t *cp, int n)
{
  for (int i = 0; i < n; i++) {
    const FriBidiCharType t = fribidi_get_bidi_type(cp[i]);
    if (FRIBIDI_IS_STRONG(t)) {
      return FRIBIDI_IS_RTL(t) ? 2 : 1;
    }
  }
  return 0;
}

/* Embedding levels of a paragraph of base direction `rtl`
 * (pango_log2vis_fill_embedding_levels, its shortcuts included). */
static void text_levels(const uint32_t *cp, int n, int rtl, uint8_t *lv)
{
  FriBidiCharType tl[96];
  FriBidiBracketType bl[96];
  FriBidiCharType *types = n <= 96 ? tl : (FriBidiCharType *)malloc(sizeof(*types) * (size_t)n);
  FriBidiBracketType *br = n <= 96 ? bl : (FriBidiBracketType *)malloc(sizeof(*br) * (size_t)n);
  FriBidiParType base = rtl ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR;
  if (!types || !br) {
    memset(lv, rtl ? 1 : 0, (size_t)n);
    goto done;
  }
  FriBidiCharType ored = 0, anded = FRIBIDI_TYPE_RLE;
  for (int i = 0; i < n; i++) {
    const FriBidiCharType t = fribidi_get_bidi_type(cp[i]);
    types[i] = t;
    ored |= t;
    if (FRIBIDI_IS_STRONG(t)) {
      anded &= t;
    }
    br[i] = t == FRIBIDI_TYPE_ON ? fribidi_get_bracket(cp[i]) : FRIBIDI_NO_BRACKET;
  }
  if (!FRIBIDI_IS_ISOLATE(ored) && !FRIBIDI_IS_RTL(ored) && !FRIBIDI_IS_ARABIC(ored) &&
      !FRIBIDI_IS_RTL(base))
  {
    memset(lv, 0, (size_t)n); /* all left-to-right */
  }
  else if (!FRIBIDI_IS_ISOLATE(ored) && !FRIBIDI_IS_NUMBER(ored) && FRIBIDI_IS_RTL(anded) &&
           FRIBIDI_IS_RTL(base))
  {
    memset(lv, 1, (size_t)n); /* all right-to-left */
  }
  else if (fribidi_get_par_embedding_levels_ex(types, br, n, &base, (FriBidiLevel *)lv) == 0) {
    memset(lv, 0, (size_t)n);
  }
done:
  if (types != tl) {
    free(types);
  }
  if (br != bl) {
    free(br);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Emoji segmentation (Pango's emoji iterator, Chromium's scanner)
 * \{ */

/* Pango's _pango_EmojiSegmentationCategory */
enum {
  EMOJI_CAT = 0,
  EMOJI_TEXT_PRESENTATION = 1,
  EMOJI_EMOJI_PRESENTATION = 2,
  EMOJI_MODIFIER_BASE = 3,
  EMOJI_MODIFIER = 4,
  EMOJI_VS_BASE = 5,
  REGIONAL_INDICATOR = 6,
  KEYCAP_BASE = 7,
  COMBINING_ENCLOSING_KEYCAP = 8,
  COMBINING_ENCLOSING_CIRCLE_BACKSLASH = 9,
  ZWJ = 10,
  VS15 = 11,
  VS16 = 12,
  TAG_BASE = 13,
  TAG_SEQUENCE = 14,
  TAG_TERM = 15,
  kMaxEmojiScannerCategory = 16
};

static unsigned char emoji_category(uint32_t c)
{
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' ') {
    return kMaxEmojiScannerCategory;
  }
  if (c >= '0' && c <= '9') {
    return KEYCAP_BASE;
  }
  switch (c) {
    case 0x20E3:
      return COMBINING_ENCLOSING_KEYCAP;
    case 0x20E0:
      return COMBINING_ENCLOSING_CIRCLE_BACKSLASH;
    case 0x200D:
      return ZWJ;
    case 0xFE0E:
      return VS15;
    case 0xFE0F:
      return VS16;
    case 0x1F3F4:
      return TAG_BASE;
    case 0xE007F:
      return TAG_TERM;
    default:
      break;
  }
  if ((c >= 0xE0030 && c <= 0xE0039) || (c >= 0xE0061 && c <= 0xE007A)) {
    return TAG_SEQUENCE;
  }
  if (c < 0xA9) { /* nothing below © is an emoji but the cases above */
    return c == '#' || c == '*' ? KEYCAP_BASE : kMaxEmojiScannerCategory;
  }
  if (EMOJI_IS(modifier_base, c)) {
    return EMOJI_MODIFIER_BASE;
  }
  if (EMOJI_IS(modifier, c)) {
    return EMOJI_MODIFIER;
  }
  if (c >= 0x1F1E6 && c <= 0x1F1FF) {
    return REGIONAL_INDICATOR;
  }
  if (EMOJI_IS(presentation, c)) {
    return EMOJI_EMOJI_PRESENTATION;
  }
  if (EMOJI_IS(emoji, c)) {
    return EMOJI_TEXT_PRESENTATION;
  }
  return kMaxEmojiScannerCategory;
}

typedef unsigned char *emoji_text_iter_t;
#  if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wunused-const-variable"
#    pragma GCC diagnostic ignored "-Wunused-variable"
#  endif
#  include "../extern/emoji/emoji_presentation_scanner.c"
#  if defined(__GNUC__)
#    pragma GCC diagnostic pop
#  endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text segmentation: Pango's break.c (extern/pango-break)
 *
 * Cursor positions, words, line breaks and hyphenation points as
 * pango_get_log_attrs finds them: the default rules (UAX #29, UAX #14 with
 * Pango's tailorings) for each paragraph, then each script run's
 * tailoring (Arabic and Indic backspace, Indic and Sinhala clusters, Thai
 * words from libthai, the Catalan middle dot).
 * \{ */

#  if defined(__GNUC__) && __GNUC__ >= 7
#    define TEXT_FALLTHROUGH __attribute__((fallthrough))
#  else
#    define TEXT_FALLTHROUGH ((void)0)
#  endif

/* Pango's PangoLogAttr (bytes, not bits: the rules write every field of
 * every character, and a byte is written in one go) */
typedef struct BuiLogAttr {
  uint8_t is_line_break;
  uint8_t is_mandatory_break;
  uint8_t is_char_break;
  uint8_t is_white;
  uint8_t is_cursor_position;
  uint8_t is_word_start;
  uint8_t is_word_end;
  uint8_t backspace_deletes_character;
  uint8_t is_expandable_space;
  uint8_t is_word_boundary;
  uint8_t break_inserts_hyphen;
  uint8_t break_removes_preceding;
} BuiLogAttr;

/* ASCII's general categories, looked up once (the common case of the
 * per-character properties below) */
static uint8_t g_ascii_gc[128];
static int g_ascii_gc_done;

static hb_unicode_general_category_t text_gc(uint32_t c)
{
  if (c < 128) {
    if (!g_ascii_gc_done) {
      for (uint32_t k = 0; k < 128; k++) {
        g_ascii_gc[k] = (uint8_t)hb_unicode_general_category(text_uf(), k);
      }
      g_ascii_gc_done = 1;
    }
    return (hb_unicode_general_category_t)g_ascii_gc[c];
  }
  return hb_unicode_general_category(text_uf(), c);
}

static hb_script_t text_script_of(uint32_t c)
{
  if (c < 128) {
    return ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') ? HB_SCRIPT_LATIN : HB_SCRIPT_COMMON;
  }
  return hb_unicode_script(text_uf(), c);
}

#  define text_is_emoji(c) EMOJI_IS(emoji, (c))
/* nothing below U+00A9 is Extended_Pictographic */
#  define text_is_ext_pict(c) ((c) >= 0xA9 && EMOJI_IS(extended_pictographic, (c)))
#  define text_tolower(c) ((c) >= 'A' && (c) <= 'Z' ? (c) + ('a' - 'A') : (c))

static int text_thai_breaks(const uint32_t *cp, int n, int *brk, int cap);
/* Pango's items of `cp` in font slot `slot` (gpu_text_shape.inc.c): n
 * flags marking where an item starts and each character's script, both
 * malloc'ed; left NULL when there is no such engine or no memory. */
static void text_font_items(int slot, const uint32_t *cp, int n, uint8_t **r_start, hb_script_t **r_script);

#  include "../extern/pango-break/line_break_table.h"
#  include "../extern/pango-break/pango_break.inc.c"

/* libthai, loaded on the first Thai text (Pango links it): word breaks
 * from its dictionary. Without it Thai breaks only where the default
 * rules allow, as Pango built without libthai. */
typedef struct ThBrk ThBrk;
static struct {
  int tried;
  ThBrk *brk;
  ThBrk *(*brk_new)(const char *);
  int (*find_breaks)(ThBrk *, const unsigned char *, int *, size_t);
  unsigned char (*uni2tis)(uint32_t);
} g_thai;

static int text_thai_load(void)
{
  if (g_thai.tried) {
    return g_thai.brk != NULL;
  }
  g_thai.tried = 1;
#  if defined(_WIN32)
  HMODULE h = LoadLibraryA("libthai-0.dll");
#    define TEXT_SYM(h, name) ((void *)GetProcAddress((h), (name)))
#  elif defined(__APPLE__)
  void *h = dlopen("libthai.0.dylib", RTLD_NOW | RTLD_LOCAL);
#    define TEXT_SYM(h, name) dlsym((h), (name))
#  elif defined(__EMSCRIPTEN__)
  void *h = NULL;
#    define TEXT_SYM(h, name) ((void)(h), (void *)NULL)
#  else
  void *h = dlopen("libthai.so.0", RTLD_NOW | RTLD_LOCAL);
#    define TEXT_SYM(h, name) dlsym((h), (name))
#  endif
  if (!h) {
    return 0;
  }
  *(void **)&g_thai.brk_new = TEXT_SYM(h, "th_brk_new");
  *(void **)&g_thai.find_breaks = TEXT_SYM(h, "th_brk_find_breaks");
  *(void **)&g_thai.uni2tis = TEXT_SYM(h, "th_uni2tis");
#  undef TEXT_SYM
  if (g_thai.brk_new && g_thai.find_breaks && g_thai.uni2tis) {
    g_thai.brk = g_thai.brk_new(NULL); /* the dictionary, kept for the process (as Pango) */
  }
  return g_thai.brk != NULL;
}

static int text_thai_breaks(const uint32_t *cp, int n, int *brk, int cap)
{
  if (!text_thai_load()) {
    return -1;
  }
  unsigned char tl[512];
  unsigned char *tis = n + 1 <= (int)sizeof(tl) ? tl : (unsigned char *)malloc((size_t)n + 1);
  if (!tis) {
    return -1;
  }
  for (int i = 0; i < n; i++) {
    tis[i] = g_thai.uni2tis(cp[i]);
  }
  tis[n] = 0;
  const int r = g_thai.find_breaks(g_thai.brk, tis, brk, (size_t)cap);
  if (tis != tl) {
    free(tis);
  }
  return r;
}

static int text_para_delim(uint32_t c)
{
  return c == '\n' || c == '\r' || c == 0x2029;
}

/* Whether a character's script has tailored breaks (break_script). */
static int text_needs_tailoring(const uint32_t *cp, int n)
{
  text_langs_init();
  const int catalan = strncmp(g_lang_default, "ca-", 3) == 0;
  for (int i = 0; i < n; i++) {
    if (cp[i] < 0x600 && !catalan) {
      continue;
    }
    switch ((int)text_script_of(cp[i])) {
      case HB_SCRIPT_ARABIC:
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
      case HB_SCRIPT_THAI:
        return 1;
      case HB_SCRIPT_LATIN:
        if (catalan) {
          return 1;
        }
        break;
      default:
        break;
    }
  }
  return 0;
}

/* The logical attributes of `cp` (attrs[0..n]) as PangoLayout finds them:
 * the default rules paragraph by paragraph (a paragraph ends after "\n",
 * "\r", "\r\n" or U+2029), then each item's tailoring. Items: where
 * `istart` (n flags, may be NULL) marks a new one, each with the script
 * `iscript` gives; else where the script or the emoji segment changes, as
 * pango_get_log_attrs tailors (PangoLayout also splits items where the
 * font changes, which only matters around Thai words). Paragraph
 * delimiters are in no item. */
static void text_log_attrs(const uint32_t *cp, int n, BuiLogAttr *attrs, const uint8_t *istart,
                           const hb_script_t *iscript)
{
  memset(attrs, 0, sizeof(BuiLogAttr) * (size_t)(n + 1));
  for (int p = 0;;) {
    int e = p;
    while (e < n && !text_para_delim(cp[e])) {
      e++;
    }
    const int next = e < n ? e + ((cp[e] == '\r' && e + 1 < n && cp[e + 1] == '\n') ? 2 : 1) : e;
    pango_default_break(cp + p, next - p, attrs + p);
    if (e >= n) {
      break; /* the last paragraph (empty after a final delimiter) */
    }
    p = next;
  }
  hb_script_t scl[64];
  unsigned char tyl[64];
  hb_script_t *sc = NULL;
  unsigned char *ty = NULL;
  if (!istart && !text_needs_tailoring(cp, n)) {
    return; /* no script here has tailored breaks */
  }
  if (!istart) {
    sc = n <= 64 ? scl : (hb_script_t *)malloc(sizeof(hb_script_t) * (size_t)n);
    ty = n <= 64 ? tyl : (unsigned char *)malloc((size_t)n);
    if (!sc || !ty) {
      goto out;
    }
    text_scripts(cp, n, sc);
    for (int j = 0; j < n; j++) {
      ty[j] = emoji_category(cp[j]);
    }
    /* ty[j] becomes 1 in an emoji segment, 0 elsewhere */
    for (int p = 0; p < n;) {
      bool is_emoji = false, has_vs = false;
      int q = (int)(scan_emoji_presentation(ty + p, ty + n, &is_emoji, &has_vs) - ty);
      if (q <= p) {
        q = p + 1;
      }
      memset(ty + p, is_emoji ? 1 : 0, (size_t)(q - p));
      p = q;
    }
    iscript = sc;
  }
  text_langs_init();
  for (int i = 0; i < n;) {
    if (text_para_delim(cp[i])) {
      i++;
      continue;
    }
    int j = i + 1;
    while (j < n && !text_para_delim(cp[j]) &&
           (istart ? !istart[j] : (sc[j] == sc[i] && ty[j] == ty[i])))
    {
      j++;
    }
    pango_tailor_break(cp + i, j - i, j < n ? cp[j] : 0, iscript[i], text_script_lang(iscript[i]),
                       attrs + i);
    i = j;
  }
out:
  if (sc != scl) {
    free(sc);
  }
  if (ty != tyl) {
    free(ty);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Decomposition and the public Unicode API
 * \{ */

/* The full canonical decomposition of `c` into `out` (at most `max`). */
static int text_decompose(uint32_t c, uint32_t *out, int max)
{
  hb_codepoint_t a = 0, b = 0;
  if (max <= 0) {
    return 0;
  }
  if (!hb_unicode_decompose(text_uf(), c, &a, &b)) {
    out[0] = c;
    return 1;
  }
  int n = text_decompose(a, out, max);
  if (b && n < max) {
    n += text_decompose(b, out + n, max - n);
  }
  return n;
}

static int text_utf8_put(uint32_t c, char *o)
{
  if (c < 0x80) {
    o[0] = (char)c;
    return 1;
  }
  if (c < 0x800) {
    o[0] = (char)(0xC0 | (c >> 6));
    o[1] = (char)(0x80 | (c & 0x3F));
    return 2;
  }
  if (c < 0x10000) {
    o[0] = (char)(0xE0 | (c >> 12));
    o[1] = (char)(0x80 | ((c >> 6) & 0x3F));
    o[2] = (char)(0x80 | (c & 0x3F));
    return 3;
  }
  o[0] = (char)(0xF0 | (c >> 18));
  o[1] = (char)(0x80 | ((c >> 12) & 0x3F));
  o[2] = (char)(0x80 | ((c >> 6) & 0x3F));
  o[3] = (char)(0x80 | (c & 0x3F));
  return 4;
}

int BUI_text_nfd(const char *utf8, int len, char *out, int cap)
{
  uint32_t buf[64];
  int n = 0;
  for (int i = 0; i < len && n < 64;) {
    n += text_decompose((uint32_t)utf8_next(utf8, len, &i), buf + n, 64 - n);
  }
  /* canonical ordering: marks by combining class, stably */
  for (int i = 1; i < n; i++) {
    const uint32_t c = buf[i];
    const unsigned cc = hb_unicode_combining_class(text_uf(), c);
    if (cc == 0) {
      continue;
    }
    int j = i;
    while (j > 0) {
      const unsigned pc = hb_unicode_combining_class(text_uf(), buf[j - 1]);
      if (pc == 0 || pc <= cc) {
        break;
      }
      buf[j] = buf[j - 1];
      j--;
    }
    buf[j] = c;
  }
  int bytes = 0;
  for (int i = 0; i < n; i++) {
    char o[4];
    const int k = text_utf8_put(buf[i], o);
    if (bytes + k > cap) {
      return -1;
    }
    memcpy(out + bytes, o, (size_t)k);
    bytes += k;
  }
  return bytes;
}

/* Code points and their byte offsets (off[n] = len); NULL when out of memory. */
static uint32_t *text_decode(const char *utf8, int len, int **r_off, int *r_n)
{
  uint32_t *cp = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)(len > 0 ? len : 1));
  int *off = (int *)malloc(sizeof(int) * (size_t)(len + 1));
  if (!cp || !off) {
    free(cp);
    free(off);
    return NULL;
  }
  int n = 0;
  for (int i = 0; i < len; n++) {
    off[n] = i;
    cp[n] = (uint32_t)utf8_next(utf8, len, &i);
  }
  off[n] = len;
  *r_off = off;
  *r_n = n;
  return cp;
}

int BUI_text_direction(const char *utf8, int len)
{
  for (int i = 0; i < len;) {
    const uint32_t c = (uint32_t)utf8_next(utf8, len, &i);
    const int d = text_strong_dir(&c, 1);
    if (d) {
      return d;
    }
  }
  return 0;
}

int BUI_text_bidi(const char *utf8, int len, int base, uint8_t *levels)
{
  int rtl = base == 2 || base == 3;
  if (base == 0 || base == 3) {
    const int d = BUI_text_direction(utf8, len);
    if (d) {
      rtl = d == 2;
    }
  }
  if (len <= 0) {
    return rtl;
  }
  int *off = NULL, n = 0;
  uint32_t *cp = text_decode(utf8, len, &off, &n);
  uint8_t *lv = cp ? (uint8_t *)malloc((size_t)n + 1) : NULL;
  if (!lv) {
    memset(levels, rtl, (size_t)len);
  }
  else {
    text_levels(cp, n, rtl, lv);
    for (int k = 0; k < n; k++) {
      memset(levels + off[k], lv[k], (size_t)(off[k + 1] - off[k]));
    }
  }
  free(lv);
  free(cp);
  free(off);
  return rtl;
}

/* The logical attributes of `utf8` per byte (attrs[0..len]: the
 * boundary before each character; BUI_TEXT_WHITE and
 * BUI_TEXT_EXPANDABLE_SPACE at the character's first byte), or with
 * `cursors` only BUI_TEXT_CURSOR and BUI_TEXT_BACKSPACE_CHAR in a byte per
 * offset. */
static void text_breaks(const char *utf8, int len, uint16_t *attrs, uint8_t *cursors, int slot)
{
  if (attrs) {
    memset(attrs, 0, sizeof(uint16_t) * ((size_t)len + 1));
  }
  else {
    memset(cursors, 0, (size_t)len + 1);
  }
  int *off = NULL, n = 0;
  uint32_t *cp = len > 0 ? text_decode(utf8, len, &off, &n) : NULL;
  BuiLogAttr la[65];
  BuiLogAttr *a = n + 1 <= 65 ? la : (BuiLogAttr *)malloc(sizeof(BuiLogAttr) * ((size_t)n + 1));
  if (len <= 0) {
    off = NULL;
  }
  if (len > 0 && (!cp || !a)) {
    if (attrs) {
      attrs[0] = attrs[len] = BUI_TEXT_CURSOR | BUI_TEXT_LINE_BREAK;
    }
    else {
      cursors[0] = cursors[len] = BUI_TEXT_CURSOR;
    }
  }
  else {
    uint32_t none = 0;
    uint8_t *istart = NULL;
    hb_script_t *iscript = NULL;
    if (slot >= 0 && cp) {
      text_font_items(slot, cp, n, &istart, &iscript);
    }
    text_log_attrs(cp ? cp : &none, n, a, istart, iscript);
    free(istart);
    free(iscript);
    for (int k = 0; k <= n; k++) {
      const BuiLogAttr *t = &a[k];
      const int o = off ? off[k] : 0;
      uint16_t v = (uint16_t)((t->is_cursor_position ? BUI_TEXT_CURSOR : 0) |
                              (t->is_cursor_position && t->backspace_deletes_character ?
                                   BUI_TEXT_BACKSPACE_CHAR :
                                   0));
      if (!attrs) {
        cursors[o] = (uint8_t)v;
        continue;
      }
      v |= (uint16_t)((t->is_line_break ? BUI_TEXT_LINE_BREAK : 0) |
                      (t->is_mandatory_break ? BUI_TEXT_MANDATORY_BREAK : 0) |
                      (t->is_char_break ? BUI_TEXT_CHAR_BREAK : 0) |
                      (t->is_white ? BUI_TEXT_WHITE : 0) |
                      (t->is_word_start ? BUI_TEXT_WORD_START : 0) |
                      (t->is_word_end ? BUI_TEXT_WORD_END : 0) |
                      (t->is_word_boundary ? BUI_TEXT_WORD_BOUNDARY : 0) |
                      (t->break_inserts_hyphen ? BUI_TEXT_INSERTS_HYPHEN : 0) |
                      (t->break_removes_preceding ? BUI_TEXT_REMOVES_PRECEDING : 0) |
                      (t->is_expandable_space ? BUI_TEXT_EXPANDABLE_SPACE : 0));
      attrs[o] = v;
    }
  }
  if (a != la) {
    free(a);
  }
  free(cp);
  free(off);
}

void BUI_text_breaks(const char *utf8, int len, uint16_t *attrs)
{
  text_breaks(utf8, len, attrs, NULL, -1);
}

/** \} */
