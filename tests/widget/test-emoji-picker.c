/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static gboolean
quit_loop (gpointer user_data)
{
  g_main_loop_quit (user_data);
  return G_SOURCE_REMOVE;
}

static void
run (guint milliseconds)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add (milliseconds, quit_loop, loop);
  g_main_loop_run (loop);
}

static GListModel *
make_items (const char *const *names,
            const char *const *glyphs)
{
  GListStore *store = g_list_store_new (AWRA_TYPE_EMOJI_ITEM);

  for (guint i = 0; names[i] != NULL; i++) {
    g_autoptr (AwraEmojiItem) item = awra_emoji_item_new (names[i], names[i], glyphs[i]);

    g_list_store_append (store, item);
  }
  return G_LIST_MODEL (store);
}

static GListModel *
smileys (void)
{
  static const char *names[] = { "grinning", "joy", "wink", "thinking", "smile", "sob", NULL };
  static const char *glyphs[] = { "😀", "😂", "😉", "🤔", "😄", "😭", NULL };

  return make_items (names, glyphs);
}

static GListModel *
food (void)
{
  static const char *names[] = { "apple", "pear", "pizza", NULL };
  static const char *glyphs[] = { "🍎", "🍐", "🍕", NULL };

  return make_items (names, glyphs);
}

static guint
n_rows (AwraEmojiPicker *picker)
{
  GtkListView *grid = NULL;
  GtkWidget *widget = GTK_WIDGET (picker);

  /* The grid is the only list view inside. */
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL && grid == NULL;) {
    if (GTK_IS_LIST_VIEW (child)) {
      grid = GTK_LIST_VIEW (child);
      break;
    }
    if (gtk_widget_get_first_child (child) != NULL) {
      child = gtk_widget_get_first_child (child);
      continue;
    }
    while (child != NULL && gtk_widget_get_next_sibling (child) == NULL)
      child = gtk_widget_get_parent (child) == widget ? NULL : gtk_widget_get_parent (child);
    if (child != NULL)
      child = gtk_widget_get_next_sibling (child);
  }
  g_assert_nonnull (grid);
  return g_list_model_get_n_items (G_LIST_MODEL (gtk_list_view_get_model (grid)));
}

static gboolean
press (GtkWidget *widget,
       guint      keyval)
{
  g_autoptr (GListModel) controllers = gtk_widget_observe_controllers (widget);

  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
    g_autoptr (GtkEventController) controller = g_list_model_get_item (controllers, i);
    gboolean handled = FALSE;

    if (GTK_IS_EVENT_CONTROLLER_KEY (controller)) {
      g_signal_emit_by_name (controller, "key-pressed", keyval, 0, 0, &handled);
      return handled;
    }
  }
  g_assert_not_reached ();
}

static void
record_pick (AwraEmojiPicker  *picker,
             AwraEmojiItem    *item,
             char            **name)
{
  (void) picker;
  g_free (*name);
  *name = g_strdup (awra_emoji_item_get_name (item));
}

static void
test_item (void)
{
  g_autoptr (AwraEmojiItem) item = awra_emoji_item_new ("1f44d", "thumbs_up", "👍");
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (AwraEmojiItem) image = NULL;
  static const char *keywords[] = { "Approve", "yes", NULL };
  guchar pixel[4] = { 255, 0, 0, 255 };
  g_autoptr (GBytes) bytes = g_bytes_new (pixel, sizeof pixel);

  g_assert_cmpstr (awra_emoji_item_get_id (item), ==, "1f44d");
  g_assert_cmpstr (awra_emoji_item_get_name (item), ==, "thumbs_up");
  g_assert_cmpstr (awra_emoji_item_get_text (item), ==, "👍");
  g_assert_null (awra_emoji_item_get_paintable (item));
  g_assert_null (awra_emoji_item_get_keywords (item));
  awra_emoji_item_set_keywords (item, keywords);
  g_assert_cmpstr (awra_emoji_item_get_keywords (item)[0], ==, "Approve");
  g_assert_true (awra_emoji_item_matches (item, "THUMB"));
  g_assert_true (awra_emoji_item_matches (item, "approv"));
  g_assert_true (awra_emoji_item_matches (item, ""));
  g_assert_false (awra_emoji_item_matches (item, "heart"));
  awra_emoji_item_set_keywords (item, NULL);
  g_assert_false (awra_emoji_item_matches (item, "approve"));

  texture = gdk_memory_texture_new (1, 1, GDK_MEMORY_R8G8B8A8, bytes, 4);
  image = awra_emoji_item_new_for_paintable ("blob", "blob", GDK_PAINTABLE (texture));
  g_assert_true (awra_emoji_item_get_paintable (image) == GDK_PAINTABLE (texture));
  g_assert_null (awra_emoji_item_get_text (image));
}

static void
test_sections_and_search (void)
{
  GtkWidget *widget = g_object_ref_sink (awra_emoji_picker_new ());
  AwraEmojiPicker *picker = AWRA_EMOJI_PICKER (widget);
  g_autoptr (GListModel) faces = smileys ();
  g_autoptr (GListModel) meals = food ();
  g_autoptr (GListModel) recent = food ();

  g_assert_cmpuint (n_rows (picker), ==, 0);
  awra_emoji_picker_add_section (picker, "smileys", "Smileys", "emoji-people-symbolic", faces);
  awra_emoji_picker_add_section (picker, "food", "Food", NULL, meals);
  /* A picker with no size yet shows no rows, so its grid makes none. */
  g_assert_cmpuint (n_rows (picker), ==, 0);
  {
    int width = 0;
    int height = 0;

    gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &width, NULL, NULL);
    gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, width, NULL, &height, NULL, NULL);
    /* Nine columns at its natural width. */
    gtk_widget_size_allocate (widget, &(GtkAllocation) { 0, 0, width, height }, -1);
  }
  /* One row for each section. */
  g_assert_cmpuint (n_rows (picker), ==, 2);
  awra_emoji_picker_set_recent (picker, recent);
  g_assert_cmpuint (n_rows (picker), ==, 3);

  /* A search finds by name, across sections, once. */
  awra_emoji_picker_set_search_text (picker, "p");
  g_assert_cmpstr (awra_emoji_picker_get_search_text (picker), ==, "p");
  g_assert_cmpuint (n_rows (picker), ==, 1);
  awra_emoji_picker_set_search_text (picker, "nothing matches this");
  g_assert_cmpuint (n_rows (picker), ==, 0);
  awra_emoji_picker_set_search_text (picker, NULL);
  g_assert_cmpuint (n_rows (picker), ==, 3);

  /* Sections follow their model. */
  g_list_store_remove_all (G_LIST_STORE (meals));
  g_assert_cmpuint (n_rows (picker), ==, 2);

  /* Adding a section again replaces it. */
  awra_emoji_picker_add_section (picker, "smileys", "Faces", NULL, recent);
  g_assert_cmpuint (n_rows (picker), ==, 2);
  awra_emoji_picker_remove_section (picker, "smileys");
  g_assert_cmpuint (n_rows (picker), ==, 1);
  awra_emoji_picker_remove_section (picker, "unknown");
  {
    g_autoptr (GListModel) more = food ();

    awra_emoji_picker_add_section (picker, "food", "Food", NULL, more);
  }
  awra_emoji_picker_clear (picker);
  /* The recent items stay. */
  g_assert_cmpuint (n_rows (picker), ==, 1);
  awra_emoji_picker_set_recent (picker, NULL);
  g_assert_cmpuint (n_rows (picker), ==, 0);
  g_object_unref (widget);
}

static void
test_keyboard_and_pick (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *widget = awra_emoji_picker_new ();
  AwraEmojiPicker *picker = AWRA_EMOJI_PICKER (widget);
  g_autoptr (GListModel) faces = smileys ();
  g_autoptr (GListModel) meals = food ();
  g_autofree char *picked = NULL;

  g_signal_connect (picker, "picked", G_CALLBACK (record_pick), &picked);
  awra_emoji_picker_add_section (picker, "smileys", "Smileys", NULL, faces);
  awra_emoji_picker_add_section (picker, "food", "Food", NULL, meals);
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 400);
  gtk_window_present (GTK_WINDOW (window));
  gtk_widget_grab_focus (widget);
  run (150);

  /* The cursor starts on the first item. */
  g_assert_true (press (widget, GDK_KEY_Return));
  g_assert_cmpstr (picked, ==, "grinning");
  g_assert_true (press (widget, GDK_KEY_Right));
  g_assert_true (press (widget, GDK_KEY_Return));
  g_assert_cmpstr (picked, ==, "joy");
  /* Five columns at this width: down to the next row, then to the next
   * section at the same column, or its last item. */
  g_assert_true (press (widget, GDK_KEY_Down));
  g_assert_true (press (widget, GDK_KEY_Return));
  g_assert_cmpstr (picked, ==, "sob");
  g_assert_true (press (widget, GDK_KEY_Down));
  g_assert_true (press (widget, GDK_KEY_Return));
  g_assert_cmpstr (picked, ==, "apple");
  g_assert_true (press (widget, GDK_KEY_Up));
  g_assert_true (press (widget, GDK_KEY_Left));
  g_assert_true (press (widget, GDK_KEY_Return));
  g_assert_cmpstr (picked, ==, "smile");
  g_assert_false (press (widget, GDK_KEY_a));

  /* Searching moves the cursor to the first match. */
  awra_emoji_picker_set_search_text (picker, "piz");
  g_assert_true (press (widget, GDK_KEY_Return));
  g_assert_cmpstr (picked, ==, "pizza");
  gtk_window_destroy (GTK_WINDOW (window));
  run (20);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/emoji-picker/item", test_item);
  g_test_add_func ("/emoji-picker/sections-and-search", test_sections_and_search);
  g_test_add_func ("/emoji-picker/keyboard-and-pick", test_keyboard_and_pick);
  return g_test_run ();
}
