/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>
#include <string.h>
#include <math.h>

#define SHOWCASE_PAGE_COUNT 11

typedef struct {
  AwraWindow *window;
  AwraContext *context;
  AwraStyleManager *style;
  AwraDiagnostics *diagnostics;
  AwraToastOverlay *toast;
  AwraNavigationView *navigation;
  AwraHeader *header;
  AwraNavigationItem *first_page_button;
  AwraNavigationItem *page_buttons[SHOWCASE_PAGE_COUNT];
  GtkLabel *platform_label;
  GtkLabel *resolution_label;
  GtkLabel *motion_label;
  GtkLabel *search_result;
  AwraDetailPane *details_pane;
  AwraMasterDetail *overview_master_detail;
  GtkRange *tint_red_slider;
  GtkRange *tint_green_slider;
  GtkRange *tint_blue_slider;
  GtkRange *canvas_opacity_slider;
  GtkRange *grain_slider;
  AwraSwitch *native_blur_switch;
  GtkLabel *material_tuning_label;
  gboolean syncing_material_tuning;
} ShowcaseState;

typedef enum {
  MATERIAL_EDITOR_RED,
  MATERIAL_EDITOR_GREEN,
  MATERIAL_EDITOR_BLUE,
  MATERIAL_EDITOR_ALPHA,
  MATERIAL_EDITOR_RADIUS,
  MATERIAL_EDITOR_GRAIN,
  MATERIAL_EDITOR_OUTLINE,
  MATERIAL_EDITOR_HIGHLIGHT,
} MaterialEditorChannel;

typedef struct {
  ShowcaseState *showcase;
  AwraMaterial *material;
  GtkRange *red;
  GtkRange *green;
  GtkRange *blue;
  GtkRange *alpha;
  GtkRange *radius;
  GtkRange *grain;
  GtkRange *outline;
  GtkRange *highlight;
  AwraSwitch *blur;
  GtkLabel *summary;
  gboolean syncing;
} MaterialEditorState;

typedef struct {
  const char *name;
  const char *title;
  const char *subtitle;
  const char *icon_name;
} PageInfo;

static const PageInfo page_infos[] = {
  { "overview", "Overview", "Component Gallery and complete application patterns", "go-home-symbolic" },
  { "patterns", "Application Patterns", "Complete workspaces composed only from public Awra APIs", "applications-graphics-symbolic" },
  { "surfaces", "Surfaces & Materials", "Semantic materials, nested surfaces and elevation", "view-grid-symbolic" },
  { "controls", "Controls", "Normal, selected, disabled and keyboard-focus states", "preferences-system-symbolic" },
  { "inputs", "Inputs", "Text, search, ranges, choices and IME-ready editing", "input-keyboard-symbolic" },
  { "navigation", "Navigation", "Sidebar routing, tabs and push/pop history", "go-jump-symbolic" },
  { "overlays", "Overlays", "Menus, context actions, popovers, dialog, toast and tooltip", "open-menu-symbolic" },
  { "motion", "Motion", "Semantic timing and reduced-motion behavior", "media-playback-start-symbolic" },
  { "layout", "Layout", "Responsive split views, side panels and floating content", "view-dual-symbolic" },
  { "platform", "Platform & Blur", "Native surfaces, capabilities and effect-region inspection", "video-display-symbolic" },
  { "diagnostics", "Diagnostics", "Tokens and deterministic active/inactive material resolution", "dialog-information-symbolic" },
};

G_STATIC_ASSERT (G_N_ELEMENTS (page_infos) == SHOWCASE_PAGE_COUNT);

static GtkWidget *
make_box (GtkOrientation orientation,
          AwraSpacing    spacing)
{
  GtkWidget *box = gtk_box_new (orientation, 0);

  awra_box_set_spacing (GTK_BOX (box), spacing);
  return box;
}

static GtkWidget *
make_label (const char *text,
            AwraTypography typography)
{
  GtkWidget *label = gtk_label_new (text);

  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  awra_widget_set_typography (label, typography);
  return label;
}

static GtkWidget *
make_page (const char  *title,
           const char  *description,
           GtkWidget  **body_out)
{
  GtkWidget *page = awra_page_new ();
  GtkWidget *body = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_XL);
  GtkWidget *header = awra_page_header_new ();

  awra_page_header_set_title (AWRA_PAGE_HEADER (header), title);
  awra_page_header_set_subtitle (AWRA_PAGE_HEADER (header), description);
  gtk_box_append (GTK_BOX (body), header);
  awra_page_set_child (AWRA_PAGE (page), body);
  *body_out = body;
  return page;
}

static void
append_section (GtkWidget  *body,
                const char *title,
                const char *description)
{
  GtkWidget *section = awra_section_new ();

  awra_section_set_title (AWRA_SECTION (section), title);
  awra_section_set_description (AWRA_SECTION (section), description);
  gtk_box_append (GTK_BOX (body), section);
}

static GtkWidget *
make_card (GtkWidget *child)
{
  GtkWidget *card = awra_card_new ();

  awra_card_set_child (AWRA_CARD (card), child);
  return card;
}

static GtkWidget *
make_plain_group (GtkWidget *child)
{
  GtkWidget *card = awra_card_new ();

  awra_card_set_appearance (AWRA_CARD (card), AWRA_CARD_APPEARANCE_PLAIN);
  awra_card_set_content_inset (AWRA_CARD (card), AWRA_SPACING_SM);
  awra_card_set_child (AWRA_CARD (card), child);
  return card;
}

static void
configure_inset_sidebar (AwraSidebar *sidebar)
{
  awra_sidebar_set_inset_spacing (sidebar, AWRA_SPACING_MD);
  awra_surface_set_elevation_level (awra_sidebar_get_surface (sidebar),
                                    AWRA_ELEVATION_RAISED);
}

static GtkWidget *
make_button (const char           *label,
             AwraButtonAppearance appearance)
{
  GtkWidget *button = awra_button_new_with_label (label);

  awra_button_set_appearance (AWRA_BUTTON (button), appearance);
  return button;
}

static void
show_toast (ShowcaseState *state,
            const char    *message)
{
  awra_toast_overlay_show_message (state->toast, message, 2400);
}

static void
action_activated_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ShowcaseState *state = user_data;
  const char *name = g_action_get_name (G_ACTION (action));
  g_autofree char *message = NULL;

  (void) parameter;
  message = g_strdup_printf ("Action “%s” activated", name);
  show_toast (state, message);
}

static GMenuModel *
make_action_menu (void)
{
  g_autoptr (GMenu) menu = g_menu_new ();
  g_autoptr (GMenu) document = g_menu_new ();
  g_autoptr (GMenu) organization = g_menu_new ();
  g_autoptr (GMenu) more = g_menu_new ();
  g_autoptr (GMenu) danger = g_menu_new ();
  g_autoptr (GMenuItem) item = NULL;
  g_autoptr (GIcon) icon = NULL;

#define APPEND_MENU_ITEM(section, label, action, icon_name) \
  G_STMT_START { \
    item = g_menu_item_new ((label), (action)); \
    icon = g_themed_icon_new ((icon_name)); \
    g_menu_item_set_icon (item, icon); \
    g_menu_append_item ((section), item); \
    g_clear_object (&item); \
    g_clear_object (&icon); \
  } G_STMT_END

  APPEND_MENU_ITEM (document, "Save", "showcase.save",
                    "document-save-symbolic");
  APPEND_MENU_ITEM (document, "Duplicate", "showcase.duplicate",
                    "edit-copy-symbolic");
  APPEND_MENU_ITEM (organization, "Add to favorites", "showcase.favorite",
                    "starred-symbolic");
  APPEND_MENU_ITEM (organization, "Share", "showcase.share",
                    "send-to-symbolic");
  APPEND_MENU_ITEM (more, "Inspect details", "showcase.inspect",
                    "document-properties-symbolic");
  APPEND_MENU_ITEM (more, "Open in new view", "showcase.duplicate",
                    "window-new-symbolic");
  APPEND_MENU_ITEM (danger, "Remove", "showcase.remove",
                    "user-trash-symbolic");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (document));
  g_menu_append_section (menu, NULL, G_MENU_MODEL (organization));
  g_menu_append_submenu (menu, "More", G_MENU_MODEL (more));
  g_menu_append_section (menu, NULL, G_MENU_MODEL (danger));
#undef APPEND_MENU_ITEM
  return G_MENU_MODEL (g_steal_pointer (&menu));
}

static void
appearance_clicked_cb (GtkButton *button,
                       gpointer   user_data)
{
  ShowcaseState *state = user_data;
  AwraAppearance appearance = GPOINTER_TO_INT (
    g_object_get_data (G_OBJECT (button), "appearance"));

  awra_style_manager_set_appearance (state->style, appearance);
}

static GtkWidget *
make_appearance_button (const char       *label,
                        AwraAppearance    appearance,
                        ShowcaseState    *state)
{
  GtkWidget *button = awra_button_new_with_label (label);

  awra_button_set_appearance (AWRA_BUTTON (button),
                              AWRA_BUTTON_APPEARANCE_GHOST);
  g_object_set_data (G_OBJECT (button), "appearance",
                     GINT_TO_POINTER (appearance));
  g_signal_connect (button, "clicked",
                    G_CALLBACK (appearance_clicked_cb), state);
  return button;
}

static void
accent_clicked_cb (GtkButton *button,
                   gpointer   user_data)
{
  ShowcaseState *state = user_data;
  AwraAccentSource source = GPOINTER_TO_INT (
    g_object_get_data (G_OBJECT (button), "accent-source"));

  if (source == AWRA_ACCENT_SOURCE_CUSTOM) {
    const GdkRGBA blue = { 0.19, 0.36, 0.94, 1.0 };

    awra_style_manager_set_accent (state->style, &blue);
  } else {
    awra_style_manager_set_accent_source (state->style, source);
  }
}

static GtkWidget *
make_accent_button (const char       *label,
                    AwraAccentSource source,
                    ShowcaseState   *state)
{
  GtkWidget *button = awra_button_new_with_label (label);

  awra_button_set_appearance (AWRA_BUTTON (button),
                              AWRA_BUTTON_APPEARANCE_GHOST);
  g_object_set_data (G_OBJECT (button), "accent-source",
                     GINT_TO_POINTER (source));
  g_signal_connect (button, "clicked", G_CALLBACK (accent_clicked_cb), state);
  return button;
}

static void
high_contrast_toggled_cb (GtkCheckButton *button,
                          gpointer        user_data)
{
  ShowcaseState *state = user_data;

  awra_style_manager_set_high_contrast (
    state->style, gtk_check_button_get_active (button));
}

static void
reduced_motion_toggled_cb (GtkCheckButton *button,
                           gpointer        user_data)
{
  ShowcaseState *state = user_data;

  awra_style_manager_set_reduced_motion (
    state->style, gtk_check_button_get_active (button));
}

static void
reduced_transparency_toggled_cb (GtkCheckButton *button,
                                 gpointer        user_data)
{
  ShowcaseState *state = user_data;

  awra_style_manager_set_reduced_transparency (
    state->style, gtk_check_button_get_active (button));
}

static GtkWidget *
make_preferences_button (ShowcaseState *state)
{
  GtkWidget *button = awra_menu_button_new ();
  GtkWidget *popover = awra_popover_new ();
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *contrast = awra_check_button_new_with_label ("High contrast");
  GtkWidget *motion = awra_check_button_new_with_label ("Reduced motion");
  GtkWidget *transparency = awra_check_button_new_with_label (
    "Reduced transparency");

  awra_widget_set_margin (box, AWRA_SPACING_LG);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (contrast),
                               awra_style_manager_get_high_contrast (state->style));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (motion),
                               awra_style_manager_get_reduced_motion (state->style));
  gtk_check_button_set_active (
    GTK_CHECK_BUTTON (transparency),
    awra_style_manager_get_reduced_transparency (state->style));
  g_signal_connect (contrast, "toggled",
                    G_CALLBACK (high_contrast_toggled_cb), state);
  g_signal_connect (motion, "toggled",
                    G_CALLBACK (reduced_motion_toggled_cb), state);
  g_signal_connect (transparency, "toggled",
                    G_CALLBACK (reduced_transparency_toggled_cb), state);
  gtk_box_append (GTK_BOX (box), make_label ("Accessibility & effects", AWRA_TYPOGRAPHY_TITLE_3));
  gtk_box_append (GTK_BOX (box), contrast);
  gtk_box_append (GTK_BOX (box), motion);
  gtk_box_append (GTK_BOX (box), transparency);
  gtk_box_append (GTK_BOX (box), make_label ("Accent", AWRA_TYPOGRAPHY_TITLE_3));
  {
    GtkWidget *accents = make_box (GTK_ORIENTATION_HORIZONTAL,
                                   AWRA_SPACING_XS);

    gtk_box_append (GTK_BOX (accents), make_accent_button (
      "Default", AWRA_ACCENT_SOURCE_DEFAULT, state));
    gtk_box_append (GTK_BOX (accents), make_accent_button (
      "System", AWRA_ACCENT_SOURCE_SYSTEM, state));
    gtk_box_append (GTK_BOX (accents), make_accent_button (
      "Blue", AWRA_ACCENT_SOURCE_CUSTOM, state));
    gtk_box_append (GTK_BOX (box), accents);
  }
  awra_popover_set_child (AWRA_POPOVER (popover), box);
  awra_menu_button_set_icon_name (AWRA_MENU_BUTTON (button),
                                  "preferences-system-symbolic");
  awra_menu_button_set_appearance (AWRA_MENU_BUTTON (button),
                                   AWRA_BUTTON_APPEARANCE_TOOLBAR);
  awra_widget_set_tooltip_text (button, "Appearance and accessibility");
  awra_menu_button_set_popover (AWRA_MENU_BUTTON (button),
                                AWRA_POPOVER (popover));
  return button;
}

static void
page_toggled_cb (GtkToggleButton *button,
                 gpointer         user_data)
{
  ShowcaseState *state = user_data;
  const PageInfo *info = g_object_get_data (G_OBJECT (button), "page-info");

  if (!gtk_toggle_button_get_active (button))
    return;
  awra_navigation_view_replace (state->navigation, info->name);
  awra_header_set_title (state->header, info->title);
  awra_header_set_subtitle (state->header, info->subtitle);
}

static GtkWidget *
make_sidebar (ShowcaseState *state)
{
  GtkWidget *sidebar = awra_sidebar_new ();
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *first = NULL;

  awra_widget_set_margin (box, AWRA_SPACING_MD);
  gtk_box_append (GTK_BOX (box), make_label ("AWRA SHOWCASE", AWRA_TYPOGRAPHY_EYEBROW));
  for (guint i = 0; i < G_N_ELEMENTS (page_infos); i++) {
    GtkWidget *button = awra_navigation_item_new_with_label (page_infos[i].title);

    gtk_widget_set_halign (button, GTK_ALIGN_FILL);
    awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (button),
                                        page_infos[i].icon_name);
    g_object_set_data (G_OBJECT (button), "page-info",
                       (gpointer) &page_infos[i]);
    if (first == NULL)
      first = button;
    else
      awra_navigation_item_set_group (AWRA_NAVIGATION_ITEM (button),
                                      AWRA_NAVIGATION_ITEM (first));
    g_signal_connect (button, "toggled", G_CALLBACK (page_toggled_cb), state);
    gtk_box_append (GTK_BOX (box), button);
    state->page_buttons[i] = AWRA_NAVIGATION_ITEM (button);
  }
  state->first_page_button = AWRA_NAVIGATION_ITEM (first);
  awra_sidebar_set_child (AWRA_SIDEBAR (sidebar), box);
  configure_inset_sidebar (AWRA_SIDEBAR (sidebar));
  return sidebar;
}

static void
list_setup_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *item,
               gpointer                  user_data)
{
  GtkWidget *label = make_label (NULL, AWRA_TYPOGRAPHY_BODY);

  (void) factory;
  (void) user_data;
  gtk_list_item_set_child (item, label);
}

static void
list_bind_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *item,
              gpointer                  user_data)
{
  GtkStringObject *object = gtk_list_item_get_item (item);
  GtkWidget *label = gtk_list_item_get_child (item);

  (void) factory;
  (void) user_data;
  gtk_label_set_text (GTK_LABEL (label), gtk_string_object_get_string (object));
}

static void
selection_changed_cb (GtkSelectionModel *model,
                      guint              position,
                      guint              n_items,
                      gpointer           user_data)
{
  ShowcaseState *state = user_data;
  g_autoptr (GObject) selected = NULL;

  (void) position;
  (void) n_items;
  selected = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (model));
  if (selected == NULL)
    return;
  awra_detail_pane_set_title (
    state->details_pane,
    gtk_string_object_get_string (GTK_STRING_OBJECT (selected)));
  awra_master_detail_show_detail (state->overview_master_detail);
}

static void
master_detail_back_cb (GtkButton *button,
                       gpointer   user_data)
{
  ShowcaseState *state = user_data;

  (void) button;
  awra_master_detail_show_master (state->overview_master_detail);
}

static GtkWidget *
make_master_detail_pattern (ShowcaseState *state,
                            GMenuModel    *context_menu)
{
  static const char *items[] = {
    "Design review", "Release checklist", "Material research",
    "Accessibility audit", "KWin validation", NULL
  };
  g_autoptr (GtkStringList) strings = gtk_string_list_new (items);
  g_autoptr (GtkSingleSelection) selection = gtk_single_selection_new (
    G_LIST_MODEL (g_steal_pointer (&strings)));
  g_autoptr (GtkListItemFactory) factory = gtk_signal_list_item_factory_new ();
  GtkWidget *master_detail = awra_master_detail_new ();
  GtkWidget *list = awra_list_view_new (GTK_SELECTION_MODEL (selection), factory);
  GtkWidget *list_scroll = gtk_scrolled_window_new ();
  GtkWidget *master_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *filter_bar = awra_filter_bar_new ();
  GtkWidget *detail_pane = awra_detail_pane_new ();
  GtkWidget *detail_section = awra_section_new ();
  GtkWidget *detail_actions = make_box (GTK_ORIENTATION_HORIZONTAL,
                                        AWRA_SPACING_SM);
  GtkWidget *summary = make_label ("5 documents", AWRA_TYPOGRAPHY_MUTED);
  GtkWidget *back_button;
  GtkWidget *open_button;

  g_signal_connect (factory, "setup", G_CALLBACK (list_setup_cb), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (list_bind_cb), NULL);
  gtk_widget_set_size_request (list_scroll, -1, 260);
  gtk_widget_set_vexpand (list_scroll, TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (list_scroll), list);
  awra_filter_bar_set_search (AWRA_FILTER_BAR (filter_bar),
                              awra_search_entry_new ());
  awra_search_entry_set_placeholder_text (
    AWRA_SEARCH_ENTRY (awra_filter_bar_get_search (
      AWRA_FILTER_BAR (filter_bar))), "Filter workspaces");
  awra_filter_bar_set_summary (AWRA_FILTER_BAR (filter_bar), summary);
  gtk_box_append (GTK_BOX (master_box), filter_bar);
  gtk_box_append (GTK_BOX (master_box), list_scroll);
  awra_detail_pane_set_title (AWRA_DETAIL_PANE (detail_pane), "Design review");
  awra_detail_pane_set_subtitle (AWRA_DETAIL_PANE (detail_pane),
    "A realistic master/detail pattern composed from public Awra and GTK "
    "APIs, with selection and keyboard navigation.");
  awra_section_set_title (AWRA_SECTION (detail_section), "Workspace detail");
  awra_section_set_description (AWRA_SECTION (detail_section),
                                "Selection updates this entity pane without "
                                "manual page rhythm or a bordered card.");
  awra_detail_pane_append_section (AWRA_DETAIL_PANE (detail_pane),
                                   AWRA_SECTION (detail_section));
  back_button = make_button ("Back", AWRA_BUTTON_APPEARANCE_GHOST);
  g_signal_connect (back_button, "clicked",
                    G_CALLBACK (master_detail_back_cb), state);
  open_button = make_button ("Open document",
                             AWRA_BUTTON_APPEARANCE_PRIMARY);
  gtk_box_append (GTK_BOX (detail_actions), back_button);
  gtk_box_append (GTK_BOX (detail_actions), open_button);
  awra_detail_pane_set_actions (AWRA_DETAIL_PANE (detail_pane), detail_actions);
  awra_master_detail_set_master (AWRA_MASTER_DETAIL (master_detail), master_box);
  awra_master_detail_set_detail (AWRA_MASTER_DETAIL (master_detail), detail_pane);
  awra_master_detail_set_master_width (AWRA_MASTER_DETAIL (master_detail), 360);
  gtk_widget_set_size_request (master_detail, -1, 340);
  awra_widget_set_context_menu_model (list, context_menu);
  state->details_pane = AWRA_DETAIL_PANE (detail_pane);
  state->overview_master_detail = AWRA_MASTER_DETAIL (master_detail);
  g_signal_connect (selection, "selection-changed",
                    G_CALLBACK (selection_changed_cb), state);
  gtk_single_selection_set_selected (selection, 0);
  return master_detail;
}

static GtkWidget *
make_overview_page (ShowcaseState *state,
                    GMenuModel    *context_menu)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Every app deserves some Awra.",
    "This is a demonstration and validation application: no business logic, "
    "no application CSS, and no private framework or compositor call.", &body);
  GtkWidget *patterns = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_MD);
  GtkWidget *gallery_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *app_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);

  append_section (body, "Two complementary views",
                  "Browse individual states, then inspect complete application patterns.");
  gtk_box_append (GTK_BOX (gallery_box), make_label (
    "Component Gallery", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (gallery_box), make_label (
    "Controls, inputs, materials, motion and overlays are organized into "
    "focused pages for regression work.", AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (app_box), make_label (
    "Application Patterns", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (app_box), make_label (
    "Master/detail, workspace navigation, settings, responsive panels and "
    "floating actions show how the parts compose.", AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (patterns), make_card (gallery_box));
  gtk_box_append (GTK_BOX (patterns), make_card (app_box));
  gtk_box_set_homogeneous (GTK_BOX (patterns), TRUE);
  gtk_box_append (GTK_BOX (body), patterns);
  append_section (body, "Application Pattern · Research workspace",
                  "A selectable list, contextual actions and a persistent detail panel.");
  gtk_box_append (GTK_BOX (body),
                  make_master_detail_pattern (state, context_menu));
  return page;
}

static void
pattern_clicked_cb (GtkButton *button,
                    gpointer   user_data)
{
  ShowcaseState *state = user_data;
  const char *message = g_object_get_data (G_OBJECT (button),
                                           "pattern-message");

  show_toast (state, message != NULL ? message : "Pattern action activated");
}

static GtkWidget *
make_pattern_action_row (ShowcaseState *state,
                         const char    *title,
                         const char    *subtitle,
                         const char    *icon_name,
                         const char    *suffix,
                         GMenuModel    *context_menu)
{
  GtkWidget *row = awra_action_row_new (title, subtitle);

  awra_action_row_set_icon_name (AWRA_ACTION_ROW (row), icon_name);
  if (suffix != NULL)
    awra_action_row_set_suffix (AWRA_ACTION_ROW (row),
                                awra_badge_new (suffix));
  g_object_set_data (G_OBJECT (row), "pattern-message", (gpointer) title);
  g_signal_connect (row, "clicked", G_CALLBACK (pattern_clicked_cb), state);
  awra_widget_set_context_menu_model (row, context_menu);
  return row;
}


typedef struct {
  AwraMediaRow *row;
  AwraWaveformView *waveform;
  gint64 last_frame;
} PlaybackDemo;

static gboolean
playback_tick_cb (GtkWidget     *widget,
                  GdkFrameClock *clock,
                  gpointer       user_data)
{
  PlaybackDemo *demo = user_data;
  gint64 now = gdk_frame_clock_get_frame_time (clock);
  double duration = awra_media_row_get_duration (demo->row);
  double position;

  (void) widget;
  if (!awra_media_row_get_playing (demo->row) || duration <= 0.0) {
    demo->last_frame = 0;
    return G_SOURCE_CONTINUE;
  }
  if (demo->last_frame == 0 || now - demo->last_frame < 250000) {
    if (demo->last_frame == 0)
      demo->last_frame = now;
    return G_SOURCE_CONTINUE;
  }
  position = awra_media_row_get_position (demo->row) +
             (double) (now - demo->last_frame) / G_USEC_PER_SEC;
  demo->last_frame = now;
  if (position >= duration)
    position = 0.0;
  awra_media_row_set_position (demo->row, position);
  if (demo->waveform != NULL)
    awra_waveform_view_set_position (demo->waveform, position);
  return G_SOURCE_CONTINUE;
}

static void
playback_toggled_cb (AwraMediaRow *row,
                     gpointer      user_data)
{
  (void) user_data;
  awra_media_row_set_playing (row, !awra_media_row_get_playing (row));
}

static void
playback_stop_cb (AwraMediaRow *row,
                  gpointer      user_data)
{
  PlaybackDemo *demo = user_data;

  awra_media_row_set_playing (row, FALSE);
  awra_media_row_set_position (row, 0.0);
  if (demo->waveform != NULL)
    awra_waveform_view_set_position (demo->waveform, 0.0);
}

static void
playback_seek_cb (AwraMediaRow *row,
                  double        position,
                  gpointer      user_data)
{
  PlaybackDemo *demo = user_data;

  awra_media_row_set_position (row, position);
  if (demo->waveform != NULL)
    awra_waveform_view_set_position (demo->waveform, position);
}

static void
waveform_seek_cb (AwraWaveformView *waveform,
                  double            position,
                  gpointer          user_data)
{
  PlaybackDemo *demo = user_data;

  awra_waveform_view_set_position (waveform, position);
  awra_media_row_set_position (demo->row, position);
}

/* Makes a media row behave like a tiny player so the Showcase exercises
 * play/pause, stop, seeking and progress without an audio backend. */
static void
attach_playback_demo (GtkWidget        *row,
                      AwraWaveformView *waveform)
{
  PlaybackDemo *demo = g_new0 (PlaybackDemo, 1);

  demo->row = AWRA_MEDIA_ROW (row);
  demo->waveform = waveform;
  g_object_set_data_full (G_OBJECT (row), "showcase-playback", demo, g_free);
  g_signal_connect (row, "play-toggled", G_CALLBACK (playback_toggled_cb),
                    NULL);
  g_signal_connect (row, "stop-requested", G_CALLBACK (playback_stop_cb),
                    demo);
  g_signal_connect (row, "seek-requested", G_CALLBACK (playback_seek_cb),
                    demo);
  if (waveform != NULL)
    g_signal_connect (waveform, "seek-requested",
                      G_CALLBACK (waveform_seek_cb), demo);
  gtk_widget_add_tick_callback (row, playback_tick_cb, demo, NULL);
}

static float *
make_demo_peaks (guint  count,
                 double seed)
{
  float *peaks = g_new (float, count);

  for (guint i = 0; i < count; i++) {
    double t = (double) i / (double) count;
    double envelope = 0.35 + 0.65 * sin (G_PI * MIN (t * 1.4, 1.0));
    double texture = fabs (sin (i * 0.31 + seed) * cos (i * 0.047 + seed));

    peaks[i] = (float) CLAMP (0.12 + envelope * (0.25 + 0.75 * texture),
                              0.0, 1.0);
  }
  return peaks;
}


static void
divide_tile (GtkButton *button,
             gpointer   user_data);

static GtkWidget *
make_tile (void)
{
  GtkWidget *tile = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *right = gtk_button_new_with_label ("Divide right");
  GtkWidget *below = gtk_button_new_with_label ("Divide below");
  GtkWidget *close = gtk_button_new_with_label ("Close");

  gtk_widget_set_valign (tile, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (tile, 12);
  gtk_widget_set_margin_end (tile, 12);
  g_object_set_data (G_OBJECT (right), "side", GINT_TO_POINTER (GTK_POS_RIGHT));
  g_object_set_data (G_OBJECT (below), "side", GINT_TO_POINTER (GTK_POS_BOTTOM));
  g_signal_connect (right, "clicked", G_CALLBACK (divide_tile), tile);
  g_signal_connect (below, "clicked", G_CALLBACK (divide_tile), tile);
  g_signal_connect (close, "clicked", G_CALLBACK (divide_tile), tile);
  gtk_box_append (GTK_BOX (tile), right);
  gtk_box_append (GTK_BOX (tile), below);
  gtk_box_append (GTK_BOX (tile), close);
  return tile;
}

static void
divide_tile (GtkButton *button,
             gpointer   user_data)
{
  GtkWidget *tile = user_data;
  AwraTileView *tiles = AWRA_TILE_VIEW (gtk_widget_get_parent (tile));
  gpointer side = g_object_get_data (G_OBJECT (button), "side");
  g_autoptr (GList) children = awra_tile_view_get_children (tiles);

  if (side != NULL)
    awra_tile_view_insert (tiles, make_tile (), tile, GPOINTER_TO_INT (side));
  else if (children->next != NULL)
    awra_tile_view_remove (tiles, tile);
}

static GtkWidget *
make_tiles (void)
{
  GtkWidget *tiles = awra_tile_view_new ();

  gtk_widget_set_size_request (tiles, -1, 280);
  awra_tile_view_insert (AWRA_TILE_VIEW (tiles), make_tile (), NULL, GTK_POS_LEFT);
  return tiles;
}

static GtkWidget *
make_waveform (double duration,
               double seed,
               int    height)
{
  GtkWidget *waveform = awra_waveform_view_new ();
  g_autofree float *peaks = make_demo_peaks (240, seed);

  awra_waveform_view_set_peaks (AWRA_WAVEFORM_VIEW (waveform), peaks, 240);
  awra_waveform_view_set_duration (AWRA_WAVEFORM_VIEW (waveform), duration);
  gtk_widget_set_size_request (waveform, -1, height);
  return waveform;
}

static GtkWidget *
make_media_workspace (ShowcaseState *state,
                      GMenuModel    *context_menu)
{
  GtkWidget *frame = awra_card_new ();
  GtkWidget *root = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_NONE);
  GtkWidget *toolbar = awra_toolbar_new ();
  GtkWidget *toolbar_title = make_label ("Music", AWRA_TYPOGRAPHY_TITLE_3);
  GtkWidget *search = awra_search_entry_new ();
  GtkWidget *menu = awra_menu_button_new ();
  GtkWidget *split = awra_split_view_new ();
  GtkWidget *sidebar = awra_sidebar_new ();
  GtkWidget *rail = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_XS);
  GtkWidget *home = awra_navigation_item_new_with_label ("Home");
  GtkWidget *discover = awra_navigation_item_new_with_label ("Discover");
  GtkWidget *radio = awra_navigation_item_new_with_label ("Radio");
  GtkWidget *library = awra_navigation_item_new_with_label ("Library");
  GtkWidget *content = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *hero = awra_card_new ();
  GtkWidget *hero_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *hero_actions = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *waveform = make_waveform (222.0, 0.7, 44);
  GtkWidget *now_playing = awra_media_row_new ("La Lune");
  GtkWidget *queue = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_XS);
  GdkRGBA swatch = { 0.42, 0.30, 0.86, 1.0 };

  (void) state;
  awra_card_set_appearance (AWRA_CARD (frame), AWRA_CARD_APPEARANCE_RAISED);
  awra_card_set_content_inset (AWRA_CARD (frame), AWRA_SPACING_NONE);
  awra_card_set_child (AWRA_CARD (frame), root);
  gtk_widget_set_size_request (frame, -1, 640);
  awra_widget_set_margin (now_playing, AWRA_SPACING_SM);

  awra_search_entry_set_placeholder_text (AWRA_SEARCH_ENTRY (search),
                                          "Search songs, artists and albums");
  gtk_widget_set_hexpand (search, TRUE);
  gtk_widget_set_size_request (search, 280, -1);
  awra_menu_button_set_icon_name (AWRA_MENU_BUTTON (menu),
                                  "open-menu-symbolic");
  awra_menu_button_set_appearance (AWRA_MENU_BUTTON (menu),
                                   AWRA_BUTTON_APPEARANCE_TOOLBAR);
  awra_menu_button_set_menu_model (AWRA_MENU_BUTTON (menu), context_menu);
  awra_widget_set_tooltip_text (menu, "Workspace actions");
  awra_toolbar_set_start_widget (AWRA_TOOLBAR (toolbar), toolbar_title);
  awra_toolbar_set_center_widget (AWRA_TOOLBAR (toolbar), search);
  awra_toolbar_set_end_widget (AWRA_TOOLBAR (toolbar), menu);
  gtk_box_append (GTK_BOX (root), toolbar);

  awra_widget_set_margin (rail, AWRA_SPACING_MD);
  gtk_box_append (GTK_BOX (rail), make_label ("BROWSE",
                                             AWRA_TYPOGRAPHY_EYEBROW));
  awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (home),
                                      "go-home-symbolic");
  awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (discover),
                                      "find-location-symbolic");
  awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (radio),
                                      "media-optical-symbolic");
  awra_navigation_item_set_icon_name (AWRA_NAVIGATION_ITEM (library),
                                      "folder-music-symbolic");
  awra_navigation_item_set_group (AWRA_NAVIGATION_ITEM (discover),
                                  AWRA_NAVIGATION_ITEM (home));
  awra_navigation_item_set_group (AWRA_NAVIGATION_ITEM (radio),
                                  AWRA_NAVIGATION_ITEM (home));
  awra_navigation_item_set_group (AWRA_NAVIGATION_ITEM (library),
                                  AWRA_NAVIGATION_ITEM (home));
  awra_navigation_item_set_selected (AWRA_NAVIGATION_ITEM (home), TRUE);
  gtk_box_append (GTK_BOX (rail), home);
  gtk_box_append (GTK_BOX (rail), discover);
  gtk_box_append (GTK_BOX (rail), radio);
  gtk_box_append (GTK_BOX (rail), library);
  awra_sidebar_set_child (AWRA_SIDEBAR (sidebar), rail);
  configure_inset_sidebar (AWRA_SIDEBAR (sidebar));

  awra_widget_set_margin (content, AWRA_SPACING_XL);
  gtk_box_append (GTK_BOX (content), make_label ("Good evening",
                                                AWRA_TYPOGRAPHY_DISPLAY));
  gtk_box_append (GTK_BOX (content), make_label (
    "A quiet media workspace with chrome, content and one emphasized layer.",
    AWRA_TYPOGRAPHY_MUTED));
  awra_card_set_appearance (AWRA_CARD (hero), AWRA_CARD_APPEARANCE_RAISED);
  awra_card_set_content_inset (AWRA_CARD (hero), AWRA_SPACING_XL);
  gtk_box_append (GTK_BOX (hero_box), make_label ("READY TO PLAY",
                                                 AWRA_TYPOGRAPHY_EYEBROW));
  gtk_box_append (GTK_BOX (hero_box), make_label ("La Lune",
                                                 AWRA_TYPOGRAPHY_TITLE_1));
  gtk_box_append (GTK_BOX (hero_box), make_label (
    "Noirmate · Start a focused radio queue from this track.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (hero_actions), make_button (
    "Play", AWRA_BUTTON_APPEARANCE_PRIMARY));
  gtk_box_append (GTK_BOX (hero_actions), make_button (
    "Add to library", AWRA_BUTTON_APPEARANCE_SECONDARY));
  gtk_box_append (GTK_BOX (hero_box), hero_actions);
  awra_media_row_set_subtitle (AWRA_MEDIA_ROW (now_playing), "Noirmate · La Lune");
  awra_media_row_set_duration (AWRA_MEDIA_ROW (now_playing), 222.0);
  awra_media_row_set_position (AWRA_MEDIA_ROW (now_playing), 96.0);
  awra_media_row_set_playing (AWRA_MEDIA_ROW (now_playing), TRUE);
  awra_media_row_set_show_volume (AWRA_MEDIA_ROW (now_playing), TRUE);
  awra_media_row_set_volume (AWRA_MEDIA_ROW (now_playing), 0.72);
  awra_media_row_set_icon_name (AWRA_MEDIA_ROW (now_playing),
                                "audio-x-generic-symbolic");
  awra_media_row_set_accent_color (AWRA_MEDIA_ROW (now_playing), &swatch);
  awra_waveform_view_set_position (AWRA_WAVEFORM_VIEW (waveform), 96.0);
  attach_playback_demo (now_playing, AWRA_WAVEFORM_VIEW (waveform));
  gtk_box_append (GTK_BOX (hero_box), waveform);
  awra_card_set_child (AWRA_CARD (hero), hero_box);
  awra_widget_set_context_menu_model (hero, context_menu);
  gtk_box_append (GTK_BOX (content), hero);
  gtk_box_append (GTK_BOX (queue), make_label ("UP NEXT",
                                              AWRA_TYPOGRAPHY_EYEBROW));
  {
    static const struct {
      const char *title;
      const char *subtitle;
      double duration;
    } tracks[] = {
      { "Silver Lines", "Mira", 248.0 },
      { "Afterglow", "Noirmate", 222.0 },
    };

    for (guint i = 0; i < G_N_ELEMENTS (tracks); i++) {
      GtkWidget *row = awra_media_row_new (tracks[i].title);

      awra_media_row_set_subtitle (AWRA_MEDIA_ROW (row), tracks[i].subtitle);
      awra_media_row_set_duration (AWRA_MEDIA_ROW (row), tracks[i].duration);
      awra_media_row_set_icon_name (AWRA_MEDIA_ROW (row),
                                    "audio-x-generic-symbolic");
      awra_media_row_set_show_stop_button (AWRA_MEDIA_ROW (row), FALSE);
      attach_playback_demo (row, NULL);
      awra_widget_set_context_menu_model (row, context_menu);
      gtk_box_append (GTK_BOX (queue), row);
    }
  }
  gtk_box_append (GTK_BOX (content), queue);
  awra_split_view_set_sidebar (AWRA_SPLIT_VIEW (split), sidebar);
  awra_split_view_set_content (AWRA_SPLIT_VIEW (split), content);
  gtk_widget_set_vexpand (split, TRUE);
  gtk_box_append (GTK_BOX (root), split);
  /* The transport docks below the workspace, like a desktop player bar. */
  gtk_box_append (GTK_BOX (root), now_playing);
  return frame;
}


typedef struct {
  ShowcaseState *showcase;
  AwraKeyboardView *keyboard;
  AwraMediaRow *row;
  AwraWaveformView *waveform;
  GtkLabel *selection_label;
} SoundboardDemo;

typedef struct {
  const char *code;
  const char *sound;
  double duration;
  GdkRGBA track;
  gboolean has_mood;
  GdkRGBA mood;
  const char *badge;
} SoundboardBinding;

static const SoundboardBinding soundboard_bindings[] = {
  { "KeyA", "Thunder", 6.0, { 0.25, 0.42, 0.86, 1.0 }, TRUE, { 0.55, 0.36, 0.92, 1.0 }, NULL },
  { "KeyS", "Battle theme", 214.0, { 0.72, 0.20, 0.26, 1.0 }, TRUE, { 0.96, 0.42, 0.20, 1.0 }, "2" },
  { "KeyD", "Footsteps", 3.5, { 0.25, 0.42, 0.86, 1.0 }, FALSE, { 0, 0, 0, 0 }, NULL },
  { "KeyF", "Sword clash", 1.8, { 0.25, 0.42, 0.86, 1.0 }, TRUE, { 0.96, 0.42, 0.20, 1.0 }, NULL },
  { "KeyJ", "Rain loop", 95.0, { 0.16, 0.56, 0.48, 1.0 }, TRUE, { 0.36, 0.72, 0.90, 1.0 }, NULL },
  { "KeyK", "Village", 180.0, { 0.16, 0.56, 0.48, 1.0 }, FALSE, { 0, 0, 0, 0 }, "3" },
  { "Space", "Silence the room", 2.0, { 0.48, 0.46, 0.52, 1.0 }, FALSE, { 0, 0, 0, 0 }, NULL },
  { "Digit1", "Opening", 132.0, { 0.72, 0.20, 0.26, 1.0 }, FALSE, { 0, 0, 0, 0 }, NULL },
  { "Numpad5", "Heartbeat", 12.0, { 0.84, 0.66, 0.22, 1.0 }, TRUE, { 0.86, 0.22, 0.30, 1.0 }, NULL },
};

static const SoundboardBinding *
soundboard_binding (const char *code)
{
  for (guint i = 0; code != NULL && i < G_N_ELEMENTS (soundboard_bindings); i++) {
    if (g_str_equal (soundboard_bindings[i].code, code))
      return &soundboard_bindings[i];
  }
  return NULL;
}

static gboolean
soundboard_stop_active_cb (gpointer user_data)
{
  AwraKeyCap *cap = user_data;

  awra_key_cap_set_active (cap, FALSE);
  return G_SOURCE_REMOVE;
}

static void
soundboard_selection_changed_cb (AwraKeyboardView *keyboard,
                                 SoundboardDemo   *demo)
{
  g_auto (GStrv) codes = awra_keyboard_view_get_selected_codes (keyboard);
  const SoundboardBinding *binding = codes[0] != NULL
                                       ? soundboard_binding (codes[0]) : NULL;
  g_autofree char *summary = NULL;

  if (codes[0] == NULL) {
    summary = g_strdup ("No key selected");
  } else if (codes[1] != NULL) {
    summary = g_strdup_printf ("%u keys selected", g_strv_length (codes));
  } else {
    AwraKeyCap *cap = awra_keyboard_view_get_key_cap (keyboard, codes[0]);

    summary = g_strdup_printf ("Key %s · %s", awra_key_cap_get_label (cap),
                               binding != NULL ? binding->sound : "free");
  }
  gtk_label_set_text (demo->selection_label, summary);
  awra_media_row_set_title (demo->row, binding != NULL ? binding->sound
                                                       : "Nothing assigned");
  awra_media_row_set_subtitle (demo->row, binding != NULL ? codes[0] : NULL);
  awra_media_row_set_duration (demo->row, binding != NULL ? binding->duration
                                                          : 0.0);
  awra_media_row_set_position (demo->row, 0.0);
  awra_media_row_set_playing (demo->row, FALSE);
  awra_media_row_set_accent_color (demo->row,
                                   binding != NULL ? &binding->track : NULL);
  awra_waveform_view_set_duration (demo->waveform,
                                   binding != NULL ? binding->duration : 0.0);
  awra_waveform_view_set_marker_position (demo->waveform,
                                          binding != NULL
                                            ? binding->duration * 0.12 : -1.0);
  awra_waveform_view_set_hint_position (demo->waveform,
                                        binding != NULL
                                          ? binding->duration * 0.31 : -1.0);
  awra_waveform_view_set_position (demo->waveform, -1.0);
  if (binding != NULL) {
    g_autofree float *peaks = make_demo_peaks (240, binding->duration);

    awra_waveform_view_set_peaks (demo->waveform, peaks, 240);
  } else {
    awra_waveform_view_set_peaks (demo->waveform, NULL, 0);
  }
}

static void
soundboard_key_activated_cb (AwraKeyboardView *keyboard,
                             const char       *code,
                             GdkModifierType   modifiers,
                             SoundboardDemo   *demo)
{
  AwraKeyCap *cap = awra_keyboard_view_get_key_cap (keyboard, code);

  (void) modifiers;
  if (soundboard_binding (code) == NULL)
    return;
  awra_key_cap_flash (cap);
  awra_key_cap_set_active (cap, TRUE);
  g_timeout_add_full (G_PRIORITY_DEFAULT, 1600, soundboard_stop_active_cb,
                      g_object_ref (cap), g_object_unref);
  awra_media_row_set_playing (demo->row, TRUE);
}

static void
soundboard_files_dropped_cb (AwraKeyboardView *keyboard,
                             const char       *code,
                             GdkFileList      *files,
                             SoundboardDemo   *demo)
{
  g_autoptr (GSList) list = gdk_file_list_get_files (files);
  AwraKeyCap *cap = awra_keyboard_view_get_key_cap (keyboard, code);
  g_autofree char *message = g_strdup_printf ("%u file(s) assigned to %s",
                                              g_slist_length (list),
                                              awra_key_cap_get_label (cap));

  awra_key_cap_set_bound (cap, TRUE);
  awra_key_cap_set_secondary_label (cap, "Dropped sound");
  awra_toast_overlay_show_with_action (demo->showcase->toast, message, "Undo",
                                       "showcase.undo", 4000);
}

static void
soundboard_layout_cb (AwraSegmentedControl *control,
                      GParamSpec           *pspec,
                      SoundboardDemo       *demo)
{
  (void) pspec;
  awra_keyboard_view_set_physical_layout (
    demo->keyboard,
    awra_segmented_control_get_selected (control) == 1
      ? AWRA_KEYBOARD_PHYSICAL_LAYOUT_ISO
      : AWRA_KEYBOARD_PHYSICAL_LAYOUT_ANSI);
}

static void
soundboard_numpad_cb (AwraSwitch     *switcher,
                      GParamSpec     *pspec,
                      SoundboardDemo *demo)
{
  (void) pspec;
  awra_keyboard_view_set_show_numpad (demo->keyboard,
                                      awra_switch_get_active (switcher));
}

static void
soundboard_shortcut_captured_cb (AwraShortcutCapture *capture,
                                 SoundboardDemo      *demo)
{
  g_auto (GStrv) codes = awra_shortcut_capture_get_codes (capture);
  g_autofree char *joined = g_strjoinv (" + ", codes);
  g_autofree char *message = g_strdup_printf ("Stop-all shortcut: %s", joined);

  show_toast (demo->showcase, message);
}

static GtkWidget *
make_soundboard_workspace (ShowcaseState *state,
                           GMenuModel    *context_menu)
{
  SoundboardDemo *demo = g_new0 (SoundboardDemo, 1);
  GtkWidget *frame = awra_card_new ();
  GtkWidget *root = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_LG);
  GtkWidget *toolbar = awra_toolbar_new ();
  GtkWidget *tools = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *layout = awra_segmented_control_new ();
  GtkWidget *numpad = awra_switch_new ();
  GtkWidget *stop_all = awra_shortcut_capture_new ();
  GtkWidget *keyboard = awra_keyboard_view_new ();
  GtkWidget *details = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *waveform = make_waveform (0.0, 0.0, 52);
  GtkWidget *row = awra_media_row_new (NULL);
  const guint stop_keys[] = { 37, 9 }; /* Left Ctrl + Escape */
  AwraKeyboardView *view = AWRA_KEYBOARD_VIEW (keyboard);

  demo->showcase = state;
  demo->keyboard = view;
  demo->row = AWRA_MEDIA_ROW (row);
  demo->waveform = AWRA_WAVEFORM_VIEW (waveform);
  demo->selection_label = GTK_LABEL (make_label ("No key selected",
                                                 AWRA_TYPOGRAPHY_MUTED));
  g_object_set_data_full (G_OBJECT (frame), "showcase-soundboard", demo,
                          g_free);

  awra_card_set_appearance (AWRA_CARD (frame), AWRA_CARD_APPEARANCE_RAISED);
  awra_toolbar_set_start_widget (AWRA_TOOLBAR (toolbar),
                                 make_label ("Soundboard",
                                             AWRA_TYPOGRAPHY_TITLE_2));
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (layout), "ANSI");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (layout), "ISO");
  awra_segmented_control_set_selected (AWRA_SEGMENTED_CONTROL (layout), 0);
  awra_widget_set_tooltip_text (numpad, "Show the numeric keypad");
  awra_shortcut_capture_set_placeholder (AWRA_SHORTCUT_CAPTURE (stop_all),
                                         "Stop all…");
  awra_shortcut_capture_set_shortcut (AWRA_SHORTCUT_CAPTURE (stop_all),
                                      stop_keys, G_N_ELEMENTS (stop_keys),
                                      GDK_CONTROL_MASK);
  awra_widget_set_tooltip_text (stop_all,
                                "Click, then press a new stop-all shortcut");
  gtk_box_append (GTK_BOX (tools), layout);
  gtk_box_append (GTK_BOX (tools), make_label ("Numpad",
                                              AWRA_TYPOGRAPHY_MUTED));
  gtk_box_append (GTK_BOX (tools), numpad);
  gtk_box_append (GTK_BOX (tools), stop_all);
  awra_toolbar_set_end_widget (AWRA_TOOLBAR (toolbar), tools);
  gtk_box_append (GTK_BOX (root), toolbar);

  awra_keyboard_view_set_selection_mode (view, GTK_SELECTION_MULTIPLE);
  awra_keyboard_view_set_accepts_file_drops (view, TRUE);
  /* Configure every bound cap, including the hidden numpad section. */
  awra_keyboard_view_set_show_numpad (view, TRUE);
  for (guint i = 0; i < G_N_ELEMENTS (soundboard_bindings); i++) {
    const SoundboardBinding *binding = &soundboard_bindings[i];
    AwraKeyCap *cap = awra_keyboard_view_get_key_cap (view, binding->code);

    awra_key_cap_set_bound (cap, TRUE);
    awra_key_cap_set_secondary_label (cap, binding->sound);
    awra_key_cap_set_tint (cap, &binding->track);
    if (binding->has_mood)
      awra_key_cap_set_edge_color (cap, &binding->mood);
    awra_key_cap_set_badge (cap, binding->badge);
    awra_widget_set_context_menu_model (GTK_WIDGET (cap), context_menu);
  }
  awra_keyboard_view_set_show_numpad (view, FALSE);
  awra_key_cap_set_dimmed (awra_keyboard_view_get_key_cap (view, "F12"), TRUE);
  gtk_box_append (GTK_BOX (root), keyboard);

  gtk_box_append (GTK_BOX (details), GTK_WIDGET (demo->selection_label));
  awra_waveform_view_set_marker_editable (AWRA_WAVEFORM_VIEW (waveform), TRUE);
  awra_media_row_set_show_volume (AWRA_MEDIA_ROW (row), TRUE);
  attach_playback_demo (row, AWRA_WAVEFORM_VIEW (waveform));
  gtk_box_append (GTK_BOX (details), waveform);
  gtk_box_append (GTK_BOX (details), row);
  gtk_box_append (GTK_BOX (root), details);
  awra_card_set_child (AWRA_CARD (frame), root);

  g_signal_connect (keyboard, "selection-changed",
                    G_CALLBACK (soundboard_selection_changed_cb), demo);
  g_signal_connect (keyboard, "key-activated",
                    G_CALLBACK (soundboard_key_activated_cb), demo);
  g_signal_connect (keyboard, "files-dropped",
                    G_CALLBACK (soundboard_files_dropped_cb), demo);
  g_signal_connect (layout, "notify::selected",
                    G_CALLBACK (soundboard_layout_cb), demo);
  g_signal_connect (numpad, "notify::active",
                    G_CALLBACK (soundboard_numpad_cb), demo);
  g_signal_connect (stop_all, "captured",
                    G_CALLBACK (soundboard_shortcut_captured_cb), demo);
  awra_keyboard_view_set_selected_codes (view,
                                         (const char *[]) { "KeyS", NULL });
  return frame;
}

static GtkWidget *
make_launcher_tile (ShowcaseState *state,
                    const char    *title,
                    const char    *subtitle,
                    const char    *icon_name,
                    gboolean       raised,
                    GMenuModel    *context_menu)
{
  GtkWidget *card = awra_card_new ();
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *icon = gtk_image_new_from_icon_name (icon_name);
  GtkWidget *button = make_button ("Launch", AWRA_BUTTON_APPEARANCE_GHOST);

  awra_card_set_appearance (AWRA_CARD (card), raised
    ? AWRA_CARD_APPEARANCE_RAISED : AWRA_CARD_APPEARANCE_TINTED);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 30);
  gtk_widget_set_halign (icon, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), icon);
  gtk_box_append (GTK_BOX (box), make_label (title, AWRA_TYPOGRAPHY_TITLE_3));
  gtk_box_append (GTK_BOX (box), make_label (subtitle,
                                            AWRA_TYPOGRAPHY_MUTED));
  gtk_box_append (GTK_BOX (box), button);
  awra_card_set_child (AWRA_CARD (card), box);
  gtk_widget_set_hexpand (card, TRUE);
  g_object_set_data (G_OBJECT (button), "pattern-message", (gpointer) title);
  g_signal_connect (button, "clicked", G_CALLBACK (pattern_clicked_cb), state);
  awra_widget_set_context_menu_model (card, context_menu);
  return card;
}

static GtkWidget *
make_launcher_workspace (ShowcaseState *state,
                         GMenuModel    *context_menu)
{
  GtkWidget *frame = awra_card_new ();
  GtkWidget *root = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_LG);
  GtkWidget *toolbar = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *search = awra_search_entry_new ();
  GtkWidget *add = awra_button_new_from_icon_name ("list-add-symbolic");
  GtkWidget *settings = awra_menu_button_new ();
  GtkWidget *grid = gtk_grid_new ();
  static const struct {
    const char *title;
    const char *subtitle;
    const char *icon;
  } tiles[] = {
    { "Rocket League", "Last opened today", "applications-games-symbolic" },
    { "Catalogue", "Reference workspace", "system-search-symbolic" },
    { "Soundboard", "Audio automation", "audio-card-symbolic" },
    { "SupaCodex", "Development tools", "utilities-terminal-symbolic" },
    { "Studio", "Creative session", "applications-multimedia-symbolic" },
    { "Archive", "12 saved presets", "folder-documents-symbolic" },
  };

  awra_card_set_appearance (AWRA_CARD (frame), AWRA_CARD_APPEARANCE_RAISED);
  gtk_box_append (GTK_BOX (toolbar), make_label ("Personal Launcher",
                                                AWRA_TYPOGRAPHY_TITLE_2));
  awra_search_entry_set_placeholder_text (AWRA_SEARCH_ENTRY (search),
                                          "Search environments");
  gtk_widget_set_hexpand (search, TRUE);
  gtk_box_append (GTK_BOX (toolbar), search);
  awra_button_set_appearance (AWRA_BUTTON (add),
                              AWRA_BUTTON_APPEARANCE_TOOLBAR);
  awra_widget_set_tooltip_text (add, "Add environment");
  g_object_set_data (G_OBJECT (add), "pattern-message", "New environment");
  g_signal_connect (add, "clicked", G_CALLBACK (pattern_clicked_cb), state);
  gtk_box_append (GTK_BOX (toolbar), add);
  awra_menu_button_set_icon_name (AWRA_MENU_BUTTON (settings),
                                  "preferences-system-symbolic");
  awra_menu_button_set_appearance (AWRA_MENU_BUTTON (settings),
                                   AWRA_BUTTON_APPEARANCE_TOOLBAR);
  awra_menu_button_set_menu_model (AWRA_MENU_BUTTON (settings), context_menu);
  gtk_box_append (GTK_BOX (toolbar), settings);
  gtk_box_append (GTK_BOX (root), toolbar);
  awra_grid_set_spacing (GTK_GRID (grid), AWRA_SPACING_MD,
                         AWRA_SPACING_MD);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
  for (guint i = 0; i < G_N_ELEMENTS (tiles); i++)
    gtk_grid_attach (GTK_GRID (grid), make_launcher_tile (
      state, tiles[i].title, tiles[i].subtitle, tiles[i].icon,
      i == 0 || i == 3, context_menu), i % 3, i / 3, 1, 1);
  gtk_box_append (GTK_BOX (root), grid);
  awra_card_set_child (AWRA_CARD (frame), root);
  return frame;
}

static void
edge_panel_toggle_cb (GtkButton *button,
                      gpointer   user_data)
{
  AwraEdgePanel *panel = user_data;

  (void) button;
  awra_edge_panel_set_revealed (panel,
                                !awra_edge_panel_get_revealed (panel));
}

typedef struct {
  ShowcaseState *state;
  AwraRail *rail;
  GtkLabel *channel;
  AwraTypingIndicator *typing;
  AwraAvatar *speaker;
  AwraRailItem *busy_item;
  AwraChip *party;
  guint tick;
} MessagingDemo;

static void
messaging_reaction_toggled_cb (AwraChip *chip,
                               gpointer  user_data)
{
  (void) user_data;
  awra_chip_set_count (chip, awra_chip_get_count (chip) +
                               (awra_chip_get_active (chip) ? 1 : -1));
}

static void
messaging_selected_cb (AwraRail   *rail,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  MessagingDemo *demo = user_data;
  AwraRailItem *item = awra_rail_get_selected_item (rail);
  g_autofree char *title = NULL;

  (void) pspec;
  if (item == NULL)
    return;
  title = g_strdup_printf ("# %s", awra_rail_item_get_label (item));
  gtk_label_set_text (demo->channel, title);
  awra_rail_item_set_unread (item, FALSE);
  awra_rail_item_set_mention_count (item, 0);
}

static void
messaging_item_moved_cb (AwraRail  *rail,
                         GtkWidget *item,
                         guint      position,
                         gpointer   user_data)
{
  MessagingDemo *demo = user_data;
  g_autofree char *message = NULL;

  (void) rail;
  message = g_strdup_printf ("Moved to position %u", position + 1);
  (void) item;
  show_toast (demo->state, message);
}

static void
messaging_item_merged_cb (AwraRail  *rail,
                          GtkWidget *item,
                          GtkWidget *onto,
                          gpointer   user_data)
{
  MessagingDemo *demo = user_data;
  g_autofree char *message = NULL;
  const char *target = AWRA_IS_RAIL_GROUP (onto)
    ? awra_rail_group_get_label (AWRA_RAIL_GROUP (onto))
    : awra_rail_item_get_label (AWRA_RAIL_ITEM (onto));

  (void) rail;
  message = g_strdup_printf ("Make a folder of “%s” and “%s”",
                             awra_rail_item_get_label (AWRA_RAIL_ITEM (item)), target);
  show_toast (demo->state, message);
}

static void
messaging_simulate_cb (GtkButton *button,
                       gpointer   user_data)
{
  MessagingDemo *demo = user_data;
  static const AwraPresence presences[] = {
    AWRA_PRESENCE_ONLINE, AWRA_PRESENCE_IDLE, AWRA_PRESENCE_BUSY, AWRA_PRESENCE_OFFLINE,
  };

  (void) button;
  demo->tick++;
  awra_typing_indicator_set_active (demo->typing,
                                    !awra_typing_indicator_get_active (demo->typing));
  awra_avatar_set_speaking (demo->speaker, !awra_avatar_get_speaking (demo->speaker));
  awra_avatar_set_presence (demo->speaker, presences[demo->tick % G_N_ELEMENTS (presences)]);
  awra_rail_item_set_mention_count (demo->busy_item,
                                    awra_rail_item_get_mention_count (demo->busy_item) + 1);
  awra_chip_set_count (demo->party, awra_chip_get_count (demo->party) + 1);
}

static GtkWidget *
make_message (const char   *author,
              const char   *time,
              const char   *text,
              AwraPresence  presence,
              GtkWidget    *reactions)
{
  GtkWidget *row = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_MD);
  GtkWidget *column = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_XS);
  GtkWidget *heading = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *avatar = awra_avatar_new (36, author);
  GtkWidget *name = make_label (author, AWRA_TYPOGRAPHY_LABEL);
  GtkWidget *stamp = make_label (time, AWRA_TYPOGRAPHY_MUTED);
  GtkWidget *body = make_label (text, AWRA_TYPOGRAPHY_BODY);

  awra_avatar_set_presence (AWRA_AVATAR (avatar), presence);
  gtk_widget_set_valign (avatar, GTK_ALIGN_START);
  gtk_label_set_wrap (GTK_LABEL (stamp), FALSE);
  gtk_box_append (GTK_BOX (heading), name);
  gtk_box_append (GTK_BOX (heading), stamp);
  gtk_box_append (GTK_BOX (column), heading);
  gtk_box_append (GTK_BOX (column), body);
  if (reactions != NULL)
    gtk_box_append (GTK_BOX (column), reactions);
  gtk_widget_set_hexpand (column, TRUE);
  gtk_box_append (GTK_BOX (row), avatar);
  gtk_box_append (GTK_BOX (row), column);
  return row;
}

static GtkWidget *
make_reaction (const char *emoji,
               int         count,
               gboolean    active)
{
  GtkWidget *chip = awra_chip_new (NULL);

  awra_chip_set_emoji (AWRA_CHIP (chip), emoji);
  awra_chip_set_count (AWRA_CHIP (chip), count);
  awra_chip_set_active (AWRA_CHIP (chip), active);
  g_signal_connect (chip, "toggled", G_CALLBACK (messaging_reaction_toggled_cb), NULL);
  return chip;
}

static GtkWidget *
make_rail_item (const char *label,
                gboolean    unread,
                guint       mentions,
                gboolean    muted)
{
  GtkWidget *item = awra_rail_item_new (label);

  awra_rail_item_set_child (AWRA_RAIL_ITEM (item), awra_avatar_new (48, label));
  awra_rail_item_set_unread (AWRA_RAIL_ITEM (item), unread);
  awra_rail_item_set_mention_count (AWRA_RAIL_ITEM (item), mentions);
  awra_rail_item_set_muted (AWRA_RAIL_ITEM (item), muted);
  return item;
}

static GtkWidget *
make_messaging_workspace (ShowcaseState *state)
{
  MessagingDemo *demo = g_new0 (MessagingDemo, 1);
  GtkWidget *frame = awra_card_new ();
  GtkWidget *root = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_LG);
  GtkWidget *rail = awra_rail_new ();
  GtkWidget *home = awra_rail_item_new ("Direct messages");
  GtkWidget *add = awra_rail_item_new ("Add a workspace");
  GtkWidget *folder = awra_rail_group_new ("Studio");
  GtkWidget *conversation = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_LG);
  GtkWidget *header = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *voice = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *reactions = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_XS);
  GtkWidget *filters = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_XS);
  GtkWidget *composer = awra_entry_new ();
  GtkWidget *simulate = make_button ("Simulate activity", AWRA_BUTTON_APPEARANCE_SECONDARY);
  GtkWidget *selected;
  GdkRGBA folder_color = { 0.36, 0.52, 0.96, 1.0 };
  static const char *people[] = { "Ada Lovelace", "Grace Hopper", "Alan Turing", "Linus" };

  demo->state = state;
  demo->rail = AWRA_RAIL (rail);
  g_object_set_data_full (G_OBJECT (frame), "messaging-demo", demo, g_free);
  awra_card_set_appearance (AWRA_CARD (frame), AWRA_CARD_APPEARANCE_RAISED);

  /* The rail: a header, workspaces, a folder, and an action in the footer. */
  awra_rail_item_set_icon_name (AWRA_RAIL_ITEM (home), "mail-unread-symbolic");
  awra_rail_set_header (AWRA_RAIL (rail), home);
  awra_rail_item_set_icon_name (AWRA_RAIL_ITEM (add), "list-add-symbolic");
  awra_rail_set_footer (AWRA_RAIL (rail), add);
  awra_rail_add_separator (AWRA_RAIL (rail));
  selected = make_rail_item ("Design Review", FALSE, 0, FALSE);
  awra_rail_append (AWRA_RAIL (rail), selected);
  awra_rail_append (AWRA_RAIL (rail), make_rail_item ("Rust Lang", TRUE, 0, FALSE));
  demo->busy_item = AWRA_RAIL_ITEM (make_rail_item ("GNOME", FALSE, 3, FALSE));
  awra_rail_append (AWRA_RAIL (rail), GTK_WIDGET (demo->busy_item));
  awra_rail_group_set_color (AWRA_RAIL_GROUP (folder), &folder_color);
  awra_rail_group_append (AWRA_RAIL_GROUP (folder),
                          AWRA_RAIL_ITEM (make_rail_item ("Mixing", TRUE, 0, FALSE)));
  awra_rail_group_append (AWRA_RAIL_GROUP (folder),
                          AWRA_RAIL_ITEM (make_rail_item ("Field Recording", FALSE, 2, FALSE)));
  awra_rail_group_append (AWRA_RAIL_GROUP (folder),
                          AWRA_RAIL_ITEM (make_rail_item ("Mastering", FALSE, 0, FALSE)));
  awra_rail_append (AWRA_RAIL (rail), folder);
  awra_rail_append (AWRA_RAIL (rail), make_rail_item ("Typography", FALSE, 0, TRUE));
  awra_rail_append (AWRA_RAIL (rail), make_rail_item ("Night Owls", TRUE, 0, FALSE));
  awra_rail_set_reorderable (AWRA_RAIL (rail), TRUE);
  gtk_widget_set_size_request (rail, -1, 470);
  gtk_box_append (GTK_BOX (root), rail);

  /* The conversation. */
  demo->channel = GTK_LABEL (make_label ("# Design Review", AWRA_TYPOGRAPHY_TITLE_3));
  gtk_widget_set_hexpand (GTK_WIDGET (demo->channel), TRUE);
  gtk_box_append (GTK_BOX (header), GTK_WIDGET (demo->channel));
  for (guint i = 0; i < G_N_ELEMENTS (people); i++) {
    GtkWidget *person = awra_avatar_new (28, people[i]);

    awra_avatar_set_presence (AWRA_AVATAR (person), AWRA_PRESENCE_ONLINE);
    gtk_widget_set_valign (person, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (voice), person);
    if (i == 1) {
      demo->speaker = AWRA_AVATAR (person);
      awra_avatar_set_speaking (demo->speaker, TRUE);
    }
  }
  gtk_box_append (GTK_BOX (header), voice);
  gtk_box_append (GTK_BOX (conversation), header);
  gtk_box_append (GTK_BOX (filters), awra_chip_new ("All"));
  awra_chip_set_active (AWRA_CHIP (gtk_widget_get_last_child (filters)), TRUE);
  gtk_box_append (GTK_BOX (filters), awra_chip_new ("Mentions"));
  awra_chip_set_count (AWRA_CHIP (gtk_widget_get_last_child (filters)), 3);
  gtk_box_append (GTK_BOX (filters), awra_chip_new ("Files"));
  awra_chip_set_icon_name (AWRA_CHIP (gtk_widget_get_last_child (filters)),
                           "folder-documents-symbolic");
  gtk_box_append (GTK_BOX (conversation), filters);
  gtk_box_append (GTK_BOX (conversation), make_message (
    "Grace Hopper", "09:41", "The new rail feels right: the tiles answer before I even click.",
    AWRA_PRESENCE_ONLINE, NULL));
  demo->party = AWRA_CHIP (make_reaction ("🎉", 4, FALSE));
  gtk_box_append (GTK_BOX (reactions), make_reaction ("👍", 12, TRUE));
  gtk_box_append (GTK_BOX (reactions), GTK_WIDGET (demo->party));
  gtk_box_append (GTK_BOX (reactions), make_reaction ("👀", 2, FALSE));
  gtk_box_append (GTK_BOX (conversation), make_message (
    "Ada Lovelace", "09:44", "Shipping it behind reduced motion, of course. Reactions next.",
    AWRA_PRESENCE_IDLE, reactions));
  demo->typing = AWRA_TYPING_INDICATOR (awra_typing_indicator_new ());
  awra_typing_indicator_set_label (demo->typing, "Alan is typing…");
  awra_typing_indicator_set_active (demo->typing, TRUE);
  gtk_box_append (GTK_BOX (conversation), GTK_WIDGET (demo->typing));
  gtk_entry_set_placeholder_text (GTK_ENTRY (composer), "Message # Design Review");
  gtk_widget_set_valign (composer, GTK_ALIGN_END);
  gtk_widget_set_vexpand (composer, TRUE);
  gtk_box_append (GTK_BOX (conversation), composer);
  gtk_widget_set_halign (simulate, GTK_ALIGN_START);
  g_signal_connect (simulate, "clicked", G_CALLBACK (messaging_simulate_cb), demo);
  gtk_box_append (GTK_BOX (conversation), simulate);
  gtk_widget_set_hexpand (conversation, TRUE);
  gtk_box_append (GTK_BOX (root), conversation);

  g_signal_connect (rail, "notify::selected-item", G_CALLBACK (messaging_selected_cb), demo);
  g_signal_connect (rail, "item-moved", G_CALLBACK (messaging_item_moved_cb), demo);
  g_signal_connect (rail, "item-merged", G_CALLBACK (messaging_item_merged_cb), demo);
  awra_rail_set_selected_item (AWRA_RAIL (rail), AWRA_RAIL_ITEM (selected));
  awra_card_set_child (AWRA_CARD (frame), root);
  return frame;
}

static GtkWidget *
make_edge_workspace (ShowcaseState *state,
                     GMenuModel    *context_menu)
{
  GtkWidget *edge = awra_edge_panel_new ();
  GtkWidget *canvas = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *toolbar = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *reveal = make_button ("Toggle inspector",
                                   AWRA_BUTTON_APPEARANCE_SECONDARY);
  GtkWidget *preview = awra_card_new ();
  GtkWidget *preview_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *panel = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *mode = awra_segmented_control_new ();
  GtkWidget *opacity = awra_slider_new_with_range (
    GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);

  awra_widget_set_margin (canvas, AWRA_SPACING_XL);
  gtk_box_append (GTK_BOX (toolbar), make_label ("Design workspace",
                                                AWRA_TYPOGRAPHY_TITLE_2));
  gtk_widget_set_hexpand (gtk_widget_get_first_child (toolbar), TRUE);
  gtk_box_append (GTK_BOX (toolbar), reveal);
  gtk_box_append (GTK_BOX (canvas), toolbar);
  awra_card_set_appearance (AWRA_CARD (preview), AWRA_CARD_APPEARANCE_PLAIN);
  awra_card_set_content_inset (AWRA_CARD (preview), AWRA_SPACING_XL);
  gtk_widget_set_vexpand (preview_box, TRUE);
  gtk_box_append (GTK_BOX (preview_box), make_label (
    "Hover the far right edge", AWRA_TYPOGRAPHY_TITLE_1));
  gtk_box_append (GTK_BOX (preview_box), make_label (
    "The inspector is absent from the layout until the pointer reaches its "
    "reveal zone. It then appears as a true floating material above content.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (preview_box), make_pattern_action_row (
    state, "Selected layer", "Hero / Artwork / Mask", "layers-symbolic",
    "3 ITEMS", context_menu));
  awra_card_set_child (AWRA_CARD (preview), preview_box);
  gtk_widget_set_vexpand (preview, TRUE);
  gtk_box_append (GTK_BOX (canvas), preview);

  awra_widget_set_margin (panel, AWRA_SPACING_LG);
  gtk_box_append (GTK_BOX (panel), make_label ("Inspector",
                                             AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (panel), make_label (
    "A public AwraEdgePanel, usable on either logical edge.",
    AWRA_TYPOGRAPHY_MUTED));
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (mode), "Style");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (mode), "Layout");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (mode), "Motion");
  gtk_box_append (GTK_BOX (panel), mode);
  gtk_box_append (GTK_BOX (panel), make_label ("Opacity",
                                             AWRA_TYPOGRAPHY_TITLE_3));
  gtk_range_set_value (GTK_RANGE (opacity), 72);
  gtk_box_append (GTK_BOX (panel), opacity);
  gtk_box_append (GTK_BOX (panel), make_pattern_action_row (
    state, "Material", "Floating · elevated", "applications-graphics-symbolic",
    "FROSTED", context_menu));
  gtk_box_append (GTK_BOX (panel), make_pattern_action_row (
    state, "Alignment", "End edge · 14 px inset", "format-justify-right-symbolic",
    NULL, context_menu));
  awra_edge_panel_set_content (AWRA_EDGE_PANEL (edge), canvas);
  awra_edge_panel_set_panel (AWRA_EDGE_PANEL (edge), panel);
  awra_edge_panel_set_edge (AWRA_EDGE_PANEL (edge), AWRA_PANEL_EDGE_END);
  awra_edge_panel_set_panel_width (AWRA_EDGE_PANEL (edge), 310);
  awra_edge_panel_set_reveal_width (AWRA_EDGE_PANEL (edge), 18);
  awra_edge_panel_set_hide_delay (AWRA_EDGE_PANEL (edge), 600);
  awra_edge_panel_set_auto_hide (AWRA_EDGE_PANEL (edge), TRUE);
  awra_edge_panel_set_revealed (AWRA_EDGE_PANEL (edge), FALSE);
  gtk_widget_set_size_request (edge, -1, 390);
  g_signal_connect (reveal, "clicked", G_CALLBACK (edge_panel_toggle_cb), edge);
  return edge;
}

/* A conversation: authors and messages are generated, everything else is the
 * public timeline API. Items are "author\ttext" strings. */

typedef struct {
  GtkStringList *messages;
  AwraConversationView *view;
  AwraComposer *composer;
  GtkSortListModel *sections;
  gpointer first_unread;
  AwraCompletionPopover *mentions;
  GtkStringFilter *mention_filter;
  GtkFilterListModel *mention_model;
  guint older_loaded;
  guint received;
} ConversationDemo;

static const char *conversation_authors[] = {
  "Ines", "Malik", "Sora", "Ada", "Théo", NULL,
};

static const char *conversation_lines[] = {
  "Pushed the new build, can someone check the release notes?",
  "Looks good on my side.",
  "The sidebar flickers when I resize the window quickly, is that known?",
  "Yes, there is a fix in review. It should land tonight.",
  "Lunch?",
  "I moved the meeting to 3 pm so everyone can make it, and added the agenda "
  "to the shared document. Please add anything you want to discuss.",
  "👍",
  "Benchmarks are in: startup is 18% faster and memory went down by 40 MB.",
  "Nice work!",
  "Can we keep the old shortcut for one more release? People rely on it.",
};

/* Items are "days ago\tauthor\ttext". */
static char *
conversation_message (guint       index,
                      guint       days_ago,
                      const char *prefix)
{
  return g_strdup_printf ("%u\t%s\t%s%s", days_ago,
                          conversation_authors[(index * 7 + index / 3) % (G_N_ELEMENTS (conversation_authors) - 1)],
                          prefix,
                          conversation_lines[(index * 3) % G_N_ELEMENTS (conversation_lines)]);
}

static guint
conversation_days_ago (gpointer item)
{
  return (guint) g_ascii_strtoull (gtk_string_object_get_string (item), NULL, 10);
}

static int
conversation_section_compare (gconstpointer a,
                              gconstpointer b,
                              gpointer      user_data)
{
  guint days_a = conversation_days_ago ((gpointer) a);
  guint days_b = conversation_days_ago ((gpointer) b);

  (void) user_data;
  /* Older days first; messages keep their order within a day. */
  return days_a == days_b ? GTK_ORDERING_EQUAL
                          : days_a > days_b ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
}

static void
conversation_header_setup_cb (GtkSignalListItemFactory *factory,
                              GtkListHeader            *header,
                              gpointer                  user_data)
{
  GtkWidget *divider = awra_divider_new (NULL);

  (void) factory;
  (void) user_data;
  gtk_widget_set_margin_start (divider, 20);
  gtk_widget_set_margin_end (divider, 20);
  gtk_widget_set_margin_top (divider, 8);
  gtk_list_header_set_child (header, divider);
}

static void
conversation_header_bind_cb (GtkSignalListItemFactory *factory,
                             GtkListHeader            *header,
                             gpointer                  user_data)
{
  static const char *days[] = {
    "Today", "Yesterday", "Sunday", "Saturday", "Friday", "Thursday", "Wednesday",
  };
  guint days_ago = conversation_days_ago (gtk_list_header_get_item (header));
  g_autofree char *older = g_strdup_printf ("%u days ago", days_ago);

  (void) factory;
  (void) user_data;
  awra_divider_set_label (AWRA_DIVIDER (gtk_list_header_get_child (header)),
                          days_ago < G_N_ELEMENTS (days) ? days[days_ago] : older);
}

static void
conversation_setup_cb (GtkSignalListItemFactory *factory,
                       GtkListItem              *item,
                       gpointer                  user_data)
{
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_XS);
  GtkWidget *unread = awra_divider_new ("NEW");
  GtkWidget *author = make_label (NULL, AWRA_TYPOGRAPHY_LABEL);
  GtkWidget *text = make_label (NULL, AWRA_TYPOGRAPHY_BODY);

  (void) factory;
  (void) user_data;
  gtk_label_set_selectable (GTK_LABEL (text), FALSE);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 6);
  awra_divider_set_appearance (AWRA_DIVIDER (unread), AWRA_DIVIDER_APPEARANCE_DANGER);
  awra_divider_set_label_position (AWRA_DIVIDER (unread), AWRA_DIVIDER_LABEL_POSITION_END);
  gtk_widget_set_visible (unread, FALSE);
  gtk_box_append (GTK_BOX (box), unread);
  gtk_box_append (GTK_BOX (box), author);
  gtk_box_append (GTK_BOX (box), text);
  gtk_list_item_set_child (item, box);
  gtk_list_item_set_activatable (item, FALSE);
}

static void
conversation_bind_cb (GtkSignalListItemFactory *factory,
                      GtkListItem              *item,
                      gpointer                  user_data)
{
  ConversationDemo *demo = user_data;
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *unread = gtk_widget_get_first_child (box);
  gpointer string = gtk_list_item_get_item (item);
  const char *message = strchr (gtk_string_object_get_string (string), '\t') + 1;
  const char *tab = strchr (message, '\t');
  g_autofree char *author = g_strndup (message, tab - message);

  (void) factory;
  gtk_widget_set_visible (unread, string == demo->first_unread);
  gtk_label_set_text (GTK_LABEL (gtk_widget_get_next_sibling (unread)), author);
  gtk_label_set_text (GTK_LABEL (gtk_widget_get_last_child (box)), tab + 1);
}

static void
conversation_edge_cb (AwraConversationView *view,
                      GtkPositionType       edge,
                      ConversationDemo     *demo)
{
  g_autoptr (GPtrArray) older = NULL;

  (void) view;
  if (edge != GTK_POS_TOP || demo->older_loaded >= 2000)
    return;
  older = g_ptr_array_new_with_free_func (g_free);
  for (guint i = 0; i < 100; i++)
    g_ptr_array_add (older, conversation_message (demo->older_loaded + i,
                                                  5 + (demo->older_loaded + 99 - i) / 120, ""));
  g_ptr_array_add (older, NULL);
  demo->older_loaded += 100;
  gtk_string_list_splice (demo->messages, 0, 0, (const char * const *) older->pdata);
}

static void
conversation_receive (ConversationDemo *demo,
                      guint             count)
{
  for (guint i = 0; i < count; i++) {
    g_autofree char *message = conversation_message (demo->received++ * 5 + 1, 0, "");
    guint n = g_list_model_get_n_items (G_LIST_MODEL (demo->sections));

    gtk_string_list_append (demo->messages, message);
    if (demo->first_unread == NULL && !awra_conversation_view_get_at_bottom (demo->view))
      demo->first_unread = g_list_model_get_item (G_LIST_MODEL (demo->messages), n);
  }
  if (!awra_conversation_view_get_at_bottom (demo->view))
    awra_conversation_view_set_unread_count (
      demo->view, awra_conversation_view_get_unread_count (demo->view) + count);
}

static void
conversation_receive_cb (GtkButton        *button,
                         ConversationDemo *demo)
{
  (void) button;
  conversation_receive (demo, 1);
}

static void
conversation_burst_cb (GtkButton        *button,
                       ConversationDemo *demo)
{
  (void) button;
  conversation_receive (demo, 4);
}

static void
conversation_find_cb (GtkButton        *button,
                      ConversationDemo *demo)
{
  guint n = g_list_model_get_n_items (G_LIST_MODEL (demo->messages));

  (void) button;
  if (n > 40)
    awra_conversation_view_scroll_to_item (demo->view, n - 37,
                                           AWRA_CONVERSATION_ALIGN_CENTER, TRUE);
}

static void
conversation_at_bottom_cb (AwraConversationView *view,
                           GParamSpec           *pspec,
                           ConversationDemo     *demo)
{
  (void) pspec;
  (void) demo;
  if (awra_conversation_view_get_at_bottom (view))
    awra_conversation_view_set_unread_count (view, 0);
}

static void
conversation_submit_cb (AwraComposer     *composer,
                        ConversationDemo *demo)
{
  g_autofree char *message = g_strdup_printf ("0\tYou\t%s", awra_composer_get_text (composer));

  /* Writing reads everything above. */
  g_clear_object (&demo->first_unread);

  gtk_string_list_append (demo->messages, message);
  awra_composer_set_text (composer, NULL);
  if (awra_composer_get_banner (composer) != NULL)
    awra_composer_set_banner (composer, NULL);
  awra_conversation_view_scroll_to_end (demo->view, TRUE);
}

static void
conversation_reply_cb (GtkButton        *button,
                       ConversationDemo *demo)
{
  (void) button;
  awra_composer_show_banner (demo->composer, "Replying to Malik", TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (demo->composer));
}

static GListModel *
conversation_emoji (const char * const *pairs)
{
  GListStore *store = g_list_store_new (AWRA_TYPE_EMOJI_ITEM);

  for (guint i = 0; pairs[i] != NULL; i += 2) {
    g_autoptr (AwraEmojiItem) item = awra_emoji_item_new (pairs[i + 1], pairs[i + 1], pairs[i]);

    g_list_store_append (store, item);
  }
  return G_LIST_MODEL (store);
}

static void
conversation_emoji_picked_cb (AwraEmojiPicker  *picker,
                              AwraEmojiItem    *item,
                              ConversationDemo *demo)
{
  GtkTextBuffer *buffer = awra_composer_get_buffer (demo->composer);

  gtk_text_buffer_insert_at_cursor (buffer, awra_emoji_item_get_text (item), -1);
  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (GTK_WIDGET (picker), GTK_TYPE_POPOVER)));
  gtk_widget_grab_focus (GTK_WIDGET (demo->composer));
}

static GtkWidget *
conversation_emoji_button (ConversationDemo *demo)
{
  static const char *faces[] = {
    "😀", "grinning", "😂", "joy", "😉", "wink", "😍", "heart_eyes", "🤔", "thinking",
    "😎", "sunglasses", "😴", "sleeping", "😭", "sob", "😡", "rage", "🥳", "partying_face",
    "👍", "thumbs_up", "👎", "thumbs_down", "👏", "clap", "🙏", "pray", "💪", "muscle", NULL,
  };
  static const char *nature[] = {
    "🐶", "dog", "🐱", "cat", "🦊", "fox", "🐼", "panda", "🐸", "frog", "🐧", "penguin",
    "🌵", "cactus", "🌻", "sunflower", "🌸", "cherry_blossom", "🍀", "four_leaf_clover", NULL,
  };
  static const char *food[] = {
    "🍎", "apple", "🍐", "pear", "🍋", "lemon", "🍉", "watermelon", "🍓", "strawberry",
    "🍕", "pizza", "🍔", "hamburger", "🍟", "fries", "☕", "coffee", "🍰", "cake", NULL,
  };
  static const char *recent[] = { "👍", "thumbs_up", "🎉", "tada", "❤️", "heart", NULL };
  GtkWidget *button = awra_menu_button_new ();
  GtkWidget *popover = awra_popover_new ();
  GtkWidget *picker = awra_emoji_picker_new ();
  g_autoptr (GListModel) recent_items = conversation_emoji (recent);
  g_autoptr (GListModel) face_items = conversation_emoji (faces);
  g_autoptr (GListModel) nature_items = conversation_emoji (nature);
  g_autoptr (GListModel) food_items = conversation_emoji (food);

  awra_emoji_picker_set_recent (AWRA_EMOJI_PICKER (picker), recent_items);
  awra_emoji_picker_add_section (AWRA_EMOJI_PICKER (picker), "people", "Smileys & people",
                                 "emoji-people-symbolic", face_items);
  awra_emoji_picker_add_section (AWRA_EMOJI_PICKER (picker), "nature", "Animals & nature",
                                 "emoji-nature-symbolic", nature_items);
  awra_emoji_picker_add_section (AWRA_EMOJI_PICKER (picker), "food", "Food & drink",
                                 "emoji-food-symbolic", food_items);
  g_signal_connect (picker, "picked", G_CALLBACK (conversation_emoji_picked_cb), demo);
  gtk_widget_set_size_request (picker, -1, 340);
  awra_popover_set_child (AWRA_POPOVER (popover), picker);
  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_TOP);
  awra_menu_button_set_icon_name (AWRA_MENU_BUTTON (button), "face-smile-symbolic");
  awra_menu_button_set_appearance (AWRA_MENU_BUTTON (button), AWRA_BUTTON_APPEARANCE_GHOST);
  awra_menu_button_set_popover (AWRA_MENU_BUTTON (button), AWRA_POPOVER (popover));
  gtk_widget_set_tooltip_text (button, "Emoji");
  return button;
}

static GtkWidget *
conversation_icon_button (const char *icon_name,
                          const char *tooltip)
{
  GtkWidget *button = awra_button_new_from_icon_name (icon_name);

  awra_button_set_appearance (AWRA_BUTTON (button), AWRA_BUTTON_APPEARANCE_GHOST);
  gtk_widget_set_tooltip_text (button, tooltip);
  return button;
}

/* The "@word" just before the caret, if the caret ends one. */
static gboolean
conversation_mention_bounds (AwraComposer *composer,
                             GtkTextIter  *start,
                             GtkTextIter  *end)
{
  GtkTextBuffer *buffer = awra_composer_get_buffer (composer);

  gtk_text_buffer_get_iter_at_mark (buffer, end, gtk_text_buffer_get_insert (buffer));
  *start = *end;
  while (gtk_text_iter_backward_char (start)) {
    gunichar c = gtk_text_iter_get_char (start);

    if (c == '@')
      return TRUE;
    if (!g_unichar_isalnum (c))
      return FALSE;
  }
  return FALSE;
}

static void
conversation_text_changed_cb (GtkTextBuffer    *buffer,
                              ConversationDemo *demo)
{
  GtkTextIter start;
  GtkTextIter end;
  g_autofree char *word = NULL;

  (void) buffer;
  if (!conversation_mention_bounds (demo->composer, &start, &end)) {
    gtk_popover_popdown (GTK_POPOVER (demo->mentions));
    return;
  }
  gtk_text_iter_forward_char (&start);
  word = gtk_text_iter_get_text (&start, &end);
  gtk_string_filter_set_search (demo->mention_filter, word);
  awra_completion_popover_present_at_cursor (demo->mentions);
}

static void
conversation_mention_cb (AwraCompletionPopover *mentions,
                         guint                  position,
                         ConversationDemo      *demo)
{
  GtkTextBuffer *buffer = awra_composer_get_buffer (demo->composer);
  g_autoptr (GtkStringObject) name = g_list_model_get_item (G_LIST_MODEL (demo->mention_model), position);
  g_autofree char *mention = g_strdup_printf ("@%s ", gtk_string_object_get_string (name));
  GtkTextIter start;
  GtkTextIter end;

  if (!conversation_mention_bounds (demo->composer, &start, &end))
    return;
  gtk_popover_popdown (GTK_POPOVER (mentions));
  g_signal_handlers_block_by_func (buffer, conversation_text_changed_cb, demo);
  gtk_text_buffer_delete (buffer, &start, &end);
  gtk_text_buffer_insert (buffer, &start, mention, -1);
  g_signal_handlers_unblock_by_func (buffer, conversation_text_changed_cb, demo);
}

static void
conversation_mention_setup_cb (GtkSignalListItemFactory *factory,
                               GtkListItem              *item,
                               gpointer                  user_data)
{
  (void) factory;
  (void) user_data;
  gtk_list_item_set_child (item, make_label (NULL, AWRA_TYPOGRAPHY_BODY));
}

static void
conversation_mention_bind_cb (GtkSignalListItemFactory *factory,
                              GtkListItem              *item,
                              gpointer                  user_data)
{
  (void) factory;
  (void) user_data;
  gtk_label_set_text (GTK_LABEL (gtk_list_item_get_child (item)),
                      gtk_string_object_get_string (gtk_list_item_get_item (item)));
}

static void
conversation_demo_free (gpointer data)
{
  ConversationDemo *demo = data;

  g_clear_object (&demo->messages);
  g_clear_object (&demo->sections);
  g_clear_object (&demo->first_unread);
  g_clear_object (&demo->mention_model);
  g_free (demo);
}

static GtkWidget *
make_conversation_workspace (void)
{
  ConversationDemo *demo = g_new0 (ConversationDemo, 1);
  g_autoptr (GPtrArray) initial = g_ptr_array_new_with_free_func (g_free);
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *actions = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_XS);
  GtkWidget *receive = make_button ("Receive a message", AWRA_BUTTON_APPEARANCE_SECONDARY);
  GtkWidget *burst = make_button ("Receive four", AWRA_BUTTON_APPEARANCE_GHOST);
  GtkWidget *find = make_button ("Find an older message", AWRA_BUTTON_APPEARANCE_GHOST);
  GtkWidget *reply = make_button ("Reply", AWRA_BUTTON_APPEARANCE_GHOST);
  GtkWidget *composer = awra_composer_new ();
  GtkWidget *timeline;
  GtkWidget *card;

  for (guint i = 0; i < 400; i++)
    g_ptr_array_add (initial, conversation_message (5000 + i, (399 - i) / 90, ""));
  g_ptr_array_add (initial, NULL);
  demo->messages = gtk_string_list_new ((const char * const *) initial->pdata);
  g_signal_connect (factory, "setup", G_CALLBACK (conversation_setup_cb), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (conversation_bind_cb), demo);
  demo->sections = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (demo->messages)), NULL);
  gtk_sort_list_model_set_section_sorter (
    demo->sections,
    GTK_SORTER (gtk_custom_sorter_new (conversation_section_compare, NULL, NULL)));
  timeline = awra_conversation_view_new (G_LIST_MODEL (g_object_ref (demo->sections)), factory);
  {
    GtkListItemFactory *headers = gtk_signal_list_item_factory_new ();

    g_signal_connect (headers, "setup", G_CALLBACK (conversation_header_setup_cb), NULL);
    g_signal_connect (headers, "bind", G_CALLBACK (conversation_header_bind_cb), NULL);
    awra_conversation_view_set_header_factory (AWRA_CONVERSATION_VIEW (timeline), headers);
    g_object_unref (headers);
  }
  demo->view = AWRA_CONVERSATION_VIEW (timeline);
  gtk_widget_set_size_request (timeline, -1, 330);
  gtk_widget_set_vexpand (timeline, TRUE);
  g_object_set_data_full (G_OBJECT (timeline), "showcase-conversation", demo,
                          conversation_demo_free);
  g_signal_connect (timeline, "edge-reached", G_CALLBACK (conversation_edge_cb), demo);
  g_signal_connect (timeline, "notify::at-bottom",
                    G_CALLBACK (conversation_at_bottom_cb), demo);
  g_signal_connect (receive, "clicked", G_CALLBACK (conversation_receive_cb), demo);
  g_signal_connect (burst, "clicked", G_CALLBACK (conversation_burst_cb), demo);
  g_signal_connect (find, "clicked", G_CALLBACK (conversation_find_cb), demo);
  gtk_box_append (GTK_BOX (actions), receive);
  gtk_box_append (GTK_BOX (actions), burst);
  gtk_box_append (GTK_BOX (actions), find);
  gtk_box_append (GTK_BOX (box), timeline);
  demo->composer = AWRA_COMPOSER (composer);
  awra_composer_set_placeholder_text (demo->composer, "Message #design-review — type @ to mention");
  awra_composer_set_start_widget (demo->composer,
                                  conversation_icon_button ("list-add-symbolic", "Attach"));
  awra_composer_set_end_widget (demo->composer, conversation_emoji_button (demo));
  gtk_widget_set_margin_start (composer, 12);
  gtk_widget_set_margin_end (composer, 12);
  gtk_widget_set_margin_bottom (composer, 12);
  g_signal_connect (composer, "submit", G_CALLBACK (conversation_submit_cb), demo);
  {
    GtkListItemFactory *mention_factory = gtk_signal_list_item_factory_new ();
    GtkStringList *people = gtk_string_list_new (conversation_authors);

    demo->mention_filter = gtk_string_filter_new (
      gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string"));
    gtk_string_filter_set_match_mode (demo->mention_filter, GTK_STRING_FILTER_MATCH_MODE_PREFIX);
    demo->mention_model = gtk_filter_list_model_new (G_LIST_MODEL (people),
                                                     GTK_FILTER (demo->mention_filter));
    g_signal_connect (mention_factory, "setup", G_CALLBACK (conversation_mention_setup_cb), NULL);
    g_signal_connect (mention_factory, "bind", G_CALLBACK (conversation_mention_bind_cb), NULL);
    demo->mentions = AWRA_COMPLETION_POPOVER (awra_completion_popover_new ());
    awra_completion_popover_set_model (demo->mentions, G_LIST_MODEL (demo->mention_model));
    awra_completion_popover_set_factory (demo->mentions, mention_factory);
    g_object_unref (mention_factory);
    awra_completion_popover_attach (demo->mentions,
                                    GTK_WIDGET (awra_composer_get_view (demo->composer)));
    g_signal_connect (demo->mentions, "activated", G_CALLBACK (conversation_mention_cb), demo);
    g_signal_connect (awra_composer_get_buffer (demo->composer), "changed",
                      G_CALLBACK (conversation_text_changed_cb), demo);
  }
  g_signal_connect (reply, "clicked", G_CALLBACK (conversation_reply_cb), demo);
  gtk_box_append (GTK_BOX (actions), reply);
  gtk_box_append (GTK_BOX (box), composer);
  card = make_card (box);
  awra_card_set_content_inset (AWRA_CARD (card), AWRA_SPACING_NONE);
  gtk_widget_set_overflow (card, GTK_OVERFLOW_HIDDEN);
  {
    GtkWidget *outer = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);

    gtk_box_append (GTK_BOX (outer), actions);
    gtk_box_append (GTK_BOX (outer), card);
    g_object_set_data (G_OBJECT (outer), "showcase-conversation-box", box);
    return outer;
  }
}

static GtkWidget *
make_patterns_page (ShowcaseState *state,
                    GMenuModel    *context_menu)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Application Patterns",
    "These are intentionally fake products, but every surface, control, menu "
    "and interaction is composed from public Awra APIs with no local CSS.",
    &body);

  append_section (body, "Messaging workspace",
                  "A rail of workspaces with folders, unread pills and mention "
                  "badges; avatars with presence and a speaking ring; reactions "
                  "that bounce and count; a typing line that never moves the "
                  "conversation. Drag a workspace to reorder it, or onto another "
                  "to ask for a folder.");
  gtk_box_append (GTK_BOX (body), make_messaging_workspace (state));
  append_section (body, "Conversation",
                  "A virtualized timeline that follows new messages, keeps "
                  "its place while older ones load above, and offers a way "
                  "back to the present when scrolled up.");
  gtk_box_append (GTK_BOX (body), make_conversation_workspace ());

  {
    static const char *records[] = {
      "Design system audit", "Wayland lifecycle", "Rust projection",
      "Light appearance", NULL
    };
    g_autoptr (GtkStringList) strings = gtk_string_list_new (records);
    g_autoptr (GtkMultiSelection) selection = gtk_multi_selection_new (
      G_LIST_MODEL (g_steal_pointer (&strings)));
    g_autoptr (GtkListItemFactory) name_factory =
      gtk_signal_list_item_factory_new ();
    g_autoptr (GtkListItemFactory) status_factory =
      gtk_signal_list_item_factory_new ();
    g_autoptr (AwraDataColumn) name_column = NULL;
    g_autoptr (AwraDataColumn) status_column = NULL;
    GtkWidget *data_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
    GtkWidget *data_view = awra_data_view_new (GTK_SELECTION_MODEL (selection));
    GtkWidget *selection_toolbar = awra_selection_toolbar_new (
      GTK_SELECTION_MODEL (selection));
    GtkWidget *selection_actions = make_box (GTK_ORIENTATION_HORIZONTAL,
                                             AWRA_SPACING_XS);
    GtkWidget *summary = make_box (GTK_ORIENTATION_HORIZONTAL,
                                   AWRA_SPACING_LG);
    GtkWidget *metadata = awra_metadata_group_new ();
    GtkWidget *health = awra_stat_bar_new ("Coverage");
    GtkWidget *badges = awra_badge_group_new ();

    g_signal_connect (name_factory, "setup", G_CALLBACK (list_setup_cb), NULL);
    g_signal_connect (name_factory, "bind", G_CALLBACK (list_bind_cb), NULL);
    g_signal_connect (status_factory, "setup", G_CALLBACK (list_setup_cb), NULL);
    g_signal_connect (status_factory, "bind", G_CALLBACK (list_bind_cb), NULL);
    name_column = awra_data_column_new ("Work item", name_factory);
    status_column = awra_data_column_new ("Status", status_factory);
    awra_data_column_set_expand (name_column, TRUE);
    awra_data_column_set_fixed_width (status_column, 180);
    awra_data_view_append_column (AWRA_DATA_VIEW (data_view), name_column);
    awra_data_view_append_column (AWRA_DATA_VIEW (data_view), status_column);
    gtk_box_append (GTK_BOX (selection_actions), make_button (
      "Archive", AWRA_BUTTON_APPEARANCE_SECONDARY));
    gtk_box_append (GTK_BOX (selection_actions), make_button (
      "Export", AWRA_BUTTON_APPEARANCE_GHOST));
    awra_selection_toolbar_set_title (
      AWRA_SELECTION_TOOLBAR (selection_toolbar), "work item");
    awra_selection_toolbar_set_plural_title (
      AWRA_SELECTION_TOOLBAR (selection_toolbar), "work items");
    awra_selection_toolbar_set_actions (
      AWRA_SELECTION_TOOLBAR (selection_toolbar), selection_actions);
    gtk_selection_model_select_item (GTK_SELECTION_MODEL (selection), 0, FALSE);
    gtk_selection_model_select_item (GTK_SELECTION_MODEL (selection), 2, FALSE);
    gtk_widget_set_size_request (data_view, -1, 260);
    gtk_box_append (GTK_BOX (data_box), selection_toolbar);
    gtk_box_append (GTK_BOX (data_box), data_view);
    awra_metadata_group_set_title (AWRA_METADATA_GROUP (metadata),
                                   "Selection summary");
    awra_metadata_group_append (AWRA_METADATA_GROUP (metadata),
                                "Selected", "2 records");
    awra_metadata_group_append (AWRA_METADATA_GROUP (metadata),
                                "Updated", "Today");
    awra_stat_bar_set_value (AWRA_STAT_BAR (health), 78.0);
    awra_badge_group_append (AWRA_BADGE_GROUP (badges),
      AWRA_BADGE (awra_badge_new ("ACCESSIBLE")));
    awra_badge_group_append (AWRA_BADGE_GROUP (badges),
      AWRA_BADGE (awra_badge_new ("VIRTUALIZED")));
    gtk_widget_set_hexpand (metadata, TRUE);
    gtk_widget_set_hexpand (health, TRUE);
    gtk_box_append (GTK_BOX (summary), metadata);
    gtk_box_append (GTK_BOX (summary), health);
    gtk_box_append (GTK_BOX (data_box), summary);
    gtk_box_append (GTK_BOX (data_box), badges);
    append_section (body, "Data workspace",
                    "Virtualized GTK data, typed columns and actions bound to "
                    "the selection model rather than manual row bookkeeping.");
    gtk_box_append (GTK_BOX (body), data_box);
  }

  append_section (body, "Media workspace",
                  "A restrained permanent rail, immersive content and one "
                  "raised hero surface. Right-click tracks or artwork.");
  gtk_box_append (GTK_BOX (body), make_media_workspace (state, context_menu));
  append_section (body, "Soundboard",
                  "A physical keyboard that follows the active layout: click "
                  "or use the arrow keys, Ctrl/Shift extend the selection, "
                  "drop audio files on a key, and press Enter to play it.");
  gtk_box_append (GTK_BOX (body), make_soundboard_workspace (state,
                                                             context_menu));
  append_section (body, "Launcher board",
                  "Search, toolbar actions and a denser tile composition with "
                  "selective elevation instead of borders on every region.");
  gtk_box_append (GTK_BOX (body), make_launcher_workspace (state,
                                                            context_menu));
  append_section (body, "Edge-revealed inspector",
                  "Hover the far right edge of the workspace, use the explicit "
                  "button, or press Escape from the panel to dismiss it.");
  gtk_box_append (GTK_BOX (body), make_edge_workspace (state, context_menu));
  return page;
}

static gboolean
material_editor_is_opaque (AwraMaterial *material)
{
  return awra_material_get_kind (material) == AWRA_MATERIAL_KIND_SOLID ||
         awra_material_get_preset (material) == AWRA_MATERIAL_PRESET_OPAQUE;
}

static gboolean
material_editor_has_grain (AwraMaterial *material)
{
  AwraMaterialPreset preset = awra_material_get_preset (material);

  return (preset == AWRA_MATERIAL_PRESET_NONE &&
          awra_material_get_kind (material) == AWRA_MATERIAL_KIND_FROSTED) ||
         preset == AWRA_MATERIAL_PRESET_CANVAS ||
         preset == AWRA_MATERIAL_PRESET_FLOATING;
}

static void
material_editor_update (MaterialEditorState *editor)
{
  const GdkRGBA *tint = awra_style_manager_get_material_tint (
    editor->showcase->style, editor->material);
  double radius = awra_style_manager_get_material_radius (
    editor->showcase->style, editor->material);
  double grain = awra_style_manager_get_material_grain_scale (
    editor->showcase->style, editor->material);
  double outline = awra_style_manager_get_material_outline_strength (
    editor->showcase->style, editor->material);
  double highlight = awra_style_manager_get_material_highlight_strength (
    editor->showcase->style, editor->material);
  gboolean blur = awra_style_manager_get_material_blur_enabled (
    editor->showcase->style, editor->material);
  g_autofree char *summary = NULL;

  editor->syncing = TRUE;
  gtk_range_set_value (editor->red, tint->red * 255.0);
  gtk_range_set_value (editor->green, tint->green * 255.0);
  gtk_range_set_value (editor->blue, tint->blue * 255.0);
  gtk_range_set_value (editor->alpha, tint->alpha * 100.0);
  gtk_range_set_value (editor->radius, radius);
  gtk_range_set_value (editor->grain, grain * 100.0);
  gtk_range_set_value (editor->outline, outline * 100.0);
  gtk_range_set_value (editor->highlight, highlight * 100.0);
  if (editor->blur != NULL)
    awra_switch_set_active (editor->blur, blur);
  editor->syncing = FALSE;
  summary = g_strdup_printf (
    "#%02X%02X%02X · alpha %.0f%% · radius %.0f px\n"
    "grain %.0f%% · outline %.0f%% · highlight %.0f%%%s",
    (int) (tint->red * 255.0 + 0.5),
    (int) (tint->green * 255.0 + 0.5),
    (int) (tint->blue * 255.0 + 0.5),
    tint->alpha * 100.0,
    radius,
    grain * 100.0,
    outline * 100.0,
    highlight * 100.0,
    editor->blur != NULL ? (blur ? " · blur on" : " · blur off") : "");
  gtk_label_set_text (editor->summary, summary);
}

static void
material_editor_value_changed_cb (GtkRange *range,
                                  gpointer  user_data)
{
  MaterialEditorState *editor = user_data;
  MaterialEditorChannel channel = GPOINTER_TO_INT (
    g_object_get_data (G_OBJECT (range), "material-editor-channel"));
  double value = gtk_range_get_value (range);
  GdkRGBA tint;

  if (editor->syncing)
    return;
  switch (channel) {
  case MATERIAL_EDITOR_RED:
  case MATERIAL_EDITOR_GREEN:
  case MATERIAL_EDITOR_BLUE:
  case MATERIAL_EDITOR_ALPHA:
    tint = *awra_style_manager_get_material_tint (
      editor->showcase->style, editor->material);
    if (channel == MATERIAL_EDITOR_RED)
      tint.red = value / 255.0;
    else if (channel == MATERIAL_EDITOR_GREEN)
      tint.green = value / 255.0;
    else if (channel == MATERIAL_EDITOR_BLUE)
      tint.blue = value / 255.0;
    else
      tint.alpha = value / 100.0;
    awra_style_manager_set_material_tint (
      editor->showcase->style, editor->material, &tint);
    break;
  case MATERIAL_EDITOR_RADIUS:
    awra_style_manager_set_material_radius (
      editor->showcase->style, editor->material, value);
    break;
  case MATERIAL_EDITOR_GRAIN:
    awra_style_manager_set_material_grain_scale (
      editor->showcase->style, editor->material, value / 100.0);
    break;
  case MATERIAL_EDITOR_OUTLINE:
    awra_style_manager_set_material_outline_strength (
      editor->showcase->style, editor->material, value / 100.0);
    break;
  case MATERIAL_EDITOR_HIGHLIGHT:
    awra_style_manager_set_material_highlight_strength (
      editor->showcase->style, editor->material, value / 100.0);
    break;
  default:
    g_assert_not_reached ();
  }
}

static GtkWidget *
material_editor_slider (MaterialEditorState  *editor,
                        const char           *label,
                        double                maximum,
                        double                step,
                        MaterialEditorChannel channel,
                        GtkRange            **range_out)
{
  GtkWidget *row = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_MD);
  GtkWidget *caption = make_label (label, AWRA_TYPOGRAPHY_BODY);
  GtkWidget *slider = awra_slider_new_with_range (
    GTK_ORIENTATION_HORIZONTAL, 0.0, maximum, step);

  gtk_widget_set_size_request (caption, 88, -1);
  gtk_widget_set_hexpand (slider, TRUE);
  g_object_set_data (G_OBJECT (slider), "material-editor-channel",
                     GINT_TO_POINTER (channel));
  g_signal_connect (slider, "value-changed",
                    G_CALLBACK (material_editor_value_changed_cb), editor);
  gtk_box_append (GTK_BOX (row), caption);
  gtk_box_append (GTK_BOX (row), slider);
  *range_out = GTK_RANGE (slider);
  return row;
}

static void
material_editor_blur_changed_cb (AwraSwitch *switcher,
                                 GParamSpec *pspec,
                                 gpointer    user_data)
{
  MaterialEditorState *editor = user_data;

  (void) pspec;
  if (!editor->syncing)
    awra_style_manager_set_material_blur_enabled (
      editor->showcase->style, editor->material,
      awra_switch_get_active (switcher));
}

static void
material_editor_reset_cb (GtkButton *button,
                          gpointer   user_data)
{
  MaterialEditorState *editor = user_data;

  (void) button;
  awra_style_manager_reset_material_tuning_for_material (
    editor->showcase->style, editor->material);
  show_toast (editor->showcase, "Material profile reset to Awra defaults");
}

static void
material_editor_style_changed_cb (AwraStyleManager *manager,
                                  GParamSpec       *pspec,
                                  GtkWidget        *popover)
{
  MaterialEditorState *editor = g_object_get_data (
    G_OBJECT (popover), "material-editor-state");

  (void) manager;
  (void) pspec;
  if (editor != NULL)
    material_editor_update (editor);
}

static void
material_editor_free (gpointer data)
{
  MaterialEditorState *editor = data;

  g_clear_object (&editor->material);
  g_free (editor);
}

static GtkWidget *
make_material_settings_button (ShowcaseState *state,
                               AwraMaterial  *material,
                               const char    *title)
{
  MaterialEditorState *editor = g_new0 (MaterialEditorState, 1);
  GtkWidget *button = awra_menu_button_new ();
  GtkWidget *popover = awra_popover_new ();
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *alpha_row;
  GtkWidget *grain_row;
  GtkWidget *reset = make_button ("Reset this profile",
                                  AWRA_BUTTON_APPEARANCE_SECONDARY);

  editor->showcase = state;
  editor->material = g_object_ref (material);
  awra_widget_set_margin (box, AWRA_SPACING_LG);
  gtk_widget_set_size_request (box, 440, -1);
  gtk_box_append (GTK_BOX (box), make_label (title,
                                            AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (box), make_label (
    "Global Awra recipe · every surface using this material updates live.",
    AWRA_TYPOGRAPHY_MUTED));
  gtk_box_append (GTK_BOX (box), material_editor_slider (
    editor, "Tint · red", 255, 1, MATERIAL_EDITOR_RED, &editor->red));
  gtk_box_append (GTK_BOX (box), material_editor_slider (
    editor, "Tint · green", 255, 1, MATERIAL_EDITOR_GREEN, &editor->green));
  gtk_box_append (GTK_BOX (box), material_editor_slider (
    editor, "Tint · blue", 255, 1, MATERIAL_EDITOR_BLUE, &editor->blue));
  alpha_row = material_editor_slider (
    editor, "Opacity", 100, 1, MATERIAL_EDITOR_ALPHA, &editor->alpha);
  gtk_widget_set_sensitive (alpha_row, !material_editor_is_opaque (material));
  gtk_box_append (GTK_BOX (box), alpha_row);
  gtk_box_append (GTK_BOX (box), material_editor_slider (
    editor, "Corner radius", 48, 1, MATERIAL_EDITOR_RADIUS,
    &editor->radius));
  grain_row = material_editor_slider (
    editor, "Grain", 400, 1, MATERIAL_EDITOR_GRAIN, &editor->grain);
  gtk_widget_set_sensitive (grain_row, material_editor_has_grain (material));
  gtk_box_append (GTK_BOX (box), grain_row);
  gtk_box_append (GTK_BOX (box), material_editor_slider (
    editor, "Outline", 200, 1, MATERIAL_EDITOR_OUTLINE,
    &editor->outline));
  gtk_box_append (GTK_BOX (box), material_editor_slider (
    editor, "Highlight", 200, 1, MATERIAL_EDITOR_HIGHLIGHT,
    &editor->highlight));
  if (material_editor_has_grain (material)) {
    GtkWidget *blur_row = make_box (GTK_ORIENTATION_HORIZONTAL,
                                    AWRA_SPACING_MD);

    gtk_box_append (GTK_BOX (blur_row), make_label (
      "Native background blur", AWRA_TYPOGRAPHY_BODY));
    editor->blur = AWRA_SWITCH (awra_switch_new ());
    gtk_widget_set_hexpand (GTK_WIDGET (editor->blur), TRUE);
    gtk_widget_set_halign (GTK_WIDGET (editor->blur), GTK_ALIGN_END);
    gtk_box_append (GTK_BOX (blur_row), GTK_WIDGET (editor->blur));
    gtk_box_append (GTK_BOX (box), blur_row);
    g_signal_connect (editor->blur, "notify::active",
                      G_CALLBACK (material_editor_blur_changed_cb), editor);
  }
  editor->summary = GTK_LABEL (make_label (NULL, AWRA_TYPOGRAPHY_MONOSPACE));
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (editor->summary));
  gtk_box_append (GTK_BOX (box), reset);
  g_signal_connect (reset, "clicked",
                    G_CALLBACK (material_editor_reset_cb), editor);
  g_object_set_data_full (G_OBJECT (popover), "material-editor-state",
                          editor, material_editor_free);
  g_signal_connect_object (state->style, "notify::token-set",
                           G_CALLBACK (material_editor_style_changed_cb),
                           popover, 0);
  awra_popover_set_child (AWRA_POPOVER (popover), box);
  awra_menu_button_set_icon_name (AWRA_MENU_BUTTON (button),
                                  "preferences-system-symbolic");
  awra_menu_button_set_appearance (AWRA_MENU_BUTTON (button),
                                   AWRA_BUTTON_APPEARANCE_TOOLBAR);
  awra_menu_button_set_popover (AWRA_MENU_BUTTON (button),
                                AWRA_POPOVER (popover));
  awra_widget_set_tooltip_text (button, "Tune this global material profile");
  material_editor_update (editor);
  return button;
}

static GtkWidget *
make_material_card (const char      *title,
                    const char      *description,
                    AwraMaterial    *material,
                    AwraSurfaceRole  role,
                    guint            elevation,
                    GMenuModel      *context_menu,
                    ShowcaseState   *state)
{
  GtkWidget *surface = awra_surface_new_with_role (role);
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_SM);
  GtkWidget *header = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *title_label = make_label (title, AWRA_TYPOGRAPHY_TITLE_3);

  gtk_widget_set_hexpand (surface, TRUE);
  gtk_widget_set_size_request (surface, 190, 150);
  gtk_widget_set_hexpand (title_label, TRUE);
  gtk_box_append (GTK_BOX (header), title_label);
  gtk_box_append (GTK_BOX (header), make_material_settings_button (
    state, material, title));
  gtk_box_append (GTK_BOX (box), header);
  gtk_box_append (GTK_BOX (box), make_label (description, AWRA_TYPOGRAPHY_BODY));
  awra_widget_set_margin (box, AWRA_SPACING_LG);
  awra_surface_set_child (AWRA_SURFACE (surface), box);
  awra_surface_set_material (AWRA_SURFACE (surface), material);
  awra_surface_set_elevation (AWRA_SURFACE (surface), elevation);
  awra_widget_set_context_menu_model (surface, context_menu);
  return surface;
}

static GtkWidget *
make_surfaces_page (ShowcaseState *state,
                    GMenuModel    *context_menu)
{
  g_autoptr (AwraMaterial) solid = awra_material_new_solid ();
  g_autoptr (AwraMaterial) translucent = awra_material_new_translucent ();
  g_autoptr (AwraMaterial) frosted = awra_material_new_frosted ();
  g_autoptr (AwraMaterial) canvas = awra_material_new_for_preset (AWRA_MATERIAL_PRESET_CANVAS);
  g_autoptr (AwraMaterial) content = awra_material_new_for_preset (AWRA_MATERIAL_PRESET_CONTENT);
  g_autoptr (AwraMaterial) chrome = awra_material_new_for_preset (AWRA_MATERIAL_PRESET_CHROME);
  g_autoptr (AwraMaterial) layer = awra_material_new_for_preset (AWRA_MATERIAL_PRESET_LAYER);
  g_autoptr (AwraMaterial) floating = awra_material_new_for_preset (AWRA_MATERIAL_PRESET_FLOATING);
  g_autoptr (AwraMaterial) opaque = awra_material_new_for_preset (AWRA_MATERIAL_PRESET_OPAQUE);
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Surfaces & Materials",
    "Materials retain their semantic identity across themes. Right-click any "
    "surface to exercise the shared context menu.", &body);
  GtkWidget *row = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_MD);
  GtkWidget *presets = gtk_grid_new ();
  GtkWidget *elevations = make_box (GTK_ORIENTATION_HORIZONTAL,
                                    AWRA_SPACING_MD);

  append_section (body, "Material intents", "Solid, explicit alpha and native Frosted.");
  gtk_box_set_homogeneous (GTK_BOX (row), TRUE);
  gtk_box_append (GTK_BOX (row), make_material_card (
    "Solid", "Opaque by intent and the Frosted safety fallback.", solid,
    AWRA_SURFACE_ROLE_CARD, 1, context_menu, state));
  gtk_box_append (GTK_BOX (row), make_material_card (
    "Translucent", "Tint and alpha without requesting compositor blur.", translucent,
    AWRA_SURFACE_ROLE_CARD, 2, context_menu, state));
  gtk_box_append (GTK_BOX (row), make_material_card (
    "Frosted", "Blur when capable; the same chromatic identity, opaque otherwise.",
    frosted, AWRA_SURFACE_ROLE_FLOATING, 4, context_menu, state));
  gtk_box_append (GTK_BOX (body), row);
  append_section (body, "Semantic presets",
                  "Role-aware recipes compose lightly instead of stacking identical glass panels.");
  awra_grid_set_spacing (GTK_GRID (presets), AWRA_SPACING_MD,
                         AWRA_SPACING_MD);
  gtk_grid_attach (GTK_GRID (presets), make_material_card (
    "Canvas", "Native frosted foundation.", canvas,
    AWRA_SURFACE_ROLE_WINDOW, AWRA_ELEVATION_FLAT, context_menu, state), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (presets), make_material_card (
    "Content", "Unpainted reading plane.", content,
    AWRA_SURFACE_ROLE_CONTENT, AWRA_ELEVATION_FLAT, context_menu, state), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (presets), make_material_card (
    "Chrome", "Structural sidebar and header tint.", chrome,
    AWRA_SURFACE_ROLE_TOOLBAR, AWRA_ELEVATION_FLAT, context_menu, state), 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (presets), make_material_card (
    "Layer", "Quiet content grouping.", layer,
    AWRA_SURFACE_ROLE_CARD, AWRA_ELEVATION_FLAT, context_menu, state), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (presets), make_material_card (
    "Floating", "Menus, dialogs and HUD.", floating,
    AWRA_SURFACE_ROLE_FLOATING, AWRA_ELEVATION_FLOATING, context_menu, state), 1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (presets), make_material_card (
    "Opaque", "Explicit accessibility-safe plane.", opaque,
    AWRA_SURFACE_ROLE_CARD, AWRA_ELEVATION_FLAT, context_menu, state), 2, 1, 1, 1);
  gtk_box_append (GTK_BOX (body), presets);
  append_section (body, "Elevation hierarchy",
                  "Nested cards separate content without requesting sibling backdrop filters.");
  gtk_box_set_homogeneous (GTK_BOX (elevations), TRUE);
  for (AwraElevation elevation = AWRA_ELEVATION_FLAT;
       elevation <= AWRA_ELEVATION_FLOATING;
       elevation++) {
    const char *names[] = { "Flat", "Raised", "Floating" };
    gtk_box_append (GTK_BOX (elevations), make_material_card (
      names[elevation], elevation == AWRA_ELEVATION_FLOATING
        ? "Transient surface with true depth" : "Content grouping surface",
      elevation == AWRA_ELEVATION_FLOATING ? floating : layer,
      elevation == AWRA_ELEVATION_FLOATING
        ? AWRA_SURFACE_ROLE_FLOATING : AWRA_SURFACE_ROLE_CARD,
      elevation, context_menu, state));
  }
  gtk_box_append (GTK_BOX (body), elevations);
  return page;
}

static GtkWidget *
make_controls_page (void)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Controls",
    "The gallery keeps GTK actions, keyboard behavior and accessible roles while "
    "Awra owns every identity color, border, radius and focus ring.", &body);
  GtkWidget *grid = gtk_grid_new ();
  GtkWidget *selected = awra_toggle_button_new_with_label ("Selected");
  GtkWidget *disabled = awra_button_new_with_label ("Disabled");
  GtkWidget *primary = awra_button_new_with_label ("Primary");
  GtkWidget *ghost = awra_button_new_with_label ("Ghost");
  GtkWidget *destructive = awra_button_new_with_label ("Destructive");
  GtkWidget *switcher = awra_switch_new ();
  GtkWidget *check = awra_check_button_new_with_label ("Checked");
  GtkWidget *radio_a = awra_check_button_new_with_label ("Option A");
  GtkWidget *radio_b = awra_check_button_new_with_label ("Option B");
  GtkWidget *segments = awra_segmented_control_new ();
  GtkWidget *progress = awra_progress_bar_new ();

  awra_grid_set_spacing (GTK_GRID (grid), AWRA_SPACING_LG,
                         AWRA_SPACING_LG);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (selected), TRUE);
  awra_button_set_appearance (AWRA_BUTTON (primary),
                              AWRA_BUTTON_APPEARANCE_PRIMARY);
  awra_button_set_appearance (AWRA_BUTTON (ghost),
                              AWRA_BUTTON_APPEARANCE_GHOST);
  awra_button_set_appearance (AWRA_BUTTON (destructive),
                              AWRA_BUTTON_APPEARANCE_DESTRUCTIVE);
  gtk_widget_set_sensitive (disabled, FALSE);
  awra_switch_set_active (AWRA_SWITCH (switcher), TRUE);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
  awra_check_button_set_group (AWRA_CHECK_BUTTON (radio_b),
                               AWRA_CHECK_BUTTON (radio_a));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (radio_a), TRUE);
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "Day");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "Week");
  awra_segmented_control_append (AWRA_SEGMENTED_CONTROL (segments), "Month");
  awra_segmented_control_set_selected (AWRA_SEGMENTED_CONTROL (segments), 1);
  awra_progress_bar_set_fraction (AWRA_PROGRESS_BAR (progress), 0.68);
  awra_progress_bar_set_text (AWRA_PROGRESS_BAR (progress), "68 % complete");
  awra_progress_bar_set_show_text (AWRA_PROGRESS_BAR (progress), TRUE);
  gtk_grid_attach (GTK_GRID (grid), primary, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), awra_button_new_with_label ("Secondary"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), ghost, 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), destructive, 3, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), selected, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), disabled, 1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), switcher, 2, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 3, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), radio_a, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), radio_b, 1, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), segments, 0, 3, 4, 1);
  gtk_grid_attach (GTK_GRID (grid), progress, 0, 4, 4, 1);
  gtk_box_append (GTK_BOX (body), make_plain_group (grid));
  append_section (body, "Action rows",
                  "Reusable settings and list rows use hierarchy, spacing and "
                  "a quiet hover plane instead of individual card borders.");
  {
    GtkWidget *rows = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_XS);
    GtkWidget *network = awra_action_row_new (
      "Network", "Connected to Studio 5 GHz");
    GtkWidget *notifications = awra_action_row_new (
      "Notifications", "Mentions and direct messages");
    GtkWidget *storage = awra_action_row_new (
      "Storage", "18.4 GB available");

    awra_action_row_set_icon_name (AWRA_ACTION_ROW (network),
                                   "network-wireless-symbolic");
    awra_action_row_set_icon_name (AWRA_ACTION_ROW (notifications),
                                   "preferences-system-notifications-symbolic");
    awra_action_row_set_icon_name (AWRA_ACTION_ROW (storage),
                                   "drive-harddisk-symbolic");
    awra_action_row_set_suffix (AWRA_ACTION_ROW (network),
                                awra_badge_new ("ONLINE"));
    awra_action_row_set_suffix (AWRA_ACTION_ROW (notifications),
                                awra_badge_new ("3"));
    gtk_box_append (GTK_BOX (rows), network);
    gtk_box_append (GTK_BOX (rows), notifications);
    gtk_box_append (GTK_BOX (rows), storage);
    gtk_box_append (GTK_BOX (body), make_plain_group (rows));
  }
  append_section (body, "Key caps and level",
                  "Free keys recede as wells, bound keys sit on the plane, "
                  "and domain tints keep legible legends. Activate a cap to "
                  "replay its press motion.");
  {
    GtkWidget *caps = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
    GtkWidget *volume = awra_volume_control_new ();
    static const struct {
      const char *label;
      const char *secondary;
      double units;
      guint state;
    } samples[] = {
      { "Q", NULL, 1.0, 0 },
      { "W", "Bound", 1.0, 1 },
      { "E", "Tinted", 1.0, 2 },
      { "R", "Selected", 1.0, 3 },
      { "T", "Playing", 1.0, 4 },
      { "Y", "Filtered", 1.0, 5 },
      { "Shift", "Wide key", 2.25, 6 },
    };
    const GdkRGBA tint = { 0.20, 0.45, 0.82, 1.0 };
    const GdkRGBA mood = { 0.96, 0.52, 0.22, 1.0 };

    for (guint i = 0; i < G_N_ELEMENTS (samples); i++) {
      GtkWidget *cap = awra_key_cap_new (NULL, samples[i].label);

      awra_key_cap_set_secondary_label (AWRA_KEY_CAP (cap),
                                        samples[i].secondary);
      awra_key_cap_set_width_units (AWRA_KEY_CAP (cap), samples[i].units);
      awra_key_cap_set_bound (AWRA_KEY_CAP (cap), samples[i].state > 0);
      if (samples[i].state == 2 || samples[i].state == 6) {
        awra_key_cap_set_tint (AWRA_KEY_CAP (cap), &tint);
        awra_key_cap_set_edge_color (AWRA_KEY_CAP (cap), &mood);
      }
      awra_key_cap_set_selected (AWRA_KEY_CAP (cap), samples[i].state == 3);
      awra_key_cap_set_active (AWRA_KEY_CAP (cap), samples[i].state == 4);
      awra_key_cap_set_dimmed (AWRA_KEY_CAP (cap), samples[i].state == 5);
      if (samples[i].state == 6)
        awra_key_cap_set_badge (AWRA_KEY_CAP (cap), "2");
      g_signal_connect_swapped (cap, "activated",
                                G_CALLBACK (awra_key_cap_flash), cap);
      gtk_widget_set_size_request (cap, -1, 56);
      gtk_widget_set_valign (cap, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (caps), cap);
    }
    awra_volume_control_set_volume (AWRA_VOLUME_CONTROL (volume), 0.64);
    gtk_widget_set_size_request (volume, 180, -1);
    gtk_widget_set_valign (volume, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (volume, TRUE);
    gtk_widget_set_halign (volume, GTK_ALIGN_END);
    gtk_box_append (GTK_BOX (caps), volume);
    gtk_box_append (GTK_BOX (body), make_plain_group (caps));
  }
  append_section (body, "Interval timeline",
                  "Chapters, clips and subtitle cues share a backend-neutral ruler. "
                  "Drag this editable track to preview a synchronization offset.");
  {
    GtkWidget *timeline = awra_timeline_new ();
    awra_timeline_set_duration (AWRA_TIMELINE (timeline), 60);
    awra_timeline_set_position (AWRA_TIMELINE (timeline), 24);
    awra_timeline_set_editable (AWRA_TIMELINE (timeline), TRUE);
    awra_timeline_add_interval (AWRA_TIMELINE (timeline), 5, 16, "Opening");
    awra_timeline_add_interval (AWRA_TIMELINE (timeline), 20, 32, "A subtitle cue");
    awra_timeline_add_interval (AWRA_TIMELINE (timeline), 38, 56, "Another cue");
    gtk_box_append (GTK_BOX (body), timeline);
  }
  append_section (body, "Floating controls",
                  "The canvas keeps its full allocation. Move the pointer to reveal "
                  "the bar; hover or keyboard focus protects its controls from hiding.");
  {
    AwraOverlayBar *overlay = AWRA_OVERLAY_BAR (awra_overlay_bar_new ());
    GtkWidget *canvas = gtk_label_new ("Full-size content beneath floating controls");
    GtkWidget *controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *timeline = awra_timeline_new ();
    gtk_widget_set_size_request (GTK_WIDGET (overlay), -1, 240);
    awra_timeline_set_duration (AWRA_TIMELINE (timeline), 120);
    awra_timeline_set_step_increment (AWRA_TIMELINE (timeline), 5);
    awra_timeline_set_page_increment (AWRA_TIMELINE (timeline), 30);
    awra_timeline_set_position (AWRA_TIMELINE (timeline), 32);
    awra_timeline_set_preview (AWRA_TIMELINE (timeline), gtk_label_new ("Backend-owned preview"));
    gtk_widget_set_hexpand (timeline, TRUE);
    gtk_box_append (GTK_BOX (controls), awra_button_new_from_icon_name ("media-playback-start-symbolic"));
    gtk_box_append (GTK_BOX (controls), timeline);
    awra_overlay_bar_set_content (overlay, canvas);
    awra_overlay_bar_set_bar (overlay, controls);
    awra_surface_set_backdrop_widget (awra_overlay_bar_get_surface (overlay), canvas);
    awra_overlay_bar_set_auto_hide (overlay, TRUE);
    gtk_box_append (GTK_BOX (body), GTK_WIDGET (overlay));
  }
  append_section (body, "Keyboard-focus proof",
                  "Use Tab and Shift+Tab: focus-visible rings remain explicit in Light and Dark.");
  gtk_box_append (GTK_BOX (body), awra_badge_new ("GTK behavior · Awra rendering"));
  return page;
}

static void
search_changed_cb (GtkSearchEntry *entry,
                   gpointer        user_data)
{
  ShowcaseState *state = user_data;
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
  g_autofree char *result = NULL;

  result = *text == '\0'
             ? g_strdup ("Type to filter the fake library.")
             : g_strdup_printf ("3 demo results for “%s”", text);
  gtk_label_set_text (state->search_result, result);
}

static void
show_toast_static_cb (const char *message,
                      GtkWidget  *source)
{
  ShowcaseState *state = g_object_get_data (G_OBJECT (source),
                                            "showcase-state");

  if (state != NULL)
    show_toast (state, message);
}

static void
drop_files_cb (AwraDropZone *drop,
               GdkFileList  *files,
               gpointer      user_data)
{
  g_autoptr (GSList) list = gdk_file_list_get_files (files);
  g_autofree char *message = g_strdup_printf ("%u audio file(s) added",
                                              g_slist_length (list));

  (void) drop;
  show_toast (user_data, message);
}

static GtkWidget *
make_inputs_page (ShowcaseState *state)
{
  static const char *choices[] = { "Design", "Engineering", "Research", NULL };
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Inputs",
    "A realistic filter form exercises editable text, IME, clipboard, search, "
    "selection, switches and continuous values.", &body);
  GtkWidget *form = awra_form_new ();
  GtkWidget *name = awra_entry_new ();
  GtkWidget *search = awra_search_entry_new ();
  GtkWidget *slider = awra_slider_new_with_range (
    GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  GtkWidget *spin = awra_spin_button_new_with_range (1, 20, 1);
  GtkWidget *dropdown = awra_dropdown_new_from_strings (choices);
  GtkWidget *switcher = awra_switch_new ();
  GtkWidget *name_row = awra_form_row_new ();
  GtkWidget *search_row = awra_form_row_new ();
  GtkWidget *team_row = awra_form_row_new ();
  GtkWidget *items_row = awra_form_row_new ();
  GtkWidget *intensity_row = awra_form_row_new ();
  GtkWidget *preview_row = awra_form_row_new ();
  GtkWidget *results_row = awra_form_row_new ();
  GtkWidget *password = awra_password_entry_new ();
  GtkWidget *notes = awra_text_area_new ();
  GtkWidget *tags = awra_tag_entry_new ();
  GtkWidget *password_row = awra_form_row_new ();
  GtkWidget *notes_row = awra_form_row_new ();
  GtkWidget *tags_row = awra_form_row_new ();
  GtkWidget *choice_group = awra_choice_group_new ();
  GtkWidget *banner = awra_status_banner_new ();
  GtkWidget *states = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_MD);
  GtkWidget *empty = awra_empty_state_new ();
  GtkWidget *loading = awra_loading_state_new ();
  GtkWidget *error = awra_error_state_new ();

  gtk_entry_set_placeholder_text (GTK_ENTRY (name), "Workspace name / IME");
  awra_search_entry_set_placeholder_text (AWRA_SEARCH_ENTRY (search),
                                           "Search the fake library");
  gtk_range_set_value (GTK_RANGE (slider), 46.0);
  awra_spin_button_set_value (AWRA_SPIN_BUTTON (spin), 6.0);
  awra_switch_set_active (AWRA_SWITCH (switcher), TRUE);
  state->search_result = GTK_LABEL (make_label (
    "Type to filter the fake library.", AWRA_TYPOGRAPHY_MUTED));
  awra_form_row_set_label (AWRA_FORM_ROW (name_row), "Name");
  awra_form_row_set_help_text (AWRA_FORM_ROW (name_row),
                               "Supports IME and clipboard operations");
  awra_form_row_set_required (AWRA_FORM_ROW (name_row), TRUE);
  awra_form_row_set_control (AWRA_FORM_ROW (name_row), name);
  awra_form_row_set_label (AWRA_FORM_ROW (search_row), "Search");
  awra_form_row_set_control (AWRA_FORM_ROW (search_row), search);
  awra_form_row_set_label (AWRA_FORM_ROW (team_row), "Team");
  awra_form_row_set_control (AWRA_FORM_ROW (team_row), dropdown);
  awra_form_row_set_label (AWRA_FORM_ROW (items_row), "Items");
  awra_form_row_set_control (AWRA_FORM_ROW (items_row), spin);
  awra_form_row_set_label (AWRA_FORM_ROW (intensity_row), "Intensity");
  awra_form_row_set_control (AWRA_FORM_ROW (intensity_row), slider);
  awra_form_row_set_label (AWRA_FORM_ROW (preview_row), "Live preview");
  awra_form_row_set_control (AWRA_FORM_ROW (preview_row), switcher);
  awra_form_row_set_label (AWRA_FORM_ROW (results_row), "Results");
  awra_form_row_set_control (AWRA_FORM_ROW (results_row),
                             GTK_WIDGET (state->search_result));
  awra_password_entry_set_placeholder_text (AWRA_PASSWORD_ENTRY (password),
                                             "Workspace secret");
  awra_form_row_set_label (AWRA_FORM_ROW (password_row), "Password");
  awra_form_row_set_help_text (AWRA_FORM_ROW (password_row),
                               "GTK owns secure editing and the reveal action");
  awra_form_row_set_control (AWRA_FORM_ROW (password_row), password);
  awra_text_area_set_placeholder_text (AWRA_TEXT_AREA (notes),
                                       "Add release notes…");
  awra_form_row_set_label (AWRA_FORM_ROW (notes_row), "Notes");
  awra_form_row_set_control (AWRA_FORM_ROW (notes_row), notes);
  awra_tag_entry_add (AWRA_TAG_ENTRY (tags), "Design");
  awra_tag_entry_add (AWRA_TAG_ENTRY (tags), "Accessibility");
  awra_form_row_set_label (AWRA_FORM_ROW (tags_row), "Tags");
  awra_form_row_set_control (AWRA_FORM_ROW (tags_row), tags);
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (name_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (search_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (team_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (items_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (intensity_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (preview_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (results_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (password_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (notes_row));
  awra_form_append (AWRA_FORM (form), AWRA_FORM_ROW (tags_row));
  g_signal_connect (awra_search_entry_get_delegate (AWRA_SEARCH_ENTRY (search)),
                    "search-changed", G_CALLBACK (search_changed_cb), state);
  gtk_box_append (GTK_BOX (body), make_plain_group (form));
  awra_choice_group_set_title (AWRA_CHOICE_GROUP (choice_group),
                               "Notification digest");
  awra_choice_group_set_description (AWRA_CHOICE_GROUP (choice_group),
                                     "An accessible exclusive choice group.");
  awra_choice_group_set_exclusive (AWRA_CHOICE_GROUP (choice_group), TRUE);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (awra_choice_group_append (
    AWRA_CHOICE_GROUP (choice_group), "Daily")), TRUE);
  awra_choice_group_append (AWRA_CHOICE_GROUP (choice_group), "Weekly");
  awra_choice_group_append (AWRA_CHOICE_GROUP (choice_group), "Never");
  gtk_box_append (GTK_BOX (body), choice_group);
  awra_status_banner_set_title (AWRA_STATUS_BANNER (banner),
                                "Draft settings");
  awra_status_banner_set_description (AWRA_STATUS_BANNER (banner),
                                      "Changes are local to this demonstration.");
  awra_status_banner_set_appearance (AWRA_STATUS_BANNER (banner),
                                     AWRA_STATUS_APPEARANCE_INFO);
  awra_status_banner_set_action (AWRA_STATUS_BANNER (banner),
                                 make_button ("Apply", AWRA_BUTTON_APPEARANCE_GHOST));
  gtk_box_append (GTK_BOX (body), banner);
  append_section (body, "Tiles",
                  "Drag a divider to resize its neighbours, double-click it to "
                  "even them out, or divide a tile with its buttons.");
  gtk_box_append (GTK_BOX (body), make_tiles ());

  append_section (body, "Media inputs",
                  "Record a shortcut (Escape cancels), drag the start marker "
                  "or use the arrow keys, and drop audio files on the well.");
  {
    GtkWidget *media = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
    GtkWidget *capture_row = awra_form_row_new ();
    GtkWidget *capture = awra_shortcut_capture_new ();
    GtkWidget *start_row = awra_form_row_new ();
    GtkWidget *waveform = make_waveform (184.0, 2.1, 48);
    GtkWidget *drop = awra_drop_zone_new ();
    GtkWidget *form_media = awra_form_new ();
    static const char *audio[] = { "mp3", "ogg", "flac", "wav", "m4a", NULL };

    awra_shortcut_capture_set_placeholder (AWRA_SHORTCUT_CAPTURE (capture),
                                           "Click to record");
    awra_shortcut_capture_set_allow_chords (AWRA_SHORTCUT_CAPTURE (capture),
                                            TRUE);
    awra_form_row_set_label (AWRA_FORM_ROW (capture_row), "Shortcut");
    awra_form_row_set_help_text (AWRA_FORM_ROW (capture_row),
                                 "Chords such as A+Z are allowed");
    awra_form_row_set_control (AWRA_FORM_ROW (capture_row), capture);
    awra_waveform_view_set_marker_editable (AWRA_WAVEFORM_VIEW (waveform),
                                            TRUE);
    awra_waveform_view_set_marker_position (AWRA_WAVEFORM_VIEW (waveform),
                                            21.5);
    awra_waveform_view_set_hint_position (AWRA_WAVEFORM_VIEW (waveform), 34.0);
    awra_waveform_view_set_position (AWRA_WAVEFORM_VIEW (waveform), 58.0);
    awra_form_row_set_label (AWRA_FORM_ROW (start_row), "Start point");
    awra_form_row_set_help_text (AWRA_FORM_ROW (start_row),
                                 "The dashed line is a suggestion");
    awra_form_row_set_control (AWRA_FORM_ROW (start_row), waveform);
    awra_form_append (AWRA_FORM (form_media), AWRA_FORM_ROW (capture_row));
    awra_form_append (AWRA_FORM (form_media), AWRA_FORM_ROW (start_row));
    awra_drop_zone_set_title (AWRA_DROP_ZONE (drop), "Drop audio files");
    awra_drop_zone_set_subtitle (AWRA_DROP_ZONE (drop),
                                 "MP3, OGG, FLAC, WAV or M4A");
    awra_drop_zone_set_icon_name (AWRA_DROP_ZONE (drop),
                                  "folder-music-symbolic");
    awra_drop_zone_set_show_browse_button (AWRA_DROP_ZONE (drop), TRUE);
    awra_drop_zone_set_accepted_extensions (AWRA_DROP_ZONE (drop), audio);
    g_signal_connect_swapped (drop, "browse-requested",
                              G_CALLBACK (show_toast_static_cb),
                              (gpointer) "Browse requested");
    g_signal_connect (drop, "files-dropped",
                      G_CALLBACK (drop_files_cb), state);
    g_signal_connect_swapped (drop, "rejected",
                              G_CALLBACK (show_toast_static_cb),
                              (gpointer) "Those files are not audio");
    g_object_set_data (G_OBJECT (drop), "showcase-state", state);
    gtk_box_append (GTK_BOX (media), make_plain_group (form_media));
    gtk_box_append (GTK_BOX (media), drop);
    gtk_box_append (GTK_BOX (body), media);
  }
  append_section (body, "Page states",
                  "Empty, loading and recoverable error states share the same "
                  "typography and action rhythm without local composition.");
  awra_empty_state_set_title (AWRA_EMPTY_STATE (empty), "No projects");
  awra_empty_state_set_description (AWRA_EMPTY_STATE (empty),
                                    "Create a project to begin.");
  awra_empty_state_set_action (AWRA_EMPTY_STATE (empty),
                               make_button ("Create", AWRA_BUTTON_APPEARANCE_PRIMARY));
  awra_loading_state_set_title (AWRA_LOADING_STATE (loading), "Loading");
  awra_loading_state_set_description (AWRA_LOADING_STATE (loading),
                                      "Fetching the fake library.");
  awra_error_state_set_title (AWRA_ERROR_STATE (error), "Could not sync");
  awra_error_state_set_description (AWRA_ERROR_STATE (error),
                                    "The demo service is intentionally unavailable.");
  awra_error_state_set_action (AWRA_ERROR_STATE (error),
                               make_button ("Retry", AWRA_BUTTON_APPEARANCE_SECONDARY));
  gtk_widget_set_hexpand (empty, TRUE);
  gtk_widget_set_hexpand (loading, TRUE);
  gtk_widget_set_hexpand (error, TRUE);
  gtk_box_append (GTK_BOX (states), empty);
  gtk_box_append (GTK_BOX (states), loading);
  gtk_box_append (GTK_BOX (states), error);
  gtk_box_append (GTK_BOX (body), states);
  return page;
}

static void
navigation_push_cb (GtkButton *button,
                    gpointer   user_data)
{
  AwraNavigationView *navigation = user_data;
  const char *name = g_object_get_data (G_OBJECT (button), "destination");

  awra_navigation_view_push (navigation, name);
}

static void
navigation_pop_cb (GtkButton *button,
                   gpointer   user_data)
{
  (void) button;
  awra_navigation_view_pop (AWRA_NAVIGATION_VIEW (user_data));
}

static gboolean
palette_activate_cb (AwraCommandPalette *palette,
                     const char         *action,
                     GVariant           *target,
                     ShowcaseState      *state)
{
  g_autofree char *message = g_strdup_printf ("Command activated: %s", action);
  (void) palette;
  (void) target;
  show_toast (state, message);
  return TRUE;
}

static void
open_palette_cb (GtkButton *button,
                 ShowcaseState *state)
{
  g_autoptr (GMenu) menu = g_menu_new ();
  GtkWidget *palette = awra_command_palette_new (GTK_WINDOW (state->window));

  (void) button;
  g_menu_append (menu, "Open recent workspace", "showcase.open-recent");
  g_menu_append (menu, "Create a document", "showcase.create-document");
  g_menu_append (menu, "Toggle inspector", "showcase.toggle-inspector");
  awra_command_palette_set_menu_model (AWRA_COMMAND_PALETTE (palette),
                                       G_MENU_MODEL (menu));
  g_signal_connect (palette, "activate-command",
                    G_CALLBACK (palette_activate_cb), state);
  awra_command_palette_present (AWRA_COMMAND_PALETTE (palette));
}

static GtkWidget *
make_navigation_page (ShowcaseState *state)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Navigation",
    "The main sidebar replaces top-level pages. This page also demonstrates "
    "tabs and an independent push/pop history.", &body);
  GtkWidget *tabs = awra_tab_view_new ();
  GtkWidget *nested = awra_navigation_view_new ();
  GtkWidget *home_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *details_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *push = make_button ("Push details", AWRA_BUTTON_APPEARANCE_PRIMARY);
  GtkWidget *back = make_button ("Back", AWRA_BUTTON_APPEARANCE_GHOST);
  GtkWidget *open_palette = make_button (
    "Open command palette", AWRA_BUTTON_APPEARANCE_SECONDARY);
  GtkWidget *breadcrumbs = awra_breadcrumb_bar_new ();
  g_autoptr (GListStore) breadcrumb_model = g_list_store_new (
    AWRA_TYPE_BREADCRUMB_ITEM);
  g_autoptr (AwraBreadcrumbItem) root_item = awra_breadcrumb_item_new (
    "Workspace", "workspace");
  g_autoptr (AwraBreadcrumbItem) project_item = awra_breadcrumb_item_new (
    "Design system", "project");
  g_autoptr (AwraBreadcrumbItem) page_item = awra_breadcrumb_item_new (
    "Navigation", "navigation");

  g_list_store_append (breadcrumb_model, root_item);
  g_list_store_append (breadcrumb_model, project_item);
  g_list_store_append (breadcrumb_model, page_item);
  awra_breadcrumb_bar_set_model (AWRA_BREADCRUMB_BAR (breadcrumbs),
                                 G_LIST_MODEL (breadcrumb_model));
  gtk_box_append (GTK_BOX (body), breadcrumbs);
  GtkWidget *path_bar = awra_path_bar_new ();
  awra_path_bar_set_model (AWRA_PATH_BAR (path_bar),
                           G_LIST_MODEL (breadcrumb_model));
  awra_path_bar_set_text (AWRA_PATH_BAR (path_bar), "/workspace/awra/navigation");
  awra_path_bar_set_icon_name (AWRA_PATH_BAR (path_bar), "folder-symbolic");
  gtk_box_append (GTK_BOX (body), path_bar);
  gtk_box_append (GTK_BOX (body), open_palette);
  g_signal_connect (open_palette, "clicked", G_CALLBACK (open_palette_cb),
                    state);

  awra_tab_view_add (AWRA_TAB_VIEW (tabs),
                     make_label ("A dashboard tab with stable selection state.",
                                 AWRA_TYPOGRAPHY_BODY),
                     "dashboard", "Dashboard");
  awra_tab_view_add (AWRA_TAB_VIEW (tabs),
                     make_label ("A files tab driven by GtkStack.",
                                 AWRA_TYPOGRAPHY_BODY),
                     "files", "Files");
  awra_tab_view_add (AWRA_TAB_VIEW (tabs),
                     make_label ("A settings tab using the same Awra token set.",
                                 AWRA_TYPOGRAPHY_BODY),
                     "settings", "Settings");
  gtk_widget_set_size_request (tabs, -1, 150);
  gtk_box_append (GTK_BOX (body), tabs);

  append_section (body, "Push / pop flow",
                  "History is explicit and transitions honor reduced motion.");
  awra_widget_set_margin (home_box, AWRA_SPACING_LG);
  awra_widget_set_margin (details_box, AWRA_SPACING_LG);
  gtk_box_append (GTK_BOX (home_box), make_label (
    "Project home", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (home_box), make_label (
    "Open a nested details page without changing the top-level sidebar.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (home_box), push);
  gtk_box_append (GTK_BOX (details_box), make_label (
    "Project details", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (details_box), make_label (
    "Pop restores the previous page and its focus context.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (details_box), back);
  awra_navigation_view_add (AWRA_NAVIGATION_VIEW (nested), home_box, "home");
  awra_navigation_view_add (AWRA_NAVIGATION_VIEW (nested), details_box, "details");
  awra_navigation_view_push (AWRA_NAVIGATION_VIEW (nested), "home");
  g_object_set_data (G_OBJECT (push), "destination", "details");
  g_signal_connect (push, "clicked", G_CALLBACK (navigation_push_cb), nested);
  g_signal_connect (back, "clicked", G_CALLBACK (navigation_pop_cb), nested);
  gtk_widget_set_size_request (nested, -1, 230);
  gtk_box_append (GTK_BOX (body), make_card (nested));
  return page;
}

static void
toast_clicked_cb (GtkButton *button,
                  gpointer   user_data)
{
  (void) button;
  show_toast (user_data, "Saved inside the current Awra window");
}

static void
undo_toast_clicked_cb (GtkButton *button,
                       gpointer   user_data)
{
  ShowcaseState *state = user_data;

  (void) button;
  awra_toast_overlay_show_with_action (state->toast, "Sound removed from F5",
                                       "Undo", "showcase.undo", 5000);
}

static void
dialog_clicked_cb (GtkButton *button,
                   gpointer   user_data)
{
  ShowcaseState *state = user_data;
  GtkWidget *dialog = awra_dialog_new (GTK_WINDOW (state->window));
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *actions = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *cancel = make_button ("Cancel", AWRA_BUTTON_APPEARANCE_GHOST);
  GtkWidget *confirm = make_button ("Confirm", AWRA_BUTTON_APPEARANCE_PRIMARY);

  (void) button;
  gtk_window_set_title (GTK_WINDOW (dialog), "Demo action");
  awra_widget_set_margin (box, AWRA_SPACING_XL);
  gtk_box_append (GTK_BOX (box), make_label (
    "Confirm demo action", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (box), make_label (
    "This action demonstrates a compact native Awra dialog with a clear "
    "decision hierarchy.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_widget_set_halign (actions, GTK_ALIGN_END);
  gtk_box_append (GTK_BOX (actions), cancel);
  gtk_box_append (GTK_BOX (actions), confirm);
  gtk_box_append (GTK_BOX (box), actions);
  awra_window_set_content (AWRA_WINDOW (dialog), box);
  g_signal_connect_swapped (cancel, "clicked",
                            G_CALLBACK (gtk_window_destroy), dialog);
  g_signal_connect_swapped (confirm, "clicked",
                            G_CALLBACK (gtk_window_destroy), dialog);
  gtk_window_present (GTK_WINDOW (dialog));
}

static GtkWidget *
make_overlays_page (ShowcaseState *state,
                    GMenuModel    *menu)
{
  static const char *choices[] = { "Compact", "Medium", "Expanded", NULL };
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Overlays",
    "Open and dismiss every overlay repeatedly. Right-click the action card "
    "for the same GMenuModel exposed by the menu button.", &body);
  GtkWidget *actions = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *menu_button = awra_menu_button_new ();
  GtkWidget *custom_button = awra_menu_button_new ();
  GtkWidget *custom_popover = awra_popover_new ();
  GtkWidget *popover_box = make_box (GTK_ORIENTATION_VERTICAL,
                                     AWRA_SPACING_NONE);
  GtkWidget *toast = make_button ("Show toast", AWRA_BUTTON_APPEARANCE_SECONDARY);
  GtkWidget *undo_toast = make_button ("Toast with Undo",
                                       AWRA_BUTTON_APPEARANCE_SECONDARY);
  GtkWidget *dialog = make_button ("Open dialog", AWRA_BUTTON_APPEARANCE_PRIMARY);
  GtkWidget *tooltip = make_button ("Hover for tooltip", AWRA_BUTTON_APPEARANCE_GHOST);
  GtkWidget *card;

  awra_menu_button_set_label (AWRA_MENU_BUTTON (menu_button), "Application menu");
  awra_menu_button_set_menu_model (AWRA_MENU_BUTTON (menu_button), menu);
  awra_box_apply_layout_preset (GTK_BOX (popover_box),
                                AWRA_LAYOUT_PRESET_OVERLAY);
  gtk_box_append (GTK_BOX (popover_box), make_label (
    "Project summary", AWRA_TYPOGRAPHY_TITLE_3));
  gtk_box_append (GTK_BOX (popover_box), make_label (
    "A native AwraPopover can hold arbitrary interactive content.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (popover_box), awra_switch_new ());
  awra_popover_set_child (AWRA_POPOVER (custom_popover), popover_box);
  awra_menu_button_set_label (AWRA_MENU_BUTTON (custom_button), "Custom popover");
  awra_menu_button_set_popover (AWRA_MENU_BUTTON (custom_button),
                                AWRA_POPOVER (custom_popover));
  g_signal_connect (toast, "clicked", G_CALLBACK (toast_clicked_cb), state);
  g_signal_connect (undo_toast, "clicked", G_CALLBACK (undo_toast_clicked_cb),
                    state);
  g_signal_connect (dialog, "clicked", G_CALLBACK (dialog_clicked_cb), state);
  awra_widget_set_tooltip_text (tooltip,
                                "Tooltip text remains accessible to assistive technology");
  gtk_box_append (GTK_BOX (actions), menu_button);
  gtk_box_append (GTK_BOX (actions), custom_button);
  gtk_box_append (GTK_BOX (actions),
                  awra_dropdown_new_from_strings (choices));
  gtk_box_append (GTK_BOX (actions), toast);
  gtk_box_append (GTK_BOX (actions), undo_toast);
  gtk_box_append (GTK_BOX (actions), dialog);
  gtk_box_append (GTK_BOX (actions), tooltip);
  card = make_plain_group (actions);
  awra_widget_set_context_menu_model (card, menu);
  gtk_box_append (GTK_BOX (body), card);
  append_section (body, "Context menu",
                  "Right-click the card, a material surface, or a row in Overview.");
  gtk_box_append (GTK_BOX (body), make_label (
    "Escape, outside click and keyboard navigation are handled by GTK; native "
    "effect registration remains private to Awra.", AWRA_TYPOGRAPHY_BODY));
  return page;
}

static void
update_motion_label (ShowcaseState *state)
{
  g_autofree char *text = g_strdup_printf (
    "Instant %u ms · Fast %u ms · Normal %u ms · Slow %u ms\n"
    "Reduced motion: %s",
    awra_motion_get_duration (state->style, AWRA_MOTION_PRESET_INSTANT),
    awra_motion_get_duration (state->style, AWRA_MOTION_PRESET_FAST),
    awra_motion_get_duration (state->style, AWRA_MOTION_PRESET_NORMAL),
    awra_motion_get_duration (state->style, AWRA_MOTION_PRESET_SLOW),
    awra_style_manager_get_reduced_motion (state->style) ? "yes" : "no");

  if (state->motion_label != NULL)
    gtk_label_set_text (state->motion_label, text);
}

static void
motion_switch_notify_cb (AwraSwitch *switcher,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  ShowcaseState *state = user_data;

  (void) pspec;
  awra_style_manager_set_reduced_motion (
    state->style, awra_switch_get_active (switcher));
  update_motion_label (state);
}

static void
spring_target_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
  AwraSpringAnimation *spring = user_data;
  int position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "spring-position"));

  /* Wherever the knob is and however fast it moves, it bends towards the
   * new place: click quickly to interrupt it. */
  awra_spring_animation_retarget (spring, position);
}

static GtkWidget *
make_spring_demo (void)
{
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *track = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_NONE);
  GtkWidget *buttons = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  GtkWidget *knob = awra_avatar_new (40, "Spring");
  AwraAnimation *spring;
  static const struct {
    const char *label;
    int position;
  } targets[] = { { "Start", 0 }, { "Middle", 180 }, { "End", 360 } };

  gtk_widget_set_halign (knob, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (track), knob);
  gtk_widget_set_size_request (track, 400, -1);
  spring = awra_spring_animation_new (knob, 0, 0, 0.72, 260.0,
                                      awra_property_animation_target_new (G_OBJECT (knob),
                                                                          "margin-start"));
  awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (spring), 0.5);
  g_object_set_data_full (G_OBJECT (box), "spring", spring, g_object_unref);
  for (guint i = 0; i < G_N_ELEMENTS (targets); i++) {
    GtkWidget *button = make_button (targets[i].label, AWRA_BUTTON_APPEARANCE_SECONDARY);

    g_object_set_data (G_OBJECT (button), "spring-position",
                       GINT_TO_POINTER (targets[i].position));
    g_signal_connect (button, "clicked", G_CALLBACK (spring_target_clicked_cb), spring);
    gtk_box_append (GTK_BOX (buttons), button);
  }
  gtk_box_append (GTK_BOX (box), track);
  gtk_box_append (GTK_BOX (box), buttons);
  return box;
}

static GtkWidget *
make_motion_page (ShowcaseState *state)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Motion",
    "Durations are semantic and become zero when reduced motion is enabled. "
    "Navigation and tab transitions retarget without application CSS.", &body);
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *skeleton = awra_skeleton_new ();
  GtkWidget *switcher = awra_switch_new ();
  GtkWidget *progress = awra_progress_bar_new ();

  state->motion_label = GTK_LABEL (make_label (NULL, AWRA_TYPOGRAPHY_BODY));
  awra_switch_set_active (AWRA_SWITCH (switcher),
                          awra_style_manager_get_reduced_motion (state->style));
  awra_progress_bar_set_fraction (AWRA_PROGRESS_BAR (progress), 0.72);
  awra_progress_bar_set_show_text (AWRA_PROGRESS_BAR (progress), TRUE);
  awra_progress_bar_set_text (AWRA_PROGRESS_BAR (progress), "Retargetable progress");
  gtk_box_append (GTK_BOX (box), make_label (
    "Reduced motion", AWRA_TYPOGRAPHY_TITLE_3));
  gtk_box_append (GTK_BOX (box), switcher);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (state->motion_label));
  awra_skeleton_set_lines (AWRA_SKELETON (skeleton), 4);
  gtk_widget_set_size_request (skeleton, 420, -1);
  gtk_box_append (GTK_BOX (box), skeleton);
  gtk_box_append (GTK_BOX (box), progress);
  g_signal_connect (switcher, "notify::active",
                    G_CALLBACK (motion_switch_notify_cb), state);
  update_motion_label (state);
  gtk_box_append (GTK_BOX (body), make_plain_group (box));
  append_section (body, "Springs and interruption",
                  "An AwraSpringAnimation drives the avatar through a property "
                  "target. Retargeting keeps its value and velocity, so clicking "
                  "another place mid-flight bends the motion instead of restarting it.");
  gtk_box_append (GTK_BOX (body), make_plain_group (make_spring_demo ()));
  return page;
}

static void
panel_toggled_cb (GtkToggleButton *button,
                  gpointer         user_data)
{
  awra_split_view_set_show_sidebar (
    AWRA_SPLIT_VIEW (user_data), gtk_toggle_button_get_active (button));
}

static void
persistent_panel_mode_changed_cb (AwraSwitch *switcher,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  AwraEdgePanel *edge = AWRA_EDGE_PANEL (user_data);
  gboolean pinned = awra_switch_get_active (switcher);

  (void) pspec;
  awra_edge_panel_set_auto_hide (edge, !pinned);
  if (pinned)
    awra_edge_panel_set_revealed (edge, TRUE);
}

static GtkWidget *
make_persistent_edge_panel_demo (void)
{
  const guint panel_width = 300;
  GtkWidget *edge = awra_edge_panel_new ();
  GtkWidget *content = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *panel = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *panel_header = make_box (GTK_ORIENTATION_HORIZONTAL,
                                      AWRA_SPACING_SM);
  GtkWidget *panel_title = make_label ("Layers", AWRA_TYPOGRAPHY_TITLE_2);
  GtkWidget *preview = awra_card_new ();
  GtkWidget *preview_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *preview_actions = make_box (GTK_ORIENTATION_HORIZONTAL,
                                         AWRA_SPACING_SM);
  GtkWidget *progress = awra_progress_bar_new ();
  GtkWidget *pin_row = awra_action_row_new (
    "Keep panel visible", "Disable to reveal it from the edge");
  GtkWidget *pin_switch = awra_switch_new ();
  GtkWidget *zoom = awra_slider_new_with_range (
    GTK_ORIENTATION_HORIZONTAL, 25, 200, 5);
  GtkWidget *layers = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_XS);
  GtkWidget *foreground = awra_action_row_new ("Foreground", "Visible");
  GtkWidget *artwork = awra_action_row_new ("Artwork", "Selected · 72%");
  GtkWidget *background = awra_action_row_new ("Background", "Locked");

  awra_widget_set_margin (content, AWRA_SPACING_XL);
  gtk_widget_set_margin_start (
    content,
    (int) panel_width +
      2 * awra_spacing_resolve (content, AWRA_SPACING_XXL));
  gtk_box_append (GTK_BOX (content), make_label (
    "Canvas preview", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (content), make_label (
    "The working area remains visually continuous while a semantic Floating "
    "surface carries contextual tools above its edge.", AWRA_TYPOGRAPHY_MUTED));
  awra_card_set_appearance (AWRA_CARD (preview),
                            AWRA_CARD_APPEARANCE_RAISED);
  gtk_box_append (GTK_BOX (preview_box), make_label (
    "MIDNIGHT STUDY", AWRA_TYPOGRAPHY_EYEBROW));
  gtk_box_append (GTK_BOX (preview_box), make_label (
    "Editorial artwork", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (preview_box), make_label (
    "A believable canvas makes the inspector's depth and placement "
    "immediately understandable.", AWRA_TYPOGRAPHY_BODY));
  awra_progress_bar_set_fraction (AWRA_PROGRESS_BAR (progress), 0.72);
  gtk_box_append (GTK_BOX (preview_box), progress);
  gtk_box_append (GTK_BOX (preview_actions), make_button (
    "Preview", AWRA_BUTTON_APPEARANCE_PRIMARY));
  gtk_box_append (GTK_BOX (preview_actions), make_button (
    "Export", AWRA_BUTTON_APPEARANCE_SECONDARY));
  gtk_box_append (GTK_BOX (preview_box), preview_actions);
  awra_card_set_child (AWRA_CARD (preview), preview_box);
  gtk_box_append (GTK_BOX (content), preview);

  awra_widget_set_margin (panel, AWRA_SPACING_LG);
  gtk_widget_set_hexpand (panel_title, TRUE);
  gtk_box_append (GTK_BOX (panel_header), panel_title);
  gtk_box_append (GTK_BOX (panel_header), awra_badge_new ("FLOATING"));
  gtk_box_append (GTK_BOX (panel), panel_header);
  awra_action_row_set_icon_name (AWRA_ACTION_ROW (pin_row),
                                 "view-pin-symbolic");
  awra_switch_set_active (AWRA_SWITCH (pin_switch), TRUE);
  awra_action_row_set_suffix (AWRA_ACTION_ROW (pin_row), pin_switch);
  gtk_box_append (GTK_BOX (panel), pin_row);
  gtk_box_append (GTK_BOX (panel), make_label ("Zoom",
                                             AWRA_TYPOGRAPHY_TITLE_3));
  gtk_range_set_value (GTK_RANGE (zoom), 100);
  gtk_box_append (GTK_BOX (panel), zoom);
  awra_action_row_set_icon_name (AWRA_ACTION_ROW (foreground),
                                 "view-visible-symbolic");
  awra_action_row_set_suffix (AWRA_ACTION_ROW (foreground),
                              awra_badge_new ("100%"));
  awra_action_row_set_icon_name (AWRA_ACTION_ROW (artwork),
                                 "applications-graphics-symbolic");
  awra_action_row_set_icon_name (AWRA_ACTION_ROW (background),
                                 "changes-prevent-symbolic");
  gtk_box_append (GTK_BOX (layers), foreground);
  gtk_box_append (GTK_BOX (layers), artwork);
  gtk_box_append (GTK_BOX (layers), background);
  gtk_box_append (GTK_BOX (panel), layers);
  awra_edge_panel_set_content (AWRA_EDGE_PANEL (edge), content);
  awra_edge_panel_set_panel (AWRA_EDGE_PANEL (edge), panel);
  awra_edge_panel_set_edge (AWRA_EDGE_PANEL (edge), AWRA_PANEL_EDGE_START);
  awra_edge_panel_set_panel_width (AWRA_EDGE_PANEL (edge), panel_width);
  awra_edge_panel_set_auto_hide (AWRA_EDGE_PANEL (edge), FALSE);
  awra_edge_panel_set_revealed (AWRA_EDGE_PANEL (edge), TRUE);
  gtk_widget_set_size_request (edge, -1, 380);
  g_signal_connect (pin_switch, "notify::active",
                    G_CALLBACK (persistent_panel_mode_changed_cb), edge);
  return edge;
}

static GtkWidget *
make_layout_page (void)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Layout",
    "Resize the window through compact, medium and expanded modes. Sidebar "
    "collapse never mutates the root material or blur request.", &body);
  GtkWidget *split = awra_split_view_new ();
  GtkWidget *panel = awra_sidebar_new ();
  GtkWidget *panel_box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *workspace = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *toggle = awra_toggle_button_new_with_label ("Show side panel");
  GtkWidget *floating;

  awra_widget_set_margin (panel_box, AWRA_SPACING_LG);
  gtk_box_append (GTK_BOX (panel_box), make_label (
    "Inspector panel", AWRA_TYPOGRAPHY_TITLE_3));
  gtk_box_append (GTK_BOX (panel_box), make_label (
    "Secondary controls live beside the primary workspace.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_box_append (GTK_BOX (panel_box), awra_slider_new_with_range (
    GTK_ORIENTATION_HORIZONTAL, 0, 100, 1));
  awra_sidebar_set_child (AWRA_SIDEBAR (panel), panel_box);
  gtk_box_append (GTK_BOX (workspace), make_label (
    "Responsive canvas", AWRA_TYPOGRAPHY_TITLE_2));
  gtk_box_append (GTK_BOX (workspace), make_label (
    "The content remains usable while the panel collapses at narrow widths.",
    AWRA_TYPOGRAPHY_BODY));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
  gtk_box_append (GTK_BOX (workspace), toggle);
  floating = awra_surface_new_with_role (AWRA_SURFACE_ROLE_FLOATING);
  awra_surface_set_elevation_level (AWRA_SURFACE (floating),
                                    AWRA_ELEVATION_FLOATING);
  awra_surface_set_child (AWRA_SURFACE (floating),
                          make_label ("Floating surface · semantic elevation",
                                      AWRA_TYPOGRAPHY_BODY));
  awra_widget_set_margin (
    awra_surface_get_child (AWRA_SURFACE (floating)), AWRA_SPACING_LG);
  gtk_box_append (GTK_BOX (workspace), floating);
  awra_split_view_set_sidebar (AWRA_SPLIT_VIEW (split), panel);
  awra_split_view_set_content (AWRA_SPLIT_VIEW (split), make_card (workspace));
  gtk_widget_set_size_request (split, -1, 330);
  g_signal_connect (toggle, "toggled", G_CALLBACK (panel_toggled_cb), split);
  gtk_box_append (GTK_BOX (body), split);
  append_section (body, "Floating side panel",
                  "The same public edge-panel primitive can remain visible "
                  "above content, or become an edge-revealed inspector as "
                  "shown in Application Patterns.");
  gtk_box_append (GTK_BOX (body), make_persistent_edge_panel_demo ());
  return page;
}

static void
update_platform_label (ShowcaseState *state)
{
  const char *fallback = awra_diagnostics_get_fallback_reason (state->diagnostics);
  g_autofree char *text = g_strdup_printf (
    "Session: %s\nGDK backend: %s\nEffect backend: %s\n"
    "Wayland interface announced: %s\nBlur capability active: %s\n"
    "Native blur enabled by application: %s\n"
    "Native surfaces tracked: %u\nEffects applied: %u\n"
    "Effect-region updates: %" G_GUINT64_FORMAT "\n"
    "Regions:\n%s\nFallback: %s",
    awra_diagnostics_get_session_type (state->diagnostics),
    awra_diagnostics_get_gdk_backend (state->diagnostics),
    awra_diagnostics_get_effect_backend (state->diagnostics),
    awra_diagnostics_get_interface_announced (state->diagnostics) ? "yes" : "no",
    (awra_diagnostics_get_capabilities (state->diagnostics) &
     AWRA_EFFECT_CAPABILITY_BLUR) != 0 ? "yes" : "no",
    awra_style_manager_get_native_blur_enabled (state->style) ? "yes" : "no",
    awra_diagnostics_get_native_surfaces (state->diagnostics),
    awra_diagnostics_get_effects_applied (state->diagnostics),
    awra_diagnostics_get_effect_region_updates (state->diagnostics),
    awra_diagnostics_get_effect_region_summary (state->diagnostics),
    fallback != NULL ? fallback : "none");

  if (state->platform_label != NULL)
    gtk_label_set_text (state->platform_label, text);
}

static void
diagnostics_notify_cb (AwraDiagnostics *diagnostics,
                       GParamSpec      *pspec,
                       gpointer         user_data)
{
  ShowcaseState *state = g_object_get_data (G_OBJECT (user_data),
                                            "showcase-state");

  (void) diagnostics;
  (void) pspec;
  update_platform_label (state);
}

static void
inspector_toggled_cb (GtkCheckButton *button,
                      gpointer        user_data)
{
  awra_inspector_set_effect_region_visible (
    AWRA_INSPECTOR (user_data), gtk_check_button_get_active (button));
}

static void
overdraw_toggled_cb (GtkCheckButton *button,
                     gpointer        user_data)
{
  awra_inspector_set_overdraw_visible (
    AWRA_INSPECTOR (user_data), gtk_check_button_get_active (button));
}

typedef enum {
  TUNING_TINT_RED,
  TUNING_TINT_GREEN,
  TUNING_TINT_BLUE,
  TUNING_CANVAS_OPACITY,
  TUNING_GRAIN,
} TuningChannel;

static void
update_material_tuning_controls (ShowcaseState *state)
{
  const GdkRGBA *tint;
  g_autoptr (AwraMaterial) canvas = NULL;
  g_autoptr (AwraMaterialResolution) resolved = NULL;
  g_autofree char *text = NULL;
  int red;
  int green;
  int blue;

  if (state->material_tuning_label == NULL)
    return;
  canvas = awra_material_new_for_preset (AWRA_MATERIAL_PRESET_CANVAS);
  tint = awra_style_manager_get_canvas_tint (state->style);
  state->syncing_material_tuning = TRUE;
  gtk_range_set_value (state->tint_red_slider, tint->red * 255.0);
  gtk_range_set_value (state->tint_green_slider, tint->green * 255.0);
  gtk_range_set_value (state->tint_blue_slider, tint->blue * 255.0);
  gtk_range_set_value (state->canvas_opacity_slider, tint->alpha * 100.0);
  gtk_range_set_value (state->grain_slider,
                       awra_style_manager_get_material_grain_scale (
                         state->style, canvas) *
                       100.0);
  awra_switch_set_active (
    state->native_blur_switch,
    awra_style_manager_get_native_blur_enabled (state->style));
  state->syncing_material_tuning = FALSE;

  resolved = awra_context_resolve_material (
    state->context, canvas, AWRA_SURFACE_ROLE_WINDOW, TRUE,
    AWRA_ELEVATION_FLAT);
  red = (int) (tint->red * 255.0 + 0.5);
  green = (int) (tint->green * 255.0 + 0.5);
  blue = (int) (tint->blue * 255.0 + 0.5);
  text = g_strdup_printf (
    "Canvas tint: #%02X%02X%02X · opacity: %.0f%% · wallpaper contribution: ~%.0f%%\n"
    "Grain: %.0f%% of preset (Canvas effective alpha %.4f)\n"
    "Native blur: %s · resolved request: %s · fallback: %s\n"
    "Blur strength/radius: compositor-controlled; ext-background-effect-v1 "
    "only exposes activation and region.",
    red, green, blue,
    tint->alpha * 100.0,
    (1.0 - tint->alpha) * 100.0,
    awra_style_manager_get_material_grain_scale (
      state->style, canvas) * 100.0,
    0.016 * awra_style_manager_get_material_grain_scale (
      state->style, canvas),
    awra_style_manager_get_native_blur_enabled (state->style)
      ? "enabled" : "disabled",
    awra_material_resolution_get_request_blur (resolved) ? "yes" : "no",
    awra_material_resolution_get_fallback (resolved) ? "yes" : "no");
  gtk_label_set_text (state->material_tuning_label, text);
}

static void
material_tuning_value_changed_cb (GtkRange *range,
                                  gpointer  user_data)
{
  ShowcaseState *state = user_data;
  TuningChannel channel = GPOINTER_TO_INT (
    g_object_get_data (G_OBJECT (range), "tuning-channel"));
  GdkRGBA tint;
  double value;

  if (state->syncing_material_tuning)
    return;
  value = gtk_range_get_value (range);
  if (channel == TUNING_GRAIN) {
    g_autoptr (AwraMaterial) canvas = awra_material_new_for_preset (
      AWRA_MATERIAL_PRESET_CANVAS);

    awra_style_manager_set_material_grain_scale (
      state->style, canvas, value / 100.0);
    return;
  }

  tint = *awra_style_manager_get_canvas_tint (state->style);
  switch (channel) {
  case TUNING_TINT_RED:
    tint.red = value / 255.0;
    break;
  case TUNING_TINT_GREEN:
    tint.green = value / 255.0;
    break;
  case TUNING_TINT_BLUE:
    tint.blue = value / 255.0;
    break;
  case TUNING_CANVAS_OPACITY:
    tint.alpha = value / 100.0;
    break;
  case TUNING_GRAIN:
  default:
    g_assert_not_reached ();
  }
  awra_style_manager_set_canvas_tint (state->style, &tint);
}

static void
native_blur_toggled_cb (AwraSwitch *switcher,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  ShowcaseState *state = user_data;

  (void) pspec;
  if (!state->syncing_material_tuning)
    awra_style_manager_set_native_blur_enabled (
      state->style, awra_switch_get_active (switcher));
}

static void
reset_material_tuning_clicked_cb (GtkButton *button,
                                  gpointer   user_data)
{
  ShowcaseState *state = user_data;

  (void) button;
  awra_style_manager_reset_material_tuning (state->style);
  show_toast (state, "Material tuning reset to Awra defaults");
}

static GtkWidget *
make_tuning_slider_row (const char     *label,
                        double          maximum,
                        double          step,
                        TuningChannel   channel,
                        GtkRange      **range_out,
                        ShowcaseState  *state)
{
  GtkWidget *row = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_MD);
  GtkWidget *title = make_label (label, AWRA_TYPOGRAPHY_BODY);
  GtkWidget *slider = awra_slider_new_with_range (
    GTK_ORIENTATION_HORIZONTAL, 0.0, maximum, step);

  gtk_widget_set_size_request (title, 150, -1);
  gtk_widget_set_hexpand (slider, TRUE);
  g_object_set_data (G_OBJECT (slider), "tuning-channel",
                     GINT_TO_POINTER (channel));
  g_signal_connect (slider, "value-changed",
                    G_CALLBACK (material_tuning_value_changed_cb), state);
  gtk_box_append (GTK_BOX (row), title);
  gtk_box_append (GTK_BOX (row), slider);
  *range_out = GTK_RANGE (slider);
  return row;
}

static GtkWidget *
make_material_tuning_lab (ShowcaseState *state)
{
  GtkWidget *box = make_box (GTK_ORIENTATION_VERTICAL, AWRA_SPACING_MD);
  GtkWidget *blur_row = make_box (GTK_ORIENTATION_HORIZONTAL,
                                  AWRA_SPACING_MD);
  GtkWidget *blur_label = make_label ("Native background blur",
                                      AWRA_TYPOGRAPHY_BODY);
  GtkWidget *blur_switch = awra_switch_new ();
  GtkWidget *reset = make_button ("Reset Awra defaults",
                                  AWRA_BUTTON_APPEARANCE_SECONDARY);

  gtk_box_append (GTK_BOX (box), make_tuning_slider_row (
    "Tint · red", 255.0, 1.0, TUNING_TINT_RED,
    &state->tint_red_slider, state));
  gtk_box_append (GTK_BOX (box), make_tuning_slider_row (
    "Tint · green", 255.0, 1.0, TUNING_TINT_GREEN,
    &state->tint_green_slider, state));
  gtk_box_append (GTK_BOX (box), make_tuning_slider_row (
    "Tint · blue", 255.0, 1.0, TUNING_TINT_BLUE,
    &state->tint_blue_slider, state));
  gtk_box_append (GTK_BOX (box), make_tuning_slider_row (
    "Canvas opacity", 100.0, 1.0, TUNING_CANVAS_OPACITY,
    &state->canvas_opacity_slider, state));
  gtk_box_append (GTK_BOX (box), make_tuning_slider_row (
    "Grain strength", 300.0, 5.0, TUNING_GRAIN,
    &state->grain_slider, state));

  gtk_widget_set_size_request (blur_label, 150, -1);
  gtk_widget_set_hexpand (blur_label, TRUE);
  state->native_blur_switch = AWRA_SWITCH (blur_switch);
  g_signal_connect (blur_switch, "notify::active",
                    G_CALLBACK (native_blur_toggled_cb), state);
  gtk_box_append (GTK_BOX (blur_row), blur_label);
  gtk_box_append (GTK_BOX (blur_row), blur_switch);
  gtk_box_append (GTK_BOX (box), blur_row);

  state->material_tuning_label = GTK_LABEL (make_label (
    NULL, AWRA_TYPOGRAPHY_MONOSPACE));
  gtk_label_set_selectable (state->material_tuning_label, TRUE);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (state->material_tuning_label));
  gtk_widget_set_halign (reset, GTK_ALIGN_START);
  g_signal_connect (reset, "clicked",
                    G_CALLBACK (reset_material_tuning_clicked_cb), state);
  gtk_box_append (GTK_BOX (box), reset);
  update_material_tuning_controls (state);
  return box;
}

static GtkWidget *
make_platform_page (ShowcaseState *state)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Platform & Blur",
    "Tune Awra's public material parameters live and inspect the native "
    "effect. Showcase never branches on a compositor name or calls Wayland "
    "or KWin.", &body);
  GtkWidget *inspector = awra_inspector_new ();
  GtkWidget *toggle = awra_check_button_new_with_label (
    "Visualize the root native-effect region");
  GtkWidget *overdraw = awra_check_button_new_with_label (
    "Visualize inspected-surface overdraw");

  append_section (body, "Live material lab",
                  "These controls change framework tokens globally; there is "
                  "no Showcase CSS override.");
  gtk_box_append (GTK_BOX (body), make_card (
    make_material_tuning_lab (state)));
  append_section (body, "Native diagnostics",
                  "Capability and applied-effect counts come from Awra.");
  state->platform_label = GTK_LABEL (make_label (
    NULL, AWRA_TYPOGRAPHY_BODY));
  gtk_label_set_selectable (state->platform_label, TRUE);
  update_platform_label (state);
  gtk_box_append (GTK_BOX (body), make_plain_group (GTK_WIDGET (state->platform_label)));
  append_section (body, "Effect-region inspector",
                  "The outline is diagnostic only and does not alter submitted geometry.");
  awra_inspector_set_target (AWRA_INSPECTOR (inspector), GTK_WIDGET (state->window));
  g_signal_connect (toggle, "toggled", G_CALLBACK (inspector_toggled_cb), inspector);
  g_signal_connect (overdraw, "toggled",
                    G_CALLBACK (overdraw_toggled_cb), inspector);
  gtk_box_append (GTK_BOX (body), toggle);
  gtk_box_append (GTK_BOX (body), overdraw);
  gtk_box_append (GTK_BOX (body), inspector);
  return page;
}

static char *
format_rgba (const GdkRGBA *color)
{
  return g_strdup_printf ("rgba(%.3f, %.3f, %.3f, %.3f)",
                          color->red, color->green, color->blue, color->alpha);
}

static void
update_resolution_label (ShowcaseState *state)
{
  g_autoptr (AwraMaterial) material = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_CANVAS);
  g_autoptr (AwraMaterialResolution) active = NULL;
  g_autoptr (AwraMaterialResolution) inactive = NULL;
  AwraTokenSet *tokens = awra_style_manager_get_token_set (state->style);
  g_autofree char *active_fill = NULL;
  g_autofree char *inactive_fill = NULL;
  g_autofree char *active_border = NULL;
  g_autofree char *inactive_border = NULL;
  g_autofree char *active_highlight = NULL;
  g_autofree char *inactive_highlight = NULL;
  g_autofree char *active_shadow = NULL;
  g_autofree char *inactive_shadow = NULL;
  g_autofree char *background = NULL;
  g_autofree char *foreground = NULL;
  g_autofree char *control = NULL;
  g_autofree char *chrome = NULL;
  g_autofree char *layer = NULL;
  g_autofree char *floating = NULL;
  g_autofree char *separator = NULL;
  g_autofree char *focus = NULL;
  g_autofree char *accent = NULL;
  g_autofree char *text = NULL;
  gboolean invariant;

  active = awra_context_resolve_material (
    state->context, material, AWRA_SURFACE_ROLE_WINDOW, TRUE,
    AWRA_ELEVATION_FLOATING);
  inactive = awra_context_resolve_material (
    state->context, material, AWRA_SURFACE_ROLE_WINDOW, FALSE,
    AWRA_ELEVATION_FLOATING);
  active_fill = format_rgba (awra_material_resolution_get_fill (active));
  inactive_fill = format_rgba (awra_material_resolution_get_fill (inactive));
  active_border = format_rgba (awra_material_resolution_get_border (active));
  inactive_border = format_rgba (awra_material_resolution_get_border (inactive));
  active_highlight = format_rgba (awra_material_resolution_get_highlight (active));
  inactive_highlight = format_rgba (awra_material_resolution_get_highlight (inactive));
  active_shadow = format_rgba (awra_material_resolution_get_shadow (active));
  inactive_shadow = format_rgba (awra_material_resolution_get_shadow (inactive));
  background = format_rgba (awra_token_set_get_background (tokens));
  foreground = format_rgba (awra_token_set_get_foreground (tokens));
  control = format_rgba (awra_token_set_get_control_background (tokens));
  chrome = format_rgba (awra_token_set_get_surface_chrome (tokens));
  layer = format_rgba (awra_token_set_get_surface_layer (tokens));
  floating = format_rgba (awra_token_set_get_surface_floating (tokens));
  separator = format_rgba (awra_token_set_get_separator (tokens));
  focus = format_rgba (awra_token_set_get_focus_ring (tokens));
  accent = format_rgba (awra_style_manager_get_accent (state->style));
  invariant = gdk_rgba_equal (awra_material_resolution_get_fill (active),
                              awra_material_resolution_get_fill (inactive)) &&
              awra_material_resolution_get_request_blur (active) ==
              awra_material_resolution_get_request_blur (inactive);
  text = g_strdup_printf (
    "TOKEN SNAPSHOT\n"
    "profile: %s · accent source: %s%s · resolved: %s\n"
    "background: %s · foreground: %s\n"
    "control: %s · focus: %s · window opacity: %.3f · surface opacity: %.3f\n\n"
    "semantic chrome: %s\nsemantic layer: %s\nsemantic floating: %s\n"
    "separator: %s · radius window/large/control: %.0f / %.0f / %.0f px\n\n"
    "CANVAS RESOLUTION\n"
    "                         ACTIVE                              INACTIVE\n"
    "fill/tint      %-36s %s\n"
    "border        %-36s %s\n"
    "highlight     %-36s %s\n"
    "shadow        %-36s %s\n"
    "request blur  %-36s %s\n"
    "fallback      %-36s %s\n\n"
    "INVARIANT fill/tint/opacity + blur request: %s\n"
    "Only decoration alpha changes when inactive: border ×0.94, "
    "highlight ×0.88, shadow ×0.90.",
    awra_token_set_get_dark (tokens) ? "Dark" : "Light",
    awra_style_manager_get_accent_source (state->style) ==
      AWRA_ACCENT_SOURCE_SYSTEM ? "SYSTEM" :
    awra_style_manager_get_accent_source (state->style) ==
      AWRA_ACCENT_SOURCE_CUSTOM ? "CUSTOM" : "DEFAULT",
    awra_style_manager_get_accent_source (state->style) ==
      AWRA_ACCENT_SOURCE_SYSTEM &&
      !awra_style_manager_get_system_accent_available (state->style)
        ? " (DEFAULT fallback)" : "",
    accent,
    background, foreground, control, focus,
    awra_token_set_get_window_opacity (tokens),
    awra_token_set_get_surface_opacity (tokens),
    chrome, layer, floating, separator,
    awra_token_set_get_radius_for_size (tokens, AWRA_RADIUS_WINDOW),
    awra_token_set_get_radius_for_size (tokens, AWRA_RADIUS_LARGE),
    awra_token_set_get_radius_for_size (tokens, AWRA_RADIUS_MEDIUM),
    active_fill, inactive_fill,
    active_border, inactive_border,
    active_highlight, inactive_highlight,
    active_shadow, inactive_shadow,
    awra_material_resolution_get_request_blur (active) ? "yes" : "no",
    awra_material_resolution_get_request_blur (inactive) ? "yes" : "no",
    awra_material_resolution_get_fallback (active) ? "yes" : "no",
    awra_material_resolution_get_fallback (inactive) ? "yes" : "no",
    invariant ? "PASS" : "FAIL");
  if (state->resolution_label != NULL)
    gtk_label_set_text (state->resolution_label, text);
}

static void
style_notify_cb (AwraStyleManager *manager,
                 GParamSpec       *pspec,
                 gpointer          user_data)
{
  ShowcaseState *state = g_object_get_data (G_OBJECT (user_data),
                                            "showcase-state");

  (void) manager;
  (void) pspec;
  update_resolution_label (state);
  update_motion_label (state);
  update_material_tuning_controls (state);
  update_platform_label (state);
}

static GtkWidget *
make_diagnostics_page (ShowcaseState *state)
{
  GtkWidget *body;
  GtkWidget *page = make_page (
    "Diagnostics",
    "Resolved values are compared directly, independently of focus timing or "
    "screenshots. Switch Light/Dark or accessibility preferences in the header.",
    &body);

  state->resolution_label = GTK_LABEL (make_label (
    NULL, AWRA_TYPOGRAPHY_MONOSPACE));
  gtk_label_set_selectable (state->resolution_label, TRUE);
  update_resolution_label (state);
  gtk_box_append (GTK_BOX (body), make_card (GTK_WIDGET (state->resolution_label)));
  append_section (body, "Isolation contract",
                  "Awra identity properties win over GTK themes and USER CSS; "
                  "fonts, scaling, IME and accessibility remain system-owned.");
  gtk_box_append (GTK_BOX (body), make_label (
    "Automated probes compare Breeze, Breeze Dark, hostile XDG user CSS and a "
    "runtime USER-priority provider using an explicitly Light Awra scene.",
    AWRA_TYPOGRAPHY_BODY));
  return page;
}

static gboolean
showcase_smoke_cb (gpointer user_data)
{
  ShowcaseState *state = user_data;
  const GdkRGBA tuned_canvas = { 0.10, 0.16, 0.24, 0.48 };

  for (guint i = 0; i < G_N_ELEMENTS (page_infos); i++) {
    g_assert_true (awra_navigation_view_replace (state->navigation,
                                                 page_infos[i].name));
    g_assert_cmpstr (awra_navigation_view_get_visible_page_name (
                       state->navigation), ==, page_infos[i].name);
  }
  awra_style_manager_set_appearance (state->style, AWRA_APPEARANCE_LIGHT);
  update_resolution_label (state);
  awra_style_manager_set_appearance (state->style, AWRA_APPEARANCE_DARK);
  update_resolution_label (state);
  awra_style_manager_set_canvas_tint (state->style, &tuned_canvas);
  awra_style_manager_set_grain_scale (state->style, 1.75);
  awra_style_manager_set_native_blur_enabled (state->style, FALSE);
  g_assert_cmpfloat_with_epsilon (
    gtk_range_get_value (state->canvas_opacity_slider), 48.0, 0.001);
  g_assert_cmpfloat_with_epsilon (
    gtk_range_get_value (state->grain_slider), 175.0, 0.001);
  g_assert_false (awra_switch_get_active (state->native_blur_switch));
  awra_style_manager_reset_material_tuning (state->style);
  g_assert_cmpfloat_with_epsilon (
    gtk_range_get_value (state->canvas_opacity_slider), 60.0, 0.001);
  g_assert_cmpfloat_with_epsilon (
    gtk_range_get_value (state->grain_slider), 20.0, 0.001);
  g_assert_true (awra_switch_get_active (state->native_blur_switch));
  awra_style_manager_set_appearance (state->style, AWRA_APPEARANCE_SYSTEM);
  show_toast (state, "Showcase smoke test");
  g_application_quit (g_application_get_default ());
  return G_SOURCE_REMOVE;
}

static void
activate_cb (GtkApplication *application,
             gpointer        user_data)
{
  static const GActionEntry actions[] = {
    { "save", action_activated_cb, NULL, NULL, NULL, { 0 } },
    { "duplicate", action_activated_cb, NULL, NULL, NULL, { 0 } },
    { "favorite", action_activated_cb, NULL, NULL, NULL, { 0 } },
    { "share", action_activated_cb, NULL, NULL, NULL, { 0 } },
    { "inspect", action_activated_cb, NULL, NULL, NULL, { 0 } },
    { "remove", action_activated_cb, NULL, NULL, NULL, { 0 } },
    { "undo", action_activated_cb, NULL, NULL, NULL, { 0 } },
  };
  ShowcaseState *state = g_new0 (ShowcaseState, 1);
  g_autoptr (GSimpleActionGroup) action_group = g_simple_action_group_new ();
  g_autoptr (GMenuModel) action_menu = make_action_menu ();
  GtkWidget *window;
  GtkWidget *root_split;
  GtkWidget *sidebar;
  GtkWidget *toolbar;
  GtkWidget *toast;

  (void) user_data;
  awra_init ();
  window = awra_window_new (application);
  state->window = AWRA_WINDOW (window);
  state->context = awra_context_get_for_display (gtk_widget_get_display (window));
  state->style = awra_context_get_style_manager (state->context);
  state->diagnostics = awra_context_get_diagnostics (state->context);
  gtk_window_set_title (GTK_WINDOW (window), "Awra Showcase");
  gtk_window_set_default_size (GTK_WINDOW (window), 1180, 780);
  g_object_set_data_full (G_OBJECT (window), "showcase-state", state, g_free);

  toast = awra_toast_overlay_new ();
  state->toast = AWRA_TOAST_OVERLAY (toast);
  state->navigation = AWRA_NAVIGATION_VIEW (awra_navigation_view_new ());
  state->header = AWRA_HEADER (awra_header_new ());
  sidebar = make_sidebar (state);

  toolbar = make_box (GTK_ORIENTATION_HORIZONTAL, AWRA_SPACING_SM);
  gtk_box_append (GTK_BOX (toolbar), make_appearance_button (
    "System", AWRA_APPEARANCE_SYSTEM, state));
  gtk_box_append (GTK_BOX (toolbar), make_appearance_button (
    "Light", AWRA_APPEARANCE_LIGHT, state));
  gtk_box_append (GTK_BOX (toolbar), make_appearance_button (
    "Dark", AWRA_APPEARANCE_DARK, state));
  gtk_box_append (GTK_BOX (toolbar), make_preferences_button (state));
  awra_header_set_end_widget (state->header, toolbar);
  awra_header_set_blend_with_window (state->header, TRUE);
  awra_header_set_drag_enabled (state->header, TRUE);
  awra_header_set_show_window_controls (state->header, TRUE);
  awra_window_set_chrome (state->window, GTK_WIDGET (state->header));

  g_action_map_add_action_entries (G_ACTION_MAP (action_group), actions,
                                   G_N_ELEMENTS (actions), state);
  gtk_widget_insert_action_group (window, "showcase",
                                  G_ACTION_GROUP (action_group));

  awra_navigation_view_add (state->navigation,
    make_overview_page (state, action_menu), "overview");
  awra_navigation_view_add (state->navigation,
    make_patterns_page (state, action_menu), "patterns");
  awra_navigation_view_add (state->navigation,
    make_surfaces_page (state, action_menu), "surfaces");
  awra_navigation_view_add (state->navigation,
    make_controls_page (), "controls");
  awra_navigation_view_add (state->navigation,
    make_inputs_page (state), "inputs");
  awra_navigation_view_add (state->navigation,
    make_navigation_page (state), "navigation");
  awra_navigation_view_add (state->navigation,
    make_overlays_page (state, action_menu), "overlays");
  awra_navigation_view_add (state->navigation,
    make_motion_page (state), "motion");
  awra_navigation_view_add (state->navigation,
    make_layout_page (), "layout");
  awra_navigation_view_add (state->navigation,
    make_platform_page (state), "platform");
  awra_navigation_view_add (state->navigation,
    make_diagnostics_page (state), "diagnostics");

  gtk_widget_set_vexpand (GTK_WIDGET (state->navigation), TRUE);
  root_split = awra_split_view_new ();
  awra_split_view_set_sidebar (AWRA_SPLIT_VIEW (root_split), sidebar);
  awra_split_view_set_content (AWRA_SPLIT_VIEW (root_split),
                               GTK_WIDGET (state->navigation));
  awra_toast_overlay_set_child (state->toast, root_split);
  awra_window_set_content (state->window, toast);

  g_signal_connect_object (state->diagnostics, "notify",
                           G_CALLBACK (diagnostics_notify_cb), window, 0);
  g_signal_connect_object (state->style, "notify",
                           G_CALLBACK (style_notify_cb), window, 0);
  {
    const char *appearance = g_getenv ("AWRA_SHOWCASE_APPEARANCE");

    if (g_strcmp0 (appearance, "light") == 0)
      awra_style_manager_set_appearance (state->style, AWRA_APPEARANCE_LIGHT);
    else if (g_strcmp0 (appearance, "dark") == 0)
      awra_style_manager_set_appearance (state->style, AWRA_APPEARANCE_DARK);
  }
  {
    const char *accent = g_getenv ("AWRA_SHOWCASE_ACCENT");
    GdkRGBA custom;

    if (g_strcmp0 (accent, "system") == 0)
      awra_style_manager_set_accent_source (state->style,
                                            AWRA_ACCENT_SOURCE_SYSTEM);
    else if (g_strcmp0 (accent, "default") == 0)
      awra_style_manager_set_accent_source (state->style,
                                            AWRA_ACCENT_SOURCE_DEFAULT);
    else if (g_strcmp0 (accent, "blue") == 0) {
      custom = (GdkRGBA) { 0.19, 0.36, 0.94, 1.0 };
      awra_style_manager_set_accent (state->style, &custom);
    } else if (accent != NULL && gdk_rgba_parse (&custom, accent)) {
      awra_style_manager_set_accent (state->style, &custom);
    }
  }
  awra_navigation_item_set_selected (state->first_page_button, TRUE);
  {
    const char *page = g_getenv ("AWRA_SHOWCASE_PAGE");

    for (guint i = 0; page != NULL && i < G_N_ELEMENTS (page_infos); i++) {
      if (g_str_equal (page, page_infos[i].name)) {
        awra_navigation_item_set_selected (state->page_buttons[i], TRUE);
        break;
      }
    }
  }
  gtk_window_present (GTK_WINDOW (window));
  if (g_getenv ("AWRA_SHOWCASE_SMOKE_TEST") != NULL)
    g_idle_add (showcase_smoke_cb, state);
}

int
main (int   argc,
      char *argv[])
{
  GApplicationFlags flags = g_getenv ("AWRA_SHOWCASE_SMOKE_TEST") != NULL ||
                            g_getenv ("AWRA_SHOWCASE_NON_UNIQUE") != NULL
                              ? G_APPLICATION_NON_UNIQUE
                              : G_APPLICATION_DEFAULT_FLAGS;
  g_autoptr (GtkApplication) application = gtk_application_new (
    "org.awra.Showcase", flags);

  g_signal_connect (application, "activate", G_CALLBACK (activate_cb), NULL);
  return g_application_run (G_APPLICATION (application), argc, argv);
}
