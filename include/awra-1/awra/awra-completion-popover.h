/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_COMPLETION_POPOVER (awra_completion_popover_get_type ())
/**
 * AwraCompletionPopover:
 *
 * Suggestions shown next to the caret of a text field while the user types:
 * people to mention, emoji shortcodes, commands.
 *
 * Attach it to a #GtkTextView or a #GtkEditable such as a #GtkEntry. The
 * application filters #AwraCompletionPopover:model as the text changes and
 * calls awra_completion_popover_present_at_cursor(). While the popover is
 * shown, the text keeps the focus and the popover takes Up and Down (which
 * move the selection and wrap around), Tab and Enter (which emit
 * #AwraCompletionPopover::activated) and Escape (which closes it and emits
 * #AwraCompletionPopover::dismissed). Hovering a row selects it and clicking
 * it activates it. At most eight rows show at once; the list is virtualized.
 *
 * The popover belongs to the text widget it is attached to and goes away with
 * it. It hides when the model becomes empty and when the text loses the
 * focus.
 *
 * Since: 1.1
 */
G_DECLARE_FINAL_TYPE (AwraCompletionPopover, awra_completion_popover, AWRA, COMPLETION_POPOVER, GtkPopover)

/**
 * awra_completion_popover_new:
 *
 * Returns: a new completion popover
 *
 * Since: 1.1
 */
GtkWidget          *awra_completion_popover_new                 (void);
/**
 * awra_completion_popover_get_model:
 * @self: a completion popover
 *
 * Returns: (transfer none) (nullable): the suggestions
 *
 * Since: 1.1
 */
GListModel         *awra_completion_popover_get_model           (AwraCompletionPopover *self);
/**
 * awra_completion_popover_set_model:
 * @self: a completion popover
 * @model: (nullable): the suggestions, already filtered for what is typed
 *
 * Since: 1.1
 */
void                awra_completion_popover_set_model           (AwraCompletionPopover *self,
                                                                 GListModel            *model);
/**
 * awra_completion_popover_get_factory:
 * @self: a completion popover
 *
 * Returns: (transfer none) (nullable): the factory that makes rows
 *
 * Since: 1.1
 */
GtkListItemFactory *awra_completion_popover_get_factory         (AwraCompletionPopover *self);
/**
 * awra_completion_popover_set_factory:
 * @self: a completion popover
 * @factory: (nullable): the factory that makes rows
 *
 * Since: 1.1
 */
void                awra_completion_popover_set_factory         (AwraCompletionPopover *self,
                                                                 GtkListItemFactory    *factory);
/**
 * awra_completion_popover_get_selected:
 * @self: a completion popover
 *
 * Returns: the position of the selected suggestion, or
 *   %GTK_INVALID_LIST_POSITION when there is none
 *
 * Since: 1.1
 */
guint               awra_completion_popover_get_selected        (AwraCompletionPopover *self);
/**
 * awra_completion_popover_set_selected:
 * @self: a completion popover
 * @position: the position of the suggestion to select
 *
 * Since: 1.1
 */
void                awra_completion_popover_set_selected        (AwraCompletionPopover *self,
                                                                 guint                  position);
/**
 * awra_completion_popover_get_attached_widget:
 * @self: a completion popover
 *
 * Returns: (transfer none) (nullable): the text widget it completes
 *
 * Since: 1.1
 */
GtkWidget          *awra_completion_popover_get_attached_widget (AwraCompletionPopover *self);
/**
 * awra_completion_popover_attach:
 * @self: a completion popover
 * @text_widget: (nullable): a #GtkTextView or a #GtkEditable, or %NULL to
 *   detach
 *
 * Attaches the popover to a text widget, which becomes its parent.
 *
 * Since: 1.1
 */
void                awra_completion_popover_attach              (AwraCompletionPopover *self,
                                                                 GtkWidget             *text_widget);
/**
 * awra_completion_popover_present_at_cursor:
 * @self: a completion popover
 *
 * Shows the popover above the line of the caret, or below it when there is
 * no room above, and selects the first suggestion. Calling it again as the
 * text changes follows the caret. Nothing is shown while the model is empty.
 *
 * Since: 1.1
 */
void                awra_completion_popover_present_at_cursor   (AwraCompletionPopover *self);

G_END_DECLS
