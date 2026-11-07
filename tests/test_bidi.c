/* Bidi, complex-script shaping and bidi editing, as Pango and GTK do them:
 * embedding levels and paragraph directions (UAX #9), grapheme boundaries
 * and GTK's backspace, Arabic joining and lam-alef, visual order and caret
 * geometry of runs, and the layout's strong cursor, visual Left / Right
 * moves and hit-testing in mixed paragraphs. Checks that need an Arabic or
 * Hebrew font are skipped when the machine has none.
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

static float width(const char *s, int flags)
{
  float w = 0.0f;
  BUI_text_measure("", flags, k_size, s, (int)strlen(s), &w);
  return w;
}

static float index_x(const char *s, int index, int trailing)
{
  float x = 0.0f;
  BUI_text_index_to_x("", 0, k_size, s, (int)strlen(s), index, trailing, &x);
  return x;
}

/* Hebrew / Arabic: an alef and a mem of different widths (a missing font
 * draws both as the same box) */
#define HE_ALEF "\xd7\x90"
#define HE_BET "\xd7\x91"
#define HE_SHALOM "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d"
#define AR_ALEF "\xd8\xa7"
#define AR_BEH "\xd8\xa8"
#define AR_LAM "\xd9\x84"
#define AR_MEEM "\xd9\x85"
#define AR_KASRA "\xd9\x90"

static void check_bidi_levels(void)
{
  /* "abc שלום 123": abc 0, Hebrew 1, the space before the digits 1 (they
   * act as R), the digits 2 */
  const char *t = "abc " HE_SHALOM " 123";
  uint8_t lv[64];
  const int n = (int)strlen(t);
  CHECK(BUI_text_bidi(t, n, 0, lv) == 0, "LTR paragraph");
  CHECK(lv[0] == 0 && lv[4] == 1 && lv[12] == 1 && lv[13] == 2, "levels %d %d %d %d", lv[0], lv[4],
        lv[12], lv[13]);
  /* the first strong character sets the paragraph's direction */
  const char *r = HE_SHALOM " abc";
  CHECK(BUI_text_bidi(r, (int)strlen(r), 0, lv) == 1, "RTL paragraph");
  CHECK(lv[0] == 1 && lv[9] == 2, "RTL levels %d %d", lv[0], lv[9]);
  CHECK(BUI_text_direction("123 abc", 7) == 1, "LTR first strong");
  CHECK(BUI_text_direction("123 " HE_ALEF, 6) == 2, "RTL first strong");
  CHECK(BUI_text_direction("123 ...", 7) == 0, "no strong character");
  /* neutral text: the base given */
  CHECK(BUI_text_bidi("123", 3, 3, lv) == 1, "neutral paragraph, RTL context");
}

static void check_graphemes(void)
{
  uint8_t a[64];
  /* e + combining acute: one grapheme, backspace takes it whole (Latin) */
  BUI_text_breaks("e\xcc\x81x", 4, a);
  CHECK((a[0] & BUI_TEXT_CURSOR) && !(a[1] & BUI_TEXT_CURSOR) && (a[3] & BUI_TEXT_CURSOR) &&
            (a[4] & BUI_TEXT_CURSOR),
        "e + U+0301");
  CHECK(!(a[3] & BUI_TEXT_BACKSPACE_CHAR), "backspace deletes a Latin grapheme whole");
  /* Arabic letter + kasra: backspace deletes the mark alone */
  BUI_text_breaks(AR_BEH AR_KASRA, 4, a);
  CHECK(!(a[2] & BUI_TEXT_CURSOR) && (a[4] & BUI_TEXT_BACKSPACE_CHAR), "beh + kasra");
  /* Devanagari ka + vowel sign i */
  BUI_text_breaks("\xe0\xa4\x95\xe0\xa4\xbf", 6, a);
  CHECK(!(a[3] & BUI_TEXT_CURSOR) && (a[6] & BUI_TEXT_BACKSPACE_CHAR), "ka + i");
  /* a ZWJ family and a flag: one grapheme each */
  const char *fam = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
  BUI_text_breaks(fam, (int)strlen(fam), a);
  int inner = 0;
  for (int i = 1; i < (int)strlen(fam); i++) {
    inner += (a[i] & BUI_TEXT_CURSOR) != 0;
  }
  CHECK(inner == 0, "ZWJ sequence split %d times", inner);
  BUI_text_breaks("\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA", 16, a);
  CHECK(!(a[4] & BUI_TEXT_CURSOR) && (a[8] & BUI_TEXT_CURSOR) && !(a[12] & BUI_TEXT_CURSOR),
        "regional indicator pairs");
  /* NFD: a Hangul syllable into its jamo, an accented letter */
  char out[32];
  CHECK(BUI_text_nfd("\xed\x95\x9c", 3, out, 32) == 9 && !memcmp(out, "\xe1\x84\x92\xe1\x85\xa1\xe1\x86\xab", 9),
        "NFD of U+D55C");
  CHECK(BUI_text_nfd("\xc3\xa9", 2, out, 32) == 3 && !memcmp(out, "e\xcc\x81", 3), "NFD of U+00E9");
}

/* GTK's backspace and delete in a text edit */
static void check_backspace(void)
{
  static const struct {
    const char *text, *after;
    const char *key;
  } cases[] = {
      {AR_BEH AR_KASRA, AR_BEH, "BackSpace"},                    /* the mark only */
      {"\xd8\xa2", AR_ALEF, "BackSpace"},                      /* alef madda -> alef (NFD) */
      {"\xed\x95\x9c", "", "BackSpace"},                        /* a Hangul syllable whole */
      {"\xe0\xa4\x95\xe0\xa4\xbf", "\xe0\xa4\x95", "BackSpace"}, /* कि -> क */
      {"a\xc3\xa9", "a", "BackSpace"},                           /* é whole */
      {"ae\xcc\x81", "a", "BackSpace"},                          /* e + acute whole */
      {"\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7", "", "BackSpace"},       /* a flag whole */
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    BKTextEdit *te = bk_te_new(cases[i].text, false);
    te->caret = te->len;
    te->anchor = -1;
    bk_te_key(te, NULL, NULL, cases[i].key, 0);
    CHECK(strcmp(te->text, cases[i].after) == 0 && te->caret == (int)strlen(cases[i].after),
          "backspace case %d: caret %d", (int)i, te->caret);
    bk_te_free(te);
  }
  /* delete and arrows move by graphemes */
  BKTextEdit *te = bk_te_new("e\xcc\x81x", false);
  te->caret = 0;
  te->anchor = -1;
  bk_te_key(te, NULL, NULL, "Right", 0);
  CHECK(te->caret == 3, "Right over e + acute: %d", te->caret);
  bk_te_key(te, NULL, NULL, "Left", 0);
  bk_te_key(te, NULL, NULL, "Delete", 0);
  CHECK(strcmp(te->text, "x") == 0, "Delete of e + acute");
  bk_te_free(te);
}

static void check_shaping(bool arabic, bool hebrew)
{
  if (arabic) {
    /* lam-alef is one ligature, narrower than its letters apart */
    const float lamalef = width(AR_LAM AR_ALEF, 0);
    CHECK(lamalef > 0.0f && lamalef < width(AR_LAM, 0) + width(AR_ALEF, 0), "lam-alef %.1f", lamalef);
    /* letters join: initial + final forms, not two isolated ones */
    CHECK(width(AR_BEH AR_BEH, 0) != 2.0f * width(AR_BEH, 0), "beh beh joined");
    /* a run of one level never reorders, and an RTL run lays out right to left */
    const char *w = AR_BEH AR_MEEM;
    float x0 = 0.0f, x1 = 0.0f;
    BUI_text_index_to_x("", BUI_TEXT_RTL, k_size, w, 4, 0, 0, &x0);
    BUI_text_index_to_x("", BUI_TEXT_RTL, k_size, w, 4, 4, 0, &x1);
    CHECK(x0 > x1 && x1 == 0.0f, "RTL run: start %.1f end %.1f", x0, x1);
  }
  if (hebrew) {
    /* "abc אב": the Hebrew is reversed at the right */
    const char *t = "abc " HE_ALEF HE_BET;
    const float w = width(t, 0);
    CHECK(index_x(t, 4, 0) == w, "alef's leading edge %.1f, width %.1f", index_x(t, 4, 0), w);
    CHECK(index_x(t, 6, 1) == width("abc ", 0), "bet's trailing edge");
    /* hit-testing: the right end is the alef, its leading half */
    int idx = -1, tr = -1;
    BUI_text_x_to_index("", 0, k_size, t, (int)strlen(t), w - 1.0f, &idx, &tr);
    CHECK(idx == 4 && tr == 0, "hit at the right end: %d + %d", idx, tr);
  }
}

/* the layout: Pango's strong cursor, visual moves and hit-testing */
static void check_layout(bool arabic, bool hebrew)
{
  if (!hebrew || !arabic) {
    return;
  }
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.family = "";
  AwraTextLayout *l = awra_text_layout_new();
  awra_text_layout_set_font(l, &f);
  /* an RTL paragraph: its start is at the right, its end at the left */
  awra_text_layout_set_text(l, HE_SHALOM " abc", false);
  int w = 0;
  awra_text_layout_size(l, -1, &w, NULL);
  float s, k, y, h;
  awra_text_layout_cursor_pos(l, -1, 0, &s, &k, &y, &h);
  CHECK(fabsf(s - (float)w) <= 1.0f, "RTL start at %.1f of %d", s, w);
  awra_text_layout_cursor_pos(l, -1, 12, &s, &k, &y, &h);
  CHECK(s == 0.0f, "RTL end at %.1f", s);
  /* left of an RTL line: its logical end */
  CHECK(awra_text_layout_pos_to_index(l, -1, -5.0f, 2.0f) == 12, "left of RTL line");
  CHECK(awra_text_layout_pos_to_index(l, -1, (float)w + 5.0f, 2.0f) == 0, "right of RTL line");
  /* visual moves, as Pango makes them (pango_layout_move_cursor_visually),
   * through an LTR paragraph with Arabic digits into an RTL one */
  awra_text_layout_set_text(l, "abc \xd9\xa1\xd9\xa2\xd9\xa3 def\n(" AR_MEEM "\xd8\xb1\xd8\xad" AR_BEH AR_ALEF ") [abc]", false);
  static const int right[] = {1, 2, 3, 4, 6, 8, 10, 11, 12, 13, 14, 33, 32, 30, 31, 29, 28, 27, 26, 24, 22, 20, 18, 16, 15, 14};
  int idx = 0;
  for (size_t i = 0; i < sizeof(right) / sizeof(right[0]); i++) {
    idx = awra_text_layout_move_visually(l, -1, idx, 1);
    CHECK(idx == right[i], "move right %d: %d, Pango %d", (int)i, idx, right[i]);
    if (idx != right[i]) {
      break;
    }
  }
  awra_text_layout_free(l);
}

/* lines drawn only with a fallback font are as tall as that font, as
 * Pango's line extents are (Devanagari in FreeSans: shorter than DejaVu) */
static void check_line_height(void)
{
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  AwraTextLayout *a = awra_text_layout_new(), *b = awra_text_layout_new();
  awra_text_layout_set_font(a, &f);
  awra_text_layout_set_font(b, &f);
  awra_text_layout_set_text(a, "Hello", false);
  awra_text_layout_set_text(b, "Hello \xe0\xa4\xa8\xe0\xa4\xae", false);
  int ha, hb;
  awra_text_layout_size(a, -1, NULL, &ha);
  awra_text_layout_size(b, -1, NULL, &hb);
  CHECK(hb >= ha, "a line with the font and a fallback: %d < %d", hb, ha);
  awra_text_layout_free(a);
  awra_text_layout_free(b);
}

/* Pango's ellipsizing: the line fills the width (the ellipsis is padded
 * out), an RTL paragraph's END ellipsis is at its left, the kept text keeps
 * its first character */
static void check_ellipsize(bool arabic)
{
  if (!arabic) {
    return;
  }
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.family = "";
  AwraTextLayout *l = awra_text_layout_new();
  awra_text_layout_set_font(l, &f);
  const char *t = AR_MEEM AR_BEH AR_ALEF " " AR_LAM AR_ALEF " " AR_BEH AR_MEEM AR_BEH AR_MEEM " "
                  AR_MEEM AR_BEH AR_ALEF " " AR_LAM AR_ALEF " " AR_BEH AR_MEEM AR_BEH AR_MEEM;
  awra_text_layout_set_text(l, t, false);
  awra_text_layout_set_wrap(l, AWRA_WRAP_NONE, AWRA_ELLIPSIZE_END, -1);
  float lx, lw;
  awra_text_layout_logical(l, 60, &lx, &lw);
  CHECK(fabsf(lx) < 0.01f && fabsf(lw - 60.0f) < 0.01f, "ellipsized line %.2f + %.2f, not 0 + 60", lx, lw);
  float r[4];
  awra_text_layout_char_rect(l, 60, 0, r);
  CHECK(fabsf(r[0] + r[2] - 60.0f) < 0.01f, "first character's box ends at %.2f, not 60", r[0] + r[2]);
  /* the ellipsis is the leftmost thing: clicking it lands in the hidden text */
  const int hit = awra_text_layout_pos_to_index(l, 60, 1.0f, 2.0f);
  CHECK(hit > 0 && hit <= (int)strlen(t), "hit on the ellipsis: %d", hit);
  awra_text_layout_free(l);
}

int main(void)
{
  awra_init("org.awra.TestBidi");
  const bool arabic = width(AR_ALEF, 0) != width(AR_MEEM, 0);
  const bool hebrew = width("\xd7\x99", 0) != width("\xd7\xa9", 0);
  if (!arabic) {
    fprintf(stderr, "skip: no Arabic font\n");
  }
  if (!hebrew) {
    fprintf(stderr, "skip: no Hebrew font\n");
  }
  check_bidi_levels();
  check_graphemes();
  check_backspace();
  check_shaping(arabic, hebrew);
  check_layout(arabic, hebrew);
  check_line_height();
  check_ellipsize(arabic);
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("bidi ok\n");
  return 0;
}
