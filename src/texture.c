/* Awra 2 textures (GdkTexture / GdkMemoryTexture): immutable pixels shared
 * by every image, avatar or picture that shows them, by reference.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include "GPU_raster.h"

struct AwraTexture {
  int refs;
  int w, h;
  int format; /* BUI_PIXMAP_* */
  uint32_t *px;
  uint64_t key;
};

static uint64_t g_texture_keys = 1;

static AwraTexture *texture_alloc(int w, int h, int format, size_t bytes)
{
  if (w <= 0 || h <= 0) {
    return NULL;
  }
  AwraTexture *t = (AwraTexture *)calloc(1, sizeof(AwraTexture));
  if (!t) {
    return NULL;
  }
  t->px = (uint32_t *)malloc(bytes);
  if (!t->px) {
    free(t);
    return NULL;
  }
  t->refs = 1;
  t->w = w;
  t->h = h;
  t->format = format;
  /* high bit: a texture's key, never one the kit's image buffers use */
  t->key = (1ull << 63) | g_texture_keys++;
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
    t->refs++;
  }
  return t;
}

void awra_texture_unref(AwraTexture *t)
{
  if (!t || --t->refs > 0) {
    return;
  }
  BUI_image_forget(t->key);
  free(t->px);
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

static uint8_t clamp8(float v)
{
  return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5f);
}

/* video's narrow range YCbCr to RGB, as the raster draws it */
static void download_nv12(const AwraTexture *t, uint8_t *rgba, int stride)
{
  const bool bt709 = t->format == BUI_PIXMAP_NV12_BT709;
  const uint8_t *yp = (const uint8_t *)t->px, *uv = yp + (size_t)t->w * t->h;
  for (int y = 0; y < t->h; y++) {
    uint8_t *d = rgba + (size_t)y * stride;
    for (int x = 0; x < t->w; x++, d += 4) {
      const float c = 1.164f * (yp[(size_t)y * t->w + x] - 16);
      const uint8_t *q = uv + (size_t)(y / 2) * t->w + (x / 2) * 2;
      const float u = q[0] - 128.0f, v = q[1] - 128.0f;
      d[0] = clamp8(c + (bt709 ? 1.793f : 1.596f) * v);
      d[1] = clamp8(c - (bt709 ? 0.213f : 0.392f) * u - (bt709 ? 0.533f : 0.813f) * v);
      d[2] = clamp8(c + (bt709 ? 2.112f : 2.017f) * u);
      d[3] = 255;
    }
  }
}

bool awra_texture_download(const AwraTexture *t, uint8_t *rgba, int stride)
{
  if (t && (t->format == BUI_PIXMAP_NV12_BT601 || t->format == BUI_PIXMAP_NV12_BT709)) {
    download_nv12(t, rgba, stride);
    return true;
  }
  if (!t || t->format != BUI_PIXMAP_ARGB) {
    return false;
  }
  for (int y = 0; y < t->h; y++) {
    uint8_t *d = rgba + (size_t)y * stride;
    const uint32_t *s = t->px + (size_t)y * t->w;
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
  return true;
}

BuiPixmap awra_texture_pixmap(const AwraTexture *t)
{
  BuiPixmap pm = {t->px, t->w, t->h, t->key, t->format, 0};
  return pm;
}

/* Draws a texture into box (x, y, w, h), raster pixels, with rounded
 * corners, fitted (0 fill, 1 contain, 2 cover, 3 scale-down) and clipped to
 * the box. */
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
  if (radius > 0.0f) {
    const float radii[4] = {radius, radius, radius, radius};
    BUI_raster_image_rounded(d->r, bx, by, bw, bh, &pm, src, d->opacity, radii);
  }
  else {
    BUI_raster_image(d->r, bx, by, bw, bh, &pm, src, d->opacity, 0);
  }
}
