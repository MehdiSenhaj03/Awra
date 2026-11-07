/* The desktop's colour scheme from the XDG Settings portal
 * (org.freedesktop.appearance color-scheme), as GTK 4 reads it for
 * gtk-interface-color-scheme, which Awra 1 followed: read once before the
 * first frame, then followed live (SettingChanged). The session bus socket
 * is watched by the loop: nothing is polled.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#if !defined(_WIN32) && !defined(__APPLE__)

#  include "dbus.h"

#  include <stdlib.h>
#  include <string.h>

void awra_style_set_portal_scheme(int scheme); /* style.c */

static AwraDBus *g_bus;
static bool g_answered;

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

#else

void awra_portal_start(void)
{
}

#endif
