/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-button.h>
#include <awra/awra-layout.h>
#include <awra/awra-material.h>
#include <awra/awra-selection-toolbar.h>
#include <awra/awra-surface.h>
#include <awra/awra-typography.h>

#include "awra-i18n-private.h"

struct _AwraSelectionToolbar {
  GtkWidget parent_instance;
  AwraSurface *surface;
  GtkBox *root;
  GtkLabel *count_label;
  GtkWidget *actions;
  GtkWidget *clear_button;
  GtkSelectionModel *model;
  gulong selection_changed_id;
  char *title;
  char *plural_title;
  guint selected_count;
  gboolean auto_hide;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_TITLE,
  PROP_PLURAL_TITLE,
  PROP_ACTIONS,
  PROP_SELECTED_COUNT,
  PROP_AUTO_HIDE,
  PROP_CLEAR_LABEL,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraSelectionToolbar, awra_selection_toolbar,
                     GTK_TYPE_WIDGET)

static void
update_selection (AwraSelectionToolbar *self)
{
  g_autoptr (GtkBitset) selected = NULL;
  g_autofree char *text = NULL;
  guint count = 0;

  if (self->model != NULL) {
    selected = gtk_selection_model_get_selection (self->model);
    count = (guint) MIN (gtk_bitset_get_size (selected), G_MAXUINT);
  }
  if (count != self->selected_count) {
    self->selected_count = count;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_COUNT]);
  }
  text = g_strdup_printf ("%u %s", count,
                          count == 1 ? self->title : self->plural_title);
  gtk_label_set_text (self->count_label, text);
  gtk_widget_set_sensitive (self->clear_button, count > 0);
  if (self->auto_hide)
    gtk_widget_set_visible (GTK_WIDGET (self), count > 0);
}

static void
selection_changed_cb (GtkSelectionModel    *model,
                      guint                 position,
                      guint                 n_items,
                      AwraSelectionToolbar *self)
{
  (void) model;
  (void) position;
  (void) n_items;
  update_selection (self);
}

static void
clear_clicked_cb (GtkButton            *button,
                  AwraSelectionToolbar *self)
{
  (void) button;
  awra_selection_toolbar_clear_selection (self);
}

static void
awra_selection_toolbar_dispose (GObject *object)
{
  AwraSelectionToolbar *self = AWRA_SELECTION_TOOLBAR (object);

  if (self->model != NULL && self->selection_changed_id != 0)
    g_signal_handler_disconnect (self->model, self->selection_changed_id);
  self->selection_changed_id = 0;
  g_clear_object (&self->model);
  if (self->surface != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->surface));
    self->surface = NULL;
    self->root = NULL;
    self->count_label = NULL;
    self->actions = NULL;
    self->clear_button = NULL;
  }
  G_OBJECT_CLASS (awra_selection_toolbar_parent_class)->dispose (object);
}

static void
awra_selection_toolbar_finalize (GObject *object)
{
  g_clear_pointer (&AWRA_SELECTION_TOOLBAR (object)->title, g_free);
  g_clear_pointer (&AWRA_SELECTION_TOOLBAR (object)->plural_title, g_free);
  G_OBJECT_CLASS (awra_selection_toolbar_parent_class)->finalize (object);
}

static void
awra_selection_toolbar_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  AwraSelectionToolbar *self = AWRA_SELECTION_TOOLBAR (widget);

  if (self->surface != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->surface), snapshot);
}

static void
awra_selection_toolbar_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  AwraSelectionToolbar *self = AWRA_SELECTION_TOOLBAR (object);

  switch (property_id) {
  case PROP_CLEAR_LABEL: g_value_set_string (value, awra_selection_toolbar_get_clear_label (self)); break;
  case PROP_MODEL: g_value_set_object (value, self->model); break;
  case PROP_TITLE: g_value_set_string (value, self->title); break;
  case PROP_PLURAL_TITLE: g_value_set_string (value, self->plural_title); break;
  case PROP_ACTIONS: g_value_set_object (value, self->actions); break;
  case PROP_SELECTED_COUNT: g_value_set_uint (value, self->selected_count); break;
  case PROP_AUTO_HIDE: g_value_set_boolean (value, self->auto_hide); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_selection_toolbar_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  AwraSelectionToolbar *self = AWRA_SELECTION_TOOLBAR (object);

  switch (property_id) {
  case PROP_CLEAR_LABEL: awra_selection_toolbar_set_clear_label (self, g_value_get_string (value)); break;
  case PROP_MODEL: awra_selection_toolbar_set_model (self, g_value_get_object (value)); break;
  case PROP_TITLE: awra_selection_toolbar_set_title (self, g_value_get_string (value)); break;
  case PROP_PLURAL_TITLE: awra_selection_toolbar_set_plural_title (self, g_value_get_string (value)); break;
  case PROP_ACTIONS: awra_selection_toolbar_set_actions (self, g_value_get_object (value)); break;
  case PROP_AUTO_HIDE: awra_selection_toolbar_set_auto_hide (self, g_value_get_boolean (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_selection_toolbar_class_init (AwraSelectionToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_selection_toolbar_dispose;
  object_class->finalize = awra_selection_toolbar_finalize;
  object_class->get_property = awra_selection_toolbar_get_property;
  object_class->set_property = awra_selection_toolbar_set_property;
  widget_class->snapshot = awra_selection_toolbar_snapshot;
  properties[PROP_MODEL] = g_param_spec_object (
    "model", NULL, NULL, GTK_TYPE_SELECTION_MODEL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, "item",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_PLURAL_TITLE] = g_param_spec_string (
    "plural-title", NULL, NULL, "items",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACTIONS] = g_param_spec_object (
    "actions", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SELECTED_COUNT] = g_param_spec_uint (
    "selected-count", NULL, NULL, 0, G_MAXUINT, 0,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_AUTO_HIDE] = g_param_spec_boolean (
    "auto-hide", NULL, NULL, TRUE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CLEAR_LABEL] = g_param_spec_string (
    "clear-label", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awraselectiontoolbar");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TOOLBAR);
}

static void
awra_selection_toolbar_init (AwraSelectionToolbar *self)
{
  g_autoptr (AwraMaterial) material = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_LAYER);

  self->title = g_strdup (_("item"));
  self->plural_title = g_strdup (_("items"));
  self->auto_hide = TRUE;
  self->surface = AWRA_SURFACE (awra_surface_new_with_role (
    AWRA_SURFACE_ROLE_TOOLBAR));
  awra_surface_set_material (self->surface, material);
  awra_surface_set_elevation (self->surface, AWRA_ELEVATION_RAISED);
  self->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->count_label = GTK_LABEL (gtk_label_new (NULL));
  self->clear_button = awra_button_new_with_label (_("Clear"));
  awra_button_set_appearance (AWRA_BUTTON (self->clear_button),
                              AWRA_BUTTON_APPEARANCE_GHOST);
  awra_box_set_spacing (self->root, AWRA_SPACING_SM);
  awra_widget_set_margin (GTK_WIDGET (self->root), AWRA_SPACING_SM);
  awra_widget_set_typography (GTK_WIDGET (self->count_label),
                              AWRA_TYPOGRAPHY_BODY);
  gtk_widget_set_hexpand (GTK_WIDGET (self->count_label), TRUE);
  gtk_label_set_xalign (self->count_label, 0.0f);
  gtk_box_append (self->root, GTK_WIDGET (self->count_label));
  gtk_box_append (self->root, self->clear_button);
  awra_surface_set_child (self->surface, GTK_WIDGET (self->root));
  gtk_widget_set_parent (GTK_WIDGET (self->surface), GTK_WIDGET (self));
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
  g_signal_connect (self->clear_button, "clicked",
                    G_CALLBACK (clear_clicked_cb), self);
  update_selection (self);
}

GtkWidget *
awra_selection_toolbar_new (GtkSelectionModel *model)
{
  return g_object_new (AWRA_TYPE_SELECTION_TOOLBAR, "model", model, NULL);
}

GtkSelectionModel *
awra_selection_toolbar_get_model (AwraSelectionToolbar *self)
{
  g_return_val_if_fail (AWRA_IS_SELECTION_TOOLBAR (self), NULL);
  return self->model;
}

void
awra_selection_toolbar_set_model (AwraSelectionToolbar *self,
                                  GtkSelectionModel    *model)
{
  g_return_if_fail (AWRA_IS_SELECTION_TOOLBAR (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));
  if (self->model == model)
    return;
  if (self->model != NULL && self->selection_changed_id != 0)
    g_signal_handler_disconnect (self->model, self->selection_changed_id);
  self->selection_changed_id = 0;
  g_set_object (&self->model, model);
  if (model != NULL)
    self->selection_changed_id = g_signal_connect (
      model, "selection-changed", G_CALLBACK (selection_changed_cb), self);
  update_selection (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

const char *
awra_selection_toolbar_get_title (AwraSelectionToolbar *self)
{
  g_return_val_if_fail (AWRA_IS_SELECTION_TOOLBAR (self), NULL);
  return self->title;
}

void
awra_selection_toolbar_set_title (AwraSelectionToolbar *self,
                                  const char           *title)
{
  g_return_if_fail (AWRA_IS_SELECTION_TOOLBAR (self));
  if (g_strcmp0 (self->title, title) == 0)
    return;
  g_free (self->title);
  self->title = g_strdup (title != NULL && *title != '\0' ? title : _("item"));
  update_selection (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

const char *
awra_selection_toolbar_get_plural_title (AwraSelectionToolbar *self)
{
  g_return_val_if_fail (AWRA_IS_SELECTION_TOOLBAR (self), NULL);
  return self->plural_title;
}

void
awra_selection_toolbar_set_plural_title (AwraSelectionToolbar *self,
                                         const char           *title)
{
  const char *value;

  g_return_if_fail (AWRA_IS_SELECTION_TOOLBAR (self));
  value = title != NULL && *title != '\0' ? title : _("items");
  if (g_strcmp0 (self->plural_title, value) == 0)
    return;
  g_free (self->plural_title);
  self->plural_title = g_strdup (value);
  update_selection (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLURAL_TITLE]);
}

GtkWidget *
awra_selection_toolbar_get_actions (AwraSelectionToolbar *self)
{
  g_return_val_if_fail (AWRA_IS_SELECTION_TOOLBAR (self), NULL);
  return self->actions;
}

void
awra_selection_toolbar_set_actions (AwraSelectionToolbar *self,
                                    GtkWidget            *actions)
{
  g_return_if_fail (AWRA_IS_SELECTION_TOOLBAR (self));
  g_return_if_fail (actions == NULL || GTK_IS_WIDGET (actions));
  if (self->actions == actions)
    return;
  if (self->actions != NULL)
    gtk_box_remove (self->root, self->actions);
  self->actions = actions;
  if (actions != NULL)
    gtk_box_insert_child_after (self->root, actions,
                                GTK_WIDGET (self->count_label));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIONS]);
}

guint
awra_selection_toolbar_get_selected_count (AwraSelectionToolbar *self)
{
  g_return_val_if_fail (AWRA_IS_SELECTION_TOOLBAR (self), 0);
  return self->selected_count;
}

gboolean
awra_selection_toolbar_get_auto_hide (AwraSelectionToolbar *self)
{
  g_return_val_if_fail (AWRA_IS_SELECTION_TOOLBAR (self), TRUE);
  return self->auto_hide;
}

void
awra_selection_toolbar_set_auto_hide (AwraSelectionToolbar *self,
                                      gboolean              auto_hide)
{
  g_return_if_fail (AWRA_IS_SELECTION_TOOLBAR (self));
  auto_hide = !!auto_hide;
  if (self->auto_hide == auto_hide)
    return;
  self->auto_hide = auto_hide;
  gtk_widget_set_visible (GTK_WIDGET (self),
                          !auto_hide || self->selected_count > 0);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTO_HIDE]);
}

void
awra_selection_toolbar_clear_selection (AwraSelectionToolbar *self)
{
  g_return_if_fail (AWRA_IS_SELECTION_TOOLBAR (self));
  if (self->model != NULL)
    gtk_selection_model_unselect_all (self->model);
}

const char *
awra_selection_toolbar_get_clear_label (AwraSelectionToolbar *self)
{
  g_return_val_if_fail (AWRA_IS_SELECTION_TOOLBAR (self), NULL);
  return gtk_button_get_label (GTK_BUTTON (self->clear_button));
}

void
awra_selection_toolbar_set_clear_label (AwraSelectionToolbar *self, const char *label)
{
  g_return_if_fail (AWRA_IS_SELECTION_TOOLBAR (self));
  const char *effective = label != NULL ? label : _("Clear");
  if (g_strcmp0 (gtk_button_get_label (GTK_BUTTON (self->clear_button)), effective) == 0) return;
  gtk_button_set_label (GTK_BUTTON (self->clear_button), effective);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLEAR_LABEL]);
}
