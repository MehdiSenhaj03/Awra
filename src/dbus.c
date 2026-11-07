/* A minimal D-Bus client (see dbus.h).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _WIN32

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include "dbus.h"

#include "awra-private.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* ======================================================================= */
/* Writer */

void awra_dbus_w_init(AwraDBusW *w)
{
  memset(w, 0, sizeof(*w));
}

void awra_dbus_w_free(AwraDBusW *w)
{
  free(w->b);
  memset(w, 0, sizeof(*w));
}

static bool w_reserve(AwraDBusW *w, size_t extra)
{
  if (w->oom) {
    return false;
  }
  if (w->n + extra <= w->cap) {
    return true;
  }
  size_t cap = w->cap ? w->cap : 256;
  while (cap < w->n + extra) {
    cap *= 2;
  }
  uint8_t *b = (uint8_t *)realloc(w->b, cap);
  if (!b) {
    w->oom = true;
    return false;
  }
  w->b = b;
  w->cap = cap;
  return true;
}

static void w_put(AwraDBusW *w, const void *p, size_t n)
{
  if (w_reserve(w, n)) {
    memcpy(w->b + w->n, p, n);
    w->n += n;
  }
}

void awra_dbus_w_align(AwraDBusW *w, int a)
{
  const size_t pad = (size_t)((a - (int)(w->n % (size_t)a)) % a);
  if (pad && w_reserve(w, pad)) {
    memset(w->b + w->n, 0, pad);
    w->n += pad;
  }
}

void awra_dbus_w_byte(AwraDBusW *w, uint8_t v)
{
  w_put(w, &v, 1);
}

void awra_dbus_w_bool(AwraDBusW *w, bool v)
{
  awra_dbus_w_u32(w, v ? 1u : 0u);
}

void awra_dbus_w_i16(AwraDBusW *w, int16_t v)
{
  awra_dbus_w_align(w, 2);
  w_put(w, &v, 2);
}

void awra_dbus_w_i32(AwraDBusW *w, int32_t v)
{
  awra_dbus_w_align(w, 4);
  w_put(w, &v, 4);
}

void awra_dbus_w_u32(AwraDBusW *w, uint32_t v)
{
  awra_dbus_w_align(w, 4);
  w_put(w, &v, 4);
}

void awra_dbus_w_double(AwraDBusW *w, double v)
{
  awra_dbus_w_align(w, 8);
  w_put(w, &v, 8);
}

/* the length of the valid UTF-8 sequence at s (0: invalid) */
static int utf8_valid_at(const unsigned char *s, size_t left)
{
  const unsigned c = s[0];
  if (c < 0x80) {
    return 1;
  }
  int n;
  unsigned cp;
  if ((c & 0xE0) == 0xC0) {
    n = 2;
    cp = c & 0x1F;
  }
  else if ((c & 0xF0) == 0xE0) {
    n = 3;
    cp = c & 0x0F;
  }
  else if ((c & 0xF8) == 0xF0) {
    n = 4;
    cp = c & 0x07;
  }
  else {
    return 0;
  }
  if ((size_t)n > left) {
    return 0;
  }
  for (int i = 1; i < n; i++) {
    if ((s[i] & 0xC0) != 0x80) {
      return 0;
    }
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
      cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
  {
    return 0;
  }
  return n;
}

void awra_dbus_w_str_len(AwraDBusW *w, const char *s, size_t len)
{
  s = s ? s : "";
  const unsigned char *u = (const unsigned char *)s;
  /* the common case: valid, written as it is */
  size_t i = 0, fixed = 0;
  while (i < len) {
    const int k = u[i] ? utf8_valid_at(u + i, len - i) : 0;
    if (!k) {
      fixed++;
      i++;
    }
    else {
      i += (size_t)k;
    }
  }
  awra_dbus_w_align(w, 4);
  const uint32_t out_len = (uint32_t)(len + fixed * 2); /* U+FFFD: 3 bytes for 1 */
  w_put(w, &out_len, 4);
  if (!fixed) {
    w_put(w, s, len);
  }
  else {
    for (i = 0; i < len;) {
      const int k = u[i] ? utf8_valid_at(u + i, len - i) : 0;
      if (!k) {
        w_put(w, "\xEF\xBF\xBD", 3);
        i++;
      }
      else {
        w_put(w, u + i, (size_t)k);
        i += (size_t)k;
      }
    }
  }
  awra_dbus_w_byte(w, 0);
}

void awra_dbus_w_str(AwraDBusW *w, const char *s)
{
  awra_dbus_w_str_len(w, s ? s : "", s ? strlen(s) : 0);
}

void awra_dbus_w_sig(AwraDBusW *w, const char *sig)
{
  const size_t n = strlen(sig);
  awra_dbus_w_byte(w, (uint8_t)n);
  w_put(w, sig, n + 1);
}

AwraDBusArray awra_dbus_w_array_open(AwraDBusW *w, int elem_align)
{
  AwraDBusArray a;
  awra_dbus_w_u32(w, 0);
  a.at = w->n - 4;
  awra_dbus_w_align(w, elem_align); /* even when empty */
  a.start = w->n;
  return a;
}

void awra_dbus_w_array_close(AwraDBusW *w, AwraDBusArray a)
{
  if (!w->oom) {
    const uint32_t len = (uint32_t)(w->n - a.start);
    memcpy(w->b + a.at, &len, 4);
  }
}

/* ======================================================================= */
/* Reader */

void awra_dbus_r_init(AwraDBusR *r, const AwraDBusMsg *m)
{
  r->p = m->body;
  r->n = m->body_len;
  r->pos = 0;
  r->be = m->big_endian;
  r->err = false;
}

static void r_align(AwraDBusR *r, int a)
{
  r->pos = (r->pos + (size_t)a - 1) & ~(size_t)(a - 1);
}

static bool r_need(AwraDBusR *r, size_t n)
{
  if (r->err || r->pos + n > r->n) {
    r->err = true;
    return false;
  }
  return true;
}

uint8_t awra_dbus_r_byte(AwraDBusR *r)
{
  return r_need(r, 1) ? r->p[r->pos++] : 0;
}

uint32_t awra_dbus_r_u32(AwraDBusR *r)
{
  r_align(r, 4);
  if (!r_need(r, 4)) {
    return 0;
  }
  uint32_t v;
  memcpy(&v, r->p + r->pos, 4);
  r->pos += 4;
  if (r->be) {
    v = __builtin_bswap32(v);
  }
  return v;
}

int32_t awra_dbus_r_i32(AwraDBusR *r)
{
  return (int32_t)awra_dbus_r_u32(r);
}

bool awra_dbus_r_bool(AwraDBusR *r)
{
  return awra_dbus_r_u32(r) != 0;
}

double awra_dbus_r_double(AwraDBusR *r)
{
  r_align(r, 8);
  if (!r_need(r, 8)) {
    return 0.0;
  }
  uint64_t v;
  memcpy(&v, r->p + r->pos, 8);
  r->pos += 8;
  if (r->be) {
    v = __builtin_bswap64(v);
  }
  double d;
  memcpy(&d, &v, 8);
  return d;
}

const char *awra_dbus_r_str(AwraDBusR *r)
{
  const uint32_t len = awra_dbus_r_u32(r);
  if (!r_need(r, (size_t)len + 1) || r->p[r->pos + len] != 0) {
    r->err = true;
    return "";
  }
  const char *s = (const char *)r->p + r->pos;
  r->pos += len + 1;
  return s;
}

const char *awra_dbus_r_sig(AwraDBusR *r)
{
  const uint8_t len = awra_dbus_r_byte(r);
  if (!r_need(r, (size_t)len + 1) || r->p[r->pos + len] != 0) {
    r->err = true;
    return "";
  }
  const char *s = (const char *)r->p + r->pos;
  r->pos += (size_t)len + 1;
  return s;
}

bool awra_dbus_r_array(AwraDBusR *r, int elem_align, size_t *end)
{
  const uint32_t len = awra_dbus_r_u32(r);
  r_align(r, elem_align);
  if (r->err || r->pos + len > r->n) {
    r->err = true;
    return false;
  }
  *end = r->pos + len;
  return len > 0;
}

static int sig_align(char c)
{
  switch (c) {
    case 'y':
    case 'g':
    case 'v':
      return 1;
    case 'n':
    case 'q':
      return 2;
    case 'x':
    case 't':
    case 'd':
    case '(':
    case '{':
      return 8;
    default:
      return 4;
  }
}

/* the end of the complete type at sig */
static const char *sig_next(const char *sig)
{
  if (*sig == 'a') {
    return sig_next(sig + 1);
  }
  if (*sig == '(' || *sig == '{') {
    const char close = *sig == '(' ? ')' : '}';
    sig++;
    while (*sig && *sig != close) {
      sig = sig_next(sig);
    }
    return *sig ? sig + 1 : sig;
  }
  return *sig ? sig + 1 : sig;
}

void awra_dbus_r_skip(AwraDBusR *r, const char **psig)
{
  const char *sig = *psig;
  if (r->err || !*sig) {
    r->err = true;
    return;
  }
  switch (*sig) {
    case 'y':
      awra_dbus_r_byte(r);
      break;
    case 'n':
    case 'q':
      r_align(r, 2);
      if (r_need(r, 2)) {
        r->pos += 2;
      }
      break;
    case 'b':
    case 'i':
    case 'u':
    case 'h':
      awra_dbus_r_u32(r);
      break;
    case 'x':
    case 't':
    case 'd':
      awra_dbus_r_double(r);
      break;
    case 's':
    case 'o':
      awra_dbus_r_str(r);
      break;
    case 'g':
      awra_dbus_r_sig(r);
      break;
    case 'v': {
      const char *inner = awra_dbus_r_sig(r);
      awra_dbus_r_skip(r, &inner);
      break;
    }
    case 'a': {
      size_t end;
      const char *elem = sig + 1;
      if (awra_dbus_r_array(r, sig_align(*elem), &end)) {
        r->pos = end;
      }
      break;
    }
    case '(':
    case '{': {
      awra_dbus_r_struct(r);
      const char close = *sig == '(' ? ')' : '}';
      const char *s = sig + 1;
      while (*s && *s != close && !r->err) {
        awra_dbus_r_skip(r, &s);
      }
      break;
    }
    default:
      r->err = true;
      break;
  }
  *psig = sig_next(sig);
}

/* ======================================================================= */
/* Connection */

typedef struct Pending {
  uint32_t serial;
  AwraDBusReply fn;
  void *data;
  struct Pending *next;
} Pending;

enum { ST_AUTH, ST_HELLO, ST_READY, ST_CLOSED };

struct AwraDBus {
  int fd;
  int state;
  AwraDBusCallbacks cb;
  uint32_t serial;
  char *unique;
  uint8_t *in;
  size_t in_n, in_cap;
  Pending *pending;
  int busy;         /* inside the read callback */
  bool free_later;  /* closed from a callback: freed when it returns */
  bool remote_gone; /* tell `closed` */
};

const char *awra_dbus_session_address(void)
{
  static char buf[512];
  const char *a = getenv("DBUS_SESSION_BUS_ADDRESS");
  if (a && a[0]) {
    return a;
  }
  const char *rt = getenv("XDG_RUNTIME_DIR");
  if (rt && rt[0]) {
    snprintf(buf, sizeof(buf), "unix:path=%s/bus", rt);
    if (access(buf + 10, F_OK) == 0) {
      return buf;
    }
  }
  return NULL;
}

static int hexval(char c)
{
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
         c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* value of key in one address entry ("unix:path=...,guid=..."), unescaped */
static bool addr_value(const char *entry, size_t len, const char *key, char *out, size_t size)
{
  const char *p = memchr(entry, ':', len);
  if (!p) {
    return false;
  }
  p++;
  const char *end = entry + len;
  const size_t klen = strlen(key);
  while (p < end) {
    const char *comma = memchr(p, ',', (size_t)(end - p));
    const char *e = comma ? comma : end;
    if ((size_t)(e - p) > klen && !strncmp(p, key, klen) && p[klen] == '=') {
      size_t o = 0;
      for (const char *v = p + klen + 1; v < e && o + 1 < size; v++) {
        if (*v == '%' && v + 2 < e && hexval(v[1]) >= 0 && hexval(v[2]) >= 0) {
          out[o++] = (char)(hexval(v[1]) * 16 + hexval(v[2]));
          v += 2;
        }
        else {
          out[o++] = *v;
        }
      }
      out[o] = 0;
      return true;
    }
    p = e + 1;
  }
  return false;
}

static int connect_entry(const char *entry, size_t len)
{
  if (len < 5 || strncmp(entry, "unix:", 5) != 0) {
    return -1;
  }
  struct sockaddr_un sa;
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  char val[sizeof(sa.sun_path)];
  socklen_t salen;
  if (addr_value(entry, len, "path", val, sizeof(val))) {
    memcpy(sa.sun_path, val, strlen(val) + 1);
    salen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(val) + 1);
  }
  else if (addr_value(entry, len, "abstract", val, sizeof(val) - 1)) {
    sa.sun_path[0] = 0;
    memcpy(sa.sun_path + 1, val, strlen(val));
    salen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + strlen(val));
  }
  else {
    return -1;
  }
  const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return -1;
  }
  if (connect(fd, (struct sockaddr *)&sa, salen) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static bool send_all(AwraDBus *b, const void *p, size_t n)
{
  const uint8_t *c = (const uint8_t *)p;
  while (n > 0) {
    const ssize_t k = send(b->fd, c, n, MSG_NOSIGNAL);
    if (k < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    c += k;
    n -= (size_t)k;
  }
  return true;
}

static void on_readable(int fd, void *data);
static uint32_t send_message(AwraDBus *b, int type, unsigned flags, uint32_t reply_serial,
                             const char *dest, const char *path, const char *iface,
                             const char *member, const char *error, const char *sig,
                             const AwraDBusW *body);

static void hello_reply(AwraDBus *b, const AwraDBusMsg *m, void *data)
{
  if (!m || m->type != AWRA_DBUS_RETURN) {
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  const char *name = awra_dbus_r_str(&r);
  if (r.err) {
    return;
  }
  b->unique = awra_strdup(name);
  b->state = ST_READY;
  if (b->cb.ready) {
    b->cb.ready(b, b->cb.data);
  }
}

AwraDBus *awra_dbus_connect(const char *address, const AwraDBusCallbacks *cb)
{
  if (!address) {
    return NULL;
  }
  int fd = -1;
  for (const char *e = address; *e && fd < 0;) {
    const char *semi = strchr(e, ';');
    const size_t len = semi ? (size_t)(semi - e) : strlen(e);
    fd = connect_entry(e, len);
    e += len + (semi ? 1 : 0);
  }
  if (fd < 0) {
    return NULL;
  }
  AwraDBus *b = (AwraDBus *)calloc(1, sizeof(AwraDBus));
  if (!b) {
    close(fd);
    return NULL;
  }
  b->fd = fd;
  b->cb = *cb;
  b->state = ST_AUTH;
  /* EXTERNAL authentication with our uid, as hex of its decimal digits */
  char uid[32], line[128];
  snprintf(uid, sizeof(uid), "%u", (unsigned)getuid());
  int o = snprintf(line, sizeof(line), "AUTH EXTERNAL ");
  for (const char *c = uid; *c && o < (int)sizeof(line) - 4; c++) {
    o += snprintf(line + o, sizeof(line) - (size_t)o, "%02x", (unsigned char)*c);
  }
  o += snprintf(line + o, sizeof(line) - (size_t)o, "\r\n");
  const char nul = 0;
  if (!send_all(b, &nul, 1) || !send_all(b, line, (size_t)o) ||
      !BK_loop_watch_fd(fd, on_readable, b))
  {
    close(fd);
    free(b);
    return NULL;
  }
  return b;
}

const char *awra_dbus_unique_name(const AwraDBus *b)
{
  return b && b->unique ? b->unique : "";
}

static void really_free(AwraDBus *b)
{
  Pending *p = b->pending;
  b->pending = NULL;
  while (p) {
    Pending *next = p->next;
    if (p->fn) {
      p->fn(b, NULL, p->data);
    }
    free(p);
    p = next;
  }
  free(b->unique);
  free(b->in);
  free(b);
}

void awra_dbus_close(AwraDBus *b)
{
  if (!b || b->state == ST_CLOSED) {
    return;
  }
  b->state = ST_CLOSED;
  BK_loop_unwatch_fd(b->fd);
  close(b->fd);
  b->fd = -1;
  if (b->busy) {
    b->free_later = true;
    return;
  }
  really_free(b);
}

static void remote_closed(AwraDBus *b)
{
  if (b->state == ST_CLOSED) {
    return;
  }
  const AwraDBusCallbacks cb = b->cb;
  awra_dbus_close(b);
  if (cb.closed) {
    cb.closed(b, cb.data); /* b is valid until the read callback returns */
  }
}

static uint32_t rd32(const uint8_t *p, bool be)
{
  uint32_t v;
  memcpy(&v, p, 4);
  return be ? __builtin_bswap32(v) : v;
}

/* parses one complete message at p (len bytes) */
static bool parse_message(const uint8_t *p, size_t len, AwraDBusMsg *m)
{
  memset(m, 0, sizeof(*m));
  m->big_endian = p[0] == 'B';
  m->type = p[1];
  m->flags = p[2];
  m->body_len = rd32(p + 4, m->big_endian);
  m->serial = rd32(p + 8, m->big_endian);
  const uint32_t fields_len = rd32(p + 12, m->big_endian);
  AwraDBusR r = {p, 16 + (size_t)fields_len, 16, m->big_endian, false};
  const size_t end = 16 + fields_len;
  while (r.pos < end && !r.err) {
    awra_dbus_r_struct(&r);
    if (r.pos >= end) {
      break;
    }
    const uint8_t code = awra_dbus_r_byte(&r);
    const char *sig = awra_dbus_r_sig(&r);
    if (sig[0] == 'o' || sig[0] == 's') {
      const char *v = awra_dbus_r_str(&r);
      switch (code) {
        case 1: m->path = v; break;
        case 2: m->iface = v; break;
        case 3: m->member = v; break;
        case 4: m->error = v; break;
        case 6: m->dest = v; break;
        case 7: m->sender = v; break;
        default: break;
      }
    }
    else if (sig[0] == 'u') {
      const uint32_t v = awra_dbus_r_u32(&r);
      if (code == 5) {
        m->reply_serial = v;
      }
    }
    else if (sig[0] == 'g') {
      const char *v = awra_dbus_r_sig(&r);
      if (code == 8) {
        m->sig = v;
      }
    }
    else {
      const char *s = sig;
      awra_dbus_r_skip(&r, &s);
    }
  }
  if (r.err) {
    return false;
  }
  const size_t body = (end + 7) & ~(size_t)7;
  if (body + m->body_len > len) {
    return false;
  }
  m->body = p + body;
  if (!m->sig) {
    m->sig = "";
  }
  return true;
}

static void dispatch(AwraDBus *b, const AwraDBusMsg *m)
{
  if (m->type == AWRA_DBUS_RETURN || m->type == AWRA_DBUS_ERROR) {
    for (Pending **pp = &b->pending; *pp; pp = &(*pp)->next) {
      if ((*pp)->serial == m->reply_serial) {
        Pending *p = *pp;
        *pp = p->next;
        if (p->fn) {
          p->fn(b, m, p->data);
        }
        free(p);
        return;
      }
    }
    return;
  }
  if (b->cb.message) {
    b->cb.message(b, m, b->cb.data);
  }
}

static void process(AwraDBus *b)
{
  size_t off = 0;
  if (b->state == ST_AUTH) {
    const uint8_t *nl = NULL;
    for (size_t i = 0; i + 1 < b->in_n; i++) {
      if (b->in[i] == '\r' && b->in[i + 1] == '\n') {
        nl = b->in + i;
        break;
      }
    }
    if (!nl) {
      return;
    }
    if (b->in_n < 3 || strncmp((const char *)b->in, "OK ", 3) != 0) {
      remote_closed(b); /* REJECTED: no way in */
      return;
    }
    off = (size_t)(nl - b->in) + 2;
    b->state = ST_HELLO;
    if (!send_all(b, "BEGIN\r\n", 7)) {
      remote_closed(b);
      return;
    }
    send_message(b, AWRA_DBUS_CALL, 0, 0, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                 "org.freedesktop.DBus", "Hello", NULL, NULL, NULL);
    Pending *p = (Pending *)calloc(1, sizeof(Pending));
    if (p) {
      p->serial = b->serial;
      p->fn = hello_reply;
      p->next = b->pending;
      b->pending = p;
    }
  }
  while (b->state != ST_CLOSED && b->in_n - off >= 16) {
    const uint8_t *p = b->in + off;
    if ((p[0] != 'l' && p[0] != 'B') || p[3] != 1) {
      remote_closed(b);
      return;
    }
    const bool be = p[0] == 'B';
    const uint32_t body_len = rd32(p + 4, be), fields_len = rd32(p + 12, be);
    if (body_len > (64u << 20) || fields_len > (1u << 20)) {
      remote_closed(b);
      return;
    }
    const size_t total = ((16 + (size_t)fields_len + 7) & ~(size_t)7) + body_len;
    if (b->in_n - off < total) {
      break;
    }
    AwraDBusMsg m;
    if (parse_message(p, total, &m)) {
      dispatch(b, &m);
    }
    off += total;
  }
  if (b->state != ST_CLOSED && off > 0) {
    memmove(b->in, b->in + off, b->in_n - off);
    b->in_n -= off;
  }
}

static void on_readable(int fd, void *data)
{
  AwraDBus *b = (AwraDBus *)data;
  b->busy++;
  for (;;) {
    if (b->in_cap - b->in_n < 4096) {
      const size_t cap = b->in_cap ? b->in_cap * 2 : 8192;
      uint8_t *in = (uint8_t *)realloc(b->in, cap);
      if (!in) {
        remote_closed(b);
        break;
      }
      b->in = in;
      b->in_cap = cap;
    }
    const ssize_t k = recv(fd, b->in + b->in_n, b->in_cap - b->in_n, MSG_DONTWAIT);
    if (k > 0) {
      b->in_n += (size_t)k;
      continue;
    }
    if (k < 0 && errno == EINTR) {
      continue;
    }
    if (k == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
      process(b); /* what came before the end */
      remote_closed(b);
    }
    break;
  }
  if (b->state != ST_CLOSED) {
    process(b);
  }
  /* everything read: the buffer goes until the next message (an idle
   * connection holds no memory) */
  if (b->state != ST_CLOSED && b->in_n == 0 && b->in) {
    free(b->in);
    b->in = NULL;
    b->in_cap = 0;
  }
  b->busy--;
  if (b->free_later && !b->busy) {
    really_free(b);
  }
}

/* ----------------------------------------------------------------------- */
/* Sending */

static void w_field_str(AwraDBusW *w, uint8_t code, const char *sig, const char *v)
{
  awra_dbus_w_struct(w);
  awra_dbus_w_byte(w, code);
  awra_dbus_w_sig(w, sig);
  if (sig[0] == 'g') {
    awra_dbus_w_sig(w, v);
  }
  else {
    awra_dbus_w_str(w, v);
  }
}

static uint32_t send_message(AwraDBus *b, int type, unsigned flags, uint32_t reply_serial,
                             const char *dest, const char *path, const char *iface,
                             const char *member, const char *error, const char *sig,
                             const AwraDBusW *body)
{
  if (!b || b->state == ST_CLOSED || b->state == ST_AUTH) {
    return 0;
  }
  if (body && body->oom) {
    return 0;
  }
  const uint32_t serial = ++b->serial ? b->serial : ++b->serial;
  AwraDBusW h;
  awra_dbus_w_init(&h);
  awra_dbus_w_byte(&h, 'l');
  awra_dbus_w_byte(&h, (uint8_t)type);
  awra_dbus_w_byte(&h, (uint8_t)flags);
  awra_dbus_w_byte(&h, 1);
  awra_dbus_w_u32(&h, body ? (uint32_t)body->n : 0);
  awra_dbus_w_u32(&h, serial);
  const AwraDBusArray fields = awra_dbus_w_array_open(&h, 8);
  if (path) {
    w_field_str(&h, 1, "o", path);
  }
  if (iface) {
    w_field_str(&h, 2, "s", iface);
  }
  if (member) {
    w_field_str(&h, 3, "s", member);
  }
  if (error) {
    w_field_str(&h, 4, "s", error);
  }
  if (reply_serial) {
    awra_dbus_w_struct(&h);
    awra_dbus_w_byte(&h, 5);
    awra_dbus_w_sig(&h, "u");
    awra_dbus_w_u32(&h, reply_serial);
  }
  if (dest) {
    w_field_str(&h, 6, "s", dest);
  }
  if (sig && sig[0]) {
    w_field_str(&h, 8, "g", sig);
  }
  awra_dbus_w_array_close(&h, fields);
  awra_dbus_w_align(&h, 8);
  if (body && body->n) {
    w_put(&h, body->b, body->n);
  }
  bool ok = !h.oom && send_all(b, h.b, h.n);
  awra_dbus_w_free(&h);
  return ok ? serial : 0;
}

uint32_t awra_dbus_call_flags(AwraDBus *b, unsigned flags, const char *dest, const char *path,
                              const char *iface, const char *member, const char *sig,
                              const AwraDBusW *body, AwraDBusReply reply, void *data)
{
  const uint32_t serial = send_message(b, AWRA_DBUS_CALL, reply ? flags : flags | 1u, 0, dest,
                                       path, iface, member, NULL, sig, body);
  if (serial && reply) {
    Pending *p = (Pending *)calloc(1, sizeof(Pending));
    if (p) {
      p->serial = serial;
      p->fn = reply;
      p->data = data;
      p->next = b->pending;
      b->pending = p;
    }
  }
  else if (!serial && reply) {
    reply(b, NULL, data);
  }
  return serial;
}

uint32_t awra_dbus_call(AwraDBus *b, const char *dest, const char *path, const char *iface,
                        const char *member, const char *sig, const AwraDBusW *body,
                        AwraDBusReply reply, void *data)
{
  return awra_dbus_call_flags(b, 0, dest, path, iface, member, sig, body, reply, data);
}

void awra_dbus_reply(AwraDBus *b, const AwraDBusMsg *call, const char *sig, const AwraDBusW *body)
{
  if (call->flags & 1u) { /* NO_REPLY_EXPECTED */
    return;
  }
  send_message(b, AWRA_DBUS_RETURN, 1u, call->serial, call->sender, NULL, NULL, NULL, NULL, sig,
               body);
}

void awra_dbus_error(AwraDBus *b, const AwraDBusMsg *call, const char *name, const char *text)
{
  if (call->flags & 1u) {
    return;
  }
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_str(&w, text ? text : "");
  send_message(b, AWRA_DBUS_ERROR, 1u, call->serial, call->sender, NULL, NULL, NULL, name, "s",
               &w);
  awra_dbus_w_free(&w);
}

void awra_dbus_signal(AwraDBus *b, const char *path, const char *iface, const char *member,
                      const char *sig, const AwraDBusW *body)
{
  send_message(b, AWRA_DBUS_SIGNAL, 1u, 0, NULL, path, iface, member, NULL, sig, body);
}

void awra_dbus_add_match(AwraDBus *b, const char *rule)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_str(&w, rule);
  awra_dbus_call(b, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                 "AddMatch", "s", &w, NULL, NULL);
  awra_dbus_w_free(&w);
}

#endif /* !_WIN32 */
