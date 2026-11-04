/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-navigation-item.h>
#include <awra/awra-layout.h>
#include <awra/awra-token-set.h>

#include "widgets/awra-widget-style-private.h"

struct _AwraNavigationItem {
  GtkToggleButton parent_instance;
  GtkImage *icon;
  GtkLabel *label;
  GtkLabel *subtitle;
  GtkBox *box;
  GtkWidget *prefix;
  GtkWidget *suffix;
  char *icon_name;
  gboolean emphasized;
  gboolean muted;
  GdkRGBA label_color;
  gboolean has_label_color;
  AwraStyleWatch style_watch;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ICON_NAME,
  PROP_SUFFIX,
  PROP_SELECTED,
  PROP_PREFIX,
  PROP_SUBTITLE,
  PROP_EMPHASIZED,
  PROP_MUTED,
  PROP_LABEL_COLOR,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraNavigationItem, awra_navigation_item,
                     GTK_TYPE_TOGGLE_BUTTON)

static void
active_notify_cb (AwraNavigationItem *self,
                  GParamSpec         *pspec,
                  gpointer            user_data)
{
  (void) pspec;
  (void) user_data;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
}

/* The label colour, made readable on the current appearance. */
static void
update_label_color (AwraNavigationItem *self)
{
  PangoAttrList *attributes = NULL;

  if (self->has_label_color) {
    gboolean dark = awra_token_set_get_dark (awra_widget_get_token_set (GTK_WIDGET (self)));
    GdkRGBA color = awra_rgba_readable (&self->label_color, dark);

    attributes = pango_attr_list_new ();
    pango_attr_list_insert (attributes,
                            pango_attr_foreground_new ((guint16) (color.red * 65535),
                                                       (guint16) (color.green * 65535),
                                                       (guint16) (color.blue * 65535)));
  }
  gtk_label_set_attributes (self->label, attributes);
  g_clear_pointer (&attributes, pango_attr_list_unref);
}

static void
awra_navigation_item_map (GtkWidget *widget)
{
  AwraNavigationItem *self = AWRA_NAVIGATION_ITEM (widget);

  GTK_WIDGET_CLASS (awra_navigation_item_parent_class)->map (widget);
  if (self->has_label_color) {
    awra_style_watch_start (&self->style_watch, widget,
                            (AwraStyleWatchFunc) update_label_color, self);
    update_label_color (self);
  }
}

static void
awra_navigation_item_unmap (GtkWidget *widget)
{
  awra_style_watch_stop (&AWRA_NAVIGATION_ITEM (widget)->style_watch);
  GTK_WIDGET_CLASS (awra_navigation_item_parent_class)->unmap (widget);
}

static void
awra_navigation_item_finalize (GObject *object)
{
  g_clear_pointer (&AWRA_NAVIGATION_ITEM (object)->icon_name, g_free);
  G_OBJECT_CLASS (awra_navigation_item_parent_class)->finalize (object);
}

static void
awra_navigation_item_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  AwraNavigationItem *self = AWRA_NAVIGATION_ITEM (object);

  switch (property_id) {
  case PROP_LABEL:
    g_value_set_string (value, awra_navigation_item_get_label (self));
    break;
  case PROP_ICON_NAME:
    g_value_set_string (value, self->icon_name);
    break;
  case PROP_SUFFIX:
    g_value_set_object (value, self->suffix);
    break;
  case PROP_SELECTED:
    g_value_set_boolean (value, awra_navigation_item_get_selected (self));
    break;
  case PROP_PREFIX:
    g_value_set_object (value, self->prefix);
    break;
  case PROP_SUBTITLE:
    g_value_set_string (value, awra_navigation_item_get_subtitle (self));
    break;
  case PROP_EMPHASIZED:
    g_value_set_boolean (value, self->emphasized);
    break;
  case PROP_LABEL_COLOR:
    g_value_set_boxed (value, awra_navigation_item_get_label_color (self));
    break;
  case PROP_MUTED:
    g_value_set_boolean (value, self->muted);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_navigation_item_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  AwraNavigationItem *self = AWRA_NAVIGATION_ITEM (object);

  switch (property_id) {
  case PROP_LABEL:
    awra_navigation_item_set_label (self, g_value_get_string (value));
    break;
  case PROP_ICON_NAME:
    awra_navigation_item_set_icon_name (self, g_value_get_string (value));
    break;
  case PROP_SUFFIX:
    awra_navigation_item_set_suffix (self, g_value_get_object (value));
    break;
  case PROP_SELECTED:
    awra_navigation_item_set_selected (self, g_value_get_boolean (value));
    break;
  case PROP_PREFIX:
    awra_navigation_item_set_prefix (self, g_value_get_object (value));
    break;
  case PROP_SUBTITLE:
    awra_navigation_item_set_subtitle (self, g_value_get_string (value));
    break;
  case PROP_EMPHASIZED:
    awra_navigation_item_set_emphasized (self, g_value_get_boolean (value));
    break;
  case PROP_LABEL_COLOR:
    awra_navigation_item_set_label_color (self, g_value_get_boxed (value));
    break;
  case PROP_MUTED:
    awra_navigation_item_set_muted (self, g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_navigation_item_class_init (AwraNavigationItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = awra_navigation_item_finalize;
  widget_class->map = awra_navigation_item_map;
  widget_class->unmap = awra_navigation_item_unmap;
  object_class->get_property = awra_navigation_item_get_property;
  object_class->set_property = awra_navigation_item_set_property;
  properties[PROP_LABEL] = g_param_spec_string (
    "label", NULL, NULL, "",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ICON_NAME] = g_param_spec_string (
    "icon-name", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SUFFIX] = g_param_spec_object (
    "suffix", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SELECTED] = g_param_spec_boolean (
    "selected", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraNavigationItem:prefix:
   *
   * A leading widget shown in place of the icon, such as an avatar or a
   * color swatch.
   *
   * Since: 1.1
   */
  properties[PROP_PREFIX] = g_param_spec_object (
    "prefix", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraNavigationItem:subtitle:
   *
   * A second, quieter line under the label, such as a status or a count;
   * hidden while empty.
   *
   * Since: 1.1
   */
  properties[PROP_SUBTITLE] = g_param_spec_string (
    "subtitle", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraNavigationItem:emphasized:
   *
   * The destination has something new: its label reads at full contrast
   * and weight, as unread conversations or folders with new mail do.
   *
   * Since: 1.1
   */
  properties[PROP_EMPHASIZED] = g_param_spec_boolean (
    "emphasized", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraNavigationItem:muted:
   *
   * The destination is silenced: it recedes until hovered or selected.
   *
   * Since: 1.1
   */
  properties[PROP_MUTED] = g_param_spec_boolean (
    "muted", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraNavigationItem:label-color:
   *
   * A colour that belongs to the destination, such as a person's role
   * colour, for the label. Awra lightens or darkens it as the appearance
   * needs, so it always reads.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL_COLOR] = g_param_spec_boxed (
    "label-color", NULL, NULL, GDK_TYPE_RGBA,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "button");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_BUTTON);
}

static void
awra_navigation_item_init (AwraNavigationItem *self)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  awra_box_set_spacing (GTK_BOX (box), AWRA_SPACING_MD);
  self->box = GTK_BOX (box);
  self->icon = GTK_IMAGE (gtk_image_new ());
  self->label = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-navigation-item");
  gtk_widget_add_css_class (box, "awra-navigation-item-box");
  gtk_widget_add_css_class (GTK_WIDGET (self->icon),
                            "awra-navigation-item-icon");
  gtk_widget_add_css_class (GTK_WIDGET (self->label),
                            "awra-navigation-item-label");
  gtk_widget_set_visible (GTK_WIDGET (self->icon), FALSE);
  gtk_label_set_xalign (self->label, 0.0f);
  gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_END);
  self->subtitle = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self->subtitle),
                            "awra-navigation-item-subtitle");
  gtk_label_set_xalign (self->subtitle, 0.0f);
  gtk_label_set_ellipsize (self->subtitle, PANGO_ELLIPSIZE_END);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), FALSE);
  GtkWidget *texts = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class (texts, "awra-navigation-item-texts");
  gtk_widget_set_hexpand (texts, TRUE);
  gtk_widget_set_valign (texts, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (texts), GTK_WIDGET (self->label));
  gtk_box_append (GTK_BOX (texts), GTK_WIDGET (self->subtitle));
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->icon));
  gtk_box_append (GTK_BOX (box), texts);
  gtk_button_set_child (GTK_BUTTON (self), box);
  gtk_button_set_has_frame (GTK_BUTTON (self), FALSE);
  g_signal_connect (self, "notify::active", G_CALLBACK (active_notify_cb), NULL);
}

GtkWidget *
awra_navigation_item_new (void)
{
  return g_object_new (AWRA_TYPE_NAVIGATION_ITEM, NULL);
}

GtkWidget *
awra_navigation_item_new_with_label (const char *label)
{
  return g_object_new (AWRA_TYPE_NAVIGATION_ITEM, "label", label, NULL);
}

const char *
awra_navigation_item_get_label (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), NULL);
  return gtk_label_get_text (self->label);
}

void
awra_navigation_item_set_label (AwraNavigationItem *self,
                                const char         *label)
{
  const char *value;

  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  value = label != NULL ? label : "";
  if (g_strcmp0 (awra_navigation_item_get_label (self), value) == 0)
    return;
  gtk_label_set_text (self->label, value);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, value,
                                  -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

const char *
awra_navigation_item_get_icon_name (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), NULL);
  return self->icon_name;
}

void
awra_navigation_item_set_icon_name (AwraNavigationItem *self,
                                    const char         *icon_name)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  if (g_strcmp0 (self->icon_name, icon_name) == 0)
    return;
  g_free (self->icon_name);
  self->icon_name = g_strdup (icon_name);
  if (icon_name != NULL && *icon_name != '\0') {
    gtk_image_set_from_icon_name (self->icon, icon_name);
    gtk_widget_set_visible (GTK_WIDGET (self->icon), self->prefix == NULL);
  } else {
    gtk_image_clear (self->icon);
    gtk_widget_set_visible (GTK_WIDGET (self->icon), FALSE);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

GtkWidget *
awra_navigation_item_get_suffix (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), NULL);
  return self->suffix;
}

void
awra_navigation_item_set_suffix (AwraNavigationItem *self,
                                 GtkWidget          *suffix)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  g_return_if_fail (suffix == NULL || GTK_IS_WIDGET (suffix));
  g_return_if_fail (suffix == NULL || gtk_widget_get_parent (suffix) == NULL ||
                    gtk_widget_get_parent (suffix) == GTK_WIDGET (self->box));

  if (self->suffix == suffix)
    return;
  if (self->suffix != NULL)
    gtk_box_remove (self->box, self->suffix);
  self->suffix = suffix;
  if (suffix != NULL) {
    gtk_widget_add_css_class (suffix, "awra-navigation-item-suffix");
    gtk_box_append (self->box, suffix);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUFFIX]);
}

gboolean
awra_navigation_item_get_selected (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), FALSE);
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self));
}

void
awra_navigation_item_set_selected (AwraNavigationItem *self,
                                   gboolean            selected)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self), !!selected);
}

void
awra_navigation_item_set_group (AwraNavigationItem *self,
                                AwraNavigationItem *group)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  g_return_if_fail (group == NULL || AWRA_IS_NAVIGATION_ITEM (group));
  gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (self),
                               group != NULL ? GTK_TOGGLE_BUTTON (group) : NULL);
}

GtkWidget *
awra_navigation_item_get_prefix (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), NULL);
  return self->prefix;
}

void
awra_navigation_item_set_prefix (AwraNavigationItem *self,
                                 GtkWidget          *prefix)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  g_return_if_fail (prefix == NULL || GTK_IS_WIDGET (prefix));
  g_return_if_fail (prefix == NULL || gtk_widget_get_parent (prefix) == NULL ||
                    gtk_widget_get_parent (prefix) == GTK_WIDGET (self->box));

  if (self->prefix == prefix)
    return;
  if (self->prefix != NULL)
    gtk_box_remove (self->box, self->prefix);
  self->prefix = prefix;
  if (prefix != NULL) {
    gtk_widget_add_css_class (prefix, "awra-navigation-item-prefix");
    gtk_widget_set_valign (prefix, GTK_ALIGN_CENTER);
    gtk_box_insert_child_after (self->box, prefix, NULL);
  }
  gtk_widget_set_visible (GTK_WIDGET (self->icon),
                          prefix == NULL && self->icon_name != NULL &&
                          *self->icon_name != '\0');
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PREFIX]);
}

const char *
awra_navigation_item_get_subtitle (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), NULL);
  return gtk_label_get_text (self->subtitle);
}

void
awra_navigation_item_set_subtitle (AwraNavigationItem *self,
                                   const char         *subtitle)
{
  const char *value;

  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  value = subtitle != NULL ? subtitle : "";
  if (g_strcmp0 (gtk_label_get_text (self->subtitle), value) == 0)
    return;
  gtk_label_set_text (self->subtitle, value);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), *value != '\0');
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, value,
                                  -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

gboolean
awra_navigation_item_get_emphasized (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), FALSE);
  return self->emphasized;
}

void
awra_navigation_item_set_emphasized (AwraNavigationItem *self,
                                     gboolean            emphasized)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  emphasized = !!emphasized;
  if (self->emphasized == emphasized)
    return;
  self->emphasized = emphasized;
  if (emphasized)
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-emphasized");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-emphasized");
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EMPHASIZED]);
}

gboolean
awra_navigation_item_get_muted (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), FALSE);
  return self->muted;
}

void
awra_navigation_item_set_muted (AwraNavigationItem *self,
                                gboolean            muted)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  muted = !!muted;
  if (self->muted == muted)
    return;
  self->muted = muted;
  if (muted)
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-muted");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-muted");
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MUTED]);
}

const GdkRGBA *
awra_navigation_item_get_label_color (AwraNavigationItem *self)
{
  g_return_val_if_fail (AWRA_IS_NAVIGATION_ITEM (self), NULL);
  return self->has_label_color ? &self->label_color : NULL;
}

void
awra_navigation_item_set_label_color (AwraNavigationItem *self,
                                      const GdkRGBA      *color)
{
  g_return_if_fail (AWRA_IS_NAVIGATION_ITEM (self));
  if (color == NULL && !self->has_label_color)
    return;
  if (color != NULL && self->has_label_color && gdk_rgba_equal (color, &self->label_color))
    return;
  self->has_label_color = color != NULL;
  if (color != NULL)
    self->label_color = *color;
  if (gtk_widget_get_mapped (GTK_WIDGET (self))) {
    if (color != NULL && self->style_watch.style == NULL)
      awra_style_watch_start (&self->style_watch, GTK_WIDGET (self),
                              (AwraStyleWatchFunc) update_label_color, self);
    else if (color == NULL)
      awra_style_watch_stop (&self->style_watch);
  }
  update_label_color (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL_COLOR]);
}
