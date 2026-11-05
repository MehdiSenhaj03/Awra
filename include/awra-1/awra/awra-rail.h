/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

#include <awra/awra-rail-group.h>
#include <awra/awra-rail-item.h>

G_BEGIN_DECLS

/**
 * AwraRail:
 *
 * A vertical rail of destinations to switch between, such as the workspaces
 * of a chat application, the accounts of a mail client or the projects of a
 * launcher.
 *
 * The rail holds #AwraRailItem, #AwraRailGroup folders and separators in a
 * column that scrolls, between an optional header and footer that stay in
 * place. Activating an item selects it and deselects the others. Up and Down
 * move the focus from item to item.
 *
 * A reorderable rail lets the user drag an item to another place: the other
 * items make room as it moves, and #AwraRail::item-moved reports the new
 * position. Dropping an item onto another one emits #AwraRail::item-merged
 * instead, for the application to make a folder.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_RAIL (awra_rail_get_type ())
G_DECLARE_FINAL_TYPE (AwraRail, awra_rail, AWRA, RAIL, GtkWidget)

/**
 * awra_rail_new:
 *
 * Returns: a new rail
 *
 * Since: 1.1
 */
GtkWidget *awra_rail_new               (void);
/**
 * awra_rail_append:
 * @self: a rail
 * @widget: an #AwraRailItem or #AwraRailGroup
 *
 * Since: 1.1
 */
void       awra_rail_append            (AwraRail  *self,
                                        GtkWidget *widget);
/**
 * awra_rail_prepend:
 * @self: a rail
 * @widget: an #AwraRailItem or #AwraRailGroup
 *
 * Since: 1.1
 */
void       awra_rail_prepend           (AwraRail  *self,
                                        GtkWidget *widget);
/**
 * awra_rail_insert:
 * @self: a rail
 * @widget: an #AwraRailItem or #AwraRailGroup
 * @position: the position among the rail's entries, or -1 to append
 *
 * Separators count as entries.
 *
 * Since: 1.1
 */
void       awra_rail_insert            (AwraRail  *self,
                                        GtkWidget *widget,
                                        int        position);
/**
 * awra_rail_remove:
 * @self: a rail
 * @widget: an entry of the rail: an item, a group or a separator
 *
 * Since: 1.1
 */
void       awra_rail_remove            (AwraRail  *self,
                                        GtkWidget *widget);
/**
 * awra_rail_add_separator:
 * @self: a rail
 *
 * Appends a short line that separates groups of entries.
 *
 * Returns: (transfer none): the separator, which awra_rail_remove() accepts
 *
 * Since: 1.1
 */
GtkWidget *awra_rail_add_separator     (AwraRail  *self);
/**
 * awra_rail_get_n_entries:
 * @self: a rail
 *
 * Returns: the number of entries, separators included
 *
 * Since: 1.1
 */
guint      awra_rail_get_n_entries     (AwraRail  *self);
/**
 * awra_rail_get_entry:
 * @self: a rail
 * @position: the position of an entry
 *
 * Returns: (transfer none) (nullable): the entry at @position, or %NULL
 *
 * Since: 1.1
 */
GtkWidget *awra_rail_get_entry         (AwraRail  *self,
                                        guint      position);
/**
 * awra_rail_get_selected_item:
 * @self: a rail
 *
 * Returns: (transfer none) (nullable): the selected item, in the rail or in
 *   one of its groups
 *
 * Since: 1.1
 */
AwraRailItem *awra_rail_get_selected_item (AwraRail     *self);
/**
 * awra_rail_set_selected_item:
 * @self: a rail
 * @item: (nullable): an item of the rail or of one of its groups, or %NULL
 *
 * Since: 1.1
 */
void       awra_rail_set_selected_item (AwraRail     *self,
                                        AwraRailItem *item);
/**
 * awra_rail_get_header:
 * @self: a rail
 *
 * Returns: (transfer none) (nullable): the widget above the entries
 *
 * Since: 1.1
 */
GtkWidget *awra_rail_get_header        (AwraRail  *self);
/**
 * awra_rail_set_header:
 * @self: a rail
 * @header: (nullable): a widget that stays above the entries, such as a
 *   home item
 *
 * An #AwraRailItem header takes part in the selection like the entries.
 *
 * Since: 1.1
 */
void       awra_rail_set_header        (AwraRail  *self,
                                        GtkWidget *header);
/**
 * awra_rail_get_footer:
 * @self: a rail
 *
 * Returns: (transfer none) (nullable): the widget below the entries
 *
 * Since: 1.1
 */
GtkWidget *awra_rail_get_footer        (AwraRail  *self);
/**
 * awra_rail_set_footer:
 * @self: a rail
 * @footer: (nullable): a widget that stays below the entries, such as an
 *   item that adds a workspace
 *
 * Since: 1.1
 */
void       awra_rail_set_footer        (AwraRail  *self,
                                        GtkWidget *footer);
/**
 * awra_rail_get_reorderable:
 * @self: a rail
 *
 * Returns: whether the user can drag entries to reorder them
 *
 * Since: 1.1
 */
gboolean   awra_rail_get_reorderable   (AwraRail  *self);
/**
 * awra_rail_set_reorderable:
 * @self: a rail
 * @reorderable: whether the user can drag entries to reorder them
 *
 * Items and groups can be dragged; items inside a group and separators stay
 * in place.
 *
 * Since: 1.1
 */
void       awra_rail_set_reorderable   (AwraRail  *self,
                                        gboolean   reorderable);

G_END_DECLS
