/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-context-menu.h>

typedef struct {
  GtkWidget *widget;
  AwraPopover *popover;
  GtkEventController *gesture;
  gulong destroy_handler;
} ContextMenuAttachment;

static GQuark attachment_quark;

static void
context_pressed_cb (GtkGestureClick       *gesture,
                    int                    n_press,
                    double                 x,
                    double                 y,
                    ContextMenuAttachment *attachment)
{
  GdkRectangle pointing = { (int) x, (int) y, 1, 1 };

  (void) n_press;
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_popover_set_pointing_to (GTK_POPOVER (attachment->popover), &pointing);
  gtk_popover_popup (GTK_POPOVER (attachment->popover));
}

/* A widget being disposed unparents the popover itself: by the time its
 * data is freed, GTK has already found a child left on it ("Finalizing
 * ... still has children left"), as recycled list rows showed. */
static void
widget_destroy_cb (GtkWidget             *widget,
                   ContextMenuAttachment *attachment)
{
  if (attachment->popover != NULL &&
      gtk_widget_get_parent (GTK_WIDGET (attachment->popover)) == widget)
    gtk_widget_unparent (GTK_WIDGET (attachment->popover));
  attachment->popover = NULL;
}

static void
attachment_free (gpointer data)
{
  ContextMenuAttachment *attachment = data;

  if (attachment->widget != NULL && attachment->destroy_handler != 0)
    g_signal_handler_disconnect (attachment->widget, attachment->destroy_handler);

  if (attachment->widget != NULL && attachment->gesture != NULL)
    gtk_widget_remove_controller (attachment->widget, attachment->gesture);
  if (attachment->popover != NULL &&
      gtk_widget_get_parent (GTK_WIDGET (attachment->popover)) ==
        attachment->widget)
    gtk_widget_unparent (GTK_WIDGET (attachment->popover));
  if (attachment->widget != NULL)
    g_object_remove_weak_pointer (G_OBJECT (attachment->widget),
                                  (gpointer *) &attachment->widget);
  g_free (attachment);
}

static ContextMenuAttachment *
get_attachment (GtkWidget *widget)
{
  if (attachment_quark == 0)
    attachment_quark = g_quark_from_static_string ("awra-context-menu");
  return g_object_get_qdata (G_OBJECT (widget), attachment_quark);
}

void
awra_widget_set_context_menu_model (GtkWidget  *widget,
                                    GMenuModel *model)
{
  ContextMenuAttachment *attachment;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));
  attachment = get_attachment (widget);
  if (model == NULL) {
    if (attachment != NULL)
      g_object_set_qdata_full (G_OBJECT (widget), attachment_quark,
                               NULL, NULL);
    return;
  }
  if (attachment == NULL) {
    GtkGesture *gesture = gtk_gesture_click_new ();

    attachment = g_new0 (ContextMenuAttachment, 1);
    attachment->widget = widget;
    g_object_add_weak_pointer (G_OBJECT (widget),
                               (gpointer *) &attachment->widget);
    attachment->popover = AWRA_POPOVER (awra_popover_new ());
    attachment->gesture = GTK_EVENT_CONTROLLER (gesture);
    gtk_widget_set_parent (GTK_WIDGET (attachment->popover), widget);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                   GDK_BUTTON_SECONDARY);
    g_signal_connect (gesture, "pressed",
                      G_CALLBACK (context_pressed_cb), attachment);
    gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
    attachment->destroy_handler = g_signal_connect (widget, "destroy",
                                                    G_CALLBACK (widget_destroy_cb), attachment);
    g_object_set_qdata_full (G_OBJECT (widget), attachment_quark,
                             attachment, attachment_free);
  }
  if (attachment->popover != NULL)
    awra_popover_set_menu_model (attachment->popover, model);
}

GMenuModel *
awra_widget_get_context_menu_model (GtkWidget *widget)
{
  ContextMenuAttachment *attachment;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  attachment = get_attachment (widget);
  return attachment != NULL && attachment->popover != NULL
           ? awra_popover_get_menu_model (attachment->popover)
           : NULL;
}

AwraPopover *
awra_widget_get_context_menu_popover (GtkWidget *widget)
{
  ContextMenuAttachment *attachment;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  attachment = get_attachment (widget);
  return attachment != NULL ? attachment->popover : NULL;
}
