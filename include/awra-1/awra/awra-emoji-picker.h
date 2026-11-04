/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>
#include <awra/awra-emoji-item.h>

G_BEGIN_DECLS

#define AWRA_TYPE_EMOJI_PICKER (awra_emoji_picker_get_type ())
/**
 * AwraEmojiPicker:
 *
 * A picker for emoji, custom emoji and stickers, fed by the application one
 * section at a time. It fits in an #AwraPopover.
 *
 * A search field filters every section by name and keywords. Tabs, one per
 * section, scroll to it and follow the scrolling. The grid is virtualized;
 * a preview under it shows the item under the pointer or the keyboard cursor
 * in large with its `:name:`. The arrow keys move the cursor through the grid,
 * even while the search field has the focus, and Enter picks it, as a click
 * does.
 *
 * A "Recent" section comes first when awra_emoji_picker_set_recent() gives it
 * items; the application keeps that list.
 *
 * Since: 1.1
 */
G_DECLARE_FINAL_TYPE (AwraEmojiPicker, awra_emoji_picker, AWRA, EMOJI_PICKER, GtkWidget)

/**
 * awra_emoji_picker_new:
 *
 * Returns: a new, empty emoji picker
 *
 * Since: 1.1
 */
GtkWidget  *awra_emoji_picker_new            (void);
/**
 * awra_emoji_picker_add_section:
 * @self: an emoji picker
 * @id: what identifies the section
 * @title: the title shown above it
 * @icon_name: (nullable): the icon of its tab, or %NULL to show its first item
 * @items: a model of #AwraEmojiItem
 *
 * Appends a section. A section with an @id already present is replaced in
 * place. The picker follows changes of @items.
 *
 * Since: 1.1
 */
void        awra_emoji_picker_add_section    (AwraEmojiPicker *self,
                                              const char      *id,
                                              const char      *title,
                                              const char      *icon_name,
                                              GListModel      *items);
/**
 * awra_emoji_picker_remove_section:
 * @self: an emoji picker
 * @id: the section to remove
 *
 * Since: 1.1
 */
void        awra_emoji_picker_remove_section (AwraEmojiPicker *self,
                                              const char      *id);
/**
 * awra_emoji_picker_clear:
 * @self: an emoji picker
 *
 * Removes every section added with awra_emoji_picker_add_section(). The
 * recent items stay.
 *
 * Since: 1.1
 */
void        awra_emoji_picker_clear          (AwraEmojiPicker *self);
/**
 * awra_emoji_picker_set_recent:
 * @self: an emoji picker
 * @items: (nullable): a model of #AwraEmojiItem, most recent first, or %NULL
 *
 * Shows a "Recent" section first while @items holds anything.
 *
 * Since: 1.1
 */
void        awra_emoji_picker_set_recent     (AwraEmojiPicker *self,
                                              GListModel      *items);
/**
 * awra_emoji_picker_get_search_text:
 * @self: an emoji picker
 *
 * Returns: (transfer none): what the search field holds
 *
 * Since: 1.1
 */
const char *awra_emoji_picker_get_search_text (AwraEmojiPicker *self);
/**
 * awra_emoji_picker_set_search_text:
 * @self: an emoji picker
 * @text: (nullable): what to search for, or %NULL to show everything
 *
 * Since: 1.1
 */
void        awra_emoji_picker_set_search_text (AwraEmojiPicker *self,
                                               const char      *text);

G_END_DECLS
