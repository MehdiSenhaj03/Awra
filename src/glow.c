/* The "suggested" style role: the icon of what the person will probably
 * want next glows in the accent colour, apart from the selection.
 *
 * Awra 1 (awra.css, "What the person will probably want next"):
 *   .awra-role-suggested image {
 *     -gtk-icon-shadow: 0 0 10px alpha(@awra_accent, 1), 0 0 3px alpha(@awra_accent, 0.9);
 *   }
 *   .awra-role-collection-view image,
 *   .awra-role-canvas-table image { transition: -gtk-icon-shadow 500ms ease-out; }
 *
 * GSK draws an icon shadow as the icon's alpha blurred by a Gaussian of
 * sigma = radius / 2, in the shadow's colour, under the icon. Both shadows
 * share the accent: painted one over the other they are one mask,
 * 1 - (1 - a10) (1 - 0.9 a3). A transition from none interpolates from a
 * transparent shadow of radius 0 (gtk_css_shadow_value_new_for_transition):
 * at progress p the radii are 10p and 3p and the alpha p.
 *
 * The resting mask is computed once per icon and size; frames of a
 * transition compute theirs (one icon at a time glows). */

#include "awra-private.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GLOW_MS 500
#define GLOW_KEY "awra-glow"

typedef struct Glow {
  bool on;         /* the role applies */
  double from, to; /* progress of the transition, 0 none .. 1 lit */
  double p;        /* current progress (eased) */
  bool running;
  /* the resting mask (p = 1) */
  uint8_t *mask;
  int mw, mh, pad;
  const void *src; /* the alpha it was made from (icon caches keep theirs) */
  int sw, sh;
} Glow;

static void glow_free(void *data)
{
  Glow *g = (Glow *)data;
  free(g->mask);
  free(g);
}

static int g_suggested_bit = -2;

static void bits(void)
{
  if (g_suggested_bit == -2) {
    g_suggested_bit = awra_class_bit("suggested");
  }
}

/* Awra 1 transitioned the shadow in its collection views and tables (the
 * collection-view and canvas-table roles): Awra 2's AwraCollectionView */
static bool in_collection(const AwraWidget *w)
{
  for (const BKNode *n = w; n; n = n->parent) {
    if (awra_is_widget((BKNode *)n)) {
      const AwraClass *c = (const AwraClass *)n->cls;
      if (c->type && !strcmp(c->type, "AwraCollectionView")) {
        return true;
      }
    }
  }
  return false;
}

static bool ancestor_has(const AwraWidget *w, int bit)
{
  if (bit < 0) {
    return false;
  }
  for (const BKNode *n = w; n; n = n->parent) {
    if (awra_is_widget((BKNode *)n) && awra_has_class_bit((const AwraWidget *)n, bit)) {
      return true;
    }
  }
  return false;
}

/* CSS ease-out: cubic-bezier(0, 0, 0.58, 1) */
static double ease_out(double t)
{
  if (t <= 0.0 || t >= 1.0) {
    return t <= 0.0 ? 0.0 : 1.0;
  }
  const double x1 = 0.0, x2 = 0.58, y1 = 0.0, y2 = 1.0;
  double u = t;
  for (int i = 0; i < 8; i++) {
    const double x = 3 * x1 * u * (1 - u) * (1 - u) + 3 * x2 * u * u * (1 - u) + u * u * u - t;
    const double dx = 3 * x1 * (1 - u) * (1 - 3 * u) + 3 * x2 * u * (2 - 3 * u) + 3 * u * u;
    if (fabs(x) < 1e-6 || fabs(dx) < 1e-9) {
      break;
    }
    u -= x / dx;
  }
  u = u < 0 ? 0 : u > 1 ? 1 : u;
  return 3 * y1 * u * (1 - u) * (1 - u) + 3 * y2 * u * u * (1 - u) + u * u * u;
}

/* one separable Gaussian pass over float rows (sigma in pixels) */
static void blur_pass(const float *in, float *out, int w, int h, int stride_in, int step_in, int stride_out,
                      int step_out, const float *k, int r)
{
  for (int y = 0; y < h; y++) {
    const float *row = in + (size_t)y * stride_in;
    float *dst = out + (size_t)y * stride_out;
    for (int x = 0; x < w; x++) {
      float acc = 0.0f;
      const int lo = x - r < 0 ? -x : -r, hi = x + r >= w ? w - 1 - x : r;
      for (int i = lo; i <= hi; i++) {
        acc += row[(x + i) * step_in] * k[i + r];
      }
      dst[x * step_out] = acc;
    }
  }
}

/* alpha (0..1) of the icon blurred by sigma, into `out` (mw x mh, icon at pad) */
static void blurred(const float *src, int mw, int mh, double sigma, float *out, float *tmp)
{
  if (sigma < 0.05) {
    memcpy(out, src, sizeof(float) * (size_t)mw * mh);
    return;
  }
  int r = (int)ceil(sigma * 3.0);
  float *k = (float *)malloc(sizeof(float) * (size_t)(2 * r + 1));
  if (!k) {
    memcpy(out, src, sizeof(float) * (size_t)mw * mh);
    return;
  }
  double sum = 0.0;
  for (int i = -r; i <= r; i++) {
    k[i + r] = (float)exp(-(double)i * i / (2.0 * sigma * sigma));
    sum += k[i + r];
  }
  for (int i = 0; i <= 2 * r; i++) {
    k[i] = (float)(k[i] / sum);
  }
  blur_pass(src, tmp, mw, mh, mw, 1, mw, 1, k, r);  /* rows */
  blur_pass(tmp, out, mh, mw, 1, mw, 1, mw, k, r);  /* columns (transposed walk) */
  free(k);
}

/* The two shadows at progress p as one 8-bit mask (alpha already includes p). */
static uint8_t *make_mask(const uint8_t *a8, const uint32_t *argb, int sw, int sh, int pad, double p,
                          int *r_w, int *r_h)
{
  const int mw = sw + 2 * pad, mh = sh + 2 * pad;
  float *src = (float *)calloc((size_t)mw * mh, sizeof(float));
  float *b10 = (float *)malloc(sizeof(float) * (size_t)mw * mh);
  float *b3 = (float *)malloc(sizeof(float) * (size_t)mw * mh);
  float *tmp = (float *)malloc(sizeof(float) * (size_t)mw * mh);
  uint8_t *m = (uint8_t *)malloc((size_t)mw * mh);
  if (!src || !b10 || !b3 || !tmp || !m) {
    free(src), free(b10), free(b3), free(tmp), free(m);
    return NULL;
  }
  for (int y = 0; y < sh; y++) {
    for (int x = 0; x < sw; x++) {
      const unsigned v = a8 ? a8[y * sw + x] : argb[y * sw + x] >> 24;
      src[(y + pad) * mw + x + pad] = v / 255.0f;
    }
  }
  const double s10 = awra_pxf(10.0) * 0.5 * p, s3 = awra_pxf(3.0) * 0.5 * p;
  blurred(src, mw, mh, s10, b10, tmp);
  blurred(src, mw, mh, s3, b3, tmp);
  for (size_t i = 0; i < (size_t)mw * mh; i++) {
    const float a = 1.0f - (1.0f - (float)p * b10[i]) * (1.0f - 0.9f * (float)p * b3[i]);
    m[i] = (uint8_t)(a <= 0.0f ? 0 : a >= 1.0f ? 255 : a * 255.0f + 0.5f);
  }
  free(src), free(b10), free(b3), free(tmp);
  *r_w = mw;
  *r_h = mh;
  return m;
}

static void glow_step(AwraWidget *w, double eased_cubic, void *data)
{
  Glow *g = (Glow *)data;
  /* the frame clock eases by a cubic: back to linear time, then CSS ease-out */
  const double t = 1.0 - cbrt(1.0 - (eased_cubic < 0 ? 0 : eased_cubic > 1 ? 1 : eased_cubic));
  g->p = g->from + (g->to - g->from) * ease_out(t);
  g->running = t < 1.0;
  awra_invalidate(w);
}

static bool glow_start(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  Glow *g = (Glow *)awra_widget_get_data(w, GLOW_KEY);
  if (g && g->running && awra_window_of(w)) {
    awra_anim_stop_for(w);
    g->from = g->p;
    awra_anim_start(w, GLOW_MS, glow_step, g, NULL);
  }
  return false;
}

bool awra_icon_glow_wanted(const AwraWidget *w)
{
  bits();
  return ancestor_has(w, g_suggested_bit) || awra_widget_get_data(w, GLOW_KEY) != NULL;
}

void awra_icon_glow_draw(AwraWidget *w, AwraDraw *d, const uint8_t *a8, const uint32_t *argb, int sw, int sh,
                         float ix, float iy, bool fresh)
{
  bits();
  if ((!a8 && !argb) || sw <= 0 || sh <= 0) {
    return;
  }
  const bool on = ancestor_has(w, g_suggested_bit);
  Glow *g = (Glow *)awra_widget_get_data(w, GLOW_KEY);
  if (!g) {
    if (!on) {
      return;
    }
    g = (Glow *)calloc(1, sizeof(Glow));
    if (!g) {
      return;
    }
    /* an icon drawn for the first time has no previous style to move from:
     * lit at once; one already shown (a recycled cell) moves to it */
    g->on = fresh;
    g->p = g->from = g->to = fresh ? 1.0 : 0.0;
    awra_widget_set_data(w, GLOW_KEY, g, glow_free);
    const int pad = (int)ceil(awra_pxf(10.0) * 0.5 * 3.0) + 1;
    if (w->overdraw < pad) {
      w->overdraw = (short)pad;
      bk_relayout(w); /* the subtree's spill: damage and culling cover the glow */
    }
  }
  if (g->on != on) {
    g->on = on;
    const bool animate = in_collection(w);
    awra_anim_stop_for(w);
    g->from = g->p;
    g->to = on ? 1.0 : 0.0;
    if (animate) {
      /* a frame requested while this one draws is lost: the transition
       * starts right after it */
      g->running = true;
      awra_timeout_add(0, glow_start, awra_widget_ref(w), (AwraDestroyNotify)awra_widget_unref);
    }
    else {
      g->p = g->to;
      g->running = false;
    }
  }
  if (g->p <= 0.0) {
    return;
  }
  const int pad = (int)ceil(awra_pxf(10.0) * 0.5 * 3.0) + 1;
  int mw = 0, mh = 0;
  const uint8_t *m;
  uint8_t *frame = NULL;
  if (g->p >= 1.0) {
    if (!g->mask || g->src != (a8 ? (const void *)a8 : (const void *)argb) || g->sw != sw || g->sh != sh ||
        g->pad != pad) {
      free(g->mask);
      g->mask = make_mask(a8, argb, sw, sh, pad, 1.0, &g->mw, &g->mh);
      g->src = a8 ? (const void *)a8 : (const void *)argb;
      g->sw = sw;
      g->sh = sh;
      g->pad = pad;
    }
    m = g->mask;
    mw = g->mw;
    mh = g->mh;
  }
  else {
    frame = make_mask(a8, argb, sw, sh, pad, g->p, &mw, &mh);
    m = frame;
  }
  if (m) {
    const AwraRGBA a = awra_tokens()->accent;
    float c[4] = {a.r, a.g, a.b, a.a * d->opacity};
    BUI_raster_mask(d->r, (int)floorf(ix + 0.5f) + awra_shift_ix() - pad, (int)floorf(iy + 0.5f) + awra_shift_iy() - pad, m,
                    mw, mh, mw, c);
  }
  free(frame);
}

/* An image's glow as its style says now, without a transition (its state
 * changed with the item a recycled cell shows, not for the item). */
static void settle(AwraWidget *w)
{
  if (!awra_widget_is_image_internal(w)) {
    return;
  }
  const bool on = ancestor_has(w, g_suggested_bit);
  Glow *g = (Glow *)awra_widget_get_data(w, GLOW_KEY);
  if (!g) {
    if (!on) {
      return;
    }
    g = (Glow *)calloc(1, sizeof(Glow));
    if (!g) {
      return;
    }
    awra_widget_set_data(w, GLOW_KEY, g, glow_free);
    const int pad = (int)ceil(awra_pxf(10.0) * 0.5 * 3.0) + 1;
    if (w->overdraw < pad) {
      w->overdraw = (short)pad;
      bk_relayout(w);
    }
  }
  if (g->running) {
    awra_anim_stop_for(w);
    g->running = false;
  }
  g->on = on;
  g->p = g->from = g->to = on ? 1.0 : 0.0;
  awra_invalidate(w);
}

void awra_widget_finish_transitions(AwraWidget *w)
{
  bits();
  if (!w) {
    return;
  }
  settle(w);
  for (int i = 0; i < w->nkids; i++) {
    if (awra_is_widget(w->kids[i])) {
      awra_widget_finish_transitions(w->kids[i]);
    }
  }
}
