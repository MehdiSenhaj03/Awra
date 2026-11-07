/* AwraMetricRow's setters (Awra 1: awra_metric_row_set_label, _set_value,
 * _set_icon_name): the row built by awra_metric_row_new (chips.c) holds an
 * icon, its label and its value, in that order. */

#include "awra-private.h"

static AwraWidget *metric_part(AwraWidget *row, int index)
{
  if (!row || row->nkids < 3) {
    return NULL;
  }
  AwraWidget *k = row->kids[index];
  return awra_is_widget(k) ? k : NULL;
}

void awra_metric_row_set_label(AwraWidget *row, const char *label)
{
  AwraWidget *l = metric_part(row, 1);
  if (l) {
    awra_label_set_text(l, label ? label : "");
  }
}

void awra_metric_row_set_value(AwraWidget *row, const char *value)
{
  AwraWidget *v = metric_part(row, 2);
  if (v) {
    awra_label_set_text(v, value ? value : "");
    awra_widget_set_tooltip(v, value);
  }
}

void awra_metric_row_set_icon_name(AwraWidget *row, const char *icon_name)
{
  AwraWidget *i = metric_part(row, 0);
  if (i) {
    awra_image_set_icon_name(i, icon_name);
    awra_widget_set_visible(i, icon_name && icon_name[0]);
  }
}
