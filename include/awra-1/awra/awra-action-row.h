/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AWRA_TYPE_ACTION_ROW (awra_action_row_get_type ())
G_DECLARE_FINAL_TYPE (AwraActionRow, awra_action_row, AWRA, ACTION_ROW, GtkButton)

GtkWidget  *awra_action_row_new           (const char    *title,
                                            const char    *subtitle);
const char *awra_action_row_get_title     (AwraActionRow *self);
void        awra_action_row_set_title     (AwraActionRow *self,
                                            const char    *title);
const char *awra_action_row_get_subtitle  (AwraActionRow *self);
void        awra_action_row_set_subtitle  (AwraActionRow *self,
                                            const char    *subtitle);
/**
 * awra_action_row_get_subtitle_lines:
 * @self: an Awra action row
 *
 * Returns: how many lines the subtitle may take, 0 for as many as it needs
 *
 * Since: 1.1
 */
int         awra_action_row_get_subtitle_lines (AwraActionRow *self);
/**
 * awra_action_row_set_subtitle_lines:
 * @self: an Awra action row
 * @lines: how many lines the subtitle may take before it's ellipsized; 1
 *   keeps it on one line, 0 lets it wrap onto as many as it needs
 *
 * Settings and explanations read better whole: let their subtitle wrap
 * rather than cut it off.
 *
 * Since: 1.1
 */
void        awra_action_row_set_subtitle_lines (AwraActionRow *self,
                                                int            lines);
const char *awra_action_row_get_icon_name (AwraActionRow *self);
void        awra_action_row_set_icon_name (AwraActionRow *self,
                                            const char    *icon_name);
/**
 * awra_action_row_get_suffix:
 * @self: an Awra action row
 *
 * Returns: (transfer none) (nullable): the decorative suffix widget
 */
GtkWidget  *awra_action_row_get_suffix    (AwraActionRow *self);
/**
 * awra_action_row_set_suffix:
 * @self: an Awra action row
 * @suffix: (nullable): a decorative suffix, or %NULL
 */
void        awra_action_row_set_suffix    (AwraActionRow *self,
                                            GtkWidget     *suffix);

G_END_DECLS
