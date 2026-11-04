/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * AwraTypingIndicator:
 *
 * Tells that someone is writing: three dots that rise in a wave, followed by
 * a short line such as "Ada is typing…".
 *
 * The indicator always keeps its height, shown or not, so that the content
 * around it never moves when it appears. It fades and slides in when it
 * becomes active and out when it stops. Under reduced motion the dots stay
 * still.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_TYPING_INDICATOR (awra_typing_indicator_get_type ())
G_DECLARE_FINAL_TYPE (AwraTypingIndicator, awra_typing_indicator, AWRA, TYPING_INDICATOR, GtkWidget)

/**
 * awra_typing_indicator_new:
 *
 * Returns: a new, inactive typing indicator
 *
 * Since: 1.1
 */
GtkWidget  *awra_typing_indicator_new        (void);
/**
 * awra_typing_indicator_get_label:
 * @self: a typing indicator
 *
 * Returns: (nullable): the line beside the dots
 *
 * Since: 1.1
 */
const char *awra_typing_indicator_get_label  (AwraTypingIndicator *self);
/**
 * awra_typing_indicator_set_label:
 * @self: a typing indicator
 * @label: (nullable): the line beside the dots, such as who is writing
 *
 * A long line is ellipsized.
 *
 * Since: 1.1
 */
void        awra_typing_indicator_set_label  (AwraTypingIndicator *self,
                                              const char          *label);
/**
 * awra_typing_indicator_get_active:
 * @self: a typing indicator
 *
 * Returns: whether the indicator is shown
 *
 * Since: 1.1
 */
gboolean    awra_typing_indicator_get_active (AwraTypingIndicator *self);
/**
 * awra_typing_indicator_set_active:
 * @self: a typing indicator
 * @active: whether someone is writing
 *
 * Since: 1.1
 */
void        awra_typing_indicator_set_active (AwraTypingIndicator *self,
                                              gboolean             active);

G_END_DECLS
