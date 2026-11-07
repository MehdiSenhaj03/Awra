/* BUI-Awra: popups on Wayland (included by kit_window_wayland.c).
 *
 * As GDK's Wayland backend: each popup is an xdg_popup whose positioner is
 * built from the layout (gdk/wayland/gdkpopup-wayland.c: anchor rectangle
 * in the parent's window geometry, anchor and gravity from the gravities,
 * constraint adjustments from the hints, reactive), the compositor places
 * it against the output and answers with its box (configure); a nested
 * popup's parent is the popup it opened from; an autohide popup grabs
 * (xdg_popup.grab with the last input's serial) and popup_done closes it.
 * Moved or resized while shown: xdg_popup.reposition. Its frames are shm
 * buffers drawn where they changed (one, a second while the compositor
 * holds the first), paced by its own frame callbacks; blur behind its
 * rounded box as the window's.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

typedef struct WPop {
  BKWindow *win;
  int slot;
  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_popup *popup;
  struct wp_viewport *viewport;
  struct ext_background_effect_surface_v1 *effect;
  struct org_kde_kwin_blur *kblur;
  struct wl_callback *frame_cb;
  Buffer buffers[2];
  int front;
  /* what was asked */
  BKPopupLayout layout;
  int req_w, req_h, grab;
  int parent;  /* slot of the parent popup, -1 the window */
  uint32_t token, token_seen;
  /* what the compositor gave: the box (logical, in the parent's geometry) */
  int cx, cy, cw, ch, has_configure;
  int configured; /* a configure was acked: it can show */
  /* the surface, window px */
  int ox, oy, w, h;
  BKRect box;
  int drawn, blur_dirty;
} WPop;

/* GDK's last implicit grab serial: the last button or key press */
static uint32_t latest_serial(void)
{
  return g_grab_serial;
}

static int wl_pop_place(void *data, int slot, const BKPopupLayout *l, int w, int h, int grab,
                        BKRect *r_rect);
static void wl_pop_close(void *data, int slot);

static void wl_popups_init(BKWindow *win)
{
  win->pophost.data = win;
  win->pophost.flags = 0;
  win->pophost.corner_radius = 0;
  win->pophost.place = wl_pop_place;
  win->pophost.close = wl_pop_close;
  if (win->flags & BK_WINDOW_TRANSPARENT) {
    bk_popup_host_set(win->ctx, &win->pophost);
  }
}

static WPop *pop_of_surface(struct wl_surface *s)
{
  for (BKWindow *w = g_windows; w; w = w->next) {
    for (int i = 0; w->pops && i < BK_POPUP_SLOTS; i++) {
      if (w->pops[i].surface && w->pops[i].surface == s) {
        return &w->pops[i];
      }
    }
  }
  return NULL;
}

static BKWindow *window_of_any_surface(struct wl_surface *s, WPop **r_pop)
{
  *r_pop = NULL;
  BKWindow *win = window_of_surface(s);
  if (win) {
    return win;
  }
  WPop *p = pop_of_surface(s);
  if (!p) {
    return NULL;
  }
  *r_pop = p;
  return p->win;
}

static int pop_origin_x(const WPop *p)
{
  return p ? p->ox : 0;
}
static int pop_origin_y(const WPop *p)
{
  return p ? p->oy : 0;
}

static int wl_popups_in_flight(const BKWindow *win)
{
  for (int i = 0; win->pops && i < BK_POPUP_SLOTS; i++) {
    if (win->pops[i].frame_cb) {
      return 1;
    }
  }
  return 0;
}

/* A popup can draw now: shown, no frame in flight, damage, a free buffer. */
static int wl_popups_ready(const BKWindow *win)
{
  for (int i = 0; win->pops && i < BK_POPUP_SLOTS; i++) {
    const WPop *p = &win->pops[i];
    if (p->surface && p->configured && !p->frame_cb &&
        (!p->drawn || bk_popup_needs_render(win->ctx, i)) &&
        (!p->buffers[0].busy || !p->buffers[1].busy))
    {
      return 1;
    }
  }
  return 0;
}

/* The popup slot `slot` opened from (its owner inside it): its parent
 * surface; -1 the window. The tooltip's: the popup its widget is in. */
static int parent_slot(BKWindow *win, int slot)
{
  BKContext *ctx = win->ctx;
  const BKNode *from = slot == BK_POPUP_TIP ? ctx->tip_node :
                       slot < ctx->npopups   ? ctx->popups[slot]->parent :
                                               NULL;
  for (const BKNode *n = from; n; n = n->parent) {
    const int i = bk_popup_index(ctx, n);
    if (i >= 0 && i != slot && (slot == BK_POPUP_TIP || i < slot)) {
      return win->pops && win->pops[i].popup ? i : -1;
    }
  }
  return -1;
}

static BKRect parent_box(BKWindow *win, int parent)
{
  if (parent < 0) {
    return bk_rect(0, 0, win->w, win->h);
  }
  return win->pops[parent].box;
}

static enum xdg_positioner_anchor anchor_of(int g)
{
  switch (g) {
    case BK_GRAVITY_NORTH_WEST:
      return XDG_POSITIONER_ANCHOR_TOP_LEFT;
    case BK_GRAVITY_NORTH:
      return XDG_POSITIONER_ANCHOR_TOP;
    case BK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
    case BK_GRAVITY_WEST:
      return XDG_POSITIONER_ANCHOR_LEFT;
    case BK_GRAVITY_EAST:
      return XDG_POSITIONER_ANCHOR_RIGHT;
    case BK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
    case BK_GRAVITY_SOUTH:
      return XDG_POSITIONER_ANCHOR_BOTTOM;
    case BK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
    default:
      return XDG_POSITIONER_ANCHOR_NONE;
  }
}

/* the popup's point at the anchor point: the gravity is the other way */
static enum xdg_positioner_gravity gravity_of(int g)
{
  switch (g) {
    case BK_GRAVITY_NORTH_WEST:
      return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
    case BK_GRAVITY_NORTH:
      return XDG_POSITIONER_GRAVITY_BOTTOM;
    case BK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
    case BK_GRAVITY_WEST:
      return XDG_POSITIONER_GRAVITY_RIGHT;
    case BK_GRAVITY_EAST:
      return XDG_POSITIONER_GRAVITY_LEFT;
    case BK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
    case BK_GRAVITY_SOUTH:
      return XDG_POSITIONER_GRAVITY_TOP;
    case BK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_GRAVITY_TOP_LEFT;
    default:
      return XDG_POSITIONER_GRAVITY_NONE;
  }
}

/* logical from window px, rounded outwards for sizes */
static int to_logical(double px, double s)
{
  return (int)floor(px / s + 0.5);
}

static struct xdg_positioner *make_positioner(BKWindow *win, WPop *p)
{
  const double s = win_scale(win);
  const BKPopupLayout *l = &p->layout;
  const BKRect pb = parent_box(win, p->parent);
  const int bw = p->req_w - l->shadow[0] - l->shadow[1];
  const int bh = p->req_h - l->shadow[2] - l->shadow[3];
  struct xdg_positioner *pos = xdg_wm_base_create_positioner(g_wm_base);
  xdg_positioner_set_size(pos, BK_MAX(1, (int)ceil(bw / s)), BK_MAX(1, (int)ceil(bh / s)));
  /* the anchor in the parent's geometry, inside it, never empty */
  BKRect a = l->anchor;
  a.xmin = BK_MAX(a.xmin, pb.xmin);
  a.ymin = BK_MAX(a.ymin, pb.ymin);
  a.xmax = BK_MIN(a.xmax, pb.xmax);
  a.ymax = BK_MIN(a.ymax, pb.ymax);
  int ax, ay, aw, ah;
  if (a.xmax <= a.xmin || a.ymax <= a.ymin) {
    ax = ay = 0;
    aw = ah = 1;
  }
  else {
    ax = to_logical(a.xmin - pb.xmin, s);
    ay = to_logical(a.ymin - pb.ymin, s);
    aw = BK_MAX(1, to_logical(a.xmax - pb.xmin, s) - ax);
    ah = BK_MAX(1, to_logical(a.ymax - pb.ymin, s) - ay);
  }
  xdg_positioner_set_anchor_rect(pos, ax, ay, aw, ah);
  xdg_positioner_set_offset(pos, to_logical(l->dx, s), to_logical(l->dy, s));
  xdg_positioner_set_anchor(pos, anchor_of(l->rect_anchor));
  xdg_positioner_set_gravity(pos, gravity_of(l->surface_anchor));
  uint32_t ca = 0;
  if (l->hints & BK_ANCHOR_FLIP_X) {
    ca |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
  }
  if (l->hints & BK_ANCHOR_FLIP_Y) {
    ca |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
  }
  if (l->hints & BK_ANCHOR_SLIDE_X) {
    ca |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
  }
  if (l->hints & BK_ANCHOR_SLIDE_Y) {
    ca |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
  }
  if (l->hints & BK_ANCHOR_RESIZE_X) {
    ca |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
  }
  if (l->hints & BK_ANCHOR_RESIZE_Y) {
    ca |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
  }
  xdg_positioner_set_constraint_adjustment(pos, ca);
  if (xdg_positioner_get_version(pos) >= XDG_POSITIONER_SET_REACTIVE_SINCE_VERSION) {
    xdg_positioner_set_reactive(pos); /* follows its parent when that moves */
  }
  return pos;
}

static void pop_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
  WPop *p = (WPop *)data;
  wl_callback_destroy(cb);
  if (p->frame_cb == cb) {
    p->frame_cb = NULL;
  }
}
static const struct wl_callback_listener g_pop_frame_listener = {pop_frame_done};

static void popup_configure(void *data, struct xdg_popup *xp, int32_t x, int32_t y, int32_t w,
                            int32_t h)
{
  WPop *p = (WPop *)data;
  p->cx = x;
  p->cy = y;
  p->cw = w;
  p->ch = h;
  p->has_configure = 1;
}

static void popup_done(void *data, struct xdg_popup *xp)
{
  WPop *p = (WPop *)data;
  /* the compositor dismissed it (a click elsewhere, the grab broken): it
   * and the popups above it close, as GDK's popup_done */
  bk_popup_dismissed(p->win->ctx, p->slot);
}

static void popup_repositioned(void *data, struct xdg_popup *xp, uint32_t token)
{
  ((WPop *)data)->token_seen = token;
}

static const struct xdg_popup_listener g_popup_listener = {popup_configure, popup_done,
                                                           popup_repositioned};

static void pop_set_geometry(BKWindow *win, WPop *p)
{
  const double s = win_scale(win);
  const BKPopupLayout *l = &p->layout;
  xdg_surface_set_window_geometry(
      p->xdg_surface, to_logical(l->shadow[0], s), to_logical(l->shadow[2], s),
      BK_MAX(1, (int)ceil((p->req_w - l->shadow[0] - l->shadow[1]) / s)),
      BK_MAX(1, (int)ceil((p->req_h - l->shadow[2] - l->shadow[3]) / s)));
}

static void pop_surface_configure(void *data, struct xdg_surface *xs, uint32_t serial)
{
  WPop *p = (WPop *)data;
  BKWindow *win = p->win;
  xdg_surface_ack_configure(xs, serial);
  if (!p->has_configure) {
    return;
  }
  p->has_configure = 0;
  /* the box in window px: the parent's geometry origin plus the offset */
  const double s = win_scale(win);
  const BKRect pb = parent_box(win, p->parent);
  const BKPopupLayout *l = &p->layout;
  const int bx = pb.xmin + (int)floor(p->cx * s + 0.5), by = pb.ymin + (int)floor(p->cy * s + 0.5);
  /* its size: what was asked unless the compositor changed it (resized) */
  int bw = p->req_w - l->shadow[0] - l->shadow[1], bh = p->req_h - l->shadow[2] - l->shadow[3];
  if (p->cw != (int)ceil(bw / s)) {
    bw = (int)floor(p->cw * s + 0.5);
  }
  if (p->ch != (int)ceil(bh / s)) {
    bh = (int)floor(p->ch * s + 0.5);
  }
  p->box = bk_rect(bx, by, BK_MAX(1, bw), BK_MAX(1, bh));
  const int ox = bx - l->shadow[0], oy = by - l->shadow[2];
  const int w = bw + l->shadow[0] + l->shadow[1], h = bh + l->shadow[2] + l->shadow[3];
  if (w != p->w || h != p->h) {
    p->drawn = 0; /* a new buffer size */
  }
  p->ox = ox;
  p->oy = oy;
  p->w = w;
  p->h = h;
  p->configured = 1;
  p->blur_dirty = 1;
  if (p->layout.shadow[0] || p->layout.shadow[1] || p->layout.shadow[2] || p->layout.shadow[3]) {
    /* input on the box, not on its shadow */
    struct wl_region *r = wl_compositor_create_region(g_compositor);
    wl_region_add(r, to_logical(l->shadow[0], s), to_logical(l->shadow[2], s), p->cw, p->ch);
    wl_surface_set_input_region(p->surface, r);
    wl_region_destroy(r);
  }
  bk_popup_configured(win->ctx, p->slot, p->box);
  win->render_pending = 1;
}

static const struct xdg_surface_listener g_pop_surface_listener = {pop_surface_configure};

static void pop_apply_blur(BKWindow *win, WPop *p)
{
  if (!p->blur_dirty) {
    return;
  }
  p->blur_dirty = 0;
  const double s = win_scale(win);
  const BKPopupLayout *l = &p->layout;
  const int on = l->blur;
  const int x = to_logical(p->box.xmin - p->ox, s), y = to_logical(p->box.ymin - p->oy, s);
  const int w = to_logical(bk_rw(&p->box), s), h = to_logical(bk_rh(&p->box), s);
  const int r = to_logical(l->radius, s);
  if (g_effect_mgr && g_effect_caps > 0 && (g_effect_caps & 1)) {
    if (!on && !p->effect) {
      return;
    }
    if (!p->effect) {
      p->effect = ext_background_effect_manager_v1_get_background_effect(g_effect_mgr, p->surface);
    }
    if (on) {
      struct wl_region *rg = rounded_region(x, y, w, h, r);
      ext_background_effect_surface_v1_set_blur_region(p->effect, rg);
      wl_region_destroy(rg);
    }
    else {
      ext_background_effect_surface_v1_set_blur_region(p->effect, NULL);
    }
    return;
  }
  if (g_kblur_mgr) {
    if (on) {
      if (!p->kblur) {
        p->kblur = org_kde_kwin_blur_manager_create(g_kblur_mgr, p->surface);
      }
      struct wl_region *rg = rounded_region(x, y, w, h, r);
      org_kde_kwin_blur_set_region(p->kblur, rg);
      org_kde_kwin_blur_commit(p->kblur);
      wl_region_destroy(rg);
    }
    else if (p->kblur) {
      org_kde_kwin_blur_release(p->kblur);
      p->kblur = NULL;
      org_kde_kwin_blur_manager_unset(g_kblur_mgr, p->surface);
    }
  }
}

static void pop_destroy(WPop *p)
{
  if (p->frame_cb) {
    wl_callback_destroy(p->frame_cb);
  }
  if (p->effect) {
    ext_background_effect_surface_v1_destroy(p->effect);
  }
  if (p->kblur) {
    org_kde_kwin_blur_release(p->kblur);
  }
  if (p->popup) {
    xdg_popup_destroy(p->popup);
  }
  if (p->xdg_surface) {
    xdg_surface_destroy(p->xdg_surface);
  }
  if (p->viewport) {
    wp_viewport_destroy(p->viewport);
  }
  for (int i = 0; i < 2; i++) {
    buffer_free(&p->buffers[i]);
  }
  if (p->surface) {
    if (g_pointer_pop == p) {
      /* the pointer was on it: until it enters another surface, nowhere */
      g_leave_win = g_pointer_win;
      g_pointer_win = NULL;
      g_pointer_pop = NULL;
    }
    if (g_key_pop == p) {
      g_key_pop = NULL; /* the keyboard goes back to the window (its enter follows) */
    }
    wl_surface_destroy(p->surface);
  }
  memset(p, 0, sizeof(*p));
  p->front = -1;
}

/* Creates the popup's surface and role for what it was asked. */
static int pop_create(BKWindow *win, WPop *p)
{
  if (p->parent >= 0 && !win->pops[p->parent].popup) {
    return 0;
  }
  p->surface = wl_compositor_create_surface(g_compositor);
  if (win->viewport) {
    p->viewport = wp_viewporter_get_viewport(g_viewporter, p->surface);
  }
  p->xdg_surface = xdg_wm_base_get_xdg_surface(g_wm_base, p->surface);
  xdg_surface_add_listener(p->xdg_surface, &g_pop_surface_listener, p);
  struct xdg_positioner *pos = make_positioner(win, p);
  struct xdg_surface *parent = p->parent >= 0 ? win->pops[p->parent].xdg_surface : win->xdg_surface;
  p->popup = xdg_surface_get_popup(p->xdg_surface, parent, pos);
  xdg_positioner_destroy(pos);
  pop_set_geometry(win, p); /* once it has its role */
  xdg_popup_add_listener(p->popup, &g_popup_listener, p);
  if (p->grab && g_seat) {
    xdg_popup_grab(p->popup, g_seat, latest_serial());
  }
  wl_surface_commit(p->surface); /* the first configure answers */
  return 1;
}

static int wl_pop_place(void *data, int slot, const BKPopupLayout *l, int w, int h, int grab,
                        BKRect *r_rect)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->open || !win->mapped || !win->xdg_surface) {
    return 0;
  }
  if (!win->pops) {
    win->pops = (WPop *)calloc(BK_POPUP_SLOTS, sizeof(WPop));
    if (!win->pops) {
      return 0;
    }
    for (int i = 0; i < BK_POPUP_SLOTS; i++) {
      win->pops[i].front = -1;
    }
  }
  if (!win->pop_raster) {
    win->pop_raster = BUI_raster_create(1, 1);
    BUI_raster_set_diff(win->pop_raster, 0);
  }
  WPop *p = &win->pops[slot];
  const bool same = p->popup && p->req_w == w && p->req_h == h &&
                    memcmp(&p->layout, l, sizeof(*l)) == 0;
  if (same) {
    if (p->configured) {
      *r_rect = p->box;
      return 1;
    }
    return 0;
  }
  const bool blur_changed = !p->popup || p->layout.blur != l->blur || p->layout.radius != l->radius;
  p->win = win;
  p->slot = slot;
  p->layout = *l;
  p->req_w = w;
  p->req_h = h;
  p->blur_dirty |= blur_changed;
  if (!p->popup) {
    p->grab = grab;
    p->parent = parent_slot(win, slot);
    p->front = -1;
    if (!pop_create(win, p)) {
      return 0;
    }
    return 0;
  }
  /* moved or resized while shown: the compositor places it again */
  if (xdg_popup_get_version(p->popup) >= XDG_POPUP_REPOSITION_SINCE_VERSION) {
    pop_set_geometry(win, p);
    struct xdg_positioner *pos = make_positioner(win, p);
    xdg_popup_reposition(p->popup, pos, ++p->token);
    xdg_positioner_destroy(pos);
    wl_surface_commit(p->surface);
  }
  else { /* xdg-shell < 3: shown again (GDK hides and shows it) */
    const BKPopupLayout keep = p->layout;
    const int rw = p->req_w, rh = p->req_h, g = p->grab, par = p->parent;
    pop_destroy(p);
    p->win = win;
    p->slot = slot;
    p->layout = keep;
    p->req_w = rw;
    p->req_h = rh;
    p->grab = g;
    p->parent = par;
    p->blur_dirty = 1;
    pop_create(win, p);
  }
  if (p->configured) {
    *r_rect = p->box; /* where it is until the compositor answers */
    return 1;
  }
  return 0;
}

static void wl_pop_close(void *data, int slot)
{
  BKWindow *win = (BKWindow *)data;
  if (!win->pops || !win->pops[slot].surface) {
    return;
  }
  pop_destroy(&win->pops[slot]);
  win->render_pending = 1;
  wl_display_flush(g_dpy);
}

static void wl_popups_free(BKWindow *win)
{
  if (!win->pops) {
    return;
  }
  for (int i = BK_POPUP_SLOTS - 1; i >= 0; i--) { /* children first */
    if (win->pops[i].surface) {
      pop_destroy(&win->pops[i]);
    }
  }
}

/* A buffer of the popup's size the compositor doesn't hold. */
static Buffer *pop_buffer(WPop *p)
{
  Buffer *pick = NULL;
  for (int i = 0; i < 2; i++) {
    Buffer *b = &p->buffers[i];
    if (i == p->front || b->busy) {
      continue;
    }
    /* the second only when the compositor holds the first */
    if (!b->wl && i == 1 && !p->buffers[0].wl) {
      continue;
    }
    if (b->wl && b->w == p->w && b->h == p->h) {
      return b;
    }
    if (!pick) {
      pick = b;
    }
  }
  if (pick && !buffer_alloc(pick, p->w, p->h)) {
    return NULL;
  }
  return pick;
}

static int pop_render(BKWindow *win, WPop *p)
{
  if (!p->configured || p->frame_cb) {
    return 0;
  }
  const int full_asked = !p->drawn;
  if (!full_asked && !bk_popup_needs_render(win->ctx, p->slot)) {
    return 0;
  }
  Buffer *b = pop_buffer(p);
  if (!b) {
    win->render_pending = 1; /* both held: after a release */
    return 0;
  }
  Buffer *front = p->front >= 0 ? &p->buffers[p->front] : NULL;
  int full = full_asked || !b->valid;
  if (!full) {
    repair(b, front);
    full = !b->valid;
  }
  BUI_raster_set_target(win->pop_raster, b->px, b->w, b->h);
  const int regions = bk_render_popup(win->ctx, p->slot, win->pop_raster, b->w, b->h, p->ox, p->oy,
                                      full);
  if (!regions && !full) {
    return 0;
  }
  b->valid = 1;
  const int(*rects)[4];
  int n = BUI_raster_damage(win->pop_raster, &rects);
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
  Buffer *o = &p->buffers[b == &p->buffers[0] ? 1 : 0];
  if (o->wl) {
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
  pop_apply_blur(win, p);
  wl_surface_attach(p->surface, b->wl, 0, 0);
  if (full) {
    wl_surface_damage_buffer(p->surface, 0, 0, b->w, b->h);
  }
  else {
    for (int i = 0; i < n; i++) {
      wl_surface_damage_buffer(p->surface, rects[i][0], rects[i][1], rects[i][2], rects[i][3]);
    }
  }
  const double s = win_scale(win);
  if (p->viewport) {
    wp_viewport_set_destination(p->viewport, BK_MAX(1, (int)ceil(b->w / s)),
                                BK_MAX(1, (int)ceil(b->h / s)));
  }
  else {
    wl_surface_set_buffer_scale(p->surface, win->int_scale);
  }
  p->frame_cb = wl_surface_frame(p->surface);
  wl_callback_add_listener(p->frame_cb, &g_pop_frame_listener, p);
  wl_surface_commit(p->surface);
  b->busy = 1;
  p->front = (int)(b - p->buffers);
  p->drawn = 1;
  return 1;
}

static int wl_popups_render(BKWindow *win)
{
  int committed = 0;
  for (int i = 0; win->pops && i < BK_POPUP_SLOTS; i++) {
    if (win->pops[i].surface) {
      committed += pop_render(win, &win->pops[i]);
    }
  }
  return committed;
}
