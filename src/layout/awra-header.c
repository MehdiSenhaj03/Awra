/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-header.h>
#include <awra/awra-layout.h>
#include <awra/awra-surface.h>

#include "awra-config.h"
#ifdef AWRA_HAVE_WIN32
#include <gdk/win32/gdkwin32.h>
#include "platform/win32/awra-win32-private.h"
#endif

enum {
  /* A center search/segmented control should use the calm visual span of the
   * header when space permits, while retaining the child's real minimum in a
   * compact window. GtkCenterBox otherwise stops at the child's comparatively
   * small natural request even when it explicitly asks to expand. */
  HEADER_EXPANDING_CENTER_NATURAL_WIDTH = 568,
};

typedef struct {
  GtkWidget parent_instance;
  GtkWidget *child;
} AwraHeaderCenter;

typedef GtkWidgetClass AwraHeaderCenterClass;

#define AWRA_TYPE_HEADER_CENTER (awra_header_center_get_type ())
#define AWRA_HEADER_CENTER(obj) ((AwraHeaderCenter *) (obj))

G_DEFINE_TYPE (AwraHeaderCenter, awra_header_center, GTK_TYPE_WIDGET)

static void
awra_header_center_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  AwraHeaderCenter *self = AWRA_HEADER_CENTER (widget);

  if (self->child == NULL) {
    *minimum = *natural = 0;
    if (minimum_baseline != NULL)
      *minimum_baseline = -1;
    if (natural_baseline != NULL)
      *natural_baseline = -1;
    return;
  }

  gtk_widget_measure (self->child, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_hexpand (self->child))
    *natural = MAX (*natural, HEADER_EXPANDING_CENTER_NATURAL_WIDTH);
}

static void
awra_header_center_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  AwraHeaderCenter *self = AWRA_HEADER_CENTER (widget);

  if (self->child != NULL)
    gtk_widget_allocate (self->child, width, height, baseline, NULL);
}

static void
awra_header_center_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  AwraHeaderCenter *self = AWRA_HEADER_CENTER (widget);

  if (self->child != NULL)
    gtk_widget_snapshot_child (widget, self->child, snapshot);
}

static void
awra_header_center_dispose (GObject *object)
{
  AwraHeaderCenter *self = AWRA_HEADER_CENTER (object);

  if (self->child != NULL) {
    gtk_widget_unparent (self->child);
    self->child = NULL;
  }
  G_OBJECT_CLASS (awra_header_center_parent_class)->dispose (object);
}

static void
awra_header_center_class_init (AwraHeaderCenterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_header_center_dispose;
  widget_class->measure = awra_header_center_measure;
  widget_class->size_allocate = awra_header_center_size_allocate;
  widget_class->snapshot = awra_header_center_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awraheadercenter");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_PRESENTATION);
}

static void
awra_header_center_init (AwraHeaderCenter *self)
{
  /* GtkCenterBox keeps a non-expanding center at the actual visual centre.
   * The wrapped child may still opt into the larger natural span above; making
   * the wrapper itself expand would consume asymmetric leftover space and
   * visibly pull a search field toward the larger side cluster. */
  gtk_widget_set_hexpand (GTK_WIDGET (self), FALSE);
}

static GtkWidget *
awra_header_center_new (void)
{
  return g_object_new (AWRA_TYPE_HEADER_CENTER, NULL);
}

static void
awra_header_center_set_child (AwraHeaderCenter *self,
                              GtkWidget        *child)
{
  if (self->child == child)
    return;
  if (self->child != NULL)
    gtk_widget_unparent (self->child);
  self->child = child;
  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
}

typedef struct {
  GtkWidget parent_instance;
  GtkWidget *start;
  GtkWidget *center;
  GtkWidget *end;
  gboolean expand_center;
} AwraHeaderContent;

typedef GtkWidgetClass AwraHeaderContentClass;

#define AWRA_TYPE_HEADER_CONTENT (awra_header_content_get_type ())
#define AWRA_HEADER_CONTENT(obj) ((AwraHeaderContent *) (obj))

G_DEFINE_TYPE (AwraHeaderContent, awra_header_content, GTK_TYPE_WIDGET)

static void
measure_child (GtkWidget      *child,
               GtkOrientation  orientation,
               int             for_size,
               int            *minimum,
               int            *natural)
{
  if (child == NULL || !gtk_widget_should_layout (child)) {
    *minimum = *natural = 0;
    return;
  }
  gtk_widget_measure (child, orientation, for_size,
                      minimum, natural, NULL, NULL);
}

static void
awra_header_content_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             int             for_size,
                             int            *minimum,
                             int            *natural,
                             int            *minimum_baseline,
                             int            *natural_baseline)
{
  AwraHeaderContent *self = AWRA_HEADER_CONTENT (widget);
  int start_minimum;
  int start_natural;
  int center_minimum;
  int center_natural;
  int end_minimum;
  int end_natural;

  measure_child (self->start, orientation, for_size,
                 &start_minimum, &start_natural);
  measure_child (self->center, orientation, for_size,
                 &center_minimum, &center_natural);
  measure_child (self->end, orientation, for_size,
                 &end_minimum, &end_natural);

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    /* Natural width keeps symmetric side reservations so the centre stays
     * visually stable; the minimum does not, otherwise a wide end cluster
     * would force the whole window wider than the screen. */
    *minimum = start_minimum + center_minimum + end_minimum;
    *natural = self->expand_center ? start_natural + center_natural + end_natural
                                   : center_natural + 2 * MAX (start_natural, end_natural);
  } else {
    *minimum = MAX (center_minimum,
                    MAX (start_minimum, end_minimum));
    *natural = MAX (center_natural,
                    MAX (start_natural, end_natural));
  }
  if (minimum_baseline != NULL)
    *minimum_baseline = -1;
  if (natural_baseline != NULL)
    *natural_baseline = -1;
}

static void
allocate_child_at (GtkWidget *child,
                   int        x,
                   int        width,
                   int        height,
                   int        baseline)
{
  graphene_point_t offset;

  if (child == NULL || !gtk_widget_should_layout (child))
    return;
  offset = GRAPHENE_POINT_INIT ((float) x, 0.0f);
  gtk_widget_allocate (child, MAX (0, width), height, baseline,
                       gsk_transform_translate (NULL, &offset));
}

static void
awra_header_content_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  AwraHeaderContent *self = AWRA_HEADER_CONTENT (widget);
  int start_minimum;
  int start_natural;
  int center_minimum;
  int center_natural;
  int end_minimum;
  int end_natural;
  int available;
  int start_width;
  int end_width;
  int gap_start;
  int gap_end;
  int gap;
  int center_width;
  int center_x;
  gboolean rtl;

  measure_child (self->start, GTK_ORIENTATION_HORIZONTAL, height,
                 &start_minimum, &start_natural);
  measure_child (self->center, GTK_ORIENTATION_HORIZONTAL, height,
                 &center_minimum, &center_natural);
  measure_child (self->end, GTK_ORIENTATION_HORIZONTAL, height,
                 &end_minimum, &end_natural);

  /* Sides get their natural width when it fits beside the centre's minimum,
   * never less than their own minimum. The centre then takes its preferred
   * span inside the remaining gap, as close to the true centre as the sides
   * allow: perfectly centred when there is room, shifted (never overlapped)
   * when the window is narrow. */
  available = MAX (0, width - center_minimum);
  start_width = MAX (start_minimum,
                     MIN (start_natural, available - end_minimum));
  end_width = MAX (end_minimum,
                   MIN (end_natural, available - start_width));
  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  gap_start = rtl ? end_width : start_width;
  gap_end = width - (rtl ? start_width : end_width);
  gap = MAX (0, gap_end - gap_start);
  center_width = self->expand_center ? gap : MIN (center_natural, gap);
  center_width = MAX (center_width, MIN (center_minimum, gap));
  center_x = CLAMP ((width - center_width) / 2,
                    gap_start, MAX (gap_start, gap_end - center_width));

  allocate_child_at (self->start,
                     rtl ? width - start_width : 0,
                     start_width, height, baseline);
  allocate_child_at (self->center, center_x,
                     center_width, height, baseline);
  allocate_child_at (self->end,
                     rtl ? 0 : width - end_width,
                     end_width, height, baseline);
}

static void
awra_header_content_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  AwraHeaderContent *self = AWRA_HEADER_CONTENT (widget);

  if (self->start != NULL)
    gtk_widget_snapshot_child (widget, self->start, snapshot);
  if (self->center != NULL)
    gtk_widget_snapshot_child (widget, self->center, snapshot);
  if (self->end != NULL)
    gtk_widget_snapshot_child (widget, self->end, snapshot);
}

static void
awra_header_content_dispose (GObject *object)
{
  AwraHeaderContent *self = AWRA_HEADER_CONTENT (object);

  if (self->start != NULL)
    gtk_widget_unparent (self->start);
  if (self->center != NULL)
    gtk_widget_unparent (self->center);
  if (self->end != NULL)
    gtk_widget_unparent (self->end);
  self->start = self->center = self->end = NULL;
  G_OBJECT_CLASS (awra_header_content_parent_class)->dispose (object);
}

static void
awra_header_content_class_init (AwraHeaderContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_header_content_dispose;
  widget_class->measure = awra_header_content_measure;
  widget_class->size_allocate = awra_header_content_size_allocate;
  widget_class->snapshot = awra_header_content_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awraheadercontent");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_PRESENTATION);
}

static void
awra_header_content_init (AwraHeaderContent *self)
{
  (void) self;
}

static GtkWidget *
awra_header_content_new (GtkWidget *start,
                         GtkWidget *center,
                         GtkWidget *end)
{
  AwraHeaderContent *self = g_object_new (AWRA_TYPE_HEADER_CONTENT, NULL);

  self->start = start;
  self->center = center;
  self->end = end;
  gtk_widget_set_parent (start, GTK_WIDGET (self));
  gtk_widget_set_parent (center, GTK_WIDGET (self));
  gtk_widget_set_parent (end, GTK_WIDGET (self));
  return GTK_WIDGET (self);
}

struct _AwraHeader {
  GtkWidget parent_instance;
  AwraSurface *surface;
  GtkWindowHandle *handle;
  GtkWidget *root;
  AwraHeaderContent *content;
  AwraHeaderCenter *center_slot;
  GtkBox *start_box;
  GtkBox *end_box;
  GtkBox *titles;
  GtkLabel *title;
  GtkLabel *subtitle;
  GtkWidget *start_controls;
  GtkWidget *end_controls;
  GtkWidget *start_widget;
  GtkWidget *center_widget;
  GtkWidget *end_widget;
  gboolean show_window_controls;
  gboolean drag_enabled;
  gboolean blend_with_window;
  gboolean compact;
  guint max_width;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_START_WIDGET,
  PROP_CENTER_WIDGET,
  PROP_END_WIDGET,
  PROP_SHOW_WINDOW_CONTROLS,
  PROP_DRAG_ENABLED,
  PROP_MATERIAL,
  PROP_BLEND_WITH_WINDOW,
  PROP_COMPACT,
  PROP_EXPAND_CENTER,
  PROP_MAX_WIDTH,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraHeader, awra_header, GTK_TYPE_WIDGET)

static void
update_titles_visibility (AwraHeader *self)
{
  const char *title = gtk_label_get_text (self->title);
  const char *subtitle = gtk_label_get_text (self->subtitle);

  gtk_widget_set_visible (
    GTK_WIDGET (self->titles),
    (title != NULL && *title != '\0') ||
      (subtitle != NULL && *subtitle != '\0'));
}

static void
update_window_controls_visibility (AwraHeader *self)
{
  gtk_widget_set_visible (
    self->start_controls,
    self->show_window_controls &&
      !gtk_window_controls_get_empty (
        GTK_WINDOW_CONTROLS (self->start_controls)));
  gtk_widget_set_visible (
    self->end_controls,
    self->show_window_controls &&
      !gtk_window_controls_get_empty (
        GTK_WINDOW_CONTROLS (self->end_controls)));
}

static void
window_controls_empty_changed_cb (GtkWindowControls *controls,
                                  GParamSpec        *pspec,
                                  AwraHeader        *self)
{
  (void) controls;
  (void) pspec;
  update_window_controls_visibility (self);
}

static void
update_surface_grammar (AwraHeader *self)
{
  if (self->blend_with_window) {
    /* A blended header is part of the window canvas, not a second toolbar
     * plane. Keeping the toolbar role here made the resolver apply a distinct
     * tint even though AwraWindow supplied the exact root material. */
    awra_surface_set_role (self->surface, AWRA_SURFACE_ROLE_WINDOW);
    awra_surface_set_corner_mask (
      self->surface,
      AWRA_SURFACE_CORNER_TOP_START | AWRA_SURFACE_CORNER_TOP_END);
    awra_surface_set_edge_mask (
      self->surface,
      AWRA_SURFACE_EDGE_TOP | AWRA_SURFACE_EDGE_START | AWRA_SURFACE_EDGE_END);
#ifdef AWRA_HAVE_WIN32
    /* DWM rounds the window: its top corners must match, or the backdrop
     * shows between the two curves. */
    if (GDK_IS_WIN32_DISPLAY (gtk_widget_get_display (GTK_WIDGET (self))))
      awra_surface_set_radius (self->surface, awra_win32_window_corner_radius ());
#endif
  } else {
    awra_surface_set_role (self->surface, AWRA_SURFACE_ROLE_TOOLBAR);
    awra_surface_set_corner_mask (self->surface, AWRA_SURFACE_CORNER_ALL);
    awra_surface_set_edge_mask (self->surface, AWRA_SURFACE_EDGE_ALL);
    /* Back to the material radius when no longer blended. */
    awra_surface_set_radius (self->surface, -1.0);
  }
}

static void
rebuild_root (AwraHeader *self)
{
  GtkWidget *surface = GTK_WIDGET (self->surface);
  GtkWidget *next_root;

  if (self->root != NULL && gtk_widget_get_parent (self->root) == GTK_WIDGET (self))
    gtk_widget_unparent (self->root);

  if (self->drag_enabled) {
    if (gtk_widget_get_parent (surface) != GTK_WIDGET (self->handle))
      gtk_window_handle_set_child (self->handle, surface);
    next_root = GTK_WIDGET (self->handle);
  } else {
    if (gtk_widget_get_parent (surface) == GTK_WIDGET (self->handle))
      gtk_window_handle_set_child (self->handle, NULL);
    next_root = surface;
  }

  self->root = next_root;
  gtk_widget_set_parent (self->root, GTK_WIDGET (self));
}

static void
awra_header_dispose (GObject *object)
{
  AwraHeader *self = AWRA_HEADER (object);

  if (self->root != NULL && gtk_widget_get_parent (self->root) == GTK_WIDGET (self))
    gtk_widget_unparent (self->root);
  if (gtk_widget_get_parent (GTK_WIDGET (self->surface)) ==
      GTK_WIDGET (self->handle))
    gtk_window_handle_set_child (self->handle, NULL);
  g_clear_object (&self->surface);
  g_clear_object (&self->handle);
  self->root = NULL;
  self->content = NULL;
  self->center_slot = NULL;
  self->start_box = NULL;
  self->end_box = NULL;
  self->titles = NULL;
  self->title = NULL;
  self->subtitle = NULL;
  self->start_controls = NULL;
  self->end_controls = NULL;
  self->start_widget = NULL;
  self->center_widget = NULL;
  self->end_widget = NULL;
  G_OBJECT_CLASS (awra_header_parent_class)->dispose (object);
}

static void
awra_header_expand (GtkWidget *widget, gboolean *horizontal, gboolean *vertical)
{
  AwraHeader *self = AWRA_HEADER (widget);
  *horizontal = self->root && gtk_widget_compute_expand (self->root, GTK_ORIENTATION_HORIZONTAL);
  *vertical = self->root && gtk_widget_compute_expand (self->root, GTK_ORIENTATION_VERTICAL);
}

static void
awra_header_measure (GtkWidget *widget, GtkOrientation orientation, int size,
                     int *minimum, int *natural, int *min_base, int *nat_base)
{
  AwraHeader *self = AWRA_HEADER (widget);
  if (orientation == GTK_ORIENTATION_VERTICAL && self->max_width && size >= 0) {
    int root_minimum;
    gtk_widget_measure (self->root, GTK_ORIENTATION_HORIZONTAL, -1,
                          &root_minimum, NULL, NULL, NULL);
    size = MIN (size, MAX (root_minimum, (int) self->max_width));
  }
  gtk_widget_measure (self->root, orientation, size, minimum, natural, min_base, nat_base);
  if (orientation == GTK_ORIENTATION_HORIZONTAL && self->max_width)
    *natural = MAX (*minimum, MIN (*natural, (int) self->max_width));
}

static void
awra_header_allocate (GtkWidget *widget, int width, int height, int baseline)
{
  AwraHeader *self = AWRA_HEADER (widget);
  int minimum;
  gtk_widget_measure (self->root, GTK_ORIENTATION_HORIZONTAL, height, &minimum, NULL, NULL, NULL);
  int span = self->max_width ? MIN (width, MAX (minimum, (int) self->max_width)) : width;
  graphene_point_t origin = GRAPHENE_POINT_INIT ((width - span) / 2.0f, 0);
  gtk_widget_allocate (self->root, span, height, baseline,
                        gsk_transform_translate (NULL, &origin));
}

static gboolean
awra_header_contains (GtkWidget *widget, double x, double y)
{
  AwraHeader *self = AWRA_HEADER (widget);
  graphene_rect_t bounds;
  graphene_point_t point = GRAPHENE_POINT_INIT (x, y);
  return self->root && gtk_widget_compute_bounds (self->root, widget, &bounds) &&
         graphene_rect_contains_point (&bounds, &point);
}

static void
awra_header_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  AwraHeader *self = AWRA_HEADER (widget);

  if (self->root != NULL)
    gtk_widget_snapshot_child (widget, self->root, snapshot);
}

static void
awra_header_get_property (GObject    *object,
                          guint       id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  AwraHeader *self = AWRA_HEADER (object);

  switch (id) {
  case PROP_MAX_WIDTH:
    g_value_set_uint (value, self->max_width);
    break;
  case PROP_TITLE:
    g_value_set_string (value, awra_header_get_title (self));
    break;
  case PROP_SUBTITLE:
    g_value_set_string (value, awra_header_get_subtitle (self));
    break;
  case PROP_START_WIDGET:
    g_value_set_object (value, self->start_widget);
    break;
  case PROP_CENTER_WIDGET:
    g_value_set_object (value, self->center_widget);
    break;
  case PROP_END_WIDGET:
    g_value_set_object (value, self->end_widget);
    break;
  case PROP_SHOW_WINDOW_CONTROLS:
    g_value_set_boolean (value, self->show_window_controls);
    break;
  case PROP_DRAG_ENABLED:
    g_value_set_boolean (value, self->drag_enabled);
    break;
  case PROP_MATERIAL:
    g_value_set_object (value, awra_header_get_material (self));
    break;
  case PROP_EXPAND_CENTER: g_value_set_boolean (value, self->content->expand_center); break;
  case PROP_COMPACT: g_value_set_boolean (value, self->compact); break;
  case PROP_BLEND_WITH_WINDOW:
    g_value_set_boolean (value, self->blend_with_window);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_header_set_property (GObject      *object,
                          guint         id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  AwraHeader *self = AWRA_HEADER (object);

  switch (id) {
  case PROP_MAX_WIDTH:
    awra_header_set_max_width (self, g_value_get_uint (value));
    break;
  case PROP_TITLE:
    awra_header_set_title (self, g_value_get_string (value));
    break;
  case PROP_SUBTITLE:
    awra_header_set_subtitle (self, g_value_get_string (value));
    break;
  case PROP_START_WIDGET:
    awra_header_set_start_widget (self, g_value_get_object (value));
    break;
  case PROP_CENTER_WIDGET:
    awra_header_set_center_widget (self, g_value_get_object (value));
    break;
  case PROP_END_WIDGET:
    awra_header_set_end_widget (self, g_value_get_object (value));
    break;
  case PROP_SHOW_WINDOW_CONTROLS:
    awra_header_set_show_window_controls (self, g_value_get_boolean (value));
    break;
  case PROP_DRAG_ENABLED:
    awra_header_set_drag_enabled (self, g_value_get_boolean (value));
    break;
  case PROP_MATERIAL:
    awra_header_set_material (self, g_value_get_object (value));
    break;
  case PROP_EXPAND_CENTER: awra_header_set_expand_center (self, g_value_get_boolean (value)); break;
  case PROP_COMPACT: awra_header_set_compact (self, g_value_get_boolean (value)); break;
  case PROP_BLEND_WITH_WINDOW:
    awra_header_set_blend_with_window (self, g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_header_class_init (AwraHeaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_header_dispose;
  object_class->get_property = awra_header_get_property;
  object_class->set_property = awra_header_set_property;
  widget_class->snapshot = awra_header_snapshot;
  widget_class->compute_expand = awra_header_expand;
  widget_class->measure = awra_header_measure;
  widget_class->size_allocate = awra_header_allocate;
  widget_class->contains = awra_header_contains;
  properties[PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SUBTITLE] = g_param_spec_string (
    "subtitle", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_START_WIDGET] = g_param_spec_object (
    "start-widget", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CENTER_WIDGET] = g_param_spec_object (
    "center-widget", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_END_WIDGET] = g_param_spec_object (
    "end-widget", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SHOW_WINDOW_CONTROLS] = g_param_spec_boolean (
    "show-window-controls", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_DRAG_ENABLED] = g_param_spec_boolean (
    "drag-enabled", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_MATERIAL] = g_param_spec_object (
    "material", NULL, NULL, AWRA_TYPE_MATERIAL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_BLEND_WITH_WINDOW] = g_param_spec_boolean (
    "blend-with-window", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_COMPACT] = g_param_spec_boolean (
    "compact", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_EXPAND_CENTER] = g_param_spec_boolean (
    "expand-center", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraHeader:max-width:
   *
   * Maximum centred visual width, respecting the content minimum. Zero is unbounded.
   * Since: 1.1
   */
  properties[PROP_MAX_WIDTH] = g_param_spec_uint (
    "max-width", NULL, "Maximum visual width; zero means unbounded", 0, 8192, 0,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awraheader");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BANNER);
}

static void
awra_header_init (AwraHeader *self)
{
  self->surface = g_object_ref_sink (
    AWRA_SURFACE (awra_surface_new_with_role (AWRA_SURFACE_ROLE_TOOLBAR)));
  self->handle = g_object_ref_sink (GTK_WINDOW_HANDLE (gtk_window_handle_new ()));
  self->center_slot = AWRA_HEADER_CENTER (awra_header_center_new ());
  self->start_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->end_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->titles = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 1));
  awra_box_set_spacing (self->start_box, AWRA_SPACING_MD);
  awra_box_set_spacing (self->end_box, AWRA_SPACING_SM);
  self->title = GTK_LABEL (gtk_label_new (NULL));
  self->subtitle = GTK_LABEL (gtk_label_new (NULL));
  self->start_controls = gtk_window_controls_new (GTK_PACK_START);
  self->end_controls = gtk_window_controls_new (GTK_PACK_END);
  /* The side boxes fill the header; caption buttons keep their own square
   * allocation instead of stretching to the full height of the chrome. */
  gtk_widget_set_valign (self->start_controls, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (self->end_controls, GTK_ALIGN_CENTER);

  g_signal_connect (self->start_controls, "notify::empty",
                    G_CALLBACK (window_controls_empty_changed_cb), self);
  g_signal_connect (self->end_controls, "notify::empty",
                    G_CALLBACK (window_controls_empty_changed_cb), self);

  self->content = AWRA_HEADER_CONTENT (awra_header_content_new (
    GTK_WIDGET (self->start_box),
    GTK_WIDGET (self->center_slot),
    GTK_WIDGET (self->end_box)));
  gtk_widget_add_css_class (GTK_WIDGET (self->content), "awra-header-content");
  gtk_widget_add_css_class (GTK_WIDGET (self->title), "awra-header-title");
  gtk_widget_add_css_class (GTK_WIDGET (self->subtitle), "awra-header-subtitle");
  gtk_widget_add_css_class (self->start_controls, "awra-header-controls");
  gtk_widget_add_css_class (self->end_controls, "awra-header-controls");
  gtk_label_set_xalign (self->title, 0.0f);
  gtk_label_set_xalign (self->subtitle, 0.0f);
  gtk_label_set_ellipsize (self->title, PANGO_ELLIPSIZE_END);
  gtk_label_set_ellipsize (self->subtitle, PANGO_ELLIPSIZE_END);
  /* The side boxes intentionally fill the header so branding and window
   * controls share one allocation.  The title stack itself must not fill that
   * height: a vertical GtkBox lays a lone label at its top edge when stretched,
   * which made compact dialog titles sit visibly above the close control. */
  gtk_widget_set_valign (GTK_WIDGET (self->titles), GTK_ALIGN_CENTER);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->titles), FALSE);
  gtk_widget_set_visible (self->start_controls, FALSE);
  gtk_widget_set_visible (self->end_controls, FALSE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->content), TRUE);

  gtk_box_append (self->start_box, self->start_controls);
  gtk_box_append (self->titles, GTK_WIDGET (self->title));
  gtk_box_append (self->titles, GTK_WIDGET (self->subtitle));
  gtk_box_append (self->start_box, GTK_WIDGET (self->titles));
  gtk_box_append (self->end_box, self->end_controls);
  awra_surface_set_child (self->surface, GTK_WIDGET (self->content));
  rebuild_root (self);
}

GtkWidget *
awra_header_new (void)
{
  return g_object_new (AWRA_TYPE_HEADER, NULL);
}

const char *
awra_header_get_title (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), NULL);
  return gtk_label_get_text (self->title);
}

void
awra_header_set_title (AwraHeader *self,
                       const char *title)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  if (g_strcmp0 (awra_header_get_title (self), title) == 0)
    return;
  gtk_label_set_text (self->title, title != NULL ? title : "");
  update_titles_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

const char *
awra_header_get_subtitle (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), NULL);
  return gtk_label_get_text (self->subtitle);
}

void
awra_header_set_subtitle (AwraHeader *self,
                          const char *subtitle)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  if (g_strcmp0 (awra_header_get_subtitle (self), subtitle) == 0)
    return;
  gtk_label_set_text (self->subtitle, subtitle != NULL ? subtitle : "");
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle),
                          subtitle != NULL && *subtitle != '\0');
  update_titles_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

GtkWidget *
awra_header_get_start_widget (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), NULL);
  return self->start_widget;
}

void
awra_header_set_start_widget (AwraHeader *self,
                              GtkWidget  *widget)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  if (self->start_widget == widget)
    return;
  if (self->start_widget != NULL)
    gtk_box_remove (self->start_box, self->start_widget);
  self->start_widget = widget;
  if (widget != NULL) {
    gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
    gtk_box_insert_child_after (self->start_box, widget, self->start_controls);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_START_WIDGET]);
}

GtkWidget *
awra_header_get_center_widget (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), NULL);
  return self->center_widget;
}

void
awra_header_set_center_widget (AwraHeader *self,
                               GtkWidget  *widget)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  if (self->center_widget == widget)
    return;
  if (widget != NULL)
    gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  awra_header_center_set_child (self->center_slot, widget);
  self->center_widget = widget;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CENTER_WIDGET]);
}

GtkWidget *
awra_header_get_end_widget (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), NULL);
  return self->end_widget;
}

void
awra_header_set_end_widget (AwraHeader *self,
                            GtkWidget  *widget)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  if (self->end_widget == widget)
    return;
  if (self->end_widget != NULL)
    gtk_box_remove (self->end_box, self->end_widget);
  self->end_widget = widget;
  if (widget != NULL) {
    gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
    gtk_box_prepend (self->end_box, widget);
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_END_WIDGET]);
}

gboolean
awra_header_get_show_window_controls (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), FALSE);
  return self->show_window_controls;
}

void
awra_header_set_show_window_controls (AwraHeader *self,
                                      gboolean    show_controls)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  show_controls = !!show_controls;
  if (self->show_window_controls == show_controls)
    return;
  self->show_window_controls = show_controls;
  update_window_controls_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_SHOW_WINDOW_CONTROLS]);
}

gboolean
awra_header_get_drag_enabled (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), FALSE);
  return self->drag_enabled;
}

void
awra_header_set_drag_enabled (AwraHeader *self,
                              gboolean    drag_enabled)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  drag_enabled = !!drag_enabled;
  if (self->drag_enabled == drag_enabled)
    return;
  self->drag_enabled = drag_enabled;
  rebuild_root (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DRAG_ENABLED]);
}

AwraMaterial *
awra_header_get_material (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), NULL);
  return awra_surface_get_material (self->surface);
}

void
awra_header_set_material (AwraHeader   *self,
                          AwraMaterial *material)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  g_return_if_fail (AWRA_IS_MATERIAL (material));
  if (awra_header_get_material (self) == material)
    return;
  awra_surface_set_material (self->surface, material);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MATERIAL]);
}

gboolean
awra_header_get_blend_with_window (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), FALSE);
  return self->blend_with_window;
}

void
awra_header_set_blend_with_window (AwraHeader *self,
                                   gboolean    blend)
{
  g_autoptr (AwraMaterial) material = NULL;

  g_return_if_fail (AWRA_IS_HEADER (self));
  blend = !!blend;
  if (self->blend_with_window == blend)
    return;
  self->blend_with_window = blend;
  material = awra_material_new_for_preset (
    blend ? AWRA_MATERIAL_PRESET_CANVAS : AWRA_MATERIAL_PRESET_CHROME);
  awra_header_set_material (self, material);
  update_surface_grammar (self);
  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_BLEND_WITH_WINDOW]);
}

gboolean
awra_header_get_compact (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), FALSE);
  return self->compact;
}

void
awra_header_set_compact (AwraHeader *self, gboolean compact)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  compact = !!compact;
  if (self->compact == compact) return;
  self->compact = compact;
  if (compact) gtk_widget_add_css_class (GTK_WIDGET (self), "awra-compact");
  else gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-compact");
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPACT]);
}

gboolean awra_header_get_expand_center (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), FALSE);
  return self->content->expand_center;
}
void awra_header_set_expand_center (AwraHeader *self, gboolean expand)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  expand = !!expand;
  if (self->content->expand_center == expand) return;
  self->content->expand_center = expand;
  gtk_widget_queue_resize (GTK_WIDGET (self->content));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPAND_CENTER]);
}

AwraSurface *
awra_header_get_surface (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), NULL);
  return self->surface;
}

guint
awra_header_get_max_width (AwraHeader *self)
{
  g_return_val_if_fail (AWRA_IS_HEADER (self), 0);
  return self->max_width;
}

void
awra_header_set_max_width (AwraHeader *self, guint width)
{
  g_return_if_fail (AWRA_IS_HEADER (self));
  width = MIN (width, 8192);
  if (self->max_width == width)
    return;
  self->max_width = width;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_WIDTH]);
}
