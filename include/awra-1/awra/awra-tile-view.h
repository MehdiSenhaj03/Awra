/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_TILE_VIEW (awra_tile_view_get_type ())
G_DECLARE_FINAL_TYPE (AwraTileView, awra_tile_view, AWRA, TILE_VIEW, GtkWidget)

/**
 * awra_tile_view_new:
 *
 * A surface divided into as many tiles as the user wants. Each child is a tile;
 * a tile can be divided again in either direction, and dividers between tiles
 * resize their two neighbours, or even out their row on a double click.
 *
 * Every child stays a direct child of the view, whatever the arrangement: it is
 * never reparented, so it keeps its state when tiles are added, removed or
 * moved. Those changes animate from what is on screen, without ever giving a
 * child less than its minimum size, and follow the reduced-motion preference.
 *
 * Sides are physical: %GTK_POS_LEFT stays on the left in a right-to-left locale.
 * Children are expected to stay visible.
 *
 * Returns: a new tile view
 *
 * Since: 1.1
 */
GtkWidget *awra_tile_view_new              (void);
/**
 * awra_tile_view_insert:
 * @self: a tile view
 * @child: the widget to add
 * @beside: (nullable): the child that gives up half of its space, or %NULL to
 *   add a tile along a whole side of the view
 * @side: the side of @beside, or of the view, on which @child goes
 *
 * Since: 1.1
 */
void       awra_tile_view_insert           (AwraTileView    *self,
                                            GtkWidget       *child,
                                            GtkWidget       *beside,
                                            GtkPositionType  side);
/**
 * awra_tile_view_remove:
 * @self: a tile view
 * @child: the child to remove
 *
 * Removes @child at once and shares its space among its neighbours; what was on
 * screen fades away behind them.
 *
 * Since: 1.1
 */
void       awra_tile_view_remove           (AwraTileView    *self,
                                            GtkWidget       *child);
/**
 * awra_tile_view_move:
 * @self: a tile view
 * @child: the child to move
 * @beside: (nullable): another child, or %NULL for a whole side of the view
 * @side: the side on which @child goes
 *
 * Moves a tile to another place, as awra_tile_view_insert() would have put it.
 * It travels there above the other tiles.
 *
 * Since: 1.1
 */
void       awra_tile_view_move             (AwraTileView    *self,
                                            GtkWidget       *child,
                                            GtkWidget       *beside,
                                            GtkPositionType  side);
/**
 * awra_tile_view_get_children:
 * @self: a tile view
 *
 * Returns: (transfer container) (element-type GtkWidget): the children, in
 *   reading order
 *
 * Since: 1.1
 */
GList     *awra_tile_view_get_children     (AwraTileView    *self);
/**
 * awra_tile_view_get_child_bounds:
 * @self: a tile view
 * @child: a child
 * @bounds: (out): where @child is, or is going, in the coordinates of @self
 *
 * Returns: %TRUE when the view has a size and @bounds was set
 *
 * Since: 1.1
 */
gboolean   awra_tile_view_get_child_bounds (AwraTileView    *self,
                                            GtkWidget       *child,
                                            GdkRectangle    *bounds);
/**
 * awra_tile_view_touches_edge:
 * @self: a tile view
 * @child: a child
 * @side: a side of the view
 *
 * Returns: %TRUE when @child runs along that side of the view
 *
 * Since: 1.1
 */
gboolean   awra_tile_view_touches_edge     (AwraTileView    *self,
                                            GtkWidget       *child,
                                            GtkPositionType  side);
/**
 * awra_tile_view_compute_insertion:
 * @self: a tile view
 * @beside: (nullable): a child, or %NULL for a whole side of the view
 * @side: the side to insert on
 * @bounds: (out): where a new tile would go, in the coordinates of @self
 *
 * Tells where awra_tile_view_insert() would put a tile, to preview it. A new
 * tile is taken to need as much room as the tiles it joins.
 *
 * Returns: %TRUE when a tile fits there
 *
 * Since: 1.1
 */
gboolean   awra_tile_view_compute_insertion (AwraTileView    *self,
                                             GtkWidget       *beside,
                                             GtkPositionType  side,
                                             GdkRectangle    *bounds);
/**
 * awra_tile_view_equalize:
 * @self: a tile view
 *
 * Evens out the tiles of every row and column.
 *
 * Since: 1.1
 */
void       awra_tile_view_equalize         (AwraTileView    *self);
/**
 * awra_tile_view_get_arrangement:
 * @self: a tile view
 *
 * Returns: (transfer floating): the shape of the view and the share of each
 *   tile, to keep and give back to awra_tile_view_set_arrangement()
 *
 * Since: 1.1
 */
GVariant  *awra_tile_view_get_arrangement  (AwraTileView    *self);
/**
 * awra_tile_view_set_arrangement:
 * @self: a tile view
 * @arrangement: an arrangement
 *
 * Arranges the current children, taken in reading order, as @arrangement says.
 * This does not animate.
 *
 * Returns: %TRUE when @arrangement is valid and has as many tiles as there are
 *   children; otherwise nothing changes
 *
 * Since: 1.1
 */
gboolean   awra_tile_view_set_arrangement  (AwraTileView    *self,
                                            GVariant        *arrangement);
/**
 * awra_tile_view_get_column_spacing:
 * @self: a tile view
 *
 * Returns: the gap between columns, which their divider is centered on
 *
 * Since: 1.1
 */
int        awra_tile_view_get_column_spacing (AwraTileView  *self);
/**
 * awra_tile_view_set_column_spacing:
 * @self: a tile view
 * @spacing: the gap between columns, in pixels
 *
 * Since: 1.1
 */
void       awra_tile_view_set_column_spacing (AwraTileView  *self,
                                              int            spacing);
/**
 * awra_tile_view_get_row_spacing:
 * @self: a tile view
 *
 * Returns: the gap between rows, which their divider is centered on
 *
 * Since: 1.1
 */
int        awra_tile_view_get_row_spacing  (AwraTileView    *self);
/**
 * awra_tile_view_set_row_spacing:
 * @self: a tile view
 * @spacing: the gap between rows, in pixels
 *
 * Since: 1.1
 */
void       awra_tile_view_set_row_spacing  (AwraTileView    *self,
                                            int              spacing);

G_END_DECLS
