/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra-zoom-controller.h>
#include <math.h>

struct _AwraZoomController {
  GObject parent_instance;
  GtkWidget *target;
  GtkEventController *scroll;
  GtkCssProvider *provider;
  GdkDisplay *display;
  char *scope;
  double scale;
  double minimum;
  double maximum;
  gboolean scale_text;
};
enum { PROP_0, PROP_SCALE, PROP_MIN_SCALE, PROP_MAX_SCALE, PROP_SCALE_TEXT, N_PROPS };
static GParamSpec *properties[N_PROPS];
static gint next_scope;
G_DEFINE_FINAL_TYPE (AwraZoomController, awra_zoom_controller, G_TYPE_OBJECT)

static void
update_text_scale (AwraZoomController *self)
{
  char number[G_ASCII_DTOSTR_BUF_SIZE];
  g_autofree char *css = NULL;
  g_ascii_dtostr (number, sizeof number, self->scale);
  css = g_strdup_printf (".%s { font-size: %sem; }", self->scope, number);
  gtk_css_provider_load_from_string (self->provider, !self->scale_text || self->scale == 1.0 ? "" : css);
}

static gboolean
scroll_cb (GtkEventControllerScroll *controller, double dx, double dy, AwraZoomController *self)
{
  (void) dx;
  return awra_zoom_controller_handle_scroll (self, dy,
    gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (controller)));
}

static void
awra_zoom_controller_dispose (GObject *object)
{
  AwraZoomController *self = AWRA_ZOOM_CONTROLLER (object);
  if (self->target != NULL) {
    gtk_widget_remove_css_class (self->target, self->scope);
    if (gtk_event_controller_get_widget (self->scroll) == self->target)
      gtk_widget_remove_controller (self->target, self->scroll);
    g_object_remove_weak_pointer (G_OBJECT (self->target), (gpointer *) &self->target);
    self->target = NULL;
  }
  if (self->provider != NULL && self->display != NULL)
    gtk_style_context_remove_provider_for_display (self->display, GTK_STYLE_PROVIDER (self->provider));
  g_clear_object (&self->scroll);
  g_clear_object (&self->provider);
  g_clear_object (&self->display);
  G_OBJECT_CLASS (awra_zoom_controller_parent_class)->dispose (object);
}

static void
awra_zoom_controller_finalize (GObject *object)
{
  g_free (AWRA_ZOOM_CONTROLLER (object)->scope);
  G_OBJECT_CLASS (awra_zoom_controller_parent_class)->finalize (object);
}

static void
get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec)
{
  AwraZoomController *self = AWRA_ZOOM_CONTROLLER (object);
  switch (id) {
  case PROP_SCALE: g_value_set_double (value, self->scale); break;
  case PROP_MIN_SCALE: g_value_set_double (value, self->minimum); break;
  case PROP_MAX_SCALE: g_value_set_double (value, self->maximum); break;
  case PROP_SCALE_TEXT: g_value_set_boolean (value, self->scale_text); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec)
{
  if (id == PROP_SCALE)
    awra_zoom_controller_set_scale (AWRA_ZOOM_CONTROLLER (object), g_value_get_double (value));
  else if (id == PROP_SCALE_TEXT)
    awra_zoom_controller_set_scale_text (AWRA_ZOOM_CONTROLLER (object), g_value_get_boolean (value));
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
}

static void
awra_zoom_controller_class_init (AwraZoomControllerClass *klass)
{
  GObjectClass *oc = G_OBJECT_CLASS (klass);
  oc->dispose = awra_zoom_controller_dispose;
  oc->finalize = awra_zoom_controller_finalize;
  oc->get_property = get_property;
  oc->set_property = set_property;
  properties[PROP_SCALE] = g_param_spec_double ("scale", NULL, NULL, .05, 64, 1,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_MIN_SCALE] = g_param_spec_double ("min-scale", NULL, NULL, .05, 64, .5,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_MAX_SCALE] = g_param_spec_double ("max-scale", NULL, NULL, .05, 64, 3,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_SCALE_TEXT] = g_param_spec_boolean ("scale-text", NULL, NULL, TRUE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (oc, N_PROPS, properties);
}

static void
awra_zoom_controller_init (AwraZoomController *self)
{
  self->scale = 1;
  self->minimum = .5;
  self->maximum = 3;
  self->scale_text = TRUE;
  self->provider = gtk_css_provider_new ();
  self->scope = g_strdup_printf ("awra-zoom-target-%u", (guint) g_atomic_int_add (&next_scope, 1));
}

AwraZoomController *
awra_zoom_controller_new (GtkWidget *target)
{
  g_return_val_if_fail (GTK_IS_WIDGET (target), NULL);
  AwraZoomController *self = g_object_new (AWRA_TYPE_ZOOM_CONTROLLER, NULL);
  self->target = target;
  g_object_add_weak_pointer (G_OBJECT (target), (gpointer *) &self->target);
  self->display = g_object_ref (gtk_widget_get_display (target));
  gtk_widget_add_css_class (target, self->scope);
  gtk_style_context_add_provider_for_display (self->display, GTK_STYLE_PROVIDER (self->provider),
    GTK_STYLE_PROVIDER_PRIORITY_USER + 2);
  self->scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                                  GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
  g_object_ref (self->scroll);
  gtk_event_controller_set_propagation_phase (self->scroll, GTK_PHASE_CAPTURE);
  g_signal_connect_object (self->scroll, "scroll", G_CALLBACK (scroll_cb), self, 0);
  gtk_widget_add_controller (target, self->scroll);
  return self;
}

GtkWidget *awra_zoom_controller_get_target (AwraZoomController *self)
{
  g_return_val_if_fail (AWRA_IS_ZOOM_CONTROLLER (self), NULL);
  return self->target;
}

double awra_zoom_controller_get_scale (AwraZoomController *self)
{
  g_return_val_if_fail (AWRA_IS_ZOOM_CONTROLLER (self), 1);
  return self->scale;
}

gboolean awra_zoom_controller_get_scale_text (AwraZoomController *self)
{
  g_return_val_if_fail (AWRA_IS_ZOOM_CONTROLLER (self), TRUE);
  return self->scale_text;
}

void awra_zoom_controller_set_scale_text (AwraZoomController *self, gboolean scale_text)
{
  g_return_if_fail (AWRA_IS_ZOOM_CONTROLLER (self));
  scale_text = !!scale_text;
  if (self->scale_text == scale_text) return;
  self->scale_text = scale_text;
  if (self->provider != NULL) update_text_scale (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCALE_TEXT]);
}

void awra_zoom_controller_set_scale (AwraZoomController *self, double scale)
{
  g_return_if_fail (AWRA_IS_ZOOM_CONTROLLER (self));
  if (!isfinite (scale)) return;
  scale = CLAMP (scale, self->minimum, self->maximum);
  if (fabs (scale - self->scale) < .000001) return;
  self->scale = scale;
  if (self->provider != NULL) update_text_scale (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCALE]);
}

void awra_zoom_controller_set_range (AwraZoomController *self, double minimum, double maximum)
{
  g_return_if_fail (AWRA_IS_ZOOM_CONTROLLER (self));
  g_return_if_fail (isfinite (minimum) && isfinite (maximum) && minimum >= .05 && maximum <= 64 && maximum >= minimum);
  g_object_freeze_notify (G_OBJECT (self));
  if (self->minimum != minimum) {
    self->minimum = minimum;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_SCALE]);
  }
  if (self->maximum != maximum) {
    self->maximum = maximum;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_SCALE]);
  }
  awra_zoom_controller_set_scale (self, self->scale);
  g_object_thaw_notify (G_OBJECT (self));
}
void awra_zoom_controller_zoom_in (AwraZoomController *self) { awra_zoom_controller_set_scale (self, awra_zoom_controller_get_scale (self) * 1.1); }
void awra_zoom_controller_zoom_out (AwraZoomController *self) { awra_zoom_controller_set_scale (self, awra_zoom_controller_get_scale (self) / 1.1); }
void awra_zoom_controller_reset (AwraZoomController *self) { awra_zoom_controller_set_scale (self, 1); }

gboolean awra_zoom_controller_handle_scroll (AwraZoomController *self, double delta, GdkModifierType modifiers)
{
  g_return_val_if_fail (AWRA_IS_ZOOM_CONTROLLER (self), FALSE);
  if (self->target == NULL || !(modifiers & GDK_CONTROL_MASK) || !isfinite (delta) || delta == 0)
    return FALSE;
  awra_zoom_controller_set_scale (self, self->scale * pow (1.1, -CLAMP (delta, -8, 8)));
  return TRUE;
}
