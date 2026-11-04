/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-button-content.h>

/* An icon and a label side by side, for buttons that need both. The label
 * names the button for assistive technologies; the icon is decoration. */

struct _AwraButtonContent {
  GtkWidget parent_instance;
  GtkWidget *image;
  GtkWidget *label;
  char *icon_name;
};

enum { PROP_0, PROP_ICON_NAME, PROP_LABEL, N_PROPS };
static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraButtonContent, awra_button_content, GTK_TYPE_WIDGET)

static void
awra_button_content_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  AwraButtonContent *self = AWRA_BUTTON_CONTENT (object);

  switch (property_id) {
  case PROP_ICON_NAME:
    g_value_set_string (value, self->icon_name);
    break;
  case PROP_LABEL:
    g_value_set_string (value, awra_button_content_get_label (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_button_content_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  AwraButtonContent *self = AWRA_BUTTON_CONTENT (object);

  switch (property_id) {
  case PROP_ICON_NAME:
    awra_button_content_set_icon_name (self, g_value_get_string (value));
    break;
  case PROP_LABEL:
    awra_button_content_set_label (self, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_button_content_dispose (GObject *object)
{
  AwraButtonContent *self = AWRA_BUTTON_CONTENT (object);

  g_clear_pointer (&self->image, gtk_widget_unparent);
  g_clear_pointer (&self->label, gtk_widget_unparent);
  G_OBJECT_CLASS (awra_button_content_parent_class)->dispose (object);
}

static void
awra_button_content_finalize (GObject *object)
{
  g_free (AWRA_BUTTON_CONTENT (object)->icon_name);
  G_OBJECT_CLASS (awra_button_content_parent_class)->finalize (object);
}

static void
awra_button_content_class_init (AwraButtonContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = awra_button_content_get_property;
  object_class->set_property = awra_button_content_set_property;
  object_class->dispose = awra_button_content_dispose;
  object_class->finalize = awra_button_content_finalize;
  gtk_widget_class_set_css_name (widget_class, "awrabuttoncontent");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);

  /**
   * AwraButtonContent:icon-name:
   *
   * The icon shown before the label.
   *
   * Since: 1.1
   */
  properties[PROP_ICON_NAME] = g_param_spec_string (
    "icon-name", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraButtonContent:label:
   *
   * The label.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL] = g_param_spec_string (
    "label", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_button_content_init (AwraButtonContent *self)
{
  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  self->image = gtk_image_new ();
  gtk_widget_set_visible (self->image, FALSE);
  gtk_accessible_update_state (GTK_ACCESSIBLE (self->image),
                               GTK_ACCESSIBLE_STATE_HIDDEN, TRUE, -1);
  gtk_widget_set_parent (self->image, GTK_WIDGET (self));
  self->label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (self->label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_visible (self->label, FALSE);
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));
}

GtkWidget *
awra_button_content_new (const char *icon_name,
                         const char *label)
{
  return g_object_new (AWRA_TYPE_BUTTON_CONTENT, "icon-name", icon_name, "label", label, NULL);
}

const char *
awra_button_content_get_icon_name (AwraButtonContent *self)
{
  g_return_val_if_fail (AWRA_IS_BUTTON_CONTENT (self), NULL);
  return self->icon_name;
}

void
awra_button_content_set_icon_name (AwraButtonContent *self,
                                   const char        *icon_name)
{
  g_return_if_fail (AWRA_IS_BUTTON_CONTENT (self));
  if (icon_name != NULL && *icon_name == '\0')
    icon_name = NULL;
  if (!g_set_str (&self->icon_name, icon_name))
    return;
  gtk_image_set_from_icon_name (GTK_IMAGE (self->image), icon_name);
  gtk_widget_set_visible (self->image, icon_name != NULL);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

const char *
awra_button_content_get_label (AwraButtonContent *self)
{
  const char *label;

  g_return_val_if_fail (AWRA_IS_BUTTON_CONTENT (self), NULL);
  label = gtk_label_get_label (GTK_LABEL (self->label));
  return label != NULL && *label != '\0' ? label : NULL;
}

void
awra_button_content_set_label (AwraButtonContent *self,
                               const char        *label)
{
  g_return_if_fail (AWRA_IS_BUTTON_CONTENT (self));
  if (label != NULL && *label == '\0')
    label = NULL;
  if (g_strcmp0 (awra_button_content_get_label (self), label) == 0)
    return;
  gtk_label_set_label (GTK_LABEL (self->label), label);
  gtk_widget_set_visible (self->label, label != NULL);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, label != NULL ? label : "", -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}
