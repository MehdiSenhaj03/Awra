/* Awra 2 path bar: Awra 1's AwraPathBar and AwraBreadcrumbBar in one
 * silhouette. Breadcrumbs (toolbar-like buttons, the current one in 600,
 * go-next separators) and a text field swap inside the same frame, so
 * entering an edit never moves the surrounding chrome; the trail reveals
 * its end when it is longer than the field, and some of the field always
 * stays empty to start an edit (awra.css awrapathbar, awrabreadcrumbbar).
 * Suggestions are given by the application and shown under the field;
 * Up / Down go through them, Tab or a click inserts one, Return submits.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

void awra_menu_popover_set_highlighted(AwraWidget *pop, int index);

typedef struct Crumb {
  char *label;
  int x, w; /* raster, relative to the trail start */
} Crumb;

typedef struct PathExt {
  char *icon, *text, *placeholder;
  Crumb *crumbs;
  int n;
  AwraWidget *entry, *end, *popover;
  bool editing, invalid, syncing;
  int hover, press, drop;
  int trail_x, trail_w; /* raster, window */
  int offset;           /* trail scrolled to show its end */
  char **values;
  int nsugg, highlight;
} PathExt;

#define PX(w) AWRA_EXT(w, PathExt)

static AwraFont crumb_font(bool current)
{
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  if (current) {
    f.weight = 600;
  }
  return f;
}

/* Breadcrumbs are toolbar buttons: in a compact header they take its
 * 5 x 8 padding (awraheader.awra-compact .awra-toolbar-button wins over
 * the breadcrumb's 1 x 6), elsewhere 1 x 6. */
static int crumb_pad(AwraWidget *w)
{
  return awra_px(awra_in_compact_header(w) ? 8 : 6);
}

static void layout_crumbs(AwraWidget *w)
{
  PathExt *x = PX(w);
  const int pad = crumb_pad(w), gap = awra_px(awra_spacing(AWRA_SPACING_XS)), sep = awra_px(12);
  int cx = 0;
  for (int i = 0; i < x->n; i++) {
    if (i > 0) {
      cx += gap + sep + gap;
    }
    AwraFont f = crumb_font(i == x->n - 1);
    const char *l = x->crumbs[i].label;
    x->crumbs[i].x = cx;
    x->crumbs[i].w = (int)ceilf(awra_text_width(&f, l, (int)strlen(l))) + 2 * pad;
    cx += x->crumbs[i].w;
  }
}

static int trail_width(PathExt *x)
{
  return x->n ? x->crumbs[x->n - 1].x + x->crumbs[x->n - 1].w : 0;
}

static int icon_space(PathExt *x)
{
  return x->icon ? awra_px(2) + awra_px(15) + awra_px(4) : 0;
}

static void path_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  PathExt *x = PX(w);
  int emn = 0, ent = 0;
  if (x->end) {
    awra_measure(x->end, o, -1, &emn, &ent);
  }
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    const int base = awra_px(1) * 2 + awra_px(6) + awra_px(3) + icon_space(x) + emn;
    *r_min = base + awra_px(64);
    layout_crumbs(w);
    *r_nat = base + trail_width(x) + awra_px(36);
  }
  else {
    int imn = 0, int_ = 0;
    awra_measure(x->entry, o, -1, &imn, &int_);
    const int inner = BK_MAX(awra_px(30), BK_MAX(int_, ent));
    *r_min = *r_nat = inner + 2 * awra_px(1);
  }
}

static void path_allocate(AwraWidget *w)
{
  PathExt *x = PX(w);
  layout_crumbs(w);
  const int b = awra_px(1);
  const int left = w->x + b + awra_px(6) + icon_space(x);
  int right = w->x + w->w - b - awra_px(3);
  if (x->end) {
    int mn, nat;
    awra_measure(x->end, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nat);
    awra_allocate(x->end, right - nat, w->y + b, nat, w->h - 2 * b);
    right -= nat;
  }
  /* the field */
  awra_allocate(x->entry, left + awra_px(4), w->y + b, BK_MAX(0, right - left - awra_px(8)), w->h - 2 * b);
  /* the trail keeps 36 px of the field empty, and shows its end */
  x->trail_x = left;
  x->trail_w = BK_MAX(0, right - left - awra_px(36));
  const int tw = trail_width(x);
  x->offset = tw > x->trail_w ? tw - x->trail_w : 0;
}

static void path_draw(AwraWidget *w, AwraDraw *d)
{
  PathExt *x = PX(w);
  const AwraTokens *t = awra_tokens();
  AwraBox box = awra_box_of(w);
  const float radius = awra_pxf(9);
  AwraRGBA bg = awra_with_alpha(t->foreground, awra_hovered(w) && !x->editing ? 0.085f : 0.055f);
  const AwraRGBA *border = NULL;
  if (x->editing) {
    bg = t->control_well;
    border = x->invalid ? &t->danger : &t->focus_ring;
  }
  else if (x->invalid) {
    border = &t->danger;
  }
  awra_paint_rbox(d, box, radius, &bg, border ? awra_pxf(1) : 0, border);
  const int b = awra_px(1);
  if (x->icon) {
    const int s = awra_px(15);
    const float ix = w->x + b + awra_px(6) + awra_px(2);
    awra_paint_icon(d, x->icon, ix, w->y + floorf((w->h - s) * 0.5f), s, &t->muted);
  }
  if (x->editing) {
    return;
  }
  /* breadcrumbs */
  bk_clip_push(x->trail_x, w->y, x->trail_w + (x->offset ? 0 : awra_px(36)), w->h);
  const bool compact = awra_in_compact_header(w);
  const int ch = BK_MIN(w->h - 2 * awra_px(1), compact ? awra_px(32) : awra_px(24));
  const float cy = w->y + floorf((w->h - ch) * 0.5f);
  for (int i = 0; i < x->n; i++) {
    const Crumb *c = &x->crumbs[i];
    const float cx = (float)(x->trail_x + c->x - x->offset);
    const bool current = i == x->n - 1;
    if (i > 0) {
      const int s = awra_px(12);
      const int gap = awra_px(awra_spacing(AWRA_SPACING_XS));
      awra_paint_icon(d, "go-next-symbolic", cx - gap - s, w->y + floorf((w->h - s) * 0.5f), s,
                      &t->disabled);
    }
    AwraBox cb = {cx, cy, (float)c->w, (float)ch};
    if (i == x->drop) {
      awra_paint_rbox(d, cb, awra_pxf(6), &t->accent_soft, awra_pxf(1), &t->accent);
    }
    else if (i == x->hover && awra_hovered(w)) {
      awra_paint_rbox(d, cb, awra_pxf(compact ? 8 : 6), &t->control_cap_hover, 0, NULL);
    }
    AwraFont f = crumb_font(current);
    AwraRGBA fg = i == x->drop ? t->accent_content :
                  (current || (i == x->hover && awra_hovered(w))) ? t->foreground : t->muted;
    const float base = w->y + floorf((w->h - awra_font_line_height(&f)) * 0.5f + 0.5f) + awra_font_ascent(&f);
    awra_draw_text(d, &f, cx + crumb_pad(w), base, c->label, (int)strlen(c->label), &fg);
  }
  bk_clip_pop();
}

static int crumb_at(AwraWidget *w, int px, int py)
{
  PathExt *x = PX(w);
  if (x->editing || py < w->y || py >= w->y + w->h || px < x->trail_x ||
      px >= x->trail_x + x->trail_w + (x->offset ? 0 : awra_px(36)))
  {
    return -1;
  }
  for (int i = 0; i < x->n; i++) {
    const int cx = x->trail_x + x->crumbs[i].x - x->offset;
    if (px >= cx && px < cx + x->crumbs[i].w) {
      return i;
    }
  }
  return -1;
}

static void close_suggestions(AwraWidget *w)
{
  PathExt *x = PX(w);
  if (x->popover && awra_popover_visible(x->popover)) {
    awra_popover_popdown(x->popover);
  }
  x->highlight = -1;
}

static void set_editing(AwraWidget *w, bool editing)
{
  PathExt *x = PX(w);
  if (x->editing == editing) {
    return;
  }
  x->editing = editing;
  awra_widget_set_visible(x->entry, editing);
  if (!editing) {
    close_suggestions(w);
    x->invalid = false;
  }
  awra_widget_set_cursor(w, editing ? "text" : NULL);
  awra_queue_allocate(w);
  awra_invalidate(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "editing");
}

static int path_press(AwraWidget *w, BKEvent *e)
{
  PathExt *x = PX(w);
  x->press = crumb_at(w, e->x, e->y);
  return 1;
}

static int path_release(AwraWidget *w, BKEvent *e)
{
  PathExt *x = PX(w);
  const int i = crumb_at(w, e->x, e->y);
  if (!e->inside) {
    return 1;
  }
  if (i >= 0 && i == x->press) {
    awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, i);
  }
  else if (x->press < 0 && !x->editing) {
    awra_path_bar_begin_edit(w, NULL);
  }
  return 1;
}

static int path_motion(AwraWidget *w, BKEvent *e)
{
  PathExt *x = PX(w);
  const int i = crumb_at(w, e->x, e->y);
  if (i != x->hover) {
    x->hover = i;
    awra_widget_set_tooltip(w, i < 0 && !x->editing ? awra_widget_get_data(w, "awra-blank-tip") : NULL);
    awra_invalidate(w);
  }
  return 0;
}

static int path_cursor(AwraWidget *w, int px, int py)
{
  return crumb_at(w, px, py) >= 0 ? BK_CURSOR_DEFAULT : BK_CURSOR_TEXT;
}

static void path_leave(AwraWidget *w, const AwraArgs *a, void *data)
{
  (void)a;
  (void)data;
  PX(w)->hover = -1;
  awra_invalidate(w);
}

static void path_finalize(AwraWidget *w)
{
  PathExt *x = PX(w);
  for (int i = 0; i < x->n; i++) {
    free(x->crumbs[i].label);
  }
  free(x->crumbs);
  for (int i = 0; i < x->nsugg; i++) {
    free(x->values[i]);
  }
  free(x->values);
  free(x->icon);
  free(x->text);
  free(x->placeholder);
  if (x->popover) {
    awra_popover_popdown(x->popover);
    awra_widget_unref(x->popover);
  }
}

static AwraClass path_class = {
    .bk = AWRA_BK("AwraPathBar", CLS_INTERACTIVE),
    .type = "AwraPathBar",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_HOVER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(PathExt),
    .finalize = path_finalize,
    .measure = path_measure,
    .allocate = path_allocate,
    .draw = path_draw,
    .press = path_press,
    .release = path_release,
    .motion = path_motion,
    .cursor = path_cursor,
};

/* ----------------------------------------------------------------------- */
/* The field */

static void insert_suggestion(AwraWidget *w, int i)
{
  PathExt *x = PX(w);
  if (i < 0 || i >= x->nsugg) {
    return;
  }
  awra_entry_set_text(x->entry, x->values[i]);
  awra_entry_set_position(x->entry, -1);
  close_suggestions(w);
}

static void submit(AwraWidget *w)
{
  PathExt *x = PX(w);
  bool refused = false;
  AwraArgs a = {0};
  a.signal = AWRA_SIGNAL_ACTIVATE;
  a.index = -1;
  a.text = awra_entry_text(x->entry);
  a.handled = &refused;
  awra_widget_ref(w);
  awra_emit(w, &a);
  if (refused) {
    x->invalid = true;
    awra_invalidate(w);
  }
  else {
    set_editing(w, false);
  }
  awra_widget_unref(w);
}

static void entry_key(AwraWidget *e, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  PathExt *x = PX(w);
  const bool open = x->popover && awra_popover_visible(x->popover) && x->nsugg > 0;
  if (bk_keysym_is(a->keysym, "Escape")) {
    if (open) {
      close_suggestions(w);
    }
    else {
      set_editing(w, false);
    }
    *a->handled = true;
    return;
  }
  if (open && (bk_keysym_is(a->keysym, "Down") || bk_keysym_is(a->keysym, "Up"))) {
    const int d = bk_keysym_is(a->keysym, "Down") ? 1 : -1;
    x->highlight = x->highlight < 0 ? (d > 0 ? 0 : x->nsugg - 1) : (x->highlight + d + x->nsugg) % x->nsugg;
    awra_menu_popover_set_highlighted(x->popover, x->highlight);
    *a->handled = true;
    return;
  }
  if (open && bk_keysym_is(a->keysym, "Tab")) {
    insert_suggestion(w, x->highlight >= 0 ? x->highlight : 0);
    *a->handled = true;
    return;
  }
  if (bk_keysym_is(a->keysym, "Return") || bk_keysym_is(a->keysym, "KP_Enter")) {
    if (open && x->highlight >= 0) {
      insert_suggestion(w, x->highlight);
    }
    else {
      submit(w);
    }
    *a->handled = true;
  }
}

static void entry_changed(AwraWidget *e, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  PathExt *x = PX(w);
  (void)e;
  if (x->syncing) {
    return;
  }
  if (x->invalid) {
    x->invalid = false;
    awra_invalidate(w);
  }
  awra_emit_text(w, AWRA_SIGNAL_CHANGED, a->text);
}

static void suggestion_activated(AwraWidget *row, const AwraArgs *a, void *data)
{
  (void)row;
  insert_suggestion((AwraWidget *)data, a->index);
}

/* ----------------------------------------------------------------------- */
/* Public API */

AwraWidget *awra_path_bar_new(void)
{
  AwraWidget *w = awra_widget_new(&path_class);
  PathExt *x = PX(w);
  x->hover = x->press = x->drop = x->highlight = -1;
  x->entry = awra_entry_new();
  awra_entry_set_has_frame(x->entry, false);
  awra_widget_set_visible(x->entry, false);
  awra_child_insert(w, x->entry, -1);
  awra_widget_unref(x->entry);
  awra_signal_connect(x->entry, AWRA_SIGNAL_KEY_PRESSED, entry_key, w, NULL);
  awra_signal_connect(x->entry, AWRA_SIGNAL_CHANGED, entry_changed, w, NULL);
  awra_signal_connect(w, AWRA_SIGNAL_LEAVE, path_leave, NULL, NULL);
  awra_widget_set_hexpand(w, true);
  awra_widget_set_cursor(w, NULL);
  return w;
}

void awra_path_bar_set_items(AwraWidget *w, const char *const *labels, int n)
{
  PathExt *x = PX(w);
  for (int i = 0; i < x->n; i++) {
    free(x->crumbs[i].label);
  }
  free(x->crumbs);
  x->crumbs = (Crumb *)calloc((size_t)BK_MAX(1, n), sizeof(Crumb));
  x->n = n;
  for (int i = 0; i < n; i++) {
    x->crumbs[i].label = awra_strdup(labels[i] ? labels[i] : "");
  }
  x->hover = x->drop = -1;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_path_bar_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_set_str(&PX(w)->icon, icon && icon[0] ? icon : NULL);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_path_bar_set_text(AwraWidget *w, const char *text)
{
  awra_set_str(&PX(w)->text, text);
}

void awra_path_bar_set_placeholder(AwraWidget *w, const char *text)
{
  awra_entry_set_placeholder(PX(w)->entry, text);
}

void awra_path_bar_set_end_widget(AwraWidget *w, AwraWidget *end)
{
  awra_set_slot(w, &PX(w)->end, end);
}

void awra_path_bar_set_blank_tooltip(AwraWidget *w, const char *text)
{
  awra_widget_set_data(w, "awra-blank-tip", awra_strdup(text), free);
}

void awra_path_bar_begin_edit(AwraWidget *w, const char *initial)
{
  PathExt *x = PX(w);
  x->syncing = true;
  awra_entry_set_text(x->entry, initial ? initial : (x->text ? x->text : ""));
  x->syncing = false;
  set_editing(w, true);
  AwraWidget *win = awra_window_of(w);
  if (win) {
    awra_set_focus(win, x->entry, true);
  }
  if (initial) {
    awra_entry_set_position(x->entry, -1);
  }
  else {
    awra_entry_select_all(x->entry);
  }
}

void awra_path_bar_end_edit(AwraWidget *w)
{
  set_editing(w, false);
}

bool awra_path_bar_editing(AwraWidget *w)
{
  return PX(w)->editing;
}

const char *awra_path_bar_edit_text(AwraWidget *w)
{
  return awra_entry_text(PX(w)->entry);
}

AwraWidget *awra_path_bar_entry(AwraWidget *w)
{
  return PX(w)->entry;
}

void awra_path_bar_set_suggestions(AwraWidget *w, const char *const *titles,
                                   const char *const *values, int n)
{
  PathExt *x = PX(w);
  for (int i = 0; i < x->nsugg; i++) {
    free(x->values[i]);
  }
  free(x->values);
  x->values = NULL;
  x->nsugg = 0;
  x->highlight = -1;
  if (x->popover) {
    awra_popover_popdown(x->popover);
    awra_widget_unref(x->popover);
    x->popover = NULL;
  }
  if (n <= 0 || !x->editing) {
    return;
  }
  x->values = (char **)calloc((size_t)n, sizeof(char *));
  AwraMenuItem *items = (AwraMenuItem *)calloc((size_t)n, sizeof(AwraMenuItem));
  for (int i = 0; i < n; i++) {
    x->values[i] = awra_strdup(values[i]);
    items[i].label = titles[i];
    items[i].icon_name = "folder-symbolic";
    items[i].check = -1;
    items[i].sensitive = true;
    items[i].handler = suggestion_activated;
    items[i].data = w;
  }
  x->nsugg = n;
  x->popover = awra_menu_popover_new(items, n);
  free(items);
  awra_popover_set_anchor(x->popover, w);
  awra_popover_popup(x->popover);
  /* the field keeps the keyboard */
  AwraWidget *win = awra_window_of(w);
  if (win) {
    awra_set_focus(win, x->entry, false);
  }
}

void awra_path_bar_set_invalid(AwraWidget *w, bool invalid)
{
  PX(w)->invalid = invalid;
  awra_invalidate(w);
}

int awra_path_bar_item_at(AwraWidget *w, double px, double py)
{
  return crumb_at(w, w->x + awra_px(px), w->y + awra_px(py));
}

void awra_path_bar_set_drop_item(AwraWidget *w, int index)
{
  PathExt *x = PX(w);
  if (x->drop != index) {
    x->drop = index;
    awra_invalidate(w);
  }
}
