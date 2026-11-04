/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-rail-group.h>

#include <awra/awra-avatar.h>
#include <awra/awra-spring-animation.h>
#include <awra/awra-token-set.h>
#include <math.h>

#include "navigation/awra-rail-private.h"
#include "widgets/awra-widget-style-private.h"

#define PREVIEW_COUNT 4
#define FOLDER_ICON "awra-folder-symbolic"

/* The face of a group's tile: a two by two preview of its first items while
 * collapsed, a folder while expanded, cross-fading as the group opens. */

#define AWRA_TYPE_RAIL_GROUP_FACE (awra_rail_group_face_get_type ())
G_DECLARE_FINAL_TYPE (AwraRailGroupFace, awra_rail_group_face, AWRA, RAIL_GROUP_FACE, GtkWidget)

struct _AwraRailGroupFace {
  GtkWidget parent_instance;
  GtkWidget *minis[PREVIEW_COUNT];
  guint n_minis;
  char *icon_name;
  GdkPaintable *icon;
  GdkPaintable *folder;
  int icon_px;
  double open;
};

G_DEFINE_FINAL_TYPE (AwraRailGroupFace, awra_rail_group_face, GTK_TYPE_WIDGET)

static void
face_geometry (GtkWidget *widget,
               double    *mini,
               double    *pad,
               double    *gap)
{
  double tile = awra_rail_tile_size (widget);

  *gap = round (tile / 12.0);
  *pad = round (tile / 8.0);
  *mini = floor ((tile - 2 * *pad - *gap) / 2.0);
}

static GdkPaintable *
lookup_icon (GtkWidget  *widget,
             const char *name,
             int         px)
{
  return GDK_PAINTABLE (gtk_icon_theme_lookup_icon (
    gtk_icon_theme_get_for_display (gtk_widget_get_display (widget)), name, NULL, px,
    gtk_widget_get_scale_factor (widget), gtk_widget_get_direction (widget), 0));
}

static void
draw_symbolic (GtkSnapshot   *snapshot,
               GdkPaintable  *icon,
               double         tile,
               int            px,
               const GdkRGBA *color)
{
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (round ((tile - px) / 2),
                                                          round ((tile - px) / 2)));
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (icon), snapshot,
                                            px, px, color, 1);
  gtk_snapshot_restore (snapshot);
}

static void
awra_rail_group_face_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  AwraRailGroupFace *self = AWRA_RAIL_GROUP_FACE (widget);
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);
  double tile = awra_rail_tile_size (widget);
  int px = (int) round (tile * 0.42);

  if (self->icon_px != px) {
    g_clear_object (&self->icon);
    g_clear_object (&self->folder);
    self->icon_px = px;
  }
  if (self->open < 1.0) {
    gtk_snapshot_push_opacity (snapshot, 1.0 - self->open);
    if (self->icon_name != NULL) {
      if (self->icon == NULL)
        self->icon = lookup_icon (widget, self->icon_name, px);
      draw_symbolic (snapshot, self->icon, tile, px, awra_token_set_get_foreground (tokens));
    } else {
      for (guint i = 0; i < self->n_minis; i++)
        gtk_widget_snapshot_child (widget, self->minis[i], snapshot);
    }
    gtk_snapshot_pop (snapshot);
  }
  if (self->open > 0.0) {
    if (self->folder == NULL)
      self->folder = lookup_icon (widget, self->icon_name != NULL ? self->icon_name : FOLDER_ICON,
                                  px);
    gtk_snapshot_push_opacity (snapshot, self->open);
    draw_symbolic (snapshot, self->folder, tile, px, awra_token_set_get_foreground (tokens));
    gtk_snapshot_pop (snapshot);
  }
}

static void
awra_rail_group_face_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  (void) orientation;
  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  *minimum = *natural = (int) awra_rail_tile_size (widget);
}

static void
awra_rail_group_face_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  AwraRailGroupFace *self = AWRA_RAIL_GROUP_FACE (widget);
  gboolean rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  double mini;
  double pad;
  double gap;

  (void) height;
  (void) baseline;
  face_geometry (widget, &mini, &pad, &gap);
  for (guint i = 0; i < self->n_minis; i++) {
    guint column = rtl ? 1 - i % 2 : i % 2;
    double x = pad + column * (mini + gap);
    double y = pad + (i / 2) * (mini + gap);

    /* A single row sits in the middle of the tile. */
    if (self->n_minis <= 2)
      y = (width - mini) / 2;
    gtk_widget_allocate (self->minis[i], (int) mini, (int) mini, -1,
                         gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)));
  }
  awra_widget_present_popovers (widget);
}

static void
awra_rail_group_face_dispose (GObject *object)
{
  AwraRailGroupFace *self = AWRA_RAIL_GROUP_FACE (object);

  for (guint i = 0; i < self->n_minis; i++)
    g_clear_pointer (&self->minis[i], gtk_widget_unparent);
  self->n_minis = 0;
  g_clear_object (&self->icon);
  g_clear_object (&self->folder);
  g_clear_pointer (&self->icon_name, g_free);
  G_OBJECT_CLASS (awra_rail_group_face_parent_class)->dispose (object);
}

static void
awra_rail_group_face_class_init (AwraRailGroupFaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  G_OBJECT_CLASS (klass)->dispose = awra_rail_group_face_dispose;
  widget_class->snapshot = awra_rail_group_face_snapshot;
  widget_class->measure = awra_rail_group_face_measure;
  widget_class->size_allocate = awra_rail_group_face_size_allocate;
  gtk_widget_class_set_css_name (widget_class, "awrarailgroupface");
}

static void
awra_rail_group_face_init (AwraRailGroupFace *self)
{
  gtk_widget_set_can_target (GTK_WIDGET (self), FALSE);
}

static GtkWidget *
mini_for_item (AwraRailGroupFace *self,
               AwraRailItem      *item)
{
  GtkWidget *child = awra_rail_item_get_child (item);
  double mini;
  double pad;
  double gap;
  GtkWidget *avatar;

  face_geometry (GTK_WIDGET (self), &mini, &pad, &gap);
  avatar = awra_avatar_new ((int) MAX (8.0, mini), NULL);
  awra_avatar_set_shape (AWRA_AVATAR (avatar), AWRA_AVATAR_SHAPE_ROUNDED);
  g_object_bind_property (item, "label", avatar, "text", G_BINDING_SYNC_CREATE);
  if (AWRA_IS_AVATAR (child)) {
    g_object_bind_property (child, "custom-image", avatar, "custom-image", G_BINDING_SYNC_CREATE);
    g_object_bind_property (child, "icon-name", avatar, "icon-name", G_BINDING_SYNC_CREATE);
    g_object_bind_property (child, "show-initials", avatar, "show-initials",
                            G_BINDING_SYNC_CREATE);
  } else if (GTK_IS_IMAGE (child) &&
             gtk_image_get_storage_type (GTK_IMAGE (child)) == GTK_IMAGE_PAINTABLE) {
    awra_avatar_set_custom_image (AWRA_AVATAR (avatar), gtk_image_get_paintable (GTK_IMAGE (child)));
  } else if (GTK_IS_IMAGE (child) &&
             gtk_image_get_storage_type (GTK_IMAGE (child)) == GTK_IMAGE_ICON_NAME) {
    awra_avatar_set_icon_name (AWRA_AVATAR (avatar), gtk_image_get_icon_name (GTK_IMAGE (child)));
    awra_avatar_set_show_initials (AWRA_AVATAR (avatar), FALSE);
  } else if (child == NULL && awra_rail_item_get_icon_name (item) != NULL) {
    g_object_bind_property (item, "icon-name", avatar, "icon-name", G_BINDING_SYNC_CREATE);
    awra_avatar_set_show_initials (AWRA_AVATAR (avatar), FALSE);
  }
  return avatar;
}

static void
face_rebuild (AwraRailGroupFace *self,
              GPtrArray         *items)
{
  for (guint i = 0; i < self->n_minis; i++)
    g_clear_pointer (&self->minis[i], gtk_widget_unparent);
  self->n_minis = MIN (items->len, PREVIEW_COUNT);
  for (guint i = 0; i < self->n_minis; i++) {
    self->minis[i] = mini_for_item (self, g_ptr_array_index (items, i));
    gtk_widget_set_parent (self->minis[i], GTK_WIDGET (self));
  }
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

/* The group */

struct _AwraRailGroup {
  GtkWidget parent_instance;
  char *label;
  char *icon_name;
  GdkRGBA color;
  gboolean has_color;
  gboolean expanded;
  AwraRailItem *header;
  AwraRailGroupFace *face;
  GPtrArray *items;
  double reveal;
  AwraAnimation *reveal_animation;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ICON_NAME,
  PROP_COLOR,
  PROP_EXPANDED,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraRailGroup, awra_rail_group, GTK_TYPE_WIDGET)

static void
update_summary (AwraRailGroup *self)
{
  gboolean unread = FALSE;
  gboolean selected = FALSE;
  guint mentions = 0;

  for (guint i = 0; i < self->items->len; i++) {
    AwraRailItem *item = g_ptr_array_index (self->items, i);

    unread |= awra_rail_item_get_unread (item) && !awra_rail_item_get_muted (item);
    selected |= awra_rail_item_get_selected (item);
    mentions += awra_rail_item_get_mention_count (item);
  }
  /* Open, the items speak for themselves. */
  if (self->expanded) {
    unread = FALSE;
    selected = FALSE;
    mentions = 0;
  }
  awra_rail_item_set_unread (self->header, unread);
  awra_rail_item_set_mention_count (self->header, mentions);
  awra_rail_item_set_selected (self->header, selected);
}

static void
item_notify_cb (AwraRailGroup *self,
                GParamSpec    *pspec,
                AwraRailItem  *item)
{
  const char *name = g_param_spec_get_name (pspec);

  (void) item;
  if (g_str_equal (name, "child") || g_str_equal (name, "icon-name"))
    face_rebuild (self->face, self->items);
  else if (g_str_equal (name, "unread") || g_str_equal (name, "muted") ||
           g_str_equal (name, "mention-count") || g_str_equal (name, "selected"))
    update_summary (self);
}

static void
set_reveal (double   value,
            gpointer user_data)
{
  AwraRailGroup *self = user_data;

  self->reveal = CLAMP (value, 0.0, 1.0);
  self->face->open = self->reveal;
  if (self->reveal <= 0.0 && !self->expanded) {
    for (guint i = 0; i < self->items->len; i++)
      gtk_widget_set_child_visible (g_ptr_array_index (self->items, i), FALSE);
  }
  gtk_widget_queue_draw (GTK_WIDGET (self->face));
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
header_activate_cb (AwraRailGroup *self)
{
  awra_rail_group_set_expanded (self, !self->expanded);
}

static double
items_height (AwraRailGroup *self)
{
  double gap = awra_rail_entry_gap (GTK_WIDGET (self));
  double height = 0;

  for (guint i = 0; i < self->items->len; i++) {
    int item_height = 0;

    gtk_widget_measure (g_ptr_array_index (self->items, i), GTK_ORIENTATION_VERTICAL, -1,
                        &item_height, NULL, NULL, NULL);
    height += gap + item_height;
  }
  return height;
}

static void
awra_rail_group_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  AwraRailGroup *self = AWRA_RAIL_GROUP (widget);
  int header = 0;

  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  gtk_widget_measure (GTK_WIDGET (self->header), orientation, -1, &header, NULL, NULL, NULL);
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    *minimum = *natural = header;
    return;
  }
  *minimum = *natural = header + (int) ceil (items_height (self) * self->reveal);
}

static void
awra_rail_group_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  AwraRailGroup *self = AWRA_RAIL_GROUP (widget);
  double gap = awra_rail_entry_gap (widget);
  int header = 0;
  double y;

  (void) height;
  (void) baseline;
  gtk_widget_measure (GTK_WIDGET (self->header), GTK_ORIENTATION_VERTICAL, -1, &header,
                      NULL, NULL, NULL);
  gtk_widget_allocate (GTK_WIDGET (self->header), width, header, -1, NULL);
  y = header;
  for (guint i = 0; i < self->items->len; i++) {
    GtkWidget *item = g_ptr_array_index (self->items, i);
    int item_height = 0;

    gtk_widget_measure (item, GTK_ORIENTATION_VERTICAL, -1, &item_height, NULL, NULL, NULL);
    y += gap;
    gtk_widget_allocate (item, width, item_height, -1,
                         gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (0, y)));
    y += item_height;
  }
  awra_widget_present_popovers (widget);
}

static void
awra_rail_group_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  AwraRailGroup *self = AWRA_RAIL_GROUP (widget);
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);
  double tile = awra_rail_tile_size (widget);
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);

  if (self->reveal > 0.0) {
    /* The open folder: a column the width of a tile behind its items. */
    GdkRGBA color = self->has_color
      ? awra_rgba_with_alpha (&self->color, awra_token_set_get_dark (tokens) ? 0.30 : 0.22)
      : *awra_token_set_get_control_well (tokens);
    GskRoundedRect column;
    graphene_rect_t bounds = GRAPHENE_RECT_INIT (round ((width - tile) / 2), 0, tile, height);

    color.alpha *= self->reveal;
    gsk_rounded_rect_init_from_rect (&column, &bounds, tile / 2);
    gtk_snapshot_push_rounded_clip (snapshot, &column);
    gtk_snapshot_append_color (snapshot, &color, &bounds);
    gtk_snapshot_pop (snapshot);
  }
  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->header), snapshot);
  if (self->reveal <= 0.0)
    return;
  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (-tile, 0, width + 2 * tile, height));
  gtk_snapshot_push_opacity (snapshot, self->reveal);
  for (guint i = 0; i < self->items->len; i++)
    gtk_widget_snapshot_child (widget, g_ptr_array_index (self->items, i), snapshot);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
awra_rail_group_dispose (GObject *object)
{
  AwraRailGroup *self = AWRA_RAIL_GROUP (object);

  if (self->reveal_animation != NULL)
    awra_animation_skip (self->reveal_animation);
  g_clear_object (&self->reveal_animation);
  if (self->items != NULL) {
    for (guint i = 0; i < self->items->len; i++) {
      GtkWidget *item = g_ptr_array_index (self->items, i);

      g_signal_handlers_disconnect_by_data (item, self);
      gtk_widget_unparent (item);
    }
    g_clear_pointer (&self->items, g_ptr_array_unref);
  }
  if (self->header != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->header));
    self->header = NULL;
    self->face = NULL;
  }
  G_OBJECT_CLASS (awra_rail_group_parent_class)->dispose (object);
}

static void
awra_rail_group_finalize (GObject *object)
{
  AwraRailGroup *self = AWRA_RAIL_GROUP (object);

  g_free (self->label);
  g_free (self->icon_name);
  G_OBJECT_CLASS (awra_rail_group_parent_class)->finalize (object);
}

static void
awra_rail_group_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  AwraRailGroup *self = AWRA_RAIL_GROUP (object);

  switch (prop_id) {
    case PROP_LABEL: g_value_set_string (value, self->label); break;
    case PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
    case PROP_COLOR: g_value_set_boxed (value, self->has_color ? &self->color : NULL); break;
    case PROP_EXPANDED: g_value_set_boolean (value, self->expanded); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rail_group_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  AwraRailGroup *self = AWRA_RAIL_GROUP (object);

  switch (prop_id) {
    case PROP_LABEL: awra_rail_group_set_label (self, g_value_get_string (value)); break;
    case PROP_ICON_NAME: awra_rail_group_set_icon_name (self, g_value_get_string (value)); break;
    case PROP_COLOR: awra_rail_group_set_color (self, g_value_get_boxed (value)); break;
    case PROP_EXPANDED: awra_rail_group_set_expanded (self, g_value_get_boolean (value)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rail_group_class_init (AwraRailGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->dispose = awra_rail_group_dispose;
  object_class->finalize = awra_rail_group_finalize;
  object_class->get_property = awra_rail_group_get_property;
  object_class->set_property = awra_rail_group_set_property;
  widget_class->measure = awra_rail_group_measure;
  widget_class->size_allocate = awra_rail_group_size_allocate;
  widget_class->snapshot = awra_rail_group_snapshot;

  /**
   * AwraRailGroup:label:
   *
   * The name of the folder.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL] = g_param_spec_string ("label", NULL, NULL, NULL, flags);
  /**
   * AwraRailGroup:icon-name:
   *
   * An icon that replaces the preview of the items.
   *
   * Since: 1.1
   */
  properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL, NULL, flags);
  /**
   * AwraRailGroup:color:
   *
   * The colour of the folder.
   *
   * Since: 1.1
   */
  properties[PROP_COLOR] = g_param_spec_boxed ("color", NULL, NULL, GDK_TYPE_RGBA, flags);
  /**
   * AwraRailGroup:expanded:
   *
   * Whether the items are shown.
   *
   * Since: 1.1
   */
  properties[PROP_EXPANDED] = g_param_spec_boolean ("expanded", NULL, NULL, FALSE, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awrarailgroup");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_rail_group_init (AwraRailGroup *self)
{
  self->items = g_ptr_array_new ();
  self->header = g_object_new (AWRA_TYPE_RAIL_ITEM,
                               "accessible-role", GTK_ACCESSIBLE_ROLE_BUTTON, NULL);
  awra_rail_item_set_group_header (self->header, FALSE);
  self->face = g_object_new (AWRA_TYPE_RAIL_GROUP_FACE, NULL);
  awra_rail_item_set_child (self->header, GTK_WIDGET (self->face));
  gtk_widget_set_parent (GTK_WIDGET (self->header), GTK_WIDGET (self));
  g_signal_connect_swapped (self->header, "activate", G_CALLBACK (header_activate_cb), self);
}

GtkWidget *
awra_rail_group_new (const char *label)
{
  return g_object_new (AWRA_TYPE_RAIL_GROUP, "label", label, NULL);
}

const char *
awra_rail_group_get_label (AwraRailGroup *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_GROUP (self), NULL);
  return self->label;
}

void
awra_rail_group_set_label (AwraRailGroup *self,
                           const char    *label)
{
  g_return_if_fail (AWRA_IS_RAIL_GROUP (self));
  if (!g_set_str (&self->label, label))
    return;
  awra_rail_item_set_label (self->header, label);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  label != NULL ? label : "", -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

const char *
awra_rail_group_get_icon_name (AwraRailGroup *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_GROUP (self), NULL);
  return self->icon_name;
}

void
awra_rail_group_set_icon_name (AwraRailGroup *self,
                               const char    *icon_name)
{
  g_return_if_fail (AWRA_IS_RAIL_GROUP (self));
  if (!g_set_str (&self->icon_name, icon_name))
    return;
  g_set_str (&self->face->icon_name, icon_name);
  g_clear_object (&self->face->icon);
  g_clear_object (&self->face->folder);
  gtk_widget_queue_draw (GTK_WIDGET (self->face));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

const GdkRGBA *
awra_rail_group_get_color (AwraRailGroup *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_GROUP (self), NULL);
  return self->has_color ? &self->color : NULL;
}

void
awra_rail_group_set_color (AwraRailGroup *self,
                           const GdkRGBA *color)
{
  g_return_if_fail (AWRA_IS_RAIL_GROUP (self));
  if (color == NULL && !self->has_color)
    return;
  if (color != NULL && self->has_color && gdk_rgba_equal (color, &self->color))
    return;
  self->has_color = color != NULL;
  if (color != NULL)
    self->color = *color;
  awra_rail_item_set_tint (self->header, color);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

gboolean
awra_rail_group_get_expanded (AwraRailGroup *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_GROUP (self), FALSE);
  return self->expanded;
}

void
awra_rail_group_set_expanded (AwraRailGroup *self,
                              gboolean       expanded)
{
  g_return_if_fail (AWRA_IS_RAIL_GROUP (self));
  expanded = !!expanded;
  if (self->expanded == expanded)
    return;
  self->expanded = expanded;
  if (expanded) {
    for (guint i = 0; i < self->items->len; i++)
      gtk_widget_set_child_visible (g_ptr_array_index (self->items, i), TRUE);
  }
  if (self->reveal_animation == NULL) {
    self->reveal_animation =
      awra_spring_animation_new (GTK_WIDGET (self), self->reveal, self->reveal, 1.0, 360.0,
                                 awra_callback_animation_target_new (set_reveal, self, NULL));
    awra_spring_animation_set_clamp (AWRA_SPRING_ANIMATION (self->reveal_animation), TRUE);
    awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (self->reveal_animation), 0.002);
  }
  awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (self->reveal_animation),
                                  expanded ? 1.0 : 0.0);
  awra_rail_item_set_group_header (self->header, expanded);
  update_summary (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPANDED]);
}

void
awra_rail_group_append (AwraRailGroup *self,
                        AwraRailItem  *item)
{
  awra_rail_group_insert (self, item, -1);
}

void
awra_rail_group_insert (AwraRailGroup *self,
                        AwraRailItem  *item,
                        int            position)
{
  GtkWidget *sibling = NULL;

  g_return_if_fail (AWRA_IS_RAIL_GROUP (self));
  g_return_if_fail (AWRA_IS_RAIL_ITEM (item));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (item)) == NULL);
  if (position < 0 || (guint) position > self->items->len)
    position = self->items->len;
  if ((guint) position < self->items->len)
    sibling = g_ptr_array_index (self->items, position);
  g_ptr_array_insert (self->items, position, item);
  gtk_widget_insert_before (GTK_WIDGET (item), GTK_WIDGET (self), sibling);
  gtk_widget_set_child_visible (GTK_WIDGET (item), self->expanded || self->reveal > 0.0);
  g_signal_connect_swapped (item, "notify", G_CALLBACK (item_notify_cb), self);
  face_rebuild (self->face, self->items);
  update_summary (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
awra_rail_group_remove (AwraRailGroup *self,
                        AwraRailItem  *item)
{
  GtkWidget *rail;

  g_return_if_fail (AWRA_IS_RAIL_GROUP (self));
  g_return_if_fail (AWRA_IS_RAIL_ITEM (item));
  if (!g_ptr_array_remove (self->items, item)) {
    g_critical ("%s: the item is not in this group", G_STRFUNC);
    return;
  }
  rail = gtk_widget_get_ancestor (GTK_WIDGET (self), AWRA_TYPE_RAIL);
  if (rail != NULL && awra_rail_get_selected_item (AWRA_RAIL (rail)) == item)
    awra_rail_set_selected_item (AWRA_RAIL (rail), NULL);
  g_signal_handlers_disconnect_by_data (item, self);
  gtk_widget_unparent (GTK_WIDGET (item));
  face_rebuild (self->face, self->items);
  update_summary (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

guint
awra_rail_group_get_n_items (AwraRailGroup *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_GROUP (self), 0);
  return self->items->len;
}

AwraRailItem *
awra_rail_group_get_item (AwraRailGroup *self,
                          guint          position)
{
  g_return_val_if_fail (AWRA_IS_RAIL_GROUP (self), NULL);
  if (position >= self->items->len)
    return NULL;
  return g_ptr_array_index (self->items, position);
}

AwraRailItem *
awra_rail_group_get_header (AwraRailGroup *self)
{
  return self->header;
}
