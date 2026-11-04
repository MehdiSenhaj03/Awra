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

static int
natural_width (GtkWidget *widget)
{
  int natural = 0;

  gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &natural, NULL, NULL);
  return natural;
}

static void
count_signal (GtkWidget *widget,
              guint     *count)
{
  (void) widget;
  (*count)++;
}

static void
test_chip_properties (void)
{
  AwraChip *chip = AWRA_CHIP (g_object_ref_sink (awra_chip_new ("Mentions")));
  g_autoptr (GdkPaintable) picture = gdk_paintable_new_empty (32, 16);
  int bare;
  int counted;

  g_assert_cmpstr (awra_chip_get_label (chip), ==, "Mentions");
  g_assert_null (awra_chip_get_icon_name (chip));
  g_assert_null (awra_chip_get_paintable (chip));
  g_assert_null (awra_chip_get_emoji (chip));
  g_assert_cmpint (awra_chip_get_count (chip), ==, -1);
  g_assert_false (awra_chip_get_active (chip));
  g_assert_true (awra_chip_get_toggleable (chip));
  g_assert_cmpint (awra_chip_get_appearance (chip), ==, AWRA_CHIP_APPEARANCE_NEUTRAL);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (chip)), ==,
                   GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON);
  bare = natural_width (GTK_WIDGET (chip));
  awra_chip_set_count (chip, 12);
  counted = natural_width (GTK_WIDGET (chip));
  g_assert_cmpint (counted, >, bare);
  awra_chip_set_icon_name (chip, "mail-unread-symbolic");
  g_assert_cmpint (natural_width (GTK_WIDGET (chip)), >, counted);
  awra_chip_set_emoji (chip, "🎉");
  awra_chip_set_paintable (chip, picture);
  awra_chip_set_appearance (chip, AWRA_CHIP_APPEARANCE_ACCENT);
  awra_chip_set_active (chip, TRUE);
  g_assert_cmpstr (awra_chip_get_emoji (chip), ==, "🎉");
  g_assert_true (awra_chip_get_paintable (chip) == picture);
  g_assert_true (awra_chip_get_active (chip));
  g_assert_true (gtk_widget_get_state_flags (GTK_WIDGET (chip)) & GTK_STATE_FLAG_CHECKED);
  g_object_set (chip, "label", NULL, "count", -1, "paintable", NULL, "emoji", NULL,
                "icon-name", NULL, NULL);
  /* Even empty, a chip stays at least round. */
  {
    int height = 0;

    gtk_widget_measure (GTK_WIDGET (chip), GTK_ORIENTATION_VERTICAL, -1, &height, NULL, NULL,
                        NULL);
    g_assert_cmpint (natural_width (GTK_WIDGET (chip)), >=, height);
  }
  g_object_unref (chip);
}

static void
test_chip_toggles (void)
{
  AwraChip *chip = AWRA_CHIP (g_object_ref_sink (awra_chip_new (NULL)));
  guint toggled = 0;
  guint activated = 0;

  awra_chip_set_emoji (chip, "👍");
  awra_chip_set_count (chip, 2);
  g_signal_connect (chip, "toggled", G_CALLBACK (count_signal), &toggled);
  g_signal_connect (chip, "activate", G_CALLBACK (count_signal), &activated);
  /* Changing the state from code is silent. */
  awra_chip_set_active (chip, TRUE);
  g_assert_cmpuint (toggled, ==, 0);
  gtk_widget_activate (GTK_WIDGET (chip));
  g_assert_false (awra_chip_get_active (chip));
  g_assert_cmpuint (toggled, ==, 1);
  g_assert_cmpuint (activated, ==, 1);
  awra_chip_set_toggleable (chip, FALSE);
  g_assert_false (awra_chip_get_toggleable (chip));
  gtk_widget_activate (GTK_WIDGET (chip));
  g_assert_false (awra_chip_get_active (chip));
  g_assert_cmpuint (toggled, ==, 1);
  g_assert_cmpuint (activated, ==, 2);
  g_object_unref (chip);
}

static void
test_chip_mapped (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  AwraChip *chip = AWRA_CHIP (awra_chip_new ("Votes"));
  GtkWidget *rtl = awra_chip_new ("RTL");
  GtkWidget *weak = GTK_WIDGET (chip);

  g_object_add_weak_pointer (G_OBJECT (chip), (gpointer *) &weak);
  awra_chip_set_count (chip, 19);
  awra_chip_set_count (AWRA_CHIP (rtl), 3);
  awra_chip_set_icon_name (AWRA_CHIP (rtl), "starred-symbolic");
  gtk_widget_set_direction (rtl, GTK_TEXT_DIR_RTL);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (chip));
  gtk_box_append (GTK_BOX (box), rtl);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));
  run_for (120);
  /* Digits roll, whole or after a shared prefix, up and down. */
  awra_chip_set_count (chip, 20);
  run_for (40);
  render (GTK_WIDGET (chip));
  awra_chip_set_count (chip, 21);
  run_for (40);
  render (GTK_WIDGET (chip));
  awra_chip_set_count (chip, 9);
  gtk_widget_activate (GTK_WIDGET (chip));
  render (GTK_WIDGET (chip));
  run_for (400);
  render (GTK_WIDGET (chip));
  render (rtl);
  g_assert_cmpint (awra_chip_get_count (chip), ==, 9);
  g_assert_true (awra_chip_get_active (chip));
  awra_chip_set_count (chip, 10);
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
  g_test_add_func ("/chip/properties", test_chip_properties);
  g_test_add_func ("/chip/toggles", test_chip_toggles);
  g_test_add_func ("/chip/mapped", test_chip_mapped);
  return g_test_run ();
}
