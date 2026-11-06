/* Showcase pages, shared by the interactive showcase and the screenshot tool.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "pages.h"

#include <stdio.h>
#include <string.h>

static AwraWidget *label(const char *text, AwraTypography t)
{
  AwraWidget *l = awra_label_new(text);
  awra_widget_set_typography(l, t);
  awra_label_set_xalign(l, 0.0f);
  return l;
}

AwraWidget *showcase_page_basics(void)
{
  AwraWidget *col = awra_box_new(AWRA_ORIENTATION_VERTICAL, 12);
  awra_widget_set_margins(col, 24, 32, 24, 32);
  awra_box_append(col, label("Buttons", AWRA_TYPOGRAPHY_TITLE_2));
  AwraWidget *row = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 8);
  const char *names[] = {"Primary", "Secondary", "Ghost", "Destructive", "Toolbar"};
  for (int i = 0; i < 5; i++) {
    AwraWidget *b = awra_button_new_with_label(names[i]);
    awra_button_set_appearance(b, (AwraButtonAppearance)i);
    awra_box_append(row, b);
  }
  AwraWidget *ib = awra_button_new_from_icon_name("edit-find-symbolic");
  awra_box_append(row, ib);
  AwraWidget *ilb = awra_button_new_with_label("Ajouter des sons");
  awra_button_set_icon_name(ilb, "list-add-symbolic");
  awra_button_set_appearance(ilb, AWRA_BUTTON_PRIMARY);
  awra_box_append(row, ilb);
  awra_box_append(col, row);
  AwraWidget *t = awra_toggle_button_new_with_label("Toggle (on)");
  awra_toggle_button_set_active(t, true);
  awra_box_append(col, t);
  awra_widget_set_halign(t, AWRA_ALIGN_START);
  awra_box_append(col, label("Typography", AWRA_TYPOGRAPHY_TITLE_2));
  const char *roles[] = {"Body text", "Muted text", "EYEBROW", "Title 3", "Title 2", "Title 1",
                         "Display", "Monospace 0123", "Label", "Reading text"};
  for (int i = 0; i < 10; i++) {
    awra_box_append(col, label(roles[i], (AwraTypography)i));
  }
  AwraWidget *wrap = label("Glisse des fichiers audio ou un dossier sur une touche, ou n'importe où dans la "
                           "fenêtre, ou ajoute-les ici (fichiers ou YouTube) : chaque son trouve tout seul "
                           "une touche libre.",
                           AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_wrap(wrap, AWRA_WRAP_WORD);
  awra_label_set_max_width_chars(wrap, 60);
  awra_box_append(col, wrap);
  return col;
}

static AwraWidget *hbox(int sp)
{
  return awra_box_new(AWRA_ORIENTATION_HORIZONTAL, sp);
}

static AwraWidget *vbox(int sp)
{
  return awra_box_new(AWRA_ORIENTATION_VERTICAL, sp);
}

AwraWidget *showcase_page_gallery(void)
{
  AwraWidget *split = awra_split_view_new();
  AwraWidget *side = awra_sidebar_new();
  awra_sidebar_append_heading(side, "SONS");
  AwraWidget *n1 = awra_navigation_item_new("Clavier", "input-keyboard-symbolic");
  AwraWidget *n2 = awra_navigation_item_new("Biblioth\xC3\xA8que", "folder-music-symbolic");
  AwraWidget *n3 = awra_navigation_item_new("D\xC3\xA9" "couvrir", "starred-symbolic");
  awra_navigation_item_set_group(n2, n1);
  awra_navigation_item_set_group(n3, n1);
  awra_navigation_item_set_selected(n1, true);
  awra_sidebar_append(side, n1);
  awra_sidebar_append(side, n2);
  awra_sidebar_append(side, n3);
  awra_sidebar_append_heading(side, "OUTILS");
  awra_sidebar_append(side, awra_navigation_item_new("R\xC3\xA9glages", "emblem-system-symbolic"));
  awra_split_view_set_sidebar(split, side);

  AwraWidget *col = vbox(16);
  awra_widget_set_margins(col, 20, 24, 20, 24);
  AwraWidget *row = hbox(8);
  AwraWidget *search = awra_search_entry_new();
  awra_entry_set_placeholder(search, "Filtrer : nom, t:piste, l:boucle");
  awra_widget_set_hexpand(search, true);
  awra_box_append(row, search);
  AwraWidget *add = awra_button_new_with_label("Ajouter des sons");
  awra_button_set_icon_name(add, "list-add-symbolic");
  awra_button_set_appearance(add, AWRA_BUTTON_PRIMARY);
  awra_box_append(row, add);
  awra_box_append(col, row);

  AwraWidget *kb = awra_keyboard_view_new(AWRA_KEYBOARD_LAYOUT_ISO);
  awra_keyboard_view_set_show_navigation(kb, false);
  AwraWidget *cap = awra_keyboard_view_key_cap(kb, "KeyA");
  awra_key_cap_set_bound(cap, true);
  awra_key_cap_set_secondary_label(cap, "Intro");
  AwraRGBA tint = {0.49f, 0.31f, 0.95f, 0.85f};
  awra_key_cap_set_tint(awra_keyboard_view_key_cap(kb, "KeyS"), &tint);
  awra_key_cap_set_bound(awra_keyboard_view_key_cap(kb, "KeyS"), true);
  awra_key_cap_set_selected(awra_keyboard_view_key_cap(kb, "KeyD"), true);
  awra_box_append(col, kb);

  AwraWidget *controls = hbox(16);
  AwraWidget *sw = awra_switch_new();
  awra_switch_set_active(sw, true);
  awra_box_append(controls, sw);
  awra_box_append(controls, awra_switch_new());
  AwraWidget *ck = awra_check_button_new_with_label("Boucle");
  awra_check_button_set_active(ck, true);
  awra_box_append(controls, ck);
  awra_box_append(controls, awra_check_button_new_with_label("Fondu"));
  AwraWidget *sl = awra_slider_new(0, 1, 0);
  awra_slider_set_value(sl, 0.6);
  awra_widget_set_size_request(sl, 160, -1);
  awra_widget_set_valign(sl, AWRA_ALIGN_CENTER);
  awra_box_append(controls, sl);
  awra_box_append(controls, awra_volume_control_new());
  AwraWidget *seg = awra_segmented_control_new();
  awra_segmented_control_append(seg, "Tout", NULL);
  awra_segmented_control_append(seg, "Musique", NULL);
  awra_segmented_control_append(seg, "Bruitages", NULL);
  awra_box_append(controls, seg);
  awra_box_append(col, controls);

  AwraWidget *row2 = hbox(12);
  const char *items[] = {"Mon profil", "Manga", "Combat"};
  awra_box_append(row2, awra_dropdown_new(items, 3));
  AwraWidget *spin = awra_spin_button_new(0, 5000, 10);
  awra_spin_button_set_value(spin, 250);
  awra_box_append(row2, spin);
  AwraWidget *mb = awra_menu_button_new();
  awra_menu_button_set_label(mb, "Options");
  awra_box_append(row2, mb);
  awra_box_append(row2, awra_badge_new("GRASS", AWRA_BADGE_SUCCESS));
  awra_box_append(row2, awra_badge_new("Info", AWRA_BADGE_INFO));
  awra_box_append(row2, awra_badge_new("3", AWRA_BADGE_ACCENT));
  AwraWidget *cap2 = awra_shortcut_capture_new();
  awra_shortcut_capture_set_placeholder(cap2, "Aucun raccourci");
  awra_box_append(row2, cap2);
  awra_box_append(col, row2);

  AwraWidget *cards = hbox(12);
  AwraWidget *card = awra_card_new();
  AwraWidget *mr = awra_media_row_new("Principale");
  awra_media_row_set_subtitle(mr, "Silence");
  awra_media_row_set_icon_name(mr, "audio-x-generic-symbolic");
  AwraRGBA acc = {0.49f, 0.31f, 0.95f, 1};
  awra_media_row_set_accent_color(mr, &acc);
  awra_media_row_set_show_play_button(mr, false);
  awra_media_row_set_show_volume(mr, true);
  awra_card_set_child(card, mr);
  awra_widget_set_size_request(card, 380, -1);
  awra_box_append(cards, card);
  AwraWidget *banner = awra_status_banner_new("\xC3\x89" "coute en pause", "Les touches ne d\xC3\xA9" "clenchent plus de sons.", AWRA_STATUS_WARNING);
  awra_widget_set_hexpand(banner, true);
  awra_box_append(cards, banner);
  awra_box_append(col, cards);

  AwraWidget *wave = awra_waveform_view_new();
  float peaks[120];
  for (int i = 0; i < 120; i++) {
    peaks[i] = 0.25f + 0.7f * (float)((i * 37 % 23) / 23.0);
  }
  awra_waveform_view_set_peaks(wave, peaks, 120);
  awra_waveform_view_set_duration(wave, 30);
  awra_waveform_view_set_position(wave, 11);
  awra_waveform_view_set_marker_position(wave, 4);
  awra_box_append(col, wave);
  AwraWidget *pb = awra_progress_bar_new();
  awra_progress_bar_set_fraction(pb, 0.35);
  awra_box_append(col, pb);

  AwraWidget *form = awra_form_new();
  AwraWidget *fr = awra_form_row_new("D\xC3\xA9lai entre deux sons (ms)", "Ignore les r\xC3\xA9p\xC3\xA9titions trop rapides.");
  awra_form_row_set_control(fr, awra_spin_button_new(0, 5000, 10));
  awra_form_append(form, fr);
  AwraWidget *ar = awra_action_row_new("Musique seule", "Un morceau \xC3\xA0 la fois, en fondu");
  awra_action_row_set_icon_name(ar, "audio-x-generic-symbolic");
  awra_action_row_set_activatable(ar, true);
  awra_action_row_add_suffix(ar, awra_button_new_with_label("Choisir"));
  awra_form_append(form, ar);
  awra_box_append(col, form);

  AwraWidget *toasts = awra_toast_overlay_new();
  AwraWidget *scroll = awra_scrolled_new();
  awra_scrolled_set_policy(scroll, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  awra_scrolled_set_child(scroll, col);
  awra_toast_overlay_set_child(toasts, scroll);
  awra_toast_overlay_show(toasts, "Profil import\xC3\xA9", 0);
  awra_split_view_set_content(split, toasts);
  return split;
}

AwraWidget *showcase_window(const char *page, bool offscreen, int w, int h, double scale)
{
  AwraWidget *win = offscreen ? awra_window_new_offscreen("Awra Showcase", w, h, scale) :
                                awra_window_new("Awra Showcase", w, h);
  AwraWidget *content = NULL;
  content = page && !strcmp(page, "gallery") ? showcase_page_gallery() : showcase_page_basics();
  awra_window_set_child(win, content);
  return win;
}
