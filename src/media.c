/* Awra 2 media components: key caps, the keyboard view, waveforms, media
 * rows, drop zones and drawing areas (spec B §16-20).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

int awra_key_code_evdev(const char *code);
const char *awra_key_code_label(const char *code);
extern AwraClass awra_slider_class;
void awra_spinner_set_size_internal(AwraWidget *w, int size);

/* ======================================================================= */
/* Key cap (spec B §16) */

typedef struct CapExt {
  char *code, *label, *secondary, *badge;
  bool bound, active, selected, dimmed, drop, compact;
  bool has_tint, has_edge;
  AwraRGBA tint, edge;
  float wu, hu; /* units */
  float depth;  /* 0 rest .. 1 pressed */
  double pulse_start;
  unsigned pulse_timer;
} CapExt;

#define KX(w) AWRA_EXT(w, CapExt)

static void cap_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  CapExt *x = KX(w);
  const float u = o == AWRA_ORIENTATION_HORIZONTAL ? x->wu : x->hu;
  if (x->compact) {
    AwraFont fnt = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    fnt.size *= 0.86f;
    if (o == AWRA_ORIENTATION_HORIZONTAL) {
      const float tw = awra_text_width(&fnt, x->label ? x->label : "", -1);
      *mn = *nt = BK_MAX(awra_px(28), (int)ceilf(tw) + awra_px(16));
    }
    else {
      *mn = *nt = awra_px(28);
    }
    return;
  }
  *mn = awra_px(20 * u);
  *nt = awra_px(48 * u);
}

static float cap_pulse(const CapExt *x)
{
  if (awra_style_reduced_motion()) {
    return 0.5f;
  }
  const double t = (awra_now_ms() - x->pulse_start) / 1600.0;
  return 0.5f + 0.5f * (float)sin(2 * M_PI * t);
}

static AwraFont legend_font(const CapExt *x, float scale)
{
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.size *= 0.86f * scale;
  f.weight = x->bound ? 700 : 600;
  return f;
}

static void cap_draw(AwraWidget *w, AwraDraw *d)
{
  CapExt *x = KX(w);
  const AwraTokens *t = awra_tokens();
  const float saved = d->opacity;
  if (x->dimmed) {
    d->opacity *= 0.38f;
  }
  AwraBox b = awra_box_of(w);
  const float r = fminf(awra_pxf(7), fminf(b.w, b.h) * 0.22f);
  const bool hover = awra_hovered(w);
  const float pulse = cap_pulse(x);
  if (x->active) {
    AwraRGBA glow = awra_with_alpha(t->accent, 0.22f + 0.30f * pulse);
    awra_paint_shadow(d, b, r, 0, 0, awra_pxf(2), awra_pxf(8), &glow, false);
  }
  if (x->bound && !x->dimmed) {
    AwraRGBA contact = {0, 0, 0, (t->dark ? 0.30f : 0.12f) * (1.0f - x->depth)};
    awra_paint_shadow(d, b, r, 0, awra_pxf(1), 0, awra_pxf(2), &contact, true);
  }
  b.y += x->depth * awra_pxf(1.5f);
  /* fill */
  if (x->bound) {
    awra_paint_rbox(d, b, r, hover ? &t->control_cap_hover : &t->control_cap, 0, NULL);
  }
  else {
    awra_paint_rbox(d, b, r, &t->control_well, 0, NULL);
    if (hover) {
      AwraRGBA h = t->control_cap_hover;
      h.a *= 0.6f;
      awra_paint_rbox(d, b, r, &h, 0, NULL);
    }
  }
  if (x->has_tint) {
    AwraRGBA tint = x->tint;
    tint.a *= hover ? 0.92f : 1.0f;
    awra_paint_rbox(d, b, r, &tint, 0, NULL);
  }
  if (x->selected || x->drop) {
    AwraRGBA s = t->accent_soft;
    if (x->drop) {
      s.a = fminf(1.0f, s.a * 2.0f);
    }
    awra_paint_rbox(d, b, r, &s, 0, NULL);
  }
  if (x->depth > 0) {
    AwraRGBA p = t->control_pressed;
    p.a *= x->depth;
    awra_paint_rbox(d, b, r, &p, 0, NULL);
  }
  const float r4[4] = {r, r, r, r};
  if (x->has_edge) {
    const float w4[4] = {0, 0, awra_pxf(3), 0};
    awra_paint_rbox4(d, b, r4, NULL, w4, &x->edge);
  }
  if (x->bound && x->depth < 1) {
    AwraRGBA rim = t->rim;
    rim.a *= 1.0f - x->depth;
    const float w4[4] = {awra_pxf(1), 0, 0, 0};
    awra_paint_rbox4(d, b, r4, NULL, w4, &rim);
  }
  else if (!x->bound) {
    const AwraRGBA lip = {0, 0, 0, t->dark ? 0.22f : 0.06f};
    const float w4[4] = {awra_pxf(2), awra_pxf(1), 0, awra_pxf(1)};
    awra_paint_rbox4(d, b, r4, NULL, w4, &lip);
  }
  if (x->selected || x->drop) {
    awra_paint_rbox(d, b, r, NULL, awra_pxf(2), &t->accent);
  }
  else if (x->active) {
    AwraRGBA a = awra_with_alpha(t->accent, 0.55f + 0.35f * pulse);
    awra_paint_rbox(d, b, r, NULL, awra_pxf(1.5f), &a);
  }
  else if (t->high_contrast || !x->bound) {
    awra_paint_rbox(d, b, r, NULL, awra_pxf(t->high_contrast ? 1.5f : 1.0f), &t->outline);
  }
  /* legends */
  const float inset = fmaxf(awra_pxf(3), fminf(awra_pxf(8), b.h / 8.0f));
  AwraRGBA fg = x->selected ? t->accent_content : x->bound ? t->foreground : t->muted;
  if (x->has_tint) {
    AwraRGBA bgc = awra_mix(t->background, x->tint, x->tint.a);
    const float lum = 0.2126f * bgc.r + 0.7152f * bgc.g + 0.0722f * bgc.b;
    awra_parse_color(lum > 0.55f ? "#15121a" : "#ffffff", &fg);
  }
  bk_clip_push((int)b.x, (int)b.y, (int)ceilf(b.w), (int)ceilf(b.h));
  float badge_w = 0;
  if (x->badge && x->badge[0]) {
    AwraFont bf = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    bf.size *= 0.62f;
    bf.weight = 800;
    const float tw = awra_text_width(&bf, x->badge, -1);
    const float bh = awra_font_line_height(&bf);
    badge_w = fminf(fmaxf(tw + awra_pxf(8), bh), b.w - awra_pxf(4));
    AwraBox bb = {b.x + b.w - badge_w - awra_pxf(3), b.y + awra_pxf(3), badge_w, bh};
    awra_paint_rbox(d, bb, bh * 0.5f, &t->accent, 0, NULL);
    awra_draw_text(d, &bf, bb.x + (badge_w - tw) * 0.5f, bb.y + awra_font_ascent(&bf), x->badge, -1,
                   &t->on_accent);
  }
  if (x->label && x->label[0]) {
    AwraFont lf = legend_font(x, 1.0f);
    const float avail = b.w - 2 * inset - badge_w;
    float tw = awra_text_width(&lf, x->label, -1);
    if (tw > avail && tw > 0) { /* scaled down to fit, at most to 65 % */
      const float s = fmaxf(0.65f, avail / tw);
      lf = legend_font(x, s);
      tw = awra_text_width(&lf, x->label, -1);
    }
    AwraFont sf = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    sf.size *= 0.70f;
    sf.weight = 500;
    const float lh = awra_font_line_height(&lf), sh = awra_font_line_height(&sf);
    if (x->secondary && x->secondary[0] && lh + sh + 2 * inset <= b.h) {
      awra_draw_text(d, &lf, b.x + inset, b.y + inset + awra_font_ascent(&lf), x->label, -1, &fg);
      AwraRGBA sc = x->has_tint ? awra_with_alpha(fg, 0.84f) : t->muted;
      awra_draw_text(d, &sf, b.x + inset, b.y + b.h - inset - sh + awra_font_ascent(&sf), x->secondary,
                     -1, &sc);
    }
    else {
      const float lx = b.x + floorf((b.w - fminf(tw, b.w - 2 * inset)) * 0.5f + 0.5f);
      const float ly = b.y + floorf((b.h - lh) * 0.5f + awra_font_ascent(&lf) + 0.5f);
      awra_draw_text(d, &lf, fmaxf(lx, b.x + inset), ly, x->label, -1, &fg);
    }
  }
  bk_clip_pop();
  if (awra_focus_visible(w)) {
    AwraBox o = awra_box_inset(awra_box_of(w), -awra_pxf(3));
    awra_paint_rbox(d, o, r + awra_pxf(3), NULL, awra_pxf(2), &t->focus_ring);
  }
  d->opacity = saved;
}

static void depth_anim(AwraWidget *w, double t, void *data)
{
  CapExt *x = KX(w);
  float from;
  memcpy(&from, &data, sizeof(float));
  x->depth = from * (float)(1.0 - t);
  awra_invalidate(w);
}

static int cap_press(AwraWidget *w, BKEvent *e)
{
  awra_anim_stop_for(w);
  KX(w)->depth = 1.0f;
  awra_invalidate(w);
  return 0;
}

static int cap_release(AwraWidget *w, BKEvent *e)
{
  CapExt *x = KX(w);
  void *data = NULL;
  memcpy(&data, &x->depth, sizeof(float));
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_FAST), depth_anim, data, NULL);
  if (e->inside) {
    AwraArgs a = {0};
    a.signal = AWRA_SIGNAL_CLICKED;
    a.text = x->code;
    a.button = 1;
    a.mods = e->mods & (BK_SHIFT | BK_CTRL | BK_ALT);
    awra_emit(w, &a);
  }
  return 0;
}

static bool pulse_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  awra_invalidate(w);
  return KX(w)->active;
}

static void cap_finalize(AwraWidget *w)
{
  CapExt *x = KX(w);
  if (x->pulse_timer) {
    awra_source_remove(x->pulse_timer);
  }
  free(x->code);
  free(x->label);
  free(x->secondary);
  free(x->badge);
}

static void cap_map(AwraWidget *w, bool mapped)
{
  CapExt *x = KX(w);
  if (!mapped && x->pulse_timer) {
    awra_source_remove(x->pulse_timer);
    x->pulse_timer = 0;
  }
  else if (mapped && x->active && !x->pulse_timer && !awra_style_reduced_motion()) {
    x->pulse_timer = awra_timeout_add(50, pulse_tick, w, NULL);
  }
}

/* Awra 1's AwraKeyCap: a button named "label, secondary" (or its code),
 * selected, dimmed keys disabled, pressed while it plays */
static void cap_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  CapExt *x = KX(w);
  const char *label = x->label && x->label[0] ? x->label : x->code;
  if (x->secondary && x->secondary[0] && label) {
    snprintf(info->buf, sizeof(info->buf), "%s, %s", label, x->secondary);
    info->label = info->buf;
  }
  else {
    info->label = label;
  }
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_SELECTED, x->selected);
  if (x->dimmed) {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_DISABLED, 1);
  }
  info->action = "click";
}

static bool cap_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  if (item < 0 && verb == AWRA_A11Y_DO_ACTIVATE && awra_sensitive(w)) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
    return true;
  }
  return false;
}

static AwraClass cap_class = {
    .a11y = cap_a11y,
    .a11y_do = cap_a11y_do,
    .bk = AWRA_BK("AwraKeyCap", CLS_INTERACTIVE),
    .type = "AwraKeyCap",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(CapExt),
    .finalize = cap_finalize,
    .measure = cap_measure,
    .draw = cap_draw,
    .press = cap_press,
    .release = cap_release,
    .map = cap_map,
    .overdraw = 12,
};

AwraWidget *awra_key_cap_new(const char *code, const char *label)
{
  AwraWidget *w = awra_widget_new(&cap_class);
  CapExt *x = KX(w);
  x->code = awra_strdup(code);
  x->label = awra_strdup(label ? label : (code ? awra_key_code_label(code) : ""));
  x->wu = x->hu = 1.0f;
  x->pulse_start = awra_now_ms();
  return w;
}

const char *awra_key_cap_code(AwraWidget *w)
{
  return KX(w)->code;
}

#define CAP_STR(fn, field)                                    \
  void fn(AwraWidget *w, const char *v)                       \
  {                                                           \
    if (KX(w)->field && v && !strcmp(KX(w)->field, v)) {      \
      return;                                                 \
    }                                                         \
    awra_set_str(&KX(w)->field, v);                           \
    awra_invalidate(w);                                       \
  }
CAP_STR(awra_key_cap_set_label, label)
CAP_STR(awra_key_cap_set_secondary_label, secondary)
CAP_STR(awra_key_cap_set_badge, badge)

#define CAP_BOOL(fn, field)          \
  void fn(AwraWidget *w, bool v)     \
  {                                  \
    if (KX(w)->field != v) {         \
      KX(w)->field = v;              \
      awra_invalidate(w);            \
    }                                \
  }
CAP_BOOL(awra_key_cap_set_bound, bound)
CAP_BOOL(awra_key_cap_set_selected, selected)
CAP_BOOL(awra_key_cap_set_dimmed, dimmed)
CAP_BOOL(awra_key_cap_set_drop_highlight, drop)

void awra_key_cap_set_active(AwraWidget *w, bool a)
{
  CapExt *x = KX(w);
  if (x->active == a) {
    return;
  }
  x->active = a;
  x->pulse_start = awra_now_ms();
  cap_map(w, awra_widget_mapped(w));
  awra_invalidate(w);
}

void awra_key_cap_set_tint(AwraWidget *w, const AwraRGBA *c)
{
  KX(w)->has_tint = c != NULL;
  if (c) {
    KX(w)->tint = *c;
  }
  awra_invalidate(w);
}

void awra_key_cap_set_edge_color(AwraWidget *w, const AwraRGBA *c)
{
  KX(w)->has_edge = c != NULL;
  if (c) {
    KX(w)->edge = *c;
  }
  awra_invalidate(w);
}

void awra_key_cap_set_width_units(AwraWidget *w, double u)
{
  KX(w)->wu = (float)(u < 0.25 ? 0.25 : u > 12 ? 12 : u);
  awra_widget_queue_resize(w);
}

void awra_key_cap_set_height_units(AwraWidget *w, double u)
{
  KX(w)->hu = (float)(u < 0.25 ? 0.25 : u > 12 ? 12 : u);
  awra_widget_queue_resize(w);
}

void awra_key_cap_set_compact(AwraWidget *w, bool c)
{
  KX(w)->compact = c;
  awra_widget_queue_resize(w);
}

static void flash_anim(AwraWidget *w, double t, void *data)
{
  CapExt *x = KX(w);
  /* 30 %: down (ease-out), 70 %: back up */
  if (t < 0.3) {
    x->depth = (float)awra_ease_out_cubic(t / 0.3);
  }
  else {
    x->depth = (float)(1.0 - awra_ease_out_cubic((t - 0.3) / 0.7));
  }
  awra_invalidate(w);
}

void awra_key_cap_flash(AwraWidget *w)
{
  awra_anim_stop_for(w);
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_NORMAL), flash_anim, NULL, NULL);
}

/* ======================================================================= */
/* Keyboard view (spec B §17) */

typedef struct KeyPos {
  const char *code;
  float x, y, w, h;
} KeyPos;

typedef struct KbdExt {
  KeyPos *keys;
  int nkeys;
  float total_w, total_h;
  bool fn_row, nav, numpad, iso;
  AwraSelectionMode mode;
  bool movable, drops;
  const char **codes_cache;
  const char **sel_cache;
  AwraWidget *drag_from;
  AwraWidget *drop_cap;
  int anchor; /* range selection anchor (key index) */
} KbdExt;

#define KBX(w) AWRA_EXT(w, KbdExt)

static void kbd_add(KbdExt *x, const char *code, float kx, float ky, float kw, float kh)
{
  x->keys = (KeyPos *)realloc(x->keys, sizeof(KeyPos) * (size_t)(x->nkeys + 1));
  x->keys[x->nkeys++] = (KeyPos){code, kx, ky, kw, kh};
}

static void kbd_build_positions(KbdExt *x)
{
  x->nkeys = 0;
  float my = 0;
  if (x->fn_row) {
    kbd_add(x, "Escape", 0, 0, 1, 1);
    const char *f[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
    const float fx[] = {2, 3, 4, 5, 6.5f, 7.5f, 8.5f, 9.5f, 11, 12, 13, 14};
    for (int i = 0; i < 12; i++) {
      kbd_add(x, f[i], fx[i], 0, 1, 1);
    }
    my = 1.25f;
  }
  const char *r1[] = {"Backquote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
                      "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal"};
  for (int i = 0; i < 13; i++) {
    kbd_add(x, r1[i], (float)i, my, 1, 1);
  }
  kbd_add(x, "Backspace", 13, my, 2, 1);
  kbd_add(x, "Tab", 0, my + 1, 1.5f, 1);
  const char *r2[] = {"KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO",
                      "KeyP", "BracketLeft", "BracketRight"};
  for (int i = 0; i < 12; i++) {
    kbd_add(x, r2[i], 1.5f + i, my + 1, 1, 1);
  }
  if (!x->iso) {
    kbd_add(x, "Backslash", 13.5f, my + 1, 1.5f, 1);
  }
  kbd_add(x, "CapsLock", 0, my + 2, 1.75f, 1);
  const char *r3[] = {"KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
                      "Semicolon", "Quote"};
  for (int i = 0; i < 11; i++) {
    kbd_add(x, r3[i], 1.75f + i, my + 2, 1, 1);
  }
  if (x->iso) {
    kbd_add(x, "Enter", 13.75f, my + 1, 1.25f, 2);
    kbd_add(x, "Backslash", 12.75f, my + 2, 1, 1);
    kbd_add(x, "ShiftLeft", 0, my + 3, 1.25f, 1);
    kbd_add(x, "IntlBackslash", 1.25f, my + 3, 1, 1);
  }
  else {
    kbd_add(x, "Enter", 12.75f, my + 2, 2.25f, 1);
    kbd_add(x, "ShiftLeft", 0, my + 3, 2.25f, 1);
  }
  const char *r4[] = {"KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM", "Comma", "Period", "Slash"};
  for (int i = 0; i < 10; i++) {
    kbd_add(x, r4[i], 2.25f + i, my + 3, 1, 1);
  }
  kbd_add(x, "ShiftRight", 12.25f, my + 3, 2.75f, 1);
  kbd_add(x, "ControlLeft", 0, my + 4, 1.25f, 1);
  kbd_add(x, "MetaLeft", 1.25f, my + 4, 1.25f, 1);
  kbd_add(x, "AltLeft", 2.5f, my + 4, 1.25f, 1);
  kbd_add(x, "Space", 3.75f, my + 4, 6.25f, 1);
  kbd_add(x, "AltRight", 10, my + 4, 1.25f, 1);
  kbd_add(x, "MetaRight", 11.25f, my + 4, 1.25f, 1);
  kbd_add(x, "ContextMenu", 12.5f, my + 4, 1.25f, 1);
  kbd_add(x, "ControlRight", 13.75f, my + 4, 1.25f, 1);
  float total = 15;
  float nx = 15.25f;
  if (x->nav) {
    if (x->fn_row) {
      kbd_add(x, "PrintScreen", nx, 0, 1, 1);
      kbd_add(x, "ScrollLock", nx + 1, 0, 1, 1);
      kbd_add(x, "Pause", nx + 2, 0, 1, 1);
    }
    kbd_add(x, "Insert", nx, my, 1, 1);
    kbd_add(x, "Home", nx + 1, my, 1, 1);
    kbd_add(x, "PageUp", nx + 2, my, 1, 1);
    kbd_add(x, "Delete", nx, my + 1, 1, 1);
    kbd_add(x, "End", nx + 1, my + 1, 1, 1);
    kbd_add(x, "PageDown", nx + 2, my + 1, 1, 1);
    kbd_add(x, "ArrowUp", nx + 1, my + 3, 1, 1);
    kbd_add(x, "ArrowLeft", nx, my + 4, 1, 1);
    kbd_add(x, "ArrowDown", nx + 1, my + 4, 1, 1);
    kbd_add(x, "ArrowRight", nx + 2, my + 4, 1, 1);
    total = 18.25f;
  }
  if (x->numpad) {
    const float px = x->nav ? nx + 3.25f : 15.25f;
    kbd_add(x, "NumLock", px, my, 1, 1);
    kbd_add(x, "NumpadDivide", px + 1, my, 1, 1);
    kbd_add(x, "NumpadMultiply", px + 2, my, 1, 1);
    kbd_add(x, "NumpadSubtract", px + 3, my, 1, 1);
    const char *np[] = {"Numpad7", "Numpad8", "Numpad9", "Numpad4", "Numpad5", "Numpad6",
                        "Numpad1", "Numpad2", "Numpad3"};
    for (int i = 0; i < 9; i++) {
      kbd_add(x, np[i], px + (float)(i % 3), my + 1 + (float)(i / 3), 1, 1);
    }
    kbd_add(x, "NumpadAdd", px + 3, my + 1, 1, 2);
    kbd_add(x, "NumpadEnter", px + 3, my + 3, 1, 2);
    kbd_add(x, "Numpad0", px, my + 4, 2, 1);
    kbd_add(x, "NumpadDecimal", px + 2, my + 4, 1, 1);
    total = px + 4;
  }
  x->total_w = total;
  x->total_h = my + 5;
}

static AwraWidget *kbd_cap_by_code(AwraWidget *v, const char *code)
{
  for (int i = 0; code && i < v->nkids; i++) {
    const char *c = KX(v->kids[i])->code;
    if (c && !strcmp(c, code)) {
      return v->kids[i];
    }
  }
  return NULL;
}

static void kbd_cap_clicked(AwraWidget *cap, const AwraArgs *a, void *data);

static void kbd_rebuild(AwraWidget *v)
{
  KbdExt *x = KBX(v);
  kbd_build_positions(x);
  /* keep caps that stay (their state), add the missing, remove the gone */
  for (int i = v->nkids - 1; i >= 0; i--) {
    AwraWidget *c = v->kids[i];
    bool keep = false;
    for (int k = 0; k < x->nkeys && !keep; k++) {
      keep = !strcmp(x->keys[k].code, KX(c)->code);
    }
    if (!keep) {
      awra_child_remove(v, c);
    }
  }
  for (int k = 0; k < x->nkeys; k++) {
    AwraWidget *c = kbd_cap_by_code(v, x->keys[k].code);
    if (!c) {
      c = awra_key_cap_new(x->keys[k].code, NULL);
      awra_signal_connect(c, AWRA_SIGNAL_CLICKED, kbd_cap_clicked, v, NULL);
      awra_child_insert(v, c, -1);
      awra_widget_unref(c);
    }
    awra_key_cap_set_width_units(c, x->keys[k].w);
    awra_key_cap_set_height_units(c, x->keys[k].h);
  }
  free(x->codes_cache);
  x->codes_cache = NULL;
  awra_widget_queue_resize(v);
}

static void kbd_measure(AwraWidget *v, int o, int f, int *mn, int *nt)
{
  KbdExt *x = KBX(v);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *mn = (int)ceilf(x->total_w * awra_pxf(22));
    *nt = (int)ceilf(x->total_w * awra_pxf(46));
    return;
  }
  float unit = f >= 0 ? f / x->total_w : awra_pxf(46);
  unit = fminf(fmaxf(unit, awra_pxf(22)), awra_pxf(64));
  *mn = *nt = (int)ceilf(x->total_h * unit);
}

static void kbd_allocate(AwraWidget *v)
{
  KbdExt *x = KBX(v);
  float unit = fminf((float)v->w / x->total_w, (float)v->h / x->total_h);
  unit = fminf(fmaxf(unit, 1.0f), awra_pxf(64));
  const float gap = fminf(fmaxf(floorf(unit * 0.09f + 0.5f), awra_pxf(2)), awra_pxf(6));
  const float ox = v->x + floorf((v->w - unit * x->total_w) * 0.5f);
  const float oy = v->y + floorf((v->h - unit * x->total_h) * 0.5f);
  for (int k = 0; k < x->nkeys; k++) {
    const KeyPos *p = &x->keys[k];
    AwraWidget *c = kbd_cap_by_code(v, p->code);
    if (!c) {
      continue;
    }
    const int x0 = (int)floorf(ox + p->x * unit + gap * 0.5f + 0.5f);
    const int y0 = (int)floorf(oy + p->y * unit + gap * 0.5f + 0.5f);
    const int x1 = (int)floorf(ox + (p->x + p->w) * unit - gap * 0.5f + 0.5f);
    const int y1 = (int)floorf(oy + (p->y + p->h) * unit - gap * 0.5f + 0.5f);
    awra_allocate(c, x0, y0, BK_MAX(1, x1 - x0), BK_MAX(1, y1 - y0));
  }
}

static void kbd_finalize(AwraWidget *v)
{
  KbdExt *x = KBX(v);
  free(x->keys);
  free(x->codes_cache);
  free(x->sel_cache);
}

static void kbd_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->label = "Keyboard";
  if (KBX(w)->mode == AWRA_SELECTION_MULTIPLE) {
    info->flags |= AWRA_A11Y_MULTISELECTABLE;
  }
  info->selection = KBX(w)->mode != AWRA_SELECTION_NONE;
}

static AwraClass kbd_class = {
    .a11y = kbd_a11y,
    .bk = AWRA_BK("AwraKeyboardView", 0),
    .type = "AwraKeyboardView",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(KbdExt),
    .finalize = kbd_finalize,
    .measure = kbd_measure,
    .allocate = kbd_allocate,
};

static void kbd_selection_changed(AwraWidget *v)
{
  KbdExt *x = KBX(v);
  free(x->sel_cache);
  x->sel_cache = NULL;
  int n = 0;
  for (int i = 0; i < v->nkids; i++) {
    n += KX(v->kids[i])->selected;
  }
  awra_emit_index(v, AWRA_SIGNAL_SELECTED, n);
}

static void kbd_cap_clicked(AwraWidget *cap, const AwraArgs *a, void *data)
{
  AwraWidget *v = (AwraWidget *)data;
  KbdExt *x = KBX(v);
  if (x->mode == AWRA_SELECTION_NONE) {
    return;
  }
  const bool add = x->mode == AWRA_SELECTION_MULTIPLE && (a->mods & (BK_CTRL | BK_SHIFT));
  if (!add) {
    for (int i = 0; i < v->nkids; i++) {
      if (v->kids[i] != cap) {
        awra_key_cap_set_selected(v->kids[i], false);
      }
    }
    awra_key_cap_set_selected(cap, true);
  }
  else {
    awra_key_cap_set_selected(cap, !KX(cap)->selected);
  }
  kbd_selection_changed(v);
}

AwraWidget *awra_keyboard_view_new(AwraKeyboardLayout layout)
{
  AwraWidget *v = awra_widget_new(&kbd_class);
  KbdExt *x = KBX(v);
  x->fn_row = true;
  x->nav = true;
  x->iso = layout == AWRA_KEYBOARD_LAYOUT_ISO;
  x->mode = AWRA_SELECTION_SINGLE;
  kbd_rebuild(v);
  return v;
}

void awra_keyboard_view_set_layout(AwraWidget *v, AwraKeyboardLayout layout)
{
  KBX(v)->iso = layout == AWRA_KEYBOARD_LAYOUT_ISO;
  kbd_rebuild(v);
}

void awra_keyboard_view_set_show_function_row(AwraWidget *v, bool s)
{
  KBX(v)->fn_row = s;
  kbd_rebuild(v);
}

void awra_keyboard_view_set_show_navigation(AwraWidget *v, bool s)
{
  KBX(v)->nav = s;
  kbd_rebuild(v);
}

void awra_keyboard_view_set_show_numpad(AwraWidget *v, bool s)
{
  KBX(v)->numpad = s;
  kbd_rebuild(v);
}

bool awra_keyboard_view_shows_navigation(AwraWidget *v)
{
  return KBX(v)->nav;
}

void awra_keyboard_view_set_selection_mode(AwraWidget *v, AwraSelectionMode m)
{
  KBX(v)->mode = m;
}

void awra_keyboard_view_set_keys_movable(AwraWidget *v, bool m)
{
  KBX(v)->movable = m;
}

void awra_keyboard_view_set_accepts_file_drops(AwraWidget *v, bool a)
{
  KBX(v)->drops = a;
  awra_widget_set_accepts_file_drops(v, a);
}

AwraWidget *awra_keyboard_view_key_cap(AwraWidget *v, const char *code)
{
  return kbd_cap_by_code(v, code);
}

/* the key codes, in layout order (cached) */
const char *const *awra_keyboard_view_codes(AwraWidget *v)
{
  KbdExt *x = KBX(v);
  if (!x->codes_cache) {
    x->codes_cache = (const char **)calloc((size_t)x->nkeys + 1, sizeof(char *));
    for (int k = 0; k < x->nkeys; k++) {
      x->codes_cache[k] = x->keys[k].code;
    }
  }
  return x->codes_cache;
}

const char *const *awra_keyboard_view_selected_codes(AwraWidget *v)
{
  KbdExt *x = KBX(v);
  free(x->sel_cache);
  x->sel_cache = (const char **)calloc((size_t)v->nkids + 1, sizeof(char *));
  int n = 0;
  for (int i = 0; i < v->nkids; i++) {
    if (KX(v->kids[i])->selected) {
      x->sel_cache[n++] = KX(v->kids[i])->code;
    }
  }
  return x->sel_cache;
}

void awra_keyboard_view_set_selected_codes(AwraWidget *v, const char *const *codes, int n)
{
  for (int i = 0; i < v->nkids; i++) {
    bool sel = false;
    for (int k = 0; k < n && !sel; k++) {
      sel = codes[k] && !strcmp(codes[k], KX(v->kids[i])->code);
    }
    awra_key_cap_set_selected(v->kids[i], sel);
  }
  free(KBX(v)->sel_cache);
  KBX(v)->sel_cache = NULL;
}

void awra_keyboard_view_unselect_all(AwraWidget *v)
{
  awra_keyboard_view_set_selected_codes(v, NULL, 0);
  kbd_selection_changed(v);
}

const char *awra_keyboard_view_code_at(AwraWidget *v, double lx, double ly)
{
  const int px = v->x + awra_px(lx), py = v->y + awra_px(ly);
  for (int i = 0; i < v->nkids; i++) {
    if (bk_contains(v->kids[i], px, py)) {
      return KX(v->kids[i])->code;
    }
  }
  return NULL;
}

/* compatibility with the per-key API (set_key_label etc.) */
void awra_keyboard_view_set_key_label(AwraWidget *v, const char *key, const char *label)
{
  AwraWidget *c = kbd_cap_by_code(v, key);
  if (c) {
    awra_key_cap_set_label(c, label);
  }
}

void awra_keyboard_view_set_key_caption(AwraWidget *v, const char *key, const char *caption)
{
  AwraWidget *c = kbd_cap_by_code(v, key);
  if (c) {
    awra_key_cap_set_secondary_label(c, caption);
  }
}

void awra_keyboard_view_set_key_state(AwraWidget *v, const char *key, int state)
{
  AwraWidget *c = kbd_cap_by_code(v, key);
  if (c) {
    awra_key_cap_set_bound(c, state >= 1);
    awra_key_cap_set_active(c, state == 2);
    awra_key_cap_set_selected(c, state == 3);
  }
}

void awra_keyboard_view_set_key_color(AwraWidget *v, const char *key, const AwraRGBA *color)
{
  AwraWidget *c = kbd_cap_by_code(v, key);
  if (c) {
    awra_key_cap_set_tint(c, color);
  }
}

void awra_keyboard_view_clear(AwraWidget *v)
{
  for (int i = 0; i < v->nkids; i++) {
    awra_key_cap_set_bound(v->kids[i], false);
    awra_key_cap_set_active(v->kids[i], false);
    awra_key_cap_set_tint(v->kids[i], NULL);
  }
}

void awra_keyboard_view_flash_key(AwraWidget *v, const char *key)
{
  AwraWidget *c = kbd_cap_by_code(v, key);
  if (c) {
    awra_key_cap_flash(c);
  }
}

/* drops on a keyboard: the key under the pointer is the target */
bool awra_keyboard_view_drop(AwraWidget *v, int kind, int px, int py, const char *paths);
bool awra_keyboard_view_drop(AwraWidget *v, int kind, int px, int py, const char *paths)
{
  KbdExt *x = KBX(v);
  AwraWidget *target = NULL;
  for (int i = 0; i < v->nkids && kind != 2; i++) {
    if (bk_contains(v->kids[i], px, py)) {
      target = v->kids[i];
    }
  }
  if (x->drop_cap != target) {
    if (x->drop_cap) {
      awra_key_cap_set_drop_highlight(x->drop_cap, false);
    }
    x->drop_cap = target;
    if (target) {
      awra_key_cap_set_drop_highlight(target, true);
    }
  }
  if (kind == 3 && target) { /* dropped */
    awra_key_cap_set_drop_highlight(target, false);
    x->drop_cap = NULL;
    const size_t n = strlen(KX(target)->code) + strlen(paths) + 2;
    char *text = (char *)malloc(n);
    snprintf(text, n, "%s\n%s", KX(target)->code, paths);
    awra_emit_text(v, AWRA_SIGNAL_FILES_DROPPED, text);
    free(text);
    return true;
  }
  return target != NULL;
}

/* ======================================================================= */
/* Waveform (spec B §18) */

typedef struct WaveExt {
  float *peaks;
  int npeaks, expected;
  double duration, position, hint, marker;
  bool marker_editable, dragging_marker;
} WaveExt;

#define WVX(w) AWRA_EXT(w, WaveExt)

static void wave_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *mn = awra_px(48);
    *nt = awra_px(240);
  }
  else {
    *mn = awra_px(24);
    *nt = awra_px(48);
  }
}

static float wave_x(AwraWidget *w, double seconds)
{
  WaveExt *x = WVX(w);
  const double frac = x->duration > 0 ? seconds / x->duration : 0;
  return (float)(w->x + awra_pxf(4) + fmin(fmax(frac, 0), 1) * (w->w - awra_pxf(8)));
}

static void wave_draw(AwraWidget *w, AwraDraw *d)
{
  WaveExt *x = WVX(w);
  const AwraTokens *t = awra_tokens();
  AwraBox b = awra_box_of(w);
  const float wr = fminf(awra_pxf(7), b.h * 0.5f);
  awra_paint_rbox(d, b, wr, &t->control_well, t->high_contrast ? awra_pxf(1) : 0, &t->outline);
  const float pitch = awra_pxf(4), bw = awra_pxf(2.5f), inset = awra_pxf(4);
  const float span = fmaxf(b.w - 2 * inset, 1);
  const int nbars = BK_MAX(1, (int)floorf((span + awra_pxf(1.5f)) / pitch));
  const float amp = fmaxf(b.h * 0.5f - inset, 1);
  const float pos_x = x->duration > 0 && x->position >= 0 ? wave_x(w, x->position) : -1e9f;
  const float mark_x = x->marker >= 0 && x->duration > 0 ? wave_x(w, x->marker) : -1e9f;
  const int cap = BK_MAX(1, BK_MAX(x->expected, x->npeaks));
  for (int i = 0; i < nbars; i++) {
    float peak = 0.0f;
    const int a = i * cap / nbars, z = BK_MAX(a + 1, (i + 1) * cap / nbars);
    bool loaded = x->npeaks == 0;
    for (int k = a; k < z && k < x->npeaks; k++) {
      peak = fmaxf(peak, x->peaks[k]);
      loaded = true;
    }
    if (!loaded) {
      continue;
    }
    peak = fminf(fmaxf(peak, 0), 1);
    const float bh = fmaxf(awra_pxf(2), peak * 2 * amp);
    const float bx = b.x + inset + pitch * i;
    const bool played = bx + bw * 0.5f <= pos_x;
    AwraRGBA c = played ? t->accent : awra_with_alpha(t->foreground, x->npeaks ? 0.38f : 0.12f);
    if (bx + bw * 0.5f < mark_x) {
      c.a *= played ? 0.55f : 0.5f;
    }
    awra_paint_rbox(d, (AwraBox){bx, b.y + b.h * 0.5f - bh * 0.5f, bw, bh}, bw * 0.5f, &c, 0, NULL);
  }
  if (x->hint >= 0 && x->duration > 0) {
    const float hx = wave_x(w, x->hint);
    const AwraRGBA hc = awra_with_alpha(t->foreground, 0.62f);
    for (float y = b.y + awra_pxf(2); y < b.y + b.h - awra_pxf(2); y += awra_pxf(6)) {
      awra_paint_rect(d, (AwraBox){hx - awra_pxf(0.75f), y, awra_pxf(1.5f), fminf(awra_pxf(3), b.y + b.h - awra_pxf(2) - y)}, &hc);
    }
  }
  if (pos_x > -1e8f) {
    const AwraRGBA pc = awra_with_alpha(t->foreground, 0.85f);
    awra_paint_rect(d, (AwraBox){pos_x - awra_pxf(0.75f), b.y + awra_pxf(2), awra_pxf(1.5f), b.h - awra_pxf(4)}, &pc);
  }
  if (mark_x > -1e8f) {
    awra_paint_rect(d, (AwraBox){mark_x - awra_pxf(1), b.y, awra_pxf(2), b.h}, &t->accent_content);
    const float cy = b.y + fminf(awra_pxf(5.5f), b.h * 0.5f);
    awra_paint_circle(d, mark_x, cy, awra_pxf(4.5f) + awra_pxf(1), &t->control_thumb);
    awra_paint_circle(d, mark_x, cy, awra_pxf(4.5f), &t->accent_content);
  }
  if (awra_focus_visible(w)) {
    awra_paint_rbox(d, awra_box_inset(b, -awra_pxf(3)), wr + awra_pxf(3), NULL, awra_pxf(2), &t->focus_ring);
  }
}

static double wave_seconds(AwraWidget *w, int px)
{
  WaveExt *x = WVX(w);
  const double frac = (px - w->x - awra_pxf(4)) / fmax(1.0, w->w - awra_pxf(8));
  return fmin(fmax(frac, 0), 1) * x->duration;
}

static int wave_press(AwraWidget *w, BKEvent *e)
{
  WaveExt *x = WVX(w);
  if (x->duration <= 0) {
    return 1;
  }
  const double s = wave_seconds(w, e->x);
  if (x->marker_editable) {
    x->dragging_marker = true;
    x->marker = s;
    awra_invalidate(w);
    awra_emit_value(w, AWRA_SIGNAL_MARKER_MOVED, s);
    return 1;
  }
  awra_emit_value(w, AWRA_SIGNAL_SEEK, s);
  return 1;
}

static int wave_drag(AwraWidget *w, BKEvent *e)
{
  WaveExt *x = WVX(w);
  if (x->dragging_marker) {
    x->marker = wave_seconds(w, e->x);
    awra_invalidate(w);
    awra_emit_value(w, AWRA_SIGNAL_MARKER_MOVED, x->marker);
  }
  return 1;
}

static int wave_release(AwraWidget *w, BKEvent *e)
{
  WaveExt *x = WVX(w);
  if (x->dragging_marker) {
    x->dragging_marker = false;
    awra_emit_value(w, AWRA_SIGNAL_MARKER_COMMITTED, x->marker);
  }
  return 1;
}

static int wave_motion(AwraWidget *w, BKEvent *e)
{
  WaveExt *x = WVX(w);
  if (x->duration > 0) {
    x->hint = wave_seconds(w, e->x);
    awra_invalidate(w);
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "hint-position");
  }
  return 0;
}

static void wave_leave_handler(AwraWidget *w, const AwraArgs *a, void *data)
{
  WVX(w)->hint = -1;
  awra_invalidate(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "hint-position");
}

static void wave_finalize(AwraWidget *w)
{
  free(WVX(w)->peaks);
}

/* the waveform: a slider over the sound's time (the playing position) */
static void wave_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  WaveExt *x = WVX(w);
  info->has_value = true;
  info->min = 0.0;
  info->max = x->duration > 0 ? x->duration : 1.0;
  info->value = x->position;
  info->step = 1.0;
  const int p = x->position > 0 ? (int)x->position : 0, d = x->duration > 0 ? (int)x->duration : 0;
  snprintf(info->buf, sizeof(info->buf), "%d:%02d of %d:%02d", p / 60, p % 60, d / 60, d % 60);
  info->value_text = info->buf;
  info->flags |= AWRA_A11Y_HORIZONTAL;
}

static bool wave_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  if (item < 0 && verb == AWRA_A11Y_DO_SET_VALUE && awra_sensitive(w)) {
    awra_emit_value(w, AWRA_SIGNAL_SEEK, value);
    return true;
  }
  return false;
}

static AwraClass wave_class = {
    .a11y = wave_a11y,
    .a11y_do = wave_a11y_do,
    .bk = AWRA_BK("AwraWaveformView", CLS_INTERACTIVE),
    .type = "AwraWaveformView",
    .flags = AWRA_CLASS_FOCUSABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(WaveExt),
    .finalize = wave_finalize,
    .measure = wave_measure,
    .draw = wave_draw,
    .press = wave_press,
    .drag = wave_drag,
    .release = wave_release,
    .motion = wave_motion,
    .overdraw = 6,
};

AwraWidget *awra_waveform_view_new(void)
{
  AwraWidget *w = awra_widget_new(&wave_class);
  WaveExt *x = WVX(w);
  x->position = x->hint = x->marker = -1;
  awra_signal_connect(w, AWRA_SIGNAL_LEAVE, wave_leave_handler, NULL, NULL);
  return w;
}

void awra_waveform_view_set_peaks(AwraWidget *w, const float *peaks, int n)
{
  WaveExt *x = WVX(w);
  free(x->peaks);
  x->peaks = n > 0 ? (float *)malloc(sizeof(float) * (size_t)n) : NULL;
  if (x->peaks) {
    memcpy(x->peaks, peaks, sizeof(float) * (size_t)n);
  }
  x->npeaks = x->peaks ? n : 0;
  awra_invalidate(w);
}

void awra_waveform_view_set_expected_peaks(AwraWidget *w, int n)
{
  WVX(w)->expected = n;
  awra_invalidate(w);
}

void awra_waveform_view_set_duration(AwraWidget *w, double s)
{
  WVX(w)->duration = s;
  awra_invalidate(w);
}

void awra_waveform_view_set_position(AwraWidget *w, double s)
{
  if (WVX(w)->position != s) {
    WVX(w)->position = s;
    awra_invalidate(w);
  }
}

void awra_waveform_view_set_hint_position(AwraWidget *w, double s)
{
  WVX(w)->hint = s;
  awra_invalidate(w);
}

double awra_waveform_view_hint_position(AwraWidget *w)
{
  return WVX(w)->hint;
}

double awra_waveform_view_marker_position(AwraWidget *w)
{
  return WVX(w)->marker;
}

void awra_waveform_view_set_marker_position(AwraWidget *w, double s)
{
  WVX(w)->marker = s;
  awra_invalidate(w);
}

void awra_waveform_view_set_marker_editable(AwraWidget *w, bool e)
{
  WVX(w)->marker_editable = e;
}

/* ======================================================================= */
/* Media row (spec B §19) */

typedef struct MediaExt {
  AwraWidget *grid, *tile, *title, *subtitle, *play, *stop, *volume, *seek, *elapsed, *total,
      *suffix, *ends;
  bool has_accent, playing, seekable;
  AwraRGBA accent;
  double duration, position;
} MediaExt;

#define MX(w) AWRA_EXT(w, MediaExt)

static void media_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  const int ph = awra_px(20), pv = awra_px(16);
  awra_measure(MX(w)->grid, o, f >= 0 ? f - (o == AWRA_ORIENTATION_HORIZONTAL ? pv : ph) : -1, mn, nt);
  const int p = o == AWRA_ORIENTATION_HORIZONTAL ? ph : pv;
  *mn += p;
  *nt += p;
}

static void media_allocate(AwraWidget *w)
{
  awra_allocate(MX(w)->grid, w->x + awra_px(10), w->y + awra_px(8), w->w - awra_px(20),
                w->h - awra_px(16));
}

static void media_draw(AwraWidget *w, AwraDraw *d)
{
  MediaExt *x = MX(w);
  if (!awra_child_visible(x->tile)) {
    return;
  }
  AwraBox tb = awra_box_of(x->tile);
  if (x->has_accent) {
    awra_paint_rbox(d, tb, awra_pxf(11), &x->accent, 0, NULL);
  }
  else {
    awra_paint_rbox(d, tb, awra_pxf(11), &awra_tokens()->control_cap, 0, NULL);
    const float r4[4] = {awra_pxf(11), awra_pxf(11), awra_pxf(11), awra_pxf(11)},
                w4[4] = {awra_pxf(1), 0, 0, 0};
    awra_paint_rbox4(d, tb, r4, NULL, w4, &awra_tokens()->rim);
  }
}

/* Awra 1's AwraMediaRow: a group named by its title */
static void media_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  const char *t = awra_label_text(MX(w)->title);
  info->label = t && t[0] ? t : "Media";
}

static AwraClass media_class = {
    .a11y = media_a11y,
    .bk = AWRA_BK("AwraMediaRow", 0),
    .type = "AwraMediaRow",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(MediaExt),
    .measure = media_measure,
    .allocate = media_allocate,
    .draw = media_draw,
};

static void fmt_time(char *buf, size_t n, double s)
{
  const int t = s > 0 ? (int)s : 0;
  snprintf(buf, n, "%d:%02d", t / 60, t % 60);
}

/* the seek slider reads "0:12 of 3:45" (Awra 1) */
static void media_value_text(MediaExt *x)
{
  char a[16], b[16], vt[40];
  fmt_time(a, sizeof(a), x->position);
  fmt_time(b, sizeof(b), x->duration);
  snprintf(vt, sizeof(vt), "%s of %s", a, b);
  awra_accessible_update_property_string(x->seek, AWRA_ACCESSIBLE_PROPERTY_VALUE_TEXT, vt);
}

static void media_sync_times(AwraWidget *w)
{
  MediaExt *x = MX(w);
  char a[16], b[16];
  fmt_time(a, sizeof(a), x->position);
  fmt_time(b, sizeof(b), x->duration);
  awra_label_set_text(x->elapsed, a);
  awra_label_set_text(x->total, b);
  awra_slider_set_range(x->seek, 0, x->duration > 0 ? x->duration : 1);
  media_value_text(x);
}

static void media_play(AwraWidget *b, const AwraArgs *a, void *data)
{
  awra_emit_simple((AwraWidget *)data, AWRA_SIGNAL_PLAY_REQUESTED);
}

static void media_stop(AwraWidget *b, const AwraArgs *a, void *data)
{
  awra_emit_simple((AwraWidget *)data, AWRA_SIGNAL_STOP_REQUESTED);
}

static void media_volume(AwraWidget *v, const AwraArgs *a, void *data)
{
  awra_emit_value((AwraWidget *)data, AWRA_SIGNAL_VALUE_CHANGED, a->value);
}

static void media_seek(AwraWidget *s, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  if (awra_widget_get_data(w, "awra-syncing")) {
    return;
  }
  awra_emit_value(w, AWRA_SIGNAL_SEEK, a->value);
}

static AwraWidget *small_label(void)
{
  AwraWidget *l = awra_label_new("0:00");
  awra_widget_set_typography(l, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_scale(l, 0.82 / 0.88);
  awra_label_set_width_chars(l, 4);
  return l;
}

AwraWidget *awra_media_row_new(const char *title)
{
  AwraWidget *w = awra_widget_new(&media_class);
  MediaExt *x = MX(w);
  x->grid = awra_grid_new();
  awra_grid_set_row_spacing(x->grid, 4);
  awra_grid_set_column_spacing(x->grid, 12);
  x->tile = awra_image_new_from_icon_name(NULL);
  awra_image_set_pixel_size(x->tile, 18);
  awra_widget_set_size_request(x->tile, 36, 36);
  awra_widget_set_valign(x->tile, AWRA_ALIGN_CENTER);
  awra_widget_set_visible(x->tile, false);
  AwraWidget *texts = awra_box_new(AWRA_ORIENTATION_VERTICAL, 1);
  awra_widget_set_valign(texts, AWRA_ALIGN_CENTER);
  awra_widget_set_hexpand(texts, true);
  x->title = awra_label_new(title ? title : "");
  awra_label_set_xalign(x->title, 0.0f);
  awra_label_set_ellipsize(x->title, AWRA_ELLIPSIZE_END);
  awra_label_set_weight(x->title, 650);
  x->subtitle = awra_label_new("");
  awra_widget_set_typography(x->subtitle, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_xalign(x->subtitle, 0.0f);
  awra_label_set_ellipsize(x->subtitle, AWRA_ELLIPSIZE_END);
  awra_widget_set_visible(x->subtitle, false);
  awra_box_append(texts, x->title);
  awra_widget_unref(x->title); /* its parent holds it */
  awra_box_append(texts, x->subtitle);
  awra_widget_unref(x->subtitle); /* its parent holds it */
  x->ends = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 4);
  awra_widget_set_valign(x->ends, AWRA_ALIGN_CENTER);
  x->play = awra_button_new_from_icon_name("media-playback-start-symbolic");
  awra_button_set_appearance(x->play, AWRA_BUTTON_TOOLBAR);
  x->stop = awra_button_new_from_icon_name("media-playback-stop-symbolic");
  awra_button_set_appearance(x->stop, AWRA_BUTTON_TOOLBAR);
  awra_accessible_update_property_string(x->play, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Play");
  awra_accessible_update_property_string(x->stop, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Stop");
  x->volume = awra_volume_control_new();
  awra_widget_set_size_request(x->volume, 120, -1);
  awra_widget_set_visible(x->volume, false);
  awra_box_append(x->ends, x->play);
  awra_widget_unref(x->play); /* its parent holds it */
  awra_box_append(x->ends, x->stop);
  awra_widget_unref(x->stop); /* its parent holds it */
  awra_box_append(x->ends, x->volume);
  awra_widget_unref(x->volume); /* its parent holds it */
  AwraWidget *seekbox = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 8);
  x->elapsed = small_label();
  awra_label_set_xalign(x->elapsed, 1.0f);
  x->seek = awra_slider_new(0, 1, 0);
  awra_widget_set_hexpand(x->seek, true);
  awra_widget_set_valign(x->seek, AWRA_ALIGN_CENTER);
  x->total = small_label();
  awra_label_set_xalign(x->total, 0.0f);
  awra_accessible_update_property_string(x->seek, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Seek");
  awra_accessible_update_state(x->elapsed, AWRA_ACCESSIBLE_STATE_HIDDEN, 1);
  awra_accessible_update_state(x->total, AWRA_ACCESSIBLE_STATE_HIDDEN, 1);
  awra_box_append(seekbox, x->elapsed);
  awra_box_append(seekbox, x->seek);
  awra_widget_unref(x->seek); /* its parent holds it */
  awra_box_append(seekbox, x->total);
  awra_grid_attach(x->grid, x->tile, 0, 0, 1, 2);
  awra_grid_attach(x->grid, texts, 1, 0, 1, 1);
  awra_widget_unref(texts); /* its parent holds it */
  awra_grid_attach(x->grid, x->ends, 2, 0, 1, 1);
  awra_widget_unref(x->ends); /* its parent holds it */
  awra_grid_attach(x->grid, seekbox, 1, 1, 2, 1);
  awra_widget_unref(seekbox); /* its parent holds it */
  awra_child_insert(w, x->grid, -1);
  awra_widget_unref(x->grid); /* its parent holds it */
  awra_signal_connect(x->play, AWRA_SIGNAL_CLICKED, media_play, w, NULL);
  awra_signal_connect(x->stop, AWRA_SIGNAL_CLICKED, media_stop, w, NULL);
  awra_signal_connect(x->volume, AWRA_SIGNAL_VALUE_CHANGED, media_volume, w, NULL);
  awra_signal_connect(x->seek, AWRA_SIGNAL_VALUE_CHANGED, media_seek, w, NULL);
  x->seekable = true;
  media_sync_times(w);
  return w;
}

void awra_media_row_set_title(AwraWidget *w, const char *t)
{
  awra_label_set_text(MX(w)->title, t);
}

void awra_media_row_set_subtitle(AwraWidget *w, const char *s)
{
  awra_label_set_text(MX(w)->subtitle, s);
  awra_widget_set_visible(MX(w)->subtitle, s && s[0]);
}

void awra_media_row_set_icon_name(AwraWidget *w, const char *icon)
{
  MediaExt *x = MX(w);
  awra_image_set_icon_name(x->tile, icon);
  awra_widget_set_visible(x->tile, (icon && icon[0]) || x->has_accent);
}

void awra_media_row_set_accent_color(AwraWidget *w, const AwraRGBA *c)
{
  MediaExt *x = MX(w);
  x->has_accent = c != NULL;
  if (c) {
    x->accent = *c;
    const float l = 0.2126f * c->r + 0.7152f * c->g + 0.0722f * c->b;
    AwraRGBA fg;
    awra_parse_color(l > 0.55f ? "#15121a" : "#ffffff", &fg);
    awra_image_set_color(x->tile, &fg);
  }
  else {
    awra_image_set_color(x->tile, NULL);
  }
  awra_widget_set_visible(x->tile, true);
  awra_invalidate(w);
}

void awra_media_row_set_show_play_button(AwraWidget *w, bool s)
{
  awra_widget_set_visible(MX(w)->play, s);
}

void awra_media_row_set_show_stop_button(AwraWidget *w, bool s)
{
  awra_widget_set_visible(MX(w)->stop, s);
}

void awra_media_row_set_show_volume(AwraWidget *w, bool s)
{
  awra_widget_set_visible(MX(w)->volume, s);
}

void awra_media_row_set_volume(AwraWidget *w, double v)
{
  awra_volume_control_set_value(MX(w)->volume, v);
}

void awra_media_row_set_playing(AwraWidget *w, bool p)
{
  MediaExt *x = MX(w);
  x->playing = p;
  awra_button_set_icon_name(x->play, p ? "media-playback-pause-symbolic" : "media-playback-start-symbolic");
  awra_accessible_update_property_string(x->play, AWRA_ACCESSIBLE_PROPERTY_LABEL, p ? "Pause" : "Play");
  awra_label_set_color(x->title, p ? NULL : &awra_tokens()->disabled);
  awra_widget_set_sensitive(x->seek, p && x->seekable);
}

void awra_media_row_set_duration(AwraWidget *w, double s)
{
  MX(w)->duration = s;
  media_sync_times(w);
}

void awra_media_row_set_position(AwraWidget *w, double s)
{
  MediaExt *x = MX(w);
  x->position = s;
  awra_widget_set_data(w, "awra-syncing", (void *)1, NULL);
  awra_slider_set_value(x->seek, s);
  awra_widget_set_data(w, "awra-syncing", NULL, NULL);
  char a[16];
  fmt_time(a, sizeof(a), s);
  awra_label_set_text(x->elapsed, a);
  media_value_text(x);
}

void awra_media_row_set_seekable(AwraWidget *w, bool s)
{
  MX(w)->seekable = s;
  awra_widget_set_sensitive(MX(w)->seek, s && MX(w)->playing);
}

void awra_media_row_set_suffix(AwraWidget *w, AwraWidget *suffix)
{
  MediaExt *x = MX(w);
  if (x->suffix) {
    awra_box_remove(x->ends, x->suffix);
  }
  x->suffix = suffix;
  if (suffix) {
    awra_box_append(x->ends, suffix);
  }
}

/* ======================================================================= */
/* Drop zone (spec B §20) */

typedef struct ZoneExt {
  AwraWidget *well, *icon, *title, *subtitle, *browse, *child;
  bool hover;
} ZoneExt;

#define ZX(w) AWRA_EXT(w, ZoneExt)

static void zone_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  ZoneExt *x = ZX(w);
  awra_measure(x->child ? x->child : x->well, o, f, mn, nt);
}

static void zone_allocate(AwraWidget *w)
{
  ZoneExt *x = ZX(w);
  awra_allocate(x->child ? x->child : x->well, w->x, w->y, w->w, w->h);
}

static void zone_paint_frame(AwraWidget *w, AwraDraw *d, AwraBox b);

static void zone_draw(AwraWidget *w, AwraDraw *d)
{
  ZoneExt *x = ZX(w);
  if (x->child) {
    return;
  }
  zone_paint_frame(w, d, awra_box_of(w));
}

/* A zone wrapping content shows its message over it while files are
 * dragged over (Awra 1's AwraDropZone:drop-message). */
static void zone_draw_over(AwraWidget *w, AwraDraw *d)
{
  ZoneExt *x = ZX(w);
  if (!x->child || !x->hover) {
    return;
  }
  const AwraTokens *t = awra_tokens();
  const AwraBox b = awra_box_inset(awra_box_of(w), awra_pxf(12));
  AwraRGBA under = t->material_tint[AWRA_MATERIAL_FLOATING];
  under.a = 0.94f;
  awra_paint_rbox(d, b, awra_pxf(17), &under, 0, NULL);
  zone_paint_frame(w, d, b);
  const int s = awra_px(32);
  const char *msg = awra_label_text(x->title);
  float th = 0;
  int lw = 0, lh = 0;
  AwraTextLayout *l = NULL;
  if (msg && msg[0]) {
    l = awra_text_layout_new();
    AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    f.weight = 640;
    awra_text_layout_set_font(l, &f);
    awra_text_layout_set_text(l, msg, false);
    awra_text_layout_set_wrap(l, AWRA_WRAP_WORD, AWRA_ELLIPSIZE_NONE, -1);
    awra_text_layout_set_justify(l, AWRA_JUSTIFY_CENTER);
    awra_text_layout_size(l, (int)(b.w - awra_pxf(48)), &lw, &lh);
    th = (float)lh + awra_pxf(8);
  }
  const float top = b.y + (b.h - s - th) * 0.5f;
  awra_paint_icon(d, "document-send-symbolic", b.x + floorf((b.w - s) * 0.5f), floorf(top), s, &t->accent_content);
  if (l) {
    awra_text_layout_draw(l, d, b.x + floorf((b.w - lw) * 0.5f), top + s + awra_pxf(8), lw, &t->foreground);
    awra_text_layout_free(l);
  }
}

static void zone_paint_frame(AwraWidget *w, AwraDraw *d, AwraBox b)
{
  ZoneExt *x = ZX(w);
  const AwraTokens *t = awra_tokens();
  awra_paint_rbox(d, b, awra_pxf(17), x->hover ? &t->accent_soft : &t->control_well, 0, NULL);
  /* 2 px dashed border */
  const AwraRGBA c = x->hover ? t->accent : (t->high_contrast ? t->outline : t->border);
  const float bw = awra_pxf(2), dash = awra_pxf(6), r = awra_pxf(17);
  for (float px = b.x + r; px < b.x + b.w - r; px += dash * 2) {
    const float len = fminf(dash, b.x + b.w - r - px);
    awra_paint_rect(d, (AwraBox){px, b.y, len, bw}, &c);
    awra_paint_rect(d, (AwraBox){px, b.y + b.h - bw, len, bw}, &c);
  }
  for (float py = b.y + r; py < b.y + b.h - r; py += dash * 2) {
    const float len = fminf(dash, b.y + b.h - r - py);
    awra_paint_rect(d, (AwraBox){b.x, py, bw, len}, &c);
    awra_paint_rect(d, (AwraBox){b.x + b.w - bw, py, bw, len}, &c);
  }
  /* corners as solid arcs (short) */
  const float r4[4] = {r, r, r, r}, w4[4] = {bw, bw, bw, bw};
  bk_clip_push((int)b.x, (int)b.y, (int)r, (int)r);
  awra_paint_rbox4(d, b, r4, NULL, w4, &c);
  bk_clip_pop();
  bk_clip_push((int)(b.x + b.w - r), (int)b.y, (int)r + 1, (int)r);
  awra_paint_rbox4(d, b, r4, NULL, w4, &c);
  bk_clip_pop();
  bk_clip_push((int)b.x, (int)(b.y + b.h - r), (int)r, (int)r + 1);
  awra_paint_rbox4(d, b, r4, NULL, w4, &c);
  bk_clip_pop();
  bk_clip_push((int)(b.x + b.w - r), (int)(b.y + b.h - r), (int)r + 1, (int)r + 1);
  awra_paint_rbox4(d, b, r4, NULL, w4, &c);
  bk_clip_pop();
}

/* a drop zone: a group named by its title, described by its subtitle */
static void zone_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  ZoneExt *x = ZX(w);
  if (awra_widget_visible(x->title)) {
    info->label = awra_label_text(x->title);
  }
  if (awra_widget_visible(x->subtitle)) {
    info->description = awra_label_text(x->subtitle);
  }
}

static AwraClass zone_class = {
    .a11y = zone_a11y,
    .bk = AWRA_BK("AwraDropZone", 0),
    .type = "AwraDropZone",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(ZoneExt),
    .measure = zone_measure,
    .allocate = zone_allocate,
    .draw = zone_draw,
    .draw_over = zone_draw_over,
};

static void zone_browse(AwraWidget *b, const AwraArgs *a, void *data)
{
  awra_emit_simple((AwraWidget *)data, AWRA_SIGNAL_BROWSE_REQUESTED);
}

static void zone_hover(AwraWidget *w, const AwraArgs *a, void *data)
{
  ZoneExt *x = ZX(w);
  x->hover = a->active;
  awra_image_set_color(x->icon, a->active ? &awra_tokens()->accent_content : &awra_tokens()->muted);
  awra_invalidate(w);
}

AwraWidget *awra_drop_zone_new(void)
{
  AwraWidget *w = awra_widget_new(&zone_class);
  ZoneExt *x = ZX(w);
  x->well = awra_box_new(AWRA_ORIENTATION_VERTICAL, 8);
  awra_widget_set_margins(x->well, 24, 24, 24, 24);
  awra_widget_set_size_request(x->well, -1, 120);
  x->icon = awra_image_new_from_icon_name("document-send-symbolic");
  awra_image_set_pixel_size(x->icon, 32);
  awra_image_set_color(x->icon, &awra_tokens()->muted);
  awra_widget_set_valign(x->icon, AWRA_ALIGN_END);
  awra_widget_set_vexpand(x->icon, true);
  x->title = awra_label_new("");
  awra_label_set_scale(x->title, 1.08);
  awra_label_set_weight(x->title, 640);
  awra_label_set_wrap(x->title, AWRA_WRAP_WORD);
  awra_label_set_justify(x->title, AWRA_JUSTIFY_CENTER);
  awra_widget_set_visible(x->title, false);
  x->subtitle = awra_label_new("");
  awra_widget_set_typography(x->subtitle, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_wrap(x->subtitle, AWRA_WRAP_WORD);
  awra_label_set_justify(x->subtitle, AWRA_JUSTIFY_CENTER);
  awra_widget_set_visible(x->subtitle, false);
  x->browse = awra_button_new_with_label("Browse\xE2\x80\xA6");
  awra_widget_set_halign(x->browse, AWRA_ALIGN_CENTER);
  awra_widget_set_valign(x->browse, AWRA_ALIGN_START);
  awra_widget_set_margin_top(x->browse, 4);
  awra_widget_set_visible(x->browse, false);
  awra_signal_connect(x->browse, AWRA_SIGNAL_CLICKED, zone_browse, w, NULL);
  AwraWidget *spacer = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_vexpand(spacer, true);
  awra_box_append(x->well, x->icon);
  awra_widget_unref(x->icon); /* its parent holds it */
  awra_box_append(x->well, x->title);
  awra_widget_unref(x->title); /* its parent holds it */
  awra_box_append(x->well, x->subtitle);
  awra_widget_unref(x->subtitle); /* its parent holds it */
  awra_box_append(x->well, x->browse);
  awra_widget_unref(x->browse); /* its parent holds it */
  awra_box_append(x->well, spacer);
  awra_widget_unref(spacer); /* its parent holds it */
  awra_child_insert(w, x->well, -1);
  awra_widget_unref(x->well); /* its parent holds it */
  awra_widget_set_accepts_file_drops(w, true);
  awra_signal_connect(w, AWRA_SIGNAL_DROP_HOVER, zone_hover, NULL, NULL);
  return w;
}

void awra_drop_zone_set_hover(AwraWidget *w, bool hover)
{
  AwraArgs a = {0};
  a.active = hover;
  zone_hover(w, &a, NULL);
}

void awra_drop_zone_set_title(AwraWidget *w, const char *t)
{
  awra_label_set_text(ZX(w)->title, t);
  awra_widget_set_visible(ZX(w)->title, t && t[0]);
}

void awra_drop_zone_set_subtitle(AwraWidget *w, const char *s)
{
  awra_label_set_text(ZX(w)->subtitle, s);
  awra_widget_set_visible(ZX(w)->subtitle, s && s[0]);
}

void awra_drop_zone_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_image_set_icon_name(ZX(w)->icon, icon);
}

void awra_drop_zone_set_browse_label(AwraWidget *w, const char *label)
{
  if (label) {
    awra_button_set_label(ZX(w)->browse, label);
  }
  awra_widget_set_visible(ZX(w)->browse, label != NULL);
}

void awra_drop_zone_set_child(AwraWidget *w, AwraWidget *child)
{
  ZoneExt *x = ZX(w);
  awra_widget_set_visible(x->well, child == NULL);
  awra_set_slot(w, &x->child, child);
}

/* ======================================================================= */
/* Drawing areas and their canvas */

typedef struct AreaExt {
  AwraDrawFunc fn;
  void *data;
  AwraDestroyNotify destroy;
  int cw, ch;
} AreaExt;

#define AX(w) AWRA_EXT(w, AreaExt)

static void area_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  *mn = 0;
  *nt = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? AX(w)->cw : AX(w)->ch);
}

static void area_draw(AwraWidget *w, AwraDraw *d)
{
  AreaExt *x = AX(w);
  if (!x->fn) {
    return;
  }
  AwraCanvas c = {d, (float)w->x, (float)w->y, awra_scale()};
  bk_clip_push(w->x, w->y, w->w, w->h);
  x->fn(w, &c, awra_widget_width(w), awra_widget_height(w), x->data);
  bk_clip_pop();
}

static void area_finalize(AwraWidget *w)
{
  if (AX(w)->destroy) {
    AX(w)->destroy(AX(w)->data);
  }
}

static AwraClass area_class = {
    .bk = AWRA_BK("AwraDrawingArea", 0),
    .type = "AwraDrawingArea",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(AreaExt),
    .finalize = area_finalize,
    .measure = area_measure,
    .draw = area_draw,
};

AwraWidget *awra_drawing_area_new(AwraDrawFunc draw, void *data, AwraDestroyNotify destroy)
{
  AwraWidget *w = awra_widget_new(&area_class);
  AX(w)->fn = draw;
  AX(w)->data = data;
  AX(w)->destroy = destroy;
  return w;
}

void awra_drawing_area_set_content_size(AwraWidget *w, int width, int height)
{
  AX(w)->cw = width;
  AX(w)->ch = height;
  awra_widget_queue_resize(w);
}

#define CB(x0, y0, w0, h0) \
  (AwraBox){c->ox + (float)(x0) * c->scale, c->oy + (float)(y0) * c->scale, (float)(w0) * c->scale, (float)(h0) * c->scale}

void awra_canvas_fill_rect(AwraCanvas *c, double x, double y, double w, double h, const AwraRGBA *rgba)
{
  awra_paint_rect(c->d, CB(x, y, w, h), rgba);
}

void awra_canvas_round_rect(AwraCanvas *c, double x, double y, double w, double h, double r,
                            const AwraRGBA *fill, double bw, const AwraRGBA *border)
{
  awra_paint_rbox(c->d, CB(x, y, w, h), (float)(r * c->scale), fill, (float)(bw * c->scale), border);
}

void awra_canvas_line(AwraCanvas *c, double x0, double y0, double x1, double y1, double width,
                      const AwraRGBA *rgba)
{
  awra_paint_line(c->d, c->ox + (float)x0 * c->scale, c->oy + (float)y0 * c->scale,
                  c->ox + (float)x1 * c->scale, c->oy + (float)y1 * c->scale, (float)width * c->scale, rgba);
}

void awra_canvas_circle(AwraCanvas *c, double cx, double cy, double r, const AwraRGBA *rgba)
{
  awra_paint_circle(c->d, c->ox + (float)cx * c->scale, c->oy + (float)cy * c->scale, (float)r * c->scale, rgba);
}

void awra_canvas_text(AwraCanvas *c, double x, double y, const char *text, AwraTypography t,
                      const AwraRGBA *rgba)
{
  AwraFont f = awra_font_for(t);
  awra_draw_text(c->d, &f, c->ox + (float)x * c->scale, c->oy + (float)y * c->scale + awra_font_ascent(&f),
                 text, -1, rgba);
}

void awra_canvas_icon(AwraCanvas *c, const char *icon, double x, double y, double size,
                      const AwraRGBA *rgba)
{
  awra_paint_icon(c->d, icon, c->ox + (float)x * c->scale, c->oy + (float)y * c->scale,
                  (int)(size * c->scale + 0.5), rgba);
}
