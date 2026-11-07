/* Awra 2 icon themes, shared by icontheme.c (GtkIconTheme's lookup),
 * icons.c (loading, recolouring, drawing) and contenttype.c.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The built-in icons (icons-data.c, tools/gen_icons.py): GTK's resource
 * icons (/org/gtk/libgtk/icons: its symbolic icons, unthemed, and hicolor's
 * 16x16/status/image-missing.png), Awra's (/org/awra/icons, unthemed, as
 * Awra 1 added them) and Adwaita's symbolic icons, the "Adwaita" theme of
 * systems that have none installed (Windows). Paths are relative to their
 * set, sorted (strcmp). */
typedef struct AwraBuiltinIcon {
  const char *path;
  const unsigned char *data;
  unsigned len;
} AwraBuiltinIcon;

typedef struct AwraBuiltinSet {
  const char *name; /* "gtk", "awra", "adwaita" */
  const AwraBuiltinIcon *icons;
  int count;
} AwraBuiltinSet;

extern const AwraBuiltinSet awra_builtin_gtk, awra_builtin_awra, awra_builtin_adwaita;
extern const char awra_builtin_hicolor_index[]; /* GTK's hicolor.index.theme */
extern const char awra_builtin_adwaita_index[];

const AwraBuiltinIcon *awra_builtin_find(const AwraBuiltinSet *set, const char *path);

/* A file the lookup chose (or a built-in icon, or an SVG the application
 * registered): shared by every lookup that ends there, its rasters cached
 * (icons.c) while a lookup holds it. */
typedef struct AwraIconRaster AwraIconRaster;
typedef struct AwraIconFile {
  char *path;                /* NULL: data */
  const unsigned char *data; /* built-in or registered bytes */
  size_t len;
  char *icon_name; /* the name chosen */
  bool is_svg, is_symbolic, is_png;
  int refs; /* lookups that hold it */
  AwraIconRaster *rasters;
  struct AwraIconFile *next; /* hash chain */
} AwraIconFile;

enum {
  AWRA_ICON_FLAG_FORCE_REGULAR = 1 << 0,
  AWRA_ICON_FLAG_FORCE_SYMBOLIC = 1 << 1,
};

/* GtkIconTheme's choice for a widget's icon: `spec` holds the names of a
 * GThemedIcon ('\n'-separated, expanded as g_themed_icon_new_from_names
 * does) or a file path; size and scale as gtk_icon_theme_lookup_icon takes
 * them (application px, integer window scale), dir 1 LTR, 2 RTL, 0 none.
 * Never NULL for a name (image-missing at worst); cached. */
AwraIconFile *awra_icon_theme_choose(const char *spec, int size, int scale, int dir, unsigned flags);
/* The theme's serial: bumped whenever the theme changes. */
unsigned awra_icon_theme_serial(void);

/* icons.c */
void awra_icon_file_free_rasters(AwraIconFile *f);
void awra_icon_rasters_clear(void);
/* the file's bytes (NULL when it can't be read): *r_own true when the
 * caller frees them */
const unsigned char *awra_icon_file_bytes(const AwraIconFile *f, size_t *r_len, bool *r_own);

/* GThemedIcon's names (gthemedicon.c g_themed_icon_update_names): the names
 * given, their dash-shortened fallbacks with `default_fallbacks`, then the
 * symbolic/regular variants. Writes them '\n'-separated into a malloc'd
 * string; NULL on no memory. */
char *awra_themed_icon_expand(const char *const *names, int n, bool default_fallbacks);

/* Reading files (icontheme.c): the whole file, malloc'd with a NUL past
 * *r_len; NULL when unreadable. */
char *awra_read_file(const char *path, size_t *r_len);
/* Calls fn for each entry name of a directory; false when it can't be
 * opened. */
bool awra_list_dir(const char *path, void (*fn)(const char *name, void *data), void *data);
/* The XDG data directories GLib would give (g_get_user_data_dir first,
 * then g_get_system_data_dirs), each without a trailing slash: a
 * NULL-terminated malloc'd array of malloc'd strings (awra_strv_free). */
char **awra_xdg_data_dirs(bool with_user);
void awra_strv_free(char **v);
