/* Widget snapshots (GtkWidgetPaintable) and the drag image that follows the
 * pointer over a window, drawn above everything and repainted where it
 * moves.
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

/* the window's frame pixel at x, y (premultiplied 0xAARRGGBB) */
static uint32_t frame_px(AwraWidget *win, int x, int y)
{
  BKContext *ctx = awra_ctx(win);
  const uint32_t *px = BUI_raster_pixels(ctx->raster);
  return px[(size_t)y * BUI_raster_width(ctx->raster) + x];
}

int main(void)
{
  awra_init("org.awra.TestDragIcon");
  AwraWidget *win = awra_window_new_offscreen("drag", 400, 300, 1.0);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *button = awra_button_new_with_label("Drag me");
  awra_widget_set_halign(button, AWRA_ALIGN_START);
  awra_widget_set_valign(button, AWRA_ALIGN_START);
  awra_box_append(box, button);
  awra_window_set_child(win, box);
  awra_widget_unref(button); /* the tree holds them */
  awra_widget_unref(box);
  awra_window_render_offscreen(win, true);

  /* a snapshot: the button's size, its pixels where it draws, transparent
   * around its rounded corners */
  AwraTexture *snap = awra_widget_snapshot(button);
  CHECK(snap != NULL, "no snapshot");
  if (snap) {
    const int w = awra_texture_width(snap), h = awra_texture_height(snap);
    CHECK(w == button->w && h == button->h, "snapshot %dx%d, button %dx%d", w, h, button->w,
          button->h);
    uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
    awra_texture_download(snap, rgba, w * 4);
    int opaque = 0;
    for (int i = 0; i < w * h; i++) {
      opaque += rgba[i * 4 + 3] > 8;
    }
    CHECK(opaque > w * h / 2, "the snapshot is mostly empty (%d of %d)", opaque, w * h);
    CHECK(rgba[3] < 255, "the corner is not transparent");
    free(rgba);
  }
  /* the window itself is untouched by the snapshot */
  const uint32_t before = frame_px(win, 300, 200);

  /* the image under the pointer, hot spot (5, 5) */
  BKContext *ctx = awra_ctx(win);
  BK_motion(ctx, 200, 150, 0);
  awra_window_set_drag_icon(win, snap, 5, 5);
  awra_window_render_offscreen(win, false);
  const uint32_t at = frame_px(win, 200 - 5 + 20, 150 - 5 + button->h / 2);
  CHECK(at != before, "the image isn't drawn under the pointer");
  /* it moves with the pointer: the old place is clean again */
  BK_motion(ctx, 300, 250, 0);
  awra_window_render_offscreen(win, false);
  CHECK(frame_px(win, 200 - 5 + 20, 150 - 5 + button->h / 2) == before,
        "the image stayed where the pointer was");
  CHECK(frame_px(win, 300 - 5 + 20, 250 - 5 + button->h / 2) != before, "the image didn't move");
  /* taken away */
  awra_window_set_drag_icon(win, NULL, 0, 0);
  awra_window_render_offscreen(win, false);
  CHECK(frame_px(win, 300 - 5 + 20, 250 - 5 + button->h / 2) == before, "the image stayed");
  /* default images */
  AwraTexture *label = awra_texture_new_for_text("Some text");
  CHECK(label && awra_texture_width(label) > 20, "no label image");
  AwraTexture *icon = awra_texture_new_for_icon("text-x-generic-symbolic", 32);
  CHECK(icon && awra_texture_width(icon) == 32, "no icon image");
  awra_texture_unref(label);
  awra_texture_unref(icon);
  awra_texture_unref(snap);
  awra_widget_destroy(win);
  if (failures) {
    return 1;
  }
  printf("drag icon ok\n");
  return 0;
}
