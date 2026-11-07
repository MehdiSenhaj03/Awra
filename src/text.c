/* Awra 2 text: typography roles, measurement and paragraph layout (wrap,
 * ellipsize, a subset of Pango markup) over BUI's glyph-run text engine.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>

/* ----------------------------------------------------------------------- */
/* Typography (awra.css .awra-body ... .awra-reading) */

typedef struct Role {
  float em, weight, line_height, letter_spacing_em;
  int color; /* 0 foreground, 1 muted */
  bool mono;
} Role;

static const Role g_roles[] = {
    [AWRA_TYPOGRAPHY_BODY] = {1.0f, 400, 0, 0, 0, false},
    [AWRA_TYPOGRAPHY_MUTED] = {0.88f, 400, 0, 0, 1, false},
    [AWRA_TYPOGRAPHY_EYEBROW] = {0.78f, 700, 0, 0.08f, 1, false},
    [AWRA_TYPOGRAPHY_TITLE_3] = {1.08f, 640, 0, 0, 0, false},
    [AWRA_TYPOGRAPHY_TITLE_2] = {1.30f, 670, 1.16f, 0, 0, false},
    [AWRA_TYPOGRAPHY_TITLE_1] = {1.68f, 700, 1.12f, 0, 0, false},
    [AWRA_TYPOGRAPHY_DISPLAY] = {2.15f, 730, 1.05f, 0, 0, false},
    [AWRA_TYPOGRAPHY_MONOSPACE] = {1.0f, 400, 0, 0, 0, true},
    [AWRA_TYPOGRAPHY_LABEL] = {1.0f, 700, 0, 0, 0, false},
    [AWRA_TYPOGRAPHY_READING] = {1.07f, 400, 1.04f, 0, 0, false},
};

AwraFont awra_font_for(AwraTypography t)
{
  if (t < 0 || t > AWRA_TYPOGRAPHY_READING) {
    t = AWRA_TYPOGRAPHY_BODY;
  }
  const Role *r = &g_roles[t];
  AwraFont f;
  f.family = r->mono ? awra_mono_family() : awra_font_family();
  f.size = awra_font_px() * r->em * awra_scale();
  f.weight = (int)r->weight;
  f.italic = false;
  f.letter_spacing = r->letter_spacing_em * f.size;
  return f;
}

AwraRGBA awra_typography_color(AwraTypography t)
{
  if (t < 0 || t > AWRA_TYPOGRAPHY_READING) {
    t = AWRA_TYPOGRAPHY_BODY;
  }
  return g_roles[t].color ? awra_tokens()->muted : awra_tokens()->foreground;
}

static float role_line_height(AwraTypography t)
{
  return (t >= 0 && t <= AWRA_TYPOGRAPHY_READING) ? g_roles[t].line_height : 0.0f;
}

static int font_flags(const AwraFont *f)
{
  return BUI_FONT_WEIGHT(f->weight) | (f->italic ? BUI_FONT_ITALIC : 0) |
         (f->weight >= 600 ? BUI_FONT_BOLD : 0);
}

float awra_font_ascent(const AwraFont *f)
{
  float a, d;
  BUI_text_metrics(f->family, font_flags(f), f->size, &a, &d);
  return a;
}

void awra_font_extents(const AwraFont *f, const char *s, int len, float *r_ascent,
                       float *r_descent)
{
  if (len < 0) {
    len = (int)strlen(s);
  }
  BUI_text_extents(f->family, font_flags(f), f->size, s, len, r_ascent, r_descent);
}

float awra_font_line_height(const AwraFont *f)
{
  float a, d;
  BUI_text_metrics(f->family, font_flags(f), f->size, &a, &d);
  return a + d;
}

/* Letter spacing in Pango units, as GTK gives Pango the CSS value
 * (pango_attr_letter_spacing_new (px * PANGO_SCALE)). */
static int ls_units(const AwraFont *f)
{
  return (int)(f->letter_spacing * 1024.0f);
}

/* distribute_letter_spacing: the half before a grapheme (px) and the half
 * after it */
static void ls_halves(int ls, float *r_left, float *r_right)
{
  int left = ls / 2;
  if ((ls & 1023) == 0) {
    left = (left + 512) & ~1023; /* hinting: PANGO_UNITS_ROUND */
  }
  *r_left = (float)left / 1024.0f;
  *r_right = (float)(ls - left) / 1024.0f;
}

/* `xflags`: BUI_TEXT_LTR / BUI_TEXT_RTL for a run of one bidi level of a
 * paragraph laid out here, 0 for text that is a paragraph of its own. The
 * width as shaped in the context set (BUI_text_set_context). */
static float plain_width_x(const AwraFont *f, int xflags, const char *s, int len)
{
  float w = 0.0f;
  if (len <= 0) {
    return 0.0f;
  }
  BUI_text_measure(f->family, font_flags(f) | xflags, f->size, s, len, &w);
  return w;
}

static int utf8_len(unsigned char c)
{
  return c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
}

static int utf8_count(const char *s, int len)
{
  int n = 0;
  for (int i = 0; i < len; i += utf8_len((unsigned char)s[i])) {
    n++;
  }
  return n;
}

/* A line of its own (a PangoLayout of one line): letter spacing between
 * its graphemes, not before the first or after the last
 * (adjust_line_letter_spacing). */
float awra_text_width(const AwraFont *f, const char *s, int len)
{
  if (len < 0) {
    len = (int)strlen(s);
  }
  const int ls = ls_units(f);
  if (!ls || len <= 0) {
    return plain_width_x(f, 0, s, len);
  }
  const BuiTextContext c = {NULL, 0, NULL, 0, ls, 0};
  BUI_text_set_context(&c);
  const float w = plain_width_x(f, 0, s, len);
  BUI_text_set_context(NULL);
  float left, right;
  ls_halves(ls, &left, &right);
  return w - left - right;
}

/* draws `s` shaped in the context set, its origin at x */
static void draw_text_x(AwraDraw *d, const AwraFont *f, int xflags, float x, float baseline,
                        const char *s, int len, const AwraRGBA *color)
{
  if (len <= 0 || !color) {
    return;
  }
  float c[4];
  awra_rgba_f(color, c);
  c[3] *= d->opacity;
  if (c[3] <= 0.0f) {
    return;
  }
  BUI_raster_set_font(d->r, f->family, font_flags(f) | xflags);
  SR_text(d->r, x, baseline, s, len, f->size, c);
}

/* the glyphs of bytes [from, to) of the run `s` shaped whole (in the
 * context set), drawn where they are in it (the run's origin at x) */
static void draw_text_part_x(AwraDraw *d, const AwraFont *f, int xflags, float x, float baseline,
                             const char *s, int len, int from, int to, const AwraRGBA *color)
{
  float c[4];
  awra_rgba_f(color, c);
  c[3] *= d->opacity;
  if (c[3] <= 0.0f || to <= from) {
    return;
  }
  BUI_raster_set_font(d->r, f->family, font_flags(f) | xflags);
  SR_text_part(d->r, x, baseline, s, len, from, to, f->size, c);
}

void awra_draw_text(AwraDraw *d, const AwraFont *f, float x, float baseline, const char *s,
                    int len, const AwraRGBA *color)
{
  if (len < 0) {
    len = (int)strlen(s);
  }
  const int ls = ls_units(f);
  if (!ls) {
    draw_text_x(d, f, 0, x, baseline, s, len, color);
    return;
  }
  const BuiTextContext c = {NULL, 0, NULL, 0, ls, 0};
  BUI_text_set_context(&c);
  float left, right;
  ls_halves(ls, &left, &right);
  draw_text_x(d, f, 0, x - left, baseline, s, len, color);
  BUI_text_set_context(NULL);
}

/* The ink box of `s` drawn in `f` (Pango's pixel ink extents: the pixels
 * its glyphs touch), relative to the pen's start on the baseline: x and y
 * of its top left (y negative above the baseline), width and height. False
 * when nothing is drawn. */
bool awra_text_ink(const AwraFont *f, const char *s, int len, int *r_x, int *r_y, int *r_w, int *r_h)
{
  *r_x = *r_y = *r_w = *r_h = 0;
  if (len < 0) {
    len = (int)strlen(s);
  }
  float width;
  const BuiTextRun *run = BUI_text_prepare(f->family, font_flags(f), f->size, s, len, &width);
  if (!run) {
    return false;
  }
  int mw, mh, ox, asc;
  const uint8_t *m = BUI_text_run_mask(run, &mw, &mh, &ox, &asc);
  if (!m) {
    return false;
  }
  int x0 = mw, y0 = mh, x1 = -1, y1 = -1;
  for (int y = 0; y < mh; y++) {
    for (int x = 0; x < mw; x++) {
      if (m[(size_t)y * mw + x]) {
        x0 = x < x0 ? x : x0;
        x1 = x > x1 ? x : x1;
        y0 = y < y0 ? y : y0;
        y1 = y > y1 ? y : y1;
      }
    }
  }
  if (x1 < 0) {
    return false;
  }
  *r_x = x0 - ox;
  *r_y = y0 - asc;
  *r_w = x1 - x0 + 1;
  *r_h = y1 - y0 + 1;
  return true;
}

/* ----------------------------------------------------------------------- */
/* Paragraph layout */

typedef struct Span {
  int start, end; /* byte range in the plain text */
  int weight;     /* 0: the base font's */
  int italic;     /* -1 base */
  float scale;    /* size factor, 1 = base */
  bool has_color;
  AwraRGBA color;
  float alpha; /* < 0: none */
  bool underline, strike, mono;
  bool has_bg;
  AwraRGBA bg;
  short link; /* index in the layout's links, -1 none */
} Span;

typedef struct Run {
  int start, end; /* -1: an ellipsis */
  int span;
  float x, w;     /* x: visual, from the line's left edge */
  float sx;       /* a part kept by ellipsizing: where it starts in its whole run (visual) */
  int shp_start, shp_end; /* the run it was shaped as (a part: the whole one) */
  float lsl;      /* letter spacing trimmed off its left (the line's first run) */
  uint8_t level;  /* bidi embedding level */
  uint8_t hyph;   /* the line breaks after it with a hyphen (BuiTextContext.hyphen) */
  bool zero;      /* the space a wrapped line ends with: no width, not drawn */
} Run;

typedef struct Line {
  int first_run, n_runs; /* runs in logical order */
  int start, end;        /* the line's text (trailing spaces included, '\n' not) */
  float w;
  float ascent, height;
  bool rtl;              /* its paragraph's direction */
} Line;

struct AwraTextLayout {
  char *text; /* plain text */
  int len;
  Span *spans;
  int n_spans;
  AwraFont font;
  AwraWrap wrap;
  AwraEllipsize ellipsize;
  int max_lines;
  AwraJustify justify;
  float line_factor;
  float para_extra, wrap_extra; /* raster px above each paragraph's first line, and
                                 * above its other lines (GtkTextView's pixels-above-lines
                                 * and pixels-inside-wrap) */
  /* the layout for one width */
  int laid_width; /* -2: dirty */
  Line *lines;
  int n_lines, cap_lines;
  Run *runs;
  int n_runs, cap_runs;
  char *ell_text; /* text of an ellipsized line (with "…") */
  int w, h, baseline;
  /* cached measures */
  int min_w, nat_w;
  /* links (<a href>) and their look */
  char **links;
  int n_links;
  int hover_link;
  bool link_style;
  AwraRGBA link_color, link_hover_color;
  /* inline objects: each U+FFFC is a square of this side (raster px) */
  float object_size;
  /* bidi: each byte's embedding level (bits 0-6) and its paragraph's
   * direction (bit 7: right to left), len + 1 bytes; NULL when the text
   * has nothing right-to-left (all levels 0) */
  uint8_t *levels;
  uint16_t *breaks; /* BUI_text_breaks of the text (len + 1), made on demand */
  bool bidi_done;
  bool base_rtl;   /* the widget reads right to left: paragraphs without a
                    * strong character are right to left (Pango's context
                    * base direction) */
  bool dir_back;   /* a paragraph without a strong character takes the next one's
                    * direction when none before it has one (GtkTextView) */
  float wf;        /* the widest line, unrounded */
  bool has_tabs;   /* the text holds a tab: each is a run of its own, to the next stop */
};

#define OBJECT_CHAR "\xEF\xBF\xBC"

static bool is_object(const AwraTextLayout *l, int i)
{
  return l->object_size > 0.0f && i + 3 <= l->len && (unsigned char)l->text[i] == 0xEF &&
         (unsigned char)l->text[i + 1] == 0xBF && (unsigned char)l->text[i + 2] == 0xBC;
}

static bool is_tab(const AwraTextLayout *l, int i)
{
  return l->has_tabs && i >= 0 && i < l->len && l->text[i] == '\t';
}

/* Pango's default tab stops (shape_tab): every eight spaces of the
 * layout's font, the next one at least a space past `x` (raster px from
 * the line's start); the tab's advance */
static float tab_advance(const AwraTextLayout *l, float x)
{
  const float tab = awra_text_width(&l->font, "        ", 8);
  if (tab <= 0.0f) {
    return 0.0f;
  }
  const float space = tab / 8.0f;
  float pos = tab * floorf((x + space) / tab);
  while (pos < x + space - 0.001f) {
    pos += tab;
  }
  return pos - x;
}

AwraTextLayout *awra_text_layout_new(void)
{
  AwraTextLayout *l = (AwraTextLayout *)calloc(1, sizeof(AwraTextLayout));
  if (l) {
    l->laid_width = -2;
    l->min_w = l->nat_w = -1;
    l->text = awra_strdup("");
    l->hover_link = -1;
  }
  return l;
}

void awra_text_layout_free(AwraTextLayout *l)
{
  if (!l) {
    return;
  }
  free(l->text);
  free(l->spans);
  free(l->lines);
  free(l->runs);
  free(l->ell_text);
  for (int i = 0; i < l->n_links; i++) {
    free(l->links[i]);
  }
  free(l->links);
  free(l->levels);
  free(l->breaks);
  free(l);
}

static void dirty(AwraTextLayout *l)
{
  l->laid_width = -2;
  l->min_w = l->nat_w = -1;
}

/* the text changed: its bidi levels and breaks too */
static void dirty_text(AwraTextLayout *l)
{
  dirty(l);
  l->has_tabs = l->text && strchr(l->text, '\t') != NULL;
  l->bidi_done = false;
  free(l->breaks);
  l->breaks = NULL;
}

const char *awra_text_layout_text(const AwraTextLayout *l)
{
  return l->text;
}

/* -- markup -- */

typedef struct Builder {
  char *buf;
  int len, cap;
  Span *spans;
  int n, cap_spans;
  Span stack[16];
  int depth;
  char **links;
  int n_links;
} Builder;

static void b_putc(Builder *b, const char *s, int n)
{
  if (b->len + n + 1 > b->cap) {
    b->cap = (b->len + n + 1) * 2;
    b->buf = (char *)realloc(b->buf, (size_t)b->cap);
  }
  memcpy(b->buf + b->len, s, (size_t)n);
  b->len += n;
  b->buf[b->len] = 0;
}

static void b_span_open(Builder *b, Span s)
{
  if (b->depth < 16) {
    s.start = b->len;
    b->stack[b->depth++] = s;
  }
}

static void b_span_close(Builder *b)
{
  if (b->depth == 0) {
    return;
  }
  Span s = b->stack[--b->depth];
  s.end = b->len;
  if (s.end <= s.start) {
    return;
  }
  if (b->n == b->cap_spans) {
    b->cap_spans = b->cap_spans ? b->cap_spans * 2 : 8;
    b->spans = (Span *)realloc(b->spans, sizeof(Span) * (size_t)b->cap_spans);
  }
  b->spans[b->n++] = s;
}

static Span span_inherit(const Builder *b)
{
  Span s;
  if (b->depth > 0) {
    s = b->stack[b->depth - 1];
  }
  else {
    memset(&s, 0, sizeof(s));
    s.italic = -1;
    s.scale = 1.0f;
    s.alpha = -1.0f;
    s.link = -1;
  }
  return s;
}

static const char *attr_value(const char *tag, const char *name, char *out, size_t size)
{
  const size_t nl = strlen(name);
  for (const char *p = tag; (p = strstr(p, name)) != NULL; p += nl) {
    if (p > tag && (p[-1] != ' ' && p[-1] != '\t')) {
      continue;
    }
    const char *q = p + nl;
    while (*q == ' ') {
      q++;
    }
    if (*q != '=') {
      continue;
    }
    q++;
    while (*q == ' ') {
      q++;
    }
    const char quote = (*q == '"' || *q == '\'') ? *q++ : 0;
    size_t n = 0;
    while (q[n] && (quote ? q[n] != quote : (q[n] != ' ' && q[n] != '>'))) {
      n++;
    }
    if (n >= size) {
      n = size - 1;
    }
    memcpy(out, q, n);
    out[n] = 0;
    return out;
  }
  return NULL;
}

static int parse_weight(const char *v)
{
  if (!strcmp(v, "bold")) {
    return 700;
  }
  if (!strcmp(v, "semibold")) {
    return 600;
  }
  if (!strcmp(v, "medium")) {
    return 500;
  }
  if (!strcmp(v, "light")) {
    return 300;
  }
  if (!strcmp(v, "heavy") || !strcmp(v, "ultrabold")) {
    return 800;
  }
  if (!strcmp(v, "normal") || !strcmp(v, "book")) {
    return 400;
  }
  const int w = atoi(v);
  return w > 0 ? w : 0;
}

static float parse_size(const char *v)
{
  if (!strcmp(v, "xx-small")) {
    return 0.5787f;
  }
  if (!strcmp(v, "x-small")) {
    return 0.6944f;
  }
  if (!strcmp(v, "small") || !strcmp(v, "smaller")) {
    return 0.8333f;
  }
  if (!strcmp(v, "medium")) {
    return 1.0f;
  }
  if (!strcmp(v, "large") || !strcmp(v, "larger")) {
    return 1.2f;
  }
  if (!strcmp(v, "x-large")) {
    return 1.44f;
  }
  if (!strcmp(v, "xx-large")) {
    return 1.728f;
  }
  const size_t n = strlen(v);
  if (n > 1 && v[n - 1] == '%') {
    return (float)atof(v) / 100.0f;
  }
  if (n > 2 && !strcmp(v + n - 2, "pt")) {
    return (float)(atof(v) * 96.0 / 72.0 / awra_font_px());
  }
  const double units = atof(v); /* Pango units: 1024ths of a point */
  return units > 0 ? (float)(units / 1024.0 * 96.0 / 72.0 / awra_font_px()) : 1.0f;
}

static void parse_markup(AwraTextLayout *l, const char *m)
{
  Builder b;
  memset(&b, 0, sizeof(b));
  b_putc(&b, "", 0);
  for (const char *p = m; *p;) {
    if (*p == '<') {
      const char *end = strchr(p, '>');
      if (!end) {
        b_putc(&b, p, (int)strlen(p));
        break;
      }
      char tag[256];
      const size_t tl = (size_t)(end - p - 1) < sizeof(tag) - 1 ? (size_t)(end - p - 1) :
                                                                   sizeof(tag) - 1;
      memcpy(tag, p + 1, tl);
      tag[tl] = 0;
      p = end + 1;
      if (tag[0] == '/') {
        b_span_close(&b);
        continue;
      }
      Span s = span_inherit(&b);
      char name[16] = "";
      sscanf(tag, "%15s", name);
      if (!strcmp(name, "b")) {
        s.weight = 700;
      }
      else if (!strcmp(name, "i")) {
        s.italic = 1;
      }
      else if (!strcmp(name, "u")) {
        s.underline = true;
      }
      else if (!strcmp(name, "s")) {
        s.strike = true;
      }
      else if (!strcmp(name, "tt")) {
        s.mono = true;
      }
      else if (!strcmp(name, "small")) {
        s.scale *= 0.8333f;
      }
      else if (!strcmp(name, "big")) {
        s.scale *= 1.2f;
      }
      else if (!strcmp(name, "a")) {
        char v[1024];
        if (attr_value(tag, "href", v, sizeof(v))) {
          /* the href as written, entities resolved */
          char *href = awra_strdup(v);
          char *o = href;
          for (const char *q = v; *q;) {
            if (*q == '&') {
              const char *ents[] = {"&amp;", "&", "&lt;", "<", "&gt;", ">", "&quot;", "\"", "&apos;", "'"};
              bool done = false;
              for (int k = 0; k < 10; k += 2) {
                const size_t n = strlen(ents[k]);
                if (!strncmp(q, ents[k], n)) {
                  *o++ = ents[k + 1][0];
                  q += n;
                  done = true;
                  break;
                }
              }
              if (done) {
                continue;
              }
            }
            *o++ = *q++;
          }
          *o = 0;
          b.links = (char **)realloc(b.links, sizeof(char *) * (size_t)(b.n_links + 1));
          b.links[b.n_links] = href;
          s.link = (short)b.n_links++;
          s.underline = true;
        }
      }
      else if (!strcmp(name, "span")) {
        char v[96];
        if (attr_value(tag, "weight", v, sizeof(v)) || attr_value(tag, "font_weight", v, sizeof(v))) {
          s.weight = parse_weight(v);
        }
        if (attr_value(tag, "foreground", v, sizeof(v)) || attr_value(tag, "color", v, sizeof(v)) ||
            attr_value(tag, "fgcolor", v, sizeof(v)))
        {
          s.has_color = awra_parse_color(v, &s.color);
        }
        if (attr_value(tag, "alpha", v, sizeof(v)) || attr_value(tag, "fgalpha", v, sizeof(v))) {
          const size_t n = strlen(v);
          s.alpha = n && v[n - 1] == '%' ? (float)atof(v) / 100.0f : (float)atof(v) / 65535.0f;
        }
        if (attr_value(tag, "size", v, sizeof(v)) || attr_value(tag, "font_size", v, sizeof(v))) {
          s.scale *= parse_size(v);
        }
        if (attr_value(tag, "style", v, sizeof(v))) {
          s.italic = !strcmp(v, "italic") || !strcmp(v, "oblique");
        }
        if (attr_value(tag, "font_family", v, sizeof(v)) || attr_value(tag, "face", v, sizeof(v))) {
          s.mono = !strcmp(v, "monospace");
        }
        if (attr_value(tag, "underline", v, sizeof(v))) {
          s.underline = strcmp(v, "none") != 0;
        }
        if (attr_value(tag, "background", v, sizeof(v)) || attr_value(tag, "bgcolor", v, sizeof(v))) {
          s.has_bg = awra_parse_color(v, &s.bg);
        }
        if (s.has_bg && attr_value(tag, "bgalpha", v, sizeof(v))) {
          const size_t n = strlen(v);
          s.bg.a = n && v[n - 1] == '%' ? (float)atof(v) / 100.0f : (float)atof(v) / 65535.0f;
        }
        if (attr_value(tag, "strikethrough", v, sizeof(v))) {
          s.strike = !strcmp(v, "true");
        }
      }
      b_span_open(&b, s);
      if (tl > 0 && tag[tl - 1] == '/') {
        b_span_close(&b);
      }
      continue;
    }
    if (*p == '&') {
      const char *semi = strchr(p, ';');
      if (semi && semi - p < 10) {
        char ent[12];
        memcpy(ent, p + 1, (size_t)(semi - p - 1));
        ent[semi - p - 1] = 0;
        const char *out = NULL;
        char num[8];
        if (!strcmp(ent, "amp")) {
          out = "&";
        }
        else if (!strcmp(ent, "lt")) {
          out = "<";
        }
        else if (!strcmp(ent, "gt")) {
          out = ">";
        }
        else if (!strcmp(ent, "quot")) {
          out = "\"";
        }
        else if (!strcmp(ent, "apos")) {
          out = "'";
        }
        else if (ent[0] == '#') {
          unsigned cp = ent[1] == 'x' ? (unsigned)strtoul(ent + 2, NULL, 16) :
                                        (unsigned)strtoul(ent + 1, NULL, 10);
          int n = 0;
          if (cp < 0x80) {
            num[n++] = (char)cp;
          }
          else if (cp < 0x800) {
            num[n++] = (char)(0xC0 | (cp >> 6));
            num[n++] = (char)(0x80 | (cp & 63));
          }
          else if (cp < 0x10000) {
            num[n++] = (char)(0xE0 | (cp >> 12));
            num[n++] = (char)(0x80 | ((cp >> 6) & 63));
            num[n++] = (char)(0x80 | (cp & 63));
          }
          else {
            num[n++] = (char)(0xF0 | (cp >> 18));
            num[n++] = (char)(0x80 | ((cp >> 12) & 63));
            num[n++] = (char)(0x80 | ((cp >> 6) & 63));
            num[n++] = (char)(0x80 | (cp & 63));
          }
          num[n] = 0;
          out = num;
        }
        if (out) {
          b_putc(&b, out, (int)strlen(out));
          p = semi + 1;
          continue;
        }
      }
    }
    b_putc(&b, p, 1);
    p++;
  }
  while (b.depth) {
    b_span_close(&b);
  }
  free(l->text);
  free(l->spans);
  for (int i = 0; i < l->n_links; i++) {
    free(l->links[i]);
  }
  free(l->links);
  l->text = b.buf ? b.buf : awra_strdup("");
  l->len = b.len;
  l->spans = b.spans;
  l->n_spans = b.n;
  l->links = b.links;
  l->n_links = b.n_links;
  l->hover_link = -1;
}

void awra_text_layout_set_text(AwraTextLayout *l, const char *text, bool markup)
{
  if (!text) {
    text = "";
  }
  if (markup) {
    parse_markup(l, text);
  }
  else {
    if (!l->n_spans && strcmp(l->text, text) == 0) {
      return;
    }
    free(l->spans);
    l->spans = NULL;
    l->n_spans = 0;
    for (int i = 0; i < l->n_links; i++) {
      free(l->links[i]);
    }
    free(l->links);
    l->links = NULL;
    l->n_links = 0;
    l->hover_link = -1;
    awra_set_str(&l->text, text);
    l->len = (int)strlen(l->text);
  }
  dirty_text(l);
}

void awra_text_layout_set_base_dir(AwraTextLayout *l, bool rtl)
{
  if (l->base_rtl != rtl) {
    l->base_rtl = rtl;
    dirty(l);
    l->bidi_done = false;
  }
}

void awra_text_layout_set_font(AwraTextLayout *l, const AwraFont *f)
{
  if (l->font.family == f->family && l->font.size == f->size && l->font.weight == f->weight &&
      l->font.italic == f->italic && l->font.letter_spacing == f->letter_spacing)
  {
    return;
  }
  if (l->font.family != f->family) {
    free(l->breaks); /* items, and so tailored breaks, follow the font */
    l->breaks = NULL;
  }
  l->font = *f;
  dirty(l);
}

void awra_text_layout_set_line_factor(AwraTextLayout *l, float factor);
void awra_text_layout_set_line_factor(AwraTextLayout *l, float factor)
{
  if (l->line_factor != factor) {
    l->line_factor = factor;
    dirty(l);
  }
}

void awra_text_layout_set_line_extra(AwraTextLayout *l, float paragraph, float wrapped)
{
  if (l->para_extra != paragraph || l->wrap_extra != wrapped) {
    l->para_extra = paragraph;
    l->wrap_extra = wrapped;
    dirty(l);
  }
}

void awra_text_layout_set_wrap(AwraTextLayout *l, AwraWrap wrap, AwraEllipsize e, int max_lines)
{
  if (l->wrap != wrap || l->ellipsize != e || l->max_lines != max_lines) {
    l->wrap = wrap;
    l->ellipsize = e;
    l->max_lines = max_lines;
    dirty(l);
  }
}

void awra_text_layout_set_text_view(AwraTextLayout *l, bool on)
{
  if (l->dir_back != on) {
    l->dir_back = on;
    dirty_text(l);
  }
}

void awra_text_layout_set_justify(AwraTextLayout *l, AwraJustify j)
{
  l->justify = j;
}

/* the font of a span */
static AwraFont span_font(const AwraTextLayout *l, int span)
{
  AwraFont f = l->font;
  if (span < 0) {
    return f;
  }
  const Span *s = &l->spans[span];
  if (s->weight > 0) {
    f.weight = s->weight;
  }
  if (s->italic >= 0) {
    f.italic = s->italic != 0;
  }
  if (s->mono) {
    f.family = awra_mono_family();
  }
  f.size *= s->scale;
  return f;
}

/* the innermost span covering byte i: spans nest, and are stored as they
 * close, inner before outer, so of spans that start together the one that
 * ends first, then the one stored first (<span><a>x</a></span>: the link) */
static int span_at(const AwraTextLayout *l, int i)
{
  int found = -1;
  for (int k = 0; k < l->n_spans; k++) {
    const Span *s = &l->spans[k];
    if (s->start <= i && i < s->end) {
      if (found < 0 || s->start > l->spans[found].start ||
          (s->start == l->spans[found].start && s->end < l->spans[found].end))
      {
        found = k;
      }
    }
  }
  return found;
}

/* -- bidi -- */

/* Whether `s` may hold right-to-left text: characters of the right-to-left
 * blocks (Hebrew, Arabic, Syriac, Thaana, NKo, Samaritan, Mandaic, their
 * presentation forms, the RTL SMP blocks) or the controls that open
 * right-to-left text (RLM, RLE, RLO, RLI, FSI). Text without any is laid
 * out with no bidi work at all. */
static bool text_maybe_rtl(const char *s, int len)
{
  for (int i = 0; i < len; i++) {
    const unsigned char c = (unsigned char)s[i];
    if (c < 0xD6) {
      continue;
    }
    if (c <= 0xDF) {
      return true; /* U+0580..U+07FF */
    }
    const unsigned char c1 = i + 1 < len ? (unsigned char)s[i + 1] : 0;
    const unsigned char c2 = i + 2 < len ? (unsigned char)s[i + 2] : 0;
    if ((c == 0xE0 && c1 >= 0xA0 && c1 <= 0xA3) ||                   /* U+0800..U+08FF */
        (c == 0xE2 && c1 == 0x80 && (c2 == 0x8F || c2 == 0xAB || c2 == 0xAE)) ||
        (c == 0xE2 && c1 == 0x81 && (c2 == 0xA7 || c2 == 0xA8)) ||  /* RLI, FSI */
        (c == 0xEF && ((c1 >= 0xAC && c1 <= 0xB7) || (c1 >= 0xB9 && c1 <= 0xBB))) ||
        (c == 0xF0 && (c1 == 0x90 || c1 == 0x9E) && c2 >= 0xA0))
    {
      return true;
    }
  }
  return false;
}

/* The levels of every paragraph (Pango: a paragraph's direction is its first
 * strong character's, else the previous paragraph's, else the whole text's
 * first strong character's, else the context's, left to right in Awra;
 * GtkTextView: else the next paragraph's). */
static void ensure_bidi(AwraTextLayout *l)
{
  if (l->bidi_done) {
    return;
  }
  l->bidi_done = true;
  free(l->levels);
  l->levels = NULL;
  if (!l->base_rtl && !text_maybe_rtl(l->text, l->len)) {
    return;
  }
  uint8_t *lv = (uint8_t *)malloc((size_t)l->len + 1);
  if (!lv) {
    return;
  }
  /* Pango starts from the first strong character of the whole text, else
   * the context's base direction */
  int prev = l->dir_back ? 0 : BUI_text_direction(l->text, l->len);
  if (!prev && !l->dir_back && l->base_rtl) {
    prev = 2;
  }
  for (int pos = 0; pos <= l->len;) {
    int end = pos;
    while (end < l->len && l->text[end] != '\n') {
      end++;
    }
    int dir = BUI_text_direction(l->text + pos, end - pos);
    if (!dir) {
      dir = prev;
    }
    if (!dir && l->dir_back) {
      for (int q = end + 1; q <= l->len && !dir;) {
        int qe = q;
        while (qe < l->len && l->text[qe] != '\n') {
          qe++;
        }
        dir = BUI_text_direction(l->text + q, qe - q);
        q = qe + 1;
      }
    }
    if (!dir) {
      dir = l->base_rtl ? 2 : 1;
    }
    prev = dir;
    BUI_text_bidi(l->text + pos, end - pos, dir, lv + pos);
    const uint8_t para = (uint8_t)(dir == 2 ? 0x80 : 0);
    for (int i = pos; i < end; i++) {
      lv[i] = (uint8_t)(lv[i] | para);
    }
    lv[end] = (uint8_t)(para | (dir == 2 ? 1 : 0)); /* the '\n' (or the end) */
    pos = end + 1;
  }
  l->levels = lv;
}

static int level_at(const AwraTextLayout *l, int i)
{
  return l->levels ? l->levels[i] & 0x7F : 0;
}

static bool para_rtl(const AwraTextLayout *l, int i)
{
  return l->levels && (l->levels[i] & 0x80);
}

/* BUI_TEXT_LTR / RTL for a run at `level` (0 when the text has no bidi:
 * runs then share the engine's cache with plain text) */
static int level_flags(const AwraTextLayout *l, int level)
{
  return l->levels ? ((level & 1) ? BUI_TEXT_RTL : BUI_TEXT_LTR) : 0;
}

static const uint16_t *ensure_breaks(AwraTextLayout *l)
{
  if (!l->breaks) {
    l->breaks = (uint16_t *)malloc(sizeof(uint16_t) * ((size_t)l->len + 1));
    if (l->breaks) {
      /* as PangoLayout has them for this font (items end where it falls back) */
      BUI_text_breaks_font(l->font.family, font_flags(&l->font), l->font.size, l->text, l->len,
                           l->breaks);
    }
  }
  return l->breaks;
}

static bool is_cursor(AwraTextLayout *l, int i)
{
  const uint16_t *b = ensure_breaks(l);
  return i <= 0 || i >= l->len || !b || (b[i] & BUI_TEXT_CURSOR);
}

/* the next / previous cursor position (grapheme boundary) */
static int next_cursor(AwraTextLayout *l, int i)
{
  if (i >= l->len) {
    return l->len;
  }
  do {
    i += utf8_len((unsigned char)l->text[i]);
  } while (i < l->len && !is_cursor(l, i));
  return BK_MIN(i, l->len);
}

static int prev_cursor(AwraTextLayout *l, int i)
{
  if (i <= 0) {
    return 0;
  }
  do {
    i--;
    while (i > 0 && ((unsigned char)l->text[i] & 0xC0) == 0x80) {
      i--;
    }
  } while (i > 0 && !is_cursor(l, i));
  return i;
}

/* end of the uniform segment starting at i: style, inline objects, bidi level */
static int segment_end(const AwraTextLayout *l, int i, int limit)
{
  int end = limit;
  if (l->has_tabs) {
    if (is_tab(l, i)) {
      return BK_MIN(limit, i + 1); /* a tab is a run of its own */
    }
    const char *t = memchr(l->text + i, '\t', (size_t)(limit - i));
    if (t) {
      end = (int)(t - l->text);
    }
  }
  if (l->object_size > 0.0f) {
    if (is_object(l, i)) {
      return BK_MIN(limit, i + 3); /* an object is a run of its own */
    }
    const char *o = strstr(l->text + i, OBJECT_CHAR);
    if (o && o - l->text < end) {
      end = (int)(o - l->text);
    }
  }
  for (int k = 0; k < l->n_spans; k++) {
    if (l->spans[k].start > i && l->spans[k].start < end) {
      end = l->spans[k].start;
    }
    if (l->spans[k].end > i && l->spans[k].end < end) {
      end = l->spans[k].end;
    }
  }
  if (l->levels) {
    const int lv = level_at(l, i);
    for (int k = i + 1; k < end; k++) {
      if (level_at(l, k) != lv) {
        end = k;
        break;
      }
    }
  }
  return end;
}

/* Shapes what follows as Pango shapes the item [a, b) of a paragraph:
 * with the paragraph's text around it as context, the span's letter
 * spacing and a hyphen when the line breaks after it with one (`hyph`,
 * BuiTextContext.hyphen). Cleared with BUI_text_set_context(NULL). */
static void run_context(const AwraTextLayout *l, const AwraFont *f, int a, int b, int hyph)
{
  int ps = a, pe = b;
  while (ps > 0 && a - ps < 24 && l->text[ps - 1] != '\n') {
    ps--;
  }
  while (pe < l->len && pe - b < 24 && l->text[pe] != '\n') {
    pe++;
  }
  const BuiTextContext c = {l->text + ps, a - ps, l->text + b, pe - b, ls_units(f), hyph};
  BUI_text_set_context(&c);
}

/* width of a uniform segment [a, b): text, or inline objects */
static float seg_width(const AwraTextLayout *l, const AwraFont *f, int a, int b, int hyph)
{
  if (b > a && is_object(l, a)) {
    return l->object_size * (float)((b - a + 2) / 3);
  }
  if (b <= a) {
    return 0.0f;
  }
  run_context(l, f, a, b, hyph);
  const float w = plain_width_x(f, level_flags(l, level_at(l, a)), l->text + a, b - a);
  BUI_text_set_context(NULL);
  return w;
}

/* the width of [a, b) shaped as runs; `hyph`: the last one ends with a hyphen */
static float range_width(const AwraTextLayout *l, int a, int b, int hyph)
{
  float w = 0.0f;
  while (a < b) {
    const int e = segment_end(l, a, b);
    if (is_tab(l, a)) { /* `a` starts the line */
      w += tab_advance(l, w);
      a = e;
      continue;
    }
    const int sp = span_at(l, a);
    AwraFont f = span_font(l, sp);
    w += seg_width(l, &f, a, e, e == b ? hyph : 0);
    a = e;
  }
  return w;
}

static Run *push_run(AwraTextLayout *l)
{
  if (l->n_runs == l->cap_runs) {
    l->cap_runs = l->cap_runs ? l->cap_runs * 2 : 16;
    l->runs = (Run *)realloc(l->runs, sizeof(Run) * (size_t)l->cap_runs);
  }
  Run *r = &l->runs[l->n_runs++];
  memset(r, 0, sizeof(*r));
  return r;
}

/* the runs of [a, b) (lines place them in visual order: finish_line);
 * `hyph`: the last one ends with a hyphen */
static void add_run(AwraTextLayout *l, int a, int b, int hyph)
{
  while (a < b) {
    const int e = segment_end(l, a, b);
    Run *r = push_run(l);
    r->start = a;
    r->end = e;
    r->span = span_at(l, a);
    r->level = (uint8_t)level_at(l, a);
    r->shp_start = a;
    r->shp_end = e;
    r->hyph = (uint8_t)(e == b && !is_object(l, a) && !is_tab(l, a) ? hyph : 0);
    AwraFont f = span_font(l, r->span);
    if (is_tab(l, a)) {
      /* to the next stop from where the line has got to */
      float x = 0.0f;
      for (int k = l->n_lines ? l->lines[l->n_lines - 1].first_run : 0; k < l->n_runs - 1; k++) {
        x += l->runs[k].w;
      }
      r->w = tab_advance(l, x);
    }
    else {
      r->w = seg_width(l, &f, a, e, r->hyph);
    }
    a = e;
  }
}

/* an ellipsis run of the span at `at`, at the paragraph's level */
static void add_ellipsis(AwraTextLayout *l, int at, float w)
{
  Run *r = push_run(l);
  r->start = r->end = -1;
  r->span = span_at(l, at);
  r->w = w;
  r->level = (uint8_t)(para_rtl(l, at) ? 1 : 0);
}

/* The line's runs placed in visual order (bidi rule L2 over their levels),
 * its width. */
static void finish_line(AwraTextLayout *l, Line *ln)
{
  Run *r = &l->runs[ln->first_run];
  const int n = ln->n_runs;
  int local[32];
  int *order = n <= 32 ? local : (int *)malloc(sizeof(int) * (size_t)n);
  if (!order) {
    return;
  }
  int maxl = 0, minodd = 255;
  for (int i = 0; i < n; i++) {
    order[i] = i;
    maxl = BK_MAX(maxl, (int)r[i].level);
    if ((r[i].level & 1) && r[i].level < minodd) {
      minodd = r[i].level;
    }
  }
  for (int lv = maxl; lv >= minodd && lv > 0; lv--) {
    for (int i = 0; i < n;) {
      if (r[order[i]].level < lv) {
        i++;
        continue;
      }
      int j = i;
      while (j < n && r[order[j]].level >= lv) {
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
  /* letter spacing: none before the line's visually first grapheme or
   * after its last (adjust_line_letter_spacing, after reordering) */
  int first = -1, last = -1;
  for (int v = 0; v < n; v++) {
    const Run *q = &r[order[v]];
    if (!q->zero && q->start >= 0 && !is_object(l, q->start) && !is_tab(l, q->start)) {
      first = first < 0 ? order[v] : first;
      last = order[v];
    }
  }
  if (first >= 0) {
    AwraFont f0 = span_font(l, r[first].span), f1 = span_font(l, r[last].span);
    float left, right, unused;
    ls_halves(ls_units(&f0), &left, &unused);
    ls_halves(ls_units(&f1), &unused, &right);
    r[first].lsl = left;
    r[first].w -= left;
    r[last].w -= right;
  }
  float x = 0.0f;
  for (int v = 0; v < n; v++) {
    r[order[v]].x = x;
    x += r[order[v]].w;
  }
  ln->w = x;
  if (order != local) {
    free(order);
  }
}

static bool has_non_ascii(const char *s, int len)
{
  for (int i = 0; i < len; i++) {
    if ((unsigned char)s[i] >= 0x80) {
      return true;
    }
  }
  return false;
}

static bool has_letters(const char *s, int len)
{
  for (int i = 0; i < len; i++) {
    const int c = (unsigned char)s[i] | 0x20;
    if (c >= 'a' && c <= 'z') {
      return true;
    }
  }
  return false;
}

static void line_metrics(AwraTextLayout *l, Line *ln, bool para_start)
{
  /* Pango's logical extents of a line: the union of its runs' fonts (a run
   * drawn only with fallback fonts has theirs); an empty line has the
   * layout font's */
  float asc = 0.0f, desc = 0.0f;
  int counted = 0;
  for (int i = 0; i < ln->n_runs; i++) {
    const Run *r = &l->runs[ln->first_run + i];
    if ((r->start >= 0 && is_object(l, r->start)) || r->zero) {
      continue;
    }
    AwraFont f = span_font(l, r->span);
    float ra, rd;
    if (r->start < 0 || r->end <= r->start ||
        (!has_non_ascii(l->text + r->start, r->end - r->start) &&
         (BUI_text_latin_own_font() || !has_letters(l->text + r->start, r->end - r->start))))
    {
      /* ASCII drawn with the font itself: its metrics */
      ra = awra_font_ascent(&f);
      rd = awra_font_line_height(&f) - ra;
    }
    else {
      /* the run as shaped (its context), so the engine's run is shared */
      run_context(l, &f, r->shp_start, r->shp_end, r->hyph);
      BUI_text_extents(f.family, font_flags(&f) | level_flags(l, r->level), f.size,
                       l->text + r->shp_start, r->shp_end - r->shp_start, &ra, &rd);
      BUI_text_set_context(NULL);
    }
    asc = counted ? fmaxf(asc, ra) : ra;
    desc = counted ? fmaxf(desc, rd) : rd;
    counted = 1;
  }
  if (!counted) {
    asc = awra_font_ascent(&l->font);
    desc = awra_font_line_height(&l->font) - asc;
  }
  float h = asc + desc;
  if (l->object_size > 0.0f) {
    for (int i = 0; i < ln->n_runs; i++) {
      const Run *r = &l->runs[ln->first_run + i];
      if (r->start >= 0 && is_object(l, r->start)) {
        /* a square standing on the baseline, 4/5 above it (Pango shape) */
        const float desc = h - asc;
        const float oa = l->object_size * 0.8f, od = l->object_size * 0.2f;
        asc = fmaxf(asc, oa);
        h = asc + fmaxf(desc, od);
        break;
      }
    }
  }
  if (l->line_factor > 0.0f) {
    const float nh = h * l->line_factor;
    asc += (nh - h) * 0.5f;
    h = nh;
  }
  const float extra = para_start ? l->para_extra : l->wrap_extra;
  if (extra > 0.0f) {
    asc += extra;
    h += extra;
  }
  ln->ascent = asc;
  ln->height = h;
}

static Line *push_line(AwraTextLayout *l)
{
  if (l->n_lines == l->cap_lines) {
    l->cap_lines = l->cap_lines ? l->cap_lines * 2 : 4;
    l->lines = (Line *)realloc(l->lines, sizeof(Line) * (size_t)l->cap_lines);
  }
  Line *ln = &l->lines[l->n_lines++];
  memset(ln, 0, sizeof(*ln));
  ln->first_run = l->n_runs;
  return ln;
}

static const char ELLIPSIS[] = "\xE2\x80\xA6";

/* Ellipsizes the line [a, b) to `width` as Pango does
 * (_pango_layout_line_ellipsize): over the line's runs in logical order and
 * their shaped clusters, a gap starts at the cluster at the mode's centre
 * (END: the line's end, START: its start, MIDDLE: its middle), widened to
 * grapheme boundaries, then grown one span at a time on the side that
 * keeps it closest to the centre until the line fits. The ellipsis takes
 * the font at the gap's start and the lowest level of the text it hides,
 * and fills the line out to `width`; what is kept is drawn from the runs
 * as they were shaped (no reshaping: letters keep their joined forms). */
typedef struct EllRun {
  Run r;
  BuiTextCluster *cl;
  int ncl;
} EllRun;

typedef struct EllIter {
  int k, c; /* run, cluster */
} EllIter;

static float ell_width(const EllRun *er, EllIter it)
{
  return er[it.k].cl[it.c].width;
}

static bool ell_prev(const EllRun *er, EllIter *it)
{
  if (it->c > 0) {
    it->c--;
    return true;
  }
  for (int k = it->k - 1; k >= 0; k--) {
    if (er[k].ncl > 0) {
      it->k = k;
      it->c = er[k].ncl - 1;
      return true;
    }
  }
  return false;
}

static bool ell_next(const EllRun *er, int nr, EllIter *it)
{
  if (it->c + 1 < er[it->k].ncl) {
    it->c++;
    return true;
  }
  for (int k = it->k + 1; k < nr; k++) {
    if (er[k].ncl > 0) {
      it->k = k;
      it->c = 0;
      return true;
    }
  }
  return false;
}

static float ell_ellipsis(AwraTextLayout *l, const EllRun *er, EllIter gs)
{
  AwraFont f = span_font(l, span_at(l, er[gs.k].cl[gs.c].start));
  return awra_text_width(&f, ELLIPSIS, 3);
}

/* a kept part [from, to) of a run: drawn from the run's own shaping */
static void ell_keep(AwraTextLayout *l, const EllRun *e, int c0, int c1)
{
  if (c1 <= c0) {
    return;
  }
  Run *r = push_run(l);
  *r = e->r;
  r->start = e->cl[c0].start;
  r->end = e->cl[c1 - 1].end;
  float before = 0.0f, kept = 0.0f, after = 0.0f;
  for (int c = 0; c < e->ncl; c++) {
    if (c < c0) {
      before += e->cl[c].width;
    }
    else if (c < c1) {
      kept += e->cl[c].width;
    }
    else {
      after += e->cl[c].width;
    }
  }
  r->w = kept;
  r->sx = (r->level & 1) ? after : before; /* where the part starts in the run, visually */
}

static void ellipsize_line(AwraTextLayout *l, Line *ln, int a, int b, float width)
{
  free(l->ell_text);
  l->ell_text = NULL;
  /* the line's runs in logical order, and their clusters */
  const int first = l->n_runs;
  add_run(l, a, b, 0);
  const int nr = l->n_runs - first;
  EllRun local[16];
  EllRun *er = nr <= 16 ? local : (EllRun *)calloc((size_t)nr, sizeof(EllRun));
  if (!er || nr == 0) {
    if (er && er != local) {
      free(er);
    }
    ln->n_runs = l->n_runs - ln->first_run;
    finish_line(l, ln);
    return;
  }
  float total = 0.0f;
  int ok = 1;
  for (int k = 0; k < nr; k++) {
    EllRun *e = &er[k];
    e->r = l->runs[first + k];
    total += e->r.w;
    const int n = e->r.end - e->r.start;
    if (is_object(l, e->r.start)) {
      e->cl = (BuiTextCluster *)malloc(sizeof(BuiTextCluster));
      e->ncl = e->cl ? 1 : 0;
      if (e->cl) {
        e->cl[0] = (BuiTextCluster){e->r.start, e->r.end, e->r.w};
      }
    }
    else {
      AwraFont f = span_font(l, e->r.span);
      const int xf = font_flags(&f) | level_flags(l, e->r.level);
      run_context(l, &f, e->r.start, e->r.end, 0);
      const int count = BUI_text_clusters(f.family, xf, f.size, l->text + e->r.start, n, NULL, 0);
      e->cl = count > 0 ? (BuiTextCluster *)malloc(sizeof(BuiTextCluster) * (size_t)count) : NULL;
      e->ncl = e->cl ? BUI_text_clusters(f.family, xf, f.size, l->text + e->r.start, n, e->cl, count) : 0;
      BUI_text_set_context(NULL);
      for (int c = 0; c < e->ncl; c++) {
        e->cl[c].start += e->r.start;
        e->cl[c].end += e->r.start;
      }
    }
    ok = ok && e->ncl > 0;
  }
  l->n_runs = first;
  if (ok) {
    /* the cluster at the gap's centre */
    const float center = l->ellipsize == AWRA_ELLIPSIZE_START ? 0.0f :
                         l->ellipsize == AWRA_ELLIPSIZE_MIDDLE ? total / 2.0f :
                                                                  total;
    float x = 0.0f;
    int k = 0;
    for (; k < nr; k++) {
      if (x + er[k].r.w > center) {
        break;
      }
      x += er[k].r.w;
    }
    if (k == nr) {
      k--;
      x -= er[k].r.w;
    }
    int c = 0;
    float cw = 0.0f;
    bool have = false;
    for (; c < er[k].ncl; c++) {
      cw = er[k].cl[c].width;
      have = true;
      if (x + cw > center) {
        break;
      }
      x += cw;
    }
    if (c == er[k].ncl) {
      c--;
      x -= cw;
    }
    (void)have;
    EllIter gs = {k, c}, ge = {k, c};
    float gsx = x, gex = x + cw;
    /* widened to grapheme boundaries */
    while (!(gs.k == 0 && gs.c == 0) && !is_cursor(l, er[gs.k].cl[gs.c].start)) {
      if (!ell_prev(er, &gs)) {
        break;
      }
      gsx -= ell_width(er, gs);
    }
    while (!(ge.k == nr - 1 && ge.c == er[ge.k].ncl - 1) && !is_cursor(l, er[ge.k].cl[ge.c].end)) {
      if (!ell_next(er, nr, &ge)) {
        break;
      }
      gex += ell_width(er, ge);
    }
    float ew = ell_ellipsis(l, er, gs);
    while (total - (gex - gsx) + ew > width) {
      /* one span further on each side (remove_one_span) */
      EllIter ns = gs, ne = ge;
      float nsx = gsx, nex = gex, w = 0.0f;
      do {
        if (!ell_prev(er, &ns)) {
          break;
        }
        w = ell_width(er, ns);
        nsx -= w;
      } while (!((ns.k == 0 && ns.c == 0) || is_cursor(l, er[ns.k].cl[ns.c].start)) || w == 0.0f);
      do {
        if (!ell_next(er, nr, &ne)) {
          break;
        }
        w = ell_width(er, ne);
        nex += w;
      } while (!((ne.k == nr - 1 && ne.c == er[ne.k].ncl - 1) || is_cursor(l, er[ne.k].cl[ne.c].end)) ||
               w == 0.0f);
      if (gex == nex && gsx == nsx) {
        break;
      }
      if (gex == nex || (gsx != nsx && center - nsx < nex - center)) {
        gs = ns;
        gsx = nsx;
        ew = ell_ellipsis(l, er, gs);
      }
      else {
        ge = ne;
        gex = nex;
      }
    }
    const float extra = fmaxf(width - (total - (gex - gsx) + ew), 0.0f);
    /* the line: what comes before the gap, the ellipsis, what comes after */
    for (int q = 0; q < gs.k; q++) {
      Run *r = push_run(l);
      *r = er[q].r;
    }
    ell_keep(l, &er[gs.k], 0, gs.c);
    int level = 255;
    for (int q = gs.k; q <= ge.k; q++) {
      level = BK_MIN(level, (int)er[q].r.level);
    }
    add_ellipsis(l, er[gs.k].cl[gs.c].start, ew + extra);
    l->runs[l->n_runs - 1].level = (uint8_t)level;
    ell_keep(l, &er[ge.k], ge.c + 1, er[ge.k].ncl);
    for (int q = ge.k + 1; q < nr; q++) {
      Run *r = push_run(l);
      *r = er[q].r;
    }
  }
  else {
    for (int q = 0; q < nr; q++) {
      Run *r = push_run(l);
      *r = er[q].r;
    }
  }
  for (int q = 0; q < nr; q++) {
    free(er[q].cl);
  }
  if (er != local) {
    free(er);
  }
  ln->n_runs = l->n_runs - ln->first_run;
  finish_line(l, ln);
}

/* The hyphen a line broken at `k` ends with (BuiTextContext.hyphen) */
static int break_hyphen(AwraTextLayout *l, int k, int para_end)
{
  const uint16_t *br = ensure_breaks(l);
  if (!br || k >= para_end) {
    return 0;
  }
  return (br[k] & BUI_TEXT_REMOVES_PRECEDING) ? 2 : (br[k] & BUI_TEXT_INSERTS_HYPHEN) ? 1 : 0;
}

/* A paragraph being broken into lines: its characters' logical widths
 * (Pango's log widths: each cluster's width shared by its characters, at
 * their first byte), from shaping it whole as Pango shapes an item once,
 * to find the breaks worth measuring. */
typedef struct Para {
  int start, end;
  float *lw;         /* indexed from `start`; NULL until needed */
  float safe;        /* Pango's safe distance: three average characters */
  float hyphen_w;    /* a hyphen's width (find_hyphen_width) */
  int ls_from, ls;   /* the first line separator at or after ls_from (-1: none) */
} Para;

static void para_widths(AwraTextLayout *l, Para *P)
{
  if (P->lw) {
    return;
  }
  const int n = P->end - P->start;
  P->lw = (float *)calloc((size_t)(n > 0 ? n : 1), sizeof(float));
  if (!P->lw) {
    return;
  }
  for (int a = P->start; a < P->end;) {
    const int e = segment_end(l, a, P->end);
    if (is_object(l, a)) {
      for (int k = a; k < e; k += 3) {
        P->lw[k - P->start] = l->object_size;
      }
      a = e;
      continue;
    }
    AwraFont f = span_font(l, span_at(l, a));
    const int xf = font_flags(&f) | level_flags(l, level_at(l, a));
    BuiTextCluster local[64];
    run_context(l, &f, a, e, 0);
    int nc = BUI_text_clusters(f.family, xf, f.size, l->text + a, e - a, NULL, 0);
    BuiTextCluster *cl = nc <= 64 ? local : (BuiTextCluster *)malloc(sizeof(BuiTextCluster) * (size_t)nc);
    nc = cl ? BUI_text_clusters(f.family, xf, f.size, l->text + a, e - a, cl, nc) : 0;
    BUI_text_set_context(NULL);
    for (int c = 0; c < nc; c++) {
      const int cs = a + cl[c].start, ce = a + cl[c].end;
      const int chars = utf8_count(l->text + cs, ce - cs);
      for (int k = cs; k < ce; k += utf8_len((unsigned char)l->text[k])) {
        P->lw[k - P->start] = chars > 0 ? cl[c].width / (float)chars : 0.0f;
      }
    }
    if (cl != local) {
      free(cl);
    }
    a = e;
  }
}

/* the end of the line starting at `a`: after the first line separator
 * (U+2028), else the paragraph's end */
static int para_line_limit(const AwraTextLayout *l, Para *P, int a)
{
  if (P->ls_from < 0 || a < P->ls_from || (P->ls >= 0 && a > P->ls)) {
    P->ls = -1;
    for (int q = a; q + 2 < P->end; q++) {
      if ((unsigned char)l->text[q] == 0xE2 && (unsigned char)l->text[q + 1] == 0x80 &&
          (unsigned char)l->text[q + 2] == 0xA8)
      {
        P->ls = q;
        break;
      }
    }
    P->ls_from = a;
  }
  return P->ls >= 0 ? P->ls + 3 : P->end;
}

/* Where the line starting at `a` breaks to fit `W`, as Pango's
 * process_item chooses: the last break opportunity whose line fits (its
 * last character not counted when it is white, a hyphen counted where
 * one is shown), else the narrowest; at least one character. Breaks far
 * from the width are judged by the log widths, the others measured whole
 * (kerning, whole pixels), as Pango measures them; a narrower line further
 * on is taken (joined Arabic letters). `lim`: the line's end anyway. */
static int line_break(AwraTextLayout *l, Para *P, int a, int lim, float W)
{
  const uint16_t *br = ensure_breaks(l);
  para_widths(l, P);
  const float *lw = P->lw;
  int mode = l->wrap == AWRA_WRAP_CHAR ? BUI_TEXT_CHAR_BREAK : BUI_TEXT_LINE_BREAK;
  int best = -1;
  float best_w = 0.0f;
  for (;;) {
    float acc = 0.0f; /* the log widths of [a, k) */
    for (int k = a, prev = a; k <= lim; prev = k, k += k < lim ? utf8_len((unsigned char)l->text[k]) : 1) {
      if (k > a) {
        acc += is_tab(l, prev) ? tab_advance(l, acc) : lw ? lw[prev - P->start] : 0.0f;
      }
      if (k == a || !(k == lim || !br || (br[k] & mode))) {
        continue;
      }
      const bool white = k < P->end && br && (br[prev] & BUI_TEXT_WHITE);
      const int hyph = white ? 0 : break_hyphen(l, k, P->end);
      const float extra = white ? -(lw ? lw[prev - P->start] : 0.0f) : hyph ? P->hyphen_w : 0.0f;
      if (lw && best >= 0 && fminf(acc + extra, acc) > W + P->safe) {
        break; /* nothing further can fit or be narrower */
      }
      float w;
      if (lw && acc + extra < W - P->safe) {
        w = acc + extra; /* clearly fits */
      }
      else {
        w = range_width(l, a, white ? prev : k, hyph);
      }
      if (best < 0 || w <= W + 0.01f || w < best_w) {
        best = k;
        best_w = w;
      }
    }
    if (best_w > W + 0.01f && l->wrap == AWRA_WRAP_WORD_CHAR && mode == BUI_TEXT_LINE_BREAK) {
      mode = BUI_TEXT_CHAR_BREAK; /* PANGO_WRAP_WORD_CHAR: try again inside words */
      best = -1;
      continue;
    }
    break;
  }
  return best > a ? best : lim;
}

static void layout(AwraTextLayout *l, int width)
{
  if (l->laid_width == width) {
    return;
  }
  ensure_bidi(l);
  l->laid_width = width;
  l->n_lines = 0;
  l->n_runs = 0;
  const float W = width < 0 ? 1e9f : (float)width;
  const bool wrap = l->wrap != AWRA_WRAP_NONE && width >= 0;
  const int max_lines = l->max_lines > 0 ? l->max_lines : (1 << 30);
  /* process_item looks at breaks up to three average characters past the
   * width (pango_font_metrics_get_approximate_char_width: the sample
   * text's width per character) */
  static const char sample[] = "The quick brown fox jumps over the lazy dog.";
  const float safe = wrap ? 3.0f * awra_text_width(&l->font, sample, (int)sizeof(sample) - 1) /
                                (float)(sizeof(sample) - 1) :
                            0.0f;
  const float hyphen_w = wrap ? awra_text_width(&l->font, "\xE2\x80\x90", 3) : 0.0f;
  int pos = 0;
  while (pos <= l->len) {
    int para_end = pos;
    while (para_end < l->len && l->text[para_end] != '\n') {
      para_end++;
    }
    const bool rtl = para_rtl(l, pos);
    int a = pos;
    Para P = {pos, para_end, NULL, safe, hyphen_w, -1, -1};
    do {
      const bool para_start = a == pos;
      Line *ln = push_line(l);
      ln->start = a;
      ln->rtl = rtl;
      const bool last_allowed = l->n_lines >= max_lines;
      const bool ellipsize_here = l->ellipsize != AWRA_ELLIPSIZE_NONE && (!wrap || last_allowed);
      /* a line separator (U+2028) ends the line, unless it is ellipsized
       * (Pango's BREAK_LINE_SEPARATOR) */
      const int lim = ellipsize_here ? para_end : para_line_limit(l, &P, a);
      /* the rest of the line as one: by its log widths when far too wide */
      bool fits = true;
      if (wrap || ellipsize_here) {
        if (wrap && P.lw) {
          float acc = 0.0f;
          for (int k = a; k < lim && acc <= W + P.safe; k++) {
            acc += is_tab(l, k) ? tab_advance(l, acc) : P.lw[k - P.start];
          }
          fits = acc <= W + P.safe;
        }
        fits = fits && range_width(l, a, lim, 0) <= W + 0.01f;
      }
      if (fits || (!wrap && !ellipsize_here) || (lim < para_end && !wrap)) {
        add_run(l, a, lim, 0);
        ln->n_runs = l->n_runs - ln->first_run;
        finish_line(l, ln);
        a = lim;
      }
      else if (ellipsize_here) {
        ellipsize_line(l, ln, a, para_end, W);
        a = para_end;
      }
      else {
        const int brk = line_break(l, &P, a, lim, W);
        /* the line keeps the white space it breaks after; its last
         * character, white, takes no width (zero_line_final_trailing_whitespace) */
        int vis = brk;
        const uint16_t *br = ensure_breaks(l);
        if (brk < para_end && brk > a && br) {
          int c = brk - 1;
          while (c > a && ((unsigned char)l->text[c] & 0xC0) == 0x80) {
            c--;
          }
          if (br[c] & BUI_TEXT_WHITE) {
            vis = c;
          }
        }
        add_run(l, a, vis, brk == vis ? break_hyphen(l, brk, para_end) : 0);
        if (vis < brk) {
          const int first_space = l->n_runs;
          add_run(l, vis, brk, 0);
          for (int k = first_space; k < l->n_runs; k++) {
            l->runs[k].w = 0.0f;
            l->runs[k].zero = true;
          }
        }
        a = brk;
        ln->n_runs = l->n_runs - ln->first_run;
        finish_line(l, ln);
      }
      ln = &l->lines[l->n_lines - 1];
      ln->end = a < para_end ? a : para_end;
      line_metrics(l, ln, para_start);
      if (l->n_lines >= max_lines && (a < para_end || para_end < l->len)) {
        pos = l->len + 1;
        break;
      }
    } while (a < para_end);
    free(P.lw);
    if (pos > l->len) {
      break;
    }
    pos = para_end + 1;
  }
  float h = 0.0f, w = 0.0f;
  for (int i = 0; i < l->n_lines; i++) {
    h += l->lines[i].height;
    w = fmaxf(w, l->lines[i].w);
  }
  if (l->n_lines == 0) {
    h = awra_font_line_height(&l->font);
  }
  l->wf = w;
  /* GtkLabel measures in application pixels: at a whole scale factor its
   * size is a whole number of them (pango_layout_get_pixel_size's ceil) */
  const float sc = awra_scale();
  if (sc > 1.0f && sc == floorf(sc)) {
    l->w = (int)(ceilf(w / sc - 1e-3f) * sc);
    l->h = (int)(ceilf(h / sc - 1e-3f) * sc);
  }
  else {
    l->w = (int)ceilf(w);
    l->h = (int)ceilf(h);
  }
  l->baseline = l->n_lines ? (int)floorf(l->lines[0].ascent + 0.5f) : 0;
}

void awra_text_layout_size(AwraTextLayout *l, int width, int *r_w, int *r_h)
{
  layout(l, width);
  if (r_w) {
    *r_w = l->w;
  }
  if (r_h) {
    *r_h = l->h;
  }
}

/* the widest line's width laid out at `width` (the layout is left laid
 * out there) */
static int width_at(AwraTextLayout *l, int width)
{
  layout(l, width);
  return l->w;
}

int awra_text_layout_natural_width(AwraTextLayout *l)
{
  if (l->nat_w < 0) {
    l->nat_w = width_at(l, -1);
  }
  return l->nat_w;
}

/* GtkLabel's minimum width: a wrapping layout laid out at width 0 (a line
 * per unbreakable stretch: the longest word, or with PANGO_WRAP_CHAR /
 * WORD_CHAR the widest grapheme); an ellipsizing one, its ellipsis but no
 * wider than its natural width */
int awra_text_layout_min_width(AwraTextLayout *l)
{
  if (l->min_w < 0) {
    if (l->wrap == AWRA_WRAP_NONE && l->ellipsize == AWRA_ELLIPSIZE_NONE) {
      l->min_w = awra_text_layout_natural_width(l);
    }
    else if (l->wrap == AWRA_WRAP_NONE || l->ellipsize != AWRA_ELLIPSIZE_NONE) {
      AwraFont f = span_font(l, l->n_spans ? 0 : -1);
      l->min_w = (int)ceilf(awra_text_width(&f, ELLIPSIS, 3));
      if (l->wrap != AWRA_WRAP_NONE) {
        l->min_w = BK_MIN(l->min_w, awra_text_layout_natural_width(l));
      }
    }
    else {
      l->min_w = width_at(l, 0);
    }
  }
  return l->min_w;
}

int awra_text_layout_baseline(AwraTextLayout *l)
{
  if (l->laid_width == -2) {
    layout(l, -1);
  }
  return l->baseline;
}

/* A line's x in the layout: aligned as Pango aligns it (the alignment of a
 * right-to-left paragraph mirrored: its lines start at the right), inside
 * the width the layout is laid out at when it wraps or ellipsizes (Pango's
 * layout width), else inside its widest line. */
static float line_x(const AwraTextLayout *l, const Line *ln, int width)
{
  const bool in_width = width >= 0 && (l->wrap != AWRA_WRAP_NONE || l->ellipsize != AWRA_ELLIPSIZE_NONE);
  const float avail = in_width ? (float)width : l->wf;
  AwraJustify j = l->justify == AWRA_JUSTIFY_FILL ? AWRA_JUSTIFY_LEFT : l->justify;
  if (ln->rtl && j != AWRA_JUSTIFY_CENTER) {
    j = j == AWRA_JUSTIFY_LEFT ? AWRA_JUSTIFY_RIGHT : AWRA_JUSTIFY_LEFT;
  }
  if (j == AWRA_JUSTIFY_CENTER) {
    /* Pango halves in its units: whole pixels with hinted metrics, else
     * where it falls (GSK draws it there) */
    return BUI_text_hint_metrics() ? floorf((avail - ln->w) * 0.5f + 0.5f) : (avail - ln->w) * 0.5f;
  }
  if (j == AWRA_JUSTIFY_RIGHT) {
    return avail - ln->w;
  }
  return 0.0f;
}

void awra_text_layout_logical(AwraTextLayout *l, int width, float *r_x, float *r_w)
{
  layout(l, width);
  float x0 = 1e9f, x1 = -1e9f;
  for (int i = 0; i < l->n_lines; i++) {
    const float lx = line_x(l, &l->lines[i], width);
    x0 = fminf(x0, lx);
    x1 = fmaxf(x1, lx + l->lines[i].w);
  }
  if (l->n_lines == 0) {
    x0 = x1 = 0.0f;
  }
  *r_x = x0;
  *r_w = x1 - x0;
}

void awra_text_layout_draw(AwraTextLayout *l, AwraDraw *d, float x, float y, int width,
                           const AwraRGBA *color)
{
  layout(l, width);
  float ly = y;
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    const float lx = x + line_x(l, ln, width);
    const float base = floorf(ly + ln->ascent + 0.5f);
    /* backgrounds first (Pango's background attribute: the run's logical box) */
    for (int k = 0; k < ln->n_runs; k++) {
      const Run *r = &l->runs[ln->first_run + k];
      if (r->span >= 0 && l->spans[r->span].has_bg) {
        AwraRGBA bg = l->spans[r->span].bg;
        awra_paint_rect(d, (AwraBox){lx + r->x, ly, r->w, ln->height}, &bg);
      }
    }
    for (int k = 0; k < ln->n_runs; k++) {
      const Run *r = &l->runs[ln->first_run + k];
      AwraFont f = span_font(l, r->span);
      AwraRGBA c = *color;
      if (r->span >= 0) {
        const Span *s = &l->spans[r->span];
        if (s->link >= 0 && l->link_style) {
          c = s->link == l->hover_link ? l->link_hover_color : l->link_color;
        }
        if (s->has_color) {
          c = s->color;
        }
        if (s->alpha >= 0.0f) {
          c.a *= s->alpha;
        }
      }
      if ((r->start >= 0 && (is_object(l, r->start) || is_tab(l, r->start))) || r->zero) {
        continue;
      }
      if (r->start < 0) {
        draw_text_x(d, &f, 0, lx + r->x, base, ELLIPSIS, 3, &c);
      }
      else {
        run_context(l, &f, r->shp_start, r->shp_end, r->hyph);
        if (r->shp_start != r->start || r->shp_end != r->end) {
          /* a part kept by ellipsizing: from the whole run's shaping */
          draw_text_part_x(d, &f, level_flags(l, r->level), lx + r->x - r->sx - r->lsl, base,
                           l->text + r->shp_start, r->shp_end - r->shp_start,
                           r->start - r->shp_start, r->end - r->shp_start, &c);
        }
        else {
          draw_text_x(d, &f, level_flags(l, r->level), lx + r->x - r->lsl, base,
                      l->text + r->start, r->end - r->start, &c);
        }
        BUI_text_set_context(NULL);
      }
      if (r->span >= 0 && (l->spans[r->span].underline || l->spans[r->span].strike)) {
        /* Pango's renderer: a rectangle from the font's metrics, its top
         * `position` above the line's own (unrounded) baseline */
        float up, ut, sp, st;
        BUI_text_decoration_metrics(f.family, font_flags(&f), f.size, &up, &ut, &sp, &st);
        const float pango_base = ly + ln->ascent;
        if (l->spans[r->span].underline) {
          awra_paint_rect(d, (AwraBox){lx + r->x, pango_base - up, r->w, ut}, &c);
        }
        if (l->spans[r->span].strike) {
          awra_paint_rect(d, (AwraBox){lx + r->x, pango_base - sp, r->w, st}, &c);
        }
      }
    }
    ly += ln->height;
  }
}

/* typography roles' line factors are applied by labels */
float awra_typography_line_factor(AwraTypography t);
float awra_typography_line_factor(AwraTypography t)
{
  return role_line_height(t);
}

/* ----------------------------------------------------------------------- */
/* Positions (editing): byte index <-> point, relative to the layout origin,
 * as Pango places and moves cursors (pango_layout_get_cursor_pos,
 * pango_layout_xy_to_index, pango_layout_move_cursor_visually) */

/* x (from the line's left edge) of the leading or trailing edge of the
 * character at `index` in run `r` */
static float run_index_x(AwraTextLayout *l, const Run *r, int index, bool trailing)
{
  if (r->zero) {
    return r->x;
  }
  if (r->start < 0 || is_object(l, r->start) || is_tab(l, r->start)) {
    const float frac = trailing ? 1.0f : 0.0f;
    return (r->level & 1) ? r->x + r->w * (1.0f - frac) : r->x + r->w * frac;
  }
  AwraFont f = span_font(l, r->span);
  float x = 0.0f;
  run_context(l, &f, r->shp_start, r->shp_end, r->hyph);
  BUI_text_index_to_x(f.family, font_flags(&f) | level_flags(l, r->level), f.size,
                      l->text + r->shp_start, r->shp_end - r->shp_start, index - r->shp_start,
                      trailing, &x);
  BUI_text_set_context(NULL);
  return r->x + x - r->sx - r->lsl;
}

/* the line holding byte `index` (a boundary between two lines is the
 * second's start, as Pango has it) */
static int line_of(const AwraTextLayout *l, int index)
{
  int found = 0;
  for (int i = 0; i < l->n_lines; i++) {
    if (l->lines[i].start > index) {
      break;
    }
    found = i;
  }
  return found;
}

/* pango_layout_line_index_to_x: an index outside the line's runs is in its
 * trailing spaces (the line's logical end) or in ellipsized text (the
 * ellipsis) */
static float line_index_x(AwraTextLayout *l, const Line *ln, int index, bool trailing)
{
  const Run *ell = NULL;
  for (int k = 0; k < ln->n_runs; k++) {
    const Run *r = &l->runs[ln->first_run + k];
    if (r->start < 0) {
      ell = r;
    }
    else if (r->start <= index && index < r->end) {
      return run_index_x(l, r, index, trailing);
    }
  }
  if (ell && index < ln->end) {
    return run_index_x(l, ell, 0, false);
  }
  return ln->rtl ? 0.0f : ln->w;
}

/* pango_layout_get_cursor_pos within line `li`: the strong and weak cursor */
static void cursor_x(AwraTextLayout *l, int li, int index, float *r_strong, float *r_weak)
{
  const Line *ln = &l->lines[li];
  index = BK_CLAMP(index, ln->start, ln->end);
  int level1, level2;
  float x1, x2;
  if (index <= ln->start) {
    level1 = ln->rtl ? 1 : 0;
    x1 = ln->rtl ? ln->w : 0.0f;
  }
  else {
    int prev = index - 1;
    while (prev > ln->start && ((unsigned char)l->text[prev] & 0xC0) == 0x80) {
      prev--;
    }
    level1 = level_at(l, prev);
    x1 = line_index_x(l, ln, prev, true);
  }
  if (index >= ln->end) {
    level2 = ln->rtl ? 1 : 0;
    x2 = ln->rtl ? 0.0f : ln->w;
  }
  else {
    level2 = level_at(l, index);
    x2 = line_index_x(l, ln, index, false);
  }
  const bool dir1 = level1 & 1, dir2 = level2 & 1;
  const bool first = dir1 == ln->rtl && (dir2 != dir1 || level1 < level2);
  if (r_strong) {
    *r_strong = first ? x1 : x2;
  }
  if (r_weak) {
    *r_weak = first ? x2 : x1;
  }
}

void awra_text_layout_cursor_pos(AwraTextLayout *l, int width, int index, float *r_strong,
                                 float *r_weak, float *r_y, float *r_h)
{
  layout(l, width);
  index = BK_CLAMP(index, 0, l->len);
  float y = 0.0f;
  if (l->n_lines == 0) {
    *r_strong = *r_weak = *r_y = 0.0f;
    *r_h = awra_font_line_height(&l->font);
    return;
  }
  const int li = line_of(l, index);
  for (int i = 0; i < li; i++) {
    y += l->lines[i].height;
  }
  const float lx = line_x(l, &l->lines[li], width);
  float s, w;
  cursor_x(l, li, index, &s, &w);
  *r_strong = lx + s;
  *r_weak = lx + w;
  *r_y = y;
  *r_h = l->lines[li].height;
}

void awra_text_layout_index_to_pos(AwraTextLayout *l, int width, int index, float *r_x,
                                   float *r_y, float *r_h)
{
  float weak;
  awra_text_layout_cursor_pos(l, width, index, r_x, &weak, r_y, r_h);
}

/* pango_layout_line_x_to_index: the character at x in line `li` and 0, or
 * the characters of its grapheme when x is in its trailing half */
static int line_x_to_index(AwraTextLayout *l, int li, float x, int *r_trailing)
{
  const Line *ln = &l->lines[li];
  *r_trailing = 0;
  if (ln->end <= ln->start) {
    return ln->start;
  }
  const int last = BK_MAX(ln->start, prev_cursor(l, ln->end));
  const int last_trailing = utf8_count(l->text + last, ln->end - last);
  /* a wrapped line's end is the next line's start: never put the cursor
   * there from this line (Pango's suppress_last_trailing) */
  const bool suppress = li + 1 < l->n_lines && l->lines[li + 1].start == ln->end;
  if (x < 0.0f) {
    if (!ln->rtl) {
      return ln->start;
    }
    *r_trailing = suppress ? 0 : last_trailing;
    return last;
  }
  for (int k = 0; k < ln->n_runs; k++) {
    const Run *r = &l->runs[ln->first_run + k];
    if (!(x >= r->x && x < r->x + r->w)) {
      continue;
    }
    const float rel = x - r->x;
    const bool second_half = (r->level & 1) ? rel < r->w * 0.5f : rel >= r->w * 0.5f;
    if (r->start < 0) {
      /* the ellipsis: the ellipsized text's start or end */
      const int gs = k > 0 ? l->runs[ln->first_run + k - 1].end : ln->start;
      const int ge = k + 1 < ln->n_runs ? l->runs[ln->first_run + k + 1].start : ln->end;
      return second_half ? ge : gs;
    }
    if (is_object(l, r->start)) {
      *r_trailing = second_half && !(suppress && r->end >= ln->end) ? 1 : 0;
      return r->start;
    }
    AwraFont f = span_font(l, r->span);
    int idx = 0, tr = 0;
    run_context(l, &f, r->shp_start, r->shp_end, r->hyph);
    BUI_text_x_to_index(f.family, font_flags(&f) | level_flags(l, r->level), f.size,
                        l->text + r->shp_start, r->shp_end - r->shp_start, rel + r->sx + r->lsl, &idx,
                        &tr);
    BUI_text_set_context(NULL);
    int index = BK_CLAMP(r->shp_start + idx, r->start, r->end);
    if (tr && suppress) {
      int e = index;
      for (int c = 0; c < tr && e < l->len; c++) {
        e += utf8_len((unsigned char)l->text[e]);
      }
      if (e >= ln->end) {
        tr = 0;
      }
    }
    *r_trailing = tr;
    return index;
  }
  if (!ln->rtl) {
    *r_trailing = suppress ? 0 : last_trailing;
    return last;
  }
  return ln->start;
}

void awra_text_layout_char_rect(AwraTextLayout *l, int width, int index, float r[4])
{
  layout(l, width);
  index = BK_CLAMP(index, 0, l->len);
  if (l->n_lines == 0) {
    r[0] = r[1] = r[2] = 0.0f;
    r[3] = awra_font_line_height(&l->font);
    return;
  }
  const int li = line_of(l, index);
  float y = 0.0f;
  for (int i = 0; i < li; i++) {
    y += l->lines[i].height;
  }
  const Line *ln = &l->lines[li];
  const float x0 = line_index_x(l, ln, index, false);
  const float x1 = index < ln->end ? line_index_x(l, ln, index, true) : x0;
  r[0] = line_x(l, ln, width) + fminf(x0, x1);
  r[1] = y;
  r[2] = fabsf(x1 - x0);
  r[3] = ln->height;
}

int awra_text_layout_char_at(AwraTextLayout *l, int width, float px, float py)
{
  layout(l, width);
  if (l->n_lines == 0) {
    return 0;
  }
  float y = 0.0f;
  int li = l->n_lines - 1;
  for (int i = 0; i < l->n_lines; i++) {
    if (py < y + l->lines[i].height) {
      li = i;
      break;
    }
    y += l->lines[i].height;
  }
  int trailing = 0;
  return line_x_to_index(l, li, px - line_x(l, &l->lines[li], width), &trailing);
}

int awra_text_layout_pos_to_index(AwraTextLayout *l, int width, float px, float py)
{
  layout(l, width);
  if (l->n_lines == 0) {
    return 0;
  }
  float y = 0.0f;
  int li = l->n_lines - 1;
  for (int i = 0; i < l->n_lines; i++) {
    if (py < y + l->lines[i].height) {
      li = i;
      break;
    }
    y += l->lines[i].height;
  }
  int trailing = 0;
  int index = line_x_to_index(l, li, px - line_x(l, &l->lines[li], width), &trailing);
  while (trailing-- > 0 && index < l->len) {
    index += utf8_len((unsigned char)l->text[index]);
  }
  return index;
}

typedef struct CursorStop {
  int pos;
  float x;
} CursorStop;

static int cmp_stop(const void *a, const void *b)
{
  const float xa = ((const CursorStop *)a)->x, xb = ((const CursorStop *)b)->x;
  return xa < xb ? -1 : xa > xb ? 1 : 0;
}

/* pango_layout_line_get_cursors (strong): the line's cursor positions by x
 * (its end only when it is this line's, not the next one's start) */
static int line_cursors(AwraTextLayout *l, int li, CursorStop **r_stops)
{
  const Line *ln = &l->lines[li];
  const bool own_end = !(li + 1 < l->n_lines && l->lines[li + 1].start == ln->end);
  const int cap = (ln->end - ln->start) + 2;
  CursorStop *c = (CursorStop *)malloc(sizeof(CursorStop) * (size_t)cap);
  int n = 0;
  if (!c) {
    *r_stops = NULL;
    return 0;
  }
  for (int i = ln->start; i < ln->end || (own_end && i == ln->end);) {
    if (is_cursor(l, i)) {
      c[n].pos = i;
      cursor_x(l, li, i, &c[n].x, NULL);
      n++;
    }
    if (i >= ln->end) {
      break;
    }
    i += utf8_len((unsigned char)l->text[i]);
  }
  /* a stable sort by x (insertion: lines are short) */
  for (int i = 1; i < n; i++) {
    const CursorStop t = c[i];
    int j = i;
    while (j > 0 && cmp_stop(&c[j - 1], &t) > 0) {
      c[j] = c[j - 1];
      j--;
    }
    c[j] = t;
  }
  *r_stops = c;
  return n;
}

int awra_text_layout_move_visually(AwraTextLayout *l, int width, int index, int dir)
{
  layout(l, width);
  if (l->n_lines == 0) {
    return index;
  }
  dir = dir >= 0 ? 1 : -1;
  index = BK_CLAMP(index, 0, l->len);
  int li = line_of(l, index);
  const Line *ln = &l->lines[li];
  if (index > ln->end) {
    index = ln->end;
  }
  CursorStop *c = NULL;
  int n = line_cursors(l, li, &c);
  float px;
  cursor_x(l, li, index, &px, NULL);
  int vis = -1;
  for (int j = 0; j < n; j++) {
    if (c[j].x == px) {
      vis = j;
      if (dir < 0) {
        break;
      }
    }
  }
  if (vis == -1 && index == ln->end) {
    vis = ln->rtl ? 0 : n;
  }
  bool off_start, off_end;
  if (!ln->rtl) {
    off_start = index == ln->start && dir < 0;
    off_end = index == ln->end && dir > 0;
  }
  else {
    off_end = index == ln->end && dir < 0;
    off_start = index == ln->start && dir > 0;
  }
  if (off_start || off_end) {
    bool para;
    if (off_start) {
      if (li == 0) {
        free(c);
        return 0; /* off the layout's start */
      }
      li--;
      para = l->lines[li].end != index;
    }
    else {
      if (li == l->n_lines - 1) {
        free(c);
        return index; /* off its end: stays */
      }
      li++;
      para = l->lines[li].start != index;
    }
    ln = &l->lines[li];
    free(c);
    n = line_cursors(l, li, &c);
    if (off_start && dir < 0) {
      vis = n + (para ? 1 : 0);
    }
    else if (off_end && dir > 0) {
      vis = para ? -1 : 0;
    }
  }
  vis += dir;
  int out;
  if (vis >= 0 && vis < n) {
    out = c[vis].pos;
  }
  else if (vis >= n - 1) {
    out = ln->end;
  }
  else {
    out = ln->start;
  }
  free(c);
  return out;
}

int awra_text_layout_cursor_step(AwraTextLayout *l, int index, int dir)
{
  index = BK_CLAMP(index, 0, l->len);
  return dir > 0 ? next_cursor(l, index) : prev_cursor(l, index);
}

/* ----------------------------------------------------------------------- */
/* Links, inline objects, selections */

int awra_text_layout_n_links(const AwraTextLayout *l)
{
  return l->n_links;
}

const char *awra_text_layout_link(const AwraTextLayout *l, int index)
{
  return index >= 0 && index < l->n_links ? l->links[index] : NULL;
}

void awra_text_layout_set_link_style(AwraTextLayout *l, int hover, const AwraRGBA *color,
                                     const AwraRGBA *hover_color)
{
  l->hover_link = hover;
  l->link_style = color != NULL;
  if (color) {
    l->link_color = *color;
    l->link_hover_color = hover_color ? *hover_color : *color;
  }
}

/* The run under a point of the layout (origin at its top-left), -1 none. */
static int run_at(AwraTextLayout *l, int width, float px, float py)
{
  layout(l, width);
  float y = 0.0f;
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    if (py >= y && py < y + ln->height) {
      const float lx = line_x(l, ln, width);
      for (int k = 0; k < ln->n_runs; k++) {
        const Run *r = &l->runs[ln->first_run + k];
        if (px >= lx + r->x && px < lx + r->x + r->w) {
          return ln->first_run + k;
        }
      }
      return -1;
    }
    y += ln->height;
  }
  return -1;
}

int awra_text_layout_link_at(AwraTextLayout *l, int width, float px, float py)
{
  if (!l->n_links) {
    return -1;
  }
  const int r = run_at(l, width, px, py);
  if (r < 0 || l->runs[r].span < 0) {
    return -1;
  }
  return l->spans[l->runs[r].span].link;
}

void awra_text_layout_set_object_size(AwraTextLayout *l, float size)
{
  if (l->object_size != size) {
    l->object_size = size;
    dirty(l);
  }
}

/* The inline objects laid out at `width`: calls fn with each one's index
 * (in text order) and the top-left of its square, layout coordinates. */
void awra_text_layout_foreach_object(AwraTextLayout *l, int width,
                                     void (*fn)(int index, float x, float y, void *data), void *data)
{
  if (l->object_size <= 0.0f) {
    return;
  }
  layout(l, width);
  int index = 0;
  /* objects before the laid-out runs (cut by max lines) are skipped, but
   * counted, so indexes follow the text */
  float y = 0.0f;
  int next = 0;
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    const float lx = line_x(l, ln, width);
    const float base = floorf(y + ln->ascent + 0.5f);
    for (int k = 0; k < ln->n_runs; k++) {
      const Run *r = &l->runs[ln->first_run + k];
      if (r->start < 0 || !is_object(l, r->start)) {
        continue;
      }
      /* count the objects of the text skipped before this run */
      for (const char *o = strstr(l->text + next, OBJECT_CHAR); o && o - l->text < r->start;
           o = strstr(o + 3, OBJECT_CHAR))
      {
        index++;
      }
      for (int j = r->start; j + 2 < r->end + 1 && j < r->end; j += 3) {
        fn(index++, lx + r->x + (j - r->start) / 3 * l->object_size, base - l->object_size * 0.8f, data);
      }
      next = r->end;
    }
    y += ln->height;
  }
}

/* Fills the selection [start, end) (byte offsets) behind the text, as GTK
 * paints it: per line, each run's part of the range
 * (pango_layout_line_get_x_ranges), and where the selection goes on before
 * or after the line, the line's ends out to the edge of an area `area_w`
 * wide (Pango's ranges: the layout's width; GtkTextView: the view's).
 * `merge` (GtkText): one box around all of it. */
static void draw_selection(AwraTextLayout *l, AwraDraw *d, float x, float y, int width, int start,
                           int end, const AwraRGBA *color, float area_w, bool merge)
{
  if (start > end) {
    const int t = start;
    start = end;
    end = t;
  }
  if (start == end) {
    return;
  }
  layout(l, width);
  const bool view = area_w >= 0.0f;
  if (!view) {
    /* Pango's right end reaches the layout's width: none without one */
    const bool in_width = width >= 0 && (l->wrap != AWRA_WRAP_NONE || l->ellipsize != AWRA_ELLIPSIZE_NONE);
    area_w = in_width ? (float)width : -1.0f;
  }
  float ly = y, mx0 = 1e9f, mx1 = -1e9f, my0 = 1e9f, my1 = -1e9f;
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    const bool para_end = ln->end >= l->len || l->text[ln->end] == '\n';
    /* the lines GDK's clip region (GtkTextView: its snapshot) takes */
    const bool take = view ? (start < ln->end || (start == ln->end && para_end)) && end > ln->start :
                             end >= ln->start && start < ln->end;
    if (!take) {
      ly += ln->height;
      continue;
    }
    const bool before = start < ln->start, after = end > ln->end;
    const float lxo = line_x(l, ln, width), lx = x + lxo;
    float rx[2 * 64];
    int nr = 0;
    if (lxo > 0.0f && ((!ln->rtl && before) || (ln->rtl && after))) {
      rx[nr++] = x;
      rx[nr++] = lx;
    }
    for (int k = 0; k < ln->n_runs; k++) {
      const Run *r = &l->runs[ln->first_run + k];
      if (r->start < 0 || r->zero || r->end <= start || r->start >= end) {
        continue;
      }
      const int a = BK_MAX(start, r->start), b = BK_MIN(end, r->end);
      int pb = b - 1; /* the trailing edge of the last character in range */
      while (pb > a && ((unsigned char)l->text[pb] & 0xC0) == 0x80) {
        pb--;
      }
      const float xa = run_index_x(l, r, a, false), xb = run_index_x(l, r, pb, true);
      if (nr + 2 <= (int)(sizeof(rx) / sizeof(rx[0]))) {
        rx[nr++] = lx + fminf(xa, xb);
        rx[nr++] = lx + fmaxf(xa, xb);
      }
    }
    if (area_w >= 0.0f && lxo + ln->w < area_w && ((!ln->rtl && after) || (ln->rtl && before)) &&
        nr + 2 <= (int)(sizeof(rx) / sizeof(rx[0])))
    {
      rx[nr++] = lx + ln->w;
      rx[nr++] = x + area_w;
    }
    for (int k = 0; k < nr; k += 2) {
      if (rx[k + 1] <= rx[k]) {
        continue;
      }
      if (merge) {
        mx0 = fminf(mx0, rx[k]);
        mx1 = fmaxf(mx1, rx[k + 1]);
        my0 = fminf(my0, ly);
        my1 = fmaxf(my1, ly + ln->height);
      }
      else {
        awra_paint_rect(d, (AwraBox){rx[k], ly, rx[k + 1] - rx[k], ln->height}, color);
      }
    }
    ly += ln->height;
  }
  if (merge && mx1 > mx0) {
    awra_paint_rect(d, (AwraBox){mx0, my0, mx1 - mx0, my1 - my0}, color);
  }
}

void awra_text_layout_draw_selection(AwraTextLayout *l, AwraDraw *d, float x, float y, int width,
                                     int start, int end, const AwraRGBA *color)
{
  draw_selection(l, d, x, y, width, start, end, color, -1.0f, false);
}

void awra_text_layout_draw_selection_view(AwraTextLayout *l, AwraDraw *d, float x, float y,
                                          int width, int start, int end, const AwraRGBA *color,
                                          float area_w)
{
  draw_selection(l, d, x, y, width, start, end, color, area_w, false);
}

void awra_text_layout_draw_selection_box(AwraTextLayout *l, AwraDraw *d, float x, float y, int width,
                                         int start, int end, const AwraRGBA *color)
{
  draw_selection(l, d, x, y, width, start, end, color, -1.0f, true);
}

/* The word around byte `index` (letters, digits and marks). */
void awra_text_layout_word_at(const AwraTextLayout *l, int index, int *r_start, int *r_end)
{
  int a = BK_CLAMP(index, 0, l->len), b = a;
#define WORDCH(c) ((unsigned char)(c) >= 0x80 || ((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || (c) == '_')
  while (a > 0 && WORDCH(l->text[a - 1])) {
    a--;
  }
  while (b < l->len && WORDCH(l->text[b])) {
    b++;
  }
#undef WORDCH
  *r_start = a;
  *r_end = b;
}
