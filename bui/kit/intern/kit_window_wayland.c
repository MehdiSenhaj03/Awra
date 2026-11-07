/* BUI-Awra: native window host for Wayland.
 *
 * The BK_window_* API (see kit_window_x11.c, kit_window_win32.c) on a
 * Wayland compositor, without XWayland:
 *  - xdg-shell toplevels, client-side decorated when asked (xdg-decoration),
 *    moved / resized / maximized through the compositor's interactive grabs;
 *  - frames are premultiplied ARGB8888 shm buffers the raster draws into
 *    directly (no copy): two (three when the compositor holds both), and a
 *    buffer about to be reused is first repaired from the newest one, only
 *    where later frames changed it;
 *  - one render per display frame (wl_surface.frame), none while hidden;
 *  - fractional scaling (wp-fractional-scale + viewporter), else the integer
 *    buffer scale of the surface's outputs;
 *  - compositor blur behind the window: ext-background-effect-v1, else
 *    KWin's org_kde_kwin_blur, over a rounded region;
 *  - keyboard through xkbcommon (X keysym names, as the X11 host reports
 *    them) with the compositor's key repeat; precise and discrete scrolling;
 *    cursor-shape-v1 cursors, else the cursor theme; the clipboard.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifdef BUI_WINDOW_WAYLAND
/* this host's BK_window_* are dispatched to (kit_window_unix.c) */
#  define BK_WIN_PREFIX bkwl
#  include "kit_window_unix.h"
#endif
#include "kit_intern.h"
#include "kit_loop.h"

#ifdef BUI_WINDOW_WAYLAND

#  include <errno.h>
#  include <fcntl.h>
#  include <linux/input-event-codes.h>
#  include <math.h>
#  include <poll.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <sys/mman.h>
#  include <time.h>
#  include <unistd.h>

#  include <wayland-client.h>
#  include <wayland-cursor.h>
#  include <xkbcommon/xkbcommon.h>

#  include "wayland/cursor-shape-v1-client.h"
#  include "wayland/ext-background-effect-v1-client.h"
#  include "wayland/fractional-scale-v1-client.h"
#  include "wayland/kde-blur-client.h"
#  include "wayland/viewporter-client.h"
#  include "wayland/xdg-decoration-unstable-v1-client.h"
#  include "wayland/xdg-shell-client.h"


#  define MAX_TIMERS 64
#  define MAX_BUFFERS 3

typedef struct Timer {
  int id; /* > 0 user timer, -1 the tooltip timer, -2 custom frames, 0 free */
  double due;
} Timer;

typedef struct Buffer {
  struct wl_buffer *wl;
  uint32_t *px;
  size_t size;
  int w, h;
  int busy; /* the compositor still reads it */
  /* what later frames changed since this buffer was drawn (union, buffer px) */
  int stale[4]; /* x0 y0 x1 y1; x1 <= x0: up to date */
  int valid;    /* holds a complete frame */
} Buffer;

struct BKWindow {
  BKContext *ctx;
  BKHost prev;
  BuiRaster *raster;
  BKWindowCallbacks cb;
  int flags;
  char *title, *app_id;

  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *toplevel;
  struct zxdg_toplevel_decoration_v1 *deco;
  struct wp_viewport *viewport;
  struct wp_fractional_scale_v1 *fscale;
  struct ext_background_effect_surface_v1 *effect;
  struct org_kde_kwin_blur *kblur;
  struct wl_callback *frame_cb;

  Buffer buffers[MAX_BUFFERS];
  int front; /* index of the buffer last committed, -1 none */

  int mapped;     /* role objects exist (shown) */
  int configured; /* the first configure arrived */
  int open;
  int lw, lh;          /* logical size */
  int pending_w, pending_h, pending_states, has_pending;
  int min_w, min_h, max_w, max_h;
  int scale120;   /* fractional scale * 120 (0: integer scaling) */
  int int_scale;  /* integer buffer scale */
  int w, h;       /* buffer pixels */
  int states;     /* BK_WSTATE_* */

  int render_pending, full, frames;
  int want_frame, frame_vsync;
  double frame_start;
  int cursor, tip_id, next_timer;
  Timer timers[MAX_TIMERS];

  int blur[5]; /* x y w h radius (logical) */
  int blur_dirty;

  struct BKWindow *next;
};

/* -- globals -- */

static struct wl_display *g_dpy;
static struct wl_registry *g_registry;
static struct wl_compositor *g_compositor;
static uint32_t g_compositor_version;
static struct wl_shm *g_shm;
static struct xdg_wm_base *g_wm_base;
static struct wl_seat *g_seat;
static uint32_t g_seat_version;
static struct wl_pointer *g_pointer;
static struct wl_keyboard *g_keyboard;
static struct wl_data_device_manager *g_ddm;
static struct wl_data_device *g_data_device;
static struct wp_cursor_shape_manager_v1 *g_cursor_shape_mgr;
static struct wp_cursor_shape_device_v1 *g_cursor_shape_dev;
static struct wp_fractional_scale_manager_v1 *g_fscale_mgr;
static struct wp_viewporter *g_viewporter;
static struct ext_background_effect_manager_v1 *g_effect_mgr;
static int g_effect_caps = -1; /* -1 not announced yet */
static struct org_kde_kwin_blur_manager *g_kblur_mgr;
static struct zxdg_decoration_manager_v1 *g_deco_mgr;

/* outputs: their integer scale, for surfaces without fractional scaling */
typedef struct Output {
  struct wl_output *wl;
  uint32_t name;
  int scale;
  int w, h;
  struct Output *next;
} Output;
static Output *g_outputs;

static BKWindow *g_windows;
static int g_open, g_quit, g_keep_alive;
static int (*g_stop)(void);
static void (*g_wake_cb)(void);
static int g_wake_pipe[2] = {-1, -1};

/* pointer */
static BKWindow *g_pointer_win;
static uint32_t g_pointer_serial, g_last_press_serial;
static double g_px, g_py; /* surface-local logical */
static int g_buttons;     /* BK_BUTTON* held */
static float g_axis_x, g_axis_y;   /* this frame's scroll, notches */
static int g_axis_discrete;        /* the frame had wheel clicks */
static int g_axis_has;
static struct wl_cursor_theme *g_cursor_theme;
static int g_cursor_theme_size;
static struct wl_surface *g_cursor_surface;

/* keyboard */
static struct xkb_context *g_xkb;
static struct xkb_keymap *g_keymap;
static struct xkb_state *g_xkb_state;
static BKWindow *g_key_win;
static int g_repeat_rate = 25, g_repeat_delay = 600;
static uint32_t g_repeat_key; /* evdev code + 8, 0: none */
static double g_repeat_due;
static int g_mods;

/* clipboard */
static struct wl_data_offer *g_sel_offer; /* the current selection, someone else's */
static int g_sel_offer_text;
static struct wl_data_source *g_source;
static char *g_clip_text;
static uint32_t g_key_serial;

static double now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static double win_scale(const BKWindow *win)
{
  return win->scale120 ? win->scale120 / 120.0 : (double)win->int_scale;
}

/* ----------------------------------------------------------------------- */
/* Outputs */

static void output_geometry(void *data, struct wl_output *o, int32_t x, int32_t y, int32_t pw,
                            int32_t ph, int32_t sub, const char *make, const char *model,
                            int32_t transform)
{
}
static void output_mode(void *data, struct wl_output *o, uint32_t flags, int32_t w, int32_t h,
                        int32_t refresh)
{
  Output *out = (Output *)data;
  if (flags & WL_OUTPUT_MODE_CURRENT) {
    out->w = w;
    out->h = h;
  }
}
static void output_done(void *data, struct wl_output *o) {}
static void output_scale(void *data, struct wl_output *o, int32_t factor)
{
  ((Output *)data)->scale = factor > 0 ? factor : 1;
}
static void output_name(void *data, struct wl_output *o, const char *name) {}
static void output_description(void *data, struct wl_output *o, const char *d) {}
static const struct wl_output_listener g_output_listener = {
    output_geometry, output_mode, output_done, output_scale, output_name, output_description};

/* ----------------------------------------------------------------------- */
/* Buffers */

static void buffer_release(void *data, struct wl_buffer *b)
{
  ((Buffer *)data)->busy = 0;
}
static const struct wl_buffer_listener g_buffer_listener = {buffer_release};

static void buffer_free(Buffer *b)
{
  if (b->wl) {
    wl_buffer_destroy(b->wl);
  }
  if (b->px) {
    munmap(b->px, b->size);
  }
  memset(b, 0, sizeof(*b));
}

static int buffer_alloc(Buffer *b, int w, int h)
{
  buffer_free(b);
  const int stride = w * 4;
  const size_t size = (size_t)stride * (size_t)h;
  const int fd = memfd_create("bui-frame", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) {
    return 0;
  }
  if (ftruncate(fd, (off_t)size) != 0) {
    close(fd);
    return 0;
  }
  void *px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (px == MAP_FAILED) {
    close(fd);
    return 0;
  }
  struct wl_shm_pool *pool = wl_shm_create_pool(g_shm, fd, (int32_t)size);
  b->wl = wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);
  wl_buffer_add_listener(b->wl, &g_buffer_listener, b);
  b->px = (uint32_t *)px;
  b->size = size;
  b->w = w;
  b->h = h;
  b->busy = 0;
  b->valid = 0;
  b->stale[0] = b->stale[1] = b->stale[2] = b->stale[3] = 0;
  return 1;
}

static void buffers_free(BKWindow *win)
{
  for (int i = 0; i < MAX_BUFFERS; i++) {
    buffer_free(&win->buffers[i]);
  }
  win->front = -1;
}

/* A buffer the compositor doesn't hold, of the window's size (allocated or
 * reallocated as needed); NULL when every one is busy. */
static Buffer *buffer_next(BKWindow *win)
{
  Buffer *pick = NULL;
  for (int i = 0; i < MAX_BUFFERS; i++) {
    Buffer *b = &win->buffers[i];
    if (i == win->front || b->busy) {
      continue;
    }
    /* the third buffer only comes into use when the first two are held */
    if (!b->wl && i == 2 && (!win->buffers[0].wl || !win->buffers[1].wl)) {
      continue;
    }
    if (b->wl && b->w == win->w && b->h == win->h) {
      return b;
    }
    if (!pick) {
      pick = b;
    }
  }
  if (pick && !buffer_alloc(pick, win->w, win->h)) {
    return NULL;
  }
  return pick;
}

/* ----------------------------------------------------------------------- */
/* Host callbacks */

static void host_request(void *data)
{
  BKWindow *win = (BKWindow *)data;
  if (win->open) {
    win->render_pending = 1;
  }
}

static const char *const g_cursor_names[] = {
    "default", "ew-resize", "ns-resize", "text", "crosshair", "pointer", "move",
    NULL /* none */, "nesw-resize", "nwse-resize", "not-allowed", "wait", "progress"};
static const uint32_t g_cursor_shapes[] = {
    WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT,     WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE,
    WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE,   WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT,
    WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR,   WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER,
    WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE,        0,
    WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE,
    WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT,
    WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS};

static void apply_cursor(BKWindow *win)
{
  if (!g_pointer || g_pointer_win != win) {
    return;
  }
  const int c = BK_CLAMP(win->cursor, 0, 12);
  if (c == BK_CURSOR_NONE) {
    wl_pointer_set_cursor(g_pointer, g_pointer_serial, NULL, 0, 0);
    return;
  }
  if (g_cursor_shape_dev) {
    wp_cursor_shape_device_v1_set_shape(g_cursor_shape_dev, g_pointer_serial, g_cursor_shapes[c]);
    return;
  }
  /* the cursor theme, at the window's integer scale */
  const int scale = (int)ceil(win_scale(win));
  const char *env = getenv("XCURSOR_SIZE");
  const int size = (env && atoi(env) > 0 ? atoi(env) : 24) * scale;
  if (!g_cursor_theme || g_cursor_theme_size != size) {
    if (g_cursor_theme) {
      wl_cursor_theme_destroy(g_cursor_theme);
    }
    g_cursor_theme = wl_cursor_theme_load(getenv("XCURSOR_THEME"), size, g_shm);
    g_cursor_theme_size = size;
  }
  if (!g_cursor_theme) {
    return;
  }
  struct wl_cursor *cur = wl_cursor_theme_get_cursor(g_cursor_theme, g_cursor_names[c]);
  if (!cur) {
    cur = wl_cursor_theme_get_cursor(g_cursor_theme, "left_ptr");
  }
  if (!cur || cur->image_count == 0) {
    return;
  }
  struct wl_cursor_image *img = cur->images[0];
  if (!g_cursor_surface) {
    g_cursor_surface = wl_compositor_create_surface(g_compositor);
  }
  wl_surface_set_buffer_scale(g_cursor_surface, scale);
  wl_surface_attach(g_cursor_surface, wl_cursor_image_get_buffer(img), 0, 0);
  wl_surface_damage(g_cursor_surface, 0, 0, INT32_MAX, INT32_MAX);
  wl_surface_commit(g_cursor_surface);
  wl_pointer_set_cursor(g_pointer, g_pointer_serial, g_cursor_surface,
                        (int32_t)img->hotspot_x / scale, (int32_t)img->hotspot_y / scale);
}

static void host_set_cursor(void *data, int c)
{
  BKWindow *win = (BKWindow *)data;
  if (win->cursor == c) {
    return;
  }
  win->cursor = c;
  apply_cursor(win);
}

static void host_warp(void *data, int x, int y)
{
  /* Wayland clients can't move the pointer (pointer-warp is not common
   * yet): continuous drags keep working from the relative motion */
}

static char *host_clipboard_get(void *data)
{
  if (g_source && g_clip_text) {
    return strdup(g_clip_text); /* our own selection */
  }
  if (!g_sel_offer || !g_sel_offer_text) {
    return NULL;
  }
  int fds[2];
  if (pipe2(fds, O_CLOEXEC) != 0) {
    return NULL;
  }
  wl_data_offer_receive(g_sel_offer, g_sel_offer_text == 2 ? "text/plain;charset=utf-8" :
                                                              "text/plain",
                        fds[1]);
  close(fds[1]);
  wl_display_flush(g_dpy);
  size_t len = 0, cap = 4096;
  char *buf = (char *)malloc(cap + 1);
  const double until = now_ms() + 1000.0;
  while (buf) {
    struct pollfd p = {fds[0], POLLIN, 0};
    const int left = (int)(until - now_ms());
    if (left <= 0 || poll(&p, 1, left) <= 0) {
      break;
    }
    if (len == cap) {
      char *grown = (char *)realloc(buf, cap * 2 + 1);
      if (!grown) {
        break;
      }
      buf = grown;
      cap *= 2;
    }
    const ssize_t n = read(fds[0], buf + len, cap - len);
    if (n <= 0) {
      break;
    }
    len += (size_t)n;
  }
  close(fds[0]);
  if (buf) {
    buf[len] = 0;
  }
  return buf;
}

static void source_target(void *data, struct wl_data_source *s, const char *mime) {}
static void source_send(void *data, struct wl_data_source *s, const char *mime, int32_t fd)
{
  if (g_clip_text) {
    const char *p = g_clip_text;
    size_t left = strlen(p);
    while (left > 0) {
      const ssize_t n = write(fd, p, left);
      if (n <= 0) {
        break;
      }
      p += n;
      left -= (size_t)n;
    }
  }
  close(fd);
}
static void source_cancelled(void *data, struct wl_data_source *s)
{
  if (s == g_source) {
    g_source = NULL;
  }
  wl_data_source_destroy(s);
}
static void source_dnd_drop_performed(void *data, struct wl_data_source *s) {}
static void source_dnd_finished(void *data, struct wl_data_source *s) {}
static void source_action(void *data, struct wl_data_source *s, uint32_t a) {}
static const struct wl_data_source_listener g_source_listener = {
    source_target,          source_send,         source_cancelled,
    source_dnd_drop_performed, source_dnd_finished, source_action};

static void host_clipboard_set(void *data, const char *text)
{
  free(g_clip_text);
  g_clip_text = strdup(text ? text : "");
  if (!g_ddm || !g_data_device) {
    return;
  }
  struct wl_data_source *s = wl_data_device_manager_create_data_source(g_ddm);
  wl_data_source_add_listener(s, &g_source_listener, NULL);
  wl_data_source_offer(s, "text/plain;charset=utf-8");
  wl_data_source_offer(s, "text/plain");
  wl_data_source_offer(s, "UTF8_STRING");
  wl_data_device_set_selection(g_data_device, s, g_key_serial);
  if (g_source) {
    wl_data_source_destroy(g_source);
  }
  g_source = s;
  wl_display_flush(g_dpy);
}

static Timer *timer_slot(BKWindow *win, int id)
{
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (win->timers[i].id == id) {
      return &win->timers[i];
    }
  }
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (win->timers[i].id == 0) {
      return &win->timers[i];
    }
  }
  return NULL;
}

static void host_schedule(void *data, int ms, int timer)
{
  BKWindow *win = (BKWindow *)data;
  const int slot = timer == BK_TIMER_CUSTOM ? -2 : -1;
  if (slot == -1) {
    win->tip_id = timer;
  }
  Timer *t = timer_slot(win, slot);
  if (t) {
    t->id = slot;
    t->due = now_ms() + ms;
  }
}

static void host_emit(void *data, BKNode *node, int kind, void *ref, double a, double b)
{
  BKWindow *win = (BKWindow *)data;
  if (win->prev.emit) {
    win->prev.emit(win->prev.data, node, kind, ref, a, b);
  }
}

/* ----------------------------------------------------------------------- */
/* Blur */

/* A rounded box as scanline rectangles (surface-local logical pixels). */
static struct wl_region *rounded_region(int x, int y, int w, int h, int r)
{
  struct wl_region *region = wl_compositor_create_region(g_compositor);
  r = BK_MAX(0, BK_MIN(r, BK_MIN(w, h) / 2));
  if (r == 0) {
    wl_region_add(region, x, y, w, h);
    return region;
  }
  wl_region_add(region, x, y + r, w, h - 2 * r);
  for (int i = 0; i < r; i++) {
    /* the row's half-width inside a corner circle, measured at its center */
    const double dy = r - i - 0.5;
    const int inset = (int)floor(r - sqrt((double)r * r - dy * dy) + 0.5);
    wl_region_add(region, x + inset, y + i, w - 2 * inset, 1);
    wl_region_add(region, x + inset, y + h - i - 1, w - 2 * inset, 1);
  }
  return region;
}

static int blur_supported(void)
{
  return (g_effect_mgr && g_effect_caps > 0 && (g_effect_caps & 1)) || g_kblur_mgr;
}

static void apply_blur(BKWindow *win)
{
  if (!win->surface || !win->blur_dirty) {
    return;
  }
  win->blur_dirty = 0;
  const int on = win->blur[2] > 0 && win->blur[3] > 0 && (win->flags & BK_WINDOW_TRANSPARENT);
  if (g_effect_mgr && g_effect_caps > 0 && (g_effect_caps & 1)) {
    if (!win->effect) {
      win->effect = ext_background_effect_manager_v1_get_background_effect(g_effect_mgr,
                                                                             win->surface);
    }
    if (on) {
      struct wl_region *r = rounded_region(win->blur[0], win->blur[1], win->blur[2], win->blur[3],
                                           win->blur[4]);
      ext_background_effect_surface_v1_set_blur_region(win->effect, r);
      wl_region_destroy(r);
    }
    else {
      ext_background_effect_surface_v1_set_blur_region(win->effect, NULL);
    }
    return;
  }
  if (g_kblur_mgr) {
    if (on) {
      if (!win->kblur) {
        win->kblur = org_kde_kwin_blur_manager_create(g_kblur_mgr, win->surface);
      }
      struct wl_region *r = rounded_region(win->blur[0], win->blur[1], win->blur[2], win->blur[3],
                                           win->blur[4]);
      org_kde_kwin_blur_set_region(win->kblur, r);
      org_kde_kwin_blur_commit(win->kblur);
      wl_region_destroy(r);
    }
    else if (win->kblur) {
      org_kde_kwin_blur_release(win->kblur);
      win->kblur = NULL;
      org_kde_kwin_blur_manager_unset(g_kblur_mgr, win->surface);
    }
  }
}

/* ----------------------------------------------------------------------- */
/* Rendering */

static void frame_done(void *data, struct wl_callback *cb, uint32_t time);
static const struct wl_callback_listener g_frame_listener = {frame_done};

static void frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
  BKWindow *win = (BKWindow *)data;
  wl_callback_destroy(cb);
  if (win->frame_cb == cb) {
    win->frame_cb = NULL;
  }
}

/* The buffer size for the logical size at the window's scale. */
static void update_buffer_size(BKWindow *win)
{
  const double s = win_scale(win);
  win->w = BK_MAX(1, (int)floor(win->lw * s + 0.5));
  win->h = BK_MAX(1, (int)floor(win->lh * s + 0.5));
}

static void repair(Buffer *b, const Buffer *from)
{
  if (!from || !from->valid || from->w != b->w || from->h != b->h) {
    b->valid = 0;
    return;
  }
  const int x0 = BK_MAX(0, b->stale[0]), y0 = BK_MAX(0, b->stale[1]);
  const int x1 = BK_MIN(b->w, b->stale[2]), y1 = BK_MIN(b->h, b->stale[3]);
  for (int y = y0; y < y1 && x1 > x0; y++) {
    memcpy(b->px + (size_t)y * b->w + x0, from->px + (size_t)y * b->w + x0,
           sizeof(uint32_t) * (size_t)(x1 - x0));
  }
  b->stale[0] = b->stale[1] = b->stale[2] = b->stale[3] = 0;
}

static void render_window(BKWindow *win)
{
  if (!win->open || !win->configured || !win->mapped || win->frame_cb) {
    return; /* the next frame callback renders */
  }
  if (win->want_frame && win->cb.on_frame) {
    win->want_frame = 0;
    win->frame_start = now_ms();
    win->cb.on_frame(win->cb.data, win);
  }
  win->render_pending = 0;
  Buffer *b = buffer_next(win);
  if (!b) {
    win->render_pending = 1; /* every buffer is held: wait for a release */
    return;
  }
  Buffer *front = win->front >= 0 ? &win->buffers[win->front] : NULL;
  int full = win->full || !b->valid;
  if (!full) {
    repair(b, front);
    full = !b->valid;
  }
  win->full = 0;
  BUI_raster_set_target(win->raster, b->px, b->w, b->h);
  const int regions = BK_render(win->ctx, win->raster, b->w, b->h, full);
  if (!regions && !full) {
    /* nothing changed: keep the buffer for the next frame */
    if (BK_animating(win->ctx)) {
      win->render_pending = 1;
    }
    return;
  }
  b->valid = 1;
  const int(*rects)[4];
  int n = BUI_raster_damage(win->raster, &rects);
  int bbox[4] = {b->w, b->h, 0, 0};
  if (full) {
    n = 0;
    bbox[0] = bbox[1] = 0;
    bbox[2] = b->w;
    bbox[3] = b->h;
  }
  for (int i = 0; i < n; i++) {
    bbox[0] = BK_MIN(bbox[0], rects[i][0]);
    bbox[1] = BK_MIN(bbox[1], rects[i][1]);
    bbox[2] = BK_MAX(bbox[2], rects[i][0] + rects[i][2]);
    bbox[3] = BK_MAX(bbox[3], rects[i][1] + rects[i][3]);
  }
  /* the other buffers miss this frame's changes */
  for (int i = 0; i < MAX_BUFFERS; i++) {
    Buffer *o = &win->buffers[i];
    if (o == b || !o->wl) {
      continue;
    }
    if (o->stale[2] <= o->stale[0]) {
      memcpy(o->stale, bbox, sizeof(bbox));
    }
    else {
      o->stale[0] = BK_MIN(o->stale[0], bbox[0]);
      o->stale[1] = BK_MIN(o->stale[1], bbox[1]);
      o->stale[2] = BK_MAX(o->stale[2], bbox[2]);
      o->stale[3] = BK_MAX(o->stale[3], bbox[3]);
    }
  }
  apply_blur(win);
  wl_surface_attach(win->surface, b->wl, 0, 0);
  if (full) {
    wl_surface_damage_buffer(win->surface, 0, 0, b->w, b->h);
  }
  else {
    for (int i = 0; i < n; i++) {
      wl_surface_damage_buffer(win->surface, rects[i][0], rects[i][1], rects[i][2], rects[i][3]);
    }
  }
  if (win->viewport) {
    wp_viewport_set_destination(win->viewport, win->lw, win->lh);
  }
  else {
    wl_surface_set_buffer_scale(win->surface, win->int_scale);
  }
  win->frame_cb = wl_surface_frame(win->surface);
  wl_callback_add_listener(win->frame_cb, &g_frame_listener, win);
  wl_surface_commit(win->surface);
  b->busy = 1;
  win->front = (int)(b - win->buffers);
  win->frames++;
  if (win->want_frame) {
    bk_frame_ms = now_ms() - win->frame_start;
    bk_frame_budget_ms = win->frame_vsync ? 1000.0 / 60.0 : 0.0;
    win->render_pending = 1;
  }
  else {
    bk_frame_ms = 0.0;
  }
  if (BK_animating(win->ctx)) {
    win->render_pending = 1; /* paced by the frame callback */
  }
}

/* ----------------------------------------------------------------------- */
/* Scale */

static void notify_state(BKWindow *win)
{
  if (win->cb.on_state) {
    win->cb.on_state(win->cb.data, win, BK_window_state(win));
  }
}

static void set_scale(BKWindow *win, int scale120, int int_scale)
{
  if (scale120 == win->scale120 && int_scale == win->int_scale) {
    return;
  }
  win->scale120 = scale120;
  win->int_scale = BK_MAX(1, int_scale);
  BK_set_scale((float)win_scale(win));
  update_buffer_size(win);
  win->full = 1;
  win->render_pending = 1;
  apply_cursor(win);
  notify_state(win);
}

static void fscale_preferred(void *data, struct wp_fractional_scale_v1 *f, uint32_t scale)
{
  BKWindow *win = (BKWindow *)data;
  set_scale(win, (int)scale, win->int_scale);
}
static const struct wp_fractional_scale_v1_listener g_fscale_listener = {fscale_preferred};

static void surface_enter(void *data, struct wl_surface *s, struct wl_output *o)
{
  BKWindow *win = (BKWindow *)data;
  if (win->fscale) {
    return;
  }
  for (Output *out = g_outputs; out; out = out->next) {
    if (out->wl == o && g_compositor_version < 6) {
      set_scale(win, 0, out->scale);
    }
  }
}
static void surface_leave(void *data, struct wl_surface *s, struct wl_output *o) {}
static void surface_preferred_buffer_scale(void *data, struct wl_surface *s, int32_t factor)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->fscale) {
    set_scale(win, 0, factor);
  }
}
static void surface_preferred_transform(void *data, struct wl_surface *s, uint32_t t) {}
static const struct wl_surface_listener g_surface_listener = {
    surface_enter, surface_leave, surface_preferred_buffer_scale, surface_preferred_transform};

/* ----------------------------------------------------------------------- */
/* xdg-shell */

static void wm_ping(void *data, struct xdg_wm_base *b, uint32_t serial)
{
  xdg_wm_base_pong(b, serial);
}
static const struct xdg_wm_base_listener g_wm_listener = {wm_ping};

static void xdg_surface_configure(void *data, struct xdg_surface *s, uint32_t serial)
{
  BKWindow *win = (BKWindow *)data;
  xdg_surface_ack_configure(s, serial);
  int changed = 0;
  if (win->has_pending) {
    win->has_pending = 0;
    if (win->pending_w > 0 && win->pending_h > 0 &&
        (win->pending_w != win->lw || win->pending_h != win->lh))
    {
      win->lw = win->pending_w;
      win->lh = win->pending_h;
      changed = 1;
    }
    const int keep = win->states & BK_WSTATE_BLUR;
    if ((win->pending_states | keep) != win->states) {
      win->states = win->pending_states | keep;
      changed = 1;
    }
  }
  update_buffer_size(win);
  if (!win->configured || changed) {
    win->configured = 1;
    win->full = 1;
    win->blur_dirty = 1;
    notify_state(win);
  }
  win->render_pending = 1;
  render_window(win);
}
static const struct xdg_surface_listener g_xdg_surface_listener = {xdg_surface_configure};

static void toplevel_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h,
                               struct wl_array *states)
{
  BKWindow *win = (BKWindow *)data;
  int st = 0;
  uint32_t *s;
  wl_array_for_each (s, states) {
    switch (*s) {
      case XDG_TOPLEVEL_STATE_MAXIMIZED:
        st |= BK_WSTATE_MAXIMIZED;
        break;
      case XDG_TOPLEVEL_STATE_FULLSCREEN:
        st |= BK_WSTATE_FULLSCREEN;
        break;
      case XDG_TOPLEVEL_STATE_ACTIVATED:
        st |= BK_WSTATE_ACTIVE;
        break;
      case XDG_TOPLEVEL_STATE_TILED_LEFT:
      case XDG_TOPLEVEL_STATE_TILED_RIGHT:
      case XDG_TOPLEVEL_STATE_TILED_TOP:
      case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
        st |= BK_WSTATE_TILED;
        break;
    }
  }
  if (w > 0 && win->min_w > 0) {
    w = BK_MAX(w, win->min_w);
  }
  if (h > 0 && win->min_h > 0) {
    h = BK_MAX(h, win->min_h);
  }
  win->pending_w = w;
  win->pending_h = h;
  win->pending_states = st;
  win->has_pending = 1;
}
static void toplevel_close(void *data, struct xdg_toplevel *t)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->cb.on_close || win->cb.on_close(win->cb.data, win)) {
    BK_window_close(win);
  }
}
static void toplevel_bounds(void *data, struct xdg_toplevel *t, int32_t w, int32_t h) {}
static void toplevel_caps(void *data, struct xdg_toplevel *t, struct wl_array *caps) {}
static const struct xdg_toplevel_listener g_toplevel_listener = {
    toplevel_configure, toplevel_close, toplevel_bounds, toplevel_caps};

static void deco_configure(void *data, struct zxdg_toplevel_decoration_v1 *d, uint32_t mode) {}
static const struct zxdg_toplevel_decoration_v1_listener g_deco_listener = {deco_configure};

/* ----------------------------------------------------------------------- */
/* Pointer */

static BKWindow *window_of_surface(struct wl_surface *s)
{
  for (BKWindow *w = g_windows; w; w = w->next) {
    if (w->surface == s) {
      return w;
    }
  }
  return NULL;
}

static int pointer_x(BKWindow *win)
{
  return (int)floor(g_px * win_scale(win));
}
static int pointer_y(BKWindow *win)
{
  return (int)floor(g_py * win_scale(win));
}

static void pointer_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s,
                          wl_fixed_t x, wl_fixed_t y)
{
  g_pointer_win = window_of_surface(s);
  g_pointer_serial = serial;
  g_px = wl_fixed_to_double(x);
  g_py = wl_fixed_to_double(y);
  if (g_pointer_win) {
    g_pointer_win->cursor = -1; /* force */
    host_set_cursor(g_pointer_win, BK_cursor(g_pointer_win->ctx));
    BK_motion(g_pointer_win->ctx, pointer_x(g_pointer_win), pointer_y(g_pointer_win),
              g_mods | g_buttons);
  }
}
static void pointer_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{
  BKWindow *win = g_pointer_win;
  g_pointer_win = NULL;
  if (win && win->open && !g_buttons) {
    BK_motion(win->ctx, -100000, -100000, 0);
  }
}
static void pointer_motion(void *data, struct wl_pointer *p, uint32_t t, wl_fixed_t x,
                           wl_fixed_t y)
{
  g_px = wl_fixed_to_double(x);
  g_py = wl_fixed_to_double(y);
  if (g_pointer_win) {
    BK_motion(g_pointer_win->ctx, pointer_x(g_pointer_win), pointer_y(g_pointer_win),
              g_mods | g_buttons);
  }
}
static void pointer_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t t,
                           uint32_t button, uint32_t state)
{
  BKWindow *win = g_pointer_win;
  const int press = state == WL_POINTER_BUTTON_STATE_PRESSED;
  if (press) {
    g_last_press_serial = serial;
  }
  const int bit = button == BTN_LEFT ? BK_BUTTON1 : button == BTN_MIDDLE ? BK_BUTTON2 :
                  button == BTN_RIGHT ? BK_BUTTON3 : 0;
  if (press) {
    g_buttons |= bit;
  }
  else {
    g_buttons &= ~bit;
  }
  if (!win || !win->open) {
    return;
  }
  BKContext *ctx = win->ctx;
  const int x = pointer_x(win), y = pointer_y(win);
  switch (button) {
    case BTN_LEFT:
      if (press) {
        BK_press(ctx, x, y, g_mods | BK_BUTTON1);
      }
      else {
        BK_release(ctx, x, y, g_mods);
      }
      break;
    case BTN_MIDDLE:
      BK_button(ctx, x, y, 2, !press, g_mods);
      break;
    case BTN_RIGHT:
      if (press) {
        BK_context_menu(ctx, x, y, g_mods);
      }
      else {
        BK_button(ctx, x, y, 3, 1, g_mods);
      }
      break;
  }
}
static void pointer_axis(void *data, struct wl_pointer *p, uint32_t t, uint32_t axis,
                         wl_fixed_t value)
{
  /* continuous: 10 surface units per wheel notch (libinput's convention) */
  const float v = (float)wl_fixed_to_double(value) / 10.0f;
  if (g_axis_discrete) {
    return; /* the discrete event of this frame says it in notches */
  }
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    g_axis_y -= v;
  }
  else {
    g_axis_x += v;
  }
  g_axis_has = 1;
}
static void pointer_frame(void *data, struct wl_pointer *p)
{
  BKWindow *win = g_pointer_win;
  if (g_axis_has && win && win->open) {
    BK_scroll(win->ctx, pointer_x(win), pointer_y(win), g_axis_x, g_axis_y, g_mods);
  }
  g_axis_x = g_axis_y = 0.0f;
  g_axis_has = 0;
  g_axis_discrete = 0;
}
static void pointer_axis_source(void *data, struct wl_pointer *p, uint32_t source) {}
static void pointer_axis_stop(void *data, struct wl_pointer *p, uint32_t t, uint32_t axis) {}
static void pointer_axis_discrete(void *data, struct wl_pointer *p, uint32_t axis, int32_t d)
{
  if (!g_axis_discrete) {
    g_axis_x = g_axis_y = 0.0f;
  }
  g_axis_discrete = 1;
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    g_axis_y -= (float)d;
  }
  else {
    g_axis_x += (float)d;
  }
  g_axis_has = 1;
}
static void pointer_axis_value120(void *data, struct wl_pointer *p, uint32_t axis, int32_t v120)
{
  if (!g_axis_discrete) {
    g_axis_x = g_axis_y = 0.0f;
  }
  g_axis_discrete = 1;
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    g_axis_y -= (float)v120 / 120.0f;
  }
  else {
    g_axis_x += (float)v120 / 120.0f;
  }
  g_axis_has = 1;
}
static void pointer_axis_relative_direction(void *data, struct wl_pointer *p, uint32_t axis,
                                            uint32_t dir)
{
}
static const struct wl_pointer_listener g_pointer_listener = {
    pointer_enter,         pointer_leave,     pointer_motion,      pointer_button,
    pointer_axis,          pointer_frame,     pointer_axis_source, pointer_axis_stop,
    pointer_axis_discrete, pointer_axis_value120, pointer_axis_relative_direction};

/* ----------------------------------------------------------------------- */
/* Keyboard */

static void update_mods(void)
{
  g_mods = 0;
  if (!g_xkb_state) {
    return;
  }
  if (xkb_state_mod_name_is_active(g_xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) {
    g_mods |= BK_SHIFT;
  }
  if (xkb_state_mod_name_is_active(g_xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) {
    g_mods |= BK_CTRL;
  }
  if (xkb_state_mod_name_is_active(g_xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) {
    g_mods |= BK_ALT;
  }
}

static void keyboard_keymap(void *data, struct wl_keyboard *k, uint32_t format, int32_t fd,
                            uint32_t size)
{
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }
  char *map = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (map == MAP_FAILED) {
    return;
  }
  if (!g_xkb) {
    g_xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  }
  struct xkb_keymap *km = g_xkb ? xkb_keymap_new_from_string(g_xkb, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                             XKB_KEYMAP_COMPILE_NO_FLAGS) :
                                  NULL;
  munmap(map, size);
  if (!km) {
    return;
  }
  if (g_xkb_state) {
    xkb_state_unref(g_xkb_state);
  }
  if (g_keymap) {
    xkb_keymap_unref(g_keymap);
  }
  g_keymap = km;
  g_xkb_state = xkb_state_new(km);
  if (bk_layout_hook) {
    bk_layout_hook();
  }
}
static void keyboard_enter(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s,
                           struct wl_array *keys)
{
  g_key_win = window_of_surface(s);
  g_key_serial = serial;
}
static void keyboard_leave(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s)
{
  g_key_win = NULL;
  g_repeat_key = 0;
}

/* A key press (or repeat): the X keysym name and the text it types. */
static const char *wl_key_label(int evdev)
{
  static char buf[16];
  if (!g_keymap) {
    return NULL;
  }
  struct xkb_state *st = xkb_state_new(g_keymap); /* no modifiers: the base level */
  if (!st) {
    return NULL;
  }
  buf[0] = 0;
  const xkb_keysym_t sym = xkb_state_key_get_one_sym(st, (xkb_keycode_t)evdev + 8);
  const xkb_keysym_t up = xkb_keysym_to_upper(sym);
  xkb_keysym_to_utf8(up, buf, sizeof(buf));
  xkb_state_unref(st);
  return (unsigned char)buf[0] >= 0x20 ? buf : NULL;
}

static void key_send(BKWindow *win, uint32_t code, int press)
{
  bk_key_label_fn = wl_key_label;
  bk_last_key_code = (int)code - 8;
  if (!g_xkb_state || !win || !win->open) {
    return;
  }
  const xkb_keysym_t sym = xkb_state_key_get_one_sym(g_xkb_state, code);
  char name[64] = "";
  xkb_keysym_get_name(sym, name, sizeof(name));
  if (!press) {
    BK_key_release(win->ctx, name, g_mods);
    return;
  }
  char text[32] = "";
  xkb_state_key_get_utf8(g_xkb_state, code, text, sizeof(text));
  const int printable = text[0] && (unsigned char)text[0] >= 0x20 && text[0] != 0x7f &&
                        !(g_mods & BK_CTRL);
  BK_key(win->ctx, printable ? text : "", name, g_mods);
}

static void keyboard_key(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t t,
                         uint32_t key, uint32_t state)
{
  g_key_serial = serial;
  const uint32_t code = key + 8;
  const int press = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  key_send(g_key_win, code, press);
  if (press && g_keymap && xkb_keymap_key_repeats(g_keymap, code) && g_repeat_rate > 0) {
    g_repeat_key = code;
    g_repeat_due = now_ms() + g_repeat_delay;
  }
  else if (!press && g_repeat_key == code) {
    g_repeat_key = 0;
  }
}
static void keyboard_modifiers(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t dep,
                               uint32_t lat, uint32_t lock, uint32_t group)
{
  static uint32_t last_group = 0;
  if (g_xkb_state) {
    xkb_state_update_mask(g_xkb_state, dep, lat, lock, 0, 0, group);
  }
  update_mods();
  if (group != last_group) {
    last_group = group;
    if (bk_layout_hook) {
      bk_layout_hook();
    }
  }
}
static void keyboard_repeat_info(void *data, struct wl_keyboard *k, int32_t rate, int32_t delay)
{
  g_repeat_rate = rate;
  g_repeat_delay = delay;
}
static const struct wl_keyboard_listener g_keyboard_listener = {
    keyboard_keymap, keyboard_enter,     keyboard_leave,
    keyboard_key,    keyboard_modifiers, keyboard_repeat_info};

/* ----------------------------------------------------------------------- */
/* Seat and data device */

static int g_new_offer_uris;

static void offer_offer(void *data, struct wl_data_offer *o, const char *mime)
{
  int *text = (int *)data;
  if (!strcmp(mime, "text/plain;charset=utf-8")) {
    *text = 2;
  }
  else if (!*text && (!strcmp(mime, "text/plain") || !strcmp(mime, "UTF8_STRING"))) {
    *text = 1;
  }
  if (!strcmp(mime, "text/uri-list")) {
    g_new_offer_uris = 1;
  }
}
static void offer_source_actions(void *data, struct wl_data_offer *o, uint32_t a) {}
static void offer_action(void *data, struct wl_data_offer *o, uint32_t a) {}
static const struct wl_data_offer_listener g_offer_listener = {offer_offer, offer_source_actions,
                                                               offer_action};
static int g_new_offer_text;
static struct wl_data_offer *g_new_offer;

static void dd_data_offer(void *data, struct wl_data_device *d, struct wl_data_offer *o)
{
  g_new_offer = o;
  g_new_offer_text = 0;
  g_new_offer_uris = 0;
  wl_data_offer_add_listener(o, &g_offer_listener, &g_new_offer_text);
}
/* BUI-Awra: file drags (text/uri-list) from other applications */
static BKWindow *g_dnd_win;
static struct wl_data_offer *g_dnd_offer;
static uint32_t g_dnd_serial;
static int g_dnd_x, g_dnd_y;

static void dnd_answer(int accepts)
{
  if (!g_dnd_offer) {
    return;
  }
  wl_data_offer_accept(g_dnd_offer, g_dnd_serial, accepts ? "text/uri-list" : NULL);
  if (wl_data_offer_get_version(g_dnd_offer) >= 3) {
    wl_data_offer_set_actions(g_dnd_offer,
                              accepts ? WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY :
                                        WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE,
                              WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
  }
}

static void dd_enter(void *data, struct wl_data_device *d, uint32_t serial, struct wl_surface *s,
                     wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *o)
{
  g_dnd_win = window_of_surface(s);
  g_dnd_offer = (o && g_new_offer_uris) ? o : NULL;
  g_dnd_serial = serial;
  if (!g_dnd_win || !g_dnd_offer) {
    if (o) {
      wl_data_offer_accept(o, serial, NULL);
    }
    return;
  }
  const double sc = win_scale(g_dnd_win);
  g_dnd_x = (int)(wl_fixed_to_double(x) * sc);
  g_dnd_y = (int)(wl_fixed_to_double(y) * sc);
  dnd_answer(BK_drop(g_dnd_win->ctx, BK_DROP_ENTER, g_dnd_x, g_dnd_y, NULL));
}
static void dd_leave(void *data, struct wl_data_device *d)
{
  if (g_dnd_win && g_dnd_offer) {
    BK_drop(g_dnd_win->ctx, BK_DROP_LEAVE, g_dnd_x, g_dnd_y, NULL);
  }
  if (g_new_offer && g_new_offer != g_sel_offer) {
    wl_data_offer_destroy(g_new_offer);
  }
  g_new_offer = NULL;
  g_dnd_offer = NULL;
  g_dnd_win = NULL;
}
static void dd_motion(void *data, struct wl_data_device *d, uint32_t t, wl_fixed_t x,
                      wl_fixed_t y)
{
  if (!g_dnd_win || !g_dnd_offer) {
    return;
  }
  const double sc = win_scale(g_dnd_win);
  g_dnd_x = (int)(wl_fixed_to_double(x) * sc);
  g_dnd_y = (int)(wl_fixed_to_double(y) * sc);
  dnd_answer(BK_drop(g_dnd_win->ctx, BK_DROP_MOTION, g_dnd_x, g_dnd_y, NULL));
}
static char *read_offer(struct wl_data_offer *o, const char *mime)
{
  int fds[2];
  if (pipe2(fds, O_CLOEXEC) != 0) {
    return NULL;
  }
  wl_data_offer_receive(o, mime, fds[1]);
  close(fds[1]);
  wl_display_flush(g_dpy);
  size_t len = 0, cap = 4096;
  char *buf = (char *)malloc(cap + 1);
  const double until = now_ms() + 3000.0;
  while (buf) {
    struct pollfd p = {fds[0], POLLIN, 0};
    const int left = (int)(until - now_ms());
    if (left <= 0 || poll(&p, 1, left) <= 0) {
      break;
    }
    if (len == cap) {
      char *grown = (char *)realloc(buf, cap * 2 + 1);
      if (!grown) {
        break;
      }
      buf = grown;
      cap *= 2;
    }
    const ssize_t n = read(fds[0], buf + len, cap - len);
    if (n <= 0) {
      break;
    }
    len += (size_t)n;
  }
  close(fds[0]);
  if (buf) {
    buf[len] = 0;
  }
  return buf;
}
static void dd_drop(void *data, struct wl_data_device *d)
{
  if (!g_dnd_win || !g_dnd_offer) {
    return;
  }
  char *uris = read_offer(g_dnd_offer, "text/uri-list");
  if (wl_data_offer_get_version(g_dnd_offer) >= 3) {
    wl_data_offer_finish(g_dnd_offer);
  }
  if (uris) {
    BK_drop(g_dnd_win->ctx, BK_DROP_DROP, g_dnd_x, g_dnd_y, uris);
    free(uris);
  }
  g_dnd_win->render_pending = 1;
}
static void dd_selection(void *data, struct wl_data_device *d, struct wl_data_offer *o)
{
  if (g_sel_offer && g_sel_offer != o) {
    wl_data_offer_destroy(g_sel_offer);
  }
  g_sel_offer = o;
  g_sel_offer_text = (o && o == g_new_offer) ? g_new_offer_text : 0;
  g_new_offer = NULL;
}
static const struct wl_data_device_listener g_dd_listener = {dd_data_offer, dd_enter,  dd_leave,
                                                             dd_motion,     dd_drop,   dd_selection};

static void seat_caps(void *data, struct wl_seat *seat, uint32_t caps)
{
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !g_pointer) {
    g_pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(g_pointer, &g_pointer_listener, NULL);
    if (g_cursor_shape_mgr) {
      g_cursor_shape_dev = wp_cursor_shape_manager_v1_get_pointer(g_cursor_shape_mgr, g_pointer);
    }
  }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && g_pointer) {
    if (g_cursor_shape_dev) {
      wp_cursor_shape_device_v1_destroy(g_cursor_shape_dev);
      g_cursor_shape_dev = NULL;
    }
    wl_pointer_destroy(g_pointer);
    g_pointer = NULL;
  }
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !g_keyboard) {
    g_keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(g_keyboard, &g_keyboard_listener, NULL);
  }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && g_keyboard) {
    wl_keyboard_destroy(g_keyboard);
    g_keyboard = NULL;
  }
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) {}
static const struct wl_seat_listener g_seat_listener = {seat_caps, seat_name};

static void effect_caps(void *data, struct ext_background_effect_manager_v1 *m, uint32_t caps)
{
  g_effect_caps = (int)caps;
  for (BKWindow *w = g_windows; w; w = w->next) {
    w->blur_dirty = 1;
    if (blur_supported()) {
      w->states |= BK_WSTATE_BLUR;
    }
    else {
      w->states &= ~BK_WSTATE_BLUR;
    }
    w->render_pending = 1;
    notify_state(w);
  }
}
static const struct ext_background_effect_manager_v1_listener g_effect_listener = {effect_caps};

static void registry_global(void *data, struct wl_registry *r, uint32_t name, const char *iface,
                            uint32_t version)
{
#  define BIND(var, interface, max)                                                          \
    var = wl_registry_bind(r, name, &interface, version < (max) ? version : (max))
  if (!strcmp(iface, wl_compositor_interface.name)) {
    g_compositor_version = version < 6 ? version : 6;
    BIND(g_compositor, wl_compositor_interface, 6);
  }
  else if (!strcmp(iface, wl_shm_interface.name)) {
    BIND(g_shm, wl_shm_interface, 1);
  }
  else if (!strcmp(iface, xdg_wm_base_interface.name)) {
    BIND(g_wm_base, xdg_wm_base_interface, 5);
    xdg_wm_base_add_listener(g_wm_base, &g_wm_listener, NULL);
  }
  else if (!strcmp(iface, wl_seat_interface.name) && !g_seat) {
    g_seat_version = version < 8 ? version : 8;
    BIND(g_seat, wl_seat_interface, 8);
    wl_seat_add_listener(g_seat, &g_seat_listener, NULL);
  }
  else if (!strcmp(iface, wl_data_device_manager_interface.name)) {
    BIND(g_ddm, wl_data_device_manager_interface, 3);
  }
  else if (!strcmp(iface, wp_cursor_shape_manager_v1_interface.name)) {
    BIND(g_cursor_shape_mgr, wp_cursor_shape_manager_v1_interface, 1);
  }
  else if (!strcmp(iface, wp_fractional_scale_manager_v1_interface.name)) {
    BIND(g_fscale_mgr, wp_fractional_scale_manager_v1_interface, 1);
  }
  else if (!strcmp(iface, wp_viewporter_interface.name)) {
    BIND(g_viewporter, wp_viewporter_interface, 1);
  }
  else if (!strcmp(iface, ext_background_effect_manager_v1_interface.name)) {
    BIND(g_effect_mgr, ext_background_effect_manager_v1_interface, 1);
    ext_background_effect_manager_v1_add_listener(g_effect_mgr, &g_effect_listener, NULL);
  }
  else if (!strcmp(iface, org_kde_kwin_blur_manager_interface.name)) {
    BIND(g_kblur_mgr, org_kde_kwin_blur_manager_interface, 1);
  }
  else if (!strcmp(iface, zxdg_decoration_manager_v1_interface.name)) {
    BIND(g_deco_mgr, zxdg_decoration_manager_v1_interface, 1);
  }
  else if (!strcmp(iface, wl_output_interface.name)) {
    Output *o = (Output *)calloc(1, sizeof(Output));
    if (o) {
      o->name = name;
      o->scale = 1;
      o->wl = (struct wl_output *)wl_registry_bind(r, name, &wl_output_interface,
                                                   version < 4 ? version : 4);
      wl_output_add_listener(o->wl, &g_output_listener, o);
      o->next = g_outputs;
      g_outputs = o;
    }
  }
#  undef BIND
}
static void registry_remove(void *data, struct wl_registry *r, uint32_t name)
{
  for (Output **p = &g_outputs; *p; p = &(*p)->next) {
    if ((*p)->name == name) {
      Output *o = *p;
      *p = o->next;
      wl_output_destroy(o->wl);
      free(o);
      return;
    }
  }
}
static const struct wl_registry_listener g_registry_listener = {registry_global, registry_remove};

static int open_display(void)
{
  if (g_dpy) {
    return 1;
  }
  if (!getenv("WAYLAND_DISPLAY") && !getenv("WAYLAND_SOCKET")) {
    return 0;
  }
  g_dpy = wl_display_connect(NULL);
  if (!g_dpy) {
    return 0;
  }
  g_registry = wl_display_get_registry(g_dpy);
  wl_registry_add_listener(g_registry, &g_registry_listener, NULL);
  wl_display_roundtrip(g_dpy); /* globals */
  if (!g_compositor || !g_shm || !g_wm_base) {
    wl_display_disconnect(g_dpy);
    g_dpy = NULL;
    return 0;
  }
  if (g_seat && g_ddm) {
    g_data_device = wl_data_device_manager_get_data_device(g_ddm, g_seat);
    wl_data_device_add_listener(g_data_device, &g_dd_listener, NULL);
  }
  wl_display_roundtrip(g_dpy); /* seat capabilities, output scales, effect capabilities */
  if (g_pointer && g_cursor_shape_mgr && !g_cursor_shape_dev) {
    g_cursor_shape_dev = wp_cursor_shape_manager_v1_get_pointer(g_cursor_shape_mgr, g_pointer);
  }
  return 1;
}

/* ----------------------------------------------------------------------- */
/* Windows */

int BK_window_supported(void)
{
  return open_display();
}

static void map_window(BKWindow *win)
{
  if (win->mapped || !win->open) {
    return;
  }
  win->xdg_surface = xdg_wm_base_get_xdg_surface(g_wm_base, win->surface);
  xdg_surface_add_listener(win->xdg_surface, &g_xdg_surface_listener, win);
  win->toplevel = xdg_surface_get_toplevel(win->xdg_surface);
  xdg_toplevel_add_listener(win->toplevel, &g_toplevel_listener, win);
  xdg_toplevel_set_title(win->toplevel, win->title ? win->title : "");
  if (win->app_id) {
    xdg_toplevel_set_app_id(win->toplevel, win->app_id);
  }
  if (win->min_w > 0 || win->min_h > 0) {
    xdg_toplevel_set_min_size(win->toplevel, win->min_w, win->min_h);
  }
  if (win->max_w > 0 || win->max_h > 0) {
    xdg_toplevel_set_max_size(win->toplevel, win->max_w, win->max_h);
  }
  if (g_deco_mgr) {
    win->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(g_deco_mgr, win->toplevel);
    zxdg_toplevel_decoration_v1_add_listener(win->deco, &g_deco_listener, win);
    zxdg_toplevel_decoration_v1_set_mode(
        win->deco, (win->flags & BK_WINDOW_UNDECORATED) ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE :
                                                          ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }
  win->mapped = 1;
  win->configured = 0;
  win->states &= ~BK_WSTATE_HIDDEN;
  wl_surface_commit(win->surface); /* the first configure answers */
  wl_display_flush(g_dpy);
}

static void unmap_window(BKWindow *win)
{
  if (!win->mapped) {
    return;
  }
  if (win->frame_cb) {
    wl_callback_destroy(win->frame_cb);
    win->frame_cb = NULL;
  }
  if (win->deco) {
    zxdg_toplevel_decoration_v1_destroy(win->deco);
    win->deco = NULL;
  }
  xdg_toplevel_destroy(win->toplevel);
  xdg_surface_destroy(win->xdg_surface);
  win->toplevel = NULL;
  win->xdg_surface = NULL;
  wl_surface_attach(win->surface, NULL, 0, 0);
  wl_surface_commit(win->surface);
  win->mapped = 0;
  win->configured = 0;
  win->front = -1;
  for (int i = 0; i < MAX_BUFFERS; i++) {
    win->buffers[i].valid = 0;
  }
  win->states |= BK_WSTATE_HIDDEN;
  wl_display_flush(g_dpy);
}

BKWindow *BK_window_new_ex(BKContext *ctx, const char *title, int width, int height, int flags)
{
  if (!open_display()) {
    return NULL;
  }
  BKWindow *win = (BKWindow *)calloc(1, sizeof(BKWindow));
  if (!win) {
    return NULL;
  }
  win->ctx = ctx;
  win->prev = ctx->host;
  win->flags = flags;
  win->raster = BUI_raster_create(1, 1);
  BUI_raster_set_diff(win->raster, 0); /* the kit's regions are the damage */
  win->next_timer = 1;
  win->front = -1;
  win->lw = BK_MAX(1, width);
  win->lh = BK_MAX(1, height);
  win->int_scale = 1;
  win->title = strdup(title ? title : "");
  win->cursor = -1;
  win->open = 1;
  win->surface = wl_compositor_create_surface(g_compositor);
  wl_surface_add_listener(win->surface, &g_surface_listener, win);
  if (g_fscale_mgr && g_viewporter) {
    win->viewport = wp_viewporter_get_viewport(g_viewporter, win->surface);
    win->fscale = wp_fractional_scale_manager_v1_get_fractional_scale(g_fscale_mgr, win->surface);
    wp_fractional_scale_v1_add_listener(win->fscale, &g_fscale_listener, win);
    win->scale120 = 120;
  }
  else {
    /* until the surface enters an output: the largest output scale */
    int s = 1;
    for (Output *o = g_outputs; o; o = o->next) {
      s = BK_MAX(s, o->scale);
    }
    win->int_scale = s;
  }
  if (!(flags & BK_WINDOW_TRANSPARENT)) {
    struct wl_region *opaque = wl_compositor_create_region(g_compositor);
    wl_region_add(opaque, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_set_opaque_region(win->surface, opaque);
    wl_region_destroy(opaque);
  }
  if (blur_supported()) {
    win->states |= BK_WSTATE_BLUR;
  }
  BK_set_scale((float)win_scale(win));
  update_buffer_size(win);
  ctx->transparent = (flags & BK_WINDOW_TRANSPARENT) != 0;
  win->next = g_windows;
  g_windows = win;
  g_open++;
  BKHost host = {win,                host_request,       host_set_cursor, host_warp,
                 host_clipboard_get, host_clipboard_set, host_schedule,   host_emit};
  BK_context_set_host(ctx, &host);
  return win;
}

BKWindow *BK_window_new(BKContext *ctx, const char *title, int width, int height)
{
  return BK_window_new_ex(ctx, title, width, height, 0);
}

void BK_window_close(BKWindow *win)
{
  if (!win || !win->open) {
    return;
  }
  unmap_window(win);
  if (win->effect) {
    ext_background_effect_surface_v1_destroy(win->effect);
    win->effect = NULL;
  }
  if (win->kblur) {
    org_kde_kwin_blur_release(win->kblur);
    win->kblur = NULL;
  }
  if (win->fscale) {
    wp_fractional_scale_v1_destroy(win->fscale);
    win->fscale = NULL;
  }
  if (win->viewport) {
    wp_viewport_destroy(win->viewport);
    win->viewport = NULL;
  }
  if (g_pointer_win == win) {
    g_pointer_win = NULL;
  }
  if (g_key_win == win) {
    g_key_win = NULL;
  }
  buffers_free(win);
  /* the raster pointed into a buffer just unmapped */
  BUI_raster_set_target(win->raster, NULL, 1, 1);
  wl_surface_destroy(win->surface);
  win->surface = NULL;
  win->open = 0;
  g_open--;
  wl_display_flush(g_dpy);
}

void BK_window_free(BKWindow *win)
{
  if (!win) {
    return;
  }
  BK_window_close(win);
  for (BKWindow **p = &g_windows; *p; p = &(*p)->next) {
    if (*p == win) {
      *p = win->next;
      break;
    }
  }
  if (win->ctx) {
    BK_context_set_host(win->ctx, &win->prev);
  }
  BUI_raster_free(win->raster);
  free(win->title);
  free(win->app_id);
  free(win);
}

void BK_window_set_callbacks(BKWindow *win, const BKWindowCallbacks *cb)
{
  win->cb = *cb;
}

void BK_window_show(BKWindow *win)
{
  map_window(win);
  win->render_pending = 1;
}

void BK_window_hide(BKWindow *win)
{
  unmap_window(win);
}

int BK_window_is_open(BKWindow *win)
{
  return win && win->open;
}

void BK_window_set_title(BKWindow *win, const char *title)
{
  free(win->title);
  win->title = strdup(title ? title : "");
  if (win->toplevel) {
    xdg_toplevel_set_title(win->toplevel, win->title);
  }
}

void BK_window_set_app_id(BKWindow *win, const char *app_id)
{
  free(win->app_id);
  win->app_id = app_id ? strdup(app_id) : NULL;
  if (win->toplevel && win->app_id) {
    xdg_toplevel_set_app_id(win->toplevel, win->app_id);
  }
}

void BK_window_set_size(BKWindow *win, int width, int height)
{
  /* the compositor decides the size of a configured toplevel; before the
   * first configure (and when it lets us), this is the size we propose */
  win->lw = BK_MAX(1, width);
  win->lh = BK_MAX(1, height);
  update_buffer_size(win);
  win->full = 1;
  win->render_pending = 1;
}

void BK_window_get_size(BKWindow *win, int *w, int *h)
{
  *w = win->open ? win->w : 0;
  *h = win->open ? win->h : 0;
}

BuiRaster *BK_window_raster(BKWindow *win)
{
  return win->raster;
}

void BK_window_set_gpu(BKWindow *win, int on) {}
int BK_window_gpu(BKWindow *win)
{
  return 0;
}
int BK_window_frames(BKWindow *win)
{
  return win->frames;
}

void BK_window_render_now(BKWindow *win)
{
  win->full = 1;
  win->render_pending = 1;
  render_window(win);
  wl_display_flush(g_dpy);
}

int BK_window_timer(BKWindow *win, int ms)
{
  const int id = win->next_timer++;
  Timer *t = timer_slot(win, id);
  if (t) {
    t->id = id;
    t->due = now_ms() + BK_MAX(0, ms);
  }
  return id;
}

void BK_window_cancel_timer(BKWindow *win, int id)
{
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (win->timers[i].id == id) {
      win->timers[i].id = 0;
    }
  }
}

void BK_window_request_frame(BKWindow *win, int vsync)
{
  win->want_frame = 1;
  win->frame_vsync = vsync;
  win->render_pending = 1;
}

void *BK_window_handle(BKWindow *win)
{
  return win->surface;
}

int BK_window_set_blur(BKWindow *win, int x, int y, int w, int h, int radius)
{
  if (win->blur[0] != x || win->blur[1] != y || win->blur[2] != w || win->blur[3] != h ||
      win->blur[4] != radius)
  {
    win->blur[0] = x;
    win->blur[1] = y;
    win->blur[2] = w;
    win->blur[3] = h;
    win->blur[4] = radius;
    win->blur_dirty = 1;
    win->render_pending = 1; /* applied with the next commit */
  }
  return blur_supported();
}

int BK_window_blur_available(BKWindow *win)
{
  if (g_kblur_mgr) {
    return 1;
  }
  if (!g_effect_mgr) {
    return 0;
  }
  return g_effect_caps < 0 ? -1 : (g_effect_caps & 1) != 0;
}

void BK_window_begin_move(BKWindow *win)
{
  if (win->toplevel && g_seat) {
    /* the drag started by the press: release the kit's grab first */
    g_buttons = 0;
    xdg_toplevel_move(win->toplevel, g_seat, g_last_press_serial);
    wl_display_flush(g_dpy);
  }
}

void BK_window_begin_resize(BKWindow *win, int edges)
{
  if (!win->toplevel || !g_seat) {
    return;
  }
  uint32_t e = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
  const int t = edges & BK_EDGE_TOP, b = edges & BK_EDGE_BOTTOM;
  const int l = edges & BK_EDGE_LEFT, r = edges & BK_EDGE_RIGHT;
  if (t && l) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
  }
  else if (t && r) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
  }
  else if (b && l) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
  }
  else if (b && r) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
  }
  else if (t) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
  }
  else if (b) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
  }
  else if (l) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
  }
  else if (r) {
    e = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
  }
  g_buttons = 0;
  xdg_toplevel_resize(win->toplevel, g_seat, g_last_press_serial, e);
  wl_display_flush(g_dpy);
}

void BK_window_show_menu(BKWindow *win, int x, int y)
{
  if (win->toplevel && g_seat) {
    const double s = win_scale(win);
    xdg_toplevel_show_window_menu(win->toplevel, g_seat, g_last_press_serial, (int)(x / s),
                                  (int)(y / s));
  }
}

int BK_window_state(BKWindow *win)
{
  int st = win->states;
  if (blur_supported()) {
    st |= BK_WSTATE_BLUR;
  }
  else {
    st &= ~BK_WSTATE_BLUR;
  }
  return st;
}

float BK_window_scale(BKWindow *win)
{
  return (float)win_scale(win);
}

void BK_window_wm(BKWindow *win, const char *key, double a, double b)
{
  if (!strcmp(key, "minsize") || !strcmp(key, "maxsize")) {
    const int mn = !strcmp(key, "minsize");
    if (mn) {
      win->min_w = (int)a;
      win->min_h = (int)b;
    }
    else {
      win->max_w = (int)a;
      win->max_h = (int)b;
    }
    if (win->toplevel) {
      if (mn) {
        xdg_toplevel_set_min_size(win->toplevel, win->min_w, win->min_h);
      }
      else {
        xdg_toplevel_set_max_size(win->toplevel, win->max_w, win->max_h);
      }
      wl_surface_commit(win->surface);
    }
    return;
  }
  if (!win->toplevel) {
    return;
  }
  if (!strcmp(key, "state")) {
    switch ((int)a) {
      case 0:
        xdg_toplevel_unset_maximized(win->toplevel);
        xdg_toplevel_unset_fullscreen(win->toplevel);
        break;
      case 1:
        xdg_toplevel_set_minimized(win->toplevel);
        break;
      case 3:
        xdg_toplevel_set_maximized(win->toplevel);
        break;
      case 4:
        xdg_toplevel_set_fullscreen(win->toplevel, NULL);
        break;
    }
  }
  wl_display_flush(g_dpy);
}

int BK_window_wm_get(BKWindow *win, const char *key, double *r_a, double *r_b)
{
  *r_a = *r_b = 0.0;
  if (!strcmp(key, "scale")) {
    *r_a = *r_b = win ? win_scale(win) : 1.0;
    return 1;
  }
  if (!strcmp(key, "screen")) {
    for (Output *o = g_outputs; o; o = o->next) {
      *r_a = o->w / (double)BK_MAX(1, o->scale);
      *r_b = o->h / (double)BK_MAX(1, o->scale);
      return 1;
    }
    return 0;
  }
  if (!strcmp(key, "state") && win) {
    const int st = win->states;
    *r_a = (st & BK_WSTATE_FULLSCREEN) ? 4 : (st & BK_WSTATE_MAXIMIZED) ? 3 : 0;
    return 1;
  }
  return 0;
}

/* ----------------------------------------------------------------------- */
/* The loop */

void bk_loop_wake_host(void)
{
  if (g_wake_pipe[1] >= 0) {
    const char c = 1;
    ssize_t n = write(g_wake_pipe[1], &c, 1);
    (void)n;
  }
}

static int ensure_wake_pipe(void)
{
  if (g_wake_pipe[0] >= 0) {
    return 1;
  }
  if (pipe2(g_wake_pipe, O_CLOEXEC | O_NONBLOCK) != 0) {
    g_wake_pipe[0] = g_wake_pipe[1] = -1;
    return 0;
  }
  return 1;
}

int bk_window_wake_ready(void)
{
  return g_dpy && ensure_wake_pipe();
}

int BK_window_wake_init(void (*cb)(void))
{
  g_wake_cb = cb;
  return ensure_wake_pipe();
}

void BK_window_wake(void)
{
  bk_loop_wake_host();
}

void BK_window_set_keep_alive(int on)
{
  g_keep_alive = on;
}

void BK_window_set_stop_check(int (*stop)(void))
{
  g_stop = stop;
}

void BK_window_quit(void)
{
  g_quit = 1;
  bk_loop_wake_host();
}

static void run_timers(void)
{
  const double t = now_ms();
  for (BKWindow *win = g_windows; win; win = win->next) {
    for (int i = 0; i < MAX_TIMERS && win->open; i++) {
      Timer *tm = &win->timers[i];
      if (tm->id != 0 && tm->due <= t) {
        const int id = tm->id;
        tm->id = 0;
        if (id == -1) {
          BK_timer(win->ctx, win->tip_id);
        }
        else if (id == -2) {
          BK_timer(win->ctx, BK_TIMER_CUSTOM);
        }
        else if (win->cb.on_timer) {
          win->cb.on_timer(win->cb.data, win, id);
        }
      }
    }
  }
  if (g_repeat_key && g_key_win && t >= g_repeat_due) {
    key_send(g_key_win, g_repeat_key, 1);
    g_repeat_due = t + 1000.0 / BK_MAX(1, g_repeat_rate);
  }
  bk_loop_run_due();
}

/* Milliseconds until the next timer (-1: none). */
static int next_timeout(void)
{
  double due = 0.0;
  for (BKWindow *win = g_windows; win; win = win->next) {
    for (int i = 0; i < MAX_TIMERS && win->open; i++) {
      if (win->timers[i].id != 0 && (due == 0.0 || win->timers[i].due < due)) {
        due = win->timers[i].due;
      }
    }
  }
  if (g_repeat_key && g_key_win && (due == 0.0 || g_repeat_due < due)) {
    due = g_repeat_due;
  }
  int ms = -1;
  if (due != 0.0) {
    const double left = due - now_ms();
    ms = left <= 0.0 ? 0 : (int)(left + 0.999);
  }
  return bk_loop_timeout_ms(ms);
}

static int render_ready(void)
{
  for (BKWindow *win = g_windows; win; win = win->next) {
    if (win->open && win->mapped && win->configured && win->render_pending && !win->frame_cb) {
      return 1;
    }
  }
  return 0;
}

/* One turn: wait (at most `timeout_ms`, -1 forever) and handle what came. */
static void loop_turn(int timeout_ms)
{
  ensure_wake_pipe();
  while (wl_display_prepare_read(g_dpy) != 0) {
    wl_display_dispatch_pending(g_dpy);
  }
  wl_display_flush(g_dpy);
  int timeout = next_timeout();
  if (timeout_ms >= 0 && (timeout < 0 || timeout_ms < timeout)) {
    timeout = timeout_ms;
  }
  if (render_ready()) {
    timeout = 0;
  }
  struct pollfd fds[10] = {{wl_display_get_fd(g_dpy), POLLIN, 0}, {g_wake_pipe[0], POLLIN, 0}};
  /* watched descriptors (BK_loop_watch_fd) after the display and the wake pipe */
  const int nextra = bk_loop_fd_count() < 8 ? bk_loop_fd_count() : 8;
  for (int i = 0; i < nextra; i++) {
    fds[2 + i].fd = bk_loop_fd(i);
    fds[2 + i].events = POLLIN;
    fds[2 + i].revents = 0;
  }
  if (g_wake_pipe[0] < 0) {
    fds[1].fd = -1; /* poll skips negative descriptors */
  }
  const int n = poll(fds, (nfds_t)(2 + nextra), timeout);
  if (n > 0 && (fds[0].revents & POLLIN)) {
    wl_display_read_events(g_dpy);
  }
  else {
    wl_display_cancel_read(g_dpy);
  }
  if (n > 0 && (fds[0].revents & (POLLERR | POLLHUP))) {
    /* the compositor went away */
    for (BKWindow *win = g_windows; win; win = win->next) {
      win->open = 0;
    }
    g_open = 0;
    g_quit = 1;
    return;
  }
  wl_display_dispatch_pending(g_dpy);
  if (n > 0 && g_wake_pipe[0] >= 0 && (fds[1].revents & POLLIN)) {
    char buf[64];
    while (read(g_wake_pipe[0], buf, sizeof(buf)) > 0) {
    }
    BK_image_poll();
    if (g_wake_cb) {
      g_wake_cb();
    }
  }
  for (int i = 0; n > 0 && i < nextra; i++) {
    if (fds[2 + i].revents & (POLLIN | POLLHUP | POLLERR)) {
      bk_loop_fd_ready(fds[2 + i].fd);
    }
  }
  run_timers();
  for (BKWindow *win = g_windows; win; win = win->next) {
    if (win->render_pending) {
      render_window(win);
    }
  }
  wl_display_flush(g_dpy);
}

void BK_window_run(void)
{
  g_quit = 0;
  while (!g_quit && (g_open > 0 || g_keep_alive)) {
    loop_turn(-1);
    if (g_stop && g_stop()) {
      break;
    }
  }
}

int BK_window_pump(void)
{
  if (g_dpy) {
    loop_turn(0);
  }
  return g_open;
}

const char *BK_window_backend(void)
{
  return "wayland";
}

#endif /* BUI_WINDOW_WAYLAND */
