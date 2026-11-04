/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-window.h>

G_BEGIN_DECLS

#define AWRA_TYPE_COMMAND_PALETTE (awra_command_palette_get_type ())
G_DECLARE_FINAL_TYPE (AwraCommandPalette, awra_command_palette,
                      AWRA, COMMAND_PALETTE, AwraWindow)

/**
 * awra_command_palette_new:
 * @parent: (nullable): the transient parent
 *
 * Returns: (transfer full): a new command palette
 */
GtkWidget  *awra_command_palette_new                  (GtkWindow          *parent);
/**
 * awra_command_palette_get_menu_model:
 * Returns: (transfer none) (nullable): the command menu model
 */
GMenuModel *awra_command_palette_get_menu_model       (AwraCommandPalette *self);
/**
 * awra_command_palette_set_menu_model:
 * @self: a command palette
 * @model: (nullable): commands represented as a #GMenuModel
 *
 * Sections become headings. An item's `icon` shows before its label, an
 * `accel` attribute (an accelerator string) and an `awra-detail` attribute
 * (any short text, such as where the item belongs or how many unread it
 * holds) show after it, muted. The first result is chosen; Up and Down
 * choose another, Enter opens it and Escape closes the palette.
 */
void        awra_command_palette_set_menu_model       (AwraCommandPalette *self,
                                                        GMenuModel         *model);
const char *awra_command_palette_get_title            (AwraCommandPalette *self);
/**
 * awra_command_palette_set_title:
 * @title: (nullable): localized heading, or %NULL for the default
 */
void        awra_command_palette_set_title            (AwraCommandPalette *self,
                                                        const char         *title);
const char *awra_command_palette_get_query            (AwraCommandPalette *self);
void        awra_command_palette_set_query            (AwraCommandPalette *self,
                                                        const char         *query);
const char *awra_command_palette_get_placeholder_text (AwraCommandPalette *self);
void        awra_command_palette_set_placeholder_text (AwraCommandPalette *self,
                                                        const char         *text);
const char *awra_command_palette_get_empty_text       (AwraCommandPalette *self);
/**
 * awra_command_palette_set_empty_text:
 * @text: (nullable): localized empty-result text, or %NULL for the default
 */
void        awra_command_palette_set_empty_text       (AwraCommandPalette *self,
                                                        const char         *text);
void        awra_command_palette_present              (AwraCommandPalette *self);
void        awra_command_palette_close                (AwraCommandPalette *self);

G_END_DECLS
