/* Awra 2 buttons: AwraButton (Primary, Secondary, Ghost, Destructive,
 * Toolbar, Success), AwraToggleButton, and the button looks other
 * components reuse (awra.css L1110-1323, L3594-3627).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

/* ======================================================================= */
/* Looks */

void awra_button_colors(AwraLook look, bool hover, bool pressed, bool checked, bool sensitive,
                        AwraButtonColors *r)
{
  const AwraTokens *t = awra_tokens();
  const AwraRGBA none = {0, 0, 0, 0};
  memset(r, 0, sizeof(*r));
  r->fg = t->foreground;
  r->bg = none;
  r->rim = none;
  r->ring = none;
  const AwraRGBA white20 = {1, 1, 1, 0.20f};
  switch (look) {
    case AWRA_LOOK_SECONDARY:
      r->bg = t->control_cap;
      r->rim = t->rim;
      if (hover) {
        r->bg = t->control_cap_hover;
      }
      if (pressed) {
        r->bg = t->control_pressed;
        r->rim = none;
        r->pressed_shadow = 0.18f;
      }
      break;
    case AWRA_LOOK_PRIMARY:
      r->fg = t->on_accent;
      r->bg = t->accent;
      r->rim = white20;
      if (hover) {
        r->rim = awra_with_alpha(t->on_accent, 0.10f);
        r->ring = awra_with_alpha(t->on_accent, 0.20f);
      }
      if (pressed || checked) {
        r->rim = none;
        r->ring = awra_with_alpha(t->on_accent, 0.12f);
        r->pressed_shadow = 0.28f;
      }
      break;
    case AWRA_LOOK_GHOST:
    case AWRA_LOOK_TOOLBAR:
      if (hover) {
        r->bg = t->control_cap_hover;
        r->rim = t->rim;
      }
      if (pressed) {
        r->bg = t->control_pressed;
        r->rim = none;
        r->pressed_shadow = 0.16f;
      }
      break;
    case AWRA_LOOK_DESTRUCTIVE:
      r->fg = t->on_accent;
      r->bg = t->danger;
      r->rim = t->rim;
      if (pressed) {
        r->rim = none;
        r->pressed_shadow = 0.18f;
      }
      break;
    case AWRA_LOOK_SUCCESS:
      r->fg = awra_rgba(1, 1, 1, 1);
      r->bg = t->success;
      r->rim = awra_rgba(1, 1, 1, 0.18f);
      if (hover) {
        r->bg = awra_shade(t->success, 1.08f);
      }
      if (pressed) {
        r->bg = awra_shade(t->success, 0.9f);
      }
      return; /* no disabled look (awra.css L1242) */
    case AWRA_LOOK_SEGMENT:
      if (hover) {
        r->bg = t->control_cap;
      }
      if (pressed) {
        r->bg = t->control_pressed;
        r->pressed_shadow = 0.18f;
      }
      break;
    case AWRA_LOOK_WINDOW_CONTROL:
      if (hover) {
        r->bg = t->control_cap_hover;
      }
      if (pressed) {
        r->bg = t->control_pressed;
      }
      break;
    case AWRA_LOOK_NAV_ITEM:
      r->fg = t->muted;
      if (hover) {
        r->fg = t->foreground;
        r->bg = t->control_cap_hover;
      }
      if (checked) {
        r->fg = t->foreground;
        r->bg = t->control_cap;
        r->rim = t->rim;
      }
      if (pressed) {
        r->bg = t->control_pressed;
      }
      break;
    case AWRA_LOOK_ACTION_ROW:
    case AWRA_LOOK_MENU_ROW:
    case AWRA_LOOK_ROW_ACTION:
    case AWRA_LOOK_COLLECTION_TILE:
      if (hover) {
        r->bg = t->control_cap_hover;
      }
      if (pressed || (checked && look == AWRA_LOOK_ROW_ACTION)) {
        r->bg = t->control_pressed;
      }
      break;
    case AWRA_LOOK_INLINE_ACTION:
      /* quiet inside content: muted at rest */
      r->fg = t->muted;
      if (hover) {
        r->fg = t->foreground;
        r->bg = t->control_cap_hover;
        r->rim = t->rim;
      }
      if (pressed || checked) {
        r->fg = t->foreground;
        r->bg = t->control_pressed;
        r->rim = none;
      }
      break;
    case AWRA_LOOK_SIDEBAR_HEADING:
      /* only its colour answers the pointer */
      r->fg = hover ? t->foreground : t->muted;
      return;
    case AWRA_LOOK_SEARCH_LAUNCHER:
      r->fg = t->muted;
      r->bg = t->control_cap_hover;
      r->ring = awra_with_alpha(t->foreground, 0.06f);
      if (hover) {
        r->fg = t->foreground;
      }
      break;
    case AWRA_LOOK_CARD_ACTION:
      r->bg = awra_with_alpha(t->foreground, 0.045f);
      r->rim = t->rim;
      if (hover) {
        r->bg = awra_with_alpha(t->foreground, 0.075f);
      }
      if (pressed) {
        r->bg = t->control_pressed;
      }
      break;
  }
  /* checked toggles (L1298): accent-soft with the accent text */
  if (checked && (look == AWRA_LOOK_SECONDARY || look == AWRA_LOOK_GHOST ||
                  look == AWRA_LOOK_TOOLBAR || look == AWRA_LOOK_SEGMENT))
  {
    r->fg = t->accent_content;
    r->bg = t->accent_soft;
    r->rim = t->rim;
    r->pressed_shadow = 0.0f;
  }
  if (!sensitive) {
    r->fg = t->disabled;
    if (look == AWRA_LOOK_PRIMARY || look == AWRA_LOOK_DESTRUCTIVE ||
        look == AWRA_LOOK_SECONDARY)
    {
      r->bg = t->control_cap;
    }
    if (look == AWRA_LOOK_GHOST || look == AWRA_LOOK_TOOLBAR || look == AWRA_LOOK_NAV_ITEM) {
      r->bg = none;
    }
    r->rim = none;
    r->ring = none;
    r->pressed_shadow = 0.0f;
  }
}

static AwraRGBA lerp_rgba(AwraRGBA a, AwraRGBA b, float t)
{
  /* transitions interpolate premultiplied, like GTK's color transitions */
  return awra_mix(a, b, t);
}

void awra_paint_button_bg(AwraDraw *d, AwraBox b, float radius, const AwraButtonColors *c,
                          float press_t, bool focus_ring)
{
  /* transform: scale(.96) around the center while pressed */
  if (press_t > 0.0f) {
    const float s = 1.0f - 0.04f * press_t;
    const float nw = b.w * s, nh = b.h * s;
    b.x += (b.w - nw) * 0.5f;
    b.y += (b.h - nh) * 0.5f;
    b.w = nw;
    b.h = nh;
    radius *= s;
  }
  awra_paint_rbox(d, b, radius, &c->bg, 0, NULL);
  if (c->ring.a > 0) {
    awra_paint_rbox(d, b, radius, NULL, awra_pxf(1), &c->ring);
  }
  if (c->rim.a > 0) { /* inset 0 1px rim */
    awra_paint_inset_shadow(d, b, radius, 0, awra_pxf(1), 0, 0, &c->rim);
  }
  if (c->pressed_shadow > 0) { /* inset 0 1px 2px rgba(10,8,18,a) */
    const AwraRGBA dark = {10 / 255.0f, 8 / 255.0f, 18 / 255.0f, c->pressed_shadow};
    awra_paint_inset_shadow(d, b, radius, 0, awra_pxf(1), 0, awra_pxf(2), &dark);
  }
  if (focus_ring) {
    /* outline: 3px solid @awra_focus; outline-offset: 2px */
    const float w = awra_pxf(3), off = awra_pxf(2);
    AwraBox o = {b.x - off - w, b.y - off - w, b.w + 2 * (off + w), b.h + 2 * (off + w)};
    awra_paint_rbox(d, o, radius + off + w, NULL, w, &awra_tokens()->focus_ring);
  }
}

/* ======================================================================= */
/* AwraButton */

typedef struct ButtonExt {
  AwraButtonAppearance appearance;
  int look; /* AwraLook */
  char *label;
  char *icon;
  AwraWidget *child;
  bool frame;
  bool toggle, active;
  AwraWidget *group_next; /* toggle group ring */
  AwraTextLayout *layout;
  unsigned style_gen;
  AwraButtonColors colors, from;
  float color_t;
  int pad_x, pad_y, min_w; /* > 0: overrides (dropdowns) */
} ButtonExt;

#define BX(w) AWRA_EXT(w, ButtonExt)

static AwraLook look_of(AwraButtonAppearance a)
{
  switch (a) {
    case AWRA_BUTTON_PRIMARY:
      return AWRA_LOOK_PRIMARY;
    case AWRA_BUTTON_GHOST:
      return AWRA_LOOK_GHOST;
    case AWRA_BUTTON_DESTRUCTIVE:
      return AWRA_LOOK_DESTRUCTIVE;
    case AWRA_BUTTON_TOOLBAR:
      return AWRA_LOOK_TOOLBAR;
    case AWRA_BUTTON_SECONDARY:
    default:
      return AWRA_LOOK_SECONDARY;
  }
}

static void button_init(AwraWidget *w)
{
  ButtonExt *x = BX(w);
  x->appearance = AWRA_BUTTON_SECONDARY;
  x->look = AWRA_LOOK_SECONDARY;
  x->frame = true;
  x->group_next = w;
  w->flags |= NF_EVENTS;
}

static void button_finalize(AwraWidget *w)
{
  ButtonExt *x = BX(w);
  /* leave the toggle group ring */
  if (x->group_next && x->group_next != w) {
    AwraWidget *p = x->group_next;
    while (BX(p)->group_next != w) {
      p = BX(p)->group_next;
    }
    BX(p)->group_next = x->group_next;
  }
  free(x->label);
  free(x->icon);
  awra_text_layout_free(x->layout);
}

static bool icon_only(const AwraWidget *w)
{
  const ButtonExt *x = BX(w);
  return x->icon && !(x->label && x->label[0]) && !x->child;
}

static bool is_toolbar(const AwraWidget *w)
{
  return BX(w)->look == AWRA_LOOK_TOOLBAR;
}

/* the look of a style role (Awra 1's add_style_role on a button), else the
 * appearance's */
static int role_look(const AwraWidget *w)
{
  enum { N = 6 };
  static int bits[N] = {-2};
  static const char *const names[N] = {"inline-action", "row-action", "search-launcher",
                                       "collection-tile", "card-action", "sidebar-heading"};
  static const int looks[N] = {AWRA_LOOK_INLINE_ACTION, AWRA_LOOK_ROW_ACTION,
                               AWRA_LOOK_SEARCH_LAUNCHER, AWRA_LOOK_COLLECTION_TILE,
                               AWRA_LOOK_CARD_ACTION, AWRA_LOOK_SIDEBAR_HEADING};
  if (bits[0] == -2) {
    for (int i = 0; i < N; i++) {
      bits[i] = awra_class_bit(names[i]);
    }
  }
  for (int i = 0; i < N; i++) {
    if (bits[i] >= 0 && awra_has_class_bit(w, bits[i])) {
      return looks[i];
    }
  }
  return -1;
}

static int eff_look(const AwraWidget *w)
{
  const int r = role_look(w);
  return r >= 0 ? r : BX(w)->look;
}

/* padding (logical) and radius per variant (spec B §1.1) */
static void geometry(const AwraWidget *w, float *pad_x, float *pad_y, float *min_w, float *min_h,
                     float *radius)
{
  const bool tb = is_toolbar(w), io = icon_only(w);
  switch (role_look(w)) {
    case AWRA_LOOK_INLINE_ACTION:
      /* an Awra toolbar button: 2 px; any other: 3 px 6 px */
      *pad_x = tb ? 2 : 6;
      *pad_y = tb ? 2 : 3;
      *min_w = *min_h = 18;
      *radius = tb ? 6 : 7;
      return;
    case AWRA_LOOK_ROW_ACTION:
      *pad_x = 8;
      *pad_y = 6;
      *min_w = *min_h = 0;
      *radius = 8;
      return;
    case AWRA_LOOK_SEARCH_LAUNCHER:
      *pad_x = 12;
      *pad_y = 5;
      *min_w = *min_h = 0;
      *radius = 999;
      return;
    case AWRA_LOOK_COLLECTION_TILE:
      *pad_x = *pad_y = 4;
      *min_w = *min_h = 0;
      *radius = 12;
      return;
    case AWRA_LOOK_CARD_ACTION:
      *pad_x = *pad_y = 16;
      *min_w = *min_h = 0;
      *radius = 12;
      return;
    case AWRA_LOOK_SIDEBAR_HEADING:
      *pad_x = 8;
      *pad_y = 4;
      *min_w = *min_h = 0;
      *radius = 6;
      return;
    default:
      break;
  }
  if (BX(w)->look == AWRA_LOOK_WINDOW_CONTROL) {
    *pad_x = *pad_y = 8;
    *min_w = *min_h = 16;
    *radius = 8;
    return;
  }
  if (BX(w)->look == AWRA_LOOK_SEGMENT) {
    *pad_x = io ? 5 : 12;
    *pad_y = 5;
    *min_w = *min_h = 24;
    *radius = 10;
    return;
  }
  if (io) {
    *pad_x = *pad_y = tb ? 5 : 6;
    *min_w = *min_h = 22;
  }
  else if (tb) {
    *pad_x = 8;
    *pad_y = 5;
    *min_w = 24;
    *min_h = 24;
  }
  else {
    *pad_x = 13;
    *pad_y = 6;
    *min_w = 24;
    *min_h = 22;
  }
  *radius = tb ? 10 : 11;
  if (tb && awra_in_compact_header(w)) {
    *pad_x = 8;
    *pad_y = 5;
    *min_w = *min_h = 22;
    *radius = 8;
  }
  if (BX(w)->pad_x > 0) {
    *pad_x = BX(w)->pad_x;
    *pad_y = BX(w)->pad_y;
    *min_w = BX(w)->min_w;
  }
  if (awra_has_class_bit(w, AWRA_CB_CIRCULAR) || awra_has_class_bit(w, AWRA_CB_PILL)) {
    *radius = 999;
  }
  if (awra_has_class_bit(w, AWRA_CB_COMPACT)) {
    *pad_y = 3;
    *min_h = 20;
  }
}

static AwraFont button_font(AwraWidget *w)
{
  AwraFont f = awra_font_for(awra_widget_typography(w));
  if (BX(w)->look == AWRA_LOOK_PRIMARY) {
    f.weight = 640;
  }
  return f;
}

static void button_sync(AwraWidget *w)
{
  ButtonExt *x = BX(w);
  if (!x->label) {
    return;
  }
  if (!x->layout) {
    x->layout = awra_text_layout_new();
  }
  AwraFont f = button_font(w);
  awra_text_layout_set_font(x->layout, &f);
  awra_text_layout_set_base_dir(x->layout, awra_widget_rtl(w));
  awra_text_layout_set_text(x->layout, x->label, false);
  awra_text_layout_set_wrap(x->layout, AWRA_WRAP_NONE, AWRA_ELLIPSIZE_END, 1);
  x->style_gen = awra_style_generation();
}

static void content_size(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  ButtonExt *x = BX(w);
  if (x->child) {
    awra_measure(x->child, o, for_size, r_min, r_nat);
    return;
  }
  const int icon = x->icon ? awra_px(16) : 0;
  int tmin = 0, tnat = 0, th = 0;
  if (x->label && x->label[0]) {
    button_sync(w);
    tmin = awra_text_layout_min_width(x->layout);
    tnat = awra_text_layout_natural_width(x->layout);
    awra_text_layout_size(x->layout, -1, NULL, &th);
  }
  const int gap = (icon && tnat) ? awra_px(6) : 0;
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    /* label only: GtkButton's plain label, never ellipsized; icon and
     * label: AwraButtonContent's, which ellipsizes */
    *r_min = icon + gap + (tnat ? (icon ? tmin : tnat) : 0);
    *r_nat = icon + gap + tnat;
  }
  else {
    *r_min = *r_nat = BK_MAX(icon, th);
  }
}

static void button_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  float px, py, mw, mh, rad;
  geometry(w, &px, &py, &mw, &mh, &rad);
  const int pad = o == AWRA_ORIENTATION_HORIZONTAL ? awra_px(px) * 2 : awra_px(py) * 2;
  const int floor_ = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? mw : mh);
  int cm, cn;
  const int inner_for = for_size >= 0 ? BK_MAX(0, for_size - (o == AWRA_ORIENTATION_HORIZONTAL ?
                                                                  awra_px(py) * 2 :
                                                                  awra_px(px) * 2)) :
                                        -1;
  content_size(w, o, inner_for, &cm, &cn);
  *r_min = BK_MAX(cm, floor_) + pad;
  *r_nat = BK_MAX(cn, floor_) + pad;
}

static void button_allocate(AwraWidget *w)
{
  ButtonExt *x = BX(w);
  if (!x->child) {
    return;
  }
  float px, py, mw, mh, rad;
  geometry(w, &px, &py, &mw, &mh, &rad);
  const int ipx = awra_px(px), ipy = awra_px(py);
  awra_allocate(x->child, w->x + ipx, w->y + ipy, BK_MAX(0, w->w - 2 * ipx),
                BK_MAX(0, w->h - 2 * ipy));
}

static void current_colors(AwraWidget *w, AwraButtonColors *c)
{
  ButtonExt *x = BX(w);
  const float ht = awra_hover_t(w), pt = awra_press_t(w);
  const bool sens = awra_sensitive(w);
  const bool checked = x->toggle && x->active;
  AwraButtonColors rest, hov, prs;
  const AwraLook look = (AwraLook)eff_look(w);
  awra_button_colors(look, false, false, checked, sens, &rest);
  if (ht <= 0.0f && pt <= 0.0f) {
    *c = rest;
  }
  else {
    awra_button_colors(look, true, false, checked, sens, &hov);
    awra_button_colors(look, true, true, checked, sens, &prs);
    c->fg = lerp_rgba(lerp_rgba(rest.fg, hov.fg, ht), prs.fg, pt);
    c->bg = lerp_rgba(lerp_rgba(rest.bg, hov.bg, ht), prs.bg, pt);
    c->rim = lerp_rgba(lerp_rgba(rest.rim, hov.rim, ht), prs.rim, pt);
    c->ring = lerp_rgba(lerp_rgba(rest.ring, hov.ring, ht), prs.ring, pt);
    c->pressed_shadow = rest.pressed_shadow + (prs.pressed_shadow - rest.pressed_shadow) * pt;
  }
  if (!x->frame && !(ht > 0 || pt > 0 || checked)) {
    c->bg.a = 0;
    c->rim.a = 0;
  }
}

static void button_draw(AwraWidget *w, AwraDraw *d)
{
  ButtonExt *x = BX(w);
  float px, py, mw, mh, rad;
  geometry(w, &px, &py, &mw, &mh, &rad);
  AwraButtonColors c;
  current_colors(w, &c);
  const bool reduced = awra_style_reduced_motion();
  awra_paint_button_bg(d, awra_box_of(w), awra_pxf(rad), &c, reduced ? 0.0f : awra_press_t(w),
                       awra_focus_visible(w));
  x->colors = c;
  if (x->child) {
    return; /* the child draws, with this button's color (child_style) */
  }
  if (x->style_gen != awra_style_generation()) {
    button_sync(w);
  }
  const int icon = x->icon ? awra_px(16) : 0;
  int tw = 0, th = 0;
  if (x->label && x->label[0]) {
    awra_text_layout_size(x->layout, -1, &tw, &th);
  }
  const int gap = (icon && tw) ? awra_px(6) : 0;
  const int ipx = awra_px(px);
  const int avail = w->w - 2 * ipx;
  const int text_w = BK_MIN(tw, BK_MAX(0, avail - icon - gap));
  const int total = icon + gap + text_w;
  /* GtkLabel / GtkImage place by floor (gtk_label_get_layout_location) */
  float cx = (float)w->x + floorf((w->w - total) * 0.5f);
  /* the icon starts the button's box (at the right, right to left) */
  if (icon) {
    awra_paint_icon(d, x->icon, awra_mirror_x(w, cx, (float)icon),
                    (float)w->y + floorf((w->h - icon) * 0.5f), icon, &c.fg);
    cx += icon + gap;
  }
  if (tw) {
    if (text_w < tw) {
      awra_text_layout_size(x->layout, text_w, NULL, &th);
    }
    awra_text_layout_draw(x->layout, d, awra_mirror_x(w, cx, (float)text_w),
                          (float)w->y + floorf((w->h - th) * 0.5f), text_w < tw ? text_w : -1,
                          &c.fg);
  }
}

static bool button_child_style(AwraWidget *w, AwraRGBA *r_color, int *r_weight)
{
  AwraButtonColors c;
  current_colors(w, &c);
  *r_color = c.fg;
  *r_weight = BX(w)->look == AWRA_LOOK_PRIMARY ? 640 : 0;
  return true;
}

/* awra.css: `.awra-button label { color: inherit }` (and the menu
 * button's) outranks a typography's colour (`.awra-muted`...): a muted or
 * monospace label in an Awra button takes the button's colour; on plain
 * buttons, inline and row actions keep muted labels muted, search launchers
 * pass their colour to every label, other roles to none. */
extern AwraClass awra_button_class;

bool awra_button_label_inherits(const AwraWidget *w, AwraTypography t)
{
  if (AWRA_CLASS_OF(w) != &awra_button_class) {
    return false;
  }
  switch (role_look(w)) {
    case -1:
      break;
    case AWRA_LOOK_INLINE_ACTION:
    case AWRA_LOOK_ROW_ACTION:
      return t != AWRA_TYPOGRAPHY_MUTED;
    case AWRA_LOOK_SEARCH_LAUNCHER:
      return true;
    default:
      return false;
  }
  switch (BX(w)->look) {
    case AWRA_LOOK_SECONDARY:
    case AWRA_LOOK_PRIMARY:
    case AWRA_LOOK_GHOST:
    case AWRA_LOOK_DESTRUCTIVE:
    case AWRA_LOOK_TOOLBAR:
    case AWRA_LOOK_SUCCESS:
      return true;
    default:
      return false;
  }
}

static void toggle_set(AwraWidget *w, bool active, bool notify);

static void button_activate(AwraWidget *w)
{
  ButtonExt *x = BX(w);
  awra_widget_ref(w);
  if (x->toggle) {
    /* in a group, activating the active one keeps it active (radio) */
    if (x->group_next != w && x->active) {
      awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
      awra_widget_unref(w);
      return;
    }
    toggle_set(w, !x->active, true);
  }
  awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
  awra_widget_unref(w);
}

static int button_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside && awra_sensitive(w)) {
    button_activate(w);
  }
  return 1;
}

static int button_press(AwraWidget *w, BKEvent *e)
{
  return 1;
}

static void button_style_changed(AwraWidget *w)
{
  button_sync(w);
  /* the margins a role brings (awra.css), once */
  const int role = role_look(w);
  if (role >= 0 && !awra_widget_get_data(w, "awra-role-margins")) {
    awra_widget_set_data(w, "awra-role-margins", (void *)1, NULL);
    if (role == AWRA_LOOK_SIDEBAR_HEADING) {
      awra_widget_set_margins(w, 14, 8, 2, 8);
    }
    else if (role == AWRA_LOOK_ROW_ACTION) {
      awra_widget_set_margin_top(w, 1);
      awra_widget_set_margin_bottom(w, 1);
    }
  }
}

static bool button_set_string(AwraWidget *w, const char *name, const char *v)
{
  if (!strcmp(name, "label")) {
    awra_button_set_label(w, v);
    return true;
  }
  if (!strcmp(name, "icon-name")) {
    awra_button_set_icon_name(w, v);
    return true;
  }
  return false;
}

static const char *button_get_string(AwraWidget *w, const char *name)
{
  if (!strcmp(name, "label")) {
    return BX(w)->label;
  }
  if (!strcmp(name, "icon-name")) {
    return BX(w)->icon;
  }
  return NULL;
}

void awra_popover_owner_a11y(AwraWidget *w, AwraA11yInfo *info); /* popover.c */

/* GtkButton, GtkToggleButton (pressed), grouped toggles (radio buttons),
 * GtkMenuButton and GtkDropDown (has-popup, expanded) */
static void button_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  ButtonExt *x = BX(w);
  if (!x->child && x->label && x->label[0]) {
    info->label = x->label;
  }
  if (x->toggle) {
    if (x->group_next && x->group_next != w) {
      info->role = AWRA_ACCESSIBLE_ROLE_RADIO;
      awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_CHECKED, x->active);
    }
    else {
      info->role = AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON;
      awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_PRESSED, x->active);
    }
  }
  if (x->appearance == AWRA_BUTTON_PRIMARY) {
    info->flags |= AWRA_A11Y_DEFAULT;
  }
  awra_popover_owner_a11y(w, info);
}

AwraClass awra_button_class = {
    .a11y = button_a11y,
    .bk = AWRA_BK("AwraButton", CLS_INTERACTIVE),
    .type = "AwraButton",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE,
    .ext_size = sizeof(ButtonExt),
    .init = button_init,
    .finalize = button_finalize,
    .measure = button_measure,
    .allocate = button_allocate,
    .draw = button_draw,
    .press = button_press,
    .release = button_release,
    .activate = button_activate,
    .style_changed = button_style_changed,
    .set_string = button_set_string,
    .get_string = button_get_string,
    .child_style = button_child_style,
    .overdraw = 6,
};

AwraWidget *awra_button_new(void)
{
  return awra_widget_new(&awra_button_class);
}

AwraWidget *awra_button_new_with_label(const char *label)
{
  AwraWidget *w = awra_button_new();
  awra_button_set_label(w, label);
  return w;
}

AwraWidget *awra_button_new_from_icon_name(const char *icon)
{
  AwraWidget *w = awra_button_new();
  awra_button_set_icon_name(w, icon);
  return w;
}

static bool is_button(const AwraWidget *w)
{
  return awra_widget_is(w, "AwraButton");
}

void awra_button_set_label(AwraWidget *w, const char *label)
{
  if (!is_button(w)) {
    return;
  }
  ButtonExt *x = BX(w);
  /* gtk_button_set_label: the label replaces a child set before */
  if (x->child && label) {
    awra_set_slot(w, &x->child, NULL);
  }
  else if (x->label && label && !strcmp(x->label, label)) {
    return;
  }
  awra_set_str(&x->label, label);
  button_sync(w);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

const char *awra_button_label(AwraWidget *w)
{
  return is_button(w) ? BX(w)->label : NULL;
}

void awra_button_set_icon_name(AwraWidget *w, const char *icon)
{
  if (!is_button(w)) {
    return;
  }
  ButtonExt *x = BX(w);
  if (x->icon && icon && !strcmp(x->icon, icon)) {
    return;
  }
  awra_set_str(&x->icon, icon && icon[0] ? icon : NULL);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_button_set_child(AwraWidget *w, AwraWidget *child)
{
  if (!is_button(w)) {
    return;
  }
  awra_set_slot(w, &BX(w)->child, child);
}

void awra_button_set_appearance(AwraWidget *w, AwraButtonAppearance a)
{
  if (!is_button(w)) {
    return;
  }
  ButtonExt *x = BX(w);
  x->appearance = a;
  x->look = look_of(a);
  if (a == AWRA_BUTTON_SUCCESS) {
    x->look = AWRA_LOOK_SUCCESS;
  }
  button_sync(w);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

AwraButtonAppearance awra_button_appearance(AwraWidget *w)
{
  return is_button(w) ? BX(w)->appearance : AWRA_BUTTON_SECONDARY;
}

void awra_button_set_has_frame(AwraWidget *w, bool frame)
{
  if (is_button(w)) {
    BX(w)->frame = frame;
    awra_invalidate(w);
  }
}

/* internal: buttons with another look (segments, window controls) */
void awra_button_set_look(AwraWidget *w, int look);
void awra_button_set_look(AwraWidget *w, int look)
{
  BX(w)->look = look;
  awra_invalidate(w);
}

/* ======================================================================= */
/* AwraToggleButton: an AwraButton that stays down */

static void toggle_set(AwraWidget *w, bool active, bool notify)
{
  ButtonExt *x = BX(w);
  if (x->active == active) {
    return;
  }
  x->active = active;
  awra_invalidate(w);
  if (active) {
    for (AwraWidget *o = x->group_next; o && o != w; o = BX(o)->group_next) {
      if (BX(o)->active) {
        BX(o)->active = false;
        awra_invalidate(o);
        awra_emit_active(o, AWRA_SIGNAL_TOGGLED, false);
      }
    }
  }
  if (notify) {
    awra_emit_active(w, AWRA_SIGNAL_TOGGLED, active);
  }
}

AwraWidget *awra_toggle_button_new(void)
{
  AwraWidget *w = awra_button_new();
  BX(w)->toggle = true;
  awra_widget_set_data(w, "awra-type", (void *)"AwraToggleButton", NULL);
  return w;
}

AwraWidget *awra_toggle_button_new_with_label(const char *label)
{
  AwraWidget *w = awra_toggle_button_new();
  awra_button_set_label(w, label);
  return w;
}

void awra_toggle_button_set_active(AwraWidget *w, bool active)
{
  if (is_button(w)) {
    toggle_set(w, active, true);
  }
}

bool awra_toggle_button_active(AwraWidget *w)
{
  return is_button(w) && BX(w)->active;
}

void awra_toggle_button_set_group(AwraWidget *w, AwraWidget *member)
{
  if (!is_button(w) || !member || !is_button(member) || w == member) {
    return;
  }
  ButtonExt *x = BX(w);
  BX(w)->toggle = true;
  BX(member)->toggle = true;
  /* splice w into member's ring */
  x->group_next = BX(member)->group_next;
  BX(member)->group_next = w;
}

void awra_button_set_padding_internal(AwraWidget *w, int pad_x, int pad_y, int min_w)
{
  BX(w)->pad_x = pad_x;
  BX(w)->pad_y = pad_y;
  BX(w)->min_w = min_w;
  awra_widget_queue_resize(w);
}
