/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>
#include <awra/awra-enums.h>

G_BEGIN_DECLS

#define AWRA_TYPE_DIVIDER (awra_divider_get_type ())
/**
 * AwraDivider:
 *
 * A horizontal line that separates content, with an optional label: the day
 * above a group of messages, or where unread messages start.
 *
 * It is a separator for assistive technologies, named by its label.
 *
 * Since: 1.1
 */
G_DECLARE_FINAL_TYPE (AwraDivider, awra_divider, AWRA, DIVIDER, GtkWidget)

/**
 * awra_divider_new:
 * @label: (nullable): the label, or %NULL for a plain line
 *
 * Returns: a new divider
 *
 * Since: 1.1
 */
GtkWidget                *awra_divider_new                (const char               *label);
/**
 * awra_divider_get_label:
 * @self: a divider
 *
 * Returns: (transfer none) (nullable): the label
 *
 * Since: 1.1
 */
const char               *awra_divider_get_label          (AwraDivider              *self);
/**
 * awra_divider_set_label:
 * @self: a divider
 * @label: (nullable): the label, or %NULL for a plain line
 *
 * Since: 1.1
 */
void                      awra_divider_set_label          (AwraDivider              *self,
                                                           const char               *label);
/**
 * awra_divider_get_label_position:
 * @self: a divider
 *
 * Returns: where the label sits
 *
 * Since: 1.1
 */
AwraDividerLabelPosition  awra_divider_get_label_position (AwraDivider              *self);
/**
 * awra_divider_set_label_position:
 * @self: a divider
 * @position: where the label sits
 *
 * Since: 1.1
 */
void                      awra_divider_set_label_position (AwraDivider              *self,
                                                           AwraDividerLabelPosition  position);
/**
 * awra_divider_get_appearance:
 * @self: a divider
 *
 * Returns: how the divider looks
 *
 * Since: 1.1
 */
AwraDividerAppearance     awra_divider_get_appearance     (AwraDivider              *self);
/**
 * awra_divider_set_appearance:
 * @self: a divider
 * @appearance: how the divider looks
 *
 * Since: 1.1
 */
void                      awra_divider_set_appearance     (AwraDivider              *self,
                                                           AwraDividerAppearance     appearance);

G_END_DECLS
