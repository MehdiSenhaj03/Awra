/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_AVATAR_GROUP (awra_avatar_group_get_type ())
G_DECLARE_FINAL_TYPE (AwraAvatarGroup, awra_avatar_group, AWRA, AVATAR_GROUP, GtkWidget)

/**
 * awra_avatar_group_new:
 *
 * Makes a group of avatars, such as the people of a conversation, which
 * overlap from the start: each one is cut where the next one covers it,
 * with a thin gap that reads on any background, so no ring has to match
 * the surface behind.
 *
 * Returns: a new avatar group
 *
 * Since: 1.1
 */
GtkWidget *awra_avatar_group_new        (void);
/**
 * awra_avatar_group_append:
 * @self: an avatar group
 * @avatar: an avatar, usually an #AwraAvatar, to show after the others
 *
 * Since: 1.1
 */
void       awra_avatar_group_append     (AwraAvatarGroup *self,
                                         GtkWidget       *avatar);
/**
 * awra_avatar_group_remove_all:
 * @self: an avatar group
 *
 * Since: 1.1
 */
void       awra_avatar_group_remove_all (AwraAvatarGroup *self);

G_END_DECLS
