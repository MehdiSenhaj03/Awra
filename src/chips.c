/* Awra 2 small content pieces (Awra 1's AwraChip, AwraDivider, AwraSkeleton,
 * AwraTypingIndicator, AwraBadgeGroup, AwraMetadataGroup, AwraMetricRow).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

#include "GPU_raster.h"

/* ======================================================================= */
/* Chip: a pill with a picture, emoji or icon, a label and a count; it
 * toggles when toggleable (reactions). Sizes follow the control height
 * (34 px): 28 px tall, 18 px pictures, 6 px gaps, 10 px sides. */

#define CONTROL_HEIGHT 34.0
#define PRESSED_SCALE 0.9f

typedef struct ChipExt {
  char *label, *icon_name, *emoji;
  AwraTexture *texture;
  int count, previous_count;
  bool active, toggleable;
  int appearance; /* AwraChipAppearance */
  float active_mix, hover_mix, roll, scale;
} ChipExt;

#define CHX(w) AWRA_EXT(w, ChipExt)

static float chip_height(void)
{
  return roundf((float)(CONTROL_HEIGHT * 0.82));
}
static float chip_icon(void)
{
  return roundf(chip_height() * 0.64f);
}
static float chip_gap(void)
{
  return roundf(chip_height() * 0.2f);
}
static float chip_side(void)
{
  return roundf(chip_height() * 0.34f);
}

static bool chip_has_leading(const ChipExt *x)
{
  return x->texture || (x->emoji && x->emoji[0]) || x->icon_name;
}

static AwraFont chip_label_font(void)
{
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.size *= 0.92f;
  f.weight = 600;
  f.letter_spacing = 0;
  return f;
}

static AwraFont chip_count_font(void)
{
  AwraFont f = chip_label_font();
  f.weight = 700;
  return f;
}

/* logical px */
static float chip_content_width(const ChipExt *x)
{
  const float s = awra_scale();
  float w = 0;
  int parts = 0;
  if (chip_has_leading(x)) {
    w += chip_icon();
    parts++;
  }
  if (x->label && x->label[0]) {
    AwraFont f = chip_label_font();
    w += ceilf(awra_text_width(&f, x->label, -1)) / s;
    parts++;
  }
  if (x->count >= 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", x->count);
    AwraFont f = chip_count_font();
    w += ceilf(awra_text_width(&f, buf, -1)) / s;
    parts++;
  }
  return w + BK_MAX(parts - 1, 0) * chip_gap();
}

static void chip_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)for_size;
  const float h = chip_height();
  if (o == AWRA_ORIENTATION_VERTICAL) {
    *r_min = *r_nat = awra_px(h);
    return;
  }
  const float wid = fmaxf(h, ceilf(chip_content_width(CHX(w)) + 2 * chip_side()));
  *r_min = *r_nat = awra_px(wid);
}

static void chip_colors(AwraWidget *w, AwraRGBA *bg, AwraRGBA *border, AwraRGBA *fg, AwraRGBA *count)
{
  ChipExt *x = CHX(w);
  const AwraTokens *t = awra_tokens();
  const AwraRGBA clear = {0, 0, 0, 0};
  const AwraRGBA plate = t->dark ? t->control_cap : t->control_well;
  const AwraRGBA hover_fill = t->dark ? t->control_cap_hover : awra_with_alpha(t->control_well, fminf(1.0f, t->control_well.a * 1.6f));
  AwraRGBA rb, rbo, rf, rc, ob, obo, of, oc;
  if (x->appearance == 1) {
    rb = t->accent_soft;
    rbo = clear;
    rf = t->accent_content;
    rc = rf;
    ob = t->accent;
    obo = t->accent;
    of = t->on_accent;
    oc = of;
  }
  else {
    rb = plate;
    rbo = clear;
    rf = t->foreground;
    rc = t->muted;
    ob = t->accent_soft;
    obo = awra_with_alpha(t->accent, 0.9f);
    of = t->accent_content;
    oc = of;
  }
  if (t->high_contrast) {
    rbo = t->outline;
  }
  const float hm = awra_hover_t(w);
  if (x->appearance == 0) {
    rb = awra_mix(rb, hover_fill, hm);
  }
  rbo = awra_mix(rbo, t->outline, hm);
  *bg = awra_mix(rb, ob, x->active_mix);
  *border = awra_mix(rbo, obo, x->active_mix);
  *fg = awra_mix(rf, of, x->active_mix);
  *count = awra_mix(rc, oc, x->active_mix);
}

static void chip_draw(AwraWidget *w, AwraDraw *d)
{
  ChipExt *x = CHX(w);
  const float s = awra_scale();
  const float ch = fminf((float)w->h, awra_pxf(chip_height()));
  const float top = roundf((w->h - ch) / 2.0f);
  AwraRGBA bg, border, fg, cnt;
  chip_colors(w, &bg, &border, &fg, &cnt);
  const float press = 1.0f - (1.0f - PRESSED_SCALE) * awra_press_t(w);
  const float sc = press * x->scale;
  const float bw = w->w * sc, bh = ch * sc;
  const float bx = w->x + (w->w - bw) / 2.0f, by = w->y + top + (ch - bh) / 2.0f;
  const float saved = d->opacity;
  if (!awra_sensitive(w)) {
    d->opacity *= 0.5f;
  }
  awra_paint_rbox(d, (AwraBox){bx, by, bw, bh}, bh / 2.0f, &bg, border.a > 0 ? s * sc : 0, border.a > 0 ? &border : NULL);
  float cx = bx + awra_pxf(chip_side()) * sc;
  const float cy = by;
  if (chip_has_leading(x)) {
    const float size = awra_pxf(chip_icon()) * sc;
    const float iy = cy + roundf((bh - size) / 2.0f);
    if (x->texture) {
      awra_paint_texture(d, x->texture, (AwraBox){cx, iy, size, size}, 0, 1);
    }
    else if (x->emoji && x->emoji[0]) {
      AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
      f.size = size * 0.82f;
      f.letter_spacing = 0;
      const float ew = awra_text_width(&f, x->emoji, -1);
      const float asc = awra_font_ascent(&f), lh = awra_font_line_height(&f);
      awra_draw_text(d, &f, roundf(cx + (size - ew) / 2.0f), roundf(cy + (bh - lh) / 2.0f + asc), x->emoji, -1,
                     &fg);
    }
    else if (x->icon_name) {
      const int px = (int)roundf(size * 0.9f);
      awra_paint_icon(d, x->icon_name, roundf(cx + (size - px) / 2.0f), roundf(cy + (bh - px) / 2.0f), px, &fg);
    }
    cx += size + awra_pxf(chip_gap()) * sc;
  }
  if (x->label && x->label[0]) {
    AwraFont f = chip_label_font();
    f.size *= sc;
    const float asc = awra_font_ascent(&f), lh = awra_font_line_height(&f);
    awra_draw_text(d, &f, roundf(cx), roundf(cy + (bh - lh) / 2.0f + asc), x->label, -1, &fg);
    cx += ceilf(awra_text_width(&f, x->label, -1)) + awra_pxf(chip_gap()) * sc;
  }
  if (x->count >= 0) {
    char buf[16], prev[16];
    snprintf(buf, sizeof(buf), "%d", x->count);
    AwraFont f = chip_count_font();
    f.size *= sc;
    const float asc = awra_font_ascent(&f), lh = awra_font_line_height(&f);
    const float base = roundf(cy + (bh - lh) / 2.0f + asc);
    if (x->roll >= 1.0f || x->previous_count < 0) {
      awra_draw_text(d, &f, roundf(cx), base, buf, -1, &cnt);
    }
    else {
      /* the digits roll: the old count leaves, the new one comes */
      snprintf(prev, sizeof(prev), "%d", x->previous_count);
      const int dir = x->count > x->previous_count ? 1 : -1;
      const float e = (float)awra_ease_out_cubic(x->roll);
      const float travel = bh * 0.7f;
      bk_clip_push((int)(cx - 2), (int)cy, (int)(awra_text_width(&f, buf, -1) + 4), (int)bh);
      AwraRGBA a = awra_with_alpha(cnt, cnt.a * (1.0f - e));
      awra_draw_text(d, &f, roundf(cx), base - dir * travel * e, prev, -1, &a);
      a = awra_with_alpha(cnt, cnt.a * e);
      awra_draw_text(d, &f, roundf(cx), base + dir * travel * (1.0f - e), buf, -1, &a);
      bk_clip_pop();
    }
  }
  if (awra_focus_visible(w)) {
    const float o = awra_pxf(3);
    awra_paint_rbox(d, (AwraBox){bx - o, by - o, bw + 2 * o, bh + 2 * o}, bh / 2 + o, NULL, awra_pxf(2),
                    &awra_tokens()->focus_ring);
  }
  d->opacity = saved;
}

static void chip_active_anim(AwraWidget *w, double t, void *data)
{
  const float from = (float)(intptr_t)data / 1000.0f;
  ChipExt *x = CHX(w);
  x->active_mix = from + ((x->active ? 1.0f : 0.0f) - from) * (float)t;
  awra_invalidate(w);
}

static void chip_set_active_internal(AwraWidget *w, bool active)
{
  ChipExt *x = CHX(w);
  if (x->active == active) {
    return;
  }
  x->active = active;
  if (!awra_aux(w)->mapped || awra_style_reduced_motion()) {
    x->active_mix = active ? 1.0f : 0.0f;
    awra_invalidate(w);
    return;
  }
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_FAST), chip_active_anim,
                  (void *)(intptr_t)(x->active_mix * 1000.0f), NULL);
}

static void chip_activate(AwraWidget *w)
{
  ChipExt *x = CHX(w);
  awra_widget_ref(w);
  if (x->toggleable) {
    chip_set_active_internal(w, !x->active);
    awra_emit_active(w, AWRA_SIGNAL_TOGGLED, x->active);
  }
  awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
  awra_widget_unref(w);
}

static int chip_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside) {
    chip_activate(w);
  }
  return 1;
}

static void chip_finalize(AwraWidget *w)
{
  ChipExt *x = CHX(w);
  free(x->label);
  free(x->icon_name);
  free(x->emoji);
  awra_texture_unref(x->texture);
}

/* Awra 1's AwraChip: a toggle button named "label, count", pressed when
 * active */
static void chip_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  ChipExt *x = CHX(w);
  const char *name = x->label && x->label[0] ? x->label : x->emoji;
  if (name && x->count >= 0) {
    snprintf(info->buf, sizeof(info->buf), "%s, %d", name, x->count);
    info->label = info->buf;
  }
  else if (name) {
    info->label = name;
  }
  else if (x->count >= 0) {
    snprintf(info->buf, sizeof(info->buf), "%d", x->count);
    info->label = info->buf;
  }
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_PRESSED, x->active);
}

static AwraClass chip_class = {
    .a11y = chip_a11y,
    .bk = AWRA_BK("AwraChip", CLS_INTERACTIVE),
    .type = "AwraChip",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(ChipExt),
    .finalize = chip_finalize,
    .measure = chip_measure,
    .draw = chip_draw,
    .release = chip_release,
    .activate = chip_activate,
    .overdraw = 4,
};

AwraWidget *awra_chip_new(const char *label)
{
  AwraWidget *w = awra_widget_new(&chip_class);
  ChipExt *x = CHX(w);
  x->label = label ? awra_strdup(label) : NULL;
  x->count = -1;
  x->previous_count = -1;
  x->roll = 1.0f;
  x->scale = 1.0f;
  awra_widget_set_cursor(w, "pointer");
  return w;
}

static void chip_changed(AwraWidget *w)
{
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_chip_set_label(AwraWidget *w, const char *label)
{
  awra_set_str(&CHX(w)->label, label);
  chip_changed(w);
}

const char *awra_chip_label(AwraWidget *w)
{
  return CHX(w)->label;
}

void awra_chip_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_set_str(&CHX(w)->icon_name, icon);
  chip_changed(w);
}

void awra_chip_set_texture(AwraWidget *w, AwraTexture *texture)
{
  ChipExt *x = CHX(w);
  if (x->texture == texture) {
    return;
  }
  const bool had = x->texture != NULL;
  awra_texture_ref(texture);
  awra_texture_unref(x->texture);
  x->texture = texture;
  if (had != (texture != NULL)) {
    chip_changed(w);
  }
  awra_invalidate(w);
}

void awra_chip_set_emoji(AwraWidget *w, const char *emoji)
{
  awra_set_str(&CHX(w)->emoji, emoji);
  chip_changed(w);
}

static void chip_roll_anim(AwraWidget *w, double t, void *data)
{
  (void)data;
  CHX(w)->roll = (float)t;
  awra_invalidate(w);
}

void awra_chip_set_count(AwraWidget *w, int count)
{
  ChipExt *x = CHX(w);
  if (x->count == count) {
    return;
  }
  x->previous_count = x->count;
  x->count = count;
  chip_changed(w);
  if (x->previous_count >= 0 && count >= 0 && awra_aux(w)->mapped && !awra_style_reduced_motion()) {
    x->roll = 0.0f;
    awra_anim_start(w, awra_motion_duration(AWRA_MOTION_NORMAL), chip_roll_anim, NULL, NULL);
  }
  else {
    x->roll = 1.0f;
  }
}

int awra_chip_count(AwraWidget *w)
{
  return CHX(w)->count;
}

void awra_chip_set_active(AwraWidget *w, bool active)
{
  chip_set_active_internal(w, active);
}

bool awra_chip_active(AwraWidget *w)
{
  return CHX(w)->active;
}

void awra_chip_set_toggleable(AwraWidget *w, bool toggleable)
{
  CHX(w)->toggleable = toggleable;
}

void awra_chip_set_appearance(AwraWidget *w, int appearance)
{
  CHX(w)->appearance = appearance;
  awra_invalidate(w);
}

/* ======================================================================= */
/* Divider: a 1 px line through the middle, an optional label (start,
 * center or end) 10 px from it; 6 px above and below (awra.css). */

#define LABEL_GAP 10

typedef struct DividerExt {
  AwraWidget *label;
  int position; /* AwraDividerLabelPosition */
  int appearance; /* AwraDividerAppearance */
} DividerExt;

#define DVX(w) AWRA_EXT(w, DividerExt)

static bool divider_has_label(AwraWidget *w)
{
  return DVX(w)->label && awra_widget_visible(DVX(w)->label);
}

static void divider_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)for_size;
  DividerExt *x = DVX(w);
  const int pad = awra_px(6) * 2;
  if (!divider_has_label(w)) {
    *r_min = *r_nat = o == AWRA_ORIENTATION_VERTICAL ? awra_px(1) + pad : 0;
    return;
  }
  awra_measure(x->label, o, -1, r_min, r_nat);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    const int gaps = awra_px(x->position == AWRA_DIVIDER_LABEL_CENTER ? 2 * LABEL_GAP : LABEL_GAP);
    *r_min += gaps;
    *r_nat += gaps;
  }
  else {
    *r_min = BK_MAX(*r_min, awra_px(1)) + pad;
    *r_nat = BK_MAX(*r_nat, awra_px(1)) + pad;
  }
}

static void divider_allocate(AwraWidget *w)
{
  DividerExt *x = DVX(w);
  if (!divider_has_label(w)) {
    return;
  }
  const int pad = awra_px(6);
  const int width = w->w, height = w->h - 2 * pad;
  int lmin, lw, lh, mn;
  awra_measure(x->label, AWRA_ORIENTATION_HORIZONTAL, -1, &lmin, &lw);
  const int available =
      width - awra_px(x->position == AWRA_DIVIDER_LABEL_CENTER ? 2 * LABEL_GAP : LABEL_GAP);
  lw = BK_CLAMP(lw, lmin, BK_MAX(lmin, available));
  awra_measure(x->label, AWRA_ORIENTATION_VERTICAL, lw, &mn, &lh);
  float lx;
  if (x->position == AWRA_DIVIDER_LABEL_CENTER) {
    lx = (width - lw) / 2.0f;
  }
  else if (x->position == AWRA_DIVIDER_LABEL_START) {
    lx = 0;
  }
  else {
    lx = (float)(width - lw);
  }
  awra_allocate(x->label, w->x + (int)roundf(lx), w->y + pad + (int)roundf((height - lh) / 2.0f), lw, lh);
}

static AwraRGBA divider_color(AwraWidget *w)
{
  const AwraTokens *t = awra_tokens();
  switch (DVX(w)->appearance) {
    case AWRA_DIVIDER_ACCENT: return awra_with_alpha(t->accent, 0.55f);
    case AWRA_DIVIDER_DANGER: return t->danger;
    default: return t->high_contrast ? t->outline : t->separator;
  }
}

static void divider_draw(AwraWidget *w, AwraDraw *d)
{
  DividerExt *x = DVX(w);
  const AwraRGBA c = divider_color(w);
  float y = floorf(w->y + w->h / 2.0f);
  const float lw = awra_pxf(1);
  if (!divider_has_label(w)) {
    awra_paint_rect(d, (AwraBox){(float)w->x, y, (float)w->w, lw}, &c);
    return;
  }
  AwraWidget *l = x->label;
  y = floorf(l->y + l->h / 2.0f);
  const float gap = awra_pxf(LABEL_GAP);
  const float a = l->x - gap - w->x;
  if (a >= 1.0f) {
    awra_paint_rect(d, (AwraBox){(float)w->x, y, a, lw}, &c);
  }
  const float from = l->x + l->w + gap;
  if (w->x + w->w - from >= 1.0f) {
    awra_paint_rect(d, (AwraBox){from, y, w->x + w->w - from, lw}, &c);
  }
  if (x->appearance == AWRA_DIVIDER_DANGER) {
    /* the label is a pill on the danger colour */
    awra_paint_rbox(d, awra_box_of(l), l->h / 2.0f, &awra_tokens()->danger, 0, NULL);
  }
}

static bool divider_child_style(AwraWidget *w, AwraRGBA *c, int *weight)
{
  const AwraTokens *t = awra_tokens();
  switch (DVX(w)->appearance) {
    case AWRA_DIVIDER_ACCENT: *c = t->accent_content; *weight = 600; break;
    case AWRA_DIVIDER_DANGER: *c = t->on_accent; *weight = 700; break;
    default: *c = t->muted; *weight = 600; break;
  }
  return true;
}

/* a separator; with a label, a group named by it */
static void divider_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  if (divider_has_label(w)) {
    info->role = AWRA_ACCESSIBLE_ROLE_GROUP;
    info->label = awra_label_text(DVX(w)->label);
  }
}

static AwraClass divider_class = {
    .a11y = divider_a11y,
    .bk = AWRA_BK("AwraDivider", 0),
    .type = "AwraDivider",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(DividerExt),
    .measure = divider_measure,
    .allocate = divider_allocate,
    .draw = divider_draw,
    .child_style = divider_child_style,
};

static void divider_style_label(AwraWidget *w)
{
  DividerExt *x = DVX(w);
  if (!x->label) {
    return;
  }
  const bool danger = x->appearance == AWRA_DIVIDER_DANGER;
  awra_label_set_scale(x->label, danger ? 0.72 : 0.84);
  awra_label_set_weight(x->label, danger ? 700 : 600);
  awra_widget_set_margins(x->label, danger ? 1 : 0, danger ? 7 : 0, danger ? 1 : 0, danger ? 7 : 0);
}

AwraWidget *awra_divider_new(const char *label)
{
  AwraWidget *w = awra_widget_new(&divider_class);
  DividerExt *x = DVX(w);
  x->position = AWRA_DIVIDER_LABEL_CENTER;
  x->label = awra_label_new(label ? label : "");
  awra_widget_set_visible(x->label, label && label[0]);
  awra_child_insert(w, x->label, -1);
  awra_widget_unref(x->label);
  divider_style_label(w);
  return w;
}

void awra_divider_set_label(AwraWidget *w, const char *label)
{
  DividerExt *x = DVX(w);
  awra_label_set_text(x->label, label ? label : "");
  awra_widget_set_visible(x->label, label && label[0]);
  awra_widget_queue_resize(w);
}

void awra_divider_set_label_position(AwraWidget *w, AwraDividerLabelPosition position)
{
  DVX(w)->position = position;
  awra_widget_queue_resize(w);
}

void awra_divider_set_appearance(AwraWidget *w, AwraDividerAppearance appearance)
{
  DVX(w)->appearance = appearance;
  divider_style_label(w);
  awra_invalidate(w);
}

/* ======================================================================= */
/* Skeleton: lines of 10 px, 18 px apart, the last at 64 %, pulsing */

typedef struct SkeletonExt {
  int lines;
  bool animated;
  float pulse;
  unsigned timer;
} SkeletonExt;

#define SKX(w) AWRA_EXT(w, SkeletonExt)

static void skeleton_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)for_size;
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = awra_px(96);
    *r_nat = awra_px(320);
  }
  else {
    *r_min = *r_nat = awra_px(SKX(w)->lines * 18 - 8);
  }
}

static void skeleton_draw(AwraWidget *w, AwraDraw *d)
{
  SkeletonExt *x = SKX(w);
  AwraRGBA c = awra_tokens()->control_cap;
  c.a = fmaxf(c.a, 0.08f) * x->pulse;
  for (int i = 0; i < x->lines; i++) {
    const float lw = i + 1 == x->lines ? w->w * 0.64f : (float)w->w;
    awra_paint_rbox(d, (AwraBox){(float)w->x, w->y + awra_pxf(i * 18.0), lw, awra_pxf(10)}, awra_pxf(5), &c, 0, NULL);
  }
}

static bool skeleton_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  const double phase = fmod(awra_now_ms(), 1200.0) / 1200.0;
  SKX(w)->pulse = (float)(0.72 + 0.28 * (1.0 - fabs(phase * 2.0 - 1.0)));
  awra_invalidate(w);
  return true;
}

static void skeleton_update(AwraWidget *w)
{
  SkeletonExt *x = SKX(w);
  const bool run = x->animated && !awra_style_reduced_motion() && awra_aux(w)->mapped;
  if (run && !x->timer) {
    x->timer = awra_timeout_add(33, skeleton_tick, w, NULL);
  }
  else if (!run && x->timer) {
    awra_source_remove(x->timer);
    x->timer = 0;
    x->pulse = 1.0f;
    awra_invalidate(w);
  }
}

static void skeleton_map(AwraWidget *w, bool mapped)
{
  (void)mapped;
  skeleton_update(w);
}

static void skeleton_finalize(AwraWidget *w)
{
  if (SKX(w)->timer) {
    awra_source_remove(SKX(w)->timer);
  }
}

/* a placeholder while content loads: a busy group */
static void skeleton_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->label = "Loading";
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_BUSY, 1);
}

static AwraClass skeleton_class = {
    .a11y = skeleton_a11y,
    .bk = AWRA_BK("AwraSkeleton", 0),
    .type = "AwraSkeleton",
    .ext_size = sizeof(SkeletonExt),
    .finalize = skeleton_finalize,
    .measure = skeleton_measure,
    .draw = skeleton_draw,
    .map = skeleton_map,
};

AwraWidget *awra_skeleton_new(void)
{
  AwraWidget *w = awra_widget_new(&skeleton_class);
  SKX(w)->lines = 3;
  SKX(w)->animated = true;
  SKX(w)->pulse = 1.0f;
  return w;
}

void awra_skeleton_set_lines(AwraWidget *w, int lines)
{
  SKX(w)->lines = BK_CLAMP(lines, 1, 12);
  awra_widget_queue_resize(w);
}

void awra_skeleton_set_animated(AwraWidget *w, bool animated)
{
  SKX(w)->animated = animated;
  skeleton_update(w);
}

/* ======================================================================= */
/* Typing indicator: three dots in a wave, then a label; the same size
 * shown or hidden */

#define DOTS 3
#define WAVE_MS 1300.0
#define STAGGER 0.15
#define BUMP 0.48
#define SLIDE 4.0f

typedef struct TypingExt {
  AwraWidget *label;
  bool active;
  float reveal;
  double phase_start;
  unsigned timer;
} TypingExt;

#define TYX(w) AWRA_EXT(w, TypingExt)

static float typing_dot(AwraWidget *w)
{
  int mn, nt;
  awra_measure(TYX(w)->label, AWRA_ORIENTATION_VERTICAL, -1, &mn, &nt);
  return fmaxf(awra_pxf(4), roundf(mn * 0.34f));
}

static float typing_dots_width(AwraWidget *w)
{
  const float d = typing_dot(w);
  return DOTS * d + (DOTS - 1) * roundf(d * 0.55f);
}

static bool typing_label_shown(AwraWidget *w)
{
  const char *t = awra_label_text(TYX(w)->label);
  return t && t[0];
}

static void typing_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)for_size;
  int lm, ln;
  awra_measure(TYX(w)->label, o, -1, &lm, &ln);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    const bool shown = typing_label_shown(w);
    const float extra = shown ? awra_pxf(awra_spacing(AWRA_SPACING_SM)) : 0;
    *r_min = (int)ceilf(typing_dots_width(w) + extra + (shown ? lm : 0));
    *r_nat = (int)ceilf(typing_dots_width(w) + extra + (shown ? ln : 0));
  }
  else {
    *r_min = lm;
    *r_nat = ln;
  }
}

static void typing_allocate(AwraWidget *w)
{
  TypingExt *x = TYX(w);
  const int dx = (int)ceilf(typing_dots_width(w) + awra_pxf(awra_spacing(AWRA_SPACING_SM)));
  const int slide = (int)roundf(awra_pxf(SLIDE) * (1.0f - x->reveal));
  awra_allocate(x->label, w->x + dx, w->y + slide, BK_MAX(0, w->w - dx), w->h);
}

static void typing_draw(AwraWidget *w, AwraDraw *d)
{
  TypingExt *x = TYX(w);
  if (x->reveal <= 0.0f) {
    awra_aux(x->label)->opacity = 0.0f;
    return;
  }
  const float dia = typing_dot(w);
  const float spacing = roundf(dia * 0.55f);
  const bool still = x->timer == 0;
  const double phase = still ? 0.0 : fmod((awra_now_ms() - x->phase_start) / WAVE_MS, 1.0);
  const float slide = roundf(awra_pxf(SLIDE) * (1.0f - x->reveal));
  for (int i = 0; i < DOTS; i++) {
    const double local = fmod(phase - i * STAGGER + 1.0, 1.0);
    const double bump = local < BUMP ? (1.0 - cos(2.0 * M_PI * local / BUMP)) / 2.0 : 0.0;
    const float lift = still ? 0.0f : (float)(bump * dia * 0.6);
    const float cx = w->x + i * (dia + spacing) + dia / 2.0f;
    const float cy = w->y + slide + (w->h - dia) / 2.0f - lift + dia * 0.15f + dia / 2.0f;
    AwraRGBA c = awra_with_alpha(awra_tokens()->muted,
                                 awra_tokens()->muted.a * x->reveal * (still ? 0.75f : (float)(0.45 + 0.55 * bump)));
    awra_paint_circle(d, cx, cy, dia / 2.0f, &c);
  }
  awra_aux(x->label)->opacity = x->reveal;
}

static bool typing_tick(void *data)
{
  awra_invalidate((AwraWidget *)data);
  return true;
}

static void typing_update(AwraWidget *w)
{
  TypingExt *x = TYX(w);
  const bool run = x->active && awra_aux(w)->mapped && !awra_style_reduced_motion();
  if (run && !x->timer) {
    x->phase_start = awra_now_ms();
    x->timer = awra_timeout_add(33, typing_tick, w, NULL);
  }
  else if (!run && x->timer) {
    awra_source_remove(x->timer);
    x->timer = 0;
  }
}

static void typing_map(AwraWidget *w, bool mapped)
{
  (void)mapped;
  typing_update(w);
}

static void typing_finalize(AwraWidget *w)
{
  if (TYX(w)->timer) {
    awra_source_remove(TYX(w)->timer);
  }
}

/* Awra 1: a status named by its text, hidden while nobody types */
static void typing_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  TypingExt *x = TYX(w);
  info->label = x->label ? awra_label_text(x->label) : NULL;
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_HIDDEN, !x->active);
}

static AwraClass typing_class = {
    .a11y = typing_a11y,
    .bk = AWRA_BK("AwraTypingIndicator", 0),
    .type = "AwraTypingIndicator",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(TypingExt),
    .finalize = typing_finalize,
    .measure = typing_measure,
    .allocate = typing_allocate,
    .draw = typing_draw,
    .map = typing_map,
};

AwraWidget *awra_typing_indicator_new(void)
{
  AwraWidget *w = awra_widget_new(&typing_class);
  TypingExt *x = TYX(w);
  x->label = awra_label_new("");
  awra_label_set_ellipsize(x->label, AWRA_ELLIPSIZE_END);
  awra_label_set_xalign(x->label, 0.0f);
  awra_widget_set_typography(x->label, AWRA_TYPOGRAPHY_MUTED);
  awra_child_insert(w, x->label, -1);
  awra_widget_unref(x->label);
  awra_aux(x->label)->opacity = 0.0f;
  awra_widget_set_can_target(w, false);
  return w;
}

void awra_typing_indicator_set_label(AwraWidget *w, const char *label)
{
  awra_label_set_text(TYX(w)->label, label ? label : "");
  awra_widget_queue_resize(w);
}

static void typing_reveal_anim(AwraWidget *w, double t, void *data)
{
  const float from = (float)(intptr_t)data / 1000.0f;
  TypingExt *x = TYX(w);
  x->reveal = from + ((x->active ? 1.0f : 0.0f) - from) * (float)t;
  awra_queue_allocate(w);
  awra_invalidate(w);
}

void awra_typing_indicator_set_active(AwraWidget *w, bool active)
{
  TypingExt *x = TYX(w);
  if (x->active == active) {
    return;
  }
  x->active = active;
  typing_update(w);
  if (!awra_aux(w)->mapped || awra_style_reduced_motion()) {
    x->reveal = active ? 1.0f : 0.0f;
    awra_queue_allocate(w);
    awra_invalidate(w);
    return;
  }
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_FAST), typing_reveal_anim,
                  (void *)(intptr_t)(x->reveal * 1000.0f), NULL);
}

/* ======================================================================= */
/* Badge group: badges flowing 6 px apart; metadata group: a title and rows
 * of a label and a value */

AwraWidget *awra_badge_group_new(void)
{
  AwraWidget *w = awra_flow_box_new();
  awra_flow_box_set_selection_mode(w, AWRA_SELECTION_NONE);
  awra_flow_box_set_spacing(w, 6, 6);
  awra_widget_set_name(w, "AwraBadgeGroup");
  return w;
}

void awra_badge_group_append(AwraWidget *group, AwraWidget *badge)
{
  awra_flow_box_append(group, badge);
}

void awra_badge_group_remove_all(AwraWidget *group)
{
  awra_container_remove_all(group);
}

AwraWidget *awra_metric_row_new(const char *label, const char *value)
{
  AwraWidget *row = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, awra_spacing(AWRA_SPACING_SM));
  AwraWidget *icon = awra_image_new();
  awra_widget_set_visible(icon, false);
  AwraWidget *l = awra_label_new(label ? label : "");
  awra_widget_set_hexpand(l, true);
  awra_label_set_xalign(l, 0.0f);
  awra_label_set_ellipsize(l, AWRA_ELLIPSIZE_END);
  awra_widget_set_typography(l, AWRA_TYPOGRAPHY_BODY);
  AwraWidget *v = awra_label_new(value ? value : "");
  awra_label_set_xalign(v, 1.0f);
  awra_label_set_ellipsize(v, AWRA_ELLIPSIZE_END);
  awra_label_set_max_width_chars(v, 32);
  awra_widget_set_typography(v, AWRA_TYPOGRAPHY_MONOSPACE);
  awra_widget_set_tooltip(v, value);
  awra_box_append(row, icon);
  awra_box_append(row, l);
  awra_box_append(row, v);
  awra_widget_unref(icon);
  awra_widget_unref(l);
  awra_widget_unref(v);
  awra_widget_set_name(row, "AwraMetricRow");
  awra_widget_set_size_request(row, -1, 30); /* awra.css: awrametricrow { min-height: 30px } */
  return row;
}

AwraWidget *awra_metadata_group_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_VERTICAL, awra_spacing(AWRA_SPACING_SM));
  AwraWidget *title = awra_label_new("");
  awra_widget_set_typography(title, AWRA_TYPOGRAPHY_TITLE_3);
  awra_label_set_xalign(title, 0.0f);
  awra_widget_set_visible(title, false);
  awra_box_append(w, title);
  awra_widget_unref(title);
  awra_widget_set_name(w, "AwraMetadataGroup");
  return w;
}

void awra_metadata_group_set_title(AwraWidget *w, const char *title)
{
  AwraWidget *l = w->kids[0];
  awra_label_set_text(l, title ? title : "");
  awra_widget_set_visible(l, title && title[0]);
}

AwraWidget *awra_metadata_group_append(AwraWidget *w, const char *label, const char *value)
{
  AwraWidget *row = awra_metric_row_new(label, value);
  awra_box_append(w, row);
  awra_widget_unref(row);
  /* its labels take the group's roles (prose-metadata) now they are in it */
  for (int i = 0; i < row->nkids; i++) {
    AwraWidget *k = row->kids[i];
    if (awra_is_widget(k) && AWRA_CLASS_OF(k)->style_changed) {
      AWRA_CLASS_OF(k)->style_changed(k);
    }
  }
  return row;
}
