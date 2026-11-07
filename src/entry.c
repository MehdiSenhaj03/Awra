/* Awra 2 text inputs: AwraEntry, AwraSearchEntry, the multi-line text view,
 * AwraSpinButton and AwraShortcutCapture (spec B §8, §9, §15). Editing
 * keys, undo and the clipboard come from the kit's text edit (Blender's
 * rules); measuring, caret and selection use Awra's text layout.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

const char *awra_key_code_name(int evdev);
const char *awra_key_code_label(const char *code);

/* ======================================================================= */
/* Shared editing core */

typedef struct EditExt {
  BKTextEdit *te;
  AwraTextLayout *layout;
  char *placeholder, *icon, *accel;
  bool multiline, editable, visibility, search, spin, frameless;
  int max_len;
  float scroll_x, scroll_y;
  bool caret_on;
  unsigned blink_timer;
  double last_input;
  /* search */
  unsigned search_delay, search_timer;
  bool escape_stops; /* Escape stops the search even with text (GtkSearchEntry's stop-search) */
  /* spin */
  double min, max, step, value;
  int digits;
  int width_chars; /* spin buttons: the text's width in chars (Awra 1's) */
  int spin_hot; /* -1 none, 0 down, 1 up */
  bool dragging;
  char masked[1]; /* (unused) */
  int notified_caret, notified_anchor;
  /* bare: no frame (inside a composer); margins (logical), lines shown */
  bool bare;
  int bare_mx, bare_my, max_lines;
  const AwraRGBA *caret_color;
  /* GtkPasswordEntry's peek icon at the end: a click shows or hides the text */
  bool peek;
} EditExt;

/* the peek icon's box: 24 px, its right edge 17 px in from the field's
 * (GtkPasswordEntry's image after the text, as Awra 1 measures) */
#define PEEK_BOX 24
#define PEEK_END 17

#define EX(w) AWRA_EXT(w, EditExt)

static AwraFont edit_font(AwraWidget *w)
{
  return awra_font_for(awra_widget_typography(w));
}


/* the text drawn: bullets for hidden passwords */
static void layout_sync(AwraWidget *w)
{
  EditExt *x = EX(w);
  AwraFont f = edit_font(w);
  awra_text_layout_set_font(x->layout, &f);
  awra_text_layout_set_base_dir(x->layout, awra_widget_rtl(w));
  awra_text_layout_set_wrap(x->layout, x->multiline ? AWRA_WRAP_WORD : AWRA_WRAP_NONE,
                            AWRA_ELLIPSIZE_NONE, -1);
  awra_text_layout_set_text_view(x->layout, x->multiline);
  if (!x->visibility) {
    const int n = (int)strlen(x->te->text);
    char *dots = (char *)malloc((size_t)n * 3 + 1);
    int k = 0;
    for (int i = 0; i < n; i++) {
      if (((unsigned char)x->te->text[i] & 0xC0) != 0x80) {
        memcpy(dots + k, "\xE2\x80\xA2", 3);
        k += 3;
      }
    }
    dots[k] = 0;
    awra_text_layout_set_text(x->layout, dots, false);
    free(dots);
  }
  else {
    awra_text_layout_set_text(x->layout, x->te->text, false);
  }
}

/* byte index in the shown text for a byte index in the real text */
static int shown_index(AwraWidget *w, int i)
{
  if (EX(w)->visibility) {
    return i;
  }
  int chars = 0;
  for (int k = 0; k < i; k++) {
    if (((unsigned char)EX(w)->te->text[k] & 0xC0) != 0x80) {
      chars++;
    }
  }
  return chars * 3;
}

static int real_index(AwraWidget *w, int shown)
{
  if (EX(w)->visibility) {
    return shown;
  }
  const int chars = shown / 3;
  int k = 0, c = 0;
  const char *t = EX(w)->te->text;
  while (t[k] && c < chars) {
    k++;
    while ((t[k] & 0xC0) == 0x80) {
      k++;
    }
    c++;
  }
  return k;
}

static void edit_init(AwraWidget *w)
{
  EditExt *x = EX(w);
  x->te = bk_te_new("", false);
  x->te->caret = 0;
  x->te->anchor = -1;
  x->layout = awra_text_layout_new();
  x->editable = true;
  x->visibility = true;
  x->spin_hot = -1;
  x->search_delay = 150;
}

static void edit_finalize(AwraWidget *w)
{
  EditExt *x = EX(w);
  if (x->blink_timer) {
    awra_source_remove(x->blink_timer);
  }
  if (x->search_timer) {
    awra_source_remove(x->search_timer);
  }
  bk_te_free(x->te);
  awra_text_layout_free(x->layout);
  free(x->placeholder);
  free(x->icon);
  free(x->accel);
}

/* paddings (spec B §8: entry 5/11, search 5/8, spin 5/11 with 48 px buttons) */
/* Editing a name in place (Awra 1's inline-edit role, awra.css L3990): as
 * tall as the text it replaces, padding 1px 5px, radius 6, no minimum. */
static bool inline_edit(const AwraWidget *w)
{
  static int bit = -2;
  if (bit == -2) {
    bit = awra_class_bit("inline-edit");
  }
  return bit >= 0 && awra_has_class_bit(w, bit);
}

/* A GtkEntry that is not an AwraEntry (awra_entry_new_gtk): GTK 4's
 * default theme draws it (Default-dark/light.css: min-height 32px,
 * padding 0 8px, 1px border, radius 5px), Awra 1's stylesheet leaves it. */
static bool entry_gtk(const AwraWidget *w)
{
  return awra_widget_get_data((AwraWidget *)w, "awra-gtk-entry") != NULL;
}

static void edit_pads(AwraWidget *w, int *l, int *r, int *t, int *b)
{
  EditExt *x = EX(w);
  if (x->frameless) {
    *t = *b = *l = *r = 0;
    return;
  }
  if (inline_edit(w) && !x->multiline && !x->bare) {
    *t = *b = awra_px(1) + awra_px(1);
    *l = *r = awra_px(5) + awra_px(1);
    return;
  }
  if (entry_gtk(w) && !x->multiline && !x->bare) {
    *t = *b = awra_px(1);
    *l = *r = awra_px(8) + awra_px(1);
    if (x->icon) { /* entry > image.left { margin-right: 6px } */
      *l += awra_px(16) + awra_px(6);
    }
    if (awra_widget_rtl(w)) {
      const int k = *l;
      *l = *r;
      *r = k;
    }
    return;
  }
  *t = *b = awra_px(5) + awra_px(1);
  *l = *r = awra_px(x->search ? 8 : 11) + awra_px(1);
  if (x->icon || x->search) {
    *l += awra_px(16) + awra_px(x->search ? 4 + 6 : 12);
  }
  if (x->search && x->te && x->te->len > 0) {
    /* GtkSearchEntry: the clear icon shows (and takes room) with text */
    *r += awra_px(16) + awra_px(6 + 4);
  }
  if (x->spin) {
    *r += awra_px(96);
  }
  if (x->peek) {
    *r = awra_px(PEEK_END + PEEK_BOX);
  }
  if (x->accel) {
    AwraFont f = edit_font(w);
    f.size *= 0.86f;
    *r += (int)ceilf(awra_text_width(&f, x->accel, -1)) + awra_px(8);
  }
  if (x->multiline) {
    *t = *b = awra_px(8);
    *l = *r = awra_px(11);
  }
  if (x->bare) {
    *t = *b = awra_px(x->bare_my);
    *l = *r = awra_px(x->bare_mx);
  }
  if (awra_widget_rtl(w)) { /* the start (icon) side is the right one */
    const int k = *l;
    *l = *r;
    *r = k;
  }
}

static void edit_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  EditExt *x = EX(w);
  int l, r, t, b;
  edit_pads(w, &l, &r, &t, &b);
  layout_sync(w);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    if (x->spin) {
      /* GtkText with width-chars: char_pixels (the larger of Pango's
       * approximate char and digit widths, rounded up) times the chars */
      AwraFont f = edit_font(w);
      static const char sample[] = "The quick brown fox jumps over the lazy dog.";
      float cw = awra_text_width(&f, sample, (int)sizeof(sample) - 1) / (float)(sizeof(sample) - 1);
      for (char d = '0'; d <= '9'; d++) {
        cw = fmaxf(cw, awra_text_width(&f, &d, 1));
      }
      const int tw = (int)ceilf(cw) * x->width_chars;
      *r_min = *r_nat = l + r + tw;
      return;
    }
    *r_min = l + r + (inline_edit(w) ? 0 : awra_px(64));
    *r_nat = l + r + awra_px(150);
    return;
  }
  AwraFont f = edit_font(w);
  int th = (int)ceilf(awra_font_line_height(&f));
  if (x->bare) {
    /* as tall as its lines, one at least, max_lines at most */
    int h;
    awra_text_layout_size(x->layout, for_size >= 0 ? for_size - l - r : -1, NULL, &h);
    const int line = th;
    th = BK_MAX(line, h);
    if (x->max_lines > 0) {
      th = BK_MIN(th, line * x->max_lines);
    }
    *r_min = *r_nat = th + t + b;
    return;
  }
  if (x->multiline) {
    int h;
    awra_text_layout_size(x->layout, for_size >= 0 ? for_size - l - r : -1, NULL, &h);
    th = BK_MAX(th * 3, h);
  }
  if (entry_gtk(w) && !x->frameless) {
    th = BK_MAX(th, awra_px(32)); /* min-height: 32px */
  }
  else if (!x->frameless && !inline_edit(w)) {
    th = BK_MAX(th, awra_px(22));
  }
  if (x->spin) {
    th = BK_MAX(th, awra_px(24));
  }
  *r_min = *r_nat = th + t + b;
}

static AwraBox text_area(AwraWidget *w)
{
  int l, r, t, b;
  edit_pads(w, &l, &r, &t, &b);
  return (AwraBox){(float)(w->x + l), (float)(w->y + t), (float)(w->w - l - r), (float)(w->h - t - b)};
}

/* x of the text's origin: scrolled; right to left, a single line shorter
 * than its area sits at the area's right (GtkText's xalign flips with the
 * widget's direction) */
static float text_x(AwraWidget *w, AwraBox a)
{
  EditExt *x = EX(w);
  if (!x->multiline && awra_widget_rtl(w)) {
    int tw;
    awra_text_layout_size(x->layout, -1, &tw, NULL);
    if (tw <= a.w) {
      return a.x + a.w - tw;
    }
  }
  return a.x - x->scroll_x;
}

static void ensure_caret_visible(AwraWidget *w)
{
  EditExt *x = EX(w);
  AwraBox a = text_area(w);
  if (a.w <= 0) {
    /* not laid out yet: the text starts at its beginning */
    x->scroll_x = 0;
    return;
  }
  float cx, cy, ch;
  awra_text_layout_index_to_pos(x->layout, x->multiline ? (int)a.w : -1,
                                shown_index(w, x->te->caret), &cx, &cy, &ch);
  if (!x->multiline) {
    if (cx - x->scroll_x > a.w - 1) {
      x->scroll_x = cx - a.w + 1;
    }
    if (cx - x->scroll_x < 0) {
      x->scroll_x = cx;
    }
    int tw;
    awra_text_layout_size(x->layout, -1, &tw, NULL);
    if (tw - x->scroll_x < a.w) {
      x->scroll_x = fmaxf(0, tw - a.w + 1);
    }
  }
  else {
    if (cy + ch - x->scroll_y > a.h) {
      x->scroll_y = cy + ch - a.h;
    }
    if (cy - x->scroll_y < 0) {
      x->scroll_y = cy;
    }
  }
}


/* A quiet line rather than a well until it is used (awra.css): an entry
 * set so, or a search field in a sidebar or under .awra-role-quiet-search. */
static bool entry_quiet(AwraWidget *w)
{
  if (awra_widget_get_data(w, "awra-quiet")) {
    return true;
  }
  if (!EX(w)->search) {
    return false;
  }
  for (BKNode *p = w; p; p = p->parent) {
    if (!awra_is_widget(p)) {
      continue;
    }
    const char *name = awra_widget_name(p);
    if ((name && !strcmp(name, "AwraSidebar")) || awra_widget_has_class(p, "awra-role-quiet-search") ||
        awra_widget_has_class(p, "quiet-search"))
    {
      return true;
    }
  }
  return false;
}

static void edit_draw(AwraWidget *w, AwraDraw *d)
{
  EditExt *x = EX(w);
  const AwraTokens *t = awra_tokens();
  const bool focus = awra_widget_has_focus(w);
  const bool sens = awra_sensitive(w);
  const float radius = awra_pxf(inline_edit(w) ? 6 : x->search ? 9 : 11);
  AwraBox b = awra_box_of(w);
  const bool gtk = entry_gtk(w) && !x->multiline;
  /* GTK's default theme: its colours (Default-dark.css / Default-light.css) */
  const AwraRGBA gtk_fg = t->dark ? (sens ? (AwraRGBA){1, 1, 1, 1} : (AwraRGBA){0x91 / 255.0f, 0x91 / 255.0f, 0x90 / 255.0f, 1}) :
                                    (sens ? (AwraRGBA){0, 0, 0, 1} : (AwraRGBA){0x92 / 255.0f, 0x95 / 255.0f, 0x95 / 255.0f, 1});
  if (x->frameless || x->bare) {
    /* a bare text field: its container draws the frame */
  }
  else if (gtk) {
    const AwraRGBA bg = t->dark ? (sens ? (AwraRGBA){0x2d / 255.0f, 0x2d / 255.0f, 0x2d / 255.0f, 1} :
                                          (AwraRGBA){0x32 / 255.0f, 0x32 / 255.0f, 0x32 / 255.0f, 1}) :
                                  (sens ? (AwraRGBA){1, 1, 1, 1} : (AwraRGBA){0xfa / 255.0f, 0xf9 / 255.0f, 0xf8 / 255.0f, 1});
    const AwraRGBA border = t->dark ? (AwraRGBA){0x1b / 255.0f, 0x1b / 255.0f, 0x1b / 255.0f, 1} :
                                      (AwraRGBA){0xcd / 255.0f, 0xc7 / 255.0f, 0xc2 / 255.0f, 1};
    awra_paint_rbox(d, b, awra_pxf(5), &bg, awra_pxf(1), &border);
    if (focus) { /* entry:focus-within: outline 2px, offset -2px */
      const AwraRGBA ring = t->dark ? (AwraRGBA){21 / 255.0f, 83 / 255.0f, 158 / 255.0f, 0.7f} :
                                      (AwraRGBA){53 / 255.0f, 132 / 255.0f, 228 / 255.0f, 0.5f};
      awra_paint_rbox(d, b, awra_pxf(5), NULL, awra_pxf(2), &ring);
    }
  }
  else {
  if (focus) { /* outline 1px @awra_selection, offset 2 */
    const float ow = awra_pxf(1), off = awra_pxf(2);
    AwraBox o = {b.x - off - ow, b.y - off - ow, b.w + 2 * (off + ow), b.h + 2 * (off + ow)};
    awra_paint_rbox(d, o, radius + off + ow, NULL, ow, &t->selection);
  }
  const bool quiet = entry_quiet(w);
  const AwraRGBA *fill = quiet && !focus ? &t->control_cap_hover : &t->control_well;
  /* a form row's validation state colours the border (awra.css
   * .awra-entry.awra-validation-*) */
  AwraRGBA vb = {0};
  if (awra_widget_has_class(w, "validation-error")) {
    vb = t->danger;
    vb.a *= 0.88f;
  }
  else if (awra_widget_has_class(w, "validation-warning")) {
    vb = (AwraRGBA){0.910f, 0.706f, 0.310f, 0.78f};
  }
  else if (awra_widget_has_class(w, "validation-success")) {
    vb = (AwraRGBA){0.322f, 0.788f, 0.510f, 0.72f};
  }
  awra_paint_rbox(d, b, radius, fill, focus || vb.a > 0 ? awra_pxf(1) : 0, vb.a > 0 ? &vb : &t->focus_ring);
  if (!quiet) { /* inset 0 1px 2px rgba(0,0,0,.20), inside the 1 px border */
    const AwraRGBA sh = {0, 0, 0, 0.20f};
    awra_paint_inset_shadow(d, awra_box_inset(b, awra_pxf(1)), radius - awra_pxf(1), 0, awra_pxf(1), 0,
                            awra_pxf(2), &sh);
  }
  }
  AwraBox a = text_area(w);
  const AwraRGBA fg = gtk ? gtk_fg : sens ? t->foreground : t->disabled;
  /* icons */
  if (x->search || x->icon) {
    const int s = awra_px(16);
    const float ix = awra_mirror_x(w, b.x + awra_pxf(1) + awra_pxf(x->search ? 8 : gtk ? 8 : 11), (float)s);
    awra_paint_icon(d, x->search ? "system-search-symbolic" : x->icon, ix,
                    b.y + floorf((b.h - s) * 0.5f + 0.5f), s, &t->muted);
  }
  if (x->peek) {
    const int s = awra_px(16);
    const float ix = b.x + b.w - awra_pxf(PEEK_END) - awra_pxf((PEEK_BOX + 16) / 2.0f);
    awra_paint_icon(d, x->visibility ? "view-conceal-symbolic" : "view-reveal-symbolic", roundf(ix),
                    b.y + floorf((b.h - s) * 0.5f + 0.5f), s, &t->muted);
  }
  if (x->search && x->te->len > 0) {
    const int s = awra_px(16);
    /* GtkSearchEntry's clear image: 20 px wide inside the 9 px padding, its
     * icon at its start (as the search icon at the other end) */
    const float ix = awra_mirror_x(w, b.x + b.w - awra_pxf(9) - awra_pxf(20), (float)s);
    awra_paint_icon(d, "edit-clear-symbolic", ix, b.y + floorf((b.h - s) * 0.5f + 0.5f), s,
                    &t->muted);
  }
  if (x->accel) {
    AwraFont f = edit_font(w);
    f.size *= 0.86f;
    const float aw = awra_text_width(&f, x->accel, -1);
    const float ax = awra_mirror_x(w, b.x + b.w - awra_pxf(12) - aw, aw);
    const float base = b.y + floorf((b.h - awra_font_line_height(&f)) * 0.5f + awra_font_ascent(&f) + 0.5f);
    awra_draw_text(d, &f, ax, base, x->accel, -1, &t->muted);
  }
  if (x->spin) {
    const int s = awra_px(16);
    const float bw = awra_pxf(48);
    for (int k = 0; k < 2; k++) {
      AwraBox sb = awra_mirror_box(w, (AwraBox){b.x + b.w - awra_pxf(12) - bw * (2 - k),
                                                b.y + (b.h - awra_pxf(24)) * 0.5f, bw, awra_pxf(24)});
      const bool at_limit = k == 0 ? x->value <= x->min : x->value >= x->max;
      if (x->spin_hot == k && !at_limit && sens) {
        awra_paint_rbox(d, sb, awra_pxf(6), &t->selection, 0, NULL);
      }
      AwraRGBA ic = t->muted;
      if (at_limit) {
        ic.a *= 0.5f;
      }
      awra_paint_icon(d, k == 0 ? "value-decrease-symbolic" : "value-increase-symbolic",
                      sb.x + (sb.w - s) * 0.5f, sb.y + (sb.h - s) * 0.5f, s, &ic);
    }
  }
  /* text, clipped to its area */
  bk_clip_push((int)a.x, (int)b.y, (int)a.w, (int)b.h);
  int tw, th;
  const int lw = x->multiline ? (int)a.w : -1;
  awra_text_layout_size(x->layout, lw, &tw, &th);
  const float ty = x->multiline ? a.y - x->scroll_y : a.y + (float)(int)((a.h - th) * 0.5f); /* GtkText: truncated */
  const float tx = text_x(w, a);
  if (x->te->len == 0) {
    if (x->placeholder && !(x->spin)) {
      AwraTextLayout *pl = awra_text_layout_new();
      AwraFont f = edit_font(w);
      awra_text_layout_set_font(pl, &f);
      awra_text_layout_set_base_dir(pl, awra_widget_rtl(w));
      awra_text_layout_set_text(pl, x->placeholder, false);
      awra_text_layout_set_wrap(pl, AWRA_WRAP_NONE, AWRA_ELLIPSIZE_END, 1);
      AwraRGBA pc = fg;
      pc.a *= 0.55f;
      if (x->bare) {
        pc = sens ? t->muted : t->disabled;
      }
      float px = a.x;
      int pw, ph;
      awra_text_layout_size(pl, (int)a.w, &pw, &ph);
      if (awra_widget_rtl(w)) { /* the placeholder label's xalign flips too */
        float lx0 = 0.0f, lw0 = (float)pw;
        awra_text_layout_logical(pl, (int)a.w, &lx0, &lw0);
        px = a.x + a.w - fminf(ceilf(lw0), a.w) - lx0;
      }
      /* GtkText's placeholder is a GtkLabel over the text area: its own
       * layout, centred with the half pixel rounded down
       * (gtk_label_get_layout_location) */
      const float py = x->multiline ? ty : a.y + floorf((a.h - (float)ph) * 0.5f);
      awra_text_layout_draw(pl, d, px, py, (int)a.w, &pc);
      awra_text_layout_free(pl);
    }
  }
  else {
    if (x->te->anchor >= 0 && x->te->anchor != x->te->caret && focus) {
      /* GtkTextView: each line's selected runs and its ends; GtkText: one
       * box around them (a bidi selection spans what lies between) */
      const int s0 = shown_index(w, BK_MIN(x->te->anchor, x->te->caret));
      const int s1 = shown_index(w, BK_MAX(x->te->anchor, x->te->caret));
      if (x->multiline) {
        awra_text_layout_draw_selection_view(x->layout, d, tx, ty, lw, s0, s1, &t->selection, a.w);
      }
      else {
        /* entry > text > selection:focus-within, GTK's theme */
        const AwraRGBA gsel = t->dark ? (AwraRGBA){21 / 255.0f, 83 / 255.0f, 158 / 255.0f, 0.8f} :
                                        (AwraRGBA){53 / 255.0f, 132 / 255.0f, 228 / 255.0f, 0.3f};
        awra_text_layout_draw_selection_box(x->layout, d, tx, ty, lw, s0, s1, gtk ? &gsel : &t->selection);
      }
    }
    awra_text_layout_draw(x->layout, d, tx, ty, lw, &fg);
    if (gtk && !x->multiline && x->te->anchor >= 0 && x->te->anchor != x->te->caret && focus) {
      /* entry > text > selection:focus-within { color: #eeeeec / #2e3436 } */
      float ax, ay, ah, bx, by, bh;
      awra_text_layout_index_to_pos(x->layout, -1, shown_index(w, BK_MIN(x->te->anchor, x->te->caret)), &ax, &ay, &ah);
      awra_text_layout_index_to_pos(x->layout, -1, shown_index(w, BK_MAX(x->te->anchor, x->te->caret)), &bx, &by, &bh);
      const AwraRGBA sc = t->dark ? (AwraRGBA){0xee / 255.0f, 0xee / 255.0f, 0xec / 255.0f, 1} :
                                    (AwraRGBA){0x2e / 255.0f, 0x34 / 255.0f, 0x36 / 255.0f, 1};
      const float x0 = fminf(ax, bx), x1 = fmaxf(ax, bx);
      bk_clip_push((int)floorf(tx + x0), (int)b.y, (int)(ceilf(tx + x1) - floorf(tx + x0)), (int)b.h);
      awra_text_layout_draw(x->layout, d, tx, ty, lw, &sc);
      bk_clip_pop();
    }
  }
  /* GtkText draws its cursor only without a selection */
  const bool selecting = x->te->anchor >= 0 && x->te->anchor != x->te->caret;
  if (focus && x->caret_on && x->editable && !selecting) {
    float cx, cy, ch;
    awra_text_layout_index_to_pos(x->layout, lw, shown_index(w, x->te->caret), &cx, &cy, &ch);
    awra_paint_rect(d, (AwraBox){floorf(tx + cx), ty + cy, awra_pxf(1), ch},
                    x->caret_color ? x->caret_color : &fg);
  }
  bk_clip_pop();
}

static bool blink(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  EditExt *x = EX(w);
  if (!awra_widget_has_focus(w) || awra_now_ms() - x->last_input > 10000) {
    x->caret_on = awra_widget_has_focus(w);
    awra_invalidate(w);
    x->blink_timer = 0;
    return false; /* GTK stops blinking after 10 s idle */
  }
  x->caret_on = !x->caret_on;
  awra_invalidate(w);
  return true;
}

static void restart_blink(AwraWidget *w)
{
  EditExt *x = EX(w);
  x->caret_on = true;
  x->last_input = awra_now_ms();
  if (!x->blink_timer && !awra_style_reduced_motion()) {
    x->blink_timer = awra_timeout_add(600, blink, w, NULL);
  }
  awra_invalidate(w);
}

static void text_changed(AwraWidget *w)
{
  EditExt *x = EX(w);
  layout_sync(w);
  ensure_caret_visible(w);
  if (x->multiline) {
    awra_widget_queue_resize(w);
  }
  awra_invalidate(w);
  awra_emit_text(w, AWRA_SIGNAL_CHANGED, x->te->text);
}

static bool search_fire(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  EX(w)->search_timer = 0;
  awra_emit_text(w, AWRA_SIGNAL_SEARCH_CHANGED, EX(w)->te->text);
  return false;
}

static void after_edit(AwraWidget *w, const char *before)
{
  EditExt *x = EX(w);
  if (strcmp(before, x->te->text) != 0) {
    if (x->max_len > 0 && (int)strlen(x->te->text) > x->max_len) {
      x->te->text[x->max_len] = 0;
      x->te->len = x->max_len;
      x->te->caret = BK_MIN(x->te->caret, x->max_len);
    }
    text_changed(w);
    if (x->search) {
      if (x->search_timer) {
        awra_source_remove(x->search_timer);
      }
      x->search_timer = awra_timeout_add(x->search_delay, search_fire, w, NULL);
    }
  }
  else {
    ensure_caret_visible(w);
    awra_invalidate(w);
  }
  restart_blink(w);
}

static void spin_set(AwraWidget *w, double v, bool notify);

static int edit_key(AwraWidget *w, BKEvent *e)
{
  EditExt *x = EX(w);
  BKContext *ctx = bk_ctx(w);
  if (!ctx) {
    return 0;
  }
  if (x->spin && (bk_keysym_is(e->keysym, "Up") || bk_keysym_is(e->keysym, "Down"))) {
    spin_set(w, x->value + (bk_keysym_is(e->keysym, "Up") ? x->step : -x->step), true);
    return 1;
  }
  if (bk_keysym_is(e->keysym, "Return") || bk_keysym_is(e->keysym, "KP_Enter")) {
    if (x->multiline && x->editable) {
      char *before = awra_strdup(x->te->text);
      bk_te_key(x->te, ctx, "\n", "", 0);
      /* te_key treats Return as confirm: insert the newline ourselves */
      if (!strcmp(before, x->te->text)) {
        int a = x->te->caret;
        char *t = (char *)malloc((size_t)x->te->len + 2);
        memcpy(t, x->te->text, (size_t)a);
        t[a] = '\n';
        memcpy(t + a + 1, x->te->text + a, (size_t)(x->te->len - a + 1));
        free(x->te->text);
        x->te->text = t;
        x->te->len++;
        x->te->cap = x->te->len + 1;
        x->te->caret = a + 1;
        x->te->anchor = -1;
      }
      after_edit(w, before);
      free(before);
      return 1;
    }
    if (x->spin) {
      spin_set(w, atof(x->te->text), true);
    }
    awra_emit_text(w, AWRA_SIGNAL_ACTIVATE, x->te->text);
    return 1;
  }
  if (bk_keysym_is(e->keysym, "Escape")) {
    if (x->search && x->te->len > 0 && !x->escape_stops) {
      awra_entry_set_text(w, "");
      return 1;
    }
    if (x->search) {
      return awra_emit_simple(w, AWRA_SIGNAL_STOP_SEARCH) ? 1 : 0;
    }
    return 0;
  }
  if (bk_keysym_is(e->keysym, "Tab") || bk_keysym_is(e->keysym, "ISO_Left_Tab")) {
    return 0; /* focus moves on */
  }
  const bool nav = bk_keysym_is(e->keysym, "Left") || bk_keysym_is(e->keysym, "Right") ||
                   bk_keysym_is(e->keysym, "Home") || bk_keysym_is(e->keysym, "End");
  if (!x->editable && !nav && !((e->mods & BK_CTRL) && (bk_keysym_is(e->keysym, "c") ||
                                                         bk_keysym_is(e->keysym, "a"))))
  {
    return 0;
  }
  if (x->multiline && (bk_keysym_is(e->keysym, "Up") || bk_keysym_is(e->keysym, "Down"))) {
    AwraBox a = text_area(w);
    float cx, cy, ch;
    awra_text_layout_index_to_pos(x->layout, (int)a.w, x->te->caret, &cx, &cy, &ch);
    const float ny = bk_keysym_is(e->keysym, "Up") ? cy - ch * 0.5f : cy + ch * 1.5f;
    const int idx = awra_text_layout_pos_to_index(x->layout, (int)a.w, cx, ny);
    if (e->mods & BK_SHIFT) {
      if (x->te->anchor < 0) {
        x->te->anchor = x->te->caret;
      }
    }
    else {
      x->te->anchor = -1;
    }
    x->te->caret = idx;
    after_edit(w, x->te->text);
    return 1;
  }
  if ((bk_keysym_is(e->keysym, "Left") || bk_keysym_is(e->keysym, "Right")) &&
      !(e->mods & (BK_CTRL | BK_ALT)))
  {
    /* GtkText's visual moves: Left and Right go left and right on screen
     * (pango_layout_move_cursor_visually), in bidi text too; a selection
     * collapses to its end on that side */
    const int dir = bk_keysym_is(e->keysym, "Left") ? -1 : 1;
    const int lw = x->multiline ? (int)text_area(w).w : -1;
    const bool shift = e->mods & BK_SHIFT;
    BKTextEdit *te = x->te;
    int to;
    if (te->anchor >= 0 && te->anchor != te->caret && !shift) {
      float xa, xc, wk, yy, hh;
      awra_text_layout_cursor_pos(x->layout, lw, shown_index(w, te->anchor), &xa, &wk, &yy, &hh);
      awra_text_layout_cursor_pos(x->layout, lw, shown_index(w, te->caret), &xc, &wk, &yy, &hh);
      to = (dir < 0) == (xc < xa) ? te->caret : te->anchor;
    }
    else {
      to = real_index(w, awra_text_layout_move_visually(x->layout, lw, shown_index(w, te->caret), dir));
    }
    if (shift) {
      if (te->anchor < 0) {
        te->anchor = te->caret;
      }
    }
    else {
      te->anchor = -1;
    }
    te->caret = BK_CLAMP(to, 0, te->len);
    after_edit(w, te->text);
    return 1;
  }
  char *before = awra_strdup(x->te->text);
  const int r = bk_te_key(x->te, ctx, e->text, e->keysym, e->mods);
  after_edit(w, before);
  free(before);
  return r != TE_NONE;
}

static int edit_index_at(AwraWidget *w, int px, int py)
{
  EditExt *x = EX(w);
  AwraBox a = text_area(w);
  int tw, th;
  const int lw = x->multiline ? (int)a.w : -1;
  awra_text_layout_size(x->layout, lw, &tw, &th);
  const float ty = x->multiline ? a.y - x->scroll_y : a.y + (float)(int)((a.h - th) * 0.5f); /* GtkText: truncated */
  return real_index(w, awra_text_layout_pos_to_index(x->layout, lw, px - text_x(w, a), py - ty));
}

static bool in_peek_icon(AwraWidget *w, int px)
{
  return EX(w)->peek && px >= w->x + w->w - awra_px(PEEK_END + PEEK_BOX) && px < w->x + w->w - awra_px(PEEK_END);
}

static bool in_clear_icon(AwraWidget *w, int px)
{
  return EX(w)->search && EX(w)->te->len > 0 && awra_mirror_px(w, px) >= w->x + w->w - awra_px(9 + 20);
}

static int spin_button_at(AwraWidget *w, int px, int py)
{
  EditExt *x = EX(w);
  if (!x->spin) {
    return -1;
  }
  const float bw = awra_pxf(48);
  const float right = w->x + w->w - awra_pxf(12);
  px = awra_mirror_px(w, px);
  if (px >= right - 2 * bw && px < right - bw) {
    return 0;
  }
  if (px >= right - bw && px < right + awra_pxf(12)) {
    return 1;
  }
  return -1;
}

static int edit_press(AwraWidget *w, BKEvent *e)
{
  EditExt *x = EX(w);
  const int sb = spin_button_at(w, e->x, e->y);
  if (sb >= 0) {
    spin_set(w, x->value + (sb ? x->step : -x->step), true);
    return 1;
  }
  if (in_clear_icon(w, e->x)) {
    awra_entry_set_text(w, "");
    return 1;
  }
  if (in_peek_icon(w, e->x)) {
    awra_entry_set_visibility(w, !x->visibility);
    return 1;
  }
  BKContext *ctx = bk_ctx(w);
  const int idx = edit_index_at(w, e->x, e->y);
  if (ctx && ctx->last_click_node == NULL) { /* double click: a word */
    bk_te_select_word(x->te, idx);
  }
  else if (e->mods & BK_SHIFT) {
    if (x->te->anchor < 0) {
      x->te->anchor = x->te->caret;
    }
    x->te->caret = idx;
  }
  else {
    x->te->caret = idx;
    x->te->anchor = idx;
  }
  x->dragging = true;
  restart_blink(w);
  return 1;
}

static int edit_drag(AwraWidget *w, BKEvent *e)
{
  EditExt *x = EX(w);
  if (!x->dragging) {
    return 0;
  }
  x->te->caret = edit_index_at(w, e->x, e->y);
  ensure_caret_visible(w);
  awra_invalidate(w);
  return 1;
}

static int edit_release(AwraWidget *w, BKEvent *e)
{
  EditExt *x = EX(w);
  x->dragging = false;
  if (x->te->anchor == x->te->caret) {
    x->te->anchor = -1;
  }
  return 1;
}

static int edit_motion(AwraWidget *w, BKEvent *e)
{
  EditExt *x = EX(w);
  const int hot = spin_button_at(w, e->x, e->y);
  if (hot != x->spin_hot) {
    x->spin_hot = hot;
    awra_invalidate(w);
  }
  return 0;
}

static int edit_cursor(AwraWidget *w, int px, int py)
{
  if (spin_button_at(w, px, py) >= 0 || in_clear_icon(w, px) || in_peek_icon(w, px)) {
    return BK_CURSOR_DEFAULT;
  }
  return BK_CURSOR_TEXT;
}

static void edit_finish(BKNode *n, bool confirm)
{
  /* focus left the field */
  EditExt *x = EX(n);
  x->te->anchor = -1;
  x->caret_on = false;
  if (x->spin) {
    spin_set(n, atof(x->te->text), true);
  }
  awra_invalidate(n);
}

static int edit_scroll(AwraWidget *w, BKEvent *e)
{
  EditExt *x = EX(w);
  if (x->spin && awra_widget_has_focus(w)) {
    spin_set(w, x->value + (e->wheel_y > 0 ? x->step : -x->step), true);
    return 1;
  }
  return 0;
}

static void edit_style_changed(AwraWidget *w)
{
  layout_sync(w);
}

static bool edit_set_string(AwraWidget *w, const char *name, const char *v)
{
  if (!strcmp(name, "text")) {
    awra_entry_set_text(w, v);
    return true;
  }
  if (!strcmp(name, "placeholder")) {
    awra_entry_set_placeholder(w, v);
    return true;
  }
  return false;
}

static const char *edit_get_string(AwraWidget *w, const char *name)
{
  return !strcmp(name, "text") ? EX(w)->te->text : NULL;
}

/* the caret or the selection moved: notify "cursor-position" */
static void caret_notify(AwraWidget *w)
{
  EditExt *x = EX(w);
  if (x->te->caret != x->notified_caret || x->te->anchor != x->notified_anchor) {
    x->notified_caret = x->te->caret;
    x->notified_anchor = x->te->anchor;
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "cursor-position");
  }
}

static int edit_key_notify(AwraWidget *w, BKEvent *e)
{
  awra_widget_ref(w);
  const int r = edit_key(w, e);
  caret_notify(w);
  awra_widget_unref(w);
  return r;
}

static int edit_press_notify(AwraWidget *w, BKEvent *e)
{
  const int r = edit_press(w, e);
  caret_notify(w);
  return r;
}

static int edit_drag_notify(AwraWidget *w, BKEvent *e)
{
  const int r = edit_drag(w, e);
  caret_notify(w);
  return r;
}

/* GtkEntry, GtkSearchEntry, GtkSpinButton, GtkTextView, GtkPasswordEntry:
 * their text (none for hidden passwords), caret and selection */
static void edit_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  EditExt *x = EX(w);
  const char *t = x->te->text ? x->te->text : "";
  info->role = x->spin ? AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON :
               x->search ? AWRA_ACCESSIBLE_ROLE_SEARCH_BOX : AWRA_ACCESSIBLE_ROLE_TEXT_BOX;
  info->text = t;
  info->placeholder = x->placeholder;
  info->action = "activate";
  if (x->multiline) {
    info->flags |= AWRA_A11Y_MULTI_LINE;
  }
  if (!x->editable) {
    info->flags |= AWRA_A11Y_READ_ONLY;
  }
  if (!x->visibility) {
    info->flags |= AWRA_A11Y_PASSWORD;
  }
  int a = x->te->caret, b = x->te->anchor;
  info->caret = awra_utf8_chars(t, a);
  if (b >= 0 && b != a) {
    info->sel_start = awra_utf8_chars(t, a < b ? a : b);
    info->sel_end = awra_utf8_chars(t, a < b ? b : a);
  }
  if (x->spin) {
    info->has_value = true;
    info->value = x->value;
    info->min = x->min;
    info->max = x->max;
    info->step = x->step;
  }
}

static bool edit_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  EditExt *x = EX(w);
  if (item >= 0 || !awra_sensitive(w)) {
    return false;
  }
  if (verb == AWRA_A11Y_DO_SET_VALUE && x->spin) {
    awra_spin_button_set_value(w, value);
    return true;
  }
  if (verb == AWRA_A11Y_DO_ACTIVATE) {
    awra_widget_grab_focus(w);
    if (!x->multiline) {
      awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, 0);
    }
    return true;
  }
  return false;
}

/* the layout's place, as edit_draw puts it (hidden passwords: none) */
static bool edit_a11y_layout(AwraWidget *w, AwraTextLayout **r_layout, float *r_x, float *r_y,
                             int *r_width)
{
  EditExt *x = EX(w);
  if (!x->visibility) {
    return false;
  }
  const AwraBox a = text_area(w);
  const int lw = x->multiline ? (int)a.w : -1;
  int tw, th;
  awra_text_layout_size(x->layout, lw, &tw, &th);
  *r_layout = x->layout;
  *r_x = text_x(w, a);
  *r_y = x->multiline ? a.y - x->scroll_y : a.y + (float)(int)((a.h - th) * 0.5f); /* GtkText: truncated */
  *r_width = lw;
  return true;
}

static AwraClass entry_class = {
    .a11y = edit_a11y,
    .a11y_layout = edit_a11y_layout,
    .a11y_do = edit_a11y_do,
    .bk = AWRA_BK("AwraEntry", CLS_INTERACTIVE),
    .type = "AwraEntry",
    .flags = AWRA_CLASS_TEXT_INPUT | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(EditExt),
    .init = edit_init,
    .finalize = edit_finalize,
    .measure = edit_measure,
    .draw = edit_draw,
    .press = edit_press_notify,
    .drag = edit_drag_notify,
    .release = edit_release,
    .motion = edit_motion,
    .key = edit_key_notify,
    .scroll = edit_scroll,
    .cursor = edit_cursor,
    .style_changed = edit_style_changed,
    .set_string = edit_set_string,
    .get_string = edit_get_string,
    .overdraw = 4,
};

static AwraWidget *edit_new(void)
{
  AwraWidget *w = awra_widget_new(&entry_class);
  /* the window's focus change calls finish_edit */
  ((BKClass *)&entry_class.bk)->finish_edit = edit_finish;
  layout_sync(w);
  return w;
}

AwraWidget *awra_entry_new(void)
{
  return edit_new();
}

void awra_entry_set_text(AwraWidget *w, const char *text)
{
  EditExt *x = EX(w);
  if (!text) {
    text = "";
  }
  if (!strcmp(x->te->text, text)) {
    return;
  }
  char *before = awra_strdup(x->te->text);
  const int n = (int)strlen(text);
  if (n + 1 > x->te->cap) {
    x->te->cap = n + 64;
    x->te->text = (char *)realloc(x->te->text, (size_t)x->te->cap);
  }
  memcpy(x->te->text, text, (size_t)n + 1);
  x->te->len = n;
  x->te->caret = n;
  x->te->anchor = -1;
  x->scroll_x = 0;
  after_edit(w, before);
  free(before);
}

const char *awra_entry_text(AwraWidget *w)
{
  return EX(w)->te->text;
}

void awra_entry_set_placeholder(AwraWidget *w, const char *text)
{
  awra_set_str(&EX(w)->placeholder, text);
  awra_invalidate(w);
}

void awra_entry_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_set_str(&EX(w)->icon, icon);
  awra_widget_queue_resize(w);
}

void awra_entry_set_peek_icon_internal(AwraWidget *w, bool peek);
void awra_entry_set_peek_icon_internal(AwraWidget *w, bool peek)
{
  if (EX(w)->peek != peek) {
    EX(w)->peek = peek;
    awra_widget_queue_resize(w);
    awra_invalidate(w);
  }
}

void awra_entry_set_visibility(AwraWidget *w, bool visible)
{
  EX(w)->visibility = visible;
  layout_sync(w);
  awra_invalidate(w);
}

void awra_entry_set_max_length(AwraWidget *w, int chars)
{
  EX(w)->max_len = chars;
}

void awra_entry_select_all(AwraWidget *w)
{
  EX(w)->te->anchor = 0;
  EX(w)->te->caret = EX(w)->te->len;
  awra_invalidate(w);
}

void awra_entry_select_region(AwraWidget *w, int start, int end)
{
  EditExt *x = EX(w);
  start = start < 0 ? x->te->len : BK_MIN(start, x->te->len);
  end = end < 0 ? x->te->len : BK_MIN(end, x->te->len);
  x->te->anchor = start;
  x->te->caret = end;
  ensure_caret_visible(w);
  awra_invalidate(w);
}

void awra_entry_set_has_frame(AwraWidget *w, bool frame)
{
  EX(w)->frameless = !frame;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_entry_set_position(AwraWidget *w, int position)
{
  EditExt *x = EX(w);
  x->te->caret = position < 0 ? x->te->len : BK_MIN(position, x->te->len);
  x->te->anchor = -1;
  awra_invalidate(w);
}

void awra_entry_set_accel_hint(AwraWidget *w, const char *hint)
{
  awra_set_str(&EX(w)->accel, hint);
  awra_widget_queue_resize(w);
}

void awra_entry_set_editable(AwraWidget *w, bool editable)
{
  EX(w)->editable = editable;
}

AwraWidget *awra_search_entry_new(void)
{
  AwraWidget *w = edit_new();
  EX(w)->search = true;
  awra_widget_set_data(w, "awra-type", (void *)"AwraSearchEntry", NULL);
  return w;
}

void awra_search_entry_set_delay(AwraWidget *w, unsigned ms)
{
  EX(w)->search_delay = ms;
}

void awra_search_entry_set_escape_stops(AwraWidget *w, bool stops)
{
  EX(w)->escape_stops = stops;
}

AwraWidget *awra_entry_new_gtk(void)
{
  AwraWidget *w = awra_entry_new();
  awra_widget_set_data(w, "awra-gtk-entry", (void *)1, NULL);
  return w;
}

void awra_entry_set_quiet(AwraWidget *w, bool quiet);
void awra_entry_set_quiet(AwraWidget *w, bool quiet)
{
  awra_widget_set_data(w, "awra-quiet", quiet ? (void *)1 : NULL, NULL);
  awra_invalidate(w);
}

AwraWidget *awra_text_view_new(void)
{
  AwraWidget *w = edit_new();
  EX(w)->multiline = true;
  awra_widget_set_data(w, "awra-type", (void *)"AwraTextView", NULL);
  layout_sync(w);
  return w;
}

void awra_text_view_set_text(AwraWidget *w, const char *text)
{
  awra_entry_set_text(w, text);
}

const char *awra_text_view_text(AwraWidget *w)
{
  return awra_entry_text(w);
}

void awra_text_view_set_placeholder(AwraWidget *w, const char *t)
{
  awra_entry_set_placeholder(w, t);
}

void awra_text_view_set_editable(AwraWidget *w, bool e)
{
  awra_entry_set_editable(w, e);
}

void awra_text_view_set_wrap(AwraWidget *w, AwraWrap wrap)
{
  EX(w)->multiline = true;
  layout_sync(w);
  awra_widget_queue_resize(w);
}

/* ----------------------------------------------------------------------- */
/* Spin button */

static void spin_show(AwraWidget *w)
{
  EditExt *x = EX(w);
  char buf[64];
  snprintf(buf, sizeof(buf), "%.*f", x->digits, x->value);
  char *before = awra_strdup(x->te->text);
  const int n = (int)strlen(buf);
  if (n + 1 > x->te->cap) {
    x->te->cap = n + 16;
    x->te->text = (char *)realloc(x->te->text, (size_t)x->te->cap);
  }
  memcpy(x->te->text, buf, (size_t)n + 1);
  x->te->len = n;
  x->te->caret = BK_MIN(x->te->caret, n);
  layout_sync(w);
  awra_invalidate(w);
  free(before);
}

static void spin_set(AwraWidget *w, double v, bool notify)
{
  EditExt *x = EX(w);
  if (x->step > 0) {
    v = x->min + floor((v - x->min) / x->step + 0.5) * x->step;
  }
  v = v < x->min ? x->min : v > x->max ? x->max : v;
  const bool changed = v != x->value;
  x->value = v;
  spin_show(w);
  if (changed && notify) {
    awra_emit_value(w, AWRA_SIGNAL_VALUE_CHANGED, v);
  }
}

AwraWidget *awra_spin_button_new(double min, double max, double step)
{
  AwraWidget *w = edit_new();
  EditExt *x = EX(w);
  x->spin = true;
  x->min = min;
  x->max = max;
  x->step = step;
  x->value = min;
  x->digits = step > 0 && step < 1 ? (step < 0.1 ? 2 : 1) : 0;
  /* Awra 1's fit_width_to_range, at construction while its GtkSpinButton
   * still shows no decimals: the widest of the bounds as integers, 2 at
   * least (decimals set later don't widen it) */
  char lo[64], hi[64];
  snprintf(lo, sizeof(lo), "%.0f", min);
  snprintf(hi, sizeof(hi), "%.0f", max);
  x->width_chars = BK_MAX(2, (int)BK_MAX(strlen(lo), strlen(hi)));
  awra_widget_set_data(w, "awra-type", (void *)"AwraSpinButton", NULL);
  spin_show(w);
  return w;
}

void awra_spin_button_set_value(AwraWidget *w, double v)
{
  spin_set(w, v, true);
}

double awra_spin_button_value(AwraWidget *w)
{
  return EX(w)->value;
}

void awra_spin_button_set_digits(AwraWidget *w, int digits)
{
  EX(w)->digits = digits;
  spin_show(w);
  awra_widget_queue_resize(w);
}

void awra_spin_button_set_range(AwraWidget *w, double min, double max)
{
  EX(w)->min = min;
  EX(w)->max = max;
  spin_set(w, EX(w)->value, false);
  awra_widget_queue_resize(w);
}

/* ======================================================================= */
/* Shortcut capture (spec B §15) */

typedef struct CaptureExt {
  bool listening, allow_chords;
  char *placeholder;
  char **codes; /* captured physical codes */
  int ncodes;
  char **labels; /* chips shown */
  int nlabels;
  int held[16];  /* evdev keys held while listening */
  int nheld;
  int seen[16];  /* every key held during this capture, in order */
  char *seen_sym[16]; /* and the key symbols they typed */
  int nseen;
  char **syms; /* captured key symbols, as codes */
  char *accel;
} CaptureExt;

#define CPX(w) AWRA_EXT(w, CaptureExt)

static void strv_free(char **v, int n)
{
  for (int i = 0; i < n; i++) {
    free(v[i]);
  }
  free(v);
}

static AwraFont chip_font(void)
{
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.size *= 0.86f;
  f.weight = 650;
  return f;
}

static const char *capture_status(AwraWidget *w)
{
  CaptureExt *x = CPX(w);
  if (x->listening) {
    return x->nseen ? "\xE2\x80\xA6" : "Press keys\xE2\x80\xA6";
  }
  if (x->nlabels) {
    return "";
  }
  return x->placeholder ? x->placeholder : "Not set";
}

static void capture_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  CaptureExt *x = CPX(w);
  if (o == AWRA_ORIENTATION_VERTICAL) {
    *r_min = *r_nat = awra_px(24) + awra_px(12) + awra_px(2);
    return;
  }
  AwraFont cf = chip_font(), bf = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  float w_ = 0;
  for (int i = 0; i < x->nlabels; i++) {
    w_ += BK_MAX(awra_pxf(14), awra_text_width(&cf, x->labels[i], -1)) + awra_pxf(14) + awra_pxf(4);
  }
  const char *st = capture_status(w);
  if (st[0]) {
    w_ += awra_text_width(&bf, st, -1) + awra_pxf(6);
  }
  const int pad = awra_px(16) + awra_px(2);
  *r_min = BK_MAX(awra_px(96), (int)ceilf(w_)) + pad;
  *r_nat = *r_min;
}

static void capture_draw(AwraWidget *w, AwraDraw *d)
{
  CaptureExt *x = CPX(w);
  const AwraTokens *t = awra_tokens();
  AwraBox b = awra_box_of(w);
  const float r = awra_pxf(11);
  if (x->listening || awra_focus_visible(w)) {
    const float ow = awra_pxf(x->listening ? 2 : 1), off = awra_pxf(2);
    AwraBox o = {b.x - off - ow, b.y - off - ow, b.w + 2 * (off + ow), b.h + 2 * (off + ow)};
    awra_paint_rbox(d, o, r + off + ow, NULL, ow, &t->selection);
  }
  const AwraRGBA *border = x->listening ? &t->accent : awra_focus_visible(w) ? &t->focus_ring :
                           awra_hovered(w) ? &t->border : NULL;
  awra_paint_rbox(d, b, r, x->listening ? &t->accent_soft : &t->control_well,
                  border ? awra_pxf(1) : 0, border);
  if (!x->listening) { /* inset 0 1px 2px rgba(0,0,0,.20), inside the 1 px border */
    const AwraRGBA sh = {0, 0, 0, 0.20f};
    awra_paint_inset_shadow(d, awra_box_inset(b, awra_pxf(1)), r - awra_pxf(1), 0, awra_pxf(1), 0,
                            awra_pxf(2), &sh);
  }
  float cx = b.x + awra_pxf(9);
  const AwraFont cf = chip_font();
  const float ch = awra_font_line_height(&cf) + awra_pxf(2);
  const float cy = b.y + floorf((b.h - ch) * 0.5f + 0.5f);
  const bool sens = awra_sensitive(w);
  for (int i = 0; i < x->nlabels; i++) {
    const float tw = awra_text_width(&cf, x->labels[i], -1);
    const float cw = fmaxf(awra_pxf(14), tw) + awra_pxf(14);
    AwraBox chip = {cx, cy, cw, ch};
    const AwraRGBA drop = {0, 0, 0, 0.22f};
    awra_paint_shadow(d, chip, awra_pxf(6), 0, awra_pxf(1), 0, awra_pxf(1), &drop, true);
    awra_paint_rbox(d, chip, awra_pxf(6), &t->control_cap, 0, NULL);
    const float r4[4] = {awra_pxf(6), awra_pxf(6), awra_pxf(6), awra_pxf(6)},
                w4[4] = {awra_pxf(1), 0, 0, 0};
    awra_paint_rbox4(d, chip, r4, NULL, w4, &t->rim);
    awra_draw_text(d, &cf, cx + (cw - tw) * 0.5f, cy + awra_pxf(1) + awra_font_ascent(&cf),
                   x->labels[i], -1, sens ? &t->foreground : &t->disabled);
    cx += cw + awra_pxf(4);
  }
  const char *st = capture_status(w);
  if (st[0]) {
    AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
    if (x->listening) {
      f.weight = 600;
    }
    const float base = b.y + floorf((b.h - awra_font_line_height(&f)) * 0.5f + awra_font_ascent(&f) + 0.5f);
    awra_draw_text(d, &f, cx + awra_pxf(3), base, st, -1,
                   x->listening ? &t->accent_content : &t->muted);
  }
}

static void capture_set_listening(AwraWidget *w, bool on)
{
  CaptureExt *x = CPX(w);
  if (x->listening == on) {
    return;
  }
  x->listening = on;
  for (int i = 0; i < x->nseen; i++) {
    free(x->seen_sym[i]);
    x->seen_sym[i] = NULL;
  }
  x->nheld = x->nseen = 0;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "listening");
}

static void capture_finish(AwraWidget *w)
{
  CaptureExt *x = CPX(w);
  strv_free(x->codes, x->ncodes);
  strv_free(x->syms, x->ncodes);
  x->codes = (char **)calloc((size_t)x->nseen + 1, sizeof(char *));
  x->syms = (char **)calloc((size_t)x->nseen + 1, sizeof(char *));
  x->ncodes = 0;
  for (int i = 0; i < x->nseen; i++) {
    const char *name = awra_key_code_name(x->seen[i]);
    if (name) {
      x->syms[x->ncodes] = awra_strdup(x->seen_sym[i] ? x->seen_sym[i] : "");
      x->codes[x->ncodes++] = awra_strdup(name);
    }
  }
  /* default chips: the keys' labels */
  strv_free(x->labels, x->nlabels);
  x->labels = (char **)calloc((size_t)x->ncodes + 1, sizeof(char *));
  x->nlabels = x->ncodes;
  for (int i = 0; i < x->ncodes; i++) {
    x->labels[i] = awra_strdup(awra_key_code_label(x->codes[i]));
  }
  capture_set_listening(w, false);
  awra_emit_simple(w, AWRA_SIGNAL_CAPTURED);
}

static int capture_key(AwraWidget *w, BKEvent *e)
{
  CaptureExt *x = CPX(w);
  if (!x->listening) {
    if (bk_keysym_is(e->keysym, "Return") || bk_keysym_is(e->keysym, "space")) {
      capture_set_listening(w, true);
      return 1;
    }
    return 0;
  }
  const int code = BK_last_key_code();
  if (bk_keysym_is(e->keysym, "Escape") && x->nseen == 0) {
    capture_set_listening(w, false);
    return 1;
  }
  bool known = false;
  for (int i = 0; i < x->nheld; i++) {
    known |= x->held[i] == code;
  }
  if (!known && x->nheld < 16) {
    x->held[x->nheld++] = code;
  }
  bool seen = false;
  for (int i = 0; i < x->nseen; i++) {
    seen |= x->seen[i] == code;
  }
  if (!seen && x->nseen < 16) {
    x->seen_sym[x->nseen] = e->keysym ? awra_strdup(e->keysym) : NULL;
    x->seen[x->nseen++] = code;
  }
  /* without chords, a non-modifier key ends the capture */
  const char *name = awra_key_code_name(code);
  const bool modifier = name && (strstr(name, "Control") || strstr(name, "Shift") ||
                                 strstr(name, "Alt") || strstr(name, "Meta"));
  if (!x->allow_chords && !modifier) {
    capture_finish(w);
  }
  awra_invalidate(w);
  return 1;
}

static int capture_key_release(AwraWidget *w)
{
  CaptureExt *x = CPX(w);
  if (!x->listening) {
    return 0;
  }
  const int code = BK_last_key_code();
  for (int i = 0; i < x->nheld; i++) {
    if (x->held[i] == code) {
      x->held[i] = x->held[--x->nheld];
      break;
    }
  }
  if (x->nheld == 0 && x->nseen > 0) {
    capture_finish(w);
  }
  return 1;
}

static void capture_key_released(AwraWidget *w, const AwraArgs *a, void *data)
{
  if (capture_key_release(w)) {
    *a->handled = true;
  }
}

static void capture_activate(AwraWidget *w)
{
  capture_set_listening(w, !CPX(w)->listening);
}

static int capture_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside) {
    capture_activate(w);
  }
  return 1;
}

static void capture_finalize(AwraWidget *w)
{
  CaptureExt *x = CPX(w);
  free(x->placeholder);
  free(x->accel);
  for (int i = 0; i < x->nseen; i++) {
    free(x->seen_sym[i]);
  }
  strv_free(x->syms, x->ncodes);
  strv_free(x->codes, x->ncodes);
  strv_free(x->labels, x->nlabels);
}

static void capture_focus_lost(BKNode *n, bool confirm)
{
  capture_set_listening(n, false);
}

/* Awra 1's AwraShortcutCapture: a button showing the shortcut, pressed
 * while it listens */
static void capture_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  CaptureExt *x = CPX(w);
  const char *status = capture_status(w);
  info->label = status && status[0] ? status : x->accel;
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_PRESSED, x->listening);
}

static AwraClass capture_class = {
    .a11y = capture_a11y,
    .bk = AWRA_BK("AwraShortcutCapture", CLS_INTERACTIVE),
    .type = "AwraShortcutCapture",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(CaptureExt),
    .finalize = capture_finalize,
    .measure = capture_measure,
    .draw = capture_draw,
    .release = capture_release,
    .activate = capture_activate,
    .key = capture_key,
    .cursor = NULL,
    .overdraw = 5,
};

AwraWidget *awra_shortcut_capture_new(void)
{
  ((BKClass *)&capture_class.bk)->finish_edit = capture_focus_lost;
  AwraWidget *w = awra_widget_new(&capture_class);
  awra_widget_set_halign(w, AWRA_ALIGN_START);
  awra_widget_set_cursor(w, "pointer");
  awra_signal_connect(w, AWRA_SIGNAL_KEY_RELEASED, capture_key_released, NULL, NULL);
  return w;
}

void awra_shortcut_capture_set_accelerator(AwraWidget *w, const char *accel)
{
  awra_set_str(&CPX(w)->accel, accel);
}

const char *awra_shortcut_capture_accelerator(AwraWidget *w)
{
  return CPX(w)->accel;
}

void awra_shortcut_capture_set_placeholder(AwraWidget *w, const char *text)
{
  awra_set_str(&CPX(w)->placeholder, text);
  awra_widget_queue_resize(w);
}

void awra_shortcut_capture_start(AwraWidget *w)
{
  AwraWidget *win = awra_window_of(w);
  if (win) {
    awra_set_focus(win, w, false);
  }
  capture_set_listening(w, true);
}

bool awra_shortcut_capture_capturing(AwraWidget *w)
{
  return CPX(w)->listening;
}

void awra_shortcut_capture_set_allow_chords(AwraWidget *w, bool allow);
void awra_shortcut_capture_set_allow_chords(AwraWidget *w, bool allow)
{
  CPX(w)->allow_chords = allow;
}

void awra_shortcut_capture_clear(AwraWidget *w);
void awra_shortcut_capture_clear(AwraWidget *w)
{
  CaptureExt *x = CPX(w);
  strv_free(x->syms, x->ncodes);
  strv_free(x->codes, x->ncodes);
  strv_free(x->labels, x->nlabels);
  x->codes = x->labels = x->syms = NULL;
  x->ncodes = x->nlabels = 0;
  capture_set_listening(w, false);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

int awra_shortcut_capture_codes(AwraWidget *w, const char *const **r_codes);
int awra_shortcut_capture_codes(AwraWidget *w, const char *const **r_codes)
{
  *r_codes = (const char *const *)CPX(w)->codes;
  return CPX(w)->ncodes;
}

void awra_shortcut_capture_set_display_labels(AwraWidget *w, const char *const *labels, int n);
void awra_shortcut_capture_set_display_labels(AwraWidget *w, const char *const *labels, int n)
{
  CaptureExt *x = CPX(w);
  strv_free(x->labels, x->nlabels);
  x->labels = (char **)calloc((size_t)n + 1, sizeof(char *));
  x->nlabels = n;
  for (int i = 0; i < n; i++) {
    x->labels[i] = awra_strdup(labels[i]);
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

/* ----------------------------------------------------------------------- */
/* Editing by character offsets (GtkEditable, GtkTextBuffer) */

static int byte_of(const char *t, int chars)
{
  int i = 0;
  for (int c = 0; t[i] && c < chars; c++) {
    i++;
    while ((t[i] & 0xC0) == 0x80) {
      i++;
    }
  }
  return i;
}

static int chars_of(const char *t, int bytes)
{
  int c = 0;
  for (int i = 0; i < bytes && t[i]; i++) {
    if (((unsigned char)t[i] & 0xC0) != 0x80) {
      c++;
    }
  }
  return c;
}

int awra_editable_length(AwraWidget *w)
{
  const char *t = EX(w)->te->text;
  return chars_of(t, (int)strlen(t));
}

int awra_editable_cursor(AwraWidget *w)
{
  EditExt *x = EX(w);
  return chars_of(x->te->text, x->te->caret);
}

void awra_editable_set_cursor(AwraWidget *w, int offset)
{
  EditExt *x = EX(w);
  if (offset < 0) {
    offset = awra_editable_length(w);
  }
  x->te->caret = byte_of(x->te->text, offset);
  x->te->anchor = -1;
  ensure_caret_visible(w);
  restart_blink(w);
  caret_notify(w);
}

bool awra_editable_selection(AwraWidget *w, int *r_start, int *r_end)
{
  EditExt *x = EX(w);
  const int caret = chars_of(x->te->text, x->te->caret);
  if (x->te->anchor < 0 || x->te->anchor == x->te->caret) {
    *r_start = *r_end = caret;
    return false;
  }
  const int anchor = chars_of(x->te->text, x->te->anchor);
  *r_start = BK_MIN(caret, anchor);
  *r_end = BK_MAX(caret, anchor);
  return true;
}

void awra_editable_select(AwraWidget *w, int start, int end)
{
  EditExt *x = EX(w);
  const int n = awra_editable_length(w);
  if (start < 0) {
    start = n;
  }
  if (end < 0) {
    end = n;
  }
  x->te->anchor = byte_of(x->te->text, BK_MIN(start, n));
  x->te->caret = byte_of(x->te->text, BK_MIN(end, n));
  if (x->te->anchor == x->te->caret) {
    x->te->anchor = -1;
  }
  ensure_caret_visible(w);
  awra_invalidate(w);
  caret_notify(w);
}

static void splice_text(AwraWidget *w, int from, int to, const char *insert)
{
  EditExt *x = EX(w);
  char *before = awra_strdup(x->te->text);
  const int a = byte_of(x->te->text, from), b = byte_of(x->te->text, to);
  const int ins = insert ? (int)strlen(insert) : 0;
  const int n = x->te->len - (b - a) + ins;
  if (n + 1 > x->te->cap) {
    x->te->cap = n + 64;
    x->te->text = (char *)realloc(x->te->text, (size_t)x->te->cap);
  }
  memmove(x->te->text + a + ins, x->te->text + b, (size_t)(x->te->len - b + 1));
  if (ins) {
    memcpy(x->te->text + a, insert, (size_t)ins);
  }
  x->te->len = n;
  x->te->caret = a + ins;
  x->te->anchor = -1;
  after_edit(w, before);
  free(before);
  caret_notify(w);
}

void awra_editable_insert(AwraWidget *w, int offset, const char *text)
{
  if (offset < 0) {
    int s0, s1;
    if (awra_editable_selection(w, &s0, &s1)) {
      splice_text(w, s0, s1, text);
      return;
    }
    offset = awra_editable_cursor(w);
  }
  splice_text(w, offset, offset, text);
}

void awra_editable_delete(AwraWidget *w, int start, int end)
{
  const int n = awra_editable_length(w);
  if (end < 0 || end > n) {
    end = n;
  }
  if (start < 0) {
    start = 0;
  }
  if (start < end) {
    splice_text(w, start, end, NULL);
  }
}

/* A text view without a frame, its lines at most max_lines (composers). */
void awra_text_view_set_bare_internal(AwraWidget *w, int margin_x, int margin_y, int max_lines);
void awra_text_view_set_bare_internal(AwraWidget *w, int margin_x, int margin_y, int max_lines)
{
  EditExt *x = EX(w);
  x->bare = true;
  x->bare_mx = margin_x;
  x->bare_my = margin_y;
  x->max_lines = max_lines;
  x->caret_color = &awra_tokens()->accent_content;
  awra_widget_queue_resize(w);
}

/* The caret's box in the widget, logical px. */
void awra_editable_caret_rect(AwraWidget *w, double *r_x, double *r_y, double *r_h)
{
  EditExt *x = EX(w);
  AwraBox a = text_area(w);
  const int lw = x->multiline ? (int)a.w : -1;
  float cx, cy, ch;
  awra_text_layout_index_to_pos(x->layout, lw, shown_index(w, x->te->caret), &cx, &cy, &ch);
  const float s = awra_scale();
  *r_x = (text_x(w, a) - w->x + cx) / s;
  *r_y = (a.y - w->y + cy - (x->multiline ? x->scroll_y : 0)) / s;
  *r_h = ch / s;
}

int awra_shortcut_capture_keysyms(AwraWidget *w, const char *const **r_keysyms)
{
  *r_keysyms = (const char *const *)CPX(w)->syms;
  return CPX(w)->syms ? CPX(w)->ncodes : 0;
}

void awra_shortcut_capture_set_codes(AwraWidget *w, const char *const *codes, int n)
{
  CaptureExt *x = CPX(w);
  strv_free(x->syms, x->ncodes);
  strv_free(x->codes, x->ncodes);
  x->syms = NULL;
  x->codes = (char **)calloc((size_t)n + 1, sizeof(char *));
  x->ncodes = n;
  for (int i = 0; i < n; i++) {
    x->codes[i] = awra_strdup(codes[i]);
  }
  const char **labels = (const char **)calloc((size_t)n + 1, sizeof(char *));
  for (int i = 0; i < n; i++) {
    labels[i] = awra_key_code_label(codes[i]);
  }
  awra_shortcut_capture_set_display_labels(w, labels, n);
  free(labels);
}
