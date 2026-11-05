/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

#include <awra/awra-rail-item.h>

G_BEGIN_DECLS

/**
 * AwraRailGroup:
 *
 * A folder of #AwraRailItem in an #AwraRail.
 *
 * Collapsed, the group is a single tile that previews its first four items
 * in a small grid, or shows its icon, and sums up their unread activity and
 * mentions. Activating it expands it: its items unfold below the tile on a
 * tinted background, and fold back when it is activated again.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_RAIL_GROUP (awra_rail_group_get_type ())
G_DECLARE_FINAL_TYPE (AwraRailGroup, awra_rail_group, AWRA, RAIL_GROUP, GtkWidget)

/**
 * awra_rail_group_new:
 * @label: (nullable): the name of the folder
 *
 * Returns: a new rail group
 *
 * Since: 1.1
 */
GtkWidget     *awra_rail_group_new           (const char    *label);
/**
 * awra_rail_group_get_label:
 * @self: a rail group
 *
 * Returns: (nullable): the name of the folder
 *
 * Since: 1.1
 */
const char    *awra_rail_group_get_label     (AwraRailGroup *self);
/**
 * awra_rail_group_set_label:
 * @self: a rail group
 * @label: (nullable): the name of the folder
 *
 * Since: 1.1
 */
void           awra_rail_group_set_label     (AwraRailGroup *self,
                                              const char    *label);
/**
 * awra_rail_group_get_icon_name:
 * @self: a rail group
 *
 * Returns: (nullable): the icon that replaces the preview of the items
 *
 * Since: 1.1
 */
const char    *awra_rail_group_get_icon_name (AwraRailGroup *self);
/**
 * awra_rail_group_set_icon_name:
 * @self: a rail group
 * @icon_name: (nullable): an icon that replaces the preview of the items, or
 *   %NULL to preview them
 *
 * Since: 1.1
 */
void           awra_rail_group_set_icon_name (AwraRailGroup *self,
                                              const char    *icon_name);
/**
 * awra_rail_group_get_color:
 * @self: a rail group
 *
 * Returns: (nullable): the colour of the folder
 *
 * Since: 1.1
 */
const GdkRGBA *awra_rail_group_get_color     (AwraRailGroup *self);
/**
 * awra_rail_group_set_color:
 * @self: a rail group
 * @color: (nullable): the colour of the folder, or %NULL for a neutral one
 *
 * Tints the collapsed tile and the background of the expanded folder.
 *
 * Since: 1.1
 */
void           awra_rail_group_set_color     (AwraRailGroup *self,
                                              const GdkRGBA *color);
/**
 * awra_rail_group_get_expanded:
 * @self: a rail group
 *
 * Returns: whether the items are shown
 *
 * Since: 1.1
 */
gboolean       awra_rail_group_get_expanded  (AwraRailGroup *self);
/**
 * awra_rail_group_set_expanded:
 * @self: a rail group
 * @expanded: whether to show the items
 *
 * Since: 1.1
 */
void           awra_rail_group_set_expanded  (AwraRailGroup *self,
                                              gboolean       expanded);
/**
 * awra_rail_group_append:
 * @self: a rail group
 * @item: an item to add at the end
 *
 * Since: 1.1
 */
void           awra_rail_group_append        (AwraRailGroup *self,
                                              AwraRailItem  *item);
/**
 * awra_rail_group_insert:
 * @self: a rail group
 * @item: an item to add
 * @position: the position of the item, or -1 to append
 *
 * Since: 1.1
 */
void           awra_rail_group_insert        (AwraRailGroup *self,
                                              AwraRailItem  *item,
                                              int            position);
/**
 * awra_rail_group_remove:
 * @self: a rail group
 * @item: an item of the group
 *
 * Since: 1.1
 */
void           awra_rail_group_remove        (AwraRailGroup *self,
                                              AwraRailItem  *item);
/**
 * awra_rail_group_get_n_items:
 * @self: a rail group
 *
 * Returns: the number of items in the group
 *
 * Since: 1.1
 */
guint          awra_rail_group_get_n_items   (AwraRailGroup *self);
/**
 * awra_rail_group_get_item:
 * @self: a rail group
 * @position: the position of an item
 *
 * Returns: (transfer none) (nullable): the item at @position, or %NULL
 *
 * Since: 1.1
 */
AwraRailItem  *awra_rail_group_get_item      (AwraRailGroup *self,
                                              guint          position);

G_END_DECLS
