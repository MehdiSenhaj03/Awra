/* Popups in native surfaces of their own: GDK's layout resolution (the
 * numbers GTK 4.22 gives for the same layouts on X11), popovers placed by
 * a host as GtkPopover's are, their damage kept to their own surface, the
 * in-window fallback, autohide, and an in-application drag's cursor and
 * hover.
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

/* -- GDK's helper against GTK's own results (python3 GTK 4.22, X11) ----- */

static void layout_cases(void)
{
  /* a 1024 x 700 screen; the window at (wx, wy): bounds in its coordinates */
  struct {
    int wx, wy;
    BKRect anchor;
    int ra, sa;
    unsigned hints;
    int w, h, ex, ey;
  } cases[] = {
      /* a menu button's popover: below, centred (GTK: 279 248) */
      {0, 0, {374, 214, 408, 248}, BK_GRAVITY_SOUTH, BK_GRAVITY_NORTH,
       BK_ANCHOR_FLIP_Y | BK_ANCHOR_SLIDE_X | BK_ANCHOR_RESIZE, 224, 209, 279, 248},
      /* near the screen's corner: flipped above, slid left (GTK: 200 5) */
      {600, 430, {374, 214, 408, 248}, BK_GRAVITY_SOUTH, BK_GRAVITY_NORTH,
       BK_ANCHOR_FLIP_Y | BK_ANCHOR_SLIDE_X | BK_ANCHOR_RESIZE, 224, 209, 200, 5},
      /* a submenu at the screen's right edge: flipped to the left
       * (GTK: -217 36 from its menu at 200 248) */
      {600, 100, {207, 327, 417, 363}, BK_GRAVITY_EAST, BK_GRAVITY_WEST,
       BK_ANCHOR_FLIP_X | BK_ANCHOR_SLIDE_Y | BK_ANCHOR_RESIZE, 224, 122, -17, 284},
      /* a context menu pointing at a click near the corner (GTK: 200 -59) */
      {600, 430, {390, 150, 391, 151}, BK_GRAVITY_SOUTH, BK_GRAVITY_NORTH,
       BK_ANCHOR_FLIP_Y | BK_ANCHOR_SLIDE_X | BK_ANCHOR_RESIZE, 224, 209, 200, -59},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    BKPopupLayout l;
    memset(&l, 0, sizeof(l));
    l.anchor = cases[i].anchor;
    l.rect_anchor = cases[i].ra;
    l.surface_anchor = cases[i].sa;
    l.hints = cases[i].hints;
    const BKRect bounds = bk_rect(-cases[i].wx, -cases[i].wy, 1024, 700);
    BKRect r;
    bk_popup_layout_resolve(&l, cases[i].w, cases[i].h, &bounds, &r, NULL);
    CHECK(r.xmin == cases[i].ex && r.ymin == cases[i].ey, "case %zu: %d %d, GTK %d %d", i, r.xmin,
          r.ymin, cases[i].ex, cases[i].ey);
  }
  /* taller than the work area: resized to it (GDK_ANCHOR_RESIZE) */
  BKPopupLayout l;
  memset(&l, 0, sizeof(l));
  l.anchor = bk_rect(10, 10, 20, 20);
  l.rect_anchor = BK_GRAVITY_SOUTH;
  l.surface_anchor = BK_GRAVITY_NORTH;
  l.hints = BK_ANCHOR_FLIP_Y | BK_ANCHOR_SLIDE_X | BK_ANCHOR_RESIZE;
  const BKRect bounds = bk_rect(0, 0, 400, 300);
  BKRect r;
  bk_popup_layout_resolve(&l, 200, 900, &bounds, &r, NULL);
  CHECK(r.ymin == 30 && r.ymax == 300, "resized to %d..%d", r.ymin, r.ymax);
}

/* -- a host: synchronous placement on a 1024 x 700 screen, the window at
 * 600, 430 (X11's and Win32's way) ------------------------------------- */

static int placed[BK_POPUP_SLOTS], closed[BK_POPUP_SLOTS];
static BKRect boxes[BK_POPUP_SLOTS];
static int g_shrink; /* the host gives popups at most this height (0: no limit) */

static int host_place(void *data, int slot, const BKPopupLayout *l, int w, int h, int grab,
                      BKRect *r_rect)
{
  const BKRect bounds = bk_rect(-600, -430, 1024, 700);
  bk_popup_layout_resolve(l, w, h, &bounds, r_rect, NULL);
  if (g_shrink && bk_rh(r_rect) > g_shrink) { /* a compositor that resized it */
    r_rect->ymax = r_rect->ymin + g_shrink;
  }
  boxes[slot] = *r_rect;
  placed[slot]++;
  return 1;
}

static void host_close(void *data, int slot)
{
  closed[slot]++;
}

static BKPopupHost g_host = {NULL, 0, 0, host_place, host_close, 0, NULL};

static AwraWidget *g_win;

static void frame(void)
{
  awra_pump();
  awra_window_render_offscreen(g_win, false);
}

/* the unfold animation done */
static void settle(void)
{
  const double end = awra_now_ms() + 200;
  while (awra_now_ms() < end) {
    frame();
  }
}

static long rss_kb(void)
{
  FILE *f = fopen("/proc/self/status", "r");
  char line[256];
  long v = -1;
  while (f && fgets(line, sizeof line, f)) {
    if (!strncmp(line, "VmRSS:", 6)) {
      v = atol(line + 6);
    }
  }
  if (f) {
    fclose(f);
  }
  return v;
}

static void item_setup(AwraWidget *list, AwraWidget *item, void *data)
{
  AwraWidget *l = awra_label_new("");
  awra_list_item_set_child(item, l);
  awra_widget_unref(l);
}

static void item_bind(AwraWidget *list, AwraWidget *item, void *data)
{
  static const char *const names[] = {"alpha", "beta", "gamma"};
  AwraWidget *l = awra_widget_child(item, 0);
  if (l) {
    awra_label_set_text(l, names[awra_list_item_position(item) % 3]);
  }
}

static void no_schedule(void *data, int ms, int timer)
{
  (void)data; /* the test fires the tooltip's timer itself */
}

static void wait_ms(AwraWidget *win, double ms)
{
  const double end = awra_now_ms() + ms;
  while (awra_now_ms() < end) {
    awra_pump();
    awra_window_render_offscreen(win, false);
  }
}

/* The path bar's suggestions, the composer's completion list, a popover
 * the system shrinks below its minimum, the tooltip and the cursor's size. */
static void more_tests(void)
{
  AwraWidget *win = awra_window_new_offscreen("more", 480, 340, 1.0);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 8);
  awra_widget_set_margins(box, 12, 12, 12, 12);
  AwraWidget *bar = awra_path_bar_new();
  const char *const crumbs[] = {"home", "user"};
  awra_path_bar_set_items(bar, crumbs, 2);
  awra_box_append(box, bar);
  AwraWidget *other = awra_button_new_with_label("Elsewhere");
  awra_widget_set_halign(other, AWRA_ALIGN_START);
  awra_box_append(box, other);
  AwraWidget *tall = awra_button_new_with_label("tall");
  awra_widget_set_size_request(tall, 100, 80);
  awra_widget_set_halign(tall, AWRA_ALIGN_START);
  awra_widget_set_tooltip(tall, "A tip");
  awra_box_append(box, tall);
  AwraWidget *view = awra_text_view_new();
  awra_widget_set_vexpand(view, true);
  awra_box_append(box, view);
  awra_window_set_child(win, box);
  awra_widget_unref(bar);
  awra_widget_unref(other);
  awra_widget_unref(tall);
  awra_widget_unref(view);
  awra_widget_unref(box);
  BKContext *ctx = awra_ctx(win);
  g_host.cursor_size = 48;
  bk_popup_host_set(ctx, &g_host);
  wait_ms(win, 30);

  /* path bar: suggestions leave the keyboard in the field and don't hide
   * on clicks; the keyboard leaving the field ends the edit and them */
  awra_path_bar_begin_edit(bar, "/ho");
  const char *const titles[] = {"home", "hosts"}, *const values[] = {"/home", "/hosts"};
  awra_path_bar_set_suggestions(bar, titles, values, 2);
  wait_ms(win, 150);
  AwraWidget *entry = awra_path_bar_entry(bar);
  CHECK(BK_popup_count(ctx) == 1, "no suggestions");
  CHECK(BK_popup_count(ctx) && (BK_popup(ctx, 0)->flags & NF_NO_AUTOHIDE),
        "suggestions autohide");
  CHECK(awra_widget_has_focus(entry), "the field lost the keyboard to its suggestions");
  awra_widget_grab_focus(other);
  wait_ms(win, 30);
  CHECK(!awra_path_bar_editing(bar), "editing kept with the keyboard elsewhere");
  CHECK(BK_popup_count(ctx) == 0, "suggestions kept with the keyboard elsewhere");

  /* the composer's completion: above the caret, at its start (Awra 1:
   * GTK_POS_TOP, offset -4, start), not autohide */
  AwraWidget *comp = awra_completion_popover_new();
  AwraWidget *list = awra_completion_popover_list(comp);
  awra_list_view_set_factory(list, item_setup, item_bind, NULL, NULL, NULL, NULL);
  awra_list_view_items_changed(list, 0, 0, 3);
  awra_completion_popover_attach(comp, view);
  awra_text_view_set_text(view, "Hello @al");
  awra_widget_grab_focus(view);
  wait_ms(win, 30);
  awra_completion_popover_present_at_cursor(comp);
  wait_ms(win, 150);
  double cx, cy, ch;
  awra_editable_caret_rect(view, &cx, &cy, &ch);
  const int caret_x = view->x + (int)floor(cx), caret_y = view->y + (int)floor(cy);
  CHECK(BK_popup_count(ctx) == 1 && bk_popup_surfaced(ctx, 0), "no completion surface");
  if (BK_popup_count(ctx) == 1) {
    CHECK(boxes[0].ymax == caret_y - 4 && boxes[0].xmin == caret_x,
          "completion at %d,%d..%d; caret at %d,%d", boxes[0].xmin, boxes[0].ymin, boxes[0].ymax,
          caret_x, caret_y);
    CHECK(BK_popup(ctx, 0)->flags & NF_NO_AUTOHIDE, "completion autohide");
  }
  awra_completion_popover_popdown(comp);
  awra_widget_unref(comp);
  wait_ms(win, 30);

  /* shrunk under its minimum by the system: it pops down (GTK) */
  AwraWidget *pop = awra_popover_new();
  AwraWidget *big = awra_label_new("big");
  awra_widget_set_size_request(big, 200, 150);
  awra_popover_set_child(pop, big);
  awra_widget_unref(big);
  awra_popover_set_anchor(pop, other);
  g_shrink = 60;
  awra_popover_popup(pop);
  wait_ms(win, 60);
  CHECK(!awra_popover_visible(pop), "a popover shrunk under its minimum stayed");
  g_shrink = 0;
  awra_popover_popup(pop);
  wait_ms(win, 60);
  CHECK(awra_popover_visible(pop), "a popover at its size closed");
  awra_popover_popdown(pop);
  awra_widget_unref(pop);
  wait_ms(win, 30);

  /* a tooltip over a widget taller than 56 px anchors to the cursor, as
   * big as the host's (48 px here): under it, 4 px above its hot spot */
  const int px = tall->x + 20, py = tall->y + 20;
  ctx->host.schedule = no_schedule;
  BK_motion(ctx, px, py, 0);
  BK_timer(ctx, ctx->tip_serial);
  CHECK(ctx->tip_shown && ctx->tip_rect.ymin == py - 4 + 48, "tooltip at %d, pointer at %d",
        ctx->tip_rect.ymin, py);
  BK_motion(ctx, -100000, -100000, 0);
  g_host.cursor_size = 0;
  bk_popup_host_set(ctx, NULL);
  awra_window_close(win);
}

int main(int argc, char **argv)
{
  awra_init("org.awra.TestPopups");
  layout_cases();

  g_win = awra_window_new_offscreen("popups", 420, 260, 1.0);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_margins(box, 12, 12, 12, 12);
  AwraWidget *hover = awra_button_new_with_label("Hover me");
  awra_widget_set_halign(hover, AWRA_ALIGN_START);
  awra_widget_set_vexpand(hover, true);
  awra_box_append(box, hover);
  AwraWidget *button = awra_menu_button_new();
  awra_widget_set_halign(button, AWRA_ALIGN_END);
  awra_box_append(box, button);
  awra_window_set_child(g_win, box);
  const AwraMenuItem items[] = {
      {"Open", NULL, "Ctrl+O", -1, true, false, NULL, NULL},
      {"Save", NULL, "Ctrl+S", -1, true, false, NULL, NULL},
      {"Quit", NULL, NULL, -1, true, true, NULL, NULL},
  };
  awra_menu_button_set_menu(button, items, 3);
  frame();
  BKContext *ctx = awra_ctx(g_win);
  AwraWidget *menu = awra_menu_button_popover(button);

  /* in the window without a host: kept inside it */
  awra_popover_popup(menu);
  frame();
  BKNode *pop = BK_popup(ctx, 0);
  CHECK(pop && !bk_popup_surfaced(ctx, 0), "in-window popup");
  if (pop) {
    CHECK(pop->y + pop->h <= 260 && pop->x >= 0 && pop->x + pop->w <= 420,
          "in-window popup outside its window: %d,%d %dx%d", pop->x, pop->y, pop->w, pop->h);
  }
  awra_popover_popdown(menu);
  frame();

  /* with a host: its own surface, placed as GTK places it (below the
   * button, flipped above it at the screen's bottom), outside the window */
  bk_popup_host_set(ctx, &g_host);
  awra_popover_popup(menu);
  settle();
  pop = BK_popup(ctx, 0);
  CHECK(pop && bk_popup_surfaced(ctx, 0) && placed[0] >= 1, "no surface (%d)", placed[0]);
  if (pop) {
    const BKRect a = bk_node_rect(button);
    CHECK(boxes[0].ymax == a.ymin && pop->y == boxes[0].ymin,
          "not flipped above the button: %d..%d, button at %d", boxes[0].ymin, boxes[0].ymax,
          a.ymin);
    CHECK(pop->x + pop->w > 420 || pop->x < 0 || pop->y < 0, "inside the window");
    /* its damage stays on its surface: the window draws nothing */
    awra_window_render_offscreen(g_win, false);
    const int before = awra_window_render_offscreen(g_win, false);
    CHECK(before == 0, "idle window redrew %d regions", before);
    BKNode *rows[8];
    int n = 0;
    BKNode *stack[64];
    int top = 0;
    stack[top++] = pop;
    while (top && n < 8) { /* the menu's rows, in order */
      BKNode *c = stack[--top];
      if (awra_widget_is(c, "AwraMenuRow")) {
        rows[n++] = c;
      }
      for (int i = c->nkids - 1; i >= 0 && top < 64; i--) {
        stack[top++] = c->kids[i];
      }
    }
    CHECK(n == 3, "%d rows", n);
    BuiRaster *r = BUI_raster_create(1, 1);
    BUI_raster_set_diff(r, 0); /* as the hosts' */
    const int full = bk_render_popup(ctx, 0, r, pop->w, pop->h, pop->x, pop->y, 1);
    CHECK(full == 1, "popup surface not drawn");
    const uint32_t centre = BUI_raster_pixels(r)[(size_t)(pop->h / 2) * pop->w + pop->w / 2];
    CHECK((centre >> 24) != 0, "popup surface empty in its middle");
    if (n == 3) {
      BK_motion(ctx, rows[1]->x + 5, rows[1]->y + 5, 0);
      awra_pump();
      for (int i = 0; argc > 1 && i < ctx->pops[0].ndirty; i++) {
        const BKRect d = ctx->pops[0].dirty[i];
        fprintf(stderr, "dirty %d,%d %dx%d\n", d.xmin - pop->x, d.ymin - pop->y, bk_rw(&d),
                bk_rh(&d));
      }
      const int main_regions = awra_window_render_offscreen(g_win, false);
      const int pop_regions = bk_render_popup(ctx, 0, r, pop->w, pop->h, pop->x, pop->y, 0);
      CHECK(main_regions == 0, "a hover in the menu redrew the window (%d)", main_regions);
      CHECK(pop_regions > 0, "the hover did not redraw the menu");
      const int(*rects)[4];
      const int nd = BUI_raster_damage(r, &rects);
      int area = 0;
      for (int i = 0; i < nd; i++) {
        area += rects[i][2] * rects[i][3];
        if (argc > 1) {
          fprintf(stderr, "hover damage %d,%d %dx%d (popup %dx%d, row %d,%d %dx%d)\n", rects[i][0],
                  rects[i][1], rects[i][2], rects[i][3], pop->w, pop->h, rows[1]->x - pop->x,
                  rows[1]->y - pop->y, rows[1]->w, rows[1]->h);
        }
      }
      CHECK(area < pop->w * pop->h / 2, "the hover redrew %d of %d px", area, pop->w * pop->h);
    }
    BUI_raster_free(r);
  }
  /* an outside click closes it, its surface goes */
  BK_press(ctx, 20, 60, BK_BUTTON1);
  BK_release(ctx, 20, 60, 0);
  frame();
  CHECK(BK_popup_count(ctx) == 0 && closed[0] >= 1, "outside click: %d open, %d closed",
        BK_popup_count(ctx), closed[0]);

  /* not autohide: an outside click goes to the window, the popup stays */
  awra_popover_set_autohide(menu, false);
  awra_popover_popup(menu);
  frame();
  BK_press(ctx, 20, 60, BK_BUTTON1);
  BK_release(ctx, 20, 60, 0);
  frame();
  CHECK(BK_popup_count(ctx) == 1, "a click elsewhere closed a popover without autohide");
  awra_popover_popdown(menu);
  awra_popover_set_autohide(menu, true);
  frame();

  /* opened and closed many times: memory flat */
  long rss0 = 0;
  for (int i = 0; i < 400; i++) {
    awra_popover_popup(menu);
    frame();
    awra_popover_popdown(menu);
    frame();
    if (i == 99) {
      rss0 = rss_kb();
    }
  }
  const long rss1 = rss_kb();
  CHECK(rss1 - rss0 < 256, "RSS grew %ld kB over 300 menus", rss1 - rss0);
  if (argc > 1) {
    fprintf(stderr, "rss after 100 menus %ld kB, after 400 %ld kB\n", rss0, rss1);
  }

  /* an in-application drag: its cursor, no hover under it */
  BK_motion(ctx, hover->x + 4, hover->y + 4, 0);
  frame();
  CHECK(awra_aux(hover)->hovered, "hover before the drag");
  awra_window_set_drag_action(g_win, AWRA_DRAG_ACTION_NONE);
  CHECK(BK_cursor(ctx) == BK_CURSOR_NO_DROP, "no-drop cursor: %d", BK_cursor(ctx));
  CHECK(!awra_aux(hover)->hovered, "hover kept under the drag");
  BK_motion(ctx, hover->x + 6, hover->y + 6, 0);
  CHECK(!awra_aux(hover)->hovered, "hover under the drag");
  awra_window_set_drag_action(g_win, AWRA_DRAG_ACTION_COPY);
  CHECK(BK_cursor(ctx) == BK_CURSOR_COPY, "copy cursor: %d", BK_cursor(ctx));
  awra_window_set_drag_action(g_win, AWRA_DRAG_ACTION_MOVE);
  CHECK(BK_cursor(ctx) == BK_CURSOR_DND_MOVE, "move cursor: %d", BK_cursor(ctx));
  awra_window_set_drag_action(g_win, -1);
  CHECK(BK_cursor(ctx) != BK_CURSOR_DND_MOVE, "the drag's cursor stayed");
  CHECK(awra_aux(hover)->hovered, "hover not back after the drag");

  bk_popup_host_set(ctx, NULL);
  awra_window_close(g_win);
  more_tests();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
  }
  return failures ? 1 : 0;
}
