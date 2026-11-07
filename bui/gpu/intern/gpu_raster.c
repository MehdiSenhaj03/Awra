/* BUI software raster backend — see GPU_raster.h.
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "GPU_raster.h"
#include "gpu_d3d11.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TILE_W 64
#define TILE_H 32

struct BuiRaster {
  int w, h;
  uint32_t *pix;  /* current frame */
  uint32_t *prev; /* last presented frame, for damage */
  int full_damage;
  int external; /* BUI-Awra: `pix` belongs to the host (a shared-memory buffer) */
  int nodiff;   /* BUI-Awra: no `prev`: damage is what was drawn (the kit's regions) */
  /* BUI-Awra: the coordinates drawing uses at the buffer's top-left pixel
   * (BUI_raster_set_origin: a popup's own buffer drawn in its window's
   * coordinates); the clip is in these coordinates, damage in the buffer's */
  int ox, oy;

  /* clip, exclusive max, always inside the buffer */
  int cx0, cy0, cx1, cy1;

  int (*damage)[4];
  int damage_len, damage_cap;

  char font_family[64];
  int font_flags;

  BuiGpuSink sink;

  /* GPU mode: primitives are recorded as instances (gpu_d3d11.c) and pix /
   * prev are not allocated (read back on demand by BUI_raster_pixels). */
  BuiGpu *gpu;
  BuiGpu *gpu_pending;
  int gpu_switched; /* the renderer changed: the next frame must be full */
  struct BuiRaster *scratch; /* CPU coverage for polygons in GPU mode */
  /* BUI-Awra: what BUI_raster_read and BUI_raster_corners_save kept on a
   * GPU this frame, by the caller's buffer: the frame snapshot holding it,
   * the part of the frame (x, y, w, h) and where it lies in the snapshot */
  struct {
    const uint32_t *key;
    int snap, x, y, w, h, dx, dy;
  } kept[64];
  int n_kept;
};

/* -------------------------------------------------------------------- */
/** \name Pixel helpers
 * \{ */

typedef struct Paint {
  uint32_t rgb; /* 0xFFRRGGBB: the color at full alpha (BUI-Awra: premultiplied frames) */
  int a;        /* 0..256 */
} Paint;

/* BUI-Awra: row `py` (drawing coordinates) of the buffer, indexed by x in
 * drawing coordinates; only rows and columns inside the clip are touched */
#define RASTER_ROW(r, py) ((r)->pix + (ptrdiff_t)((py) - (r)->oy) * (r)->w - (r)->ox)

static inline int clampi(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline float clampf(float v, float lo, float hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline Paint paint_from(const float c[4])
{
  Paint p;
  int r = (int)(clampf(c[0], 0.0f, 1.0f) * 255.0f + 0.5f);
  int g = (int)(clampf(c[1], 0.0f, 1.0f) * 255.0f + 0.5f);
  int b = (int)(clampf(c[2], 0.0f, 1.0f) * 255.0f + 0.5f);
  p.rgb = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  p.a = (int)(clampf(c[3], 0.0f, 1.0f) * 256.0f + 0.5f);
  return p;
}

/* Blend s over d with factor f in 0..256, on all four channels. With `s`
 * opaque (0xFF alpha) this is premultiplied source-over of s * f: the frame
 * keeps a correct premultiplied alpha channel (Awra: translucent windows),
 * for the cost of the old RGB-only blend (two lanes either way). */
static inline uint32_t blend(uint32_t d, uint32_t s, uint32_t f)
{
  const uint32_t nf = 256 - f;
  const uint32_t rb = (((s & 0xFF00FFu) * f + (d & 0xFF00FFu) * nf) >> 8) & 0xFF00FFu;
  const uint32_t ag = (((s >> 8) & 0xFF00FFu) * f + ((d >> 8) & 0xFF00FFu) * nf) & 0xFF00FF00u;
  return rb | ag;
}

/* BUI-Awra: s over d with factor f / 65280 (paint alpha 0..256 times
 * coverage 0..255), rounded to the nearest level on each channel, as GSK
 * blends glyph coverage (a GPU's unorm blend). */
static inline uint32_t blend_cov(uint32_t d, uint32_t s, uint32_t f)
{
  const uint32_t nf = 65280 - f;
  uint32_t out = 0;
  for (int sh = 0; sh < 32; sh += 8) {
    const uint32_t c = (((s >> sh) & 0xFFu) * f + ((d >> sh) & 0xFFu) * nf + 32640) / 65280;
    out |= c << sh;
  }
  return out;
}

/* Write with coverage cov (0..1) scaled by paint alpha. */
static inline void put_cov(uint32_t *px, Paint p, float cov)
{
  if (cov <= 0.0f) {
    return;
  }
  int f = (int)(cov * (float)p.a + 0.5f);
  if (f >= 256) {
    *px = p.rgb;
  }
  else if (f > 0) {
    *px = blend(*px, p.rgb, (uint32_t)f);
  }
}

/* Integer bbox [x0,x1) x [y0,y1) clipped; returns 0 when empty. */
static int clip_box(const BuiRaster *r, float fx0, float fy0, float fx1, float fy1, int *x0, int *y0,
                    int *x1, int *y1)
{
  if (!(fx1 > fx0) || !(fy1 > fy0)) {
    return 0;
  }
  /* Guard against NaN / huge values before converting to int. */
  fx0 = clampf(fx0, -1e6f, 1e6f);
  fy0 = clampf(fy0, -1e6f, 1e6f);
  fx1 = clampf(fx1, -1e6f, 1e6f);
  fy1 = clampf(fy1, -1e6f, 1e6f);
  *x0 = clampi((int)floorf(fx0), r->cx0, r->cx1);
  *y0 = clampi((int)floorf(fy0), r->cy0, r->cy1);
  *x1 = clampi((int)ceilf(fx1), r->cx0, r->cx1);
  *y1 = clampi((int)ceilf(fy1), r->cy0, r->cy1);
  return *x1 > *x0 && *y1 > *y0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lifetime / frame
 * \{ */

static void raster_reset_clip(BuiRaster *r)
{
  r->cx0 = r->ox;
  r->cy0 = r->oy;
  r->cx1 = r->ox + r->w;
  r->cy1 = r->oy + r->h;
}

static void raster_alloc(BuiRaster *r, int w, int h)
{
  if (r->external) { /* the host attaches a buffer of the new size itself */
    r->external = 0;
    r->pix = NULL;
  }
  free(r->pix);
  free(r->prev);
  r->w = w > 0 ? w : 1;
  r->h = h > 0 ? h : 1;
  if (r->gpu) { /* the frame lives on the GPU */
    r->pix = r->prev = NULL;
    r->full_damage = 1;
    raster_reset_clip(r);
    return;
  }
  r->pix = (uint32_t *)calloc((size_t)r->w * r->h, sizeof(uint32_t));
  r->prev = r->nodiff ? NULL : (uint32_t *)calloc((size_t)r->w * r->h, sizeof(uint32_t));
  r->full_damage = 1;
  raster_reset_clip(r);
}

static void sink_begin(void *u, int w, int h)
{
  BUI_raster_begin_frame((BuiRaster *)u, w, h);
}
static void sink_scissor(void *u, int x, int y, int w, int h)
{
  BUI_raster_set_clip((BuiRaster *)u, x, y, w, h);
}
static void sink_tris(void *u, const float (*co)[2], const float (*col)[4], int n)
{
  BUI_raster_triangles((BuiRaster *)u, co, col, n);
}
static void sink_lines(void *u, const float (*co)[2], const float (*col)[4], int n)
{
  for (int i = 0; i + 1 < n; i += 2) {
    BUI_raster_line((BuiRaster *)u, co[i][0], co[i][1], co[i + 1][0], co[i + 1][1], 1.0f, col[i]);
  }
}
static void sink_points(void *u, const float (*co)[2], const float (*col)[4], int n, float size)
{
  BUI_raster_points((BuiRaster *)u, co, col, n, size);
}
static void sink_text(void *u, float x, float y, const char *text, float size, const float rgba[4])
{
  BUI_raster_text((BuiRaster *)u, x, y, text, (int)strlen(text), size, rgba);
}
static void sink_end(void *u)
{
  BUI_raster_end_frame((BuiRaster *)u);
}

BuiRaster *BUI_raster_create(int width, int height)
{
  BuiRaster *r = (BuiRaster *)calloc(1, sizeof(BuiRaster));
  if (!r) {
    return NULL;
  }
  raster_alloc(r, width, height);
  r->sink.user = r;
  r->sink.begin_frame = sink_begin;
  r->sink.scissor = sink_scissor;
  r->sink.triangles = sink_tris;
  r->sink.lines = sink_lines;
  r->sink.points = sink_points;
  r->sink.text = sink_text;
  r->sink.end_frame = sink_end;
  return r;
}

void BUI_raster_free(BuiRaster *r)
{
  if (r && r->external) {
    r->pix = NULL;
  }
  if (!r) {
    return;
  }
  bui_gpu_free(r->gpu);
  bui_gpu_free(r->gpu_pending);
  BUI_raster_free(r->scratch);
  free(r->pix);
  free(r->prev);
  free(r->damage);
  free(r);
}

const BuiGpuSink *BUI_raster_sink(BuiRaster *r)
{
  return &r->sink;
}

void BUI_raster_begin_frame(BuiRaster *r, int width, int height)
{
  if (width > 0 && height > 0 && (width != r->w || height != r->h)) {
    raster_alloc(r, width, height);
  }
  raster_reset_clip(r);
  r->n_kept = 0;
  if (r->gpu) {
    bui_gpu_begin(r->gpu, r->w, r->h);
  }
}

/* -------------------------------------------------------------------- */
/** \name GPU mode
 * \{ */

static int g_gpu_default = -1;

void BUI_gpu_set_default(int on)
{
  g_gpu_default = on ? 1 : 0;
}

int BUI_gpu_default(void)
{
  const char *env = getenv("BUI_GPU");
  if (env && env[0]) {
    return env[0] == '1' || env[0] == 'y' || env[0] == 'Y' || env[0] == 't' || env[0] == 'T';
  }
  return g_gpu_default > 0;
}

#ifndef BUI_SLUG_DEFAULT
#  define BUI_SLUG_DEFAULT 1
#endif
static int g_slug = -1;

void BUI_gpu_set_slug(int on)
{
  g_slug = on ? 1 : 0;
}

int BUI_gpu_slug(void)
{
  /* called per text run: read the environment once */
  static int env_slug = -2; /* -2 unread, -1 unset */
  if (env_slug == -2) {
    const char *env = getenv("BUI_SLUG");
    env_slug = (env && env[0]) ? (env[0] == '1' || env[0] == 'y' || env[0] == 'Y' || env[0] == 't' ||
                                  env[0] == 'T') :
                                 -1;
  }
  if (env_slug >= 0) {
    return env_slug;
  }
  return g_slug >= 0 ? g_slug : BUI_SLUG_DEFAULT;
}

int BUI_raster_gpu_attach(BuiRaster *r, void *window_handle, int wait)
{
  if (r->gpu || r->gpu_pending) {
    return 1;
  }
  r->gpu_pending = bui_gpu_create(window_handle);
  if (!r->gpu_pending) {
    return 0;
  }
  if (wait) {
    bui_gpu_state(r->gpu_pending, 1);
    BUI_raster_gpu_poll(r);
    return r->gpu != NULL;
  }
  return 1;
}

int BUI_raster_gpu_poll(BuiRaster *r)
{
  if (r->gpu_pending) {
    const int st = bui_gpu_state(r->gpu_pending, 0);
    if (st == 0) {
      return 0;
    }
    if (st > 0) {
      r->gpu = r->gpu_pending;
      r->gpu_pending = NULL;
      raster_alloc(r, r->w, r->h); /* drops the CPU frame */
      bui_gpu_begin(r->gpu, r->w, r->h);
      r->gpu_switched = 1;
    }
    else {
      bui_gpu_free(r->gpu_pending);
      r->gpu_pending = NULL;
    }
  }
  const int sw = r->gpu_switched;
  r->gpu_switched = 0;
  return sw;
}

int BUI_raster_is_gpu(const BuiRaster *r)
{
  return r->gpu != NULL;
}

/* back to the software raster (device lost) */
static void gpu_drop(BuiRaster *r)
{
  bui_gpu_free(r->gpu);
  r->gpu = NULL;
  r->pix = r->prev = NULL;
  raster_alloc(r, r->w, r->h);
  r->gpu_switched = 1;
}

/* Instance over the clipped pixel box; NULL when nothing to draw. */
static BuiGpuInst *gpu_inst(BuiRaster *r, int kind, int x0, int y0, int x1, int y1)
{
  return bui_gpu_inst(r->gpu, kind, x0, y0, x1, y1);
}

static void v4(float d[4], float a, float b, float c, float e)
{
  d[0] = a;
  d[1] = b;
  d[2] = c;
  d[3] = e;
}

static void c4(float d[4], const float *s)
{
  if (s) {
    memcpy(d, s, sizeof(float[4]));
  }
}

/** \} */

static void damage_push(BuiRaster *r, int x, int y, int w, int h)
{
  /* Merge with the previous rect when it is the same column span directly
   * above (common for tall widgets), keeping uploads few and large. */
  if (r->damage_len > 0) {
    int *last = r->damage[r->damage_len - 1];
    if (last[0] == x && last[2] == w && last[1] + last[3] == y) {
      last[3] += h;
      return;
    }
  }
  if (r->damage_len == r->damage_cap) {
    int cap = r->damage_cap ? r->damage_cap * 2 : 64;
    int(*nd)[4] = (int(*)[4])realloc(r->damage, sizeof(int[4]) * (size_t)cap);
    if (!nd) {
      return;
    }
    r->damage = nd;
    r->damage_cap = cap;
  }
  int *d = r->damage[r->damage_len++];
  d[0] = x;
  d[1] = y;
  d[2] = w;
  d[3] = h;
}

void BUI_raster_end_frame(BuiRaster *r)
{
  r->damage_len = 0;
  if (r->gpu) {
    bui_gpu_damage(r->gpu, 0, 0, 0, 0);
    damage_push(r, 0, 0, r->w, r->h);
    r->full_damage = 0;
    return;
  }
  const size_t total = (size_t)r->w * r->h;
  if (r->full_damage || !r->prev) {
    damage_push(r, 0, 0, r->w, r->h);
    if (r->prev) {
      memcpy(r->prev, r->pix, total * sizeof(uint32_t));
    }
    r->full_damage = 0;
    return;
  }
  for (int ty = 0; ty < r->h; ty += TILE_H) {
    const int th = (ty + TILE_H <= r->h) ? TILE_H : r->h - ty;
    int run_x = -1;
    /* One step past the last tile acts as a clean sentinel that closes a
     * run touching the right edge (for any width, not just multiples of
     * TILE_W — otherwise right-edge changes were never reported). */
    for (int tx = 0; tx < r->w + TILE_W; tx += TILE_W) {
      int dirty = 0;
      if (tx < r->w) {
        const int tw = (tx + TILE_W <= r->w) ? TILE_W : r->w - tx;
        for (int y = ty; y < ty + th; y++) {
          const size_t o = (size_t)y * r->w + tx;
          if (memcmp(r->pix + o, r->prev + o, (size_t)tw * sizeof(uint32_t)) != 0) {
            dirty = 1;
            break;
          }
        }
      }
      if (dirty && run_x < 0) {
        run_x = tx;
      }
      else if (!dirty && run_x >= 0) {
        const int end = tx < r->w ? tx : r->w;
        damage_push(r, run_x, ty, end - run_x, th);
        for (int y = ty; y < ty + th; y++) {
          const size_t o = (size_t)y * r->w + run_x;
          memcpy(r->prev + o, r->pix + o, (size_t)(end - run_x) * sizeof(uint32_t));
        }
        run_x = -1;
      }
    }
  }
}

int BUI_raster_width(const BuiRaster *r)
{
  return r->w;
}
int BUI_raster_height(const BuiRaster *r)
{
  return r->h;
}
const uint32_t *BUI_raster_pixels(const BuiRaster *r)
{
  if (r->gpu) { /* read the GPU frame back (tests, screenshots) */
    BuiRaster *m = (BuiRaster *)r;
    if (!m->prev) {
      m->prev = (uint32_t *)malloc((size_t)r->w * r->h * sizeof(uint32_t));
    }
    if (m->prev && bui_gpu_read(r->gpu, m->prev, r->w, r->h)) {
      return m->prev;
    }
  }
  return r->pix;
}

int BUI_raster_damage(const BuiRaster *r, const int (**r_rects)[4])
{
  *r_rects = (const int(*)[4])r->damage;
  return r->damage_len;
}

void BUI_raster_invalidate(BuiRaster *r)
{
  r->full_damage = 1;
}

size_t BUI_raster_view_size(void)
{
  return sizeof(BuiRaster);
}

void BUI_raster_view(const BuiRaster *src, BuiRaster *dst, int x0, int y0, int x1, int y1)
{
  *dst = *src; /* shares the pixels; drawing only touches pix inside the clip */
  dst->cx0 = clampi(x0 > src->cx0 ? x0 : src->cx0, src->ox, src->ox + src->w);
  dst->cy0 = clampi(y0 > src->cy0 ? y0 : src->cy0, src->oy, src->oy + src->h);
  dst->cx1 = clampi(x1 < src->cx1 ? x1 : src->cx1, dst->cx0, src->ox + src->w);
  dst->cy1 = clampi(y1 < src->cy1 ? y1 : src->cy1, dst->cy0, src->oy + src->h);
}

void BUI_raster_set_clip(BuiRaster *r, int x, int y, int w, int h)
{
  if (w <= 0 || h <= 0) {
    raster_reset_clip(r);
    return;
  }
  r->cx0 = clampi(x, r->ox, r->ox + r->w);
  r->cy0 = clampi(y, r->oy, r->oy + r->h);
  r->cx1 = clampi(x + w, r->ox, r->ox + r->w);
  r->cy1 = clampi(y + h, r->oy, r->oy + r->h);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Primitives
 * \{ */

void BUI_raster_fill_rect(BuiRaster *r, float x, float y, float w, float h, const float rgba[4])
{
  int x0, y0, x1, y1;
  if (!clip_box(r, x, y, x + w, y + h, &x0, &y0, &x1, &y1)) {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = rgba[3] > 0.0f ? gpu_inst(r, BG_RECT, x0, y0, x1, y1) : NULL;
    if (in) {
      v4(in->rect, x, y, w, h);
      c4(in->c0, rgba);
    }
    return;
  }
  const Paint p = paint_from(rgba);
  if (p.a <= 0) {
    return;
  }
  const float fx0 = x, fx1 = x + w, fy0 = y, fy1 = y + h;
  for (int py = y0; py < y1; py++) {
    const float cy = fminf(fy1, (float)py + 1.0f) - fmaxf(fy0, (float)py);
    if (cy <= 0.0f) {
      continue;
    }
    uint32_t *row = RASTER_ROW(r, py);
    /* fully covered interior span */
    const int ix0 = clampi((int)ceilf(fx0), x0, x1);
    const int ix1 = clampi((int)floorf(fx1), x0, x1);
    for (int px = x0; px < ix0 && px < x1; px++) {
      const float cx = fminf(fx1, (float)px + 1.0f) - fmaxf(fx0, (float)px);
      put_cov(row + px, p, cx * cy);
    }
    if (cy >= 1.0f && p.a >= 256) {
      for (int px = ix0; px < ix1; px++) {
        row[px] = p.rgb;
      }
    }
    else {
      const int f = (int)(cy * (float)p.a + 0.5f);
      if (f >= 256) {
        for (int px = ix0; px < ix1; px++) {
          row[px] = p.rgb;
        }
      }
      else if (f > 0) {
        for (int px = ix0; px < ix1; px++) {
          row[px] = blend(row[px], p.rgb, (uint32_t)f);
        }
      }
    }
    /* trailing partial column(s); when the rect sits inside one pixel the
     * leading loop already covered it (ix1 < ix0). */
    for (int px = (ix1 > ix0 ? ix1 : ix0); px < x1; px++) {
      const float cx = fminf(fx1, (float)px + 1.0f) - fmaxf(fx0, (float)px);
      put_cov(row + px, p, cx * cy);
    }
  }
}

void BUI_raster_frame_rect(
    BuiRaster *r, float x, float y, float w, float h, float t, const float rgba[4])
{
  if (t <= 0.0f || w <= 0.0f || h <= 0.0f) {
    return;
  }
  if (t * 2.0f >= w || t * 2.0f >= h) {
    BUI_raster_fill_rect(r, x, y, w, h, rgba);
    return;
  }
  /* Non-overlapping sides so translucent borders blend once. */
  BUI_raster_fill_rect(r, x, y, w, t, rgba);
  BUI_raster_fill_rect(r, x, y + h - t, w, t, rgba);
  BUI_raster_fill_rect(r, x, y + t, t, h - 2.0f * t, rgba);
  BUI_raster_fill_rect(r, x + w - t, y + t, t, h - 2.0f * t, rgba);
}

void BUI_raster_round_rect(
    BuiRaster *r, float x, float y, float w, float h, float radius, const float rgba[4])
{
  if (w <= 0.0f || h <= 0.0f) {
    return;
  }
  const float hw = w * 0.5f, hh = h * 0.5f;
  float rad = fminf(radius, fminf(hw, hh));
  if (rad <= 0.25f) {
    BUI_raster_fill_rect(r, x, y, w, h, rgba);
    return;
  }
  int x0, y0, x1, y1;
  if (!clip_box(r, x, y, x + w, y + h, &x0, &y0, &x1, &y1)) {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = rgba[3] > 0.0f ? gpu_inst(r, BG_WIDGET, x0, y0, x1, y1) : NULL;
    if (in) {
      v4(in->rect, x, y, w, h);
      c4(in->c0, rgba);
      c4(in->c1, rgba);
      v4(in->p, BG_WIDGET, rad, rad, 0.0f);
      v4(in->uv, BUI_CORNER_ALL, BG_W_INNER, 0.0f, 0.0f);
    }
    return;
  }
  const Paint p = paint_from(rgba);
  if (p.a <= 0) {
    return;
  }
  const float cx = x + hw, cy = y + hh;
  const float bx = hw - rad, by = hh - rad;
  const float rin = rad - 0.5f; /* coverage is exactly 1 inside this radius */
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float qy = fabsf((float)py + 0.5f - cy) - by;
    /* Half-width (from cx) of this row's fully covered span: the interior
     * is filled directly; the SDF only runs on the anti-aliased rim. */
    float ih = -1.0f;
    if (rin > 0.0f) {
      if (qy <= 0.0f) {
        ih = bx + rin;
      }
      else if (qy < rin) {
        ih = bx + sqrtf(rin * rin - qy * qy);
      }
    }
    int sa = x1, sb = x0; /* solid span [sa, sb) */
    if (ih >= 0.0f) {
      sa = clampi((int)ceilf(cx - ih - 0.5f), x0, x1);
      sb = clampi((int)floorf(cx + ih - 0.5f) + 1, x0, x1);
      if (sb > sa) {
        if (p.a >= 256) {
          for (int px = sa; px < sb; px++) {
            row[px] = p.rgb;
          }
        }
        else {
          for (int px = sa; px < sb; px++) {
            row[px] = blend(row[px], p.rgb, (uint32_t)p.a);
          }
        }
      }
      else {
        sa = x1;
        sb = x0;
      }
    }
    for (int px = x0; px < x1; px++) {
      if (px == sa) {
        px = sb - 1; /* skip the solid span */
        continue;
      }
      const float qx = fabsf((float)px + 0.5f - cx) - bx;
      float d;
      if (qx <= 0.0f && qy <= 0.0f) {
        d = fmaxf(qx, qy) - rad;
        if (d <= -0.5f) {
          if (p.a >= 256) {
            row[px] = p.rgb;
          }
          else {
            row[px] = blend(row[px], p.rgb, (uint32_t)p.a);
          }
          continue;
        }
      }
      else {
        const float mx = fmaxf(qx, 0.0f), my = fmaxf(qy, 0.0f);
        d = sqrtf(mx * mx + my * my) + fminf(fmaxf(qx, qy), 0.0f) - rad;
      }
      put_cov(row + px, p, clampf(0.5f - d, 0.0f, 1.0f));
    }
  }
}

void BUI_raster_circle(
    BuiRaster *r, float cx, float cy, float radius, float line_width, const float rgba[4])
{
  if (radius <= 0.0f) {
    return;
  }
  const float ext = radius + (line_width > 0.0f ? line_width : 0.0f) + 1.0f;
  int x0, y0, x1, y1;
  if (!clip_box(r, cx - ext, cy - ext, cx + ext, cy + ext, &x0, &y0, &x1, &y1)) {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = rgba[3] > 0.0f ? gpu_inst(r, BG_ELLIPSE, x0, y0, x1, y1) : NULL;
    if (in) {
      v4(in->rect, cx, cy, radius, radius);
      c4(in->c0, rgba);
      in->p[1] = line_width;
    }
    return;
  }
  const Paint p = paint_from(rgba);
  if (p.a <= 0) {
    return;
  }
  const float half_lw = line_width * 0.5f;
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float dy = (float)py + 0.5f - cy;
    for (int px = x0; px < x1; px++) {
      const float dx = (float)px + 0.5f - cx;
      const float dist = sqrtf(dx * dx + dy * dy);
      float cov;
      if (line_width > 0.0f) {
        cov = clampf(half_lw - fabsf(dist - radius) + 0.5f, 0.0f, 1.0f);
      }
      else {
        cov = clampf(radius - dist + 0.5f, 0.0f, 1.0f);
      }
      put_cov(row + px, p, cov);
    }
  }
}

void BUI_raster_line(
    BuiRaster *r, float x1, float y1, float x2, float y2, float width, const float rgba[4])
{
  if (width <= 0.0f) {
    width = 1.0f;
  }
  /* Axis-aligned lines are pixel-exact spans (crisp, like GL lines on the
   * pixel grid). Integer coords address the pixel to the right/below. */
  if (y1 == y2) {
    const float xa = fminf(x1, x2), xb = fmaxf(x1, x2);
    BUI_raster_fill_rect(r, xa, y1 - (width - 1.0f) * 0.5f, fmaxf(xb - xa, 1.0f), width, rgba);
    return;
  }
  if (x1 == x2) {
    const float ya = fminf(y1, y2), yb = fmaxf(y1, y2);
    BUI_raster_fill_rect(r, x1 - (width - 1.0f) * 0.5f, ya, width, fmaxf(yb - ya, 1.0f), rgba);
    return;
  }
  /* Diagonal: anti-aliased capsule between pixel centers. */
  const float ax = x1 + 0.5f, ay = y1 + 0.5f, bx = x2 + 0.5f, by = y2 + 0.5f;
  const float hw = width * 0.5f;
  int ix0, iy0, ix1, iy1;
  if (!clip_box(r, fminf(ax, bx) - hw - 1.0f, fminf(ay, by) - hw - 1.0f,
                fmaxf(ax, bx) + hw + 1.0f, fmaxf(ay, by) + hw + 1.0f, &ix0, &iy0, &ix1, &iy1))
  {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = rgba[3] > 0.0f ? gpu_inst(r, BG_LINE, ix0, iy0, ix1, iy1) : NULL;
    if (in) {
      v4(in->rect, ax, ay, bx, by);
      c4(in->c0, rgba);
      in->p[1] = hw;
    }
    return;
  }
  const Paint p = paint_from(rgba);
  if (p.a <= 0) {
    return;
  }
  const float vx = bx - ax, vy = by - ay;
  const float len2 = vx * vx + vy * vy;
  for (int py = iy0; py < iy1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float wy = (float)py + 0.5f - ay;
    for (int px = ix0; px < ix1; px++) {
      const float wx = (float)px + 0.5f - ax;
      const float t = clampf((wx * vx + wy * vy) / len2, 0.0f, 1.0f);
      const float ex = wx - vx * t, ey = wy - vy * t;
      const float d = sqrtf(ex * ex + ey * ey) - hw;
      put_cov(row + px, p, clampf(0.5f - d, 0.0f, 1.0f));
    }
  }
}

void BUI_raster_ellipse(BuiRaster *r,
                        float cx,
                        float cy,
                        float rx,
                        float ry,
                        float line_width,
                        const float rgba[4])
{
  rx = fabsf(rx);
  ry = fabsf(ry);
  if (rx <= 0.0f || ry <= 0.0f) {
    return;
  }
  if (fabsf(rx - ry) < 1e-3f) {
    BUI_raster_circle(r, cx, cy, rx, line_width, rgba);
    return;
  }
  const float ext = (line_width > 0.0f ? line_width : 0.0f) + 1.0f;
  int x0, y0, x1, y1;
  if (!clip_box(r, cx - rx - ext, cy - ry - ext, cx + rx + ext, cy + ry + ext, &x0, &y0, &x1,
                &y1))
  {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = rgba[3] > 0.0f ? gpu_inst(r, BG_ELLIPSE, x0, y0, x1, y1) : NULL;
    if (in) {
      v4(in->rect, cx, cy, rx, ry);
      c4(in->c0, rgba);
      in->p[1] = line_width;
    }
    return;
  }
  const Paint p = paint_from(rgba);
  if (p.a <= 0) {
    return;
  }
  const float half_lw = line_width * 0.5f;
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float dy = ((float)py + 0.5f - cy) / ry;
    for (int px = x0; px < x1; px++) {
      const float dx = ((float)px + 0.5f - cx) / rx;
      /* First-order distance: (|p/r| - 1) / |grad| — accurate near the edge,
       * which is all anti-aliasing needs. */
      const float k = sqrtf(dx * dx + dy * dy);
      const float gx = dx / rx, gy = dy / ry;
      const float g = sqrtf(gx * gx + gy * gy);
      const float d = g > 0.0f ? (k - 1.0f) * k / g : -fminf(rx, ry);
      float cov;
      if (line_width > 0.0f) {
        cov = clampf(half_lw - fabsf(d) + 0.5f, 0.0f, 1.0f);
      }
      else {
        cov = clampf(0.5f - d, 0.0f, 1.0f);
      }
      put_cov(row + px, p, cov);
    }
  }
}

/* Signed distance to a rounded box with a per-corner radius mask. Pixel
 * center (sx, sy); box center (cx, cy), half size (hw, hh). */
static inline float sd_widget_box(
    float sx, float sy, float cx, float cy, float hw, float hh, float rad, int corners)
{
  const float px = sx - cx, py = sy - cy;
  int bit;
  if (py < 0.0f) {
    bit = px < 0.0f ? BUI_CORNER_TOP_LEFT : BUI_CORNER_TOP_RIGHT;
  }
  else {
    bit = px < 0.0f ? BUI_CORNER_BOTTOM_LEFT : BUI_CORNER_BOTTOM_RIGHT;
  }
  const float r = (corners & bit) ? rad : 0.0f;
  const float qx = fabsf(px) - (hw - r), qy = fabsf(py) - (hh - r);
  /* Straight edges need no sqrt; only true corner pixels do. */
  if (qx <= 0.0f || qy <= 0.0f) {
    return fmaxf(qx, qy) - r;
  }
  return sqrtf(qx * qx + qy * qy) - r;
}

void BUI_raster_widget(BuiRaster *r,
                       float x,
                       float y,
                       float w,
                       float h,
                       float radius,
                       int corners,
                       const float inner_top[4],
                       const float inner_bottom[4],
                       const float outline[4],
                       float outline_width,
                       const float emboss[4])
{
  if (w <= 0.0f || h <= 0.0f) {
    return;
  }
  const float hw = w * 0.5f, hh = h * 0.5f;
  const float rad = clampf(radius, 0.0f, fminf(hw, hh));
  const float cx = x + hw, cy = y + hh;
  const float ow = outline ? fmaxf(outline_width, 0.0f) : 0.0f;
  const int has_emboss = emboss && emboss[3] > 0.0f;
  int x0, y0, x1, y1;
  if (!clip_box(r, x - 1.0f, y - 1.0f, x + w + 1.0f, y + h + (has_emboss ? 2.0f : 1.0f), &x0, &y0,
                &x1, &y1))
  {
    return;
  }
  const float *top = inner_top ? inner_top : inner_bottom;
  const float *bot = inner_bottom ? inner_bottom : inner_top;
  if (r->gpu) {
    BuiGpuInst *in = gpu_inst(r, BG_WIDGET, x0, y0, x1, y1);
    if (in) {
      v4(in->rect, x, y, w, h);
      c4(in->c0, top);
      c4(in->c1, bot);
      if (ow > 0.0f) {
        c4(in->c2, outline);
      }
      if (has_emboss) {
        c4(in->c3, emboss);
      }
      v4(in->p, BG_WIDGET, rad, rad, 1.0f);
      v4(in->uv, (float)corners, (float)(BG_W_LEGACY | (top ? BG_W_INNER : 0)), ow, 0.0f);
    }
    return;
  }
  const int gradient = top && bot && memcmp(top, bot, sizeof(float[4])) != 0;
  Paint p_inner = top ? paint_from(top) : (Paint){0, 0};
  const Paint p_out = outline ? paint_from(outline) : (Paint){0, 0};
  const Paint p_emb = has_emboss ? paint_from(emboss) : (Paint){0, 0};
  /* Fully covered interior margin: no SDF needed farther than this from
   * every edge (skips sqrt for the bulk of each widget). */
  const float safe = rad + ow + 1.0f;

  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float sy = (float)py + 0.5f;
    if (gradient) {
      const float t = clampf((sy - y) / h, 0.0f, 1.0f);
      float c[4];
      for (int k = 0; k < 4; k++) {
        c[k] = top[k] + (bot[k] - top[k]) * t;
      }
      p_inner = paint_from(c);
    }
    const int row_inside = (sy - y) >= safe && (y + h - sy) >= safe;
    /* Interior span of this row: filled directly, no per-pixel SDF. */
    int sa = x1, sb = x1;
    if (row_inside) {
      sa = clampi((int)ceilf(x + safe - 0.5f), x0, x1);
      sb = clampi((int)floorf(x + w - safe - 0.5f) + 1, x0, x1);
      if (sb <= sa) {
        sa = sb = x1;
      }
      else if (top) {
        if (p_inner.a >= 256) {
          for (int px = sa; px < sb; px++) {
            row[px] = p_inner.rgb;
          }
        }
        else if (p_inner.a > 0) {
          for (int px = sa; px < sb; px++) {
            row[px] = blend(row[px], p_inner.rgb, (uint32_t)p_inner.a);
          }
        }
      }
    }
    for (int px = x0; px < x1; px++) {
      if (px == sa) {
        px = sb - 1; /* skip the interior span */
        continue;
      }
      const float sx = (float)px + 0.5f;
      const float d = sd_widget_box(sx, sy, cx, cy, hw, hh, rad, corners);
      const float cov = clampf(0.5f - d, 0.0f, 1.0f);
      if (has_emboss && sy > cy) {
        /* Emboss: the same shape shifted 1px down, visible only outside. */
        const float de = sd_widget_box(sx, sy - 1.0f, cx, cy, hw, hh, rad, corners);
        const float ecov = clampf(0.5f - de, 0.0f, 1.0f) - cov;
        if (ecov > 0.0f) {
          put_cov(row + px, p_emb, ecov);
        }
      }
      if (cov <= 0.0f) {
        continue;
      }
      if (top) {
        put_cov(row + px, p_inner, cov);
      }
      if (ow > 0.0f) {
        /* Outline band: inside the edge, `ow` pixels wide. */
        const float inner_cov = clampf(0.5f - (d + ow), 0.0f, 1.0f);
        const float band = cov - inner_cov;
        if (band > 0.0f) {
          put_cov(row + px, p_out, band);
        }
      }
    }
  }
}

/* Blender's widget base (GPU_SHADER_2D_WIDGET_BASE) in software: the same
 * parameters interface_widgets.cc feeds the shader. Outer shape `rect`/`rad`,
 * inner shape `rect` inset by `pixelsize` with `radi`; the band between them
 * is the outline, the outer shape shifted down by one pixel (bottom half
 * only) is the emboss. */
void BUI_raster_widget_base(BuiRaster *r, const BuiWidgetBase *p)
{
  const float x = p->rect[0], y = p->rect[1], w = p->rect[2], h = p->rect[3];
  if (w <= 0.0f || h <= 0.0f) {
    return;
  }
  const float px1 = p->pixelsize > 0.0f ? p->pixelsize : 1.0f;
  const float hw = w * 0.5f, hh = h * 0.5f, cx = x + hw, cy = y + hh;
  const float rad = clampf(p->rad, 0.0f, fminf(hw, hh));
  const float hwi = hw - px1, hhi = hh - px1;
  const float radi = clampf(p->radi, 0.0f, fmaxf(0.0f, fminf(hwi, hhi)));
  const int has_inner = p->inner1[3] > 0.0f || p->inner2[3] > 0.0f || p->alpha_checker;
  const int has_outline = p->outline[3] > 0.0f;
  const int has_emboss = p->emboss[3] > 0.0f;
  if (!has_inner && !has_outline && !has_emboss) {
    return;
  }
  int x0, y0, x1, y1;
  if (!clip_box(r, x - 1.0f, y - 1.0f, x + w + 1.0f, y + h + px1 + 1.0f, &x0, &y0, &x1, &y1)) {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = gpu_inst(r, BG_WIDGET, x0, y0, x1, y1);
    if (in) {
      v4(in->rect, x, y, w, h);
      c4(in->c0, p->inner1);
      c4(in->c1, p->inner2);
      if (has_outline) {
        c4(in->c2, p->outline);
      }
      if (has_emboss) {
        c4(in->c3, p->emboss);
      }
      v4(in->p, BG_WIDGET, rad, radi, px1);
      const int flags = (p->shade_horizontal ? BG_W_SHADE_H : 0) |
                        (p->alpha_checker ? BG_W_CHECKER : 0) | (has_inner ? BG_W_INNER : 0);
      v4(in->uv, (float)p->corners, (float)flags, 0.0f, 0.0f);
    }
    return;
  }
  const int gradient = memcmp(p->inner1, p->inner2, sizeof(float[4])) != 0;
  const int per_pixel_inner = p->alpha_checker || (gradient && p->shade_horizontal);
  const Paint p_out = paint_from(p->outline);
  const Paint p_emb = paint_from(p->emboss);
  Paint p_in = paint_from(p->inner1);
  /* Columns farther than this from the left/right edges never see a corner:
   * there the distance field only depends on the row (computed once). */
  const float cornerw = fmaxf(rad, radi) + px1 + 1.0f;
  const int ma = clampi((int)ceilf(x + cornerw - 0.5f), x0, x1);
  const int mb = clampi((int)floorf(x + w - cornerw - 0.5f) + 1, ma, x1);

  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float sy = (float)py + 0.5f;
    if (gradient && !p->shade_horizontal) {
      const float t = clampf((sy - (y + px1)) / fmaxf(1.0f, h - 2.0f * px1), 0.0f, 1.0f);
      float c[4];
      for (int k = 0; k < 4; k++) {
        c[k] = p->inner1[k] + (p->inner2[k] - p->inner1[k]) * t;
      }
      p_in = paint_from(c);
    }
    /* -- middle columns: row-constant coverage -- */
    int span_a = ma, span_b = mb;
    if (per_pixel_inner) {
      span_a = span_b = x1; /* rare: checker / horizontal shading -> exact path */
    }
    if (span_a < span_b) {
      const float ay = fabsf(sy - cy);
      const float co = clampf(0.5f - (ay - hh), 0.0f, 1.0f);
      const float ci = fminf(co, clampf(0.5f - (ay - hhi), 0.0f, 1.0f));
      float ec = 0.0f;
      if (has_emboss && sy > cy) {
        ec = clampf(0.5f - (fabsf(sy - px1 - cy) - hh), 0.0f, 1.0f) - co;
      }
      const float band = has_outline ? co - ci : 0.0f;
      const int fill_inner = has_inner && ci > 0.0f;
      if (!(ec > 0.0f) && !(band > 0.0f) && fill_inner && ci >= 1.0f && p_in.a >= 256) {
        for (int i = span_a; i < span_b; i++) {
          row[i] = p_in.rgb; /* opaque interior: plain store */
        }
      }
      else if (ec > 0.0f || band > 0.0f || fill_inner) {
        const int fe = (int)(ec * (float)p_emb.a + 0.5f);
        const int fi = fill_inner ? (int)(ci * (float)p_in.a + 0.5f) : 0;
        const int fo = (int)(band * (float)p_out.a + 0.5f);
        for (int i = span_a; i < span_b; i++) {
          uint32_t d = row[i];
          if (fe > 0) {
            d = fe >= 256 ? p_emb.rgb : blend(d, p_emb.rgb, (uint32_t)fe);
          }
          if (fi > 0) {
            d = fi >= 256 ? p_in.rgb : blend(d, p_in.rgb, (uint32_t)fi);
          }
          if (fo > 0) {
            d = fo >= 256 ? p_out.rgb : blend(d, p_out.rgb, (uint32_t)fo);
          }
          row[i] = d;
        }
      }
    }
    /* -- corner columns (and everything on the exact path): full SDF -- */
    for (int px = x0; px < x1; px++) {
      if (px == span_a && span_a < span_b) {
        px = span_b - 1;
        continue;
      }
      const float sx = (float)px + 0.5f;
      const float d = sd_widget_box(sx, sy, cx, cy, hw, hh, rad, p->corners);
      const float co = clampf(0.5f - d, 0.0f, 1.0f);
      if (has_emboss && sy > cy) {
        const float de = sd_widget_box(sx, sy - px1, cx, cy, hw, hh, rad, p->corners);
        const float ec = clampf(0.5f - de, 0.0f, 1.0f) - co;
        if (ec > 0.0f) {
          put_cov(row + px, p_emb, ec);
        }
      }
      if (co <= 0.0f) {
        continue;
      }
      const float di = sd_widget_box(sx, sy, cx, cy, hwi, hhi, radi, p->corners);
      const float ci = fminf(co, clampf(0.5f - di, 0.0f, 1.0f));
      if (has_inner && ci > 0.0f) {
        if (p->alpha_checker) {
          /* checkerColorAndSize: 100/255 and 160/255, 8 px cells */
          const int cell = ((int)((sx - x) / 8.0f) + (int)((sy - y) / 8.0f)) & 1;
          const float g = cell ? 160.0f / 255.0f : 100.0f / 255.0f;
          const float gc[4] = {g, g, g, 1.0f};
          put_cov(row + px, paint_from(gc), ci);
        }
        Paint pi = p_in;
        if (gradient && p->shade_horizontal) {
          const float t = clampf((sx - (x + px1)) / fmaxf(1.0f, w - 2.0f * px1), 0.0f, 1.0f);
          float c[4];
          for (int k = 0; k < 4; k++) {
            c[k] = p->inner1[k] + (p->inner2[k] - p->inner1[k]) * t;
          }
          pi = paint_from(c);
        }
        put_cov(row + px, pi, ci);
      }
      if (has_outline && co - ci > 0.0f) {
        put_cov(row + px, p_out, co - ci);
      }
    }
  }
}

/* Premultiplied source over the (premultiplied) frame, all four channels. */
static inline uint32_t premul_over(uint32_t d, uint32_t s)
{
  const uint32_t ia = 255u - (s >> 24);
  uint32_t rb = (d & 0xFF00FFu) * ia + 0x800080u;
  uint32_t ag = ((d >> 8) & 0xFF00FFu) * ia + 0x800080u;
  rb = ((rb + ((rb >> 8) & 0xFF00FFu)) >> 8) & 0xFF00FFu; /* /255, rounded */
  ag = (ag + ((ag >> 8) & 0xFF00FFu)) & 0xFF00FF00u;
  return s + rb + ag;
}

/* All four channels times f/256. */
static inline uint32_t premul_scale(uint32_t c, uint32_t f)
{
  const uint32_t ag = ((c >> 8) & 0xFF00FFu) * f & 0xFF00FF00u;
  const uint32_t rb = ((c & 0xFF00FFu) * f >> 8) & 0xFF00FFu;
  return ag | rb;
}

/* An image pixel (premultiplied) times ga/256 over the frame. */
static inline void put_image_px(uint32_t *d, uint32_t c, uint32_t ga)
{
  if (ga < 256) {
    c = premul_scale(c, ga);
  }
  if (c >= 0xFF000000u) {
    *d = c;
  }
  else if (c) {
    *d = premul_over(*d, c);
  }
}

/* Bilinear tap of premultiplied pixels: fx, fy are 8-bit fractions. */
static inline uint32_t bilerp(uint32_t c00, uint32_t c10, uint32_t c01, uint32_t c11, uint32_t fx,
                              uint32_t fy)
{
  const uint32_t w11 = fx * fy >> 8, w10 = fx - w11, w01 = fy - w11, w00 = 256 - fx - fy + w11;
  const uint32_t rb = ((c00 & 0xFF00FFu) * w00 + (c10 & 0xFF00FFu) * w10 +
                       (c01 & 0xFF00FFu) * w01 + (c11 & 0xFF00FFu) * w11) >> 8 & 0xFF00FFu;
  const uint32_t ag = (((c00 >> 8) & 0xFF00FFu) * w00 + ((c10 >> 8) & 0xFF00FFu) * w10 +
                       ((c01 >> 8) & 0xFF00FFu) * w01 + ((c11 >> 8) & 0xFF00FFu) * w11) &
                      0xFF00FF00u;
  return ag | rb;
}

/* BUI-Awra: a straight RGBA texel (bytes R G B A, BUI_PIXMAP_RGBA) as
 * premultiplied 0xAARRGGBB, rounded as GdkMemoryTexture premultiplies. */
static inline uint32_t div255(uint32_t v)
{
  v += 128;
  return (v + (v >> 8)) >> 8;
}

static inline uint32_t rgba_texel(const uint32_t *p)
{
  const uint8_t *b = (const uint8_t *)p;
  const uint32_t a = b[3];
  if (a == 255) {
    return 0xFF000000u | (uint32_t)b[0] << 16 | (uint32_t)b[1] << 8 | b[2];
  }
  if (a == 0) {
    return 0;
  }
  return a << 24 | div255(b[0] * a) << 16 | div255(b[1] * a) << 8 | div255(b[2] * a);
}

/* A texel of an RGB pixmap (ARGB or RGBA) as premultiplied ARGB. */
static inline uint32_t pixmap_texel(const BuiPixmap *img, size_t i)
{
  return img->format == BUI_PIXMAP_RGBA ? rgba_texel(img->px + i) : img->px[i];
}

const uint32_t *bui_pixmap_upload(int format, const uint32_t *px, int *stride, int w, int h,
                                  uint32_t **scratch, size_t *cap)
{
  if (format != BUI_PIXMAP_RGBA && format != BUI_PIXMAP_I420_BT601 &&
      format != BUI_PIXMAP_I420_BT709)
  {
    return px;
  }
  const size_t bytes = format == BUI_PIXMAP_RGBA ? (size_t)w * h * 4 : ((size_t)w * h * 3 / 2 + 3) & ~(size_t)3;
  if (*cap < bytes) {
    uint32_t *grown = (uint32_t *)realloc(*scratch, bytes);
    if (!grown) {
      return NULL;
    }
    *scratch = grown;
    *cap = bytes;
  }
  uint32_t *out = *scratch;
  if (format == BUI_PIXMAP_RGBA) {
    for (int y = 0; y < h; y++) {
      const uint32_t *row = px + (size_t)y * *stride;
      uint32_t *o = out + (size_t)y * w;
      for (int x = 0; x < w; x++) {
        o[x] = rgba_texel(row + x);
      }
    }
  }
  else { /* I420 to NV12: luma as it is, chroma interleaved */
    const uint8_t *src = (const uint8_t *)px, *u = src + (size_t)w * h, *v = u + (size_t)w / 2 * (h / 2);
    uint8_t *dst = (uint8_t *)out, *uv = dst + (size_t)w * h;
    memcpy(dst, src, (size_t)w * h);
    for (size_t i = 0, n = (size_t)w / 2 * (h / 2); i < n; i++) {
      uv[2 * i] = u[i];
      uv[2 * i + 1] = v[i];
    }
  }
  *stride = w;
  return out;
}

static uint64_t pixmap_key(const BuiPixmap *img)
{
  if (img->key) {
    return img->key;
  }
  /* content hash: an image updated in place gets a new atlas entry */
  uint64_t h = 1469598103934665603ull;
  /* in 32-bit words: NV12 holds 1.5 bytes a pixel */
  const size_t px = (size_t)img->w * img->h;
  const size_t n = BUI_pixmap_yuv(img->format) ? px * 3 / 8 : px;
  for (size_t i = 0; i < n; i++) {
    h = (h ^ img->px[i]) * 1099511628211ull;
  }
  return h | (1ull << 63); /* never a BKImBuf key (counters) */
}

/* -- forgotten images (GPU renderers free big-image textures) ------------------- */

#define FORGET_LOG 256 /* power of two */
static uint64_t g_forget[FORGET_LOG];
static volatile long g_forget_head = 0;

void BUI_image_forget(uint64_t key)
{
#ifdef _MSC_VER
  const unsigned i = (unsigned)_InterlockedIncrement(&g_forget_head) - 1;
#else
  const unsigned i = (unsigned)__atomic_fetch_add(&g_forget_head, 1, __ATOMIC_ACQ_REL);
#endif
  g_forget[i & (FORGET_LOG - 1)] = key;
}

int bui_image_forgotten(unsigned *seen, uint64_t *r_keys, int max)
{
#ifdef _MSC_VER
  const unsigned head = (unsigned)_InterlockedOr(&g_forget_head, 0);
#else
  const unsigned head = (unsigned)__atomic_load_n(&g_forget_head, __ATOMIC_ACQUIRE);
#endif
  const unsigned n = head - *seen;
  *seen = head;
  if (n > (unsigned)max || n > FORGET_LOG / 2) {
    return -1;
  }
  for (unsigned k = 0; k < n; k++) {
    r_keys[k] = g_forget[(head - n + k) & (FORGET_LOG - 1)];
  }
  return (int)n;
}

static inline int imin(int a, int b)
{
  return a < b ? a : b;
}

/* A pixmap over the atlas (BUI_IMAGE_ATLAS_MAX): textures of its own, tiled only when a
 * side is over the device's limit. Tiles overlap by a pixel so bilinear
 * samples at their seams read real neighbours; each tile draws the part of
 * the destination whose samples fall in its core. */
static void gpu_image_big(BuiRaster *r, float x, float y, float w, float h, const BuiPixmap *img,
                          float sx, float sy, float sw, float sh, float alpha, int flags,
                          int x0, int y0, int x1, int y1)
{
  const uint64_t key = pixmap_key(img);
  const int core = bui_gpu_max_texture(r->gpu) - 2;
  if (core < 1) {
    return;
  }
  const int tiled = img->w > core + 2 || img->h > core + 2;
  if (tiled && BUI_pixmap_yuv(img->format)) {
    return; /* video frames are decoded within the device's limit */
  }
  const int nx = tiled ? (img->w + core - 1) / core : 1, ny = tiled ? (img->h + core - 1) / core : 1;
  const float kx = w / sw, ky = h / sh;
  for (int j = 0; j < ny; j++) {
    for (int i = 0; i < nx; i++) {
      /* core [cx0, cx1) x [cy0, cy1); texture one pixel wider where it can */
      const int cx0 = tiled ? i * core : 0, cx1 = tiled ? imin(img->w, cx0 + core) : img->w;
      const int cy0 = tiled ? j * core : 0, cy1 = tiled ? imin(img->h, cy0 + core) : img->h;
      const float u0 = fmaxf((float)cx0, sx), u1 = fminf((float)cx1, sx + sw);
      const float v0 = fmaxf((float)cy0, sy), v1 = fminf((float)cy1, sy + sh);
      if (!(u1 > u0) || !(v1 > v0)) {
        continue;
      }
      const int tx0 = cx0 > 0 ? cx0 - 1 : 0, tx1 = imin(img->w, cx1 + 1);
      const int ty0 = cy0 > 0 ? cy0 - 1 : 0, ty1 = imin(img->h, cy1 + 1);
      const uint64_t tkey = tiled ? (key ^ ((uint64_t)(j * nx + i + 1) * 0x9E3779B97F4A7C15ull)) : key;
      const int tex = tiled ? bui_gpu_image_tex(r->gpu, tkey, key, 0, img->format,
                                                img->px + (size_t)ty0 * img->w + tx0, img->w,
                                                tx1 - tx0, ty1 - ty0) :
                              bui_gpu_image_tex(r->gpu, key, img->stream ? img->stream : key,
                                                img->stream != 0, img->format, img->px, img->w,
                                                img->w, img->h);
      if (!tex) {
        continue;
      }
      /* the destination pixels whose centers sample this core */
      int bx0 = x0, by0 = y0, bx1 = x1, by1 = y1;
      if (tiled) {
        bx0 = clampi((int)ceilf(x + (u0 - sx) * kx - 0.5f), x0, x1);
        bx1 = clampi((int)ceilf(x + (u1 - sx) * kx - 0.5f), x0, x1);
        by0 = clampi((int)ceilf(y + (v0 - sy) * ky - 0.5f), y0, y1);
        by1 = clampi((int)ceilf(y + (v1 - sy) * ky - 0.5f), y0, y1);
        if (bx1 <= bx0 || by1 <= by0) {
          continue;
        }
      }
      BuiGpuInst *in = bui_gpu_inst_image(r->gpu, tex, bx0, by0, bx1, by1);
      if (in) {
        v4(in->rect, x, y, w, h);
        v4(in->uv, sx - (float)tx0, sy - (float)ty0, sw, sh);
        in->p[1] = alpha;
        in->p[2] = (flags & BUI_IMAGE_NEAREST) ? 1.0f : 0.0f;
        in->p[3] = (float)BUI_pixmap_yuv(img->format); /* > 0: YCbCr (luma + chroma textures) */
      }
    }
  }
}

/* -- NV12 video frames --------------------------------------------------------------
 * Converted to RGB while drawing: only the pixels drawn are converted (the
 * decoder already scaled the frame to about the size shown, so this is
 * usually the 1:1 path). Limited-range YCbCr, 16.16 fixed point:
 * {Y gain, Cr->R, Cb->G, Cr->G, Cb->B}. */
static const int k_yuv[2][5] = {
    {76309, 104597, 25675, 53279, 132201}, /* BT.601 */
    {76309, 117489, 13975, 34925, 138438}, /* BT.709 */
};

static inline uint32_t yuv_argb(const int *k, int y, int u, int v)
{
  const int c = (y - 16) * k[0] + 32768;
  u -= 128;
  v -= 128;
  const int r = clampi((c + k[1] * v) >> 16, 0, 255);
  const int g = clampi((c - k[2] * u - k[3] * v) >> 16, 0, 255);
  const int b = clampi((c + k[4] * u) >> 16, 0, 255);
  return 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

/* BUI-Awra: where a YCbCr pixmap's chroma is: NV12 interleaves Cb Cr in
 * rows of w bytes; I420 keeps them in planes of rows of w / 2. Cb of chroma
 * texel (cx, cy) is u[cy * stride + cx * step], Cr the same in v. */
typedef struct Chroma {
  const uint8_t *u, *v;
  int stride, step;
} Chroma;

static Chroma pixmap_chroma(const BuiPixmap *img)
{
  const uint8_t *c = (const uint8_t *)img->px + (size_t)img->w * img->h;
  if (img->format == BUI_PIXMAP_I420_BT601 || img->format == BUI_PIXMAP_I420_BT709) {
    const Chroma ch = {c, c + (size_t)(img->w / 2) * (img->h / 2), img->w / 2, 1};
    return ch;
  }
  const Chroma ch = {c, c + 1, img->w, 2};
  return ch;
}

void BUI_pixmap_read(const BuiPixmap *img, int y0, int rows, uint32_t *dst)
{
  const int yuv = BUI_pixmap_yuv(img->format);
  if (!yuv) {
    if (img->format == BUI_PIXMAP_RGBA) {
      for (size_t i = (size_t)y0 * img->w, n = i + (size_t)img->w * rows; i < n; i++) {
        *dst++ = rgba_texel(img->px + i);
      }
      return;
    }
    memcpy(dst, img->px + (size_t)y0 * img->w, sizeof(uint32_t) * (size_t)img->w * rows);
    return;
  }
  const int *k = k_yuv[yuv == 2];
  const uint8_t *yp = (const uint8_t *)img->px;
  const Chroma ch = pixmap_chroma(img);
  for (int y = y0; y < y0 + rows; y++) {
    const uint8_t *yr = yp + (size_t)y * img->w;
    const size_t crow = (size_t)(y >> 1) * ch.stride;
    for (int x = 0; x < img->w; x++) {
      const size_t c = crow + (size_t)(x >> 1) * ch.step;
      *dst++ = yuv_argb(k, yr[x], ch.u[c], ch.v[c]);
    }
  }
}

/* 8-bit bilinear of four samples (fx, fy: 0..256). */
static inline int lerp2(int a, int b, int c, int d, int fx, int fy)
{
  const int top = a * 256 + (b - a) * fx, bot = c * 256 + (d - c) * fx;
  return (top * 256 + (bot - top) * fy + 32768) >> 16;
}

static void raster_nv12(BuiRaster *r, float x, float y, float w, float h, const BuiPixmap *img,
                        float sx, float sy, float sw, float sh, uint32_t ga, int nearest,
                        int x0, int y0, int x1, int y1)
{
  const int *k = k_yuv[BUI_pixmap_yuv(img->format) == 2];
  const uint8_t *yp = (const uint8_t *)img->px;
  const Chroma ch = pixmap_chroma(img);
  const int iw = img->w, cs = ch.stride, step = ch.step;
  const float kx = sw / w, ky = sh / h;
  if (fabsf(kx - 1.0f) < 1e-4f && fabsf(ky - 1.0f) < 1e-4f && x == floorf(x) && y == floorf(y) &&
      sx == floorf(sx) && sy == floorf(sy))
  {
    /* 1:1: one luma sample and its chroma pair per pixel */
    const int ox = (int)sx - (int)x, oy = (int)sy - (int)y;
    for (int py = y0; py < y1; py++) {
      uint32_t *row = RASTER_ROW(r, py);
      const int ty = py + oy;
      const uint8_t *yr = yp + (size_t)ty * iw;
      const uint8_t *cu = ch.u + (size_t)(ty >> 1) * cs, *cv = ch.v + (size_t)(ty >> 1) * cs;
      if (ga >= 256) {
        /* opaque (every video frame): the chroma terms once per pair, stored */
        for (int px = x0; px < x1; px++) {
          const int tx = px + ox;
          const size_t c = (size_t)(tx >> 1) * step;
          const int u = cu[c] - 128, v = cv[c] - 128;
          const int rv = k[1] * v + 32768, gu = 32768 - k[2] * u - k[3] * v, bu = k[4] * u + 32768;
          for (int n = (tx & 1) ? 1 : imin(2, x1 - px); n > 0; n--, px++) {
            const int l = (yr[px + ox] - 16) * k[0];
            row[px] = 0xFF000000u | (uint32_t)clampi((l + rv) >> 16, 0, 255) << 16 |
                      (uint32_t)clampi((l + gu) >> 16, 0, 255) << 8 |
                      (uint32_t)clampi((l + bu) >> 16, 0, 255);
          }
          px--;
        }
        continue;
      }
      for (int px = x0; px < x1; px++) {
        const int tx = px + ox;
        const size_t c = (size_t)(tx >> 1) * step;
        put_image_px(row + px, yuv_argb(k, yr[tx], cu[c], cv[c]), ga);
      }
    }
    return;
  }
  /* scaled: bilinear luma, bilinear chroma at half resolution (texel centers
   * at .5, as BUI_raster_image and the shaders) */
  const int ix0 = (int)sx, iy0 = (int)sy;
  const int ix1 = (int)fmaxf((float)ix0, ceilf(sx + sw) - 1.0f);
  const int iy1 = (int)fmaxf((float)iy0, ceilf(sy + sh) - 1.0f);
  const float bias = nearest ? 0.0f : 0.5f;
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float v = sy + ((float)py + 0.5f - y) * ky;
    const float vy = v - bias, vc = v * 0.5f - 0.5f;
    const int ty = (int)floorf(vy), tc = (int)floorf(vc);
    const int fy = nearest ? 0 : (int)((vy - (float)ty) * 256.0f);
    const int fcy = (int)((vc - (float)tc) * 256.0f);
    const uint8_t *ya = yp + (size_t)clampi(ty, iy0, iy1) * iw;
    const uint8_t *yb = yp + (size_t)clampi(ty + 1, iy0, iy1) * iw;
    const size_t ca = (size_t)clampi(tc, iy0 >> 1, iy1 >> 1) * cs;
    const size_t cb = (size_t)clampi(tc + 1, iy0 >> 1, iy1 >> 1) * cs;
    /* texel coordinates in 16.16 fixed point, stepped per pixel (no float
     * floor per pixel: this is the path of every resized video frame) */
    const int64_t du = (int64_t)(kx * 65536.0f);
    int64_t u = (int64_t)((sx + ((float)x0 + 0.5f - x) * kx) * 65536.0f);
    const int64_t ub = (int64_t)(bias * 65536.0f);
    for (int px = x0; px < x1; px++, u += du) {
      const int64_t uy = u - ub, uc = (u >> 1) - 32768;
      const int tx = (int)(uy >> 16), tcx = (int)(uc >> 16);
      const int fx = nearest ? 0 : (int)((uy >> 8) & 255);
      const int fcx = (int)((uc >> 8) & 255);
      const int xa = clampi(tx, ix0, ix1), xb = clampi(tx + 1, ix0, ix1);
      const size_t ca0 = (size_t)clampi(tcx, ix0 >> 1, ix1 >> 1) * step;
      const size_t cb0 = (size_t)clampi(tcx + 1, ix0 >> 1, ix1 >> 1) * step;
      const uint8_t *u = ch.u, *v = ch.v;
      const int Y = lerp2(ya[xa], ya[xb], yb[xa], yb[xb], fx, fy);
      const int U = lerp2(u[ca + ca0], u[ca + cb0], u[cb + ca0], u[cb + cb0], fcx, fcy);
      const int V = lerp2(v[ca + ca0], v[ca + cb0], v[cb + ca0], v[cb + cb0], fcx, fcy);
      put_image_px(row + px, yuv_argb(k, Y, U, V), ga);
    }
  }
}

/* BUI-Awra: mipmap levels, as OpenGL makes them for GSK's trilinear
 * filter (glGenerateMipmap: each level half the one above, rounded down,
 * at least 1, a bilinear blit of it: the 2x2 average where the size is
 * even). Kept per image for the images drawn most recently, only from the
 * finest level a draw needed. Main thread. */
#define MIP_LEVELS 16
#define MIP_BUDGET (6u << 20) /* bytes of levels kept */
typedef struct MipChain {
  uint64_t key;
  const uint32_t *px; /* the image the levels were made of */
  int w, h, format;
  int first;                   /* the finest level kept */
  int n;                       /* levels 0 .. n-1 exist (0: the image) */
  uint32_t *lv[MIP_LEVELS];    /* premultiplied ARGB, NULL below `first` */
  int lw[MIP_LEVELS], lh[MIP_LEVELS];
  size_t bytes;
  unsigned used;
} MipChain;
static MipChain g_mips[32];
static unsigned g_mip_clock;
static size_t g_mip_bytes;
static unsigned g_mip_forget_seen;

static void mip_drop(MipChain *m)
{
  for (int i = 0; i < MIP_LEVELS; i++) {
    free(m->lv[i]);
  }
  g_mip_bytes -= m->bytes;
  memset(m, 0, sizeof(*m));
}

/* One level from the one above it: a bilinear blit to half its size. */
static void mip_halve(const uint32_t *src, int sw, int sh, uint32_t *dst, int dw, int dh)
{
  const float kx = (float)sw / dw, ky = (float)sh / dh;
  for (int y = 0; y < dh; y++) {
    const float v = ((float)y + 0.5f) * ky - 0.5f;
    const int ty = (int)floorf(v);
    const uint32_t fy = (uint32_t)((v - (float)ty) * 256.0f + 0.5f);
    const uint32_t *ra = src + (size_t)clampi(ty, 0, sh - 1) * sw;
    const uint32_t *rb = src + (size_t)clampi(ty + 1, 0, sh - 1) * sw;
    for (int x = 0; x < dw; x++) {
      const float u = ((float)x + 0.5f) * kx - 0.5f;
      const int tx = (int)floorf(u);
      const uint32_t fx = (uint32_t)((u - (float)tx) * 256.0f + 0.5f);
      const int xa = clampi(tx, 0, sw - 1), xb = clampi(tx + 1, 0, sw - 1);
      if (fx == 128 && fy == 128) {
        /* the 2x2 average, rounded */
        const uint32_t c[4] = {ra[xa], ra[xb], rb[xa], rb[xb]};
        uint32_t o = 0;
        for (int sh8 = 0; sh8 < 32; sh8 += 8) {
          const uint32_t t = ((c[0] >> sh8) & 255) + ((c[1] >> sh8) & 255) +
                             ((c[2] >> sh8) & 255) + ((c[3] >> sh8) & 255);
          o |= ((t + 2) >> 2) << sh8;
        }
        dst[(size_t)y * dw + x] = o;
      }
      else {
        dst[(size_t)y * dw + x] = bilerp(ra[xa], ra[xb], rb[xa], rb[xb], fx > 255 ? 255 : fx,
                                         fy > 255 ? 255 : fy);
      }
    }
  }
}

/* The image's levels from `first` on (level 0 is the image itself). */
static MipChain *mip_chain(const BuiPixmap *img, int first)
{
  /* images gone for good take their levels with them */
  uint64_t gone[64];
  const int n_gone = bui_image_forgotten(&g_mip_forget_seen, gone, 64);
  for (int i = 0; i < (int)(sizeof(g_mips) / sizeof(g_mips[0])); i++) {
    MipChain *m = &g_mips[i];
    if (!m->n) {
      continue;
    }
    int drop = n_gone < 0;
    for (int k = 0; k < n_gone && !drop; k++) {
      drop = gone[k] == m->key;
    }
    if (drop) {
      mip_drop(m);
    }
  }
  const uint64_t key = pixmap_key(img);
  MipChain *m = NULL, *lru = &g_mips[0];
  for (int i = 0; i < (int)(sizeof(g_mips) / sizeof(g_mips[0])); i++) {
    MipChain *c = &g_mips[i];
    if (c->n && c->key == key && c->px == img->px && c->w == img->w && c->h == img->h &&
        c->format == img->format)
    {
      m = c;
      break;
    }
    if (!c->n || (lru->n && c->used < lru->used)) {
      lru = c;
    }
  }
  if (m && m->first <= first) {
    m->used = ++g_mip_clock;
    return m;
  }
  if (!m) {
    m = lru;
    if (m->n) {
      mip_drop(m);
    }
    m->key = key;
    m->px = img->px;
    m->w = img->w;
    m->h = img->h;
    m->format = img->format;
  }
  else {
    /* finer levels than kept: made again from the image */
    for (int i = 0; i < MIP_LEVELS; i++) {
      free(m->lv[i]);
      m->lv[i] = NULL;
    }
    g_mip_bytes -= m->bytes;
    m->bytes = 0;
  }
  /* level 0 as premultiplied ARGB, then halved down to 1x1, keeping the
   * levels from `first` */
  int w = img->w, h = img->h, n = 1;
  m->lw[0] = w;
  m->lh[0] = h;
  uint32_t *prev = NULL;
  if (img->format == BUI_PIXMAP_RGBA) {
    prev = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)w * h);
    if (!prev) {
      return NULL;
    }
    for (size_t i = 0, k = (size_t)w * h; i < k; i++) {
      prev[i] = rgba_texel(img->px + i);
    }
  }
  const uint32_t *above = prev ? prev : img->px;
  while ((w > 1 || h > 1) && n < MIP_LEVELS) {
    const int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
    uint32_t *lv = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)nw * nh);
    if (!lv) {
      break;
    }
    mip_halve(above, w, h, lv, nw, nh);
    if (prev && (n - 1 < first || n - 1 == 0)) {
      free(prev); /* a level above the finest one kept */
    }
    else if (prev) {
      m->lv[n - 1] = prev;
      m->bytes += sizeof(uint32_t) * (size_t)w * h;
    }
    prev = lv;
    above = lv;
    m->lw[n] = w = nw;
    m->lh[n] = h = nh;
    n++;
  }
  if (prev && n - 1 > 0) {
    m->lv[n - 1] = prev;
    m->bytes += sizeof(uint32_t) * (size_t)w * h;
  }
  else {
    free(prev);
  }
  m->n = n;
  m->first = first;
  m->used = ++g_mip_clock;
  g_mip_bytes += m->bytes;
  /* over budget: the least recently drawn images' levels go */
  while (g_mip_bytes > MIP_BUDGET) {
    MipChain *old = NULL;
    for (int i = 0; i < (int)(sizeof(g_mips) / sizeof(g_mips[0])); i++) {
      MipChain *c = &g_mips[i];
      if (c->n && c != m && (!old || c->used < old->used)) {
        old = c;
      }
    }
    if (!old) {
      break;
    }
    mip_drop(old);
  }
  return m;
}

/* A bilinear tap of a level `w` texels wide (`p`, or the image itself when
 * NULL), at texel coordinates of that level (centres at .5), clamped to
 * the level's part `lim` of the source rectangle. */
static inline uint32_t mip_tap(const BuiPixmap *img, const uint32_t *p, int w, float u, float v,
                               const int lim[4])
{
  u -= 0.5f;
  v -= 0.5f;
  const int tx = (int)floorf(u), ty = (int)floorf(v);
  const uint32_t fx = (uint32_t)((u - (float)tx) * 256.0f), fy = (uint32_t)((v - (float)ty) * 256.0f);
  const int xa = clampi(tx, lim[0], lim[2]), xb = clampi(tx + 1, lim[0], lim[2]);
  const size_t ra = (size_t)clampi(ty, lim[1], lim[3]) * w, rb = (size_t)clampi(ty + 1, lim[1], lim[3]) * w;
  if (!p) {
    return bilerp(pixmap_texel(img, ra + xa), pixmap_texel(img, ra + xb), pixmap_texel(img, rb + xa),
                  pixmap_texel(img, rb + xb), fx, fy);
  }
  return bilerp(p[ra + xa], p[ra + xb], p[rb + xa], p[rb + xb], fx, fy);
}

/* BUI-Awra: how an image is sampled where it is drawn (x, y, w, h) from
 * its part (sx, sy, sw, sh): bilinear, nearest, or trilinear between two
 * mipmap levels with a weight, as OpenGL's LINEAR_MIPMAP_LINEAR (the
 * level of detail log2 of the texels a pixel spans, the larger way). */
typedef struct ImgSampler {
  const BuiPixmap *img;
  float x, y, sx, sy, kx, ky;
  int nearest;
  const MipChain *mip; /* NULL: level 0 alone */
  int l0, l1;
  uint32_t mix; /* 0..256: weight of l1 */
  int lim[2][4];
} ImgSampler;

static void sampler_init(ImgSampler *s, const BuiPixmap *img, float x, float y, float w, float h,
                         float sx, float sy, float sw, float sh, int flags)
{
  memset(s, 0, sizeof(*s));
  s->img = img;
  s->x = x;
  s->y = y;
  s->sx = sx;
  s->sy = sy;
  s->kx = sw / w;
  s->ky = sh / h;
  s->nearest = (flags & BUI_IMAGE_NEAREST) != 0;
  const float rho = fmaxf(s->kx, s->ky);
  const int rgb = img->format == BUI_PIXMAP_ARGB || img->format == BUI_PIXMAP_RGBA;
  const int trilinear = !s->nearest && rgb && !img->stream &&
                         ((flags & BUI_IMAGE_TRILINEAR) ||
                          ((flags & BUI_IMAGE_MIPMAP_AUTO) && (s->kx > 2.0f || s->ky > 2.0f)));
  for (int k = 0; k < 2; k++) {
    s->lim[k][0] = (int)sx;
    s->lim[k][1] = (int)sy;
    s->lim[k][2] = (int)fmaxf(sx, ceilf(sx + sw) - 1.0f);
    s->lim[k][3] = (int)fmaxf(sy, ceilf(sy + sh) - 1.0f);
  }
  if (!trilinear || rho <= 1.0f) {
    return;
  }
  const float lod = log2f(rho);
  int l0 = (int)floorf(lod);
  uint32_t mix = (uint32_t)((lod - (float)l0) * 256.0f + 0.5f);
  if (mix >= 256) {
    l0++;
    mix = 0;
  }
  const MipChain *m = mip_chain(img, l0);
  if (!m) {
    return;
  }
  if (l0 >= m->n - 1) {
    l0 = m->n - 1;
    mix = 0;
  }
  s->mip = m;
  s->l0 = l0;
  s->l1 = mix ? l0 + 1 : l0;
  s->mix = mix;
  for (int k = 0; k < 2; k++) {
    const int l = k ? s->l1 : s->l0;
    const float fx = (float)m->lw[l] / img->w, fy = (float)m->lh[l] / img->h;
    s->lim[k][0] = clampi((int)floorf(sx * fx), 0, m->lw[l] - 1);
    s->lim[k][1] = clampi((int)floorf(sy * fy), 0, m->lh[l] - 1);
    s->lim[k][2] = clampi((int)ceilf((sx + sw) * fx) - 1, s->lim[k][0], m->lw[l] - 1);
    s->lim[k][3] = clampi((int)ceilf((sy + sh) * fy) - 1, s->lim[k][1], m->lh[l] - 1);
  }
}

/* The premultiplied colour at the centre (fx, fy) of a raster pixel. */
static inline uint32_t sampler_at(const ImgSampler *s, float fx, float fy)
{
  const BuiPixmap *img = s->img;
  /* level-0 texel coordinates */
  const float u = s->sx + (fx - s->x) * s->kx, v = s->sy + (fy - s->y) * s->ky;
  if (!s->mip) {
    if (s->nearest) {
      const int tx = clampi((int)floorf(u), s->lim[0][0], s->lim[0][2]);
      const int ty = clampi((int)floorf(v), s->lim[0][1], s->lim[0][3]);
      return pixmap_texel(img, (size_t)ty * img->w + tx);
    }
    return mip_tap(img, NULL, img->w, u, v, s->lim[0]);
  }
  const MipChain *m = s->mip;
  const float nu = u / img->w, nv = v / img->h;
  const uint32_t a = mip_tap(img, m->lv[s->l0], m->lw[s->l0], nu * m->lw[s->l0], nv * m->lh[s->l0],
                             s->lim[0]);
  if (!s->mix) {
    return a;
  }
  const uint32_t b = mip_tap(img, m->lv[s->l1], m->lw[s->l1], nu * m->lw[s->l1], nv * m->lh[s->l1],
                             s->lim[1]);
  return bilerp(a, b, a, b, s->mix > 255 ? 255 : s->mix, 0);
}

void BUI_raster_image(BuiRaster *r,
                      float x,
                      float y,
                      float w,
                      float h,
                      const BuiPixmap *img,
                      const float src[4],
                      float alpha,
                      int flags)
{
  int x0, y0, x1, y1;
  if (!img || !img->px || img->w <= 0 || img->h <= 0 || !(alpha > 0.0f) ||
      !clip_box(r, x, y, x + w, y + h, &x0, &y0, &x1, &y1))
  {
    return;
  }
  /* the source rectangle, inside the image */
  float sx = 0.0f, sy = 0.0f, sw = (float)img->w, sh = (float)img->h;
  if (src) {
    sx = clampf(src[0], 0.0f, (float)img->w);
    sy = clampf(src[1], 0.0f, (float)img->h);
    sw = clampf(src[2], 0.0f, (float)img->w - sx);
    sh = clampf(src[3], 0.0f, (float)img->h - sy);
    if (!(sw > 0.0f) || !(sh > 0.0f)) {
      return;
    }
  }
  alpha = clampf(alpha, 0.0f, 1.0f);
  if (BUI_pixmap_yuv(img->format)) {
    if ((img->w | img->h) & 1) {
      return; /* 4:2:0 is even-sized */
    }
    if (!r->gpu) {
      raster_nv12(r, x, y, w, h, img, sx, sy, sw, sh, (uint32_t)(alpha * 256.0f + 0.5f),
                  (flags & BUI_IMAGE_NEAREST) != 0, x0, y0, x1, y1);
      return;
    }
  }
  if (r->gpu && (BUI_pixmap_yuv(img->format) || img->stream || img->w > BUI_IMAGE_ATLAS_MAX ||
                 img->h > BUI_IMAGE_ATLAS_MAX))
  {
    gpu_image_big(r, x, y, w, h, img, sx, sy, sw, sh, alpha, flags, x0, y0, x1, y1);
    return;
  }
  if (r->gpu) {
    int ax, ay;
    if (bui_gpu_image(r->gpu, pixmap_key(img), img->format, img->px, img->w, img->h, &ax, &ay)) {
      BuiGpuInst *in = gpu_inst(r, BG_IMAGE, x0, y0, x1, y1);
      if (in) {
        v4(in->rect, x, y, w, h);
        v4(in->uv, (float)ax + sx, (float)ay + sy, sw, sh);
        in->p[1] = alpha;
        in->p[2] = (flags & BUI_IMAGE_NEAREST) ? 1.0f : 0.0f;
      }
    }
    return;
  }
  const uint32_t ga = (uint32_t)(alpha * 256.0f + 0.5f);
  const float kx = sw / w, ky = sh / h;
  /* 1:1 on whole pixels: a copy with blending */
  if (fabsf(kx - 1.0f) < 1e-4f && fabsf(ky - 1.0f) < 1e-4f && x == floorf(x) && y == floorf(y) &&
      sx == floorf(sx) && sy == floorf(sy))
  {
    for (int py = y0; py < y1; py++) {
      uint32_t *row = RASTER_ROW(r, py);
      const uint32_t *s = img->px + (size_t)(py - (int)y + (int)sy) * img->w + (int)sx - (int)x;
      if (img->format == BUI_PIXMAP_RGBA) {
        for (int px = x0; px < x1; px++) {
          put_image_px(row + px, rgba_texel(s + px), ga);
        }
        continue;
      }
      for (int px = x0; px < x1; px++) {
        put_image_px(row + px, s[px], ga);
      }
    }
    return;
  }
  if (flags & (BUI_IMAGE_TRILINEAR | BUI_IMAGE_MIPMAP_AUTO)) {
    ImgSampler smp;
    sampler_init(&smp, img, x, y, w, h, sx, sy, sw, sh, flags);
    if (smp.mip) {
      for (int py = y0; py < y1; py++) {
        uint32_t *row = RASTER_ROW(r, py);
        for (int px = x0; px < x1; px++) {
          put_image_px(row + px, sampler_at(&smp, (float)px + 0.5f, (float)py + 0.5f), ga);
        }
      }
      return;
    }
  }
  const int nearest = (flags & BUI_IMAGE_NEAREST) != 0;
  const int rgba = img->format == BUI_PIXMAP_RGBA;
  const int ix0 = (int)sx, iy0 = (int)sy;
  const int ix1 = (int)fmaxf((float)ix0, ceilf(sx + sw) - 1.0f);
  const int iy1 = (int)fmaxf((float)iy0, ceilf(sy + sh) - 1.0f);
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    /* texel space, centers at .5 */
    const float v = sy + ((float)py + 0.5f - y) * ky - (nearest ? 0.0f : 0.5f);
    int ty = (int)floorf(v);
    const uint32_t fy = nearest ? 0u : (uint32_t)((v - (float)ty) * 256.0f);
    const int ta = clampi(ty, iy0, iy1), tb = clampi(ty + 1, iy0, iy1);
    const uint32_t *ra = img->px + (size_t)ta * img->w, *rb = img->px + (size_t)tb * img->w;
    for (int px = x0; px < x1; px++) {
      const float u = sx + ((float)px + 0.5f - x) * kx - (nearest ? 0.0f : 0.5f);
      const int tx = (int)floorf(u);
      uint32_t c;
      if (nearest) {
        c = rgba ? rgba_texel(ra + clampi(tx, ix0, ix1)) : ra[clampi(tx, ix0, ix1)];
      }
      else {
        const uint32_t fx = (uint32_t)((u - (float)tx) * 256.0f);
        const int xa = clampi(tx, ix0, ix1), xb = clampi(tx + 1, ix0, ix1);
        c = rgba ? bilerp(rgba_texel(ra + xa), rgba_texel(ra + xb), rgba_texel(rb + xa),
                          rgba_texel(rb + xb), fx, fy) :
                   bilerp(ra[xa], ra[xb], rb[xa], rb[xb], fx, fy);
      }
      put_image_px(row + px, c, ga);
    }
  }
}

void BUI_raster_end_frame_region(BuiRaster *r, int rx, int ry, int rw, int rh)
{
  /* Like end_frame, but only compares tiles touching the region that was
   * redrawn: the rest of the frame is known to be unchanged. */
  if (r->full_damage || rw <= 0 || rh <= 0) {
    BUI_raster_end_frame(r);
    return;
  }
  rx -= r->ox; /* BUI-Awra: the region in buffer pixels */
  ry -= r->oy;
  r->damage_len = 0;
  if (r->gpu) {
    bui_gpu_damage(r->gpu, rx, ry, rw, rh);
    damage_push(r, rx, ry, rw, rh);
    return;
  }
  if (!r->prev) { /* BUI-Awra: no diff: the region drawn is the damage */
    const int x0 = clampi(rx, 0, r->w), y0 = clampi(ry, 0, r->h);
    const int x1 = clampi(rx + rw, 0, r->w), y1 = clampi(ry + rh, 0, r->h);
    if (x1 > x0 && y1 > y0) {
      damage_push(r, x0, y0, x1 - x0, y1 - y0);
    }
    return;
  }
  const int tx0 = clampi(rx, 0, r->w) / TILE_W * TILE_W;
  const int ty0 = clampi(ry, 0, r->h) / TILE_H * TILE_H;
  const int tx1 = clampi(rx + rw, 0, r->w), ty1 = clampi(ry + rh, 0, r->h);
  for (int ty = ty0; ty < ty1; ty += TILE_H) {
    const int th = (ty + TILE_H <= r->h) ? TILE_H : r->h - ty;
    int run_x = -1;
    for (int tx = tx0; tx < tx1 + TILE_W; tx += TILE_W) {
      int dirty = 0;
      if (tx < tx1) {
        const int tw = (tx + TILE_W <= r->w) ? TILE_W : r->w - tx;
        for (int y = ty; y < ty + th; y++) {
          const size_t o = (size_t)y * r->w + tx;
          if (memcmp(r->pix + o, r->prev + o, (size_t)tw * sizeof(uint32_t)) != 0) {
            dirty = 1;
            break;
          }
        }
      }
      if (dirty && run_x < 0) {
        run_x = tx;
      }
      else if (!dirty && run_x >= 0) {
        const int end = tx < r->w ? tx : r->w;
        damage_push(r, run_x, ty, end - run_x, th);
        for (int y = ty; y < ty + th; y++) {
          const size_t o = (size_t)y * r->w + run_x;
          memcpy(r->prev + o, r->pix + o, (size_t)(end - run_x) * sizeof(uint32_t));
        }
        run_x = -1;
      }
    }
  }
}

void BUI_raster_shadow(
    BuiRaster *r, float x, float y, float w, float h, float radius, float size, float alpha)
{
  if (w <= 0.0f || h <= 0.0f || size <= 0.0f || alpha <= 0.0f) {
    return;
  }
  int x0, y0, x1, y1;
  if (!clip_box(r, x - size, y - size * 0.5f, x + w + size, y + h + size * 1.5f, &x0, &y0, &x1,
                &y1))
  {
    return;
  }
  const float hw = w * 0.5f, hh = h * 0.5f;
  const float rad = clampf(radius, 0.0f, fminf(hw, hh));
  if (r->gpu) {
    BuiGpuInst *in = gpu_inst(r, BG_SHADOW, x0, y0, x1, y1);
    if (in) {
      v4(in->rect, x, y, w, h);
      v4(in->p, BG_SHADOW, rad, size, alpha);
    }
    return;
  }
  /* Light comes from above: the shadow sits slightly lower. */
  const float cx = x + hw, cy = y + hh + size * 0.25f;
  const Paint black = {0xFF000000u, 256};
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float sy = (float)py + 0.5f;
    for (int px = x0; px < x1; px++) {
      const float sx = (float)px + 0.5f;
      const float d = sd_widget_box(sx, sy, cx, cy, hw, hh, rad, BUI_CORNER_ALL);
      /* Only outside the box itself (the popup covers the inside). */
      const float dbox = sd_widget_box(sx, sy, x + hw, y + hh, hw, hh, rad, BUI_CORNER_ALL);
      if (dbox <= -0.5f || d >= size) {
        continue;
      }
      float t = 1.0f - clampf(d / size, 0.0f, 1.0f);
      t = t * t * (3.0f - 2.0f * t); /* smoothstep falloff */
      put_cov(row + px, black, t * alpha * clampf(0.5f + dbox, 0.0f, 1.0f));
    }
  }
}

#define POLY_SUBSAMPLES 4

static int cmp_float(const void *a, const void *b)
{
  const float fa = *(const float *)a, fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

void BUI_raster_polygon(BuiRaster *r, const float (*pts)[2], int n, const float rgba[4])
{
  if (n < 3) {
    return;
  }
  float minx = pts[0][0], maxx = pts[0][0], miny = pts[0][1], maxy = pts[0][1];
  for (int i = 1; i < n; i++) {
    minx = fminf(minx, pts[i][0]);
    maxx = fmaxf(maxx, pts[i][0]);
    miny = fminf(miny, pts[i][1]);
    maxy = fmaxf(maxy, pts[i][1]);
  }
  int x0, y0, x1, y1;
  if (!clip_box(r, minx, miny, maxx, maxy, &x0, &y0, &x1, &y1)) {
    return;
  }
  if (r->gpu) {
    if (rgba[3] <= 0.0f) {
      return;
    }
    /* Exact software coverage (even-odd, anti-aliased) as a mask: white on
     * black in a scratch raster, green channel = coverage. */
    const int mw = x1 - x0, mh = y1 - y0;
    if (!r->scratch) {
      r->scratch = BUI_raster_create(mw, mh);
    }
    BuiRaster *s = r->scratch;
    if (!s) {
      return;
    }
    BUI_raster_begin_frame(s, mw > s->w ? mw : s->w, mh > s->h ? mh : s->h);
    BUI_raster_set_clip(s, 0, 0, mw, mh);
    for (int yy = 0; yy < mh; yy++) {
      memset(s->pix + (size_t)yy * s->w, 0, (size_t)mw * sizeof(uint32_t));
    }
    float(*tp)[2] = (float(*)[2])malloc(sizeof(float[2]) * (size_t)n);
    uint8_t *mask = (uint8_t *)malloc((size_t)mw * mh);
    if (tp && mask) {
      for (int i = 0; i < n; i++) {
        tp[i][0] = pts[i][0] - (float)x0;
        tp[i][1] = pts[i][1] - (float)y0;
      }
      const float white[4] = {1, 1, 1, 1};
      BUI_raster_polygon(s, (const float(*)[2])tp, n, white);
      for (int yy = 0; yy < mh; yy++) {
        const uint32_t *row = s->pix + (size_t)yy * s->w;
        for (int xx = 0; xx < mw; xx++) {
          mask[(size_t)yy * mw + xx] = (uint8_t)((row[xx] >> 8) & 0xFF);
        }
      }
      int ax, ay;
      BuiGpuInst *in = bui_gpu_mask(r->gpu, NULL, 0, mask, mw, mh, &ax, &ay) ?
                           gpu_inst(r, BG_MASK, x0, y0, x1, y1) :
                           NULL;
      if (in) {
        v4(in->rect, (float)x0, (float)y0, (float)mw, (float)mh);
        v4(in->uv, (float)ax, (float)ay, 0.0f, 0.0f);
        c4(in->c0, rgba);
      }
    }
    free(tp);
    free(mask);
    return;
  }
  const Paint p = paint_from(rgba);
  if (p.a <= 0) {
    return;
  }
  const int span = x1 - x0;
  float *cov = (float *)calloc((size_t)span + 1, sizeof(float));
  float *xs = (float *)malloc(sizeof(float) * (size_t)n);
  if (!cov || !xs) {
    free(cov);
    free(xs);
    return;
  }
  const float sub_w = 1.0f / POLY_SUBSAMPLES;
  for (int py = y0; py < y1; py++) {
    memset(cov, 0, sizeof(float) * ((size_t)span + 1));
    int any = 0;
    for (int s = 0; s < POLY_SUBSAMPLES; s++) {
      const float sy = (float)py + ((float)s + 0.5f) * sub_w;
      int nx = 0;
      for (int i = 0, j = n - 1; i < n; j = i++) {
        const float ay = pts[j][1], by = pts[i][1];
        if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) {
          const float t = (sy - ay) / (by - ay);
          xs[nx++] = pts[j][0] + t * (pts[i][0] - pts[j][0]);
        }
      }
      if (nx < 2) {
        continue;
      }
      qsort(xs, (size_t)nx, sizeof(float), cmp_float);
      /* Even-odd spans with fractional horizontal coverage at the ends. */
      for (int k = 0; k + 1 < nx; k += 2) {
        const float a = fmaxf(xs[k], (float)x0), b = fminf(xs[k + 1], (float)x1);
        if (b <= a) {
          continue;
        }
        any = 1;
        const int ia = (int)floorf(a), ib = (int)floorf(b);
        if (ia == ib) {
          cov[ia - x0] += (b - a) * sub_w;
          continue;
        }
        cov[ia - x0] += ((float)ia + 1.0f - a) * sub_w;
        for (int x = ia + 1; x < ib; x++) {
          cov[x - x0] += sub_w;
        }
        if (ib < x1) {
          cov[ib - x0] += (b - (float)ib) * sub_w;
        }
      }
    }
    if (!any) {
      continue;
    }
    uint32_t *row = RASTER_ROW(r, py);
    for (int x = 0; x < span; x++) {
      put_cov(row + x0 + x, p, fminf(cov[x], 1.0f));
    }
  }
  free(cov);
  free(xs);
}

static inline float edge_fn(float ax, float ay, float bx, float by, float px, float py)
{
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

static inline int is_top_left(float ax, float ay, float bx, float by)
{
  /* For counter-clockwise (in y-down space: positive area) winding. */
  return (ay == by && bx < ax) || (by > ay);
}

void BUI_raster_triangles(BuiRaster *r,
                          const float (*co)[2],
                          const float (*col)[4],
                          int count)
{
  if (r->gpu) {
    const int clip[4] = {r->cx0, r->cy0, r->cx1, r->cy1};
    bui_gpu_triangles(r->gpu, co, col, count, clip);
    return;
  }
  for (int i = 0; i + 2 < count; i += 3) {
    float v[3][2] = {{co[i][0], co[i][1]}, {co[i + 1][0], co[i + 1][1]}, {co[i + 2][0], co[i + 2][1]}};
    const float *c[3] = {col[i], col[i + 1], col[i + 2]};
    float area = edge_fn(v[0][0], v[0][1], v[1][0], v[1][1], v[2][0], v[2][1]);
    if (area == 0.0f || area != area) {
      continue;
    }
    if (area < 0.0f) {
      float tv[2] = {v[1][0], v[1][1]};
      v[1][0] = v[2][0];
      v[1][1] = v[2][1];
      v[2][0] = tv[0];
      v[2][1] = tv[1];
      const float *tc = c[1];
      c[1] = c[2];
      c[2] = tc;
      area = -area;
    }
    int x0, y0, x1, y1;
    if (!clip_box(r, fminf(v[0][0], fminf(v[1][0], v[2][0])), fminf(v[0][1], fminf(v[1][1], v[2][1])),
                  fmaxf(v[0][0], fmaxf(v[1][0], v[2][0])), fmaxf(v[0][1], fmaxf(v[1][1], v[2][1])),
                  &x0, &y0, &x1, &y1))
    {
      continue;
    }
    const int flat = memcmp(c[0], c[1], sizeof(float[4])) == 0 &&
                     memcmp(c[0], c[2], sizeof(float[4])) == 0;
    const Paint flat_p = paint_from(c[0]);
    const int tl0 = is_top_left(v[1][0], v[1][1], v[2][0], v[2][1]);
    const int tl1 = is_top_left(v[2][0], v[2][1], v[0][0], v[0][1]);
    const int tl2 = is_top_left(v[0][0], v[0][1], v[1][0], v[1][1]);
    const float inv_area = 1.0f / area;
    for (int py = y0; py < y1; py++) {
      uint32_t *row = RASTER_ROW(r, py);
      const float sy = (float)py + 0.5f;
      for (int px = x0; px < x1; px++) {
        const float sx = (float)px + 0.5f;
        const float w0 = edge_fn(v[1][0], v[1][1], v[2][0], v[2][1], sx, sy);
        const float w1 = edge_fn(v[2][0], v[2][1], v[0][0], v[0][1], sx, sy);
        const float w2 = edge_fn(v[0][0], v[0][1], v[1][0], v[1][1], sx, sy);
        if ((w0 < 0.0f || (w0 == 0.0f && !tl0)) || (w1 < 0.0f || (w1 == 0.0f && !tl1)) ||
            (w2 < 0.0f || (w2 == 0.0f && !tl2)))
        {
          continue;
        }
        if (flat) {
          put_cov(row + px, flat_p, 1.0f);
        }
        else {
          float rgba[4];
          const float b0 = w0 * inv_area, b1 = w1 * inv_area, b2 = w2 * inv_area;
          for (int k = 0; k < 4; k++) {
            rgba[k] = c[0][k] * b0 + c[1][k] * b1 + c[2][k] * b2;
          }
          put_cov(row + px, paint_from(rgba), 1.0f);
        }
      }
    }
  }
}

void BUI_raster_points(BuiRaster *r,
                       const float (*co)[2],
                       const float (*col)[4],
                       int count,
                       float size)
{
  const float rad = fmaxf(0.5f, size * 0.5f);
  for (int i = 0; i < count; i++) {
    BUI_raster_circle(r, co[i][0], co[i][1], rad, 0.0f, col[i]);
  }
}

/* -------------------------------------------------------------------- */
/** \name Textures + textured meshes (BUI_raster_mesh)
 *
 * The registry is shared by every raster: the software path samples the
 * RGBA8 copy kept here, GPU devices upload it on first use and then only the
 * rectangles logged by later updates.
 * \{ */

#define TEX_LOG 32

typedef struct BuiTexture {
  int id;
  int w, h;
  uint8_t *rgba;
  unsigned gen; /* bumped by every update */
  struct {
    unsigned gen; /* the generation this update produced */
    int rect[4];
  } log[TEX_LOG];
  unsigned nlog; /* updates logged so far (ring index = nlog % TEX_LOG) */
} BuiTexture;

static BuiTexture **g_tex;     /* slot -> texture (NULL: free) */
static unsigned short *g_tex_serial;
static int g_tex_slots;
static unsigned g_tex_free_gen;

static BuiTexture *tex_get(int id)
{
  const int slot = (id & 0xFFFF) - 1;
  if (id <= 0 || slot < 0 || slot >= g_tex_slots) {
    return NULL;
  }
  BuiTexture *t = g_tex[slot];
  return t && t->id == id ? t : NULL;
}

static void tex_copy(BuiTexture *t, int x, int y, int w, int h, const uint8_t *src, int pitch)
{
  /* clip to the texture, moving the source along */
  if (x < 0) {
    src -= x * 4;
    w += x;
    x = 0;
  }
  if (y < 0) {
    src -= (ptrdiff_t)y * pitch;
    h += y;
    y = 0;
  }
  w = w < t->w - x ? w : t->w - x;
  h = h < t->h - y ? h : t->h - y;
  for (int row = 0; row < h; row++) {
    memcpy(t->rgba + ((size_t)(y + row) * t->w + x) * 4, src + (size_t)row * pitch,
           (size_t)w * 4);
  }
}

int BUI_texture_create(int w, int h, const void *rgba, int pitch)
{
  if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
    return 0;
  }
  BuiTexture *t = (BuiTexture *)calloc(1, sizeof(BuiTexture));
  if (!t || !(t->rgba = (uint8_t *)calloc((size_t)w * h, 4))) {
    free(t);
    return 0;
  }
  t->w = w;
  t->h = h;
  t->gen = 1;
  if (rgba) {
    tex_copy(t, 0, 0, w, h, (const uint8_t *)rgba, pitch > 0 ? pitch : w * 4);
  }
  int slot = 0;
  while (slot < g_tex_slots && g_tex[slot]) {
    slot++;
  }
  if (slot == g_tex_slots) {
    if (slot >= 0xFFFF) {
      free(t->rgba);
      free(t);
      return 0;
    }
    const int cap = g_tex_slots ? g_tex_slots * 2 : 16;
    BuiTexture **nt = (BuiTexture **)realloc(g_tex, sizeof(BuiTexture *) * (size_t)cap);
    unsigned short *ns = nt ? (unsigned short *)realloc(g_tex_serial,
                                                        sizeof(unsigned short) * (size_t)cap) :
                              NULL;
    if (nt) {
      g_tex = nt;
    }
    if (!nt || !ns) {
      free(t->rgba);
      free(t);
      return 0;
    }
    g_tex_serial = ns;
    for (int i = g_tex_slots; i < cap; i++) {
      g_tex[i] = NULL;
      g_tex_serial[i] = 0;
    }
    g_tex_slots = cap;
  }
  g_tex_serial[slot] = (unsigned short)(g_tex_serial[slot] % 0x7FFF + 1);
  t->id = ((int)g_tex_serial[slot] << 16) | (slot + 1);
  g_tex[slot] = t;
  return t->id;
}

int BUI_texture_update(int tex, int x, int y, int w, int h, const void *rgba, int pitch)
{
  BuiTexture *t = tex_get(tex);
  if (!t || !rgba) {
    return 0;
  }
  if (w <= 0 || h <= 0 || x >= t->w || y >= t->h || x + w <= 0 || y + h <= 0) {
    return 1;
  }
  tex_copy(t, x, y, w, h, (const uint8_t *)rgba, pitch > 0 ? pitch : w * 4);
  const int x0 = x > 0 ? x : 0, y0 = y > 0 ? y : 0;
  const int x1 = x + w < t->w ? x + w : t->w, y1 = y + h < t->h ? y + h : t->h;
  t->gen++;
  const unsigned k = t->nlog++ % TEX_LOG;
  t->log[k].gen = t->gen;
  t->log[k].rect[0] = x0;
  t->log[k].rect[1] = y0;
  t->log[k].rect[2] = x1 - x0;
  t->log[k].rect[3] = y1 - y0;
  return 1;
}

void BUI_texture_free(int tex)
{
  BuiTexture *t = tex_get(tex);
  if (!t) {
    return;
  }
  g_tex[(tex & 0xFFFF) - 1] = NULL;
  free(t->rgba);
  free(t);
  g_tex_free_gen++;
}

int BUI_texture_size(int tex, int *r_w, int *r_h)
{
  const BuiTexture *t = tex_get(tex);
  if (r_w) {
    *r_w = t ? t->w : 0;
  }
  if (r_h) {
    *r_h = t ? t->h : 0;
  }
  return t != NULL;
}

int bui_texture_view(int tex, BuiTexView *r_view)
{
  const BuiTexture *t = tex_get(tex);
  if (!t) {
    return 0;
  }
  r_view->w = t->w;
  r_view->h = t->h;
  r_view->rgba = t->rgba;
  r_view->gen = t->gen;
  return 1;
}

int bui_texture_changes(int tex, unsigned since, int (*r_rects)[4], int max)
{
  const BuiTexture *t = tex_get(tex);
  if (!t) {
    return -1;
  }
  const unsigned pending = t->gen - since; /* updates the caller has not seen */
  if (pending == 0) {
    return 0;
  }
  if (pending > (t->nlog < TEX_LOG ? t->nlog : TEX_LOG) || (int)pending > max) {
    return -1;
  }
  for (unsigned i = 0; i < pending; i++) {
    memcpy(r_rects[i], t->log[(t->nlog - pending + i) % TEX_LOG].rect, sizeof(int[4]));
  }
  return (int)pending;
}

unsigned bui_texture_free_gen(void)
{
  return g_tex_free_gen;
}

/* Bilinear (or nearest) sample at texture coordinates (u, v), clamped to the
 * edge, texel centers at +0.5 like the GPU samplers; out is 0..1 RGBA. */
static inline void tex_sample(const BuiTexture *t, float u, float v, int nearest, float out[4])
{
  if (nearest) {
    const int x = clampi((int)floorf(u * t->w), 0, t->w - 1);
    const int y = clampi((int)floorf(v * t->h), 0, t->h - 1);
    const uint8_t *p = t->rgba + ((size_t)y * t->w + x) * 4;
    for (int k = 0; k < 4; k++) {
      out[k] = p[k] * (1.0f / 255.0f);
    }
    return;
  }
  const float fx = u * t->w - 0.5f, fy = v * t->h - 0.5f;
  const float flx = floorf(fx), fly = floorf(fy);
  const float ax = fx - flx, ay = fy - fly;
  const int x0 = clampi((int)flx, 0, t->w - 1), x1 = clampi((int)flx + 1, 0, t->w - 1);
  const int y0 = clampi((int)fly, 0, t->h - 1), y1 = clampi((int)fly + 1, 0, t->h - 1);
  const uint8_t *r0 = t->rgba + (size_t)y0 * t->w * 4, *r1 = t->rgba + (size_t)y1 * t->w * 4;
  const uint8_t *p00 = r0 + x0 * 4, *p10 = r0 + x1 * 4, *p01 = r1 + x0 * 4, *p11 = r1 + x1 * 4;
  const float w00 = (1.0f - ax) * (1.0f - ay), w10 = ax * (1.0f - ay);
  const float w01 = (1.0f - ax) * ay, w11 = ax * ay;
  for (int k = 0; k < 4; k++) {
    out[k] = (p00[k] * w00 + p10[k] * w10 + p01[k] * w01 + p11[k] * w11) * (1.0f / 255.0f);
  }
}

/* Straight-alpha source-over of color c (0..1) into a premultiplied pixel. */
static inline void put_straight(uint32_t *px, const float c[4])
{
  const float a = c[3];
  if (!(a > 0.0f)) {
    return;
  }
  Paint p = paint_from(c);
  if (p.a >= 256) {
    *px = p.rgb;
  }
  else if (p.a > 0) {
    *px = blend(*px, p.rgb, (uint32_t)p.a);
  }
}

static inline void col_unpack(uint32_t c, float out[4])
{
  out[0] = (float)(c & 0xFF) * (1.0f / 255.0f);
  out[1] = (float)((c >> 8) & 0xFF) * (1.0f / 255.0f);
  out[2] = (float)((c >> 16) & 0xFF) * (1.0f / 255.0f);
  out[3] = (float)(c >> 24) * (1.0f / 255.0f);
}

/* Vertex coordinate in 1/256 px (D3D's subpixel precision), kept in range. */
static inline int64_t mesh_fx(float v)
{
  v = clampf(v, -1e6f, 1e6f);
  return (int64_t)floorf(v * 256.0f + 0.5f);
}

/* One triangle, pixel centers inside (top-left rule), clipped to
 * [cx0, cx1) x [cy0, cy1). */
static void mesh_triangle(BuiRaster *r,
                          const BuiMeshVertex *va,
                          const BuiMeshVertex *vb,
                          const BuiMeshVertex *vc,
                          float ox,
                          float oy,
                          const BuiTexture *t,
                          int nearest,
                          int cx0,
                          int cy0,
                          int cx1,
                          int cy1)
{
  int64_t X[3] = {mesh_fx(va->x + ox), mesh_fx(vb->x + ox), mesh_fx(vc->x + ox)};
  int64_t Y[3] = {mesh_fx(va->y + oy), mesh_fx(vb->y + oy), mesh_fx(vc->y + oy)};
  const BuiMeshVertex *v[3] = {va, vb, vc};
  int64_t area = (X[1] - X[0]) * (Y[2] - Y[0]) - (Y[1] - Y[0]) * (X[2] - X[0]);
  if (area == 0) {
    return;
  }
  if (area < 0) { /* make it clockwise on screen (positive area, y down) */
    int64_t tx = X[1], ty = Y[1];
    X[1] = X[2];
    Y[1] = Y[2];
    X[2] = tx;
    Y[2] = ty;
    const BuiMeshVertex *tv = v[1];
    v[1] = v[2];
    v[2] = tv;
    area = -area;
  }
  int64_t mnx = X[0], mxx = X[0], mny = Y[0], mxy = Y[0];
  for (int i = 1; i < 3; i++) {
    mnx = X[i] < mnx ? X[i] : mnx;
    mxx = X[i] > mxx ? X[i] : mxx;
    mny = Y[i] < mny ? Y[i] : mny;
    mxy = Y[i] > mxy ? Y[i] : mxy;
  }
  /* pixels whose centers (px * 256 + 128) can be inside */
  int x0 = (int)((mnx - 128 + 255) >> 8), x1 = (int)((mxx - 128) >> 8) + 1;
  int y0 = (int)((mny - 128 + 255) >> 8), y1 = (int)((mxy - 128) >> 8) + 1;
  x0 = x0 > cx0 ? x0 : cx0;
  y0 = y0 > cy0 ? y0 : cy0;
  x1 = x1 < cx1 ? x1 : cx1;
  y1 = y1 < cy1 ? y1 : cy1;
  if (x0 >= x1 || y0 >= y1) {
    return;
  }
  /* edge k runs from vertex k+1 to k+2 (opposite vertex k): E > 0 inside.
   * Top-left rule: an edge owns its pixels when it is a top edge (horizontal,
   * running right) or a left edge (running up). */
  int64_t A[3], B[3], E0[3];
  int bias[3];
  const int64_t px0 = (int64_t)x0 * 256 + 128, py0 = (int64_t)y0 * 256 + 128;
  for (int k = 0; k < 3; k++) {
    const int a = (k + 1) % 3, b = (k + 2) % 3;
    const int64_t dx = X[b] - X[a], dy = Y[b] - Y[a];
    A[k] = -dy * 256; /* per pixel step in x */
    B[k] = dx * 256;  /* per pixel step in y */
    E0[k] = dx * (py0 - Y[a]) - dy * (px0 - X[a]);
    const int top_left = (dy == 0 && dx > 0) || dy < 0;
    bias[k] = top_left ? 0 : -1;
  }
  float col[3][4];
  for (int i = 0; i < 3; i++) {
    col_unpack(v[i]->col, col[i]);
  }
  const int flat_col = v[0]->col == v[1]->col && v[0]->col == v[2]->col;
  const int flat_uv = !t || (v[0]->u == v[1]->u && v[0]->u == v[2]->u && v[0]->v == v[1]->v &&
                             v[0]->v == v[2]->v);
  const double inv = 1.0 / (double)area;
  float tflat[4] = {1.0f, 1.0f, 1.0f, 1.0f}; /* the texel when uv is constant */
  if (flat_uv && t) {
    tex_sample(t, v[0]->u, v[0]->v, nearest, tflat);
  }
  float flat[4];
  for (int k = 0; k < 4; k++) {
    flat[k] = col[0][k] * tflat[k];
  }
  const int flat_all = flat_col && flat_uv;
  if (flat_all && !(flat[3] > 0.0f)) {
    return;
  }
  for (int py = y0; py < y1; py++) {
    int64_t e0 = E0[0] + B[0] * (py - y0), e1 = E0[1] + B[1] * (py - y0),
            e2 = E0[2] + B[2] * (py - y0);
    uint32_t *row = RASTER_ROW(r, py);
    for (int px = x0; px < x1; px++, e0 += A[0], e1 += A[1], e2 += A[2]) {
      if (e0 + bias[0] < 0 || e1 + bias[1] < 0 || e2 + bias[2] < 0) {
        continue;
      }
      if (flat_all) {
        put_straight(row + px, flat);
        continue;
      }
      /* barycentric weights of vertices 0, 1, 2 */
      const float l0 = (float)((double)e0 * inv), l1 = (float)((double)e1 * inv);
      const float l2 = 1.0f - l0 - l1;
      float c[4];
      if (flat_col) {
        memcpy(c, col[0], sizeof(c));
      }
      else {
        for (int k = 0; k < 4; k++) {
          c[k] = col[0][k] * l0 + col[1][k] * l1 + col[2][k] * l2;
        }
      }
      if (flat_uv) {
        for (int k = 0; k < 4; k++) {
          c[k] *= tflat[k];
        }
      }
      else {
        float tx[4];
        tex_sample(t, v[0]->u * l0 + v[1]->u * l1 + v[2]->u * l2,
                   v[0]->v * l0 + v[1]->v * l1 + v[2]->v * l2, nearest, tx);
        for (int k = 0; k < 4; k++) {
          c[k] *= tx[k];
        }
      }
      put_straight(row + px, c);
    }
  }
}

void BUI_raster_mesh(BuiRaster *r,
                     const BuiMeshVertex *vtx,
                     int n_vtx,
                     const void *idx,
                     int n_idx,
                     int flags,
                     int tex,
                     const float clip[4],
                     float ox,
                     float oy)
{
  if (!vtx || !idx || n_vtx <= 0 || n_idx < 3) {
    return;
  }
  int cx0 = r->cx0, cy0 = r->cy0, cx1 = r->cx1, cy1 = r->cy1;
  if (clip) { /* a scissor: whole pixels, truncated like ImGui's backends do */
    const float fx0 = clampf(clip[0] + ox, -1e6f, 1e6f), fy0 = clampf(clip[1] + oy, -1e6f, 1e6f);
    const float fx1 = clampf(clip[2] + ox, -1e6f, 1e6f), fy1 = clampf(clip[3] + oy, -1e6f, 1e6f);
    cx0 = (int)fx0 > cx0 ? (int)fx0 : cx0;
    cy0 = (int)fy0 > cy0 ? (int)fy0 : cy0;
    cx1 = (int)fx1 < cx1 ? (int)fx1 : cx1;
    cy1 = (int)fy1 < cy1 ? (int)fy1 : cy1;
  }
  if (cx0 >= cx1 || cy0 >= cy1) {
    return;
  }
  const int idx32 = (flags & BUI_MESH_INDEX32) != 0, nearest = (flags & BUI_MESH_NEAREST) != 0;
  if (r->gpu) {
    const int box[4] = {cx0, cy0, cx1, cy1};
    bui_gpu_mesh(r->gpu, vtx, n_vtx, idx, idx32, n_idx, ox, oy, tex, nearest, box);
    return;
  }
  const BuiTexture *t = tex ? tex_get(tex) : NULL;
  const uint16_t *i16 = (const uint16_t *)idx;
  const uint32_t *i32 = (const uint32_t *)idx;
  for (int i = 0; i + 2 < n_idx; i += 3) {
    const uint32_t a = idx32 ? i32[i] : i16[i], b = idx32 ? i32[i + 1] : i16[i + 1],
                   c = idx32 ? i32[i + 2] : i16[i + 2];
    if (a >= (uint32_t)n_vtx || b >= (uint32_t)n_vtx || c >= (uint32_t)n_vtx) {
      continue;
    }
    mesh_triangle(r, &vtx[a], &vtx[b], &vtx[c], ox, oy, t, nearest, cx0, cy0, cx1, cy1);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color widgets (HSV wheel, gradients, lit sphere)
 * \{ */

static void hsv_to_rgb_f(float h, float s, float v, float out[3])
{
  h = h - floorf(h);
  const float h6 = h * 6.0f;
  const float r = clampf(fabsf(h6 - 3.0f) - 1.0f, 0.0f, 1.0f);
  const float g = clampf(2.0f - fabsf(h6 - 2.0f), 0.0f, 1.0f);
  const float b = clampf(2.0f - fabsf(h6 - 4.0f), 0.0f, 1.0f);
  out[0] = ((r - 1.0f) * s + 1.0f) * v;
  out[1] = ((g - 1.0f) * s + 1.0f) * v;
  out[2] = ((b - 1.0f) * s + 1.0f) * v;
}

void BUI_raster_hsv_circle(BuiRaster *r, float cx, float cy, float radius, float value)
{
  int x0, y0, x1, y1;
  if (radius <= 0.0f ||
      !clip_box(r, cx - radius - 1, cy - radius - 1, cx + radius + 1, cy + radius + 1, &x0, &y0,
                &x1, &y1))
  {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = gpu_inst(r, BG_HSV, x0, y0, x1, y1);
    if (in) {
      v4(in->rect, cx, cy, radius, value);
    }
    return;
  }
  const float inv_2pi = 0.15915494309189535f;
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float dy = (float)py + 0.5f - cy;
    for (int px = x0; px < x1; px++) {
      const float dx = (float)px + 0.5f - cx;
      const float dist = sqrtf(dx * dx + dy * dy);
      const float cov = clampf(radius - dist + 0.5f, 0.0f, 1.0f);
      if (cov <= 0.0f) {
        continue;
      }
      /* Blender's wheel (ui_hsvcircle_vals_from_pos): hue from the angle
       * measured from "up", red at the bottom; saturation = distance. */
      const float hue = atan2f(dx, -dy) * inv_2pi + 0.5f;
      float rgba[4] = {0, 0, 0, 1.0f};
      hsv_to_rgb_f(hue, fminf(dist / radius, 1.0f), value, rgba);
      put_cov(row + px, paint_from(rgba), cov);
    }
  }
}

void BUI_raster_gradient(BuiRaster *r,
                         float x,
                         float y,
                         float w,
                         float h,
                         const float tl[4],
                         const float tr[4],
                         const float br[4],
                         const float bl[4])
{
  int x0, y0, x1, y1;
  if (!clip_box(r, x, y, x + w, y + h, &x0, &y0, &x1, &y1)) {
    return;
  }
  if (r->gpu) {
    BuiGpuInst *in = gpu_inst(r, BG_GRADIENT, x0, y0, x1, y1);
    if (in) {
      v4(in->rect, x, y, w, h);
      c4(in->c0, tl);
      c4(in->c1, tr);
      c4(in->c2, br);
      c4(in->c3, bl);
    }
    return;
  }
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float v = clampf(((float)py + 0.5f - y) / h, 0.0f, 1.0f);
    float left[4], right[4];
    for (int k = 0; k < 4; k++) {
      left[k] = tl[k] + (bl[k] - tl[k]) * v;
      right[k] = tr[k] + (br[k] - tr[k]) * v;
    }
    for (int px = x0; px < x1; px++) {
      const float u = clampf(((float)px + 0.5f - x) / w, 0.0f, 1.0f);
      float c[4];
      for (int k = 0; k < 4; k++) {
        c[k] = left[k] + (right[k] - left[k]) * u;
      }
      put_cov(row + px, paint_from(c), 1.0f);
    }
  }
}

void BUI_raster_sphere(
    BuiRaster *r, float cx, float cy, float radius, const float light[3], const float rgba[4])
{
  int x0, y0, x1, y1;
  if (radius <= 0.0f ||
      !clip_box(r, cx - radius - 1, cy - radius - 1, cx + radius + 1, cy + radius + 1, &x0, &y0,
                &x1, &y1))
  {
    return;
  }
  float L[3] = {light[0], light[1], light[2]};
  const float len = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
  if (len > 0.0f) {
    L[0] /= len;
    L[1] /= len;
    L[2] /= len;
  }
  else {
    L[2] = 1.0f;
  }
  /* Half vector for a viewer looking down -z. */
  float H[3] = {L[0], L[1], L[2] + 1.0f};
  const float hl = sqrtf(H[0] * H[0] + H[1] * H[1] + H[2] * H[2]);
  if (hl > 0.0f) {
    H[0] /= hl;
    H[1] /= hl;
    H[2] /= hl;
  }
  if (r->gpu) {
    BuiGpuInst *in = gpu_inst(r, BG_SPHERE, x0, y0, x1, y1);
    if (in) {
      v4(in->rect, cx, cy, radius, 0.0f);
      c4(in->c0, rgba);
      v4(in->c1, L[0], L[1], L[2], 0.0f);
      v4(in->c2, H[0], H[1], H[2], 0.0f);
    }
    return;
  }
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float ny = -((float)py + 0.5f - cy) / radius; /* up is +y */
    for (int px = x0; px < x1; px++) {
      const float nx = ((float)px + 0.5f - cx) / radius;
      const float d2 = nx * nx + ny * ny;
      const float cov = clampf((1.0f - sqrtf(d2)) * radius + 0.5f, 0.0f, 1.0f);
      if (cov <= 0.0f) {
        continue;
      }
      const float nz = sqrtf(fmaxf(0.0f, 1.0f - d2));
      const float diff = fmaxf(0.0f, nx * L[0] + ny * L[1] + nz * L[2]);
      float spec = fmaxf(0.0f, nx * H[0] + ny * H[1] + nz * H[2]);
      spec = spec * spec;
      spec = spec * spec;
      spec = spec * spec * spec * spec; /* ^16 */
      float c[4];
      for (int k = 0; k < 3; k++) {
        c[k] = rgba[k] * (0.12f + 0.88f * diff) + 0.6f * spec;
      }
      c[3] = rgba[3];
      put_cov(row + px, paint_from(c), cov);
    }
  }
}

/** \} */

void BUI_raster_set_font(BuiRaster *r, const char *family, int flags)
{
  if (!family) {
    family = "";
  }
  strncpy(r->font_family, family, sizeof(r->font_family) - 1);
  r->font_family[sizeof(r->font_family) - 1] = '\0';
  r->font_flags = flags;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text: glyph-run cache over a font backend
 *
 * One cache for every platform: a run is a (font slot, UTF-8 string) pair
 * with its advance and, once drawn, an 8-bit coverage mask. Two backends
 * fill slots and masks:
 *  - GDI (Windows, with BUI_TEXT_GDI): the system's hinted grayscale AA.
 *  - Portable (stb_truetype, the default everywhere): Inter is embedded as
 *    the default face; others come from BUI_text_add_font(), the BUI_FONT /
 *    BUI_FONT_DIR environment variables, or the platform's font folders.
 * \{ */

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include "gpu_text_unicode.inc.c"

#if defined(_WIN32) && defined(BUI_TEXT_GDI)
#  define TEXT_GDI 1
#else
#  define TEXT_GDI 0
#endif

typedef struct FontSlot {
  char family[64];
  int px;   /* the size rounded to whole pixels (paddings, underlines) */
  int px64; /* BUI-Awra: the exact size, in 1/64 px (fractional sizes) */
  float size; /* BUI-Awra: the size asked for (HarfBuzz's scale, as Pango sets it) */
  int flags;
  int ascent;
  int height;
  int fb; /* BUI-Awra: a fallback slot of face fb - 1 (0: a family's slot) */
#if TEXT_GDI
  HFONT font;
#else
  int face;
  float scale;     /* font units -> pixels */
  int synth;       /* BUI_FONT_BOLD / ITALIC flags the face lacks */
  float embolden;  /* synthetic bold: extra pixels per glyph */
  void *ft_size;   /* FreeType: this slot's FT_Size */
  float opsz;      /* FreeType: optical size (Blender: the text size) */
  float wght;      /* BUI-Awra: weight on a variable face's wght axis (0: the face's) */
  float bmscale;   /* BUI-Awra: bitmap-strike faces: strike pixels -> text pixels */
  void *hb;        /* BUI-Awra: hb_font_t, made on first shaping */
  /* BUI-Awra: glyphs as GTK 4 renders them (cairo-ft in GSK's glyph cache) */
  int load;         /* FreeType load flags */
  int8_t hm;        /* opened with hinted metrics (BUI_text_set_hint_metrics) */
  float lscale;     /* opened at this layout scale (BUI_text_set_layout_scale) */
  int8_t mono;      /* no antialiasing: monochrome glyphs */
  int8_t ebold;     /* synthetic bold as cairo-ft does it (FT_GlyphSlot_Embolden) */
  int8_t xform;     /* the outlines are transformed by `mat` (synthetic oblique: the
                     * fontconfig pattern's matrix, as Pango gives it to cairo) */
  int8_t yph;       /* quarter-pixel rows too (GSK with GTK's hint style none) */
  float mat[4];     /* xx, xy, yx, yy (y up) */
  int8_t fc_aa, fc_hint, fc_autohint, fc_bitmap; /* the fontconfig pattern's (FontOpts) */
#endif
} FontSlot;

#if TEXT_GDI
#  define SLOT_MODE_MATCH(s) 1
#else
/* BUI-Awra: hinted metrics (GTK at scale 1 with gtk-hint-font-metrics, as
 * Awra 1 set it there): advances on whole pixels, as Pango rounds them.
 * Slots are opened for one mode: GTK renders glyphs differently in each. */
static int g_hint_metrics = 1;
/* BUI-Awra: device pixels per logical pixel: Pango lays text out at the
 * logical size (HarfBuzz positions in its Pango units), GSK scales it */
static float g_layout_scale = 1.0f;
#  define SLOT_MODE_MATCH(s) ((s)->hm == g_hint_metrics && (s)->lscale == g_layout_scale)
#endif

/* BUI-Awra: fallback faces take slots too (one per face and size) */
#define MAX_FONTS 96
static FontSlot g_fonts[MAX_FONTS];
static int g_font_count = 0;

/* glyphs of a run (portable engine; for GPU text): ids and positions,
 * built on first use */
typedef struct RunGlyphs {
  int ng, cap;
  uint16_t *gid;
  float *gx;
  float *gy;      /* BUI-Awra: y offsets (marks; y down), NULL when all are 0 */
  uint8_t *gslot; /* BUI-Awra: the font slot of each glyph (fallback fonts) */
} RunGlyphs;

/* BUI-Awra: caret geometry of a run, built on first use: its clusters in
 * logical order and the cursor positions (per byte, len + 1) */
typedef struct RunCarets {
  int n;
  struct RunCluster *cl;
  uint8_t *cur;
} RunCarets;

typedef struct TextRun {
  uint64_t hash;
  char *text;
  uint8_t *mask;   /* coverage, built on first draw */
  RunGlyphs *gl;   /* GPU text: NULL until asked for */
  RunCarets *car;  /* BUI-Awra: NULL until asked for */
  /* BUI-Awra: colour glyphs (emoji), drawn from the glyph cache over the
   * mask: their slot, glyph and offset from the run's origin (baseline) */
  struct RunColor {
    uint16_t glyph;
    uint8_t slot;
    int16_t x, y;
  } *col;
  int len;
  int pfrom, pto; /* BUI-Awra: a part of the run (pto > pfrom): only the glyphs of those
                   * bytes, placed as in the whole run */
  int mw;   /* mask width */
  float advance;
  float ext_asc, ext_desc; /* BUI-Awra: its fonts' logical extents (when uses_font) */
  int16_t font;            /* its slot (< MAX_FONTS) */
  int16_t ncol;
  int16_t mh, ox;          /* mask height; ox: left padding baked into the mask */
  int16_t asc;             /* BUI-Awra: the baseline's row in the mask */
  int16_t ls;              /* BUI-Awra: letter spacing, Pango units (BuiTextContext) */
  uint8_t pre, post;       /* BUI-Awra: bytes of context before / after `text` (BuiTextContext) */
  uint8_t dir : 2;         /* BUI-Awra: 0 a paragraph of its own, 1 one LTR level, 2 one RTL level */
  uint8_t built : 1;       /* the mask was made */
  uint8_t uses_font : 1;   /* BUI-Awra: ext_asc / ext_desc are known (not with GDI) */
  uint8_t hyphen : 2;      /* BUI-Awra: BuiTextContext.hyphen */
  uint8_t ydep : 1;        /* BUI-Awra: glyphs off the baseline: the mask depends on sy */
  int16_t sx, sy;          /* BUI-Awra: the origin's offset from whole pixels the mask was
                            * built for, in 1/32768 px ([-16384, 16384)) */
} TextRun;

/* BUI-Awra: a cluster of a run (characters shaped together): its byte
 * range, its leading and trailing edges (x from the run's left edge) and,
 * for ligature carets, its one spacing glyph. */
struct RunCluster {
  int start, end;
  float lead, trail;
  float lig_dx; /* the spacing glyph's x offset */
  uint16_t lig; /* the spacing glyph, 0: none (or several) */
  uint8_t slot, rtl;
};

#define RUN_CAP 8192 /* power of two */
#define RUN_MAX_FILL 6000
static TextRun *g_runs = NULL;
static int g_run_count = 0;
static unsigned g_text_gen = 1; /* bumped whenever runs or fonts are freed */

/* backend */
static int be_font_open(FontSlot *s, const char *family, int flags, int px);
static void be_font_close(FontSlot *s);
/* the run's width; r_ext: the ascent and descent of its logical extents
 * and 1 when they are known (0: the font's metrics stand for them) */
static float be_measure(const TextRun *e, float r_ext[3]);
static void be_build(TextRun *e, const FontSlot *s);
static void be_flush_glyphs(void);

static void runs_flush(void)
{
  g_text_gen++;
  if (!g_runs) {
    return;
  }
  for (int i = 0; i < RUN_CAP; i++) {
    if (g_runs[i].text) {
      free(g_runs[i].text - g_runs[i].pre);
    }
    free(g_runs[i].mask);
    if (g_runs[i].gl) {
      free(g_runs[i].gl->gid);
      free(g_runs[i].gl->gx);
      free(g_runs[i].gl->gy);
      free(g_runs[i].gl->gslot);
      free(g_runs[i].gl);
    }
    free(g_runs[i].col);
    if (g_runs[i].car) {
      free(g_runs[i].car->cl);
      free(g_runs[i].car->cur);
      free(g_runs[i].car);
    }
  }
  memset(g_runs, 0, sizeof(TextRun) * RUN_CAP);
  g_run_count = 0;
}

static void fonts_flush(void)
{
  g_text_gen++;
  for (int i = 0; i < g_font_count; i++) {
    be_font_close(&g_fonts[i]);
  }
  g_font_count = 0;
  be_flush_glyphs();
  runs_flush(); /* runs reference font slots */
}

static int font_get(const char *family, int flags, float size)
{
  if (!family || !family[0] || strcmp(family, "TkDefaultFont") == 0) {
    family = "";
  }
  if (!(size >= 1.0f)) {
    size = 1.0f;
  }
  if (size > 512.0f) {
    size = 512.0f;
  }
  const int px64 = (int)(size * 64.0f + 0.5f);
  const int px = (int)(size + 0.5f);
  for (int i = 0; i < g_font_count; i++) {
    if (g_fonts[i].px64 == px64 && g_fonts[i].flags == flags && !g_fonts[i].fb &&
        SLOT_MODE_MATCH(&g_fonts[i]) && strcmp(g_fonts[i].family, family) == 0)
    {
      return i;
    }
  }
  if (g_font_count == MAX_FONTS) {
    fonts_flush();
  }
  FontSlot *s = &g_fonts[g_font_count];
  memset(s, 0, sizeof(*s));
  strncpy(s->family, family, sizeof(s->family) - 1);
  s->px = px;
  s->px64 = px64;
  s->size = size;
  s->flags = flags;
  if (!be_font_open(s, family, flags, px)) {
    return -1;
  }
  return g_font_count++;
}

#define TEXT_DIRS (BUI_TEXT_LTR | BUI_TEXT_RTL)

static int text_dir(int flags)
{
  return (flags & BUI_TEXT_RTL) ? 2 : (flags & BUI_TEXT_LTR) ? 1 : 0;
}

/* BUI-Awra: the shaping context of the runs looked up next
 * (BUI_text_set_context): up to 5 code points each side, as many as
 * HarfBuzz keeps (HB_BUFFER_CONTEXT_LENGTH). */
static struct {
  char pre[20], post[20];
  int pre_len, post_len;
  int ls, hyphen;
} g_ctx;

void BUI_text_set_context(const BuiTextContext *ctx)
{
  memset(&g_ctx, 0, sizeof(g_ctx));
  if (!ctx) {
    return;
  }
  if (ctx->before && ctx->before_len > 0) {
    int a = ctx->before_len;
    for (int k = 0; k < 5 && a > 0; k++) {
      do {
        a--;
      } while (a > 0 && ((unsigned char)ctx->before[a] & 0xC0) == 0x80);
    }
    g_ctx.pre_len = ctx->before_len - a;
    memcpy(g_ctx.pre, ctx->before + a, (size_t)g_ctx.pre_len);
  }
  if (ctx->after && ctx->after_len > 0) {
    int b = 0;
    for (int k = 0; k < 5 && b < ctx->after_len; k++) {
      b += utf8_step(ctx->after, b);
    }
    if (b > ctx->after_len) {
      b = ctx->after_len;
    }
    g_ctx.post_len = b;
    memcpy(g_ctx.post, ctx->after, (size_t)b);
  }
  const int ls = ctx->letter_spacing;
  g_ctx.ls = ls < -32767 ? -32767 : ls > 32767 ? 32767 : ls;
  g_ctx.hyphen = ctx->hyphen == 1 || ctx->hyphen == 2 ? ctx->hyphen : 0;
}

static uint64_t run_hash(int font, int dir, const char *s, int len, int pfrom, int pto)
{
  uint64_t h = 1469598103934665603ull ^ (uint64_t)((font * 4 + dir) * 2654435761u) ^
               ((uint64_t)(uint32_t)pfrom << 40) ^ ((uint64_t)(uint32_t)pto << 20);
  for (int i = 0; i < len; i++) {
    h ^= (uint8_t)s[i];
    h *= 1099511628211ull;
  }
  if (g_ctx.pre_len | g_ctx.post_len | g_ctx.ls | g_ctx.hyphen) {
    h ^= (uint64_t)(uint32_t)(g_ctx.ls * 4 + g_ctx.hyphen) * 0x9E3779B97F4A7C15ull;
    for (int i = 0; i < g_ctx.pre_len; i++) {
      h = (h ^ (uint8_t)g_ctx.pre[i]) * 1099511628211ull;
    }
    h = (h ^ 0xFF) * 1099511628211ull;
    for (int i = 0; i < g_ctx.post_len; i++) {
      h = (h ^ (uint8_t)g_ctx.post[i]) * 1099511628211ull;
    }
  }
  return h ? h : 1;
}

static TextRun *run_get_part(int font, int dir, const char *s, int len, int pfrom, int pto)
{
  if (!g_runs) {
    g_runs = (TextRun *)calloc(RUN_CAP, sizeof(TextRun));
    if (!g_runs) {
      return NULL;
    }
  }
  const uint64_t h = run_hash(font, dir, s, len, pfrom, pto);
  int idx = (int)(h & (RUN_CAP - 1));
  for (;;) {
    TextRun *e = &g_runs[idx];
    if (e->hash == 0) {
      break;
    }
    if (e->hash == h && e->font == font && e->dir == dir && e->len == len && e->pfrom == pfrom &&
        e->pto == pto && e->ls == g_ctx.ls && e->hyphen == g_ctx.hyphen &&
        e->pre == g_ctx.pre_len && e->post == g_ctx.post_len &&
        memcmp(e->text, s, (size_t)len) == 0 &&
        memcmp(e->text - e->pre, g_ctx.pre, (size_t)e->pre) == 0 &&
        memcmp(e->text + len, g_ctx.post, (size_t)e->post) == 0)
    {
      return e;
    }
    idx = (idx + 1) & (RUN_CAP - 1);
  }
  if (g_run_count >= RUN_MAX_FILL) {
    runs_flush();
    return run_get_part(font, dir, s, len, pfrom, pto);
  }
  TextRun *e = &g_runs[idx];
  char *buf = (char *)malloc((size_t)(g_ctx.pre_len + len + g_ctx.post_len) + 1);
  if (!buf) {
    return NULL;
  }
  memcpy(buf, g_ctx.pre, (size_t)g_ctx.pre_len);
  memcpy(buf + g_ctx.pre_len, s, (size_t)len);
  memcpy(buf + g_ctx.pre_len + len, g_ctx.post, (size_t)g_ctx.post_len);
  buf[g_ctx.pre_len + len + g_ctx.post_len] = 0;
  e->text = buf + g_ctx.pre_len;
  e->pre = (uint8_t)g_ctx.pre_len;
  e->post = (uint8_t)g_ctx.post_len;
  e->ls = (int16_t)g_ctx.ls;
  e->hyphen = (uint8_t)g_ctx.hyphen;
  e->hash = h;
  e->font = (int16_t)font;
  e->dir = (uint8_t)dir;
  e->len = len;
  e->pfrom = pfrom;
  e->pto = pto;
  g_run_count++;
  float ext[3] = {0.0f, 0.0f, 1.0f};
  e->advance = be_measure(e, ext);
  e->ext_asc = ext[0];
  e->ext_desc = ext[1];
  e->uses_font = ext[2] != 0.0f;
  return e;
}

static TextRun *run_get(int font, int dir, const char *s, int len)
{
  return run_get_part(font, dir, s, len, 0, 0);
}

int BUI_text_native(void)
{
  /* the portable engine reports 1 once a face can be found */
  return font_get("", 0, 12.0f) >= 0;
}

int BUI_text_measure(
    const char *family, int flags, float size, const char *utf8, int len, float *r_width)
{
  *r_width = 0.0f;
  if (len <= 0) {
    return 1;
  }
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  if (f < 0) {
    return 0;
  }
  TextRun *e = run_get(f, text_dir(flags), utf8, len);
  if (!e) {
    return 0;
  }
  *r_width = e->advance;
  return 1;
}

unsigned BUI_text_generation(void)
{
  return g_text_gen;
}

const BuiTextRun *BUI_text_prepare(
    const char *family, int flags, float size, const char *utf8, int len, float *r_width)
{
  if (r_width) {
    *r_width = 0.0f;
  }
  if (len <= 0) {
    return NULL;
  }
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  if (f < 0) {
    return NULL;
  }
  TextRun *e = run_get(f, text_dir(flags), utf8, len);
  if (!e) {
    return NULL;
  }
  if (r_width) {
    *r_width = e->advance;
  }
  if (!e->built) {
    e->built = 1;
    if (e->advance > 0.0f) {
      be_build(e, &g_fonts[e->font]);
    }
  }
  /* NULL: whitespace-only */
  return e->mask || e->ncol ? (const BuiTextRun *)e : NULL;
}

/* BUI-Awra: the run's mask for drawing at x, y. GSK places each glyph at
 * the quarter pixel of its absolute x and on the whole row of its absolute
 * y, so the mask is built for the origin's offset from whole pixels (the
 * mask itself is drawn on whole pixels): a run drawn at x.5 has its glyphs
 * at the phases GTK gives them. Rebuilt when drawn elsewhere (once for
 * text that stays put). */
static void run_build_at(TextRun *e, float x, float y)
{
  const int fx = clampi((int)floorf((x - floorf(x + 0.5f)) * 32768.0f + 0.5f), -16384, 16383);
  const int fy = clampi((int)floorf((y - floorf(y + 0.5f)) * 32768.0f + 0.5f), -16384, 16383);
  if (e->built && e->sx == fx && (!e->ydep || e->sy == fy)) {
    return;
  }
  if (e->built) {
    free(e->mask);
    e->mask = NULL;
    free(e->col);
    e->col = NULL;
    e->ncol = 0;
  }
  e->built = 1;
  e->sx = (int16_t)fx;
  e->sy = (int16_t)fy;
  if (e->advance > 0.0f) {
    be_build(e, &g_fonts[e->font]);
  }
}

int BUI_raster_text(BuiRaster *r,
                    float x,
                    float y,
                    const char *utf8,
                    int len,
                    float size,
                    const float rgba[4])
{
  if (len <= 0) {
    return 1;
  }
  const int f = font_get(r->font_family, r->font_flags & ~TEXT_DIRS, size);
  if (f < 0) {
    return 0;
  }
  TextRun *e = run_get(f, text_dir(r->font_flags), utf8, len);
  if (!e) {
    return 1;
  }
  if (r->gpu) { /* GPU text places glyphs itself */
    if (!e->built) {
      run_build_at(e, 0.0f, 0.0f);
    }
  }
  else {
    run_build_at(e, x, y);
  }
  if (e->mask || e->ncol) {
    BUI_raster_text_run(r, x, y, (const BuiTextRun *)e, rgba);
  }
  return 1;
}

static int gpu_text_glyphs(BuiRaster *r, TextRun *e, float x, float y, const float rgba[4]);
static void run_draw_color(BuiRaster *r, const TextRun *e, float x, float y, float alpha);

void BUI_raster_text_run(BuiRaster *r, float x, float y, const BuiTextRun *run, const float rgba[4])
{
  const TextRun *e = (const TextRun *)run;
  const Paint p = paint_from(rgba);
  if (!e || (!e->mask && !e->ncol) || p.a <= 0) {
    return;
  }
  if (e->ncol) {
    /* BUI-Awra: colour glyphs keep their colours; the text's alpha fades them */
    run_draw_color(r, e, x, y, rgba[3]);
    if (!e->mask) {
      return;
    }
  }
  const int dx = (int)floorf(x + 0.5f) - e->ox;
  const int dy = (int)floorf(y + 0.5f) - e->asc;
  const int x0 = clampi(dx, r->cx0, r->cx1), x1 = clampi(dx + e->mw, r->cx0, r->cx1);
  const int y0 = clampi(dy, r->cy0, r->cy1), y1 = clampi(dy + e->mh, r->cy0, r->cy1);
  if (r->gpu && gpu_text_glyphs(r, (TextRun *)e, x, y, rgba)) {
    return;
  }
  if (r->gpu) {
    int ax, ay;
    BuiGpuInst *in = x1 > x0 && y1 > y0 &&
                             bui_gpu_mask(r->gpu, e, g_text_gen, e->mask, e->mw, e->mh, &ax, &ay) ?
                         gpu_inst(r, BG_MASK, x0, y0, x1, y1) :
                         NULL;
    if (in) {
      v4(in->rect, (float)dx, (float)dy, (float)e->mw, (float)e->mh);
      v4(in->uv, (float)ax, (float)ay, 0.0f, 0.0f);
      c4(in->c0, rgba);
    }
    return;
  }
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const uint8_t *m = e->mask + (size_t)(py - dy) * e->mw - dx;
    for (int px = x0; px < x1; px++) {
      const uint32_t cov = m[px];
      if (cov == 0) {
        continue;
      }
      const uint32_t fac = (uint32_t)p.a * cov; /* 0..65280 */
      if (fac >= 65280) {
        row[px] = p.rgb;
      }
      else {
        row[px] = blend_cov(row[px], p.rgb, fac);
      }
    }
  }
}

static int run_glyphs(TextRun *e);
static int glyph_is_color(int slot, int glyph);
static int glyph_phase(int slot, float pen, int *r_whole);
static int glyph_yphase(int slot, float y, int *r_whole);
static int glyph_slug_var(int slot, unsigned *r_var);
static int glyph_draw_info(const TextRun *e, int i, int *r_face, float *r_scale, float *r_embolden,
                           int *r_baked, float *r_shear);
static const void *glyph_bitmap(const TextRun *e, int i, float pen, float yfrac, int *r_x, int *r_y,
                                int *r_w, int *r_h, const uint8_t **r_bits);

/* GPU text, one instance per glyph: Slug (outline curves evaluated in the
 * pixel shader) when the renderer supports it, else glyph bitmaps from the
 * shared glyph cache packed into the GPU atlas (Blender's BLF approach).
 * Returns 0 to fall back to the run's mask (no glyph data). */
static int gpu_text_glyphs(BuiRaster *r, TextRun *e, float x, float y, const float rgba[4])
{
  if (!run_glyphs(e)) {
    return 0;
  }
  const RunGlyphs *gl = e->gl;
  if (gl->ng == 0) {
    return 1;
  }
  int face = -1;
  float scale = 0.0f, embolden = 0.0f, shear = 0.0f;
  int passes = 1, info_slot = -1, baked = 0, slot_slug = 0;
  unsigned var = 0;
  const FontSlot *fs = &g_fonts[e->font];
  const float ox = floorf(x + 0.5f), base = floorf(y + 0.5f);
  const int slug = BUI_gpu_slug() && bui_gpu_slug_on(r->gpu);
  for (int i = 0; i < gl->ng; i++) {
    /* BUI-Awra: each glyph's own slot (fallback fonts); colour glyphs are
     * drawn by run_draw_color */
    const int gslot = gl->gslot ? gl->gslot[i] : e->font;
    if (gslot != info_slot) {
      info_slot = gslot;
      if (!glyph_draw_info(e, i, &face, &scale, &embolden, &baked, &shear)) {
        return 0;
      }
      passes = embolden > 0.0f ? 1 + (int)(embolden + 0.5f) : 1; /* synthetic bold */
      slot_slug = slug && glyph_slug_var(gslot, &var);
    }
    if (glyph_is_color(gslot, gl->gid[i])) {
      continue;
    }
    if (slot_slug) {
      float bbox[4], band[4];
      int loc[4];
      /* cairo-ft's synthetic bold: the outline emboldened, one pass */
      if (!bui_gpu_slug_glyph(r->gpu, face, gl->gid[i], var, 0 /* faces are never freed */, bbox, band,
                              loc)) {
        continue; /* empty glyph (space) */
      }
      /* where the bitmap path puts the glyph: GSK's quarter-pixel phase of
       * the absolute x, whole (or quarter) rows of the absolute y */
      int whole, row;
      const int ph = glyph_phase(gslot, gl->gx[i] + (x - ox), &whole);
      const int yph = glyph_yphase(gslot, y - base + (gl->gy ? gl->gy[i] : 0.0f), &row) >> 2;
      const float pen = ox + (float)whole + 0.25f * (float)ph;
      const float gb = base + (float)row + 0.25f * (float)yph;
      /* screen box of the glyph (font units, y up -> pixels, y down), plus shear and AA */
      const float top = gb - bbox[3] * scale, bottom = gb - bbox[1] * scale;
      const float lean = shear * (bbox[3] > 0.0f ? bbox[3] : 0.0f) * scale;
      for (int k = 0; k < (baked ? 1 : passes); k++) {
        const float gx = pen + (float)k;
        int x0, y0, x1, y1;
        if (!clip_box(r, gx + bbox[0] * scale - 1.0f, top - 1.0f, gx + bbox[2] * scale + lean + 1.0f,
                      bottom + 1.0f, &x0, &y0, &x1, &y1)) {
          continue;
        }
        BuiGpuInst *in = gpu_inst(r, BG_SLUG, x0, y0, x1, y1);
        if (in) {
          v4(in->rect, gx, gb, scale, shear);
          c4(in->c0, rgba);
          v4(in->c1, band[0], band[1], band[2], band[3]);
          v4(in->uv, (float)loc[0], (float)loc[1], (float)loc[2], (float)loc[3]);
        }
      }
    }
    else {
      int gx, gy, gw, gh;
      const uint8_t *bits;
      /* the phase of the glyph's absolute x, as GSK takes it */
      const void *key = glyph_bitmap(e, i, gl->gx[i] + (x - ox), y - base, &gx, &gy, &gw, &gh, &bits);
      int ax, ay;
      if (!key || gw <= 0 || gh <= 0 || !bui_gpu_mask(r->gpu, key, g_text_gen, bits, gw, gh, &ax, &ay)) {
        continue;
      }
      for (int k = 0; k < (baked ? 1 : passes); k++) {
        const int px = (int)ox + gx + k, py = (int)base + gy;
        int x0, y0, x1, y1;
        if (!clip_box(r, (float)px, (float)py, (float)(px + gw), (float)(py + gh), &x0, &y0, &x1, &y1)) {
          continue;
        }
        BuiGpuInst *in = gpu_inst(r, BG_MASK, x0, y0, x1, y1);
        if (in) {
          v4(in->rect, (float)px, (float)py, (float)gw, (float)gh);
          v4(in->uv, (float)ax, (float)ay, 0.0f, 0.0f);
          c4(in->c0, rgba);
        }
      }
    }
  }
  if (fs->flags & BUI_FONT_UNDERLINE) {
    const float t = fs->px / 14 > 1 ? (float)(fs->px / 14) : 1.0f;
    const float uy = base + (float)((fs->height - fs->ascent) / 2);
    BUI_raster_fill_rect(r, ox, uy, e->advance, t, rgba);
  }
  return 1;
}

/* Text runs for GPU renderers: mask size, left padding and ascent. */
const uint8_t *BUI_text_run_mask(const BuiTextRun *run, int *r_w, int *r_h, int *r_ox, int *r_ascent)
{
  const TextRun *e = (const TextRun *)run;
  if (!e || !e->mask) {
    return NULL;
  }
  *r_w = e->mw;
  *r_h = e->mh;
  *r_ox = e->ox;
  *r_ascent = e->asc;
  return e->mask;
}

/** \} */

#if TEXT_GDI
/* -------------------------------------------------------------------- */
/** \name Text backend: GDI
 * \{ */

/* GDI does its own font fallback: breaks are tailored by script runs */
static void text_font_items(int slot, const uint32_t *cp, int n, uint8_t **r_start, hb_script_t **r_script)
{
  (void)slot;
  (void)cp;
  (void)n;
  *r_start = NULL;
  *r_script = NULL;
}

#  ifndef BUI_NO_INTER
#    include "../extern/font_inter.h"
const void *BUI_text_default_font(int *r_len)
{
  if (r_len) {
    *r_len = (int)sizeof(g_font_inter_variable);
  }
  return g_font_inter_variable;
}
#  else
const void *BUI_text_default_font(int *r_len)
{
  if (r_len) {
    *r_len = 0;
  }
  return NULL;
}
#  endif

static HDC g_dc = NULL;

static HDC text_dc(void)
{
  if (!g_dc) {
    g_dc = CreateCompatibleDC(NULL);
    SetBkMode(g_dc, TRANSPARENT);
    SetTextAlign(g_dc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
  }
  return g_dc;
}

static int be_font_open(FontSlot *s, const char *family, int flags, int px)
{
  if (!family[0]) {
    family = "Segoe UI";
  }
  wchar_t wfam[64];
  if (MultiByteToWideChar(CP_UTF8, 0, family, -1, wfam, 64) == 0) {
    wcscpy(wfam, L"Segoe UI");
  }
  HFONT font = CreateFontW(-px, 0, 0, 0, (flags & BUI_FONT_BOLD) ? FW_BOLD : FW_NORMAL,
                           (flags & BUI_FONT_ITALIC) ? TRUE : FALSE,
                           (flags & BUI_FONT_UNDERLINE) ? TRUE : FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, wfam);
  if (!font) {
    return 0;
  }
  s->font = font;
  HDC dc = text_dc();
  HGDIOBJ old = SelectObject(dc, font);
  TEXTMETRICW tm;
  GetTextMetricsW(dc, &tm);
  SelectObject(dc, old);
  s->ascent = tm.tmAscent;
  s->height = tm.tmHeight;
  return 1;
}

static void be_font_close(FontSlot *s)
{
  DeleteObject(s->font);
}

static void be_flush_glyphs(void) {}

static int run_glyphs(TextRun *e)
{
  (void)e;
  return 0;
}

static int glyph_is_color(int slot, int glyph)
{
  (void)slot;
  (void)glyph;
  return 0;
}

static void run_draw_color(BuiRaster *r, const TextRun *e, float x, float y, float alpha)
{
  (void)r;
  (void)e;
  (void)x;
  (void)y;
  (void)alpha;
}

void BUI_text_set_png_decoder(BuiPngDecodeFn fn)
{
  (void)fn;
}
static int glyph_draw_info(const TextRun *e, int i, int *r_face, float *r_scale, float *r_embolden,
                           int *r_baked, float *r_shear)
{
  (void)e;
  (void)i;
  (void)r_face;
  (void)r_scale;
  (void)r_embolden;
  (void)r_baked;
  (void)r_shear;
  return 0;
}
static int glyph_phase(int slot, float pen, int *r_whole)
{
  (void)slot;
  *r_whole = (int)floorf(pen + 0.5f);
  return 0;
}
static int glyph_yphase(int slot, float y, int *r_whole)
{
  (void)slot;
  *r_whole = (int)floorf(y + 0.5f);
  return 0;
}
static int glyph_slug_var(int slot, unsigned *r_var)
{
  (void)slot;
  *r_var = 0;
  return 0;
}
static const void *glyph_bitmap(const TextRun *e, int i, float pen, float yfrac, int *r_x, int *r_y,
                                int *r_w, int *r_h, const uint8_t **r_bits)
{
  (void)e;
  (void)i;
  (void)pen;
  (void)yfrac;
  (void)r_x;
  (void)r_y;
  (void)r_w;
  (void)r_h;
  (void)r_bits;
  return NULL;
}
int bui_text_glyph_outline(int face, int glyph, unsigned var, float **r_curves, int *r_n,
                           float r_bbox[4])
{
  (void)face;
  (void)glyph;
  (void)var;
  (void)r_bbox;
  *r_curves = NULL;
  *r_n = 0;
  return 0;
}

static wchar_t *to_wide(const char *s, int len, int *r_n)
{
  int n = MultiByteToWideChar(CP_UTF8, 0, s, len, NULL, 0);
  wchar_t *w = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)(n + 1));
  if (!w) {
    *r_n = 0;
    return NULL;
  }
  MultiByteToWideChar(CP_UTF8, 0, s, len, w, n);
  w[n] = 0;
  *r_n = n;
  return w;
}

static float gdi_measure(const FontSlot *s, int dir, const char *utf8, int len, float r_ext[3])
{
  r_ext[0] = r_ext[1] = 0.0f;
  r_ext[2] = 1.0f;
  int wn;
  wchar_t *w = to_wide(utf8, len, &wn);
  HDC dc = text_dc();
  HGDIOBJ old = SelectObject(dc, s->font);
  SIZE sz = {0, 0};
  (void)dir; /* GDI orders and shapes runs itself */
  if (w && wn > 0) {
    GetTextExtentPoint32W(dc, w, wn, &sz);
  }
  SelectObject(dc, old);
  free(w);
  return (float)sz.cx;
}

/* GDI shapes runs whole: the context of BUI_text_set_context is not used */
static float be_measure(const TextRun *e, float r_ext[3])
{
  const float w = gdi_measure(&g_fonts[e->font], e->dir, e->text, e->len, r_ext);
  r_ext[2] = 0.0f; /* the font's metrics stand for the run's extents */
  return w;
}

static void be_build(TextRun *e, const FontSlot *fs)
{
  int wn;
  wchar_t *w = to_wide(e->text, e->len, &wn);
  if (!w || wn == 0) {
    free(w);
    return;
  }
  const int pad = 2 + fs->px / 4; /* bearings / italic overhang */
  const int mw = (int)e->advance + pad * 2;
  const int mh = fs->height + 1;
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = mw;
  bmi.bmiHeader.biHeight = -mh;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void *bits = NULL;
  HDC dc = text_dc();
  HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!bmp || !bits) {
    free(w);
    return;
  }
  memset(bits, 0, (size_t)mw * mh * 4);
  HGDIOBJ old_bmp = SelectObject(dc, bmp);
  HGDIOBJ old_font = SelectObject(dc, fs->font);
  SetTextColor(dc, RGB(255, 255, 255));
  TextOutW(dc, pad, 0, w, wn);
  GdiFlush();
  e->mask = (uint8_t *)malloc((size_t)mw * mh);
  if (e->mask) {
    const uint32_t *src = (const uint32_t *)bits;
    for (int i = 0; i < mw * mh; i++) {
      /* grayscale AA: channels match; take green (perceptual) */
      e->mask[i] = (uint8_t)((src[i] >> 8) & 0xFF);
    }
    e->mw = mw;
    e->mh = (int16_t)mh;
    e->ox = (int16_t)pad;
    e->asc = (int16_t)fs->ascent;
  }
  SelectObject(dc, old_font);
  SelectObject(dc, old_bmp);
  DeleteObject(bmp);
  free(w);
}

int BUI_text_add_font(const char *family, int flags, const void *data, int len)
{
  (void)flags;
  /* private to this process; addressable by the font's own family name */
  DWORD n = 0;
  HANDLE h = AddFontMemResourceEx((void *)data, (DWORD)len, NULL, &n);
  (void)family;
  if (h && n > 0) {
    fonts_flush();
    return 1;
  }
  return 0;
}

/** \} */

/* BUI-Awra: caret geometry from prefix widths (GDI orders runs itself). */
static float run_index_to_x(TextRun *e, int index, int trailing)
{
  if (trailing && index < e->len) {
    index += utf8_step(e->text, index);
  }
  index = index < 0 ? 0 : index > e->len ? e->len : index;
  float ext[3];
  return index > 0 ? gdi_measure(&g_fonts[e->font], 0, e->text, index, ext) : 0.0f;
}

static int run_x_to_index(TextRun *e, float x, int *r_trailing)
{
  float prev = 0.0f;
  int start = 0;
  *r_trailing = 0;
  for (int i = 0; i < e->len;) {
    const int next = i + utf8_step(e->text, i);
    const float w = run_index_to_x(e, next, 0);
    if (x < w) {
      *r_trailing = x >= (prev + w) * 0.5f;
      return i;
    }
    prev = w;
    start = i;
    i = next;
  }
  *r_trailing = e->len > 0;
  return start;
}

#else /* portable */
/* -------------------------------------------------------------------- */
/** \name Text backend: stb_truetype
 * \{ */

#  if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wunused-function"
#    pragma GCC diagnostic ignored "-Wsign-compare"
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wcast-qual"
#  endif
#  define STBTT_STATIC
#  define STB_TRUETYPE_IMPLEMENTATION
#  include "../extern/stb_truetype.h"
#  if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#  endif
#  include <stdio.h>
#  if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#    include <dlfcn.h>
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#  endif

/* FreeType (BUI_TEXT_FT, the default): glyphs are loaded, hinted and
 * rasterized as GTK 4 has cairo-ft do it (BUI-Awra: glyph_raster_ft), the
 * variable Inter at the text's optical size. stb_truetype still reads
 * cmaps, metrics and kern tables. */
#  ifdef BUI_TEXT_FT
#    define TEXT_FT 1
#    include <ft2build.h>
#    include FT_FREETYPE_H
#    include FT_MULTIPLE_MASTERS_H
#    include FT_OUTLINE_H
#    include FT_SIZES_H
#    include FT_ADVANCES_H
#    include FT_TRUETYPE_TABLES_H
#    include FT_SYNTHESIS_H
static FT_Library g_ft = NULL;
#  else
#    define TEXT_FT 0
#  endif

typedef struct Face {
  char key[96]; /* file path, or "mem:<family>" */
  unsigned char *data;
  stbtt_fontinfo info;
  int ascent, descent, line_gap; /* font units */
#  if TEXT_FT
  FT_Face ft;
  FT_MM_Var *mm;            /* variation axes (NULL: not a variable font) */
  int ax_wght, ax_opsz;     /* axis indices, -1 when absent */
  float wght;               /* this face's weight on the wght axis */
  float set_wght, set_opsz; /* the design coordinates last set */
#  endif
  int os2_weight; /* BUI-Awra: usWeightClass (static faces: the weight they draw) */
  int mapped;     /* BUI-Awra: `data` is a read-only file mapping, not malloc'd */
  /* BUI-Awra: fallback and colour fonts */
  long len;       /* bytes of `data` */
  int fontstart;  /* offset of this face's table directory in `data` */
  int ft_only;    /* stb_truetype can't read it (a bitmap-only colour font): FreeType only */
  int color;      /* colour glyphs: CBDT / sbix bitmaps or COLR layers */
  int fixed;      /* bitmap strikes only (no outlines): scaled to the text size */
  int upem;       /* units per em */
  int hb_ascent, hb_descent; /* HarfBuzz's (Pango's) ascender / descender: hhea, or OS/2's
                              * typo metrics with USE_TYPO_METRICS */
  int index;      /* the face's index in its file (collections) */
  void *hb;       /* hb_face_t, made on first shaping */
  uint16_t *ascii; /* glyphs of U+0000..U+007F, looked up on first use (NULL: not yet) */
} Face;

#  define MAX_FACES 64
static Face g_faces[MAX_FACES];
static int g_face_count = 0;

/* registered (in-memory) fonts: family + style -> face */
typedef struct FaceReg {
  char family[64];
  int style; /* BUI_FONT_BOLD | BUI_FONT_ITALIC */
  int face;
} FaceReg;
static FaceReg g_regs[MAX_FACES];
static int g_reg_count = 0;

static int ieq(const char *a, const char *b)
{
  for (; *a && *b; a++, b++) {
    int ca = (unsigned char)*a, cb = (unsigned char)*b;
    if (ca >= 'A' && ca <= 'Z') {
      ca += 32;
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb += 32;
    }
    if (ca != cb) {
      return 0;
    }
  }
  return *a == *b;
}

#  if TEXT_FT
/* Index of the variation axis `tag` (e.g. 'wght'), or -1. */
static int ft_axis(const FT_MM_Var *mm, FT_ULong tag)
{
  for (FT_UInt i = 0; mm && i < mm->num_axis; i++) {
    if (mm->axis[i].tag == tag) {
      return (int)i;
    }
  }
  return -1;
}

static void ft_face_open(Face *f, const unsigned char *data, long len, int index)
{
  f->ft = NULL;
  f->mm = NULL;
  f->ax_wght = f->ax_opsz = -1;
  f->wght = 400.0f;
  f->set_wght = f->set_opsz = -1.0f;
  if (!g_ft && FT_Init_FreeType(&g_ft) != 0) {
    g_ft = NULL;
    return;
  }
  if (FT_New_Memory_Face(g_ft, data, (FT_Long)len, (FT_Long)index, &f->ft) != 0) {
    f->ft = NULL;
    return;
  }
  if (FT_HAS_MULTIPLE_MASTERS(f->ft) && FT_Get_MM_Var(f->ft, &f->mm) == 0) {
    f->ax_wght = ft_axis(f->mm, FT_MAKE_TAG('w', 'g', 'h', 't'));
    f->ax_opsz = ft_axis(f->mm, FT_MAKE_TAG('o', 'p', 's', 'z'));
    if (f->ax_wght >= 0) {
      f->wght = (float)f->mm->axis[f->ax_wght].def / 65536.0f;
    }
  }
}

/* Make `s` the active size of its face and set the variation axes it needs:
 * the face's weight, and the optical size as Blender sets it
 * (blf_glyph_set_variation_optical_size: the size, clamped to the axis). */
static FT_Face ft_use(const FontSlot *s)
{
  Face *f = &g_faces[s->face];
  if (!f->ft || !s->ft_size) {
    return NULL;
  }
  FT_Activate_Size((FT_Size)s->ft_size);
  const float wght = s->wght > 0.0f ? s->wght : f->wght;
  if (f->mm && (f->set_wght != wght || f->set_opsz != s->opsz)) {
    FT_Fixed coords[16];
    const FT_UInt n = f->mm->num_axis < 16 ? f->mm->num_axis : 16;
    if (FT_Get_Var_Design_Coordinates(f->ft, n, coords) == 0) {
      if (f->ax_wght >= 0 && f->ax_wght < (int)n) {
        const FT_Var_Axis *a = &f->mm->axis[f->ax_wght];
        FT_Fixed v = (FT_Fixed)(wght * 65536.0f);
        v = v < a->minimum ? a->minimum : v > a->maximum ? a->maximum : v;
        coords[f->ax_wght] = v;
      }
      if (f->ax_opsz >= 0 && f->ax_opsz < (int)n) {
        const FT_Var_Axis *a = &f->mm->axis[f->ax_opsz];
        FT_Fixed v = (FT_Fixed)(s->opsz * 65536.0f);
        v = v < a->minimum ? a->minimum : v > a->maximum ? a->maximum : v;
        coords[f->ax_opsz] = v;
      }
      FT_Set_Var_Design_Coordinates(f->ft, n, coords);
    }
    f->set_wght = wght;
    f->set_opsz = s->opsz;
  }
  return f->ft;
}
#  endif

/* BUI-Awra: a face stb_truetype can't read (no glyf / CFF outlines: a
 * colour bitmap font such as Noto Color Emoji), through FreeType alone. */
static int face_init_ft_only(Face *f, unsigned char *data, long len, int index)
{
#  if TEXT_FT
  ft_face_open(f, data, len, index);
  if (!f->ft) {
    return 0;
  }
  f->ft_only = 1;
  f->data = data;
  f->upem = f->ft->units_per_EM > 0 ? f->ft->units_per_EM : 2048;
  const TT_OS2 *os2 = (const TT_OS2 *)FT_Get_Sfnt_Table(f->ft, FT_SFNT_OS2);
  if (os2 && os2->version != 0xFFFF && (os2->usWinAscent || os2->usWinDescent)) {
    f->ascent = os2->usWinAscent;
    f->descent = -(int)os2->usWinDescent;
    f->os2_weight = os2->usWeightClass;
  }
  else {
    f->ascent = f->ft->ascender;
    f->descent = f->ft->descender;
  }
  f->line_gap = 0;
  if (f->os2_weight <= 0) {
    f->os2_weight = 400;
  }
  return 1;
#  else
  (void)f;
  (void)data;
  (void)len;
  (void)index;
  return 0;
#  endif
}

/* BUI-Awra: the ascender and descender HarfBuzz reports (hb_ot_metrics):
 * OS/2 typo metrics when USE_TYPO_METRICS is set, else hhea's, else win. */
static void face_hb_metrics(Face *f, const unsigned char *data, long len)
{
  const stbtt_uint32 hhea = stbtt__find_table((stbtt_uint8 *)data, (stbtt_uint32)f->fontstart, "hhea");
  const stbtt_uint32 os2 = stbtt__find_table((stbtt_uint8 *)data, (stbtt_uint32)f->fontstart, "OS/2");
  int asc = 0, desc = 0;
  if (os2 && (long)os2 + 78 <= len && (ttUSHORT((stbtt_uint8 *)data + os2 + 62) & 0x80)) {
    asc = ttSHORT((stbtt_uint8 *)data + os2 + 68);
    desc = ttSHORT((stbtt_uint8 *)data + os2 + 70);
  }
  if (!asc && !desc && hhea && (long)hhea + 8 <= len) {
    asc = ttSHORT((stbtt_uint8 *)data + hhea + 4);
    desc = ttSHORT((stbtt_uint8 *)data + hhea + 6);
  }
  if (!asc && !desc && os2 && (long)os2 + 78 <= len) {
    asc = ttUSHORT((stbtt_uint8 *)data + os2 + 74);
    desc = -(int)ttUSHORT((stbtt_uint8 *)data + os2 + 76);
  }
  f->hb_ascent = asc ? asc : f->ascent;
  f->hb_descent = desc ? desc : f->descent;
}

static int face_init_index(Face *f, unsigned char *data, long len, int index)
{
  const int off = stbtt_GetFontOffsetForIndex(data, index);
  if (off < 0) {
    return 0;
  }
  f->len = len;
  f->fontstart = off;
  f->index = index;
  if (!stbtt_InitFont(&f->info, data, off)) {
    if (!face_init_ft_only(f, data, len, index)) {
      return 0;
    }
  }
  else {
#  if TEXT_FT
    ft_face_open(f, data, len, index);
#  endif
    f->data = data;
    f->upem = ttUSHORT((stbtt_uint8 *)data + stbtt__find_table(data, (stbtt_uint32)off, "head") + 18);
  }
#  if TEXT_FT
  if (f->ft) {
    f->color = FT_HAS_COLOR(f->ft) ? 1 : 0;
    f->fixed = !FT_IS_SCALABLE(f->ft) && FT_HAS_FIXED_SIZES(f->ft);
  }
#  endif
  if (f->ft_only) {
    face_hb_metrics(f, data, len);
    return 1;
  }
  /* GDI's metrics: the OS/2 table's Windows ascent/descent when present */
  int a, d, g;
  if (stbtt_GetFontVMetricsOS2(&f->info, &a, &d, &g)) {
    int wa = 0, wd = 0;
    const stbtt_uint32 os2 = stbtt__find_table(f->info.data, (stbtt_uint32)f->info.fontstart, "OS/2");
    if (os2) {
      wa = ttUSHORT(f->info.data + os2 + 74);
      wd = ttUSHORT(f->info.data + os2 + 76);
      f->os2_weight = ttUSHORT(f->info.data + os2 + 4);
    }
    if (wa > 0 || wd > 0) {
      a = wa;
      d = -wd;
    }
  }
  else {
    stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
  }
  f->ascent = a;
  f->descent = d;
  face_hb_metrics(f, data, len);
  f->line_gap = g;
  if (f->os2_weight <= 0) {
    f->os2_weight = 400;
  }
  return 1;
}

static int face_init(Face *f, unsigned char *data, long len)
{
  return face_init_index(f, data, len, 0);
}

/* BUI-Awra: font files are mapped read-only, not copied: the pages are the
 * system's shared page cache (several processes using one font share them)
 * and only the parts read are ever loaded, instead of a private copy of a
 * whole (often multi-megabyte, variable) font in every process. */
static unsigned char *font_map(const char *path, long *r_len, int *r_mapped)
{
  *r_mapped = 0;
#  if defined(_WIN32)
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER size;
    unsigned char *data = NULL;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 30)) {
      HANDLE map = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
      if (map) {
        data = (unsigned char *)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(map);
      }
    }
    CloseHandle(file);
    if (data) {
      *r_len = (long)size.QuadPart;
      *r_mapped = 1;
      return data;
    }
  }
#  elif !defined(__EMSCRIPTEN__)
  const int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd >= 0) {
    struct stat st;
    void *data = MAP_FAILED;
    if (fstat(fd, &st) == 0 && st.st_size > 0 && st.st_size < (1L << 30)) {
      data = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    }
    close(fd);
    if (data != MAP_FAILED) {
      *r_len = (long)st.st_size;
      *r_mapped = 1;
      return (unsigned char *)data;
    }
  }
#  endif
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return NULL;
  }
  fseek(fp, 0, SEEK_END);
  const long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  unsigned char *data = n > 0 ? (unsigned char *)malloc((size_t)n) : NULL;
  if (!data || fread(data, 1, (size_t)n, fp) != (size_t)n) {
    free(data);
    fclose(fp);
    return NULL;
  }
  fclose(fp);
  *r_len = n;
  return data;
}

static void font_unmap(unsigned char *data, long len, int mapped)
{
  if (!mapped) {
    free(data);
    return;
  }
#  if defined(_WIN32)
  (void)len;
  UnmapViewOfFile(data);
#  elif !defined(__EMSCRIPTEN__)
  munmap(data, (size_t)len);
#  endif
}

/* A face of a font file: `index` is the face in a collection (.ttc). */
static int face_load_file_index(const char *path, int index)
{
  char key[sizeof(g_faces[0].key)];
  if (snprintf(key, sizeof(key), index ? "%s#%d" : "%s", path, index) >= (int)sizeof(key)) {
    return -1;
  }
  for (int i = 0; i < g_face_count; i++) {
    if (strcmp(g_faces[i].key, key) == 0) {
      return i;
    }
  }
  if (g_face_count == MAX_FACES) {
    return -1;
  }
  long n = 0;
  int mapped = 0;
  unsigned char *data = font_map(path, &n, &mapped);
  if (!data) {
    return -1;
  }
  Face *f = &g_faces[g_face_count];
  memset(f, 0, sizeof(*f));
  if (!face_init_index(f, data, n, index)) {
    font_unmap(data, n, mapped);
    return -1;
  }
  f->mapped = mapped;
  strcpy(f->key, key);
  return g_face_count++;
}

static int face_load_file(const char *path)
{
  return face_load_file_index(path, 0);
}

#  ifndef BUI_NO_INTER
#    include "../extern/font_inter.h"

/* Inter, embedded: the default UI face (Blender's), so text looks the same
 * on every platform. The variable font, as Blender ships it: with FreeType,
 * regular and bold are its weights 400 and 700 (BLF_BOLD: +300); without,
 * bold is synthesized. `bold`: 0 regular, 1 bold. -1 when it can't load. */
static int face_inter(int bold)
{
  static int ids[2] = {-2, -2};
  if (ids[bold] == -2) {
    ids[bold] = -1;
    if (g_face_count < MAX_FACES) {
      Face *f = &g_faces[g_face_count];
      memset(f, 0, sizeof(*f));
      /* stb_truetype and FreeType only read the data */
      unsigned char *data = (unsigned char *)g_font_inter_variable;
      if (face_init(f, data, (long)sizeof(g_font_inter_variable))) {
#    if TEXT_FT
        f->wght = bold ? 700.0f : 400.0f;
#    endif
        strcpy(f->key, bold ? "embed:inter-bold" : "embed:inter");
        ids[bold] = g_face_count++;
      }
    }
  }
  return ids[bold];
}

const void *BUI_text_default_font(int *r_len)
{
  if (r_len) {
    *r_len = (int)sizeof(g_font_inter_variable);
  }
  return g_font_inter_variable;
}
#  else
const void *BUI_text_default_font(int *r_len)
{
  if (r_len) {
    *r_len = 0;
  }
  return NULL;
}
#  endif

int BUI_text_add_font(const char *family, int flags, const void *data, int len)
{
  if (!data || len <= 0 || g_face_count == MAX_FACES || g_reg_count == MAX_FACES) {
    return 0;
  }
  unsigned char *copy = (unsigned char *)malloc((size_t)len);
  if (!copy) {
    return 0;
  }
  memcpy(copy, data, (size_t)len);
  Face *f = &g_faces[g_face_count];
  memset(f, 0, sizeof(*f));
  if (!face_init(f, copy, len)) {
    free(copy);
    return 0;
  }
  snprintf(f->key, sizeof(f->key), "mem:%s", family ? family : "");
  FaceReg *r = &g_regs[g_reg_count++];
  strncpy(r->family, family ? family : "", sizeof(r->family) - 1);
  r->family[sizeof(r->family) - 1] = '\0';
  r->style = flags & (BUI_FONT_BOLD | BUI_FONT_ITALIC);
  r->face = g_face_count++;
  fonts_flush(); /* the new face may replace a fallback */
  return 1;
}

/* Font folders, most specific first. */
static int font_dirs(char dirs[][260], int max)
{
  int n = 0;
  const char *env = getenv("BUI_FONT_DIR");
  if (env && env[0] && n < max) {
    snprintf(dirs[n++], 260, "%s", env);
  }
#  ifdef _WIN32
  const char *win = getenv("WINDIR");
  snprintf(dirs[n++], 260, "%s/Fonts", win ? win : "C:/Windows");
  const char *local = getenv("LOCALAPPDATA");
  if (local && n < max) {
    snprintf(dirs[n++], 260, "%s/Microsoft/Windows/Fonts", local);
  }
#  elif defined(__APPLE__)
  static const char *mac[] = {"/System/Library/Fonts", "/System/Library/Fonts/Supplemental",
                              "/Library/Fonts"};
  for (int i = 0; i < 3 && n < max; i++) {
    snprintf(dirs[n++], 260, "%s", mac[i]);
  }
  const char *home = getenv("HOME");
  if (home && n < max) {
    snprintf(dirs[n++], 260, "%s/Library/Fonts", home);
  }
#  else
  static const char *nix[] = {
      "/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/dejavu",
      "/usr/share/fonts/TTF",             "/usr/share/fonts/truetype/liberation",
      "/usr/share/fonts/liberation",      "/usr/share/fonts/truetype/noto",
      "/usr/share/fonts/noto",            "/usr/share/fonts/google-noto",
      "/usr/share/fonts/truetype",        "/usr/local/share/fonts",
      "/usr/share/fonts"};
  for (int i = 0; i < (int)(sizeof(nix) / sizeof(nix[0])) && n < max; i++) {
    snprintf(dirs[n++], 260, "%s", nix[i]);
  }
  const char *home = getenv("HOME");
  if (home && n + 1 < max) {
    snprintf(dirs[n++], 260, "%s/.local/share/fonts", home);
    snprintf(dirs[n++], 260, "%s/.fonts", home);
  }
#  endif
  return n;
}

/* File names per generic family and style (regular, bold, italic, both). */
typedef struct FamilyFiles {
  const char *names; /* '|'-separated aliases */
  const char *files[4];
} FamilyFiles;

static const FamilyFiles g_families[] = {
    {"|sans|sans-serif|inter|segoe ui|system-ui|helvetica|arial|dejavu sans|liberation sans|noto "
     "sans|tkdefaultfont|tktextfont|tkheadingfont|tkcaptionfont|tkmenufont|",
     {"segoeui.ttf|DejaVuSans.ttf|LiberationSans-Regular.ttf|NotoSans-Regular.ttf|Arial.ttf|"
      "arial.ttf|Helvetica.ttc|SFNS.ttf",
      "segoeuib.ttf|DejaVuSans-Bold.ttf|LiberationSans-Bold.ttf|NotoSans-Bold.ttf|Arial "
      "Bold.ttf|arialbd.ttf",
      "segoeuii.ttf|DejaVuSans-Oblique.ttf|LiberationSans-Italic.ttf|NotoSans-Italic.ttf|Arial "
      "Italic.ttf|ariali.ttf",
      "segoeuiz.ttf|DejaVuSans-BoldOblique.ttf|LiberationSans-BoldItalic.ttf|NotoSans-"
      "BoldItalic.ttf|Arial Bold Italic.ttf|arialbi.ttf"}},
    {"|mono|monospace|courier|courier new|consolas|menlo|dejavu sans mono|liberation "
     "mono|tkfixedfont|",
     {"consola.ttf|DejaVuSansMono.ttf|LiberationMono-Regular.ttf|NotoSansMono-Regular.ttf|"
      "Menlo.ttc|cour.ttf",
      "consolab.ttf|DejaVuSansMono-Bold.ttf|LiberationMono-Bold.ttf|NotoSansMono-Bold.ttf|"
      "courbd.ttf",
      "consolai.ttf|DejaVuSansMono-Oblique.ttf|LiberationMono-Italic.ttf|couri.ttf",
      "consolaz.ttf|DejaVuSansMono-BoldOblique.ttf|LiberationMono-BoldItalic.ttf|courbi.ttf"}},
    {"|serif|times|times new roman|georgia|dejavu serif|liberation serif|",
     {"times.ttf|DejaVuSerif.ttf|LiberationSerif-Regular.ttf|NotoSerif-Regular.ttf|Times New "
      "Roman.ttf|Times.ttc",
      "timesbd.ttf|DejaVuSerif-Bold.ttf|LiberationSerif-Bold.ttf|NotoSerif-Bold.ttf|Times New "
      "Roman Bold.ttf",
      "timesi.ttf|DejaVuSerif-Italic.ttf|LiberationSerif-Italic.ttf|NotoSerif-Italic.ttf|Times "
      "New Roman Italic.ttf",
      "timesbi.ttf|DejaVuSerif-BoldItalic.ttf|LiberationSerif-BoldItalic.ttf|NotoSerif-"
      "BoldItalic.ttf|Times New Roman Bold Italic.ttf"}},
};

static int family_index(const char *family)
{
  if (!family[0]) {
    return 0;
  }
  char probe[72];
  snprintf(probe, sizeof(probe), "|%s|", family);
  for (int i = 0; probe[i]; i++) {
    if (probe[i] >= 'A' && probe[i] <= 'Z') {
      probe[i] = (char)(probe[i] + 32);
    }
  }
  for (int i = 0; i < (int)(sizeof(g_families) / sizeof(g_families[0])); i++) {
    if (strstr(g_families[i].names, probe)) {
      return i;
    }
  }
  return -1;
}

/* The default face (empty, generic sans or Tk's named fonts), as opposed to
 * a named system family. */
static int family_generic(const char *family)
{
  static const char *names =
      "|sans|sans-serif|inter|system-ui|ui-sans-serif|tkdefaultfont|tktextfont|tkheadingfont|"
      "tkcaptionfont|tkmenufont|tksmallcaptionfont|tkiconfont|tktooltipfont|";
  char probe[72];
  if (!family[0]) {
    return 1;
  }
  snprintf(probe, sizeof(probe), "|%s|", family);
  for (int i = 0; probe[i]; i++) {
    if (probe[i] >= 'A' && probe[i] <= 'Z') {
      probe[i] = (char)(probe[i] + 32);
    }
  }
  return strstr(names, probe) != NULL;
}

/* First '|'-separated file found in any font folder. */
static int face_from_files(const char *files, char dirs[][260], int ndirs)
{
  const char *p = files;
  while (*p) {
    const char *end = strchr(p, '|');
    const int n = end ? (int)(end - p) : (int)strlen(p);
    for (int d = 0; d < ndirs; d++) {
      char path[600];
      snprintf(path, sizeof(path), "%s/%.*s", dirs[d], n, p);
      const int f = face_load_file(path);
      if (f >= 0) {
        return f;
      }
    }
    p += n + (end ? 1 : 0);
  }
  return -1;
}

/* BUI-Awra: how a face is to be rendered, from its fontconfig pattern as
 * cairo-ft reads it (_get_pattern_ft_options); without fontconfig, what its
 * defaults amount to. */
typedef struct FontOpts {
  int8_t antialias; /* FC_ANTIALIAS */
  int8_t hintstyle; /* FC_HINT_STYLE (0 none .. 3 full); FC_HINTING false: 0 */
  int8_t autohint;  /* FC_AUTOHINT */
  int8_t bitmap;    /* FC_EMBEDDED_BITMAP */
  int8_t embolden;  /* FC_EMBOLDEN; -1: no pattern (BUI decides synthesis) */
  int8_t matrix;    /* FC_MATRIX is set (90-synthetic.conf's oblique) */
  float mat[4];     /* its xx, xy, yx, yy */
} FontOpts;

static const FontOpts g_fo_default = {1, 3, 0, 0, -1, 0, {1, 0, 0, 1}};

#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
/* BUI-Awra: fontconfig, loaded at run time (never linked), resolves families
 * the way GTK / Pango do on Linux: aliases ("sans-serif" -> the desktop's
 * font), the nearest weight and slant, collections and variable fonts. */
typedef void FcPattern_;
typedef unsigned char FcChar8_;
static struct {
  int state; /* 0 untried, 1 loaded, -1 unavailable */
  FcPattern_ *(*PatternCreate)(void);
  int (*PatternAddString)(FcPattern_ *, const char *, const FcChar8_ *);
  int (*PatternAddInteger)(FcPattern_ *, const char *, int);
  int (*ConfigSubstitute)(void *, FcPattern_ *, int);
  void (*DefaultSubstitute)(FcPattern_ *);
  FcPattern_ *(*FontMatch)(void *, FcPattern_ *, int *);
  int (*PatternGetString)(const FcPattern_ *, const char *, int, FcChar8_ **);
  int (*PatternGetInteger)(const FcPattern_ *, const char *, int, int *);
  int (*PatternGetBool)(const FcPattern_ *, const char *, int, int *);
  int (*PatternGetMatrix)(const FcPattern_ *, const char *, int, double **);
  void (*PatternDestroy)(FcPattern_ *);
  int (*WeightFromOpenType)(int);
} g_fc;

static int fc_load(void)
{
  if (g_fc.state) {
    return g_fc.state > 0;
  }
  g_fc.state = -1;
  if (getenv("BUI_NO_FONTCONFIG")) {
    return 0;
  }
  void *lib = dlopen("libfontconfig.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    return 0;
  }
#    define FC_SYM(name)                                                   \
      if (!(*(void **)&g_fc.name = dlsym(lib, "Fc" #name))) {              \
        return 0;                                                          \
      }
  FC_SYM(PatternCreate)
  FC_SYM(PatternAddString)
  FC_SYM(PatternAddInteger)
  FC_SYM(ConfigSubstitute)
  FC_SYM(DefaultSubstitute)
  FC_SYM(FontMatch)
  FC_SYM(PatternGetString)
  FC_SYM(PatternGetInteger)
  FC_SYM(PatternGetBool)
  FC_SYM(PatternGetMatrix)
  FC_SYM(PatternDestroy)
  FC_SYM(WeightFromOpenType)
#    undef FC_SYM
  g_fc.state = 1;
  return 1;
}

/* The file and face index fontconfig picks for family / weight / italic,
 * and the rendering options of the font it makes of them. */
static int fc_match(const char *family, int weight, int italic, char *r_path, int size,
                    int *r_index, FontOpts *r_opts)
{
  if (!fc_load()) {
    return 0;
  }
  FcPattern_ *pat = g_fc.PatternCreate();
  if (!pat) {
    return 0;
  }
  g_fc.PatternAddString(pat, "family", (const FcChar8_ *)(family[0] ? family : "sans-serif"));
  g_fc.PatternAddInteger(pat, "weight", g_fc.WeightFromOpenType(weight));
  g_fc.PatternAddInteger(pat, "slant", italic ? 100 : 0);
  g_fc.ConfigSubstitute(NULL, pat, 0 /* FcMatchPattern */);
  g_fc.DefaultSubstitute(pat);
  int result = 0;
  FcPattern_ *m = g_fc.FontMatch(NULL, pat, &result);
  g_fc.PatternDestroy(pat);
  if (!m) {
    return 0;
  }
  FcChar8_ *file = NULL;
  int index = 0, ok = 0;
  if (g_fc.PatternGetString(m, "file", 0, &file) == 0 && file) {
    g_fc.PatternGetInteger(m, "index", 0, &index);
    snprintf(r_path, (size_t)size, "%s", (const char *)file);
    *r_index = index & 0xFFFF; /* the high bits name a variable font's instance */
    ok = 1;
    /* cairo-ft's defaults for what the pattern lacks */
    int aa = 1, hinting = 1, hs = 3, ah = 0, bm = 0, eb = 0;
    g_fc.PatternGetBool(m, "antialias", 0, &aa);
    g_fc.PatternGetBool(m, "hinting", 0, &hinting);
    g_fc.PatternGetInteger(m, "hintstyle", 0, &hs);
    g_fc.PatternGetBool(m, "autohint", 0, &ah);
    g_fc.PatternGetBool(m, "embeddedbitmap", 0, &bm);
    g_fc.PatternGetBool(m, "embolden", 0, &eb);
    r_opts->antialias = (int8_t)(aa != 0);
    r_opts->hintstyle = (int8_t)(!hinting ? 0 : hs < 0 ? 0 : hs > 3 ? 3 : hs);
    r_opts->autohint = (int8_t)(ah != 0);
    r_opts->bitmap = (int8_t)(bm != 0);
    r_opts->embolden = (int8_t)(eb != 0);
    /* the product of its matrices, as Pango makes the font matrix of them
     * (pango_cairo_fc_font_create): FcMatrix is xx, xy, yx, yy */
    double mt[4] = {1, 0, 0, 1}, *v = NULL;
    for (int i = 0; g_fc.PatternGetMatrix(m, "matrix", i, &v) == 0 && v; i++) {
      const double t[4] = {mt[0] * v[0] + mt[1] * v[2], mt[0] * v[1] + mt[1] * v[3],
                           mt[2] * v[0] + mt[3] * v[2], mt[2] * v[1] + mt[3] * v[3]};
      memcpy(mt, t, sizeof(mt));
    }
    r_opts->matrix = (int8_t)(mt[0] != 1 || mt[1] != 0 || mt[2] != 0 || mt[3] != 1);
    for (int i = 0; i < 4; i++) {
      r_opts->mat[i] = (float)mt[i];
    }
  }
  g_fc.PatternDestroy(m);
  return ok;
}
#  endif

#  ifdef _WIN32
/* BUI-Awra: Segoe UI's static weights, as Windows (and GTK there) use them. */
static int face_segoe(int weight, int italic)
{
  static const struct {
    int weight;
    const char *upright, *italic;
  } files[] = {{300, "segoeuil.ttf", "seguili.ttf"},
               {350, "segoeuisl.ttf", "seguisli.ttf"},
               {400, "segoeui.ttf", "segoeuii.ttf"},
               {600, "seguisb.ttf", "seguisbi.ttf"},
               {700, "segoeuib.ttf", "segoeuiz.ttf"},
               {900, "seguibl.ttf", "seguibli.ttf"}};
  int best = 2;
  for (int i = 0; i < 6; i++) {
    if (abs(files[i].weight - weight) < abs(files[best].weight - weight)) {
      best = i;
    }
  }
  const char *win = getenv("WINDIR");
  char path[300];
  snprintf(path, sizeof(path), "%s/Fonts/%s", win ? win : "C:/Windows",
           italic ? files[best].italic : files[best].upright);
  int f = face_load_file(path);
  if (f < 0 && italic) {
    snprintf(path, sizeof(path), "%s/Fonts/%s", win ? win : "C:/Windows", files[best].upright);
    f = face_load_file(path);
  }
  return f;
}
#  endif

static int face_find_style(const char *family, int style, int *r_have);

#  ifdef _WIN32
/* BUI-Awra: any installed family through GDI (what GTK's Pango backend on
 * Windows reaches): the font file's bytes of the face GDI picks for the
 * family, weight and slant; for a collection, the face whose table
 * directory GDI reports. */
static int face_gdi(const char *family, int weight, int italic)
{
  char key[sizeof(g_faces[0].key)];
  snprintf(key, sizeof(key), "gdi:%s:%d:%d", family, weight, italic);
  for (int i = 0; i < g_face_count; i++) {
    if (strcmp(g_faces[i].key, key) == 0) {
      return i;
    }
  }
  if (g_face_count == MAX_FACES) {
    return -1;
  }
  LOGFONTW lf;
  memset(&lf, 0, sizeof(lf));
  lf.lfHeight = -64;
  lf.lfWeight = weight;
  lf.lfItalic = (BYTE)(italic != 0);
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
  MultiByteToWideChar(CP_UTF8, 0, family, -1, lf.lfFaceName, LF_FACESIZE - 1);
  HFONT font = CreateFontIndirectW(&lf);
  HDC dc = font ? CreateCompatibleDC(NULL) : NULL;
  if (!dc) {
    if (font) {
      DeleteObject(font);
    }
    return -1;
  }
  HGDIOBJ old = SelectObject(dc, font);
  const DWORD ttcf = 0x66637474; /* 'ttcf' */
  int index = 0;
  DWORD size = GetFontData(dc, ttcf, 0, NULL, 0);
  const int collection = size != GDI_ERROR && size > 12;
  if (!collection) {
    size = GetFontData(dc, 0, 0, NULL, 0);
  }
  unsigned char *data = (size != GDI_ERROR && size > 12) ? (unsigned char *)malloc(size) : NULL;
  if (data && GetFontData(dc, collection ? ttcf : 0, 0, data, size) != size) {
    free(data);
    data = NULL;
  }
  if (data && collection) {
    /* the selected face's table directory, matched against the collection's */
    unsigned char head[12];
    if (GetFontData(dc, 0, 0, head, sizeof(head)) == sizeof(head)) {
      const int n = stbtt_GetNumberOfFonts(data);
      for (int i = 0; i < n; i++) {
        const int off = stbtt_GetFontOffsetForIndex(data, i);
        if (off >= 0 && (DWORD)off + 12 <= size && memcmp(data + off, head, 12) == 0) {
          index = i;
          break;
        }
      }
    }
  }
  SelectObject(dc, old);
  DeleteDC(dc);
  DeleteObject(font);
  if (!data) {
    return -1;
  }
  Face *f = &g_faces[g_face_count];
  memset(f, 0, sizeof(*f));
  if (!face_init_index(f, data, (long)size, index)) {
    free(data);
    return -1;
  }
  f->mapped = 0;
  strcpy(f->key, key);
  return g_face_count++;
}
#  endif

/* The face for family + style + weight; *r_have gets the style bits it
 * really has (BUI_FONT_BOLD: it draws `weight` without synthesis). */
static int face_find(const char *family, int style, int weight, int *r_have, FontOpts *r_opts)
{
  const int italic = (style & BUI_FONT_ITALIC) != 0;
  int f = -1;
  *r_opts = g_fo_default;
  /* registered faces and explicit files keep upstream's rules */
  for (int i = 0; i < g_reg_count; i++) {
    if (ieq(g_regs[i].family, family)) {
      return face_find_style(family, style, r_have);
    }
  }
#  if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  if (!getenv("BUI_FONT")) {
    char path[512];
    int index = 0;
    if (fc_match(family, weight, italic, path, (int)sizeof(path), &index, r_opts)) {
      f = face_load_file_index(path, index);
    }
    if (f < 0) {
      *r_opts = g_fo_default;
    }
  }
#  elif defined(_WIN32)
  const int generic = !family[0] || ieq(family, "segoe ui") || ieq(family, "sans") ||
                      ieq(family, "sans-serif") || ieq(family, "system-ui");
  if (generic) {
    f = face_segoe(weight, italic);
  }
  if (f < 0) {
    /* the family itself, else the desktop's UI font (Segoe UI, or what
     * GDI substitutes for it) */
    f = face_gdi(generic ? "Segoe UI" : family, weight, italic);
    if (f < 0 && !generic) {
      f = face_gdi("Segoe UI", weight, italic);
    }
  }
#  endif
  if (f >= 0) {
    /* fontconfig synthesizes the slant (a matrix, FontOpts): no BUI rule */
    *r_have = italic ? BUI_FONT_ITALIC : 0;
    return f;
  }
  return face_find_style(family, (style & ~BUI_FONT_BOLD) | (weight >= 600 ? BUI_FONT_BOLD : 0),
                         r_have);
}

/* The face for family + style; *r_have gets the style bits it really has. */
static int face_find_style(const char *family, int style, int *r_have)
{
  /* 1. registered fonts: exact style, then any style of the family */
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < g_reg_count; i++) {
#    ifdef BUI_NO_INTER
      const int default_face = !family[0] && i == 0; /* no embedded UI font */
#    else
      const int default_face = 0;
#    endif
      if ((ieq(g_regs[i].family, family) || default_face) &&
          (pass == 1 || g_regs[i].style == style))
      {
        *r_have = g_regs[i].style;
        return g_regs[i].face;
      }
    }
  }
  /* 2. an explicit file */
  const size_t fl = strlen(family);
  if (fl > 4 && (ieq(family + fl - 4, ".ttf") || ieq(family + fl - 4, ".otf") ||
                 ieq(family + fl - 4, ".ttc")))
  {
    const int f = face_load_file(family);
    if (f >= 0) {
      *r_have = 0;
      return f;
    }
  }
  char dirs[16][260];
  const int nd = font_dirs(dirs, 16);
  /* 3. BUI_FONT overrides the default face */
  const char *env = (!family[0] || family_index(family) == 0) ? getenv("BUI_FONT") : NULL;
  if (env && env[0]) {
    const int f = face_load_file(env);
    if (f >= 0) {
      *r_have = 0;
      return f;
    }
  }
  /* 4. a known generic family, then "<family>.ttf", then the default */
  int fam = family_index(family);
  int generic = family_generic(family);
  if (fam < 0) {
    char files[200];
    char squashed[64];
    int k = 0;
    for (const char *c = family; *c && k < 63; c++) {
      if (*c != ' ') {
        squashed[k++] = *c;
      }
    }
    squashed[k] = 0;
    snprintf(files, sizeof(files), "%s.ttf|%s.ttf|%s.otf|%s-Regular.ttf", family, squashed,
             family, squashed);
    const int f = face_from_files(files, dirs, nd);
    if (f >= 0) {
      *r_have = 0;
      return f;
    }
    fam = 0;
    generic = 1; /* unknown family: the default face */
  }
  const int want = (style & BUI_FONT_BOLD ? 1 : 0) | (style & BUI_FONT_ITALIC ? 2 : 0);
#  ifdef BUI_NO_INTER
  (void)generic;
#  else
  if (fam == 0 && generic) { /* Inter (italic synthesized, like Blender) */
    const int f = face_inter(want & 1);
    if (f >= 0) {
      /* FreeType instances the bold weight; stb_truetype can't (synthesized) */
      *r_have = TEXT_FT && g_faces[f].ft ? (style & BUI_FONT_BOLD) : 0;
      return f;
    }
  }
#  endif
  int f = face_from_files(g_families[fam].files[want], dirs, nd);
  if (f >= 0) {
    *r_have = style;
    return f;
  }
  if (want) { /* synthesize the style from the regular face */
    f = face_from_files(g_families[fam].files[0], dirs, nd);
    if (f >= 0) {
      *r_have = 0;
      return f;
    }
  }
  if (fam != 0) {
    return face_find_style("", style, r_have);
  }
  if (g_reg_count > 0) {
    *r_have = g_regs[0].style;
    return g_regs[0].face;
  }
  return -1;
}

/* BUI-Awra: the desktop's font options, as GtkSettings has them
 * (gtk-xft-hintstyle / gtk-xft-hinting / gtk-xft-antialias):
 * BUI_text_set_font_options. -1: unset. */
#  ifdef _WIN32
/* GDK's Windows backend: hinting on, hintfull, antialiased, always
 * (gdkwin32misc.c); its DirectWrite glyphs (cairo-dwrite: natural
 * symmetric, grid-fitted vertically, grayscale) come closest as FreeType's
 * native hinting (the v40 interpreter hints only vertically, as ClearType
 * natural does) */
static int g_desk_hint = 3, g_desk_aa = 1;
#  else
static int g_desk_hint = -1, g_desk_aa = -1;
#  endif

#  if TEXT_FT
/* The load flags and antialiasing of a slot's glyphs, as cairo-ft merges
 * GTK's font options with the face's fontconfig pattern
 * (_cairo_ft_options_merge). GTK's options: with hinted metrics (its manual
 * font rendering, Awra 1's choice at scale 1) the desktop's settings; else
 * (automatic, on dense displays) grayscale antialiasing and slight hinting. */
static void slot_render_opts(FontSlot *s, const FontOpts *fo)
{
  s->fc_aa = fo->antialias;
  s->fc_hint = fo->hintstyle;
  s->fc_autohint = fo->autohint;
  s->fc_bitmap = fo->bitmap;
  s->hm = (int8_t)g_hint_metrics;
  /* GSK's glyph cache places glyphs on quarter rows too when the font's
   * own hint style (GTK's option, not merged with the pattern) is none:
   * GTK's manual rendering with the desktop's hinting off */
  s->yph = (int8_t)(g_hint_metrics && g_desk_hint == 0);
  const int opt_hint = g_hint_metrics ? g_desk_hint : 1 /* CAIRO_HINT_STYLE_SLIGHT */;
  const int opt_aa = g_hint_metrics ? g_desk_aa : 1;
  const int aa = fo->antialias && opt_aa != 0;
  /* the pattern's hint style counts only with antialiasing (cairo reads it
   * there); -1: the default (full) */
  const int pat_hint = fo->antialias ? fo->hintstyle : -1;
  int hint = opt_hint >= 0 ? opt_hint : pat_hint;
  if (pat_hint == 0) {
    hint = 0;
  }
  int load = FT_LOAD_DEFAULT;
  if (aa) {
    /* embedded bitmaps only when the pattern allows them and hints */
    if (!fo->bitmap || pat_hint == 0) {
      load |= FT_LOAD_NO_BITMAP;
    }
    load |= hint == 0 ? FT_LOAD_NO_HINTING : hint == 1 ? FT_LOAD_TARGET_LIGHT : 0;
  }
  else {
    load |= (hint == 0 ? FT_LOAD_NO_HINTING : FT_LOAD_TARGET_MONO) | FT_LOAD_MONOCHROME;
  }
  if (fo->autohint) {
    load |= FT_LOAD_FORCE_AUTOHINT;
  }
  s->load = load;
  s->mono = (int8_t)!aa;
}
#  endif

void BUI_text_set_font_options(int hint_style, int antialias)
{
  hint_style = hint_style < -1 ? -1 : hint_style > 3 ? 3 : hint_style;
  antialias = antialias < -1 ? -1 : antialias > 1 ? 1 : antialias;
  if (hint_style != g_desk_hint || antialias != g_desk_aa) {
    g_desk_hint = hint_style;
    g_desk_aa = antialias;
    fonts_flush(); /* glyphs and runs come back with the new options */
  }
}

/* A slot over `face` (the face drawing `weight` and the `have` style bits
 * without synthesis); `fo`: its fontconfig pattern's rendering options. */
static int slot_open(FontSlot *s, int face, int style, int weight, int have, const FontOpts *fo)
{
  const Face *f = &g_faces[face];
  const float size = (float)s->px64 / 64.0f; /* BUI-Awra: fractional sizes */
  s->face = face;
  /* GDI: -px = em height */
  s->scale = f->ft_only ? size / (float)f->upem : stbtt_ScaleForMappingEmToPixels(&f->info, size);
  s->ascent = (int)ceilf((float)f->ascent * s->scale - 0.01f);
  s->height = s->ascent + (int)ceilf((float)-f->descent * s->scale - 0.01f);
  s->synth = (style & BUI_FONT_ITALIC) & ~have;
  s->wght = 0.0f;
  s->bmscale = 0.0f;
  float bolder = 0.0f; /* how far the face is from the weight asked for, in 300ths */
#  if TEXT_FT
  if (f->ft && f->mm && f->ax_wght >= 0) {
    s->wght = (float)weight; /* a variable face draws any weight */
  }
  else
#  endif
  if (!(have & BUI_FONT_BOLD) || weight != 700) {
    bolder = (float)(weight - f->os2_weight) / 300.0f;
  }
  /* fontconfig's 90-synthetic.conf, as Pango renders: embolden only when
   * bold (700+) is asked of a face of medium weight or less */
  if (weight < 700 || f->os2_weight > 500) {
    bolder = 0.0f;
  }
  if (f->color) {
    /* cairo emboldens no colour glyph; colour fonts aren't slanted either */
    bolder = 0.0f;
    s->synth = 0;
  }
  s->embolden = 0.0f;
  s->ebold = 0;
  s->hm = (int8_t)g_hint_metrics;
  s->lscale = g_layout_scale;
#  if TEXT_FT
  slot_render_opts(s, fo);
  if (fo->embolden >= 0 && f->ft && !f->color) {
    /* fontconfig said (90-synthetic.conf): cairo-ft emboldens the outlines */
    s->ebold = (int8_t)fo->embolden;
    if (s->ebold) {
      s->synth |= BUI_FONT_BOLD;
    }
  }
  else
#  else
  (void)fo;
#  endif
  if (bolder > 0.3f) { /* synthesize the missing weight, as Pango does */
    s->synth |= BUI_FONT_BOLD;
#  if TEXT_FT
    if (f->ft) {
      s->ebold = 1;
    }
    else
#  endif
    {
      s->embolden = (size < 24.0f ? 1.0f : size / 24.0f) * (bolder > 1.0f ? 1.0f : bolder);
    }
  }
  /* synthetic oblique: fontconfig's matrix (90-synthetic.conf: x + 0.2 y),
   * which Pango makes cairo's font matrix, so FreeType transforms the
   * hinted outlines; without fontconfig, that same slant */
  s->xform = 0;
  s->mat[0] = s->mat[3] = 1.0f;
  s->mat[1] = s->mat[2] = 0.0f;
#  if TEXT_FT
  if (f->ft && !f->color) {
    if (fo->matrix) {
      memcpy(s->mat, fo->mat, sizeof(s->mat));
      s->xform = 1;
      s->synth |= BUI_FONT_ITALIC;
    }
    else if (s->synth & BUI_FONT_ITALIC) {
      s->mat[1] = 0.2f;
      s->xform = 1;
    }
  }
#  endif
  s->ft_size = NULL;
  s->opsz = size;
#  if TEXT_FT
  FT_Size ftsize;
  if (f->ft && FT_New_Size(f->ft, &ftsize) == 0) {
    FT_Size size_ = ftsize;
    FT_Activate_Size(size_);
    int ok;
    if (f->fixed) {
      /* BUI-Awra: a bitmap strike (Noto Color Emoji: 109 ppem), scaled to the
       * text size as cairo scales it: the smallest strike at least that
       * big, else the biggest */
      int best = -1;
      for (int i = 0; i < f->ft->num_fixed_sizes; i++) {
        const FT_Pos ppem = f->ft->available_sizes[i].y_ppem;
        if (best < 0) {
          best = i;
          continue;
        }
        const FT_Pos bp = f->ft->available_sizes[best].y_ppem;
        if ((ppem >= s->px64 && (bp < s->px64 || ppem < bp)) || (bp < s->px64 && ppem > bp)) {
          best = i;
        }
      }
      ok = best >= 0 && FT_Select_Size(f->ft, best) == 0;
      if (ok) {
        s->bmscale = (float)s->px64 / (float)f->ft->available_sizes[best].y_ppem;
      }
    }
    else {
      ok = FT_Set_Char_Size(f->ft, 0, (FT_F26Dot6)s->px64, 72, 72) == 0;
    }
    if (ok) {
      s->ft_size = size_;
    }
    else {
      FT_Done_Size(size_);
    }
  }
  if (f->ft_only && !s->ft_size) {
    return 0;
  }
#  endif
  return 1;
}

static int be_font_open(FontSlot *s, const char *family, int flags, int px)
{
  (void)px;
  const int style = flags & (BUI_FONT_BOLD | BUI_FONT_ITALIC);
  int weight = (flags >> BUI_FONT_WEIGHT_SHIFT) & 0x3FF;
  if (weight <= 0) {
    weight = (style & BUI_FONT_BOLD) ? 700 : 400;
  }
  int have = 0;
  FontOpts fo;
  const int face = face_find(family, style, weight, &have, &fo);
  if (face < 0) {
    return 0;
  }
  return slot_open(s, face, style, weight, have, &fo);
}

/* BUI-Awra: the slot drawing `face` at the size and style of slot
 * `primary` (a fallback font). Never flushes the slots (runs being built
 * hold them): -1 when they are full. */
static int slot_fallback(int primary, int face)
{
  const FontSlot *p = &g_fonts[primary];
  if (face == p->face) {
    return primary;
  }
  for (int i = 0; i < g_font_count; i++) {
    if (g_fonts[i].fb == face + 1 && g_fonts[i].px64 == p->px64 && g_fonts[i].flags == p->flags &&
        g_fonts[i].hm == p->hm && g_fonts[i].lscale == p->lscale)
    {
      return i;
    }
  }
  if (g_font_count == MAX_FONTS) {
    return -1;
  }
  FontSlot *s = &g_fonts[g_font_count];
  memset(s, 0, sizeof(*s));
  memcpy(s->family, p->family, sizeof(s->family));
  s->px = p->px;
  s->px64 = p->px64;
  s->size = p->size;
  s->flags = p->flags;
  s->fb = face + 1;
  const int style = p->flags & (BUI_FONT_BOLD | BUI_FONT_ITALIC);
  int weight = (p->flags >> BUI_FONT_WEIGHT_SHIFT) & 0x3FF;
  if (weight <= 0) {
    weight = (style & BUI_FONT_BOLD) ? 700 : 400;
  }
  /* the style the face has: fontconfig sorted it for this weight and slant */
  int have = g_faces[face].os2_weight >= 600 ? BUI_FONT_BOLD : 0;
#  if TEXT_FT
  if (g_faces[face].ft && (g_faces[face].ft->style_flags & FT_STYLE_FLAG_ITALIC)) {
    have |= BUI_FONT_ITALIC;
  }
#  endif
  /* the primary's options (one fontconfig setup); synthesis by BUI's rules */
  const FontOpts fo = {p->fc_aa, p->fc_hint, p->fc_autohint, p->fc_bitmap, -1, 0, {1, 0, 0, 1}};
  if (!slot_open(s, face, style, weight, have, &fo)) {
    be_font_close(s);
    return -1;
  }
  return g_font_count++;
}

static void be_font_close(FontSlot *s)
{
#  if TEXT_FT
  if (s->ft_size) {
    FT_Done_Size((FT_Size)s->ft_size);
    s->ft_size = NULL;
  }
#  endif
  if (s->hb) {
    hb_font_destroy((hb_font_t *)s->hb);
    s->hb = NULL;
  }
}

#  include "gpu_text_shape.inc.c"

/* Glyph bitmaps by (slot, glyph, quarter-pixel phase). Rasterized once,
 * composited into every run that uses them. */
typedef struct GlyphBmp {
  uint32_t key; /* 0 = empty */
  int16_t w, h, xoff, yoff;
  uint8_t color; /* BUI-Awra: `bits` are premultiplied 0xAARRGGBB pixels (emoji) */
  uint8_t *bits;
} GlyphBmp;

#  define GLYPH_CAP 4096 /* power of two */
#  define GLYPH_MAX_FILL 3000
static GlyphBmp *g_glyphs = NULL;
static int g_glyph_count = 0;

static void be_flush_glyphs(void)
{
  g_text_gen++; /* glyph bitmaps are GPU atlas keys */
  if (!g_glyphs) {
    return;
  }
  for (int i = 0; i < GLYPH_CAP; i++) {
    free(g_glyphs[i].bits);
  }
  memset(g_glyphs, 0, sizeof(GlyphBmp) * GLYPH_CAP);
  g_glyph_count = 0;
}

#  if TEXT_FT
/* pngshim.c: CBDT / sbix PNGs go through this */
extern BuiPngDecodeFn bui_ft_png_decode;

void BUI_text_set_png_decoder(BuiPngDecodeFn decode)
{
  if (bui_ft_png_decode != decode) {
    bui_ft_png_decode = decode;
    be_flush_glyphs(); /* glyphs and runs built without it come back in colour */
    runs_flush();
  }
}
#  else
void BUI_text_set_png_decoder(BuiPngDecodeFn decode)
{
  (void)decode;
}
#  endif

#  if TEXT_FT
/* BUI-Awra: premultiplied BGRA (FreeType) resampled to w x h ARGB pixels:
 * the area each pixel covers averaged when shrinking (a CBDT strike to the
 * text size), bilinear when growing. */
static void argb_resample(const uint8_t *src, int sw, int sh, int pitch, uint32_t *dst, int dw,
                          int dh)
{
  const float kx = (float)sw / (float)dw, ky = (float)sh / (float)dh;
  for (int y = 0; y < dh; y++) {
    for (int x = 0; x < dw; x++) {
      float acc[4] = {0.0f, 0.0f, 0.0f, 0.0f}, wsum = 0.0f;
      if (kx <= 1.0f && ky <= 1.0f) {
        const float u = ((float)x + 0.5f) * kx - 0.5f, v = ((float)y + 0.5f) * ky - 0.5f;
        const int x0 = (int)floorf(u), y0 = (int)floorf(v);
        const float fx = u - (float)x0, fy = v - (float)y0;
        for (int j = 0; j < 2; j++) {
          for (int i = 0; i < 2; i++) {
            const int sx = clampi(x0 + i, 0, sw - 1), sy = clampi(y0 + j, 0, sh - 1);
            const float wgt = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
            const uint8_t *px = src + (size_t)sy * pitch + (size_t)sx * 4;
            for (int c = 0; c < 4; c++) {
              acc[c] += wgt * px[c];
            }
            wsum += wgt;
          }
        }
      }
      else {
        const float x0 = (float)x * kx, x1 = x0 + kx, y0 = (float)y * ky, y1 = y0 + ky;
        for (int sy = (int)y0; sy < sh && (float)sy < y1; sy++) {
          const float wy = fminf(y1, (float)sy + 1.0f) - fmaxf(y0, (float)sy);
          const uint8_t *row = src + (size_t)sy * pitch;
          for (int sx = (int)x0; sx < sw && (float)sx < x1; sx++) {
            const float wgt = wy * (fminf(x1, (float)sx + 1.0f) - fmaxf(x0, (float)sx));
            const uint8_t *px = row + (size_t)sx * 4;
            for (int c = 0; c < 4; c++) {
              acc[c] += wgt * px[c];
            }
            wsum += wgt;
          }
        }
      }
      uint32_t out = 0;
      if (wsum > 0.0f) {
        const float inv = 1.0f / wsum;
        const uint32_t b = (uint32_t)clampi((int)(acc[0] * inv + 0.5f), 0, 255);
        const uint32_t gg = (uint32_t)clampi((int)(acc[1] * inv + 0.5f), 0, 255);
        const uint32_t r = (uint32_t)clampi((int)(acc[2] * inv + 0.5f), 0, 255);
        const uint32_t a = (uint32_t)clampi((int)(acc[3] * inv + 0.5f), 0, 255);
        out = (a << 24) | ((r > a ? a : r) << 16) | ((gg > a ? a : gg) << 8) | (b > a ? a : b);
      }
      dst[(size_t)y * dw + x] = out;
    }
  }
}

/* BUI-Awra: a colour glyph (CBDT / sbix bitmap scaled to the text size, or
 * COLR layers blended by FreeType) into `g`. 0: not a colour glyph. */
static int glyph_color(const FontSlot *s, FT_Face ft, int glyph, GlyphBmp *g)
{
  const Face *f = &g_faces[s->face];
  const FT_Int32 load = FT_LOAD_COLOR | (f->fixed ? 0 : FT_LOAD_TARGET_LIGHT);
  if (FT_Load_Glyph(ft, (FT_UInt)glyph, load) != 0) {
    return 0;
  }
  if (ft->glyph->format == FT_GLYPH_FORMAT_OUTLINE &&
      FT_Render_Glyph(ft->glyph, FT_RENDER_MODE_NORMAL) != 0)
  {
    return 0;
  }
  const FT_Bitmap *b = &ft->glyph->bitmap;
  if (ft->glyph->format != FT_GLYPH_FORMAT_BITMAP || b->pixel_mode != FT_PIXEL_MODE_BGRA ||
      b->pitch < 0)
  {
    return 0; /* a monochrome glyph of a colour font: the outline path draws it */
  }
  const float k = f->fixed && s->bmscale > 0.0f ? s->bmscale : 1.0f;
  const int sw = (int)b->width, sh = (int)b->rows;
  g->color = 1;
  g->bits = NULL;
  g->w = g->h = 0;
  g->xoff = (int16_t)floorf((float)ft->glyph->bitmap_left * k + 0.5f);
  g->yoff = (int16_t)floorf((float)-ft->glyph->bitmap_top * k + 0.5f);
  if (sw <= 0 || sh <= 0) {
    return 1;
  }
  const int dw = k == 1.0f ? sw : clampi((int)floorf((float)sw * k + 0.5f), 1, 4096);
  const int dh = k == 1.0f ? sh : clampi((int)floorf((float)sh * k + 0.5f), 1, 4096);
  uint32_t *px = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)dw * (size_t)dh);
  if (!px) {
    return 1;
  }
  if (dw == sw && dh == sh) {
    for (int y = 0; y < sh; y++) {
      const uint8_t *row = b->buffer + (size_t)y * b->pitch;
      for (int x = 0; x < sw; x++) {
        const uint8_t *q = row + (size_t)x * 4;
        px[(size_t)y * sw + x] = ((uint32_t)q[3] << 24) | ((uint32_t)q[2] << 16) |
                                 ((uint32_t)q[1] << 8) | q[0];
      }
    }
  }
  else {
    argb_resample(b->buffer, sw, sh, b->pitch, px, dw, dh);
  }
  g->bits = (uint8_t *)px;
  g->w = (int16_t)dw;
  g->h = (int16_t)dh;
  return 1;
}
#  endif

#  if TEXT_FT
/* Pango's ink rectangle of a glyph (cairo_scaled_font_glyph_extents in
 * Pango units) as GSK's glyph cache sizes its image from it: the pixels
 * [r[0], r[2]) x [r[1], r[3]) around the pen (y down) at x phase `phase`.
 * The glyph is drawn in that image and nothing outside shows, so hinting
 * that moves an edge past the unhinted box is cut off, as GTK shows it.
 * cairo-ft's metrics: with hinted metrics the glyph's own (hinted, rounded
 * out when unhinted); without, the outline's at the em size (unhinted),
 * scaled to the text size. In 26.6 pixels: bx, by (bearings, y up), bw, bh. */
static void glyph_ink_box(const FontSlot *s, double bx, double by, double bw, double bh,
                          int phase, int r[4])
{
  if (s->xform) {
    /* cairo: the font-space box's corners through the font matrix */
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    for (int k = 0; k < 4; k++) {
      const double cx = bx + ((k & 1) ? bw : 0.0), cy = by - ((k & 2) ? bh : 0.0);
      const double tx = s->mat[0] * cx + s->mat[1] * cy, ty = s->mat[2] * cx + s->mat[3] * cy;
      x0 = tx < x0 ? tx : x0;
      x1 = tx > x1 ? tx : x1;
      y0 = ty < y0 ? ty : y0;
      y1 = ty > y1 ? ty : y1;
    }
    bx = x0;
    bw = x1 - x0;
    by = y1;
    bh = y1 - y0;
  }
  /* pango_units_from_double */
  const double ix = floor(bx * 16.0 + 0.5), iy = floor(-by * 16.0 + 0.5);
  const double iw = floor(bw * 16.0 + 0.5), ih = floor(bh * 16.0 + 0.5);
  const double subx = (phase & 3) / 4.0, suby = (phase >> 2) / 4.0;
  r[0] = (int)floor(ix / 1024.0 + subx);
  r[1] = (int)floor(iy / 1024.0 + suby);
  r[2] = (int)ceil((ix + iw) / 1024.0 + subx);
  r[3] = (int)ceil((iy + ih) / 1024.0 + suby);
}

/* FT_GlyphSlot_Embolden's strength (26.6), as cairo-ft emboldens. */
static FT_Pos ft_embolden_strength(FT_UShort ppem)
{
  return (FT_Pos)ppem * 0x0AAA / 1024;
}

/* The ink box of the unhinted glyph at the em size (cairo-ft without
 * hinted metrics), or 0. */
static int glyph_ink_em(const FontSlot *s, FT_Face ft, int glyph, int phase, int r[4])
{
  if (FT_Load_Glyph(ft, (FT_UInt)glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0 ||
      ft->glyph->format != FT_GLYPH_FORMAT_OUTLINE || ft->units_per_EM == 0)
  {
    return 0;
  }
  FT_BBox b;
  FT_Outline_Get_CBox(&ft->glyph->outline, &b);
  double bx = (double)b.xMin, by = (double)b.yMax;
  double bw = (double)(b.xMax - b.xMin), bh = (double)(b.yMax - b.yMin);
  if (s->ebold) {
    const double e = (double)ft_embolden_strength(ft->units_per_EM) / 64.0;
    bw += e;
    bh += e;
    by += e;
  }
  const double k = (double)s->px64 / (double)ft->units_per_EM; /* units -> 26.6 */
  glyph_ink_box(s, bx * k, by * k, bw * k, bh * k, phase, r);
  return 1;
}

/* A glyph as GTK 4 draws it: GSK's glyph cache has cairo-ft draw it
 * (FT_Load_Glyph with the slot's load flags, FT_GlyphSlot_Embolden for
 * synthetic bold, the outline moved by the quarter-pixel phase, FreeType's
 * rasterizer; embedded bitmaps as they are) into an image of the glyph's
 * ink box. Coverage as FreeType gives it: GSK blends it without gamma. */
static void glyph_raster_ft(const FontSlot *s, FT_Face ft, int glyph, int phase, GlyphBmp *g,
                            int *r_w, int *r_h, int *r_xo, int *r_yo)
{
  int clip[4];
  int clipped = !s->hm && glyph_ink_em(s, ft, glyph, phase, clip);
  if (FT_Load_Glyph(ft, (FT_UInt)glyph, s->load) != 0) {
    return;
  }
  FT_GlyphSlot gs = ft->glyph;
  if (s->xform && gs->format == FT_GLYPH_FORMAT_OUTLINE) {
    /* FT_Set_Transform's: after hinting, before emboldening; the metrics
     * stay the untransformed glyph's */
    const FT_Matrix m = {(FT_Fixed)(s->mat[0] * 65536.0), (FT_Fixed)(s->mat[1] * 65536.0),
                         (FT_Fixed)(s->mat[2] * 65536.0), (FT_Fixed)(s->mat[3] * 65536.0)};
    FT_Outline_Transform(&gs->outline, &m);
  }
  if (s->ebold) {
    FT_GlyphSlot_Embolden(gs);
  }
  if (gs->format == FT_GLYPH_FORMAT_OUTLINE) {
    if (s->hm) {
      FT_Pos bx = gs->metrics.horiBearingX, by = gs->metrics.horiBearingY;
      FT_Pos bw = gs->metrics.width, bh = gs->metrics.height;
      if (s->load & FT_LOAD_NO_HINTING) { /* cairo hints the metrics itself */
        const FT_Pos x1 = bx & -64, x2 = (bx + bw + 63) & -64;
        const FT_Pos y1 = (-by) & -64, y2 = (-by + bh + 63) & -64;
        bx = x1;
        bw = x2 - x1;
        by = -y1;
        bh = y2 - y1;
      }
      glyph_ink_box(s, (double)bx, (double)by, (double)bw, (double)bh, phase, clip);
      clipped = 1;
    }
    /* quarter-pixel position (cairo's x and y phases; y down) */
    FT_Outline_Translate(&gs->outline, (FT_Pos)(phase & 3) * 16, -(FT_Pos)(phase >> 2) * 16);
    /* no antialiasing: FreeType's monochrome rasterizer, as cairo-ft's */
    if (FT_Render_Glyph(gs, s->mono ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL) != 0) {
      return;
    }
  }
  else if (gs->format != FT_GLYPH_FORMAT_BITMAP) {
    return;
  }
  const FT_Bitmap *b = &gs->bitmap;
  if (b->pixel_mode != FT_PIXEL_MODE_GRAY && b->pixel_mode != FT_PIXEL_MODE_MONO) {
    return;
  }
  int x0 = gs->bitmap_left, y0 = -gs->bitmap_top;
  int x1 = x0 + (int)b->width, y1 = y0 + (int)b->rows;
  const int bl = x0, bt = y0;
  if (clipped) {
    x0 = clip[0] > x0 ? clip[0] : x0;
    y0 = clip[1] > y0 ? clip[1] : y0;
    x1 = clip[2] < x1 ? clip[2] : x1;
    y1 = clip[3] < y1 ? clip[3] : y1;
  }
  *r_xo = x0;
  *r_yo = y0;
  const int w = x1 - x0, h = y1 - y0;
  if (w <= 0 || h <= 0) {
    return;
  }
  g->bits = (uint8_t *)malloc((size_t)w * (size_t)h);
  if (!g->bits) {
    return;
  }
  for (int y = 0; y < h; y++) {
    const uint8_t *src = b->buffer + (ptrdiff_t)(y + y0 - bt) * b->pitch;
    uint8_t *dst = g->bits + (size_t)y * w;
    if (b->pixel_mode == FT_PIXEL_MODE_GRAY) {
      memcpy(dst, src + (x0 - bl), (size_t)w);
    }
    else {
      for (int x = 0; x < w; x++) {
        const int bit = x + x0 - bl;
        dst[x] = (src[bit >> 3] & (0x80 >> (bit & 7))) ? 255 : 0;
      }
    }
  }
  *r_w = w;
  *r_h = h;
}
#  endif

static const GlyphBmp *glyph_get(int slot, const FontSlot *s, int glyph, int phase)
{
  if (!g_glyphs) {
    g_glyphs = (GlyphBmp *)calloc(GLYPH_CAP, sizeof(GlyphBmp));
    if (!g_glyphs) {
      return NULL;
    }
  }
  /* phase: x quarter in bits 0-1, y quarter in bits 2-3 */
  const uint32_t key = (((uint32_t)slot + 1) << 24) ^ ((uint32_t)glyph << 4) ^ (uint32_t)phase;
  uint32_t idx = (key * 2654435761u) >> 20 & (GLYPH_CAP - 1);
  for (;;) {
    GlyphBmp *g = &g_glyphs[idx];
    if (g->key == 0) {
      break;
    }
    if (g->key == key) {
      return g;
    }
    idx = (idx + 1) & (GLYPH_CAP - 1);
  }
  if (g_glyph_count >= GLYPH_MAX_FILL) {
    be_flush_glyphs();
    return glyph_get(slot, s, glyph, phase);
  }
  GlyphBmp *g = &g_glyphs[idx];
  const stbtt_fontinfo *info = &g_faces[s->face].info;
  int w = 0, h = 0, xo = 0, yo = 0;
#  if TEXT_FT
  FT_Face ft = ft_use(s);
  if (ft && g_faces[s->face].color && glyph_color(s, ft, glyph, g)) {
    g->key = key;
    g_glyph_count++;
    return g;
  }
  if (ft) {
    g->bits = NULL;
    glyph_raster_ft(s, ft, glyph, phase, g, &w, &h, &xo, &yo);
    g->key = key;
    g->w = (int16_t)(g->bits ? w : 0);
    g->h = (int16_t)(g->bits ? h : 0);
    g->xoff = (int16_t)xo;
    g->yoff = (int16_t)yo;
    g_glyph_count++;
    return g;
  }
#  endif
  g->bits = stbtt_GetGlyphBitmapSubpixel(
      info, s->scale, s->scale, (float)phase * 0.25f, 0.0f, glyph, &w, &h, &xo, &yo);
  if (g->bits) {
    /* stb's coverage is linear; blending it in sRGB thins strokes. Lift it
     * (gamma 1/1.45, close to how GDI's grayscale AA reads). */
    static uint8_t lut[256];
    if (!lut[255]) {
      for (int i = 0; i < 256; i++) {
        lut[i] = (uint8_t)(255.0f * powf((float)i / 255.0f, 1.0f / 1.45f) + 0.5f);
      }
    }
    for (int i = 0; i < w * h; i++) {
      g->bits[i] = lut[g->bits[i]];
    }
  }
  g->key = key;
  g->w = (int16_t)w;
  g->h = (int16_t)h;
  g->xoff = (int16_t)xo;
  g->yoff = (int16_t)yo;
  g_glyph_count++;
  return g;
}

/* Walks the glyphs of a run (visual order): their slot (BUI-Awra: fallback
 * fonts), glyph and position in pixels from the run's origin (x unrounded,
 * y down). */
typedef void (*GlyphFn)(void *user, int slot, int glyph, float x, float y);

void BUI_text_set_hint_metrics(int on)
{
  g_hint_metrics = on != 0;
}

void BUI_text_set_layout_scale(float scale)
{
  g_layout_scale = scale >= 0.25f && scale <= 16.0f ? scale : 1.0f;
}

int BUI_text_hint_metrics(void)
{
  return g_hint_metrics;
}

/* r_ext (may be NULL): the ascent and descent of the run's logical
 * extents, the union of its items' fonts as Pango's line extents take
 * them (a run drawn only with fallback fonts has none of its own font's
 * metrics), and 1 (known). */
static float glyph_walk_part(const TextRun *e, GlyphFn fn, void *user, float r_ext[3])
{
  WRun w;
  const int slot0 = e->font, from = e->pfrom, to = e->pto;
  text_itemize_run(e, &w);
  if (r_ext) {
    /* the union of its items' logical extents (Pango's line extents):
     * each font's ascent and descent, rounded outwards with hinted
     * metrics, where the item sits after baseline alignment */
    r_ext[0] = r_ext[1] = r_ext[2] = 0.0f;
    for (int i = 0; i < w.nit; i++) {
      const WItem *it = &w.it[i];
      const FontSlot *fs = &g_fonts[it->slot];
      const Face *f = &g_faces[fs->face];
      float a, d;
      if (it->slot == slot0) {
        a = (float)f->ascent * fs->scale; /* as BUI_text_metrics */
        d = (float)-f->descent * fs->scale;
      }
      else {
        a = (float)f->hb_ascent * fs->scale;
        d = (float)-f->hb_descent * fs->scale;
      }
      if (g_hint_metrics) {
        a = ceilf(a - 0.001f);
        d = ceilf(d - 0.001f);
      }
      a -= it->dy;
      d += it->dy;
      r_ext[0] = r_ext[2] != 0.0f ? fmaxf(r_ext[0], a) : a;
      r_ext[1] = r_ext[2] != 0.0f ? fmaxf(r_ext[1], d) : d;
      r_ext[2] = 1.0f;
    }
  }
  if (fn) {
    for (int i = 0; i < w.n; i++) {
      const WGlyph *g = &w.g[i];
      if (!(g->flags & WG_HIDE) && (to <= from || (g->cluster >= from && g->cluster < to))) {
        fn(user, g->slot, g->glyph, g->x + g->dx, g->dy);
      }
    }
  }
  const float width = w.width;
  wrun_free(&w);
  return width;
}

/* The run's width: whole pixels with hinted metrics (whole advances), else
 * Pango's unrounded logical width. */
static float be_measure(const TextRun *e, float r_ext[3])
{
  return glyph_walk_part(e, NULL, NULL, r_ext);
}

static void list_glyph(void *user, int slot, int glyph, float x, float y)
{
  RunGlyphs *l = (RunGlyphs *)user;
  if (l->ng == l->cap) {
    const int cap = l->cap ? l->cap * 2 : 16;
    uint16_t *g = (uint16_t *)realloc(l->gid, sizeof(uint16_t) * (size_t)cap);
    if (!g) {
      return;
    }
    l->gid = g;
    float *x_ = (float *)realloc(l->gx, sizeof(float) * (size_t)cap);
    if (!x_) {
      return;
    }
    l->gx = x_;
    uint8_t *sl = (uint8_t *)realloc(l->gslot, (size_t)cap);
    if (!sl) {
      return;
    }
    l->gslot = sl;
    if (l->gy) {
      float *y_ = (float *)realloc(l->gy, sizeof(float) * (size_t)cap);
      if (!y_) {
        return;
      }
      l->gy = y_;
    }
    l->cap = cap;
  }
  if (y != 0.0f && !l->gy) { /* the first offset glyph: offsets for all */
    l->gy = (float *)calloc((size_t)l->cap, sizeof(float));
    if (!l->gy) {
      return;
    }
  }
  l->gid[l->ng] = (uint16_t)glyph;
  l->gx[l->ng] = x;
  if (l->gy) {
    l->gy[l->ng] = y;
  }
  l->gslot[l->ng] = (uint8_t)slot;
  l->ng++;
}

/* The run's glyph ids and positions (built once). */
static int run_glyphs(TextRun *e)
{
  if (!e->gl) {
    e->gl = (RunGlyphs *)calloc(1, sizeof(RunGlyphs));
    if (!e->gl) {
      return 0;
    }
    glyph_walk_part(e, list_glyph, e->gl, NULL);
  }
  return 1;
}

/* BUI-Awra: the run's clusters and cursor positions (built once). */
static int run_clusters(TextRun *e)
{
  if (e->car) {
    return 1;
  }
  WRun w;
  text_itemize_run(e, &w);
  struct RunCluster *cl = (struct RunCluster *)malloc(sizeof(*cl) * (size_t)(w.n > 0 ? w.n : 1));
  uint8_t *cur = (uint8_t *)malloc((size_t)e->len + 1);
  RunCarets *car = (RunCarets *)malloc(sizeof(RunCarets));
  if (!cl || !cur || !car) {
    free(cl);
    free(cur);
    free(car);
    wrun_free(&w);
    return 0;
  }
  int n = 0;
  for (int i = 0; i < w.n;) {
    const WGlyph *g0 = &w.g[i];
    int j = i;
    float x0 = g0->x, x1 = g0->x;
    int spacing = 0, lig = -1;
    while (j < w.n && w.g[j].cluster == g0->cluster && w.g[j].item == g0->item) {
      const WGlyph *g = &w.g[j];
      x1 = g->x + g->adv;
      FontSlot *fs = &g_fonts[g->slot];
      hb_face_t *hf = face_hb(&g_faces[fs->face]);
      if (hf && !(g->flags & WG_HIDE) &&
          hb_ot_layout_get_glyph_class(hf, g->glyph) != HB_OT_LAYOUT_GLYPH_CLASS_MARK)
      {
        spacing++;
        lig = j;
      }
      j++;
    }
    struct RunCluster *c = &cl[n++];
    c->start = g0->cluster;
    c->end = e->len;
    c->rtl = (g0->flags & WG_RTL) != 0;
    c->lead = c->rtl ? x1 : x0;
    c->trail = c->rtl ? x0 : x1;
    c->lig = spacing == 1 ? w.g[lig].glyph : 0;
    c->slot = spacing == 1 ? w.g[lig].slot : g0->slot;
    c->lig_dx = spacing == 1 ? w.g[lig].dx : 0.0f;
    i = j;
  }
  wrun_free(&w);
  /* logical order; a cluster ends where the next begins */
  for (int i = 1; i < n; i++) {
    const struct RunCluster t = cl[i];
    int j = i;
    while (j > 0 && cl[j - 1].start > t.start) {
      cl[j] = cl[j - 1];
      j--;
    }
    cl[j] = t;
  }
  for (int i = 0; i + 1 < n; i++) {
    cl[i].end = cl[i + 1].start;
  }
  text_breaks(e->text, e->len, NULL, cur, -1);
  car->cl = cl;
  car->n = n;
  car->cur = cur;
  e->car = car;
  return 1;
}

/* The cluster holding byte `index` (the last one for index >= len). */
static int run_cluster_at(const TextRun *e, int index)
{
  int lo = 0, hi = e->car->n - 1;
  while (lo < hi) {
    const int mid = (lo + hi + 1) / 2;
    if (e->car->cl[mid].start <= index) {
      lo = mid;
    }
    else {
      hi = mid - 1;
    }
  }
  return lo;
}

/* pango_layout_line_index_to_x over one run: the index moved to its
 * grapheme's start (leading edge) or last character (trailing edge), then
 * pango_glyph_string_index_to_x_full over the run's clusters. */
static float run_index_to_x(TextRun *e, int index, int trailing)
{
  if (!run_clusters(e) || e->car->n == 0) {
    return 0.0f;
  }
  if (index < e->len) {
    if (trailing) {
      while (index + utf8_step(e->text, index) < e->len &&
             !(e->car->cur[index + utf8_step(e->text, index)] & BUI_TEXT_CURSOR))
      {
        index += utf8_step(e->text, index);
      }
    }
    else {
      while (index > 0 && !(e->car->cur[index] & BUI_TEXT_CURSOR)) {
        index--;
        while (index > 0 && ((unsigned char)e->text[index] & 0xC0) == 0x80) {
          index--;
        }
      }
    }
  }
  const struct RunCluster *c = &e->car->cl[run_cluster_at(e, index)];
  int chars = 0, offset = 0;
  for (int i = c->start; i < c->end; i += utf8_step(e->text, i)) {
    if (!(e->car->cur[i] & BUI_TEXT_CURSOR)) {
      continue;
    }
    if (i < index) {
      offset++;
    }
    chars++;
  }
  if (trailing) {
    offset = offset + 1 < chars ? offset + 1 : chars;
  }
  if (chars == 0) {
    return c->lead;
  }
  if (offset > 0 && offset < chars && c->lig) {
    /* the font's ligature caret */
    FontSlot *fs = &g_fonts[c->slot];
    hb_font_t *font = slot_hb(fs);
    hb_position_t caret = 0;
    unsigned int count = 1;
    if (font &&
        hb_ot_layout_get_ligature_carets(font, c->rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR, c->lig,
                                         (unsigned int)(offset - 1), &count, &caret) > 0 &&
        count > 0)
    {
      return fminf(c->lead, c->trail) + (float)caret * (fs->lscale / 1024.0f) + c->lig_dx;
    }
  }
  /* in Pango units (at the logical size), divided as Pango divides them */
  const float ls = g_fonts[c->slot].lscale;
  const long lead = lrintf(c->lead / ls * 1024.0f), trail = lrintf(c->trail / ls * 1024.0f);
  return (float)(((long)(chars - offset) * lead + (long)offset * trail) / chars) * (ls / 1024.0f);
}

/* pango_glyph_string_x_to_index, then the grapheme it falls in
 * (pango_layout_line_x_to_index). */
static int run_x_to_index(TextRun *e, float x, int *r_trailing)
{
  *r_trailing = 0;
  if (!run_clusters(e) || e->car->n == 0) {
    return 0;
  }
  /* the cluster under x; outside the run, the visually first or last */
  int best = -1, left = -1, right = -1;
  for (int i = 0; i < e->car->n; i++) {
    const struct RunCluster *c = &e->car->cl[i];
    const float x0 = fminf(c->lead, c->trail), x1 = fmaxf(c->lead, c->trail);
    if (x >= x0 && x < x1) {
      best = i;
      break;
    }
    if (left < 0 || x0 < fminf(e->car->cl[left].lead, e->car->cl[left].trail)) {
      left = i;
    }
    if (right < 0 || x1 > fmaxf(e->car->cl[right].lead, e->car->cl[right].trail) ||
        (x1 == fmaxf(e->car->cl[right].lead, e->car->cl[right].trail) && x1 > x0))
    {
      right = i;
    }
  }
  int index, trailing;
  if (best < 0) {
    /* outside the run: the visual end of its outermost cluster, as
     * pango_layout_line_x_to_index picks the line's leftmost or rightmost
     * character (trailing: the characters of its grapheme) */
    const int at_right = x >= fmaxf(e->car->cl[right].lead, e->car->cl[right].trail);
    const struct RunCluster *c = &e->car->cl[at_right ? right : left];
    if (c->rtl == at_right) { /* its leading edge */
      *r_trailing = 0;
      return c->start;
    }
    index = c->start;
    for (int i = c->start; i < c->end; i += utf8_step(e->text, i)) {
      if (e->car->cur[i] & BUI_TEXT_CURSOR) {
        index = i; /* its last grapheme */
      }
    }
    while (index > 0 && !(e->car->cur[index] & BUI_TEXT_CURSOR)) {
      index--; /* a cluster inside a grapheme (a vowel sign): its grapheme */
      while (index > 0 && ((unsigned char)e->text[index] & 0xC0) == 0x80) {
        index--;
      }
    }
    int chars = 0;
    for (int i = index; i < e->len; i += utf8_step(e->text, i)) {
      if (i > index && (e->car->cur[i] & BUI_TEXT_CURSOR)) {
        break;
      }
      chars++;
    }
    *r_trailing = chars;
    return index;
  }
  {
    const struct RunCluster *c = &e->car->cl[best];
    int chars = 0;
    for (int i = c->start; i < c->end; i += utf8_step(e->text, i)) {
      chars++;
    }
    const double cpos = (double)(x - c->lead) * chars / (double)(c->trail - c->lead);
    int k = 0;
    if (!c->rtl) { /* the boundary belongs to the next character... */
      while (k + 1 <= cpos && k + 1 < chars) {
        k++;
      }
      trailing = cpos - (int)cpos >= 0.5;
    }
    else { /* ...or, right to left, to the previous one */
      while (k + 1 < cpos && k + 1 < chars) {
        k++;
      }
      const double flip = chars - cpos;
      trailing = !(flip - (int)flip >= 0.5);
    }
    index = c->start;
    for (int i = 0; i < k; i++) {
      index += utf8_step(e->text, index);
    }
  }
  /* the grapheme it falls in: its first character, trailing when x is
   * past the grapheme's middle (in characters, as Pango counts) */
  int start = index, back = 0, fwd = 0;
  while (start > 0 && !(e->car->cur[start] & BUI_TEXT_CURSOR)) {
    start--;
    while (start > 0 && ((unsigned char)e->text[start] & 0xC0) == 0x80) {
      start--;
    }
    back++;
  }
  int i = index;
  do {
    i += utf8_step(e->text, i);
    fwd++;
  } while (i < e->len && !(e->car->cur[i] & BUI_TEXT_CURSOR));
  *r_trailing = back + trailing > (back + fwd) / 2 ? back + fwd : 0;
  return start;
}

/* r_embolden: synthetic bold in pixels (extra passes of glyphs that don't
 * have it); r_baked: the glyph bitmaps and Slug outlines have it already
 * (cairo-ft's emboldening) */
static int glyph_draw_info(const TextRun *e, int i, int *r_face, float *r_scale, float *r_embolden,
                           int *r_baked, float *r_shear)
{
  const FontSlot *s = &g_fonts[e->gl->gslot ? e->gl->gslot[i] : e->font];
  *r_face = s->face;
  *r_scale = s->scale;
  *r_embolden = s->ebold ? (float)s->px64 / (64.0f * 24.0f) : s->embolden;
  *r_baked = s->ebold;
  *r_shear = s->xform ? s->mat[1] : (s->synth & BUI_FONT_ITALIC) ? 0.2f : 0.0f;
  return 1;
}

/* BUI-Awra: colour glyphs sit on whole pixels (no subpixel phases). */
static int glyph_phase(int slot, float pen, int *r_whole)
{
  if (g_faces[g_fonts[slot].face].color) {
    *r_whole = (int)floorf(pen + 0.5f);
    return 0;
  }
  const float px = floorf(pen * 4.0f + 0.5f) * 0.25f;
  *r_whole = (int)floorf(px);
  return (int)((px - (float)*r_whole) * 4.0f) & 3;
}

/* BUI-Awra: the row of a glyph at y (down): whole rows, as cairo places
 * hinted glyphs; quarter rows (phase bits 2-3) when GSK aligns them so
 * (GTK's hint style none). */
static int glyph_yphase(int slot, float y, int *r_whole)
{
  if (!g_fonts[slot].yph || g_faces[g_fonts[slot].face].color) {
    *r_whole = (int)floorf(y + 0.5f);
    return 0;
  }
  const float py = floorf(y * 4.0f + 0.5f) * 0.25f;
  *r_whole = (int)floorf(py);
  return ((int)((py - (float)*r_whole) * 4.0f) & 3) << 2;
}

/* BUI-Awra: whether Slug draws the slot's glyphs as the glyph cache does,
 * and the outline variant it asks for (bui_text_glyph_outline). GSK draws
 * glyph images: Slug evaluates unhinted outlines, so glyphs that FreeType
 * hints, renders monochrome, takes from embedded bitmaps or transforms
 * other than by a slant come from the glyph cache instead. */
static int glyph_slug_var(int slot, unsigned *r_var)
{
  const FontSlot *s = &g_fonts[slot];
  const Face *f = &g_faces[s->face];
  *r_var = 0;
  (void)f;
#  if TEXT_FT
  if (!f->ft) {
    return 1; /* stb_truetype draws unhinted outlines too */
  }
  if (s->mono || !(s->load & FT_LOAD_NO_HINTING) ||
      (!(s->load & FT_LOAD_NO_BITMAP) && FT_HAS_FIXED_SIZES(f->ft)) ||
      (s->xform && (s->mat[0] != 1.0f || s->mat[2] != 0.0f || s->mat[3] != 1.0f)))
  {
    return 0;
  }
  unsigned var = 0;
  if (f->mm && f->ax_wght >= 0) {
    const float w = s->wght > 0.0f ? s->wght : f->wght;
    var |= (unsigned)(w + 0.5f) & 0x3FFu;
  }
  if (f->mm && f->ax_opsz >= 0) {
    const float o = s->opsz * 2.0f + 0.5f;
    var |= (o < 1.0f ? 1u : o > 255.0f ? 255u : (unsigned)o) << 10;
  }
  if (s->ebold) {
    var |= 1u << 18;
  }
  *r_var = var;
#  endif
  return 1;
}

static int glyph_is_color(int slot, int glyph)
{
  if (!g_faces[g_fonts[slot].face].color) {
    return 0;
  }
  const GlyphBmp *g = glyph_get(slot, &g_fonts[slot], glyph, 0);
  return g && g->color;
}

/* Glyph `i` of a run as a cached bitmap at its quarter-pixel phase: its
 * offset from the run origin (baseline), size, and a stable key. */
static const void *glyph_bitmap(const TextRun *e, int i, float pen, float yfrac, int *r_x, int *r_y,
                                int *r_w, int *r_h, const uint8_t **r_bits)
{
  const int slot = e->gl->gslot ? e->gl->gslot[i] : e->font;
  int whole, row;
  const int phase = glyph_phase(slot, pen, &whole) |
                    glyph_yphase(slot, yfrac + (e->gl->gy ? e->gl->gy[i] : 0.0f), &row);
  const GlyphBmp *g = glyph_get(slot, &g_fonts[slot], e->gl->gid[i], phase);
  if (!g || !g->bits || g->color) {
    return NULL;
  }
  *r_x = whole + g->xoff;
  *r_y = g->yoff + row;
  *r_w = g->w;
  *r_h = g->h;
  *r_bits = g->bits;
  return g;
}

/* BUI-Awra: the run's colour glyphs, from the glyph cache, at x, y
 * (baseline), times `alpha`. */
static void run_draw_color(BuiRaster *r, const TextRun *e, float x, float y, float alpha)
{
  const float ox = floorf(x + 0.5f), base = floorf(y + 0.5f);
  for (int k = 0; k < e->ncol; k++) {
    const struct RunColor *c = &e->col[k];
    const GlyphBmp *g = glyph_get(c->slot, &g_fonts[c->slot], c->glyph, 0);
    if (!g || !g->bits || !g->color) {
      continue;
    }
    /* a key for GPU atlases: the glyph's cache entry in this generation */
    const BuiPixmap pm = {(const uint32_t *)g->bits, g->w, g->h,
                          (1ull << 62) | ((uint64_t)(g_text_gen & 0x3FFFFFFFu) << 32) | g->key,
                          BUI_PIXMAP_ARGB, 0};
    BUI_raster_image(r, ox + (float)c->x, base + (float)c->y, (float)g->w, (float)g->h, &pm, NULL,
                     alpha, 0);
  }
}

#  if TEXT_FT
/* FT_Outline_Decompose into the same quadratic list as below. */
typedef struct OutlineSink {
  float *c;
  int n, cap;
  float cx, cy;
} OutlineSink;

static int osink_push(OutlineSink *o, float x1, float y1, float x2, float y2, float x3, float y3)
{
  if (o->n == o->cap) {
    o->cap = o->cap ? o->cap * 2 : 64;
    float *c = (float *)realloc(o->c, sizeof(float) * 6 * (size_t)o->cap);
    if (!c) {
      return 1;
    }
    o->c = c;
  }
  float *q = o->c + 6 * o->n++;
  q[0] = x1; q[1] = y1; q[2] = x2; q[3] = y2; q[4] = x3; q[5] = y3;
  return 0;
}

static int osink_move(const FT_Vector *to, void *u)
{
  OutlineSink *o = (OutlineSink *)u;
  o->cx = (float)to->x;
  o->cy = (float)to->y;
  return 0;
}

static int osink_line(const FT_Vector *to, void *u)
{
  OutlineSink *o = (OutlineSink *)u;
  const float x = (float)to->x, y = (float)to->y;
  const int r = osink_push(o, o->cx, o->cy, x, y, x, y);
  o->cx = x;
  o->cy = y;
  return r;
}

static int osink_conic(const FT_Vector *ctl, const FT_Vector *to, void *u)
{
  OutlineSink *o = (OutlineSink *)u;
  const float x = (float)to->x, y = (float)to->y;
  const int r = osink_push(o, o->cx, o->cy, (float)ctl->x, (float)ctl->y, x, y);
  o->cx = x;
  o->cy = y;
  return r;
}

static int osink_cubic(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *u)
{
  /* split at t = 1/2; each half approximated by one quadratic */
  OutlineSink *o = (OutlineSink *)u;
  const float p0x = o->cx, p0y = o->cy, p1x = (float)c1->x, p1y = (float)c1->y;
  const float p2x = (float)c2->x, p2y = (float)c2->y, x = (float)to->x, y = (float)to->y;
  const float m01x = (p0x + p1x) / 2, m01y = (p0y + p1y) / 2, m12x = (p1x + p2x) / 2,
              m12y = (p1y + p2y) / 2, m23x = (p2x + x) / 2, m23y = (p2y + y) / 2;
  const float ax = (m01x + m12x) / 2, ay = (m01y + m12y) / 2, bx = (m12x + m23x) / 2,
              by = (m12y + m23y) / 2, mx = (ax + bx) / 2, my = (ay + by) / 2;
  int r = osink_push(o, p0x, p0y, (3 * (m01x + ax) - p0x - mx) / 4, (3 * (m01y + ay) - p0y - my) / 4,
                     mx, my);
  r |= osink_push(o, mx, my, (3 * (bx + m23x) - mx - x) / 4, (3 * (by + m23y) - my - y) / 4, x, y);
  o->cx = x;
  o->cy = y;
  return r;
}

/* The outline of the instance the face draws (its weight; UI optical size). */
static int ft_glyph_outline(int face, int glyph, unsigned var, float **r_curves, int *r_n,
                            float r_bbox[4])
{
  Face *f = &g_faces[face];
  FontSlot tmp;
  memset(&tmp, 0, sizeof(tmp));
  tmp.face = face;
  /* the slot's instance of a variable face (glyph_slug_var) */
  tmp.wght = (float)(var & 0x3FFu);
  tmp.opsz = (var >> 10 & 0xFFu) ? (float)(var >> 10 & 0xFFu) * 0.5f : 14.0f;
  const int bold = (var >> 18) & 1;
  FT_Size size = NULL;
  if (FT_New_Size(f->ft, &size) != 0) {
    return 0;
  }
  tmp.ft_size = size;
  ft_use(&tmp);
  int ok = 0;
  if (FT_Load_Glyph(f->ft, (FT_UInt)glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) == 0 &&
      f->ft->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
  {
    if (bold) {
      /* FT_GlyphSlot_Embolden's strength (ppem * 0x0AAA / 1024 in 26.6) in font units */
      const FT_Pos e = (FT_Pos)f->ft->units_per_EM * 0x0AAA / 65536;
      FT_Outline_EmboldenXY(&f->ft->glyph->outline, e, e);
    }
    static const FT_Outline_Funcs funcs = {osink_move, osink_line, osink_conic, osink_cubic, 0, 0};
    OutlineSink o = {NULL, 0, 0, 0.0f, 0.0f};
    if (FT_Outline_Decompose(&f->ft->glyph->outline, &funcs, &o) == 0 && o.n > 0) {
      FT_BBox b;
      FT_Outline_Get_CBox(&f->ft->glyph->outline, &b);
      r_bbox[0] = (float)b.xMin;
      r_bbox[1] = (float)b.yMin;
      r_bbox[2] = (float)b.xMax;
      r_bbox[3] = (float)b.yMax;
      *r_curves = o.c;
      *r_n = o.n;
      ok = 1;
    }
    else {
      free(o.c);
    }
  }
  FT_Done_Size(size);
  f->set_opsz = -1.0f; /* the next slot sets its own */
  return ok;
}
#  endif

/* A glyph's outline as quadratic curves (x1 y1 x2 y2 x3 y3, font units,
 * y up) and its box; lines are {p1, p2, p2}, cubics are split into
 * quadratics. Returns 0 for empty glyphs. `*r_curves` is malloc'd. */
int bui_text_glyph_outline(int face, int glyph, unsigned var, float **r_curves, int *r_n,
                           float r_bbox[4])
{
  *r_curves = NULL;
  *r_n = 0;
  if (face < 0 || face >= g_face_count) {
    return 0;
  }
#  if TEXT_FT
  if (g_faces[face].ft) {
    return ft_glyph_outline(face, glyph, var, r_curves, r_n, r_bbox);
  }
#  endif
  (void)var; /* stb_truetype: the face's own outlines (glyph_slug_var gives 0) */
  const stbtt_fontinfo *info = &g_faces[face].info;
  stbtt_vertex *v = NULL;
  const int nv = stbtt_GetGlyphShape(info, glyph, &v);
  if (nv <= 0) {
    return 0;
  }
  float *c = (float *)malloc(sizeof(float) * 6 * (size_t)nv * 2);
  if (!c) {
    stbtt_FreeShape(info, v);
    return 0;
  }
  int n = 0;
  float cx = 0, cy = 0, sx = 0, sy = 0;
  for (int k = 0; k < nv; k++) {
    const float x = v[k].x, y = v[k].y;
    float *q;
    switch (v[k].type) {
      case STBTT_vmove:
        if ((cx != sx || cy != sy) && n > 0) { /* close the previous contour */
          q = c + 6 * n++;
          q[0] = cx; q[1] = cy; q[2] = sx; q[3] = sy; q[4] = sx; q[5] = sy;
        }
        cx = sx = x;
        cy = sy = y;
        break;
      case STBTT_vline:
        q = c + 6 * n++;
        q[0] = cx; q[1] = cy; q[2] = x; q[3] = y; q[4] = x; q[5] = y;
        cx = x;
        cy = y;
        break;
      case STBTT_vcurve:
        q = c + 6 * n++;
        q[0] = cx; q[1] = cy; q[2] = v[k].cx; q[3] = v[k].cy; q[4] = x; q[5] = y;
        cx = x;
        cy = y;
        break;
      case STBTT_vcubic: {
        /* split at t = 1/2; each half approximated by one quadratic */
        const float p0x = cx, p0y = cy, p1x = v[k].cx, p1y = v[k].cy, p2x = v[k].cx1, p2y = v[k].cy1;
        const float m01x = (p0x + p1x) / 2, m01y = (p0y + p1y) / 2, m12x = (p1x + p2x) / 2,
                    m12y = (p1y + p2y) / 2, m23x = (p2x + x) / 2, m23y = (p2y + y) / 2;
        const float ax = (m01x + m12x) / 2, ay = (m01y + m12y) / 2, bx = (m12x + m23x) / 2,
                    by = (m12y + m23y) / 2, mx = (ax + bx) / 2, my = (ay + by) / 2;
        q = c + 6 * n++;
        q[0] = p0x; q[1] = p0y;
        q[2] = (3 * (m01x + ax) - p0x - mx) / 4; q[3] = (3 * (m01y + ay) - p0y - my) / 4;
        q[4] = mx; q[5] = my;
        q = c + 6 * n++;
        q[0] = mx; q[1] = my;
        q[2] = (3 * (bx + m23x) - mx - x) / 4; q[3] = (3 * (by + m23y) - my - y) / 4;
        q[4] = x; q[5] = y;
        cx = x;
        cy = y;
        break;
      }
      default:
        break;
    }
  }
  if ((cx != sx || cy != sy) && n > 0) {
    float *q = c + 6 * n++;
    q[0] = cx; q[1] = cy; q[2] = sx; q[3] = sy; q[4] = sx; q[5] = sy;
  }
  stbtt_FreeShape(info, v);
  int x0, y0, x1, y1;
  stbtt_GetGlyphBox(info, glyph, &x0, &y0, &x1, &y1);
  r_bbox[0] = (float)x0;
  r_bbox[1] = (float)y0;
  r_bbox[2] = (float)x1;
  r_bbox[3] = (float)y1;
  if (n == 0) {
    free(c);
    return 0;
  }
  *r_curves = c;
  *r_n = n;
  return 1;
}

/* BUI-Awra: a run's mask is built in two passes over its glyphs: their
 * bounds (marks above and below the font's box, fallback fonts' glyphs),
 * then their coverage. */
static int text_imax(int a, int b)
{
  return a > b ? a : b;
}

typedef struct BuildGlyph {
  uint16_t glyph;
  uint8_t slot, phase;
  int x, y; /* whole pixels from the run's origin (baseline) */
} BuildGlyph;

typedef struct BuildCtx {
  BuildGlyph *g;
  int n, cap;
  float sx, sy; /* the run origin's offset from whole pixels */
  int ydep;     /* a glyph sits off the baseline */
} BuildCtx;

static void build_glyph(void *user, int slot, int glyph, float x, float y)
{
  BuildCtx *c = (BuildCtx *)user;
  if (c->n == c->cap) {
    const int cap = c->cap ? c->cap * 2 : 32;
    BuildGlyph *g = (BuildGlyph *)realloc(c->g, sizeof(BuildGlyph) * (size_t)cap);
    if (!g) {
      return;
    }
    c->g = g;
    c->cap = cap;
  }
  BuildGlyph *b = &c->g[c->n++];
  int whole;
  /* GSK: quarter pixels of the absolute x, whole rows of the absolute y */
  int row;
  b->phase = (uint8_t)(glyph_phase(slot, x + c->sx, &whole) | glyph_yphase(slot, y + c->sy, &row));
  b->glyph = (uint16_t)glyph;
  b->slot = (uint8_t)slot;
  b->x = whole;
  b->y = row;
  c->ydep |= y != 0.0f || g_fonts[slot].yph;
}

static void blit_glyph(uint8_t *mask, int mw, int mh, int gx, int gy, int base, const GlyphBmp *g,
                       float shear)
{
  for (int y = 0; y < g->h; y++) {
    const int my = gy + y;
    if (my < 0 || my >= mh) {
      continue;
    }
    uint8_t *row = mask + (size_t)my * mw;
    const uint8_t *src = g->bits + (size_t)y * g->w;
    if (shear == 0.0f) {
      for (int x = 0; x < g->w; x++) {
        const int mx = gx + x;
        if (mx >= 0 && mx < mw && src[x]) {
          /* overlapping glyphs: one over the other, as GSK draws them */
          const int a = row[mx], b = src[x];
          row[mx] = (uint8_t)(a + b - (a * b + 127) / 255);
        }
      }
    }
    else {
      /* shear about the baseline, splitting coverage between two pixels */
      const float sh = (float)(base - my) * shear;
      const int si = (int)floorf(sh);
      const int fr = (int)((sh - (float)si) * 256.0f);
      for (int x = 0; x < g->w; x++) {
        if (!src[x]) {
          continue;
        }
        const int mx = gx + x + si;
        const int hi = (src[x] * fr) >> 8, lo = src[x] - hi;
        if (mx >= 0 && mx < mw) {
          const int v = row[mx] + lo;
          row[mx] = (uint8_t)(v > 255 ? 255 : v);
        }
        if (mx + 1 >= 0 && mx + 1 < mw) {
          const int v = row[mx + 1] + hi;
          row[mx + 1] = (uint8_t)(v > 255 ? 255 : v);
        }
      }
    }
  }
}

static void be_build(TextRun *e, const FontSlot *fs)
{
  BuildCtx c = {NULL, 0, 0, (float)e->sx / 32768.0f, (float)e->sy / 32768.0f, 0};
  glyph_walk_part(e, build_glyph, &c, NULL);
  e->ydep = (uint8_t)(c.ydep != 0);
  /* bounds: the font's box (as before), grown to every glyph's bitmap */
  const int pad = 2 + fs->px / 4; /* bearings / italic overhang */
  int left = pad, right = pad, asc = fs->ascent, desc = fs->height + 1 - fs->ascent;
  int ncol = 0;
  for (int i = 0; i < c.n; i++) {
    const BuildGlyph *b = &c.g[i];
    const GlyphBmp *g = glyph_get(b->slot, &g_fonts[b->slot], b->glyph, b->phase);
    if (!g || !g->bits) {
      continue;
    }
    if (g->color) {
      ncol++;
      continue;
    }
    /* synthetic italic sheared here (glyphs without FreeType; FreeType's
     * outlines are slanted already) */
    const int blit_shear = (g_fonts[b->slot].synth & BUI_FONT_ITALIC) && !g_fonts[b->slot].xform;
    const int lean = blit_shear ? (asc + 1) / 5 + 1 : 0;
    const int x0 = b->x + g->xoff, x1 = x0 + g->w + lean;
    const int y0 = b->y + g->yoff, y1 = y0 + g->h;
    left = text_imax(left, -x0 + 1);
    right = text_imax(right, x1 - (int)e->advance + 1);
    asc = text_imax(asc, -y0);
    desc = text_imax(desc, y1);
  }
  const int mw = (int)e->advance + left + right;
  const int mh = asc + desc;
  uint8_t *mask = c.n > ncol ? (uint8_t *)calloc((size_t)mw * mh, 1) : NULL;
  struct RunColor *col = ncol ? (struct RunColor *)malloc(sizeof(*col) * (size_t)ncol) : NULL;
  int nc = 0;
  for (int i = 0; i < c.n; i++) {
    const BuildGlyph *b = &c.g[i];
    const GlyphBmp *g = glyph_get(b->slot, &g_fonts[b->slot], b->glyph, b->phase);
    if (!g || !g->bits) {
      continue;
    }
    if (g->color) {
      if (col && nc < ncol) {
        struct RunColor *rc = &col[nc++];
        rc->glyph = b->glyph;
        rc->slot = b->slot;
        rc->x = (int16_t)(b->x + g->xoff);
        rc->y = (int16_t)(b->y + g->yoff);
      }
      continue;
    }
    if (mask) {
      /* synthetic italic (the glyph's own font): pixels per pixel above the baseline */
      const float shear =
          (g_fonts[b->slot].synth & BUI_FONT_ITALIC) && !g_fonts[b->slot].xform ? 0.2f : 0.0f;
      blit_glyph(mask, mw, mh, left + b->x + g->xoff, asc + b->y + g->yoff, asc, g, shear);
    }
  }
  free(c.g);
  if (nc) {
    e->col = col;
    e->ncol = (int16_t)(nc < 32767 ? nc : 32767);
  }
  else {
    free(col);
  }
  if (!mask) {
    return;
  }
  if (fs->embolden > 0.0f) { /* synthetic bold: smear right */
    const int k = (int)(fs->embolden + 0.5f);
    for (int y = 0; y < mh; y++) {
      uint8_t *row = mask + (size_t)y * mw;
      for (int x = mw - 1; x >= k; x--) {
        int v = row[x];
        for (int j = 1; j <= k; j++) {
          v = row[x - j] > v ? row[x - j] : v;
        }
        row[x] = (uint8_t)v;
      }
    }
  }
  if (fs->flags & BUI_FONT_UNDERLINE) {
    const int t = fs->px / 14 > 1 ? fs->px / 14 : 1;
    const int y0 = asc + (fs->height - fs->ascent) / 2;
    for (int y = y0; y < y0 + t && y < mh; y++) {
      memset(mask + (size_t)y * mw + left, 255, (size_t)e->advance);
    }
  }
  int any = 0;
  for (int i = 0; i < mw * mh && !any; i++) {
    any = mask[i] != 0;
  }
  if (!any) {
    free(mask);
    return;
  }
  e->mask = mask;
  e->mw = mw;
  e->mh = (int16_t)mh;
  e->ox = (int16_t)left;
  e->asc = (int16_t)asc;
}

/** \} */
#endif /* text backends */

/* -------------------------------------------------------------------- */
/** \name Native present
 * \{ */

#ifdef _WIN32
int BUI_raster_present_native(BuiRaster *r, void *window_handle, int full)
{
  if (r->gpu) {
    if (!bui_gpu_present(r->gpu, full)) {
      gpu_drop(r); /* device lost: the software raster takes over */
    }
    return 1;
  }
  HWND hwnd = (HWND)window_handle;
  if (!hwnd || !IsWindow(hwnd)) {
    return 0;
  }
  HDC hdc = GetDC(hwnd);
  if (!hdc) {
    return 0;
  }
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = r->w;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  int whole[1][4] = {{0, 0, r->w, r->h}};
  const int(*rects)[4] = full ? (const int(*)[4])whole : (const int(*)[4])r->damage;
  const int n = full ? 1 : r->damage_len;
  /* Each rect is copied into its own tight top-down DIB and blitted whole
   * (src origin 0,0, all scanlines). Sub-rect source offsets into a
   * top-down DIB are interpreted inconsistently by GDI, which left partial
   * updates on screen stale. */
  uint32_t *tmp = NULL;
  size_t tmp_cap = 0;
  for (int i = 0; i < n; i++) {
    const int rx = rects[i][0], ry = rects[i][1], rw = rects[i][2], rh = rects[i][3];
    if (rw <= 0 || rh <= 0) {
      continue;
    }
    const uint32_t *bits = r->pix;
    if (!(rx == 0 && rw == r->w)) {
      const size_t need = (size_t)rw * rh;
      if (need > tmp_cap) {
        uint32_t *nt = (uint32_t *)realloc(tmp, need * sizeof(uint32_t));
        if (!nt) {
          continue;
        }
        tmp = nt;
        tmp_cap = need;
      }
      for (int row = 0; row < rh; row++) {
        memcpy(tmp + (size_t)row * rw, r->pix + (size_t)(ry + row) * r->w + rx,
               (size_t)rw * sizeof(uint32_t));
      }
      bits = tmp;
    }
    else {
      bits = r->pix + (size_t)ry * r->w; /* full-width band is contiguous */
    }
    bmi.bmiHeader.biWidth = rw;
    bmi.bmiHeader.biHeight = -rh;
    SetDIBitsToDevice(hdc, rx, ry, (DWORD)rw, (DWORD)rh, 0, 0, 0, (UINT)rh, bits, &bmi,
                      DIB_RGB_COLORS);
  }
  free(tmp);
  GdiFlush();
  ReleaseDC(hwnd, hdc);
  return 1;
}
#else
int BUI_raster_present_native(BuiRaster *r, void *window_handle, int full)
{
  if (r->gpu) {
    if (!bui_gpu_present(r->gpu, full)) {
      gpu_drop(r);
    }
    return 1;
  }
  (void)window_handle;
  (void)full;
  return 0;
}
#endif

/** \} */

#include "gpu_raster_awra.inc.c"

/* BUI-Awra: a font's vertical metrics (unrounded px): what text layouts need
 * for line heights and baselines. 0 when no face is found. */
int BUI_text_metrics(const char *family, int flags, float size, float *r_ascent, float *r_descent)
{
  *r_ascent = *r_descent = 0.0f;
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  if (f < 0) {
    return 0;
  }
#if TEXT_GDI
  *r_ascent = (float)g_fonts[f].ascent;
  *r_descent = (float)(g_fonts[f].height - g_fonts[f].ascent);
#else
  const FontSlot *s = &g_fonts[f];
  const Face *face = &g_faces[s->face];
  *r_ascent = (float)face->ascent * s->scale;
  *r_descent = (float)-face->descent * s->scale;
  if (g_hint_metrics) {
    /* FreeType's hinted size metrics, which Pango's line extents use with
     * hinted font metrics: ascender and descender each rounded outwards
     * to whole pixels (DejaVu Sans 13.33 px: 13 + 4, a 17 px line) */
    *r_ascent = ceilf(*r_ascent - 0.001f);
    *r_descent = ceilf(*r_descent - 0.001f);
  }
#endif
  return 1;
}

/* BUI-Awra: the underline's and strikethrough's position (of their top edge,
 * up from the baseline) and thickness, in px, as Pango's font metrics have
 * them (pangofc-font.c get_face_metrics: HarfBuzz's post and OS/2 values,
 * or 1 px thick, the underline 1 px under the baseline and the line through
 * at half the ascent where the font says 0; rounded up to whole pixels with
 * hinted font metrics, as pangocairo-fcfont.c). */
int BUI_text_decoration_metrics(const char *family, int flags, float size, float *r_ul_pos,
                                float *r_ul_thick, float *r_st_pos, float *r_st_thick)
{
  *r_ul_pos = -1.0f;
  *r_ul_thick = *r_st_thick = 1.0f;
  *r_st_pos = size * 0.3f;
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  if (f < 0) {
    return 0;
  }
#if TEXT_GDI
  return 0;
#else
  const FontSlot *s = &g_fonts[f];
  const Face *face = &g_faces[s->face];
  const stbtt_uint8 *data = (const stbtt_uint8 *)face->data;
  float asc = (float)face->hb_ascent * s->scale;
  if (!data) {
    return 0;
  }
  const stbtt_uint32 post = stbtt__find_table((stbtt_uint8 *)data, (stbtt_uint32)face->fontstart, "post");
  const stbtt_uint32 os2 = stbtt__find_table((stbtt_uint8 *)data, (stbtt_uint32)face->fontstart, "OS/2");
  const int ul_pos = post ? ttSHORT(data + post + 8) : 0;
  const int ul_thick = post ? ttSHORT(data + post + 10) : 0;
  const int st_thick = os2 ? ttSHORT(data + os2 + 26) : 0;
  const int st_pos = os2 ? ttSHORT(data + os2 + 28) : 0;
  *r_ul_thick = ul_thick != 0 ? (float)ul_thick * s->scale : 1.0f;
  *r_ul_pos = ul_pos != 0 ? (float)ul_pos * s->scale : -1.0f;
  *r_st_thick = st_thick != 0 ? (float)st_thick * s->scale : 1.0f;
  *r_st_pos = st_pos != 0 ? (float)st_pos * s->scale : asc * 0.5f;
  if (g_hint_metrics) {
    *r_ul_pos = ceilf(*r_ul_pos - 0.001f);
    *r_ul_thick = ceilf(*r_ul_thick - 0.001f);
    *r_st_pos = ceilf(*r_st_pos - 0.001f);
    *r_st_thick = ceilf(*r_st_thick - 0.001f);
  }
  return 1;
#endif
}

/* BUI-Awra: BUI_text_breaks as PangoLayout finds the attributes of text
 * in this font: tailored item by item, items split where the font changes
 * too (libthai sees only the Thai of a "123ภาษา"). */
void BUI_text_breaks_font(const char *family, int flags, float size, const char *utf8, int len,
                          uint16_t *attrs)
{
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  text_breaks(utf8, len, attrs, NULL, f);
}

/* BUI-Awra: the ascent and descent of a run of text (unrounded px): the
 * font's, or higher where the fonts it falls back to reach further (Pango's
 * logical extents: a line is as tall as the tallest font in it). */
int BUI_text_extents(const char *family, int flags, float size, const char *utf8, int len,
                     float *r_ascent, float *r_descent)
{
  if (!BUI_text_metrics(family, flags, size, r_ascent, r_descent)) {
    return 0;
  }
  if (len <= 0) {
    return 1;
  }
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  const TextRun *e = f >= 0 ? run_get(f, text_dir(flags), utf8, len) : NULL;
  if (e && e->uses_font) {
    /* the union of its fonts' extents, as aligned (glyph_walk_part) */
    *r_ascent = e->ext_asc;
    *r_descent = e->ext_desc;
  }
  return 1;
}

int BUI_text_latin_own_font(void)
{
  return text_latin_own_font();
}

int BUI_text_index_to_x(const char *family, int flags, float size, const char *utf8, int len,
                        int index, int trailing, float *r_x)
{
  *r_x = 0.0f;
  if (len <= 0) {
    return 1;
  }
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  TextRun *e = f >= 0 ? run_get(f, text_dir(flags), utf8, len) : NULL;
  if (!e) {
    return 0;
  }
  *r_x = run_index_to_x(e, index < 0 ? 0 : index > len ? len : index, trailing);
  return 1;
}

int BUI_text_x_to_index(const char *family, int flags, float size, const char *utf8, int len,
                        float x, int *r_index, int *r_trailing)
{
  *r_index = 0;
  *r_trailing = 0;
  if (len <= 0) {
    return 1;
  }
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  TextRun *e = f >= 0 ? run_get(f, text_dir(flags), utf8, len) : NULL;
  if (!e) {
    return 0;
  }
  *r_index = run_x_to_index(e, x, r_trailing);
  return 1;
}

int BUI_raster_text_part(BuiRaster *r, float x, float y, const char *utf8, int len, int from, int to,
                         float size, const float rgba[4])
{
  if (len <= 0 || to <= from) {
    return 1;
  }
  if (from <= 0 && to >= len) {
    return BUI_raster_text(r, x, y, utf8, len, size, rgba);
  }
#if TEXT_GDI
  /* GDI orders and shapes runs itself: the part on its own, where it is */
  {
    const int f = font_get(r->font_family, r->font_flags & ~TEXT_DIRS, size);
    TextRun *e = f >= 0 ? run_get(f, 0, utf8, len) : NULL;
    if (!e) {
      return 0;
    }
    return BUI_raster_text(r, x + run_index_to_x(e, from, 0), y, utf8 + from, to - from, size, rgba);
  }
#endif
  const int f = font_get(r->font_family, r->font_flags & ~TEXT_DIRS, size);
  TextRun *e = f >= 0 ? run_get_part(f, text_dir(r->font_flags), utf8, len, from, to) : NULL;
  if (!e) {
    return 0;
  }
  if (r->gpu) {
    if (!e->built) {
      run_build_at(e, 0.0f, 0.0f);
    }
  }
  else {
    run_build_at(e, x, y);
  }
  if (e->mask || e->ncol) {
    BUI_raster_text_run(r, x, y, (const BuiTextRun *)e, rgba);
  }
  return 1;
}

int BUI_text_clusters(const char *family, int flags, float size, const char *utf8, int len,
                      BuiTextCluster *out, int max)
{
  if (len <= 0) {
    return 0;
  }
  const int f = font_get(family, flags & ~TEXT_DIRS, size);
  TextRun *e = f >= 0 ? run_get(f, text_dir(flags), utf8, len) : NULL;
#if TEXT_GDI
  /* GDI: characters as clusters, from prefix widths */
  if (!e) {
    return 0;
  }
  int n = 0;
  float prev = 0.0f;
  for (int i = 0; i < len;) {
    const int next = i + utf8_step(utf8, i);
    const float w = run_index_to_x(e, next, 0);
    if (out && n < max) {
      out[n].start = i;
      out[n].end = next;
      out[n].width = w - prev;
    }
    n++;
    prev = w;
    i = next;
  }
  return out ? (n < max ? n : max) : n;
#else
  if (!e || !run_clusters(e)) {
    return 0;
  }
  const RunCarets *k = e->car;
  if (!out) {
    return k->n;
  }
  const int n = k->n < max ? k->n : max;
  for (int i = 0; i < n; i++) {
    out[i].start = k->cl[i].start;
    out[i].end = k->cl[i].end;
    out[i].width = fabsf(k->cl[i].trail - k->cl[i].lead);
  }
  return n;
#endif
}
