/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "widgets/awra-widget-style-private.h"

#include <awra/awra-context.h>

#include <math.h>

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

static double
channel_luminance (double channel)
{
  return channel <= 0.04045 ? channel / 12.92 : pow ((channel + 0.055) / 1.055, 2.4);
}

static double
relative_luminance (const GdkRGBA *color)
{
  return 0.2126 * channel_luminance (color->red) +
         0.7152 * channel_luminance (color->green) +
         0.0722 * channel_luminance (color->blue);
}

GdkRGBA
awra_rgba_readable (const GdkRGBA *color,
                    gboolean       dark)
{
  /* Contrast 4.5 against Awra's darkest and lightest surfaces. */
  const double darkest_text = 0.27;
  const double lightest_text = 0.17;
  double target = dark ? 1.0 : 0.0;
  GdkRGBA result = *color;

  result.alpha = 1.0;
  for (int step = 0; step < 20; step++) {
    double luminance = relative_luminance (&result);

    if (dark ? luminance >= darkest_text : luminance <= lightest_text)
      break;
    result.red += (target - result.red) * 0.1;
    result.green += (target - result.green) * 0.1;
    result.blue += (target - result.blue) * 0.1;
  }
  result.alpha = color->alpha;
  return result;
}

void
awra_widget_present_popovers (GtkWidget *widget)
{
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    if (GTK_IS_POPOVER (child) && gtk_widget_get_visible (child))
      gtk_popover_present (GTK_POPOVER (child));
}
