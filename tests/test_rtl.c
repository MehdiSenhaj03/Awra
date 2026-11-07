/* Right-to-left widgets as GTK lays them out: the default direction from
 * the locale, children mirrored in their container (start is right),
 * labels and entries aligned at the right, scales inverted with Left
 * raising them, switches on at the left, text layouts' base direction,
 * icons that have right-to-left variants mirrored, and the first focus as
 * gtk_window_show moves it (gtk_widget_focus_sort).
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

static void render(AwraWidget *win)
{
  awra_window_render_offscreen(win, true);
}

static void check_locale(void)
{
  /* gtk_get_locale_direction: the language of messages */
  setenv("LANG", "ar_EG.UTF-8", 1);
  unsetenv("LC_ALL");
  unsetenv("LC_MESSAGES");
  unsetenv("LANGUAGE");
  CHECK(awra_get_locale_direction() == AWRA_TEXT_DIR_RTL, "ar_EG reads right to left");
  setenv("LANG", "he_IL.UTF-8", 1);
  CHECK(awra_get_locale_direction() == AWRA_TEXT_DIR_RTL, "he_IL");
  setenv("LANGUAGE", "fr:ar", 1);
  CHECK(awra_get_locale_direction() == AWRA_TEXT_DIR_LTR, "LANGUAGE's first entry wins");
  unsetenv("LANGUAGE");
  setenv("LANG", "de_DE.UTF-8", 1);
  CHECK(awra_get_locale_direction() == AWRA_TEXT_DIR_LTR, "de_DE");
  setenv("LANG", "C.UTF-8", 1);
  CHECK(awra_get_locale_direction() == AWRA_TEXT_DIR_LTR, "C");
}

static void check_layout(void)
{
  AwraWidget *win = awra_window_new_offscreen("RTL", 400, 300, 1.0);
  AwraWidget *v = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *row = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 0);
  AwraWidget *a = awra_label_new("first"), *b = awra_label_new("second");
  awra_box_append(row, a);
  awra_box_append(row, b);
  awra_box_append(v, row);
  AwraWidget *start = awra_label_new("start");
  awra_widget_set_halign(start, AWRA_ALIGN_START);
  awra_widget_set_margin_start(start, 10);
  awra_box_append(v, start);
  AwraWidget *slider = awra_slider_new(0, 100, 1);
  awra_slider_set_value(slider, 50);
  awra_box_append(v, slider);
  awra_window_set_child(win, v);
  render(win);
  CHECK(a->x < b->x, "left to right: first at the left");
  const int ax = a->x;
  /* the default direction mirrors everything */
  awra_widget_set_default_direction(AWRA_TEXT_DIR_RTL);
  CHECK(awra_widget_get_direction(row) == AWRA_TEXT_DIR_RTL, "follows the default");
  render(win);
  CHECK(a->x > b->x, "right to left: first at the right (%d, %d)", a->x, b->x);
  CHECK(a->x + a->w == row->x + row->w, "first at the row's right end");
  CHECK(start->x + start->w + awra_px(10) == v->x + v->w, "halign start and margin start at the right");
  /* a widget's own direction overrides the default for its children */
  awra_widget_set_direction(row, AWRA_TEXT_DIR_LTR);
  render(win);
  CHECK(a->x == ax, "a left-to-right row inside: first at the left again");
  awra_widget_set_direction(row, AWRA_TEXT_DIR_NONE);
  render(win);
  CHECK(a->x > b->x, "back to the default");
  /* Left raises a horizontal scale right to left (should_invert_move) */
  BKEvent e = {0};
  e.keysym = "Left";
  AWRA_CLASS_OF(slider)->key(slider, &e);
  double v1 = 0;
  AWRA_CLASS_OF(slider)->get_double(slider, "value", &v1);
  CHECK(v1 > 50, "Left raises: %g", v1);
  awra_widget_set_default_direction(AWRA_TEXT_DIR_LTR);
  render(win);
  CHECK(a->x == ax, "left to right again");
  awra_widget_destroy(win);
}

static void check_text(void)
{
  /* a paragraph without strong characters takes the widget's direction */
  AwraTextLayout *l = awra_text_layout_new();
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  awra_text_layout_set_font(l, &f);
  awra_text_layout_set_text(l, "123 456", false);
  float x0, y, h;
  awra_text_layout_index_to_pos(l, 300, 0, &x0, &y, &h);
  awra_text_layout_set_wrap(l, AWRA_WRAP_WORD, AWRA_ELLIPSIZE_NONE, 0);
  awra_text_layout_index_to_pos(l, 300, 0, &x0, &y, &h);
  awra_text_layout_set_base_dir(l, true);
  float x1;
  awra_text_layout_index_to_pos(l, 300, 0, &x1, &y, &h);
  CHECK(x1 > x0 + 100, "neutral paragraph at the right: %.1f vs %.1f", x1, x0);
  /* a left-to-right paragraph stays at the left */
  awra_text_layout_set_text(l, "abc", false);
  awra_text_layout_index_to_pos(l, 300, 0, &x1, &y, &h);
  CHECK(x1 < 1.0f, "LTR paragraph at the left: %.1f", x1);
  awra_text_layout_free(l);
}

static void check_icons(void)
{
  /* go-next drawn right to left is go-next mirrored (GTK's go-next-symbolic-rtl) */
  const AwraIconMask *m = awra_icon_mask("go-next-symbolic", 16);
  CHECK(m != NULL, "icon");
  if (!m) {
    return;
  }
  BuiRaster *r = BUI_raster_create(16, 16);
  CHECK(r != NULL, "raster");
  if (!r) {
    return;
  }
  /* the left half of go-next carries less ink than its right half (the
   * arrow's head); mirrored, the other way round */
  long left = 0, right = 0;
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 8; x++) {
      left += m->a[y * 16 + x];
      right += m->a[y * 16 + 15 - x];
    }
  }
  AwraDraw d = {r, 1.0f, true};
  const AwraRGBA white = {1, 1, 1, 1};
  BUI_raster_begin_frame(r, 16, 16);
  BUI_raster_clear(r, 0, 0, 16, 16);
  awra_paint_icon(&d, "go-next-symbolic", 0, 0, 16, &white);
  BUI_raster_end_frame(r);
  const uint32_t *px = BUI_raster_pixels(r);
  long dl = 0, dr = 0;
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 8; x++) {
      dl += px[y * 16 + x] & 0xFF; /* white: any channel is the coverage */
      dr += px[y * 16 + 15 - x] & 0xFF;
    }
  }
  CHECK((left < right) == (dl > dr), "mirrored right to left: %ld/%ld vs %ld/%ld", left, right, dl, dr);
  BUI_raster_free(r);
}

/* A window's first focus as GTK's: the first focusable widget in focus
 * order, where a box's children go from left to right as they show (a
 * right-to-left row starts at its end) and other containers' in reading
 * order (right to left in a right-to-left grid). */
static AwraWidget *first_focus(AwraTextDirection dir, bool grid, AwraWidget **a, AwraWidget **b)
{
  AwraTextDirection was = awra_widget_get_default_direction();
  awra_widget_set_default_direction(dir);
  AwraWidget *win = awra_window_new_offscreen("Focus", 400, 300, 1.0);
  *a = awra_button_new_with_label("A");
  *b = awra_entry_new();
  if (grid) {
    /* the second child in the first column */
    AwraWidget *g = awra_grid_new();
    awra_grid_attach(g, *a, 1, 0, 1, 1);
    awra_grid_attach(g, *b, 0, 0, 1, 1);
    awra_window_set_child(win, g);
  }
  else {
    /* a rail and a column: Dawra's sidebar */
    AwraWidget *row = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 0);
    AwraWidget *column = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
    awra_box_append(row, *a);
    awra_box_append(column, awra_label_new("Direct messages"));
    awra_box_append(column, *b);
    awra_widget_set_hexpand(column, true);
    awra_box_append(row, column);
    awra_window_set_child(win, row);
  }
  awra_window_present(win);
  AwraWidget *focus = awra_window_focus(win);
  awra_window_close(win);
  awra_widget_set_default_direction(was);
  return focus;
}

static void check_first_focus(void)
{
  AwraWidget *a, *b;
  CHECK(first_focus(AWRA_TEXT_DIR_LTR, false, &a, &b) == a, "a row starts at its left: the rail");
  CHECK(first_focus(AWRA_TEXT_DIR_RTL, false, &a, &b) == b,
        "a right-to-left row starts at its left too: the column's field");
  CHECK(first_focus(AWRA_TEXT_DIR_LTR, true, &a, &b) == b, "a grid in reading order: column 0");
  CHECK(first_focus(AWRA_TEXT_DIR_RTL, true, &a, &b) == b,
        "a right-to-left grid reads from its right: column 0 still");
}

int main(void)
{
  check_locale();
  awra_init("org.awra.TestRtl");
  check_layout();
  check_text();
  check_icons();
  check_first_focus();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  return 0;
}
