/* Awra 2 rail (Awra 1's AwraRail, AwraRailItem, AwraRailGroup): a column
 * of 48 px tiles for places (servers, workspaces), round at rest and a
 * rounded square when hovered or selected, with a pill at the edge (unread,
 * hover, selected), a mention badge cut into the corner, folders that open
 * into a column, and separators. A header and a footer stay put; the
 * entries between them scroll.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

#define PRESSED_SCALE 0.94f
#define PREVIEW_COUNT 4

void awra_avatar_set_cut_internal(AwraWidget *w, bool cut, float cx, float cy, float r);
void awra_avatar_set_radius_internal(AwraWidget *w, float radius);
void awra_avatar_set_cut_rect_internal(AwraWidget *w, bool cut, float rx, float ry, float rw, float rh, float r);
void awra_paint_rbox_cut(AwraDraw *d, AwraBox b, float radius, AwraBox cut, float cut_radius,
                         const AwraRGBA *color);
static void badge_box(AwraWidget *w, float tile, float ox, float oy, AwraBox *r_box, char *text, AwraFont *r_font);

/* tile 40 + 8, inset 12, gap 8 (navigation height + spacing tokens) */
static float tile_size(void)
{
  return 40.0f + awra_spacing(AWRA_SPACING_SM);
}
static float tile_inset(void)
{
  return (float)awra_spacing(AWRA_SPACING_MD);
}
static float entry_gap(void)
{
  return (float)awra_spacing(AWRA_SPACING_SM);
}

/* ======================================================================= */
/* Items */

typedef struct ItemExt {
  char *label, *icon_name;
  AwraWidget *child;
  bool selected, unread, muted, group_header, drop_target, lifted, has_tint;
  AwraRGBA tint;
  unsigned mention_count, shown_count;
  /* presentation: pill height (logical), shape 0 round .. 1 square, scale,
   * badge scale; from -> to over an animation */
  float pill, shape, scale, badge;
  float from[4], to[4];
  AwraWidget *rail;
} RailItemExt;

#define RIX(w) AWRA_EXT(w, RailItemExt)

static float corner_radius(const RailItemExt *x, float tile)
{
  const float square = fminf((float)awra_radius(AWRA_RADIUS_LARGE), tile / 2);
  return tile / 2 + (square - tile / 2) * fminf(fmaxf(x->shape, 0.0f), 1.2f);
}

static float pill_target(AwraWidget *w)
{
  RailItemExt *x = RIX(w);
  const float tile = tile_size();
  if (x->lifted) {
    return 0.0f;
  }
  if (x->selected) {
    return roundf(tile * 0.84f);
  }
  if (awra_hovered(w) || x->drop_target) {
    return roundf(tile * 0.42f);
  }
  if (x->unread && !x->muted) {
    return roundf(tile / 5.0f);
  }
  return 0.0f;
}

static void item_anim(AwraWidget *w, double t, void *data)
{
  (void)data;
  RailItemExt *x = RIX(w);
  float *v[4] = {&x->pill, &x->shape, &x->scale, &x->badge};
  for (int i = 0; i < 4; i++) {
    *v[i] = x->from[i] + (x->to[i] - x->from[i]) * (float)t;
  }
  if (x->badge <= 0.0f && x->mention_count == 0) {
    x->shown_count = 0;
  }
  if (x->child && awra_widget_is(x->child, "AwraAvatar")) {
    awra_avatar_set_radius_internal(x->child, corner_radius(x, tile_size()));
  }
  awra_invalidate(w);
}

static void item_update(AwraWidget *w)
{
  RailItemExt *x = RIX(w);
  const bool square = x->selected || awra_hovered(w) || x->drop_target || x->lifted;
  const float to[4] = {pill_target(w), square ? 1.0f : 0.0f,
                       x->lifted ? 1.06f : awra_pressed(w) ? PRESSED_SCALE : 1.0f,
                       x->mention_count > 0 ? 1.0f : 0.0f};
  const float now[4] = {x->pill, x->shape, x->scale, x->badge};
  if (!memcmp(to, x->to, sizeof(to)) && !memcmp(now, to, sizeof(to))) {
    return;
  }
  memcpy(x->to, to, sizeof(to));
  memcpy(x->from, now, sizeof(now));
  if (x->mention_count > 0) {
    x->shown_count = x->mention_count;
  }
  awra_anim_stop_for(w);
  awra_anim_start(w, awra_aux(w)->mapped ? awra_motion_duration(AWRA_MOTION_NORMAL) : 0, item_anim, NULL, NULL);
}

static void item_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)w;
  (void)for_size;
  const float tile = tile_size();
  *r_min = *r_nat = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? tile + 2 * tile_inset() : tile);
}

static void item_allocate(AwraWidget *w)
{
  RailItemExt *x = RIX(w);
  if (x->child) {
    const int tile = awra_px(tile_size());
    awra_allocate(x->child, w->x + (w->w - tile) / 2, w->y + (w->h - tile) / 2, tile, tile);
  }
}

static void tile_colors(AwraWidget *w, AwraRGBA *bg, AwraRGBA *fg)
{
  RailItemExt *x = RIX(w);
  const AwraTokens *t = awra_tokens();
  AwraRGBA rest = t->control_cap, rest_fg = t->foreground;
  const AwraRGBA active = x->selected ? t->accent : t->accent_soft;
  const AwraRGBA active_fg = x->selected ? t->on_accent : t->accent_content;
  if (x->has_tint) {
    *bg = awra_with_alpha(x->tint, t->dark ? 0.36f : 0.28f);
    *fg = rest_fg;
    return;
  }
  if (x->group_header) {
    *bg = t->control_well;
    *fg = rest_fg;
    return;
  }
  *bg = awra_mix(rest, active, fminf(1.0f, x->shape));
  *fg = awra_mix(rest_fg, active_fg, fminf(1.0f, x->shape));
}

static void badge_box(AwraWidget *w, float tile, float ox, float oy, AwraBox *r_box, char *text, AwraFont *r_font)
{
  RailItemExt *x = RIX(w);
  const float s = awra_scale();
  const float height = roundf(tile / 3.0f);
  if (x->shown_count > 99) {
    snprintf(text, 8, "99+");
  }
  else {
    snprintf(text, 8, "%u", x->shown_count);
  }
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.size = height * 0.66f * s;
  f.weight = 700;
  f.letter_spacing = 0;
  *r_font = f;
  const float ink = awra_text_width(&f, text, -1) / s;
  const float width = fmaxf(height, ink + height * 0.7f);
  const float end = tile + 2;
  *r_box = (AwraBox){ox + (end - width) * s, oy + (tile + 2 - height) * s, width * s, height * s};
}

static void item_draw(AwraWidget *w, AwraDraw *d)
{
  RailItemExt *x = RIX(w);
  const AwraTokens *t = awra_tokens();
  const float s = awra_scale();
  const float tile = tile_size();
  const float tpx = tile * s;
  const float ox = w->x + roundf((w->w - tpx) / 2), oy = w->y + roundf((w->h - tpx) / 2);
  /* the pill at the start edge: 4 px wide, its height animated */
  const float unread = roundf(tile / 5.0f);
  if (x->pill >= 0.5f) {
    const float pw = 4.0f * fminf(1.0f, x->pill / unread) * s;
    bk_clip_push(w->x, w->y, (int)ceilf(pw), w->h);
    awra_paint_rbox(d, (AwraBox){w->x - pw, w->y + (w->h - x->pill * s) / 2, pw * 2, x->pill * s}, pw,
                    &t->foreground, 0, NULL);
    bk_clip_pop();
  }
  const float sc = x->scale;
  const float cx = ox + tpx / 2, cy = oy + tpx / 2;
  const float tw = tpx * sc;
  const AwraBox tb = {cx - tw / 2, cy - tw / 2, tw, tw};
  const float radius = corner_radius(x, tile) * s * sc;
  if (x->lifted) {
    const AwraRGBA sh = {0, 0, 0, 0.32f};
    awra_paint_shadow(d, tb, radius, 0, 6 * s, 0, 16 * s, &sh, true);
  }
  const bool dim = x->muted && !awra_hovered(w) && !x->selected;
  const float saved = d->opacity;
  if (dim) {
    d->opacity *= 0.48f;
  }
  AwraRGBA bg, fg;
  tile_colors(w, &bg, &fg);
  /* the mention badge's place, cut out of the tile with a ring around it */
  AwraBox cut = {0, 0, 0, 0};
  float cut_r = 0;
  if (x->shown_count > 0 && x->badge > 0.0f) {
    AwraBox bb;
    char text[8];
    AwraFont f;
    badge_box(w, tile, ox, oy, &bb, text, &f);
    const float ring = fmaxf(2.0f, tile / 16.0f) * awra_scale();
    const float bcx = bb.x + bb.w / 2, bcy = bb.y + bb.h / 2;
    cut = (AwraBox){bcx - (bb.w / 2 + ring) * x->badge, bcy - (bb.h / 2 + ring) * x->badge,
                    (bb.w + 2 * ring) * x->badge, (bb.h + 2 * ring) * x->badge};
    cut_r = cut.h / 2;
  }
  if (x->child && awra_widget_is(x->child, "AwraAvatar")) {
    const float s = awra_scale();
    awra_avatar_set_cut_rect_internal(x->child, cut.w > 0, (cut.x - ox) / s, (cut.y - oy) / s, cut.w / s,
                                      cut.h / s, cut_r / s);
  }
  else {
    awra_paint_rbox_cut(d, tb, radius, cut, cut_r, &bg);
  }
  if (!x->child && x->icon_name) {
    const int px = (int)roundf(tile * 0.42f * s * sc);
    awra_paint_icon(d, x->icon_name, roundf(cx - px / 2.0f), roundf(cy - px / 2.0f), px, &fg);
  }
  d->opacity = saved;
  (void)dim;
}

/* the badge (and its cutout) and rings go over the child */
static void item_draw_over(AwraWidget *w, AwraDraw *d)
{
  RailItemExt *x = RIX(w);
  const AwraTokens *t = awra_tokens();
  const float s = awra_scale();
  const float tile = tile_size();
  const float tpx = tile * s;
  const float ox = w->x + roundf((w->w - tpx) / 2), oy = w->y + roundf((w->h - tpx) / 2);
  const float radius = corner_radius(x, tile) * s;
  if (x->shown_count > 0 && x->badge > 0.0f) {
    AwraBox b;
    char text[8];
    AwraFont f;
    badge_box(w, tile, ox, oy, &b, text, &f);
    const float bcx = b.x + b.w / 2, bcy = b.y + b.h / 2;
    AwraBox bb = {bcx - b.w / 2 * x->badge, bcy - b.h / 2 * x->badge, b.w * x->badge, b.h * x->badge};
    awra_paint_rbox(d, bb, bb.h / 2, &t->danger, 0, NULL);
    f.size *= x->badge;
    const float tw = awra_text_width(&f, text, -1);
    const AwraRGBA white = {1, 1, 1, 1};
    /* Pango's ink box of digits: about 0.73 em above the baseline */
    const float ink_h = f.size * 0.729f;
    awra_draw_text(d, &f, roundf(bcx - tw / 2), roundf(bcy + ink_h / 2), text, -1, &white);
  }
  if (x->drop_target || awra_focus_visible(w)) {
    const float o = 4 * s;
    const AwraRGBA *c = x->drop_target ? &t->accent : &t->focus_ring;
    awra_paint_rbox(d, (AwraBox){ox - o, oy - o, tpx + 2 * o, tpx + 2 * o}, radius + o, NULL, 2 * s, c);
  }
}

static void item_hover(AwraWidget *w, bool on)
{
  (void)on;
  item_update(w);
}

void awra_rail_item_activated_internal(AwraWidget *rail, AwraWidget *item);

static void item_activate(AwraWidget *w)
{
  RailItemExt *x = RIX(w);
  awra_widget_ref(w);
  if (!x->group_header) {
    AwraWidget *rail = NULL;
    for (BKNode *p = w->parent; p; p = p->parent) {
      if (awra_is_widget(p) && awra_widget_is(p, "AwraRail")) {
        rail = p;
        break;
      }
    }
    if (rail) {
      awra_rail_item_activated_internal(rail, w);
    }
  }
  awra_emit_simple(w, AWRA_SIGNAL_ACTIVATE);
  awra_widget_unref(w);
}

static int item_press(AwraWidget *w, BKEvent *e)
{
  (void)e;
  item_update(w);
  return 1;
}

static int item_release(AwraWidget *w, BKEvent *e)
{
  item_update(w);
  if (e->inside && !RIX(w)->lifted) {
    item_activate(w);
  }
  return 1;
}

static void item_finalize(AwraWidget *w)
{
  RailItemExt *x = RIX(w);
  free(x->label);
  free(x->icon_name);
}

/* Awra 1's AwraRailItem: a tab named by its label, described by its
 * mentions / unread / muted state; a group's header is a button that
 * expands it */
static void item_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  RailItemExt *x = RIX(w);
  info->label = x->label;
  int o = 0;
  info->buf[0] = 0;
  if (x->mention_count > 0) {
    o = snprintf(info->buf, sizeof(info->buf), x->mention_count == 1 ? "%u mention" : "%u mentions",
                 x->mention_count);
  }
  else if (x->unread && !x->muted) {
    o = snprintf(info->buf, sizeof(info->buf), "Unread");
  }
  if (x->muted && o >= 0 && o < (int)sizeof(info->buf)) {
    snprintf(info->buf + o, sizeof(info->buf) - (size_t)o, "%sMuted", o ? ", " : "");
  }
  if (info->buf[0]) {
    info->description = info->buf;
  }
  if (x->group_header) {
    info->role = AWRA_ACCESSIBLE_ROLE_BUTTON;
    AwraWidget *g = w->parent;
    if (g && awra_widget_is(g, "AwraRailGroup")) {
      awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_EXPANDED, awra_rail_group_expanded(g));
    }
  }
  else {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_SELECTED, x->selected);
  }
}

static AwraClass item_class = {
    .a11y = item_a11y,
    .bk = AWRA_BK("AwraRailItem", CLS_INTERACTIVE),
    .type = "AwraRailItem",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE,
    .ext_size = sizeof(RailItemExt),
    .finalize = item_finalize,
    .measure = item_measure,
    .allocate = item_allocate,
    .draw = item_draw,
    .draw_over = item_draw_over,
    .press = item_press,
    .release = item_release,
    .activate = item_activate,
    .hover = item_hover,
    .overdraw = 8,
};

static AwraWidget *item_new_internal(const char *label)
{
  AwraWidget *w = awra_widget_new(&item_class);
  RailItemExt *x = RIX(w);
  x->label = label ? awra_strdup(label) : NULL;
  x->scale = 1.0f;
  x->to[2] = 1.0f;
  awra_widget_set_tooltip(w, label);
  return w;
}

AwraWidget *awra_rail_item_new(const char *label)
{
  return item_new_internal(label);
}

void awra_rail_item_set_label(AwraWidget *w, const char *label)
{
  awra_set_str(&RIX(w)->label, label);
  awra_widget_set_tooltip(w, label);
}

const char *awra_rail_item_label(AwraWidget *w)
{
  return RIX(w)->label;
}

void awra_rail_item_set_child(AwraWidget *w, AwraWidget *child)
{
  RailItemExt *x = RIX(w);
  awra_set_slot(w, &x->child, child);
  if (child && awra_widget_is(child, "AwraAvatar")) {
    awra_avatar_set_size(child, (int)tile_size());
    awra_avatar_set_shape(child, AWRA_AVATAR_SHAPE_ROUNDED);
    awra_avatar_set_radius_internal(child, corner_radius(x, tile_size()));
  }
  if (child) {
    awra_widget_set_can_target(child, false);
  }
}

AwraWidget *awra_rail_item_child(AwraWidget *w)
{
  return RIX(w)->child;
}

void awra_rail_item_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_set_str(&RIX(w)->icon_name, icon);
  awra_invalidate(w);
}

void awra_rail_item_set_selected(AwraWidget *w, bool selected)
{
  if (RIX(w)->selected != selected) {
    RIX(w)->selected = selected;
    item_update(w);
  }
}

bool awra_rail_item_selected(AwraWidget *w)
{
  return RIX(w)->selected;
}

void awra_rail_item_set_unread(AwraWidget *w, bool unread)
{
  if (RIX(w)->unread != unread) {
    RIX(w)->unread = unread;
    item_update(w);
  }
}

void awra_rail_item_set_mention_count(AwraWidget *w, unsigned count)
{
  RailItemExt *x = RIX(w);
  if (x->mention_count != count) {
    x->mention_count = count;
    if (count > 0) {
      x->shown_count = count;
    }
    item_update(w);
    awra_invalidate(w);
  }
}

void awra_rail_item_set_muted(AwraWidget *w, bool muted)
{
  if (RIX(w)->muted != muted) {
    RIX(w)->muted = muted;
    item_update(w);
    awra_invalidate(w);
  }
}

void awra_rail_item_set_tint(AwraWidget *w, const AwraRGBA *tint)
{
  RailItemExt *x = RIX(w);
  x->has_tint = tint != NULL;
  if (tint) {
    x->tint = *tint;
  }
  awra_invalidate(w);
}

void awra_rail_item_set_drop_target(AwraWidget *w, bool target)
{
  if (RIX(w)->drop_target != target) {
    RIX(w)->drop_target = target;
    item_update(w);
  }
}

/* ======================================================================= */
/* Separator: a 2 px rounded line two thirds of a tile long */

static void sep_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)w;
  (void)for_size;
  *r_min = *r_nat = o == AWRA_ORIENTATION_HORIZONTAL ? awra_px(tile_size() * 2.0f / 3.0f) : awra_px(2);
}

static void sep_draw(AwraWidget *w, AwraDraw *d)
{
  const float len = roundf(tile_size() * 2.0f / 3.0f) * awra_scale();
  AwraRGBA c = awra_tokens()->foreground;
  c.a = 0.18f;
  awra_paint_rbox(d, (AwraBox){w->x + roundf((w->w - len) / 2), (float)w->y, len, awra_pxf(2)}, awra_pxf(1), &c, 0,
                  NULL);
}

static AwraClass sep_class = {
    .bk = AWRA_BK("AwraRailSeparator", 0),
    .type = "AwraRailSeparator",
    .measure = sep_measure,
    .draw = sep_draw,
};

/* ======================================================================= */
/* Groups: a tile whose face previews four items (or shows the folder
 * icon while open), then the items in a column behind them */

typedef struct GroupExt {
  AwraWidget *header; /* a rail item */
  AwraWidget **items;
  int n_items;
  char *label, *icon_name;
  bool expanded, has_color;
  AwraRGBA color;
  float reveal;
} GroupExt;

#define GRX(w) AWRA_EXT(w, GroupExt)

static int group_items_height(AwraWidget *w)
{
  GroupExt *x = GRX(w);
  int h = 0;
  for (int i = 0; i < x->n_items; i++) {
    int mn, nt;
    awra_measure(x->items[i], AWRA_ORIENTATION_VERTICAL, -1, &mn, &nt);
    h += awra_px(entry_gap()) + mn;
  }
  return h;
}

static void group_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)for_size;
  GroupExt *x = GRX(w);
  int hm, hn;
  awra_measure(x->header, o, -1, &hm, &hn);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = *r_nat = hm;
    return;
  }
  *r_min = *r_nat = hm + (int)ceilf(group_items_height(w) * x->reveal);
}

static void group_allocate(AwraWidget *w)
{
  GroupExt *x = GRX(w);
  int hm, hn;
  awra_measure(x->header, AWRA_ORIENTATION_VERTICAL, -1, &hm, &hn);
  awra_allocate(x->header, w->x, w->y, w->w, hm);
  int y = hm;
  for (int i = 0; i < x->n_items; i++) {
    int mn, nt;
    awra_measure(x->items[i], AWRA_ORIENTATION_VERTICAL, -1, &mn, &nt);
    y += awra_px(entry_gap());
    awra_allocate(x->items[i], w->x, w->y + y, w->w, mn);
    y += mn;
  }
}

static void group_draw(AwraWidget *w, AwraDraw *d)
{
  GroupExt *x = GRX(w);
  if (x->reveal <= 0.0f) {
    return;
  }
  const AwraTokens *t = awra_tokens();
  const float tile = tile_size() * awra_scale();
  AwraRGBA c = x->has_color ? awra_with_alpha(x->color, t->dark ? 0.30f : 0.22f) : t->control_well;
  c.a *= x->reveal;
  awra_paint_rbox(d, (AwraBox){w->x + roundf((w->w - tile) / 2), (float)w->y, tile, (float)w->h}, tile / 2, &c, 0,
                  NULL);
}

/* the face: previews of the first four items, or the folder icon */
static void face_draw(AwraWidget *item, AwraDraw *d)
{
  AwraWidget *group = (AwraWidget *)item->parent;
  if (!group || !awra_widget_is(group, "AwraRailGroup")) {
    return;
  }
  GroupExt *x = GRX(group);
  const float s = awra_scale();
  const float tile = tile_size();
  const float ox = item->x + roundf((item->w - tile * s) / 2), oy = item->y + roundf((item->h - tile * s) / 2);
  const AwraRGBA fg = awra_tokens()->foreground;
  const int px = (int)roundf(tile * 0.42f * s);
  const float open = x->expanded ? x->reveal : 0.0f;
  if (open < 1.0f) {
    const float saved = d->opacity;
    d->opacity *= 1.0f - open;
    if (x->icon_name) {
      awra_paint_icon(d, x->icon_name, roundf(ox + (tile * s - px) / 2), roundf(oy + (tile * s - px) / 2), px, &fg);
    }
    else {
      const float gap = roundf(tile / 12.0f), pad = roundf(tile / 8.0f);
      const float mini = floorf((tile - 2 * pad - gap) / 2.0f);
      for (int i = 0; i < x->n_items && i < PREVIEW_COUNT; i++) {
        RailItemExt *ix = RIX(x->items[i]);
        const float mx = ox + (pad + (i % 2) * (mini + gap)) * s, my = oy + (pad + (i / 2) * (mini + gap)) * s;
        AwraWidget *child = ix->child;
        if (child && awra_widget_is(child, "AwraAvatar")) {
          /* a small copy of the item's avatar: its texture or plate */
          AwraTexture *tex = awra_avatar_texture(child);
          if (tex) {
            awra_paint_texture(d, tex, (AwraBox){mx, my, mini * s, mini * s}, mini * s / 2, 2);
            continue;
          }
        }
        AwraRGBA c = awra_tokens()->control_cap;
        awra_paint_rbox(d, (AwraBox){mx, my, mini * s, mini * s}, mini * s / 2, &c, 0, NULL);
      }
    }
    d->opacity = saved;
  }
  if (open > 0.0f) {
    const float saved = d->opacity;
    d->opacity *= open;
    awra_paint_icon(d, x->icon_name ? x->icon_name : "awra-folder-symbolic", roundf(ox + (tile * s - px) / 2),
                    roundf(oy + (tile * s - px) / 2), px, &fg);
    d->opacity = saved;
  }
}

static void face_item_draw(AwraWidget *w, AwraDraw *d)
{
  item_draw(w, d);
  face_draw(w, d);
}

static AwraClass face_item_class;

static void group_finalize(AwraWidget *w)
{
  GroupExt *x = GRX(w);
  free(x->items);
  free(x->label);
  free(x->icon_name);
}

static void group_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->label = GRX(w)->label;
}

static AwraClass group_class = {
    .a11y = group_a11y,
    .bk = AWRA_BK("AwraRailGroup", 0),
    .type = "AwraRailGroup",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP,
    .ext_size = sizeof(GroupExt),
    .finalize = group_finalize,
    .measure = group_measure,
    .allocate = group_allocate,
    .draw = group_draw,
};

static void group_reveal_anim(AwraWidget *w, double t, void *data)
{
  const float from = (float)(intptr_t)data / 1000.0f;
  GroupExt *x = GRX(w);
  x->reveal = from + ((x->expanded ? 1.0f : 0.0f) - from) * (float)t;
  for (int i = 0; i < x->n_items; i++) {
    awra_widget_set_child_visible(x->items[i], x->reveal > 0.0f);
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

static void header_toggled(AwraWidget *item, const AwraArgs *args, void *data)
{
  (void)item;
  (void)args;
  AwraWidget *group = (AwraWidget *)data;
  awra_rail_group_set_expanded(group, !GRX(group)->expanded);
  awra_emit_active(group, AWRA_SIGNAL_TOGGLED, GRX(group)->expanded);
}

AwraWidget *awra_rail_group_new(const char *label)
{
  if (!face_item_class.type) {
    face_item_class = item_class;
    face_item_class.draw = face_item_draw;
  }
  AwraWidget *w = awra_widget_new(&group_class);
  GroupExt *x = GRX(w);
  x->label = label ? awra_strdup(label) : NULL;
  x->header = awra_widget_new(&face_item_class);
  RailItemExt *hx = RIX(x->header);
  hx->label = label ? awra_strdup(label) : NULL;
  hx->scale = 1.0f;
  hx->to[2] = 1.0f;
  hx->group_header = true;
  awra_widget_set_tooltip(x->header, label);
  awra_child_insert(w, x->header, -1);
  awra_widget_unref(x->header);
  awra_signal_connect(x->header, AWRA_SIGNAL_ACTIVATE, header_toggled, w, NULL);
  return w;
}

void awra_rail_group_set_label(AwraWidget *w, const char *label)
{
  awra_set_str(&GRX(w)->label, label);
  awra_rail_item_set_label(GRX(w)->header, label);
}

void awra_rail_group_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_set_str(&GRX(w)->icon_name, icon);
  awra_invalidate(GRX(w)->header);
}

void awra_rail_group_set_color(AwraWidget *w, const AwraRGBA *color)
{
  GroupExt *x = GRX(w);
  x->has_color = color != NULL;
  if (color) {
    x->color = *color;
  }
  awra_rail_item_set_tint(x->header, color);
  awra_invalidate(w);
}

void awra_rail_group_set_expanded(AwraWidget *w, bool expanded)
{
  GroupExt *x = GRX(w);
  if (x->expanded == expanded) {
    return;
  }
  x->expanded = expanded;
  for (int i = 0; i < x->n_items; i++) {
    awra_widget_set_child_visible(x->items[i], true);
  }
  awra_anim_stop_for(w);
  awra_anim_start(w, awra_aux(w)->mapped ? awra_motion_duration(AWRA_MOTION_NORMAL) : 0, group_reveal_anim,
                  (void *)(intptr_t)(x->reveal * 1000.0f), NULL);
}

bool awra_rail_group_expanded(AwraWidget *w)
{
  return GRX(w)->expanded;
}

void awra_rail_group_insert(AwraWidget *w, AwraWidget *item, int position)
{
  GroupExt *x = GRX(w);
  if (position < 0 || position > x->n_items) {
    position = x->n_items;
  }
  x->items = (AwraWidget **)realloc(x->items, sizeof(AwraWidget *) * (size_t)(x->n_items + 1));
  memmove(x->items + position + 1, x->items + position, sizeof(AwraWidget *) * (size_t)(x->n_items - position));
  x->items[position] = item;
  x->n_items++;
  awra_child_insert(w, item, -1);
  awra_widget_set_child_visible(item, x->reveal > 0.0f);
  awra_invalidate(x->header);
}

void awra_rail_group_append(AwraWidget *w, AwraWidget *item)
{
  awra_rail_group_insert(w, item, -1);
}

void awra_rail_group_remove(AwraWidget *w, AwraWidget *item)
{
  GroupExt *x = GRX(w);
  for (int i = 0; i < x->n_items; i++) {
    if (x->items[i] == item) {
      memmove(x->items + i, x->items + i + 1, sizeof(AwraWidget *) * (size_t)(x->n_items - i - 1));
      x->n_items--;
      awra_child_remove(w, item);
      awra_invalidate(x->header);
      return;
    }
  }
}

int awra_rail_group_n_items(AwraWidget *w)
{
  return GRX(w)->n_items;
}

AwraWidget *awra_rail_group_item(AwraWidget *w, int index)
{
  GroupExt *x = GRX(w);
  return index >= 0 && index < x->n_items ? x->items[index] : NULL;
}

/* ======================================================================= */
/* The rail: header, a scrolled column of entries, footer */

typedef struct RailExt {
  AwraWidget *box, *scroller, *list, *header, *footer;
  AwraWidget *selected;
  bool reorderable;
} RailExt;

#define RLX(w) AWRA_EXT(w, RailExt)

static void list_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)for_size;
  const int gap = awra_px(entry_gap());
  int total = 0;
  for (int i = 0; i < w->nkids; i++) {
    if (!awra_child_visible(w->kids[i])) {
      continue;
    }
    int mn, nt;
    awra_measure(w->kids[i], o, -1, &mn, &nt);
    if (o == AWRA_ORIENTATION_HORIZONTAL) {
      total = BK_MAX(total, mn);
    }
    else {
      total += mn + gap;
    }
  }
  if (o == AWRA_ORIENTATION_VERTICAL) {
    total += gap;
  }
  *r_min = *r_nat = total;
}

static void list_allocate(AwraWidget *w)
{
  const int gap = awra_px(entry_gap());
  int y = gap;
  for (int i = 0; i < w->nkids; i++) {
    if (!awra_child_visible(w->kids[i])) {
      continue;
    }
    int mn, nt;
    awra_measure(w->kids[i], AWRA_ORIENTATION_VERTICAL, -1, &mn, &nt);
    awra_allocate(w->kids[i], w->x, w->y + y, w->w, mn);
    y += mn + gap;
  }
}

static AwraClass list_class = {
    .bk = AWRA_BK("AwraRailList", 0),
    .type = "AwraRailList",
    .flags = AWRA_CLASS_CONTAINER,
    .measure = list_measure,
    .allocate = list_allocate,
};

static void rail_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  awra_measure(RLX(w)->box, o, for_size, r_min, r_nat);
}

static void rail_allocate(AwraWidget *w)
{
  awra_allocate(RLX(w)->box, w->x, w->y, w->w, w->h);
}

static void rail_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->flags |= AWRA_A11Y_VERTICAL; /* Awra 1: orientation vertical */
  info->selection = true;
}

static AwraClass rail_class = {
    .a11y = rail_a11y,
    .bk = AWRA_BK("AwraRail", 0),
    .type = "AwraRail",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(RailExt),
    .measure = rail_measure,
    .allocate = rail_allocate,
};

AwraWidget *awra_rail_new(void)
{
  AwraWidget *w = awra_widget_new(&rail_class);
  RailExt *x = RLX(w);
  x->box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_child_insert(w, x->box, -1);
  awra_widget_unref(x->box);
  x->scroller = awra_scrolled_new();
  awra_scrolled_set_policy(x->scroller, AWRA_POLICY_NEVER, AWRA_POLICY_EXTERNAL);
  awra_scrolled_set_propagate_natural(x->scroller, true, false);
  awra_widget_set_vexpand(x->scroller, true);
  x->list = awra_widget_new(&list_class);
  awra_scrolled_set_child(x->scroller, x->list);
  awra_widget_unref(x->list);
  awra_box_append(x->box, x->scroller);
  awra_widget_unref(x->scroller);
  return w;
}

static void select_item(AwraWidget *rail, AwraWidget *item)
{
  RailExt *x = RLX(rail);
  if (x->selected == item) {
    return;
  }
  if (x->selected) {
    awra_rail_item_set_selected(x->selected, false);
    awra_widget_unref(x->selected);
  }
  x->selected = item ? awra_widget_ref(item) : NULL;
  if (item) {
    awra_rail_item_set_selected(item, true);
  }
  awra_emit_text(rail, AWRA_SIGNAL_NOTIFY, "selected-item");
}

void awra_rail_item_activated_internal(AwraWidget *rail, AwraWidget *item)
{
  select_item(rail, item);
}

void awra_rail_insert(AwraWidget *w, AwraWidget *entry, int position)
{
  RailExt *x = RLX(w);
  if (entry->parent == x->list) {
    awra_widget_ref(entry);
    awra_child_remove(x->list, entry);
    awra_child_insert(x->list, entry, position);
    awra_widget_unref(entry);
    return;
  }
  awra_child_insert(x->list, entry, position);
}

void awra_rail_append(AwraWidget *w, AwraWidget *entry)
{
  awra_rail_insert(w, entry, -1);
}

void awra_rail_remove(AwraWidget *w, AwraWidget *entry)
{
  RailExt *x = RLX(w);
  bool inside = false;
  for (BKNode *p = x->selected; p && !inside; p = p->parent) {
    inside = p == entry;
  }
  if (x->selected && inside) {
    awra_widget_unref(x->selected);
    x->selected = NULL;
  }
  awra_child_remove(x->list, entry);
}

AwraWidget *awra_rail_add_separator(AwraWidget *w)
{
  AwraWidget *sep = awra_widget_new(&sep_class);
  awra_widget_set_can_target(sep, false);
  awra_child_insert(RLX(w)->list, sep, -1);
  awra_widget_unref(sep);
  return sep;
}

int awra_rail_n_entries(AwraWidget *w)
{
  return RLX(w)->list->nkids;
}

AwraWidget *awra_rail_entry(AwraWidget *w, int position)
{
  AwraWidget *l = RLX(w)->list;
  return position >= 0 && position < l->nkids ? l->kids[position] : NULL;
}

AwraWidget *awra_rail_selected_item(AwraWidget *w)
{
  return RLX(w)->selected;
}

void awra_rail_set_selected_item(AwraWidget *w, AwraWidget *item)
{
  select_item(w, item);
}

void awra_rail_set_header(AwraWidget *w, AwraWidget *header)
{
  RailExt *x = RLX(w);
  if (x->header) {
    awra_box_remove(x->box, x->header);
  }
  x->header = header;
  if (header) {
    awra_box_prepend(x->box, header);
  }
}

void awra_rail_set_footer(AwraWidget *w, AwraWidget *footer)
{
  RailExt *x = RLX(w);
  if (x->footer) {
    awra_box_remove(x->box, x->footer);
  }
  x->footer = footer;
  if (footer) {
    awra_box_append(x->box, footer);
  }
}

void awra_rail_set_reorderable(AwraWidget *w, bool reorderable)
{
  RLX(w)->reorderable = reorderable;
}

bool awra_rail_reorderable(AwraWidget *w)
{
  return RLX(w)->reorderable;
}
