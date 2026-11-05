/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-form-row.h>
#include <awra/awra-layout.h>
#include <awra/awra-typography.h>
#include <awra/awra.h>

#include "awra-form-row-private.h"

struct _AwraFormRow {
  GtkWidget parent_instance;
  GtkBox *root;
  GtkLabel *label_widget;
  /* Holds the label: beside the control, it places the label level with
   * the control's first line (see label_offset()). */
  GtkWidget *label_slot;
  GtkBox *field;
  GtkLabel *help_widget;
  GtkLabel *error_widget;
  GtkWidget *control;
  char *label;
  char *help_text;
  char *error_message;
  gboolean required;
  gboolean compact;
  AwraValidationState validation_state;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_HELP_TEXT,
  PROP_ERROR_MESSAGE,
  PROP_CONTROL,
  PROP_REQUIRED,
  PROP_VALIDATION_STATE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraFormRow, awra_form_row, GTK_TYPE_WIDGET)

static gboolean
validation_state_is_valid (AwraValidationState state)
{
  return state >= AWRA_VALIDATION_STATE_NONE &&
         state <= AWRA_VALIDATION_STATE_ERROR;
}

static void
update_label (AwraFormRow *self)
{
  g_autofree char *visible = NULL;

  if (self->required && self->label != NULL && *self->label != '\0')
    visible = g_strdup_printf ("%s *", self->label);
  gtk_label_set_text (self->label_widget,
                      visible != NULL ? visible :
                      (self->label != NULL ? self->label : ""));
  gtk_widget_set_visible (GTK_WIDGET (self->label_widget),
                          self->label != NULL && *self->label != '\0');
}

static void
reset_validation_classes (GtkWidget *widget)
{
  gtk_widget_remove_css_class (widget, "awra-validation-success");
  gtk_widget_remove_css_class (widget, "awra-validation-warning");
  gtk_widget_remove_css_class (widget, "awra-validation-error");
}

/* The widget that takes focus: the control itself, or the one it wraps
 * (switches, spin buttons and search entries wrap a GTK widget). */
static GtkWidget *
focus_target (GtkWidget *control)
{
  if (gtk_widget_get_focusable (control))
    return control;
  for (GtkWidget *child = gtk_widget_get_first_child (control); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    GtkWidget *target = focus_target (child);

    if (target != NULL)
      return target;
  }
  return NULL;
}

static void
describe (AwraFormRow *self,
          GtkWidget   *widget)
{
  gtk_accessible_update_property (
    GTK_ACCESSIBLE (widget),
    GTK_ACCESSIBLE_PROPERTY_LABEL,
    self->label != NULL ? self->label : "",
    GTK_ACCESSIBLE_PROPERTY_REQUIRED,
    self->required,
    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
    self->validation_state == AWRA_VALIDATION_STATE_ERROR &&
    self->error_message != NULL
      ? self->error_message
      : (self->help_text != NULL ? self->help_text : ""),
    -1);
  gtk_accessible_update_state (
    GTK_ACCESSIBLE (widget),
    GTK_ACCESSIBLE_STATE_INVALID,
    self->validation_state == AWRA_VALIDATION_STATE_ERROR
      ? GTK_ACCESSIBLE_INVALID_TRUE
      : GTK_ACCESSIBLE_INVALID_FALSE,
    -1);
}

static void
update_accessibility (AwraFormRow *self)
{
  GtkWidget *target;

  if (self->control == NULL)
    return;
  target = focus_target (self->control);
  gtk_label_set_mnemonic_widget (self->label_widget,
                                 target != NULL ? target : self->control);
  describe (self, self->control);
  if (target != NULL && target != self->control)
    describe (self, target);
}

static void
update_validation (AwraFormRow *self)
{
  gboolean show_error =
    self->validation_state == AWRA_VALIDATION_STATE_ERROR &&
    self->error_message != NULL && *self->error_message != '\0';
  GtkWidget *widget = GTK_WIDGET (self);

  reset_validation_classes (widget);
  if (self->control != NULL)
    reset_validation_classes (self->control);
  switch (self->validation_state) {
  case AWRA_VALIDATION_STATE_SUCCESS:
    gtk_widget_add_css_class (widget, "awra-validation-success");
    if (self->control != NULL)
      gtk_widget_add_css_class (self->control, "awra-validation-success");
    break;
  case AWRA_VALIDATION_STATE_WARNING:
    gtk_widget_add_css_class (widget, "awra-validation-warning");
    if (self->control != NULL)
      gtk_widget_add_css_class (self->control, "awra-validation-warning");
    break;
  case AWRA_VALIDATION_STATE_ERROR:
    gtk_widget_add_css_class (widget, "awra-validation-error");
    if (self->control != NULL)
      gtk_widget_add_css_class (self->control, "awra-validation-error");
    break;
  case AWRA_VALIDATION_STATE_NONE:
  default:
    break;
  }
  gtk_widget_set_visible (GTK_WIDGET (self->error_widget), show_error);
  gtk_widget_set_visible (GTK_WIDGET (self->help_widget),
                          !show_error && self->help_text != NULL &&
                          *self->help_text != '\0');
  update_accessibility (self);
}

/* Narrower than this, or than side by side needs, the label goes above its
 * control. */
#define COMPACT_BELOW 560
/* A common minimum keeps label columns aligned across the forms of a page;
 * AwraForm widens it to its longest label. */
#define LABEL_WIDTH 224

static void
update_responsive_layout (AwraFormRow *self,
                          gboolean     compact)
{
  if (self->compact == compact)
    return;
  self->compact = compact;
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->root),
                                  compact ? GTK_ORIENTATION_VERTICAL
                                          : GTK_ORIENTATION_HORIZONTAL);
  /* Stacked, the label belongs to its control: keep them closer together
   * than consecutive rows so each pair reads as one group. */
  awra_box_set_spacing (self->root,
                        compact ? AWRA_SPACING_SM : AWRA_SPACING_LG);
  gtk_widget_set_size_request (GTK_WIDGET (self->label_widget),
                               compact ? -1 : LABEL_WIDTH, -1);
  gtk_widget_set_valign (self->label_slot, compact ? GTK_ALIGN_START : GTK_ALIGN_FILL);
  if (compact)
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-compact");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-compact");
}

/* Its height depends on its width at least through its layout. */
/* How far below the row's top the label goes, beside its control: baseline
 * to baseline when the control has one, centred on a one-line control
 * such as a switch, and level with the first line of a tall one such as a
 * text area, rather than centred on it and its help text. */
static int
label_offset (AwraFormRow *self,
              int          label_height,
              int          label_baseline)
{
  int control_height = 0;
  int control_baseline = -1;

  if (self->compact || self->control == NULL || !gtk_widget_should_layout (self->control))
    return 0;
  gtk_widget_measure (self->control, GTK_ORIENTATION_VERTICAL, -1,
                      &control_height, NULL, &control_baseline, NULL);
  if (control_height > 3 * label_height)
    return awra_spacing_resolve (GTK_WIDGET (self), AWRA_SPACING_SM) + 2;
  if (control_baseline >= 0 && label_baseline >= 0)
    return MAX (0, control_baseline - label_baseline);
  return MAX (0, (control_height - label_height) / 2);
}

static void
label_slot_measure (GtkWidget      *slot,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  gtk_widget_measure (gtk_widget_get_first_child (slot), orientation, for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
label_slot_allocate (GtkWidget *slot,
                     int        width,
                     int        height,
                     int        baseline)
{
  AwraFormRow *self = g_object_get_data (G_OBJECT (slot), "awra-form-row");
  GtkWidget *label = gtk_widget_get_first_child (slot);
  int label_height = 0;
  int label_baseline = -1;
  int offset;

  (void) baseline;
  gtk_widget_measure (label, GTK_ORIENTATION_VERTICAL, width, &label_height, NULL,
                      &label_baseline, NULL);
  offset = self != NULL ? label_offset (self, label_height, label_baseline) : 0;
  offset = CLAMP (offset, 0, MAX (0, height - label_height));
  gtk_widget_allocate (label, width, MIN (label_height, height), -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (0, offset)));
}

static GtkSizeRequestMode
label_slot_request_mode (GtkWidget *slot)
{
  return gtk_widget_get_request_mode (gtk_widget_get_first_child (slot));
}

static GtkSizeRequestMode
awra_form_row_request_mode (G_GNUC_UNUSED GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
measure_part (GtkWidget      *part,
              GtkOrientation  orientation,
              int             for_size,
              int            *minimum,
              int            *natural)
{
  *minimum = *natural = 0;
  if (gtk_widget_should_layout (part))
    gtk_widget_measure (part, orientation, for_size, minimum, natural, NULL, NULL);
}

/* The width side by side needs: the label column, then the control. */
static int
side_by_side_minimum (AwraFormRow *self)
{
  GtkWidget *label = GTK_WIDGET (self->label_widget);
  int label_min = 0, label_nat, field_min, field_nat, spacing = 0;

  if (gtk_widget_should_layout (label)) {
    measure_part (label, GTK_ORIENTATION_HORIZONTAL, -1, &label_min, &label_nat);
    label_min = MAX (label_min, LABEL_WIDTH);
    spacing = awra_spacing_resolve (GTK_WIDGET (self), AWRA_SPACING_LG);
  }
  measure_part (GTK_WIDGET (self->field), GTK_ORIENTATION_HORIZONTAL, -1, &field_min,
                &field_nat);
  return label_min + spacing + field_min;
}

static gboolean
compact_for_width (AwraFormRow *self,
                   int          width)
{
  return width < COMPACT_BELOW || width < side_by_side_minimum (self);
}

/* The layout only changes once the row is allocated: until then, it is
 * measured as the width it gets will lay it out. Otherwise a new row, side
 * by side, would widen its column for a frame, and stacked rows would get
 * the heights of side-by-side ones. */
static void
awra_form_row_measure (GtkWidget *widget, GtkOrientation orientation,
                       int for_size, int *minimum, int *natural,
                       int *minimum_baseline, int *natural_baseline)
{
  AwraFormRow *self = AWRA_FORM_ROW (widget);
  GtkWidget *label = GTK_WIDGET (self->label_widget);
  GtkWidget *field = GTK_WIDGET (self->field);
  gboolean has_label = gtk_widget_should_layout (label);
  int label_min, label_nat, field_min, field_nat, spacing;

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    /* Height follows width here: a width for a given height means nothing,
     * and wrapped text would report inconsistent sizes. */
    gtk_widget_measure (GTK_WIDGET (self->root), orientation, -1, minimum,
                        natural, minimum_baseline, natural_baseline);
    /* Too narrow for side by side, it stacks: no wider than that. */
    if (!self->compact) {
      measure_part (label, orientation, -1, &label_min, &label_nat);
      measure_part (field, orientation, -1, &field_min, &field_nat);
      *minimum = MIN (*minimum, MAX (label_min, field_min));
    }
    /* Side by side is shorter than stacked. A container looking for the
     * width that fits a height lands there, so it must be a natural width. */
    *natural = MAX (*natural, MAX (COMPACT_BELOW, side_by_side_minimum (self)));
    return;
  }
  if (for_size < 0 || compact_for_width (self, for_size) == self->compact) {
    gtk_widget_measure (GTK_WIDGET (self->root), orientation, for_size, minimum,
                        natural, minimum_baseline, natural_baseline);
    return;
  }

  if (minimum_baseline != NULL)
    *minimum_baseline = -1;
  if (natural_baseline != NULL)
    *natural_baseline = -1;
  if (!self->compact) {
    spacing = has_label ? awra_spacing_resolve (widget, AWRA_SPACING_SM) : 0;
    measure_part (field, GTK_ORIENTATION_HORIZONTAL, -1, &field_min, &field_nat);
    measure_part (field, orientation, MAX (for_size, field_min), &field_min, &field_nat);
    measure_part (label, orientation, -1, &label_min, &label_nat);
    *minimum = label_min + spacing + field_min;
    *natural = label_nat + spacing + field_nat;
  } else {
    int label_width = 0, field_width;

    spacing = has_label ? awra_spacing_resolve (widget, AWRA_SPACING_LG) : 0;
    if (has_label) {
      measure_part (label, GTK_ORIENTATION_HORIZONTAL, -1, &label_min, &label_nat);
      label_width = MAX (label_min, LABEL_WIDTH);
    }
    measure_part (field, GTK_ORIENTATION_HORIZONTAL, -1, &field_min, &field_nat);
    field_width = MAX (for_size - label_width - spacing, field_min);
    measure_part (label, orientation, label_width, &label_min, &label_nat);
    measure_part (field, orientation, field_width, &field_min, &field_nat);
    *minimum = MAX (label_min, field_min);
    *natural = MAX (label_nat, field_nat);
  }
}

static void
awra_form_row_allocate (GtkWidget *widget, int width, int height,
                        int baseline)
{
  AwraFormRow *self = AWRA_FORM_ROW (widget);

  update_responsive_layout (self, compact_for_width (self, width));
  gtk_widget_allocate (GTK_WIDGET (self->root), width, height, baseline, NULL);
}

static void
awra_form_row_dispose (GObject *object)
{
  AwraFormRow *self = AWRA_FORM_ROW (object);

  if (self->root != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->root));
    self->root = NULL;
    self->label_widget = NULL;
    self->label_slot = NULL;
    self->field = NULL;
    self->help_widget = NULL;
    self->error_widget = NULL;
    self->control = NULL;
  }
  G_OBJECT_CLASS (awra_form_row_parent_class)->dispose (object);
}

static void
awra_form_row_finalize (GObject *object)
{
  AwraFormRow *self = AWRA_FORM_ROW (object);

  g_clear_pointer (&self->label, g_free);
  g_clear_pointer (&self->help_text, g_free);
  g_clear_pointer (&self->error_message, g_free);
  G_OBJECT_CLASS (awra_form_row_parent_class)->finalize (object);
}

static void
awra_form_row_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  AwraFormRow *self = AWRA_FORM_ROW (object);

  switch (property_id) {
  case PROP_LABEL: g_value_set_string (value, self->label); break;
  case PROP_HELP_TEXT: g_value_set_string (value, self->help_text); break;
  case PROP_ERROR_MESSAGE: g_value_set_string (value, self->error_message); break;
  case PROP_CONTROL: g_value_set_object (value, self->control); break;
  case PROP_REQUIRED: g_value_set_boolean (value, self->required); break;
  case PROP_VALIDATION_STATE: g_value_set_enum (value, self->validation_state); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_form_row_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  AwraFormRow *self = AWRA_FORM_ROW (object);

  switch (property_id) {
  case PROP_LABEL: awra_form_row_set_label (self, g_value_get_string (value)); break;
  case PROP_HELP_TEXT: awra_form_row_set_help_text (self, g_value_get_string (value)); break;
  case PROP_ERROR_MESSAGE: awra_form_row_set_error_message (self, g_value_get_string (value)); break;
  case PROP_CONTROL: awra_form_row_set_control (self, g_value_get_object (value)); break;
  case PROP_REQUIRED: awra_form_row_set_required (self, g_value_get_boolean (value)); break;
  case PROP_VALIDATION_STATE: awra_form_row_set_validation_state (self, g_value_get_enum (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_form_row_class_init (AwraFormRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_form_row_dispose;
  object_class->finalize = awra_form_row_finalize;
  object_class->get_property = awra_form_row_get_property;
  object_class->set_property = awra_form_row_set_property;
  properties[PROP_LABEL] = g_param_spec_string (
    "label", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_HELP_TEXT] = g_param_spec_string (
    "help-text", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ERROR_MESSAGE] = g_param_spec_string (
    "error-message", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CONTROL] = g_param_spec_object (
    "control", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_REQUIRED] = g_param_spec_boolean (
    "required", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_VALIDATION_STATE] = g_param_spec_enum (
    "validation-state", NULL, NULL, AWRA_TYPE_VALIDATION_STATE,
    AWRA_VALIDATION_STATE_NONE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awraformrow");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_form_row_init (AwraFormRow *self)
{
  gtk_widget_set_layout_manager (
    GTK_WIDGET (self),
    gtk_custom_layout_new (awra_form_row_request_mode,
                           awra_form_row_measure,
                           awra_form_row_allocate));
  self->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->field = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->label_widget = GTK_LABEL (gtk_label_new (NULL));
  self->help_widget = GTK_LABEL (gtk_label_new (NULL));
  self->error_widget = GTK_LABEL (gtk_label_new (NULL));
  self->validation_state = AWRA_VALIDATION_STATE_NONE;
  awra_box_set_spacing (self->root, AWRA_SPACING_LG);
  awra_box_set_spacing (self->field, AWRA_SPACING_XS);
  awra_widget_set_typography (GTK_WIDGET (self->label_widget),
                              AWRA_TYPOGRAPHY_BODY);
  awra_widget_set_typography (GTK_WIDGET (self->help_widget),
                              AWRA_TYPOGRAPHY_MUTED);
  awra_widget_set_typography (GTK_WIDGET (self->error_widget),
                              AWRA_TYPOGRAPHY_MUTED);
  gtk_widget_add_css_class (GTK_WIDGET (self->error_widget),
                            "awra-form-error");
  gtk_label_set_xalign (self->label_widget, 0.0f);
  gtk_label_set_xalign (self->help_widget, 0.0f);
  gtk_label_set_xalign (self->error_widget, 0.0f);
  gtk_label_set_wrap (self->help_widget, TRUE);
  gtk_label_set_wrap (self->error_widget, TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->field), TRUE);
  /* Labels line up with the control, not with the control and its help text. */
  gtk_box_set_baseline_child (self->field, 0);
  gtk_widget_set_valign (GTK_WIDGET (self->field), GTK_ALIGN_START);
  gtk_widget_set_size_request (GTK_WIDGET (self->label_widget), LABEL_WIDTH, -1);
  self->label_slot = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_layout_manager (self->label_slot,
                                 gtk_custom_layout_new (label_slot_request_mode,
                                                        label_slot_measure,
                                                        label_slot_allocate));
  g_object_set_data (G_OBJECT (self->label_slot), "awra-form-row", self);
  gtk_widget_set_valign (self->label_slot, GTK_ALIGN_FILL);
  gtk_box_append (GTK_BOX (self->label_slot), GTK_WIDGET (self->label_widget));
  g_object_bind_property (self->label_widget, "visible", self->label_slot, "visible",
                          G_BINDING_SYNC_CREATE);
  gtk_box_append (self->root, self->label_slot);
  gtk_box_append (self->root, GTK_WIDGET (self->field));
  gtk_box_append (self->field, GTK_WIDGET (self->help_widget));
  gtk_box_append (self->field, GTK_WIDGET (self->error_widget));
  gtk_widget_set_parent (GTK_WIDGET (self->root), GTK_WIDGET (self));
  update_label (self);
  update_validation (self);
}

GtkWidget *awra_form_row_new (void) { return g_object_new (AWRA_TYPE_FORM_ROW, NULL); }
const char *awra_form_row_get_label (AwraFormRow *self) { g_return_val_if_fail (AWRA_IS_FORM_ROW (self), NULL); return self->label; }
void awra_form_row_set_label (AwraFormRow *self, const char *label) { g_return_if_fail (AWRA_IS_FORM_ROW (self)); if (g_strcmp0 (self->label, label) == 0) return; g_free (self->label); self->label = g_strdup (label); update_label (self); update_accessibility (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]); }
const char *awra_form_row_get_help_text (AwraFormRow *self) { g_return_val_if_fail (AWRA_IS_FORM_ROW (self), NULL); return self->help_text; }
void awra_form_row_set_help_text (AwraFormRow *self, const char *text) { g_return_if_fail (AWRA_IS_FORM_ROW (self)); if (g_strcmp0 (self->help_text, text) == 0) return; g_free (self->help_text); self->help_text = g_strdup (text); gtk_label_set_text (self->help_widget, text != NULL ? text : ""); update_validation (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HELP_TEXT]); }
const char *awra_form_row_get_error_message (AwraFormRow *self) { g_return_val_if_fail (AWRA_IS_FORM_ROW (self), NULL); return self->error_message; }
void awra_form_row_set_error_message (AwraFormRow *self, const char *message) { g_return_if_fail (AWRA_IS_FORM_ROW (self)); if (g_strcmp0 (self->error_message, message) == 0) return; g_free (self->error_message); self->error_message = g_strdup (message); gtk_label_set_text (self->error_widget, message != NULL ? message : ""); update_validation (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR_MESSAGE]); }
GtkWidget *awra_form_row_get_control (AwraFormRow *self) { g_return_val_if_fail (AWRA_IS_FORM_ROW (self), NULL); return self->control; }
/* Text fields and pickers fill the field column; compact controls (spin
 * buttons, switches, segmented controls, shortcut captures…) keep their natural
 * width instead of stretching across the form. An explicit hexpand wins. */
static gboolean
control_fills_width (GtkWidget *control)
{
  return GTK_IS_EDITABLE (control) && !GTK_IS_SPIN_BUTTON (control)
    ? TRUE
    : AWRA_IS_ENTRY (control) || AWRA_IS_SEARCH_ENTRY (control) ||
      AWRA_IS_PASSWORD_ENTRY (control) || AWRA_IS_TEXT_AREA (control) ||
      AWRA_IS_TAG_ENTRY (control) || AWRA_IS_DROPDOWN (control) ||
      GTK_IS_DROP_DOWN (control) || GTK_IS_TEXT_VIEW (control) ||
      GTK_IS_SCROLLED_WINDOW (control);
}

void
awra_form_row_set_control (AwraFormRow *self,
                           GtkWidget   *control)
{
  g_return_if_fail (AWRA_IS_FORM_ROW (self));
  g_return_if_fail (control == NULL || GTK_IS_WIDGET (control));
  if (self->control == control)
    return;
  if (self->control != NULL)
    gtk_box_remove (self->field, self->control);
  self->control = control;
  if (control != NULL) {
    if (!gtk_widget_get_hexpand_set (control)) {
      if (control_fills_width (control))
        gtk_widget_set_hexpand (control, TRUE);
      else
        gtk_widget_set_halign (control, GTK_ALIGN_START);
    }
    gtk_box_prepend (self->field, control);
  }
  update_validation (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTROL]);
}

GtkWidget *
awra_form_row_get_label_widget (AwraFormRow *self)
{
  g_return_val_if_fail (AWRA_IS_FORM_ROW (self), NULL);
  return GTK_WIDGET (self->label_widget);
}
gboolean awra_form_row_get_required (AwraFormRow *self) { g_return_val_if_fail (AWRA_IS_FORM_ROW (self), FALSE); return self->required; }
void awra_form_row_set_required (AwraFormRow *self, gboolean required) { g_return_if_fail (AWRA_IS_FORM_ROW (self)); required = !!required; if (self->required == required) return; self->required = required; update_label (self); update_accessibility (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REQUIRED]); }
AwraValidationState awra_form_row_get_validation_state (AwraFormRow *self) { g_return_val_if_fail (AWRA_IS_FORM_ROW (self), AWRA_VALIDATION_STATE_NONE); return self->validation_state; }
void awra_form_row_set_validation_state (AwraFormRow *self, AwraValidationState state) { g_return_if_fail (AWRA_IS_FORM_ROW (self)); g_return_if_fail (validation_state_is_valid (state)); if (self->validation_state == state) return; self->validation_state = state; update_validation (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALIDATION_STATE]); }
