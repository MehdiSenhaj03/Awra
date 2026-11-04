/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_COLOR_PALETTE (awra_color_palette_get_type ())
G_DECLARE_FINAL_TYPE (AwraColorPalette, awra_color_palette, AWRA, COLOR_PALETTE, GtkWidget)

/**
 * awra_color_palette_new:
 *
 * Makes a palette: round swatches to choose a colour from, which wrap onto
 * as many lines as the width needs. It can lead with an automatic choice
 * (#AwraColorPalette:automatic-label), such as "System" or "Default", and
 * end with a swatch that opens the system's colour chooser for any colour
 * (#AwraColorPalette:allows-custom).
 *
 * Returns: (transfer full): a new colour palette
 *
 * Since: 1.1
 */
GtkWidget     *awra_color_palette_new                 (void);
/**
 * awra_color_palette_add_color:
 * @self: a colour palette
 * @color: a colour to offer
 * @name: (nullable): what the colour is called, for its tooltip and for
 *   assistive technologies; its hex code when %NULL
 *
 * Since: 1.1
 */
void           awra_color_palette_add_color           (AwraColorPalette *self,
                                                       const GdkRGBA    *color,
                                                       const char       *name);
/**
 * awra_color_palette_clear:
 * @self: a colour palette
 *
 * Removes every colour added with awra_color_palette_add_color().
 *
 * Since: 1.1
 */
void           awra_color_palette_clear               (AwraColorPalette *self);
/**
 * awra_color_palette_get_selected:
 * @self: a colour palette
 *
 * Returns: (transfer none) (nullable): the chosen colour, or %NULL for the
 *   automatic choice
 *
 * Since: 1.1
 */
const GdkRGBA *awra_color_palette_get_selected        (AwraColorPalette *self);
/**
 * awra_color_palette_set_selected:
 * @self: a colour palette
 * @color: (nullable): the colour to show as chosen, or %NULL for the
 *   automatic choice
 *
 * A colour that isn't one of the palette's shows as a swatch of its own at
 * the end, chosen.
 *
 * Since: 1.1
 */
void           awra_color_palette_set_selected        (AwraColorPalette *self,
                                                       const GdkRGBA    *color);
/**
 * awra_color_palette_get_automatic_label:
 * @self: a colour palette
 *
 * Returns: (transfer none) (nullable): the label of the automatic choice
 *
 * Since: 1.1
 */
const char    *awra_color_palette_get_automatic_label (AwraColorPalette *self);
/**
 * awra_color_palette_set_automatic_label:
 * @self: a colour palette
 * @label: (nullable): the label of a leading choice that picks no colour,
 *   such as "System", or %NULL for none
 *
 * Since: 1.1
 */
void           awra_color_palette_set_automatic_label (AwraColorPalette *self,
                                                       const char       *label);
/**
 * awra_color_palette_get_allows_custom:
 * @self: a colour palette
 *
 * Returns: whether a last swatch offers any colour
 *
 * Since: 1.1
 */
gboolean       awra_color_palette_get_allows_custom   (AwraColorPalette *self);
/**
 * awra_color_palette_set_allows_custom:
 * @self: a colour palette
 * @allows_custom: whether a last swatch opens the system's colour chooser
 *
 * Since: 1.1
 */
void           awra_color_palette_set_allows_custom   (AwraColorPalette *self,
                                                       gboolean          allows_custom);

G_END_DECLS
