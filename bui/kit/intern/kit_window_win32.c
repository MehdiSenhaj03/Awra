/* Native window host (Win32). Other platforms: kit_window_x11.c (X11),
 * kit_window_null.c (none).
 *
 * A plain HWND that owns a BuiRaster and drives a BKContext: mouse and
 * keyboard messages go straight to BK_* input, redraw requests coalesce into
 * one posted render message, and the damaged tiles are blitted with
 * BUI_raster_present_native. Nothing on the input -> render -> present path
 * leaves C. Animations run vsync-paced (DwmFlush).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kit_intern.h"

#ifdef _WIN32

#  ifndef UNICODE
#    define UNICODE
#  endif
#  define WIN32_LEAN_AND_MEAN
#  define COBJMACROS
#  include <windows.h>
#  include <windowsx.h>
#  include <ole2.h>
#  include <shellapi.h>
#  include <dwmapi.h>

#  include "kit_loop.h"
#  include "kit_transfer.h"

#  define WM_BK_RENDER (WM_APP + 1)
#  define ID_TIP 1
#  define ID_ANIM 2
#  define ID_WAKE 3 /* BK_TIMER_CUSTOM: custom nodes' frames */
#  define ID_USER_BASE 1000

struct BKWindow {
  HWND hwnd;
  BKContext *ctx;
  BKHost prev; /* the host's own callbacks (emit) */
  BuiRaster *raster;
  BKWindowCallbacks cb;
  int render_pending, frames, cursor, tip_id, next_timer, tracking, full;
  int want_frame, frame_vsync; /* BK_window_request_frame */
  LARGE_INTEGER frame_start;    /* when the last animation frame began */
  float scale;
  int min_w, min_h, max_w, max_h; /* client size limits (logical px; 0 = none) */
  int fixed_w, fixed_h;           /* axes the user may not resize (Tk's resizable) */
  int user_sizing;                /* inside a user move/resize loop */
  DWORD style;                    /* framed style restored by "override" 0 */
  /* BUI-Awra */
  int flags;                      /* BK_WINDOW_* */
  int state;                      /* BK_WSTATE_* last reported */
  int fullscreen, blur_on, dark;
  WINDOWPLACEMENT restore;        /* before fullscreen */
  DWORD restore_style;
  struct DropTarget *drop;
};

static int g_open = 0;
static BKWindow *g_any_window; /* BUI-Awra: a window to own the clipboard */

/* Wake BK_window_run's GetMessage without WM_QUIT: a quit message would also
 * end any other loop on this thread (e.g. a Tk mainloop in the same app). */
static void wake_loop(void)
{
  PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
}
static int g_quit = 0;
static int g_keep_alive = 0;
static int (*g_stop)(void) = NULL;

/* -- cross-thread wake: a message-only window on the loop's thread ------------------- */
#  define WM_BK_WAKE (WM_APP + 7)
static HWND g_wake_hwnd = NULL;
static void (*g_wake_cb)(void) = NULL;
static volatile LONG g_wake_pending = 0;

static LRESULT CALLBACK wake_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  if (msg == WM_BK_WAKE) {
    InterlockedExchange(&g_wake_pending, 0); /* wakes after this point post again */
    BK_image_poll(); /* decoded images: damage what shows them */
    if (g_wake_cb) {
      g_wake_cb();
    }
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

int bk_window_wake_ready(void)
{
  if (g_wake_hwnd) {
    return 1;
  }
  WNDCLASSW wc = {0};
  wc.lpfnWndProc = wake_proc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.lpszClassName = L"BUIKitWake";
  RegisterClassW(&wc); /* fails harmlessly when already registered */
  g_wake_hwnd = CreateWindowExW(0, L"BUIKitWake", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
                                wc.hInstance, NULL);
  return g_wake_hwnd != NULL;
}

int BK_window_wake_init(void (*cb)(void))
{
  g_wake_cb = cb;
  return bk_window_wake_ready();
}

void BK_window_wake(void)
{
  if (g_wake_hwnd && InterlockedExchange(&g_wake_pending, 1) == 0) {
    PostMessageW(g_wake_hwnd, WM_BK_WAKE, 0, 0);
  }
}

void BK_window_set_keep_alive(int on)
{
  g_keep_alive = on;
}
static const wchar_t *k_class = L"BUIKitWindow";

int BK_window_supported(void)
{
  return 1;
}

static void dpi_awareness(void)
{
  static int done = 0;
  if (done) {
    return;
  }
  done = 1;
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  typedef BOOL(WINAPI * SetCtx)(HANDLE);
  SetCtx set_ctx = user32 ? (SetCtx)(void *)GetProcAddress(user32,
                                                           "SetProcessDpiAwarenessContext") :
                            NULL;
  if (!set_ctx || !set_ctx((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */)) {
    SetProcessDPIAware();
  }
}

static float window_dpi_scale(HWND hwnd)
{
  const char *env = getenv("BUI_UI_SCALE");
  if (env && atof(env) > 0) {
    return (float)atof(env);
  }
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  typedef UINT(WINAPI * GetDpi)(HWND);
  GetDpi get = user32 ? (GetDpi)(void *)GetProcAddress(user32, "GetDpiForWindow") : NULL;
  const UINT dpi = (get && hwnd) ? get(hwnd) : 96;
  return BK_MAX(1.0f, dpi / 96.0f);
}

/* -- BUI-Awra: DWM backdrop (as Awra 1's win32 effect backend) ------------ */

#  define BK_DWMWA_USE_IMMERSIVE_DARK_MODE 20
#  define BK_DWMWA_WINDOW_CORNER_PREFERENCE 33
#  define BK_DWMWA_BORDER_COLOR 34
#  define BK_DWMWA_SYSTEMBACKDROP_TYPE 38
#  define BK_DWMWCP_DONOTROUND 1
#  define BK_DWMWCP_ROUND 2
#  define BK_DWMWA_COLOR_NONE 0xFFFFFFFEu
#  define BK_DWMSBT_NONE 1
#  define BK_DWMSBT_TRANSIENTWINDOW 3 /* Acrylic: blurs what is behind */
#  define BK_WCA_ACCENT_POLICY 19
#  define BK_ACCENT_DISABLED 0
#  define BK_ACCENT_ENABLE_BLURBEHIND 3
#  define BUILD_WIN10_1803 17134
#  define BUILD_WIN11 22000
#  define BUILD_WIN11_22H2 22621

typedef struct {
  int accent_state, accent_flags;
  DWORD gradient_color;
  int animation_id;
} AccentPolicy;
typedef struct {
  int attribute;
  void *data;
  SIZE_T size;
} CompositionAttribute;
typedef BOOL(WINAPI *SetCompositionFn)(HWND, CompositionAttribute *);
typedef HRESULT(WINAPI *DwmSetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT(WINAPI *DwmExtendFn)(HWND, const MARGINS *);
typedef HRESULT(WINAPI *DwmEnabledFn)(BOOL *);

static struct {
  int loaded;
  DWORD build;
  SetCompositionFn set_composition;
  DwmSetAttrFn set_attr;
  DwmExtendFn extend;
  DwmEnabledFn enabled;
} g_dwm;

static void dwm_load(void)
{
  if (g_dwm.loaded) {
    return;
  }
  g_dwm.loaded = 1;
  typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW *);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  RtlGetVersionFn rtl = ntdll ? (RtlGetVersionFn)(void *)GetProcAddress(ntdll, "RtlGetVersion") :
                                NULL;
  OSVERSIONINFOW info;
  memset(&info, 0, sizeof(info));
  info.dwOSVersionInfoSize = sizeof(info);
  if (rtl && rtl(&info) == 0) {
    g_dwm.build = info.dwBuildNumber;
  }
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  g_dwm.set_composition = user32 ? (SetCompositionFn)(void *)GetProcAddress(
                                       user32, "SetWindowCompositionAttribute") :
                                   NULL;
  HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
  if (dwm) {
    g_dwm.set_attr = (DwmSetAttrFn)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    g_dwm.extend = (DwmExtendFn)(void *)GetProcAddress(dwm, "DwmExtendFrameIntoClientArea");
    g_dwm.enabled = (DwmEnabledFn)(void *)GetProcAddress(dwm, "DwmIsCompositionEnabled");
  }
}

static void dwm_attr(HWND hwnd, DWORD attr, const void *v, DWORD size)
{
  if (g_dwm.set_attr) {
    g_dwm.set_attr(hwnd, attr, v, size);
  }
}

/* Settings > Personalisation > Colours > Transparency effects. */
static int transparency_enabled(void)
{
  DWORD enabled = 1, size = sizeof(enabled);
  RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"EnableTransparency", RRF_RT_REG_DWORD, NULL, &enabled, &size);
  return enabled != 0;
}

/* 2 the system backdrop (Windows 11 22H2+), 1 the accent blur (Windows 10
 * 1803+), 0 none */
static int backdrop_mode(void)
{
  dwm_load();
  if (g_dwm.build >= BUILD_WIN11_22H2 && g_dwm.set_attr && g_dwm.extend) {
    return 2;
  }
  if (g_dwm.build >= BUILD_WIN10_1803 && g_dwm.set_composition) {
    return 1;
  }
  return 0;
}

static int blur_capable(void)
{
  BOOL composition = FALSE;
  return backdrop_mode() != 0 && g_dwm.enabled && SUCCEEDED(g_dwm.enabled(&composition)) &&
         composition && transparency_enabled();
}

static void set_accent(HWND hwnd, int state)
{
  AccentPolicy policy = {state, 2, 0, 0};
  CompositionAttribute attribute = {BK_WCA_ACCENT_POLICY, &policy, sizeof(policy)};
  if (g_dwm.set_composition) {
    g_dwm.set_composition(hwnd, &attribute);
  }
}

static wchar_t *utf8_to_wide(const char *s)
{
  const int n = MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, NULL, 0);
  wchar_t *w = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)n);
  MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, w, n);
  return w;
}

static char *wide_to_utf8(const wchar_t *w, int len)
{
  const int n = WideCharToMultiByte(CP_UTF8, 0, w, len, NULL, 0, NULL, NULL);
  char *s = (char *)malloc((size_t)n + 1);
  WideCharToMultiByte(CP_UTF8, 0, w, len, s, n, NULL, NULL);
  s[n] = 0;
  return s;
}

/* -- host services -- */

static void host_request(void *data)
{
  BKWindow *win = (BKWindow *)data;
  if (win->hwnd && !win->render_pending) {
    win->render_pending = 1;
    PostMessageW(win->hwnd, WM_BK_RENDER, 0, 0);
  }
}

static HCURSOR cursor_handle(int c)
{
  LPCWSTR id = IDC_ARROW;
  switch (c) {
    case BK_CURSOR_EW:
      id = IDC_SIZEWE;
      break;
    case BK_CURSOR_NS:
      id = IDC_SIZENS;
      break;
    case BK_CURSOR_TEXT:
      id = IDC_IBEAM;
      break;
    case BK_CURSOR_CROSSHAIR:
      id = IDC_CROSS;
      break;
    case BK_CURSOR_HAND:
      id = IDC_HAND;
      break;
    case BK_CURSOR_MOVE:
      id = IDC_SIZEALL;
      break;
    case BK_CURSOR_NESW:
      id = IDC_SIZENESW;
      break;
    case BK_CURSOR_NWSE:
      id = IDC_SIZENWSE;
      break;
    case BK_CURSOR_NOT_ALLOWED:
      id = IDC_NO;
      break;
    case BK_CURSOR_WAIT:
      id = IDC_WAIT;
      break;
    case BK_CURSOR_PROGRESS:
      id = IDC_APPSTARTING;
      break;
    case BK_CURSOR_NONE:
      return NULL;
  }
  return LoadCursorW(NULL, id);
}

static void host_set_cursor(void *data, int cursor)
{
  BKWindow *win = (BKWindow *)data;
  win->cursor = cursor;
  SetCursor(cursor_handle(cursor));
}

static void host_warp(void *data, int x, int y)
{
  BKWindow *win = (BKWindow *)data;
  POINT p = {x, y};
  ClientToScreen(win->hwnd, &p);
  SetCursorPos(p.x, p.y);
}

/* BUI-Awra: plain text through the typed clipboard (BK_window_clipboard_*) */
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

static void host_schedule(void *data, int ms, int timer)
{
  BKWindow *win = (BKWindow *)data;
  if (timer == BK_TIMER_CUSTOM) { /* its own timer: tooltips keep theirs */
    SetTimer(win->hwnd, ID_WAKE, (UINT)(ms > 0 ? ms : 1), NULL);
    return;
  }
  win->tip_id = timer;
  SetTimer(win->hwnd, ID_TIP, (UINT)ms, NULL);
}

static void host_emit(void *data, BKNode *node, int kind, void *ref, double a, double b)
{
  BKWindow *win = (BKWindow *)data;
  if (win->prev.emit) {
    win->prev.emit(win->prev.data, node, kind, ref, a, b);
  }
}

/* -- rendering -- */

typedef HRESULT(WINAPI *DwmFlushFn)(void);

static void vsync(void)
{
  static DwmFlushFn flush = NULL;
  static int loaded = 0;
  if (!loaded) {
    loaded = 1;
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    flush = dwm ? (DwmFlushFn)(void *)GetProcAddress(dwm, "DwmFlush") : NULL;
  }
  if (flush) {
    flush();
  }
  else {
    Sleep(8);
  }
}

/* The primary display's refresh interval in ms. */
static double refresh_ms(void)
{
  static double ms = 0.0;
  if (ms <= 0.0) {
    DEVMODEW dm;
    memset(&dm, 0, sizeof(dm));
    dm.dmSize = sizeof(dm);
    const int hz = EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) ? (int)dm.dmDisplayFrequency : 0;
    ms = 1000.0 / (hz > 1 ? hz : 60);
  }
  return ms;
}

/* Milliseconds since the current animation frame began. */
static double frame_ms(const BKWindow *win)
{
  LARGE_INTEGER now, freq;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&freq);
  return (double)(now.QuadPart - win->frame_start.QuadPart) * 1000.0 / (double)freq.QuadPart;
}

static void do_render(BKWindow *win)
{
  RECT rc;
  GetClientRect(win->hwnd, &rc);
  const int w = BK_MAX(1, rc.right - rc.left), h = BK_MAX(1, rc.bottom - rc.top);
  const int resized = BUI_raster_width(win->raster) != w || BUI_raster_height(win->raster) != h;
  /* BUI_PROFILE: kit raster and present times, averaged over 60 frames */
  static int prof = -1;
  static double acc_render, acc_present;
  static int acc_n;
  LARGE_INTEGER t0, t1, t2, freq;
  if (prof < 0) {
    prof = getenv("BUI_PROFILE") != NULL;
  }
  QueryPerformanceCounter(&t0);
  if (BUI_raster_gpu_poll(win->raster)) {
    win->full = 1; /* the renderer changed: its frame starts empty */
  }
  const int regions = BK_render(win->ctx, win->raster, w, h, win->full);
  QueryPerformanceCounter(&t1);
  win->full = 0;
  if (regions || resized) {
    BUI_raster_present_native(win->raster, win->hwnd, resized);
    win->frames++;
  }
  QueryPerformanceCounter(&t2);
  if (prof) {
    QueryPerformanceFrequency(&freq);
    acc_render += (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;
    acc_present += (double)(t2.QuadPart - t1.QuadPart) * 1000.0 / (double)freq.QuadPart;
    if (++acc_n == 60) {
      fprintf(stderr, "kit ms: raster %.2f  present %.2f  (%dx%d)\n", acc_render / 60,
              acc_present / 60, w, h);
      acc_render = acc_present = 0;
      acc_n = 0;
    }
  }
  if (BK_animating(win->ctx)) {
    vsync(); /* pace animation frames to the display */
    host_request(win);
  }
}

/* -- input mapping -- */

static int mods_now(WPARAM wp, int mouse)
{
  int m = 0;
  if (GetKeyState(VK_SHIFT) < 0) {
    m |= BK_SHIFT;
  }
  if (GetKeyState(VK_CONTROL) < 0) {
    m |= BK_CTRL;
  }
  if (GetKeyState(VK_MENU) < 0) {
    m |= BK_ALT;
  }
  if (mouse && (wp & MK_LBUTTON)) {
    m |= BK_BUTTON1;
  }
  return m;
}

static const char *vk_keysym(WPARAM vk, char *buf)
{
  switch (vk) {
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Prior";
    case VK_NEXT: return "Next";
    case VK_RETURN: return "Return";
    case VK_ESCAPE: return "Escape";
    case VK_BACK: return "BackSpace";
    case VK_DELETE: return "Delete";
    case VK_TAB: return "Tab";
    case VK_INSERT: return "Insert";
    case VK_SHIFT: return "Shift_L";
    case VK_CONTROL: return "Control_L";
    case VK_MENU: return "Alt_L";
    case VK_SUBTRACT: return "KP_Subtract";
    case VK_OEM_MINUS: return "minus";
    case VK_SPACE: return "space";
    /* BUI-Awra: the rest of the keyboard (captures, shortcuts) */
    case VK_LWIN: return "Super_L";
    case VK_RWIN: return "Super_R";
    case VK_APPS: return "Menu";
    case VK_CAPITAL: return "Caps_Lock";
    case VK_NUMLOCK: return "Num_Lock";
    case VK_SCROLL: return "Scroll_Lock";
    case VK_PAUSE: return "Pause";
    case VK_SNAPSHOT: return "Print";
    case VK_MULTIPLY: return "KP_Multiply";
    case VK_ADD: return "KP_Add";
    case VK_DECIMAL: return "KP_Decimal";
    case VK_DIVIDE: return "KP_Divide";
  }
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
    snprintf(buf, 8, "KP_%d", (int)(vk - VK_NUMPAD0));
    return buf;
  }
  if (vk >= VK_OEM_1 && vk <= VK_OEM_102) {
    /* punctuation: the X keysym name of the character the layout gives */
    const UINT ch = MapVirtualKeyW((UINT)vk, 2 /* MAPVK_VK_TO_CHAR */) & 0x7FFF;
    static const struct {
      char c;
      const char *name;
    } names[] = {{',', "comma"},     {'.', "period"},      {'/', "slash"},
                 {';', "semicolon"}, {'\'', "apostrophe"}, {'[', "bracketleft"},
                 {']', "bracketright"}, {'\\', "backslash"}, {'`', "grave"},
                 {'-', "minus"},     {'=', "equal"},       {'<', "less"}};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
      if ((UINT)names[i].c == ch) {
        return names[i].name;
      }
    }
    if (ch > 32 && ch < 127) {
      buf[0] = (char)ch;
      buf[1] = 0;
      return buf;
    }
  }
  if (vk >= VK_F1 && vk <= VK_F24) {
    snprintf(buf, 8, "F%d", (int)(vk - VK_F1 + 1));
    return buf;
  }
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
    buf[0] = (char)((vk >= 'A' && vk <= 'Z') ? vk - 'A' + 'a' : vk);
    buf[1] = 0;
    return buf;
  }
  return NULL;
}

/* -- BUI-Awra: window state ---------------------------------------------- */

static int compute_state(BKWindow *win)
{
  int st = 0;
  if (!win->hwnd) {
    return 0;
  }
  if (IsZoomed(win->hwnd)) {
    st |= BK_WSTATE_MAXIMIZED;
  }
  if (win->fullscreen) {
    st |= BK_WSTATE_FULLSCREEN;
  }
  if (GetForegroundWindow() == win->hwnd || GetActiveWindow() == win->hwnd) {
    st |= BK_WSTATE_ACTIVE;
  }
  if (!IsWindowVisible(win->hwnd) || IsIconic(win->hwnd)) {
    st |= BK_WSTATE_HIDDEN;
  }
  if ((win->flags & BK_WINDOW_TRANSPARENT) && blur_capable()) {
    st |= BK_WSTATE_BLUR;
  }
  return st;
}

static void report_state(BKWindow *win)
{
  const int st = compute_state(win);
  if (st != win->state) {
    win->state = st;
    if (win->cb.on_state) {
      win->cb.on_state(win->cb.data, win, st);
    }
  }
}

/* PC set-1 scan codes (extended: 0xE0xx) to Linux evdev codes, the names
 * Awra's key codes use. Below 0x59 they are the same numbers. */
static int scancode_to_evdev(int sc, int extended)
{
  if (!extended) {
    switch (sc) {
      case 0x59: return 117; /* KP_Equal */
      case 0x64: return 183; /* F13.. */
      case 0x65: return 184;
      case 0x66: return 185;
      case 0x67: return 186;
      case 0x68: return 187;
      case 0x69: return 188;
      case 0x6A: return 189;
      case 0x6B: return 190;
      case 0x6C: return 191;
      case 0x6D: return 192;
      case 0x6E: return 193;
      case 0x76: return 194;
      case 0x70: return 93;  /* Katakana/Hiragana */
      case 0x73: return 89;  /* RO */
      case 0x79: return 92;  /* Henkan */
      case 0x7B: return 94;  /* Muhenkan */
      case 0x7D: return 124; /* Yen */
      case 0x7E: return 121; /* KP_Comma */
    }
    return sc < 0x59 ? sc : 0;
  }
  switch (sc) {
    case 0x1C: return 96;  /* KP_Enter */
    case 0x1D: return 97;  /* Control_R */
    case 0x35: return 98;  /* KP_Divide */
    case 0x37: return 99;  /* Print */
    case 0x38: return 100; /* Alt_R */
    case 0x45: return 69;  /* Num_Lock */
    case 0x46: return 119; /* Pause (Ctrl) */
    case 0x47: return 102; /* Home */
    case 0x48: return 103; /* Up */
    case 0x49: return 104; /* Prior */
    case 0x4B: return 105; /* Left */
    case 0x4D: return 106; /* Right */
    case 0x4F: return 107; /* End */
    case 0x50: return 108; /* Down */
    case 0x51: return 109; /* Next */
    case 0x52: return 110; /* Insert */
    case 0x53: return 111; /* Delete */
    case 0x5B: return 125; /* Super_L */
    case 0x5C: return 126; /* Super_R */
    case 0x5D: return 127; /* Menu */
  }
  return 0;
}

static int evdev_to_scancode(int ev)
{
  for (int sc = 1; sc < 0x80; sc++) {
    if (scancode_to_evdev(sc, 0) == ev) {
      return sc;
    }
    if (scancode_to_evdev(sc, 1) == ev) {
      return sc | 0xE000;
    }
  }
  return 0;
}

/* The character the layout prints on a physical key (without modifiers),
 * for BK_key_label. */
static const char *win32_key_label(int evdev)
{
  static char out[8];
  const int sc = evdev_to_scancode(evdev);
  if (!sc) {
    return NULL;
  }
  HKL layout = GetKeyboardLayout(0);
  const UINT vk = MapVirtualKeyExW((UINT)sc, 3 /* MAPVK_VSC_TO_VK_EX */, layout);
  if (!vk) {
    return NULL;
  }
  BYTE keys[256];
  memset(keys, 0, sizeof(keys));
  wchar_t wc[4];
  /* flag 4: leaves the keyboard state (dead keys) untouched */
  int n = ToUnicodeEx(vk, (UINT)sc, keys, wc, 4, 4, layout);
  if (n < 0) {
    n = 1; /* a dead key: its own character */
  }
  if (n <= 0 || wc[0] < 32) {
    return NULL;
  }
  const int len = WideCharToMultiByte(CP_UTF8, 0, wc, 1, out, (int)sizeof(out) - 1, NULL, NULL);
  out[len > 0 ? len : 0] = 0;
  return out[0] ? out : NULL;
}

/* -- BUI-Awra: file drops (OLE: hover and drop) ----------------------------- */

typedef struct DropTarget {
  IDropTarget iface;
  LONG refs;
  BKWindow *win;
  char *uris; /* of the drag in progress */
} DropTarget;

static HRESULT STDMETHODCALLTYPE dt_query(IDropTarget *self, REFIID riid, void **out)
{
  if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) {
    *out = self;
    self->lpVtbl->AddRef(self);
    return S_OK;
  }
  *out = NULL;
  return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE dt_addref(IDropTarget *self)
{
  return (ULONG)InterlockedIncrement(&((DropTarget *)self)->refs);
}
static ULONG STDMETHODCALLTYPE dt_release(IDropTarget *self)
{
  DropTarget *t = (DropTarget *)self;
  const LONG n = InterlockedDecrement(&t->refs);
  if (n == 0) {
    free(t->uris);
    free(t);
  }
  return (ULONG)n;
}

/* file:// URIs of a CF_HDROP, '\r\n'-separated (text/uri-list) */
static char *hdrop_uris(HDROP drop)
{
  const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
  size_t cap = 256, len = 0;
  char *out = (char *)malloc(cap);
  if (!out) {
    return NULL;
  }
  out[0] = 0;
  for (UINT i = 0; i < n; i++) {
    const UINT wl = DragQueryFileW(drop, i, NULL, 0);
    wchar_t *w = (wchar_t *)malloc(sizeof(wchar_t) * (wl + 1));
    if (!w) {
      continue;
    }
    DragQueryFileW(drop, i, w, wl + 1);
    char *path = wide_to_utf8(w, -1);
    free(w);
    const size_t pl = strlen(path);
    while (len + pl * 3 + 16 > cap) {
      cap *= 2;
      out = (char *)realloc(out, cap);
    }
    len += (size_t)sprintf(out + len, "file:///");
    for (const char *c = path; *c; c++) {
      const unsigned char ch = (unsigned char)*c;
      if (ch == '\\') {
        out[len++] = '/';
      }
      else if (ch == '%' || ch == ' ' || ch == '#' || ch < 32) {
        len += (size_t)sprintf(out + len, "%%%02X", ch);
      }
      else {
        out[len++] = (char)ch;
      }
    }
    out[len++] = '\r';
    out[len++] = '\n';
    out[len] = 0;
    free(path);
  }
  return out;
}

static void dt_point(DropTarget *t, POINTL pt, int *x, int *y)
{
  POINT p = {pt.x, pt.y};
  ScreenToClient(t->win->hwnd, &p);
  *x = p.x;
  *y = p.y;
}

static HRESULT STDMETHODCALLTYPE dt_enter(IDropTarget *self, IDataObject *data, DWORD keys,
                                          POINTL pt, DWORD *effect)
{
  DropTarget *t = (DropTarget *)self;
  (void)keys;
  FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  STGMEDIUM med;
  free(t->uris);
  t->uris = NULL;
  if (SUCCEEDED(IDataObject_GetData(data, &fmt, &med))) {
    HDROP drop = (HDROP)GlobalLock(med.hGlobal);
    if (drop) {
      t->uris = hdrop_uris(drop);
      GlobalUnlock(med.hGlobal);
    }
    ReleaseStgMedium(&med);
  }
  if (!t->uris) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }
  int x, y;
  dt_point(t, pt, &x, &y);
  const int ok = BK_drop(t->win->ctx, BK_DROP_ENTER, x, y, t->uris);
  *effect = ok ? DROPEFFECT_COPY : DROPEFFECT_NONE;
  return S_OK;
}
static HRESULT STDMETHODCALLTYPE dt_over(IDropTarget *self, DWORD keys, POINTL pt, DWORD *effect)
{
  DropTarget *t = (DropTarget *)self;
  (void)keys;
  if (!t->uris) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }
  int x, y;
  dt_point(t, pt, &x, &y);
  const int ok = BK_drop(t->win->ctx, BK_DROP_MOTION, x, y, t->uris);
  *effect = ok ? DROPEFFECT_COPY : DROPEFFECT_NONE;
  return S_OK;
}
static HRESULT STDMETHODCALLTYPE dt_leave(IDropTarget *self)
{
  DropTarget *t = (DropTarget *)self;
  BK_drop(t->win->ctx, BK_DROP_LEAVE, 0, 0, NULL);
  free(t->uris);
  t->uris = NULL;
  return S_OK;
}
static HRESULT STDMETHODCALLTYPE dt_drop(IDropTarget *self, IDataObject *data, DWORD keys,
                                         POINTL pt, DWORD *effect)
{
  DropTarget *t = (DropTarget *)self;
  (void)data;
  (void)keys;
  int x, y;
  dt_point(t, pt, &x, &y);
  const int ok = t->uris ? BK_drop(t->win->ctx, BK_DROP_DROP, x, y, t->uris) : 0;
  *effect = ok ? DROPEFFECT_COPY : DROPEFFECT_NONE;
  free(t->uris);
  t->uris = NULL;
  return S_OK;
}

static IDropTargetVtbl g_drop_vtbl = {dt_query, dt_addref, dt_release, dt_enter,
                                      dt_over,  dt_leave,  dt_drop};

static void drop_register(BKWindow *win)
{
  static int ole = 0;
  if (!ole) {
    ole = SUCCEEDED(OleInitialize(NULL)) ? 1 : -1;
  }
  if (ole < 0) {
    DragAcceptFiles(win->hwnd, TRUE); /* drops without hover */
    return;
  }
  DropTarget *t = (DropTarget *)calloc(1, sizeof(DropTarget));
  if (!t) {
    return;
  }
  t->iface.lpVtbl = &g_drop_vtbl;
  t->refs = 1;
  t->win = win;
  if (FAILED(RegisterDragDrop(win->hwnd, &t->iface))) {
    dt_release(&t->iface);
    DragAcceptFiles(win->hwnd, TRUE);
    return;
  }
  win->drop = t;
}

static void drop_unregister(BKWindow *win)
{
  if (win->drop) {
    if (win->hwnd) {
      RevokeDragDrop(win->hwnd);
    }
    win->drop->win = NULL;
    dt_release(&win->drop->iface);
    win->drop = NULL;
  }
}

/* -- BUI-Awra: borderless window ------------------------------------------- */

static int frame_band(HWND hwnd)
{
  typedef UINT(WINAPI * GetDpiFn)(HWND);
  typedef int(WINAPI * MetricsFn)(int, UINT);
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  GetDpiFn get_dpi = user32 ? (GetDpiFn)(void *)GetProcAddress(user32, "GetDpiForWindow") : NULL;
  MetricsFn metrics = user32 ? (MetricsFn)(void *)GetProcAddress(user32, "GetSystemMetricsForDpi") :
                               NULL;
  if (get_dpi && metrics) {
    const UINT dpi = get_dpi(hwnd);
    return metrics(SM_CXSIZEFRAME, dpi) + metrics(SM_CXPADDEDBORDER, dpi);
  }
  return GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

static int can_resize(BKWindow *win)
{
  return !win->fixed_w && !win->fixed_h && !IsZoomed(win->hwnd) && !win->fullscreen;
}

/* The native border band inside the edges (awra-win32-window.c) */
static LRESULT border_hit(BKWindow *win, LPARAM lp)
{
  POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
  RECT r;
  if (!can_resize(win) || !GetWindowRect(win->hwnd, &r)) {
    return HTCLIENT;
  }
  const int b = frame_band(win->hwnd);
  const int left = pt.x < r.left + b, right = pt.x >= r.right - b;
  const int top = pt.y < r.top + b, bottom = pt.y >= r.bottom - b;
  if (top) {
    return left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
  }
  if (bottom) {
    return left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
  }
  return left ? HTLEFT : right ? HTRIGHT : HTCLIENT;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  BKWindow *win = (BKWindow *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  if (!win) {
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
  BKContext *ctx = win->ctx;
  const int borderless = (win->flags & BK_WINDOW_UNDECORATED) != 0;
  switch (msg) {
    case WM_NCCALCSIZE:
      if (borderless && wp) {
        /* the client area covers the whole window; maximized, the frame
         * Windows puts off-screen is taken out */
        if (IsZoomed(hwnd) && !win->fullscreen) {
          NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS *)lp;
          const int b = frame_band(hwnd);
          p->rgrc[0].left += b;
          p->rgrc[0].top += b;
          p->rgrc[0].right -= b;
          p->rgrc[0].bottom -= b;
        }
        return 0;
      }
      break;
    case WM_NCHITTEST:
      if (borderless) {
        return border_hit(win, lp);
      }
      break;
    case WM_NCACTIVATE:
      if (borderless) {
        report_state(win);
        return DefWindowProcW(hwnd, msg, wp, -1); /* no frame repaint */
      }
      break;
    case WM_ACTIVATE:
    case WM_SHOWWINDOW:
      PostMessageW(hwnd, WM_APP + 9, 0, 0); /* the state once the change is done */
      break;
    case WM_APP + 9:
      report_state(win);
      return 0;
    case WM_INPUTLANGCHANGE:
      if (bk_layout_hook) {
        bk_layout_hook();
      }
      break;
    case WM_SETTINGCHANGE:
    case WM_DWMCOMPOSITIONCHANGED:
      win->state = -1; /* the blur capability may have changed */
      report_state(win);
      break;
    case WM_DROPFILES: {
      HDROP drop = (HDROP)wp;
      char *uris = hdrop_uris(drop);
      POINT p;
      DragQueryPoint(drop, &p);
      if (uris) {
        BK_drop(ctx, BK_DROP_DROP, p.x, p.y, uris);
        free(uris);
      }
      DragFinish(drop);
      return 0;
    }
    case WM_BK_RENDER:
      if (win->want_frame && win->cb.on_frame) {
        /* render_pending is still set: edits made here join this frame */
        win->want_frame = 0;
        QueryPerformanceCounter(&win->frame_start);
        win->cb.on_frame(win->cb.data, win);
      }
      win->render_pending = 0;
      do_render(win);
      if (win->want_frame) { /* the host asked for the next frame */
        bk_frame_ms = frame_ms(win);
        bk_frame_budget_ms = win->frame_vsync ? refresh_ms() : 0.0;
        if (win->frame_vsync && bk_frame_ms < refresh_ms() - 1.5) {
          vsync(); /* early: wait for the next refresh; late: start right away */
        }
        host_request(win);
      }
      else {
        bk_frame_ms = 0.0; /* the animation ended */
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      win->full = 1;
      do_render(win);
      BUI_raster_present_native(win->raster, hwnd, 1);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      host_request(win);
      report_state(win);
      return 0;
    case WM_DPICHANGED: {
      win->scale = HIWORD(wp) / 96.0f;
      if (!getenv("BUI_UI_SCALE")) {
        BK_set_scale(win->scale);
      }
      const RECT *r = (const RECT *)lp;
      SetWindowPos(hwnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      BK_node_relayout(ctx->root);
      host_request(win);
      return 0;
    }
    case WM_SETCURSOR:
      if (LOWORD(lp) == HTCLIENT) {
        SetCursor(cursor_handle(win->cursor));
        return TRUE;
      }
      break;
    case WM_MOUSEMOVE:
      if (!win->tracking) {
        TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&t);
        win->tracking = 1;
      }
      BK_motion(ctx, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), mods_now(wp, 1));
      return 0;
    case WM_MOUSELEAVE:
      win->tracking = 0;
      if (GetCapture() != hwnd) {
        BK_motion(ctx, -100000, -100000, 0);
      }
      return 0;
    case WM_LBUTTONDOWN:
      SetCapture(hwnd);
      SetFocus(hwnd);
      BK_press(ctx, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), mods_now(wp, 1));
      return 0;
    case WM_LBUTTONUP:
      ReleaseCapture();
      BK_release(ctx, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), mods_now(wp, 1));
      return 0;
    case WM_RBUTTONDOWN:
      BK_context_menu(ctx, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), mods_now(wp, 1));
      return 0;
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      BK_button(ctx, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), msg == WM_RBUTTONUP ? 3 : 2,
                msg != WM_MBUTTONDOWN, mods_now(wp, 1));
      return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      bk_last_key_code = scancode_to_evdev((int)((lp >> 16) & 0xFF), (int)((lp >> 24) & 1));
      char buf[8];
      const char *ks = vk_keysym(wp, buf);
      if (ks) {
        BK_key_release(ctx, ks, mods_now(0, 0));
      }
      break;
    }
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE:
      win->user_sizing = msg == WM_ENTERSIZEMOVE;
      break;
    case WM_GETMINMAXINFO: {
      MINMAXINFO *mm = (MINMAXINFO *)lp;
      if (win->user_sizing && (win->fixed_w || win->fixed_h)) { /* only the user is limited */
        RECT wr;
        GetWindowRect(hwnd, &wr);
        if (win->fixed_w) {
          mm->ptMinTrackSize.x = mm->ptMaxTrackSize.x = wr.right - wr.left;
        }
        if (win->fixed_h) {
          mm->ptMinTrackSize.y = mm->ptMaxTrackSize.y = wr.bottom - wr.top;
        }
        return 0;
      }
      const DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
      if (win->min_w || win->min_h) {
        RECT r = {0, 0, (LONG)(win->min_w * win->scale), (LONG)(win->min_h * win->scale)};
        if (!borderless) {
          AdjustWindowRect(&r, style, FALSE);
        }
        mm->ptMinTrackSize.x = win->min_w ? r.right - r.left : mm->ptMinTrackSize.x;
        mm->ptMinTrackSize.y = win->min_h ? r.bottom - r.top : mm->ptMinTrackSize.y;
      }
      if (win->max_w || win->max_h) {
        RECT r = {0, 0, (LONG)(win->max_w * win->scale), (LONG)(win->max_h * win->scale)};
        if (!borderless) {
          AdjustWindowRect(&r, style, FALSE);
        }
        mm->ptMaxTrackSize.x = win->max_w ? r.right - r.left : mm->ptMaxTrackSize.x;
        mm->ptMaxTrackSize.y = win->max_h ? r.bottom - r.top : mm->ptMaxTrackSize.y;
      }
      return 0;
    }
    case WM_MOUSEWHEEL: {
      POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      ScreenToClient(hwnd, &p);
      /* precise: touchpads send fractions of a notch (WHEEL_DELTA) */
      BK_scroll(ctx, p.x, p.y, 0.0f, (float)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA,
                mods_now(0, 0));
      return 0;
    }
    case WM_MOUSEHWHEEL: { /* tilt wheel / sideways swipe: > 0 right */
      POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      ScreenToClient(hwnd, &p);
      BK_scroll(ctx, p.x, p.y, (float)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA, 0.0f,
                mods_now(0, 0));
      return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      /* the physical key, also for the WM_CHAR this key brings */
      bk_last_key_code = scancode_to_evdev((int)((lp >> 16) & 0xFF), (int)((lp >> 24) & 1));
      char buf[8];
      const int mods = mods_now(0, 0);
      const char *ks = vk_keysym(wp, buf);
      const int is_char_key = ks && ((wp >= 'A' && wp <= 'Z') || (wp >= '0' && wp <= '9') ||
                                     wp == VK_SPACE || wp == VK_OEM_MINUS);
      /* printable keys arrive as WM_CHAR unless Ctrl/Alt are held */
      if (ks && (!is_char_key || (mods & (BK_CTRL | BK_ALT)))) {
        if (BK_key(ctx, "", ks, mods)) {
          return 0;
        }
      }
      if (msg == WM_SYSKEYDOWN) {
        break; /* Alt+F4 and the system menu */
      }
      return 0;
    }
    case WM_CHAR: {
      static wchar_t high = 0;
      wchar_t wc[3] = {(wchar_t)wp, 0, 0};
      if (wp >= 0xD800 && wp <= 0xDBFF) {
        high = (wchar_t)wp;
        return 0;
      }
      if (wp >= 0xDC00 && wp <= 0xDFFF && high) {
        wc[0] = high;
        wc[1] = (wchar_t)wp;
        high = 0;
      }
      if (wc[0] < 32 || wc[0] == 127 || (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0)) {
        return 0; /* control characters: handled as keys */
      }
      char *text = wide_to_utf8(wc, -1);
      const char *keysym = (text[0] == ' ' && !text[1]) ? "space" :
                           (text[0] == '-' && !text[1]) ? "minus" :
                                                          text;
      BK_key(ctx, text, keysym, mods_now(0, 0));
      free(text);
      return 0;
    }
    case WM_TIMER:
      if (wp == ID_TIP) {
        KillTimer(hwnd, ID_TIP);
        BK_timer(ctx, win->tip_id);
      }
      else if (wp == ID_WAKE) {
        KillTimer(hwnd, ID_WAKE);
        BK_timer(ctx, BK_TIMER_CUSTOM);
      }
      else if (wp >= ID_USER_BASE) {
        KillTimer(hwnd, wp);
        if (win->cb.on_timer) {
          win->cb.on_timer(win->cb.data, win, (int)(wp - ID_USER_BASE));
        }
      }
      return 0;
    case WM_CLOSE:
      if (win->cb.on_close && !win->cb.on_close(win->cb.data, win)) {
        return 0;
      }
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      drop_unregister(win);
      win->hwnd = NULL;
      if (--g_open <= 0) {
        wake_loop();
      }
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

BKWindow *BK_window_new(BKContext *ctx, const char *title, int width, int height)
{
  return BK_window_new_ex(ctx, title, width, height, 0);
}

BKWindow *BK_window_new_ex(BKContext *ctx, const char *title, int width, int height, int flags)
{
  dpi_awareness();
  static int registered = 0;
  HINSTANCE inst = GetModuleHandleW(NULL);
  if (!registered) {
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = NULL; /* WM_SETCURSOR */
    wc.lpszClassName = k_class;
    /* the executable's own icon (resource 1), else the default */
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) {
      wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    if (!RegisterClassExW(&wc)) {
      return NULL;
    }
    registered = 1;
  }
  bk_key_label_fn = win32_key_label;
  BKWindow *win = (BKWindow *)calloc(1, sizeof(BKWindow));
  win->ctx = ctx;
  win->prev = ctx->host;
  win->raster = BUI_raster_create(1, 1);
  win->next_timer = 1;
  win->flags = flags;
  win->state = -1;
  win->scale = window_dpi_scale(NULL);
  {
    /* system DPI until the window exists */
    HDC dc = GetDC(NULL);
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    if (!getenv("BUI_UI_SCALE")) {
      win->scale = BK_MAX(1.0f, dpi / 96.0f);
    }
  }
  BK_set_scale(win->scale);
  /* borderless: the native frame's styles stay (snap, the minimize and
   * maximize animations, Win+arrows); WM_NCCALCSIZE takes the frame away */
  const DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  RECT r = {0, 0, (LONG)(width * win->scale), (LONG)(height * win->scale)};
  if (!(flags & BK_WINDOW_UNDECORATED)) {
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  }
  wchar_t *wt = utf8_to_wide(title);
  win->hwnd = CreateWindowExW(0, k_class, wt, style, CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
  free(wt);
  if (!win->hwnd) {
    BUI_raster_free(win->raster);
    free(win);
    return NULL;
  }
  SetWindowLongPtrW(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);
  g_open++;
  if (!g_any_window) {
    g_any_window = win; /* BUI-Awra: the clipboard's owner when none is named */
  }
  if (flags & BK_WINDOW_UNDECORATED) {
    /* the new client area (the whole window) takes effect now */
    SetWindowPos(win->hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }
  dwm_load();
  if (g_dwm.build >= BUILD_WIN11) {
    /* DWM rounds the window and draws no accent border (awra_win32_style_frame) */
    const int corner = BK_DWMWCP_ROUND;
    const COLORREF border = BK_DWMWA_COLOR_NONE;
    dwm_attr(win->hwnd, BK_DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    dwm_attr(win->hwnd, BK_DWMWA_BORDER_COLOR, &border, sizeof(border));
  }
  if ((flags & BK_WINDOW_TRANSPARENT) && g_dwm.extend) {
    /* a sheet of glass: DWM composes the client area with its alpha (the
     * raster is premultiplied ARGB) */
    const MARGINS margins = {-1, -1, -1, -1};
    g_dwm.extend(win->hwnd, &margins);
  }
  if (BUI_gpu_default()) {
    BUI_raster_gpu_attach(win->raster, win->hwnd, 0);
  }
  BKHost host = {win,           host_request,       host_set_cursor, host_warp,
                 host_clipboard_get, host_clipboard_set, host_schedule, host_emit};
  BK_context_set_host(ctx, &host);
  drop_register(win);
  return win;
}

void BK_window_set_app_id(BKWindow *win, const char *app_id)
{
  (void)win;
  /* the taskbar groups the process's windows under this id */
  static int done = 0;
  if (done || !app_id || !app_id[0]) {
    return;
  }
  done = 1;
  typedef HRESULT(WINAPI * SetIdFn)(PCWSTR);
  HMODULE shell = LoadLibraryW(L"shell32.dll");
  SetIdFn set = shell ? (SetIdFn)(void *)GetProcAddress(shell,
                                                       "SetCurrentProcessExplicitAppUserModelID") :
                        NULL;
  if (set) {
    wchar_t *w = utf8_to_wide(app_id);
    set(w);
    free(w);
  }
}

int BK_window_blur_available(BKWindow *win)
{
  return win && (win->flags & BK_WINDOW_TRANSPARENT) && blur_capable();
}

/* The backdrop behind the window (Awra 1's win32 backend): Acrylic on
 * Windows 11 22H2+, the accent blur before; DWM rounds the corners when
 * radius > 0. An empty rect takes it away. */
int BK_window_set_blur(BKWindow *win, int x, int y, int w, int h, int radius)
{
  (void)x;
  (void)y;
  if (!win || !win->hwnd || !(win->flags & BK_WINDOW_TRANSPARENT)) {
    return 0;
  }
  const int mode = backdrop_mode();
  const int on = w > 0 && h > 0 && blur_capable();
  if (mode == 2) {
    const int backdrop = on ? BK_DWMSBT_TRANSIENTWINDOW : BK_DWMSBT_NONE;
    dwm_attr(win->hwnd, BK_DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
  }
  else if (mode == 1) {
    set_accent(win->hwnd, on ? BK_ACCENT_ENABLE_BLURBEHIND : BK_ACCENT_DISABLED);
  }
  if (g_dwm.build >= BUILD_WIN11) {
    const int corner = radius > 0 ? BK_DWMWCP_ROUND : BK_DWMWCP_DONOTROUND;
    dwm_attr(win->hwnd, BK_DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
  }
  win->blur_on = on;
  return on;
}

void BK_window_begin_move(BKWindow *win)
{
  if (!win || !win->hwnd) {
    return;
  }
  /* the native move loop: Aero Snap, shake, monitors */
  ReleaseCapture();
  POINT p;
  GetCursorPos(&p);
  SendMessageW(win->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(p.x, p.y));
  /* the button went up inside the move loop: the kit still thinks it is down */
  ScreenToClient(win->hwnd, &p);
  BK_release(win->ctx, p.x, p.y, mods_now(0, 0));
}

void BK_window_begin_resize(BKWindow *win, int edges)
{
  if (!win || !win->hwnd) {
    return;
  }
  WPARAM hit = 0;
  const int t = edges & BK_EDGE_TOP, b = edges & BK_EDGE_BOTTOM;
  const int l = edges & BK_EDGE_LEFT, r = edges & BK_EDGE_RIGHT;
  hit = t ? (l ? HTTOPLEFT : r ? HTTOPRIGHT : HTTOP) :
        b ? (l ? HTBOTTOMLEFT : r ? HTBOTTOMRIGHT : HTBOTTOM) :
        l ? HTLEFT : r ? HTRIGHT : 0;
  if (!hit) {
    return;
  }
  ReleaseCapture();
  POINT p;
  GetCursorPos(&p);
  SendMessageW(win->hwnd, WM_NCLBUTTONDOWN, hit, MAKELPARAM(p.x, p.y));
  ScreenToClient(win->hwnd, &p);
  BK_release(win->ctx, p.x, p.y, mods_now(0, 0));
}

void BK_window_show_menu(BKWindow *win, int x, int y)
{
  if (!win || !win->hwnd) {
    return;
  }
  HMENU menu = GetSystemMenu(win->hwnd, FALSE);
  if (!menu) {
    return;
  }
  const int zoomed = IsZoomed(win->hwnd);
  EnableMenuItem(menu, SC_RESTORE, MF_BYCOMMAND | (zoomed ? MF_ENABLED : MF_GRAYED));
  EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | (zoomed ? MF_GRAYED : MF_ENABLED));
  EnableMenuItem(menu, SC_MOVE, MF_BYCOMMAND | (zoomed ? MF_GRAYED : MF_ENABLED));
  EnableMenuItem(menu, SC_SIZE, MF_BYCOMMAND | (can_resize(win) ? MF_ENABLED : MF_GRAYED));
  POINT p = {x, y};
  ClientToScreen(win->hwnd, &p);
  const int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0,
                                      win->hwnd, NULL);
  if (cmd) {
    PostMessageW(win->hwnd, WM_SYSCOMMAND, (WPARAM)cmd, 0);
  }
}

int BK_window_state(BKWindow *win)
{
  return win ? compute_state(win) : 0;
}

float BK_window_scale(BKWindow *win)
{
  return win ? win->scale : 1.0f;
}

void BK_window_hide(BKWindow *win)
{
  if (win && win->hwnd) {
    ShowWindow(win->hwnd, SW_HIDE);
    report_state(win);
  }
}

const char *BK_window_backend(void)
{
  return "win32";
}

static void set_fullscreen(BKWindow *win, int on)
{
  HWND h = win->hwnd;
  if (!h || on == win->fullscreen) {
    return;
  }
  if (on) {
    MONITORINFO mi = {sizeof(mi)};
    win->restore.length = sizeof(win->restore);
    GetWindowPlacement(h, &win->restore);
    win->restore_style = (DWORD)GetWindowLongPtrW(h, GWL_STYLE);
    if (!GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi)) {
      return;
    }
    win->fullscreen = 1;
    SetWindowLongPtrW(h, GWL_STYLE, (LONG_PTR)(win->restore_style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
    SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
  else {
    win->fullscreen = 0;
    SetWindowLongPtrW(h, GWL_STYLE, (LONG_PTR)win->restore_style);
    SetWindowPlacement(h, &win->restore);
    SetWindowPos(h, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
  report_state(win);
}

void BK_window_free(BKWindow *win)
{
  if (!win) {
    return;
  }
  drop_unregister(win);
  if (g_any_window == win) {
    g_any_window = NULL;
  }
  if (win->hwnd) {
    SetWindowLongPtrW(win->hwnd, GWLP_USERDATA, 0);
    DestroyWindow(win->hwnd);
    if (--g_open <= 0) {
      wake_loop();
    }
  }
  if (win->ctx) {
    BK_context_set_host(win->ctx, &win->prev);
  }
  BUI_raster_free(win->raster);
  free(win);
}

void BK_window_set_callbacks(BKWindow *win, const BKWindowCallbacks *cb)
{
  win->cb = *cb;
}

void BK_window_show(BKWindow *win)
{
  if (win->hwnd) {
    ShowWindow(win->hwnd, SW_SHOW);
    UpdateWindow(win->hwnd);
  }
}

void BK_window_close(BKWindow *win)
{
  if (win->hwnd) {
    DestroyWindow(win->hwnd);
  }
}

int BK_window_is_open(BKWindow *win)
{
  return win && win->hwnd != NULL;
}

void BK_window_set_title(BKWindow *win, const char *title)
{
  if (win->hwnd) {
    wchar_t *w = utf8_to_wide(title);
    SetWindowTextW(win->hwnd, w);
    free(w);
  }
}

void BK_window_set_size(BKWindow *win, int width, int height)
{
  if (!win->hwnd) {
    return;
  }
  RECT r = {0, 0, (LONG)(width * win->scale), (LONG)(height * win->scale)};
  if (!(win->flags & BK_WINDOW_UNDECORATED)) {
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  }
  SetWindowPos(win->hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void BK_window_get_size(BKWindow *win, int *w, int *h)
{
  RECT rc = {0, 0, 0, 0};
  if (win->hwnd) {
    GetClientRect(win->hwnd, &rc);
  }
  *w = rc.right;
  *h = rc.bottom;
}

BuiRaster *BK_window_raster(BKWindow *win)
{
  return win->raster;
}

void BK_window_set_gpu(BKWindow *win, int on)
{
  if (win && on) {
    BUI_raster_gpu_attach(win->raster, win->hwnd, 0);
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
  if (win->hwnd) {
    win->render_pending = 0;
    do_render(win);
  }
}

int BK_window_timer(BKWindow *win, int ms)
{
  const int id = win->next_timer++;
  if (win->hwnd) {
    SetTimer(win->hwnd, ID_USER_BASE + (UINT_PTR)id, (UINT)BK_MAX(0, ms), NULL);
  }
  return id;
}

void BK_window_request_frame(BKWindow *win, int vsync)
{
  win->want_frame = 1;
  win->frame_vsync = vsync;
  host_request(win);
}

void BK_window_cancel_timer(BKWindow *win, int id)
{
  if (win->hwnd) {
    KillTimer(win->hwnd, ID_USER_BASE + (UINT_PTR)id);
  }
}

void *BK_window_handle(BKWindow *win)
{
  return win->hwnd;
}

static void set_style(HWND hwnd, DWORD style)
{
  SetWindowLongPtrW(hwnd, GWL_STYLE, (LONG_PTR)(style | (GetWindowLongPtrW(hwnd, GWL_STYLE) &
                                                         WS_VISIBLE)));
  SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void BK_window_wm(BKWindow *win, const char *key, double a, double b)
{
  HWND h = win->hwnd;
  if (!h) {
    return;
  }
  if (!strcmp(key, "pos")) {
    SetWindowPos(h, NULL, (int)a, (int)b, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  else if (!strcmp(key, "minsize") || !strcmp(key, "maxsize")) {
    int *w = key[1] == 'i' ? &win->min_w : &win->max_w, *hh = key[1] == 'i' ? &win->min_h :
                                                                              &win->max_h;
    *w = (int)a;
    *hh = (int)b;
  }
  else if (!strcmp(key, "resizable")) {
    DWORD style = (DWORD)GetWindowLongPtrW(h, GWL_STYLE) & ~WS_VISIBLE;
    if (style & WS_CAPTION) {
      style = (a || b) ? (style | WS_THICKFRAME | WS_MAXIMIZEBOX) :
                         (style & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
      set_style(h, style);
    }
    win->fixed_w = a == 0.0;
    win->fixed_h = b == 0.0;
  }
  else if (!strcmp(key, "topmost")) {
    SetWindowPos(h, a ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  else if (!strcmp(key, "alpha")) {
    const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    SetWindowLongPtrW(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    SetLayeredWindowAttributes(h, 0, (BYTE)(BK_CLAMP(a, 0.0, 1.0) * 255.0 + 0.5), LWA_ALPHA);
  }
  else if (!strcmp(key, "override")) {
    if (!win->style) {
      win->style = (DWORD)GetWindowLongPtrW(h, GWL_STYLE) & ~WS_VISIBLE;
    }
    set_style(h, a ? WS_POPUP | WS_CLIPCHILDREN : win->style);
  }
  else if (!strcmp(key, "state")) {
    static const int show[4] = {SW_RESTORE, SW_MINIMIZE, SW_HIDE, SW_MAXIMIZE};
    ShowWindow(h, show[BK_CLAMP((int)a, 0, 3)]);
  }
  else if (!strcmp(key, "lift") || !strcmp(key, "lower")) {
    SetWindowPos(h, key[1] == 'i' ? HWND_TOP : HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  else if (!strcmp(key, "disabled")) { /* modal dialogs disable their parent */
    EnableWindow(h, a == 0.0);
  }
  else if (!strcmp(key, "focus")) {
    SetForegroundWindow(h);
    SetFocus(h);
  }
  else if (!strcmp(key, "dark")) { /* BUI-Awra: tints the backdrop and the frame */
    const BOOL dark = a != 0.0;
    win->dark = dark;
    dwm_attr(h, BK_DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
  }
  else if (!strcmp(key, "fullscreen")) {
    set_fullscreen(win, a != 0.0);
  }
}

int BK_window_wm_get(BKWindow *win, const char *key, double *r_a, double *r_b)
{
  *r_a = *r_b = 0.0;
  if (!strcmp(key, "screen")) {
    *r_a = GetSystemMetrics(SM_CXSCREEN);
    *r_b = GetSystemMetrics(SM_CYSCREEN);
    return 1;
  }
  if (!strcmp(key, "pointer")) {
    POINT p;
    GetCursorPos(&p);
    *r_a = p.x;
    *r_b = p.y;
    return 1;
  }
  if (!strcmp(key, "scale")) {
    *r_a = win ? win->scale : 1.0;
    return 1;
  }
  if (!win || !win->hwnd) {
    return 0;
  }
  if (!strcmp(key, "pos")) { /* client origin on screen */
    POINT p = {0, 0};
    ClientToScreen(win->hwnd, &p);
    *r_a = p.x;
    *r_b = p.y;
    return 1;
  }
  if (!strcmp(key, "state")) {
    *r_a = IsIconic(win->hwnd) ? 1 : !IsWindowVisible(win->hwnd) ? 2 : IsZoomed(win->hwnd) ? 3 :
                                                                                           0;
    return 1;
  }
  return 0;
}

void BK_window_set_stop_check(int (*stop)(void))
{
  g_stop = stop;
}

void BK_window_quit(void)
{
  g_quit = 1;
  wake_loop();
}

/* BUI-Awra: a loop timer was added: shorten the current wait */
void bk_loop_wake_host(void)
{
  wake_loop();
}

void BK_window_run(void)
{
  g_quit = 0;
  MSG msg;
  while ((g_open > 0 || g_keep_alive) && !g_quit) {
    /* wait for messages, at most until the next loop timer is due */
    const int timeout = bk_loop_timeout_ms(-1);
    if (!PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE)) {
      MsgWaitForMultipleObjectsEx(0, NULL, timeout < 0 ? INFINITE : (DWORD)timeout, QS_ALLINPUT,
                                  MWMO_INPUTAVAILABLE);
    }
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        g_quit = 1;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (g_stop && g_stop()) {
        return;
      }
    }
    bk_loop_run_due();
  }
}

int BK_window_pump(void)
{
  bk_loop_run_due();
  MSG msg;
  while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
    if (g_stop && g_stop()) {
      break;
    }
  }
  return g_open;
}


/* =========================================================================
 * BUI-Awra: data transfer. The clipboard by type (text as CF_UNICODETEXT,
 * file lists as CF_HDROP, PNG images as "PNG" plus CF_DIBV5 / CF_DIB for
 * the applications that only take those, other types under their MIME
 * name), and drags out through OLE (DoDragDrop with an IDataObject, the
 * image through the shell's drag image helper).
 * ========================================================================= */

/* shlobj.h, which WIN32_LEAN_AND_MEAN leaves out */
HRESULT WINAPI SHCreateStdEnumFmtEtc(UINT cfmt, const FORMATETC afmt[], IEnumFORMATETC **out);

/* DROPFILES (shlobj.h) */
typedef struct BuiDropFiles {
  DWORD pFiles;
  POINT pt;
  BOOL fNC;
  BOOL fWide;
} BuiDropFiles;

static UINT cf_png(void)
{
  static UINT cf = 0;
  if (!cf) {
    cf = RegisterClipboardFormatA("PNG");
  }
  return cf;
}

static HGLOBAL global_bytes(const void *data, size_t len)
{
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len ? len : 1);
  void *p = h ? GlobalLock(h) : NULL;
  if (!p) {
    if (h) {
      GlobalFree(h);
    }
    return NULL;
  }
  if (len) {
    memcpy(p, data, len);
  }
  GlobalUnlock(h);
  return h;
}

static HGLOBAL global_text(const char *utf8)
{
  wchar_t *w = utf8_to_wide(utf8);
  if (!w) {
    return NULL;
  }
  HGLOBAL h = global_bytes(w, (wcslen(w) + 1) * sizeof(wchar_t));
  free(w);
  return h;
}

/* text/uri-list -> DROPFILES (wide paths, double NUL) */
static HGLOBAL global_hdrop(const char *uris, size_t len)
{
  size_t wchars = 1;
  wchar_t **paths = NULL;
  int n = 0, cap = 0;
  for (const char *p = uris, *end = uris + len; p < end;) {
    const char *eol = p;
    while (eol < end && *eol != '\n' && *eol != '\r') {
      eol++;
    }
    if (eol > p && *p != '#') {
      char *path = bk_uri_to_path(p, (size_t)(eol - p));
      if (path) {
        for (char *c = path; *c; c++) {
          if (*c == '/') {
            *c = '\\';
          }
        }
        if (n == cap) {
          cap = cap ? cap * 2 : 8;
          wchar_t **g = (wchar_t **)realloc(paths, sizeof(wchar_t *) * (size_t)cap);
          if (!g) {
            free(path);
            break;
          }
          paths = g;
        }
        paths[n] = utf8_to_wide(path);
        free(path);
        if (paths[n]) {
          wchars += wcslen(paths[n]) + 1;
          n++;
        }
      }
    }
    p = eol;
    while (p < end && (*p == '\n' || *p == '\r')) {
      p++;
    }
  }
  HGLOBAL h = NULL;
  if (n > 0) {
    h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(BuiDropFiles) + wchars * sizeof(wchar_t));
    BuiDropFiles *df = h ? (BuiDropFiles *)GlobalLock(h) : NULL;
    if (df) {
      df->pFiles = sizeof(BuiDropFiles);
      df->fWide = TRUE;
      wchar_t *w = (wchar_t *)(df + 1);
      for (int i = 0; i < n; i++) {
        const size_t l = wcslen(paths[i]) + 1;
        memcpy(w, paths[i], l * sizeof(wchar_t));
        w += l;
      }
      *w = 0;
      GlobalUnlock(h);
    }
  }
  for (int i = 0; i < n; i++) {
    free(paths[i]);
  }
  free(paths);
  return h;
}

/* A decoded image as a DIB: v5 (with its alpha) or the plain one. */
static HGLOBAL global_dib(const void *png, size_t len, int v5)
{
  BKImBuf *ib = BK_imbuf_decode(png, len, 0, 0);
  if (!ib) {
    return NULL;
  }
  int w = 0, h = 0;
  BK_imbuf_size(ib, &w, &h);
  const uint32_t *px = BK_imbuf_pixels(ib, 0);
  const size_t head = v5 ? sizeof(BITMAPV5HEADER) : sizeof(BITMAPINFOHEADER);
  const size_t bytes = (size_t)w * h * 4;
  HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, head + bytes);
  unsigned char *p = g ? (unsigned char *)GlobalLock(g) : NULL;
  if (p && px) {
    BITMAPV5HEADER *bh = (BITMAPV5HEADER *)p; /* the v4/v5 fields only when v5 */
    bh->bV5Size = (DWORD)head;
    bh->bV5Width = w;
    bh->bV5Height = h; /* bottom-up */
    bh->bV5Planes = 1;
    bh->bV5BitCount = 32;
    bh->bV5Compression = v5 ? BI_BITFIELDS : BI_RGB;
    bh->bV5SizeImage = (DWORD)bytes;
    if (v5) {
      bh->bV5RedMask = 0x00FF0000;
      bh->bV5GreenMask = 0x0000FF00;
      bh->bV5BlueMask = 0x000000FF;
      bh->bV5AlphaMask = 0xFF000000;
      bh->bV5CSType = 0x73524742; /* LCS_sRGB, 'sRGB' */
      bh->bV5Intent = LCS_GM_IMAGES;
    }
    uint32_t *dst = (uint32_t *)(p + head);
    for (int y = 0; y < h; y++) {
      const uint32_t *row = px + (size_t)(h - 1 - y) * w;
      for (int x = 0; x < w; x++) { /* straight colour */
        const uint32_t c = row[x], a = c >> 24;
        uint32_t o = c;
        if (a && a < 255) {
          o = (a << 24) | (((c >> 16 & 255) * 255 / a) << 16) | (((c >> 8 & 255) * 255 / a) << 8) |
              ((c & 255) * 255 / a);
        }
        dst[(size_t)y * w + x] = o;
      }
    }
  }
  if (p) {
    GlobalUnlock(g);
  }
  else if (g) {
    GlobalFree(g);
    g = NULL;
  }
  BK_imbuf_free(ib);
  return g;
}

/* The clipboard formats an entry goes by, and their data (one each). */
static int entry_formats(const BKClipEntry *e, UINT *cf, HGLOBAL *data, int max)
{
  int n = 0;
  if (bk_mime_is_text(e->mime)) {
    cf[n] = CF_UNICODETEXT;
    data[n++] = global_text((const char *)e->data);
  }
  else if (!strcmp(e->mime, "text/uri-list")) {
    cf[n] = CF_HDROP;
    data[n++] = global_hdrop((const char *)e->data, e->len);
  }
  else if (!strcmp(e->mime, "image/png")) {
    cf[n] = cf_png();
    data[n++] = global_bytes(e->data, e->len);
    if (max >= 3) {
      cf[n] = CF_DIBV5;
      data[n++] = global_dib(e->data, e->len, 1);
      cf[n] = CF_DIB;
      data[n++] = global_dib(e->data, e->len, 0);
    }
  }
  else {
    cf[n] = RegisterClipboardFormatA(e->mime);
    data[n++] = cf[0] ? global_bytes(e->data, e->len) : NULL;
  }
  return n;
}


static HWND clip_owner(BKWindow *win)
{
  return win && win->hwnd ? win->hwnd : (g_any_window ? g_any_window->hwnd : NULL);
}

int BK_window_clipboard_set(BKWindow *win, const BKClipItem *items, int n)
{
  BKClipSet *set = bk_clipset_new(items, n);
  if (!set) {
    return 0;
  }
  int ok = 0;
  if (OpenClipboard(clip_owner(win))) {
    EmptyClipboard();
    for (int i = 0; i < set->n; i++) {
      UINT cf[4];
      HGLOBAL data[4];
      const int k = entry_formats(&set->e[i], cf, data, 4);
      for (int j = 0; j < k; j++) {
        if (data[j] && SetClipboardData(cf[j], data[j])) {
          ok = 1;
        }
        else if (data[j]) {
          GlobalFree(data[j]);
        }
      }
    }
    CloseClipboard();
  }
  bk_clipset_free(set);
  return ok;
}

char *BK_window_clipboard_formats(BKWindow *win)
{
  if (!OpenClipboard(clip_owner(win))) {
    return NULL;
  }
  const char *names[64];
  char owned[32][128];
  int n = 0, k = 0, text = 0, files = 0, png = 0, dib = 0;
  for (UINT cf = EnumClipboardFormats(0); cf && n < 60; cf = EnumClipboardFormats(cf)) {
    if (cf == CF_UNICODETEXT || cf == CF_TEXT || cf == CF_OEMTEXT) {
      text = 1;
    }
    else if (cf == CF_HDROP) {
      files = 1;
    }
    else if (cf == cf_png()) {
      png = 1;
    }
    else if (cf == CF_DIB || cf == CF_DIBV5 || cf == CF_BITMAP) {
      dib = 1;
    }
    else if (cf >= 0xC000 && k < 32 &&
             GetClipboardFormatNameA(cf, owned[k], (int)sizeof(owned[k])) > 0 &&
             strchr(owned[k], '/'))
    {
      names[n++] = owned[k++];
    }
  }
  CloseClipboard();
  if (files) {
    names[n++] = "text/uri-list";
  }
  if (png) {
    names[n++] = "image/png";
  }
  if (dib) {
    names[n++] = "image/bmp";
  }
  if (text) {
    names[n++] = "text/plain;charset=utf-8";
  }
  return bk_mimes_join(names, n);
}

static void *global_copy(HANDLE h, size_t *r_len)
{
  const void *p = h ? GlobalLock(h) : NULL;
  if (!p) {
    return NULL;
  }
  void *out = bk_bytes_dup(p, GlobalSize(h), r_len);
  GlobalUnlock(h);
  return out;
}

void *BK_window_clipboard_read(BKWindow *win, const char *mime, size_t *r_len)
{
  *r_len = 0;
  if (!mime || !OpenClipboard(clip_owner(win))) {
    return NULL;
  }
  void *out = NULL;
  if (bk_mime_is_text(mime)) {
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    const wchar_t *w = h ? (const wchar_t *)GlobalLock(h) : NULL;
    if (w) {
      char *s = wide_to_utf8(w, -1);
      GlobalUnlock(h);
      if (s) {
        *r_len = strlen(s);
        out = s;
      }
    }
  }
  else if (!strcmp(mime, "text/uri-list")) {
    HANDLE h = GetClipboardData(CF_HDROP);
    HDROP drop = h ? (HDROP)GlobalLock(h) : NULL;
    if (drop) {
      char *s = hdrop_uris(drop);
      GlobalUnlock(h);
      if (s) {
        *r_len = strlen(s);
        out = s;
      }
    }
  }
  else if (!strcmp(mime, "image/png")) {
    out = global_copy(GetClipboardData(cf_png()), r_len);
  }
  else if (!strcmp(mime, "image/bmp")) { /* the DIB as it is (no file header) */
    HANDLE h = GetClipboardData(CF_DIBV5);
    if (!h) {
      h = GetClipboardData(CF_DIB);
    }
    out = global_copy(h, r_len);
  }
  else {
    const UINT cf = RegisterClipboardFormatA(mime);
    out = cf ? global_copy(GetClipboardData(cf), r_len) : NULL;
  }
  CloseClipboard();
  return out;
}

/* -- drags out: IDataObject, IDropSource ------------------------------------ */

typedef struct DataEntry {
  FORMATETC fmt;
  STGMEDIUM med;
} DataEntry;

typedef struct DataObj {
  IDataObject iface;
  LONG refs;
  DataEntry *e;
  int n, cap;
} DataObj;

static int dobj_find(DataObj *d, const FORMATETC *f)
{
  for (int i = 0; i < d->n; i++) {
    if (d->e[i].fmt.cfFormat == f->cfFormat && (d->e[i].fmt.tymed & f->tymed) &&
        d->e[i].fmt.dwAspect == f->dwAspect)
    {
      return i;
    }
  }
  return -1;
}

static HRESULT STDMETHODCALLTYPE do_query(IDataObject *self, REFIID riid, void **out)
{
  if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDataObject)) {
    *out = self;
    self->lpVtbl->AddRef(self);
    return S_OK;
  }
  *out = NULL;
  return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE do_addref(IDataObject *self)
{
  return (ULONG)InterlockedIncrement(&((DataObj *)self)->refs);
}
static ULONG STDMETHODCALLTYPE do_release(IDataObject *self)
{
  DataObj *d = (DataObj *)self;
  const LONG n = InterlockedDecrement(&d->refs);
  if (n == 0) {
    for (int i = 0; i < d->n; i++) {
      ReleaseStgMedium(&d->e[i].med);
    }
    free(d->e);
    free(d);
  }
  return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE do_get(IDataObject *self, FORMATETC *f, STGMEDIUM *out)
{
  DataObj *d = (DataObj *)self;
  const int i = dobj_find(d, f);
  if (i < 0) {
    return DV_E_FORMATETC;
  }
  memset(out, 0, sizeof(*out));
  const STGMEDIUM *m = &d->e[i].med;
  if (m->tymed == TYMED_HGLOBAL) { /* a copy the caller frees */
    const SIZE_T len = GlobalSize(m->hGlobal);
    const void *src = GlobalLock(m->hGlobal);
    HGLOBAL h = src ? global_bytes(src, len) : NULL;
    if (src) {
      GlobalUnlock(m->hGlobal);
    }
    if (!h) {
      return E_OUTOFMEMORY;
    }
    out->tymed = TYMED_HGLOBAL;
    out->hGlobal = h;
    return S_OK;
  }
  return DV_E_TYMED;
}
static HRESULT STDMETHODCALLTYPE do_get_here(IDataObject *self, FORMATETC *f, STGMEDIUM *m)
{
  return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE do_query_get(IDataObject *self, FORMATETC *f)
{
  return dobj_find((DataObj *)self, f) >= 0 ? S_OK : DV_E_FORMATETC;
}
static HRESULT STDMETHODCALLTYPE do_canonical(IDataObject *self, FORMATETC *in, FORMATETC *out)
{
  out->ptd = NULL;
  return E_NOTIMPL;
}
static int dobj_add(DataObj *d, CLIPFORMAT cf, HGLOBAL h)
{
  if (!h) {
    return 0;
  }
  if (d->n == d->cap) {
    const int cap = d->cap ? d->cap * 2 : 8;
    DataEntry *e = (DataEntry *)realloc(d->e, sizeof(DataEntry) * (size_t)cap);
    if (!e) {
      GlobalFree(h);
      return 0;
    }
    d->e = e;
    d->cap = cap;
  }
  DataEntry *e = &d->e[d->n++];
  memset(e, 0, sizeof(*e));
  e->fmt.cfFormat = cf;
  e->fmt.dwAspect = DVASPECT_CONTENT;
  e->fmt.lindex = -1;
  e->fmt.tymed = TYMED_HGLOBAL;
  e->med.tymed = TYMED_HGLOBAL;
  e->med.hGlobal = h;
  return 1;
}
/* the shell's drag image helper stores its own formats here */
static HRESULT STDMETHODCALLTYPE do_set(IDataObject *self, FORMATETC *f, STGMEDIUM *m, BOOL release)
{
  DataObj *d = (DataObj *)self;
  if (!f || !m || m->tymed != TYMED_HGLOBAL) {
    return E_NOTIMPL;
  }
  const int i = dobj_find(d, f);
  HGLOBAL h = m->hGlobal;
  if (!release) {
    const void *src = GlobalLock(m->hGlobal);
    h = src ? global_bytes(src, GlobalSize(m->hGlobal)) : NULL;
    if (src) {
      GlobalUnlock(m->hGlobal);
    }
  }
  if (!h) {
    return E_OUTOFMEMORY;
  }
  if (i >= 0) {
    ReleaseStgMedium(&d->e[i].med);
    d->e[i].med.tymed = TYMED_HGLOBAL;
    d->e[i].med.hGlobal = h;
    d->e[i].med.pUnkForRelease = NULL;
    return S_OK;
  }
  return dobj_add(d, f->cfFormat, h) ? S_OK : E_OUTOFMEMORY;
}
static HRESULT STDMETHODCALLTYPE do_enum(IDataObject *self, DWORD dir, IEnumFORMATETC **out)
{
  DataObj *d = (DataObj *)self;
  if (dir != DATADIR_GET) {
    return E_NOTIMPL;
  }
  FORMATETC *fmts = (FORMATETC *)malloc(sizeof(FORMATETC) * (size_t)(d->n ? d->n : 1));
  if (!fmts) {
    return E_OUTOFMEMORY;
  }
  for (int i = 0; i < d->n; i++) {
    fmts[i] = d->e[i].fmt;
  }
  const HRESULT hr = SHCreateStdEnumFmtEtc((UINT)d->n, fmts, out);
  free(fmts);
  return hr;
}
static HRESULT STDMETHODCALLTYPE do_advise(IDataObject *self, FORMATETC *f, DWORD a,
                                           IAdviseSink *s, DWORD *c)
{
  return OLE_E_ADVISENOTSUPPORTED;
}
static HRESULT STDMETHODCALLTYPE do_unadvise(IDataObject *self, DWORD c)
{
  return OLE_E_ADVISENOTSUPPORTED;
}
static HRESULT STDMETHODCALLTYPE do_enum_advise(IDataObject *self, IEnumSTATDATA **e)
{
  return OLE_E_ADVISENOTSUPPORTED;
}
static IDataObjectVtbl g_dobj_vtbl = {do_query,     do_addref,  do_release,   do_get,
                                      do_get_here,  do_query_get, do_canonical, do_set,
                                      do_enum,      do_advise,  do_unadvise,  do_enum_advise};

typedef struct DropSrc {
  IDropSource iface;
  LONG refs;
} DropSrc;
static HRESULT STDMETHODCALLTYPE ds_query(IDropSource *self, REFIID riid, void **out)
{
  if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropSource)) {
    *out = self;
    self->lpVtbl->AddRef(self);
    return S_OK;
  }
  *out = NULL;
  return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE ds_addref(IDropSource *self)
{
  return (ULONG)InterlockedIncrement(&((DropSrc *)self)->refs);
}
static ULONG STDMETHODCALLTYPE ds_release(IDropSource *self)
{
  DropSrc *d = (DropSrc *)self;
  const LONG n = InterlockedDecrement(&d->refs);
  if (n == 0) {
    free(d);
  }
  return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE ds_continue(IDropSource *self, BOOL escape, DWORD keys)
{
  if (escape) {
    return DRAGDROP_S_CANCEL;
  }
  if (!(keys & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))) {
    return DRAGDROP_S_DROP;
  }
  return S_OK;
}
static HRESULT STDMETHODCALLTYPE ds_feedback(IDropSource *self, DWORD effect)
{
  return DRAGDROP_S_USEDEFAULTCURSORS;
}
static IDropSourceVtbl g_dsrc_vtbl = {ds_query, ds_addref, ds_release, ds_continue, ds_feedback};

/* IDragSourceHelper (shobjidl.h), declared here: the shell draws the image */
typedef struct BuiShDragImage {
  SIZE sizeDragImage;
  POINT ptOffset;
  HBITMAP hbmpDragImage;
  COLORREF crColorKey;
} BuiShDragImage;
typedef struct BuiDragHelper BuiDragHelper;
typedef struct BuiDragHelperVtbl {
  HRESULT(STDMETHODCALLTYPE *QueryInterface)(BuiDragHelper *, REFIID, void **);
  ULONG(STDMETHODCALLTYPE *AddRef)(BuiDragHelper *);
  ULONG(STDMETHODCALLTYPE *Release)(BuiDragHelper *);
  HRESULT(STDMETHODCALLTYPE *InitializeFromBitmap)(BuiDragHelper *, BuiShDragImage *, IDataObject *);
  HRESULT(STDMETHODCALLTYPE *InitializeFromWindow)(BuiDragHelper *, HWND, POINT *, IDataObject *);
} BuiDragHelperVtbl;
struct BuiDragHelper {
  const BuiDragHelperVtbl *lpVtbl;
};
static const GUID g_clsid_drag_helper = {0x4657278a, 0x411b, 0x11d2, {0x83, 0x9a, 0x00, 0xc0, 0x4f, 0xd9, 0x18, 0xd0}};
static const GUID g_iid_drag_source_helper = {0xde5bf786, 0x477a, 0x11d2, {0x83, 0x9d, 0x00, 0xc0, 0x4f, 0xd9, 0x18, 0xd0}};

static void drag_image(IDataObject *data, const uint32_t *px, int w, int h, int hx, int hy)
{
  BuiDragHelper *helper = NULL;
  if (FAILED(CoCreateInstance(&g_clsid_drag_helper, NULL, CLSCTX_INPROC_SERVER,
                              &g_iid_drag_source_helper, (void **)&helper)) ||
      !helper)
  {
    return;
  }
  BITMAPINFO bi;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h; /* top-down, premultiplied alpha */
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void *bits = NULL;
  HBITMAP bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (bmp && bits) {
    memcpy(bits, px, (size_t)w * h * 4);
    BuiShDragImage img;
    img.sizeDragImage.cx = w;
    img.sizeDragImage.cy = h;
    img.ptOffset.x = hx;
    img.ptOffset.y = hy;
    img.hbmpDragImage = bmp;
    img.crColorKey = CLR_NONE;
    if (FAILED(helper->lpVtbl->InitializeFromBitmap(helper, &img, data))) {
      DeleteObject(bmp); /* the helper owns it when it succeeds */
    }
  }
  else if (bmp) {
    DeleteObject(bmp);
  }
  helper->lpVtbl->Release(helper);
}

int BK_window_drag_start(BKWindow *win, const BKClipItem *items, int n, const uint32_t *icon,
                         int icon_w, int icon_h, int hot_x, int hot_y, int actions,
                         BKDragDoneFn done, void *user)
{
  if (!win || !win->hwnd || !(GetKeyState(VK_LBUTTON) & 0x8000)) {
    return 0;
  }
  if (FAILED(OleInitialize(NULL))) { /* balanced below (counted per thread) */
    return 0;
  }
  BKClipSet *set = bk_clipset_new(items, n);
  DataObj *d = set ? (DataObj *)calloc(1, sizeof(DataObj)) : NULL;
  DropSrc *src = d ? (DropSrc *)calloc(1, sizeof(DropSrc)) : NULL;
  if (!src) {
    free(d);
    bk_clipset_free(set);
    OleUninitialize();
    return 0;
  }
  d->iface.lpVtbl = &g_dobj_vtbl;
  d->refs = 1;
  src->iface.lpVtbl = &g_dsrc_vtbl;
  src->refs = 1;
  for (int i = 0; i < set->n; i++) {
    UINT cf[4];
    HGLOBAL data[4];
    const int k = entry_formats(&set->e[i], cf, data, 2); /* PNG without DIBs */
    for (int j = 0; j < k; j++) {
      dobj_add(d, (CLIPFORMAT)cf[j], data[j]);
    }
  }
  bk_clipset_free(set);
  if (icon && icon_w > 0 && icon_h > 0) {
    drag_image(&d->iface, icon, icon_w, icon_h, hot_x, hot_y);
  }
  ReleaseCapture();
  BK_pointer_cancel(win->ctx);
  DWORD ok = 0;
  ok |= (actions & BK_DRAG_COPY) || !actions ? DROPEFFECT_COPY : 0;
  ok |= (actions & BK_DRAG_MOVE) ? DROPEFFECT_MOVE : 0;
  DWORD effect = DROPEFFECT_NONE;
  const HRESULT hr = DoDragDrop(&d->iface, &src->iface, ok, &effect);
  d->iface.lpVtbl->Release(&d->iface);
  src->iface.lpVtbl->Release(&src->iface);
  OleUninitialize();
  const int action = hr != DRAGDROP_S_DROP ? BK_DRAG_NONE :
                     (effect & DROPEFFECT_MOVE) ? BK_DRAG_MOVE :
                     (effect & DROPEFFECT_COPY) ? BK_DRAG_COPY : BK_DRAG_NONE;
  if (done) {
    done(user, action);
  }
  return 1;
}
#endif /* _WIN32 */
