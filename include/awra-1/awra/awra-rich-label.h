/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_RICH_LABEL (awra_rich_label_get_type ())
G_DECLARE_FINAL_TYPE (AwraRichLabel, awra_rich_label, AWRA, RICH_LABEL, GtkWidget)

/**
 * awra_rich_label_new:
 *
 * Makes a label that shows pictures among its text, such as a chat
 * message's custom emoji. Its text and markup go on its #GtkLabel
 * (awra_rich_label_get_label()), which keeps wrapping, links, selection and
 * typography; each U+FFFC OBJECT REPLACEMENT CHARACTER in that text stands
 * for the next picture given with awra_rich_label_append_paintable(), drawn
 * at #AwraRichLabel:inline-size on the text's baseline.
 *
 * Returns: a new rich label
 *
 * Since: 1.1
 */
GtkWidget *awra_rich_label_new              (void);
/**
 * awra_rich_label_get_label:
 * @self: a rich label
 *
 * Returns: (transfer none): the label that holds the text
 *
 * Since: 1.1
 */
GtkLabel  *awra_rich_label_get_label        (AwraRichLabel *self);
/**
 * awra_rich_label_append_paintable:
 * @self: a rich label
 * @paintable: (nullable): the picture for the next U+FFFC in the text, or
 *   %NULL to keep its room empty until the picture arrives
 *
 * Since: 1.1
 */
void       awra_rich_label_append_paintable (AwraRichLabel *self,
                                             GdkPaintable  *paintable);
/**
 * awra_rich_label_clear_paintables:
 * @self: a rich label
 *
 * Forgets every picture given, as when the text changes.
 *
 * Since: 1.1
 */
void       awra_rich_label_clear_paintables (AwraRichLabel *self);
/**
 * awra_rich_label_get_inline_size:
 * @self: a rich label
 *
 * Returns: the side of each picture, in logical pixels
 *
 * Since: 1.1
 */
int        awra_rich_label_get_inline_size  (AwraRichLabel *self);
/**
 * awra_rich_label_set_inline_size:
 * @self: a rich label
 * @size: the side of each picture, in logical pixels, or 0 to follow the
 *   text's height
 *
 * A message of nothing but pictures can show them larger, as chat clients
 * show a message of nothing but emoji.
 *
 * Since: 1.1
 */
void       awra_rich_label_set_inline_size  (AwraRichLabel *self,
                                             int            size);

G_END_DECLS
