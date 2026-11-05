/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>
#include <awra/awra-breadcrumb-item.h>

G_BEGIN_DECLS

#define AWRA_TYPE_PATH_BAR (awra_path_bar_get_type ())
G_DECLARE_FINAL_TYPE (AwraPathBar, awra_path_bar, AWRA, PATH_BAR, GtkWidget)

GtkWidget  *awra_path_bar_new (void);
/**
 * awra_path_bar_get_model:
 * @self: a path bar
 *
 * Returns: (transfer none) (nullable): the #AwraBreadcrumbItem trail
 *
 * Since: 1.1
 */
GListModel *awra_path_bar_get_model (AwraPathBar *self);
/**
 * awra_path_bar_set_model:
 * @self: a path bar
 * @model: (nullable): a model containing #AwraBreadcrumbItem objects
 *
 * Since: 1.1
 */
void        awra_path_bar_set_model (AwraPathBar *self,
                                     GListModel  *model);
/**
 * awra_path_bar_get_text:
 * @self: a path bar
 *
 * Returns: (transfer none) (nullable): the textual location offered for editing
 *
 * Since: 1.1
 */
const char *awra_path_bar_get_text (AwraPathBar *self);
/**
 * awra_path_bar_set_text:
 * @self: a path bar
 * @text: (nullable): the textual location offered for editing
 *
 * Sets the committed location. An edit in progress keeps the user's text.
 *
 * Since: 1.1
 */
void        awra_path_bar_set_text (AwraPathBar *self,
                                    const char  *text);
/**
 * awra_path_bar_get_edit_text:
 * @self: a path bar
 *
 * Returns: (transfer none): the text being typed, or an empty string
 *
 * Since: 1.1
 */
const char *awra_path_bar_get_edit_text (AwraPathBar *self);
/**
 * awra_path_bar_get_placeholder_text:
 * @self: a path bar
 *
 * Returns: (transfer none) (nullable): the hint shown in an empty field
 *
 * Since: 1.1
 */
const char *awra_path_bar_get_placeholder_text (AwraPathBar *self);
/**
 * awra_path_bar_set_placeholder_text:
 * @self: a path bar
 * @text: (nullable): the hint shown in an empty field
 *
 * Since: 1.1
 */
void        awra_path_bar_set_placeholder_text (AwraPathBar *self,
                                                const char  *text);
/**
 * awra_path_bar_get_icon_name:
 * @self: a path bar
 *
 * Returns: (transfer none) (nullable): the leading icon name
 *
 * Since: 1.1
 */
const char *awra_path_bar_get_icon_name (AwraPathBar *self);
/**
 * awra_path_bar_set_icon_name:
 * @self: a path bar
 * @icon_name: (nullable): a leading icon, or %NULL for none
 *
 * Since: 1.1
 */
void        awra_path_bar_set_icon_name (AwraPathBar *self,
                                         const char  *icon_name);
/**
 * awra_path_bar_get_end_widget:
 * @self: a path bar
 *
 * Returns: (transfer none) (nullable): the trailing widget
 *
 * Since: 1.1
 */
GtkWidget  *awra_path_bar_get_end_widget (AwraPathBar *self);
/**
 * awra_path_bar_set_end_widget:
 * @self: a path bar
 * @widget: (nullable): a trailing widget, such as a location menu
 *
 * The widget stays visible in both modes, so a click on it never starts an
 * edit and the field does not change width.
 *
 * Since: 1.1
 */
void        awra_path_bar_set_end_widget (AwraPathBar *self,
                                          GtkWidget   *widget);
/**
 * awra_path_bar_get_editing:
 * @self: a path bar
 *
 * Returns: whether the text field replaces the breadcrumbs
 *
 * Since: 1.1
 */
gboolean    awra_path_bar_get_editing (AwraPathBar *self);
/**
 * awra_path_bar_set_editing:
 * @self: a path bar
 * @editing: whether to show the text field
 *
 * Entering edit mode loads the committed text, focuses it and selects it.
 * Leaving edit mode discards the typed text.
 *
 * Since: 1.1
 */
void        awra_path_bar_set_editing (AwraPathBar *self,
                                       gboolean     editing);
/**
 * awra_path_bar_begin_edit:
 * @self: a path bar
 * @text: (nullable): the initial text, or %NULL for the committed location
 *
 * Enters edit mode. With @text, the caret follows it, so that the first typed
 * character of a location can open the field and remain part of the input.
 * With %NULL this is equivalent to awra_path_bar_set_editing().
 *
 * Since: 1.1
 */
void        awra_path_bar_begin_edit (AwraPathBar *self,
                                      const char  *text);
/**
 * awra_path_bar_get_suggestions:
 * @self: a path bar
 *
 * Returns: (transfer none) (nullable): the #AwraBreadcrumbItem suggestions
 *
 * Since: 1.1
 */
GListModel *awra_path_bar_get_suggestions (AwraPathBar *self);
/**
 * awra_path_bar_set_suggestions:
 * @self: a path bar
 * @model: (nullable): a model containing #AwraBreadcrumbItem objects
 *
 * Offers completions for the text being typed: each item shows its label and
 * inserts its name without committing, so the user can keep descending. The
 * consumer owns the lookup, usually in response to
 * #AwraPathBar:edit-text, so the bar works with any hierarchical address.
 *
 * Since: 1.1
 */
void        awra_path_bar_set_suggestions (AwraPathBar *self,
                                           GListModel  *model);

/**
 * awra_path_bar_get_item_at:
 * @self: a path bar
 * @x: horizontal position, in the coordinates of the path bar
 * @y: vertical position, in the coordinates of the path bar
 *
 * Finds the breadcrumb under a point, for instance to make each one a drop
 * target. Nothing is found while the text field is shown.
 *
 * Returns: (transfer none) (nullable): the breadcrumb at this position
 *
 * Since: 1.1
 */
AwraBreadcrumbItem *awra_path_bar_get_item_at (AwraPathBar *self,
                                               double       x,
                                               double       y);
/**
 * awra_path_bar_set_drop_item:
 * @self: a path bar
 * @item: (nullable): the breadcrumb that would receive a drop
 *
 * Marks one breadcrumb as the target of the drag in progress.
 *
 * Since: 1.1
 */
void        awra_path_bar_set_drop_item       (AwraPathBar        *self,
                                               AwraBreadcrumbItem *item);

G_END_DECLS
