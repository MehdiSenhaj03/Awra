/* Awra 2 windows: the root of a widget tree, a BUI kit context and a native
 * window (or an offscreen raster). The window paints the Canvas material
 * (frosted when the compositor blurs, opaque otherwise), lays its title bar
 * over its content, resizes from its edges (client-side decorations), keeps
 * the hover chain, keyboard focus and shortcuts, and paces animations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <ctype.h>
#include <stdio.h>

bool awra_anims_tick(void);
void awra_map_tree(AwraWidget *w, bool mapped);
AwraWidget *awra_header_new_default(AwraWidget *window);
void awra_header_window_state_changed(AwraWidget *header);

typedef struct Shortcut {
  unsigned long id;
  int mods;
  char keysym[32];
  AwraHandler fn;
  void *data;
  AwraDestroyNotify destroy;
  struct Shortcut *next;
} Shortcut;

#define MAX_PATH 64

typedef struct WinExt {
  BKContext *ctx;
  BKWindow *bkw;
  char *title;
  AwraWidget *titlebar, *child;
  bool hide_on_close, keep_rounded, resizable, presented, closed;
  int default_w, default_h, min_w, min_h;
  int state;
  int blur_state; /* -1 unknown, 0 no, 1 yes */
  AwraWidget *path[MAX_PATH];
  int npath;
  AwraWidget *focus;
  bool focus_moving; /* the focus is unmapped: it moves on idle */
  AwraWidget *pressed3; /* secondary-button press target */
  Shortcut *shortcuts;
  bool offscreen;
  BuiRaster *raster;
  int off_w, off_h;
  float scale;
  uint64_t frames, pixels;
  int blur_rect[5];
  bool overlay_titlebar;
  /* GTK's focus-visible: the window shows its focus ring after keyboard
   * use (a key that moved the focus or a cursor, Alt), hides it on a
   * pointer press and 3 s after the last key */
  bool focus_visible, key_tracked;
  AwraWidget *key_press_focus; /* compared only, not held */
  unsigned focus_visible_timer;
  /* passive floating widgets (awra_window_add_overlay) */
  AwraWidget *overlays[8];
  double ov_x[8], ov_y[8];
  int noverlays;
} WinExt;

#define WEXT(w) AWRA_EXT(w, WinExt)

static AwraWidget **g_windows;
static int g_nwindows;

static void set_focus_visible(AwraWidget *win, bool visible);

static void window_init(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  x->resizable = true;
  x->default_w = 900;
  x->default_h = 620;
  x->blur_state = -1;
  x->scale = 1.0f;
}

static void window_finalize(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  if (x->focus_visible_timer) {
    awra_source_remove(x->focus_visible_timer);
    x->focus_visible_timer = 0;
  }
  for (Shortcut *s = x->shortcuts; s;) {
    Shortcut *n = s->next;
    if (s->destroy) {
      s->destroy(s->data);
    }
    free(s);
    s = n;
  }
  if (x->bkw) {
    BK_window_free(x->bkw);
    x->bkw = NULL;
  }
  if (x->ctx) {
    x->ctx->root = NULL;
    w->ctx_root = NULL;
    BK_context_free(x->ctx);
    x->ctx = NULL;
  }
  if (x->raster) {
    BUI_raster_free(x->raster);
  }
  free(x->title);
}

/* Windows: DWM rounds the window and draws its shadow (awra.css's
 * window.awra-win32: border-radius 0); the window is drawn square. */
static bool on_win32(void)
{
  static int v = -1;
  if (v < 0) {
    const char *b = BK_window_backend();
    v = b && !strcmp(b, "win32");
  }
  return v != 0;
}

static bool squared(const AwraWidget *w)
{
  const WinExt *x = WEXT(w);
  if (on_win32() && !x->offscreen) {
    return true;
  }
  return !x->keep_rounded && (x->state & (AWRA_WINDOW_STATE_MAXIMIZED |
                                          AWRA_WINDOW_STATE_FULLSCREEN | AWRA_WINDOW_STATE_TILED));
}

static bool blur_active(const AwraWidget *w)
{
  const WinExt *x = WEXT(w);
  return x->blur_state == 1 && !x->offscreen && awra_native_blur_enabled() &&
         !awra_reduced_transparency() && !awra_tokens()->high_contrast;
}

bool awra_style_blur_active(void)
{
  return g_nwindows > 0 && blur_active(g_windows[0]);
}

static void window_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  WinExt *x = WEXT(w);
  int tmin = 0, tnat = 0, cmin = 0, cnat = 0;
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    if (x->titlebar) {
      awra_measure(x->titlebar, o, -1, &tmin, &tnat);
    }
    if (x->child) {
      awra_measure(x->child, o, -1, &cmin, &cnat);
    }
    *r_min = BK_MAX(tmin, cmin);
    *r_nat = BK_MAX(tnat, cnat);
  }
  else {
    if (x->titlebar) {
      awra_measure(x->titlebar, o, for_size, &tmin, &tnat);
    }
    if (x->child) {
      awra_measure(x->child, o, for_size, &cmin, &cnat);
    }
    if (x->overlay_titlebar) {
      *r_min = BK_MAX(tmin, cmin);
      *r_nat = BK_MAX(tnat, cnat);
    }
    else {
      *r_min = tmin + cmin;
      *r_nat = tnat + cnat;
    }
  }
}

static void place_overlay(AwraWidget *w, int i)
{
  WinExt *x = WEXT(w);
  AwraWidget *c = x->overlays[i];
  int mn, pw, ph;
  awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &pw);
  pw = BK_MIN(pw, w->w);
  awra_measure(c, AWRA_ORIENTATION_VERTICAL, pw, &mn, &ph);
  ph = BK_MIN(ph, w->h);
  const int m = awra_px(4);
  int px = w->x + awra_px(x->ov_x[i]), py = w->y + awra_px(x->ov_y[i]);
  px = BK_CLAMP(px, w->x + m, BK_MAX(w->x + m, w->x + w->w - pw - m));
  py = BK_CLAMP(py, w->y + m, BK_MAX(w->y + m, w->y + w->h - ph - m));
  awra_allocate(c, px, py, pw, ph);
}

static void window_allocate(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  int th = 0;
  if (x->titlebar && awra_child_visible(x->titlebar)) {
    int mn, nat;
    awra_measure(x->titlebar, AWRA_ORIENTATION_VERTICAL, w->w, &mn, &nat);
    th = mn;
    awra_allocate(x->titlebar, w->x, w->y, w->w, th);
  }
  if (x->overlay_titlebar) {
    th = 0; /* the title bar floats over the content */
  }
  if (x->child) {
    awra_allocate(x->child, w->x, w->y + th, w->w, BK_MAX(0, w->h - th));
  }
  for (int i = 0; i < x->noverlays; i++) {
    place_overlay(w, i);
  }
  /* the compositor blurs behind the window's rounded shape (on Windows:
   * DWM's rounding when the window is not maximized) */
  if (x->bkw) {
    const bool corners = on_win32() ? !(x->state & (AWRA_WINDOW_STATE_MAXIMIZED |
                                                    AWRA_WINDOW_STATE_FULLSCREEN)) :
                                      !squared(w);
    const int r = corners ? (int)awra_tokens()->radii[AWRA_RADIUS_WINDOW] : 0;
    const int lw = awra_widget_width(w), lh = awra_widget_height(w);
    const bool want = awra_native_blur_enabled() && !awra_reduced_transparency();
    const int rect[5] = {0, 0, want ? lw : 0, want ? lh : 0, r};
    if (memcmp(rect, x->blur_rect, sizeof(rect)) != 0) {
      memcpy(x->blur_rect, rect, sizeof(rect));
      BK_window_set_blur(x->bkw, rect[0], rect[1], rect[2], rect[3], rect[4]);
    }
  }
}

static void window_draw(AwraWidget *w, AwraDraw *d)
{
  AwraResolvedMaterial m;
  awra_material_resolve(AWRA_MATERIAL_CANVAS, AWRA_ROLE_WINDOW, AWRA_ELEVATION_FLAT,
                        blur_active(w), &m);
  const bool sq = squared(w);
  awra_paint_material(d, awra_box_of(w), &m, sq ? 0.0f : awra_pxf(m.radius), 15,
                      sq ? 0 : AWRA_EDGE_ALL, AWRA_ELEVATION_FLAT);
}

/* Client-side resize: the 6 px band inside the window's edge (corners 16). */
static int edge_at(AwraWidget *w, int px, int py)
{
  WinExt *x = WEXT(w);
  /* Windows resizes through its native border band (WM_NCHITTEST) */
  if (!x->bkw || !x->resizable || squared(w) || on_win32()) {
    return 0;
  }
  const int band = awra_px(6), corner = awra_px(16);
  int e = 0;
  const int l = px - w->x, t = py - w->y, r = w->x + w->w - 1 - px, b = w->y + w->h - 1 - py;
  if (l < band) {
    e |= BK_EDGE_LEFT;
  }
  if (r < band) {
    e |= BK_EDGE_RIGHT;
  }
  if (t < band) {
    e |= BK_EDGE_TOP;
  }
  if (b < band) {
    e |= BK_EDGE_BOTTOM;
  }
  if (e & (BK_EDGE_LEFT | BK_EDGE_RIGHT)) {
    if (t < corner) {
      e |= BK_EDGE_TOP;
    }
    if (b < corner) {
      e |= BK_EDGE_BOTTOM;
    }
  }
  if (e & (BK_EDGE_TOP | BK_EDGE_BOTTOM)) {
    if (l < corner) {
      e |= BK_EDGE_LEFT;
    }
    if (r < corner) {
      e |= BK_EDGE_RIGHT;
    }
  }
  return e;
}

static BKNode *window_hit(BKNode *n, int x, int y)
{
  if (edge_at(n, x, y)) {
    return n;
  }
  return awra_bk_hit(n, x, y);
}

static int window_cursor(AwraWidget *w, int x, int y)
{
  const int e = edge_at(w, x, y);
  if (!e) {
    return -1;
  }
  if ((e & BK_EDGE_TOP && e & BK_EDGE_LEFT) || (e & BK_EDGE_BOTTOM && e & BK_EDGE_RIGHT)) {
    return BK_CURSOR_NWSE;
  }
  if ((e & BK_EDGE_TOP && e & BK_EDGE_RIGHT) || (e & BK_EDGE_BOTTOM && e & BK_EDGE_LEFT)) {
    return BK_CURSOR_NESW;
  }
  return (e & (BK_EDGE_LEFT | BK_EDGE_RIGHT)) ? BK_CURSOR_EW : BK_CURSOR_NS;
}

static int window_press(AwraWidget *w, BKEvent *e)
{
  const int edges = edge_at(w, e->x, e->y);
  if (edges) {
    BK_window_begin_resize(WEXT(w)->bkw, edges);
    return 1;
  }
  return 0;
}

static int window_key(AwraWidget *w, BKEvent *e);

static AwraClass window_class = {
    .bk = AWRA_BK("AwraWindow", 0),
    .type = "AwraWindow",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(WinExt),
    .init = window_init,
    .finalize = window_finalize,
    .measure = window_measure,
    .allocate = window_allocate,
    .draw = window_draw,
    .press = window_press,
    .cursor = window_cursor,
    .key = window_key,
};

/* ----------------------------------------------------------------------- */
/* Hover chain and secondary buttons (the kit's pointer hook) */

static AwraWidget *window_of_ctx(BKContext *ctx)
{
  return (AwraWidget *)ctx->user;
}

static int build_path(BKContext *ctx, int x, int y, AwraWidget **out)
{
  BKNode *top = NULL;
  for (int i = ctx->npopups - 1; i >= 0 && !top; i--) {
    BKNode *p = ctx->popups[i];
    if ((p->flags & NF_VISIBLE) && bk_contains(p, x, y)) {
      top = p;
    }
  }
  if (!top) {
    top = ctx->root;
  }
  int n = 0;
  BKNode *cur = top;
  while (cur && n < MAX_PATH) {
    if (!(cur->flags & NF_VISIBLE) || !bk_contains(cur, x, y) ||
        (awra_is_widget(cur) && !awra_aux(cur)->can_target))
    {
      break;
    }
    out[n++] = cur;
    BKNode *next = NULL;
    for (int i = cur->nkids - 1; i >= 0; i--) {
      BKNode *k = cur->kids[i];
      if ((k->flags & NF_VISIBLE) && bk_contains(k, x, y) &&
          !(awra_is_widget(k) && !awra_aux(k)->can_target))
      {
        next = k;
        break;
      }
    }
    cur = next;
  }
  return n;
}

static void set_hover(AwraWidget *n, bool on)
{
  if (!awra_is_widget(n)) {
    return;
  }
  AwraAux *a = awra_aux(n);
  if ((bool)a->hovered == on) {
    return;
  }
  a->hovered = on;
  if (AWRA_CLASS_OF(n)->flags & AWRA_CLASS_HOVER) {
    awra_animate_state(n);
  }
  if (AWRA_CLASS_OF(n)->hover) {
    AWRA_CLASS_OF(n)->hover(n, on);
  }
  static int entry_bit = -2;
  if (entry_bit == -2) {
    entry_bit = awra_class_bit("conversation-entry");
  }
  if (awra_has_class_bit(n, entry_bit)) {
    awra_invalidate(n);
  }
  awra_emit_simple(n, on ? AWRA_SIGNAL_ENTER : AWRA_SIGNAL_LEAVE);
}

static void update_hover(AwraWidget *win, int x, int y, bool leave)
{
  WinExt *wx = WEXT(win);
  AwraWidget *np[MAX_PATH];
  const int nn = leave ? 0 : build_path(wx->ctx, x, y, np);
  /* leave what is no longer under the pointer (deepest first) */
  for (int i = wx->npath - 1; i >= 0; i--) {
    bool still = false;
    for (int k = 0; k < nn && !still; k++) {
      still = np[k] == wx->path[i];
    }
    if (!still) {
      set_hover(wx->path[i], false);
      awra_widget_unref(wx->path[i]);
    }
  }
  for (int k = 0; k < nn; k++) {
    bool was = false;
    for (int i = 0; i < wx->npath && !was; i++) {
      was = wx->path[i] == np[k];
    }
    if (!was) {
      awra_widget_ref(np[k]);
      set_hover(np[k], true);
    }
  }
  memcpy(wx->path, np, sizeof(AwraWidget *) * (size_t)nn);
  wx->npath = nn;
  /* motion to the widgets that listen, deepest first */
  if (!leave) {
    for (int k = nn - 1; k >= 0; k--) {
      AwraWidget *n = np[k];
      if (awra_has_handlers(n, AWRA_SIGNAL_MOTION) && !(wx->ctx->grab == n)) {
        AwraArgs a = {0};
        a.signal = AWRA_SIGNAL_MOTION;
        a.x = (x - n->x) / awra_scale();
        a.y = (y - n->y) / awra_scale();
        awra_emit(n, &a);
      }
    }
  }
}

static void pointer_hook(BKContext *ctx, int kind, int x, int y, int mods)
{
  AwraWidget *win = window_of_ctx(ctx);
  if (!win) {
    return;
  }
  if (kind == BK_EV_MOTION || kind == BK_EV_LEAVE) {
    awra_drag_icon_pointer(win, x, y, kind == BK_EV_LEAVE); /* transfer.c */
    update_hover(win, x, y, kind == BK_EV_LEAVE);
    return;
  }
  if (kind == BK_EV_PRESS && awra_has_handlers(win, AWRA_SIGNAL_PRESS_CAPTURE)) {
    AwraArgs a = {0};
    a.signal = AWRA_SIGNAL_PRESS_CAPTURE;
    a.x = x / awra_scale();
    a.y = y / awra_scale();
    a.button = (mods & BK_BUTTON3) ? 3 : (mods & BK_BUTTON2) ? 2 : 1;
    a.n_press = 1;
    a.mods = mods & (BK_SHIFT | BK_CTRL | BK_ALT);
    awra_emit(win, &a);
  }
  if (kind == BK_EV_PRESS && !(mods & (BK_BUTTON2 | BK_BUTTON3))) {
    update_hover(win, x, y, false);
    /* a press on nothing focusable keeps the focus (GTK), but hides its
     * ring: rings show on keyboard use only */
    set_focus_visible(win, false);
    return;
  }
  if (!(mods & (BK_BUTTON2 | BK_BUTTON3))) {
    return;
  }
  const int button = (mods & BK_BUTTON3) ? 3 : 2;
  AwraWidget *path[MAX_PATH];
  const int n = build_path(ctx, x, y, path);
  if (kind == BK_EV_PRESS && ctx->npopups && button == 3) {
    bool inside = false;
    for (int i = 0; i < ctx->npopups; i++) {
      inside |= bk_contains(ctx->popups[i], x, y);
    }
    if (!inside) {
      BK_close_popups(ctx);
    }
  }
  for (int k = n - 1; k >= 0; k--) {
    AwraWidget *t = path[k];
    const AwraSignal sig = kind == BK_EV_PRESS ? AWRA_SIGNAL_PRESSED : AWRA_SIGNAL_RELEASED;
    if (awra_is_widget(t) && awra_sensitive(t) && awra_has_handlers(t, sig)) {
      AwraArgs a = {0};
      a.signal = sig;
      a.x = (x - t->x) / awra_scale();
      a.y = (y - t->y) / awra_scale();
      a.button = button;
      a.n_press = 1;
      a.mods = mods & (BK_SHIFT | BK_CTRL | BK_ALT);
      if (awra_emit(t, &a)) {
        break;
      }
    }
  }
}

/* ----------------------------------------------------------------------- */
/* File drops */

bool awra_keyboard_view_drop(AwraWidget *v, int kind, int px, int py, const char *paths);

static AwraWidget *drop_target_at(BKContext *ctx, int x, int y)
{
  AwraWidget *path[MAX_PATH];
  const int n = build_path(ctx, x, y, path);
  for (int k = n - 1; k >= 0; k--) {
    if (awra_widget_get_data(path[k], "awra-drops") && awra_sensitive(path[k])) {
      return path[k];
    }
  }
  return NULL;
}

/* file:// URIs to paths ('\n'-separated), percent-decoded */
static char *uris_to_paths(const char *uris)
{
  const size_t n = strlen(uris);
  char *out = (char *)malloc(n + 2);
  size_t o = 0;
  for (const char *p = uris; *p;) {
    const char *e = p;
    while (*e && *e != '\r' && *e != '\n') {
      e++;
    }
    if (e > p && *p != '#') {
      const char *s = p;
      if (!strncmp(s, "file://", 7)) {
        s += 7;
        if (*s != '/') { /* file://host/path */
          s = strchr(s, '/') ? strchr(s, '/') : s;
        }
      }
#ifdef _WIN32
      if (s[0] == '/' && s[2] == ':') {
        s++;
      }
#endif
      if (o) {
        out[o++] = '\n';
      }
      for (; s < e; s++) {
        if (*s == '%' && s + 2 < e && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
          char hex[3] = {s[1], s[2], 0};
          out[o++] = (char)strtol(hex, NULL, 16);
          s += 2;
        }
        else {
          out[o++] = *s;
        }
      }
    }
    p = e;
    while (*p == '\r' || *p == '\n') {
      p++;
    }
  }
  out[o] = 0;
  return out;
}

/* drop-hover and files-dropped carry the pointer, relative to the target */
static void emit_drop(AwraWidget *t, AwraSignal sig, bool active, int x, int y, const char *text)
{
  AwraArgs a = {0};
  a.signal = sig;
  a.active = active;
  a.text = text;
  a.x = (x - t->x) / awra_scale();
  a.y = (y - t->y) / awra_scale();
  awra_emit(t, &a);
}

static int drop_hook(BKContext *ctx, int kind, int x, int y, const char *uris)
{
  AwraWidget *win = window_of_ctx(ctx);
  if (!win) {
    return 0;
  }
  /* the window hears that files came over it, and that they left or landed */
  const bool inside = kind != BK_DROP_LEAVE && kind != BK_DROP_DROP;
  if (inside != (awra_widget_get_data(win, "awra-drag-inside") != NULL) && !awra_widget_get_data(win, "awra-drops")) {
    awra_widget_set_data(win, "awra-drag-inside", inside ? (void *)1 : NULL, NULL);
    emit_drop(win, AWRA_SIGNAL_DROP_HOVER, inside, x, y, NULL);
  }
  AwraWidget *t = kind == BK_DROP_LEAVE ? NULL : drop_target_at(ctx, x, y);
  AwraWidget *old = (AwraWidget *)awra_widget_get_data(win, "awra-drop-target");
  if (old != t) {
    if (old) {
      if (awra_widget_is(old, "AwraKeyboardView")) {
        awra_keyboard_view_drop(old, BK_DROP_LEAVE, x, y, NULL);
      }
      emit_drop(old, AWRA_SIGNAL_DROP_HOVER, false, x, y, NULL);
    }
    awra_widget_set_data(win, "awra-drop-target", t, NULL);
  }
  if (!t) {
    return 0;
  }
  if (awra_widget_is(t, "AwraKeyboardView")) {
    if (old != t) {
      awra_emit_active(t, AWRA_SIGNAL_DROP_HOVER, true);
    }
    char *paths = kind == BK_DROP_DROP && uris ? uris_to_paths(uris) : NULL;
    const bool ok = awra_keyboard_view_drop(t, kind, x, y, paths ? paths : "");
    free(paths);
    if (kind == BK_DROP_DROP) {
      awra_widget_set_data(win, "awra-drop-target", NULL, NULL);
      awra_emit_active(t, AWRA_SIGNAL_DROP_HOVER, false);
    }
    return ok;
  }
  if (kind == BK_DROP_DROP && uris) {
    char *paths = uris_to_paths(uris);
    awra_widget_set_data(win, "awra-drop-target", NULL, NULL);
    emit_drop(t, AWRA_SIGNAL_DROP_HOVER, false, x, y, NULL);
    emit_drop(t, AWRA_SIGNAL_FILES_DROPPED, false, x, y, paths);
    free(paths);
  }
  else {
    /* every move: a widget with places inside (rows, tiles) follows it */
    emit_drop(t, AWRA_SIGNAL_DROP_HOVER, true, x, y, NULL);
  }
  return 1;
}

void awra_widget_set_accepts_file_drops(AwraWidget *w, bool accepts)
{
  awra_widget_set_data(w, "awra-drops", accepts ? (void *)1 : NULL, NULL);
}

/* ----------------------------------------------------------------------- */
/* Keyboard: focus chain, shortcuts, Tab */

static bool focusable(const AwraWidget *n)
{
  if (!awra_is_widget(n) || !awra_widget_mapped(n) || !awra_sensitive(n)) {
    return false;
  }
  const AwraAux *a = awra_aux(n);
  if (a->focusable_set) {
    return a->focusable;
  }
  return (AWRA_CLASS_OF(n)->flags & (AWRA_CLASS_FOCUSABLE | AWRA_CLASS_TEXT_INPUT)) != 0;
}

static void collect_focusable(AwraWidget *n, AwraWidget **out, int *count, int max)
{
  static int leaving = -2;
  if (leaving == -2) {
    leaving = awra_class_bit("stack-leaving");
  }
  if (!(n->flags & NF_VISIBLE) || *count >= max ||
      (leaving >= 0 && awra_is_widget(n) && awra_has_class_bit(n, leaving)))
  {
    return;
  }
  if (focusable(n)) {
    out[(*count)++] = n;
  }
  for (int i = 0; i < n->nkids; i++) {
    collect_focusable(n->kids[i], out, count, max);
  }
}

AwraWidget *awra_window_focus(AwraWidget *win)
{
  return win && awra_widget_is(win, "AwraWindow") ? WEXT(win)->focus : NULL;
}

void awra_window_set_focus(AwraWidget *win, AwraWidget *w)
{
  if (win && awra_widget_is(win, "AwraWindow")) {
    awra_set_focus(win, w, false);
  }
}

AwraWidget *awra_focus_of(AwraWidget *win)
{
  return win ? WEXT(win)->focus : NULL;
}

static bool focus_visible_expired(void *data)
{
  AwraWidget *win = (AwraWidget *)data;
  WEXT(win)->focus_visible_timer = 0;
  set_focus_visible(win, false);
  return false;
}

/* gtk_window_set_focus_visible: the focus widget's ring follows; a timer
 * hides it 3 s later (re-armed by each keyboard use) */
static void set_focus_visible(AwraWidget *win, bool visible)
{
  WinExt *x = WEXT(win);
  if (x->focus_visible_timer) {
    awra_source_remove(x->focus_visible_timer);
    x->focus_visible_timer = 0;
  }
  if (visible && !x->offscreen) {
    x->focus_visible_timer = awra_timeout_add(3000, focus_visible_expired, win, NULL);
  }
  x->focus_visible = visible;
  AwraWidget *f = x->focus;
  if (f && awra_is_widget(f) && (bool)awra_aux(f)->focus_visible != visible) {
    awra_aux(f)->focus_visible = visible;
    awra_invalidate(f);
  }
}

/* a widget moved its own cursor from the keyboard (lists, tabs) */
void awra_window_keyboard_moved(AwraWidget *w);
void awra_window_keyboard_moved(AwraWidget *w)
{
  AwraWidget *win = awra_window_of(w);
  if (win) {
    set_focus_visible(win, true);
  }
}

void awra_set_focus(AwraWidget *win, AwraWidget *w, bool keyboard)
{
  WinExt *x = WEXT(win);
  if (x->focus == w) {
    if (keyboard) {
      set_focus_visible(win, true);
    }
    return;
  }
  AwraWidget *old = x->focus;
  x->focus = w;
  if (old) {
    awra_aux(old)->focus_visible = 0;
    awra_invalidate(old);
    if (AWRA_CLASS_OF(old)->bk.finish_edit) {
      AWRA_CLASS_OF(old)->bk.finish_edit(old, true);
    }
    awra_emit_text(old, AWRA_SIGNAL_NOTIFY, "has-focus");
    awra_widget_unref(old);
  }
  if (w) {
    awra_widget_ref(w);
    awra_aux(w)->focus_visible = x->focus_visible;
    awra_invalidate(w);
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "has-focus");
  }
  if (keyboard) {
    set_focus_visible(win, true);
  }
  awra_emit_text(win, AWRA_SIGNAL_NOTIFY, "focus");
}

static bool is_modifier(const char *keysym)
{
  static const char *const mods[] = {"Shift_L", "Shift_R", "Control_L", "Control_R", "Alt_L",
                                     "Alt_R", "Super_L", "Super_R", "Meta_L", "Meta_R",
                                     "ISO_Level3_Shift", "Caps_Lock", "Num_Lock", "Hyper_L",
                                     "Hyper_R"};
  for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
    if (bk_keysym_is(keysym, mods[i])) {
      return true;
    }
  }
  return false;
}

static bool is_navigation(const char *keysym)
{
  static const char *const keys[] = {"Up", "Down", "Left", "Right", "Home", "End", "Page_Up",
                                     "Page_Down", "KP_Up", "KP_Down", "KP_Left", "KP_Right",
                                     "Tab", "ISO_Left_Tab"};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    if (bk_keysym_is(keysym, keys[i])) {
      return true;
    }
  }
  return false;
}

/* _gtk_window_update_focus_visible: a key press remembers the focus; its
 * release shows the ring if the focus moved (or it showed already); Alt
 * alone shows it */
static void key_focus_visible(AwraWidget *win, bool press, const char *keysym, int mods)
{
  WinExt *x = WEXT(win);
  if (press) {
    x->key_tracked = !x->focus_visible;
    x->key_press_focus = x->focus;
    if ((bk_keysym_is(keysym, "Alt_L") || bk_keysym_is(keysym, "Alt_R")) &&
        !(mods & (BK_CTRL | BK_SHIFT)))
    {
      set_focus_visible(win, true);
    }
    return;
  }
  if (is_modifier(keysym) && !x->focus_visible) {
    return;
  }
  if (!x->key_tracked || x->focus != x->key_press_focus) {
    set_focus_visible(win, true);
  }
  x->key_tracked = false;
  x->key_press_focus = NULL;
}

static void move_focus(AwraWidget *win, int dir)
{
  WinExt *x = WEXT(win);
  AwraWidget *list[512];
  int n = 0;
  /* inside the top popup when one is open */
  AwraWidget *scope = x->ctx->npopups ? x->ctx->popups[x->ctx->npopups - 1] : win;
  collect_focusable(scope, list, &n, 512);
  if (!n) {
    return;
  }
  int cur = -1;
  for (int i = 0; i < n; i++) {
    if (list[i] == x->focus) {
      cur = i;
    }
  }
  const int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
  awra_set_focus(win, list[next], true);
}

/* The first focusable widget of `n`'s subtree, in tab order. */
static AwraWidget *first_focusable(AwraWidget *n)
{
  AwraWidget *list[512];
  int count = 0;
  if (n) {
    collect_focusable(n, list, &count, 512);
  }
  return count ? list[0] : NULL;
}

static void focus_move_unref(void *data)
{
  awra_widget_unref((AwraWidget *)data);
}

/* GtkWindow's move_focus: the focused widget went away; the focus goes to
 * its nearest focusable ancestor, else to the first focusable widget of the
 * content, then of the titlebar, keeping whether it shows as keyboard
 * focus. */
static bool focus_move(void *data)
{
  AwraWidget *win = (AwraWidget *)data;
  WinExt *x = WEXT(win);
  x->focus_moving = false;
  AwraWidget *old = x->focus;
  if (x->closed || !old || awra_widget_mapped(old)) {
    return false;
  }
  if (awra_window_of(old) != win) {
    /* taken out of the window (gtk_widget_unparent): no focus */
    awra_set_focus(win, NULL, false);
    return false;
  }
  AwraWidget *next = NULL;
  for (AwraWidget *p = old->parent; p && !next; p = p->parent) {
    if (awra_is_widget(p) && focusable(p)) {
      next = p;
    }
  }
  if (!next) {
    next = first_focusable(x->child);
  }
  if (!next) {
    next = first_focusable(x->titlebar);
  }
  awra_set_focus(win, next, false); /* shown as the window shows its focus */
  return false;
}

void awra_window_focus_unmapped(AwraWidget *w); /* core.c */
void awra_window_focus_unmapped(AwraWidget *w)
{
  for (int i = 0; i < g_nwindows; i++) {
    AwraWidget *win = g_windows[i];
    WinExt *x = WEXT(win);
    if (x->focus == w && !x->focus_moving && !x->closed) {
      x->focus_moving = true;
      awra_widget_ref(win);
      awra_idle_add(focus_move, win, focus_move_unref);
    }
  }
}

static int parse_accel(const char *accel, char *keysym, size_t size)
{
  int mods = 0;
  const char *p = accel;
  while (*p == '<') {
    const char *e = strchr(p, '>');
    if (!e) {
      break;
    }
    char m[16];
    snprintf(m, sizeof(m), "%.*s", (int)(e - p - 1), p + 1);
    for (char *c = m; *c; c++) {
      *c = (char)tolower((unsigned char)*c);
    }
    if (!strcmp(m, "ctrl") || !strcmp(m, "control") || !strcmp(m, "primary")) {
      mods |= BK_CTRL;
    }
    else if (!strcmp(m, "shift")) {
      mods |= BK_SHIFT;
    }
    else if (!strcmp(m, "alt")) {
      mods |= BK_ALT;
    }
    p = e + 1;
  }
  snprintf(keysym, size, "%s", p);
  return mods;
}

unsigned long awra_window_add_shortcut(AwraWidget *win, const char *accel, AwraHandler fn,
                                       void *data, AwraDestroyNotify destroy)
{
  static unsigned long next = 1;
  if (!awra_widget_is(win, "AwraWindow") || !accel || !fn) {
    return 0;
  }
  Shortcut *s = (Shortcut *)calloc(1, sizeof(Shortcut));
  if (!s) {
    return 0;
  }
  s->id = next++;
  s->mods = parse_accel(accel, s->keysym, sizeof(s->keysym));
  s->fn = fn;
  s->data = data;
  s->destroy = destroy;
  s->next = WEXT(win)->shortcuts;
  WEXT(win)->shortcuts = s;
  return s->id;
}

void awra_window_remove_shortcut(AwraWidget *win, unsigned long id)
{
  for (Shortcut **p = &WEXT(win)->shortcuts; *p; p = &(*p)->next) {
    if ((*p)->id == id) {
      Shortcut *s = *p;
      *p = s->next;
      if (s->destroy) {
        s->destroy(s->data);
      }
      free(s);
      return;
    }
  }
}

/* Whether a keysym is a printable character (a letter, or punctuation by
 * its X name: "question", "plus"...): Shift may have been what made it. */
static bool printable_keysym(const char *k)
{
  static const char *const names[] = {
      "space",     "exclam",     "quotedbl",    "numbersign", "dollar",     "percent",
      "ampersand", "apostrophe", "parenleft",   "parenright", "asterisk",   "plus",
      "comma",     "minus",      "period",      "slash",      "colon",      "semicolon",
      "less",      "equal",      "greater",     "question",   "at",         "bracketleft",
      "backslash", "bracketright", "asciicircum", "underscore", "grave",    "braceleft",
      "bar",       "braceright", "asciitilde",  "section",    "degree",     "sterling",
      "mu",        "diaeresis",  NULL};
  if (k[0] && !k[1]) {
    return true;
  }
  for (int i = 0; names[i]; i++) {
    if (!strcmp(k, names[i])) {
      return true;
    }
  }
  return false;
}

bool awra_window_dispatch_shortcut(AwraWidget *win, const char *keysym, int mods)
{
  const int m = mods & (BK_SHIFT | BK_CTRL | BK_ALT);
  /* an exact match first (Ctrl+Shift+B is <Primary><Shift>b, not
   * <Primary>b), then one where Shift only made the character (GTK's
   * GDK_KEY_MATCH_EXACT before _PARTIAL) */
  Shortcut *found = NULL;
  for (Shortcut *s = WEXT(win)->shortcuts; s && !found; s = s->next) {
    if (s->mods == m && bk_keysym_is(keysym, s->keysym)) {
      found = s;
    }
  }
  for (Shortcut *s = WEXT(win)->shortcuts; s && !found; s = s->next) {
    const bool shift_free = !(s->mods & BK_SHIFT) && printable_keysym(keysym);
    if (shift_free && (s->mods | BK_SHIFT) == m && bk_keysym_is(keysym, s->keysym)) {
      found = s;
    }
  }
  if (!found) {
    return false;
  }
  AwraArgs a = {0};
  a.signal = AWRA_SIGNAL_ACTIVATE;
  a.keysym = keysym;
  a.mods = m;
  bool handled = true;
  a.handled = &handled;
  found->fn(win, &a, found->data);
  return true;
}

static int key_hook(BKContext *ctx, int press, const char *text, const char *keysym, int mods)
{
  AwraWidget *win = window_of_ctx(ctx);
  if (!win) {
    return 0;
  }
  WinExt *x = WEXT(win);
  /* a focus the key press took away moves before its release is weighed
   * (key_focus_visible: a key that moved the focus shows it) */
  if (!press && x->focus_moving) {
    focus_move(win); /* its idle is then a no-op */
  }
  AwraWidget *start = x->focus;
  /* a focus inside a closed popup (or gone) no longer counts */
  if (start && !awra_widget_mapped(start)) {
    start = NULL;
  }
  BKEvent e;
  memset(&e, 0, sizeof(e));
  e.text = text;
  e.keysym = keysym;
  e.mods = mods;
  e.x = ctx->pointer_x;
  e.y = ctx->pointer_y;
  awra_widget_ref(win);
  key_focus_visible(win, press != 0, keysym, mods);
  int handled = 0;
  const AwraSignal sig = press ? AWRA_SIGNAL_KEY_PRESSED : AWRA_SIGNAL_KEY_RELEASED;
  const char *code = awra_key_code_name(BK_last_key_code());
  if (press && awra_has_handlers(win, AWRA_SIGNAL_KEY_CAPTURE)) {
    AwraArgs a = {0};
    a.signal = AWRA_SIGNAL_KEY_CAPTURE;
    a.keysym = keysym;
    a.text = text;
    a.code = code;
    a.mods = mods & (BK_SHIFT | BK_CTRL | BK_ALT);
    handled = awra_emit(win, &a);
    /* the handler may have moved the focus */
    start = x->focus && awra_widget_mapped(x->focus) ? x->focus : NULL;
  }
  for (AwraWidget *n = start ? start : win; n && !handled; n = n->parent) {
    if (!awra_is_widget(n)) {
      continue;
    }
    if (awra_has_handlers(n, sig)) {
      AwraArgs a = {0};
      a.signal = sig;
      a.keysym = keysym;
      a.text = text;
      a.code = code;
      a.mods = mods & (BK_SHIFT | BK_CTRL | BK_ALT);
      if (awra_emit(n, &a)) {
        handled = 1;
        break;
      }
    }
    if (press && n != win && AWRA_CLASS_OF(n)->key && AWRA_CLASS_OF(n)->key(n, &e)) {
      handled = 1;
      if (is_navigation(keysym)) {
        set_focus_visible(win, true); /* a cursor moved inside the widget */
      }
    }
  }
  if (!handled && press) {
    /* popups get Escape and their own keys before the window's shortcuts */
    if (ctx->npopups && bk_keysym_is(keysym, "Escape")) {
      handled = 0; /* the kit closes the top popup */
    }
    else if (awra_window_dispatch_shortcut(win, keysym, mods)) {
      handled = 1;
    }
    else if (bk_keysym_is(keysym, "Tab") || bk_keysym_is(keysym, "ISO_Left_Tab")) {
      move_focus(win, (mods & BK_SHIFT) || bk_keysym_is(keysym, "ISO_Left_Tab") ? -1 : 1);
      handled = 1;
    }
    else if (start && (bk_keysym_is(keysym, "Return") || bk_keysym_is(keysym, "KP_Enter") ||
                       bk_keysym_is(keysym, "space")) &&
             (AWRA_CLASS_OF(start)->flags & AWRA_CLASS_ACTIVATABLE) &&
             AWRA_CLASS_OF(start)->activate)
    {
      AWRA_CLASS_OF(start)->activate(start);
      handled = 1;
    }
  }
  awra_widget_unref(win);
  return handled;
}

static int window_key(AwraWidget *w, BKEvent *e)
{
  (void)w;
  (void)e;
  return 0;
}

/* ----------------------------------------------------------------------- */
/* Native window callbacks */

static void window_destroy(AwraWidget *w);

static int on_close(void *data, BKWindow *bkw)
{
  AwraWidget *w = (AwraWidget *)data;
  WinExt *x = WEXT(w);
  bool refuse = false;
  AwraArgs a = {0};
  a.signal = AWRA_SIGNAL_CLOSE_REQUEST;
  a.handled = &refuse;
  awra_emit(w, &a);
  if (refuse) {
    return 0;
  }
  if (x->hide_on_close) {
    awra_window_hide(w);
    return 0;
  }
  window_destroy(w);
  return 0; /* closed it already */
}

static void on_frame(void *data, BKWindow *bkw)
{
  if (awra_anims_tick()) {
    awra_windows_request_frame();
  }
}

static void on_state(void *data, BKWindow *bkw, int state)
{
  AwraWidget *w = (AwraWidget *)data;
  WinExt *x = WEXT(w);
  const int avail = BK_window_blur_available(bkw);
  const int old_state = x->state, old_blur = x->blur_state;
  x->state = state & (AWRA_WINDOW_STATE_MAXIMIZED | AWRA_WINDOW_STATE_FULLSCREEN |
                      AWRA_WINDOW_STATE_ACTIVE | AWRA_WINDOW_STATE_TILED |
                      AWRA_WINDOW_STATE_HIDDEN);
  if (avail == 1) {
    x->state |= AWRA_WINDOW_STATE_BLUR;
  }
  x->blur_state = avail;
  const float s = BK_window_scale(bkw);
  if (s != x->scale) {
    x->scale = s;
    awra_window_style_changed_all(); /* every measurement is in raster px */
  }
  if ((old_state ^ x->state) & AWRA_WINDOW_STATE_ACTIVE) {
    awra_a11y_window_changed(w, (x->state & AWRA_WINDOW_STATE_ACTIVE) ? AWRA_A11Y_WINDOW_ACTIVATE
                                                                       : AWRA_A11Y_WINDOW_DEACTIVATE);
  }
  if (old_state != x->state || old_blur != x->blur_state) {
    awra_aux(w)->needs_allocate = 1;
    memset(x->blur_rect, 0, sizeof(x->blur_rect));
    awra_widget_queue_resize(w);
    awra_invalidate(w);
    if (x->titlebar) {
      awra_header_window_state_changed(x->titlebar);
    }
    awra_emit_index(w, AWRA_SIGNAL_STATE, x->state);
  }
}

/* ----------------------------------------------------------------------- */
/* Public API */

static AwraWidget *window_create(const char *title, int width, int height, bool offscreen,
                                 double scale)
{
  window_class.bk.hit = window_hit;
  AwraWidget *w = awra_widget_new(&window_class);
  if (!w) {
    return NULL;
  }
  WinExt *x = WEXT(w);
  x->title = awra_strdup(title ? title : "");
  if (width > 0) {
    x->default_w = width;
  }
  if (height > 0) {
    x->default_h = height;
  }
  x->offscreen = offscreen;
  x->ctx = BK_context_new(w);
  x->ctx->root_fills = true;
  x->ctx->transparent = true;
  x->ctx->user = w;
  x->ctx->pointer_hook = pointer_hook;
  x->ctx->key_hook = key_hook;
  x->ctx->drop_hook = drop_hook;
  if (offscreen) {
    x->scale = (float)(scale > 0 ? scale : 1.0);
    BK_set_scale(x->scale);
    x->off_w = (int)(x->default_w * x->scale + 0.5);
    x->off_h = (int)(x->default_h * x->scale + 0.5);
    x->raster = BUI_raster_create(x->off_w, x->off_h);
    BUI_raster_set_diff(x->raster, 0);
    x->blur_state = 0;
  }
  /* the window list keeps toplevels alive, as GTK does */
  g_windows = (AwraWidget **)realloc(g_windows, sizeof(AwraWidget *) * (size_t)(g_nwindows + 1));
  g_windows[g_nwindows++] = w;
  AwraWidget *header = awra_header_new_default(w);
  awra_window_set_titlebar(w, header);
  awra_widget_unref(header); /* the window holds it */
  return w;
}

BKWindow *awra_window_bkw(AwraWidget *w)
{
  return awra_widget_is(w, "AwraWindow") ? WEXT(w)->bkw : NULL;
}

AwraWidget *awra_window_new(const char *title, int width, int height)
{
  return window_create(title, width, height, false, 1.0);
}

AwraWidget *awra_window_new_offscreen(const char *title, int width, int height, double scale)
{
  return window_create(title, width, height, true, scale);
}

static void forget_measures(AwraWidget *n)
{
  if (awra_is_widget(n)) {
    AwraAux *a = awra_aux(n);
    a->cache_valid = 0;
    a->expand_valid = 0;
    a->mode_valid = 0;
    a->needs_allocate = 1;
  }
  for (int i = 0; i < n->nkids; i++) {
    forget_measures(n->kids[i]);
  }
}

static void ensure_native(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  if (x->bkw || x->offscreen || x->closed) {
    return;
  }
  /* GtkWindow: the default size, but never under what the contents need
   * (measured at the current scale: the window's own may differ, so the
   * measures are forgotten once it is known) */
  const bool measured = x->child || x->titlebar;
  if (measured) {
    const float s = awra_scale();
    int mn = 0, nat = 0;
    window_measure(w, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nat);
    x->default_w = BK_MAX(x->default_w, (int)ceilf(mn / s));
    window_measure(w, AWRA_ORIENTATION_VERTICAL, (int)(x->default_w * s), &mn, &nat);
    x->default_h = BK_MAX(x->default_h, (int)ceilf(mn / s));
  }
  x->bkw = BK_window_new_ex(x->ctx, x->title, x->default_w, x->default_h,
                            BK_WINDOW_TRANSPARENT | BK_WINDOW_UNDECORATED);
  if (!x->bkw) {
    fprintf(stderr, "awra: no native window (no display?)\n");
    return;
  }
  BK_window_set_app_id(x->bkw, awra_app_id());
  BKWindowCallbacks cb = {w, NULL, on_close, on_frame, on_state};
  BK_window_set_callbacks(x->bkw, &cb);
  if (x->min_w > 0 || x->min_h > 0) {
    BK_window_wm(x->bkw, "minsize", x->min_w, x->min_h);
  }
  BK_window_wm(x->bkw, "dark", awra_style_is_dark() ? 1.0 : 0.0, 0.0);
  x->scale = BK_window_scale(x->bkw);
  if (measured) {
    forget_measures(w);
  }
  awra_window_identity_native(w); /* identity.c: its icon */
  x->blur_state = BK_window_blur_available(x->bkw);
  if (x->blur_state == 1) {
    x->state |= AWRA_WINDOW_STATE_BLUR;
  }
}

/* AWRA_DUMP_TREE=seconds: the window's widgets, their boxes and sizes, on
 * stderr that long after it shows (layout debugging). */
static void dump_tree(AwraWidget *w, int depth)
{
  int mn = 0, nt = 0, hm = 0, hn = 0;
  awra_measure(w, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
  awra_measure(w, AWRA_ORIENTATION_VERTICAL, w->w, &hm, &hn);
  const AwraClass *c = AWRA_CLASS_OF(w);
  fprintf(stderr, "%*s%s%s%s [%d,%d %dx%d] w:%d/%d h(w):%d/%d%s\n", depth * 2, "",
          c->type ? c->type : "?", awra_widget_name(w) ? " #" : "",
          awra_widget_name(w) ? awra_widget_name(w) : "", w->x, w->y, w->w, w->h, mn, nt, hm, hn,
          awra_child_visible(w) ? "" : " hidden");
  for (int i = 0; i < w->nkids; i++) {
    if (awra_is_widget(w->kids[i])) {
      dump_tree(w->kids[i], depth + 1);
    }
  }
}

static bool dump_later(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  if (!WEXT(w)->closed) {
    dump_tree(w, 0);
  }
  return false;
}

static void dump_unref(void *data)
{
  awra_widget_unref((AwraWidget *)data);
}

void awra_window_present(AwraWidget *w)
{
  if (!awra_widget_is(w, "AwraWindow")) {
    return;
  }
  WinExt *x = WEXT(w);
  ensure_native(w);
  if (!x->presented) {
    x->presented = true;
    const char *dump = getenv("AWRA_DUMP_TREE");
    if (dump && atoi(dump) > 0) {
      awra_timeout_add((unsigned)atoi(dump) * 1000, dump_later, awra_widget_ref(w), dump_unref);
    }
    awra_map_tree(w, true);
    /* gtk_window_show: a window without focus moves it forward, to the
     * first focusable widget of its content, then of its titlebar (not
     * shown as keyboard focus) */
    if (!x->focus) {
      AwraWidget *list[512];
      int n = 0;
      if (x->child) {
        collect_focusable(x->child, list, &n, 512);
      }
      if (!n && x->titlebar) {
        collect_focusable(x->titlebar, list, &n, 512);
      }
      if (n) {
        awra_set_focus(w, list[0], false);
      }
    }
    awra_a11y_window_changed(w, AWRA_A11Y_WINDOW_CREATE);
  }
  x->state &= ~AWRA_WINDOW_STATE_HIDDEN;
  if (!x->offscreen) {
    awra_a11y_platform_start(); /* assistive technologies may look (on demand) */
  }
  if (x->bkw) {
    BK_window_show(x->bkw);
    BK_window_wm(x->bkw, "focus", 0, 0);
    awra_window_identity_present(w); /* identity.c: activation (startup id) */
  }
}

void awra_window_hide(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  if (x->bkw) {
    BK_window_hide(x->bkw);
  }
  x->state |= AWRA_WINDOW_STATE_HIDDEN;
  if (x->presented) {
    awra_a11y_window_changed(w, AWRA_A11Y_WINDOW_DESTROY);
    x->presented = false;
    awra_map_tree(w, false);
  }
  awra_emit_index(w, AWRA_SIGNAL_STATE, x->state);
}

void awra_window_close(AwraWidget *w)
{
  if (awra_widget_is(w, "AwraWindow") && !WEXT(w)->closed) {
    on_close(w, WEXT(w)->bkw);
  }
}

void awra_window_close_request(AwraWidget *w)
{
  awra_window_close(w);
}

void awra_window_begin_move_internal(AwraWidget *w)
{
  if (WEXT(w)->bkw) {
    BK_window_begin_move(WEXT(w)->bkw);
  }
}

static bool release_closed(void *data)
{
  awra_widget_unref((AwraWidget *)data);
  return false;
}

static void window_destroy(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  if (x->closed) {
    return;
  }
  x->closed = true;
  if (x->presented) {
    awra_a11y_window_changed(w, AWRA_A11Y_WINDOW_DESTROY);
  }
  awra_map_tree(w, false);
  for (int i = 0; i < x->npath; i++) {
    awra_widget_unref(x->path[i]);
  }
  x->npath = 0;
  awra_set_focus(w, NULL, false);
  BK_close_popups(x->ctx);
  if (x->bkw) {
    BK_window_close(x->bkw);
  }
  for (int i = 0; i < g_nwindows; i++) {
    if (g_windows[i] == w) {
      memmove(g_windows + i, g_windows + i + 1, sizeof(AwraWidget *) * (size_t)(g_nwindows - i - 1));
      g_nwindows--;
      break;
    }
  }
  /* the list's reference goes once the event that closed the window is
   * handled: a button of a dialog that closes it is still in its press */
  awra_idle_add(release_closed, w, NULL);
}

void awra_window_set_title(AwraWidget *w, const char *title)
{
  WinExt *x = WEXT(w);
  awra_set_str(&x->title, title ? title : "");
  if (x->bkw) {
    BK_window_set_title(x->bkw, x->title);
  }
  awra_a11y_dirty(w);
  if (x->titlebar && awra_widget_is(x->titlebar, "AwraHeader") &&
      awra_widget_get_data(x->titlebar, "awra-default-header"))
  {
    awra_header_set_title(x->titlebar, x->title);
  }
}

const char *awra_window_title(AwraWidget *w)
{
  return WEXT(w)->title;
}

/* Paint order: the content, the title bar over it, then the overlays. */
static void restack(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  AwraWidget *order[10];
  int n = 0;
  if (x->titlebar) {
    order[n++] = x->titlebar;
  }
  for (int i = 0; i < x->noverlays; i++) {
    order[n++] = x->overlays[i];
  }
  for (int i = 0; i < n; i++) {
    if (order[i]->parent == w) {
      awra_widget_ref(order[i]);
      BK_node_remove(w, order[i]);
      BK_node_add(w, order[i], -1);
      awra_widget_unref(order[i]);
    }
  }
}

void awra_window_set_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &WEXT(w)->child, child);
  restack(w);
}

void awra_window_set_overlay_titlebar(AwraWidget *w, bool overlay)
{
  WinExt *x = WEXT(w);
  if (x->overlay_titlebar != overlay) {
    x->overlay_titlebar = overlay;
    awra_widget_queue_resize(w);
    awra_invalidate(w);
  }
}

bool awra_window_overlay_titlebar(AwraWidget *w)
{
  return WEXT(w)->overlay_titlebar;
}

/* Awra 1's AwraWindow:overlay-chrome */
void awra_window_set_overlay_chrome(AwraWidget *w, bool overlaid)
{
  awra_window_set_overlay_titlebar(w, overlaid);
}

void awra_window_add_overlay(AwraWidget *w, AwraWidget *child)
{
  WinExt *x = WEXT(w);
  if (!child || x->noverlays >= 8 || child->parent == w) {
    return;
  }
  awra_widget_set_can_target(child, false);
  x->ov_x[x->noverlays] = x->ov_y[x->noverlays] = 0;
  x->overlays[x->noverlays++] = child;
  awra_child_insert(w, child, -1);
  awra_queue_allocate(w);
}

void awra_window_remove_overlay(AwraWidget *w, AwraWidget *child)
{
  WinExt *x = WEXT(w);
  for (int i = 0; i < x->noverlays; i++) {
    if (x->overlays[i] == child) {
      memmove(&x->overlays[i], &x->overlays[i + 1], sizeof(AwraWidget *) * (size_t)(x->noverlays - i - 1));
      memmove(&x->ov_x[i], &x->ov_x[i + 1], sizeof(double) * (size_t)(x->noverlays - i - 1));
      memmove(&x->ov_y[i], &x->ov_y[i + 1], sizeof(double) * (size_t)(x->noverlays - i - 1));
      x->noverlays--;
      awra_child_remove(w, child);
      return;
    }
  }
}

void awra_window_place_overlay(AwraWidget *w, AwraWidget *child, double px, double py)
{
  WinExt *x = WEXT(w);
  for (int i = 0; i < x->noverlays; i++) {
    if (x->overlays[i] == child) {
      x->ov_x[i] = px;
      x->ov_y[i] = py;
      if (w->w > 0) {
        place_overlay(w, i);
      }
      return;
    }
  }
}

AwraWidget *awra_window_child(AwraWidget *w)
{
  return WEXT(w)->child;
}

void awra_window_set_titlebar(AwraWidget *w, AwraWidget *titlebar)
{
  WinExt *x = WEXT(w);
  awra_set_slot(w, &x->titlebar, titlebar);
  /* the title bar paints over the content: keep it last among children */
  restack(w);
}

AwraWidget *awra_window_titlebar(AwraWidget *w)
{
  return WEXT(w)->titlebar;
}

void awra_window_set_default_size(AwraWidget *w, int width, int height)
{
  WinExt *x = WEXT(w);
  x->default_w = width > 0 ? width : x->default_w;
  x->default_h = height > 0 ? height : x->default_h;
  /* GTK 4 resizes a shown window to its new default size, unless it is
   * maximized or fullscreen (it then applies once restored) */
  if (x->bkw && (!x->presented || !(x->state & (AWRA_WINDOW_STATE_MAXIMIZED | AWRA_WINDOW_STATE_FULLSCREEN)))) {
    BK_window_set_size(x->bkw, x->default_w, x->default_h);
  }
  if (x->offscreen) {
    x->off_w = (int)(x->default_w * x->scale + 0.5);
    x->off_h = (int)(x->default_h * x->scale + 0.5);
  }
}

void awra_window_get_size(AwraWidget *w, int *width, int *height)
{
  if (width) {
    *width = w->w > 0 ? awra_widget_width(w) : WEXT(w)->default_w;
  }
  if (height) {
    *height = w->h > 0 ? awra_widget_height(w) : WEXT(w)->default_h;
  }
}

void awra_window_set_min_size(AwraWidget *w, int width, int height)
{
  WinExt *x = WEXT(w);
  x->min_w = width;
  x->min_h = height;
  if (x->bkw) {
    BK_window_wm(x->bkw, "minsize", width, height);
  }
}

void awra_window_set_resizable(AwraWidget *w, bool resizable)
{
  WEXT(w)->resizable = resizable;
}

static void wm_state(AwraWidget *w, int state)
{
  WinExt *x = WEXT(w);
  if (x->bkw) {
    BK_window_wm(x->bkw, "state", state, 0);
  }
}

void awra_window_maximize(AwraWidget *w)
{
  wm_state(w, 3);
}

void awra_window_unmaximize(AwraWidget *w)
{
  wm_state(w, 0);
}

void awra_window_minimize(AwraWidget *w)
{
  wm_state(w, 1);
}

void awra_window_fullscreen(AwraWidget *w, bool fullscreen)
{
  wm_state(w, fullscreen ? 4 : 0);
}

int awra_window_state(AwraWidget *w)
{
  return WEXT(w)->state;
}

bool awra_window_is_active(AwraWidget *w)
{
  return (WEXT(w)->state & AWRA_WINDOW_STATE_ACTIVE) != 0;
}

void awra_window_set_hide_on_close(AwraWidget *w, bool hide)
{
  WEXT(w)->hide_on_close = hide;
}

void awra_window_set_keep_rounded_corners(AwraWidget *w, bool keep)
{
  WEXT(w)->keep_rounded = keep;
  awra_invalidate(w);
}

void *awra_window_native_handle(AwraWidget *w)
{
  WinExt *x = WEXT(w);
  return x->bkw ? BK_window_handle(x->bkw) : NULL;
}

/* the window's top-left on the screen, logical px (false: unknown, as on
 * Wayland) */
bool awra_window_screen_origin(AwraWidget *w, double *x, double *y);
bool awra_window_screen_origin(AwraWidget *w, double *x, double *y)
{
  *x = *y = 0.0;
  if (!awra_widget_is(w, "AwraWindow") || !WEXT(w)->bkw) {
    return false;
  }
  double a, b;
  if (!BK_window_wm_get(WEXT(w)->bkw, "pos", &a, &b)) {
    return false;
  }
  const double s = WEXT(w)->scale > 0 ? WEXT(w)->scale : 1.0;
  *x = a / s;
  *y = b / s;
  return true;
}

int awra_window_count(void)
{
  return g_nwindows;
}

AwraWidget *awra_window_nth(int i)
{
  return i >= 0 && i < g_nwindows ? g_windows[i] : NULL;
}

void awra_window_stats(AwraWidget *w, uint64_t *frames, uint64_t *pixels)
{
  WinExt *x = WEXT(w);
  if (frames) {
    *frames = x->bkw ? (uint64_t)BK_window_frames(x->bkw) : x->frames;
  }
  if (pixels) {
    *pixels = x->pixels;
  }
}

int awra_window_render_offscreen(AwraWidget *w, bool full)
{
  WinExt *x = WEXT(w);
  if (!x->offscreen) {
    return 0;
  }
  if (!x->presented) {
    x->presented = true;
    awra_map_tree(w, true);
  }
  BK_set_scale(x->scale);
  while (awra_animating()) { /* offscreen frames show the settled state */
    if (!awra_anims_tick()) {
      break;
    }
    break;
  }
  const int regions = BK_render(x->ctx, x->raster, x->off_w, x->off_h, full);
  if (regions) {
    x->frames++;
    const int(*rects)[4];
    const int n = BUI_raster_damage(x->raster, &rects);
    for (int i = 0; i < n; i++) {
      x->pixels += (uint64_t)rects[i][2] * rects[i][3];
    }
  }
  return regions;
}

bool awra_window_has_popup(AwraWidget *w)
{
  if (!w || !awra_widget_is(w, "AwraWindow")) {
    return false;
  }
  BKContext *ctx = awra_ctx(w);
  return ctx && ctx->npopups > 0;
}

BuiRaster *awra_window_raster(AwraWidget *w)
{
  if (!w || !awra_widget_is(w, "AwraWindow")) {
    return NULL;
  }
  WinExt *x = WEXT(w);
  return x->offscreen ? x->raster : (x->bkw ? BK_window_raster(x->bkw) : NULL);
}

bool awra_window_snapshot_png(AwraWidget *w, const char *path)
{
  WinExt *x = WEXT(w);
  BuiRaster *r = x->offscreen ? x->raster : (x->bkw ? BK_window_raster(x->bkw) : NULL);
  if (!r) {
    return false;
  }
  if (x->offscreen) {
    awra_window_render_offscreen(w, true);
  }
  return awra_write_png(path, BUI_raster_pixels(r), BUI_raster_width(r), BUI_raster_height(r));
}

/* ----------------------------------------------------------------------- */
/* Style changes and frames */

static void restyle_tree(AwraWidget *n)
{
  if (awra_is_widget(n)) {
    AwraAux *a = awra_aux(n);
    a->cache_valid = 0;
    a->expand_valid = 0;
    a->needs_allocate = 1;
    n->overdraw = (short)awra_px(AWRA_CLASS_OF(n)->overdraw);
    if (AWRA_CLASS_OF(n)->style_changed) {
      AWRA_CLASS_OF(n)->style_changed(n);
    }
  }
  for (int i = 0; i < n->nkids; i++) {
    restyle_tree(n->kids[i]);
  }
}

void awra_window_style_changed_all(void)
{
  for (int i = 0; i < g_nwindows; i++) {
    AwraWidget *w = g_windows[i];
    WinExt *x = WEXT(w);
    restyle_tree(w);
    for (int k = 0; k < x->ctx->npopups; k++) {
      restyle_tree(x->ctx->popups[k]);
    }
    memset(x->blur_rect, 0, sizeof(x->blur_rect));
    if (x->bkw) {
      /* Windows tints the backdrop and the frame for the appearance */
      BK_window_wm(x->bkw, "dark", awra_style_is_dark() ? 1.0 : 0.0, 0.0);
    }
    x->ctx->layout_dirty = true;
    x->ctx->full_dirty = true;
    bk_ctx_request(x->ctx);
  }
}

void awra_windows_request_frame(void)
{
  for (int i = 0; i < g_nwindows; i++) {
    WinExt *x = WEXT(g_windows[i]);
    if (x->bkw) {
      BK_window_request_frame(x->bkw, 1);
    }
  }
}

bool awra_widget_pointer_position(AwraWidget *w, double *r_x, double *r_y)
{
  BKContext *ctx = w ? bk_ctx(w) : NULL;
  if (!ctx || !awra_widget_mapped(w)) {
    return false;
  }
  const float s = awra_scale();
  *r_x = (ctx->pointer_x - w->x) / s;
  *r_y = (ctx->pointer_y - w->y) / s;
  return *r_x >= 0 && *r_y >= 0 && *r_x < w->w / s && *r_y < w->h / s;
}
