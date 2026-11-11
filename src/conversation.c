/* Awra 2 conversation view (Awra 1's AwraConversationView): a list view of
 * messages that starts at the newest, follows new ones in while the newest
 * shows, keeps what is read in place while older ones load above, offers a
 * pill back to the newest, and shows what happens at each end (loading, or
 * a failure with a retry).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

#define AT_BOTTOM_SLACK 24.0
#define GLIDE_PAGES 2.0
#define PILL_MARGIN 14
#define HIGHLIGHT_MS 1500.0
/* new messages appended while following rise in (awra.css's
 * awra-conversation-arrive: 220 ms, 8 px, cubic-bezier(.2, .8, .2, 1));
 * a larger batch is a load, not arrivals */
#define ARRIVE_MS 220.0
#define ARRIVE_RISE 8.0f
#define MAX_ARRIVING 8

void awra_scrolled_shift_internal(AwraWidget *scrolled, double delta);
bool awra_scrolled_shifting_internal(AwraWidget *scrolled);
void awra_scrolled_get_vadjustment(AwraWidget *scrolled, double *value, double *upper, double *page);
long long awra_list_view_offset_internal(AwraWidget *w, unsigned position);
int awra_list_view_height_internal(AwraWidget *w, unsigned position);
void awra_list_view_drop_anchor_internal(AwraWidget *w);
void awra_list_view_item_set_offset_internal(AwraWidget *item, int dy);

/* CSS cubic-bezier(x1, y1, x2, y2) at progress t (0..1). */
static double css_bezier(double x1, double y1, double x2, double y2, double t)
{
  if (t <= 0.0 || t >= 1.0) {
    return t <= 0.0 ? 0.0 : 1.0;
  }
  /* solve x(u) = t by Newton steps, then y(u) */
  double u = t;
  for (int i = 0; i < 8; i++) {
    const double x = 3 * x1 * u * (1 - u) * (1 - u) + 3 * x2 * u * u * (1 - u) + u * u * u - t;
    const double dx = 3 * x1 * (1 - u) * (1 - 3 * u) + 3 * x2 * u * (2 - 3 * u) + 3 * u * u;
    if (fabs(x) < 1e-5 || fabs(dx) < 1e-6) {
      break;
    }
    u -= x / dx;
  }
  u = u < 0 ? 0 : u > 1 ? 1 : u;
  return 3 * y1 * u * (1 - u) * (1 - u) + 3 * y2 * u * u * (1 - u) + u * u * u;
}

/* ======================================================================= */
/* A floating pill (the jump button, the edges' status): the window's
 * background under a cap, an outline, a soft shadow, a rim */

typedef struct PillExt {
  AwraWidget *child;
  bool accent, clickable;
} PillExt;

#define PLX(w) AWRA_EXT(w, PillExt)

static void pill_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  PillExt *x = PLX(w);
  const int ph = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? (x->clickable ? 26 : 28) : 12);
  int mn = 0, nt = 0;
  if (x->child) {
    awra_measure(x->child, o, for_size >= 0 ? BK_MAX(0, for_size - ph) : -1, &mn, &nt);
  }
  if (o == AWRA_ORIENTATION_VERTICAL) {
    mn = BK_MAX(mn, awra_px(20));
    nt = BK_MAX(nt, awra_px(20));
  }
  *r_min = mn + ph;
  *r_nat = nt + ph;
}

static void pill_allocate(AwraWidget *w)
{
  PillExt *x = PLX(w);
  if (!x->child) {
    return;
  }
  /* padding 6 15 6 11 (the jump) or 6 14 */
  const int ps = awra_px(x->clickable ? 11 : 14), pe = awra_px(x->clickable ? 15 : 14), pv = awra_px(6);
  awra_allocate(x->child, w->x + ps, w->y + pv, BK_MAX(0, w->w - ps - pe), BK_MAX(0, w->h - 2 * pv));
}

static void pill_draw(AwraWidget *w, AwraDraw *d)
{
  PillExt *x = PLX(w);
  const AwraTokens *t = awra_tokens();
  const AwraBox b = awra_box_of(w);
  const float r = b.h / 2;
  const float s = awra_scale();
  if (x->accent) {
    AwraRGBA sh = awra_with_alpha(t->accent, 0.55f);
    awra_paint_shadow(d, b, r, 0, 6 * s, -4 * s, 16 * s, &sh, true);
    AwraRGBA fill = t->accent;
    if (awra_hovered(w)) {
      fill = awra_mix(fill, t->on_accent, 0.10f);
    }
    awra_paint_rbox(d, b, r, &fill, 0, NULL);
    const AwraRGBA rim = {1, 1, 1, 0.20f};
    awra_paint_inset_shadow(d, b, r, 0, s, 0, 0, &rim); /* inset 0 1px, as GSK draws it */
    return;
  }
  const AwraRGBA sh = {0, 0, 0, 0.30f};
  awra_paint_shadow(d, b, r, 0, 6 * s, -4 * s, 16 * s, &sh, true);
  awra_paint_rbox(d, b, r, &t->background, 0, NULL);
  AwraRGBA cap = t->control_cap;
  if (x->clickable && awra_pressed(w)) {
    cap = t->control_pressed;
  }
  else if (x->clickable && awra_hovered(w)) {
    cap = t->control_cap_hover;
  }
  awra_paint_rbox(d, b, r, &cap, 0, NULL);
  /* 0 0 0 1px outline, outside */
  awra_paint_rbox(d, (AwraBox){b.x - s, b.y - s, b.w + 2 * s, b.h + 2 * s}, r + s, NULL, s, &t->outline);
  awra_paint_inset_shadow(d, b, r, 0, s, 0, 0, &t->rim); /* inset 0 1px @awra_rim */
}

static bool pill_child_style(AwraWidget *w, AwraRGBA *c, int *weight)
{
  *c = PLX(w)->accent ? awra_tokens()->on_accent : awra_tokens()->foreground;
  *weight = 600;
  return true;
}

static int pill_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside && PLX(w)->clickable) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
  }
  return 1;
}

/* a pill: a button when clickable (the jump pill), else a status */
static void pill_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->role = PLX(w)->clickable ? AWRA_ACCESSIBLE_ROLE_BUTTON : AWRA_ACCESSIBLE_ROLE_STATUS;
  info->action = PLX(w)->clickable ? "click" : NULL;
}

static bool pill_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  if (item < 0 && verb == AWRA_A11Y_DO_ACTIVATE && PLX(w)->clickable && awra_sensitive(w)) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
    return true;
  }
  return false;
}

static AwraClass pill_class = {
    .a11y = pill_a11y,
    .a11y_do = pill_a11y_do,
    .bk = AWRA_BK("AwraConversationPill", 0),
    .type = "AwraConversationPill",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_HOVER,
    .ext_size = sizeof(PillExt),
    .measure = pill_measure,
    .allocate = pill_allocate,
    .draw = pill_draw,
    .child_style = pill_child_style,
    .release = pill_release,
    .overdraw = 22,
};

static AwraWidget *pill_new(AwraWidget *child, bool clickable)
{
  AwraWidget *w = awra_widget_new(&pill_class);
  PLX(w)->clickable = clickable;
  awra_set_slot(w, &PLX(w)->child, child);
  if (clickable) {
    w->flags |= NF_EVENTS;
    awra_widget_set_cursor(w, "pointer");
  }
  return w;
}

/* ======================================================================= */
/* The view */

typedef struct ConvExt {
  AwraWidget *scroller, *list;
  AwraWidget *pill, *pill_label;
  AwraWidget *edges[2], *edge_spinner[2], *edge_icon[2], *edge_label[2], *edge_retry[2];
  int edge_status[2];
  char *jump_label;
  unsigned unread;
  int threshold; /* logical, -1: a page */
  bool follow, at_bottom, top_armed, bottom_armed, pill_shown;
  bool setting_value;
  /* glide */
  double glide_from, glide_to;
  bool gliding, glide_end;
  /* highlight: a row's tint and bar, full for 35% of the time, then
   * fading out (awra.css's awra-conversation-highlight) */
  unsigned highlight_pos;
  double highlight_start, highlight_until;
  unsigned highlight_timer; /* reduced motion: when it goes */
  /* rows rising in: their positions and when they arrived */
  unsigned arriving[MAX_ARRIVING];
  double arrived[MAX_ARRIVING];
  int n_arriving;
  bool moving; /* the list's motion animation runs */
  unsigned n_items;
  bool frame_queued;
} ConvExt;

#define CVX(w) AWRA_EXT(w, ConvExt)

static double end_value(AwraWidget *w)
{
  double value, upper, page;
  awra_scrolled_get_vadjustment(CVX(w)->scroller, &value, &upper, &page);
  return fmax(0.0, upper - page);
}

static double distance_to_end(AwraWidget *w)
{
  double value, upper, page;
  awra_scrolled_get_vadjustment(CVX(w)->scroller, &value, &upper, &page);
  return upper - page - value;
}

static void set_value(AwraWidget *w, double v)
{
  ConvExt *x = CVX(w);
  double value, upper, page;
  awra_scrolled_get_vadjustment(x->scroller, &value, &upper, &page);
  x->setting_value = true;
  awra_scrolled_shift_internal(x->scroller, (v - value) * awra_scale());
  x->setting_value = false;
}

static void set_pill_shown(AwraWidget *w, bool shown)
{
  ConvExt *x = CVX(w);
  if (x->pill_shown == shown) {
    return;
  }
  x->pill_shown = shown;
  awra_widget_set_child_visible(x->pill, shown);
  awra_queue_allocate(w);
}

static void update_at_bottom(AwraWidget *w)
{
  ConvExt *x = CVX(w);
  const bool at = x->n_items == 0 || distance_to_end(w) <= AT_BOTTOM_SLACK || (x->gliding && x->glide_end);
  set_pill_shown(w, !at);
  if (x->at_bottom != at) {
    x->at_bottom = at;
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "at-bottom");
  }
}

static void check_edges(AwraWidget *w)
{
  ConvExt *x = CVX(w);
  double value, upper, page;
  awra_scrolled_get_vadjustment(x->scroller, &value, &upper, &page);
  const double threshold = x->threshold < 0 ? page : x->threshold;
  bool emit_top = false, emit_bottom = false;
  if (x->n_items > 0 && page > 0) {
    if (value <= threshold) {
      emit_top = x->top_armed;
      x->top_armed = false;
    }
    else {
      x->top_armed = true;
    }
    if (upper - page - value <= threshold) {
      emit_bottom = x->bottom_armed;
      x->bottom_armed = false;
    }
    else {
      x->bottom_armed = true;
    }
  }
  awra_widget_ref(w);
  if (emit_top && x->edge_status[0] == AWRA_EDGE_STATUS_NONE) {
    awra_emit_index(w, AWRA_SIGNAL_EDGE_REACHED, 0);
  }
  if (emit_bottom && x->edge_status[1] == AWRA_EDGE_STATUS_NONE) {
    awra_emit_index(w, AWRA_SIGNAL_EDGE_REACHED, 1);
  }
  awra_widget_unref(w);
}

static bool after_frame(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  x->frame_queued = false;
  update_at_bottom(w);
  check_edges(w);
  awra_widget_unref(w);
  return false;
}

static void queue_checks(AwraWidget *w)
{
  ConvExt *x = CVX(w);
  if (!x->frame_queued) {
    x->frame_queued = true;
    awra_widget_ref(w);
    awra_idle_add(after_frame, w, NULL);
  }
}

static void scrolled_changed(AwraWidget *scroller, const AwraArgs *args, void *data)
{
  (void)scroller;
  (void)args;
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  /* the list keeping its rows in place while they are measured is not the
   * user scrolling: a view following the newest keeps following */
  if (!x->setting_value && !awra_scrolled_shifting_internal(x->scroller)) {
    /* the user scrolled: following stops until the end shows again */
    if (x->gliding) {
      x->gliding = false; /* first: glide_done then knows it was stopped */
      awra_anim_stop_for(w);
    }
    x->follow = distance_to_end(w) <= AT_BOTTOM_SLACK;
  }
  queue_checks(w);
}

static bool place_arrivals(ConvExt *x);

static void conv_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  awra_measure(CVX(w)->scroller, o, for_size, r_min, r_nat);
}

static void place_floating(AwraWidget *w, AwraWidget *c, int y_from_top, bool from_bottom, int below)
{
  int mn, nw, nh;
  const int margin = awra_px(PILL_MARGIN);
  awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nw);
  nw = BK_CLAMP(nw, mn, BK_MAX(mn, w->w - 2 * margin));
  awra_measure(c, AWRA_ORIENTATION_VERTICAL, nw, &mn, &nh);
  int y = from_bottom ? BK_MAX(0, w->h - nh - below) : y_from_top;
  awra_allocate(c, w->x + (w->w - nw) / 2, w->y + y, nw, nh);
}

static void conv_allocate(AwraWidget *w)
{
  ConvExt *x = CVX(w);
  awra_allocate(x->scroller, w->x, w->y, w->w, w->h);
  /* rows just bound for arrivals start transparent and low, not in place */
  if (x->n_arriving && place_arrivals(x)) {
    awra_allocate(x->scroller, w->x, w->y, w->w, w->h);
  }
  /* following: the newest stays at the bottom whatever came in. The end
   * moves as the rows there are measured, so it is reached within this
   * frame, a few layouts at most: a chat opened at its end otherwise drew
   * a frame of rows not yet laid out (an empty view) before settling. */
  for (int pass = 0; pass < 4 && x->follow && !x->gliding; pass++) {
    const double end = end_value(w);
    double value, upper, page;
    awra_scrolled_get_vadjustment(x->scroller, &value, &upper, &page);
    if (fabs(value - end) <= 0.5) {
      break;
    }
    awra_list_view_drop_anchor_internal(x->list);
    set_value(w, end);
    awra_allocate(x->scroller, w->x, w->y, w->w, w->h);
  }
  int pmn, pnh = 0;
  awra_measure(x->pill, AWRA_ORIENTATION_VERTICAL, -1, &pmn, &pnh);
  place_floating(w, x->pill, 0, true, awra_px(PILL_MARGIN));
  place_floating(w, x->edges[0], awra_px(PILL_MARGIN), false, 0);
  place_floating(w, x->edges[1], 0, true, pnh + 2 * awra_px(PILL_MARGIN));
  queue_checks(w);
}

/* How much of the highlight shows now (0 gone). */
static float highlight_alpha(const ConvExt *x)
{
  const double now = awra_now_ms();
  if (now >= x->highlight_until) {
    return 0.0f;
  }
  if (awra_style_reduced_motion()) {
    return 1.0f; /* without motion it still says where to look, then leaves */
  }
  const double p = (now - x->highlight_start) / HIGHLIGHT_MS;
  if (p <= 0.35) {
    return 1.0f;
  }
  /* ease-out within the last keyframe */
  return (float)(1.0 - css_bezier(0.0, 0.0, 0.58, 1.0, (p - 0.35) / 0.65));
}

static void conv_draw(AwraWidget *w, AwraDraw *d)
{
  ConvExt *x = CVX(w);
  /* a row being highlighted, under its content as its background: the
   * accent's soft tint and a 3 px bar at its start */
  const float a = highlight_alpha(x);
  if (a <= 0.0f) {
    return;
  }
  AwraWidget *it = awra_list_view_item(x->list, x->highlight_pos);
  if (!it) {
    return;
  }
  const AwraTokens *t = awra_tokens();
  const AwraRGBA soft = awra_with_alpha(t->accent_soft, t->accent_soft.a * a);
  const AwraRGBA bar = awra_with_alpha(t->accent, t->accent.a * a);
  bk_clip_push(w->x, w->y, w->w, w->h);
  awra_paint_rect(d, awra_box_of(it), &soft);
  awra_paint_rect(d, (AwraBox){(float)it->x, (float)it->y, awra_pxf(3), (float)it->h}, &bar);
  bk_clip_pop();
}

/* -- motion: arrivals and the highlight, on the list's frame clock ------- */

static void invalidate_row(ConvExt *x, unsigned position)
{
  AwraWidget *it = awra_list_view_item(x->list, position);
  if (it) {
    awra_invalidate(it);
  }
}

/* Each arrival's opacity and rise now; true when one of them moved. */
static bool place_arrivals(ConvExt *x)
{
  const double now = awra_now_ms();
  bool moved = false;
  int keep = 0;
  for (int i = 0; i < x->n_arriving; i++) {
    AwraWidget *it = awra_list_view_item(x->list, x->arriving[i]);
    const double p = (now - x->arrived[i]) / ARRIVE_MS;
    const double e = css_bezier(0.2, 0.8, 0.2, 1.0, p < 0 ? 0 : p > 1 ? 1 : p);
    if (it) {
      const int dy = p >= 1.0 ? 0 : (int)lroundf((float)(1.0 - e) * ARRIVE_RISE * awra_scale());
      moved |= awra_aux(it)->opacity != (float)e;
      awra_widget_set_opacity(it, e);
      awra_list_view_item_set_offset_internal(it, dy);
    }
    if (p < 1.0) {
      x->arriving[keep] = x->arriving[i];
      x->arrived[keep] = x->arrived[i];
      keep++;
    }
  }
  x->n_arriving = keep;
  return moved;
}

static void motion_frame(AwraWidget *list, double t, void *data)
{
  (void)list;
  (void)t;
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  place_arrivals(x);
  if (x->highlight_until > 0) {
    invalidate_row(x, x->highlight_pos);
  }
}

static void motion_done(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  x->moving = false;
  place_arrivals(x);
  if (awra_now_ms() >= x->highlight_until && x->highlight_until > 0) {
    invalidate_row(x, x->highlight_pos);
    x->highlight_until = 0;
  }
}

/* Runs the motion until the last arrival and the highlight are over. */
static void motion_start(AwraWidget *w)
{
  ConvExt *x = CVX(w);
  const double now = awra_now_ms();
  double until = x->highlight_until;
  for (int i = 0; i < x->n_arriving; i++) {
    until = fmax(until, x->arrived[i] + ARRIVE_MS);
  }
  if (until <= now || awra_style_reduced_motion()) {
    return;
  }
  if (x->moving) {
    awra_anim_stop_for(x->list); /* its done runs: restarted below */
  }
  x->moving = true;
  awra_anim_start(x->list, (unsigned)ceil(until - now), motion_frame, w, motion_done);
}

/* Positions after items changed at `position` (`removed` gone, `added`
 * new): moved along, or UINT32_MAX when the item went. */
static unsigned moved_position(unsigned p, unsigned position, unsigned removed, unsigned added)
{
  if (p == UINT32_MAX || p < position) {
    return p;
  }
  if (p < position + removed) {
    return UINT32_MAX;
  }
  return p - removed + added;
}

static void conv_finalize(AwraWidget *w)
{
  ConvExt *x = CVX(w);
  free(x->jump_label);
  if (x->moving) {
    awra_anim_stop_for(x->list);
  }
  if (x->highlight_timer) {
    awra_source_remove(x->highlight_timer);
  }
}

static AwraClass conv_class = {
    .bk = AWRA_BK("AwraConversationView", 0),
    .type = "AwraConversationView",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP,
    .ext_size = sizeof(ConvExt),
    .finalize = conv_finalize,
    .measure = conv_measure,
    .allocate = conv_allocate,
    .draw = conv_draw,
};

static void update_pill_label(AwraWidget *w)
{
  ConvExt *x = CVX(w);
  char buf[96];
  const char *text;
  if (x->jump_label) {
    text = x->jump_label;
  }
  else if (x->unread > 0) {
    snprintf(buf, sizeof(buf), x->unread == 1 ? "%u new message" : "%u new messages", x->unread);
    text = buf;
  }
  else {
    text = "Jump to present";
  }
  awra_label_set_text(x->pill_label, text);
  PLX(x->pill)->accent = x->unread > 0;
  awra_invalidate(x->pill);
}

static void glide_anim(AwraWidget *w, double t, void *data)
{
  (void)data;
  ConvExt *x = CVX(w);
  const double to = x->glide_end ? end_value(w) : x->glide_to;
  set_value(w, x->glide_from + (to - x->glide_from) * t);
}

static void glide_done(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  const bool finished = x->gliding; /* else stopped: the user took over */
  x->gliding = false;
  if (finished && x->glide_end) {
    x->follow = true;
    set_value(w, end_value(w));
  }
  queue_checks(w);
  awra_widget_unref(w);
}

static void glide_to(AwraWidget *w, double target, bool to_end, bool animate)
{
  ConvExt *x = CVX(w);
  double value, upper, page;
  awra_scrolled_get_vadjustment(x->scroller, &value, &upper, &page);
  if (x->gliding) {
    x->gliding = false; /* first: glide_done then knows it was stopped */
    awra_anim_stop_for(w);
  }
  const bool far = fabs(target - value) > page * GLIDE_PAGES;
  if (!animate || far || awra_style_reduced_motion() || !awra_aux(w)->mapped) {
    set_value(w, target);
    if (to_end) {
      x->follow = true;
    }
    queue_checks(w);
    return;
  }
  x->glide_from = value;
  x->glide_to = target;
  x->glide_end = to_end;
  x->gliding = true;
  awra_widget_ref(w);
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_SLOW), glide_anim, w, glide_done);
}

static void pill_clicked(AwraWidget *pill, const AwraArgs *args, void *data)
{
  (void)pill;
  (void)args;
  AwraWidget *w = (AwraWidget *)data;
  awra_widget_ref(w);
  awra_conversation_view_scroll_to_end(w, true);
  awra_emit_simple(w, AWRA_SIGNAL_JUMP_ACTIVATED);
  awra_widget_unref(w);
}

static void retry_clicked(AwraWidget *button, const AwraArgs *args, void *data)
{
  (void)args;
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  awra_emit_index(w, AWRA_SIGNAL_RETRY, button == x->edge_retry[0] ? 0 : 1);
}

static void make_edge(AwraWidget *w, int i)
{
  ConvExt *x = CVX(w);
  AwraWidget *line = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 8);
  x->edge_spinner[i] = awra_spinner_new();
  x->edge_icon[i] = awra_image_new_from_icon_name("dialog-warning-symbolic");
  awra_image_set_color(x->edge_icon[i], &awra_tokens()->danger);
  x->edge_label[i] = awra_label_new("");
  awra_label_set_ellipsize(x->edge_label[i], AWRA_ELLIPSIZE_END);
  awra_label_set_scale(x->edge_label[i], 0.92);
  awra_label_set_weight(x->edge_label[i], 600);
  x->edge_retry[i] = awra_button_new_with_label("Try again");
  awra_button_set_appearance(x->edge_retry[i], AWRA_BUTTON_GHOST);
  awra_widget_set_margins(x->edge_retry[i], -3, -8, -3, 0);
  awra_signal_connect(x->edge_retry[i], AWRA_SIGNAL_CLICKED, retry_clicked, w, NULL);
  awra_box_append(line, x->edge_spinner[i]);
  awra_box_append(line, x->edge_icon[i]);
  awra_box_append(line, x->edge_label[i]);
  awra_box_append(line, x->edge_retry[i]);
  awra_widget_unref(x->edge_spinner[i]);
  awra_widget_unref(x->edge_icon[i]);
  awra_widget_unref(x->edge_label[i]);
  awra_widget_unref(x->edge_retry[i]);
  AwraWidget *pill = pill_new(line, false);
  awra_widget_unref(line);
  awra_widget_set_margins(pill, 2, 8, 10, 8);
  AwraWidget *rev = awra_revealer_new();
  awra_revealer_set_transition(rev, i == 0 ? AWRA_REVEALER_SLIDE_DOWN : AWRA_REVEALER_SLIDE_UP, 180);
  awra_revealer_set_child(rev, pill);
  awra_widget_unref(pill);
  awra_child_insert(w, rev, -1);
  awra_widget_unref(rev);
  x->edges[i] = rev;
}

AwraWidget *awra_conversation_view_new(void)
{
  AwraWidget *w = awra_widget_new(&conv_class);
  ConvExt *x = CVX(w);
  x->follow = true;
  x->at_bottom = true;
  x->top_armed = true;
  x->bottom_armed = true;
  x->threshold = -1;
  x->highlight_pos = UINT32_MAX;
  x->scroller = awra_scrolled_new();
  awra_scrolled_set_whole_steps(x->scroller, true); /* a GtkListView's (GtkListBase) */
  awra_scrolled_set_policy(x->scroller, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  /* awraconversationview > scrolledwindow > scrollbar */
  awra_scrolled_set_scrollbar_style_internal(x->scroller, 1);
  x->list = awra_list_view_new();
  awra_list_view_set_padding(x->list, 8, 8);
  /* a short conversation sits at the bottom, next to where the next
   * message will appear, like the end of a longer one */
  awra_widget_set_valign(x->list, AWRA_ALIGN_END);
  awra_scrolled_set_child(x->scroller, x->list);
  awra_widget_unref(x->list);
  awra_child_insert(w, x->scroller, -1);
  awra_widget_unref(x->scroller);
  awra_signal_connect(x->scroller, AWRA_SIGNAL_VALUE_CHANGED, scrolled_changed, w, NULL);
  /* on glass, a hard edge under a header or above a composer reads as a seam */
  awra_scrolled_set_edge_fades(x->scroller, 16, 12);

  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 6);
  AwraWidget *icon = awra_image_new_from_icon_name("go-down-symbolic");
  awra_image_set_pixel_size(icon, 14);
  x->pill_label = awra_label_new("");
  awra_label_set_ellipsize(x->pill_label, AWRA_ELLIPSIZE_END);
  awra_label_set_scale(x->pill_label, 0.92);
  awra_label_set_weight(x->pill_label, 600);
  awra_box_append(box, icon);
  awra_box_append(box, x->pill_label);
  awra_widget_unref(icon);
  awra_widget_unref(x->pill_label);
  x->pill = pill_new(box, true);
  awra_widget_unref(box);
  awra_child_insert(w, x->pill, -1);
  awra_widget_unref(x->pill);
  awra_widget_set_child_visible(x->pill, false);
  awra_signal_connect(x->pill, AWRA_SIGNAL_CLICKED, pill_clicked, w, NULL);
  update_pill_label(w);
  make_edge(w, 0);
  make_edge(w, 1);
  return w;
}

AwraWidget *awra_conversation_view_list(AwraWidget *w)
{
  return CVX(w)->list;
}

void awra_conversation_view_items_changed(AwraWidget *w, unsigned position, unsigned removed,
                                          unsigned added)
{
  ConvExt *x = CVX(w);
  const unsigned old_n = x->n_items;
  x->n_items = old_n - removed + added;
  if (position == 0 && added > 0 && old_n > 0) {
    x->top_armed = true;
  }
  /* the highlighted row and those rising in move with their items */
  x->highlight_pos = moved_position(x->highlight_pos, position, removed, added);
  if (x->highlight_pos == UINT32_MAX) {
    x->highlight_until = 0;
  }
  int keep = 0;
  for (int i = 0; i < x->n_arriving; i++) {
    const unsigned p = moved_position(x->arriving[i], position, removed, added);
    if (p != UINT32_MAX) {
      x->arriving[keep] = p;
      x->arrived[keep++] = x->arrived[i];
    }
  }
  x->n_arriving = keep;
  const bool append = removed == 0 && added > 0 && position == old_n && old_n > 0;
  if (append && !x->follow) {
    x->bottom_armed = true;
  }
  awra_list_view_items_changed(x->list, position, removed, added);
  if (append && x->follow && added <= MAX_ARRIVING && awra_aux(w)->mapped &&
      awra_motion_duration(AWRA_MOTION_NORMAL) > 0)
  {
    const double now = awra_now_ms();
    for (unsigned p = position; p < position + added; p++) {
      if (x->n_arriving == MAX_ARRIVING) {
        /* the oldest makes room */
        memmove(x->arriving, x->arriving + 1, sizeof(unsigned) * (MAX_ARRIVING - 1));
        memmove(x->arrived, x->arrived + 1, sizeof(double) * (MAX_ARRIVING - 1));
        x->n_arriving--;
      }
      x->arriving[x->n_arriving] = p;
      x->arrived[x->n_arriving++] = now;
    }
    motion_start(w);
  }
  queue_checks(w);
}

bool awra_conversation_view_at_bottom(AwraWidget *w)
{
  return CVX(w)->at_bottom;
}

void awra_conversation_view_set_follow(AwraWidget *w, bool follow)
{
  CVX(w)->follow = follow;
  awra_queue_allocate(w);
}

bool awra_conversation_view_follow(AwraWidget *w)
{
  return CVX(w)->follow;
}

void awra_conversation_view_set_unread_count(AwraWidget *w, unsigned count)
{
  CVX(w)->unread = count;
  update_pill_label(w);
}

unsigned awra_conversation_view_unread_count(AwraWidget *w)
{
  return CVX(w)->unread;
}

void awra_conversation_view_set_jump_label(AwraWidget *w, const char *label)
{
  awra_set_str(&CVX(w)->jump_label, label);
  update_pill_label(w);
}

const char *awra_conversation_view_jump_label(AwraWidget *w)
{
  return CVX(w)->jump_label;
}

void awra_conversation_view_set_edge_threshold(AwraWidget *w, int threshold)
{
  CVX(w)->threshold = threshold;
}

void awra_conversation_view_scroll_to_end(AwraWidget *w, bool animate)
{
  glide_to(w, end_value(w), true, animate);
}

/* Reduced motion: the highlight stays whole, then goes at once. */
static bool highlight_end(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  x->highlight_timer = 0;
  invalidate_row(x, x->highlight_pos);
  x->highlight_until = 0;
  return false;
}

static bool scroll_item_idle(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ConvExt *x = CVX(w);
  const unsigned pos = x->highlight_pos;
  (void)pos;
  awra_widget_unref(w);
  return false;
}

void awra_conversation_view_scroll_to_item(AwraWidget *w, unsigned position, AwraConversationAlign align,
                                           bool highlight)
{
  ConvExt *x = CVX(w);
  if (position >= x->n_items) {
    return;
  }
  x->follow = false;
  if (x->gliding) {
    x->gliding = false; /* first: glide_done then knows it was stopped */
    awra_anim_stop_for(w);
  }
  awra_list_view_scroll_to(x->list, position, align == AWRA_CONVERSATION_ALIGN_TOP ? 1 :
                                              align == AWRA_CONVERSATION_ALIGN_END ? 3 :
                                                                                     2);
  if (highlight) {
    if (x->highlight_until > 0) {
      invalidate_row(x, x->highlight_pos);
    }
    x->highlight_pos = position;
    x->highlight_start = awra_now_ms();
    x->highlight_until = x->highlight_start + HIGHLIGHT_MS;
    if (x->highlight_timer) {
      awra_source_remove(x->highlight_timer);
      x->highlight_timer = 0;
    }
    if (awra_style_reduced_motion()) {
      x->highlight_timer = awra_timeout_add((unsigned)HIGHLIGHT_MS, highlight_end, w, NULL);
    }
    else {
      motion_start(w);
    }
  }
  awra_widget_ref(w);
  awra_idle_add(scroll_item_idle, w, NULL);
}

void awra_conversation_view_set_edge_status(AwraWidget *w, int edge, AwraEdgeStatus status, const char *label)
{
  ConvExt *x = CVX(w);
  const int i = edge == 0 ? 0 : 1;
  const bool failed = status == AWRA_EDGE_STATUS_FAILED;
  if (status != AWRA_EDGE_STATUS_NONE) {
    awra_label_set_text(x->edge_label[i], label ? label : "");
    awra_widget_set_visible(x->edge_label[i], label && label[0]);
    awra_widget_set_visible(x->edge_spinner[i], !failed);
    awra_spinner_set_spinning(x->edge_spinner[i], !failed);
    awra_widget_set_visible(x->edge_icon[i], failed);
    awra_widget_set_visible(x->edge_retry[i], failed);
  }
  else {
    awra_spinner_set_spinning(x->edge_spinner[i], false);
  }
  awra_revealer_set_reveal_child(x->edges[i], status != AWRA_EDGE_STATUS_NONE);
  if (x->edge_status[i] != AWRA_EDGE_STATUS_NONE && status == AWRA_EDGE_STATUS_NONE) {
    if (i == 0) {
      x->top_armed = true;
    }
    else {
      x->bottom_armed = true;
    }
  }
  x->edge_status[i] = status;
}

bool awra_conversation_view_visible_range(AwraWidget *w, unsigned *first, unsigned *last)
{
  return awra_list_view_visible_range(CVX(w)->list, first, last);
}
