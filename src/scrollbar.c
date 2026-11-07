/* The scrolled window's vertical overlay scrollbar, as GtkScrolledWindow
 * shows it with GTK's default theme (which Awra 1 left its scrollbars to):
 *
 * - an indicator, 3 px of rgba(fg, .4) in a 1 px border of white at .4,
 *   shown while the pointer moves over the window or it scrolls, fading in
 *   and out over a second (ease-out cubic), gone 2 s after it showed
 *   unless the pointer is near it (gtkscrolledwindow.c: indicator_*);
 * - near it (5 px, 10 px once there) or on it, after 30 ms, the full
 *   scrollbar (.hovering): a 1 px border of white at .8 around
 *   $scrollbar_bg_color at .8, an 8 px slider in $scrollbar_slider_color,
 *   darker under the pointer, the selection's while dragged; 300 ms
 *   (ease-out quad) between the two;
 * - pressing the trough warps the slider there (gtk-primary-button-warps-
 *   slider) and drags it, as pressing the slider does.
 *
 * The colours are GTK 4.22's default theme, light variant, the one GTK
 * draws under Awra 1 in light and dark alike. Contexts restyle it as
 * awra.css does (awra_scrolled_set_scrollbar_style_internal): lists and
 * the conversation view keep it 3 px in and show it only while the
 * pointer is over them; pickers 4 px / 2 px in at 60 %; the composer
 * 4 px from its ends; the rail and tab strips without one.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <math.h>

#define FADE_MS 1000.0   /* INDICATOR_FADE_OUT_DURATION */
#define HIDE_DELAY 2000.0 /* INDICATOR_FADE_OUT_DELAY */
#define CONCEAL_MS 500   /* INDICATOR_FADE_OUT_TIME */
#define OVER_MS 300.0    /* $_scrollbar_transition */
#define CLOSE 5          /* INDICATOR_CLOSE_DISTANCE */
#define FAR 10           /* INDICATOR_FAR_DISTANCE */

typedef struct BarExt {
  AwraWidget *sw; /* its scrolled window: its parent, not held */
  /* the fade (GtkProgressTracker over FADE_MS, eased out cubic) */
  float fade, fade_from, fade_to;
  double fade_start;
  double last_scroll;
  /* .hovering, and its transition */
  bool over;
  float over_t, over_from;
  double over_start;
  unsigned over_timer, conceal_timer;
  /* pressed: the slider follows the pointer */
  bool dragging, slider_hover;
  bool inside; /* the pointer is over the scrolled window */
  int px, py;  /* where, last (raster px) */
  bool drawn;  /* the last frame showed it */
  double drag_offset; /* raster px from the slider's top */
  /* the context's restyling (awra.css) */
  int style;
} BarExt;

#define BRX(w) AWRA_EXT(w, BarExt)

enum {
  STYLE_DEFAULT = 0,
  STYLE_LIST = 1,     /* awralistview / awraconversationview scrollbar */
  STYLE_PICKER = 2,   /* awraemojipicker / completion popover */
  STYLE_COMPOSER = 3, /* awracomposer's field */
  STYLE_NONE = 4,     /* the rail, tab strips */
  STYLE_PILL = 5,     /* Awra's 5 px pill: awrapage's, .awra-dropdown-popup's */
};

bool awra_scrolled_pill_bar_internal(AwraWidget *sw);

/* GTK's default theme scrollbar unless a widget restyled it, or Awra's pill
 * under an AwraPage (or a dropdown's popup) */
static int style_of(const BarExt *x)
{
  if (x->style == STYLE_DEFAULT && x->sw && awra_scrolled_pill_bar_internal(x->sw)) {
    return STYLE_PILL;
  }
  return x->style;
}

/* the scrolled window's metrics: value, upper, page (raster px) */
void awra_scrolled_metrics_internal(AwraWidget *sw, double *r_value, double *r_upper, double *r_page,
                                    bool *r_can);
void awra_scrolled_set_vvalue_px_internal(AwraWidget *sw, double value);

static double now(void)
{
  return awra_now_ms();
}

static float ease_out_cubic(float t)
{
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}

static float ease_out_quad(float t)
{
  const float u = 1.0f - t;
  return 1.0f - u * u;
}

static void style_margins(const BarExt *x, int m[4]) /* top, end, bottom, start */
{
  m[0] = m[1] = m[2] = m[3] = 0;
  if (x->style == STYLE_LIST) {
    m[0] = m[1] = m[2] = m[3] = 3;
  }
  else if (x->style == STYLE_PICKER) {
    m[0] = m[2] = 4;
    m[1] = m[3] = 2;
  }
  else if (x->style == STYLE_COMPOSER) {
    m[0] = m[2] = 4;
  }
}

/* The scrollbar's box, its trough, the slider's box and how far into the
 * hovering look it is, for the current state. */
typedef struct BarGeom {
  AwraBox bar, trough, slider;
  float q;      /* 0 the indicator .. 1 .hovering */
  float border; /* the scrollbar's border (hovering) */
  float inset;  /* the slider's transparent border (4 px hovering) */
  bool shown;
} BarGeom;

static bool geometry(AwraWidget *w, BarGeom *g)
{
  BarExt *x = BRX(w);
  AwraWidget *sw = x->sw;
  double value, upper, page;
  bool can;
  awra_scrolled_metrics_internal(sw, &value, &upper, &page, &can);
  memset(g, 0, sizeof(*g));
  if (!can || x->style == STYLE_NONE || sw->w <= 0 || sw->h <= 0) {
    return false;
  }
  if (style_of(x) == STYLE_PILL) {
    /* awra.css: a 6 px scrollbar, its slider 5 px wide and 18 long at
     * least, no border, @awra_thumb, the same in every state */
    const float bw = awra_pxf(5);
    g->bar = awra_mirror_box(sw, (AwraBox){(float)(sw->x + sw->w) - awra_pxf(6), (float)sw->y, bw, (float)sw->h});
    g->trough = g->bar;
    float len = upper > 0 ? (float)(g->bar.h * page / upper) : g->bar.h;
    len = fminf(fmaxf(len, awra_pxf(18)), g->bar.h);
    const double range = upper - page;
    const float pos = range > 0 ? (float)(value / range) * (g->bar.h - len) : 0.0f;
    g->slider = (AwraBox){g->bar.x, g->bar.y + pos, bw, len};
    g->shown = true;
    return true;
  }
  int m[4];
  style_margins(x, m);
  const float q = x->dragging ? 1.0f : ease_out_quad(x->over_t);
  g->q = q;
  /* the indicator: a 3 px slider in a 1 px border; hovering: 8 px in 4 px
   * of transparent border, in the scrollbar's 1 px border */
  const float min_w = x->style == STYLE_LIST ? 8.0f : x->style == STYLE_PICKER ? 6.0f : 0.0f;
  const float ind_w = fmaxf(5.0f, min_w), hov_w = fmaxf(18.0f, min_w);
  const float bw = awra_pxf(ind_w + (hov_w - ind_w) * q);
  g->border = awra_pxf(q);
  g->inset = awra_pxf(4.0f * q);
  const float x1 = (float)(sw->x + sw->w) - awra_pxf((float)m[1]);
  g->bar = (AwraBox){x1 - bw, (float)sw->y + awra_pxf((float)m[0]), bw,
                     (float)sw->h - awra_pxf((float)(m[0] + m[2]))};
  g->bar = awra_mirror_box(sw, g->bar);
  g->trough = (AwraBox){g->bar.x + g->border, g->bar.y + g->border, g->bar.w - 2 * g->border,
                        g->bar.h - 2 * g->border};
  /* GtkRange: page / upper of the trough, 40 px (and its borders) at least */
  const float min_len = awra_pxf(40.0f) + 2.0f * (awra_pxf(1.0f) + (g->inset - awra_pxf(q)));
  float len = upper > 0 ? (float)(g->trough.h * page / upper) : g->trough.h;
  len = fminf(fmaxf(len, min_len), g->trough.h);
  const double range = upper - page;
  const float pos = range > 0 ? (float)(value / range) * (g->trough.h - len) : 0.0f;
  /* the slider at its own width (5 px as an indicator, its trough's when
   * hovering), centred in a wider trough (a list's 8 px scrollbar) */
  const float sw_ = fminf(g->trough.w, awra_pxf(5.0f) + (g->trough.w - awra_pxf(5.0f)) * q);
  const float sx = g->trough.x + floorf((g->trough.w - sw_) * 0.5f);
  g->slider = (AwraBox){sx, g->trough.y + pos, sw_, len};
  g->shown = true;
  return true;
}

/* Repaints the strip the scrollbar can cover, at its widest: a narrowing
 * scrollbar must take what it drew wide with it. */
static void bar_dirty(AwraWidget *w)
{
  BarExt *x = BRX(w);
  AwraWidget *sw = x->sw;
  if (!sw || sw->w <= 0 || sw->h <= 0) {
    return;
  }
  BarGeom g;
  const bool showing = geometry(w, &g) && (x->fade > 0.0f || x->fade_to > 0.0f);
  if (!showing && !x->drawn) {
    return; /* nothing to show and nothing shown: nothing to repaint */
  }
  int m[4];
  style_margins(x, m);
  const int wide = awra_px(18 + m[1]) + 1;
  BKRect r = bk_rect((int)awra_mirror_x(sw, (float)(sw->x + sw->w - wide), (float)wide), sw->y, wide, sw->h);
  bk_node_dirty(sw, r);
}

static float gate(AwraWidget *w)
{
  /* lists: opacity 0 but while the pointer is over them (awra.css
   * awralistview:hover scrollbar); pickers: .6 */
  BarExt *x = BRX(w);
  if (x->style == STYLE_LIST) {
    return x->inside || x->dragging ? 1.0f : 0.0f;
  }
  return x->style == STYLE_PICKER ? 0.6f : 1.0f;
}

static void bar_draw(AwraWidget *w, AwraDraw *d)
{
  BarExt *x = BRX(w);
  BarGeom g;
  const float alpha = x->fade * gate(w);
  x->drawn = false;
  if (alpha <= 0.0f || !geometry(w, &g)) {
    return;
  }
  x->drawn = true;
  const float saved = d->opacity;
  d->opacity *= alpha;
  if (style_of(x) == STYLE_PILL) {
    awra_paint_rbox(d, g.slider, g.slider.w * 0.5f, &awra_tokens()->control_thumb, 0, NULL);
    d->opacity = saved;
    return;
  }
  /* GTK's default theme, in its variant for the desktop's scheme (not the
   * application's appearance): light fg #2e3436, bg #f6f5f4; dark fg
   * #eeeeec, bg #353535, base #2d2d2d */
  const bool dark = awra_style_gtk_dark();
  const AwraRGBA fg = dark ? (AwraRGBA){238 / 255.0f, 238 / 255.0f, 236 / 255.0f, 1} :
                             (AwraRGBA){46 / 255.0f, 52 / 255.0f, 54 / 255.0f, 1};
  const AwraRGBA bg = dark ? (AwraRGBA){53 / 255.0f, 53 / 255.0f, 53 / 255.0f, 1} :
                             (AwraRGBA){246 / 255.0f, 245 / 255.0f, 244 / 255.0f, 1};
  const AwraRGBA contrast = dark ? (AwraRGBA){0, 0, 0, 1} : (AwraRGBA){1, 1, 1, 1};
  const float q = g.q;
  if (q > 0.0f) {
    /* .hovering: gtkalpha ($scrollbar_bg_color, .8), its border white (black
     * in the dark) at .8; $scrollbar_bg_color is mix (base, bg) dark */
    AwraRGBA sb = dark ? (AwraRGBA){49 / 255.0f, 49 / 255.0f, 49 / 255.0f, 1} : awra_mix(bg, fg, 0.2f);
    sb.a = 0.8f * q;
    const AwraRGBA edge = awra_with_alpha(contrast, 0.8f * q);
    awra_paint_rect(d, g.bar, &sb);
    if (g.border > 0.0f) {
      const float b4[4] = {g.border, g.border, g.border, g.border};
      const float r4[4] = {0, 0, 0, 0};
      awra_paint_rbox4(d, g.bar, r4, NULL, b4, &edge);
    }
  }
  /* the slider: the indicator's rgba(fg, .4) in white at .4 turning into
   * $scrollbar_slider_color (mix (fg, bg, 60%)), .2 more of fg under the
   * pointer, the selection's darkened while dragged */
  AwraRGBA ind = fg;
  ind.a = 0.4f;
  AwraRGBA full = awra_mix(bg, fg, 0.6f);
  if (x->dragging) {
    full = (AwraRGBA){0x1b / 255.0f, 0x6a / 255.0f, 0xcb / 255.0f, 1};
  }
  else if (x->inside && x->px >= g.bar.x && x->px < g.bar.x + g.bar.w && x->py >= g.slider.y &&
           x->py < g.slider.y + g.slider.h) {
    full = awra_mix(bg, fg, 0.8f); /* slider:hover, where the pointer is now */
  }
  const AwraRGBA fill = awra_mix(ind, full, q);
  const AwraBox s = {g.slider.x + g.inset, g.slider.y + g.inset, g.slider.w - 2 * g.inset,
                     g.slider.h - 2 * g.inset};
  /* border-radius 10, the box's (the visible part keeps what is left) */
  const float r = fminf(awra_pxf(10.0f) - g.inset, fminf(s.w, s.h) * 0.5f);
  awra_paint_rbox(d, s, fmaxf(r, 0.0f), &fill, 0, NULL);
  if (q < 1.0f) {
    const AwraRGBA rim = awra_with_alpha(contrast, 0.4f * (1.0f - q));
    awra_paint_rbox(d, s, fmaxf(r, 0.0f), NULL, awra_pxf(1.0f), &rim);
  }
  d->opacity = saved;
}

/* -- animation: one frame callback drives the fade and the transition,
 * from their start times -- */

static void bar_tick(AwraWidget *w, double t, void *data)
{
  (void)t;
  (void)data;
  BarExt *x = BRX(w);
  const double n = now();
  if (x->fade != x->fade_to) {
    const float p = (float)fmin(1.0, (n - x->fade_start) / FADE_MS);
    x->fade = p >= 1.0f ? x->fade_to : x->fade_from + (x->fade_to - x->fade_from) * ease_out_cubic(p);
  }
  const float target = x->over ? 1.0f : 0.0f;
  if (x->over_t != target) {
    const float p = (float)fmin(1.0, (n - x->over_start) / OVER_MS);
    x->over_t = p >= 1.0f ? target : x->over_from + (target - x->over_from) * p;
  }
  bar_dirty(w);
}

static void animate(AwraWidget *w)
{
  /* a second covers the fade (begun now or before) and the transition */
  awra_anim_stop_for(w);
  awra_anim_start(w, (unsigned)FADE_MS, bar_tick, NULL, NULL);
  bar_dirty(w);
}

static bool conceal(void *data);

static void start_fade(AwraWidget *w, float target)
{
  BarExt *x = BRX(w);
  if (x->fade_to == target) {
    return;
  }
  x->fade_to = target;
  if (target != 0.0f) {
    x->last_scroll = now();
  }
  x->fade_from = x->fade;
  x->fade_start = now();
  if (awra_style_reduced_motion()) {
    x->fade = target;
    bar_dirty(w);
  }
  else {
    animate(w);
  }
  /* indicator_set_fade: a check every 500 ms while it shows */
  if (target > 0.0f && !x->conceal_timer) {
    x->conceal_timer = awra_timeout_add(CONCEAL_MS, conceal, w, NULL);
  }
}

static bool conceal(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  BarExt *x = BRX(w);
  if (now() - x->last_scroll >= HIDE_DELAY && !x->over && !x->dragging) {
    start_fade(w, 0.0f);
  }
  if (x->fade_to == 0.0f) {
    x->conceal_timer = 0;
    return false;
  }
  return true;
}

static void set_over(AwraWidget *w, bool over)
{
  BarExt *x = BRX(w);
  if (x->over_timer) {
    awra_source_remove(x->over_timer);
    x->over_timer = 0;
  }
  if (x->over == over) {
    return;
  }
  x->over = over;
  x->over_from = x->over_t;
  x->over_start = now();
  if (awra_style_reduced_motion()) {
    x->over_t = over ? 1.0f : 0.0f;
  }
  else {
    animate(w);
  }
  /* the scrollbar widens: its box changes */
  awra_queue_allocate(x->sw);
  bar_dirty(w);
}

static bool over_timeout(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  BRX(w)->over_timer = 0;
  set_over(w, true);
  return false;
}

/* -- called by the scrolled window -- */

void awra_scrollbar_value_changed_internal(AwraWidget *w);
void awra_scrollbar_value_changed_internal(AwraWidget *w)
{
  BarExt *x = BRX(w);
  x->last_scroll = now();
  start_fade(w, 1.0f);
}

/* captured_motion: any motion over the scrolled window */
void awra_scrollbar_motion_internal(AwraWidget *w, int px, int py, bool on_bar, bool buttons);
void awra_scrollbar_motion_internal(AwraWidget *w, int px, int py, bool on_bar, bool buttons)
{
  (void)py;
  BarExt *x = BRX(w);
  BarGeom g;
  x->px = px;
  x->py = py;
  if (!x->inside) {
    x->inside = true;
    bar_dirty(w);
  }
  if (!geometry(w, &g)) {
    return;
  }
  start_fade(w, 1.0f);
  if (buttons && !on_bar) {
    set_over(w, false);
    return;
  }
  /* coords_close_to_indicator, along x for the vertical one */
  const float dist = awra_pxf(x->over ? FAR : CLOSE);
  const bool close = px >= g.bar.x - dist && px < g.bar.x + g.bar.w + dist;
  if (x->over_timer) {
    awra_source_remove(x->over_timer);
    x->over_timer = 0;
  }
  if (on_bar) {
    set_over(w, true);
  }
  else if (close) {
    if (!x->over) {
      x->over_timer = awra_timeout_add(30, over_timeout, w, NULL);
    }
  }
  else {
    set_over(w, false);
  }
}

void awra_scrollbar_leave_internal(AwraWidget *w);
void awra_scrollbar_leave_internal(AwraWidget *w)
{
  BarExt *x = BRX(w);
  if (x->inside) {
    x->inside = false;
    bar_dirty(w);
  }
  if (!x->dragging) {
    set_over(w, false);
  }
}

/* The box the scrollbar takes events in (its look's, when shown). */
void awra_scrollbar_allocate_internal(AwraWidget *w);
void awra_scrollbar_allocate_internal(AwraWidget *w)
{
  BarExt *x = BRX(w);
  BarGeom g;
  const bool on = geometry(w, &g);
  awra_widget_set_child_visible(w, on);
  if (!on) {
    return;
  }
  /* at the width it is going to, so the pointer finds it */
  const BarExt saved = *x;
  x->over_t = x->over || x->dragging ? 1.0f : 0.0f;
  geometry(w, &g);
  x->over_t = saved.over_t;
  awra_allocate(w, (int)floorf(g.bar.x), (int)floorf(g.bar.y), (int)ceilf(g.bar.w), (int)ceilf(g.bar.h));
}

void awra_scrolled_set_scrollbar_style_internal(AwraWidget *w, int style);
void awra_scrollbar_set_style_internal(AwraWidget *w, int style);
void awra_scrollbar_set_style_internal(AwraWidget *w, int style)
{
  BRX(w)->style = style;
  awra_queue_allocate(BRX(w)->sw);
  bar_dirty(w);
}

/* -- its own input: the slider dragged, the trough pressed -- */

static void drag_to(AwraWidget *w, int py)
{
  BarExt *x = BRX(w);
  BarGeom g;
  double value, upper, page;
  bool can;
  if (!geometry(w, &g)) {
    return;
  }
  awra_scrolled_metrics_internal(x->sw, &value, &upper, &page, &can);
  const double span = g.trough.h - g.slider.h;
  const double top = py - x->drag_offset - g.trough.y;
  const double v = span > 0 ? top / span * (upper - page) : 0;
  awra_scrolled_set_vvalue_px_internal(x->sw, v);
}

static int bar_press(AwraWidget *w, BKEvent *e)
{
  BarExt *x = BRX(w);
  BarGeom g;
  if (!geometry(w, &g)) {
    return 0;
  }
  set_over(w, true);
  if (e->y >= g.slider.y && e->y < g.slider.y + g.slider.h) {
    x->drag_offset = e->y - g.slider.y;
  }
  else {
    /* gtk-primary-button-warps-slider: the slider's middle comes here */
    x->drag_offset = g.slider.h * 0.5;
    drag_to(w, e->y);
  }
  x->dragging = true;
  bar_dirty(w);
  return 1;
}

static int bar_drag(AwraWidget *w, BKEvent *e)
{
  if (BRX(w)->dragging) {
    drag_to(w, e->y);
  }
  return 1;
}

static int bar_release(AwraWidget *w, BKEvent *e)
{
  (void)e;
  BarExt *x = BRX(w);
  x->dragging = false;
  x->last_scroll = now();
  bar_dirty(w);
  return 1;
}

static int bar_motion(AwraWidget *w, BKEvent *e)
{
  BarExt *x = BRX(w);
  BarGeom g;
  const bool on = geometry(w, &g) && e->y >= g.slider.y && e->y < g.slider.y + g.slider.h;
  if (on != x->slider_hover) {
    x->slider_hover = on;
    bar_dirty(w);
  }
  return 1;
}

static void bar_hover(AwraWidget *w, bool on)
{
  BarExt *x = BRX(w);
  if (!on && x->slider_hover) {
    x->slider_hover = false;
    bar_dirty(w);
  }
}

static void bar_finalize(AwraWidget *w)
{
  BarExt *x = BRX(w);
  if (x->over_timer) {
    awra_source_remove(x->over_timer);
  }
  if (x->conceal_timer) {
    awra_source_remove(x->conceal_timer);
  }
}

static AwraClass bar_class = {
    .bk = AWRA_BK("AwraScrollbar", 0),
    .type = "AwraScrollbar",
    .ext_size = sizeof(BarExt),
    .finalize = bar_finalize,
    .draw = bar_draw,
    .press = bar_press,
    .drag = bar_drag,
    .release = bar_release,
    .motion = bar_motion,
    .hover = bar_hover,
};

AwraWidget *awra_scrollbar_new_internal(AwraWidget *sw);
AwraWidget *awra_scrollbar_new_internal(AwraWidget *sw)
{
  AwraWidget *w = awra_widget_new(&bar_class);
  BRX(w)->sw = sw;
  w->flags |= NF_EVENTS;
  return w;
}
