/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_SELECTION_TOOLBAR (awra_selection_toolbar_get_type ())
G_DECLARE_FINAL_TYPE (AwraSelectionToolbar, awra_selection_toolbar,
                      AWRA, SELECTION_TOOLBAR, GtkWidget)

/**
 * awra_selection_toolbar_new:
 * @model: (nullable): selection model to observe
 */
GtkWidget         *awra_selection_toolbar_new                (GtkSelectionModel    *model);
/**
 * awra_selection_toolbar_get_model:
 * Returns: (transfer none) (nullable): the observed selection model
 */
GtkSelectionModel *awra_selection_toolbar_get_model          (AwraSelectionToolbar *self);
/**
 * awra_selection_toolbar_set_model:
 * @model: (nullable): selection model to observe
 */
void               awra_selection_toolbar_set_model          (AwraSelectionToolbar *self,
                                                              GtkSelectionModel    *model);
const char         *awra_selection_toolbar_get_title          (AwraSelectionToolbar *self);
/**
 * awra_selection_toolbar_set_title:
 * @title: (nullable): singular localized noun, or %NULL for the default
 */
void                awra_selection_toolbar_set_title          (AwraSelectionToolbar *self,
                                                              const char           *title);
const char         *awra_selection_toolbar_get_plural_title   (AwraSelectionToolbar *self);
/**
 * awra_selection_toolbar_set_plural_title:
 * @title: (nullable): plural localized noun, or %NULL for the default
 */
void                awra_selection_toolbar_set_plural_title   (AwraSelectionToolbar *self,
                                                              const char           *title);
/**
 * awra_selection_toolbar_get_actions:
 * Returns: (transfer none) (nullable): the contextual actions
 */
GtkWidget          *awra_selection_toolbar_get_actions        (AwraSelectionToolbar *self);
/**
 * awra_selection_toolbar_set_actions:
 * @actions: (nullable): contextual actions, or %NULL
 */
void                awra_selection_toolbar_set_actions        (AwraSelectionToolbar *self,
                                                              GtkWidget            *actions);
guint               awra_selection_toolbar_get_selected_count (AwraSelectionToolbar *self);
gboolean            awra_selection_toolbar_get_auto_hide      (AwraSelectionToolbar *self);
void                awra_selection_toolbar_set_auto_hide      (AwraSelectionToolbar *self,
                                                              gboolean              auto_hide);
void                awra_selection_toolbar_clear_selection    (AwraSelectionToolbar *self);

/**
 * awra_selection_toolbar_get_clear_label:
 * @self: a selection toolbar
 *
 * Returns: (transfer none): the clear-selection button label
 * Since: 1.1
 */
const char *awra_selection_toolbar_get_clear_label (AwraSelectionToolbar *self);
/**
 * awra_selection_toolbar_set_clear_label:
 * @self: a selection toolbar
 * @label: (nullable): the button label, or %NULL for the localized default
 *
 * Since: 1.1
 */
void awra_selection_toolbar_set_clear_label (AwraSelectionToolbar *self, const char *label);

G_END_DECLS
