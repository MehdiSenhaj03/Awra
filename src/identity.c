/* Awra 2 window identity: the application icon by its theme name (GTK's
 * gtk_window_set_icon_name / set_default_icon_name) and the startup ID or
 * activation token a window is presented with (gtk_window_set_startup_id).
 *
 * The icon's images come from the icon theme folders (hicolor: the sizes
 * there, the scalable SVG rasterized) or an SVG the application registered;
 * the window hosts take them (X11 _NET_WM_ICON, Windows WM_SETICON) or the
 * name (Wayland's xdg-toplevel-icon, the images when no theme has it). They
 * are made when a window gets its icon and freed right after.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdarg.h>
#include <stdio.h>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

BKImBuf *imb_svg_rasterize(const char *svg, int want_w, int want_h); /* kit */
const char *awra_icon_svg_source(const char *name);                  /* icons.c */

static char *g_default_icon;

static const int k_sizes[] = {16, 24, 32, 48, 64, 128};
#define N_SIZES (int)(sizeof(k_sizes) / sizeof(k_sizes[0]))

/* Formats a path into `out`: false when it doesn't fit (the lookup is then
 * skipped, never made with a truncated path). */
#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
static bool path_fmt(char *out, size_t cap, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(out, cap, fmt, ap);
  va_end(ap);
  return n >= 0 && (size_t)n < cap;
}

/* The icon theme folders, most specific first (XDG on Linux; beside the
 * executable on Windows, as GTK ships them there). */
static int icon_dirs(char dirs[][512], int max)
{
  int n = 0;
#ifdef _WIN32
  char exe[MAX_PATH];
  const DWORD l = GetModuleFileNameA(NULL, exe, MAX_PATH);
  if (l > 0 && l < MAX_PATH) {
    char *slash = strrchr(exe, '\\');
    if (slash) {
      *slash = 0;
      n += path_fmt(dirs[n], 512, "%s/share/icons", exe);
      if (n < max) {
        n += path_fmt(dirs[n], 512, "%s/../share/icons", exe);
      }
    }
  }
#else
  const char *home = getenv("XDG_DATA_HOME");
  const char *h = getenv("HOME");
  if (home && home[0]) {
    n += path_fmt(dirs[n], 512, "%s/icons", home);
  }
  else if (h) {
    n += path_fmt(dirs[n], 512, "%s/.local/share/icons", h);
  }
  if (h && n < max) {
    n += path_fmt(dirs[n], 512, "%s/.icons", h);
  }
  const char *data = getenv("XDG_DATA_DIRS");
  if (!data || !data[0]) {
    data = "/usr/local/share:/usr/share";
  }
  for (const char *p = data; *p && n < max;) {
    const char *e = strchr(p, ':');
    const int len = e ? (int)(e - p) : (int)strlen(p);
    if (len > 0) {
      n += path_fmt(dirs[n], 512, "%.*s/icons", len, p);
    }
    p += len + (e ? 1 : 0);
  }
#endif
  return n;
}

static bool readable(const char *path)
{
  FILE *f = fopen(path, "rb");
  if (f) {
    fclose(f);
  }
  return f != NULL;
}

/* One image of `size` px a side, premultiplied (malloc'd), from a decoded
 * image (scaled to fit, centred). */
static uint32_t *square(BKImBuf *ib, int size)
{
  int w = 0, h = 0;
  BK_imbuf_size(ib, &w, &h);
  const uint32_t *src = BK_imbuf_pixels(ib, 0);
  uint32_t *px = src ? (uint32_t *)calloc((size_t)size * size, sizeof(uint32_t)) : NULL;
  if (!px || w <= 0 || h <= 0) {
    free(px);
    return NULL;
  }
  const int ox = (size - w) / 2, oy = (size - h) / 2;
  for (int y = 0; y < h && y + oy < size; y++) {
    for (int x = 0; x < w && x + ox < size; x++) {
      if (y + oy >= 0 && x + ox >= 0) {
        px[(size_t)(y + oy) * size + x + ox] = src[(size_t)y * w + x];
      }
    }
  }
  return px;
}

/* The icon's images; *r_themed: a theme folder has it (Wayland compositors
 * find it by its name). Returns the count. */
static int icon_images(const char *name, uint32_t **px, int *sizes, bool *r_themed)
{
  int n = 0;
  *r_themed = false;
  const char *svg = awra_icon_svg_source(name);
  char dirs[16][512];
  const int nd = svg ? 0 : icon_dirs(dirs, 16);
  char scalable[1100] = "";
  for (int s = 0; s < N_SIZES; s++) {
    BKImBuf *ib = NULL;
    for (int d = 0; d < nd && !ib; d++) {
      char path[1100];
      if (path_fmt(path, sizeof(path), "%s/hicolor/%dx%d/apps/%s.png", dirs[d], k_sizes[s],
                   k_sizes[s], name) &&
          readable(path))
      {
        ib = BK_imbuf_load(path, k_sizes[s], k_sizes[s]);
        *r_themed = *r_themed || ib;
      }
      if (!scalable[0] &&
          path_fmt(path, sizeof(path), "%s/hicolor/scalable/apps/%s.svg", dirs[d], name) &&
          readable(path))
      {
        memcpy(scalable, path, sizeof(scalable));
        *r_themed = true;
      }
    }
    if (!ib && scalable[0]) {
      ib = BK_imbuf_load(scalable, k_sizes[s], k_sizes[s]);
    }
    if (!ib && svg) {
      ib = imb_svg_rasterize(svg, k_sizes[s], k_sizes[s]);
    }
    if (!ib && nd && s == N_SIZES - 1 && n == 0) { /* last: /usr/share/pixmaps */
      char path[1100];
      if (path_fmt(path, sizeof(path), "/usr/share/pixmaps/%s.png", name) && readable(path)) {
        ib = BK_imbuf_load(path, 128, 128);
      }
    }
    if (ib) {
      px[n] = square(ib, k_sizes[s]);
      BK_imbuf_free(ib);
      if (px[n]) {
        sizes[n++] = k_sizes[s];
      }
    }
  }
  return n;
}

static const char *icon_name_of(AwraWidget *win)
{
  const char *own = (const char *)awra_widget_get_data(win, "awra-icon-name");
  return own && own[0] ? own : g_default_icon;
}

static void apply_icon(AwraWidget *win)
{
  BKWindow *bkw = awra_window_bkw(win);
  if (!bkw) {
    return;
  }
  const char *name = icon_name_of(win);
  if (!name || !name[0]) {
    BK_window_set_icon(bkw, NULL, NULL, NULL, 0);
    return;
  }
  uint32_t *px[N_SIZES];
  int sizes[N_SIZES];
  bool themed = false;
  const bool wayland = strcmp(awra_backend(), "wayland") == 0;
  /* Wayland: a theme's icon goes by its name alone (no image memory) */
  int n = 0;
  if (wayland) {
    char dirs[16][512];
    const int nd = icon_dirs(dirs, 16);
    for (int d = 0; d < nd && !themed; d++) {
      char path[1100];
      themed = path_fmt(path, sizeof(path), "%s/hicolor/scalable/apps/%s.svg", dirs[d], name) &&
               readable(path);
      for (int s = 0; s < N_SIZES && !themed; s++) {
        themed = path_fmt(path, sizeof(path), "%s/hicolor/%dx%d/apps/%s.png", dirs[d],
                          k_sizes[s], k_sizes[s], name) &&
                 readable(path);
      }
    }
  }
  if (!themed) {
    n = icon_images(name, px, sizes, &themed);
  }
  BK_window_set_icon(bkw, name, (const uint32_t *const *)px, sizes, n);
  for (int i = 0; i < n; i++) {
    free(px[i]);
  }
}

void awra_window_identity_native(AwraWidget *win)
{
  if (icon_name_of(win)) {
    apply_icon(win);
  }
}

void awra_window_set_icon_name(AwraWidget *win, const char *name)
{
  if (!awra_widget_is(win, "AwraWindow")) {
    return;
  }
  awra_widget_set_data(win, "awra-icon-name", name ? awra_strdup(name) : NULL, free);
  apply_icon(win);
}

const char *awra_window_icon_name(AwraWidget *win)
{
  return awra_widget_is(win, "AwraWindow") ? icon_name_of(win) : NULL;
}

void awra_window_set_default_icon_name(const char *name)
{
  free(g_default_icon);
  g_default_icon = name ? awra_strdup(name) : NULL;
  for (int i = 0; i < awra_window_count(); i++) {
    AwraWidget *w = awra_window_nth(i);
    const char *own = (const char *)awra_widget_get_data(w, "awra-icon-name");
    if (!own || !own[0]) {
      apply_icon(w);
    }
  }
}

void awra_window_set_startup_id(AwraWidget *win, const char *startup_id)
{
  if (!awra_widget_is(win, "AwraWindow")) {
    return;
  }
  BKWindow *bkw = awra_window_bkw(win);
  if (bkw && awra_widget_visible(win)) { /* shown already: brought forward with it now */
    BK_window_activate(bkw, startup_id);
    return;
  }
  awra_widget_set_data(win, "awra-startup-id", startup_id ? awra_strdup(startup_id) : NULL, free);
}

/* "wayland:<handle>" or "x11:<hex window id>", as the desktop portal names
 * the window of the application that asks */
static void transient_apply(AwraWidget *win, BKWindow *bkw)
{
  const char *parent = (const char *)awra_widget_get_data(win, "awra-transient-for");
  const char *colon = parent ? strchr(parent, ':') : NULL;
  if (!parent) {
    BK_window_set_transient_for_exported(bkw, NULL);
    return;
  }
  const bool wayland = !strncmp(parent, "wayland:", 8);
  const bool x11 = !strncmp(parent, "x11:", 4);
  const char *backend = BK_window_backend();
  /* a handle for the other windowing system means nothing here */
  if (colon && ((wayland && backend && !strcmp(backend, "wayland")) || (x11 && backend && !strcmp(backend, "x11")))) {
    BK_window_set_transient_for_exported(bkw, colon + 1);
  }
}

void awra_window_set_transient_for_exported(AwraWidget *win, const char *parent)
{
  if (!awra_widget_is(win, "AwraWindow")) {
    return;
  }
  awra_widget_set_data(win, "awra-transient-for", parent && parent[0] ? awra_strdup(parent) : NULL, free);
  BKWindow *bkw = awra_window_bkw(win);
  if (bkw) {
    transient_apply(win, bkw);
  }
}

void awra_window_identity_present(AwraWidget *win)
{
  BKWindow *bkw = awra_window_bkw(win);
  if (!bkw) {
    return;
  }
  if (awra_widget_get_data(win, "awra-transient-for")) {
    transient_apply(win, bkw);
  }
  const char *id = (const char *)awra_widget_get_data(win, "awra-startup-id");
  BK_window_activate(bkw, id);
  if (id) {
    awra_widget_set_data(win, "awra-startup-id", NULL, NULL); /* once */
  }
}
