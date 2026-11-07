/* BUI-Awra: what Pango, fontconfig, HarfBuzz and FriBidi did for Awra 1's
 * text, for the portable text engine (included by gpu_raster.c):
 *
 *  - bidi (UAX #9) with FriBidi (extern/fribidi), as Pango resolves it: a
 *    run's paragraph direction from its first strong character, or one
 *    direction the caller gives (a run of one level of a paragraph laid out
 *    by the caller); runs reordered visually (L2);
 *  - script runs (Pango's script iterator, paired punctuation included)
 *    and the language each script implies when the user's language doesn't
 *    write it (Pango's derived language: fonts are sorted for it);
 *  - emoji itemization: Pango's emoji presentation scanner (Chromium's,
 *    extern/emoji) splits text into emoji and text segments; an emoji
 *    segment is drawn with the "emoji" family (a colour font) as Pango asks
 *    fontconfig for it;
 *  - per-character font fallback: a character the font lacks is drawn with
 *    the first font of fontconfig's sorted list (FcFontSort, by charset and
 *    language) that has it, on Windows with the system's font links
 *    (SystemLink) and Segoe UI Emoji / Symbol; spaces and default
 *    ignorables take the font of the run they are in, as Pango itemizes;
 *  - shaping with HarfBuzz (extern/harfbuzz), item by item with the run's
 *    text as context, as pango_hb_shape does: every script's shaper (Arabic
 *    joining and lam-alef, marks, Indic, Thai, Hangul...), kerning, emoji
 *    sequences, mirrored glyphs in right-to-left runs, default ignorables
 *    hidden, missing space characters given HarfBuzz's fallback widths;
 *    advances rounded to whole pixels with hinted metrics as Pango rounds
 *    them;
 *  - grapheme boundaries (UAX #29, Pango's rules) for caret positions,
 *    hit-testing and backspace.
 *
 * Faces and font slots are cached; glyph bitmaps go through the shared
 * glyph cache like any other glyph.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */


/* -------------------------------------------------------------------- */
/** \name Unicode properties
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

/* Pango's consider_as_space(): characters that never choose a font
 * (controls, format characters but Arabic's, separators, variation
 * selectors). */
static int text_space_like(uint32_t c)
{
  if (c < 0x20 || (c >= 0x7F && c <= 0x9F)) {
    return 1; /* Cc */
  }
  if (c < 0xA0) {
    return c == ' ';
  }
  switch (c) {
    case 0xA0:
    case 0xAD:
    case 0x61C:
    case 0x180E:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
    case 0xFEFF:
      return 1;
    default:
      break;
  }
  return (c >= 0x2000 && c <= 0x200F) || (c >= 0x202A && c <= 0x202E) ||
         (c >= 0x2060 && c <= 0x206F) || (c >= 0xFE00 && c <= 0xFE0F) ||
         (c >= 0xFFF9 && c <= 0xFFFB) || c == 0x110BD || c == 0x110CD ||
         (c >= 0x13430 && c <= 0x1343F) || (c >= 0x1BCA0 && c <= 0x1BCA3) ||
         (c >= 0x1D173 && c <= 0x1D17A) || c == 0xE0001 || (c >= 0xE0020 && c <= 0xE007F) ||
         (c >= 0xE0100 && c <= 0xE01EF);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Fallback font lists
 * \{ */

/* A sorted list of fonts to fall back through: fontconfig's sort of a
 * pattern on Linux, files (the system's font links) elsewhere. */
typedef struct FbCand {
  int face;      /* -2: not loaded yet, -1: can't load */
  int8_t color;  /* -1: unknown */
  int index;     /* face in a collection (file lists) */
  char *path;    /* file lists */
  char *family;  /* file lists: the family to find in a collection */
} FbCand;

typedef struct FbList {
  char family[64];
  char lang[16]; /* the language fonts are sorted for ("": the user's) */
  int weight, italic;
  int n;
  FbCand *c;
  void *set; /* fontconfig: the FcFontSet the list reads charsets from */
} FbList;

#  define MAX_FB_LISTS 16 /* fb_find's memo keys hold 4 bits of list */
static FbList g_fb[MAX_FB_LISTS];
static int g_fb_count = 0;

#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
typedef struct {
  int nfont, sfont;
  FcPattern_ **fonts;
} FcFontSet_;

static struct {
  int state;
  FcFontSet_ *(*FontSort)(void *, FcPattern_ *, int, void **, int *);
  int (*PatternGetCharSet)(const FcPattern_ *, const char *, int, void **);
  int (*CharSetHasChar)(const void *, uint32_t);
  int (*PatternGetBool)(const FcPattern_ *, const char *, int, int *);
} g_fcs;

static int fcs_load(void)
{
  if (g_fcs.state) {
    return g_fcs.state > 0;
  }
  g_fcs.state = -1;
  if (!fc_load()) {
    return 0;
  }
  void *lib = dlopen("libfontconfig.so.1", RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
  if (!lib) {
    return 0;
  }
  *(void **)&g_fcs.FontSort = dlsym(lib, "FcFontSort");
  *(void **)&g_fcs.PatternGetCharSet = dlsym(lib, "FcPatternGetCharSet");
  *(void **)&g_fcs.CharSetHasChar = dlsym(lib, "FcCharSetHasChar");
  *(void **)&g_fcs.PatternGetBool = dlsym(lib, "FcPatternGetBool");
  /* the library stays loaded (fc_load's handle); this handle is a second
   * reference to it, kept for the process's life like the first */
  if (!g_fcs.FontSort || !g_fcs.PatternGetCharSet || !g_fcs.CharSetHasChar ||
      !g_fcs.PatternGetBool)
  {
    return 0;
  }
  g_fcs.state = 1;
  return 1;
}

static int fb_build_fc(FbList *l)
{
  if (!fcs_load()) {
    return 0;
  }
  FcPattern_ *pat = g_fc.PatternCreate();
  if (!pat) {
    return 0;
  }
  g_fc.PatternAddString(pat, "family", (const FcChar8_ *)(l->family[0] ? l->family : "sans-serif"));
  g_fc.PatternAddInteger(pat, "weight", g_fc.WeightFromOpenType(l->weight));
  g_fc.PatternAddInteger(pat, "slant", l->italic ? 100 : 0);
  if (l->lang[0]) {
    g_fc.PatternAddString(pat, "lang", (const FcChar8_ *)l->lang);
  }
  g_fc.ConfigSubstitute(NULL, pat, 0 /* FcMatchPattern */);
  g_fc.DefaultSubstitute(pat);
  int result = 0;
  FcFontSet_ *set = g_fcs.FontSort(NULL, pat, 1 /* trim */, NULL, &result);
  g_fc.PatternDestroy(pat);
  if (!set) {
    return 0;
  }
  l->set = set;
  l->n = set->nfont;
  l->c = (FbCand *)calloc((size_t)(l->n > 0 ? l->n : 1), sizeof(FbCand));
  if (!l->c) {
    l->n = 0;
    return 0;
  }
  for (int i = 0; i < l->n; i++) {
    l->c[i].face = -2;
    int b = 0;
    l->c[i].color = (int8_t)(g_fcs.PatternGetBool(set->fonts[i], "color", 0, &b) == 0 && b);
  }
  return 1;
}

static int fb_fc_covers(const FbList *l, int k, uint32_t cp)
{
  void *cs = NULL;
  const FcFontSet_ *set = (const FcFontSet_ *)l->set;
  return g_fcs.PatternGetCharSet(set->fonts[k], "charset", 0, &cs) == 0 && cs &&
         g_fcs.CharSetHasChar(cs, cp);
}

static int fb_fc_face(const FbList *l, int k)
{
  const FcFontSet_ *set = (const FcFontSet_ *)l->set;
  FcChar8_ *file = NULL;
  int index = 0;
  if (g_fc.PatternGetString(set->fonts[k], "file", 0, &file) != 0 || !file) {
    return -1;
  }
  g_fc.PatternGetInteger(set->fonts[k], "index", 0, &index);
  return face_load_file_index((const char *)file, index & 0xFFFF);
}
#  endif

static void fb_add_file(FbList *l, int *cap, const char *path, const char *family)
{
  for (int i = 0; i < l->n; i++) {
    if (ieq(l->c[i].path, path)) {
      return;
    }
  }
  if (l->n == *cap) {
    const int nc = *cap ? *cap * 2 : 16;
    FbCand *c = (FbCand *)realloc(l->c, sizeof(FbCand) * (size_t)nc);
    if (!c) {
      return;
    }
    l->c = c;
    *cap = nc;
  }
  FbCand *c = &l->c[l->n];
  memset(c, 0, sizeof(*c));
  c->face = -2;
  c->color = -1;
  c->path = (char *)malloc(strlen(path) + 1);
  if (!c->path) {
    return;
  }
  strcpy(c->path, path);
  if (family && family[0]) {
    c->family = (char *)malloc(strlen(family) + 1);
    if (c->family) {
      strcpy(c->family, family);
    }
  }
  l->n++;
}

/* `names`: '|'-separated file names, looked for in the font folders. */
static void fb_add_files(FbList *l, int *cap, const char *names)
{
  char dirs[16][260];
  const int nd = font_dirs(dirs, 16);
  for (const char *p = names; *p;) {
    const char *end = strchr(p, '|');
    const int n = end ? (int)(end - p) : (int)strlen(p);
    for (int d = 0; d < nd; d++) {
      char path[600];
      const size_t dl = strlen(dirs[d]);
      if (dl + 1 + (size_t)n >= sizeof(path)) {
        continue; /* never a truncated path */
      }
      memcpy(path, dirs[d], dl);
      path[dl] = '/';
      memcpy(path + dl + 1, p, (size_t)n);
      path[dl + 1 + (size_t)n] = 0;
#  ifdef _WIN32
      const DWORD attr = GetFileAttributesA(path);
      const int exists = attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#  else
      const int exists = access(path, R_OK) == 0;
#  endif
      if (exists) {
        fb_add_file(l, cap, path, NULL);
        break;
      }
    }
    p += n + (end ? 1 : 0);
  }
}

#  ifdef _WIN32
/* The fonts Windows links to `family` for the characters it lacks
 * (HKLM\...\FontLink\SystemLink: "FILE.TTF,Family[,scale,scale]"). */
static void fb_add_systemlink(FbList *l, int *cap, const char *family)
{
  HKEY key;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink", 0,
                    KEY_READ, &key) != ERROR_SUCCESS)
  {
    return;
  }
  wchar_t name[128];
  MultiByteToWideChar(CP_UTF8, 0, family, -1, name, 127);
  name[127] = 0;
  DWORD type = 0, size = 0;
  if (RegQueryValueExW(key, name, NULL, &type, NULL, &size) == ERROR_SUCCESS &&
      type == REG_MULTI_SZ && size > 0 && size < (1u << 20))
  {
    wchar_t *buf = (wchar_t *)calloc(size / sizeof(wchar_t) + 2, sizeof(wchar_t));
    if (buf && RegQueryValueExW(key, name, NULL, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
      const char *win = getenv("WINDIR");
      for (const wchar_t *e = buf; *e; e += wcslen(e) + 1) {
        char entry[600];
        WideCharToMultiByte(CP_UTF8, 0, e, -1, entry, (int)sizeof(entry), NULL, NULL);
        entry[sizeof(entry) - 1] = 0;
        char *comma = strchr(entry, ',');
        char *fam = NULL;
        if (comma) {
          *comma = 0;
          fam = comma + 1;
          char *c2 = strchr(fam, ',');
          if (c2) {
            *c2 = 0;
          }
        }
        char path[700];
        if (strchr(entry, ':') || entry[0] == '\\') {
          snprintf(path, sizeof(path), "%s", entry);
        }
        else {
          snprintf(path, sizeof(path), "%s/Fonts/%s", win ? win : "C:/Windows", entry);
        }
        const DWORD attr = GetFileAttributesA(path);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
          fb_add_file(l, cap, path, fam);
        }
      }
    }
    free(buf);
  }
  RegCloseKey(key);
}
#  endif

static int fb_build_files(FbList *l, int emoji)
{
  int cap = 0;
  if (emoji) {
    fb_add_files(l, &cap, "seguiemj.ttf|NotoColorEmoji.ttf|Apple Color Emoji.ttc|seguisym.ttf");
    return l->n > 0;
  }
#  ifdef _WIN32
  fb_add_systemlink(l, &cap, l->family[0] ? l->family : "Segoe UI");
  if (l->family[0] && !ieq(l->family, "Segoe UI")) {
    fb_add_systemlink(l, &cap, "Segoe UI");
  }
  fb_add_files(l, &cap,
               "seguisym.ttf|seguiemj.ttf|seguihis.ttf|Nirmala.ttf|NirmalaS.ttf|LeelawUI.ttf|"
               "ebrima.ttf|gadugi.ttf|msyi.ttf|himalaya.ttf|mmrtext.ttf|mongolianbaiti.ttf|"
               "taile.ttf|ntailu.ttf|phagspa.ttf|msyh.ttc|YuGothM.ttc|malgun.ttf|tahoma.ttf|"
               "arial.ttf|times.ttf|cambria.ttc|seguibl.ttf");
#  else
  fb_add_files(l, &cap,
               "DejaVuSans.ttf|NotoSans-Regular.ttf|NotoSansSymbols-Regular.ttf|"
               "NotoSansSymbols2-Regular.ttf|NotoSansCJK-Regular.ttc|FreeSerif.ttf|"
               "NotoColorEmoji.ttf|unifont.otf");
#  endif
  return l->n > 0;
}

static FbList *fb_list(const char *family, int weight, int italic, const char *lang)
{
#  if defined(_WIN32) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
  lang = ""; /* the file lists don't depend on it */
#  endif
  for (int i = 0; i < g_fb_count; i++) {
    FbList *l = &g_fb[i];
    if (l->weight == weight && l->italic == italic && strcmp(l->family, family) == 0 &&
        strcmp(l->lang, lang) == 0)
    {
      return l;
    }
  }
  if (g_fb_count == MAX_FB_LISTS) {
    return NULL;
  }
  FbList *l = &g_fb[g_fb_count++];
  memset(l, 0, sizeof(*l));
  snprintf(l->family, sizeof(l->family), "%s", family);
  snprintf(l->lang, sizeof(l->lang), "%s", lang);
  l->weight = weight;
  l->italic = italic;
  const int emoji = strcmp(family, "emoji") == 0;
#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  if (!getenv("BUI_FONT") && fb_build_fc(l)) {
    return l;
  }
#  endif
  fb_build_files(l, emoji);
  return l;
}

/* The face of candidate k, loaded on first use. */
static int fb_face(FbList *l, int k)
{
  FbCand *c = &l->c[k];
  if (c->face != -2) {
    return c->face;
  }
  c->face = -1;
#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  if (l->set) {
    c->face = fb_fc_face(l, k);
    return c->face;
  }
#  endif
  if (!c->path) {
    return -1;
  }
  int index = 0;
  const size_t pl = strlen(c->path);
  if (c->family && pl > 4 && ieq(c->path + pl - 4, ".ttc")) {
    /* the collection's face of that family */
    long len = 0;
    int mapped = 0;
    unsigned char *data = font_map(c->path, &len, &mapped);
    if (data) {
      const int nf = stbtt_GetNumberOfFonts(data);
      for (int i = 0; i < nf && i < 64; i++) {
        stbtt_fontinfo info;
        const int off = stbtt_GetFontOffsetForIndex(data, i);
        int nl = 0;
        const char *nm = off >= 0 && stbtt_InitFont(&info, data, off) ?
                             stbtt_GetFontNameString(&info, &nl, STBTT_PLATFORM_ID_MICROSOFT,
                                                     STBTT_MS_EID_UNICODE_BMP,
                                                     STBTT_MS_LANG_ENGLISH, 1) :
                             NULL;
        if (nm && nl > 0) {
          /* the name is UTF-16BE: compare its ASCII */
          char ascii[128];
          int k2 = 0;
          for (int j = 0; j + 1 < nl && k2 < 127; j += 2) {
            ascii[k2++] = nm[j] ? '?' : nm[j + 1];
          }
          ascii[k2] = 0;
          if (ieq(ascii, c->family)) {
            index = i;
            break;
          }
        }
      }
      font_unmap(data, len, mapped);
    }
  }
  c->face = face_load_file_index(c->path, index);
  return c->face;
}

static int fb_color(FbList *l, int k)
{
  FbCand *c = &l->c[k];
  if (c->color < 0) {
    const int f = fb_face(l, k);
    c->color = (int8_t)(f >= 0 && g_faces[f].color);
  }
  return c->color;
}

static int face_has(int face, uint32_t cp);

static int fb_covers(FbList *l, int k, uint32_t cp)
{
#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  if (l->set) {
    return fb_fc_covers(l, k, cp);
  }
#  endif
  const int f = fb_face(l, k);
  return f >= 0 && face_has(f, cp);
}

/* Remembered answers of fb_find: (list, cp, colour) -> candidate. */
typedef struct FbMemo {
  uint32_t key;
  int16_t cand;
} FbMemo;
#  define FB_MEMO 1024 /* power of two */
static FbMemo g_fb_memo[FB_MEMO];

/* The first candidate of `l` that has `cp` (a colour font when `color`),
 * -1 when none. */
static int fb_find(FbList *l, uint32_t cp, int color)
{
  const uint32_t key = (cp | ((uint32_t)(l - g_fb) << 21) | ((uint32_t)(color != 0) << 25)) |
                       0x80000000u;
  FbMemo *m = &g_fb_memo[((key * 2654435761u) >> 16) & (FB_MEMO - 1)];
  if (m->key == key) {
    return m->cand;
  }
  int found = -1;
  for (int k = 0; k < l->n && found < 0; k++) {
    if (color && !fb_color(l, k)) {
      continue;
    }
    if (fb_covers(l, k, cp) && fb_face(l, k) >= 0) {
      found = k;
    }
  }
  m->key = key;
  m->cand = (int16_t)found;
  return found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name HarfBuzz faces and fonts
 *
 * One hb_face per face (over the face's own bytes, no copy) and one
 * hb_font per slot, set up as Pango sets up its fonts
 * (pango_fc_font_create_hb_font): scale in Pango units (1/1024 px), the
 * slot's variation coordinates, the synthetic slant.
 * \{ */

static hb_face_t *face_hb(Face *f)
{
  if (!f->hb && f->data) {
    hb_blob_t *blob = hb_blob_create((const char *)f->data, (unsigned int)f->len,
                                     HB_MEMORY_MODE_READONLY, NULL, NULL);
    f->hb = hb_face_create(blob, (unsigned int)f->index);
    hb_blob_destroy(blob);
  }
  return (hb_face_t *)f->hb;
}

static hb_font_t *slot_hb(FontSlot *s)
{
  if (s->hb) {
    return (hb_font_t *)s->hb;
  }
  Face *f = &g_faces[s->face];
  hb_face_t *face = face_hb(f);
  if (!face) {
    return NULL;
  }
  hb_font_t *font = hb_font_create(face);
  /* the size in whole Pango units (1/1024 px), as Pango rounds it (get_scaled_size) */
  const int scale = (int)(s->size * 1024.0f + 0.5f);
  hb_font_set_scale(font, scale, scale);
  hb_font_set_ptem(font, (float)s->px64 / 64.0f * 0.75f);
  if (s->synth & BUI_FONT_ITALIC) {
    hb_font_set_synthetic_slant(font, 0.2f);
  }
#  if TEXT_FT
  if (f->mm) {
    hb_variation_t v[2];
    unsigned int nv = 0;
    if (f->ax_wght >= 0) {
      v[nv].tag = HB_TAG('w', 'g', 'h', 't');
      v[nv++].value = s->wght > 0.0f ? s->wght : f->wght;
    }
    if (f->ax_opsz >= 0) {
      v[nv].tag = HB_TAG('o', 'p', 's', 'z');
      v[nv++].value = s->opsz;
    }
    if (nv) {
      hb_font_set_variations(font, v, nv);
    }
  }
#  endif
  s->hb = font;
  return font;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Itemization and shaping
 *
 * A run's characters get a bidi level, a script, an emoji segment and a
 * font; items are the stretches where none of them changes, shaped one by
 * one by HarfBuzz and laid out in visual order.
 * \{ */

static int face_glyph_lookup(const Face *f, uint32_t cp)
{
#  if TEXT_FT
  if (f->ft_only) {
    return (int)FT_Get_Char_Index(f->ft, (FT_ULong)cp);
  }
#  endif
  return stbtt_FindGlyphIndex(&f->info, (int)cp);
}

static int face_glyph(int face, uint32_t cp)
{
  Face *f = &g_faces[face];
  if (cp < 128) {
    /* ASCII, the common case: a table per face (256 bytes, for its life) */
    if (!f->ascii) {
      f->ascii = (uint16_t *)malloc(sizeof(uint16_t) * 128);
      if (!f->ascii) {
        return face_glyph_lookup(f, cp);
      }
      for (uint32_t c = 0; c < 128; c++) {
        f->ascii[c] = (uint16_t)face_glyph_lookup(f, c);
      }
    }
    return f->ascii[cp];
  }
  return face_glyph_lookup(f, cp);
}

static int face_has(int face, uint32_t cp)
{
  return face_glyph(face, cp) != 0;
}

/* One glyph of a run, in visual order. */
typedef struct WGlyph {
  uint16_t glyph;
  uint8_t slot;
  uint8_t flags; /* WG_RTL: from a right-to-left item; WG_HIDE: not drawn */
  float x;       /* pen position (sum of the advances before it), px */
  float adv;     /* advance, px (whole pixels with hinted metrics) */
  float dx, dy;  /* offset from the pen (dy down), px */
  int cluster;   /* byte offset of the character (cluster) it shows */
  int item;      /* the item it belongs to */
} WGlyph;
#  define WG_RTL 1
#  define WG_HIDE 2

/* An item: a stretch of one level, script, segment and font. */
typedef struct WItem {
  int start, end; /* byte range */
  uint8_t level;
  uint8_t slot;
} WItem;

typedef struct WRun {
  WGlyph *g;
  int n, cap;
  WItem *it;
  int nit, capit;
  float width;
  WGlyph local[48];
  WItem ilocal[8];
} WRun;

static void wrun_init(WRun *w)
{
  w->g = w->local;
  w->cap = (int)(sizeof(w->local) / sizeof(w->local[0]));
  w->n = 0;
  w->it = w->ilocal;
  w->capit = (int)(sizeof(w->ilocal) / sizeof(w->ilocal[0]));
  w->nit = 0;
  w->width = 0.0f;
}

static WGlyph *wrun_push(WRun *w)
{
  if (w->n == w->cap) {
    const int cap = w->cap * 2;
    WGlyph *g = (WGlyph *)malloc(sizeof(WGlyph) * (size_t)cap);
    if (!g) {
      return NULL;
    }
    memcpy(g, w->g, sizeof(WGlyph) * (size_t)w->n);
    if (w->g != w->local) {
      free(w->g);
    }
    w->g = g;
    w->cap = cap;
  }
  return &w->g[w->n++];
}

static WItem *wrun_item(WRun *w)
{
  if (w->nit == w->capit) {
    const int cap = w->capit * 2;
    WItem *it = (WItem *)malloc(sizeof(WItem) * (size_t)cap);
    if (!it) {
      return NULL;
    }
    memcpy(it, w->it, sizeof(WItem) * (size_t)w->nit);
    if (w->it != w->ilocal) {
      free(w->it);
    }
    w->it = it;
    w->capit = cap;
  }
  return &w->it[w->nit++];
}

static void wrun_free(WRun *w)
{
  if (w->g != w->local) {
    free(w->g);
  }
  if (w->it != w->ilocal) {
    free(w->it);
  }
}

static hb_buffer_t *g_hb_buf = NULL;

#  define TEXT_INVISIBLE 0x10FFFFu /* the glyph HarfBuzz gives default ignorables */

/* Shapes characters [a, b) of `cp` (the run's text, the context) in slot
 * `slot` at bidi level `level`, script `script`, language `lang`, and
 * appends their glyphs (visual order) at pen `*pen`. */
static void text_shape_item(WRun *w, int slot, const uint32_t *cp, const int *off, int n, int a, int b,
                            int level, hb_script_t script, const char *lang, float *pen)
{
  FontSlot *s = &g_fonts[slot];
  hb_font_t *font = slot_hb(s);
  if (!font) {
    return;
  }
  if (!g_hb_buf) {
    g_hb_buf = hb_buffer_create();
  }
  hb_buffer_t *buf = g_hb_buf;
  hb_buffer_clear_contents(buf);
  hb_buffer_set_direction(buf, (level & 1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  hb_buffer_set_script(buf, script);
  hb_buffer_set_language(buf, hb_language_from_string(lang, -1));
  hb_buffer_set_cluster_level(buf, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
  hb_buffer_set_flags(buf, (hb_buffer_flags_t)(HB_BUFFER_FLAG_BOT | HB_BUFFER_FLAG_EOT));
  hb_buffer_set_invisible_glyph(buf, TEXT_INVISIBLE);
  hb_buffer_add_codepoints(buf, cp, n, (unsigned int)a, b - a);
  hb_shape(font, buf, NULL, 0);
  unsigned int ng = 0;
  const hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, &ng);
  const hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, NULL);
  const float k = 1.0f / 1024.0f;
  const int item = w->nit - 1;
  for (unsigned int i = 0; i < ng; i++) {
    WGlyph *g = wrun_push(w);
    if (!g) {
      return;
    }
    float adv = (float)pos[i].x_advance * k;
    if (adv != 0.0f) {
      adv += s->embolden;
    }
    if (g_hint_metrics) {
      adv = floorf(adv + 0.5f); /* PANGO_UNITS_ROUND of the width */
    }
    g->glyph = (uint16_t)(info[i].codepoint == TEXT_INVISIBLE ? 0 : info[i].codepoint);
    g->slot = (uint8_t)slot;
    g->flags = (uint8_t)(((level & 1) ? WG_RTL : 0) | (info[i].codepoint == TEXT_INVISIBLE ? WG_HIDE : 0));
    g->x = *pen;
    g->adv = adv;
    g->dx = (float)pos[i].x_offset * k;
    g->dy = -(float)pos[i].y_offset * k;
    g->cluster = off[info[i].cluster];
    g->item = item;
    *pen += adv;
  }
}

/* Visual order of items `it` (bidi rule L2 over their levels). */
static void text_reorder(const WItem *it, int n, int *order)
{
  int maxl = 0, minodd = 255;
  for (int i = 0; i < n; i++) {
    order[i] = i;
    maxl = it[i].level > maxl ? it[i].level : maxl;
    if ((it[i].level & 1) && it[i].level < minodd) {
      minodd = it[i].level;
    }
  }
  for (int l = maxl; l >= minodd && l > 0; l--) {
    for (int i = 0; i < n;) {
      if (it[order[i]].level < l) {
        i++;
        continue;
      }
      int j = i;
      while (j < n && it[order[j]].level >= l) {
        j++;
      }
      for (int p = i, q = j - 1; p < q; p++, q--) {
        const int t = order[p];
        order[p] = order[q];
        order[q] = t;
      }
      i = j;
    }
  }
}

/* The glyphs of `utf8` in slot `slot` and its fallbacks, in visual order.
 * `dir`: 0 a paragraph of its own (direction from its first strong
 * character, left-to-right when none), 1 one left-to-right level, 2 one
 * right-to-left level. */
static void text_itemize(int slot, const char *utf8, int len, int dir, WRun *w)
{
  wrun_init(w);
  if (len <= 0) {
    return;
  }
  const FontSlot *s = &g_fonts[slot];
  const int face0 = s->face;
  /* the common case: printable ASCII the font has, in a left-to-right run
   * or paragraph: one item (Pango's itemizer finds nothing to split it:
   * level 0, the Latin script, or Common without letters, no emoji, one
   * font) */
  if (dir != 2 && len <= 4096) {
    int letters = 0, i = 0;
    for (; i < len; i++) {
      const unsigned char c = (unsigned char)utf8[i];
      if (c < 0x20 || c >= 0x7F || !face_glyph(face0, c)) {
        break;
      }
      letters |= (c | 0x20) >= 'a' && (c | 0x20) <= 'z';
    }
    if (i == len) {
      uint32_t cps[256];
      int offs[257];
      uint32_t *cp = len <= 256 ? cps : (uint32_t *)malloc(sizeof(uint32_t) * (size_t)len);
      int *off = len <= 256 ? offs : (int *)malloc(sizeof(int) * (size_t)(len + 1));
      WItem *it = cp && off ? wrun_item(w) : NULL;
      if (it) {
        for (int k = 0; k < len; k++) {
          cp[k] = (unsigned char)utf8[k];
          off[k] = k;
        }
        off[len] = len;
        it->start = 0;
        it->end = len;
        it->level = 0;
        it->slot = (uint8_t)slot;
        const hb_script_t script = letters ? HB_SCRIPT_LATIN : HB_SCRIPT_COMMON;
        float pen = 0.0f;
        text_shape_item(w, slot, cp, off, len, 0, len, 0, script, text_script_lang(script), &pen);
        w->width = pen;
      }
      if (cp != cps) {
        free(cp);
      }
      if (off != offs) {
        free(off);
      }
      if (it) {
        return;
      }
      wrun_free(w);
      wrun_init(w);
    }
  }
  /* characters: code points, their byte offsets, levels, scripts, emoji
   * segments and fonts */
  int n = 0;
  for (int k = 0; k < len; n++) {
    utf8_next(utf8, len, &k);
  }
  typedef struct Ch {
    int16_t slot; /* -1: a space (takes its run's font) */
    uint8_t seg;  /* 1: emoji segment, 2: with a variation selector */
    uint8_t level;
    int pos;      /* the font's position in its list (Pango's font_position) */
    hb_script_t script;
  } Ch;
  enum { LOCAL = 64 };
  Ch chl[LOCAL];
  uint32_t cpl[LOCAL];
  int offl[LOCAL + 1];
  unsigned char tl[LOCAL];
  Ch *ch = chl;
  uint32_t *cp = cpl;
  int *off = offl;
  unsigned char *types = tl;
  if (n > LOCAL) {
    ch = (Ch *)malloc(sizeof(Ch) * (size_t)n);
    cp = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)n);
    off = (int *)malloc(sizeof(int) * (size_t)(n + 1));
    types = (unsigned char *)malloc((size_t)n);
    if (!ch || !cp || !off || !types) {
      goto out;
    }
  }
  for (int k = 0, j = 0; k < len; j++) {
    off[j] = k;
    cp[j] = (uint32_t)utf8_next(utf8, len, &k);
    if (cp[j] == '\t') {
      cp[j] = ' ';
    }
  }
  off[n] = len;
  /* bidi levels */
  {
    uint8_t lvl[LOCAL];
    uint8_t *lv = n <= LOCAL ? lvl : (uint8_t *)malloc((size_t)n);
    if (!lv) {
      goto out;
    }
    if (dir == 1 || dir == 2) {
      memset(lv, dir == 2 ? 1 : 0, (size_t)n);
    }
    else {
      text_levels(cp, n, text_strong_dir(cp, n) == 2, lv);
    }
    for (int j = 0; j < n; j++) {
      ch[j].level = lv[j];
    }
    if (lv != lvl) {
      free(lv);
    }
  }
  /* scripts */
  {
    hb_script_t scl[LOCAL];
    hb_script_t *sc = n <= LOCAL ? scl : (hb_script_t *)malloc(sizeof(hb_script_t) * (size_t)n);
    if (!sc) {
      goto out;
    }
    text_scripts(cp, n, sc);
    for (int j = 0; j < n; j++) {
      ch[j].script = sc[j];
    }
    if (sc != scl) {
      free(sc);
    }
  }
  /* Pango's emoji iterator */
  for (int j = 0; j < n; j++) {
    types[j] = emoji_category(cp[j]);
  }
  for (int p = 0; p < n;) {
    bool is_emoji = false, has_vs = false;
    int q = (int)(scan_emoji_presentation(types + p, types + n, &is_emoji, &has_vs) - types);
    if (q <= p) {
      q = p + 1;
    }
    for (int k = p; k < q; k++) {
      ch[k].seg = (uint8_t)((is_emoji ? 1 : 0) | (has_vs ? 2 : 0));
    }
    p = q;
  }
  /* each character's font */
  int weight = (s->flags >> BUI_FONT_WEIGHT_SHIFT) & 0x3FF;
  if (weight <= 0) {
    weight = (s->flags & BUI_FONT_BOLD) ? 700 : 400;
  }
  const int italic = (s->flags & BUI_FONT_ITALIC) != 0;
  text_langs_init();
  FbList *text_list = NULL, *emoji_list = NULL;
  int text_tried = 0, emoji_tried = 0;
#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  FbList *lang_list = NULL;
  const char *lang_of_list = NULL;
#  endif
  for (int k = 0; k < n; k++) {
    Ch *c = &ch[k];
    c->slot = -1;
    c->pos = 0xFFFF;
    if (text_space_like(cp[k])) {
      continue;
    }
    if (c->seg & 1) { /* the "emoji" family, a colour font when a VS16 asks */
      if (!emoji_tried) {
        emoji_tried = 1;
        emoji_list = fb_list("emoji", weight, italic, "");
      }
      int cand = -1;
      if (emoji_list) {
        cand = fb_find(emoji_list, cp[k], (c->seg & 2) != 0);
        if (cand < 0 && (c->seg & 2)) {
          cand = fb_find(emoji_list, cp[k], 0);
        }
      }
      const int fs = cand >= 0 ? slot_fallback(slot, fb_face(emoji_list, cand)) : -1;
      if (fs >= 0) {
        c->slot = (int16_t)fs;
        c->pos = cand;
        continue;
      }
    }
#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    /* a script the user's language doesn't write: the fonts fontconfig
     * sorts for the language Pango derives for it */
    const char *lang = text_script_lang(c->script);
    if (lang != g_lang_default) {
      if (lang != lang_of_list) {
        lang_of_list = lang;
        lang_list = fb_list(s->family, weight, italic, lang);
      }
      if (lang_list && lang_list->set) {
        const int cand = fb_find(lang_list, cp[k], 0);
        const int fs = cand >= 0 ? slot_fallback(slot, fb_face(lang_list, cand)) : -1;
        c->slot = (int16_t)(fs >= 0 ? fs : slot);
        c->pos = fs >= 0 ? cand : 0;
        continue;
      }
    }
#  endif
    if (face_has(face0, cp[k])) {
      c->slot = (int16_t)slot;
      c->pos = 0;
      continue;
    }
    if (!text_tried) {
      text_tried = 1;
      text_list = fb_list(s->family, weight, italic, "");
    }
    const int cand = text_list ? fb_find(text_list, cp[k], 0) : -1;
    const int fs = cand >= 0 ? slot_fallback(slot, fb_face(text_list, cand)) : -1;
    c->slot = (int16_t)(fs >= 0 ? fs : slot);
    c->pos = fs >= 0 ? cand + 1 : 0;
  }
  /* items, as Pango's itemize_state_add_character: spaces take the font of
   * the item they are in, or of the next character when an item starts
   * with them; trailing spaces move to the next item when its font is
   * less of a fallback. Levels, scripts and emoji segments split items. */
  int item_start = 0, item_slot = -1, item_pos = 0, first_space = -1;
  for (int k = 0; k < n; k++) {
    const int is_space = ch[k].slot < 0;
    const int prev_first_space = first_space;
    if (is_space) {
      if (first_space < 0) {
        first_space = k;
      }
    }
    else {
      first_space = -1;
    }
    int fslot = ch[k].slot, fpos = ch[k].pos;
    if (k > 0) {
      const int seg_change = ch[k].seg != ch[k - 1].seg || ch[k].level != ch[k - 1].level ||
                             ch[k].script != ch[k - 1].script;
      if (!seg_change) {
        if (item_slot < 0 && fslot >= 0) {
          item_slot = fslot;
          item_pos = fpos;
        }
        else if (item_slot >= 0 && fslot < 0) {
          fslot = item_slot;
          fpos = item_pos;
        }
        if (item_slot == fslot) {
          continue;
        }
      }
      int at = k;
      if (!seg_change && item_slot >= 0 && prev_first_space > item_start && fpos < item_pos) {
        at = prev_first_space;
      }
      for (int j = item_start; j < at; j++) {
        ch[j].slot = (int16_t)(item_slot >= 0 ? item_slot : slot);
      }
      item_start = at;
    }
    item_slot = fslot;
    item_pos = fpos;
  }
  for (int j = item_start; j < n; j++) {
    ch[j].slot = (int16_t)(item_slot >= 0 ? item_slot : slot);
  }
  /* items in logical order, then shaped in visual order */
  for (int k = 0; k < n;) {
    int e = k + 1;
    while (e < n && ch[e].slot == ch[k].slot && ch[e].seg == ch[k].seg &&
           ch[e].level == ch[k].level && ch[e].script == ch[k].script)
    {
      e++;
    }
    WItem *it = wrun_item(w);
    if (!it) {
      goto out;
    }
    it->start = k; /* characters for now, bytes below */
    it->end = e;
    it->level = ch[k].level;
    it->slot = (uint8_t)ch[k].slot;
    k = e;
  }
  {
    int ordl[8];
    int *order = w->nit <= 8 ? ordl : (int *)malloc(sizeof(int) * (size_t)w->nit);
    if (!order) {
      goto out;
    }
    text_reorder(w->it, w->nit, order);
    float pen = 0.0f;
    const int nit = w->nit;
    for (int v = 0; v < nit; v++) {
      const int i = order[v];
      const WItem item = w->it[i];
      /* text_shape_item appends to the last item: make it this one */
      WItem *slotted = wrun_item(w);
      if (!slotted) {
        break;
      }
      *slotted = item;
      slotted->start = off[item.start];
      slotted->end = off[item.end];
      text_shape_item(w, item.slot, cp, off, n, item.start, item.end, item.level,
                      ch[item.start].script, text_script_lang(ch[item.start].script), &pen);
    }
    /* keep the items in visual order (the shaped copies) */
    memmove(w->it, w->it + nit, sizeof(WItem) * (size_t)(w->nit - nit));
    w->nit -= nit;
    for (int i = 0; i < w->n; i++) {
      w->g[i].item -= nit;
    }
    w->width = pen;
    if (order != ordl) {
      free(order);
    }
  }
out:
  if (ch != chl) {
    free(ch);
  }
  if (cp != cpl) {
    free(cp);
  }
  if (off != offl) {
    free(off);
  }
  if (types != tl) {
    free(types);
  }
}

/** \} */
