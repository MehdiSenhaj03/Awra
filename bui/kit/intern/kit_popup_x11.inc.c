/* BUI-Awra: popups on X11 (included by kit_window_x11.c).
 *
 * As GDK's X11 backend: a popup is an override-redirect window with the
 * window's ARGB visual, placed in root coordinates by GDK's layout helper
 * against the work area of the monitor its anchor is on
 * (gdk_x11_surface_layout_popup), moved with its window, and an autohide
 * popup grabs the pointer and the keyboard with owner events
 * (gdk_seat_grab): the application's windows get their events as usual,
 * a click anywhere else comes to the popup, outside it, and closes it.
 * Each popup draws into its own raster (BUI_raster_set_origin: the
 * window's coordinates), put with XPutImage where it changed.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

typedef struct XPop {
  Window xid;
  BuiRaster *raster;
  XImage *image;
  int ox, oy, w, h; /* the surface, window px (its shadow included) */
  BKRect box;       /* the popup's box, window px */
  int kind, grab, blur, radius;
  int mapped, drawn;
} XPop;

static Window g_pop_grab; /* the popup holding the grab (0: none) */

/* libXrandr, loaded with the first popup (monitors) */
typedef struct XRRMonitorInfoBK {
  Atom name;
  Bool primary, automatic;
  int noutput, x, y, width, height, mwidth, mheight;
  void *outputs;
} XRRMonitorInfoBK;
static XRRMonitorInfoBK *(*p_XRRGetMonitors)(Display *, Window, Bool, int *);
static void (*p_XRRFreeMonitors)(XRRMonitorInfoBK *);

static int x11_pop_place(void *data, int slot, const BKPopupLayout *l, int w, int h, int grab,
                         BKRect *r_rect);
static void x11_pop_close(void *data, int slot);

/* An integer XSETTINGS value (the settings daemon's), -1 when none. */
static int xsettings_int(const char *want)
{
  char sel[32];
  snprintf(sel, sizeof(sel), "_XSETTINGS_S%d", DefaultScreen(g_dpy));
  const Window owner = XGetSelectionOwner(g_dpy, XInternAtom(g_dpy, sel, False));
  if (!owner) {
    return -1;
  }
  const Atom prop = XInternAtom(g_dpy, "_XSETTINGS_SETTINGS", False);
  Atom type;
  int format;
  unsigned long n = 0, after;
  unsigned char *d = NULL;
  if (XGetWindowProperty(g_dpy, owner, prop, 0, 1 << 16, False, prop, &type, &format, &n, &after,
                         &d) != Success ||
      !d || format != 8 || n < 12)
  {
    if (d) {
      XFree(d);
    }
    return -1;
  }
  /* byte order, serial, count; then type, name, serial, value per setting */
  const int msb = d[0] == 1;
#  define XS_U16(p) (msb ? (unsigned)((p)[0] << 8 | (p)[1]) : (unsigned)((p)[1] << 8 | (p)[0]))
#  define XS_U32(p)                                                                         (msb ? ((uint32_t)(p)[0] << 24 | (uint32_t)(p)[1] << 16 | (uint32_t)(p)[2] << 8 | (p)[3]) :            ((uint32_t)(p)[3] << 24 | (uint32_t)(p)[2] << 16 | (uint32_t)(p)[1] << 8 | (p)[0]))
  const uint32_t count = XS_U32(d + 8);
  size_t at = 12;
  int result = -1;
  for (uint32_t i = 0; i < count && at + 8 <= n; i++) {
    const int kind = d[at];
    const unsigned len = XS_U16(d + at + 2);
    const size_t name_at = at + 4, name_end = name_at + ((len + 3) & ~3u);
    if (name_end + 4 > n) {
      break;
    }
    at = name_end + 4; /* past its serial */
    const bool match = len == strlen(want) && memcmp(d + name_at, want, len) == 0;
    if (kind == 0) { /* integer */
      if (at + 4 > n) {
        break;
      }
      if (match) {
        result = (int)XS_U32(d + at);
      }
      at += 4;
    }
    else if (kind == 1) { /* string */
      if (at + 4 > n) {
        break;
      }
      at += 4 + ((XS_U32(d + at) + 3) & ~3u);
    }
    else if (kind == 2) { /* colour */
      at += 8;
    }
    else {
      break;
    }
  }
#  undef XS_U16
#  undef XS_U32
  XFree(d);
  return result;
}

/* GDK's gtk-cursor-theme-size on X11: the settings daemon's
 * Gtk/CursorThemeSize, in device px (GDK divides by the window scale);
 * without one GTK's tooltips take 16 (0 here: the kit's default), even
 * where Xcursor draws the pointer at another size. */
static int x11_cursor_size(const BKWindow *win)
{
  const int size = xsettings_int("Gtk/CursorThemeSize");
  return size > 0 ? BK_MAX(1, (int)(size / BK_MAX(1.0f, win->scale) + 0.5f)) : 0;
}

static void x11_popups_init(BKWindow *win)
{
  win->pophost.data = win;
  win->pophost.flags = 0;
  win->pophost.corner_radius = 0;
  win->pophost.place = x11_pop_place;
  win->pophost.close = x11_pop_close;
  win->pophost.cursor_size = x11_cursor_size(win);
  if (win->depth == 32) { /* override-redirect windows with alpha: as its own */
    bk_popup_host_set(win->ctx, &win->pophost);
  }
}

static void root_origin(BKWindow *win, int *r_x, int *r_y)
{
  Window child;
  *r_x = *r_y = 0;
  XTranslateCoordinates(g_dpy, win->xid, DefaultRootWindow(g_dpy), 0, 0, r_x, r_y, &child);
}

static long *root_longs(const char *name, int *r_n)
{
  Atom type;
  int format;
  unsigned long n = 0, after;
  unsigned char *data = NULL;
  *r_n = 0;
  if (XGetWindowProperty(g_dpy, DefaultRootWindow(g_dpy), XInternAtom(g_dpy, name, False), 0,
                         1024, False, XA_CARDINAL, &type, &format, &n, &after, &data) != Success ||
      !data || format != 32)
  {
    if (data) {
      XFree(data);
    }
    return NULL;
  }
  *r_n = (int)n;
  return (long *)data;
}

static int rect_overlap_area(const BKRect *a, const BKRect *b)
{
  const int w = BK_MIN(a->xmax, b->xmax) - BK_MAX(a->xmin, b->xmin);
  const int h = BK_MIN(a->ymax, b->ymax) - BK_MAX(a->ymin, b->ymin);
  return w > 0 && h > 0 ? w * h : 0;
}

/* The work area of the monitor `r` (root coordinates) is most on
 * (gdk_x11_monitor_get_workarea): the monitor's geometry, less the panels
 * the window manager reserves (_GTK_WORKAREAS_Dn per monitor, else
 * _NET_WORKAREA). */
static BKRect x11_workarea(const BKRect *r)
{
  const int screen = DefaultScreen(g_dpy);
  BKRect mon = bk_rect(0, 0, DisplayWidth(g_dpy, screen), DisplayHeight(g_dpy, screen));
  static int tried = 0;
  if (!tried) {
    tried = 1;
    void *lib = dlopen("libXrandr.so.2", RTLD_NOW | RTLD_LOCAL);
    if (lib) {
      p_XRRGetMonitors = (XRRMonitorInfoBK * (*)(Display *, Window, Bool, int *))dlsym(
          lib, "XRRGetMonitors");
      p_XRRFreeMonitors = (void (*)(XRRMonitorInfoBK *))dlsym(lib, "XRRFreeMonitors");
    }
  }
  BKRect probe = *r;
  probe.xmax = BK_MAX(probe.xmax, probe.xmin + 1);
  probe.ymax = BK_MAX(probe.ymax, probe.ymin + 1);
  bool primary = true;
  if (p_XRRGetMonitors && p_XRRFreeMonitors) {
    int n = 0;
    XRRMonitorInfoBK *m = p_XRRGetMonitors(g_dpy, DefaultRootWindow(g_dpy), True, &n);
    int best = -1, best_area = 0;
    for (int i = 0; m && i < n; i++) {
      const BKRect g = bk_rect(m[i].x, m[i].y, m[i].width, m[i].height);
      const int a = rect_overlap_area(&g, &probe);
      if (a > best_area || (best < 0 && m[i].primary)) {
        best = i;
        best_area = a;
      }
    }
    if (best >= 0) {
      mon = bk_rect(m[best].x, m[best].y, m[best].width, m[best].height);
      primary = m[best].primary || n == 1;
    }
    if (m) {
      p_XRRFreeMonitors(m);
    }
  }
  int nd = 0;
  long *desk = root_longs("_NET_CURRENT_DESKTOP", &nd);
  const long d = desk && nd > 0 ? desk[0] : 0;
  if (desk) {
    XFree(desk);
  }
  char name[64];
  snprintf(name, sizeof(name), "_GTK_WORKAREAS_D%ld", d);
  int n = 0;
  long *wa = root_longs(name, &n);
  BKRect area = mon;
  bool found = false;
  for (int i = 0; wa && i + 3 < n; i += 4) {
    const BKRect g = bk_rect((int)wa[i], (int)wa[i + 1], (int)wa[i + 2], (int)wa[i + 3]);
    if (rect_overlap_area(&g, &mon) > 0) {
      area = g;
      found = true;
      break;
    }
  }
  if (wa) {
    XFree(wa);
  }
  if (!found && primary) {
    wa = root_longs("_NET_WORKAREA", &n);
    if (wa && 4 * d + 3 < n) {
      const BKRect g = bk_rect((int)wa[4 * d], (int)wa[4 * d + 1], (int)wa[4 * d + 2],
                               (int)wa[4 * d + 3]);
      if (rect_overlap_area(&g, &mon) > 0) {
        area.xmin = BK_MAX(mon.xmin, g.xmin);
        area.ymin = BK_MAX(mon.ymin, g.ymin);
        area.xmax = BK_MIN(mon.xmax, g.xmax);
        area.ymax = BK_MIN(mon.ymax, g.ymax);
      }
    }
    if (wa) {
      XFree(wa);
    }
  }
  return area;
}

/* The compositor's blur behind the box (KWin's property, as the window's). */
static void pop_blur(XPop *p)
{
  const Atom atom = XInternAtom(g_dpy, "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
  if (!p->blur) {
    XDeleteProperty(g_dpy, p->xid, atom);
    return;
  }
  const int x = p->box.xmin - p->ox, y = p->box.ymin - p->oy;
  const int w = bk_rw(&p->box), h = bk_rh(&p->box);
  const int r = BK_MAX(0, BK_MIN(p->radius, BK_MIN(w, h) / 2));
  long *rects = (long *)malloc(sizeof(long) * 4 * (size_t)(2 * r + 1));
  if (!rects) {
    return;
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
  XChangeProperty(g_dpy, p->xid, atom, XA_CARDINAL, 32, PropModeReplace,
                  (const unsigned char *)rects, n);
  free(rects);
}

/* The grab goes to the newest autohide popup that shows; none: released. */
static void pop_regrab(BKWindow *win)
{
  Window want = 0;
  for (int i = BK_POPUP_SLOTS - 1; i >= 0 && win->pops && !want; i--) {
    if (win->pops[i].xid && win->pops[i].mapped && win->pops[i].grab) {
      want = win->pops[i].xid;
    }
  }
  if (want == g_pop_grab) {
    return;
  }
  if (want) {
    const unsigned mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                          EnterWindowMask | LeaveWindowMask;
    XGrabPointer(g_dpy, want, True, mask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XGrabKeyboard(g_dpy, want, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  }
  else {
    XUngrabPointer(g_dpy, CurrentTime);
    XUngrabKeyboard(g_dpy, CurrentTime);
  }
  g_pop_grab = want;
  XFlush(g_dpy);
}

static void x11_pop_cursor(BKWindow *win, int slot, Cursor c)
{
  if (win->pops[slot].xid && c) {
    XDefineCursor(g_dpy, win->pops[slot].xid, c);
  }
}

static void pop_destroy(XPop *p)
{
  if (p->xid) {
    if (g_pop_grab == p->xid) {
      g_pop_grab = 0; /* the server drops a grab with its window */
    }
    XDestroyWindow(g_dpy, p->xid);
  }
  if (p->image) {
    p->image->data = NULL; /* the raster owns the pixels */
    XDestroyImage(p->image);
  }
  BUI_raster_free(p->raster);
  memset(p, 0, sizeof(*p));
}

static int x11_pop_place(void *data, int slot, const BKPopupLayout *l, int w, int h, int grab,
                         BKRect *r_rect)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->xid) {
    return 0;
  }
  if (!win->pops) {
    win->pops = (XPop *)calloc(BK_POPUP_SLOTS, sizeof(XPop));
    if (!win->pops) {
      return 0;
    }
  }
  XPop *p = &win->pops[slot];
  root_origin(win, &win->root_x, &win->root_y);
  BKRect anchor = l->anchor;
  anchor.xmin += win->root_x;
  anchor.xmax += win->root_x;
  anchor.ymin += win->root_y;
  anchor.ymax += win->root_y;
  BKRect bounds = x11_workarea(&anchor);
  bounds.xmin -= win->root_x;
  bounds.xmax -= win->root_x;
  bounds.ymin -= win->root_y;
  bounds.ymax -= win->root_y;
  BKRect box;
  bk_popup_layout_resolve(l, w, h, &bounds, &box, NULL);
  const int ox = box.xmin - l->shadow[0], oy = box.ymin - l->shadow[2];
  const int sw = bk_rw(&box) + l->shadow[0] + l->shadow[1];
  const int sh = bk_rh(&box) + l->shadow[2] + l->shadow[3];
  const bool moved = ox != p->ox || oy != p->oy, resized = sw != p->w || sh != p->h;
  const bool blur_changed = l->blur != p->blur || l->radius != p->radius ||
                            memcmp(&box, &p->box, sizeof(box)) != 0;
  p->box = box;
  p->ox = ox;
  p->oy = oy;
  p->w = sw;
  p->h = sh;
  p->grab = grab;
  p->kind = l->kind;
  p->blur = l->blur;
  p->radius = l->radius;
  if (!p->xid) {
    const int screen = DefaultScreen(g_dpy);
    XSetWindowAttributes attr;
    memset(&attr, 0, sizeof(attr));
    attr.override_redirect = True;
    attr.colormap = win->cmap;
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.save_under = True;
    attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                      EnterWindowMask | LeaveWindowMask | KeyPressMask | KeyReleaseMask;
    p->xid = XCreateWindow(g_dpy, RootWindow(g_dpy, screen), win->root_x + ox, win->root_y + oy,
                           (unsigned)BK_MAX(1, sw), (unsigned)BK_MAX(1, sh), 0, win->depth,
                           InputOutput, win->visual,
                           CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel |
                               CWSaveUnder | CWEventMask,
                           &attr);
    if (!p->xid) {
      return 0;
    }
    const Atom type = XInternAtom(g_dpy, "_NET_WM_WINDOW_TYPE", False);
    const Atom kind = XInternAtom(g_dpy,
                                  l->kind == BK_POPUP_KIND_TOOLTIP ? "_NET_WM_WINDOW_TYPE_TOOLTIP" :
                                  l->kind == BK_POPUP_KIND_MENU   ? "_NET_WM_WINDOW_TYPE_POPUP_MENU" :
                                                                    "_NET_WM_WINDOW_TYPE_DIALOG",
                                  False);
    XChangeProperty(g_dpy, p->xid, type, XA_ATOM, 32, PropModeReplace,
                    (const unsigned char *)&kind, 1);
    const Window owner = win->xid;
    XChangeProperty(g_dpy, p->xid, XInternAtom(g_dpy, "WM_TRANSIENT_FOR", False), XA_WINDOW, 32,
                    PropModeReplace, (const unsigned char *)&owner, 1);
    p->raster = BUI_raster_create(1, 1);
    BUI_raster_set_diff(p->raster, 0); /* the kit's regions are the damage */
    pop_blur(p);
    if (win->cursors[BK_CLAMP(win->cursor, 0, 15)]) {
      XDefineCursor(g_dpy, p->xid, win->cursors[BK_CLAMP(win->cursor, 0, 15)]);
    }
  }
  else {
    if (moved || resized) {
      XMoveResizeWindow(g_dpy, p->xid, win->root_x + ox, win->root_y + oy,
                        (unsigned)BK_MAX(1, sw), (unsigned)BK_MAX(1, sh));
    }
    if (resized) {
      p->drawn = 0;
    }
    if (blur_changed) {
      pop_blur(p);
    }
    if (p->mapped) {
      pop_regrab(win);
    }
  }
  win->render_pending = 1;
  *r_rect = box;
  return 1;
}

static void x11_pop_close(void *data, int slot)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->pops || !win->pops[slot].xid) {
    return;
  }
  pop_destroy(&win->pops[slot]);
  pop_regrab(win);
  XFlush(g_dpy);
}

static void x11_popups_free(BKWindow *win)
{
  if (!win->pops) {
    return;
  }
  for (int i = 0; i < BK_POPUP_SLOTS; i++) {
    pop_destroy(&win->pops[i]);
  }
  pop_regrab(win);
  if (win->ctx && win->ctx->popup_host == &win->pophost) {
    bk_popup_host_set(win->ctx, NULL);
  }
}

/* GDK's gdk_x11_surface_update_popups: the window moved, its popups keep
 * their place on it. */
static void x11_popups_follow(BKWindow *win)
{
  if (!win->pops) {
    return;
  }
  bool any = false;
  for (int i = 0; i < BK_POPUP_SLOTS && !any; i++) {
    any = win->pops[i].xid != 0;
  }
  if (!any) {
    return;
  }
  int rx, ry;
  root_origin(win, &rx, &ry);
  if (rx == win->root_x && ry == win->root_y) {
    return;
  }
  win->root_x = rx;
  win->root_y = ry;
  for (int i = 0; i < BK_POPUP_SLOTS; i++) {
    XPop *p = &win->pops[i];
    if (p->xid) {
      XMoveWindow(g_dpy, p->xid, rx + p->ox, ry + p->oy);
    }
  }
  XFlush(g_dpy);
}

static void pop_put(BKWindow *win, XPop *p, int all)
{
  const int w = BUI_raster_width(p->raster), h = BUI_raster_height(p->raster);
  if (!p->image || p->image->width != w || p->image->height != h) {
    if (p->image) {
      p->image->data = NULL;
      XDestroyImage(p->image);
    }
    p->image = XCreateImage(g_dpy, win->visual, (unsigned)win->depth, ZPixmap, 0, NULL,
                            (unsigned)w, (unsigned)h, 32, w * 4);
    all = 1;
  }
  if (!p->image) {
    return;
  }
  p->image->data = (char *)BUI_raster_pixels(p->raster);
  if (all) {
    XPutImage(g_dpy, p->xid, win->gc, p->image, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
    return;
  }
  const int(*rects)[4];
  const int n = BUI_raster_damage(p->raster, &rects);
  for (int i = 0; i < n; i++) {
    XPutImage(g_dpy, p->xid, win->gc, p->image, rects[i][0], rects[i][1], rects[i][0],
              rects[i][1], (unsigned)rects[i][2], (unsigned)rects[i][3]);
  }
}

static void x11_popups_render(BKWindow *win)
{
  if (!win->pops || !win->xid) {
    return;
  }
  bool flush = false;
  for (int i = 0; i < BK_POPUP_SLOTS; i++) {
    XPop *p = &win->pops[i];
    if (!p->xid) {
      continue;
    }
    const int full = !p->drawn;
    if (!full && !bk_popup_needs_render(win->ctx, i)) {
      continue;
    }
    const int regions = bk_render_popup(win->ctx, i, p->raster, p->w, p->h, p->ox, p->oy, full);
    if (!regions && !full) {
      continue;
    }
    if (!p->mapped) {
      XMapRaised(g_dpy, p->xid);
      p->mapped = 1;
      pop_regrab(win);
    }
    pop_put(win, p, full);
    p->drawn = 1;
    flush = true;
  }
  if (flush) {
    XFlush(g_dpy);
  }
}

/* Input on a popup's window, in its window's coordinates. */
static int x11_popup_event(XEvent *ev)
{
  BKWindow *win = NULL;
  XPop *p = NULL;
  for (BKWindow *w = g_windows; w && !p; w = w->next) {
    for (int i = 0; w->pops && i < BK_POPUP_SLOTS; i++) {
      if (w->pops[i].xid && w->pops[i].xid == ev->xany.window) {
        win = w;
        p = &w->pops[i];
        break;
      }
    }
  }
  if (!p) {
    return 0;
  }
  BKContext *ctx = win->ctx;
  switch (ev->type) {
    case Expose:
      if (ev->xexpose.count == 0 && p->drawn) {
        pop_put(win, p, 1); /* its pixels are kept: shown again, not drawn */
        XFlush(g_dpy);
      }
      break;
    case EnterNotify:
      win->leave_pending = 0;
      break;
    case LeaveNotify:
      if (ev->xcrossing.mode == NotifyNormal) {
        win->leave_pending = 1;
      }
      break;
    case MotionNotify: {
      const XMotionEvent *m = &ev->xmotion;
      if (m->x < 0 || m->y < 0 || m->x >= p->w || m->y >= p->h) {
        win->leave_pending = 1; /* over another application, while grabbed */
        break;
      }
      win->leave_pending = 0;
      BK_motion(ctx, p->ox + m->x, p->oy + m->y, mods_of(m->state));
      break;
    }
    case ButtonPress: {
      const XButtonEvent *b = &ev->xbutton;
      if (b->x < 0 || b->y < 0 || b->x >= p->w || b->y >= p->h) {
        /* a click outside the application while grabbed: the autohide
         * popups close and it goes no further (GTK's autohide) */
        if (b->button <= Button3) {
          bk_close_autohide_popups(ctx);
        }
        break;
      }
      const int x = p->ox + b->x, y = p->oy + b->y, mods = mods_of(b->state);
      switch (b->button) {
        case Button1:
          BK_press(ctx, x, y, mods | BK_BUTTON1);
          break;
        case Button2:
          BK_button(ctx, x, y, 2, 0, mods);
          break;
        case Button3:
          BK_context_menu(ctx, x, y, mods);
          break;
        case Button4:
        case Button5:
          BK_scroll(ctx, x, y, 0.0f, b->button == Button4 ? 1.0f : -1.0f, mods & ~BK_BUTTON1);
          break;
        case 6:
        case 7:
          BK_scroll(ctx, x, y, b->button == 6 ? -1.0f : 1.0f, 0.0f, mods & ~BK_BUTTON1);
          break;
      }
      break;
    }
    case ButtonRelease: {
      const XButtonEvent *b = &ev->xbutton;
      const int x = p->ox + b->x, y = p->oy + b->y, mods = mods_of(b->state) & ~BK_BUTTON1;
      if (b->button == Button1) {
        BK_release(ctx, x, y, mods);
      }
      else if (b->button == Button2 || b->button == Button3) {
        BK_button(ctx, x, y, (int)b->button == Button2 ? 2 : 3, 1, mods);
      }
      break;
    }
    case KeyPress:
    case KeyRelease: /* grabbed: the keys of no window of ours come here */
      handle_key(win, &ev->xkey, ev->type == KeyPress);
      break;
  }
  return 1;
}
