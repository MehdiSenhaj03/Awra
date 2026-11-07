/* Awra 2 icon themes: GtkIconTheme's lookup, rule for rule (GTK 4.22,
 * gtk/gtkicontheme.c), so that an icon name shows the file GTK would show.
 *
 * - The theme: gtk-icon-theme-name as GtkSettings has it: the settings
 *   portal's org.gnome.desktop.interface icon-theme on Wayland ("gnome" when
 *   the portal answers without it), the settings daemon's Net/IconThemeName
 *   on X11, else gtk-4.0/settings.ini (/etc, $XDG_CONFIG_DIRS, then
 *   $XDG_CONFIG_HOME), else "Adwaita"; followed live.
 * - The search path: $XDG_DATA_HOME/icons, ~/.icons, $XDG_DATA_DIRS/icons,
 *   $XDG_DATA_DIRS/pixmaps; themes found there by their index.theme, their
 *   Inherits chain, then hicolor; directories Fixed / Scalable / Threshold
 *   with their sizes and scales, chosen by GTK's distance rules; the
 *   icon-theme.cache files (mapped, looked up by hash) where they are up to
 *   date, else the directories, listed the first time a lookup needs them.
 * - The names: a symbolic name is looked for in every theme first, then
 *   every name in each theme in turn, then unthemed icons (images directly
 *   in the search path, GTK's and Awra's resource icons, SVGs the
 *   application registered), then image-missing. Widgets look icons up as
 *   GtkImage does: a GThemedIcon of the name (its symbolic / regular
 *   variant after it), the widget's direction (-ltr / -rtl names first).
 * - Adwaita's symbolic icons are built in: the "Adwaita" theme of systems
 *   without one (Windows, as GTK's Windows builds ship it).
 *
 * Nothing is read before the first lookup; a lookup's answer is cached
 * (bounded), so drawing never touches the file system; GTK's check for
 * changed directories (every 5 s at most) runs only when a lookup misses.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "icontheme.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

double awra_now_ms(void);                  /* core.c */
const char *awra_portal_icon_theme(void);  /* portal.c */
void awra_portal_start(void);              /* portal.c */
void awra_windows_icons_changed(void);     /* window.c */

/* ======================================================================= */
/* Small helpers */

static char *join(const char *a, const char *b)
{
  const size_t la = strlen(a), lb = strlen(b);
  const bool slash = la > 0 && a[la - 1] != '/';
  char *s = (char *)malloc(la + lb + 2);
  if (s) {
    memcpy(s, a, la);
    if (slash) {
      s[la] = '/';
    }
    memcpy(s + la + slash, b, lb + 1);
  }
  return s;
}

static bool has_suffix(const char *s, const char *suffix)
{
  const size_t ls = strlen(s), lx = strlen(suffix);
  return ls >= lx && memcmp(s + ls - lx, suffix, lx) == 0;
}

static uint32_t fnv(const char *s, uint32_t h)
{
  for (; *s; s++) {
    h = (h ^ (unsigned char)*s) * 16777619u;
  }
  return h;
}

void awra_strv_free(char **v)
{
  if (v) {
    for (char **p = v; *p; p++) {
      free(*p);
    }
    free(v);
  }
}

typedef struct Strv {
  char **v;
  int n, cap;
} Strv;

static void strv_add(Strv *s, char *str) /* takes str */
{
  if (!str) {
    return;
  }
  if (s->n + 2 > s->cap) {
    const int cap = s->cap ? s->cap * 2 : 8;
    char **v = (char **)realloc(s->v, sizeof(char *) * cap);
    if (!v) {
      free(str);
      return;
    }
    s->v = v;
    s->cap = cap;
  }
  s->v[s->n++] = str;
  s->v[s->n] = NULL;
}

static bool strv_has(const Strv *s, const char *str)
{
  for (int i = 0; i < s->n; i++) {
    if (!strcmp(s->v[i], str)) {
      return true;
    }
  }
  return false;
}

#ifdef _WIN32
static wchar_t *wide(const char *utf8)
{
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  wchar_t *w = n > 0 ? (wchar_t *)malloc(sizeof(wchar_t) * n) : NULL;
  if (w) {
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
  }
  return w;
}
#endif

/* stat: 1 directory, 2 regular file, 0 neither (or missing) */
static int path_kind(const char *path, long long *r_mtime, long long *r_size)
{
#ifdef _WIN32
  wchar_t *w = wide(path);
  struct _stat64 st;
  const int ok = w && _wstat64(w, &st) == 0;
  free(w);
#else
  struct stat st;
  const int ok = stat(path, &st) == 0;
#endif
  if (!ok) {
    return 0;
  }
  if (r_mtime) {
    *r_mtime = (long long)st.st_mtime;
  }
  if (r_size) {
    *r_size = (long long)st.st_size;
  }
#ifdef _WIN32
  return (st.st_mode & _S_IFDIR) ? 1 : (st.st_mode & _S_IFREG) ? 2 : 0;
#else
  return S_ISDIR(st.st_mode) ? 1 : S_ISREG(st.st_mode) ? 2 : 0;
#endif
}

char *awra_read_file(const char *path, size_t *r_len)
{
#ifdef _WIN32
  wchar_t *w = wide(path);
  FILE *f = w ? _wfopen(w, L"rb") : NULL;
  free(w);
#else
  FILE *f = fopen(path, "rb");
#endif
  if (!f) {
    return NULL;
  }
  char *buf = NULL;
  size_t len = 0, cap = 0;
  for (;;) {
    if (len + 4096 + 1 > cap) {
      cap = cap ? cap * 2 : 8192;
      char *nb = (char *)realloc(buf, cap);
      if (!nb) {
        free(buf);
        fclose(f);
        return NULL;
      }
      buf = nb;
    }
    const size_t got = fread(buf + len, 1, cap - len - 1, f);
    len += got;
    if (got == 0) {
      break;
    }
  }
  fclose(f);
  buf[len] = 0;
  if (r_len) {
    *r_len = len;
  }
  return buf;
}

bool awra_list_dir(const char *path, void (*fn)(const char *name, void *data), void *data)
{
#ifdef _WIN32
  char *pattern = join(path, "*");
  wchar_t *w = pattern ? wide(pattern) : NULL;
  free(pattern);
  WIN32_FIND_DATAW fd;
  HANDLE h = w ? FindFirstFileW(w, &fd) : INVALID_HANDLE_VALUE;
  free(w);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }
  do {
    char name[MAX_PATH * 3];
    if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof(name), NULL, NULL) > 0 &&
        strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
    {
      fn(name, data);
    }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return true;
#else
  DIR *d = opendir(path);
  if (!d) {
    return false;
  }
  struct dirent *e;
  while ((e = readdir(d))) {
    if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) {
      fn(e->d_name, data);
    }
  }
  closedir(d);
  return true;
#endif
}

static char *home_dir(void)
{
#ifdef _WIN32
  const char *h = getenv("USERPROFILE");
#else
  const char *h = getenv("HOME");
#endif
  return awra_strdup(h && h[0] ? h : "/");
}

#ifdef _WIN32
/* the directory of the executable, its "share" as GLib finds it there */
static char *exe_dir(void)
{
  wchar_t w[MAX_PATH];
  const DWORD l = GetModuleFileNameW(NULL, w, MAX_PATH);
  if (l == 0 || l >= MAX_PATH) {
    return NULL;
  }
  char buf[MAX_PATH * 3];
  if (WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL) <= 0) {
    return NULL;
  }
  for (char *p = buf; *p; p++) {
    if (*p == '\\') {
      *p = '/';
    }
  }
  char *slash = strrchr(buf, '/');
  if (slash) {
    *slash = 0;
  }
  return awra_strdup(buf);
}
#endif

/* GLib's g_get_user_data_dir + g_get_system_data_dirs */
char **awra_xdg_data_dirs(bool with_user)
{
  Strv s = {0};
#ifdef _WIN32
  if (with_user) {
    const char *la = getenv("LOCALAPPDATA");
    if (la && la[0]) {
      strv_add(&s, awra_strdup(la));
    }
  }
  char *exe = exe_dir();
  if (exe) {
    strv_add(&s, join(exe, "share"));
    char *up = join(exe, "../share");
    strv_add(&s, up);
    free(exe);
  }
#else
  if (with_user) {
    const char *dh = getenv("XDG_DATA_HOME");
    if (dh && dh[0]) {
      strv_add(&s, awra_strdup(dh));
    }
    else {
      char *h = home_dir();
      strv_add(&s, join(h, ".local/share"));
      free(h);
    }
  }
  const char *dd = getenv("XDG_DATA_DIRS");
  if (!dd || !dd[0]) {
    dd = "/usr/local/share/:/usr/share/";
  }
  for (const char *p = dd; *p;) {
    const char *e = strchr(p, ':');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    if (len > 0) {
      char *d = (char *)malloc(len + 1);
      if (d) {
        memcpy(d, p, len);
        d[len] = 0;
        while (len > 1 && d[len - 1] == '/') {
          d[--len] = 0;
        }
        strv_add(&s, d);
      }
    }
    p += (e ? (size_t)(e - p) + 1 : strlen(p));
  }
#endif
  if (!s.v) {
    strv_add(&s, awra_strdup("/usr/share"));
  }
  return s.v;
}

/* ======================================================================= */
/* index.theme (GKeyFile's rules that matter: groups, the last value of a
 * key wins, duplicate groups merge, ',' lists) */

typedef struct IniEntry {
  const char *group, *key, *value;
} IniEntry;

typedef struct Ini {
  char *text;
  IniEntry *e;
  int n;
  /* by group: the first entry of each group name (hash, open addressing),
   * then each entry's next one of the same group (-1: none) */
  int *head, *next;
  const char **head_name;
  int cap;
} Ini;

static void ini_parse(Ini *ini, char *text)
{
  memset(ini, 0, sizeof(*ini));
  ini->text = text;
  int cap = 0;
  const char *group = "";
  for (char *line = text; line && *line;) {
    char *next = strchr(line, '\n');
    if (next) {
      *next++ = 0;
    }
    size_t l = strlen(line);
    while (l > 0 && (line[l - 1] == '\r' || line[l - 1] == ' ' || line[l - 1] == '\t')) {
      line[--l] = 0;
    }
    char *p = line;
    while (*p == ' ' || *p == '\t') {
      p++;
    }
    if (*p == '[') {
      char *end = strchr(p, ']');
      if (end) {
        *end = 0;
        group = p + 1;
      }
    }
    else if (*p && *p != '#') {
      char *eq = strchr(p, '=');
      if (eq) {
        char *k_end = eq;
        while (k_end > p && (k_end[-1] == ' ' || k_end[-1] == '\t')) {
          k_end--;
        }
        *k_end = 0;
        char *v = eq + 1;
        while (*v == ' ' || *v == '\t') {
          v++;
        }
        if (ini->n + 1 > cap) {
          cap = cap ? cap * 2 : 256;
          IniEntry *ne = (IniEntry *)realloc(ini->e, sizeof(IniEntry) * cap);
          if (!ne) {
            return;
          }
          ini->e = ne;
        }
        ini->e[ini->n++] = (IniEntry){group, p, v};
      }
    }
    line = next;
  }
}

static void ini_free(Ini *ini)
{
  free(ini->text);
  free(ini->e);
  free(ini->head);
  free(ini->next);
  free(ini->head_name);
  memset(ini, 0, sizeof(*ini));
}

/* the index of groups (index.theme files have hundreds: hicolor's ~470) */
static void ini_index(Ini *ini)
{
  int cap = 64;
  while (cap < ini->n * 2) {
    cap *= 2;
  }
  ini->head = (int *)malloc(sizeof(int) * cap);
  ini->head_name = (const char **)calloc((size_t)cap, sizeof(char *));
  ini->next = (int *)malloc(sizeof(int) * (ini->n ? ini->n : 1));
  int *tail = (int *)malloc(sizeof(int) * cap);
  if (!ini->head || !ini->head_name || !ini->next || !tail) {
    free(ini->head);
    free(ini->head_name);
    free(ini->next);
    free(tail);
    ini->head = ini->next = NULL;
    ini->head_name = NULL;
    return;
  }
  ini->cap = cap;
  for (int i = 0; i < ini->n; i++) {
    ini->next[i] = -1;
    uint32_t h = fnv(ini->e[i].group, 2166136261u) & (uint32_t)(cap - 1);
    while (ini->head_name[h] && strcmp(ini->head_name[h], ini->e[i].group) != 0) {
      h = (h + 1) & (uint32_t)(cap - 1);
    }
    if (!ini->head_name[h]) {
      ini->head_name[h] = ini->e[i].group;
      ini->head[h] = i;
    }
    else {
      ini->next[tail[h]] = i;
    }
    tail[h] = i;
  }
  free(tail);
}

static const char *ini_get(Ini *ini, const char *group, const char *key)
{
  if (!ini->head && ini->n) {
    ini_index(ini);
  }
  const char *v = NULL;
  if (!ini->head) {
    for (int i = 0; i < ini->n; i++) {
      if (!strcmp(ini->e[i].key, key) && !strcmp(ini->e[i].group, group)) {
        v = ini->e[i].value; /* the last one wins */
      }
    }
    return v;
  }
  uint32_t h = fnv(group, 2166136261u) & (uint32_t)(ini->cap - 1);
  while (ini->head_name[h] && strcmp(ini->head_name[h], group) != 0) {
    h = (h + 1) & (uint32_t)(ini->cap - 1);
  }
  if (!ini->head_name[h]) {
    return NULL;
  }
  for (int i = ini->head[h]; i >= 0; i = ini->next[i]) {
    if (!strcmp(ini->e[i].key, key)) {
      v = ini->e[i].value; /* the last one wins (duplicate groups merge) */
    }
  }
  return v;
}

/* g_key_file_get_integer: a whole decimal number, else not one */
static bool ini_int(Ini *ini, const char *group, const char *key, int *r)
{
  const char *v = ini_get(ini, group, key);
  if (!v) {
    return false;
  }
  char *end;
  errno = 0;
  const long n = strtol(v, &end, 10);
  if (end == v || *end || errno) {
    *r = 0;
    return true; /* has the key, an invalid number reads 0 */
  }
  *r = (int)n;
  return true;
}

/* g_key_file_get_string_list with ',' (escaped "\," kept), empty items
 * after the last separator dropped */
static void ini_list(Ini *ini, const char *group, const char *key, Strv *out)
{
  const char *v = ini_get(ini, group, key);
  if (!v) {
    return;
  }
  const char *p = v;
  while (*p) {
    char buf[1024];
    size_t n = 0;
    while (*p && *p != ',') {
      if (*p == '\\' && p[1]) {
        p++;
      }
      if (n + 1 < sizeof(buf)) {
        buf[n++] = *p;
      }
      p++;
    }
    buf[n] = 0;
    strv_add(out, awra_strdup(buf));
    if (*p == ',') {
      p++;
    }
  }
}

/* ======================================================================= */
/* icon-theme.cache (gtk-update-icon-cache's format: header, hash of names,
 * image lists of directory index + flags; big-endian) */

enum {
  SUFFIX_NONE = 0,
  SUFFIX_XPM = 1 << 0,
  SUFFIX_SVG = 1 << 1,
  SUFFIX_PNG = 1 << 2,
  SUFFIX_SYMBOLIC_PNG = 1 << 4,
};

/* The file is read in 4 KiB blocks as lookups need them, a few kept: a
 * lookup touches a handful of scattered entries, and mapping the file
 * would bring large parts of it in (Papirus' is 2.8 MB). */
#define CACHE_BLOCK 4096
#define CACHE_SLOTS 16

typedef struct IconCache {
#ifdef _WIN32
  FILE *f;
#else
  int fd;
#endif
  size_t len;
  uint32_t id[CACHE_SLOTS]; /* block number + 1, 0: empty */
  unsigned used[CACHE_SLOTS];
  unsigned clock;
  uint8_t *blk[CACHE_SLOTS];
} IconCache;

static const uint8_t *cache_block(IconCache *c, uint32_t b)
{
  int victim = 0;
  for (int i = 0; i < CACHE_SLOTS; i++) {
    if (c->id[i] == b + 1) {
      c->used[i] = ++c->clock;
      return c->blk[i];
    }
    if (c->used[i] < c->used[victim]) {
      victim = i;
    }
  }
  if (!c->blk[victim]) {
    c->blk[victim] = (uint8_t *)malloc(CACHE_BLOCK);
    if (!c->blk[victim]) {
      return NULL;
    }
  }
  const size_t off = (size_t)b * CACHE_BLOCK;
  const size_t want = c->len - off < CACHE_BLOCK ? c->len - off : CACHE_BLOCK;
#ifdef _WIN32
  const bool ok = _fseeki64(c->f, (long long)off, SEEK_SET) == 0 && fread(c->blk[victim], 1, want, c->f) == want;
#else
  const bool ok = pread(c->fd, c->blk[victim], want, (off_t)off) == (ssize_t)want;
#endif
  if (!ok) {
    c->id[victim] = 0;
    return NULL;
  }
  c->id[victim] = b + 1;
  c->used[victim] = ++c->clock;
  return c->blk[victim];
}

/* n bytes at off (within one call, possibly across blocks); false out of
 * the file */
static bool cache_read(IconCache *c, uint32_t off, void *dst, size_t n)
{
  if ((size_t)off + n > c->len) {
    return false;
  }
  uint8_t *d = (uint8_t *)dst;
  while (n) {
    const uint8_t *blk = cache_block(c, off / CACHE_BLOCK);
    if (!blk) {
      return false;
    }
    const size_t in = off % CACHE_BLOCK, k = CACHE_BLOCK - in < n ? CACHE_BLOCK - in : n;
    memcpy(d, blk + in, k);
    d += k;
    off += (uint32_t)k;
    n -= k;
  }
  return true;
}

static uint32_t rd32(IconCache *c, uint32_t off)
{
  uint8_t p[4];
  if (!cache_read(c, off, p, 4)) {
    return 0xffffffffu;
  }
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static unsigned rd16(IconCache *c, uint32_t off)
{
  uint8_t p[2];
  if (!cache_read(c, off, p, 2)) {
    return 0xffffu;
  }
  return (unsigned)p[0] << 8 | p[1];
}

/* whether the NUL-terminated string at off is `s` */
static bool cache_streq(IconCache *c, uint32_t off, const char *s)
{
  const size_t n = strlen(s) + 1; /* its NUL too */
  for (size_t done = 0; done < n;) {
    if ((size_t)off + done >= c->len) {
      return false;
    }
    const uint8_t *blk = cache_block(c, (uint32_t)((off + done) / CACHE_BLOCK));
    if (!blk) {
      return false;
    }
    const size_t in = (off + done) % CACHE_BLOCK;
    size_t k = CACHE_BLOCK - in < n - done ? CACHE_BLOCK - in : n - done;
    if ((size_t)off + done + k > c->len) {
      k = c->len - off - done;
    }
    if (memcmp(blk + in, s + done, k) != 0) {
      return false;
    }
    done += k;
  }
  return true;
}

/* gtk_icon_cache_new_for_path: only when not older than the directory */
static IconCache *cache_open(const char *dir)
{
  long long dir_mtime = 0, mtime = 0, size = 0;
  if (path_kind(dir, &dir_mtime, NULL) != 1) {
    return NULL;
  }
  char *path = join(dir, "icon-theme.cache");
  if (!path) {
    return NULL;
  }
  IconCache *c = NULL;
  if (path_kind(path, &mtime, &size) == 2 && size >= 12 && mtime >= dir_mtime && size < 0x7fffffff) {
    c = (IconCache *)calloc(1, sizeof(IconCache));
#ifdef _WIN32
    wchar_t *w = c ? wide(path) : NULL;
    c && (c->f = w ? _wfopen(w, L"rb") : NULL);
    free(w);
    if (c && !c->f) {
      free(c);
      c = NULL;
    }
#else
    if (c && (c->fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) {
      free(c);
      c = NULL;
    }
#endif
    if (c) {
      c->len = (size_t)size;
    }
  }
  free(path);
  return c;
}

static void cache_free(IconCache *c)
{
  if (!c) {
    return;
  }
#ifdef _WIN32
  fclose(c->f);
#else
  close(c->fd);
#endif
  for (int i = 0; i < CACHE_SLOTS; i++) {
    free(c->blk[i]);
  }
  free(c);
}

static int cache_dir_index(IconCache *c, const char *dir)
{
  const uint32_t list = rd32(c, 8);
  const uint32_t n = rd32(c, list);
  if (n == 0xffffffffu) {
    return -1;
  }
  for (uint32_t i = 0; i < n && i < 65536; i++) {
    const uint32_t off = rd32(c, list + 4 + 4 * i);
    if (off == 0xffffffffu) {
      return -1;
    }
    if (cache_streq(c, off, dir)) {
      return (int)i;
    }
  }
  return -1;
}

/* the image list of a name, 0 when the cache has none */
static uint32_t cache_images(IconCache *c, const char *name)
{
  const signed char *p = (const signed char *)name;
  uint32_t h = (uint32_t)(int)*p;
  if (h) {
    for (p++; *p; p++) {
      h = (h << 5) - h + (uint32_t)(int)*p;
    }
  }
  const uint32_t hash = rd32(c, 4);
  const uint32_t buckets = rd32(c, hash);
  if (buckets == 0 || buckets == 0xffffffffu) {
    return 0;
  }
  uint32_t chain = rd32(c, hash + 4 + 4 * (h % buckets));
  for (int guard = 0; chain != 0xffffffffu && guard < 100000; guard++) {
    const uint32_t name_off = rd32(c, chain + 4);
    if (name_off == 0xffffffffu) {
      return 0;
    }
    if (cache_streq(c, name_off, name)) {
      return rd32(c, chain + 8);
    }
    chain = rd32(c, chain);
  }
  return 0;
}

static unsigned cache_flags(IconCache *c, uint32_t images, int dir)
{
  if (!images || images == 0xffffffffu) {
    return 0;
  }
  const uint32_t n = rd32(c, images);
  for (uint32_t j = 0; j < n && j < 4096; j++) {
    const unsigned d = rd16(c, images + 4 + 8 * j);
    if (d == 0xffffu) {
      return 0;
    }
    if ((int)d == dir) {
      return rd16(c, images + 4 + 8 * j + 2);
    }
  }
  return 0;
}

/* ======================================================================= */
/* Names of a directory listed (themes without a cache): name -> suffixes */

typedef struct NameTab {
  char **names;
  uint8_t *flags;
  int n, cap; /* cap: a power of two (open addressing) */
} NameTab;

static int suffix_from_name(const char *name)
{
  if (has_suffix(name, ".symbolic.png")) {
    return SUFFIX_SYMBOLIC_PNG;
  }
  if (has_suffix(name, ".png")) {
    return SUFFIX_PNG;
  }
  if (has_suffix(name, ".svg")) {
    return SUFFIX_SVG;
  }
  if (has_suffix(name, ".xpm")) {
    return SUFFIX_XPM;
  }
  return SUFFIX_NONE;
}

static void strip_suffix(char *name, int suffix)
{
  const size_t l = strlen(name);
  if (suffix & SUFFIX_SYMBOLIC_PNG) {
    name[l - strlen(".symbolic.png")] = 0;
  }
  else if (suffix) {
    name[l - 4] = 0;
  }
}

static int nametab_find(const NameTab *t, const char *name)
{
  if (!t || !t->cap) {
    return -1;
  }
  for (uint32_t i = fnv(name, 2166136261u) & (uint32_t)(t->cap - 1);; i = (i + 1) & (uint32_t)(t->cap - 1)) {
    if (!t->names[i]) {
      return -1;
    }
    if (!strcmp(t->names[i], name)) {
      return (int)i;
    }
  }
}

static void nametab_add(NameTab *t, const char *name, int suffix)
{
  const int at = nametab_find(t, name);
  if (at >= 0) {
    t->flags[at] |= (uint8_t)suffix;
    return;
  }
  if ((t->n + 1) * 2 > t->cap) {
    const int cap = t->cap ? t->cap * 2 : 64;
    char **names = (char **)calloc((size_t)cap, sizeof(char *));
    uint8_t *flags = (uint8_t *)calloc((size_t)cap, 1);
    if (!names || !flags) {
      free(names);
      free(flags);
      return;
    }
    for (int i = 0; i < t->cap; i++) {
      if (t->names[i]) {
        uint32_t j = fnv(t->names[i], 2166136261u) & (uint32_t)(cap - 1);
        while (names[j]) {
          j = (j + 1) & (uint32_t)(cap - 1);
        }
        names[j] = t->names[i];
        flags[j] = t->flags[i];
      }
    }
    free(t->names);
    free(t->flags);
    t->names = names;
    t->flags = flags;
    t->cap = cap;
  }
  char *dup = awra_strdup(name);
  if (!dup) {
    return;
  }
  uint32_t j = fnv(name, 2166136261u) & (uint32_t)(t->cap - 1);
  while (t->names[j]) {
    j = (j + 1) & (uint32_t)(t->cap - 1);
  }
  t->names[j] = dup;
  t->flags[j] = (uint8_t)suffix;
  t->n++;
}

static void nametab_free(NameTab *t)
{
  if (!t) {
    return;
  }
  for (int i = 0; i < t->cap; i++) {
    free(t->names[i]);
  }
  free(t->names);
  free(t->flags);
  free(t);
}

static void scan_one(const char *file, void *data)
{
  const int suffix = suffix_from_name(file);
  if (suffix == SUFFIX_NONE) {
    return;
  }
  char buf[512];
  if (strlen(file) >= sizeof(buf)) {
    return;
  }
  strcpy(buf, file);
  strip_suffix(buf, suffix);
  nametab_add((NameTab *)data, buf, suffix);
}

/* ======================================================================= */
/* Themes */

enum { DIR_FIXED, DIR_SCALABLE, DIR_THRESHOLD };

typedef struct DirSize {
  int type, size, min_size, max_size, threshold, scale;
} DirSize;

/* A place themes are looked for (GTK's dir_mtimes): a search path
 * directory with a theme's name, a search path directory itself, or a set
 * of built-in icons. */
typedef struct Root {
  char *path; /* NULL: built-in */
  const AwraBuiltinSet *builtin;
  bool exists;
  long long mtime;
  IconCache *cache;
  bool cache_tried;
} Root;

typedef struct Dir {
  int root;
  char *subdir;
  char *path; /* root/subdir (files) */
  int size_index;
  int cache_dir; /* in the root's cache, -1: listed */
  NameTab *tab;  /* listed names (no cache) */
  bool listed;
} Dir;

typedef struct Theme {
  char *name;
  DirSize *sizes;
  int nsizes;
  Dir *dirs;
  int ndirs;
} Theme;

typedef struct Src {
  char *path; /* or data */
  const unsigned char *data;
  size_t len;
  int suffix;
} Src;

typedef struct Unthemed {
  char *name;
  Src svg, nosvg;
  struct Unthemed *next;
} Unthemed;

typedef struct Registered {
  char *name;
  char *svg;
  size_t len;
  struct Registered *next;
} Registered;

#define UNTHEMED_BUCKETS 256
#define FILE_BUCKETS 256
#define RES_BUCKETS 1024
#define RES_MAX 2048

typedef struct Res {
  char *key;
  uint32_t hash;
  AwraIconFile *file;
  struct Res *next;             /* bucket */
  struct Res *newer, *older;    /* use order */
} Res;

typedef struct ChangedListener {
  unsigned long id;
  void (*fn)(void *data);
  void *data;
  struct ChangedListener *next;
} ChangedListener;

static struct {
  /* configuration */
  char **search_path; /* NULL: the default */
  bool custom_theme;  /* awra_icon_theme_set_theme_name */
  char *current;      /* the theme in use (NULL: none yet) */
  /* loaded */
  bool valid;
  Root *roots;
  int nroots, caproots;
  Theme *themes;
  int nthemes;
  Unthemed *unthemed[UNTHEMED_BUCKETS];
  bool unthemed_loaded;
  int nbase; /* search path directories in roots (after the themes') */
  int base_root;
  double last_stat;
  unsigned serial;
  /* answers */
  AwraIconFile *files[FILE_BUCKETS];
  Res *res[RES_BUCKETS];
  Res *newest, *oldest;
  int nres;
  /* the application's */
  Registered *registered;
  ChangedListener *listeners;
  unsigned long next_listener;
  bool notify_hooked;
} T;

static const AwraBuiltinSet *const k_resource_sets[] = {&awra_builtin_gtk, &awra_builtin_awra};

const AwraBuiltinIcon *awra_builtin_find(const AwraBuiltinSet *set, const char *path)
{
  int lo = 0, hi = set->count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const int c = strcmp(set->icons[mid].path, path);
    if (c == 0) {
      return &set->icons[mid];
    }
    if (c < 0) {
      lo = mid + 1;
    }
    else {
      hi = mid - 1;
    }
  }
  return NULL;
}

/* whether a built-in set has files under "subdir/" */
static bool builtin_has_dir(const AwraBuiltinSet *set, const char *subdir)
{
  const size_t l = strlen(subdir);
  for (int i = 0; i < set->count; i++) {
    if (!strncmp(set->icons[i].path, subdir, l) && set->icons[i].path[l] == '/') {
      return true;
    }
  }
  return false;
}

static char **search_path(void)
{
  if (T.search_path) {
    return T.search_path;
  }
  /* gtk_icon_theme_init */
  char **data = awra_xdg_data_dirs(false);
  Strv s = {0};
  const char *dh = getenv("XDG_DATA_HOME");
  char *home = home_dir();
#ifdef _WIN32
  {
    const char *la = getenv("LOCALAPPDATA");
    char *u = la && la[0] ? join(la, "icons") : NULL;
    strv_add(&s, u);
  }
#else
  if (dh && dh[0]) {
    strv_add(&s, join(dh, "icons"));
  }
  else {
    char *u = join(home, ".local/share");
    strv_add(&s, u ? join(u, "icons") : NULL);
    free(u);
  }
#endif
  strv_add(&s, join(home, ".icons"));
  for (char **p = data; p && *p; p++) {
    strv_add(&s, join(*p, "icons"));
  }
  for (char **p = data; p && *p; p++) {
    strv_add(&s, join(*p, "pixmaps"));
  }
  free(home);
  awra_strv_free(data);
  T.search_path = s.v;
  return T.search_path;
}

static int add_root(const char *path, const AwraBuiltinSet *builtin)
{
  if (T.nroots + 1 > T.caproots) {
    const int cap = T.caproots ? T.caproots * 2 : 32;
    Root *r = (Root *)realloc(T.roots, sizeof(Root) * cap);
    if (!r) {
      return -1;
    }
    T.roots = r;
    T.caproots = cap;
  }
  Root *r = &T.roots[T.nroots];
  memset(r, 0, sizeof(*r));
  r->builtin = builtin;
  if (path) {
    r->path = awra_strdup(path);
    r->exists = path_kind(path, &r->mtime, NULL) == 1;
  }
  else {
    r->exists = true;
  }
  return T.nroots++;
}

static int theme_size_index(Theme *t, const DirSize *ds)
{
  for (int i = 0; i < t->nsizes; i++) {
    const DirSize *o = &t->sizes[i];
    if (o->type == ds->type && o->size == ds->size && o->min_size == ds->min_size &&
        o->max_size == ds->max_size && o->threshold == ds->threshold && o->scale == ds->scale)
    {
      return i;
    }
  }
  DirSize *n = (DirSize *)realloc(t->sizes, sizeof(DirSize) * (t->nsizes + 1));
  if (!n) {
    return -1;
  }
  t->sizes = n;
  t->sizes[t->nsizes] = *ds;
  return t->nsizes++;
}

static void theme_add_dir(Theme *t, int root, const char *subdir, int size_index, int cache_dir)
{
  Dir *d = (Dir *)realloc(t->dirs, sizeof(Dir) * (t->ndirs + 1));
  if (!d) {
    return;
  }
  t->dirs = d;
  Dir *n = &t->dirs[t->ndirs++];
  memset(n, 0, sizeof(*n));
  n->root = root;
  n->subdir = awra_strdup(subdir);
  n->path = T.roots[root].path ? join(T.roots[root].path, subdir) : NULL;
  n->size_index = size_index;
  n->cache_dir = cache_dir;
}

/* theme_subdir_load */
static void theme_subdir_load(Theme *t, Ini *ini, const char *subdir)
{
  DirSize ds;
  if (!ini_int(ini, subdir, "Size", &ds.size)) {
    return; /* "has no size field" */
  }
  ds.type = DIR_THRESHOLD;
  const char *type = ini_get(ini, subdir, "Type");
  if (type) {
    if (!strcmp(type, "Fixed")) {
      ds.type = DIR_FIXED;
    }
    else if (!strcmp(type, "Scalable")) {
      ds.type = DIR_SCALABLE;
    }
    else if (!strcmp(type, "Threshold")) {
      ds.type = DIR_THRESHOLD;
    }
  }
  if (!ini_int(ini, subdir, "MaxSize", &ds.max_size)) {
    ds.max_size = ds.size;
  }
  if (!ini_int(ini, subdir, "MinSize", &ds.min_size)) {
    ds.min_size = ds.size;
  }
  if (!ini_int(ini, subdir, "Threshold", &ds.threshold)) {
    ds.threshold = 2;
  }
  if (!ini_int(ini, subdir, "Scale", &ds.scale)) {
    ds.scale = 1;
  }
  const int si = theme_size_index(t, &ds);
  if (si < 0) {
    return;
  }
  /* every theme place known so far (GTK's dir_mtimes, earlier themes'
   * included) */
  for (int i = 0; i < T.nroots; i++) {
    Root *r = &T.roots[i];
    if (!r->exists) {
      continue;
    }
    if (r->builtin) {
      if (builtin_has_dir(r->builtin, subdir)) {
        theme_add_dir(t, i, subdir, si, -1);
      }
      continue;
    }
    char *full = join(r->path, subdir);
    if (!full) {
      continue;
    }
    if (r->cache || path_kind(full, NULL, NULL) == 1) {
      if (!r->cache && !r->cache_tried) {
        r->cache_tried = true;
        r->cache = cache_open(r->path);
      }
      if (r->cache) {
        const int ci = cache_dir_index(r->cache, subdir);
        if (ci >= 0) {
          theme_add_dir(t, i, subdir, si, ci);
        }
      }
      else {
        theme_add_dir(t, i, subdir, si, -1); /* listed when a lookup needs it */
      }
    }
    free(full);
  }
  /* hicolor: the resource icons in its directories */
  if (!strcmp(t->name, "hicolor")) {
    for (size_t k = 0; k < sizeof(k_resource_sets) / sizeof(k_resource_sets[0]); k++) {
      if (builtin_has_dir(k_resource_sets[k], subdir)) {
        int root = -1;
        for (int i = 0; i < T.nroots; i++) {
          if (T.roots[i].builtin == k_resource_sets[k]) {
            root = i;
          }
        }
        if (root < 0) {
          root = add_root(NULL, k_resource_sets[k]);
          T.roots[root].exists = false; /* not a theme place for later themes */
        }
        if (root >= 0) {
          theme_add_dir(t, root, subdir, si, -1);
        }
      }
    }
  }
}

static void insert_theme(const char *name)
{
  for (int i = 0; i < T.nthemes; i++) {
    if (!strcmp(T.themes[i].name, name)) {
      return;
    }
  }
  char *text = NULL;
  for (char **p = search_path(); *p; p++) {
    char *path = join(*p, name);
    if (!path) {
      continue;
    }
    add_root(path, NULL);
    if (!text && T.roots[T.nroots - 1].exists) {
      char *file = join(path, "index.theme");
      if (file && path_kind(file, NULL, NULL) == 2) {
        text = awra_read_file(file, NULL);
      }
      free(file);
    }
    free(path);
  }
  if (!text) {
    if (!strcmp(name, "hicolor")) {
      text = awra_strdup(awra_builtin_hicolor_index);
    }
    else if (!strcmp(name, "Adwaita")) {
      /* no Adwaita installed: the built-in one (GTK's Windows builds ship it) */
      text = awra_strdup(awra_builtin_adwaita_index);
      add_root(NULL, &awra_builtin_adwaita);
    }
    if (!text) {
      return;
    }
  }
  Ini ini;
  ini_parse(&ini, text);
  Strv dirs = {0}, scaled = {0}, inherits = {0};
  ini_list(&ini, "Icon Theme", "Directories", &dirs);
  if (!dirs.n) {
    ini_free(&ini);
    return; /* "has no directories" */
  }
  ini_list(&ini, "Icon Theme", "ScaledDirectories", &scaled);
  Theme *nt = (Theme *)realloc(T.themes, sizeof(Theme) * (T.nthemes + 1));
  if (!nt) {
    awra_strv_free(dirs.v);
    awra_strv_free(scaled.v);
    ini_free(&ini);
    return;
  }
  T.themes = nt;
  const int ti = T.nthemes++;
  memset(&T.themes[ti], 0, sizeof(Theme));
  T.themes[ti].name = awra_strdup(name);
  for (int i = 0; i < dirs.n; i++) {
    theme_subdir_load(&T.themes[ti], &ini, dirs.v[i]);
  }
  for (int i = 0; i < scaled.n; i++) {
    theme_subdir_load(&T.themes[ti], &ini, scaled.v[i]);
  }
  awra_strv_free(dirs.v);
  awra_strv_free(scaled.v);
  ini_list(&ini, "Icon Theme", "Inherits", &inherits);
  ini_free(&ini);
  for (int i = 0; i < inherits.n; i++) {
    insert_theme(inherits.v[i]);
  }
  awra_strv_free(inherits.v);
}

static void free_src(Src *s)
{
  free(s->path);
}

static void free_unthemed(void)
{
  for (int b = 0; b < UNTHEMED_BUCKETS; b++) {
    while (T.unthemed[b]) {
      Unthemed *u = T.unthemed[b];
      T.unthemed[b] = u->next;
      free(u->name);
      free_src(&u->svg);
      free_src(&u->nosvg);
      free(u);
    }
  }
  T.unthemed_loaded = false;
}

static Unthemed *unthemed_find(const char *name)
{
  for (Unthemed *u = T.unthemed[fnv(name, 2166136261u) % UNTHEMED_BUCKETS]; u; u = u->next) {
    if (!strcmp(u->name, name)) {
      return u;
    }
  }
  return NULL;
}

/* add_unthemed_icon */
static void add_unthemed(const char *file, const char *path, const unsigned char *data, size_t len)
{
  const int suffix = suffix_from_name(file);
  if (suffix == SUFFIX_NONE) {
    return;
  }
  char base[512];
  if (strlen(file) >= sizeof(base)) {
    return;
  }
  strcpy(base, file);
  strip_suffix(base, suffix);
  Src s = {path ? awra_strdup(path) : NULL, data, len, suffix};
  Unthemed *u = unthemed_find(base);
  if (!u) {
    u = (Unthemed *)calloc(1, sizeof(Unthemed));
    if (!u) {
      free_src(&s);
      return;
    }
    u->name = awra_strdup(base);
    const uint32_t b = fnv(base, 2166136261u) % UNTHEMED_BUCKETS;
    u->next = T.unthemed[b];
    T.unthemed[b] = u;
  }
  if (suffix == SUFFIX_SVG) {
    if (u->svg.suffix) {
      free_src(&s);
    }
    else {
      u->svg = s;
    }
  }
  else if (u->nosvg.suffix) {
    if (suffix > u->nosvg.suffix) {
      free_src(&u->nosvg);
      u->nosvg = s;
    }
    else {
      free_src(&s);
    }
  }
  else {
    u->nosvg = s;
  }
}

typedef struct ScanCtx {
  const char *dir;
} ScanCtx;

static void unthemed_one(const char *file, void *data)
{
  const ScanCtx *c = (const ScanCtx *)data;
  if (suffix_from_name(file) == SUFFIX_NONE) {
    return;
  }
  char *path = join(c->dir, file);
  if (path) {
    add_unthemed(file, path, NULL, 0);
    free(path);
  }
}

/* the unthemed icons, the first time a lookup gets to them */
static void load_unthemed(void)
{
  if (T.unthemed_loaded) {
    return;
  }
  T.unthemed_loaded = true;
  for (int i = 0; i < T.nbase; i++) {
    Root *r = &T.roots[T.base_root + i];
    if (!r->exists) {
      continue;
    }
    if (!r->cache_tried) {
      r->cache_tried = true;
      r->cache = cache_open(r->path);
    }
    if (r->cache) {
      continue; /* as GTK: a cache there, no unthemed icons from it */
    }
    ScanCtx c = {r->path};
    awra_list_dir(r->path, unthemed_one, &c);
  }
  /* the resource paths: GTK's, Awra's, then the application's */
  for (size_t k = 0; k < sizeof(k_resource_sets) / sizeof(k_resource_sets[0]); k++) {
    const AwraBuiltinSet *set = k_resource_sets[k];
    for (int i = 0; i < set->count; i++) {
      if (!strchr(set->icons[i].path, '/')) {
        add_unthemed(set->icons[i].path, NULL, set->icons[i].data, set->icons[i].len);
      }
    }
  }
  for (Registered *r = T.registered; r; r = r->next) {
    char file[300];
    if (strlen(r->name) + 5 < sizeof(file)) {
      snprintf(file, sizeof(file), "%s.svg", r->name);
      add_unthemed(file, NULL, (const unsigned char *)r->svg, r->len);
    }
  }
}

static void load_themes(void)
{
  if (T.current) {
    insert_theme(T.current);
  }
  insert_theme("hicolor");
  T.base_root = T.nroots;
  T.nbase = 0;
  for (char **p = search_path(); *p; p++) {
    add_root(*p, NULL);
    T.nbase++;
  }
  T.valid = true;
  T.last_stat = awra_now_ms();
}

static void blow_themes(void)
{
  for (int i = 0; i < T.nthemes; i++) {
    Theme *t = &T.themes[i];
    for (int k = 0; k < t->ndirs; k++) {
      free(t->dirs[k].subdir);
      free(t->dirs[k].path);
      nametab_free(t->dirs[k].tab);
    }
    free(t->dirs);
    free(t->sizes);
    free(t->name);
  }
  free(T.themes);
  T.themes = NULL;
  T.nthemes = 0;
  for (int i = 0; i < T.nroots; i++) {
    free(T.roots[i].path);
    cache_free(T.roots[i].cache);
  }
  free(T.roots);
  T.roots = NULL;
  T.nroots = T.caproots = 0;
  T.nbase = 0;
  free_unthemed();
  T.valid = false;
  T.serial++;
}

/* rescan_themes: a theme place changed since it was read */
static bool rescan(void)
{
  for (int i = 0; i < T.nroots; i++) {
    const Root *r = &T.roots[i];
    if (!r->path) {
      continue;
    }
    long long mtime = 0;
    const bool is_dir = path_kind(r->path, &mtime, NULL) == 1;
    if (is_dir && r->exists && mtime == r->mtime) {
      continue;
    }
    if (!r->exists && !is_dir) {
      continue;
    }
    return true;
  }
  T.last_stat = awra_now_ms();
  return false;
}

/* ======================================================================= */
/* The answers */

static void res_unlink(Res *r)
{
  if (r->newer) {
    r->newer->older = r->older;
  }
  else {
    T.newest = r->older;
  }
  if (r->older) {
    r->older->newer = r->newer;
  }
  else {
    T.oldest = r->newer;
  }
  r->newer = r->older = NULL;
}

static void res_push(Res *r)
{
  r->older = T.newest;
  r->newer = NULL;
  if (T.newest) {
    T.newest->newer = r;
  }
  T.newest = r;
  if (!T.oldest) {
    T.oldest = r;
  }
}

static void file_unref(AwraIconFile *f)
{
  if (!f || --f->refs > 0) {
    return;
  }
  AwraIconFile **p = &T.files[(f->path ? fnv(f->path, 2166136261u) : (uint32_t)((uintptr_t)f->data >> 4)) %
                              FILE_BUCKETS];
  for (; *p; p = &(*p)->next) {
    if (*p == f) {
      *p = f->next;
      break;
    }
  }
  awra_icon_file_free_rasters(f);
  free(f->path);
  free(f->icon_name);
  free(f);
}

static void res_free(Res *r)
{
  Res **p = &T.res[r->hash % RES_BUCKETS];
  for (; *p; p = &(*p)->next) {
    if (*p == r) {
      *p = r->next;
      break;
    }
  }
  res_unlink(r);
  file_unref(r->file);
  free(r->key);
  free(r);
  T.nres--;
}

static void res_clear(void)
{
  while (T.oldest) {
    res_free(T.oldest);
  }
  awra_icon_rasters_clear();
}

static bool uri_is_symbolic(const char *file)
{
  return has_suffix(file, "-symbolic.svg") || has_suffix(file, ".symbolic.png") ||
         has_suffix(file, "-symbolic-ltr.svg") || has_suffix(file, "-symbolic-rtl.svg");
}

/* the shared record of a file (a path, or built-in / registered bytes
 * named `file`) */
static AwraIconFile *file_get(const char *path, const unsigned char *data, size_t len,
                              const char *file, const char *icon_name)
{
  const uint32_t b = (path ? fnv(path, 2166136261u) : (uint32_t)((uintptr_t)data >> 4)) % FILE_BUCKETS;
  for (AwraIconFile *f = T.files[b]; f; f = f->next) {
    if (path ? (f->path && !strcmp(f->path, path)) : (!f->path && f->data == data)) {
      f->refs++;
      return f;
    }
  }
  AwraIconFile *f = (AwraIconFile *)calloc(1, sizeof(AwraIconFile));
  if (!f) {
    return NULL;
  }
  f->path = path ? awra_strdup(path) : NULL;
  f->data = data;
  f->len = len;
  f->icon_name = icon_name ? awra_strdup(icon_name) : NULL;
  f->is_svg = has_suffix(file, ".svg");
  f->is_png = has_suffix(file, ".png");
  f->is_symbolic = uri_is_symbolic(file);
  f->refs = 1;
  f->next = T.files[b];
  T.files[b] = f;
  return f;
}

/* ======================================================================= */
/* Lookup */

static void ensure_valid(void);

static bool is_symbolic_name(const char *n)
{
  return has_suffix(n, "-symbolic") || has_suffix(n, "-symbolic-ltr") || has_suffix(n, "-symbolic-rtl");
}

static int theme_dir_size_difference(const DirSize *d, int size, int scale)
{
  const int scaled_size = size * scale;
  const int scaled_dir_size = d->size * d->scale;
  switch (d->type) {
    case DIR_FIXED:
      return abs(scaled_size - scaled_dir_size);
    case DIR_SCALABLE:
      if (scaled_size < d->min_size * d->scale) {
        return d->min_size * d->scale - scaled_size;
      }
      if (size > d->max_size * d->scale) { /* sic: GTK compares the unscaled size here */
        return scaled_size - d->max_size * d->scale;
      }
      return 0;
    default: {
      const int min = (d->size - d->threshold) * d->scale;
      const int max = (d->size + d->threshold) * d->scale;
      if (scaled_size < min) {
        return min - scaled_size;
      }
      if (scaled_size > max) {
        return scaled_size - max;
      }
      return 0;
    }
  }
}

/* compare_dir_size_matches: a is a better match than b */
static bool better(const DirSize *a, int da, const DirSize *b, int db, int size, int scale)
{
  if (da == 0) {
    if (db != 0) {
      return true;
    }
  }
  else {
    if (a->size >= size && b->size < size) {
      return true;
    }
    if (a->size < size && b->size >= size) {
      return false;
    }
    if (da < db) {
      return true;
    }
    if (da > db) {
      return false;
    }
  }
  if (a->scale == scale && b->scale != scale) {
    return true;
  }
  if (a->scale != scale && b->scale == scale) {
    return false;
  }
  if (a->type != DIR_SCALABLE && b->type == DIR_SCALABLE) {
    return true;
  }
  if (a->type == DIR_SCALABLE && b->type != DIR_SCALABLE) {
    return false;
  }
  const int ea = abs(size * scale - a->size * a->scale);
  const int eb = abs(size * scale - b->size * b->scale);
  return ea <= eb;
}

static int best_suffix(int s)
{
  if (s & SUFFIX_SYMBOLIC_PNG) {
    return SUFFIX_SYMBOLIC_PNG;
  }
  if (s & SUFFIX_PNG) {
    return SUFFIX_PNG;
  }
  if (s & SUFFIX_SVG) {
    return SUFFIX_SVG;
  }
  if (s & SUFFIX_XPM) {
    return SUFFIX_XPM;
  }
  return SUFFIX_NONE;
}

static const char *suffix_string(int s)
{
  switch (s) {
    case SUFFIX_XPM:
      return ".xpm";
    case SUFFIX_SVG:
      return ".svg";
    case SUFFIX_PNG:
      return ".png";
    default:
      return ".symbolic.png";
  }
}

/* the suffixes a theme directory has `name` with */
static int dir_suffixes(Dir *d, const char *name, uint32_t *memo_images, int *memo_root)
{
  Root *r = &T.roots[d->root];
  if (r->builtin) {
    int s = 0;
    char path[600];
    static const char *const exts[] = {".svg", ".png", ".symbolic.png", ".xpm"};
    for (int i = 0; i < 4; i++) {
      snprintf(path, sizeof(path), "%s/%s%s", d->subdir, name, exts[i]);
      if (awra_builtin_find(r->builtin, path)) {
        s |= suffix_from_name(path);
      }
    }
    return s;
  }
  if (d->cache_dir >= 0 && r->cache) {
    /* memo: the root's image lists of name and "name.symbolic" */
    if (*memo_root != d->root) {
      *memo_root = d->root;
      memo_images[0] = cache_images(r->cache, name);
      char sym[600];
      snprintf(sym, sizeof(sym), "%s.symbolic", name);
      memo_images[1] = cache_images(r->cache, sym);
    }
    /* "name.symbolic" with a PNG is name.symbolic.png (gtkiconcache.c) */
    int s = (int)cache_flags(r->cache, memo_images[0], d->cache_dir);
    const int sp = (int)cache_flags(r->cache, memo_images[1], d->cache_dir);
    if (sp & SUFFIX_PNG) {
      s |= (sp | SUFFIX_SYMBOLIC_PNG) & ~SUFFIX_PNG;
    }
    return s;
  }
  if (!d->listed) {
    d->listed = true;
    d->tab = (NameTab *)calloc(1, sizeof(NameTab));
    if (d->tab && d->path) {
      awra_list_dir(d->path, scan_one, d->tab);
    }
  }
  const int at = nametab_find(d->tab, name);
  return at >= 0 ? d->tab->flags[at] : 0;
}

/* theme_lookup_icon */
static AwraIconFile *theme_lookup(Theme *t, const char *name, int size, int scale)
{
  int best_dir = -1, best_sfx = 0, best_diff = 0;
  /* the first directory of each size that has it */
  int first[64];
  int *firsts = t->nsizes <= 64 ? first : (int *)malloc(sizeof(int) * t->nsizes);
  int *sfx = (int *)malloc(sizeof(int) * (t->nsizes ? t->nsizes : 1));
  if (!firsts || !sfx) {
    if (firsts != first) {
      free(firsts);
    }
    free(sfx);
    return NULL;
  }
  for (int i = 0; i < t->nsizes; i++) {
    firsts[i] = -1;
  }
  uint32_t memo[2] = {0, 0};
  int memo_root = -1;
  for (int k = 0; k < t->ndirs; k++) {
    Dir *d = &t->dirs[k];
    if (firsts[d->size_index] >= 0) {
      continue;
    }
    const int s = dir_suffixes(d, name, memo, &memo_root);
    if (s) {
      firsts[d->size_index] = k;
      sfx[d->size_index] = best_suffix(s);
    }
  }
  for (int i = 0; i < t->nsizes; i++) {
    if (firsts[i] < 0 || sfx[i] == SUFFIX_NONE) {
      continue;
    }
    const int diff = theme_dir_size_difference(&t->sizes[i], size, scale);
    if (best_dir < 0 ||
        better(&t->sizes[i], diff, &t->sizes[t->dirs[best_dir].size_index], best_diff, size, scale))
    {
      best_dir = firsts[i];
      best_sfx = sfx[i];
      best_diff = diff;
    }
  }
  if (firsts != first) {
    free(firsts);
  }
  free(sfx);
  if (best_dir < 0) {
    return NULL;
  }
  Dir *d = &t->dirs[best_dir];
  char file[600];
  snprintf(file, sizeof(file), "%s%s", name, suffix_string(best_sfx));
  Root *r = &T.roots[d->root];
  if (r->builtin) {
    char path[700];
    snprintf(path, sizeof(path), "%s/%s", d->subdir, file);
    const AwraBuiltinIcon *b = awra_builtin_find(r->builtin, path);
    return b ? file_get(NULL, b->data, b->len, file, name) : NULL;
  }
  char *path = join(d->path, file);
  AwraIconFile *f = path ? file_get(path, NULL, 0, file, name) : NULL;
  free(path);
  return f;
}

/* real_choose_icon */
static AwraIconFile *real_choose(const char *const *names, int n, int size, int scale, int depth)
{
  ensure_valid();
  for (int t = 0; t < T.nthemes; t++) {
    for (int i = 0; i < n && is_symbolic_name(names[i]); i++) {
      AwraIconFile *f = theme_lookup(&T.themes[t], names[i], size, scale);
      if (f) {
        return f;
      }
    }
  }
  for (int t = 0; t < T.nthemes; t++) {
    for (int i = 0; i < n; i++) {
      AwraIconFile *f = theme_lookup(&T.themes[t], names[i], size, scale);
      if (f) {
        return f;
      }
    }
  }
  load_unthemed();
  for (int i = 0; i < n; i++) {
    Unthemed *u = unthemed_find(names[i]);
    if (!u) {
      continue;
    }
    /* an SVG beats an XPM, not a PNG */
    const Src *s = u->svg.suffix && (!u->nosvg.suffix || u->nosvg.suffix < SUFFIX_PNG) ? &u->svg : &u->nosvg;
    if (!s->suffix) {
      continue;
    }
    char file[600];
    snprintf(file, sizeof(file), "%s%s", u->name, suffix_string(s->suffix));
    return file_get(s->path, s->data, s->len, s->path ? s->path : file, names[i]);
  }
  if (depth > 0) {
    /* no image-missing anywhere (a search path without hicolor): GTK's */
    const AwraBuiltinIcon *b = awra_builtin_find(&awra_builtin_gtk, "16x16/status/image-missing.png");
    return b ? file_get(NULL, b->data, b->len, "image-missing.png", "image-missing") : NULL;
  }
  static const char *const missing[] = {"image-missing"};
  return real_choose(missing, 1, size, scale, depth + 1);
}

/* choose_icon: the direction's names, regular / symbolic forced */
static AwraIconFile *choose(const char *const *names, int n, int size, int scale, int dir,
                            unsigned flags)
{
  const char *suffix = dir == 1 ? "-ltr" : dir == 2 ? "-rtl" : NULL;
  bool has_regular = false, has_symbolic = false;
  for (int i = 0; i < n; i++) {
    if (is_symbolic_name(names[i])) {
      has_symbolic = true;
    }
    else {
      has_regular = true;
    }
  }
  Strv out = {0};
#define ADD(str)                                                                                   \
  do {                                                                                             \
    char *s_ = (str);                                                                              \
    if (s_ && suffix) {                                                                            \
      char *d_ = (char *)malloc(strlen(s_) + 5);                                                   \
      if (d_) {                                                                                    \
        strcpy(d_, s_);                                                                            \
        strcat(d_, suffix);                                                                        \
        strv_add(&out, d_);                                                                        \
      }                                                                                            \
    }                                                                                              \
    strv_add(&out, s_);                                                                            \
  } while (0)
  if ((flags & AWRA_ICON_FLAG_FORCE_REGULAR) && has_symbolic) {
    for (int i = 0; i < n; i++) {
      if (is_symbolic_name(names[i])) {
        char *s = awra_strdup(names[i]);
        if (s && strlen(s) >= 9) {
          s[strlen(s) - 9] = 0; /* sic: GTK strips 9 bytes whatever the suffix */
        }
        ADD(s);
      }
      else {
        ADD(awra_strdup(names[i]));
      }
    }
    for (int i = 0; i < n; i++) {
      if (is_symbolic_name(names[i])) {
        ADD(awra_strdup(names[i]));
      }
    }
  }
  else if ((flags & AWRA_ICON_FLAG_FORCE_SYMBOLIC) && has_regular) {
    for (int i = 0; i < n; i++) {
      if (!is_symbolic_name(names[i])) {
        char *s = (char *)malloc(strlen(names[i]) + 10);
        if (s) {
          strcpy(s, names[i]);
          strcat(s, "-symbolic");
        }
        ADD(s);
      }
      else {
        ADD(awra_strdup(names[i]));
      }
    }
    for (int i = 0; i < n; i++) {
      if (!is_symbolic_name(names[i])) {
        ADD(awra_strdup(names[i]));
      }
    }
  }
  else if (suffix) {
    for (int i = 0; i < n; i++) {
      ADD(awra_strdup(names[i]));
    }
  }
  else {
    return real_choose(names, n, size, scale, 0);
  }
#undef ADD
  AwraIconFile *f = real_choose((const char *const *)out.v, out.n, size, scale, 0);
  awra_strv_free(out.v);
  return f;
}

/* ======================================================================= */
/* The theme in use */

static void settings_ini(const char *path, char **io)
{
  char *text = awra_read_file(path, NULL);
  if (!text) {
    return;
  }
  Ini ini;
  ini_parse(&ini, text);
  const char *v = ini_get(&ini, "Settings", "gtk-icon-theme-name");
  if (v) {
    free(*io);
    *io = awra_strdup(v);
  }
  ini_free(&ini);
}

/* gtk-icon-theme-name as GtkSettings has it */
static char *settings_theme_name(void)
{
  char *name = NULL;
  /* settings.ini, lowest priority first (gtk_settings_init) */
#ifdef _WIN32
  char *exe = exe_dir();
  if (exe) {
    char *p = join(exe, "../etc/gtk-4.0/settings.ini");
    if (p) {
      settings_ini(p, &name);
    }
    free(p);
    free(exe);
  }
  const char *la = getenv("LOCALAPPDATA");
  if (la && la[0]) {
    char *p = join(la, "gtk-4.0/settings.ini");
    if (p) {
      settings_ini(p, &name);
    }
    free(p);
  }
#else
  settings_ini("/etc/gtk-4.0/settings.ini", &name);
  const char *cd = getenv("XDG_CONFIG_DIRS");
  if (!cd || !cd[0]) {
    cd = "/etc/xdg";
  }
  /* g_get_system_config_dirs order, each later one overriding: GTK loads
   * them first to last */
  for (const char *p = cd; *p;) {
    const char *e = strchr(p, ':');
    const size_t len = e ? (size_t)(e - p) : strlen(p);
    if (len > 0 && len < 900) {
      char path[1000];
      snprintf(path, sizeof(path), "%.*s/gtk-4.0/settings.ini", (int)len, p);
      settings_ini(path, &name);
    }
    p += len + (e ? 1 : 0);
  }
  const char *ch = getenv("XDG_CONFIG_HOME");
  char *user = NULL;
  if (ch && ch[0]) {
    user = join(ch, "gtk-4.0/settings.ini");
  }
  else {
    char *h = home_dir();
    user = join(h, ".config/gtk-4.0/settings.ini");
    free(h);
  }
  if (user) {
    settings_ini(user, &name);
  }
  free(user);
  /* the desktop's own setting overrides the files */
  const char *backend = awra_backend();
  if (backend && !strcmp(backend, "wayland")) {
    const char *p = awra_portal_icon_theme();
    if (p) {
      free(name);
      name = awra_strdup(p);
    }
  }
  else if (backend && !strcmp(backend, "x11")) {
    char v[256];
    if (BK_xsettings_string("Net/IconThemeName", v, sizeof(v))) {
      free(name);
      name = awra_strdup(v);
    }
  }
#endif
  return name ? name : awra_strdup("Adwaita");
}

static void on_kit_settings(void);

static void notify_changed(void)
{
  awra_windows_icons_changed();
  for (ChangedListener *l = T.listeners; l;) {
    ChangedListener *next = l->next; /* a handler may disconnect itself */
    l->fn(l->data);
    l = next;
  }
}

/* do_theme_change */
static void theme_change(bool notify)
{
  res_clear();
  if (T.valid) {
    blow_themes();
  }
  else {
    T.serial++;
  }
  if (notify) {
    notify_changed();
  }
}

static void update_current(bool notify)
{
  if (T.custom_theme) {
    return;
  }
  char *name = settings_theme_name();
  if (!T.current || !name || strcmp(T.current, name) != 0) {
    free(T.current);
    T.current = name;
    theme_change(notify && T.valid);
  }
  else {
    free(name);
  }
}

static void ensure_valid(void)
{
  if (!T.notify_hooked) {
    T.notify_hooked = true;
    BK_set_settings_notify(on_kit_settings);
    awra_portal_start(); /* Wayland's settings, before the theme is chosen */
    update_current(false);
  }
  if (T.valid && awra_now_ms() - T.last_stat > 5000.0) {
    if (rescan()) {
      /* a theme changed on disk: read again, the windows redrawn */
      res_clear();
      blow_themes();
      load_themes();
      notify_changed();
      return;
    }
  }
  if (!T.valid) {
    load_themes();
  }
}

static void on_kit_settings(void)
{
  update_current(true);
}

/* portal.c: the portal's icon-theme changed (or answered late) */
void awra_icon_theme_settings_changed(void);
void awra_icon_theme_settings_changed(void)
{
  if (T.notify_hooked) {
    update_current(true);
  }
}

unsigned awra_icon_theme_serial(void)
{
  return T.serial;
}

/* ======================================================================= */
/* GThemedIcon */

char *awra_themed_icon_expand(const char *const *init, int n, bool fallbacks)
{
  Strv names = {0}, variants = {0};
  for (int i = 0; i < n; i++) {
    if (!init[i] || !init[i][0]) {
      continue;
    }
    const bool symbolic = has_suffix(init[i], "-symbolic");
    char *name = awra_strdup(init[i]);
    if (!name) {
      continue;
    }
    if (symbolic) {
      name[strlen(name) - 9] = 0;
    }
    if (strv_has(&names, name)) {
      free(name);
      continue;
    }
    strv_add(&names, awra_strdup(symbolic ? init[i] : name));
    if (fallbacks) {
      char *last = awra_strdup(name);
      char *dash;
      while (last && (dash = strrchr(last, '-')) != NULL) {
        *dash = 0;
        char *fb;
        if (symbolic) {
          fb = (char *)malloc(strlen(last) + 10);
          if (fb) {
            strcpy(fb, last);
            strcat(fb, "-symbolic");
          }
        }
        else {
          fb = awra_strdup(last);
        }
        if (!fb) {
          break;
        }
        if (strv_has(&names, fb)) {
          free(fb);
          break;
        }
        strv_add(&names, fb);
      }
      free(last);
    }
    free(name);
  }
  for (int i = 0; i < names.n; i++) {
    const char *nm = names.v[i];
    char *variant;
    if (has_suffix(nm, "-symbolic")) {
      variant = awra_strdup(nm);
      if (variant) {
        variant[strlen(variant) - 9] = 0;
      }
    }
    else {
      variant = (char *)malloc(strlen(nm) + 10);
      if (variant) {
        strcpy(variant, nm);
        strcat(variant, "-symbolic");
      }
    }
    if (!variant) {
      continue;
    }
    if (strv_has(&names, variant) || strv_has(&variants, variant)) {
      free(variant);
      continue;
    }
    strv_add(&variants, variant);
  }
  /* the names, then their variants in the same order (GLib builds both
   * lists backwards) */
  size_t total = 1;
  for (int i = 0; i < names.n; i++) {
    total += strlen(names.v[i]) + 1;
  }
  for (int i = 0; i < variants.n; i++) {
    total += strlen(variants.v[i]) + 1;
  }
  char *out = (char *)malloc(total);
  if (out) {
    char *p = out;
    for (int i = 0; i < names.n; i++) {
      p += sprintf(p, "%s%s", p == out ? "" : "\n", names.v[i]);
    }
    for (int i = 0; i < variants.n; i++) {
      p += sprintf(p, "%s%s", p == out ? "" : "\n", variants.v[i]);
    }
    *p = 0;
  }
  awra_strv_free(names.v);
  awra_strv_free(variants.v);
  return out;
}

/* ======================================================================= */
/* Choosing, cached */

static bool is_path(const char *spec)
{
#ifdef _WIN32
  if (spec[0] && spec[1] == ':') {
    return true;
  }
#endif
  return spec[0] == '/' || spec[0] == '\\' || !strncmp(spec, "./", 2) || !strncmp(spec, "file://", 7);
}

static AwraIconFile *lookup_spec(const char *spec, int size, int scale, int dir, unsigned flags)
{
  /* GTK's and Awra's resource files by their URI (CSS url(), as Awra 1's
   * check marks are) */
  static const struct {
    const char *prefix;
    const AwraBuiltinSet *set;
  } res[] = {{"resource:///org/gtk/libgtk/icons/", &awra_builtin_gtk},
             {"resource:///org/awra/icons/", &awra_builtin_awra}};
  for (size_t i = 0; i < sizeof(res) / sizeof(res[0]); i++) {
    const size_t l = strlen(res[i].prefix);
    if (!strncmp(spec, res[i].prefix, l)) {
      const AwraBuiltinIcon *b = awra_builtin_find(res[i].set, spec + l);
      return b ? file_get(NULL, b->data, b->len, spec + l, NULL) : NULL;
    }
  }
  if (is_path(spec)) {
    /* a file (gtk_icon_paintable_new_for_file) */
    const char *path = !strncmp(spec, "file://", 7) ? spec + 7 : spec;
    const char *base = strrchr(path, '/');
    return file_get(path, NULL, 0, base ? base + 1 : path, NULL);
  }
  /* the names of a GThemedIcon of the given ones */
  const char *init[32];
  char buf[2048];
  int n = 0;
  if (strlen(spec) >= sizeof(buf)) {
    return NULL;
  }
  strcpy(buf, spec);
  for (char *p = buf; p && n < 32;) {
    char *nl = strchr(p, '\n');
    if (nl) {
      *nl = 0;
    }
    if (*p) {
      init[n++] = p;
    }
    p = nl ? nl + 1 : NULL;
  }
  if (n == 0) {
    return NULL;
  }
  char *expanded = awra_themed_icon_expand(init, n, false);
  if (!expanded) {
    return NULL;
  }
  const char *names[64];
  int m = 0;
  for (char *p = expanded; p && m < 64;) {
    char *nl = strchr(p, '\n');
    if (nl) {
      *nl = 0;
    }
    names[m++] = p;
    p = nl ? nl + 1 : NULL;
  }
  AwraIconFile *f = choose(names, m, size, scale, dir, flags);
  free(expanded);
  return f;
}

AwraIconFile *awra_icon_theme_choose(const char *spec, int size, int scale, int dir, unsigned flags)
{
  if (!spec || !spec[0] || size <= 0) {
    return NULL;
  }
  scale = scale < 1 ? 1 : scale;
  char tail[48];
  snprintf(tail, sizeof(tail), "\x1f%d\x1f%d\x1f%d\x1f%u", size, scale, dir, flags);
  const uint32_t h = fnv(tail, fnv(spec, 2166136261u));
  const size_t ls = strlen(spec), lt = strlen(tail);
  for (Res *r = T.res[h % RES_BUCKETS]; r; r = r->next) {
    if (r->hash == h && !strncmp(r->key, spec, ls) && !strcmp(r->key + ls, tail)) {
      if (T.newest != r) {
        res_unlink(r);
        res_push(r);
      }
      return r->file;
    }
  }
  AwraIconFile *f = lookup_spec(spec, size, scale, dir, flags);
  if (!f) {
    return NULL;
  }
  Res *r = (Res *)calloc(1, sizeof(Res));
  char *key = r ? (char *)malloc(ls + lt + 1) : NULL;
  if (!r || !key) {
    free(r);
    file_unref(f); /* out of memory: nothing drawn */
    return NULL;
  }
  memcpy(key, spec, ls);
  memcpy(key + ls, tail, lt + 1);
  r->key = key;
  r->hash = h;
  r->file = f;
  r->next = T.res[h % RES_BUCKETS];
  T.res[h % RES_BUCKETS] = r;
  res_push(r);
  T.nres++;
  while (T.nres > RES_MAX && T.oldest && T.oldest != r) {
    res_free(T.oldest);
  }
  return f;
}

/* ======================================================================= */
/* Public API */

char *awra_icon_theme_get_theme_name(void)
{
  if (T.custom_theme) {
    return awra_strdup(T.current);
  }
  if (!T.notify_hooked) {
    ensure_valid();
  }
  return T.current ? awra_strdup(T.current) : settings_theme_name();
}

void awra_icon_theme_set_theme_name(const char *name)
{
  if (name) {
    const bool was_custom = T.custom_theme;
    T.custom_theme = true;
    if (!was_custom || !T.current || strcmp(T.current, name) != 0) {
      free(T.current);
      T.current = awra_strdup(name);
      theme_change(true);
    }
  }
  else if (T.custom_theme) {
    T.custom_theme = false;
    update_current(true);
  }
}

char *awra_icon_theme_get_search_path(void)
{
  char **sp = search_path();
  size_t total = 1;
  for (char **p = sp; p && *p; p++) {
    total += strlen(*p) + 1;
  }
  char *out = (char *)malloc(total);
  if (out) {
    char *w = out;
    for (char **p = sp; p && *p; p++) {
      w += sprintf(w, "%s%s", w == out ? "" : "\n", *p);
    }
    *w = 0;
  }
  return out;
}

void awra_icon_theme_set_search_path(const char *const *paths, int n)
{
  Strv s = {0};
  for (int i = 0; paths && i < n; i++) {
    if (paths[i]) {
      strv_add(&s, awra_strdup(paths[i]));
    }
  }
  if (!s.v) {
    s.v = (char **)calloc(1, sizeof(char *));
  }
  awra_strv_free(T.search_path);
  T.search_path = s.v;
  theme_change(true);
}

void awra_icon_theme_add_search_path(const char *path)
{
  if (!path) {
    return;
  }
  char **sp = search_path();
  Strv s = {0};
  for (char **p = sp; p && *p; p++) {
    strv_add(&s, awra_strdup(*p));
  }
  strv_add(&s, awra_strdup(path));
  awra_strv_free(T.search_path);
  T.search_path = s.v;
  theme_change(true);
}

bool awra_icon_theme_has_icon(const char *name)
{
  if (!name || !name[0]) {
    return false;
  }
  ensure_valid();
  for (int t = 0; t < T.nthemes; t++) {
    Theme *th = &T.themes[t];
    uint32_t memo[2] = {0, 0};
    int memo_root = -1;
    for (int k = 0; k < th->ndirs; k++) {
      if (dir_suffixes(&th->dirs[k], name, memo, &memo_root)) {
        return true;
      }
    }
  }
  load_unthemed();
  return unthemed_find(name) != NULL;
}

bool awra_icon_theme_has_names(const char *const *names, int n)
{
  for (int i = 0; names && i < n; i++) {
    if (awra_icon_theme_has_icon(names[i])) {
      return true;
    }
  }
  return false;
}

unsigned long awra_icon_theme_connect_changed(void (*fn)(void *data), void *data)
{
  if (!fn) {
    return 0;
  }
  ChangedListener *l = (ChangedListener *)calloc(1, sizeof(ChangedListener));
  if (!l) {
    return 0;
  }
  l->id = ++T.next_listener;
  l->fn = fn;
  l->data = data;
  l->next = T.listeners;
  T.listeners = l;
  return l->id;
}

void awra_icon_theme_disconnect(unsigned long id)
{
  for (ChangedListener **p = &T.listeners; *p; p = &(*p)->next) {
    if ((*p)->id == id) {
      ChangedListener *l = *p;
      *p = l->next;
      free(l);
      return;
    }
  }
}

/* Registered SVGs: resource icons of the application (unthemed) */
bool awra_icon_register_svg(const char *name, const char *svg, size_t length)
{
  if (!name || !name[0] || !svg) {
    return false;
  }
  char *copy = (char *)malloc(length + 1);
  if (!copy) {
    return false;
  }
  memcpy(copy, svg, length);
  copy[length] = 0;
  Registered *r = T.registered;
  while (r && strcmp(r->name, name) != 0) {
    r = r->next;
  }
  if (!r) {
    r = (Registered *)calloc(1, sizeof(Registered));
    char *n = awra_strdup(name);
    if (!r || !n) {
      free(r);
      free(n);
      free(copy);
      return false;
    }
    r->name = n;
    r->next = T.registered;
    T.registered = r;
  }
  /* what was looked up with the old bytes goes first */
  res_clear();
  free_unthemed();
  T.serial++;
  free(r->svg);
  r->svg = copy;
  r->len = length;
  awra_windows_icons_changed();
  return true;
}

/* An SVG the application registered, else a built-in one of that name. */
const char *awra_icon_svg_source(const char *name);
const char *awra_icon_svg_source(const char *name)
{
  if (!name) {
    return NULL;
  }
  for (Registered *r = T.registered; r; r = r->next) {
    if (!strcmp(r->name, name)) {
      return r->svg;
    }
  }
  char file[300];
  if (strlen(name) + 5 >= sizeof(file)) {
    return NULL;
  }
  snprintf(file, sizeof(file), "%s.svg", name);
  const AwraBuiltinIcon *b = awra_builtin_find(&awra_builtin_awra, file);
  if (!b) {
    b = awra_builtin_find(&awra_builtin_gtk, file);
  }
  /* Adwaita's, under their theme directories */
  const size_t fl = strlen(file);
  for (int i = 0; !b && i < awra_builtin_adwaita.count; i++) {
    const char *p = awra_builtin_adwaita.icons[i].path;
    const size_t pl = strlen(p);
    if (pl > fl && p[pl - fl - 1] == '/' && !strcmp(p + pl - fl, file)) {
      b = &awra_builtin_adwaita.icons[i];
    }
  }
  return b ? (const char *)b->data : NULL;
}

bool awra_icon_exists(const char *name)
{
  return awra_icon_theme_has_icon(name);
}

/* Everything let go (tests, leak checks): the next lookup starts over. */
void awra_icon_theme_reset(void);
void awra_icon_theme_reset(void)
{
  res_clear();
  if (T.valid) {
    blow_themes();
  }
  awra_strv_free(T.search_path);
  T.search_path = NULL;
  T.custom_theme = false;
  free(T.current);
  T.current = NULL;
  while (T.registered) {
    Registered *r = T.registered;
    T.registered = r->next;
    free(r->name);
    free(r->svg);
    free(r);
  }
  while (T.listeners) {
    ChangedListener *l = T.listeners;
    T.listeners = l->next;
    free(l);
  }
  if (T.notify_hooked) {
    BK_set_settings_notify(NULL);
    T.notify_hooked = false;
  }
}

/* ======================================================================= */
/* GtkIconPaintable */

AwraTexture *awra_icon_file_render_texture(const AwraIconFile *f, int px, const AwraRGBA *fg); /* icons.c */

struct AwraIconPaintable {
  int refs;
  AwraIconFile file; /* its own copy */
  int size, scale;
};

static AwraIconPaintable *paintable_new(AwraIconFile *f, int size, int scale)
{
  AwraIconPaintable *p = f ? (AwraIconPaintable *)calloc(1, sizeof(AwraIconPaintable)) : NULL;
  if (p) {
    p->refs = 1;
    p->size = size;
    p->scale = scale;
    p->file.path = f->path ? awra_strdup(f->path) : NULL;
    p->file.icon_name = f->icon_name ? awra_strdup(f->icon_name) : NULL;
    p->file.is_svg = f->is_svg;
    p->file.is_png = f->is_png;
    p->file.is_symbolic = f->is_symbolic;
    if (!f->path && f->data) {
      /* registered bytes may be replaced: a copy */
      unsigned char *d = (unsigned char *)malloc(f->len + 1);
      if (d) {
        memcpy(d, f->data, f->len);
        d[f->len] = 0;
      }
      p->file.data = d;
      p->file.len = f->len;
    }
  }
  return p;
}

AwraIconPaintable *awra_icon_theme_lookup_icon(const char *icon_name, const char *const *fallbacks,
                                               int size, int scale, AwraTextDirection direction,
                                               AwraIconLookupFlags flags)
{
  if (!icon_name || size <= 0 || scale < 1) {
    return NULL;
  }
  const char *names[64];
  int n = 0;
  names[n++] = icon_name;
  for (const char *const *f = fallbacks; f && *f && n < 64; f++) {
    names[n++] = *f;
  }
  AwraIconFile *f = choose(names, n, size, scale, (int)direction,
                           flags & (AWRA_ICON_LOOKUP_FORCE_REGULAR | AWRA_ICON_LOOKUP_FORCE_SYMBOLIC));
  AwraIconPaintable *p = paintable_new(f, size, scale);
  file_unref(f);
  return p;
}

AwraIconPaintable *awra_icon_theme_lookup_names(const char *const *names, int n, int size, int scale,
                                                AwraTextDirection direction,
                                                AwraIconLookupFlags flags)
{
  if (!names || n <= 0 || size <= 0 || scale < 1) {
    return NULL;
  }
  char *expanded = awra_themed_icon_expand(names, n, false);
  if (!expanded) {
    return NULL;
  }
  const char *list[64];
  int m = 0;
  for (char *p = expanded; p && *p && m < 64;) {
    char *nl = strchr(p, '\n');
    if (nl) {
      *nl = 0;
    }
    list[m++] = p;
    p = nl ? nl + 1 : NULL;
  }
  AwraIconPaintable *p = NULL;
  if (m > 0) {
    AwraIconFile *f = choose(list, m, size, scale, (int)direction,
                             flags & (AWRA_ICON_LOOKUP_FORCE_REGULAR | AWRA_ICON_LOOKUP_FORCE_SYMBOLIC));
    p = paintable_new(f, size, scale);
    file_unref(f);
  }
  free(expanded);
  return p;
}

AwraIconPaintable *awra_icon_paintable_new_for_file(const char *path, int size, int scale)
{
  if (!path) {
    return NULL;
  }
  const char *base = strrchr(path, '/');
  AwraIconFile tmp = {0};
  tmp.path = (char *)path;
  base = base ? base + 1 : path;
  tmp.is_svg = has_suffix(base, ".svg");
  tmp.is_png = has_suffix(base, ".png");
  tmp.is_symbolic = uri_is_symbolic(base);
  return paintable_new(&tmp, size, scale < 1 ? 1 : scale);
}

AwraIconPaintable *awra_icon_paintable_ref(AwraIconPaintable *icon)
{
  if (icon) {
    icon->refs++;
  }
  return icon;
}

void awra_icon_paintable_unref(AwraIconPaintable *icon)
{
  if (!icon || --icon->refs > 0) {
    return;
  }
  free(icon->file.path);
  free(icon->file.icon_name);
  free((void *)icon->file.data);
  free(icon);
}

const char *awra_icon_paintable_get_file(const AwraIconPaintable *icon)
{
  return icon ? icon->file.path : NULL;
}

const char *awra_icon_paintable_get_icon_name(const AwraIconPaintable *icon)
{
  return icon ? icon->file.icon_name : NULL;
}

bool awra_icon_paintable_is_symbolic(const AwraIconPaintable *icon)
{
  return icon && icon->file.is_symbolic;
}

int awra_icon_paintable_get_size(const AwraIconPaintable *icon)
{
  return icon ? icon->size : 0;
}

int awra_icon_paintable_get_scale(const AwraIconPaintable *icon)
{
  return icon ? icon->scale : 0;
}

AwraTexture *awra_icon_paintable_render(AwraIconPaintable *icon, const AwraRGBA *color)
{
  return icon ? awra_icon_file_render_texture(&icon->file, icon->size * icon->scale, color) : NULL;
}

char *awra_themed_icon_names(const char *const *names, int n, bool use_default_fallbacks)
{
  return names && n > 0 ? awra_themed_icon_expand(names, n, use_default_fallbacks) : NULL;
}
