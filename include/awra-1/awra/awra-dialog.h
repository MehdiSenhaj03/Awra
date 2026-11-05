/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-enums.h>
#include <awra/awra-window.h>

G_BEGIN_DECLS

#define AWRA_TYPE_DIALOG (awra_dialog_get_type ())
G_DECLARE_FINAL_TYPE (AwraDialog, awra_dialog, AWRA, DIALOG, AwraWindow)

/**
 * awra_dialog_new:
 * @parent: (nullable): the transient parent, or %NULL
 *
 * Returns: (transfer full): a new Awra dialog
 */
GtkWidget  *awra_dialog_new                     (GtkWindow           *parent);
/**
 * awra_dialog_get_body:
 * @self: an Awra dialog
 *
 * Returns: (transfer none) (nullable): the content shown above the responses
 *
 * Since: 1.1
 */
GtkWidget  *awra_dialog_get_body                (AwraDialog          *self);
/**
 * awra_dialog_set_body:
 * @self: an Awra dialog
 * @body: (nullable): the content to show above the responses
 *
 * Lets the dialog manage its content: @body, then the row of responses.
 * A dialog filled with awra_window_set_content() has no responses row.
 *
 * Since: 1.1
 */
void        awra_dialog_set_body                (AwraDialog          *self,
                                                 GtkWidget           *body);
/**
 * awra_dialog_add_response:
 * @self: an Awra dialog
 * @id: the response identifier
 * @label: the button label
 *
 * Adds a response button; choosing it emits #AwraDialog::response with @id,
 * then closes the dialog, unless awra_dialog_set_response_closes() keeps it
 * open.
 *
 * Since: 1.1
 */
void        awra_dialog_add_response            (AwraDialog          *self,
                                                 const char          *id,
                                                 const char          *label);
/**
 * awra_dialog_set_response_appearance:
 * @self: an Awra dialog
 * @id: a response identifier
 * @appearance: the button appearance, such as primary or destructive
 *
 * Since: 1.1
 */
void        awra_dialog_set_response_appearance (AwraDialog          *self,
                                                 const char          *id,
                                                 AwraButtonAppearance appearance);
/**
 * awra_dialog_set_response_enabled:
 * @self: an Awra dialog
 * @id: a response identifier
 * @enabled: whether the response can be chosen
 *
 * Since: 1.1
 */
void        awra_dialog_set_response_enabled    (AwraDialog          *self,
                                                 const char          *id,
                                                 gboolean             enabled);
/**
 * awra_dialog_get_response_closes:
 * @self: an Awra dialog
 * @id: a response identifier
 *
 * Returns: whether choosing the response closes the dialog
 *
 * Since: 1.1
 */
gboolean    awra_dialog_get_response_closes     (AwraDialog          *self,
                                                 const char          *id);
/**
 * awra_dialog_set_response_closes:
 * @self: an Awra dialog
 * @id: a response identifier
 * @closes: whether choosing the response closes the dialog
 *
 * A response that doesn't close only emits #AwraDialog::response, so the
 * dialog can stay open while the work it starts runs, show that work with
 * awra_dialog_set_response_busy(), show an error in its body, and close
 * itself with awra_dialog_close() once done. Responses close by default.
 *
 * Since: 1.1
 */
void        awra_dialog_set_response_closes     (AwraDialog          *self,
                                                 const char          *id,
                                                 gboolean             closes);
/**
 * awra_dialog_set_response_busy:
 * @self: an Awra dialog
 * @id: a response identifier
 * @busy: whether the response's work is under way
 *
 * Shows a spinner in the response's button and ignores it until it's no
 * longer busy, as awra_button_set_busy() does.
 *
 * Since: 1.1
 */
void        awra_dialog_set_response_busy       (AwraDialog          *self,
                                                 const char          *id,
                                                 gboolean             busy);
/**
 * awra_dialog_get_default_response:
 * @self: an Awra dialog
 *
 * Returns: (transfer none) (nullable): the response Enter activates
 *
 * Since: 1.1
 */
const char *awra_dialog_get_default_response    (AwraDialog          *self);
/**
 * awra_dialog_set_default_response:
 * @self: an Awra dialog
 * @id: (nullable): the response Enter activates
 *
 * Since: 1.1
 */
void        awra_dialog_set_default_response    (AwraDialog          *self,
                                                 const char          *id);
/**
 * awra_dialog_get_close_response:
 * @self: an Awra dialog
 *
 * Returns: (transfer none) (nullable): the response given on Escape or close
 *
 * Since: 1.1
 */
const char *awra_dialog_get_close_response      (AwraDialog          *self);
/**
 * awra_dialog_set_close_response:
 * @self: an Awra dialog
 * @id: (nullable): the response given when the dialog is closed with Escape
 *   or its close button
 *
 * Since: 1.1
 */
void        awra_dialog_set_close_response      (AwraDialog          *self,
                                                 const char          *id);
/**
 * awra_dialog_respond:
 * @self: an Awra dialog
 * @id: a response identifier
 *
 * Emits #AwraDialog::response with @id, then closes the dialog unless @id
 * is a response that doesn't close.
 *
 * Since: 1.1
 */
void        awra_dialog_respond                 (AwraDialog          *self,
                                                 const char          *id);
/**
 * awra_dialog_close:
 * @self: an Awra dialog
 *
 * Closes the dialog without giving any response, as an application does
 * once the work a response that doesn't close has started is done.
 * gtk_window_close() answers the close response instead, as Escape does.
 *
 * Since: 1.1
 */
void        awra_dialog_close                   (AwraDialog          *self);

G_END_DECLS
