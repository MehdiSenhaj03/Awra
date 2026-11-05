/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra.h>
#include <math.h>

static void settle (void)
{
  gint64 end = g_get_monotonic_time () + 150 * G_TIME_SPAN_MILLISECOND;
  while (g_get_monotonic_time () < end) {
    while (g_main_context_iteration (NULL, FALSE));
    g_usleep (1000);
  }
}

static void zoom_input_and_scope (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *content = gtk_label_new ("File content");
  GtkWidget *chrome = gtk_label_new ("Toolbar");
  gtk_box_append (GTK_BOX (box), content);
  gtk_box_append (GTK_BOX (box), chrome);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));
  settle ();
  int content_height = gtk_widget_get_height (content);
  int chrome_height = gtk_widget_get_height (chrome);
  g_autoptr (AwraZoomController) zoom = awra_zoom_controller_new (content);
  g_assert_true (awra_zoom_controller_get_target (zoom) == content);
  g_assert_false (awra_zoom_controller_handle_scroll (zoom, -1, 0));
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, 1);
  g_assert_false (awra_zoom_controller_handle_scroll (zoom, 0, GDK_CONTROL_MASK));
  g_assert_true (awra_zoom_controller_handle_scroll (zoom, -1, GDK_CONTROL_MASK));
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), >, 1);
  awra_zoom_controller_set_scale (zoom, 2);
  settle ();
  g_assert_cmpint (gtk_widget_get_height (content), >, content_height);
  g_assert_cmpint (gtk_widget_get_height (chrome), ==, chrome_height);
  awra_zoom_controller_set_scale_text (zoom, FALSE);
  settle ();
  g_assert_false (awra_zoom_controller_get_scale_text (zoom));
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, 2);
  g_assert_cmpint (gtk_widget_get_height (content), ==, content_height);
  awra_zoom_controller_set_scale_text (zoom, TRUE);
  settle ();
  g_assert_cmpint (gtk_widget_get_height (content), >, content_height);
  awra_zoom_controller_set_range (zoom, .5, 2);
  awra_zoom_controller_set_scale (zoom, 30);
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, 2);
  g_assert_true (awra_zoom_controller_handle_scroll (zoom, -1, GDK_CONTROL_MASK));
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, 2);
  awra_zoom_controller_set_scale (zoom, NAN);
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, 2);
  awra_zoom_controller_set_scale (zoom, .1);
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, .5);
  awra_zoom_controller_reset (zoom);
  settle ();
  g_assert_cmpint (gtk_widget_get_height (content), ==, content_height);
  awra_zoom_controller_set_scale (zoom, 2);
  g_clear_object (&zoom);
  settle ();
  g_assert_cmpint (gtk_widget_get_height (content), ==, content_height);
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}

static void weak_target_lifetime (void)
{
  GtkWidget *target = g_object_ref_sink (gtk_label_new ("Target"));
  g_autoptr (AwraZoomController) zoom = awra_zoom_controller_new (target);
  g_autoptr (GListModel) controllers = gtk_widget_observe_controllers (target);
  gboolean found = FALSE;
  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
    g_autoptr (GtkEventController) controller = g_list_model_get_item (controllers, i);
    if (GTK_IS_EVENT_CONTROLLER_SCROLL (controller)) {
      found = TRUE;
      g_assert_cmpint (gtk_event_controller_get_propagation_phase (controller), ==, GTK_PHASE_CAPTURE);
    }
  }
  g_assert_true (found);
  g_clear_object (&controllers);
  g_object_unref (target);
  g_assert_null (awra_zoom_controller_get_target (zoom));
  g_assert_false (awra_zoom_controller_handle_scroll (zoom, -1, GDK_CONTROL_MASK));
}

static void range_notified (AwraZoomController *zoom, GParamSpec *property, guint *count)
{
  (void) property;
  double minimum, maximum, scale;
  g_object_get (zoom, "min-scale", &minimum, "max-scale", &maximum, "scale", &scale, NULL);
  g_assert_cmpfloat (minimum, <=, maximum);
  g_assert_cmpfloat (scale, >=, minimum);
  g_assert_cmpfloat (scale, <=, maximum);
  (*count)++;
}

static void coherent_range_notifications (void)
{
  GtkWidget *target = g_object_ref_sink (gtk_label_new ("Target"));
  g_autoptr (AwraZoomController) zoom = awra_zoom_controller_new (target);
  guint count = 0;
  g_signal_connect (zoom, "notify", G_CALLBACK (range_notified), &count);
  awra_zoom_controller_set_range (zoom, 4, 8);
  g_assert_cmpuint (count, ==, 3);
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, 4);
  awra_zoom_controller_set_range (zoom, .1, .2);
  g_assert_cmpuint (count, ==, 6);
  g_assert_cmpfloat (awra_zoom_controller_get_scale (zoom), ==, .2);
  g_object_unref (target);
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ()) return 77;
  awra_init ();
  g_test_add_func ("/zoom/input-scope-reset", zoom_input_and_scope);
  g_test_add_func ("/zoom/weak-target-lifetime", weak_target_lifetime);
  g_test_add_func ("/zoom/coherent-range-notifications", coherent_range_notifications);
  return g_test_run ();
}
