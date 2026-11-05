/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra-context.h>
#include <awra/awra-layout.h>
#include <awra/awra-progress-bar.h>
#include <awra/awra-task-row.h>
#include <awra/awra-typography.h>
#include <math.h>

struct _AwraTaskRow
{
  GtkWidget parent_instance;
  GtkWidget *grid, *title, *subtitle, *icon, *spinner, *progress, *suffix;
  AwraStyleManager *style;
  char *icon_name;
  gboolean indeterminate, show_progress;
};
enum
{
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_ICON_NAME,
  PROP_FRACTION,
  PROP_INDETERMINATE,
  PROP_SHOW_PROGRESS,
  PROP_SUFFIX,
  N_PROPS
};
static GParamSpec *properties[N_PROPS];
G_DEFINE_FINAL_TYPE (AwraTaskRow, awra_task_row, GTK_TYPE_WIDGET)

static void
update_progress (AwraTaskRow *self)
{
  gboolean busy = self->show_progress && self->indeterminate;
  gboolean animated
      = busy && !awra_style_manager_get_reduced_motion (self->style);
  gtk_widget_set_visible (self->spinner, animated);
  gtk_spinner_set_spinning (GTK_SPINNER (self->spinner), animated);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->icon),
                                busy ? "content-loading-symbolic"
                                     : self->icon_name);
  gtk_widget_set_visible (
      self->icon, (busy && !animated) || (!busy && self->icon_name != NULL));
  gtk_widget_set_visible (self->progress,
                          self->show_progress && !self->indeterminate);
}
static void
motion_changed (AwraStyleManager *style, GParamSpec *pspec, AwraTaskRow *self)
{
  (void)style;
  (void)pspec;
  update_progress (self);
}
static void
awra_task_row_dispose (GObject *object)
{
  AwraTaskRow *self = AWRA_TASK_ROW (object);
  if (self->style)
    {
      g_signal_handlers_disconnect_by_data (self->style, self);
      g_clear_object (&self->style);
    }
  self->suffix = NULL;
  g_clear_pointer (&self->grid, gtk_widget_unparent);
  G_OBJECT_CLASS (awra_task_row_parent_class)->dispose (object);
}
static void
awra_task_row_finalize (GObject *object)
{
  g_free (AWRA_TASK_ROW (object)->icon_name);
  G_OBJECT_CLASS (awra_task_row_parent_class)->finalize (object);
}
static void
awra_task_row_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  AwraTaskRow *self = AWRA_TASK_ROW (widget);
  if (self->grid)
    gtk_widget_snapshot_child (widget, self->grid, snapshot);
}
static void
awra_task_row_get_property (GObject *object, guint id, GValue *value,
                            GParamSpec *pspec)
{
  AwraTaskRow *self = AWRA_TASK_ROW (object);
  switch (id)
    {
    case PROP_TITLE:
      g_value_set_string (value, awra_task_row_get_title (self));
      break;
    case PROP_SUBTITLE:
      g_value_set_string (value, awra_task_row_get_subtitle (self));
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, awra_task_row_get_icon_name (self));
      break;
    case PROP_FRACTION:
      g_value_set_double (value, awra_task_row_get_fraction (self));
      break;
    case PROP_INDETERMINATE:
      g_value_set_boolean (value, awra_task_row_get_indeterminate (self));
      break;
    case PROP_SHOW_PROGRESS:
      g_value_set_boolean (value, awra_task_row_get_show_progress (self));
      break;
    case PROP_SUFFIX:
      g_value_set_object (value, awra_task_row_get_suffix (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}
static void
awra_task_row_set_property (GObject *object, guint id, const GValue *value,
                            GParamSpec *pspec)
{
  AwraTaskRow *self = AWRA_TASK_ROW (object);
  switch (id)
    {
    case PROP_TITLE:
      awra_task_row_set_title (self, g_value_get_string (value));
      break;
    case PROP_SUBTITLE:
      awra_task_row_set_subtitle (self, g_value_get_string (value));
      break;
    case PROP_ICON_NAME:
      awra_task_row_set_icon_name (self, g_value_get_string (value));
      break;
    case PROP_FRACTION:
      awra_task_row_set_fraction (self, g_value_get_double (value));
      break;
    case PROP_INDETERMINATE:
      awra_task_row_set_indeterminate (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_PROGRESS:
      awra_task_row_set_show_progress (self, g_value_get_boolean (value));
      break;
    case PROP_SUFFIX:
      awra_task_row_set_suffix (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}
static void
awra_task_row_class_init (AwraTaskRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_task_row_dispose;
  object_class->finalize = awra_task_row_finalize;
  object_class->get_property = awra_task_row_get_property;
  object_class->set_property = awra_task_row_set_property;
  widget_class->snapshot = awra_task_row_snapshot;
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrataskrow");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_GROUP);
  properties[PROP_TITLE] = g_param_spec_string (
      "title", NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SUBTITLE] = g_param_spec_string (
      "subtitle", NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ICON_NAME] = g_param_spec_string (
      "icon-name", NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_FRACTION] = g_param_spec_double (
      "fraction", NULL, NULL, 0.0, 1.0, 0.0,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_INDETERMINATE] = g_param_spec_boolean (
      "indeterminate", NULL, NULL, FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SHOW_PROGRESS] = g_param_spec_boolean (
      "show-progress", NULL, NULL, FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SUFFIX] = g_param_spec_object (
      "suffix", NULL, NULL, GTK_TYPE_WIDGET,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}
static void
awra_task_row_init (AwraTaskRow *self)
{
  self->style = g_object_ref (
      awra_context_get_style_manager (awra_context_get_for_display (
          gtk_widget_get_display (GTK_WIDGET (self)))));
  g_signal_connect_object (self->style, "notify::reduced-motion",
                           G_CALLBACK (motion_changed), self, 0);
  self->grid = gtk_grid_new ();
  awra_grid_apply_layout_preset (GTK_GRID (self->grid),
                                 AWRA_LAYOUT_PRESET_CONTROL_GROUP);
  gtk_widget_set_parent (self->grid, GTK_WIDGET (self));
  self->icon = gtk_image_new ();
  self->spinner = gtk_spinner_new ();
  GtkWidget *leading = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_valign (leading, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (leading), self->icon);
  gtk_box_append (GTK_BOX (leading), self->spinner);
  gtk_grid_attach (GTK_GRID (self->grid), leading, 0, 0, 1, 2);
  self->title = gtk_label_new (NULL);
  self->subtitle = gtk_label_new (NULL);
  GtkWidget *labels[] = { self->title, self->subtitle };
  for (guint i = 0; i < G_N_ELEMENTS (labels); i++)
    {
      gtk_label_set_xalign (GTK_LABEL (labels[i]), 0);
      gtk_label_set_ellipsize (GTK_LABEL (labels[i]), PANGO_ELLIPSIZE_END);
      gtk_widget_set_hexpand (labels[i], TRUE);
      gtk_grid_attach (GTK_GRID (self->grid), labels[i], 1, i, 1, 1);
    }
  gtk_widget_add_css_class (self->title, "awra-task-title");
  gtk_widget_add_css_class (self->subtitle, "awra-task-subtitle");
  awra_widget_set_typography (self->subtitle, AWRA_TYPOGRAPHY_MUTED);
  self->progress = awra_progress_bar_new ();
  gtk_grid_attach (GTK_GRID (self->grid), self->progress, 1, 2, 1, 1);
  gtk_widget_set_visible (self->subtitle, FALSE);
  update_progress (self);
}
GtkWidget *
awra_task_row_new (const char *title)
{
  return g_object_new (AWRA_TYPE_TASK_ROW, "title", title, NULL);
}

const char *
awra_task_row_get_title (AwraTaskRow *self)
{
  g_return_val_if_fail (AWRA_IS_TASK_ROW (self), NULL);
  return gtk_label_get_text (GTK_LABEL (self->title));
}
void
awra_task_row_set_title (AwraTaskRow *self, const char *value)
{
  g_return_if_fail (AWRA_IS_TASK_ROW (self));
  if (g_strcmp0 (gtk_label_get_text (GTK_LABEL (self->title)),
                 value ? value : "")
      == 0)
    return;
  gtk_label_set_text (GTK_LABEL (self->title), value ? value : "");
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  value ? value : "", -1);
  gtk_accessible_update_property (
      GTK_ACCESSIBLE (
          awra_progress_bar_get_delegate (AWRA_PROGRESS_BAR (self->progress))),
      GTK_ACCESSIBLE_PROPERTY_LABEL, value ? value : "", -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

const char *
awra_task_row_get_subtitle (AwraTaskRow *self)
{
  g_return_val_if_fail (AWRA_IS_TASK_ROW (self), NULL);
  return gtk_label_get_text (GTK_LABEL (self->subtitle));
}
void
awra_task_row_set_subtitle (AwraTaskRow *self, const char *value)
{
  g_return_if_fail (AWRA_IS_TASK_ROW (self));
  if (g_strcmp0 (gtk_label_get_text (GTK_LABEL (self->subtitle)),
                 value ? value : "")
      == 0)
    return;
  gtk_label_set_text (GTK_LABEL (self->subtitle), value ? value : "");
  gtk_widget_set_visible (self->subtitle, value && *value);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

const char *
awra_task_row_get_icon_name (AwraTaskRow *self)
{
  g_return_val_if_fail (AWRA_IS_TASK_ROW (self), NULL);
  return self->icon_name;
}
void
awra_task_row_set_icon_name (AwraTaskRow *self, const char *value)
{
  g_return_if_fail (AWRA_IS_TASK_ROW (self));
  if (g_strcmp0 (self->icon_name, value) == 0)
    return;
  g_free (self->icon_name);
  self->icon_name = g_strdup (value);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->icon), value);
  update_progress (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}
double
awra_task_row_get_fraction (AwraTaskRow *self)
{
  g_return_val_if_fail (AWRA_IS_TASK_ROW (self), 0);
  return awra_progress_bar_get_fraction (AWRA_PROGRESS_BAR (self->progress));
}
void
awra_task_row_set_fraction (AwraTaskRow *self, double value)
{
  g_return_if_fail (AWRA_IS_TASK_ROW (self));
  value = isfinite (value) ? CLAMP (value, 0.0, 1.0) : 0.0;
  if (awra_task_row_get_fraction (self) == value)
    return;
  awra_progress_bar_set_fraction (AWRA_PROGRESS_BAR (self->progress), value);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FRACTION]);
}

gboolean
awra_task_row_get_indeterminate (AwraTaskRow *self)
{
  g_return_val_if_fail (AWRA_IS_TASK_ROW (self), FALSE);
  return self->indeterminate;
}
void
awra_task_row_set_indeterminate (AwraTaskRow *self, gboolean value)
{
  g_return_if_fail (AWRA_IS_TASK_ROW (self));
  value = !!value;
  if (self->indeterminate == value)
    return;
  self->indeterminate = value;
  update_progress (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INDETERMINATE]);
}

gboolean
awra_task_row_get_show_progress (AwraTaskRow *self)
{
  g_return_val_if_fail (AWRA_IS_TASK_ROW (self), FALSE);
  return self->show_progress;
}
void
awra_task_row_set_show_progress (AwraTaskRow *self, gboolean value)
{
  g_return_if_fail (AWRA_IS_TASK_ROW (self));
  value = !!value;
  if (self->show_progress == value)
    return;
  self->show_progress = value;
  update_progress (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_PROGRESS]);
}

GtkWidget *
awra_task_row_get_suffix (AwraTaskRow *self)
{
  g_return_val_if_fail (AWRA_IS_TASK_ROW (self), NULL);
  return self->suffix;
}
void
awra_task_row_set_suffix (AwraTaskRow *self, GtkWidget *value)
{
  g_return_if_fail (AWRA_IS_TASK_ROW (self));
  g_return_if_fail (value == NULL || GTK_IS_WIDGET (value));
  if (self->suffix == value)
    return;
  g_return_if_fail (value == NULL || gtk_widget_get_parent (value) == NULL);
  if (self->suffix)
    gtk_grid_remove (GTK_GRID (self->grid), self->suffix);
  self->suffix = value;
  if (value)
    {
      gtk_widget_set_valign (value, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (self->grid), value, 2, 0, 1, 3);
    }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUFFIX]);
}
