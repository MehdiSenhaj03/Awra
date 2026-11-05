/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_GRID_VIEW (awra_grid_view_get_type ())
G_DECLARE_FINAL_TYPE (AwraGridView, awra_grid_view, AWRA, GRID_VIEW, GtkWidget)

/**
 * awra_grid_view_new:
 * @model: (nullable): the selection model
 * @factory: (nullable): the virtualized item factory
 *
 * Creates a scrollable grid with keyboard navigation and rubberband selection.
 * Since: 1.1
 */
GtkWidget         *awra_grid_view_new          (GtkSelectionModel *model,
                                                GtkListItemFactory *factory);
/**
 * awra_grid_view_get_model:
 * @self: an Awra grid view
 *
 * Returns: (transfer none) (nullable): the selection model
 */
GtkSelectionModel *awra_grid_view_get_model    (AwraGridView *self);
/**
 * awra_grid_view_set_model:
 * @self: a grid view
 * @model: (nullable): the selection model
 */
void               awra_grid_view_set_model    (AwraGridView       *self,
                                                GtkSelectionModel  *model);
/**
 * awra_grid_view_get_factory:
 * @self: an Awra grid view
 *
 * Returns: (transfer none) (nullable): the item factory
 */
GtkListItemFactory *awra_grid_view_get_factory (AwraGridView *self);
/**
 * awra_grid_view_set_factory:
 * @self: a grid view
 * @factory: (nullable): the item factory
 */
void                awra_grid_view_set_factory (AwraGridView       *self,
                                                GtkListItemFactory *factory);
/**
 * awra_grid_view_get_delegate:
 * @self: an Awra grid view
 *
 * Returns: (transfer none): the underlying GTK grid view
 */
GtkGridView        *awra_grid_view_get_delegate (AwraGridView *self);

G_END_DECLS
