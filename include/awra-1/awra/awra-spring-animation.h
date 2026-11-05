/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-animation.h>

G_BEGIN_DECLS

/**
 * AwraSpringAnimation:
 *
 * An animation that follows a damped spring from one value to another.
 *
 * A spring has no fixed duration: it settles when its value and velocity
 * become negligible (see #AwraSpringAnimation:epsilon). It suits motion
 * that responds to the pointer or can be interrupted. Call
 * awra_spring_animation_retarget() to send it to another destination while
 * it moves: the value and the velocity carry on from where they are, so the
 * motion never jumps or stops abruptly.
 *
 * The damping ratio decides the character of the motion: 1 reaches the
 * destination as fast as possible without overshooting, less than 1
 * overshoots and oscillates (0.7–0.85 gives a lively but calm bounce), more
 * than 1 approaches slowly.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_SPRING_ANIMATION (awra_spring_animation_get_type ())
GDK_DECLARE_INTERNAL_TYPE (AwraSpringAnimation, awra_spring_animation, AWRA, SPRING_ANIMATION, AwraAnimation)

/**
 * awra_spring_animation_new:
 * @widget: the widget whose frame clock drives the animation
 * @from: the starting value
 * @to: the destination
 * @damping_ratio: how quickly oscillation dies out; 1 is critical damping
 * @stiffness: how strongly the spring pulls towards the destination
 * @target: (transfer full): the target that receives the value
 *
 * Creates a spring with a mass of 1. A stiffness between 200 and 600 gives
 * motion in the range of Awra's semantic durations.
 *
 * Returns: (transfer full): a new spring animation
 *
 * Since: 1.1
 */
AwraAnimation *awra_spring_animation_new                    (GtkWidget           *widget,
                                                             double               from,
                                                             double               to,
                                                             double               damping_ratio,
                                                             double               stiffness,
                                                             AwraAnimationTarget *target);
/**
 * awra_spring_animation_retarget:
 * @self: a spring animation
 * @value_to: the new destination
 *
 * Sends the spring to @value_to from wherever it is, keeping its current
 * value and velocity, so an interrupted motion bends smoothly towards its
 * new destination.
 *
 * A playing spring keeps playing. A finished spring starts again from its
 * current value. An idle spring starts from #AwraSpringAnimation:value-from.
 * A paused spring takes the new destination and stays paused.
 *
 * Since: 1.1
 */
void           awra_spring_animation_retarget               (AwraSpringAnimation *self,
                                                             double               value_to);
/**
 * awra_spring_animation_get_value_from:
 * @self: a spring animation
 *
 * Returns: the starting value
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_value_from         (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_value_from:
 * @self: a spring animation
 * @value: the starting value
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_value_from         (AwraSpringAnimation *self,
                                                             double               value);
/**
 * awra_spring_animation_get_value_to:
 * @self: a spring animation
 *
 * Returns: the destination
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_value_to           (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_value_to:
 * @self: a spring animation
 * @value: the destination
 *
 * Sets the destination for the next playback. Use
 * awra_spring_animation_retarget() to change it during playback.
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_value_to           (AwraSpringAnimation *self,
                                                             double               value);
/**
 * awra_spring_animation_get_damping_ratio:
 * @self: a spring animation
 *
 * Returns: the damping ratio
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_damping_ratio      (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_damping_ratio:
 * @self: a spring animation
 * @damping_ratio: the damping ratio, 0 or more
 *
 * A ratio of 0 never settles: the animation then lasts forever.
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_damping_ratio      (AwraSpringAnimation *self,
                                                             double               damping_ratio);
/**
 * awra_spring_animation_get_mass:
 * @self: a spring animation
 *
 * Returns: the mass
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_mass               (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_mass:
 * @self: a spring animation
 * @mass: the mass, more than 0
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_mass               (AwraSpringAnimation *self,
                                                             double               mass);
/**
 * awra_spring_animation_get_stiffness:
 * @self: a spring animation
 *
 * Returns: the stiffness
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_stiffness          (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_stiffness:
 * @self: a spring animation
 * @stiffness: the stiffness, more than 0
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_stiffness          (AwraSpringAnimation *self,
                                                             double               stiffness);
/**
 * awra_spring_animation_get_initial_velocity:
 * @self: a spring animation
 *
 * Returns: the velocity at the start, in units per second
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_initial_velocity   (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_initial_velocity:
 * @self: a spring animation
 * @velocity: the velocity at the start, in units per second
 *
 * Lets a gesture hand its speed over to the spring, such as a swipe that is
 * released.
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_initial_velocity   (AwraSpringAnimation *self,
                                                             double               velocity);
/**
 * awra_spring_animation_get_epsilon:
 * @self: a spring animation
 *
 * Returns: the precision under which the spring is considered settled
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_epsilon            (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_epsilon:
 * @self: a spring animation
 * @epsilon: the precision, more than 0
 *
 * The spring is settled once it is closer than @epsilon to its destination
 * and moves slower than @epsilon per frame. The default, 0.001, suits values
 * between 0 and 1; use about 0.1 for values in pixels.
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_epsilon            (AwraSpringAnimation *self,
                                                             double               epsilon);
/**
 * awra_spring_animation_get_clamp:
 * @self: a spring animation
 *
 * Returns: whether the animation ends when it first reaches its destination
 *
 * Since: 1.1
 */
gboolean       awra_spring_animation_get_clamp              (AwraSpringAnimation *self);
/**
 * awra_spring_animation_set_clamp:
 * @self: a spring animation
 * @clamp: whether to end when the destination is first reached
 *
 * A clamped spring never overshoots, for values that must stay in range such
 * as an opacity.
 *
 * Since: 1.1
 */
void           awra_spring_animation_set_clamp              (AwraSpringAnimation *self,
                                                             gboolean             clamp);
/**
 * awra_spring_animation_get_velocity:
 * @self: a spring animation
 *
 * Returns: the current velocity, in units per second
 *
 * Since: 1.1
 */
double         awra_spring_animation_get_velocity           (AwraSpringAnimation *self);
/**
 * awra_spring_animation_get_estimated_duration:
 * @self: a spring animation
 *
 * Returns: how long the spring takes to settle from its current start, in
 *   milliseconds, or %AWRA_DURATION_INFINITE
 *
 * Since: 1.1
 */
guint          awra_spring_animation_get_estimated_duration (AwraSpringAnimation *self);

G_END_DECLS
