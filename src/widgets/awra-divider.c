/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>

#include "widgets/awra-widget-style-private.h"
#include <awra/awra-divider.h>

/* Calibration, not ABI: space between a line and the label. */
#define LABEL_GAP 10

struct _AwraDivider {
  GtkWidget parent_instance;
  GtkLabel *label;
  AwraDividerLabelPosition label_position;
  AwraDividerAppearance appearance;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_LABEL_POSITION,
  PROP_APPEARANCE,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraDivider, awra_divider, GTK_TYPE_WIDGET)

static gboolean
has_label (AwraDivider *self)
{
  return gtk_widget_get_visible (GTK_WIDGET (self->label));
}

static void
awra_divider_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  AwraDivider *self = AWRA_DIVIDER (widget);
  int gaps;

  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  if (!has_label (self)) {
    *minimum = *natural = orientation == GTK_ORIENTATION_VERTICAL ? 1 : 0;
    return;
  }
  gtk_widget_measure (GTK_WIDGET (self->label), orientation, -1, minimum, natural, NULL, NULL);
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    gaps = self->label_position == AWRA_DIVIDER_LABEL_POSITION_CENTER ? 2 * LABEL_GAP : LABEL_GAP;
    *minimum += gaps;
    *natural += gaps;
  }
}

static void
awra_divider_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  AwraDivider *self = AWRA_DIVIDER (widget);
  int label_min = 0;
  int label_width = 0;
  int label_height = 0;
  float x;
  int available;

  (void) baseline;
  if (!has_label (self))
    return;
  gtk_widget_measure (GTK_WIDGET (self->label), GTK_ORIENTATION_HORIZONTAL, -1,
                      &label_min, &label_width, NULL, NULL);
  available = width - (self->label_position == AWRA_DIVIDER_LABEL_POSITION_CENTER
                         ? 2 * LABEL_GAP : LABEL_GAP);
  label_width = CLAMP (label_width, label_min, MAX (label_min, available));
  gtk_widget_measure (GTK_WIDGET (self->label), GTK_ORIENTATION_VERTICAL, label_width,
                      NULL, &label_height, NULL, NULL);
  if (self->label_position == AWRA_DIVIDER_LABEL_POSITION_CENTER)
    x = (width - label_width) / 2.0f;
  else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x = 0;
  else
    x = width - label_width;
  gtk_widget_allocate (GTK_WIDGET (self->label), label_width, label_height, -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (
                         roundf (x), roundf ((height - label_height) / 2.0f))));
  awra_widget_present_popovers (widget);
}

static void
append_line (GtkSnapshot   *snapshot,
             const GdkRGBA *color,
             float          from,
             float          to,
             float          y)
{
  if (to - from < 1.0f)
    return;
  gtk_snapshot_append_color (snapshot, color, &GRAPHENE_RECT_INIT (from, y, to - from, 1.0f));
}

static void
awra_divider_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  AwraDivider *self = AWRA_DIVIDER (widget);
  float width = gtk_widget_get_width (widget);
  float y = floorf (gtk_widget_get_height (widget) / 2.0f);
  GdkRGBA color;
  graphene_rect_t bounds;

  /* The line takes the divider's CSS colour, so each appearance and the
   * high-contrast theme own it like any other Awra colour. */
  gtk_widget_get_color (widget, &color);
  if (!has_label (self) ||
      !gtk_widget_compute_bounds (GTK_WIDGET (self->label), widget, &bounds)) {
    append_line (snapshot, &color, 0, width, y);
    return;
  }
  y = floorf (bounds.origin.y + bounds.size.height / 2.0f);
  append_line (snapshot, &color, 0, bounds.origin.x - LABEL_GAP, y);
  append_line (snapshot, &color, bounds.origin.x + bounds.size.width + LABEL_GAP, width, y);
  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->label), snapshot);
}

static void
update_appearance (AwraDivider *self)
{
  static const char *classes[] = {
    "awra-divider-neutral", "awra-divider-accent", "awra-divider-danger",
  };

  for (guint i = 0; i < G_N_ELEMENTS (classes); i++) {
    if (i == (guint) self->appearance)
      gtk_widget_add_css_class (GTK_WIDGET (self), classes[i]);
    else
      gtk_widget_remove_css_class (GTK_WIDGET (self), classes[i]);
  }
}

static void
awra_divider_dispose (GObject *object)
{
  AwraDivider *self = AWRA_DIVIDER (object);

  if (self->label != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->label));
    self->label = NULL;
  }
  G_OBJECT_CLASS (awra_divider_parent_class)->dispose (object);
}

static void
awra_divider_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  AwraDivider *self = AWRA_DIVIDER (object);

  switch (prop_id) {
  case PROP_LABEL:
    g_value_set_string (value, awra_divider_get_label (self));
    break;
  case PROP_LABEL_POSITION:
    g_value_set_enum (value, self->label_position);
    break;
  case PROP_APPEARANCE:
    g_value_set_enum (value, self->appearance);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_divider_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  AwraDivider *self = AWRA_DIVIDER (object);

  switch (prop_id) {
  case PROP_LABEL:
    awra_divider_set_label (self, g_value_get_string (value));
    break;
  case PROP_LABEL_POSITION:
    awra_divider_set_label_position (self, g_value_get_enum (value));
    break;
  case PROP_APPEARANCE:
    awra_divider_set_appearance (self, g_value_get_enum (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_divider_class_init (AwraDividerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_divider_dispose;
  object_class->get_property = awra_divider_get_property;
  object_class->set_property = awra_divider_set_property;
  widget_class->measure = awra_divider_measure;
  widget_class->size_allocate = awra_divider_size_allocate;
  widget_class->snapshot = awra_divider_snapshot;

  /**
   * AwraDivider:label:
   *
   * The label, or %NULL for a plain line.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraDivider:label-position:
   *
   * Where the label sits.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL_POSITION] =
    g_param_spec_enum ("label-position", NULL, NULL, AWRA_TYPE_DIVIDER_LABEL_POSITION,
                       AWRA_DIVIDER_LABEL_POSITION_CENTER,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraDivider:appearance:
   *
   * How the divider looks.
   *
   * Since: 1.1
   */
  properties[PROP_APPEARANCE] =
    g_param_spec_enum ("appearance", NULL, NULL, AWRA_TYPE_DIVIDER_APPEARANCE,
                       AWRA_DIVIDER_APPEARANCE_NEUTRAL,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awradivider");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SEPARATOR);
}

static void
awra_divider_init (AwraDivider *self)
{
  self->label = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_END);
  gtk_label_set_single_line_mode (self->label, TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->label), FALSE);
  /* The divider says it; the label would only repeat it. */
  gtk_accessible_update_state (GTK_ACCESSIBLE (self->label), GTK_ACCESSIBLE_STATE_HIDDEN, TRUE, -1);
  gtk_widget_set_parent (GTK_WIDGET (self->label), GTK_WIDGET (self));
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_HORIZONTAL,
                                  -1);
  update_appearance (self);
}

GtkWidget *
awra_divider_new (const char *label)
{
  return g_object_new (AWRA_TYPE_DIVIDER, "label", label, NULL);
}

const char *
awra_divider_get_label (AwraDivider *self)
{
  g_return_val_if_fail (AWRA_IS_DIVIDER (self), NULL);
  return has_label (self) ? gtk_label_get_text (self->label) : NULL;
}

void
awra_divider_set_label (AwraDivider *self,
                        const char  *label)
{
  g_return_if_fail (AWRA_IS_DIVIDER (self));
  if (label != NULL && *label == '\0')
    label = NULL;
  if (g_strcmp0 (awra_divider_get_label (self), label) == 0)
    return;
  gtk_label_set_text (self->label, label != NULL ? label : "");
  gtk_widget_set_visible (GTK_WIDGET (self->label), label != NULL);
  if (label != NULL)
    gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL, label, -1);
  else
    gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

AwraDividerLabelPosition
awra_divider_get_label_position (AwraDivider *self)
{
  g_return_val_if_fail (AWRA_IS_DIVIDER (self), AWRA_DIVIDER_LABEL_POSITION_CENTER);
  return self->label_position;
}

void
awra_divider_set_label_position (AwraDivider              *self,
                                 AwraDividerLabelPosition  position)
{
  g_return_if_fail (AWRA_IS_DIVIDER (self));
  g_return_if_fail (position <= AWRA_DIVIDER_LABEL_POSITION_END);
  if (self->label_position == position)
    return;
  self->label_position = position;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL_POSITION]);
}

AwraDividerAppearance
awra_divider_get_appearance (AwraDivider *self)
{
  g_return_val_if_fail (AWRA_IS_DIVIDER (self), AWRA_DIVIDER_APPEARANCE_NEUTRAL);
  return self->appearance;
}

void
awra_divider_set_appearance (AwraDivider           *self,
                             AwraDividerAppearance  appearance)
{
  g_return_if_fail (AWRA_IS_DIVIDER (self));
  g_return_if_fail (appearance <= AWRA_DIVIDER_APPEARANCE_DANGER);
  if (self->appearance == appearance)
    return;
  self->appearance = appearance;
  update_appearance (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPEARANCE]);
}
