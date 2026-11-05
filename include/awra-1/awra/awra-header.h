/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>
#include <awra/awra-material.h>
#include <awra/awra-surface.h>

G_BEGIN_DECLS

#define AWRA_TYPE_HEADER (awra_header_get_type ())
G_DECLARE_FINAL_TYPE (AwraHeader, awra_header, AWRA, HEADER, GtkWidget)

GtkWidget  *awra_header_new            (void);
const char *awra_header_get_title      (AwraHeader *self);
void        awra_header_set_title      (AwraHeader *self,
                                        const char *title);
const char *awra_header_get_subtitle   (AwraHeader *self);
void        awra_header_set_subtitle   (AwraHeader *self,
                                        const char *subtitle);
/**
 * awra_header_get_start_widget:
 * @self: an Awra header
 *
 * Returns: (transfer none) (nullable): the leading widget
 */
GtkWidget  *awra_header_get_start_widget (AwraHeader *self);
/**
 * awra_header_set_start_widget:
 * @self: an Awra header
 * @widget: (nullable): the leading widget, or %NULL
 */
void        awra_header_set_start_widget (AwraHeader *self,
                                          GtkWidget  *widget);
/**
 * awra_header_get_end_widget:
 * @self: an Awra header
 *
 * Returns: (transfer none) (nullable): the trailing widget
 */
GtkWidget  *awra_header_get_end_widget (AwraHeader *self);
/**
 * awra_header_set_end_widget:
 * @self: an Awra header
 * @widget: (nullable): the trailing widget, or %NULL
 */
void        awra_header_set_end_widget (AwraHeader *self,
                                        GtkWidget  *widget);
/**
 * awra_header_get_center_widget:
 * @self: an Awra header
 *
 * Returns: (transfer none) (nullable): the centered widget
 */
GtkWidget  *awra_header_get_center_widget (AwraHeader *self);
void        awra_header_set_center_widget (AwraHeader *self,
                                           GtkWidget  *widget);
gboolean    awra_header_get_show_window_controls (AwraHeader *self);
void        awra_header_set_show_window_controls (AwraHeader *self,
                                                  gboolean    show_controls);
gboolean    awra_header_get_drag_enabled (AwraHeader *self);
void        awra_header_set_drag_enabled (AwraHeader *self,
                                          gboolean    drag_enabled);
/**
 * awra_header_get_material:
 * @self: an Awra header
 *
 * Returns: (transfer none): the header material
 */
AwraMaterial *awra_header_get_material (AwraHeader *self);
void          awra_header_set_material (AwraHeader   *self,
                                        AwraMaterial *material);
gboolean      awra_header_get_blend_with_window (AwraHeader *self);
void          awra_header_set_blend_with_window (AwraHeader *self,
                                                  gboolean    blend);

/**
 * awra_header_get_compact:
 * @self: a header
 *
 * Returns: whether compact desktop chrome is enabled
 * Since: 1.1
 */
gboolean awra_header_get_compact (AwraHeader *self);
/**
 * awra_header_set_compact:
 * @self: a header
 * @compact: whether to use compact desktop chrome
 *
 * Since: 1.1
 */
void awra_header_set_compact (AwraHeader *self, gboolean compact);

/**
 * awra_header_get_expand_center:
 * @self: a header
 * Returns: whether the center fills the space between side controls
 * Since: 1.1
 */
gboolean awra_header_get_expand_center (AwraHeader *self);
/**
 * awra_header_set_expand_center:
 * @self: a header
 * @expand: whether to fill the remaining center space
 *
 * Useful for a shared navigation/address control. The default keeps the
 * existing centered, natural-width presentation.
 * Since: 1.1
 */
void awra_header_set_expand_center (AwraHeader *self, gboolean expand);

/**
 * awra_header_get_surface:
 * @self: a header
 *
 * Returns: (transfer none): the owned surface, for material/backdrop configuration
 * Since: 1.1
 */
AwraSurface *awra_header_get_surface (AwraHeader *self);
/**
 * awra_header_get_max_width:
 * @self: a header
 *
 * Returns: maximum visual width, or zero for no limit
 * Since: 1.1
 */
guint awra_header_get_max_width (AwraHeader *self);
/**
 * awra_header_set_max_width:
 * @self: a header
 * @width: maximum logical width, or zero for no limit
 *
 * Centres the header within its allocation, respecting its content minimum.
 * Empty space outside the visual header does not intercept pointer events.
 * Since: 1.1
 */
void awra_header_set_max_width (AwraHeader *self, guint width);

G_END_DECLS
