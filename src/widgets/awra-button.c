/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-button.h>
#include <awra/awra-button-content.h>
#include <awra/awra-context.h>

struct _AwraButton {
  GtkButton parent_instance;
  AwraStyleManager *style_manager;
  gulong style_handler;
  AwraButtonAppearance appearance;
  /* Shown over the hidden content while busy; GtkButton's bin layout
   * centres it in the button's own allocation. */
  GtkWidget *spinner;
  gboolean busy;
};

enum { PROP_0, PROP_APPEARANCE, PROP_BUSY, N_PROPS };
static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraButton, awra_button, GTK_TYPE_BUTTON)

static const char *appearance_classes[] = {
  "awra-primary",
  "awra-secondary",
  "awra-ghost",
  "awra-destructive",
  "awra-toolbar-button",
  "awra-success",
};

static void
update_button_appearance (AwraButton *self)
{
  for (guint i = 0; i < G_N_ELEMENTS (appearance_classes); i++)
    gtk_widget_remove_css_class (GTK_WIDGET (self), appearance_classes[i]);
  gtk_widget_add_css_class (GTK_WIDGET (self),
                            appearance_classes[self->appearance]);
}

static void
awra_button_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  if (property_id == PROP_APPEARANCE)
    g_value_set_enum (value, AWRA_BUTTON (object)->appearance);
  else if (property_id == PROP_BUSY)
    g_value_set_boolean (value, AWRA_BUTTON (object)->busy);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
awra_button_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  if (property_id == PROP_APPEARANCE)
    awra_button_set_appearance (AWRA_BUTTON (object), g_value_get_enum (value));
  else if (property_id == PROP_BUSY)
    awra_button_set_busy (AWRA_BUTTON (object), g_value_get_boolean (value));
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
update_appearance_class (AwraButton *self)
{
  gboolean dark = awra_token_set_get_dark (
    awra_style_manager_get_token_set (self->style_manager));

  gtk_widget_remove_css_class (GTK_WIDGET (self), dark ? "awra-light" : "awra-dark");
  gtk_widget_add_css_class (GTK_WIDGET (self), dark ? "awra-dark" : "awra-light");
  if (awra_style_manager_get_high_contrast (self->style_manager))
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-high-contrast");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-high-contrast");
}

static void
style_changed_cb (AwraStyleManager *manager,
                  GParamSpec       *pspec,
                  AwraButton       *self)
{
  (void) manager;
  (void) pspec;
  update_appearance_class (self);
}

static void
awra_button_map (GtkWidget *widget)
{
  AwraButton *self = AWRA_BUTTON (widget);
  AwraContext *context;

  GTK_WIDGET_CLASS (awra_button_parent_class)->map (widget);
  context = awra_context_get_for_display (gtk_widget_get_display (widget));
  self->style_manager = awra_context_get_style_manager (context);
  update_appearance_class (self);
  self->style_handler = g_signal_connect (self->style_manager,
                                          "notify::token-set",
                                          G_CALLBACK (style_changed_cb),
                                          self);
}

static void
awra_button_unmap (GtkWidget *widget)
{
  AwraButton *self = AWRA_BUTTON (widget);

  if (self->style_handler != 0) {
    g_signal_handler_disconnect (self->style_manager, self->style_handler);
    self->style_handler = 0;
    self->style_manager = NULL;
  }
  GTK_WIDGET_CLASS (awra_button_parent_class)->unmap (widget);
}

/* An icon alone tells a screen reader nothing: what its tooltip says is what it does. */
static void
sync_accessible_label (AwraButton *self)
{
  const char *tooltip = gtk_widget_get_tooltip_text (GTK_WIDGET (self));

  if (gtk_button_get_icon_name (GTK_BUTTON (self)) != NULL && tooltip != NULL && *tooltip != '\0')
    gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL, tooltip, -1);
  else
    gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL);
}

static void
awra_button_dispose (GObject *object)
{
  g_clear_pointer (&AWRA_BUTTON (object)->spinner, gtk_widget_unparent);
  G_OBJECT_CLASS (awra_button_parent_class)->dispose (object);
}

/* A busy button swallows its clicks before anyone else hears them. */
static void
clicked_cb (AwraButton *self)
{
  if (self->busy)
    g_signal_stop_emission_by_name (self, "clicked");
}

static void
awra_button_class_init (AwraButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = awra_button_get_property;
  object_class->set_property = awra_button_set_property;
  object_class->dispose = awra_button_dispose;
  widget_class->map = awra_button_map;
  widget_class->unmap = awra_button_unmap;
  gtk_widget_class_set_css_name (widget_class, "button");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
  properties[PROP_APPEARANCE] = g_param_spec_enum (
    "appearance", NULL, NULL, AWRA_TYPE_BUTTON_APPEARANCE,
    AWRA_BUTTON_APPEARANCE_SECONDARY,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraButton:busy:
   *
   * Whether the button's action is under way: it shows a spinner and
   * ignores clicks.
   *
   * Since: 1.1
   */
  properties[PROP_BUSY] = g_param_spec_boolean (
    "busy", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_button_init (AwraButton *self)
{
  self->appearance = AWRA_BUTTON_APPEARANCE_SECONDARY;
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-button");
  update_button_appearance (self);
  gtk_button_set_has_frame (GTK_BUTTON (self), FALSE);
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
  g_signal_connect (self, "clicked", G_CALLBACK (clicked_cb), NULL);
  g_signal_connect (self, "notify::tooltip-text", G_CALLBACK (sync_accessible_label), NULL);
  g_signal_connect (self, "notify::icon-name", G_CALLBACK (sync_accessible_label), NULL);
}

GtkWidget *
awra_button_new (void)
{
  return g_object_new (AWRA_TYPE_BUTTON, NULL);
}

GtkWidget *
awra_button_new_with_label (const char *label)
{
  return g_object_new (AWRA_TYPE_BUTTON, "label", label, NULL);
}

GtkWidget *
awra_button_new_from_icon_name (const char *icon_name)
{
  return g_object_new (AWRA_TYPE_BUTTON, "icon-name", icon_name, NULL);
}

AwraButtonAppearance
awra_button_get_appearance (AwraButton *self)
{
  g_return_val_if_fail (AWRA_IS_BUTTON (self),
                        AWRA_BUTTON_APPEARANCE_SECONDARY);
  return self->appearance;
}

void
awra_button_set_appearance (AwraButton           *self,
                            AwraButtonAppearance  appearance)
{
  g_return_if_fail (AWRA_IS_BUTTON (self));
  g_return_if_fail (appearance >= AWRA_BUTTON_APPEARANCE_PRIMARY &&
                    appearance <= AWRA_BUTTON_APPEARANCE_SUCCESS);
  if (self->appearance == appearance)
    return;
  self->appearance = appearance;
  update_button_appearance (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPEARANCE]);
}

GtkWidget *
awra_button_new_with_icon_and_label (const char *icon_name,
                                     const char *label)
{
  GtkWidget *button = awra_button_new ();

  gtk_button_set_child (GTK_BUTTON (button), awra_button_content_new (icon_name, label));
  return button;
}

gboolean
awra_button_get_busy (AwraButton *self)
{
  g_return_val_if_fail (AWRA_IS_BUTTON (self), FALSE);
  return self->busy;
}

void
awra_button_set_busy (AwraButton *self,
                      gboolean    busy)
{
  GtkWidget *child;

  g_return_if_fail (AWRA_IS_BUTTON (self));
  busy = !!busy;
  if (self->busy == busy)
    return;
  self->busy = busy;
  if (busy && self->spinner == NULL) {
    self->spinner = gtk_spinner_new ();
    gtk_widget_set_halign (self->spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (self->spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_can_target (self->spinner, FALSE);
    gtk_widget_set_parent (self->spinner, GTK_WIDGET (self));
  }
  if (self->spinner != NULL) {
    gtk_widget_set_visible (self->spinner, busy);
    gtk_spinner_set_spinning (GTK_SPINNER (self->spinner), busy);
  }
  /* The content keeps its room, so the button doesn't change size. */
  child = gtk_button_get_child (GTK_BUTTON (self));
  if (child != NULL)
    gtk_widget_set_opacity (child, busy ? 0.0 : 1.0);
  if (busy)
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-busy");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-busy");
  gtk_accessible_update_state (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_STATE_BUSY, busy, -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BUSY]);
}
