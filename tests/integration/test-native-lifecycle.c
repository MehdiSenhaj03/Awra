/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static void
quit_loop (gpointer data)
{
  g_main_loop_quit (data);
}

static void
iterate_for (guint milliseconds)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add_once (milliseconds, quit_loop, loop);
  g_main_loop_run (loop);
}

static gboolean
wait_for_mapped (GtkWidget *widget,
                 gboolean   mapped,
                 guint      timeout_ms)
{
  gint64 deadline = g_get_monotonic_time () + timeout_ms * G_TIME_SPAN_MILLISECOND;

  while (gtk_widget_get_mapped (widget) != mapped &&
         g_get_monotonic_time () < deadline) {
    while (g_main_context_iteration (NULL, FALSE))
      ;
    g_usleep (1000);
  }
  return gtk_widget_get_mapped (widget) == mapped;
}

static guint
stress_iterations (void)
{
  const char *value = g_getenv ("AWRA_STRESS_ITERATIONS");
  guint64 parsed;

  if (value == NULL || *value == '\0')
    return 100;
  parsed = g_ascii_strtoull (value, NULL, 10);
  return (guint) CLAMP (parsed, 1, 10000);
}

/* A menu scrolls when it exceeds the screen: items sit inside its viewport. */
static GtkWidget *
get_menu_box (AwraPopover *popover)
{
  GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW (awra_popover_get_child (popover));

  return gtk_viewport_get_child (GTK_VIEWPORT (gtk_scrolled_window_get_child (scroller)));
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr (GtkApplication) app = NULL;
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *menu_button;
  GtkWidget *popover;
  GtkWidget *dropdown;
  GtkWidget *context_target;
  GtkWidget *context_popover;
  GtkWidget *edge_panel;
  g_autoptr (GMenu) context_menu = g_menu_new ();
  g_autoptr (GMenu) application_menu = g_menu_new ();
  g_autoptr (GMenu) application_submenu = g_menu_new ();
  AwraDiagnostics *diagnostics;
  AwraStyleManager *style;
  AwraContext *context;
  g_autoptr (AwraMaterial) canvas = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_CANVAS);
  g_autoptr (AwraMaterial) floating = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_FLOATING);
  gboolean blur;

  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ()) {
    g_test_message ("No display available; skipping native lifecycle test");
    return 77;
  }

  app = gtk_application_new ("org.awra.TestNativeLifecycle",
                             G_APPLICATION_NON_UNIQUE);
  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);

  window = awra_window_new (app);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  menu_button = awra_menu_button_new ();
  g_menu_append (application_submenu, "Nested action", NULL);
  g_menu_append_submenu (application_menu, "More",
                         G_MENU_MODEL (application_submenu));
  awra_menu_button_set_label (AWRA_MENU_BUTTON (menu_button), "Open");
  awra_menu_button_set_menu_model (AWRA_MENU_BUTTON (menu_button),
                                   G_MENU_MODEL (application_menu));
  popover = GTK_WIDGET (awra_menu_button_get_popover (
    AWRA_MENU_BUTTON (menu_button)));
  {
    static const char *choices[] = { "Design", "Engineering", "Research", NULL };

    dropdown = awra_dropdown_new_from_strings (choices);
  }
  context_target = awra_action_row_new ("Context target", "Right-click lifecycle");
  gtk_box_append (GTK_BOX (box), menu_button);
  gtk_box_append (GTK_BOX (box), dropdown);
  gtk_box_append (GTK_BOX (box), context_target);
  awra_window_set_content (AWRA_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));
  gtk_test_widget_wait_for_draw (window);

  context = awra_context_get_for_display (gtk_widget_get_display (window));
  diagnostics = awra_context_get_diagnostics (context);
  style = awra_context_get_style_manager (context);
  for (guint i = 0; i < stress_iterations (); i++)
    g_assert_true (context == awra_context_get_for_display (
                              gtk_widget_get_display (window)));
  blur = (awra_diagnostics_get_capabilities (diagnostics) &
          AWRA_EFFECT_CAPABILITY_BLUR) != 0;
  g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 1);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                    ==,
                    blur ? 1 : 0);
  if (blur) {
    const GdkRGBA diagnostic_blue = { 0.18, 0.42, 0.92, 1.0 };
    guint64 region_updates =
      awra_diagnostics_get_effect_region_updates (diagnostics);

    /* Token changes redraw the material but must not resend an unchanged
     * native region to the compositor. */
    awra_style_manager_set_accent (style, &diagnostic_blue);
    iterate_for (5);
    g_assert_cmpuint (awra_diagnostics_get_effect_region_updates (diagnostics),
                      ==, region_updates);
    awra_style_manager_set_accent_source (style, AWRA_ACCENT_SOURCE_DEFAULT);
    iterate_for (5);
    g_assert_cmpuint (awra_diagnostics_get_effect_region_updates (diagnostics),
                      ==, region_updates);
  }
  awra_style_manager_set_material_blur_enabled (style, canvas, FALSE);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics), ==, 0);
  awra_style_manager_reset_material_tuning_for_material (style, canvas);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                    ==,
                    blur ? 1 : 0);

  for (guint i = 0; i < stress_iterations (); i++) {
    awra_menu_button_popup (AWRA_MENU_BUTTON (menu_button));
    g_assert_true (wait_for_mapped (popover, TRUE, 1000));
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 2);
    g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                      ==,
                      blur ? 2 : 0);

    awra_menu_button_popdown (AWRA_MENU_BUTTON (menu_button));
    g_assert_true (wait_for_mapped (popover, FALSE, 1000));
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 1);
    g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                      ==,
                      blur ? 1 : 0);
  }

  {
    GtkWidget *popover_bin = gtk_widget_get_first_child (menu_button);
    GtkWidget *trigger = gtk_popover_bin_get_child (
      GTK_POPOVER_BIN (popover_bin));

    g_assert_true (AWRA_IS_BUTTON (trigger));
    g_signal_emit_by_name (trigger, "clicked");
    g_assert_true (wait_for_mapped (popover, TRUE, 1000));
    g_signal_emit_by_name (trigger, "clicked");
    g_assert_true (wait_for_mapped (popover, FALSE, 1000));
  }

  {
    GtkWidget *menu_box;
    GtkWidget *submenu_button;
    GtkWidget *submenu_popover;
    GtkWidget *submenu_box;
    GtkWidget *nested_action;

    /* Menu items are built each time the menu opens: look them up once it
     * is open, and again after reopening it. */
    awra_menu_button_popup (AWRA_MENU_BUTTON (menu_button));
    g_assert_true (wait_for_mapped (popover, TRUE, 1000));
    menu_box = get_menu_box (AWRA_POPOVER (popover));
    submenu_button = gtk_widget_get_first_child (menu_box);
    g_assert_true (AWRA_IS_MENU_BUTTON (submenu_button));
    submenu_popover = GTK_WIDGET (awra_menu_button_get_popover (
      AWRA_MENU_BUTTON (submenu_button)));
    g_assert_true (gtk_popover_get_cascade_popdown (
      GTK_POPOVER (popover)));
    g_assert_true (gtk_popover_get_cascade_popdown (
      GTK_POPOVER (submenu_popover)));
    g_assert_cmpint (gtk_popover_get_position (
                       GTK_POPOVER (submenu_popover)), ==, GTK_POS_RIGHT);

    awra_menu_button_popup (AWRA_MENU_BUTTON (submenu_button));
    g_assert_true (wait_for_mapped (submenu_popover, TRUE, 1000));
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 3);
    awra_menu_button_popdown (AWRA_MENU_BUTTON (menu_button));
    g_assert_true (wait_for_mapped (submenu_popover, FALSE, 1000));
    g_assert_true (wait_for_mapped (popover, FALSE, 1000));

    awra_menu_button_popup (AWRA_MENU_BUTTON (menu_button));
    g_assert_true (wait_for_mapped (popover, TRUE, 1000));
    menu_box = get_menu_box (AWRA_POPOVER (popover));
    submenu_button = gtk_widget_get_first_child (menu_box);
    g_assert_true (AWRA_IS_MENU_BUTTON (submenu_button));
    submenu_popover = GTK_WIDGET (awra_menu_button_get_popover (
      AWRA_MENU_BUTTON (submenu_button)));
    awra_menu_button_popup (AWRA_MENU_BUTTON (submenu_button));
    g_assert_true (wait_for_mapped (submenu_popover, TRUE, 1000));
    submenu_box = get_menu_box (AWRA_POPOVER (submenu_popover));
    nested_action = gtk_widget_get_first_child (submenu_box);
    g_assert_true (GTK_IS_BUTTON (nested_action));
    g_signal_emit_by_name (nested_action, "clicked");
    g_assert_true (wait_for_mapped (submenu_popover, FALSE, 1000));
    g_assert_true (wait_for_mapped (popover, FALSE, 1000));
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 1);
  }

  popover = GTK_WIDGET (awra_dropdown_get_popover (
    AWRA_DROPDOWN (dropdown)));
  g_assert_cmpint (awra_material_get_preset (awra_surface_get_material (
                     awra_popover_get_surface (AWRA_POPOVER (popover)))), ==,
                   AWRA_MATERIAL_PRESET_FLOATING);
  awra_dropdown_popup (AWRA_DROPDOWN (dropdown));
  g_assert_true (wait_for_mapped (popover, TRUE, 1000));
  g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 2);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                    ==,
                    blur ? 2 : 0);
  awra_style_manager_set_material_blur_enabled (style, floating, FALSE);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                    ==,
                    blur ? 1 : 0);

  awra_style_manager_reset_material_tuning_for_material (style, floating);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                    ==,
                    blur ? 2 : 0);
  {
    GtkWidget *popup_content = awra_popover_get_child (
      AWRA_POPOVER (popover));
    GtkWidget *scroller = gtk_widget_get_first_child (popup_content);
    GtkWidget *list_view = gtk_scrolled_window_get_child (
      GTK_SCROLLED_WINDOW (scroller));

    g_assert_true (GTK_IS_LIST_VIEW (list_view));
    g_signal_emit_by_name (list_view, "activate", 2U);
  }
  g_assert_true (wait_for_mapped (popover, FALSE, 1000));
  g_assert_cmpuint (awra_dropdown_get_selected (AWRA_DROPDOWN (dropdown)),
                    ==,
                    2);
  g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 1);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                    ==,
                    blur ? 1 : 0);

  /* Two simultaneous roots exercise coordinator bookkeeping independently of
   * focus. The Canvas request and tracked count must not be coupled to which
   * toplevel is active. */
  {
    GtkWidget *second = awra_window_new (app);

    awra_window_set_content (AWRA_WINDOW (second),
                             gtk_label_new ("Second native root"));
    gtk_window_present (GTK_WINDOW (second));
    g_assert_true (wait_for_mapped (second, TRUE, 1000));
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 2);
    g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                      ==,
                      blur ? 2 : 0);
    gtk_widget_set_state_flags (window, GTK_STATE_FLAG_BACKDROP, FALSE);
    gtk_widget_unset_state_flags (second, GTK_STATE_FLAG_BACKDROP);
    iterate_for (5);
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 2);
    g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                      ==,
                      blur ? 2 : 0);
    gtk_widget_unset_state_flags (window, GTK_STATE_FLAG_BACKDROP);
    gtk_window_destroy (GTK_WINDOW (second));
    iterate_for (5);
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 1);
  }

  /* Successive dialogs are real native surfaces. Destroying each while it is
   * mapped proves that a transient never leaves a tracked backend handle. */
  for (guint i = 0; i < MIN (stress_iterations (), 25); i++) {
    GtkWidget *dialog = awra_dialog_new (GTK_WINDOW (window));

    awra_window_set_content (AWRA_WINDOW (dialog),
                             gtk_label_new ("Lifecycle dialog"));
    gtk_window_present (GTK_WINDOW (dialog));
    g_assert_true (wait_for_mapped (dialog, TRUE, 1000));
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 2);
    gtk_window_destroy (GTK_WINDOW (dialog));
    iterate_for (2);
    g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 1);
  }

  g_menu_append (context_menu, "Inspect", "app.inspect");
  awra_widget_set_context_menu_model (context_target,
                                      G_MENU_MODEL (context_menu));
  context_popover = GTK_WIDGET (
    awra_widget_get_context_menu_popover (context_target));
  g_assert_true (AWRA_IS_POPOVER (context_popover));
  gtk_popover_popup (GTK_POPOVER (context_popover));
  g_assert_true (wait_for_mapped (context_popover, TRUE, 1000));
  g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 2);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics),
                    ==,
                    blur ? 2 : 0);
  gtk_popover_popdown (GTK_POPOVER (context_popover));
  g_assert_true (wait_for_mapped (context_popover, FALSE, 1000));
  awra_widget_set_context_menu_model (context_target, NULL);
  g_assert_null (awra_widget_get_context_menu_popover (context_target));
  g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 1);

  edge_panel = awra_edge_panel_new ();
  awra_edge_panel_set_content (AWRA_EDGE_PANEL (edge_panel),
                               gtk_label_new ("Canvas"));
  awra_edge_panel_set_panel (AWRA_EDGE_PANEL (edge_panel),
                             awra_action_row_new ("Inspector", "Floating"));
  awra_edge_panel_set_auto_hide (AWRA_EDGE_PANEL (edge_panel), TRUE);
  awra_edge_panel_set_revealed (AWRA_EDGE_PANEL (edge_panel), FALSE);
  awra_window_set_content (AWRA_WINDOW (window), edge_panel);
  iterate_for (20);
  awra_edge_panel_set_revealed (AWRA_EDGE_PANEL (edge_panel), TRUE);
  iterate_for (20);
  g_assert_true (awra_edge_panel_get_revealed (AWRA_EDGE_PANEL (edge_panel)));
  awra_edge_panel_set_revealed (AWRA_EDGE_PANEL (edge_panel), FALSE);
  g_assert_false (awra_edge_panel_get_revealed (AWRA_EDGE_PANEL (edge_panel)));

  /* Pending motion and timeout callbacks must be cancelled by destruction. */
  {
    GtkWidget *toast = awra_toast_overlay_new ();

    awra_toast_overlay_set_child (AWRA_TOAST_OVERLAY (toast),
                                  gtk_label_new ("Content"));
    awra_toast_overlay_show_message (AWRA_TOAST_OVERLAY (toast),
                                     "Destroy with pending timeout", 60000);
    awra_window_set_content (AWRA_WINDOW (window), toast);
    iterate_for (5);
    g_assert_true (awra_toast_overlay_get_revealed (
      AWRA_TOAST_OVERLAY (toast)));
  }

  gtk_window_destroy (GTK_WINDOW (window));
  iterate_for (20);
  g_assert_cmpuint (awra_diagnostics_get_native_surfaces (diagnostics), ==, 0);
  g_assert_cmpuint (awra_diagnostics_get_effects_applied (diagnostics), ==, 0);
  return 0;
}
