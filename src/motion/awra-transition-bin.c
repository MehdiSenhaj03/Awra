/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-transition-bin.h>

#include <awra/awra-context.h>
#include <awra/awra-motion.h>

/* The old frame only drifts: it must read as leaving, not as moving away. */
#define OUTGOING_DRIFT 0.45
/* Fade through: the old frame is gone before the new one is fully there, so
 * two sets of text never stay legible over each other. */
#define OUTGOING_SPAN 0.45
#define INCOMING_DELAY 0.2

struct _AwraTransitionBin {
  GtkWidget parent_instance;
  GtkWidget *child;
  GdkPaintable *outgoing;
  int fixed_top;
  double offset_x;
  double offset_y;
  double progress;
  gint64 started;
  guint duration;
  guint tick_id;
};

enum {
  PROP_0,
  PROP_CHILD,
  PROP_FIXED_TOP,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraTransitionBin, awra_transition_bin, GTK_TYPE_WIDGET)

static void
finish (AwraTransitionBin *self)
{
  if (self->tick_id != 0) {
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
    self->tick_id = 0;
  }
  g_clear_object (&self->outgoing);
  self->progress = 1.0;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
transition_tick (GtkWidget     *widget,
                 GdkFrameClock *clock,
                 gpointer       user_data)
{
  AwraTransitionBin *self = AWRA_TRANSITION_BIN (widget);
  gint64 now = gdk_frame_clock_get_frame_time (clock);

  (void) user_data;
  if (self->started == 0)
    self->started = now;
  self->progress = (double) (now - self->started) / (self->duration * 1000.0);
  if (self->progress >= 1.0) {
    self->tick_id = 0;
    finish (self);
    return G_SOURCE_REMOVE;
  }
  gtk_widget_queue_draw (widget);
  return G_SOURCE_CONTINUE;
}

static void
awra_transition_bin_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  AwraTransitionBin *self = AWRA_TRANSITION_BIN (widget);
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);
  float top = MIN (self->fixed_top, height);
  double eased;
  double incoming;

  if (self->child == NULL)
    return;
  if (self->tick_id == 0) {
    gtk_widget_snapshot_child (widget, self->child, snapshot);
    return;
  }
  eased = awra_motion_ease_out_cubic (self->progress);
  incoming = awra_motion_ease_out_cubic ((self->progress - INCOMING_DELAY) / (1.0 - INCOMING_DELAY));
  if (top > 0) {
    gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, top));
    gtk_widget_snapshot_child (widget, self->child, snapshot);
    gtk_snapshot_pop (snapshot);
  }
  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, top, width, height - top));
  if (self->outgoing != NULL && self->progress < OUTGOING_SPAN) {
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot,
                            &GRAPHENE_POINT_INIT (-self->offset_x * OUTGOING_DRIFT * eased,
                                                  -self->offset_y * OUTGOING_DRIFT * eased));
    gtk_snapshot_push_opacity (snapshot, 1.0 - self->progress / OUTGOING_SPAN);
    gdk_paintable_snapshot (self->outgoing, snapshot, width, height);
    gtk_snapshot_pop (snapshot);
    gtk_snapshot_restore (snapshot);
  }
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (self->offset_x * (1.0 - incoming),
                                                self->offset_y * (1.0 - incoming)));
  gtk_snapshot_push_opacity (snapshot, incoming);
  gtk_widget_snapshot_child (widget, self->child, snapshot);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
awra_transition_bin_unmap (GtkWidget *widget)
{
  finish (AWRA_TRANSITION_BIN (widget));
  GTK_WIDGET_CLASS (awra_transition_bin_parent_class)->unmap (widget);
}

static void
awra_transition_bin_dispose (GObject *object)
{
  AwraTransitionBin *self = AWRA_TRANSITION_BIN (object);

  g_clear_object (&self->outgoing);
  g_clear_pointer (&self->child, gtk_widget_unparent);
  G_OBJECT_CLASS (awra_transition_bin_parent_class)->dispose (object);
}

static void
awra_transition_bin_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  AwraTransitionBin *self = AWRA_TRANSITION_BIN (object);

  switch (prop_id) {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;
    case PROP_FIXED_TOP:
      g_value_set_int (value, self->fixed_top);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_transition_bin_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  AwraTransitionBin *self = AWRA_TRANSITION_BIN (object);

  switch (prop_id) {
    case PROP_CHILD:
      awra_transition_bin_set_child (self, g_value_get_object (value));
      break;
    case PROP_FIXED_TOP:
      awra_transition_bin_set_fixed_top (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_transition_bin_class_init (AwraTransitionBinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_transition_bin_dispose;
  object_class->get_property = awra_transition_bin_get_property;
  object_class->set_property = awra_transition_bin_set_property;
  widget_class->snapshot = awra_transition_bin_snapshot;
  widget_class->unmap = awra_transition_bin_unmap;
  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_FIXED_TOP] =
    g_param_spec_int ("fixed-top", NULL, NULL, 0, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awratransitionbin");
}

static void
awra_transition_bin_init (AwraTransitionBin *self)
{
  self->progress = 1.0;
}

GtkWidget *
awra_transition_bin_new (void)
{
  return g_object_new (AWRA_TYPE_TRANSITION_BIN, NULL);
}

GtkWidget *
awra_transition_bin_get_child (AwraTransitionBin *self)
{
  g_return_val_if_fail (AWRA_IS_TRANSITION_BIN (self), NULL);
  return self->child;
}

void
awra_transition_bin_set_child (AwraTransitionBin *self,
                               GtkWidget         *child)
{
  g_return_if_fail (AWRA_IS_TRANSITION_BIN (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));
  if (self->child == child)
    return;
  g_clear_pointer (&self->child, gtk_widget_unparent);
  self->child = child;
  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

int
awra_transition_bin_get_fixed_top (AwraTransitionBin *self)
{
  g_return_val_if_fail (AWRA_IS_TRANSITION_BIN (self), 0);
  return self->fixed_top;
}

void
awra_transition_bin_set_fixed_top (AwraTransitionBin *self,
                                   int                height)
{
  g_return_if_fail (AWRA_IS_TRANSITION_BIN (self));
  g_return_if_fail (height >= 0);
  if (self->fixed_top == height)
    return;
  self->fixed_top = height;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FIXED_TOP]);
}

void
awra_transition_bin_play (AwraTransitionBin *self,
                          double             offset_x,
                          double             offset_y)
{
  GtkWidget *widget;
  AwraContext *context;
  guint duration;
  g_autoptr (GdkPaintable) live = NULL;

  g_return_if_fail (AWRA_IS_TRANSITION_BIN (self));
  widget = GTK_WIDGET (self);
  if (!gtk_widget_get_mapped (widget) || self->child == NULL)
    return;
  context = awra_context_get_for_display (gtk_widget_get_display (widget));
  duration = awra_motion_get_duration (awra_context_get_style_manager (context),
                                       AWRA_MOTION_PRESET_NORMAL);
  if (duration == 0)
    return;
  live = gtk_widget_paintable_new (widget);
  g_set_object (&self->outgoing, NULL);
  self->outgoing = gdk_paintable_get_current_image (live);
  self->offset_x = offset_x;
  self->offset_y = offset_y;
  self->duration = duration;
  self->progress = 0.0;
  self->started = 0;
  if (self->tick_id == 0)
    self->tick_id = gtk_widget_add_tick_callback (widget, transition_tick, NULL, NULL);
}
