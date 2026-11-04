/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-animation.h>

G_BEGIN_DECLS

/**
 * AwraTimedAnimation:
 *
 * An animation from one value to another over a fixed duration, following
 * an #AwraEasing curve. It can repeat, play backwards, and alternate its
 * direction on every repetition.
 *
 * Prefer awra_timed_animation_new_for_preset(), which takes its duration from
 * Awra's semantic motion timing.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_TIMED_ANIMATION (awra_timed_animation_get_type ())
GDK_DECLARE_INTERNAL_TYPE (AwraTimedAnimation, awra_timed_animation, AWRA, TIMED_ANIMATION, AwraAnimation)

/**
 * awra_easing_ease:
 * @self: an easing curve
 * @value: the progress, from 0 to 1
 *
 * Maps linear progress to progress along the curve. @value is clamped to
 * [0, 1]; curves such as %AWRA_EASING_EASE_OUT_BACK may return values
 * slightly outside that range.
 *
 * Returns: the eased progress
 *
 * Since: 1.1
 */
double         awra_easing_ease                       (AwraEasing          self,
                                                       double              value);

/**
 * awra_timed_animation_new:
 * @widget: the widget whose frame clock drives the animation
 * @from: the starting value
 * @to: the final value
 * @duration: the duration of one iteration, in milliseconds
 * @target: (transfer full): the target that receives the value
 *
 * Returns: (transfer full): a new timed animation
 *
 * Since: 1.1
 */
AwraAnimation *awra_timed_animation_new               (GtkWidget           *widget,
                                                       double               from,
                                                       double               to,
                                                       guint                duration,
                                                       AwraAnimationTarget *target);
/**
 * awra_timed_animation_new_for_preset:
 * @widget: the widget whose frame clock drives the animation
 * @from: the starting value
 * @to: the final value
 * @preset: the semantic duration
 * @target: (transfer full): the target that receives the value
 *
 * Creates an animation that lasts as long as @preset. Reduced motion is
 * honoured when the animation plays, not here.
 *
 * Returns: (transfer full): a new timed animation
 *
 * Since: 1.1
 */
AwraAnimation *awra_timed_animation_new_for_preset    (GtkWidget           *widget,
                                                       double               from,
                                                       double               to,
                                                       AwraMotionPreset     preset,
                                                       AwraAnimationTarget *target);

/**
 * awra_timed_animation_get_value_from:
 * @self: a timed animation
 *
 * Returns: the starting value
 *
 * Since: 1.1
 */
double         awra_timed_animation_get_value_from    (AwraTimedAnimation *self);
/**
 * awra_timed_animation_set_value_from:
 * @self: a timed animation
 * @value: the starting value
 *
 * Since: 1.1
 */
void           awra_timed_animation_set_value_from    (AwraTimedAnimation *self,
                                                       double              value);
/**
 * awra_timed_animation_get_value_to:
 * @self: a timed animation
 *
 * Returns: the final value
 *
 * Since: 1.1
 */
double         awra_timed_animation_get_value_to      (AwraTimedAnimation *self);
/**
 * awra_timed_animation_set_value_to:
 * @self: a timed animation
 * @value: the final value
 *
 * Since: 1.1
 */
void           awra_timed_animation_set_value_to      (AwraTimedAnimation *self,
                                                       double              value);
/**
 * awra_timed_animation_get_duration:
 * @self: a timed animation
 *
 * Returns: the duration of one iteration, in milliseconds
 *
 * Since: 1.1
 */
guint          awra_timed_animation_get_duration      (AwraTimedAnimation *self);
/**
 * awra_timed_animation_set_duration:
 * @self: a timed animation
 * @duration: the duration of one iteration, in milliseconds
 *
 * Since: 1.1
 */
void           awra_timed_animation_set_duration      (AwraTimedAnimation *self,
                                                       guint               duration);
/**
 * awra_timed_animation_get_easing:
 * @self: a timed animation
 *
 * Returns: the easing curve
 *
 * Since: 1.1
 */
AwraEasing     awra_timed_animation_get_easing        (AwraTimedAnimation *self);
/**
 * awra_timed_animation_set_easing:
 * @self: a timed animation
 * @easing: the easing curve
 *
 * The default is %AWRA_EASING_EASE_OUT_CUBIC.
 *
 * Since: 1.1
 */
void           awra_timed_animation_set_easing        (AwraTimedAnimation *self,
                                                       AwraEasing          easing);
/**
 * awra_timed_animation_get_repeat_count:
 * @self: a timed animation
 *
 * Returns: the number of iterations, 0 meaning forever
 *
 * Since: 1.1
 */
guint          awra_timed_animation_get_repeat_count  (AwraTimedAnimation *self);
/**
 * awra_timed_animation_set_repeat_count:
 * @self: a timed animation
 * @repeat_count: the number of iterations, or 0 to repeat forever
 *
 * The default is 1. An animation that repeats forever only ends when it is
 * skipped, reset, or its widget is unmapped.
 *
 * Since: 1.1
 */
void           awra_timed_animation_set_repeat_count  (AwraTimedAnimation *self,
                                                       guint               repeat_count);
/**
 * awra_timed_animation_get_reverse:
 * @self: a timed animation
 *
 * Returns: whether the animation runs from its final value to its starting
 *   value
 *
 * Since: 1.1
 */
gboolean       awra_timed_animation_get_reverse       (AwraTimedAnimation *self);
/**
 * awra_timed_animation_set_reverse:
 * @self: a timed animation
 * @reverse: whether to run from the final value to the starting value
 *
 * Since: 1.1
 */
void           awra_timed_animation_set_reverse       (AwraTimedAnimation *self,
                                                       gboolean            reverse);
/**
 * awra_timed_animation_get_alternate:
 * @self: a timed animation
 *
 * Returns: whether every other iteration runs backwards
 *
 * Since: 1.1
 */
gboolean       awra_timed_animation_get_alternate     (AwraTimedAnimation *self);
/**
 * awra_timed_animation_set_alternate:
 * @self: a timed animation
 * @alternate: whether every other iteration runs backwards
 *
 * With a repeat count, this gives a back-and-forth motion such as a pulse.
 *
 * Since: 1.1
 */
void           awra_timed_animation_set_alternate     (AwraTimedAnimation *self,
                                                       gboolean            alternate);

G_END_DECLS
