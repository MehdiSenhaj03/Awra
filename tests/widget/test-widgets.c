/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>
#include <glib/gstdio.h>
#include <math.h>

#include "layout/awra-split-view-private.h"
#include "widgets/awra-widget-style-private.h"

static guint click_count;

static gboolean
render_node_contains_type (GskRenderNode     *node,
                           GskRenderNodeType  type)
{
  GskRenderNode **children;
  gsize n_children;

  if (gsk_render_node_get_node_type (node) == type)
    return TRUE;

  children = gsk_render_node_get_children (node, &n_children);
  for (gsize i = 0; i < n_children; i++) {
    if (render_node_contains_type (children[i], type))
      return TRUE;
  }
  return FALSE;
}

static void
settle_window (GtkWidget *window)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  /* A bare Xvfb server has no window manager to synchronously acknowledge the
   * initial configure request.  Wait through the style/allocation boundary so
   * responsive assertions observe the requested window size on every GDK
   * backend, rather than GTK's transient natural allocation. */
  g_timeout_add_once (180, (GSourceOnceFunc) g_main_loop_quit, loop);
  g_main_loop_run (loop);
  gtk_test_widget_wait_for_draw (window);
}

typedef struct {
  guint requests;
  gboolean veto;
} TabCloseState;

static void
clicked_cb (AwraButton *button,
            gpointer    user_data)
{
  (void) button;
  (void) user_data;
  click_count++;
}

static gboolean
tab_close_page_cb (AwraTabView  *tabs,
                   GtkWidget    *page,
                   TabCloseState *state)
{
  (void) tabs;
  g_assert_true (GTK_IS_WIDGET (page));
  state->requests++;
  return state->veto;
}

static void
test_constructors (void)
{
  g_autoptr (GtkWidget) surface = g_object_ref_sink (
    awra_surface_new_with_role (AWRA_SURFACE_ROLE_CARD));
  GtkWidget *button = awra_button_new_with_label ("Activate");
  g_autoptr (AwraMaterial) material = awra_material_new_translucent ();

  g_assert_cmpint (awra_material_get_preset (
                     awra_surface_get_material (AWRA_SURFACE (surface))), ==,
                   AWRA_MATERIAL_PRESET_LAYER);
  awra_surface_set_material (AWRA_SURFACE (surface), material);
  awra_surface_set_child (AWRA_SURFACE (surface), button);
  g_assert_true (AWRA_IS_BUTTON (button));
  g_assert_true (awra_surface_get_child (AWRA_SURFACE (surface)) == button);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (button)),
                   ==,
                   GTK_ACCESSIBLE_ROLE_BUTTON);

  g_signal_connect (button, "clicked", G_CALLBACK (clicked_cb), NULL);
  g_signal_emit_by_name (button, "clicked");
  g_assert_cmpuint (click_count, ==, 1);
}

static void
test_builder (void)
{
  static const char xml[] =
    "<interface>"
    " <object class='AwraSurface' id='surface'>"
    "  <property name='role'>card</property>"
    "  <property name='child'>"
    "   <object class='AwraButton' id='button'>"
    "    <property name='label'>Builder button</property>"
    "    <property name='appearance'>primary</property>"
    "   </object>"
    "  </property>"
    " </object>"
    " <object class='AwraEdgePanel' id='edge-panel'>"
    "  <property name='edge'>start</property>"
    "  <property name='auto-hide'>1</property>"
    "  <property name='revealed'>0</property>"
    "  <property name='content'>"
    "   <object class='GtkLabel'><property name='label'>Canvas</property></object>"
    "  </property>"
    "  <property name='panel'>"
    "   <object class='AwraActionRow' id='action-row'>"
    "    <property name='title'>Inspector</property>"
    "    <property name='subtitle'>Edge revealed</property>"
    "    <property name='icon-name'>preferences-system-symbolic</property>"
    "   </object>"
    "  </property>"
    " </object>"
    " <object class='AwraPage' id='page'>"
    "  <property name='max-content-width'>840</property>"
    "  <property name='content-inset'>xl</property>"
    "  <property name='child'>"
    "   <object class='GtkLabel'><property name='label'>Page content</property></object>"
    "  </property>"
    " </object>"
    " <object class='AwraPageHeader' id='page-header'>"
    "  <property name='title'>Library</property>"
    "  <property name='subtitle'>A typed page heading</property>"
    " </object>"
    " <object class='AwraSection' id='section'>"
    "  <property name='title'>Recent work</property>"
    "  <property name='description'>No application spacing required</property>"
    "  <property name='child'>"
    "   <object class='GtkLabel'><property name='label'>Section content</property></object>"
    "  </property>"
    " </object>"
    " <object class='AwraToolbar' id='toolbar'>"
    "  <property name='compact'>1</property>"
    "  <property name='start-widget'>"
    "   <object class='AwraButton'><property name='label'>Back</property></object>"
    "  </property>"
    " </object>"
    " <object class='AwraForm' id='form'>"
    "  <child type='row'>"
    "   <object class='AwraFormRow' id='form-row'>"
    "    <property name='label'>Account</property>"
    "    <property name='help-text'>Public profile name</property>"
    "    <property name='required'>1</property>"
    "    <property name='control'>"
    "     <object class='AwraEntry'/>"
    "    </property>"
    "   </object>"
    "  </child>"
    " </object>"
    " <object class='AwraEmptyState' id='empty-state'>"
    "  <property name='title'>No documents</property>"
    "  <property name='description'>Create the first document.</property>"
    " </object>"
    " <object class='AwraLoadingState' id='loading-state'>"
    "  <property name='title'>Loading library</property>"
    "  <property name='active'>0</property>"
    " </object>"
    " <object class='AwraErrorState' id='error-state'>"
    "  <property name='title'>Library unavailable</property>"
    "  <property name='description'>Try again later.</property>"
    " </object>"
    " <object class='AwraFilterBar' id='filter-bar'>"
    "  <child type='search'><object class='AwraSearchEntry'/></child>"
    "  <child type='filter'><object class='AwraDropdown'/></child>"
    "  <child type='summary'><object class='GtkLabel'><property name='label'>12 results</property></object></child>"
    "  <child type='reset'><object class='AwraButton'><property name='label'>Reset</property></object></child>"
    " </object>"
    " <object class='AwraMasterDetail' id='master-detail'>"
    "  <property name='master'><object class='GtkLabel'><property name='label'>Master</property></object></property>"
    "  <property name='detail'><object class='GtkLabel'><property name='label'>Detail</property></object></property>"
    "  <property name='visible-pane'>detail</property>"
    " </object>"
    " <object class='AwraDetailPane' id='detail-pane'>"
    "  <property name='title'>Bulbasaur</property>"
    "  <property name='subtitle'>Sample item</property>"
    "  <child type='section'>"
    "   <object class='AwraSection'><property name='title'>Overview</property></object>"
    "  </child>"
    " </object>"
    " <object class='AwraDataView' id='data-view'>"
    "  <property name='state'>empty</property>"
    "  <child type='column'>"
    "   <object class='AwraDataColumn' id='data-column'>"
    "    <property name='title'>Name</property>"
    "    <property name='expand'>1</property>"
    "   </object>"
    "  </child>"
    " </object>"
    " <object class='AwraSelectionToolbar' id='selection-toolbar'>"
    "  <property name='title'>record</property>"
    "  <property name='auto-hide'>0</property>"
    " </object>"
    " <object class='AwraTextArea' id='text-area'>"
    "  <property name='placeholder-text'>Notes</property>"
    "  <property name='text'>Builder text</property>"
    " </object>"
    " <object class='AwraPasswordEntry' id='password-entry'>"
    "  <property name='placeholder-text'>Password</property>"
    " </object>"
    " <object class='AwraTagEntry' id='tag-entry'><property name='editable'>1</property></object>"
    " <object class='AwraChoiceGroup' id='choice-group'>"
    "  <property name='title'>Notifications</property>"
    "  <property name='exclusive'>1</property>"
    "  <child type='choice'><object class='AwraCheckButton'><property name='label'>All</property></object></child>"
    " </object>"
    " <object class='AwraStatusBanner' id='status-banner'>"
    "  <property name='title'>Offline</property>"
    "  <property name='appearance'>warning</property>"
    " </object>"
    " <object class='AwraResponsiveBin' id='responsive-bin'>"
    "  <property name='compact-width'>540</property>"
    "  <property name='expanded-width'>880</property>"
    "  <property name='child'><object class='GtkLabel'><property name='label'>Adaptive</property></object></property>"
    " </object>"
    " <object class='AwraBreadcrumbBar' id='breadcrumb-bar'/>"
    " <object class='AwraStatBar' id='stat-bar'><property name='label'>HP</property><property name='value'>45</property><property name='maximum'>100</property></object>"
    " <object class='AwraBadgeGroup' id='badge-group'>"
    "  <child type='badge'><object class='AwraBadge'><property name='text'>GRASS</property></object></child>"
    " </object>"
    " <object class='AwraMetadataGroup' id='metadata-group'>"
    "  <property name='title'>Metadata</property>"
    "  <child type='row'><object class='AwraMetricRow'><property name='label'>Height</property><property name='value'>0.7 m</property></object></child>"
    " </object>"
    " <object class='AwraSkeleton' id='skeleton'><property name='lines'>4</property><property name='animated'>0</property></object>"
    "</interface>";
  g_autoptr (GtkBuilder) builder = gtk_builder_new ();
  g_autoptr (GError) error = NULL;
  GObject *surface;
  GObject *button;
  GObject *edge_panel;
  GObject *action_row;
  GObject *page;
  GObject *page_header;
  GObject *section;
  GObject *toolbar;
  GObject *form;
  GObject *form_row;
  GObject *empty_state;
  GObject *loading_state;
  GObject *error_state;
  GObject *filter_bar;
  GObject *master_detail;
  GObject *detail_pane;
  GObject *data_view;
  GObject *data_column;
  GObject *selection_toolbar;
  GObject *text_area;
  GObject *password_entry;
  GObject *tag_entry;
  GObject *choice_group;
  GObject *status_banner;
  GObject *responsive_bin;
  GObject *breadcrumb_bar;
  GObject *stat_bar;
  GObject *badge_group;
  GObject *metadata_group;
  GObject *skeleton;

  g_type_ensure (AWRA_TYPE_SURFACE);
  g_type_ensure (AWRA_TYPE_BUTTON);
  g_type_ensure (AWRA_TYPE_EDGE_PANEL);
  g_type_ensure (AWRA_TYPE_ACTION_ROW);
  g_type_ensure (AWRA_TYPE_PAGE);
  g_type_ensure (AWRA_TYPE_PAGE_HEADER);
  g_type_ensure (AWRA_TYPE_SECTION);
  g_type_ensure (AWRA_TYPE_TOOLBAR);
  g_type_ensure (AWRA_TYPE_FORM);
  g_type_ensure (AWRA_TYPE_FORM_ROW);
  g_type_ensure (AWRA_TYPE_ENTRY);
  g_type_ensure (AWRA_TYPE_EMPTY_STATE);
  g_type_ensure (AWRA_TYPE_LOADING_STATE);
  g_type_ensure (AWRA_TYPE_ERROR_STATE);
  g_type_ensure (AWRA_TYPE_FILTER_BAR);
  g_type_ensure (AWRA_TYPE_MASTER_DETAIL);
  g_type_ensure (AWRA_TYPE_DETAIL_PANE);
  g_type_ensure (AWRA_TYPE_SEARCH_ENTRY);
  g_type_ensure (AWRA_TYPE_DROPDOWN);
  g_type_ensure (AWRA_TYPE_DATA_VIEW);
  g_type_ensure (AWRA_TYPE_DATA_COLUMN);
  g_type_ensure (AWRA_TYPE_SELECTION_TOOLBAR);
  g_type_ensure (AWRA_TYPE_TEXT_AREA);
  g_type_ensure (AWRA_TYPE_PASSWORD_ENTRY);
  g_type_ensure (AWRA_TYPE_TAG_ENTRY);
  g_type_ensure (AWRA_TYPE_CHOICE_GROUP);
  g_type_ensure (AWRA_TYPE_CHECK_BUTTON);
  g_type_ensure (AWRA_TYPE_STATUS_BANNER);
  g_type_ensure (AWRA_TYPE_RESPONSIVE_BIN);
  g_type_ensure (AWRA_TYPE_BREADCRUMB_BAR);
  g_type_ensure (AWRA_TYPE_STAT_BAR);
  g_type_ensure (AWRA_TYPE_BADGE_GROUP);
  g_type_ensure (AWRA_TYPE_BADGE);
  g_type_ensure (AWRA_TYPE_METADATA_GROUP);
  g_type_ensure (AWRA_TYPE_METRIC_ROW);
  g_type_ensure (AWRA_TYPE_SKELETON);
  gtk_builder_add_from_string (builder, xml, -1, &error);
  g_assert_no_error (error);
  surface = gtk_builder_get_object (builder, "surface");
  button = gtk_builder_get_object (builder, "button");
  edge_panel = gtk_builder_get_object (builder, "edge-panel");
  action_row = gtk_builder_get_object (builder, "action-row");
  page = gtk_builder_get_object (builder, "page");
  page_header = gtk_builder_get_object (builder, "page-header");
  section = gtk_builder_get_object (builder, "section");
  toolbar = gtk_builder_get_object (builder, "toolbar");
  form = gtk_builder_get_object (builder, "form");
  form_row = gtk_builder_get_object (builder, "form-row");
  empty_state = gtk_builder_get_object (builder, "empty-state");
  loading_state = gtk_builder_get_object (builder, "loading-state");
  error_state = gtk_builder_get_object (builder, "error-state");
  filter_bar = gtk_builder_get_object (builder, "filter-bar");
  master_detail = gtk_builder_get_object (builder, "master-detail");
  detail_pane = gtk_builder_get_object (builder, "detail-pane");
  data_view = gtk_builder_get_object (builder, "data-view");
  data_column = gtk_builder_get_object (builder, "data-column");
  selection_toolbar = gtk_builder_get_object (builder, "selection-toolbar");
  text_area = gtk_builder_get_object (builder, "text-area");
  password_entry = gtk_builder_get_object (builder, "password-entry");
  tag_entry = gtk_builder_get_object (builder, "tag-entry");
  choice_group = gtk_builder_get_object (builder, "choice-group");
  status_banner = gtk_builder_get_object (builder, "status-banner");
  responsive_bin = gtk_builder_get_object (builder, "responsive-bin");
  breadcrumb_bar = gtk_builder_get_object (builder, "breadcrumb-bar");
  stat_bar = gtk_builder_get_object (builder, "stat-bar");
  badge_group = gtk_builder_get_object (builder, "badge-group");
  metadata_group = gtk_builder_get_object (builder, "metadata-group");
  skeleton = gtk_builder_get_object (builder, "skeleton");
  g_assert_true (AWRA_IS_SURFACE (surface));
  g_assert_true (AWRA_IS_BUTTON (button));
  g_assert_cmpstr (gtk_button_get_label (GTK_BUTTON (button)),
                   ==,
                   "Builder button");
  g_assert_cmpint (awra_button_get_appearance (AWRA_BUTTON (button)), ==,
                   AWRA_BUTTON_APPEARANCE_PRIMARY);
  g_assert_true (AWRA_IS_EDGE_PANEL (edge_panel));
  g_assert_cmpint (awra_edge_panel_get_edge (AWRA_EDGE_PANEL (edge_panel)), ==,
                   AWRA_PANEL_EDGE_START);
  g_assert_true (awra_edge_panel_get_auto_hide (
    AWRA_EDGE_PANEL (edge_panel)));
  g_assert_false (awra_edge_panel_get_revealed (
    AWRA_EDGE_PANEL (edge_panel)));
  g_assert_true (AWRA_IS_ACTION_ROW (action_row));
  g_assert_cmpstr (awra_action_row_get_title (AWRA_ACTION_ROW (action_row)),
                   ==, "Inspector");
  g_assert_true (AWRA_IS_PAGE (page));
  g_assert_cmpuint (awra_page_get_max_content_width (AWRA_PAGE (page)), ==,
                    840);
  g_assert_cmpint (awra_page_get_content_inset (AWRA_PAGE (page)), ==,
                   AWRA_SPACING_XL);
  g_assert_true (AWRA_IS_PAGE_HEADER (page_header));
  g_assert_cmpstr (awra_page_header_get_title (
                     AWRA_PAGE_HEADER (page_header)), ==, "Library");
  g_assert_true (AWRA_IS_SECTION (section));
  g_assert_cmpstr (awra_section_get_description (AWRA_SECTION (section)), ==,
                   "No application spacing required");
  g_assert_true (AWRA_IS_TOOLBAR (toolbar));
  g_assert_true (awra_toolbar_get_compact (AWRA_TOOLBAR (toolbar)));
  g_assert_true (AWRA_IS_BUTTON (awra_toolbar_get_start_widget (
                                  AWRA_TOOLBAR (toolbar))));
  g_assert_true (AWRA_IS_FORM (form));
  g_assert_cmpuint (awra_form_get_n_rows (AWRA_FORM (form)), ==, 1);
  g_assert_true (awra_form_get_row (AWRA_FORM (form), 0) ==
                 AWRA_FORM_ROW (form_row));
  g_assert_true (AWRA_IS_ENTRY (awra_form_row_get_control (
                                 AWRA_FORM_ROW (form_row))));
  g_assert_true (awra_form_row_get_required (AWRA_FORM_ROW (form_row)));
  g_assert_cmpstr (awra_empty_state_get_title (
                     AWRA_EMPTY_STATE (empty_state)), ==, "No documents");
  g_assert_false (awra_loading_state_get_active (
                    AWRA_LOADING_STATE (loading_state)));
  g_assert_cmpstr (awra_error_state_get_description (
                     AWRA_ERROR_STATE (error_state)), ==, "Try again later.");
  g_assert_cmpuint (awra_filter_bar_get_n_filters (
                      AWRA_FILTER_BAR (filter_bar)), ==, 1);
  g_assert_true (AWRA_IS_SEARCH_ENTRY (awra_filter_bar_get_search (
                                        AWRA_FILTER_BAR (filter_bar))));
  g_assert_cmpint (awra_master_detail_get_visible_pane (
                     AWRA_MASTER_DETAIL (master_detail)), ==,
                   AWRA_MASTER_DETAIL_PANE_DETAIL);
  g_assert_cmpstr (awra_detail_pane_get_title (
                     AWRA_DETAIL_PANE (detail_pane)), ==, "Bulbasaur");
  g_assert_cmpuint (awra_detail_pane_get_n_sections (
                      AWRA_DETAIL_PANE (detail_pane)), ==, 1);
  g_assert_true (AWRA_IS_DATA_VIEW (data_view));
  g_assert_cmpint (awra_data_view_get_state (AWRA_DATA_VIEW (data_view)), ==,
                   AWRA_DATA_VIEW_STATE_EMPTY);
  g_assert_cmpuint (awra_data_view_get_n_columns (
                      AWRA_DATA_VIEW (data_view)), ==, 1);
  g_assert_true (awra_data_view_get_column (AWRA_DATA_VIEW (data_view), 0) ==
                 AWRA_DATA_COLUMN (data_column));
  g_assert_true (awra_data_column_get_expand (
                   AWRA_DATA_COLUMN (data_column)));
  g_assert_true (AWRA_IS_SELECTION_TOOLBAR (selection_toolbar));
  g_assert_false (awra_selection_toolbar_get_auto_hide (
                    AWRA_SELECTION_TOOLBAR (selection_toolbar)));
  g_assert_cmpstr (awra_text_area_get_text (AWRA_TEXT_AREA (text_area)), ==,
                   "Builder text");
  g_assert_cmpstr (awra_password_entry_get_placeholder_text (
                     AWRA_PASSWORD_ENTRY (password_entry)), ==, "Password");
  g_assert_true (awra_tag_entry_get_editable (AWRA_TAG_ENTRY (tag_entry)));
  g_assert_cmpuint (awra_choice_group_get_n_choices (
                      AWRA_CHOICE_GROUP (choice_group)), ==, 1);
  g_assert_cmpint (awra_status_banner_get_appearance (
                     AWRA_STATUS_BANNER (status_banner)), ==,
                   AWRA_STATUS_APPEARANCE_WARNING);
  g_assert_true (AWRA_IS_RESPONSIVE_BIN (responsive_bin));
  g_assert_nonnull (awra_responsive_bin_get_child (
    AWRA_RESPONSIVE_BIN (responsive_bin)));
  g_assert_cmpuint (awra_responsive_bin_get_compact_width (
                      AWRA_RESPONSIVE_BIN (responsive_bin)), ==, 540);
  g_assert_true (AWRA_IS_BREADCRUMB_BAR (breadcrumb_bar));
  g_assert_cmpfloat (awra_stat_bar_get_value (AWRA_STAT_BAR (stat_bar)), ==,
                     45.0);
  g_assert_cmpuint (awra_badge_group_get_n_badges (
                      AWRA_BADGE_GROUP (badge_group)), ==, 1);
  g_assert_cmpuint (awra_metadata_group_get_n_rows (
                      AWRA_METADATA_GROUP (metadata_group)), ==, 1);
  g_assert_cmpuint (awra_skeleton_get_lines (AWRA_SKELETON (skeleton)), ==, 4);
  g_assert_false (awra_skeleton_get_animated (AWRA_SKELETON (skeleton)));
}

static void
test_controls (void)
{
  g_autoptr (GtkWidget) box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
  GtkWidget *toggle = awra_toggle_button_new_with_label ("Toggle");
  GtkWidget *entry = awra_entry_new ();
  GtkWidget *search = awra_search_entry_new ();
  GtkWidget *switcher = awra_switch_new ();
  GtkWidget *slider = awra_slider_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 10, 1);
  GtkWidget *check = awra_check_button_new_with_label ("Check");
  GtkWidget *radio = awra_check_button_new_with_label ("Radio");
  GtkWidget *spin = awra_spin_button_new_with_range (-5, 5, 0.5);
  GtkWidget *button = awra_button_new_with_label ("Primary");

  gtk_box_append (GTK_BOX (box), toggle);
  gtk_box_append (GTK_BOX (box), entry);
  gtk_box_append (GTK_BOX (box), search);
  gtk_box_append (GTK_BOX (box), switcher);
  gtk_box_append (GTK_BOX (box), slider);
  gtk_box_append (GTK_BOX (box), check);
  gtk_box_append (GTK_BOX (box), radio);
  gtk_box_append (GTK_BOX (box), spin);
  gtk_box_append (GTK_BOX (box), button);

  awra_button_set_appearance (AWRA_BUTTON (button),
                              AWRA_BUTTON_APPEARANCE_PRIMARY);
  g_assert_cmpint (awra_button_get_appearance (AWRA_BUTTON (button)), ==,
                   AWRA_BUTTON_APPEARANCE_PRIMARY);
  awra_toggle_button_set_appearance (AWRA_TOGGLE_BUTTON (toggle),
                                     AWRA_BUTTON_APPEARANCE_GHOST);
  g_assert_cmpint (awra_toggle_button_get_appearance (
                     AWRA_TOGGLE_BUTTON (toggle)), ==,
                   AWRA_BUTTON_APPEARANCE_GHOST);

  gtk_editable_set_text (GTK_EDITABLE (entry), "IME-ready");
  g_assert_cmpstr (gtk_editable_get_text (GTK_EDITABLE (entry)), ==, "IME-ready");
  awra_search_entry_set_text (AWRA_SEARCH_ENTRY (search), "query");
  awra_search_entry_set_placeholder_text (AWRA_SEARCH_ENTRY (search), "Search");
  g_assert_cmpstr (awra_search_entry_get_text (AWRA_SEARCH_ENTRY (search)), ==, "query");
  g_assert_cmpstr (awra_search_entry_get_placeholder_text (AWRA_SEARCH_ENTRY (search)), ==, "Search");
  awra_switch_set_active (AWRA_SWITCH (switcher), TRUE);
  g_assert_true (awra_switch_get_active (AWRA_SWITCH (switcher)));
  g_assert_cmpint (gtk_widget_get_valign (switcher), ==, GTK_ALIGN_CENTER);
  g_assert_cmpint (gtk_widget_get_valign (GTK_WIDGET (
                     awra_switch_get_delegate (AWRA_SWITCH (switcher)))), ==,
                   GTK_ALIGN_CENTER);
  gtk_range_set_value (GTK_RANGE (slider), 7.0);
  g_assert_cmpfloat (gtk_range_get_value (GTK_RANGE (slider)), ==, 7.0);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
  g_assert_true (gtk_check_button_get_active (GTK_CHECK_BUTTON (check)));
  awra_check_button_set_group (AWRA_CHECK_BUTTON (radio), AWRA_CHECK_BUTTON (check));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
  g_assert_false (gtk_check_button_get_active (GTK_CHECK_BUTTON (check)));
  awra_spin_button_set_value (AWRA_SPIN_BUTTON (spin), 2.5);
  g_assert_cmpfloat (awra_spin_button_get_value (AWRA_SPIN_BUTTON (spin)), ==, 2.5);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
test_layout_components (void)
{
  g_autoptr (GtkWidget) split = g_object_ref_sink (awra_split_view_new ());
  GtkWidget *sidebar = awra_sidebar_new ();
  GtkWidget *content = awra_card_new ();
  GtkWidget *header = awra_header_new ();
  GtkWidget *start = awra_button_new_with_label ("Back");
  GtkWidget *center = awra_segmented_control_new ();
  GtkWidget *end = awra_button_new_with_label ("Share");
  GtkWidget *card_content;
  int medium = awra_spacing_resolve (sidebar, AWRA_SPACING_MD);
  int large = awra_spacing_resolve (content, AWRA_SPACING_LG);
  int small = awra_spacing_resolve (content, AWRA_SPACING_SM);

  g_assert_cmpuint (awra_sidebar_get_inset (AWRA_SIDEBAR (sidebar)), ==,
                    (guint) medium);
  awra_sidebar_set_child (AWRA_SIDEBAR (sidebar), gtk_label_new ("Navigation"));
  awra_sidebar_set_inset_spacing (AWRA_SIDEBAR (sidebar), AWRA_SPACING_MD);
  g_assert_cmpuint (awra_sidebar_get_inset (AWRA_SIDEBAR (sidebar)), ==,
                    (guint) medium);
  g_assert_cmpint (gtk_widget_get_margin_start (GTK_WIDGET (
                     awra_sidebar_get_surface (AWRA_SIDEBAR (sidebar)))),
                   ==, medium);
  g_assert_cmpint (gtk_widget_get_margin_bottom (GTK_WIDGET (
                     awra_sidebar_get_surface (AWRA_SIDEBAR (sidebar)))),
                   ==, medium);
  awra_card_set_child (AWRA_CARD (content), gtk_label_new ("Content"));
  g_assert_cmpint (awra_card_get_content_inset (AWRA_CARD (content)), ==,
                   AWRA_SPACING_LG);
  card_content = awra_surface_get_child (
    awra_card_get_surface (AWRA_CARD (content)));
  g_assert_cmpint (gtk_widget_get_margin_start (card_content), ==, large);
  g_assert_cmpint (gtk_widget_get_margin_bottom (card_content), ==, large);
  awra_card_set_content_inset (AWRA_CARD (content), AWRA_SPACING_SM);
  g_assert_cmpint (gtk_widget_get_margin_start (card_content), ==, small);
  awra_header_set_title (AWRA_HEADER (header), "Library");
  awra_header_set_subtitle (AWRA_HEADER (header), "Responsive foundation");
  awra_header_set_start_widget (AWRA_HEADER (header), start);
  awra_header_set_center_widget (AWRA_HEADER (header), center);
  awra_header_set_end_widget (AWRA_HEADER (header), end);
  awra_header_set_show_window_controls (AWRA_HEADER (header), TRUE);
  awra_header_set_drag_enabled (AWRA_HEADER (header), TRUE);
  awra_header_set_blend_with_window (AWRA_HEADER (header), TRUE);
  g_assert_true (awra_header_get_start_widget (AWRA_HEADER (header)) == start);
  g_assert_true (awra_header_get_center_widget (AWRA_HEADER (header)) == center);
  g_assert_true (awra_header_get_end_widget (AWRA_HEADER (header)) == end);
  g_assert_cmpint (gtk_widget_get_valign (start), ==, GTK_ALIGN_CENTER);
  g_assert_cmpint (gtk_widget_get_valign (center), ==, GTK_ALIGN_CENTER);
  g_assert_cmpint (gtk_widget_get_valign (end), ==, GTK_ALIGN_CENTER);
  g_assert_true (awra_header_get_show_window_controls (AWRA_HEADER (header)));
  g_assert_true (awra_header_get_drag_enabled (AWRA_HEADER (header)));
  g_assert_true (awra_header_get_blend_with_window (AWRA_HEADER (header)));
  g_assert_cmpint (awra_material_get_preset (
                     awra_header_get_material (AWRA_HEADER (header))), ==,
                   AWRA_MATERIAL_PRESET_CANVAS);
  awra_header_set_drag_enabled (AWRA_HEADER (header), FALSE);
  g_assert_false (awra_header_get_drag_enabled (AWRA_HEADER (header)));

  awra_split_view_set_sidebar (AWRA_SPLIT_VIEW (split), sidebar);
  awra_split_view_set_content (AWRA_SPLIT_VIEW (split), content);
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 520);
  g_assert_cmpint (awra_split_view_get_layout_mode (AWRA_SPLIT_VIEW (split)),
                   ==,
                   AWRA_LAYOUT_MODE_COMPACT);
  g_assert_false (gtk_widget_get_visible (sidebar));
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 760);
  g_assert_cmpint (awra_split_view_get_layout_mode (AWRA_SPLIT_VIEW (split)),
                   ==,
                   AWRA_LAYOUT_MODE_MEDIUM);
  g_assert_true (gtk_widget_get_visible (sidebar));
  {
    int sidebar_width;

    gtk_widget_get_size_request (sidebar, &sidebar_width, NULL);
    g_assert_cmpint (sidebar_width, ==, 234);
  }
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 1100);
  g_assert_cmpint (awra_split_view_get_layout_mode (AWRA_SPLIT_VIEW (split)),
                   ==,
                   AWRA_LAYOUT_MODE_EXPANDED);
  g_assert_true (gtk_widget_get_visible (sidebar));
  {
    int sidebar_width;

    gtk_widget_get_size_request (sidebar, &sidebar_width, NULL);
    g_assert_cmpint (sidebar_width, ==, 254);
  }
  g_object_ref_sink (header);
  g_object_unref (header);
}

static void
test_public_layout_recipes (void)
{
  g_autoptr (GtkWidget) page = g_object_ref_sink (
    gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  g_autoptr (GtkWidget) section = g_object_ref_sink (
    gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  g_autoptr (GtkWidget) grid = g_object_ref_sink (gtk_grid_new ());
  int page_inset = awra_spacing_resolve (page, AWRA_SPACING_XXL);
  int page_gap = awra_spacing_resolve (page, AWRA_SPACING_XL);
  int section_gap = awra_spacing_resolve (section, AWRA_SPACING_XS);
  int toolbar_inset = awra_spacing_resolve (grid, AWRA_SPACING_MD);
  int toolbar_gap = awra_spacing_resolve (grid, AWRA_SPACING_SM);

  awra_box_apply_layout_preset (GTK_BOX (page), AWRA_LAYOUT_PRESET_PAGE);
  g_assert_cmpint (gtk_widget_get_margin_top (page), ==, page_inset);
  g_assert_cmpint (gtk_widget_get_margin_start (page), ==, page_inset);
  g_assert_cmpint (gtk_box_get_spacing (GTK_BOX (page)), ==, page_gap);

  awra_box_apply_layout_preset (GTK_BOX (section),
                                AWRA_LAYOUT_PRESET_SECTION);
  g_assert_cmpint (gtk_widget_get_margin_top (section), ==, 0);
  g_assert_cmpint (gtk_box_get_spacing (GTK_BOX (section)), ==, section_gap);

  awra_grid_apply_layout_preset (GTK_GRID (grid),
                                 AWRA_LAYOUT_PRESET_TOOLBAR);
  g_assert_cmpint (gtk_widget_get_margin_end (grid), ==, toolbar_inset);
  g_assert_cmpuint (gtk_grid_get_row_spacing (GTK_GRID (grid)), ==,
                    (guint) toolbar_gap);
  g_assert_cmpuint (gtk_grid_get_column_spacing (GTK_GRID (grid)), ==,
                    (guint) toolbar_gap);
  g_assert_cmpint (awra_spacing_resolve (page, AWRA_SPACING_NONE), ==, 0);
}

static void
test_mapped_header_center_geometry (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *header = awra_header_new ();
  GtkWidget *start = gtk_button_new_with_label ("Workspace");
  GtkWidget *center = gtk_search_entry_new ();
  GtkWidget *end = gtk_button_new_with_label ("Menu");
  graphene_rect_t bounds;
  float header_center;
  float widget_center;

  gtk_widget_set_size_request (start, 180, -1);
  gtk_widget_set_size_request (center, 120, -1);
  gtk_widget_set_hexpand (center, TRUE);
  gtk_widget_set_size_request (end, 72, -1);
  awra_header_set_start_widget (AWRA_HEADER (header), start);
  awra_header_set_center_widget (AWRA_HEADER (header), center);
  awra_header_set_end_widget (AWRA_HEADER (header), end);

  gtk_window_set_child (GTK_WINDOW (window), header);
  gtk_window_set_default_size (GTK_WINDOW (window), 1000, 100);
  gtk_widget_set_size_request (window, 1000, 100);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);

  g_assert_true (gtk_widget_compute_bounds (center, header, &bounds));
  g_assert_cmpfloat (bounds.size.width, >=, 560.0f);
  g_assert_cmpfloat (bounds.size.width, <=, 576.0f);
  header_center = gtk_widget_get_width (header) / 2.0f;
  widget_center = bounds.origin.x + bounds.size.width / 2.0f;
  g_assert_cmpfloat (fabsf (widget_center - header_center), <=, 1.0f);

  /* Address controls may fill the actual gap instead of reserving symmetric
   * gutters. Existing headers keep their centered allocation by default. */
  g_assert_false (awra_header_get_expand_center (AWRA_HEADER (header)));
  float centered_width = bounds.size.width;
  awra_header_set_expand_center (AWRA_HEADER (header), TRUE);
  settle_window (window);
  graphene_rect_t start_bounds, end_bounds;
  g_assert_true (gtk_widget_compute_bounds (center, header, &bounds));
  g_assert_true (gtk_widget_compute_bounds (start, header, &start_bounds));
  g_assert_true (gtk_widget_compute_bounds (end, header, &end_bounds));
  g_assert_cmpfloat (bounds.size.width, >, centered_width + 50);
  g_assert_cmpfloat (bounds.origin.x, >=, start_bounds.origin.x + start_bounds.size.width);
  g_assert_cmpfloat (bounds.origin.x + bounds.size.width, <=, end_bounds.origin.x);
  g_object_set (header, "expand-center", FALSE, NULL);
  settle_window (window);
  g_assert_false (awra_header_get_expand_center (AWRA_HEADER (header)));
  g_assert_true (gtk_widget_compute_bounds (center, header, &bounds));
  g_assert_cmpfloat (fabsf (bounds.size.width - centered_width), <=, 1.0f);
  gtk_window_destroy (GTK_WINDOW (window));
}

static GtkWidget *
find_descendant_with_class (GtkWidget  *root,
                            const char *css_class)
{
  GtkWidget *child;

  if (gtk_widget_has_css_class (root, css_class))
    return root;
  for (child = gtk_widget_get_first_child (root);
       child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    GtkWidget *match = find_descendant_with_class (child, css_class);

    if (match != NULL)
      return match;
  }
  return NULL;
}

static void
test_mapped_header_title_alignment (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *header = awra_header_new ();
  GtkWidget *end = gtk_button_new_from_icon_name ("window-close-symbolic");
  GtkWidget *title;
  graphene_rect_t title_bounds;
  graphene_rect_t end_bounds;
  float title_center;
  float end_center;

  awra_header_set_title (AWRA_HEADER (header), "Compact dialog title");
  gtk_widget_set_size_request (end, 24, 24);
  awra_header_set_end_widget (AWRA_HEADER (header), end);
  gtk_window_set_child (GTK_WINDOW (window), header);
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 64);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);

  title = find_descendant_with_class (header, "awra-header-title");
  g_assert_nonnull (title);
  g_assert_true (gtk_widget_compute_bounds (title, header, &title_bounds));
  g_assert_true (gtk_widget_compute_bounds (end, header, &end_bounds));
  title_center = title_bounds.origin.y + title_bounds.size.height / 2.0f;
  end_center = end_bounds.origin.y + end_bounds.size.height / 2.0f;
  g_assert_cmpfloat (fabsf (title_center - end_center), <=, 1.0f);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_typed_page_components (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestTypedPage", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *page = awra_page_new ();
  GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *header = awra_page_header_new ();
  GtkWidget *section = awra_section_new ();
  GtkWidget *toolbar = awra_toolbar_new ();
  GtkWidget *actions = awra_button_new_with_label ("Create");
  GtkWidget *section_actions = awra_button_new_with_label ("Refresh");
  GtkWidget *start = awra_button_new_with_label ("Back");
  GtkWidget *center = awra_segmented_control_new ();
  GtkWidget *end = awra_button_new_with_label ("Share");
  graphene_point_t origin = GRAPHENE_POINT_INIT (0, 0);
  graphene_point_t position;
  guint page_width;
  guint child_width;

  awra_box_set_spacing (GTK_BOX (content), AWRA_SPACING_XL);
  awra_page_header_set_title (AWRA_PAGE_HEADER (header), "Workspace");
  awra_page_header_set_subtitle (AWRA_PAGE_HEADER (header),
                                 "Responsive by default");
  awra_page_header_set_actions (AWRA_PAGE_HEADER (header), actions);
  awra_page_header_set_child (AWRA_PAGE_HEADER (header),
                              gtk_label_new ("Context"));
  awra_section_set_title (AWRA_SECTION (section), "Documents");
  awra_section_set_description (AWRA_SECTION (section),
                                "A semantic, unboxed section");
  awra_section_set_actions (AWRA_SECTION (section), section_actions);
  awra_section_set_child (AWRA_SECTION (section),
                          gtk_label_new ("Section body"));
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (center), "Grid");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (center), "List");
  awra_toolbar_set_start_widget (AWRA_TOOLBAR (toolbar), start);
  awra_toolbar_set_center_widget (AWRA_TOOLBAR (toolbar), center);
  awra_toolbar_set_end_widget (AWRA_TOOLBAR (toolbar), end);
  awra_toolbar_set_compact (AWRA_TOOLBAR (toolbar), TRUE);
  gtk_box_append (GTK_BOX (content), header);
  gtk_box_append (GTK_BOX (content), toolbar);
  gtk_box_append (GTK_BOX (content), section);
  awra_page_set_max_content_width (AWRA_PAGE (page), 840);
  awra_page_set_content_inset (AWRA_PAGE (page), AWRA_SPACING_XL);
  awra_page_set_child (AWRA_PAGE (page), content);

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  awra_window_set_content (AWRA_WINDOW (window), page);
  gtk_window_set_default_size (GTK_WINDOW (window), 1200, 700);
  /* default-size is advisory and a bare Xvfb server has no window manager to
   * honor it.  This fixture needs a genuinely wide allocation to validate the
   * page's max-content-width and centering contract. */
  gtk_widget_set_size_request (window, 1200, 700);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);

  page_width = gtk_widget_get_width (page);
  child_width = gtk_widget_get_width (content);
  g_assert_cmpuint (page_width, >=, 1000);
  g_assert_cmpuint (child_width, <=, 840);
  g_assert_cmpuint (child_width, >, 700);
  g_assert_true (gtk_widget_compute_point (content, page, &origin, &position));
  g_assert_cmpfloat (position.x, >, 100.0f);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (page)), ==, GTK_ACCESSIBLE_ROLE_MAIN);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (toolbar)), ==,
                   GTK_ACCESSIBLE_ROLE_TOOLBAR);
  g_assert_true (awra_page_get_child (AWRA_PAGE (page)) == content);
  g_assert_true (awra_page_header_get_actions (AWRA_PAGE_HEADER (header)) ==
                 actions);
  g_assert_true (awra_section_get_child (AWRA_SECTION (section)) != NULL);

  awra_page_scroll_to_top (AWRA_PAGE (page));
  awra_page_header_set_actions (AWRA_PAGE_HEADER (header), NULL);
  awra_section_set_actions (AWRA_SECTION (section), NULL);
  awra_toolbar_set_start_widget (AWRA_TOOLBAR (toolbar), NULL);
  g_assert_null (awra_page_header_get_actions (AWRA_PAGE_HEADER (header)));
  g_assert_null (awra_section_get_actions (AWRA_SECTION (section)));
  g_assert_null (awra_toolbar_get_start_widget (AWRA_TOOLBAR (toolbar)));
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_form_and_page_states (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestFormStates", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *form = awra_form_new ();
  GtkWidget *name_row = awra_form_row_new ();
  GtkWidget *email_row = awra_form_row_new ();
  GtkWidget *name_entry = awra_entry_new ();
  GtkWidget *email_entry = awra_entry_new ();
  GtkWidget *empty = awra_empty_state_new ();
  GtkWidget *loading = awra_loading_state_new ();
  GtkWidget *error_state = awra_error_state_new ();
  GtkWidget *retry = awra_button_new_with_label ("Retry");

  awra_form_row_set_label (AWRA_FORM_ROW (name_row), "Name");
  awra_form_row_set_help_text (AWRA_FORM_ROW (name_row),
                               "Shown on your public profile");
  awra_form_row_set_required (AWRA_FORM_ROW (name_row), TRUE);
  awra_form_row_set_control (AWRA_FORM_ROW (name_row), name_entry);
  awra_form_row_set_label (AWRA_FORM_ROW (email_row), "Email");
  awra_form_row_set_error_message (AWRA_FORM_ROW (email_row),
                                   "Enter a valid email address");
  awra_form_row_set_control (AWRA_FORM_ROW (email_row), email_entry);
  awra_form_row_set_validation_state (AWRA_FORM_ROW (email_row),
                                      AWRA_VALIDATION_STATE_ERROR);
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (email_row));
  awra_form_insert (AWRA_FORM (form), AWRA_FORM_ROW (name_row), 0);
  g_assert_cmpuint (awra_form_get_n_rows (AWRA_FORM (form)), ==, 2);
  g_assert_true (awra_form_get_row (AWRA_FORM (form), 0) ==
                 AWRA_FORM_ROW (name_row));
  g_assert_cmpint (awra_form_row_get_validation_state (
                     AWRA_FORM_ROW (email_row)), ==,
                   AWRA_VALIDATION_STATE_ERROR);
  g_assert_true (gtk_widget_has_css_class (email_entry,
                                           "awra-validation-error"));

  awra_empty_state_set_title (AWRA_EMPTY_STATE (empty), "No results");
  awra_empty_state_set_description (AWRA_EMPTY_STATE (empty),
                                    "Try changing the filters.");
  awra_empty_state_set_action (AWRA_EMPTY_STATE (empty),
                               awra_button_new_with_label ("Reset filters"));
  awra_loading_state_set_title (AWRA_LOADING_STATE (loading), "Loading");
  awra_loading_state_set_active (AWRA_LOADING_STATE (loading), FALSE);
  awra_error_state_set_title (AWRA_ERROR_STATE (error_state), "Could not load");
  awra_error_state_set_action (AWRA_ERROR_STATE (error_state), retry);
  g_assert_nonnull (awra_empty_state_get_action (AWRA_EMPTY_STATE (empty)));
  g_assert_false (awra_loading_state_get_active (AWRA_LOADING_STATE (loading)));
  g_assert_true (awra_error_state_get_action (AWRA_ERROR_STATE (error_state)) ==
                 retry);

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  awra_window_set_content (AWRA_WINDOW (window), form);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 420);
  gtk_window_present (GTK_WINDOW (window));
  gtk_test_widget_wait_for_draw (window);
  g_assert_false (gtk_widget_has_css_class (name_row, "awra-compact"));
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 420);
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 420);
  gtk_test_widget_wait_for_draw (window);
  if (gtk_widget_get_width (name_row) < 560)
    g_assert_true (gtk_widget_has_css_class (name_row, "awra-compact"));
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_ref_sink (empty);
  g_object_ref_sink (loading);
  g_object_ref_sink (error_state);
  g_object_unref (empty);
  g_object_unref (loading);
  g_object_unref (error_state);
}

static void
test_filter_and_master_detail (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestMasterDetail", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *filter_bar = awra_filter_bar_new ();
  GtkWidget *search = awra_search_entry_new ();
  GtkWidget *filter = awra_dropdown_new_from_strings (
    (const char * const[]) { "All", "Favorites", NULL });
  GtkWidget *second_filter = awra_check_button_new_with_label ("Available");
  GtkWidget *summary = gtk_label_new ("12 results");
  GtkWidget *reset = awra_button_new_with_label ("Reset");
  GtkWidget *master_detail = awra_master_detail_new ();
  GtkWidget *master = awra_list_view_new (NULL, NULL);
  GtkWidget *detail = awra_detail_pane_new ();
  GtkWidget *metadata = awra_badge_new ("ACTIVE");
  GtkWidget *actions = awra_button_new_with_label ("Edit");
  GtkWidget *section = awra_section_new ();

  awra_filter_bar_set_search (AWRA_FILTER_BAR (filter_bar), search);
  awra_filter_bar_append_filter (AWRA_FILTER_BAR (filter_bar), filter);
  awra_filter_bar_insert_filter (AWRA_FILTER_BAR (filter_bar), second_filter, 0);
  awra_filter_bar_set_summary (AWRA_FILTER_BAR (filter_bar), summary);
  awra_filter_bar_set_reset_widget (AWRA_FILTER_BAR (filter_bar), reset);
  g_assert_cmpuint (awra_filter_bar_get_n_filters (
                      AWRA_FILTER_BAR (filter_bar)), ==, 2);
  g_assert_true (awra_filter_bar_get_filter (
                   AWRA_FILTER_BAR (filter_bar), 0) == second_filter);
  awra_filter_bar_remove_filter (AWRA_FILTER_BAR (filter_bar), second_filter);
  g_assert_cmpuint (awra_filter_bar_get_n_filters (
                      AWRA_FILTER_BAR (filter_bar)), ==, 1);

  awra_detail_pane_set_title (AWRA_DETAIL_PANE (detail), "Bulbasaur");
  awra_detail_pane_set_subtitle (AWRA_DETAIL_PANE (detail), "Sample item");
  awra_detail_pane_set_media (AWRA_DETAIL_PANE (detail),
                              gtk_image_new_from_icon_name (
                                "image-x-generic-symbolic"));
  awra_detail_pane_set_metadata (AWRA_DETAIL_PANE (detail), metadata);
  awra_detail_pane_set_actions (AWRA_DETAIL_PANE (detail), actions);
  awra_section_set_title (AWRA_SECTION (section), "Overview");
  awra_section_set_child (AWRA_SECTION (section),
                          gtk_label_new ("A reusable entity section"));
  awra_detail_pane_append_section (AWRA_DETAIL_PANE (detail),
                                   AWRA_SECTION (section));
  g_assert_true (awra_detail_pane_get_section (
                   AWRA_DETAIL_PANE (detail), 0) == AWRA_SECTION (section));
  awra_master_detail_set_master (AWRA_MASTER_DETAIL (master_detail), master);
  awra_master_detail_set_detail (AWRA_MASTER_DETAIL (master_detail), detail);
  awra_master_detail_set_master_width (AWRA_MASTER_DETAIL (master_detail), 300);

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  awra_window_set_content (AWRA_WINDOW (window), master_detail);
  gtk_window_set_default_size (GTK_WINDOW (window), 1120, 620);
  gtk_widget_set_size_request (window, 1120, 620);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);
  g_assert_cmpint (awra_master_detail_get_layout_mode (
                     AWRA_MASTER_DETAIL (master_detail)), ==,
                   AWRA_LAYOUT_MODE_EXPANDED);
  g_assert_true (gtk_widget_get_visible (master));
  g_assert_true (gtk_widget_get_visible (detail));
  awra_master_detail_show_detail (AWRA_MASTER_DETAIL (master_detail));
  g_assert_cmpint (awra_master_detail_get_visible_pane (
                     AWRA_MASTER_DETAIL (master_detail)), ==,
                   AWRA_MASTER_DETAIL_PANE_DETAIL);
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_ref_sink (filter_bar);
  g_object_unref (filter_bar);
}

static void
test_data_view_and_selection_toolbar (void)
{
  static const char *items[] = { "Alpha", "Beta", "Gamma", NULL };
  g_autoptr (GtkStringList) strings = gtk_string_list_new (items);
  g_autoptr (GtkMultiSelection) selection = gtk_multi_selection_new (
    G_LIST_MODEL (g_steal_pointer (&strings)));
  g_autoptr (GtkListItemFactory) factory = gtk_signal_list_item_factory_new ();
  g_autoptr (AwraDataColumn) name_column = awra_data_column_new ("Name", factory);
  g_autoptr (AwraDataColumn) metric_column = awra_data_column_new ("Metric", NULL);
  g_autoptr (GtkWidget) data_view = g_object_ref_sink (
    awra_data_view_new (GTK_SELECTION_MODEL (selection)));
  g_autoptr (GtkWidget) toolbar = g_object_ref_sink (
    awra_selection_toolbar_new (GTK_SELECTION_MODEL (selection)));
  GtkWidget *actions = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *custom_empty = awra_empty_state_new ();
  GtkWidget *cell = gtk_label_new ("42");

  awra_data_column_set_expand (name_column, TRUE);
  awra_data_column_set_resizable (name_column, TRUE);
  awra_data_column_set_fixed_width (metric_column, 96);
  awra_data_column_set_alignment (metric_column, GTK_ALIGN_END);
  awra_data_column_apply_alignment (metric_column, cell);
  g_assert_cmpfloat (gtk_label_get_xalign (GTK_LABEL (cell)), ==, 1.0f);
  awra_data_view_append_column (AWRA_DATA_VIEW (data_view), name_column);
  awra_data_view_insert_column (AWRA_DATA_VIEW (data_view), metric_column, 0);
  g_assert_cmpuint (awra_data_view_get_n_columns (
                      AWRA_DATA_VIEW (data_view)), ==, 2);
  g_assert_true (awra_data_view_get_column (AWRA_DATA_VIEW (data_view), 0) ==
                 metric_column);
  g_assert_true (awra_data_view_get_model (AWRA_DATA_VIEW (data_view)) ==
                 GTK_SELECTION_MODEL (selection));
  awra_data_view_set_empty_widget (AWRA_DATA_VIEW (data_view), custom_empty);
  awra_data_view_set_state (AWRA_DATA_VIEW (data_view),
                            AWRA_DATA_VIEW_STATE_EMPTY);
  awra_data_view_set_loading_more (AWRA_DATA_VIEW (data_view), TRUE);
  g_assert_true (awra_data_view_get_loading_more (AWRA_DATA_VIEW (data_view)));
  g_assert_true (awra_data_view_get_empty_widget (AWRA_DATA_VIEW (data_view)) ==
                 custom_empty);
  awra_data_view_remove_column (AWRA_DATA_VIEW (data_view), metric_column);
  g_assert_cmpuint (awra_data_view_get_n_columns (
                      AWRA_DATA_VIEW (data_view)), ==, 1);

  awra_selection_toolbar_set_title (AWRA_SELECTION_TOOLBAR (toolbar), "record");
  awra_selection_toolbar_set_plural_title (AWRA_SELECTION_TOOLBAR (toolbar),
                                           "records");
  g_assert_cmpstr (awra_selection_toolbar_get_plural_title (
                     AWRA_SELECTION_TOOLBAR (toolbar)), ==, "records");
  gtk_box_append (GTK_BOX (actions), awra_button_new_with_label ("Export"));
  awra_selection_toolbar_set_actions (AWRA_SELECTION_TOOLBAR (toolbar), actions);
  gtk_selection_model_select_item (GTK_SELECTION_MODEL (selection), 0, FALSE);
  gtk_selection_model_select_item (GTK_SELECTION_MODEL (selection), 2, FALSE);
  g_assert_cmpuint (awra_selection_toolbar_get_selected_count (
                      AWRA_SELECTION_TOOLBAR (toolbar)), ==, 2);
  g_assert_true (gtk_widget_get_visible (toolbar));
  awra_selection_toolbar_clear_selection (AWRA_SELECTION_TOOLBAR (toolbar));
  g_assert_cmpuint (awra_selection_toolbar_get_selected_count (
                      AWRA_SELECTION_TOOLBAR (toolbar)), ==, 0);
  g_assert_false (gtk_widget_get_visible (toolbar));
}

static void
test_advanced_inputs_and_status (void)
{
  g_autoptr (GtkWidget) text_area = g_object_ref_sink (awra_text_area_new ());
  g_autoptr (GtkWidget) password = g_object_ref_sink (
    awra_password_entry_new ());
  g_autoptr (GtkWidget) tags = g_object_ref_sink (awra_tag_entry_new ());
  g_autoptr (GtkWidget) choices = g_object_ref_sink (awra_choice_group_new ());
  g_autoptr (GtkWidget) banner = g_object_ref_sink (awra_status_banner_new ());
  AwraCheckButton *daily;
  AwraCheckButton *weekly;
  GtkWidget *action = awra_button_new_with_label ("Retry");

  awra_text_area_set_placeholder_text (AWRA_TEXT_AREA (text_area), "Notes");
  awra_text_area_set_text (AWRA_TEXT_AREA (text_area), "Multiline\ntext");
  awra_text_area_set_monospace (AWRA_TEXT_AREA (text_area), TRUE);
  g_assert_cmpstr (awra_text_area_get_text (AWRA_TEXT_AREA (text_area)), ==,
                   "Multiline\ntext");
  g_assert_true (awra_text_area_get_monospace (AWRA_TEXT_AREA (text_area)));
  g_assert_true (GTK_IS_TEXT_BUFFER (awra_text_area_get_buffer (
                                      AWRA_TEXT_AREA (text_area))));

  awra_password_entry_set_placeholder_text (AWRA_PASSWORD_ENTRY (password),
                                             "Secret");
  awra_password_entry_set_text (AWRA_PASSWORD_ENTRY (password), "hunter2");
  awra_password_entry_set_show_peek_icon (AWRA_PASSWORD_ENTRY (password), FALSE);
  awra_password_entry_set_activates_default (AWRA_PASSWORD_ENTRY (password), TRUE);
  g_assert_cmpstr (awra_password_entry_get_text (
                     AWRA_PASSWORD_ENTRY (password)), ==, "hunter2");
  g_assert_true (awra_password_entry_get_activates_default (
                   AWRA_PASSWORD_ENTRY (password)));

  g_assert_true (awra_tag_entry_add (AWRA_TAG_ENTRY (tags), "Design"));
  g_assert_true (awra_tag_entry_add (AWRA_TAG_ENTRY (tags), "GTK"));
  g_assert_false (awra_tag_entry_add (AWRA_TAG_ENTRY (tags), "GTK"));
  g_assert_cmpuint (awra_tag_entry_get_n_tags (AWRA_TAG_ENTRY (tags)), ==, 2);
  g_assert_true (awra_tag_entry_remove (AWRA_TAG_ENTRY (tags), "Design"));
  g_assert_cmpstr (awra_tag_entry_get_tag (AWRA_TAG_ENTRY (tags), 0), ==, "GTK");

  awra_choice_group_set_title (AWRA_CHOICE_GROUP (choices), "Digest");
  awra_choice_group_set_exclusive (AWRA_CHOICE_GROUP (choices), TRUE);
  daily = awra_choice_group_append (AWRA_CHOICE_GROUP (choices), "Daily");
  weekly = awra_choice_group_append (AWRA_CHOICE_GROUP (choices), "Weekly");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (daily), TRUE);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (weekly), TRUE);
  g_assert_false (gtk_check_button_get_active (GTK_CHECK_BUTTON (daily)));
  g_assert_true (gtk_check_button_get_active (GTK_CHECK_BUTTON (weekly)));

  awra_status_banner_set_title (AWRA_STATUS_BANNER (banner), "Offline");
  awra_status_banner_set_description (AWRA_STATUS_BANNER (banner),
                                      "Changes remain local.");
  awra_status_banner_set_appearance (AWRA_STATUS_BANNER (banner),
                                     AWRA_STATUS_APPEARANCE_WARNING);
  awra_status_banner_set_action (AWRA_STATUS_BANNER (banner), action);
  awra_status_banner_set_dismissible (AWRA_STATUS_BANNER (banner), TRUE);
  g_assert_true (awra_status_banner_get_action (AWRA_STATUS_BANNER (banner)) ==
                 action);
  awra_status_banner_set_revealed (AWRA_STATUS_BANNER (banner), FALSE);
  g_assert_false (awra_status_banner_get_revealed (
                    AWRA_STATUS_BANNER (banner)));
}

static void
test_edge_panel (void)
{
  g_autoptr (GtkWidget) edge = g_object_ref_sink (awra_edge_panel_new ());
  GtkWidget *content = gtk_label_new ("Workspace");
  GtkWidget *panel = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  awra_edge_panel_set_content (AWRA_EDGE_PANEL (edge), content);
  awra_edge_panel_set_panel (AWRA_EDGE_PANEL (edge), panel);
  awra_edge_panel_set_edge (AWRA_EDGE_PANEL (edge), AWRA_PANEL_EDGE_START);
  awra_edge_panel_set_auto_hide (AWRA_EDGE_PANEL (edge), TRUE);
  awra_edge_panel_set_revealed (AWRA_EDGE_PANEL (edge), FALSE);
  awra_edge_panel_set_panel_width (AWRA_EDGE_PANEL (edge), 344);
  awra_edge_panel_set_reveal_width (AWRA_EDGE_PANEL (edge), 18);
  awra_edge_panel_set_hide_delay (AWRA_EDGE_PANEL (edge), 450);

  g_assert_true (awra_edge_panel_get_content (AWRA_EDGE_PANEL (edge)) ==
                 content);
  g_assert_true (awra_edge_panel_get_panel (AWRA_EDGE_PANEL (edge)) == panel);
  g_assert_cmpint (awra_edge_panel_get_edge (AWRA_EDGE_PANEL (edge)), ==,
                   AWRA_PANEL_EDGE_START);
  g_assert_true (awra_edge_panel_get_auto_hide (AWRA_EDGE_PANEL (edge)));
  g_assert_false (awra_edge_panel_get_revealed (AWRA_EDGE_PANEL (edge)));
  g_assert_cmpuint (awra_edge_panel_get_panel_width (
                      AWRA_EDGE_PANEL (edge)), ==, 344);
  g_assert_cmpuint (awra_edge_panel_get_reveal_width (
                      AWRA_EDGE_PANEL (edge)), ==, 18);
  g_assert_cmpuint (awra_edge_panel_get_hide_delay (
                      AWRA_EDGE_PANEL (edge)), ==, 450);
  g_assert_cmpint (awra_surface_get_role (awra_edge_panel_get_surface (
                     AWRA_EDGE_PANEL (edge))), ==,
                   AWRA_SURFACE_ROLE_FLOATING);
  g_assert_cmpint (awra_surface_get_elevation_level (
                     awra_edge_panel_get_surface (AWRA_EDGE_PANEL (edge))), ==,
                   AWRA_ELEVATION_FLOATING);
  g_assert_cmpint (awra_material_get_preset (awra_surface_get_material (
                     awra_edge_panel_get_surface (AWRA_EDGE_PANEL (edge)))), ==,
                   AWRA_MATERIAL_PRESET_FLOATING);
}

static void
test_mapped_inset_sidebar (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestMappedSidebar", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *split;
  GtkWidget *sidebar;

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  split = awra_split_view_new ();
  sidebar = awra_sidebar_new ();
  awra_sidebar_set_inset (AWRA_SIDEBAR (sidebar), 12);
  awra_sidebar_set_child (AWRA_SIDEBAR (sidebar),
                          gtk_label_new ("Application Patterns"));
  awra_split_view_set_sidebar (AWRA_SPLIT_VIEW (split), sidebar);
  awra_split_view_set_content (AWRA_SPLIT_VIEW (split),
                               gtk_label_new ("Overview"));
  awra_window_set_content (AWRA_WINDOW (window), split);
  gtk_window_set_default_size (GTK_WINDOW (window), 1180, 780);
  gtk_widget_set_size_request (window, 1180, 780);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);

  g_assert_cmpint (gtk_widget_get_width (split), >=, 900);
  g_assert_cmpint (awra_split_view_get_layout_mode (AWRA_SPLIT_VIEW (split)),
                   ==, AWRA_LAYOUT_MODE_EXPANDED);
  g_assert_true (gtk_widget_get_visible (sidebar));
  g_assert_true (gtk_widget_get_mapped (sidebar));
  g_assert_cmpint (gtk_widget_get_width (sidebar), >=, 254);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_responsive_bin (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestResponsiveBin", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *responsive;
  GtkWidget *child;

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  responsive = awra_responsive_bin_new ();
  child = gtk_label_new ("Adaptive workspace");
  awra_responsive_bin_set_child (AWRA_RESPONSIVE_BIN (responsive), child);
  awra_responsive_bin_set_compact_width (AWRA_RESPONSIVE_BIN (responsive),
                                         600);
  awra_responsive_bin_set_expanded_width (AWRA_RESPONSIVE_BIN (responsive),
                                          900);
  awra_window_set_content (AWRA_WINDOW (window), responsive);
  gtk_window_set_default_size (GTK_WINDOW (window), 1100, 600);
  gtk_widget_set_size_request (window, 1100, 600);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);

  g_assert_cmpint (awra_responsive_bin_get_layout_mode (
                     AWRA_RESPONSIVE_BIN (responsive)), ==,
                   AWRA_LAYOUT_MODE_EXPANDED);
  g_assert_cmpint (gtk_widget_get_width (child), ==,
                   gtk_widget_get_width (responsive));

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
breadcrumb_activated_cb (AwraBreadcrumbBar  *bar,
                         AwraBreadcrumbItem *item,
                         guint              *count)
{
  (void) bar;
  g_assert_cmpstr (awra_breadcrumb_item_get_name (item), ==, "detail");
  (*count)++;
}

static void
test_breadcrumb_bar (void)
{
  g_autoptr (GtkWidget) bar = g_object_ref_sink (awra_breadcrumb_bar_new ());
  g_autoptr (GListStore) model = g_list_store_new (AWRA_TYPE_BREADCRUMB_ITEM);
  g_autoptr (AwraBreadcrumbItem) home = awra_breadcrumb_item_new (
    "Library", "library");
  g_autoptr (AwraBreadcrumbItem) detail = awra_breadcrumb_item_new (
    "Bulbasaur", "detail");
  guint activated = 0;

  g_list_store_append (model, home);
  g_list_store_append (model, detail);
  awra_breadcrumb_bar_set_model (AWRA_BREADCRUMB_BAR (bar),
                                 G_LIST_MODEL (model));
  g_assert_true (awra_breadcrumb_bar_get_model (AWRA_BREADCRUMB_BAR (bar)) ==
                 G_LIST_MODEL (model));
  g_signal_connect (bar, "activate-item",
                    G_CALLBACK (breadcrumb_activated_cb), &activated);
  g_signal_emit_by_name (bar, "activate-item", detail);
  g_assert_cmpuint (activated, ==, 1);
  awra_breadcrumb_item_set_label (detail, "Ivysaur");
  g_assert_cmpstr (awra_breadcrumb_item_get_label (detail), ==, "Ivysaur");
  awra_breadcrumb_bar_set_model (AWRA_BREADCRUMB_BAR (bar), NULL);
}

static void
test_data_components (void)
{
  g_autoptr (GtkWidget) metric = g_object_ref_sink (
    awra_metric_row_new ("Height", "0.7 m"));
  g_autoptr (GtkWidget) stat = g_object_ref_sink (awra_stat_bar_new ("HP"));
  g_autoptr (GtkWidget) badges = g_object_ref_sink (awra_badge_group_new ());
  g_autoptr (GtkWidget) metadata = g_object_ref_sink (
    awra_metadata_group_new ());
  AwraBadge *grass = AWRA_BADGE (awra_badge_new ("GRASS"));
  AwraMetricRow *weight;

  awra_metric_row_set_icon_name (AWRA_METRIC_ROW (metric),
                                 "document-properties-symbolic");
  g_assert_cmpstr (awra_metric_row_get_value (AWRA_METRIC_ROW (metric)), ==,
                   "0.7 m");
  awra_stat_bar_set_maximum (AWRA_STAT_BAR (stat), 255.0);
  awra_stat_bar_set_value (AWRA_STAT_BAR (stat), 45.0);
  g_assert_cmpfloat (awra_stat_bar_get_value (AWRA_STAT_BAR (stat)), ==, 45.0);
  awra_badge_group_append (AWRA_BADGE_GROUP (badges), grass);
  g_assert_true (awra_badge_group_get_badge (AWRA_BADGE_GROUP (badges), 0) ==
                 grass);
  awra_metadata_group_set_title (AWRA_METADATA_GROUP (metadata), "Profile");
  weight = awra_metadata_group_append (AWRA_METADATA_GROUP (metadata),
                                       "Weight", "6.9 kg");
  g_assert_true (awra_metadata_group_get_row (
                   AWRA_METADATA_GROUP (metadata), 0) == weight);
}

static void
test_mapped_switch_geometry (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestSwitchGeometry", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *plain_row;
  GtkWidget *plain_switch;
  GtkWidget *action_row;
  GtkWidget *suffix_switch;
  GtkWidget *grid;
  GtkWidget *grid_switch;
  GtkWidget *tall_button;
  int plain_height;
  int suffix_height;
  int grid_height;

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  plain_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  plain_switch = awra_switch_new ();
  gtk_box_append (GTK_BOX (plain_row), gtk_label_new ("Live preview"));
  gtk_box_append (GTK_BOX (plain_row), plain_switch);
  gtk_box_append (GTK_BOX (box), plain_row);

  action_row = awra_action_row_new ("Keep panel visible",
                                    "Disable to reveal it from the edge");
  suffix_switch = awra_switch_new ();
  awra_action_row_set_suffix (AWRA_ACTION_ROW (action_row), suffix_switch);
  gtk_box_append (GTK_BOX (box), action_row);

  grid = gtk_grid_new ();
  tall_button = awra_button_new_with_label ("Selected");
  gtk_widget_set_size_request (tall_button, -1, 42);
  grid_switch = awra_switch_new ();
  gtk_grid_attach (GTK_GRID (grid), tall_button, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), grid_switch, 1, 0, 1, 1);
  gtk_box_append (GTK_BOX (box), grid);

  awra_window_set_content (AWRA_WINDOW (window), box);
  gtk_window_set_default_size (GTK_WINDOW (window), 520, 260);
  gtk_window_present (GTK_WINDOW (window));
  gtk_test_widget_wait_for_draw (window);

  plain_height = gtk_widget_get_height (GTK_WIDGET (
    awra_switch_get_delegate (AWRA_SWITCH (plain_switch))));
  suffix_height = gtk_widget_get_height (GTK_WIDGET (
    awra_switch_get_delegate (AWRA_SWITCH (suffix_switch))));
  grid_height = gtk_widget_get_height (GTK_WIDGET (
    awra_switch_get_delegate (AWRA_SWITCH (grid_switch))));
  g_assert_cmpint (plain_height, <=, 30);
  g_assert_cmpint (suffix_height, ==, plain_height);
  g_assert_cmpint (grid_height, ==, plain_height);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_action_row_and_context_menu (void)
{
  g_autoptr (GtkWidget) target = g_object_ref_sink (
    gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget *row = awra_action_row_new ("Wi-Fi", "Connected");
  GtkWidget *suffix = awra_badge_new ("On");
  g_autoptr (GMenu) menu = g_menu_new ();

  awra_action_row_set_icon_name (AWRA_ACTION_ROW (row),
                                 "network-wireless-symbolic");
  awra_action_row_set_suffix (AWRA_ACTION_ROW (row), suffix);
  gtk_box_append (GTK_BOX (target), row);
  g_assert_cmpstr (awra_action_row_get_title (AWRA_ACTION_ROW (row)), ==,
                   "Wi-Fi");
  g_assert_cmpstr (awra_action_row_get_subtitle (AWRA_ACTION_ROW (row)), ==,
                   "Connected");
  g_assert_cmpstr (awra_action_row_get_icon_name (AWRA_ACTION_ROW (row)), ==,
                   "network-wireless-symbolic");
  g_assert_true (awra_action_row_get_suffix (AWRA_ACTION_ROW (row)) == suffix);
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (row)), ==,
                   GTK_ACCESSIBLE_ROLE_BUTTON);

  g_menu_append (menu, "Inspect", "app.inspect");
  awra_widget_set_context_menu_model (row, G_MENU_MODEL (menu));
  g_assert_true (awra_widget_get_context_menu_model (row) ==
                 G_MENU_MODEL (menu));
  g_assert_true (AWRA_IS_POPOVER (
    awra_widget_get_context_menu_popover (row)));
  awra_widget_set_context_menu_model (row, NULL);
  g_assert_null (awra_widget_get_context_menu_model (row));
  g_assert_null (awra_widget_get_context_menu_popover (row));
}

/* Rows of a form share one label column, compact controls keep their natural
 * width and text fields fill the field column. */
static void
test_form_alignment_and_control_width (void)
{
  g_autoptr (GtkWidget) form = g_object_ref_sink (awra_form_new ());
  AwraFormRow *short_row = AWRA_FORM_ROW (awra_form_row_new ());
  AwraFormRow *long_row = AWRA_FORM_ROW (awra_form_row_new ());
  GtkWidget *spin = awra_spin_button_new_with_range (1024.0, 65535.0, 1.0);
  GtkWidget *entry = awra_entry_new ();
  GtkWidget *short_label;
  GtkWidget *long_label;
  int short_width = 0;
  int long_width = 0;

  awra_form_row_set_label (short_row, "Port");
  awra_form_row_set_label (long_row,
                           "A considerably longer label that needs more room");
  awra_form_row_set_control (short_row, spin);
  awra_form_row_set_control (long_row, entry);
  awra_form_append (AWRA_FORM (form), short_row);
  awra_form_append (AWRA_FORM (form), long_row);

  short_label = gtk_widget_get_first_child (
    gtk_widget_get_first_child (GTK_WIDGET (short_row)));
  long_label = gtk_widget_get_first_child (
    gtk_widget_get_first_child (GTK_WIDGET (long_row)));
  gtk_widget_measure (short_label, GTK_ORIENTATION_HORIZONTAL, -1,
                      NULL, &short_width, NULL, NULL);
  gtk_widget_measure (long_label, GTK_ORIENTATION_HORIZONTAL, -1,
                      NULL, &long_width, NULL, NULL);
  g_assert_cmpint (short_width, ==, long_width);

  g_assert_false (gtk_widget_get_hexpand (spin));
  g_assert_cmpint (gtk_widget_get_halign (spin), ==, GTK_ALIGN_START);
  g_assert_true (gtk_widget_get_hexpand (entry));
  g_assert_cmpint (gtk_editable_get_width_chars (GTK_EDITABLE (
                     awra_spin_button_get_delegate (AWRA_SPIN_BUTTON (spin)))),
                   >=, 5);
}

/* A row not allocated yet is still side by side: it is measured as the width
 * it gets will lay it out, never wider than stacked, so a new row does not
 * widen a narrow column nor get a side-by-side height there for a frame. */
static void
test_form_row_measured_for_its_layout (void)
{
  g_autoptr (GtkWidget) row = g_object_ref_sink (awra_form_row_new ());
  GtkWidget *entry = awra_entry_new ();
  GtkWidget *label;
  int row_min = 0, label_min = 0, field_min = 0;
  int row_height = 0, label_height = 0, field_height = 0;

  awra_form_row_set_label (AWRA_FORM_ROW (row), "Lecture");
  awra_form_row_set_control (AWRA_FORM_ROW (row), entry);
  label = gtk_widget_get_first_child (gtk_widget_get_first_child (row));
  g_assert_false (gtk_widget_has_css_class (row, "awra-compact"));

  gtk_widget_measure (row, GTK_ORIENTATION_HORIZONTAL, -1,
                      &row_min, NULL, NULL, NULL);
  gtk_widget_measure (label, GTK_ORIENTATION_HORIZONTAL, -1,
                      &label_min, NULL, NULL, NULL);
  gtk_widget_measure (entry, GTK_ORIENTATION_HORIZONTAL, -1,
                      &field_min, NULL, NULL, NULL);
  g_assert_cmpint (row_min, <, label_min + field_min);

  g_assert_cmpint (gtk_widget_get_request_mode (row), ==,
                   GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH);
  gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, 320,
                      &row_height, NULL, NULL, NULL);
  gtk_widget_measure (label, GTK_ORIENTATION_VERTICAL, -1,
                      &label_height, NULL, NULL, NULL);
  gtk_widget_measure (entry, GTK_ORIENTATION_VERTICAL, 320,
                      &field_height, NULL, NULL, NULL);
  g_assert_cmpint (row_height, >=, label_height + field_height);
}

/* A divider the user dragged stays put while the mode does not change. */
static void
test_split_view_keeps_dragged_divider (void)
{
  g_autoptr (GtkWidget) split = g_object_ref_sink (awra_split_view_new ());
  GtkPaned *paned;

  awra_split_view_set_sidebar (AWRA_SPLIT_VIEW (split), gtk_label_new ("Sidebar"));
  awra_split_view_set_content (AWRA_SPLIT_VIEW (split), gtk_label_new ("Content"));
  paned = GTK_PANED (gtk_widget_get_first_child (split));

  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 1200);
  g_assert_cmpint (gtk_paned_get_position (paned), ==, 244);
  gtk_paned_set_position (paned, 400);
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 1300);
  g_assert_cmpint (gtk_paned_get_position (paned), ==, 400);
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 800);
  g_assert_cmpint (gtk_paned_get_position (paned), ==, 220);
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 1300);
  g_assert_cmpint (gtk_paned_get_position (paned), ==, 244);
}

/* The two widths of a responsive container can be set in either order. */
static void
test_responsive_widths_in_any_order (void)
{
  g_autoptr (GtkWidget) bin = g_object_ref_sink (awra_responsive_bin_new ());
  g_autoptr (GtkWidget) split = g_object_ref_sink (awra_split_view_new ());

  g_object_set (bin, "compact-width", 1000, "expanded-width", 800, NULL);
  g_assert_cmpuint (awra_responsive_bin_get_compact_width (AWRA_RESPONSIVE_BIN (bin)), ==, 1000);
  g_assert_cmpuint (awra_responsive_bin_get_expanded_width (AWRA_RESPONSIVE_BIN (bin)), ==, 800);
  g_object_set (split, "compact-width", 1000, "expanded-width", 700, NULL);
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 900);
  g_assert_cmpint (awra_split_view_get_layout_mode (AWRA_SPLIT_VIEW (split)), ==,
                   AWRA_LAYOUT_MODE_COMPACT);
  awra_split_view_update_for_width (AWRA_SPLIT_VIEW (split), 1100);
  g_assert_cmpint (awra_split_view_get_layout_mode (AWRA_SPLIT_VIEW (split)), ==,
                   AWRA_LAYOUT_MODE_EXPANDED);
}

static GtkWidget *
sized_box (int width,
           int height)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_widget_set_size_request (box, width, height);
  return box;
}

/* A master-detail is measured as the width it gets lays it out: one pane
 * alone when narrow, both side by side when wide; the hidden pane keeps
 * its own visibility. */
static void
test_master_detail_measured_for_its_layout (void)
{
  g_autoptr (GtkWidget) view = g_object_ref_sink (awra_master_detail_new ());
  GtkWidget *master = sized_box (200, 100);
  GtkWidget *detail = sized_box (450, 150);
  int minimum = 0, natural = 0;

  awra_master_detail_set_master (AWRA_MASTER_DETAIL (view), master);
  awra_master_detail_set_detail (AWRA_MASTER_DETAIL (view), detail);

  gtk_widget_measure (view, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, &natural, NULL, NULL);
  g_assert_cmpint (minimum, ==, 200);
  g_assert_cmpint (natural, >=, 320 + 450);
  g_assert_cmpint (gtk_widget_get_request_mode (view), ==, GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH);
  gtk_widget_measure (view, GTK_ORIENTATION_VERTICAL, 500, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 100);
  gtk_widget_measure (view, GTK_ORIENTATION_VERTICAL, 1200, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 150);
  /* Wider than compact, but too narrow for both panes: still one. */
  gtk_widget_measure (view, GTK_ORIENTATION_VERTICAL, 700, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 100);

  awra_master_detail_show_detail (AWRA_MASTER_DETAIL (view));
  gtk_widget_measure (view, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 450);
  gtk_widget_measure (view, GTK_ORIENTATION_VERTICAL, 500, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 150);
  g_assert_true (gtk_widget_get_visible (master));
}

/* A new filter bar, still side by side, is measured as the width it gets
 * will lay it out. */
static void
test_filter_bar_measured_for_its_layout (void)
{
  g_autoptr (GtkWidget) bar = g_object_ref_sink (awra_filter_bar_new ());
  int stacked = 0, side = 0, minimum = 0;

  awra_filter_bar_set_search (AWRA_FILTER_BAR (bar), sized_box (300, 40));
  awra_filter_bar_append_filter (AWRA_FILTER_BAR (bar), sized_box (200, 40));
  awra_filter_bar_set_summary (AWRA_FILTER_BAR (bar), sized_box (100, 20));
  awra_filter_bar_set_reset_widget (AWRA_FILTER_BAR (bar), sized_box (80, 30));
  g_assert_false (awra_filter_bar_get_compact (AWRA_FILTER_BAR (bar)));

  gtk_widget_measure (bar, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, <, 300 + 200 + 100 + 80);
  gtk_widget_measure (bar, GTK_ORIENTATION_VERTICAL, 400, &stacked, NULL, NULL, NULL);
  gtk_widget_measure (bar, GTK_ORIENTATION_VERTICAL, 1200, &side, NULL, NULL, NULL);
  g_assert_cmpint (stacked, >=, 40 + 40 + 30);
  g_assert_cmpint (side, <, stacked);
}

/* Pushing slides in from the end, popping goes back from the start,
 * whatever order the pages were added in. */
static void
test_navigation_direction (void)
{
  g_autoptr (GtkWidget) view = g_object_ref_sink (awra_navigation_view_new ());
  GtkStack *stack = awra_navigation_view_get_stack (AWRA_NAVIGATION_VIEW (view));

  awra_navigation_view_add (AWRA_NAVIGATION_VIEW (view), gtk_label_new ("Detail"), "detail");
  awra_navigation_view_add (AWRA_NAVIGATION_VIEW (view), gtk_label_new ("Home"), "home");
  gtk_stack_set_transition_duration (stack, 200);
  gtk_widget_set_direction (view, GTK_TEXT_DIR_LTR);

  g_assert_true (awra_navigation_view_push (AWRA_NAVIGATION_VIEW (view), "home"));
  g_assert_true (awra_navigation_view_push (AWRA_NAVIGATION_VIEW (view), "detail"));
  g_assert_cmpint (gtk_stack_get_transition_type (stack), ==,
                   GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
  g_assert_true (awra_navigation_view_pop (AWRA_NAVIGATION_VIEW (view)));
  g_assert_cmpint (gtk_stack_get_transition_type (stack), ==,
                   GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
  g_assert_cmpstr (awra_navigation_view_get_visible_page_name (AWRA_NAVIGATION_VIEW (view)),
                   ==, "home");
}

typedef struct {
  guint count;
  char *last;
} ResponseLog;

static void
dialog_response_cb (AwraDialog  *dialog,
                    const char  *response,
                    ResponseLog *log)
{
  (void) dialog;
  log->count++;
  g_free (log->last);
  log->last = g_strdup (response);
}

/* Responses: buttons below the body, a default one, and the one closing
 * the dialog gives. */
static void
test_dialog_responses (void)
{
  ResponseLog log = { 0 };
  ResponseLog saved = { 0 };
  GtkWidget *dialog = awra_dialog_new (NULL);
  GtkWidget *body = gtk_label_new ("Delete the profile?");

  g_object_ref_sink (dialog);
  awra_dialog_set_body (AWRA_DIALOG (dialog), body);
  g_assert_true (awra_dialog_get_body (AWRA_DIALOG (dialog)) == body);
  g_assert_true (gtk_widget_is_ancestor (body, dialog));
  awra_dialog_add_response (AWRA_DIALOG (dialog), "cancel", "Cancel");
  awra_dialog_add_response (AWRA_DIALOG (dialog), "delete", "Delete");
  awra_dialog_set_response_appearance (AWRA_DIALOG (dialog), "delete",
                                       AWRA_BUTTON_APPEARANCE_DESTRUCTIVE);
  awra_dialog_set_default_response (AWRA_DIALOG (dialog), "delete");
  awra_dialog_set_close_response (AWRA_DIALOG (dialog), "cancel");
  g_assert_cmpstr (awra_dialog_get_default_response (AWRA_DIALOG (dialog)), ==, "delete");
  g_assert_cmpstr (awra_dialog_get_close_response (AWRA_DIALOG (dialog)), ==, "cancel");
  g_assert_nonnull (gtk_window_get_default_widget (GTK_WINDOW (dialog)));
  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), &log);
  g_signal_connect (dialog, "response::delete", G_CALLBACK (dialog_response_cb), &saved);

  /* Closing answers the close response (GTK only closes realized windows). */
  gtk_widget_realize (dialog);
  gtk_window_close (GTK_WINDOW (dialog));
  g_assert_cmpuint (log.count, ==, 1);
  g_assert_cmpstr (log.last, ==, "cancel");
  g_assert_cmpuint (saved.count, ==, 0);
  g_object_unref (dialog);

  dialog = g_object_ref_sink (awra_dialog_new (NULL));
  awra_dialog_add_response (AWRA_DIALOG (dialog), "delete", "Delete");
  awra_dialog_set_close_response (AWRA_DIALOG (dialog), "cancel");
  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), &log);
  g_signal_connect (dialog, "response::delete", G_CALLBACK (dialog_response_cb), &saved);
  awra_dialog_respond (AWRA_DIALOG (dialog), "delete");
  /* Once, not also as a close response. */
  g_assert_cmpuint (log.count, ==, 2);
  g_assert_cmpstr (log.last, ==, "delete");
  g_assert_cmpuint (saved.count, ==, 1);
  g_object_unref (dialog);
  g_free (log.last);
  g_free (saved.last);
}

/* A response that stays: it answers, the dialog stays open and shows the
 * work under way, and closing it afterwards gives no other response. */
static void
test_dialog_response_stays (void)
{
  ResponseLog log = { 0 };
  GtkWidget *dialog = g_object_ref_sink (awra_dialog_new (NULL));
  GtkWidget *button;

  awra_dialog_add_response (AWRA_DIALOG (dialog), "cancel", "Cancel");
  awra_dialog_add_response (AWRA_DIALOG (dialog), "join", "Join");
  awra_dialog_set_close_response (AWRA_DIALOG (dialog), "cancel");
  g_assert_true (awra_dialog_get_response_closes (AWRA_DIALOG (dialog), "join"));
  awra_dialog_set_response_closes (AWRA_DIALOG (dialog), "join", FALSE);
  g_assert_false (awra_dialog_get_response_closes (AWRA_DIALOG (dialog), "join"));
  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), &log);
  gtk_window_present (GTK_WINDOW (dialog));

  awra_dialog_respond (AWRA_DIALOG (dialog), "join");
  g_assert_cmpuint (log.count, ==, 1);
  g_assert_cmpstr (log.last, ==, "join");
  g_assert_true (gtk_widget_get_visible (dialog));

  awra_dialog_set_default_response (AWRA_DIALOG (dialog), "join");
  button = gtk_window_get_default_widget (GTK_WINDOW (dialog));
  awra_dialog_set_response_busy (AWRA_DIALOG (dialog), "join", TRUE);
  g_assert_true (awra_button_get_busy (AWRA_BUTTON (button)));
  awra_dialog_close (AWRA_DIALOG (dialog));
  g_assert_cmpuint (log.count, ==, 1);
  g_assert_false (gtk_widget_get_visible (dialog));
  g_object_unref (dialog);
  g_free (log.last);
}

static void
count_clicks_cb (GtkButton *button,
                 guint     *clicks)
{
  (void) button;
  (*clicks)++;
}

static GtkWidget *
find_css_name (GtkWidget  *widget,
               const char *name)
{
  if (g_str_equal (gtk_widget_get_css_name (widget), name))
    return widget;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    GtkWidget *found = find_css_name (child, name);

    if (found != NULL)
      return found;
  }
  return NULL;
}

/* A page's content is measured for the width it gets, so wrapped text
 * gets the height of all its lines and the page scrolls to its end. */
static void
test_page_wraps_its_content (void)
{
  GtkWidget *page = g_object_ref_sink (awra_page_new ());
  GtkWidget *label = gtk_label_new (
    "A long explanation that wraps onto many lines when the page is narrow, "
    "and onto few when it is wide, so its height depends on its width.");
  GtkWidget *viewport;
  int narrow = 0;
  int wide = 0;

  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  awra_page_set_child (AWRA_PAGE (page), label);
  viewport = find_css_name (page, "awrapageviewport");
  g_assert_nonnull (viewport);
  g_assert_cmpint (gtk_widget_get_request_mode (viewport), ==, GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH);
  gtk_widget_measure (viewport, GTK_ORIENTATION_VERTICAL, 220, &narrow, NULL, NULL, NULL);
  gtk_widget_measure (viewport, GTK_ORIENTATION_VERTICAL, 900, &wide, NULL, NULL, NULL);
  g_assert_cmpint (narrow, >, wide);
  g_object_unref (page);
}

static void
palette_chosen_cb (AwraColorPalette *palette,
                   const GdkRGBA    *color,
                   int              *chosen)
{
  (void) palette;
  *chosen = color != NULL ? 1 : 2;
}

/* A palette: its colours, an automatic choice, a colour from outside it,
 * and a signal only for the person's choices. */
static void
test_color_palette (void)
{
  GtkWidget *widget = g_object_ref_sink (awra_color_palette_new ());
  AwraColorPalette *palette = AWRA_COLOR_PALETTE (widget);
  GdkRGBA red = { 0.9, 0.2, 0.2, 1.0 };
  GdkRGBA blue = { 0.2, 0.4, 0.9, 1.0 };
  GdkRGBA odd = { 0.3, 0.7, 0.5, 1.0 };
  GtkWidget *swatch = NULL;
  int chosen = 0;

  g_assert_null (awra_color_palette_get_selected (palette));
  awra_color_palette_set_automatic_label (palette, "System");
  g_assert_cmpstr (awra_color_palette_get_automatic_label (palette), ==, "System");
  awra_color_palette_add_color (palette, &red, "Red");
  awra_color_palette_add_color (palette, &blue, NULL);
  awra_color_palette_set_allows_custom (palette, TRUE);
  g_assert_true (awra_color_palette_get_allows_custom (palette));
  g_signal_connect (widget, "chosen", G_CALLBACK (palette_chosen_cb), &chosen);

  awra_color_palette_set_selected (palette, &blue);
  g_assert_true (gdk_rgba_equal (awra_color_palette_get_selected (palette), &blue));
  g_assert_cmpint (chosen, ==, 0);
  awra_color_palette_set_selected (palette, &odd);
  g_assert_true (gdk_rgba_equal (awra_color_palette_get_selected (palette), &odd));

  /* Clicking the red swatch chooses it and says so. */
  for (GtkWidget *child = find_css_name (widget, "flowbox") != NULL
         ? gtk_widget_get_first_child (find_css_name (widget, "flowbox")) : NULL;
       child != NULL && swatch == NULL; child = gtk_widget_get_next_sibling (child)) {
    GtkWidget *button = gtk_flow_box_child_get_child (GTK_FLOW_BOX_CHILD (child));

    if (g_strcmp0 (gtk_widget_get_tooltip_text (button), "Red") == 0)
      swatch = button;
  }
  g_assert_nonnull (swatch);
  g_signal_emit_by_name (swatch, "clicked");
  g_assert_cmpint (chosen, ==, 1);
  g_assert_true (gdk_rgba_equal (awra_color_palette_get_selected (palette), &red));
  g_assert_true (gtk_widget_has_css_class (swatch, "awra-selected"));

  awra_color_palette_set_selected (palette, NULL);
  g_assert_null (awra_color_palette_get_selected (palette));
  g_assert_false (gtk_widget_has_css_class (swatch, "awra-selected"));
  awra_color_palette_clear (palette);
  g_object_unref (widget);
}

/* A widget with a context menu lets its popover go as it's disposed, as
 * a list's recycled rows are, without leaving GTK a child to warn about. */
static void
test_context_menu_released_with_widget (void)
{
  g_autoptr (GMenu) menu = g_menu_new ();
  GtkWidget *box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  GtkWidget *label = gtk_label_new ("Row");

  g_menu_append (menu, "Copy", "row.copy");
  gtk_box_append (GTK_BOX (box), label);
  awra_widget_set_context_menu_model (label, G_MENU_MODEL (menu));
  g_assert_nonnull (awra_widget_get_context_menu_popover (label));
  /* Disposing the label with its parent must not warn. */
  gtk_box_remove (GTK_BOX (box), label);
  g_object_unref (box);
}

static void
count_signal_cb (GtkWidget *widget,
                 guint     *count)
{
  (void) widget;
  (*count)++;
}

/* Enter in a search entry reaches its own activate signal. */
static void
test_search_entry_activate (void)
{
  GtkWidget *entry = g_object_ref_sink (awra_search_entry_new ());
  guint activated = 0;

  g_signal_connect (entry, "activate", G_CALLBACK (count_signal_cb), &activated);
  g_signal_emit_by_name (awra_search_entry_get_delegate (AWRA_SEARCH_ENTRY (entry)), "activate");
  g_assert_cmpuint (activated, ==, 1);
  g_object_unref (entry);
}

static GtkWidget *
find_css_class (GtkWidget  *widget,
                const char *name)
{
  if (gtk_widget_has_css_class (widget, name))
    return widget;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    GtkWidget *found = find_css_class (child, name);

    if (found != NULL)
      return found;
  }
  return NULL;
}

static void
edge_retry_cb (AwraConversationView *view,
               GtkPositionType       edge,
               int                  *retried)
{
  (void) view;
  *retried = edge == GTK_POS_TOP ? 1 : 2;
}

/* An edge says what it's doing, and a failed one offers to retry. */
static void
test_conversation_edge_status (void)
{
  GtkWidget *view = g_object_ref_sink (awra_conversation_view_new (NULL, NULL));
  int retried = 0;

  g_assert_cmpint (awra_conversation_view_get_edge_status (AWRA_CONVERSATION_VIEW (view), GTK_POS_TOP),
                   ==, AWRA_EDGE_STATUS_NONE);
  awra_conversation_view_set_edge_status (AWRA_CONVERSATION_VIEW (view), GTK_POS_TOP,
                                          AWRA_EDGE_STATUS_LOADING, "Loading earlier messages…");
  g_assert_cmpint (awra_conversation_view_get_edge_status (AWRA_CONVERSATION_VIEW (view), GTK_POS_TOP),
                   ==, AWRA_EDGE_STATUS_LOADING);
  g_signal_connect (view, "retry", G_CALLBACK (edge_retry_cb), &retried);
  awra_conversation_view_set_edge_status (AWRA_CONVERSATION_VIEW (view), GTK_POS_TOP,
                                          AWRA_EDGE_STATUS_FAILED, "Couldn't load earlier messages");
  g_signal_emit_by_name (find_css_class (view, "awra-conversation-edge-retry"), "clicked");
  g_assert_cmpint (retried, ==, 1);
  awra_conversation_view_set_edge_status (AWRA_CONVERSATION_VIEW (view), GTK_POS_TOP,
                                          AWRA_EDGE_STATUS_NONE, NULL);
  g_assert_cmpint (awra_conversation_view_get_edge_status (AWRA_CONVERSATION_VIEW (view), GTK_POS_BOTTOM),
                   ==, AWRA_EDGE_STATUS_NONE);
  g_object_unref (view);
}

/* A rail item allocates its children itself; its context menu still
 * shows. */
static void
test_rail_item_context_menu (void)
{
  g_autoptr (GMenu) menu = g_menu_new ();
  GtkWidget *window = gtk_window_new ();
  GtkWidget *item = awra_rail_item_new ("Server");
  AwraPopover *popover;

  g_menu_append (menu, "Mute", "rail.mute");
  gtk_window_set_child (GTK_WINDOW (window), item);
  awra_widget_set_context_menu_model (item, G_MENU_MODEL (menu));
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);
  popover = awra_widget_get_context_menu_popover (item);
  gtk_popover_popup (GTK_POPOVER (popover));
  settle_window (window);
  g_assert_true (gtk_widget_get_mapped (GTK_WIDGET (popover)));
  gtk_popover_popdown (GTK_POPOVER (popover));
  gtk_window_destroy (GTK_WINDOW (window));
}

/* Avatars in a group overlap: narrower together than side by side. */
static void
test_avatar_group (void)
{
  GtkWidget *group = g_object_ref_sink (awra_avatar_group_new ());
  int one = 0;
  int three = 0;

  awra_avatar_group_append (AWRA_AVATAR_GROUP (group), awra_avatar_new (40, "Bob"));
  gtk_widget_measure (group, GTK_ORIENTATION_HORIZONTAL, -1, &one, NULL, NULL, NULL);
  awra_avatar_group_append (AWRA_AVATAR_GROUP (group), awra_avatar_new (40, "Carol"));
  awra_avatar_group_append (AWRA_AVATAR_GROUP (group), awra_avatar_new (40, "Dave"));
  gtk_widget_measure (group, GTK_ORIENTATION_HORIZONTAL, -1, &three, NULL, NULL, NULL);
  g_assert_cmpint (three, >, one);
  g_assert_cmpint (three, <, 3 * one);
  gtk_widget_size_allocate (group, &(GtkAllocation) { 0, 0, three, one }, -1);
  awra_avatar_group_remove_all (AWRA_AVATAR_GROUP (group));
  g_assert_null (gtk_widget_get_first_child (group));
  g_object_unref (group);
}

/* A subtitle allowed more lines wraps onto them at a narrow width. */
static void
test_action_row_subtitle_lines (void)
{
  GtkWidget *row = g_object_ref_sink (awra_action_row_new (
    "Notifications", "Shows a notification for every message in the channels you follow, even muted ones"));
  int one_line = 0;
  int wrapped = 0;

  g_assert_cmpint (awra_action_row_get_subtitle_lines (AWRA_ACTION_ROW (row)), ==, 1);
  gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, 160, &one_line, NULL, NULL, NULL);
  awra_action_row_set_subtitle_lines (AWRA_ACTION_ROW (row), 0);
  gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, 160, &wrapped, NULL, NULL, NULL);
  g_assert_cmpint (wrapped, >, one_line);
  awra_action_row_set_subtitle_lines (AWRA_ACTION_ROW (row), 1);
  gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, 160, &wrapped, NULL, NULL, NULL);
  g_assert_cmpint (wrapped, ==, one_line);
  g_object_unref (row);
}

/* A busy button keeps its size, hides its content and ignores clicks; an
 * icon and label content names it by its label. */
static void
test_button_busy_and_content (void)
{
  GtkWidget *button = g_object_ref_sink (awra_button_new_with_icon_and_label ("edit-copy-symbolic", "Copy"));
  GtkWidget *content = gtk_button_get_child (GTK_BUTTON (button));
  guint clicks = 0;
  int idle_width = 0;
  int busy_width = 0;

  g_assert_true (AWRA_IS_BUTTON_CONTENT (content));
  g_assert_cmpstr (awra_button_content_get_icon_name (AWRA_BUTTON_CONTENT (content)), ==, "edit-copy-symbolic");
  g_assert_cmpstr (awra_button_content_get_label (AWRA_BUTTON_CONTENT (content)), ==, "Copy");
  awra_button_content_set_label (AWRA_BUTTON_CONTENT (content), "");
  g_assert_null (awra_button_content_get_label (AWRA_BUTTON_CONTENT (content)));
  awra_button_content_set_label (AWRA_BUTTON_CONTENT (content), "Copy link");

  g_signal_connect (button, "clicked", G_CALLBACK (count_clicks_cb), &clicks);
  gtk_widget_measure (button, GTK_ORIENTATION_HORIZONTAL, -1, &idle_width, NULL, NULL, NULL);
  awra_button_set_busy (AWRA_BUTTON (button), TRUE);
  g_assert_true (awra_button_get_busy (AWRA_BUTTON (button)));
  g_assert_true (gtk_widget_has_css_class (button, "awra-busy"));
  g_assert_cmpfloat (gtk_widget_get_opacity (content), ==, 0.0);
  gtk_widget_measure (button, GTK_ORIENTATION_HORIZONTAL, -1, &busy_width, NULL, NULL, NULL);
  g_assert_cmpint (busy_width, ==, idle_width);
  g_signal_emit_by_name (button, "clicked");
  g_assert_cmpuint (clicks, ==, 0);

  awra_button_set_busy (AWRA_BUTTON (button), FALSE);
  g_assert_cmpfloat (gtk_widget_get_opacity (content), ==, 1.0);
  g_signal_emit_by_name (button, "clicked");
  g_assert_cmpuint (clicks, ==, 1);
  g_object_unref (button);
}

static GtkWidget *
find_role (GtkWidget         *widget,
           GtkAccessibleRole  role)
{
  if (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (widget)) == role)
    return widget;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    GtkWidget *found = find_role (child, role);

    if (found != NULL)
      return found;
  }
  return NULL;
}

/* What assistive technologies are told: radio segments with one Tab stop,
 * required form fields, toasts as status updates, tabs of a tab list. */
static void
test_accessibility_semantics (void)
{
  g_autoptr (GtkWidget) segmented = g_object_ref_sink (awra_segmented_control_new ());
  g_autoptr (GtkWidget) row = g_object_ref_sink (awra_form_row_new ());
  g_autoptr (GtkWidget) toasts = g_object_ref_sink (awra_toast_overlay_new ());
  g_autoptr (GtkWidget) tabs = g_object_ref_sink (awra_tab_view_new ());
  GtkWidget *entry = awra_entry_new ();
  GtkWidget *first;
  GtkWidget *second;
  g_autoptr (GtkATContext) context = NULL;
  gboolean states;

  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segmented), "Day");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segmented), "Week");
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (segmented)), ==,
                   GTK_ACCESSIBLE_ROLE_RADIO_GROUP);
  first = gtk_widget_get_first_child (gtk_widget_get_first_child (segmented));
  second = gtk_widget_get_next_sibling (first);
  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (first)), ==,
                   GTK_ACCESSIBLE_ROLE_RADIO);
  /* States and properties live in an AT context, absent with GTK_A11Y=none. */
  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (first));
  states = context != NULL;
  if (states)
    gtk_test_accessible_assert_state (first, GTK_ACCESSIBLE_STATE_CHECKED,
                                      GTK_ACCESSIBLE_TRISTATE_TRUE);
  g_assert_true (gtk_widget_get_focusable (first));
  g_assert_false (gtk_widget_get_focusable (second));
  awra_segmented_control_set_selected (AWRA_SEGMENTED_CONTROL (segmented), 1);
  if (states)
    gtk_test_accessible_assert_state (second, GTK_ACCESSIBLE_STATE_CHECKED,
                                      GTK_ACCESSIBLE_TRISTATE_TRUE);
  g_assert_false (gtk_widget_get_focusable (first));
  g_assert_true (gtk_widget_get_focusable (second));

  awra_form_row_set_label (AWRA_FORM_ROW (row), "Name");
  awra_form_row_set_control (AWRA_FORM_ROW (row), entry);
  awra_form_row_set_required (AWRA_FORM_ROW (row), TRUE);
  if (states)
    gtk_test_accessible_assert_property (entry, GTK_ACCESSIBLE_PROPERTY_REQUIRED, TRUE);

  g_assert_nonnull (find_role (toasts, GTK_ACCESSIBLE_ROLE_STATUS));

  awra_tab_view_append (AWRA_TAB_VIEW (tabs), gtk_label_new ("One"), "One", FALSE);
  g_assert_nonnull (find_role (tabs, GTK_ACCESSIBLE_ROLE_TAB_LIST));
  g_assert_nonnull (find_role (tabs, GTK_ACCESSIBLE_ROLE_TAB));
}

/* Interactive suffix controls must see clicks before the row claims them. */
static void
test_action_row_suffix_receives_clicks (void)
{
  g_autoptr (GtkWidget) row = g_object_ref_sink (
    awra_action_row_new ("Track", "Interactive suffix"));
  g_autoptr (GListModel) controllers = NULL;
  gboolean found_click = FALSE;

  awra_action_row_set_suffix (AWRA_ACTION_ROW (row),
                              gtk_button_new_from_icon_name ("list-add-symbolic"));
  controllers = gtk_widget_observe_controllers (row);
  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
    g_autoptr (GtkEventController) controller =
      g_list_model_get_item (controllers, i);

    if (!GTK_IS_GESTURE_CLICK (controller))
      continue;
    found_click = TRUE;
    g_assert_cmpint (gtk_event_controller_get_propagation_phase (controller),
                     ==, GTK_PHASE_BUBBLE);
  }
  g_assert_true (found_click);
}

static void
test_window_and_popover (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestWidgets",
    G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  GtkWidget *content;
  GtkWidget *popover;
  GtkWidget *popover_child;
  GtkWidget *default_chrome;
  GtkWidget *custom_chrome;

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  window = awra_window_new (app);
  default_chrome = awra_window_get_chrome (AWRA_WINDOW (window));
  g_assert_true (AWRA_IS_HEADER (default_chrome));
  g_assert_true (awra_header_get_blend_with_window (
    AWRA_HEADER (default_chrome)));
  g_assert_true (awra_header_get_material (AWRA_HEADER (default_chrome)) ==
                 awra_surface_get_material (
                   awra_window_get_root_surface (AWRA_WINDOW (window))));
  g_assert_cmpint (awra_surface_get_corner_mask (
                     awra_window_get_root_surface (AWRA_WINDOW (window))), ==,
                   AWRA_SURFACE_CORNER_BOTTOM_START |
                   AWRA_SURFACE_CORNER_BOTTOM_END);
  g_assert_cmpint (awra_surface_get_edge_mask (
                     awra_window_get_root_surface (AWRA_WINDOW (window))), ==,
                   AWRA_SURFACE_EDGE_BOTTOM |
                   AWRA_SURFACE_EDGE_START |
                   AWRA_SURFACE_EDGE_END);
  custom_chrome = awra_header_new ();
  awra_header_set_blend_with_window (AWRA_HEADER (custom_chrome), TRUE);
  awra_header_set_drag_enabled (AWRA_HEADER (custom_chrome), TRUE);
  awra_window_set_chrome (AWRA_WINDOW (window), custom_chrome);
  g_assert_true (awra_window_get_chrome (AWRA_WINDOW (window)) == custom_chrome);
  awra_window_set_chrome (AWRA_WINDOW (window), NULL);
  g_assert_true (awra_window_get_chrome (AWRA_WINDOW (window)) == default_chrome);
  content = gtk_label_new ("Content");
  awra_window_set_content (AWRA_WINDOW (window), content);
  g_assert_true (awra_window_get_content (AWRA_WINDOW (window)) == content);
  g_assert_cmpint (awra_surface_get_role (
                     awra_window_get_root_surface (AWRA_WINDOW (window))),
                   ==,
                   AWRA_SURFACE_ROLE_WINDOW);
  g_assert_cmpint (awra_material_get_preset (awra_surface_get_material (
                     awra_window_get_root_surface (AWRA_WINDOW (window)))), ==,
                   AWRA_MATERIAL_PRESET_CANVAS);

  popover = awra_popover_new ();
  popover_child = gtk_label_new ("Popover content");
  awra_popover_set_child (AWRA_POPOVER (popover), popover_child);
  g_assert_true (awra_popover_get_child (AWRA_POPOVER (popover)) == popover_child);
  g_assert_cmpint (awra_material_get_preset (awra_surface_get_material (
                     awra_popover_get_surface (AWRA_POPOVER (popover)))), ==,
                   AWRA_MATERIAL_PRESET_FLOATING);
  g_object_ref_sink (popover);
  g_object_unref (popover);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_overlays_and_navigation (void)
{
  static const char *choices[] = { "System", "Light", "Dark", NULL };
  g_autoptr (GtkWidget) dropdown = g_object_ref_sink (
    awra_dropdown_new_from_strings (choices));
  g_autoptr (GtkWidget) navigation = g_object_ref_sink (
    awra_navigation_view_new ());
  g_autoptr (GtkWidget) toast = g_object_ref_sink (
    awra_toast_overlay_new ());
  g_autoptr (GtkWidget) popover = g_object_ref_sink (awra_popover_new ());
  g_autoptr (GtkWidget) menu_button = g_object_ref_sink (awra_menu_button_new ());
  g_autoptr (GtkWidget) nav_a = g_object_ref_sink (
    awra_navigation_item_new_with_label ("Overview"));
  g_autoptr (GtkWidget) nav_b = g_object_ref_sink (
    awra_navigation_item_new_with_label ("Controls"));
  g_autoptr (GMenu) menu = g_menu_new ();
  GtkWidget *content = gtk_label_new ("Toast content");
  GtkWidget *home = gtk_label_new ("Home");
  GtkWidget *details = gtk_label_new ("Details");

  g_assert_cmpuint (g_list_model_get_n_items (
                      awra_dropdown_get_model (AWRA_DROPDOWN (dropdown))),
                    ==,
                    3);
  awra_dropdown_set_selected (AWRA_DROPDOWN (dropdown), 2);
  g_assert_cmpuint (awra_dropdown_get_selected (AWRA_DROPDOWN (dropdown)), ==, 2);
  g_assert_true (GTK_IS_DROP_DOWN (
    awra_dropdown_get_delegate (AWRA_DROPDOWN (dropdown))));
  g_assert_null (gtk_widget_get_parent (GTK_WIDGET (
    awra_dropdown_get_delegate (AWRA_DROPDOWN (dropdown)))));
  g_assert_true (GTK_IS_POPOVER_BIN (
    gtk_widget_get_first_child (dropdown)));
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (dropdown)), ==,
                   GTK_ACCESSIBLE_ROLE_COMBO_BOX);
  g_assert_true (AWRA_IS_POPOVER (
    awra_dropdown_get_popover (AWRA_DROPDOWN (dropdown))));
  g_assert_cmpint (awra_material_get_preset (awra_surface_get_material (
                     awra_popover_get_surface (awra_dropdown_get_popover (
                       AWRA_DROPDOWN (dropdown))))), ==,
                   AWRA_MATERIAL_PRESET_FLOATING);
  gtk_drop_down_set_selected (
    awra_dropdown_get_delegate (AWRA_DROPDOWN (dropdown)), 1);
  g_assert_cmpuint (awra_dropdown_get_selected (AWRA_DROPDOWN (dropdown)), ==, 1);

  awra_navigation_view_add (AWRA_NAVIGATION_VIEW (navigation), home, "home");
  awra_navigation_view_add (AWRA_NAVIGATION_VIEW (navigation), details, "details");
  g_assert_true (awra_navigation_view_push (AWRA_NAVIGATION_VIEW (navigation),
                                            "home"));
  g_assert_true (awra_navigation_view_push (AWRA_NAVIGATION_VIEW (navigation),
                                            "details"));
  g_assert_true (awra_navigation_view_get_can_pop (
    AWRA_NAVIGATION_VIEW (navigation)));
  g_assert_cmpstr (awra_navigation_view_get_visible_page_name (
                     AWRA_NAVIGATION_VIEW (navigation)),
                   ==,
                   "details");
  g_assert_true (awra_navigation_view_pop (AWRA_NAVIGATION_VIEW (navigation)));
  g_assert_cmpstr (awra_navigation_view_get_visible_page_name (
                     AWRA_NAVIGATION_VIEW (navigation)),
                   ==,
                   "home");
  g_assert_true (awra_navigation_view_replace (AWRA_NAVIGATION_VIEW (navigation),
                                               "details"));
  g_assert_false (awra_navigation_view_get_can_pop (
    AWRA_NAVIGATION_VIEW (navigation)));

  awra_toast_overlay_set_child (AWRA_TOAST_OVERLAY (toast), content);
  g_assert_true (awra_toast_overlay_get_child (AWRA_TOAST_OVERLAY (toast)) == content);
  awra_toast_overlay_show_message (AWRA_TOAST_OVERLAY (toast), "Saved", 0);
  g_assert_true (awra_toast_overlay_get_revealed (AWRA_TOAST_OVERLAY (toast)));

  g_menu_append (menu, "Refresh", "app.refresh");
  g_menu_append (menu, "Close", "win.close");
  awra_popover_set_menu_model (AWRA_POPOVER (popover), G_MENU_MODEL (menu));
  g_assert_true (awra_popover_get_menu_model (AWRA_POPOVER (popover)) ==
                 G_MENU_MODEL (menu));
  g_assert_true (GTK_IS_SCROLLED_WINDOW (awra_popover_get_child (AWRA_POPOVER (popover))));
  awra_menu_button_set_menu_model (AWRA_MENU_BUTTON (menu_button),
                                   G_MENU_MODEL (menu));
  g_assert_true (AWRA_IS_POPOVER (
    awra_menu_button_get_popover (AWRA_MENU_BUTTON (menu_button))));
  g_assert_true (GTK_IS_POPOVER_BIN (
    gtk_widget_get_first_child (menu_button)));
  g_assert_true (AWRA_IS_BUTTON (gtk_popover_bin_get_child (
    GTK_POPOVER_BIN (gtk_widget_get_first_child (menu_button)))));
  g_assert_false (GTK_IS_MENU_BUTTON (gtk_popover_bin_get_child (
    GTK_POPOVER_BIN (gtk_widget_get_first_child (menu_button)))));
  awra_menu_button_set_appearance (AWRA_MENU_BUTTON (menu_button),
                                   AWRA_BUTTON_APPEARANCE_TOOLBAR);
  g_assert_cmpint (awra_menu_button_get_appearance (
                     AWRA_MENU_BUTTON (menu_button)), ==,
                   AWRA_BUTTON_APPEARANCE_TOOLBAR);

  awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (nav_a),
                                      "go-home-symbolic");
  awra_navigation_item_set_group (AWRA_NAVIGATION_ITEM (nav_b),
                                  AWRA_NAVIGATION_ITEM (nav_a));
  awra_navigation_item_set_selected (AWRA_NAVIGATION_ITEM (nav_a), TRUE);
  g_assert_true (awra_navigation_item_get_selected (
    AWRA_NAVIGATION_ITEM (nav_a)));
  g_assert_cmpstr (awra_navigation_item_get_label (
                     AWRA_NAVIGATION_ITEM (nav_a)), ==, "Overview");
  g_assert_cmpstr (awra_navigation_item_get_icon_name (
                     AWRA_NAVIGATION_ITEM (nav_a)), ==, "go-home-symbolic");
  g_assert_cmpint (gtk_accessible_get_accessible_role (
                     GTK_ACCESSIBLE (nav_a)), ==, GTK_ACCESSIBLE_ROLE_BUTTON);

  awra_widget_set_tooltip_text (menu_button, "Application actions");
  g_assert_cmpstr (gtk_widget_get_tooltip_text (menu_button),
                   ==,
                   "Application actions");
}

static void
test_navigation_item_details (void)
{
  GtkWidget *item = awra_navigation_item_new_with_label ("Ada");
  GtkWidget *prefix = gtk_label_new ("A");
  GtkWidget *other = gtk_label_new ("B");

  g_object_ref_sink (item);
  g_object_ref_sink (prefix);
  awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (item),
                                      "go-home-symbolic");
  /* A prefix stands in for the icon, and gives its place back. */
  awra_navigation_item_set_prefix (AWRA_NAVIGATION_ITEM (item), prefix);
  g_assert_true (awra_navigation_item_get_prefix (
                   AWRA_NAVIGATION_ITEM (item)) == prefix);
  g_assert_true (gtk_widget_is_ancestor (prefix, item));
  awra_navigation_item_set_prefix (AWRA_NAVIGATION_ITEM (item), other);
  g_assert_null (gtk_widget_get_parent (prefix));
  g_assert_true (gtk_widget_is_ancestor (other, item));
  awra_navigation_item_set_prefix (AWRA_NAVIGATION_ITEM (item), NULL);
  g_assert_null (awra_navigation_item_get_prefix (AWRA_NAVIGATION_ITEM (item)));

  /* The subtitle shows only while it says something. */
  g_assert_cmpstr (awra_navigation_item_get_subtitle (
                     AWRA_NAVIGATION_ITEM (item)), ==, "");
  awra_navigation_item_set_subtitle (AWRA_NAVIGATION_ITEM (item),
                                     "Playing chess");
  g_assert_cmpstr (awra_navigation_item_get_subtitle (
                     AWRA_NAVIGATION_ITEM (item)), ==, "Playing chess");
  awra_navigation_item_set_subtitle (AWRA_NAVIGATION_ITEM (item), NULL);
  g_assert_cmpstr (awra_navigation_item_get_subtitle (
                     AWRA_NAVIGATION_ITEM (item)), ==, "");

  awra_navigation_item_set_emphasized (AWRA_NAVIGATION_ITEM (item), TRUE);
  g_assert_true (awra_navigation_item_get_emphasized (
                   AWRA_NAVIGATION_ITEM (item)));
  g_assert_true (gtk_widget_has_css_class (item, "awra-emphasized"));
  awra_navigation_item_set_muted (AWRA_NAVIGATION_ITEM (item), TRUE);
  g_assert_true (awra_navigation_item_get_muted (AWRA_NAVIGATION_ITEM (item)));
  g_object_set (item, "muted", FALSE, "emphasized", FALSE, NULL);
  g_assert_false (gtk_widget_has_css_class (item, "awra-muted"));
  g_assert_false (gtk_widget_has_css_class (item, "awra-emphasized"));

  /* A label colour is kept as given and drawn readable. */
  {
    GdkRGBA navy = { 0.05, 0.05, 0.3, 1.0 };
    GtkWidget *label = NULL;
    PangoAttrList *attributes;

    g_assert_null (awra_navigation_item_get_label_color (AWRA_NAVIGATION_ITEM (item)));
    awra_navigation_item_set_label_color (AWRA_NAVIGATION_ITEM (item), &navy);
    g_assert_true (gdk_rgba_equal (awra_navigation_item_get_label_color (AWRA_NAVIGATION_ITEM (item)), &navy));
    for (GtkWidget *child = gtk_widget_get_first_child (item); child != NULL && label == NULL;) {
      if (GTK_IS_LABEL (child) && g_str_equal (gtk_label_get_text (GTK_LABEL (child)), "Ada"))
        label = child;
      else if (gtk_widget_get_first_child (child) != NULL)
        child = gtk_widget_get_first_child (child);
      else {
        while (child != NULL && gtk_widget_get_next_sibling (child) == NULL)
          child = child != item ? gtk_widget_get_parent (child) : NULL;
        child = child != NULL && child != item ? gtk_widget_get_next_sibling (child) : NULL;
      }
    }
    g_assert_nonnull (label);
    attributes = gtk_label_get_attributes (GTK_LABEL (label));
    g_assert_nonnull (attributes);
    awra_navigation_item_set_label_color (AWRA_NAVIGATION_ITEM (item), NULL);
    g_assert_null (gtk_label_get_attributes (GTK_LABEL (label)));
  }

  g_object_unref (prefix);
  g_object_unref (item);
}

/* A colour made readable keeps its hue and reaches text contrast. */
static void
test_readable_colors (void)
{
  GdkRGBA navy = { 0.05, 0.05, 0.3, 1.0 };
  GdkRGBA pale = { 1.0, 0.95, 0.6, 1.0 };
  GdkRGBA on_dark = awra_rgba_readable (&navy, TRUE);
  GdkRGBA on_light = awra_rgba_readable (&pale, FALSE);
  GdkRGBA kept = awra_rgba_readable (&navy, FALSE);

  g_assert_cmpfloat (on_dark.blue, >, navy.blue);
  g_assert_cmpfloat (on_dark.blue, >, on_dark.red);
  g_assert_cmpfloat (on_light.red, <, pale.red);
  g_assert_cmpfloat (on_light.red, >, on_light.blue);
  g_assert_true (gdk_rgba_equal (&kept, &navy));
}

static void
test_dialog (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestDialog",
    G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  GtkWidget *parent;
  GtkWidget *dialog;
  int default_width;
  int default_height;

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  parent = awra_window_new (app);
  dialog = awra_dialog_new (GTK_WINDOW (parent));
  gtk_window_get_default_size (GTK_WINDOW (dialog),
                               &default_width,
                               &default_height);
  g_assert_cmpint (default_width, ==, 440);
  g_assert_cmpint (default_height, ==, -1);
  g_assert_true (gtk_window_get_modal (GTK_WINDOW (dialog)));
  g_assert_false (gtk_window_get_resizable (GTK_WINDOW (dialog)));
  g_assert_true (gtk_window_get_transient_for (GTK_WINDOW (dialog)) ==
                 GTK_WINDOW (parent));
  g_assert_cmpint (awra_surface_get_role (
                     awra_window_get_root_surface (AWRA_WINDOW (dialog))),
                   ==,
                   AWRA_SURFACE_ROLE_DIALOG);
  g_assert_cmpint (awra_material_get_preset (awra_surface_get_material (
                     awra_window_get_root_surface (AWRA_WINDOW (dialog)))), ==,
                   AWRA_MATERIAL_PRESET_FLOATING);
  g_assert_cmpint (awra_material_get_preset (awra_header_get_material (
                     AWRA_HEADER (awra_window_get_chrome (
                       AWRA_WINDOW (dialog))))), ==,
                   AWRA_MATERIAL_PRESET_FLOATING);
  gtk_window_destroy (GTK_WINDOW (dialog));
  gtk_window_destroy (GTK_WINDOW (parent));
}

static void
test_command_palette (void)
{
  g_autoptr (GtkApplication) app = gtk_application_new (
    "org.awra.TestCommandPalette", G_APPLICATION_NON_UNIQUE);
  g_autoptr (GError) error = NULL;
  g_autoptr (GMenu) menu = g_menu_new ();
  GtkWidget *parent;
  GtkWidget *palette;

  g_assert_true (g_application_register (G_APPLICATION (app), NULL, &error));
  g_assert_no_error (error);
  parent = awra_window_new (app);
  palette = awra_command_palette_new (GTK_WINDOW (parent));
  g_menu_append (menu, "Open project", "app.open");
  g_menu_append (menu, "Close window", "win.close");
  awra_command_palette_set_menu_model (AWRA_COMMAND_PALETTE (palette),
                                       G_MENU_MODEL (menu));
  awra_command_palette_set_query (AWRA_COMMAND_PALETTE (palette), "open");
  awra_command_palette_set_placeholder_text (
    AWRA_COMMAND_PALETTE (palette), "Search actions");
  awra_command_palette_set_title (AWRA_COMMAND_PALETTE (palette),
                                  "Quick actions");
  awra_command_palette_set_empty_text (AWRA_COMMAND_PALETTE (palette),
                                       "Nothing found");
  g_assert_true (awra_command_palette_get_menu_model (
                   AWRA_COMMAND_PALETTE (palette)) == G_MENU_MODEL (menu));
  g_assert_cmpstr (awra_command_palette_get_query (
                     AWRA_COMMAND_PALETTE (palette)), ==, "open");
  g_assert_cmpstr (awra_command_palette_get_placeholder_text (
                     AWRA_COMMAND_PALETTE (palette)), ==, "Search actions");
  g_assert_cmpstr (awra_command_palette_get_title (
                     AWRA_COMMAND_PALETTE (palette)), ==, "Quick actions");
  g_assert_cmpstr (awra_command_palette_get_empty_text (
                     AWRA_COMMAND_PALETTE (palette)), ==, "Nothing found");
  /* The window header carries the title; the body does not repeat it. */
  g_assert_cmpstr (gtk_window_get_title (GTK_WINDOW (palette)), ==,
                   "Quick actions");

  {
    g_autoptr (GMenu) model = g_menu_new ();
    g_autoptr (GMenu) section = g_menu_new ();
    g_autoptr (GMenuItem) item = g_menu_item_new ("Undo", "win.undo");

    g_menu_item_set_attribute (item, "accel", "s", "<Control>z");
    g_menu_append_item (section, item);
    g_menu_append (section, "Redo", "win.redo");
    g_menu_append_section (model, "Edit", G_MENU_MODEL (section));
    awra_command_palette_set_query (AWRA_COMMAND_PALETTE (palette), "");
    awra_command_palette_set_menu_model (AWRA_COMMAND_PALETTE (palette),
                                         G_MENU_MODEL (model));
    g_assert_nonnull (find_descendant_with_class (palette,
                                                  "awra-command-heading"));
    g_assert_nonnull (find_descendant_with_class (palette,
                                                  "awra-command-accel"));
  }
  {
    GWeakRef weak;
    g_autoptr (GtkWidget) alive = NULL;

    /* A language binding releases its transfer-full result after present().
     * GTK must still own the palette, just as it owns other toplevels. */
    g_weak_ref_init (&weak, palette);
    g_object_unref (palette);
    alive = g_weak_ref_get (&weak);
    g_assert_nonnull (alive);
    gtk_window_destroy (GTK_WINDOW (alive));
    g_weak_ref_clear (&weak);
  }
  gtk_window_destroy (GTK_WINDOW (parent));
}

static void
test_extended_components (void)
{
  static const char *items[] = { "Alpha", "Beta", "Gamma", NULL };
  AwraContext *context = awra_context_get_for_display (gdk_display_get_default ());
  AwraStyleManager *manager = awra_context_get_style_manager (context);
  g_autoptr (AwraTokenSet) default_tokens = awra_token_set_new (
    TRUE, NULL, FALSE);
  g_autoptr (GtkWidget) badge = g_object_ref_sink (awra_badge_new ("3"));
  g_autoptr (GtkWidget) progress = g_object_ref_sink (awra_progress_bar_new ());
  g_autoptr (GtkWidget) segments = g_object_ref_sink (
    awra_segmented_control_new ());
  g_autoptr (GtkWidget) tabs = g_object_ref_sink (awra_tab_view_new ());
  g_autoptr (GtkStringList) strings = gtk_string_list_new (items);
  g_autoptr (GtkSingleSelection) selection = gtk_single_selection_new (
    G_LIST_MODEL (g_steal_pointer (&strings)));
  g_autoptr (GtkListItemFactory) factory = gtk_signal_list_item_factory_new ();
  g_autoptr (GtkWidget) list = g_object_ref_sink (
    awra_list_view_new (GTK_SELECTION_MODEL (selection), factory));
  g_autoptr (GtkWidget) list_header = g_object_ref_sink (
    gtk_label_new ("Name"));
  g_autoptr (GtkWidget) card = g_object_ref_sink (awra_card_new ());
  g_autoptr (GtkWidget) inspector = g_object_ref_sink (awra_inspector_new ());
  g_autoptr (GtkWidget) inspected_target = NULL;
  g_autofree char *inspector_report = NULL;
  g_autofree char *report_path = NULL;
  g_autoptr (GError) report_error = NULL;
  const GdkRGBA blue = { 0.19, 0.36, 0.94, 1.0 };

  awra_style_manager_set_accent_source (manager, AWRA_ACCENT_SOURCE_DEFAULT);
  g_assert_cmpint (awra_style_manager_get_accent_source (manager), ==,
                   AWRA_ACCENT_SOURCE_DEFAULT);
  awra_style_manager_set_accent (manager, &blue);
  g_assert_cmpint (awra_style_manager_get_accent_source (manager), ==,
                   AWRA_ACCENT_SOURCE_CUSTOM);
  g_assert_true (gdk_rgba_equal (awra_style_manager_get_accent (manager),
                                 &blue));
  awra_style_manager_set_accent_source (manager, AWRA_ACCENT_SOURCE_SYSTEM);
  if (!awra_style_manager_get_system_accent_available (manager))
    g_assert_true (gdk_rgba_equal (awra_style_manager_get_accent (manager),
                                   awra_token_set_get_accent (default_tokens)));
  awra_style_manager_set_accent_source (manager, AWRA_ACCENT_SOURCE_DEFAULT);

  /* Durations follow the system's animation preference. */
  if (awra_style_manager_get_reduced_motion (manager)) {
    g_assert_cmpuint (awra_motion_get_duration (manager,
                                                AWRA_MOTION_PRESET_FAST),
                      ==,
                      0);
  } else {
    g_assert_cmpuint (awra_motion_get_duration (manager,
                                                AWRA_MOTION_PRESET_FAST),
                      >=,
                      90);
    g_assert_cmpuint (awra_motion_get_duration (manager,
                                                AWRA_MOTION_PRESET_FAST),
                      <=,
                      140);
  }
  awra_style_manager_set_reduced_motion (manager, TRUE);
  g_assert_cmpuint (awra_motion_get_duration (manager,
                                              AWRA_MOTION_PRESET_SLOW),
                    ==,
                    0);
  awra_style_manager_set_reduced_motion (manager, FALSE);
  g_assert_cmpfloat (awra_motion_ease_out_cubic (0.0), ==, 0.0);
  g_assert_cmpfloat (awra_motion_ease_out_cubic (1.0), ==, 1.0);
  g_assert_cmpfloat (awra_motion_ease_in_out_cubic (0.5), ==, 0.5);

  awra_badge_set_text (AWRA_BADGE (badge), "12");
  g_assert_cmpstr (awra_badge_get_text (AWRA_BADGE (badge)), ==, "12");
  awra_progress_bar_set_fraction (AWRA_PROGRESS_BAR (progress), 0.65);
  awra_progress_bar_set_text (AWRA_PROGRESS_BAR (progress), "65 %");
  awra_progress_bar_set_show_text (AWRA_PROGRESS_BAR (progress), TRUE);
  g_assert_cmpfloat (awra_progress_bar_get_fraction (
                       AWRA_PROGRESS_BAR (progress)),
                     ==,
                     0.65);
  g_assert_cmpstr (awra_progress_bar_get_text (AWRA_PROGRESS_BAR (progress)),
                   ==,
                   "65 %");
  g_assert_true (awra_progress_bar_get_show_text (
    AWRA_PROGRESS_BAR (progress)));

  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "Day");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "Week");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "Month");
  g_assert_cmpuint (awra_segmented_control_get_n_items (
                      AWRA_SEGMENTED_CONTROL (segments)),
                    ==,
                    3);
  awra_segmented_control_set_selected (AWRA_SEGMENTED_CONTROL (segments), 2);
  g_assert_cmpuint (awra_segmented_control_get_selected (
                      AWRA_SEGMENTED_CONTROL (segments)),
                    ==,
                    2);

  awra_tab_view_add (AWRA_TAB_VIEW (tabs), gtk_label_new ("Overview"),
                     "overview", "Overview");
  awra_tab_view_add (AWRA_TAB_VIEW (tabs), gtk_label_new ("Details"),
                     "details", "Details");
  awra_tab_view_set_selected_name (AWRA_TAB_VIEW (tabs), "details");
  g_assert_cmpstr (awra_tab_view_get_selected_name (AWRA_TAB_VIEW (tabs)),
                   ==,
                   "details");
  g_assert_true (awra_list_view_get_model (AWRA_LIST_VIEW (list)) ==
                 GTK_SELECTION_MODEL (selection));
  g_assert_true (awra_list_view_get_factory (AWRA_LIST_VIEW (list)) == factory);
  awra_list_view_set_header (AWRA_LIST_VIEW (list), list_header);
  g_assert_true (awra_list_view_get_header (AWRA_LIST_VIEW (list)) ==
                 list_header);
  g_assert_true (gtk_widget_has_css_class (list_header, "awra-list-header"));
  awra_list_view_set_header (AWRA_LIST_VIEW (list), NULL);
  g_assert_null (awra_list_view_get_header (AWRA_LIST_VIEW (list)));
  g_assert_false (gtk_widget_has_css_class (list_header, "awra-list-header"));

  g_assert_cmpint (awra_card_get_appearance (AWRA_CARD (card)), ==,
                   AWRA_CARD_APPEARANCE_TINTED);
  awra_card_set_appearance (AWRA_CARD (card), AWRA_CARD_APPEARANCE_PLAIN);
  g_assert_cmpint (awra_material_get_preset (awra_surface_get_material (
                     awra_card_get_surface (AWRA_CARD (card)))), ==,
                   AWRA_MATERIAL_PRESET_CONTENT);
  awra_card_set_appearance (AWRA_CARD (card), AWRA_CARD_APPEARANCE_RAISED);
  g_assert_cmpint (awra_surface_get_elevation_level (
                     awra_card_get_surface (AWRA_CARD (card))), ==,
                   AWRA_ELEVATION_RAISED);

  awra_inspector_set_target (AWRA_INSPECTOR (inspector), card);
  g_assert_nonnull (strstr (awra_inspector_get_details (
                              AWRA_INSPECTOR (inspector)),
                            "AwraCard"));
  awra_inspector_set_effect_region_visible (AWRA_INSPECTOR (inspector), TRUE);
  g_assert_true (gtk_widget_has_css_class (
    GTK_WIDGET (awra_card_get_surface (AWRA_CARD (card))),
    "awra-effect-region-debug"));
  awra_inspector_set_overdraw_visible (AWRA_INSPECTOR (inspector), TRUE);
  g_assert_true (gtk_widget_has_css_class (
    GTK_WIDGET (awra_card_get_surface (AWRA_CARD (card))),
    "awra-overdraw-debug"));
  inspector_report = awra_inspector_dup_report (AWRA_INSPECTOR (inspector));
  g_assert_nonnull (strstr (inspector_report, "Resolved surface"));
  g_assert_nonnull (strstr (inspector_report, "active-fill"));
  g_assert_nonnull (strstr (inspector_report, "effect-backend"));
  {
    int fd = g_file_open_tmp ("awra-inspector-XXXXXX", &report_path,
                              &report_error);
    g_assert_no_error (report_error);
    g_assert_cmpint (fd, >=, 0);
    close (fd);
  }
  g_assert_true (awra_inspector_export_report (AWRA_INSPECTOR (inspector),
                                               report_path, &report_error));
  g_assert_no_error (report_error);
  g_assert_cmpint (g_remove (report_path), ==, 0);
  inspected_target = awra_inspector_get_target (AWRA_INSPECTOR (inspector));
  g_assert_true (inspected_target == card);
}

static void
test_dynamic_tabs (void)
{
  g_autoptr (GtkWidget) tabs = g_object_ref_sink (awra_tab_view_new ());
  GtkWidget *home = gtk_label_new ("Home page");
  GtkWidget *details = gtk_label_new ("Details page");
  GtkWidget *other = gtk_label_new ("Other page");
  g_autoptr (GtkWidget) details_ref = NULL;
  GtkWidget *bar;
  GtkWidget *pinned_bar;
  GtkWidget *viewport_mask;
  GtkWidget *scroller;
  GtkWidget *viewport;
  GtkWidget *strip;
  GtkWidget *home_tab;
  GtkWidget *details_tab;
  GtkWidget *other_tab;
  g_autoptr (GtkWidget) page_icon = g_object_ref_sink (
    gtk_image_new_from_icon_name ("image-x-generic-symbolic"));
  GtkPolicyType horizontal_policy;
  GtkPolicyType vertical_policy;
  TabCloseState state = { 0, TRUE };

  g_signal_connect (tabs, "close-page", G_CALLBACK (tab_close_page_cb),
                    &state);
  g_assert_true (awra_tab_view_append (AWRA_TAB_VIEW (tabs), home,
                                      "Home", TRUE) == home);
  g_assert_true (awra_tab_view_append (AWRA_TAB_VIEW (tabs), details,
                                      "Bulbasaur", FALSE) == details);
  g_assert_true (awra_tab_view_append (AWRA_TAB_VIEW (tabs), other,
                                      "Ivysaur", FALSE) == other);
  details_ref = g_object_ref (details);
  bar = awra_tab_view_get_tab_bar (AWRA_TAB_VIEW (tabs));
  g_assert_true (GTK_IS_BOX (bar));
  g_assert_true (awra_tab_view_get_content_visible (AWRA_TAB_VIEW (tabs)));
  awra_tab_view_set_content_visible (AWRA_TAB_VIEW (tabs), FALSE);
  g_assert_false (awra_tab_view_get_content_visible (AWRA_TAB_VIEW (tabs)));
  awra_tab_view_set_content_visible (AWRA_TAB_VIEW (tabs), TRUE);
  pinned_bar = gtk_widget_get_first_child (bar);
  viewport_mask = gtk_widget_get_next_sibling (pinned_bar);
  scroller = gtk_widget_get_first_child (viewport_mask);
  viewport = gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (scroller));
  strip = GTK_IS_VIEWPORT (viewport)
            ? gtk_viewport_get_child (GTK_VIEWPORT (viewport))
            : viewport;
  home_tab = gtk_widget_get_first_child (pinned_bar);
  details_tab = gtk_widget_get_first_child (strip);
  other_tab = gtk_widget_get_last_child (strip);
  g_assert_nonnull (home_tab);
  g_assert_nonnull (details_tab);
  g_assert_nonnull (other_tab);
  g_assert_true (gtk_widget_has_css_class (viewport_mask,
                                           "awra-tab-viewport-mask"));
  g_assert_true (gtk_widget_get_hexpand (viewport_mask));
  g_assert_true (gtk_widget_get_hexpand (scroller));
  g_assert_true (gtk_widget_get_hexpand (strip));
  g_assert_false (gtk_widget_get_hexpand (details_tab));
  gtk_scrolled_window_get_policy (GTK_SCROLLED_WINDOW (scroller),
                                  &horizontal_policy, &vertical_policy);
  g_assert_cmpint (horizontal_policy, ==, GTK_POLICY_EXTERNAL);
  g_assert_cmpint (vertical_policy, ==, GTK_POLICY_NEVER);

  awra_tab_view_set_page_icon_name (AWRA_TAB_VIEW (tabs), home,
                                    "go-home-symbolic");
  awra_tab_view_set_page_title_visible (AWRA_TAB_VIEW (tabs), home, FALSE);
  {
    GtkWidget *body = gtk_widget_get_first_child (home_tab);
    GtkWidget *select = gtk_widget_get_first_child (body);
    GtkWidget *content = gtk_button_get_child (GTK_BUTTON (select));
    GtkWidget *icon = gtk_widget_get_first_child (content);
    GtkWidget *label = gtk_widget_get_next_sibling (icon);
    GtkWidget *separator = gtk_widget_get_last_child (home_tab);

    g_assert_false (gtk_widget_get_hexpand (select));
    g_assert_true (gtk_widget_get_visible (icon));
    g_assert_false (gtk_widget_get_visible (label));
    g_assert_true (gtk_widget_has_css_class (separator,
                                            "awra-tab-separator"));
    g_assert_false (gtk_widget_has_css_class (
      separator, "awra-tab-separator-hidden"));
  }

  g_assert_false (gtk_widget_has_css_class (
    gtk_widget_get_last_child (details_tab),
    "awra-tab-separator-hidden"));

  awra_tab_view_set_page_title (AWRA_TAB_VIEW (tabs), details,
                                "Ivysaur");
  awra_tab_view_set_page_icon (AWRA_TAB_VIEW (tabs), details,
                               GTK_IMAGE (page_icon));
  {
    GtkWidget *body = gtk_widget_get_first_child (details_tab);
    GtkWidget *select = gtk_widget_get_first_child (body);
    GtkWidget *content = gtk_button_get_child (GTK_BUTTON (select));

    g_assert_true (gtk_widget_get_first_child (content) == page_icon);
    g_assert_true (gtk_widget_get_parent (page_icon) == content);
    g_assert_true (gtk_widget_has_css_class (page_icon, "awra-tab-icon"));
    g_assert_cmpint (gtk_image_get_pixel_size (GTK_IMAGE (page_icon)), ==, 22);
    g_assert_false (gtk_widget_get_can_target (page_icon));
    g_assert_false (gtk_widget_get_focusable (page_icon));
  }
  awra_tab_view_set_page_tooltip (AWRA_TAB_VIEW (tabs), details,
                                  "Item #002");
  g_assert_cmpstr (gtk_widget_get_tooltip_text (details_tab), ==,
                   "Item #002");
  awra_tab_view_set_selected_page (AWRA_TAB_VIEW (tabs), details);
  g_assert_true (awra_tab_view_get_selected_page (AWRA_TAB_VIEW (tabs)) ==
                 details);
  g_assert_true (gtk_widget_has_css_class (details_tab,
                                           "awra-tab-selected"));
  {
    GtkWidget *body = gtk_widget_get_first_child (details_tab);
    GtkWidget *close = gtk_widget_get_next_sibling (
      gtk_widget_get_first_child (body));

    g_assert_true (gtk_widget_has_css_class (close,
                                             "awra-tab-close-visible"));
    g_assert_true (gtk_widget_get_can_target (close));
    g_assert_false (gtk_widget_has_css_class (
      gtk_widget_get_last_child (home_tab),
      "awra-tab-separator-hidden"));
    g_assert_true (gtk_widget_has_css_class (
      gtk_widget_get_last_child (details_tab),
      "awra-tab-separator-hidden"));
  }

  awra_tab_view_close_page (AWRA_TAB_VIEW (tabs), details);
  g_assert_cmpuint (state.requests, ==, 1);
  g_assert_nonnull (gtk_widget_get_parent (details));

  state.veto = FALSE;
  awra_tab_view_close_page (AWRA_TAB_VIEW (tabs), details);
  g_assert_cmpuint (state.requests, ==, 2);
  g_assert_null (gtk_widget_get_parent (details));
  g_assert_true (awra_tab_view_get_selected_page (AWRA_TAB_VIEW (tabs)) ==
                 home);
}

static void
test_dropdown_committed_selection (void)
{
  static const char *choices[] = { "All", "Grass", "Poison", NULL };
  g_autoptr (GtkWidget) dropdown = g_object_ref_sink (
    awra_dropdown_new_from_strings (choices));
  AwraPopover *popover = awra_dropdown_get_popover (
    AWRA_DROPDOWN (dropdown));
  GtkWidget *content = awra_popover_get_child (popover);
  GtkWidget *scroller = gtk_widget_get_first_child (content);
  GtkListView *list = GTK_LIST_VIEW (
    gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (scroller)));
  GtkSelectionModel *popup_selection = gtk_list_view_get_model (list);

  awra_dropdown_set_selected (AWRA_DROPDOWN (dropdown), 0);
  gtk_selection_model_select_item (popup_selection, 1, TRUE);
  g_assert_cmpuint (awra_dropdown_get_selected (AWRA_DROPDOWN (dropdown)),
                    ==, 0);

  g_signal_emit_by_name (list, "activate", 1);
  g_assert_cmpuint (awra_dropdown_get_selected (AWRA_DROPDOWN (dropdown)),
                    ==, 1);
}

static void
test_tab_overflow_geometry (void)
{
  g_autoptr (GtkWidget) tabs = g_object_ref_sink (awra_tab_view_new ());
  GtkWidget *window = gtk_window_new ();
  GtkWidget *home = gtk_label_new ("Home");
  GtkWidget *first_page = NULL;
  GtkWidget *last_page = NULL;
  GtkWidget *bar;
  GtkWidget *pinned_bar;
  GtkWidget *viewport_mask;
  GtkWidget *scroller;
  GtkWidget *viewport;
  GtkWidget *strip;
  GtkWidget *first_tab;
  GtkWidget *second_tab;
  GtkWidget *last_tab;
  GtkWidget *home_select;
  GtkWidget *first_close;
  GtkWidget *last_close;
  GtkAdjustment *adjustment;
  graphene_point_t origin = GRAPHENE_POINT_INIT (0, 0);
  graphene_point_t first_position;
  graphene_point_t second_position;
  graphene_rect_t home_icon_bounds;
  graphene_rect_t home_select_bounds;
  graphene_rect_t first_body_bounds;
  graphene_rect_t second_body_bounds;
  graphene_rect_t last_body_bounds;
  graphene_rect_t content_bounds;
  graphene_rect_t title_bounds;
  graphene_rect_t label_bounds;
  graphene_rect_t selected_label_bounds;

  awra_tab_view_append (AWRA_TAB_VIEW (tabs), home, "Home", TRUE);
  awra_tab_view_set_page_icon_name (AWRA_TAB_VIEW (tabs), home,
                                    "go-home-symbolic");
  awra_tab_view_set_page_title_visible (AWRA_TAB_VIEW (tabs), home, FALSE);
  for (guint i = 0; i < 15; i++) {
    g_autofree char *title = g_strdup_printf ("Document %u", i + 1);
    GtkWidget *page = gtk_label_new (title);
    GtkImage *icon = GTK_IMAGE (gtk_image_new_from_icon_name (
      "image-x-generic-symbolic"));

    awra_tab_view_append (AWRA_TAB_VIEW (tabs), page, title, FALSE);
    awra_tab_view_set_page_icon (AWRA_TAB_VIEW (tabs), page, icon);
    if (first_page == NULL)
      first_page = page;
    last_page = page;
  }

  bar = awra_tab_view_get_tab_bar (AWRA_TAB_VIEW (tabs));
  pinned_bar = gtk_widget_get_first_child (bar);
  viewport_mask = gtk_widget_get_next_sibling (pinned_bar);
  scroller = gtk_widget_get_first_child (viewport_mask);
  viewport = gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (scroller));
  strip = gtk_viewport_get_child (GTK_VIEWPORT (viewport));
  first_tab = gtk_widget_get_first_child (strip);
  second_tab = gtk_widget_get_next_sibling (first_tab);
  last_tab = gtk_widget_get_last_child (strip);

  gtk_window_set_child (GTK_WINDOW (window), tabs);
  gtk_window_set_default_size (GTK_WINDOW (window), 820, 300);
  gtk_widget_set_size_request (window, 820, 300);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);

  adjustment = gtk_scrolled_window_get_hadjustment (
    GTK_SCROLLED_WINDOW (scroller));
  g_assert_cmpint (gtk_widget_get_width (window), <=, 840);
  g_assert_cmpfloat (gtk_adjustment_get_upper (adjustment), >,
                     gtk_adjustment_get_page_size (adjustment));
  g_assert_true (gtk_widget_compute_point (first_tab, strip,
                                           &origin, &first_position));
  g_assert_true (gtk_widget_compute_point (second_tab, strip,
                                           &origin, &second_position));
  g_assert_cmpfloat (second_position.x - first_position.x, >=, 130.0f);
  g_assert_cmpfloat (second_position.x - first_position.x, <=, 134.0f);
  home_select = gtk_widget_get_first_child (gtk_widget_get_first_child (
    gtk_widget_get_first_child (pinned_bar)));
  g_assert_cmpint (gtk_widget_get_width (home_select), ==,
                   gtk_widget_get_height (home_select));
  g_assert_cmpint (gtk_widget_get_width (home_select), ==, 36);
  {
    GtkWidget *content = gtk_button_get_child (GTK_BUTTON (home_select));
    GtkWidget *icon = gtk_widget_get_first_child (content);
    float icon_center_x;
    float icon_center_y;

    g_assert_true (gtk_widget_compute_bounds (icon, home_select,
                                              &home_icon_bounds));
    icon_center_x = home_icon_bounds.origin.x + home_icon_bounds.size.width / 2;
    icon_center_y = home_icon_bounds.origin.y + home_icon_bounds.size.height / 2;
    g_assert_cmpfloat (ABS (icon_center_x -
                            gtk_widget_get_width (home_select) / 2.0f), <=,
                       0.5f);
    g_assert_cmpfloat (ABS (icon_center_y -
                            gtk_widget_get_height (home_select) / 2.0f), <=,
                       0.5f);
  }
  g_assert_true (gtk_widget_compute_bounds (home_select, bar,
                                            &home_select_bounds));
  g_assert_cmpfloat (home_select_bounds.origin.y, <=, 3.0f);
  g_assert_cmpfloat (
    gtk_widget_get_height (bar) -
      (home_select_bounds.origin.y + home_select_bounds.size.height),
    <=, 3.0f);
  {
    GtkWidget *first_body = gtk_widget_get_first_child (first_tab);
    GtkWidget *second_body = gtk_widget_get_first_child (second_tab);
    GtkWidget *first_separator = gtk_widget_get_last_child (first_tab);
    GtkWidget *first_select = gtk_widget_get_first_child (first_body);
    GtkWidget *first_content = gtk_button_get_child (GTK_BUTTON (first_select));
    GtkWidget *first_icon = gtk_widget_get_first_child (first_content);
    GtkWidget *first_title = gtk_widget_get_next_sibling (first_icon);
    GtkWidget *first_label = gtk_widget_get_first_child (first_title);
    g_autoptr (GListModel) controllers = NULL;
    GtkEventController *motion = NULL;
    double scroll_end;

    g_assert_true (gtk_widget_has_css_class (first_body,
                                             "awra-tab-pill"));
    g_assert_true (gtk_widget_has_css_class (first_separator,
                                             "awra-tab-separator"));
    g_assert_true (gtk_widget_get_parent (first_body) == first_tab);
    g_assert_true (gtk_widget_get_parent (first_separator) == first_tab);
    g_assert_true (gtk_widget_compute_bounds (first_body, strip,
                                              &first_body_bounds));
    g_assert_true (gtk_widget_compute_bounds (second_body, strip,
                                              &second_body_bounds));
    g_assert_cmpfloat (
      second_body_bounds.origin.x -
        (first_body_bounds.origin.x + first_body_bounds.size.width),
      >=, 6.0f);

    /* The close affordance is an overlay: revealing it must not move the
     * title away from the geometric centre of the capsule. */
    awra_tab_view_set_page_title (AWRA_TAB_VIEW (tabs), first_page, "One");
    settle_window (window);
    g_assert_true (gtk_widget_compute_bounds (first_content, first_body,
                                              &content_bounds));
    g_assert_true (gtk_widget_compute_bounds (first_label, first_body,
                                              &label_bounds));
    g_assert_cmpfloat (
      ABS ((content_bounds.origin.x + content_bounds.size.width / 2.0f) -
           gtk_widget_get_width (first_body) / 2.0f), <=, 0.5f);

    /* Hovering the neighbour of the selected tab must never make the two
     * painted capsules touch or consume the separator's geometry. */
    awra_tab_view_set_selected_page (AWRA_TAB_VIEW (tabs), first_page);
    gtk_widget_set_state_flags (second_tab, GTK_STATE_FLAG_PRELIGHT, FALSE);
    settle_window (window);
    g_assert_true (gtk_widget_has_css_class (first_tab,
                                             "awra-tab-selected"));
    g_assert_true (gtk_widget_compute_bounds (first_label, first_body,
                                              &selected_label_bounds));
    g_assert_cmpfloat (ABS (selected_label_bounds.origin.x -
                            label_bounds.origin.x), <=, 0.5f);
    g_assert_cmpfloat (ABS (selected_label_bounds.size.width -
                            label_bounds.size.width), <=, 0.5f);
    g_assert_true (gtk_widget_compute_bounds (first_body, strip,
                                              &first_body_bounds));
    g_assert_true (gtk_widget_compute_bounds (second_body, strip,
                                              &second_body_bounds));
    g_assert_cmpfloat (
      second_body_bounds.origin.x -
        (first_body_bounds.origin.x + first_body_bounds.size.width),
      >=, 6.0f);
    gtk_widget_unset_state_flags (second_tab, GTK_STATE_FLAG_PRELIGHT);

    /* Moving the content beneath a stationary pointer generates enter/leave
     * transitions. Those hover updates must never pull the rail back to the
     * selected page after the user has scrolled beyond it. */
    scroll_end = gtk_adjustment_get_upper (adjustment) -
                 gtk_adjustment_get_page_size (adjustment);
    gtk_adjustment_set_value (adjustment, scroll_end);
    controllers = gtk_widget_observe_controllers (second_tab);
    for (guint i = 0; i < g_list_model_get_n_items (controllers); i++) {
      g_autoptr (GObject) controller = g_list_model_get_item (controllers, i);

      if (GTK_IS_EVENT_CONTROLLER_MOTION (controller)) {
        motion = GTK_EVENT_CONTROLLER (controller);
        break;
      }
    }
    g_assert_nonnull (motion);
    g_signal_emit_by_name (motion, "enter", 0.0, 0.0);
    settle_window (window);
    g_assert_cmpfloat (ABS (gtk_adjustment_get_value (adjustment) -
                            scroll_end), <=, 0.5);

    awra_tab_view_set_page_title (
      AWRA_TAB_VIEW (tabs), first_page,
      "A deliberately long document title that must fade");
    settle_window (window);
    g_assert_true (gtk_widget_compute_bounds (first_title, first_body,
                                              &title_bounds));
    g_assert_true (gtk_widget_compute_bounds (first_label, first_title,
                                              &label_bounds));
    g_assert_cmpfloat (label_bounds.size.width, >, title_bounds.size.width);

    awra_tab_view_set_selected_page (AWRA_TAB_VIEW (tabs), home);
    settle_window (window);
  }
  first_close = gtk_widget_get_next_sibling (
    gtk_widget_get_first_child (gtk_widget_get_first_child (first_tab)));
  g_assert_cmpint (gtk_widget_get_width (first_close), ==,
                   gtk_widget_get_height (first_close));
  g_assert_cmpint (gtk_widget_get_width (first_close), ==, 22);
  g_assert_false (gtk_widget_has_css_class (first_close,
                                            "awra-tab-close-visible"));

  awra_tab_view_set_selected_page (AWRA_TAB_VIEW (tabs), last_page);
  settle_window (window);
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), >, 0.0);
  last_close = gtk_widget_get_next_sibling (
    gtk_widget_get_first_child (gtk_widget_get_first_child (last_tab)));
  g_assert_true (gtk_widget_has_css_class (last_close,
                                           "awra-tab-close-visible"));
  g_assert_true (gtk_widget_get_can_target (last_close));
  g_assert_cmpint (gtk_widget_get_width (window), <=, 840);
  {
    GtkWidget *last_body = gtk_widget_get_first_child (last_tab);
    float left_inset;
    float right_inset;

    g_assert_true (gtk_widget_compute_bounds (last_body, bar,
                                              &last_body_bounds));
    left_inset = home_select_bounds.origin.x;
    right_inset = gtk_widget_get_width (bar) -
                  (last_body_bounds.origin.x + last_body_bounds.size.width);
    g_assert_cmpfloat (ABS (left_inset - right_inset), <=, 1.0f);
  }

  gtk_window_set_child (GTK_WINDOW (window), NULL);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_public_material_resolution (void)
{
  AwraContext *context = awra_context_get_for_display (gdk_display_get_default ());
  g_autoptr (AwraMaterial) material = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_CANVAS);
  g_autoptr (AwraMaterialResolution) active =
    awra_context_resolve_material (context, material,
                                   AWRA_SURFACE_ROLE_WINDOW, TRUE, 4);
  g_autoptr (AwraMaterialResolution) inactive =
    awra_context_resolve_material (context, material,
                                   AWRA_SURFACE_ROLE_WINDOW, FALSE, 4);

  g_assert_nonnull (active);
  g_assert_nonnull (inactive);
  g_assert_true (gdk_rgba_equal (awra_material_resolution_get_fill (active),
                                 awra_material_resolution_get_fill (inactive)));
  g_assert_cmpint (awra_material_resolution_get_request_blur (active), ==,
                   awra_material_resolution_get_request_blur (inactive));
  g_assert_true (gdk_rgba_equal (awra_material_resolution_get_border (active),
                                 awra_material_resolution_get_border (inactive)));
  g_assert_true (gdk_rgba_equal (
    awra_material_resolution_get_highlight (active),
    awra_material_resolution_get_highlight (inactive)));
  g_assert_true (gdk_rgba_equal (awra_material_resolution_get_shadow (active),
                                 awra_material_resolution_get_shadow (inactive)));
  g_assert_cmpint (awra_material_resolution_get_border_edges (active), ==,
                   AWRA_SURFACE_EDGE_ALL);
}

static void
test_public_material_tuning (void)
{
  AwraContext *context = awra_context_get_for_display (gdk_display_get_default ());
  AwraStyleManager *style = awra_context_get_style_manager (context);
  AwraAppearance previous_appearance = awra_style_manager_get_appearance (style);
  const GdkRGBA tint = { 0.11, 0.22, 0.33, 0.44 };
  const GdkRGBA floating_tint = { 0.08, 0.16, 0.24, 0.76 };
  g_autoptr (AwraMaterial) canvas = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_CANVAS);
  g_autoptr (AwraMaterial) floating = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_FLOATING);
  g_autoptr (AwraMaterial) floating_second = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_FLOATING);
  g_autoptr (AwraMaterial) chrome = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_CHROME);
  g_autoptr (AwraMaterialResolution) resolved = NULL;

  awra_style_manager_reset_material_tuning (style);
  awra_style_manager_set_appearance (style, AWRA_APPEARANCE_DARK);
  awra_style_manager_set_canvas_tint (style, &tint);
  awra_style_manager_set_grain_scale (style, 2.25);
  awra_style_manager_set_native_blur_enabled (style, FALSE);
  g_assert_true (gdk_rgba_equal (
    awra_style_manager_get_canvas_tint (style), &tint));
  g_assert_cmpfloat (awra_style_manager_get_grain_scale (style), ==, 2.25);
  g_assert_cmpfloat (awra_token_set_get_grain_scale (
                       awra_style_manager_get_token_set (style)), ==, 2.25);
  g_assert_false (awra_style_manager_get_native_blur_enabled (style));

  resolved = awra_context_resolve_material (
    context, canvas, AWRA_SURFACE_ROLE_WINDOW, TRUE, AWRA_ELEVATION_FLAT);
  g_assert_false (awra_material_resolution_get_request_blur (resolved));
  g_assert_true (awra_material_resolution_get_fallback (resolved));
  g_assert_cmpfloat (awra_material_resolution_get_fill (resolved)->red,
                     ==, tint.red);
  g_assert_cmpfloat (awra_material_resolution_get_fill (resolved)->alpha,
                     ==, 1.0);

  awra_style_manager_set_native_blur_enabled (style, TRUE);
  awra_style_manager_set_material_tint (style, floating, &floating_tint);
  awra_style_manager_set_material_radius (style, floating, 23.0);
  awra_style_manager_set_material_grain_scale (style, floating, 0.75);
  awra_style_manager_set_material_outline_strength (style, floating, 0.60);
  awra_style_manager_set_material_highlight_strength (style, floating, 1.30);
  awra_style_manager_set_material_blur_enabled (style, floating, FALSE);
  g_assert_true (gdk_rgba_equal (
    awra_style_manager_get_material_tint (style, floating), &floating_tint));
  g_assert_true (gdk_rgba_equal (
    awra_style_manager_get_material_tint (style, floating_second),
    &floating_tint));
  g_assert_cmpfloat (awra_style_manager_get_material_radius (
                       style, floating), ==, 23.0);
  g_assert_cmpfloat (awra_style_manager_get_material_grain_scale (
                       style, floating), ==, 0.75);
  g_assert_cmpfloat (awra_style_manager_get_material_outline_strength (
                       style, floating), ==, 0.60);
  g_assert_cmpfloat (awra_style_manager_get_material_highlight_strength (
                       style, floating), ==, 1.30);
  g_assert_false (awra_style_manager_get_material_blur_enabled (
    style, floating));
  g_clear_object (&resolved);
  resolved = awra_context_resolve_material (
    context, floating, AWRA_SURFACE_ROLE_FLOATING, TRUE,
    AWRA_ELEVATION_FLOATING);
  g_assert_cmpfloat (awra_material_resolution_get_fill (resolved)->red,
                     ==, floating_tint.red);
  g_assert_cmpfloat (awra_material_resolution_get_radius (resolved), ==, 23.0);
  g_assert_false (awra_material_resolution_get_request_blur (resolved));
  g_assert_true (awra_material_resolution_get_fallback (resolved));

  g_assert_cmpfloat (awra_style_manager_get_material_radius (
                       style, chrome), >, 0.0);
  awra_style_manager_set_material_radius (style, chrome, 21.0);
  g_assert_cmpfloat (awra_style_manager_get_material_radius (
                       style, chrome), ==, 21.0);
  awra_style_manager_reset_material_tuning_for_material (style, floating);
  g_assert_true (awra_style_manager_get_material_blur_enabled (
    style, floating));
  g_assert_false (gdk_rgba_equal (
    awra_style_manager_get_material_tint (style, floating), &floating_tint));

  awra_style_manager_reset_material_tuning (style);
  g_assert_cmpfloat (awra_style_manager_get_grain_scale (style), ==, 0.20);
  g_assert_true (awra_style_manager_get_native_blur_enabled (style));
  g_assert_false (gdk_rgba_equal (
    awra_style_manager_get_canvas_tint (style), &tint));
  awra_style_manager_set_appearance (style, previous_appearance);
}

static void
test_public_typography (void)
{
  g_autoptr (GtkWidget) label = g_object_ref_sink (gtk_label_new ("Heading"));

  awra_widget_set_typography (label, AWRA_TYPOGRAPHY_TITLE_2);
  g_assert_true (gtk_widget_has_css_class (label, "awra-title-2"));
  g_assert_false (gtk_widget_has_css_class (label, "awra-body"));

  awra_widget_set_typography (label, AWRA_TYPOGRAPHY_BODY);
  g_assert_true (gtk_widget_has_css_class (label, "awra-body"));
  g_assert_false (gtk_widget_has_css_class (label, "awra-title-2"));
}

static void
test_badge_appearances (void)
{
  g_autoptr (GtkWidget) window = g_object_ref_sink (gtk_window_new ());
  g_autoptr (GtkWidget) badge = g_object_ref_sink (awra_badge_new ("Fire"));
  g_autoptr (GtkSnapshot) snapshot = NULL;
  g_autoptr (GskRenderNode) node = NULL;
  GdkRGBA custom = { 0.20, 0.72, 0.38, 1.0 };
  int minimum_width;
  int minimum_height;

  g_assert_cmpint (awra_badge_get_appearance (AWRA_BADGE (badge)), ==,
                   AWRA_BADGE_APPEARANCE_ACCENT);
  awra_badge_set_appearance (AWRA_BADGE (badge),
                             AWRA_BADGE_APPEARANCE_WARNING);
  g_assert_cmpint (awra_badge_get_appearance (AWRA_BADGE (badge)), ==,
                   AWRA_BADGE_APPEARANCE_WARNING);
  g_assert_true (gtk_widget_has_css_class (badge, "awra-badge-warning"));
  g_assert_false (gtk_widget_has_css_class (badge, "awra-badge-accent"));
  awra_badge_set_custom_color (AWRA_BADGE (badge), &custom);
  g_assert_true (gdk_rgba_equal (
    awra_badge_get_custom_color (AWRA_BADGE (badge)), &custom));
  g_assert_true (gtk_widget_has_css_class (badge, "awra-badge-custom"));
  g_assert_false (gtk_widget_has_css_class (badge,
                                            "awra-badge-custom-dark-text"));
  awra_badge_set_text (AWRA_BADGE (badge), "GRASS");
  gtk_widget_measure (badge, GTK_ORIENTATION_HORIZONTAL, -1,
                      &minimum_width, NULL, NULL, NULL);
  gtk_widget_measure (badge, GTK_ORIENTATION_VERTICAL, -1,
                      &minimum_height, NULL, NULL, NULL);
  g_assert_cmpint (minimum_width, >=, 48);
  g_assert_cmpint (minimum_height, >=, 22);

  gtk_window_set_child (GTK_WINDOW (window), badge);
  gtk_window_present (GTK_WINDOW (window));
  settle_window (window);
  snapshot = gtk_snapshot_new ();
  GTK_WIDGET_GET_CLASS (badge)->snapshot (badge, snapshot);
  node = gtk_snapshot_to_node (snapshot);
  g_assert_nonnull (node);
  g_assert_false (render_node_contains_type (node, GSK_BORDER_NODE));

  /* A light colour takes dark text. */
  {
    GdkRGBA yellow = { 1.0, 0.9, 0.3, 1.0 };
    GtkWidget *label = gtk_widget_get_first_child (badge);
    PangoAttrIterator *iterator;
    PangoAttrColor *foreground;

    awra_badge_set_custom_color (AWRA_BADGE (badge), &yellow);
    g_assert_true (GTK_IS_LABEL (label));
    iterator = pango_attr_list_get_iterator (gtk_label_get_attributes (GTK_LABEL (label)));
    foreground = (PangoAttrColor *) pango_attr_iterator_get (iterator, PANGO_ATTR_FOREGROUND);
    g_assert_nonnull (foreground);
    g_assert_cmpuint (foreground->color.red, <, 0x8000);
    pango_attr_iterator_destroy (iterator);
  }

  awra_badge_set_custom_color (AWRA_BADGE (badge), NULL);
  g_assert_null (awra_badge_get_custom_color (AWRA_BADGE (badge)));
  g_assert_false (gtk_widget_has_css_class (badge, "awra-badge-custom"));
  gtk_window_set_child (GTK_WINDOW (window), NULL);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_public_style_roles (void)
{
  g_autoptr (GtkWidget) widget = g_object_ref_sink (gtk_box_new (
    GTK_ORIENTATION_HORIZONTAL, 0));

  g_assert_true (awra_widget_add_style_role (widget, "data-root"));
  g_assert_true (gtk_widget_has_css_class (widget, "awra-data-root"));
  g_assert_true (awra_widget_remove_style_role (widget, "data-root"));
  g_assert_false (gtk_widget_has_css_class (widget, "awra-data-root"));

  g_assert_true (awra_widget_add_style_role (widget, "sample-role"));
  g_assert_true (gtk_widget_has_css_class (widget,
                                          "awra-role-sample-role"));
  g_assert_false (gtk_widget_has_css_class (widget, "sample-role"));
  g_assert_true (awra_widget_remove_style_role (widget, "sample-role"));
  g_assert_false (gtk_widget_has_css_class (widget,
                                           "awra-role-sample-role"));

  /* Generic theme class names must never be introduced by public roles. */
  g_assert_true (awra_widget_add_style_role (widget, "sidebar-pane"));
  g_assert_true (gtk_widget_has_css_class (widget,
                                          "awra-role-sidebar-pane"));
  g_assert_false (gtk_widget_has_css_class (widget, "sidebar-pane"));
  g_assert_false (awra_widget_add_style_role (widget, "Invalid role"));
  g_assert_false (awra_widget_add_style_role (widget, ""));
}

static void
test_form_row_prefers_side_by_side (void)
{
  GtkWidget *row = g_object_ref_sink (awra_form_row_new ());
  int minimum = 0;
  int natural = 0;

  awra_form_row_set_label (AWRA_FORM_ROW (row), "Name");
  awra_form_row_set_control (AWRA_FORM_ROW (row), awra_entry_new ());
  /* Side by side is shorter than stacked: a container fitting a height lands
   * on that width, so it has to stay within the natural width. */
  gtk_widget_measure (row, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, &natural, NULL, NULL);
  g_assert_cmpint (natural, >=, 560);
  g_assert_cmpint (minimum, <=, natural);
  g_object_unref (row);
}

static void
test_text_on_pixel_grid (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  GListModel *monitors = gdk_display_get_monitors (display);
  GtkFontRendering rendering = GTK_FONT_RENDERING_AUTOMATIC;
  gboolean dense = g_list_model_get_n_items (monitors) > 0;

  (void) awra_context_get_style_manager (awra_context_get_for_display (display));
  for (guint i = 0; dense && i < g_list_model_get_n_items (monitors); i++) {
    g_autoptr (GdkMonitor) monitor = g_list_model_get_item (monitors, i);

    dense = gdk_monitor_get_scale (monitor) > 1.0;
  }
  g_object_get (gtk_settings_get_for_display (display),
                "gtk-font-rendering", &rendering, NULL);
  /* At scale 1, small text keeps the dots of its i and the tops of its figures. */
  g_assert_cmpint (rendering, ==, dense ? GTK_FONT_RENDERING_AUTOMATIC
                                        : GTK_FONT_RENDERING_MANUAL);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ()) {
    g_test_message ("No display available; skipping GTK widget tests");
    return 77;
  }

  g_test_add_func ("/awra/widgets/constructors", test_constructors);
  g_test_add_func ("/awra/widgets/text-on-pixel-grid", test_text_on_pixel_grid);
  g_test_add_func ("/awra/widgets/builder", test_builder);
  g_test_add_func ("/awra/widgets/controls", test_controls);
  g_test_add_func ("/awra/widgets/layout", test_layout_components);
  g_test_add_func ("/awra/widgets/public-layout-recipes",
                   test_public_layout_recipes);
  g_test_add_func ("/awra/widgets/mapped-header-center-geometry",
                   test_mapped_header_center_geometry);
  g_test_add_func ("/awra/widgets/mapped-header-title-alignment",
                   test_mapped_header_title_alignment);
  g_test_add_func ("/awra/widgets/typed-page-components",
                   test_typed_page_components);
  g_test_add_func ("/awra/widgets/form-and-page-states",
                   test_form_and_page_states);
  g_test_add_func ("/awra/widgets/filter-and-master-detail",
                   test_filter_and_master_detail);
  g_test_add_func ("/awra/widgets/data-view-selection-toolbar",
                   test_data_view_and_selection_toolbar);
  g_test_add_func ("/awra/widgets/advanced-inputs-status",
                   test_advanced_inputs_and_status);
  g_test_add_func ("/awra/widgets/mapped-inset-sidebar",
                   test_mapped_inset_sidebar);
  g_test_add_func ("/awra/widgets/responsive-bin", test_responsive_bin);
  g_test_add_func ("/awra/widgets/breadcrumb-bar", test_breadcrumb_bar);
  g_test_add_func ("/awra/widgets/data-components", test_data_components);
  g_test_add_func ("/awra/widgets/mapped-switch-geometry",
                   test_mapped_switch_geometry);
  g_test_add_func ("/awra/widgets/edge-panel", test_edge_panel);
  g_test_add_func ("/awra/widgets/action-row-context-menu",
                   test_action_row_and_context_menu);
  g_test_add_func ("/awra/widgets/form-alignment-and-control-width",
                   test_form_alignment_and_control_width);
  g_test_add_func ("/awra/widgets/form-row-measured-for-its-layout",
                   test_form_row_measured_for_its_layout);
  g_test_add_func ("/awra/widgets/split-view-keeps-dragged-divider",
                   test_split_view_keeps_dragged_divider);
  g_test_add_func ("/awra/widgets/responsive-widths-in-any-order",
                   test_responsive_widths_in_any_order);
  g_test_add_func ("/awra/widgets/master-detail-measured-for-its-layout",
                   test_master_detail_measured_for_its_layout);
  g_test_add_func ("/awra/widgets/filter-bar-measured-for-its-layout",
                   test_filter_bar_measured_for_its_layout);
  g_test_add_func ("/awra/widgets/navigation-direction",
                   test_navigation_direction);
  g_test_add_func ("/awra/widgets/dialog-responses",
                   test_dialog_responses);
  g_test_add_func ("/awra/widgets/dialog-response-stays",
                   test_dialog_response_stays);
  g_test_add_func ("/awra/widgets/context-menu-released-with-widget",
                   test_context_menu_released_with_widget);
  g_test_add_func ("/awra/widgets/search-entry-activate",
                   test_search_entry_activate);
  g_test_add_func ("/awra/widgets/conversation-edge-status",
                   test_conversation_edge_status);
  g_test_add_func ("/awra/widgets/rail-item-context-menu",
                   test_rail_item_context_menu);
  g_test_add_func ("/awra/widgets/avatar-group",
                   test_avatar_group);
  g_test_add_func ("/awra/widgets/color-palette",
                   test_color_palette);
  g_test_add_func ("/awra/widgets/page-wraps-its-content",
                   test_page_wraps_its_content);
  g_test_add_func ("/awra/widgets/action-row-subtitle-lines",
                   test_action_row_subtitle_lines);
  g_test_add_func ("/awra/widgets/button-busy-and-content",
                   test_button_busy_and_content);
  g_test_add_func ("/awra/widgets/accessibility-semantics",
                   test_accessibility_semantics);
  g_test_add_func ("/awra/widgets/action-row-suffix-receives-clicks",
                   test_action_row_suffix_receives_clicks);
  g_test_add_func ("/awra/widgets/window-popover", test_window_and_popover);
  g_test_add_func ("/awra/widgets/overlays-navigation",
                   test_overlays_and_navigation);
  g_test_add_func ("/awra/widgets/dialog", test_dialog);
  g_test_add_func ("/awra/widgets/navigation-item-details",
                   test_navigation_item_details);
  g_test_add_func ("/awra/widgets/readable-colors",
                   test_readable_colors);
  g_test_add_func ("/awra/widgets/command-palette", test_command_palette);
  g_test_add_func ("/awra/widgets/extended-components",
                   test_extended_components);
  g_test_add_func ("/awra/widgets/dynamic-tabs", test_dynamic_tabs);
  g_test_add_func ("/awra/widgets/tab-overflow-geometry",
                   test_tab_overflow_geometry);
  g_test_add_func ("/awra/widgets/dropdown-committed-selection",
                   test_dropdown_committed_selection);
  g_test_add_func ("/awra/widgets/public-material-resolution",
                   test_public_material_resolution);
  g_test_add_func ("/awra/widgets/public-material-tuning",
                   test_public_material_tuning);
  g_test_add_func ("/awra/widgets/public-typography",
                   test_public_typography);
  g_test_add_func ("/awra/widgets/badge-appearances",
                   test_badge_appearances);
  g_test_add_func ("/awra/widgets/form-row-natural-width",
                   test_form_row_prefers_side_by_side);
  g_test_add_func ("/awra/widgets/public-style-roles",
                   test_public_style_roles);
  return g_test_run ();
}
