/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

#include <awra/awra-enums.h>

G_BEGIN_DECLS

/**
 * AwraChip:
 *
 * A compact pill for reactions, filters and tags.
 *
 * A chip shows an icon, a picture or an emoji, a label and a count, any of
 * which may be left out. Clicking a toggleable chip turns it on or off with a
 * small bounce; an active chip takes the accent. When the count changes, its
 * digits roll up or down to the new value.
 *
 * The chip is a toggle button for assistive technologies; its accessible name
 * combines the label or emoji and the count.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_CHIP (awra_chip_get_type ())
G_DECLARE_FINAL_TYPE (AwraChip, awra_chip, AWRA, CHIP, GtkWidget)

/**
 * awra_chip_new:
 * @label: (nullable): the text of the chip
 *
 * Returns: a new chip
 *
 * Since: 1.1
 */
GtkWidget          *awra_chip_new            (const char         *label);
/**
 * awra_chip_get_label:
 * @self: a chip
 *
 * Returns: (nullable): the text of the chip
 *
 * Since: 1.1
 */
const char         *awra_chip_get_label      (AwraChip           *self);
/**
 * awra_chip_set_label:
 * @self: a chip
 * @label: (nullable): the text of the chip
 *
 * Since: 1.1
 */
void                awra_chip_set_label      (AwraChip           *self,
                                              const char         *label);
/**
 * awra_chip_get_icon_name:
 * @self: a chip
 *
 * Returns: (nullable): the symbolic icon before the label
 *
 * Since: 1.1
 */
const char         *awra_chip_get_icon_name  (AwraChip           *self);
/**
 * awra_chip_set_icon_name:
 * @self: a chip
 * @icon_name: (nullable): a symbolic icon to show before the label
 *
 * A picture or an emoji takes the place of the icon when set.
 *
 * Since: 1.1
 */
void                awra_chip_set_icon_name  (AwraChip           *self,
                                              const char         *icon_name);
/**
 * awra_chip_get_paintable:
 * @self: a chip
 *
 * Returns: (transfer none) (nullable): the picture before the label
 *
 * Since: 1.1
 */
GdkPaintable       *awra_chip_get_paintable  (AwraChip           *self);
/**
 * awra_chip_set_paintable:
 * @self: a chip
 * @paintable: (nullable): a picture to show before the label, such as a
 *   custom emoji
 *
 * The picture is drawn at icon size, keeping its aspect ratio. Animated
 * paintables play.
 *
 * Since: 1.1
 */
void                awra_chip_set_paintable  (AwraChip           *self,
                                              GdkPaintable       *paintable);
/**
 * awra_chip_get_emoji:
 * @self: a chip
 *
 * Returns: (nullable): the emoji before the label
 *
 * Since: 1.1
 */
const char         *awra_chip_get_emoji      (AwraChip           *self);
/**
 * awra_chip_set_emoji:
 * @self: a chip
 * @emoji: (nullable): an emoji, or any short text, drawn at icon size before
 *   the label
 *
 * Since: 1.1
 */
void                awra_chip_set_emoji      (AwraChip           *self,
                                              const char         *emoji);
/**
 * awra_chip_get_count:
 * @self: a chip
 *
 * Returns: the count, or -1 when none is shown
 *
 * Since: 1.1
 */
int                 awra_chip_get_count      (AwraChip           *self);
/**
 * awra_chip_set_count:
 * @self: a chip
 * @count: the count, or -1 to show none
 *
 * The digits that change roll up when the count grows and down when it
 * shrinks.
 *
 * Since: 1.1
 */
void                awra_chip_set_count      (AwraChip           *self,
                                              int                 count);
/**
 * awra_chip_get_active:
 * @self: a chip
 *
 * Returns: whether the chip is on
 *
 * Since: 1.1
 */
gboolean            awra_chip_get_active     (AwraChip           *self);
/**
 * awra_chip_set_active:
 * @self: a chip
 * @active: whether the chip is on
 *
 * Changing the state from code emits neither #AwraChip::toggled nor
 * #AwraChip::activate.
 *
 * Since: 1.1
 */
void                awra_chip_set_active     (AwraChip           *self,
                                              gboolean            active);
/**
 * awra_chip_get_toggleable:
 * @self: a chip
 *
 * Returns: whether clicking the chip turns it on and off
 *
 * Since: 1.1
 */
gboolean            awra_chip_get_toggleable (AwraChip           *self);
/**
 * awra_chip_set_toggleable:
 * @self: a chip
 * @toggleable: whether clicking the chip turns it on and off
 *
 * A chip that is not toggleable only emits #AwraChip::activate, for an
 * application that decides the state itself, such as after a server
 * confirmed a reaction.
 *
 * Since: 1.1
 */
void                awra_chip_set_toggleable (AwraChip           *self,
                                              gboolean            toggleable);
/**
 * awra_chip_get_appearance:
 * @self: a chip
 *
 * Returns: the tone of the chip
 *
 * Since: 1.1
 */
AwraChipAppearance  awra_chip_get_appearance (AwraChip           *self);
/**
 * awra_chip_set_appearance:
 * @self: a chip
 * @appearance: the tone of the chip
 *
 * Since: 1.1
 */
void                awra_chip_set_appearance (AwraChip           *self,
                                              AwraChipAppearance  appearance);

G_END_DECLS
