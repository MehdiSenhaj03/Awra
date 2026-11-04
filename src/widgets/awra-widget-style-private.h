/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-style-manager.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Follows the style manager of a mapped widget: tokens and reduced motion.
 * Start it in map() and stop it in unmap(). */
typedef struct {
  AwraStyleManager *style;
  gulong token_handler;
  gulong motion_handler;
} AwraStyleWatch;

typedef void (*AwraStyleWatchFunc) (gpointer user_data);

void          awra_style_watch_start         (AwraStyleWatch     *watch,
                                              GtkWidget          *widget,
                                              AwraStyleWatchFunc  changed,
                                              gpointer            user_data);
void          awra_style_watch_stop          (AwraStyleWatch     *watch);

AwraStyleManager *awra_widget_get_style_manager (GtkWidget *widget);
AwraTokenSet *awra_widget_get_token_set      (GtkWidget          *widget);
gboolean      awra_widget_get_reduced_motion (GtkWidget          *widget);

/* A colour with its alpha multiplied by @alpha. */
GdkRGBA       awra_rgba_with_alpha           (const GdkRGBA      *color,
                                              double              alpha);

/* @color moved towards white on dark surfaces, or black on light ones,
 * until it reads as text (4.5:1 against the surface), keeping its hue. */
GdkRGBA       awra_rgba_readable             (const GdkRGBA      *color,
                                              gboolean            dark);

G_END_DECLS
