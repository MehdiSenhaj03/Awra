/* Awra 2 content types: GIO's fdo backend (gio/gcontenttype-fdo.c and its
 * xdgmime, GLib 2.88) over the shared-mime-info database, rule for rule:
 *
 * - the types of a file name (mime.cache: literal names, then the
 *   reverse-suffix tree in lower case then as written, then fnmatch
 *   globs; by weight), the type of data (magic, by priority, the
 *   text-or-binary fallback), g_content_type_guess's way of putting both
 *   together, and GLocalFileInfo's for a file on disk;
 * - aliases, parents (subclasses, text/plain above every text type, everything but
 *   inode types under application/octet-stream), is_a / equals;
 * - the icons g_content_type_get_icon names: the "icons" entry, the type
 *   with '/' as '-', the generic icon ("generic-icons", else
 *   <media>-x-generic), as a GThemedIcon (their -symbolic variants after);
 * - the description: the <comment> of <type>.xml best matching the user's
 *   languages (g_get_language_names).
 *
 * The mime.cache files of $XDG_DATA_HOME/mime and $XDG_DATA_DIRS/mime are
 * mapped the first time a type is asked for; without any, the text files
 * (globs2, aliases, subclasses, icons, generic-icons) are read instead, as
 * xdgmime does (no magic then).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "icontheme.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <fnmatch.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

static const char k_unknown[] = "application/octet-stream";
static const char k_empty[] = "application/x-zerosize";
static const char k_textplain[] = "text/plain";

typedef struct MimeCache {
  const uint8_t *buf;
  size_t len;
  bool mapped;
} MimeCache;

/* text mode (no cache anywhere) */
typedef struct Pair {
  char *a, *b;
  int weight; /* globs */
  bool case_sensitive;
} Pair;

typedef struct PairList {
  Pair *v;
  int n, cap;
} PairList;

static struct {
  bool loaded;
  char **dirs; /* .../mime */
  MimeCache **caches;
  int ncaches;
  /* text mode */
  PairList globs, aliases, parents, icons, generic;
} M;

/* ----------------------------------------------------------------------- */
/* Loading */

static MimeCache *cache_map(const char *path)
{
#ifndef _WIN32
  const int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return NULL;
  }
  struct stat st;
  if (fstat(fd, &st) != 0 || st.st_size < 40) {
    close(fd);
    return NULL;
  }
  void *m = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (m == MAP_FAILED) {
    return NULL;
  }
  MimeCache *c = (MimeCache *)calloc(1, sizeof(MimeCache));
  if (!c) {
    munmap(m, (size_t)st.st_size);
    return NULL;
  }
  c->buf = (const uint8_t *)m;
  c->len = (size_t)st.st_size;
  c->mapped = true;
#else
  size_t len = 0;
  char *data = awra_read_file(path, &len);
  if (!data || len < 40) {
    free(data);
    return NULL;
  }
  MimeCache *c = (MimeCache *)calloc(1, sizeof(MimeCache));
  if (!c) {
    free(data);
    return NULL;
  }
  c->buf = (const uint8_t *)data;
  c->len = len;
#endif
  /* xdgmime takes major version 1, minor 1 or 2 */
  const unsigned major = (unsigned)c->buf[0] << 8 | c->buf[1];
  if (major != 1) {
#ifndef _WIN32
    munmap((void *)c->buf, c->len);
#else
    free((void *)c->buf);
#endif
    free(c);
    return NULL;
  }
  return c;
}

static void pair_add(PairList *l, const char *a, const char *b, int weight, bool cs)
{
  if (l->n + 1 > l->cap) {
    const int cap = l->cap ? l->cap * 2 : 256;
    Pair *v = (Pair *)realloc(l->v, sizeof(Pair) * cap);
    if (!v) {
      return;
    }
    l->v = v;
    l->cap = cap;
  }
  Pair p = {awra_strdup(a), awra_strdup(b), weight, cs};
  if (!p.a || !p.b) {
    free(p.a);
    free(p.b);
    return;
  }
  l->v[l->n++] = p;
}

/* "a:b" or "a b" lines */
static void read_pairs(const char *path, PairList *l, char sep)
{
  char *text = awra_read_file(path, NULL);
  if (!text) {
    return;
  }
  for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
    if (line[0] == '#') {
      continue;
    }
    char *s = strchr(line, sep);
    if (!s) {
      continue;
    }
    *s = 0;
    char *b = s + 1;
    size_t n = strlen(b);
    while (n && (b[n - 1] == '\r' || b[n - 1] == ' ')) {
      b[--n] = 0;
    }
    pair_add(l, line, b, 0, false);
  }
  free(text);
}

/* globs2: weight:type:glob[:flags] (globs: type:glob, weight 50) */
static void read_globs(const char *path, bool v2)
{
  char *text = awra_read_file(path, NULL);
  if (!text) {
    return;
  }
  for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
    if (line[0] == '#') {
      continue;
    }
    int weight = 50;
    char *type = line;
    if (v2) {
      char *c = strchr(line, ':');
      if (!c) {
        continue;
      }
      *c = 0;
      weight = atoi(line);
      type = c + 1;
    }
    char *g = strchr(type, ':');
    if (!g) {
      continue;
    }
    *g++ = 0;
    char *flags = v2 ? strchr(g, ':') : NULL;
    bool cs = false;
    if (flags) {
      *flags++ = 0;
      cs = strstr(flags, "cs") != NULL;
    }
    pair_add(&M.globs, g, type, weight, cs);
  }
  free(text);
}

static void load(void)
{
  if (M.loaded) {
    return;
  }
  M.loaded = true;
  char **data = awra_xdg_data_dirs(true);
  int n = 0;
  for (char **p = data; p && *p; p++) {
    n++;
  }
  M.dirs = (char **)calloc((size_t)n + 1, sizeof(char *));
  M.caches = (MimeCache **)calloc((size_t)n + 1, sizeof(MimeCache *));
  if (!M.dirs || !M.caches) {
    awra_strv_free(data);
    return;
  }
  for (int i = 0; i < n; i++) {
    const size_t l = strlen(data[i]);
    M.dirs[i] = (char *)malloc(l + 6);
    if (M.dirs[i]) {
      memcpy(M.dirs[i], data[i], l);
      memcpy(M.dirs[i] + l, "/mime", 6);
    }
  }
  awra_strv_free(data);
  /* xdg_mime_init_from_directory: a directory's cache, else its files */
  for (int i = 0; i < n; i++) {
    if (!M.dirs[i]) {
      continue;
    }
    char path[1100];
    snprintf(path, sizeof(path), "%s/mime.cache", M.dirs[i]);
    MimeCache *c = cache_map(path);
    if (c) {
      M.caches[M.ncaches++] = c;
      continue;
    }
    snprintf(path, sizeof(path), "%s/globs2", M.dirs[i]);
    struct stat st;
    if (stat(path, &st) == 0) {
      read_globs(path, true);
    }
    else {
      snprintf(path, sizeof(path), "%s/globs", M.dirs[i]);
      read_globs(path, false);
    }
    snprintf(path, sizeof(path), "%s/aliases", M.dirs[i]);
    read_pairs(path, &M.aliases, ' ');
    snprintf(path, sizeof(path), "%s/subclasses", M.dirs[i]);
    read_pairs(path, &M.parents, ' ');
    snprintf(path, sizeof(path), "%s/icons", M.dirs[i]);
    read_pairs(path, &M.icons, ':');
    snprintf(path, sizeof(path), "%s/generic-icons", M.dirs[i]);
    read_pairs(path, &M.generic, ':');
  }
}

/* ----------------------------------------------------------------------- */
/* mime.cache reading (bounds checked as xdgmime's OUT_OF_BOUNDS) */

static uint32_t U32(const MimeCache *c, uint32_t off)
{
  if ((size_t)off + 4 > c->len) {
    return 0;
  }
  const uint8_t *p = c->buf + off;
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static bool oob(const MimeCache *c, uint32_t off, uint32_t n, uint32_t size)
{
  return (uint64_t)off + (uint64_t)n * size > c->len;
}

static const char *STR(const MimeCache *c, uint32_t off)
{
  if (off >= c->len || !memchr(c->buf + off, 0, c->len - off)) {
    return "";
  }
  return (const char *)c->buf + off;
}

/* a sorted (string, value) list at header offset `hdr`, entry size `es` */
static const char *cache_sorted_lookup(int hdr, int es, const char *key, int value_at)
{
  for (int i = 0; i < M.ncaches; i++) {
    const MimeCache *c = M.caches[i];
    const uint32_t list = U32(c, (uint32_t)hdr);
    if (oob(c, list, 1, 4)) {
      continue;
    }
    const uint32_t n = U32(c, list);
    if (oob(c, list + 4, n, (uint32_t)es)) {
      continue;
    }
    int lo = 0, hi = (int)n - 1;
    while (lo <= hi) {
      const int mid = (lo + hi) / 2;
      const int cmp = strcmp(STR(c, U32(c, list + 4 + (uint32_t)es * mid)), key);
      if (cmp < 0) {
        lo = mid + 1;
      }
      else if (cmp > 0) {
        hi = mid - 1;
      }
      else {
        return STR(c, U32(c, list + 4 + (uint32_t)es * mid + (uint32_t)value_at));
      }
    }
  }
  return NULL;
}

static const char *pairs_lookup(const PairList *l, const char *a)
{
  for (int i = 0; i < l->n; i++) {
    if (!strcmp(l->v[i].a, a)) {
      return l->v[i].b;
    }
  }
  return NULL;
}

static const char *unalias(const char *mime)
{
  load();
  const char *r = M.ncaches ? cache_sorted_lookup(4, 8, mime, 4) : pairs_lookup(&M.aliases, mime);
  return r ? r : mime;
}

/* ----------------------------------------------------------------------- */
/* File names */

typedef struct MimeWeight {
  const char *mime;
  int weight;
} MimeWeight;

static char *ascii_lower(const char *s)
{
  char *l = awra_strdup(s);
  for (char *p = l; p && *p; p++) {
    if (*p >= 'A' && *p <= 'Z') {
      *p = (char)(*p - 'A' + 'a');
    }
  }
  return l;
}

static int cache_literal(const char *name, const char **out, bool cs_check)
{
  for (int i = 0; i < M.ncaches; i++) {
    const MimeCache *c = M.caches[i];
    const uint32_t list = U32(c, 12);
    if (oob(c, list, 1, 4)) {
      continue;
    }
    const uint32_t n = U32(c, list);
    if (oob(c, list + 4, n, 12)) {
      continue;
    }
    int lo = 0, hi = (int)n - 1;
    while (lo <= hi) {
      const int mid = (lo + hi) / 2;
      const int cmp = strcmp(STR(c, U32(c, list + 4 + 12 * mid)), name);
      if (cmp < 0) {
        lo = mid + 1;
      }
      else if (cmp > 0) {
        hi = mid - 1;
      }
      else {
        const uint32_t w = U32(c, list + 4 + 12 * mid + 8);
        if (cs_check || !(w & 0x100)) {
          out[0] = STR(c, U32(c, list + 4 + 12 * mid + 4));
          return 1;
        }
        return 0;
      }
    }
  }
  return 0;
}

static int node_suffix(const MimeCache *c, uint32_t n_entries, uint32_t offset, const char *name, int len,
                       bool cs_check, MimeWeight *out, int max, int depth)
{
  if (len <= 0 || depth > 255) {
    return 0;
  }
  const uint32_t ch = (unsigned char)name[len - 1];
  int lo = 0, hi = (int)n_entries - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint32_t mc = U32(c, offset + 12 * mid);
    if (mc < ch) {
      lo = mid + 1;
    }
    else if (mc > ch) {
      hi = mid - 1;
    }
    else {
      len--;
      int n = 0;
      const uint32_t nc = U32(c, offset + 12 * mid + 4), co = U32(c, offset + 12 * mid + 8);
      if (oob(c, co, nc, 12)) {
        return 0;
      }
      if (len > 0) {
        n = node_suffix(c, nc, co, name, len, cs_check, out, max, depth + 1);
      }
      if (n == 0) {
        for (uint32_t i = 0; n < max && i < nc; i++) {
          if (U32(c, co + 12 * i) != 0) {
            break;
          }
          const uint32_t w = U32(c, co + 12 * i + 8);
          if (cs_check || !(w & 0x100)) {
            out[n].mime = STR(c, U32(c, co + 12 * i + 4));
            out[n].weight = (int)(w & 0xff);
            n++;
          }
        }
      }
      return n;
    }
  }
  return 0;
}

static int cache_suffix(const char *name, int len, bool cs, MimeWeight *out, int max)
{
  int n = 0;
  for (int i = 0; i < M.ncaches && n < max; i++) {
    const MimeCache *c = M.caches[i];
    const uint32_t list = U32(c, 16);
    if (oob(c, list, 1, 8)) {
      continue;
    }
    const uint32_t ne = U32(c, list), off = U32(c, list + 4);
    if (oob(c, off, ne, 12)) {
      continue;
    }
    n += node_suffix(c, ne, off, name, len, cs, out + n, max - n, 0);
  }
  return n;
}

static bool glob_match(const char *glob, const char *name)
{
#ifdef _WIN32
  /* fnmatch without flags: '*', '?', '[...]' */
  if (!*glob) {
    return !*name;
  }
  if (*glob == '*') {
    for (const char *p = name;; p++) {
      if (glob_match(glob + 1, p)) {
        return true;
      }
      if (!*p) {
        return false;
      }
    }
  }
  if (!*name) {
    return false;
  }
  if (*glob == '[') {
    const char *e = strchr(glob + 1, ']');
    if (e) {
      bool neg = glob[1] == '!' || glob[1] == '^', hit = false;
      for (const char *p = glob + 1 + neg; p < e; p++) {
        if (p[1] == '-' && p + 2 < e) {
          hit = hit || (*name >= p[0] && *name <= p[2]);
          p += 2;
        }
        else {
          hit = hit || *name == *p;
        }
      }
      return hit != neg && glob_match(e + 1, name + 1);
    }
  }
  return (*glob == '?' || *glob == *name) && glob_match(glob + 1, name + 1);
#else
  return fnmatch(glob, name, 0) == 0;
#endif
}

static int cache_fnmatch(const char *name, MimeWeight *out, int max, bool cs)
{
  int n = 0;
  for (int i = 0; i < M.ncaches; i++) {
    const MimeCache *c = M.caches[i];
    const uint32_t list = U32(c, 20);
    if (oob(c, list, 1, 4)) {
      continue;
    }
    const uint32_t ne = U32(c, list);
    if (oob(c, list + 4, ne, 12)) {
      continue;
    }
    for (uint32_t j = 0; j < ne && n < max; j++) {
      const uint32_t w = U32(c, list + 4 + 12 * j + 8);
      if ((cs || !(w & 0x100)) && glob_match(STR(c, U32(c, list + 4 + 12 * j)), name)) {
        out[n].mime = STR(c, U32(c, list + 4 + 12 * j + 4));
        out[n].weight = (int)(w & 0xff);
        n++;
      }
    }
    if (n == max) {
      break;
    }
  }
  return n;
}

static int filter_dupes(MimeWeight *m, int n)
{
  int last = n;
  for (int i = 0; i < last; i++) {
    int j = i + 1;
    while (j < last) {
      if (!strcmp(m[i].mime, m[j].mime)) {
        m[i].weight = m[i].weight > m[j].weight ? m[i].weight : m[j].weight;
        last--;
        m[j] = m[last];
      }
      else {
        j++;
      }
    }
  }
  return last;
}

static int cmp_weight(const void *a, const void *b)
{
  return ((const MimeWeight *)b)->weight - ((const MimeWeight *)a)->weight;
}

/* text mode: xdgmime's glob hash, close enough to its order */
static int text_lookup(const char *name, MimeWeight *m, int max)
{
  char *lower = ascii_lower(name);
  int n = 0;
  if (!lower) {
    return 0;
  }
  for (int pass = 0; pass < 2 && !n; pass++) {
    for (int i = 0; i < M.globs.n; i++) {
      const Pair *g = &M.globs.v[i];
      if (!strpbrk(g->a, "*?[") && !strcmp(g->a, pass == 0 || g->case_sensitive ? name : lower)) {
        m[0].mime = g->b;
        m[0].weight = 100;
        free(lower);
        return 1;
      }
    }
  }
  /* "*.ext": the longest suffix wins */
  size_t best = 0;
  for (int i = 0; i < M.globs.n && n < max; i++) {
    const Pair *g = &M.globs.v[i];
    if (g->a[0] == '*' && !strpbrk(g->a + 1, "*?[")) {
      const size_t sl = strlen(g->a + 1), nl = strlen(name);
      const char *subject = g->case_sensitive ? name : lower;
      if (sl <= nl && !strcmp(subject + nl - sl, g->a + 1) && sl >= best) {
        if (sl > best) {
          n = 0;
          best = sl;
        }
        m[n].mime = g->b;
        m[n].weight = g->weight;
        n++;
      }
    }
  }
  if (n < 2) {
    for (int i = 0; i < M.globs.n && n < max; i++) {
      const Pair *g = &M.globs.v[i];
      if (strpbrk(g->a, "*?[") && !(g->a[0] == '*' && !strpbrk(g->a + 1, "*?[")) &&
          glob_match(g->a, g->case_sensitive ? name : lower))
      {
        m[n].mime = g->b;
        m[n].weight = g->weight;
        n++;
      }
    }
  }
  free(lower);
  return n;
}

/* xdg_mime_get_mime_types_from_file_name */
static int types_from_name(const char *name, const char **out, int max)
{
  load();
  MimeWeight m[10];
  int n;
  if (M.ncaches) {
    char *lower = ascii_lower(name);
    if (!lower) {
      return 0;
    }
    if (cache_literal(lower, out, false) || cache_literal(name, out, true)) {
      free(lower);
      return 1;
    }
    const int len = (int)strlen(name);
    n = cache_suffix(lower, len, false, m, 10);
    if (n < 2) {
      n += cache_suffix(name, len, true, m + n, 10 - n);
    }
    if (n == 0) {
      n = cache_fnmatch(lower, m, 10, false);
    }
    if (n < 2) {
      n += cache_fnmatch(name, m + n, 10 - n, true);
    }
    free(lower);
  }
  else {
    n = text_lookup(name, m, 10);
  }
  n = filter_dupes(m, n);
  qsort(m, (size_t)n, sizeof(MimeWeight), cmp_weight);
  if (n > max) {
    n = max;
  }
  for (int i = 0; i < n; i++) {
    out[i] = m[i].mime;
  }
  return n;
}

/* ----------------------------------------------------------------------- */
/* Parents */

static bool media_equal(const char *a, const char *b)
{
  const char *sa = strchr(a, '/');
  return sa && !strncmp(a, b, (size_t)(sa - a + 1));
}

static bool subclass(const char *mime, const char *base, int depth)
{
  const char *um = unalias(mime), *ub = unalias(base);
  if (!strcmp(um, ub)) {
    return true;
  }
  const size_t bl = strlen(ub);
  if (bl >= 2 && !strcmp(ub + bl - 2, "/*") && media_equal(um, ub)) {
    return true;
  }
  if (!strcmp(ub, "text/plain") && !strncmp(um, "text/", 5)) {
    return true;
  }
  if (!strcmp(ub, "application/octet-stream") && strncmp(um, "inode/", 6) != 0) {
    return true;
  }
  if (depth > 32) {
    return false; /* circular parents */
  }
  if (M.ncaches) {
    for (int i = 0; i < M.ncaches; i++) {
      const MimeCache *c = M.caches[i];
      const uint32_t list = U32(c, 8);
      if (oob(c, list, 1, 4)) {
        continue;
      }
      const uint32_t n = U32(c, list);
      if (oob(c, list + 4, n, 8)) {
        continue;
      }
      int lo = 0, hi = (int)n - 1;
      while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const int cmp = strcmp(STR(c, U32(c, list + 4 + 8 * mid)), um);
        if (cmp < 0) {
          lo = mid + 1;
        }
        else if (cmp > 0) {
          hi = mid - 1;
        }
        else {
          const uint32_t off = U32(c, list + 4 + 8 * mid + 4);
          const uint32_t np = U32(c, off);
          for (uint32_t j = 0; j < np && j < 64; j++) {
            if (subclass(STR(c, U32(c, off + 4 + 4 * j)), ub, depth + 1)) {
              return true;
            }
          }
          break;
        }
      }
    }
    return false;
  }
  for (int i = 0; i < M.parents.n; i++) {
    if (!strcmp(M.parents.v[i].a, um) && subclass(M.parents.v[i].b, ub, depth + 1)) {
      return true;
    }
  }
  return false;
}

/* ----------------------------------------------------------------------- */
/* Data */

static bool matchlet(const MimeCache *c, uint32_t off, const uint8_t *data, size_t len, int depth)
{
  const uint32_t start = U32(c, off), range = U32(c, off + 4), dlen = U32(c, off + 12);
  const uint32_t doff = U32(c, off + 16), moff = U32(c, off + 20);
  const uint32_t nc = U32(c, off + 24), co = U32(c, off + 28);
  if (oob(c, co, nc, 32) || oob(c, doff, dlen, 1) || (moff && oob(c, moff, dlen, 1)) || depth > 64) {
    return false;
  }
  bool ok = false;
  for (uint64_t i = start; i < (uint64_t)start + range && !ok; i++) {
    if (i + dlen > len) {
      return false;
    }
    if (moff) {
      ok = true;
      for (uint32_t j = 0; j < dlen; j++) {
        const uint8_t mk = c->buf[moff + j];
        if ((c->buf[doff + j] & mk) != (data[i + j] & mk)) {
          ok = false;
          break;
        }
      }
    }
    else {
      ok = memcmp(c->buf + doff, data + i, dlen) == 0;
    }
  }
  if (!ok) {
    return false;
  }
  if (nc == 0) {
    return true;
  }
  for (uint32_t i = 0; i < nc; i++) {
    if (matchlet(c, co + 32 * i, data, len, depth + 1)) {
      return true;
    }
  }
  return false;
}

static const char *cache_magic(const MimeCache *c, const uint8_t *data, size_t len, int *prio)
{
  *prio = 0;
  const uint32_t list = U32(c, 24);
  if (oob(c, list, 1, 12)) {
    return NULL;
  }
  const uint32_t n = U32(c, list), off = U32(c, list + 8);
  if (oob(c, off, n, 16)) {
    return NULL;
  }
  for (uint32_t j = 0; j < n; j++) {
    const uint32_t m = off + 16 * j;
    const uint32_t nm = U32(c, m + 8), mo = U32(c, m + 12);
    if (oob(c, mo, nm, 32)) {
      continue;
    }
    for (uint32_t i = 0; i < nm; i++) {
      if (matchlet(c, mo + 32 * i, data, len, 0)) {
        *prio = (int)U32(c, m);
        return STR(c, U32(c, m + 4));
      }
    }
  }
  return NULL;
}

/* xdg_mime_get_mime_type_for_data; *r_fallback: the text-or-binary guess */
static const char *type_for_data(const uint8_t *data, size_t len, int *r_prio)
{
  if (len == 0) {
    *r_prio = 100;
    return k_empty;
  }
  load();
  const char *type = NULL;
  int priority = 0;
  for (int i = 0; i < M.ncaches; i++) {
    int prio;
    const char *match = cache_magic(M.caches[i], data, len, &prio);
    if (prio > priority) {
      priority = prio;
      type = match;
    }
  }
  *r_prio = priority;
  if (priority > 0 && type) {
    return type;
  }
  for (size_t i = 0; i < 128 && i < len; i++) {
    if (data[i] < 32 && data[i] != 9 && data[i] != 10 && data[i] != 13) {
      return k_unknown;
    }
  }
  return k_textplain;
}

static bool looks_like_text(const uint8_t *data, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    const unsigned char c = data[i];
    if (iscntrl(c) && !isspace(c) && c != '\b') {
      return false;
    }
  }
  return true;
}

/* ----------------------------------------------------------------------- */
/* Public API */

char *awra_content_type_guess(const char *filename, const void *data, size_t size, bool *uncertain)
{
  const char *names[10];
  int n = 0;
  if (uncertain) {
    *uncertain = false;
  }
  load();
  if (filename) {
    const size_t l = strlen(filename);
    if (l > 0 && filename[l - 1] == '/') {
      names[0] = "inode/directory";
      n = 1;
      if (uncertain) {
        *uncertain = true;
      }
    }
    else {
      const char *base = strrchr(filename, '/');
#ifdef _WIN32
      const char *b2 = strrchr(filename, '\\');
      if (b2 && (!base || b2 > base)) {
        base = b2;
      }
#endif
      n = types_from_name(base ? base + 1 : filename, names, 10);
    }
  }
  if (n == 1) {
    return awra_strdup(names[0]);
  }
  const char *sniffed = k_unknown;
  int prio = 0;
  if (data) {
    sniffed = type_for_data((const uint8_t *)data, size, &prio);
    if (sniffed == k_unknown && looks_like_text((const uint8_t *)data, size)) {
      sniffed = "text/plain";
    }
    if (filename && !strcmp(sniffed, "application/x-desktop")) {
      sniffed = "text/plain"; /* never sniff a desktop file GIO's way */
    }
  }
  if (n == 0) {
    if (sniffed == k_unknown && uncertain) {
      *uncertain = true;
    }
    return awra_strdup(sniffed);
  }
  if (sniffed != k_unknown) {
    if (prio >= 80) {
      return awra_strdup(sniffed);
    }
    for (int i = 0; i < n; i++) {
      if (subclass(names[i], sniffed, 0)) {
        return awra_strdup(names[i]);
      }
    }
  }
  if (uncertain) {
    *uncertain = true;
  }
  return awra_strdup(names[0]);
}

/* GLocalFileInfo's content type of a file on disk (following links). */
char *awra_content_type_for_file(const char *path, bool fast);
char *awra_content_type_for_file(const char *path, bool fast)
{
  if (!path) {
    return NULL;
  }
  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;
#ifndef _WIN32
  struct stat lst, st;
  if (lstat(path, &lst) == 0 && S_ISLNK(lst.st_mode) && stat(path, &st) != 0) {
    return awra_content_type_from_mime_type("inode/symlink");
  }
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return awra_content_type_from_mime_type("inode/directory");
    }
    if (S_ISCHR(st.st_mode)) {
      return awra_content_type_from_mime_type("inode/chardevice");
    }
    if (S_ISBLK(st.st_mode)) {
      return awra_content_type_from_mime_type("inode/blockdevice");
    }
    if (S_ISFIFO(st.st_mode)) {
      return awra_content_type_from_mime_type("inode/fifo");
    }
    if (S_ISSOCK(st.st_mode)) {
      return awra_content_type_from_mime_type("inode/socket");
    }
    if (S_ISREG(st.st_mode) && st.st_size == 0) {
      return awra_content_type_from_mime_type("application/x-zerosize");
    }
  }
#else
  struct _stat64 st;
  if (_stat64(path, &st) == 0 && (st.st_mode & _S_IFDIR)) {
    return awra_content_type_from_mime_type("inode/directory");
  }
#endif
  bool uncertain = false;
  char *type = awra_content_type_guess(base, NULL, 0, &uncertain);
#ifndef _WIN32
  if (!fast && uncertain) {
    /* the start of the file (xdgmime's largest extent, 16 KiB at most) */
    uint32_t extent = 0;
    for (int i = 0; i < M.ncaches; i++) {
      const uint32_t off = U32(M.caches[i], 24);
      const uint32_t e = oob(M.caches[i], off, 1, 8) ? 0 : U32(M.caches[i], off + 4);
      extent = e > extent ? e : extent;
    }
    if (extent == 0 || extent > 16384) {
      extent = 16384;
    }
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
      uint8_t *buf = (uint8_t *)malloc(extent);
      const ssize_t got = buf ? read(fd, buf, extent) : -1;
      close(fd);
      if (got >= 0) {
        free(type);
        type = awra_content_type_guess(base, buf, (size_t)got, NULL);
      }
      free(buf);
    }
  }
#endif
  return type;
}

char *awra_content_type_from_mime_type(const char *mime)
{
  return mime ? awra_strdup(unalias(mime)) : NULL;
}

char *awra_content_type_get_mime_type(const char *type)
{
  return type ? awra_strdup(type) : NULL;
}

bool awra_content_type_is_a(const char *type, const char *supertype)
{
  if (!type || !supertype) {
    return false;
  }
  load();
  return subclass(type, supertype, 0);
}

bool awra_content_type_is_mime_type(const char *type, const char *mime)
{
  return awra_content_type_is_a(type, mime);
}

bool awra_content_type_equals(const char *a, const char *b)
{
  if (!a || !b) {
    return false;
  }
  return !strcmp(unalias(a), unalias(b));
}

bool awra_content_type_is_unknown(const char *type)
{
  return type && !strcmp(type, k_unknown);
}

bool awra_content_type_can_be_executable(const char *type)
{
  return awra_content_type_is_a(type, "application/x-executable") || awra_content_type_is_a(type, "text/plain");
}

static const char *icon_of(int hdr, PairList *l, const char *type)
{
  load();
  return M.ncaches ? cache_sorted_lookup(hdr, 8, type, 4) : pairs_lookup(l, type);
}

char *awra_content_type_get_generic_icon_name(const char *type)
{
  if (!type) {
    return NULL;
  }
  const char *g = icon_of(36, &M.generic, type);
  if (g) {
    return awra_strdup(g);
  }
  const char *slash = strchr(type, '/');
  const size_t n = slash ? (size_t)(slash - type) : strlen(type);
  char *out = (char *)malloc(n + 11);
  if (out) {
    memcpy(out, type, n);
    memcpy(out + n, "-x-generic", 11);
  }
  return out;
}

static char *icon_names(const char *type, bool symbolic)
{
  if (!type) {
    return NULL;
  }
  char *names[6];
  int n = 0;
  const char *xdg = icon_of(32, &M.icons, type);
  if (xdg) {
    names[n++] = awra_strdup(xdg);
  }
  char *t = awra_strdup(type);
  for (char *p = t; p && *p; p++) {
    if (*p == '/') {
      *p = '-';
    }
  }
  names[n++] = t;
  names[n++] = awra_content_type_get_generic_icon_name(type);
  if (symbolic) {
    for (int i = 0; i < n; i++) {
      names[n + i] = names[i];
      char *s = names[i] ? (char *)malloc(strlen(names[i]) + 10) : NULL;
      if (s) {
        strcpy(s, names[i]);
        strcat(s, "-symbolic");
      }
      names[i] = s;
    }
    n += n;
  }
  const char *list[6];
  int m = 0;
  for (int i = 0; i < n; i++) {
    if (names[i]) {
      list[m++] = names[i];
    }
  }
  char *out = awra_themed_icon_expand(list, m, false);
  for (int i = 0; i < n; i++) {
    free(names[i]);
  }
  return out;
}

char *awra_content_type_get_icon_names(const char *type)
{
  return icon_names(type, false);
}

char *awra_content_type_get_symbolic_icon_names(const char *type)
{
  return icon_names(type, true);
}

/* g_get_language_names: LANGUAGE, else LC_ALL, LC_MESSAGES, LANG, each
 * with its variants (ll_CC.codeset@mod ... ll), then "C" */
static int language_names(char out[][64], int max)
{
  const char *v = NULL;
  static const char *const envs[] = {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"};
  for (int i = 0; i < 4 && !v; i++) {
    const char *e = getenv(envs[i]);
    if (e && e[0]) {
      v = e;
    }
  }
  int n = 0;
  char buf[256];
  snprintf(buf, sizeof(buf), "%s", v ? v : "C");
  for (char *save = NULL, *lang = strtok_r(buf, ":", &save); lang && n < max; lang = strtok_r(NULL, ":", &save)) {
    char l[64] = "", terr[64] = "", code[64] = "", mod[64] = "";
    const char *p = lang;
    size_t k = strcspn(p, "_.@");
    snprintf(l, sizeof(l), "%.*s", (int)k, p);
    p += k;
    if (*p == '_') {
      k = strcspn(p + 1, ".@");
      snprintf(terr, sizeof(terr), "_%.*s", (int)k, p + 1);
      p += 1 + k;
    }
    if (*p == '.') {
      k = strcspn(p + 1, "@");
      snprintf(code, sizeof(code), ".%.*s", (int)k, p + 1);
      p += 1 + k;
    }
    if (*p == '@') {
      snprintf(mod, sizeof(mod), "%s", p);
    }
    /* _g_compute_locale_variants: every subset of the parts, most first */
    for (int mask = 7; mask >= 0 && n < max; mask--) {
      if (((mask & 4) && !mod[0]) || ((mask & 2) && !terr[0]) || ((mask & 1) && !code[0])) {
        continue;
      }
      snprintf(out[n++], 64, "%s%s%s%s", l, (mask & 2) ? terr : "", (mask & 1) ? code : "",
               (mask & 4) ? mod : "");
    }
  }
  bool has_c = false;
  for (int i = 0; i < n; i++) {
    has_c = has_c || !strcmp(out[i], "C");
  }
  if (!has_c && n < max) {
    snprintf(out[n++], 64, "C");
  }
  return n;
}

static char *comment_of(const char *path)
{
  char *text = awra_read_file(path, NULL);
  if (!text) {
    return NULL;
  }
  char langs[48][64];
  const int nl = language_names(langs, 48);
  char *best = NULL;
  int best_level = 0;
  for (char *p = strstr(text, "<comment"); p; p = strstr(p + 8, "<comment")) {
    char *gt = strchr(p, '>');
    if (!gt || (p[8] != '>' && p[8] != ' ' && p[8] != '\t' && p[8] != '\n')) {
      continue;
    }
    if (gt[-1] == '/') {
      continue;
    }
    const char *lang = "C";
    char lbuf[64];
    char *la = strstr(p, "xml:lang=");
    if (la && la < gt) {
      const char q = la[9];
      char *end = strchr(la + 10, q);
      if (end && end < gt) {
        snprintf(lbuf, sizeof(lbuf), "%.*s", (int)(end - la - 10), la + 10);
        lang = lbuf;
      }
    }
    int level = 0;
    for (int i = 0; i < nl; i++) {
      if (!strcmp(langs[i], lang)) {
        level = 1000 - i;
        break;
      }
    }
    char *close = strstr(gt, "</comment>");
    if (!close) {
      break;
    }
    if (level > best_level) {
      free(best);
      best = (char *)malloc((size_t)(close - gt));
      if (best) {
        memcpy(best, gt + 1, (size_t)(close - gt - 1));
        best[close - gt - 1] = 0;
        /* entities (GMarkup's) */
        char *w = best;
        for (char *r = best; *r;) {
          static const struct {
            const char *e;
            char c;
          } ents[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
          bool done = false;
          for (size_t e = 0; e < 5 && *r == '&'; e++) {
            const size_t el = strlen(ents[e].e);
            if (!strncmp(r, ents[e].e, el)) {
              *w++ = ents[e].c;
              r += el;
              done = true;
              break;
            }
          }
          if (!done) {
            *w++ = *r++;
          }
        }
        *w = 0;
      }
      best_level = level;
    }
  }
  free(text);
  return best;
}

char *awra_content_type_get_description(const char *type)
{
  if (!type) {
    return NULL;
  }
  load();
  const char *t = unalias(type);
  static struct {
    char *type, *text;
  } memo[64];
  static int nmemo;
  for (int i = 0; i < nmemo; i++) {
    if (!strcmp(memo[i].type, t)) {
      return awra_strdup(memo[i].text);
    }
  }
  char *comment = NULL;
  for (int i = 0; M.dirs && M.dirs[i] && !comment; i++) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s.xml", M.dirs[i], t);
    comment = comment_of(path);
  }
  if (!comment) {
    const size_t l = strlen(t);
    comment = (char *)malloc(l + 6);
    if (comment) {
      snprintf(comment, l + 6, "%s type", t);
    }
  }
  if (comment && nmemo < 64) {
    memo[nmemo].type = awra_strdup(t);
    memo[nmemo].text = awra_strdup(comment);
    if (memo[nmemo].type && memo[nmemo].text) {
      nmemo++;
    }
    else {
      free(memo[nmemo].type);
      free(memo[nmemo].text);
    }
  }
  return comment;
}
