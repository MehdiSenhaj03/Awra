/* While a menu that closes on a click outside is open, the wheel outside it
 * scrolls nothing: the list under a context menu stays where it was.
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

static AwraWidget *g_win;
bool awra_anims_tick(void); /* core.c: one frame of the running animations */

static void frame(void)
{
  awra_pump();
  awra_window_render_offscreen(g_win, false);
}

/* a wheel step, its smooth scroll run to the end */
static void wheel(BKContext *ctx, int x, int y)
{
  const int used = BK_scroll(ctx, x, y, 0.0f, -1.0f, 0); /* down */
  (void)used;
  const double end = awra_now_ms() + 400;
  while (awra_now_ms() < end) {
    awra_anims_tick();
    frame();
  }
}

int main(void)
{
  awra_init("org.awra.TestPopupWheel");
  g_win = awra_window_new_offscreen("wheel", 400, 300, 1.0);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *button = awra_menu_button_new();
  awra_widget_set_halign(button, AWRA_ALIGN_START);
  const AwraMenuItem items[] = {
      {"Open", NULL, NULL, -1, true, false, NULL, NULL},
      {"Copy", NULL, NULL, -1, true, false, NULL, NULL},
  };
  awra_menu_button_set_menu(button, items, 2);
  awra_box_append(box, button);
  AwraWidget *scrolled = awra_scrolled_new();
  awra_widget_set_vexpand(scrolled, true);
  AwraWidget *list = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  for (int i = 0; i < 60; i++) {
    char text[32];
    snprintf(text, sizeof(text), "Row %d", i);
    AwraWidget *l = awra_label_new(text);
    awra_box_append(list, l);
    awra_widget_unref(l);
  }
  awra_scrolled_set_child(scrolled, list);
  awra_box_append(box, scrolled);
  awra_window_set_child(g_win, box);
  awra_widget_unref(list);
  awra_widget_unref(scrolled);
  awra_widget_unref(button);
  awra_widget_unref(box);
  awra_window_render_offscreen(g_win, true);
  frame();
  BKContext *ctx = awra_ctx(g_win);
  const int px = scrolled->x + scrolled->w / 2, py = scrolled->y + scrolled->h / 2;

  /* no menu: the wheel scrolls the list */
  wheel(ctx, px, py);
  const double scrolled_to = awra_scrolled_vvalue(scrolled);
  CHECK(scrolled_to > 0.0, "the wheel didn't scroll the list");

  /* a menu open: the wheel outside it leaves the list where it is */
  awra_popover_popup(awra_menu_button_popover(button));
  frame();
  CHECK(BK_popup_count(ctx) == 1, "the menu didn't open");
  BKNode *pop = BK_popup(ctx, 0);
  CHECK(pop && !(px >= pop->x && px < pop->x + pop->w && py >= pop->y && py < pop->y + pop->h),
        "the test point is inside the menu");
  wheel(ctx, px, py);
  CHECK(awra_scrolled_vvalue(scrolled) == scrolled_to, "the wheel scrolled the list under an open menu (%g -> %g)",
        scrolled_to, awra_scrolled_vvalue(scrolled));
  CHECK(BK_popup_count(ctx) == 1, "the wheel closed the menu");

  /* closed again: the wheel scrolls */
  awra_popover_popdown(awra_menu_button_popover(button));
  frame();
  wheel(ctx, px, py);
  CHECK(awra_scrolled_vvalue(scrolled) > scrolled_to, "the wheel didn't scroll once the menu closed");

  awra_widget_destroy(g_win);
  awra_pump();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("popup wheel: ok\n");
  return 0;
}
