/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "widgets/awra-widget-style-private.h"

#include <awra/awra-context.h>

void
awra_style_watch_start (AwraStyleWatch     *watch,
                        GtkWidget          *widget,
                        AwraStyleWatchFunc  changed,
                        gpointer            user_data)
{
  awra_style_watch_stop (watch);
  watch->style = awra_widget_get_style_manager (widget);
  watch->token_handler = g_signal_connect_swapped (watch->style, "notify::token-set",
                                                   G_CALLBACK (changed), user_data);
  watch->motion_handler = g_signal_connect_swapped (watch->style, "notify::reduced-motion",
                                                    G_CALLBACK (changed), user_data);
}

void
awra_style_watch_stop (AwraStyleWatch *watch)
{
  if (watch->style != NULL) {
    g_clear_signal_handler (&watch->token_handler, watch->style);
    g_clear_signal_handler (&watch->motion_handler, watch->style);
  }
  watch->style = NULL;
}

AwraStyleManager *
awra_widget_get_style_manager (GtkWidget *widget)
{
  return awra_context_get_style_manager (
    awra_context_get_for_display (gtk_widget_get_display (widget)));
}

AwraTokenSet *
awra_widget_get_token_set (GtkWidget *widget)
{
  return awra_style_manager_get_token_set (awra_widget_get_style_manager (widget));
}

gboolean
awra_widget_get_reduced_motion (GtkWidget *widget)
{
  return awra_style_manager_get_reduced_motion (awra_widget_get_style_manager (widget));
}

GdkRGBA
awra_rgba_with_alpha (const GdkRGBA *color,
                      double         alpha)
{
  GdkRGBA result = *color;

  result.alpha *= alpha;
  return result;
}
