/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gdk/gdk.h>
#include <awra/awra-enums.h>

G_BEGIN_DECLS

#define AWRA_TYPE_TOKEN_SET (awra_token_set_get_type ())
G_DECLARE_FINAL_TYPE (AwraTokenSet, awra_token_set, AWRA, TOKEN_SET, GObject)

/**
 * awra_token_set_new:
 * @dark: whether to create the dark profile
 * @accent: (nullable): an accent color, or %NULL for Awra purple
 * @high_contrast: whether to strengthen contrast and borders
 *
 * Returns: (transfer full): a new immutable token snapshot
 */
AwraTokenSet *awra_token_set_new                 (gboolean       dark,
                                                  const GdkRGBA *accent,
                                                  gboolean       high_contrast);
gboolean      awra_token_set_get_dark            (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_background     (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_foreground     (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_accent         (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_accent_content (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_border         (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_muted_foreground (AwraTokenSet *self);
const GdkRGBA *awra_token_set_get_control_background (AwraTokenSet *self);
const GdkRGBA *awra_token_set_get_control_hover  (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_control_pressed (AwraTokenSet *self);
const GdkRGBA *awra_token_set_get_control_well   (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_control_cap    (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_control_cap_hover (AwraTokenSet *self);
const GdkRGBA *awra_token_set_get_control_thumb  (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_control_track  (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_selection      (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_focus_ring     (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_disabled_foreground (AwraTokenSet *self);
const GdkRGBA *awra_token_set_get_on_accent       (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_danger          (AwraTokenSet  *self);
/**
 * awra_token_set_get_success:
 * @self: a token set
 *
 * The colour of a positive or available state, strong enough for a small
 * indicator such as a presence dot.
 *
 * Returns: (transfer none): the success colour
 *
 * Since: 1.1
 */
const GdkRGBA *awra_token_set_get_success         (AwraTokenSet  *self);
/**
 * awra_token_set_get_warning:
 * @self: a token set
 *
 * The colour of a state that needs attention without being critical.
 *
 * Returns: (transfer none): the warning colour
 *
 * Since: 1.1
 */
const GdkRGBA *awra_token_set_get_warning         (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_accent_soft     (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_surface_canvas  (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_surface_content (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_surface_chrome  (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_surface_layer   (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_surface_floating (AwraTokenSet *self);
const GdkRGBA *awra_token_set_get_surface_opaque  (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_outline         (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_separator       (AwraTokenSet  *self);
const GdkRGBA *awra_token_set_get_rim_highlight   (AwraTokenSet  *self);
double        awra_token_set_get_window_opacity  (AwraTokenSet  *self);
double        awra_token_set_get_surface_opacity (AwraTokenSet  *self);
double        awra_token_set_get_radius          (AwraTokenSet  *self);
double        awra_token_set_get_border_width    (AwraTokenSet  *self);
double        awra_token_set_get_spacing         (AwraTokenSet  *self,
                                                   AwraSpacing    spacing);
double        awra_token_set_get_radius_for_size (AwraTokenSet  *self,
                                                   AwraRadius     radius);
double        awra_token_set_get_control_height  (AwraTokenSet  *self);
double        awra_token_set_get_navigation_height (AwraTokenSet *self);
double        awra_token_set_get_header_height   (AwraTokenSet  *self);
double        awra_token_set_get_grain_scale     (AwraTokenSet  *self);
gboolean      awra_token_set_get_high_contrast   (AwraTokenSet  *self);

G_END_DECLS
