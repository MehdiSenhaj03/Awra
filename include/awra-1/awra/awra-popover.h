/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>
#include <awra/awra-surface.h>

G_BEGIN_DECLS

#define AWRA_TYPE_POPOVER (awra_popover_get_type ())
G_DECLARE_FINAL_TYPE (AwraPopover, awra_popover, AWRA, POPOVER, GtkPopover)

GtkWidget   *awra_popover_new         (void);
/**
 * awra_popover_get_child:
 * @self: an Awra popover
 *
 * Returns: (transfer none) (nullable): the popover content
 */
GtkWidget   *awra_popover_get_child   (AwraPopover *self);
/**
 * awra_popover_set_child:
 * @self: an Awra popover
 * @child: (nullable): popover content, or %NULL
 */
void         awra_popover_set_child   (AwraPopover *self,
                                       GtkWidget    *child);
/**
 * awra_popover_get_surface:
 * @self: an Awra popover
 *
 * Returns: (transfer none): the popover surface
 */
AwraSurface *awra_popover_get_surface (AwraPopover *self);
/**
 * awra_popover_get_menu_model:
 * @self: an Awra popover
 *
 * Returns: (transfer none) (nullable): the menu model
 */
GMenuModel  *awra_popover_get_menu_model (AwraPopover *self);
/**
 * awra_popover_set_menu_model:
 * @self: an Awra popover
 * @model: (nullable): a menu model, or %NULL
 */
void         awra_popover_set_menu_model (AwraPopover *self,
                                          GMenuModel   *model);
/**
 * awra_popover_add_menu_widget:
 * @self: an Awra popover
 * @child: a widget without a parent
 * @id: the id menu items name it by
 *
 * Gives the menu a widget of the application's own, such as a slider, in
 * place of the item whose "custom" attribute is @id, as #GtkPopoverMenu's
 * custom items are. The popover keeps @child and shows it again each time
 * the menu is rebuilt; its submenus find it too. Clicking or dragging in
 * it leaves the menu open.
 *
 * Returns: %TRUE if @id named no other widget and @child was added
 * Since: 1.1
 */
gboolean     awra_popover_add_menu_widget    (AwraPopover *self,
                                              GtkWidget   *child,
                                              const char  *id);
/**
 * awra_popover_remove_menu_widget:
 * @self: an Awra popover
 * @child: a widget added with awra_popover_add_menu_widget()
 *
 * Takes @child out of the menu and forgets it.
 *
 * Returns: %TRUE if @child had been added
 * Since: 1.1
 */
gboolean     awra_popover_remove_menu_widget (AwraPopover *self,
                                              GtkWidget   *child);

G_END_DECLS
