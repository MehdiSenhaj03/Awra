/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * AwraTaskRow:
 *
 * A backend-independent background task presentation with identity, progress
 * and application-supplied actions. No task scheduling or I/O is performed.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_TASK_ROW (awra_task_row_get_type ())
G_DECLARE_FINAL_TYPE (AwraTaskRow, awra_task_row, AWRA, TASK_ROW, GtkWidget)

/**
 * awra_task_row_new:
 * @title: (nullable): the task title
 *
 * Returns: (transfer floating): a new task row
 * Since: 1.1
 */
GtkWidget *awra_task_row_new (const char *title);

/**
 * awra_task_row_get_title:
 * @self: a task row
 *
 * Returns: (transfer none) (nullable): the title
 * Since: 1.1
 */
const char *awra_task_row_get_title (AwraTaskRow *self);
/**
 * awra_task_row_set_title:
 * @self: a task row
 * @value: (nullable): the task title
 *
 * Since: 1.1
 */
void awra_task_row_set_title (AwraTaskRow *self, const char *value);

/**
 * awra_task_row_get_subtitle:
 * @self: a task row
 *
 * Returns: (transfer none) (nullable): the subtitle
 * Since: 1.1
 */
const char *awra_task_row_get_subtitle (AwraTaskRow *self);
/**
 * awra_task_row_set_subtitle:
 * @self: a task row
 * @value: (nullable): status or secondary text
 *
 * Since: 1.1
 */
void awra_task_row_set_subtitle (AwraTaskRow *self, const char *value);

/**
 * awra_task_row_get_icon_name:
 * @self: a task row
 *
 * Returns: (transfer none) (nullable): the icon name
 * Since: 1.1
 */
const char *awra_task_row_get_icon_name (AwraTaskRow *self);
/**
 * awra_task_row_set_icon_name:
 * @self: a task row
 * @value: (nullable): a themed icon name
 *
 * Since: 1.1
 */
void awra_task_row_set_icon_name (AwraTaskRow *self, const char *value);

/**
 * awra_task_row_get_fraction:
 * @self: a task row
 *
 * Returns: the fraction
 * Since: 1.1
 */
double awra_task_row_get_fraction (AwraTaskRow *self);
/**
 * awra_task_row_set_fraction:
 * @self: a task row
 * @value: progress between 0 and 1; out-of-range values are clamped
 *
 * Since: 1.1
 */
void awra_task_row_set_fraction (AwraTaskRow *self, double value);

/**
 * awra_task_row_get_indeterminate:
 * @self: a task row
 *
 * Returns: the indeterminate
 * Since: 1.1
 */
gboolean awra_task_row_get_indeterminate (AwraTaskRow *self);
/**
 * awra_task_row_set_indeterminate:
 * @self: a task row
 * @value: whether duration is unknown; shows a spinner instead of the bar
 *
 * Since: 1.1
 */
void awra_task_row_set_indeterminate (AwraTaskRow *self, gboolean value);

/**
 * awra_task_row_get_show_progress:
 * @self: a task row
 *
 * Returns: the show progress
 * Since: 1.1
 */
gboolean awra_task_row_get_show_progress (AwraTaskRow *self);
/**
 * awra_task_row_set_show_progress:
 * @self: a task row
 * @value: whether progress (determinate or indeterminate) is visible
 *
 * Since: 1.1
 */
void awra_task_row_set_show_progress (AwraTaskRow *self, gboolean value);

/**
 * awra_task_row_get_suffix:
 * @self: a task row
 *
 * Returns: (transfer none) (nullable): the suffix
 * Since: 1.1
 */
GtkWidget *awra_task_row_get_suffix (AwraTaskRow *self);
/**
 * awra_task_row_set_suffix:
 * @self: a task row
 * @value: (nullable): unparented action widget, or %NULL
 *
 * Since: 1.1
 */
void awra_task_row_set_suffix (AwraTaskRow *self, GtkWidget *value);

G_END_DECLS
