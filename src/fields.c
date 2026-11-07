/* Awra 2 fields (Awra 1's AwraTextArea, AwraPasswordEntry, AwraTagEntry);
 * the overlay bar is in video.c.
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

void awra_entry_set_peek_icon_internal(AwraWidget *w, bool peek);

AwraWidget *awra_password_entry_new(void)
{
  AwraWidget *box = awra_overlay_new();
  awra_widget_set_name(box, "AwraPasswordEntry");
  AwraWidget *entry = awra_entry_new();
  awra_entry_set_visibility(entry, false);
  /* the peek icon shows by default, as Awra 1's password entry has it */
  awra_entry_set_peek_icon_internal(entry, true);
  awra_overlay_set_child(box, entry);
  awra_widget_set_data(box, "awra-entry", entry, NULL);
  awra_widget_unref(entry);
  return box;
}

AwraWidget *awra_password_entry_entry(AwraWidget *w)
{
  return (AwraWidget *)awra_widget_get_data(w, "awra-entry");
}

void awra_password_entry_set_show_peek_icon(AwraWidget *w, bool show)
{
  awra_entry_set_peek_icon_internal(awra_password_entry_entry(w), show);
}

/* ======================================================================= */
/* Tag entry (Awra 1's AwraTagEntry): one well (awra.css awratagentry:
 * padding 5px, a 1 px border, transparent or @awra_focus while it holds
 * the focus, radius 11, @awra_well with an inset 0 1px 2px shadow) holding
 * a column 4 px apart: the tags, buttons "tag  ×" that remove themselves,
 * flowing 6 px apart; then a frameless field, 22 px tall with 4 px each
 * side, that adds one on Return. */

void awra_entry_set_inner_internal(AwraWidget *w, int pad_x, int min_h);
void awra_button_set_metrics_internal(AwraWidget *w, int pad_x, int pad_y, int min_w, int min_h);
void awra_button_set_tag_internal(AwraWidget *w);

typedef struct TagExt {
  AwraWidget *column, *flow, *entry;
} TagExt;
#define TGX(w) AWRA_EXT(w, TagExt)
#define TAG_INSET 6 /* the border and the padding */

static void tags_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  const int pad = 2 * awra_px(TAG_INSET);
  int mn = 0, nt = 0;
  awra_measure(TGX(w)->column, o, for_size >= 0 ? BK_MAX(0, for_size - pad) : -1, &mn, &nt);
  *r_min = mn + pad;
  *r_nat = nt + pad;
}

static void tags_allocate(AwraWidget *w)
{
  const int in = awra_px(TAG_INSET);
  awra_allocate(TGX(w)->column, w->x + in, w->y + in, BK_MAX(0, w->w - 2 * in), BK_MAX(0, w->h - 2 * in));
}

static void tags_draw(AwraWidget *w, AwraDraw *d)
{
  const AwraTokens *t = awra_tokens();
  const AwraBox b = awra_box_of(w);
  const float r = awra_pxf(11), bd = awra_pxf(1);
  awra_paint_rbox(d, b, r, &t->control_well, 0, NULL);
  const AwraBox pb = {b.x + bd, b.y + bd, b.w - 2 * bd, b.h - 2 * bd};
  const AwraRGBA sh = {0, 0, 0, 0.20f};
  awra_paint_inset_shadow(d, pb, r - bd, 0, awra_pxf(1), 0, awra_pxf(2), &sh);
  if (awra_widget_has_focus(TGX(w)->entry)) { /* awratagentry:focus-within */
    awra_paint_rbox(d, b, r, NULL, bd, &t->focus_ring);
  }
}

static AwraClass tags_class = {
    .bk = AWRA_BK("AwraTagEntry", 0),
    .type = "AwraTagEntry",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(TagExt),
    .measure = tags_measure,
    .allocate = tags_allocate,
    .draw = tags_draw,
};

static void tag_clicked(AwraWidget *button, const AwraArgs *a, void *data)
{
  (void)a;
  AwraWidget *w = (AwraWidget *)data;
  char *text = awra_strdup((const char *)awra_widget_get_data(button, "awra-tag-value"));
  awra_widget_ref(w);
  awra_container_remove(TGX(w)->flow, button);
  awra_emit_text(w, AWRA_SIGNAL_STOP_REQUESTED, text);
  awra_widget_unref(w);
  free(text);
}

static bool is_tags(AwraWidget *w)
{
  return w && awra_widget_is(w, "AwraTagEntry");
}

bool awra_tag_entry_add(AwraWidget *w, const char *tag)
{
  if (!is_tags(w) || !tag) {
    return false;
  }
  /* g_strstrip: no blank tags, none twice */
  while (*tag == ' ' || *tag == '\t' || *tag == '\n') {
    tag++;
  }
  size_t n = strlen(tag);
  while (n > 0 && (tag[n - 1] == ' ' || tag[n - 1] == '\t' || tag[n - 1] == '\n')) {
    n--;
  }
  if (n == 0) {
    return false;
  }
  char *value = (char *)malloc(n + 1);
  memcpy(value, tag, n);
  value[n] = 0;
  AwraWidget *flow = TGX(w)->flow;
  for (int i = 0; i < flow->nkids; i++) {
    const char *v = (const char *)awra_widget_get_data(flow->kids[i], "awra-tag-value");
    if (v && !strcmp(v, value)) {
      free(value);
      return false;
    }
  }
  char *label = (char *)malloc(n + 8);
  snprintf(label, n + 8, "%s  \xC3\x97", value);
  AwraWidget *button = awra_button_new_with_label(label);
  free(label);
  awra_button_set_tag_internal(button);
  awra_widget_set_data(button, "awra-tag-value", value, free);
  awra_signal_connect(button, AWRA_SIGNAL_CLICKED, tag_clicked, w, NULL);
  awra_flow_box_append(flow, button);
  awra_widget_unref(button);
  return true;
}

void awra_tag_entry_clear(AwraWidget *w)
{
  if (is_tags(w)) {
    awra_container_remove_all(TGX(w)->flow);
  }
}

int awra_tag_entry_n_tags(AwraWidget *w)
{
  return is_tags(w) ? TGX(w)->flow->nkids : 0;
}

const char *awra_tag_entry_tag(AwraWidget *w, int index)
{
  if (!is_tags(w)) {
    return NULL;
  }
  AwraWidget *flow = TGX(w)->flow;
  return index >= 0 && index < flow->nkids ?
             (const char *)awra_widget_get_data(flow->kids[index], "awra-tag-value") :
             NULL;
}

static void tag_typed(AwraWidget *entry, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  char *text = awra_strdup(a->text ? a->text : "");
  if (text[0] && awra_tag_entry_add(w, text)) {
    awra_entry_set_text(entry, "");
    AwraWidget *flow = TGX(w)->flow;
    awra_emit_text(w, AWRA_SIGNAL_ACTIVATE,
                   (const char *)awra_widget_get_data(flow->kids[flow->nkids - 1], "awra-tag-value"));
  }
  free(text);
}

static void tags_focus(AwraWidget *entry, const AwraArgs *a, void *data)
{
  (void)entry;
  if (a->text && !strcmp(a->text, "has-focus")) {
    awra_invalidate((AwraWidget *)data); /* the focus ring */
  }
}

AwraWidget *awra_tag_entry_new(void)
{
  AwraWidget *w = awra_widget_new(&tags_class);
  TagExt *x = TGX(w);
  x->column = awra_box_new(AWRA_ORIENTATION_VERTICAL, 4);
  x->flow = awra_flow_box_new();
  awra_flow_box_set_selection_mode(x->flow, AWRA_SELECTION_NONE);
  awra_flow_box_set_spacing(x->flow, 6, 6);
  awra_flow_box_set_child_padding(x->flow, 0); /* awra.css: none in a tag entry */
  x->entry = awra_entry_new();
  awra_entry_set_inner_internal(x->entry, 8, 22); /* 0 4px on the entry and its text */
  awra_entry_set_placeholder(x->entry, "Add a tag\xE2\x80\xA6");
  awra_signal_connect(x->entry, AWRA_SIGNAL_ACTIVATE, tag_typed, w, NULL);
  awra_signal_connect(x->entry, AWRA_SIGNAL_NOTIFY, tags_focus, w, NULL);
  awra_box_append(x->column, x->flow);
  awra_box_append(x->column, x->entry);
  awra_child_insert(w, x->column, -1);
  awra_widget_unref(x->flow);
  awra_widget_unref(x->entry);
  awra_widget_unref(x->column);
  return w;
}

AwraWidget *awra_tag_entry_entry(AwraWidget *w)
{
  return is_tags(w) ? TGX(w)->entry : NULL;
}
