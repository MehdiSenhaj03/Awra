/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-layout.h>
#include <awra/awra-page-header.h>
#include <awra/awra-typography.h>

struct _AwraPageHeader {
  GtkWidget parent_instance;
  GtkBox *root;
  GtkBox *heading;
  GtkBox *titles;
  GtkLabel *title_label;
  GtkLabel *subtitle_label;
  GtkWidget *actions;
  GtkWidget *child;
  char *title;
  char *subtitle;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_ACTIONS,
  PROP_CHILD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraPageHeader, awra_page_header, GTK_TYPE_WIDGET)

static void
update_visibility (AwraPageHeader *self)
{
  gboolean has_title = self->title != NULL && *self->title != '\0';
  gboolean has_subtitle = self->subtitle != NULL && *self->subtitle != '\0';

  gtk_widget_set_visible (GTK_WIDGET (self->title_label), has_title);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle_label), has_subtitle);
  gtk_widget_set_visible (GTK_WIDGET (self->titles), has_title || has_subtitle);
  gtk_widget_set_visible (GTK_WIDGET (self->heading),
                          has_title || has_subtitle || self->actions != NULL);
}

static void
awra_page_header_dispose (GObject *object)
{
  AwraPageHeader *self = AWRA_PAGE_HEADER (object);

  if (self->root != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->root));
    self->root = NULL;
    self->heading = NULL;
    self->titles = NULL;
    self->title_label = NULL;
    self->subtitle_label = NULL;
    self->actions = NULL;
    self->child = NULL;
  }
  G_OBJECT_CLASS (awra_page_header_parent_class)->dispose (object);
}

static void
awra_page_header_finalize (GObject *object)
{
  AwraPageHeader *self = AWRA_PAGE_HEADER (object);

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  G_OBJECT_CLASS (awra_page_header_parent_class)->finalize (object);
}

static void
awra_page_header_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  AwraPageHeader *self = AWRA_PAGE_HEADER (widget);

  if (self->root != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->root), snapshot);
}

static void
awra_page_header_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  AwraPageHeader *self = AWRA_PAGE_HEADER (object);

  switch (property_id) {
  case PROP_TITLE:
    g_value_set_string (value, self->title);
    break;
  case PROP_SUBTITLE:
    g_value_set_string (value, self->subtitle);
    break;
  case PROP_ACTIONS:
    g_value_set_object (value, self->actions);
    break;
  case PROP_CHILD:
    g_value_set_object (value, self->child);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_page_header_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  AwraPageHeader *self = AWRA_PAGE_HEADER (object);

  switch (property_id) {
  case PROP_TITLE:
    awra_page_header_set_title (self, g_value_get_string (value));
    break;
  case PROP_SUBTITLE:
    awra_page_header_set_subtitle (self, g_value_get_string (value));
    break;
  case PROP_ACTIONS:
    awra_page_header_set_actions (self, g_value_get_object (value));
    break;
  case PROP_CHILD:
    awra_page_header_set_child (self, g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_page_header_class_init (AwraPageHeaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_page_header_dispose;
  object_class->finalize = awra_page_header_finalize;
  object_class->get_property = awra_page_header_get_property;
  object_class->set_property = awra_page_header_set_property;
  widget_class->snapshot = awra_page_header_snapshot;
  properties[PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SUBTITLE] = g_param_spec_string (
    "subtitle", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACTIONS] = g_param_spec_object (
    "actions", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CHILD] = g_param_spec_object (
    "child", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrapageheader");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_page_header_init (AwraPageHeader *self)
{
  self->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->heading = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->titles = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->title_label = GTK_LABEL (gtk_label_new (NULL));
  self->subtitle_label = GTK_LABEL (gtk_label_new (NULL));
  awra_box_set_spacing (self->root, AWRA_SPACING_MD);
  awra_box_set_spacing (self->heading, AWRA_SPACING_LG);
  awra_box_set_spacing (self->titles, AWRA_SPACING_XS);
  awra_widget_set_typography (GTK_WIDGET (self->title_label),
                              AWRA_TYPOGRAPHY_TITLE_1);
  awra_widget_set_typography (GTK_WIDGET (self->subtitle_label),
                              AWRA_TYPOGRAPHY_MUTED);
  gtk_label_set_xalign (self->title_label, 0.0f);
  gtk_label_set_xalign (self->subtitle_label, 0.0f);
  gtk_label_set_wrap (self->subtitle_label, TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->titles), TRUE);
  gtk_widget_set_valign (GTK_WIDGET (self->titles), GTK_ALIGN_CENTER);
  gtk_box_append (self->titles, GTK_WIDGET (self->title_label));
  gtk_box_append (self->titles, GTK_WIDGET (self->subtitle_label));
  gtk_box_append (self->heading, GTK_WIDGET (self->titles));
  gtk_box_append (self->root, GTK_WIDGET (self->heading));
  gtk_widget_set_parent (GTK_WIDGET (self->root), GTK_WIDGET (self));
  update_visibility (self);
}

GtkWidget *
awra_page_header_new (void)
{
  return g_object_new (AWRA_TYPE_PAGE_HEADER, NULL);
}

const char *
awra_page_header_get_title (AwraPageHeader *self)
{
  g_return_val_if_fail (AWRA_IS_PAGE_HEADER (self), NULL);
  return self->title;
}

void
awra_page_header_set_title (AwraPageHeader *self,
                            const char     *title)
{
  g_return_if_fail (AWRA_IS_PAGE_HEADER (self));
  if (g_strcmp0 (self->title, title) == 0)
    return;
  g_free (self->title);
  self->title = g_strdup (title);
  gtk_label_set_text (self->title_label, title != NULL ? title : "");
  update_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

const char *
awra_page_header_get_subtitle (AwraPageHeader *self)
{
  g_return_val_if_fail (AWRA_IS_PAGE_HEADER (self), NULL);
  return self->subtitle;
}

void
awra_page_header_set_subtitle (AwraPageHeader *self,
                               const char     *subtitle)
{
  g_return_if_fail (AWRA_IS_PAGE_HEADER (self));
  if (g_strcmp0 (self->subtitle, subtitle) == 0)
    return;
  g_free (self->subtitle);
  self->subtitle = g_strdup (subtitle);
  gtk_label_set_text (self->subtitle_label,
                      subtitle != NULL ? subtitle : "");
  update_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

GtkWidget *
awra_page_header_get_actions (AwraPageHeader *self)
{
  g_return_val_if_fail (AWRA_IS_PAGE_HEADER (self), NULL);
  return self->actions;
}

void
awra_page_header_set_actions (AwraPageHeader *self,
                              GtkWidget      *actions)
{
  g_return_if_fail (AWRA_IS_PAGE_HEADER (self));
  g_return_if_fail (actions == NULL || GTK_IS_WIDGET (actions));
  if (self->actions == actions)
    return;
  if (self->actions != NULL)
    gtk_box_remove (self->heading, self->actions);
  self->actions = actions;
  /* Actions sit on the title's line, whatever the subtitle under it: the
   * title takes a control's height, centred in it, and both align at the
   * top. */
  gtk_widget_set_valign (GTK_WIDGET (self->titles),
                         actions != NULL ? GTK_ALIGN_START : GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (GTK_WIDGET (self->title_label), -1, actions != NULL ? 34 : -1);
  if (actions != NULL) {
    gtk_widget_set_valign (actions, GTK_ALIGN_START);
    gtk_box_append (self->heading, actions);
  }
  update_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIONS]);
}

GtkWidget *
awra_page_header_get_child (AwraPageHeader *self)
{
  g_return_val_if_fail (AWRA_IS_PAGE_HEADER (self), NULL);
  return self->child;
}

void
awra_page_header_set_child (AwraPageHeader *self,
                            GtkWidget      *child)
{
  g_return_if_fail (AWRA_IS_PAGE_HEADER (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));
  if (self->child == child)
    return;
  if (self->child != NULL)
    gtk_box_remove (self->root, self->child);
  self->child = child;
  if (child != NULL)
    gtk_box_append (self->root, child);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}
