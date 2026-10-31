/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra.h>

static void spin (void)
{
  gint64 end = g_get_monotonic_time () + 200 * G_TIME_SPAN_MILLISECOND;
  while (g_get_monotonic_time () < end) {
    while (g_main_context_iteration (NULL, FALSE));
    g_usleep (1000);
  }
}

static GListModel *
make_items (const char *first, ...)
{
  GListStore *store = g_list_store_new (AWRA_TYPE_BREADCRUMB_ITEM);
  va_list args;

  va_start (args, first);
  for (const char *name = first; name != NULL; name = va_arg (args, const char *)) {
    g_autoptr (AwraBreadcrumbItem) item = awra_breadcrumb_item_new (name, name);
    g_list_store_append (store, item);
  }
  va_end (args);
  return G_LIST_MODEL (store);
}

static gboolean accepted;
static char *submitted;

static gboolean
submit_cb (AwraPathBar *bar, const char *text, gpointer data)
{
  (void) bar; (void) data;
  g_free (submitted);
  submitted = g_strdup (text);
  return accepted;
}

static void
clear_suggestions_cb (AwraPathBar *bar, GParamSpec *pspec, gpointer data)
{
  (void) pspec; (void) data;
  awra_path_bar_set_suggestions (bar, NULL);
}

static GtkText *
find_text (GtkWidget *widget)
{
  if (GTK_IS_TEXT (widget))
    return GTK_TEXT (widget);
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    GtkText *found = find_text (child);
    if (found != NULL)
      return found;
  }
  return NULL;
}

static void
path_bar_contract (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *widget = awra_path_bar_new ();
  AwraPathBar *bar = AWRA_PATH_BAR (widget);
  GtkWidget *end = gtk_button_new ();
  g_autoptr (GListModel) trail = make_items ("/", "/home", NULL);
  g_autoptr (GListModel) first = make_items ("/home/a/", "/home/b/", NULL);
  g_autoptr (GListModel) second = make_items ("/home/c/", NULL);
  GtkText *text;

  g_signal_connect (bar, "submit", G_CALLBACK (submit_cb), NULL);
  awra_path_bar_set_model (bar, trail);
  g_assert_true (awra_path_bar_get_model (bar) == trail);
  /* Nothing is under an unallocated bar, and marking no item is always valid. */
  g_assert_null (awra_path_bar_get_item_at (bar, 4, 4));
  awra_path_bar_set_drop_item (bar, NULL);
  awra_path_bar_set_drop_item (bar, g_list_model_get_item (trail, 0));
  awra_path_bar_set_text (bar, "/home");
  awra_path_bar_set_icon_name (bar, "folder-symbolic");
  awra_path_bar_set_end_widget (bar, end);
  awra_path_bar_set_placeholder_text (bar, "Path");
  g_assert_cmpstr (awra_path_bar_get_placeholder_text (bar), ==, "Path");
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));
  spin ();

  text = find_text (widget);
  g_assert_nonnull (text);
  g_assert_false (awra_path_bar_get_editing (bar));
  g_assert_cmpstr (awra_path_bar_get_edit_text (bar), ==, "");

  /* Editing starts from the committed location, fully selected. */
  awra_path_bar_set_editing (bar, TRUE);
  g_assert_cmpstr (awra_path_bar_get_edit_text (bar), ==, "/home");
  g_assert_true (gtk_widget_has_css_class (widget, "awra-path-bar-editing"));
  g_assert_true (gtk_widget_get_visible (GTK_WIDGET (text)));

  /* A committed location changing underneath does not disturb the typing. */
  gtk_editable_set_text (GTK_EDITABLE (text), "/ho");
  awra_path_bar_set_text (bar, "/var");
  g_assert_cmpstr (awra_path_bar_get_edit_text (bar), ==, "/ho");

  /* A refused location stays editable and marked. */
  accepted = FALSE;
  g_signal_emit_by_name (text, "activate");
  g_assert_cmpstr (submitted, ==, "/ho");
  g_assert_true (awra_path_bar_get_editing (bar));
  g_assert_true (gtk_widget_has_css_class (widget, "awra-path-bar-invalid"));
  gtk_editable_set_text (GTK_EDITABLE (text), "/home");
  g_assert_false (gtk_widget_has_css_class (widget, "awra-path-bar-invalid"));

  /* An accepted location closes the field and drops its text. */
  accepted = TRUE;
  g_signal_emit_by_name (text, "activate");
  g_assert_false (awra_path_bar_get_editing (bar));
  g_assert_cmpstr (awra_path_bar_get_edit_text (bar), ==, "");

  /* The first typed character can open the field and remain the input. */
  awra_path_bar_begin_edit (bar, "/");
  g_assert_true (awra_path_bar_get_editing (bar));
  g_assert_cmpstr (awra_path_bar_get_edit_text (bar), ==, "/");

  /* Suggestions may be replaced, mutated or cleared at any time. */
  awra_path_bar_set_suggestions (bar, first);
  g_assert_true (awra_path_bar_get_suggestions (bar) == first);
  spin ();
  awra_path_bar_set_suggestions (bar, second);
  g_list_store_remove (G_LIST_STORE (second), 0);
  awra_path_bar_set_suggestions (bar, NULL);
  g_assert_null (awra_path_bar_get_suggestions (bar));

  /* Observers usually replace the list while a suggestion is being inserted. */
  {
    g_autoptr (GListModel) owned = make_items ("/home/inserted/", NULL);
    GtkWidget *popover = gtk_widget_get_last_child (widget);
    GtkWidget *row = popover;
    gulong handler = g_signal_connect (bar, "notify::edit-text",
                                       G_CALLBACK (clear_suggestions_cb), NULL);

    awra_path_bar_set_suggestions (bar, owned);
    while (row != NULL && !GTK_IS_BUTTON (row))
      row = gtk_widget_get_first_child (row);
    g_assert_nonnull (row);
    g_clear_pointer (&submitted, g_free);
    g_signal_emit_by_name (row, "clicked");
    g_assert_null (submitted);
    g_assert_true (awra_path_bar_get_editing (bar));
    g_assert_cmpstr (awra_path_bar_get_edit_text (bar), ==, "/home/inserted/");
    g_signal_handler_disconnect (bar, handler);
  }

  awra_path_bar_set_suggestions (bar, first);
  awra_path_bar_set_end_widget (bar, NULL);
  awra_path_bar_set_icon_name (bar, NULL);
  awra_path_bar_set_editing (bar, FALSE);
  awra_path_bar_set_editing (bar, TRUE);
  gtk_window_destroy (GTK_WINDOW (window));
  spin ();
  g_clear_pointer (&submitted, g_free);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  /* GDK warns about frame timings on virtual displays, whatever the widget does. */
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);
  if (!gtk_init_check ())
    return 77;
  awra_init ();
  g_test_add_func ("/awra/path-bar/contract", path_bar_contract);
  return g_test_run ();
}
