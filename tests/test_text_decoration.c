/* Links and text decorations as Pango draws them: a link nested in a span
 * takes the link's style (the innermost span wins, though both start
 * together), and underlines and strikethroughs are the font's own
 * rectangles (post and OS/2, as pangofc-font.c reads them through
 * HarfBuzz), their top that far from the line's unrounded baseline.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include "GPU_raster.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Draws `markup` black on white at (4, 4) into `out`. */
#define W 300
#define H 40
static void draw(const char *markup, AwraTextLayout **r_l, uint32_t *out)
{
  BuiRaster *r = BUI_raster_create(W, H);
  BUI_raster_begin_frame(r, W, H);
  const float white[4] = {1, 1, 1, 1};
  BUI_raster_fill_rect(r, 0, 0, W, H, white);
  AwraTextLayout *l = awra_text_layout_new();
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  awra_text_layout_set_font(l, &f);
  awra_text_layout_set_text(l, markup, true);
  AwraDraw d = {r, 1.0f};
  const AwraRGBA black = {0, 0, 0, 1};
  awra_text_layout_draw(l, &d, 4, 4, -1, &black);
  memcpy(out, BUI_raster_pixels(r), sizeof(uint32_t) * W * H);
  BUI_raster_free(r);
  *r_l = l;
}

static float dark(uint32_t p)
{
  /* any channel order: white is 255 everywhere */
  const int a = (int)(p & 255), b = (int)((p >> 8) & 255), c = (int)((p >> 16) & 255);
  return 1.0f - (float)(a + b + c) / 765.0f;
}

int main(void)
{
  awra_init("org.awra.TestTextDecoration");

  /* a link inside a span is a link */
  {
    AwraTextLayout *l;
    static uint32_t px[W * H];
    draw("<span weight=\"600\"><a href=\"x\">link</a></span>", &l, px);
    CHECK(awra_text_layout_n_links(l) == 1, "links: %d", awra_text_layout_n_links(l));
    CHECK(awra_text_layout_link_at(l, -1, 6, 8) == 0, "no link under the text");
    awra_text_layout_free(l);
  }

  /* the underline: the font's thickness, its top `-position` under the
   * unrounded baseline (Inter / DejaVu: under the glyphs, not in them) */
  {
    AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    float up, ut, sp, st;
    const int own = BUI_text_decoration_metrics(f.family, 0, f.size, &up, &ut, &sp, &st);
    CHECK(own, "the body font has no metrics");
    CHECK(up < 0.0f && ut > 0.0f && ut < 3.0f, "underline %.3f %.3f", up, ut);
    CHECK(sp > 0.0f && st > 0.0f && st < 3.0f, "strikethrough %.3f %.3f", sp, st);

    AwraTextLayout *l;
    static uint32_t under[W * H];
    draw("<u>mmmm</u>", &l, under);
    const float base = 4.0f + (float)awra_text_layout_baseline(l);
    awra_text_layout_free(l);
    /* the column between the first two m's stems has only the underline */
    float cov[H] = {0};
    for (int y = 0; y < H; y++) {
      float best = 0.0f;
      for (int x = 6; x < 30; x++) {
        best = fmaxf(best, dark(under[y * W + x]));
      }
      cov[y] = best;
    }
    int first = -1;
    for (int y = (int)base + 1; y < H; y++) {
      if (cov[y] > 0.02f) {
        first = y;
        break;
      }
    }
    CHECK(first >= (int)floorf(base - up) && first <= (int)ceilf(base - up),
          "underline starts at %d, want %.2f", first, base - up);
  }

  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("text decoration: ok\n");
  return 0;
}
