/* Submenus: AWRA_MENU_ITEM_SUBMENU rows open nested menus beside them; keys
 * (Up/Down, Right, Left, Return, Escape) and the pointer resting on a row
 * open and close them; activating an item closes the whole chain.
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

static int activated = -1;
static void on_item(AwraWidget *w, const AwraArgs *a, void *data)
{
  activated = a->index;
}

static AwraWidget *g_win;

static void frame(void)
{
  awra_pump();
  awra_window_render_offscreen(g_win, false);
}

static void key(const char *keysym)
{
  BK_key(awra_ctx(g_win), "", keysym, 0);
  frame();
}

static void wait_ms(double ms)
{
  const double end = awra_now_ms() + ms;
  while (awra_now_ms() < end) {
    frame();
  }
}

/* the menu rows of popup i, in order */
static void rows_of(BKNode *n, BKNode **out, int *k)
{
  for (int i = 0; i < n->nkids; i++) {
    if (awra_widget_is(n->kids[i], "AwraMenuRow")) {
      out[(*k)++] = n->kids[i];
    }
    else {
      rows_of(n->kids[i], out, k);
    }
  }
}

static int popups(void)
{
  return BK_popup_count(awra_ctx(g_win));
}

int main(int argc, char **argv)
{
  awra_init("org.awra.TestSubmenu");
  g_win = awra_window_new_offscreen("submenus", 640, 420, 1.0);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_margins(box, 12, 12, 12, 12);
  AwraWidget *button = awra_menu_button_new();
  awra_widget_set_halign(button, AWRA_ALIGN_START);
  awra_box_append(box, button);
  awra_window_set_child(g_win, box);
  /* 0 Open, 1 Share > {2 Email, 3 More > {4 Deep} 5, 6 Link} 7, 8 Quit */
  const AwraMenuItem items[] = {
      {"Open", NULL, "Ctrl+O", -1, true, false, on_item, NULL},
      {"Share", NULL, NULL, AWRA_MENU_ITEM_SUBMENU, true, false, NULL, NULL},
      {"Email", "mail-send-symbolic", NULL, -1, true, false, on_item, NULL},
      {"More", NULL, NULL, AWRA_MENU_ITEM_SUBMENU, true, false, NULL, NULL},
      {"Deep", NULL, NULL, -1, true, false, on_item, NULL},
      {NULL, NULL, NULL, AWRA_MENU_ITEM_SUBMENU_END, true, false, NULL, NULL},
      {"Link", NULL, NULL, -1, true, false, on_item, NULL},
      {NULL, NULL, NULL, AWRA_MENU_ITEM_SUBMENU_END, true, false, NULL, NULL},
      {"Quit", NULL, NULL, -1, true, true, on_item, NULL},
  };
  awra_menu_button_set_menu(button, items, 9);
  frame();
  AwraWidget *menu = awra_menu_button_popover(button);

  /* keyboard: the menu opens with its first row focused (gtk_popover_show),
   * down to Share, Right opens it with Email focused */
  awra_popover_popup(menu);
  frame();
  CHECK(popups() == 1, "menu not open");
  key("Down");
  key("Right");
  CHECK(popups() == 2, "Right did not open the submenu (%d)", popups());
  key("Down"); /* More */
  key("Right");
  CHECK(popups() == 3, "nested submenu not open (%d)", popups());
  key("Left");
  CHECK(popups() == 2, "Left did not close the nested submenu (%d)", popups());
  key("Escape");
  CHECK(popups() == 1, "Escape closed %d menus", 2 - popups());
  key("Right"); /* the focus went back to Share */
  CHECK(popups() == 2, "focus not back on Share");
  key("Return"); /* Email */
  CHECK(activated == 2, "Email gave index %d", activated);
  CHECK(popups() == 0, "the chain stayed open (%d)", popups());

  /* the pointer resting on Share opens it; on Open, closes it */
  awra_popover_popup(menu);
  frame();
  BKNode *rows[16];
  int n = 0;
  rows_of(BK_popup(awra_ctx(g_win), 0), rows, &n);
  CHECK(n == 3, "%d rows in the menu", n);
  if (n == 3) {
    BKContext *ctx = awra_ctx(g_win);
    BK_motion(ctx, rows[1]->x + rows[1]->w / 2, rows[1]->y + rows[1]->h / 2, 0);
    frame();
    CHECK(popups() == 1, "opened without waiting");
    wait_ms(320);
    CHECK(popups() == 2, "hovering did not open the submenu");
    /* the submenu sits beside the row, centred on it */
    BKNode *sub = BK_popup(ctx, 1);
    BKNode *top = BK_popup(ctx, 0);
    if (sub && top) {
      CHECK(sub->x >= rows[1]->x + rows[1]->w - 1, "submenu at x %d, row ends at %d", sub->x,
            rows[1]->x + rows[1]->w);
      const int c1 = sub->y + sub->h / 2, c0 = rows[1]->y + rows[1]->h / 2;
      CHECK(abs(c1 - c0) <= 6, "submenu centre %d, row centre %d", c1, c0);
    }
    if (argc > 1) {
      awra_window_snapshot_png(g_win, argv[1]);
    }
    BK_motion(ctx, rows[0]->x + rows[0]->w / 2, rows[0]->y + rows[0]->h / 2, 0);
    wait_ms(320);
    CHECK(popups() == 1, "resting on another row kept the submenu (%d)", popups());
    /* a click in the submenu activates and closes everything */
    BK_motion(ctx, rows[1]->x + rows[1]->w / 2, rows[1]->y + rows[1]->h / 2, 0);
    BK_press(ctx, rows[1]->x + rows[1]->w / 2, rows[1]->y + rows[1]->h / 2, 0);
    BK_release(ctx, rows[1]->x + rows[1]->w / 2, rows[1]->y + rows[1]->h / 2, 0);
    frame();
    CHECK(popups() == 2, "a click did not open the submenu (%d)", popups());
    BKNode *srows[16];
    int sn = 0;
    rows_of(BK_popup(ctx, 1), srows, &sn);
    CHECK(sn == 3, "%d rows in the submenu", sn);
    if (sn == 3) {
      const int x = srows[2]->x + srows[2]->w / 2, y = srows[2]->y + srows[2]->h / 2;
      BK_motion(ctx, x, y, 0);
      BK_press(ctx, x, y, 0);
      BK_release(ctx, x, y, 0);
      frame();
      CHECK(activated == 6, "Link gave index %d", activated);
      CHECK(popups() == 0, "%d menus still open", popups());
    }
  }
  awra_widget_destroy(g_win);
  if (failures) {
    return 1;
  }
  printf("submenus ok\n");
  return 0;
}
