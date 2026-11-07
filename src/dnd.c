/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Drag and drop within an application: the destination under a drag
 * (GTK's :drop(active) state). */

#include "awra-private.h"

static int drop_bit(void)
{
  static int bit = -2;
  if (bit == -2) {
    bit = awra_class_bit("drop-active");
  }
  return bit;
}

void awra_widget_set_drop_active(AwraWidget *w, bool active)
{
  if (!w || !awra_is_widget(w) || awra_widget_drop_active(w) == active) {
    return;
  }
  if (active) {
    awra_widget_add_class(w, "drop-active");
  }
  else {
    awra_widget_remove_class(w, "drop-active");
  }
  awra_invalidate(w);
}

bool awra_widget_drop_active(AwraWidget *w)
{
  const int bit = drop_bit();
  return w && awra_is_widget(w) && bit >= 0 && awra_has_class_bit(w, bit);
}

/* An in-application drag (GTK's GdkDrag on X11 and Wayland): the pointer
 * shows what a drop would do (copy, dnd-move, no-drop as the compositor's
 * and Adwaita's cursors), and the widgets under it get no hover: the drag
 * holds the pointer (GDK_CROSSING_GRAB), so prelights end when it starts
 * and come back where the pointer is when it ends. */
bool awra_window_dragging(AwraWidget *win)
{
  return win && awra_widget_get_data(win, "awra-dragging") != NULL;
}

void awra_window_set_drag_action(AwraWidget *win, int action)
{
  if (!win || !awra_widget_is(win, "AwraWindow")) {
    return;
  }
  BKContext *ctx = awra_ctx(win);
  const bool was = awra_window_dragging(win);
  if (action < 0) {
    if (!was) {
      return;
    }
    awra_widget_set_data(win, "awra-dragging", NULL, NULL);
    if (ctx) {
      bk_set_drag_cursor(ctx, -1);
    }
    awra_window_hover_reset(win, false);
    return;
  }
  if (!was) {
    awra_widget_set_data(win, "awra-dragging", (void *)1, NULL);
    awra_window_hover_reset(win, true);
  }
  if (ctx) {
    bk_set_drag_cursor(ctx, action == AWRA_DRAG_ACTION_MOVE   ? BK_CURSOR_DND_MOVE :
                            action == AWRA_DRAG_ACTION_COPY   ? BK_CURSOR_COPY :
                                                                BK_CURSOR_NO_DROP);
  }
}

/* GTK's default theme draws a drop destination with
 *   border-color: $drop_target_color;
 *   box-shadow: inset 0 0 0 1px $drop_target_color;
 * ($drop_target_color: #2ec27e light, #26a269 dark), under the children.
 * Awra 1 keeps it: its stylesheet does not restyle :drop(active). */
void awra_paint_drop_active(AwraDraw *d, AwraWidget *w)
{
  const int bit = drop_bit();
  if (bit < 0 || !awra_has_class_bit(w, bit)) {
    return;
  }
  const AwraRGBA c = awra_tokens()->dark ? awra_rgba(0x26 / 255.0f, 0xa2 / 255.0f, 0x69 / 255.0f, 1.0f)
                                         : awra_rgba(0x2e / 255.0f, 0xc2 / 255.0f, 0x7e / 255.0f, 1.0f);
  awra_paint_rbox(d, awra_box_of(w), 0.0f, NULL, awra_pxf(1), &c);
}
