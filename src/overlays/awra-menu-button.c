/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-button.h>
#include <awra/awra-context.h>
#include <awra/awra-menu-button.h>
#include <awra/awra-layout.h>

struct _AwraMenuButton {
  GtkWidget parent_instance;
  GtkPopoverBin *popover_bin;
  AwraButton *button;
  AwraPopover *popover;
  char *label;
  char *icon_name;
  gulong popover_map_handler;
  gulong popover_unmap_handler;
  guint pointer_reset_source;
  AwraButtonAppearance appearance;
  gboolean popover_open;
  gboolean pointer_press;
  gboolean open_on_press;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_POPOVER,
  PROP_APPEARANCE,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraMenuButton, awra_menu_button, GTK_TYPE_WIDGET)

static void
chevron_draw_cb (GtkDrawingArea *area,
                 cairo_t        *cr,
                 int             width,
                 int             height,
                 gpointer        user_data)
{
  AwraContext *context = awra_context_get_for_display (
    gtk_widget_get_display (GTK_WIDGET (area)));
  const GdkRGBA *color = awra_token_set_get_muted_foreground (
    awra_style_manager_get_token_set (
      awra_context_get_style_manager (context)));
  double center_x = width / 2.0;
  double center_y = height / 2.0;

  (void) user_data;
  gdk_cairo_set_source_rgba (cr, color);
  cairo_set_line_width (cr, 1.7);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_move_to (cr, center_x - 3.0, center_y - 1.5);
  cairo_line_to (cr, center_x, center_y + 1.8);
  cairo_line_to (cr, center_x + 3.0, center_y - 1.5);
  cairo_stroke (cr);
}

static GtkWidget *
make_label_content (AwraMenuButton *self)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *label = gtk_label_new (self->label);
  GtkWidget *chevron = gtk_drawing_area_new ();

  awra_box_set_spacing (GTK_BOX (box), AWRA_SPACING_SM);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_add_css_class (box, "awra-menu-button-content");
  gtk_widget_add_css_class (label, "awra-menu-button-label");
  gtk_widget_add_css_class (chevron, "awra-menu-button-chevron");
  gtk_widget_set_size_request (chevron, 10, 10);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (chevron),
                                  chevron_draw_cb, NULL, NULL);
  gtk_box_append (GTK_BOX (box), label);
  gtk_box_append (GTK_BOX (box), chevron);
  return box;
}

static void
update_button_accessible_label (AwraMenuButton *self,
                                const char     *label)
{
  if (label != NULL)
    gtk_accessible_update_property (
      GTK_ACCESSIBLE (self->button), GTK_ACCESSIBLE_PROPERTY_LABEL, label,
      -1);
  else
    gtk_accessible_reset_property (
      GTK_ACCESSIBLE (self->button), GTK_ACCESSIBLE_PROPERTY_LABEL);
}

static void
update_accessible_state (AwraMenuButton *self,
                         gboolean        expanded)
{
  gtk_accessible_update_state (
    GTK_ACCESSIBLE (self->button),
    GTK_ACCESSIBLE_STATE_EXPANDED, expanded,
    -1);
  gtk_accessible_update_state (
    GTK_ACCESSIBLE (self),
    GTK_ACCESSIBLE_STATE_EXPANDED, expanded,
    -1);
}

static void
popover_map_cb (GtkWidget      *popover,
                AwraMenuButton *self)
{
  (void) popover;
  self->popover_open = TRUE;
  gtk_widget_add_css_class (GTK_WIDGET (self->button), "awra-open");
  update_accessible_state (self, TRUE);
}

static void
popover_unmap_cb (GtkWidget      *popover,
                  AwraMenuButton *self)
{
  (void) popover;
  self->popover_open = FALSE;
  gtk_widget_remove_css_class (GTK_WIDGET (self->button), "awra-open");
  update_accessible_state (self, FALSE);
}

static void
clear_pointer_press (AwraMenuButton *self)
{
  self->pointer_press = FALSE;
  self->open_on_press = FALSE;
}

static gboolean
pointer_reset_cb (gpointer data)
{
  AwraMenuButton *self = data;

  self->pointer_reset_source = 0;
  clear_pointer_press (self);
  return G_SOURCE_REMOVE;
}

static void
button_press_cb (GtkGestureClick *gesture,
                 int              n_press,
                 double           x,
                 double           y,
                 AwraMenuButton  *self)
{
  (void) gesture;
  (void) n_press;
  (void) x;
  (void) y;
  if (self->pointer_reset_source != 0)
    g_source_remove (self->pointer_reset_source);
  self->pointer_press = TRUE;
  self->open_on_press = self->popover_open;
  self->pointer_reset_source = g_timeout_add (1000, pointer_reset_cb, self);
}

static void
button_clicked_cb (GtkButton      *button,
                   AwraMenuButton *self)
{
  gboolean close = self->pointer_press
                     ? self->open_on_press
                     : self->popover_open;

  (void) button;
  if (self->pointer_reset_source != 0) {
    g_source_remove (self->pointer_reset_source);
    self->pointer_reset_source = 0;
  }
  clear_pointer_press (self);
  if (close)
    awra_menu_button_popdown (self);
  else
    awra_menu_button_popup (self);
}

static void
disconnect_popover (AwraMenuButton *self)
{
  if (self->popover == NULL)
    return;
  if (self->popover_map_handler != 0) {
    g_signal_handler_disconnect (self->popover,
                                 self->popover_map_handler);
    self->popover_map_handler = 0;
  }
  if (self->popover_unmap_handler != 0) {
    g_signal_handler_disconnect (self->popover,
                                 self->popover_unmap_handler);
    self->popover_unmap_handler = 0;
  }
}

static void
awra_menu_button_dispose (GObject *object)
{
  AwraMenuButton *self = AWRA_MENU_BUTTON (object);

  if (self->pointer_reset_source != 0) {
    g_source_remove (self->pointer_reset_source);
    self->pointer_reset_source = 0;
  }
  disconnect_popover (self);
  if (self->popover_bin != NULL) {
    gtk_popover_bin_popdown (self->popover_bin);
    gtk_popover_bin_set_popover (self->popover_bin, NULL);
    gtk_popover_bin_set_child (self->popover_bin, NULL);
    gtk_widget_unparent (GTK_WIDGET (self->popover_bin));
    self->popover_bin = NULL;
    self->button = NULL;
    self->popover = NULL;
  }
  g_clear_pointer (&self->label, g_free);
  g_clear_pointer (&self->icon_name, g_free);
  G_OBJECT_CLASS (awra_menu_button_parent_class)->dispose (object);
}

static void
awra_menu_button_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  AwraMenuButton *self = AWRA_MENU_BUTTON (widget);

  if (self->popover_bin != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->popover_bin),
                               snapshot);
}

static void
awra_menu_button_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  AwraMenuButton *self = AWRA_MENU_BUTTON (object);

  switch (property_id) {
  case PROP_LABEL:
    g_value_set_string (value, self->label);
    break;
  case PROP_POPOVER:
    g_value_set_object (value, self->popover);
    break;
  case PROP_APPEARANCE:
    g_value_set_enum (value, self->appearance);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_menu_button_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id) {
  case PROP_LABEL:
    awra_menu_button_set_label (AWRA_MENU_BUTTON (object),
                                g_value_get_string (value));
    break;
  case PROP_POPOVER:
    awra_menu_button_set_popover (AWRA_MENU_BUTTON (object),
                                  g_value_get_object (value));
    break;
  case PROP_APPEARANCE:
    awra_menu_button_set_appearance (AWRA_MENU_BUTTON (object),
                                     g_value_get_enum (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_menu_button_class_init (AwraMenuButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_menu_button_dispose;
  object_class->get_property = awra_menu_button_get_property;
  object_class->set_property = awra_menu_button_set_property;
  widget_class->snapshot = awra_menu_button_snapshot;
  properties[PROP_LABEL] = g_param_spec_string (
    "label", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_POPOVER] = g_param_spec_object (
    "popover", NULL, NULL, AWRA_TYPE_POPOVER,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_APPEARANCE] = g_param_spec_enum (
    "appearance", NULL, NULL, AWRA_TYPE_BUTTON_APPEARANCE,
    AWRA_BUTTON_APPEARANCE_SECONDARY,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class,
                                            GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awramenubutton");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_menu_button_init (AwraMenuButton *self)
{
  GtkGesture *press;

  self->appearance = AWRA_BUTTON_APPEARANCE_SECONDARY;
  self->button = AWRA_BUTTON (awra_button_new ());
  awra_button_set_appearance (self->button, self->appearance);
  gtk_widget_add_css_class (GTK_WIDGET (self->button), "awra-menu-button");
  g_signal_connect (self->button, "clicked",
                    G_CALLBACK (button_clicked_cb), self);
  press = gtk_gesture_click_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (press),
                                               GTK_PHASE_CAPTURE);
  g_signal_connect (press, "pressed", G_CALLBACK (button_press_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self->button),
                             GTK_EVENT_CONTROLLER (press));

  self->popover_bin = GTK_POPOVER_BIN (gtk_popover_bin_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->popover_bin),
                            "awra-menu-button-bin");
  gtk_popover_bin_set_child (self->popover_bin, GTK_WIDGET (self->button));
  gtk_widget_set_parent (GTK_WIDGET (self->popover_bin), GTK_WIDGET (self));
  update_accessible_state (self, FALSE);
}

GtkWidget *
awra_menu_button_new (void)
{
  return g_object_new (AWRA_TYPE_MENU_BUTTON, NULL);
}

const char *
awra_menu_button_get_label (AwraMenuButton *self)
{
  g_return_val_if_fail (AWRA_IS_MENU_BUTTON (self), NULL);
  return self->label;
}

void
awra_menu_button_set_label (AwraMenuButton *self,
                            const char     *label)
{
  gboolean changed;

  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  changed = g_strcmp0 (self->label, label) != 0;
  if (!changed && self->icon_name == NULL)
    return;
  g_free (self->label);
  self->label = g_strdup (label);
  g_clear_pointer (&self->icon_name, g_free);
  gtk_button_set_child (GTK_BUTTON (self->button),
                        label != NULL ? make_label_content (self) : NULL);
  update_button_accessible_label (self, label);
  if (changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

void
awra_menu_button_set_icon_name (AwraMenuButton *self,
                                const char     *icon_name)
{
  gboolean label_changed;

  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  if (g_strcmp0 (self->icon_name, icon_name) == 0 && self->label == NULL)
    return;
  label_changed = self->label != NULL;
  g_clear_pointer (&self->label, g_free);
  g_free (self->icon_name);
  self->icon_name = g_strdup (icon_name);
  gtk_button_set_child (
    GTK_BUTTON (self->button),
    icon_name != NULL ? gtk_image_new_from_icon_name (icon_name) : NULL);
  update_button_accessible_label (self, NULL);
  if (label_changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

GtkWidget *
awra_menu_button_get_child (AwraMenuButton *self)
{
  g_return_val_if_fail (AWRA_IS_MENU_BUTTON (self), NULL);
  return gtk_button_get_child (GTK_BUTTON (self->button));
}

void
awra_menu_button_set_child (AwraMenuButton *self,
                            GtkWidget      *child)
{
  gboolean label_changed;

  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));
  if (gtk_button_get_child (GTK_BUTTON (self->button)) == child)
    return;
  label_changed = self->label != NULL;
  g_clear_pointer (&self->label, g_free);
  g_clear_pointer (&self->icon_name, g_free);
  gtk_button_set_child (GTK_BUTTON (self->button), child);
  update_button_accessible_label (self, NULL);
  if (label_changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

AwraButtonAppearance
awra_menu_button_get_appearance (AwraMenuButton *self)
{
  g_return_val_if_fail (AWRA_IS_MENU_BUTTON (self),
                        AWRA_BUTTON_APPEARANCE_SECONDARY);
  return self->appearance;
}

void
awra_menu_button_set_appearance (AwraMenuButton       *self,
                                 AwraButtonAppearance  appearance)
{
  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  g_return_if_fail (appearance >= AWRA_BUTTON_APPEARANCE_PRIMARY &&
                    appearance <= AWRA_BUTTON_APPEARANCE_SUCCESS);
  if (self->appearance == appearance)
    return;
  self->appearance = appearance;
  awra_button_set_appearance (self->button, appearance);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPEARANCE]);
}

AwraPopover *
awra_menu_button_get_popover (AwraMenuButton *self)
{
  g_return_val_if_fail (AWRA_IS_MENU_BUTTON (self), NULL);
  return self->popover;
}

void
awra_menu_button_set_popover (AwraMenuButton *self,
                              AwraPopover    *popover)
{
  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  g_return_if_fail (popover == NULL || AWRA_IS_POPOVER (popover));
  if (self->popover == popover)
    return;

  if (self->popover_open)
    gtk_popover_bin_popdown (self->popover_bin);
  disconnect_popover (self);
  gtk_popover_bin_set_popover (
    self->popover_bin, popover != NULL ? GTK_WIDGET (popover) : NULL);
  self->popover = popover;
  self->popover_open = FALSE;
  update_accessible_state (self, FALSE);
  if (popover != NULL) {
    self->popover_map_handler = g_signal_connect (
      popover, "map", G_CALLBACK (popover_map_cb), self);
    self->popover_unmap_handler = g_signal_connect (
      popover, "unmap", G_CALLBACK (popover_unmap_cb), self);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPOVER]);
}

void
awra_menu_button_popup (AwraMenuButton *self)
{
  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  if (self->popover != NULL)
    gtk_popover_bin_popup (self->popover_bin);
}

void
awra_menu_button_popdown (AwraMenuButton *self)
{
  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  gtk_popover_bin_popdown (self->popover_bin);
}

void
awra_menu_button_set_menu_model (AwraMenuButton *self,
                                 GMenuModel     *model)
{
  AwraPopover *popover;

  g_return_if_fail (AWRA_IS_MENU_BUTTON (self));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));
  popover = self->popover;
  if (popover == NULL && model != NULL) {
    popover = AWRA_POPOVER (awra_popover_new ());
    awra_menu_button_set_popover (self, popover);
  }
  if (popover != NULL)
    awra_popover_set_menu_model (popover, model);
}
