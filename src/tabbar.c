/* Awra 2 tab bar: the strip of Awra 1's AwraTabView in its workspace role
 * (awra.css .awra-tab-*, .awra-role-workspace-tabs, .awra-role-active-tabs):
 * fixed-width tabs (132 to 220 px, the width shared), a 30 px capsule per
 * tab with 2 px of room, its icon and title, a close button over its end
 * shown on the current and hovered tabs, thin separators between tabs.
 * The current tab of the focused view takes the selection color.
 * Tabs reorder by dragging along the bar; a drag that leaves the bar is
 * handed to the application (drag-begin). The wheel changes tab, a middle
 * click closes one, a double or middle click on the blank asks for a new one.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

#define TAB_MIN 132
#define TAB_MAX 220

typedef struct Tab {
  char *title, *icon, *tooltip;
} Tab;

typedef struct TabExt {
  Tab *tabs;
  int n, cap, selected;
  bool active;
  int hover, hover_close, press, press_close;
  int press_x, press_y;
  bool dragging, handed;
  int drag_x; /* the dragged tab's left edge, raster, relative */
  int insertion; /* a drop marker from outside, -1 none */
  int tab_w;
} TabExt;

#define TX(w) AWRA_EXT(w, TabExt)

static void tabs_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  TabExt *x = TX(w);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = 0;
    *r_nat = x->n * awra_px(TAB_MAX);
  }
  else {
    *r_min = *r_nat = awra_px(32);
  }
}

static void tabs_allocate(AwraWidget *w)
{
  TabExt *x = TX(w);
  const int count = BK_MAX(1, x->n + (x->insertion >= 0 ? 1 : 0));
  x->tab_w = BK_CLAMP(w->w / count, awra_px(TAB_MIN), awra_px(TAB_MAX));
}

static AwraBox pill_box(AwraWidget *w, int i, int left)
{
  /* .awra-tab-pill margin 0 2px, then the separator (2 px margin, 1 px);
   * the last tab's pill has no margin after it and no separator */
  const int ph = awra_px(30);
  const bool last = i >= TX(w)->n - 1;
  const int after = last ? 0 : awra_px(2) + awra_px(2) + awra_px(1);
  return (AwraBox){(float)(left + awra_px(2)), (float)(w->y + (w->h - ph) / 2),
                   (float)(TX(w)->tab_w - awra_px(2) - after), (float)ph};
}

static int tab_left(AwraWidget *w, int i)
{
  TabExt *x = TX(w);
  int slot = i;
  if (x->insertion >= 0 && i >= x->insertion) {
    slot++;
  }
  if (x->dragging && x->press >= 0) {
    if (i == x->press) {
      return w->x + x->drag_x;
    }
    /* the others make room where the dragged one would land */
    const int target = BK_CLAMP((x->drag_x + x->tab_w / 2) / BK_MAX(1, x->tab_w), 0, x->n - 1);
    if (i > x->press && i <= target) {
      slot--;
    }
    else if (i < x->press && i >= target) {
      slot++;
    }
  }
  return w->x + slot * x->tab_w;
}

static AwraBox close_box(AwraWidget *w, AwraBox pill)
{
  const int s = awra_px(22);
  return (AwraBox){pill.x + pill.w - awra_px(4) - s, pill.y + (pill.h - s) * 0.5f, (float)s, (float)s};
}

/* x of [x, x + width) mirrored in `box` when right to left */
static float mirror_in(const AwraWidget *w, AwraBox box, float x, float width)
{
  return awra_widget_rtl(w) ? 2.0f * box.x + box.w - x - width : x;
}

/* Tabs are laid out left to right and drawn mirrored right to left (the
 * first tab at the right, as GtkNotebook places them); pointer positions
 * are mirrored back. */
static void tabs_draw(AwraWidget *w, AwraDraw *d)
{
  TabExt *x = TX(w);
  const AwraTokens *t = awra_tokens();
  const bool hovered = awra_hovered(w);
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < x->n; i++) {
      const bool dragged = x->dragging && i == x->press;
      if ((pass == 1) != dragged) {
        continue;
      }
      const Tab *tab = &x->tabs[i];
      const int left = tab_left(w, i);
      AwraBox pill = awra_mirror_box(w, pill_box(w, i, left));
      const bool sel = i == x->selected;
      const bool hov = hovered && i == x->hover;
      AwraRGBA fg = awra_with_alpha(t->foreground, t->foreground.a * 0.62f);
      if (sel) {
        AwraRGBA bg = x->active ? t->selection : awra_with_alpha(t->foreground, 0.075f);
        awra_paint_rbox(d, pill, awra_pxf(6), &bg, 0, NULL);
        if (!x->active) {
          AwraRGBA rim = awra_with_alpha(t->rim, t->rim.a * 0.24f);
          const float r4[4] = {awra_pxf(6), awra_pxf(6), awra_pxf(6), awra_pxf(6)},
                      w4[4] = {awra_pxf(1), 0, 0, 0};
          awra_paint_rbox4(d, pill, r4, NULL, w4, &rim);
        }
        fg = x->active ? t->accent_content : awra_with_alpha(t->foreground, t->foreground.a * 0.92f);
      }
      else if (hov || dragged) {
        AwraRGBA bg = awra_with_alpha(t->foreground, 0.055f);
        awra_paint_rbox(d, pill, awra_pxf(6), &bg, 0, NULL);
        fg = t->foreground;
      }
      /* separator after the tab, unless beside an emphasized one */
      if (i < x->n - 1 && !sel && !(i + 1 == x->selected) && !hov && !(hovered && i + 1 == x->hover) &&
          !x->dragging)
      {
        AwraRGBA sc = awra_with_alpha(t->foreground, 0.085f);
        awra_paint_rect(d, awra_mirror_box(w, (AwraBox){(float)(left + x->tab_w - awra_px(1)),
                                                        (float)(w->y + awra_px(9)), awra_pxf(1),
                                                        (float)(w->h - 2 * awra_px(9))}),
                        &sc);
      }
      float cx = pill.x + awra_px(20);
      const float right = pill.x + pill.w - awra_px(20);
      if (tab->icon) {
        /* Awra 1's tab: a 22 px image (TAB_PAGE_ICON_SIZE), 6 px before the
         * title (the content box's spacing), in the tab's colour (only the
         * title takes the active tabs' accent) */
        const int s = awra_px(22);
        AwraRGBA ic = fg;
        if (sel && x->active) {
          ic = awra_with_alpha(t->foreground, t->foreground.a * 0.92f);
        }
        awra_paint_icon(d, tab->icon, mirror_in(w, pill, cx, (float)s), pill.y + floorf((pill.h - s) * 0.5f),
                        s, &ic);
        cx += s + awra_px(6);
      }
      AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
      AwraTextLayout *l = awra_text_layout_new();
      awra_text_layout_set_font(l, &f);
      awra_text_layout_set_base_dir(l, awra_widget_rtl(w));
      awra_text_layout_set_text(l, tab->title ? tab->title : "", false);
      awra_text_layout_set_wrap(l, AWRA_WRAP_NONE, AWRA_ELLIPSIZE_END, 1);
      int tw, th;
      const int avail = (int)BK_MAX(0, right - cx);
      awra_text_layout_size(l, avail, &tw, &th);
      const float tx = mirror_in(w, pill, cx, (float)avail);
      float ox = tx;
      if (awra_widget_rtl(w)) {
        /* the title label's xalign flips: its text ends at the right */
        float lx0 = 0.0f, lw = (float)tw;
        awra_text_layout_logical(l, avail, &lx0, &lw);
        ox = tx + avail - fminf(ceilf(lw), (float)avail) - lx0;
      }
      bk_clip_push((int)tx, (int)pill.y, avail, (int)pill.h);
      awra_text_layout_draw(l, d, ox, pill.y + floorf((pill.h - th) * 0.5f), avail, &fg); /* GtkLabel: floored */
      bk_clip_pop();
      awra_text_layout_free(l);
      if (sel || hov) {
        AwraBox cb = close_box(w, pill);
        cb.x = mirror_in(w, pill, cb.x, cb.w);
        AwraRGBA cc = awra_with_alpha(t->foreground, t->foreground.a * 0.62f);
        if (hovered && x->hover_close == i) {
          AwraRGBA cbg = awra_with_alpha(t->foreground, awra_pressed(w) ? 0.16f : 0.115f);
          awra_paint_rbox(d, cb, cb.w * 0.5f, &cbg, 0, NULL);
          cc = t->foreground;
        }
        const int s = awra_px(16);
        /* Awra 1's tab close button: its own awra-close-symbolic */
        awra_paint_icon(d, "awra-close-symbolic", cb.x + floorf((cb.w - s) * 0.5f), cb.y + floorf((cb.h - s) * 0.5f), s,
                        &cc);
      }
    }
  }
  /* keyboard focus: the ring around the selected tab (GTK's :focus-visible) */
  if (awra_focus_visible(w) && x->selected >= 0 && x->selected < x->n) {
    awra_paint_focus_ring(d, awra_mirror_box(w, pill_box(w, x->selected, tab_left(w, x->selected))),
                          awra_pxf(6));
  }
  if (x->insertion >= 0) {
    AwraBox pb = awra_mirror_box(w, pill_box(w, x->insertion, w->x + x->insertion * x->tab_w));
    AwraRGBA fill = awra_with_alpha(t->focus_ring, 0.14f);
    AwraRGBA line = awra_with_alpha(t->focus_ring, 0.55f);
    awra_paint_rbox(d, pb, awra_pxf(6), &fill, awra_pxf(1), &line);
  }
}

static int tab_at(AwraWidget *w, int px, int py, bool *r_close)
{
  TabExt *x = TX(w);
  if (r_close) {
    *r_close = false;
  }
  if (py < w->y || py >= w->y + w->h || x->tab_w <= 0) {
    return -1;
  }
  px = awra_mirror_px(w, px);
  const int i = (px - w->x) / x->tab_w;
  if (px < w->x || i < 0 || i >= x->n) {
    return -1;
  }
  if (r_close && (i == x->selected || i == x->hover)) {
    AwraBox cb = close_box(w, pill_box(w, i, w->x + i * x->tab_w));
    *r_close = px >= cb.x && px < cb.x + cb.w && py >= cb.y && py < cb.y + cb.h;
  }
  return i;
}

static void select_tab(AwraWidget *w, int i, bool notify)
{
  TabExt *x = TX(w);
  if (i < 0 || i >= x->n || i == x->selected) {
    return;
  }
  x->selected = i;
  awra_invalidate(w);
  if (notify) {
    awra_emit_index(w, AWRA_SIGNAL_SELECTED, i);
  }
}

static int tabs_press(AwraWidget *w, BKEvent *e)
{
  TabExt *x = TX(w);
  BKContext *ctx = bk_ctx(w);
  const bool dbl = ctx && ctx->last_click_node == NULL;
  bool close = false;
  x->press = tab_at(w, e->x, e->y, &close);
  x->press_close = close;
  x->press_x = e->x;
  x->press_y = e->y;
  x->dragging = false;
  x->handed = false;
  if (x->press < 0) {
    if (dbl) {
      awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, -1);
    }
    return 1;
  }
  if (!close) {
    select_tab(w, x->press, true);
  }
  return 1;
}

static int tabs_drag(AwraWidget *w, BKEvent *e)
{
  TabExt *x = TX(w);
  if (x->press < 0 || x->press_close || x->handed) {
    return 1;
  }
  const int dx = e->x - x->press_x, dy = e->y - x->press_y;
  if (!x->dragging && dx * dx + dy * dy < awra_px(8) * awra_px(8)) {
    return 1;
  }
  if (abs(dy) > awra_px(24) || e->y < w->y - awra_px(8) || e->y > w->y + w->h + awra_px(8)) {
    /* out of the bar: the application takes it (another view, a window) */
    x->dragging = false;
    x->handed = true;
    awra_invalidate(w);
    AwraArgs a = {0};
    a.signal = AWRA_SIGNAL_DRAG_BEGIN;
    a.index = x->press;
    a.x = (e->x - w->x) / awra_scale();
    a.y = (e->y - w->y) / awra_scale();
    awra_emit(w, &a);
    return 1;
  }
  x->dragging = true;
  x->drag_x = BK_CLAMP(x->press * x->tab_w + (awra_widget_rtl(w) ? -dx : dx), 0,
                       BK_MAX(0, (x->n - 1) * x->tab_w));
  awra_invalidate(w);
  return 1;
}

static int tabs_release(AwraWidget *w, BKEvent *e)
{
  TabExt *x = TX(w);
  if (x->handed) {
    x->handed = false;
    return 1;
  }
  if (x->dragging) {
    x->dragging = false;
    const int from = x->press;
    const int to = BK_CLAMP((x->drag_x + x->tab_w / 2) / BK_MAX(1, x->tab_w), 0, x->n - 1);
    if (from != to) {
      awra_tab_bar_move(w, from, to);
      char buf[32];
      snprintf(buf, sizeof(buf), "%d\t%d", from, to);
      awra_emit_text(w, AWRA_SIGNAL_MOVED, buf);
    }
    awra_invalidate(w);
    return 1;
  }
  if (x->press >= 0 && x->press_close && e->inside) {
    bool close = false;
    if (tab_at(w, e->x, e->y, &close) == x->press && close) {
      awra_emit_index(w, AWRA_SIGNAL_CLOSE_REQUEST, x->press);
    }
  }
  return 1;
}

static int tabs_motion(AwraWidget *w, BKEvent *e)
{
  TabExt *x = TX(w);
  bool close = false;
  const int i = tab_at(w, e->x, e->y, NULL);
  const int prev = x->hover;
  x->hover = i;
  tab_at(w, e->x, e->y, &close);
  const int hc = close ? i : -1;
  if (i != prev || hc != x->hover_close) {
    x->hover_close = hc;
    awra_widget_set_tooltip(w, i >= 0 && !close ? x->tabs[i].tooltip : NULL);
    awra_invalidate(w);
  }
  return 0;
}

static int tabs_scroll(AwraWidget *w, BKEvent *e)
{
  TabExt *x = TX(w);
  const float d = fabsf(e->wheel_y) > fabsf(e->wheel_x) ? -e->wheel_y : e->wheel_x;
  if (d == 0 || x->n < 2) {
    return 0;
  }
  /* stops at the ends */
  select_tab(w, x->selected + (d > 0 ? 1 : -1), true);
  return 1;
}

static void tabs_leave(AwraWidget *w, const AwraArgs *a, void *data)
{
  (void)a;
  (void)data;
  TX(w)->hover = -1;
  TX(w)->hover_close = -1;
  awra_invalidate(w);
}

static void tabs_button(AwraWidget *w, const AwraArgs *a, void *data)
{
  (void)data;
  if (a->signal != AWRA_SIGNAL_PRESSED || a->button != 2) {
    return;
  }
  const int i = tab_at(w, w->x + awra_px(a->x), w->y + awra_px(a->y), NULL);
  if (i >= 0) {
    awra_emit_index(w, AWRA_SIGNAL_CLOSE_REQUEST, i);
  }
  else {
    awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, -1);
  }
  if (a->handled) {
    *a->handled = true;
  }
}

static void tabs_finalize(AwraWidget *w)
{
  TabExt *x = TX(w);
  for (int i = 0; i < x->n; i++) {
    free(x->tabs[i].title);
    free(x->tabs[i].icon);
    free(x->tabs[i].tooltip);
  }
  free(x->tabs);
}

/* the keyboard: Left / Right / Home / End choose a tab (GtkNotebook) */
static int tabs_key(AwraWidget *w, BKEvent *e)
{
  TabExt *x = TX(w);
  if (x->n <= 0) {
    return 0;
  }
  int i = x->selected;
  /* right to left, Left goes to the next tab (it is on the left) */
  const bool rtl = awra_widget_rtl(w);
  const bool left = bk_keysym_is(e->keysym, "Left") || bk_keysym_is(e->keysym, "KP_Left");
  const bool right = bk_keysym_is(e->keysym, "Right") || bk_keysym_is(e->keysym, "KP_Right");
  if (rtl ? right : left) {
    i = i <= 0 ? 0 : i - 1;
  }
  else if (rtl ? left : right) {
    i = i < 0 ? 0 : BK_MIN(x->n - 1, i + 1);
  }
  else if (bk_keysym_is(e->keysym, "Home")) {
    i = 0;
  }
  else if (bk_keysym_is(e->keysym, "End")) {
    i = x->n - 1;
  }
  else {
    return 0;
  }
  select_tab(w, i, true);
  return 1;
}

/* GtkNotebook's tabs: a tab list of tabs (drawn: virtual children) */
static void tabs_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->selection = true;
  info->flags |= AWRA_A11Y_HORIZONTAL;
}

static int tabs_a11y_items(AwraWidget *w)
{
  return TX(w)->n;
}

static void tabs_a11y_item(AwraWidget *w, int i, AwraA11yInfo *info, double rect[4])
{
  TabExt *x = TX(w);
  if (i < 0 || i >= x->n) {
    return;
  }
  info->role = AWRA_ACCESSIBLE_ROLE_TAB;
  info->label = x->tabs[i].title;
  info->description = x->tabs[i].tooltip;
  info->action = "click";
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_SELECTED, i == x->selected);
  info->flags |= AWRA_A11Y_FOCUSABLE;
  AwraWidget *win = awra_window_of(w);
  if (i == x->selected && win && awra_window_focus(win) == w) {
    info->flags |= AWRA_A11Y_FOCUSED;
  }
  const AwraBox b = awra_mirror_box(w, pill_box(w, i, tab_left(w, i)));
  const double sc = awra_scale();
  rect[0] = b.x / sc;
  rect[1] = b.y / sc;
  rect[2] = b.w / sc;
  rect[3] = b.h / sc;
}

static bool tabs_a11y_do(AwraWidget *w, int i, int verb, double value)
{
  TabExt *x = TX(w);
  if (i < 0 || i >= x->n || !awra_sensitive(w)) {
    return false;
  }
  if (verb == AWRA_A11Y_DO_ACTIVATE || verb == AWRA_A11Y_DO_SELECT) {
    select_tab(w, i, true);
    return true;
  }
  if (verb == AWRA_A11Y_DO_FOCUS) {
    select_tab(w, i, true);
    awra_widget_grab_focus(w);
    return true;
  }
  return verb == AWRA_A11Y_DO_SCROLL_TO;
}

static AwraClass tabs_class = {
    .a11y = tabs_a11y,
    .a11y_items = tabs_a11y_items,
    .a11y_item = tabs_a11y_item,
    .a11y_do = tabs_a11y_do,
    .key = tabs_key,
    .overdraw = 4,
    .bk = AWRA_BK("AwraTabBar", CLS_INTERACTIVE),
    .type = "AwraTabBar",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(TabExt),
    .finalize = tabs_finalize,
    .measure = tabs_measure,
    .allocate = tabs_allocate,
    .draw = tabs_draw,
    .press = tabs_press,
    .drag = tabs_drag,
    .release = tabs_release,
    .motion = tabs_motion,
    .scroll = tabs_scroll,
};

AwraWidget *awra_tab_bar_new(void)
{
  AwraWidget *w = awra_widget_new(&tabs_class);
  TabExt *x = TX(w);
  x->selected = -1;
  x->hover = x->hover_close = x->press = x->insertion = -1;
  awra_signal_connect(w, AWRA_SIGNAL_LEAVE, tabs_leave, NULL, NULL);
  awra_signal_connect(w, AWRA_SIGNAL_PRESSED, tabs_button, NULL, NULL);
  return w;
}

int awra_tab_bar_insert(AwraWidget *w, int position, const char *title, const char *icon)
{
  TabExt *x = TX(w);
  if (x->n == x->cap) {
    x->cap = x->cap ? x->cap * 2 : 8;
    x->tabs = (Tab *)realloc(x->tabs, sizeof(Tab) * (size_t)x->cap);
  }
  if (position < 0 || position > x->n) {
    position = x->n;
  }
  memmove(&x->tabs[position + 1], &x->tabs[position], sizeof(Tab) * (size_t)(x->n - position));
  x->tabs[position] = (Tab){awra_strdup(title ? title : ""), awra_strdup(icon), NULL};
  x->n++;
  if (x->selected >= position) {
    x->selected++;
  }
  if (x->selected < 0) {
    x->selected = position;
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
  return position;
}

void awra_tab_bar_remove(AwraWidget *w, int index)
{
  TabExt *x = TX(w);
  if (index < 0 || index >= x->n) {
    return;
  }
  free(x->tabs[index].title);
  free(x->tabs[index].icon);
  free(x->tabs[index].tooltip);
  memmove(&x->tabs[index], &x->tabs[index + 1], sizeof(Tab) * (size_t)(x->n - index - 1));
  x->n--;
  if (x->selected > index || x->selected >= x->n) {
    x->selected = x->n ? x->selected - 1 : -1;
    if (x->selected < 0 && x->n) {
      x->selected = 0;
    }
  }
  x->hover = x->hover_close = -1;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_tab_bar_move(AwraWidget *w, int from, int to)
{
  TabExt *x = TX(w);
  if (from < 0 || from >= x->n || to < 0 || to >= x->n || from == to) {
    return;
  }
  Tab t = x->tabs[from];
  const int sel_was = x->selected;
  if (from < to) {
    memmove(&x->tabs[from], &x->tabs[from + 1], sizeof(Tab) * (size_t)(to - from));
  }
  else {
    memmove(&x->tabs[to + 1], &x->tabs[to], sizeof(Tab) * (size_t)(from - to));
  }
  x->tabs[to] = t;
  if (sel_was == from) {
    x->selected = to;
  }
  else if (from < sel_was && to >= sel_was) {
    x->selected--;
  }
  else if (from > sel_was && to <= sel_was) {
    x->selected++;
  }
  awra_invalidate(w);
}

void awra_tab_bar_set_title(AwraWidget *w, int index, const char *title)
{
  TabExt *x = TX(w);
  if (index >= 0 && index < x->n) {
    awra_set_str(&x->tabs[index].title, title ? title : "");
    awra_invalidate(w);
  }
}

void awra_tab_bar_set_icon_name(AwraWidget *w, int index, const char *icon)
{
  TabExt *x = TX(w);
  if (index >= 0 && index < x->n) {
    awra_set_str(&x->tabs[index].icon, icon);
    awra_invalidate(w);
  }
}

void awra_tab_bar_set_tab_tooltip(AwraWidget *w, int index, const char *text)
{
  TabExt *x = TX(w);
  if (index >= 0 && index < x->n) {
    awra_set_str(&x->tabs[index].tooltip, text);
  }
}

int awra_tab_bar_count(AwraWidget *w)
{
  return TX(w)->n;
}

int awra_tab_bar_selected(AwraWidget *w)
{
  return TX(w)->selected;
}

void awra_tab_bar_set_selected(AwraWidget *w, int index)
{
  select_tab(w, index, false);
}

void awra_tab_bar_set_active(AwraWidget *w, bool active)
{
  TX(w)->active = active;
  awra_invalidate(w);
}

int awra_tab_bar_index_at(AwraWidget *w, double px, double py)
{
  return tab_at(w, w->x + awra_px(px), w->y + awra_px(py), NULL);
}

bool awra_tab_bar_tab_bounds(AwraWidget *w, int index, double *r_x, double *r_y, double *r_w,
                             double *r_h)
{
  const TabExt *x = TX(w);
  if (index < 0 || index >= x->n || x->tab_w <= 0) {
    return false;
  }
  const double s = awra_scale();
  *r_x = index * x->tab_w / s;
  *r_y = 0;
  *r_w = x->tab_w / s;
  *r_h = w->h / s;
  return true;
}

int awra_tab_bar_insertion_at(AwraWidget *w, double px)
{
  TabExt *x = TX(w);
  const int rx = awra_px(px);
  return BK_CLAMP((rx + x->tab_w / 2) / BK_MAX(1, x->tab_w), 0, x->n);
}

void awra_tab_bar_set_insertion(AwraWidget *w, int position)
{
  TabExt *x = TX(w);
  if (x->insertion != position) {
    x->insertion = position;
    awra_queue_allocate(w);
    awra_invalidate(w);
  }
}
