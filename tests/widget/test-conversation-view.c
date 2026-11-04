/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>

#include <awra/awra.h>

typedef struct {
  guint setups;
  guint binds;
  guint unbinds;
  guint teardowns;
  GPtrArray *bound;
} Rows;

static void
setup_cb (GtkSignalListItemFactory *factory,
          GtkListItem              *item,
          Rows                     *rows)
{
  GtkWidget *label = gtk_label_new (NULL);

  (void) factory;
  /* Rows of different heights, like messages. */
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);
  gtk_list_item_set_child (item, label);
  rows->setups++;
}

static void
bind_cb (GtkSignalListItemFactory *factory,
         GtkListItem              *item,
         Rows                     *rows)
{
  GtkStringObject *string = gtk_list_item_get_item (item);

  (void) factory;
  gtk_label_set_text (GTK_LABEL (gtk_list_item_get_child (item)),
                      gtk_string_object_get_string (string));
  g_ptr_array_add (rows->bound, item);
  rows->binds++;
}

static void
unbind_cb (GtkSignalListItemFactory *factory,
           GtkListItem              *item,
           Rows                     *rows)
{
  (void) factory;
  g_ptr_array_remove (rows->bound, item);
  rows->unbinds++;
}

static void
teardown_cb (GtkSignalListItemFactory *factory,
             GtkListItem              *item,
             Rows                     *rows)
{
  (void) factory;
  (void) item;
  rows->teardowns++;
}

static GtkListItemFactory *
make_factory (Rows *rows)
{
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();

  rows->bound = g_ptr_array_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_cb), rows);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_cb), rows);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cb), rows);
  g_signal_connect (factory, "teardown", G_CALLBACK (teardown_cb), rows);
  return factory;
}

static GtkStringList *
make_strings (const char *prefix,
              guint       count)
{
  g_autoptr (GPtrArray) strings = g_ptr_array_new_with_free_func (g_free);

  for (guint i = 0; i < count; i++)
    g_ptr_array_add (strings, i % 5 == 2
                       ? g_strdup_printf ("%s %u\nwith a second line", prefix, i)
                       : g_strdup_printf ("%s %u", prefix, i));
  g_ptr_array_add (strings, NULL);
  return gtk_string_list_new ((const char * const *) strings->pdata);
}

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

static GtkAdjustment *
vadjustment (AwraConversationView *view)
{
  return gtk_scrollable_get_vadjustment (
    GTK_SCROLLABLE (awra_conversation_view_get_delegate (view)));
}

static double
distance_to_end (AwraConversationView *view)
{
  GtkAdjustment *adjustment = vadjustment (view);

  return gtk_adjustment_get_upper (adjustment) -
         gtk_adjustment_get_page_size (adjustment) -
         gtk_adjustment_get_value (adjustment);
}

static GtkWidget *
row_for (Rows  *rows,
         guint  position)
{
  for (guint i = 0; i < rows->bound->len; i++) {
    GtkListItem *item = g_ptr_array_index (rows->bound, i);

    if (gtk_list_item_get_position (item) == position)
      return gtk_widget_get_parent (gtk_list_item_get_child (item));
  }
  return NULL;
}

static GtkWidget *
present (AwraConversationView *view)
{
  GtkWidget *window = gtk_window_new ();

  gtk_window_set_default_size (GTK_WINDOW (window), 360, 300);
  gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (view));
  gtk_window_present (GTK_WINDOW (window));
  run (250);
  return window;
}

static void
test_properties (void)
{
  GtkWidget *widget = g_object_ref_sink (awra_conversation_view_new (NULL, NULL));
  AwraConversationView *view = AWRA_CONVERSATION_VIEW (widget);
  g_autoptr (GtkStringList) strings = make_strings ("item", 3);
  g_autoptr (GtkListItemFactory) headers = gtk_signal_list_item_factory_new ();
  Rows rows = { 0 };
  g_autoptr (GtkListItemFactory) factory = make_factory (&rows);
  g_autofree char *label = NULL;
  guint first = 0;
  guint last = 0;
  int threshold = 0;

  g_assert_null (awra_conversation_view_get_model (view));
  g_assert_null (awra_conversation_view_get_factory (view));
  g_assert_true (GTK_IS_LIST_VIEW (awra_conversation_view_get_delegate (view)));
  g_assert_true (awra_conversation_view_get_follow (view));
  g_assert_true (awra_conversation_view_get_at_bottom (view));
  g_assert_cmpint (awra_conversation_view_get_edge_threshold (view), ==, -1);
  g_assert_false (awra_conversation_view_get_visible_range (view, &first, &last));
  g_assert_cmpuint (first, ==, GTK_INVALID_LIST_POSITION);

  awra_conversation_view_set_model (view, G_LIST_MODEL (strings));
  awra_conversation_view_set_factory (view, factory);
  awra_conversation_view_set_header_factory (view, headers);
  g_assert_true (awra_conversation_view_get_model (view) == G_LIST_MODEL (strings));
  g_assert_true (awra_conversation_view_get_factory (view) == factory);
  g_assert_true (awra_conversation_view_get_header_factory (view) == headers);
  g_assert_true (gtk_list_view_get_header_factory (
    awra_conversation_view_get_delegate (view)) == headers);

  awra_conversation_view_set_follow (view, FALSE);
  g_assert_false (awra_conversation_view_get_follow (view));
  awra_conversation_view_set_unread_count (view, 4);
  g_assert_cmpuint (awra_conversation_view_get_unread_count (view), ==, 4);
  awra_conversation_view_set_jump_label (view, "Back to the live log");
  g_assert_cmpstr (awra_conversation_view_get_jump_label (view), ==, "Back to the live log");
  awra_conversation_view_set_jump_label (view, NULL);
  g_assert_null (awra_conversation_view_get_jump_label (view));
  awra_conversation_view_set_edge_threshold (view, 120);
  g_object_get (view, "edge-threshold", &threshold, "jump-label", &label, NULL);
  g_assert_cmpint (threshold, ==, 120);
  g_assert_null (label);

  /* A new model is shown from its end. */
  awra_conversation_view_set_model (view, NULL);
  g_assert_true (awra_conversation_view_get_follow (view));
  awra_conversation_view_set_factory (view, NULL);
  g_object_unref (widget);
  g_ptr_array_unref (rows.bound);
}

static void
test_factory_forwarding (void)
{
  g_autoptr (GtkStringList) strings = make_strings ("message", 20);
  Rows rows = { 0 };
  GtkListItemFactory *factory = make_factory (&rows);
  GtkWidget *widget = awra_conversation_view_new (G_LIST_MODEL (g_object_ref (strings)), factory);
  GtkWidget *window = present (AWRA_CONVERSATION_VIEW (widget));

  g_assert_cmpuint (rows.setups, >, 0);
  g_assert_cmpuint (rows.binds, >, 0);
  g_assert_cmpuint (rows.bound->len, >, 0);
  /* The last item is shown first, at the bottom. */
  g_assert_nonnull (row_for (&rows, 19));
  g_assert_cmpfloat (distance_to_end (AWRA_CONVERSATION_VIEW (widget)), <=, 1.0);
  gtk_window_destroy (GTK_WINDOW (window));
  run (20);
  /* Every row was given back to the application's factory. */
  g_assert_cmpuint (rows.binds, ==, rows.unbinds);
  g_assert_cmpuint (rows.setups, ==, rows.teardowns);
  g_ptr_array_unref (rows.bound);
}

static void
test_builder_factory (void)
{
  static const char ui[] =
    "<interface>"
    "  <template class=\"GtkListItem\">"
    "    <property name=\"child\">"
    "      <object class=\"GtkLabel\">"
    "        <binding name=\"label\">"
    "          <lookup name=\"string\" type=\"GtkStringObject\">"
    "            <lookup name=\"item\">GtkListItem</lookup>"
    "          </lookup>"
    "        </binding>"
    "      </object>"
    "    </property>"
    "  </template>"
    "</interface>";
  g_autoptr (GBytes) bytes = g_bytes_new_static (ui, sizeof ui - 1);
  g_autoptr (GtkStringList) strings = make_strings ("line", 8);
  GtkWidget *widget = awra_conversation_view_new (
    G_LIST_MODEL (g_object_ref (strings)),
    gtk_builder_list_item_factory_new_from_bytes (NULL, bytes));
  GtkWidget *window = present (AWRA_CONVERSATION_VIEW (widget));
  GtkWidget *row = gtk_widget_get_last_child (
    GTK_WIDGET (awra_conversation_view_get_delegate (AWRA_CONVERSATION_VIEW (widget))));
  guint first = 0;
  guint last = 0;

  g_assert_nonnull (row);
  g_assert_true (GTK_IS_LABEL (gtk_widget_get_first_child (row)));
  g_assert_true (g_str_has_prefix (gtk_label_get_label (GTK_LABEL (gtk_widget_get_first_child (row))), "line 7"));
  g_assert_true (awra_conversation_view_get_visible_range (AWRA_CONVERSATION_VIEW (widget), &first, &last));
  g_assert_cmpuint (first, ==, 0);
  g_assert_cmpuint (last, ==, 7);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
count_edge (AwraConversationView *view,
            GtkPositionType       edge,
            guint                *counts)
{
  (void) view;
  counts[edge == GTK_POS_TOP ? 0 : 1]++;
}

static void
count_signal (AwraConversationView *view,
              guint                *count)
{
  (void) view;
  (*count)++;
}

static void
test_virtualized_timeline (void)
{
  g_autoptr (GtkStringList) strings = make_strings ("message", 50000);
  Rows rows = { 0 };
  GtkWidget *widget = awra_conversation_view_new (G_LIST_MODEL (g_object_ref (strings)),
                                                  make_factory (&rows));
  AwraConversationView *view = AWRA_CONVERSATION_VIEW (widget);
  guint edges[2] = { 0, 0 };
  guint ranges = 0;
  GtkWidget *window;
  GtkWidget *row;
  graphene_point_t before;
  graphene_point_t after;
  guint first = 0;
  guint last = 0;

  g_signal_connect (view, "edge-reached", G_CALLBACK (count_edge), edges);
  g_signal_connect (view, "visible-range-changed", G_CALLBACK (count_signal), &ranges);
  window = present (view);

  /* Virtualized: a screenful of rows, not fifty thousand. */
  g_assert_cmpuint (rows.setups, <, 500);
  g_assert_true (awra_conversation_view_get_at_bottom (view));
  g_assert_true (awra_conversation_view_get_visible_range (view, &first, &last));
  g_assert_cmpuint (last, ==, 49999);
  g_assert_cmpuint (ranges, >, 0);
  g_assert_cmpuint (edges[1], ==, 1);
  g_assert_cmpuint (edges[0], ==, 0);

  /* Appending while following keeps the newest item in view. */
  gtk_string_list_append (strings, "fresh");
  run (400);
  g_assert_true (awra_conversation_view_get_follow (view));
  g_assert_true (awra_conversation_view_get_visible_range (view, &first, &last));
  g_assert_cmpuint (last, ==, 50000);
  g_assert_cmpfloat (distance_to_end (view), <=, 1.0);

  /* The user scrolls up: following stops, the pill appears. */
  gtk_adjustment_set_value (vadjustment (view),
                            gtk_adjustment_get_value (vadjustment (view)) - 200);
  run (100);
  g_assert_false (awra_conversation_view_get_follow (view));
  g_assert_false (awra_conversation_view_get_at_bottom (view));
  g_assert_true (awra_conversation_view_get_visible_range (view, &first, &last));

  /* Appending no longer moves the view. */
  gtk_string_list_append (strings, "while away");
  run (200);
  {
    guint first_now = 0;
    guint last_now = 0;

    g_assert_true (awra_conversation_view_get_visible_range (view, &first_now, &last_now));
    g_assert_cmpuint (first_now, ==, first);
    g_assert_cmpuint (last_now, ==, last);
  }

  /* Older history inserted above leaves what is on screen in place. */
  row = row_for (&rows, first + 1);
  g_assert_nonnull (row);
  g_assert_true (gtk_widget_compute_point (row, window, &GRAPHENE_POINT_INIT (0, 0), &before));
  {
    g_autoptr (GtkStringList) older = make_strings ("older", 100);
    g_autofree const char **items = g_new0 (const char *, 101);

    for (guint i = 0; i < 100; i++)
      items[i] = gtk_string_list_get_string (older, i);
    gtk_string_list_splice (strings, 0, 0, items);
  }
  run (200);
  g_assert_true (row == row_for (&rows, first + 101));
  g_assert_true (gtk_widget_compute_point (row, window, &GRAPHENE_POINT_INIT (0, 0), &after));
  g_assert_cmpfloat (fabs (after.y - before.y), <, 0.5);
  g_assert_true (awra_conversation_view_get_visible_range (view, &first, &last));
  g_assert_cmpuint (first, >=, 100);

  /* Back to the present through the pill. */
  g_signal_emit_by_name (view, "jump-activated");
  run (600);
  g_assert_true (awra_conversation_view_get_follow (view));
  g_assert_true (awra_conversation_view_get_at_bottom (view));
  g_assert_cmpfloat (distance_to_end (view), <=, 1.0);

  gtk_window_destroy (GTK_WINDOW (window));
  g_ptr_array_unref (rows.bound);
}

static void
test_scroll_to_item (void)
{
  g_autoptr (GtkStringList) strings = make_strings ("message", 2000);
  Rows rows = { 0 };
  GtkWidget *widget = awra_conversation_view_new (G_LIST_MODEL (g_object_ref (strings)),
                                                  make_factory (&rows));
  AwraConversationView *view = AWRA_CONVERSATION_VIEW (widget);
  GtkWidget *window = present (view);
  GtkWidget *row;
  graphene_rect_t bounds;
  guint edges[2] = { 0, 0 };
  guint first = 0;
  guint last = 0;
  double height;

  g_signal_connect (view, "edge-reached", G_CALLBACK (count_edge), edges);
  /* Far away: the list jumps there and the item is centred. */
  awra_conversation_view_scroll_to_item (view, 700, AWRA_CONVERSATION_ALIGN_CENTER, TRUE);
  g_assert_false (awra_conversation_view_get_follow (view));
  run (300);
  row = row_for (&rows, 700);
  g_assert_nonnull (row);
  g_assert_true (gtk_widget_has_css_class (row, "awra-conversation-highlight"));
  height = gtk_widget_get_height (GTK_WIDGET (awra_conversation_view_get_delegate (view)));
  g_assert_true (gtk_widget_compute_bounds (row, GTK_WIDGET (awra_conversation_view_get_delegate (view)),
                                            &bounds));
  g_assert_cmpfloat (fabs (bounds.origin.y + bounds.size.height / 2 - height / 2), <=, 1.0);

  /* Highlighting again replays the flash. */
  awra_conversation_view_scroll_to_item (view, 700, AWRA_CONVERSATION_ALIGN_TOP, TRUE);
  g_assert_true (gtk_widget_has_css_class (row, "awra-conversation-highlight-again"));
  g_assert_false (gtk_widget_has_css_class (row, "awra-conversation-highlight"));
  run (400);
  g_assert_true (gtk_widget_compute_bounds (row, GTK_WIDGET (awra_conversation_view_get_delegate (view)),
                                            &bounds));
  g_assert_cmpfloat (fabs (bounds.origin.y), <=, 1.0);

  /* Home reaches the start, which asks for older items. */
  awra_conversation_view_scroll_to_item (view, 0, AWRA_CONVERSATION_ALIGN_TOP, FALSE);
  run (300);
  g_assert_true (awra_conversation_view_get_visible_range (view, &first, &last));
  g_assert_cmpuint (first, ==, 0);
  g_assert_cmpuint (edges[0], ==, 1);

  /* The highlight leaves on its own. */
  run (1400);
  g_assert_false (gtk_widget_has_css_class (row, "awra-conversation-highlight-again"));

  awra_conversation_view_scroll_to_end (view, FALSE);
  g_assert_true (awra_conversation_view_get_follow (view));
  run (100);
  g_assert_cmpfloat (distance_to_end (view), <=, 1.0);
  gtk_window_destroy (GTK_WINDOW (window));
  g_ptr_array_unref (rows.bound);
}

static void
test_arrival (void)
{
  g_autoptr (GtkStringList) strings = make_strings ("message", 40);
  Rows rows = { 0 };
  GtkWidget *widget = awra_conversation_view_new (G_LIST_MODEL (g_object_ref (strings)),
                                                  make_factory (&rows));
  AwraConversationView *view = AWRA_CONVERSATION_VIEW (widget);
  GtkWidget *window = present (view);
  GtkWidget *row;
  guint arriving = 0;

  gtk_string_list_append (strings, "arrived");
  run (120);
  row = row_for (&rows, 40);
  g_assert_nonnull (row);
  g_assert_true (gtk_widget_has_css_class (row, "awra-conversation-arriving"));
  /* Rows that were already there do not rise again. */
  for (guint i = 0; i < 40; i++) {
    GtkWidget *other = row_for (&rows, i);

    if (other != NULL && gtk_widget_has_css_class (other, "awra-conversation-arriving"))
      arriving++;
  }
  g_assert_cmpuint (arriving, ==, 0);
  run (500);
  g_assert_false (gtk_widget_has_css_class (row, "awra-conversation-arriving"));
  g_assert_cmpfloat (distance_to_end (view), <=, 1.0);

  /* Destroyed in the middle of a glide and an arrival. */
  for (guint i = 0; i < 5; i++)
    gtk_string_list_append (strings, "burst");
  run (40);
  gtk_window_destroy (GTK_WINDOW (window));
  run (20);
  g_ptr_array_unref (rows.bound);
}

static void
test_reduced_motion (void)
{
  AwraStyleManager *style = awra_context_get_style_manager (
    awra_context_get_for_display (gdk_display_get_default ()));
  g_autoptr (GtkStringList) strings = make_strings ("message", 400);
  Rows rows = { 0 };
  GtkWidget *widget = awra_conversation_view_new (G_LIST_MODEL (g_object_ref (strings)),
                                                  make_factory (&rows));
  AwraConversationView *view = AWRA_CONVERSATION_VIEW (widget);
  GtkWidget *window;

  awra_style_manager_set_reduced_motion (style, TRUE);
  window = present (view);
  gtk_adjustment_set_value (vadjustment (view), 0);
  run (60);
  g_assert_false (awra_conversation_view_get_at_bottom (view));
  /* The glide jumps to its end. */
  awra_conversation_view_scroll_to_end (view, TRUE);
  g_assert_true (awra_conversation_view_get_follow (view));
  run (60);
  g_assert_cmpfloat (distance_to_end (view), <=, 1.0);
  gtk_string_list_append (strings, "no rise");
  run (60);
  g_assert_false (gtk_widget_has_css_class (row_for (&rows, 400), "awra-conversation-arriving"));
  g_assert_cmpfloat (distance_to_end (view), <=, 1.0);
  awra_style_manager_set_reduced_motion (style, FALSE);
  gtk_window_destroy (GTK_WINDOW (window));
  g_ptr_array_unref (rows.bound);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/conversation-view/properties", test_properties);
  g_test_add_func ("/conversation-view/factory-forwarding", test_factory_forwarding);
  g_test_add_func ("/conversation-view/builder-factory", test_builder_factory);
  g_test_add_func ("/conversation-view/virtualized-timeline", test_virtualized_timeline);
  g_test_add_func ("/conversation-view/scroll-to-item", test_scroll_to_item);
  g_test_add_func ("/conversation-view/arrival", test_arrival);
  g_test_add_func ("/conversation-view/reduced-motion", test_reduced_motion);
  return g_test_run ();
}
