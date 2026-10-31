/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static void
test_child_ownership (void)
{
  GtkWidget *bin = g_object_ref_sink (awra_transition_bin_new ());
  GtkWidget *child = gtk_label_new ("contenu");
  int minimum = 0;
  int label_minimum = 0;

  awra_transition_bin_set_child (AWRA_TRANSITION_BIN (bin), child);
  g_assert_true (awra_transition_bin_get_child (AWRA_TRANSITION_BIN (bin)) == child);
  g_assert_true (gtk_widget_get_parent (child) == bin);
  gtk_widget_measure (bin, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &label_minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, label_minimum);
  awra_transition_bin_set_child (AWRA_TRANSITION_BIN (bin), NULL);
  g_assert_null (awra_transition_bin_get_child (AWRA_TRANSITION_BIN (bin)));
  g_object_unref (bin);
}

static void
test_play_needs_a_mapped_child (void)
{
  GtkWidget *bin = g_object_ref_sink (awra_transition_bin_new ());

  /* Nothing is on screen: nothing to capture, and nothing may start. */
  awra_transition_bin_play (AWRA_TRANSITION_BIN (bin), 16, 0);
  awra_transition_bin_set_child (AWRA_TRANSITION_BIN (bin), gtk_label_new ("contenu"));
  awra_transition_bin_play (AWRA_TRANSITION_BIN (bin), 0, 0);
  g_object_unref (bin);
}

static gboolean
quit_loop (gpointer user_data)
{
  g_main_loop_quit (user_data);
  return G_SOURCE_REMOVE;
}

static void
test_transition_finishes (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *bin = awra_transition_bin_new ();
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  awra_transition_bin_set_child (AWRA_TRANSITION_BIN (bin), gtk_label_new ("avant"));
  awra_transition_bin_set_fixed_top (AWRA_TRANSITION_BIN (bin), 8);
  g_assert_cmpint (awra_transition_bin_get_fixed_top (AWRA_TRANSITION_BIN (bin)), ==, 8);
  gtk_window_set_child (GTK_WINDOW (window), bin);
  gtk_window_present (GTK_WINDOW (window));
  g_timeout_add (120, quit_loop, loop);
  g_main_loop_run (loop);
  awra_transition_bin_play (AWRA_TRANSITION_BIN (bin), 16, 0);
  /* Interrupting a transition restarts it from what is on screen. */
  awra_transition_bin_play (AWRA_TRANSITION_BIN (bin), -16, 0);
  g_timeout_add (500, quit_loop, loop);
  g_main_loop_run (loop);
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/transition-bin/child", test_child_ownership);
  g_test_add_func ("/transition-bin/unmapped", test_play_needs_a_mapped_child);
  g_test_add_func ("/transition-bin/finishes", test_transition_finishes);
  return g_test_run ();
}
