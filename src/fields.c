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
