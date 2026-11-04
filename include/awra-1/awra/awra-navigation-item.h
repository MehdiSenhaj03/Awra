/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_NAVIGATION_ITEM (awra_navigation_item_get_type ())
G_DECLARE_FINAL_TYPE (AwraNavigationItem, awra_navigation_item,
                      AWRA, NAVIGATION_ITEM, GtkToggleButton)

GtkWidget  *awra_navigation_item_new           (void);
GtkWidget  *awra_navigation_item_new_with_label (const char *label);
const char *awra_navigation_item_get_label     (AwraNavigationItem *self);
void        awra_navigation_item_set_label     (AwraNavigationItem *self,
                                                 const char         *label);
const char *awra_navigation_item_get_icon_name (AwraNavigationItem *self);
void        awra_navigation_item_set_icon_name (AwraNavigationItem *self,
                                                 const char         *icon_name);
/**
 * awra_navigation_item_get_suffix:
 * @self: an Awra navigation item
 *
 * Returns: (transfer none) (nullable): the trailing widget
 *
 * Since: 1.1
 */
GtkWidget  *awra_navigation_item_get_suffix    (AwraNavigationItem *self);
/**
 * awra_navigation_item_set_suffix:
 * @self: an Awra navigation item
 * @suffix: (transfer none) (nullable): an unparented trailing widget
 *
 * Since: 1.1
 */
void        awra_navigation_item_set_suffix    (AwraNavigationItem *self,
                                                 GtkWidget          *suffix);
gboolean    awra_navigation_item_get_selected  (AwraNavigationItem *self);
void        awra_navigation_item_set_selected  (AwraNavigationItem *self,
                                                 gboolean            selected);
void        awra_navigation_item_set_group     (AwraNavigationItem *self,
                                                 AwraNavigationItem *group);
/**
 * awra_navigation_item_get_prefix:
 * @self: an Awra navigation item
 *
 * Returns: (transfer none) (nullable): the leading widget
 *
 * Since: 1.1
 */
GtkWidget  *awra_navigation_item_get_prefix    (AwraNavigationItem *self);
/**
 * awra_navigation_item_set_prefix:
 * @self: an Awra navigation item
 * @prefix: (transfer none) (nullable): an unparented leading widget, shown
 *   in place of the icon
 *
 * Since: 1.1
 */
void        awra_navigation_item_set_prefix    (AwraNavigationItem *self,
                                                 GtkWidget          *prefix);
/**
 * awra_navigation_item_get_subtitle:
 * @self: an Awra navigation item
 *
 * Returns: (transfer none): the second line, empty for none
 *
 * Since: 1.1
 */
const char *awra_navigation_item_get_subtitle  (AwraNavigationItem *self);
/**
 * awra_navigation_item_set_subtitle:
 * @self: an Awra navigation item
 * @subtitle: (nullable): a quieter second line under the label
 *
 * Since: 1.1
 */
void        awra_navigation_item_set_subtitle  (AwraNavigationItem *self,
                                                 const char         *subtitle);
/**
 * awra_navigation_item_get_emphasized:
 * @self: an Awra navigation item
 *
 * Returns: whether the destination has something new
 *
 * Since: 1.1
 */
gboolean    awra_navigation_item_get_emphasized (AwraNavigationItem *self);
/**
 * awra_navigation_item_set_emphasized:
 * @self: an Awra navigation item
 * @emphasized: whether the destination has something new
 *
 * Since: 1.1
 */
void        awra_navigation_item_set_emphasized (AwraNavigationItem *self,
                                                  gboolean            emphasized);
/**
 * awra_navigation_item_get_muted:
 * @self: an Awra navigation item
 *
 * Returns: whether the destination is silenced
 *
 * Since: 1.1
 */
gboolean    awra_navigation_item_get_muted     (AwraNavigationItem *self);
/**
 * awra_navigation_item_set_muted:
 * @self: an Awra navigation item
 * @muted: whether the destination is silenced, receding until hovered
 *
 * Since: 1.1
 */
void        awra_navigation_item_set_muted     (AwraNavigationItem *self,
                                                 gboolean            muted);

G_END_DECLS
