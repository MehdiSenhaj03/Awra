/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Set on a text widget while an AwraCompletionPopover attached to it is shown,
 * so other Awra widgets leave Enter, Tab and the arrows to the completion. */
#define AWRA_COMPLETION_ACTIVE_KEY "awra-completion-popover-active"

static inline gboolean
awra_completion_popover_is_active_for (GtkWidget *text_widget)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (text_widget),
                                             AWRA_COMPLETION_ACTIVE_KEY));
}

G_END_DECLS
