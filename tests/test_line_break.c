/* Line breaking as Pango does it: UAX #14 break opportunities with Pango's
 * tailorings (BUI_text_breaks), word boundaries, hyphens, and paragraphs
 * broken into lines (WORD, CHAR, WORD_CHAR wrapping, minimum widths), the
 * shaping context of runs (Arabic joining across spans, hyphens, letter
 * spacing). Checks that need a font for a script are skipped without one;
 * Thai dictionary breaks are checked when libthai is installed.
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

/* the attributes of `s`, at byte offsets */
static uint16_t g_attrs[512];
static const uint16_t *breaks(const char *s)
{
  BUI_text_breaks(s, (int)strlen(s), g_attrs);
  return g_attrs;
}

/* byte offsets where a line may break, as a string "3 7 12" */
static const char *line_breaks(const char *s)
{
  static char out[256];
  const uint16_t *a = breaks(s);
  const int n = (int)strlen(s);
  out[0] = 0;
  for (int i = 1; i < n; i++) {
    if (a[i] & BUI_TEXT_LINE_BREAK) {
      snprintf(out + strlen(out), sizeof(out) - strlen(out), "%s%d", out[0] ? " " : "", i);
    }
  }
  return out;
}

static float width(const char *s)
{
  float w = 0.0f;
  BUI_text_measure("", 0, k_size, s, (int)strlen(s), &w);
  return w;
}

static void check_uax14(void)
{
  /* after spaces, after hyphens before letters; not inside numbers */
  CHECK(!strcmp(line_breaks("well-known words"), "5 11"), "Latin: %s", line_breaks("well-known words"));
  CHECK(!strcmp(line_breaks("1,234.56 x"), "9"), "numbers: %s", line_breaks("1,234.56 x"));
  /* quotes and parentheses stay with their words */
  CHECK(!strcmp(line_breaks("(a) \"b\""), "4"), "punctuation: %s", line_breaks("(a) \"b\""));
  /* CJK: between ideographs, but not before 。 、 or small kana, not after 「 */
  const char *ja = "\xe6\x97\xa5\xe6\x9c\xac\xe3\x80\x82\xe3\x80\x8c\xe5\xbc\x95\xe3\x80\x8d\xe3\x81\x81";
  const uint16_t *a = breaks(ja);
  CHECK((a[3] & BUI_TEXT_LINE_BREAK) && !(a[6] & BUI_TEXT_LINE_BREAK), "日|本 but not before 。");
  CHECK((a[9] & BUI_TEXT_LINE_BREAK) && !(a[12] & BUI_TEXT_LINE_BREAK), "before 「 but not after it");
  CHECK(!(a[15] & BUI_TEXT_LINE_BREAK) && !(a[18] & BUI_TEXT_LINE_BREAK), "not before 」 or ぁ (kinsoku)");
  /* mandatory breaks */
  a = breaks("a\nb\xe2\x80\xa8" "c");
  CHECK((a[2] & BUI_TEXT_MANDATORY_BREAK) && (a[6] & BUI_TEXT_MANDATORY_BREAK), "LF and U+2028");
  /* white space, word starts and ends */
  a = breaks("ab cd");
  CHECK((a[2] & BUI_TEXT_WHITE) && (a[0] & BUI_TEXT_WORD_START) && (a[2] & BUI_TEXT_WORD_END) &&
            (a[3] & BUI_TEXT_WORD_START) && (a[5] & BUI_TEXT_WORD_END),
        "words");
  /* a soft hyphen: a break that shows a hyphen; U+2027 replaced by one */
  a = breaks("Donau\xc2\xad" "dampf");
  CHECK((a[7] & BUI_TEXT_LINE_BREAK) && (a[7] & BUI_TEXT_INSERTS_HYPHEN), "soft hyphen");
  a = breaks("ab\xe2\x80\xa7" "cd");
  CHECK((a[5] & BUI_TEXT_INSERTS_HYPHEN) && (a[5] & BUI_TEXT_REMOVES_PRECEDING), "hyphenation point");
  /* char breaks between graphemes only */
  a = breaks("e\xcc\x81x");
  CHECK((a[3] & BUI_TEXT_CHAR_BREAK) && !(a[1] & BUI_TEXT_CHAR_BREAK), "char breaks");
  /* Thai: one word without libthai (Pango's fallback), its words with it */
  const char *th = "\xe0\xb8\xa0\xe0\xb8\xb2\xe0\xb8\xa9\xe0\xb8\xb2\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2"; /* ภาษาไทย */
  a = breaks(th);
  bool thai_words = (a[12] & BUI_TEXT_LINE_BREAK) != 0;
  int inside = 0;
  for (int i = 1; i < (int)strlen(th); i++) {
    inside += (a[i] & BUI_TEXT_LINE_BREAK) != 0;
  }
  CHECK(thai_words ? inside == 1 : inside == 0, "Thai breaks: %d inside (libthai %d)", inside, thai_words);
  fprintf(stderr, "Thai words: %s\n", thai_words ? "libthai" : "none (no libthai)");
  /* the tailored breaks follow the items of a font: no Thai word break
   * between Latin digits and Thai drawn by another font */
  uint16_t fa[64];
  const char *mixed = "34\xe0\xb8\xa0\xe0\xb8\xb2\xe0\xb8\xa9\xe0\xb8\xb2";
  BUI_text_breaks_font("", 0, k_size, mixed, (int)strlen(mixed), fa);
  CHECK(!(fa[2] & BUI_TEXT_LINE_BREAK), "34|ภาษา is no break in PangoLayout");
}

static AwraTextLayout *wrapped(const char *text, AwraWrap mode)
{
  AwraTextLayout *l = awra_text_layout_new();
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.family = "";
  f.size = k_size;
  f.letter_spacing = 0;
  awra_text_layout_set_font(l, &f);
  awra_text_layout_set_text(l, text, false);
  awra_text_layout_set_wrap(l, mode, AWRA_ELLIPSIZE_NONE, 0);
  return l;
}

static int lines_of(AwraTextLayout *l, int width)
{
  int w, h;
  awra_text_layout_size(l, width, &w, &h);
  const int lh = (int)ceilf(awra_font_line_height(&(AwraFont){"", k_size, 400, false, 0}));
  return lh > 0 ? (h + lh / 2) / lh : 0;
}

static void check_wrapping(void)
{
  const char *text = "alpha beta gamma";
  AwraTextLayout *l = wrapped(text, AWRA_WRAP_WORD);
  const int word = (int)ceilf(width("gamma"));
  /* the minimum width is the widest word (GtkLabel lays out at width 0) */
  CHECK(awra_text_layout_min_width(l) == word, "min %d, widest word %d", awra_text_layout_min_width(l), word);
  CHECK(lines_of(l, word) == 3, "three lines at the widest word's width: %d", lines_of(l, word));
  CHECK(lines_of(l, 100000) == 1, "one line when it fits");
  /* the space a line breaks after takes no width */
  int w;
  awra_text_layout_size(l, (int)ceilf(width("alpha beta")), &w, NULL);
  CHECK(w == (int)ceilf(width("alpha beta")), "trailing space not counted: %d", w);
  awra_text_layout_free(l);
  /* a word longer than the line: WORD overflows, WORD_CHAR and CHAR break it */
  const char *longw = "Supercalifragilistic ok";
  l = wrapped(longw, AWRA_WRAP_WORD);
  awra_text_layout_size(l, 40, &w, NULL);
  CHECK(w > 40, "WORD overflows: %d", w);
  awra_text_layout_free(l);
  l = wrapped(longw, AWRA_WRAP_WORD_CHAR);
  awra_text_layout_size(l, 40, &w, NULL);
  CHECK(w <= 40 && lines_of(l, 40) > 2, "WORD_CHAR breaks the word: %d px, %d lines", w, lines_of(l, 40));
  CHECK(awra_text_layout_min_width(l) < 20, "WORD_CHAR's minimum is a character: %d",
        awra_text_layout_min_width(l));
  awra_text_layout_free(l);
  l = wrapped("ab cd", AWRA_WRAP_CHAR);
  CHECK(lines_of(l, (int)ceilf(width("ab c"))) == 2, "CHAR breaks between letters");
  awra_text_layout_free(l);
  /* a soft hyphen shows a hyphen where the line breaks */
  l = wrapped("Donau\xc2\xad" "dampfschiff", AWRA_WRAP_WORD);
  awra_text_layout_size(l, (int)ceilf(width("Donau-")), &w, NULL);
  CHECK(lines_of(l, (int)ceilf(width("Donau-"))) == 2 && w >= (int)width("Donau") + 2,
        "hyphenated line: %d lines, %d px", lines_of(l, (int)ceilf(width("Donau-"))), w);
  awra_text_layout_free(l);
  /* a line separator ends a line */
  l = wrapped("a\xe2\x80\xa8" "b", AWRA_WRAP_NONE);
  CHECK(lines_of(l, -1) == 2, "U+2028: %d lines", lines_of(l, -1));
  awra_text_layout_free(l);
}

static void check_context(bool arabic)
{
  /* letter spacing: between graphemes; ligatures off */
  BuiTextContext c = {0};
  c.letter_spacing = 2 * 1024;
  float plain = width("abc"), spaced;
  BUI_text_set_context(&c);
  spaced = width("abc");
  BUI_text_set_context(NULL);
  CHECK(fabsf(spaced - plain - 6.0f) < 0.01f, "letter spacing: %.2f -> %.2f", plain, spaced);
  /* a hyphen at the end */
  c = (BuiTextContext){0};
  c.hyphen = 1;
  BUI_text_set_context(&c);
  const float hy = width("ab");
  BUI_text_set_context(NULL);
  CHECK(hy > width("ab"), "hyphen added");
  /* the context is part of a run's identity: back to plain without it */
  CHECK(width("abc") == plain, "plain run unchanged");
  if (!arabic) {
    return;
  }
  /* beh before another letter joins (initial form, narrower) */
  const char *beh = "\xd8\xa8";
  const float alone = width(beh);
  c = (BuiTextContext){0};
  c.after = beh;
  c.after_len = 2;
  BUI_text_set_context(&c);
  const float joined = width(beh);
  BUI_text_set_context(NULL);
  CHECK(joined < alone, "Arabic joins across the context: %.1f vs %.1f", joined, alone);
  /* in a layout: a span inside an Arabic word keeps it joined */
  AwraTextLayout *l = wrapped("\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7", AWRA_WRAP_NONE);
  const int whole = awra_text_layout_natural_width(l);
  awra_text_layout_set_text(l, "\xd9\x85\xd8\xb1<span foreground=\"red\">\xd8\xad</span>\xd8\xa8\xd8\xa7", true);
  CHECK(awra_text_layout_natural_width(l) == whole, "span inside a word: %d vs %d",
        awra_text_layout_natural_width(l), whole);
  awra_text_layout_free(l);
}

int main(void)
{
  awra_init("org.awra.TestLineBreak");
  const bool arabic = width("\xd8\xa7") != width("\xd9\x85");
  if (!arabic) {
    fprintf(stderr, "skip: no Arabic font\n");
  }
  check_uax14();
  check_wrapping();
  check_context(arabic);
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  return 0;
}
