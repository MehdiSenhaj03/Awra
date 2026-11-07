/* What Awra draws through BUI's raster, shifted by the draw translation of
 * the subtree being drawn (awra_draw_shift: GTK's fractional child
 * transforms, such as an AwraPage's column centred at x.5). Every Awra
 * source calls the raster through these; BUI's own internal calls don't
 * (they would shift twice).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

extern float awra_shift_x, awra_shift_y;

static inline int awra_shift_ix(void)
{
  return (int)floorf(awra_shift_x + 0.5f);
}
static inline int awra_shift_iy(void)
{
  return (int)floorf(awra_shift_y + 0.5f);
}

#define AWRA_SHIFT_XY(fn, r, x, y, ...) fn(r, (x) + awra_shift_x, (y) + awra_shift_y, __VA_ARGS__)

#define SR_fill_rect(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_fill_rect, r, x, y, __VA_ARGS__)
#define SR_frame_rect(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_frame_rect, r, x, y, __VA_ARGS__)
#define SR_round_rect(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_round_rect, r, x, y, __VA_ARGS__)
#define SR_circle(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_circle, r, x, y, __VA_ARGS__)
#define SR_ellipse(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_ellipse, r, x, y, __VA_ARGS__)
#define SR_image(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_image, r, x, y, __VA_ARGS__)
#define SR_gradient(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_gradient, r, x, y, __VA_ARGS__)
#define SR_text(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_text, r, x, y, __VA_ARGS__)
#define SR_text_part(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_text_part, r, x, y, __VA_ARGS__)
#define SR_text_run(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_text_run, r, x, y, __VA_ARGS__)
#define SR_rbox(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_rbox, r, x, y, __VA_ARGS__)
#define SR_box_shadow(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_box_shadow, r, x, y, __VA_ARGS__)
#define SR_box_shadow_inset(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_box_shadow_inset, r, x, y, __VA_ARGS__)
#define SR_image_rounded(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_image_rounded, r, x, y, __VA_ARGS__)
#define SR_rbox_gradient(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_rbox_gradient, r, x, y, __VA_ARGS__)
#define SR_corners_save(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_corners_save, r, x, y, __VA_ARGS__)
#define SR_corners_clip(r, x, y, ...) AWRA_SHIFT_XY(BUI_raster_corners_clip, r, x, y, __VA_ARGS__)
#define SR_line(r, x1, y1, x2, y2, ...) \
  BUI_raster_line(r, (x1) + awra_shift_x, (y1) + awra_shift_y, (x2) + awra_shift_x, (y2) + awra_shift_y, __VA_ARGS__)
/* whole-pixel regions follow the shift rounded */
#define SR_read(r, x, y, ...) BUI_raster_read(r, (x) + awra_shift_ix(), (y) + awra_shift_iy(), __VA_ARGS__)
#define SR_fade(r, x, y, ...) BUI_raster_fade(r, (x) + awra_shift_ix(), (y) + awra_shift_iy(), __VA_ARGS__)
#define SR_clear(r, x, y, ...) BUI_raster_clear(r, (x) + awra_shift_ix(), (y) + awra_shift_iy(), __VA_ARGS__)
void SR_polygon(BuiRaster *r, const float (*pts)[2], int n, const float rgba[4]);
void SR_mask(BuiRaster *r, int x, int y, const uint8_t *mask, int w, int h, int stride, const float rgba[4]);
