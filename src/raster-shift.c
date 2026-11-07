/* The draw translation (raster-shift.h).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <math.h>
#include <stdlib.h>

float awra_shift_x, awra_shift_y;

void SR_polygon(BuiRaster *r, const float (*pts)[2], int n, const float rgba[4])
{
  if (awra_shift_x == 0.0f && awra_shift_y == 0.0f) {
    BUI_raster_polygon(r, pts, n, rgba);
    return;
  }
  float(*moved)[2] = (float(*)[2])malloc(sizeof(float[2]) * (size_t)(n > 0 ? n : 1));
  if (!moved) {
    return;
  }
  for (int i = 0; i < n; i++) {
    moved[i][0] = pts[i][0] + awra_shift_x;
    moved[i][1] = pts[i][1] + awra_shift_y;
  }
  BUI_raster_polygon(r, (const float(*)[2])moved, n, rgba);
  free(moved);
}

/* A coverage mask (a symbolic icon) at a fractional position: GTK draws
 * symbolic icons as paths there, their edges a fraction further; the
 * whole-pixel mask, filtered half way, comes closest to that. */
void SR_mask(BuiRaster *r, int x, int y, const uint8_t *mask, int w, int h, int stride, const float rgba[4])
{
  const float fx = x + awra_shift_x, fy = y + awra_shift_y;
  const int ix = (int)floorf(fx), iy = (int)floorf(fy);
  const float ax = fx - ix, ay = fy - iy;
  if (ax < 1.0f / 512 && ay < 1.0f / 512) {
    BUI_raster_mask(r, ix, iy, mask, w, h, stride, rgba);
    return;
  }
  const int nw = w + (ax > 0 ? 1 : 0), nh = h + (ay > 0 ? 1 : 0);
  uint8_t *m = (uint8_t *)malloc((size_t)nw * (size_t)nh);
  if (!m) {
    return;
  }
  for (int j = 0; j < nh; j++) {
    for (int i = 0; i < nw; i++) {
      float v = 0.0f;
      for (int dj = 0; dj < 2; dj++) {
        const int sj = j - dj;
        const float wy = dj ? ay : 1.0f - ay;
        if (sj < 0 || sj >= h || wy <= 0.0f) {
          continue;
        }
        for (int di = 0; di < 2; di++) {
          const int si = i - di;
          const float wx = di ? ax : 1.0f - ax;
          if (si < 0 || si >= w || wx <= 0.0f) {
            continue;
          }
          v += wx * wy * mask[(size_t)sj * stride + si];
        }
      }
      m[(size_t)j * nw + i] = (uint8_t)(v + 0.5f);
    }
  }
  BUI_raster_mask(r, ix, iy, m, nw, nh, nw, rgba);
  free(m);
}
