/* What ExplorerAwra added: the transition bin, the drag preview, a tab's
 * bounds, popovers pointing at a point, the rubberband's look, exact
 * shortcuts first, a widget's opacity. Offscreen; run under AddressSanitizer
 * too.
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

static void frame(void)
{
  awra_pump();
  awra_window_render_offscreen(g_win, false);
}

static void wait_ms(double ms)
{
  const double end = awra_now_ms() + ms;
  while (awra_now_ms() < end) {
    frame();
  }
}

static int g_hits[2];
static void hit(AwraWidget *w, const AwraArgs *a, void *data)
{
  (void)w;
  (void)a;
  g_hits[(intptr_t)data]++;
}

static void transition_bin(void)
{
  AwraWidget *bin = awra_transition_bin_new();
  AwraWidget *label = awra_label_new("Before");
  awra_transition_bin_set_child(bin, label);
  awra_widget_unref(label);
  awra_window_set_child(g_win, bin);
  awra_widget_unref(bin);
  awra_window_render_offscreen(g_win, true);
  CHECK(awra_transition_bin_child(bin) == label, "the child");
  const int x0 = label->x;
  awra_transition_bin_set_fixed_top(bin, 10);
  awra_label_set_text(label, "After");
  awra_transition_bin_play(bin, 18, 0);
  /* at once: the new frame starts away and invisible */
  awra_pump();
  CHECK(awra_widget_opacity(label) < 0.5, "the new frame shows at once (%.2f)",
        awra_widget_opacity(label));
  wait_ms(awra_motion_duration(AWRA_MOTION_NORMAL) + 80);
  CHECK(label->x == x0, "the child stays moved (%d, %d)", label->x, x0);
  CHECK(awra_widget_opacity(label) == 1.0, "the child's opacity isn't given back");
  /* a motion cut short by another, then by the bin going */
  awra_transition_bin_play(bin, -18, 0);
  frame();
  awra_transition_bin_play(bin, 0, 0);
  frame();
  awra_window_set_child(g_win, NULL);
  frame();
}

static void drag_preview(void)
{
  AwraWidget *p = awra_drag_preview_new();
  awra_drag_preview_set_icon_size(p, 64);
  awra_drag_preview_set_count(p, 3);
  awra_drag_preview_set_item_icon(p, 0, "text-x-generic-symbolic");
  awra_drag_preview_set_item_icon(p, 1, "folder-symbolic");
  AwraTexture *t = awra_texture_new_for_icon("folder-symbolic", 48);
  awra_drag_preview_set_item(p, 2, t);
  awra_drag_preview_set_item(p, 2, t); /* replaced: the first one is freed */
  awra_texture_unref(t);
  awra_drag_preview_set_item(p, 7, NULL); /* out of the pile: nothing */
  AwraTexture *pile = awra_drag_preview_texture(p);
  CHECK(pile && awra_texture_width(pile) == 64 + 32 + 12, "the pile's size");
  if (pile) {
    const int w = awra_texture_width(pile);
    uint8_t *rgba = (uint8_t *)malloc((size_t)w * w * 4);
    awra_texture_download(pile, rgba, w * 4);
    int drawn = 0;
    for (int i = 0; i < w * w; i++) {
      drawn += rgba[i * 4 + 3] > 16;
    }
    CHECK(drawn > 64 * 64 / 4, "the pile is empty (%d)", drawn);
    CHECK(rgba[3] == 0, "the corner isn't transparent");
    free(rgba);
    awra_texture_unref(pile);
  }
  /* the window's drag image, fanning out, then taken away */
  awra_drag_preview_set_drag_window(p, g_win, 0, 0);
  CHECK(awra_widget_get_data(g_win, "awra-drag-icon") != NULL, "no drag image");
  wait_ms(300);
  awra_drag_preview_set_drag_window(p, NULL, 0, 0);
  /* going while it is the drag image: the window forgets it */
  awra_drag_preview_set_drag_window(p, g_win, 4, 4);
  awra_widget_unref(p);
  wait_ms(300); /* its fan holds it until it settles */
  CHECK(awra_widget_get_data(g_win, "awra-drag-preview") == NULL, "the window keeps a dead preview");
}

static void tab_bounds_and_popover(void)
{
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *bar = awra_tab_bar_new();
  awra_tab_bar_insert(bar, -1, "One", NULL);
  awra_tab_bar_insert(bar, -1, "Two", NULL);
  awra_box_append(box, bar);
  awra_widget_unref(bar);
  AwraWidget *area = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_vexpand(area, true);
  awra_box_append(box, area);
  awra_widget_unref(area);
  awra_window_set_child(g_win, box);
  awra_widget_unref(box);
  awra_window_render_offscreen(g_win, true);
  double x, y, w, h;
  CHECK(awra_tab_bar_tab_bounds(bar, 1, &x, &y, &w, &h) && x > 0 && w > 0 && h > 0,
        "no bounds for the second tab");
  CHECK(!awra_tab_bar_tab_bounds(bar, 2, &x, &y, &w, &h), "a tab that isn't there");

  /* a menu pointing at a point: centred below it */
  AwraMenuItem items[2] = {{"First", NULL, NULL, -1, true, NULL, NULL},
                           {"Second", NULL, NULL, -1, true, NULL, NULL}};
  AwraWidget *menu = awra_menu_popover_new(items, 2);
  awra_popover_point_to(menu, area, 200, 50);
  wait_ms(200);
  const int cx = menu->x + menu->w / 2 - area->x;
  CHECK(abs(cx - 200) <= 2, "not centred on the point (%d)", cx);
  CHECK(menu->y >= area->y + 50 - 1, "not below the point");
  awra_popover_popdown(menu);
  frame();
  /* at the bottom: above the point */
  awra_popover_point_to(menu, area, 200, area->h - 5);
  wait_ms(200);
  CHECK(menu->y + menu->h <= area->y + area->h - 5 + 1, "not above the point");
  awra_popover_popdown(menu);
  frame();
  awra_widget_unref(menu);
  awra_window_set_child(g_win, NULL);
  frame();
}

static void shortcuts(void)
{
  g_hits[0] = g_hits[1] = 0;
  awra_window_add_shortcut(g_win, "<Primary>b", hit, (void *)0, NULL);
  awra_window_add_shortcut(g_win, "<Primary><Shift>b", hit, (void *)1, NULL);
  awra_window_dispatch_shortcut(g_win, "B", BK_CTRL | BK_SHIFT);
  CHECK(g_hits[1] == 1 && g_hits[0] == 0, "Ctrl+Shift+B ran %d/%d", g_hits[0], g_hits[1]);
  awra_window_dispatch_shortcut(g_win, "b", BK_CTRL);
  CHECK(g_hits[0] == 1, "Ctrl+B didn't run");
  /* Shift making the character still matches */
  awra_window_add_shortcut(g_win, "<Primary>question", hit, (void *)0, NULL);
  awra_window_dispatch_shortcut(g_win, "question", BK_CTRL | BK_SHIFT);
  CHECK(g_hits[0] == 2, "Ctrl+? didn't run");
}

static void band_and_opacity(void)
{
  AwraWidget *view = awra_collection_view_new(AWRA_COLLECTION_LIST);
  awra_collection_view_set_band_style(view, true, 0.10, 0.65, 0, true);
  awra_window_set_child(g_win, view);
  awra_widget_unref(view);
  awra_window_render_offscreen(g_win, true);
  awra_widget_set_opacity(view, 0.25);
  CHECK(fabs(awra_widget_opacity(view) - 0.25) < 1e-6, "opacity %.2f", awra_widget_opacity(view));
  awra_widget_set_opacity(view, 1.0);
  /* a band drawn and let go: it fades */
  BKContext *ctx = awra_ctx(g_win);
  BK_motion(ctx, 300, 250, 0);
  BK_button(ctx, 300, 250, 1, 0, 0);
  BK_motion(ctx, 200, 150, 0);
  BK_motion(ctx, 100, 60, 0);
  frame();
  BK_button(ctx, 100, 60, 1, 1, 0);
  wait_ms(awra_motion_duration(AWRA_MOTION_FAST) + 80);
  awra_window_set_child(g_win, NULL);
  frame();
}

/* a page closed while the stack cross-fades from it */
static void stack_page_gone(void)
{
  AwraWidget *stack = awra_stack_new();
  awra_stack_set_transition(stack, AWRA_TRANSITION_CROSSFADE, 200);
  AwraWidget *a = awra_label_new("A"), *b = awra_label_new("B");
  awra_stack_add_named(stack, a, "a");
  awra_stack_add_named(stack, b, "b");
  awra_widget_unref(a);
  awra_widget_unref(b);
  awra_window_set_child(g_win, stack);
  awra_widget_unref(stack);
  awra_window_render_offscreen(g_win, true);
  awra_stack_set_visible_child(stack, b);
  frame();
  awra_container_remove(stack, a); /* freed while it fades out */
  wait_ms(250);
  CHECK(awra_stack_visible_child(stack) == b, "the visible page changed");
  awra_container_remove(stack, b); /* the visible one */
  CHECK(awra_stack_visible_child(stack) == NULL, "a page that is gone stays visible");
  frame();
  awra_window_set_child(g_win, NULL);
  frame();
}

/* The palette is a window of its own, as Awra 1's: presenting and closing
 * it leaves the window's keyboard focus where it was */
static void focus_comes_back(void)
{
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *button = awra_button_new_with_label("Here");
  awra_box_append(box, button);
  awra_widget_unref(button);
  awra_window_set_child(g_win, box);
  awra_widget_unref(box);
  awra_window_render_offscreen(g_win, true);
  awra_widget_grab_focus(button);
  CHECK(awra_window_focus(g_win) == button, "no focus to start with");
  AwraWidget *palette = awra_command_palette_new();
  AwraCommand cmds[1] = {{"Zoom", NULL, NULL, NULL, NULL}};
  awra_command_palette_set_commands(palette, cmds, 1);
  awra_command_palette_present(palette, g_win);
  frame();
  AwraWidget *pw = awra_widget_root(palette);
  CHECK(pw && pw != g_win, "the palette isn't a window of its own");
  CHECK(awra_window_focus(g_win) == button, "the palette moved its parent's focus");
  awra_command_palette_close(palette);
  frame();
  CHECK(awra_window_state(pw) & AWRA_WINDOW_STATE_HIDDEN, "the palette stayed open");
  CHECK(awra_window_focus(g_win) == button, "the keyboard didn't come back");
  awra_widget_unref(palette);
  awra_window_set_child(g_win, NULL);
  frame();
}

/* gtk_widget_set_visible on a window shows or hides the window itself: a
 * window hidden to the tray must not leave its surface (and its blur) */
static void window_visibility(void)
{
  AwraWidget *w = awra_window_new("hidden", 200, 100);
  awra_window_present(w);
  CHECK(awra_widget_visible(w), "a presented window isn't visible");
  CHECK(!(awra_window_state(w) & AWRA_WINDOW_STATE_HIDDEN), "a presented window is hidden");
  awra_widget_set_visible(w, false);
  CHECK(!awra_widget_visible(w), "set_visible (false) left the window visible");
  CHECK(awra_window_state(w) & AWRA_WINDOW_STATE_HIDDEN, "set_visible (false) didn't hide the window");
  awra_widget_set_visible(w, true);
  CHECK(awra_widget_visible(w), "set_visible (true) didn't show the window again");
  CHECK(!(awra_window_state(w) & AWRA_WINDOW_STATE_HIDDEN), "set_visible (true) left the window hidden");
  awra_widget_destroy(w);
  frame();
}

int main(void)
{
  awra_init("org.awra.TestExplorerWidgets");
  g_win = awra_window_new_offscreen("explorer", 500, 400, 1.0);
  awra_window_render_offscreen(g_win, true);
  transition_bin();
  drag_preview();
  tab_bounds_and_popover();
  shortcuts();
  band_and_opacity();
  stack_page_gone();
  focus_comes_back();
  window_visibility();
  awra_widget_destroy(g_win);
  if (failures) {
    return 1;
  }
  printf("explorer widgets ok\n");
  return 0;
}
