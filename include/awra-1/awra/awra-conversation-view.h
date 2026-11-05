/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>
#include <awra/awra-enums.h>

G_BEGIN_DECLS

#define AWRA_TYPE_CONVERSATION_VIEW (awra_conversation_view_get_type ())
/**
 * AwraConversationView:
 *
 * A virtualized timeline that grows at the bottom: the messages of a chat, the
 * replies of a comment thread, the lines of a log or a console.
 *
 * The view starts at the end of its model and follows it: items appended while
 * the end is in view glide into place, and only those rise in. Scrolling up
 * stops following; reaching the end again resumes it. Items inserted at the
 * start (older history) never move what is on screen. Away from the end, a
 * pill at the bottom offers to jump back to the newest items and can count
 * unread ones.
 *
 * Rows come from #AwraConversationView:factory, which must be a
 * #GtkSignalListItemFactory or a #GtkBuilderListItemFactory. When the model
 * implements #GtkSectionModel, #AwraConversationView:header-factory adds a
 * header above each section, such as a day separator.
 *
 * Page Up, Page Down, Home and End scroll the view while it holds the focus.
 *
 * Since: 1.1
 */
G_DECLARE_FINAL_TYPE (AwraConversationView, awra_conversation_view, AWRA, CONVERSATION_VIEW, GtkWidget)

/**
 * awra_conversation_view_new:
 * @model: (nullable) (transfer full): the items
 * @factory: (nullable) (transfer full): the factory that makes rows
 *
 * Returns: a new conversation view
 *
 * Since: 1.1
 */
GtkWidget          *awra_conversation_view_new                (GListModel         *model,
                                                               GtkListItemFactory *factory);
/**
 * awra_conversation_view_get_model:
 * @self: a conversation view
 *
 * Returns: (transfer none) (nullable): the items
 *
 * Since: 1.1
 */
GListModel         *awra_conversation_view_get_model          (AwraConversationView *self);
/**
 * awra_conversation_view_set_model:
 * @self: a conversation view
 * @model: (nullable): the items
 *
 * Replaces the items. The view shows the end of the new model.
 *
 * Since: 1.1
 */
void                awra_conversation_view_set_model          (AwraConversationView *self,
                                                               GListModel           *model);
/**
 * awra_conversation_view_get_factory:
 * @self: a conversation view
 *
 * Returns: (transfer none) (nullable): the factory that makes rows
 *
 * Since: 1.1
 */
GtkListItemFactory *awra_conversation_view_get_factory        (AwraConversationView *self);
/**
 * awra_conversation_view_set_factory:
 * @self: a conversation view
 * @factory: (nullable): a #GtkSignalListItemFactory or a
 *   #GtkBuilderListItemFactory
 *
 * Since: 1.1
 */
void                awra_conversation_view_set_factory        (AwraConversationView *self,
                                                               GtkListItemFactory   *factory);
/**
 * awra_conversation_view_get_header_factory:
 * @self: a conversation view
 *
 * Returns: (transfer none) (nullable): the factory that makes section headers
 *
 * Since: 1.1
 */
GtkListItemFactory *awra_conversation_view_get_header_factory (AwraConversationView *self);
/**
 * awra_conversation_view_set_header_factory:
 * @self: a conversation view
 * @factory: (nullable): the factory that makes section headers, for a model
 *   that implements #GtkSectionModel
 *
 * Since: 1.1
 */
void                awra_conversation_view_set_header_factory (AwraConversationView *self,
                                                               GtkListItemFactory   *factory);
/**
 * awra_conversation_view_get_delegate:
 * @self: a conversation view
 *
 * Returns: (transfer none): the underlying GTK list view
 *
 * Since: 1.1
 */
GtkListView        *awra_conversation_view_get_delegate       (AwraConversationView *self);
/**
 * awra_conversation_view_get_at_bottom:
 * @self: a conversation view
 *
 * Returns: whether the end of the timeline is in view, or the view is gliding
 *   to it
 *
 * Since: 1.1
 */
gboolean            awra_conversation_view_get_at_bottom      (AwraConversationView *self);
/**
 * awra_conversation_view_get_follow:
 * @self: a conversation view
 *
 * Returns: whether the view keeps the newest items in view
 *
 * Since: 1.1
 */
gboolean            awra_conversation_view_get_follow         (AwraConversationView *self);
/**
 * awra_conversation_view_set_follow:
 * @self: a conversation view
 * @follow: whether the view keeps the newest items in view
 *
 * Following is on by default. The user turns it off by scrolling away from
 * the end and on again by reaching it.
 *
 * Since: 1.1
 */
void                awra_conversation_view_set_follow         (AwraConversationView *self,
                                                               gboolean              follow);
/**
 * awra_conversation_view_get_unread_count:
 * @self: a conversation view
 *
 * Returns: the number of items the jump pill announces
 *
 * Since: 1.1
 */
guint               awra_conversation_view_get_unread_count   (AwraConversationView *self);
/**
 * awra_conversation_view_set_unread_count:
 * @self: a conversation view
 * @count: the number of unread items, or 0
 *
 * Sets the count shown by the jump pill, such as "3 new messages". The
 * application decides what is unread and resets the count.
 *
 * Since: 1.1
 */
void                awra_conversation_view_set_unread_count   (AwraConversationView *self,
                                                               guint                 count);
/**
 * awra_conversation_view_get_jump_label:
 * @self: a conversation view
 *
 * Returns: (transfer none) (nullable): the label that replaces the jump pill's
 *   own
 *
 * Since: 1.1
 */
const char         *awra_conversation_view_get_jump_label     (AwraConversationView *self);
/**
 * awra_conversation_view_set_jump_label:
 * @self: a conversation view
 * @label: (nullable): a label for the jump pill, or %NULL for the default
 *   one, which counts unread items
 *
 * Since: 1.1
 */
void                awra_conversation_view_set_jump_label     (AwraConversationView *self,
                                                               const char           *label);
/**
 * awra_conversation_view_get_edge_threshold:
 * @self: a conversation view
 *
 * Returns: the distance from an end at which #AwraConversationView::edge-reached
 *   is emitted, in pixels, or -1 for the height of the view
 *
 * Since: 1.1
 */
int                 awra_conversation_view_get_edge_threshold (AwraConversationView *self);
/**
 * awra_conversation_view_set_edge_threshold:
 * @self: a conversation view
 * @threshold: a distance in pixels, or -1 for the height of the view
 *
 * Since: 1.1
 */
void                awra_conversation_view_set_edge_threshold (AwraConversationView *self,
                                                               int                   threshold);
/**
 * awra_conversation_view_scroll_to_end:
 * @self: a conversation view
 * @animate: whether to glide there
 *
 * Shows the newest items and follows them. From far away, the view first
 * moves close to the end so the glide stays short. The glide follows the
 * reduced-motion preference.
 *
 * Since: 1.1
 */
void                awra_conversation_view_scroll_to_end      (AwraConversationView *self,
                                                               gboolean              animate);
/**
 * awra_conversation_view_scroll_to_item:
 * @self: a conversation view
 * @position: the position of the item
 * @align: where the item ends up
 * @highlight: whether to flash the item briefly
 *
 * Brings an item into view, gliding when it is near, and stops following the
 * end. A highlight fades out in about a second and a half, which helps the
 * eye find a reply's original message or a search result.
 *
 * Since: 1.1
 */
void                awra_conversation_view_scroll_to_item     (AwraConversationView  *self,
                                                               guint                  position,
                                                               AwraConversationAlign  align,
                                                               gboolean               highlight);
/**
 * awra_conversation_view_get_visible_range:
 * @self: a conversation view
 * @first: (out) (optional): the position of the first visible item
 * @last: (out) (optional): the position of the last visible item
 *
 * Returns: %TRUE when at least one item is visible
 *
 * Since: 1.1
 */
gboolean            awra_conversation_view_get_visible_range  (AwraConversationView *self,
                                                               guint                *first,
                                                               guint                *last);
/**
 * awra_conversation_view_get_edge_status:
 * @self: an Awra conversation view
 * @edge: %GTK_POS_TOP or %GTK_POS_BOTTOM
 *
 * Returns: what is happening at @edge
 *
 * Since: 1.1
 */
AwraEdgeStatus awra_conversation_view_get_edge_status (AwraConversationView *self,
                                                        GtkPositionType       edge);
/**
 * awra_conversation_view_set_edge_status:
 * @self: an Awra conversation view
 * @edge: %GTK_POS_TOP for older items, %GTK_POS_BOTTOM for newer ones
 * @status: what is happening there
 * @label: (nullable): what to say, such as "Loading earlier messages…",
 *   or %NULL to show only a spinner or a retry
 *
 * Shows a small floating status at @edge while more items load, or after
 * loading them failed, with a retry that emits
 * #AwraConversationView::retry. While an edge isn't idle,
 * #AwraConversationView::edge-reached isn't emitted for it, so a failed
 * load waits to be retried instead of being asked for again on every
 * scroll.
 *
 * Since: 1.1
 */
void           awra_conversation_view_set_edge_status (AwraConversationView *self,
                                                        GtkPositionType       edge,
                                                        AwraEdgeStatus        status,
                                                        const char           *label);

G_END_DECLS
