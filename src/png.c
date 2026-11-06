/* A small PNG writer (stored deflate blocks): snapshots for tests and the
 * visual comparison tools, without an encoder dependency.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>

static uint32_t crc_table[256];

static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
  if (!crc_table[1]) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t k = i;
      for (int j = 0; j < 8; j++) {
        k = k & 1 ? 0xEDB88320u ^ (k >> 1) : k >> 1;
      }
      crc_table[i] = k;
    }
  }
  c = ~c;
  while (n--) {
    c = crc_table[(c ^ *p++) & 255] ^ (c >> 8);
  }
  return ~c;
}

static void be32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
  uint8_t h[8];
  be32(h, len);
  memcpy(h + 4, type, 4);
  fwrite(h, 1, 8, f);
  if (len) {
    fwrite(data, 1, len, f);
  }
  uint32_t c = crc(0, (const uint8_t *)type, 4);
  c = crc(c, data, len);
  uint8_t t[4];
  be32(t, c);
  fwrite(t, 1, 4, f);
}

bool awra_write_png(const char *path, const uint32_t *argb, int w, int h)
{
  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);
  uint8_t ihdr[13];
  be32(ihdr, (uint32_t)w);
  be32(ihdr + 4, (uint32_t)h);
  ihdr[8] = 8;
  ihdr[9] = 6; /* RGBA */
  ihdr[10] = ihdr[11] = ihdr[12] = 0;
  chunk(f, "IHDR", ihdr, 13);
  const size_t row = (size_t)w * 4 + 1, raw = row * (size_t)h;
  uint8_t *data = (uint8_t *)malloc(raw);
  if (!data) {
    fclose(f);
    return false;
  }
  for (int y = 0; y < h; y++) {
    uint8_t *o = data + row * (size_t)y;
    *o++ = 0;
    for (int x = 0; x < w; x++) {
      const uint32_t p = argb[(size_t)y * w + x];
      const uint32_t a = p >> 24;
      uint32_t r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
      if (a && a < 255) { /* un-premultiply */
        r = (r * 255 + a / 2) / a;
        g = (g * 255 + a / 2) / a;
        b = (b * 255 + a / 2) / a;
      }
      *o++ = (uint8_t)(r > 255 ? 255 : r);
      *o++ = (uint8_t)(g > 255 ? 255 : g);
      *o++ = (uint8_t)(b > 255 ? 255 : b);
      *o++ = (uint8_t)a;
    }
  }
  /* zlib stream of stored blocks */
  const size_t nblocks = raw / 65535 + 1;
  const size_t zlen = 2 + raw + nblocks * 5 + 4;
  uint8_t *z = (uint8_t *)malloc(zlen);
  if (!z) {
    free(data);
    fclose(f);
    return false;
  }
  size_t zi = 0;
  z[zi++] = 0x78;
  z[zi++] = 0x01;
  uint32_t s1 = 1, s2 = 0;
  for (size_t off = 0; off < raw || off == 0;) {
    const size_t n = raw - off > 65535 ? 65535 : raw - off;
    z[zi++] = off + n >= raw ? 1 : 0;
    z[zi++] = (uint8_t)(n & 255);
    z[zi++] = (uint8_t)(n >> 8);
    z[zi++] = (uint8_t)(~n & 255);
    z[zi++] = (uint8_t)((~n >> 8) & 255);
    memcpy(z + zi, data + off, n);
    for (size_t i = 0; i < n; i++) {
      s1 = (s1 + data[off + i]) % 65521;
      s2 = (s2 + s1) % 65521;
    }
    zi += n;
    off += n;
    if (n == 0) {
      break;
    }
  }
  be32(z + zi, (s2 << 16) | s1);
  zi += 4;
  chunk(f, "IDAT", z, (uint32_t)zi);
  chunk(f, "IEND", NULL, 0);
  free(z);
  free(data);
  fclose(f);
  return true;
}
