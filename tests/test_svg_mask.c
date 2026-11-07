/* SVG masks (luminance) and clip paths in the icon rasterizer: Dawra's tray
 * logo cuts a hole for its unread dot with a mask.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>

BKImBuf *imb_svg_rasterize(const char *svg, int want_w, int want_h);

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

static uint32_t px(BKImBuf *ib, int x, int y)
{
  int w, h;
  BK_imbuf_size(ib, &w, &h);
  return BK_imbuf_pixels(ib, 0)[y * w + x];
}

#define ALPHA(c) ((int)((c) >> 24))
#define RED(c) ((int)(((c) >> 16) & 255))
#define GREEN(c) ((int)(((c) >> 8) & 255))
#define BLUE(c) ((int)((c) & 255))

int main(void)
{
  awra_init("org.awra.TestSvgMask");
  /* a white square through a mask that is white but for a black circle in
   * its middle: a hole there; a red dot drawn over the hole unmasked */
  static const char *hole =
      "<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64' viewBox='0 0 64 64'>"
      "<mask id='m'><rect width='64' height='64' fill='#fff'/><circle cx='32' cy='32' r='12'/></mask>"
      "<g mask='url(#m)'><rect width='64' height='64' fill='#fff'/></g>"
      "<circle cx='56' cy='8' r='4' fill='#f00'/></svg>";
  BKImBuf *ib = imb_svg_rasterize(hole, 64, 64);
  CHECK(ib != NULL, "no image");
  if (ib) {
    CHECK(ALPHA(px(ib, 32, 32)) == 0, "the masked hole is drawn: %08x", px(ib, 32, 32));
    CHECK(ALPHA(px(ib, 4, 4)) == 255, "outside the hole: %08x", px(ib, 4, 4));
    CHECK(RED(px(ib, 56, 8)) > 200 && GREEN(px(ib, 56, 8)) < 40, "the dot: %08x", px(ib, 56, 8));
    BK_imbuf_free(ib);
  }
  /* a clip path (in <defs>), a mask with a gradient in objectBoundingBox
   * units (x2="1": the box's right edge), and a reference to nothing */
  static const char *clip =
      "<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64' viewBox='0 0 64 64'><defs>"
      "<clipPath id='c'><rect x='0' y='0' width='32' height='32'/></clipPath>"
      "<linearGradient id='g' x1='0' y1='0' x2='1' y2='0'>"
      "<stop offset='0' stop-color='#fff'/><stop offset='1' stop-color='#000'/></linearGradient>"
      "<mask id='m'><rect width='64' height='64' fill='url(#g)'/></mask></defs>"
      "<rect width='64' height='32' fill='#0f0' clip-path='url(#c)'/>"
      "<rect y='40' width='64' height='16' fill='#00f' mask='url(#m)'/>"
      "<rect y='60' width='64' height='4' fill='#f00' mask='url(#missing)'/></svg>";
  ib = imb_svg_rasterize(clip, 64, 64);
  CHECK(ib != NULL, "no image");
  if (ib) {
    CHECK(ALPHA(px(ib, 8, 8)) == 255 && GREEN(px(ib, 8, 8)) > 200, "inside the clip: %08x", px(ib, 8, 8));
    CHECK(ALPHA(px(ib, 48, 8)) == 0, "outside the clip: %08x", px(ib, 48, 8));
    const int left = ALPHA(px(ib, 4, 48)), mid = ALPHA(px(ib, 32, 48)), right = ALPHA(px(ib, 60, 48));
    CHECK(left > 220 && mid > 90 && mid < 170 && right < 30, "gradient mask %d %d %d", left, mid, right);
    CHECK(ALPHA(px(ib, 32, 62)) == 255, "a missing mask hides: %08x", px(ib, 32, 62));
    BK_imbuf_free(ib);
  }
  if (failures) {
    return 1;
  }
  printf("svg masks ok\n");
  return 0;
}
