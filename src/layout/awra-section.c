/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-layout.h>
#include <awra/awra-section.h>
#include <awra/awra-typography.h>

struct _AwraSection {
  GtkWidget parent_instance;
  GtkBox *root;
  GtkBox *heading;
  GtkBox *titles;
  GtkLabel *title_label;
  GtkLabel *description_label;
  GtkWidget *actions;
  GtkWidget *child;
  char *title;
  char *description;
};

enum { PROP_0, PROP_TITLE, PROP_DESCRIPTION, PROP_ACTIONS, PROP_CHILD, N_PROPS };
static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraSection, awra_section, GTK_TYPE_WIDGET)

static void
update_visibility (AwraSection *self)
{
  gboolean has_title = self->title != NULL && *self->title != '\0';
  gboolean has_description = self->description != NULL && *self->description != '\0';

  gtk_widget_set_visible (GTK_WIDGET (self->title_label), has_title);
  gtk_widget_set_visible (GTK_WIDGET (self->description_label), has_description);
  gtk_widget_set_visible (GTK_WIDGET (self->titles), has_title || has_description);
  gtk_widget_set_visible (GTK_WIDGET (self->heading),
                          has_title || has_description || self->actions != NULL);
}

static void
awra_section_dispose (GObject *object)
{
  AwraSection *self = AWRA_SECTION (object);

  if (self->root != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->root));
    self->root = NULL;
    self->heading = NULL;
    self->titles = NULL;
    self->title_label = NULL;
    self->description_label = NULL;
    self->actions = NULL;
    self->child = NULL;
  }
  G_OBJECT_CLASS (awra_section_parent_class)->dispose (object);
}

static void
awra_section_finalize (GObject *object)
{
  AwraSection *self = AWRA_SECTION (object);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->description, g_free);
  G_OBJECT_CLASS (awra_section_parent_class)->finalize (object);
}

static void
awra_section_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  AwraSection *self = AWRA_SECTION (widget);
  if (self->root != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->root), snapshot);
}

static void
awra_section_get_property (GObject *object, guint id, GValue *value,
                           GParamSpec *pspec)
{
  AwraSection *self = AWRA_SECTION (object);
  switch (id) {
  case PROP_TITLE: g_value_set_string (value, self->title); break;
  case PROP_DESCRIPTION: g_value_set_string (value, self->description); break;
  case PROP_ACTIONS: g_value_set_object (value, self->actions); break;
  case PROP_CHILD: g_value_set_object (value, self->child); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_section_set_property (GObject *object, guint id, const GValue *value,
                           GParamSpec *pspec)
{
  AwraSection *self = AWRA_SECTION (object);
  switch (id) {
  case PROP_TITLE: awra_section_set_title (self, g_value_get_string (value)); break;
  case PROP_DESCRIPTION: awra_section_set_description (self, g_value_get_string (value)); break;
  case PROP_ACTIONS: awra_section_set_actions (self, g_value_get_object (value)); break;
  case PROP_CHILD: awra_section_set_child (self, g_value_get_object (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_section_class_init (AwraSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_section_dispose;
  object_class->finalize = awra_section_finalize;
  object_class->get_property = awra_section_get_property;
  object_class->set_property = awra_section_set_property;
  widget_class->snapshot = awra_section_snapshot;
  properties[PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_DESCRIPTION] = g_param_spec_string (
    "description", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACTIONS] = g_param_spec_object (
    "actions", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CHILD] = g_param_spec_object (
    "child", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrasection");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_section_init (AwraSection *self)
{
  self->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->heading = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->titles = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->title_label = GTK_LABEL (gtk_label_new (NULL));
  self->description_label = GTK_LABEL (gtk_label_new (NULL));
  awra_box_set_spacing (self->root, AWRA_SPACING_MD);
  awra_box_set_spacing (self->heading, AWRA_SPACING_LG);
  awra_box_set_spacing (self->titles, AWRA_SPACING_XS);
  awra_widget_set_typography (GTK_WIDGET (self->title_label),
                              AWRA_TYPOGRAPHY_TITLE_3);
  awra_widget_set_typography (GTK_WIDGET (self->description_label),
                              AWRA_TYPOGRAPHY_MUTED);
  gtk_label_set_xalign (self->title_label, 0.0f);
  gtk_label_set_xalign (self->description_label, 0.0f);
  gtk_label_set_wrap (self->description_label, TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->titles), TRUE);
  gtk_box_append (self->titles, GTK_WIDGET (self->title_label));
  gtk_box_append (self->titles, GTK_WIDGET (self->description_label));
  gtk_box_append (self->heading, GTK_WIDGET (self->titles));
  gtk_box_append (self->root, GTK_WIDGET (self->heading));
  gtk_widget_set_parent (GTK_WIDGET (self->root), GTK_WIDGET (self));
  update_visibility (self);
}

GtkWidget *awra_section_new (void) { return g_object_new (AWRA_TYPE_SECTION, NULL); }
const char *awra_section_get_title (AwraSection *self) { g_return_val_if_fail (AWRA_IS_SECTION (self), NULL); return self->title; }
void awra_section_set_title (AwraSection *self, const char *title) { g_return_if_fail (AWRA_IS_SECTION (self)); if (g_strcmp0 (self->title, title) == 0) return; g_free (self->title); self->title = g_strdup (title); gtk_label_set_text (self->title_label, title != NULL ? title : ""); update_visibility (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]); }
const char *awra_section_get_description (AwraSection *self) { g_return_val_if_fail (AWRA_IS_SECTION (self), NULL); return self->description; }
void awra_section_set_description (AwraSection *self, const char *description) { g_return_if_fail (AWRA_IS_SECTION (self)); if (g_strcmp0 (self->description, description) == 0) return; g_free (self->description); self->description = g_strdup (description); gtk_label_set_text (self->description_label, description != NULL ? description : ""); update_visibility (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DESCRIPTION]); }
GtkWidget *awra_section_get_actions (AwraSection *self) { g_return_val_if_fail (AWRA_IS_SECTION (self), NULL); return self->actions; }
void awra_section_set_actions (AwraSection *self, GtkWidget *actions) { g_return_if_fail (AWRA_IS_SECTION (self)); g_return_if_fail (actions == NULL || GTK_IS_WIDGET (actions)); if (self->actions == actions) return; if (self->actions != NULL) gtk_box_remove (self->heading, self->actions); self->actions = actions;
  /* Actions sit on the title's line, whatever the description under it:
   * the title takes a control's height, centred in it, and both align at
   * the top. */
  gtk_widget_set_valign (GTK_WIDGET (self->titles), actions != NULL ? GTK_ALIGN_START : GTK_ALIGN_FILL);
  gtk_widget_set_size_request (GTK_WIDGET (self->title_label), -1, actions != NULL ? 34 : -1);
  if (actions != NULL) { gtk_widget_set_valign (actions, GTK_ALIGN_START); gtk_box_append (self->heading, actions); } update_visibility (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIONS]); }
GtkWidget *awra_section_get_child (AwraSection *self) { g_return_val_if_fail (AWRA_IS_SECTION (self), NULL); return self->child; }
void awra_section_set_child (AwraSection *self, GtkWidget *child) { g_return_if_fail (AWRA_IS_SECTION (self)); g_return_if_fail (child == NULL || GTK_IS_WIDGET (child)); if (self->child == child) return; if (self->child != NULL) gtk_box_remove (self->root, self->child); self->child = child; if (child != NULL) gtk_box_append (self->root, child); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]); }
