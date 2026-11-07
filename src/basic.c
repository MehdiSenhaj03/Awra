/* Awra 2 labels and images (GtkLabel / GtkImage semantics).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

bool awra_inherited_style(const AwraWidget *w, AwraRGBA *r_color, int *r_weight)
{
  for (const BKNode *n = w ? w->parent : NULL; n; n = n->parent) {
    if (awra_is_widget(n) && AWRA_CLASS_OF(n)->child_style &&
        AWRA_CLASS_OF(n)->child_style((AwraWidget *)n, r_color, r_weight))
    {
      return true;
    }
  }
  return false;
}

/* ======================================================================= */
/* Label */

typedef struct LabelExt {
  AwraTextLayout *layout;
  char *source; /* text or markup as given */
  bool markup;
  AwraWrap wrap;
  AwraEllipsize ellipsize;
  AwraJustify justify;
  float xalign, yalign;
  int lines, width_chars, max_width_chars;
  bool has_color;
  AwraRGBA color;
  int weight;
  float scale;
  unsigned style_gen;
  /* links, selection */
  bool selectable;
  int hover_link, pressed_link;
  int sel_anchor, sel_cursor; /* byte offsets */
  bool selecting;
  /* inline pictures (U+FFFC), drawn over their squares */
  AwraTexture **objects;
  int n_objects;
  int inline_size; /* logical px, 0: the letters' height */
  bool rich;       /* every U+FFFC is a picture's square */
  short above_paragraph, inside_wrap; /* logical px above lines (GtkTextView's) */
} LabelExt;

#define LX(w) AWRA_EXT(w, LabelExt)

static AwraFont label_font(AwraWidget *w)
{
  LabelExt *x = LX(w);
  const AwraTypography t = awra_widget_typography(w);
  AwraFont f = awra_font_for(t);
  AwraRGBA c;
  int weight = 0;
  if (!awra_aux(w)->typography_set && awra_inherited_style(w, &c, &weight) && weight > 0) {
    f.weight = weight;
  }
  if (x->weight > 0) {
    f.weight = x->weight;
  }
  if (x->scale > 0.0f && x->scale != 1.0f) {
    f.size *= x->scale;
    f.letter_spacing *= x->scale;
  }
  return f;
}

static void label_sync(AwraWidget *w)
{
  LabelExt *x = LX(w);
  AwraFont f = label_font(w);
  awra_text_layout_set_font(x->layout, &f);
  awra_text_layout_set_base_dir(x->layout, awra_widget_rtl(w));
  if (x->objects || x->inline_size > 0 || x->rich) {
    /* RichLabel: as tall as the text's own letters, at least 12 px */
    const float size = x->inline_size > 0 ? awra_pxf(x->inline_size) :
                                            fmaxf(awra_font_line_height(&f), awra_pxf(12));
    awra_text_layout_set_object_size(x->layout, floorf(size));
  }
  awra_text_layout_set_wrap(x->layout, x->wrap, x->ellipsize, x->lines);
  awra_text_layout_set_line_factor(x->layout, awra_typography_line_factor(awra_widget_typography(w)));
  awra_text_layout_set_line_extra(x->layout, awra_pxf(x->above_paragraph), awra_pxf(x->inside_wrap));
  awra_text_layout_set_justify(x->layout, x->justify);
  x->style_gen = awra_style_generation();
}

static void label_init(AwraWidget *w)
{
  LabelExt *x = LX(w);
  x->layout = awra_text_layout_new();
  x->xalign = 0.5f;
  x->yalign = 0.5f;
  x->scale = 1.0f;
  x->lines = -1;
  x->width_chars = -1;
  x->max_width_chars = -1;
  x->hover_link = -1;
  x->pressed_link = -1;
}

static void label_clear_objects(LabelExt *x)
{
  for (int i = 0; i < x->n_objects; i++) {
    awra_texture_unref(x->objects[i]);
  }
  free(x->objects);
  x->objects = NULL;
  x->n_objects = 0;
}

static void label_finalize(AwraWidget *w)
{
  awra_text_layout_free(LX(w)->layout);
  free(LX(w)->source);
  label_clear_objects(LX(w));
}

/* GtkLabel's get_char_pixels: the larger of Pango's approximate char width
 * (the sample string's average) and approximate digit width (the widest
 * digit) */
static float char_width(AwraWidget *w)
{
  AwraFont f = label_font(w);
  static const char sample[] = "The quick brown fox jumps over the lazy dog.";
  const float cw = awra_text_width(&f, sample, (int)sizeof(sample) - 1) / (float)(sizeof(sample) - 1);
  float dw = 0.0f;
  for (char d = '0'; d <= '9'; d++) {
    dw = fmaxf(dw, awra_text_width(&f, &d, 1));
  }
  return fmaxf(cw, dw);
}

/* my_pango_layout_get_width_for_height (gtklabel.c): the smallest width
 * in [min, max] whose wrapped height fits */
static int label_width_for_height(AwraTextLayout *l, int height, int min, int max)
{
  while (min < max) {
    int mid = (min + max) / 2, tw = 0, th = 0;
    awra_text_layout_size(l, mid, &tw, &th);
    if (tw > mid) {
      min = mid = tw;
    }
    else if (th > height) {
      min = mid + 1;
    }
    else {
      max = tw;
    }
  }
  return min;
}

static void label_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  LabelExt *x = LX(w);
  label_sync(w);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    int mn = awra_text_layout_min_width(x->layout);
    int nt = awra_text_layout_natural_width(x->layout);
    const float cw = (x->width_chars > 0 || x->max_width_chars > 0) ? char_width(w) : 0;
    if (x->width_chars > 0) {
      mn = BK_MAX(mn, (int)ceilf(cw * x->width_chars));
    }
    if (x->wrap != AWRA_WRAP_NONE && for_size >= 0) {
      /* width for a height: GTK's get_width_for_height (natural wrap
       * inherited, no ellipsis: minimum = natural) */
      const int lo = x->width_chars > 0 ? (int)ceilf(cw * x->width_chars) : 0;
      const int w4h = label_width_for_height(x->layout, for_size, lo, BK_MAX(lo, nt));
      *r_min = *r_nat = w4h;
      return;
    }
    if (x->max_width_chars > 0 && (x->wrap != AWRA_WRAP_NONE || x->ellipsize)) {
      const int limit = (int)ceilf(cw * BK_MAX(x->width_chars, x->max_width_chars));
      if (x->wrap != AWRA_WRAP_NONE && nt > limit) {
        /* the widest line laid out at that width (GtkLabel's natural) */
        awra_text_layout_size(x->layout, limit, &nt, NULL);
      }
      else {
        nt = BK_MIN(nt, limit);
      }
    }
    *r_min = mn;
    *r_nat = BK_MAX(mn, nt);
    return;
  }
  int h;
  const bool limited = x->wrap != AWRA_WRAP_NONE;
  awra_text_layout_size(x->layout, limited ? for_size : -1, NULL, &h);
  if (x->wrap != AWRA_WRAP_NONE && for_size < 0) {
    /* GtkLabel get_height_for_width (-1): the minimum assumes an infinite
     * width, the natural is the height at the natural width */
    int nw = awra_text_layout_natural_width(x->layout);
    if (x->max_width_chars > 0) {
      const float cw = char_width(w);
      nw = BK_MIN(nw, (int)ceilf(cw * BK_MAX(x->width_chars, x->max_width_chars)));
    }
    int hn;
    awra_text_layout_size(x->layout, nw, NULL, &hn);
    *r_min = h;
    *r_nat = BK_MAX(h, hn);
    return;
  }
  *r_min = *r_nat = h;
}

static AwraRGBA label_color(AwraWidget *w)
{
  LabelExt *x = LX(w);
  if (x->has_color) {
    return x->color;
  }
  AwraRGBA c = awra_typography_color(awra_widget_typography(w));
  int weight;
  if (!awra_aux(w)->typography_set || awra_widget_typography(w) == AWRA_TYPOGRAPHY_BODY ||
      awra_widget_typography(w) == AWRA_TYPOGRAPHY_LABEL)
  {
    AwraRGBA ic;
    if (awra_inherited_style(w, &ic, &weight) && ic.a > 0) {
      c = ic;
    }
  }
  if (awra_has_class_bit(w, AWRA_CB_DIM)) {
    c.a *= 0.55f;
  }
  if (awra_has_class_bit(w, AWRA_CB_ACCENT)) {
    c = awra_tokens()->accent_content;
  }
  if (awra_has_class_bit(w, AWRA_CB_ERROR)) {
    c = awra_tokens()->danger;
  }
  if (awra_has_class_bit(w, AWRA_CB_SUCCESS)) {
    c = awra_tokens()->success;
  }
  if (awra_has_class_bit(w, AWRA_CB_WARNING)) {
    c = awra_tokens()->warning;
  }
  return c;
}

/* where the layout sits in the label (raster px) and the width it's laid out at */
static void label_origin(AwraWidget *w, float *r_x, float *r_y, int *r_width)
{
  LabelExt *x = LX(w);
  int tw, th;
  const bool fit = x->wrap != AWRA_WRAP_NONE || x->ellipsize != AWRA_ELLIPSIZE_NONE;
  awra_text_layout_size(x->layout, fit ? w->w : -1, &tw, &th);
  /* GtkLabel: a layout that wraps or ellipsizes is as wide as the label,
   * its lines aligned in it (right-to-left paragraphs at the right); the
   * label places its logical extents (leftmost line to rightmost end) by
   * xalign */
  float lx0 = 0.0f, lw = (float)tw;
  if (fit) {
    awra_text_layout_logical(x->layout, w->w, &lx0, &lw);
    lw = fminf(ceilf(lw), (float)w->w);
  }
  /* right to left, xalign counts from the right (gtk_label_get_layout_location) */
  const float xalign = awra_widget_rtl(w) ? 1.0f - x->xalign : x->xalign;
  *r_x = (float)w->x + floorf((w->w - lw) * xalign + 0.5f) - lx0;
  *r_y = (float)w->y + floorf((w->h - th) * x->yalign + 0.5f);
  *r_width = fit ? w->w : -1;
}

typedef struct ObjectDraw {
  AwraWidget *w;
  AwraDraw *d;
  float ox, oy, size;
} ObjectDraw;

static void draw_object(int index, float x, float y, void *data)
{
  ObjectDraw *od = (ObjectDraw *)data;
  LabelExt *lx = LX(od->w);
  if (index < lx->n_objects && lx->objects[index]) {
    awra_paint_texture(od->d, lx->objects[index], (AwraBox){od->ox + x, od->oy + y, od->size, od->size},
                       0.0f, 1);
  }
}

static void label_draw(AwraWidget *w, AwraDraw *d)
{
  LabelExt *x = LX(w);
  if (x->style_gen != awra_style_generation()) {
    label_sync(w);
  }
  float lx, ly;
  int width;
  label_origin(w, &lx, &ly, &width);
  const AwraRGBA c = label_color(w);
  if (x->selectable && x->sel_anchor != x->sel_cursor) {
    AwraRGBA sel = awra_tokens()->selection;
    awra_text_layout_draw_selection(x->layout, d, lx, ly, width, x->sel_anchor, x->sel_cursor, &sel);
  }
  if (awra_text_layout_n_links(x->layout)) {
    /* awra.css: links take the readable accent, the foreground on hover */
    const AwraTokens *t = awra_tokens();
    awra_text_layout_set_link_style(x->layout, awra_aux(w)->hovered ? x->hover_link : -1,
                                    &t->accent_content, &t->foreground);
  }
  awra_text_layout_draw(x->layout, d, lx, ly, width, &c);
  if (x->n_objects) {
    AwraFont f = label_font(w);
    ObjectDraw od = {w, d, lx, ly,
                     floorf(x->inline_size > 0 ? awra_pxf(x->inline_size) :
                                                 fmaxf(awra_font_line_height(&f), awra_pxf(12)))};
    awra_text_layout_foreach_object(x->layout, width, draw_object, &od);
  }
}

static int label_index_at(AwraWidget *w, int px, int py)
{
  LabelExt *x = LX(w);
  float lx, ly;
  int width;
  label_origin(w, &lx, &ly, &width);
  return awra_text_layout_pos_to_index(x->layout, width, px - lx, py - ly);
}

static int label_link_at(AwraWidget *w, int px, int py)
{
  LabelExt *x = LX(w);
  if (!awra_text_layout_n_links(x->layout)) {
    return -1;
  }
  float lx, ly;
  int width;
  label_origin(w, &lx, &ly, &width);
  return awra_text_layout_link_at(x->layout, width, px - lx, py - ly);
}

static int label_motion(AwraWidget *w, BKEvent *e)
{
  LabelExt *x = LX(w);
  const int link = label_link_at(w, e->x, e->y);
  if (link != x->hover_link) {
    x->hover_link = link;
    awra_invalidate(w);
  }
  return 0;
}

static void label_hover(AwraWidget *w, bool on)
{
  if (!on && LX(w)->hover_link >= 0) {
    LX(w)->hover_link = -1;
    awra_invalidate(w);
  }
}

static int label_cursor(AwraWidget *w, int px, int py)
{
  if (label_link_at(w, px, py) >= 0) {
    return BK_CURSOR_HAND;
  }
  return LX(w)->selectable ? BK_CURSOR_TEXT : -1;
}

static int label_press(AwraWidget *w, BKEvent *e)
{
  LabelExt *x = LX(w);
  x->pressed_link = label_link_at(w, e->x, e->y);
  if (x->selectable && x->pressed_link < 0) {
    const int i = label_index_at(w, e->x, e->y);
    BKContext *ctx = bk_ctx(w);
    const bool dbl = ctx && ctx->last_click_node == NULL;
    if (dbl) {
      awra_text_layout_word_at(x->layout, i, &x->sel_anchor, &x->sel_cursor);
    }
    else if (e->mods & BK_SHIFT) {
      x->sel_cursor = i;
    }
    else {
      x->sel_anchor = x->sel_cursor = i;
    }
    x->selecting = true;
    AwraWidget *win = awra_window_of(w);
    if (win) {
      awra_set_focus(win, w, false);
    }
    awra_invalidate(w);
  }
  return 1;
}

static int label_drag(AwraWidget *w, BKEvent *e)
{
  LabelExt *x = LX(w);
  if (x->selecting) {
    const int i = label_index_at(w, e->x, e->y);
    if (i != x->sel_cursor) {
      x->sel_cursor = i;
      awra_invalidate(w);
    }
  }
  return 1;
}

static int label_release(AwraWidget *w, BKEvent *e)
{
  LabelExt *x = LX(w);
  x->selecting = false;
  const int link = x->pressed_link;
  x->pressed_link = -1;
  if (link >= 0 && e->inside && !e->dragged && label_link_at(w, e->x, e->y) == link) {
    const char *href = awra_text_layout_link(x->layout, link);
    awra_widget_ref(w);
    char *uri = awra_strdup(href);
    if (!awra_emit_text(w, AWRA_SIGNAL_ACTIVATE_LINK, uri) && uri) {
      awra_show_uri(uri);
    }
    free(uri);
    awra_widget_unref(w);
    return 1;
  }
  /* a click without a drag on plain text: no handler wanted it */
  if (!x->selectable && awra_has_handlers(w, AWRA_SIGNAL_CLICKED) && e->inside) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
  }
  return 1;
}

static int label_key(AwraWidget *w, BKEvent *e)
{
  LabelExt *x = LX(w);
  if (!x->selectable || !(e->mods & BK_CTRL)) {
    return 0;
  }
  if (bk_keysym_is(e->keysym, "c") || bk_keysym_is(e->keysym, "C")) {
    const int a = BK_MIN(x->sel_anchor, x->sel_cursor), b = BK_MAX(x->sel_anchor, x->sel_cursor);
    if (a == b) {
      return 0;
    }
    if (awra_emit_simple(w, AWRA_SIGNAL_COPY_CLIPBOARD)) {
      return 1;
    }
    const char *t = awra_text_layout_text(x->layout);
    char *part = (char *)malloc((size_t)(b - a + 1));
    if (part) {
      memcpy(part, t + a, (size_t)(b - a));
      part[b - a] = 0;
      awra_clipboard_set_text(w, part);
      free(part);
    }
    return 1;
  }
  if (bk_keysym_is(e->keysym, "a")) {
    x->sel_anchor = 0;
    x->sel_cursor = (int)strlen(awra_text_layout_text(x->layout));
    awra_invalidate(w);
    return 1;
  }
  return 0;
}

const char *awra_label_current_uri(AwraWidget *w)
{
  LabelExt *x = LX(w);
  return x->hover_link >= 0 ? awra_text_layout_link(x->layout, x->hover_link) : NULL;
}

static void label_events(AwraWidget *w)
{
  LabelExt *x = LX(w);
  if (x->selectable || awra_text_layout_n_links(x->layout) > 0) {
    w->flags |= NF_EVENTS;
  }
}

static void label_style_changed(AwraWidget *w)
{
  label_sync(w);
}

static bool label_set_string(AwraWidget *w, const char *name, const char *v)
{
  if (!strcmp(name, "label") || !strcmp(name, "text")) {
    awra_label_set_text(w, v);
    return true;
  }
  if (!strcmp(name, "markup")) {
    awra_label_set_markup(w, v);
    return true;
  }
  return false;
}

static const char *label_get_string(AwraWidget *w, const char *name)
{
  if (!strcmp(name, "label") || !strcmp(name, "text")) {
    return awra_text_layout_text(LX(w)->layout);
  }
  return NULL;
}

static int label_request_mode(AwraWidget *w)
{
  return LX(w)->wrap != AWRA_WRAP_NONE;
}

/* GtkLabel: its text is its name and its Text */
static void label_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  LabelExt *x = LX(w);
  const char *t = awra_text_layout_text(x->layout);
  info->label = t;
  info->text = t;
  if (x->selectable) {
    const int a = x->sel_anchor < x->sel_cursor ? x->sel_anchor : x->sel_cursor;
    const int b = x->sel_anchor < x->sel_cursor ? x->sel_cursor : x->sel_anchor;
    info->caret = awra_utf8_chars(t, x->sel_cursor);
    if (a != b) {
      info->sel_start = awra_utf8_chars(t, a);
      info->sel_end = awra_utf8_chars(t, b);
    }
  }
  if (awra_text_layout_n_links(x->layout) > 0 && x->n_objects == 0) {
    info->action = "activate";
  }
}

static bool label_a11y_layout(AwraWidget *w, AwraTextLayout **r_layout, float *r_x, float *r_y,
                              int *r_width)
{
  *r_layout = LX(w)->layout;
  label_origin(w, r_x, r_y, r_width);
  return true;
}

static AwraClass label_class = {
    .a11y = label_a11y,
    .a11y_layout = label_a11y_layout,
    .request_mode = label_request_mode,
    .bk = AWRA_BK("AwraLabel", 0),
    .type = "AwraLabel",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(LabelExt),
    .init = label_init,
    .finalize = label_finalize,
    .measure = label_measure,
    .draw = label_draw,
    .style_changed = label_style_changed,
    .set_string = label_set_string,
    .get_string = label_get_string,
    .press = label_press,
    .drag = label_drag,
    .release = label_release,
    .motion = label_motion,
    .key = label_key,
    .cursor = label_cursor,
    .hover = label_hover,
};

AwraWidget *awra_label_new(const char *text)
{
  AwraWidget *w = awra_widget_new(&label_class);
  awra_label_set_text(w, text);
  return w;
}

#define LABEL_OK(w) awra_widget_is(w, "AwraLabel")

static void label_changed(AwraWidget *w, bool size_may_change)
{
  if (size_may_change) {
    awra_widget_queue_resize(w);
  }
  awra_invalidate(w);
}

void awra_label_set_text(AwraWidget *w, const char *text)
{
  if (!LABEL_OK(w)) {
    return;
  }
  LabelExt *x = LX(w);
  if (!x->markup && x->source && text && !strcmp(x->source, text)) {
    return;
  }
  int ow = awra_text_layout_natural_width(x->layout);
  awra_set_str(&x->source, text ? text : "");
  x->markup = false;
  x->sel_anchor = x->sel_cursor = 0;
  awra_text_layout_set_text(x->layout, x->source, false);
  /* the same width and one line: no relayout, just repaint (clocks, counters) */
  const bool same = x->wrap == AWRA_WRAP_NONE && !strchr(x->source, '\n') &&
                    awra_text_layout_natural_width(x->layout) == ow && w->w > 0;
  label_changed(w, !same);
}

const char *awra_label_text(AwraWidget *w)
{
  return LABEL_OK(w) ? awra_text_layout_text(LX(w)->layout) : NULL;
}

void awra_label_set_markup(AwraWidget *w, const char *markup)
{
  if (!LABEL_OK(w)) {
    return;
  }
  LabelExt *x = LX(w);
  awra_set_str(&x->source, markup ? markup : "");
  x->markup = true;
  x->sel_anchor = x->sel_cursor = 0;
  x->hover_link = -1;
  awra_text_layout_set_text(x->layout, x->source, true);
  label_events(w);
  label_changed(w, true);
}

void awra_label_set_wrap(AwraWidget *w, AwraWrap wrap)
{
  LX(w)->wrap = wrap;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_ellipsize(AwraWidget *w, AwraEllipsize e)
{
  LX(w)->ellipsize = e;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_xalign(AwraWidget *w, float a)
{
  LX(w)->xalign = a;
  awra_invalidate(w);
}

void awra_label_set_yalign(AwraWidget *w, float a)
{
  LX(w)->yalign = a;
  awra_invalidate(w);
}

void awra_label_set_justify(AwraWidget *w, AwraJustify j)
{
  LX(w)->justify = j;
  label_sync(w);
  awra_invalidate(w);
}

void awra_label_set_lines(AwraWidget *w, int lines)
{
  LX(w)->lines = lines;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_line_spacing(AwraWidget *w, int above_paragraph, int inside_wrap)
{
  LX(w)->above_paragraph = (short)BK_MAX(0, above_paragraph);
  LX(w)->inside_wrap = (short)BK_MAX(0, inside_wrap);
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_max_width_chars(AwraWidget *w, int chars)
{
  LX(w)->max_width_chars = chars;
  label_changed(w, true);
}

void awra_label_set_width_chars(AwraWidget *w, int chars)
{
  LX(w)->width_chars = chars;
  label_changed(w, true);
}

void awra_label_set_selectable(AwraWidget *w, bool selectable)
{
  LabelExt *x = LX(w);
  x->selectable = selectable;
  if (!selectable) {
    x->sel_anchor = x->sel_cursor = 0;
  }
  label_events(w);
  awra_invalidate(w);
}

bool awra_label_selection_bounds(AwraWidget *w, int *r_start, int *r_end)
{
  LabelExt *x = LX(w);
  const int a = BK_MIN(x->sel_anchor, x->sel_cursor), b = BK_MAX(x->sel_anchor, x->sel_cursor);
  if (r_start) {
    *r_start = a;
  }
  if (r_end) {
    *r_end = b;
  }
  return a != b;
}

void awra_label_select_region(AwraWidget *w, int start, int end)
{
  LabelExt *x = LX(w);
  const int len = (int)strlen(awra_text_layout_text(x->layout));
  x->sel_anchor = BK_CLAMP(start, 0, len);
  x->sel_cursor = end < 0 ? len : BK_CLAMP(end, 0, len);
  awra_invalidate(w);
}

void awra_label_set_inline_size(AwraWidget *w, int size)
{
  LX(w)->inline_size = size;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_clear_inline_textures(AwraWidget *w)
{
  LabelExt *x = LX(w);
  if (!x->n_objects) {
    return;
  }
  label_clear_objects(x);
  awra_invalidate(w);
}

void awra_label_set_inline_texture(AwraWidget *w, int index, AwraTexture *texture)
{
  LabelExt *x = LX(w);
  if (index < 0) {
    index = x->n_objects;
  }
  if (index >= x->n_objects) {
    const bool first = x->objects == NULL;
    x->objects = (AwraTexture **)realloc(x->objects, sizeof(AwraTexture *) * (size_t)(index + 1));
    for (int i = x->n_objects; i <= index; i++) {
      x->objects[i] = NULL;
    }
    x->n_objects = index + 1;
    if (first) {
      label_sync(w);
      label_changed(w, true);
    }
  }
  if (x->objects[index] != texture) {
    awra_texture_ref(texture);
    awra_texture_unref(x->objects[index]);
    x->objects[index] = texture;
  }
  awra_invalidate(w);
}

void awra_label_set_color(AwraWidget *w, const AwraRGBA *c)
{
  LabelExt *x = LX(w);
  x->has_color = c != NULL;
  if (c) {
    x->color = *c;
  }
  awra_invalidate(w);
}

void awra_label_set_weight(AwraWidget *w, int weight)
{
  LX(w)->weight = weight;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_scale(AwraWidget *w, double scale)
{
  LX(w)->scale = (float)scale;
  label_sync(w);
  label_changed(w, true);
}

void awra_label_set_monospace_digits(AwraWidget *w, bool tabular)
{
  (void)w;
  (void)tabular;
}

/* ======================================================================= */
/* Image */

typedef struct ImageExt {
  char *icon;
  int pixel_size; /* logical, -1 = 16 */
  /* pictures */
  char *path;
  uint32_t *argb; /* decoded at the drawn size */
  int iw, ih;     /* intrinsic size */
  int dw, dh;     /* decoded size */
  void *bytes;
  size_t nbytes;
  float radius;
  int fit;
  bool has_color;
  AwraRGBA color;
  AwraTexture *tex; /* shared pixels (textures) */
  bool no_shrink;
} ImageExt;

#define IX(w) AWRA_EXT(w, ImageExt)

static void image_finalize(AwraWidget *w)
{
  ImageExt *x = IX(w);
  free(x->icon);
  free(x->path);
  free(x->argb);
  free(x->bytes);
  awra_texture_unref(x->tex);
}

static int icon_size(const AwraWidget *w)
{
  const ImageExt *x = IX(w);
  return awra_px(x->pixel_size > 0 ? x->pixel_size : 16);
}

static void image_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  ImageExt *x = IX(w);
  if (x->icon) {
    *r_min = *r_nat = icon_size(w);
    return;
  }
  if (x->pixel_size > 0) {
    *r_min = *r_nat = awra_px(x->pixel_size);
    return;
  }
  /* gtk_picture_measure: the intrinsic size, or the other dimension's
   * share of the aspect ratio for a given size */
  if (!x->iw || !x->ih || for_size == 0) {
    *r_min = *r_nat = 0;
    return;
  }
  const double iw = awra_px(x->iw), ih = awra_px(x->ih);
  const bool horizontal = o == AWRA_ORIENTATION_HORIZONTAL;
  double fs = for_size;
  if (fs > 0 && x->fit == 3) {
    const double opposite = horizontal ? ih : iw;
    if (opposite < fs) {
      fs = opposite;
    }
  }
  double nat;
  if (fs > 0) {
    nat = horizontal ? fs * iw / ih : fs * ih / iw;
  }
  else {
    nat = horizontal ? iw : ih;
  }
  *r_min = x->no_shrink ? (int)ceil(horizontal ? iw : ih) : 0;
  *r_nat = BK_MAX(*r_min, (int)ceil(nat));
}

static void decode_at(AwraWidget *w, int dw, int dh)
{
  ImageExt *x = IX(w);
  if ((x->dw == dw && x->dh == dh && x->argb) || (!x->path && !x->bytes)) {
    return;
  }
  BKImBuf *ib = x->path ? BK_imbuf_load(x->path, dw, dh) : BK_imbuf_decode(x->bytes, x->nbytes, dw, dh);
  if (!ib) {
    return;
  }
  int iw, ih;
  BK_imbuf_size(ib, &iw, &ih);
  free(x->argb);
  x->argb = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)iw * ih);
  if (x->argb) {
    memcpy(x->argb, BK_imbuf_pixels(ib, 0), sizeof(uint32_t) * (size_t)iw * ih);
  }
  x->dw = iw;
  x->dh = ih;
  BK_imbuf_free(ib);
}

/* How well something is going, on its icon (Awra 1's quality-good, -fair and
 * -poor roles, awra.css L1280): the status banners' colours. */
static bool quality_color(const AwraWidget *w, AwraRGBA *r_color)
{
  static int bits[3] = {-2};
  static const char *const names[3] = {"quality-good", "quality-fair", "quality-poor"};
  static const uint32_t dark[3] = {0x80dca5, 0xf0c66b, 0xff929c};
  static const uint32_t light[3] = {0x217846, 0x8a6215, 0xa02f3a};
  if (bits[0] == -2) {
    for (int i = 0; i < 3; i++) {
      bits[i] = awra_class_bit(names[i]);
    }
  }
  for (int i = 0; i < 3; i++) {
    if (bits[i] >= 0 && awra_has_class_bit(w, bits[i])) {
      const uint32_t v = awra_style_is_dark() ? dark[i] : light[i];
      *r_color = (AwraRGBA){((v >> 16) & 0xff) / 255.0f, ((v >> 8) & 0xff) / 255.0f, (v & 0xff) / 255.0f, 1.0f};
      return true;
    }
  }
  return false;
}

static void image_draw(AwraWidget *w, AwraDraw *d)
{
  ImageExt *x = IX(w);
  if (x->icon) {
    const int s = icon_size(w);
    AwraRGBA c = awra_tokens()->foreground;
    int weight;
    if (x->has_color) {
      c = x->color;
    }
    else if (!quality_color(w, &c) && !awra_inherited_style(w, &c, &weight)) {
      c = awra_typography_color(awra_widget_typography(w));
    }
    if (!awra_sensitive(w)) {
      c.a *= 0.5f;
    }
    const float ix = w->x + floorf((w->w - s) * 0.5f), iy = w->y + floorf((w->h - s) * 0.5f);
    if (awra_widget_has_class(w, "glow")) {
      /* "glow": the icon lit by a soft halo of the accent (an item worth pointing at) */
      static const float rings[3][2] = {{0.42f, 0.07f}, {0.30f, 0.10f}, {0.18f, 0.14f}};
      for (int i = 0; i < 3; i++) {
        AwraRGBA halo = awra_tokens()->accent;
        halo.a = rings[i][1];
        const float grow = s * rings[i][0];
        AwraBox hb = {ix - grow, iy - grow, s + 2 * grow, s + 2 * grow};
        awra_paint_rbox(d, hb, hb.w * 0.5f, &halo, 0, NULL);
      }
    }
    if (awra_icon_mask(x->icon, s)) {
      awra_paint_icon(d, x->icon, ix, iy, s, &c);
    }
    else {
      const BuiPixmap *pm = awra_icon_pixmap(x->icon, s);
      if (pm) {
        BUI_raster_image(d->r, ix, iy, (float)pm->w, (float)pm->h, pm, NULL, d->opacity, 0);
      }
    }
    return;
  }
  if (x->tex) {
    awra_paint_texture(d, x->tex, awra_box_of(w), awra_pxf(x->radius), x->fit);
    return;
  }
  if (!x->iw || !x->ih || w->w <= 0 || w->h <= 0) {
    return;
  }
  /* contain (default) / cover / fill in the allocation */
  float bw = (float)w->w, bh = (float)w->h, bx = (float)w->x, by = (float)w->y;
  const float ar = (float)x->iw / x->ih, br = bw / bh;
  if (x->fit != 0) {
    const bool contain = x->fit == 1;
    if ((ar > br) == contain) {
      const float nh = bw / ar;
      by += (bh - nh) * 0.5f;
      bh = nh;
    }
    else {
      const float nw = bh * ar;
      bx += (bw - nw) * 0.5f;
      bw = nw;
    }
  }
  decode_at(w, (int)ceilf(bw), (int)ceilf(bh));
  if (!x->argb) {
    return;
  }
  const BuiPixmap pm = {x->argb, x->dw, x->dh, (uint64_t)(uintptr_t)x->argb ^ (uint64_t)x->dw << 40,
                        BUI_PIXMAP_ARGB, 0};
  /* cover: crop the source to the box (drawn clipped to the allocation) */
  if (x->fit == 2) {
    bk_clip_push(w->x, w->y, w->w, w->h);
  }
  const float r = awra_pxf(x->radius);
  const float radii[4] = {r, r, r, r};
  BUI_raster_image_rounded(d->r, bx, by, bw, bh, &pm, NULL, d->opacity, radii);
  if (x->fit == 2) {
    bk_clip_pop();
  }
}

/* GtkImage: named only by the application; unnamed icons are decoration
 * (Awra 1 hid the icons of its buttons and rows) */
static void image_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  AwraWidget *by[1];
  const char *label = awra_a11y_property_string(w, AWRA_ACCESSIBLE_PROPERTY_LABEL);
  if ((!label || !label[0]) && !w->desc &&
      !awra_a11y_relation(w, AWRA_ACCESSIBLE_RELATION_LABELLED_BY, by, 1))
  {
    info->role = AWRA_ACCESSIBLE_ROLE_PRESENTATION;
  }
}

static AwraClass image_class = {
    .a11y = image_a11y,
    .bk = AWRA_BK("AwraImage", 0),
    .type = "AwraImage",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(ImageExt),
    .finalize = image_finalize,
    .measure = image_measure,
    .draw = image_draw,
};

AwraWidget *awra_image_new(void)
{
  AwraWidget *w = awra_widget_new(&image_class);
  IX(w)->pixel_size = -1;
  IX(w)->fit = 1;
  return w;
}

AwraWidget *awra_image_new_from_icon_name(const char *icon)
{
  AwraWidget *w = awra_image_new();
  awra_image_set_icon_name(w, icon);
  return w;
}

void awra_image_set_icon_name(AwraWidget *w, const char *icon)
{
  ImageExt *x = IX(w);
  if (x->icon && icon && !strcmp(x->icon, icon)) {
    return;
  }
  awra_set_str(&x->icon, icon && icon[0] ? icon : NULL);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_pixel_size(AwraWidget *w, int size)
{
  IX(w)->pixel_size = size;
  awra_widget_queue_resize(w);
}

static void picture_reset(AwraWidget *w)
{
  ImageExt *x = IX(w);
  free(x->icon);
  x->icon = NULL;
  free(x->argb);
  x->argb = NULL;
  x->dw = x->dh = 0;
  awra_texture_unref(x->tex);
  x->tex = NULL;
}

void awra_image_set_file(AwraWidget *w, const char *path)
{
  ImageExt *x = IX(w);
  picture_reset(w);
  free(x->bytes);
  x->bytes = NULL;
  awra_set_str(&x->path, path);
  BKImageInfo info = {0};
  if (path && BK_image_probe_file(path, &info)) {
    x->iw = info.width;
    x->ih = info.height;
  }
  else {
    x->iw = x->ih = 0;
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_bytes(AwraWidget *w, const void *data, size_t len)
{
  ImageExt *x = IX(w);
  picture_reset(w);
  free(x->path);
  x->path = NULL;
  free(x->bytes);
  x->bytes = malloc(len);
  if (x->bytes) {
    memcpy(x->bytes, data, len);
  }
  x->nbytes = len;
  BKImageInfo info = {0};
  if (x->bytes && BK_image_probe(x->bytes, len, &info)) {
    x->iw = info.width;
    x->ih = info.height;
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_rgba(AwraWidget *w, const uint8_t *rgba, int width, int height, int stride)
{
  ImageExt *x = IX(w);
  picture_reset(w);
  free(x->path);
  x->path = NULL;
  free(x->bytes);
  x->bytes = NULL;
  BKImBuf *ib = BK_imbuf_from_rgba(rgba, width, height, stride);
  if (!ib) {
    return;
  }
  x->argb = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)width * height);
  if (x->argb) {
    memcpy(x->argb, BK_imbuf_pixels(ib, 0), sizeof(uint32_t) * (size_t)width * height);
  }
  BK_imbuf_free(ib);
  x->iw = x->dw = width;
  x->ih = x->dh = height;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_image_set_corner_radius(AwraWidget *w, double r)
{
  IX(w)->radius = (float)r;
  awra_invalidate(w);
}

void awra_image_set_content_fit(AwraWidget *w, int fit)
{
  IX(w)->fit = fit;
  awra_invalidate(w);
}

void awra_image_set_color(AwraWidget *w, const AwraRGBA *c)
{
  ImageExt *x = IX(w);
  x->has_color = c != NULL;
  if (c) {
    x->color = *c;
  }
  awra_invalidate(w);
}

void awra_image_set_texture(AwraWidget *w, AwraTexture *texture)
{
  ImageExt *x = IX(w);
  if (x->tex == texture && !x->icon) {
    return;
  }
  const int ow = x->iw, oh = x->ih;
  awra_texture_ref(texture);
  picture_reset(w);
  free(x->path);
  x->path = NULL;
  free(x->bytes);
  x->bytes = NULL;
  x->tex = texture;
  x->iw = awra_texture_width(texture);
  x->ih = awra_texture_height(texture);
  if (x->iw != ow || x->ih != oh) {
    awra_widget_queue_resize(w);
  }
  awra_invalidate(w);
}

AwraTexture *awra_image_texture(AwraWidget *w)
{
  return IX(w)->tex;
}

void awra_image_set_can_shrink(AwraWidget *w, bool can_shrink)
{
  IX(w)->no_shrink = !can_shrink;
  awra_widget_queue_resize(w);
}

/* ======================================================================= */
/* Rich label (Awra 1's AwraRichLabel): a label whose U+FFFC characters are
 * squares as tall as its letters, with a picture drawn over each */

static void rich_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  awra_measure(w->kids[0], o, for_size, r_min, r_nat);
}

static void rich_allocate(AwraWidget *w)
{
  awra_allocate(w->kids[0], w->x, w->y, w->w, w->h);
}

static AwraClass rich_class = {
    .bk = AWRA_BK("AwraRichLabel", 0),
    .type = "AwraRichLabel",
    .flags = AWRA_CLASS_CONTAINER,
    .measure = rich_measure,
    .allocate = rich_allocate,
};

AwraWidget *awra_rich_label_new(void)
{
  AwraWidget *w = awra_widget_new(&rich_class);
  AwraWidget *l = awra_label_new("");
  LX(l)->rich = true;
  label_sync(l);
  awra_child_insert(w, l, -1);
  awra_widget_unref(l);
  return w;
}

AwraWidget *awra_rich_label_label(AwraWidget *w)
{
  return w->nkids ? w->kids[0] : NULL;
}

void awra_rich_label_clear_textures(AwraWidget *w)
{
  awra_label_clear_inline_textures(w->kids[0]);
}

void awra_rich_label_set_texture(AwraWidget *w, int index, AwraTexture *texture)
{
  awra_label_set_inline_texture(w->kids[0], index, texture);
}

void awra_rich_label_set_inline_size(AwraWidget *w, int size)
{
  awra_label_set_inline_size(w->kids[0], size);
}
