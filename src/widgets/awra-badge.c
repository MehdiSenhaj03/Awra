/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-badge.h>

struct _AwraBadge {
  GtkWidget parent_instance;
  GtkLabel *label;
  AwraBadgeAppearance appearance;
  GdkRGBA custom_color;
  gboolean custom_color_set;
};

enum {
  PROP_0,
  PROP_TEXT,
  PROP_APPEARANCE,
  PROP_CUSTOM_COLOR,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];
static const char *appearance_classes[] = {
  "awra-badge-neutral",
  "awra-badge-accent",
  "awra-badge-info",
  "awra-badge-success",
  "awra-badge-warning",
  "awra-badge-danger",
};

/* White text on a dark custom colour, near-black on a light one, so a
 * badge in any colour reads in both appearances. */
static void
update_custom_color_classes (AwraBadge *self)
{
  PangoAttrList *attributes;
  double luminance;
  guint16 level;

  if (!self->custom_color_set) {
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-badge-custom");
    if (self->label != NULL)
      gtk_label_set_attributes (self->label, NULL);
    return;
  }
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-badge-custom");
  if (self->label == NULL)
    return;
  luminance = 0.2126 * self->custom_color.red + 0.7152 * self->custom_color.green +
              0.0722 * self->custom_color.blue;
  level = luminance > 0.6 ? 0x1c1c : 0xffff;
  attributes = pango_attr_list_new ();
  pango_attr_list_insert (attributes, pango_attr_foreground_new (level, level, level));
  gtk_label_set_attributes (self->label, attributes);
  pango_attr_list_unref (attributes);
}

G_DEFINE_FINAL_TYPE (AwraBadge, awra_badge, GTK_TYPE_WIDGET)

static void
awra_badge_dispose (GObject *object)
{
  AwraBadge *self = AWRA_BADGE (object);

  if (self->label != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->label));
    self->label = NULL;
  }
  G_OBJECT_CLASS (awra_badge_parent_class)->dispose (object);
}

static void
awra_badge_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  AwraBadge *self = AWRA_BADGE (widget);

  if (self->custom_color_set &&
      gtk_widget_get_width (widget) > 0 &&
      gtk_widget_get_height (widget) > 0) {
    graphene_rect_t bounds = GRAPHENE_RECT_INIT (
      0, 0, gtk_widget_get_width (widget), gtk_widget_get_height (widget));
    GskRoundedRect rounded;
    GdkRGBA fill = self->custom_color;

    fill.alpha = CLAMP (fill.alpha, 0.0, 1.0);
    gsk_rounded_rect_init_from_rect (&rounded, &bounds,
                                     gtk_widget_get_height (widget) / 2.0f);
    gtk_snapshot_push_rounded_clip (snapshot, &rounded);
    gtk_snapshot_append_color (snapshot, &fill, &bounds);
    gtk_snapshot_pop (snapshot);
  }

  if (self->label != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->label), snapshot);
}

static void
awra_badge_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  if (property_id == PROP_TEXT)
    g_value_set_string (value, awra_badge_get_text (AWRA_BADGE (object)));
  else if (property_id == PROP_APPEARANCE)
    g_value_set_enum (value, awra_badge_get_appearance (AWRA_BADGE (object)));
  else if (property_id == PROP_CUSTOM_COLOR)
    g_value_set_boxed (value,
                       awra_badge_get_custom_color (AWRA_BADGE (object)));
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
awra_badge_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  if (property_id == PROP_TEXT)
    awra_badge_set_text (AWRA_BADGE (object), g_value_get_string (value));
  else if (property_id == PROP_APPEARANCE)
    awra_badge_set_appearance (AWRA_BADGE (object), g_value_get_enum (value));
  else if (property_id == PROP_CUSTOM_COLOR)
    awra_badge_set_custom_color (AWRA_BADGE (object),
                                 g_value_get_boxed (value));
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
awra_badge_class_init (AwraBadgeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_badge_dispose;
  object_class->get_property = awra_badge_get_property;
  object_class->set_property = awra_badge_set_property;
  widget_class->snapshot = awra_badge_snapshot;
  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL, NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);
  properties[PROP_APPEARANCE] =
    g_param_spec_enum ("appearance", NULL, NULL,
                       AWRA_TYPE_BADGE_APPEARANCE,
                       AWRA_BADGE_APPEARANCE_ACCENT,
                       G_PARAM_READWRITE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);
  properties[PROP_CUSTOM_COLOR] =
    g_param_spec_boxed ("custom-color", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrabadge");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_GENERIC);
}

static void
awra_badge_init (AwraBadge *self)
{
  self->label = GTK_LABEL (gtk_label_new (NULL));
  self->appearance = AWRA_BADGE_APPEARANCE_ACCENT;
  /* GtkBinLayout does not use the custom widget's CSS padding as content
   * insets. Keep the badge's breathing room on its internal label so the
   * advertised metrics are also its measured metrics. */
  gtk_widget_set_margin_start (GTK_WIDGET (self->label), 10);
  gtk_widget_set_margin_end (GTK_WIDGET (self->label), 10);
  gtk_widget_set_margin_top (GTK_WIDGET (self->label), 4);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self->label), 4);
  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_START);
  gtk_widget_add_css_class (GTK_WIDGET (self),
                            appearance_classes[self->appearance]);
  gtk_widget_set_parent (GTK_WIDGET (self->label), GTK_WIDGET (self));
}

GtkWidget *
awra_badge_new (const char *text)
{
  return g_object_new (AWRA_TYPE_BADGE, "text", text, NULL);
}

const char *
awra_badge_get_text (AwraBadge *self)
{
  g_return_val_if_fail (AWRA_IS_BADGE (self), NULL);
  return gtk_label_get_text (self->label);
}

void
awra_badge_set_text (AwraBadge  *self,
                     const char *text)
{
  g_return_if_fail (AWRA_IS_BADGE (self));

  if (g_strcmp0 (gtk_label_get_text (self->label), text) == 0)
    return;
  gtk_label_set_text (self->label, text != NULL ? text : "");
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  text != NULL ? text : "",
                                  -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

AwraBadgeAppearance
awra_badge_get_appearance (AwraBadge *self)
{
  g_return_val_if_fail (AWRA_IS_BADGE (self),
                        AWRA_BADGE_APPEARANCE_NEUTRAL);
  return self->appearance;
}

void
awra_badge_set_appearance (AwraBadge           *self,
                           AwraBadgeAppearance  appearance)
{
  g_return_if_fail (AWRA_IS_BADGE (self));
  g_return_if_fail (appearance >= AWRA_BADGE_APPEARANCE_NEUTRAL &&
                    appearance <= AWRA_BADGE_APPEARANCE_DANGER);
  if (self->appearance == appearance)
    return;
  gtk_widget_remove_css_class (GTK_WIDGET (self),
                               appearance_classes[self->appearance]);
  self->appearance = appearance;
  gtk_widget_add_css_class (GTK_WIDGET (self),
                            appearance_classes[self->appearance]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPEARANCE]);
}

const GdkRGBA *
awra_badge_get_custom_color (AwraBadge *self)
{
  g_return_val_if_fail (AWRA_IS_BADGE (self), NULL);
  return self->custom_color_set ? &self->custom_color : NULL;
}

void
awra_badge_set_custom_color (AwraBadge     *self,
                             const GdkRGBA *color)
{
  g_return_if_fail (AWRA_IS_BADGE (self));
  if ((color == NULL && !self->custom_color_set) ||
      (color != NULL && self->custom_color_set &&
       gdk_rgba_equal (color, &self->custom_color)))
    return;
  if (color != NULL) {
    self->custom_color = *color;
    self->custom_color_set = TRUE;
  } else {
    self->custom_color_set = FALSE;
  }
  update_custom_color_classes (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_COLOR]);
}
