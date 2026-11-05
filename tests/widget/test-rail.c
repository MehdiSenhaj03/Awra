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

static AwraRailItem *
item_new (const char *label)
{
  GtkWidget *item = awra_rail_item_new (label);

  awra_rail_item_set_child (AWRA_RAIL_ITEM (item), awra_avatar_new (32, label));
  return AWRA_RAIL_ITEM (item);
}

static void
test_item_properties (void)
{
  AwraRailItem *item = AWRA_RAIL_ITEM (g_object_ref_sink (awra_rail_item_new ("Workshop")));
  GtkWidget *avatar = awra_avatar_new (24, "Workshop");
  int minimum = 0;
  int height = 0;

  g_assert_cmpstr (awra_rail_item_get_label (item), ==, "Workshop");
  g_assert_null (awra_rail_item_get_child (item));
  g_assert_false (awra_rail_item_get_selected (item));
  g_assert_false (awra_rail_item_get_unread (item));
  g_assert_false (awra_rail_item_get_muted (item));
  g_assert_cmpuint (awra_rail_item_get_mention_count (item), ==, 0);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (item)), ==,
                   GTK_ACCESSIBLE_ROLE_TAB);
  g_assert_true (gtk_widget_get_focusable (GTK_WIDGET (item)));
  awra_rail_item_set_child (item, avatar);
  g_assert_true (awra_rail_item_get_child (item) == avatar);
  g_assert_true (gtk_widget_get_parent (avatar) == GTK_WIDGET (item));
  gtk_widget_measure (GTK_WIDGET (item), GTK_ORIENTATION_VERTICAL, -1, &height, NULL, NULL, NULL);
  /* An avatar child takes the size and rounded shape of the tile. */
  g_assert_cmpint (awra_avatar_get_size (AWRA_AVATAR (avatar)), ==, height);
  g_assert_cmpint (awra_avatar_get_shape (AWRA_AVATAR (avatar)), ==, AWRA_AVATAR_SHAPE_ROUNDED);
  gtk_widget_measure (GTK_WIDGET (item), GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, >, height);
  awra_rail_item_set_icon_name (item, "list-add-symbolic");
  awra_rail_item_set_unread (item, TRUE);
  awra_rail_item_set_muted (item, TRUE);
  awra_rail_item_set_mention_count (item, 120);
  awra_rail_item_set_selected (item, TRUE);
  g_assert_cmpstr (awra_rail_item_get_icon_name (item), ==, "list-add-symbolic");
  g_assert_true (awra_rail_item_get_unread (item));
  g_assert_true (awra_rail_item_get_muted (item));
  g_assert_cmpuint (awra_rail_item_get_mention_count (item), ==, 120);
  g_assert_true (awra_rail_item_get_selected (item));
  g_assert_true (gtk_widget_get_state_flags (GTK_WIDGET (item)) & GTK_STATE_FLAG_SELECTED);
  g_object_ref (avatar);
  awra_rail_item_set_child (item, NULL);
  g_assert_null (gtk_widget_get_parent (avatar));
  g_object_unref (avatar);
  g_object_set (item, "label", "Atelier", "mention-count", 0, NULL);
  g_assert_cmpstr (awra_rail_item_get_label (item), ==, "Atelier");
  g_object_unref (item);
}

static void
test_entries (void)
{
  AwraRail *rail = AWRA_RAIL (g_object_ref_sink (awra_rail_new ()));
  AwraRailItem *first = item_new ("First");
  AwraRailItem *second = item_new ("Second");
  AwraRailItem *third = item_new ("Third");
  GtkWidget *group = awra_rail_group_new ("Folder");
  GtkWidget *separator;

  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (rail)), ==,
                   GTK_ACCESSIBLE_ROLE_TAB_LIST);
  awra_rail_append (rail, GTK_WIDGET (second));
  awra_rail_prepend (rail, GTK_WIDGET (first));
  separator = awra_rail_add_separator (rail);
  awra_rail_insert (rail, group, 2);
  awra_rail_insert (rail, GTK_WIDGET (third), -1);
  g_assert_cmpuint (awra_rail_get_n_entries (rail), ==, 5);
  g_assert_true (awra_rail_get_entry (rail, 0) == GTK_WIDGET (first));
  g_assert_true (awra_rail_get_entry (rail, 1) == GTK_WIDGET (second));
  g_assert_true (awra_rail_get_entry (rail, 2) == group);
  g_assert_true (awra_rail_get_entry (rail, 3) == separator);
  g_assert_true (awra_rail_get_entry (rail, 4) == GTK_WIDGET (third));
  g_assert_null (awra_rail_get_entry (rail, 5));
  awra_rail_remove (rail, separator);
  awra_rail_remove (rail, GTK_WIDGET (first));
  g_assert_cmpuint (awra_rail_get_n_entries (rail), ==, 3);
  g_assert_true (awra_rail_get_entry (rail, 0) == GTK_WIDGET (second));
  g_assert_false (awra_rail_get_reorderable (rail));
  awra_rail_set_reorderable (rail, TRUE);
  g_assert_true (awra_rail_get_reorderable (rail));
  g_object_unref (rail);
}

static void
test_single_selection (void)
{
  AwraRail *rail = AWRA_RAIL (g_object_ref_sink (awra_rail_new ()));
  AwraRailItem *home = item_new ("Home");
  AwraRailItem *add = item_new ("Add");
  AwraRailItem *first = item_new ("First");
  AwraRailItem *second = item_new ("Second");
  AwraRailItem *inner = item_new ("Inner");
  AwraRailGroup *group = AWRA_RAIL_GROUP (awra_rail_group_new ("Folder"));
  guint notifications = 0;

  awra_rail_set_header (rail, GTK_WIDGET (home));
  awra_rail_set_footer (rail, GTK_WIDGET (add));
  g_assert_true (awra_rail_get_header (rail) == GTK_WIDGET (home));
  g_assert_true (awra_rail_get_footer (rail) == GTK_WIDGET (add));
  awra_rail_append (rail, GTK_WIDGET (first));
  awra_rail_append (rail, GTK_WIDGET (second));
  awra_rail_group_append (group, inner);
  awra_rail_append (rail, GTK_WIDGET (group));
  g_signal_connect_swapped (rail, "notify::selected-item", G_CALLBACK (g_atomic_int_inc),
                            &notifications);
  g_assert_null (awra_rail_get_selected_item (rail));
  awra_rail_item_set_selected (first, TRUE);
  g_assert_true (awra_rail_get_selected_item (rail) == first);
  awra_rail_item_set_selected (second, TRUE);
  g_assert_true (awra_rail_get_selected_item (rail) == second);
  g_assert_false (awra_rail_item_get_selected (first));
  awra_rail_set_selected_item (rail, inner);
  g_assert_true (awra_rail_item_get_selected (inner));
  g_assert_false (awra_rail_item_get_selected (second));
  /* Activation selects, except for footer actions. */
  gtk_widget_activate (GTK_WIDGET (home));
  g_assert_true (awra_rail_get_selected_item (rail) == home);
  gtk_widget_activate (GTK_WIDGET (add));
  g_assert_true (awra_rail_get_selected_item (rail) == home);
  awra_rail_set_selected_item (rail, NULL);
  g_assert_null (awra_rail_get_selected_item (rail));
  g_assert_false (awra_rail_item_get_selected (home));
  /* Removing the selection clears it. */
  awra_rail_set_selected_item (rail, inner);
  awra_rail_group_remove (group, inner);
  g_assert_null (awra_rail_get_selected_item (rail));
  awra_rail_item_set_selected (first, TRUE);
  awra_rail_remove (rail, GTK_WIDGET (first));
  g_assert_null (awra_rail_get_selected_item (rail));
  g_assert_cmpuint (notifications, >=, 8);
  /* An entry that arrives selected becomes the selection. */
  {
    AwraRailItem *ready = item_new ("Ready");

    awra_rail_item_set_selected (ready, TRUE);
    awra_rail_append (rail, GTK_WIDGET (ready));
    g_assert_true (awra_rail_get_selected_item (rail) == ready);
  }
  awra_rail_set_header (rail, NULL);
  awra_rail_set_footer (rail, NULL);
  g_object_unref (rail);
}

static void
test_group (void)
{
  AwraRailGroup *group = AWRA_RAIL_GROUP (g_object_ref_sink (awra_rail_group_new ("Work")));
  AwraRailItem *items[5];
  AwraRailItem *header;
  GdkRGBA color = { 0.3, 0.5, 0.9, 1.0 };
  int collapsed = 0;

  g_assert_cmpstr (awra_rail_group_get_label (group), ==, "Work");
  g_assert_false (awra_rail_group_get_expanded (group));
  g_assert_null (awra_rail_group_get_color (group));
  for (guint i = 0; i < G_N_ELEMENTS (items); i++) {
    g_autofree char *label = g_strdup_printf ("Item %u", i);

    items[i] = item_new (label);
    awra_rail_group_insert (group, items[i], i == 0 ? -1 : 0);
  }
  g_assert_cmpuint (awra_rail_group_get_n_items (group), ==, 5);
  g_assert_true (awra_rail_group_get_item (group, 0) == items[4]);
  g_assert_true (awra_rail_group_get_item (group, 4) == items[0]);
  g_assert_null (awra_rail_group_get_item (group, 5));
  /* The group's own tile sums up its items while collapsed. */
  header = AWRA_RAIL_ITEM (gtk_widget_get_first_child (GTK_WIDGET (group)));
  awra_rail_item_set_mention_count (items[1], 2);
  awra_rail_item_set_mention_count (items[2], 3);
  awra_rail_item_set_unread (items[3], TRUE);
  g_assert_cmpuint (awra_rail_item_get_mention_count (header), ==, 5);
  g_assert_true (awra_rail_item_get_unread (header));
  awra_rail_item_set_muted (items[3], TRUE);
  g_assert_false (awra_rail_item_get_unread (header));
  awra_rail_item_set_selected (items[1], TRUE);
  g_assert_true (awra_rail_item_get_selected (header));
  gtk_widget_measure (GTK_WIDGET (group), GTK_ORIENTATION_VERTICAL, -1, &collapsed, NULL,
                      NULL, NULL);
  /* Activating the tile opens the folder; not mapped, it opens at once. */
  gtk_widget_activate (GTK_WIDGET (header));
  g_assert_true (awra_rail_group_get_expanded (group));
  g_assert_cmpuint (awra_rail_item_get_mention_count (header), ==, 0);
  g_assert_false (awra_rail_item_get_selected (header));
  {
    int expanded = 0;

    gtk_widget_measure (GTK_WIDGET (group), GTK_ORIENTATION_VERTICAL, -1, &expanded, NULL,
                        NULL, NULL);
    g_assert_cmpint (expanded, >, collapsed * 5);
  }
  awra_rail_group_set_color (group, &color);
  g_assert_true (gdk_rgba_equal (awra_rail_group_get_color (group), &color));
  awra_rail_group_set_icon_name (group, "folder-symbolic");
  g_assert_cmpstr (awra_rail_group_get_icon_name (group), ==, "folder-symbolic");
  awra_rail_group_remove (group, items[0]);
  g_assert_cmpuint (awra_rail_group_get_n_items (group), ==, 4);
  g_object_set (group, "expanded", FALSE, "color", NULL, NULL);
  g_assert_false (awra_rail_group_get_expanded (group));
  g_object_unref (group);
}

static void
test_mapped_lifecycle (void)
{
  GtkWidget *window = gtk_window_new ();
  AwraRail *rail = AWRA_RAIL (awra_rail_new ());
  AwraRailGroup *group = AWRA_RAIL_GROUP (awra_rail_group_new ("Folder"));
  AwraRailItem *item = item_new ("Item");
  GtkWidget *weak_rail = GTK_WIDGET (rail);
  GtkWidget *weak_item = GTK_WIDGET (item);

  g_object_add_weak_pointer (G_OBJECT (rail), (gpointer *) &weak_rail);
  g_object_add_weak_pointer (G_OBJECT (item), (gpointer *) &weak_item);
  for (guint i = 0; i < 3; i++) {
    g_autofree char *label = g_strdup_printf ("Inner %u", i);

    awra_rail_group_append (group, item_new (label));
  }
  awra_rail_append (rail, GTK_WIDGET (item));
  awra_rail_add_separator (rail);
  awra_rail_append (rail, GTK_WIDGET (group));
  for (guint i = 0; i < 20; i++) {
    g_autofree char *label = g_strdup_printf ("Filler %u", i);

    awra_rail_append (rail, GTK_WIDGET (item_new (label)));
  }
  awra_rail_set_reorderable (rail, TRUE);
  gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (rail));
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 300);
  gtk_window_present (GTK_WINDOW (window));
  run_for (150);
  /* Interrupt every animation half-way, then tear everything down. */
  awra_rail_group_set_expanded (group, TRUE);
  awra_rail_item_set_mention_count (item, 4);
  awra_rail_item_set_unread (item, TRUE);
  run_for (60);
  awra_rail_item_set_mention_count (item, 5);
  awra_rail_group_set_expanded (group, FALSE);
  awra_rail_set_selected_item (rail, awra_rail_group_get_item (group, 1));
  run_for (60);
  awra_rail_group_set_expanded (group, TRUE);
  run_for (500);
  g_assert_true (gtk_widget_get_mapped (GTK_WIDGET (awra_rail_group_get_item (group, 2))));
  /* An entry the application keeps outlives the rail safely. */
  g_object_ref (group);
  gtk_window_destroy (GTK_WINDOW (window));
  run_for (50);
  g_assert_null (weak_rail);
  g_assert_null (weak_item);
  g_assert_null (gtk_widget_get_parent (GTK_WIDGET (group)));
  awra_rail_group_set_expanded (group, FALSE);
  g_object_unref (group);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  awra_init ();
  g_test_add_func ("/rail/item-properties", test_item_properties);
  g_test_add_func ("/rail/entries", test_entries);
  g_test_add_func ("/rail/single-selection", test_single_selection);
  g_test_add_func ("/rail/group", test_group);
  g_test_add_func ("/rail/mapped-lifecycle", test_mapped_lifecycle);
  return g_test_run ();
}
