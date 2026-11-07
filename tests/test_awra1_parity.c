/* What Awra 1 (GTK) did that Awra 2 reproduces: action rows and their
 * suffix, the split view's sidebar inset, GtkGrid's expansion of spanned
 * lines, button labels replacing children, modal windows, media rows'
 * idle state, waveform hints, inset shadows, unset default sizes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include "GPU_raster.h"

#include <stdio.h>

AwraWidget *awra_first_focusable(AwraWidget *root); /* popover.c */

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

static void frame(AwraWidget *win)
{
  awra_pump();
  awra_window_render_offscreen(win, false);
}

static int clicks;
static void on_clicked(AwraWidget *w, const AwraArgs *a, void *data)
{
  clicks++;
}

static int notifies;
static void on_notify(AwraWidget *w, const AwraArgs *a, void *data)
{
  if (a->text && !strcmp(a->text, "hint-position")) {
    notifies++;
  }
}

static void action_rows(void)
{
  AwraWidget *win = awra_window_new_offscreen("rows", 480, 200, 1.0);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *row = awra_action_row_new("Musique seule", "Un morceau à la fois");
  awra_box_append(box, row);
  awra_window_set_child(win, box);
  awra_window_present(win);
  /* a flat GtkButton in Awra 1: it takes the focus */
  AwraWidget *f = awra_first_focusable(row);
  CHECK(f == row, "an action row takes the focus");
  AwraWidget *a = awra_button_new_with_label("Choisir");
  AwraWidget *b = awra_button_new_with_label("Autre");
  awra_action_row_set_suffix(row, a);
  awra_action_row_set_suffix(row, b);
  CHECK(awra_action_row_get_suffix(row) == b, "set_suffix replaces the suffix");
  CHECK(a->parent == NULL, "the replaced suffix left the row");
  frame(win);
  /* the suffix keeps its fill alignment: as tall as the row's content */
  CHECK(b->h == awra_px(42), "a suffix button fills the row (%d)", b->h);
  CHECK(row->h == awra_px(60), "58 px row and its 1 px margins (%d)", row->h);
  awra_action_row_set_suffix(row, NULL);
  CHECK(awra_action_row_get_suffix(row) == NULL && b->parent == NULL, "NULL removes the suffix");
  awra_widget_unref(a);
  awra_widget_unref(b);
  awra_widget_unref(row);
  awra_widget_unref(box);
  awra_window_close(win);
}

static void split_view(void)
{
  AwraWidget *win = awra_window_new_offscreen("split", 1600, 900, 1.0);
  AwraWidget *split = awra_split_view_new();
  AwraWidget *side = awra_sidebar_new();
  AwraWidget *content = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_split_view_set_sidebar(split, side);
  awra_split_view_set_content(split, content);
  awra_window_set_child(win, split);
  awra_window_present(win);
  frame(win);
  /* Awra 1: gtk_paned_set_position (244 + inset * 2) with a 12 px inset */
  CHECK(side->w == 268, "the sidebar is 244 px plus its insets (%d)", side->w);
  awra_widget_unref(side);
  awra_widget_unref(content);
  awra_widget_unref(split);
  awra_window_close(win);
}

static void grid_spans(void)
{
  AwraWidget *win = awra_window_new_offscreen("grid", 500, 200, 1.0);
  AwraWidget *grid = awra_grid_new();
  AwraWidget *text = awra_label_new("Title");
  awra_widget_set_hexpand(text, true);
  AwraWidget *ends = awra_button_new_with_label("End");
  AwraWidget *seek = awra_slider_new(0, 1, 0);
  awra_widget_set_hexpand(seek, true);
  awra_grid_attach(grid, text, 0, 0, 1, 1);
  awra_grid_attach(grid, ends, 1, 0, 1, 1);
  awra_grid_attach(grid, seek, 0, 1, 2, 1);
  awra_window_set_child(win, grid);
  awra_window_present(win);
  frame(win);
  /* gtk_grid_request_compute_expand: the spanning slider expands no
   * column, one of its columns already does: the end column keeps its
   * natural width at the end */
  int mn, nat;
  awra_measure(ends, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nat);
  CHECK(ends->w == nat, "the end column keeps its natural width (%d, %d)", ends->w, nat);
  CHECK(ends->x + ends->w == grid->x + grid->w, "at the end");
  awra_widget_unref(text);
  awra_widget_unref(ends);
  awra_widget_unref(seek);
  awra_widget_unref(grid);
  awra_window_close(win);
}

static void button_label(void)
{
  AwraWidget *b = awra_button_new();
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 0);
  awra_button_set_child(b, box);
  awra_button_set_label(b, "Plain");
  CHECK(box->parent == NULL, "gtk_button_set_label replaces the child");
  CHECK(awra_button_label(b) && !strcmp(awra_button_label(b), "Plain"), "the label");
  awra_widget_unref(box);
  awra_widget_unref(b);
}

static void modal_windows(void)
{
  AwraWidget *parent = awra_window_new_offscreen("parent", 300, 200, 1.0);
  AwraWidget *button = awra_button_new_with_label("Click");
  awra_signal_connect(button, AWRA_SIGNAL_CLICKED, on_clicked, NULL, NULL);
  awra_window_set_child(parent, button);
  awra_window_present(parent);
  frame(parent);
  const int x = button->x + button->w / 2, y = button->y + button->h / 2;
  BK_motion(awra_ctx(parent), x, y, 0);
  BK_press(awra_ctx(parent), x, y, 0);
  BK_release(awra_ctx(parent), x, y, 0);
  CHECK(clicks == 1, "the parent takes clicks (%d)", clicks);
  AwraWidget *dialog = awra_window_new_offscreen("dialog", 200, 100, 1.0);
  awra_window_set_transient_for(dialog, parent);
  awra_window_set_modal(dialog, true);
  CHECK(awra_window_get_transient_for(dialog) == parent && awra_window_get_modal(dialog),
        "transient and modal");
  awra_window_present(dialog);
  BK_press(awra_ctx(parent), x, y, 0);
  BK_release(awra_ctx(parent), x, y, 0);
  CHECK(clicks == 1, "a modal window blocks its parent (%d)", clicks);
  awra_window_close(dialog);
  awra_pump();
  BK_press(awra_ctx(parent), x, y, 0);
  BK_release(awra_ctx(parent), x, y, 0);
  CHECK(clicks == 2, "closed, the parent takes clicks again (%d)", clicks);
  awra_widget_unref(button);
  awra_window_close(parent);
}

static void media_rows(void)
{
  AwraWidget *row = awra_media_row_new("Musique");
  AwraWidget *stop = NULL;
  awra_media_row_set_playing(row, false);
  awra_media_row_set_duration(row, 0);
  /* Awra 1's sync_seek: idle without a duration */
  AwraWidget *grid = awra_widget_child(row, 0);
  AwraWidget *ends = grid ? awra_widget_child(grid, 2) : NULL;
  stop = ends ? awra_widget_child(ends, 1) : NULL;
  CHECK(stop && !awra_widget_sensitive(stop), "idle: no stop");
  awra_media_row_set_duration(row, 12.0);
  CHECK(stop && awra_widget_sensitive(stop), "with a duration: stop");
  awra_widget_unref(row);
}

static void waveform_hint(void)
{
  AwraWidget *w = awra_waveform_view_new();
  awra_signal_connect(w, AWRA_SIGNAL_NOTIFY, on_notify, NULL, NULL);
  awra_waveform_view_set_hint_position(w, 2.0);
  CHECK(notifies == 1 && awra_waveform_view_hint_position(w) == 2.0, "the hint notifies");
  awra_waveform_view_set_hint_position(w, 2.0);
  CHECK(notifies == 1, "an unchanged hint does not");
  awra_waveform_view_set_hint_position(w, -5.0);
  CHECK(awra_waveform_view_hint_position(w) < 0 && notifies == 2, "negative hides it");
  awra_widget_unref(w);
}

static void inset_shadow(void)
{
  BuiRaster *r = BUI_raster_create(40, 40);
  const float bg[4] = {1, 1, 1, 1}, sh[4] = {0, 0, 0, 0.2f}, rad[4] = {6, 6, 6, 6};
  BUI_raster_rbox(r, 0, 0, 40, 40, rad, bg, NULL, NULL);
  BUI_raster_box_shadow_inset(r, 4, 4, 32, 32, rad, 0, 1, 0, 2, sh);
  const uint32_t *px = BUI_raster_pixels(r);
  const int top = (int)(px[4 * 40 + 20] & 255), mid = (int)(px[20 * 40 + 20] & 255);
  const int out = (int)(px[2 * 40 + 20] & 255);
  CHECK(top < 240 && mid == 255 && out == 255, "inset 0 1px 2px: dark under the top edge only (%d %d %d)",
        top, mid, out);
  BUI_raster_free(r);
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  awra_init("org.awra.TestAwra1Parity");
  action_rows();
  split_view();
  grid_spans();
  button_label();
  modal_windows();
  media_rows();
  waveform_hint();
  inset_shadow();
  awra_pump();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("ok\n");
  return 0;
}
