/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * AwraAnimationTarget:
 *
 * What an #AwraAnimation drives with its value on every frame.
 *
 * Use #AwraCallbackAnimationTarget to receive the value in a function, or
 * #AwraPropertyAnimationTarget to write it to a numeric property.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_ANIMATION_TARGET (awra_animation_target_get_type ())
GDK_DECLARE_INTERNAL_TYPE (AwraAnimationTarget, awra_animation_target, AWRA, ANIMATION_TARGET, GObject)

/**
 * AwraAnimationTargetFunc:
 * @value: the animation's value for this frame
 * @user_data: the data passed to awra_callback_animation_target_new()
 *
 * Receives an animation's value on every frame.
 *
 * Since: 1.1
 */
typedef void (*AwraAnimationTargetFunc) (double   value,
                                         gpointer user_data);

/**
 * AwraCallbackAnimationTarget:
 *
 * An animation target that calls a function with each value.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_CALLBACK_ANIMATION_TARGET (awra_callback_animation_target_get_type ())
GDK_DECLARE_INTERNAL_TYPE (AwraCallbackAnimationTarget, awra_callback_animation_target, AWRA, CALLBACK_ANIMATION_TARGET, AwraAnimationTarget)

/**
 * awra_callback_animation_target_new:
 * @callback: (scope notified) (closure user_data) (destroy destroy): the
 *   function that receives each value
 * @user_data: data passed to @callback
 * @destroy: (nullable): frees @user_data when the target is finalized
 *
 * Creates a target that calls @callback with the animation's value on every
 * frame, typically to store it and queue a draw.
 *
 * Returns: (transfer full): a new animation target
 *
 * Since: 1.1
 */
AwraAnimationTarget *awra_callback_animation_target_new (AwraAnimationTargetFunc callback,
                                                         gpointer                user_data,
                                                         GDestroyNotify          destroy);

/**
 * AwraPropertyAnimationTarget:
 *
 * An animation target that writes each value to a numeric property of an
 * object. Integer properties receive the rounded value.
 *
 * The target holds a weak reference to the object: once the object is
 * finalized, values are ignored.
 *
 * Since: 1.1
 */
#define AWRA_TYPE_PROPERTY_ANIMATION_TARGET (awra_property_animation_target_get_type ())
GDK_DECLARE_INTERNAL_TYPE (AwraPropertyAnimationTarget, awra_property_animation_target, AWRA, PROPERTY_ANIMATION_TARGET, AwraAnimationTarget)

/**
 * awra_property_animation_target_new:
 * @object: the object whose property is animated
 * @property_name: the name of a writable numeric property of @object
 *
 * Creates a target that writes each value to @property_name of @object.
 *
 * Returns: (transfer full): a new animation target
 *
 * Since: 1.1
 */
AwraAnimationTarget *awra_property_animation_target_new              (GObject                     *object,
                                                                      const char                  *property_name);
/**
 * awra_property_animation_target_get_object:
 * @self: a property animation target
 *
 * Returns: (transfer none) (nullable): the animated object, or %NULL once it
 *   was finalized
 *
 * Since: 1.1
 */
GObject             *awra_property_animation_target_get_object        (AwraPropertyAnimationTarget *self);
/**
 * awra_property_animation_target_get_property_name:
 * @self: a property animation target
 *
 * Returns: (transfer none): the name of the animated property
 *
 * Since: 1.1
 */
const char          *awra_property_animation_target_get_property_name (AwraPropertyAnimationTarget *self);

G_END_DECLS
