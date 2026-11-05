/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra.h>
#include <math.h>

typedef struct
{
  guint seeks, commits;
  double value;
} Log;
static void
seek_cb (AwraTimeline *t, double value, Log *log)
{
  (void)t;
  log->seeks++;
  log->value = value;
}
static void
commit_cb (AwraTimeline *t, double value, Log *log)
{
  (void)t;
  log->commits++;
  log->value = value;
}
static GtkEventController *
controller (GtkWidget *w, GType type)
{
  GListModel *model = gtk_widget_observe_controllers (w);
  GtkEventController *result = NULL;
  for (guint n = 0; n < g_list_model_get_n_items (model); n++)
    {
      GtkEventController *c = g_list_model_get_item (model, n);
      if (G_TYPE_CHECK_INSTANCE_TYPE (c, type))
        {
          result = c;
          break;
        }
      g_object_unref (c);
    }
  g_object_unref (model);
  return result;
}
static void
test_contract (void)
{
  g_autoptr (GtkWidget) w = g_object_ref_sink (awra_timeline_new ());
  AwraTimeline *t = AWRA_TIMELINE (w);
  Log log = { 0 };
  g_assert_cmpfloat (awra_timeline_get_step_increment (t), ==, 1);
  g_assert_cmpfloat (awra_timeline_get_page_increment (t), ==, 10);
  g_autoptr (GtkBuilder) builder = gtk_builder_new_from_string (
      "<interface><object class='AwraTimeline' id='timeline'>"
      "<property name='step-increment'>2.5</property>"
      "<property name='page-increment'>15</property>"
      "</object></interface>", -1);
  AwraTimeline *built = AWRA_TIMELINE (gtk_builder_get_object (builder, "timeline"));
  g_assert_cmpfloat (awra_timeline_get_step_increment (built), ==, 2.5);
  g_assert_cmpfloat (awra_timeline_get_page_increment (built), ==, 15);
  awra_timeline_set_step_increment (t, 5);
  awra_timeline_set_page_increment (t, 30);
  g_assert_cmpfloat (awra_timeline_get_step_increment (t), ==, 5);
  g_assert_cmpfloat (awra_timeline_get_page_increment (t), ==, 30);
  awra_timeline_set_step_increment (t, NAN);
  awra_timeline_set_page_increment (t, 0);
  g_assert_cmpfloat (awra_timeline_get_step_increment (t), ==, 1);
  g_assert_cmpfloat (awra_timeline_get_page_increment (t), ==, 10);
  g_signal_connect (t, "seek-requested", G_CALLBACK (seek_cb), &log);
  g_signal_connect (t, "offset-committed", G_CALLBACK (commit_cb), &log);
  awra_timeline_set_duration (t, 120);
  awra_timeline_set_position (t, 80);
  awra_timeline_set_duration (t, 30);
  g_assert_cmpfloat (awra_timeline_get_position (t), ==, 30);
  awra_timeline_set_offset (t, NAN);
  g_assert_cmpfloat (awra_timeline_get_offset (t), ==, 0);
  awra_timeline_add_interval (t, 2, 5, "A long cue");
  awra_timeline_add_interval (t, 5, 5, "Bookmark");
  awra_timeline_add_interval (t, 6, 2, "Invalid");
  g_assert_cmpuint (awra_timeline_get_n_intervals (t), ==, 2);
  g_assert_true (GTK_IS_ACCESSIBLE_RANGE (t));
  g_assert_true (
      GTK_ACCESSIBLE_RANGE_GET_IFACE (t)->set_current_value (GTK_ACCESSIBLE_RANGE (t), 15));
  g_assert_cmpuint (log.seeks, ==, 1);
  g_assert_cmpfloat (log.value, ==, 15);
  g_assert_cmpfloat (awra_timeline_get_position (t), ==, 30);
  awra_timeline_set_editable (t, TRUE);
  g_assert_true (
      GTK_ACCESSIBLE_RANGE_GET_IFACE (t)->set_current_value (GTK_ACCESSIBLE_RANGE (t), -0.5));
  g_assert_cmpfloat (awra_timeline_get_offset (t), ==, -0.5);
  g_assert_cmpuint (log.commits, ==, 1);
  gtk_widget_set_sensitive (w, FALSE);
  g_assert_false (
      GTK_ACCESSIBLE_RANGE_GET_IFACE (t)->set_current_value (GTK_ACCESSIBLE_RANGE (t), 1));
  awra_timeline_clear_intervals (t);
  g_assert_cmpuint (awra_timeline_get_n_intervals (t), ==, 0);
}
static void
test_input (void)
{
  g_autoptr (GtkWidget) win = g_object_ref_sink (gtk_window_new ());
  GtkWidget *w = awra_timeline_new ();
  AwraTimeline *t = AWRA_TIMELINE (w);
  Log log = { 0 };
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 120);
  gtk_window_set_child (GTK_WINDOW (win), w);
  awra_timeline_set_duration (t, 100);
  awra_timeline_set_position (t, 10);
  awra_timeline_add_interval (t, 10, 12, "Subtitle");
  g_signal_connect (t, "seek-requested", G_CALLBACK (seek_cb), &log);
  g_signal_connect (t, "offset-committed", G_CALLBACK (commit_cb), &log);
  gtk_window_present (GTK_WINDOW (win));
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add_once (160, (GSourceOnceFunc)g_main_loop_quit, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
  g_autoptr (GtkEventController) k
      = controller (w, GTK_TYPE_EVENT_CONTROLLER_KEY);
  gboolean handled = FALSE;
  g_signal_emit_by_name (k, "key-pressed", GDK_KEY_Right, 0, 0, &handled);
  g_assert_true (handled);
  g_assert_cmpfloat (log.value, ==, 11);
  awra_timeline_set_step_increment (t, 5);
  awra_timeline_set_page_increment (t, 30);
  g_signal_emit_by_name (k, "key-pressed", GDK_KEY_Right, 0, 0, &handled);
  g_assert_cmpfloat (log.value, ==, 15);
  g_signal_emit_by_name (k, "key-pressed", GDK_KEY_Right, 0, GDK_SHIFT_MASK, &handled);
  g_assert_cmpfloat (log.value, ==, 40);
  awra_timeline_set_step_increment (t, 1);
  awra_timeline_set_page_increment (t, 10);
  gtk_widget_set_direction (w, GTK_TEXT_DIR_RTL);
  g_signal_emit_by_name (k, "key-pressed", GDK_KEY_Right, 0, 0, &handled);
  g_assert_cmpfloat (log.value, ==, 9);
  gtk_widget_set_direction (w, GTK_TEXT_DIR_LTR);
  awra_timeline_set_editable (t, TRUE);
  g_signal_emit_by_name (k, "key-pressed", GDK_KEY_Left, 0, 0, &handled);
  g_assert_cmpfloat_with_epsilon (log.value, -0.1, 0.001);
  g_autoptr (GtkEventController) drag = controller (w, GTK_TYPE_GESTURE_DRAG);
  g_signal_emit_by_name (drag, "drag-begin", 100.0, 20.0);
  g_signal_emit_by_name (drag, "drag-update", 40.0, 0.0);
  double edited = awra_timeline_get_offset (t);
  g_assert_cmpfloat (edited, >, 0);
  g_signal_emit_by_name (drag, "cancel", NULL);
  g_assert_cmpfloat_with_epsilon (awra_timeline_get_offset (t), -0.1, 0.001);
  awra_timeline_set_preview (t, gtk_label_new ("Preview"));
  g_assert_nonnull (awra_timeline_get_preview (t));
  g_autoptr (GtkEventController) motion = controller (w, GTK_TYPE_EVENT_CONTROLLER_MOTION);
  g_signal_emit_by_name (motion, "motion", 50.0, 20.0);
  GdkRectangle anchor;
  gtk_popover_get_pointing_to (GTK_POPOVER (awra_timeline_get_preview_popover (t)), &anchor);
  g_assert_cmpint (anchor.x, ==, 50);
  double first_hover = awra_timeline_get_hover_time (t);
  g_signal_emit_by_name (motion, "motion", 250.0, 20.0);
  gtk_popover_get_pointing_to (GTK_POPOVER (awra_timeline_get_preview_popover (t)), &anchor);
  g_assert_cmpint (anchor.x, ==, 250);
  g_assert_cmpfloat (awra_timeline_get_hover_time (t), >, first_hover);
  gtk_widget_set_direction (w, GTK_TEXT_DIR_RTL);
  g_signal_emit_by_name (motion, "motion", 50.0, 20.0);
  g_assert_cmpfloat (awra_timeline_get_hover_time (t), >, first_hover);
  awra_timeline_set_duration (t, 0);
  g_assert_cmpfloat (awra_timeline_get_hover_x (t), ==, -1);
  awra_timeline_set_preview (t, NULL);
  g_assert_null (awra_timeline_get_preview (t));
  g_autoptr (GtkBuilder) builder = gtk_builder_new_from_string (
      "<interface><object class='AwraTimeline' id='time'><property "
      "name='duration'>60</property><property "
      "name='editable'>true</property></object></interface>",
      -1);
  g_assert_true (awra_timeline_get_editable (
      AWRA_TIMELINE (gtk_builder_get_object (builder, "time"))));
  gtk_window_destroy (GTK_WINDOW (win));
}
int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  awra_init ();
  g_test_add_func ("/timeline/contract", test_contract);
  g_test_add_func ("/timeline/input", test_input);
  return g_test_run ();
}
