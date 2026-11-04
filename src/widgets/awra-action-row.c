/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-action-row.h>
#include <awra/awra-layout.h>

struct _AwraActionRow {
  GtkButton parent_instance;
  GtkBox *content;
  GtkImage *icon;
  GtkLabel *title;
  GtkLabel *subtitle;
  GtkWidget *suffix;
  char *icon_name;
  int subtitle_lines;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_ICON_NAME,
  PROP_SUFFIX,
  PROP_SUBTITLE_LINES,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraActionRow, awra_action_row, GTK_TYPE_BUTTON)

static void
awra_action_row_dispose (GObject *object)
{
  AWRA_ACTION_ROW (object)->suffix = NULL;
  G_OBJECT_CLASS (awra_action_row_parent_class)->dispose (object);
}

static void
awra_action_row_finalize (GObject *object)
{
  g_clear_pointer (&AWRA_ACTION_ROW (object)->icon_name, g_free);
  G_OBJECT_CLASS (awra_action_row_parent_class)->finalize (object);
}

static void
awra_action_row_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  AwraActionRow *self = AWRA_ACTION_ROW (object);

  switch (property_id) {
  case PROP_TITLE: g_value_set_string (value, awra_action_row_get_title (self)); break;
  case PROP_SUBTITLE: g_value_set_string (value, awra_action_row_get_subtitle (self)); break;
  case PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
  case PROP_SUFFIX: g_value_set_object (value, self->suffix); break;
  case PROP_SUBTITLE_LINES: g_value_set_int (value, self->subtitle_lines); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_action_row_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  AwraActionRow *self = AWRA_ACTION_ROW (object);

  switch (property_id) {
  case PROP_TITLE: awra_action_row_set_title (self, g_value_get_string (value)); break;
  case PROP_SUBTITLE: awra_action_row_set_subtitle (self, g_value_get_string (value)); break;
  case PROP_ICON_NAME: awra_action_row_set_icon_name (self, g_value_get_string (value)); break;
  case PROP_SUFFIX: awra_action_row_set_suffix (self, g_value_get_object (value)); break;
  case PROP_SUBTITLE_LINES: awra_action_row_set_subtitle_lines (self, g_value_get_int (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_action_row_class_init (AwraActionRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_action_row_dispose;
  object_class->finalize = awra_action_row_finalize;
  object_class->get_property = awra_action_row_get_property;
  object_class->set_property = awra_action_row_set_property;
  properties[PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, "",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SUBTITLE] = g_param_spec_string (
    "subtitle", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ICON_NAME] = g_param_spec_string (
    "icon-name", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SUFFIX] = g_param_spec_object (
    "suffix", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraActionRow:subtitle-lines:
   *
   * How many lines the subtitle may take before it's ellipsized: 1 keeps
   * it on one line, 0 lets it wrap onto as many as it needs.
   *
   * Since: 1.1
   */
  properties[PROP_SUBTITLE_LINES] = g_param_spec_int (
    "subtitle-lines", NULL, NULL, 0, G_MAXINT, 1,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "button");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_BUTTON);
}

/* GtkButton claims clicks in the capture phase, before any descendant sees
 * them, which made buttons placed in the suffix unreachable. In the bubble
 * phase the suffix controls handle their own clicks first and clicks on the
 * title, subtitle or icon still activate the row. */
static void
let_suffix_handle_clicks (AwraActionRow *self)
{
  g_autoptr (GListModel) controllers =
    gtk_widget_observe_controllers (GTK_WIDGET (self));
  guint n = g_list_model_get_n_items (controllers);

  for (guint i = 0; i < n; i++) {
    g_autoptr (GtkEventController) controller =
      g_list_model_get_item (controllers, i);

    if (GTK_IS_GESTURE_CLICK (controller))
      gtk_event_controller_set_propagation_phase (controller,
                                                  GTK_PHASE_BUBBLE);
  }
}

static void
awra_action_row_init (AwraActionRow *self)
{
  GtkWidget *labels = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  self->content = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  awra_box_set_spacing (self->content, AWRA_SPACING_MD);
  self->icon = GTK_IMAGE (gtk_image_new ());
  self->title = GTK_LABEL (gtk_label_new (NULL));
  self->subtitle = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-action-row");
  gtk_widget_add_css_class (GTK_WIDGET (self->icon), "awra-action-row-icon");
  gtk_widget_add_css_class (GTK_WIDGET (self->title), "awra-action-row-title");
  gtk_widget_add_css_class (GTK_WIDGET (self->subtitle),
                            "awra-action-row-subtitle");
  gtk_widget_set_visible (GTK_WIDGET (self->icon), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), FALSE);
  gtk_widget_set_hexpand (labels, TRUE);
  gtk_label_set_xalign (self->title, 0.0f);
  gtk_label_set_xalign (self->subtitle, 0.0f);
  gtk_label_set_ellipsize (self->title, PANGO_ELLIPSIZE_END);
  gtk_label_set_ellipsize (self->subtitle, PANGO_ELLIPSIZE_END);
  self->subtitle_lines = 1;
  gtk_box_append (GTK_BOX (labels), GTK_WIDGET (self->title));
  gtk_box_append (GTK_BOX (labels), GTK_WIDGET (self->subtitle));
  gtk_box_append (self->content, GTK_WIDGET (self->icon));
  gtk_box_append (self->content, labels);
  gtk_button_set_child (GTK_BUTTON (self), GTK_WIDGET (self->content));
  gtk_button_set_has_frame (GTK_BUTTON (self), FALSE);
  let_suffix_handle_clicks (self);
}

GtkWidget *
awra_action_row_new (const char *title,
                     const char *subtitle)
{
  return g_object_new (AWRA_TYPE_ACTION_ROW,
                       "title", title,
                       "subtitle", subtitle,
                       NULL);
}

const char *awra_action_row_get_title (AwraActionRow *self) { g_return_val_if_fail (AWRA_IS_ACTION_ROW (self), NULL); return gtk_label_get_text (self->title); }
void awra_action_row_set_title (AwraActionRow *self, const char *title) { const char *value; g_return_if_fail (AWRA_IS_ACTION_ROW (self)); value = title != NULL ? title : ""; if (g_strcmp0 (awra_action_row_get_title (self), value) == 0) return; gtk_label_set_text (self->title, value); gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL, value, -1); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]); }
const char *awra_action_row_get_subtitle (AwraActionRow *self) { g_return_val_if_fail (AWRA_IS_ACTION_ROW (self), NULL); return gtk_label_get_text (self->subtitle); }
void awra_action_row_set_subtitle (AwraActionRow *self, const char *subtitle) { const char *value; g_return_if_fail (AWRA_IS_ACTION_ROW (self)); value = subtitle != NULL ? subtitle : ""; if (g_strcmp0 (awra_action_row_get_subtitle (self), value) == 0) return; gtk_label_set_text (self->subtitle, value); gtk_widget_set_visible (GTK_WIDGET (self->subtitle), *value != '\0'); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]); }
const char *awra_action_row_get_icon_name (AwraActionRow *self) { g_return_val_if_fail (AWRA_IS_ACTION_ROW (self), NULL); return self->icon_name; }
void awra_action_row_set_icon_name (AwraActionRow *self, const char *icon_name) { g_return_if_fail (AWRA_IS_ACTION_ROW (self)); if (g_strcmp0 (self->icon_name, icon_name) == 0) return; g_free (self->icon_name); self->icon_name = g_strdup (icon_name); if (icon_name != NULL && *icon_name != '\0') { gtk_image_set_from_icon_name (self->icon, icon_name); gtk_widget_set_visible (GTK_WIDGET (self->icon), TRUE); } else { gtk_image_clear (self->icon); gtk_widget_set_visible (GTK_WIDGET (self->icon), FALSE); } g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]); }
GtkWidget *awra_action_row_get_suffix (AwraActionRow *self) { g_return_val_if_fail (AWRA_IS_ACTION_ROW (self), NULL); return self->suffix; }
void awra_action_row_set_suffix (AwraActionRow *self, GtkWidget *suffix) { g_return_if_fail (AWRA_IS_ACTION_ROW (self)); g_return_if_fail (suffix == NULL || GTK_IS_WIDGET (suffix)); if (self->suffix == suffix) return; if (self->suffix != NULL) gtk_box_remove (self->content, self->suffix); self->suffix = suffix; if (suffix != NULL) gtk_box_append (self->content, suffix); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUFFIX]); }

int
awra_action_row_get_subtitle_lines (AwraActionRow *self)
{
  g_return_val_if_fail (AWRA_IS_ACTION_ROW (self), 1);
  return self->subtitle_lines;
}

void
awra_action_row_set_subtitle_lines (AwraActionRow *self,
                                    int            lines)
{
  g_return_if_fail (AWRA_IS_ACTION_ROW (self));
  g_return_if_fail (lines >= 0);
  if (self->subtitle_lines == lines)
    return;
  self->subtitle_lines = lines;
  gtk_label_set_wrap (self->subtitle, lines != 1);
  gtk_label_set_wrap_mode (self->subtitle, PANGO_WRAP_WORD_CHAR);
  gtk_label_set_lines (self->subtitle, lines > 1 ? lines : -1);
  gtk_label_set_ellipsize (self->subtitle, lines == 0 ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END);
  /* A wrapping label asks for its natural width; let the row's width decide. */
  gtk_label_set_max_width_chars (self->subtitle, lines != 1 ? 1 : -1);
  gtk_widget_set_hexpand (GTK_WIDGET (self->subtitle), lines != 1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE_LINES]);
}
