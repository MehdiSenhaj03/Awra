/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <awra/awra.h>

static guint setups;
static void setup (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
  (void) factory; (void) data;
  setups++;
  gtk_list_item_set_child (item, gtk_label_new ("Collection item"));
}

static void spin (void)
{
  gint64 end = g_get_monotonic_time () + 300 * G_TIME_SPAN_MILLISECOND;
  while (g_get_monotonic_time () < end) {
    while (g_main_context_iteration (NULL, FALSE));
    g_usleep (1000);
  }
}

static void grid_contract (void)
{
  g_autoptr (GtkStringList) items = gtk_string_list_new (NULL);
  g_autoptr (GtkMultiSelection) selection = NULL;
  g_autoptr (GtkListItemFactory) factory = gtk_signal_list_item_factory_new ();
  GtkWidget *window = gtk_window_new ();
  GtkWidget *grid;
  for (guint i = 0; i < 20000; i++)
    gtk_string_list_append (items, "Item");
  selection = gtk_multi_selection_new (G_LIST_MODEL (g_object_ref (items)));
  g_signal_connect (factory, "setup", G_CALLBACK (setup), NULL);
  grid = awra_grid_view_new (GTK_SELECTION_MODEL (selection), factory);
  g_assert_true (awra_grid_view_get_model (AWRA_GRID_VIEW (grid)) == GTK_SELECTION_MODEL (selection));
  g_assert_true (awra_grid_view_get_factory (AWRA_GRID_VIEW (grid)) == factory);
  g_assert_true (gtk_grid_view_get_enable_rubberband (awra_grid_view_get_delegate (AWRA_GRID_VIEW (grid))));
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  gtk_window_set_child (GTK_WINDOW (window), grid);
  gtk_window_present (GTK_WINDOW (window));
  spin ();
  g_assert_cmpuint (setups, >, 0);
  g_assert_cmpuint (setups, <, 2000); /* GTK recycles a bounded viewport. */
  gtk_selection_model_select_item (GTK_SELECTION_MODEL (selection), 19999, FALSE);
  g_assert_true (gtk_selection_model_is_selected (GTK_SELECTION_MODEL (selection), 19999));
  awra_grid_view_set_model (AWRA_GRID_VIEW (grid), NULL);
  g_assert_null (awra_grid_view_get_model (AWRA_GRID_VIEW (grid)));
  awra_grid_view_set_factory (AWRA_GRID_VIEW (grid), NULL);
  g_assert_null (awra_grid_view_get_factory (AWRA_GRID_VIEW (grid)));
  gtk_window_destroy (GTK_WINDOW (window));
  spin ();
}

static void breadcrumb_overflow (void)
{
  g_autoptr (GListStore) model = g_list_store_new (AWRA_TYPE_BREADCRUMB_ITEM);
  GtkWidget *window = gtk_window_new ();
  GtkWidget *bar = awra_breadcrumb_bar_new ();
  for (guint i = 0; i < 30; i++) {
    g_autoptr (AwraBreadcrumbItem) item = awra_breadcrumb_item_new ("A long directory name", "directory");
    g_list_store_append (model, item);
  }
  awra_breadcrumb_bar_set_model (AWRA_BREADCRUMB_BAR (bar), G_LIST_MODEL (model));
  gtk_window_set_default_size (GTK_WINDOW (window), 320, 100);
  gtk_window_set_child (GTK_WINDOW (window), bar);
  gtk_window_present (GTK_WINDOW (window));
  spin ();
  g_assert_cmpint (gtk_widget_get_width (bar), <=, 320);
  GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW (gtk_widget_get_first_child (bar));
  GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (scroller);
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), >, 0);
  gtk_widget_set_direction (bar, GTK_TEXT_DIR_RTL);
  g_signal_emit_by_name (adjustment, "changed");
  spin ();
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), ==, gtk_adjustment_get_lower (adjustment));
  gtk_window_destroy (GTK_WINDOW (window));
  spin ();
}

static void text_area_contrast (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *area = awra_text_area_new ();
  GtkWidget *delegate = GTK_WIDGET (awra_text_area_get_delegate (AWRA_TEXT_AREA (area)));
  AwraStyleManager *manager = awra_context_get_style_manager (
    awra_context_get_for_display (gdk_display_get_default ()));
  awra_text_area_set_text (AWRA_TEXT_AREA (area), "Read-only file preview");
  gtk_text_view_set_editable (GTK_TEXT_VIEW (delegate), FALSE);
  gtk_window_set_child (GTK_WINDOW (window), area);
  gtk_window_present (GTK_WINDOW (window));
  for (guint i = 0; i < 2; i++) {
    GdkRGBA actual;
    awra_style_manager_set_appearance (manager, i == 0 ? AWRA_APPEARANCE_DARK : AWRA_APPEARANCE_LIGHT);
    spin ();
    const GdkRGBA *expected = awra_token_set_get_foreground (
      awra_style_manager_get_token_set (manager));
    gtk_widget_get_color (delegate, &actual);
    g_assert_cmpfloat_with_epsilon (actual.red, expected->red, .01);
    g_assert_cmpfloat_with_epsilon (actual.green, expected->green, .01);
    g_assert_cmpfloat_with_epsilon (actual.blue, expected->blue, .01);
  }
  awra_style_manager_set_appearance (manager, AWRA_APPEARANCE_SYSTEM);
  gtk_window_destroy (GTK_WINDOW (window));
  spin ();
}

static void compact_workspace_contract (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *header = awra_header_new ();
  GtkWidget *root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *toolbar = awra_selection_toolbar_new (NULL);
  awra_selection_toolbar_set_auto_hide (AWRA_SELECTION_TOOLBAR (toolbar), FALSE);
  awra_selection_toolbar_set_clear_label (AWRA_SELECTION_TOOLBAR (toolbar), "Dismiss selection");
  g_assert_cmpstr (awra_selection_toolbar_get_clear_label (AWRA_SELECTION_TOOLBAR (toolbar)), ==, "Dismiss selection");
  awra_header_set_title (AWRA_HEADER (header), "Workspace");
  gtk_box_append (GTK_BOX (root), header);
  gtk_box_append (GTK_BOX (root), toolbar);
  gtk_window_set_child (GTK_WINDOW (window), root);
  gtk_window_present (GTK_WINDOW (window));
  spin ();
  int regular_height = gtk_widget_get_height (header);
  awra_header_set_compact (AWRA_HEADER (header), TRUE);
  spin ();
  g_assert_true (awra_header_get_compact (AWRA_HEADER (header)));
  g_assert_cmpint (gtk_widget_get_height (header), <, regular_height);
  g_assert_cmpint (gtk_widget_get_height (header), >=, 32);
  awra_selection_toolbar_set_clear_label (AWRA_SELECTION_TOOLBAR (toolbar), NULL);
  g_assert_nonnull (awra_selection_toolbar_get_clear_label (AWRA_SELECTION_TOOLBAR (toolbar)));
  gtk_window_destroy (GTK_WINDOW (window));
  spin ();
}

static void narrow_metadata (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *row = awra_metric_row_new ("Location", "A very long metadata value that must fit inside a compact inspector pane");
  gtk_window_set_default_size (GTK_WINDOW (window), 260, 80);
  gtk_window_set_child (GTK_WINDOW (window), row);
  gtk_window_present (GTK_WINDOW (window));
  spin ();
  g_assert_cmpint (gtk_widget_get_width (row), <=, 260);
  GtkWidget *root = gtk_widget_get_first_child (row);
  GtkWidget *value = gtk_widget_get_last_child (root);
  graphene_rect_t bounds;
  g_assert_true (gtk_widget_compute_bounds (value, row, &bounds));
  g_assert_cmpfloat (bounds.origin.x + bounds.size.width, <=, gtk_widget_get_width (row));
  g_assert_true (gtk_label_get_layout (GTK_LABEL (value)) != NULL);
  gtk_window_destroy (GTK_WINDOW (window));
  spin ();
}

static void tab_placeholder_lifecycle (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *tabs = awra_tab_view_new ();
  GtkWidget *first = gtk_label_new ("First");
  GtkWidget *second = gtk_label_new ("Second");
  awra_tab_view_append (AWRA_TAB_VIEW (tabs), first, "First", FALSE);
  awra_tab_view_append (AWRA_TAB_VIEW (tabs), second, "Second", FALSE);
  gtk_window_set_child (GTK_WINDOW (window), tabs);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 200);
  gtk_window_present (GTK_WINDOW (window));
  spin ();
  g_assert_cmpuint (awra_tab_view_get_insertion_position (AWRA_TAB_VIEW (tabs), 0.0), ==, 0);
  g_assert_cmpuint (awra_tab_view_get_insertion_position (AWRA_TAB_VIEW (tabs), 5000.0), ==, 2);
  awra_tab_view_set_page_dragged (AWRA_TAB_VIEW (tabs), first, TRUE);
  g_assert_cmpuint (awra_tab_view_get_insertion_position (AWRA_TAB_VIEW (tabs), 5000.0), ==, 1);
  awra_tab_view_show_placeholder (AWRA_TAB_VIEW (tabs), 1);
  spin ();
  /* Removing a lifted page and closing mid-animation must both be safe. */
  awra_tab_view_remove (AWRA_TAB_VIEW (tabs), first);
  awra_tab_view_show_placeholder (AWRA_TAB_VIEW (tabs), 0);
  awra_tab_view_hide_placeholder (AWRA_TAB_VIEW (tabs));
  gtk_window_destroy (GTK_WINDOW (window));
  spin ();
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ()) return 77;
  awra_init ();
  g_test_add_func ("/awra/grid/virtualization-model-lifecycle", grid_contract);
  g_test_add_func ("/awra/tabs/placeholder-lifecycle", tab_placeholder_lifecycle);
  g_test_add_func ("/awra/breadcrumb/overflow", breadcrumb_overflow);
  g_test_add_func ("/awra/text-area/readonly-contrast", text_area_contrast);
  g_test_add_func ("/awra/workspace/compact-controls", compact_workspace_contract);
  g_test_add_func ("/awra/workspace/narrow-metadata", narrow_metadata);
  return g_test_run ();
}
