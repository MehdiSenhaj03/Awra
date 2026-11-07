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
/* drags that reorder the rail and merge entries into folders (below) */
static int entry_drag(AwraWidget *item, BKEvent *e);
static bool entry_drop(AwraWidget *item);
static bool rail_dragging_item(AwraWidget *item);

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
  if (rail_dragging_item(w)) {
    return; /* while an entry is dragged, only the drop target reacts */
  }
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
  /* a drag that moved or merged it ends; it doesn't activate */
  const bool dragged = entry_drop(w);
  item_update(w);
  if (e->inside && !dragged && !RIX(w)->lifted) {
    item_activate(w);
  }
  return 1;
}

static int item_drag(AwraWidget *w, BKEvent *e)
{
  return entry_drag(w, e);
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
    .drag = item_drag,
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

static void item_set_lifted(AwraWidget *w, bool lifted)
{
  if (RIX(w)->lifted != lifted) {
    RIX(w)->lifted = lifted;
    item_update(w);
    awra_invalidate(w);
  }
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

/* How close to an edge of the visible column a drag scrolls, and how fast
 * (logical px, px a second); the middle share of a tile that merges an item
 * dropped there rather than placing it beside (Awra 1's rail). */
#define AUTOSCROLL_EDGE 40.0f
#define AUTOSCROLL_SPEED 720.0f
#define MERGE_ZONE 0.5f
#define DRAG_THRESHOLD 8

/* An entry's offset from its place in the column: entries move out of the
 * way of a dragged one, then settle where they belong (raster px). */
typedef struct Motion {
  AwraWidget *entry;
  float offset, from, to;
} Motion;

typedef struct ListExt {
  AwraWidget *rail;
  /* the lifted entry, drawn last (on top) and placed at dragged_y; its
   * place in the column is `origin` meanwhile */
  AwraWidget *dragged;
  int origin;
  float dragged_y;
  Motion *motion;
  int n_motion;
} ListExt;

#define LSX(w) AWRA_EXT(w, ListExt)

typedef struct RailExt {
  AwraWidget *box, *scroller, *list, *header, *footer;
  AwraWidget *selected;
  bool reorderable;
  /* a press on an entry that may become a drag (raster px) */
  AwraWidget *pressed;
  float press_list_y, pointer_view_y, grab_offset;
  bool dragging, dragged; /* dragged: the press turned into a drag */
  int drop;
  AwraWidget *merge;
  unsigned autoscroll;
  double autoscroll_time;
} RailExt;

#define RLX(w) AWRA_EXT(w, RailExt)

static Motion *motion_of(AwraWidget *list, const AwraWidget *entry)
{
  ListExt *x = LSX(list);
  for (int i = 0; i < x->n_motion; i++) {
    if (x->motion[i].entry == entry) {
      return &x->motion[i];
    }
  }
  return NULL;
}

static float entry_offset(AwraWidget *list, const AwraWidget *entry)
{
  const Motion *m = motion_of(list, entry);
  return m ? m->offset : 0.0f;
}

static void forget_motion(AwraWidget *list, const AwraWidget *entry)
{
  ListExt *x = LSX(list);
  for (int i = 0; i < x->n_motion; i++) {
    if (x->motion[i].entry == entry) {
      x->motion[i] = x->motion[--x->n_motion];
      return;
    }
  }
}

static void motion_anim(AwraWidget *list, double t, void *data)
{
  (void)data;
  ListExt *x = LSX(list);
  bool moving = false;
  for (int i = 0; i < x->n_motion; i++) {
    Motion *m = &x->motion[i];
    m->offset = m->from + (m->to - m->from) * (float)t;
    moving |= m->offset != 0.0f || m->to != 0.0f;
  }
  if (t >= 1.0 && !moving && !x->dragged) {
    x->n_motion = 0; /* everything settled */
  }
  awra_queue_allocate(list);
  awra_invalidate(list);
}

/* `entry`'s motion, made at rest when it has none (NULL: out of memory). */
static Motion *motion_add(AwraWidget *list, AwraWidget *entry)
{
  ListExt *x = LSX(list);
  Motion *m = motion_of(list, entry);
  if (!m) {
    Motion *grown = (Motion *)realloc(x->motion, sizeof(Motion) * (size_t)(x->n_motion + 1));
    if (!grown) {
      return NULL;
    }
    x->motion = grown;
    m = &x->motion[x->n_motion++];
    *m = (Motion){entry, 0.0f, 0.0f, 0.0f};
  }
  return m;
}

/* Moves `entry` towards `offset` (animated, all entries together). */
static bool retarget(AwraWidget *list, AwraWidget *entry, float offset)
{
  Motion *m = motion_of(list, entry);
  if (!m && offset == 0.0f) {
    return false;
  }
  m = m ? m : motion_add(list, entry);
  if (!m || m->to == offset) {
    return false;
  }
  m->to = offset;
  return true;
}

static void motion_start(AwraWidget *list)
{
  ListExt *x = LSX(list);
  for (int i = 0; i < x->n_motion; i++) {
    x->motion[i].from = x->motion[i].offset;
  }
  awra_anim_stop_for(list);
  awra_anim_start(list, awra_motion_duration(AWRA_MOTION_NORMAL), motion_anim, NULL, NULL);
}

static int entry_height(AwraWidget *entry)
{
  int mn, nt;
  awra_measure(entry, AWRA_ORIENTATION_VERTICAL, -1, &mn, &nt);
  return mn;
}

/* The entries in their order: with an entry lifted, it is drawn last but
 * its place stays `origin` until it is dropped. */
static int list_order(AwraWidget *list, AwraWidget **out)
{
  ListExt *x = LSX(list);
  int n = 0;
  for (int i = 0; i < list->nkids; i++) {
    if (x->dragged && n == x->origin) {
      out[n++] = x->dragged;
    }
    if (list->kids[i] != x->dragged) {
      out[n++] = list->kids[i];
    }
  }
  if (x->dragged && n == x->origin) {
    out[n++] = x->dragged;
  }
  return n;
}

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
  ListExt *x = LSX(w);
  const int gap = awra_px(entry_gap());
  AwraWidget *stack[64], **order = w->nkids <= 64 ? stack : (AwraWidget **)malloc(sizeof(AwraWidget *) * (size_t)w->nkids);
  if (!order) {
    return;
  }
  const int n = list_order(w, order);
  int y = gap;
  for (int i = 0; i < n; i++) {
    AwraWidget *c = order[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    const int h = entry_height(c);
    const float top = c == x->dragged ? x->dragged_y : y + entry_offset(w, c);
    awra_allocate(c, w->x, w->y + (int)roundf(top), w->w, h);
    y += h + gap;
  }
  if (order != stack) {
    free(order);
  }
}

static void list_finalize(AwraWidget *w)
{
  free(LSX(w)->motion);
}

static AwraClass list_class = {
    .bk = AWRA_BK("AwraRailList", 0),
    .type = "AwraRailList",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(ListExt),
    .finalize = list_finalize,
    .measure = list_measure,
    .allocate = list_allocate,
};

/* -- drags --------------------------------------------------------------- */

static AwraWidget *rail_of(AwraWidget *w)
{
  for (BKNode *p = w ? w->parent : NULL; p; p = p->parent) {
    if (awra_is_widget(p) && awra_widget_is(p, "AwraRail")) {
      return p;
    }
  }
  return NULL;
}

/* The entry an item drags: itself at the top of the column, its folder when
 * it is the folder's own tile; an item inside a folder doesn't drag. */
static AwraWidget *drag_entry(AwraWidget *item, AwraWidget *list)
{
  if (item->parent == list) {
    return item;
  }
  if (RIX(item)->group_header && item->parent && item->parent->parent == list) {
    return item->parent;
  }
  return NULL;
}

static bool is_group(const AwraWidget *w)
{
  return awra_widget_is(w, "AwraRailGroup");
}

static bool is_entry(const AwraWidget *w)
{
  return awra_widget_is(w, "AwraRailItem") || is_group(w);
}

/* The tile that shows an entry: an item, or a folder's own tile. */
static AwraWidget *drag_face(AwraWidget *entry)
{
  return is_group(entry) ? GRX(entry)->header : entry;
}

static bool rail_dragging_item(AwraWidget *item)
{
  AwraWidget *rail = rail_of(item);
  return rail && RLX(rail)->dragging;
}

static float view_top(AwraWidget *rail)
{
  return (float)RLX(rail)->scroller->y;
}

static float scroll_px(AwraWidget *rail)
{
  return (float)(awra_scrolled_vvalue(RLX(rail)->scroller) * awra_scale());
}

static void set_merge(AwraWidget *rail, AwraWidget *target)
{
  RailExt *x = RLX(rail);
  if (x->merge == target) {
    return;
  }
  if (x->merge) {
    awra_rail_item_set_drop_target(drag_face(x->merge), false);
  }
  x->merge = target;
  if (target) {
    awra_rail_item_set_drop_target(drag_face(target), true);
  }
}

/* The top of each entry but the dragged one with room for it at `drop`,
 * in order (`tops` has room for list->nkids). */
static void layout_with_gap(AwraWidget *list, AwraWidget **order, int n, int drop, float *tops)
{
  ListExt *lx = LSX(list);
  const int gap = awra_px(entry_gap());
  float y = (float)gap;
  int index = 0;
  for (int i = 0; i < n; i++) {
    if (order[i] == lx->dragged) {
      continue;
    }
    if (index == drop) {
      y += entry_height(lx->dragged) + gap;
    }
    tops[index++] = y;
    y += entry_height(order[i]) + gap;
  }
}

/* Places the dragged entry under the pointer and moves the others to make
 * room where it would drop, or holds them still over a merge target. */
static void update_drag(AwraWidget *rail)
{
  RailExt *x = RLX(rail);
  AwraWidget *list = x->list;
  ListExt *lx = LSX(list);
  const int gap = awra_px(entry_gap());
  const float tile = (float)awra_px(tile_size());
  const int n = list->nkids;
  AwraWidget **order = (AwraWidget **)malloc(sizeof(AwraWidget *) * (size_t)n);
  float *tops = (float *)malloc(sizeof(float) * (size_t)n);
  if (!order || !tops) {
    free(order);
    free(tops);
    return;
  }
  list_order(list, order);
  const int dragged_h = entry_height(lx->dragged);
  float top = x->pointer_view_y + scroll_px(rail) - x->grab_offset;
  top = fminf(fmaxf(top, 0.0f), fmaxf(0.0f, (float)(list->h - dragged_h)));
  lx->dragged_y = top;
  const float center = top + fminf((float)dragged_h, tile) / 2.0f;
  AwraWidget *merge = NULL;
  int drop = 0;
  /* over the middle of a tile where it stands now, an item merges */
  if (!is_group(lx->dragged)) {
    layout_with_gap(list, order, n, x->drop, tops);
    int index = 0;
    for (int i = 0; i < n; i++) {
      if (order[i] == lx->dragged) {
        continue;
      }
      const float tile_center = tops[index] + tile / 2.0f;
      if (is_entry(order[i]) && fabsf(center - tile_center) < tile * MERGE_ZONE / 2.0f) {
        merge = order[i];
      }
      index++;
    }
  }
  if (merge) {
    drop = x->drop;
  }
  else {
    /* otherwise it slots in after every entry whose middle it passed */
    float compact = (float)gap;
    for (int i = 0; i < n; i++) {
      if (order[i] == lx->dragged) {
        continue;
      }
      const int h = entry_height(order[i]);
      if (center > compact + fminf((float)h, tile) / 2.0f) {
        drop++;
      }
      compact += h + gap;
    }
  }
  x->drop = drop;
  set_merge(rail, merge);
  layout_with_gap(list, order, n, drop, tops);
  float natural = (float)gap;
  int index = 0;
  bool moved = false;
  for (int i = 0; i < n; i++) {
    if (order[i] != lx->dragged) {
      moved |= retarget(list, order[i], tops[index] - natural);
      index++;
    }
    natural += entry_height(order[i]) + gap;
  }
  if (moved) {
    motion_start(list);
  }
  free(order);
  free(tops);
  awra_queue_allocate(list);
  awra_invalidate(list);
}

static bool autoscroll_tick(void *data)
{
  AwraWidget *rail = (AwraWidget *)data;
  RailExt *x = RLX(rail);
  if (!x->dragging) {
    x->autoscroll = 0;
    return false;
  }
  const float s = awra_scale(), edge = AUTOSCROLL_EDGE * s;
  const float page = (float)x->scroller->h;
  const double now = awra_now_ms();
  const double elapsed = x->autoscroll_time > 0 ? (now - x->autoscroll_time) / 1000.0 : 0.0;
  x->autoscroll_time = now;
  float speed = 0.0f;
  if (x->pointer_view_y < edge) {
    speed = -AUTOSCROLL_SPEED * (1.0f - fmaxf(x->pointer_view_y, 0.0f) / edge);
  }
  else if (x->pointer_view_y > page - edge) {
    speed = AUTOSCROLL_SPEED * (1.0f - fmaxf(page - x->pointer_view_y, 0.0f) / edge);
  }
  if (speed == 0.0f) {
    x->autoscroll = 0; /* out of the edges: no timer until it comes back */
    x->autoscroll_time = 0;
    return false;
  }
  if (elapsed > 0) {
    awra_scrolled_set_vvalue(x->scroller, awra_scrolled_vvalue(x->scroller) + speed * elapsed);
    update_drag(rail);
  }
  return true;
}

static void stop_autoscroll(RailExt *x)
{
  if (x->autoscroll) {
    awra_source_remove(x->autoscroll);
  }
  x->autoscroll = 0;
  x->autoscroll_time = 0;
}

static void begin_drag(AwraWidget *rail, AwraWidget *entry)
{
  RailExt *x = RLX(rail);
  AwraWidget *list = x->list;
  ListExt *lx = LSX(list);
  const int origin = awra_child_index(list, entry);
  if (origin < 0) {
    return;
  }
  x->dragging = true;
  x->dragged = true;
  x->drop = origin;
  x->grab_offset = x->press_list_y - (float)(entry->y - list->y);
  lx->dragged = awra_widget_ref(entry);
  lx->origin = origin;
  lx->dragged_y = (float)(entry->y - list->y);
  /* drawn last: over its neighbours */
  memmove(list->kids + origin, list->kids + origin + 1, sizeof(BKNode *) * (size_t)(list->nkids - origin - 1));
  list->kids[list->nkids - 1] = entry;
  item_set_lifted(drag_face(entry), true);
}

static void end_drag(AwraWidget *rail, bool commit)
{
  RailExt *x = RLX(rail);
  AwraWidget *list = x->list;
  ListExt *lx = LSX(list);
  AwraWidget *dragged = lx->dragged;
  stop_autoscroll(x);
  x->dragging = false;
  x->pressed = NULL;
  if (!dragged) {
    return;
  }
  AwraWidget *merge = x->merge ? awra_widget_ref(x->merge) : NULL;
  set_merge(rail, NULL);
  item_set_lifted(drag_face(dragged), false);
  const int n = list->nkids;
  AwraWidget **order = (AwraWidget **)malloc(sizeof(AwraWidget *) * (size_t)n);
  float *seen = (float *)malloc(sizeof(float) * (size_t)n);
  const int gap = awra_px(entry_gap());
  int final = lx->origin;
  if (order && seen) {
    /* where each entry is on screen now */
    list_order(list, order);
    int y = gap;
    for (int i = 0; i < n; i++) {
      seen[i] = order[i] == dragged ? lx->dragged_y : y + entry_offset(list, order[i]);
      y += entry_height(order[i]) + gap;
    }
    if (commit && !merge && x->drop != lx->origin) {
      final = x->drop;
    }
    /* the new order, the dragged entry at `final`; it was kept last */
    AwraWidget **kids = list->kids;
    int k = 0;
    for (int i = 0; i < n; i++) {
      if (order[i] != dragged) {
        if (k == final) {
          kids[k++] = dragged;
        }
        kids[k++] = order[i];
      }
    }
    if (k < n) {
      kids[k++] = dragged;
    }
    lx->dragged = NULL;
    /* every entry stays where it is on screen, then settles into its place */
    y = gap;
    for (int i = 0; i < n; i++) {
      AwraWidget *c = kids[i];
      float was = 0.0f;
      for (int j = 0; j < n; j++) {
        if (order[j] == c) {
          was = seen[j];
          break;
        }
      }
      Motion *m = was != (float)y ? motion_add(list, c) : motion_of(list, c);
      if (m) {
        m->offset = was - (float)y;
        m->to = 0.0f;
      }
      y += entry_height(c) + gap;
    }
    motion_start(list);
  }
  else {
    /* out of memory: put it back where it was */
    memmove(list->kids + lx->origin + 1, list->kids + lx->origin, sizeof(BKNode *) * (size_t)(n - lx->origin - 1));
    list->kids[lx->origin] = dragged;
    lx->dragged = NULL;
  }
  free(order);
  free(seen);
  awra_queue_allocate(list);
  awra_invalidate(list);
  AwraArgs args = {0};
  args.signal = AWRA_SIGNAL_MOVED;
  if (commit && merge) {
    args.active = true;
    args.index = awra_child_index(list, dragged);
    args.value = awra_child_index(list, merge);
    awra_emit(rail, &args);
  }
  else if (commit && final != lx->origin) {
    args.index = final;
    awra_emit(rail, &args);
  }
  if (merge) {
    awra_widget_unref(merge);
  }
  awra_widget_unref(dragged);
}

static int entry_drag(AwraWidget *item, BKEvent *e)
{
  AwraWidget *rail = rail_of(item);
  if (!rail) {
    return 0;
  }
  RailExt *x = RLX(rail);
  AwraWidget *list = x->list;
  if (!x->dragging) {
    AwraWidget *entry = x->reorderable ? drag_entry(item, list) : NULL;
    const int t = awra_px(DRAG_THRESHOLD);
    if (!entry || e->dx * e->dx + e->dy * e->dy < t * t) {
      return 0;
    }
    /* the press, in list coordinates */
    x->press_list_y = (float)(e->y - e->dy - list->y);
    begin_drag(rail, entry);
    if (!x->dragging) {
      return 0;
    }
  }
  x->pointer_view_y = (float)e->y - view_top(rail);
  update_drag(rail);
  const float edge = AUTOSCROLL_EDGE * awra_scale();
  if (!x->autoscroll && (x->pointer_view_y < edge || x->pointer_view_y > x->scroller->h - edge)) {
    x->autoscroll_time = 0;
    x->autoscroll = awra_timeout_add(16, autoscroll_tick, rail, NULL);
  }
  return 1;
}

/* The press on `item` ends: true when it had become a drag. */
static bool entry_drop(AwraWidget *item)
{
  AwraWidget *rail = rail_of(item);
  if (!rail) {
    return false;
  }
  RailExt *x = RLX(rail);
  const bool dragged = x->dragged;
  if (x->dragging) {
    awra_widget_ref(rail);
    end_drag(rail, true);
    awra_widget_unref(rail);
  }
  x->dragged = false;
  return dragged;
}

static void rail_map(AwraWidget *w, bool mapped)
{
  if (!mapped && RLX(w)->dragging) {
    end_drag(w, false);
  }
}

static void rail_finalize(AwraWidget *w)
{
  RailExt *x = RLX(w);
  stop_autoscroll(x);
  if (x->selected) {
    awra_widget_unref(x->selected);
  }
}

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
    .finalize = rail_finalize,
    .measure = rail_measure,
    .allocate = rail_allocate,
    .map = rail_map,
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
  LSX(x->list)->rail = w;
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
  if (x->dragging) {
    end_drag(w, false); /* the column changes under it */
  }
  if (entry->parent == x->list) {
    forget_motion(x->list, entry);
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
  if (x->dragging) {
    end_drag(w, false);
  }
  forget_motion(x->list, entry);
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
  if (!reorderable && RLX(w)->dragging) {
    end_drag(w, false);
  }
}

bool awra_rail_reorderable(AwraWidget *w)
{
  return RLX(w)->reorderable;
}
