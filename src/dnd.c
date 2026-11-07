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
