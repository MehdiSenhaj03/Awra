/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-animation-target.h>

#include <math.h>

#include "motion/awra-animation-private.h"

G_DEFINE_ABSTRACT_TYPE (AwraAnimationTarget, awra_animation_target, G_TYPE_OBJECT)

static void
awra_animation_target_class_init (AwraAnimationTargetClass *klass)
{
  (void) klass;
}

static void
awra_animation_target_init (AwraAnimationTarget *self)
{
  (void) self;
}

void
awra_animation_target_set_value (AwraAnimationTarget *self,
                                 double               value)
{
  g_return_if_fail (AWRA_IS_ANIMATION_TARGET (self));
  AWRA_ANIMATION_TARGET_GET_CLASS (self)->set_value (self, value);
}

/* Callback target */

struct _AwraCallbackAnimationTarget {
  AwraAnimationTarget parent_instance;
  AwraAnimationTargetFunc callback;
  gpointer user_data;
  GDestroyNotify destroy;
};

struct _AwraCallbackAnimationTargetClass {
  AwraAnimationTargetClass parent_class;
};

G_DEFINE_FINAL_TYPE (AwraCallbackAnimationTarget, awra_callback_animation_target,
                     AWRA_TYPE_ANIMATION_TARGET)

static void
callback_target_set_value (AwraAnimationTarget *target,
                           double               value)
{
  AwraCallbackAnimationTarget *self = AWRA_CALLBACK_ANIMATION_TARGET (target);

  self->callback (value, self->user_data);
}

static void
awra_callback_animation_target_finalize (GObject *object)
{
  AwraCallbackAnimationTarget *self = AWRA_CALLBACK_ANIMATION_TARGET (object);

  if (self->destroy != NULL)
    self->destroy (self->user_data);
  G_OBJECT_CLASS (awra_callback_animation_target_parent_class)->finalize (object);
}

static void
awra_callback_animation_target_class_init (AwraCallbackAnimationTargetClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = awra_callback_animation_target_finalize;
  AWRA_ANIMATION_TARGET_CLASS (klass)->set_value = callback_target_set_value;
}

static void
awra_callback_animation_target_init (AwraCallbackAnimationTarget *self)
{
  (void) self;
}

AwraAnimationTarget *
awra_callback_animation_target_new (AwraAnimationTargetFunc callback,
                                    gpointer                user_data,
                                    GDestroyNotify          destroy)
{
  AwraCallbackAnimationTarget *self;

  g_return_val_if_fail (callback != NULL, NULL);
  self = g_object_new (AWRA_TYPE_CALLBACK_ANIMATION_TARGET, NULL);
  self->callback = callback;
  self->user_data = user_data;
  self->destroy = destroy;
  return AWRA_ANIMATION_TARGET (self);
}

/* Property target */

struct _AwraPropertyAnimationTarget {
  AwraAnimationTarget parent_instance;
  GObject *object;
  GParamSpec *pspec;
  char *property_name;
};

struct _AwraPropertyAnimationTargetClass {
  AwraAnimationTargetClass parent_class;
};

enum {
  PROP_0,
  PROP_OBJECT,
  PROP_PROPERTY_NAME,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraPropertyAnimationTarget, awra_property_animation_target,
                     AWRA_TYPE_ANIMATION_TARGET)

static gboolean
is_integer_type (GType type)
{
  switch (G_TYPE_FUNDAMENTAL (type)) {
    case G_TYPE_CHAR:
    case G_TYPE_UCHAR:
    case G_TYPE_INT:
    case G_TYPE_UINT:
    case G_TYPE_LONG:
    case G_TYPE_ULONG:
    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      return TRUE;
    default:
      return FALSE;
  }
}

static gboolean
is_numeric_type (GType type)
{
  return is_integer_type (type) ||
         G_TYPE_FUNDAMENTAL (type) == G_TYPE_FLOAT ||
         G_TYPE_FUNDAMENTAL (type) == G_TYPE_DOUBLE;
}

static void
property_target_set_value (AwraAnimationTarget *target,
                           double               value)
{
  AwraPropertyAnimationTarget *self = AWRA_PROPERTY_ANIMATION_TARGET (target);
  g_auto (GValue) source = G_VALUE_INIT;
  g_auto (GValue) converted = G_VALUE_INIT;
  GType type;

  if (self->object == NULL || self->pspec == NULL)
    return;
  type = G_PARAM_SPEC_VALUE_TYPE (self->pspec);
  if (is_integer_type (type)) {
    g_value_init (&source, G_TYPE_INT64);
    g_value_set_int64 (&source, (gint64) llround (value));
  } else {
    g_value_init (&source, G_TYPE_DOUBLE);
    g_value_set_double (&source, value);
  }
  g_value_init (&converted, type);
  if (!g_value_transform (&source, &converted))
    return;
  g_param_value_validate (self->pspec, &converted);
  g_object_set_property (self->object, self->property_name, &converted);
}

static void
property_target_set_object (AwraPropertyAnimationTarget *self,
                            GObject                     *object)
{
  if (self->object != NULL)
    g_object_remove_weak_pointer (self->object, (gpointer *) &self->object);
  self->object = object;
  if (object != NULL)
    g_object_add_weak_pointer (object, (gpointer *) &self->object);
}

static void
awra_property_animation_target_constructed (GObject *object)
{
  AwraPropertyAnimationTarget *self = AWRA_PROPERTY_ANIMATION_TARGET (object);

  G_OBJECT_CLASS (awra_property_animation_target_parent_class)->constructed (object);
  if (self->object == NULL || self->property_name == NULL)
    return;
  self->pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (self->object),
                                              self->property_name);
  if (self->pspec == NULL) {
    g_critical ("%s has no property named '%s'",
                G_OBJECT_TYPE_NAME (self->object), self->property_name);
    return;
  }
  if (!(self->pspec->flags & G_PARAM_WRITABLE) ||
      (self->pspec->flags & G_PARAM_CONSTRUCT_ONLY) ||
      !is_numeric_type (G_PARAM_SPEC_VALUE_TYPE (self->pspec))) {
    g_critical ("%s:%s is not a writable numeric property",
                G_OBJECT_TYPE_NAME (self->object), self->property_name);
    self->pspec = NULL;
    return;
  }
  g_param_spec_ref (self->pspec);
}

static void
awra_property_animation_target_dispose (GObject *object)
{
  property_target_set_object (AWRA_PROPERTY_ANIMATION_TARGET (object), NULL);
  G_OBJECT_CLASS (awra_property_animation_target_parent_class)->dispose (object);
}

static void
awra_property_animation_target_finalize (GObject *object)
{
  AwraPropertyAnimationTarget *self = AWRA_PROPERTY_ANIMATION_TARGET (object);

  g_clear_pointer (&self->pspec, g_param_spec_unref);
  g_free (self->property_name);
  G_OBJECT_CLASS (awra_property_animation_target_parent_class)->finalize (object);
}

static void
awra_property_animation_target_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  AwraPropertyAnimationTarget *self = AWRA_PROPERTY_ANIMATION_TARGET (object);

  switch (prop_id) {
    case PROP_OBJECT:
      g_value_set_object (value, self->object);
      break;
    case PROP_PROPERTY_NAME:
      g_value_set_string (value, self->property_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_property_animation_target_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  AwraPropertyAnimationTarget *self = AWRA_PROPERTY_ANIMATION_TARGET (object);

  switch (prop_id) {
    case PROP_OBJECT:
      property_target_set_object (self, g_value_get_object (value));
      break;
    case PROP_PROPERTY_NAME:
      self->property_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_property_animation_target_class_init (AwraPropertyAnimationTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = awra_property_animation_target_constructed;
  object_class->dispose = awra_property_animation_target_dispose;
  object_class->finalize = awra_property_animation_target_finalize;
  object_class->get_property = awra_property_animation_target_get_property;
  object_class->set_property = awra_property_animation_target_set_property;
  AWRA_ANIMATION_TARGET_CLASS (klass)->set_value = property_target_set_value;

  /**
   * AwraPropertyAnimationTarget:object:
   *
   * The object whose property is animated, held weakly.
   *
   * Since: 1.1
   */
  properties[PROP_OBJECT] =
    g_param_spec_object ("object", NULL, NULL, G_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraPropertyAnimationTarget:property-name:
   *
   * The name of the animated property.
   *
   * Since: 1.1
   */
  properties[PROP_PROPERTY_NAME] =
    g_param_spec_string ("property-name", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_property_animation_target_init (AwraPropertyAnimationTarget *self)
{
  (void) self;
}

AwraAnimationTarget *
awra_property_animation_target_new (GObject    *object,
                                    const char *property_name)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);
  return g_object_new (AWRA_TYPE_PROPERTY_ANIMATION_TARGET,
                       "object", object,
                       "property-name", property_name,
                       NULL);
}

GObject *
awra_property_animation_target_get_object (AwraPropertyAnimationTarget *self)
{
  g_return_val_if_fail (AWRA_IS_PROPERTY_ANIMATION_TARGET (self), NULL);
  return self->object;
}

const char *
awra_property_animation_target_get_property_name (AwraPropertyAnimationTarget *self)
{
  g_return_val_if_fail (AWRA_IS_PROPERTY_ANIMATION_TARGET (self), NULL);
  return self->property_name;
}
