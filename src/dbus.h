/* A minimal D-Bus client (Linux): enough for AT-SPI2 without libdbus or
 * GLib. Unix-socket transports (path, abstract), EXTERNAL authentication,
 * asynchronous method calls with reply callbacks, signals, and a
 * marshaller / reader for the wire format. The socket is watched by the
 * window host's loop (BK_loop_watch_fd): no thread, nothing polled.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
  AWRA_DBUS_CALL = 1,
  AWRA_DBUS_RETURN = 2,
  AWRA_DBUS_ERROR = 3,
  AWRA_DBUS_SIGNAL = 4,
};

/* A received message; strings point into the connection's buffer and live
 * as long as the callback that gets it. */
typedef struct AwraDBusMsg {
  int type;
  unsigned flags;
  uint32_t serial, reply_serial;
  const char *path, *iface, *member, *error, *dest, *sender, *sig;
  const uint8_t *body;
  uint32_t body_len;
  bool big_endian;
} AwraDBusMsg;

typedef struct AwraDBus AwraDBus;
typedef void (*AwraDBusHandler)(AwraDBus *bus, const AwraDBusMsg *msg, void *data);
/* reply NULL: the connection closed before it came */
typedef void (*AwraDBusReply)(AwraDBus *bus, const AwraDBusMsg *reply, void *data);

typedef struct AwraDBusCallbacks {
  void (*ready)(AwraDBus *bus, void *data);  /* authenticated, Hello answered */
  void (*closed)(AwraDBus *bus, void *data); /* the bus went (the AwraDBus is freed after) */
  AwraDBusHandler message;                   /* calls and signals */
  void *data;
} AwraDBusCallbacks;

/* Connects (non-blocking from the caller's view: authentication and Hello
 * run from the loop). NULL when no address works. */
AwraDBus *awra_dbus_connect(const char *address, const AwraDBusCallbacks *cb);
/* The session bus (DBUS_SESSION_BUS_ADDRESS, else $XDG_RUNTIME_DIR/bus). */
const char *awra_dbus_session_address(void);
void awra_dbus_close(AwraDBus *bus);
const char *awra_dbus_unique_name(const AwraDBus *bus);

/* ----------------------------------------------------------------------- */
/* Writing bodies */

typedef struct AwraDBusW {
  uint8_t *b;
  size_t n, cap;
  bool oom;
} AwraDBusW;

void awra_dbus_w_init(AwraDBusW *w);
void awra_dbus_w_free(AwraDBusW *w);
void awra_dbus_w_align(AwraDBusW *w, int a);
void awra_dbus_w_byte(AwraDBusW *w, uint8_t v);
void awra_dbus_w_bool(AwraDBusW *w, bool v);
void awra_dbus_w_i16(AwraDBusW *w, int16_t v);
void awra_dbus_w_i32(AwraDBusW *w, int32_t v);
void awra_dbus_w_u32(AwraDBusW *w, uint32_t v);
void awra_dbus_w_double(AwraDBusW *w, double v);
/* strings and object paths (invalid UTF-8 is replaced), signatures */
void awra_dbus_w_str(AwraDBusW *w, const char *s);
void awra_dbus_w_str_len(AwraDBusW *w, const char *s, size_t len);
void awra_dbus_w_sig(AwraDBusW *w, const char *sig);
/* arrays: open writes the length's place and the padding, close patches
 * the length */
typedef struct AwraDBusArray {
  size_t at, start;
} AwraDBusArray;
AwraDBusArray awra_dbus_w_array_open(AwraDBusW *w, int elem_align);
void awra_dbus_w_array_close(AwraDBusW *w, AwraDBusArray a);
static inline void awra_dbus_w_struct(AwraDBusW *w)
{
  awra_dbus_w_align(w, 8);
}
/* a variant: its signature, then the caller writes the value */
static inline void awra_dbus_w_variant(AwraDBusW *w, const char *sig)
{
  awra_dbus_w_sig(w, sig);
}

/* ----------------------------------------------------------------------- */
/* Sending (body may be NULL with sig NULL) */

uint32_t awra_dbus_call(AwraDBus *bus, const char *dest, const char *path, const char *iface,
                        const char *member, const char *sig, const AwraDBusW *body,
                        AwraDBusReply reply, void *data);
/* flags: 2 = NO_AUTO_START */
uint32_t awra_dbus_call_flags(AwraDBus *bus, unsigned flags, const char *dest, const char *path,
                              const char *iface, const char *member, const char *sig,
                              const AwraDBusW *body, AwraDBusReply reply, void *data);
void awra_dbus_reply(AwraDBus *bus, const AwraDBusMsg *call, const char *sig,
                     const AwraDBusW *body);
void awra_dbus_error(AwraDBus *bus, const AwraDBusMsg *call, const char *name, const char *text);
void awra_dbus_signal(AwraDBus *bus, const char *path, const char *iface, const char *member,
                      const char *sig, const AwraDBusW *body);
/* org.freedesktop.DBus.AddMatch / RemoveMatch */
void awra_dbus_add_match(AwraDBus *bus, const char *rule);

/* ----------------------------------------------------------------------- */
/* Reading */

typedef struct AwraDBusR {
  const uint8_t *p;
  size_t n, pos;
  bool be, err;
} AwraDBusR;

void awra_dbus_r_init(AwraDBusR *r, const AwraDBusMsg *m);
uint8_t awra_dbus_r_byte(AwraDBusR *r);
bool awra_dbus_r_bool(AwraDBusR *r);
int32_t awra_dbus_r_i32(AwraDBusR *r);
uint32_t awra_dbus_r_u32(AwraDBusR *r);
double awra_dbus_r_double(AwraDBusR *r);
const char *awra_dbus_r_str(AwraDBusR *r); /* also object paths */
const char *awra_dbus_r_sig(AwraDBusR *r);
/* an array: true and *end set when it has elements to read until *end */
bool awra_dbus_r_array(AwraDBusR *r, int elem_align, size_t *end);
static inline void awra_dbus_r_struct(AwraDBusR *r)
{
  r->pos = (r->pos + 7) & ~(size_t)7;
}
/* skips one complete value of signature *sig, advancing *sig */
void awra_dbus_r_skip(AwraDBusR *r, const char **sig);
