/* BUI-Awra: what Pango, fontconfig and HarfBuzz did for Awra 1's text, for
 * the portable text engine (included by gpu_raster.c):
 *
 *  - emoji itemization: Pango's emoji presentation scanner (Chromium's,
 *    extern/emoji) splits text into emoji and text segments; an emoji
 *    segment is drawn with the "emoji" family (a colour font) as Pango asks
 *    fontconfig for it;
 *  - per-character font fallback: a character the font lacks is drawn with
 *    the first font of fontconfig's sorted list (FcFontSort, by charset)
 *    that has it, on Windows with the system's font links (SystemLink) and
 *    Segoe UI Emoji / Symbol; spaces and default ignorables take the font of
 *    the run they are in, as Pango itemizes;
 *  - emoji sequences (ZWJ, skin tones, flags, keycaps, tags) shaped through
 *    the colour font's GSUB (single, multiple, ligature and contextual
 *    lookups of ccmp / liga / rlig / clig), as HarfBuzz does;
 *  - default ignorables hidden and missing space glyphs given HarfBuzz's
 *    fallback widths.
 *
 * Faces and font slots are cached; glyph bitmaps go through the shared
 * glyph cache like any other glyph.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "../extern/emoji/emoji_tables.h"

/* -------------------------------------------------------------------- */
/** \name Unicode properties
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

/* HarfBuzz's default ignorables: never drawn, no advance. */
static int text_ignorable(uint32_t c)
{
  if (c < 0xAD) {
    return 0;
  }
  return c == 0xAD || c == 0x34F || c == 0x61C || c == 0x115F || c == 0x1160 || c == 0x17B4 ||
         c == 0x17B5 || (c >= 0x180B && c <= 0x180F) || (c >= 0x200B && c <= 0x200F) ||
         (c >= 0x202A && c <= 0x202E) || (c >= 0x2060 && c <= 0x206F) || c == 0x3164 ||
         (c >= 0xFE00 && c <= 0xFE0F) || c == 0xFEFF || c == 0xFFA0 ||
         (c >= 0xFFF0 && c <= 0xFFF8) || (c >= 0x1BCA0 && c <= 0x1BCA3) ||
         (c >= 0x1D173 && c <= 0x1D17A) || (c >= 0xE0000 && c <= 0xE0FFF);
}

/* HarfBuzz's fallback width of a space character the font lacks, in ems
 * (hb-ot-shape-fallback: _hb_ot_shape_fallback_spaces), or 0 for "the space
 * glyph's own". */
static float text_space_em(uint32_t c)
{
  switch (c) {
    case 0x2000:
    case 0x2002:
      return 0.5f;
    case 0x2001:
    case 0x2003:
    case 0x3000:
      return 1.0f;
    case 0x2004:
      return 1.0f / 3.0f;
    case 0x2005:
      return 0.25f;
    case 0x2006:
      return 1.0f / 6.0f;
    case 0x2009:
      return 0.2f;
    case 0x200A:
      return 1.0f / 16.0f;
    case 0x205F:
      return 4.0f / 18.0f;
    default:
      return 0.0f;
  }
}

static int text_is_space_sep(uint32_t c)
{
  return c == ' ' || c == 0xA0 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F ||
         c == 0x3000;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GSUB: the substitutions an emoji font's sequences need
 *
 * Lookup types 1 (single), 2 (multiple), 4 (ligature), 5 (context) and 6
 * (chained context), formats 1-3, through extension lookups (7), of the
 * features ccmp, liga, rlig and clig, applied in lookup order over the
 * glyphs of a run as HarfBuzz applies them. Lookup flags (mark filtering)
 * are not applied: emoji fonts set none.
 * \{ */

typedef struct GsubBuf {
  uint16_t *g;  /* glyphs */
  uint32_t *cp; /* the character each glyph comes from (a ligature: its first) */
  int n, cap;
} GsubBuf;

typedef struct GsubTab {
  const uint8_t *t;
  uint32_t len;
} GsubTab;

static inline unsigned gs16(const GsubTab *T, uint32_t o)
{
  return o + 2 <= T->len ? ((unsigned)T->t[o] << 8) | T->t[o + 1] : 0;
}

static inline uint32_t gs32(const GsubTab *T, uint32_t o)
{
  return o + 4 <= T->len ? ((uint32_t)gs16(T, o) << 16) | gs16(T, o + 2) : 0;
}

/* Coverage index of `glyph`, or -1. */
static int gs_coverage(const GsubTab *T, uint32_t o, unsigned glyph)
{
  const unsigned fmt = gs16(T, o), n = gs16(T, o + 2);
  int lo = 0, hi = (int)n - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (fmt == 1) {
      const unsigned g = gs16(T, o + 4 + 2 * (uint32_t)mid);
      if (glyph < g) {
        hi = mid - 1;
      }
      else if (glyph > g) {
        lo = mid + 1;
      }
      else {
        return mid;
      }
    }
    else if (fmt == 2) {
      const uint32_t r = o + 4 + 6 * (uint32_t)mid;
      const unsigned s = gs16(T, r), e = gs16(T, r + 2);
      if (glyph < s) {
        hi = mid - 1;
      }
      else if (glyph > e) {
        lo = mid + 1;
      }
      else {
        return (int)(gs16(T, r + 4) + glyph - s);
      }
    }
    else {
      return -1;
    }
  }
  return -1;
}

static unsigned gs_class(const GsubTab *T, uint32_t o, unsigned glyph)
{
  const unsigned fmt = gs16(T, o);
  if (fmt == 1) {
    const unsigned start = gs16(T, o + 2), n = gs16(T, o + 4);
    return glyph >= start && glyph < start + n ? gs16(T, o + 6 + 2 * (glyph - start)) : 0;
  }
  if (fmt == 2) {
    int lo = 0, hi = (int)gs16(T, o + 2) - 1;
    while (lo <= hi) {
      const int mid = (lo + hi) / 2;
      const uint32_t r = o + 4 + 6 * (uint32_t)mid;
      if (glyph < gs16(T, r)) {
        hi = mid - 1;
      }
      else if (glyph > gs16(T, r + 2)) {
        lo = mid + 1;
      }
      else {
        return gs16(T, r + 4);
      }
    }
  }
  return 0;
}

static int gs_reserve(GsubBuf *b, int n)
{
  if (n <= b->cap) {
    return 1;
  }
  int cap = b->cap ? b->cap * 2 : 32;
  while (cap < n) {
    cap *= 2;
  }
  uint16_t *g = (uint16_t *)realloc(b->g, sizeof(uint16_t) * (size_t)cap);
  if (!g) {
    return 0;
  }
  b->g = g;
  uint32_t *cp = (uint32_t *)realloc(b->cp, sizeof(uint32_t) * (size_t)cap);
  if (!cp) {
    return 0;
  }
  b->cp = cp;
  b->cap = cap;
  return 1;
}

/* Replaces `count` glyphs at `i` with `n` glyphs (keeping the first one's
 * character). */
static int gs_replace(GsubBuf *b, int i, int count, const uint16_t *glyphs, int n)
{
  const uint32_t cp = b->cp[i];
  if (n > count && !gs_reserve(b, b->n + n - count)) {
    return 0;
  }
  memmove(b->g + i + n, b->g + i + count, sizeof(uint16_t) * (size_t)(b->n - i - count));
  memmove(b->cp + i + n, b->cp + i + count, sizeof(uint32_t) * (size_t)(b->n - i - count));
  for (int k = 0; k < n; k++) {
    b->g[i + k] = glyphs[k];
    b->cp[i + k] = cp;
  }
  b->n += n - count;
  return 1;
}

static int gs_lookup_at(const GsubTab *T, uint32_t list, unsigned index, GsubBuf *b, int i,
                        int depth, int *r_next);

/* What a sequence position must match: a glyph id, a class of a class
 * definition, or a coverage table (offset from the subtable). */
enum { GS_GLYPH = 0, GS_CLASS = 1, GS_COVER = 2 };

static int gs_test(const GsubTab *T, int kind, uint32_t base, unsigned v, unsigned glyph)
{
  switch (kind) {
    case GS_GLYPH:
      return glyph == v;
    case GS_CLASS:
      return gs_class(T, base, glyph) == v;
    default:
      return gs_coverage(T, base + v, glyph) >= 0;
  }
}

#  define GS_MAX_SEQ 32

/* Matches `n` values (u16 at `vals`) forwards from glyph j as HarfBuzz's
 * skippy iterator does: a default ignorable (VS16, ZWJ, tags) that doesn't
 * match is skipped. Positions into r_pos; returns the index after the last
 * one, or -1. */
static int gs_match_fwd(const GsubTab *T, const GsubBuf *b, int j, unsigned n, int kind,
                        uint32_t base, uint32_t vals, int *r_pos)
{
  for (unsigned k = 0; k < n; k++) {
    const unsigned v = gs16(T, vals + 2 * k);
    while (j < b->n && !gs_test(T, kind, base, v, b->g[j]) && text_ignorable(b->cp[j])) {
      j++;
    }
    if (j >= b->n || !gs_test(T, kind, base, v, b->g[j])) {
      return -1;
    }
    if (r_pos) {
      r_pos[k] = j;
    }
    j++;
  }
  return j;
}

/* The same backwards from glyph j (backtrack sequences). */
static int gs_match_back(const GsubTab *T, const GsubBuf *b, int j, unsigned n, int kind,
                         uint32_t base, uint32_t vals)
{
  for (unsigned k = 0; k < n; k++) {
    const unsigned v = gs16(T, vals + 2 * k);
    while (j >= 0 && !gs_test(T, kind, base, v, b->g[j]) && text_ignorable(b->cp[j])) {
      j--;
    }
    if (j < 0 || !gs_test(T, kind, base, v, b->g[j])) {
      return 0;
    }
    j--;
  }
  return 1;
}

/* Removes the glyphs at the (ascending) positions `pos` (ligature
 * components; the ignorables skipped between them stay, hidden later). */
static void gs_remove(GsubBuf *b, const int *pos, int n)
{
  int w = pos[0], k = 0;
  for (int r = pos[0]; r < b->n; r++) {
    if (k < n && r == pos[k]) {
      k++;
      continue;
    }
    b->g[w] = b->g[r];
    b->cp[w] = b->cp[r];
    w++;
  }
  b->n = w;
}

/* A context rule matched at the input positions `pos` (count of them):
 * applies its SeqLookupRecords; *r_next: where the main loop goes on. */
static void gs_apply_records(const GsubTab *T, uint32_t list, uint32_t recs, unsigned nrec,
                             GsubBuf *b, int *pos, int count, int depth, int *r_next)
{
  for (unsigned k = 0; k < nrec; k++) {
    const unsigned si = gs16(T, recs + 4 * k);
    const unsigned lookup = gs16(T, recs + 4 * k + 2);
    if ((int)si >= count || pos[si] >= b->n) {
      continue;
    }
    const int before = b->n;
    int next;
    gs_lookup_at(T, list, lookup, b, pos[si], depth + 1, &next);
    const int delta = b->n - before;
    if (delta) { /* glyphs added or removed after pos[si] move the positions after it */
      for (int m = 0; m < count; m++) {
        if (m != (int)si && pos[m] > pos[si]) {
          pos[m] += delta;
        }
      }
    }
  }
  const int last = pos[count - 1];
  *r_next = last + 1 > pos[0] ? last + 1 : pos[0] + 1;
}

/* One subtable at i. 1: applied (*r_next set). */
static int gs_subtable(const GsubTab *T, uint32_t list, unsigned type, uint32_t st, GsubBuf *b,
                       int i, int depth, int *r_next)
{
  const unsigned fmt = gs16(T, st);
  const unsigned g = b->g[i];
  int pos[GS_MAX_SEQ];
  switch (type) {
    case 1: {
      const int c = gs_coverage(T, st + gs16(T, st + 2), g);
      if (c < 0) {
        return 0;
      }
      if (fmt == 1) {
        b->g[i] = (uint16_t)(g + gs16(T, st + 4));
      }
      else if (fmt == 2 && (unsigned)c < gs16(T, st + 4)) {
        b->g[i] = (uint16_t)gs16(T, st + 6 + 2 * (uint32_t)c);
      }
      else {
        return 0;
      }
      *r_next = i + 1;
      return 1;
    }
    case 2: {
      const int c = gs_coverage(T, st + gs16(T, st + 2), g);
      if (c < 0 || fmt != 1 || (unsigned)c >= gs16(T, st + 4)) {
        return 0;
      }
      const uint32_t seq = st + gs16(T, st + 6 + 2 * (uint32_t)c);
      const unsigned n = gs16(T, seq);
      uint16_t out[GS_MAX_SEQ];
      if (n > GS_MAX_SEQ) {
        return 0;
      }
      for (unsigned k = 0; k < n; k++) {
        out[k] = (uint16_t)gs16(T, seq + 2 + 2 * k);
      }
      if (!gs_replace(b, i, 1, out, (int)n)) {
        return 0;
      }
      *r_next = i + (int)n;
      return 1;
    }
    case 4: {
      const int c = gs_coverage(T, st + gs16(T, st + 2), g);
      if (c < 0 || fmt != 1 || (unsigned)c >= gs16(T, st + 4)) {
        return 0;
      }
      const uint32_t set = st + gs16(T, st + 6 + 2 * (uint32_t)c);
      const unsigned nlig = gs16(T, set);
      for (unsigned l = 0; l < nlig; l++) {
        const uint32_t lig = set + gs16(T, set + 2 + 2 * l);
        const unsigned comps = gs16(T, lig + 2);
        if (comps == 0 || comps > GS_MAX_SEQ) {
          continue;
        }
        pos[0] = i;
        if (comps > 1 && gs_match_fwd(T, b, i + 1, comps - 1, GS_GLYPH, 0, lig + 4, pos + 1) < 0) {
          continue;
        }
        b->g[i] = (uint16_t)gs16(T, lig);
        if (comps > 1) {
          gs_remove(b, pos + 1, (int)comps - 1);
        }
        *r_next = i + 1;
        return 1;
      }
      return 0;
    }
    case 5:
    case 6: {
      const int chained = type == 6;
      pos[0] = i;
      if (fmt == 3) {
        uint32_t p = st + 2;
        unsigned nb = 0, nla = 0, nrec = 0;
        uint32_t back = 0, look = 0;
        if (chained) {
          nb = gs16(T, p);
          back = p + 2;
          p += 2 + 2 * nb;
        }
        const unsigned ni = gs16(T, p);
        if (!chained) {
          nrec = gs16(T, p + 2);
        }
        const uint32_t inp = p + 2 + (chained ? 0 : 2);
        p = inp + 2 * ni;
        if (chained) {
          nla = gs16(T, p);
          look = p + 2;
          p += 2 + 2 * nla;
          nrec = gs16(T, p);
          p += 2;
        }
        if (ni == 0 || ni > GS_MAX_SEQ || gs_coverage(T, st + gs16(T, inp), g) < 0) {
          return 0;
        }
        const int after = ni > 1 ? gs_match_fwd(T, b, i + 1, ni - 1, GS_COVER, st, inp + 2, pos + 1) :
                                   i + 1;
        if (after < 0 || !gs_match_back(T, b, i - 1, nb, GS_COVER, st, back) ||
            gs_match_fwd(T, b, after, nla, GS_COVER, st, look, NULL) < 0)
        {
          return 0;
        }
        gs_apply_records(T, list, p, nrec, b, pos, (int)ni, depth, r_next);
        return 1;
      }
      const int c = gs_coverage(T, st + gs16(T, st + 2), g);
      if (c < 0 || (fmt != 1 && fmt != 2)) {
        return 0;
      }
      /* format 1 compares glyphs, format 2 classes */
      uint32_t bcd = 0, icd = 0, lcd = 0, sets;
      unsigned nsets, idx;
      if (fmt == 1) {
        nsets = gs16(T, st + 4);
        sets = st + 6;
        idx = (unsigned)c;
      }
      else if (chained) {
        bcd = st + gs16(T, st + 4);
        icd = st + gs16(T, st + 6);
        lcd = st + gs16(T, st + 8);
        nsets = gs16(T, st + 10);
        sets = st + 12;
        idx = gs_class(T, icd, g);
      }
      else {
        icd = st + gs16(T, st + 4);
        nsets = gs16(T, st + 6);
        sets = st + 8;
        idx = gs_class(T, icd, g);
      }
      if (idx >= nsets || gs16(T, sets + 2 * idx) == 0) {
        return 0;
      }
      const int kind = fmt == 1 ? GS_GLYPH : GS_CLASS;
      const uint32_t set = st + gs16(T, sets + 2 * idx);
      const unsigned nrules = gs16(T, set);
      for (unsigned r = 0; r < nrules; r++) {
        const uint32_t rule = set + gs16(T, set + 2 + 2 * r);
        uint32_t p = rule;
        unsigned nb = 0, nla = 0, ni, nrec;
        uint32_t back = 0, inp, look = 0;
        if (chained) {
          nb = gs16(T, p);
          back = p + 2;
          p += 2 + 2 * nb;
          ni = gs16(T, p);
          inp = p + 2;
          p += 2 + 2 * (ni ? ni - 1 : 0);
          nla = gs16(T, p);
          look = p + 2;
          p += 2 + 2 * nla;
          nrec = gs16(T, p);
          p += 2;
        }
        else {
          ni = gs16(T, p);
          nrec = gs16(T, p + 2);
          inp = p + 4;
          p = inp + 2 * (ni ? ni - 1 : 0);
        }
        if (ni == 0 || ni > GS_MAX_SEQ) {
          continue;
        }
        const int after = ni > 1 ? gs_match_fwd(T, b, i + 1, ni - 1, kind, icd, inp, pos + 1) :
                                   i + 1;
        if (after < 0 || !gs_match_back(T, b, i - 1, nb, kind, bcd, back) ||
            gs_match_fwd(T, b, after, nla, kind, lcd, look, NULL) < 0)
        {
          continue;
        }
        gs_apply_records(T, list, p, nrec, b, pos, (int)ni, depth, r_next);
        return 1;
      }
      return 0;
    }
    default:
      return 0;
  }
}

/* Lookup `index` at glyph i. 1: applied. */
static int gs_lookup_at(const GsubTab *T, uint32_t list, unsigned index, GsubBuf *b, int i,
                        int depth, int *r_next)
{
  if (depth > 8 || index >= gs16(T, list) || i >= b->n) {
    return 0;
  }
  const uint32_t lk = list + gs16(T, list + 2 + 2 * index);
  unsigned type = gs16(T, lk);
  const unsigned nsub = gs16(T, lk + 4);
  for (unsigned s = 0; s < nsub; s++) {
    uint32_t st = lk + gs16(T, lk + 6 + 2 * s);
    unsigned t = type;
    if (t == 7) { /* extension */
      t = gs16(T, st + 2);
      st += gs32(T, st + 4);
    }
    if (gs_subtable(T, list, t, st, b, i, depth, r_next)) {
      return 1;
    }
  }
  return 0;
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
  int weight, italic;
  int n;
  FbCand *c;
  void *set; /* fontconfig: the FcFontSet the list reads charsets from */
} FbList;

#  define MAX_FB_LISTS 8
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
      snprintf(path, sizeof(path), "%s/%.*s", dirs[d], n, p);
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

static FbList *fb_list(const char *family, int weight, int italic)
{
  for (int i = 0; i < g_fb_count; i++) {
    FbList *l = &g_fb[i];
    if (l->weight == weight && l->italic == italic && strcmp(l->family, family) == 0) {
      return l;
    }
  }
  if (g_fb_count == MAX_FB_LISTS) {
    return NULL;
  }
  FbList *l = &g_fb[g_fb_count++];
  memset(l, 0, sizeof(*l));
  snprintf(l->family, sizeof(l->family), "%s", family);
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
/** \name Itemization: the font of every character, then glyphs
 * \{ */

static int face_glyph(int face, uint32_t cp)
{
  const Face *f = &g_faces[face];
#  if TEXT_FT
  if (f->ft_only) {
    return (int)FT_Get_Char_Index(f->ft, (FT_ULong)cp);
  }
#  endif
  return stbtt_FindGlyphIndex(&f->info, (int)cp);
}

static int face_has(int face, uint32_t cp)
{
  return face_glyph(face, cp) != 0;
}

/* The GSUB lookups of the features an emoji sequence needs (ccmp, liga,
 * rlig, clig), read once per face. */
static int face_gsub(Face *f)
{
  if (f->gsub_n >= 0) {
    return f->gsub_n > 0;
  }
  f->gsub_n = 0;
  const uint8_t *d = f->data;
  const uint32_t start = (uint32_t)f->fontstart;
  if (!d || f->len < 12 || start + 12 > (uint32_t)f->len) {
    return 0;
  }
  const unsigned ntab = ((unsigned)d[start + 4] << 8) | d[start + 5];
  uint32_t off = 0, len = 0;
  for (unsigned i = 0; i < ntab; i++) {
    const uint32_t r = start + 12 + 16 * i;
    if (r + 16 > (uint32_t)f->len) {
      break;
    }
    if (memcmp(d + r, "GSUB", 4) == 0) {
      off = ((uint32_t)d[r + 8] << 24) | ((uint32_t)d[r + 9] << 16) | ((uint32_t)d[r + 10] << 8) |
            d[r + 11];
      len = ((uint32_t)d[r + 12] << 24) | ((uint32_t)d[r + 13] << 16) |
            ((uint32_t)d[r + 14] << 8) | d[r + 15];
      break;
    }
  }
  if (!off || off >= (uint32_t)f->len || len > (uint32_t)f->len - off) {
    return 0;
  }
  const GsubTab T = {d + off, len};
  const uint32_t feats = gs16(&T, 6), list = gs16(&T, 8);
  const unsigned nfeat = gs16(&T, feats), nlook = gs16(&T, list);
  if (!nlook) {
    return 0;
  }
  uint8_t *want = (uint8_t *)calloc(nlook, 1);
  if (!want) {
    return 0;
  }
  int n = 0;
  for (unsigned i = 0; i < nfeat; i++) {
    const uint32_t rec = feats + 2 + 6 * i;
    if (rec + 6 > len) {
      break;
    }
    const uint8_t *tag = T.t + rec;
    if (memcmp(tag, "ccmp", 4) && memcmp(tag, "liga", 4) && memcmp(tag, "rlig", 4) &&
        memcmp(tag, "clig", 4))
    {
      continue;
    }
    const uint32_t ft = feats + gs16(&T, rec + 4);
    const unsigned nl = gs16(&T, ft + 2);
    for (unsigned k = 0; k < nl; k++) {
      const unsigned li = gs16(&T, ft + 4 + 2 * k);
      if (li < nlook && !want[li]) {
        want[li] = 1;
        n++;
      }
    }
  }
  if (n > 0) {
    f->gsub = (uint16_t *)malloc(sizeof(uint16_t) * (size_t)n);
    if (f->gsub) {
      int k = 0;
      for (unsigned li = 0; li < nlook; li++) {
        if (want[li]) {
          f->gsub[k++] = (uint16_t)li;
        }
      }
      f->gsub_off = off;
      f->gsub_len = len;
      f->gsub_n = n;
    }
  }
  free(want);
  return f->gsub_n > 0;
}

static void face_gsub_apply(Face *f, GsubBuf *b)
{
  if (!face_gsub(f)) {
    return;
  }
  const GsubTab T = {f->data + f->gsub_off, f->gsub_len};
  const uint32_t list = gs16(&T, 8);
  for (int l = 0; l < f->gsub_n; l++) {
    for (int i = 0; i < b->n;) {
      int next = i + 1;
      if (gs_lookup_at(&T, list, f->gsub[l], b, i, 0, &next) && next > i) {
        i = next;
      }
      else {
        i++;
      }
    }
  }
}

/* One glyph of a run after itemization. */
typedef struct WGlyph {
  uint16_t glyph;
  uint8_t slot;
  uint8_t flags; /* WG_ADV: `adv` replaces the glyph's advance */
  float adv;
} WGlyph;
#  define WG_ADV 1

typedef struct WRun {
  WGlyph *g;
  int n, cap;
  WGlyph local[64];
} WRun;

static void wrun_push(WRun *w, int slot, int glyph, int flags, float adv)
{
  if (w->n == w->cap) {
    const int cap = w->cap * 2;
    WGlyph *g = (WGlyph *)malloc(sizeof(WGlyph) * (size_t)cap);
    if (!g) {
      return;
    }
    memcpy(g, w->g, sizeof(WGlyph) * (size_t)w->n);
    if (w->g != w->local) {
      free(w->g);
    }
    w->g = g;
    w->cap = cap;
  }
  WGlyph *o = &w->g[w->n++];
  o->glyph = (uint16_t)glyph;
  o->slot = (uint8_t)slot;
  o->flags = (uint8_t)flags;
  o->adv = adv;
}

static void wrun_free(WRun *w)
{
  if (w->g != w->local) {
    free(w->g);
  }
}

/* The glyph of `cp` in `slot`'s face, or nothing for a default ignorable;
 * a space the face lacks becomes its space glyph at HarfBuzz's width. */
static void wrun_char(WRun *w, int slot, uint32_t cp, int glyph)
{
  if (text_ignorable(cp)) {
    return;
  }
  if (glyph == 0 && cp != ' ' && text_is_space_sep(cp)) {
    const FontSlot *s = &g_fonts[slot];
    const int space = face_glyph(s->face, ' ');
    if (space) {
      const float em = text_space_em(cp);
      wrun_push(w, slot, space, em > 0.0f ? WG_ADV : 0, em * (float)s->px64 / 64.0f);
      return;
    }
  }
  wrun_push(w, slot, glyph, 0, 0.0f);
}

/* The glyphs of `utf8` in slot `slot` and its fallbacks. */
static void text_itemize(int slot, const char *utf8, int len, WRun *w)
{
  const FontSlot *s = &g_fonts[slot];
  const int face0 = s->face;
  w->g = w->local;
  w->cap = (int)(sizeof(w->local) / sizeof(w->local[0]));
  w->n = 0;
  /* the common case: characters the font has, none of them special */
  int i = 0;
  for (; i < len;) {
    const unsigned char c = (unsigned char)utf8[i];
    if (c >= 0x80 || (c < 0x20 && c != '\t') || c == 0x7F) {
      break;
    }
    const int glyph = face_glyph(face0, c == '\t' ? ' ' : c);
    if (!glyph) {
      break;
    }
    wrun_push(w, slot, glyph, 0, 0.0f);
    i++;
  }
  if (i == len) {
    return;
  }
  w->n = 0; /* start over: fonts change, and spaces before may change with them */
  /* characters, emoji categories, segments, fonts and their positions */
  int n = 0;
  for (int k = 0; k < len; n++) {
    utf8_next(utf8, len, &k);
  }
  typedef struct Ch {
    uint32_t cp;
    int16_t slot; /* -1: a space (takes its run's font) */
    uint8_t seg;  /* 1: emoji segment, 2: with a variation selector */
    int pos;      /* the font's position in its list (Pango's font_position) */
  } Ch;
  Ch local[96];
  unsigned char tlocal[96];
  Ch *ch = n <= 96 ? local : (Ch *)malloc(sizeof(Ch) * (size_t)n);
  unsigned char *types = n <= 96 ? tlocal : (unsigned char *)malloc((size_t)n);
  if (!ch || !types) {
    if (ch != local) {
      free(ch);
    }
    if (types != tlocal) {
      free(types);
    }
    return;
  }
  for (int k = 0, j = 0; k < len; j++) {
    ch[j].cp = (uint32_t)utf8_next(utf8, len, &k);
    if (ch[j].cp == '\t') {
      ch[j].cp = ' ';
    }
    types[j] = emoji_category(ch[j].cp);
  }
  /* Pango's emoji iterator */
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
  FbList *text_list = NULL, *emoji_list = NULL;
  int text_tried = 0, emoji_tried = 0;
  for (int k = 0; k < n; k++) {
    Ch *c = &ch[k];
    c->slot = -1;
    c->pos = 0xFFFF;
    if (text_space_like(c->cp)) {
      continue;
    }
    if (c->seg & 1) { /* the "emoji" family, a colour font when a VS16 asks */
      if (!emoji_tried) {
        emoji_tried = 1;
        emoji_list = fb_list("emoji", weight, italic);
      }
      int cand = -1;
      if (emoji_list) {
        cand = fb_find(emoji_list, c->cp, (c->seg & 2) != 0);
        if (cand < 0 && (c->seg & 2)) {
          cand = fb_find(emoji_list, c->cp, 0);
        }
      }
      const int fs = cand >= 0 ? slot_fallback(slot, fb_face(emoji_list, cand)) : -1;
      if (fs >= 0) {
        c->slot = (int16_t)fs;
        c->pos = cand;
        continue;
      }
    }
    if (face_has(face0, c->cp)) {
      c->slot = (int16_t)slot;
      c->pos = 0;
      continue;
    }
    if (!text_tried) {
      text_tried = 1;
      text_list = fb_list(s->family, weight, italic);
    }
    const int cand = text_list ? fb_find(text_list, c->cp, 0) : -1;
    const int fs = cand >= 0 ? slot_fallback(slot, fb_face(text_list, cand)) : -1;
    c->slot = (int16_t)(fs >= 0 ? fs : slot);
    c->pos = fs >= 0 ? cand + 1 : 0;
  }
  /* items, as Pango's itemize_state_add_character: spaces take the font of
   * the item they are in, or of the next character when an item starts
   * with them; trailing spaces move to the next item when its font is
   * less of a fallback */
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
      const int seg_change = ch[k].seg != ch[k - 1].seg; /* emoji segments are runs of their own */
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
  /* glyphs, item by item; emoji sequences through the colour font's GSUB */
  GsubBuf b = {NULL, NULL, 0, 0};
  for (int k = 0; k < n;) {
    int e = k + 1;
    while (e < n && ch[e].slot == ch[k].slot && ch[e].seg == ch[k].seg) {
      e++;
    }
    const int is = ch[k].slot;
    const int face = g_fonts[is].face;
    if ((ch[k].seg & 1) && e - k > 1 && is != slot && gs_reserve(&b, e - k)) {
      b.n = e - k;
      for (int j = k; j < e; j++) {
        b.g[j - k] = (uint16_t)face_glyph(face, ch[j].cp);
        b.cp[j - k] = ch[j].cp;
      }
      face_gsub_apply(&g_faces[face], &b);
      for (int j = 0; j < b.n; j++) {
        wrun_char(w, is, b.cp[j], b.g[j]);
      }
    }
    else {
      for (int j = k; j < e; j++) {
        wrun_char(w, is, ch[j].cp, face_glyph(face, ch[j].cp));
      }
    }
    k = e;
  }
  free(b.g);
  free(b.cp);
  if (ch != local) {
    free(ch);
  }
  if (types != tlocal) {
    free(types);
  }
}

/** \} */
