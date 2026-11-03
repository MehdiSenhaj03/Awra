/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "awra-config.h"

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#ifdef AWRA_HAVE_WIN32
#include <gdk/win32/gdkwin32.h>
#include "platform/win32/awra-win32-private.h"
#endif
#include <awra/awra-style-manager.h>

#include "core/awra-style-manager-private.h"
#include "tokens/awra-token-set-private.h"

struct _AwraStyleManager {
  GObject parent_instance;
  GdkDisplay *display;
  GtkSettings *settings;
  AwraAppearance appearance;
  AwraAccentSource accent_source;
  GdkRGBA accent;
  GdkRGBA custom_accent;
  GdkRGBA system_accent;
  gboolean system_accent_available;
  GDBusConnection *session_bus;
  guint portal_subscription;
  guint win32_watch;
  gboolean high_contrast;
  gboolean reduced_motion;
  gboolean reduced_transparency;
  gboolean high_contrast_explicit;
  gboolean reduced_motion_explicit;
  gboolean reduced_transparency_explicit;
  gboolean canvas_tint_overridden;
  GdkRGBA canvas_tint;
  double grain_scale;
  gboolean native_blur_enabled;
  AwraMaterialProfileOverride material_overrides[AWRA_MATERIAL_PROFILE_COUNT];
  AwraTokenSet *tokens;
};

enum {
  PROP_0,
  PROP_APPEARANCE,
  PROP_ACCENT,
  PROP_ACCENT_SOURCE,
  PROP_SYSTEM_ACCENT_AVAILABLE,
  PROP_HIGH_CONTRAST,
  PROP_REDUCED_MOTION,
  PROP_REDUCED_TRANSPARENCY,
  PROP_CANVAS_TINT,
  PROP_GRAIN_SCALE,
  PROP_NATIVE_BLUR_ENABLED,
  PROP_TOKEN_SET,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraStyleManager, awra_style_manager, G_TYPE_OBJECT)

static const GdkRGBA default_accent = { 0.49, 0.31, 0.95, 1.0 };

static gboolean
variant_to_accent (GVariant *value,
                   GdkRGBA  *accent)
{
  g_autoptr (GVariant) inner = NULL;
  double channels[4] = { 0.0, 0.0, 0.0, 1.0 };
  gsize count;

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_VARIANT)) {
    inner = g_variant_get_variant (value);
    return variant_to_accent (inner, accent);
  }
  if (!g_variant_is_container (value))
    return FALSE;
  count = g_variant_n_children (value);
  if (count != 3 && count != 4)
    return FALSE;
  for (gsize i = 0; i < count; i++) {
    g_autoptr (GVariant) child = g_variant_get_child_value (value, i);

    if (!g_variant_is_of_type (child, G_VARIANT_TYPE_DOUBLE))
      return FALSE;
    channels[i] = g_variant_get_double (child);
    if (!isfinite (channels[i]) || channels[i] < 0.0 || channels[i] > 1.0)
      return FALSE;
  }
  *accent = (GdkRGBA) {
    channels[0], channels[1], channels[2], channels[3]
  };
  return TRUE;
}

static gboolean
read_portal_accent (AwraStyleManager *self,
                    GdkRGBA          *accent)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GVariant) value = NULL;

  if (self->session_bus == NULL)
    return FALSE;
  reply = g_dbus_connection_call_sync (
    self->session_bus,
    "org.freedesktop.portal.Desktop",
    "/org/freedesktop/portal/desktop",
    "org.freedesktop.portal.Settings",
    "Read",
    g_variant_new ("(ss)", "org.freedesktop.appearance", "accent-color"),
    G_VARIANT_TYPE ("(v)"),
    G_DBUS_CALL_FLAGS_NONE,
    500,
    NULL,
    NULL);
  if (reply == NULL)
    return FALSE;
  value = g_variant_get_child_value (reply, 0);
  return variant_to_accent (value, accent);
}

static gboolean
read_settings_accent (AwraStyleManager *self,
                      GdkRGBA          *accent)
{
  static const char *names[] = {
    "gtk-interface-accent-color",
    "gtk-accent-color",
  };

  if (self->settings == NULL)
    return FALSE;
  for (guint i = 0; i < G_N_ELEMENTS (names); i++) {
    GParamSpec *pspec = g_object_class_find_property (
      G_OBJECT_GET_CLASS (self->settings), names[i]);

    if (pspec == NULL)
      continue;
    if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA) {
      GdkRGBA *value = NULL;

      g_object_get (self->settings, names[i], &value, NULL);
      if (value != NULL) {
        *accent = *value;
        gdk_rgba_free (value);
        return TRUE;
      }
    } else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_STRING) {
      g_autofree char *value = NULL;

      g_object_get (self->settings, names[i], &value, NULL);
      if (value != NULL && gdk_rgba_parse (accent, value))
        return TRUE;
    }
  }
  return FALSE;
}

static gboolean
resolve_system_accent (AwraStyleManager *self,
                       GdkRGBA          *accent)
{
#ifdef AWRA_HAVE_WIN32
  if (self->win32_watch != 0)
    return awra_win32_read_accent (accent);
#endif
  return read_settings_accent (self, accent) ||
         read_portal_accent (self, accent);
}

static void
resolve_accent (AwraStyleManager *self)
{
  GdkRGBA resolved = default_accent;
  gboolean available = FALSE;

  if (self->accent_source == AWRA_ACCENT_SOURCE_CUSTOM)
    resolved = self->custom_accent;
  else if (self->accent_source == AWRA_ACCENT_SOURCE_SYSTEM) {
    available = resolve_system_accent (self, &resolved);
    if (available)
      self->system_accent = resolved;
  } else {
    available = self->system_accent_available;
  }
  self->accent = resolved;
  if (self->system_accent_available != available) {
    self->system_accent_available = available;
    g_object_notify_by_pspec (G_OBJECT (self),
                              properties[PROP_SYSTEM_ACCENT_AVAILABLE]);
  }
}

static gboolean
system_is_dark (AwraStyleManager *self)
{
  GtkInterfaceColorScheme scheme = GTK_INTERFACE_COLOR_SCHEME_DEFAULT;

  if (self->settings != NULL)
    g_object_get (self->settings, "gtk-interface-color-scheme", &scheme, NULL);

  return scheme == GTK_INTERFACE_COLOR_SCHEME_DARK;
}

static void
rebuild_tokens (AwraStyleManager *self)
{
  gboolean dark = self->appearance == AWRA_APPEARANCE_DARK ||
                  (self->appearance == AWRA_APPEARANCE_SYSTEM && system_is_dark (self));
  GdkRGBA previous_canvas = { 0 };
  gboolean had_tokens = self->tokens != NULL;

  if (had_tokens)
    previous_canvas = *awra_token_set_get_surface_canvas (self->tokens);

  g_clear_object (&self->tokens);
  self->tokens = awra_token_set_new_with_tuning (
    dark,
    &self->accent,
    self->high_contrast,
    self->canvas_tint_overridden ? &self->canvas_tint : NULL,
    self->grain_scale);
  for (guint i = 0; i < AWRA_MATERIAL_PROFILE_COUNT; i++)
    awra_token_set_apply_material_profile_override (
      self->tokens,
      (AwraMaterialProfileId) i,
      &self->material_overrides[i]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOKEN_SET]);
  if (!had_tokens ||
      !gdk_rgba_equal (&previous_canvas,
                       awra_token_set_get_surface_canvas (self->tokens)))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CANVAS_TINT]);
}

static void
settings_changed_cb (GtkSettings      *settings,
                     GParamSpec       *pspec,
                     AwraStyleManager *self)
{
  if (g_str_equal (pspec->name, "gtk-interface-color-scheme") &&
      self->appearance == AWRA_APPEARANCE_SYSTEM) {
    rebuild_tokens (self);
  } else if (g_str_equal (pspec->name, "gtk-interface-contrast") &&
             !self->high_contrast_explicit) {
    GtkInterfaceContrast contrast = GTK_INTERFACE_CONTRAST_NO_PREFERENCE;

    g_object_get (settings, "gtk-interface-contrast", &contrast, NULL);
    if (self->high_contrast != (contrast == GTK_INTERFACE_CONTRAST_MORE)) {
      self->high_contrast = contrast == GTK_INTERFACE_CONTRAST_MORE;
      rebuild_tokens (self);
      g_object_notify_by_pspec (G_OBJECT (self),
                                properties[PROP_HIGH_CONTRAST]);
    }
  } else if (g_str_equal (pspec->name, "gtk-interface-reduced-motion") &&
             !self->reduced_motion_explicit) {
    GtkReducedMotion motion = GTK_REDUCED_MOTION_NO_PREFERENCE;

    g_object_get (settings, "gtk-interface-reduced-motion", &motion, NULL);
    if (self->reduced_motion != (motion == GTK_REDUCED_MOTION_REDUCE)) {
      self->reduced_motion = motion == GTK_REDUCED_MOTION_REDUCE;
      g_object_notify_by_pspec (G_OBJECT (self),
                                properties[PROP_REDUCED_MOTION]);
    }
  }
}

static void system_accent_changed (const GdkRGBA *changed,
                                   gpointer       user_data);

static void
portal_setting_changed_cb (GDBusConnection *connection,
                           const char      *sender_name,
                           const char      *object_path,
                           const char      *interface_name,
                           const char      *signal_name,
                           GVariant        *parameters,
                           gpointer         user_data)
{
  AwraStyleManager *self = user_data;
  const char *namespace;
  const char *key;
  g_autoptr (GVariant) value = NULL;
  GdkRGBA accent;

  (void) connection;
  (void) sender_name;
  (void) object_path;
  (void) interface_name;
  (void) signal_name;
  g_variant_get (parameters, "(&s&sv)", &namespace, &key, &value);
  if (!g_str_equal (namespace, "org.freedesktop.appearance") ||
      !g_str_equal (key, "accent-color") ||
      !variant_to_accent (value, &accent))
    return;

  system_accent_changed (&accent, self);
}

static void
system_accent_changed (const GdkRGBA *changed,
                       gpointer       user_data)
{
  AwraStyleManager *self = user_data;
  GdkRGBA accent = *changed;

  self->system_accent = accent;
  if (!self->system_accent_available) {
    self->system_accent_available = TRUE;
    g_object_notify_by_pspec (G_OBJECT (self),
                              properties[PROP_SYSTEM_ACCENT_AVAILABLE]);
  }
  if (self->accent_source == AWRA_ACCENT_SOURCE_SYSTEM &&
      !gdk_rgba_equal (&self->accent, &accent)) {
    self->accent = accent;
    rebuild_tokens (self);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCENT]);
  }
}

/* Placed between two rows of pixels, small text loses the dot of an i and the
 * top of its round figures, more or less depending on where the window is.
 * While a display at scale 1 shows it, glyphs follow the pixel grid and the
 * desktop's own hinting; only a fully dense setup keeps GTK's free placement. */
static void
update_font_rendering (AwraStyleManager *self)
{
  GListModel *monitors = gdk_display_get_monitors (self->display);
  gboolean dense = g_list_model_get_n_items (monitors) > 0;

  for (guint i = 0; dense && i < g_list_model_get_n_items (monitors); i++) {
    g_autoptr (GdkMonitor) monitor = g_list_model_get_item (monitors, i);

    dense = gdk_monitor_get_scale (monitor) > 1.0;
  }
  g_object_set (self->settings, "gtk-font-rendering",
                dense ? GTK_FONT_RENDERING_AUTOMATIC : GTK_FONT_RENDERING_MANUAL,
                NULL);
}

static void
watch_monitors (AwraStyleManager *self,
                GListModel       *monitors,
                guint             position,
                guint             added)
{
  for (guint i = position; i < position + added; i++) {
    g_autoptr (GdkMonitor) monitor = g_list_model_get_item (monitors, i);

    g_signal_connect_object (monitor, "notify::scale",
                             G_CALLBACK (update_font_rendering), self,
                             G_CONNECT_SWAPPED);
  }
}

static void
monitors_changed_cb (GListModel       *monitors,
                     guint             position,
                     guint             removed,
                     guint             added,
                     AwraStyleManager *self)
{
  (void) removed;
  watch_monitors (self, monitors, position, added);
  update_font_rendering (self);
}

static void
awra_style_manager_dispose (GObject *object)
{
  AwraStyleManager *self = AWRA_STYLE_MANAGER (object);

  if (self->settings != NULL)
    g_signal_handlers_disconnect_by_data (self->settings, self);
  if (self->portal_subscription != 0 && self->session_bus != NULL) {
    g_dbus_connection_signal_unsubscribe (self->session_bus,
                                          self->portal_subscription);
    self->portal_subscription = 0;
  }
  if (self->win32_watch != 0) {
    g_source_remove (self->win32_watch);
    self->win32_watch = 0;
  }
  g_clear_object (&self->settings);
  g_clear_object (&self->display);
  g_clear_object (&self->session_bus);
  g_clear_object (&self->tokens);

  G_OBJECT_CLASS (awra_style_manager_parent_class)->dispose (object);
}

static void
awra_style_manager_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  AwraStyleManager *self = AWRA_STYLE_MANAGER (object);

  switch (property_id) {
  case PROP_APPEARANCE:
    g_value_set_enum (value, self->appearance);
    break;
  case PROP_ACCENT:
    g_value_set_boxed (value, &self->accent);
    break;
  case PROP_ACCENT_SOURCE:
    g_value_set_enum (value, self->accent_source);
    break;
  case PROP_SYSTEM_ACCENT_AVAILABLE:
    g_value_set_boolean (value, self->system_accent_available);
    break;
  case PROP_HIGH_CONTRAST:
    g_value_set_boolean (value, self->high_contrast);
    break;
  case PROP_REDUCED_MOTION:
    g_value_set_boolean (value, self->reduced_motion);
    break;
  case PROP_REDUCED_TRANSPARENCY:
    g_value_set_boolean (value, self->reduced_transparency);
    break;
  case PROP_CANVAS_TINT:
    g_value_set_boxed (value, awra_style_manager_get_canvas_tint (self));
    break;
  case PROP_GRAIN_SCALE:
    g_value_set_double (value, self->grain_scale);
    break;
  case PROP_NATIVE_BLUR_ENABLED:
    g_value_set_boolean (value, self->native_blur_enabled);
    break;
  case PROP_TOKEN_SET:
    g_value_set_object (value, self->tokens);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_style_manager_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  AwraStyleManager *self = AWRA_STYLE_MANAGER (object);

  switch (property_id) {
  case PROP_APPEARANCE:
    awra_style_manager_set_appearance (self, g_value_get_enum (value));
    break;
  case PROP_ACCENT:
    awra_style_manager_set_accent (self, g_value_get_boxed (value));
    break;
  case PROP_ACCENT_SOURCE:
    awra_style_manager_set_accent_source (self, g_value_get_enum (value));
    break;
  case PROP_HIGH_CONTRAST:
    awra_style_manager_set_high_contrast (self, g_value_get_boolean (value));
    break;
  case PROP_REDUCED_MOTION:
    awra_style_manager_set_reduced_motion (self, g_value_get_boolean (value));
    break;
  case PROP_REDUCED_TRANSPARENCY:
    awra_style_manager_set_reduced_transparency (self, g_value_get_boolean (value));
    break;
  case PROP_CANVAS_TINT:
    awra_style_manager_set_canvas_tint (self, g_value_get_boxed (value));
    break;
  case PROP_GRAIN_SCALE:
    awra_style_manager_set_grain_scale (self, g_value_get_double (value));
    break;
  case PROP_NATIVE_BLUR_ENABLED:
    awra_style_manager_set_native_blur_enabled (self,
                                                g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_style_manager_class_init (AwraStyleManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = awra_style_manager_dispose;
  object_class->get_property = awra_style_manager_get_property;
  object_class->set_property = awra_style_manager_set_property;

  properties[PROP_APPEARANCE] =
    g_param_spec_enum ("appearance", NULL, NULL, AWRA_TYPE_APPEARANCE,
                       AWRA_APPEARANCE_SYSTEM,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACCENT] =
    g_param_spec_boxed ("accent", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACCENT_SOURCE] =
    g_param_spec_enum ("accent-source", NULL, NULL, AWRA_TYPE_ACCENT_SOURCE,
                       AWRA_ACCENT_SOURCE_DEFAULT,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);
  properties[PROP_SYSTEM_ACCENT_AVAILABLE] =
    g_param_spec_boolean ("system-accent-available", NULL, NULL, FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_HIGH_CONTRAST] =
    g_param_spec_boolean ("high-contrast", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_REDUCED_MOTION] =
    g_param_spec_boolean ("reduced-motion", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_REDUCED_TRANSPARENCY] =
    g_param_spec_boolean ("reduced-transparency", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CANVAS_TINT] =
    g_param_spec_boxed ("canvas-tint", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS);
  properties[PROP_GRAIN_SCALE] =
    g_param_spec_double ("grain-scale", NULL, NULL, 0.0, 4.0,
                         AWRA_DEFAULT_GRAIN_SCALE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);
  properties[PROP_NATIVE_BLUR_ENABLED] =
    g_param_spec_boolean ("native-blur-enabled", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);
  properties[PROP_TOKEN_SET] =
    g_param_spec_object ("token-set", NULL, NULL, AWRA_TYPE_TOKEN_SET,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_style_manager_init (AwraStyleManager *self)
{
  self->appearance = AWRA_APPEARANCE_SYSTEM;
  self->accent_source = AWRA_ACCENT_SOURCE_DEFAULT;
  self->accent = default_accent;
  self->custom_accent = default_accent;
  self->system_accent = default_accent;
  self->grain_scale = AWRA_DEFAULT_GRAIN_SCALE;
  self->native_blur_enabled = TRUE;
}

AwraStyleManager *
awra_style_manager_new_for_display (GdkDisplay *display)
{
  AwraStyleManager *self;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  self = g_object_new (AWRA_TYPE_STYLE_MANAGER, NULL);
  self->display = g_object_ref (display);
  self->settings = g_object_ref (gtk_settings_get_for_display (display));
#ifdef AWRA_HAVE_WIN32
  /* Windows has no portal or session bus: read its preferences directly. */
  if (GDK_IS_WIN32_DISPLAY (display))
    self->win32_watch = awra_win32_watch_system (self->settings,
                                                 system_accent_changed, self);
  else
#endif
  self->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (self->session_bus != NULL) {
    self->portal_subscription = g_dbus_connection_signal_subscribe (
      self->session_bus,
      "org.freedesktop.portal.Desktop",
      "org.freedesktop.portal.Settings",
      "SettingChanged",
      "/org/freedesktop/portal/desktop",
      NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,
      portal_setting_changed_cb,
      self,
      NULL);
  }
  {
    GtkInterfaceContrast contrast = GTK_INTERFACE_CONTRAST_NO_PREFERENCE;
    GtkReducedMotion motion = GTK_REDUCED_MOTION_NO_PREFERENCE;

    g_object_get (self->settings,
                  "gtk-interface-contrast", &contrast,
                  "gtk-interface-reduced-motion", &motion,
                  NULL);
    self->high_contrast = contrast == GTK_INTERFACE_CONTRAST_MORE;
    self->reduced_motion = motion == GTK_REDUCED_MOTION_REDUCE;
  }
  g_signal_connect (self->settings,
                    "notify::gtk-interface-color-scheme",
                    G_CALLBACK (settings_changed_cb),
                    self);
  g_signal_connect (self->settings,
                    "notify::gtk-interface-contrast",
                    G_CALLBACK (settings_changed_cb),
                    self);
  g_signal_connect (self->settings,
                    "notify::gtk-interface-reduced-motion",
                    G_CALLBACK (settings_changed_cb),
                    self);
  {
    GListModel *monitors = gdk_display_get_monitors (display);

    g_signal_connect_object (monitors, "items-changed",
                             G_CALLBACK (monitors_changed_cb), self, 0);
    watch_monitors (self, monitors, 0, g_list_model_get_n_items (monitors));
    update_font_rendering (self);
  }
  resolve_accent (self);
  rebuild_tokens (self);

  return self;
}

AwraAppearance
awra_style_manager_get_appearance (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), AWRA_APPEARANCE_SYSTEM);
  return self->appearance;
}

void
awra_style_manager_set_appearance (AwraStyleManager *self,
                                   AwraAppearance    appearance)
{
  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (appearance >= AWRA_APPEARANCE_SYSTEM &&
                    appearance <= AWRA_APPEARANCE_DARK);

  if (self->appearance == appearance)
    return;

  self->appearance = appearance;
  rebuild_tokens (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPEARANCE]);
}

const GdkRGBA *
awra_style_manager_get_accent (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), NULL);
  return &self->accent;
}

void
awra_style_manager_set_accent (AwraStyleManager *self,
                               const GdkRGBA    *accent)
{
  gboolean color_changed;

  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (accent != NULL);

  color_changed = !gdk_rgba_equal (&self->accent, accent);
  if (!color_changed && self->accent_source == AWRA_ACCENT_SOURCE_CUSTOM)
    return;

  self->custom_accent = *accent;
  self->accent = *accent;
  if (self->accent_source != AWRA_ACCENT_SOURCE_CUSTOM) {
    self->accent_source = AWRA_ACCENT_SOURCE_CUSTOM;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCENT_SOURCE]);
  }
  rebuild_tokens (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCENT]);
}

AwraAccentSource
awra_style_manager_get_accent_source (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self),
                        AWRA_ACCENT_SOURCE_DEFAULT);
  return self->accent_source;
}

void
awra_style_manager_set_accent_source (AwraStyleManager *self,
                                      AwraAccentSource  source)
{
  GdkRGBA previous;

  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (source >= AWRA_ACCENT_SOURCE_SYSTEM &&
                    source <= AWRA_ACCENT_SOURCE_CUSTOM);
  if (self->accent_source == source)
    return;

  previous = self->accent;
  self->accent_source = source;
  resolve_accent (self);
  rebuild_tokens (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCENT_SOURCE]);
  if (!gdk_rgba_equal (&previous, &self->accent))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCENT]);
}

gboolean
awra_style_manager_get_system_accent_available (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), FALSE);
  return self->system_accent_available;
}

#define DEFINE_BOOLEAN_ACCESSORS(name, field, explicit_field, prop, rebuild) \
  gboolean \
  awra_style_manager_get_##name (AwraStyleManager *self) \
  { \
    g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), FALSE); \
    return self->field; \
  } \
  void \
  awra_style_manager_set_##name (AwraStyleManager *self, gboolean value) \
  { \
    g_return_if_fail (AWRA_IS_STYLE_MANAGER (self)); \
    value = !!value; \
    self->explicit_field = TRUE; \
    if (self->field == value) \
      return; \
    self->field = value; \
    if (rebuild) \
      rebuild_tokens (self); \
    g_object_notify_by_pspec (G_OBJECT (self), properties[prop]); \
  }

DEFINE_BOOLEAN_ACCESSORS (high_contrast, high_contrast, high_contrast_explicit,
                          PROP_HIGH_CONTRAST, TRUE)
DEFINE_BOOLEAN_ACCESSORS (reduced_motion, reduced_motion, reduced_motion_explicit,
                          PROP_REDUCED_MOTION, FALSE)
DEFINE_BOOLEAN_ACCESSORS (reduced_transparency, reduced_transparency,
                          reduced_transparency_explicit,
                          PROP_REDUCED_TRANSPARENCY, FALSE)

const GdkRGBA *
awra_style_manager_get_canvas_tint (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), NULL);
  g_return_val_if_fail (AWRA_IS_TOKEN_SET (self->tokens), NULL);
  return awra_token_set_get_surface_canvas (self->tokens);
}

void
awra_style_manager_set_canvas_tint (AwraStyleManager *self,
                                    const GdkRGBA    *tint)
{
  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (tint != NULL);
  g_return_if_fail (isfinite (tint->red) && tint->red >= 0.0 && tint->red <= 1.0);
  g_return_if_fail (isfinite (tint->green) && tint->green >= 0.0 && tint->green <= 1.0);
  g_return_if_fail (isfinite (tint->blue) && tint->blue >= 0.0 && tint->blue <= 1.0);
  g_return_if_fail (isfinite (tint->alpha) && tint->alpha >= 0.0 && tint->alpha <= 1.0);

  if (self->canvas_tint_overridden && gdk_rgba_equal (&self->canvas_tint, tint))
    return;
  self->canvas_tint = *tint;
  self->canvas_tint_overridden = TRUE;
  rebuild_tokens (self);
}

double
awra_style_manager_get_grain_scale (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self),
                        AWRA_DEFAULT_GRAIN_SCALE);
  return self->grain_scale;
}

void
awra_style_manager_set_grain_scale (AwraStyleManager *self,
                                    double            scale)
{
  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (isfinite (scale) && scale >= 0.0 && scale <= 4.0);

  if (self->grain_scale == scale)
    return;
  self->grain_scale = scale;
  rebuild_tokens (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GRAIN_SCALE]);
}

gboolean
awra_style_manager_get_native_blur_enabled (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), TRUE);
  return self->native_blur_enabled;
}

void
awra_style_manager_set_native_blur_enabled (AwraStyleManager *self,
                                            gboolean          enabled)
{
  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  enabled = !!enabled;
  if (self->native_blur_enabled == enabled)
    return;
  self->native_blur_enabled = enabled;
  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_NATIVE_BLUR_ENABLED]);
}

void
awra_style_manager_reset_material_tuning (AwraStyleManager *self)
{
  gboolean rebuild;
  gboolean grain_changed;
  gboolean blur_changed;

  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  rebuild = self->canvas_tint_overridden ||
            self->grain_scale != AWRA_DEFAULT_GRAIN_SCALE;
  for (guint i = 0; i < AWRA_MATERIAL_PROFILE_COUNT; i++) {
    const AwraMaterialProfileOverride *override =
      &self->material_overrides[i];

    rebuild = rebuild || override->tint_set || override->radius_set ||
              override->grain_scale_set ||
              override->outline_strength_set ||
              override->highlight_strength_set ||
              override->blur_enabled_set;
  }
  grain_changed = self->grain_scale != AWRA_DEFAULT_GRAIN_SCALE;
  blur_changed = !self->native_blur_enabled;
  self->canvas_tint_overridden = FALSE;
  memset (self->material_overrides, 0, sizeof self->material_overrides);
  self->grain_scale = AWRA_DEFAULT_GRAIN_SCALE;
  self->native_blur_enabled = TRUE;
  if (rebuild)
    rebuild_tokens (self);
  if (grain_changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GRAIN_SCALE]);
  if (blur_changed)
    g_object_notify_by_pspec (G_OBJECT (self),
                              properties[PROP_NATIVE_BLUR_ENABLED]);
}

static AwraMaterialProfileOverride *
get_material_override (AwraStyleManager *self,
                       AwraMaterial     *material)
{
  return &self->material_overrides[
    awra_material_profile_id_from_material (material)];
}

static gboolean
material_is_forced_opaque (AwraMaterial *material)
{
  return awra_material_get_kind (material) == AWRA_MATERIAL_KIND_SOLID ||
         awra_material_get_preset (material) == AWRA_MATERIAL_PRESET_OPAQUE;
}

static gboolean
material_can_blur (AwraMaterial *material)
{
  AwraMaterialPreset preset = awra_material_get_preset (material);

  return (preset == AWRA_MATERIAL_PRESET_NONE &&
          awra_material_get_kind (material) == AWRA_MATERIAL_KIND_FROSTED) ||
         preset == AWRA_MATERIAL_PRESET_CANVAS ||
         preset == AWRA_MATERIAL_PRESET_FLOATING;
}

const GdkRGBA *
awra_style_manager_get_material_tint (AwraStyleManager *self,
                                      AwraMaterial     *material)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), NULL);
  g_return_val_if_fail (AWRA_IS_MATERIAL (material), NULL);
  return awra_token_set_get_material_tint_internal (self->tokens, material);
}

void
awra_style_manager_set_material_tint (AwraStyleManager *self,
                                      AwraMaterial     *material,
                                      const GdkRGBA    *tint)
{
  AwraMaterialProfileOverride *override;
  GdkRGBA adjusted;

  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (AWRA_IS_MATERIAL (material));
  g_return_if_fail (tint != NULL);
  g_return_if_fail (isfinite (tint->red) &&
                    tint->red >= 0.0 && tint->red <= 1.0);
  g_return_if_fail (isfinite (tint->green) &&
                    tint->green >= 0.0 && tint->green <= 1.0);
  g_return_if_fail (isfinite (tint->blue) &&
                    tint->blue >= 0.0 && tint->blue <= 1.0);
  g_return_if_fail (isfinite (tint->alpha) &&
                    tint->alpha >= 0.0 && tint->alpha <= 1.0);
  if (awra_material_get_preset (material) == AWRA_MATERIAL_PRESET_CANVAS) {
    awra_style_manager_set_canvas_tint (self, tint);
    return;
  }
  adjusted = *tint;
  if (material_is_forced_opaque (material))
    adjusted.alpha = 1.0;
  override = get_material_override (self, material);
  if (override->tint_set && gdk_rgba_equal (&override->tint, &adjusted))
    return;
  override->tint = adjusted;
  override->tint_set = TRUE;
  rebuild_tokens (self);
}

double
awra_style_manager_get_material_radius (AwraStyleManager *self,
                                        AwraMaterial     *material)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), 0.0);
  g_return_val_if_fail (AWRA_IS_MATERIAL (material), 0.0);
  return awra_token_set_get_material_radius_internal (self->tokens, material);
}

void
awra_style_manager_set_material_radius (AwraStyleManager *self,
                                        AwraMaterial     *material,
                                        double            radius)
{
  AwraMaterialProfileOverride *override;

  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (AWRA_IS_MATERIAL (material));
  g_return_if_fail (isfinite (radius) && radius >= 0.0 && radius <= 64.0);
  override = get_material_override (self, material);
  if (override->radius_set && override->radius == radius)
    return;
  override->radius = radius;
  override->radius_set = TRUE;
  rebuild_tokens (self);
}

#define DEFINE_MATERIAL_DOUBLE_TUNING(name, field, minimum, maximum) \
  double \
  awra_style_manager_get_material_##name (AwraStyleManager *self, \
                                           AwraMaterial *material) \
  { \
    g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), 0.0); \
    g_return_val_if_fail (AWRA_IS_MATERIAL (material), 0.0); \
    return awra_token_set_get_material_##name##_internal (self->tokens, \
                                                           material); \
  } \
  void \
  awra_style_manager_set_material_##name (AwraStyleManager *self, \
                                           AwraMaterial *material, \
                                           double value) \
  { \
    AwraMaterialProfileOverride *override; \
    g_return_if_fail (AWRA_IS_STYLE_MANAGER (self)); \
    g_return_if_fail (AWRA_IS_MATERIAL (material)); \
    g_return_if_fail (isfinite (value) && value >= minimum && \
                      value <= maximum); \
    override = get_material_override (self, material); \
    if (override->field##_set && override->field == value) \
      return; \
    override->field = value; \
    override->field##_set = TRUE; \
    rebuild_tokens (self); \
  }

DEFINE_MATERIAL_DOUBLE_TUNING (grain_scale, grain_scale, 0.0, 4.0)
DEFINE_MATERIAL_DOUBLE_TUNING (outline_strength, outline_strength, 0.0, 2.0)
DEFINE_MATERIAL_DOUBLE_TUNING (highlight_strength, highlight_strength,
                               0.0, 2.0)

gboolean
awra_style_manager_get_material_blur_enabled (AwraStyleManager *self,
                                              AwraMaterial     *material)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), FALSE);
  g_return_val_if_fail (AWRA_IS_MATERIAL (material), FALSE);
  return material_can_blur (material) &&
         awra_token_set_get_material_blur_enabled_internal (self->tokens,
                                                             material);
}

void
awra_style_manager_set_material_blur_enabled (AwraStyleManager *self,
                                              AwraMaterial     *material,
                                              gboolean          enabled)
{
  AwraMaterialProfileOverride *override;

  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (AWRA_IS_MATERIAL (material));
  g_return_if_fail (material_can_blur (material));
  enabled = !!enabled;
  override = get_material_override (self, material);
  if (override->blur_enabled_set && override->blur_enabled == enabled)
    return;
  override->blur_enabled = enabled;
  override->blur_enabled_set = TRUE;
  rebuild_tokens (self);
}

void
awra_style_manager_reset_material_tuning_for_material (
  AwraStyleManager *self,
  AwraMaterial     *material)
{
  AwraMaterialProfileOverride *override;
  gboolean canvas;

  g_return_if_fail (AWRA_IS_STYLE_MANAGER (self));
  g_return_if_fail (AWRA_IS_MATERIAL (material));
  override = get_material_override (self, material);
  canvas = awra_material_get_preset (material) ==
           AWRA_MATERIAL_PRESET_CANVAS;
  if (!canvas && !override->tint_set && !override->radius_set &&
      !override->grain_scale_set && !override->outline_strength_set &&
      !override->highlight_strength_set && !override->blur_enabled_set)
    return;
  memset (override, 0, sizeof *override);
  if (canvas)
    self->canvas_tint_overridden = FALSE;
  rebuild_tokens (self);
}

AwraTokenSet *
awra_style_manager_get_token_set (AwraStyleManager *self)
{
  g_return_val_if_fail (AWRA_IS_STYLE_MANAGER (self), NULL);
  return self->tokens;
}
