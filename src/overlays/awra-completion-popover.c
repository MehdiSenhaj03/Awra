/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-completion-popover.h>

#include <awra/awra-context.h>
#include <awra/awra-material-resolution.h>
#include <awra/awra-surface.h>

#include "awra-i18n-private.h"
#include "core/awra-context-private.h"
#include "overlays/awra-completion-popover-private.h"

#define VISIBLE_ROWS 8
/* Calibration, not ABI: the height of a row before one exists to measure. */
#define DEFAULT_ROW_HEIGHT 34
#define CARET_GAP 4
#define MIN_WIDTH 220
#define MAX_WIDTH 420

struct _AwraCompletionPopover {
  GtkPopover parent_instance;

  AwraSurface *surface;
  GtkScrolledWindow *scroller;
  GtkListView *list;
  GtkSingleSelection *selection;
  GListModel *model;
  GtkListItemFactory *factory;

  GtkWidget *attached;
  GtkEventController *keys;
  GtkEventController *focus;
  gulong root_handler;

  AwraContext *context;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_FACTORY,
  PROP_SELECTED,
  N_PROPS,
};

enum {
  SIGNAL_ACTIVATED,
  SIGNAL_DISMISSED,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraCompletionPopover, awra_completion_popover, GTK_TYPE_POPOVER)

/* Matter: the same floating surface and native effects as AwraPopover. */

static double
effect_radius (AwraCompletionPopover *self)
{
  double radius = awra_surface_get_radius (self->surface);
  g_autoptr (AwraMaterialResolution) resolved = NULL;

  if (radius >= 0.0 || self->context == NULL)
    return MAX (radius, 0.0);
  resolved = awra_context_resolve_material (self->context,
                                            awra_surface_get_material (self->surface),
                                            awra_surface_get_role (self->surface),
                                            TRUE,
                                            awra_surface_get_elevation (self->surface));
  return awra_material_resolution_get_radius (resolved);
}

static guint
n_items (AwraCompletionPopover *self)
{
  return self->model != NULL ? g_list_model_get_n_items (self->model) : 0;
}

static void
select_position (AwraCompletionPopover *self,
                 guint                  position)
{
  if (position >= n_items (self))
    return;
  gtk_single_selection_set_selected (self->selection, position);
  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    gtk_list_view_scroll_to (self->list, position, GTK_LIST_SCROLL_NONE, NULL);
}

static void
activate (AwraCompletionPopover *self,
          guint                  position)
{
  if (position >= n_items (self))
    return;
  g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0, position);
}

static void
dismiss (AwraCompletionPopover *self)
{
  if (!gtk_widget_get_visible (GTK_WIDGET (self)))
    return;
  gtk_popover_popdown (GTK_POPOVER (self));
  g_signal_emit (self, signals[SIGNAL_DISMISSED], 0);
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                AwraCompletionPopover *self)
{
  guint count = n_items (self);
  guint selected = gtk_single_selection_get_selected (self->selection);

  (void) controller;
  (void) keycode;
  if (!gtk_widget_get_visible (GTK_WIDGET (self)) || count == 0)
    return FALSE;
  if ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)) != 0)
    return FALSE;
  switch (keyval) {
  case GDK_KEY_Up:
  case GDK_KEY_KP_Up:
    select_position (self, selected == GTK_INVALID_LIST_POSITION || selected == 0
                             ? count - 1 : selected - 1);
    return TRUE;
  case GDK_KEY_Down:
  case GDK_KEY_KP_Down:
    select_position (self, selected == GTK_INVALID_LIST_POSITION || selected + 1 >= count
                             ? 0 : selected + 1);
    return TRUE;
  case GDK_KEY_Page_Up:
  case GDK_KEY_KP_Page_Up:
    select_position (self, selected == GTK_INVALID_LIST_POSITION || selected < VISIBLE_ROWS
                             ? 0 : selected - VISIBLE_ROWS);
    return TRUE;
  case GDK_KEY_Page_Down:
  case GDK_KEY_KP_Page_Down:
    select_position (self, MIN (count - 1, (selected == GTK_INVALID_LIST_POSITION ? 0 : selected) +
                                             VISIBLE_ROWS));
    return TRUE;
  case GDK_KEY_Tab:
  case GDK_KEY_KP_Tab:
  case GDK_KEY_Return:
  case GDK_KEY_KP_Enter:
  case GDK_KEY_ISO_Enter:
    if ((state & GDK_SHIFT_MASK) != 0)
      return FALSE;
    activate (self, selected == GTK_INVALID_LIST_POSITION ? 0 : selected);
    return TRUE;
  case GDK_KEY_Escape:
    dismiss (self);
    return TRUE;
  default:
    return FALSE;
  }
}

static void
focus_leave_cb (GtkEventControllerFocus *controller,
                AwraCompletionPopover   *self)
{
  (void) controller;
  dismiss (self);
}

static void
list_activate_cb (GtkListView           *list,
                  guint                  position,
                  AwraCompletionPopover *self)
{
  (void) list;
  activate (self, position);
}

static void
selection_notify_cb (GtkSingleSelection    *selection,
                     GParamSpec            *pspec,
                     AwraCompletionPopover *self)
{
  (void) selection;
  (void) pspec;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
}

static void
update_max_height (AwraCompletionPopover *self)
{
  GtkWidget *row = gtk_widget_get_first_child (GTK_WIDGET (self->list));
  int height = 0;

  if (row != NULL)
    gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, -1, NULL, &height, NULL, NULL);
  if (height <= 0)
    height = DEFAULT_ROW_HEIGHT;
  gtk_scrolled_window_set_max_content_height (self->scroller, height * VISIBLE_ROWS);
}

static void
items_changed_cb (GListModel            *model,
                  guint                  position,
                  guint                  removed,
                  guint                  added,
                  AwraCompletionPopover *self)
{
  (void) model;
  (void) position;
  (void) removed;
  (void) added;
  if (n_items (self) == 0) {
    gtk_popover_popdown (GTK_POPOVER (self));
    return;
  }
  /* A new filter starts from its best match. */
  gtk_single_selection_set_selected (self->selection, 0);
  if (gtk_widget_get_visible (GTK_WIDGET (self))) {
    gtk_list_view_scroll_to (self->list, 0, GTK_LIST_SCROLL_NONE, NULL);
    gtk_popover_present (GTK_POPOVER (self));
  }
}

static void
set_active_mark (AwraCompletionPopover *self,
                 gboolean               active)
{
  if (self->attached != NULL)
    g_object_set_data (G_OBJECT (self->attached), AWRA_COMPLETION_ACTIVE_KEY,
                       GINT_TO_POINTER (active));
}

static void
awra_completion_popover_map (GtkWidget *widget)
{
  AwraCompletionPopover *self = AWRA_COMPLETION_POPOVER (widget);

  GTK_WIDGET_CLASS (awra_completion_popover_parent_class)->map (widget);
  self->context = awra_context_get_for_display (gtk_widget_get_display (widget));
  awra_effect_coordinator_attach (awra_context_get_effect_coordinator (self->context),
                                  GTK_NATIVE (self),
                                  awra_surface_get_material (self->surface),
                                  effect_radius (self));
  set_active_mark (self, TRUE);
}

static void
awra_completion_popover_unmap (GtkWidget *widget)
{
  AwraCompletionPopover *self = AWRA_COMPLETION_POPOVER (widget);

  set_active_mark (self, FALSE);
  if (self->context != NULL)
    awra_effect_coordinator_detach (awra_context_get_effect_coordinator (self->context),
                                    GTK_NATIVE (self));
  GTK_WIDGET_CLASS (awra_completion_popover_parent_class)->unmap (widget);
}

static void
awra_completion_popover_size_allocate (GtkWidget *widget,
                                       int        width,
                                       int        height,
                                       int        baseline)
{
  AwraCompletionPopover *self = AWRA_COMPLETION_POPOVER (widget);

  GTK_WIDGET_CLASS (awra_completion_popover_parent_class)->size_allocate (widget, width, height,
                                                                          baseline);
  if (self->context != NULL && gtk_widget_get_mapped (widget))
    awra_effect_coordinator_update (awra_context_get_effect_coordinator (self->context),
                                    GTK_NATIVE (self),
                                    awra_surface_get_material (self->surface),
                                    effect_radius (self));
}

/* The popover is a child of the text widget only while that widget is in a
 * window: text widgets remove the children they know about when they go
 * away, and complain about any other. While attached, the popover holds a
 * reference on itself, released with the text widget. */

static void
sync_parent (AwraCompletionPopover *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  gboolean rooted = self->attached != NULL && gtk_widget_get_root (self->attached) != NULL;

  if (rooted && gtk_widget_get_parent (widget) == NULL) {
    gtk_widget_set_parent (widget, self->attached);
  } else if (!rooted && gtk_widget_get_parent (widget) != NULL) {
    gtk_popover_popdown (GTK_POPOVER (self));
    gtk_widget_unparent (widget);
  }
}

static void
attached_root_cb (GtkWidget             *text_widget,
                  GParamSpec            *pspec,
                  AwraCompletionPopover *self)
{
  (void) text_widget;
  (void) pspec;
  sync_parent (self);
}

static void
attached_finalized_cb (gpointer  data,
                       GObject  *where_the_object_was)
{
  AwraCompletionPopover *self = data;

  (void) where_the_object_was;
  /* Its controllers and handlers went with it. */
  self->attached = NULL;
  self->keys = NULL;
  self->focus = NULL;
  self->root_handler = 0;
  if (gtk_widget_get_parent (GTK_WIDGET (self)) != NULL)
    gtk_widget_unparent (GTK_WIDGET (self));
  g_object_unref (self);
}

static void
detach (AwraCompletionPopover *self,
        gboolean               release)
{
  GtkWidget *attached = self->attached;

  if (attached == NULL)
    return;
  set_active_mark (self, FALSE);
  gtk_popover_popdown (GTK_POPOVER (self));
  g_clear_signal_handler (&self->root_handler, attached);
  g_object_weak_unref (G_OBJECT (attached), attached_finalized_cb, self);
  gtk_widget_remove_controller (attached, self->keys);
  gtk_widget_remove_controller (attached, self->focus);
  self->keys = NULL;
  self->focus = NULL;
  self->attached = NULL;
  if (gtk_widget_get_parent (GTK_WIDGET (self)) != NULL)
    gtk_widget_unparent (GTK_WIDGET (self));
  /* The reference taken by attach (); the caller may hold others. */
  if (release)
    g_object_unref (self);
}

static void
awra_completion_popover_dispose (GObject *object)
{
  AwraCompletionPopover *self = AWRA_COMPLETION_POPOVER (object);

  /* Disposed while attached: its own reference goes with it. */
  detach (self, FALSE);
  if (self->selection != NULL) {
    g_signal_handlers_disconnect_by_data (self->selection, self);
    gtk_list_view_set_model (self->list, NULL);
    g_clear_object (&self->selection);
  }
  if (self->model != NULL) {
    g_signal_handlers_disconnect_by_data (self->model, self);
    g_clear_object (&self->model);
  }
  g_clear_object (&self->factory);
  self->surface = NULL;
  self->list = NULL;
  self->scroller = NULL;
  self->context = NULL;
  G_OBJECT_CLASS (awra_completion_popover_parent_class)->dispose (object);
}

static void
awra_completion_popover_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  AwraCompletionPopover *self = AWRA_COMPLETION_POPOVER (object);

  switch (prop_id) {
  case PROP_MODEL:
    g_value_set_object (value, self->model);
    break;
  case PROP_FACTORY:
    g_value_set_object (value, self->factory);
    break;
  case PROP_SELECTED:
    g_value_set_uint (value, awra_completion_popover_get_selected (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_completion_popover_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  AwraCompletionPopover *self = AWRA_COMPLETION_POPOVER (object);

  switch (prop_id) {
  case PROP_MODEL:
    awra_completion_popover_set_model (self, g_value_get_object (value));
    break;
  case PROP_FACTORY:
    awra_completion_popover_set_factory (self, g_value_get_object (value));
    break;
  case PROP_SELECTED:
    awra_completion_popover_set_selected (self, g_value_get_uint (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_completion_popover_class_init (AwraCompletionPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_completion_popover_dispose;
  object_class->get_property = awra_completion_popover_get_property;
  object_class->set_property = awra_completion_popover_set_property;
  widget_class->map = awra_completion_popover_map;
  widget_class->unmap = awra_completion_popover_unmap;
  widget_class->size_allocate = awra_completion_popover_size_allocate;

  /**
   * AwraCompletionPopover:model:
   *
   * The suggestions, already filtered for what is typed.
   *
   * Since: 1.1
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL, G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraCompletionPopover:factory:
   *
   * The factory that makes rows.
   *
   * Since: 1.1
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL, GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraCompletionPopover:selected:
   *
   * The position of the selected suggestion, or %GTK_INVALID_LIST_POSITION.
   *
   * Since: 1.1
   */
  properties[PROP_SELECTED] =
    g_param_spec_uint ("selected", NULL, NULL, 0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraCompletionPopover::activated:
   * @self: the completion popover
   * @position: the position of the chosen suggestion
   *
   * Emitted when a suggestion is chosen with Tab, Enter or a click. The
   * handler inserts it and usually hides the popover.
   *
   * Since: 1.1
   */
  signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
  /**
   * AwraCompletionPopover::dismissed:
   * @self: the completion popover
   *
   * Emitted when the user closes the popover with Escape or by leaving the
   * text, so the application stops offering completions for this word.
   *
   * Since: 1.1
   */
  signals[SIGNAL_DISMISSED] =
    g_signal_new ("dismissed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
awra_completion_popover_init (AwraCompletionPopover *self)
{
  self->surface = AWRA_SURFACE (awra_surface_new_with_role (AWRA_SURFACE_ROLE_POPOVER));
  awra_surface_set_elevation_level (self->surface, AWRA_ELEVATION_FLOATING);
  self->selection = gtk_single_selection_new (NULL);
  gtk_single_selection_set_autoselect (self->selection, TRUE);
  gtk_single_selection_set_can_unselect (self->selection, FALSE);
  g_signal_connect (self->selection, "notify::selected", G_CALLBACK (selection_notify_cb), self);

  self->list = GTK_LIST_VIEW (gtk_list_view_new (g_object_ref (GTK_SELECTION_MODEL (self->selection)),
                                                 NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self->list), "awra-completion-list");
  /* Hovering selects, a click activates, and the text keeps the focus. */
  gtk_list_view_set_single_click_activate (self->list, TRUE);
  gtk_widget_set_can_focus (GTK_WIDGET (self->list), FALSE);
  g_signal_connect (self->list, "activate", G_CALLBACK (list_activate_cb), self);

  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  /* The minimum size must not depend on the rows: they only exist once the
   * popover is shown, and a popup that outgrows its surface is closed. */
  gtk_scrolled_window_set_policy (self->scroller, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_height (self->scroller, TRUE);
  gtk_scrolled_window_set_propagate_natural_width (self->scroller, TRUE);
  gtk_scrolled_window_set_min_content_width (self->scroller, MIN_WIDTH);
  gtk_scrolled_window_set_max_content_width (self->scroller, MAX_WIDTH);
  gtk_scrolled_window_set_max_content_height (self->scroller, DEFAULT_ROW_HEIGHT * VISIBLE_ROWS);
  gtk_scrolled_window_set_child (self->scroller, GTK_WIDGET (self->list));
  awra_surface_set_child (self->surface, GTK_WIDGET (self->scroller));

  gtk_popover_set_child (GTK_POPOVER (self), GTK_WIDGET (self->surface));
  gtk_popover_set_has_arrow (GTK_POPOVER (self), FALSE);
  gtk_popover_set_autohide (GTK_POPOVER (self), FALSE);
  gtk_popover_set_position (GTK_POPOVER (self), GTK_POS_TOP);
  gtk_popover_set_offset (GTK_POPOVER (self), 0, -CARET_GAP);
  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_START);
  gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-popover");
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-completion-popover");
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->list),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Suggestions"), -1);
}

GtkWidget *
awra_completion_popover_new (void)
{
  return g_object_new (AWRA_TYPE_COMPLETION_POPOVER, NULL);
}

GListModel *
awra_completion_popover_get_model (AwraCompletionPopover *self)
{
  g_return_val_if_fail (AWRA_IS_COMPLETION_POPOVER (self), NULL);
  return self->model;
}

void
awra_completion_popover_set_model (AwraCompletionPopover *self,
                                   GListModel            *model)
{
  g_return_if_fail (AWRA_IS_COMPLETION_POPOVER (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  if (self->model == model)
    return;
  if (self->model != NULL)
    g_signal_handlers_disconnect_by_data (self->model, self);
  g_set_object (&self->model, model);
  gtk_single_selection_set_model (self->selection, model);
  if (model != NULL)
    g_signal_connect_after (model, "items-changed", G_CALLBACK (items_changed_cb), self);
  if (n_items (self) == 0)
    gtk_popover_popdown (GTK_POPOVER (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

GtkListItemFactory *
awra_completion_popover_get_factory (AwraCompletionPopover *self)
{
  g_return_val_if_fail (AWRA_IS_COMPLETION_POPOVER (self), NULL);
  return self->factory;
}

void
awra_completion_popover_set_factory (AwraCompletionPopover *self,
                                     GtkListItemFactory    *factory)
{
  g_return_if_fail (AWRA_IS_COMPLETION_POPOVER (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));
  if (!g_set_object (&self->factory, factory))
    return;
  gtk_list_view_set_factory (self->list, factory);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

guint
awra_completion_popover_get_selected (AwraCompletionPopover *self)
{
  g_return_val_if_fail (AWRA_IS_COMPLETION_POPOVER (self), GTK_INVALID_LIST_POSITION);
  return gtk_single_selection_get_selected (self->selection);
}

void
awra_completion_popover_set_selected (AwraCompletionPopover *self,
                                      guint                  position)
{
  g_return_if_fail (AWRA_IS_COMPLETION_POPOVER (self));
  select_position (self, position);
}

GtkWidget *
awra_completion_popover_get_attached_widget (AwraCompletionPopover *self)
{
  g_return_val_if_fail (AWRA_IS_COMPLETION_POPOVER (self), NULL);
  return self->attached;
}

void
awra_completion_popover_attach (AwraCompletionPopover *self,
                                GtkWidget             *text_widget)
{
  g_return_if_fail (AWRA_IS_COMPLETION_POPOVER (self));
  g_return_if_fail (text_widget == NULL || GTK_IS_WIDGET (text_widget));
  if (self->attached == text_widget)
    return;
  if (text_widget != NULL)
    g_object_ref_sink (self);
  else
    g_object_ref (self);
  detach (self, TRUE);
  if (text_widget != NULL) {
    /* Kept by detach (), or by the text widget's lifetime. */
    g_object_ref (self);
    self->attached = text_widget;
    g_object_weak_ref (G_OBJECT (text_widget), attached_finalized_cb, self);
    self->root_handler = g_signal_connect (text_widget, "notify::root",
                                           G_CALLBACK (attached_root_cb), self);
    self->keys = gtk_event_controller_key_new ();
    gtk_event_controller_set_propagation_phase (self->keys, GTK_PHASE_CAPTURE);
    g_signal_connect (self->keys, "key-pressed", G_CALLBACK (key_pressed_cb), self);
    gtk_widget_add_controller (text_widget, self->keys);
    self->focus = gtk_event_controller_focus_new ();
    g_signal_connect (self->focus, "leave", G_CALLBACK (focus_leave_cb), self);
    gtk_widget_add_controller (text_widget, self->focus);
    sync_parent (self);
  }
  g_object_unref (self);
}

/* Where the caret is, in the coordinates of the attached widget. */
static gboolean
caret_rectangle (AwraCompletionPopover *self,
                 GdkRectangle          *rect)
{
  if (GTK_IS_TEXT_VIEW (self->attached)) {
    GtkTextView *view = GTK_TEXT_VIEW (self->attached);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
    GtkTextIter iter;
    GdkRectangle location;

    gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
    gtk_text_view_get_iter_location (view, &iter, &location);
    gtk_text_view_buffer_to_window_coords (view, GTK_TEXT_WINDOW_WIDGET,
                                           location.x, location.y, &rect->x, &rect->y);
    rect->width = 1;
    rect->height = MAX (location.height, 1);
    return TRUE;
  }
  if (GTK_IS_EDITABLE (self->attached)) {
    GtkEditable *editable = GTK_EDITABLE (self->attached);
    GtkEditable *delegate = gtk_editable_get_delegate (editable);
    GtkWidget *text = GTK_WIDGET (delegate != NULL ? delegate : editable);
    const char *contents = gtk_editable_get_text (editable);
    int position = gtk_editable_get_position (editable);
    g_autofree char *before = g_utf8_substring (contents, 0, position);
    g_autoptr (PangoLayout) layout = gtk_widget_create_pango_layout (text, before);
    graphene_rect_t bounds;
    int width = 0;

    if (!gtk_widget_compute_bounds (text, self->attached, &bounds))
      return FALSE;
    /* The caret as measured from the start of the text; a field scrolled
     * sideways keeps it within its bounds. */
    pango_layout_get_pixel_size (layout, &width, NULL);
    rect->x = (int) (bounds.origin.x + MIN (width, bounds.size.width));
    if (gtk_widget_get_direction (text) == GTK_TEXT_DIR_RTL)
      rect->x = (int) (bounds.origin.x + bounds.size.width - MIN (width, bounds.size.width));
    rect->y = (int) bounds.origin.y;
    rect->width = 1;
    rect->height = MAX ((int) bounds.size.height, 1);
    return TRUE;
  }
  return FALSE;
}

void
awra_completion_popover_present_at_cursor (AwraCompletionPopover *self)
{
  GdkRectangle rect;

  g_return_if_fail (AWRA_IS_COMPLETION_POPOVER (self));
  g_return_if_fail (self->attached != NULL);
  if (n_items (self) == 0 || gtk_widget_get_parent (GTK_WIDGET (self)) == NULL) {
    gtk_popover_popdown (GTK_POPOVER (self));
    return;
  }
  if (caret_rectangle (self, &rect))
    gtk_popover_set_pointing_to (GTK_POPOVER (self), &rect);
  else
    gtk_popover_set_pointing_to (GTK_POPOVER (self), NULL);
  if (!gtk_widget_get_visible (GTK_WIDGET (self)) ||
      gtk_single_selection_get_selected (self->selection) == GTK_INVALID_LIST_POSITION)
    gtk_single_selection_set_selected (self->selection, 0);
  update_max_height (self);
  if (!gtk_widget_get_visible (GTK_WIDGET (self)))
    gtk_popover_popup (GTK_POPOVER (self));
  else
    gtk_popover_present (GTK_POPOVER (self));
}
