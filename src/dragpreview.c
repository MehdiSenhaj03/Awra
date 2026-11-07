/* Awra 2 drag preview (Awra 1's AwraDragPreview): what a drag carries, as
 * a pile of up to three items (icons or thumbnails), the first in front,
 * the two others fanned out behind it, with a count badge when there are
 * several. The pile fans out when it appears, settling as if in the hand.
 *
 * It can be the image a window shows under the pointer during a drag
 * (awra_drag_preview_set_drag_window): it then keeps that image up to
 * date while it fans out or an item changes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include "GPU_raster.h"

#include <stdio.h>

#define PILE_SIZE 3
#define FAN_MS 220
#define SHADOW_SAMPLES 17

static const float layer_angle[PILE_SIZE] = {0.0f, -9.0f, 10.0f};
static const float layer_shift[PILE_SIZE] = {0.0f, -0.20f, 0.20f};

typedef struct Item {
  int tex;  /* BUI texture (straight RGBA, a transparent pixel around), 0 none */
  int w, h; /* its content, without the border */
} Item;

typedef struct PreviewExt {
  Item items[PILE_SIZE];
  int icon_size; /* logical px */
  unsigned count;
  double spread;
  bool fanning;
  AwraWidget *window; /* whose drag image it is (weak: cleared when it goes) */
  int hot_x, hot_y;
} PreviewExt;

#define PVX(w) AWRA_EXT(w, PreviewExt)

static int extent(const PreviewExt *x)
{
  /* room for the fan and the badge: it never resizes in flight */
  return x->icon_size + x->icon_size / 2 + 12;
}

static void preview_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)o;
  (void)for_size;
  *r_min = *r_nat = awra_px(extent(PVX(w)));
}

static void item_free(Item *it)
{
  if (it->tex) {
    BUI_texture_free(it->tex);
  }
  *it = (Item){0};
}

/* One item: a quad turned by `angle` degrees around (cx, cy), raster px;
 * `color` multiplies the texels (ImGui's packed ABGR). */
static void draw_quad(BuiRaster *r, const Item *it, float cx, float cy, float iw, float ih,
                      float angle, uint32_t color, float ox, float oy)
{
  /* the texture's transparent border, drawn too: smooth turned edges */
  const float bx = iw / (float)it->w, by = ih / (float)it->h;
  const float hw = iw * 0.5f + bx, hh = ih * 0.5f + by;
  const float a = angle * (float)M_PI / 180.0f, c = cosf(a), s = sinf(a);
  const float corners[4][2] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
  const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  BuiMeshVertex v[4];
  for (int i = 0; i < 4; i++) {
    v[i].x = cx + corners[i][0] * c - corners[i][1] * s;
    v[i].y = cy + corners[i][0] * s + corners[i][1] * c;
    v[i].u = uv[i][0];
    v[i].v = uv[i][1];
    v[i].col = color;
  }
  static const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
  BUI_raster_mesh(r, v, 4, idx, 6, 0, it->tex, NULL, ox, oy);
}

static uint32_t pack(float r, float g, float b, float a)
{
  const uint32_t R = (uint32_t)(r * 255.0f + 0.5f), G = (uint32_t)(g * 255.0f + 0.5f);
  const uint32_t B = (uint32_t)(b * 255.0f + 0.5f), A = (uint32_t)(a * 255.0f + 0.5f);
  return R | G << 8 | B << 16 | A << 24;
}

/* The pile at (x, y), raster px. */
static void draw_pile(AwraWidget *w, AwraDraw *d, float x, float y)
{
  const PreviewExt *px = PVX(w);
  const float e = awra_pxf(extent(px)), size = awra_pxf(px->icon_size);
  /* GTK's box-shadow of the item's shape, 0 3 7 rgba(0,0,0,.42): the
   * shape drawn dark a few times around, a soft edge */
  static float ring[SHADOW_SAMPLES][2];
  static bool ring_ready;
  if (!ring_ready) {
    ring[0][0] = ring[0][1] = 0.0f;
    for (int i = 0; i < 8; i++) {
      const float a = (float)i * (float)M_PI / 4.0f;
      ring[1 + i][0] = 2.0f * cosf(a);
      ring[1 + i][1] = 2.0f * sinf(a);
      ring[9 + i][0] = 4.0f * cosf(a + (float)M_PI / 8.0f);
      ring[9 + i][1] = 4.0f * sinf(a + (float)M_PI / 8.0f);
    }
    ring_ready = true;
  }
  const float shade = 1.0f - powf(1.0f - 0.42f, 1.0f / SHADOW_SAMPLES);
  /* back to front: the first item stays on top */
  for (int layer = PILE_SIZE - 1; layer >= 0; layer--) {
    const Item *it = &px->items[layer];
    if (!it->tex) {
      continue;
    }
    const float k = layer == 0 ? 1.0f : 0.88f;
    const float s = size * k;
    const float ratio = (float)it->w / (float)it->h;
    const float iw = ratio > 1.0f ? s : s * ratio, ih = ratio > 1.0f ? s / ratio : s;
    const float cx = x + e * 0.5f + layer_shift[layer] * size * (float)px->spread;
    const float cy = y + e * 0.5f;
    const float angle = layer_angle[layer] * (float)px->spread;
    const uint32_t dark = pack(0, 0, 0, shade * d->opacity);
    for (int i = 0; i < SHADOW_SAMPLES; i++) {
      draw_quad(d->r, it, cx, cy, iw, ih, angle, dark, awra_pxf(ring[i][0]),
                awra_pxf(3.0 + ring[i][1]));
    }
    draw_quad(d->r, it, cx, cy, iw, ih, angle, pack(1, 1, 1, d->opacity), 0, 0);
  }
  if (px->count < 2) {
    return;
  }
  /* the count: awra.css's .awra-drag-preview-count */
  char text[16];
  if (px->count > 999) {
    snprintf(text, sizeof(text), "999+");
  }
  else {
    snprintf(text, sizeof(text), "%u", px->count);
  }
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.size = awra_pxf(12);
  f.weight = 700;
  const float tw = awra_text_width(&f, text, (int)strlen(text));
  const float lh = awra_font_line_height(&f);
  const float bw = fmaxf(awra_pxf(14), tw) + awra_pxf(14), bh = fmaxf(awra_pxf(14), lh) + awra_pxf(6);
  const float bxp = x + fminf(e - bw, e * 0.5f + size * 0.5f - bw * 0.5f);
  const float byp = y + fmaxf(0.0f, (e - size) * 0.5f - bh * 0.5f);
  const AwraBox badge = {floorf(bxp), floorf(byp), ceilf(bw), ceilf(bh)};
  const AwraRGBA shadow = awra_rgba(10.0f / 255, 8.0f / 255, 18.0f / 255, 0.35f);
  awra_paint_shadow(d, badge, bh * 0.5f, 0, awra_pxf(2), 0, awra_pxf(6), &shadow, true);
  awra_paint_rbox(d, badge, bh * 0.5f, &awra_tokens()->accent, 0, NULL);
  const float base = badge.y + (badge.h - lh) * 0.5f + awra_font_ascent(&f);
  awra_draw_text(d, &f, badge.x + (badge.w - tw) * 0.5f, roundf(base), text, (int)strlen(text),
                 &awra_tokens()->on_accent);
}

static void preview_draw(AwraWidget *w, AwraDraw *d)
{
  draw_pile(w, d, (float)w->x, (float)w->y);
}

AwraTexture *awra_drag_preview_texture(AwraWidget *w)
{
  const int e = awra_px(extent(PVX(w)));
  BuiRaster *r = BUI_raster_create(e, e);
  if (!r) {
    return NULL;
  }
  BUI_raster_set_diff(r, 0);
  BuiRaster *saved = bk_draw.r;
  bk_draw.r = r;
  bk_clip_reset(e, e);
  AwraDraw d = {r, 1.0f};
  draw_pile(w, &d, 0, 0);
  bk_draw.r = saved;
  if (saved) {
    bk_clip_reset(BUI_raster_width(saved), BUI_raster_height(saved));
  }
  AwraTexture *t = awra_texture_new_argb(BUI_raster_pixels(r), e, e);
  BUI_raster_free(r);
  return t;
}

/* The window's drag image follows what the pile looks like now. */
static void sync_drag_icon(AwraWidget *w)
{
  PreviewExt *x = PVX(w);
  if (!x->window) {
    return;
  }
  AwraTexture *t = awra_drag_preview_texture(w);
  if (t) {
    awra_window_set_drag_icon(x->window, t, awra_px(x->hot_x), awra_px(x->hot_y));
    awra_texture_unref(t);
  }
}

static void changed(AwraWidget *w)
{
  awra_invalidate(w);
  sync_drag_icon(w);
}

static void fan_anim(AwraWidget *w, double t, void *data)
{
  (void)t;
  PreviewExt *x = PVX(w);
  const double *started = (const double *)data;
  double p = (awra_now_ms() - *started) / FAN_MS;
  p = p < 0 ? 0 : p > 1 ? 1 : p;
  /* eased out with a slight overshoot, as if the pile settled in the hand */
  x->spread = 1.0 + 2.2 * pow(p - 1.0, 3.0) + 1.2 * pow(p - 1.0, 2.0);
  changed(w);
}

static void fan_done(void *data)
{
  double *started = (double *)data;
  AwraWidget *w = *(AwraWidget **)(started + 1);
  free(started);
  PVX(w)->spread = 1.0;
  PVX(w)->fanning = false;
  changed(w);
  awra_widget_unref(w);
}

static void start_fan(AwraWidget *w)
{
  PreviewExt *x = PVX(w);
  if (x->fanning || x->count < 2) {
    return;
  }
  const bool shown = awra_widget_mapped(w) || x->window;
  if (awra_style_reduced_motion() || !shown) {
    x->spread = awra_style_reduced_motion() ? 1.0 : 0.0;
    return;
  }
  /* its start time and the widget, for the frames and the end */
  double *data = (double *)malloc(sizeof(double) + sizeof(AwraWidget *));
  if (!data) {
    x->spread = 1.0;
    return;
  }
  data[0] = awra_now_ms();
  *(AwraWidget **)(data + 1) = w;
  x->spread = 0.0;
  x->fanning = true;
  awra_widget_ref(w);
  awra_anim_start(w, FAN_MS, fan_anim, data, fan_done);
}

static void preview_map(AwraWidget *w, bool mapped)
{
  if (mapped) {
    start_fan(w);
  }
}

static void window_gone(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  PVX(w)->window = NULL;
}

static void preview_finalize(AwraWidget *w)
{
  PreviewExt *x = PVX(w);
  if (x->window) {
    awra_window_set_drag_icon(x->window, NULL, 0, 0);
    awra_widget_set_data(x->window, "awra-drag-preview", NULL, NULL);
  }
  for (int i = 0; i < PILE_SIZE; i++) {
    item_free(&x->items[i]);
  }
}

static AwraClass preview_class = {
    .bk = AWRA_BK("AwraDragPreview", 0),
    .type = "AwraDragPreview",
    .ext_size = sizeof(PreviewExt),
    .measure = preview_measure,
    .draw = preview_draw,
    .map = preview_map,
    .finalize = preview_finalize,
};

AwraWidget *awra_drag_preview_new(void)
{
  AwraWidget *w = awra_widget_new(&preview_class);
  PreviewExt *x = PVX(w);
  x->icon_size = 64;
  x->count = 1;
  x->spread = 1.0;
  awra_widget_set_can_target(w, false);
  return w;
}

void awra_drag_preview_set_icon_size(AwraWidget *w, int size)
{
  PreviewExt *x = PVX(w);
  size = size < 16 ? 16 : size > 512 ? 512 : size;
  if (x->icon_size != size) {
    x->icon_size = size;
    awra_widget_queue_resize(w);
    changed(w);
  }
}

void awra_drag_preview_set_count(AwraWidget *w, unsigned count)
{
  PreviewExt *x = PVX(w);
  if (x->count == count) {
    return;
  }
  x->count = count;
  start_fan(w);
  changed(w);
}

void awra_drag_preview_set_item(AwraWidget *w, int index, const AwraTexture *texture)
{
  PreviewExt *x = PVX(w);
  if (index < 0 || index >= PILE_SIZE) {
    return;
  }
  item_free(&x->items[index]);
  const int tw = texture ? awra_texture_width(texture) : 0, th = texture ? awra_texture_height(texture) : 0;
  if (tw > 0 && th > 0 && (size_t)tw * th <= (1u << 24)) {
    /* straight RGBA with a transparent pixel around */
    const int bw = tw + 2, bh = th + 2;
    uint8_t *rgba = (uint8_t *)calloc((size_t)bw * bh, 4);
    /* straight RGBA bytes, inside the border */
    if (rgba && awra_texture_download(texture, rgba + ((size_t)bw + 1) * 4, bw * 4)) {
      x->items[index].tex = BUI_texture_create(bw, bh, rgba, 0);
      x->items[index].w = tw;
      x->items[index].h = th;
    }
    free(rgba);
  }
  changed(w);
}

void awra_drag_preview_set_item_icon(AwraWidget *w, int index, const char *icon_name)
{
  AwraTexture *t = icon_name ? awra_texture_new_for_icon(icon_name, PVX(w)->icon_size) : NULL;
  awra_drag_preview_set_item(w, index, t);
  if (t) {
    awra_texture_unref(t);
  }
}

void awra_drag_preview_set_drag_window(AwraWidget *w, AwraWidget *window, int hot_x, int hot_y)
{
  PreviewExt *x = PVX(w);
  if (x->window && x->window != window) {
    awra_window_set_drag_icon(x->window, NULL, 0, 0);
    awra_widget_set_data(x->window, "awra-drag-preview", NULL, NULL);
  }
  x->window = window && awra_widget_is(window, "AwraWindow") ? window : NULL;
  x->hot_x = hot_x;
  x->hot_y = hot_y;
  if (x->window) {
    /* forgotten if the window goes first */
    awra_widget_set_data(x->window, "awra-drag-preview", w, window_gone);
    start_fan(w);
    sync_drag_icon(w);
  }
}
