/* BUI-Awra: data transfer helpers the window hosts share (kit_transfer.c).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BUI_kit.h"

#include <stddef.h>

/* data held by type: one allocation (free with bk_clipset_free) */
typedef struct BKClipEntry {
  const char *mime;
  const unsigned char *data; /* NUL-terminated past len */
  size_t len;
} BKClipEntry;
typedef struct BKClipSet {
  int n;
  BKClipEntry e[];
} BKClipSet;

BKClipSet *bk_clipset_new(const BKClipItem *items, int n);
void bk_clipset_free(BKClipSet *s);
/* the entry for `mime`; plain text under any of its names */
const BKClipEntry *bk_clipset_get(const BKClipSet *s, const char *mime);
/* the types to offer: the set's, text under every name; returns the count */
int bk_clipset_offers(const BKClipSet *s, const char **out, int max);
/* the offered types, '\n'-separated (malloc'd, NULL none) */
char *bk_clipset_mimes(const BKClipSet *s);
char *bk_mimes_join(const char *const *mimes, int n);
/* plain text: one of its names (UTF-8) */
int bk_mime_is_text(const char *mime);
/* the i-th name of plain text, NULL past the last */
const char *bk_text_name(int i);
/* a malloc'd copy with a NUL past `len` */
void *bk_bytes_dup(const void *data, size_t len, size_t *r_len);
/* file:// URI <-> path (malloc'd; NULL when not a local file) */
char *bk_uri_to_path(const char *uri, size_t len);
char *bk_path_to_uri(const char *path);
