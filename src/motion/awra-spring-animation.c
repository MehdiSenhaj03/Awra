/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-spring-animation.h>

#include <math.h>

#include "motion/awra-animation-private.h"

/* Springs that take longer than this to settle are treated as endless. */
#define MAX_SETTLE_MS 60000
/* Coarse search step when looking for the settling time, refined to 1 ms. */
#define SEARCH_STEP_MS 8
/* A damping ratio this close to 1 is solved as critical damping, where the
 * under- and over-damped forms divide by almost zero. */
#define CRITICAL_TOLERANCE 1e-4
/* The settled velocity is measured per frame at this rate. */
#define REFERENCE_FPS 60.0

struct _AwraSpringAnimation {
  AwraAnimation parent_instance;
  double value_from;
  double value_to;
  double damping_ratio;
  double mass;
  double stiffness;
  double initial_velocity;
  double epsilon;
  gboolean clamp;
  /* Velocity at the last computed value, in units per second. */
  double velocity;
  guint estimated_duration;
  gboolean duration_valid;
};

struct _AwraSpringAnimationClass {
  AwraAnimationClass parent_class;
};

enum {
  PROP_0,
  PROP_VALUE_FROM,
  PROP_VALUE_TO,
  PROP_DAMPING_RATIO,
  PROP_MASS,
  PROP_STIFFNESS,
  PROP_INITIAL_VELOCITY,
  PROP_EPSILON,
  PROP_CLAMP,
  PROP_VELOCITY,
  PROP_ESTIMATED_DURATION,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraSpringAnimation, awra_spring_animation, AWRA_TYPE_ANIMATION)

/* The displacement from the destination and its velocity, @t seconds after
 * the start, solving m·x'' + c·x' + k·x = 0 with x(0) = from - to and
 * x'(0) = initial velocity. */
static void
oscillate (AwraSpringAnimation *self,
           double               t,
           double              *displacement,
           double              *velocity)
{
  double x0 = self->value_from - self->value_to;
  double v0 = self->initial_velocity;
  double omega = sqrt (self->stiffness / self->mass);
  double zeta = self->damping_ratio;
  double x;
  double v;

  if (fabs (zeta - 1.0) < CRITICAL_TOLERANCE) {
    double b = v0 + omega * x0;
    double decay = exp (-omega * t);

    x = (x0 + b * t) * decay;
    v = (b - omega * (x0 + b * t)) * decay;
  } else if (zeta < 1.0) {
    double omega_d = omega * sqrt (1.0 - zeta * zeta);
    double a = x0;
    double b = (v0 + zeta * omega * x0) / omega_d;
    double decay = exp (-zeta * omega * t);
    double c = cos (omega_d * t);
    double s = sin (omega_d * t);

    x = decay * (a * c + b * s);
    v = decay * (-zeta * omega * (a * c + b * s) + omega_d * (b * c - a * s));
  } else {
    double spread = omega * sqrt (zeta * zeta - 1.0);
    double r1 = -zeta * omega + spread;
    double r2 = -zeta * omega - spread;
    double c1 = (v0 - r2 * x0) / (r1 - r2);
    double c2 = x0 - c1;
    double e1 = exp (r1 * t);
    double e2 = exp (r2 * t);

    x = c1 * e1 + c2 * e2;
    v = r1 * c1 * e1 + r2 * c2 * e2;
  }
  if (displacement != NULL)
    *displacement = x;
  if (velocity != NULL)
    *velocity = v;
}

static gboolean
settled_at (AwraSpringAnimation *self,
            double               ms)
{
  double x;
  double v;

  oscillate (self, ms / 1000.0, &x, &v);
  return fabs (x) < self->epsilon && fabs (v) / REFERENCE_FPS < self->epsilon;
}

static gboolean
crossed_at (AwraSpringAnimation *self,
            double               ms)
{
  double x0 = self->value_from - self->value_to;
  double x;

  oscillate (self, ms / 1000.0, &x, NULL);
  return x == 0.0 || (x > 0.0) != (x0 > 0.0);
}

/* The first millisecond at which @reached holds, or MAX_SETTLE_MS + 1. */
static guint
first_time (AwraSpringAnimation *self,
            gboolean           (*reached) (AwraSpringAnimation *, double))
{
  guint t = 0;

  while (t <= MAX_SETTLE_MS && !reached (self, t))
    t += SEARCH_STEP_MS;
  if (t > MAX_SETTLE_MS)
    return MAX_SETTLE_MS + 1;
  if (t == 0)
    return 0;
  for (guint earlier = t - SEARCH_STEP_MS + 1; earlier < t; earlier++) {
    if (reached (self, earlier))
      return earlier;
  }
  return t;
}

static guint
compute_duration (AwraSpringAnimation *self)
{
  double x0 = self->value_from - self->value_to;
  guint settle;

  if (fabs (x0) < self->epsilon &&
      fabs (self->initial_velocity) / REFERENCE_FPS < self->epsilon)
    return 0;
  if (self->clamp && x0 != 0.0) {
    guint crossing = first_time (self, crossed_at);

    if (crossing <= MAX_SETTLE_MS)
      return crossing;
  }
  if (self->damping_ratio <= 0.0)
    return AWRA_DURATION_INFINITE;
  settle = first_time (self, settled_at);
  return settle > MAX_SETTLE_MS ? AWRA_DURATION_INFINITE : settle;
}

static void
invalidate (AwraSpringAnimation *self)
{
  self->duration_valid = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ESTIMATED_DURATION]);
}

static guint
spring_estimate_duration (AwraAnimation *animation)
{
  AwraSpringAnimation *self = AWRA_SPRING_ANIMATION (animation);

  if (!self->duration_valid) {
    self->estimated_duration = compute_duration (self);
    self->duration_valid = TRUE;
  }
  return self->estimated_duration;
}

static double
spring_calculate_value (AwraAnimation *animation,
                        double         elapsed)
{
  AwraSpringAnimation *self = AWRA_SPRING_ANIMATION (animation);
  guint duration = spring_estimate_duration (animation);
  double x;

  if (elapsed >= (double) AWRA_DURATION_INFINITE ||
      (duration != AWRA_DURATION_INFINITE && elapsed >= duration && elapsed > 0)) {
    self->velocity = 0.0;
    return self->value_to;
  }
  oscillate (self, elapsed / 1000.0, &x, &self->velocity);
  return self->value_to + x;
}

static void
awra_spring_animation_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  AwraSpringAnimation *self = AWRA_SPRING_ANIMATION (object);

  switch (prop_id) {
    case PROP_VALUE_FROM:
      g_value_set_double (value, self->value_from);
      break;
    case PROP_VALUE_TO:
      g_value_set_double (value, self->value_to);
      break;
    case PROP_DAMPING_RATIO:
      g_value_set_double (value, self->damping_ratio);
      break;
    case PROP_MASS:
      g_value_set_double (value, self->mass);
      break;
    case PROP_STIFFNESS:
      g_value_set_double (value, self->stiffness);
      break;
    case PROP_INITIAL_VELOCITY:
      g_value_set_double (value, self->initial_velocity);
      break;
    case PROP_EPSILON:
      g_value_set_double (value, self->epsilon);
      break;
    case PROP_CLAMP:
      g_value_set_boolean (value, self->clamp);
      break;
    case PROP_VELOCITY:
      g_value_set_double (value, awra_spring_animation_get_velocity (self));
      break;
    case PROP_ESTIMATED_DURATION:
      g_value_set_uint (value, awra_spring_animation_get_estimated_duration (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_spring_animation_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  AwraSpringAnimation *self = AWRA_SPRING_ANIMATION (object);

  switch (prop_id) {
    case PROP_VALUE_FROM:
      awra_spring_animation_set_value_from (self, g_value_get_double (value));
      break;
    case PROP_VALUE_TO:
      awra_spring_animation_set_value_to (self, g_value_get_double (value));
      break;
    case PROP_DAMPING_RATIO:
      awra_spring_animation_set_damping_ratio (self, g_value_get_double (value));
      break;
    case PROP_MASS:
      awra_spring_animation_set_mass (self, g_value_get_double (value));
      break;
    case PROP_STIFFNESS:
      awra_spring_animation_set_stiffness (self, g_value_get_double (value));
      break;
    case PROP_INITIAL_VELOCITY:
      awra_spring_animation_set_initial_velocity (self, g_value_get_double (value));
      break;
    case PROP_EPSILON:
      awra_spring_animation_set_epsilon (self, g_value_get_double (value));
      break;
    case PROP_CLAMP:
      awra_spring_animation_set_clamp (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_spring_animation_class_init (AwraSpringAnimationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  AwraAnimationClass *animation_class = AWRA_ANIMATION_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->get_property = awra_spring_animation_get_property;
  object_class->set_property = awra_spring_animation_set_property;
  animation_class->estimate_duration = spring_estimate_duration;
  animation_class->calculate_value = spring_calculate_value;

  /**
   * AwraSpringAnimation:value-from:
   *
   * The starting value.
   *
   * Since: 1.1
   */
  properties[PROP_VALUE_FROM] =
    g_param_spec_double ("value-from", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0, flags);
  /**
   * AwraSpringAnimation:value-to:
   *
   * The destination.
   *
   * Since: 1.1
   */
  properties[PROP_VALUE_TO] =
    g_param_spec_double ("value-to", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0, flags);
  /**
   * AwraSpringAnimation:damping-ratio:
   *
   * How quickly oscillation dies out: 1 is critical damping, less overshoots.
   *
   * Since: 1.1
   */
  properties[PROP_DAMPING_RATIO] =
    g_param_spec_double ("damping-ratio", NULL, NULL, 0.0, G_MAXDOUBLE, 1.0, flags);
  /**
   * AwraSpringAnimation:mass:
   *
   * The mass on the spring.
   *
   * Since: 1.1
   */
  properties[PROP_MASS] =
    g_param_spec_double ("mass", NULL, NULL, G_MINDOUBLE, G_MAXDOUBLE, 1.0, flags);
  /**
   * AwraSpringAnimation:stiffness:
   *
   * How strongly the spring pulls towards its destination.
   *
   * Since: 1.1
   */
  properties[PROP_STIFFNESS] =
    g_param_spec_double ("stiffness", NULL, NULL, G_MINDOUBLE, G_MAXDOUBLE, 400.0, flags);
  /**
   * AwraSpringAnimation:initial-velocity:
   *
   * The velocity at the start, in units per second.
   *
   * Since: 1.1
   */
  properties[PROP_INITIAL_VELOCITY] =
    g_param_spec_double ("initial-velocity", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         flags);
  /**
   * AwraSpringAnimation:epsilon:
   *
   * The precision under which the spring is considered settled.
   *
   * Since: 1.1
   */
  properties[PROP_EPSILON] =
    g_param_spec_double ("epsilon", NULL, NULL, G_MINDOUBLE, G_MAXDOUBLE, 0.001, flags);
  /**
   * AwraSpringAnimation:clamp:
   *
   * Whether the animation ends when it first reaches its destination.
   *
   * Since: 1.1
   */
  properties[PROP_CLAMP] =
    g_param_spec_boolean ("clamp", NULL, NULL, FALSE, flags);
  /**
   * AwraSpringAnimation:velocity:
   *
   * The current velocity, in units per second. It changes on every frame
   * without notification; read it when needed.
   *
   * Since: 1.1
   */
  properties[PROP_VELOCITY] =
    g_param_spec_double ("velocity", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  /**
   * AwraSpringAnimation:estimated-duration:
   *
   * How long the spring takes to settle from its current start, in
   * milliseconds, or %AWRA_DURATION_INFINITE.
   *
   * Since: 1.1
   */
  properties[PROP_ESTIMATED_DURATION] =
    g_param_spec_uint ("estimated-duration", NULL, NULL, 0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_spring_animation_init (AwraSpringAnimation *self)
{
  self->damping_ratio = 1.0;
  self->mass = 1.0;
  self->stiffness = 400.0;
  self->epsilon = 0.001;
}

AwraAnimation *
awra_spring_animation_new (GtkWidget           *widget,
                           double               from,
                           double               to,
                           double               damping_ratio,
                           double               stiffness,
                           AwraAnimationTarget *target)
{
  AwraAnimation *animation;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (damping_ratio >= 0.0, NULL);
  g_return_val_if_fail (stiffness > 0.0, NULL);
  g_return_val_if_fail (AWRA_IS_ANIMATION_TARGET (target), NULL);
  animation = g_object_new (AWRA_TYPE_SPRING_ANIMATION,
                            "widget", widget,
                            "value-from", from,
                            "value-to", to,
                            "damping-ratio", damping_ratio,
                            "stiffness", stiffness,
                            "target", target,
                            NULL);
  g_object_unref (target);
  return animation;
}

void
awra_spring_animation_retarget (AwraSpringAnimation *self,
                                double               value_to)
{
  AwraAnimation *animation;
  AwraAnimationState state;
  double x;
  double v;

  g_return_if_fail (AWRA_IS_SPRING_ANIMATION (self));
  animation = AWRA_ANIMATION (self);
  state = awra_animation_get_state (animation);
  g_object_freeze_notify (G_OBJECT (self));
  switch (state) {
    case AWRA_ANIMATION_STATE_IDLE:
      awra_spring_animation_set_value_to (self, value_to);
      g_object_thaw_notify (G_OBJECT (self));
      awra_animation_play (animation);
      return;
    case AWRA_ANIMATION_STATE_FINISHED:
      awra_spring_animation_set_value_from (self, awra_animation_get_value (animation));
      awra_spring_animation_set_initial_velocity (self, 0.0);
      awra_spring_animation_set_value_to (self, value_to);
      g_object_thaw_notify (G_OBJECT (self));
      awra_animation_play (animation);
      return;
    case AWRA_ANIMATION_STATE_PLAYING:
    case AWRA_ANIMATION_STATE_PAUSED:
    default:
      /* Continue from the exact state on screen, measured at this frame. */
      x = spring_calculate_value (animation, awra_animation_get_elapsed (animation));
      v = self->velocity;
      awra_spring_animation_set_value_from (self, x);
      awra_spring_animation_set_initial_velocity (self, v);
      awra_spring_animation_set_value_to (self, value_to);
      awra_animation_restart_clock (animation);
      g_object_thaw_notify (G_OBJECT (self));
      return;
  }
}

#define DEFINE_DOUBLE_ACCESSORS(name, PROP) \
  double \
  awra_spring_animation_get_##name (AwraSpringAnimation *self) \
  { \
    g_return_val_if_fail (AWRA_IS_SPRING_ANIMATION (self), 0.0); \
    return self->name; \
  } \
  void \
  awra_spring_animation_set_##name (AwraSpringAnimation *self, \
                                    double               value) \
  { \
    g_return_if_fail (AWRA_IS_SPRING_ANIMATION (self)); \
    g_return_if_fail (G_PARAM_SPEC_DOUBLE (properties[PROP])->minimum <= value); \
    if (self->name == value) \
      return; \
    self->name = value; \
    g_object_freeze_notify (G_OBJECT (self)); \
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP]); \
    invalidate (self); \
    g_object_thaw_notify (G_OBJECT (self)); \
  }

DEFINE_DOUBLE_ACCESSORS (value_from, PROP_VALUE_FROM)
DEFINE_DOUBLE_ACCESSORS (value_to, PROP_VALUE_TO)
DEFINE_DOUBLE_ACCESSORS (damping_ratio, PROP_DAMPING_RATIO)
DEFINE_DOUBLE_ACCESSORS (mass, PROP_MASS)
DEFINE_DOUBLE_ACCESSORS (stiffness, PROP_STIFFNESS)
DEFINE_DOUBLE_ACCESSORS (initial_velocity, PROP_INITIAL_VELOCITY)
DEFINE_DOUBLE_ACCESSORS (epsilon, PROP_EPSILON)

gboolean
awra_spring_animation_get_clamp (AwraSpringAnimation *self)
{
  g_return_val_if_fail (AWRA_IS_SPRING_ANIMATION (self), FALSE);
  return self->clamp;
}

void
awra_spring_animation_set_clamp (AwraSpringAnimation *self,
                                 gboolean             clamp)
{
  g_return_if_fail (AWRA_IS_SPRING_ANIMATION (self));
  clamp = !!clamp;
  if (self->clamp == clamp)
    return;
  self->clamp = clamp;
  g_object_freeze_notify (G_OBJECT (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLAMP]);
  invalidate (self);
  g_object_thaw_notify (G_OBJECT (self));
}

double
awra_spring_animation_get_velocity (AwraSpringAnimation *self)
{
  g_return_val_if_fail (AWRA_IS_SPRING_ANIMATION (self), 0.0);
  if (awra_animation_get_state (AWRA_ANIMATION (self)) == AWRA_ANIMATION_STATE_IDLE)
    return self->initial_velocity;
  return self->velocity;
}

guint
awra_spring_animation_get_estimated_duration (AwraSpringAnimation *self)
{
  g_return_val_if_fail (AWRA_IS_SPRING_ANIMATION (self), 0);
  return spring_estimate_duration (AWRA_ANIMATION (self));
}
