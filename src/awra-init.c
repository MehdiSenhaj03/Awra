/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "awra-config.h"

#include <awra/awra.h>

#include "awra-i18n-private.h"
#include "awra-init-private.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

static GQuark provider_quark;

#define AWRA_STYLE_PROVIDER_PRIORITY_IDENTITY \
  (GTK_STYLE_PROVIDER_PRIORITY_USER + 1)

guint
awra_get_major_version (void)
{
  return AWRA_MAJOR_VERSION;
}

guint
awra_get_minor_version (void)
{
  return AWRA_MINOR_VERSION;
}

guint
awra_get_micro_version (void)
{
  return AWRA_MICRO_VERSION;
}

const char *
awra_get_version (void)
{
  return AWRA_VERSION_S;
}

void
awra_install_css_for_display (GdkDisplay *display)
{
  GtkCssProvider *provider;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (provider_quark == 0)
    provider_quark = g_quark_from_static_string ("awra-css-provider");

  if (g_object_get_qdata (G_OBJECT (display), provider_quark) != NULL)
    return;

  /* Icons Awra's own widgets rely on, found whatever the icon theme. */
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_for_display (display),
                                    "/org/awra/icons");

  provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_display (
    display,
    GTK_STYLE_PROVIDER (provider),
    AWRA_STYLE_PROVIDER_PRIORITY_IDENTITY);
  g_object_set_qdata_full (G_OBJECT (display),
                           provider_quark,
                           provider,
                           g_object_unref);
}

void
awra_update_css_for_display (GdkDisplay  *display,
                             AwraTokenSet *tokens)
{
  GtkCssProvider *provider;
  g_autoptr (GBytes) resource = NULL;
  g_autoptr (GBytes) data_app_resource = NULL;
  g_autofree char *stylesheet = NULL;
  g_autofree char *resource_text = NULL;
  g_autofree char *data_app_text = NULL;
  g_autofree char *foreground = NULL;
  g_autofree char *muted = NULL;
  g_autofree char *background = NULL;
  g_autofree char *border = NULL;
  g_autofree char *control = NULL;
  g_autofree char *control_hover = NULL;
  g_autofree char *control_pressed = NULL;
  g_autofree char *selection = NULL;
  g_autofree char *focus = NULL;
  g_autofree char *disabled = NULL;
  g_autofree char *accent = NULL;
  g_autofree char *accent_content = NULL;
  g_autofree char *accent_soft = NULL;
  g_autofree char *on_accent = NULL;
  g_autofree char *danger = NULL;
  g_autofree char *success = NULL;
  g_autofree char *warning = NULL;
  g_autofree char *chrome = NULL;
  g_autofree char *layer = NULL;
  g_autofree char *floating = NULL;
  g_autofree char *outline = NULL;
  g_autofree char *separator = NULL;
  g_autofree char *well = NULL;
  g_autofree char *cap = NULL;
  g_autofree char *cap_hover = NULL;
  g_autofree char *thumb = NULL;
  g_autofree char *track = NULL;
  g_autofree char *rim = NULL;
  gsize resource_size;
  const char *resource_data;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (AWRA_IS_TOKEN_SET (tokens));

  awra_install_css_for_display (display);
  provider = g_object_get_qdata (G_OBJECT (display), provider_quark);
  g_return_if_fail (GTK_IS_CSS_PROVIDER (provider));

  resource = g_resources_lookup_data ("/org/awra/awra.css",
                                      G_RESOURCE_LOOKUP_FLAGS_NONE,
                                      NULL);
  g_return_if_fail (resource != NULL);
  resource_data = g_bytes_get_data (resource, &resource_size);
  resource_text = g_strndup (resource_data, resource_size);
  data_app_resource = g_resources_lookup_data ("/org/awra/awra-data-app.css",
                                               G_RESOURCE_LOOKUP_FLAGS_NONE,
                                               NULL);
  g_return_if_fail (data_app_resource != NULL);
  resource_data = g_bytes_get_data (data_app_resource, &resource_size);
  data_app_text = g_strndup (resource_data, resource_size);
  foreground = gdk_rgba_to_string (awra_token_set_get_foreground (tokens));
  muted = gdk_rgba_to_string (awra_token_set_get_muted_foreground (tokens));
  background = gdk_rgba_to_string (awra_token_set_get_background (tokens));
  border = gdk_rgba_to_string (awra_token_set_get_border (tokens));
  control = gdk_rgba_to_string (awra_token_set_get_control_background (tokens));
  control_hover = gdk_rgba_to_string (awra_token_set_get_control_hover (tokens));
  control_pressed = gdk_rgba_to_string (awra_token_set_get_control_pressed (tokens));
  selection = gdk_rgba_to_string (awra_token_set_get_selection (tokens));
  focus = gdk_rgba_to_string (awra_token_set_get_focus_ring (tokens));
  disabled = gdk_rgba_to_string (
    awra_token_set_get_disabled_foreground (tokens));
  accent = gdk_rgba_to_string (awra_token_set_get_accent (tokens));
  accent_content = gdk_rgba_to_string (
    awra_token_set_get_accent_content (tokens));
  accent_soft = gdk_rgba_to_string (awra_token_set_get_accent_soft (tokens));
  on_accent = gdk_rgba_to_string (awra_token_set_get_on_accent (tokens));
  danger = gdk_rgba_to_string (awra_token_set_get_danger (tokens));
  success = gdk_rgba_to_string (awra_token_set_get_success (tokens));
  warning = gdk_rgba_to_string (awra_token_set_get_warning (tokens));
  chrome = gdk_rgba_to_string (awra_token_set_get_surface_chrome (tokens));
  layer = gdk_rgba_to_string (awra_token_set_get_surface_layer (tokens));
  floating = gdk_rgba_to_string (awra_token_set_get_surface_floating (tokens));
  outline = gdk_rgba_to_string (awra_token_set_get_outline (tokens));
  separator = gdk_rgba_to_string (awra_token_set_get_separator (tokens));
  well = gdk_rgba_to_string (awra_token_set_get_control_well (tokens));
  cap = gdk_rgba_to_string (awra_token_set_get_control_cap (tokens));
  cap_hover = gdk_rgba_to_string (
    awra_token_set_get_control_cap_hover (tokens));
  thumb = gdk_rgba_to_string (awra_token_set_get_control_thumb (tokens));
  track = gdk_rgba_to_string (awra_token_set_get_control_track (tokens));
  rim = gdk_rgba_to_string (awra_token_set_get_rim_highlight (tokens));
  stylesheet = g_strdup_printf (
    "@define-color awra_foreground %s;\n"
    "@define-color awra_muted %s;\n"
    "@define-color awra_background %s;\n"
    "@define-color awra_border %s;\n"
    "@define-color awra_control %s;\n"
    "@define-color awra_control_hover %s;\n"
    "@define-color awra_control_pressed %s;\n"
    "@define-color awra_selection %s;\n"
    "@define-color awra_focus %s;\n"
    "@define-color awra_disabled %s;\n"
    "@define-color awra_accent %s;\n"
    "@define-color awra_accent_content %s;\n"
    "@define-color awra_accent_soft %s;\n"
    "@define-color awra_on_accent %s;\n"
    "@define-color awra_danger %s;\n"
    "@define-color awra_success %s;\n"
    "@define-color awra_warning %s;\n"
    "@define-color awra_chrome %s;\n"
    "@define-color awra_layer %s;\n"
    "@define-color awra_floating %s;\n"
    "@define-color awra_outline %s;\n"
    "@define-color awra_separator %s;\n"
    "@define-color awra_well %s;\n"
    "@define-color awra_cap %s;\n"
    "@define-color awra_cap_hover %s;\n"
    "@define-color awra_thumb %s;\n"
    "@define-color awra_track %s;\n"
    "@define-color awra_rim %s;\n%s\n%s",
    foreground,
    muted,
    background,
    border,
    control,
    control_hover,
    control_pressed,
    selection,
    focus,
    disabled,
    accent,
    accent_content,
    accent_soft,
    on_accent,
    danger,
    success,
    warning,
    chrome,
    layer,
    floating,
    outline,
    separator,
    well,
    cap,
    cap_hover,
    thumb,
    track,
    rim,
    resource_text,
    data_app_text);
  gtk_css_provider_load_from_string (provider, stylesheet);
}

#ifdef G_OS_WIN32
/* Windows installs are relocatable: translations sit in share/locale next to
 * the bin/ folder holding this DLL, wherever the user installed it. */
static char *
relocated_localedir (void)
{
  HMODULE module = NULL;
  g_autofree char *root = NULL;

  if (!GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR) (gpointer) relocated_localedir, &module))
    return g_strdup (AWRA_LOCALEDIR);
  root = g_win32_get_package_installation_directory_of_module (module);
  return g_build_filename (root, "share", "locale", NULL);
}
#endif

/* Awra's visible defaults live in the awra-1 gettext domain. Bind it to the
 * install prefix, or to $AWRA_LOCALEDIR when running from a build tree. */
static void
ensure_translations (void)
{
  static gsize bound = 0;

  if (g_once_init_enter (&bound)) {
    const char *override = g_getenv ("AWRA_LOCALEDIR");
#ifdef G_OS_WIN32
    g_autofree char *installed = relocated_localedir ();
#else
    const char *installed = AWRA_LOCALEDIR;
#endif

    bindtextdomain (GETTEXT_PACKAGE,
                    override != NULL && *override != '\0'
                      ? override : installed);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    g_once_init_leave (&bound, 1);
  }
}

void
awra_init (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  ensure_translations ();

  if (display == NULL) {
    g_warning ("awra_init() called before GTK opened a display");
    return;
  }

  (void) awra_context_get_for_display (display);
}

void
awra_widget_set_tooltip_text (GtkWidget  *widget,
                              const char *text)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  gtk_widget_set_tooltip_text (widget, text);
}
