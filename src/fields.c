/* Awra 2 fields (Awra 1's AwraTextArea, AwraPasswordEntry, AwraTagEntry)
 * and the overlay bar (Awra 1's AwraOverlayBar: controls floating over
 * media, hidden while the pointer rests).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

/* ======================================================================= */
/* Text area: the framed multi-line text view */

AwraWidget *awra_text_area_new(void)
{
  AwraWidget *w = awra_text_view_new();
  awra_text_view_set_wrap(w, AWRA_WRAP_WORD);
  return w;
}

/* ======================================================================= */
/* Password entry: hidden text with a peek button at the end */

static void peek_clicked(AwraWidget *button, const AwraArgs *a, void *data)
{
  (void)a;
  AwraWidget *entry = (AwraWidget *)data;
  const bool shown = awra_widget_get_data(entry, "awra-peek") != NULL;
  awra_widget_set_data(entry, "awra-peek", shown ? NULL : (void *)1, NULL);
  awra_entry_set_visibility(entry, !shown);
  awra_button_set_icon_name(button, shown ? "view-reveal-symbolic" : "view-conceal-symbolic");
}

AwraWidget *awra_password_entry_new(void)
{
  AwraWidget *box = awra_overlay_new();
  awra_widget_set_name(box, "AwraPasswordEntry");
  AwraWidget *entry = awra_entry_new();
  awra_entry_set_visibility(entry, false);
  awra_overlay_set_child(box, entry);
  AwraWidget *peek = awra_button_new_from_icon_name("view-reveal-symbolic");
  awra_button_set_appearance(peek, AWRA_BUTTON_GHOST);
  awra_widget_set_halign(peek, AWRA_ALIGN_END);
  awra_widget_set_valign(peek, AWRA_ALIGN_CENTER);
  awra_widget_set_margin_end(peek, 4);
  awra_widget_set_tooltip(peek, "Show the text");
  awra_widget_set_visible(peek, false);
  awra_signal_connect(peek, AWRA_SIGNAL_CLICKED, peek_clicked, entry, NULL);
  awra_overlay_add_overlay(box, peek);
  awra_widget_set_data(box, "awra-entry", entry, NULL);
  awra_widget_set_data(box, "awra-peek-button", peek, NULL);
  awra_widget_unref(entry);
  awra_widget_unref(peek);
  return box;
}

AwraWidget *awra_password_entry_entry(AwraWidget *w)
{
  return (AwraWidget *)awra_widget_get_data(w, "awra-entry");
}

void awra_password_entry_set_show_peek_icon(AwraWidget *w, bool show)
{
  awra_widget_set_visible((AwraWidget *)awra_widget_get_data(w, "awra-peek-button"), show);
}

/* ======================================================================= */
/* Tag entry: chips of words, then a field that adds one on Return */

static void tag_removed(AwraWidget *chip, const AwraArgs *a, void *data)
{
  (void)a;
  AwraWidget *w = (AwraWidget *)data;
  AwraWidget *flow = (AwraWidget *)awra_widget_get_data(w, "awra-tags");
  char *text = awra_strdup(awra_chip_label(chip));
  awra_widget_ref(w);
  awra_container_remove(flow, chip);
  awra_emit_text(w, AWRA_SIGNAL_STOP_REQUESTED, text);
  awra_widget_unref(w);
  free(text);
}

bool awra_tag_entry_add(AwraWidget *w, const char *tag)
{
  AwraWidget *flow = (AwraWidget *)awra_widget_get_data(w, "awra-tags");
  if (!tag || !tag[0]) {
    return false;
  }
  for (int i = 0; i < flow->nkids; i++) {
    const char *l = awra_chip_label(flow->kids[i]);
    if (l && !strcmp(l, tag)) {
      return false;
    }
  }
  AwraWidget *chip = awra_chip_new(tag);
  awra_chip_set_icon_name(chip, "awra-close-symbolic");
  awra_widget_set_tooltip(chip, "Remove");
  awra_signal_connect(chip, AWRA_SIGNAL_CLICKED, tag_removed, w, NULL);
  awra_flow_box_append(flow, chip);
  awra_widget_unref(chip);
  return true;
}

void awra_tag_entry_clear(AwraWidget *w)
{
  awra_container_remove_all((AwraWidget *)awra_widget_get_data(w, "awra-tags"));
}

int awra_tag_entry_n_tags(AwraWidget *w)
{
  return ((AwraWidget *)awra_widget_get_data(w, "awra-tags"))->nkids;
}

const char *awra_tag_entry_tag(AwraWidget *w, int index)
{
  AwraWidget *flow = (AwraWidget *)awra_widget_get_data(w, "awra-tags");
  return index >= 0 && index < flow->nkids ? awra_chip_label(flow->kids[index]) : NULL;
}

static void tag_typed(AwraWidget *entry, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  char *text = awra_strdup(a->text ? a->text : "");
  if (text[0] && awra_tag_entry_add(w, text)) {
    awra_entry_set_text(entry, "");
    awra_emit_text(w, AWRA_SIGNAL_ACTIVATE, text);
  }
  free(text);
}

AwraWidget *awra_tag_entry_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_VERTICAL, 8);
  awra_widget_set_name(w, "AwraTagEntry");
  AwraWidget *flow = awra_flow_box_new();
  awra_flow_box_set_selection_mode(flow, AWRA_SELECTION_NONE);
  awra_flow_box_set_spacing(flow, 6, 6);
  AwraWidget *entry = awra_entry_new();
  awra_signal_connect(entry, AWRA_SIGNAL_ACTIVATE, tag_typed, w, NULL);
  awra_box_append(w, flow);
  awra_box_append(w, entry);
  awra_widget_set_data(w, "awra-tags", flow, NULL);
  awra_widget_set_data(w, "awra-entry", entry, NULL);
  awra_widget_unref(flow);
  awra_widget_unref(entry);
  return w;
}

AwraWidget *awra_tag_entry_entry(AwraWidget *w)
{
  return (AwraWidget *)awra_widget_get_data(w, "awra-entry");
}

/* ======================================================================= */
/* Overlay bar: content, and a bar floating at its foot on the Floating
 * material, revealed by the pointer and hidden after a rest */

typedef struct BarExt {
  AwraWidget *content, *surface, *revealer;
  bool auto_hide, inhibit;
  unsigned delay, timer;
  int max_width;
  int inset;
} BarExt;

#define BRX(w) AWRA_EXT(w, BarExt)

static void bar_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  BarExt *x = BRX(w);
  *mn = *nt = 0;
  if (x->content) {
    awra_measure(x->content, o, f, mn, nt);
  }
}

static void bar_allocate(AwraWidget *w)
{
  BarExt *x = BRX(w);
  if (x->content) {
    awra_allocate(x->content, w->x, w->y, w->w, w->h);
  }
  const int inset = awra_px(x->inset);
  int mn, nw, nh;
  awra_measure(x->revealer, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nw);
  int bw = w->w - 2 * inset;
  if (x->max_width > 0) {
    bw = BK_MIN(bw, awra_px(x->max_width));
  }
  bw = BK_MAX(bw, mn);
  awra_measure(x->revealer, AWRA_ORIENTATION_VERTICAL, bw, &mn, &nh);
  awra_allocate(x->revealer, w->x + (w->w - bw) / 2, w->y + w->h - nh - inset, bw, nh);
}

static bool bar_hide(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  BarExt *x = BRX(w);
  x->timer = 0;
  if (x->auto_hide && !x->inhibit && !awra_hovered(x->surface)) {
    awra_revealer_set_reveal_child(x->revealer, false);
  }
  return false;
}

void awra_overlay_bar_reveal(AwraWidget *w)
{
  BarExt *x = BRX(w);
  awra_revealer_set_reveal_child(x->revealer, true);
  if (x->timer) {
    awra_source_remove(x->timer);
    x->timer = 0;
  }
  if (x->auto_hide) {
    x->timer = awra_timeout_add(x->delay, bar_hide, w, NULL);
  }
}

static void bar_motion(AwraWidget *w, const AwraArgs *a, void *data)
{
  (void)w;
  (void)a;
  awra_overlay_bar_reveal((AwraWidget *)data);
}

static void bar_finalize(AwraWidget *w)
{
  if (BRX(w)->timer) {
    awra_source_remove(BRX(w)->timer);
  }
}

static AwraClass bar_class = {
    .bk = AWRA_BK("AwraOverlayBar", 0),
    .type = "AwraOverlayBar",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP,
    .ext_size = sizeof(BarExt),
    .finalize = bar_finalize,
    .measure = bar_measure,
    .allocate = bar_allocate,
};

AwraWidget *awra_overlay_bar_new(void)
{
  AwraWidget *w = awra_widget_new(&bar_class);
  BarExt *x = BRX(w);
  x->delay = 2200;
  x->inset = awra_spacing(AWRA_SPACING_MD);
  x->revealer = awra_revealer_new();
  awra_revealer_set_transition(x->revealer, AWRA_REVEALER_CROSSFADE, 180);
  awra_revealer_set_reveal_child(x->revealer, true);
  x->surface = awra_surface_new(AWRA_MATERIAL_FLOATING);
  awra_surface_set_padding(x->surface, 8, 12, 8, 12);
  awra_surface_set_elevation(x->surface, AWRA_ELEVATION_FLOATING);
  awra_revealer_set_child(x->revealer, x->surface);
  awra_widget_unref(x->surface);
  awra_child_insert(w, x->revealer, -1);
  awra_widget_unref(x->revealer);
  w->flags |= NF_EVENTS;
  awra_signal_connect(w, AWRA_SIGNAL_MOTION, bar_motion, w, NULL);
  return w;
}

void awra_overlay_bar_set_content(AwraWidget *w, AwraWidget *content)
{
  BarExt *x = BRX(w);
  if (x->content) {
    awra_child_remove(w, x->content);
  }
  x->content = content;
  if (content) {
    awra_child_insert(w, content, 0);
  }
}

void awra_overlay_bar_set_bar(AwraWidget *w, AwraWidget *bar)
{
  awra_surface_set_child(BRX(w)->surface, bar);
}

void awra_overlay_bar_set_auto_hide(AwraWidget *w, bool auto_hide)
{
  BRX(w)->auto_hide = auto_hide;
  awra_overlay_bar_reveal(w);
}

void awra_overlay_bar_set_inhibit_auto_hide(AwraWidget *w, bool inhibit)
{
  BRX(w)->inhibit = inhibit;
  if (inhibit) {
    awra_revealer_set_reveal_child(BRX(w)->revealer, true);
  }
  else {
    awra_overlay_bar_reveal(w);
  }
}

void awra_overlay_bar_set_revealed(AwraWidget *w, bool revealed)
{
  if (revealed) {
    awra_overlay_bar_reveal(w);
  }
  else {
    awra_revealer_set_reveal_child(BRX(w)->revealer, false);
  }
}

void awra_overlay_bar_set_hide_delay(AwraWidget *w, unsigned ms)
{
  BRX(w)->delay = ms;
}

void awra_overlay_bar_set_max_width(AwraWidget *w, int width)
{
  BRX(w)->max_width = width;
  awra_queue_allocate(w);
}

void awra_overlay_bar_set_inset(AwraWidget *w, AwraSpacing inset)
{
  BRX(w)->inset = awra_spacing(inset);
  awra_queue_allocate(w);
}

AwraWidget *awra_overlay_bar_surface(AwraWidget *w)
{
  return BRX(w)->surface;
}

int awra_overlay_bar_inset_px(AwraWidget *w)
{
  return BRX(w)->inset;
}
