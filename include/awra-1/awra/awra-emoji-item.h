/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_EMOJI_ITEM (awra_emoji_item_get_type ())
/**
 * AwraEmojiItem:
 *
 * One entry of an #AwraEmojiPicker: a Unicode emoji, or a custom image such
 * as a server emoji or a sticker.
 *
 * Since: 1.1
 */
G_DECLARE_FINAL_TYPE (AwraEmojiItem, awra_emoji_item, AWRA, EMOJI_ITEM, GObject)

/**
 * awra_emoji_item_new:
 * @id: what identifies the item for the application
 * @name: the short name, such as "thumbs_up", shown as `:thumbs_up:`
 * @text: (nullable): the emoji itself, or %NULL when @paintable is set later
 *
 * Returns: (transfer full): a new emoji item
 *
 * Since: 1.1
 */
AwraEmojiItem      *awra_emoji_item_new                (const char    *id,
                                                        const char    *name,
                                                        const char    *text);
/**
 * awra_emoji_item_new_for_paintable:
 * @id: what identifies the item for the application
 * @name: the short name
 * @paintable: the image
 *
 * Returns: (transfer full): a new emoji item showing an image
 *
 * Since: 1.1
 */
AwraEmojiItem      *awra_emoji_item_new_for_paintable  (const char    *id,
                                                        const char    *name,
                                                        GdkPaintable  *paintable);
/**
 * awra_emoji_item_get_id:
 * @self: an emoji item
 *
 * Returns: (transfer none): what identifies the item
 *
 * Since: 1.1
 */
const char         *awra_emoji_item_get_id             (AwraEmojiItem *self);
/**
 * awra_emoji_item_get_name:
 * @self: an emoji item
 *
 * Returns: (transfer none): the short name
 *
 * Since: 1.1
 */
const char         *awra_emoji_item_get_name           (AwraEmojiItem *self);
/**
 * awra_emoji_item_get_text:
 * @self: an emoji item
 *
 * Returns: (transfer none) (nullable): the emoji itself
 *
 * Since: 1.1
 */
const char         *awra_emoji_item_get_text           (AwraEmojiItem *self);
/**
 * awra_emoji_item_get_paintable:
 * @self: an emoji item
 *
 * Returns: (transfer none) (nullable): the image
 *
 * Since: 1.1
 */
GdkPaintable       *awra_emoji_item_get_paintable      (AwraEmojiItem *self);
/**
 * awra_emoji_item_set_paintable:
 * @self: an emoji item
 * @paintable: (nullable): the image, which replaces the text where both are
 *   set
 *
 * Since: 1.1
 */
void                awra_emoji_item_set_paintable      (AwraEmojiItem *self,
                                                        GdkPaintable  *paintable);
/**
 * awra_emoji_item_get_keywords:
 * @self: an emoji item
 *
 * Returns: (transfer none) (nullable) (array zero-terminated=1): more words
 *   a search finds the item by
 *
 * Since: 1.1
 */
const char * const *awra_emoji_item_get_keywords       (AwraEmojiItem *self);
/**
 * awra_emoji_item_set_keywords:
 * @self: an emoji item
 * @keywords: (nullable) (array zero-terminated=1): more words a search finds
 *   the item by
 *
 * Since: 1.1
 */
void                awra_emoji_item_set_keywords       (AwraEmojiItem      *self,
                                                        const char * const *keywords);
/**
 * awra_emoji_item_matches:
 * @self: an emoji item
 * @query: what was typed
 *
 * Returns: whether the name or a keyword contains @query, ignoring case
 *
 * Since: 1.1
 */
gboolean            awra_emoji_item_matches            (AwraEmojiItem *self,
                                                        const char    *query);

G_END_DECLS
