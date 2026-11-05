/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra.h>

static void
settle (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add_once (120, (GSourceOnceFunc)g_main_loop_quit, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}
static void
draw (GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data)
{
  (void)area;
  cairo_set_source_rgb (cr, data ? 0 : 1, data ? 1 : 0, 0);
  cairo_rectangle (cr, 0, 0, w / 2, h);
  cairo_fill (cr);
  cairo_set_source_rgb (cr, 0, 0, 1);
  cairo_rectangle (cr, w / 2, 0, w / 2, h);
  cairo_fill (cr);
}
static GBytes *
pixels (GtkWidget *widget, gsize *stride)
{
  g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();
  GTK_WIDGET_GET_CLASS (widget)->snapshot (widget, snapshot);
  g_autoptr (GskRenderNode) node = gtk_snapshot_to_node (snapshot);
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, gtk_widget_get_width (widget),
                                               gtk_widget_get_height (widget));
  GskRenderer *renderer = gtk_native_get_renderer (gtk_widget_get_native (widget));
  g_autoptr (GdkTexture) texture = gsk_renderer_render_texture (renderer, node, &bounds);
  g_autoptr (GdkTextureDownloader) dl = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (dl, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
  return gdk_texture_downloader_download_bytes (dl, stride);
}
static int
red_at (GBytes *bytes, gsize stride, int x)
{
  const guchar *data = g_bytes_get_data (bytes, NULL);
  return data[60 * stride + 4 * x];
}
static GdkTexture *
split_texture (void)
{
  guchar *data = g_malloc0 (640 * 360 * 4);
  for (int y = 0; y < 360; y++)
    for (int x = 0; x < 640; x++)
      {
        data[(y * 640 + x) * 4 + (x < 320 ? 0 : 2)] = 255;
        data[(y * 640 + x) * 4 + 3] = 255;
      }
  g_autoptr (GBytes) bytes = g_bytes_new_take (data, 640 * 360 * 4);
  return gdk_memory_texture_new (640, 360, GDK_MEMORY_R8G8B8A8, bytes, 640 * 4);
}
static void
test_backdrop (void)
{
  AwraStyleManager *style = awra_context_get_style_manager (
      awra_context_get_for_display (gdk_display_get_default ()));
  awra_style_manager_set_appearance (style, AWRA_APPEARANCE_DARK);
  GtkWidget *window = gtk_window_new (), *overlay = gtk_overlay_new ();
  GtkWidget *source = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (source), draw, NULL, NULL);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), source);
  GtkWidget *widget = awra_surface_new_with_role (AWRA_SURFACE_ROLE_FLOATING);
  AwraSurface *surface = AWRA_SURFACE (widget);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (widget, 320, 120);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), widget);
  awra_surface_set_backdrop_widget (surface, source);
  g_assert_true (awra_surface_get_backdrop_widget (surface) == source);
  gtk_window_set_child (GTK_WINDOW (window), overlay);
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 360);
  gtk_window_present (GTK_WINDOW (window));
  settle ();
  settle ();
  gsize stride;
  g_autoptr (GBytes) normal = pixels (widget, &stride);
  int spread = red_at (normal, stride, 80) - red_at (normal, stride, 240);
  int edge = red_at (normal, stride, 158) - red_at (normal, stride, 162);
  g_test_message ("backdrop red spread %d, edge %d", spread, edge);
  if (g_strcmp0 (g_getenv ("GSK_RENDERER"), "cairo") == 0 && spread == 0)
    {
      /* GTK's software renderer may not publish a cached widget image. The
       * documented fallback must stay opaque and readable in that case. */
      g_assert_cmpint (red_at (normal, stride, 80), >=, 30);
      g_assert_cmpint (red_at (normal, stride, 80), <=, 65);
      gtk_window_destroy (GTK_WINDOW (window));
      settle ();
      g_test_skip ("Cairo supplies the opaque fallback, not cached-source blur pixels");
      return;
    }
  g_assert_cmpint (spread, >, 25);
  g_assert_cmpint (ABS (edge), <, spread / 2);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (source), draw, GINT_TO_POINTER (1), NULL);
  gtk_widget_queue_draw (source);
  settle ();
  g_autoptr (GBytes) updated = pixels (widget, &stride);
  g_assert_cmpint (red_at (updated, stride, 80), <, red_at (normal, stride, 80) - 25);
  /* A GtkPicture uses its paintable directly, including content-fit geometry. */
  g_autoptr (GdkTexture) texture = split_texture ();
  GtkWidget *picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), picture);
  awra_surface_set_backdrop_widget (surface, picture);
  settle ();
  settle ();
  g_autoptr (GBytes) direct = pixels (widget, &stride);
  g_assert_cmpint (red_at (direct, stride, 80) - red_at (direct, stride, 240), >, 25);
  g_assert_cmpint (ABS (red_at (direct, stride, 158) - red_at (direct, stride, 162)), <, 20);
  /* White video is the worst case for dark secondary text contrast. */
  static const guchar white_pixel[] = { 255, 255, 255, 255 };
  g_autoptr (GBytes) white_bytes = g_bytes_new_static (white_pixel, 4);
  g_autoptr (GdkTexture) white_texture = gdk_memory_texture_new (
    1, 1, GDK_MEMORY_R8G8B8A8, white_bytes, 4);
  gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_FILL);
  gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (white_texture));
  settle ();
  g_autoptr (GBytes) white_backdrop = pixels (widget, &stride);
  g_test_message ("White backdrop luminance byte: %d", red_at (white_backdrop, stride, 80));
  g_assert_cmpint (red_at (white_backdrop, stride, 80), <=, 65);
  awra_style_manager_set_reduced_transparency (style, TRUE);
  settle ();
  g_autoptr (GBytes) reduced = pixels (widget, &stride);
  g_assert_cmpint (ABS (red_at (reduced, stride, 80) - red_at (reduced, stride, 240)), <, 5);
  awra_style_manager_set_reduced_transparency (style, FALSE);
  awra_style_manager_set_high_contrast (style, TRUE);
  settle ();
  g_autoptr (GBytes) contrast = pixels (widget, &stride);
  g_assert_cmpint (ABS (red_at (contrast, stride, 80) - red_at (contrast, stride, 240)), <, 5);
  awra_style_manager_set_high_contrast (style, FALSE);
  /* Replacing/removing the source and destroying a weakly held source. */
  awra_surface_set_backdrop_widget (surface, NULL);
  g_assert_null (awra_surface_get_backdrop_widget (surface));
  GtkWidget *other = g_object_ref_sink (gtk_drawing_area_new ());
  awra_surface_set_backdrop_widget (surface, other);
  g_object_unref (other);
  g_assert_null (awra_surface_get_backdrop_widget (surface));
  awra_surface_set_backdrop_widget (surface, picture);
  gtk_widget_set_direction (overlay, GTK_TEXT_DIR_RTL);
  settle ();
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}
static void
test_header (void)
{
  GtkWidget *window = gtk_window_new (), *overlay = gtk_overlay_new ();
  GtkWidget *source = gtk_drawing_area_new (), *header = awra_header_new ();
  gtk_overlay_set_child (GTK_OVERLAY (overlay), source);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), header);
  gtk_widget_set_valign (header, GTK_ALIGN_START);
  awra_header_set_title (AWRA_HEADER (header), "A long responsive media title");
  awra_header_set_show_window_controls (AWRA_HEADER (header), TRUE);
  awra_header_set_drag_enabled (AWRA_HEADER (header), TRUE);
  awra_header_set_max_width (AWRA_HEADER (header), 500);
  awra_surface_set_backdrop_widget (awra_header_get_surface (AWRA_HEADER (header)), source);
  gtk_window_set_child (GTK_WINDOW (window), overlay);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 400);
  gtk_window_present (GTK_WINDOW (window));
  settle ();
  g_assert_cmpint (
      gtk_widget_get_width (GTK_WIDGET (awra_header_get_surface (AWRA_HEADER (header)))), ==,
      500);
  g_assert_false (gtk_widget_contains (header, 20, 20));
  g_assert_true (gtk_widget_contains (header, 400, 20));
  gtk_widget_set_direction (header, GTK_TEXT_DIR_RTL);
  settle ();
  g_assert_false (gtk_widget_contains (header, 780, 20));
  awra_header_set_max_width (AWRA_HEADER (header), 0);
  settle ();
  g_assert_cmpint (
      gtk_widget_get_width (GTK_WIDGET (awra_header_get_surface (AWRA_HEADER (header)))), ==,
      gtk_widget_get_width (header));
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}
static void
test_builder (void)
{
  g_autoptr (GtkBuilder) builder
      = gtk_builder_new_from_string ("<interface><object class='GtkPicture' id='source'/>"
                                     "<object class='AwraSurface' id='surface'><property "
                                     "name='backdrop-widget'>source</property></object>"
                                     "<object class='AwraHeader' id='header'><property "
                                     "name='max-width'>1100</property></object></interface>",
                                     -1);
  AwraSurface *surface = AWRA_SURFACE (gtk_builder_get_object (builder, "surface"));
  AwraHeader *header = AWRA_HEADER (gtk_builder_get_object (builder, "header"));
  g_assert_true (awra_surface_get_backdrop_widget (surface)
                 == GTK_WIDGET (gtk_builder_get_object (builder, "source")));
  g_assert_cmpuint (awra_header_get_max_width (header), ==, 1100);
}
int
main (int argc, char **argv)
{
  gtk_init ();
  awra_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/awra/backdrop/pixels-lifecycle", test_backdrop);
  g_test_add_func ("/awra/backdrop/header-geometry", test_header);
  g_test_add_func ("/awra/backdrop/builder", test_builder);
  return g_test_run ();
}
