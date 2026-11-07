/* Awra 2 data transfer: the clipboard with images, file lists and any type
 * (GdkClipboard's set_texture / read_texture / FileList), PNG encoding of
 * textures, widget snapshots (GtkWidgetPaintable), the image that follows
 * the pointer during an in-application drag, and drags out to other
 * applications (the system's drag: Wayland, X11's XDND, OLE on Windows).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "kit_transfer.h"

#include <stdio.h>

BuiPixmap awra_texture_pixmap(const AwraTexture *t); /* texture.c */

/* ======================================================================= */
/* PNG encoding: filtered rows, deflated (LZ77 with hash chains, fixed
 * Huffman codes), in a zlib stream. */

typedef struct Bits {
  uint8_t *buf;
  size_t len, cap;
  uint32_t acc;
  int n;
  bool failed;
} Bits;

static void bits_byte(Bits *b, uint8_t v)
{
  if (b->len == b->cap) {
    const size_t cap = b->cap ? b->cap * 2 : 65536;
    uint8_t *g = (uint8_t *)realloc(b->buf, cap);
    if (!g) {
      b->failed = true;
      return;
    }
    b->buf = g;
    b->cap = cap;
  }
  b->buf[b->len++] = v;
}

/* `n` bits of `v`, least significant first (deflate's bit order) */
static void bits_put(Bits *b, uint32_t v, int n)
{
  b->acc |= v << b->n;
  b->n += n;
  while (b->n >= 8) {
    bits_byte(b, (uint8_t)b->acc);
    b->acc >>= 8;
    b->n -= 8;
  }
}

/* a Huffman code: its bits from the most significant */
static void bits_code(Bits *b, uint32_t code, int n)
{
  uint32_t r = 0;
  for (int i = 0; i < n; i++) {
    r = (r << 1) | ((code >> i) & 1);
  }
  bits_put(b, r, n);
}

static void put_literal(Bits *b, int c)
{
  if (c < 144) {
    bits_code(b, 0x30 + (uint32_t)c, 8);
  }
  else if (c < 256) {
    bits_code(b, 0x190 + (uint32_t)(c - 144), 9);
  }
  else if (c < 280) {
    bits_code(b, (uint32_t)(c - 256), 7);
  }
  else {
    bits_code(b, 0xC0 + (uint32_t)(c - 280), 8);
  }
}

static const uint16_t k_len_base[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t k_len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t k_dist_base[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                         33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                         1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t k_dist_extra[30] = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,
                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static void put_match(Bits *b, int len, int dist)
{
  int i = 28;
  while (k_len_base[i] > len) {
    i--;
  }
  put_literal(b, 257 + i);
  bits_put(b, (uint32_t)(len - k_len_base[i]), k_len_extra[i]);
  int d = 29;
  while (k_dist_base[d] > dist) {
    d--;
  }
  bits_code(b, (uint32_t)d, 5);
  bits_put(b, (uint32_t)(dist - k_dist_base[d]), k_dist_extra[d]);
}

#define LZ_WINDOW 32768
#define LZ_HASH (1 << 15)
#define LZ_CHAIN 48

static uint32_t lz_hash(const uint8_t *p)
{
  return ((uint32_t)p[0] << 16 ^ (uint32_t)p[1] << 8 ^ p[2]) * 2654435761u >> 17;
}

/* `src` deflated into `b` (one block with fixed codes). */
static void deflate_fixed(Bits *b, const uint8_t *src, size_t n)
{
  int32_t *head = (int32_t *)malloc(sizeof(int32_t) * LZ_HASH);
  int32_t *prev = (int32_t *)malloc(sizeof(int32_t) * LZ_WINDOW);
  if (!head || !prev) {
    free(head);
    free(prev);
    b->failed = true;
    return;
  }
  memset(head, 0xFF, sizeof(int32_t) * LZ_HASH);
  bits_put(b, 1, 1); /* BFINAL */
  bits_put(b, 1, 2); /* fixed Huffman */
  size_t i = 0;
  while (i < n) {
    int best = 0, best_d = 0;
    if (i + 3 <= n) {
      const uint32_t h = lz_hash(src + i);
      int32_t cand = head[h];
      const size_t max = n - i < 258 ? n - i : 258;
      for (int chain = 0; cand >= 0 && chain < LZ_CHAIN; chain++) {
        const size_t d = i - (size_t)cand;
        if (d > LZ_WINDOW - 1) {
          break;
        }
        if (src[cand + best] == src[i + best]) {
          size_t l = 0;
          while (l < max && src[cand + l] == src[i + l]) {
            l++;
          }
          if ((int)l > best) {
            best = (int)l;
            best_d = (int)d;
            if (l == max) {
              break;
            }
          }
        }
        cand = prev[cand & (LZ_WINDOW - 1)];
      }
      prev[i & (LZ_WINDOW - 1)] = head[h];
      head[h] = (int32_t)i;
    }
    if (best >= 3) {
      put_match(b, best, best_d);
      /* the skipped positions still go into the chains */
      for (size_t k = i + 1; k < i + (size_t)best && k + 3 <= n; k++) {
        const uint32_t h = lz_hash(src + k);
        prev[k & (LZ_WINDOW - 1)] = head[h];
        head[h] = (int32_t)k;
      }
      i += (size_t)best;
    }
    else {
      put_literal(b, src[i]);
      i++;
    }
  }
  put_literal(b, 256);
  if (b->n > 0) {
    bits_put(b, 0, 8 - b->n);
  }
  free(head);
  free(prev);
}

static uint32_t crc_png(uint32_t c, const uint8_t *p, size_t n)
{
  static uint32_t table[256];
  if (!table[1]) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t k = i;
      for (int j = 0; j < 8; j++) {
        k = k & 1 ? 0xEDB88320u ^ (k >> 1) : k >> 1;
      }
      table[i] = k;
    }
  }
  c = ~c;
  while (n--) {
    c = table[(c ^ *p++) & 255] ^ (c >> 8);
  }
  return ~c;
}

static void put_be32(Bits *b, uint32_t v)
{
  bits_byte(b, (uint8_t)(v >> 24));
  bits_byte(b, (uint8_t)(v >> 16));
  bits_byte(b, (uint8_t)(v >> 8));
  bits_byte(b, (uint8_t)v);
}

static void put_chunk(Bits *out, const char *type, const uint8_t *data, size_t len)
{
  put_be32(out, (uint32_t)len);
  const size_t at = out->len;
  for (int i = 0; i < 4; i++) {
    bits_byte(out, (uint8_t)type[i]);
  }
  for (size_t i = 0; i < len; i++) {
    bits_byte(out, data[i]);
  }
  put_be32(out, out->failed ? 0 : crc_png(0, out->buf + at, len + 4));
}

static int paeth(int a, int b, int c)
{
  const int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* An RGBA PNG of premultiplied 0xAARRGGBB pixels; malloc'd. */
void *awra_png_encode(const uint32_t *argb, int w, int h, size_t *r_len)
{
  *r_len = 0;
  if (!argb || w <= 0 || h <= 0 || (size_t)w * h > (1u << 28)) {
    return NULL;
  }
  const size_t stride = (size_t)w * 4;
  uint8_t *raw = (uint8_t *)malloc((stride + 1) * (size_t)h);
  uint8_t *rows[2] = {(uint8_t *)calloc(stride, 1), (uint8_t *)malloc(stride)};
  uint8_t *trial = (uint8_t *)malloc(stride);
  if (!raw || !rows[0] || !rows[1] || !trial) {
    free(raw);
    free(rows[0]);
    free(rows[1]);
    free(trial);
    return NULL;
  }
  /* straight RGBA, then each row's best filter (the least sum of
   * magnitudes, as libpng chooses) */
  for (int y = 0; y < h; y++) {
    uint8_t *cur = rows[(y + 1) & 1], *up = rows[y & 1];
    const uint32_t *src = argb + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      const uint32_t c = src[x], a = c >> 24;
      uint8_t *o = cur + (size_t)x * 4;
      if (a == 0) {
        o[0] = o[1] = o[2] = o[3] = 0;
      }
      else {
        const uint32_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
        o[0] = (uint8_t)(a == 255 ? r : (r * 255 + a / 2) / a);
        o[1] = (uint8_t)(a == 255 ? g : (g * 255 + a / 2) / a);
        o[2] = (uint8_t)(a == 255 ? b : (b * 255 + a / 2) / a);
        o[3] = (uint8_t)a;
      }
    }
    uint8_t *dst = raw + (size_t)y * (stride + 1);
    unsigned long best_sum = ~0ul;
    for (int f = 0; f < 5; f++) {
      unsigned long sum = 0;
      for (size_t i = 0; i < stride; i++) {
        const int left = i >= 4 ? cur[i - 4] : 0, above = y > 0 ? up[i] : 0,
                  ul = (i >= 4 && y > 0) ? up[i - 4] : 0;
        int v = cur[i];
        switch (f) {
          case 1: v -= left; break;
          case 2: v -= above; break;
          case 3: v -= (left + above) / 2; break;
          case 4: v -= paeth(left, above, ul); break;
          default: break;
        }
        trial[i] = (uint8_t)v;
        sum += (unsigned long)abs((int)(int8_t)trial[i]);
      }
      if (sum < best_sum) {
        best_sum = sum;
        dst[0] = (uint8_t)f;
        memcpy(dst + 1, trial, stride);
      }
    }
  }
  free(rows[0]);
  free(rows[1]);
  free(trial);
  const size_t rawlen = (stride + 1) * (size_t)h;
  /* zlib: header, deflate, Adler-32 */
  Bits z = {0};
  bits_byte(&z, 0x78);
  bits_byte(&z, 0x01);
  deflate_fixed(&z, raw, rawlen);
  uint32_t s1 = 1, s2 = 0;
  for (size_t i = 0; i < rawlen; i++) {
    s1 = (s1 + raw[i]) % 65521;
    s2 = (s2 + s1) % 65521;
  }
  free(raw);
  put_be32(&z, (s2 << 16) | s1);
  if (z.failed) {
    free(z.buf);
    return NULL;
  }
  Bits png = {0};
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  for (int i = 0; i < 8; i++) {
    bits_byte(&png, sig[i]);
  }
  uint8_t ihdr[13] = {(uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w,
                      (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h,
                      8, 6, 0, 0, 0};
  put_chunk(&png, "IHDR", ihdr, 13);
  put_chunk(&png, "IDAT", z.buf, z.len);
  free(z.buf);
  put_chunk(&png, "IEND", NULL, 0);
  if (png.failed) {
    free(png.buf);
    return NULL;
  }
  *r_len = png.len;
  return png.buf;
}

void *awra_texture_save_png_bytes(const AwraTexture *t, size_t *r_len)
{
  *r_len = 0;
  if (!t) {
    return NULL;
  }
  const BuiPixmap pm = awra_texture_pixmap(t);
  if (pm.format == BUI_PIXMAP_ARGB) {
    return awra_png_encode(pm.px, pm.w, pm.h, r_len);
  }
  /* video frames: their RGB first */
  uint32_t *px = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)pm.w * pm.h);
  if (!px) {
    return NULL;
  }
  BUI_pixmap_read(&pm, 0, pm.h, px);
  void *out = awra_png_encode(px, pm.w, pm.h, r_len);
  free(px);
  return out;
}

bool awra_texture_save_png(const AwraTexture *t, const char *path)
{
  size_t len;
  void *png = awra_texture_save_png_bytes(t, &len);
  if (!png || !path) {
    free(png);
    return false;
  }
  FILE *f = fopen(path, "wb");
  bool ok = f && fwrite(png, 1, len, f) == len;
  if (f && fclose(f) != 0) {
    ok = false;
  }
  free(png);
  return ok;
}

/* ======================================================================= */
/* Clipboard */

static BKWindow *bkw_of(AwraWidget *w)
{
  AwraWidget *win = w ? awra_window_of(w) : NULL;
  return win ? awra_window_bkw(win) : NULL;
}

bool awra_clipboard_set_data(AwraWidget *w, const char *const *mimes, const void *const *data,
                             const size_t *lens, int n)
{
  if (n <= 0 || n > 64 || !mimes || !data || !lens) {
    return false;
  }
  BKClipItem items[64];
  for (int i = 0; i < n; i++) {
    items[i] = (BKClipItem){mimes[i], data[i], lens[i]};
  }
  return BK_window_clipboard_set(bkw_of(w), items, n) != 0;
}

bool awra_clipboard_set_texture(AwraWidget *w, const AwraTexture *t)
{
  size_t len;
  void *png = awra_texture_save_png_bytes(t, &len);
  if (!png) {
    return false;
  }
  const BKClipItem it = {"image/png", png, len};
  const bool ok = BK_window_clipboard_set(bkw_of(w), &it, 1) != 0;
  free(png);
  return ok;
}

/* text/uri-list (CRLF), GNOME's copied-files list and the paths as text */
static bool file_items(const char *const *paths, int n, char **r_uris, char **r_gnome,
                       char **r_text)
{
  size_t ul = 0, tl = 0;
  char **uris = (char **)calloc((size_t)(n > 0 ? n : 1), sizeof(char *));
  if (!uris) {
    return false;
  }
  for (int i = 0; i < n; i++) {
    uris[i] = paths[i] ? bk_path_to_uri(paths[i]) : NULL;
    ul += uris[i] ? strlen(uris[i]) + 2 : 0;
    tl += paths[i] ? strlen(paths[i]) + 1 : 0;
  }
  char *u = (char *)malloc(ul + 1), *g = (char *)malloc(ul + 6), *t = (char *)malloc(tl + 1);
  bool ok = u && g && t;
  if (ok) {
    char *pu = u, *pg = g + 4, *pt = t;
    memcpy(g, "copy", 4);
    for (int i = 0; i < n; i++) {
      if (uris[i]) {
        const size_t l = strlen(uris[i]);
        memcpy(pu, uris[i], l);
        pu += l;
        *pu++ = '\r';
        *pu++ = '\n';
        *pg++ = '\n';
        memcpy(pg, uris[i], l);
        pg += l;
      }
      if (paths[i]) {
        const size_t l = strlen(paths[i]);
        memcpy(pt, paths[i], l);
        pt += l;
        *pt++ = '\n';
      }
    }
    *pu = 0;
    *pg = 0;
    if (pt > t) {
      pt--;
    }
    *pt = 0;
  }
  for (int i = 0; i < n; i++) {
    free(uris[i]);
  }
  free(uris);
  if (!ok) {
    free(u);
    free(g);
    free(t);
    return false;
  }
  *r_uris = u;
  *r_gnome = g;
  *r_text = t;
  return true;
}

bool awra_clipboard_set_files(AwraWidget *w, const char *const *paths, int n)
{
  char *uris, *gnome, *text;
  if (n <= 0 || !paths || !file_items(paths, n, &uris, &gnome, &text)) {
    return false;
  }
  const BKClipItem items[3] = {{"text/uri-list", uris, strlen(uris)},
                               {"text/plain;charset=utf-8", text, strlen(text)},
                               {"x-special/gnome-copied-files", gnome, strlen(gnome)}};
#ifdef _WIN32
  const int n_items = 2; /* CF_HDROP and CF_UNICODETEXT: GNOME's list means nothing there */
#else
  const int n_items = 3;
#endif
  const bool ok = BK_window_clipboard_set(bkw_of(w), items, n_items) != 0;
  free(uris);
  free(gnome);
  free(text);
  return ok;
}

char *awra_clipboard_formats(AwraWidget *w)
{
  return BK_window_clipboard_formats(bkw_of(w));
}

bool awra_clipboard_has(AwraWidget *w, const char *mime)
{
  char *f = awra_clipboard_formats(w);
  bool has = false;
  const size_t ml = mime ? strlen(mime) : 0;
  const bool prefix = ml > 1 && mime[ml - 1] == '*'; /* "image/*" */
  for (char *p = f; p && *p && !has;) {
    char *e = strchr(p, '\n');
    const size_t l = e ? (size_t)(e - p) : strlen(p);
    has = prefix ? (l >= ml - 1 && strncmp(p, mime, ml - 1) == 0) :
                   (l == ml && strncmp(p, mime, l) == 0);
    p = e ? e + 1 : p + l;
  }
  free(f);
  return has;
}

void *awra_clipboard_get_data(AwraWidget *w, const char *mime, size_t *r_len)
{
  size_t len = 0;
  void *out = BK_window_clipboard_read(bkw_of(w), mime, &len);
  if (r_len) {
    *r_len = len;
  }
  return out;
}

/* A DIB (Windows' CF_DIB / CF_DIBV5) or BMP file: 32 or 24 bits a pixel. */
static AwraTexture *dib_texture(const uint8_t *p, size_t len)
{
  if (len >= 14 && p[0] == 'B' && p[1] == 'M') { /* a file: past its header */
    p += 14;
    len -= 14;
  }
  if (len < 40) {
    return NULL;
  }
#define LE32(o) ((uint32_t)p[o] | (uint32_t)p[(o) + 1] << 8 | (uint32_t)p[(o) + 2] << 16 | (uint32_t)p[(o) + 3] << 24)
#define LE16(o) ((uint32_t)p[o] | (uint32_t)p[(o) + 1] << 8)
  const uint32_t hs = LE32(0), comp = LE32(16), bpp = LE16(14);
  const int w = (int)LE32(4), hraw = (int)LE32(8);
  const int h = hraw < 0 ? -hraw : hraw;
  if (w <= 0 || h <= 0 || w > 32768 || h > 32768 || (bpp != 32 && bpp != 24) ||
      (comp != 0 && comp != 3) || hs < 40 || hs > len)
  {
    return NULL;
  }
  size_t off = hs;
  if (comp == 3 && hs == 40) {
    off += 12; /* the BI_BITFIELDS masks after a plain header */
  }
  const size_t stride = ((size_t)w * bpp / 8 + 3) & ~(size_t)3;
  if (off + stride * (size_t)h > len) {
    return NULL;
  }
  uint32_t *px = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)w * h);
  if (!px) {
    return NULL;
  }
  bool any_alpha = false;
  for (int y = 0; y < h && bpp == 32; y++) {
    const uint8_t *row = p + off + stride * (size_t)y;
    for (int x = 0; x < w && !any_alpha; x++) {
      any_alpha = row[x * 4 + 3] != 0;
    }
  }
  for (int y = 0; y < h; y++) {
    const uint8_t *row = p + off + stride * (size_t)(hraw > 0 ? h - 1 - y : y);
    uint32_t *dst = px + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      const uint8_t *q = row + (size_t)x * (bpp / 8);
      const uint32_t a = bpp == 32 && any_alpha ? q[3] : 255;
      /* straight to premultiplied */
      dst[x] = (a << 24) | ((q[2] * a / 255) << 16) | ((q[1] * a / 255) << 8) | (q[0] * a / 255);
    }
  }
#undef LE32
#undef LE16
  AwraTexture *t = awra_texture_new_argb(px, w, h);
  free(px);
  return t;
}

AwraTexture *awra_clipboard_get_texture(AwraWidget *w)
{
  char *f = awra_clipboard_formats(w);
  if (!f) {
    return NULL;
  }
  AwraTexture *t = NULL;
  /* PNG first (lossless, alpha), then any image type the decoders read */
  static const char *const order[] = {"image/png", "image/webp", "image/jpeg", "image/gif",
                                      "image/svg+xml", "image/bmp", "image/x-bmp"};
  for (size_t i = 0; i < sizeof(order) / sizeof(order[0]) && !t; i++) {
    const size_t l = strlen(order[i]);
    bool listed = false;
    for (char *p = f; *p && !listed;) {
      char *e = strchr(p, '\n');
      const size_t pl = e ? (size_t)(e - p) : strlen(p);
      listed = pl == l && strncmp(p, order[i], l) == 0;
      p = e ? e + 1 : p + pl;
    }
    if (!listed) {
      continue;
    }
    size_t len;
    uint8_t *data = (uint8_t *)BK_window_clipboard_read(bkw_of(w), order[i], &len);
    if (data) {
      t = strstr(order[i], "bmp") ? dib_texture(data, len) : awra_texture_new_from_bytes(data, len);
      free(data);
    }
  }
  free(f);
  return t;
}

/* '\n'-separated local paths of a text/uri-list (or of GNOME's list) */
static char *uris_to_paths(const char *uris, size_t len, bool gnome)
{
  char *out = (char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  size_t n = 0;
  bool first = true;
  for (const char *p = uris, *end = uris + len; p < end;) {
    const char *eol = p;
    while (eol < end && *eol != '\n' && *eol != '\r') {
      eol++;
    }
    if (gnome && first) { /* "copy" or "cut" */
      first = false;
    }
    else if (eol > p && *p != '#') {
      char *path = bk_uri_to_path(p, (size_t)(eol - p));
      if (path) {
        const size_t l = strlen(path);
        if (n + l + 1 <= len) {
          if (n) {
            out[n++] = '\n';
          }
          memcpy(out + n, path, l);
          n += l;
        }
        free(path);
      }
    }
    p = eol;
    while (p < end && (*p == '\n' || *p == '\r')) {
      p++;
    }
  }
  out[n] = 0;
  if (!n) {
    free(out);
    return NULL;
  }
  return out;
}

char *awra_clipboard_get_files(AwraWidget *w)
{
  size_t len;
  char *u = (char *)BK_window_clipboard_read(bkw_of(w), "text/uri-list", &len);
  char *out = u ? uris_to_paths(u, len, false) : NULL;
  free(u);
  if (!out) {
    u = (char *)BK_window_clipboard_read(bkw_of(w), "x-special/gnome-copied-files", &len);
    out = u ? uris_to_paths(u, len, true) : NULL;
    free(u);
  }
  return out;
}

/* ======================================================================= */
/* Widget snapshots (GtkWidgetPaintable's current image) */

static void shift_tree(BKNode *n, int dx, int dy)
{
  n->x += dx;
  n->y += dy;
  for (int i = 0; i < n->nkids; i++) {
    shift_tree(n->kids[i], dx, dy);
  }
}

AwraTexture *awra_widget_snapshot(AwraWidget *w)
{
  if (!w || w->w <= 0 || w->h <= 0 || !(w->flags & NF_VISIBLE)) {
    return NULL;
  }
  const int ww = w->w, hh = w->h;
  if ((size_t)ww * hh > (1u << 26)) {
    return NULL;
  }
  BuiRaster *r = BUI_raster_create(ww, hh);
  if (!r) {
    return NULL;
  }
  BUI_raster_set_diff(r, 0);
  /* drawn at the origin of its own raster, as it draws in its window */
  const int dx = w->x, dy = w->y;
  shift_tree(w, -dx, -dy);
  BuiRaster *saved = bk_draw.r;
  bk_draw.r = r;
  bk_clip_reset(ww, hh);
  bk_draw_tree(w);
  bk_draw.r = saved;
  if (saved) {
    bk_clip_reset(BUI_raster_width(saved), BUI_raster_height(saved));
  }
  shift_tree(w, dx, dy);
  AwraTexture *t = awra_texture_new_argb(BUI_raster_pixels(r), ww, hh);
  BUI_raster_free(r);
  return t;
}

/* ======================================================================= */
/* The image following the pointer over a window (in-application drags) */

typedef struct DragIcon {
  AwraTexture *icon;
  int hot_x, hot_y;
  int x, y; /* the pointer, window raster px */
  bool shown;
} DragIcon;

static DragIcon *drag_icon_of(AwraWidget *win)
{
  return win ? (DragIcon *)awra_widget_get_data(win, "awra-drag-icon") : NULL;
}

static void drag_icon_damage(AwraWidget *win, const DragIcon *d)
{
  BKContext *ctx = awra_ctx(win);
  if (ctx && d->icon && d->shown) {
    bk_ctx_dirty_rect(ctx, bk_rect(d->x - d->hot_x, d->y - d->hot_y, awra_texture_width(d->icon),
                                   awra_texture_height(d->icon)));
    bk_ctx_request(ctx);
  }
}

static void drag_icon_draw(BKContext *ctx)
{
  AwraWidget *win = (AwraWidget *)ctx->user;
  const DragIcon *d = drag_icon_of(win);
  if (!d || !d->icon || !d->shown) {
    return;
  }
  const BuiPixmap pm = awra_texture_pixmap(d->icon);
  BUI_raster_image(bk_draw.r, (float)(d->x - d->hot_x), (float)(d->y - d->hot_y), (float)pm.w,
                   (float)pm.h, &pm, NULL, 1.0f, 0);
}

static void drag_icon_free(void *p)
{
  DragIcon *d = (DragIcon *)p;
  awra_texture_unref(d->icon);
  free(d);
}

void awra_window_set_drag_icon(AwraWidget *win, AwraTexture *icon, int hot_x, int hot_y)
{
  if (!awra_widget_is(win, "AwraWindow")) {
    return;
  }
  DragIcon *d = drag_icon_of(win);
  if (d) {
    drag_icon_damage(win, d);
  }
  BKContext *ctx = awra_ctx(win);
  if (!icon) {
    if (d) {
      awra_widget_set_data(win, "awra-drag-icon", NULL, NULL);
    }
    return;
  }
  if (!d) {
    d = (DragIcon *)calloc(1, sizeof(DragIcon));
    if (!d) {
      return;
    }
    awra_widget_set_data(win, "awra-drag-icon", d, drag_icon_free);
  }
  awra_texture_ref(icon);
  awra_texture_unref(d->icon);
  d->icon = icon;
  d->hot_x = hot_x;
  d->hot_y = hot_y;
  if (ctx) {
    ctx->overlay_hook = drag_icon_draw;
    d->x = ctx->pointer_x;
    d->y = ctx->pointer_y;
    d->shown = d->x > -50000 && d->y > -50000;
  }
  drag_icon_damage(win, d);
}

void awra_drag_icon_pointer(AwraWidget *win, int x, int y, bool left)
{
  DragIcon *d = drag_icon_of(win);
  if (!d || !d->icon) {
    return;
  }
  if (d->shown && (left || d->x != x || d->y != y)) {
    drag_icon_damage(win, d);
  }
  d->shown = !left;
  d->x = x;
  d->y = y;
  drag_icon_damage(win, d);
}

/* ======================================================================= */
/* Drags out of the application */

typedef struct DragDone {
  AwraWidget *source;
  AwraHandler done;
  void *data;
} DragDone;

static void drag_done(void *user, int action)
{
  DragDone *d = (DragDone *)user;
  if (d->done) {
    AwraArgs a = {0};
    a.signal = AWRA_SIGNAL_ACTIVATE;
    a.index = action;
    d->done(d->source, &a, d->data);
  }
  awra_widget_unref(d->source);
  free(d);
}

bool awra_drag_start(AwraWidget *source, const char *const *mimes, const void *const *data,
                     const size_t *lens, int n, AwraTexture *icon, int hot_x, int hot_y,
                     int actions, AwraHandler done, void *user)
{
  BKWindow *bkw = bkw_of(source);
  if (!bkw || n <= 0 || n > 64 || !mimes || !data || !lens) {
    return false;
  }
  BKClipItem items[64];
  for (int i = 0; i < n; i++) {
    items[i] = (BKClipItem){mimes[i], data[i], lens[i]};
  }
  DragDone *d = (DragDone *)calloc(1, sizeof(DragDone));
  if (!d) {
    return false;
  }
  d->source = source;
  d->done = done;
  d->data = user;
  awra_widget_ref(source);
  /* the in-window image hands over to the system's */
  AwraWidget *win = awra_window_of(source);
  awra_window_set_drag_icon(win, NULL, 0, 0);
  BuiPixmap pm = {0};
  uint32_t *px = NULL;
  if (icon) {
    pm = awra_texture_pixmap(icon);
    if (pm.format != BUI_PIXMAP_ARGB) {
      px = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)pm.w * pm.h);
      if (px) {
        BUI_pixmap_read(&pm, 0, pm.h, px);
      }
    }
  }
  const int ok = BK_window_drag_start(bkw, items, n, icon ? (px ? px : pm.px) : NULL, pm.w, pm.h,
                                      hot_x, hot_y, actions, drag_done, d);
  free(px);
  if (!ok) {
    awra_widget_unref(source);
    free(d);
    return false;
  }
  return true;
}

bool awra_drag_start_files(AwraWidget *source, const char *const *paths, int n,
                           AwraTexture *icon, int hot_x, int hot_y, int actions,
                           AwraHandler done, void *user)
{
  char *uris, *gnome, *text;
  if (n <= 0 || !paths || !file_items(paths, n, &uris, &gnome, &text)) {
    return false;
  }
  const char *mimes[2] = {"text/uri-list", "text/plain;charset=utf-8"};
  const void *data[2] = {uris, text};
  const size_t lens[2] = {strlen(uris), strlen(text)};
  const bool ok = awra_drag_start(source, mimes, data, lens, 2, icon, hot_x, hot_y, actions, done,
                                  user);
  free(uris);
  free(gnome);
  free(text);
  return ok;
}

/* ======================================================================= */
/* Default drag images (GtkDragSource without an icon): a label of the
 * text a drag carries, or an icon */

AwraTexture *awra_texture_new_for_text(const char *text)
{
  if (!text || !text[0]) {
    return NULL;
  }
  AwraWidget *l = awra_label_new(text);
  int mn, nw, nh;
  awra_measure(l, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nw);
  awra_measure(l, AWRA_ORIENTATION_VERTICAL, nw, &mn, &nh);
  awra_allocate(l, 0, 0, nw, nh);
  AwraTexture *t = awra_widget_snapshot(l);
  awra_widget_unref(l);
  return t;
}

AwraTexture *awra_texture_new_for_icon(const char *icon_name, int size)
{
  const int px = awra_px(size > 0 ? size : 32);
  const AwraIconMask *m = icon_name ? awra_icon_mask(icon_name, px) : NULL;
  if (!m || !m->a) {
    return NULL;
  }
  uint32_t *argb = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)m->w * m->h);
  if (!argb) {
    return NULL;
  }
  const AwraRGBA c = awra_tokens()->foreground;
  for (int i = 0; i < m->w * m->h; i++) {
    const float a = c.a * (float)m->a[i] / 255.0f;
    argb[i] = ((uint32_t)(a * 255.0f + 0.5f) << 24) | ((uint32_t)(c.r * a * 255.0f + 0.5f) << 16) |
              ((uint32_t)(c.g * a * 255.0f + 0.5f) << 8) | (uint32_t)(c.b * a * 255.0f + 0.5f);
  }
  AwraTexture *t = awra_texture_new_argb(argb, m->w, m->h);
  free(argb);
  return t;
}
