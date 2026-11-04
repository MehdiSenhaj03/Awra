/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-layout.h>
#include <awra/awra-page.h>

typedef struct _AwraPageViewport      AwraPageViewport;
typedef struct _AwraPageViewportClass AwraPageViewportClass;

struct _AwraPageViewport {
  GtkWidget parent_instance;
  GtkWidget *child;
  guint max_width;
};

struct _AwraPageViewportClass {
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (AwraPageViewport, awra_page_viewport, GTK_TYPE_WIDGET)

static void
awra_page_viewport_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  AwraPageViewport *self = (AwraPageViewport *) widget;
  int child_for_size = for_size;

  if (self->child == NULL) {
    *minimum = 0;
    *natural = 0;
    if (minimum_baseline != NULL)
      *minimum_baseline = -1;
    if (natural_baseline != NULL)
      *natural_baseline = -1;
    return;
  }
  if (orientation == GTK_ORIENTATION_VERTICAL &&
      child_for_size >= 0 && self->max_width > 0)
    child_for_size = MIN (child_for_size, (int) self->max_width);
  gtk_widget_measure (self->child, orientation, child_for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
  if (orientation == GTK_ORIENTATION_HORIZONTAL && self->max_width > 0) {
    *minimum = MIN (*minimum, (int) self->max_width);
    *natural = MIN (*natural, (int) self->max_width);
  }
}

/* Without this, GTK takes a widget with no layout manager for one of
 * constant size and measures it once, at its natural width: wrapped text
 * then got the height of fewer lines than it takes, and the page couldn't
 * scroll to its end. */
static GtkSizeRequestMode
awra_page_viewport_get_request_mode (GtkWidget *widget)
{
  AwraPageViewport *self = (AwraPageViewport *) widget;

  return self->child != NULL ? gtk_widget_get_request_mode (self->child)
                             : GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
awra_page_viewport_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  AwraPageViewport *self = (AwraPageViewport *) widget;
  int child_width;
  graphene_point_t offset;
  GskTransform *transform;

  if (self->child == NULL)
    return;
  child_width = self->max_width > 0
                  ? MIN (width, (int) self->max_width)
                  : width;
  offset = GRAPHENE_POINT_INIT ((width - child_width) / 2.0f, 0.0f);
  transform = gsk_transform_translate (NULL, &offset);
  /* gtk_widget_allocate() consumes @transform. */
  gtk_widget_allocate (self->child, child_width, height, baseline, transform);
}

static void
awra_page_viewport_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  AwraPageViewport *self = (AwraPageViewport *) widget;

  if (self->child != NULL)
    gtk_widget_snapshot_child (widget, self->child, snapshot);
}

static void
awra_page_viewport_dispose (GObject *object)
{
  AwraPageViewport *self = (AwraPageViewport *) object;

  if (self->child != NULL) {
    gtk_widget_unparent (self->child);
    self->child = NULL;
  }
  G_OBJECT_CLASS (awra_page_viewport_parent_class)->dispose (object);
}

static void
awra_page_viewport_class_init (AwraPageViewportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_page_viewport_dispose;
  widget_class->measure = awra_page_viewport_measure;
  widget_class->get_request_mode = awra_page_viewport_get_request_mode;
  widget_class->size_allocate = awra_page_viewport_size_allocate;
  widget_class->snapshot = awra_page_viewport_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awrapageviewport");
}

static void
awra_page_viewport_init (AwraPageViewport *self)
{
  self->max_width = 960;
}

static GtkWidget *
awra_page_viewport_new (GtkWidget *child,
                        guint      max_width)
{
  AwraPageViewport *self = g_object_new (awra_page_viewport_get_type (), NULL);

  self->child = child;
  self->max_width = max_width;
  gtk_widget_set_parent (child, GTK_WIDGET (self));
  return GTK_WIDGET (self);
}

struct _AwraPage {
  GtkWidget parent_instance;
  GtkScrolledWindow *scroller;
  AwraPageViewport *viewport;
  GtkBox *frame;
  GtkWidget *child;
  guint max_content_width;
  AwraSpacing content_inset;
};

enum {
  PROP_0,
  PROP_CHILD,
  PROP_MAX_CONTENT_WIDTH,
  PROP_CONTENT_INSET,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraPage, awra_page, GTK_TYPE_WIDGET)

static gboolean
spacing_is_valid (AwraSpacing spacing)
{
  return spacing == AWRA_SPACING_NONE ||
         (spacing >= AWRA_SPACING_XS && spacing <= AWRA_SPACING_XXL);
}

static void
awra_page_dispose (GObject *object)
{
  AwraPage *self = AWRA_PAGE (object);

  if (self->scroller != NULL) {
    gtk_scrolled_window_set_child (self->scroller, NULL);
    gtk_widget_unparent (GTK_WIDGET (self->scroller));
    self->scroller = NULL;
    self->viewport = NULL;
    self->frame = NULL;
    self->child = NULL;
  }
  G_OBJECT_CLASS (awra_page_parent_class)->dispose (object);
}

static void
awra_page_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  AwraPage *self = AWRA_PAGE (widget);

  if (self->scroller != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->scroller), snapshot);
}

static void
awra_page_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  AwraPage *self = AWRA_PAGE (object);

  switch (property_id) {
  case PROP_CHILD:
    g_value_set_object (value, self->child);
    break;
  case PROP_MAX_CONTENT_WIDTH:
    g_value_set_uint (value, self->max_content_width);
    break;
  case PROP_CONTENT_INSET:
    g_value_set_enum (value, self->content_inset);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_page_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  AwraPage *self = AWRA_PAGE (object);

  switch (property_id) {
  case PROP_CHILD:
    awra_page_set_child (self, g_value_get_object (value));
    break;
  case PROP_MAX_CONTENT_WIDTH:
    awra_page_set_max_content_width (self, g_value_get_uint (value));
    break;
  case PROP_CONTENT_INSET:
    awra_page_set_content_inset (self, g_value_get_enum (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_page_class_init (AwraPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_page_dispose;
  object_class->get_property = awra_page_get_property;
  object_class->set_property = awra_page_set_property;
  widget_class->snapshot = awra_page_snapshot;
  properties[PROP_CHILD] = g_param_spec_object (
    "child", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_MAX_CONTENT_WIDTH] = g_param_spec_uint (
    "max-content-width", NULL, NULL, 0, 4096, 960,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CONTENT_INSET] = g_param_spec_enum (
    "content-inset", NULL, NULL, AWRA_TYPE_SPACING, AWRA_SPACING_XXL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrapage");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_MAIN);
}

static void
awra_page_init (AwraPage *self)
{
  GtkWidget *viewport;

  self->max_content_width = 960;
  self->content_inset = AWRA_SPACING_XXL;
  self->frame = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  awra_widget_set_margin (GTK_WIDGET (self->frame), self->content_inset);
  viewport = awra_page_viewport_new (GTK_WIDGET (self->frame),
                                     self->max_content_width);
  self->viewport = (AwraPageViewport *) viewport;
  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  gtk_scrolled_window_set_policy (self->scroller,
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (self->scroller, viewport);
  gtk_widget_set_parent (GTK_WIDGET (self->scroller), GTK_WIDGET (self));
}

GtkWidget *
awra_page_new (void)
{
  return g_object_new (AWRA_TYPE_PAGE, NULL);
}

GtkWidget *
awra_page_get_child (AwraPage *self)
{
  g_return_val_if_fail (AWRA_IS_PAGE (self), NULL);
  return self->child;
}

void
awra_page_set_child (AwraPage  *self,
                     GtkWidget *child)
{
  g_return_if_fail (AWRA_IS_PAGE (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));
  if (self->child == child)
    return;
  if (self->child != NULL)
    gtk_box_remove (self->frame, self->child);
  self->child = child;
  if (child != NULL)
    gtk_box_append (self->frame, child);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

guint
awra_page_get_max_content_width (AwraPage *self)
{
  g_return_val_if_fail (AWRA_IS_PAGE (self), 0);
  return self->max_content_width;
}

void
awra_page_set_max_content_width (AwraPage *self,
                                 guint     width)
{
  g_return_if_fail (AWRA_IS_PAGE (self));
  g_return_if_fail (width == 0 || width >= 320);
  if (self->max_content_width == width)
    return;
  self->max_content_width = width;
  self->viewport->max_width = width;
  gtk_widget_queue_resize (GTK_WIDGET (self->viewport));
  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_MAX_CONTENT_WIDTH]);
}

AwraSpacing
awra_page_get_content_inset (AwraPage *self)
{
  g_return_val_if_fail (AWRA_IS_PAGE (self), AWRA_SPACING_NONE);
  return self->content_inset;
}

void
awra_page_set_content_inset (AwraPage    *self,
                             AwraSpacing  inset)
{
  g_return_if_fail (AWRA_IS_PAGE (self));
  g_return_if_fail (spacing_is_valid (inset));
  if (self->content_inset == inset)
    return;
  self->content_inset = inset;
  awra_widget_set_margin (GTK_WIDGET (self->frame), inset);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTENT_INSET]);
}

void
awra_page_scroll_to_top (AwraPage *self)
{
  GtkAdjustment *adjustment;

  g_return_if_fail (AWRA_IS_PAGE (self));
  adjustment = gtk_scrolled_window_get_vadjustment (self->scroller);
  gtk_adjustment_set_value (adjustment, gtk_adjustment_get_lower (adjustment));
}
