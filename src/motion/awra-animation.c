/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-animation.h>

#include <awra/awra-context.h>

#include "motion/awra-animation-private.h"

typedef struct {
  GtkWidget *widget;
  AwraAnimationTarget *target;
  double value;
  AwraAnimationState state;
  gboolean follow_reduced_motion;
  /* Frame time, in microseconds, at which the clock reads 0. */
  gint64 start_time;
  /* Where a paused animation was held, in milliseconds. */
  double paused_elapsed;
  guint tick_id;
  gulong unmap_id;
} AwraAnimationPrivate;

enum {
  PROP_0,
  PROP_WIDGET,
  PROP_TARGET,
  PROP_VALUE,
  PROP_STATE,
  PROP_FOLLOW_REDUCED_MOTION,
  N_PROPS,
};

enum {
  SIGNAL_DONE,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (AwraAnimation, awra_animation, G_TYPE_OBJECT)

static guint
estimate_duration (AwraAnimation *self)
{
  return AWRA_ANIMATION_GET_CLASS (self)->estimate_duration (self);
}

static double
calculate_value (AwraAnimation *self,
                 double         elapsed)
{
  return AWRA_ANIMATION_GET_CLASS (self)->calculate_value (self, elapsed);
}

static void
set_value (AwraAnimation *self,
           double         value)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  priv->value = value;
  if (priv->target != NULL)
    awra_animation_target_set_value (priv->target, value);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}

static void
set_state (AwraAnimation      *self,
           AwraAnimationState  state)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  if (priv->state == state)
    return;
  priv->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

static gint64
frame_time (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);
  GdkFrameClock *clock = NULL;

  if (priv->widget != NULL)
    clock = gtk_widget_get_frame_clock (priv->widget);
  if (clock != NULL)
    return gdk_frame_clock_get_frame_time (clock);
  return g_get_monotonic_time ();
}

/* Detaches from the frame clock. Releases the reference the animation holds
 * on itself while it plays: callers keep their own while they need @self. */
static void
stop_ticking (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);
  gboolean was_ticking = priv->tick_id != 0;

  if (priv->tick_id != 0 && priv->widget != NULL)
    gtk_widget_remove_tick_callback (priv->widget, priv->tick_id);
  priv->tick_id = 0;
  g_clear_signal_handler (&priv->unmap_id, priv->widget);
  if (was_ticking)
    g_object_unref (self);
}

static void
finish (AwraAnimation *self)
{
  g_object_ref (self);
  stop_ticking (self);
  set_value (self, calculate_value (self, (double) AWRA_DURATION_INFINITE));
  set_state (self, AWRA_ANIMATION_STATE_FINISHED);
  g_signal_emit (self, signals[SIGNAL_DONE], 0);
  g_object_unref (self);
}

static gboolean
animation_tick (GtkWidget     *widget,
                GdkFrameClock *clock,
                gpointer       user_data)
{
  AwraAnimation *self = AWRA_ANIMATION (user_data);
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);
  double elapsed = (gdk_frame_clock_get_frame_time (clock) - priv->start_time) / 1000.0;
  guint duration = estimate_duration (self);

  (void) widget;
  if (duration != AWRA_DURATION_INFINITE && elapsed >= duration) {
    /* finish() removes this callback; keep GTK from removing it twice. */
    finish (self);
    return G_SOURCE_CONTINUE;
  }
  set_value (self, calculate_value (self, MAX (elapsed, 0.0)));
  return G_SOURCE_CONTINUE;
}

static void
widget_unmapped_cb (AwraAnimation *self)
{
  awra_animation_skip (self);
}

static gboolean
can_animate (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  if (priv->widget == NULL || !gtk_widget_get_mapped (priv->widget))
    return FALSE;
  if (priv->follow_reduced_motion &&
      awra_motion_is_reduced_for_widget (priv->widget))
    return FALSE;
  return estimate_duration (self) > 0;
}

static void
start_ticking (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  g_object_ref (self);
  priv->tick_id = gtk_widget_add_tick_callback (priv->widget, animation_tick,
                                                self, NULL);
  priv->unmap_id = g_signal_connect_swapped (priv->widget, "unmap",
                                             G_CALLBACK (widget_unmapped_cb),
                                             self);
}

static void
widget_finalized_cb (gpointer  data,
                     GObject  *where_the_object_was)
{
  AwraAnimation *self = AWRA_ANIMATION (data);
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);
  gboolean was_ticking = priv->tick_id != 0;

  (void) where_the_object_was;
  /* GTK drops the tick callback and the handler with the widget. A mapped
   * widget is unmapped before it goes, so this is only a safety net. */
  priv->widget = NULL;
  priv->tick_id = 0;
  priv->unmap_id = 0;
  if (was_ticking) {
    priv->state = AWRA_ANIMATION_STATE_FINISHED;
    g_object_unref (self);
  }
}

static void
set_widget (AwraAnimation *self,
            GtkWidget     *widget)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  priv->widget = widget;
  if (widget != NULL)
    g_object_weak_ref (G_OBJECT (widget), widget_finalized_cb, self);
}

static void
awra_animation_dispose (GObject *object)
{
  AwraAnimation *self = AWRA_ANIMATION (object);
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  /* A playing animation holds a reference on itself: it is never disposed
   * while ticking, unless g_object_run_dispose() forces it. */
  if (priv->tick_id != 0 && priv->widget != NULL)
    gtk_widget_remove_tick_callback (priv->widget, priv->tick_id);
  priv->tick_id = 0;
  g_clear_signal_handler (&priv->unmap_id, priv->widget);
  if (priv->widget != NULL) {
    g_object_weak_unref (G_OBJECT (priv->widget), widget_finalized_cb, self);
    priv->widget = NULL;
  }
  g_clear_object (&priv->target);
  G_OBJECT_CLASS (awra_animation_parent_class)->dispose (object);
}

static void
awra_animation_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AwraAnimation *self = AWRA_ANIMATION (object);
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  switch (prop_id) {
    case PROP_WIDGET:
      g_value_set_object (value, priv->widget);
      break;
    case PROP_TARGET:
      g_value_set_object (value, priv->target);
      break;
    case PROP_VALUE:
      g_value_set_double (value, priv->value);
      break;
    case PROP_STATE:
      g_value_set_enum (value, priv->state);
      break;
    case PROP_FOLLOW_REDUCED_MOTION:
      g_value_set_boolean (value, priv->follow_reduced_motion);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_animation_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  AwraAnimation *self = AWRA_ANIMATION (object);

  switch (prop_id) {
    case PROP_WIDGET:
      set_widget (self, g_value_get_object (value));
      break;
    case PROP_TARGET:
      if (g_value_get_object (value) != NULL)
        awra_animation_set_target (self, g_value_get_object (value));
      break;
    case PROP_FOLLOW_REDUCED_MOTION:
      awra_animation_set_follow_reduced_motion (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_animation_class_init (AwraAnimationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = awra_animation_dispose;
  object_class->get_property = awra_animation_get_property;
  object_class->set_property = awra_animation_set_property;

  /**
   * AwraAnimation:widget:
   *
   * The widget whose frame clock drives the animation. The animation holds
   * it weakly.
   *
   * Since: 1.1
   */
  properties[PROP_WIDGET] =
    g_param_spec_object ("widget", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraAnimation:target:
   *
   * The target that receives the value on every frame.
   *
   * Since: 1.1
   */
  properties[PROP_TARGET] =
    g_param_spec_object ("target", NULL, NULL, AWRA_TYPE_ANIMATION_TARGET,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);
  /**
   * AwraAnimation:value:
   *
   * The value the target last received.
   *
   * Since: 1.1
   */
  properties[PROP_VALUE] =
    g_param_spec_double ("value", NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraAnimation:state:
   *
   * Where the animation is in its lifecycle.
   *
   * Since: 1.1
   */
  properties[PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL, AWRA_TYPE_ANIMATION_STATE,
                       AWRA_ANIMATION_STATE_IDLE,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraAnimation:follow-reduced-motion:
   *
   * Whether the animation jumps to its end when reduced motion is on.
   *
   * Since: 1.1
   */
  properties[PROP_FOLLOW_REDUCED_MOTION] =
    g_param_spec_boolean ("follow-reduced-motion", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraAnimation::done:
   * @self: the animation
   *
   * Emitted when the animation reaches its end, is skipped, or jumps to its
   * end because reduced motion is on or its widget left the screen.
   *
   * Since: 1.1
   */
  signals[SIGNAL_DONE] =
    g_signal_new ("done", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
awra_animation_init (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  priv->follow_reduced_motion = TRUE;
  priv->state = AWRA_ANIMATION_STATE_IDLE;
}

GtkWidget *
awra_animation_get_widget (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_val_if_fail (AWRA_IS_ANIMATION (self), NULL);
  priv = awra_animation_get_instance_private (self);
  return priv->widget;
}

AwraAnimationTarget *
awra_animation_get_target (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_val_if_fail (AWRA_IS_ANIMATION (self), NULL);
  priv = awra_animation_get_instance_private (self);
  return priv->target;
}

void
awra_animation_set_target (AwraAnimation       *self,
                           AwraAnimationTarget *target)
{
  AwraAnimationPrivate *priv;

  g_return_if_fail (AWRA_IS_ANIMATION (self));
  g_return_if_fail (AWRA_IS_ANIMATION_TARGET (target));
  priv = awra_animation_get_instance_private (self);
  if (g_set_object (&priv->target, target))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TARGET]);
}

double
awra_animation_get_value (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_val_if_fail (AWRA_IS_ANIMATION (self), 0.0);
  priv = awra_animation_get_instance_private (self);
  return priv->value;
}

AwraAnimationState
awra_animation_get_state (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_val_if_fail (AWRA_IS_ANIMATION (self), AWRA_ANIMATION_STATE_IDLE);
  priv = awra_animation_get_instance_private (self);
  return priv->state;
}

gboolean
awra_animation_get_follow_reduced_motion (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_val_if_fail (AWRA_IS_ANIMATION (self), TRUE);
  priv = awra_animation_get_instance_private (self);
  return priv->follow_reduced_motion;
}

void
awra_animation_set_follow_reduced_motion (AwraAnimation *self,
                                          gboolean       follow)
{
  AwraAnimationPrivate *priv;

  g_return_if_fail (AWRA_IS_ANIMATION (self));
  priv = awra_animation_get_instance_private (self);
  follow = !!follow;
  if (priv->follow_reduced_motion == follow)
    return;
  priv->follow_reduced_motion = follow;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOLLOW_REDUCED_MOTION]);
}

void
awra_animation_play (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_if_fail (AWRA_IS_ANIMATION (self));
  priv = awra_animation_get_instance_private (self);
  g_object_ref (self);
  stop_ticking (self);
  priv->paused_elapsed = 0.0;
  if (!can_animate (self)) {
    finish (self);
    g_object_unref (self);
    return;
  }
  priv->start_time = frame_time (self);
  start_ticking (self);
  set_value (self, calculate_value (self, 0.0));
  set_state (self, AWRA_ANIMATION_STATE_PLAYING);
  g_object_unref (self);
}

void
awra_animation_pause (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_if_fail (AWRA_IS_ANIMATION (self));
  priv = awra_animation_get_instance_private (self);
  if (priv->state != AWRA_ANIMATION_STATE_PLAYING)
    return;
  g_object_ref (self);
  priv->paused_elapsed = awra_animation_get_elapsed (self);
  stop_ticking (self);
  set_state (self, AWRA_ANIMATION_STATE_PAUSED);
  g_object_unref (self);
}

void
awra_animation_resume (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_if_fail (AWRA_IS_ANIMATION (self));
  priv = awra_animation_get_instance_private (self);
  if (priv->state != AWRA_ANIMATION_STATE_PAUSED)
    return;
  if (!can_animate (self)) {
    finish (self);
    return;
  }
  priv->start_time = frame_time (self) - (gint64) (priv->paused_elapsed * 1000.0);
  start_ticking (self);
  set_state (self, AWRA_ANIMATION_STATE_PLAYING);
}

void
awra_animation_reset (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_if_fail (AWRA_IS_ANIMATION (self));
  priv = awra_animation_get_instance_private (self);
  g_object_ref (self);
  stop_ticking (self);
  priv->paused_elapsed = 0.0;
  set_value (self, calculate_value (self, 0.0));
  set_state (self, AWRA_ANIMATION_STATE_IDLE);
  g_object_unref (self);
}

void
awra_animation_skip (AwraAnimation *self)
{
  AwraAnimationPrivate *priv;

  g_return_if_fail (AWRA_IS_ANIMATION (self));
  priv = awra_animation_get_instance_private (self);
  if (priv->state == AWRA_ANIMATION_STATE_FINISHED)
    return;
  finish (self);
}

double
awra_animation_get_elapsed (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  switch (priv->state) {
    case AWRA_ANIMATION_STATE_PLAYING:
      return MAX ((frame_time (self) - priv->start_time) / 1000.0, 0.0);
    case AWRA_ANIMATION_STATE_PAUSED:
      return priv->paused_elapsed;
    case AWRA_ANIMATION_STATE_FINISHED:
      return (double) AWRA_DURATION_INFINITE;
    case AWRA_ANIMATION_STATE_IDLE:
    default:
      return 0.0;
  }
}

void
awra_animation_restart_clock (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  priv->paused_elapsed = 0.0;
  if (priv->state == AWRA_ANIMATION_STATE_PLAYING)
    priv->start_time = frame_time (self);
}

void
awra_animation_refresh_value (AwraAnimation *self)
{
  AwraAnimationPrivate *priv = awra_animation_get_instance_private (self);

  if (priv->state == AWRA_ANIMATION_STATE_PLAYING ||
      priv->state == AWRA_ANIMATION_STATE_PAUSED)
    set_value (self, calculate_value (self, awra_animation_get_elapsed (self)));
}
