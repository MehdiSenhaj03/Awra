/* Textures encoded as PNG (clipboard images, Texture::save_to_png) decode
 * back to the same pixels, compressed.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>

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

static void roundtrip(int w, int h, int kind)
{
  uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
  uint32_t seed = 12345;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint8_t *p = rgba + ((size_t)y * w + x) * 4;
      seed = seed * 1103515245u + 12345u;
      if (kind == 0) { /* a UI-like picture: flat areas, a gradient, text-like noise */
        p[0] = (uint8_t)(x * 255 / (w > 1 ? w - 1 : 1));
        p[1] = (uint8_t)(y < h / 2 ? 40 : 200);
        p[2] = (uint8_t)((x / 8 + y / 8) & 1 ? 230 : 20);
        p[3] = (uint8_t)(x < w / 4 ? 128 : 255);
      }
      else { /* noise: hardly compressible, with transparent pixels */
        p[0] = (uint8_t)(seed >> 24);
        p[1] = (uint8_t)(seed >> 16);
        p[2] = (uint8_t)(seed >> 8);
        p[3] = (uint8_t)((seed >> 4) & 1 ? 255 : 0);
      }
    }
  }
  AwraTexture *t = awra_texture_new_rgba(rgba, w, h, w * 4);
  size_t len = 0;
  void *png = awra_texture_save_png_bytes(t, &len);
  CHECK(png && len > 8, "no PNG for %dx%d", w, h);
  AwraTexture *back = png ? awra_texture_new_from_bytes(png, len) : NULL;
  CHECK(back, "the PNG doesn't decode (%dx%d, kind %d)", w, h, kind);
  if (back) {
    CHECK(awra_texture_width(back) == w && awra_texture_height(back) == h, "size");
    uint8_t *a = (uint8_t *)malloc((size_t)w * h * 4), *b = (uint8_t *)malloc((size_t)w * h * 4);
    awra_texture_download(t, a, w * 4);
    awra_texture_download(back, b, w * 4);
    int worst = 0;
    for (size_t i = 0; i < (size_t)w * h * 4; i++) {
      const int d = abs((int)a[i] - (int)b[i]);
      worst = d > worst ? d : worst;
    }
    CHECK(worst == 0, "pixels differ by %d (%dx%d kind %d)", worst, w, h, kind);
    free(a);
    free(b);
    awra_texture_unref(back);
  }
  if (kind == 0 && w * h > 1000) {
    CHECK(len < (size_t)w * h, "PNG of %zu bytes for %d raw (not compressed)", len, w * h * 4);
  }
  free(png);
  awra_texture_unref(t);
  free(rgba);
}

int main(void)
{
  awra_init("org.awra.TestPng");
  roundtrip(1, 1, 0);
  roundtrip(3, 2, 1);
  roundtrip(300, 200, 0);
  roundtrip(257, 129, 1);
  roundtrip(1920, 64, 0);
  if (failures) {
    return 1;
  }
  printf("png encode ok\n");
  return 0;
}
