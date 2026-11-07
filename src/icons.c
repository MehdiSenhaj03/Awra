/* Awra 2 icons: the files the icon theme chose (icontheme.c) loaded and
 * drawn as GTK 4.22 draws them (gtk/gtkiconpaintable.c,
 * gtk/gdktextureutils.c).
 *
 * - Symbolic SVGs (-symbolic.svg, -symbolic-ltr/-rtl.svg) are read as GTK
 *   reads them into render nodes: only <svg>, <path>, <rect>, <circle> draw,
 *   groups and metadata are skipped, each shape is filled with the colour
 *   of its class (foreground by default, "success" / "warning" / "error",
 *   "transparent-fill"), stroked only by a "*-stroke" class, whatever fill
 *   or stroke it says, with its fill-opacity and opacity. A file GTK can't
 *   read that way (a transform, another element, an unknown attribute) is
 *   read as GtkSvg does for symbolic icons (nsvgParseSymbolic). Either way
 *   the shapes are drawn once at the device size, each class in its own
 *   colour channel, then tinted when drawn: the foreground colour of the
 *   widget, GTK's success, warning and error colours.
 * - Other SVGs are drawn at the device size (scaled to fit, centred), PNG
 *   images decoded and scaled there; .symbolic.png images are tinted as the
 *   symbolic SVGs.
 * Rasters are cached per file and device size, least recently used dropped
 * past AWRA_ICON_CACHE_BYTES, all of them when the theme changes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "icontheme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* kit/intern/image/format_svg.c */
BKImBuf *imb_svg_render(const char *svg, size_t len, int out_w, int out_h, int symbolic, float *r_w,
                        float *r_h);
extern float imb_svg_offset_x, imb_svg_offset_y; /* format_svg.c: a sub-pixel offset */

#define AWRA_ICON_CACHE_BYTES (8u << 20)

enum { RASTER_MASK, RASTER_PLANES, RASTER_COLOR };

struct AwraIconRaster {
  AwraIconFile *file;
  int px;
  int phase; /* quarter pixels it is drawn off the grid: x * 4 + y (symbolic SVGs) */
  int kind;
  uint8_t *mask;  /* MASK: coverage */
  uint32_t *argb; /* COLOR: premultiplied; PLANES: A coverage, R G B the
                   * success / warning / error parts (premultiplied) */
  uint64_t key;
  /* PLANES: the last tint */
  uint32_t *tinted;
  uint64_t tint_key;
  float tint[4][4];
  size_t bytes;
  AwraIconRaster *next; /* the file's */
  AwraIconRaster *newer, *older;
};

static struct {
  AwraIconRaster *newest, *oldest;
  size_t bytes;
  uint64_t keys;
} R;

static uint64_t new_key(void)
{
  /* bit 62: icon rasters, never a texture's (bit 63) nor the kit's */
  return (1ull << 62) | ++R.keys;
}

static void lru_unlink(AwraIconRaster *r)
{
  if (r->newer) {
    r->newer->older = r->older;
  }
  else {
    R.newest = r->older;
  }
  if (r->older) {
    r->older->newer = r->newer;
  }
  else {
    R.oldest = r->newer;
  }
  r->newer = r->older = NULL;
}

static void lru_push(AwraIconRaster *r)
{
  r->older = R.newest;
  r->newer = NULL;
  if (R.newest) {
    R.newest->newer = r;
  }
  R.newest = r;
  if (!R.oldest) {
    R.oldest = r;
  }
}

static void raster_free(AwraIconRaster *r)
{
  for (AwraIconRaster **p = &r->file->rasters; *p; p = &(*p)->next) {
    if (*p == r) {
      *p = r->next;
      break;
    }
  }
  lru_unlink(r);
  if (r->key) {
    BUI_image_forget(r->key);
  }
  if (r->tint_key) {
    BUI_image_forget(r->tint_key);
  }
  R.bytes -= r->bytes;
  free(r->mask);
  free(r->argb);
  free(r->tinted);
  free(r);
}

void awra_icon_file_free_rasters(AwraIconFile *f)
{
  while (f->rasters) {
    raster_free(f->rasters);
  }
}

void awra_icon_rasters_clear(void)
{
  while (R.oldest) {
    raster_free(R.oldest);
  }
}

const unsigned char *awra_icon_file_bytes(const AwraIconFile *f, size_t *r_len, bool *r_own)
{
  *r_own = false;
  if (!f->path) {
    *r_len = f->len;
    return f->data;
  }
  *r_own = true;
  return (const unsigned char *)awra_read_file(f->path, r_len);
}

/* ======================================================================= */
/* GTK's reading of symbolic SVGs into nodes (gdktextureutils.c
 * gsk_render_node_new_from_bytes_symbolic): the shapes it would draw,
 * written out as a plain SVG, or NULL where GTK gives up. */

typedef struct Out {
  char *s;
  size_t n, cap;
  bool oom;
} Out;

static void out_add(Out *o, const char *str, size_t len)
{
  if (o->oom) {
    return;
  }
  if (o->n + len + 1 > o->cap) {
    size_t cap = o->cap ? o->cap * 2 : 1024;
    while (cap < o->n + len + 1) {
      cap *= 2;
    }
    char *ns = (char *)realloc(o->s, cap);
    if (!ns) {
      o->oom = true;
      return;
    }
    o->s = ns;
    o->cap = cap;
  }
  memcpy(o->s + o->n, str, len);
  o->n += len;
  o->s[o->n] = 0;
}

static void out_str(Out *o, const char *str)
{
  out_add(o, str, strlen(str));
}

/* an attribute value, escaped for XML */
static void out_attr(Out *o, const char *name, const char *v)
{
  out_str(o, " ");
  out_str(o, name);
  out_str(o, "=\"");
  for (const char *p = v; *p; p++) {
    switch (*p) {
      case '"':
        out_str(o, "&quot;");
        break;
      case '<':
        out_str(o, "&lt;");
        break;
      case '&':
        out_str(o, "&amp;");
        break;
      default:
        out_add(o, p, 1);
    }
  }
  out_str(o, "\"");
}

static void out_num(Out *o, const char *name, double v)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%.9g", v);
  out_attr(o, name, buf);
}

#define MAX_ATTRS 64

typedef struct Tag {
  char name[64];
  int n;
  char *k[MAX_ATTRS];
  char *v[MAX_ATTRS];
  bool close, empty;
} Tag;

/* XML entities in place (GMarkup's) */
static void unescape(char *s)
{
  char *w = s;
  for (char *r = s; *r;) {
    if (*r == '&') {
      static const struct {
        const char *e;
        char c;
      } ents[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
      bool done = false;
      for (size_t i = 0; i < 5; i++) {
        const size_t l = strlen(ents[i].e);
        if (!strncmp(r, ents[i].e, l)) {
          *w++ = ents[i].c;
          r += l;
          done = true;
          break;
        }
      }
      if (!done && r[1] == '#') {
        char *end;
        const long c = r[2] == 'x' ? strtol(r + 3, &end, 16) : strtol(r + 2, &end, 10);
        if (*end == ';' && c > 0 && c < 128) {
          *w++ = (char)c;
          r = end + 1;
          done = true;
        }
      }
      if (!done) {
        *w++ = *r++;
      }
    }
    else {
      *w++ = *r++;
    }
  }
  *w = 0;
}

/* the next tag of `*p` (a NUL-terminated, writable text); false at the end
 * or on malformed XML */
static bool next_tag(char **p, Tag *t, bool *r_bad)
{
  char *s = *p;
  for (;;) {
    s = strchr(s, '<');
    if (!s) {
      return false;
    }
    if (!strncmp(s, "<!--", 4)) {
      char *e = strstr(s + 4, "-->");
      if (!e) {
        *r_bad = true;
        return false;
      }
      s = e + 3;
      continue;
    }
    if (!strncmp(s, "<![CDATA[", 9)) {
      char *e = strstr(s + 9, "]]>");
      if (!e) {
        *r_bad = true;
        return false;
      }
      s = e + 3;
      continue;
    }
    if (s[1] == '?' || s[1] == '!') {
      char *e = strchr(s, '>');
      if (!e) {
        *r_bad = true;
        return false;
      }
      s = e + 1;
      continue;
    }
    break;
  }
  s++;
  memset(t, 0, sizeof(*t));
  if (*s == '/') {
    t->close = true;
    s++;
  }
  size_t n = 0;
  while (*s && !strchr(" \t\r\n/>", *s)) {
    if (n + 1 < sizeof(t->name)) {
      t->name[n++] = *s;
    }
    s++;
  }
  t->name[n] = 0;
  for (;;) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
      s++;
    }
    if (!*s) {
      *r_bad = true;
      return false;
    }
    if (*s == '>') {
      s++;
      break;
    }
    if (*s == '/' && s[1] == '>') {
      t->empty = true;
      s += 2;
      break;
    }
    char *k = s;
    while (*s && !strchr(" \t\r\n=/>", *s)) {
      s++;
    }
    char *kend = s;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
      s++;
    }
    if (*s != '=') {
      *r_bad = true;
      return false;
    }
    s++;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
      s++;
    }
    const char q = *s;
    if (q != '"' && q != '\'') {
      *r_bad = true;
      return false;
    }
    char *v = ++s;
    s = strchr(s, q);
    if (!s) {
      *r_bad = true;
      return false;
    }
    *kend = 0;
    *s++ = 0;
    if (t->n < MAX_ATTRS) {
      unescape(v);
      t->k[t->n] = k;
      t->v[t->n] = v;
      t->n++;
    }
  }
  *p = s;
  return true;
}

static const char *tag_get(const Tag *t, const char *k)
{
  for (int i = 0; i < t->n; i++) {
    if (!strcmp(t->k[i], k)) {
      return t->v[i];
    }
  }
  return NULL;
}

/* g_ascii_strtod with GTK's check: the whole value a number */
static bool num(const char *v, double *r, bool px_ok)
{
  char *end;
  *r = strtod(v, &end);
  if (end == v && *v) {
    return false;
  }
  return !*end || (px_ok && !strcmp(end, "px"));
}

static bool has_class(const char *list, const char *name)
{
  const size_t n = strlen(name);
  for (const char *q = list; *q;) {
    while (*q == ' ') {
      q++;
    }
    const char *e = q;
    while (*e && *e != ' ') {
      e++;
    }
    if ((size_t)(e - q) == n && !strncmp(q, name, n)) {
      return true;
    }
    q = e;
  }
  return false;
}

static const char *const k_roles[] = {"#000000", "#ff0000", "#00ff00", "#0000ff"};

typedef struct NodeInfo {
  double w, h;
  int n_paths;
  bool only_fg;
} NodeInfo;

static char *gtk_node_svg(const unsigned char *bytes, size_t len, NodeInfo *info)
{
  char *text = (char *)malloc(len + 1);
  if (!text) {
    return NULL;
  }
  memcpy(text, bytes, len);
  text[len] = 0;
  Out o = {0};
  memset(info, 0, sizeof(*info));
  info->only_fg = true;
  bool bad = false, have_svg = false;
  Tag t;
  char *p = text;
  static const char *const allowed[] = {
      "class", "opacity", "visibility", "fill-rule", "fill-opacity", "stroke-width",
      "stroke-opacity", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit",
      "stroke-dasharray", "stroke-dashoffset", "fill", "stroke", "style", "id", "color",
      "overflow", "d", "x", "y", "width", "height", "rx", "ry", "cx", "cy", "r"};
  while (!bad && next_tag(&p, &t, &bad)) {
    if (t.close) {
      continue;
    }
    const char *el = t.name;
    if (!strcmp(el, "svg")) {
      const char *w = tag_get(&t, "width"), *h = tag_get(&t, "height"), *vb = tag_get(&t, "viewBox");
      if (!w || !h || !num(w, &info->w, true) || !num(h, &info->h, true)) {
        bad = true;
        break;
      }
      if (vb) {
        /* split on single spaces: four parts must be 0 0 width height */
        int parts = 1;
        for (const char *q = vb; *q; q++) {
          parts += *q == ' ';
        }
        if (parts == 4) {
          double d[4];
          char buf[256];
          snprintf(buf, sizeof(buf), "%s", vb);
          char *save = buf;
          bool ok = true;
          for (int i = 0; i < 4; i++) {
            char *sp = strchr(save, ' ');
            if (sp) {
              *sp = 0;
            }
            char *end;
            d[i] = strtod(save, &end);
            ok = ok && !*end;
            save = sp ? sp + 1 : save + strlen(save);
          }
          if (!ok || d[0] != 0 || d[1] != 0 || d[2] != info->w || d[3] != info->h) {
            bad = true;
            break;
          }
        }
      }
      if (!have_svg) {
        char head[160];
        snprintf(head, sizeof(head),
                 "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.9g\" height=\"%.9g\">", info->w,
                 info->h);
        out_str(&o, head);
        have_svg = true;
      }
      continue;
    }
    if (!strcmp(el, "g") || !strcmp(el, "defs") || !strcmp(el, "style") || !strcmp(el, "metadata") ||
        !strncmp(el, "sodipodi:", 9) || !strncmp(el, "inkscape:", 9) || !strncmp(el, "rdf:", 4) ||
        !strncmp(el, "cc:", 3) || !strncmp(el, "dc:", 3))
    {
      continue;
    }
    Out shape = {0};
    if (!strcmp(el, "circle")) {
      double cx = 0, cy = 0, r = 0;
      const char *a;
      if (((a = tag_get(&t, "cx")) && !num(a, &cx, false)) || ((a = tag_get(&t, "cy")) && !num(a, &cy, false)) ||
          ((a = tag_get(&t, "r")) && (!num(a, &r, false) || r < 0)))
      {
        bad = true;
        break;
      }
      if (r == 0) {
        continue;
      }
      out_str(&shape, "<circle");
      out_num(&shape, "cx", cx);
      out_num(&shape, "cy", cy);
      out_num(&shape, "r", r);
    }
    else if (!strcmp(el, "rect")) {
      double x = 0, y = 0, w = 0, h = 0, rx = 0, ry = 0;
      const char *a, *wa = tag_get(&t, "width"), *ha = tag_get(&t, "height");
      const char *rxa = tag_get(&t, "rx"), *rya = tag_get(&t, "ry");
      if (((a = tag_get(&t, "x")) && !num(a, &x, false)) || ((a = tag_get(&t, "y")) && !num(a, &y, false)) ||
          !wa || !ha || !num(wa, &w, false) || w < 0 || !num(ha, &h, false) || h < 0)
      {
        bad = true;
        break;
      }
      if (w == 0 || h == 0) {
        continue;
      }
      if ((rxa && (!num(rxa, &rx, false) || rx < 0)) || (rya && (!num(rya, &ry, false) || ry < 0))) {
        bad = true;
        break;
      }
      if (!rxa && rya) {
        rx = ry;
      }
      else if (rxa && !rya) {
        ry = rx;
      }
      out_str(&shape, "<rect");
      out_num(&shape, "x", x);
      out_num(&shape, "y", y);
      out_num(&shape, "width", w);
      out_num(&shape, "height", h);
      if (rx > 0 || ry > 0) {
        out_num(&shape, "rx", rx);
        out_num(&shape, "ry", ry);
      }
    }
    else if (!strcmp(el, "path")) {
      const char *d = tag_get(&t, "d");
      if (!d) {
        bad = true;
        break;
      }
      out_str(&shape, "<path");
      out_attr(&shape, "d", d);
    }
    else {
      bad = true; /* "Unhandled element" */
      break;
    }
    /* every attribute one GTK knows */
    for (int i = 0; i < t.n && !bad; i++) {
      bool ok = !strncmp(t.k[i], "gpa:", 4);
      for (size_t k = 0; !ok && k < sizeof(allowed) / sizeof(allowed[0]); k++) {
        ok = !strcmp(t.k[i], allowed[k]);
      }
      bad = !ok;
    }
    double fill_opacity = 1, stroke_opacity = 1, opacity = 1, v;
    const char *a;
    if ((a = tag_get(&t, "fill-opacity"))) {
      if (!num(a, &v, false)) {
        bad = true;
      }
      fill_opacity = v < 0 ? 0 : v > 1 ? 1 : v;
    }
    if ((a = tag_get(&t, "stroke-opacity"))) {
      if (!num(a, &v, false)) {
        bad = true;
      }
      stroke_opacity = v < 0 ? 0 : v > 1 ? 1 : v;
    }
    int fill = 0, stroke = -1; /* role, -1 none */
    const char *cls = tag_get(&t, "class");
    bool do_fill = true;
    double fill_alpha = fill_opacity;
    if (cls) {
      if (has_class(cls, "transparent-fill")) {
        do_fill = false;
        info->only_fg = false;
      }
      else if (has_class(cls, "foreground-fill")) {
        fill = 0;
      }
      else if (has_class(cls, "success") || has_class(cls, "success-fill")) {
        fill = 1;
        info->only_fg = false;
      }
      else if (has_class(cls, "warning") || has_class(cls, "warning-fill")) {
        fill = 2;
        fill_alpha = 1; /* sic: GTK drops the fill-opacity of warnings */
        info->only_fg = false;
      }
      else if (has_class(cls, "error") || has_class(cls, "error-fill")) {
        fill = 3;
        info->only_fg = false;
      }
      if (has_class(cls, "success-stroke")) {
        stroke = 1;
        info->only_fg = false;
      }
      else if (has_class(cls, "warning-stroke")) {
        stroke = 2;
        info->only_fg = false;
      }
      else if (has_class(cls, "error-stroke")) {
        stroke = 3;
        info->only_fg = false;
      }
      else if (has_class(cls, "foreground-stroke")) {
        stroke = 0;
      }
    }
    if ((a = tag_get(&t, "opacity")) && !num(a, &opacity, false)) {
      bad = true;
    }
    const bool visible = !((a = tag_get(&t, "visibility")) && !strcmp(a, "hidden"));
    const char *rule = tag_get(&t, "fill-rule");
    /* the stroke: width 2, round caps and joins unless said */
    const char *sw = tag_get(&t, "stroke-width"), *cap = tag_get(&t, "stroke-linecap");
    const char *join = tag_get(&t, "stroke-linejoin"), *ml = tag_get(&t, "stroke-miterlimit");
    const char *dash = tag_get(&t, "stroke-dasharray"), *dashoff = tag_get(&t, "stroke-dashoffset");
    if ((sw && !num(sw, &v, false)) ||
        (cap && strcmp(cap, "butt") && strcmp(cap, "round") && strcmp(cap, "square")) ||
        (join && strcmp(join, "miter") && strcmp(join, "round") && strcmp(join, "bevel")) ||
        (ml && (!num(ml, &v, false) || v < 1)) || (dashoff && !num(dashoff, &v, false)))
    {
      bad = true;
    }
    if (bad) {
      free(shape.s);
      break;
    }
    if (visible && (do_fill || stroke >= 0)) {
      if (do_fill) {
        out_attr(&shape, "fill", k_roles[fill]);
        out_num(&shape, "fill-opacity", fill_alpha);
        out_attr(&shape, "fill-rule", rule && !strcmp(rule, "evenodd") ? "evenodd" : "nonzero");
        info->n_paths++;
      }
      else {
        out_attr(&shape, "fill", "none");
      }
      if (stroke >= 0) {
        out_attr(&shape, "stroke", k_roles[stroke]);
        out_num(&shape, "stroke-opacity", stroke_opacity);
        out_attr(&shape, "stroke-width", sw ? sw : "2");
        out_attr(&shape, "stroke-linecap", cap ? cap : "round");
        out_attr(&shape, "stroke-linejoin", join ? join : "round");
        if (ml) {
          out_attr(&shape, "stroke-miterlimit", ml);
        }
        if (dash && strcmp(dash, "none") != 0) {
          out_attr(&shape, "stroke-dasharray", dash);
        }
        if (dashoff) {
          out_attr(&shape, "stroke-dashoffset", dashoff);
        }
        info->n_paths++;
      }
      if (opacity != 1) {
        out_num(&shape, "opacity", opacity);
      }
      out_str(&shape, "/>");
      if (shape.s) {
        out_str(&o, shape.s);
      }
    }
    free(shape.s);
    if (o.oom || shape.oom) {
      bad = true;
    }
  }
  free(text);
  if (bad || !have_svg || o.oom) {
    free(o.s);
    return NULL;
  }
  out_str(&o, "</svg>");
  if (o.oom) {
    free(o.s);
    return NULL;
  }
  return o.s;
}

/* ======================================================================= */
/* Loading */

/* a premultiplied image scaled (bilinear) to fit px x px, centred */
static uint32_t *fit_square(const uint32_t *src, int w, int h, int px)
{
  uint32_t *dst = (uint32_t *)calloc((size_t)px * px, sizeof(uint32_t));
  if (!dst || w <= 0 || h <= 0) {
    return dst;
  }
  const float s = fminf((float)px / w, (float)px / h);
  const int dw = BK_MAX(1, (int)lroundf(w * s)), dh = BK_MAX(1, (int)lroundf(h * s));
  const int ox = (px - dw) / 2, oy = (px - dh) / 2;
  if (dw == w && dh == h) {
    for (int y = 0; y < h; y++) {
      memcpy(dst + (size_t)(y + oy) * px + ox, src + (size_t)y * w, sizeof(uint32_t) * w);
    }
    return dst;
  }
  for (int y = 0; y < dh; y++) {
    const float fy = fmaxf(0.0f, (y + 0.5f) * h / dh - 0.5f);
    const int y0 = BK_MIN((int)fy, h - 1), y1 = BK_MIN(y0 + 1, h - 1);
    const float ty = fy - y0;
    for (int x = 0; x < dw; x++) {
      const float fx = fmaxf(0.0f, (x + 0.5f) * w / dw - 0.5f);
      const int x0 = BK_MIN((int)fx, w - 1), x1 = BK_MIN(x0 + 1, w - 1);
      const float tx = fx - x0;
      const uint32_t c[4] = {src[(size_t)y0 * w + x0], src[(size_t)y0 * w + x1], src[(size_t)y1 * w + x0],
                             src[(size_t)y1 * w + x1]};
      uint32_t out = 0;
      for (int sh = 0; sh < 32; sh += 8) {
        const float a = ((c[0] >> sh) & 255) * (1 - tx) + ((c[1] >> sh) & 255) * tx;
        const float b = ((c[2] >> sh) & 255) * (1 - tx) + ((c[3] >> sh) & 255) * tx;
        out |= (uint32_t)lroundf(a * (1 - ty) + b * ty) << sh;
      }
      dst[(size_t)(y + oy) * px + x + ox] = out;
    }
  }
  return dst;
}

/* role-coded pixels: a coverage mask when only the foreground is used */
static void take_planes(AwraIconRaster *r, uint32_t *px)
{
  const size_t n = (size_t)r->px * r->px;
  bool colors = false;
  for (size_t i = 0; i < n && !colors; i++) {
    colors = (px[i] & 0xffffffu) != 0;
  }
  if (!colors) {
    r->mask = (uint8_t *)malloc(n);
    if (r->mask) {
      for (size_t i = 0; i < n; i++) {
        r->mask[i] = (uint8_t)(px[i] >> 24);
      }
      r->kind = RASTER_MASK;
      r->bytes = n;
    }
    free(px);
    return;
  }
  r->kind = RASTER_PLANES;
  r->argb = px;
  r->bytes = n * 4;
}

/* An element GtkSvg (GTK 4.22) reports as not supported: GTK then loads the
 * file with gdk-pixbuf instead (gdk_texture_new_from_svg_bytes). */
static bool gtksvg_unsupported(const unsigned char *bytes, size_t len)
{
  static const char *const els[] = {"style", "textPath", "feConvolveMatrix", "feDiffuseLighting",
                                    "feMorphology", "feSpecularLighting", "feTurbulence"};
  for (size_t i = 0; i + 1 < len; i++) {
    if (bytes[i] != '<') {
      continue;
    }
    for (size_t k = 0; k < sizeof(els) / sizeof(els[0]); k++) {
      const size_t l = strlen(els[k]);
      if (i + 1 + l < len && !memcmp(bytes + i + 1, els[k], l) &&
          strchr(" \t\r\n/>", bytes[i + 1 + l]))
      {
        return true;
      }
    }
  }
  return false;
}

static void load_raster_at(AwraIconRaster *r);

static void load_raster(AwraIconRaster *r)
{
  imb_svg_offset_x = (float)(r->phase / 4) * 0.25f;
  imb_svg_offset_y = (float)(r->phase % 4) * 0.25f;
  load_raster_at(r);
  imb_svg_offset_x = imb_svg_offset_y = 0.0f;
}

static void load_raster_at(AwraIconRaster *r)
{
  const AwraIconFile *f = r->file;
  const int px = r->px;
  size_t len = 0;
  bool own = false;
  const unsigned char *bytes = awra_icon_file_bytes(f, &len, &own);
  if (!bytes) {
    return;
  }
  if (f->is_svg) {
    BKImBuf *ib = NULL;
    if (f->is_symbolic) {
      NodeInfo info;
      char *plain = gtk_node_svg(bytes, len, &info);
      if (plain) {
        ib = imb_svg_render(plain, strlen(plain), px, px, 0, NULL, NULL);
        free(plain);
      }
      else {
        ib = imb_svg_render((const char *)bytes, len, px, px, 1, NULL, NULL);
      }
    }
    else if (gtksvg_unsupported(bytes, len)) {
      /* GtkSvg doesn't take it: GTK loads it as an image of its own size
       * (gdk-pixbuf) and scales that */
      ib = imb_svg_render((const char *)bytes, len, 0, 0, 0, NULL, NULL);
      if (ib) {
        int w = 0, h = 0;
        BK_imbuf_size(ib, &w, &h);
        uint32_t *sq = fit_square(BK_imbuf_pixels(ib, 0), w, h, px);
        BK_imbuf_free(ib);
        ib = NULL;
        if (sq) {
          r->kind = RASTER_COLOR;
          r->argb = sq;
          r->bytes = (size_t)px * px * 4;
        }
      }
    }
    else {
      ib = imb_svg_render((const char *)bytes, len, px, px, 0, NULL, NULL);
    }
    if (ib) {
      uint32_t *copy = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)px * px);
      if (copy) {
        memcpy(copy, BK_imbuf_pixels(ib, 0), sizeof(uint32_t) * (size_t)px * px);
        if (f->is_symbolic) {
          take_planes(r, copy);
        }
        else {
          r->kind = RASTER_COLOR;
          r->argb = copy;
          r->bytes = (size_t)px * px * 4;
        }
      }
      BK_imbuf_free(ib);
    }
  }
  else {
    BKImBuf *ib = BK_imbuf_decode(bytes, len, px, px);
    if (ib) {
      int w = 0, h = 0;
      BK_imbuf_size(ib, &w, &h);
      uint32_t *sq = fit_square(BK_imbuf_pixels(ib, 0), w, h, px);
      BK_imbuf_free(ib);
      if (sq) {
        if (f->is_symbolic) {
          take_planes(r, sq);
        }
        else {
          r->kind = RASTER_COLOR;
          r->argb = sq;
          r->bytes = (size_t)px * px * 4;
        }
      }
    }
  }
  if (own) {
    free((void *)bytes);
  }
  if (r->argb) {
    r->key = new_key();
  }
}

static AwraIconRaster *raster_get_phase(AwraIconFile *f, int px, int phase);

static AwraIconRaster *raster_get(AwraIconFile *f, int px)
{
  return raster_get_phase(f, px, 0);
}

static AwraIconRaster *raster_get_phase(AwraIconFile *f, int px, int phase)
{
  for (AwraIconRaster *r = f->rasters; r; r = r->next) {
    if (r->px == px && r->phase == phase) {
      if (R.newest != r) {
        lru_unlink(r);
        lru_push(r);
      }
      return r;
    }
  }
  AwraIconRaster *r = (AwraIconRaster *)calloc(1, sizeof(AwraIconRaster));
  if (!r) {
    return NULL;
  }
  r->file = f;
  r->px = px;
  r->phase = phase;
  load_raster(r);
  r->next = f->rasters;
  f->rasters = r;
  lru_push(r);
  R.bytes += r->bytes;
  while (R.bytes > AWRA_ICON_CACHE_BYTES && R.oldest && R.oldest != r) {
    raster_free(R.oldest);
  }
  return r;
}

/* ======================================================================= */
/* Drawing */

/* GTK's symbolic colours: the foreground, then GTK's success, warning and
 * error (gtk/theme/Default/_colors.scss: -gtk-icon-palette's default) */
static void palette(const AwraRGBA *fg, float out[4][4])
{
  const bool dark = awra_style_is_dark();
  const uint32_t success = dark ? 0x26ab62 : 0x33d17a, warning = 0xf57900, error = 0xcc0000;
  out[0][0] = fg->r;
  out[0][1] = fg->g;
  out[0][2] = fg->b;
  out[0][3] = fg->a;
  const uint32_t c[3] = {success, warning, error};
  for (int i = 0; i < 3; i++) {
    out[i + 1][0] = ((c[i] >> 16) & 255) / 255.0f;
    out[i + 1][1] = ((c[i] >> 8) & 255) / 255.0f;
    out[i + 1][2] = (c[i] & 255) / 255.0f;
    out[i + 1][3] = 1.0f;
  }
}

/* PLANES tinted with a palette: out = R success + G warning + B error +
 * (A - R - G - B) fg (premultiplied) */
static const uint32_t *tinted(AwraIconRaster *r, float pal[4][4])
{
  if (r->tinted && !memcmp(pal, r->tint, sizeof(r->tint))) {
    return r->tinted;
  }
  const size_t n = (size_t)r->px * r->px;
  if (!r->tinted) {
    r->tinted = (uint32_t *)malloc(n * 4);
    if (!r->tinted) {
      return NULL;
    }
    r->bytes += n * 4;
    R.bytes += n * 4;
  }
  if (r->tint_key) {
    BUI_image_forget(r->tint_key);
  }
  r->tint_key = new_key();
  memcpy(r->tint, pal, sizeof(r->tint));
  for (size_t i = 0; i < n; i++) {
    const uint32_t p = r->argb[i];
    const float a = (p >> 24) / 255.0f, s = ((p >> 16) & 255) / 255.0f, w = ((p >> 8) & 255) / 255.0f,
                e = (p & 255) / 255.0f;
    const float f = fmaxf(0.0f, a - s - w - e);
    float c[4];
    /* colours premultiplied by their alpha, then by the coverage parts */
    for (int k = 0; k < 3; k++) {
      c[k] = f * pal[0][k] * pal[0][3] + s * pal[1][k] * pal[1][3] + w * pal[2][k] * pal[2][3] +
             e * pal[3][k] * pal[3][3];
    }
    c[3] = f * pal[0][3] + s * pal[1][3] + w * pal[2][3] + e * pal[3][3];
    uint32_t out = 0;
    for (int k = 0; k < 4; k++) {
      const int v = (int)lroundf(fminf(1.0f, fmaxf(0.0f, c[k])) * 255.0f);
      out |= (uint32_t)v << (k == 3 ? 24 : 16 - 8 * k);
    }
    r->tinted[i] = out;
  }
  return r->tinted;
}

/* GTK's lookup size and scale for a raster size: the integer scale of the
 * window (gtk_widget_get_scale_factor) and the size in application px */
static void lookup_size(int px, int *r_size, int *r_scale)
{
  const float s = awra_scale();
  int scale = (int)ceilf(s - 0.001f);
  scale = scale < 1 ? 1 : scale;
  int size = (int)lroundf(px / s);
  *r_size = size < 1 ? 1 : size;
  *r_scale = scale;
}

static AwraIconRaster *raster_for(const char *name, int px, bool rtl)
{
  if (!name || !name[0] || px <= 0) {
    return NULL;
  }
  int size, scale;
  lookup_size(px, &size, &scale);
  AwraIconFile *f = awra_icon_theme_choose(name, size, scale, rtl ? 2 : 1, 0);
  return f ? raster_get(f, px) : NULL;
}

static void draw_raster(AwraDraw *d, AwraIconRaster *r, int x, int y, const AwraRGBA *color)
{
  const AwraRGBA fg = color ? *color : awra_tokens()->foreground;
  switch (r->kind) {
    case RASTER_MASK: {
      if (!r->mask) {
        return;
      }
      float c[4] = {fg.r, fg.g, fg.b, fg.a * d->opacity};
      SR_mask(d->r, x, y, r->mask, r->px, r->px, r->px, c);
      return;
    }
    case RASTER_PLANES: {
      float pal[4][4];
      palette(&fg, pal);
      const uint32_t *px = tinted(r, pal);
      if (px) {
        const BuiPixmap pm = {px, r->px, r->px, r->tint_key, BUI_PIXMAP_ARGB, 0};
        SR_image(d->r, (float)x, (float)y, (float)r->px, (float)r->px, &pm, NULL, d->opacity, 0); /* symbolic: paths, at the fraction */
      }
      return;
    }
    default:
      if (r->argb) {
        const BuiPixmap pm = {r->argb, r->px, r->px, r->key, BUI_PIXMAP_ARGB, 0};
        BUI_raster_image(d->r, (float)(x + awra_shift_ix()), (float)(y + awra_shift_iy()), (float)r->px, (float)r->px, &pm, NULL, d->opacity, 0); /* snapped, as GTK's icon textures */
      }
  }
}

void awra_paint_icon(AwraDraw *d, const char *name, float x, float y, int size, const AwraRGBA *color)
{
  AwraIconRaster *r = raster_for(name, size, d->rtl);
  if (!r) {
    return;
  }
  const int ix = (int)floorf(x + 0.5f), iy = (int)floorf(y + 0.5f);
  if ((awra_shift_x != 0.0f || awra_shift_y != 0.0f) && r->kind != RASTER_COLOR && r->file->is_svg) {
    /* a symbolic icon off the pixel grid (a shifted subtree): GTK draws
     * its paths there; rasterized at that quarter-pixel phase */
    const float fx = ix + awra_shift_x, fy = iy + awra_shift_y;
    int gx = (int)floorf(fx), gy = (int)floorf(fy);
    int qx = (int)floorf((fx - gx) * 4.0f + 0.5f), qy = (int)floorf((fy - gy) * 4.0f + 0.5f);
    if (qx == 4) {
      gx++;
      qx = 0;
    }
    if (qy == 4) {
      gy++;
      qy = 0;
    }
    AwraIconRaster *p = (qx || qy) ? raster_get_phase(r->file, r->px, qx * 4 + qy) : r;
    if (p) {
      const float sx = awra_shift_x, sy = awra_shift_y;
      awra_shift_x = awra_shift_y = 0.0f; /* placed: drawn where it lands */
      draw_raster(d, p, gx, gy, color);
      awra_shift_x = sx;
      awra_shift_y = sy;
      return;
    }
  }
  draw_raster(d, r, ix, iy, color);
}

/* What the theme has for `name` at `size` raster px: a symbolic icon's
 * coverage (NULL for a full-colour one, or a multi-colour symbolic). */
const AwraIconMask *awra_icon_mask(const char *name, int size)
{
  static AwraIconMask m;
  AwraIconRaster *r = raster_for(name, size, false);
  if (!r || r->kind != RASTER_MASK || !r->mask) {
    return NULL;
  }
  m.w = m.h = r->px;
  m.a = r->mask;
  return &m;
}

/* A full-colour icon (or a file) as a pixmap; NULL for a symbolic one. */
const BuiPixmap *awra_icon_pixmap(const char *name, int size)
{
  static BuiPixmap pm;
  AwraIconRaster *r = raster_for(name, size, false);
  if (!r || r->kind != RASTER_COLOR || !r->argb) {
    return NULL;
  }
  pm = (BuiPixmap){r->argb, r->px, r->px, r->key, BUI_PIXMAP_ARGB, 0};
  return &pm;
}

/* Whether the icon a name shows is symbolic (tinted with the text). */
bool awra_icon_is_symbolic_at(const char *name, int size)
{
  AwraIconRaster *r = raster_for(name, size, false);
  return r && r->kind != RASTER_COLOR;
}

/* An icon drawn into premultiplied ARGB px x px (malloc'd), tinted with fg
 * (NULL: the text colour): drag images, window icons, textures. */
uint32_t *awra_icon_render_argb(const char *name, int px, const AwraRGBA *fg);
uint32_t *awra_icon_render_argb(const char *name, int px, const AwraRGBA *fg)
{
  AwraIconRaster *r = raster_for(name, px, false);
  if (!r) {
    return NULL;
  }
  uint32_t *out = (uint32_t *)calloc((size_t)px * px, sizeof(uint32_t));
  if (!out) {
    return NULL;
  }
  const AwraRGBA c = fg ? *fg : awra_tokens()->foreground;
  if (r->kind == RASTER_COLOR && r->argb) {
    memcpy(out, r->argb, sizeof(uint32_t) * (size_t)px * px);
  }
  else if (r->kind == RASTER_MASK && r->mask) {
    for (size_t i = 0, n = (size_t)px * px; i < n; i++) {
      const float a = r->mask[i] / 255.0f * c.a;
      out[i] = (uint32_t)lroundf(a * 255) << 24 | (uint32_t)lroundf(c.r * a * 255) << 16 |
               (uint32_t)lroundf(c.g * a * 255) << 8 | (uint32_t)lroundf(c.b * a * 255);
    }
  }
  else if (r->kind == RASTER_PLANES) {
    float pal[4][4];
    palette(&c, pal);
    const uint32_t *t = tinted(r, pal);
    if (t) {
      memcpy(out, t, sizeof(uint32_t) * (size_t)px * px);
    }
  }
  return out;
}

/* A file drawn once, outside the cache (AwraIconPaintable). */
AwraTexture *awra_icon_file_render_texture(const AwraIconFile *f, int px, const AwraRGBA *fg);
AwraTexture *awra_icon_file_render_texture(const AwraIconFile *f, int px, const AwraRGBA *fg)
{
  if (!f || px <= 0) {
    return NULL;
  }
  AwraIconRaster r = {0};
  r.file = (AwraIconFile *)f;
  r.px = px;
  load_raster(&r);
  AwraTexture *t = NULL;
  const AwraRGBA c = fg ? *fg : awra_tokens()->foreground;
  uint32_t *out = (uint32_t *)calloc((size_t)px * px, sizeof(uint32_t));
  if (out) {
    if (r.kind == RASTER_COLOR && r.argb) {
      memcpy(out, r.argb, sizeof(uint32_t) * (size_t)px * px);
    }
    else if (r.kind == RASTER_MASK && r.mask) {
      for (size_t i = 0, n = (size_t)px * px; i < n; i++) {
        const float a = r.mask[i] / 255.0f * c.a;
        out[i] = (uint32_t)lroundf(a * 255) << 24 | (uint32_t)lroundf(c.r * a * 255) << 16 |
                 (uint32_t)lroundf(c.g * a * 255) << 8 | (uint32_t)lroundf(c.b * a * 255);
      }
    }
    else if (r.kind == RASTER_PLANES && r.argb) {
      float pal[4][4];
      palette(&c, pal);
      const uint32_t *tp = tinted(&r, pal);
      if (tp) {
        memcpy(out, tp, sizeof(uint32_t) * (size_t)px * px);
      }
      R.bytes -= r.tinted ? (size_t)px * px * 4 : 0; /* tinted() counted it */
    }
    t = awra_texture_new_argb(out, px, px);
    free(out);
  }
  if (r.tint_key) {
    BUI_image_forget(r.tint_key);
  }
  free(r.mask);
  free(r.argb);
  free(r.tinted);
  return t;
}
