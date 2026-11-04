/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <string.h>
#include <math.h>
#include <awra/awra-check-button.h>
#include <awra/awra-entry.h>
#include <awra/awra-search-entry.h>
#include <awra/awra-slider.h>
#include <awra/awra-spin-button.h>
#include <awra/awra-switch.h>
#include <awra/awra-toggle-button.h>

struct _AwraToggleButton {
  GtkToggleButton parent_instance;
  AwraButtonAppearance appearance;
};
enum { TOGGLE_PROP_0, TOGGLE_PROP_APPEARANCE, TOGGLE_N_PROPS };
static GParamSpec *toggle_properties[TOGGLE_N_PROPS];
G_DEFINE_FINAL_TYPE (AwraToggleButton, awra_toggle_button, GTK_TYPE_TOGGLE_BUTTON)
static const char *toggle_appearance_classes[] = {
  "awra-primary", "awra-secondary", "awra-ghost", "awra-destructive",
  "awra-toolbar-button",
};
static void update_toggle_appearance (AwraToggleButton *self) {
  for (guint i = 0; i < G_N_ELEMENTS (toggle_appearance_classes); i++)
    gtk_widget_remove_css_class (GTK_WIDGET (self), toggle_appearance_classes[i]);
  gtk_widget_add_css_class (GTK_WIDGET (self),
                            toggle_appearance_classes[self->appearance]);
}
static void toggle_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) {
  if (id == TOGGLE_PROP_APPEARANCE)
    g_value_set_enum (value, AWRA_TOGGLE_BUTTON (object)->appearance);
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
}
static void toggle_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) {
  if (id == TOGGLE_PROP_APPEARANCE)
    awra_toggle_button_set_appearance (AWRA_TOGGLE_BUTTON (object), g_value_get_enum (value));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
}
static void awra_toggle_button_class_init (AwraToggleButtonClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = toggle_get_property;
  object_class->set_property = toggle_set_property;
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "button");
  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (klass), GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON);
  toggle_properties[TOGGLE_PROP_APPEARANCE] = g_param_spec_enum (
    "appearance", NULL, NULL, AWRA_TYPE_BUTTON_APPEARANCE,
    AWRA_BUTTON_APPEARANCE_SECONDARY,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, TOGGLE_N_PROPS,
                                     toggle_properties);
}
static void awra_toggle_button_init (AwraToggleButton *self) {
  self->appearance = AWRA_BUTTON_APPEARANCE_SECONDARY;
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-button");
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-toggle-button");
  update_toggle_appearance (self);
}
GtkWidget *awra_toggle_button_new (void) { return g_object_new (AWRA_TYPE_TOGGLE_BUTTON, NULL); }
GtkWidget *awra_toggle_button_new_with_label (const char *label) {
  return g_object_new (AWRA_TYPE_TOGGLE_BUTTON, "label", label, NULL);
}
AwraButtonAppearance awra_toggle_button_get_appearance (AwraToggleButton *self) {
  g_return_val_if_fail (AWRA_IS_TOGGLE_BUTTON (self), AWRA_BUTTON_APPEARANCE_SECONDARY);
  return self->appearance;
}
void awra_toggle_button_set_appearance (AwraToggleButton *self, AwraButtonAppearance appearance) {
  g_return_if_fail (AWRA_IS_TOGGLE_BUTTON (self));
  g_return_if_fail (appearance >= AWRA_BUTTON_APPEARANCE_PRIMARY && appearance <= AWRA_BUTTON_APPEARANCE_TOOLBAR);
  if (self->appearance == appearance) return;
  self->appearance = appearance; update_toggle_appearance (self);
  g_object_notify_by_pspec (G_OBJECT (self), toggle_properties[TOGGLE_PROP_APPEARANCE]);
}

struct _AwraEntry { GtkEntry parent_instance; };
G_DEFINE_FINAL_TYPE (AwraEntry, awra_entry, GTK_TYPE_ENTRY)
static void awra_entry_class_init (AwraEntryClass *klass) {
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "entry");
  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (klass), GTK_ACCESSIBLE_ROLE_TEXT_BOX);
}
static void awra_entry_init (AwraEntry *self) {
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-entry");
}
GtkWidget *awra_entry_new (void) { return g_object_new (AWRA_TYPE_ENTRY, NULL); }

struct _AwraSlider { GtkScale parent_instance; };
G_DEFINE_FINAL_TYPE (AwraSlider, awra_slider, GTK_TYPE_SCALE)
static void awra_slider_class_init (AwraSliderClass *klass) {
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "scale");
  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (klass), GTK_ACCESSIBLE_ROLE_SLIDER);
}
/* A wheel turned over a slider the person isn't using scrolls the page
 * around it, as it would anywhere else on the page: a long settings page
 * otherwise stops scrolling, and changes a value, whenever a slider passes
 * under the pointer. A focused slider takes the wheel itself. */
static gboolean
slider_scroll_cb (GtkEventControllerScroll *controller,
                  double                    dx,
                  double                    dy,
                  AwraSlider               *self)
{
  GtkWidget *scroller;
  GtkAdjustment *adjustment;
  double page;
  double step;

  (void) dx;
  if (gtk_widget_has_focus (GTK_WIDGET (self)) || dy == 0.0)
    return FALSE;
  scroller = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_SCROLLED_WINDOW);
  if (scroller == NULL)
    return FALSE;
  adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));
  page = gtk_adjustment_get_page_size (adjustment);
  if (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment) <= page)
    return FALSE;
  /* GtkScrolledWindow's own step for a wheel notch; pixels for touchpads. */
  step = gtk_event_controller_scroll_get_unit (controller) == GDK_SCROLL_UNIT_WHEEL
           ? pow (page, 2.0 / 3.0)
           : 1.0;
  gtk_adjustment_set_value (adjustment, gtk_adjustment_get_value (adjustment) + dy * step);
  return TRUE;
}

static void awra_slider_init (AwraSlider *self) {
  GtkEventController *scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);

  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-slider");
  gtk_scale_set_draw_value (GTK_SCALE (self), FALSE);
  gtk_event_controller_set_propagation_phase (scroll, GTK_PHASE_CAPTURE);
  g_signal_connect (scroll, "scroll", G_CALLBACK (slider_scroll_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), scroll);
}
GtkWidget *awra_slider_new_with_range (GtkOrientation orientation, double minimum, double maximum, double step) {
  GtkWidget *slider;
  g_return_val_if_fail (minimum < maximum, NULL); g_return_val_if_fail (step > 0.0, NULL);
  slider = g_object_new (AWRA_TYPE_SLIDER, "orientation", orientation, NULL);
  gtk_range_set_range (GTK_RANGE (slider), minimum, maximum);
  gtk_range_set_increments (GTK_RANGE (slider), step, step * 10.0);
  gtk_range_set_value (GTK_RANGE (slider), minimum);
  return slider;
}

struct _AwraCheckButton { GtkCheckButton parent_instance; };
G_DEFINE_FINAL_TYPE (AwraCheckButton, awra_check_button, GTK_TYPE_CHECK_BUTTON)
static void awra_check_button_class_init (AwraCheckButtonClass *klass) {
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "checkbutton");
  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (klass), GTK_ACCESSIBLE_ROLE_CHECKBOX);
}
static void awra_check_button_init (AwraCheckButton *self) {
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-check");
}
GtkWidget *awra_check_button_new (void) { return g_object_new (AWRA_TYPE_CHECK_BUTTON, NULL); }
GtkWidget *awra_check_button_new_with_label (const char *label) {
  return g_object_new (AWRA_TYPE_CHECK_BUTTON, "label", label, NULL);
}
void awra_check_button_set_group (AwraCheckButton *self, AwraCheckButton *group) {
  g_return_if_fail (AWRA_IS_CHECK_BUTTON (self)); g_return_if_fail (group == NULL || AWRA_IS_CHECK_BUTTON (group));
  gtk_check_button_set_group (GTK_CHECK_BUTTON (self), group != NULL ? GTK_CHECK_BUTTON (group) : NULL);
}

struct _AwraSearchEntry { GtkWidget parent_instance; GtkSearchEntry *delegate; };
enum { SEARCH_PROP_0, SEARCH_PROP_TEXT, SEARCH_PROP_PLACEHOLDER, SEARCH_N_PROPS };
static GParamSpec *search_properties[SEARCH_N_PROPS];
G_DEFINE_FINAL_TYPE (AwraSearchEntry, awra_search_entry, GTK_TYPE_WIDGET)
static void search_notify_cb (GtkSearchEntry *delegate, GParamSpec *pspec, AwraSearchEntry *self) {
  (void) delegate;
  g_object_notify_by_pspec (G_OBJECT (self),
    g_str_equal (pspec->name, "text") ? search_properties[SEARCH_PROP_TEXT] : search_properties[SEARCH_PROP_PLACEHOLDER]);
}
static void awra_search_entry_dispose (GObject *object) {
  AwraSearchEntry *self = AWRA_SEARCH_ENTRY (object);
  if (self->delegate != NULL) { gtk_widget_unparent (GTK_WIDGET (self->delegate)); self->delegate = NULL; }
  G_OBJECT_CLASS (awra_search_entry_parent_class)->dispose (object);
}
static void awra_search_entry_snapshot (GtkWidget *widget, GtkSnapshot *snapshot) {
  gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_SEARCH_ENTRY (widget)->delegate), snapshot);
}
static void awra_search_entry_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) {
  AwraSearchEntry *self = AWRA_SEARCH_ENTRY (object);
  if (id == SEARCH_PROP_TEXT) g_value_set_string (value, awra_search_entry_get_text (self));
  else if (id == SEARCH_PROP_PLACEHOLDER) g_value_set_string (value, awra_search_entry_get_placeholder_text (self));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
}
static void awra_search_entry_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) {
  if (id == SEARCH_PROP_TEXT) awra_search_entry_set_text (AWRA_SEARCH_ENTRY (object), g_value_get_string (value));
  else if (id == SEARCH_PROP_PLACEHOLDER) awra_search_entry_set_placeholder_text (AWRA_SEARCH_ENTRY (object), g_value_get_string (value));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
}
static gboolean awra_search_entry_grab_focus (GtkWidget *widget) { return gtk_widget_grab_focus (GTK_WIDGET (AWRA_SEARCH_ENTRY (widget)->delegate)); }
static void awra_search_entry_class_init (AwraSearchEntryClass *klass) {
  GObjectClass *oc = G_OBJECT_CLASS (klass); GtkWidgetClass *wc = GTK_WIDGET_CLASS (klass);
  oc->dispose = awra_search_entry_dispose; oc->get_property = awra_search_entry_get_property; oc->set_property = awra_search_entry_set_property;
  wc->grab_focus = awra_search_entry_grab_focus; wc->snapshot = awra_search_entry_snapshot;
  search_properties[SEARCH_PROP_TEXT] = g_param_spec_string ("text", NULL, NULL, "", G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  search_properties[SEARCH_PROP_PLACEHOLDER] = g_param_spec_string ("placeholder-text", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (oc, SEARCH_N_PROPS, search_properties);
  gtk_widget_class_set_layout_manager_type (wc, GTK_TYPE_BIN_LAYOUT); gtk_widget_class_set_css_name (wc, "awrasearchentry");
}
static void awra_search_entry_init (AwraSearchEntry *self) {
  self->delegate = GTK_SEARCH_ENTRY (gtk_search_entry_new ()); gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-entry");
  gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-search-entry"); gtk_widget_set_parent (GTK_WIDGET (self->delegate), GTK_WIDGET (self));
  g_signal_connect (self->delegate, "notify::text", G_CALLBACK (search_notify_cb), self);
  g_signal_connect (self->delegate, "notify::placeholder-text", G_CALLBACK (search_notify_cb), self);
}
GtkWidget *awra_search_entry_new (void) { return g_object_new (AWRA_TYPE_SEARCH_ENTRY, NULL); }
const char *awra_search_entry_get_text (AwraSearchEntry *self) { g_return_val_if_fail (AWRA_IS_SEARCH_ENTRY (self), NULL); return gtk_editable_get_text (GTK_EDITABLE (self->delegate)); }
void awra_search_entry_set_text (AwraSearchEntry *self, const char *text) { g_return_if_fail (AWRA_IS_SEARCH_ENTRY (self)); gtk_editable_set_text (GTK_EDITABLE (self->delegate), text != NULL ? text : ""); }
const char *awra_search_entry_get_placeholder_text (AwraSearchEntry *self) { g_return_val_if_fail (AWRA_IS_SEARCH_ENTRY (self), NULL); return gtk_search_entry_get_placeholder_text (self->delegate); }
void awra_search_entry_set_placeholder_text (AwraSearchEntry *self, const char *text) { g_return_if_fail (AWRA_IS_SEARCH_ENTRY (self)); gtk_search_entry_set_placeholder_text (self->delegate, text); }
GtkSearchEntry *awra_search_entry_get_delegate (AwraSearchEntry *self) { g_return_val_if_fail (AWRA_IS_SEARCH_ENTRY (self), NULL); return self->delegate; }

struct _AwraSwitch { GtkWidget parent_instance; GtkSwitch *delegate; };
enum { SWITCH_PROP_0, SWITCH_PROP_ACTIVE, SWITCH_N_PROPS };
static GParamSpec *switch_properties[SWITCH_N_PROPS];
G_DEFINE_FINAL_TYPE (AwraSwitch, awra_switch, GTK_TYPE_WIDGET)
static void switch_notify_cb (GtkSwitch *delegate, GParamSpec *pspec, AwraSwitch *self) { (void) delegate; (void) pspec; g_object_notify_by_pspec (G_OBJECT (self), switch_properties[SWITCH_PROP_ACTIVE]); }
static void awra_switch_dispose (GObject *object) { AwraSwitch *self = AWRA_SWITCH (object); if (self->delegate != NULL) { gtk_widget_unparent (GTK_WIDGET (self->delegate)); self->delegate = NULL; } G_OBJECT_CLASS (awra_switch_parent_class)->dispose (object); }
static void awra_switch_snapshot (GtkWidget *widget, GtkSnapshot *snapshot) { gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_SWITCH (widget)->delegate), snapshot); }
static void awra_switch_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) { if (id == SWITCH_PROP_ACTIVE) g_value_set_boolean (value, awra_switch_get_active (AWRA_SWITCH (object))); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }
static void awra_switch_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) { if (id == SWITCH_PROP_ACTIVE) awra_switch_set_active (AWRA_SWITCH (object), g_value_get_boolean (value)); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }
static gboolean awra_switch_grab_focus (GtkWidget *widget) { return gtk_widget_grab_focus (GTK_WIDGET (AWRA_SWITCH (widget)->delegate)); }
static void awra_switch_class_init (AwraSwitchClass *klass) { GObjectClass *oc = G_OBJECT_CLASS (klass); GtkWidgetClass *wc = GTK_WIDGET_CLASS (klass); oc->dispose = awra_switch_dispose; oc->get_property = awra_switch_get_property; oc->set_property = awra_switch_set_property; wc->grab_focus = awra_switch_grab_focus; wc->snapshot = awra_switch_snapshot; switch_properties[SWITCH_PROP_ACTIVE] = g_param_spec_boolean ("active", NULL, NULL, FALSE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS); g_object_class_install_properties (oc, SWITCH_N_PROPS, switch_properties); gtk_widget_class_set_layout_manager_type (wc, GTK_TYPE_BIN_LAYOUT); }
static void awra_switch_init (AwraSwitch *self) { self->delegate = GTK_SWITCH (gtk_switch_new ()); gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-switch"); gtk_widget_set_halign (GTK_WIDGET (self->delegate), GTK_ALIGN_START); gtk_widget_set_valign (GTK_WIDGET (self->delegate), GTK_ALIGN_CENTER); gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER); gtk_widget_set_parent (GTK_WIDGET (self->delegate), GTK_WIDGET (self)); g_signal_connect (self->delegate, "notify::active", G_CALLBACK (switch_notify_cb), self); }
GtkWidget *awra_switch_new (void) { return g_object_new (AWRA_TYPE_SWITCH, NULL); }
gboolean awra_switch_get_active (AwraSwitch *self) { g_return_val_if_fail (AWRA_IS_SWITCH (self), FALSE); return gtk_switch_get_active (self->delegate); }
void awra_switch_set_active (AwraSwitch *self, gboolean active) { g_return_if_fail (AWRA_IS_SWITCH (self)); gtk_switch_set_active (self->delegate, active); }
GtkSwitch *awra_switch_get_delegate (AwraSwitch *self) { g_return_val_if_fail (AWRA_IS_SWITCH (self), NULL); return self->delegate; }

struct _AwraSpinButton { GtkWidget parent_instance; GtkSpinButton *delegate; };
enum { SPIN_PROP_0, SPIN_PROP_VALUE, SPIN_N_PROPS };
static GParamSpec *spin_properties[SPIN_N_PROPS];
G_DEFINE_FINAL_TYPE (AwraSpinButton, awra_spin_button, GTK_TYPE_WIDGET)
static void spin_notify_cb (GtkSpinButton *delegate, GParamSpec *pspec, AwraSpinButton *self) { (void) delegate; (void) pspec; g_object_notify_by_pspec (G_OBJECT (self), spin_properties[SPIN_PROP_VALUE]); }
static void awra_spin_button_dispose (GObject *object) { AwraSpinButton *self = AWRA_SPIN_BUTTON (object); if (self->delegate != NULL) { gtk_widget_unparent (GTK_WIDGET (self->delegate)); self->delegate = NULL; } G_OBJECT_CLASS (awra_spin_button_parent_class)->dispose (object); }
static void awra_spin_button_snapshot (GtkWidget *widget, GtkSnapshot *snapshot) { gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_SPIN_BUTTON (widget)->delegate), snapshot); }
static void awra_spin_button_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) { if (id == SPIN_PROP_VALUE) g_value_set_double (value, awra_spin_button_get_value (AWRA_SPIN_BUTTON (object))); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }
static void awra_spin_button_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) { if (id == SPIN_PROP_VALUE) awra_spin_button_set_value (AWRA_SPIN_BUTTON (object), g_value_get_double (value)); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }
static gboolean awra_spin_button_grab_focus (GtkWidget *widget) { return gtk_widget_grab_focus (GTK_WIDGET (AWRA_SPIN_BUTTON (widget)->delegate)); }
static void awra_spin_button_class_init (AwraSpinButtonClass *klass) { GObjectClass *oc = G_OBJECT_CLASS (klass); GtkWidgetClass *wc = GTK_WIDGET_CLASS (klass); oc->dispose = awra_spin_button_dispose; oc->get_property = awra_spin_button_get_property; oc->set_property = awra_spin_button_set_property; wc->grab_focus = awra_spin_button_grab_focus; wc->snapshot = awra_spin_button_snapshot; spin_properties[SPIN_PROP_VALUE] = g_param_spec_double ("value", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS); g_object_class_install_properties (oc, SPIN_N_PROPS, spin_properties); gtk_widget_class_set_layout_manager_type (wc, GTK_TYPE_BIN_LAYOUT); }
static void awra_spin_button_init (AwraSpinButton *self) { self->delegate = GTK_SPIN_BUTTON (gtk_spin_button_new_with_range (0.0, 100.0, 1.0)); gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-entry"); gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-spin-button"); gtk_widget_set_parent (GTK_WIDGET (self->delegate), GTK_WIDGET (self)); g_signal_connect (self->delegate, "notify::value", G_CALLBACK (spin_notify_cb), self); }
/* The text field fits the widest value of the range (sign and decimals
 * included) so "8765" never shows as "876". */
static void
fit_width_to_range (AwraSpinButton *self,
                    double          minimum,
                    double          maximum)
{
  g_autofree char *low = g_strdup_printf ("%.*f",
                                          gtk_spin_button_get_digits (self->delegate),
                                          minimum);
  g_autofree char *high = g_strdup_printf ("%.*f",
                                           gtk_spin_button_get_digits (self->delegate),
                                           maximum);
  int chars = (int) MAX (strlen (low), strlen (high));

  gtk_editable_set_width_chars (GTK_EDITABLE (self->delegate), MAX (chars, 2));
}

GtkWidget *
awra_spin_button_new_with_range (double minimum,
                                 double maximum,
                                 double step)
{
  AwraSpinButton *self;

  g_return_val_if_fail (minimum < maximum, NULL);
  g_return_val_if_fail (step > 0.0, NULL);
  self = g_object_new (AWRA_TYPE_SPIN_BUTTON, NULL);
  gtk_spin_button_set_range (self->delegate, minimum, maximum);
  gtk_spin_button_set_increments (self->delegate, step, step * 10.0);
  fit_width_to_range (self, minimum, maximum);
  return GTK_WIDGET (self);
}
double awra_spin_button_get_value (AwraSpinButton *self) { g_return_val_if_fail (AWRA_IS_SPIN_BUTTON (self), 0.0); return gtk_spin_button_get_value (self->delegate); }
void awra_spin_button_set_value (AwraSpinButton *self, double value) { g_return_if_fail (AWRA_IS_SPIN_BUTTON (self)); gtk_spin_button_set_value (self->delegate, value); }
GtkSpinButton *awra_spin_button_get_delegate (AwraSpinButton *self) { g_return_val_if_fail (AWRA_IS_SPIN_BUTTON (self), NULL); return self->delegate; }
