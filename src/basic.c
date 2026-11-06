/* Awra 2 labels and images (GtkLabel / GtkImage semantics).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

bool awra_inherited_style(const AwraWidget *w, AwraRGBA *r_color, int *r_weight)
{
  for (const BKNode *n = w ? w->parent : NULL; n; n = n->parent) {
    if (awra_is_widget(n) && AWRA_CLASS_OF(n)->child_style &&
        AWRA_CLASS_OF(n)->child_style((AwraWidget *)n, r_color, r_weight))
    {
      return true;
    }
  }
  return false;
}

/* ======================================================================= */
/* Label */

typedef struct LabelExt {
  AwraTextLayout *layout;
  char *source; /* text or markup as given */
  bool markup;
  AwraWrap wrap;
  AwraEllipsize ellipsize;
  AwraJustify justify;
  float xalign, yalign;
  int lines, width_chars, max_width_chars;
  bool has_color;
  AwraRGBA color;
  int weight;
  float scale;
  unsigned style_gen;
} LabelExt;

#define LX(w) AWRA_EXT(w, LabelExt)

static AwraFont label_font(AwraWidget *w)
{
  LabelExt *x = LX(w);
  const AwraTypography t = awra_widget_typography(w);
  AwraFont f = awra_font_for(t);
  AwraRGBA c;
  int weight = 0;
  if (!awra_aux(w)->typography_set && awra_inherited_style(w, &c, &weight) && weight > 0) {
    f.weight = weight;
  }
  if (x->weight > 0) {
    f.weight = x->weight;
  }
  if (x->scale > 0.0f && x->scale != 1.0f) {
    f.size *= x->scale;
    f.letter_spacing *= x->scale;
  }
  return f;
}

static void label_sync(AwraWidget *w)
{
  LabelExt *x = LX(w);
  AwraFont f = label_font(w);
  awra_text_layout_set_font(x->layout, &f);
  awra_text_layout_set_wrap(x->layout, x->wrap, x->ellipsize, x->lines);
  awra_text_layout_set_line_factor(x->layout, awra_typography_line_factor(awra_widget_typography(w)));
  awra_text_layout_set_justify(x->layout, x->justify);
  x->style_gen = awra_style_generation();
}

static void label_init(AwraWidget *w)
{
  LabelExt *x = LX(w);
  x->layout = awra_text_layout_new();
  x->xalign = 0.5f;
  x->yalign = 0.5f;
  x->scale = 1.0f;
  x->lines = -1;
  x->width_chars = -1;
  x->max_width_chars = -1;
}

static void label_finalize(AwraWidget *w)
{
  awra_text_layout_free(LX(w)->layout);
  free(LX(w)->source);
}

static float char_width(AwraWidget *w)
{
  AwraFont f = label_font(w);
  return awra_text_width(&f, "abcdefghijklmnopqrstuvwxyz", 26) / 26.0f;
}

static void label_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  LabelExt *x = LX(w);
  label_sync(w);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    int mn = awra_text_layout_min_width(x->layout);
    int nt = awra_text_layout_natural_width(x->layout);
    const float cw = (x->width_chars > 0 || x->max_width_chars > 0) ? char_width(w) : 0;
    if (x->width_chars > 0) {
      mn = BK_MAX(mn, (int)ceilf(cw * x->width_chars));
    }
    if (x->max_width_chars > 0 && (x->wrap != AWRA_WRAP_NONE || x->ellipsize)) {
      nt = BK_MIN(nt, (int)ceilf(cw * x->max_width_chars));
    }
    *r_min = mn;
    *r_nat = BK_MAX(mn, nt);
    return;
  }
  int h;
  const bool limited = x->wrap != AWRA_WRAP_NONE;
  awra_text_layout_size(x->layout, limited ? for_size : -1, NULL, &h);
  if (x->wrap != AWRA_WRAP_NONE && for_size < 0) {
    /* without a width: the height at the natural width */
    int nw = awra_text_layout_natural_width(x->layout);
    if (x->max_width_chars > 0) {
      nw = BK_MIN(nw, (int)ceilf(char_width(w) * x->max_width_chars));
    }
    awra_text_layout_size(x->layout, nw, NULL, &h);
  }
  *r_min = *r_nat = h;
}

static AwraRGBA label_color(AwraWidget *w)
{
  LabelExt *x = LX(w);
  if (x->has_color) {
    return x->color;
  }
  AwraRGBA c = awra_typography_color(awra_widget_typography(w));
  int weight;
  if (!awra_aux(w)->typography_set || awra_widget_typography(w) == AWRA_TYPOGRAPHY_BODY ||
      awra_widget_typography(w) == AWRA_TYPOGRAPHY_LABEL)
  {
    AwraRGBA ic;
    if (awra_inherited_style(w, &ic, &weight) && ic.a > 0) {
      c = ic;
    }
  }
  if (awra_has_class_bit(w, AWRA_CB_DIM)) {
    c.a *= 0.55f;
  }
  if (awra_has_class_bit(w, AWRA_CB_ACCENT)) {
    c = awra_tokens()->accent_content;
  }
  if (awra_has_class_bit(w, AWRA_CB_ERROR)) {
    c = awra_tokens()->danger;
  }
  if (awra_has_class_bit(w, AWRA_CB_SUCCESS)) {
    c = awra_tokens()->success;
  }
  if (awra_has_class_bit(w, AWRA_CB_WARNING)) {
    c = awra_tokens()->warning;
  }
  return c;
}

static void label_draw(AwraWidget *w, AwraDraw *d)
{
  LabelExt *x = LX(w);
  if (x->style_gen != awra_style_generation()) {
    label_sync(w);
  }
  int tw, th;
  const bool fit = x->wrap != AWRA_WRAP_NONE || x->ellipsize != AWRA_ELLIPSIZE_NONE;
  awra_text_layout_size(x->layout, fit ? w->w : -1, &tw, &th);
  const int bw = fit ? BK_MIN(tw, w->w) : tw;
  const float lx = (float)w->x + floorf((w->w - bw) * x->xalign + 0.5f);
  const float ly = (float)w->y + floorf((w->h - th) * x->yalign + 0.5f);
  const AwraRGBA c = label_color(w);
  awra_text_layout_draw(x->layout, d, lx, ly, fit ? w->w : -1, &c);
}

static void label_style_changed(AwraWidget *w)
{
  label_sync(w);
}

static bool label_set_string(AwraWidget *w, const char *name, const char *v)
{
  if (!strcmp(name, "label") || !strcmp(name, "text")) {
    awra_label_set_text(w, v);
    return true;
  }
  if (!strcmp(name, "markup")) {
    awra_label_set_markup(w, v);
    return true;
  }
  return false;
}

static const char *label_get_string(AwraWidget *w, const char *name)
{
  if (!strcmp(name, "label") || !strcmp(name, "text")) {
    return awra_text_layout_text(LX(w)->layout);
  }
  return NULL;
}

static AwraClass label_class = {
    .bk = AWRA_BK("AwraLabel", 0),
    .type = "AwraLabel",
    .ext_size = sizeof(LabelExt),
    .init = label_init,
    .finalize = label_finalize,
    .measure = label_measure,
    .draw = label_draw,
    .style_changed = label_style_changed,
    .set_string = label_set_string,
    .get_string = label_get_string,
};

AwraWidget *awra_label_new(const char *text)
{
  AwraWidget *w = awra_widget_new(&label_class);
  awra_label_set_text(w, text);
  return w;
}

#define LABEL_OK(w) awra_widget_is(w, "AwraLabel")

static void label_changed(AwraWidget *w, bool size_may_change)
{
  if (size_may_change) {
    awra_widget_queue_resize(w);
  }
  awra_invalidate(w);
}

void awra_label_set_text(AwraWidget *w, const char *text)
{
  if (!LABEL_OK(w)) {
    return;
  }
  LabelExt *x = LX(w);
  if (!x->markup && x->source && text && !strcmp(x->source, text)) {
    return;
  }
  int ow = awra_text_layout_natural_width(x->layout);
  awra_set_str(&x->source, text ? text : "");
  x->markup = false;
  awra_text_layout_set_text(x->layout, x->source, false);
  /* the same width and one line: no relayout, just repaint (clocks, counters) */
  const bool same = x->wrap == AWRA_WRAP_NONE && !strchr(x->source, '\n') &&
                    awra_text_layout_natural_width(x->layout) == ow && w->w > 0;
  label_changed(w, !same);
}

const char *awra_label_text(AwraWidget *w)
{
  return LABEL_OK(w) ? awra_text_layout_text(LX(w)->layout) : NULL;
}

void awra_label_set_markup(AwraWidget *w, const char *markup)
{
  if (!LABEL_OK(w)) {
    return;
  }
  LabelExt *x = LX(w);
  awra_set_str(&x->source, markup ? markup : "");
  x->markup = true;
  awra_text_layout_set_text(x->layout, x->source, true);
  label_changed(w, true);
}

void awra_label_set_wrap(AwraWidget *w, AwraWrap wrap)
{
  LX(w)->wrap = wrap;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_ellipsize(AwraWidget *w, AwraEllipsize e)
{
  LX(w)->ellipsize = e;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_xalign(AwraWidget *w, float a)
{
  LX(w)->xalign = a;
  awra_invalidate(w);
}

void awra_label_set_yalign(AwraWidget *w, float a)
{
  LX(w)->yalign = a;
  awra_invalidate(w);
}

void awra_label_set_justify(AwraWidget *w, AwraJustify j)
{
  LX(w)->justify = j;
  label_sync(w);
  awra_invalidate(w);
}

void awra_label_set_lines(AwraWidget *w, int lines)
{
  LX(w)->lines = lines;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_max_width_chars(AwraWidget *w, int chars)
{
  LX(w)->max_width_chars = chars;
  label_changed(w, true);
}

void awra_label_set_width_chars(AwraWidget *w, int chars)
{
  LX(w)->width_chars = chars;
  label_changed(w, true);
}

void awra_label_set_selectable(AwraWidget *w, bool selectable)
{
  (void)w;
  (void)selectable;
}

void awra_label_set_color(AwraWidget *w, const AwraRGBA *c)
{
  LabelExt *x = LX(w);
  x->has_color = c != NULL;
  if (c) {
    x->color = *c;
  }
  awra_invalidate(w);
}

void awra_label_set_weight(AwraWidget *w, int weight)
{
  LX(w)->weight = weight;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_scale(AwraWidget *w, double scale)
{
  LX(w)->scale = (float)scale;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_monospace_digits(AwraWidget *w, bool tabular)
{
  (void)w;
  (void)tabular;
}

/* ======================================================================= */
/* Image */

typedef struct ImageExt {
  char *icon;
  int pixel_size; /* logical, -1 = 16 */
  /* pictures */
  char *path;
  uint32_t *argb; /* decoded at the drawn size */
  int iw, ih;     /* intrinsic size */
  int dw, dh;     /* decoded size */
  void *bytes;
  size_t nbytes;
  float radius;
  int fit;
  bool has_color;
  AwraRGBA color;
} ImageExt;

#define IX(w) AWRA_EXT(w, ImageExt)

static void image_finalize(AwraWidget *w)
{
  ImageExt *x = IX(w);
  free(x->icon);
  free(x->path);
  free(x->argb);
  free(x->bytes);
}

static int icon_size(const AwraWidget *w)
{
  const ImageExt *x = IX(w);
  return awra_px(x->pixel_size > 0 ? x->pixel_size : 16);
}

static void image_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  ImageExt *x = IX(w);
  if (x->icon) {
    *r_min = *r_nat = icon_size(w);
    return;
  }
  if (x->pixel_size > 0) {
    *r_min = *r_nat = awra_px(x->pixel_size);
    return;
  }
  const int v = o == AWRA_ORIENTATION_HORIZONTAL ? x->iw : x->ih;
  *r_min = 0;
  *r_nat = awra_px(v);
}

static void decode_at(AwraWidget *w, int dw, int dh)
{
  ImageExt *x = IX(w);
  if ((x->dw == dw && x->dh == dh && x->argb) || (!x->path && !x->bytes)) {
    return;
  }
  BKImBuf *ib = x->path ? BK_imbuf_load(x->path, dw, dh) : BK_imbuf_decode(x->bytes, x->nbytes, dw, dh);
  if (!ib) {
    return;
  }
  int iw, ih;
  BK_imbuf_size(ib, &iw, &ih);
  free(x->argb);
  x->argb = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)iw * ih);
  if (x->argb) {
    memcpy(x->argb, BK_imbuf_pixels(ib, 0), sizeof(uint32_t) * (size_t)iw * ih);
  }
  x->dw = iw;
  x->dh = ih;
  BK_imbuf_free(ib);
}

static void image_draw(AwraWidget *w, AwraDraw *d)
{
  ImageExt *x = IX(w);
  if (x->icon) {
    const int s = icon_size(w);
    AwraRGBA c = awra_tokens()->foreground;
    int weight;
    if (x->has_color) {
      c = x->color;
    }
    else if (!awra_inherited_style(w, &c, &weight)) {
      c = awra_typography_color(awra_widget_typography(w));
    }
    if (!awra_sensitive(w)) {
      c.a *= 0.5f;
    }
    const float ix = w->x + floorf((w->w - s) * 0.5f), iy = w->y + floorf((w->h - s) * 0.5f);
    if (awra_icon_mask(x->icon, s)) {
      awra_paint_icon(d, x->icon, ix, iy, s, &c);
    }
    else {
      const BuiPixmap *pm = awra_icon_pixmap(x->icon, s);
      if (pm) {
        BUI_raster_image(d->r, ix, iy, (float)pm->w, (float)pm->h, pm, NULL, d->opacity, 0);
      }
    }
    return;
  }
  if (!x->iw || !x->ih || w->w <= 0 || w->h <= 0) {
    return;
  }
  /* contain (default) / cover / fill in the allocation */
  float bw = (float)w->w, bh = (float)w->h, bx = (float)w->x, by = (float)w->y;
  const float ar = (float)x->iw / x->ih, br = bw / bh;
  if (x->fit != 0) {
    const bool contain = x->fit == 1;
    if ((ar > br) == contain) {
      const float nh = bw / ar;
      by += (bh - nh) * 0.5f;
      bh = nh;
    }
    else {
      const float nw = bh * ar;
      bx += (bw - nw) * 0.5f;
      bw = nw;
    }
  }
  decode_at(w, (int)ceilf(bw), (int)ceilf(bh));
  if (!x->argb) {
    return;
  }
  const BuiPixmap pm = {x->argb, x->dw, x->dh, (uint64_t)(uintptr_t)x->argb ^ (uint64_t)x->dw << 40,
                        BUI_PIXMAP_ARGB, 0};
  /* cover: crop the source to the box (drawn clipped to the allocation) */
  if (x->fit == 2) {
    bk_clip_push(w->x, w->y, w->w, w->h);
  }
  const float r = awra_pxf(x->radius);
  const float radii[4] = {r, r, r, r};
  BUI_raster_image_rounded(d->r, bx, by, bw, bh, &pm, NULL, d->opacity, radii);
  if (x->fit == 2) {
    bk_clip_pop();
  }
}

static AwraClass image_class = {
    .bk = AWRA_BK("AwraImage", 0),
    .type = "AwraImage",
    .ext_size = sizeof(ImageExt),
    .finalize = image_finalize,
    .measure = image_measure,
    .draw = image_draw,
};

AwraWidget *awra_image_new(void)
{
  AwraWidget *w = awra_widget_new(&image_class);
  IX(w)->pixel_size = -1;
  IX(w)->fit = 1;
  return w;
}

AwraWidget *awra_image_new_from_icon_name(const char *icon)
{
  AwraWidget *w = awra_image_new();
  awra_image_set_icon_name(w, icon);
  return w;
}

void awra_image_set_icon_name(AwraWidget *w, const char *icon)
{
  ImageExt *x = IX(w);
  if (x->icon && icon && !strcmp(x->icon, icon)) {
    return;
  }
  awra_set_str(&x->icon, icon && icon[0] ? icon : NULL);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_pixel_size(AwraWidget *w, int size)
{
  IX(w)->pixel_size = size;
  awra_widget_queue_resize(w);
}

static void picture_reset(AwraWidget *w)
{
  ImageExt *x = IX(w);
  free(x->icon);
  x->icon = NULL;
  free(x->argb);
  x->argb = NULL;
  x->dw = x->dh = 0;
}

void awra_image_set_file(AwraWidget *w, const char *path)
{
  ImageExt *x = IX(w);
  picture_reset(w);
  free(x->bytes);
  x->bytes = NULL;
  awra_set_str(&x->path, path);
  BKImageInfo info = {0};
  if (path && BK_image_probe_file(path, &info)) {
    x->iw = info.width;
    x->ih = info.height;
  }
  else {
    x->iw = x->ih = 0;
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_bytes(AwraWidget *w, const void *data, size_t len)
{
  ImageExt *x = IX(w);
  picture_reset(w);
  free(x->path);
  x->path = NULL;
  free(x->bytes);
  x->bytes = malloc(len);
  if (x->bytes) {
    memcpy(x->bytes, data, len);
  }
  x->nbytes = len;
  BKImageInfo info = {0};
  if (x->bytes && BK_image_probe(x->bytes, len, &info)) {
    x->iw = info.width;
    x->ih = info.height;
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_rgba(AwraWidget *w, const uint8_t *rgba, int width, int height, int stride)
{
  ImageExt *x = IX(w);
  picture_reset(w);
  free(x->path);
  x->path = NULL;
  free(x->bytes);
  x->bytes = NULL;
  BKImBuf *ib = BK_imbuf_from_rgba(rgba, width, height, stride);
  if (!ib) {
    return;
  }
  x->argb = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)width * height);
  if (x->argb) {
    memcpy(x->argb, BK_imbuf_pixels(ib, 0), sizeof(uint32_t) * (size_t)width * height);
  }
  BK_imbuf_free(ib);
  x->iw = x->dw = width;
  x->ih = x->dh = height;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_corner_radius(AwraWidget *w, double r)
{
  IX(w)->radius = (float)r;
  awra_invalidate(w);
}

void awra_image_set_content_fit(AwraWidget *w, int fit)
{
  IX(w)->fit = fit;
  awra_invalidate(w);
}

void awra_image_set_color(AwraWidget *w, const AwraRGBA *c)
{
  ImageExt *x = IX(w);
  x->has_color = c != NULL;
  if (c) {
    x->color = *c;
  }
  awra_invalidate(w);
}
