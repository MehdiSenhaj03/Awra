/* BUI-Awra: popups on Windows (included by kit_window_win32.c).
 *
 * As GDK's Win32 backend: a popup is a WS_POPUP tool window owned by its
 * window, never activated (the window keeps the keyboard and its active
 * look), placed in screen coordinates by GDK's layout helper against the
 * work area of the monitor its anchor is on (gdk_win32_surface_layout_popup)
 * and moved with its window. As Awra 1's popovers there (awra-win32-popup),
 * DWM draws it: its small rounded corners on Windows 11 (square on 10), its
 * shadow and the Acrylic backdrop (the accent blur on Windows 10) behind a
 * client area of glass, so the popup paints those corners and no shadow
 * (BK_POPHOST_NATIVE_FRAME). An autohide popup closes when its window
 * stops being active (a click on another window, the desktop or the
 * taskbar, Alt+Tab), as Windows' own menus do; clicks on its window close
 * it in the kit. Each draws into its own raster, blitted where it changed.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#  define BK_DWMWCP_ROUNDSMALL 3

typedef struct WinPop {
  BKWindow *win;
  int slot;
  HWND hwnd;
  BuiRaster *raster;
  int ox, oy, w, h; /* the window's client px (no shadow: DWM draws it) */
  BKRect box;
  int grab, blur, shown, drawn;
} WinPop;

static const wchar_t *k_popup_class = L"BUIKitPopup";

static int win32_pop_place(void *data, int slot, const BKPopupLayout *l, int w, int h, int grab,
                           BKRect *r_rect);
static void win32_pop_close(void *data, int slot);

static void win32_popups_init(BKWindow *win)
{
  dwm_load();
  win->pophost.data = win;
  win->pophost.flags = BK_POPHOST_NATIVE_FRAME;
  /* DWMWCP_ROUNDSMALL: 4 px on Windows 11; Windows 10 doesn't round */
  win->pophost.corner_radius = g_dwm.build >= BUILD_WIN11 ? (int)(4 * win->scale + 0.5f) : 0;
  win->pophost.place = win32_pop_place;
  win->pophost.close = win32_pop_close;
  if (win->flags & BK_WINDOW_TRANSPARENT) {
    bk_popup_host_set(win->ctx, &win->pophost);
  }
}

static int is_our_popup(const BKWindow *win, HWND h)
{
  for (int i = 0; h && win->pops && i < BK_POPUP_SLOTS; i++) {
    if (win->pops[i].hwnd == h) {
      return 1;
    }
  }
  return 0;
}

/* The pointer is on the window or one of its popups. */
static int pointer_on_window(const BKWindow *win)
{
  POINT p;
  GetCursorPos(&p);
  HWND h = WindowFromPoint(p);
  return h && (h == win->hwnd || is_our_popup(win, h));
}

static LRESULT CALLBACK popup_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  WinPop *p = (WinPop *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  if (!p || !p->win) {
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
  BKWindow *win = p->win;
  BKContext *ctx = win->ctx;
  const int x = p->ox + GET_X_LPARAM(lp), y = p->oy + GET_Y_LPARAM(lp);
  switch (msg) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE; /* the window keeps the keyboard */
    case WM_NCHITTEST:
      return HTCLIENT;
    case WM_SETCURSOR:
      if (LOWORD(lp) == HTCLIENT) {
        SetCursor(cursor_handle(win->cursor));
        return TRUE;
      }
      break;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      if (p->drawn) { /* its pixels are kept: shown again, not drawn */
        BUI_raster_present_native(p->raster, hwnd, 1);
      }
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_MOUSEMOVE: {
      TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd, 0};
      TrackMouseEvent(&t);
      BK_motion(ctx, x, y, mods_now(wp, 1));
      return 0;
    }
    case WM_MOUSELEAVE:
      PostMessageW(win->hwnd, WM_BK_LEAVE, 0, 0); /* a leave unless it is on the window now */
      return 0;
    case WM_LBUTTONDOWN:
      SetCapture(hwnd);
      BK_press(ctx, x, y, mods_now(wp, 1));
      return 0;
    case WM_LBUTTONUP:
      ReleaseCapture();
      BK_release(ctx, x, y, mods_now(wp, 1));
      return 0;
    case WM_RBUTTONDOWN:
      BK_context_menu(ctx, x, y, mods_now(wp, 1));
      return 0;
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      BK_button(ctx, x, y, msg == WM_RBUTTONUP ? 3 : 2, msg != WM_MBUTTONDOWN, mods_now(wp, 1));
      return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      /* screen coordinates: the window turns them into its own */
      return SendMessageW(win->hwnd, msg, wp, lp);
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

/* The work area of the monitor the anchor (screen px) is most on. */
static RECT work_area(const RECT *anchor)
{
  MONITORINFO mi = {sizeof(mi)};
  HMONITOR m = MonitorFromRect(anchor, MONITOR_DEFAULTTONEAREST);
  if (m && GetMonitorInfoW(m, &mi)) {
    return mi.rcWork;
  }
  RECT r = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
  return r;
}

static void pop_backdrop(BKWindow *win, WinPop *p)
{
  if (!p->hwnd) {
    return;
  }
  const int mode = backdrop_mode();
  const int on = p->blur && blur_capable();
  if (mode == 2) {
    const int backdrop = on ? BK_DWMSBT_TRANSIENTWINDOW : BK_DWMSBT_NONE;
    dwm_attr(p->hwnd, BK_DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
  }
  else if (mode == 1) {
    set_accent(p->hwnd, on ? BK_ACCENT_ENABLE_BLURBEHIND : BK_ACCENT_DISABLED);
  }
  const BOOL dark = win->dark;
  dwm_attr(p->hwnd, BK_DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

static int win32_pop_place(void *data, int slot, const BKPopupLayout *l, int w, int h, int grab,
                           BKRect *r_rect)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->hwnd) {
    return 0;
  }
  if (!win->pops) {
    win->pops = (WinPop *)calloc(BK_POPUP_SLOTS, sizeof(WinPop));
    if (!win->pops) {
      return 0;
    }
  }
  WinPop *p = &win->pops[slot];
  POINT o = {0, 0};
  ClientToScreen(win->hwnd, &o);
  win->pop_origin = o;
  RECT anchor = {l->anchor.xmin + o.x, l->anchor.ymin + o.y, l->anchor.xmax + o.x,
                 l->anchor.ymax + o.y};
  anchor.right = BK_MAX(anchor.right, anchor.left + 1);
  anchor.bottom = BK_MAX(anchor.bottom, anchor.top + 1);
  const RECT wa = work_area(&anchor);
  const BKRect bounds = {wa.left - o.x, wa.top - o.y, wa.right - o.x, wa.bottom - o.y};
  BKRect box;
  bk_popup_layout_resolve(l, w, h, &bounds, &box, NULL);
  const int ox = box.xmin - l->shadow[0], oy = box.ymin - l->shadow[2];
  const int sw = bk_rw(&box) + l->shadow[0] + l->shadow[1];
  const int sh = bk_rh(&box) + l->shadow[2] + l->shadow[3];
  const int resized = sw != p->w || sh != p->h;
  const int moved = ox != p->ox || oy != p->oy;
  const int blur_changed = l->blur != p->blur;
  p->win = win;
  p->slot = slot;
  p->box = box;
  p->ox = ox;
  p->oy = oy;
  p->w = sw;
  p->h = sh;
  p->grab = grab;
  p->blur = l->blur;
  if (!p->hwnd) {
    static int registered = 0;
    HINSTANCE inst = GetModuleHandleW(NULL);
    if (!registered) {
      WNDCLASSEXW wc = {sizeof(wc)};
      wc.lpfnWndProc = popup_proc;
      wc.hInstance = inst;
      wc.lpszClassName = k_popup_class;
      if (!RegisterClassExW(&wc)) {
        return 0;
      }
      registered = 1;
    }
    p->hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, k_popup_class,
                              L"", WS_POPUP, o.x + ox, o.y + oy, BK_MAX(1, sw), BK_MAX(1, sh),
                              win->hwnd, NULL, inst, NULL);
    if (!p->hwnd) {
      return 0;
    }
    SetWindowLongPtrW(p->hwnd, GWLP_USERDATA, (LONG_PTR)p);
    p->raster = BUI_raster_create(1, 1);
    if (g_dwm.extend) {
      const MARGINS margins = {-1, -1, -1, -1}; /* glass: composed with its alpha */
      g_dwm.extend(p->hwnd, &margins);
    }
    if (g_dwm.build >= BUILD_WIN11) {
      /* tooltips round themselves (GTK's, transparent corners) */
      const int corner = l->kind == BK_POPUP_KIND_TOOLTIP ? BK_DWMWCP_DONOTROUND :
                                                            BK_DWMWCP_ROUNDSMALL;
      const COLORREF border = BK_DWMWA_COLOR_NONE;
      dwm_attr(p->hwnd, BK_DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
      dwm_attr(p->hwnd, BK_DWMWA_BORDER_COLOR, &border, sizeof(border));
    }
    pop_backdrop(win, p);
  }
  else {
    if (moved || resized) {
      SetWindowPos(p->hwnd, NULL, o.x + ox, o.y + oy, BK_MAX(1, sw), BK_MAX(1, sh),
                   SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (resized) {
      p->drawn = 0;
    }
    if (blur_changed) {
      pop_backdrop(win, p);
    }
  }
  host_request(win);
  *r_rect = box;
  return 1;
}

static void pop_destroy(WinPop *p)
{
  if (p->hwnd) {
    SetWindowLongPtrW(p->hwnd, GWLP_USERDATA, 0);
    if (GetCapture() == p->hwnd) {
      ReleaseCapture();
    }
    DestroyWindow(p->hwnd);
  }
  BUI_raster_free(p->raster);
  memset(p, 0, sizeof(*p));
}

static void win32_pop_close(void *data, int slot)
{
  BKWindow *win = (BKWindow *)data;
  if (win->pops && win->pops[slot].hwnd) {
    pop_destroy(&win->pops[slot]);
  }
}

static void win32_popups_free(BKWindow *win)
{
  for (int i = BK_POPUP_SLOTS - 1; win->pops && i >= 0; i--) {
    pop_destroy(&win->pops[i]);
  }
  if (win->ctx && win->ctx->popup_host == &win->pophost) {
    bk_popup_host_set(win->ctx, NULL);
  }
}

static void win32_popups_render(BKWindow *win)
{
  for (int i = 0; win->pops && i < BK_POPUP_SLOTS; i++) {
    WinPop *p = &win->pops[i];
    if (!p->hwnd) {
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
    if (!p->shown) {
      /* above its window, not activated; drawn right after it shows */
      SetWindowPos(p->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
      p->shown = 1;
      BUI_raster_present_native(p->raster, p->hwnd, 1);
    }
    else {
      BUI_raster_present_native(p->raster, p->hwnd, full);
    }
    p->drawn = 1;
  }
}

/* GDK's popups keep their place on a window that moved. */
static void win32_popups_follow(BKWindow *win)
{
  if (!win->pops || !win->hwnd) {
    return;
  }
  POINT o = {0, 0};
  ClientToScreen(win->hwnd, &o);
  if (o.x == win->pop_origin.x && o.y == win->pop_origin.y) {
    return;
  }
  win->pop_origin = o;
  for (int i = 0; i < BK_POPUP_SLOTS; i++) {
    WinPop *p = &win->pops[i];
    if (p->hwnd) {
      SetWindowPos(p->hwnd, NULL, o.x + p->ox, o.y + p->oy, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
}

/* The window lost the activation to `other`: its autohide popups close
 * (Windows' menus close the same way). */
static void win32_popups_deactivated(BKWindow *win, HWND other)
{
  if (!win->pops || is_our_popup(win, other)) {
    return;
  }
  bk_close_autohide_popups(win->ctx);
}
