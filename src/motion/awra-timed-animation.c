/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-timed-animation.h>

#include <math.h>

#include "motion/awra-animation-private.h"

struct _AwraTimedAnimation {
  AwraAnimation parent_instance;
  double value_from;
  double value_to;
  guint duration;
  AwraEasing easing;
  guint repeat_count;
  gboolean reverse;
  gboolean alternate;
};

struct _AwraTimedAnimationClass {
  AwraAnimationClass parent_class;
};

enum {
  PROP_0,
  PROP_VALUE_FROM,
  PROP_VALUE_TO,
  PROP_DURATION,
  PROP_EASING,
  PROP_REPEAT_COUNT,
  PROP_REVERSE,
  PROP_ALTERNATE,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraTimedAnimation, awra_timed_animation, AWRA_TYPE_ANIMATION)

double
awra_easing_ease (AwraEasing self,
                  double     value)
{
  double t = CLAMP (value, 0.0, 1.0);
  double inverse = 1.0 - t;

  switch (self) {
    case AWRA_EASING_LINEAR:
      return t;
    case AWRA_EASING_EASE_OUT_QUAD:
      return 1.0 - inverse * inverse;
    case AWRA_EASING_EASE_OUT_CUBIC:
      return 1.0 - inverse * inverse * inverse;
    case AWRA_EASING_EASE_IN_OUT_CUBIC:
      if (t < 0.5)
        return 4.0 * t * t * t;
      inverse = 2.0 - 2.0 * t;
      return 1.0 - inverse * inverse * inverse / 2.0;
    case AWRA_EASING_EASE_OUT_QUART:
      return 1.0 - inverse * inverse * inverse * inverse;
    case AWRA_EASING_EASE_OUT_EXPO:
      /* Normalised so the curve ends exactly at 1. */
      return (1.0 - exp2 (-10.0 * t)) / (1.0 - exp2 (-10.0));
    case AWRA_EASING_EASE_OUT_BACK: {
      const double overshoot = 1.70158;
      double shifted = t - 1.0;

      return 1.0 + (overshoot + 1.0) * shifted * shifted * shifted +
             overshoot * shifted * shifted;
    }
    case AWRA_EASING_EASE_IN_OUT_SINE:
      return (1.0 - cos (G_PI * t)) / 2.0;
    default:
      g_return_val_if_reached (t);
  }
}

static guint
timed_estimate_duration (AwraAnimation *animation)
{
  AwraTimedAnimation *self = AWRA_TIMED_ANIMATION (animation);

  if (self->duration == 0)
    return 0;
  if (self->repeat_count == 0)
    return AWRA_DURATION_INFINITE;
  return (guint) MIN ((guint64) self->duration * self->repeat_count,
                      (guint64) AWRA_DURATION_INFINITE - 1);
}

static double
timed_calculate_value (AwraAnimation *animation,
                       double         elapsed)
{
  AwraTimedAnimation *self = AWRA_TIMED_ANIMATION (animation);
  guint total = timed_estimate_duration (animation);
  guint64 iteration;
  double progress;
  gboolean backwards;

  if (elapsed >= (double) AWRA_DURATION_INFINITE ||
      (total != AWRA_DURATION_INFINITE && elapsed >= total && (elapsed > 0 || total > 0))) {
    /* The end: the last iteration, or the first one of an endless loop. */
    iteration = self->repeat_count > 0 ? self->repeat_count - 1 : 0;
    progress = 1.0;
  } else if (self->duration == 0) {
    iteration = 0;
    progress = 0.0;
  } else {
    iteration = (guint64) (elapsed / self->duration);
    progress = fmod (elapsed, self->duration) / self->duration;
  }
  backwards = self->reverse;
  if (self->alternate && iteration % 2 == 1)
    backwards = !backwards;
  if (backwards)
    progress = 1.0 - progress;
  return self->value_from +
         (self->value_to - self->value_from) * awra_easing_ease (self->easing, progress);
}

static void
awra_timed_animation_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  AwraTimedAnimation *self = AWRA_TIMED_ANIMATION (object);

  switch (prop_id) {
    case PROP_VALUE_FROM:
      g_value_set_double (value, self->value_from);
      break;
    case PROP_VALUE_TO:
      g_value_set_double (value, self->value_to);
      break;
    case PROP_DURATION:
      g_value_set_uint (value, self->duration);
      break;
    case PROP_EASING:
      g_value_set_enum (value, self->easing);
      break;
    case PROP_REPEAT_COUNT:
      g_value_set_uint (value, self->repeat_count);
      break;
    case PROP_REVERSE:
      g_value_set_boolean (value, self->reverse);
      break;
    case PROP_ALTERNATE:
      g_value_set_boolean (value, self->alternate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_timed_animation_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  AwraTimedAnimation *self = AWRA_TIMED_ANIMATION (object);

  switch (prop_id) {
    case PROP_VALUE_FROM:
      awra_timed_animation_set_value_from (self, g_value_get_double (value));
      break;
    case PROP_VALUE_TO:
      awra_timed_animation_set_value_to (self, g_value_get_double (value));
      break;
    case PROP_DURATION:
      awra_timed_animation_set_duration (self, g_value_get_uint (value));
      break;
    case PROP_EASING:
      awra_timed_animation_set_easing (self, g_value_get_enum (value));
      break;
    case PROP_REPEAT_COUNT:
      awra_timed_animation_set_repeat_count (self, g_value_get_uint (value));
      break;
    case PROP_REVERSE:
      awra_timed_animation_set_reverse (self, g_value_get_boolean (value));
      break;
    case PROP_ALTERNATE:
      awra_timed_animation_set_alternate (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_timed_animation_class_init (AwraTimedAnimationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  AwraAnimationClass *animation_class = AWRA_ANIMATION_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->get_property = awra_timed_animation_get_property;
  object_class->set_property = awra_timed_animation_set_property;
  animation_class->estimate_duration = timed_estimate_duration;
  animation_class->calculate_value = timed_calculate_value;

  /**
   * AwraTimedAnimation:value-from:
   *
   * The starting value.
   *
   * Since: 1.1
   */
  properties[PROP_VALUE_FROM] =
    g_param_spec_double ("value-from", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0, flags);
  /**
   * AwraTimedAnimation:value-to:
   *
   * The final value.
   *
   * Since: 1.1
   */
  properties[PROP_VALUE_TO] =
    g_param_spec_double ("value-to", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0, flags);
  /**
   * AwraTimedAnimation:duration:
   *
   * The duration of one iteration, in milliseconds.
   *
   * Since: 1.1
   */
  properties[PROP_DURATION] =
    g_param_spec_uint ("duration", NULL, NULL, 0, G_MAXINT, 260, flags);
  /**
   * AwraTimedAnimation:easing:
   *
   * The easing curve.
   *
   * Since: 1.1
   */
  properties[PROP_EASING] =
    g_param_spec_enum ("easing", NULL, NULL, AWRA_TYPE_EASING,
                       AWRA_EASING_EASE_OUT_CUBIC, flags);
  /**
   * AwraTimedAnimation:repeat-count:
   *
   * The number of iterations; 0 repeats forever.
   *
   * Since: 1.1
   */
  properties[PROP_REPEAT_COUNT] =
    g_param_spec_uint ("repeat-count", NULL, NULL, 0, G_MAXUINT, 1, flags);
  /**
   * AwraTimedAnimation:reverse:
   *
   * Whether the animation runs from its final value to its starting value.
   *
   * Since: 1.1
   */
  properties[PROP_REVERSE] =
    g_param_spec_boolean ("reverse", NULL, NULL, FALSE, flags);
  /**
   * AwraTimedAnimation:alternate:
   *
   * Whether every other iteration runs backwards.
   *
   * Since: 1.1
   */
  properties[PROP_ALTERNATE] =
    g_param_spec_boolean ("alternate", NULL, NULL, FALSE, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_timed_animation_init (AwraTimedAnimation *self)
{
  self->duration = 260;
  self->easing = AWRA_EASING_EASE_OUT_CUBIC;
  self->repeat_count = 1;
}

AwraAnimation *
awra_timed_animation_new (GtkWidget           *widget,
                          double               from,
                          double               to,
                          guint                duration,
                          AwraAnimationTarget *target)
{
  AwraAnimation *animation;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (AWRA_IS_ANIMATION_TARGET (target), NULL);
  animation = g_object_new (AWRA_TYPE_TIMED_ANIMATION,
                            "widget", widget,
                            "value-from", from,
                            "value-to", to,
                            "duration", duration,
                            "target", target,
                            NULL);
  g_object_unref (target);
  return animation;
}

AwraAnimation *
awra_timed_animation_new_for_preset (GtkWidget           *widget,
                                     double               from,
                                     double               to,
                                     AwraMotionPreset     preset,
                                     AwraAnimationTarget *target)
{
  g_return_val_if_fail (preset >= AWRA_MOTION_PRESET_INSTANT &&
                        preset <= AWRA_MOTION_PRESET_SLOW, NULL);
  return awra_timed_animation_new (widget, from, to,
                                   awra_motion_preset_get_nominal_duration (preset),
                                   target);
}

#define DEFINE_ACCESSORS(type, name, field, PROP) \
  type \
  awra_timed_animation_get_##name (AwraTimedAnimation *self) \
  { \
    g_return_val_if_fail (AWRA_IS_TIMED_ANIMATION (self), (type) 0); \
    return self->field; \
  } \
  void \
  awra_timed_animation_set_##name (AwraTimedAnimation *self, \
                                   type                value) \
  { \
    g_return_if_fail (AWRA_IS_TIMED_ANIMATION (self)); \
    if (self->field == value) \
      return; \
    self->field = value; \
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP]); \
  }

DEFINE_ACCESSORS (double, value_from, value_from, PROP_VALUE_FROM)
DEFINE_ACCESSORS (double, value_to, value_to, PROP_VALUE_TO)
DEFINE_ACCESSORS (guint, duration, duration, PROP_DURATION)
DEFINE_ACCESSORS (AwraEasing, easing, easing, PROP_EASING)
DEFINE_ACCESSORS (guint, repeat_count, repeat_count, PROP_REPEAT_COUNT)

gboolean
awra_timed_animation_get_reverse (AwraTimedAnimation *self)
{
  g_return_val_if_fail (AWRA_IS_TIMED_ANIMATION (self), FALSE);
  return self->reverse;
}

void
awra_timed_animation_set_reverse (AwraTimedAnimation *self,
                                  gboolean            reverse)
{
  g_return_if_fail (AWRA_IS_TIMED_ANIMATION (self));
  reverse = !!reverse;
  if (self->reverse == reverse)
    return;
  self->reverse = reverse;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REVERSE]);
}

gboolean
awra_timed_animation_get_alternate (AwraTimedAnimation *self)
{
  g_return_val_if_fail (AWRA_IS_TIMED_ANIMATION (self), FALSE);
  return self->alternate;
}

void
awra_timed_animation_set_alternate (AwraTimedAnimation *self,
                                    gboolean            alternate)
{
  g_return_if_fail (AWRA_IS_TIMED_ANIMATION (self));
  alternate = !!alternate;
  if (self->alternate == alternate)
    return;
  self->alternate = alternate;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALTERNATE]);
}
