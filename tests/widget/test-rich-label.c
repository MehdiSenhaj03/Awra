/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static int
natural_width (GtkWidget *widget)
{
  int natural = 0;

  gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &natural, NULL, NULL);
  return natural;
}

static int
natural_height (GtkWidget *widget)
{
  int natural = 0;

  gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, NULL, &natural, NULL, NULL);
  return natural;
}

static GdkPaintable *
red_square (void)
{
  guchar pixels[4 * 4 * 4];
  g_autoptr (GBytes) bytes = NULL;

  for (guint i = 0; i < sizeof pixels; i += 4) {
    pixels[i] = 255;
    pixels[i + 1] = 0;
    pixels[i + 2] = 0;
    pixels[i + 3] = 255;
  }
  bytes = g_bytes_new (pixels, sizeof pixels);
  return GDK_PAINTABLE (gdk_memory_texture_new (4, 4, GDK_MEMORY_R8G8B8A8, bytes, 16));
}

static gboolean
has_texture (GskRenderNode *node)
{
  switch (gsk_render_node_get_node_type (node)) {
  case GSK_TEXTURE_NODE:
  case GSK_TEXTURE_SCALE_NODE:
    return TRUE;
  case GSK_CONTAINER_NODE:
    for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
      if (has_texture (gsk_container_node_get_child (node, i)))
        return TRUE;
    return FALSE;
  case GSK_TRANSFORM_NODE:
    return has_texture (gsk_transform_node_get_child (node));
  default:
    return FALSE;
  }
}

static void
test_pictures_take_room_and_draw (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *rich = awra_rich_label_new ();
  GtkLabel *label = awra_rich_label_get_label (AWRA_RICH_LABEL (rich));
  g_autoptr (GdkPaintable) square = red_square ();
  g_autoptr (GtkSnapshot) snapshot = NULL;
  g_autoptr (GskRenderNode) node = NULL;
  int plain;
  int with_picture;
  int small_height;

  gtk_window_set_child (GTK_WINDOW (window), rich);
  gtk_label_set_text (label, "ab");
  plain = natural_width (rich);
  gtk_label_set_text (label, "a\xef\xbf\xbc" "b");
  awra_rich_label_append_paintable (AWRA_RICH_LABEL (rich), square);
  awra_rich_label_set_inline_size (AWRA_RICH_LABEL (rich), 20);
  with_picture = natural_width (rich);
  /* The picture takes its own square on the line. */
  g_assert_cmpint (with_picture, >=, plain + 18);
  small_height = natural_height (rich);
  awra_rich_label_set_inline_size (AWRA_RICH_LABEL (rich), 48);
  g_assert_cmpint (natural_height (rich), >, small_height);

  gtk_window_present (GTK_WINDOW (window));
  while (g_main_context_iteration (NULL, FALSE))
    ;
  snapshot = gtk_snapshot_new ();
  GTK_WIDGET_GET_CLASS (rich)->snapshot (rich, snapshot);
  node = gtk_snapshot_to_node (snapshot);
  g_assert_nonnull (node);
  g_assert_true (has_texture (node));

  /* Forgetting the pictures keeps their room but draws nothing over it. */
  awra_rich_label_clear_paintables (AWRA_RICH_LABEL (rich));
  g_assert_cmpint (natural_width (rich), ==, with_picture + (48 - 20));
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv, NULL);
  awra_init ();
  g_test_add_func ("/rich-label/pictures", test_pictures_take_room_and_draw);
  return g_test_run ();
}
