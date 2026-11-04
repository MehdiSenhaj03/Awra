/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-button.h>
#include <awra/awra-dialog.h>
#include <awra/awra-layout.h>

/* A dialog either shows content set with awra_window_set_content(), or
 * manages its own: a body above a row of responses. Escape closes it either
 * way, answering its close response when it has one. */

typedef struct {
  char *id;
  GtkWidget *button;
  gboolean stays;
} Response;

struct _AwraDialog {
  AwraWindow parent_instance;
  GtkWidget *layout;
  GtkWidget *body;
  GtkWidget *actions;
  GPtrArray *responses;
  char *default_response;
  char *close_response;
  gboolean responding;
};

enum {
  PROP_0,
  PROP_BODY,
  PROP_DEFAULT_RESPONSE,
  PROP_CLOSE_RESPONSE,
  N_PROPS,
};

enum {
  SIGNAL_RESPONSE,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraDialog, awra_dialog, AWRA_TYPE_WINDOW)

static void
response_free (gpointer data)
{
  Response *response = data;

  g_free (response->id);
  g_free (response);
}

static Response *
find_response (AwraDialog *self,
               const char *id)
{
  for (guint i = 0; id != NULL && i < self->responses->len; i++) {
    Response *response = g_ptr_array_index (self->responses, i);

    if (g_str_equal (response->id, id))
      return response;
  }
  return NULL;
}

/* The body and responses layout, made the window's content on first use. */
static void
ensure_layout (AwraDialog *self)
{
  if (self->layout != NULL)
    return;
  self->layout = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  self->actions = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  awra_box_set_spacing (GTK_BOX (self->actions), AWRA_SPACING_SM);
  awra_widget_set_margin (self->actions, AWRA_SPACING_LG);
  gtk_widget_set_halign (self->actions, GTK_ALIGN_END);
  gtk_widget_set_visible (self->actions, FALSE);
  gtk_widget_add_css_class (self->actions, "awra-dialog-responses");
  gtk_box_append (GTK_BOX (self->layout), self->actions);
  awra_window_set_content (AWRA_WINDOW (self), self->layout);
}

static void
update_default (AwraDialog *self)
{
  Response *response = find_response (self, self->default_response);

  gtk_window_set_default_widget (GTK_WINDOW (self),
                                 response != NULL ? response->button : NULL);
}

static void
response_clicked_cb (GtkWidget  *button,
                     AwraDialog *self)
{
  for (guint i = 0; i < self->responses->len; i++) {
    Response *response = g_ptr_array_index (self->responses, i);

    if (response->button == button) {
      g_autofree char *id = g_strdup (response->id);

      awra_dialog_respond (self, id);
      return;
    }
  }
}

static gboolean
escape_cb (GtkWidget *widget,
           GVariant  *args,
           gpointer   user_data)
{
  AwraDialog *self = AWRA_DIALOG (widget);
  GtkWidget *focus = gtk_root_get_focus (GTK_ROOT (self));

  (void) args;
  (void) user_data;
  if (focus != NULL && gtk_widget_get_ancestor (focus, GTK_TYPE_POPOVER) != NULL)
    return FALSE;
  if (self->close_response != NULL) {
    g_autofree char *id = g_strdup (self->close_response);

    awra_dialog_respond (self, id);
  } else {
    gtk_window_close (GTK_WINDOW (self));
  }
  return TRUE;
}

static gboolean
dialog_key_pressed (GtkEventControllerKey *controller,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        state,
                    AwraDialog            *self)
{
  (void) controller;
  (void) keycode;
  (void) state;
  if (keyval != GDK_KEY_Escape)
    return FALSE;
  return escape_cb (GTK_WIDGET (self), NULL, NULL);
}

/* Closing it another way (the window's close button) answers the close
 * response too. */
static gboolean
awra_dialog_close_request (GtkWindow *window)
{
  AwraDialog *self = AWRA_DIALOG (window);

  if (!self->responding && self->close_response != NULL) {
    g_autofree char *id = g_strdup (self->close_response);

    self->responding = TRUE;
    g_signal_emit (self, signals[SIGNAL_RESPONSE], g_quark_from_string (id), id);
    self->responding = FALSE;
  }
  return GTK_WINDOW_CLASS (awra_dialog_parent_class)->close_request != NULL &&
         GTK_WINDOW_CLASS (awra_dialog_parent_class)->close_request (window);
}

static void
awra_dialog_dispose (GObject *object)
{
  AwraDialog *self = AWRA_DIALOG (object);

  g_clear_pointer (&self->responses, g_ptr_array_unref);
  self->layout = self->body = self->actions = NULL;
  G_OBJECT_CLASS (awra_dialog_parent_class)->dispose (object);
}

static void
awra_dialog_finalize (GObject *object)
{
  AwraDialog *self = AWRA_DIALOG (object);

  g_free (self->default_response);
  g_free (self->close_response);
  G_OBJECT_CLASS (awra_dialog_parent_class)->finalize (object);
}

static void
awra_dialog_get_property (GObject    *object,
                          guint       id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  AwraDialog *self = AWRA_DIALOG (object);

  switch (id) {
  case PROP_BODY:
    g_value_set_object (value, self->body);
    break;
  case PROP_DEFAULT_RESPONSE:
    g_value_set_string (value, self->default_response);
    break;
  case PROP_CLOSE_RESPONSE:
    g_value_set_string (value, self->close_response);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_dialog_set_property (GObject      *object,
                          guint         id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  AwraDialog *self = AWRA_DIALOG (object);

  switch (id) {
  case PROP_BODY:
    awra_dialog_set_body (self, g_value_get_object (value));
    break;
  case PROP_DEFAULT_RESPONSE:
    awra_dialog_set_default_response (self, g_value_get_string (value));
    break;
  case PROP_CLOSE_RESPONSE:
    awra_dialog_set_close_response (self, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_dialog_class_init (AwraDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->dispose = awra_dialog_dispose;
  object_class->finalize = awra_dialog_finalize;
  object_class->get_property = awra_dialog_get_property;
  object_class->set_property = awra_dialog_set_property;
  window_class->close_request = awra_dialog_close_request;

  /**
   * AwraDialog:body:
   *
   * The dialog's own content, shown above its responses.
   *
   * Since: 1.1
   */
  properties[PROP_BODY] = g_param_spec_object (
    "body", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraDialog:default-response:
   *
   * The response Enter activates.
   *
   * Since: 1.1
   */
  properties[PROP_DEFAULT_RESPONSE] = g_param_spec_string (
    "default-response", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraDialog:close-response:
   *
   * The response given when the dialog is closed with Escape or its close
   * button.
   *
   * Since: 1.1
   */
  properties[PROP_CLOSE_RESPONSE] = g_param_spec_string (
    "close-response", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraDialog::response:
   * @self: the dialog
   * @response: the response identifier
   *
   * Emitted when a response is chosen, with the response as detail, just
   * before the dialog closes.
   *
   * Since: 1.1
   */
  signals[SIGNAL_RESPONSE] = g_signal_new (
    "response", G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, 0, NULL, NULL, NULL,
    G_TYPE_NONE, 1, G_TYPE_STRING);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Escape, 0, escape_cb, NULL);
}

static void
awra_dialog_init (AwraDialog *self)
{
  AwraSurface *surface = awra_window_get_root_surface (AWRA_WINDOW (self));
  g_autoptr (AwraMaterial) material = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_FLOATING);

  self->responses = g_ptr_array_new_with_free_func (response_free);
  awra_surface_set_role (surface, AWRA_SURFACE_ROLE_DIALOG);
  awra_surface_set_material (surface, material);
  awra_surface_set_elevation_level (surface, AWRA_ELEVATION_MODAL);
  gtk_window_set_modal (GTK_WINDOW (self), TRUE);
  /* Search entries consume Escape before class bindings. Capture it for the
   * dialog while leaving focused popovers their own dismissal sequence. */
  GtkEventController *keys = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
  g_signal_connect (keys, "key-pressed", G_CALLBACK (dialog_key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), keys);
  /* Dialog width benefits from a calm default measure, but height must remain
   * content-driven.  A fixed default height leaves short forms with a large
   * dead band below their actions and forces every application to compensate. */
  gtk_window_set_default_size (GTK_WINDOW (self), 440, -1);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-dialog");
}

GtkWidget *
awra_dialog_new (GtkWindow *parent)
{
  AwraDialog *self;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);
  self = g_object_new (AWRA_TYPE_DIALOG, NULL);
  if (parent != NULL) {
    gtk_window_set_transient_for (GTK_WINDOW (self), parent);
    gtk_window_set_application (GTK_WINDOW (self), gtk_window_get_application (parent));
  }
  /* GtkWindow owns its initial reference until destruction; transfer-full
   * constructors give the language binding its own reference. */
  return GTK_WIDGET (g_object_ref (self));
}

GtkWidget *
awra_dialog_get_body (AwraDialog *self)
{
  g_return_val_if_fail (AWRA_IS_DIALOG (self), NULL);
  return self->body;
}

void
awra_dialog_set_body (AwraDialog *self,
                      GtkWidget  *body)
{
  g_return_if_fail (AWRA_IS_DIALOG (self));
  g_return_if_fail (body == NULL || (GTK_IS_WIDGET (body) && gtk_widget_get_parent (body) == NULL));
  if (self->body == body)
    return;
  ensure_layout (self);
  if (self->body != NULL)
    gtk_box_remove (GTK_BOX (self->layout), self->body);
  self->body = body;
  if (body != NULL) {
    /* A body that sets no inset of its own lines up with the title above
     * it and the responses below it, rather than touching the window's
     * edges. */
    if (gtk_widget_get_margin_start (body) == 0 && gtk_widget_get_margin_end (body) == 0 &&
        gtk_widget_get_margin_top (body) == 0 && gtk_widget_get_margin_bottom (body) == 0) {
      awra_widget_set_horizontal_margin (body, AWRA_SPACING_LG);
      gtk_widget_set_margin_top (body, awra_spacing_resolve (body, AWRA_SPACING_XS));
    }
    gtk_widget_set_vexpand (body, TRUE);
    gtk_box_prepend (GTK_BOX (self->layout), body);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BODY]);
}

void
awra_dialog_add_response (AwraDialog *self,
                          const char *id,
                          const char *label)
{
  Response *response;

  g_return_if_fail (AWRA_IS_DIALOG (self));
  g_return_if_fail (id != NULL && *id != '\0');
  g_return_if_fail (label != NULL);
  g_return_if_fail (find_response (self, id) == NULL);
  ensure_layout (self);
  response = g_new0 (Response, 1);
  response->id = g_strdup (id);
  response->button = awra_button_new_with_label (label);
  awra_button_set_appearance (AWRA_BUTTON (response->button),
                              AWRA_BUTTON_APPEARANCE_SECONDARY);
  g_signal_connect (response->button, "clicked", G_CALLBACK (response_clicked_cb), self);
  gtk_box_append (GTK_BOX (self->actions), response->button);
  gtk_widget_set_visible (self->actions, TRUE);
  g_ptr_array_add (self->responses, response);
  update_default (self);
}

void
awra_dialog_set_response_appearance (AwraDialog          *self,
                                     const char          *id,
                                     AwraButtonAppearance appearance)
{
  Response *response;

  g_return_if_fail (AWRA_IS_DIALOG (self));
  response = find_response (self, id);
  g_return_if_fail (response != NULL);
  awra_button_set_appearance (AWRA_BUTTON (response->button), appearance);
}

void
awra_dialog_set_response_enabled (AwraDialog *self,
                                  const char *id,
                                  gboolean    enabled)
{
  Response *response;

  g_return_if_fail (AWRA_IS_DIALOG (self));
  response = find_response (self, id);
  g_return_if_fail (response != NULL);
  gtk_widget_set_sensitive (response->button, enabled);
}

const char *
awra_dialog_get_default_response (AwraDialog *self)
{
  g_return_val_if_fail (AWRA_IS_DIALOG (self), NULL);
  return self->default_response;
}

void
awra_dialog_set_default_response (AwraDialog *self,
                                  const char *id)
{
  g_return_if_fail (AWRA_IS_DIALOG (self));
  if (!g_set_str (&self->default_response, id))
    return;
  update_default (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_RESPONSE]);
}

const char *
awra_dialog_get_close_response (AwraDialog *self)
{
  g_return_val_if_fail (AWRA_IS_DIALOG (self), NULL);
  return self->close_response;
}

void
awra_dialog_set_close_response (AwraDialog *self,
                                const char *id)
{
  g_return_if_fail (AWRA_IS_DIALOG (self));
  if (!g_set_str (&self->close_response, id))
    return;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLOSE_RESPONSE]);
}

gboolean
awra_dialog_get_response_closes (AwraDialog *self,
                                 const char *id)
{
  Response *response;

  g_return_val_if_fail (AWRA_IS_DIALOG (self), TRUE);
  response = find_response (self, id);
  g_return_val_if_fail (response != NULL, TRUE);
  return !response->stays;
}

void
awra_dialog_set_response_closes (AwraDialog *self,
                                 const char *id,
                                 gboolean    closes)
{
  Response *response;

  g_return_if_fail (AWRA_IS_DIALOG (self));
  response = find_response (self, id);
  g_return_if_fail (response != NULL);
  response->stays = !closes;
}

void
awra_dialog_set_response_busy (AwraDialog *self,
                               const char *id,
                               gboolean    busy)
{
  Response *response;

  g_return_if_fail (AWRA_IS_DIALOG (self));
  response = find_response (self, id);
  g_return_if_fail (response != NULL);
  awra_button_set_busy (AWRA_BUTTON (response->button), busy);
}

void
awra_dialog_respond (AwraDialog *self,
                     const char *id)
{
  Response *response;
  gboolean closes;

  g_return_if_fail (AWRA_IS_DIALOG (self));
  g_return_if_fail (id != NULL);
  response = find_response (self, id);
  closes = response == NULL || !response->stays;
  g_object_ref (self);
  self->responding = TRUE;
  g_signal_emit (self, signals[SIGNAL_RESPONSE], g_quark_from_string (id), id);
  if (closes)
    gtk_window_close (GTK_WINDOW (self));
  self->responding = FALSE;
  g_object_unref (self);
}

void
awra_dialog_close (AwraDialog *self)
{
  g_return_if_fail (AWRA_IS_DIALOG (self));
  g_object_ref (self);
  self->responding = TRUE;
  gtk_window_close (GTK_WINDOW (self));
  self->responding = FALSE;
  g_object_unref (self);
}
