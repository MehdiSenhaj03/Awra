/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * AwraRailItem:
 *
 * One destination in an #AwraRail, such as a workspace, a server or an
 * account.
 *
 * The item shows its child, usually an #AwraAvatar or a #GtkImage, in a tile
 * that is round at rest and becomes a rounded square when hovered or
 * selected. A pill on the start edge tells the state of the destination: a
 * dot when it has unread activity, a taller pill on hover, a full pill when
 * selected. A count badge announces mentions. Every change moves on a
 * spring, so quick pointer moves never make the tile or the pill jump.
 *
 * The item is focusable; Enter and Space activate it. Its label is shown in
 * a tooltip beside the rail and is its accessible name.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_RAIL_ITEM (awra_rail_item_get_type ())
G_DECLARE_FINAL_TYPE (AwraRailItem, awra_rail_item, AWRA, RAIL_ITEM, GtkWidget)

/**
 * awra_rail_item_new:
 * @label: (nullable): the name of the destination
 *
 * Returns: a new rail item
 *
 * Since: 1.1
 */
GtkWidget  *awra_rail_item_new               (const char   *label);
/**
 * awra_rail_item_get_label:
 * @self: a rail item
 *
 * Returns: (nullable): the name of the destination
 *
 * Since: 1.1
 */
const char *awra_rail_item_get_label         (AwraRailItem *self);
/**
 * awra_rail_item_set_label:
 * @self: a rail item
 * @label: (nullable): the name of the destination, shown in the tooltip and
 *   used as accessible name
 *
 * Since: 1.1
 */
void        awra_rail_item_set_label         (AwraRailItem *self,
                                              const char   *label);
/**
 * awra_rail_item_get_child:
 * @self: a rail item
 *
 * Returns: (transfer none) (nullable): the content of the tile
 *
 * Since: 1.1
 */
GtkWidget  *awra_rail_item_get_child         (AwraRailItem *self);
/**
 * awra_rail_item_set_child:
 * @self: a rail item
 * @child: (nullable): the content of the tile
 *
 * The child fills the tile and is clipped to its shape. An #AwraAvatar child
 * takes the size of the tile and the rounded shape, so that it follows the
 * tile as it changes shape.
 *
 * Since: 1.1
 */
void        awra_rail_item_set_child         (AwraRailItem *self,
                                              GtkWidget    *child);
/**
 * awra_rail_item_get_icon_name:
 * @self: a rail item
 *
 * Returns: (nullable): the icon shown when there is no child
 *
 * Since: 1.1
 */
const char *awra_rail_item_get_icon_name     (AwraRailItem *self);
/**
 * awra_rail_item_set_icon_name:
 * @self: a rail item
 * @icon_name: (nullable): a symbolic icon shown on a plain tile when there is
 *   no child, as for a home or an add action
 *
 * Since: 1.1
 */
void        awra_rail_item_set_icon_name     (AwraRailItem *self,
                                              const char   *icon_name);
/**
 * awra_rail_item_get_selected:
 * @self: a rail item
 *
 * Returns: whether the item is the current destination
 *
 * Since: 1.1
 */
gboolean    awra_rail_item_get_selected      (AwraRailItem *self);
/**
 * awra_rail_item_set_selected:
 * @self: a rail item
 * @selected: whether the item is the current destination
 *
 * In an #AwraRail, selecting an item deselects the others.
 *
 * Since: 1.1
 */
void        awra_rail_item_set_selected      (AwraRailItem *self,
                                              gboolean      selected);
/**
 * awra_rail_item_get_unread:
 * @self: a rail item
 *
 * Returns: whether the destination has unread activity
 *
 * Since: 1.1
 */
gboolean    awra_rail_item_get_unread        (AwraRailItem *self);
/**
 * awra_rail_item_set_unread:
 * @self: a rail item
 * @unread: whether the destination has unread activity
 *
 * Since: 1.1
 */
void        awra_rail_item_set_unread        (AwraRailItem *self,
                                              gboolean      unread);
/**
 * awra_rail_item_get_mention_count:
 * @self: a rail item
 *
 * Returns: the number of mentions waiting
 *
 * Since: 1.1
 */
guint       awra_rail_item_get_mention_count (AwraRailItem *self);
/**
 * awra_rail_item_set_mention_count:
 * @self: a rail item
 * @count: the number of mentions waiting, 0 for none
 *
 * A count shows a badge at the bottom end corner of the tile. It pops in
 * when it appears and nods when it changes. Counts above 99 show as "99+".
 *
 * Since: 1.1
 */
void        awra_rail_item_set_mention_count (AwraRailItem *self,
                                              guint         count);
/**
 * awra_rail_item_get_muted:
 * @self: a rail item
 *
 * Returns: whether the destination is muted
 *
 * Since: 1.1
 */
gboolean    awra_rail_item_get_muted         (AwraRailItem *self);
/**
 * awra_rail_item_set_muted:
 * @self: a rail item
 * @muted: whether the destination is muted
 *
 * A muted item is dimmed and hides its unread pill. Mentions still show.
 *
 * Since: 1.1
 */
void        awra_rail_item_set_muted         (AwraRailItem *self,
                                              gboolean      muted);

G_END_DECLS
