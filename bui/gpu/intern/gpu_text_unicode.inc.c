/* BUI-Awra: the Unicode both text engines need, independent of fonts
 * (included by gpu_raster.c):
 *
 *  - scripts (Pango's script iterator) and the languages they imply;
 *  - bidi levels (UAX #9) through FriBidi (extern/fribidi) as Pango
 *    resolves them, and a paragraph's direction from its first strong
 *    character;
 *  - grapheme boundaries (UAX #29) and backspace behaviour as Pango's
 *    break.c computes them (cursor positions, backspace_deletes_character);
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
      hb_script_t sc = hb_unicode_script(text_uf(), ch);
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
/** \name Grapheme boundaries (UAX #29 as Pango's break.c applies it)
 * \{ */


#  if defined(__GNUC__) && __GNUC__ >= 7
#    define TEXT_FALLTHROUGH __attribute__((fallthrough))
#  else
#    define TEXT_FALLTHROUGH ((void)0)
#  endif

enum { GB_OTHER, GB_CONTROL, GB_EXTEND, GB_ZWJ, GB_PREPEND, GB_SPACING, GB_HANGUL, GB_RI_ODD, GB_RI_EVEN };

/* Hangul jamo: L, V, T, LV, LVT (Pango's HangulJamoProps), 5 = none */
static int text_jamo(uint32_t c)
{
  if ((c >= 0x1100 && c <= 0x115F) || (c >= 0xA960 && c <= 0xA97C)) {
    return 0;
  }
  if ((c >= 0x1160 && c <= 0x11A7) || (c >= 0xD7B0 && c <= 0xD7C6)) {
    return 1;
  }
  if ((c >= 0x11A8 && c <= 0x11FF) || (c >= 0xD7CB && c <= 0xD7FB)) {
    return 2;
  }
  if (c >= 0xAC00 && c <= 0xD7A3) {
    return (c - 0xAC00) % 28 == 0 ? 3 : 4;
  }
  return 5;
}

static int text_backspace_char(uint32_t base)
{
  const int latin = (base >= 0x20 && base <= 0x2AF) || (base >= 0x1E00 && base <= 0x1EFF);
  const int cyrillic = base >= 0x400 && base <= 0x52F;
  const int greek = (base >= 0x370 && base <= 0x3FF) || (base >= 0x1F00 && base <= 0x1FFF);
  const int kana = base >= 0x3040 && base <= 0x30FF;
  const int hangul = base >= 0xAC00 && base <= 0xD7A3;
  const int math = base >= 0x2200 && base <= 0x22FF;
  return !latin && !cyrillic && !greek && !kana && !hangul && !EMOJI_IS(emoji, base) && !math;
}

/* attr[i] (i = 0..n): the boundary before character i: BUI_TEXT_CURSOR,
 * BUI_TEXT_BACKSPACE_CHAR. */
static void text_graphemes(const uint32_t *cp, int n, uint8_t *attr)
{
  static const int jamo_start[6] = {0, 1, 2, 0, 0, 5}, jamo_end[6] = {0, 1, 2, 1, 2, 5};
  hb_unicode_funcs_t *uf = text_uf();
  int prev_gb = GB_OTHER, prev_jamo = 5;
  uint32_t prev = 0, base = 0;
  int met_pict = 0, incb_cons = 0, incb_link = 0;
  for (int i = 0; i <= n; i++) {
    const uint32_t c = i < n ? cp[i] : 0x2029; /* the paragraph's end */
    const hb_unicode_general_category_t gc = hb_unicode_general_category(uf, c);
    const int jamo = text_jamo(c);
    const int hangul =
        jamo != 5 && (jamo_end[prev_jamo] == jamo_start[jamo] || jamo_end[prev_jamo] + 1 == jamo_start[jamo]);
    const int pict = EMOJI_IS(extended_pictographic, c);
    int gb = GB_OTHER;
    switch (gc) {
      case HB_UNICODE_GENERAL_CATEGORY_FORMAT:
        if (c == 0x200C) {
          gb = GB_EXTEND;
          break;
        }
        if (c == 0x200D) {
          gb = GB_ZWJ;
          break;
        }
        if ((c >= 0x600 && c <= 0x605) || c == 0x6DD || c == 0x70F || c == 0x8E2 || c == 0x110BD ||
            c == 0x110CD)
        {
          gb = GB_PREPEND;
          break;
        }
        if (c >= 0xE0020 && c <= 0xE00FF) {
          gb = GB_EXTEND;
          break;
        }
        gb = GB_CONTROL;
        break;
      case HB_UNICODE_GENERAL_CATEGORY_CONTROL:
      case HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR:
      case HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR:
      case HB_UNICODE_GENERAL_CATEGORY_SURROGATE:
        gb = GB_CONTROL;
        break;
      case HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED:
        if ((c >= 0xFFF0 && c <= 0xFFF8) || (c >= 0xE0000 && c <= 0xE0FFF)) {
          gb = GB_CONTROL;
          break;
        }
        TEXT_FALLTHROUGH;
      case HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER:
        if (hangul) {
          gb = GB_HANGUL;
        }
        if (bui_is_Consonant_Preceding_Repha(c) || bui_is_Consonant_Prefixed(c)) {
          gb = GB_PREPEND;
        }
        break;
      case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER:
        if (c >= 0xFF9E && c <= 0xFF9F) {
          gb = GB_EXTEND;
        }
        break;
      case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
        if (!(c == 0x102B || c == 0x102C || c == 0x1038 || (c >= 0x1062 && c <= 0x1064) ||
              (c >= 0x1067 && c <= 0x106D) || c == 0x1083 || (c >= 0x1087 && c <= 0x108C) ||
              c == 0x108F || (c >= 0x109A && c <= 0x109C) || c == 0x1A61 || c == 0x1A63 ||
              c == 0x1A64 || c == 0xAA7B || c == 0xAA7D || c == 0x11720 || c == 0x11721))
        {
          gb = GB_SPACING;
        }
        if (c >= 0x0900 &&
            (c == 0x09BE || c == 0x09D7 || c == 0x0B3E || c == 0x0B57 || c == 0x0BBE || c == 0x0BD7 ||
             c == 0x0CC2 || c == 0x0CD5 || c == 0x0CD6 || c == 0x0D3E || c == 0x0D57 || c == 0x0DCF ||
             c == 0x0DDF || c == 0x1D165 || (c >= 0x1D16E && c <= 0x1D172)))
        {
          gb = GB_EXTEND;
        }
        break;
      case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
      case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
        gb = GB_EXTEND;
        break;
      case HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL:
        if (c >= 0x1F1E6 && c <= 0x1F1FF) {
          gb = prev_gb == GB_RI_ODD ? GB_RI_EVEN : GB_RI_ODD;
        }
        break;
      case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL:
        if (c >= 0x1F3FB && c <= 0x1F3FF) {
          gb = GB_EXTEND;
        }
        break;
      default:
        break;
    }
    /* GB11 */
    if (met_pict) {
      met_pict = gb == GB_EXTEND || (EMOJI_IS(extended_pictographic, prev) && gb == GB_ZWJ) ||
                 (prev_gb == GB_EXTEND && gb == GB_ZWJ) || (prev_gb == GB_ZWJ && pict);
    }
    int boundary = 1; /* GB999 */
    if (prev_gb == GB_RI_ODD && gb == GB_RI_EVEN) {
      boundary = 0; /* GB12, GB13 */
    }
    if (pict && prev_gb == GB_ZWJ && met_pict) {
      boundary = 0; /* GB11 */
    }
    if (incb_cons || incb_link) { /* GB9c */
      if (incb_link && bui_is_Indic_Conjunct_Break_Consonant(c)) {
        incb_cons = 1;
        incb_link = 0;
        boundary = 0;
      }
      if (!(bui_is_Indic_Conjunct_Break_Extend(c) || bui_is_Indic_Conjunct_Break_Linker(c))) {
        incb_cons = incb_link = 0;
      }
    }
    if (prev_gb == GB_PREPEND) {
      boundary = 0; /* GB9b */
    }
    if (gb == GB_SPACING) {
      boundary = 0; /* GB9a */
    }
    if (gb == GB_EXTEND || gb == GB_ZWJ) {
      boundary = 0; /* GB9 */
    }
    if (gb == GB_HANGUL) {
      boundary = 0; /* GB6-8 */
    }
    if (prev_gb == GB_CONTROL || gb == GB_CONTROL) {
      boundary = 1; /* GB4, GB5 */
    }
    if (c == '\n' && prev == '\r') {
      boundary = 0; /* GB3 */
    }
    if (pict) {
      met_pict = 1;
    }
    if (incb_cons && !incb_link && bui_is_Indic_Conjunct_Break_Linker(c)) {
      incb_link = 1;
    }
    if (!incb_cons && !incb_link && bui_is_Indic_Conjunct_Break_Consonant(c)) {
      incb_cons = 1;
    }
    if (i == 0 || i == n) {
      boundary = 1; /* GB1, GB2 */
    }
    uint8_t a = 0;
    if (boundary) {
      a = BUI_TEXT_CURSOR;
      if (text_backspace_char(base) || bui_is_Virama(prev) || bui_is_Vowel_Dependent(prev)) {
        a |= BUI_TEXT_BACKSPACE_CHAR;
      }
    }
    attr[i] = a;
    prev_gb = gb;
    prev_jamo = jamo;
    prev = c;
    if (gc != HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK &&
        gc != HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK &&
        gc != HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
      base = c;
    }
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

void BUI_text_breaks(const char *utf8, int len, uint8_t *attrs)
{
  memset(attrs, 0, (size_t)len + 1);
  int *off = NULL, n = 0;
  uint32_t *cp = len > 0 ? text_decode(utf8, len, &off, &n) : NULL;
  uint8_t *a = cp ? (uint8_t *)malloc((size_t)n + 1) : NULL;
  if (!a) {
    attrs[0] = attrs[len] = BUI_TEXT_CURSOR;
  }
  else {
    text_graphemes(cp, n, a);
    for (int k = 0; k <= n; k++) {
      attrs[off[k]] = a[k];
    }
  }
  free(a);
  free(cp);
  free(off);
}

/** \} */
