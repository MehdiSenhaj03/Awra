/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_BUTTON_CONTENT (awra_button_content_get_type ())
G_DECLARE_FINAL_TYPE (AwraButtonContent, awra_button_content, AWRA, BUTTON_CONTENT, GtkWidget)

/**
 * awra_button_content_new:
 * @icon_name: (nullable): the icon shown before the label
 * @label: (nullable): the label
 *
 * Makes a button's content: an icon, then a label. Set it as an
 * #AwraButton's or #AwraToggleButton's child, or use
 * awra_button_new_with_icon_and_label().
 *
 * Returns: (transfer full): a new button content
 *
 * Since: 1.1
 */
GtkWidget  *awra_button_content_new           (const char        *icon_name,
                                               const char        *label);
/**
 * awra_button_content_get_icon_name:
 * @self: a button content
 *
 * Returns: (transfer none) (nullable): the icon shown before the label
 *
 * Since: 1.1
 */
const char *awra_button_content_get_icon_name (AwraButtonContent *self);
/**
 * awra_button_content_set_icon_name:
 * @self: a button content
 * @icon_name: (nullable): the icon shown before the label, or %NULL for none
 *
 * Since: 1.1
 */
void        awra_button_content_set_icon_name (AwraButtonContent *self,
                                               const char        *icon_name);
/**
 * awra_button_content_get_label:
 * @self: a button content
 *
 * Returns: (transfer none) (nullable): the label
 *
 * Since: 1.1
 */
const char *awra_button_content_get_label     (AwraButtonContent *self);
/**
 * awra_button_content_set_label:
 * @self: a button content
 * @label: (nullable): the label, or %NULL for an icon alone
 *
 * Since: 1.1
 */
void        awra_button_content_set_label     (AwraButtonContent *self,
                                               const char        *label);

G_END_DECLS
