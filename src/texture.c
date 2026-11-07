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
 * corners, fitted (0 fill, 1 contain, 2 cover, 3 scale-down; with
 * AWRA_FIT_PICTURE placed as GtkPicture places it) and clipped to the box, filtered as GSK draws a texture node (mipmapped under half its
 * size). */
void awra_paint_texture(AwraDraw *d, const AwraTexture *t, AwraBox b, float radius, int fit)
{
  awra_paint_texture_filtered(d, t, b, radius, fit, BUI_IMAGE_MIPMAP_AUTO);
}

/* awra_paint_texture with BUI_IMAGE_* filter flags (BUI_IMAGE_TRILINEAR:
 * gtk_snapshot_append_scaled_texture's GSK_SCALING_FILTER_TRILINEAR). */
void awra_paint_texture_filtered(AwraDraw *d, const AwraTexture *t, AwraBox b, float radius, int fit,
                                 int filter)
{
  if (!t || b.w <= 0 || b.h <= 0) {
    return;
  }
  float bx = b.x, by = b.y, bw = b.w, bh = b.h;
  const float ar = (float)t->w / t->h, br = bw / bh;
  float src[4] = {0, 0, (float)t->w, (float)t->h};
  const bool picture = (fit & AWRA_FIT_PICTURE) != 0;
  fit &= ~AWRA_FIT_PICTURE;
  if (picture && fit != 0) {
    /* gtk_picture_snapshot: the paintable's size rounded up, its offset
     * truncated towards zero ((int)((width - w) / 2)), drawn whole and
     * clipped to the box (here: the part of it the box shows) */
    double w, h;
    if (fit == 3 && bw >= t->w && bh >= t->h) {
      w = t->w;
      h = t->h;
    }
    else if ((ar > br) == (fit == 2)) {
      w = bh * (double)ar;
      h = bh;
    }
    else {
      w = bw;
      h = bw / (double)ar;
    }
    w = ceil(w);
    h = ceil(h);
    const int ox = (int)((bw - w) / 2), oy = (int)(floor(bh - h) / 2);
    if (fit == 2) {
      src[0] = (float)(-ox * t->w / w);
      src[1] = (float)(-oy * t->h / h);
      src[2] = (float)(bw * t->w / w);
      src[3] = (float)(bh * t->h / h);
    }
    else {
      bx += (float)ox;
      by += (float)oy;
      bw = (float)w;
      bh = (float)h;
    }
  }
  else if (fit == 3 && t->w <= bw && t->h <= bh) {
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
    BUI_raster_image_rounded_ex(d->r, bx, by, bw, bh, &pm, src, d->opacity, radii, filter);
  }
  else {
    BUI_raster_image(d->r, bx, by, bw, bh, &pm, src, d->opacity, filter);
  }
}
