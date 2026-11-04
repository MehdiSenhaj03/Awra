/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static void
test_properties (void)
{
  GtkWidget *widget = g_object_ref_sink (awra_divider_new (NULL));
  AwraDivider *divider = AWRA_DIVIDER (widget);
  AwraDividerAppearance appearance;
  g_autofree char *label = NULL;

  g_assert_null (awra_divider_get_label (divider));
  g_assert_cmpint (awra_divider_get_label_position (divider), ==, AWRA_DIVIDER_LABEL_POSITION_CENTER);
  g_assert_cmpint (awra_divider_get_appearance (divider), ==, AWRA_DIVIDER_APPEARANCE_NEUTRAL);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (widget)), ==,
                   GTK_ACCESSIBLE_ROLE_SEPARATOR);
  g_assert_true (gtk_widget_has_css_class (widget, "awra-divider-neutral"));

  awra_divider_set_label (divider, "Today");
  g_assert_cmpstr (awra_divider_get_label (divider), ==, "Today");
  awra_divider_set_label_position (divider, AWRA_DIVIDER_LABEL_POSITION_END);
  awra_divider_set_appearance (divider, AWRA_DIVIDER_APPEARANCE_DANGER);
  g_assert_true (gtk_widget_has_css_class (widget, "awra-divider-danger"));
  g_assert_false (gtk_widget_has_css_class (widget, "awra-divider-neutral"));
  g_object_get (widget, "label", &label, "appearance", &appearance, NULL);
  g_assert_cmpstr (label, ==, "Today");
  g_assert_cmpint (appearance, ==, AWRA_DIVIDER_APPEARANCE_DANGER);

  /* An empty label is no label. */
  awra_divider_set_label (divider, "");
  g_assert_null (awra_divider_get_label (divider));
  g_object_unref (widget);
}

static void
test_layout (void)
{
  GtkWidget *plain = g_object_ref_sink (awra_divider_new (NULL));
  GtkWidget *labelled = g_object_ref_sink (awra_divider_new ("A rather long day label"));
  int plain_width = 0;
  int plain_height = 0;
  int center_width = 0;
  int end_width = 0;
  int labelled_height = 0;

  gtk_widget_measure (plain, GTK_ORIENTATION_HORIZONTAL, -1, &plain_width, NULL, NULL, NULL);
  gtk_widget_measure (plain, GTK_ORIENTATION_VERTICAL, -1, &plain_height, NULL, NULL, NULL);
  gtk_widget_measure (labelled, GTK_ORIENTATION_HORIZONTAL, -1, &center_width, NULL, NULL, NULL);
  gtk_widget_measure (labelled, GTK_ORIENTATION_VERTICAL, -1, &labelled_height, NULL, NULL, NULL);
  awra_divider_set_label_position (AWRA_DIVIDER (labelled), AWRA_DIVIDER_LABEL_POSITION_END);
  gtk_widget_measure (labelled, GTK_ORIENTATION_HORIZONTAL, -1, &end_width, NULL, NULL, NULL);
  g_assert_cmpint (plain_height, >=, 1);
  g_assert_cmpint (labelled_height, >, plain_height);
  /* The label ellipsizes, and a label at the end keeps one gap. */
  g_assert_cmpint (end_width, <, center_width);
  gtk_widget_size_allocate (labelled, &(GtkAllocation) { 0, 0, 30, labelled_height }, -1);
  gtk_widget_size_allocate (plain, &(GtkAllocation) { 0, 0, 200, plain_height }, -1);
  g_object_unref (plain);
  g_object_unref (labelled);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/divider/properties", test_properties);
  g_test_add_func ("/divider/layout", test_layout);
  return g_test_run ();
}
