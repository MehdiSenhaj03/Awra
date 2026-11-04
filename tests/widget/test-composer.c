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

/* Sends a key to the composer's own controller on the text view, the way
 * GTK would deliver it in the capture phase. */
static gboolean
press (AwraComposer    *composer,
       guint            keyval,
       GdkModifierType  state)
{
  g_autoptr (GListModel) controllers =
    gtk_widget_observe_controllers (GTK_WIDGET (awra_composer_get_view (composer)));
  gboolean handled = FALSE;

  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
    g_autoptr (GtkEventController) controller = g_list_model_get_item (controllers, i);

    if (GTK_IS_EVENT_CONTROLLER_KEY (controller) &&
        gtk_event_controller_get_propagation_phase (controller) == GTK_PHASE_CAPTURE) {
      g_signal_emit_by_name (controller, "key-pressed", keyval, 0, state, &handled);
      return handled;
    }
  }
  g_assert_not_reached ();
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
  GtkWidget *widget = g_object_ref_sink (awra_composer_new ());
  AwraComposer *composer = AWRA_COMPOSER (widget);
  GtkWidget *start = gtk_button_new ();
  GtkWidget *end = gtk_button_new ();
  GtkWidget *attachments = gtk_label_new ("photo.png");
  g_autofree char *text = NULL;

  g_assert_true (GTK_IS_TEXT_VIEW (awra_composer_get_view (composer)));
  g_assert_true (awra_composer_get_buffer (composer) ==
                 gtk_text_view_get_buffer (awra_composer_get_view (composer)));
  g_assert_cmpstr (awra_composer_get_text (composer), ==, "");
  g_assert_null (awra_composer_get_placeholder_text (composer));
  g_assert_cmpuint (awra_composer_get_max_lines (composer), ==, 10);
  g_assert_true (awra_composer_get_send_on_enter (composer));
  g_assert_false (awra_composer_get_can_send (composer));
  g_assert_false (awra_composer_get_busy (composer));

  awra_composer_set_text (composer, "hello");
  g_assert_cmpstr (awra_composer_get_text (composer), ==, "hello");
  g_object_get (composer, "text", &text, NULL);
  g_assert_cmpstr (text, ==, "hello");
  awra_composer_set_text (composer, NULL);
  g_assert_cmpstr (awra_composer_get_text (composer), ==, "");
  awra_composer_set_placeholder_text (composer, "Message #general");
  g_assert_cmpstr (awra_composer_get_placeholder_text (composer), ==, "Message #general");
  awra_composer_set_max_lines (composer, 4);
  g_assert_cmpuint (awra_composer_get_max_lines (composer), ==, 4);
  awra_composer_set_busy (composer, TRUE);
  g_assert_true (awra_composer_get_busy (composer));
  awra_composer_set_busy (composer, FALSE);

  awra_composer_set_start_widget (composer, start);
  awra_composer_set_end_widget (composer, end);
  awra_composer_set_attachments (composer, attachments);
  g_assert_true (awra_composer_get_start_widget (composer) == start);
  g_assert_true (awra_composer_get_end_widget (composer) == end);
  g_assert_true (awra_composer_get_attachments (composer) == attachments);
  g_assert_true (gtk_widget_get_parent (start) == widget);
  awra_composer_set_start_widget (composer, NULL);
  awra_composer_set_attachments (composer, NULL);
  g_assert_null (awra_composer_get_start_widget (composer));
  g_assert_null (awra_composer_get_attachments (composer));

  awra_composer_show_banner (composer, "Replying to Ada", TRUE);
  g_assert_nonnull (awra_composer_get_banner (composer));
  /* A second banner replaces the first in place. */
  awra_composer_show_banner (composer, "Editing a message", FALSE);
  g_assert_nonnull (awra_composer_get_banner (composer));
  awra_composer_set_banner (composer, NULL);
  g_assert_null (awra_composer_get_banner (composer));
  g_object_unref (widget);
}

static void
test_enter (void)
{
  GtkWidget *widget = g_object_ref_sink (awra_composer_new ());
  AwraComposer *composer = AWRA_COMPOSER (widget);
  guint submits = 0;

  g_signal_connect (composer, "submit", G_CALLBACK (count), &submits);

  /* Nothing to send: Enter is taken but sends nothing. */
  g_assert_true (press (composer, GDK_KEY_Return, 0));
  g_assert_cmpuint (submits, ==, 0);
  awra_composer_set_text (composer, "   \n ");
  g_assert_true (press (composer, GDK_KEY_Return, 0));
  g_assert_cmpuint (submits, ==, 0);

  awra_composer_set_text (composer, "hello");
  g_assert_true (press (composer, GDK_KEY_Return, 0));
  g_assert_cmpuint (submits, ==, 1);
  g_assert_true (press (composer, GDK_KEY_KP_Enter, 0));
  g_assert_cmpuint (submits, ==, 2);
  /* Shift+Enter is a new line, left to the text view. */
  g_assert_false (press (composer, GDK_KEY_Return, GDK_SHIFT_MASK));
  g_assert_false (press (composer, GDK_KEY_a, 0));

  /* An input method composing keeps Enter. */
  g_signal_emit_by_name (awra_composer_get_view (composer), "preedit-changed", "かな");
  g_assert_false (press (composer, GDK_KEY_Return, 0));
  g_signal_emit_by_name (awra_composer_get_view (composer), "preedit-changed", "");
  g_assert_true (press (composer, GDK_KEY_Return, 0));
  g_assert_cmpuint (submits, ==, 3);

  /* Without send-on-enter, Ctrl+Enter sends. */
  awra_composer_set_send_on_enter (composer, FALSE);
  g_assert_false (press (composer, GDK_KEY_Return, 0));
  g_assert_true (press (composer, GDK_KEY_Return, GDK_CONTROL_MASK));
  g_assert_cmpuint (submits, ==, 4);
  awra_composer_set_send_on_enter (composer, TRUE);

  /* Attachments alone can be sent. */
  awra_composer_set_text (composer, NULL);
  awra_composer_set_can_send (composer, TRUE);
  g_assert_true (press (composer, GDK_KEY_Return, 0));
  g_assert_cmpuint (submits, ==, 5);

  /* A disabled composer sends nothing. */
  gtk_widget_set_sensitive (widget, FALSE);
  press (composer, GDK_KEY_Return, 0);
  g_assert_cmpuint (submits, ==, 5);
  g_object_unref (widget);
}

static int
height_of (GtkWidget *widget)
{
  return gtk_widget_get_height (widget);
}

static void
test_growth (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *spacer = gtk_label_new (NULL);
  GtkWidget *widget = awra_composer_new ();
  AwraComposer *composer = AWRA_COMPOSER (widget);
  g_autoptr (GString) long_text = g_string_new ("line");
  int one_line;
  int three_lines;
  int capped;
  int growing;

  gtk_widget_set_vexpand (spacer, TRUE);
  gtk_box_append (GTK_BOX (box), spacer);
  gtk_box_append (GTK_BOX (box), widget);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_set_default_size (GTK_WINDOW (window), 360, 600);
  gtk_window_present (GTK_WINDOW (window));
  run (200);
  one_line = height_of (widget);
  g_assert_cmpint (one_line, >, 0);

  awra_composer_set_text (composer, "one\ntwo\nthree");
  run (50);
  /* The well glides to its new height rather than jumping. */
  growing = height_of (widget);
  g_assert_cmpint (growing, <, one_line + 2 * 15);
  run (400);
  three_lines = height_of (widget);
  g_assert_cmpint (three_lines, >, one_line + 2 * 15);

  for (guint i = 0; i < 30; i++)
    g_string_append (long_text, "\nline");
  awra_composer_set_max_lines (composer, 4);
  awra_composer_set_text (composer, long_text->str);
  run (500);
  capped = height_of (widget);
  /* Four lines at most, then the text scrolls. */
  g_assert_cmpint (capped, >, three_lines);
  g_assert_cmpint (capped, <, three_lines + (three_lines - one_line));

  awra_composer_set_text (composer, NULL);
  run (500);
  g_assert_cmpint (height_of (widget), ==, one_line);

  /* Destroyed in the middle of a glide and a reveal. */
  awra_composer_set_busy (composer, TRUE);
  awra_composer_show_banner (composer, "Replying", TRUE);
  awra_composer_set_text (composer, "a\nb\nc");
  run (30);
  gtk_window_destroy (GTK_WINDOW (window));
  run (20);
}

static void
test_banner_close (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *widget = awra_composer_new ();
  AwraComposer *composer = AWRA_COMPOSER (widget);
  GtkWidget *banner;
  guint closed = 0;

  g_signal_connect (composer, "banner-closed", G_CALLBACK (count), &closed);
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));
  run (100);
  awra_composer_show_banner (composer, "Replying to Ada", TRUE);
  banner = awra_composer_get_banner (composer);
  run (300);
  g_assert_true (gtk_widget_get_mapped (banner));
  g_signal_emit_by_name (gtk_widget_get_last_child (banner), "clicked");
  g_assert_cmpuint (closed, ==, 1);
  g_assert_null (awra_composer_get_banner (composer));
  /* It slides out, then leaves the composer. */
  g_assert_true (gtk_widget_get_parent (banner) == widget);
  run (400);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_reduced_motion (void)
{
  AwraStyleManager *style = awra_context_get_style_manager (
    awra_context_get_for_display (gdk_display_get_default ()));
  GtkWidget *window = gtk_window_new ();
  GtkWidget *widget = awra_composer_new ();
  AwraComposer *composer = AWRA_COMPOSER (widget);
  GtkWidget *banner = g_object_ref_sink (gtk_label_new ("Editing"));
  int one_line;

  awra_style_manager_set_reduced_motion (style, TRUE);
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));
  run (100);
  one_line = height_of (widget);
  awra_composer_set_banner (composer, banner);
  awra_composer_set_text (composer, "one\ntwo");
  run (60);
  g_assert_cmpint (height_of (widget), >, one_line + gtk_widget_get_height (banner));
  awra_composer_set_banner (composer, NULL);
  /* Without motion, a removed banner leaves at once. */
  g_assert_null (gtk_widget_get_parent (banner));
  g_object_unref (banner);
  awra_style_manager_set_reduced_motion (style, FALSE);
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/composer/properties", test_properties);
  g_test_add_func ("/composer/enter", test_enter);
  g_test_add_func ("/composer/growth", test_growth);
  g_test_add_func ("/composer/banner-close", test_banner_close);
  g_test_add_func ("/composer/reduced-motion", test_reduced_motion);
  return g_test_run ();
}
