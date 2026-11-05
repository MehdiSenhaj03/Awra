/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-context.h>
#include <awra/awra-button.h>
#include <awra/awra-menu-button.h>
#include <awra/awra-popover.h>
#include <awra/awra-layout.h>

#include "awra-config.h"
#include "core/awra-context-private.h"
#include "awra-i18n-private.h"
#ifdef AWRA_HAVE_WIN32
#include <gdk/win32/gdkwin32.h>
#include "platform/win32/awra-win32-private.h"
#endif

struct _AwraPopover {
  GtkPopover parent_instance;
  AwraSurface *surface;
  AwraContext *context;
  gulong material_handler;
  gulong style_handler;
  GMenuModel *menu_model;
};

enum {
  PROP_0,
  PROP_CHILD,
  PROP_SURFACE,
  PROP_MENU_MODEL,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraPopover, awra_popover, GTK_TYPE_POPOVER)

static double
get_effect_radius (AwraPopover *self)
{
  double radius = awra_surface_get_radius (self->surface);

  if (radius >= 0.0)
    return radius;
  if (self->context != NULL) {
    g_autoptr (AwraMaterialResolution) resolved = NULL;
    GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
    gboolean active = !GTK_IS_WINDOW (root) ||
                      gtk_window_is_active (GTK_WINDOW (root));

    resolved = awra_context_resolve_material (
      self->context,
      awra_surface_get_material (self->surface),
      awra_surface_get_role (self->surface),
      active,
      awra_surface_get_elevation (self->surface));
    return awra_material_resolution_get_radius (resolved);
  }
  return 16.0;
}

static void
sync_native_effect (AwraPopover *self)
{
  if (self->context == NULL || !gtk_widget_get_mapped (GTK_WIDGET (self)))
    return;

  awra_effect_coordinator_update (
    awra_context_get_effect_coordinator (self->context),
    GTK_NATIVE (self),
    awra_surface_get_material (self->surface),
    get_effect_radius (self));
}

static void
material_changed_cb (AwraSurface *surface,
                     GParamSpec  *pspec,
                     AwraPopover *self)
{
  (void) surface;
  (void) pspec;
  sync_native_effect (self);
}

static void
style_changed_cb (AwraStyleManager *manager,
                  GParamSpec       *pspec,
                  AwraPopover      *self)
{
  (void) manager;
  (void) pspec;
  sync_native_effect (self);
}

static void rebuild_menu (AwraPopover *self);

static void
awra_popover_show (GtkWidget *widget)
{
  AwraPopover *self = AWRA_POPOVER (widget);

  if (self->menu_model != NULL) {
    rebuild_menu (self);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
  }
  GTK_WIDGET_CLASS (awra_popover_parent_class)->show (widget);
}

static void
awra_popover_map (GtkWidget *widget)
{
  AwraPopover *self = AWRA_POPOVER (widget);

  GTK_WIDGET_CLASS (awra_popover_parent_class)->map (widget);
  self->context = awra_context_get_for_display (gtk_widget_get_display (widget));
#ifdef AWRA_HAVE_WIN32
  /* The popover is its own HWND there: DWM rounds and shadows it, so draw
   * the same corner and no shadow of our own (it would square the corners). */
  if (GDK_IS_WIN32_DISPLAY (gtk_widget_get_display (widget))) {
    GdkSurface *native = gtk_native_get_surface (GTK_NATIVE (self));

    double radius = awra_win32_popup_corner_radius ();

    awra_surface_set_radius (self->surface, radius);
    awra_surface_set_elevation_level (self->surface, AWRA_ELEVATION_FLAT);
    /* GtkPopover clips its child to the contents' radius: match it too. */
    gtk_widget_add_css_class (widget, radius > 0.0 ? "awra-win32-popup"
                                                   : "awra-win32-square-popup");
    if (GDK_IS_WIN32_SURFACE (native))
      awra_win32_style_frame (gdk_win32_surface_get_handle (native), TRUE);
  }
#endif
  if (self->style_handler == 0) {
    self->style_handler = g_signal_connect (
      awra_context_get_style_manager (self->context),
      "notify::token-set",
      G_CALLBACK (style_changed_cb),
      self);
  }
  awra_effect_coordinator_attach (
    awra_context_get_effect_coordinator (self->context),
    GTK_NATIVE (self),
    awra_surface_get_material (self->surface),
    get_effect_radius (self));
}

static void
awra_popover_unmap (GtkWidget *widget)
{
  AwraPopover *self = AWRA_POPOVER (widget);

  if (self->context != NULL) {
    awra_effect_coordinator_detach (
      awra_context_get_effect_coordinator (self->context),
      GTK_NATIVE (self));
  }
  if (self->style_handler != 0) {
    g_signal_handler_disconnect (
      awra_context_get_style_manager (self->context), self->style_handler);
    self->style_handler = 0;
  }
  GTK_WIDGET_CLASS (awra_popover_parent_class)->unmap (widget);
}

static void
awra_popover_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GTK_WIDGET_CLASS (awra_popover_parent_class)->size_allocate (widget,
                                                               width,
                                                               height,
                                                               baseline);
  sync_native_effect (AWRA_POPOVER (widget));
}

static void
awra_popover_dispose (GObject *object)
{
  AwraPopover *self = AWRA_POPOVER (object);

  if (self->material_handler != 0) {
    g_signal_handler_disconnect (self->surface, self->material_handler);
    self->material_handler = 0;
  }
  if (self->style_handler != 0 && self->context != NULL) {
    g_signal_handler_disconnect (
      awra_context_get_style_manager (self->context), self->style_handler);
    self->style_handler = 0;
  }
  self->surface = NULL;
  self->context = NULL;
  g_clear_object (&self->menu_model);

  G_OBJECT_CLASS (awra_popover_parent_class)->dispose (object);
}

static void
awra_popover_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  AwraPopover *self = AWRA_POPOVER (object);

  switch (property_id) {
  case PROP_CHILD:
    g_value_set_object (value, awra_surface_get_child (self->surface));
    break;
  case PROP_SURFACE:
    g_value_set_object (value, self->surface);
    break;
  case PROP_MENU_MODEL:
    g_value_set_object (value, self->menu_model);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_popover_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  switch (property_id) {
  case PROP_CHILD:
    awra_popover_set_child (AWRA_POPOVER (object), g_value_get_object (value));
    break;
  case PROP_MENU_MODEL:
    awra_popover_set_menu_model (AWRA_POPOVER (object), g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_popover_class_init (AwraPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_popover_dispose;
  object_class->get_property = awra_popover_get_property;
  object_class->set_property = awra_popover_set_property;
  widget_class->show = awra_popover_show;
  widget_class->map = awra_popover_map;
  widget_class->unmap = awra_popover_unmap;
  widget_class->size_allocate = awra_popover_size_allocate;

  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SURFACE] =
    g_param_spec_object ("surface", NULL, NULL, AWRA_TYPE_SURFACE,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_MENU_MODEL] =
    g_param_spec_object ("menu-model", NULL, NULL, G_TYPE_MENU_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_popover_init (AwraPopover *self)
{
  self->surface = AWRA_SURFACE (awra_surface_new_with_role (AWRA_SURFACE_ROLE_POPOVER));
  awra_surface_set_elevation_level (self->surface,
                                    AWRA_ELEVATION_FLOATING);
  gtk_popover_set_child (GTK_POPOVER (self), GTK_WIDGET (self->surface));
  gtk_popover_set_has_arrow (GTK_POPOVER (self), FALSE);
  gtk_popover_set_autohide (GTK_POPOVER (self), TRUE);
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-popover");
  self->material_handler = g_signal_connect (self->surface,
                                             "notify::material",
                                             G_CALLBACK (material_changed_cb),
                                             self);
}

GtkWidget *
awra_popover_new (void)
{
  return g_object_new (AWRA_TYPE_POPOVER, NULL);
}

GtkWidget *
awra_popover_get_child (AwraPopover *self)
{
  g_return_val_if_fail (AWRA_IS_POPOVER (self), NULL);
  return awra_surface_get_child (self->surface);
}

void
awra_popover_set_child (AwraPopover *self,
                        GtkWidget   *child)
{
  g_return_if_fail (AWRA_IS_POPOVER (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (awra_surface_get_child (self->surface) == child)
    return;
  if (self->menu_model != NULL) {
    g_clear_object (&self->menu_model);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_MODEL]);
  }
  gtk_popover_set_cascade_popdown (GTK_POPOVER (self), FALSE);
  awra_surface_set_child (self->surface, child);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

AwraSurface *
awra_popover_get_surface (AwraPopover *self)
{
  g_return_val_if_fail (AWRA_IS_POPOVER (self), NULL);
  return self->surface;
}

static void
submenu_chevron_draw_cb (GtkDrawingArea *area,
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
  cairo_move_to (cr, center_x - 1.5, center_y - 3.0);
  cairo_line_to (cr, center_x + 1.8, center_y);
  cairo_line_to (cr, center_x - 1.5, center_y + 3.0);
  cairo_stroke (cr);
}

static GtkWidget *
make_submenu_chevron (void)
{
  GtkWidget *chevron = gtk_drawing_area_new ();

  gtk_widget_set_size_request (chevron, 10, 10);
  gtk_widget_add_css_class (chevron, "awra-menu-item-chevron");
  gtk_drawing_area_set_draw_func (
    GTK_DRAWING_AREA (chevron),
    submenu_chevron_draw_cb, NULL, NULL);
  return chevron;
}

static char *
get_item_accel_label (GMenuModel *model,
                      int         position,
                      const char *action,
                      GVariant   *target,
                      GtkWidget  *anchor)
{
  g_autofree char *accel = NULL;
  g_autofree char *detailed = NULL;
  g_auto (GStrv) accels = NULL;
  GdkModifierType modifiers;
  guint key;

  if (!g_menu_model_get_item_attribute (model, position, "accel", "s", &accel) &&
      action != NULL) {
    GtkRoot *root = anchor != NULL ? gtk_widget_get_root (anchor) : NULL;
    GtkApplication *application = GTK_IS_WINDOW (root)
                                    ? gtk_window_get_application (GTK_WINDOW (root))
                                    : NULL;

    if (application == NULL)
      return NULL;
    detailed = g_action_print_detailed_name (action, target);
    accels = gtk_application_get_accels_for_action (application, detailed);
    if (accels[0] == NULL)
      return NULL;
    accel = g_strdup (accels[0]);
  }
  if (accel == NULL || !gtk_accelerator_parse (accel, &key, &modifiers))
    return NULL;
  return gtk_accelerator_get_label (key, modifiers);
}

static GtkWidget *
make_menu_item_content (GMenuModel *model,
                        int         position,
                        const char *label,
                        const char *accel_label,
                        gboolean    submenu)
{
  g_autoptr (GVariant) serialized_icon = NULL;
  g_autoptr (GIcon) icon = NULL;
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *text = gtk_label_new (label != NULL ? label : _("Action"));

  awra_box_set_spacing (GTK_BOX (box), AWRA_SPACING_SM);
  serialized_icon = g_menu_model_get_item_attribute_value (
    model, position, G_MENU_ATTRIBUTE_ICON, NULL);
  if (serialized_icon != NULL)
    icon = g_icon_deserialize (serialized_icon);
  if (icon != NULL) {
    GtkWidget *image = gtk_image_new_from_gicon (icon);

    gtk_widget_add_css_class (image, "awra-menu-item-icon");
    gtk_box_append (GTK_BOX (box), image);
  }
  gtk_widget_set_hexpand (text, TRUE);
  gtk_label_set_xalign (GTK_LABEL (text), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (text), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (text, "awra-menu-item-label");
  gtk_box_append (GTK_BOX (box), text);
  if (accel_label != NULL) {
    GtkWidget *accel = gtk_label_new (accel_label);

    gtk_widget_add_css_class (accel, "awra-menu-item-accel");
    gtk_box_append (GTK_BOX (box), accel);
  }
  if (submenu)
    gtk_box_append (GTK_BOX (box), make_submenu_chevron ());
  gtk_widget_add_css_class (box, "awra-menu-item-content");
  return box;
}

static void
popdown_menu_chain (AwraPopover *self)
{
  g_autoptr (GPtrArray) chain = g_ptr_array_new_with_free_func (g_object_unref);
  GtkWidget *cursor = GTK_WIDGET (self);

  while (cursor != NULL) {
    if (AWRA_IS_POPOVER (cursor))
      g_ptr_array_add (chain, g_object_ref (cursor));
    cursor = gtk_widget_get_parent (cursor);
  }
  for (guint i = 0; i < chain->len; i++)
    gtk_popover_popdown (GTK_POPOVER (g_ptr_array_index (chain, i)));
}

static void
menu_item_clicked_cb (GtkButton    *button,
                      AwraPopover *self)
{
  (void) button;
  popdown_menu_chain (self);
}

static void
append_menu_model (AwraPopover *self,
                   GtkBox      *box,
                   GMenuModel  *model)
{
  int n_items = g_menu_model_get_n_items (model);

  for (int i = 0; i < n_items; i++) {
    g_autofree char *label = NULL;
    g_autofree char *action = NULL;
    g_autoptr (GMenuModel) section = NULL;
    g_autoptr (GMenuModel) submenu = NULL;
    g_autoptr (GVariant) target = NULL;

    section = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
    if (section != NULL) {
      if (gtk_widget_get_first_child (GTK_WIDGET (box)) != NULL) {
        GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

        gtk_widget_add_css_class (separator, "awra-menu-separator");
        gtk_box_append (box, separator);
      }
      append_menu_model (self, box, section);
      continue;
    }

    (void) g_menu_model_get_item_attribute (model,
                                             i,
                                             G_MENU_ATTRIBUTE_LABEL,
                                             "s",
                                             &label);
    submenu = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);
    if (submenu != NULL) {
      GtkWidget *button = awra_menu_button_new ();
      GtkWidget *popover = awra_popover_new ();
      awra_menu_button_set_child (
        AWRA_MENU_BUTTON (button),
        make_menu_item_content (model, i, label, NULL, TRUE));
      awra_popover_set_menu_model (AWRA_POPOVER (popover), submenu);
      gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_RIGHT);
      awra_menu_button_set_popover (AWRA_MENU_BUTTON (button), AWRA_POPOVER (popover));
      gtk_widget_add_css_class (button, "awra-menu-item");
      gtk_box_append (box, button);
      continue;
    }

    /* A toggle button lets GtkActionHelper report a boolean action's state,
     * or whether a stateful action currently equals this item's target. */
    GtkWidget *button = gtk_toggle_button_new ();
    GtkWidget *check = gtk_image_new_from_icon_name ("object-select-symbolic");
    GtkWidget *content;
    g_autofree char *accel_label = NULL;

    if (g_menu_model_get_item_attribute (model,
                                         i,
                                         G_MENU_ATTRIBUTE_ACTION,
                                         "s",
                                         &action)) {
      target = g_menu_model_get_item_attribute_value (model,
                                                       i,
                                                       G_MENU_ATTRIBUTE_TARGET,
                                                       NULL);
      if (target != NULL)
        gtk_actionable_set_action_target_value (GTK_ACTIONABLE (button), target);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action);
    }
    accel_label = get_item_accel_label (model, i, action, target,
                                        gtk_widget_get_parent (GTK_WIDGET (self)));
    content = make_menu_item_content (model, i, label, accel_label, FALSE);
    gtk_widget_add_css_class (check, "awra-menu-item-check");
    g_object_bind_property (button, "active", check, "visible",
                            G_BINDING_SYNC_CREATE);
    gtk_box_append (GTK_BOX (content), check);
    gtk_button_set_child (GTK_BUTTON (button), content);
    gtk_widget_add_css_class (button, "awra-menu-item");
    g_signal_connect (button, "clicked",
                      G_CALLBACK (menu_item_clicked_cb), self);
    gtk_box_append (box, button);
  }
}

/* Items are built from the model as it is when the menu opens: models
 * filled or changed after awra_popover_set_menu_model() show up too. */
static void
rebuild_menu (AwraPopover *self)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *scroller = gtk_scrolled_window_new ();
  int maximum_height = 600;
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (self));
  GtkNative *native = parent != NULL ? gtk_widget_get_native (parent) : NULL;
  if (native != NULL) {
    GdkSurface *surface = gtk_native_get_surface (native);
    GdkMonitor *monitor = surface != NULL ? gdk_display_get_monitor_at_surface (gtk_widget_get_display (GTK_WIDGET (self)), surface) : NULL;
    if (monitor != NULL) {
      GdkRectangle geometry;
      gdk_monitor_get_geometry (monitor, &geometry);
      maximum_height = MAX (120, geometry.height - 64);
    }
  }

  awra_box_set_spacing (GTK_BOX (box), AWRA_SPACING_XS);
  gtk_widget_add_css_class (box, "awra-menu");
  gtk_widget_add_css_class (scroller, "awra-menu-scroller");
  append_menu_model (self, GTK_BOX (box), self->menu_model);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (scroller), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), maximum_height);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), box);
  awra_surface_set_child (self->surface, scroller);
}

GMenuModel *
awra_popover_get_menu_model (AwraPopover *self)
{
  g_return_val_if_fail (AWRA_IS_POPOVER (self), NULL);
  return self->menu_model;
}

void
awra_popover_set_menu_model (AwraPopover *self,
                             GMenuModel   *model)
{
  g_return_if_fail (AWRA_IS_POPOVER (self));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  if (!g_set_object (&self->menu_model, model))
    return;

  if (model == NULL) {
    gtk_popover_set_cascade_popdown (GTK_POPOVER (self), FALSE);
    awra_surface_set_child (self->surface, NULL);
  } else {
    /* Menu popovers form one modal chain. When a submenu is dismissed by an
     * outside click, GTK also closes its parent instead of leaving a detached
     * menu behind. Arbitrary-content popovers keep the GTK default. */
    gtk_popover_set_cascade_popdown (GTK_POPOVER (self), TRUE);
    rebuild_menu (self);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_MODEL]);
}
