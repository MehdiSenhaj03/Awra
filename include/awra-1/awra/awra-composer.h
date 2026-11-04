/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_COMPOSER (awra_composer_get_type ())
/**
 * AwraComposer:
 *
 * The field where a message or a comment is written, at the end of a
 * conversation.
 *
 * One rounded well holds, from top to bottom, an optional banner (such as
 * "Replying to Ada"), an optional attachments widget, and a row with an
 * optional start widget, the text, an optional end widget and a send button.
 * The text grows from one line up to #AwraComposer:max-lines, then scrolls;
 * the well follows its height smoothly. The send button appears when there is
 * something to send.
 *
 * Enter emits #AwraComposer::submit and Shift+Enter inserts a new line, unless
 * #AwraComposer:send-on-enter is off. Enter never submits while an input method
 * is composing or an #AwraCompletionPopover attached to the text is shown. The
 * application reads the text in its handler and clears it.
 *
 * awra_composer_get_view() gives the #GtkTextView itself, for tags, child
 * anchors or more key handling.
 *
 * Since: 1.1
 */
G_DECLARE_FINAL_TYPE (AwraComposer, awra_composer, AWRA, COMPOSER, GtkWidget)

/**
 * awra_composer_new:
 *
 * Returns: a new composer
 *
 * Since: 1.1
 */
GtkWidget     *awra_composer_new                  (void);
/**
 * awra_composer_get_view:
 * @self: a composer
 *
 * Returns: (transfer none): the text view
 *
 * Since: 1.1
 */
GtkTextView   *awra_composer_get_view             (AwraComposer *self);
/**
 * awra_composer_get_buffer:
 * @self: a composer
 *
 * Returns: (transfer none): the text buffer
 *
 * Since: 1.1
 */
GtkTextBuffer *awra_composer_get_buffer           (AwraComposer *self);
/**
 * awra_composer_get_text:
 * @self: a composer
 *
 * Returns: (transfer none): the whole text
 *
 * Since: 1.1
 */
const char    *awra_composer_get_text             (AwraComposer *self);
/**
 * awra_composer_set_text:
 * @self: a composer
 * @text: (nullable): the text, or %NULL to clear it
 *
 * Replaces the text and puts the cursor at its end.
 *
 * Since: 1.1
 */
void           awra_composer_set_text             (AwraComposer *self,
                                                   const char   *text);
/**
 * awra_composer_get_placeholder_text:
 * @self: a composer
 *
 * Returns: (transfer none) (nullable): the text shown while empty
 *
 * Since: 1.1
 */
const char    *awra_composer_get_placeholder_text (AwraComposer *self);
/**
 * awra_composer_set_placeholder_text:
 * @self: a composer
 * @text: (nullable): the text shown while empty, such as "Message #general"
 *
 * Since: 1.1
 */
void           awra_composer_set_placeholder_text (AwraComposer *self,
                                                   const char   *text);
/**
 * awra_composer_get_max_lines:
 * @self: a composer
 *
 * Returns: the number of lines shown before the text scrolls
 *
 * Since: 1.1
 */
guint          awra_composer_get_max_lines        (AwraComposer *self);
/**
 * awra_composer_set_max_lines:
 * @self: a composer
 * @lines: the number of lines shown before the text scrolls, at least 1
 *
 * Since: 1.1
 */
void           awra_composer_set_max_lines        (AwraComposer *self,
                                                   guint         lines);
/**
 * awra_composer_get_can_send:
 * @self: a composer
 *
 * Returns: whether sending is offered even without text
 *
 * Since: 1.1
 */
gboolean       awra_composer_get_can_send         (AwraComposer *self);
/**
 * awra_composer_set_can_send:
 * @self: a composer
 * @can_send: whether sending is offered even without text, for example with
 *   attachments only
 *
 * Since: 1.1
 */
void           awra_composer_set_can_send         (AwraComposer *self,
                                                   gboolean      can_send);
/**
 * awra_composer_get_send_on_enter:
 * @self: a composer
 *
 * Returns: whether Enter submits
 *
 * Since: 1.1
 */
gboolean       awra_composer_get_send_on_enter    (AwraComposer *self);
/**
 * awra_composer_set_send_on_enter:
 * @self: a composer
 * @send_on_enter: whether Enter submits; Shift+Enter then inserts a new line
 *
 * Since: 1.1
 */
void           awra_composer_set_send_on_enter    (AwraComposer *self,
                                                   gboolean      send_on_enter);
/**
 * awra_composer_get_busy:
 * @self: a composer
 *
 * Returns: whether the send button shows progress
 *
 * Since: 1.1
 */
gboolean       awra_composer_get_busy             (AwraComposer *self);
/**
 * awra_composer_set_busy:
 * @self: a composer
 * @busy: whether something is being sent
 *
 * Since: 1.1
 */
void           awra_composer_set_busy             (AwraComposer *self,
                                                   gboolean      busy);
/**
 * awra_composer_get_banner:
 * @self: a composer
 *
 * Returns: (transfer none) (nullable): the banner above the text
 *
 * Since: 1.1
 */
GtkWidget     *awra_composer_get_banner           (AwraComposer *self);
/**
 * awra_composer_set_banner:
 * @self: a composer
 * @banner: (nullable): a widget shown above the text, or %NULL
 *
 * The banner slides in when one is set and out when it is removed.
 *
 * Since: 1.1
 */
void           awra_composer_set_banner           (AwraComposer *self,
                                                   GtkWidget    *banner);
/**
 * awra_composer_show_banner:
 * @self: a composer
 * @text: what the banner says, such as "Replying to Ada"
 * @closable: whether the banner has a close button
 *
 * Shows a banner with a line of text. Its close button removes it and emits
 * #AwraComposer::banner-closed.
 *
 * Since: 1.1
 */
void           awra_composer_show_banner          (AwraComposer *self,
                                                   const char   *text,
                                                   gboolean      closable);
/**
 * awra_composer_get_attachments:
 * @self: a composer
 *
 * Returns: (transfer none) (nullable): the attachments widget
 *
 * Since: 1.1
 */
GtkWidget     *awra_composer_get_attachments      (AwraComposer *self);
/**
 * awra_composer_set_attachments:
 * @self: a composer
 * @attachments: (nullable): a widget shown above the text, such as
 *   thumbnails, or %NULL
 *
 * Since: 1.1
 */
void           awra_composer_set_attachments      (AwraComposer *self,
                                                   GtkWidget    *attachments);
/**
 * awra_composer_get_start_widget:
 * @self: a composer
 *
 * Returns: (transfer none) (nullable): the widget before the text
 *
 * Since: 1.1
 */
GtkWidget     *awra_composer_get_start_widget     (AwraComposer *self);
/**
 * awra_composer_set_start_widget:
 * @self: a composer
 * @widget: (nullable): a widget before the text, such as an add button
 *
 * Since: 1.1
 */
void           awra_composer_set_start_widget     (AwraComposer *self,
                                                   GtkWidget    *widget);
/**
 * awra_composer_get_end_widget:
 * @self: a composer
 *
 * Returns: (transfer none) (nullable): the widget after the text
 *
 * Since: 1.1
 */
GtkWidget     *awra_composer_get_end_widget       (AwraComposer *self);
/**
 * awra_composer_set_end_widget:
 * @self: a composer
 * @widget: (nullable): a widget after the text, such as emoji and sticker
 *   buttons
 *
 * Since: 1.1
 */
void           awra_composer_set_end_widget       (AwraComposer *self,
                                                   GtkWidget    *widget);

G_END_DECLS
