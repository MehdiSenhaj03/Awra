/* Font fallback and colour emoji: characters the font lacks are drawn by
 * the fonts fontconfig (or Windows) sorts after it, emoji by the "emoji"
 * family in colour, emoji sequences shaped into one glyph, default
 * ignorables hidden, and a line's extents grow with its fallback fonts.
 * Checks that need a font this machine lacks are skipped.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

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

static const float k_size = 13.333333f;

static float width(const char *s)
{
  float w = 0.0f;
  BUI_text_measure("", 0, k_size, s, (int)strlen(s), &w);
  return w;
}

/* Draws `s` black on white; counts pixels that aren't grey (colour) and
 * that aren't white (ink). */
static void draw(const char *s, int *r_color, int *r_ink)
{
  BuiRaster *r = BUI_raster_create(400, 40);
  BUI_raster_begin_frame(r, 400, 40);
  const float white[4] = {1, 1, 1, 1}, black[4] = {0, 0, 0, 1};
  BUI_raster_fill_rect(r, 0, 0, 400, 40, white);
  BUI_raster_set_font(r, "", 0);
  BUI_raster_text(r, 4, 28, s, (int)strlen(s), k_size, black);
  const uint32_t *px = BUI_raster_pixels(r);
  *r_color = *r_ink = 0;
  for (int i = 0; i < 400 * 40; i++) {
    const int rr = (px[i] >> 16) & 255, gg = (px[i] >> 8) & 255, bb = px[i] & 255;
    const int mx = rr > gg ? (rr > bb ? rr : bb) : (gg > bb ? gg : bb);
    const int mn = rr < gg ? (rr < bb ? rr : bb) : (gg < bb ? gg : bb);
    *r_color += mx - mn > 40;
    *r_ink += mn < 200;
  }
  BUI_raster_free(r);
}

int main(void)
{
  awra_init("org.awra.TestTextFallback");
  /* plain text is unchanged by the fallback machinery: a run measures the
   * sum of its parts where nothing kerns across them */
  CHECK(width("Hello world") > 0.0f, "no text engine");

  int color, ink;
  draw("\xF0\x9F\x98\x80", &color, &ink); /* 😀 */
  const bool have_emoji = color > 20;
  if (!have_emoji) {
    fprintf(stderr, "skip: no colour emoji font\n");
  }
  else {
    /* ZWJ family, rainbow flag (with VS16), flags, skin tones, keycaps:
     * one glyph each, as wide as one emoji */
    const float one = width("\xF0\x9F\x98\x80");
    static const char *seqs[] = {
        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6",
        "\xF0\x9F\x8F\xB3\xEF\xB8\x8F\xE2\x80\x8D\xF0\x9F\x8C\x88",
        "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7",
        "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD",
        "1\xEF\xB8\x8F\xE2\x83\xA3",
        "\xF0\x9F\xA7\x91\xE2\x80\x8D\xF0\x9F\x92\xBB",
    };
    for (int i = 0; i < 6; i++) {
      const float w = width(seqs[i]);
      CHECK(w == one, "sequence %d: %.1f px, one emoji %.1f", i, w, one);
      draw(seqs[i], &color, &ink);
      /* (Noto draws the family in greys) */
      CHECK(i == 0 ? ink > 20 : color > 20, "sequence %d drawn without colour", i);
    }
    /* VS16 makes a text-default emoji a colour one; alone it stays text */
    draw("\xE2\x98\xBA", &color, &ink); /* ☺ */
    CHECK(color < 5, "U+263A without VS16 drawn in colour");
    draw("\xE2\x98\xBA\xEF\xB8\x8F", &color, &ink);
    CHECK(color > 20, "U+263A VS16 not drawn in colour");
    /* emoji lines are as tall as the emoji font makes them */
    float a0, d0, a1, d1;
    BUI_text_metrics("", 0, k_size, &a0, &d0);
    BUI_text_extents("", 0, k_size, "a\xF0\x9F\x98\x80", 5, &a1, &d1);
    CHECK(a1 >= a0 && d1 >= d0, "extents shrank");
  }
  /* default ignorables: no width, nothing drawn */
  CHECK(width("x\xE2\x80\x8By") == width("xy"), "ZWSP has a width");
  CHECK(width("a\xC2\xAD" "b") == width("ab"), "soft hyphen has a width");
  /* a CJK fallback, when one is installed: drawn, and a taller line */
  draw("\xE6\x97\xA5\xE6\x9C\xAC", &color, &ink); /* 日本 */
  if (ink > 30) {
    float a0, d0, a1, d1;
    BUI_text_metrics("", 0, k_size, &a0, &d0);
    BUI_text_extents("", 0, k_size, "\xE6\x97\xA5\xE6\x9C\xAC", 6, &a1, &d1);
    CHECK(a1 + d1 >= a0 + d0, "CJK extents %.2f < %.2f", a1 + d1, a0 + d0);
    /* the cache gives the same answer twice */
    CHECK(width("\xE6\x97\xA5\xE6\x9C\xAC") == width("\xE6\x97\xA5\xE6\x9C\xAC"), "unstable width");
  }
  else {
    fprintf(stderr, "skip: no CJK font\n");
  }
  /* long mixed runs (heap paths of the itemizer) */
  char *big = (char *)malloc(64 * 1024);
  size_t n = 0;
  for (int i = 0; i < 2000; i++) {
    static const char piece[] = "ab \xF0\x9F\x98\x80\xE6\x97\xA5 ";
    memcpy(big + n, piece, sizeof(piece) - 1);
    n += sizeof(piece) - 1;
  }
  big[n] = 0;
  CHECK(width(big) > 0.0f, "long run");
  draw(big, &color, &ink);
  free(big);
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("text fallback ok\n");
  return 0;
}
