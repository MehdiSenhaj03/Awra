/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static gboolean
quit_loop (gpointer user_data)
{
  g_main_loop_quit (user_data);
  return G_SOURCE_REMOVE;
}

static void
run_for (guint ms)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add (ms, quit_loop, loop);
  g_main_loop_run (loop);
}

static void
render (GtkWidget *widget)
{
  g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();
  g_autoptr (GskRenderNode) node = NULL;

  GTK_WIDGET_GET_CLASS (widget)->snapshot (widget, snapshot);
  node = gtk_snapshot_to_node (snapshot);
}

static void
test_typing_indicator (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  AwraTypingIndicator *typing = AWRA_TYPING_INDICATOR (awra_typing_indicator_new ());
  GtkWidget *weak = GTK_WIDGET (typing);
  int hidden_height = 0;
  int shown_height = 0;
  AwraStyleManager *style = awra_context_get_style_manager (
    awra_context_get_for_display (gdk_display_get_default ()));

  g_object_add_weak_pointer (G_OBJECT (typing), (gpointer *) &weak);
  g_assert_null (awra_typing_indicator_get_label (typing));
  g_assert_false (awra_typing_indicator_get_active (typing));
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (typing)), ==,
                   GTK_ACCESSIBLE_ROLE_STATUS);
  awra_typing_indicator_set_label (typing, "Ada is typing…");
  g_assert_cmpstr (awra_typing_indicator_get_label (typing), ==, "Ada is typing…");
  gtk_widget_measure (GTK_WIDGET (typing), GTK_ORIENTATION_VERTICAL, -1, &hidden_height, NULL,
                      NULL, NULL);
  g_assert_cmpint (hidden_height, >, 0);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (typing));
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));
  run_for (100);
  awra_typing_indicator_set_active (typing, TRUE);
  run_for (300);
  render (GTK_WIDGET (typing));
  /* Shown or hidden, the height is the same: nothing below moves. */
  gtk_widget_measure (GTK_WIDGET (typing), GTK_ORIENTATION_VERTICAL, -1, &shown_height, NULL,
                      NULL, NULL);
  g_assert_cmpint (shown_height, ==, hidden_height);
  awra_style_manager_set_reduced_motion (style, TRUE);
  render (GTK_WIDGET (typing));
  awra_style_manager_set_reduced_motion (style, FALSE);
  run_for (50);
  awra_typing_indicator_set_active (typing, FALSE);
  run_for (40);
  awra_typing_indicator_set_active (typing, TRUE);
  awra_typing_indicator_set_label (typing, NULL);
  run_for (40);
  render (GTK_WIDGET (typing));
  gtk_window_destroy (GTK_WINDOW (window));
  run_for (30);
  g_assert_null (weak);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  awra_init ();
  g_test_add_func ("/typing-indicator/lifecycle", test_typing_indicator);
  return g_test_run ();
}
