/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-button.h>
#include <awra/awra-layout.h>
#include <awra/awra-material.h>
#include <awra/awra-status-banner.h>
#include <awra/awra-surface.h>
#include <awra/awra-typography.h>

struct _AwraStatusBanner {
  GtkWidget parent_instance;
  AwraSurface *surface;
  GtkBox *root;
  GtkImage *icon;
  GtkBox *copy;
  GtkLabel *title_label;
  GtkLabel *description_label;
  GtkWidget *action;
  GtkWidget *dismiss_button;
  char *title;
  char *description;
  AwraStatusAppearance appearance;
  gboolean dismissible;
  gboolean revealed;
};

enum { PROP_0, PROP_TITLE, PROP_DESCRIPTION, PROP_APPEARANCE, PROP_ACTION,
       PROP_DISMISSIBLE, PROP_REVEALED, N_PROPS };
enum { DISMISSED, N_SIGNALS };
static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraStatusBanner, awra_status_banner, GTK_TYPE_WIDGET)

static const char *appearance_classes[] = {
  "awra-status-info", "awra-status-success", "awra-status-warning",
  "awra-status-error",
};
static const char *appearance_icons[] = {
  /* Bundled with Awra: not every icon theme ships these (Breeze has no
   * dialog-information-symbolic, and the banner showed an empty slot). */
  "awra-status-info-symbolic", "awra-status-success-symbolic",
  "awra-status-warning-symbolic", "awra-status-error-symbolic",
};

static void
update_appearance (AwraStatusBanner *self)
{
  for (guint i = 0; i < G_N_ELEMENTS (appearance_classes); i++)
    gtk_widget_remove_css_class (GTK_WIDGET (self), appearance_classes[i]);
  gtk_widget_add_css_class (GTK_WIDGET (self),
                            appearance_classes[self->appearance]);
  gtk_image_set_from_icon_name (self->icon, appearance_icons[self->appearance]);
}

static void
dismiss_clicked_cb (GtkButton *button,
                    AwraStatusBanner *self)
{
  (void) button;
  awra_status_banner_set_revealed (self, FALSE);
  g_signal_emit (self, signals[DISMISSED], 0);
}

static void
awra_status_banner_dispose (GObject *object)
{
  AwraStatusBanner *self = AWRA_STATUS_BANNER (object);
  if (self->surface != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->surface));
    self->surface = NULL;
    self->root = NULL;
    self->icon = NULL;
    self->copy = NULL;
    self->title_label = NULL;
    self->description_label = NULL;
    self->action = NULL;
    self->dismiss_button = NULL;
  }
  G_OBJECT_CLASS (awra_status_banner_parent_class)->dispose (object);
}

static void
awra_status_banner_finalize (GObject *object)
{
  AwraStatusBanner *self = AWRA_STATUS_BANNER (object);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->description, g_free);
  G_OBJECT_CLASS (awra_status_banner_parent_class)->finalize (object);
}

static void
awra_status_banner_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  gtk_widget_snapshot_child (
    widget, GTK_WIDGET (AWRA_STATUS_BANNER (widget)->surface), snapshot);
}

static void
awra_status_banner_get_property (GObject *object, guint id, GValue *value,
                                 GParamSpec *pspec)
{
  AwraStatusBanner *self = AWRA_STATUS_BANNER (object);
  switch (id) {
  case PROP_TITLE: g_value_set_string (value, self->title); break;
  case PROP_DESCRIPTION: g_value_set_string (value, self->description); break;
  case PROP_APPEARANCE: g_value_set_enum (value, self->appearance); break;
  case PROP_ACTION: g_value_set_object (value, self->action); break;
  case PROP_DISMISSIBLE: g_value_set_boolean (value, self->dismissible); break;
  case PROP_REVEALED: g_value_set_boolean (value, self->revealed); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_status_banner_set_property (GObject *object, guint id,
                                 const GValue *value, GParamSpec *pspec)
{
  AwraStatusBanner *self = AWRA_STATUS_BANNER (object);
  switch (id) {
  case PROP_TITLE: awra_status_banner_set_title (self, g_value_get_string (value)); break;
  case PROP_DESCRIPTION: awra_status_banner_set_description (self, g_value_get_string (value)); break;
  case PROP_APPEARANCE: awra_status_banner_set_appearance (self, g_value_get_enum (value)); break;
  case PROP_ACTION: awra_status_banner_set_action (self, g_value_get_object (value)); break;
  case PROP_DISMISSIBLE: awra_status_banner_set_dismissible (self, g_value_get_boolean (value)); break;
  case PROP_REVEALED: awra_status_banner_set_revealed (self, g_value_get_boolean (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_status_banner_class_init (AwraStatusBannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_status_banner_dispose;
  object_class->finalize = awra_status_banner_finalize;
  object_class->get_property = awra_status_banner_get_property;
  object_class->set_property = awra_status_banner_set_property;
  widget_class->snapshot = awra_status_banner_snapshot;
  properties[PROP_TITLE] = g_param_spec_string ("title", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_DESCRIPTION] = g_param_spec_string ("description", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_APPEARANCE] = g_param_spec_enum ("appearance", NULL, NULL, AWRA_TYPE_STATUS_APPEARANCE, AWRA_STATUS_APPEARANCE_INFO, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACTION] = g_param_spec_object ("action", NULL, NULL, GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_DISMISSIBLE] = g_param_spec_boolean ("dismissible", NULL, NULL, FALSE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_REVEALED] = g_param_spec_boolean ("revealed", NULL, NULL, TRUE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  signals[DISMISSED] = g_signal_new ("dismissed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrastatusbanner");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_STATUS);
}

static void
awra_status_banner_init (AwraStatusBanner *self)
{
  g_autoptr (AwraMaterial) material = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_LAYER);

  self->revealed = TRUE;
  self->surface = AWRA_SURFACE (awra_surface_new_with_role (
    AWRA_SURFACE_ROLE_CARD));
  awra_surface_set_material (self->surface, material);
  awra_surface_set_elevation (self->surface, AWRA_ELEVATION_FLAT);
  self->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->icon = GTK_IMAGE (gtk_image_new ());
  self->copy = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->title_label = GTK_LABEL (gtk_label_new (NULL));
  self->description_label = GTK_LABEL (gtk_label_new (NULL));
  self->dismiss_button = awra_button_new_from_icon_name ("window-close-symbolic");
  awra_button_set_appearance (AWRA_BUTTON (self->dismiss_button),
                              AWRA_BUTTON_APPEARANCE_GHOST);
  awra_box_set_spacing (self->root, AWRA_SPACING_MD);
  awra_box_set_spacing (self->copy, AWRA_SPACING_XS);
  awra_widget_set_margin (GTK_WIDGET (self->root), AWRA_SPACING_MD);
  awra_widget_set_typography (GTK_WIDGET (self->title_label), AWRA_TYPOGRAPHY_TITLE_3);
  awra_widget_set_typography (GTK_WIDGET (self->description_label), AWRA_TYPOGRAPHY_MUTED);
  gtk_label_set_xalign (self->title_label, 0.0f);
  gtk_label_set_xalign (self->description_label, 0.0f);
  gtk_label_set_wrap (self->title_label, TRUE);
  gtk_label_set_wrap (self->description_label, TRUE);
  /* Wrapping labels report their longest word as minimum; keep the copy from
   * claiming its whole natural width in narrow side panels. */
  gtk_label_set_natural_wrap_mode (self->title_label,
                                   GTK_NATURAL_WRAP_WORD);
  gtk_widget_set_hexpand (GTK_WIDGET (self->copy), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->title_label), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->description_label), FALSE);
  gtk_widget_set_visible (self->dismiss_button, FALSE);
  gtk_box_append (self->copy, GTK_WIDGET (self->title_label));
  gtk_box_append (self->copy, GTK_WIDGET (self->description_label));
  gtk_box_append (self->root, GTK_WIDGET (self->icon));
  gtk_box_append (self->root, GTK_WIDGET (self->copy));
  gtk_box_append (self->root, self->dismiss_button);
  awra_surface_set_child (self->surface, GTK_WIDGET (self->root));
  gtk_widget_set_parent (GTK_WIDGET (self->surface), GTK_WIDGET (self));
  g_signal_connect (self->dismiss_button, "clicked", G_CALLBACK (dismiss_clicked_cb), self);
  update_appearance (self);
}

GtkWidget *awra_status_banner_new (void) { return g_object_new (AWRA_TYPE_STATUS_BANNER, NULL); }
const char *awra_status_banner_get_title (AwraStatusBanner *self) { g_return_val_if_fail (AWRA_IS_STATUS_BANNER (self), NULL); return self->title != NULL ? self->title : ""; }
void awra_status_banner_set_title (AwraStatusBanner *self, const char *title) { g_return_if_fail (AWRA_IS_STATUS_BANNER (self)); if (g_strcmp0 (self->title, title) == 0) return; g_free (self->title); self->title = g_strdup (title); gtk_label_set_text (self->title_label, title != NULL ? title : ""); gtk_widget_set_visible (GTK_WIDGET (self->title_label), title != NULL && *title != '\0'); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]); }
const char *awra_status_banner_get_description (AwraStatusBanner *self) { g_return_val_if_fail (AWRA_IS_STATUS_BANNER (self), NULL); return self->description != NULL ? self->description : ""; }
void awra_status_banner_set_description (AwraStatusBanner *self, const char *description) { g_return_if_fail (AWRA_IS_STATUS_BANNER (self)); if (g_strcmp0 (self->description, description) == 0) return; g_free (self->description); self->description = g_strdup (description); gtk_label_set_text (self->description_label, description != NULL ? description : ""); gtk_widget_set_visible (GTK_WIDGET (self->description_label), description != NULL && *description != '\0'); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DESCRIPTION]); }
AwraStatusAppearance awra_status_banner_get_appearance (AwraStatusBanner *self) { g_return_val_if_fail (AWRA_IS_STATUS_BANNER (self), AWRA_STATUS_APPEARANCE_INFO); return self->appearance; }
void awra_status_banner_set_appearance (AwraStatusBanner *self, AwraStatusAppearance appearance) { g_return_if_fail (AWRA_IS_STATUS_BANNER (self)); g_return_if_fail (appearance >= AWRA_STATUS_APPEARANCE_INFO && appearance <= AWRA_STATUS_APPEARANCE_ERROR); if (self->appearance == appearance) return; self->appearance = appearance; update_appearance (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPEARANCE]); }
GtkWidget *awra_status_banner_get_action (AwraStatusBanner *self) { g_return_val_if_fail (AWRA_IS_STATUS_BANNER (self), NULL); return self->action; }
void awra_status_banner_set_action (AwraStatusBanner *self, GtkWidget *action) { g_return_if_fail (AWRA_IS_STATUS_BANNER (self)); g_return_if_fail (action == NULL || GTK_IS_WIDGET (action)); if (self->action == action) return; if (self->action != NULL) gtk_box_remove (self->root, self->action); self->action = action; if (action != NULL) { gtk_widget_set_valign (action, GTK_ALIGN_CENTER); gtk_box_insert_child_after (self->root, action, GTK_WIDGET (self->copy)); } g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTION]); }
gboolean awra_status_banner_get_dismissible (AwraStatusBanner *self) { g_return_val_if_fail (AWRA_IS_STATUS_BANNER (self), FALSE); return self->dismissible; }
void awra_status_banner_set_dismissible (AwraStatusBanner *self, gboolean dismissible) { g_return_if_fail (AWRA_IS_STATUS_BANNER (self)); dismissible = !!dismissible; if (self->dismissible == dismissible) return; self->dismissible = dismissible; gtk_widget_set_visible (self->dismiss_button, dismissible); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DISMISSIBLE]); }
gboolean awra_status_banner_get_revealed (AwraStatusBanner *self) { g_return_val_if_fail (AWRA_IS_STATUS_BANNER (self), FALSE); return self->revealed; }
void awra_status_banner_set_revealed (AwraStatusBanner *self, gboolean revealed) { g_return_if_fail (AWRA_IS_STATUS_BANNER (self)); revealed = !!revealed; if (self->revealed == revealed) return; self->revealed = revealed; gtk_widget_set_visible (GTK_WIDGET (self), revealed); gtk_accessible_update_state (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_STATE_HIDDEN, !revealed, -1); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REVEALED]); }
