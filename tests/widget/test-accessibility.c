/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static void
test_roles_keyboard_and_rtl (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestAccessibility", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *root;
  GtkWidget *page;
  GtkWidget *content;
  GtkWidget *navigation;
  GtkWidget *form;
  GtkWidget *row;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *stat;
  GtkWidget *edge;
  GtkWidget *panel;
  graphene_rect_t ltr_bounds;
  graphene_rect_t rtl_bounds;

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  page = awra_page_new ();
  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  navigation = awra_navigation_item_new_with_label (
    "A deliberately long translated navigation destination");
  form = awra_form_new ();
  row = awra_form_row_new ();
  entry = awra_entry_new ();
  button = awra_button_new_with_label ("Continue");
  stat = awra_stat_bar_new ("Accessible metric");
  edge = awra_edge_panel_new ();
  panel = gtk_label_new ("Inspector");

  awra_form_row_set_label (AWRA_FORM_ROW (row), "Workspace name");
  awra_form_row_set_help_text (AWRA_FORM_ROW (row),
                               "Supports IME, selection and clipboard");
  awra_form_row_set_control (AWRA_FORM_ROW (row), entry);
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (row));
  awra_stat_bar_set_maximum (AWRA_STAT_BAR (stat), 255.0);
  awra_stat_bar_set_value (AWRA_STAT_BAR (stat), 45.0);
  awra_box_apply_layout_preset (GTK_BOX (content), AWRA_LAYOUT_PRESET_PAGE);
  gtk_box_append (GTK_BOX (content), navigation);
  gtk_box_append (GTK_BOX (content), form);
  gtk_box_append (GTK_BOX (content), stat);
  gtk_box_append (GTK_BOX (content), button);
  awra_page_set_child (AWRA_PAGE (page), content);
  awra_edge_panel_set_content (AWRA_EDGE_PANEL (edge), page);
  awra_edge_panel_set_panel (AWRA_EDGE_PANEL (edge), panel);
  awra_edge_panel_set_edge (AWRA_EDGE_PANEL (edge), AWRA_PANEL_EDGE_START);
  awra_edge_panel_set_panel_width (AWRA_EDGE_PANEL (edge), 220);
  awra_edge_panel_set_revealed (AWRA_EDGE_PANEL (edge), TRUE);
  gtk_box_append (GTK_BOX (root), edge);
  awra_window_set_content (AWRA_WINDOW (window), root);
  gtk_window_set_default_size (GTK_WINDOW (window), 760, 560);
  gtk_window_present (GTK_WINDOW (window));
  gtk_test_widget_wait_for_draw (window);

  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (page)),
                   ==, GTK_ACCESSIBLE_ROLE_MAIN);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (navigation)), ==,
                   GTK_ACCESSIBLE_ROLE_BUTTON);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (entry)),
                   ==, GTK_ACCESSIBLE_ROLE_TEXT_BOX);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (stat)),
                   ==, GTK_ACCESSIBLE_ROLE_METER);
  g_assert_true (gtk_widget_grab_focus (entry));
  {
    GtkWidget *focus = gtk_root_get_focus (GTK_ROOT (window));
    g_assert_nonnull (focus);
    g_assert_true (focus == entry || gtk_widget_is_ancestor (focus, entry));
  }
  g_assert_true (gtk_widget_child_focus (root, GTK_DIR_TAB_FORWARD));

  g_assert_true (gtk_widget_compute_bounds (
    GTK_WIDGET (awra_edge_panel_get_surface (AWRA_EDGE_PANEL (edge))), edge,
    &ltr_bounds));
  gtk_widget_set_direction (edge, GTK_TEXT_DIR_RTL);
  gtk_widget_queue_allocate (edge);
  gtk_test_widget_wait_for_draw (window);
  g_assert_true (gtk_widget_compute_bounds (
    GTK_WIDGET (awra_edge_panel_get_surface (AWRA_EDGE_PANEL (edge))), edge,
    &rtl_bounds));
  g_assert_cmpfloat (rtl_bounds.origin.x, >, ltr_bounds.origin.x);
  g_assert_cmpint (gtk_widget_get_width (content), <=,
                   gtk_widget_get_width (page));

  /* Complete the native IM focus-out before destroying this fixture. GTK's
   * Wayland IM callback may otherwise arrive during the next test after its
   * client widget was finalized (also reproducible before timeline changes). */
  gtk_window_set_focus (GTK_WINDOW (window), NULL);
  GMainLoop *settle = g_main_loop_new (NULL, FALSE);
  g_timeout_add_once (180, (GSourceOnceFunc) g_main_loop_quit, settle);
  g_main_loop_run (settle);
  g_main_loop_unref (settle);
  gtk_window_destroy (GTK_WINDOW (window));
}

static gboolean
press_key (GtkWidget *widget,
           guint      keyval,
           guint      keycode)
{
  g_autoptr (GListModel) controllers = gtk_widget_observe_controllers (widget);
  gboolean handled = FALSE;

  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
    g_autoptr (GtkEventController) controller =
      g_list_model_get_item (controllers, i);

    if (GTK_IS_EVENT_CONTROLLER_KEY (controller)) {
      g_signal_emit_by_name (controller, "key-pressed", keyval, keycode, 0,
                             &handled);
      break;
    }
  }
  return handled;
}

static void
test_icon_buttons_say_what_they_do (void)
{
  g_autoptr (GtkWidget) icon = g_object_ref_sink (awra_button_new_from_icon_name ("tab-new-symbolic"));
  g_autoptr (GtkWidget) text = g_object_ref_sink (awra_button_new_with_label ("Ouvrir"));
  char *error;

  gtk_widget_set_tooltip_text (icon, "Nouvel onglet");
  error = gtk_test_accessible_check_property (GTK_ACCESSIBLE (icon), GTK_ACCESSIBLE_PROPERTY_LABEL, "Nouvel onglet");
  g_assert_null (error);
  gtk_widget_set_tooltip_text (icon, "Fermer");
  error = gtk_test_accessible_check_property (GTK_ACCESSIBLE (icon), GTK_ACCESSIBLE_PROPERTY_LABEL, "Fermer");
  g_assert_null (error);
  /* A visible label already names it. */
  gtk_widget_set_tooltip_text (text, "Ouvrir le fichier");
  g_assert_false (gtk_test_accessible_has_property (GTK_ACCESSIBLE (text), GTK_ACCESSIBLE_PROPERTY_LABEL));
}

static void
test_media_input_roles_and_rtl (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *keyboard = awra_keyboard_view_new ();
  GtkWidget *capture = awra_shortcut_capture_new ();
  GtkWidget *waveform = awra_waveform_view_new ();
  GtkWidget *row = awra_media_row_new ("Ambient rain with a deliberately "
                                       "long translated title");
  GtkWidget *volume = awra_volume_control_new ();
  GtkWidget *drop = awra_drop_zone_new ();
  AwraKeyCap *cap;
  GtkWidget *focus;

  awra_box_apply_layout_preset (GTK_BOX (box), AWRA_LAYOUT_PRESET_PAGE);
  gtk_box_append (GTK_BOX (box), keyboard);
  gtk_box_append (GTK_BOX (box), capture);
  gtk_box_append (GTK_BOX (box), waveform);
  gtk_box_append (GTK_BOX (box), row);
  gtk_box_append (GTK_BOX (box), volume);
  gtk_box_append (GTK_BOX (box), drop);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_set_default_size (GTK_WINDOW (window), 900, 700);
  gtk_window_present (GTK_WINDOW (window));
  gtk_test_widget_wait_for_draw (window);

  cap = awra_keyboard_view_get_key_cap (AWRA_KEYBOARD_VIEW (keyboard), "KeyA");
  awra_key_cap_set_secondary_label (cap, "Thunder");
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (cap)),
                   ==, GTK_ACCESSIBLE_ROLE_BUTTON);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (keyboard)), ==, GTK_ACCESSIBLE_ROLE_GROUP);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (capture)), ==, GTK_ACCESSIBLE_ROLE_BUTTON);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (waveform)), ==, GTK_ACCESSIBLE_ROLE_SLIDER);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (row)),
                   ==, GTK_ACCESSIBLE_ROLE_GROUP);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (volume)), ==, GTK_ACCESSIBLE_ROLE_GROUP);

  /* Every new control is reachable with Tab; the keyboard is one stop. */
  gtk_window_set_focus (GTK_WINDOW (window), NULL);
  g_assert_true (gtk_widget_child_focus (box, GTK_DIR_TAB_FORWARD));
  focus = gtk_root_get_focus (GTK_ROOT (window));
  g_assert_true (AWRA_IS_KEY_CAP (focus));
  g_assert_true (gtk_widget_child_focus (box, GTK_DIR_TAB_FORWARD));
  g_assert_true (gtk_root_get_focus (GTK_ROOT (window)) == capture);
  g_assert_true (gtk_widget_child_focus (box, GTK_DIR_TAB_FORWARD));
  g_assert_true (gtk_root_get_focus (GTK_ROOT (window)) == waveform);

  /* RTL mirrors the timeline, so Left moves forward in time. */
  awra_waveform_view_set_duration (AWRA_WAVEFORM_VIEW (waveform), 100.0);
  awra_waveform_view_set_marker_editable (AWRA_WAVEFORM_VIEW (waveform), TRUE);
  awra_waveform_view_set_marker_position (AWRA_WAVEFORM_VIEW (waveform), 10.0);
  gtk_widget_set_direction (box, GTK_TEXT_DIR_RTL);
  gtk_widget_set_direction (waveform, GTK_TEXT_DIR_RTL);
  gtk_widget_set_direction (keyboard, GTK_TEXT_DIR_RTL);
  gtk_test_widget_wait_for_draw (window);
  g_assert_true (press_key (waveform, GDK_KEY_Left, 113));
  g_assert_cmpfloat (awra_waveform_view_get_marker_position (
                       AWRA_WAVEFORM_VIEW (waveform)), >, 10.0);

  /* Physical keyboards are never mirrored. */
  {
    graphene_rect_t escape;
    graphene_rect_t f12;

    g_assert_true (gtk_widget_compute_bounds (
      GTK_WIDGET (awra_keyboard_view_get_key_cap (AWRA_KEYBOARD_VIEW (keyboard),
                                                  "Escape")),
      keyboard, &escape));
    g_assert_true (gtk_widget_compute_bounds (
      GTK_WIDGET (awra_keyboard_view_get_key_cap (AWRA_KEYBOARD_VIEW (keyboard),
                                                  "F12")),
      keyboard, &f12));
    g_assert_cmpfloat (escape.origin.x, <, f12.origin.x);
  }
  g_assert_cmpint (gtk_widget_get_width (row), <=, gtk_widget_get_width (box));
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  g_test_add_func ("/awra/accessibility/roles-keyboard-rtl",
                   test_roles_keyboard_and_rtl);
  g_test_add_func ("/awra/accessibility/icon-buttons-say-what-they-do", test_icon_buttons_say_what_they_do);
  g_test_add_func ("/awra/accessibility/media-input-roles-rtl",
                   test_media_input_roles_and_rtl);
  return g_test_run ();
}
