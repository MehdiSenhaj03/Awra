/* Native window host (X11, also Wayland through XWayland).
 *
 * The same BK_window_* API as kit_window_win32.c, on Xlib: input events go
 * straight to BK_* input (button and key mapping after Blender's
 * GHOST_SystemX11), redraw requests coalesce into one render per loop turn,
 * and the damaged rectangles of the CPU raster are put with XPutImage (or,
 * with BUI_GPU / BK_window_set_gpu, the raster draws with OpenGL and presents
 * through GLX on the same connection).
 * Timers, the cross-thread wake (a pipe) and frame pacing run in the select()
 * loop.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifdef BUI_WINDOW_X11
/* this host's BK_window_* are dispatched to (kit_window_unix.c) */
#  define BK_WIN_PREFIX bkx11
#  include "kit_window_unix.h"
#endif
#include "kit_intern.h"
#include "kit_loop.h"
#include "kit_transfer.h"

#ifdef BUI_WINDOW_X11

#  include <X11/Xatom.h>
#  include <X11/Xlib.h>
#  include <X11/Xresource.h>
#  include <X11/Xutil.h>
#  include <X11/cursorfont.h>
#  include <X11/keysym.h>

#  include <errno.h>
#  include <fcntl.h>
#  include <math.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <sys/select.h>
#  include <time.h>
#  include <unistd.h>

#  include <dlfcn.h>

/* libX11 is loaded when the first window opens, not linked: a build with the
 * X11 headers (every Linux wheel) imports fine on servers and containers
 * without X11, and draws native windows wherever X11 (or XWayland) exists.
 * Every Xlib function below goes through a pointer filled by dlsym. */
#  define BK_X11_FUNCS(F)   F(XAllocSizeHints) \
  F(XChangeProperty) \
  F(XCheckTypedWindowEvent) \
  F(XConvertSelection) \
  F(XCreateBitmapFromData) \
  F(XCreateFontCursor) \
  F(XCreateGC) \
  F(XCreateImage) \
  F(XCreatePixmapCursor) \
  F(XCreateWindow) \
  F(XDefineCursor) \
  F(XDestroyWindow) \
  F(XEventsQueued) \
  F(XFlush) \
  F(XFree) \
  F(XFreeGC) \
  F(XFreePixmap) \
  F(XGetSelectionOwner) \
  F(XGetWindowProperty) \
  F(XGetWMNormalHints) \
  F(XInitThreads) \
  F(XInternAtom) \
  F(XKeysymToString) \
  F(XLookupString) \
  F(XLowerWindow) \
  F(XMapWindow) \
  F(XMoveWindow) \
  F(XNextEvent) \
  F(XOpenDisplay) \
  F(XPeekEvent) \
  F(XPending) \
  F(XPutImage) \
  F(XQueryPointer) \
  F(XRaiseWindow) \
  F(XResizeWindow) \
  F(XResourceManagerString) \
  F(XrmDestroyDatabase) \
  F(XrmGetResource) \
  F(XrmGetStringDatabase) \
  F(XrmInitialize) \
  F(XSendEvent) \
  F(XSetInputFocus) \
  F(XSetSelectionOwner) \
  F(XSetWMNormalHints) \
  F(XSetWMProtocols) \
  F(XStoreName) \
  F(XWarpPointer) \
  F(XMatchVisualInfo) \
  F(XCreateColormap) \
  F(XUngrabPointer) \
  F(XIconifyWindow) \
  F(XUnmapWindow) \
  F(XDeleteProperty) \
  F(XGetAtomName) \
  F(XGrabPointer) \
  F(XTranslateCoordinates) \
  F(XSelectInput) \
  F(XExtendedMaxRequestSize) \
  F(XMaxRequestSize) \
  F(XQueryTree) \
  F(XGetWindowAttributes) \
  F(XGrabKeyboard) \
  F(XUngrabKeyboard) \
  F(XMoveResizeWindow) \
  F(XMapRaised)

#  define BK_X11_DECL(f) static __typeof__(&f) p_##f;
BK_X11_FUNCS(BK_X11_DECL)

/* 1 when libX11 is loaded and every function resolved. */
static int x11_load(void)
{
  static int state = 0; /* 0 untried, 1 loaded, -1 unavailable */
  if (state) {
    return state > 0;
  }
  void *lib = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    lib = dlopen("libX11.so", RTLD_NOW | RTLD_LOCAL);
  }
  state = -1;
  if (!lib) {
    return 0;
  }
#  define BK_X11_LOAD(f)     if (!(p_##f = (__typeof__(&f))dlsym(lib, #f))) {       return 0;     }
  BK_X11_FUNCS(BK_X11_LOAD)
  state = 1;
  return 1;
}

/* From here on, the Xlib names call through the loaded pointers. */
#  define XAllocSizeHints (*p_XAllocSizeHints)
#  define XChangeProperty (*p_XChangeProperty)
#  define XCheckTypedWindowEvent (*p_XCheckTypedWindowEvent)
#  define XConvertSelection (*p_XConvertSelection)
#  define XCreateBitmapFromData (*p_XCreateBitmapFromData)
#  define XCreateFontCursor (*p_XCreateFontCursor)
#  define XCreateGC (*p_XCreateGC)
#  define XCreateImage (*p_XCreateImage)
#  define XCreatePixmapCursor (*p_XCreatePixmapCursor)
#  define XCreateWindow (*p_XCreateWindow)
#  define XDefineCursor (*p_XDefineCursor)
#  define XDestroyWindow (*p_XDestroyWindow)
#  define XEventsQueued (*p_XEventsQueued)
#  define XFlush (*p_XFlush)
#  define XFree (*p_XFree)
#  define XFreeGC (*p_XFreeGC)
#  define XFreePixmap (*p_XFreePixmap)
#  define XGetSelectionOwner (*p_XGetSelectionOwner)
#  define XGetWindowProperty (*p_XGetWindowProperty)
#  define XGetWMNormalHints (*p_XGetWMNormalHints)
#  define XInitThreads (*p_XInitThreads)
#  define XInternAtom (*p_XInternAtom)
#  define XKeysymToString (*p_XKeysymToString)
#  define XLookupString (*p_XLookupString)
#  define XLowerWindow (*p_XLowerWindow)
#  define XMapWindow (*p_XMapWindow)
#  define XMoveWindow (*p_XMoveWindow)
#  define XNextEvent (*p_XNextEvent)
#  define XOpenDisplay (*p_XOpenDisplay)
#  define XPeekEvent (*p_XPeekEvent)
#  define XPending (*p_XPending)
#  define XPutImage (*p_XPutImage)
#  define XQueryPointer (*p_XQueryPointer)
#  define XRaiseWindow (*p_XRaiseWindow)
#  define XResizeWindow (*p_XResizeWindow)
#  define XResourceManagerString (*p_XResourceManagerString)
#  define XrmDestroyDatabase (*p_XrmDestroyDatabase)
#  define XrmGetResource (*p_XrmGetResource)
#  define XrmGetStringDatabase (*p_XrmGetStringDatabase)
#  define XrmInitialize (*p_XrmInitialize)
#  define XSendEvent (*p_XSendEvent)
#  define XSetInputFocus (*p_XSetInputFocus)
#  define XSetSelectionOwner (*p_XSetSelectionOwner)
#  define XSetWMNormalHints (*p_XSetWMNormalHints)
#  define XSetWMProtocols (*p_XSetWMProtocols)
#  define XStoreName (*p_XStoreName)
#  define XWarpPointer (*p_XWarpPointer)
#  define XMatchVisualInfo (*p_XMatchVisualInfo)
#  define XCreateColormap (*p_XCreateColormap)
#  define XUngrabPointer (*p_XUngrabPointer)
#  define XIconifyWindow (*p_XIconifyWindow)
#  define XUnmapWindow (*p_XUnmapWindow)
#  define XDeleteProperty (*p_XDeleteProperty)
#  define XGetAtomName (*p_XGetAtomName)
#  define XGrabPointer (*p_XGrabPointer)
#  define XTranslateCoordinates (*p_XTranslateCoordinates)
#  define XSelectInput (*p_XSelectInput)
#  define XExtendedMaxRequestSize (*p_XExtendedMaxRequestSize)
#  define XMaxRequestSize (*p_XMaxRequestSize)
#  define XQueryTree (*p_XQueryTree)
#  define XGetWindowAttributes (*p_XGetWindowAttributes)
#  define XGrabKeyboard (*p_XGrabKeyboard)
#  define XUngrabKeyboard (*p_XUngrabKeyboard)
#  define XMoveResizeWindow (*p_XMoveResizeWindow)
#  define XMapRaised (*p_XMapRaised)

#  define MAX_TIMERS 64

typedef struct Timer {
  int id; /* > 0 user timer, -1 the tooltip timer, 0 free */
  double due;
} Timer;

struct BKWindow {
  Window xid;
  BKContext *ctx;
  BKHost prev;
  BuiRaster *raster;
  BKWindowCallbacks cb;
  XImage *image;
  GC gc;
  int render_pending, frames, cursor, tip_id, next_timer, full;
  int want_frame, frame_vsync;
  double frame_start;
  float scale;
  int w, h;
  int flags, states, depth; /* BUI-Awra: BK_WINDOW_*, BK_WSTATE_*, visual depth */
  Visual *visual;
  Timer timers[MAX_TIMERS];
  Cursor cursors[16];
  struct BKWindow *next;
  /* BUI-Awra: popups in override-redirect windows (kit_popup_x11.inc.c) */
  Colormap cmap;
  BKPopupHost pophost;
  struct XPop *pops; /* BK_POPUP_SLOTS, allocated with the first popup */
  int root_x, root_y; /* the window's origin on the screen when popups were placed */
  int leave_pending;  /* the pointer left a surface: a leave unless it entered another */
  int mapped;         /* shown: the WM is asked for state changes */
};

static Display *g_dpy = NULL;
static Atom a_delete, a_clipboard, a_utf8, a_targets, a_bui_sel;
static BKWindow *g_windows = NULL;
static int g_open = 0, g_quit = 0, g_keep_alive = 0;
static int (*g_stop)(void) = NULL;
static void (*g_wake_cb)(void) = NULL;
static int g_wake_pipe[2] = {-1, -1};
static BKClipSet *g_clip = NULL; /* BUI-Awra: what we own on CLIPBOARD, by type */
static int x11_transfer_event(XEvent *ev); /* data transfer (below) */
/* popups (kit_popup_x11.inc.c) */
static int x11_popup_event(XEvent *ev);
static void x11_popups_init(BKWindow *win);
static void x11_popups_render(BKWindow *win);
static void x11_popups_free(BKWindow *win);
static void x11_popups_follow(BKWindow *win);
static void x11_pop_cursor(BKWindow *win, int slot, Cursor c);
static void x11_xdnd_aware(Window xid);

static double now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int open_display(void)
{
  if (g_dpy) {
    return 1;
  }
  if (!x11_load()) {
    return 0; /* no libX11: headless, or the Tk host from Python */
  }
  XInitThreads();
  g_dpy = XOpenDisplay(NULL);
  if (!g_dpy) {
    return 0;
  }
  a_delete = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
  a_clipboard = XInternAtom(g_dpy, "CLIPBOARD", False);
  a_utf8 = XInternAtom(g_dpy, "UTF8_STRING", False);
  a_targets = XInternAtom(g_dpy, "TARGETS", False);
  a_bui_sel = XInternAtom(g_dpy, "BUI_SELECTION", False);
  return 1;
}

int BK_window_supported(void)
{
  return open_display();
}

/* Xft.dpi, as GHOST_SystemX11 reads it, else 96. */
static float display_scale(void)
{
  const char *env = getenv("BUI_UI_SCALE");
  if (env) {
    return (float)atof(env);
  }
  /* BUI-Awra: GDK_SCALE, as GDK's X11 backend (a whole factor) */
  const char *gdk = getenv("GDK_SCALE");
  if (gdk && atoi(gdk) >= 1) {
    return (float)atoi(gdk);
  }
  float dpi = 96.0f;
  char *rms = XResourceManagerString(g_dpy);
  if (rms) {
    XrmInitialize();
    XrmDatabase db = XrmGetStringDatabase(rms);
    char *type = NULL;
    XrmValue value;
    if (db && XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
      dpi = (float)atof(value.addr);
    }
    if (db) {
      XrmDestroyDatabase(db);
    }
  }
  return BK_MAX(1.0f, dpi / 96.0f);
}

static BKWindow *find_window(Window xid)
{
  for (BKWindow *w = g_windows; w; w = w->next) {
    if (w->xid == xid) {
      return w;
    }
  }
  return NULL;
}

/* -- host callbacks -- */

static void host_request(void *data)
{
  BKWindow *win = (BKWindow *)data;
  if (win->xid) {
    win->render_pending = 1;
  }
}

/* GHOST_WindowX11::setWindowCursorShape */
static void host_set_cursor(void *data, int c)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->xid) {
    return;
  }
  c = BK_CLAMP(c, 0, 15);
  win->cursor = c;
  if (!win->cursors[c]) {
    /* BUI-Awra: the cursor theme's, by its names (GDK's), as Xcursor
     * loads them; the core font's when the theme has none */
    static Cursor (*load)(Display *, const char *) = NULL;
    static int tried = 0;
    if (!tried) {
      tried = 1;
      void *lib = dlopen("libXcursor.so.1", RTLD_NOW | RTLD_LOCAL);
      load = lib ? (Cursor(*)(Display *, const char *))dlsym(lib, "XcursorLibraryLoadCursor") :
                   NULL;
    }
    static const char *const names[16] = {
        "default",     "ew-resize",   "ns-resize", "text",        "crosshair", "pointer",
        "move",        NULL,          "nesw-resize", "nwse-resize", "not-allowed", "wait",
        "progress",    "copy",        "dnd-move",  "no-drop"};
    static const char *const fallback[16] = {
        "left_ptr", "sb_h_double_arrow", "sb_v_double_arrow", "xterm", "crosshair", "hand2",
        "fleur", NULL, "fd_double_arrow", "bd_double_arrow", "crossed_circle", "watch",
        "left_ptr_watch", "dnd-copy", "dnd-move", "dnd-none"};
    if (load && names[c]) {
      win->cursors[c] = load(g_dpy, names[c]);
      if (!win->cursors[c] && fallback[c]) {
        win->cursors[c] = load(g_dpy, fallback[c]);
      }
    }
  }
  if (!win->cursors[c]) {
    if (c == BK_CURSOR_NONE) { /* an empty 1x1 bitmap */
      static char zero = 0;
      Pixmap p = XCreateBitmapFromData(g_dpy, win->xid, &zero, 1, 1);
      XColor black = {0};
      win->cursors[c] = XCreatePixmapCursor(g_dpy, p, p, &black, &black, 0, 0);
      XFreePixmap(g_dpy, p);
    }
    else {
      static const unsigned shapes[16] = {XC_left_ptr,     XC_sb_h_double_arrow,
                                          XC_sb_v_double_arrow, XC_xterm,
                                          XC_crosshair,    XC_hand2,
                                          XC_fleur,        XC_left_ptr,
                                          XC_bottom_left_corner, XC_bottom_right_corner,
                                          XC_X_cursor,     XC_watch,
                                          XC_watch,        XC_plus,
                                          XC_fleur,        XC_X_cursor};
      win->cursors[c] = XCreateFontCursor(g_dpy, shapes[c]);
    }
  }
  XDefineCursor(g_dpy, win->xid, win->cursors[c]);
  for (int i = 0; win->pops && i < BK_POPUP_SLOTS; i++) { /* BUI-Awra: its popups too */
    x11_pop_cursor(win, i, win->cursors[c]);
  }
  XFlush(g_dpy);
}

static void host_warp(void *data, int x, int y)
{
  BKWindow *win = (BKWindow *)data;
  if (win->xid) {
    XWarpPointer(g_dpy, None, win->xid, 0, 0, 0, 0, x, y);
    XFlush(g_dpy);
  }
}

static void handle_event(XEvent *ev);
static void x11_set_state(BKWindow *win, int state);

/* BUI-Awra: plain text through the typed clipboard (below) */
static char *host_clipboard_get(void *data)
{
  size_t len;
  return (char *)BK_window_clipboard_read((BKWindow *)data, "text/plain;charset=utf-8", &len);
}

static void host_clipboard_set(void *data, const char *text)
{
  const BKClipItem it = {"text/plain;charset=utf-8", text ? text : "", text ? strlen(text) : 0};
  BK_window_clipboard_set((BKWindow *)data, &it, 1);
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
  /* slot -1: the tooltip; -2: custom nodes' frames (BK_TIMER_CUSTOM) */
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

/* -- rendering -- */

/* Moves the window's raster to the GPU (OpenGL through GLX on our Display);
 * BUI_raster_gpu_poll switches over at the next render, and failures leave
 * it on the software raster. */
static void gpu_attach(BKWindow *win)
{
  if (win->xid) {
    BuiGpuX11Window target = {g_dpy, (unsigned long)win->xid};
    BUI_raster_gpu_attach(win->raster, &target, 0);
  }
}

static void present(BKWindow *win, int all)
{
  if (BUI_raster_is_gpu(win->raster)) {
    BUI_raster_present_native(win->raster, NULL, all);
    if (!BUI_raster_is_gpu(win->raster)) {
      win->full = 1; /* device lost: the software raster redraws everything */
      win->render_pending = 1;
    }
    return;
  }
  const int w = BUI_raster_width(win->raster), h = BUI_raster_height(win->raster);
  if (!win->image || win->image->width != w || win->image->height != h) {
    if (win->image) {
      win->image->data = NULL; /* the raster owns the pixels */
      XDestroyImage(win->image);
    }
    Visual *vis = win->visual ? win->visual : DefaultVisual(g_dpy, DefaultScreen(g_dpy));
    win->image = XCreateImage(g_dpy, vis, (unsigned)(win->depth ? win->depth : 24), ZPixmap, 0,
                              NULL, (unsigned)w, (unsigned)h, 32, w * 4);
    all = 1;
  }
  /* 0xAARRGGBB words are B, G, R, A bytes: the 24-bit TrueColor layout */
  win->image->data = (char *)BUI_raster_pixels(win->raster);
  if (all) {
    XPutImage(g_dpy, win->xid, win->gc, win->image, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
  }
  else {
    const int(*rects)[4];
    const int n = BUI_raster_damage(win->raster, &rects);
    for (int i = 0; i < n; i++) {
      XPutImage(g_dpy, win->xid, win->gc, win->image, rects[i][0], rects[i][1], rects[i][0],
                rects[i][1], (unsigned)rects[i][2], (unsigned)rects[i][3]);
    }
  }
  XFlush(g_dpy);
}

static void do_render(BKWindow *win)
{
  const int w = BK_MAX(1, win->w), h = BK_MAX(1, win->h);
  const int resized = BUI_raster_width(win->raster) != w || BUI_raster_height(win->raster) != h;
  if (BUI_raster_gpu_poll(win->raster)) {
    win->full = 1; /* the renderer changed: its frame starts empty */
  }
  const int full = win->full || resized;
  win->full = 0;
  const int regions = BK_render(win->ctx, win->raster, w, h, full);
  if (regions || full) {
    present(win, full);
    win->frames++;
  }
}

static double refresh_ms(void)
{
  return 1000.0 / 60.0;
}

static void render_window(BKWindow *win)
{
  if (win->want_frame && win->cb.on_frame) {
    win->want_frame = 0;
    win->frame_start = now_ms();
    win->cb.on_frame(win->cb.data, win);
  }
  win->render_pending = 0;
  do_render(win);
  x11_popups_render(win); /* BUI-Awra */
  if (win->want_frame) {
    bk_frame_ms = now_ms() - win->frame_start;
    bk_frame_budget_ms = win->frame_vsync ? refresh_ms() : 0.0;
    win->render_pending = 1;
  }
  else {
    bk_frame_ms = 0.0;
  }
  if (BK_animating(win->ctx)) {
    win->render_pending = 1; /* paced by the loop's frame wait */
  }
}

/* -- input mapping (GHOST_SystemX11::processEvent) -- */

static int mods_of(unsigned state)
{
  int m = 0;
  if (state & ShiftMask) {
    m |= BK_SHIFT;
  }
  if (state & ControlMask) {
    m |= BK_CTRL;
  }
  if (state & Mod1Mask) {
    m |= BK_ALT;
  }
  if (state & Button1Mask) {
    m |= BK_BUTTON1;
  }
  return m;
}

static void handle_key(BKWindow *win, XKeyEvent *ke, int press)
{
  bk_last_key_code = (int)ke->keycode - 8;
  char text[32] = {0};
  KeySym sym = 0;
  const int len = XLookupString(ke, text, sizeof(text) - 1, &sym, NULL);
  text[BK_MAX(0, len)] = 0;
  const char *name = XKeysymToString(sym); /* the X keysym names Tk and BUI use */
  if (!name) {
    name = "";
  }
  const int mods = mods_of(ke->state);
  if (!press) {
    BK_key_release(win->ctx, name, mods);
    return;
  }
  /* control characters are not text */
  const int printable = len > 0 && (unsigned char)text[0] >= 0x20 && text[0] != 0x7f &&
                        !(mods & BK_CTRL);
  BK_key(win->ctx, printable ? text : "", name, mods);
}

static void close_window(BKWindow *win)
{
  if (!win->xid) {
    return;
  }
  x11_popups_free(win); /* BUI-Awra: before their parent */
  XDestroyWindow(g_dpy, win->xid);
  XFlush(g_dpy);
  win->xid = 0;
  g_open--;
}

static void handle_event(XEvent *ev)
{
  if (x11_transfer_event(ev)) { /* BUI-Awra: clipboard, drags in and out */
    return;
  }
  if (x11_popup_event(ev)) { /* BUI-Awra: a popup's own window */
    return;
  }
  BKWindow *win = find_window(ev->xany.window);
  if (!win || !win->xid) {
    return;
  }
  BKContext *ctx = win->ctx;
  switch (ev->type) {
    case Expose:
      if (ev->xexpose.count == 0) {
        win->full = 1;
        win->render_pending = 1;
      }
      break;
    case ConfigureNotify:
      if (ev->xconfigure.width != win->w || ev->xconfigure.height != win->h) {
        win->w = ev->xconfigure.width;
        win->h = ev->xconfigure.height;
        win->render_pending = 1;
      }
      x11_popups_follow(win); /* BUI-Awra: popups move with their window */
      break;
    case UnmapNotify:
      BK_close_popups(ctx); /* BUI-Awra: minimized: popups close, as GTK's */
      break;
    case MotionNotify:
      win->leave_pending = 0;
      BK_motion(ctx, ev->xmotion.x, ev->xmotion.y, mods_of(ev->xmotion.state));
      break;
    case LeaveNotify:
      /* BUI-Awra: a leave into one of its popups is no leave (decided once
       * the events that came with it are read); grab crossings neither */
      if (ev->xcrossing.mode == NotifyNormal &&
          !(ev->xcrossing.state & (Button1Mask | Button2Mask | Button3Mask)))
      {
        win->leave_pending = 1;
      }
      break;
    case ButtonPress: {
      const XButtonEvent *b = &ev->xbutton;
      const int mods = mods_of(b->state);
      switch (b->button) {
        case Button1:
          XSetInputFocus(g_dpy, win->xid, RevertToParent, CurrentTime);
          BK_press(ctx, b->x, b->y, mods | BK_BUTTON1);
          break;
        case Button2:
          BK_button(ctx, b->x, b->y, 2, 0, mods);
          break;
        case Button3:
          BK_context_menu(ctx, b->x, b->y, mods);
          break;
        case Button4: /* wheel up */
        case Button5:
          BK_scroll(ctx, b->x, b->y, 0.0f, b->button == Button4 ? 1.0f : -1.0f,
                    mods & ~BK_BUTTON1);
          break;
        case 6: /* horizontal wheel: left, right (widgets: Shift+wheel, as GHOST maps it) */
        case 7:
          BK_scroll(ctx, b->x, b->y, b->button == 6 ? -1.0f : 1.0f, 0.0f, mods & ~BK_BUTTON1);
          break;
      }
      break;
    }
    case ButtonRelease: {
      const XButtonEvent *b = &ev->xbutton;
      const int mods = mods_of(b->state) & ~BK_BUTTON1;
      if (b->button == Button1) {
        BK_release(ctx, b->x, b->y, mods);
      }
      else if (b->button == Button2 || b->button == Button3) {
        BK_button(ctx, b->x, b->y, (int)b->button == Button2 ? 2 : 3, 1, mods);
      }
      break;
    }
    case KeyPress:
    case KeyRelease:
      if (ev->type == KeyRelease && XEventsQueued(g_dpy, QueuedAfterReading)) {
        /* autorepeat: a release followed by a press at the same time */
        XEvent next;
        XPeekEvent(g_dpy, &next);
        if (next.type == KeyPress && next.xkey.time == ev->xkey.time &&
            next.xkey.keycode == ev->xkey.keycode)
        {
          break;
        }
      }
      handle_key(win, &ev->xkey, ev->type == KeyPress);
      break;
    case FocusIn:
    case FocusOut: {
      const int st = ev->type == FocusIn ? (win->states | BK_WSTATE_ACTIVE) :
                                           (win->states & ~BK_WSTATE_ACTIVE);
      if (st != win->states) {
        win->states = st;
        if (win->cb.on_state) {
          win->cb.on_state(win->cb.data, win, win->states);
        }
      }
      break;
    }
    case ClientMessage:
      if ((Atom)ev->xclient.data.l[0] == a_delete) {
        if (!win->cb.on_close || win->cb.on_close(win->cb.data, win)) {
          close_window(win);
        }
      }
      break;
  }
}

/* -- windows -- */

BKWindow *BK_window_new(BKContext *ctx, const char *title, int width, int height)
{
  return BK_window_new_ex(ctx, title, width, height, 0);
}

BKWindow *BK_window_new_ex(BKContext *ctx, const char *title, int width, int height, int flags)
{
  if (!open_display()) {
    return NULL;
  }
  BKWindow *win = (BKWindow *)calloc(1, sizeof(BKWindow));
  win->ctx = ctx;
  win->prev = ctx->host;
  win->raster = BUI_raster_create(1, 1);
  win->next_timer = 1;
  win->scale = display_scale();
  BK_set_scale(win->scale);
  win->w = (int)(width * win->scale);
  win->h = (int)(height * win->scale);
  const int screen = DefaultScreen(g_dpy);
  XSetWindowAttributes attr;
  memset(&attr, 0, sizeof(attr));
  attr.background_pixel = BlackPixel(g_dpy, screen);
  attr.event_mask = ExposureMask | StructureNotifyMask | PointerMotionMask | ButtonPressMask |
                    ButtonReleaseMask | KeyPressMask | KeyReleaseMask | LeaveWindowMask |
                    FocusChangeMask | PropertyChangeMask; /* PropertyNotify: INCR transfers */
  win->flags = flags;
  XVisualInfo vi;
  unsigned long mask = CWBackPixel | CWEventMask;
  int depth = CopyFromParent;
  Visual *visual = CopyFromParent;
  if ((flags & BK_WINDOW_TRANSPARENT) &&
      XMatchVisualInfo(g_dpy, screen, 32, TrueColor, &vi)) /* BUI-Awra: per-pixel alpha */
  {
    attr.colormap = XCreateColormap(g_dpy, RootWindow(g_dpy, screen), vi.visual, AllocNone);
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    mask |= CWColormap | CWBorderPixel;
    depth = 32;
    visual = vi.visual;
    win->visual = vi.visual;
    win->depth = 32;
    win->cmap = attr.colormap;
    ctx->transparent = true;
  }
  win->xid = XCreateWindow(g_dpy, RootWindow(g_dpy, screen), 0, 0, (unsigned)win->w,
                           (unsigned)win->h, 0, depth, InputOutput, visual, mask, &attr);
  if (!win->xid) {
    BUI_raster_free(win->raster);
    free(win);
    return NULL;
  }
  XSetWMProtocols(g_dpy, win->xid, &a_delete, 1);
  x11_xdnd_aware(win->xid); /* BUI-Awra: file drops (XDND) */
  if (flags & BK_WINDOW_UNDECORATED) { /* _MOTIF_WM_HINTS: no decorations */
    const long hints[5] = {2, 0, 0, 0, 0};
    const Atom motif = XInternAtom(g_dpy, "_MOTIF_WM_HINTS", False);
    XChangeProperty(g_dpy, win->xid, motif, motif, 32, PropModeReplace,
                    (const unsigned char *)hints, 5);
  }
  win->gc = XCreateGC(g_dpy, win->xid, 0, NULL);
  BK_window_set_title(win, title);
  win->next = g_windows;
  g_windows = win;
  g_open++;
  if (BUI_gpu_default()) {
    gpu_attach(win);
  }
  BKHost host = {win,           host_request,       host_set_cursor, host_warp,
                 host_clipboard_get, host_clipboard_set, host_schedule, host_emit};
  BK_context_set_host(ctx, &host);
  x11_popups_init(win); /* BUI-Awra */
  return win;
}

void BK_window_free(BKWindow *win)
{
  if (!win) {
    return;
  }
  close_window(win);
  for (BKWindow **p = &g_windows; *p; p = &(*p)->next) {
    if (*p == win) {
      *p = win->next;
      break;
    }
  }
  if (win->image) {
    win->image->data = NULL;
    XDestroyImage(win->image);
  }
  if (win->gc) {
    XFreeGC(g_dpy, win->gc);
  }
  if (win->ctx) {
    BK_context_set_host(win->ctx, &win->prev);
    if (win->ctx->popup_host == &win->pophost) {
      bk_popup_host_set(win->ctx, NULL);
    }
  }
  BUI_raster_free(win->raster);
  free(win->pops);
  free(win);
}

void BK_window_set_callbacks(BKWindow *win, const BKWindowCallbacks *cb)
{
  win->cb = *cb;
}

void BK_window_show(BKWindow *win)
{
  if (win->xid) {
    XMapWindow(g_dpy, win->xid);
    XFlush(g_dpy);
    win->render_pending = 1;
    win->mapped = 1;
  }
}

void BK_window_close(BKWindow *win)
{
  close_window(win);
}

int BK_window_is_open(BKWindow *win)
{
  return win && win->xid != 0;
}

void BK_window_set_title(BKWindow *win, const char *title)
{
  if (!win->xid) {
    return;
  }
  const char *t = title ? title : "";
  XChangeProperty(g_dpy, win->xid, XInternAtom(g_dpy, "_NET_WM_NAME", False), a_utf8, 8,
                  PropModeReplace, (const unsigned char *)t, (int)strlen(t));
  XStoreName(g_dpy, win->xid, t);
  XFlush(g_dpy);
}

void BK_window_set_size(BKWindow *win, int width, int height)
{
  if (win->xid) {
    /* X resizes asynchronously; take the size now, as Win32's SetWindowPos
     * does (a ConfigureNotify from the window manager corrects it later) */
    win->w = BK_MAX(1, (int)(width * win->scale));
    win->h = BK_MAX(1, (int)(height * win->scale));
    XResizeWindow(g_dpy, win->xid, (unsigned)win->w, (unsigned)win->h);
    XFlush(g_dpy);
    win->render_pending = 1;
  }
}

void BK_window_get_size(BKWindow *win, int *w, int *h)
{
  *w = win->xid ? win->w : 0;
  *h = win->xid ? win->h : 0;
}

BuiRaster *BK_window_raster(BKWindow *win)
{
  return win->raster;
}

void BK_window_set_gpu(BKWindow *win, int on)
{
  if (win && on) {
    gpu_attach(win);
    host_request(win);
  }
}

int BK_window_gpu(BKWindow *win)
{
  return win && BUI_raster_is_gpu(win->raster);
}

int BK_window_frames(BKWindow *win)
{
  return win->frames;
}

void BK_window_render_now(BKWindow *win)
{
  if (win->xid) {
    win->full = 1;
    render_window(win);
  }
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
  return (void *)(uintptr_t)win->xid;
}

/* GHOST_WindowX11: size hints for minsize / maxsize / resizable, moves for pos */
void BK_window_wm(BKWindow *win, const char *key, double a, double b)
{
  if (!win->xid) {
    return;
  }
  if (!strcmp(key, "pos")) {
    XMoveWindow(g_dpy, win->xid, (int)a, (int)b);
  }
  else if (!strcmp(key, "minsize") || !strcmp(key, "maxsize") || !strcmp(key, "resizable")) {
    XSizeHints *hints = XAllocSizeHints();
    long supplied;
    XGetWMNormalHints(g_dpy, win->xid, hints, &supplied);
    if (!strcmp(key, "minsize")) {
      hints->flags |= PMinSize;
      hints->min_width = (int)(a * win->scale);
      hints->min_height = (int)(b * win->scale);
    }
    else if (!strcmp(key, "maxsize")) {
      hints->flags |= PMaxSize;
      hints->max_width = (int)(a * win->scale);
      hints->max_height = (int)(b * win->scale);
    }
    else if (a == 0 || b == 0) { /* not resizable along an axis: pin it */
      hints->flags |= PMinSize | PMaxSize;
      hints->min_width = hints->max_width = a == 0 ? win->w : 1;
      hints->min_height = hints->max_height = b == 0 ? win->h : 1;
      if (a != 0) {
        hints->max_width = 1 << 15;
      }
      if (b != 0) {
        hints->max_height = 1 << 15;
      }
    }
    XSetWMNormalHints(g_dpy, win->xid, hints);
    XFree(hints);
  }
  else if (!strcmp(key, "lift")) {
    XRaiseWindow(g_dpy, win->xid);
  }
  else if (!strcmp(key, "lower")) {
    XLowerWindow(g_dpy, win->xid);
  }
  else if (!strcmp(key, "focus")) {
    XSetInputFocus(g_dpy, win->xid, RevertToParent, CurrentTime);
  }
  else if (!strcmp(key, "state")) {
    x11_set_state(win, (int)a);
  }
  XFlush(g_dpy);
}

int BK_window_wm_get(BKWindow *win, const char *key, double *r_a, double *r_b)
{
  *r_a = *r_b = 0.0;
  if (!g_dpy) {
    return 0;
  }
  const int screen = DefaultScreen(g_dpy);
  if (!strcmp(key, "screen")) {
    *r_a = DisplayWidth(g_dpy, screen);
    *r_b = DisplayHeight(g_dpy, screen);
    return 1;
  }
  if (!strcmp(key, "scale")) {
    *r_a = *r_b = win ? win->scale : display_scale();
    return 1;
  }
  if (!win || !win->xid) {
    return 0;
  }
  if (!strcmp(key, "pointer") || !strcmp(key, "pos")) {
    Window root, child;
    int rx, ry, wx, wy;
    unsigned mask;
    XQueryPointer(g_dpy, win->xid, &root, &child, &rx, &ry, &wx, &wy, &mask);
    if (!strcmp(key, "pointer")) {
      *r_a = rx;
      *r_b = ry;
    }
    else {
      *r_a = rx - wx;
      *r_b = ry - wy;
    }
    return 1;
  }
  return 0;
}

/* -- the loop -- */

static void run_timers(void)
{
  const double t = now_ms();
  for (BKWindow *win = g_windows; win; win = win->next) {
    for (int i = 0; i < MAX_TIMERS && win->xid; i++) {
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
  bk_loop_run_due();
}

/* Milliseconds until the next timer or frame (-1: nothing to wait for). */
static double next_wait(void)
{
  double wait = -1.0;
  const double t = now_ms();
  for (BKWindow *win = g_windows; win; win = win->next) {
    if (!win->xid) {
      continue;
    }
    if (win->render_pending) {
      return 0.0;
    }
    for (int i = 0; i < MAX_TIMERS; i++) {
      if (win->timers[i].id != 0) {
        const double d = BK_MAX(0.0, win->timers[i].due - t);
        wait = wait < 0 ? d : BK_MIN(wait, d);
      }
    }
  }
  return (double)bk_loop_timeout_ms(wait < 0 ? -1 : (int)(wait + 0.999));
}

static void drain_wake(void)
{
  char buf[64];
  while (g_wake_pipe[0] >= 0 && read(g_wake_pipe[0], buf, sizeof(buf)) > 0) {
  }
  BK_image_poll(); /* decoded images: damage what shows them */
  if (g_wake_cb) {
    g_wake_cb();
  }
}

/* One turn: events, timers, renders; blocks up to `block_ms` (-1: forever). */
static void turn(double block_ms)
{
  const int fd = ConnectionNumber(g_dpy);
  if (!XPending(g_dpy) && block_ms != 0.0) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    int maxfd = fd;
    if (g_wake_pipe[0] >= 0) {
      FD_SET(g_wake_pipe[0], &set);
      maxfd = BK_MAX(maxfd, g_wake_pipe[0]);
    }
    /* watched descriptors (BK_loop_watch_fd) */
    int extra[8];
    const int nextra = bk_loop_fd_count() < 8 ? bk_loop_fd_count() : 8;
    for (int i = 0; i < nextra; i++) {
      extra[i] = bk_loop_fd(i);
      if (extra[i] >= 0 && extra[i] < FD_SETSIZE) {
        FD_SET(extra[i], &set);
        maxfd = BK_MAX(maxfd, extra[i]);
      }
    }
    struct timeval tv, *ptv = NULL;
    if (block_ms > 0.0) {
      tv.tv_sec = (long)(block_ms / 1000.0);
      tv.tv_usec = (long)(fmod(block_ms, 1000.0) * 1000.0);
      ptv = &tv;
    }
    const int ready = select(maxfd + 1, &set, NULL, NULL, ptv);
    if (ready > 0 && g_wake_pipe[0] >= 0 && FD_ISSET(g_wake_pipe[0], &set)) {
      drain_wake();
    }
    for (int i = 0; ready > 0 && i < nextra; i++) {
      if (extra[i] >= 0 && extra[i] < FD_SETSIZE && FD_ISSET(extra[i], &set)) {
        bk_loop_fd_ready(extra[i]);
      }
    }
  }
  while (XPending(g_dpy)) {
    XEvent ev;
    XNextEvent(g_dpy, &ev);
    handle_event(&ev);
  }
  for (BKWindow *win = g_windows; win; win = win->next) {
    if (win->leave_pending) { /* BUI-Awra: it left the window and its popups */
      win->leave_pending = 0;
      if (win->xid) {
        BK_motion(win->ctx, -100000, -100000, 0);
      }
    }
  }
  run_timers();
  int rendered = 0;
  for (BKWindow *win = g_windows; win; win = win->next) {
    if (win->xid && win->render_pending) {
      render_window(win);
      rendered |= win->render_pending; /* animating: another frame wanted */
    }
  }
  if (rendered) {
    usleep((useconds_t)(refresh_ms() * 1000.0)); /* pace animation frames */
  }
}

void BK_window_set_stop_check(int (*stop)(void))
{
  g_stop = stop;
}

void BK_window_quit(void)
{
  g_quit = 1;
  BK_window_wake();
}

void BK_window_run(void)
{
  if (!g_dpy) {
    return;
  }
  g_quit = 0;
  while ((g_open > 0 || g_keep_alive) && !g_quit) {
    turn(next_wait());
    if (g_stop && g_stop()) {
      break;
    }
  }
}

int BK_window_pump(void)
{
  if (g_dpy) {
    turn(0.0);
  }
  return g_open;
}

int bk_window_wake_ready(void)
{
  if (g_wake_pipe[0] < 0 && pipe(g_wake_pipe) == 0) {
    fcntl(g_wake_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(g_wake_pipe[1], F_SETFL, O_NONBLOCK);
  }
  return g_wake_pipe[0] >= 0;
}

int BK_window_wake_init(void (*cb)(void))
{
  g_wake_cb = cb;
  return bk_window_wake_ready();
}

/* Thread-safe: wakes the loop, which calls the wake callback. */
void BK_window_wake(void)
{
  if (g_wake_pipe[1] >= 0) {
    const char c = 1;
    (void)!write(g_wake_pipe[1], &c, 1);
  }
}

void BK_window_set_keep_alive(int on)
{
  g_keep_alive = on;
}

/* -- BUI-Awra: the extensions of BUI_kit.h ------------------------------------ */

void bk_loop_wake_host(void)
{
  BK_window_wake();
}

void BK_window_set_app_id(BKWindow *win, const char *app_id)
{
  if (!win->xid || !app_id) {
    return;
  }
  /* WM_CLASS: instance and class, both the application id */
  const size_t n = strlen(app_id);
  char *v = (char *)malloc(n * 2 + 2);
  if (v) {
    memcpy(v, app_id, n + 1);
    memcpy(v + n + 1, app_id, n + 1);
    XChangeProperty(g_dpy, win->xid, XA_WM_CLASS, XA_STRING, 8, PropModeReplace,
                    (const unsigned char *)v, (int)(n * 2 + 2));
    free(v);
  }
}

/* KWin's _KDE_NET_WM_BLUR_BEHIND_REGION: a rounded box as scanline rects. */
int BK_window_set_blur(BKWindow *win, int x, int y, int w, int h, int radius)
{
  if (!win->xid) {
    return 0;
  }
  const Atom atom = XInternAtom(g_dpy, "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
  if (w <= 0 || h <= 0 || !(win->flags & BK_WINDOW_TRANSPARENT)) {
    XDeleteProperty(g_dpy, win->xid, atom);
    XFlush(g_dpy);
    return 1;
  }
  const float s = win->scale;
  x = (int)(x * s);
  y = (int)(y * s);
  w = (int)(w * s);
  h = (int)(h * s);
  int r = BK_MAX(0, BK_MIN((int)(radius * s), BK_MIN(w, h) / 2));
  long *rects = (long *)malloc(sizeof(long) * 4 * (size_t)(2 * r + 1));
  if (!rects) {
    return 0;
  }
  int n = 0;
  rects[n++] = x;
  rects[n++] = y + r;
  rects[n++] = w;
  rects[n++] = h - 2 * r;
  for (int i = 0; i < r; i++) {
    const double dy = r - i - 0.5;
    const int inset = (int)floor(r - sqrt((double)r * r - dy * dy) + 0.5);
    const long top[4] = {x + inset, y + i, w - 2 * inset, 1};
    const long bottom[4] = {x + inset, y + h - i - 1, w - 2 * inset, 1};
    memcpy(rects + n, top, sizeof(top));
    n += 4;
    memcpy(rects + n, bottom, sizeof(bottom));
    n += 4;
  }
  XChangeProperty(g_dpy, win->xid, atom, XA_CARDINAL, 32, PropModeReplace,
                  (const unsigned char *)rects, n);
  free(rects);
  XFlush(g_dpy);
  return 1;
}

int BK_window_blur_available(BKWindow *win)
{
  /* X11 can't ask the compositor; KWin honours the property, others ignore it */
  return (win && win->depth == 32 && getenv("KDE_FULL_SESSION")) ? 1 : 0;
}

/* _NET_WM_MOVERESIZE: the window manager takes over the drag. */
static void net_moveresize(BKWindow *win, long direction)
{
  Window root, child;
  int rx, ry, wx, wy;
  unsigned m;
  XQueryPointer(g_dpy, win->xid, &root, &child, &rx, &ry, &wx, &wy, &m);
  XUngrabPointer(g_dpy, CurrentTime);
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = win->xid;
  ev.xclient.message_type = XInternAtom(g_dpy, "_NET_WM_MOVERESIZE", False);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = rx;
  ev.xclient.data.l[1] = ry;
  ev.xclient.data.l[2] = direction;
  ev.xclient.data.l[3] = Button1;
  ev.xclient.data.l[4] = 1;
  XSendEvent(g_dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XFlush(g_dpy);
  BK_release(win->ctx, wx, wy, 0); /* the window manager has the button now */
}

void BK_window_begin_move(BKWindow *win)
{
  if (win->xid) {
    net_moveresize(win, 8);
  }
}

void BK_window_begin_resize(BKWindow *win, int edges)
{
  if (!win->xid) {
    return;
  }
  static const int dirs[16] = {
      /* 0 */ -1, /* T */ 1, /* B */ 5, -1, /* L */ 7, /* TL */ 0, /* BL */ 6, -1,
      /* R */ 3, /* TR */ 2, /* BR */ 4, -1, -1, -1, -1, -1};
  const int d = dirs[edges & 15];
  if (d >= 0) {
    net_moveresize(win, d);
  }
}

void BK_window_show_menu(BKWindow *win, int x, int y) {}

static void net_wm_state(BKWindow *win, long action, const char *a1, const char *a2)
{
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = win->xid;
  ev.xclient.message_type = XInternAtom(g_dpy, "_NET_WM_STATE", False);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = action;
  ev.xclient.data.l[1] = (long)XInternAtom(g_dpy, a1, False);
  ev.xclient.data.l[2] = a2 ? (long)XInternAtom(g_dpy, a2, False) : 0;
  ev.xclient.data.l[3] = 1;
  XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}

int BK_window_state(BKWindow *win)
{
  return win->states;
}

float BK_window_scale(BKWindow *win)
{
  return win->scale;
}

void BK_window_hide(BKWindow *win)
{
  if (win->xid) {
    BK_close_popups(win->ctx); /* BUI-Awra: hidden with their window */
    XUnmapWindow(g_dpy, win->xid);
    XFlush(g_dpy);
    win->states |= BK_WSTATE_HIDDEN;
    win->mapped = 0;
  }
}

const char *BK_window_backend(void)
{
  return "x11";
}

static void x11_set_state(BKWindow *win, int state)
{
  switch (state) {
    case 0:
      net_wm_state(win, 0, "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_MAXIMIZED_HORZ");
      net_wm_state(win, 0, "_NET_WM_STATE_FULLSCREEN", NULL);
      win->states &= ~(BK_WSTATE_MAXIMIZED | BK_WSTATE_FULLSCREEN);
      break;
    case 1:
      XIconifyWindow(g_dpy, win->xid, DefaultScreen(g_dpy));
      break;
    case 3:
      net_wm_state(win, 1, "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_MAXIMIZED_HORZ");
      win->states |= BK_WSTATE_MAXIMIZED;
      break;
    case 4:
      net_wm_state(win, 1, "_NET_WM_STATE_FULLSCREEN", NULL);
      win->states |= BK_WSTATE_FULLSCREEN;
      break;
  }
  if (win->cb.on_state) {
    win->cb.on_state(win->cb.data, win, win->states);
  }
}


/* =========================================================================
 * BUI-Awra: data transfer. The clipboard by type (large data through INCR
 * both ways), XDND drags out of the application (the source side, with a
 * drag image window) and file drops into it (the target side).
 * ========================================================================= */

static Atom a_incr, a_xdnd_aware, a_xdnd_enter, a_xdnd_position, a_xdnd_status, a_xdnd_leave,
    a_xdnd_drop, a_xdnd_finished, a_xdnd_selection, a_xdnd_type_list, a_xdnd_copy, a_xdnd_move,
    a_uri_list;

static void transfer_atoms(void)
{
  static int done = 0;
  if (done || !g_dpy) {
    return;
  }
  done = 1;
  a_incr = XInternAtom(g_dpy, "INCR", False);
  a_xdnd_aware = XInternAtom(g_dpy, "XdndAware", False);
  a_xdnd_enter = XInternAtom(g_dpy, "XdndEnter", False);
  a_xdnd_position = XInternAtom(g_dpy, "XdndPosition", False);
  a_xdnd_status = XInternAtom(g_dpy, "XdndStatus", False);
  a_xdnd_leave = XInternAtom(g_dpy, "XdndLeave", False);
  a_xdnd_drop = XInternAtom(g_dpy, "XdndDrop", False);
  a_xdnd_finished = XInternAtom(g_dpy, "XdndFinished", False);
  a_xdnd_selection = XInternAtom(g_dpy, "XdndSelection", False);
  a_xdnd_type_list = XInternAtom(g_dpy, "XdndTypeList", False);
  a_xdnd_copy = XInternAtom(g_dpy, "XdndActionCopy", False);
  a_xdnd_move = XInternAtom(g_dpy, "XdndActionMove", False);
  a_uri_list = XInternAtom(g_dpy, "text/uri-list", False);
}

static void x11_xdnd_aware(Window xid)
{
  transfer_atoms();
  const long version = 5;
  XChangeProperty(g_dpy, xid, a_xdnd_aware, XA_ATOM, 32, PropModeReplace,
                  (const unsigned char *)&version, 1);
}

/* the most a property takes at once (bigger data goes by INCR) */
static size_t chunk_max(void)
{
  long n = XExtendedMaxRequestSize(g_dpy);
  if (n <= 0) {
    n = XMaxRequestSize(g_dpy);
  }
  size_t bytes = n > 64 ? (size_t)n * 4 - 256 : 65536;
  return bytes > 262144 ? 262144 : bytes;
}

/* -- the source side of selections ---------------------------------------- */

/* a drag out in progress */
static struct {
  int active;
  BKWindow *win;
  BKClipSet *set;
  BKDragDoneFn done;
  void *user;
  int actions;
  Window target; /* the XdndAware window under the pointer, 0 none */
  int version, accepted, waiting, pending, dropped;
  Atom action;
  int rx, ry; /* the pointer, root coordinates */
  Time time;
  double drop_at;
  Window icon;
  int hot_x, hot_y;
} g_dout;

/* the selections' window: hidden, ours as long as the display is open (the
 * clipboard works before any window and after the last one closes) */
static Window g_sel_win;

static Window sel_window(void)
{
  if (!g_sel_win && open_display()) {
    XSetWindowAttributes attr;
    memset(&attr, 0, sizeof(attr));
    attr.event_mask = PropertyChangeMask;
    g_sel_win = XCreateWindow(g_dpy, DefaultRootWindow(g_dpy), -10, -10, 1, 1, 0, 0, InputOnly,
                              CopyFromParent, CWEventMask, &attr);
  }
  return g_sel_win;
}

static int owned_by_us(Atom selection)
{
  const Window o = XGetSelectionOwner(g_dpy, selection);
  return o && (o == g_sel_win || find_window(o) != NULL);
}

static const BKClipSet *owned_set(Atom selection)
{
  if (selection == a_clipboard) {
    return g_clip;
  }
  if (selection == a_xdnd_selection && g_dout.set) {
    return g_dout.set;
  }
  return NULL;
}

typedef struct IncrOut {
  Window req;
  Atom prop, type;
  unsigned char *data;
  size_t len, off;
  struct IncrOut *next;
} IncrOut;
static IncrOut *g_incr;

static void incr_free(IncrOut *o)
{
  for (IncrOut **p = &g_incr; *p; p = &(*p)->next) {
    if (*p == o) {
      *p = o->next;
      break;
    }
  }
  free(o->data);
  free(o);
}

static void answer_request(XSelectionRequestEvent *rq)
{
  XSelectionEvent sn = {SelectionNotify, 0, True, rq->display, rq->requestor, rq->selection,
                        rq->target, None, rq->time};
  const BKClipSet *set = owned_set(rq->selection);
  const Atom prop = rq->property != None ? rq->property : rq->target; /* ICCCM: old clients */
  if (set && rq->target == a_targets) {
    const char *names[64];
    const int n = bk_clipset_offers(set, names, 63);
    long atoms[64];
    atoms[0] = (long)a_targets;
    for (int i = 0; i < n; i++) {
      atoms[i + 1] = (long)XInternAtom(g_dpy, names[i], False);
    }
    XChangeProperty(g_dpy, rq->requestor, prop, XA_ATOM, 32, PropModeReplace,
                    (const unsigned char *)atoms, n + 1);
    sn.property = prop;
  }
  else if (set) {
    char *name = XGetAtomName(g_dpy, rq->target);
    const BKClipEntry *e = name ? bk_clipset_get(set, name) : NULL;
    if (name) {
      XFree(name);
    }
    if (e && e->len > chunk_max()) { /* INCR: in chunks, each when the last is taken */
      IncrOut *o = (IncrOut *)calloc(1, sizeof(IncrOut));
      if (o && (o->data = (unsigned char *)bk_bytes_dup(e->data, e->len, &o->len))) {
        o->req = rq->requestor;
        o->prop = prop;
        o->type = rq->target;
        o->next = g_incr;
        g_incr = o;
        XSelectInput(g_dpy, rq->requestor, PropertyChangeMask);
        const long size = (long)e->len;
        XChangeProperty(g_dpy, rq->requestor, prop, a_incr, 32, PropModeReplace,
                        (const unsigned char *)&size, 1);
        sn.property = prop;
      }
      else {
        free(o);
      }
    }
    else if (e) {
      XChangeProperty(g_dpy, rq->requestor, prop, rq->target, 8, PropModeReplace, e->data,
                      (int)e->len);
      sn.property = prop;
    }
  }
  XSendEvent(g_dpy, rq->requestor, False, 0, (XEvent *)&sn);
  XFlush(g_dpy);
}

static int incr_property(XPropertyEvent *pe)
{
  if (pe->state != PropertyDelete) {
    return 0;
  }
  for (IncrOut *o = g_incr; o; o = o->next) {
    if (o->req == pe->window && o->prop == pe->atom) {
      const size_t left = o->len - o->off, cm = chunk_max();
      const size_t n = left < cm ? left : cm;
      XChangeProperty(g_dpy, o->req, o->prop, o->type, 8, PropModeReplace, o->data + o->off,
                      (int)n);
      o->off += n;
      if (n == 0) { /* the empty chunk ends it */
        XSelectInput(g_dpy, o->req, NoEventMask);
        incr_free(o);
      }
      XFlush(g_dpy);
      return 1;
    }
  }
  return 0;
}

/* -- the reading side ------------------------------------------------------ */

static int wait_event(Window w, int type, XEvent *ev, int ms)
{
  const double until = now_ms() + ms;
  while (now_ms() < until) {
    if (XCheckTypedWindowEvent(g_dpy, w, type, ev)) {
      return 1;
    }
    usleep(1000);
  }
  return 0;
}

/* Converts `selection` to `target` on our window and reads the answer (INCR
 * too): malloc'd bytes (format 32 items as longs), NULL when refused. */
static void *convert_read(Window xid, Atom selection, Atom target, Time t, size_t *r_len,
                          int *r_format)
{
  *r_len = 0;
  *r_format = 8;
  XDeleteProperty(g_dpy, xid, a_bui_sel);
  XConvertSelection(g_dpy, selection, target, a_bui_sel, xid, t);
  XFlush(g_dpy);
  XEvent ev;
  if (!wait_event(xid, SelectionNotify, &ev, 1500) || ev.xselection.property == None) {
    return NULL;
  }
  Atom type;
  int format;
  unsigned long n, after;
  unsigned char *prop = NULL;
  if (XGetWindowProperty(g_dpy, xid, a_bui_sel, 0, 0x1FFFFFFF, True, AnyPropertyType, &type,
                         &format, &n, &after, &prop) != Success)
  {
    return NULL;
  }
  if (type != a_incr) {
    const size_t unit = format == 32 ? sizeof(long) : format == 16 ? sizeof(short) : 1;
    void *out = bk_bytes_dup(prop ? prop : (unsigned char *)"", n * unit, r_len);
    *r_format = format;
    if (prop) {
      XFree(prop);
    }
    return out;
  }
  if (prop) {
    XFree(prop);
  }
  /* INCR: chunks until an empty one */
  size_t len = 0, cap = 65536;
  unsigned char *buf = (unsigned char *)malloc(cap + 1);
  while (buf) {
    if (!wait_event(xid, PropertyNotify, &ev, 2000)) {
      free(buf);
      return NULL;
    }
    if (ev.xproperty.atom != a_bui_sel || ev.xproperty.state != PropertyNewValue) {
      continue;
    }
    prop = NULL;
    if (XGetWindowProperty(g_dpy, xid, a_bui_sel, 0, 0x1FFFFFFF, True, AnyPropertyType,
                           &type, &format, &n, &after, &prop) != Success)
    {
      free(buf);
      return NULL;
    }
    if (n == 0) {
      if (prop) {
        XFree(prop);
      }
      break;
    }
    if (len + n > cap) {
      while (len + n > cap) {
        cap *= 2;
      }
      unsigned char *g = (unsigned char *)realloc(buf, cap + 1);
      if (!g) {
        free(buf);
        XFree(prop);
        return NULL;
      }
      buf = g;
    }
    memcpy(buf + len, prop, n);
    len += n;
    XFree(prop);
  }
  if (buf) {
    buf[len] = 0;
    *r_len = len;
  }
  return buf;
}

static void *convert_bytes(Window xid, Atom selection, const char *mime, Time t, size_t *r_len)
{
  int format;
  return convert_read(xid, selection, XInternAtom(g_dpy, mime, False), t, r_len, &format);
}

int BK_window_clipboard_set(BKWindow *win, const BKClipItem *items, int n)
{
  (void)win;
  BKClipSet *set = bk_clipset_new(items, n);
  const Window owner = set ? sel_window() : 0;
  if (!owner) {
    bk_clipset_free(set);
    return 0;
  }
  transfer_atoms();
  bk_clipset_free(g_clip);
  g_clip = set;
  XSetSelectionOwner(g_dpy, a_clipboard, owner, CurrentTime);
  XFlush(g_dpy);
  return 1;
}

char *BK_window_clipboard_formats(BKWindow *win)
{
  (void)win;
  const Window xid = sel_window();
  if (!xid) {
    return NULL;
  }
  transfer_atoms();
  if (owned_by_us(a_clipboard)) {
    return g_clip ? bk_clipset_mimes(g_clip) : NULL;
  }
  size_t len;
  int format;
  long *atoms = (long *)convert_read(xid, a_clipboard, a_targets, CurrentTime, &len, &format);
  if (!atoms || format != 32) {
    free(atoms);
    return NULL;
  }
  const int n = (int)(len / sizeof(long));
  char *names[256];
  int k = 0;
  for (int i = 0; i < n && k < 256; i++) {
    char *nm = atoms[i] ? XGetAtomName(g_dpy, (Atom)atoms[i]) : NULL;
    if (nm) {
      if (strcmp(nm, "TARGETS") && strcmp(nm, "MULTIPLE") && strcmp(nm, "TIMESTAMP") &&
          strcmp(nm, "SAVE_TARGETS"))
      {
        names[k++] = nm;
      }
      else {
        XFree(nm);
      }
    }
  }
  free(atoms);
  char *out = bk_mimes_join((const char *const *)names, k);
  for (int i = 0; i < k; i++) {
    XFree(names[i]);
  }
  return out;
}

void *BK_window_clipboard_read(BKWindow *win, const char *mime, size_t *r_len)
{
  *r_len = 0;
  (void)win;
  const Window xid = sel_window();
  if (!xid || !mime) {
    return NULL;
  }
  transfer_atoms();
  if (owned_by_us(a_clipboard)) { /* ours: no round trip (and no waiting on ourselves) */
    const BKClipEntry *e = bk_clipset_get(g_clip, mime);
    return e ? bk_bytes_dup(e->data, e->len, r_len) : NULL;
  }
  void *out = convert_bytes(xid, a_clipboard, mime, CurrentTime, r_len);
  if (!out && bk_mime_is_text(mime)) {
    for (int i = 0; !out && bk_text_name(i); i++) {
      if (strcmp(bk_text_name(i), mime) != 0) {
        out = convert_bytes(xid, a_clipboard, bk_text_name(i), CurrentTime, r_len);
      }
    }
  }
  return out;
}

/* -- XDND: drags out ------------------------------------------------------- */

static void xdnd_send(Window to, Atom type, long l1, long l2, long l3, long l4)
{
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = to;
  ev.xclient.message_type = type;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)g_dout.win->xid;
  ev.xclient.data.l[1] = l1;
  ev.xclient.data.l[2] = l2;
  ev.xclient.data.l[3] = l3;
  ev.xclient.data.l[4] = l4;
  XSendEvent(g_dpy, to, False, NoEventMask, &ev);
}

/* the XdndAware version of `w` (0: not aware) */
static int xdnd_version(Window w)
{
  Atom type;
  int format;
  unsigned long n, after;
  unsigned char *prop = NULL;
  int v = 0;
  if (XGetWindowProperty(g_dpy, w, a_xdnd_aware, 0, 1, False, AnyPropertyType, &type, &format, &n,
                         &after, &prop) == Success &&
      prop && n == 1)
  {
    v = (int)*(long *)prop;
  }
  if (prop) {
    XFree(prop);
  }
  return v;
}

/* The XdndAware window under root point rx, ry (our image window left out). */
static Window xdnd_find(int rx, int ry, int *r_version)
{
  const Window root = DefaultRootWindow(g_dpy);
  Window r, parent, *kids = NULL, top = 0;
  unsigned nk = 0;
  *r_version = 0;
  if (!XQueryTree(g_dpy, root, &r, &parent, &kids, &nk)) {
    return 0;
  }
  for (int i = (int)nk - 1; i >= 0 && !top; i--) { /* top-most first */
    if (kids[i] == g_dout.icon) {
      continue;
    }
    XWindowAttributes wa;
    if (XGetWindowAttributes(g_dpy, kids[i], &wa) && wa.map_state == IsViewable &&
        rx >= wa.x && ry >= wa.y && rx < wa.x + wa.width + 2 * wa.border_width &&
        ry < wa.y + wa.height + 2 * wa.border_width)
    {
      top = kids[i];
    }
  }
  if (kids) {
    XFree(kids);
  }
  /* down from the top-level (a window manager's frame) to the aware window */
  for (Window w = top; w;) {
    const int v = xdnd_version(w);
    if (v >= 3) {
      *r_version = v > 5 ? 5 : v;
      return w;
    }
    int x, y;
    Window child = 0;
    if (!XTranslateCoordinates(g_dpy, root, w, rx, ry, &x, &y, &child)) {
      break;
    }
    w = child;
  }
  return 0;
}

static void xdnd_position(void)
{
  const Atom action = (g_dout.actions & BK_DRAG_MOVE) && !(g_dout.actions & BK_DRAG_COPY) ?
                          a_xdnd_move :
                          a_xdnd_copy;
  xdnd_send(g_dout.target, a_xdnd_position, 0, ((long)g_dout.rx << 16) | (g_dout.ry & 0xFFFF),
            (long)g_dout.time, (long)action);
  g_dout.waiting = 1;
  g_dout.pending = 0;
}

static void xdnd_enter(Window to, int version)
{
  const char *names[64];
  const int n = bk_clipset_offers(g_dout.set, names, 64);
  long types[3] = {0, 0, 0};
  for (int i = 0; i < n && i < 3; i++) {
    types[i] = (long)XInternAtom(g_dpy, names[i], False);
  }
  if (n > 3) { /* the whole list as a property of our window */
    long atoms[64];
    for (int i = 0; i < n; i++) {
      atoms[i] = (long)XInternAtom(g_dpy, names[i], False);
    }
    XChangeProperty(g_dpy, g_dout.win->xid, a_xdnd_type_list, XA_ATOM, 32, PropModeReplace,
                    (const unsigned char *)atoms, n);
  }
  xdnd_send(to, a_xdnd_enter, ((long)version << 24) | (n > 3 ? 1 : 0), types[0], types[1],
            types[2]);
}

static void xdnd_end(int action)
{
  if (!g_dout.active) {
    return;
  }
  g_dout.active = 0;
  if (g_dout.icon) {
    XDestroyWindow(g_dpy, g_dout.icon);
    g_dout.icon = 0;
  }
  XUngrabPointer(g_dpy, CurrentTime);
  XFlush(g_dpy);
  BKDragDoneFn done = g_dout.done;
  void *user = g_dout.user;
  g_dout.done = NULL;
  /* the set stays until someone else owns XdndSelection (late requests) */
  if (done) {
    done(user, action);
  }
}

static void xdnd_motion(int rx, int ry, Time t)
{
  g_dout.rx = rx;
  g_dout.ry = ry;
  g_dout.time = t;
  if (g_dout.icon) {
    XMoveWindow(g_dpy, g_dout.icon, rx - g_dout.hot_x, ry - g_dout.hot_y);
  }
  int version;
  const Window to = xdnd_find(rx, ry, &version);
  if (to != g_dout.target) {
    if (g_dout.target) {
      xdnd_send(g_dout.target, a_xdnd_leave, 0, 0, 0, 0);
    }
    g_dout.target = to;
    g_dout.version = version;
    g_dout.accepted = 0;
    g_dout.waiting = 0;
    if (to) {
      xdnd_enter(to, version);
    }
  }
  if (g_dout.target) {
    if (g_dout.waiting) {
      g_dout.pending = 1; /* sent when the last one is answered */
    }
    else {
      xdnd_position();
    }
  }
  XFlush(g_dpy);
}

static void xdnd_release(Time t)
{
  XUngrabPointer(g_dpy, CurrentTime);
  if (g_dout.icon) {
    XDestroyWindow(g_dpy, g_dout.icon);
    g_dout.icon = 0;
  }
  if (g_dout.target && g_dout.accepted) {
    xdnd_send(g_dout.target, a_xdnd_drop, 0, (long)t, 0, 0);
    g_dout.dropped = 1;
    g_dout.drop_at = now_ms();
    XFlush(g_dpy);
    return; /* XdndFinished ends it */
  }
  if (g_dout.target) {
    xdnd_send(g_dout.target, a_xdnd_leave, 0, 0, 0, 0);
  }
  xdnd_end(BK_DRAG_NONE);
}

/* An image window following the pointer (override-redirect, with alpha
 * where the screen has an ARGB visual). */
static Window drag_icon_window(const uint32_t *px, int w, int h, int x, int y)
{
  const int screen = DefaultScreen(g_dpy);
  XVisualInfo vi;
  XSetWindowAttributes attr;
  memset(&attr, 0, sizeof(attr));
  attr.override_redirect = True;
  unsigned long mask = CWOverrideRedirect | CWBackPixel | CWBorderPixel;
  int depth = DefaultDepth(g_dpy, screen);
  Visual *visual = DefaultVisual(g_dpy, screen);
  if (XMatchVisualInfo(g_dpy, screen, 32, TrueColor, &vi)) {
    attr.colormap = XCreateColormap(g_dpy, RootWindow(g_dpy, screen), vi.visual, AllocNone);
    mask |= CWColormap;
    depth = 32;
    visual = vi.visual;
  }
  const Window win = XCreateWindow(g_dpy, RootWindow(g_dpy, screen), x, y, (unsigned)w,
                                   (unsigned)h, 0, depth, InputOutput, visual, mask, &attr);
  if (!win) {
    return 0;
  }
  uint32_t *copy = (uint32_t *)malloc((size_t)w * h * 4);
  if (copy) {
    memcpy(copy, px, (size_t)w * h * 4);
    if (depth != 32) { /* no alpha: over a neutral grey */
      for (int i = 0; i < w * h; i++) {
        const uint32_t c = copy[i], ia = 255 - (c >> 24);
        uint32_t o = 0;
        for (int k = 0; k < 24; k += 8) {
          o |= ((((c >> k) & 255) + 0xEE * ia / 255) & 255) << k;
        }
        copy[i] = o;
      }
    }
    XImage *img = XCreateImage(g_dpy, visual, (unsigned)depth, ZPixmap, 0, (char *)copy,
                               (unsigned)w, (unsigned)h, 32, w * 4);
    XMapWindow(g_dpy, win);
    XRaiseWindow(g_dpy, win);
    if (img) {
      GC gc = XCreateGC(g_dpy, win, 0, NULL);
      XPutImage(g_dpy, win, gc, img, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
      XFreeGC(g_dpy, gc);
      img->data = NULL;
      XFree(img);
    }
    free(copy);
  }
  return win;
}

int BK_window_drag_start(BKWindow *win, const BKClipItem *items, int n, const uint32_t *icon,
                         int icon_w, int icon_h, int hot_x, int hot_y, int actions,
                         BKDragDoneFn done, void *user)
{
  if (!win || !win->xid || g_dout.active) {
    return 0;
  }
  transfer_atoms();
  Window root, child;
  int rx, ry, wx, wy;
  unsigned mask = 0;
  XQueryPointer(g_dpy, win->xid, &root, &child, &rx, &ry, &wx, &wy, &mask);
  if (!(mask & (Button1Mask | Button2Mask | Button3Mask))) {
    return 0; /* no press to drag from */
  }
  BKClipSet *set = bk_clipset_new(items, n);
  if (!set) {
    return 0;
  }
  if (XGrabPointer(g_dpy, win->xid, False, ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                   GrabModeAsync, None, None, CurrentTime) != GrabSuccess)
  {
    bk_clipset_free(set);
    return 0;
  }
  bk_clipset_free(g_dout.set);
  memset(&g_dout, 0, sizeof(g_dout));
  g_dout.active = 1;
  g_dout.win = win;
  g_dout.set = set;
  g_dout.done = done;
  g_dout.user = user;
  g_dout.actions = actions ? actions : BK_DRAG_COPY;
  g_dout.hot_x = hot_x;
  g_dout.hot_y = hot_y;
  XSetSelectionOwner(g_dpy, a_xdnd_selection, win->xid, CurrentTime);
  if (icon && icon_w > 0 && icon_h > 0) {
    g_dout.icon = drag_icon_window(icon, icon_w, icon_h, rx - hot_x, ry - hot_y);
  }
  BK_pointer_cancel(win->ctx);
  xdnd_motion(rx, ry, CurrentTime);
  return 1;
}

/* -- XDND: drops in -------------------------------------------------------- */

static struct {
  Window source;
  BKWindow *win;
  int version, uris, entered;
  int x, y;
} g_din;

static int xdnd_has_uris(const XClientMessageEvent *cm)
{
  if (cm->data.l[1] & 1) { /* more than three types: the list */
    Atom type;
    int format;
    unsigned long n, after;
    unsigned char *prop = NULL;
    int has = 0;
    if (XGetWindowProperty(g_dpy, (Window)cm->data.l[0], a_xdnd_type_list, 0, 256, False,
                           XA_ATOM, &type, &format, &n, &after, &prop) == Success &&
        prop)
    {
      for (unsigned long i = 0; i < n; i++) {
        has |= ((long *)prop)[i] == (long)a_uri_list;
      }
    }
    if (prop) {
      XFree(prop);
    }
    return has;
  }
  return cm->data.l[2] == (long)a_uri_list || cm->data.l[3] == (long)a_uri_list ||
         cm->data.l[4] == (long)a_uri_list;
}

static void xdnd_status(int accept)
{
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = g_din.source;
  ev.xclient.message_type = a_xdnd_status;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)g_din.win->xid;
  ev.xclient.data.l[1] = accept ? 1 : 0;
  ev.xclient.data.l[4] = accept ? (long)a_xdnd_copy : 0;
  XSendEvent(g_dpy, g_din.source, False, NoEventMask, &ev);
  XFlush(g_dpy);
}

static int xdnd_client_message(XClientMessageEvent *cm)
{
  const Atom t = cm->message_type;
  /* the target side: messages to one of our windows */
  if (t == a_xdnd_enter || t == a_xdnd_position || t == a_xdnd_leave || t == a_xdnd_drop) {
    BKWindow *win = find_window(cm->window);
    if (!win) {
      return 1;
    }
    if (t == a_xdnd_enter) {
      g_din.source = (Window)cm->data.l[0];
      g_din.win = win;
      g_din.version = (int)((unsigned long)cm->data.l[1] >> 24);
      g_din.uris = xdnd_has_uris(cm);
      g_din.entered = 0;
      return 1;
    }
    if (g_din.win != win || g_din.source != (Window)cm->data.l[0]) {
      return 1;
    }
    if (t == a_xdnd_position) {
      const int rx = (int)((unsigned long)cm->data.l[2] >> 16), ry = (int)(cm->data.l[2] & 0xFFFF);
      Window child;
      XTranslateCoordinates(g_dpy, DefaultRootWindow(g_dpy), win->xid, rx, ry, &g_din.x,
                            &g_din.y, &child);
      int accept = 0;
      if (g_din.uris) {
        accept = BK_drop(win->ctx, g_din.entered ? BK_DROP_MOTION : BK_DROP_ENTER, g_din.x,
                         g_din.y, NULL);
        g_din.entered = 1;
      }
      xdnd_status(accept);
      return 1;
    }
    if (t == a_xdnd_leave) {
      if (g_din.entered) {
        BK_drop(win->ctx, BK_DROP_LEAVE, g_din.x, g_din.y, NULL);
      }
      memset(&g_din, 0, sizeof(g_din));
      return 1;
    }
    /* XdndDrop: the files, then XdndFinished */
    int ok = 0;
    if (g_din.uris) {
      size_t len = 0;
      char *uris = NULL;
      if (owned_by_us(a_xdnd_selection) && g_dout.set) { /* from ourselves */
        const BKClipEntry *e = bk_clipset_get(g_dout.set, "text/uri-list");
        uris = e ? (char *)bk_bytes_dup(e->data, e->len, &len) : NULL;
      }
      else {
        int format;
        uris = (char *)convert_read(win->xid, a_xdnd_selection, a_uri_list, (Time)cm->data.l[2],
                                    &len, &format);
      }
      if (uris) {
        ok = BK_drop(win->ctx, BK_DROP_DROP, g_din.x, g_din.y, uris);
        free(uris);
      }
      if (g_din.entered) {
        BK_drop(win->ctx, BK_DROP_LEAVE, g_din.x, g_din.y, NULL);
      }
    }
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = g_din.source;
    ev.xclient.message_type = a_xdnd_finished;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)win->xid;
    ev.xclient.data.l[1] = ok ? 1 : 0;
    ev.xclient.data.l[2] = ok ? (long)a_xdnd_copy : 0;
    XSendEvent(g_dpy, g_din.source, False, NoEventMask, &ev);
    XFlush(g_dpy);
    win->render_pending = 1;
    memset(&g_din, 0, sizeof(g_din));
    return 1;
  }
  /* the source side: answers to our drag */
  if (t == a_xdnd_status && g_dout.active && (Window)cm->data.l[0] == g_dout.target) {
    g_dout.accepted = (cm->data.l[1] & 1) != 0;
    g_dout.action = (Atom)cm->data.l[4];
    g_dout.waiting = 0;
    if (g_dout.pending && !g_dout.dropped) {
      xdnd_position();
      XFlush(g_dpy);
    }
    return 1;
  }
  if (t == a_xdnd_finished && g_dout.active && (Window)cm->data.l[0] == g_dout.target) {
    const int ok = g_dout.version < 5 || (cm->data.l[1] & 1);
    const Atom a = g_dout.version >= 5 ? (Atom)cm->data.l[2] : g_dout.action;
    xdnd_end(!ok ? BK_DRAG_NONE : a == a_xdnd_move ? BK_DRAG_MOVE : BK_DRAG_COPY);
    return 1;
  }
  return 0;
}

/* Events of the transfers; 1 when handled. */
static int x11_transfer_event(XEvent *ev)
{
  transfer_atoms();
  switch (ev->type) {
    case SelectionRequest:
      answer_request(&ev->xselectionrequest);
      return 1;
    case SelectionClear:
      if (ev->xselectionclear.selection == a_xdnd_selection && !g_dout.active) {
        bk_clipset_free(g_dout.set);
        g_dout.set = NULL;
      }
      return 1;
    case PropertyNotify:
      return incr_property(&ev->xproperty);
    case ClientMessage:
      return xdnd_client_message(&ev->xclient);
    case MotionNotify:
      if (g_dout.active && !g_dout.dropped) {
        xdnd_motion(ev->xmotion.x_root, ev->xmotion.y_root, ev->xmotion.time);
        return 1;
      }
      return 0;
    case ButtonPress:
      return g_dout.active && !g_dout.dropped;
    case ButtonRelease:
      if (g_dout.active && !g_dout.dropped) {
        xdnd_release(ev->xbutton.time);
        return 1;
      }
      return 0;
    case KeyPress:
      if (g_dout.active && !g_dout.dropped) {
        char text[8];
        KeySym sym = 0;
        XLookupString(&ev->xkey, text, sizeof(text), &sym, NULL);
        if (sym == XK_Escape) {
          if (g_dout.target) {
            xdnd_send(g_dout.target, a_xdnd_leave, 0, 0, 0, 0);
          }
          xdnd_end(BK_DRAG_NONE);
        }
        return 1;
      }
      return 0;
    default:
      /* a target that never finishes doesn't hold the drag forever */
      if (g_dout.active && g_dout.dropped && now_ms() - g_dout.drop_at > 5000) {
        xdnd_end(BK_DRAG_NONE);
      }
      return 0;
  }
}


/* =========================================================================
 * BUI-Awra: window identity: _NET_WM_ICON, and activation with a startup
 * notification ID (_NET_STARTUP_ID, the "remove" message,
 * _NET_ACTIVE_WINDOW with its timestamp).
 * ========================================================================= */

void BK_window_set_icon(BKWindow *win, const char *name, const uint32_t *const *pixels,
                        const int *sizes, int n)
{
  (void)name; /* X11 has no icon names: the images */
  if (!win || !win->xid) {
    return;
  }
  const Atom a_icon = XInternAtom(g_dpy, "_NET_WM_ICON", False);
  size_t total = 0;
  for (int i = 0; i < n; i++) {
    total += sizes[i] > 0 && pixels[i] ? 2 + (size_t)sizes[i] * sizes[i] : 0;
  }
  if (!total) {
    XDeleteProperty(g_dpy, win->xid, a_icon);
    XFlush(g_dpy);
    return;
  }
  /* CARDINALs (longs in Xlib): width, height, then straight ARGB rows */
  long *data = (long *)malloc(sizeof(long) * total);
  if (!data) {
    return;
  }
  size_t k = 0;
  for (int i = 0; i < n; i++) {
    const int sz = sizes[i];
    if (sz <= 0 || !pixels[i]) {
      continue;
    }
    data[k++] = sz;
    data[k++] = sz;
    for (int p = 0; p < sz * sz; p++) {
      const uint32_t c = pixels[i][p], a = c >> 24;
      uint32_t o = c;
      if (a && a < 255) {
        o = (a << 24) | (((c >> 16 & 255) * 255 / a) << 16) | (((c >> 8 & 255) * 255 / a) << 8) |
            ((c & 255) * 255 / a);
      }
      data[k++] = (long)o;
    }
  }
  XChangeProperty(g_dpy, win->xid, a_icon, XA_CARDINAL, 32, PropModeReplace,
                  (const unsigned char *)data, (int)k);
  free(data);
  XFlush(g_dpy);
}

/* the startup-notification protocol's message, broadcast to the root
 * window in 20-byte pieces */
static void startup_message(Window from, const char *msg)
{
  const Atom begin = XInternAtom(g_dpy, "_NET_STARTUP_INFO_BEGIN", False);
  const Atom more = XInternAtom(g_dpy, "_NET_STARTUP_INFO", False);
  const size_t len = strlen(msg) + 1; /* the NUL ends it */
  for (size_t off = 0; off < len; off += 20) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = from;
    ev.xclient.message_type = off == 0 ? begin : more;
    ev.xclient.format = 8;
    const size_t n = len - off < 20 ? len - off : 20;
    memcpy(ev.xclient.data.b, msg + off, n);
    XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False, PropertyChangeMask, &ev);
  }
}

/* BUI-Awra: WM_TRANSIENT_FOR another application's window (its ID in
 * hexadecimal, as the desktop portal passes it after "x11:"). */
void BK_window_set_transient_for_exported(BKWindow *win, const char *handle)
{
  if (!win || !win->xid) {
    return;
  }
  /* hexadecimal, with or without 0x, as the portal and GTK write it */
  const unsigned long parent = handle && handle[0] ? strtoul(handle, NULL, 16) : 0;
  if (!parent) {
    XDeleteProperty(g_dpy, win->xid, 68 /* XA_WM_TRANSIENT_FOR */);
  }
  else {
    const long value = (long)parent;
    XChangeProperty(g_dpy, win->xid, 68 /* XA_WM_TRANSIENT_FOR */, 33 /* XA_WINDOW */, 32,
                    PropModeReplace, (const unsigned char *)&value, 1);
  }
  XFlush(g_dpy);
}

/* BUI-Awra: WM_TRANSIENT_FOR one of our windows, typed a dialog
 * (_NET_WM_WINDOW_TYPE_DIALOG), modal by _NET_WM_STATE_MODAL (set before
 * it maps, as GTK does; a mapped window asks the WM) */
void BK_window_set_parent(BKWindow *win, BKWindow *parent, int modal)
{
  if (!win || !win->xid) {
    return;
  }
  const Atom type = XInternAtom(g_dpy, "_NET_WM_WINDOW_TYPE", False);
  if (!parent || !parent->xid) {
    XDeleteProperty(g_dpy, win->xid, 68 /* XA_WM_TRANSIENT_FOR */);
    XDeleteProperty(g_dpy, win->xid, type);
  }
  else {
    const long value = (long)parent->xid;
    XChangeProperty(g_dpy, win->xid, 68 /* XA_WM_TRANSIENT_FOR */, 33 /* XA_WINDOW */, 32,
                    PropModeReplace, (const unsigned char *)&value, 1);
    const long dialog = (long)XInternAtom(g_dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(g_dpy, win->xid, type, 4 /* XA_ATOM */, 32, PropModeReplace,
                    (const unsigned char *)&dialog, 1);
  }
  const Atom state = XInternAtom(g_dpy, "_NET_WM_STATE", False);
  const Atom modal_atom = XInternAtom(g_dpy, "_NET_WM_STATE_MODAL", False);
  if (win->mapped) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = win->xid;
    ev.xclient.message_type = state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = modal && parent ? 1 : 0; /* _NET_WM_STATE_ADD / REMOVE */
    ev.xclient.data.l[1] = (long)modal_atom;
    ev.xclient.data.l[3] = 1;
    XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  }
  else if (modal && parent) {
    const long value = (long)modal_atom;
    XChangeProperty(g_dpy, win->xid, state, 4 /* XA_ATOM */, 32, PropModeAppend,
                    (const unsigned char *)&value, 1);
  }
  XFlush(g_dpy);
}

void BK_window_activate(BKWindow *win, const char *startup_id)
{
  if (!win || !win->xid) {
    return;
  }
  static int read_env = 0;
  static char *env_id = NULL;
  if (!read_env) { /* once: the window shown first uses it */
    read_env = 1;
    const char *e = getenv("DESKTOP_STARTUP_ID");
    if (e && e[0]) {
      env_id = strdup(e);
    }
    unsetenv("DESKTOP_STARTUP_ID");
    unsetenv("XDG_ACTIVATION_TOKEN");
  }
  char *own = NULL;
  const char *id = startup_id && startup_id[0] ? startup_id : (own = env_id);
  env_id = own ? NULL : env_id;
  long when = 0; /* CurrentTime */
  if (id) {
    const Atom a_id = XInternAtom(g_dpy, "_NET_STARTUP_ID", False);
    XChangeProperty(g_dpy, win->xid, a_id, a_utf8, 8, PropModeReplace, (const unsigned char *)id,
                    (int)strlen(id));
    const char *t = strstr(id, "_TIME");
    if (t) {
      when = strtol(t + 5, NULL, 10);
    }
    /* "remove: ID=<id>" (quoted when it has spaces, quotes or backslashes) */
    char msg[600];
    size_t n = (size_t)snprintf(msg, sizeof(msg), "remove: ID=");
    for (const char *c = id; *c && n + 3 < sizeof(msg); c++) {
      if (*c == ' ' || *c == '"' || *c == '\\') {
        msg[n++] = '\\';
      }
      msg[n++] = *c;
    }
    msg[n] = 0;
    startup_message(win->xid, msg);
  }
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = win->xid;
  ev.xclient.message_type = XInternAtom(g_dpy, "_NET_ACTIVE_WINDOW", False);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 1; /* from an application */
  ev.xclient.data.l[1] = when;
  XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XFlush(g_dpy);
  free(own);
}
#  include "kit_popup_x11.inc.c"
#endif /* BUI_WINDOW_X11 */
