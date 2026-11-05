/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>
#include <math.h>

typedef struct {
  double last;
  double max;
  double min;
  double largest_step;
  guint calls;
  guint destroyed;
} Recorder;

static void
record_value (double   value,
              gpointer user_data)
{
  Recorder *recorder = user_data;

  if (recorder->calls > 0)
    recorder->largest_step = MAX (recorder->largest_step, fabs (value - recorder->last));
  recorder->last = value;
  recorder->max = recorder->calls == 0 ? value : MAX (recorder->max, value);
  recorder->min = recorder->calls == 0 ? value : MIN (recorder->min, value);
  recorder->calls++;
}

static void
recorder_destroyed (gpointer user_data)
{
  ((Recorder *) user_data)->destroyed++;
}

static AwraAnimationTarget *
recording_target (Recorder *recorder)
{
  return awra_callback_animation_target_new (record_value, recorder, recorder_destroyed);
}

static void
count_done (AwraAnimation *animation,
            guint         *count)
{
  (void) animation;
  (*count)++;
}

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
quit_on_done (AwraAnimation *animation,
              GMainLoop     *loop)
{
  (void) animation;
  g_main_loop_quit (loop);
}

static void
run_until_done (AwraAnimation *animation)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  gulong id;
  guint timeout;

  if (awra_animation_get_state (animation) != AWRA_ANIMATION_STATE_PLAYING)
    return;
  id = g_signal_connect (animation, "done", G_CALLBACK (quit_on_done), loop);
  timeout = g_timeout_add (5000, quit_loop, loop);
  g_main_loop_run (loop);
  g_source_remove (timeout);
  g_signal_handler_disconnect (animation, id);
}

static GtkWidget *
mapped_widget (GtkWidget **window_out)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *widget = gtk_label_new ("animated");

  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));
  run_for (120);
  g_assert_true (gtk_widget_get_mapped (widget));
  *window_out = window;
  return widget;
}

static void
test_easing (void)
{
  GEnumClass *klass = g_type_class_ref (AWRA_TYPE_EASING);
  gboolean overshoots = FALSE;

  for (guint i = 0; i < klass->n_values; i++) {
    AwraEasing easing = klass->values[i].value;

    g_assert_cmpfloat_with_epsilon (awra_easing_ease (easing, 0.0), 0.0, 1e-9);
    g_assert_cmpfloat_with_epsilon (awra_easing_ease (easing, 1.0), 1.0, 1e-9);
    g_assert_cmpfloat_with_epsilon (awra_easing_ease (easing, -3.0), 0.0, 1e-9);
    g_assert_cmpfloat_with_epsilon (awra_easing_ease (easing, 4.0), 1.0, 1e-9);
  }
  for (double t = 0.0; t <= 1.0; t += 0.01)
    overshoots |= awra_easing_ease (AWRA_EASING_EASE_OUT_BACK, t) > 1.0;
  g_assert_true (overshoots);
  g_assert_cmpfloat_with_epsilon (awra_easing_ease (AWRA_EASING_LINEAR, 0.25), 0.25, 1e-9);
  g_assert_cmpfloat (awra_easing_ease (AWRA_EASING_EASE_OUT_CUBIC, 0.5), >, 0.5);
  g_assert_cmpfloat_with_epsilon (awra_easing_ease (AWRA_EASING_EASE_IN_OUT_SINE, 0.5), 0.5, 1e-9);
  g_type_class_unref (klass);
}

static void
test_unmapped_jumps_to_end (void)
{
  GtkWidget *widget = g_object_ref_sink (gtk_label_new ("hidden"));
  Recorder recorder = { 0 };
  AwraAnimation *animation;
  guint done = 0;

  animation = awra_timed_animation_new (widget, 2.0, 10.0, 200, recording_target (&recorder));
  g_assert_true (AWRA_IS_TIMED_ANIMATION (animation));
  g_assert_true (awra_animation_get_widget (animation) == widget);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_IDLE);
  g_assert_true (awra_animation_get_follow_reduced_motion (animation));
  g_signal_connect (animation, "done", G_CALLBACK (count_done), &done);
  awra_animation_play (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_FINISHED);
  g_assert_cmpfloat (awra_animation_get_value (animation), ==, 10.0);
  g_assert_cmpfloat (recorder.last, ==, 10.0);
  g_assert_cmpuint (done, ==, 1);
  /* Skipping a finished animation is a no-op. */
  awra_animation_skip (animation);
  g_assert_cmpuint (done, ==, 1);
  awra_animation_reset (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_IDLE);
  g_assert_cmpfloat (recorder.last, ==, 2.0);
  /* Pausing or resuming outside playback does nothing. */
  awra_animation_pause (animation);
  awra_animation_resume (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_IDLE);
  g_object_unref (animation);
  g_assert_cmpuint (recorder.destroyed, ==, 1);
  g_object_unref (widget);
}

static void
test_direction_and_repeat (void)
{
  GtkWidget *widget = g_object_ref_sink (gtk_label_new ("hidden"));
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_timed_animation_new_for_preset (widget, 0.0, 1.0, AWRA_MOTION_PRESET_SLOW,
                                         recording_target (&recorder));
  AwraTimedAnimation *timed = AWRA_TIMED_ANIMATION (animation);

  g_assert_cmpuint (awra_timed_animation_get_duration (timed), ==, 260);
  g_assert_cmpint (awra_timed_animation_get_easing (timed), ==, AWRA_EASING_EASE_OUT_CUBIC);
  g_assert_cmpuint (awra_timed_animation_get_repeat_count (timed), ==, 1);
  awra_timed_animation_set_reverse (timed, TRUE);
  g_assert_true (awra_timed_animation_get_reverse (timed));
  awra_animation_play (animation);
  g_assert_cmpfloat (recorder.last, ==, 0.0);
  awra_timed_animation_set_reverse (timed, FALSE);
  awra_timed_animation_set_alternate (timed, TRUE);
  awra_timed_animation_set_repeat_count (timed, 2);
  g_assert_true (awra_timed_animation_get_alternate (timed));
  awra_animation_play (animation);
  /* There and back again. */
  g_assert_cmpfloat (recorder.last, ==, 0.0);
  awra_timed_animation_set_repeat_count (timed, 0);
  awra_animation_reset (animation);
  awra_animation_skip (animation);
  g_assert_cmpfloat (recorder.last, ==, 1.0);
  awra_timed_animation_set_value_from (timed, 5.0);
  awra_timed_animation_set_value_to (timed, 6.0);
  awra_timed_animation_set_easing (timed, AWRA_EASING_LINEAR);
  g_assert_cmpfloat (awra_timed_animation_get_value_from (timed), ==, 5.0);
  g_assert_cmpfloat (awra_timed_animation_get_value_to (timed), ==, 6.0);
  g_object_unref (animation);
  g_object_unref (widget);
}

static void
test_property_target (void)
{
  GtkWidget *widget = g_object_ref_sink (gtk_label_new ("hidden"));
  AwraAnimationTarget *target = awra_property_animation_target_new (G_OBJECT (widget),
                                                                    "width-request");
  AwraAnimation *animation;

  g_assert_true (awra_property_animation_target_get_object (
                   AWRA_PROPERTY_ANIMATION_TARGET (target)) == G_OBJECT (widget));
  g_assert_cmpstr (awra_property_animation_target_get_property_name (
                     AWRA_PROPERTY_ANIMATION_TARGET (target)), ==, "width-request");
  animation = awra_timed_animation_new (widget, 0.0, 41.6, 100, target);
  awra_animation_play (animation);
  {
    int width = 0;

    gtk_widget_get_size_request (widget, &width, NULL);
    g_assert_cmpint (width, ==, 42);
  }
  g_object_unref (animation);

  animation = awra_timed_animation_new (widget, 1.0, 0.25, 100,
                                        awra_property_animation_target_new (G_OBJECT (widget),
                                                                            "opacity"));
  awra_animation_play (animation);
  g_assert_cmpfloat_with_epsilon (gtk_widget_get_opacity (widget), 0.25, 0.01);
  g_object_unref (widget);
  /* The object went away: the target ignores further values. */
  awra_animation_reset (animation);
  g_assert_null (awra_property_animation_target_get_object (
                   AWRA_PROPERTY_ANIMATION_TARGET (awra_animation_get_target (animation))));
  g_object_unref (animation);
}

static void
test_timed_playback (void)
{
  GtkWidget *window;
  GtkWidget *widget = mapped_widget (&window);
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_timed_animation_new (widget, 0.0, 100.0, 400, recording_target (&recorder));
  AwraAnimation *weak = animation;
  guint done = 0;
  double held;

  awra_timed_animation_set_easing (AWRA_TIMED_ANIMATION (animation), AWRA_EASING_LINEAR);
  g_signal_connect (animation, "done", G_CALLBACK (count_done), &done);
  awra_animation_play (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PLAYING);
  g_assert_cmpfloat (recorder.last, ==, 0.0);
  run_for (120);
  held = awra_animation_get_value (animation);
  g_assert_cmpfloat (held, >, 0.0);
  g_assert_cmpfloat (held, <, 100.0);
  awra_animation_pause (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PAUSED);
  run_for (100);
  g_assert_cmpfloat (awra_animation_get_value (animation), ==, held);
  awra_animation_resume (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PLAYING);
  /* The application drops its reference: a playing animation lives on. */
  g_object_add_weak_pointer (G_OBJECT (animation), (gpointer *) &weak);
  g_object_ref (animation);
  g_object_unref (animation);
  run_until_done (animation);
  g_assert_cmpuint (done, ==, 1);
  g_assert_cmpfloat (recorder.last, ==, 100.0);
  g_assert_cmpfloat (recorder.min, >=, 0.0);
  g_assert_cmpfloat (recorder.max, <=, 100.0);
  g_assert_cmpuint (recorder.calls, >, 3);
  g_object_unref (animation);
  g_assert_null (weak);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_fire_and_forget (void)
{
  GtkWidget *window;
  GtkWidget *widget = mapped_widget (&window);
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_timed_animation_new (widget, 0.0, 1.0, 150, recording_target (&recorder));
  AwraAnimation *weak = animation;

  g_object_add_weak_pointer (G_OBJECT (animation), (gpointer *) &weak);
  awra_animation_play (animation);
  g_object_unref (animation);
  g_assert_nonnull (weak);
  run_for (400);
  g_assert_null (weak);
  g_assert_cmpfloat (recorder.last, ==, 1.0);
  g_assert_cmpuint (recorder.destroyed, ==, 1);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_reduced_motion (void)
{
  GtkWidget *window;
  GtkWidget *widget = mapped_widget (&window);
  AwraStyleManager *style = awra_context_get_style_manager (
    awra_context_get_for_display (gtk_widget_get_display (widget)));
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_timed_animation_new (widget, 0.0, 1.0, 300, recording_target (&recorder));

  awra_style_manager_set_reduced_motion (style, TRUE);
  awra_animation_play (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_FINISHED);
  g_assert_cmpfloat (recorder.last, ==, 1.0);
  /* Motion that carries meaning may opt out. */
  awra_animation_set_follow_reduced_motion (animation, FALSE);
  awra_animation_play (animation);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PLAYING);
  awra_animation_skip (animation);
  awra_style_manager_set_reduced_motion (style, FALSE);
  g_object_unref (animation);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_unmap_finishes (void)
{
  GtkWidget *window;
  GtkWidget *widget = mapped_widget (&window);
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_timed_animation_new (widget, 0.0, 1.0, 5000, recording_target (&recorder));
  AwraAnimation *weak = animation;
  guint done = 0;

  awra_timed_animation_set_repeat_count (AWRA_TIMED_ANIMATION (animation), 0);
  g_signal_connect (animation, "done", G_CALLBACK (count_done), &done);
  g_object_add_weak_pointer (G_OBJECT (animation), (gpointer *) &weak);
  awra_animation_play (animation);
  g_object_unref (animation);
  run_for (50);
  g_assert_nonnull (weak);
  gtk_window_destroy (GTK_WINDOW (window));
  g_assert_cmpuint (done, ==, 1);
  g_assert_cmpfloat (recorder.last, ==, 1.0);
  g_assert_null (weak);
}

static void
test_spring_settles (void)
{
  GtkWidget *widget = g_object_ref_sink (gtk_label_new ("hidden"));
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_spring_animation_new (widget, 0.0, 1.0, 1.0, 400.0, recording_target (&recorder));
  AwraSpringAnimation *spring = AWRA_SPRING_ANIMATION (animation);
  guint critical;
  guint bouncy;

  g_assert_cmpfloat (awra_spring_animation_get_damping_ratio (spring), ==, 1.0);
  g_assert_cmpfloat (awra_spring_animation_get_mass (spring), ==, 1.0);
  g_assert_cmpfloat (awra_spring_animation_get_stiffness (spring), ==, 400.0);
  g_assert_cmpfloat (awra_spring_animation_get_epsilon (spring), ==, 0.001);
  g_assert_false (awra_spring_animation_get_clamp (spring));
  critical = awra_spring_animation_get_estimated_duration (spring);
  /* sqrt(400) = 20 rad/s: a critically damped spring settles in ~0.5 s. */
  g_assert_cmpuint (critical, >, 300);
  g_assert_cmpuint (critical, <, 900);
  awra_spring_animation_set_damping_ratio (spring, 0.3);
  bouncy = awra_spring_animation_get_estimated_duration (spring);
  g_assert_cmpuint (bouncy, >, critical);
  awra_spring_animation_set_clamp (spring, TRUE);
  g_assert_cmpuint (awra_spring_animation_get_estimated_duration (spring), <, bouncy);
  awra_spring_animation_set_clamp (spring, FALSE);
  awra_spring_animation_set_damping_ratio (spring, 0.0);
  g_assert_cmpuint (awra_spring_animation_get_estimated_duration (spring), ==,
                    AWRA_DURATION_INFINITE);
  awra_spring_animation_set_damping_ratio (spring, 3.0);
  g_assert_cmpuint (awra_spring_animation_get_estimated_duration (spring), >, critical);
  awra_spring_animation_set_value_from (spring, 1.0);
  awra_spring_animation_set_initial_velocity (spring, 0.0);
  g_assert_cmpuint (awra_spring_animation_get_estimated_duration (spring), ==, 0);
  awra_spring_animation_set_mass (spring, 2.0);
  awra_spring_animation_set_stiffness (spring, 300.0);
  awra_spring_animation_set_epsilon (spring, 0.1);
  awra_spring_animation_set_initial_velocity (spring, 3.0);
  g_assert_cmpfloat (awra_spring_animation_get_velocity (spring), ==, 3.0);
  awra_animation_play (animation);
  g_assert_cmpfloat (recorder.last, ==, 1.0);
  g_assert_cmpfloat (awra_spring_animation_get_velocity (spring), ==, 0.0);
  g_object_unref (animation);
  g_object_unref (widget);
}

static void
test_spring_retarget (void)
{
  GtkWidget *window;
  GtkWidget *widget = mapped_widget (&window);
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_spring_animation_new (widget, 0.0, 100.0, 0.8, 300.0, recording_target (&recorder));
  AwraSpringAnimation *spring = AWRA_SPRING_ANIMATION (animation);
  double before_value;
  double before_velocity;
  double step_before;
  guint done = 0;

  awra_spring_animation_set_epsilon (spring, 0.1);
  g_signal_connect (animation, "done", G_CALLBACK (count_done), &done);
  awra_animation_play (animation);
  run_for (90);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PLAYING);
  before_value = awra_animation_get_value (animation);
  before_velocity = awra_spring_animation_get_velocity (spring);
  g_assert_cmpfloat (before_value, >, 0.0);
  g_assert_cmpfloat (before_velocity, >, 0.0);
  step_before = recorder.largest_step;
  /* Turn around in flight: the spring keeps its value and velocity. */
  awra_spring_animation_retarget (spring, -50.0);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PLAYING);
  g_assert_cmpfloat (awra_spring_animation_get_value_to (spring), ==, -50.0);
  g_assert_cmpfloat (awra_spring_animation_get_initial_velocity (spring), >, 0.0);
  g_assert_cmpfloat (fabs (awra_spring_animation_get_value_from (spring) - before_value),
                     <, step_before + 1.0);
  run_until_done (animation);
  g_assert_cmpuint (done, ==, 1);
  g_assert_cmpfloat (recorder.last, ==, -50.0);
  /* No frame jumped by much more than the fastest frame of the first leg. */
  g_assert_cmpfloat (recorder.largest_step, <, MAX (step_before, 1.0) * 3.0);
  /* A finished spring starts again from where it rests. */
  awra_spring_animation_retarget (spring, 10.0);
  g_assert_cmpfloat (awra_spring_animation_get_value_from (spring), ==, -50.0);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PLAYING);
  awra_animation_pause (animation);
  awra_spring_animation_retarget (spring, 20.0);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_PAUSED);
  awra_animation_resume (animation);
  run_until_done (animation);
  g_assert_cmpfloat (recorder.last, ==, 20.0);
  g_object_unref (animation);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_spring_idle_retarget (void)
{
  GtkWidget *widget = g_object_ref_sink (gtk_label_new ("hidden"));
  Recorder recorder = { 0 };
  AwraAnimation *animation =
    awra_spring_animation_new (widget, 3.0, 4.0, 1.0, 500.0, recording_target (&recorder));

  awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (animation), 8.0);
  /* Unmapped: it jumps straight to the new destination. */
  g_assert_cmpfloat (recorder.last, ==, 8.0);
  g_assert_cmpint (awra_animation_get_state (animation), ==, AWRA_ANIMATION_STATE_FINISHED);
  g_object_unref (animation);
  g_object_unref (widget);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  awra_init ();
  g_test_add_func ("/animation/easing", test_easing);
  g_test_add_func ("/animation/unmapped", test_unmapped_jumps_to_end);
  g_test_add_func ("/animation/direction-repeat", test_direction_and_repeat);
  g_test_add_func ("/animation/property-target", test_property_target);
  g_test_add_func ("/animation/timed-playback", test_timed_playback);
  g_test_add_func ("/animation/fire-and-forget", test_fire_and_forget);
  g_test_add_func ("/animation/reduced-motion", test_reduced_motion);
  g_test_add_func ("/animation/unmap", test_unmap_finishes);
  g_test_add_func ("/animation/spring-settles", test_spring_settles);
  g_test_add_func ("/animation/spring-retarget", test_spring_retarget);
  g_test_add_func ("/animation/spring-idle-retarget", test_spring_idle_retarget);
  return g_test_run ();
}
