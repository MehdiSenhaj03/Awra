/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra.h>
#include <math.h>

static void
count_notify (GObject *object, GParamSpec *pspec, guint *count)
{
  (void)object;
  (void)pspec;
  (*count)++;
}

static void
test_contract (void)
{
  g_autoptr (AwraTaskRow) row
      = g_object_ref_sink (AWRA_TASK_ROW (awra_task_row_new ("Export")));
  guint notifications = 0;
  g_signal_connect (row, "notify::fraction", G_CALLBACK (count_notify),
                    &notifications);
  g_assert_cmpstr (awra_task_row_get_title (row), ==, "Export");
  g_assert_false (awra_task_row_get_show_progress (row));
  awra_task_row_set_fraction (row, 1.5);
  g_assert_cmpfloat (awra_task_row_get_fraction (row), ==, 1);
  awra_task_row_set_fraction (row, 2);
  g_assert_cmpuint (notifications, ==, 1);
  awra_task_row_set_fraction (row, NAN);
  g_assert_cmpfloat (awra_task_row_get_fraction (row), ==, 0);
  g_assert_cmpuint (notifications, ==, 2);
  awra_task_row_set_title (row, NULL);
  awra_task_row_set_subtitle (row, NULL);
  g_assert_cmpstr (awra_task_row_get_title (row), ==, "");
  awra_task_row_set_show_progress (row, TRUE);
  awra_task_row_set_indeterminate (row, TRUE);
  g_assert_true (awra_task_row_get_indeterminate (row));
  GtkWidget *first = awra_button_new_with_label ("Cancel");
  gpointer weak = first;
  g_object_add_weak_pointer (G_OBJECT (first), &weak);
  awra_task_row_set_suffix (row, first);
  g_assert_true (awra_task_row_get_suffix (row) == first);
  GtkWidget *second = awra_button_new_with_label ("Retry");
  awra_task_row_set_suffix (row, second);
  g_assert_null (weak);
  awra_task_row_set_suffix (row, NULL);
  g_assert_null (awra_task_row_get_suffix (row));
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (row)),
                   ==, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
test_builder_and_lifecycle (void)
{
  g_autoptr (GtkBuilder) builder = gtk_builder_new ();
  GError *error = NULL;
  g_type_ensure (AWRA_TYPE_TASK_ROW);
  gtk_builder_add_from_string (
      builder,
      "<interface><object class='AwraTaskRow' id='task'>"
      "<property name='title'>A very long export title repeated repeated "
      "repeated repeated</property>"
      "<property name='subtitle'>Preparing…</property>"
      "<property name='show-progress'>true</property>"
      "<property name='indeterminate'>true</property>"
      "<property name='icon-name'>document-save-symbolic</property>"
      "<property name='suffix'><object class='AwraButton'><property "
      "name='label'>Stop</property></object></property>"
      "</object></interface>",
      -1, &error);
  g_assert_no_error (error);
  GtkWidget *row = GTK_WIDGET (gtk_builder_get_object (builder, "task"));
  GtkWidget *window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 260, 180);
  gtk_widget_set_direction (row, GTK_TEXT_DIR_RTL);
  gtk_window_set_child (GTK_WINDOW (window), row);
  AwraStyleManager *style = awra_context_get_style_manager (
      awra_context_get_for_display (gtk_widget_get_display (row)));
  awra_style_manager_set_reduced_motion (style, TRUE);
  awra_style_manager_set_high_contrast (style, TRUE);
  gtk_window_present (GTK_WINDOW (window));
  for (guint i = 0; i < 20; i++)
    {
      while (g_main_context_iteration (NULL, FALSE))
        ;
      g_usleep (1000);
    }
  awra_task_row_set_indeterminate (AWRA_TASK_ROW (row), FALSE);
  awra_task_row_set_fraction (AWRA_TASK_ROW (row), .5);
  gtk_window_set_child (GTK_WINDOW (window), NULL);
  gtk_window_destroy (GTK_WINDOW (window));
  g_clear_object (&builder);
  /* Manager callbacks must not outlive the row. */
  awra_style_manager_set_reduced_motion (style, FALSE);
  awra_style_manager_set_high_contrast (style, FALSE);
}
int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv, NULL);
  awra_init ();
  g_test_add_func ("/awra/task-row/contract", test_contract);
  g_test_add_func ("/awra/task-row/builder-lifecycle",
                   test_builder_and_lifecycle);
  return g_test_run ();
}
