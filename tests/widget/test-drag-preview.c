/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static GdkPaintable *
square (void)
{
  static const guchar pixel[4] = { 0xff, 0x88, 0x00, 0xff };
  g_autoptr (GBytes) bytes = g_bytes_new_static (pixel, sizeof pixel);

  return GDK_PAINTABLE (gdk_memory_texture_new (1, 1, GDK_MEMORY_R8G8B8A8, bytes, 4));
}

static GtkWidget *
badge_of (GtkWidget *preview)
{
  return gtk_widget_get_first_child (preview);
}

static void
test_single_item (void)
{
  g_autoptr (GdkPaintable) item = square ();
  GtkWidget *preview = g_object_ref_sink (awra_drag_preview_new ());
  int minimum = 0;

  awra_drag_preview_set_item (AWRA_DRAG_PREVIEW (preview), 0, item);
  awra_drag_preview_set_count (AWRA_DRAG_PREVIEW (preview), 1);
  g_assert_false (gtk_widget_get_visible (badge_of (preview)));
  gtk_widget_measure (preview, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, >, awra_drag_preview_get_icon_size (AWRA_DRAG_PREVIEW (preview)));
  g_object_unref (preview);
}

static void
test_pile_counts_more_than_it_shows (void)
{
  g_autoptr (GdkPaintable) item = square ();
  GtkWidget *preview = g_object_ref_sink (awra_drag_preview_new ());
  AwraDragPreview *pile = AWRA_DRAG_PREVIEW (preview);

  for (guint i = 0; i < 3; i++)
    awra_drag_preview_set_item (pile, i, item);
  awra_drag_preview_set_count (pile, 42);
  g_assert_true (gtk_widget_get_visible (badge_of (preview)));
  g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (badge_of (preview))), ==, "42");
  awra_drag_preview_set_count (pile, 5000);
  g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (badge_of (preview))), ==, "999+");
  /* A thumbnail replaces an icon; clearing an item must let it go. */
  awra_drag_preview_set_item (pile, 1, NULL);
  awra_drag_preview_set_icon_size (pile, 96);
  g_assert_cmpint (awra_drag_preview_get_icon_size (pile), ==, 96);
  g_object_unref (preview);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/drag-preview/single", test_single_item);
  g_test_add_func ("/drag-preview/pile", test_pile_counts_more_than_it_shows);
  return g_test_run ();
}
