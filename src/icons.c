/* Awra 2 icons: symbolic SVGs (Awra's and Adwaita's, src/icons-data.c, or
 * registered by the app) rasterized once per size by BUI's nanosvg into
 * 8-bit coverage masks, tinted when drawn. Files and non-symbolic icons from
 * the system theme are decoded into pixmaps.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

/* kit/intern/image/format_svg.c */
BKImBuf *imb_svg_rasterize(const char *svg, int want_w, int want_h);

#include <stdio.h>

typedef struct AwraIconSource {
  const char *name;
  const char *svg;
} AwraIconSource;
extern const AwraIconSource awra_icon_sources[];

typedef struct Registered {
  char *name;
  char *svg;
  size_t len;
  struct Registered *next;
} Registered;
static Registered *g_registered;

typedef struct Cached {
  char name[256]; /* icon names, or the paths of color icons */
  int size;
  AwraIconMask mask;
  BuiPixmap pm; /* color icons */
  uint32_t *argb;
  struct Cached *next;
} Cached;

#define ICON_BUCKETS 64
static Cached *g_cache[ICON_BUCKETS];

static unsigned hash(const char *s, int size)
{
  unsigned h = 2166136261u ^ (unsigned)size * 16777619u;
  for (; *s; s++) {
    h = (h ^ (unsigned char)*s) * 16777619u;
  }
  return h % ICON_BUCKETS;
}

static const char *find_svg(const char *name, size_t *r_len)
{
  for (Registered *r = g_registered; r; r = r->next) {
    if (!strcmp(r->name, name)) {
      *r_len = r->len;
      return r->svg;
    }
  }
  /* sorted by name: binary search */
  int lo = 0, hi = 0;
  while (awra_icon_sources[hi].name) {
    hi++;
  }
  hi--;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const int c = strcmp(awra_icon_sources[mid].name, name);
    if (c == 0) {
      *r_len = strlen(awra_icon_sources[mid].svg);
      return awra_icon_sources[mid].svg;
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

bool awra_icon_register_svg(const char *name, const char *svg, size_t length)
{
  if (!name || !svg) {
    return false;
  }
  Registered *r = (Registered *)calloc(1, sizeof(Registered));
  if (!r) {
    return false;
  }
  r->name = awra_strdup(name);
  r->svg = (char *)malloc(length + 1);
  if (!r->svg) {
    free(r->name);
    free(r);
    return false;
  }
  memcpy(r->svg, svg, length);
  r->svg[length] = 0;
  r->len = length;
  r->next = g_registered;
  g_registered = r;
  /* drop cached rasterizations of that name */
  for (int b = 0; b < ICON_BUCKETS; b++) {
    for (Cached **p = &g_cache[b]; *p;) {
      if (!strcmp((*p)->name, name)) {
        Cached *c = *p;
        *p = c->next;
        free(c->mask.a);
        free(c->argb);
        free(c);
      }
      else {
        p = &(*p)->next;
      }
    }
  }
  return true;
}

bool awra_icon_exists(const char *name)
{
  size_t n;
  return name && find_svg(name, &n) != NULL;
}

/* Icon name aliases: a "-symbolic" name falls back to its plain form and
 * the reverse. */
static const char *resolve(const char *name, char *buf, size_t size, size_t *r_len)
{
  const char *svg = find_svg(name, r_len);
  if (svg) {
    return svg;
  }
  const size_t n = strlen(name);
  if (n > 9 && !strcmp(name + n - 9, "-symbolic")) {
    snprintf(buf, size, "%.*s", (int)(n - 9), name);
  }
  else {
    snprintf(buf, size, "%s-symbolic", name);
  }
  return find_svg(buf, r_len);
}

static Cached *lookup(const char *name, int size, bool create)
{
  const unsigned b = hash(name, size);
  for (Cached *c = g_cache[b]; c; c = c->next) {
    if (c->size == size && !strcmp(c->name, name)) {
      return c;
    }
  }
  if (!create || strlen(name) >= sizeof(((Cached *)0)->name)) {
    return NULL;
  }
  Cached *c = (Cached *)calloc(1, sizeof(Cached));
  if (!c) {
    return NULL;
  }
  snprintf(c->name, sizeof(c->name), "%s", name);
  c->size = size;
  c->next = g_cache[b];
  g_cache[b] = c;
  return c;
}

const AwraIconMask *awra_icon_mask(const char *name, int size)
{
  if (!name || !name[0] || size <= 0) {
    return NULL;
  }
  Cached *c = lookup(name, size, false);
  if (c) {
    return c->mask.a ? &c->mask : NULL;
  }
  c = lookup(name, size, true);
  if (!c) {
    return NULL;
  }
  char alt[96];
  size_t len = 0;
  const char *svg = resolve(name, alt, sizeof(alt), &len);
  if (!svg) {
    return NULL;
  }
  /* rasterized at the size asked (the generic decoder only shrinks) */
  BKImBuf *ib = imb_svg_rasterize(svg, size, size);
  (void)len;
  if (!ib) {
    return NULL;
  }
  int w = 0, h = 0;
  BK_imbuf_size(ib, &w, &h);
  const uint32_t *px = BK_imbuf_pixels(ib, 0);
  /* center a non-square result in a size x size mask */
  uint8_t *a = (uint8_t *)calloc((size_t)size * size, 1);
  if (a && px) {
    const int ox = (size - w) / 2, oy = (size - h) / 2;
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        const int tx = x + ox, ty = y + oy;
        if (tx >= 0 && ty >= 0 && tx < size && ty < size) {
          a[ty * size + tx] = (uint8_t)(px[y * w + x] >> 24);
        }
      }
    }
  }
  BK_imbuf_free(ib);
  c->mask.w = size;
  c->mask.h = size;
  c->mask.a = a;
  return a ? &c->mask : NULL;
}

void awra_paint_icon(AwraDraw *d, const char *name, float x, float y, int size,
                     const AwraRGBA *color)
{
  const AwraIconMask *m = awra_icon_mask(name, size);
  if (!m || !color) {
    return;
  }
  float c[4] = {color->r, color->g, color->b, color->a * d->opacity};
  BUI_raster_mask(d->r, (int)floorf(x + 0.5f), (int)floorf(y + 0.5f), m->a, m->w, m->h, m->w, c);
}

const BuiPixmap *awra_icon_pixmap(const char *name, int size)
{
  if (!name || size <= 0) {
    return NULL;
  }
  char key[256];
  if (strlen(name) + 2 > sizeof(key)) {
    return NULL;
  }
  snprintf(key, sizeof(key), "#%s", name);
  Cached *c = lookup(key, size, false);
  if (c) {
    return c->argb ? &c->pm : NULL;
  }
  c = lookup(key, size, true);
  if (!c) {
    return NULL;
  }
  BKImBuf *ib = BK_imbuf_load(name, size, size);
  if (!ib) {
    return NULL;
  }
  int w = 0, h = 0;
  BK_imbuf_size(ib, &w, &h);
  c->argb = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)w * h);
  if (c->argb) {
    memcpy(c->argb, BK_imbuf_pixels(ib, 0), sizeof(uint32_t) * (size_t)w * h);
    c->pm = (BuiPixmap){c->argb, w, h, 0, BUI_PIXMAP_ARGB, 0};
    c->pm.key = (uint64_t)(uintptr_t)c->argb;
  }
  BK_imbuf_free(ib);
  return c->argb ? &c->pm : NULL;
}
