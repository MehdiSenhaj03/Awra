/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-typography.h>

void
awra_widget_set_typography (GtkWidget      *widget,
                            AwraTypography  typography)
{
  static const char *classes[] = {
    "awra-body",
    "awra-muted",
    "awra-eyebrow",
    "awra-title-3",
    "awra-title-2",
    "awra-title-1",
    "awra-display",
    "awra-monospace",
    "awra-label",
    "awra-reading",
  };

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (typography >= AWRA_TYPOGRAPHY_BODY &&
                    typography <= AWRA_TYPOGRAPHY_READING);
  for (guint i = 0; i < G_N_ELEMENTS (classes); i++)
    gtk_widget_remove_css_class (widget, classes[i]);
  gtk_widget_add_css_class (widget, classes[typography]);
}
