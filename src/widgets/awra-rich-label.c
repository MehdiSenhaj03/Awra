/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-rich-label.h>

/* The text stays a GtkLabel, with everything a label does. Each U+FFFC in
 * it gets a Pango shape attribute, which reserves a square on the line the
 * way a glyph would; the label is drawn, then each picture is drawn over its
 * square, found through the label's own layout. Nothing is laid out twice
 * and no widget is made per picture. */

#define REPLACEMENT "\xef\xbf\xbc" /* U+FFFC */

struct _AwraRichLabel {
  GtkWidget parent_instance;
  GtkLabel *label;
  GPtrArray *paintables; /* GdkPaintable or NULL, in text order */
  int inline_size;
  gulong label_handler;
};

enum { PROP_0, PROP_INLINE_SIZE, N_PROPS };
static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraRichLabel, awra_rich_label, GTK_TYPE_WIDGET)

static int
resolved_size (AwraRichLabel *self)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  int height;

  if (self->inline_size > 0)
    return self->inline_size;
  /* As tall as the text's own line, so a picture reads as a letter. */
  context = gtk_widget_get_pango_context (GTK_WIDGET (self->label));
  metrics = pango_context_get_metrics (context, NULL, NULL);
  height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                         pango_font_metrics_get_descent (metrics));
  pango_font_metrics_unref (metrics);
  return MAX (height, 12);
}

/* Shapes each U+FFFC as a square standing on the baseline. */
static void
update_shapes (AwraRichLabel *self)
{
  const char *text = gtk_label_get_text (self->label);
  int size = resolved_size (self) * PANGO_SCALE;
  PangoRectangle rect = { 0, -size * 4 / 5, size, size };
  PangoAttrList *attributes = pango_attr_list_new ();
  const char *at = text;

  while ((at = strstr (at, REPLACEMENT)) != NULL) {
    PangoAttribute *shape = pango_attr_shape_new (&rect, &rect);

    shape->start_index = (guint) (at - text);
    shape->end_index = shape->start_index + strlen (REPLACEMENT);
    pango_attr_list_insert (attributes, shape);
    at += strlen (REPLACEMENT);
  }
  gtk_label_set_attributes (self->label, attributes);
  pango_attr_list_unref (attributes);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
label_changed_cb (GtkLabel      *label,
                  GParamSpec    *pspec,
                  AwraRichLabel *self)
{
  (void) label;
  (void) pspec;
  update_shapes (self);
}

static void
paintable_invalidated_cb (GdkPaintable  *paintable,
                          AwraRichLabel *self)
{
  (void) paintable;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
awra_rich_label_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  gtk_widget_measure (GTK_WIDGET (AWRA_RICH_LABEL (widget)->label), orientation, for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
awra_rich_label_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  gtk_widget_allocate (GTK_WIDGET (AWRA_RICH_LABEL (widget)->label), width, height, baseline, NULL);
}

static GtkSizeRequestMode
awra_rich_label_get_request_mode (GtkWidget *widget)
{
  return gtk_widget_get_request_mode (GTK_WIDGET (AWRA_RICH_LABEL (widget)->label));
}

static void
awra_rich_label_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  AwraRichLabel *self = AWRA_RICH_LABEL (widget);
  const char *text;
  PangoLayout *layout;
  const char *at;
  int offset_x = 0;
  int offset_y = 0;
  guint n = 0;

  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->label), snapshot);
  if (self->paintables->len == 0)
    return;
  text = gtk_label_get_text (self->label);
  layout = gtk_label_get_layout (self->label);
  gtk_label_get_layout_offsets (self->label, &offset_x, &offset_y);
  at = text;
  while ((at = strstr (at, REPLACEMENT)) != NULL && n < self->paintables->len) {
    GdkPaintable *paintable = g_ptr_array_index (self->paintables, n++);
    PangoRectangle place;

    if (paintable != NULL) {
      pango_layout_index_to_pos (layout, (int) (at - text), &place);
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
        offset_x + (float) place.x / PANGO_SCALE,
        offset_y + (float) place.y / PANGO_SCALE));
      gdk_paintable_snapshot (paintable, snapshot,
                              (double) place.width / PANGO_SCALE,
                              (double) place.height / PANGO_SCALE);
      gtk_snapshot_restore (snapshot);
    }
    at += strlen (REPLACEMENT);
  }
}

static void
awra_rich_label_css_changed (GtkWidget         *widget,
                             GtkCssStyleChange *change)
{
  GTK_WIDGET_CLASS (awra_rich_label_parent_class)->css_changed (widget, change);
  /* The text's size may have changed, and with it the pictures'. */
  update_shapes (AWRA_RICH_LABEL (widget));
}

static void
drop_paintables (AwraRichLabel *self)
{
  for (guint i = 0; i < self->paintables->len; i++) {
    GdkPaintable *paintable = g_ptr_array_index (self->paintables, i);

    if (paintable != NULL)
      g_signal_handlers_disconnect_by_func (paintable, paintable_invalidated_cb, self);
  }
  g_ptr_array_set_size (self->paintables, 0);
}

static void
awra_rich_label_dispose (GObject *object)
{
  AwraRichLabel *self = AWRA_RICH_LABEL (object);

  if (self->paintables != NULL)
    drop_paintables (self);
  if (self->label != NULL) {
    g_clear_signal_handler (&self->label_handler, self->label);
    gtk_widget_unparent (GTK_WIDGET (self->label));
    self->label = NULL;
  }
  G_OBJECT_CLASS (awra_rich_label_parent_class)->dispose (object);
}

static void
awra_rich_label_finalize (GObject *object)
{
  g_clear_pointer (&AWRA_RICH_LABEL (object)->paintables, g_ptr_array_unref);
  G_OBJECT_CLASS (awra_rich_label_parent_class)->finalize (object);
}

static void
awra_rich_label_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  switch (prop_id) {
  case PROP_INLINE_SIZE:
    g_value_set_int (value, AWRA_RICH_LABEL (object)->inline_size);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rich_label_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  switch (prop_id) {
  case PROP_INLINE_SIZE:
    awra_rich_label_set_inline_size (AWRA_RICH_LABEL (object), g_value_get_int (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rich_label_class_init (AwraRichLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_rich_label_dispose;
  object_class->finalize = awra_rich_label_finalize;
  object_class->get_property = awra_rich_label_get_property;
  object_class->set_property = awra_rich_label_set_property;
  widget_class->measure = awra_rich_label_measure;
  widget_class->size_allocate = awra_rich_label_size_allocate;
  widget_class->get_request_mode = awra_rich_label_get_request_mode;
  widget_class->snapshot = awra_rich_label_snapshot;
  widget_class->css_changed = awra_rich_label_css_changed;

  /**
   * AwraRichLabel:inline-size:
   *
   * The side of each picture in logical pixels; 0 follows the text's height.
   *
   * Since: 1.1
   */
  properties[PROP_INLINE_SIZE] = g_param_spec_int (
    "inline-size", NULL, NULL, 0, 512, 0,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awrarichlabel");
}

static void
awra_rich_label_init (AwraRichLabel *self)
{
  self->paintables = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  self->label = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_set_parent (GTK_WIDGET (self->label), GTK_WIDGET (self));
  self->label_handler = g_signal_connect (self->label, "notify::label",
                                          G_CALLBACK (label_changed_cb), self);
}

GtkWidget *
awra_rich_label_new (void)
{
  return g_object_new (AWRA_TYPE_RICH_LABEL, NULL);
}

GtkLabel *
awra_rich_label_get_label (AwraRichLabel *self)
{
  g_return_val_if_fail (AWRA_IS_RICH_LABEL (self), NULL);
  return self->label;
}

void
awra_rich_label_append_paintable (AwraRichLabel *self,
                                  GdkPaintable  *paintable)
{
  g_return_if_fail (AWRA_IS_RICH_LABEL (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));
  if (paintable != NULL) {
    g_ptr_array_add (self->paintables, g_object_ref (paintable));
    /* An animated picture, or one that arrives later, redraws the text. */
    if ((gdk_paintable_get_flags (paintable) & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
      g_signal_connect (paintable, "invalidate-contents",
                        G_CALLBACK (paintable_invalidated_cb), self);
  } else {
    g_ptr_array_add (self->paintables, NULL);
  }
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
awra_rich_label_clear_paintables (AwraRichLabel *self)
{
  g_return_if_fail (AWRA_IS_RICH_LABEL (self));
  drop_paintables (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

int
awra_rich_label_get_inline_size (AwraRichLabel *self)
{
  g_return_val_if_fail (AWRA_IS_RICH_LABEL (self), 0);
  return self->inline_size;
}

void
awra_rich_label_set_inline_size (AwraRichLabel *self,
                                 int            size)
{
  g_return_if_fail (AWRA_IS_RICH_LABEL (self));
  g_return_if_fail (size >= 0 && size <= 512);
  if (self->inline_size == size)
    return;
  self->inline_size = size;
  update_shapes (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INLINE_SIZE]);
}
