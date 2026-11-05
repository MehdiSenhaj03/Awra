/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <gtk/gtk.h>
#include <awra/awra-popover.h>

G_BEGIN_DECLS

/**
 * AwraTimeline:
 *
 * Since: 1.1
 */
#define AWRA_TYPE_TIMELINE (awra_timeline_get_type ())
G_DECLARE_FINAL_TYPE (AwraTimeline, awra_timeline, AWRA, TIMELINE, GtkWidget)

/**
 * awra_timeline_new:
 *
 * Returns: (transfer floating): a backend-independent time ruler
 *
 * Since: 1.1
 */
GtkWidget *awra_timeline_new (void);
/**
 * awra_timeline_get_duration:
 *
 * Since: 1.1
 */
double awra_timeline_get_duration (AwraTimeline *self);
/**
 * awra_timeline_set_duration:
 *
 * Since: 1.1
 */
void awra_timeline_set_duration (AwraTimeline *self, double duration);
/**
 * awra_timeline_get_position:
 *
 * Since: 1.1
 */
double awra_timeline_get_position (AwraTimeline *self);
/**
 * awra_timeline_set_position:
 *
 * Since: 1.1
 */
void awra_timeline_set_position (AwraTimeline *self, double position);
/**
 * awra_timeline_get_step_increment:
 * @self: a timeline
 *
 * Returns: the arrow-key step in seconds; defaults to 1
 *
 * Since: 1.1
 */
double awra_timeline_get_step_increment (AwraTimeline *self);
/**
 * awra_timeline_set_step_increment:
 * @self: a timeline
 * @increment: positive seconds per arrow-key step
 *
 * Invalid values reset the step to 1 second. Does not request a seek.
 *
 * Since: 1.1
 */
void awra_timeline_set_step_increment (AwraTimeline *self, double increment);
/**
 * awra_timeline_get_page_increment:
 * @self: a timeline
 *
 * Returns: the Shift+arrow step in seconds; defaults to 10
 *
 * Since: 1.1
 */
double awra_timeline_get_page_increment (AwraTimeline *self);
/**
 * awra_timeline_set_page_increment:
 * @self: a timeline
 * @increment: positive seconds per Shift+arrow step
 *
 * Invalid values reset the step to 10 seconds. Does not request a seek.
 *
 * Since: 1.1
 */
void awra_timeline_set_page_increment (AwraTimeline *self, double increment);
/**
 * awra_timeline_get_view_start:
 *
 * Since: 1.1
 */
double awra_timeline_get_view_start (AwraTimeline *self);
/**
 * awra_timeline_set_view_start:
 *
 * Since: 1.1
 */
void awra_timeline_set_view_start (AwraTimeline *self, double start);
/**
 * awra_timeline_get_view_duration:
 *
 * Since: 1.1
 */
double awra_timeline_get_view_duration (AwraTimeline *self);
/**
 * awra_timeline_set_view_duration:
 *
 * Since: 1.1
 */
void awra_timeline_set_view_duration (AwraTimeline *self, double duration);
/**
 * awra_timeline_get_offset:
 *
 * Since: 1.1
 */
double awra_timeline_get_offset (AwraTimeline *self);
/**
 * awra_timeline_set_offset:
 *
 * Since: 1.1
 */
void awra_timeline_set_offset (AwraTimeline *self, double offset);
/**
 * awra_timeline_get_editable:
 *
 * Since: 1.1
 */
gboolean awra_timeline_get_editable (AwraTimeline *self);
/**
 * awra_timeline_set_editable:
 *
 * Since: 1.1
 */
void awra_timeline_set_editable (AwraTimeline *self, gboolean editable);
/**
 * awra_timeline_get_hover_time:
 *
 * Since: 1.1
 */
double awra_timeline_get_hover_time (AwraTimeline *self);
/**
 * awra_timeline_get_hover_x:
 *
 * Since: 1.1
 */
double awra_timeline_get_hover_x (AwraTimeline *self);
/**
 * awra_timeline_set_preview:
 * @self: a timeline
 * @preview: (nullable): backend-provided preview content; Awra owns anchoring
 *
 * The preview follows pointer and drag coordinates, including RTL and inset,
 * and disappears on leave/unmap. Updating its content never requests a seek.
 *
 * Since: 1.1
 */
void awra_timeline_set_preview (AwraTimeline *self, GtkWidget *preview);
/**
 * awra_timeline_get_preview:
 * @self: a timeline
 * Returns: (transfer none) (nullable): the supplied preview content
 *
 * Since: 1.1
 */
GtkWidget *awra_timeline_get_preview (AwraTimeline *self);
/**
 * awra_timeline_get_preview_popover:
 * @self: a timeline
 * Returns: (transfer none): the framework-owned preview popover
 *
 * Since: 1.1
 */
AwraPopover *awra_timeline_get_preview_popover (AwraTimeline *self);
/**
 * awra_timeline_add_interval:
 * @self: a timeline
 * @start: interval start in seconds, before offset
 * @end: interval end in seconds; equal to start for a point marker
 * @label: (nullable): plain text for the interval
 *
 * Adds a clip, cue, chapter or bookmark. Intervals are copied, may overlap,
 * and are clipped to the viewport. No decoder or file format is implied.
 *
 * Since: 1.1
 */
void awra_timeline_add_interval (AwraTimeline *self, double start, double end,
                                 const char *label);
/**
 * awra_timeline_clear_intervals:
 *
 * Since: 1.1
 */
void awra_timeline_clear_intervals (AwraTimeline *self);
/**
 * awra_timeline_get_n_intervals:
 *
 * Since: 1.1
 */
guint awra_timeline_get_n_intervals (AwraTimeline *self);

G_END_DECLS
