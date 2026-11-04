/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-surface.h>

G_BEGIN_DECLS

#define AWRA_TYPE_SIDEBAR (awra_sidebar_get_type ())
G_DECLARE_FINAL_TYPE (AwraSidebar, awra_sidebar, AWRA, SIDEBAR, GtkWidget)

GtkWidget   *awra_sidebar_new         (void);
/**
 * awra_sidebar_get_child:
 * @self: an Awra sidebar
 *
 * Returns: (transfer none) (nullable): the sidebar content
 */
GtkWidget   *awra_sidebar_get_child   (AwraSidebar *self);
/**
 * awra_sidebar_set_child:
 * @self: an Awra sidebar
 * @child: (nullable): the sidebar content, or %NULL
 */
void         awra_sidebar_set_child   (AwraSidebar *self,
                                       GtkWidget   *child);
/**
 * awra_sidebar_get_surface:
 * @self: an Awra sidebar
 *
 * Returns: (transfer none): the sidebar surface
 */
AwraSurface *awra_sidebar_get_surface (AwraSidebar *self);

/**
 * awra_sidebar_get_inset:
 * @self: an Awra sidebar
 *
 * Returns: the space between the sidebar surface and its allocation
 */
guint        awra_sidebar_get_inset   (AwraSidebar *self);
/**
 * awra_sidebar_set_inset:
 * @self: an Awra sidebar
 * @inset: uniform inset in logical pixels
 *
 * Makes the semantic Chrome surface float inside the layout allocation while
 * preserving the sidebar's structural role. Set to zero for a docked sidebar:
 * its corner under the titlebar is then square, and its inner corners stay
 * soft.
 */
void         awra_sidebar_set_inset   (AwraSidebar *self,
                                       guint        inset);
/**
 * awra_sidebar_set_inset_spacing:
 * @self: an Awra sidebar
 * @spacing: a semantic spacing token
 *
 * Applies a token-based inset. New sidebars use %AWRA_SPACING_MD by default;
 * use %AWRA_SPACING_NONE for a flush docked rail.
 */
void         awra_sidebar_set_inset_spacing (AwraSidebar *self,
                                              AwraSpacing  spacing);

G_END_DECLS
