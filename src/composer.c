/* Awra 2 composer (Awra 1's AwraComposer) and completion popover (Awra 1's
 * AwraCompletionPopover).
 *
 * The composer is one well: a banner (replying, editing) and attachments
 * slide out above the row of a start widget, the text (it grows up to
 * max_lines), an end widget and the send button, which appears once there
 * is something to send. Return sends; Shift+Return breaks the line.
 *
 * The completion popover lists suggestions above the caret of a text view
 * it is attached to; Up/Down choose, Tab/Return take, Escape dismisses,
 * while the text keeps the keyboard.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#define GAP 6
#define TEXT_MARGIN_Y 7
#define TEXT_MARGIN_X 6
#define VISIBLE_ROWS 8
#define DEFAULT_ROW_HEIGHT 34
#define CARET_GAP 4
#define MIN_WIDTH 220
#define MAX_WIDTH 420

void awra_text_view_set_bare_internal(AwraWidget *w, int margin_x, int margin_y, int max_lines);
void awra_button_set_metrics_internal(AwraWidget *w, int pad_x, int pad_y, int min_w, int min_h);
void awra_button_set_icon_size_internal(AwraWidget *w, int px);
void awra_popover_set_padding_internal(AwraWidget *w, int pad, bool menu);

/* the popover attached to a text view, which takes its keys first */
static bool completion_active_for(AwraWidget *view);

/* ======================================================================= */
/* Composer */

typedef struct ComposerExt {
  AwraWidget *view, *send, *start, *end, *banner, *attachments;
  int max_lines;
  bool can_send, has_text, busy, send_on_enter;
  float send_reveal, banner_reveal, attach_reveal;
  float busy_angle;
  unsigned busy_timer;
} ComposerExt;

#define CPX(w) AWRA_EXT(w, ComposerExt)

static bool shown(AwraWidget *c)
{
  return c && awra_widget_visible(c);
}

static int nat_w(AwraWidget *c)
{
  if (!shown(c)) {
    return 0;
  }
  int mn, nt;
  awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
  return nt;
}

static int height_for(AwraWidget *c, int width)
{
  if (!shown(c)) {
    return 0;
  }
  int mn, nt;
  awra_measure(c, AWRA_ORIENTATION_VERTICAL, width, &mn, &nt);
  return nt;
}

static int gaps(AwraWidget *w)
{
  ComposerExt *x = CPX(w);
  return (shown(x->start) + shown(x->end)) * awra_px(GAP) + (int)roundf(x->send_reveal * awra_px(GAP));
}

static int field_width(AwraWidget *w, int width)
{
  ComposerExt *x = CPX(w);
  return BK_MAX(awra_px(32), width - nat_w(x->start) - nat_w(x->end) -
                                 (int)roundf(nat_w(x->send) * x->send_reveal) - gaps(w));
}

static int row_height(AwraWidget *w, int width)
{
  ComposerExt *x = CPX(w);
  int h = height_for(x->view, field_width(w, width));
  h = BK_MAX(h, height_for(x->start, nat_w(x->start)));
  h = BK_MAX(h, height_for(x->end, nat_w(x->end)));
  h = BK_MAX(h, height_for(x->send, -1));
  return h;
}

static float section_height(AwraWidget *c, float reveal, int width)
{
  if (!c || reveal <= 0.0f) {
    return 0.0f;
  }
  return (height_for(c, width) + awra_px(GAP)) * (float)awra_ease_out_cubic(reveal);
}

static void composer_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  ComposerExt *x = CPX(w);
  const int pad = awra_px(6) * 2;
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    int fm, fn;
    awra_measure(x->view, o, -1, &fm, &fn);
    int row = nat_w(x->start) + nat_w(x->end) + (int)roundf(nat_w(x->send) * x->send_reveal) + gaps(w);
    int mn = row + fm, nt = row + fn;
    AwraWidget *sec[2] = {x->banner, x->attachments};
    for (int i = 0; i < 2; i++) {
      if (sec[i]) {
        int sm, sn;
        awra_measure(sec[i], o, -1, &sm, &sn);
        mn = BK_MAX(mn, sm);
        nt = BK_MAX(nt, sn);
      }
    }
    *r_min = mn + pad;
    *r_nat = nt + pad;
    return;
  }
  const int width = for_size >= 0 ? for_size - pad : -1;
  const float h = row_height(w, width) + section_height(x->banner, x->banner_reveal, width) +
                  section_height(x->attachments, x->attach_reveal, width);
  *r_min = *r_nat = (int)ceilf(h) + pad;
}

/* Awra 1 centres the row's buttons in the band with a fractional offset
 * (a GskTransform of (band - height) / 2.0), which GTK draws on the pixel
 * row below the half: the half pixel rounds up. */
static int row_offset(int band, int height)
{
  return (int)floorf((float)(band - height) * 0.5f + 0.5f);
}

static void composer_allocate(AwraWidget *w)
{
  ComposerExt *x = CPX(w);
  const int p = awra_px(6);
  const int ox = w->x + p, oy = w->y + p, width = w->w - 2 * p, height = w->h - 2 * p;
  float y = 0;
  AwraWidget *sec[2] = {x->banner, x->attachments};
  float rev[2] = {x->banner_reveal, x->attach_reveal};
  for (int i = 0; i < 2; i++) {
    if (!sec[i]) {
      continue;
    }
    const int h = height_for(sec[i], width);
    const float region = rev[i] <= 0.0f ? 0.0f : (h + awra_px(GAP)) * (float)awra_ease_out_cubic(rev[i]);
    /* the section slides out from under the part below it */
    awra_allocate(sec[i], ox, oy + (int)(y + region - awra_px(GAP) - h), width, h);
    awra_widget_set_child_visible(sec[i], rev[i] > 0.0f);
    awra_aux(sec[i])->opacity = (float)awra_ease_out_cubic(rev[i]);
    y += region;
  }
  const int fw = field_width(w, width);
  const int fh = height_for(x->view, fw);
  const int line = height_for(x->send, -1);
  const int band = BK_MIN(BK_MAX(awra_px(TEXT_MARGIN_Y) * 2 + awra_px(17), line), BK_MAX(fh, line));
  const int bottom = oy + height;
  int cx = ox;
#define PLACE(c, cw)                                                                          \
  do {                                                                                        \
    if (shown(c) && (cw) > 0) {                                                               \
      const int ch = height_for((c), (cw));                                                   \
      awra_allocate((c), cx, bottom - band + row_offset(band, ch), (cw), ch);                 \
      cx += (cw) + awra_px(GAP);                                                              \
    }                                                                                         \
  } while (0)
  PLACE(x->start, nat_w(x->start));
  awra_allocate(x->view, cx, bottom - fh, fw, fh);
  cx += fw + awra_px(GAP);
  PLACE(x->end, nat_w(x->end));
#undef PLACE
  const int sw = nat_w(x->send);
  const int sh = height_for(x->send, -1);
  awra_allocate(x->send, ox + width - sw, bottom - band + row_offset(band, sh), sw, sh);
  awra_aux(x->send)->opacity = x->send_reveal;
}

static void composer_draw(AwraWidget *w, AwraDraw *d)
{
  const AwraTokens *t = awra_tokens();
  const AwraBox b = awra_box_of(w);
  const float r = awra_pxf(18), s = awra_scale();
  /* awracomposer:focus-within: the field or one of its buttons */
  const bool focus = awra_widget_focus_within(w);
  /* awra.css: background-color @awra_control (the control cap), not a well */
  if (!awra_sensitive(w)) {
    const AwraRGBA bg = awra_with_alpha(t->control_cap, t->control_cap.a * 0.5f);
    const AwraRGBA br = awra_with_alpha(t->border, t->border.a * 0.6f);
    awra_paint_rbox(d, b, r, &bg, s, &br);
    return;
  }
  if (focus) {
    /* 0 0 0 3px alpha(focus, .16) outside, inset 0 0 0 1px focus */
    const AwraRGBA glow = awra_with_alpha(t->focus_ring, 0.16f);
    awra_paint_rbox(d, (AwraBox){b.x - 3 * s, b.y - 3 * s, b.w + 6 * s, b.h + 6 * s}, r + 3 * s, NULL, 3 * s, &glow);
    awra_paint_rbox(d, b, r, &t->control_cap, s, &t->focus_ring);
    return;
  }
  /* box-shadow: inset 0 0 0 1px @awra_border, inset 0 1px @awra_rim, the
   * rim drawn over the border as GSK draws inset shadows in turn */
  awra_paint_rbox(d, b, r, &t->control_cap, 0, NULL);
  awra_paint_inset_shadow(d, b, r, 0, 0, s, 0, &t->border);
  awra_paint_inset_shadow(d, b, r, 0, s, 0, 0, &t->rim);
}

static void composer_draw_over(AwraWidget *w, AwraDraw *d)
{
  ComposerExt *x = CPX(w);
  if (!x->busy || x->send_reveal < 1.0f) {
    return;
  }
  /* a 2 px arc turning on the send button */
  AwraWidget *s = x->send;
  const float cx = s->x + s->w / 2.0f, cy = s->y + s->h / 2.0f;
  const float r = fminf((float)s->w, (float)s->h) / 2.0f - awra_pxf(3);
  if (r <= awra_pxf(2)) {
    return;
  }
  const AwraRGBA c = awra_tokens()->on_accent;
  const double start = x->busy_angle * M_PI / 180.0, sweep = M_PI * 0.6;
  const int n = 16;
  for (int i = 0; i < n; i++) {
    const double a0 = start + sweep * i / n, a1 = start + sweep * (i + 1) / n;
    awra_paint_line(d, cx + r * (float)cos(a0), cy + r * (float)sin(a0), cx + r * (float)cos(a1),
                    cy + r * (float)sin(a1), awra_pxf(2), &c);
  }
}

static bool composer_child_style(AwraWidget *w, AwraRGBA *c, int *weight)
{
  (void)w;
  (void)c;
  (void)weight;
  return false;
}

static void composer_finalize(AwraWidget *w)
{
  if (CPX(w)->busy_timer) {
    awra_source_remove(CPX(w)->busy_timer);
  }
}

static AwraClass composer_class = {
    .bk = AWRA_BK("AwraComposer", 0),
    .type = "AwraComposer",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED | AWRA_CLASS_FOCUS_WITHIN,
    .ext_size = sizeof(ComposerExt),
    .finalize = composer_finalize,
    .measure = composer_measure,
    .allocate = composer_allocate,
    .draw = composer_draw,
    .draw_over = composer_draw_over,
    .child_style = composer_child_style,
    .overdraw = 4,
};

static void reveal_anim(AwraWidget *w, double t, void *data)
{
  ComposerExt *x = CPX(w);
  float *v = (float *)data;
  float target;
  if (v == &x->send_reveal) {
    target = (x->has_text || x->can_send) ? 1.0f : 0.0f;
  }
  else if (v == &x->banner_reveal) {
    target = x->banner ? 1.0f : 0.0f;
  }
  else {
    target = x->attachments ? 1.0f : 0.0f;
  }
  /* each step moves the rest of the way */
  *v = *v + (target - *v) * (float)t;
  if (t >= 1.0) {
    *v = target;
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

static void reveal(AwraWidget *w, float *v, bool on)
{
  const float target = on ? 1.0f : 0.0f;
  if (*v == target) {
    return;
  }
  if (!awra_aux(w)->mapped || awra_style_reduced_motion()) {
    *v = target;
    awra_widget_queue_resize(w);
    return;
  }
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_NORMAL), reveal_anim, v, NULL);
}

static void update_send(AwraWidget *w)
{
  ComposerExt *x = CPX(w);
  const bool sendable = x->has_text || x->can_send;
  reveal(w, &x->send_reveal, sendable);
  awra_widget_set_can_target(x->send, sendable);
}

static bool text_has_content(const char *t)
{
  for (const unsigned char *p = (const unsigned char *)t; *p; p++) {
    if (*p != ' ' && *p != '\n' && *p != '\t' && *p != '\r') {
      return true;
    }
  }
  return false;
}

static void view_changed(AwraWidget *view, const AwraArgs *args, void *data)
{
  (void)view;
  AwraWidget *w = (AwraWidget *)data;
  ComposerExt *x = CPX(w);
  x->has_text = text_has_content(args->text ? args->text : "");
  update_send(w);
  awra_widget_queue_resize(w);
  awra_emit_text(w, AWRA_SIGNAL_CHANGED, args->text);
}

static void submit(AwraWidget *w)
{
  ComposerExt *x = CPX(w);
  if (!(x->has_text || x->can_send) || !awra_sensitive(w)) {
    return;
  }
  awra_emit_simple(w, AWRA_SIGNAL_ACTIVATE);
}

static void view_key(AwraWidget *view, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ComposerExt *x = CPX(w);
  if (!a->keysym || (strcmp(a->keysym, "Return") && strcmp(a->keysym, "KP_Enter") && strcmp(a->keysym, "ISO_Enter"))) {
    return;
  }
  if (completion_active_for(view)) {
    return;
  }
  const int mods = a->mods & (AWRA_MOD_SHIFT | AWRA_MOD_CTRL | AWRA_MOD_ALT);
  if (x->send_on_enter ? mods != 0 : mods != AWRA_MOD_CTRL) {
    return;
  }
  awra_widget_ref(w);
  submit(w);
  awra_widget_unref(w);
  if (a->handled) {
    *a->handled = true;
  }
}

static void send_clicked(AwraWidget *b, const AwraArgs *a, void *data)
{
  (void)b;
  (void)a;
  AwraWidget *w = (AwraWidget *)data;
  submit(w);
  awra_widget_grab_focus(CPX(w)->view);
}

static void view_focus_changed(AwraWidget *view, const AwraArgs *a, void *data)
{
  (void)view;
  (void)a;
  awra_invalidate((AwraWidget *)data);
}

AwraWidget *awra_composer_new(void)
{
  AwraWidget *w = awra_widget_new(&composer_class);
  ComposerExt *x = CPX(w);
  x->max_lines = 10;
  x->send_on_enter = true;
  x->view = awra_text_view_new();
  awra_text_view_set_bare_internal(x->view, TEXT_MARGIN_X, TEXT_MARGIN_Y, x->max_lines);
  awra_widget_set_hexpand(x->view, true);
  awra_child_insert(w, x->view, -1);
  awra_widget_unref(x->view);
  awra_signal_connect(x->view, AWRA_SIGNAL_CHANGED, view_changed, w, NULL);
  awra_signal_connect(x->view, AWRA_SIGNAL_KEY_PRESSED, view_key, w, NULL);
  awra_signal_connect(x->view, AWRA_SIGNAL_NOTIFY, view_focus_changed, w, NULL);
  x->send = awra_button_new_from_icon_name("awra-send-symbolic");
  awra_button_set_appearance(x->send, AWRA_BUTTON_PRIMARY);
  awra_widget_add_class(x->send, "circular");
  /* awra.css: min-width/min-height 32px, padding 0 (no border): 32 px, so
   * the row's band is 32 and its 34 px buttons stand a pixel below it */
  awra_button_set_metrics_internal(x->send, 0, 0, 32, 32);
  awra_widget_set_tooltip(x->send, "Send");
  awra_widget_set_can_target(x->send, false);
  awra_child_insert(w, x->send, -1);
  awra_widget_unref(x->send);
  awra_signal_connect(x->send, AWRA_SIGNAL_CLICKED, send_clicked, w, NULL);
  return w;
}

AwraWidget *awra_composer_view(AwraWidget *w)
{
  return CPX(w)->view;
}

void awra_composer_set_text(AwraWidget *w, const char *text)
{
  awra_text_view_set_text(CPX(w)->view, text ? text : "");
}

const char *awra_composer_text(AwraWidget *w)
{
  return awra_text_view_text(CPX(w)->view);
}

void awra_composer_set_placeholder(AwraWidget *w, const char *text)
{
  awra_text_view_set_placeholder(CPX(w)->view, text);
}

void awra_composer_set_max_lines(AwraWidget *w, int lines)
{
  ComposerExt *x = CPX(w);
  x->max_lines = lines;
  awra_text_view_set_bare_internal(x->view, TEXT_MARGIN_X, TEXT_MARGIN_Y, lines);
}

void awra_composer_set_can_send(AwraWidget *w, bool can_send)
{
  CPX(w)->can_send = can_send;
  update_send(w);
}

void awra_composer_set_send_on_enter(AwraWidget *w, bool on)
{
  CPX(w)->send_on_enter = on;
}

static bool busy_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ComposerExt *x = CPX(w);
  x->busy_angle = fmodf(x->busy_angle + 360.0f * 33.0f / 900.0f, 360.0f);
  awra_invalidate(w);
  return true;
}

void awra_composer_set_busy(AwraWidget *w, bool busy)
{
  ComposerExt *x = CPX(w);
  if (x->busy == busy) {
    return;
  }
  x->busy = busy;
  if (busy && !x->busy_timer) {
    x->busy_timer = awra_timeout_add(33, busy_tick, w, NULL);
  }
  else if (!busy && x->busy_timer) {
    awra_source_remove(x->busy_timer);
    x->busy_timer = 0;
  }
  awra_invalidate(w);
}

static void set_section(AwraWidget *w, AwraWidget **slot, float *rev, AwraWidget *child)
{
  if (*slot == child) {
    return;
  }
  if (*slot) {
    awra_child_remove(w, *slot);
  }
  *slot = child;
  if (child) {
    awra_child_insert(w, child, 0);
  }
  reveal(w, rev, child != NULL);
  awra_widget_queue_resize(w);
}

/* the banner's own look: a quiet band, 28 px, padding 2 2 2 10, radius 12 */
typedef struct BandExt {
  AwraWidget *child;
} BandExt;

static void band_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  BandExt *x = AWRA_EXT(w, BandExt);
  const int ph = awra_px(12), pv = awra_px(4);
  awra_measure(x->child, o, f >= 0 ? BK_MAX(0, f - (o == AWRA_ORIENTATION_HORIZONTAL ? pv : ph)) : -1, mn, nt);
  const int pad = o == AWRA_ORIENTATION_HORIZONTAL ? ph : pv;
  *mn += pad;
  *nt += pad;
  if (o == AWRA_ORIENTATION_VERTICAL) {
    *mn = BK_MAX(*mn, awra_px(28));
    *nt = BK_MAX(*nt, awra_px(28));
  }
}

static void band_allocate(AwraWidget *w)
{
  BandExt *x = AWRA_EXT(w, BandExt);
  awra_allocate(x->child, w->x + awra_px(10), w->y + awra_px(2), w->w - awra_px(12), w->h - awra_px(4));
}

static void band_draw(AwraWidget *w, AwraDraw *d)
{
  const AwraRGBA bg = awra_with_alpha(awra_tokens()->foreground, 0.045f);
  awra_paint_rbox(d, awra_box_of(w), awra_pxf(12), &bg, 0, NULL);
}

static AwraClass band_class = {
    .bk = AWRA_BK("AwraComposerBanner", 0),
    .type = "AwraComposerBanner",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(BandExt),
    .measure = band_measure,
    .allocate = band_allocate,
    .draw = band_draw,
};

void awra_composer_set_banner(AwraWidget *w, AwraWidget *banner)
{
  ComposerExt *x = CPX(w);
  AwraWidget *band = NULL;
  if (banner) {
    band = awra_widget_new(&band_class);
    awra_set_slot(band, &AWRA_EXT(band, BandExt)->child, banner);
  }
  set_section(w, &x->banner, &x->banner_reveal, band);
  if (band) {
    awra_widget_unref(band);
  }
}

AwraWidget *awra_composer_banner(AwraWidget *w)
{
  AwraWidget *band = CPX(w)->banner;
  return band ? AWRA_EXT(band, BandExt)->child : NULL;
}

static void banner_close(AwraWidget *b, const AwraArgs *a, void *data)
{
  (void)b;
  (void)a;
  AwraWidget *w = (AwraWidget *)data;
  awra_widget_ref(w);
  awra_composer_set_banner(w, NULL);
  awra_emit_simple(w, AWRA_SIGNAL_STOP_REQUESTED);
  awra_widget_unref(w);
}

void awra_composer_show_banner(AwraWidget *w, const char *text, bool closable)
{
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, GAP);
  AwraWidget *label = awra_label_new(text);
  awra_label_set_xalign(label, 0.0f);
  awra_label_set_ellipsize(label, AWRA_ELLIPSIZE_END);
  awra_label_set_scale(label, 0.92);
  awra_label_set_color(label, &awra_tokens()->muted);
  awra_widget_set_hexpand(label, true);
  awra_box_append(box, label);
  awra_widget_unref(label);
  if (closable) {
    AwraWidget *close = awra_button_new_from_icon_name("awra-close-symbolic");
    awra_button_set_appearance(close, AWRA_BUTTON_GHOST);
    awra_widget_add_class(close, "circular");
    /* awra.css: min-width/min-height 24px, padding 0 */
    awra_button_set_metrics_internal(close, 0, 0, 24, 24);
    awra_button_set_icon_size_internal(close, 12); /* its image: -gtk-icon-size 12px */
    awra_widget_set_tooltip(close, "Close");
    awra_signal_connect(close, AWRA_SIGNAL_CLICKED, banner_close, w, NULL);
    awra_box_append(box, close);
    awra_widget_unref(close);
  }
  awra_composer_set_banner(w, box);
  awra_widget_unref(box);
}

void awra_composer_set_attachments(AwraWidget *w, AwraWidget *attachments)
{
  ComposerExt *x = CPX(w);
  set_section(w, &x->attachments, &x->attach_reveal, attachments);
}

AwraWidget *awra_composer_attachments(AwraWidget *w)
{
  return CPX(w)->attachments;
}

static void set_row_slot(AwraWidget *w, AwraWidget **slot, AwraWidget *child)
{
  if (*slot == child) {
    return;
  }
  if (*slot) {
    awra_child_remove(w, *slot);
  }
  *slot = child;
  if (child) {
    awra_child_insert(w, child, -1);
  }
  awra_widget_queue_resize(w);
}

void awra_composer_set_start_widget(AwraWidget *w, AwraWidget *child)
{
  set_row_slot(w, &CPX(w)->start, child);
}

void awra_composer_set_end_widget(AwraWidget *w, AwraWidget *child)
{
  set_row_slot(w, &CPX(w)->end, child);
}

/* ======================================================================= */
/* Completion popover */

typedef struct CompletionExt {
  AwraWidget *popover, *scroller, *list, *attached;
  unsigned long key_handler;
} CompletionExt;

static CompletionExt *cmx(AwraWidget *w)
{
  return (CompletionExt *)awra_widget_get_data(w, "awra-completion");
}

static unsigned n_items(AwraWidget *w)
{
  return awra_list_view_n_items(cmx(w)->list);
}

static void activate_at(AwraWidget *w, unsigned pos)
{
  awra_widget_ref(w);
  awra_popover_popdown(cmx(w)->popover);
  awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, (int)pos);
  awra_widget_unref(w);
}

static void dismiss(AwraWidget *w)
{
  awra_popover_popdown(cmx(w)->popover);
  awra_emit_simple(w, AWRA_SIGNAL_STOP_SEARCH);
}

static void attached_key(AwraWidget *view, const AwraArgs *a, void *data)
{
  (void)view;
  AwraWidget *w = (AwraWidget *)data;
  CompletionExt *x = cmx(w);
  const unsigned count = n_items(w);
  if (!awra_popover_visible(x->popover) || count == 0 || !a->keysym) {
    return;
  }
  if (a->mods & (AWRA_MOD_CTRL | AWRA_MOD_ALT | AWRA_MOD_SUPER)) {
    return;
  }
  unsigned sel = awra_list_view_selected(x->list);
  const bool none = sel == UINT32_MAX;
  const char *k = a->keysym;
  bool done = true;
  if (!strcmp(k, "Up") || !strcmp(k, "KP_Up")) {
    awra_list_view_set_selected(x->list, none || sel == 0 ? count - 1 : sel - 1);
  }
  else if (!strcmp(k, "Down") || !strcmp(k, "KP_Down")) {
    awra_list_view_set_selected(x->list, none || sel + 1 >= count ? 0 : sel + 1);
  }
  else if (!strcmp(k, "Page_Up")) {
    awra_list_view_set_selected(x->list, none || sel < VISIBLE_ROWS ? 0 : sel - VISIBLE_ROWS);
  }
  else if (!strcmp(k, "Page_Down")) {
    awra_list_view_set_selected(x->list, BK_MIN(count - 1, (none ? 0 : sel) + VISIBLE_ROWS));
  }
  else if (!strcmp(k, "Tab") || !strcmp(k, "Return") || !strcmp(k, "KP_Enter") || !strcmp(k, "ISO_Enter")) {
    if (a->mods & AWRA_MOD_SHIFT) {
      return;
    }
    activate_at(w, none ? 0 : sel);
  }
  else if (!strcmp(k, "Escape")) {
    dismiss(w);
  }
  else {
    done = false;
  }
  if (done && a->handled) {
    *a->handled = true;
  }
}

static void list_activated(AwraWidget *list, const AwraArgs *a, void *data)
{
  (void)list;
  activate_at((AwraWidget *)data, (unsigned)a->index);
}

static void popover_hidden(AwraWidget *p, const AwraArgs *a, void *data)
{
  (void)p;
  (void)a;
  (void)data;
}

static void completion_free(void *p)
{
  CompletionExt *x = (CompletionExt *)p;
  awra_widget_unref(x->popover);
  if (x->attached) {
    awra_widget_unref(x->attached);
  }
  free(x);
}

/* the completion widget itself is an empty marker the application keeps;
 * its popover lives in the window's overlay */
AwraWidget *awra_completion_popover_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 0);
  awra_widget_set_name(w, "AwraCompletionPopover");
  CompletionExt *x = (CompletionExt *)calloc(1, sizeof(CompletionExt));
  x->popover = awra_popover_new();
  awra_popover_set_position(x->popover, 1);
  awra_popover_set_offset(x->popover, 0, -CARET_GAP);
  awra_widget_set_halign(x->popover, AWRA_ALIGN_START);
  awra_popover_set_autohide(x->popover, false); /* the text keeps the keyboard */
  awra_popover_set_padding_internal(x->popover, 0, false);
  x->scroller = awra_scrolled_new();
  awra_scrolled_set_scrollbar_style_internal(x->scroller, 2); /* the completion popover's */
  /* Awra 1's: between 220 and 420 px wide whatever its rows ask */
  awra_scrolled_set_policy(x->scroller, AWRA_POLICY_AUTOMATIC, AWRA_POLICY_AUTOMATIC);
  awra_scrolled_set_propagate_natural(x->scroller, true, true);
  awra_scrolled_set_min_content_width(x->scroller, MIN_WIDTH);
  awra_scrolled_set_max_content_width(x->scroller, MAX_WIDTH);
  awra_scrolled_set_max_content_height(x->scroller, DEFAULT_ROW_HEIGHT * VISIBLE_ROWS);
  x->list = awra_list_view_new();
  awra_list_view_set_row_style(x->list, 1);
  awra_list_view_set_padding(x->list, 5, 5);
  awra_list_view_set_single_click_activate(x->list, true);
  awra_widget_set_margins(x->list, 0, 5, 0, 5);
  awra_scrolled_set_child(x->scroller, x->list);
  awra_widget_unref(x->list);
  awra_popover_set_child(x->popover, x->scroller);
  awra_widget_unref(x->scroller);
  awra_signal_connect(x->list, AWRA_SIGNAL_ACTIVATE, list_activated, w, NULL);
  awra_signal_connect(x->popover, AWRA_SIGNAL_HIDE, popover_hidden, w, NULL);
  awra_widget_set_data(w, "awra-completion", x, completion_free);
  return w;
}

AwraWidget *awra_completion_popover_list(AwraWidget *w)
{
  return cmx(w)->list;
}

void awra_completion_popover_attach(AwraWidget *w, AwraWidget *view)
{
  CompletionExt *x = cmx(w);
  if (x->attached) {
    awra_widget_set_data(x->attached, "awra-completion-of", NULL, NULL);
    awra_signal_disconnect(x->attached, x->key_handler);
    awra_widget_unref(x->attached);
  }
  x->attached = view ? awra_widget_ref(view) : NULL;
  if (view) {
    awra_widget_set_data(view, "awra-completion-of", w, NULL);
    x->key_handler = awra_signal_connect(view, AWRA_SIGNAL_KEY_PRESSED, attached_key, w, NULL);
  }
}

static bool completion_active_for(AwraWidget *view)
{
  AwraWidget *w = (AwraWidget *)awra_widget_get_data(view, "awra-completion-of");
  return w && awra_popover_visible(cmx(w)->popover) && n_items(w) > 0;
}

void awra_completion_popover_set_selected(AwraWidget *w, unsigned position)
{
  awra_list_view_set_selected(cmx(w)->list, position);
}

unsigned awra_completion_popover_selected(AwraWidget *w)
{
  return awra_list_view_selected(cmx(w)->list);
}

bool awra_completion_popover_visible(AwraWidget *w)
{
  return awra_popover_visible(cmx(w)->popover);
}

void awra_completion_popover_present_at_cursor(AwraWidget *w)
{
  CompletionExt *x = cmx(w);
  if (!x->attached || n_items(w) == 0) {
    awra_popover_popdown(x->popover);
    return;
  }
  double cx, cy, ch;
  awra_editable_caret_rect(x->attached, &cx, &cy, &ch);
  if (awra_list_view_selected(x->list) == UINT32_MAX) {
    awra_list_view_set_selected(x->list, 0);
  }
  const unsigned rows = BK_MIN(n_items(w), VISIBLE_ROWS);
  awra_scrolled_set_max_content_height(x->scroller, (int)(rows * DEFAULT_ROW_HEIGHT + 10));
  /* Awra 1: pointing at the caret, above it (GTK_POS_TOP, offset -gap,
   * aligned to its start) */
  awra_popover_set_pointing_to(x->popover, cx, cy, 1, BK_MAX(1.0, ch));
  if (!awra_popover_visible(x->popover)) {
    awra_popover_set_anchor(x->popover, x->attached);
    awra_popover_popup(x->popover);
  }
  else {
    awra_widget_queue_resize(x->popover);
  }
}

void awra_completion_popover_popdown(AwraWidget *w)
{
  awra_popover_popdown(cmx(w)->popover);
}
