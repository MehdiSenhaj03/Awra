/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

#include <awra/awra-enums.h>

G_BEGIN_DECLS

/**
 * AwraAvatar:
 *
 * A picture of a person or a place, with initials or an icon when there is
 * no picture, an optional presence dot and a speaking ring.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_AVATAR (awra_avatar_get_type ())
G_DECLARE_FINAL_TYPE (AwraAvatar, awra_avatar, AWRA, AVATAR, GtkWidget)

/**
 * awra_avatar_new:
 * @size: the diameter, in logical pixels
 * @text: (nullable): the name of the person or place
 *
 * A picture of a person or place. Without an image, the avatar shows the
 * initials of @text on a colour derived from it, so the same name always
 * looks the same; without text, an icon.
 *
 * The avatar can show a presence dot, cut into its corner so that it reads on
 * any background, and a ring that announces someone speaking. The ring is
 * drawn just outside the avatar, within a sixth of its size. An image that
 * changes cross-fades.
 *
 * Returns: a new avatar
 *
 * Since: 1.1
 */
GtkWidget       *awra_avatar_new               (int              size,
                                                const char      *text);
/**
 * awra_avatar_get_size:
 * @self: an avatar
 *
 * Returns: the diameter, in logical pixels
 *
 * Since: 1.1
 */
int              awra_avatar_get_size          (AwraAvatar      *self);
/**
 * awra_avatar_set_size:
 * @self: an avatar
 * @size: the diameter, in logical pixels, at least 8
 *
 * Since: 1.1
 */
void             awra_avatar_set_size          (AwraAvatar      *self,
                                                int              size);
/**
 * awra_avatar_get_text:
 * @self: an avatar
 *
 * Returns: (nullable): the name of the person or place
 *
 * Since: 1.1
 */
const char      *awra_avatar_get_text          (AwraAvatar      *self);
/**
 * awra_avatar_set_text:
 * @self: an avatar
 * @text: (nullable): the name of the person or place
 *
 * Gives the initials, the background colour and the accessible name.
 *
 * Since: 1.1
 */
void             awra_avatar_set_text          (AwraAvatar      *self,
                                                const char      *text);
/**
 * awra_avatar_get_show_initials:
 * @self: an avatar
 *
 * Returns: whether initials are shown when there is no image
 *
 * Since: 1.1
 */
gboolean         awra_avatar_get_show_initials (AwraAvatar      *self);
/**
 * awra_avatar_set_show_initials:
 * @self: an avatar
 * @show_initials: whether to show initials when there is no image
 *
 * When off, the avatar shows its icon on the colour of its text.
 *
 * Since: 1.1
 */
void             awra_avatar_set_show_initials (AwraAvatar      *self,
                                                gboolean         show_initials);
/**
 * awra_avatar_get_custom_image:
 * @self: an avatar
 *
 * Returns: (transfer none) (nullable): the picture
 *
 * Since: 1.1
 */
GdkPaintable    *awra_avatar_get_custom_image  (AwraAvatar      *self);
/**
 * awra_avatar_set_custom_image:
 * @self: an avatar
 * @image: (nullable): the picture
 *
 * The picture fills the avatar and is cropped to its shape. Textures are
 * scaled down with mipmaps, so a large picture stays sharp at small sizes.
 * Changing the picture cross-fades.
 *
 * Since: 1.1
 */
void             awra_avatar_set_custom_image  (AwraAvatar      *self,
                                                GdkPaintable    *image);
/**
 * awra_avatar_get_icon_name:
 * @self: an avatar
 *
 * Returns: (nullable): the icon shown when there is neither image nor
 *   initials
 *
 * Since: 1.1
 */
const char      *awra_avatar_get_icon_name     (AwraAvatar      *self);
/**
 * awra_avatar_set_icon_name:
 * @self: an avatar
 * @icon_name: (nullable): the icon shown when there is neither image nor
 *   initials, or %NULL for a generic person
 *
 * Since: 1.1
 */
void             awra_avatar_set_icon_name     (AwraAvatar      *self,
                                                const char      *icon_name);
/**
 * awra_avatar_get_shape:
 * @self: an avatar
 *
 * Returns: the outline of the avatar
 *
 * Since: 1.1
 */
AwraAvatarShape  awra_avatar_get_shape         (AwraAvatar      *self);
/**
 * awra_avatar_set_shape:
 * @self: an avatar
 * @shape: the outline of the avatar
 *
 * Since: 1.1
 */
void             awra_avatar_set_shape         (AwraAvatar      *self,
                                                AwraAvatarShape  shape);
/**
 * awra_avatar_get_presence:
 * @self: an avatar
 *
 * Returns: the presence shown at the bottom end corner
 *
 * Since: 1.1
 */
AwraPresence     awra_avatar_get_presence      (AwraAvatar      *self);
/**
 * awra_avatar_set_presence:
 * @self: an avatar
 * @presence: the presence to show at the bottom end corner
 *
 * Since: 1.1
 */
void             awra_avatar_set_presence      (AwraAvatar      *self,
                                                AwraPresence     presence);
/**
 * awra_avatar_get_speaking:
 * @self: an avatar
 *
 * Returns: whether the speaking ring is shown
 *
 * Since: 1.1
 */
gboolean         awra_avatar_get_speaking      (AwraAvatar      *self);
/**
 * awra_avatar_set_speaking:
 * @self: an avatar
 * @speaking: whether the person is speaking
 *
 * Shows an accent ring around the avatar that breathes gently while it
 * stays on. The ring stays still when reduced motion is on.
 *
 * Since: 1.1
 */
void             awra_avatar_set_speaking      (AwraAvatar      *self,
                                                gboolean         speaking);

G_END_DECLS
