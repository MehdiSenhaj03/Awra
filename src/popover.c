/* Awra 2 overlays: popovers (Floating material, unfold animation), menus,
 * menu buttons, dropdowns, context menus, toasts, the command palette,
 * dialogs and tooltips (spec A §14-15, 18-19; spec B §10-12, 21).
 *
 * Popovers live in the window's overlay layer (the kit's popups): they are
 * drawn above the content, take input first, and close on an outside click
 * or Escape, as GTK's autohide popovers do.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <ctype.h>
#include <stdio.h>

void awra_button_set_look(AwraWidget *w, int look);
void awra_entry_set_quiet(AwraWidget *w, bool quiet);
void awra_header_set_dialog(AwraWidget *w, bool dialog);
void awra_header_set_close_only(AwraWidget *w);
void awra_header_set_compact(AwraWidget *w, bool compact);

/* ======================================================================= */
/* Popover */

typedef struct PopExt {
  AwraWidget *child;
  AwraWidget *anchor;      /* the widget it points at */
  bool anchor_weak;        /* the anchor owns it (a menu button): not referenced */
  AwraWidget *relative;    /* popup_at: a point of this widget */
  double at_x, at_y;
  bool pointing;           /* point_to: centred under the point, as GtkPopover's pointing-to */
  AwraWidget *return_focus; /* where the keyboard was before it took it (held while open) */
  int side;                /* 0 bottom 1 top 2 start 3 end */
  bool open, centered, modal_scrim;
  float unfold;            /* 0..1 */
  int pad;                 /* logical padding around the child */
  float radius;            /* logical */
  bool menu;               /* a menu (min width 210) */
  /* a submenu: the row it opens from (its anchor, not referenced: the row
   * owns the submenu) */
  AwraWidget *submenu_row;
  /* a menu: the row whose hover opens its submenu (or closes another) soon */
  unsigned hover_timer;
  AwraWidget *hover_row;
  /* GtkPopover's pointing-to (logical, in the anchor's coordinates; w < 0:
   * the anchor's bounds), offset and autohide */
  double pt_x, pt_y, pt_w, pt_h;
  int off_x, off_y;
  bool autohide_off;
  bool surfaced; /* the last placement gave it a native surface */
  bool blur;     /* ... and the compositor blurs behind it */
  unsigned too_small; /* the idle closing it: its surface is under its minimum size */
} PopExt;

#define POPX(w) AWRA_EXT(w, PopExt)

static void pop_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  PopExt *x = POPX(w);
  const int pad = awra_px(x->pad) * 2;
  int mn = 0, nt = 0;
  if (x->child) {
    awra_measure(x->child, o, for_size >= 0 ? for_size - pad : -1, &mn, &nt);
  }
  *r_min = mn + pad;
  *r_nat = nt + pad;
}

static void pop_allocate(AwraWidget *w)
{
  PopExt *x = POPX(w);
  const int pad = awra_px(x->pad);
  if (x->child) {
    awra_allocate(x->child, w->x + pad, w->y + pad, BK_MAX(0, w->w - 2 * pad),
                  BK_MAX(0, w->h - 2 * pad));
  }
}

/* Windows: the popup is a window DWM rounds (its small radius, none on
 * Windows 10) and shadows; it draws the same corner and no shadow (Awra 1's
 * awra-win32-popup). */
static bool pop_native_frame(const AwraWidget *w, float *r_radius)
{
  BKContext *ctx = POPX(w)->surfaced ? bk_ctx(w) : NULL;
  if (ctx && ctx->popup_host && (ctx->popup_host->flags & BK_POPHOST_NATIVE_FRAME)) {
    *r_radius = (float)ctx->popup_host->corner_radius;
    return true;
  }
  return false;
}

/* its material: frosted when its surface is blurred (Awra 1's effect
 * coordinator on popovers), the opaque fallback in the window */
static void pop_material(const AwraWidget *w, AwraResolvedMaterial *m, float *r_radius,
                         AwraElevation *r_elevation)
{
  const PopExt *x = POPX(w);
  *r_radius = awra_pxf(x->radius);
  *r_elevation = AWRA_ELEVATION_FLOATING;
  if (pop_native_frame(w, r_radius)) {
    *r_elevation = AWRA_ELEVATION_FLAT;
  }
  awra_material_resolve(AWRA_MATERIAL_FLOATING, AWRA_ROLE_POPOVER, *r_elevation,
                        x->surfaced && x->blur, m);
}

static void pop_draw(AwraWidget *w, AwraDraw *d)
{
  AwraResolvedMaterial m;
  float radius;
  AwraElevation elevation;
  pop_material(w, &m, &radius, &elevation);
  /* the outline and rim are drawn once, over the content (pop_draw_over):
   * drawn here too they doubled (GSK draws them once) */
  m.border_edges = 0;
  m.highlight_edges = 0;
  awra_paint_material(d, awra_box_of(w), &m, radius, 15, AWRA_EDGE_ALL, elevation);
}

static void pop_draw_over(AwraWidget *w, AwraDraw *d)
{
  /* the outline and rim sit above the content (GSK border nodes after the
   * child): redraw them so rows touching the edge don't cover them */
  AwraResolvedMaterial m;
  float r;
  AwraElevation elevation;
  pop_material(w, &m, &r, &elevation);
  const float r4[4] = {r, r, r, r};
  const float bw = awra_pxf(m.border_width), hw = awra_pxf(m.highlight_width);
  const float b4[4] = {bw, bw, bw, bw}, h4[4] = {hw, 0, 0, hw};
  awra_paint_rbox4(d, awra_box_of(w), r4, NULL, b4, &m.border);
  awra_paint_rbox4(d, awra_box_of(w), r4, NULL, h4, &m.highlight);
}

static AwraWidget *window_root(AwraWidget *w)
{
  PopExt *x = POPX(w);
  AwraWidget *a = x->anchor ? x->anchor : x->relative;
  return a ? awra_window_of(a) : NULL;
}

/* AWRA_SHADOWS=1 draws the Floating elevation's shadow (paint.c): its
 * surface then reserves the room around the box (GTK's shadow width). */
static void pop_shadow(int r_shadow[4])
{
  static int shadows = -1;
  if (shadows < 0) {
    const char *e = getenv("AWRA_SHADOWS");
    shadows = e && e[0] == '1';
  }
  if (!shadows) {
    r_shadow[0] = r_shadow[1] = r_shadow[2] = r_shadow[3] = 0;
    return;
  }
  /* 0 7px 20px: the blur each side, the offset down */
  r_shadow[0] = r_shadow[1] = awra_px(20);
  r_shadow[2] = awra_px(20 - 7);
  r_shadow[3] = awra_px(20 + 7);
}

/* GtkPopover's create_popup_layout: the anchor's bounds (or the pointing-to
 * rectangle), the gravities of its position and alignment, flipped across
 * the anchor and slid along it, resized when the work area is too small. */
static void pop_layout(AwraWidget *w, int nw, int nh, int win_w, int win_h, BKPopupLayout *l)
{
  PopExt *x = POPX(w);
  memset(l, 0, sizeof(*l));
  pop_shadow(l->shadow);
  l->kind = x->menu || x->submenu_row ? BK_POPUP_KIND_MENU : BK_POPUP_KIND_DIALOG;
  if (x->centered) {
    /* a palette: its own window in Awra 1, centred on the window, a third
     * of the free height above it */
    const int y0 = BK_MAX(awra_px(48), (win_h - nh) / 3);
    l->anchor = bk_rect(win_w / 2, y0, 1, 1);
    l->rect_anchor = BK_GRAVITY_NORTH_WEST;
    l->surface_anchor = BK_GRAVITY_NORTH;
    l->hints = BK_ANCHOR_SLIDE | BK_ANCHOR_RESIZE;
    l->kind = BK_POPUP_KIND_DIALOG;
    return;
  }
  AwraWidget *parent = x->relative ? x->relative : x->anchor;
  if (!parent) {
    l->anchor = bk_rect(win_w / 2, win_h / 2, 1, 1);
    l->rect_anchor = BK_GRAVITY_NORTH_WEST;
    l->surface_anchor = BK_GRAVITY_CENTER;
    l->hints = BK_ANCHOR_SLIDE | BK_ANCHOR_RESIZE;
    return;
  }
  const float s = awra_scale();
  /* right to left, start and end swap (GtkPopover's halign gravities,
   * submenus and menus at a point open towards the left) */
  const bool rtl = awra_widget_rtl(w);
  if (x->relative) { /* popup_at / point_to: the point */
    l->anchor = bk_rect(parent->x + (int)floor(x->at_x * s), parent->y + (int)floor(x->at_y * s), 1,
                        1);
    if (!x->pointing) {
      /* popup_at: its corner at the point (below it, or above it for side
       * 1, a caret), flipped and slid to stay on the work area */
      l->rect_anchor = BK_GRAVITY_NORTH_WEST;
      l->surface_anchor = x->side == 1 ? (rtl ? BK_GRAVITY_SOUTH_EAST : BK_GRAVITY_SOUTH_WEST) :
                                         (rtl ? BK_GRAVITY_NORTH_EAST : BK_GRAVITY_NORTH_WEST);
      l->hints = BK_ANCHOR_FLIP | BK_ANCHOR_SLIDE | BK_ANCHOR_RESIZE;
      return;
    }
  }
  else if (x->pt_w >= 0 && x->pt_h >= 0 && !x->submenu_row) {
    const int ax = parent->x + (int)floor(x->pt_x * s), ay = parent->y + (int)floor(x->pt_y * s);
    l->anchor = bk_rect(ax, ay, BK_MAX(1, (int)ceil(x->pt_w * s)), BK_MAX(1, (int)ceil(x->pt_h * s)));
  }
  else {
    l->anchor = bk_rect(parent->x, parent->y, parent->w, parent->h);
  }
  l->dx = awra_px(x->off_x);
  l->dy = awra_px(x->off_y);
  const AwraAux *a = awra_aux(w);
  /* Awra's sides: 0 bottom 1 top 2 start (left) 3 end (right) */
  const int side = x->submenu_row ? 3 : x->side;
  if (side == 2 || side == 3) {
    const bool left = (side == 2) != rtl;
    switch (a->valign) {
      case AWRA_ALIGN_START:
        l->rect_anchor = left ? BK_GRAVITY_NORTH_WEST : BK_GRAVITY_NORTH_EAST;
        l->surface_anchor = left ? BK_GRAVITY_NORTH_EAST : BK_GRAVITY_NORTH_WEST;
        break;
      case AWRA_ALIGN_END:
        l->rect_anchor = left ? BK_GRAVITY_SOUTH_WEST : BK_GRAVITY_SOUTH_EAST;
        l->surface_anchor = left ? BK_GRAVITY_SOUTH_EAST : BK_GRAVITY_SOUTH_WEST;
        break;
      default:
        l->rect_anchor = left ? BK_GRAVITY_WEST : BK_GRAVITY_EAST;
        l->surface_anchor = left ? BK_GRAVITY_EAST : BK_GRAVITY_WEST;
        break;
    }
    l->hints = BK_ANCHOR_FLIP_X | BK_ANCHOR_SLIDE_Y | BK_ANCHOR_RESIZE;
    return;
  }
  const bool top = side == 1;
  const int halign = rtl && a->halign == AWRA_ALIGN_START ? AWRA_ALIGN_END :
                     rtl && a->halign == AWRA_ALIGN_END   ? AWRA_ALIGN_START :
                                                            a->halign;
  switch (halign) {
    case AWRA_ALIGN_START:
      l->rect_anchor = top ? BK_GRAVITY_NORTH_WEST : BK_GRAVITY_SOUTH_WEST;
      l->surface_anchor = top ? BK_GRAVITY_SOUTH_WEST : BK_GRAVITY_NORTH_WEST;
      break;
    case AWRA_ALIGN_END:
      l->rect_anchor = top ? BK_GRAVITY_NORTH_EAST : BK_GRAVITY_SOUTH_EAST;
      l->surface_anchor = top ? BK_GRAVITY_SOUTH_EAST : BK_GRAVITY_NORTH_EAST;
      break;
    default:
      l->rect_anchor = top ? BK_GRAVITY_NORTH : BK_GRAVITY_SOUTH;
      l->surface_anchor = top ? BK_GRAVITY_SOUTH : BK_GRAVITY_NORTH;
      break;
  }
  l->hints = BK_ANCHOR_FLIP_Y | BK_ANCHOR_SLIDE_X | BK_ANCHOR_RESIZE;
}

static void too_small_done(void *data)
{
  awra_widget_unref((AwraWidget *)data);
}

static bool too_small_fire(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  POPX(w)->too_small = 0;
  awra_popover_popdown(w);
  return false;
}

/* GtkPopover's is_acceptable_size: the system gave its surface less than
 * the popover's minimum size (GDK_ANCHOR_RESIZE on a work area too small):
 * it pops down, as GTK's do (on idle: this is its layout). */
static void pop_check_size(AwraWidget *w, int width, int height)
{
  PopExt *x = POPX(w);
  int min_w, nat, min_h;
  awra_measure(w, AWRA_ORIENTATION_HORIZONTAL, -1, &min_w, &nat);
  awra_measure(w, AWRA_ORIENTATION_VERTICAL, BK_MAX(width, min_w), &min_h, &nat);
  if ((width >= min_w && height >= min_h) || x->too_small || !x->open) {
    return;
  }
  x->too_small = awra_timeout_add(0, too_small_fire, awra_widget_ref(w), too_small_done);
}

static void pop_place(BKNode *n, int win_w, int win_h)
{
  AwraWidget *w = n;
  PopExt *x = POPX(w);
  int mn, nw, nh;
  awra_measure(w, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nw);
  if (x->menu) {
    nw = BK_MAX(nw, awra_px(210 + 14));
  }
  awra_measure(w, AWRA_ORIENTATION_VERTICAL, nw, &mn, &nh);
  BKPopupLayout l;
  pop_layout(w, nw, nh, win_w, win_h, &l);
  BKContext *pctx = bk_ctx(w);
  if (pctx && pctx->popup_host && (pctx->popup_host->flags & (BK_POPHOST_NATIVE_FRAME | BK_POPHOST_OPAQUE))) {
    memset(l.shadow, 0, sizeof(l.shadow)); /* the system shadows it, or nothing can */
  }
  {
    AwraResolvedMaterial m;
    awra_material_resolve(AWRA_MATERIAL_FLOATING, AWRA_ROLE_POPOVER, AWRA_ELEVATION_FLOATING,
                          awra_style_blur_active(), &m);
    l.blur = m.request_blur;
    l.radius = awra_px(x->radius);
    if (x->blur != (bool)l.blur) {
      x->blur = l.blur;
      bk_invalidate(w);
    }
  }
  if (x->autohide_off) {
    w->flags |= NF_NO_AUTOHIDE;
  }
  else {
    w->flags &= ~NF_NO_AUTOHIDE;
  }
  BKRect box;
  const bool surfaced = bk_popup_layout(w, &l, nw + l.shadow[0] + l.shadow[1],
                                        nh + l.shadow[2] + l.shadow[3], &box) != 0;
  if (surfaced != x->surfaced) {
    x->surfaced = surfaced;
    bk_invalidate(w);
  }
  if (surfaced) {
    pop_check_size(w, bk_rw(&box), bk_rh(&box));
  }
  const int x0 = box.xmin;
  /* unfold: translateY(-4px) while it opens (inside its surface, as GTK's
   * contents animation) */
  const int y0 = box.ymin - (int)(awra_pxf(4) * (1.0f - x->unfold));
  nw = bk_rw(&box);
  nh = bk_rh(&box);
  /* placed by the kit: bk_arrange sets the rect; lay out the contents */
  const bool moved = w->x != x0 || w->y != y0 || w->w != nw || w->h != nh;
  if (moved) {
    bk_invalidate(w);
  }
  w->x = x0;
  w->y = y0;
  w->w = nw;
  w->h = nh;
  if (moved) {
    awra_aux(w)->needs_allocate = 1;
  }
  awra_bk_arrange(w);
  if (moved) {
    bk_invalidate(w);
  }
  (void)window_root;
}

static void menu_hover_cancel(AwraWidget *pop);

/* `n` is `root` or inside it (open popups hang under their owners) */
static bool within(const AwraWidget *n, const AwraWidget *root)
{
  for (; n; n = n->parent) {
    if (n == root) {
      return true;
    }
  }
  return false;
}

static void pop_closed(BKNode *n)
{
  AwraWidget *w = n;
  PopExt *x = POPX(w);
  if (!x->open) {
    return;
  }
  menu_hover_cancel(w);
  if (x->submenu_row) {
    /* a submenu closed while the keyboard was in it gives it back to its row */
    AwraWidget *win = awra_window_of(x->submenu_row);
    AwraWidget *f = win ? awra_focus_of(win) : NULL;
    if (f && within(f, w)) {
      awra_set_focus(win, x->submenu_row, awra_aux(f)->focus_visible);
    }
    awra_invalidate(x->submenu_row);
  }
  /* the keyboard was inside: it goes back where it was before (GTK) */
  AwraWidget *back = x->return_focus;
  x->return_focus = NULL;
  AwraWidget *bwin = back ? awra_window_of(back) : NULL;
  AwraWidget *now = bwin ? awra_focus_of(bwin) : NULL;
  const bool inside = !now || within(now, w);
  x->open = false;
  if (x->anchor) {
    awra_invalidate(x->anchor); /* .awra-open goes */
  }
  awra_map_tree(w, false);
  w->parent = NULL;
  if (back) {
    if (inside && bwin && awra_widget_mapped(back)) {
      awra_set_focus(bwin, back, now ? awra_aux(now)->focus_visible : false);
    }
    awra_widget_unref(back);
  }
  awra_emit_simple(w, AWRA_SIGNAL_HIDE);
  if (x->anchor) {
    awra_invalidate(x->anchor);
  }
  awra_widget_unref(w); /* the reference the open popup held */
}

void awra_map_tree(AwraWidget *w, bool mapped);

static void pop_finalize(AwraWidget *w)
{
  PopExt *x = POPX(w);
  menu_hover_cancel(w);
  if (x->anchor && !x->submenu_row && !x->anchor_weak) {
    awra_widget_unref(x->anchor);
  }
}

AwraWidget *awra_first_focusable(AwraWidget *root);

static int menu_key(AwraWidget *pop, BKEvent *e);

static int pop_key(AwraWidget *w, BKEvent *e)
{
  return POPX(w)->menu ? menu_key(w, e) : 0;
}

/* GtkPopover is a dialog, GtkPopoverMenu a menu */
static void pop_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->role = POPX(w)->menu ? AWRA_ACCESSIBLE_ROLE_MENU : AWRA_ACCESSIBLE_ROLE_DIALOG;
}

AwraClass awra_popover_class = {
    .a11y = pop_a11y,
    .bk = AWRA_BK("AwraPopover", CLS_POPUP),
    .type = "AwraPopover",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_POPUP,
    .ext_size = sizeof(PopExt),
    .finalize = pop_finalize,
    .measure = pop_measure,
    .allocate = pop_allocate,
    .draw = pop_draw,
    .draw_over = pop_draw_over,
    .key = pop_key,
};

static void popover_class_setup(void)
{
  awra_popover_class.bk.place = pop_place;
  awra_popover_class.bk.popup_closed = pop_closed;
}

AwraWidget *awra_popover_new(void)
{
  popover_class_setup();
  AwraWidget *w = awra_widget_new(&awra_popover_class);
  PopExt *x = POPX(w);
  x->radius = 17;
  x->unfold = 1.0f;
  x->pt_w = x->pt_h = -1;
  w->flags |= NF_EVENTS; /* clicks on its padding stay inside it */
  return w;
}

void awra_popover_set_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &POPX(w)->child, child);
}

void awra_popover_set_anchor(AwraWidget *w, AwraWidget *anchor)
{
  PopExt *x = POPX(w);
  if (anchor) {
    awra_widget_ref(anchor);
  }
  if (x->anchor && !x->anchor_weak) {
    awra_widget_unref(x->anchor);
  }
  x->anchor = anchor;
  x->anchor_weak = false;
}

/* The anchor owns the popover (a menu button holds it): a reference back
 * would keep both alive forever. The owner clears it when it goes. */
static void popover_set_owner(AwraWidget *w, AwraWidget *owner)
{
  PopExt *x = POPX(w);
  if (x->anchor == owner && x->anchor_weak) {
    return;
  }
  if (x->anchor && !x->anchor_weak) {
    awra_widget_unref(x->anchor);
  }
  x->anchor = owner;
  x->anchor_weak = true;
}

static void unfold_anim(AwraWidget *w, double t, void *data)
{
  PopExt *x = POPX(w);
  x->unfold = (float)t;
  awra_widget_set_opacity(w, t);
  BKContext *ctx = bk_ctx(w);
  if (ctx) {
    ctx->layout_dirty = true; /* re-placed each frame (translateY) */
    bk_ctx_request(ctx);
  }
}

static void popup_with(AwraWidget *w, AwraWidget *owner)
{
  PopExt *x = POPX(w);
  BKContext *ctx = owner ? awra_ctx(owner) : NULL;
  if (!ctx) {
    return;
  }
  if (x->open) {
    BK_close_popups(ctx); /* reopen fresh */
  }
  awra_widget_ref(w); /* held while open */
  x->open = true;
  awra_invalidate(owner); /* a menu button shows itself open (.awra-open) */
  x->unfold = awra_style_reduced_motion() ? 1.0f : 0.0f;
  bk_open_popup(ctx, w, owner, NULL);
  awra_map_tree(w, true);
  awra_widget_queue_resize(w);
  ctx->layout_dirty = true;
  bk_ctx_request(ctx);
  awra_anim_start(w, 120, unfold_anim, NULL, NULL);
  awra_emit_simple(w, AWRA_SIGNAL_SHOW);
  /* keyboard focus moves in: its first focusable widget (a text field
   * always; any widget when the popover was opened from the keyboard, its
   * ring shown, as GTK's popovers take the focus) */
  AwraWidget *win = awra_window_of(owner);
  if (win) {
    /* where the keyboard is now: it goes back there if it is inside the
     * popover when it closes, as with GTK's */
    AwraWidget *before = awra_focus_of(win);
    if (before && !within(before, w) && !x->return_focus) {
      x->return_focus = before;
      awra_widget_ref(before);
    }
    /* gtk_popover_show: an autohide popover moves the focus to its first
     * focusable widget, its ring shown as the window's focus-visible says */
    AwraWidget *f = awra_first_focusable(w);
    const bool keyboard = awra_focus_visible(owner);
    if (f && (!x->autohide_off || (AWRA_CLASS_OF(f)->flags & AWRA_CLASS_TEXT_INPUT) || keyboard)) {
      awra_set_focus(win, f, keyboard);
    }
  }
}

void awra_popover_popup(AwraWidget *w)
{
  PopExt *x = POPX(w);
  x->relative = NULL;
  x->pointing = false;
  popup_with(w, x->anchor);
}

void awra_popover_popup_at(AwraWidget *w, AwraWidget *relative, double px, double py)
{
  PopExt *x = POPX(w);
  x->relative = relative;
  x->at_x = px;
  x->at_y = py;
  x->pointing = false;
  popup_with(w, relative);
}

void awra_popover_point_to(AwraWidget *w, AwraWidget *relative, double px, double py)
{
  PopExt *x = POPX(w);
  x->relative = relative;
  x->at_x = px;
  x->at_y = py;
  x->pointing = true;
  popup_with(w, relative);
}

void awra_popover_popdown(AwraWidget *w)
{
  PopExt *x = POPX(w);
  if (!x->open) {
    return;
  }
  BKContext *ctx = bk_ctx(w);
  if (ctx) {
    const int i = bk_popup_index(ctx, w);
    bk_close_popups_after(ctx, i > 0 ? ctx->popups[i - 1] : NULL);
  }
  else {
    pop_closed(w);
  }
}

bool awra_popover_visible(AwraWidget *w)
{
  return POPX(w)->open;
}

/* a placement setting changed while it shows: placed again */
static void pop_replace(AwraWidget *w)
{
  BKContext *ctx = POPX(w)->open ? bk_ctx(w) : NULL;
  if (ctx) {
    ctx->layout_dirty = true;
    bk_ctx_request(ctx);
  }
}

void awra_popover_set_position(AwraWidget *w, int side)
{
  POPX(w)->side = side;
  pop_replace(w);
}

void awra_popover_set_pointing_to(AwraWidget *w, double x, double y, double width, double height)
{
  PopExt *p = POPX(w);
  p->pt_x = x;
  p->pt_y = y;
  p->pt_w = width < 0 || height < 0 ? -1 : width;
  p->pt_h = width < 0 || height < 0 ? -1 : height;
  pop_replace(w);
}

void awra_popover_set_offset(AwraWidget *w, int x, int y)
{
  POPX(w)->off_x = x;
  POPX(w)->off_y = y;
  pop_replace(w);
}

void awra_popover_set_autohide(AwraWidget *w, bool autohide)
{
  POPX(w)->autohide_off = !autohide;
  pop_replace(w);
}

bool awra_popover_autohide(AwraWidget *w)
{
  return !POPX(w)->autohide_off;
}

void awra_popover_set_padding_internal(AwraWidget *w, int pad, bool menu);
void awra_popover_set_padding_internal(AwraWidget *w, int pad, bool menu)
{
  POPX(w)->pad = pad;
  POPX(w)->menu = menu;
}

void awra_popover_set_centered_internal(AwraWidget *w, bool centered);
void awra_popover_set_centered_internal(AwraWidget *w, bool centered)
{
  POPX(w)->centered = centered;
}

AwraWidget *awra_first_focusable(AwraWidget *root);
AwraWidget *awra_first_focusable(AwraWidget *root)
{
  if (!root || !(root->flags & NF_VISIBLE)) {
    return NULL;
  }
  if (awra_is_widget(root) && awra_sensitive(root)) {
    /* as the window's Tab chain: gtk_widget_set_focusable wins over the class */
    const AwraAux *a = awra_aux(root);
    if (a->focusable_set ? a->focusable :
                           (AWRA_CLASS_OF(root)->flags & (AWRA_CLASS_FOCUSABLE | AWRA_CLASS_TEXT_INPUT)) != 0)
    {
      return root;
    }
  }
  for (int i = 0; i < root->nkids; i++) {
    AwraWidget *f = awra_first_focusable(root->kids[i]);
    if (f) {
      return f;
    }
  }
  return NULL;
}

/* ======================================================================= */
/* Menu rows (spec B §12.2): 32 px, radius 9, hover @selection */

typedef struct RowExt {
  char *label, *icon, *accel;
  int check; /* -1 none */
  bool destructive;
  AwraHandler handler;
  void *data;
  int index;
  AwraWidget *popover; /* closed on activation (dropdowns; menus find theirs) */
  int kind;            /* 0 menu row, 1 dropdown row, 2 command row, 3 submenu row,
                        * 4 choice row (a field's suggestions) */
  bool selected;       /* dropdown: the current item; palette: the Enter target */
  AwraWidget *submenu; /* kind 3: the menu it opens (owned) */
} RowExt;

#define RX(w) AWRA_EXT(w, RowExt)

static AwraFont row_font(void)
{
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.weight = 520;
  return f;
}

static void row_metrics(AwraWidget *w, int *px, int *py, int *min_h, int *gap, float *radius)
{
  switch (RX(w)->kind) {
    case 1: /* dropdown row */
      *px = awra_px(10);
      *py = awra_px(7);
      *min_h = awra_px(22);
      *gap = awra_px(24);
      *radius = awra_pxf(9);
      break;
    case 2: /* command row */
      *px = awra_px(8);
      *py = awra_px(5);
      *min_h = awra_px(22);
      *gap = awra_px(12);
      *radius = awra_pxf(8);
      break;
    case 3: /* submenu row: Awra 1's awramenubutton.awra-menu-item button
             * (min-height 24, padding 6 9, radius 9), content spacing 8 */
      *px = awra_px(9);
      *py = awra_px(6);
      *min_h = awra_px(24);
      *gap = awra_px(8);
      *radius = awra_pxf(9);
      break;
    default: /* menu rows; choice rows are Awra 1's plain buttons of class
              * .awra-menu-item (the same box) holding a label */
      *px = awra_px(9);
      *py = awra_px(5);
      *min_h = awra_px(22);
      *gap = awra_px(17);
      *radius = awra_pxf(9);
  }
}

static void row_measure(AwraWidget *w, int o, int f, int *r_min, int *r_nat)
{
  RowExt *x = RX(w);
  int px, py, mh, gap;
  float rad;
  row_metrics(w, &px, &py, &mh, &gap, &rad);
  AwraFont font = row_font();
  if (x->kind == 2 || x->kind == 4) {
    font.weight = 400;
  }
  if (o == AWRA_ORIENTATION_VERTICAL) {
    *r_min = *r_nat = BK_MAX(mh, (int)ceilf(awra_font_line_height(&font))) + 2 * py;
    return;
  }
  float wv = awra_text_width(&font, x->label ? x->label : "", -1);
  if (x->icon) {
    wv += awra_px(16) + gap;
  }
  if (x->accel) {
    AwraFont af = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    af.size *= x->kind == 2 ? 0.92f : 0.86f;
    wv += awra_text_width(&af, x->accel, -1) + gap + (x->kind == 0 ? awra_px(18) : 0);
  }
  if (x->check >= 0 || x->kind == 1) {
    wv += awra_px(x->kind == 1 ? 8 : 14) + gap;
  }
  if (x->kind == 3) {
    wv += gap + awra_px(16); /* the chevron */
  }
  *r_min = awra_px(48);
  *r_nat = (int)ceilf(wv) + 2 * px;
}

static void row_draw(AwraWidget *w, AwraDraw *d)
{
  RowExt *x = RX(w);
  const AwraTokens *t = awra_tokens();
  int px, py, mh, gap;
  float rad;
  row_metrics(w, &px, &py, &mh, &gap, &rad);
  /* an insensitive widget takes no pointer in GTK: no hover */
  const bool hot = (awra_hovered(w) || awra_focus_visible(w)) && awra_sensitive(w);
  AwraRGBA bg = {0, 0, 0, 0};
  if (x->kind == 0 || x->kind == 4) {
    bg = (hot || x->selected) ? t->selection : bg;
  }
  else if (x->kind == 3) { /* .awra-open too */
    bg = (hot || x->selected || (x->submenu && awra_popover_visible(x->submenu))) ? t->selection : bg;
  }
  else if (x->kind == 1) {
    bg = awra_pressed(w) ? t->control_pressed : hot ? t->control_cap_hover :
         x->selected ? t->control_cap : bg;
  }
  else {
    bg = (hot || x->selected) ? t->control_cap_hover : bg;
  }
  AwraBox b = awra_box_of(w); /* dropdown rows: their 1 px margins are outside */
  awra_paint_rbox(d, b, rad, &bg, 0, NULL);
  const bool sens = awra_sensitive(w);
  AwraRGBA fg = x->destructive ? t->danger : t->foreground;
  if (x->kind == 4 && x->selected) { /* .awra-path-bar-suggestion-highlighted */
    fg = t->accent_content;
  }
  /* Awra 1's menu items keep their colours when their action is off
   * (.awra-menu-item and its label, icon and accelerator set them at the
   * application's priority, over the theme's :disabled): only other rows dim */
  const bool menu_item = x->kind == 0 || x->kind == 3;
  if (!sens && !menu_item) {
    fg = t->disabled;
  }
  float cx = (float)(w->x + px);
  const float cy = b.y + b.h * 0.5f;
  if (x->icon) {
    const int s = awra_px(16);
    AwraRGBA ic = t->muted;
    if (!sens && !menu_item) {
      ic.a *= 0.5f;
    }
    awra_paint_icon(d, x->icon, awra_mirror_x(w, cx, (float)s), floorf(cy - s * 0.5f), s, &ic);
    cx += s + gap;
  }
  /* laid out left to right, mirrored right to left (GtkModelButton: the
   * icon starts the row, the submenu arrow points left at its left end) */
  float right = (float)(w->x + w->w - px);
  const bool rtl = awra_widget_rtl(w);
  const float mx0 = 2.0f * w->x + w->w;
  if (x->kind == 3) {
    /* Awra 1's submenu chevron: a 1.7 px polyline, round caps, muted, in a
     * 16 px box */
    const float s = awra_scale(), lw = 1.7f * s;
    const float ccx = right - awra_pxf(8), ccy = cy;
    const AwraRGBA *c = &t->muted;
    const float k = rtl ? -1.0f : 1.0f, ox = rtl ? mx0 - ccx : ccx; /* points mirrored */
    awra_paint_line(d, ox - k * 1.5f * s, ccy - 3 * s, ox + k * 1.8f * s, ccy, lw, c);
    awra_paint_line(d, ox + k * 1.8f * s, ccy, ox - k * 1.5f * s, ccy + 3 * s, lw, c);
    awra_paint_circle(d, ox - k * 1.5f * s, ccy - 3 * s, lw * 0.5f, c);
    awra_paint_circle(d, ox - k * 1.5f * s, ccy + 3 * s, lw * 0.5f, c);
    awra_paint_circle(d, ox + k * 1.8f * s, ccy, lw * 0.5f, c);
    right -= awra_px(16) + gap;
  }
  if (x->kind == 1 && x->selected) {
    awra_paint_circle(d, rtl ? mx0 - (right - awra_pxf(4)) : right - awra_pxf(4), cy, awra_pxf(3),
                      &t->accent_content);
    right -= awra_pxf(8) + gap;
  }
  if (x->check >= 0) {
    if (x->check == 1) {
      const int s = awra_px(14);
      awra_paint_icon(d, "object-select-symbolic", awra_mirror_x(w, right - s, (float)s),
                      floorf(cy - s * 0.5f), s, &t->accent_content);
    }
    right -= awra_px(14) + gap;
  }
  if (x->accel) {
    AwraFont af = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    af.size *= x->kind == 2 ? 0.92f : 0.86f;
    const float aw = awra_text_width(&af, x->accel, -1);
    awra_draw_text(d, &af, awra_mirror_x(w, right - aw, aw),
                   floorf(cy - ceilf(awra_font_line_height(&af)) * 0.5f) + awra_font_ascent(&af), x->accel,
                   -1, &t->muted);
    right -= aw + gap;
  }
  AwraFont f = row_font();
  if (x->kind == 2 || x->kind == 4) {
    f.weight = 400;
  }
  AwraTextLayout *l = awra_text_layout_new();
  awra_text_layout_set_font(l, &f);
  awra_text_layout_set_base_dir(l, awra_widget_rtl(w));
  awra_text_layout_set_text(l, x->label ? x->label : "", false);
  awra_text_layout_set_wrap(l, AWRA_WRAP_NONE, x->kind == 4 ? AWRA_ELLIPSIZE_MIDDLE : AWRA_ELLIPSIZE_END, 1);
  int tw, th;
  const int avail = (int)(right - cx);
  awra_text_layout_size(l, avail, &tw, &th);
  float lx = cx;
  if (rtl) { /* the label's xalign flips: its text ends at the region's right */
    float lx0 = 0.0f, lw = (float)tw;
    awra_text_layout_logical(l, avail, &lx0, &lw);
    lx = awra_mirror_x(w, cx, (float)avail) + avail - fminf(ceilf(lw), (float)avail) - lx0;
  }
  /* GTK centres the label: floor ((height - logical height) / 2) */
  awra_text_layout_draw(l, d, lx, floorf(cy - th * 0.5f), avail, &fg);
  awra_text_layout_free(l);
}

static void submenu_open(AwraWidget *row, bool keyboard);

/* The popover a menu row is in, and the menu its submenu chain starts
 * from (menus moved into another popover find that one). */
static AwraWidget *row_popover(AwraWidget *row)
{
  for (AwraWidget *n = row->parent; n; n = n->parent) {
    if (awra_is_widget(n) && AWRA_CLASS_OF(n) == &awra_popover_class) {
      return n;
    }
  }
  return NULL;
}

static AwraWidget *menu_root(AwraWidget *row)
{
  AwraWidget *pop = row_popover(row);
  while (pop && POPX(pop)->submenu_row) {
    AwraWidget *up = row_popover(POPX(pop)->submenu_row);
    if (!up) {
      break;
    }
    pop = up;
  }
  return pop;
}

static void row_activate(AwraWidget *w)
{
  RowExt *x = RX(w);
  if (x->kind == 3) { /* a submenu opens; from the keyboard, the keyboard moves in */
    submenu_open(w, awra_focus_visible(w));
    return;
  }
  awra_widget_ref(w);
  AwraWidget *pop = x->kind == 0 ? menu_root(w) : x->popover;
  if (pop) {
    awra_popover_popdown(pop);
  }
  if (x->handler) {
    AwraArgs a = {0};
    a.signal = AWRA_SIGNAL_ACTIVATE;
    a.index = x->index;
    a.text = x->label;
    x->handler(w, &a, x->data);
  }
  awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, x->index);
  awra_widget_unref(w);
}

static int row_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside && awra_sensitive(w)) {
    row_activate(w);
  }
  return 1;
}

static void row_finalize(AwraWidget *w)
{
  RowExt *x = RX(w);
  free(x->label);
  free(x->icon);
  free(x->accel);
  if (x->submenu) {
    awra_popover_popdown(x->submenu);
    POPX(x->submenu)->submenu_row = NULL;
    POPX(x->submenu)->anchor = NULL;
    awra_widget_unref(x->submenu);
  }
}

/* menu items (check items), dropdown choices, palette commands */
static void row_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  RowExt *x = RX(w);
  info->label = x->label;
  info->shortcut = x->accel;
  if (x->kind == 0) {
    info->role = x->check >= 0 ? AWRA_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX : AWRA_ACCESSIBLE_ROLE_MENU_ITEM;
    if (x->check >= 0) {
      awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_CHECKED, x->check == 1);
    }
  }
  else if (x->kind == 1) {
    info->role = AWRA_ACCESSIBLE_ROLE_MENU_ITEM_RADIO;
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_CHECKED, x->selected);
  }
  else if (x->kind == 4) { /* a plain button */
    info->role = AWRA_ACCESSIBLE_ROLE_BUTTON;
  }
  else if (x->kind == 3) {
    /* a submenu row: GtkPopoverMenu's item with a submenu */
    info->role = AWRA_ACCESSIBLE_ROLE_MENU_ITEM;
    info->flags |= AWRA_A11Y_HAS_POPUP;
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_EXPANDED,
                        x->submenu && awra_popover_visible(x->submenu));
  }
  else {
    info->role = AWRA_ACCESSIBLE_ROLE_LIST_ITEM;
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_SELECTED, x->selected);
  }
}

static void row_hover(AwraWidget *w, bool on);

static AwraClass row_class = {
    .a11y = row_a11y,
    .bk = AWRA_BK("AwraMenuRow", CLS_INTERACTIVE),
    .type = "AwraMenuRow",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE,
    .ext_size = sizeof(RowExt),
    .finalize = row_finalize,
    .measure = row_measure,
    .draw = row_draw,
    .release = row_release,
    .activate = row_activate,
    .hover = row_hover,
};

static AwraWidget *row_new(int kind, const char *label, const char *icon, const char *accel,
                           int check)
{
  AwraWidget *w = awra_widget_new(&row_class);
  RowExt *x = RX(w);
  x->kind = kind;
  x->label = awra_strdup(label);
  x->icon = awra_strdup(icon);
  x->accel = awra_strdup(accel);
  x->check = check;
  return w;
}

/* a 1 px separator with 5 / 8 px margins */
static AwraWidget *menu_separator(void)
{
  AwraWidget *s = awra_separator_new(AWRA_ORIENTATION_HORIZONTAL);
  awra_widget_set_margins(s, 5, 8, 5, 8);
  return s;
}

/* ----------------------------------------------------------------------- */
/* Submenus: rows opening a nested menu beside them (Awra 1: a menu button
 * row with a chevron and a popover at GTK_POS_RIGHT). They open on click,
 * Return, Right, or after the pointer rests on them; Left and Escape close
 * them. */

#define SUBMENU_HOVER_MS 250

/* The first (or last) row of a menu that takes the keyboard. */
static void menu_rows(AwraWidget *n, AwraWidget **rows, int *count, int max)
{
  for (int i = 0; i < n->nkids && *count < max; i++) {
    AwraWidget *c = n->kids[i];
    if (!awra_is_widget(c) || !(c->flags & NF_VISIBLE)) {
      continue;
    }
    if (AWRA_CLASS_OF(c) == &row_class) {
      if (awra_sensitive(c)) {
        rows[(*count)++] = c;
      }
    }
    else {
      menu_rows(c, rows, count, max);
    }
  }
}

static void focus_row(AwraWidget *row)
{
  AwraWidget *win = awra_window_of(row);
  if (win) {
    awra_set_focus(win, row, true);
    AwraWidget *p = awra_widget_parent(row);
    while (p && !awra_widget_is(p, "AwraScrolledWindow")) {
      p = awra_widget_parent(p);
    }
    if (p) {
      awra_scrolled_scroll_to(p, row);
    }
  }
}

static void submenu_open(AwraWidget *row, bool keyboard)
{
  RowExt *x = RX(row);
  AwraWidget *sub = x->submenu;
  if (!sub || !awra_window_of(row)) {
    return;
  }
  AwraWidget *pop = row_popover(row);
  if (pop) {
    menu_hover_cancel(pop);
  }
  if (!awra_popover_visible(sub)) {
    PopExt *px = POPX(sub);
    px->submenu_row = row;
    px->anchor = row; /* not referenced: the row owns the submenu */
    px->relative = NULL;
    popup_with(sub, row);
    awra_invalidate(row);
  }
  if (keyboard) {
    AwraWidget *rows[1];
    int n = 0;
    menu_rows(sub, rows, &n, 1);
    if (n) {
      focus_row(rows[0]);
    }
  }
}

/* Closes the submenus open from `pop` (the popups after it). */
static void submenus_close(AwraWidget *pop)
{
  BKContext *ctx = bk_ctx(pop);
  if (ctx && bk_popup_index(ctx, pop) >= 0) {
    bk_close_popups_after(ctx, pop);
  }
}

/* a pending hover: both held until it fires or is cancelled */
typedef struct HoverWait {
  AwraWidget *pop, *row;
} HoverWait;

static void hover_done(void *data)
{
  HoverWait *h = (HoverWait *)data;
  awra_widget_unref(h->row);
  awra_widget_unref(h->pop);
  free(h);
}

static bool hover_fire(void *data)
{
  HoverWait *h = (HoverWait *)data;
  AwraWidget *pop = h->pop, *row = h->row;
  PopExt *x = POPX(pop);
  x->hover_timer = 0;
  x->hover_row = NULL;
  if (!x->open || !awra_hovered(row) || !within(row, pop)) {
    return false;
  }
  if (RX(row)->kind == 3) {
    submenu_open(row, false);
  }
  else {
    submenus_close(pop);
  }
  return false;
}

static void menu_hover_cancel(AwraWidget *pop)
{
  PopExt *x = POPX(pop);
  if (x->hover_timer) {
    const unsigned id = x->hover_timer;
    x->hover_timer = 0;
    x->hover_row = NULL;
    awra_source_remove(id); /* lets go of the menu and the row */
  }
}

/* The pointer resting on a row opens its submenu, or closes the submenu
 * another row of the menu has open, after a moment (crossing rows on the
 * way into a submenu changes nothing). */
static void row_hover(AwraWidget *w, bool on)
{
  RowExt *x = RX(w);
  if (x->kind != 0 && x->kind != 3) {
    return;
  }
  AwraWidget *pop = row_popover(w);
  if (!pop || !POPX(pop)->menu || !POPX(pop)->open) {
    return;
  }
  PopExt *px = POPX(pop);
  if (!on) {
    if (px->hover_row == w) {
      menu_hover_cancel(pop);
    }
    return;
  }
  menu_hover_cancel(pop);
  BKContext *ctx = bk_ctx(pop);
  const int i = ctx ? bk_popup_index(ctx, pop) : -1;
  const bool child_open = i >= 0 && i + 1 < ctx->npopups;
  if (x->kind == 3 ? (x->submenu && awra_popover_visible(x->submenu)) : !child_open) {
    return; /* already as it should be */
  }
  if (x->kind == 3 && !awra_sensitive(w)) {
    return;
  }
  HoverWait *h = (HoverWait *)malloc(sizeof(HoverWait));
  if (!h) {
    return;
  }
  h->pop = pop;
  h->row = w;
  awra_widget_ref(pop);
  awra_widget_ref(w);
  px->hover_row = w;
  px->hover_timer = awra_timeout_add(SUBMENU_HOVER_MS, hover_fire, h, hover_done);
}

/* Up / Down move between the rows, Home / End to the ends, Right opens a
 * submenu, Left closes this one (GtkPopoverMenu's keys). */
static int menu_key(AwraWidget *pop, BKEvent *e)
{
  const char *k = e->keysym;
  const bool down = bk_keysym_is(k, "Down"), up = bk_keysym_is(k, "Up");
  const bool home = bk_keysym_is(k, "Home"), end = bk_keysym_is(k, "End");
  /* right to left, submenus open with Left and close with Right */
  const bool rtl = awra_widget_rtl(pop);
  const bool right = bk_keysym_is(k, rtl ? "Left" : "Right"), left = bk_keysym_is(k, rtl ? "Right" : "Left");
  if (!down && !up && !home && !end && !right && !left) {
    return 0;
  }
  BKContext *ctx = bk_ctx(pop);
  /* the keys go to the innermost open menu */
  if (ctx && ctx->npopups && ctx->popups[ctx->npopups - 1] != pop) {
    AwraWidget *top = ctx->popups[ctx->npopups - 1];
    if (awra_is_widget(top) && AWRA_CLASS_OF(top) == &awra_popover_class && POPX(top)->menu &&
        POPX(top)->open && bk_popup_index(ctx, pop) >= 0)
    {
      return menu_key(top, e);
    }
  }
  AwraWidget *win = POPX(pop)->submenu_row ? awra_window_of(POPX(pop)->submenu_row) :
                    POPX(pop)->anchor   ? awra_window_of(POPX(pop)->anchor) :
                    POPX(pop)->relative ? awra_window_of(POPX(pop)->relative) : NULL;
  if (!win) {
    return 0;
  }
  AwraWidget *rows[256];
  int n = 0;
  menu_rows(pop, rows, &n, 256);
  AwraWidget *f = awra_focus_of(win);
  int cur = -1;
  for (int i = 0; i < n; i++) {
    if (rows[i] == f) {
      cur = i;
    }
  }
  if (left) {
    if (POPX(pop)->submenu_row) {
      AwraWidget *row = POPX(pop)->submenu_row;
      awra_popover_popdown(pop);
      focus_row(row);
      return 1;
    }
    return 0;
  }
  if (right) {
    if (cur >= 0 && RX(rows[cur])->kind == 3) {
      submenu_open(rows[cur], true);
      return 1;
    }
    return 0;
  }
  if (!n) {
    return 1;
  }
  int next;
  if (home || end) {
    next = home ? 0 : n - 1;
  }
  else if (cur < 0) {
    next = down ? 0 : n - 1;
  }
  else {
    next = (cur + (down ? 1 : -1) + n) % n;
  }
  focus_row(rows[next]);
  return 1;
}

/* The rows of items [from, to): a submenu item takes the items up to its
 * matching AWRA_MENU_ITEM_SUBMENU_END into a menu of its own. */
static AwraWidget *menu_build(const AwraMenuItem *items, int from, int to)
{
  AwraWidget *pop = awra_popover_new();
  awra_popover_set_padding_internal(pop, 7, true);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 4);
  for (int i = from; i < to; i++) {
    const AwraMenuItem *it = &items[i];
    if (it->check == AWRA_MENU_ITEM_SUBMENU_END) {
      continue; /* unmatched */
    }
    if (it->check == AWRA_MENU_ITEM_SUBMENU) {
      int depth = 1, j = i + 1;
      for (; j < to; j++) {
        depth += items[j].check == AWRA_MENU_ITEM_SUBMENU ? 1 :
                 items[j].check == AWRA_MENU_ITEM_SUBMENU_END ? -1 : 0;
        if (depth == 0) {
          break;
        }
      }
      AwraWidget *r = row_new(3, it->label ? it->label : "", it->icon_name, NULL, -1);
      RX(r)->index = i;
      RX(r)->submenu = menu_build(items, i + 1, j);
      awra_widget_set_sensitive(r, it->sensitive);
      awra_box_append(box, r);
      awra_widget_unref(r);
      i = j; /* past its end */
      continue;
    }
    if (!it->label) {
      AwraWidget *sep = menu_separator();
      awra_box_append(box, sep);
      awra_widget_unref(sep);
      continue;
    }
    if (it->check == -3) { /* a slot for the application's own widget (custom item) */
      AwraWidget *slot = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
      awra_widget_set_name(slot, it->label);
      awra_widget_set_margins(slot, 2, 8, 2, 8);
      awra_box_append(box, slot);
      awra_widget_unref(slot);
      continue;
    }
    if (it->check == -2) { /* a section heading (GtkPopoverMenu section label) */
      AwraWidget *h = awra_label_new(it->label);
      awra_widget_set_typography(h, AWRA_TYPOGRAPHY_EYEBROW);
      awra_widget_add_class(h, "dim-label");
      awra_label_set_xalign(h, 0.0f);
      awra_widget_set_margins(h, i > from ? 6 : 2, 12, 2, 12);
      awra_box_append(box, h);
      awra_widget_unref(h);
      continue;
    }
    AwraWidget *r = row_new(0, it->label, it->icon_name, it->accel, it->check);
    RX(r)->handler = it->handler;
    RX(r)->data = it->data;
    RX(r)->index = i;
    RX(r)->destructive = it->destructive;
    awra_widget_set_sensitive(r, it->sensitive);
    awra_box_append(box, r);
    awra_widget_unref(r);
  }
  AwraWidget *scroll = awra_scrolled_new();
  awra_scrolled_set_policy(scroll, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  awra_scrolled_set_propagate_natural(scroll, true, true);
  awra_scrolled_set_max_content_height(scroll, 600);
  awra_scrolled_set_child(scroll, box);
  awra_widget_unref(box);
  awra_popover_set_child(pop, scroll);
  awra_widget_unref(scroll);
  return pop;
}

AwraWidget *awra_menu_popover_new(const AwraMenuItem *items, int n)
{
  return menu_build(items, 0, n);
}

AwraWidget *awra_choice_popover_new(const char *const *labels, int n, int width,
                                    AwraHandler handler, void *data)
{
  /* Awra 1's path bar suggestions: a .awra-menu box (padding 7, no
   * spacing) of plain buttons, the box as wide as the field's content */
  AwraWidget *pop = awra_popover_new();
  awra_popover_set_padding_internal(pop, 7, false);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  for (int i = 0; i < n; i++) {
    AwraWidget *r = row_new(4, labels[i] ? labels[i] : "", NULL, NULL, -1);
    RX(r)->handler = handler;
    RX(r)->data = data;
    RX(r)->index = i;
    awra_box_append(box, r);
    awra_widget_unref(r);
  }
  /* the .awra-menu box's size request covers its padding (the popover's
   * here); its min-width, 210, only its content */
  awra_widget_set_size_request(box, BK_MAX(width - 14, 210), -1);
  awra_popover_set_child(pop, box);
  awra_widget_unref(box);
  return pop;
}

void awra_popover_set_menu(AwraWidget *popover, const AwraMenuItem *items, int n)
{
  AwraWidget *m = menu_build(items, 0, n);
  AwraWidget *content = POPX(m)->child;
  awra_widget_ref(content);
  awra_popover_set_child(m, NULL);
  awra_popover_set_child(popover, content);
  awra_widget_unref(content);
  /* a menu's insets (.awra-menu: padding 7px, min-width 210px) */
  awra_popover_set_padding_internal(popover, POPX(m)->pad, POPX(m)->menu);
  awra_widget_unref(m);
}

/* The row a keyboard elsewhere points at (path suggestions): highlighted as
 * if hovered; -1 none. */
static void highlight_rows(AwraWidget *n, int *k, int index)
{
  for (int i = 0; i < n->nkids; i++) {
    AwraWidget *c = n->kids[i];
    if (!awra_is_widget(c)) {
      continue;
    }
    if (AWRA_CLASS_OF(c) == &row_class) {
      const bool on = *k == index;
      if (RX(c)->selected != on) {
        RX(c)->selected = on;
        awra_invalidate(c);
      }
      if (on) {
        AwraWidget *p = awra_widget_parent(c);
        while (p && !awra_widget_is(p, "AwraScrolledWindow")) {
          p = awra_widget_parent(p);
        }
        if (p) {
          awra_scrolled_scroll_to(p, c);
        }
      }
      (*k)++;
    }
    else {
      highlight_rows(c, k, index);
    }
  }
}

void awra_menu_popover_set_highlighted(AwraWidget *pop, int index)
{
  int k = 0;
  highlight_rows(pop, &k, index);
}

/* ======================================================================= */
/* Menu button: an AwraButton with a chevron that opens its popover */

typedef struct MenuBtnData {
  AwraWidget *popover;
  AwraWidget *label;
  AwraWidget *chevron;
  /* gtk_menu_button_set_create_popup_func: makes the popup each time */
  AwraMenuButtonCreatePopupFunc create;
  void *create_data;
  AwraDestroyNotify create_destroy;
} MenuBtnData;

extern AwraClass awra_button_class;

/* the chevron (awra-dropdown.c:75): a 1.7 px polyline, muted */
typedef struct ChevExt {
  bool right;
} ChevExt;

static void chev_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  *mn = *nt = o == AWRA_ORIENTATION_HORIZONTAL ? awra_px(10) : awra_px(10);
}

static void chev_draw(AwraWidget *w, AwraDraw *d)
{
  const float cx = w->x + w->w * 0.5f, cy = w->y + w->h * 0.5f, s = awra_scale();
  const AwraRGBA *c = &awra_tokens()->muted;
  const float lw = 1.7f * s;
  if (AWRA_EXT(w, ChevExt)->right) {
    awra_paint_line(d, cx - 1.5f * s, cy - 3 * s, cx + 1.8f * s, cy, lw, c);
    awra_paint_line(d, cx + 1.8f * s, cy, cx - 1.5f * s, cy + 3 * s, lw, c);
  }
  else {
    awra_paint_line(d, cx - 3 * s, cy - 1.5f * s, cx, cy + 1.8f * s, lw, c);
    awra_paint_line(d, cx, cy + 1.8f * s, cx + 3 * s, cy - 1.5f * s, lw, c);
  }
  /* round caps */
  awra_paint_circle(d, cx - 3 * s, cy - 1.5f * s, lw * 0.5f, c);
  awra_paint_circle(d, cx + 3 * s, cy - 1.5f * s, lw * 0.5f, c);
}

static AwraClass chev_class = {
    .bk = AWRA_BK("AwraChevron", 0),
    .type = "AwraChevron",
    .ext_size = sizeof(ChevExt),
    .measure = chev_measure,
    .draw = chev_draw,
};

static MenuBtnData *mb(AwraWidget *w)
{
  return (MenuBtnData *)awra_widget_get_data(w, "awra-menu-button");
}

typedef struct DropData DropData;
static DropData *dd(AwraWidget *w);
static AwraWidget *drop_popover(DropData *d);

/* A button that opens a popover (menu buttons, dropdowns): has-popup and
 * expanded while it shows (button.c asks). */
void awra_popover_owner_a11y(AwraWidget *w, AwraA11yInfo *info);
void awra_popover_owner_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  MenuBtnData *m = mb(w);
  DropData *d = m ? NULL : dd(w);
  if (!m && !d) {
    return;
  }
  AwraWidget *pop = m ? m->popover : drop_popover(d);
  info->flags |= AWRA_A11Y_HAS_POPUP;
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_EXPANDED, pop && awra_popover_visible(pop));
  if (d) {
    info->role = AWRA_ACCESSIBLE_ROLE_COMBO_BOX;
  }
}

AwraWidget *awra_attached_popover(AwraWidget *w)
{
  MenuBtnData *m = (MenuBtnData *)awra_widget_get_data(w, "awra-menu-button");
  if (m && m->popover) {
    return m->popover;
  }
  return awra_widget_get_data(w, "awra-dropdown") ? drop_popover(dd(w)) : NULL;
}

/* awra.css .awra-menu-button.awra-open: a menu button whose popover shows
 * looks pressed (checked) */
bool awra_menu_button_open(const AwraWidget *b)
{
  MenuBtnData *m = (MenuBtnData *)awra_widget_get_data((AwraWidget *)b, "awra-menu-button");
  return m && m->popover && POPX(m->popover)->open;
}

static void mb_clicked(AwraWidget *b, const AwraArgs *a, void *data)
{
  MenuBtnData *m = mb(b);
  if (!m) {
    return;
  }
  if (m->popover && awra_popover_visible(m->popover)) {
    awra_popover_popdown(m->popover);
    return;
  }
  /* GtkMenuButton: the function sets the popup before each showing */
  if (m->create) {
    m->create(b, m->create_data);
  }
  if (!m->popover) {
    return;
  }
  popover_set_owner(m->popover, b);
  awra_popover_popup(m->popover);
}

static void mb_free(void *p)
{
  MenuBtnData *m = (MenuBtnData *)p;
  if (m->create_destroy) {
    m->create_destroy(m->create_data);
  }
  if (m->popover) {
    if (POPX(m->popover)->anchor_weak) {
      POPX(m->popover)->anchor = NULL; /* the button goes */
    }
    awra_widget_unref(m->popover);
  }
  free(m);
}

AwraWidget *awra_menu_button_new(void)
{
  AwraWidget *b = awra_button_new();
  MenuBtnData *m = (MenuBtnData *)calloc(1, sizeof(MenuBtnData));
  awra_widget_set_data(b, "awra-menu-button", m, mb_free);
  awra_signal_connect(b, AWRA_SIGNAL_CLICKED, mb_clicked, NULL, NULL);
  awra_button_set_icon_name(b, "open-menu-symbolic");
  awra_widget_isolate_expand(b);
  return b;
}

void awra_menu_button_set_create_popup_func(AwraWidget *b, AwraMenuButtonCreatePopupFunc func, void *data,
                                            AwraDestroyNotify destroy)
{
  MenuBtnData *m = mb(b);
  if (!m) {
    if (destroy) {
      destroy(data);
    }
    return;
  }
  if (m->create_destroy) {
    m->create_destroy(m->create_data);
  }
  m->create = func;
  m->create_data = data;
  m->create_destroy = destroy;
}

void awra_menu_button_set_icon_name(AwraWidget *b, const char *icon)
{
  awra_button_set_child(b, NULL);
  awra_button_set_icon_name(b, icon);
}

void awra_menu_button_set_label(AwraWidget *b, const char *label)
{
  MenuBtnData *m = mb(b);
  if (!m) {
    return;
  }
  if (!m->label) {
    AwraWidget *box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 17);
    m->label = awra_label_new(label);
    awra_label_set_xalign(m->label, 0.0f);
    awra_widget_set_hexpand(m->label, true);
    m->chevron = awra_widget_new(&chev_class);
    awra_box_append(box, m->label);
    awra_widget_unref(m->label); /* its parent holds it */
    awra_box_append(box, m->chevron);
    awra_widget_unref(m->chevron); /* its parent holds it */
    awra_button_set_icon_name(b, NULL);
    awra_button_set_child(b, box);
    awra_widget_unref(box); /* its parent holds it */
  }
  else {
    awra_label_set_text(m->label, label);
  }
}

void awra_menu_button_set_popover(AwraWidget *b, AwraWidget *popover)
{
  MenuBtnData *m = mb(b);
  if (!m) {
    return;
  }
  if (popover) {
    awra_widget_ref(popover);
    popover_set_owner(popover, b);
  }
  if (m->popover) {
    if (POPX(m->popover)->anchor == b && POPX(m->popover)->anchor_weak && m->popover != popover) {
      POPX(m->popover)->anchor = NULL;
    }
    awra_widget_unref(m->popover);
  }
  m->popover = popover;
}

AwraWidget *awra_menu_button_popover(AwraWidget *b)
{
  MenuBtnData *m = mb(b);
  return m ? m->popover : NULL;
}

void awra_menu_button_set_appearance(AwraWidget *b, AwraButtonAppearance a)
{
  awra_button_set_appearance(b, a);
}

void awra_menu_button_set_menu(AwraWidget *b, const AwraMenuItem *items, int n)
{
  AwraWidget *pop = awra_menu_popover_new(items, n);
  awra_menu_button_set_popover(b, pop);
  awra_widget_unref(pop);
}

/* ======================================================================= */
/* Context menus */

typedef struct CtxMenu {
  AwraMenuItem *items;
  int n;
  char **strings;
} CtxMenu;

static void ctx_free(void *p)
{
  CtxMenu *c = (CtxMenu *)p;
  for (int i = 0; i < c->n * 3; i++) {
    free(c->strings[i]);
  }
  free(c->strings);
  free(c->items);
  free(c);
}

static void ctx_pressed(AwraWidget *w, const AwraArgs *a, void *data)
{
  if (a->button != 3) {
    return;
  }
  CtxMenu *c = (CtxMenu *)awra_widget_get_data(w, "awra-context-menu");
  if (!c) {
    return;
  }
  AwraWidget *pop = awra_menu_popover_new(c->items, c->n);
  awra_popover_point_to(pop, w, a->x, a->y); /* Awra 1: pointing at the click, below it */
  awra_widget_unref(pop);
  *a->handled = true;
}

void awra_context_menu_attach(AwraWidget *w, const AwraMenuItem *items, int n)
{
  CtxMenu *c = (CtxMenu *)calloc(1, sizeof(CtxMenu));
  c->items = (AwraMenuItem *)calloc((size_t)n + 1, sizeof(AwraMenuItem));
  c->strings = (char **)calloc((size_t)n * 3 + 1, sizeof(char *));
  c->n = n;
  for (int i = 0; i < n; i++) {
    c->items[i] = items[i];
    c->strings[i * 3] = awra_strdup(items[i].label);
    c->strings[i * 3 + 1] = awra_strdup(items[i].icon_name);
    c->strings[i * 3 + 2] = awra_strdup(items[i].accel);
    c->items[i].label = c->strings[i * 3];
    c->items[i].icon_name = c->strings[i * 3 + 1];
    c->items[i].accel = c->strings[i * 3 + 2];
  }
  awra_widget_set_data(w, "awra-context-menu", c, ctx_free);
  awra_signal_connect(w, AWRA_SIGNAL_PRESSED, ctx_pressed, NULL, NULL);
}

/* ======================================================================= */
/* Dropdown (spec B §10) */

struct DropData {
  char **items;
  int n;
  int selected;
  AwraWidget *label, *popover, *list;
};

static DropData *dd(AwraWidget *w)
{
  return (DropData *)awra_widget_get_data(w, "awra-dropdown");
}

static AwraWidget *drop_popover(DropData *d)
{
  return d->popover;
}

static void drop_free(void *p)
{
  DropData *d = (DropData *)p;
  for (int i = 0; i < d->n; i++) {
    free(d->items[i]);
  }
  free(d->items);
  if (d->popover) {
    if (POPX(d->popover)->anchor_weak) {
      POPX(d->popover)->anchor = NULL; /* the dropdown goes */
    }
    awra_widget_unref(d->popover);
  }
  free(d);
}

static void drop_row_activated(AwraWidget *row, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  awra_dropdown_set_selected(w, a->index);
}

static void drop_open(AwraWidget *w, const AwraArgs *a, void *data)
{
  DropData *d = dd(w);
  if (d->popover && awra_popover_visible(d->popover)) {
    awra_popover_popdown(d->popover);
    return;
  }
  if (d->popover) {
    awra_widget_unref(d->popover);
  }
  AwraWidget *pop = awra_popover_new();
  awra_popover_set_padding_internal(pop, 6, false);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  for (int i = 0; i < d->n; i++) {
    AwraWidget *r = row_new(1, d->items[i], NULL, NULL, -1);
    RX(r)->index = i;
    RX(r)->selected = i == d->selected;
    RX(r)->popover = pop;
    RX(r)->handler = drop_row_activated;
    RX(r)->data = w;
    awra_widget_set_margins(r, 1, 0, 1, 0); /* listview > row { margin: 1px 0 } */
    awra_box_append(box, r);
    awra_widget_unref(r); /* its parent holds it */
  }
  AwraWidget *scroll = awra_scrolled_new();
  awra_scrolled_set_policy(scroll, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  awra_scrolled_set_propagate_natural(scroll, false, true);
  awra_scrolled_set_max_content_height(scroll, 280);
  awra_scrolled_set_child(scroll, box);
  awra_widget_unref(box); /* its parent holds it */
  awra_widget_set_size_request(scroll, BK_MAX(160, awra_widget_width(w)), -1);
  awra_popover_set_child(pop, scroll);
  awra_widget_unref(scroll); /* its parent holds it */
  popover_set_owner(pop, w); /* the dropdown holds it */
  d->popover = pop;
  awra_popover_popup(pop);
}

AwraWidget *awra_dropdown_new(const char *const *items, int n)
{
  AwraWidget *w = awra_button_new();
  DropData *d = (DropData *)calloc(1, sizeof(DropData));
  d->selected = -1;
  awra_widget_set_data(w, "awra-dropdown", d, drop_free);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 15);
  d->label = awra_label_new("");
  awra_label_set_xalign(d->label, 0.0f);
  awra_label_set_ellipsize(d->label, AWRA_ELLIPSIZE_END);
  awra_widget_set_hexpand(d->label, true);
  awra_box_append(box, d->label);
  awra_widget_unref(d->label); /* its parent holds it */
  AwraWidget *chevron = awra_widget_new(&chev_class);
  awra_box_append(box, chevron);
  awra_widget_unref(chevron); /* its parent holds it */
  awra_button_set_child(w, box);
  awra_widget_unref(box); /* its parent holds it */
  /* awra.css: padding 5px 11px inside a 1 px transparent border: 34 px */
  awra_button_set_padding_internal(w, 12, 6, 96);
  awra_signal_connect(w, AWRA_SIGNAL_CLICKED, drop_open, NULL, NULL);
  awra_dropdown_set_items(w, items, n);
  if (n > 0) {
    awra_dropdown_set_selected(w, 0);
  }
  awra_widget_isolate_expand(w);
  return w;
}

void awra_dropdown_set_items(AwraWidget *w, const char *const *items, int n)
{
  DropData *d = dd(w);
  if (!d) {
    return;
  }
  for (int i = 0; i < d->n; i++) {
    free(d->items[i]);
  }
  free(d->items);
  d->items = (char **)calloc((size_t)n + 1, sizeof(char *));
  d->n = n;
  for (int i = 0; i < n; i++) {
    d->items[i] = awra_strdup(items[i]);
  }
  if (d->selected >= n) {
    d->selected = n - 1;
  }
  awra_label_set_text(d->label, d->selected >= 0 ? d->items[d->selected] : "");
}

void awra_dropdown_set_selected(AwraWidget *w, int index)
{
  DropData *d = dd(w);
  if (!d || index < -1 || index >= d->n || index == d->selected) {
    return;
  }
  d->selected = index;
  awra_label_set_text(d->label, index >= 0 ? d->items[index] : "");
  awra_emit_index(w, AWRA_SIGNAL_SELECTED, index);
}

int awra_dropdown_selected(AwraWidget *w)
{
  DropData *d = dd(w);
  return d ? d->selected : -1;
}

const char *awra_dropdown_selected_text(AwraWidget *w)
{
  DropData *d = dd(w);
  return d && d->selected >= 0 ? d->items[d->selected] : NULL;
}

void awra_dropdown_set_enable_search(AwraWidget *w, bool search)
{
  (void)w;
  (void)search;
}

/* ======================================================================= */
/* Toast overlay (spec A §14) */

typedef struct Toast {
  unsigned id;
  AwraWidget *surface;
  unsigned timer;
  struct ToastTimer *timer_data; /* the timer's, freed with it */
  unsigned timeout;
  AwraHandler handler;
  void *data;
  AwraDestroyNotify destroy; /* the data's, when the toast goes */
  int running;               /* its handler runs: the data stays until it returns */
  struct Toast *next;
} Toast;

typedef struct ToastExt {
  AwraWidget *child;
  Toast *toasts;
  unsigned next_id;
  int bottom_inset; /* logical, -1: the default 24 */
} ToastExt;

#define TX(w) AWRA_EXT(w, ToastExt)

static void toast_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  if (TX(w)->child) {
    awra_measure(TX(w)->child, o, f, mn, nt);
  }
  else {
    *mn = *nt = 0;
  }
}

static void toast_allocate(AwraWidget *w)
{
  ToastExt *x = TX(w);
  if (x->child) {
    awra_allocate(x->child, w->x, w->y, w->w, w->h);
  }
  /* the newest toast, bottom center, 24 px from the bottom */
  for (Toast *t = x->toasts; t; t = t->next) {
    const bool top = t == x->toasts;
    awra_widget_set_visible(t->surface, top);
    if (!top) {
      continue;
    }
    int mn, nw, nh;
    awra_measure(t->surface, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nw);
    nw = BK_MIN(nw, w->w - awra_px(32));
    awra_measure(t->surface, AWRA_ORIENTATION_VERTICAL, nw, &mn, &nh);
    const float slide = *(float *)awra_widget_get_data(t->surface, "awra-slide");
    const int inset = awra_px(x->bottom_inset >= 0 ? x->bottom_inset : 24);
    const int y = w->y + w->h - inset - nh + (int)((1.0f - slide) * (nh + inset));
    awra_allocate(t->surface, w->x + (w->w - nw) / 2, y, nw, nh);
  }
}

void awra_toast_overlay_set_bottom_inset(AwraWidget *w, int inset)
{
  if (TX(w)->bottom_inset != inset) {
    TX(w)->bottom_inset = inset;
    awra_queue_allocate(w);
  }
}

/* Toasts still up when the overlay goes: their timers stop (their surfaces
 * go with the overlay's children). */
static void toast_overlay_finalize(AwraWidget *w)
{
  ToastExt *x = TX(w);
  while (x->toasts) {
    Toast *t = x->toasts;
    x->toasts = t->next;
    if (t->timer) {
      awra_source_remove(t->timer);
      free(t->timer_data);
    }
    if (t->destroy) {
      t->destroy(t->data);
    }
    free(t);
  }
}

static AwraWidget *toast_bin_child(AwraWidget *w)
{
  return TX(w)->child;
}

static AwraClass toast_overlay_class = {
    .bk = AWRA_BK("AwraToastOverlay", 0),
    .type = "AwraToastOverlay",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(ToastExt),
    .finalize = toast_overlay_finalize,
    .measure = toast_measure,
    .allocate = toast_allocate,
    .bin_child = toast_bin_child,
};

/* the toast's surface: opaque Floating, radius 17 */
typedef struct ToastSurfExt {
  AwraWidget *box;
} ToastSurfExt;

static void ts_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  awra_measure(AWRA_EXT(w, ToastSurfExt)->box, o, f, mn, nt);
}

static void ts_allocate(AwraWidget *w)
{
  awra_allocate(AWRA_EXT(w, ToastSurfExt)->box, w->x, w->y, w->w, w->h);
}

static void ts_draw(AwraWidget *w, AwraDraw *d)
{
  AwraResolvedMaterial m;
  awra_material_resolve(AWRA_MATERIAL_FLOATING, AWRA_ROLE_POPOVER, AWRA_ELEVATION_FLOATING, false, &m);
  awra_paint_material(d, awra_box_of(w), &m, awra_pxf(17), 15, AWRA_EDGE_ALL, AWRA_ELEVATION_FLOATING);
}

static AwraClass toast_surface_class = {
    .bk = AWRA_BK("AwraToast", 0),
    .type = "AwraToast",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(ToastSurfExt),
    .measure = ts_measure,
    .allocate = ts_allocate,
    .draw = ts_draw,
};

AwraWidget *awra_toast_overlay_new(void)
{
  AwraWidget *w = awra_widget_new(&toast_overlay_class);
  TX(w)->next_id = 1;
  TX(w)->bottom_inset = -1;
  return w;
}

void awra_toast_overlay_set_child(AwraWidget *w, AwraWidget *child)
{
  ToastExt *x = TX(w);
  if (x->child == child) {
    return;
  }
  if (x->child) {
    awra_child_remove(w, x->child);
  }
  x->child = child;
  if (child) {
    awra_child_insert(w, child, 0);
  }
}

static void toast_remove(AwraWidget *w, Toast *t)
{
  ToastExt *x = TX(w);
  for (Toast **p = &x->toasts; *p; p = &(*p)->next) {
    if (*p == t) {
      *p = t->next;
      break;
    }
  }
  if (t->timer) {
    awra_source_remove(t->timer);
    free(t->timer_data);
  }
  awra_child_remove(w, t->surface);
  if (t->running) {
    t->running = -1; /* toast_action frees it once the handler returns */
  }
  else {
    if (t->destroy) {
      t->destroy(t->data);
    }
    free(t);
  }
  awra_queue_allocate(w);
  awra_invalidate(w);
}

typedef struct ToastTimer {
  AwraWidget *overlay;
  unsigned id;
} ToastTimer;

static bool toast_expire(void *data)
{
  ToastTimer *tt = (ToastTimer *)data;
  for (Toast *t = TX(tt->overlay)->toasts; t; t = t->next) {
    if (t->id == tt->id) {
      t->timer = 0;
      /* paused while the pointer is over it */
      if (awra_aux(t->surface)->hovered) {
        t->timer = awra_timeout_add(t->timeout, toast_expire, tt, NULL);
        return false;
      }
      toast_remove(tt->overlay, t);
      break;
    }
  }
  free(tt);
  return false;
}

static void slide_anim(AwraWidget *s, double t, void *data)
{
  *(float *)awra_widget_get_data(s, "awra-slide") = (float)t;
  awra_queue_allocate(s->parent ? s->parent : s);
  awra_invalidate(s);
}

static void toast_action(AwraWidget *b, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  const unsigned id = (unsigned)(uintptr_t)awra_widget_get_data(b, "awra-toast-id");
  for (Toast *t = TX(w)->toasts; t; t = t->next) {
    if (t->id == id) {
      awra_widget_ref(w);
      if (t->handler) {
        AwraArgs args = {0};
        args.signal = AWRA_SIGNAL_ACTIVATE;
        t->running = 1;
        t->handler(w, &args, t->data);
      }
      if (t->running == -1) { /* the handler dismissed it */
        if (t->destroy) {
          t->destroy(t->data);
        }
        free(t);
      }
      else {
        t->running = 0;
        toast_remove(w, t);
      }
      awra_widget_unref(w);
      return;
    }
  }
}

unsigned awra_toast_overlay_show_with_action(AwraWidget *w, const char *title, const char *action,
                                             AwraHandler handler, void *data, unsigned timeout)
{
  return awra_toast_overlay_show_with_action_full(w, title, action, handler, data, NULL, timeout);
}

unsigned awra_toast_overlay_show_with_action_full(AwraWidget *w, const char *title, const char *action,
                                                  AwraHandler handler, void *data,
                                                  AwraDestroyNotify destroy, unsigned timeout)
{
  ToastExt *x = TX(w);
  Toast *t = (Toast *)calloc(1, sizeof(Toast));
  t->id = x->next_id++;
  t->timeout = timeout;
  t->handler = handler;
  t->data = data;
  t->destroy = destroy;
  t->surface = awra_widget_new(&toast_surface_class);
  float *slide = (float *)malloc(sizeof(float));
  *slide = awra_style_reduced_motion() ? 1.0f : 0.0f;
  awra_widget_set_data(t->surface, "awra-slide", slide, free);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 2);
  awra_widget_set_margins(box, 0, 4, 0, 0);
  AwraWidget *label = awra_label_new(title);
  awra_label_set_wrap(label, AWRA_WRAP_WORD);
  awra_label_set_max_width_chars(label, 60);
  awra_label_set_xalign(label, 0.0f);
  awra_label_set_weight(label, 600);
  awra_widget_set_margins(label, 8, 16, 8, 16);
  awra_box_append(box, label);
  awra_widget_unref(label); /* its parent holds it */
  if (action) {
    AwraWidget *b = awra_button_new_with_label(action);
    awra_button_set_appearance(b, AWRA_BUTTON_GHOST);
    awra_widget_set_valign(b, AWRA_ALIGN_CENTER);
    awra_widget_add_class(b, "toast-action");
    awra_widget_set_data(b, "awra-toast-id", (void *)(uintptr_t)t->id, NULL);
    awra_signal_connect(b, AWRA_SIGNAL_CLICKED, toast_action, w, NULL);
    awra_box_append(box, b);
    awra_widget_unref(b); /* its parent holds it */
  }
  AWRA_EXT(t->surface, ToastSurfExt)->box = box;
  awra_child_insert(t->surface, box, -1);
  awra_widget_unref(box); /* its parent holds it */
  /* newest first */
  t->next = x->toasts;
  x->toasts = t;
  awra_child_insert(w, t->surface, -1);
  awra_widget_unref(t->surface); /* its parent holds it */
  awra_accessible_announce(t->surface, title, AWRA_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
  if (timeout > 0) {
    ToastTimer *tt = (ToastTimer *)malloc(sizeof(ToastTimer));
    tt->overlay = w;
    tt->id = t->id;
    t->timer = awra_timeout_add(timeout, toast_expire, tt, NULL);
    t->timer_data = tt;
  }
  awra_anim_start(t->surface, awra_motion_duration(AWRA_MOTION_NORMAL), slide_anim, NULL, NULL);
  awra_queue_allocate(w);
  return t->id;
}

unsigned awra_toast_overlay_show(AwraWidget *w, const char *title, unsigned timeout)
{
  return awra_toast_overlay_show_with_action(w, title, NULL, NULL, NULL, timeout);
}

void awra_toast_overlay_dismiss(AwraWidget *w, unsigned id)
{
  for (Toast *t = TX(w)->toasts; t; t = t->next) {
    if (t->id == id) {
      toast_remove(w, t);
      return;
    }
  }
}

/* ======================================================================= */
/* Command palette (spec A §18): Floating panel, header, search, list */

typedef struct PaletteData {
  AwraCommand *commands;
  int n;
  char **strings;
  AwraWidget *popover, *entry, *list, *empty, *window, *header;
  AwraWidget *column; /* the popover's, which holds the palette while open */
  char *title;
  int current; /* index in commands of the Enter target */
} PaletteData;

static PaletteData *pd(AwraWidget *w)
{
  return (PaletteData *)awra_widget_get_data(w, "awra-palette");
}

static bool icontains(const char *hay, const char *needle)
{
  if (!needle[0]) {
    return true;
  }
  const size_t n = strlen(needle);
  for (const char *h = hay; *h; h++) {
    size_t i = 0;
    while (i < n && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
      i++;
    }
    if (i == n) {
      return true;
    }
  }
  return false;
}

static void palette_activate_row(AwraWidget *row, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  PaletteData *p = pd(w);
  const int idx = a->index;
  awra_widget_ref(w);
  awra_command_palette_close(w);
  awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, idx);
  awra_widget_unref(w);
  (void)p;
}

static void palette_fill(AwraWidget *w)
{
  PaletteData *p = pd(w);
  const char *q = p->entry ? awra_entry_text(p->entry) : "";
  awra_container_remove_all(p->list);
  const char *section = NULL;
  int shown = 0;
  p->current = -1;
  for (int i = 0; i < p->n; i++) {
    const AwraCommand *c = &p->commands[i];
    if (!icontains(c->title, q) && !(c->subtitle && icontains(c->subtitle, q))) {
      continue;
    }
    if (c->section && (!section || strcmp(section, c->section) != 0)) {
      section = c->section;
      AwraWidget *h = awra_label_new(section);
      awra_widget_set_typography(h, AWRA_TYPOGRAPHY_EYEBROW);
      awra_label_set_xalign(h, 0.0f);
      awra_widget_set_margins(h, 12, 8, 4, 8);
      awra_box_append(p->list, h);
      awra_widget_unref(h); /* its parent holds it */
    }
    AwraWidget *r = row_new(2, c->title, c->icon_name, c->accel, -1);
    RX(r)->index = i;
    RX(r)->handler = palette_activate_row;
    RX(r)->data = w;
    if (p->current < 0) {
      p->current = i;
      RX(r)->selected = true;
    }
    awra_box_append(p->list, r);
    awra_widget_unref(r); /* its parent holds it */
    shown++;
  }
  awra_widget_set_visible(p->empty, shown == 0);
}

static void palette_search(AwraWidget *e, const AwraArgs *a, void *data)
{
  palette_fill((AwraWidget *)data);
}

static void palette_enter(AwraWidget *e, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  PaletteData *p = pd(w);
  if (p->current >= 0) {
    AwraArgs args = {0};
    args.index = p->current;
    palette_activate_row(NULL, &args, w);
  }
}

static void palette_escape(AwraWidget *e, const AwraArgs *a, void *data)
{
  (void)e;
  (void)a;
  awra_command_palette_close((AwraWidget *)data);
}

static void palette_free(void *v)
{
  PaletteData *p = (PaletteData *)v;
  for (int i = 0; i < p->n * 5; i++) {
    free(p->strings[i]);
  }
  free(p->strings);
  free(p->commands);
  free(p->title);
  if (p->popover) {
    awra_widget_unref(p->popover);
  }
  free(p);
}

AwraWidget *awra_command_palette_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_VERTICAL, 12); /* the palette's content */
  PaletteData *p = (PaletteData *)calloc(1, sizeof(PaletteData));
  awra_widget_set_data(w, "awra-palette", p, palette_free);
  awra_widget_set_margins(w, 16, 16, 16, 16);
  p->entry = awra_search_entry_new();
  awra_entry_set_placeholder(p->entry, "Search commands");
  awra_search_entry_set_delay(p->entry, 0);
  awra_signal_connect(p->entry, AWRA_SIGNAL_CHANGED, palette_search, w, NULL);
  awra_signal_connect(p->entry, AWRA_SIGNAL_ACTIVATE, palette_enter, w, NULL);
  /* Escape closes it, whatever was typed (Awra 1's palette) */
  awra_search_entry_set_escape_stops(p->entry, true);
  awra_signal_connect(p->entry, AWRA_SIGNAL_STOP_SEARCH, palette_escape, w, NULL);
  awra_box_append(w, p->entry);
  awra_widget_unref(p->entry); /* its parent holds it */
  AwraWidget *scroll = awra_scrolled_new();
  awra_scrolled_set_policy(scroll, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  awra_widget_set_vexpand(scroll, true);
  p->list = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_scrolled_set_child(scroll, p->list);
  awra_widget_unref(p->list); /* its parent holds it */
  awra_box_append(w, scroll);
  awra_widget_unref(scroll); /* its parent holds it */
  p->empty = awra_label_new("No matching command");
  awra_widget_set_typography(p->empty, AWRA_TYPOGRAPHY_MUTED);
  awra_widget_set_visible(p->empty, false);
  awra_box_append(w, p->empty);
  awra_widget_unref(p->empty); /* its parent holds it */
  return w;
}

void awra_command_palette_set_commands(AwraWidget *w, const AwraCommand *cmds, int n)
{
  PaletteData *p = pd(w);
  for (int i = 0; i < p->n * 5; i++) {
    free(p->strings[i]);
  }
  free(p->strings);
  free(p->commands);
  p->commands = (AwraCommand *)calloc((size_t)n + 1, sizeof(AwraCommand));
  p->strings = (char **)calloc((size_t)n * 5 + 1, sizeof(char *));
  p->n = n;
  for (int i = 0; i < n; i++) {
    char **s = p->strings + i * 5;
    s[0] = awra_strdup(cmds[i].title);
    s[1] = awra_strdup(cmds[i].subtitle);
    s[2] = awra_strdup(cmds[i].icon_name);
    s[3] = awra_strdup(cmds[i].accel);
    s[4] = awra_strdup(cmds[i].section);
    p->commands[i] = (AwraCommand){s[0], s[1], s[2], s[3], s[4]};
  }
  palette_fill(w);
}

void awra_command_palette_set_placeholder(AwraWidget *w, const char *text)
{
  awra_entry_set_placeholder(pd(w)->entry, text);
}

void awra_command_palette_set_title(AwraWidget *w, const char *title)
{
  PaletteData *p = pd(w);
  awra_set_str(&p->title, title);
  if (p->header) {
    awra_header_set_title(p->header, title ? title : "Commands");
  }
}

void awra_command_palette_set_empty_text(AwraWidget *w, const char *text)
{
  awra_label_set_text(pd(w)->empty, text ? text : "No matching command");
}

void awra_command_palette_set_query(AwraWidget *w, const char *query)
{
  awra_entry_set_text(pd(w)->entry, query ? query : "");
}

/* Closed, the palette leaves its popover's column: the palette holds the
 * popover, so the popover must not hold it back. */
static void palette_hidden(AwraWidget *pop, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  PaletteData *p = pd(w);
  if (p && p->column && w->parent == p->column) {
    awra_container_remove(p->column, w);
  }
}

void awra_command_palette_present(AwraWidget *w, AwraWidget *window)
{
  PaletteData *p = pd(w);
  if (!p->popover) {
    p->popover = awra_popover_new();
    awra_popover_set_centered_internal(p->popover, true);
    /* the palette window: default header ("Commands" + close), 560 x 460 */
    AwraWidget *col = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
    AwraWidget *h = awra_header_new();
    awra_header_set_title(h, p->title ? p->title : "Commands");
    p->header = h;
    /* Awra 1's palette is an AwraWindow (.awra-command-palette, not
     * .awra-dialog): the full 74 px header, with only its close button */
    awra_header_set_close_only(h);
    awra_header_set_drag_enabled(h, false);
    awra_box_append(col, h);
    awra_widget_unref(h); /* held by the column (and p->header, borrowed) */
    awra_widget_set_size_request(col, 560, 460);
    awra_popover_set_child(p->popover, col);
    awra_widget_unref(col);
    p->column = col;
    awra_signal_connect(p->popover, AWRA_SIGNAL_HIDE, palette_hidden, w, NULL);
  }
  if (!w->parent) {
    awra_box_append(p->column, w);
  }
  p->window = window;
  awra_entry_set_text(p->entry, "");
  palette_fill(w);
  AwraWidget *anchor = awra_window_child(window) ? awra_window_child(window) : window;
  awra_popover_set_anchor(p->popover, anchor);
  awra_popover_popup(p->popover);
  awra_set_focus(window, p->entry, false);
}

void awra_command_palette_close(AwraWidget *w)
{
  PaletteData *p = pd(w);
  if (p->popover) {
    awra_popover_popdown(p->popover);
  }
}

/* ======================================================================= */
/* Dialogs: separate native windows (Awra 1's AwraDialog is a window) */

typedef struct DialogData {
  AwraWidget *window, *body, *responses, *child, *body_label;
  int content_width;
} DialogData;

static DialogData *dlg(AwraWidget *w)
{
  return (DialogData *)awra_widget_get_data(w, "awra-dialog");
}

AwraWidget *awra_dialog_new(const char *title)
{
  AwraWidget *win = awra_window_new(title, 440, -1);
  DialogData *d = (DialogData *)calloc(1, sizeof(DialogData));
  awra_widget_set_data(win, "awra-dialog", d, free);
  d->window = win;
  awra_window_set_resizable(win, false);
  awra_window_set_modal(win, true); /* Awra 1's AwraDialog is modal */
  awra_window_set_keep_rounded_corners(win, true);
  awra_window_set_floating_internal(win, true);
  AwraWidget *h = awra_window_titlebar(win);
  awra_header_set_dialog(h, true);
  d->body = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  d->responses = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 8);
  awra_widget_set_margins(d->responses, 16, 16, 16, 16);
  awra_widget_set_halign(d->responses, AWRA_ALIGN_END);
  awra_widget_set_visible(d->responses, false);
  awra_box_append(d->body, d->responses);
  awra_widget_unref(d->responses); /* its parent holds it */
  awra_window_set_child(win, d->body);
  awra_widget_unref(d->body); /* its parent holds it */
  return win;
}

void awra_dialog_set_child(AwraWidget *w, AwraWidget *child)
{
  DialogData *d = dlg(w);
  if (!d) {
    return;
  }
  if (d->child) {
    awra_box_remove(d->body, d->child);
  }
  d->child = child;
  if (child) {
    awra_widget_set_vexpand(child, true);
    awra_box_prepend(d->body, child);
  }
}

void awra_dialog_set_body(AwraWidget *w, const char *text)
{
  DialogData *d = dlg(w);
  if (!d) {
    return;
  }
  AwraWidget *l = awra_label_new(text);
  awra_label_set_wrap(l, AWRA_WRAP_WORD);
  awra_label_set_xalign(l, 0.0f);
  awra_widget_set_margins(l, 4, 16, 0, 16);
  awra_dialog_set_child(w, l);
  awra_widget_unref(l); /* its parent holds it */
}

static void dialog_response(AwraWidget *b, const AwraArgs *a, void *data)
{
  AwraWidget *win = (AwraWidget *)data;
  const int response = (int)(intptr_t)awra_widget_get_data(b, "awra-response");
  awra_emit_index(win, AWRA_SIGNAL_SELECTED, response);
}

void awra_dialog_add_response(AwraWidget *w, int response, const char *label,
                              AwraButtonAppearance appearance)
{
  DialogData *d = dlg(w);
  if (!d) {
    return;
  }
  AwraWidget *b = awra_button_new_with_label(label);
  awra_button_set_appearance(b, appearance);
  awra_widget_set_data(b, "awra-response", (void *)(intptr_t)response, NULL);
  awra_signal_connect(b, AWRA_SIGNAL_CLICKED, dialog_response, w, NULL);
  awra_box_append(d->responses, b);
  awra_widget_unref(b); /* its parent holds it */
  awra_widget_set_visible(d->responses, true);
}

static AwraWidget *response_button(AwraWidget *w, int response)
{
  DialogData *d = dlg(w);
  for (int i = 0; d && i < d->responses->nkids; i++) {
    AwraWidget *b = d->responses->kids[i];
    if ((int)(intptr_t)awra_widget_get_data(b, "awra-response") == response) {
      return b;
    }
  }
  return NULL;
}

void awra_dialog_set_response_appearance(AwraWidget *w, int response, AwraButtonAppearance appearance)
{
  AwraWidget *b = response_button(w, response);
  if (b) {
    awra_button_set_appearance(b, appearance);
  }
}

void awra_dialog_set_response_enabled(AwraWidget *w, int response, bool enabled)
{
  AwraWidget *b = response_button(w, response);
  if (b) {
    awra_widget_set_sensitive(b, enabled);
  }
}

void awra_dialog_set_response_label(AwraWidget *w, int response, const char *label)
{
  AwraWidget *b = response_button(w, response);
  if (b) {
    awra_button_set_label(b, label);
  }
}

/* A response at work: its button holds a spinner and takes no clicks. */
void awra_dialog_set_response_busy(AwraWidget *w, int response, bool busy)
{
  AwraWidget *b = response_button(w, response);
  if (!b) {
    return;
  }
  char *label = (char *)awra_widget_get_data(b, "awra-response-label");
  if (busy && !label) {
    awra_widget_set_data(b, "awra-response-label", awra_strdup(awra_button_label(b)), free);
    AwraWidget *s = awra_spinner_new();
    awra_spinner_set_spinning(s, true);
    awra_button_set_child(b, s);
    awra_widget_unref(s);
  }
  else if (!busy && label) {
    awra_button_set_label(b, label);
    awra_widget_set_data(b, "awra-response-label", NULL, NULL);
  }
  awra_widget_set_can_target(b, !busy);
}

static void dialog_default(AwraWidget *win, const AwraArgs *a, void *data)
{
  (void)a;
  const int response = (int)(intptr_t)data;
  AwraWidget *b = response_button(win, response);
  if (b && awra_widget_sensitive(b)) {
    awra_emit_index(win, AWRA_SIGNAL_SELECTED, response);
  }
}

/* Return gives the default response. */
void awra_dialog_set_default_response(AwraWidget *w, int response)
{
  awra_window_add_shortcut(w, "Return", dialog_default, (void *)(intptr_t)response, NULL);
  awra_window_add_shortcut(w, "KP_Enter", dialog_default, (void *)(intptr_t)response, NULL);
}

void awra_dialog_present(AwraWidget *w, AwraWidget *parent)
{
  if (parent && !awra_window_get_transient_for(w)) {
    awra_window_set_transient_for(w, parent);
  }
  awra_window_present(w);
}

void awra_dialog_close(AwraWidget *w)
{
  awra_window_close(w);
}

void awra_dialog_set_content_width(AwraWidget *w, int width)
{
  awra_window_set_default_size(w, width, -1);
}

/* ======================================================================= */
/* Tooltips (spec B §21): radius 8, @floating, white text, 6 / 10 px */

static AwraFont tip_font(void)
{
  return awra_font_for(AWRA_TYPOGRAPHY_BODY);
}

static void tip_measure(BKContext *ctx, const char *text, int *r_w, int *r_h)
{
  AwraTextLayout *l = awra_text_layout_new();
  AwraFont f = tip_font();
  awra_text_layout_set_font(l, &f);
  awra_text_layout_set_base_dir(l, awra_widget_rtl(NULL));
  awra_text_layout_set_text(l, text, false);
  awra_text_layout_set_wrap(l, AWRA_WRAP_WORD, AWRA_ELLIPSIZE_NONE, -1);
  const int maxw = (int)ceilf(awra_text_width(&f, "abcdefghijklmnopqrstuvwxyz", 26) / 26.0f * 50.0f);
  int tw, th;
  awra_text_layout_size(l, maxw, &tw, &th);
  awra_text_layout_free(l);
  *r_w = tw + awra_px(20);
  *r_h = th + awra_px(12);
}

static void tip_draw(BKContext *ctx, const char *text, const BKRect *rect)
{
  AwraDraw d = {bk_draw.r, 1.0f};
  AwraBox b = {(float)rect->xmin, (float)rect->ymin, (float)bk_rw(rect), (float)bk_rh(rect)};
  AwraRGBA bg = awra_tokens()->material_tint[AWRA_MATERIAL_FLOATING];
  bg.a = 1.0f; /* drawn over the window's content: the opaque fallback */
  awra_paint_rbox(&d, b, awra_pxf(8), &bg, 0, NULL);
  AwraTextLayout *l = awra_text_layout_new();
  AwraFont f = tip_font();
  awra_text_layout_set_font(l, &f);
  awra_text_layout_set_base_dir(l, awra_widget_rtl(NULL));
  awra_text_layout_set_text(l, text, false);
  awra_text_layout_set_wrap(l, AWRA_WRAP_WORD, AWRA_ELLIPSIZE_NONE, -1);
  const AwraRGBA fg = awra_tokens()->dark ? awra_rgba(1, 1, 1, 1) : awra_tokens()->foreground;
  awra_text_layout_draw(l, &d, b.x + awra_px(10), b.y + awra_px(6), (int)b.w - awra_px(20), &fg);
  awra_text_layout_free(l);
}

void awra_tooltips_install(void);
void awra_tooltips_install(void)
{
  bk_tooltip_hooks.measure = tip_measure;
  bk_tooltip_hooks.draw = tip_draw;
}
