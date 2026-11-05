/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <gtk/gtk.h>
G_BEGIN_DECLS
#define AWRA_TYPE_ZOOM_CONTROLLER (awra_zoom_controller_get_type ())
G_DECLARE_FINAL_TYPE (AwraZoomController, awra_zoom_controller, AWRA, ZOOM_CONTROLLER, GObject)
/**
 * awra_zoom_controller_new:
 * @target: the widget whose content is zoomed
 *
 * Captures Ctrl+wheel while leaving ordinary scrolling to GTK. Text scales
 * relative to the inherited desktop font; consumers observe `scale` to resize
 * domain content such as thumbnails. The target is held weakly.
 * Returns: (transfer full): a zoom controller
 * Since: 1.1
 */
AwraZoomController *awra_zoom_controller_new (GtkWidget *target);
/**
 * awra_zoom_controller_get_target:
 * @self: a zoom controller
 * Returns: (transfer none) (nullable): the target, or NULL after its destruction
 * Since: 1.1
 */
GtkWidget *awra_zoom_controller_get_target (AwraZoomController *self);
/**
 * awra_zoom_controller_get_scale:
 * @self: a zoom controller
 * Returns: the relative content scale
 * Since: 1.1
 */
double awra_zoom_controller_get_scale (AwraZoomController *self);
/**
 * awra_zoom_controller_get_scale_text:
 * @self: a zoom controller
 * Returns: whether zoom also scales inherited text
 * Since: 1.1
 */
gboolean awra_zoom_controller_get_scale_text (AwraZoomController *self);
/**
 * awra_zoom_controller_set_scale_text:
 * @self: a zoom controller
 * @scale_text: whether to scale text, true by default
 *
 * Disable for collections whose zoom changes item geometry while preserving
 * readable labels, column headings and metadata at the desktop font size.
 * Since: 1.1
 */
void awra_zoom_controller_set_scale_text (AwraZoomController *self, gboolean scale_text);
/**
 * awra_zoom_controller_set_scale:
 * @self: a zoom controller
 * @scale: the requested scale, clamped to the configured range
 * Since: 1.1
 */
void awra_zoom_controller_set_scale (AwraZoomController *self, double scale);
/**
 * awra_zoom_controller_set_range:
 * @self: a zoom controller
 * @minimum: the smallest scale, at least 0.05
 * @maximum: the largest scale, at most 64 and no smaller than @minimum
 * Since: 1.1
 */
void awra_zoom_controller_set_range (AwraZoomController *self, double minimum, double maximum);
/**
 * awra_zoom_controller_zoom_in:
 * @self: a zoom controller
 * Since: 1.1
 */
void awra_zoom_controller_zoom_in (AwraZoomController *self);
/**
 * awra_zoom_controller_zoom_out:
 * @self: a zoom controller
 * Since: 1.1
 */
void awra_zoom_controller_zoom_out (AwraZoomController *self);
/**
 * awra_zoom_controller_reset:
 * @self: a zoom controller
 * Since: 1.1
 */
void awra_zoom_controller_reset (AwraZoomController *self);
/**
 * awra_zoom_controller_handle_scroll:
 * @self: a zoom controller
 * @delta: a vertical wheel delta; negative zooms in
 * @modifiers: current input modifiers
 *
 * Public input adapter for backends and tests. The attached GTK controller
 * already calls it for normal pointer input.
 * Returns: whether the event was consumed as a zoom gesture
 * Since: 1.1
 */
gboolean awra_zoom_controller_handle_scroll (AwraZoomController *self, double delta, GdkModifierType modifiers);
G_END_DECLS
