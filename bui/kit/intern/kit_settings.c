/* BUI-Awra: the desktop's toolkit settings the window hosts know, for the
 * toolkit above: string XSETTINGS values on X11 (GDK's gtk-icon-theme-name
 * from Net/IconThemeName...), and a callback when the settings daemon
 * changes them. Wayland's come from the settings portal (the toolkit reads
 * it), Windows has none.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "kit_intern.h"

static void (*g_settings_notify)(void);

void BK_set_settings_notify(void (*fn)(void))
{
  g_settings_notify = fn;
}

void bk_settings_changed(void)
{
  if (g_settings_notify) {
    g_settings_notify();
  }
}

#if defined(BUI_WINDOW_X11) && !defined(_WIN32)
int bk_unix_backend(void);                                           /* kit_window_unix.c */
int bkx11_xsettings_string(const char *name, char *out, size_t cap); /* kit_window_x11.c */

int BK_xsettings_string(const char *name, char *out, size_t cap)
{
  return bk_unix_backend() == 2 ? bkx11_xsettings_string(name, out, cap) : 0;
}
#else
int BK_xsettings_string(const char *name, char *out, size_t cap)
{
  (void)name;
  (void)out;
  (void)cap;
  return 0;
}
#endif
