/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>
#include <awra/awra-enums.h>

G_BEGIN_DECLS

#define AWRA_TYPE_BUTTON (awra_button_get_type ())
G_DECLARE_FINAL_TYPE (AwraButton, awra_button, AWRA, BUTTON, GtkButton)

GtkWidget *awra_button_new             (void);
GtkWidget *awra_button_new_with_label  (const char *label);
GtkWidget *awra_button_new_from_icon_name (const char *icon_name);
AwraButtonAppearance awra_button_get_appearance (AwraButton *self);
void       awra_button_set_appearance (AwraButton           *self,
                                       AwraButtonAppearance  appearance);
/**
 * awra_button_new_with_icon_and_label:
 * @icon_name: the icon shown before the label
 * @label: the label
 *
 * Makes a button showing an icon, then a label, through an
 * #AwraButtonContent child.
 *
 * Returns: a new Awra button
 *
 * Since: 1.1
 */
GtkWidget *awra_button_new_with_icon_and_label (const char *icon_name,
                                                const char *label);
/**
 * awra_button_get_busy:
 * @self: an Awra button
 *
 * Returns: whether the button shows that its action is under way
 *
 * Since: 1.1
 */
gboolean   awra_button_get_busy       (AwraButton           *self);
/**
 * awra_button_set_busy:
 * @self: an Awra button
 * @busy: whether the button's action is under way
 *
 * A busy button keeps its size and look, shows a spinner in place of its
 * content, and ignores clicks and activation until it's no longer busy.
 * Use it while the work a button started is in flight, rather than
 * disabling the button.
 *
 * Since: 1.1
 */
void       awra_button_set_busy       (AwraButton           *self,
                                       gboolean              busy);

G_END_DECLS
