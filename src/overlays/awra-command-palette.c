/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-command-palette.h>
#include <awra/awra-layout.h>
#include <awra/awra-search-entry.h>
#include <awra/awra-surface.h>
#include <awra/awra-typography.h>

#include "awra-i18n-private.h"

typedef struct {
  char *label;
  char *action;
  GVariant *target;
  GIcon *icon;
  char *accel;
  char *detail;
  const char *section; /* owned by AwraCommandPalette.sections */
} PaletteCommand;

struct _AwraCommandPalette {
  AwraWindow parent_instance;
  AwraSearchEntry *search;
  GtkListBox *list;
  GtkLabel *empty;
  GPtrArray *sections;
  GMenuModel *model;
  GPtrArray *commands;
  char *title;
  char *placeholder_text;
  char *empty_text;
  GWeakRef previous_focus;
  gboolean had_focus;
  /* The result Enter opens, marked as such while the focus stays in the
   * search field. Borrowed from the list. */
  GtkWidget *current;
  GtkScrolledWindow *scroller;
};

enum { PROP_0, PROP_MENU_MODEL, PROP_TITLE, PROP_QUERY,
       PROP_PLACEHOLDER_TEXT, PROP_EMPTY_TEXT, N_PROPS };
enum { ACTIVATE_COMMAND, N_SIGNALS };
static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];
G_DEFINE_FINAL_TYPE (AwraCommandPalette, awra_command_palette, AWRA_TYPE_WINDOW)

static void
palette_command_free (gpointer data)
{
  PaletteCommand *command = data;
  g_free (command->label);
  g_free (command->action);
  g_clear_pointer (&command->target, g_variant_unref);
  g_clear_object (&command->icon);
  g_free (command->accel);
  g_free (command->detail);
  g_free (command);
}

static void
collect_commands (AwraCommandPalette *self,
                  GMenuModel         *model,
                  const char         *section_label)
{
  for (int i = 0; i < g_menu_model_get_n_items (model); i++) {
    g_autofree char *label = NULL;
    g_autofree char *action = NULL;
    g_autoptr (GMenuModel) section = NULL;
    g_autoptr (GMenuModel) submenu = NULL;

    section = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
    submenu = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);
    if (section != NULL || submenu != NULL) {
      g_autofree char *heading = NULL;
      const char *child_label = section_label;

      /* A labelled section or submenu becomes a heading in the results. */
      if (g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_LABEL,
                                           "s", &heading) &&
          *heading != '\0') {
        child_label = heading;
        g_ptr_array_add (self->sections, g_steal_pointer (&heading));
      }
      if (section != NULL)
        collect_commands (self, section, child_label);
      if (submenu != NULL)
        collect_commands (self, submenu, child_label);
      continue;
    }
    if (!g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_LABEL,
                                          "s", &label) ||
        !g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_ACTION,
                                          "s", &action))
      continue;
    PaletteCommand *command = g_new0 (PaletteCommand, 1);
    command->label = g_steal_pointer (&label);
    command->action = g_steal_pointer (&action);
    command->target = g_menu_model_get_item_attribute_value (
      model, i, G_MENU_ATTRIBUTE_TARGET, NULL);
    {
      g_autoptr (GVariant) icon = g_menu_model_get_item_attribute_value (
        model, i, G_MENU_ATTRIBUTE_ICON, NULL);
      if (icon != NULL)
        command->icon = g_icon_deserialize (icon);
    }
    g_menu_model_get_item_attribute (model, i, "accel", "s", &command->accel);
    g_menu_model_get_item_attribute (model, i, "awra-detail", "s", &command->detail);
    command->section = section_label;
    g_ptr_array_add (self->commands, command);
  }
}

static gboolean
matches_query (const char *label, const char *query)
{
  g_autofree char *folded_label = NULL;
  g_autofree char *folded_query = NULL;
  if (query == NULL || *query == '\0')
    return TRUE;
  folded_label = g_utf8_casefold (label, -1);
  folded_query = g_utf8_casefold (query, -1);
  return strstr (folded_label, folded_query) != NULL;
}

static void
command_clicked_cb (GtkButton *button, AwraCommandPalette *self)
{
  PaletteCommand *command = g_object_get_data (G_OBJECT (button),
                                                "awra-command");
  gboolean handled = FALSE;

  g_signal_emit (self, signals[ACTIVATE_COMMAND], 0,
                 command->action, command->target, &handled);
  if (!handled)
    gtk_widget_activate_action_variant (GTK_WIDGET (self), command->action,
                                        command->target);
  awra_command_palette_close (self);
}

static GtkWidget *
create_command_content (PaletteCommand *command)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *label = gtk_label_new (command->label);

  awra_box_set_spacing (GTK_BOX (box), AWRA_SPACING_MD);
  if (command->icon != NULL) {
    GtkWidget *image = gtk_image_new_from_gicon (command->icon);
    gtk_widget_add_css_class (image, "awra-command-icon");
    gtk_box_append (GTK_BOX (box), image);
  }
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);
  if (command->detail != NULL) {
    GtkWidget *detail = gtk_label_new (command->detail);

    gtk_label_set_ellipsize (GTK_LABEL (detail), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class (detail, "awra-command-accel");
    gtk_box_append (GTK_BOX (box), detail);
  }
  if (command->accel != NULL) {
    guint key = 0;
    GdkModifierType mods = 0;

    if (gtk_accelerator_parse (command->accel, &key, &mods)) {
      g_autofree char *text = gtk_accelerator_get_label (key, mods);
      GtkWidget *accel = gtk_label_new (text);
      gtk_widget_add_css_class (accel, "awra-command-accel");
      gtk_box_append (GTK_BOX (box), accel);
    }
  }
  return box;
}

static void
append_heading (AwraCommandPalette *self,
                const char         *text)
{
  GtkWidget *row = gtk_list_box_row_new ();
  GtkWidget *label = gtk_label_new (text);

  awra_widget_set_typography (label, AWRA_TYPOGRAPHY_EYEBROW);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_add_css_class (label, "awra-command-heading");
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_widget_set_focusable (row, FALSE);
  gtk_list_box_append (self->list, row);
}

static void
set_current (AwraCommandPalette *self,
             GtkWidget          *button)
{
  if (self->current == button)
    return;
  if (self->current != NULL)
    gtk_widget_remove_css_class (self->current, "awra-command-current");
  self->current = button;
  if (button == NULL)
    return;
  gtk_widget_add_css_class (button, "awra-command-current");
  /* Keep it in view. */
  if (self->scroller != NULL) {
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment (self->scroller);
    graphene_rect_t bounds;

    if (gtk_widget_compute_bounds (button, GTK_WIDGET (self->list), &bounds)) {
      double top = bounds.origin.y;
      double bottom = top + bounds.size.height;
      double value = gtk_adjustment_get_value (adjustment);
      double page = gtk_adjustment_get_page_size (adjustment);

      if (top < value)
        gtk_adjustment_set_value (adjustment, top);
      else if (bottom > value + page)
        gtk_adjustment_set_value (adjustment, bottom - page);
    }
  }
}

/* The result buttons in order, headings left out. */
static GtkWidget *
result_after (AwraCommandPalette *self,
              GtkWidget          *from,
              gboolean            forward)
{
  GtkWidget *row = from != NULL ? gtk_widget_get_parent (from) : NULL;

  if (row == NULL)
    row = forward ? gtk_widget_get_first_child (GTK_WIDGET (self->list))
                  : gtk_widget_get_last_child (GTK_WIDGET (self->list));
  else
    row = forward ? gtk_widget_get_next_sibling (row) : gtk_widget_get_prev_sibling (row);
  for (; row != NULL;
       row = forward ? gtk_widget_get_next_sibling (row) : gtk_widget_get_prev_sibling (row)) {
    GtkWidget *child = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row));

    if (GTK_IS_BUTTON (child))
      return child;
  }
  return NULL;
}

static void
rebuild_results (AwraCommandPalette *self)
{
  GtkWidget *child;
  const char *query = awra_search_entry_get_text (self->search);
  const char *current_section = NULL;
  guint visible = 0;

  self->current = NULL;
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->list))) != NULL)
    gtk_list_box_remove (self->list, child);
  for (guint i = 0; i < self->commands->len; i++) {
    PaletteCommand *command = g_ptr_array_index (self->commands, i);
    GtkWidget *button;
    GtkWidget *row;

    if (!matches_query (command->label, query))
      continue;
    if (command->section != NULL && command->section != current_section)
      append_heading (self, command->section);
    current_section = command->section;
    row = gtk_list_box_row_new ();
    gtk_widget_set_focusable (row, FALSE);
    button = gtk_button_new ();
    gtk_button_set_child (GTK_BUTTON (button), create_command_content (command));
    gtk_widget_add_css_class (button, "awra-command-row");
    gtk_widget_set_hexpand (button, TRUE);
    gtk_widget_set_halign (button, GTK_ALIGN_FILL);
    g_object_set_data (G_OBJECT (button), "awra-command", command);
    g_signal_connect (button, "clicked", G_CALLBACK (command_clicked_cb), self);
    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), button);
    gtk_list_box_append (self->list, row);
    visible++;
  }
  gtk_widget_set_visible (GTK_WIDGET (self->empty), visible == 0);
  /* The first result is where Enter goes. */
  set_current (self, result_after (self, NULL, TRUE));
}

static void
search_changed_cb (GtkSearchEntry *entry, AwraCommandPalette *self)
{
  (void) entry;
  rebuild_results (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_QUERY]);
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller, guint keyval,
                guint keycode, GdkModifierType state,
                AwraCommandPalette *self)
{
  (void) controller; (void) keycode; (void) state;
  switch (keyval) {
  case GDK_KEY_Escape:
    awra_command_palette_close (self);
    return GDK_EVENT_STOP;
  case GDK_KEY_Down:
  case GDK_KEY_KP_Down:
  case GDK_KEY_Up:
  case GDK_KEY_KP_Up: {
    gboolean forward = keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down;
    GtkWidget *next = result_after (self, self->current, forward);

    if (next != NULL)
      set_current (self, next);
    return GDK_EVENT_STOP;
  }
  case GDK_KEY_Return:
  case GDK_KEY_KP_Enter:
  case GDK_KEY_ISO_Enter:
    if (self->current != NULL)
      g_signal_emit_by_name (self->current, "clicked");
    return GDK_EVENT_STOP;
  default:
    return GDK_EVENT_PROPAGATE;
  }
}

static gboolean
close_when_inactive (gpointer data)
{
  AwraCommandPalette *self = data;

  if (gtk_widget_get_visible (GTK_WIDGET (self)) &&
      !gtk_window_is_active (GTK_WINDOW (self))) {
    /* An outside click chose a new focus target, possibly in another app.
     * Do not move focus back to the field that opened the palette. */
    g_weak_ref_set (&self->previous_focus, NULL);
    awra_command_palette_close (self);
  }
  return G_SOURCE_REMOVE;
}

static void
active_changed_cb (GtkWindow          *window,
                   GParamSpec         *pspec,
                   AwraCommandPalette *self)
{
  (void) pspec;
  if (gtk_window_is_active (window)) {
    self->had_focus = TRUE;
  } else if (self->had_focus && gtk_widget_get_visible (GTK_WIDGET (self))) {
    /* Defer past the current event: a command may open another window and
     * close the palette itself. Keep self alive until that event finishes. */
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, close_when_inactive,
                     g_object_ref (self), g_object_unref);
  }
}

static void
awra_command_palette_dispose (GObject *object)
{
  AwraCommandPalette *self = AWRA_COMMAND_PALETTE (object);
  g_clear_object (&self->model);
  g_clear_pointer (&self->commands, g_ptr_array_unref);
  g_clear_pointer (&self->sections, g_ptr_array_unref);
  G_OBJECT_CLASS (awra_command_palette_parent_class)->dispose (object);
}

static void
awra_command_palette_finalize (GObject *object)
{
  AwraCommandPalette *self = AWRA_COMMAND_PALETTE (object);
  g_free (self->title);
  g_free (self->placeholder_text);
  g_free (self->empty_text);
  g_weak_ref_clear (&self->previous_focus);
  G_OBJECT_CLASS (awra_command_palette_parent_class)->finalize (object);
}

static void awra_command_palette_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) { AwraCommandPalette *self = AWRA_COMMAND_PALETTE (object); switch (id) { case PROP_MENU_MODEL: g_value_set_object (value, self->model); break; case PROP_TITLE: g_value_set_string (value, self->title); break; case PROP_QUERY: g_value_set_string (value, awra_command_palette_get_query (self)); break; case PROP_PLACEHOLDER_TEXT: g_value_set_string (value, self->placeholder_text); break; case PROP_EMPTY_TEXT: g_value_set_string (value, self->empty_text); break; default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); } }
static void awra_command_palette_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) { AwraCommandPalette *self = AWRA_COMMAND_PALETTE (object); switch (id) { case PROP_MENU_MODEL: awra_command_palette_set_menu_model (self, g_value_get_object (value)); break; case PROP_TITLE: awra_command_palette_set_title (self, g_value_get_string (value)); break; case PROP_QUERY: awra_command_palette_set_query (self, g_value_get_string (value)); break; case PROP_PLACEHOLDER_TEXT: awra_command_palette_set_placeholder_text (self, g_value_get_string (value)); break; case PROP_EMPTY_TEXT: awra_command_palette_set_empty_text (self, g_value_get_string (value)); break; default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); } }

static void
awra_command_palette_class_init (AwraCommandPaletteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = awra_command_palette_dispose;
  object_class->finalize = awra_command_palette_finalize;
  object_class->get_property = awra_command_palette_get_property;
  object_class->set_property = awra_command_palette_set_property;
  properties[PROP_MENU_MODEL] = g_param_spec_object ("menu-model", NULL, NULL,
    G_TYPE_MENU_MODEL, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_TITLE] = g_param_spec_string ("title", NULL, NULL, "Commands",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_QUERY] = g_param_spec_string ("query", NULL, NULL, "",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_PLACEHOLDER_TEXT] = g_param_spec_string (
    "placeholder-text", NULL, NULL, "Type a command…",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_EMPTY_TEXT] = g_param_spec_string (
    "empty-text", NULL, NULL, "No matching command",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  signals[ACTIVATE_COMMAND] = g_signal_new (
    "activate-command", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
    0, g_signal_accumulator_true_handled, NULL, NULL,
    G_TYPE_BOOLEAN, 2, G_TYPE_STRING, G_TYPE_VARIANT);
}

static void
awra_command_palette_init (AwraCommandPalette *self)
{
  GtkWidget *root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *scroller = gtk_scrolled_window_new ();
  GtkEventController *keys = gtk_event_controller_key_new ();
  g_autoptr (AwraMaterial) material = awra_material_new_for_preset (
    AWRA_MATERIAL_PRESET_FLOATING);

  self->commands = g_ptr_array_new_with_free_func (palette_command_free);
  self->sections = g_ptr_array_new_with_free_func (g_free);
  self->title = g_strdup (_("Commands"));
  self->placeholder_text = g_strdup (_("Type a command…"));
  self->empty_text = g_strdup (_("No matching command"));
  g_weak_ref_init (&self->previous_focus, NULL);
  self->search = AWRA_SEARCH_ENTRY (awra_search_entry_new ());
  self->list = GTK_LIST_BOX (gtk_list_box_new ());
  self->empty = GTK_LABEL (gtk_label_new (self->empty_text));
  awra_widget_set_typography (GTK_WIDGET (self->empty), AWRA_TYPOGRAPHY_MUTED);
  /* The window header shows the title; repeating it in the body only
   * pushes the results down. */
  gtk_window_set_title (GTK_WINDOW (self), self->title);
  gtk_widget_set_halign (GTK_WIDGET (self->empty), GTK_ALIGN_CENTER);
  awra_search_entry_set_placeholder_text (self->search, self->placeholder_text);
  gtk_list_box_set_selection_mode (self->list, GTK_SELECTION_NONE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller),
                                 GTK_WIDGET (self->list));
  gtk_widget_set_vexpand (scroller, TRUE);
  awra_box_apply_layout_preset (GTK_BOX (root), AWRA_LAYOUT_PRESET_OVERLAY);
  gtk_box_append (GTK_BOX (root), GTK_WIDGET (self->search));
  gtk_box_append (GTK_BOX (root), scroller);
  gtk_box_append (GTK_BOX (root), GTK_WIDGET (self->empty));
  awra_window_set_content (AWRA_WINDOW (self), root);
  awra_surface_set_material (awra_window_get_root_surface (AWRA_WINDOW (self)),
                             material);
  awra_surface_set_role (awra_window_get_root_surface (AWRA_WINDOW (self)),
                         AWRA_SURFACE_ROLE_DIALOG);
  awra_surface_set_elevation_level (
    awra_window_get_root_surface (AWRA_WINDOW (self)), AWRA_ELEVATION_MODAL);
  /* Like a command popup, allow the parent to receive an outside click.
   * A modal toplevel would swallow that click and never lose activation. */
  gtk_window_set_modal (GTK_WINDOW (self), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (self), 560, 460);
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-command-palette");
  g_signal_connect (self, "notify::is-active",
                    G_CALLBACK (active_changed_cb), self);
  g_signal_connect (awra_search_entry_get_delegate (self->search),
                    "search-changed",
                    G_CALLBACK (search_changed_cb), self);
  /* Before the search field, which would take Escape, Enter and the
   * arrows for itself. */
  gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
  self->scroller = GTK_SCROLLED_WINDOW (scroller);
  g_signal_connect (keys, "key-pressed", G_CALLBACK (key_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), keys);
  rebuild_results (self);
}

GtkWidget *
awra_command_palette_new (GtkWindow *parent)
{
  AwraCommandPalette *self;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);
  self = g_object_new (AWRA_TYPE_COMMAND_PALETTE, NULL);
  if (parent != NULL) {
    gtk_window_set_transient_for (GTK_WINDOW (self), parent);
    gtk_window_set_application (GTK_WINDOW (self), gtk_window_get_application (parent));
  }
  /* GtkWindow owns its initial reference until destruction. The constructor
   * promises transfer-full, so callers (including Rust) need a separate ref. */
  return GTK_WIDGET (g_object_ref (self));
}
GMenuModel *awra_command_palette_get_menu_model (AwraCommandPalette *self) { g_return_val_if_fail (AWRA_IS_COMMAND_PALETTE (self), NULL); return self->model; }
void awra_command_palette_set_menu_model (AwraCommandPalette *self, GMenuModel *model) { g_return_if_fail (AWRA_IS_COMMAND_PALETTE (self)); g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model)); if (self->model == model) return; g_set_object (&self->model, model); g_ptr_array_set_size (self->commands, 0); g_ptr_array_set_size (self->sections, 0); if (model != NULL) collect_commands (self, model, NULL); rebuild_results (self); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_MODEL]); }
const char *awra_command_palette_get_title (AwraCommandPalette *self) { g_return_val_if_fail (AWRA_IS_COMMAND_PALETTE (self), NULL); return self->title; }
void awra_command_palette_set_title (AwraCommandPalette *self, const char *title) { const char *value; g_return_if_fail (AWRA_IS_COMMAND_PALETTE (self)); value = title != NULL ? title : _("Commands"); if (g_strcmp0 (self->title, value) == 0) return; g_free (self->title); self->title = g_strdup (value); gtk_window_set_title (GTK_WINDOW (self), value); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]); }
const char *awra_command_palette_get_query (AwraCommandPalette *self) { g_return_val_if_fail (AWRA_IS_COMMAND_PALETTE (self), NULL); return awra_search_entry_get_text (self->search); }
void awra_command_palette_set_query (AwraCommandPalette *self, const char *query) { g_return_if_fail (AWRA_IS_COMMAND_PALETTE (self)); awra_search_entry_set_text (self->search, query); }
const char *awra_command_palette_get_placeholder_text (AwraCommandPalette *self) { g_return_val_if_fail (AWRA_IS_COMMAND_PALETTE (self), NULL); return self->placeholder_text; }
void awra_command_palette_set_placeholder_text (AwraCommandPalette *self, const char *text) { g_return_if_fail (AWRA_IS_COMMAND_PALETTE (self)); if (g_strcmp0 (self->placeholder_text, text) == 0) return; g_free (self->placeholder_text); self->placeholder_text = g_strdup (text); awra_search_entry_set_placeholder_text (self->search, text); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLACEHOLDER_TEXT]); }
const char *awra_command_palette_get_empty_text (AwraCommandPalette *self) { g_return_val_if_fail (AWRA_IS_COMMAND_PALETTE (self), NULL); return self->empty_text; }
void awra_command_palette_set_empty_text (AwraCommandPalette *self, const char *text) { const char *value; g_return_if_fail (AWRA_IS_COMMAND_PALETTE (self)); value = text != NULL ? text : _("No matching command"); if (g_strcmp0 (self->empty_text, value) == 0) return; g_free (self->empty_text); self->empty_text = g_strdup (value); gtk_label_set_text (self->empty, value); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EMPTY_TEXT]); }
void awra_command_palette_present (AwraCommandPalette *self) { GtkWindow *parent; GtkWidget *focus = NULL; g_return_if_fail (AWRA_IS_COMMAND_PALETTE (self)); parent = gtk_window_get_transient_for (GTK_WINDOW (self)); if (parent != NULL) focus = gtk_root_get_focus (GTK_ROOT (parent)); g_weak_ref_set (&self->previous_focus, focus); gtk_window_present (GTK_WINDOW (self)); gtk_widget_grab_focus (GTK_WIDGET (self->search)); }
void
awra_command_palette_close (AwraCommandPalette *self)
{
  g_autoptr (GtkWidget) focus = NULL;

  g_return_if_fail (AWRA_IS_COMMAND_PALETTE (self));
  focus = g_weak_ref_get (&self->previous_focus);
  g_weak_ref_set (&self->previous_focus, NULL);
  self->had_focus = FALSE;
  /* Closing may release GTK's last reference. Do not access self afterward. */
  gtk_window_close (GTK_WINDOW (self));
  if (focus != NULL && gtk_widget_get_sensitive (focus))
    gtk_widget_grab_focus (focus);
}
