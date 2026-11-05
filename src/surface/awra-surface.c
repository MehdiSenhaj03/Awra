/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-context.h>
#include <awra/awra-surface.h>
#include <awra/awra-window.h>
#include <awra/awra-layout.h>

#include "material/awra-material-resolver-private.h"

struct _AwraSurface {
  GtkWidget parent_instance;
  GtkWidget *child;
  GdkPaintable *backdrop;
  GtkWidget *backdrop_widget; /* weak */
  gulong backdrop_handler;
  AwraSurfaceRole role;
  AwraMaterial *material;
  double radius;
  AwraSurfaceCorner corner_mask;
  AwraSurfaceEdge edge_mask;
  guint elevation;
  gboolean clip_child;
  AwraStyleManager *style_manager;
  gulong style_handler;
};

enum {
  PROP_0,
  PROP_CHILD,
  PROP_ROLE,
  PROP_MATERIAL,
  PROP_RADIUS,
  PROP_CORNER_MASK,
  PROP_EDGE_MASK,
  PROP_ELEVATION,
  PROP_CLIP_CHILD,
  PROP_BACKDROP_WIDGET,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];
static void clear_backdrop (AwraSurface *self);

G_DEFINE_FINAL_TYPE (AwraSurface, awra_surface, GTK_TYPE_WIDGET)

static AwraMaterialPreset
default_preset_for_role (AwraSurfaceRole role)
{
  switch (role) {
  case AWRA_SURFACE_ROLE_WINDOW:
    return AWRA_MATERIAL_PRESET_CANVAS;
  case AWRA_SURFACE_ROLE_CONTENT:
    return AWRA_MATERIAL_PRESET_CONTENT;
  case AWRA_SURFACE_ROLE_SIDEBAR:
  case AWRA_SURFACE_ROLE_TOOLBAR:
    return AWRA_MATERIAL_PRESET_CHROME;
  case AWRA_SURFACE_ROLE_CARD:
    return AWRA_MATERIAL_PRESET_LAYER;
  case AWRA_SURFACE_ROLE_FLOATING:
  case AWRA_SURFACE_ROLE_POPOVER:
  case AWRA_SURFACE_ROLE_MENU:
  case AWRA_SURFACE_ROLE_DIALOG:
  case AWRA_SURFACE_ROLE_HUD:
    return AWRA_MATERIAL_PRESET_FLOATING;
  case AWRA_SURFACE_ROLE_TOOLTIP:
  default:
    return AWRA_MATERIAL_PRESET_OPAQUE;
  }
}

static GdkTexture *
get_grain_texture (void)
{
  static GdkTexture *texture;

  if (g_once_init_enter_pointer (&texture)) {
    const int side = 96;
    guchar *pixels = g_malloc ((gsize) side * side * 4);
    guint32 state = 0x61777261U;
    g_autoptr (GBytes) bytes = NULL;
    GdkTexture *created;

    for (int i = 0; i < side * side; i++) {
      guchar value;

      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      value = (guchar) (state & 0x3f);
      pixels[i * 4] = value;
      pixels[i * 4 + 1] = value;
      pixels[i * 4 + 2] = value;
      pixels[i * 4 + 3] = value;
    }

    bytes = g_bytes_new_take (pixels, (gsize) side * side * 4);
    created = gdk_memory_texture_new (side,
                                      side,
                                      GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                                      bytes,
                                      (gsize) side * 4);
    g_once_init_leave_pointer (&texture, created);
  }

  return texture;
}

static void
update_appearance_class (AwraSurface *self)
{
  gboolean dark;

  if (self->style_manager == NULL)
    return;
  dark = awra_token_set_get_dark (
    awra_style_manager_get_token_set (self->style_manager));
  gtk_widget_remove_css_class (GTK_WIDGET (self), dark ? "awra-light" : "awra-dark");
  gtk_widget_add_css_class (GTK_WIDGET (self), dark ? "awra-dark" : "awra-light");
  if (awra_style_manager_get_high_contrast (self->style_manager))
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-high-contrast");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-high-contrast");
  if (awra_style_manager_get_reduced_motion (self->style_manager))
    gtk_widget_add_css_class (GTK_WIDGET (self), "awra-reduced-motion");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "awra-reduced-motion");
}

static void
style_changed_cb (AwraStyleManager *manager,
                  GParamSpec       *pspec,
                  AwraSurface      *self)
{
  (void) manager;
  (void) pspec;
  update_appearance_class (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
awra_surface_map (GtkWidget *widget)
{
  AwraSurface *self = AWRA_SURFACE (widget);
  AwraContext *context;

  GTK_WIDGET_CLASS (awra_surface_parent_class)->map (widget);
  context = awra_context_get_for_display (gtk_widget_get_display (widget));
  self->style_manager = awra_context_get_style_manager (context);
  update_appearance_class (self);
  if (self->style_handler == 0) {
    self->style_handler = g_signal_connect (self->style_manager,
                                            "notify",
                                            G_CALLBACK (style_changed_cb),
                                            self);
  }
}

static void
awra_surface_unmap (GtkWidget *widget)
{
  AwraSurface *self = AWRA_SURFACE (widget);

  if (self->style_handler != 0) {
    g_signal_handler_disconnect (self->style_manager, self->style_handler);
    self->style_handler = 0;
    self->style_manager = NULL;
  }
  GTK_WIDGET_CLASS (awra_surface_parent_class)->unmap (widget);
}

/* Compositor blur only blurs what is behind the window. An overlay surface
 * (floating panel, menu, HUD…) drawn inside a window covers the window's own
 * content, which no compositor can blur: it must take the opaque fallback or
 * the content beneath stays sharp and unreadable through it. Popovers and
 * dialogs own a native surface and keep their blur. */
static gboolean
draws_over_window_content (AwraSurface *self)
{
  GtkNative *native;

  switch (self->role) {
  case AWRA_SURFACE_ROLE_FLOATING:
  case AWRA_SURFACE_ROLE_POPOVER:
  case AWRA_SURFACE_ROLE_MENU:
  case AWRA_SURFACE_ROLE_DIALOG:
  case AWRA_SURFACE_ROLE_TOOLTIP:
  case AWRA_SURFACE_ROLE_HUD:
    break;
  default:
    return FALSE;
  }

  native = gtk_widget_get_native (GTK_WIDGET (self));
  if (!GTK_IS_WINDOW (native))
    return FALSE;
  if (AWRA_IS_WINDOW (native) &&
      awra_window_get_root_surface (AWRA_WINDOW (native)) == self)
    return FALSE;
  return TRUE;
}

/* Only the outer frame participates in the window policy. Nested surfaces
 * and native popovers retain their own rounded geometry. */
static gboolean
is_window_frame (AwraSurface *self, AwraWindow *window)
{
  GtkWidget *widget = GTK_WIDGET (self);
  if (gtk_widget_get_native (widget) != GTK_NATIVE (window))
    return FALSE;
  if (self == awra_window_get_root_surface (window))
    return TRUE;
  if (awra_window_get_overlay_chrome (window))
    return FALSE;
  GtkWidget *chrome = awra_window_get_chrome (window);
  if (widget == chrome)
    return TRUE;
  if (chrome == NULL || !gtk_widget_is_ancestor (widget, chrome))
    return FALSE;
  for (GtkWidget *parent = gtk_widget_get_parent (widget); parent != chrome;
       parent = gtk_widget_get_parent (parent))
    if (AWRA_IS_SURFACE (parent))
      return FALSE;
  return TRUE;
}

/* Only cache-safe independent subtrees may be sampled. In particular never
 * snapshot an ancestor: it contains this surface and would recurse. */
static GskRenderNode *
local_backdrop_node (AwraSurface *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *source = self->backdrop_widget;
  graphene_matrix_t transform;
  if (!source || !self->backdrop || source == widget || !gtk_widget_get_mapped (source) ||
      gtk_widget_is_ancestor (widget, source) ||
      gtk_widget_is_ancestor (source, widget) ||
      gtk_widget_get_native (source) != gtk_widget_get_native (widget) ||
      !gtk_widget_compute_transform (source, widget, &transform))
    return NULL;
  if (gtk_widget_get_width (source) <= 0 || gtk_widget_get_height (source) <= 0)
    return NULL;
  GtkSnapshot *capture = gtk_snapshot_new ();
  gtk_snapshot_transform_matrix (capture, &transform);
  double width = gtk_widget_get_width (source), height = gtk_widget_get_height (source);
  GdkPaintable *image = GTK_IS_PICTURE (source)
    ? gtk_picture_get_paintable (GTK_PICTURE (source)) : NULL;
  if (image) {
    /* Keep live textures as texture nodes instead of snapshotting their
     * enclosing canvas into another full-frame render target. */
    double aspect = gdk_paintable_get_intrinsic_aspect_ratio (image);
    double image_width = width, image_height = height;
    GtkContentFit fit = gtk_picture_get_content_fit (GTK_PICTURE (source));
    if (aspect > 0 && fit != GTK_CONTENT_FIT_FILL) {
      if ((width / height > aspect) != (fit == GTK_CONTENT_FIT_COVER))
        image_width = height * aspect;
      else
        image_height = width / aspect;
      if (fit == GTK_CONTENT_FIT_SCALE_DOWN) {
        int intrinsic = gdk_paintable_get_intrinsic_width (image);
        if (intrinsic > 0 && image_width > intrinsic) {
          image_width = intrinsic;
          image_height = intrinsic / aspect;
        }
      }
    }
    graphene_rect_t source_bounds = GRAPHENE_RECT_INIT (0, 0, width, height);
    if (fit == GTK_CONTENT_FIT_COVER)
      gtk_snapshot_push_clip (capture, &source_bounds);
    graphene_point_t origin = GRAPHENE_POINT_INIT ((width - image_width) / 2,
                                                   (height - image_height) / 2);
    gtk_snapshot_translate (capture, &origin);
    gdk_paintable_snapshot (image, GDK_SNAPSHOT (capture), image_width, image_height);
    if (fit == GTK_CONTENT_FIT_COVER)
      gtk_snapshot_pop (capture);
  } else {
    gdk_paintable_snapshot (GDK_PAINTABLE (self->backdrop), GDK_SNAPSHOT (capture), width, height);
  }
  return gtk_snapshot_free_to_node (capture);
}

static void
awra_surface_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  AwraSurface *self = AWRA_SURFACE (widget);
  AwraContext *context;
  AwraStyleManager *manager;
  AwraDiagnostics *diagnostics;
  AwraTokenSet *tokens;
  AwraResolvedMaterial resolved;
  GtkRoot *root;
  gboolean active = TRUE;
  gboolean blur_available;
  g_autoptr (GskRenderNode) backdrop = NULL;
  gboolean reduced_transparency;
  gboolean square_frame = FALSE;
  AwraSurfaceCorner corner_mask = self->corner_mask;
  float radius;
  graphene_rect_t bounds;
  GskRoundedRect outline;
  graphene_size_t rounded_corner;
  graphene_size_t square_corner = GRAPHENE_SIZE_INIT_ZERO;
  const graphene_size_t *top_left;
  const graphene_size_t *top_right;
  const graphene_size_t *bottom_right;
  const graphene_size_t *bottom_left;
  float widths[4];
  GdkRGBA colors[4];

  if (gtk_widget_get_width (widget) <= 0 || gtk_widget_get_height (widget) <= 0)
    return;

  context = awra_context_get_for_display (gtk_widget_get_display (widget));
  manager = awra_context_get_style_manager (context);
  diagnostics = awra_context_get_diagnostics (context);
  tokens = awra_style_manager_get_token_set (manager);
  root = gtk_widget_get_root (widget);
  if (GTK_IS_WINDOW (root))
    active = gtk_window_is_active (GTK_WINDOW (root));
  if (!awra_style_manager_get_reduced_transparency (manager) &&
      !awra_token_set_get_high_contrast (tokens) &&
      awra_material_requests_blur (self->material))
    backdrop = local_backdrop_node (self);
  blur_available = backdrop != NULL || (self->backdrop_widget == NULL && (awra_diagnostics_get_capabilities (diagnostics) &
                    AWRA_EFFECT_CAPABILITY_BLUR) != 0 &&
                   awra_style_manager_get_native_blur_enabled (manager) &&
                   !draws_over_window_content (self));
  reduced_transparency = awra_style_manager_get_reduced_transparency (manager);

  awra_material_resolve (self->material,
                         tokens,
                         self->role,
                         active,
                         blur_available,
                         reduced_transparency,
                         self->elevation,
                         &resolved);
  radius = self->radius >= 0.0 ? (float) self->radius : (float) resolved.radius;
  /* GTK hides a native titlebar in fullscreen. The root then owns the top
   * corners that the docked header normally supplies. Keep the stored mask
   * intact so restoring the window does not round the header/content seam. */
  if (AWRA_IS_WINDOW (root) &&
      self == awra_window_get_root_surface (AWRA_WINDOW (root)) &&
      gtk_window_is_fullscreen (GTK_WINDOW (root)) &&
      gtk_window_get_titlebar (GTK_WINDOW (root)) ==
          awra_window_get_chrome (AWRA_WINDOW (root)) &&
      corner_mask == (AWRA_SURFACE_CORNER_BOTTOM_START |
                      AWRA_SURFACE_CORNER_BOTTOM_END))
    corner_mask = AWRA_SURFACE_CORNER_ALL;
  if (AWRA_IS_WINDOW (root) &&
      !awra_window_get_keep_rounded_corners (AWRA_WINDOW (root)) &&
      (gtk_window_is_maximized (GTK_WINDOW (root)) ||
       gtk_window_is_fullscreen (GTK_WINDOW (root))) &&
      is_window_frame (self, AWRA_WINDOW (root)))
    {
      square_frame = TRUE;
      radius = 0;
      resolved.border_edges = AWRA_SURFACE_EDGE_NONE;
      resolved.highlight_edges = AWRA_SURFACE_EDGE_NONE;
    }
  resolved.border_edges &= self->edge_mask;
  resolved.highlight_edges &= self->edge_mask;
  bounds = GRAPHENE_RECT_INIT (0,
                               0,
                               gtk_widget_get_width (widget),
                               gtk_widget_get_height (widget));
  rounded_corner = (graphene_size_t) GRAPHENE_SIZE_INIT (radius, radius);
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
    top_left = (corner_mask & AWRA_SURFACE_CORNER_TOP_END) != 0
                 ? &rounded_corner : &square_corner;
    top_right = (corner_mask & AWRA_SURFACE_CORNER_TOP_START) != 0
                  ? &rounded_corner : &square_corner;
    bottom_right = (corner_mask & AWRA_SURFACE_CORNER_BOTTOM_START) != 0
                     ? &rounded_corner : &square_corner;
    bottom_left = (corner_mask & AWRA_SURFACE_CORNER_BOTTOM_END) != 0
                    ? &rounded_corner : &square_corner;
  } else {
    top_left = (corner_mask & AWRA_SURFACE_CORNER_TOP_START) != 0
                 ? &rounded_corner : &square_corner;
    top_right = (corner_mask & AWRA_SURFACE_CORNER_TOP_END) != 0
                  ? &rounded_corner : &square_corner;
    bottom_right = (corner_mask & AWRA_SURFACE_CORNER_BOTTOM_END) != 0
                     ? &rounded_corner : &square_corner;
    bottom_left = (corner_mask & AWRA_SURFACE_CORNER_BOTTOM_START) != 0
                    ? &rounded_corner : &square_corner;
  }
  gsk_rounded_rect_init (&outline, &bounds,
                         top_left, top_right, bottom_right, bottom_left);

  if (!square_frame && self->elevation > AWRA_ELEVATION_FLAT) {
    float offset = self->elevation == AWRA_ELEVATION_RAISED ? 1.5f
                   : self->elevation == AWRA_ELEVATION_FLOATING ? 7.0f
                   : 12.0f;
    float blur = self->elevation == AWRA_ELEVATION_RAISED ? 8.0f
                 : self->elevation == AWRA_ELEVATION_FLOATING ? 20.0f
                 : 32.0f;
    gtk_snapshot_append_outset_shadow (snapshot,
                                       &outline,
                                       &resolved.shadow,
                                       0.0,
                                       offset,
                                       0.0,
                                       blur);
  }

  gtk_snapshot_push_rounded_clip (snapshot, &outline);
  if (backdrop && resolved.request_blur) {
    float blur = awra_spacing_resolve (widget, AWRA_SPACING_XL);
    graphene_rect_t sample = bounds;
    graphene_rect_inset (&sample, -3 * blur, -3 * blur);
    gtk_snapshot_push_blur (snapshot, blur);
    gtk_snapshot_push_clip (snapshot, &sample);
    gtk_snapshot_append_node (snapshot, backdrop);
    gtk_snapshot_pop (snapshot);
    gtk_snapshot_pop (snapshot);
    /* A neutral scrim preserves secondary-text contrast without forcing a
     * per-frame colour-matrix conversion of a full-resolution YUV texture. */
    if (awra_token_set_get_foreground (tokens)->red > 0.5) {
      const GdkRGBA shade = { 0, 0, 0, 0.6 };
      gtk_snapshot_append_color (snapshot, &shade, &bounds);
    }
  }
  gtk_snapshot_append_color (snapshot, &resolved.fill, &bounds);

  if (resolved.grain_opacity > 0.0) {
    gtk_snapshot_push_opacity (snapshot, resolved.grain_opacity);
    gtk_snapshot_append_texture (snapshot, get_grain_texture (), &bounds);
    gtk_snapshot_pop (snapshot);
  }

  if (self->child != NULL) {
    if (self->clip_child)
      gtk_widget_snapshot_child (widget, self->child, snapshot);
    else {
      gtk_snapshot_pop (snapshot);
      gtk_widget_snapshot_child (widget, self->child, snapshot);
      gtk_snapshot_push_rounded_clip (snapshot, &outline);
    }
  }
  gtk_snapshot_pop (snapshot);

  for (guint i = 0; i < G_N_ELEMENTS (widths); i++) {
    widths[i] = 0.0f;
    colors[i] = resolved.border;
  }
  if ((resolved.border_edges & AWRA_SURFACE_EDGE_TOP) != 0)
    widths[0] = (float) resolved.border_width;
  if ((resolved.border_edges & AWRA_SURFACE_EDGE_BOTTOM) != 0)
    widths[2] = (float) resolved.border_width;
  if ((resolved.border_edges & AWRA_SURFACE_EDGE_START) != 0)
    widths[gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL ? 1 : 3] =
      (float) resolved.border_width;
  if ((resolved.border_edges & AWRA_SURFACE_EDGE_END) != 0)
    widths[gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL ? 3 : 1] =
      (float) resolved.border_width;
  if (resolved.border_edges != AWRA_SURFACE_EDGE_NONE)
    gtk_snapshot_append_border (snapshot, &outline, widths, colors);

  for (guint i = 0; i < G_N_ELEMENTS (widths); i++) {
    widths[i] = 0.0f;
    colors[i] = resolved.highlight;
  }
  if ((resolved.highlight_edges & AWRA_SURFACE_EDGE_TOP) != 0)
    widths[0] = (float) resolved.highlight_width;
  if ((resolved.highlight_edges & AWRA_SURFACE_EDGE_BOTTOM) != 0)
    widths[2] = (float) resolved.highlight_width;
  if ((resolved.highlight_edges & AWRA_SURFACE_EDGE_START) != 0)
    widths[gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL ? 1 : 3] =
      (float) resolved.highlight_width;
  if ((resolved.highlight_edges & AWRA_SURFACE_EDGE_END) != 0)
    widths[gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL ? 3 : 1] =
      (float) resolved.highlight_width;
  if (resolved.highlight_edges != AWRA_SURFACE_EDGE_NONE)
    gtk_snapshot_append_border (snapshot, &outline, widths, colors);

  if (gtk_widget_has_css_class (widget, "awra-effect-region-debug")) {
    for (guint i = 0; i < G_N_ELEMENTS (widths); i++) {
      widths[i] = 2.0f;
      colors[i] = (GdkRGBA) { 0.08, 0.95, 0.82, 0.95 };
    }
    gtk_snapshot_append_border (snapshot, &outline, widths, colors);
  }
  if (gtk_widget_has_css_class (widget, "awra-overdraw-debug")) {
    const GdkRGBA overdraw = { 0.95, 0.12, 0.58, 0.14 };
    gtk_snapshot_append_color (snapshot, &overdraw, &bounds);
  }
}

static void
awra_surface_dispose (GObject *object)
{
  AwraSurface *self = AWRA_SURFACE (object);

  if (self->style_handler != 0) {
    g_signal_handler_disconnect (self->style_manager, self->style_handler);
    self->style_handler = 0;
    self->style_manager = NULL;
  }
  if (self->child != NULL) {
    gtk_widget_unparent (self->child);
    self->child = NULL;
  }
  clear_backdrop (self);
  g_clear_object (&self->material);

  G_OBJECT_CLASS (awra_surface_parent_class)->dispose (object);
}

static void
awra_surface_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  AwraSurface *self = AWRA_SURFACE (object);

  switch (property_id) {
  case PROP_BACKDROP_WIDGET:
    g_value_set_object (value, awra_surface_get_backdrop_widget (self));
    break;
  case PROP_CHILD:
    g_value_set_object (value, self->child);
    break;
  case PROP_ROLE:
    g_value_set_enum (value, self->role);
    break;
  case PROP_MATERIAL:
    g_value_set_object (value, self->material);
    break;
  case PROP_RADIUS:
    g_value_set_double (value, self->radius);
    break;
  case PROP_CORNER_MASK:
    g_value_set_flags (value, self->corner_mask);
    break;
  case PROP_EDGE_MASK:
    g_value_set_flags (value, self->edge_mask);
    break;
  case PROP_ELEVATION:
    g_value_set_uint (value, self->elevation);
    break;
  case PROP_CLIP_CHILD:
    g_value_set_boolean (value, self->clip_child);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_surface_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  AwraSurface *self = AWRA_SURFACE (object);

  switch (property_id) {
  case PROP_BACKDROP_WIDGET:
    awra_surface_set_backdrop_widget (self, g_value_get_object (value));
    break;
  case PROP_CHILD:
    awra_surface_set_child (self, g_value_get_object (value));
    break;
  case PROP_ROLE:
    awra_surface_set_role (self, g_value_get_enum (value));
    break;
  case PROP_MATERIAL:
    awra_surface_set_material (self, g_value_get_object (value));
    break;
  case PROP_RADIUS:
    awra_surface_set_radius (self, g_value_get_double (value));
    break;
  case PROP_CORNER_MASK:
    awra_surface_set_corner_mask (self, g_value_get_flags (value));
    break;
  case PROP_EDGE_MASK:
    awra_surface_set_edge_mask (self, g_value_get_flags (value));
    break;
  case PROP_ELEVATION:
    awra_surface_set_elevation (self, g_value_get_uint (value));
    break;
  case PROP_CLIP_CHILD:
    awra_surface_set_clip_child (self, g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_surface_class_init (AwraSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_surface_dispose;
  object_class->get_property = awra_surface_get_property;
  object_class->set_property = awra_surface_set_property;
  widget_class->map = awra_surface_map;
  widget_class->unmap = awra_surface_unmap;
  widget_class->snapshot = awra_surface_snapshot;

  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ROLE] =
    g_param_spec_enum ("role", NULL, NULL, AWRA_TYPE_SURFACE_ROLE,
                       AWRA_SURFACE_ROLE_CONTENT,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_MATERIAL] =
    g_param_spec_object ("material", NULL, NULL, AWRA_TYPE_MATERIAL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_RADIUS] =
    g_param_spec_double ("radius", NULL, NULL, -1.0, 128.0, -1.0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CORNER_MASK] =
    g_param_spec_flags ("corner-mask", NULL, NULL, AWRA_TYPE_SURFACE_CORNER,
                        AWRA_SURFACE_CORNER_ALL,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS);
  properties[PROP_EDGE_MASK] =
    g_param_spec_flags ("edge-mask", NULL, NULL, AWRA_TYPE_SURFACE_EDGE,
                        AWRA_SURFACE_EDGE_ALL,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS);
  properties[PROP_ELEVATION] =
    g_param_spec_uint ("elevation", NULL, NULL, 0, 24, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_CLIP_CHILD] =
    g_param_spec_boolean ("clip-child", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * AwraSurface:backdrop-widget:
   *
   * Independent local source behind a frosted material, held weakly.
   * Since: 1.1
   */
  properties[PROP_BACKDROP_WIDGET] =
    g_param_spec_object ("backdrop-widget", NULL, "Local source behind a frosted material",
                         GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrasurface");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_surface_init (AwraSurface *self)
{
  self->role = AWRA_SURFACE_ROLE_CONTENT;
  self->material = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_CONTENT);
  self->radius = -1.0;
  self->corner_mask = AWRA_SURFACE_CORNER_ALL;
  self->edge_mask = AWRA_SURFACE_EDGE_ALL;
  self->clip_child = TRUE;
  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

GtkWidget *
awra_surface_new (void)
{
  return g_object_new (AWRA_TYPE_SURFACE, NULL);
}

GtkWidget *
awra_surface_new_with_role (AwraSurfaceRole role)
{
  AwraSurface *self;
  g_autoptr (AwraMaterial) material = NULL;

  g_return_val_if_fail (role >= AWRA_SURFACE_ROLE_WINDOW &&
                        role <= AWRA_SURFACE_ROLE_HUD, NULL);
  self = g_object_new (AWRA_TYPE_SURFACE, "role", role, NULL);
  material = awra_material_new_for_preset (default_preset_for_role (role));
  awra_surface_set_material (self, material);
  return GTK_WIDGET (self);
}

GtkWidget *
awra_surface_get_child (AwraSurface *self)
{
  g_return_val_if_fail (AWRA_IS_SURFACE (self), NULL);
  return self->child;
}

void
awra_surface_set_child (AwraSurface *self,
                        GtkWidget   *child)
{
  g_return_if_fail (AWRA_IS_SURFACE (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));
  g_return_if_fail (child == NULL || gtk_widget_get_parent (child) == NULL);

  if (self->child == child)
    return;

  if (self->child != NULL)
    gtk_widget_unparent (self->child);
  self->child = child;
  if (self->child != NULL)
    gtk_widget_set_parent (self->child, GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

AwraSurfaceRole
awra_surface_get_role (AwraSurface *self)
{
  g_return_val_if_fail (AWRA_IS_SURFACE (self), AWRA_SURFACE_ROLE_CONTENT);
  return self->role;
}

void
awra_surface_set_role (AwraSurface    *self,
                       AwraSurfaceRole role)
{
  g_return_if_fail (AWRA_IS_SURFACE (self));

  if (self->role == role)
    return;
  self->role = role;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ROLE]);
}

AwraMaterial *
awra_surface_get_material (AwraSurface *self)
{
  g_return_val_if_fail (AWRA_IS_SURFACE (self), NULL);
  return self->material;
}

void
awra_surface_set_material (AwraSurface  *self,
                           AwraMaterial *material)
{
  g_return_if_fail (AWRA_IS_SURFACE (self));
  g_return_if_fail (AWRA_IS_MATERIAL (material));

  if (g_set_object (&self->material, material)) {
    gtk_widget_queue_draw (GTK_WIDGET (self));
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MATERIAL]);
  }
}

#define DEFINE_SURFACE_ACCESSORS(type, name, field, prop, fallback) \
  type \
  awra_surface_get_##name (AwraSurface *self) \
  { \
    g_return_val_if_fail (AWRA_IS_SURFACE (self), fallback); \
    return self->field; \
  } \
  void \
  awra_surface_set_##name (AwraSurface *self, type value) \
  { \
    g_return_if_fail (AWRA_IS_SURFACE (self)); \
    if (self->field == value) \
      return; \
    self->field = value; \
    gtk_widget_queue_draw (GTK_WIDGET (self)); \
    g_object_notify_by_pspec (G_OBJECT (self), properties[prop]); \
  }

DEFINE_SURFACE_ACCESSORS (double, radius, radius, PROP_RADIUS, -1.0)
DEFINE_SURFACE_ACCESSORS (AwraSurfaceCorner, corner_mask, corner_mask,
                          PROP_CORNER_MASK, AWRA_SURFACE_CORNER_ALL)
DEFINE_SURFACE_ACCESSORS (AwraSurfaceEdge, edge_mask, edge_mask,
                          PROP_EDGE_MASK, AWRA_SURFACE_EDGE_ALL)
DEFINE_SURFACE_ACCESSORS (guint, elevation, elevation, PROP_ELEVATION, 0)
DEFINE_SURFACE_ACCESSORS (gboolean, clip_child, clip_child, PROP_CLIP_CHILD, TRUE)

AwraElevation
awra_surface_get_elevation_level (AwraSurface *self)
{
  g_return_val_if_fail (AWRA_IS_SURFACE (self), AWRA_ELEVATION_FLAT);
  return (AwraElevation) MIN (self->elevation, AWRA_ELEVATION_MODAL);
}

void
awra_surface_set_elevation_level (AwraSurface   *self,
                                  AwraElevation  elevation)
{
  g_return_if_fail (AWRA_IS_SURFACE (self));
  g_return_if_fail (elevation >= AWRA_ELEVATION_FLAT &&
                    elevation <= AWRA_ELEVATION_MODAL);
  awra_surface_set_elevation (self, (guint) elevation);
}

GtkWidget *
awra_surface_get_backdrop_widget (AwraSurface *self)
{
  g_return_val_if_fail (AWRA_IS_SURFACE (self), NULL);
  return self->backdrop_widget;
}

static void
backdrop_changed (GdkPaintable *paintable, AwraSurface *self)
{
  (void) paintable;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
clear_backdrop_paintable (AwraSurface *self)
{
  if (self->backdrop)
    g_signal_handlers_disconnect_by_data (self->backdrop, self);
  g_clear_object (&self->backdrop);
}

static void
update_backdrop_paintable (AwraSurface *self)
{
  clear_backdrop_paintable (self);
  if (GTK_IS_PICTURE (self->backdrop_widget)) {
    GdkPaintable *image = gtk_picture_get_paintable (GTK_PICTURE (self->backdrop_widget));
    if (image)
      self->backdrop = g_object_ref (image);
  } else if (self->backdrop_widget) {
    self->backdrop = gtk_widget_paintable_new (self->backdrop_widget);
  }
  if (self->backdrop) {
    g_signal_connect (self->backdrop, "invalidate-contents", G_CALLBACK (backdrop_changed), self);
    g_signal_connect (self->backdrop, "invalidate-size", G_CALLBACK (backdrop_changed), self);
  }
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
backdrop_source_gone (gpointer data, GObject *source)
{
  AwraSurface *self = data;
  (void) source;
  self->backdrop_widget = NULL;
  self->backdrop_handler = 0;
  clear_backdrop_paintable (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKDROP_WIDGET]);
}

static void
clear_backdrop (AwraSurface *self)
{
  if (self->backdrop_widget) {
    if (self->backdrop_handler)
      g_signal_handler_disconnect (self->backdrop_widget, self->backdrop_handler);
    g_object_weak_unref (G_OBJECT (self->backdrop_widget), backdrop_source_gone, self);
  }
  self->backdrop_widget = NULL;
  self->backdrop_handler = 0;
  clear_backdrop_paintable (self);
}

static void
backdrop_source_changed (GObject *source, GParamSpec *pspec, AwraSurface *self)
{
  (void) source;
  if (g_str_equal (pspec->name, "paintable"))
    update_backdrop_paintable (self);
  else if (g_str_equal (pspec->name, "content-fit"))
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
awra_surface_set_backdrop_widget (AwraSurface *self, GtkWidget *source)
{
  g_return_if_fail (AWRA_IS_SURFACE (self));
  g_return_if_fail (source == NULL || GTK_IS_WIDGET (source));
  g_return_if_fail (source != GTK_WIDGET (self));
  g_return_if_fail (source == NULL || !gtk_widget_is_ancestor (GTK_WIDGET (self), source));
  g_return_if_fail (source == NULL || !gtk_widget_is_ancestor (source, GTK_WIDGET (self)));
  if (self->backdrop_widget == source)
    return;
  clear_backdrop (self);
  self->backdrop_widget = source;
  if (source) {
    g_object_weak_ref (G_OBJECT (source), backdrop_source_gone, self);
    if (GTK_IS_PICTURE (source))
      self->backdrop_handler = g_signal_connect (source, "notify", G_CALLBACK (backdrop_source_changed), self);
    update_backdrop_paintable (self);
  }
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKDROP_WIDGET]);
}
