/* BUI-Awra: what the window hosts share for data transfer: the clipboard's
 * (or a drag's) data held by type, the names plain text goes by, and file
 * lists as text/uri-list.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "kit_intern.h"
#include "kit_transfer.h"

#include <stdlib.h>
#include <string.h>

/* The names applications ask plain text by, ours first. */
static const char *const g_text_names[] = {"text/plain;charset=utf-8", "UTF8_STRING", "text/plain",
                                           "STRING", "TEXT", "text/plain;charset=UTF-8"};

int bk_mime_is_text(const char *mime)
{
  for (size_t i = 0; i < sizeof(g_text_names) / sizeof(g_text_names[0]); i++) {
    if (strcmp(mime, g_text_names[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

const char *bk_text_name(int i)
{
  return i >= 0 && i < (int)(sizeof(g_text_names) / sizeof(g_text_names[0])) ? g_text_names[i] :
                                                                                 NULL;
}

BKClipSet *bk_clipset_new(const BKClipItem *items, int n)
{
  if (n <= 0 || !items) {
    return NULL;
  }
  size_t bytes = sizeof(BKClipSet) + sizeof(BKClipEntry) * (size_t)n;
  for (int i = 0; i < n; i++) {
    if (!items[i].mime || (!items[i].data && items[i].len)) {
      return NULL;
    }
    bytes += strlen(items[i].mime) + 1 + items[i].len + 1;
  }
  /* one block: the entries, then each type's name and data */
  BKClipSet *s = (BKClipSet *)malloc(bytes);
  if (!s) {
    return NULL;
  }
  s->n = n;
  char *p = (char *)(s->e + n);
  for (int i = 0; i < n; i++) {
    const size_t ml = strlen(items[i].mime) + 1;
    memcpy(p, items[i].mime, ml);
    s->e[i].mime = p;
    p += ml;
    if (items[i].len) {
      memcpy(p, items[i].data, items[i].len);
    }
    p[items[i].len] = 0;
    s->e[i].data = (unsigned char *)p;
    s->e[i].len = items[i].len;
    p += items[i].len + 1;
  }
  return s;
}

void bk_clipset_free(BKClipSet *s)
{
  free(s);
}

const BKClipEntry *bk_clipset_get(const BKClipSet *s, const char *mime)
{
  if (!s || !mime) {
    return NULL;
  }
  for (int i = 0; i < s->n; i++) {
    if (strcmp(s->e[i].mime, mime) == 0) {
      return &s->e[i];
    }
  }
  if (bk_mime_is_text(mime)) {
    for (int i = 0; i < s->n; i++) {
      if (bk_mime_is_text(s->e[i].mime)) {
        return &s->e[i];
      }
    }
  }
  return NULL;
}

int bk_clipset_offers(const BKClipSet *s, const char **out, int max)
{
  int n = 0;
  int text = 0;
  for (int i = 0; s && i < s->n && n < max; i++) {
    if (bk_mime_is_text(s->e[i].mime)) {
      text = 1;
      continue;
    }
    out[n++] = s->e[i].mime;
  }
  for (int i = 0; text && bk_text_name(i) && n < max; i++) {
    out[n++] = bk_text_name(i);
  }
  return n;
}

char *bk_mimes_join(const char *const *mimes, int n)
{
  if (n <= 0) {
    return NULL;
  }
  size_t len = 0;
  for (int i = 0; i < n; i++) {
    len += strlen(mimes[i]) + 1;
  }
  char *out = (char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  char *p = out;
  for (int i = 0; i < n; i++) {
    const size_t l = strlen(mimes[i]);
    memcpy(p, mimes[i], l);
    p += l;
    *p++ = '\n';
  }
  p[-1] = 0;
  return out;
}

char *bk_clipset_mimes(const BKClipSet *s)
{
  const char *names[64];
  const int n = bk_clipset_offers(s, names, 64);
  return bk_mimes_join(names, n);
}

void *bk_bytes_dup(const void *data, size_t len, size_t *r_len)
{
  unsigned char *out = (unsigned char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  if (len) {
    memcpy(out, data, len);
  }
  out[len] = 0;
  if (r_len) {
    *r_len = len;
  }
  return out;
}

/* -- file lists ------------------------------------------------------------ */

static int hexval(int c)
{
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
         c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

char *bk_uri_to_path(const char *uri, size_t len)
{
  if (len < 7 || strncmp(uri, "file://", 7) != 0) {
    return NULL;
  }
  const char *p = uri + 7, *end = uri + len;
  /* file://host/path: the host part (empty or localhost) is skipped */
  if (p < end && *p != '/') {
    while (p < end && *p != '/') {
      p++;
    }
  }
  char *out = (char *)malloc((size_t)(end - p) + 1);
  if (!out) {
    return NULL;
  }
  size_t n = 0;
  for (; p < end; p++) {
    if (*p == '%' && p + 2 < end && hexval(p[1]) >= 0 && hexval(p[2]) >= 0) {
      out[n++] = (char)(hexval(p[1]) * 16 + hexval(p[2]));
      p += 2;
    }
    else {
      out[n++] = *p;
    }
  }
  out[n] = 0;
#ifdef _WIN32
  /* /C:/dir -> C:/dir */
  if (n >= 3 && out[0] == '/' && out[2] == ':') {
    memmove(out, out + 1, n);
  }
#endif
  return out;
}

char *bk_path_to_uri(const char *path)
{
  static const char hex[] = "0123456789ABCDEF";
  const size_t len = strlen(path);
  char *out = (char *)malloc(8 + len * 3 + 1);
  if (!out) {
    return NULL;
  }
  size_t n = 0;
  memcpy(out, "file://", 7);
  n = 7;
#ifdef _WIN32
  if (len >= 2 && path[1] == ':') {
    out[n++] = '/';
  }
#endif
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)path[i];
#ifdef _WIN32
    if (c == '\\') {
      c = '/';
    }
#endif
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        strchr("/-_.~!$&'()*+,;=:@", c))
    {
      out[n++] = (char)c;
    }
    else {
      out[n++] = '%';
      out[n++] = hex[c >> 4];
      out[n++] = hex[c & 15];
    }
  }
  out[n] = 0;
  return out;
}
