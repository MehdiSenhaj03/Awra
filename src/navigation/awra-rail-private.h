/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-rail.h>

G_BEGIN_DECLS

/* The side of a tile and the inset that leaves room for the pill. */
double awra_rail_tile_size  (GtkWidget *widget);
double awra_rail_tile_inset (GtkWidget *widget);
/* The vertical gap between two entries. */
double awra_rail_entry_gap  (GtkWidget *widget);

/* A group's own tile: shows a summary, toggles the group, and is never the
 * rail's selection. */
void   awra_rail_item_set_group_header (AwraRailItem  *self,
                                        gboolean       expanded);
void   awra_rail_item_set_tint         (AwraRailItem  *self,
                                        const GdkRGBA *tint);
/* Drag feedback while the rail is being reordered. */
void   awra_rail_item_set_lifted       (AwraRailItem  *self,
                                        gboolean       lifted);
void   awra_rail_item_set_drop_target  (AwraRailItem  *self,
                                        gboolean       target);

/* The rail decides what an activation selects. */
void   awra_rail_item_activated        (AwraRail      *rail,
                                        AwraRailItem  *item);
void   awra_rail_item_selection_changed (AwraRail     *rail,
                                         AwraRailItem *item);

gboolean awra_rail_is_dragging          (AwraRail     *rail);

/* The tile of a group, for drag and drop and keyboard focus. */
AwraRailItem *awra_rail_group_get_header (AwraRailGroup *self);

G_END_DECLS
