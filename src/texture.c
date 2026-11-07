/* Awra 2 textures (GdkTexture / GdkMemoryTexture): immutable pixels shared
 * by every image, avatar or picture that shows them, by reference.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include "GPU_raster.h"

struct AwraTexture {
  int refs; /* atomic: textures may be made and dropped off the UI thread */
  int w, h;
  int format; /* BUI_PIXMAP_* */
  uint32_t *px;
  uint64_t key;
  /* pixels the texture borrows (awra_texture_new_for_data): given back
   * through release(user) once the last reference goes; else px is owned */
  void (*release)(void *user);
  void *user;
  bool borrowed;
  /* drawn at less than half its size, GSK samples the texture's mipmaps
   * (trilinear): the levels, and the last drawing made from them (its
   * geometry), made and read on the UI thread, freed with the texture */
  uint32_t **mips; /* levels 1.. (level 0: px, or its ARGB copy in mip0) */
  int *mip_w, *mip_h, nmips;
  uint32_t *mip0;
  uint32_t *small;
  int small_w, small_h;
  float small_geom[9];
  uint64_t small_key;
};

static uint64_t g_texture_keys = 1;

static uint64_t next_key(void)
{
#ifdef _MSC_VER
  return (uint64_t)_InterlockedIncrement64((volatile __int64 *)&g_texture_keys);
#else
  return __atomic_add_fetch(&g_texture_keys, 1, __ATOMIC_RELAXED);
#endif
}

static AwraTexture *texture_alloc(int w, int h, int format, size_t bytes)
{
  if (w <= 0 || h <= 0) {
    return NULL;
  }
  AwraTexture *t = (AwraTexture *)calloc(1, sizeof(AwraTexture));
  if (!t) {
    return NULL;
  }
  if (bytes) {
    t->px = (uint32_t *)malloc(bytes);
    if (!t->px) {
      free(t);
      return NULL;
    }
  }
  t->refs = 1;
  t->w = w;
  t->h = h;
  t->format = format;
  /* high bit: a texture's key, never one the kit's image buffers use */
  t->key = (1ull << 63) | next_key();
  return t;
}

AwraTexture *awra_texture_new_for_data(const void *data, int width, int height,
                                       AwraTextureFormat format, bool bt709,
                                       void (*release)(void *user_data), void *user_data)
{
  int pm;
  switch (format) {
    case AWRA_TEXTURE_ARGB_PREMULTIPLIED:
      pm = BUI_PIXMAP_ARGB;
      break;
    case AWRA_TEXTURE_RGBA:
      pm = BUI_PIXMAP_RGBA;
      break;
    case AWRA_TEXTURE_NV12:
      pm = bt709 ? BUI_PIXMAP_NV12_BT709 : BUI_PIXMAP_NV12_BT601;
      break;
    case AWRA_TEXTURE_I420:
      pm = bt709 ? BUI_PIXMAP_I420_BT709 : BUI_PIXMAP_I420_BT601;
      break;
    default:
      return NULL;
  }
  /* pixels are read a word at a time; 4:2:0 covers two by two pixels */
  if (!data || ((uintptr_t)data & 3) || (BUI_pixmap_yuv(pm) && ((width | height) & 1))) {
    return NULL;
  }
  AwraTexture *t = texture_alloc(width, height, pm, 0);
  if (t) {
    t->px = (uint32_t *)data;
    t->release = release;
    t->user = user_data;
    t->borrowed = true;
  }
  return t;
}

AwraTexture *awra_texture_new_rgba(const uint8_t *rgba, int width, int height, int stride)
{
  AwraTexture *t = texture_alloc(width, height, BUI_PIXMAP_ARGB, sizeof(uint32_t) * (size_t)width * height);
  if (!t) {
    return NULL;
  }
  for (int y = 0; y < height; y++) {
    const uint8_t *s = rgba + (size_t)y * stride;
    uint32_t *d = t->px + (size_t)y * width;
    for (int x = 0; x < width; x++, s += 4) {
      const uint32_t a = s[3];
      uint32_t r = s[0], g = s[1], b = s[2];
      if (a != 255) {
        r = (r * a + 127) / 255;
        g = (g * a + 127) / 255;
        b = (b * a + 127) / 255;
      }
      d[x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }
  return t;
}

AwraTexture *awra_texture_new_argb(const uint32_t *argb, int width, int height)
{
  AwraTexture *t = texture_alloc(width, height, BUI_PIXMAP_ARGB, sizeof(uint32_t) * (size_t)width * height);
  if (t) {
    memcpy(t->px, argb, sizeof(uint32_t) * (size_t)width * height);
  }
  return t;
}

AwraTexture *awra_texture_new_argb_region(const uint32_t *px, int width, int height, int x, int y,
                                          int w, int h)
{
  AwraTexture *t = texture_alloc(w, h, BUI_PIXMAP_ARGB, sizeof(uint32_t) * (size_t)w * h);
  if (!t) {
    return NULL;
  }
  const int x0 = BK_MAX(x, 0), x1 = BK_MIN(x + w, width);
  for (int row = 0; row < h; row++) {
    uint32_t *d = t->px + (size_t)row * w;
    const int sy = y + row;
    if (sy < 0 || sy >= height || x1 <= x0) {
      memset(d, 0, sizeof(uint32_t) * (size_t)w);
      continue;
    }
    if (x0 > x) {
      memset(d, 0, sizeof(uint32_t) * (size_t)(x0 - x));
    }
    memcpy(d + (x0 - x), px + (size_t)sy * width + x0, sizeof(uint32_t) * (size_t)(x1 - x0));
    if (x + w > x1) {
      memset(d + (x1 - x), 0, sizeof(uint32_t) * (size_t)(x + w - x1));
    }
  }
  return t;
}

AwraTexture *awra_texture_new_nv12(const uint8_t *data, int width, int height, bool bt709)
{
  width &= ~1;
  height &= ~1;
  const size_t bytes = (size_t)width * height * 3 / 2;
  AwraTexture *t = texture_alloc(width, height, bt709 ? BUI_PIXMAP_NV12_BT709 : BUI_PIXMAP_NV12_BT601,
                                 (bytes + 3) & ~(size_t)3);
  if (t) {
    memcpy(t->px, data, bytes);
  }
  return t;
}

AwraTexture *awra_texture_new_i420(const uint8_t *data, int width, int height, bool bt709)
{
  /* kept as NV12: the planes of chroma interleave */
  const int w = width & ~1, h = height & ~1;
  const size_t bytes = (size_t)w * h * 3 / 2;
  AwraTexture *t = texture_alloc(w, h, bt709 ? BUI_PIXMAP_NV12_BT709 : BUI_PIXMAP_NV12_BT601,
                                 (bytes + 3) & ~(size_t)3);
  if (!t) {
    return NULL;
  }
  uint8_t *dst = (uint8_t *)t->px;
  for (int y = 0; y < h; y++) {
    memcpy(dst + (size_t)y * w, data + (size_t)y * width, (size_t)w);
  }
  const int cw = width / 2, ch = height / 2;
  const uint8_t *u = data + (size_t)width * height, *v = u + (size_t)cw * ch;
  uint8_t *uv = dst + (size_t)w * h;
  for (int y = 0; y < h / 2; y++) {
    const uint8_t *ur = u + (size_t)y * cw, *vr = v + (size_t)y * cw;
    uint8_t *o = uv + (size_t)y * w;
    for (int x = 0; x < w / 2; x++) {
      o[2 * x] = ur[x];
      o[2 * x + 1] = vr[x];
    }
  }
  return t;
}

AwraTexture *awra_texture_new_from_bytes(const void *encoded, size_t length)
{
  BKImBuf *ib = BK_imbuf_decode(encoded, length, 0, 0);
  if (!ib) {
    return NULL;
  }
  int w, h;
  BK_imbuf_size(ib, &w, &h);
  AwraTexture *t = awra_texture_new_argb(BK_imbuf_pixels(ib, 0), w, h);
  BK_imbuf_free(ib);
  return t;
}

AwraTexture *awra_texture_new_from_file(const char *path)
{
  BKImBuf *ib = BK_imbuf_load(path, 0, 0);
  if (!ib) {
    return NULL;
  }
  int w, h;
  BK_imbuf_size(ib, &w, &h);
  AwraTexture *t = awra_texture_new_argb(BK_imbuf_pixels(ib, 0), w, h);
  BK_imbuf_free(ib);
  return t;
}

AwraTexture *awra_texture_ref(AwraTexture *t)
{
  if (t) {
#ifdef _MSC_VER
    _InterlockedIncrement((volatile long *)&t->refs);
#else
    __atomic_add_fetch(&t->refs, 1, __ATOMIC_RELAXED);
#endif
  }
  return t;
}

void awra_texture_unref(AwraTexture *t)
{
  if (!t) {
    return;
  }
#ifdef _MSC_VER
  const int left = (int)_InterlockedDecrement((volatile long *)&t->refs);
#else
  const int left = __atomic_sub_fetch(&t->refs, 1, __ATOMIC_ACQ_REL);
#endif
  if (left > 0) {
    return;
  }
  BUI_image_forget(t->key);
  if (t->small) {
    BUI_image_forget(t->small_key);
    free(t->small);
  }
  for (int i = 0; i < t->nmips; i++) {
    free(t->mips[i]);
  }
  free(t->mips);
  free(t->mip_w);
  free(t->mip_h);
  free(t->mip0);
  if (!t->borrowed) {
    free(t->px);
  }
  else if (t->release) {
    t->release(t->user);
  }
  free(t);
}

int awra_texture_width(const AwraTexture *t)
{
  return t ? t->w : 0;
}

int awra_texture_height(const AwraTexture *t)
{
  return t ? t->h : 0;
}

BuiPixmap awra_texture_pixmap(const AwraTexture *t)
{
  BuiPixmap pm = {t->px, t->w, t->h, t->key, t->format, 0};
  return pm;
}

bool awra_texture_download(const AwraTexture *t, uint8_t *rgba, int stride)
{
  if (!t) {
    return false;
  }
  if (t->format == BUI_PIXMAP_RGBA) {
    for (int y = 0; y < t->h; y++) {
      memcpy(rgba + (size_t)y * stride, (const uint8_t *)t->px + (size_t)y * t->w * 4, (size_t)t->w * 4);
    }
    return true;
  }
  /* premultiplied ARGB a row at a time (YCbCr converted as drawing does) */
  uint32_t *row = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)t->w);
  if (!row) {
    return false;
  }
  const BuiPixmap pm = awra_texture_pixmap(t);
  for (int y = 0; y < t->h; y++) {
    const uint32_t *s;
    if (t->format == BUI_PIXMAP_ARGB) {
      s = t->px + (size_t)y * t->w;
    }
    else {
      BUI_pixmap_read(&pm, y, 1, row);
      s = row;
    }
    uint8_t *d = rgba + (size_t)y * stride;
    for (int x = 0; x < t->w; x++, d += 4) {
      const uint32_t p = s[x], a = p >> 24;
      uint32_t r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
      if (a && a != 255) {
        r = BK_MIN(255u, (r * 255 + a / 2) / a);
        g = BK_MIN(255u, (g * 255 + a / 2) / a);
        b = BK_MIN(255u, (b * 255 + a / 2) / a);
      }
      d[0] = (uint8_t)r;
      d[1] = (uint8_t)g;
      d[2] = (uint8_t)b;
      d[3] = (uint8_t)a;
    }
  }
  free(row);
  return true;
}


/* Draws a texture into box (x, y, w, h), raster pixels, with rounded
 * corners, fitted (0 fill, 1 contain, 2 cover, 3 scale-down) and clipped to
 * the box. */
/* GL's bilinear sample of a level at texel coordinates (u, v) in pixels
 * (CLAMP_TO_EDGE), premultiplied channels as floats */
static void bilinear(const uint32_t *px, int w, int h, float u, float v, float out[4])
{
  u -= 0.5f;
  v -= 0.5f;
  const int x0 = (int)floorf(u), y0 = (int)floorf(v);
  const float fx = u - (float)x0, fy = v - (float)y0;
  for (int k = 0; k < 4; k++) {
    out[k] = 0.0f;
  }
  for (int j = 0; j < 2; j++) {
    const int y = BK_MAX(0, BK_MIN(h - 1, y0 + j));
    for (int i = 0; i < 2; i++) {
      const int x = BK_MAX(0, BK_MIN(w - 1, x0 + i));
      const float wgt = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
      const uint32_t p = px[(size_t)y * w + x];
      out[0] += wgt * (float)(p >> 24);
      out[1] += wgt * (float)((p >> 16) & 255);
      out[2] += wgt * (float)((p >> 8) & 255);
      out[3] += wgt * (float)(p & 255);
    }
  }
}

static uint32_t pack(const float c[4])
{
  uint32_t v[4];
  for (int k = 0; k < 4; k++) {
    const float f = c[k] + 0.5f;
    v[k] = f <= 0.0f ? 0u : f >= 255.0f ? 255u : (uint32_t)f;
  }
  for (int k = 1; k < 4; k++) {
    v[k] = v[k] > v[0] ? v[0] : v[k];
  }
  return v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3];
}

/* level 0 as premultiplied ARGB */
static const uint32_t *level0(AwraTexture *t)
{
  if (t->format == BUI_PIXMAP_ARGB) {
    return t->px;
  }
  if (!t->mip0) {
    t->mip0 = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)t->w * t->h);
    if (t->mip0) {
      const BuiPixmap pm = awra_texture_pixmap(t);
      BUI_pixmap_read(&pm, 0, t->h, t->mip0);
    }
  }
  return t->mip0;
}

/* the mipmap chain as glGenerateMipmap makes it: each level half the one
 * above (floored, at least 1), sampled at its texel centres */
static bool make_mips(AwraTexture *t)
{
  if (t->mips) {
    return true;
  }
  const uint32_t *prev = level0(t);
  if (!prev) {
    return false;
  }
  int n = 0;
  for (int w = t->w, h = t->h; w > 1 || h > 1; w = BK_MAX(1, w / 2), h = BK_MAX(1, h / 2)) {
    n++;
  }
  t->mips = (uint32_t **)calloc((size_t)BK_MAX(n, 1), sizeof(uint32_t *));
  t->mip_w = (int *)calloc((size_t)BK_MAX(n, 1), sizeof(int));
  t->mip_h = (int *)calloc((size_t)BK_MAX(n, 1), sizeof(int));
  if (!t->mips || !t->mip_w || !t->mip_h) {
    free(t->mips), free(t->mip_w), free(t->mip_h);
    t->mips = NULL;
    return false;
  }
  int pw = t->w, ph = t->h;
  for (int i = 0; i < n; i++) {
    const int w = BK_MAX(1, pw / 2), h = BK_MAX(1, ph / 2);
    uint32_t *lv = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)w * h);
    if (!lv) {
      break;
    }
    const float kx = (float)pw / w, ky = (float)ph / h;
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        float c[4];
        bilinear(prev, pw, ph, (x + 0.5f) * kx, (y + 0.5f) * ky, c);
        lv[(size_t)y * w + x] = pack(c);
      }
    }
    t->mips[i] = lv;
    t->mip_w[i] = w;
    t->mip_h[i] = h;
    t->nmips = i + 1;
    prev = lv;
    pw = w;
    ph = h;
  }
  return true;
}

static const uint32_t *level(AwraTexture *t, int i, int *w, int *h)
{
  if (i <= 0 || t->nmips == 0) {
    *w = t->w;
    *h = t->h;
    return level0(t);
  }
  i = BK_MIN(i, t->nmips);
  *w = t->mip_w[i - 1];
  *h = t->mip_h[i - 1];
  return t->mips[i - 1];
}

/* GSK's coverage: the pixel's share of the rectangle (rect_coverage) times
 * the rounded clip's (0.5 - distance, rounded_rect_coverage) */
static float gsk_coverage(float px, float py, float x0, float y0, float x1, float y1, float radius)
{
  const float cx = fmaxf(0.0f, fminf(px + 0.5f, x1) - fmaxf(px - 0.5f, x0));
  const float cy = fmaxf(0.0f, fminf(py + 0.5f, y1) - fmaxf(py - 0.5f, y0));
  float cov = cx * cy;
  if (radius > 0.0f && cov > 0.0f) {
    const float r = fminf(radius, fminf((x1 - x0) * 0.5f, (y1 - y0) * 0.5f));
    float dist;
    const float lx = x0 + r, rx = x1 - r, ty = y0 + r, by = y1 - r;
    if ((px < lx || px > rx) && (py < ty || py > by)) {
      const float ex = px < lx ? lx : rx, ey = py < ty ? ty : by;
      dist = hypotf(px - ex, py - ey) - r;
    }
    else {
      const float dx = fmaxf(x0 - px, px - x1), dy = fmaxf(y0 - py, py - y1);
      const float m = fmaxf(dx, dy);
      dist = sqrtf(fmaxf(dx, 0.0f) * fmaxf(dx, 0.0f) + fmaxf(dy, 0.0f) * fmaxf(dy, 0.0f)) + fminf(m, 0.0f);
    }
    cov *= fminf(1.0f, fmaxf(0.0f, 0.5f - dist));
  }
  return cov;
}

/* A texture drawn smaller than half its size: each device pixel sampled
 * trilinearly from the mipmaps at its centre, times GSK's coverage, into a
 * pixmap on the pixel grid (kept while the geometry stays). */
static bool texture_sampled(AwraTexture *t, float bx, float by, float bw, float bh, const float src[4], float radius,
                            int *r_x, int *r_y)
{
  const int X0 = (int)floorf(bx), Y0 = (int)floorf(by);
  const int X1 = (int)ceilf(bx + bw), Y1 = (int)ceilf(by + bh);
  const int dw = X1 - X0, dh = Y1 - Y0;
  *r_x = X0;
  *r_y = Y0;
  const float geom[9] = {bx - X0, by - Y0, bw, bh, src[0], src[1], src[2], src[3], radius};
  if (t->small && t->small_w == dw && t->small_h == dh && !memcmp(t->small_geom, geom, sizeof(geom))) {
    return true;
  }
  if (dw <= 0 || dh <= 0 || BUI_pixmap_yuv(t->format) || !make_mips(t)) {
    return false;
  }
  uint32_t *out = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)dw * dh);
  if (!out) {
    return false;
  }
  /* texels per device pixel: GL's lambda = log2(rho) */
  const float sx = src[2] / bw, sy = src[3] / bh;
  const float lambda = log2f(fmaxf(sx, sy));
  const int d1 = BK_MAX(0, BK_MIN(t->nmips, (int)floorf(lambda)));
  const int d2 = BK_MIN(t->nmips, d1 + 1);
  const float mix = d2 > d1 ? fminf(1.0f, fmaxf(0.0f, lambda - floorf(lambda))) : 0.0f;
  int w1, h1, w2, h2;
  const uint32_t *l1 = level(t, d1, &w1, &h1), *l2 = level(t, d2, &w2, &h2);
  for (int y = 0; y < dh; y++) {
    const float py = (float)(Y0 + y) + 0.5f;
    const float v = (src[1] + (py - by) * sy) / t->h; /* normalized */
    for (int x = 0; x < dw; x++) {
      const float px = (float)(X0 + x) + 0.5f;
      const float cov = gsk_coverage(px, py, bx, by, bx + bw, by + bh, radius);
      if (cov <= 0.0f) {
        out[(size_t)y * dw + x] = 0;
        continue;
      }
      const float u = (src[0] + (px - bx) * sx) / t->w;
      float a[4], b[4];
      bilinear(l1, w1, h1, u * w1, v * h1, a);
      if (mix > 0.0f) {
        bilinear(l2, w2, h2, u * w2, v * h2, b);
        for (int k = 0; k < 4; k++) {
          a[k] += (b[k] - a[k]) * mix;
        }
      }
      for (int k = 0; k < 4; k++) {
        a[k] *= cov;
      }
      out[(size_t)y * dw + x] = pack(a);
    }
  }
  if (t->small) {
    BUI_image_forget(t->small_key);
    free(t->small);
  }
  t->small = out;
  t->small_w = dw;
  t->small_h = dh;
  memcpy(t->small_geom, geom, sizeof(geom));
  t->small_key = (1ull << 63) | next_key();
  return true;
}

void awra_paint_texture(AwraDraw *d, const AwraTexture *t, AwraBox b, float radius, int fit)
{
  if (!t || b.w <= 0 || b.h <= 0) {
    return;
  }
  float bx = b.x, by = b.y, bw = b.w, bh = b.h;
  const float ar = (float)t->w / t->h, br = bw / bh;
  float src[4] = {0, 0, (float)t->w, (float)t->h};
  if (fit == 3 && t->w <= bw && t->h <= bh) {
    bx += floorf((bw - t->w) * 0.5f);
    by += floorf((bh - t->h) * 0.5f);
    bw = (float)t->w;
    bh = (float)t->h;
  }
  else if (fit == 1 || fit == 3) {
    if (ar > br) {
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
  else if (fit == 2) {
    /* crop the source to the box's aspect */
    if (ar > br) {
      const float sw = t->h * br;
      src[0] = (t->w - sw) * 0.5f;
      src[2] = sw;
    }
    else {
      const float sh = t->w / br;
      src[1] = (t->h - sh) * 0.5f;
      src[3] = sh;
    }
  }
  BuiPixmap pm = awra_texture_pixmap(t);
  /* more than twice smaller: sampled from the mipmaps as GSK does */
  if ((t->w > 2.0f * bw * t->w / src[2] || t->h > 2.0f * bh * t->h / src[3]) && !BUI_pixmap_yuv(t->format)) {
    int X, Y;
    /* at the draw shift (raster-shift.h): its pixels are placed whole */
    if (texture_sampled((AwraTexture *)t, bx + awra_shift_x, by + awra_shift_y, bw, bh, src, radius, &X, &Y)) {
      const BuiPixmap spm = {t->small, t->small_w, t->small_h, t->small_key, BUI_PIXMAP_ARGB, 0};
      BUI_raster_image(d->r, (float)X, (float)Y, (float)t->small_w, (float)t->small_h, &spm, NULL, d->opacity, 0);
      return;
    }
  }
  if (radius > 0.0f) {
    const float radii[4] = {radius, radius, radius, radius};
    SR_image_rounded(d->r, bx, by, bw, bh, &pm, src, d->opacity, radii);
  }
  else {
    SR_image(d->r, bx, by, bw, bh, &pm, src, d->opacity, 0);
  }
}
