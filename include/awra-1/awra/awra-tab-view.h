/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_TAB_VIEW (awra_tab_view_get_type ())
G_DECLARE_FINAL_TYPE (AwraTabView, awra_tab_view, AWRA, TAB_VIEW, GtkWidget)

GtkWidget  *awra_tab_view_new               (void);
void        awra_tab_view_add               (AwraTabView *self,
                                             GtkWidget   *page,
                                             const char  *name,
                                             const char  *title);
/**
 * awra_tab_view_append:
 * @self: an Awra tab view
 * @page: the page to append
 * @title: the visible tab title
 * @pinned: whether the page is permanent and has no close affordance
 *
 * Appends a dynamic page. Non-pinned pages expose an Awra-owned contextual
 * close affordance and participate in the horizontally scrollable tab rail.
 * Pinned pages remain fixed while that rail scrolls. Applications can veto a
 * request through the #AwraTabView::close-page signal.
 *
 * Returns: (transfer none): @page
 */
GtkWidget  *awra_tab_view_append            (AwraTabView *self,
                                             GtkWidget   *page,
                                             const char  *title,
                                             gboolean     pinned);
void        awra_tab_view_remove            (AwraTabView *self,
                                             GtkWidget   *page);
/**
 * awra_tab_view_close_page:
 * @self: an Awra tab view
 * @page: a page contained by @self
 *
 * Requests that @page be closed. The page is removed unless a
 * #AwraTabView::close-page handler returns %TRUE.
 */
void        awra_tab_view_close_page        (AwraTabView *self,
                                             GtkWidget   *page);
/**
 * awra_tab_view_get_selected_page:
 * @self: an Awra tab view
 *
 * Returns: (transfer none) (nullable): the selected page
 */
GtkWidget  *awra_tab_view_get_selected_page (AwraTabView *self);
void        awra_tab_view_set_selected_page (AwraTabView *self,
                                             GtkWidget   *page);
void        awra_tab_view_set_page_title    (AwraTabView *self,
                                             GtkWidget   *page,
                                             const char  *title);
/**
 * awra_tab_view_set_page_icon_name:
 * @self: an Awra tab view
 * @page: a page contained by @self
 * @icon_name: (nullable): an icon name, or %NULL to hide the icon
 *
 * Sets the optional icon displayed by the page's tab. Pinned pages do not
 * receive an implicit icon; applications choose whether they represent a
 * home page, a tool, or another permanent destination.
 *
 * Since: 1.1
 */
void        awra_tab_view_set_page_icon_name (AwraTabView *self,
                                              GtkWidget   *page,
                                              const char  *icon_name);
/**
 * awra_tab_view_set_page_icon:
 * @self: an Awra tab view
 * @page: a page contained by @self
 * @icon: (transfer none) (nullable): an unparented image, or %NULL
 *
 * Sets an optional application-provided image before the page title. Awra
 * owns its size, spacing and tab-state integration while @icon remains the
 * application's content. This is suitable for thumbnails, avatars and other
 * domain imagery which cannot be represented by a themed icon name.
 *
 * @icon must be unparented. It is parented by the tab until it is replaced,
 * cleared or the page is removed.
 *
 * Since: 1.1
 */
void        awra_tab_view_set_page_icon      (AwraTabView *self,
                                              GtkWidget   *page,
                                              GtkImage    *icon);
/**
 * awra_tab_view_set_page_title_visible:
 * @self: an Awra tab view
 * @page: a page contained by @self
 * @visible: whether the tab title is visible
 *
 * Controls title visibility without changing the accessible tab label.
 *
 * Since: 1.1
 */
void        awra_tab_view_set_page_title_visible (AwraTabView *self,
                                                  GtkWidget   *page,
                                                  gboolean     visible);
void        awra_tab_view_set_page_tooltip  (AwraTabView *self,
                                             GtkWidget   *page,
                                             const char  *tooltip);
/**
 * awra_tab_view_get_tab_bar:
 * @self: an Awra tab view
 *
 * Returns: (transfer none): the Awra-owned tab bar
 */
GtkWidget  *awra_tab_view_get_tab_bar       (AwraTabView *self);
/**
 * awra_tab_view_get_page_tab:
 * @self: a tab view
 * @page: a page in the view
 * Returns: (transfer none) (nullable): the page's tab, for input controllers
 */
GtkWidget *awra_tab_view_get_page_tab (AwraTabView *self, GtkWidget *page);
/**
 * awra_tab_view_reorder_page:
 * @self: a tab view
 * @page: a non-pinned page
 * @position: the desired index among non-pinned pages
 *
 * Preserves the page, selected tab, controllers and page metadata.
 */
void awra_tab_view_reorder_page (AwraTabView *self, GtkWidget *page, guint position);
/**
 * awra_tab_view_set_page_dragged:
 * @self: a tab view
 * @page: a page of @self
 * @dragged: whether the tab is being dragged
 *
 * Lifts the tab out of its bar while a drag carries it: neighbours close the
 * gap and positions no longer count it. Clear it when the drag ends.
 *
 * Since: 1.1
 */
void awra_tab_view_set_page_dragged (AwraTabView *self, GtkWidget *page, gboolean dragged);
/**
 * awra_tab_view_get_insertion_position:
 * @self: a tab view
 * @x: a horizontal position in the coordinates of the tab bar
 *
 * Returns: the position, among tabs that are not lifted, at which a tab
 *   dropped at @x would land. It does not depend on tabs in motion.
 *
 * Since: 1.1
 */
guint awra_tab_view_get_insertion_position (AwraTabView *self, double x);
/**
 * awra_tab_view_show_placeholder:
 * @self: a tab view
 * @position: the insertion position
 *
 * Opens an empty slot at @position, moving it there if it is already shown.
 *
 * Since: 1.1
 */
void awra_tab_view_show_placeholder (AwraTabView *self, guint position);
/**
 * awra_tab_view_hide_placeholder:
 * @self: a tab view
 *
 * Since: 1.1
 */
void awra_tab_view_hide_placeholder (AwraTabView *self);
/**
 * awra_tab_view_get_scroll_selects:
 * @self: a tab view
 *
 * Returns: whether scrolling over the tab bar selects tabs
 *
 * Since: 1.1
 */
gboolean awra_tab_view_get_scroll_selects (AwraTabView *self);
/**
 * awra_tab_view_set_scroll_selects:
 * @self: a tab view
 * @selects: %TRUE to select the adjacent tab on scroll
 *
 * By default, scrolling moves an overflowing tab strip. With @selects, it
 * moves the selection instead, stopping at the first and last tabs, and the
 * strip follows the selected tab.
 *
 * Since: 1.1
 */
void awra_tab_view_set_scroll_selects (AwraTabView *self, gboolean selects);
gboolean    awra_tab_view_get_content_visible (AwraTabView *self);
void        awra_tab_view_set_content_visible (AwraTabView *self,
                                               gboolean     visible);
const char *awra_tab_view_get_selected_name (AwraTabView *self);
void        awra_tab_view_set_selected_name (AwraTabView *self,
                                             const char  *name);
/**
 * awra_tab_view_get_stack:
 * @self: an Awra tab view
 *
 * Returns: (transfer none): the underlying GTK stack
 */
GtkStack   *awra_tab_view_get_stack         (AwraTabView *self);

G_END_DECLS
