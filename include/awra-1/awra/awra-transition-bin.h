/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_TRANSITION_BIN (awra_transition_bin_get_type ())
G_DECLARE_FINAL_TYPE (AwraTransitionBin, awra_transition_bin, AWRA, TRANSITION_BIN, GtkWidget)

/**
 * awra_transition_bin_new:
 *
 * A bin that animates a change of content made in place. Call
 * awra_transition_bin_play() just before changing what the child shows: the
 * bin keeps the last frame, fades it out, and brings the new content in. It
 * suits content that is replaced rather than swapped between pages, such as a
 * list that receives another model, and follows the reduced-motion preference.
 *
 * Returns: a new transition bin
 *
 * Since: 1.1
 */
GtkWidget *awra_transition_bin_new       (void);
/**
 * awra_transition_bin_get_child:
 * @self: a transition bin
 *
 * Returns: (transfer none) (nullable): the child
 *
 * Since: 1.1
 */
GtkWidget *awra_transition_bin_get_child (AwraTransitionBin *self);
/**
 * awra_transition_bin_set_child:
 * @self: a transition bin
 * @child: (nullable): the child
 *
 * Since: 1.1
 */
void       awra_transition_bin_set_child (AwraTransitionBin *self,
                                          GtkWidget         *child);
/**
 * awra_transition_bin_get_fixed_top:
 * @self: a transition bin
 *
 * Returns: the height of the band that does not animate, in pixels
 *
 * Since: 1.1
 */
int        awra_transition_bin_get_fixed_top (AwraTransitionBin *self);
/**
 * awra_transition_bin_set_fixed_top:
 * @self: a transition bin
 * @height: the height of the band that does not animate, in pixels
 *
 * Keeps the top of the child in place, for a header shared by both contents.
 *
 * Since: 1.1
 */
void       awra_transition_bin_set_fixed_top (AwraTransitionBin *self,
                                              int                height);
/**
 * awra_transition_bin_play:
 * @self: a transition bin
 * @offset_x: where the new content starts horizontally, in pixels
 * @offset_y: where the new content starts vertically, in pixels
 *
 * Captures the current frame and starts the transition. The old frame leaves
 * towards the opposite side. Offsets of 0 give a plain crossfade. Playing again
 * during a transition continues from what is on screen.
 *
 * Since: 1.1
 */
void       awra_transition_bin_play      (AwraTransitionBin *self,
                                          double             offset_x,
                                          double             offset_y);

G_END_DECLS
