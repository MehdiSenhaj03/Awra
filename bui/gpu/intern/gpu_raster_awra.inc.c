/* BUI-Awra: raster primitives for Awra's materials (included by gpu_raster.c,
 * after its helpers). Everything draws premultiplied source-over into the
 * ARGB frame, anti-aliased with the same signed-distance coverage as the
 * upstream primitives, and runs only over the pixels it can touch.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* -------------------------------------------------------------------- */
/** \name Frame storage
 * \{ */

void BUI_raster_clear(BuiRaster *r, int x, int y, int w, int h)
{
  if (r->gpu) {
    const float zero[4] = {0, 0, 0, 0};
    (void)zero;
    return; /* the GPU target is cleared by its own begin */
  }
  const int x0 = clampi(x, r->cx0, r->cx1), x1 = clampi(x + w, r->cx0, r->cx1);
  const int y0 = clampi(y, r->cy0, r->cy1), y1 = clampi(y + h, r->cy0, r->cy1);
  if (x1 <= x0) {
    return;
  }
  for (int py = y0; py < y1; py++) {
    memset(RASTER_ROW(r, py) + x0, 0, sizeof(uint32_t) * (size_t)(x1 - x0));
  }
}

void BUI_raster_set_target(BuiRaster *r, uint32_t *pixels, int w, int h)
{
  if (r->gpu) {
    return;
  }
  if (!r->external) {
    free(r->pix);
  }
  free(r->prev);
  r->prev = NULL;
  r->pix = pixels;
  r->external = pixels != NULL;
  r->nodiff = 1;
  r->w = w > 0 ? w : 1;
  r->h = h > 0 ? h : 1;
  if (!pixels) { /* back to a buffer of its own */
    r->pix = (uint32_t *)calloc((size_t)r->w * r->h, sizeof(uint32_t));
  }
  raster_reset_clip(r);
}

void BUI_raster_set_origin(BuiRaster *r, int x, int y)
{
  if (r->gpu) {
    return;
  }
  r->ox = x;
  r->oy = y;
  raster_reset_clip(r);
}

void BUI_raster_set_diff(BuiRaster *r, int on)
{
  r->nodiff = !on;
  if (r->nodiff) {
    free(r->prev);
    r->prev = NULL;
  }
  else if (!r->prev && !r->gpu) {
    r->prev = (uint32_t *)calloc((size_t)r->w * r->h, sizeof(uint32_t));
    r->full_damage = 1;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rounded boxes with per-corner radii and per-side borders
 * \{ */

/* Signed distance from pixel center (sx, sy) to a box (x0, y0, x1, y1) whose
 * corners have radii rad[4] (top-left, top-right, bottom-right, bottom-left). */
static inline float sd_rbox(
    float sx, float sy, float x0, float y0, float x1, float y1, const float rad[4])
{
  const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
  const float hw = (x1 - x0) * 0.5f, hh = (y1 - y0) * 0.5f;
  const float px = sx - cx, py = sy - cy;
  const float rr = py < 0.0f ? (px < 0.0f ? rad[0] : rad[1]) : (px < 0.0f ? rad[3] : rad[2]);
  const float qx = fabsf(px) - (hw - rr), qy = fabsf(py) - (hh - rr);
  if (qx <= 0.0f || qy <= 0.0f) {
    return fmaxf(qx, qy) - rr;
  }
  return sqrtf(qx * qx + qy * qy) - rr;
}

static void clamp_radii(const float in[4], float w, float h, float out[4])
{
  const float lim = fminf(w, h) * 0.5f;
  for (int i = 0; i < 4; i++) {
    out[i] = clampf(in ? in[i] : 0.0f, 0.0f, lim);
  }
}

void BUI_raster_rbox(BuiRaster *r,
                     float x,
                     float y,
                     float w,
                     float h,
                     const float radii[4],
                     const float fill[4],
                     const float widths[4],
                     const float border[4])
{
  if (!(w > 0.0f) || !(h > 0.0f)) {
    return;
  }
  const int has_fill = fill && fill[3] > 0.0f;
  const int has_border = border && border[3] > 0.0f && widths &&
                         (widths[0] > 0.0f || widths[1] > 0.0f || widths[2] > 0.0f ||
                          widths[3] > 0.0f);
  if (!has_fill && !has_border) {
    return;
  }
  float rad[4];
  clamp_radii(radii, w, h, rad);
  if (r->gpu) { /* the GPU path keeps upstream's uniform-radius instances */
    if (has_fill) {
      BUI_raster_round_rect(r, x, y, w, h, rad[0], fill);
    }
    return;
  }
  int x0, y0, x1, y1;
  if (!clip_box(r, x, y, x + w, y + h, &x0, &y0, &x1, &y1)) {
    return;
  }
  const Paint pf = has_fill ? paint_from(fill) : (Paint){0, 0};
  const Paint pb = has_border ? paint_from(border) : (Paint){0, 0};
  /* inner box of the border (CSS: radii shrink by the adjacent widths) */
  const float bt = has_border ? widths[0] : 0.0f, br = has_border ? widths[1] : 0.0f;
  const float bb = has_border ? widths[2] : 0.0f, bl = has_border ? widths[3] : 0.0f;
  const float ix0 = x + bl, iy0 = y + bt, ix1 = x + w - br, iy1 = y + h - bb;
  const float irad[4] = {fmaxf(0.0f, rad[0] - fmaxf(bl, bt)), fmaxf(0.0f, rad[1] - fmaxf(br, bt)),
                         fmaxf(0.0f, rad[2] - fmaxf(br, bb)), fmaxf(0.0f, rad[3] - fmaxf(bl, bb))};
  const float rmax_top = fmaxf(rad[0], rad[1]), rmax_bot = fmaxf(rad[2], rad[3]);
  const float rmax_left = fmaxf(rad[0], rad[3]), rmax_right = fmaxf(rad[1], rad[2]);
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float sy = (float)py + 0.5f;
    /* Interior span of this row: fully inside the outer box and, with a
     * border, fully inside the inner box too: only the fill touches it. */
    int sa = x1, sb = x0;
    const int in_rows = sy - 0.5f >= y + rmax_top && sy + 0.5f <= y + h - rmax_bot;
    if (in_rows && sy - 0.5f >= iy0 && sy + 0.5f <= iy1) {
      sa = clampi((int)ceilf(fmaxf(x + rmax_left, ix0)), x0, x1);
      sb = clampi((int)floorf(fminf(x + w - rmax_right, ix1)), x0, x1);
      if (sb > sa && has_fill) {
        if (pf.a >= 256) {
          for (int px = sa; px < sb; px++) {
            row[px] = pf.rgb;
          }
        }
        else {
          for (int px = sa; px < sb; px++) {
            row[px] = blend(row[px], pf.rgb, (uint32_t)pf.a);
          }
        }
      }
      if (sb <= sa) {
        sa = x1;
        sb = x0;
      }
    }
    for (int px = x0; px < x1; px++) {
      if (px == sa) {
        px = sb - 1;
        continue;
      }
      const float sx = (float)px + 0.5f;
      const float co = clampf(0.5f - sd_rbox(sx, sy, x, y, x + w, y + h, rad), 0.0f, 1.0f);
      if (co <= 0.0f) {
        continue;
      }
      if (has_fill) {
        put_cov(row + px, pf, co);
      }
      if (has_border) {
        float ci = 0.0f;
        if (ix1 > ix0 && iy1 > iy0) {
          ci = clampf(0.5f - sd_rbox(sx, sy, ix0, iy0, ix1, iy1, irad), 0.0f, 1.0f);
        }
        if (co - ci > 0.0f) {
          put_cov(row + px, pb, co - ci);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box shadows (CSS / GSK outset shadows)
 * \{ */

/* 0.5 * erfc(x): the coverage of a Gaussian-blurred edge at signed distance
 * x (in sigma * sqrt(2) units). Abramowitz-Stegun 7.1.26, |error| < 1.5e-7. */
static inline float half_erfc(float x)
{
  const float ax = fabsf(x);
  const float t = 1.0f / (1.0f + 0.3275911f * ax);
  const float y = 1.0f - (((((1.061405429f * t - 1.453152027f) * t) + 1.421413741f) * t -
                           0.284496736f) * t + 0.254829592f) * t * expf(-ax * ax);
  return x >= 0.0f ? 0.5f * (1.0f - y) : 0.5f * (1.0f + y);
}

void BUI_raster_box_shadow(BuiRaster *r,
                           float x,
                           float y,
                           float w,
                           float h,
                           const float radii[4],
                           float dx,
                           float dy,
                           float spread,
                           float blur,
                           const float rgba[4],
                           int clip_out)
{
  if (!(w > 0.0f) || !(h > 0.0f) || !rgba || !(rgba[3] > 0.0f) || r->gpu) {
    return;
  }
  float rad[4], srad[4];
  clamp_radii(radii, w, h, rad);
  /* the shadow's shape: the box moved and grown by the spread */
  const float sx0 = x + dx - spread, sy0 = y + dy - spread;
  const float sx1 = x + w + dx + spread, sy1 = y + h + dy + spread;
  for (int i = 0; i < 4; i++) {
    srad[i] = rad[i] > 0.0f ? fmaxf(0.0f, rad[i] + spread) : 0.0f;
  }
  const float sigma = fmaxf(blur * 0.5f, 0.01f); /* CSS: blur radius = 2 sigma */
  const float reach = blur > 0.0f ? sigma * 3.0f : 0.5f;
  int x0, y0, x1, y1;
  if (!clip_box(r, sx0 - reach, sy0 - reach, sx1 + reach, sy1 + reach, &x0, &y0, &x1, &y1)) {
    return;
  }
  const Paint p = paint_from(rgba);
  const float inv = 1.0f / (sigma * 1.41421356f);
  const float rmax = fmaxf(fmaxf(rad[0], rad[1]), fmaxf(rad[2], rad[3]));
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float cy = (float)py + 0.5f;
    /* pixels fully inside the box draw nothing when the box clips the shadow */
    int sa = x1, sb = x0;
    if (clip_out && cy - 0.5f >= y + rmax && cy + 0.5f <= y + h - rmax) {
      sa = clampi((int)ceilf(x), x0, x1);
      sb = clampi((int)floorf(x + w), x0, x1);
      if (sb <= sa) {
        sa = x1;
        sb = x0;
      }
    }
    for (int px = x0; px < x1; px++) {
      if (px == sa) {
        px = sb - 1;
        continue;
      }
      const float cx = (float)px + 0.5f;
      const float d = sd_rbox(cx, cy, sx0, sy0, sx1, sy1, srad);
      float cov = blur > 0.0f ? half_erfc(d * inv) : clampf(0.5f - d, 0.0f, 1.0f);
      if (clip_out) {
        cov *= 1.0f - clampf(0.5f - sd_rbox(cx, cy, x, y, x + w, y + h, rad), 0.0f, 1.0f);
      }
      if (cov > 0.002f) {
        put_cov(row + px, p, cov);
      }
    }
  }
}

/* CSS inset box-shadow (GSK's inset shadow node): inside the rounded box,
 * the colour where the box moved by (dx, dy) and shrunk by the spread,
 * blurred, does not cover. */
void BUI_raster_box_shadow_inset(BuiRaster *r,
                                 float x,
                                 float y,
                                 float w,
                                 float h,
                                 const float radii[4],
                                 float dx,
                                 float dy,
                                 float spread,
                                 float blur,
                                 const float rgba[4])
{
  if (!(w > 0.0f) || !(h > 0.0f) || !rgba || !(rgba[3] > 0.0f) || r->gpu) {
    return;
  }
  float rad[4], hrad[4];
  clamp_radii(radii, w, h, rad);
  /* the hole: the box moved and shrunk by the spread */
  const float hx0 = x + dx + spread, hy0 = y + dy + spread;
  const float hx1 = x + w + dx - spread, hy1 = y + h + dy - spread;
  for (int i = 0; i < 4; i++) {
    hrad[i] = fmaxf(0.0f, rad[i] - spread);
  }
  const float sigma = fmaxf(blur * 0.5f, 0.01f);
  const float reach = blur > 0.0f ? sigma * 3.0f : 0.5f;
  int x0, y0, x1, y1;
  if (!clip_box(r, x, y, x + w, y + h, &x0, &y0, &x1, &y1)) {
    return;
  }
  const Paint p = paint_from(rgba);
  const float inv = 1.0f / (sigma * 1.41421356f);
  const float hmax = fmaxf(fmaxf(hrad[0], hrad[1]), fmaxf(hrad[2], hrad[3]));
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float cy = (float)py + 0.5f;
    /* deep inside the hole (rows clear of its corners): nothing to draw */
    int sa = x1, sb = x0;
    if (hx1 > hx0 && cy - 0.5f >= hy0 + hmax + reach && cy + 0.5f <= hy1 - hmax - reach) {
      sa = clampi((int)ceilf(hx0 + reach), x0, x1);
      sb = clampi((int)floorf(hx1 - reach), x0, x1);
      if (sb <= sa) {
        sa = x1;
        sb = x0;
      }
    }
    for (int px = x0; px < x1; px++) {
      if (px == sa) {
        px = sb - 1;
        continue;
      }
      const float cx = (float)px + 0.5f;
      const float co = clampf(0.5f - sd_rbox(cx, cy, x, y, x + w, y + h, rad), 0.0f, 1.0f);
      if (co <= 0.0f) {
        continue;
      }
      float hole = 0.0f;
      if (hx1 > hx0 && hy1 > hy0) {
        const float d = sd_rbox(cx, cy, hx0, hy0, hx1, hy1, hrad);
        hole = blur > 0.0f ? half_erfc(d * inv) : clampf(0.5f - d, 0.0f, 1.0f);
      }
      const float cov = co * (1.0f - hole);
      if (cov > 0.002f) {
        put_cov(row + px, p, cov);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Images in a rounded box
 * \{ */

void BUI_raster_image_rounded(BuiRaster *r,
                              float x,
                              float y,
                              float w,
                              float h,
                              const BuiPixmap *img,
                              const float src[4],
                              float alpha,
                              const float radii[4])
{
  float rad[4];
  clamp_radii(radii, w, h, rad);
  if (r->gpu || (rad[0] <= 0.0f && rad[1] <= 0.0f && rad[2] <= 0.0f && rad[3] <= 0.0f) ||
      BUI_pixmap_yuv(img->format))
  {
    BUI_raster_image(r, x, y, w, h, img, src, alpha, 0);
    return;
  }
  /* draw the straight parts with the plain image path (exact, fast), then
   * each corner square pixel by pixel with its coverage */
  const int saved[4] = {r->cx0, r->cy0, r->cx1, r->cy1};
  const float rt = fmaxf(rad[0], rad[1]), rb = fmaxf(rad[2], rad[3]);
  const float rl = fmaxf(rad[0], rad[3]), rr = fmaxf(rad[1], rad[2]);
  /* middle band (full width) and the top / bottom bands between the corners */
  const struct {
    float x0, y0, x1, y1;
  } bands[3] = {{x, y + rt, x + w, y + h - rb},
                {x + rl, y, x + w - rr, y + rt},
                {x + rl, y + h - rb, x + w - rr, y + h}};
  for (int i = 0; i < 3; i++) {
    const int bx0 = clampi((int)ceilf(bands[i].x0), saved[0], saved[2]);
    const int by0 = clampi((int)ceilf(bands[i].y0), saved[1], saved[3]);
    const int bx1 = clampi((int)floorf(bands[i].x1), saved[0], saved[2]);
    const int by1 = clampi((int)floorf(bands[i].y1), saved[1], saved[3]);
    if (bx1 > bx0 && by1 > by0) {
      r->cx0 = bx0;
      r->cy0 = by0;
      r->cx1 = bx1;
      r->cy1 = by1;
      BUI_raster_image(r, x, y, w, h, img, src, alpha, 0);
    }
  }
  r->cx0 = saved[0];
  r->cy0 = saved[1];
  r->cx1 = saved[2];
  r->cy1 = saved[3];
  /* the corner regions: sample the image (nearest-bilinear via a 1-pixel
   * draw into a scratch) and weight it by the rounded coverage */
  const float sx = src ? src[0] : 0.0f, sy = src ? src[1] : 0.0f;
  const float sw = src ? src[2] : (float)img->w, sh = src ? src[3] : (float)img->h;
  const float kx = sw / w, ky = sh / h;
  const struct {
    float x0, y0, x1, y1;
  } corners[4] = {{x, y, x + rl, y + rt},
                  {x + w - rr, y, x + w, y + rt},
                  {x + w - rr, y + h - rb, x + w, y + h},
                  {x, y + h - rb, x + rl, y + h}};
  int done_y0 = 0;
  (void)done_y0;
  for (int c = 0; c < 4; c++) {
    int cx0, cy0, cx1, cy1;
    if (!clip_box(r, corners[c].x0, corners[c].y0, corners[c].x1, corners[c].y1, &cx0, &cy0,
                  &cx1, &cy1))
    {
      continue;
    }
    /* the bands above own whole pixels strictly inside them; corners own
     * the rest (including partial pixels along the band edges) */
    for (int py = cy0; py < cy1; py++) {
      uint32_t *row = RASTER_ROW(r, py);
      const float fy = (float)py + 0.5f;
      for (int px = cx0; px < cx1; px++) {
        const float fx = (float)px + 0.5f;
        /* skip pixels a band drew */
        int drawn = 0;
        for (int i = 0; i < 3 && !drawn; i++) {
          drawn = px >= (int)ceilf(bands[i].x0) && px < (int)floorf(bands[i].x1) &&
                  py >= (int)ceilf(bands[i].y0) && py < (int)floorf(bands[i].y1);
        }
        if (drawn) {
          continue;
        }
        const float cov = clampf(0.5f - sd_rbox(fx, fy, x, y, x + w, y + h, rad), 0.0f, 1.0f);
        if (cov <= 0.0f) {
          continue;
        }
        const int tx = clampi((int)(sx + (fx - x) * kx), 0, img->w - 1);
        const int ty = clampi((int)(sy + (fy - y) * ky), 0, img->h - 1);
        const uint32_t ga = (uint32_t)(clampf(alpha * cov, 0.0f, 1.0f) * 256.0f + 0.5f);
        put_image_px(row + px, pixmap_texel(img, (size_t)ty * img->w + tx), ga);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linear gradients (CSS linear-gradient, top to bottom or left to right)
 * \{ */

void BUI_raster_rbox_gradient(BuiRaster *r,
                              float x,
                              float y,
                              float w,
                              float h,
                              const float radii[4],
                              const float from[4],
                              const float to[4],
                              int horizontal)
{
  if (!(w > 0.0f) || !(h > 0.0f) || r->gpu) {
    return;
  }
  float rad[4];
  clamp_radii(radii, w, h, rad);
  int x0, y0, x1, y1;
  if (!clip_box(r, x, y, x + w, y + h, &x0, &y0, &x1, &y1)) {
    return;
  }
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const float fy = (float)py + 0.5f;
    for (int px = x0; px < x1; px++) {
      const float fx = (float)px + 0.5f;
      const float cov = clampf(0.5f - sd_rbox(fx, fy, x, y, x + w, y + h, rad), 0.0f, 1.0f);
      if (cov <= 0.0f) {
        continue;
      }
      const float t = horizontal ? clampf((fx - x) / w, 0.0f, 1.0f) : clampf((fy - y) / h, 0.0f, 1.0f);
      float c[4];
      for (int k = 0; k < 4; k++) {
        c[k] = from[k] + (to[k] - from[k]) * t;
      }
      const Paint p = paint_from(c);
      put_cov(row + px, p, cov);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Coverage masks (symbolic icons)
 * \{ */

void BUI_raster_mask(BuiRaster *r,
                     int x,
                     int y,
                     const uint8_t *mask,
                     int w,
                     int h,
                     int stride,
                     const float rgba[4])
{
  if (!mask || w <= 0 || h <= 0 || r->gpu) {
    return;
  }
  const Paint p = paint_from(rgba);
  if (p.a <= 0) {
    return;
  }
  const int x0 = clampi(x, r->cx0, r->cx1), x1 = clampi(x + w, r->cx0, r->cx1);
  const int y0 = clampi(y, r->cy0, r->cy1), y1 = clampi(y + h, r->cy0, r->cy1);
  for (int py = y0; py < y1; py++) {
    uint32_t *row = RASTER_ROW(r, py);
    const uint8_t *m = mask + (size_t)(py - y) * stride - x;
    for (int px = x0; px < x1; px++) {
      const uint32_t cov = m[px];
      if (!cov) {
        continue;
      }
      const uint32_t fac = ((uint32_t)p.a * (cov + (cov >> 7))) >> 8;
      row[px] = fac >= 256 ? p.rgb : blend(row[px], p.rgb, fac);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fades (an alpha mask over what a widget draws)
 * \{ */

/* The clip-limited part of (x, y, w, h); 0 when empty or rendering on a GPU. */
static int fade_box(BuiRaster *r, int x, int y, int w, int h, int *x0, int *y0, int *x1, int *y1)
{
  if (r->gpu) {
    return 0;
  }
  *x0 = clampi(x, r->cx0, r->cx1);
  *y0 = clampi(y, r->cy0, r->cy1);
  *x1 = clampi(x + w, r->cx0, r->cx1);
  *y1 = clampi(y + h, r->cy0, r->cy1);
  return *x1 > *x0 && *y1 > *y0;
}

int BUI_raster_read(BuiRaster *r, int x, int y, int w, int h, uint32_t *dst)
{
  int x0, y0, x1, y1;
  if (!fade_box(r, x, y, w, h, &x0, &y0, &x1, &y1)) {
    return 0;
  }
  for (int py = y0; py < y1; py++) {
    memcpy(dst + (size_t)(py - y) * w + (x0 - x), r->pix + (size_t)py * r->w + x0,
           sizeof(uint32_t) * (size_t)(x1 - x0));
  }
  return 1;
}

void BUI_raster_fade(BuiRaster *r, int x, int y, int w, int h, const uint32_t *saved, float a0, float a1)
{
  int x0, y0, x1, y1;
  if (h <= 0 || !fade_box(r, x, y, w, h, &x0, &y0, &x1, &y1)) {
    return;
  }
  for (int py = y0; py < y1; py++) {
    /* the mask at the row's centre, as a linear gradient samples it */
    const float t = ((float)(py - y) + 0.5f) / (float)h;
    const uint32_t a = (uint32_t)(clampf(a0 + (a1 - a0) * t, 0.0f, 1.0f) * 256.0f + 0.5f);
    if (a >= 256) {
      continue;
    }
    uint32_t *row = r->pix + (size_t)py * r->w;
    const uint32_t *under = saved + (size_t)(py - y) * w - x;
    for (int px = x0; px < x1; px++) {
      /* under + (content - under) * a, per channel */
      const uint32_t u = under[px], c = row[px];
      const uint32_t urb = u & 0xFF00FFu, uag = (u >> 8) & 0xFF00FFu;
      const uint32_t crb = c & 0xFF00FFu, cag = (c >> 8) & 0xFF00FFu;
      const uint32_t rb = ((urb * (256 - a) + crb * a) >> 8) & 0xFF00FFu;
      const uint32_t ag = ((uag * (256 - a) + cag * a) >> 8) & 0xFF00FFu;
      row[px] = rb | ag << 8;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rounded clips (GTK's push_rounded_clip over what a widget draws)
 * \{ */

/* The square of corner `i` (TL, TR, BR, BL) of a box with radius `r`. */
static void corner_square(float x, float y, float w, float h, int i, int r, int *cx, int *cy)
{
  *cx = (i == 0 || i == 3) ? (int)floorf(x) : (int)ceilf(x + w) - r;
  *cy = (i == 0 || i == 1) ? (int)floorf(y) : (int)ceilf(y + h) - r;
}

int BUI_raster_corners_size(float w, float h, const float radii[4])
{
  float rad[4];
  clamp_radii(radii, w, h, rad);
  int n = 0;
  for (int i = 0; i < 4; i++) {
    const int r = (int)ceilf(rad[i]) + 1;
    n += rad[i] > 0.0f ? r * r : 0;
  }
  return n;
}

int BUI_raster_corners_save(BuiRaster *r, float x, float y, float w, float h, const float radii[4], uint32_t *dst)
{
  if (r->gpu) {
    return 0;
  }
  float rad[4];
  clamp_radii(radii, w, h, rad);
  for (int i = 0; i < 4; i++) {
    if (rad[i] <= 0.0f) {
      continue;
    }
    const int n = (int)ceilf(rad[i]) + 1;
    int cx, cy;
    corner_square(x, y, w, h, i, n, &cx, &cy);
    BUI_raster_read(r, cx, cy, n, n, dst);
    dst += (size_t)n * n;
  }
  return 1;
}

void BUI_raster_corners_clip(BuiRaster *r, float x, float y, float w, float h, const float radii[4],
                             const uint32_t *saved)
{
  if (r->gpu) {
    return;
  }
  float rad[4];
  clamp_radii(radii, w, h, rad);
  for (int i = 0; i < 4; i++) {
    if (rad[i] <= 0.0f) {
      continue;
    }
    const int n = (int)ceilf(rad[i]) + 1;
    int cx, cy, x0, y0, x1, y1;
    corner_square(x, y, w, h, i, n, &cx, &cy);
    if (fade_box(r, cx, cy, n, n, &x0, &y0, &x1, &y1)) {
      for (int py = y0; py < y1; py++) {
        uint32_t *row = r->pix + (size_t)py * r->w;
        const uint32_t *under = saved + (size_t)(py - cy) * n - cx;
        for (int px = x0; px < x1; px++) {
          const float cov = clampf(0.5f - sd_rbox((float)px + 0.5f, (float)py + 0.5f, x, y, x + w, y + h, rad),
                                   0.0f, 1.0f);
          const uint32_t a = (uint32_t)(cov * 256.0f + 0.5f);
          if (a >= 256) {
            continue;
          }
          const uint32_t u = under[px], c = row[px];
          const uint32_t rb = (((u & 0xFF00FFu) * (256 - a) + (c & 0xFF00FFu) * a) >> 8) & 0xFF00FFu;
          const uint32_t ag = ((((u >> 8) & 0xFF00FFu) * (256 - a) + ((c >> 8) & 0xFF00FFu) * a) >> 8) & 0xFF00FFu;
          row[px] = rb | ag << 8;
        }
      }
    }
    saved += (size_t)n * n;
  }
}

/** \} */
