/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static GtkWidget *
tile (const char *name)
{
  GtkWidget *child = gtk_label_new (name);

  gtk_widget_set_size_request (child, 100, 80);
  gtk_widget_set_name (child, name);
  return child;
}

static void
place (GtkWidget *view,
       int        width,
       int        height)
{
  int minimum = 0;

  gtk_widget_measure (view, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
  gtk_widget_measure (view, GTK_ORIENTATION_VERTICAL, -1, &minimum, NULL, NULL, NULL);
  gtk_widget_size_allocate (view, &(GtkAllocation) { 0, 0, width, height }, -1);
}

static char *
reading_order (GtkWidget *view)
{
  g_autoptr (GString) names = g_string_new (NULL);
  g_autoptr (GList) children = awra_tile_view_get_children (AWRA_TILE_VIEW (view));

  for (GList *link = children; link != NULL; link = link->next)
    g_string_append (names, gtk_widget_get_name (link->data));
  return g_strdup (names->str);
}

static void
assert_order (GtkWidget  *view,
              const char *expected)
{
  g_autofree char *order = reading_order (view);

  g_assert_cmpstr (order, ==, expected);
}

static void
test_division (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GtkWidget *c = tile ("c");
  GtkWidget *d = tile ("d");
  GtkWidget *e = tile ("e");
  int minimum = 0;

  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  awra_tile_view_insert (tiles, c, b, GTK_POS_BOTTOM);
  awra_tile_view_insert (tiles, d, a, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, e, NULL, GTK_POS_TOP);
  assert_order (view, "edabc");
  g_assert_true (gtk_widget_get_parent (c) == view);

  g_assert_true (awra_tile_view_touches_edge (tiles, d, GTK_POS_LEFT));
  g_assert_false (awra_tile_view_touches_edge (tiles, a, GTK_POS_LEFT));
  g_assert_true (awra_tile_view_touches_edge (tiles, e, GTK_POS_RIGHT));
  g_assert_true (awra_tile_view_touches_edge (tiles, e, GTK_POS_TOP));
  g_assert_false (awra_tile_view_touches_edge (tiles, a, GTK_POS_TOP));
  g_assert_true (awra_tile_view_touches_edge (tiles, c, GTK_POS_BOTTOM));
  g_assert_false (awra_tile_view_touches_edge (tiles, b, GTK_POS_BOTTOM));

  /* Three columns of 100, or two rows above 80 + 80, and their gaps. */
  gtk_widget_measure (view, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 302);
  gtk_widget_measure (view, GTK_ORIENTATION_VERTICAL, -1, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 80 + 1 + 80 + 1 + 80);
  g_object_unref (view);
}

static void
test_removal (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GtkWidget *c = tile ("c");
  GtkWidget *d = tile ("d");
  GtkWidget *last = g_object_ref (a);
  g_autoptr (GVariant) row = NULL;
  g_autoptr (GVariant) inner = NULL;
  g_autoptr (GVariant) kept = NULL;

  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  awra_tile_view_insert (tiles, c, b, GTK_POS_BOTTOM);
  awra_tile_view_insert (tiles, d, c, GTK_POS_RIGHT);

  /* a | (b / (c | d)) loses b: what is left is a single row of three. */
  awra_tile_view_remove (tiles, b);
  assert_order (view, "acd");
  row = g_variant_ref_sink (awra_tile_view_get_arrangement (tiles));
  inner = g_variant_get_variant (row);
  kept = g_variant_get_child_value (inner, 1);
  g_assert_cmpuint (g_variant_n_children (kept), ==, 3);
  place (view, 900, 300);
  {
    GdkRectangle bounds;

    g_assert_true (awra_tile_view_get_child_bounds (tiles, d, &bounds));
    g_assert_cmpint (bounds.height, ==, 300);
    g_assert_cmpint (bounds.x + bounds.width, ==, 900);
    g_assert_true (awra_tile_view_get_child_bounds (tiles, a, &bounds));
    g_assert_cmpint (bounds.width, >, 440);
  }

  awra_tile_view_remove (tiles, d);
  awra_tile_view_remove (tiles, c);
  assert_order (view, "a");
  awra_tile_view_remove (tiles, a);
  assert_order (view, "");
  g_assert_null (gtk_widget_get_parent (last));
  awra_tile_view_insert (tiles, last, NULL, GTK_POS_TOP);
  assert_order (view, "a");
  g_object_unref (last);
  g_object_unref (view);
}

static void
test_bounds (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GtkWidget *c = tile ("c");
  GdkRectangle bounds;
  GdkRectangle promised;

  g_assert_true (awra_tile_view_get_column_spacing (tiles) == 1);
  awra_tile_view_set_row_spacing (tiles, 9);
  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  g_assert_false (awra_tile_view_get_child_bounds (tiles, a, &bounds));
  place (view, 801, 600);
  g_assert_true (awra_tile_view_get_child_bounds (tiles, a, &bounds));
  g_assert_cmpint (bounds.x, ==, 0);
  g_assert_cmpint (bounds.width, ==, 400);
  g_assert_true (awra_tile_view_get_child_bounds (tiles, b, &bounds));
  g_assert_cmpint (bounds.x, ==, 401);
  g_assert_cmpint (bounds.width, ==, 400);

  /* What is promised for a tile is where it goes. */
  g_assert_true (awra_tile_view_compute_insertion (tiles, b, GTK_POS_BOTTOM, &promised));
  awra_tile_view_insert (tiles, c, b, GTK_POS_BOTTOM);
  place (view, 801, 600);
  g_assert_true (awra_tile_view_get_child_bounds (tiles, c, &bounds));
  g_assert_cmpint (bounds.x, ==, promised.x);
  g_assert_cmpint (bounds.y, ==, promised.y);
  g_assert_cmpint (bounds.width, ==, promised.width);
  g_assert_cmpint (bounds.height, ==, promised.height);
  g_assert_cmpint (bounds.y, ==, 305);

  /* Along a whole side, everyone makes room. */
  g_assert_true (awra_tile_view_compute_insertion (tiles, NULL, GTK_POS_RIGHT, &promised));
  g_assert_cmpint (promised.width, ==, 266);
  g_assert_cmpint (promised.x, ==, 535);
  g_assert_cmpint (promised.height, ==, 600);
  g_object_unref (view);
}

static void
test_refusal (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GdkRectangle bounds;

  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  g_assert_false (awra_tile_view_compute_insertion (tiles, a, GTK_POS_LEFT, &bounds));
  place (view, 300, 300);
  g_assert_true (awra_tile_view_compute_insertion (tiles, a, GTK_POS_RIGHT, &bounds));
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  place (view, 300, 300);
  /* 150 divided by two is below 100, but not 300 divided by two for a row. */
  g_assert_false (awra_tile_view_compute_insertion (tiles, a, GTK_POS_RIGHT, &bounds));
  g_assert_true (awra_tile_view_compute_insertion (tiles, a, GTK_POS_BOTTOM, &bounds));
  g_assert_false (awra_tile_view_compute_insertion (tiles, NULL, GTK_POS_RIGHT, &bounds));
  g_assert_true (awra_tile_view_compute_insertion (tiles, NULL, GTK_POS_TOP, &bounds));
  g_object_unref (view);
}

static void
test_minimums_win_over_shares (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  g_autoptr (GVariant) tight = NULL;
  GdkRectangle bounds;

  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  tight = g_variant_parse (NULL, "<(true, [0.99, 0.01], [<()>, <()>])>", NULL, NULL, NULL);
  g_assert_true (awra_tile_view_set_arrangement (tiles, tight));
  place (view, 500, 300);
  g_assert_true (awra_tile_view_get_child_bounds (tiles, b, &bounds));
  g_assert_cmpint (bounds.width, ==, 100);
  g_assert_cmpint (bounds.x, ==, 400);
  g_object_unref (view);
}

static void
test_arrangement (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  GtkWidget *copy = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  AwraTileView *other = AWRA_TILE_VIEW (copy);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GtkWidget *c = tile ("c");
  GtkWidget *x = tile ("x");
  GtkWidget *y = tile ("y");
  GtkWidget *z = tile ("z");
  g_autoptr (GVariant) kept = NULL;
  g_autoptr (GVariant) copied = NULL;
  g_autofree char *text = NULL;
  g_autofree char *copied_text = NULL;
  const char *refused[] = {
    "<()>",
    "<(true, [1.0], [<()>])>",
    "<(true, [0.5, 0.5], [<()>])>",
    "<(true, [0.5, -0.5, 1.0], [<()>, <()>, <()>])>",
    "<(true, [0.5, 0.0, 1.0], [<()>, <()>, <()>])>",
    "<(true, [0.5, 1.0, 1.0], [<()>, <()>, <1>])>",
    "<(true, [0.5, 0.5], [<()>, <(false, [0.5, 0.5], [<()>, <()>, <()>])>])>",
    "(true, [0.5, 0.5, 0.5], [<()>, <()>, <()>])",
    "<1>",
  };

  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  awra_tile_view_insert (tiles, c, b, GTK_POS_BOTTOM);
  awra_tile_view_insert (other, x, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (other, y, x, GTK_POS_RIGHT);
  awra_tile_view_insert (other, z, y, GTK_POS_RIGHT);

  kept = g_variant_ref_sink (awra_tile_view_get_arrangement (tiles));
  text = g_variant_print (kept, FALSE);
  g_assert_true (awra_tile_view_set_arrangement (other, kept));
  assert_order (copy, "xyz");
  copied = g_variant_ref_sink (awra_tile_view_get_arrangement (other));
  copied_text = g_variant_print (copied, FALSE);
  g_assert_cmpstr (copied_text, ==, text);
  g_assert_false (awra_tile_view_touches_edge (other, z, GTK_POS_TOP));
  g_assert_true (awra_tile_view_touches_edge (other, y, GTK_POS_TOP));

  for (guint i = 0; i < G_N_ELEMENTS (refused); i++)
    {
      g_autoptr (GVariant) bad = g_variant_parse (NULL, refused[i], NULL, NULL, NULL);

      g_assert_nonnull (bad);
      g_assert_false (awra_tile_view_set_arrangement (other, bad));
    }
  assert_order (copy, "xyz");
  g_assert_false (awra_tile_view_touches_edge (other, z, GTK_POS_TOP));

  /* Nothing to arrange: a view with a single tile has no shape to change. */
  awra_tile_view_remove (other, z);
  awra_tile_view_remove (other, y);
  {
    g_autoptr (GVariant) single = g_variant_ref_sink (awra_tile_view_get_arrangement (other));

    g_assert_true (awra_tile_view_set_arrangement (other, single));
  }
  g_object_unref (copy);
  g_object_unref (view);
}

static void
test_deep_arrangement_is_refused (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  g_autoptr (GString) text = g_string_new ("<()>");
  g_autoptr (GVariant) deep = NULL;

  for (guint i = 0; i < 40; i++)
    {
      awra_tile_view_insert (tiles, tile ("t"), NULL, GTK_POS_RIGHT);
      g_string_prepend (text, "<(true, [0.5, 0.5], [<()>, ");
      g_string_append (text, "])>");
    }
  awra_tile_view_insert (tiles, tile ("t"), NULL, GTK_POS_RIGHT);
  deep = g_variant_parse (NULL, text->str, NULL, NULL, NULL);
  g_assert_nonnull (deep);
  g_assert_false (awra_tile_view_set_arrangement (tiles, deep));
  g_object_unref (view);
}

static void
test_move (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GtkWidget *c = tile ("c");

  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  awra_tile_view_insert (tiles, c, b, GTK_POS_RIGHT);
  awra_tile_view_move (tiles, a, c, GTK_POS_RIGHT);
  assert_order (view, "bca");
  awra_tile_view_move (tiles, a, NULL, GTK_POS_BOTTOM);
  assert_order (view, "bca");
  g_assert_true (awra_tile_view_touches_edge (tiles, a, GTK_POS_LEFT));
  g_assert_false (awra_tile_view_touches_edge (tiles, b, GTK_POS_BOTTOM));
  awra_tile_view_move (tiles, c, a, GTK_POS_BOTTOM);
  assert_order (view, "bac");
  g_assert_true (gtk_widget_get_parent (a) == view);
  g_object_unref (view);
}

static void
test_equalize (void)
{
  GtkWidget *view = g_object_ref_sink (awra_tile_view_new ());
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GtkWidget *c = tile ("c");
  GdkRectangle bounds;

  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  awra_tile_view_insert (tiles, c, b, GTK_POS_RIGHT);
  awra_tile_view_equalize (tiles);
  place (view, 902, 300);
  g_assert_true (awra_tile_view_get_child_bounds (tiles, b, &bounds));
  g_assert_cmpint (bounds.x, ==, 301);
  g_assert_cmpint (bounds.width, ==, 300);
  g_object_unref (view);
}

static gboolean
quit_loop (gpointer user_data)
{
  g_main_loop_quit (user_data);
  return G_SOURCE_REMOVE;
}

static void
wait_for (GMainLoop *loop,
          guint      milliseconds)
{
  g_timeout_add (milliseconds, quit_loop, loop);
  g_main_loop_run (loop);
}

static void
test_motion_on_screen (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *view = awra_tile_view_new ();
  AwraTileView *tiles = AWRA_TILE_VIEW (view);
  GtkWidget *a = tile ("a");
  GtkWidget *b = tile ("b");
  GtkWidget *c = tile ("c");
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  GdkRectangle bounds;

  gtk_window_set_default_size (GTK_WINDOW (window), 900, 600);
  gtk_window_set_child (GTK_WINDOW (window), view);
  awra_tile_view_insert (tiles, a, NULL, GTK_POS_LEFT);
  gtk_window_present (GTK_WINDOW (window));
  wait_for (loop, 150);
  awra_tile_view_insert (tiles, b, a, GTK_POS_RIGHT);
  /* Interrupted changes start again from what is on screen. */
  wait_for (loop, 40);
  awra_tile_view_insert (tiles, c, b, GTK_POS_BOTTOM);
  wait_for (loop, 40);
  awra_tile_view_move (tiles, a, c, GTK_POS_RIGHT);
  wait_for (loop, 40);
  awra_tile_view_remove (tiles, b);
  wait_for (loop, 700);
  assert_order (view, "ca");
  g_assert_true (awra_tile_view_get_child_bounds (tiles, a, &bounds));
  g_assert_cmpint (gtk_widget_get_width (a), ==, bounds.width);
  g_assert_cmpint (gtk_widget_get_height (a), ==, bounds.height);

  /* The last tile can go in the middle of a change, and the window with it. */
  awra_tile_view_remove (tiles, a);
  awra_tile_view_remove (tiles, c);
  wait_for (loop, 40);
  awra_tile_view_insert (tiles, tile ("d"), NULL, GTK_POS_LEFT);
  awra_tile_view_insert (tiles, tile ("e"), NULL, GTK_POS_LEFT);
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/tile-view/division", test_division);
  g_test_add_func ("/tile-view/removal", test_removal);
  g_test_add_func ("/tile-view/bounds", test_bounds);
  g_test_add_func ("/tile-view/refusal", test_refusal);
  g_test_add_func ("/tile-view/minimums-win-over-shares", test_minimums_win_over_shares);
  g_test_add_func ("/tile-view/arrangement", test_arrangement);
  g_test_add_func ("/tile-view/deep-arrangement-is-refused", test_deep_arrangement_is_refused);
  g_test_add_func ("/tile-view/move", test_move);
  g_test_add_func ("/tile-view/equalize", test_equalize);
  g_test_add_func ("/tile-view/motion-on-screen", test_motion_on_screen);
  return g_test_run ();
}
