/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static const char row_ui[] =
  "<interface><template class=\"GtkListItem\"><property name=\"child\">"
  "<object class=\"GtkLabel\"><binding name=\"label\">"
  "<lookup name=\"string\" type=\"GtkStringObject\">"
  "<lookup name=\"item\">GtkListItem</lookup></lookup></binding>"
  "</object></property></template></interface>";

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

/* Delivers a key to every capture-phase key controller of @widget, in order,
 * until one takes it, as GTK does. */
static gboolean
press (GtkWidget *widget,
       guint      keyval)
{
  g_autoptr (GListModel) controllers = gtk_widget_observe_controllers (widget);

  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
    g_autoptr (GtkEventController) controller = g_list_model_get_item (controllers, i);
    gboolean handled = FALSE;

    if (!GTK_IS_EVENT_CONTROLLER_KEY (controller) ||
        gtk_event_controller_get_propagation_phase (controller) != GTK_PHASE_CAPTURE)
      continue;
    g_signal_emit_by_name (controller, "key-pressed", keyval, 0, 0, &handled);
    if (handled)
      return TRUE;
  }
  return FALSE;
}

static AwraCompletionPopover *
make_completion (GtkStringList **strings_out)
{
  static const char *names[] = { "ada", "alan", "annie", NULL };
  g_autoptr (GBytes) bytes = g_bytes_new_static (row_ui, sizeof row_ui - 1);
  g_autoptr (GtkListItemFactory) factory = gtk_builder_list_item_factory_new_from_bytes (NULL, bytes);
  GtkStringList *strings = gtk_string_list_new (names);
  AwraCompletionPopover *completion = AWRA_COMPLETION_POPOVER (awra_completion_popover_new ());

  awra_completion_popover_set_model (completion, G_LIST_MODEL (strings));
  awra_completion_popover_set_factory (completion, factory);
  *strings_out = strings;
  return completion;
}

static void
record_position (AwraCompletionPopover *completion,
                 guint                  position,
                 guint                 *out)
{
  (void) completion;
  *out = position;
}

static void
count (gpointer  instance,
       guint    *counter)
{
  (void) instance;
  (*counter)++;
}

static void
test_properties (void)
{
  GtkStringList *strings = NULL;
  AwraCompletionPopover *completion = make_completion (&strings);

  g_object_ref_sink (completion);
  g_assert_true (awra_completion_popover_get_model (completion) == G_LIST_MODEL (strings));
  g_assert_nonnull (awra_completion_popover_get_factory (completion));
  g_assert_null (awra_completion_popover_get_attached_widget (completion));
  /* A single selection that always has a suggestion chosen. */
  g_assert_cmpuint (awra_completion_popover_get_selected (completion), ==, 0);
  awra_completion_popover_set_selected (completion, 2);
  g_assert_cmpuint (awra_completion_popover_get_selected (completion), ==, 2);
  awra_completion_popover_set_model (completion, NULL);
  g_assert_cmpuint (awra_completion_popover_get_selected (completion), ==, GTK_INVALID_LIST_POSITION);
  g_object_unref (completion);
  g_object_unref (strings);
}

static void
test_keyboard (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *view = gtk_text_view_new ();
  GtkStringList *strings = NULL;
  AwraCompletionPopover *completion = make_completion (&strings);
  guint activated = G_MAXUINT;
  guint dismissed = 0;

  g_signal_connect (completion, "activated", G_CALLBACK (record_position), &activated);
  g_signal_connect (completion, "dismissed", G_CALLBACK (count), &dismissed);
  gtk_window_set_child (GTK_WINDOW (window), view);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);
  awra_completion_popover_attach (completion, view);
  g_assert_true (gtk_widget_get_parent (GTK_WIDGET (completion)) == view);
  gtk_window_present (GTK_WINDOW (window));
  gtk_widget_grab_focus (view);
  run (100);

  /* Hidden: the keys are the text's. */
  g_assert_false (press (view, GDK_KEY_Down));
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), "hi @a", -1);
  awra_completion_popover_present_at_cursor (completion);
  run (100);
  g_assert_true (gtk_widget_get_visible (GTK_WIDGET (completion)));
  g_assert_cmpuint (awra_completion_popover_get_selected (completion), ==, 0);

  /* Up and Down wrap around. */
  g_assert_true (press (view, GDK_KEY_Up));
  g_assert_cmpuint (awra_completion_popover_get_selected (completion), ==, 2);
  g_assert_true (press (view, GDK_KEY_Down));
  g_assert_cmpuint (awra_completion_popover_get_selected (completion), ==, 0);
  g_assert_true (press (view, GDK_KEY_Down));
  g_assert_true (press (view, GDK_KEY_Return));
  g_assert_cmpuint (activated, ==, 1);
  g_assert_true (press (view, GDK_KEY_Tab));
  g_assert_cmpuint (activated, ==, 1);
  /* Other keys still type. */
  g_assert_false (press (view, GDK_KEY_b));

  /* A narrower filter starts again from its first match. */
  gtk_string_list_remove (strings, 0);
  g_assert_cmpuint (awra_completion_popover_get_selected (completion), ==, 0);

  g_assert_true (press (view, GDK_KEY_Escape));
  g_assert_cmpuint (dismissed, ==, 1);
  g_assert_false (gtk_widget_get_visible (GTK_WIDGET (completion)));

  /* An empty model shows nothing. */
  awra_completion_popover_present_at_cursor (completion);
  g_assert_true (gtk_widget_get_visible (GTK_WIDGET (completion)));
  gtk_string_list_splice (strings, 0, 2, NULL);
  g_assert_false (gtk_widget_get_visible (GTK_WIDGET (completion)));
  awra_completion_popover_present_at_cursor (completion);
  g_assert_false (gtk_widget_get_visible (GTK_WIDGET (completion)));

  /* The popover goes away with its text widget. */
  gtk_window_destroy (GTK_WINDOW (window));
  run (20);
  g_object_unref (strings);
}

static void
test_entry_and_reattach (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *entry = gtk_entry_new ();
  GtkWidget *other = gtk_entry_new ();
  GtkStringList *strings = NULL;
  AwraCompletionPopover *completion = make_completion (&strings);

  gtk_box_append (GTK_BOX (box), entry);
  gtk_box_append (GTK_BOX (box), other);
  gtk_window_set_child (GTK_WINDOW (window), box);
  awra_completion_popover_attach (completion, entry);
  gtk_window_present (GTK_WINDOW (window));
  gtk_widget_grab_focus (entry);
  gtk_editable_set_text (GTK_EDITABLE (entry), "/he");
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  run (100);
  awra_completion_popover_present_at_cursor (completion);
  run (100);
  g_assert_true (gtk_widget_get_visible (GTK_WIDGET (completion)));
  g_assert_true (press (entry, GDK_KEY_Down));

  /* Moving to another field keeps the popover alive. */
  awra_completion_popover_attach (completion, other);
  g_assert_true (awra_completion_popover_get_attached_widget (completion) == other);
  g_assert_false (gtk_widget_get_visible (GTK_WIDGET (completion)));
  g_assert_false (press (entry, GDK_KEY_Down));
  /* Detached, it belongs to whoever holds it. */
  g_object_ref (completion);
  awra_completion_popover_attach (completion, NULL);
  g_assert_null (gtk_widget_get_parent (GTK_WIDGET (completion)));
  g_object_unref (completion);
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (strings);
}

static void
test_composer_leaves_enter (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *composer = awra_composer_new ();
  GtkWidget *view = GTK_WIDGET (awra_composer_get_view (AWRA_COMPOSER (composer)));
  GtkStringList *strings = NULL;
  AwraCompletionPopover *completion = make_completion (&strings);
  guint activated = G_MAXUINT;
  guint submits = 0;

  g_signal_connect (completion, "activated", G_CALLBACK (record_position), &activated);
  g_signal_connect (composer, "submit", G_CALLBACK (count), &submits);
  gtk_window_set_child (GTK_WINDOW (window), composer);
  awra_completion_popover_attach (completion, view);
  gtk_window_present (GTK_WINDOW (window));
  gtk_widget_grab_focus (composer);
  awra_composer_set_text (AWRA_COMPOSER (composer), "hi @a");
  run (100);
  awra_completion_popover_present_at_cursor (completion);
  run (100);
  /* Enter chooses the suggestion and sends nothing. */
  g_assert_true (press (view, GDK_KEY_Return));
  g_assert_cmpuint (activated, ==, 0);
  g_assert_cmpuint (submits, ==, 0);
  gtk_popover_popdown (GTK_POPOVER (completion));
  g_assert_true (press (view, GDK_KEY_Return));
  g_assert_cmpuint (submits, ==, 1);
  gtk_window_destroy (GTK_WINDOW (window));
  run (20);
  g_object_unref (strings);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/completion-popover/properties", test_properties);
  g_test_add_func ("/completion-popover/keyboard", test_keyboard);
  g_test_add_func ("/completion-popover/entry-and-reattach", test_entry_and_reattach);
  g_test_add_func ("/completion-popover/composer-leaves-enter", test_composer_leaves_enter);
  return g_test_run ();
}
