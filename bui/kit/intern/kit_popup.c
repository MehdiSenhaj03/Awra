/* BUI-Awra: popups in native surfaces of their own (see kit_intern.h).
 *
 * The layout resolution is GDK's (gdk/gdksurface.c, GTK 4.22):
 * maybe_flip_position and gdk_surface_layout_popup_helper, so a popup
 * flips, slides and shrinks against the monitor's work area exactly as a
 * GtkPopover does on X11 and Windows; on Wayland the compositor applies the
 * same rules to the xdg_positioner built from the layout.
 *
 * Each open popup (and the tooltip) has a slot: what the host was asked,
 * where the popup is, and its own damage, so a hover in a menu repaints the
 * menu's surface and nothing of the window under it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "kit_intern.h"

/* BUI_POPUP_SURFACES=0: popups drawn in their window (comparisons). */
static bool surfaces_enabled(void)
{
  static int on = -1;
  if (on < 0) {
    const char *e = getenv("BUI_POPUP_SURFACES");
    on = !(e && e[0] == '0');
  }
  return on != 0;
}

/* -------------------------------------------------------------------- */
/* GDK's layout helper */

static int x_sign(int g)
{
  switch (g) {
    case BK_GRAVITY_NORTH_WEST:
    case BK_GRAVITY_WEST:
    case BK_GRAVITY_SOUTH_WEST:
      return -1;
    case BK_GRAVITY_NORTH_EAST:
    case BK_GRAVITY_EAST:
    case BK_GRAVITY_SOUTH_EAST:
      return 1;
    default:
      return 0;
  }
}

static int y_sign(int g)
{
  switch (g) {
    case BK_GRAVITY_NORTH_WEST:
    case BK_GRAVITY_NORTH:
    case BK_GRAVITY_NORTH_EAST:
      return -1;
    case BK_GRAVITY_SOUTH_WEST:
    case BK_GRAVITY_SOUTH:
    case BK_GRAVITY_SOUTH_EAST:
      return 1;
    default:
      return 0;
  }
}

static int maybe_flip_position(int bounds_pos, int bounds_size, int rect_pos, int rect_size,
                               int surface_size, int rect_sign, int surface_sign, int offset,
                               bool flip, bool *flipped)
{
  *flipped = false;
  const int primary = rect_pos + (1 + rect_sign) * rect_size / 2 + offset -
                      (1 + surface_sign) * surface_size / 2;
  if (!flip || (primary >= bounds_pos && primary + surface_size <= bounds_pos + bounds_size)) {
    return primary;
  }
  *flipped = true;
  const int secondary = rect_pos + (1 - rect_sign) * rect_size / 2 - offset -
                        (1 - surface_sign) * surface_size / 2;
  if ((secondary >= bounds_pos && secondary + surface_size <= bounds_pos + bounds_size) ||
      primary > bounds_pos + bounds_size)
  {
    return secondary;
  }
  *flipped = false;
  return primary;
}

void bk_popup_layout_resolve(const BKPopupLayout *l, int w, int h, const BKRect *bounds,
                             BKRect *r_rect, int r_flip[2])
{
  const int bx = bounds->xmin, by = bounds->ymin, bw = bk_rw(bounds), bh = bk_rh(bounds);
  const BKRect *a = &l->anchor;
  int fw = w - l->shadow[0] - l->shadow[1], fh = h - l->shadow[2] - l->shadow[3];
  bool flipped_x, flipped_y;
  int fx = maybe_flip_position(bx, bw, a->xmin, bk_rw(a), fw, x_sign(l->rect_anchor),
                               x_sign(l->surface_anchor), l->dx,
                               (l->hints & BK_ANCHOR_FLIP_X) != 0, &flipped_x);
  int fy = maybe_flip_position(by, bh, a->ymin, bk_rh(a), fh, y_sign(l->rect_anchor),
                               y_sign(l->surface_anchor), l->dy,
                               (l->hints & BK_ANCHOR_FLIP_Y) != 0, &flipped_y);
  if (l->hints & BK_ANCHOR_SLIDE_X) {
    if (fx + fw > bx + bw) {
      fx = bx + bw - fw;
    }
    if (fx < bx) {
      fx = bx;
    }
  }
  if (l->hints & BK_ANCHOR_SLIDE_Y) {
    if (fy + fh > by + bh) {
      fy = by + bh - fh;
    }
    if (fy < by) {
      fy = by;
    }
  }
  if (l->hints & BK_ANCHOR_RESIZE_X) {
    if (fx < bx) {
      fw -= bx - fx;
      fx = bx;
    }
    if (fx + fw > bx + bw) {
      fw = bx + bw - fx;
    }
  }
  if (l->hints & BK_ANCHOR_RESIZE_Y) {
    if (fy < by) {
      fh -= by - fy;
      fy = by;
    }
    if (fy + fh > by + bh) {
      fh = by + bh - fy;
    }
  }
  *r_rect = bk_rect(fx, fy, BK_MAX(1, fw), BK_MAX(1, fh));
  if (r_flip) {
    r_flip[0] = flipped_x;
    r_flip[1] = flipped_y;
  }
}

/* -------------------------------------------------------------------- */
/* Slots */

static BKPopupState *slots(BKContext *ctx)
{
  if (!ctx->pops) {
    ctx->pops = (BKPopupState *)calloc(BK_POPUP_SLOTS, sizeof(BKPopupState));
  }
  return ctx->pops;
}

bool bk_popup_surfaced(const BKContext *ctx, int slot)
{
  return ctx && ctx->pops && slot >= 0 && slot < BK_POPUP_SLOTS && ctx->pops[slot].surfaced;
}

bool bk_window_monitor(BKContext *ctx, BKRect *r_geometry)
{
  const BKPopupHost *h = ctx ? ctx->popup_host : NULL;
  return h && h->monitor && h->monitor(h->data, r_geometry);
}

void bk_popup_host_set(BKContext *ctx, const BKPopupHost *host)
{
  ctx->popup_host = host;
  if (!host && ctx->pops) {
    /* the host's surfaces are gone with it: what is still open is drawn in
     * the window from now on */
    memset(ctx->pops, 0, sizeof(BKPopupState) * BK_POPUP_SLOTS);
    ctx->full_dirty = true;
    ctx->layout_dirty = true;
  }
}

void bk_popup_slot_reset(BKContext *ctx, int slot)
{
  if (!ctx->pops || slot < 0 || slot >= BK_POPUP_SLOTS) {
    return;
  }
  BKPopupState *ps = &ctx->pops[slot];
  if ((ps->asked || ps->surfaced) && ctx->popup_host) {
    ctx->popup_host->close(ctx->popup_host->data, slot);
  }
  memset(ps, 0, sizeof(*ps));
}

/* The window's own box: where popups go without a host. */
static BKRect window_bounds(const BKContext *ctx)
{
  return bk_rect(0, 0, BK_MAX(1, ctx->win_w), BK_MAX(1, ctx->win_h));
}

static int place_slot(BKContext *ctx, int slot, const BKPopupLayout *l, int w, int h, int grab,
                      BKRect *r_rect)
{
  if (!ctx->popup_host || !surfaces_enabled()) {
    const BKRect bounds = window_bounds(ctx);
    bk_popup_layout_resolve(l, w, h, &bounds, r_rect, NULL);
    return 0;
  }
  BKPopupState *ps = &slots(ctx)[slot];
  const bool same = ps->asked && ps->w == w && ps->h == h &&
                    memcmp(&ps->layout, l, sizeof(*l)) == 0;
  /* asked again while its place is not known: a host that could not show
   * it yet (a window not configured) does now, one waiting for the
   * compositor's answer sees the same request and waits */
  if (!same || !ps->known) {
    ps->layout = *l;
    ps->w = w;
    ps->h = h;
    ps->asked = true;
    BKRect rect;
    if (ctx->popup_host->place(ctx->popup_host->data, slot, l, w, h, grab, &rect)) {
      if (!ps->known || memcmp(&rect, &ps->rect, sizeof(rect)) != 0) {
        ps->full = true;
      }
      ps->rect = rect;
      ps->known = true;
    }
    else if (!ps->known) {
      /* until the compositor answers: where it would go unconstrained */
      const BKRect any = bk_rect(-(1 << 24), -(1 << 24), 1 << 25, 1 << 25);
      bk_popup_layout_resolve(l, w, h, &any, &ps->rect, NULL);
    }
  }
  if (!ps->surfaced) {
    ps->surfaced = true;
    ps->full = true;
  }
  *r_rect = ps->rect;
  return 1;
}

int bk_popup_layout(BKNode *popup, const BKPopupLayout *l, int w, int h, BKRect *r_rect)
{
  BKContext *ctx = bk_ctx(popup);
  const int slot = ctx ? bk_popup_index(ctx, popup) : -1;
  if (slot < 0) {
    const BKRect bounds = ctx ? window_bounds(ctx) : bk_rect(0, 0, 1 << 20, 1 << 20);
    bk_popup_layout_resolve(l, w, h, &bounds, r_rect, NULL);
    return 0;
  }
  return place_slot(ctx, slot, l, w, h, !(popup->flags & NF_NO_AUTOHIDE), r_rect);
}

int bk_tooltip_place(BKContext *ctx, const BKPopupLayout *l, int w, int h, BKRect *r_rect)
{
  return place_slot(ctx, BK_POPUP_TIP, l, w, h, 0, r_rect);
}

void bk_popup_configured(BKContext *ctx, int slot, BKRect rect)
{
  if (!ctx || slot < 0 || slot >= BK_POPUP_SLOTS) {
    return;
  }
  BKPopupState *ps = &slots(ctx)[slot];
  if (!ps->known || memcmp(&rect, &ps->rect, sizeof(rect)) != 0) {
    ps->full = true;
  }
  ps->rect = rect;
  ps->known = true;
  ctx->layout_dirty = true;
  bk_ctx_request(ctx);
}

void bk_popup_dismissed(BKContext *ctx, int slot)
{
  if (!ctx) {
    return;
  }
  if (slot == BK_POPUP_TIP) {
    bk_tooltip_hide(ctx);
    return;
  }
  if (slot >= 0 && slot < ctx->npopups) {
    bk_close_popups_after(ctx, slot > 0 ? ctx->popups[slot - 1] : NULL);
  }
}

/* -------------------------------------------------------------------- */
/* Damage */

static void popup_dirty(BKContext *ctx, BKPopupState *ps, BKRect r)
{
  if (r.xmax <= r.xmin || r.ymax <= r.ymin) {
    return;
  }
  bk_ctx_request(ctx);
  if (ps->full) {
    return;
  }
  for (int i = 0; i < ps->ndirty; i++) {
    BKRect *d = &ps->dirty[i];
    if (r.xmin <= d->xmax && r.xmax >= d->xmin && r.ymin <= d->ymax && r.ymax >= d->ymin) {
      d->xmin = BK_MIN(d->xmin, r.xmin);
      d->ymin = BK_MIN(d->ymin, r.ymin);
      d->xmax = BK_MAX(d->xmax, r.xmax);
      d->ymax = BK_MAX(d->ymax, r.ymax);
      return;
    }
  }
  if (ps->ndirty == (int)(sizeof(ps->dirty) / sizeof(ps->dirty[0]))) {
    ps->full = true; /* a small surface: redraw it whole */
    ps->ndirty = 0;
    return;
  }
  ps->dirty[ps->ndirty++] = r;
}

void bk_surface_dirty(BKContext *ctx, int slot, BKRect r)
{
  if (!ctx) {
    return;
  }
  if (bk_popup_surfaced(ctx, slot)) {
    popup_dirty(ctx, &ctx->pops[slot], r);
    return;
  }
  bk_ctx_dirty_rect(ctx, r);
}

void bk_node_dirty(BKNode *n, BKRect r)
{
  BKContext *ctx = NULL;
  BKNode *pop = NULL;
  for (BKNode *p = n; p; p = p->parent) {
    if (!pop && (p->cls->flags & CLS_POPUP)) {
      pop = p;
    }
    if (p->ctx_root) {
      ctx = p->ctx_root;
      break;
    }
  }
  if (!ctx) {
    return;
  }
  if (pop && ctx->pops) {
    const int slot = bk_popup_index(ctx, pop);
    if (bk_popup_surfaced(ctx, slot)) {
      popup_dirty(ctx, &ctx->pops[slot], r);
      return;
    }
  }
  bk_ctx_dirty_rect(ctx, r);
}

bool bk_popup_needs_render(const BKContext *ctx, int slot)
{
  if (!bk_popup_surfaced(ctx, slot)) {
    return false;
  }
  const BKPopupState *ps = &ctx->pops[slot];
  return ps->full || ps->ndirty > 0 || ctx->layout_dirty;
}

/* -------------------------------------------------------------------- */
/* Drawing */

static void draw_slot(BKContext *ctx, int slot, BKNode *node, const BKRect *rect,
                      const BKRect *surf)
{
  bk_image_pass_begin(ctx, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
  bk_image_pass_slot(slot);
  bk_clip_reset_at(surf->xmin, surf->ymin, bk_rw(surf), bk_rh(surf));
  bk_clip_push(rect->xmin, rect->ymin, bk_rw(rect), bk_rh(rect));
  BUI_raster_clear(bk_draw.r, rect->xmin, rect->ymin, bk_rw(rect), bk_rh(rect));
  if (slot == BK_POPUP_TIP) {
    bk_draw_tooltip_at(ctx);
  }
  else if (node && NODE_VISIBLE(node)) {
    bk_draw_tree(node);
  }
  bk_clip_reset_at(surf->xmin, surf->ymin, bk_rw(surf), bk_rh(surf));
  bk_image_pass_end();
}

int bk_render_popup(BKContext *ctx, int slot, BuiRaster *r, int w, int h, int ox, int oy,
                    int full)
{
  bk_ctx_prepare(ctx);
  if (!bk_popup_surfaced(ctx, slot)) {
    return 0;
  }
  BKPopupState *ps = &ctx->pops[slot];
  BKNode *node = NULL;
  if (slot != BK_POPUP_TIP) {
    if (slot >= ctx->npopups) {
      return 0;
    }
    node = ctx->popups[slot];
  }
  full = full || ps->full;
  if (!full && ps->ndirty == 0) {
    return 0;
  }
  BuiRaster *saved = bk_draw.r;
  bk_draw.r = r;
  BUI_raster_set_origin(r, ox, oy);
  BUI_raster_begin_frame(r, w, h);
  const BKRect surf = bk_rect(ox, oy, w, h);
  int regions = 0;
  if (full) {
    draw_slot(ctx, slot, node, &surf, &surf);
    BUI_raster_end_frame(r);
    regions = 1;
  }
  else {
    BKRect bbox = {0, 0, 0, 0};
    for (int i = 0; i < ps->ndirty; i++) {
      BKRect d = ps->dirty[i];
      d.xmin = BK_MAX(d.xmin, surf.xmin);
      d.ymin = BK_MAX(d.ymin, surf.ymin);
      d.xmax = BK_MIN(d.xmax, surf.xmax);
      d.ymax = BK_MIN(d.ymax, surf.ymax);
      if (d.xmax <= d.xmin || d.ymax <= d.ymin) {
        continue;
      }
      draw_slot(ctx, slot, node, &d, &surf);
      if (regions++ == 0) {
        bbox = d;
      }
      else {
        bbox.xmin = BK_MIN(bbox.xmin, d.xmin);
        bbox.ymin = BK_MIN(bbox.ymin, d.ymin);
        bbox.xmax = BK_MAX(bbox.xmax, d.xmax);
        bbox.ymax = BK_MAX(bbox.ymax, d.ymax);
      }
    }
    if (regions) {
      BUI_raster_end_frame_region(r, bbox.xmin, bbox.ymin, bk_rw(&bbox), bk_rh(&bbox));
    }
  }
  ps->full = false;
  ps->ndirty = 0;
  bk_draw.r = saved;
  return regions;
}
