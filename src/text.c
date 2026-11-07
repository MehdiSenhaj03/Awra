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

float awra_font_line_height(const AwraFont *f)
{
  float a, d;
  BUI_text_metrics(f->family, font_flags(f), f->size, &a, &d);
  return a + d;
}

static float plain_width(const AwraFont *f, const char *s, int len)
{
  float w = 0.0f;
  if (len <= 0) {
    return 0.0f;
  }
  BUI_text_measure(f->family, font_flags(f), f->size, s, len, &w);
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

float awra_text_width(const AwraFont *f, const char *s, int len)
{
  if (len < 0) {
    len = (int)strlen(s);
  }
  float w = plain_width(f, s, len);
  if (f->letter_spacing != 0.0f) {
    w += f->letter_spacing * utf8_count(s, len);
  }
  return w;
}

void awra_draw_text(AwraDraw *d, const AwraFont *f, float x, float baseline, const char *s,
                    int len, const AwraRGBA *color)
{
  if (len < 0) {
    len = (int)strlen(s);
  }
  if (len <= 0 || !color) {
    return;
  }
  float c[4];
  awra_rgba_f(color, c);
  c[3] *= d->opacity;
  if (c[3] <= 0.0f) {
    return;
  }
  BUI_raster_set_font(d->r, f->family, font_flags(f));
  if (f->letter_spacing == 0.0f) {
    BUI_raster_text(d->r, x, baseline, s, len, f->size, c);
    return;
  }
  for (int i = 0; i < len;) {
    const int n = utf8_len((unsigned char)s[i]);
    BUI_raster_text(d->r, x, baseline, s + i, n, f->size, c);
    x += plain_width(f, s + i, n) + f->letter_spacing;
    i += n;
  }
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
  int start, end;
  int span;
  float x, w;
} Run;

typedef struct Line {
  int first_run, n_runs;
  float w;
  float ascent, height;
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
};

#define OBJECT_CHAR "\xEF\xBF\xBC"

static bool is_object(const AwraTextLayout *l, int i)
{
  return l->object_size > 0.0f && i + 3 <= l->len && (unsigned char)l->text[i] == 0xEF &&
         (unsigned char)l->text[i + 1] == 0xBF && (unsigned char)l->text[i + 2] == 0xBC;
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
  free(l);
}

static void dirty(AwraTextLayout *l)
{
  l->laid_width = -2;
  l->min_w = l->nat_w = -1;
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
  dirty(l);
}

void awra_text_layout_set_font(AwraTextLayout *l, const AwraFont *f)
{
  if (l->font.family == f->family && l->font.size == f->size && l->font.weight == f->weight &&
      l->font.italic == f->italic && l->font.letter_spacing == f->letter_spacing)
  {
    return;
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

/* the innermost span covering byte i (spans nest: the last one opened wins) */
static int span_at(const AwraTextLayout *l, int i)
{
  int found = -1;
  for (int k = 0; k < l->n_spans; k++) {
    if (l->spans[k].start <= i && i < l->spans[k].end) {
      if (found < 0 || l->spans[k].start >= l->spans[found].start) {
        found = k;
      }
    }
  }
  return found;
}

/* end of the uniform-style segment starting at i */
static int segment_end(const AwraTextLayout *l, int i, int limit)
{
  int end = limit;
  if (l->object_size > 0.0f) {
    if (is_object(l, i)) {
      return BK_MIN(limit, i + 3);
    }
    const char *o = strstr(l->text + i, OBJECT_CHAR);
    if (o && o - l->text < end) {
      end = (int)(o - l->text);
    }
  }
  if (!l->n_spans) {
    return end;
  }
  for (int k = 0; k < l->n_spans; k++) {
    if (l->spans[k].start > i && l->spans[k].start < end) {
      end = l->spans[k].start;
    }
    if (l->spans[k].end > i && l->spans[k].end < end) {
      end = l->spans[k].end;
    }
  }
  return end;
}

/* width of a uniform segment [a, b): text, or inline objects */
static float seg_width(const AwraTextLayout *l, const AwraFont *f, int a, int b)
{
  if (b > a && is_object(l, a)) {
    return l->object_size * (float)((b - a + 2) / 3);
  }
  return awra_text_width(f, l->text + a, b - a);
}

static float range_width(const AwraTextLayout *l, int a, int b)
{
  float w = 0.0f;
  while (a < b) {
    const int e = segment_end(l, a, b);
    const int sp = span_at(l, a);
    AwraFont f = span_font(l, sp);
    w += seg_width(l, &f, a, e);
    a = e;
  }
  return w;
}

static void add_run(AwraTextLayout *l, int a, int b, float x)
{
  while (a < b) {
    const int e = segment_end(l, a, b);
    if (l->n_runs == l->cap_runs) {
      l->cap_runs = l->cap_runs ? l->cap_runs * 2 : 16;
      l->runs = (Run *)realloc(l->runs, sizeof(Run) * (size_t)l->cap_runs);
    }
    Run *r = &l->runs[l->n_runs++];
    r->start = a;
    r->end = e;
    r->span = span_at(l, a);
    AwraFont f = span_font(l, r->span);
    r->x = x;
    r->w = seg_width(l, &f, a, e);
    x += r->w;
    a = e;
  }
}

static void line_metrics(AwraTextLayout *l, Line *ln, bool para_start)
{
  float asc = awra_font_ascent(&l->font), h = awra_font_line_height(&l->font);
  for (int i = 0; i < ln->n_runs; i++) {
    const Run *r = &l->runs[ln->first_run + i];
    if (r->span >= 0 && l->spans[r->span].scale != 1.0f) {
      AwraFont f = span_font(l, r->span);
      asc = fmaxf(asc, awra_font_ascent(&f));
      h = fmaxf(h, awra_font_line_height(&f));
    }
  }
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

/* Lays out a line [a, b) ellipsized to `width` (END / START / MIDDLE). */
static void ellipsize_line(AwraTextLayout *l, Line *ln, int a, int b, float width)
{
  AwraFont f0 = span_font(l, span_at(l, a));
  const float ew = awra_text_width(&f0, ELLIPSIS, 3);
  const float avail = width - ew;
  int cut = a;
  if (l->ellipsize == AWRA_ELLIPSIZE_START) {
    /* keep the longest tail that fits */
    int start = b;
    for (int i = b; i > a;) {
      int j = i - 1;
      while (j > a && ((unsigned char)l->text[j] & 0xC0) == 0x80) {
        j--;
      }
      if (range_width(l, j, b) > avail) {
        break;
      }
      start = j;
      i = j;
    }
    free(l->ell_text);
    l->ell_text = NULL;
    add_run(l, a, a, 0); /* nothing */
    /* the ellipsis then the tail: draw the ellipsis as a run of its own */
    const int er = l->n_runs;
    if (l->n_runs == l->cap_runs) {
      l->cap_runs = l->cap_runs ? l->cap_runs * 2 : 16;
      l->runs = (Run *)realloc(l->runs, sizeof(Run) * (size_t)l->cap_runs);
    }
    l->runs[er] = (Run){-1, -1, span_at(l, a), 0, ew};
    l->n_runs++;
    add_run(l, start, b, ew);
    ln->n_runs = l->n_runs - ln->first_run;
    ln->w = ew + range_width(l, start, b);
    return;
  }
  if (l->ellipsize == AWRA_ELLIPSIZE_MIDDLE) {
    int head = a, tail = b;
    float used = 0.0f;
    for (bool front = true;; front = !front) {
      if (front) {
        const int n = utf8_len((unsigned char)l->text[head]);
        const float cw = range_width(l, head, head + n);
        if (head >= tail || used + cw > avail) {
          break;
        }
        used += cw;
        head += n;
      }
      else {
        int j = tail - 1;
        while (j > head && ((unsigned char)l->text[j] & 0xC0) == 0x80) {
          j--;
        }
        const float cw = range_width(l, j, tail);
        if (j < head || used + cw > avail) {
          break;
        }
        used += cw;
        tail = j;
      }
    }
    add_run(l, a, head, 0);
    const float hw = range_width(l, a, head);
    if (l->n_runs == l->cap_runs) {
      l->cap_runs = l->cap_runs ? l->cap_runs * 2 : 16;
      l->runs = (Run *)realloc(l->runs, sizeof(Run) * (size_t)l->cap_runs);
    }
    l->runs[l->n_runs++] = (Run){-1, -1, span_at(l, a), hw, ew};
    add_run(l, tail, b, hw + ew);
    ln->n_runs = l->n_runs - ln->first_run;
    ln->w = hw + ew + range_width(l, tail, b);
    return;
  }
  /* END: the longest head that fits */
  float used = 0.0f;
  for (int i = a; i < b;) {
    const int n = utf8_len((unsigned char)l->text[i]);
    const float cw = range_width(l, i, i + n);
    if (used + cw > avail) {
      break;
    }
    used += cw;
    i += n;
    cut = i;
  }
  /* no trailing space before the ellipsis */
  while (cut > a && l->text[cut - 1] == ' ') {
    cut--;
    used = range_width(l, a, cut);
  }
  add_run(l, a, cut, 0);
  if (l->n_runs == l->cap_runs) {
    l->cap_runs = l->cap_runs ? l->cap_runs * 2 : 16;
    l->runs = (Run *)realloc(l->runs, sizeof(Run) * (size_t)l->cap_runs);
  }
  l->runs[l->n_runs++] = (Run){-1, -1, span_at(l, cut > a ? cut - 1 : a), used, ew};
  ln->n_runs = l->n_runs - ln->first_run;
  ln->w = used + ew;
}

static void layout(AwraTextLayout *l, int width)
{
  if (l->laid_width == width) {
    return;
  }
  l->laid_width = width;
  l->n_lines = 0;
  l->n_runs = 0;
  const float W = width < 0 ? 1e9f : (float)width;
  const bool wrap = l->wrap != AWRA_WRAP_NONE && width >= 0;
  const int max_lines = l->max_lines > 0 ? l->max_lines : (1 << 30);
  int pos = 0;
  while (pos <= l->len) {
    int para_end = pos;
    while (para_end < l->len && l->text[para_end] != '\n') {
      para_end++;
    }
    int a = pos;
    do {
      const bool para_start = a == pos;
      Line *ln = push_line(l);
      const bool last_allowed = l->n_lines >= max_lines;
      const float full = range_width(l, a, para_end);
      if (full <= W + 0.01f) {
        add_run(l, a, para_end, 0);
        ln->n_runs = l->n_runs - ln->first_run;
        ln->w = full;
        a = para_end;
      }
      else if (!wrap || last_allowed) {
        if (l->ellipsize != AWRA_ELLIPSIZE_NONE) {
          ellipsize_line(l, ln, a, para_end, W);
        }
        else {
          add_run(l, a, para_end, 0);
          ln->n_runs = l->n_runs - ln->first_run;
          ln->w = full;
        }
        a = para_end;
      }
      else {
        /* the last break opportunity whose line fits, lines measured whole
         * (kerning, whole-pixel positions) as Pango measures them */
        int brk = -1, j = a;
        while (j < para_end) {
          int k = j;
          while (k < para_end) {
            k += utf8_len((unsigned char)l->text[k]);
            const char c = l->text[k - 1];
            if (c == ' ' || c == '-' || c == '/') {
              break;
            }
          }
          int ve = k;
          while (ve > a && l->text[ve - 1] == ' ') {
            ve--;
          }
          if (range_width(l, a, ve) > W + 0.01f) {
            break;
          }
          brk = k;
          j = k;
        }
        if (brk <= a || l->wrap == AWRA_WRAP_CHAR) {
          /* break inside the word: as many characters as fit, at least one */
          int i = a + utf8_len((unsigned char)l->text[a]);
          while (i < para_end) {
            const int n = utf8_len((unsigned char)l->text[i]);
            if (range_width(l, a, i + n) > W + 0.01f) {
              break;
            }
            i += n;
          }
          if (l->wrap == AWRA_WRAP_CHAR && brk > a && brk >= i) {
            i = brk;
          }
          brk = i;
        }
        int vis_end = brk;
        while (vis_end > a && l->text[vis_end - 1] == ' ') {
          vis_end--;
        }
        add_run(l, a, vis_end, 0);
        ln->n_runs = l->n_runs - ln->first_run;
        ln->w = range_width(l, a, vis_end);
        a = brk;
        while (a < para_end && l->text[a] == ' ') {
          a++;
        }
      }
      line_metrics(l, ln, para_start);
      if (l->n_lines >= max_lines && (a < para_end || para_end < l->len)) {
        pos = l->len + 1;
        break;
      }
    } while (a < para_end);
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
  l->w = (int)ceilf(w);
  l->h = (int)ceilf(h);
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

int awra_text_layout_natural_width(AwraTextLayout *l)
{
  if (l->nat_w < 0) {
    float w = 0.0f;
    for (int a = 0; a <= l->len;) {
      int e = a;
      while (e < l->len && l->text[e] != '\n') {
        e++;
      }
      w = fmaxf(w, range_width(l, a, e));
      a = e + 1;
    }
    l->nat_w = (int)ceilf(w);
  }
  return l->nat_w;
}

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
      float w = 0.0f;
      int a = 0;
      for (int i = 0; i <= l->len; i++) {
        if (i == l->len || l->text[i] == ' ' || l->text[i] == '\n') {
          w = fmaxf(w, range_width(l, a, i));
          a = i + 1;
        }
      }
      l->min_w = (int)ceilf(w);
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

void awra_text_layout_draw(AwraTextLayout *l, AwraDraw *d, float x, float y, int width,
                           const AwraRGBA *color)
{
  layout(l, width);
  float ly = y;
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    float lx = x;
    /* lines align inside the paragraph's block (its widest line) */
    if (l->justify == AWRA_JUSTIFY_CENTER) {
      lx += floorf((l->w - ln->w) * 0.5f + 0.5f);
    }
    else if (l->justify == AWRA_JUSTIFY_RIGHT) {
      lx += (float)l->w - ln->w;
    }
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
      if (r->start >= 0 && is_object(l, r->start)) {
        continue;
      }
      if (r->start < 0) {
        awra_draw_text(d, &f, lx + r->x, base, ELLIPSIS, 3, &c);
      }
      else {
        awra_draw_text(d, &f, lx + r->x, base, l->text + r->start, r->end - r->start, &c);
      }
      if (r->span >= 0 && (l->spans[r->span].underline || l->spans[r->span].strike)) {
        const float t = fmaxf(1.0f, f.size / 14.0f);
        const float uy = l->spans[r->span].underline ? base + t * 1.5f : base - f.size * 0.3f;
        awra_paint_rect(d, (AwraBox){lx + r->x, uy, r->w, t}, &c);
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
/* Positions (editing): byte index <-> point, relative to the layout origin */

void awra_text_layout_index_to_pos(AwraTextLayout *l, int width, int index, float *r_x,
                                   float *r_y, float *r_h);
void awra_text_layout_index_to_pos(AwraTextLayout *l, int width, int index, float *r_x,
                                   float *r_y, float *r_h)
{
  layout(l, width);
  float y = 0.0f;
  *r_x = 0.0f;
  *r_y = 0.0f;
  *r_h = l->n_lines ? l->lines[0].height : awra_font_line_height(&l->font);
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    int ls = l->len, le = 0;
    for (int k = 0; k < ln->n_runs; k++) {
      const Run *r = &l->runs[ln->first_run + k];
      if (r->start >= 0) {
        ls = BK_MIN(ls, r->start);
        le = BK_MAX(le, r->end);
      }
    }
    if (ln->n_runs == 0) {
      ls = le = -1;
    }
    const bool last = i == l->n_lines - 1;
    const int next_start = last ? l->len + 1 : (l->lines[i + 1].n_runs ? l->runs[l->lines[i + 1].first_run].start : -1);
    if (index >= ls && (index <= le || last || (next_start >= 0 && index < next_start))) {
      *r_y = y;
      *r_h = ln->height;
      float x = 0.0f;
      for (int k = 0; k < ln->n_runs; k++) {
        const Run *r = &l->runs[ln->first_run + k];
        if (r->start < 0) {
          continue;
        }
        if (index <= r->end) {
          AwraFont f = span_font(l, r->span);
          x = r->x + seg_width(l, &f, r->start, r->start + BK_MAX(0, BK_MIN(index, r->end) - r->start));
          break;
        }
        x = r->x + r->w;
      }
      *r_x = x;
      return;
    }
    y += ln->height;
    *r_y = y - ln->height;
  }
}

int awra_text_layout_pos_to_index(AwraTextLayout *l, int width, float px, float py);
int awra_text_layout_pos_to_index(AwraTextLayout *l, int width, float px, float py)
{
  layout(l, width);
  float y = 0.0f;
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    if (py < y + ln->height || i == l->n_lines - 1) {
      int best = -1;
      float best_d = 1e9f;
      for (int k = 0; k < ln->n_runs; k++) {
        const Run *r = &l->runs[ln->first_run + k];
        if (r->start < 0) {
          continue;
        }
        AwraFont f = span_font(l, r->span);
        for (int j = r->start; j <= r->end;) {
          const float x = r->x + seg_width(l, &f, r->start, j);
          const float d = fabsf(x - px);
          if (d < best_d) {
            best_d = d;
            best = j;
          }
          if (j == r->end) {
            break;
          }
          j += utf8_len((unsigned char)l->text[j]);
        }
      }
      if (best < 0) { /* an empty line: its start */
        int s = 0;
        for (int k = 0; k < i; k++) {
          const Line *pl = &l->lines[k];
          for (int q = 0; q < pl->n_runs; q++) {
            if (l->runs[pl->first_run + q].end > s) {
              s = l->runs[pl->first_run + q].end;
            }
          }
        }
        best = BK_MIN(l->len, s + (i > 0 ? 1 : 0));
      }
      return best;
    }
    y += ln->height;
  }
  return l->len;
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
      float lx = 0.0f;
      if (l->justify == AWRA_JUSTIFY_CENTER) {
        lx = floorf((l->w - ln->w) * 0.5f + 0.5f);
      }
      else if (l->justify == AWRA_JUSTIFY_RIGHT) {
        lx = (float)l->w - ln->w;
      }
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
    float lx = 0.0f;
    if (l->justify == AWRA_JUSTIFY_CENTER) {
      lx = floorf((l->w - ln->w) * 0.5f + 0.5f);
    }
    else if (l->justify == AWRA_JUSTIFY_RIGHT) {
      lx = (float)l->w - ln->w;
    }
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

/* Fills the selection [start, end) (byte offsets) behind the text. */
void awra_text_layout_draw_selection(AwraTextLayout *l, AwraDraw *d, float x, float y, int width,
                                     int start, int end, const AwraRGBA *color)
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
  float ly = y;
  for (int i = 0; i < l->n_lines; i++) {
    const Line *ln = &l->lines[i];
    float lx = x;
    if (l->justify == AWRA_JUSTIFY_CENTER) {
      lx += floorf((l->w - ln->w) * 0.5f + 0.5f);
    }
    else if (l->justify == AWRA_JUSTIFY_RIGHT) {
      lx += (float)l->w - ln->w;
    }
    for (int k = 0; k < ln->n_runs; k++) {
      const Run *r = &l->runs[ln->first_run + k];
      if (r->start < 0 || r->end <= start || r->start >= end) {
        continue;
      }
      AwraFont f = span_font(l, r->span);
      const int a = BK_MAX(start, r->start), b = BK_MIN(end, r->end);
      const float x0 = r->x + seg_width(l, &f, r->start, a);
      const float x1 = r->x + seg_width(l, &f, r->start, b);
      awra_paint_rect(d, (AwraBox){lx + x0, ly, x1 - x0, ln->height}, color);
    }
    ly += ln->height;
  }
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
