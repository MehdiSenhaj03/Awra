/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>
#include <glib/gstdio.h>
#include <string.h>

static const char *self_program;

static const char hostile_css[] =
  ".awra-button, .awra-entry, .awra-dropdown, .awra-switch,"
  ".awra-check, .awra-progress, .awra-segment, .awra-tabs button,"
  ".awra-list row, .awra-navigation-item, .awra-body, .awra-muted,"
  ".awra-action-row, .awra-menu-item, .awra-edge-trigger,"
  ".awra-tab-bar, .awra-tab-item, .awra-tab-pill, .awra-tab-button,"
  ".awra-tab-title, .awra-tab-label, .awra-tab-close,"
  ".awra-eyebrow, .awra-title-1, .awra-title-2, .awra-title-3,"
  ".awra-display, .awra-monospace, awrapage, awrapageviewport,"
  "awrapageheader, awrasection, awratoolbar, awraform, awraformrow,"
  "awraemptystate, awraloadingstate, awraerrorstate, awrafilterbar,"
  "awramasterdetail, awradetailpane,"
  "awradataview, awraselectiontoolbar, awratextarea, awrapasswordentry,"
  "awratagentry, awrachoicegroup, awrastatusbanner,"
  "awrakeycap, awrakeycap label, awrakeyboardview, awrashortcutcapture,"
  "awrashortcutcapture label, awrawaveformview, awratimeline, awravolumecontrol,"
  "awramediarow, awramediarow label, awradropzone, awradropzone label,"
  "awradropzone box, awraavatar,"
  "awradataview columnview,"
  "awradataview columnview > header > button,"
  "awradataview columnview listview > row {"
  " background-color: #ff0033; background-image: linear-gradient(#00ff00,#0000ff);"
  " color: #00ff00; border: 9px solid #0044ff; border-radius: 1px;"
  " box-shadow: 0 0 0 12px #ffff00; outline: 7px solid #ff00ff;"
  " min-width: 91px; min-height: 73px; padding: 19px; opacity: .37; }"
  /* This is representative of real desktop themes which use the generic
   * libadwaita role to flood-fill the content beside a sidebar. Awra's
   * public `sidebar-pane` role must never install that raw CSS class. */
  ".sidebar-pane:first-child {"
  " box-shadow: 9000px 0 0 9999px #252932; min-width: 900px; }"
  ".sidebar-pane:backdrop { box-shadow: none; min-width: 0; }"
  ".awra-slider trough, .awra-slider highlight, .awra-slider slider,"
  ".awra-switch slider, .awra-check check, .awra-check radio,"
  ".awra-progress trough, .awra-progress progress {"
  " background-color: #ff0033; background-image: linear-gradient(#00ff00,#0000ff);"
  " border: 8px solid #0044ff; border-radius: 0; box-shadow: 0 0 9px #ffff00; }"
  "popover, popover > contents, popover scrolledwindow, popover listview,"
  "popover listview > row, popover label {"
  " background-color: #ff0033; background-image: linear-gradient(#00ff00,#0000ff);"
  " color: #00ff00; border: 9px solid #0044ff; border-radius: 1px;"
  " box-shadow: 0 0 0 12px #ffff00; outline: 7px solid #ff00ff;"
  " min-width: 91px; min-height: 73px; padding: 19px; opacity: .37; }";

static gboolean
quit_loop_cb (gpointer data)
{
  g_main_loop_quit (data);
  return G_SOURCE_REMOVE;
}

static void
settle_ui (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  /* Awra's longest current component transition is 110 ms.  Waiting for two
   * frames beyond that boundary keeps this pixel probe from sampling an
   * in-flight transition when a compositor or CI runner delivers a late
   * frame.  The comparisons below remain exact; only their sampling point is
   * made deterministic. */
  g_timeout_add (180, quit_loop_cb, loop);
  g_main_loop_run (loop);
  while (g_main_context_iteration (NULL, FALSE))
    ;
}

static void
popover_mapped_cb (GtkWidget *widget,
                   GMainLoop *loop)
{
  (void) widget;
  g_main_loop_quit (loop);
}

static void
wait_for_popover_map (GtkWidget *popover)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  g_autoptr (GSource) timeout = g_timeout_source_new (2000);
  gulong map_id;

  if (!gtk_widget_get_mapped (popover)) {
    map_id = g_signal_connect (popover, "map",
                               G_CALLBACK (popover_mapped_cb), loop);
    g_source_set_callback (timeout, quit_loop_cb, loop, NULL);
    g_source_attach (timeout, NULL);
    g_main_loop_run (loop);
    g_source_destroy (timeout);
    g_signal_handler_disconnect (popover, map_id);
  }
  settle_ui ();
  g_assert_true (gtk_widget_get_mapped (popover));
}

static void
settle_scroll_indicators (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  /* GtkScrolledWindow may keep its overlay indicator in the initial activity
   * state for roughly one second after mapping.  Rendering every child under
   * ASan is slow enough for two otherwise adjacent captures to straddle that
   * timeout.  Let this GTK-owned state expire before comparing focus states;
   * the comparison itself remains exact and runs with normal animations. */
  g_timeout_add (1200, quit_loop_cb, loop);
  g_main_loop_run (loop);
  while (g_main_context_iteration (NULL, FALSE))
    ;
}

static GtkWidget *
make_probe_scene (void)
{
  static const char *choices[] = { "One", "Two", NULL };
  GtkWidget *grid = gtk_grid_new ();
  GtkWidget *button = awra_button_new ();
  GtkWidget *toggle = awra_toggle_button_new ();
  GtkWidget *entry = awra_entry_new ();
  GtkWidget *search = awra_search_entry_new ();
  GtkWidget *switcher = awra_switch_new ();
  GtkWidget *slider = awra_slider_new_with_range (
    GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
  GtkWidget *check = awra_check_button_new ();
  GtkWidget *spin = awra_spin_button_new_with_range (0.0, 10.0, 1.0);
  GtkWidget *dropdown = awra_dropdown_new_from_strings (choices);
  GtkWidget *progress = awra_progress_bar_new ();
  GtkWidget *segments = awra_segmented_control_new ();
  GtkWidget *disabled = awra_button_new ();
  GtkWidget *navigation_item = awra_navigation_item_new_with_label ("Overview");
  GtkWidget *menu_button = awra_menu_button_new ();
  GtkWidget *action_row = awra_action_row_new (
    "Inspector", "Floating edge panel");
  GtkWidget *title = gtk_label_new ("Semantic heading");
  GtkWidget *body = gtk_label_new ("System font, Awra hierarchy");
  GtkWidget *page = awra_page_new ();
  GtkWidget *page_header = awra_page_header_new ();
  GtkWidget *section = awra_section_new ();
  GtkWidget *toolbar = awra_toolbar_new ();
  GtkWidget *page_content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *page_body = gtk_label_new ("Typed page");
  GtkWidget *section_content = gtk_label_new ("Content");
  GtkWidget *form = awra_form_new ();
  GtkWidget *form_row = awra_form_row_new ();
  GtkWidget *form_entry = awra_entry_new ();
  GtkWidget *empty_state = awra_empty_state_new ();
  GtkWidget *filter_bar = awra_filter_bar_new ();
  GtkWidget *master_detail = awra_master_detail_new ();
  GtkWidget *master_content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *data_view = awra_data_view_new (NULL);
  GtkWidget *selection_toolbar = awra_selection_toolbar_new (NULL);
  AwraDataColumn *data_column = awra_data_column_new ("Document", NULL);
  GtkWidget *detail_pane = awra_detail_pane_new ();
  GtkWidget *detail_section = awra_section_new ();
  GtkWidget *filter_summary = gtk_label_new ("2 results");
  GtkWidget *detail_content = gtk_label_new ("Semantic section");
  GtkWidget *text_area = awra_text_area_new ();
  GtkWidget *password = awra_password_entry_new ();
  GtkWidget *tags = awra_tag_entry_new ();
  GtkWidget *choice_group = awra_choice_group_new ();
  GtkWidget *banner = awra_status_banner_new ();
  GtkWidget *tabs = awra_tab_view_new ();
  GtkWidget *tab_home = gtk_label_new ("Home page");
  GtkWidget *tab_document = gtk_label_new ("Document page");
  GMenu *menu = g_menu_new ();

  g_assert_true (awra_widget_add_style_role (grid, "sidebar-pane"));
  g_assert_true (gtk_widget_has_css_class (grid,
                                          "awra-role-sidebar-pane"));
  g_assert_false (gtk_widget_has_css_class (grid, "sidebar-pane"));

  awra_grid_set_spacing (GTK_GRID (grid), AWRA_SPACING_MD,
                         AWRA_SPACING_LG);
  awra_widget_set_margin (grid, AWRA_SPACING_XL);
  gtk_widget_set_size_request (button, 110, -1);
  gtk_widget_set_size_request (toggle, 110, -1);
  gtk_widget_set_size_request (entry, 220, -1);
  gtk_widget_set_size_request (search, 220, -1);
  gtk_widget_set_size_request (slider, 220, -1);
  gtk_widget_set_sensitive (disabled, FALSE);
  awra_button_set_appearance (AWRA_BUTTON (button),
                              AWRA_BUTTON_APPEARANCE_PRIMARY);
  awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (navigation_item),
                                      "go-home-symbolic");
  awra_navigation_item_set_selected (AWRA_NAVIGATION_ITEM (navigation_item),
                                     TRUE);
  awra_widget_set_typography (title, AWRA_TYPOGRAPHY_TITLE_2);
  awra_widget_set_typography (body, AWRA_TYPOGRAPHY_BODY);
  g_menu_append (menu, "Refresh", "app.refresh");
  awra_menu_button_set_icon_name (AWRA_MENU_BUTTON (menu_button),
                                  "preferences-system-symbolic");
  awra_menu_button_set_appearance (AWRA_MENU_BUTTON (menu_button),
                                   AWRA_BUTTON_APPEARANCE_TOOLBAR);
  awra_menu_button_set_menu_model (AWRA_MENU_BUTTON (menu_button),
                                   G_MENU_MODEL (menu));
  awra_action_row_set_icon_name (AWRA_ACTION_ROW (action_row),
                                 "document-properties-symbolic");
  awra_action_row_set_suffix (AWRA_ACTION_ROW (action_row),
                              awra_badge_new ("NEW"));
  awra_page_set_max_content_width (AWRA_PAGE (page), 420);
  awra_page_set_content_inset (AWRA_PAGE (page), AWRA_SPACING_SM);
  awra_box_set_spacing (GTK_BOX (page_content), AWRA_SPACING_MD);
  awra_widget_set_typography (page_body, AWRA_TYPOGRAPHY_BODY);
  awra_page_header_set_title (AWRA_PAGE_HEADER (page_header), "Library");
  awra_page_header_set_subtitle (AWRA_PAGE_HEADER (page_header),
                                 "Typed heading");
  awra_section_set_title (AWRA_SECTION (section), "Recent work");
  awra_section_set_description (AWRA_SECTION (section), "Unboxed section");
  awra_widget_set_typography (section_content, AWRA_TYPOGRAPHY_BODY);
  awra_section_set_child (AWRA_SECTION (section), section_content);
  awra_toolbar_set_start_widget (AWRA_TOOLBAR (toolbar),
                                 awra_button_new_with_label ("Back"));
  awra_toolbar_set_end_widget (AWRA_TOOLBAR (toolbar),
                               awra_button_new_with_label ("Done"));
  awra_form_row_set_label (AWRA_FORM_ROW (form_row), "Workspace name");
  awra_form_row_set_help_text (AWRA_FORM_ROW (form_row),
                               "A framework-owned form rhythm");
  awra_form_row_set_control (AWRA_FORM_ROW (form_row), form_entry);
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (form_row));
  awra_empty_state_set_title (AWRA_EMPTY_STATE (empty_state), "No results");
  awra_empty_state_set_description (AWRA_EMPTY_STATE (empty_state),
                                    "Change the current filters.");
  awra_empty_state_set_action (AWRA_EMPTY_STATE (empty_state),
                               awra_button_new_with_label ("Reset"));
  awra_text_area_set_placeholder_text (AWRA_TEXT_AREA (text_area), "Notes");
  awra_password_entry_set_placeholder_text (AWRA_PASSWORD_ENTRY (password),
                                             "Password");
  awra_tag_entry_add (AWRA_TAG_ENTRY (tags), "GTK");
  awra_choice_group_set_title (AWRA_CHOICE_GROUP (choice_group), "Mode");
  awra_choice_group_append (AWRA_CHOICE_GROUP (choice_group), "Automatic");
  awra_status_banner_set_title (AWRA_STATUS_BANNER (banner), "Ready");
  awra_tab_view_append (AWRA_TAB_VIEW (tabs), tab_home, "Home", TRUE);
  awra_tab_view_set_page_icon_name (AWRA_TAB_VIEW (tabs), tab_home,
                                    "go-home-symbolic");
  awra_tab_view_set_page_title_visible (AWRA_TAB_VIEW (tabs), tab_home,
                                        FALSE);
  awra_tab_view_append (AWRA_TAB_VIEW (tabs), tab_document,
                        "A deliberately long document title", FALSE);
  awra_tab_view_set_selected_page (AWRA_TAB_VIEW (tabs), tab_document);
  awra_tab_view_set_content_visible (AWRA_TAB_VIEW (tabs), FALSE);
  gtk_widget_set_size_request (tabs, 360, -1);
  awra_filter_bar_set_search (AWRA_FILTER_BAR (filter_bar),
                              awra_search_entry_new ());
  awra_filter_bar_append_filter (AWRA_FILTER_BAR (filter_bar),
                                 awra_dropdown_new_from_strings (choices));
  awra_widget_set_typography (filter_summary, AWRA_TYPOGRAPHY_MUTED);
  awra_filter_bar_set_summary (AWRA_FILTER_BAR (filter_bar), filter_summary);
  awra_selection_toolbar_set_auto_hide (
    AWRA_SELECTION_TOOLBAR (selection_toolbar), FALSE);
  awra_data_view_append_column (AWRA_DATA_VIEW (data_view), data_column);
  awra_data_view_set_state (AWRA_DATA_VIEW (data_view),
                            AWRA_DATA_VIEW_STATE_EMPTY);
  gtk_box_append (GTK_BOX (master_content), selection_toolbar);
  gtk_box_append (GTK_BOX (master_content), data_view);
  g_object_unref (data_column);
  awra_detail_pane_set_title (AWRA_DETAIL_PANE (detail_pane), "Document");
  awra_detail_pane_set_subtitle (AWRA_DETAIL_PANE (detail_pane),
                                 "Responsive detail");
  awra_section_set_title (AWRA_SECTION (detail_section), "Overview");
  awra_widget_set_typography (detail_content, AWRA_TYPOGRAPHY_BODY);
  awra_section_set_child (AWRA_SECTION (detail_section), detail_content);
  awra_detail_pane_append_section (AWRA_DETAIL_PANE (detail_pane),
                                   AWRA_SECTION (detail_section));
  awra_master_detail_set_master (AWRA_MASTER_DETAIL (master_detail),
                                 master_content);
  awra_master_detail_set_detail (AWRA_MASTER_DETAIL (master_detail),
                                 detail_pane);
  gtk_widget_set_size_request (master_detail, -1, 140);
  gtk_box_append (GTK_BOX (page_content), page_header);
  gtk_box_append (GTK_BOX (page_content), toolbar);
  gtk_box_append (GTK_BOX (page_content), filter_bar);
  gtk_box_append (GTK_BOX (page_content), section);
  gtk_box_append (GTK_BOX (page_content), form);
  gtk_box_append (GTK_BOX (page_content), empty_state);
  gtk_box_append (GTK_BOX (page_content), banner);
  gtk_box_append (GTK_BOX (page_content), text_area);
  gtk_box_append (GTK_BOX (page_content), password);
  gtk_box_append (GTK_BOX (page_content), tags);
  gtk_box_append (GTK_BOX (page_content), choice_group);
  gtk_box_append (GTK_BOX (page_content), page_body);
  awra_page_set_child (AWRA_PAGE (page), page_content);
  gtk_widget_set_size_request (page, 280, 260);
  g_object_unref (menu);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
  awra_switch_set_active (AWRA_SWITCH (switcher), TRUE);
  gtk_range_set_value (GTK_RANGE (slider), 62.0);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
  awra_spin_button_set_value (AWRA_SPIN_BUTTON (spin), 4.0);
  awra_progress_bar_set_fraction (AWRA_PROGRESS_BAR (progress), 0.64);
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "A");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "B");
  awra_segmented_control_set_selected (AWRA_SEGMENTED_CONTROL (segments), 1);

  gtk_grid_attach (GTK_GRID (grid), button, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), toggle, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), disabled, 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 0, 1, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), search, 0, 2, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), switcher, 2, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 2, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), slider, 0, 3, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), spin, 2, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), dropdown, 0, 4, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), progress, 1, 4, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), segments, 0, 5, 3, 1);
  gtk_grid_attach (GTK_GRID (grid), navigation_item, 0, 6, 3, 1);
  gtk_grid_attach (GTK_GRID (grid), menu_button, 0, 7, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), title, 0, 8, 3, 1);
  gtk_grid_attach (GTK_GRID (grid), body, 0, 9, 3, 1);
  gtk_grid_attach (GTK_GRID (grid), action_row, 0, 10, 3, 1);
  gtk_grid_attach (GTK_GRID (grid), page, 0, 11, 3, 1);
  gtk_grid_attach (GTK_GRID (grid), master_detail, 0, 12, 3, 1);
  gtk_grid_attach (GTK_GRID (grid), tabs, 0, 13, 3, 1);
  {
    static const float peaks[] = { 0.2f, 0.6f, 0.9f, 0.4f, 0.7f, 0.3f,
                                   0.8f, 0.5f, 0.2f, 0.6f };
    static const guint shortcut[] = { 37, 38 };
    GtkWidget *media = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *caps = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *bound = awra_key_cap_new ("KeyA", "A");
    GtkWidget *free_key = awra_key_cap_new ("KeyS", "S");
    GtkWidget *capture = awra_shortcut_capture_new ();
    GtkWidget *waveform = awra_waveform_view_new ();
    GtkWidget *row = awra_media_row_new ("Rain");
    GtkWidget *drop = awra_drop_zone_new ();
    GtkWidget *tiles = awra_tile_view_new ();
    GtkWidget *left_tile = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *right_tile = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GdkRGBA tint = { 0.70, 0.20, 0.25, 1.0 };

    awra_key_cap_set_bound (AWRA_KEY_CAP (bound), TRUE);
    awra_key_cap_set_secondary_label (AWRA_KEY_CAP (bound), "Thunder");
    awra_key_cap_set_badge (AWRA_KEY_CAP (bound), "2");
    awra_key_cap_set_tint (AWRA_KEY_CAP (free_key), &tint);
    awra_shortcut_capture_set_shortcut (AWRA_SHORTCUT_CAPTURE (capture),
                                        shortcut, G_N_ELEMENTS (shortcut),
                                        GDK_CONTROL_MASK);
    awra_shortcut_capture_set_display_labels (
      AWRA_SHORTCUT_CAPTURE (capture),
      (const char *[]) { "Ctrl", "A", NULL });
    awra_waveform_view_set_peaks (AWRA_WAVEFORM_VIEW (waveform), peaks,
                                  G_N_ELEMENTS (peaks));
    awra_waveform_view_set_duration (AWRA_WAVEFORM_VIEW (waveform), 10.0);
    awra_waveform_view_set_position (AWRA_WAVEFORM_VIEW (waveform), 4.0);
    awra_waveform_view_set_marker_position (AWRA_WAVEFORM_VIEW (waveform),
                                            2.0);
    gtk_widget_set_size_request (waveform, -1, 32);
    awra_media_row_set_subtitle (AWRA_MEDIA_ROW (row), "Ambience");
    awra_media_row_set_duration (AWRA_MEDIA_ROW (row), 90.0);
    awra_media_row_set_position (AWRA_MEDIA_ROW (row), 30.0);
    awra_media_row_set_show_volume (AWRA_MEDIA_ROW (row), TRUE);
    awra_drop_zone_set_title (AWRA_DROP_ZONE (drop), "Drop files");
    gtk_box_append (GTK_BOX (caps), bound);
    gtk_box_append (GTK_BOX (caps), free_key);
    gtk_box_append (GTK_BOX (caps), capture);
    gtk_box_append (GTK_BOX (media), caps);
    gtk_box_append (GTK_BOX (media), waveform);
    gtk_box_append (GTK_BOX (media), row);
    gtk_box_append (GTK_BOX (media), drop);
    awra_tile_view_insert (AWRA_TILE_VIEW (tiles), left_tile, NULL, GTK_POS_LEFT);
    awra_tile_view_insert (AWRA_TILE_VIEW (tiles), right_tile, left_tile,
                           GTK_POS_RIGHT);
    gtk_widget_set_size_request (tiles, -1, 24);
    gtk_box_append (GTK_BOX (media), tiles);
    GtkWidget *timeline = awra_timeline_new ();
    awra_timeline_set_duration (AWRA_TIMELINE (timeline), 60);
    awra_timeline_set_position (AWRA_TIMELINE (timeline), 24);
    awra_timeline_add_interval (AWRA_TIMELINE (timeline), 10, 20, "Cue");
    gtk_box_append (GTK_BOX (media), timeline);
    {
      GtkWidget *people = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      GtkWidget *person = awra_avatar_new (32, "Ada Lovelace");
      GtkWidget *place = awra_avatar_new (32, "Workshop");

      awra_avatar_set_presence (AWRA_AVATAR (person), AWRA_PRESENCE_IDLE);
      awra_avatar_set_shape (AWRA_AVATAR (place), AWRA_AVATAR_SHAPE_ROUNDED);
      gtk_box_append (GTK_BOX (people), person);
      gtk_box_append (GTK_BOX (people), place);
      gtk_box_append (GTK_BOX (media), people);
    }
    gtk_grid_attach (GTK_GRID (grid), media, 0, 14, 3, 1);
  }
  g_object_set_data (G_OBJECT (grid), "awra-probe-dropdown", dropdown);
  g_object_set_data (G_OBJECT (grid), "awra-probe-menu-button", menu_button);
  g_object_set_data (G_OBJECT (grid), "awra-probe-filter-bar", filter_bar);
  g_object_set_data (G_OBJECT (grid), "awra-probe-master-detail",
                     master_detail);
  g_object_set_data (G_OBJECT (grid), "awra-probe-detail-pane", detail_pane);
  return grid;
}

static char *
snapshot_checksum (GtkWidget *widget)
{
  g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();
  g_autoptr (GskRenderNode) node = NULL;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GdkTextureDownloader) downloader = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GChecksum) checksum = g_checksum_new (G_CHECKSUM_SHA256);
  GskRenderer *renderer;
  graphene_rect_t bounds;
  gsize size;
  gsize stride;
  const guchar *data;
  guint dimensions[2];

  GTK_WIDGET_GET_CLASS (widget)->snapshot (widget, snapshot);
  node = gtk_snapshot_to_node (snapshot);
  if (node == NULL)
    return g_strdup ("empty");
  gsk_render_node_get_bounds (node, &bounds);
  renderer = gtk_native_get_renderer (gtk_widget_get_native (widget));
  g_assert_nonnull (renderer);
  texture = gsk_renderer_render_texture (renderer, node, &bounds);
  if (GTK_IS_GRID (widget) && g_getenv ("AWRA_PROBE_PNG") != NULL)
    g_assert_true (gdk_texture_save_to_png (
      texture, g_getenv ("AWRA_PROBE_PNG")));
  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (
    downloader, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  dimensions[0] = gdk_texture_get_width (texture);
  dimensions[1] = gdk_texture_get_height (texture);
  data = g_bytes_get_data (bytes, &size);
  g_checksum_update (checksum, (const guchar *) dimensions,
                     sizeof dimensions);
  g_checksum_update (checksum, (const guchar *) &stride, sizeof stride);
  g_checksum_update (checksum, data, size);
  return g_strdup (g_checksum_get_string (checksum));
}

static void
print_probe_checksums (GtkWidget *scene)
{
  if (g_getenv ("AWRA_PROBE_PNG") != NULL) {
    g_autofree char *scene_checksum = snapshot_checksum (scene);

    (void) scene_checksum;
  }
  for (GtkWidget *child = gtk_widget_get_first_child (scene);
       child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    g_autofree char *checksum = snapshot_checksum (child);
    graphene_point_t origin = GRAPHENE_POINT_INIT (0, 0);
    graphene_point_t position;

    g_assert_true (gtk_widget_compute_point (child, scene, &origin, &position));
    g_print ("%s@%.0f,%.0f=%dx%d:%s\n", G_OBJECT_TYPE_NAME (child),
             position.x, position.y,
             gtk_widget_get_width (child), gtk_widget_get_height (child),
             checksum);
  }
}

static char *
probe_geometry (GtkWidget *scene)
{
  g_autoptr (GString) geometry = g_string_new (NULL);

  for (GtkWidget *child = gtk_widget_get_first_child (scene);
       child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    graphene_point_t origin = GRAPHENE_POINT_INIT (0, 0);
    graphene_point_t position;

    g_assert_true (gtk_widget_compute_point (child, scene, &origin, &position));
    g_string_append_printf (geometry, "%s@%.0f,%.0f=%dx%d\n",
                            G_OBJECT_TYPE_NAME (child), position.x, position.y,
                            gtk_widget_get_width (child),
                            gtk_widget_get_height (child));
  }
  return g_string_free (g_steal_pointer (&geometry), FALSE);
}

static char *
probe_pixels (GtkWidget *scene)
{
  g_autoptr (GString) pixels = g_string_new (NULL);

  for (GtkWidget *child = gtk_widget_get_first_child (scene);
       child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    g_autofree char *checksum = snapshot_checksum (child);

    g_string_append_printf (pixels, "%s:%s\n", G_OBJECT_TYPE_NAME (child),
                            checksum);
  }
  return g_string_free (g_steal_pointer (&pixels), FALSE);
}

static int
run_probe_child (void)
{
  g_autoptr (GtkCssProvider) hostile = NULL;
  g_autoptr (GtkApplication) app = NULL;
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *scene;
  AwraDropdown *dropdown;
  AwraPopover *dropdown_popover;
  AwraMenuButton *menu_button;
  AwraPopover *menu_popover;
  AwraContext *context;

  if (!gtk_init_check ())
    return 77;
  awra_init ();
  context = awra_context_get_for_display (gdk_display_get_default ());
  awra_style_manager_set_appearance (
    awra_context_get_style_manager (context), AWRA_APPEARANCE_LIGHT);

  if (g_getenv ("AWRA_PROBE_RUNTIME_USER_PROVIDER") != NULL) {
    hostile = gtk_css_provider_new ();
    gtk_css_provider_load_from_string (hostile, hostile_css);
    gtk_style_context_add_provider_for_display (
      gdk_display_get_default (), GTK_STYLE_PROVIDER (hostile),
      GTK_STYLE_PROVIDER_PRIORITY_USER);
  }

  app = gtk_application_new ("org.awra.StyleProbe",
                             G_APPLICATION_NON_UNIQUE);
  if (!g_application_register (G_APPLICATION (app), NULL, &error))
    return 1;
  window = awra_window_new (app);
  gtk_window_set_default_size (GTK_WINDOW (window), 680, 900);
  scene = make_probe_scene ();
  awra_window_set_content (AWRA_WINDOW (window), scene);
  gtk_window_present (GTK_WINDOW (window));
  settle_ui ();
  dropdown = g_object_get_data (G_OBJECT (scene), "awra-probe-dropdown");
  dropdown_popover = awra_dropdown_get_popover (dropdown);
  gtk_popover_set_autohide (GTK_POPOVER (dropdown_popover), FALSE);
  awra_dropdown_popup (dropdown);
  wait_for_popover_map (GTK_WIDGET (dropdown_popover));
  g_assert_true (gtk_widget_grab_focus (gtk_widget_get_first_child (scene)));
  settle_scroll_indicators ();
  g_assert_true (gtk_widget_get_mapped (GTK_WIDGET (dropdown_popover)));
  {
    g_autofree char *active_geometry = probe_geometry (scene);
    g_autofree char *active_pixels = snapshot_checksum (scene);
    g_autofree char *active_children = probe_pixels (scene);
    g_autofree char *active_filter = snapshot_checksum (g_object_get_data (
      G_OBJECT (scene), "awra-probe-filter-bar"));
    g_autofree char *active_master_detail = snapshot_checksum (
      g_object_get_data (G_OBJECT (scene), "awra-probe-master-detail"));
    g_autofree char *active_detail = snapshot_checksum (g_object_get_data (
      G_OBJECT (scene), "awra-probe-detail-pane"));
    g_autofree char *inactive_geometry = NULL;
    g_autofree char *inactive_pixels = NULL;
    g_autofree char *inactive_children = NULL;
    g_autofree char *inactive_filter = NULL;
    g_autofree char *inactive_master_detail = NULL;
    g_autofree char *inactive_detail = NULL;

    gtk_widget_set_state_flags (window, GTK_STATE_FLAG_BACKDROP, FALSE);
    settle_ui ();
    inactive_geometry = probe_geometry (scene);
    inactive_pixels = snapshot_checksum (scene);
    inactive_children = probe_pixels (scene);
    inactive_filter = snapshot_checksum (g_object_get_data (
      G_OBJECT (scene), "awra-probe-filter-bar"));
    inactive_master_detail = snapshot_checksum (g_object_get_data (
      G_OBJECT (scene), "awra-probe-master-detail"));
    inactive_detail = snapshot_checksum (g_object_get_data (
      G_OBJECT (scene), "awra-probe-detail-pane"));
    if (g_strcmp0 (active_filter, inactive_filter) != 0)
      g_printerr ("filter active=%s inactive=%s\n", active_filter,
                  inactive_filter);
    if (g_strcmp0 (active_master_detail, inactive_master_detail) != 0)
      g_printerr ("master-detail active=%s inactive=%s\n",
                  active_master_detail, inactive_master_detail);
    if (g_strcmp0 (active_detail, inactive_detail) != 0)
      g_printerr ("detail active=%s inactive=%s\n", active_detail,
                  inactive_detail);
    g_assert_cmpstr (active_filter, ==, inactive_filter);
    g_assert_cmpstr (active_master_detail, ==, inactive_master_detail);
    g_assert_cmpstr (active_detail, ==, inactive_detail);
    g_assert_cmpstr (active_geometry, ==, inactive_geometry);
    if (g_strcmp0 (active_children, inactive_children) != 0)
      g_printerr ("active children:\n%sinactive children:\n%s",
                  active_children, inactive_children);
    g_assert_cmpstr (active_children, ==, inactive_children);
    g_assert_cmpstr (active_pixels, ==, inactive_pixels);
    gtk_widget_unset_state_flags (window, GTK_STATE_FLAG_BACKDROP);
    settle_ui ();
  }
  print_probe_checksums (scene);
  {
    g_autofree char *checksum = snapshot_checksum (
      GTK_WIDGET (dropdown_popover));

    g_print ("AwraDropdownPopup=%dx%d:%s\n",
             gtk_widget_get_width (GTK_WIDGET (dropdown_popover)),
             gtk_widget_get_height (GTK_WIDGET (dropdown_popover)),
             checksum);
  }
  awra_dropdown_popdown (dropdown);
  menu_button = g_object_get_data (G_OBJECT (scene),
                                   "awra-probe-menu-button");
  menu_popover = awra_menu_button_get_popover (menu_button);
  gtk_popover_set_autohide (GTK_POPOVER (menu_popover), FALSE);
  awra_menu_button_popup (menu_button);
  wait_for_popover_map (GTK_WIDGET (menu_popover));
  {
    g_autofree char *checksum = snapshot_checksum (GTK_WIDGET (menu_popover));

    g_print ("AwraMenuPopup=%dx%d:%s\n",
             gtk_widget_get_width (GTK_WIDGET (menu_popover)),
             gtk_widget_get_height (GTK_WIDGET (menu_popover)),
             checksum);
  }
  awra_menu_button_popdown (menu_button);
  gtk_window_destroy (GTK_WINDOW (window));
  return 0;
}

static char *
run_probe (const char *program,
           const char *theme,
           const char *config_home,
           gboolean    runtime_provider,
           int        *status_out)
{
  g_auto (GStrv) environment = g_get_environ ();
  g_autofree char *stdout_text = NULL;
  g_autofree char *stderr_text = NULL;
  g_autoptr (GError) error = NULL;
  char *arguments[] = { (char *) program, NULL };
  int status = 0;

  environment = g_environ_setenv (environment, "AWRA_STYLE_PROBE_CHILD", "1", TRUE);
  environment = g_environ_setenv (environment, "AWRA_FORCE_FALLBACK", "1", TRUE);
  environment = g_environ_setenv (environment, "GTK_THEME", theme, TRUE);
  environment = g_environ_setenv (environment, "XDG_CONFIG_HOME", config_home, TRUE);
  /* GTK 4.23 can emit this warning internally when Xvfb reports a synthetic
   * frame twice.  Keep criticals fatal in the child and inspect every warning
   * below, allowing only that exact backend diagnostic. */
  environment = g_environ_setenv (environment, "G_DEBUG", "fatal-criticals", TRUE);
  if (runtime_provider)
    environment = g_environ_setenv (environment,
                                    "AWRA_PROBE_RUNTIME_USER_PROVIDER",
                                    "1", TRUE);
  else
    environment = g_environ_unsetenv (environment,
                                      "AWRA_PROBE_RUNTIME_USER_PROVIDER");
  /* A portal daemon started on this bus would write to the same stderr, and
   * the check below judges the probe's own output. */
  environment = g_environ_setenv (environment, "GDK_DEBUG", "no-portals", FALSE);
  g_assert_true (g_spawn_sync (NULL, arguments, environment,
                               G_SPAWN_DEFAULT, NULL, NULL,
                               &stdout_text, &stderr_text, &status, &error));
  g_assert_no_error (error);
  /* The probe's stdout is in text mode: \r\n on Windows. */
  if (stdout_text != NULL) {
    char *kept = stdout_text;

    for (const char *read = stdout_text; *read != '\0'; read++) {
      if (*read != '\r')
        *kept++ = *read;
    }
    *kept = '\0';
  }
  if (status_out != NULL)
    *status_out = status;
  if (stderr_text != NULL) {
    g_auto (GStrv) lines = g_strsplit (stderr_text, "\n", -1);

    for (gsize i = 0; lines[i] != NULL; i++) {
      if (strstr (lines[i], "WARNING") != NULL ||
          strstr (lines[i], "CRITICAL") != NULL) {
        g_assert_nonnull (strstr (
          lines[i],
          "Gdk-WARNING"));
        g_assert_nonnull (strstr (
          lines[i],
          "gdk_frame_timings_submitted() called on submitted frame."));
      }
    }
  }
  if (status != 0 && stderr_text != NULL)
    g_test_message ("style probe stderr: %s", stderr_text);
  if (status == 0)
    g_assert_true (g_regex_match_simple (
      "^AwraButton@-?[0-9]+,-?[0-9]+=[0-9]+x[0-9]+:(empty|[0-9a-f]{64})\\n",
                                        stdout_text, 0, 0));
  return g_steal_pointer (&stdout_text);
}

static void
remove_test_tree (const char *path)
{
  if (g_file_test (path, G_FILE_TEST_IS_SYMLINK)) {
    g_assert_cmpint (g_remove (path), ==, 0);
  } else if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
    g_autoptr (GDir) directory = g_dir_open (path, 0, NULL);
    const char *name;

    g_assert_nonnull (directory);
    while ((name = g_dir_read_name (directory)) != NULL) {
      g_autofree char *child = g_build_filename (path, name, NULL);

      remove_test_tree (child);
    }
    g_assert_cmpint (g_rmdir (path), ==, 0);
  } else {
    g_assert_cmpint (g_remove (path), ==, 0);
  }
}

/* The probe exits with 77 when the theme it needs is not installed. */
static gboolean
probe_skipped (int status)
{
  g_autoptr (GError) error = NULL;

  return !g_spawn_check_wait_status (status, &error) &&
         g_error_matches (error, G_SPAWN_EXIT_ERROR, 77);
}

static void
test_theme_and_user_provider_isolation (void)
{
  const char *program = self_program;
  g_autofree char *temporary = g_dir_make_tmp ("awra-style-test-XXXXXX", NULL);
  g_autofree char *gtk_dir = g_build_filename (temporary, "gtk-4.0", NULL);
  g_autofree char *user_css_path = g_build_filename (gtk_dir, "gtk.css", NULL);
  g_autofree char *breeze = NULL;
  g_autofree char *breeze_dark = NULL;
  g_autofree char *user_file = NULL;
  g_autofree char *runtime_user = NULL;
  int status;

  g_assert_nonnull (program);
  g_assert_nonnull (temporary);
  g_assert_cmpint (g_mkdir_with_parents (gtk_dir, 0700), ==, 0);
  breeze = run_probe (program, "Breeze", temporary, FALSE, &status);
  if (probe_skipped (status)) {
    g_test_skip ("No display available for child style probes");
    goto cleanup;
  }
  g_assert_cmpint (status, ==, 0);
  breeze_dark = run_probe (program, "Breeze-Dark", temporary, FALSE, &status);
  g_assert_cmpint (status, ==, 0);
  g_assert_cmpstr (breeze, ==, breeze_dark);

  g_assert_true (g_file_set_contents (user_css_path, hostile_css, -1, NULL));
  user_file = run_probe (program, "Breeze", temporary, FALSE, &status);
  g_assert_cmpint (status, ==, 0);
  g_assert_cmpstr (breeze, ==, user_file);

  g_assert_cmpint (g_remove (user_css_path), ==, 0);
  runtime_user = run_probe (program, "Breeze", temporary, TRUE, &status);
  g_assert_cmpint (status, ==, 0);
  g_assert_cmpstr (breeze, ==, runtime_user);

cleanup:
  /* IM modules and desktop services may legitimately create cache sockets or
   * directories below an isolated XDG_CONFIG_HOME.  They are part of this
   * fixture's temporary tree, not evidence of a framework leak. */
  remove_test_tree (temporary);
}

int
main (int   argc,
      char *argv[])
{
  if (g_getenv ("AWRA_STYLE_PROBE_CHILD") != NULL)
    return run_probe_child ();

  self_program = argv[0];
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/awra/visual/theme-and-user-provider-isolation",
                   test_theme_and_user_provider_isolation);
  return g_test_run ();
}
