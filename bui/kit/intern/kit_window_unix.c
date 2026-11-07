/* BUI-Awra: the Linux window host, chosen at run time: Wayland when the
 * session has one (WAYLAND_DISPLAY connects), else X11. BUI_BACKEND=x11 or
 * BUI_BACKEND=wayland forces one.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "kit_intern.h"
#include "kit_loop.h"

#if defined(BUI_WINDOW_WAYLAND) || defined(BUI_WINDOW_X11)

#  include <stdlib.h>
#  include <string.h>

#  ifdef BUI_WINDOW_WAYLAND
int bkwl_window_supported(void);
BKWindow * bkwl_window_new(BKContext *ctx, const char *title, int width, int height);
BKWindow * bkwl_window_new_ex(BKContext *ctx, const char *title, int width, int height, int flags);
void bkwl_window_free(BKWindow *win);
void bkwl_window_set_callbacks(BKWindow *win, const BKWindowCallbacks *cb);
void bkwl_window_show(BKWindow *win);
void bkwl_window_hide(BKWindow *win);
void bkwl_window_close(BKWindow *win);
int bkwl_window_is_open(BKWindow *win);
void bkwl_window_set_title(BKWindow *win, const char *title);
void bkwl_window_set_app_id(BKWindow *win, const char *app_id);
void bkwl_window_set_size(BKWindow *win, int width, int height);
void bkwl_window_get_size(BKWindow *win, int *r_width, int *r_height);
BuiRaster * bkwl_window_raster(BKWindow *win);
void bkwl_window_set_gpu(BKWindow *win, int on);
int bkwl_window_gpu(BKWindow *win);
int bkwl_window_frames(BKWindow *win);
void bkwl_window_render_now(BKWindow *win);
int bkwl_window_timer(BKWindow *win, int ms);
void bkwl_window_cancel_timer(BKWindow *win, int id);
void bkwl_window_request_frame(BKWindow *win, int vsync);
void * bkwl_window_handle(BKWindow *win);
void bkwl_window_wm(BKWindow *win, const char *key, double a, double b);
int bkwl_window_wm_get(BKWindow *win, const char *key, double *r_a, double *r_b);
void bkwl_window_run(void);
int bkwl_window_pump(void);
void bkwl_window_quit(void);
void bkwl_window_set_stop_check(int (*stop)(void));
int bkwl_window_wake_init(void (*cb)(void));
void bkwl_window_wake(void);
void bkwl_window_set_keep_alive(int on);
int bkwl_window_set_blur(BKWindow *win, int x, int y, int w, int h, int radius);
int bkwl_window_blur_available(BKWindow *win);
void bkwl_window_begin_move(BKWindow *win);
void bkwl_window_begin_resize(BKWindow *win, int edges);
void bkwl_window_show_menu(BKWindow *win, int x, int y);
int bkwl_window_state(BKWindow *win);
float bkwl_window_scale(BKWindow *win);
const char * bkwl_window_backend(void);
int bkwl_window_wake_ready(void);
void bkwl_loop_wake_host(void);
int bkwl_window_clipboard_set(BKWindow *win, const BKClipItem *items, int n);
char * bkwl_window_clipboard_formats(BKWindow *win);
void * bkwl_window_clipboard_read(BKWindow *win, const char *mime, size_t *r_len);
int bkwl_window_drag_start(BKWindow *win, const BKClipItem *items, int n, const uint32_t *icon,
                         int icon_w, int icon_h, int hot_x, int hot_y, int actions,
                         BKDragDoneFn done, void *user);
void bkwl_window_set_icon(BKWindow *win, const char *name, const uint32_t *const *pixels,
                        const int *sizes, int n);
void bkwl_window_activate(BKWindow *win, const char *startup_id);
void bkwl_window_set_transient_for_exported(BKWindow *win, const char *handle);
void bkwl_window_set_parent(BKWindow *win, BKWindow *parent, int modal);
#  endif
#  ifdef BUI_WINDOW_X11
int bkx11_window_supported(void);
BKWindow * bkx11_window_new(BKContext *ctx, const char *title, int width, int height);
BKWindow * bkx11_window_new_ex(BKContext *ctx, const char *title, int width, int height, int flags);
void bkx11_window_free(BKWindow *win);
void bkx11_window_set_callbacks(BKWindow *win, const BKWindowCallbacks *cb);
void bkx11_window_show(BKWindow *win);
void bkx11_window_hide(BKWindow *win);
void bkx11_window_close(BKWindow *win);
int bkx11_window_is_open(BKWindow *win);
void bkx11_window_set_title(BKWindow *win, const char *title);
void bkx11_window_set_app_id(BKWindow *win, const char *app_id);
void bkx11_window_set_size(BKWindow *win, int width, int height);
void bkx11_window_get_size(BKWindow *win, int *r_width, int *r_height);
BuiRaster * bkx11_window_raster(BKWindow *win);
void bkx11_window_set_gpu(BKWindow *win, int on);
int bkx11_window_gpu(BKWindow *win);
int bkx11_window_frames(BKWindow *win);
void bkx11_window_render_now(BKWindow *win);
int bkx11_window_timer(BKWindow *win, int ms);
void bkx11_window_cancel_timer(BKWindow *win, int id);
void bkx11_window_request_frame(BKWindow *win, int vsync);
void * bkx11_window_handle(BKWindow *win);
void bkx11_window_wm(BKWindow *win, const char *key, double a, double b);
int bkx11_window_wm_get(BKWindow *win, const char *key, double *r_a, double *r_b);
void bkx11_window_run(void);
int bkx11_window_pump(void);
void bkx11_window_quit(void);
void bkx11_window_set_stop_check(int (*stop)(void));
int bkx11_window_wake_init(void (*cb)(void));
void bkx11_window_wake(void);
void bkx11_window_set_keep_alive(int on);
int bkx11_window_set_blur(BKWindow *win, int x, int y, int w, int h, int radius);
int bkx11_window_blur_available(BKWindow *win);
void bkx11_window_begin_move(BKWindow *win);
void bkx11_window_begin_resize(BKWindow *win, int edges);
void bkx11_window_show_menu(BKWindow *win, int x, int y);
int bkx11_window_state(BKWindow *win);
float bkx11_window_scale(BKWindow *win);
const char * bkx11_window_backend(void);
int bkx11_window_wake_ready(void);
void bkx11_loop_wake_host(void);
int bkx11_window_clipboard_set(BKWindow *win, const BKClipItem *items, int n);
char * bkx11_window_clipboard_formats(BKWindow *win);
void * bkx11_window_clipboard_read(BKWindow *win, const char *mime, size_t *r_len);
int bkx11_window_drag_start(BKWindow *win, const BKClipItem *items, int n, const uint32_t *icon,
                         int icon_w, int icon_h, int hot_x, int hot_y, int actions,
                         BKDragDoneFn done, void *user);
void bkx11_window_set_icon(BKWindow *win, const char *name, const uint32_t *const *pixels,
                        const int *sizes, int n);
void bkx11_window_activate(BKWindow *win, const char *startup_id);
void bkx11_window_set_transient_for_exported(BKWindow *win, const char *handle);
void bkx11_window_set_parent(BKWindow *win, BKWindow *parent, int modal);
#  endif

/* 1 Wayland, 2 X11, 0 not chosen yet */
static int g_backend = 0;

static int backend(void)
{
  if (g_backend) {
    return g_backend;
  }
  const char *want = getenv("BUI_BACKEND");
#  ifdef BUI_WINDOW_WAYLAND
  if (!(want && !strcmp(want, "x11")) && bkwl_window_supported()) {
    return g_backend = 1;
  }
#  endif
#  ifdef BUI_WINDOW_X11
  (void)want;
  return g_backend = 2;
#  else
  return g_backend = 1;
#  endif
}

#  if defined(BUI_WINDOW_WAYLAND) && defined(BUI_WINDOW_X11)
#    define DISPATCH(call) (backend() == 1 ? bkwl_##call : bkx11_##call)
#  elif defined(BUI_WINDOW_WAYLAND)
#    define DISPATCH(call) ((void)backend(), bkwl_##call)
#  else
#    define DISPATCH(call) ((void)backend(), bkx11_##call)
#  endif

int BK_window_supported(void)
{
  return DISPATCH(window_supported)();
}

BKWindow * BK_window_new(BKContext *ctx, const char *title, int width, int height)
{
  return DISPATCH(window_new)(ctx, title, width, height);
}

BKWindow * BK_window_new_ex(BKContext *ctx, const char *title, int width, int height, int flags)
{
  return DISPATCH(window_new_ex)(ctx, title, width, height, flags);
}

void BK_window_free(BKWindow *win)
{
  DISPATCH(window_free)(win);
}

void BK_window_set_callbacks(BKWindow *win, const BKWindowCallbacks *cb)
{
  DISPATCH(window_set_callbacks)(win, cb);
}

void BK_window_show(BKWindow *win)
{
  DISPATCH(window_show)(win);
}

void BK_window_hide(BKWindow *win)
{
  DISPATCH(window_hide)(win);
}

void BK_window_close(BKWindow *win)
{
  DISPATCH(window_close)(win);
}

int BK_window_is_open(BKWindow *win)
{
  return DISPATCH(window_is_open)(win);
}

void BK_window_set_title(BKWindow *win, const char *title)
{
  DISPATCH(window_set_title)(win, title);
}

void BK_window_set_app_id(BKWindow *win, const char *app_id)
{
  DISPATCH(window_set_app_id)(win, app_id);
}

void BK_window_set_size(BKWindow *win, int width, int height)
{
  DISPATCH(window_set_size)(win, width, height);
}

void BK_window_get_size(BKWindow *win, int *r_width, int *r_height)
{
  DISPATCH(window_get_size)(win, r_width, r_height);
}

BuiRaster * BK_window_raster(BKWindow *win)
{
  return DISPATCH(window_raster)(win);
}

void BK_window_set_gpu(BKWindow *win, int on)
{
  DISPATCH(window_set_gpu)(win, on);
}

int BK_window_gpu(BKWindow *win)
{
  return DISPATCH(window_gpu)(win);
}

int BK_window_frames(BKWindow *win)
{
  return DISPATCH(window_frames)(win);
}

void BK_window_render_now(BKWindow *win)
{
  DISPATCH(window_render_now)(win);
}

int BK_window_timer(BKWindow *win, int ms)
{
  return DISPATCH(window_timer)(win, ms);
}

void BK_window_cancel_timer(BKWindow *win, int id)
{
  DISPATCH(window_cancel_timer)(win, id);
}

void BK_window_request_frame(BKWindow *win, int vsync)
{
  DISPATCH(window_request_frame)(win, vsync);
}

void * BK_window_handle(BKWindow *win)
{
  return DISPATCH(window_handle)(win);
}

void BK_window_wm(BKWindow *win, const char *key, double a, double b)
{
  DISPATCH(window_wm)(win, key, a, b);
}

int BK_window_wm_get(BKWindow *win, const char *key, double *r_a, double *r_b)
{
  return DISPATCH(window_wm_get)(win, key, r_a, r_b);
}

void BK_window_run(void)
{
  DISPATCH(window_run)();
}

int BK_window_pump(void)
{
  return DISPATCH(window_pump)();
}

void BK_window_quit(void)
{
  DISPATCH(window_quit)();
}

void BK_window_set_stop_check(int (*stop)(void))
{
  DISPATCH(window_set_stop_check)(stop);
}

int BK_window_wake_init(void (*cb)(void))
{
  return DISPATCH(window_wake_init)(cb);
}

void BK_window_wake(void)
{
  DISPATCH(window_wake)();
}

void BK_window_set_keep_alive(int on)
{
  DISPATCH(window_set_keep_alive)(on);
}

int BK_window_set_blur(BKWindow *win, int x, int y, int w, int h, int radius)
{
  return DISPATCH(window_set_blur)(win, x, y, w, h, radius);
}

int BK_window_blur_available(BKWindow *win)
{
  return DISPATCH(window_blur_available)(win);
}

void BK_window_begin_move(BKWindow *win)
{
  DISPATCH(window_begin_move)(win);
}

void BK_window_begin_resize(BKWindow *win, int edges)
{
  DISPATCH(window_begin_resize)(win, edges);
}

void BK_window_show_menu(BKWindow *win, int x, int y)
{
  DISPATCH(window_show_menu)(win, x, y);
}

int BK_window_state(BKWindow *win)
{
  return DISPATCH(window_state)(win);
}

float BK_window_scale(BKWindow *win)
{
  return DISPATCH(window_scale)(win);
}

const char * BK_window_backend(void)
{
  return DISPATCH(window_backend)();
}

int bk_window_wake_ready(void)
{
  return DISPATCH(window_wake_ready)();
}

void bk_loop_wake_host(void)
{
  if (g_backend) {
    DISPATCH(loop_wake_host)();
  }
}

int BK_window_clipboard_set(BKWindow *win, const BKClipItem *items, int n)
{
  return DISPATCH(window_clipboard_set)(win, items, n);
}

char * BK_window_clipboard_formats(BKWindow *win)
{
  return DISPATCH(window_clipboard_formats)(win);
}

void * BK_window_clipboard_read(BKWindow *win, const char *mime, size_t *r_len)
{
  return DISPATCH(window_clipboard_read)(win, mime, r_len);
}

int BK_window_drag_start(BKWindow *win, const BKClipItem *items, int n, const uint32_t *icon,
                         int icon_w, int icon_h, int hot_x, int hot_y, int actions,
                         BKDragDoneFn done, void *user)
{
  return DISPATCH(window_drag_start)(win, items, n, icon, icon_w, icon_h, hot_x, hot_y, actions,
                                     done, user);
}

void BK_window_set_icon(BKWindow *win, const char *name, const uint32_t *const *pixels,
                        const int *sizes, int n)
{
  DISPATCH(window_set_icon)(win, name, pixels, sizes, n);
}

void BK_window_activate(BKWindow *win, const char *startup_id)
{
  DISPATCH(window_activate)(win, startup_id);
}

void BK_window_set_transient_for_exported(BKWindow *win, const char *handle)
{
  DISPATCH(window_set_transient_for_exported)(win, handle);
}

void BK_window_set_parent(BKWindow *win, BKWindow *parent, int modal)
{
  DISPATCH(window_set_parent)(win, parent, modal);
}

#endif
