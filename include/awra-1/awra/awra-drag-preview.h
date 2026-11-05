/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_DRAG_PREVIEW (awra_drag_preview_get_type ())
G_DECLARE_FINAL_TYPE (AwraDragPreview, awra_drag_preview, AWRA, DRAG_PREVIEW, GtkWidget)

/**
 * awra_drag_preview_new:
 *
 * A drag icon that shows what is being carried: one item as itself, several
 * as a pile that fans out, with the total in a badge. Set it as the child of
 * a #GtkDragIcon.
 *
 * Returns: a new drag preview
 *
 * Since: 1.1
 */
GtkWidget *awra_drag_preview_new            (void);
/**
 * awra_drag_preview_get_icon_size:
 * @self: a drag preview
 *
 * Returns: the size of each item, in pixels
 *
 * Since: 1.1
 */
int        awra_drag_preview_get_icon_size  (AwraDragPreview *self);
/**
 * awra_drag_preview_set_icon_size:
 * @self: a drag preview
 * @size: the size of each item, in pixels
 *
 * Since: 1.1
 */
void       awra_drag_preview_set_icon_size  (AwraDragPreview *self,
                                             int              size);
/**
 * awra_drag_preview_get_count:
 * @self: a drag preview
 *
 * Returns: the number of carried items
 *
 * Since: 1.1
 */
guint      awra_drag_preview_get_count      (AwraDragPreview *self);
/**
 * awra_drag_preview_set_count:
 * @self: a drag preview
 * @count: the number of carried items, which can exceed the visible pile
 *
 * Since: 1.1
 */
void       awra_drag_preview_set_count      (AwraDragPreview *self,
                                             guint            count);
/**
 * awra_drag_preview_set_item:
 * @self: a drag preview
 * @index: 0 for the front of the pile, up to 2 for the back
 * @paintable: (nullable): the image of the item
 *
 * Replacing an item is cheap, so a thumbnail can take over from an icon while
 * the drag is in progress.
 *
 * Since: 1.1
 */
void       awra_drag_preview_set_item       (AwraDragPreview *self,
                                             guint            index,
                                             GdkPaintable    *paintable);

G_END_DECLS
