/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-breadcrumb-bar.h>
#include <awra/awra-context.h>
#include <awra/awra-layout.h>
#include <awra/awra-motion.h>
#include <awra/awra-path-bar.h>
#include <awra/awra-popover.h>

#include "awra-i18n-private.h"

enum {
  /* Completion is a short list of next steps, not a file browser. */
  MAX_VISIBLE_SUGGESTIONS = 8,
};

struct _AwraPathBar {
  GtkWidget parent_instance;
  GtkBox *box;
  GtkImage *icon;
  GtkStack *stack;
  AwraBreadcrumbBar *breadcrumbs;
  GtkText *entry;
  GtkWidget *end_widget;
  GtkPopover *popover;
  GtkBox *suggestion_box;
  GListModel *suggestions;
  gulong suggestions_handler;
  char *text;
  char *icon_name;
  guint focus_idle;
  int highlighted;
  gboolean editing;
  gboolean syncing;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_TEXT,
  PROP_EDIT_TEXT,
  PROP_PLACEHOLDER_TEXT,
  PROP_ICON_NAME,
  PROP_END_WIDGET,
  PROP_EDITING,
  PROP_SUGGESTIONS,
  N_PROPS,
};

enum { ACTIVATE_ITEM, SUBMIT, N_SIGNALS };

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraPathBar, awra_path_bar, GTK_TYPE_WIDGET)

static void
sync_transition (AwraPathBar *self)
{
  AwraContext *context = awra_context_get_for_display (
    gtk_widget_get_display (GTK_WIDGET (self)));

  gtk_stack_set_transition_duration (
    self->stack,
    awra_motion_get_duration (awra_context_get_style_manager (context),
                              AWRA_MOTION_PRESET_FAST));
}

static void
sync_popover (AwraPathBar *self)
{
  gboolean shown = self->editing &&
                   gtk_widget_get_mapped (GTK_WIDGET (self)) &&
                   gtk_widget_get_first_child (
                     GTK_WIDGET (self->suggestion_box)) != NULL;

  if (shown) {
    gtk_widget_set_size_request (GTK_WIDGET (self->suggestion_box),
                                 gtk_widget_get_width (GTK_WIDGET (self)), -1);
    gtk_popover_popup (self->popover);
  } else {
    gtk_popover_popdown (self->popover);
  }
}

static void
apply_highlight (AwraPathBar *self)
{
  int index = 0;

  for (GtkWidget *child = gtk_widget_get_first_child (
         GTK_WIDGET (self->suggestion_box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child), index++) {
    if (index == self->highlighted)
      gtk_widget_add_css_class (child, "awra-path-bar-suggestion-highlighted");
    else
      gtk_widget_remove_css_class (child, "awra-path-bar-suggestion-highlighted");
  }
}

static void
replace_text (AwraPathBar *self,
              const char  *text)
{
  /* @text may belong to a suggestion that observers replace on notification. */
  g_autofree char *owned = g_strdup (text != NULL ? text : "");

  self->syncing = TRUE;
  gtk_editable_set_text (GTK_EDITABLE (self->entry), owned);
  gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);
  self->syncing = FALSE;
  self->highlighted = -1;
  apply_highlight (self);
  /* GtkEditable reports the intermediate empty text: announce only the result. */
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDIT_TEXT]);
}

static gboolean
submit_text (AwraPathBar *self,
             const char  *text)
{
  gboolean handled = FALSE;
  g_autofree char *owned = g_strdup (text);

  g_object_ref (self);
  g_signal_emit (self, signals[SUBMIT], 0, owned, &handled);
  if (handled) {
    awra_path_bar_set_editing (self, FALSE);
  } else {
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-path-bar-invalid");
    gtk_widget_error_bell (GTK_WIDGET (self));
  }
  g_object_unref (self);
  return handled;
}

static void
suggestion_clicked_cb (GtkButton   *button,
                       AwraPathBar *self)
{
  /* A suggestion is a step, not a destination: the user may keep descending. */
  replace_text (self, awra_breadcrumb_item_get_name (
    g_object_get_data (G_OBJECT (button), "awra-path-bar-item")));
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
rebuild_suggestions (AwraPathBar *self)
{
  GtkWidget *child;
  guint count = self->suggestions != NULL
                  ? MIN (g_list_model_get_n_items (self->suggestions),
                         MAX_VISIBLE_SUGGESTIONS)
                  : 0;

  while ((child = gtk_widget_get_first_child (
            GTK_WIDGET (self->suggestion_box))) != NULL)
    gtk_box_remove (self->suggestion_box, child);
  for (guint i = 0; i < count; i++) {
    g_autoptr (AwraBreadcrumbItem) item =
      g_list_model_get_item (self->suggestions, i);
    GtkWidget *button = gtk_button_new ();
    GtkWidget *label = gtk_label_new (awra_breadcrumb_item_get_label (item));

    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_button_set_child (GTK_BUTTON (button), label);
    /* The entry keeps the keyboard; the list only follows it. */
    gtk_widget_set_focusable (button, FALSE);
    gtk_widget_set_focus_on_click (button, FALSE);
    gtk_widget_add_css_class (button, "awra-menu-item");
    g_object_set_data_full (G_OBJECT (button), "awra-path-bar-item",
                            g_object_ref (item), g_object_unref);
    g_signal_connect (button, "clicked",
                      G_CALLBACK (suggestion_clicked_cb), self);
    gtk_box_append (self->suggestion_box, button);
  }
  self->highlighted = -1;
  sync_popover (self);
}

static void
suggestions_changed_cb (GListModel  *model,
                        guint        position,
                        guint        removed,
                        guint        added,
                        AwraPathBar *self)
{
  (void) model; (void) position; (void) removed; (void) added;
  rebuild_suggestions (self);
}

static AwraBreadcrumbItem *
highlighted_suggestion (AwraPathBar *self,
                        int          index)
{
  GtkWidget *child = gtk_widget_get_first_child (
    GTK_WIDGET (self->suggestion_box));

  for (int i = 0; child != NULL && i < index; i++)
    child = gtk_widget_get_next_sibling (child);
  return child != NULL && index >= 0
           ? g_object_get_data (G_OBJECT (child), "awra-path-bar-item")
           : NULL;
}

static void
entry_changed_cb (GtkEditable *editable,
                  AwraPathBar *self)
{
  (void) editable;
  gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-path-bar-invalid");
  if (self->syncing)
    return;
  self->highlighted = -1;
  apply_highlight (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDIT_TEXT]);
}

static void
entry_activate_cb (GtkText     *entry,
                   AwraPathBar *self)
{
  AwraBreadcrumbItem *item = highlighted_suggestion (self, self->highlighted);

  if (item != NULL) {
    replace_text (self, awra_breadcrumb_item_get_name (item));
    return;
  }
  g_autofree char *text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
  submit_text (self, text);
}

static gboolean
entry_key_pressed_cb (GtkEventControllerKey *controller,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
                      AwraPathBar           *self)
{
  int count = 0;

  (void) controller; (void) keycode; (void) state;
  for (GtkWidget *child = gtk_widget_get_first_child (
         GTK_WIDGET (self->suggestion_box));
       child != NULL; child = gtk_widget_get_next_sibling (child))
    count++;

  switch (keyval) {
  case GDK_KEY_Escape:
    awra_path_bar_set_editing (self, FALSE);
    return TRUE;
  case GDK_KEY_Down:
  case GDK_KEY_Up:
    if (count == 0)
      return FALSE;
    if (keyval == GDK_KEY_Down)
      self->highlighted = self->highlighted + 1 >= count ? 0 : self->highlighted + 1;
    else
      self->highlighted = self->highlighted <= 0 ? count - 1 : self->highlighted - 1;
    apply_highlight (self);
    return TRUE;
  case GDK_KEY_Tab:
    if (count == 0)
      return FALSE;
    /* Without a highlight, Tab takes the first suggestion. */
    replace_text (self, awra_breadcrumb_item_get_name (
      highlighted_suggestion (self, MAX (self->highlighted, 0))));
    return TRUE;
  default:
    return FALSE;
  }
}

static gboolean
focus_left_idle (gpointer data)
{
  AwraPathBar *self = data;
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

  self->focus_idle = 0;
  /* Switching windows must not lose a half-typed location. */
  if (self->editing &&
      !gtk_widget_has_focus (GTK_WIDGET (self->entry)) &&
      !gtk_event_controller_focus_contains_focus (
        g_object_get_data (G_OBJECT (self), "awra-path-bar-focus")) &&
      (!GTK_IS_WINDOW (root) || gtk_window_is_active (GTK_WINDOW (root))))
    awra_path_bar_set_editing (self, FALSE);
  return G_SOURCE_REMOVE;
}

static void
focus_left_cb (GtkEventControllerFocus *controller,
               AwraPathBar             *self)
{
  (void) controller;
  if (self->focus_idle == 0)
    self->focus_idle = g_idle_add (focus_left_idle, self);
}

static void
pressed_cb (GtkGestureClick *gesture,
            int              n_press,
            double           x,
            double           y,
            AwraPathBar     *self)
{
  (void) n_press;
  /* A press reaches this gesture even over a button, which only acts on
   * release: editing now would hide the breadcrumb under the pointer. */
  for (GtkWidget *picked = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_DEFAULT);
       picked != NULL && picked != GTK_WIDGET (self);
       picked = gtk_widget_get_parent (picked)) {
    if (GTK_IS_BUTTON (picked)) {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }
  }
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  if (!self->editing)
    awra_path_bar_set_editing (self, TRUE);
}

static void
activate_item_cb (AwraBreadcrumbBar  *bar,
                  AwraBreadcrumbItem *item,
                  AwraPathBar        *self)
{
  (void) bar;
  g_signal_emit (self, signals[ACTIVATE_ITEM], 0, item);
}

static void
awra_path_bar_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  gtk_widget_measure (GTK_WIDGET (AWRA_PATH_BAR (widget)->box), orientation,
                      for_size, minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
awra_path_bar_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  AwraPathBar *self = AWRA_PATH_BAR (widget);

  gtk_widget_allocate (GTK_WIDGET (self->box), width, height, baseline, NULL);
  gtk_popover_present (self->popover);
}

static void
awra_path_bar_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_PATH_BAR (widget)->box),
                             snapshot);
}

static gboolean
awra_path_bar_grab_focus (GtkWidget *widget)
{
  AwraPathBar *self = AWRA_PATH_BAR (widget);

  return self->editing ? gtk_widget_grab_focus (GTK_WIDGET (self->entry))
                       : GTK_WIDGET_CLASS (awra_path_bar_parent_class)->grab_focus (widget);
}

static void
awra_path_bar_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (awra_path_bar_parent_class)->map (widget);
  sync_transition (AWRA_PATH_BAR (widget));
}

static void
awra_path_bar_unmap (GtkWidget *widget)
{
  gtk_popover_popdown (AWRA_PATH_BAR (widget)->popover);
  GTK_WIDGET_CLASS (awra_path_bar_parent_class)->unmap (widget);
}

static void
awra_path_bar_dispose (GObject *object)
{
  AwraPathBar *self = AWRA_PATH_BAR (object);

  g_clear_handle_id (&self->focus_idle, g_source_remove);
  if (self->suggestions_handler != 0) {
    g_signal_handler_disconnect (self->suggestions, self->suggestions_handler);
    self->suggestions_handler = 0;
  }
  g_clear_object (&self->suggestions);
  if (self->popover != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->popover));
    self->popover = NULL;
  }
  if (self->box != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->box));
    self->box = NULL;
  }
  G_OBJECT_CLASS (awra_path_bar_parent_class)->dispose (object);
}

static void
awra_path_bar_finalize (GObject *object)
{
  AwraPathBar *self = AWRA_PATH_BAR (object);

  g_free (self->text);
  g_free (self->icon_name);
  G_OBJECT_CLASS (awra_path_bar_parent_class)->finalize (object);
}

static void
awra_path_bar_get_property (GObject    *object,
                            guint       id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  AwraPathBar *self = AWRA_PATH_BAR (object);

  switch (id) {
  case PROP_MODEL: g_value_set_object (value, awra_path_bar_get_model (self)); break;
  case PROP_TEXT: g_value_set_string (value, self->text); break;
  case PROP_EDIT_TEXT: g_value_set_string (value, awra_path_bar_get_edit_text (self)); break;
  case PROP_PLACEHOLDER_TEXT: g_value_set_string (value, awra_path_bar_get_placeholder_text (self)); break;
  case PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
  case PROP_END_WIDGET: g_value_set_object (value, self->end_widget); break;
  case PROP_EDITING: g_value_set_boolean (value, self->editing); break;
  case PROP_SUGGESTIONS: g_value_set_object (value, self->suggestions); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_path_bar_set_property (GObject      *object,
                            guint         id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  AwraPathBar *self = AWRA_PATH_BAR (object);

  switch (id) {
  case PROP_MODEL: awra_path_bar_set_model (self, g_value_get_object (value)); break;
  case PROP_TEXT: awra_path_bar_set_text (self, g_value_get_string (value)); break;
  case PROP_PLACEHOLDER_TEXT: awra_path_bar_set_placeholder_text (self, g_value_get_string (value)); break;
  case PROP_ICON_NAME: awra_path_bar_set_icon_name (self, g_value_get_string (value)); break;
  case PROP_END_WIDGET: awra_path_bar_set_end_widget (self, g_value_get_object (value)); break;
  case PROP_EDITING: awra_path_bar_set_editing (self, g_value_get_boolean (value)); break;
  case PROP_SUGGESTIONS: awra_path_bar_set_suggestions (self, g_value_get_object (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_path_bar_class_init (AwraPathBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS;

  object_class->dispose = awra_path_bar_dispose;
  object_class->finalize = awra_path_bar_finalize;
  object_class->get_property = awra_path_bar_get_property;
  object_class->set_property = awra_path_bar_set_property;
  widget_class->measure = awra_path_bar_measure;
  widget_class->size_allocate = awra_path_bar_size_allocate;
  widget_class->snapshot = awra_path_bar_snapshot;
  widget_class->grab_focus = awra_path_bar_grab_focus;
  widget_class->map = awra_path_bar_map;
  widget_class->unmap = awra_path_bar_unmap;

  properties[PROP_MODEL] = g_param_spec_object (
    "model", NULL, NULL, G_TYPE_LIST_MODEL, flags);
  properties[PROP_TEXT] = g_param_spec_string (
    "text", NULL, NULL, NULL, flags);
  properties[PROP_EDIT_TEXT] = g_param_spec_string (
    "edit-text", NULL, NULL, "",
    G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_PLACEHOLDER_TEXT] = g_param_spec_string (
    "placeholder-text", NULL, NULL, NULL, flags);
  properties[PROP_ICON_NAME] = g_param_spec_string (
    "icon-name", NULL, NULL, NULL, flags);
  properties[PROP_END_WIDGET] = g_param_spec_object (
    "end-widget", NULL, NULL, GTK_TYPE_WIDGET, flags);
  properties[PROP_EDITING] = g_param_spec_boolean (
    "editing", NULL, NULL, FALSE, flags);
  properties[PROP_SUGGESTIONS] = g_param_spec_object (
    "suggestions", NULL, NULL, G_TYPE_LIST_MODEL, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraPathBar::activate-item:
   * @self: the path bar
   * @item: the breadcrumb that was clicked
   *
   * Since: 1.1
   */
  signals[ACTIVATE_ITEM] = g_signal_new (
    "activate-item", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
    NULL, NULL, NULL, G_TYPE_NONE, 1, AWRA_TYPE_BREADCRUMB_ITEM);
  /**
   * AwraPathBar::submit:
   * @self: the path bar
   * @text: the typed location
   *
   * Emitted when the user commits a location with Enter or a suggestion.
   *
   * Returns: %TRUE to accept the location and leave edit mode, %FALSE to keep
   *   the text in the field, marked invalid, so that it can be corrected
   *
   * Since: 1.1
   */
  signals[SUBMIT] = g_signal_new (
    "submit", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
    g_signal_accumulator_true_handled, NULL, NULL,
    G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  gtk_widget_class_set_css_name (widget_class, "awrapathbar");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_NAVIGATION);
}

static void
awra_path_bar_init (AwraPathBar *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkEventController *keys = gtk_event_controller_key_new ();
  GtkEventController *focus = gtk_event_controller_focus_new ();
  GtkGesture *click = gtk_gesture_click_new ();

  self->highlighted = -1;
  gtk_widget_set_focusable (widget, FALSE);
  gtk_widget_set_cursor_from_name (widget, "text");

  self->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_widget_set_parent (GTK_WIDGET (self->box), widget);

  self->icon = GTK_IMAGE (gtk_image_new ());
  gtk_widget_set_visible (GTK_WIDGET (self->icon), FALSE);
  gtk_widget_add_css_class (GTK_WIDGET (self->icon), "awra-path-bar-icon");
  gtk_box_append (self->box, GTK_WIDGET (self->icon));

  self->stack = GTK_STACK (gtk_stack_new ());
  gtk_widget_set_hexpand (GTK_WIDGET (self->stack), TRUE);
  gtk_stack_set_transition_type (self->stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_box_append (self->box, GTK_WIDGET (self->stack));

  self->breadcrumbs = AWRA_BREADCRUMB_BAR (awra_breadcrumb_bar_new ());
  gtk_widget_set_cursor_from_name (GTK_WIDGET (self->breadcrumbs), "default");
  g_signal_connect (self->breadcrumbs, "activate-item",
                    G_CALLBACK (activate_item_cb), self);
  gtk_stack_add_named (self->stack, GTK_WIDGET (self->breadcrumbs), "breadcrumbs");

  self->entry = GTK_TEXT (gtk_text_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->entry), "awra-path-bar-text");
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->entry),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Location"),
                                  -1);
  g_signal_connect (self->entry, "changed", G_CALLBACK (entry_changed_cb), self);
  g_signal_connect (self->entry, "activate", G_CALLBACK (entry_activate_cb), self);
  gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
  g_signal_connect (keys, "key-pressed", G_CALLBACK (entry_key_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self->entry), keys);
  g_signal_connect (focus, "leave", G_CALLBACK (focus_left_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self->entry), focus);
  g_object_set_data (G_OBJECT (self), "awra-path-bar-focus", focus);
  gtk_stack_add_named (self->stack, GTK_WIDGET (self->entry), "entry");

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_PRIMARY);
  g_signal_connect (click, "pressed", G_CALLBACK (pressed_cb), self);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (click));

  self->suggestion_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  gtk_widget_add_css_class (GTK_WIDGET (self->suggestion_box), "awra-menu");
  self->popover = GTK_POPOVER (awra_popover_new ());
  awra_popover_set_child (AWRA_POPOVER (self->popover),
                          GTK_WIDGET (self->suggestion_box));
  gtk_popover_set_position (self->popover, GTK_POS_BOTTOM);
  /* An autohiding popover would take the keyboard from the field. */
  gtk_popover_set_autohide (self->popover, FALSE);
  gtk_widget_set_halign (GTK_WIDGET (self->popover), GTK_ALIGN_START);
  gtk_widget_set_parent (GTK_WIDGET (self->popover), widget);
}

GtkWidget *
awra_path_bar_new (void)
{
  return g_object_new (AWRA_TYPE_PATH_BAR, NULL);
}

GListModel *
awra_path_bar_get_model (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  return awra_breadcrumb_bar_get_model (self->breadcrumbs);
}

void
awra_path_bar_set_model (AwraPathBar *self,
                         GListModel  *model)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  if (awra_breadcrumb_bar_get_model (self->breadcrumbs) == model)
    return;
  awra_breadcrumb_bar_set_model (self->breadcrumbs, model);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

const char *
awra_path_bar_get_text (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  return self->text;
}

void
awra_path_bar_set_text (AwraPathBar *self,
                        const char  *text)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  if (g_set_str (&self->text, text))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

const char *
awra_path_bar_get_edit_text (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  return gtk_editable_get_text (GTK_EDITABLE (self->entry));
}

const char *
awra_path_bar_get_placeholder_text (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  return gtk_text_get_placeholder_text (self->entry);
}

void
awra_path_bar_set_placeholder_text (AwraPathBar *self,
                                    const char  *text)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  if (g_strcmp0 (gtk_text_get_placeholder_text (self->entry), text) == 0)
    return;
  gtk_text_set_placeholder_text (self->entry, text);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLACEHOLDER_TEXT]);
}

const char *
awra_path_bar_get_icon_name (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  return self->icon_name;
}

void
awra_path_bar_set_icon_name (AwraPathBar *self,
                             const char  *icon_name)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  if (!g_set_str (&self->icon_name, icon_name))
    return;
  gtk_image_set_from_icon_name (self->icon, icon_name);
  gtk_widget_set_visible (GTK_WIDGET (self->icon), icon_name != NULL);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

GtkWidget *
awra_path_bar_get_end_widget (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  return self->end_widget;
}

void
awra_path_bar_set_end_widget (AwraPathBar *self,
                              GtkWidget   *widget)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  if (self->end_widget == widget)
    return;
  if (self->end_widget != NULL)
    gtk_box_remove (self->box, self->end_widget);
  self->end_widget = widget;
  if (widget != NULL) {
    gtk_widget_set_cursor_from_name (widget, "default");
    gtk_box_append (self->box, widget);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_END_WIDGET]);
}

gboolean
awra_path_bar_get_editing (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), FALSE);
  return self->editing;
}

void
awra_path_bar_begin_edit (AwraPathBar *self,
                          const char  *text)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  gboolean started = !self->editing;

  self->editing = TRUE;
  gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-path-bar-invalid");
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-path-bar-editing");
  replace_text (self, text != NULL ? text : self->text);
  gtk_stack_set_visible_child_name (self->stack, "entry");
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
  if (text == NULL)
    gtk_editable_select_region (GTK_EDITABLE (self->entry), 0, -1);
  sync_popover (self);
  if (started)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDITING]);
}

void
awra_path_bar_set_editing (AwraPathBar *self,
                           gboolean     editing)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  editing = !!editing;
  if (self->editing == editing)
    return;
  if (editing) {
    awra_path_bar_begin_edit (self, NULL);
    return;
  }
  self->editing = FALSE;
  gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-path-bar-invalid");
  gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-path-bar-editing");
  gtk_stack_set_visible_child_name (self->stack, "breadcrumbs");
  replace_text (self, NULL);
  sync_popover (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDITING]);
}

GListModel *
awra_path_bar_get_suggestions (AwraPathBar *self)
{
  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  return self->suggestions;
}

void
awra_path_bar_set_suggestions (AwraPathBar *self,
                               GListModel  *model)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  g_return_if_fail (model == NULL ||
                    g_list_model_get_item_type (model) == AWRA_TYPE_BREADCRUMB_ITEM);
  if (self->suggestions == model)
    return;
  if (self->suggestions_handler != 0) {
    g_signal_handler_disconnect (self->suggestions, self->suggestions_handler);
    self->suggestions_handler = 0;
  }
  g_set_object (&self->suggestions, model);
  if (model != NULL)
    self->suggestions_handler = g_signal_connect (
      model, "items-changed", G_CALLBACK (suggestions_changed_cb), self);
  rebuild_suggestions (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUGGESTIONS]);
}

AwraBreadcrumbItem *
awra_path_bar_get_item_at (AwraPathBar *self,
                           double       x,
                           double       y)
{
  GtkWidget *node;

  g_return_val_if_fail (AWRA_IS_PATH_BAR (self), NULL);
  node = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_DEFAULT);
  for (; node != NULL && node != GTK_WIDGET (self); node = gtk_widget_get_parent (node)) {
    AwraBreadcrumbItem *item = g_object_get_data (G_OBJECT (node), "awra-breadcrumb-item");

    if (item != NULL)
      return item;
  }
  return NULL;
}

static void
mark_drop_item (GtkWidget          *widget,
                AwraBreadcrumbItem *item)
{
  gpointer own = g_object_get_data (G_OBJECT (widget), "awra-breadcrumb-item");

  if (own != NULL) {
    if (own == item)
      gtk_widget_add_css_class (widget, "awra-breadcrumb-drop");
    else
      gtk_widget_remove_css_class (widget, "awra-breadcrumb-drop");
  }
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    mark_drop_item (child, item);
}

void
awra_path_bar_set_drop_item (AwraPathBar        *self,
                             AwraBreadcrumbItem *item)
{
  g_return_if_fail (AWRA_IS_PATH_BAR (self));
  g_return_if_fail (item == NULL || AWRA_IS_BREADCRUMB_ITEM (item));
  mark_drop_item (GTK_WIDGET (self->breadcrumbs), item);
}
