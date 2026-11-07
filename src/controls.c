/* Awra 2 controls: check buttons, switches, sliders, volume controls,
 * segmented controls, progress bars and spinners (spec B §3-7, §14).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

extern AwraClass awra_button_class;
void awra_button_set_look(AwraWidget *w, int look);

/* ======================================================================= */
/* Check button (check and radio) */

typedef struct CheckExt {
  bool active, inconsistent, radio;
  AwraWidget *label;
  AwraWidget *group_next;
} CheckExt;

#define CX(w) AWRA_EXT(w, CheckExt)

static int check_box_px(void)
{
  return awra_px(19);
}

static void check_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  CheckExt *x = CX(w);
  int lm = 0, ln = 0;
  if (x->label && awra_child_visible(x->label)) {
    awra_measure(x->label, o, -1, &lm, &ln);
  }
  const int box = check_box_px();
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    const int gap = ln ? awra_px(8) : 0;
    *r_min = box + gap + lm;
    *r_nat = box + gap + ln;
  }
  else {
    *r_min = BK_MAX(box, lm);
    *r_nat = BK_MAX(box, ln);
  }
}

static void check_allocate(AwraWidget *w)
{
  CheckExt *x = CX(w);
  if (x->label) {
    const int off = check_box_px() + awra_px(8);
    awra_allocate(x->label, w->x + off, w->y, BK_MAX(0, w->w - off), w->h);
  }
}

static void check_draw(AwraWidget *w, AwraDraw *d)
{
  CheckExt *x = CX(w);
  const AwraTokens *t = awra_tokens();
  const int box = check_box_px();
  AwraBox b = {(float)w->x, (float)w->y + floorf((w->h - box) * 0.5f + 0.5f), (float)box, (float)box};
  const float r = x->radio ? box * 0.5f : awra_pxf(6);
  const bool on = x->active;
  const bool hot = awra_hovered(w) || awra_focus_visible(w);
  if (hot) { /* outline 2px selection, offset 1 */
    const float ow = awra_pxf(2), off = awra_pxf(1);
    AwraBox o = {b.x - off - ow, b.y - off - ow, b.w + 2 * (off + ow), b.h + 2 * (off + ow)};
    awra_paint_rbox(d, o, r + off + ow, NULL, ow, &t->selection);
  }
  const AwraRGBA fill = on ? t->accent : t->control_well;
  const AwraRGBA border = on ? t->accent : (hot ? t->focus_ring : t->outline);
  awra_paint_rbox(d, b, r, &fill, awra_pxf(1), &border);
  if (!on) { /* inset 0 1px 2px rgba(0,0,0,.12) */
    const AwraRGBA sh = {0, 0, 0, 0.09f};
    const float r4[4] = {r, r, r, r}, w4[4] = {awra_pxf(1.5), 0, 0, 0};
    awra_paint_rbox4(d, awra_box_inset(b, awra_pxf(1)), r4, NULL, w4, &sh);
  }
  if (on || x->inconsistent) {
    const AwraRGBA white = {1, 1, 1, on ? 1.0f : 0.0f};
    const int s = awra_px(16);
    const char *glyph = x->inconsistent ? "mixed-symbolic" : x->radio ? "radio-symbolic" :
                                                                         "check-symbolic";
    awra_paint_icon(d, glyph, b.x + (box - s) * 0.5f, b.y + (box - s) * 0.5f, s, &white);
  }
}

static bool check_child_style(AwraWidget *w, AwraRGBA *c, int *weight)
{
  *c = awra_sensitive(w) ? awra_tokens()->foreground : awra_rgba(0.57f, 0.58f, 0.58f, 1);
  *weight = 0;
  return true;
}

static void check_set(AwraWidget *w, bool active)
{
  CheckExt *x = CX(w);
  if (x->active == active) {
    return;
  }
  x->active = active;
  awra_invalidate(w);
  if (active) {
    for (AwraWidget *o = x->group_next; o && o != w; o = CX(o)->group_next) {
      if (CX(o)->active) {
        CX(o)->active = false;
        awra_invalidate(o);
        awra_emit_active(o, AWRA_SIGNAL_TOGGLED, false);
      }
    }
  }
  awra_emit_active(w, AWRA_SIGNAL_TOGGLED, active);
}

static void check_activate(AwraWidget *w)
{
  CheckExt *x = CX(w);
  if (x->radio && x->active) {
    return;
  }
  x->inconsistent = false;
  check_set(w, !x->active);
}

static int check_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside) {
    check_activate(w);
  }
  return 1;
}

static void check_init(AwraWidget *w)
{
  CX(w)->group_next = w;
}

static void check_finalize(AwraWidget *w)
{
  CheckExt *x = CX(w);
  if (x->group_next && x->group_next != w) {
    AwraWidget *p = x->group_next;
    while (CX(p)->group_next != w) {
      p = CX(p)->group_next;
    }
    CX(p)->group_next = x->group_next;
  }
}

/* GtkCheckButton: a check box, or a radio button in a group */
static void check_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  CheckExt *x = CX(w);
  if (x->radio || (x->group_next && x->group_next != w)) {
    info->role = AWRA_ACCESSIBLE_ROLE_RADIO;
  }
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_CHECKED,
                      x->inconsistent ? AWRA_ACCESSIBLE_TRISTATE_MIXED : x->active);
}

static AwraClass check_class = {
    .a11y = check_a11y,
    .bk = AWRA_BK("AwraCheckButton", CLS_INTERACTIVE),
    .type = "AwraCheckButton",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(CheckExt),
    .init = check_init,
    .finalize = check_finalize,
    .measure = check_measure,
    .allocate = check_allocate,
    .draw = check_draw,
    .release = check_release,
    .activate = check_activate,
    .child_style = check_child_style,
    .overdraw = 4,
};

AwraWidget *awra_check_button_new(void)
{
  return awra_widget_new(&check_class);
}

AwraWidget *awra_check_button_new_with_label(const char *label)
{
  AwraWidget *w = awra_check_button_new();
  awra_check_button_set_label(w, label);
  return w;
}

void awra_check_button_set_active(AwraWidget *w, bool a)
{
  check_set(w, a);
}

bool awra_check_button_active(AwraWidget *w)
{
  return CX(w)->active;
}

void awra_check_button_set_label(AwraWidget *w, const char *label)
{
  CheckExt *x = CX(w);
  if (!x->label) {
    x->label = awra_label_new(label);
    awra_label_set_xalign(x->label, 0.0f);
    awra_child_insert(w, x->label, -1);
  }
  else {
    awra_label_set_text(x->label, label);
  }
}

void awra_check_button_set_group(AwraWidget *w, AwraWidget *m)
{
  if (!m || m == w) {
    return;
  }
  CX(w)->radio = CX(m)->radio = true;
  CX(w)->group_next = CX(m)->group_next;
  CX(m)->group_next = w;
  awra_invalidate(w);
  awra_invalidate(m);
}

void awra_check_button_set_inconsistent(AwraWidget *w, bool i)
{
  CX(w)->inconsistent = i;
  awra_invalidate(w);
}

/* ======================================================================= */
/* Switch: 48 x 28, thumb 21 x 22 (spec B §4) */

typedef struct SwitchExt {
  bool active;
  float pos; /* handle position 0..1 */
} SwitchExt;

#define SWX(w) AWRA_EXT(w, SwitchExt)

static void switch_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  *r_min = *r_nat = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? 48 : 28);
}

static void switch_draw(AwraWidget *w, AwraDraw *d)
{
  SwitchExt *x = SWX(w);
  const AwraTokens *t = awra_tokens();
  const int bw = awra_px(48), bh = awra_px(28);
  AwraBox b = {(float)w->x, (float)w->y + floorf((w->h - bh) * 0.5f), (float)bw, (float)bh};
  const float saved = d->opacity;
  if (!awra_sensitive(w)) {
    d->opacity *= 0.58f;
  }
  const bool on = x->active && awra_sensitive(w);
  /* the color follows the state with a 110 ms transition, carried by pos */
  awra_paint_rbox(d, b, bh * 0.5f, on ? &t->accent : &t->control_well, 0, NULL);
  if (on) {
    const AwraRGBA edge = {1, 1, 1, 0.08f};
    awra_paint_rbox(d, b, bh * 0.5f, NULL, awra_pxf(1), &edge);
  }
  else { /* inset 0 1px 2px rgba(0,0,0,.22) */
    const AwraRGBA sh = {0, 0, 0, 0.16f};
    const float r = bh * 0.5f, r4[4] = {r, r, r, r}, w4[4] = {awra_pxf(1.5), 0, 0, 0};
    awra_paint_rbox4(d, b, r4, NULL, w4, &sh);
  }
  const float tw = awra_pxf(21), th = awra_pxf(22), inset = awra_pxf(3);
  AwraBox thumb = {b.x + inset + floorf(x->pos * tw + 0.5f), b.y + inset, tw, th};
  const AwraRGBA shadow = {0, 0, 0, 0.28f};
  awra_paint_shadow(d, thumb, th * 0.5f, 0, awra_pxf(1), 0, awra_pxf(4), &shadow, true);
  awra_paint_rbox(d, thumb, th * 0.5f, &t->control_thumb, 0, NULL);
  if (awra_focus_visible(w)) {
    const float fw = awra_pxf(3), off = awra_pxf(2);
    AwraBox o = {b.x - off - fw, b.y - off - fw, b.w + 2 * (off + fw), b.h + 2 * (off + fw)};
    awra_paint_rbox(d, o, bh * 0.5f + off + fw, NULL, fw, &t->focus_ring);
  }
  d->opacity = saved;
}

static void switch_anim(AwraWidget *w, double t, void *data)
{
  SwitchExt *x = SWX(w);
  float from;
  memcpy(&from, &data, sizeof(float));
  const float to = x->active ? 1.0f : 0.0f;
  x->pos = from + (to - from) * (float)t;
  awra_invalidate(w);
}

static void switch_set(AwraWidget *w, bool active, bool animate)
{
  SwitchExt *x = SWX(w);
  if (x->active == active) {
    return;
  }
  x->active = active;
  awra_anim_stop_for(w);
  if (animate && awra_widget_mapped(w)) {
    void *data = NULL;
    float from = x->pos;
    memcpy(&data, &from, sizeof(float));
    awra_anim_start(w, 100, switch_anim, data, NULL);
  }
  else {
    x->pos = active ? 1.0f : 0.0f;
    awra_invalidate(w);
  }
  awra_emit_active(w, AWRA_SIGNAL_TOGGLED, active);
}

static void switch_activate(AwraWidget *w)
{
  switch_set(w, !SWX(w)->active, true);
}

static int switch_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside) {
    switch_activate(w);
  }
  return 1;
}

static void switch_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_CHECKED, SWX(w)->active);
}

static AwraClass switch_class = {
    .a11y = switch_a11y,
    .bk = AWRA_BK("AwraSwitch", CLS_INTERACTIVE),
    .type = "AwraSwitch",
    .flags = AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(SwitchExt),
    .measure = switch_measure,
    .draw = switch_draw,
    .release = switch_release,
    .activate = switch_activate,
    .overdraw = 6,
};

AwraWidget *awra_switch_new(void)
{
  AwraWidget *w = awra_widget_new(&switch_class);
  awra_widget_set_valign(w, AWRA_ALIGN_CENTER);
  return w;
}

void awra_switch_set_active(AwraWidget *w, bool a)
{
  switch_set(w, a, false);
}

bool awra_switch_active(AwraWidget *w)
{
  return SWX(w)->active;
}

/* ======================================================================= */
/* Slider (GtkScale, spec B §5): 19 px tall, 6 px trough, 16 px thumb */

typedef struct SliderExt {
  double min, max, step, value;
  int orientation;
  bool muted; /* volume: highlight in the disabled color */
  bool dragging;
  double marks[8]; /* values ticked under (beside) the trough */
  int nmarks;
  int digits; /* GtkScale:draw-value: the value above, with these decimals; -1 none */
} SliderExt;

#define SLX(w) AWRA_EXT(w, SliderExt)

/* GtkScale's value text: "%.*f" of the value (C locale) */
static int slider_value_text(const SliderExt *x, double v, char *buf, size_t size)
{
  int n = snprintf(buf, size, "%.*f", x->digits, v);
  if (n > 1 && buf[0] == '-' && strspn(buf + 1, "0.") == (size_t)(n - 1)) {
    memmove(buf, buf + 1, (size_t)n); /* weed_out_neg_zero */
    n--;
  }
  return n;
}

/* Value text above a horizontal slider (GTK_POS_TOP): its line, then the
 * range, whose trough sits at the bottom (GtkScale with draw-value under
 * GTK's default theme and awra.css: a 17 px value line makes it 35 px). */
static int slider_value_height(const AwraWidget *w)
{
  const SliderExt *x = SLX(w);
  if (x->digits < 0 || x->orientation != AWRA_ORIENTATION_HORIZONTAL) {
    return 0;
  }
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  return (int)ceilf(awra_font_line_height(&f));
}

static void slider_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  const bool along = o == SLX(w)->orientation;
  const int vh = slider_value_height(w);
  if (vh && along) {
    /* as wide as the widest value it shows */
    const SliderExt *x = SLX(w);
    AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    char a[64], b[64];
    const int na = slider_value_text(x, x->min, a, sizeof(a)), nb = slider_value_text(x, x->max, b, sizeof(b));
    const int tw = (int)ceilf(fmaxf(awra_text_width(&f, a, na), awra_text_width(&f, b, nb)));
    *r_min = *r_nat = BK_MAX(awra_px(15), tw);
    return;
  }
  if (vh) {
    *r_min = *r_nat = vh + awra_px(18);
    return;
  }
  *r_min = *r_nat = along ? awra_px(15) : awra_px(19) + (SLX(w)->nmarks ? awra_px(8) : 0);
}

static double slider_frac(const AwraWidget *w)
{
  const SliderExt *x = SLX(w);
  return x->max > x->min ? (x->value - x->min) / (x->max - x->min) : 0.0;
}

static void slider_draw(AwraWidget *w, AwraDraw *d)
{
  SliderExt *x = SLX(w);
  const AwraTokens *t = awra_tokens();
  const float saved = d->opacity;
  if (!awra_sensitive(w)) {
    d->opacity *= 0.58f;
  }
  const bool horiz = x->orientation == AWRA_ORIENTATION_HORIZONTAL;
  const float th = awra_pxf(6);
  const float frac = (float)slider_frac(w);
  /* marks take a strip after the trough */
  const int mk = x->nmarks ? awra_px(8) : 0;
  const int vh = slider_value_height(w);
  AwraBox trough = horiz ? (AwraBox){(float)w->x, vh ? (float)(w->y + w->h) - th : (float)w->y + floorf((w->h - mk - th) * 0.5f), (float)w->w, th} :
                           (AwraBox){(float)w->x + floorf((w->w - mk - th) * 0.5f), (float)w->y, th, (float)w->h};
  for (int i = 0; i < x->nmarks; i++) {
    const double f = x->max > x->min ? (x->marks[i] - x->min) / (x->max - x->min) : 0;
    const float len = (horiz ? trough.w : trough.h) - th;
    const float at = floorf((float)f * len + th * 0.5f);
    const float tw = awra_pxf(1), tl = awra_pxf(6);
    AwraBox tick = horiz ? (AwraBox){trough.x + at, trough.y + th + awra_pxf(3), tw, tl} :
                           (AwraBox){trough.x + th + awra_pxf(3), trough.y + trough.h - th * 0.5f - at, tl, tw};
    awra_paint_rbox(d, tick, 0, &t->muted, 0, NULL);
  }
  awra_paint_rbox(d, trough, th * 0.5f, &t->control_track, 0, NULL);
  { /* inset 0 1px 2px rgba(0,0,0,.18) */
    const AwraRGBA sh = {0, 0, 0, 0.13f};
    const float r = th * 0.5f, r4[4] = {r, r, r, r}, w4[4] = {awra_pxf(1.2), 0, 0, 0};
    awra_paint_rbox4(d, trough, r4, NULL, w4, &sh);
  }
  const float len = horiz ? trough.w : trough.h;
  const float mpos = floorf((len - th) * frac); /* the slider's margin box (6 px) */
  const AwraRGBA hl = x->muted ? t->disabled : t->accent;
  if (horiz) {
    awra_paint_rbox(d, (AwraBox){trough.x, trough.y, fmaxf(th, frac * len), th}, th * 0.5f, &hl, 0,
                    NULL);
  }
  else {
    const float hh = fmaxf(th, frac * len);
    awra_paint_rbox(d, (AwraBox){trough.x, trough.y + len - hh, th, hh}, th * 0.5f, &hl, 0, NULL);
  }
  const float ts = awra_pxf(16);
  const float cxy = (horiz ? trough.x : trough.y) + (horiz ? mpos : (len - th - mpos)) + th * 0.5f;
  AwraBox thumb = horiz ? (AwraBox){cxy - ts * 0.5f, trough.y + th * 0.5f - ts * 0.5f, ts, ts} :
                          (AwraBox){trough.x + th * 0.5f - ts * 0.5f, cxy - ts * 0.5f, ts, ts};
  const AwraRGBA shadow = {0, 0, 0, 0.30f}, edge = {0, 0, 0, 0.12f};
  awra_paint_shadow(d, thumb, ts * 0.5f, 0, awra_pxf(2), 0, awra_pxf(6), &shadow, true);
  awra_paint_rbox(d, thumb, ts * 0.5f, &t->control_thumb, awra_pxf(1), &edge);
  if (vh) {
    /* scale > value { color: alpha(currentColor, .55) }, centered on the
     * thumb and kept inside the slider */
    char buf[64];
    const int n = slider_value_text(x, x->value, buf, sizeof(buf));
    AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    const float tw = ceilf(awra_text_width(&f, buf, n));
    const float tx = fminf(fmaxf((float)w->x, floorf(cxy - tw * 0.5f)), (float)(w->x + w->w) - tw);
    const AwraRGBA c = awra_with_alpha(t->foreground, 0.55f);
    awra_draw_text(d, &f, tx, (float)w->y + floorf(awra_font_ascent(&f) + 0.5f), buf, n, &c);
  }
  d->opacity = saved;
}

static void slider_set_value_internal(AwraWidget *w, double v, bool notify)
{
  SliderExt *x = SLX(w);
  if (x->step > 0) {
    v = x->min + floor((v - x->min) / x->step + 0.5) * x->step;
  }
  v = v < x->min ? x->min : v > x->max ? x->max : v;
  if (v == x->value) {
    return;
  }
  x->value = v;
  awra_invalidate(w);
  if (notify) {
    awra_emit_value(w, AWRA_SIGNAL_VALUE_CHANGED, v);
  }
}

static void slider_from_pointer(AwraWidget *w, int px, int py)
{
  SliderExt *x = SLX(w);
  const bool horiz = x->orientation == AWRA_ORIENTATION_HORIZONTAL;
  const float th = awra_pxf(6);
  const float len = (horiz ? w->w : w->h) - th;
  float pos = horiz ? (px - w->x - th * 0.5f) : (w->y + w->h - th * 0.5f - py);
  double frac = len > 0 ? pos / len : 0;
  frac = frac < 0 ? 0 : frac > 1 ? 1 : frac;
  slider_set_value_internal(w, x->min + frac * (x->max - x->min), true);
}

static int slider_press(AwraWidget *w, BKEvent *e)
{
  SLX(w)->dragging = true;
  slider_from_pointer(w, e->x, e->y);
  return 1;
}

static int slider_drag(AwraWidget *w, BKEvent *e)
{
  slider_from_pointer(w, e->x, e->y);
  return 1;
}

static int slider_release(AwraWidget *w, BKEvent *e)
{
  SLX(w)->dragging = false;
  return 1;
}

static int slider_key(AwraWidget *w, BKEvent *e)
{
  SliderExt *x = SLX(w);
  const double step = x->step > 0 ? x->step : (x->max - x->min) / 100.0;
  if (bk_keysym_is(e->keysym, "Right") || bk_keysym_is(e->keysym, "Up")) {
    slider_set_value_internal(w, x->value + step, true);
    return 1;
  }
  if (bk_keysym_is(e->keysym, "Left") || bk_keysym_is(e->keysym, "Down")) {
    slider_set_value_internal(w, x->value - step, true);
    return 1;
  }
  if (bk_keysym_is(e->keysym, "Home")) {
    slider_set_value_internal(w, x->min, true);
    return 1;
  }
  if (bk_keysym_is(e->keysym, "End")) {
    slider_set_value_internal(w, x->max, true);
    return 1;
  }
  return 0;
}

static int slider_scroll(AwraWidget *w, BKEvent *e)
{
  /* an unfocused slider lets the page scroll (awra-controls.c) */
  if (!awra_widget_has_focus(w)) {
    return 0;
  }
  SliderExt *x = SLX(w);
  const double step = x->step > 0 ? x->step : (x->max - x->min) / 100.0;
  slider_set_value_internal(w, x->value + e->wheel_y * step, true);
  return 1;
}

static bool slider_get_double(AwraWidget *w, const char *name, double *v)
{
  if (!strcmp(name, "value")) {
    *v = SLX(w)->value;
    return true;
  }
  return false;
}

static bool slider_set_double(AwraWidget *w, const char *name, double v)
{
  if (!strcmp(name, "value")) {
    awra_slider_set_value(w, v);
    return true;
  }
  return false;
}

/* GtkScale: a range */
static void slider_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  SliderExt *x = SLX(w);
  info->has_value = true;
  info->value = x->value;
  info->min = x->min;
  info->max = x->max;
  info->step = x->step > 0 ? x->step : (x->max - x->min) / 100.0;
  info->flags |= x->orientation == AWRA_ORIENTATION_HORIZONTAL ? AWRA_A11Y_HORIZONTAL
                                                               : AWRA_A11Y_VERTICAL;
}

static bool slider_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  if (item < 0 && verb == AWRA_A11Y_DO_SET_VALUE && awra_sensitive(w)) {
    slider_set_value_internal(w, value, true);
    return true;
  }
  return false;
}

AwraClass awra_slider_class = {
    .a11y = slider_a11y,
    .a11y_do = slider_a11y_do,
    .bk = AWRA_BK("AwraSlider", CLS_INTERACTIVE),
    .type = "AwraSlider",
    .flags = AWRA_CLASS_FOCUSABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(SliderExt),
    .measure = slider_measure,
    .draw = slider_draw,
    .press = slider_press,
    .drag = slider_drag,
    .release = slider_release,
    .key = slider_key,
    .scroll = slider_scroll,
    .get_double = slider_get_double,
    .set_double = slider_set_double,
    .overdraw = 12,
};

AwraWidget *awra_slider_new(double min, double max, double step)
{
  AwraWidget *w = awra_widget_new(&awra_slider_class);
  SliderExt *x = SLX(w);
  x->min = min;
  x->max = max > min ? max : min + 1;
  x->step = step;
  x->value = min;
  x->digits = -1;
  return w;
}

void awra_slider_set_draw_value(AwraWidget *w, bool draw, int digits)
{
  SLX(w)->digits = draw ? BK_CLAMP(digits, 0, 10) : -1;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_slider_set_value(AwraWidget *w, double v)
{
  slider_set_value_internal(w, v, true);
}

double awra_slider_value(AwraWidget *w)
{
  return SLX(w)->value;
}

void awra_slider_set_range(AwraWidget *w, double min, double max)
{
  SliderExt *x = SLX(w);
  x->min = min;
  x->max = max > min ? max : min + 1e-9;
  slider_set_value_internal(w, x->value, false);
  awra_invalidate(w);
}

void awra_slider_set_orientation(AwraWidget *w, AwraOrientation o)
{
  SLX(w)->orientation = o;
  awra_widget_queue_resize(w);
}

void awra_slider_set_muted_internal(AwraWidget *w, bool muted);
void awra_slider_set_muted_internal(AwraWidget *w, bool muted)
{
  SLX(w)->muted = muted;
  awra_invalidate(w);
}

/* ======================================================================= */
/* Volume control (spec B §6): mute button + 88 px slider, spacing 2 */

typedef struct VolumeExt {
  AwraWidget *button, *slider;
  bool muted;
} VolumeExt;

#define VX(w) ((VolumeExt *)awra_widget_get_data(w, "awra-volume"))

static const char *volume_icon(double v, bool muted)
{
  if (muted || v <= 0.0) {
    return "audio-volume-muted-symbolic";
  }
  return v < 0.34 ? "audio-volume-low-symbolic" :
         v < 0.67 ? "audio-volume-medium-symbolic" : "audio-volume-high-symbolic";
}

static void volume_sync(AwraWidget *w)
{
  VolumeExt *x = VX(w);
  const double v = awra_slider_value(x->slider);
  awra_button_set_icon_name(x->button, volume_icon(v, x->muted));
  awra_widget_set_tooltip(x->button, x->muted ? "Unmute" : "Mute");
  char tip[16];
  snprintf(tip, sizeof(tip), "%d %%", (int)(v * 100 + 0.5));
  awra_widget_set_tooltip(x->slider, tip);
  awra_slider_set_muted_internal(x->slider, x->muted);
  awra_accessible_update_property_string(x->slider, AWRA_ACCESSIBLE_PROPERTY_VALUE_TEXT, tip);
  awra_accessible_update_state(x->button, AWRA_ACCESSIBLE_STATE_PRESSED, x->muted);
}

static void volume_slider_changed(AwraWidget *s, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  volume_sync(w);
  awra_emit_value(w, AWRA_SIGNAL_VALUE_CHANGED, a->value);
}

static void volume_button_clicked(AwraWidget *b, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  awra_volume_control_set_muted(w, !VX(w)->muted);
}


AwraWidget *awra_volume_control_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 2);
  /* a box with the volume control's data */
  VolumeExt *x = (VolumeExt *)calloc(1, sizeof(VolumeExt));
  awra_widget_set_data(w, "awra-volume", x, free);
  x->button = awra_button_new_from_icon_name("audio-volume-high-symbolic");
  awra_button_set_appearance(x->button, AWRA_BUTTON_TOOLBAR);
  awra_widget_set_valign(x->button, AWRA_ALIGN_CENTER);
  x->slider = awra_slider_new(0, 1, 0);
  awra_widget_set_size_request(x->slider, 88, -1);
  awra_widget_set_hexpand(x->slider, true);
  awra_widget_set_valign(x->slider, AWRA_ALIGN_CENTER);
  awra_box_append(w, x->button);
  awra_box_append(w, x->slider);
  awra_signal_connect(x->slider, AWRA_SIGNAL_VALUE_CHANGED, volume_slider_changed, w, NULL);
  awra_signal_connect(x->button, AWRA_SIGNAL_CLICKED, volume_button_clicked, w, NULL);
  awra_slider_set_value(x->slider, 1.0);
  volume_sync(w);
  awra_widget_isolate_expand(w);
  /* Awra 1's AwraVolumeControl: a group, its slider and mute button */
  awra_accessible_set_role(w, AWRA_ACCESSIBLE_ROLE_GROUP);
  awra_accessible_update_property_string(w, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Volume");
  awra_accessible_update_property_string(x->slider, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Volume");
  return w;
}


void awra_volume_control_set_value(AwraWidget *w, double v)
{
  if (VX(w)) {
    awra_slider_set_value(VX(w)->slider, v);
  }
}

double awra_volume_control_value(AwraWidget *w)
{
  return VX(w) ? awra_slider_value(VX(w)->slider) : 0.0;
}

void awra_volume_control_set_muted(AwraWidget *w, bool muted)
{
  VolumeExt *x = VX(w);
  if (!x || x->muted == muted) {
    return;
  }
  x->muted = muted;
  volume_sync(w);
  awra_emit_active(w, AWRA_SIGNAL_TOGGLED, muted);
}

bool awra_volume_control_muted(AwraWidget *w)
{
  return VX(w) && VX(w)->muted;
}

/* ======================================================================= */
/* Segmented control (spec B §3): a well (radius 12) of toggle segments */

typedef struct SegExt {
  AwraWidget *box;
  int selected;
} SegExt;

#define SGX(w) AWRA_EXT(w, SegExt)

static void seg_draw(AwraWidget *w, AwraDraw *d)
{
  awra_paint_rbox(d, awra_box_of(w), awra_pxf(12), &awra_tokens()->control_well, 0, NULL);
}

static void seg_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  awra_measure(SGX(w)->box, o, f, mn, nt);
}

static void seg_allocate(AwraWidget *w)
{
  awra_allocate(SGX(w)->box, w->x, w->y, w->w, w->h);
}

static AwraClass seg_class = {
    .bk = AWRA_BK("AwraSegmentedControl", 0),
    .type = "AwraSegmentedControl",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(SegExt),
    .measure = seg_measure,
    .allocate = seg_allocate,
    .draw = seg_draw,
};

AwraWidget *awra_segmented_control_new(void)
{
  AwraWidget *w = awra_widget_new(&seg_class);
  SegExt *x = SGX(w);
  x->box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 0);
  x->selected = -1;
  awra_child_insert(w, x->box, -1);
  awra_widget_set_halign(w, AWRA_ALIGN_START);
  return w;
}

static void seg_toggled(AwraWidget *b, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  if (!a->active) {
    return;
  }
  SegExt *x = SGX(w);
  const int i = awra_child_index(x->box, b);
  if (i != x->selected) {
    x->selected = i;
    awra_emit_index(w, AWRA_SIGNAL_SELECTED, i);
  }
}

int awra_segmented_control_append(AwraWidget *w, const char *label, const char *icon)
{
  SegExt *x = SGX(w);
  AwraWidget *b = awra_toggle_button_new();
  if (label) {
    awra_button_set_label(b, label);
  }
  if (icon) {
    awra_button_set_icon_name(b, icon);
  }
  awra_button_set_look(b, AWRA_LOOK_SEGMENT);
  if (x->box->nkids > 0) {
    awra_toggle_button_set_group(b, x->box->kids[0]);
  }
  awra_box_append(x->box, b);
  awra_signal_connect(b, AWRA_SIGNAL_TOGGLED, seg_toggled, w, NULL);
  const int index = x->box->nkids - 1;
  if (index == 0) {
    awra_toggle_button_set_active(b, true);
  }
  return index;
}

void awra_segmented_control_set_selected(AwraWidget *w, int index)
{
  SegExt *x = SGX(w);
  if (index >= 0 && index < x->box->nkids) {
    awra_toggle_button_set_active(x->box->kids[index], true);
  }
}

int awra_segmented_control_selected(AwraWidget *w)
{
  return SGX(w)->selected;
}

void awra_segmented_control_remove_all(AwraWidget *w)
{
  awra_container_remove_all(SGX(w)->box);
  SGX(w)->selected = -1;
}

/* ======================================================================= */
/* Progress bar (spec B §14): a 6 px pill */

typedef struct ProgExt {
  double fraction;
  bool pulsing;
  double pulse;
  char *text;
} ProgExt;

#define PX(w) AWRA_EXT(w, ProgExt)

static void prog_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  *mn = *nt = o == AWRA_ORIENTATION_HORIZONTAL ? awra_px(1) : awra_px(6);
}

static void prog_draw(AwraWidget *w, AwraDraw *d)
{
  ProgExt *x = PX(w);
  const AwraTokens *t = awra_tokens();
  const float h = awra_pxf(6);
  AwraBox b = {(float)w->x, (float)w->y + floorf((w->h - h) * 0.5f), (float)w->w, h};
  /* the node and its trough both paint the track */
  AwraRGBA track = t->control_track;
  track.a = 1.0f - (1.0f - track.a) * (1.0f - track.a);
  awra_paint_rbox(d, b, h * 0.5f, &track, 0, NULL);
  float pw, px = b.x;
  if (x->pulsing) {
    pw = b.w * 0.25f;
    px = b.x + (float)x->pulse * (b.w - pw);
  }
  else {
    pw = awra_pxf(1) + (b.w - awra_pxf(1)) * (float)x->fraction;
  }
  awra_paint_rbox(d, (AwraBox){px, b.y, pw, h}, h * 0.5f, &t->accent, 0, NULL);
}

static void prog_finalize(AwraWidget *w)
{
  free(PX(w)->text);
}

/* GtkProgressBar: a 0..1 range (busy while it pulses), its text */
static void prog_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  ProgExt *x = PX(w);
  info->has_value = !x->pulsing;
  info->value = x->fraction;
  info->min = 0.0;
  info->max = 1.0;
  info->value_text = x->text;
  if (x->pulsing) {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_BUSY, 1);
  }
}

static AwraClass prog_class = {
    .a11y = prog_a11y,
    .bk = AWRA_BK("AwraProgressBar", 0),
    .type = "AwraProgressBar",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(ProgExt),
    .finalize = prog_finalize,
    .measure = prog_measure,
    .draw = prog_draw,
};

AwraWidget *awra_progress_bar_new(void)
{
  return awra_widget_new(&prog_class);
}

void awra_progress_bar_set_fraction(AwraWidget *w, double f)
{
  ProgExt *x = PX(w);
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  if (x->fraction == f && !x->pulsing) {
    return;
  }
  x->fraction = f;
  x->pulsing = false;
  awra_invalidate(w);
}

double awra_progress_bar_fraction(AwraWidget *w)
{
  return PX(w)->fraction;
}

void awra_progress_bar_pulse(AwraWidget *w)
{
  ProgExt *x = PX(w);
  x->pulsing = true;
  x->pulse += 0.1;
  if (x->pulse > 1.0) {
    x->pulse = 0.0;
  }
  awra_invalidate(w);
}

void awra_progress_bar_set_text(AwraWidget *w, const char *text)
{
  awra_set_str(&PX(w)->text, text);
  awra_invalidate(w);
}

/* ======================================================================= */
/* Spinner: GTK's process-working ring, one turn per second */

typedef struct SpinnerExt {
  bool spinning;
  unsigned timer;
  double start;
  int size; /* logical */
} SpinnerExt;

#define SPX(w) AWRA_EXT(w, SpinnerExt)

static void spinner_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  *mn = *nt = awra_px(SPX(w)->size);
}

static void spinner_draw(AwraWidget *w, AwraDraw *d)
{
  SpinnerExt *x = SPX(w);
  if (!x->spinning) {
    return;
  }
  AwraRGBA c;
  int weight;
  if (awra_widget_get_data(w, "awra-accent")) {
    c = awra_tokens()->accent_content;
  }
  else if (!awra_inherited_style(w, &c, &weight)) {
    c = awra_tokens()->foreground;
  }
  const float s = (float)BK_MIN(w->w, w->h);
  const float cx = w->x + w->w * 0.5f, cy = w->y + w->h * 0.5f;
  const float r = s * 0.5f - s * 0.09f, lw = s * 0.125f;
  const double t = awra_style_reduced_motion() ? 0 : fmod((awra_now_ms() - x->start) / 1000.0, 1.0);
  const int segs = 24;
  for (int i = 0; i < segs; i++) {
    const double a0 = t * 2 * M_PI + i * (2 * M_PI * 0.75 / segs);
    const double a1 = a0 + 2 * M_PI * 0.75 / segs;
    AwraRGBA cc = c;
    cc.a *= 0.25f + 0.75f * (float)i / segs;
    awra_paint_line(d, cx + r * (float)cos(a0), cy + r * (float)sin(a0), cx + r * (float)cos(a1),
                    cy + r * (float)sin(a1), lw, &cc);
  }
}

static bool spinner_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  awra_invalidate(w);
  return SPX(w)->spinning;
}

static void spinner_map(AwraWidget *w, bool mapped)
{
  SpinnerExt *x = SPX(w);
  if (mapped && x->spinning && !x->timer && !awra_style_reduced_motion()) {
    x->timer = awra_timeout_add(33, spinner_tick, w, NULL);
  }
  else if (!mapped && x->timer) {
    awra_source_remove(x->timer);
    x->timer = 0;
  }
}

static void spinner_finalize(AwraWidget *w)
{
  if (SPX(w)->timer) {
    awra_source_remove(SPX(w)->timer);
  }
}

/* GtkSpinner: busy while it spins */
static void spinner_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_BUSY, SPX(w)->spinning);
}

static AwraClass spinner_class = {
    .a11y = spinner_a11y,
    .bk = AWRA_BK("AwraSpinner", 0),
    .type = "AwraSpinner",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(SpinnerExt),
    .finalize = spinner_finalize,
    .measure = spinner_measure,
    .draw = spinner_draw,
    .map = spinner_map,
};

AwraWidget *awra_spinner_new(void)
{
  AwraWidget *w = awra_widget_new(&spinner_class);
  SPX(w)->size = 16;
  SPX(w)->start = awra_now_ms();
  return w;
}

void awra_spinner_set_spinning(AwraWidget *w, bool spinning)
{
  SpinnerExt *x = SPX(w);
  x->spinning = spinning;
  spinner_map(w, awra_widget_mapped(w) && spinning);
  if (!spinning && x->timer) {
    awra_source_remove(x->timer);
    x->timer = 0;
  }
  awra_invalidate(w);
}

void awra_spinner_set_size_internal(AwraWidget *w, int size);
void awra_spinner_set_size_internal(AwraWidget *w, int size)
{
  SPX(w)->size = size;
  awra_widget_queue_resize(w);
}

void awra_slider_add_mark(AwraWidget *w, double value)
{
  SliderExt *x = SLX(w);
  if (x->nmarks < 8) {
    x->marks[x->nmarks++] = value;
    awra_widget_queue_resize(w);
  }
}

void awra_slider_clear_marks(AwraWidget *w)
{
  SLX(w)->nmarks = 0;
  awra_widget_queue_resize(w);
}
