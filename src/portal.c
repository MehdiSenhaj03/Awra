/* The desktop's colour scheme from the XDG Settings portal
 * (org.freedesktop.appearance color-scheme), as GTK 4 reads it for
 * gtk-interface-color-scheme, which Awra 1 followed: read once before the
 * first frame, then followed live (SettingChanged). The session bus socket
 * is watched by the loop: nothing is polled.
 *
 * On Wayland GTK also takes its font options (gtk-xft-antialias,
 * gtk-xft-hinting, gtk-xft-hintstyle) from the portal's GNOME settings
 * (gdksettings-wayland.c): read here too, for the text engine.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#if !defined(_WIN32) && !defined(__APPLE__)

#  include "dbus.h"

#  include <stdlib.h>
#  include <string.h>

void awra_style_set_portal_scheme(int scheme); /* style.c */
void awra_portal_start(void);

static AwraDBus *g_bus;
static bool g_answered;

/* GTK's font settings from the portal (gdksettings-wayland.c): the
 * org.gnome.desktop.interface keys, else the GNOME settings daemon's
 * xsettings plugin keys; -1 when absent. GsdFontHinting: 0 none .. 3 full;
 * GsdFontAntialiasingMode: 0 none, 1 grayscale, 2 rgba. */
static struct {
  bool portal; /* ReadAll answered with settings (GTK keeps the portal) */
  int if_aa, if_hint, xs_aa, xs_hint;
  bool apply; /* Wayland windows: hand them to the text engine */
} g_font = {false, -1, -1, -1, -1, false};

#  define PORTAL_DEST "org.freedesktop.portal.Desktop"
#  define PORTAL_PATH "/org/freedesktop/portal/desktop"
#  define PORTAL_IFACE "org.freedesktop.portal.Settings"

/* a variant holding a uint32 (or a variant of one: the older Read) */
static bool read_scheme(AwraDBusR *r, uint32_t *out)
{
  for (int depth = 0; depth < 2 && !r->err; depth++) {
    const char *sig = awra_dbus_r_sig(r);
    if (!sig || r->err) {
      return false;
    }
    if (!strcmp(sig, "u")) {
      *out = awra_dbus_r_u32(r);
      return !r->err;
    }
    if (strcmp(sig, "v") != 0) {
      return false;
    }
  }
  return false;
}

static void read_reply(AwraDBus *bus, const AwraDBusMsg *m, void *data);

static int gsd_enum(const char *v, bool hinting)
{
  static const char *const hint[] = {"none", "slight", "medium", "full"};
  static const char *const aa[] = {"none", "grayscale", "rgba"};
  for (int i = 0; i < (hinting ? 4 : 3); i++) {
    if (!strcmp(v, hinting ? hint[i] : aa[i])) {
      return i;
    }
  }
  return -1;
}

/* One setting (a variant) of namespace `ns`, `key`. */
static void font_setting(AwraDBusR *r, const char *ns, const char *key)
{
  const char *sig = awra_dbus_r_sig(r);
  if (!sig || r->err) {
    return;
  }
  if (!strcmp(sig, "v")) { /* SettingChanged: the value is a variant (some portals nest one) */
    font_setting(r, ns, key);
    return;
  }
  const bool iface = !strcmp(ns, "org.gnome.desktop.interface");
  const bool xs = !strcmp(ns, "org.gnome.settings-daemon.plugins.xsettings");
  int *slot = NULL;
  bool hinting = false;
  if (iface && !strcmp(key, "font-antialiasing")) {
    slot = &g_font.if_aa;
  }
  else if (iface && !strcmp(key, "font-hinting")) {
    slot = &g_font.if_hint;
    hinting = true;
  }
  else if (xs && !strcmp(key, "antialiasing")) {
    slot = &g_font.xs_aa;
  }
  else if (xs && !strcmp(key, "hinting")) {
    slot = &g_font.xs_hint;
    hinting = true;
  }
  if (slot && !strcmp(sig, "s")) {
    const char *v = awra_dbus_r_str(r);
    if (v && !r->err) {
      *slot = gsd_enum(v, hinting);
    }
    return;
  }
  awra_dbus_r_skip(r, &sig);
}

/* GTK's update_xft_settings, to BUI_text_set_font_options */
static void font_apply(void)
{
  if (!g_font.apply) {
    return;
  }
  int aa = 1, hint = 2; /* no portal: grayscale, medium */
  if (g_font.portal) {
    if (g_font.if_aa >= 0) {
      aa = g_font.if_aa;
      hint = g_font.if_hint >= 0 ? g_font.if_hint : 1;
    }
    else {
      aa = g_font.xs_aa >= 0 ? g_font.xs_aa : 1;
      hint = g_font.xs_hint >= 0 ? g_font.xs_hint : 1;
    }
  }
  /* GTK 4 antialiases in grayscale whatever the mode, unless none */
  BUI_text_set_font_options(hint, aa != 0);
}

static void read_all_reply(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  (void)bus;
  (void)data;
  if (!m || m->type != AWRA_DBUS_RETURN) {
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  size_t end;
  if (!awra_dbus_r_array(&r, 8, &end)) {
    return; /* no settings: GTK drops the portal */
  }
  g_font.portal = true;
  while (r.pos < end && !r.err) {
    awra_dbus_r_struct(&r);
    const char *ns = awra_dbus_r_str(&r);
    size_t end2;
    if (!ns || r.err || !awra_dbus_r_array(&r, 8, &end2)) {
      continue;
    }
    while (r.pos < end2 && !r.err) {
      awra_dbus_r_struct(&r);
      const char *key = awra_dbus_r_str(&r);
      if (!key || r.err) {
        break;
      }
      font_setting(&r, ns, key);
    }
  }
  font_apply();
}

static void ask(AwraDBus *bus, const char *method)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_str(&w, "org.freedesktop.appearance");
  awra_dbus_w_str(&w, "color-scheme");
  awra_dbus_call(bus, PORTAL_DEST, PORTAL_PATH, PORTAL_IFACE, method, "ss", &w, read_reply,
                 (void *)method);
  awra_dbus_w_free(&w);
}

static void read_reply(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  if (m && m->type == AWRA_DBUS_ERROR && !strcmp((const char *)data, "ReadOne")) {
    ask(bus, "Read"); /* a portal older than ReadOne */
    return;
  }
  g_answered = true;
  if (!m || m->type != AWRA_DBUS_RETURN) {
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  uint32_t v;
  if (read_scheme(&r, &v)) {
    awra_style_set_portal_scheme((int)v);
  }
}

static void ready(AwraDBus *bus, void *data)
{
  (void)data;
  awra_dbus_add_match(bus, "type='signal',interface='" PORTAL_IFACE "',member='SettingChanged',"
                           "arg0='org.freedesktop.appearance',arg1='color-scheme'");
  awra_dbus_add_match(bus, "type='signal',interface='" PORTAL_IFACE "',member='SettingChanged',"
                           "arg0='org.gnome.desktop.interface'");
  awra_dbus_add_match(bus, "type='signal',interface='" PORTAL_IFACE "',member='SettingChanged',"
                           "arg0='org.gnome.settings-daemon.plugins.xsettings'");
  /* GTK's ReadAll (gdksettings-wayland.c), answered before the colour
   * scheme the first frame waits for */
  AwraDBusW w;
  awra_dbus_w_init(&w);
  AwraDBusArray a = awra_dbus_w_array_open(&w, 4);
  awra_dbus_w_str(&w, "org.gnome.*");
  awra_dbus_w_str(&w, "org.freedesktop.appearance");
  awra_dbus_w_array_close(&w, a);
  awra_dbus_call(bus, PORTAL_DEST, PORTAL_PATH, PORTAL_IFACE, "ReadAll", "as", &w, read_all_reply,
                 NULL);
  awra_dbus_w_free(&w);
  ask(bus, "ReadOne");
}

static void message(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  (void)bus;
  (void)data;
  if (m->type != AWRA_DBUS_SIGNAL || !m->member || strcmp(m->member, "SettingChanged") != 0) {
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  const char *ns = awra_dbus_r_str(&r);
  const char *key = awra_dbus_r_str(&r);
  uint32_t v;
  if (ns && key && !strcmp(ns, "org.freedesktop.appearance") && !strcmp(key, "color-scheme") &&
      read_scheme(&r, &v))
  {
    awra_style_set_portal_scheme((int)v);
  }
  else if (ns && key && !strncmp(ns, "org.gnome.", 10)) {
    const int before[4] = {g_font.if_aa, g_font.if_hint, g_font.xs_aa, g_font.xs_hint};
    font_setting(&r, ns, key);
    if (before[0] != g_font.if_aa || before[1] != g_font.if_hint || before[2] != g_font.xs_aa ||
        before[3] != g_font.xs_hint)
    {
      font_apply();
    }
  }
}

/* Windows on Wayland take GTK's font options from the portal (on X11 the
 * kit reads XSETTINGS); without a session bus, GTK's defaults. */
void awra_portal_wayland_fonts(void)
{
  if (g_font.apply) {
    return;
  }
  awra_portal_start();
  g_font.apply = true;
  font_apply();
}

static void closed(AwraDBus *bus, void *data)
{
  (void)data;
  if (g_bus == bus) {
    g_bus = NULL;
  }
  g_answered = true;
}

void awra_portal_start(void)
{
  static bool started;
  if (started) {
    return;
  }
  started = true;
  const char *addr = awra_dbus_session_address();
  if (!addr) {
    return;
  }
  const AwraDBusCallbacks cb = {ready, closed, message, NULL};
  g_bus = awra_dbus_connect(addr, &cb);
  /* the answer before the first frame (GTK waits for its settings too);
   * a desktop without the portal costs at most this */
  awra_dbus_pump(g_bus, &g_answered, 250);
}

/* A desktop notification (org.freedesktop.Notifications.Notify, as
 * GApplication's fallback): false without a session bus. */
bool awra_desktop_notify(const char *app_name, const char *icon, const char *title, const char *body)
{
  awra_portal_start();
  if (!g_bus) {
    return false;
  }
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_str(&w, app_name ? app_name : "");
  awra_dbus_w_u32(&w, 0);
  awra_dbus_w_str(&w, icon ? icon : "");
  awra_dbus_w_str(&w, title ? title : "");
  awra_dbus_w_str(&w, body ? body : "");
  AwraDBusArray actions = awra_dbus_w_array_open(&w, 4);
  awra_dbus_w_array_close(&w, actions);
  AwraDBusArray hints = awra_dbus_w_array_open(&w, 8);
  awra_dbus_w_array_close(&w, hints);
  awra_dbus_w_i32(&w, -1);
  const bool ok = !w.oom && awra_dbus_call(g_bus, "org.freedesktop.Notifications",
                                           "/org/freedesktop/Notifications",
                                           "org.freedesktop.Notifications", "Notify",
                                           "susssasa{sv}i", &w, NULL, NULL) != 0;
  awra_dbus_w_free(&w);
  return ok;
}

#else

void awra_portal_start(void)
{
}

void awra_portal_wayland_fonts(void)
{
}

bool awra_desktop_notify(const char *app_name, const char *icon, const char *title, const char *body)
{
  (void)app_name;
  (void)icon;
  (void)title;
  (void)body;
  return false; /* Windows applications notify through their tray icon */
}

#endif
