/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

#include <awra/awra-animation-target.h>
#include <awra/awra-enums.h>

G_BEGIN_DECLS

/**
 * AWRA_DURATION_INFINITE:
 *
 * The duration, in milliseconds, of an animation that never ends on its own,
 * such as a timed animation that repeats forever or an undamped spring.
 *
 * Since: 1.1
 */
#define AWRA_DURATION_INFINITE ((guint) 0xffffffff)

/**
 * AwraAnimation:
 *
 * A value that changes over time and drives an #AwraAnimationTarget.
 *
 * An animation follows the frame clock of its widget, so it advances once
 * per displayed frame and stops when the widget leaves the screen. It is the
 * base of #AwraTimedAnimation, which follows a duration and an easing curve,
 * and #AwraSpringAnimation, which follows a physical spring and can change
 * its destination in flight.
 *
 * An animation keeps itself alive while it plays: an application can start
 * one and drop its reference. When reduced motion is on, when the widget is
 * not mapped, or when the widget is unmapped during playback, the animation
 * jumps to its end, so the target always receives its final value and
 * #AwraAnimation::done is always emitted.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_ANIMATION (awra_animation_get_type ())
GDK_DECLARE_INTERNAL_TYPE (AwraAnimation, awra_animation, AWRA, ANIMATION, GObject)

/**
 * awra_animation_get_widget:
 * @self: an animation
 *
 * Returns: (transfer none) (nullable): the widget whose frame clock drives
 *   the animation, or %NULL once it was finalized
 *
 * Since: 1.1
 */
GtkWidget           *awra_animation_get_widget                (AwraAnimation       *self);
/**
 * awra_animation_get_target:
 * @self: an animation
 *
 * Returns: (transfer none): the target that receives the value
 *
 * Since: 1.1
 */
AwraAnimationTarget *awra_animation_get_target                (AwraAnimation       *self);
/**
 * awra_animation_set_target:
 * @self: an animation
 * @target: the target that receives the value
 *
 * Since: 1.1
 */
void                 awra_animation_set_target                (AwraAnimation       *self,
                                                               AwraAnimationTarget *target);
/**
 * awra_animation_get_value:
 * @self: an animation
 *
 * Returns: the value the target last received
 *
 * Since: 1.1
 */
double               awra_animation_get_value                 (AwraAnimation       *self);
/**
 * awra_animation_get_state:
 * @self: an animation
 *
 * Returns: where the animation is in its lifecycle
 *
 * Since: 1.1
 */
AwraAnimationState   awra_animation_get_state                 (AwraAnimation       *self);
/**
 * awra_animation_get_follow_reduced_motion:
 * @self: an animation
 *
 * Returns: whether the animation jumps to its end when reduced motion is on
 *
 * Since: 1.1
 */
gboolean             awra_animation_get_follow_reduced_motion (AwraAnimation       *self);
/**
 * awra_animation_set_follow_reduced_motion:
 * @self: an animation
 * @follow: whether to jump to the end when reduced motion is on
 *
 * Only turn this off for motion that carries meaning which cannot be shown
 * otherwise, such as a progress indicator; keep it short and calm.
 *
 * Since: 1.1
 */
void                 awra_animation_set_follow_reduced_motion (AwraAnimation       *self,
                                                               gboolean             follow);
/**
 * awra_animation_play:
 * @self: an animation
 *
 * Starts the animation from its beginning. Playing an animation that is
 * already playing, paused or finished restarts it.
 *
 * Since: 1.1
 */
void                 awra_animation_play                      (AwraAnimation       *self);
/**
 * awra_animation_pause:
 * @self: an animation
 *
 * Holds a playing animation at its current value. Does nothing otherwise.
 *
 * Since: 1.1
 */
void                 awra_animation_pause                     (AwraAnimation       *self);
/**
 * awra_animation_resume:
 * @self: an animation
 *
 * Continues a paused animation from where it was held. Does nothing
 * otherwise.
 *
 * Since: 1.1
 */
void                 awra_animation_resume                    (AwraAnimation       *self);
/**
 * awra_animation_reset:
 * @self: an animation
 *
 * Stops the animation and gives the target its starting value. The state
 * becomes %AWRA_ANIMATION_STATE_IDLE.
 *
 * Since: 1.1
 */
void                 awra_animation_reset                     (AwraAnimation       *self);
/**
 * awra_animation_skip:
 * @self: an animation
 *
 * Gives the target the final value at once and finishes the animation,
 * emitting #AwraAnimation::done.
 *
 * Since: 1.1
 */
void                 awra_animation_skip                      (AwraAnimation       *self);

G_END_DECLS
