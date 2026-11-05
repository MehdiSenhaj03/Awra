/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-enums.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_STATUS_BANNER (awra_status_banner_get_type ())
G_DECLARE_FINAL_TYPE (AwraStatusBanner, awra_status_banner,
                      AWRA, STATUS_BANNER, GtkWidget)

GtkWidget            *awra_status_banner_new             (void);
/**
 * awra_status_banner_get_title:
 * @self: a status banner
 *
 * Returns: (transfer none): the title, empty when there is none
 */
const char           *awra_status_banner_get_title       (AwraStatusBanner    *self);
void                  awra_status_banner_set_title       (AwraStatusBanner    *self,
                                                         const char          *title);
/**
 * awra_status_banner_get_description:
 * @self: a status banner
 *
 * Returns: (transfer none): the description, empty when there is none
 */
const char           *awra_status_banner_get_description (AwraStatusBanner    *self);
void                  awra_status_banner_set_description (AwraStatusBanner    *self,
                                                         const char          *description);
AwraStatusAppearance  awra_status_banner_get_appearance  (AwraStatusBanner    *self);
void                  awra_status_banner_set_appearance  (AwraStatusBanner    *self,
                                                         AwraStatusAppearance appearance);
/**
 * awra_status_banner_get_action:
 * Returns: (transfer none) (nullable): the banner action
 */
GtkWidget            *awra_status_banner_get_action      (AwraStatusBanner    *self);
/**
 * awra_status_banner_set_action:
 * @action: (nullable): the banner action, or %NULL
 */
void                  awra_status_banner_set_action      (AwraStatusBanner    *self,
                                                         GtkWidget           *action);
gboolean              awra_status_banner_get_dismissible (AwraStatusBanner    *self);
void                  awra_status_banner_set_dismissible (AwraStatusBanner    *self,
                                                         gboolean             dismissible);
gboolean              awra_status_banner_get_revealed    (AwraStatusBanner    *self);
void                  awra_status_banner_set_revealed    (AwraStatusBanner    *self,
                                                         gboolean             revealed);

G_END_DECLS
