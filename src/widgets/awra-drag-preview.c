/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-drag-preview.h>

#include <math.h>

#define PILE_SIZE 3
#define FAN_DURATION_US 220000

struct _AwraDragPreview {
  GtkWidget parent_instance;
  GtkLabel *badge;
  GdkPaintable *items[PILE_SIZE];
  int icon_size;
  guint count;
  double spread;
  gint64 started;
  guint tick_id;
};

enum {
  PROP_0,
  PROP_ICON_SIZE,
  PROP_COUNT,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];
/* Rotation and horizontal shift of each layer when fully fanned out. */
static const float layer_angle[PILE_SIZE] = { 0.0f, -9.0f, 10.0f };
static const float layer_shift[PILE_SIZE] = { 0.0f, -0.20f, 0.20f };

G_DEFINE_FINAL_TYPE (AwraDragPreview, awra_drag_preview, GTK_TYPE_WIDGET)

static int
extent (AwraDragPreview *self)
{
  /* Room for the fan and the badge, so the widget never resizes in flight. */
  return self->icon_size + self->icon_size / 2 + 12;
}

static void
item_invalidated (AwraDragPreview *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
fan_tick (GtkWidget     *widget,
          GdkFrameClock *clock,
          gpointer       user_data)
{
  AwraDragPreview *self = AWRA_DRAG_PREVIEW (widget);
  double t;

  (void) user_data;
  if (self->started == 0)
    self->started = gdk_frame_clock_get_frame_time (clock);
  t = (double) (gdk_frame_clock_get_frame_time (clock) - self->started) / FAN_DURATION_US;
  t = CLAMP (t, 0.0, 1.0);
  /* Ease out with a slight overshoot, as if the pile settled in the hand. */
  self->spread = 1.0 + 2.2 * pow (t - 1.0, 3.0) + 1.2 * pow (t - 1.0, 2.0);
  gtk_widget_queue_draw (widget);
  if (t >= 1.0) {
    self->spread = 1.0;
    self->tick_id = 0;
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

static void
start_fan (AwraDragPreview *self)
{
  gboolean animate = FALSE;

  if (self->tick_id != 0 || self->count < 2)
    return;
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-enable-animations", &animate, NULL);
  if (!animate || !gtk_widget_get_mapped (GTK_WIDGET (self))) {
    self->spread = animate ? 0.0 : 1.0;
    return;
  }
  self->started = 0;
  self->spread = 0.0;
  self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self), fan_tick, NULL, NULL);
}

static void
update_badge (AwraDragPreview *self)
{
  g_autofree char *text = NULL;

  gtk_widget_set_visible (GTK_WIDGET (self->badge), self->count > 1);
  text = self->count > 999 ? g_strdup ("999+") : g_strdup_printf ("%u", self->count);
  gtk_label_set_text (self->badge, text);
}

static void
awra_drag_preview_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (awra_drag_preview_parent_class)->map (widget);
  start_fan (AWRA_DRAG_PREVIEW (widget));
}

static void
awra_drag_preview_measure (GtkWidget      *widget,
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
  *minimum = *natural = extent (AWRA_DRAG_PREVIEW (widget));
}

static void
awra_drag_preview_size_allocate (GtkWidget *widget,
                                 int        width,
                                 int        height,
                                 int        baseline)
{
  AwraDragPreview *self = AWRA_DRAG_PREVIEW (widget);
  GtkRequisition size;
  GtkAllocation allocation;

  (void) height;
  (void) baseline;
  gtk_widget_get_preferred_size (GTK_WIDGET (self->badge), NULL, &size);
  allocation.width = size.width;
  allocation.height = size.height;
  allocation.x = MIN (width - size.width, width / 2 + self->icon_size / 2 - size.width / 2);
  allocation.y = MAX (0, (height - self->icon_size) / 2 - size.height / 2);
  gtk_widget_size_allocate (GTK_WIDGET (self->badge), &allocation, -1);
}

static void
snapshot_item (AwraDragPreview *self,
               GtkSnapshot     *snapshot,
               guint            layer,
               double           scale)
{
  float width = gtk_widget_get_width (GTK_WIDGET (self));
  float height = gtk_widget_get_height (GTK_WIDGET (self));
  float size = self->icon_size * scale;
  double ratio = gdk_paintable_get_intrinsic_aspect_ratio (self->items[layer]);
  float item_width = ratio > 1.0 ? size : ratio > 0.0 ? size * ratio : size;
  float item_height = ratio > 1.0 ? size / ratio : size;
  float shift = layer_shift[layer] * self->icon_size * self->spread;
  GskShadow shadow = { { 0, 0, 0, 0.42f }, 0, 3, 7 };

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2 + shift, height / 2));
  gtk_snapshot_rotate (snapshot, layer_angle[layer] * self->spread);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-item_width / 2, -item_height / 2));
  gtk_snapshot_push_shadow (snapshot, &shadow, 1);
  gdk_paintable_snapshot (self->items[layer], snapshot, item_width, item_height);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
}

static void
awra_drag_preview_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  AwraDragPreview *self = AWRA_DRAG_PREVIEW (widget);

  /* Back to front: the first item stays on top of the pile. */
  for (int layer = PILE_SIZE - 1; layer >= 0; layer--) {
    if (self->items[layer] != NULL)
      snapshot_item (self, snapshot, layer, layer == 0 ? 1.0 : 0.88);
  }
  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->badge), snapshot);
}

static void
awra_drag_preview_dispose (GObject *object)
{
  AwraDragPreview *self = AWRA_DRAG_PREVIEW (object);

  for (guint i = 0; i < PILE_SIZE; i++)
    awra_drag_preview_set_item (self, i, NULL);
  if (self->badge != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->badge));
    self->badge = NULL;
  }
  G_OBJECT_CLASS (awra_drag_preview_parent_class)->dispose (object);
}

static void
awra_drag_preview_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  AwraDragPreview *self = AWRA_DRAG_PREVIEW (object);

  switch (prop_id) {
    case PROP_ICON_SIZE:
      g_value_set_int (value, self->icon_size);
      break;
    case PROP_COUNT:
      g_value_set_uint (value, self->count);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_drag_preview_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  AwraDragPreview *self = AWRA_DRAG_PREVIEW (object);

  switch (prop_id) {
    case PROP_ICON_SIZE:
      awra_drag_preview_set_icon_size (self, g_value_get_int (value));
      break;
    case PROP_COUNT:
      awra_drag_preview_set_count (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_drag_preview_class_init (AwraDragPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_drag_preview_dispose;
  object_class->get_property = awra_drag_preview_get_property;
  object_class->set_property = awra_drag_preview_set_property;
  widget_class->map = awra_drag_preview_map;
  widget_class->measure = awra_drag_preview_measure;
  widget_class->size_allocate = awra_drag_preview_size_allocate;
  widget_class->snapshot = awra_drag_preview_snapshot;
  properties[PROP_ICON_SIZE] =
    g_param_spec_int ("icon-size", NULL, NULL, 16, 512, 64,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_COUNT] =
    g_param_spec_uint ("count", NULL, NULL, 0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awradragpreview");
}

static void
awra_drag_preview_init (AwraDragPreview *self)
{
  self->icon_size = 64;
  self->spread = 1.0;
  self->badge = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self->badge), "awra-drag-preview-count");
  gtk_widget_set_visible (GTK_WIDGET (self->badge), FALSE);
  gtk_widget_set_parent (GTK_WIDGET (self->badge), GTK_WIDGET (self));
}

GtkWidget *
awra_drag_preview_new (void)
{
  return g_object_new (AWRA_TYPE_DRAG_PREVIEW, NULL);
}

int
awra_drag_preview_get_icon_size (AwraDragPreview *self)
{
  g_return_val_if_fail (AWRA_IS_DRAG_PREVIEW (self), 0);
  return self->icon_size;
}

void
awra_drag_preview_set_icon_size (AwraDragPreview *self,
                                 int              size)
{
  g_return_if_fail (AWRA_IS_DRAG_PREVIEW (self));
  g_return_if_fail (size >= 16 && size <= 512);
  if (self->icon_size == size)
    return;
  self->icon_size = size;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_SIZE]);
}

guint
awra_drag_preview_get_count (AwraDragPreview *self)
{
  g_return_val_if_fail (AWRA_IS_DRAG_PREVIEW (self), 0);
  return self->count;
}

void
awra_drag_preview_set_count (AwraDragPreview *self,
                             guint            count)
{
  g_return_if_fail (AWRA_IS_DRAG_PREVIEW (self));
  if (self->count == count)
    return;
  self->count = count;
  update_badge (self);
  start_fan (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COUNT]);
}

void
awra_drag_preview_set_item (AwraDragPreview *self,
                            guint            index,
                            GdkPaintable    *paintable)
{
  g_return_if_fail (AWRA_IS_DRAG_PREVIEW (self));
  g_return_if_fail (index < PILE_SIZE);
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));
  if (self->items[index] != NULL)
    g_signal_handlers_disconnect_by_func (self->items[index], item_invalidated, self);
  g_set_object (&self->items[index], paintable);
  if (paintable != NULL)
    g_signal_connect_swapped (paintable, "invalidate-contents", G_CALLBACK (item_invalidated), self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}
