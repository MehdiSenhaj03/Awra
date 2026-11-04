/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-typing-indicator.h>

#include <awra/awra-spring-animation.h>
#include <awra/awra-timed-animation.h>
#include <awra/awra-token-set.h>
#include <awra/awra-typography.h>
#include <math.h>

#include "widgets/awra-widget-style-private.h"

#define DOTS 3
#define WAVE_DURATION 1300
/* Each dot starts its rise this share of a cycle after the previous one. */
#define STAGGER 0.15
/* Share of a cycle a dot spends rising and falling; it rests otherwise. */
#define BUMP 0.48
#define SLIDE 4.0

struct _AwraTypingIndicator {
  GtkWidget parent_instance;
  GtkWidget *label;
  gboolean active;
  double reveal;
  double phase;
  AwraAnimation *reveal_animation;
  AwraAnimation *wave_animation;
  AwraStyleWatch watch;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ACTIVE,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraTypingIndicator, awra_typing_indicator, GTK_TYPE_WIDGET)

static double
dot_diameter (AwraTypingIndicator *self)
{
  int height = 0;

  gtk_widget_measure (self->label, GTK_ORIENTATION_VERTICAL, -1, &height, NULL, NULL, NULL);
  return MAX (4.0, round (height * 0.34));
}

static double
dots_width (AwraTypingIndicator *self)
{
  double diameter = dot_diameter (self);

  return DOTS * diameter + (DOTS - 1) * round (diameter * 0.55);
}

static double
label_gap (AwraTypingIndicator *self)
{
  return awra_token_set_get_spacing (awra_widget_get_token_set (GTK_WIDGET (self)),
                                     AWRA_SPACING_SM);
}

static gboolean
label_shown (AwraTypingIndicator *self)
{
  const char *text = gtk_label_get_text (GTK_LABEL (self->label));

  return text != NULL && *text != '\0';
}

static void
set_reveal (double   value,
            gpointer user_data)
{
  AwraTypingIndicator *self = user_data;

  self->reveal = CLAMP (value, 0.0, 1.0);
  if (self->reveal <= 0.0 && !self->active && self->wave_animation != NULL)
    awra_animation_reset (self->wave_animation);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_phase (double   value,
           gpointer user_data)
{
  AwraTypingIndicator *self = user_data;

  self->phase = value;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
update_wave (AwraTypingIndicator *self)
{
  gboolean wanted = self->active && gtk_widget_get_mapped (GTK_WIDGET (self)) &&
                    !awra_widget_get_reduced_motion (GTK_WIDGET (self));

  if (!wanted) {
    /* Fading out keeps the wave going until the dots are gone. */
    if (self->wave_animation != NULL && !(self->reveal > 0.0 && !self->active &&
                                          gtk_widget_get_mapped (GTK_WIDGET (self)) &&
                                          !awra_widget_get_reduced_motion (GTK_WIDGET (self))))
      awra_animation_reset (self->wave_animation);
    return;
  }
  if (self->wave_animation == NULL) {
    self->wave_animation =
      awra_timed_animation_new (GTK_WIDGET (self), 0.0, 1.0, WAVE_DURATION,
                                awra_callback_animation_target_new (set_phase, self, NULL));
    awra_timed_animation_set_easing (AWRA_TIMED_ANIMATION (self->wave_animation),
                                     AWRA_EASING_LINEAR);
    awra_timed_animation_set_repeat_count (AWRA_TIMED_ANIMATION (self->wave_animation), 0);
  }
  if (awra_animation_get_state (self->wave_animation) != AWRA_ANIMATION_STATE_PLAYING)
    awra_animation_play (self->wave_animation);
}

static void
update_accessible (AwraTypingIndicator *self)
{
  gtk_accessible_update_state (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_STATE_HIDDEN,
                               !self->active, -1);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  gtk_label_get_text (GTK_LABEL (self->label)), -1);
}

static void
awra_typing_indicator_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural,
                               int            *minimum_baseline,
                               int            *natural_baseline)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (widget);
  int label_minimum = 0;
  int label_natural = 0;
  double dots = dots_width (self);

  (void) for_size;
  /* The same size shown or hidden: what follows never moves. */
  gtk_widget_measure (self->label, orientation, -1, &label_minimum, &label_natural,
                      minimum_baseline, natural_baseline);
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    double extra = label_shown (self) ? label_gap (self) : 0;

    *minimum = (int) ceil (dots + extra + (label_shown (self) ? label_minimum : 0));
    *natural = (int) ceil (dots + extra + (label_shown (self) ? label_natural : 0));
  } else {
    *minimum = label_minimum;
    *natural = label_natural;
  }
}

static void
awra_typing_indicator_size_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (widget);
  double start = dots_width (self) + label_gap (self);
  gboolean rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  int label_width = MAX (0, width - (int) start);

  gtk_widget_allocate (self->label, label_width, height, baseline,
                       gsk_transform_translate (NULL,
                                                &GRAPHENE_POINT_INIT (rtl ? 0 : start, 0)));
}

static void
awra_typing_indicator_snapshot (GtkWidget   *widget,
                                GtkSnapshot *snapshot)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (widget);
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);
  double diameter = dot_diameter (self);
  double spacing = round (diameter * 0.55);
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);
  gboolean rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  gboolean still = self->wave_animation == NULL ||
                   awra_animation_get_state (self->wave_animation) != AWRA_ANIMATION_STATE_PLAYING;
  double origin = rtl ? width - dots_width (self) : 0;

  if (self->reveal <= 0.0)
    return;
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, round (SLIDE * (1.0 - self->reveal))));
  gtk_snapshot_push_opacity (snapshot, self->reveal);
  for (guint i = 0; i < DOTS; i++) {
    guint order = rtl ? DOTS - 1 - i : i;
    double local = fmod (self->phase - order * STAGGER + 1.0, 1.0);
    /* A smooth bump: rest, rise, fall, rest. */
    double bump = local < BUMP ? (1.0 - cos (2.0 * G_PI * local / BUMP)) / 2.0 : 0.0;
    double lift = still ? 0.0 : bump * diameter * 0.6;
    double x = origin + i * (diameter + spacing);
    double y = (height - diameter) / 2.0 - lift + diameter * 0.15;
    GdkRGBA color = awra_rgba_with_alpha (awra_token_set_get_muted_foreground (tokens),
                                          still ? 0.75 : 0.45 + 0.55 * bump);
    GskRoundedRect dot;

    gsk_rounded_rect_init_from_rect (&dot, &GRAPHENE_RECT_INIT (x, y, diameter, diameter),
                                     diameter / 2.0);
    gtk_snapshot_push_rounded_clip (snapshot, &dot);
    gtk_snapshot_append_color (snapshot, &color, &dot.bounds);
    gtk_snapshot_pop (snapshot);
  }
  if (label_shown (self))
    gtk_widget_snapshot_child (widget, self->label, snapshot);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
}

static void
style_changed (gpointer user_data)
{
  AwraTypingIndicator *self = user_data;

  update_wave (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
awra_typing_indicator_map (GtkWidget *widget)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (widget);

  GTK_WIDGET_CLASS (awra_typing_indicator_parent_class)->map (widget);
  awra_style_watch_start (&self->watch, widget, style_changed, self);
  update_wave (self);
}

static void
awra_typing_indicator_unmap (GtkWidget *widget)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (widget);

  awra_style_watch_stop (&self->watch);
  if (self->wave_animation != NULL)
    awra_animation_reset (self->wave_animation);
  GTK_WIDGET_CLASS (awra_typing_indicator_parent_class)->unmap (widget);
}

static void
awra_typing_indicator_dispose (GObject *object)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (object);

  awra_style_watch_stop (&self->watch);
  if (self->wave_animation != NULL)
    awra_animation_reset (self->wave_animation);
  g_clear_object (&self->wave_animation);
  if (self->reveal_animation != NULL)
    awra_animation_skip (self->reveal_animation);
  g_clear_object (&self->reveal_animation);
  g_clear_pointer (&self->label, gtk_widget_unparent);
  G_OBJECT_CLASS (awra_typing_indicator_parent_class)->dispose (object);
}

static void
awra_typing_indicator_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (object);

  switch (prop_id) {
    case PROP_LABEL: g_value_set_string (value, awra_typing_indicator_get_label (self)); break;
    case PROP_ACTIVE: g_value_set_boolean (value, self->active); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_typing_indicator_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  AwraTypingIndicator *self = AWRA_TYPING_INDICATOR (object);

  switch (prop_id) {
    case PROP_LABEL: awra_typing_indicator_set_label (self, g_value_get_string (value)); break;
    case PROP_ACTIVE: awra_typing_indicator_set_active (self, g_value_get_boolean (value)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_typing_indicator_class_init (AwraTypingIndicatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->dispose = awra_typing_indicator_dispose;
  object_class->get_property = awra_typing_indicator_get_property;
  object_class->set_property = awra_typing_indicator_set_property;
  widget_class->measure = awra_typing_indicator_measure;
  widget_class->size_allocate = awra_typing_indicator_size_allocate;
  widget_class->snapshot = awra_typing_indicator_snapshot;
  widget_class->map = awra_typing_indicator_map;
  widget_class->unmap = awra_typing_indicator_unmap;

  /**
   * AwraTypingIndicator:label:
   *
   * The line beside the dots.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL] = g_param_spec_string ("label", NULL, NULL, NULL, flags);
  /**
   * AwraTypingIndicator:active:
   *
   * Whether the indicator is shown.
   *
   * Since: 1.1
   */
  properties[PROP_ACTIVE] = g_param_spec_boolean ("active", NULL, NULL, FALSE, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awratypingindicator");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_STATUS);
}

static void
awra_typing_indicator_init (AwraTypingIndicator *self)
{
  self->label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (self->label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (self->label), 0.0f);
  gtk_label_set_single_line_mode (GTK_LABEL (self->label), TRUE);
  awra_widget_set_typography (self->label, AWRA_TYPOGRAPHY_MUTED);
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));
  gtk_widget_set_can_target (GTK_WIDGET (self), FALSE);
  update_accessible (self);
}

GtkWidget *
awra_typing_indicator_new (void)
{
  return g_object_new (AWRA_TYPE_TYPING_INDICATOR, NULL);
}

const char *
awra_typing_indicator_get_label (AwraTypingIndicator *self)
{
  const char *text;

  g_return_val_if_fail (AWRA_IS_TYPING_INDICATOR (self), NULL);
  text = gtk_label_get_text (GTK_LABEL (self->label));
  return text != NULL && *text != '\0' ? text : NULL;
}

void
awra_typing_indicator_set_label (AwraTypingIndicator *self,
                                 const char          *label)
{
  g_return_if_fail (AWRA_IS_TYPING_INDICATOR (self));
  if (g_strcmp0 (awra_typing_indicator_get_label (self), label) == 0)
    return;
  gtk_label_set_text (GTK_LABEL (self->label), label != NULL ? label : "");
  update_accessible (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

gboolean
awra_typing_indicator_get_active (AwraTypingIndicator *self)
{
  g_return_val_if_fail (AWRA_IS_TYPING_INDICATOR (self), FALSE);
  return self->active;
}

void
awra_typing_indicator_set_active (AwraTypingIndicator *self,
                                  gboolean             active)
{
  g_return_if_fail (AWRA_IS_TYPING_INDICATOR (self));
  active = !!active;
  if (self->active == active)
    return;
  self->active = active;
  if (self->reveal_animation == NULL) {
    self->reveal_animation =
      awra_spring_animation_new (GTK_WIDGET (self), self->reveal, self->reveal, 1.0, 300.0,
                                 awra_callback_animation_target_new (set_reveal, self, NULL));
    awra_spring_animation_set_clamp (AWRA_SPRING_ANIMATION (self->reveal_animation), TRUE);
    awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (self->reveal_animation), 0.003);
  }
  awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (self->reveal_animation),
                                  active ? 1.0 : 0.0);
  update_wave (self);
  update_accessible (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
}
