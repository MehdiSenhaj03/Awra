/* Images filtered as GSK filters textures: GSK_SCALING_FILTER_TRILINEAR
 * (BUI_IMAGE_TRILINEAR) mixes the two mipmap levels around the scale,
 * BUI_IMAGE_MIPMAP_AUTO does so only under half size (a texture node),
 * bilinear otherwise. A fine checkerboard drawn at a quarter of its size
 * averages to grey with mipmaps, and aliases without.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "GPU_raster.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(cond, ...)                                                   \
  do {                                                                     \
    if (!(cond)) {                                                         \
      failures++;                                                          \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                 \
      fprintf(stderr, __VA_ARGS__);                                        \
      fputc('\n', stderr);                                                 \
    }                                                                      \
  } while (0)

#define N 64

/* the spread of the green channel over the drawn square */
static int spread(int size, int flags, int *r_mean)
{
  static uint32_t px[N * N];
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      px[y * N + x] = ((x ^ y) & 1) ? 0xFFFFFFFFu : 0xFF000000u;
    }
  }
  const BuiPixmap pm = {px, N, N, 0x1234, BUI_PIXMAP_ARGB, 0};
  BuiRaster *r = BUI_raster_create(N, N);
  BUI_raster_begin_frame(r, N, N);
  const float red[4] = {1, 0, 0, 1};
  BUI_raster_fill_rect(r, 0, 0, N, N, red);
  BUI_raster_image(r, 0, 0, (float)size, (float)size, &pm, NULL, 1.0f, flags);
  const uint32_t *out = BUI_raster_pixels(r);
  int lo = 255, hi = 0, sum = 0;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      const int g = (int)((out[y * N + x] >> 8) & 255);
      lo = g < lo ? g : lo;
      hi = g > hi ? g : hi;
      sum += g;
    }
  }
  BUI_raster_free(r);
  *r_mean = sum / (size * size);
  return hi - lo;
}

int main(void)
{
  int mean;
  /* a quarter: level 2 (2x2 of the 4x4 average) */
  CHECK(spread(16, BUI_IMAGE_TRILINEAR, &mean) <= 2, "trilinear at 1/4 not flat");
  CHECK(mean >= 125 && mean <= 130, "trilinear at 1/4: mean %d", mean);
  CHECK(spread(16, BUI_IMAGE_MIPMAP_AUTO, &mean) <= 2, "auto at 1/4 not flat");
  /* without mipmaps, bilinear sees single texels (a fifth: beats) */
  CHECK(spread(13, 0, &mean) > 64, "bilinear at 1/5 is flat");
  CHECK(spread(13, BUI_IMAGE_TRILINEAR, &mean) <= 8, "trilinear at 1/5 not flat");
  /* auto at 3/4 is bilinear: the same as no flag */
  int m2;
  CHECK(spread(48, BUI_IMAGE_MIPMAP_AUTO, &mean) == spread(48, 0, &m2) && mean == m2,
        "auto above half size is not bilinear");
  /* trilinear at 3/4: between level 0 and level 1 (flat grey) */
  const int s3 = spread(48, BUI_IMAGE_TRILINEAR, &mean);
  CHECK(s3 < spread(48, 0, &m2), "trilinear at 3/4 not smoother than bilinear");
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("image filters: ok\n");
  return 0;
}
