/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <string.h>

#include <awra/awra-emoji-item.h>

struct _AwraEmojiItem {
  GObject parent_instance;
  char *id;
  char *name;
  char *text;
  GdkPaintable *paintable;
  char **keywords;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_TEXT,
  PROP_PAINTABLE,
  PROP_KEYWORDS,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraEmojiItem, awra_emoji_item, G_TYPE_OBJECT)

static void
awra_emoji_item_finalize (GObject *object)
{
  AwraEmojiItem *self = AWRA_EMOJI_ITEM (object);

  g_free (self->id);
  g_free (self->name);
  g_free (self->text);
  g_clear_object (&self->paintable);
  g_strfreev (self->keywords);
  G_OBJECT_CLASS (awra_emoji_item_parent_class)->finalize (object);
}

static void
awra_emoji_item_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  AwraEmojiItem *self = AWRA_EMOJI_ITEM (object);

  switch (prop_id) {
  case PROP_ID:
    g_value_set_string (value, self->id);
    break;
  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;
  case PROP_TEXT:
    g_value_set_string (value, self->text);
    break;
  case PROP_PAINTABLE:
    g_value_set_object (value, self->paintable);
    break;
  case PROP_KEYWORDS:
    g_value_set_boxed (value, self->keywords);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_emoji_item_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  AwraEmojiItem *self = AWRA_EMOJI_ITEM (object);

  switch (prop_id) {
  case PROP_ID:
    self->id = g_value_dup_string (value);
    break;
  case PROP_NAME:
    self->name = g_value_dup_string (value);
    break;
  case PROP_TEXT:
    self->text = g_value_dup_string (value);
    break;
  case PROP_PAINTABLE:
    awra_emoji_item_set_paintable (self, g_value_get_object (value));
    break;
  case PROP_KEYWORDS:
    awra_emoji_item_set_keywords (self, g_value_get_boxed (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_emoji_item_class_init (AwraEmojiItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = awra_emoji_item_finalize;
  object_class->get_property = awra_emoji_item_get_property;
  object_class->set_property = awra_emoji_item_set_property;

  /**
   * AwraEmojiItem:id:
   *
   * What identifies the item for the application.
   *
   * Since: 1.1
   */
  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraEmojiItem:name:
   *
   * The short name, shown as `:name:`.
   *
   * Since: 1.1
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraEmojiItem:text:
   *
   * The emoji itself, or %NULL for an image.
   *
   * Since: 1.1
   */
  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraEmojiItem:paintable:
   *
   * The image, which replaces the text where both are set.
   *
   * Since: 1.1
   */
  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL, GDK_TYPE_PAINTABLE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraEmojiItem:keywords:
   *
   * More words a search finds the item by.
   *
   * Since: 1.1
   */
  properties[PROP_KEYWORDS] =
    g_param_spec_boxed ("keywords", NULL, NULL, G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_emoji_item_init (AwraEmojiItem *self)
{
  (void) self;
}

AwraEmojiItem *
awra_emoji_item_new (const char *id,
                     const char *name,
                     const char *text)
{
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  return g_object_new (AWRA_TYPE_EMOJI_ITEM, "id", id, "name", name, "text", text, NULL);
}

AwraEmojiItem *
awra_emoji_item_new_for_paintable (const char   *id,
                                   const char   *name,
                                   GdkPaintable *paintable)
{
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);
  return g_object_new (AWRA_TYPE_EMOJI_ITEM, "id", id, "name", name, "paintable", paintable, NULL);
}

const char *
awra_emoji_item_get_id (AwraEmojiItem *self)
{
  g_return_val_if_fail (AWRA_IS_EMOJI_ITEM (self), NULL);
  return self->id;
}

const char *
awra_emoji_item_get_name (AwraEmojiItem *self)
{
  g_return_val_if_fail (AWRA_IS_EMOJI_ITEM (self), NULL);
  return self->name;
}

const char *
awra_emoji_item_get_text (AwraEmojiItem *self)
{
  g_return_val_if_fail (AWRA_IS_EMOJI_ITEM (self), NULL);
  return self->text;
}

GdkPaintable *
awra_emoji_item_get_paintable (AwraEmojiItem *self)
{
  g_return_val_if_fail (AWRA_IS_EMOJI_ITEM (self), NULL);
  return self->paintable;
}

void
awra_emoji_item_set_paintable (AwraEmojiItem *self,
                               GdkPaintable  *paintable)
{
  g_return_if_fail (AWRA_IS_EMOJI_ITEM (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));
  if (g_set_object (&self->paintable, paintable))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

const char * const *
awra_emoji_item_get_keywords (AwraEmojiItem *self)
{
  g_return_val_if_fail (AWRA_IS_EMOJI_ITEM (self), NULL);
  return (const char * const *) self->keywords;
}

void
awra_emoji_item_set_keywords (AwraEmojiItem      *self,
                              const char * const *keywords)
{
  g_return_if_fail (AWRA_IS_EMOJI_ITEM (self));
  if (keywords != NULL && keywords[0] == NULL)
    keywords = NULL;
  if (self->keywords == NULL ? keywords == NULL
                             : keywords != NULL &&
                                 g_strv_equal ((const char * const *) self->keywords, keywords))
    return;
  g_strfreev (self->keywords);
  self->keywords = keywords != NULL && keywords[0] != NULL ? g_strdupv ((char **) keywords) : NULL;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEYWORDS]);
}

static gboolean
contains_folded (const char *haystack,
                 const char *folded_query)
{
  g_autofree char *folded = NULL;

  if (haystack == NULL)
    return FALSE;
  folded = g_utf8_casefold (haystack, -1);
  return strstr (folded, folded_query) != NULL;
}

gboolean
awra_emoji_item_matches (AwraEmojiItem *self,
                         const char    *query)
{
  g_autofree char *folded = NULL;

  g_return_val_if_fail (AWRA_IS_EMOJI_ITEM (self), FALSE);
  g_return_val_if_fail (query != NULL, FALSE);
  if (*query == '\0')
    return TRUE;
  folded = g_utf8_casefold (query, -1);
  if (contains_folded (self->name, folded))
    return TRUE;
  for (guint i = 0; self->keywords != NULL && self->keywords[i] != NULL; i++) {
    if (contains_folded (self->keywords[i], folded))
      return TRUE;
  }
  return FALSE;
}
