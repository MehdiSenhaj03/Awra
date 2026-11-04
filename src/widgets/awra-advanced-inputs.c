/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-password-entry.h>
#include <awra/awra-tag-entry.h>
#include <awra/awra-text-area.h>

#include "awra-i18n-private.h"

/* AwraTextArea */

struct _AwraTextArea {
  GtkWidget parent_instance;
  GtkOverlay *overlay;
  GtkScrolledWindow *scroller;
  GtkTextView *delegate;
  GtkLabel *placeholder;
  char *text_cache;
};

enum { TEXT_PROP_0, TEXT_PROP_TEXT, TEXT_PROP_PLACEHOLDER, TEXT_PROP_MONOSPACE,
       TEXT_N_PROPS };
static GParamSpec *text_properties[TEXT_N_PROPS];

G_DEFINE_FINAL_TYPE (AwraTextArea, awra_text_area, GTK_TYPE_WIDGET)

static void
text_buffer_changed_cb (GtkTextBuffer *buffer,
                        AwraTextArea  *self)
{
  GtkTextIter start;
  GtkTextIter end;

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  g_free (self->text_cache);
  self->text_cache = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->placeholder),
                          *self->text_cache == '\0');
  g_object_notify_by_pspec (G_OBJECT (self), text_properties[TEXT_PROP_TEXT]);
}

static void
awra_text_area_dispose (GObject *object)
{
  AwraTextArea *self = AWRA_TEXT_AREA (object);
  if (self->overlay != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->overlay));
    self->overlay = NULL;
    self->scroller = NULL;
    self->delegate = NULL;
    self->placeholder = NULL;
  }
  G_OBJECT_CLASS (awra_text_area_parent_class)->dispose (object);
}

static void
awra_text_area_finalize (GObject *object)
{
  g_clear_pointer (&AWRA_TEXT_AREA (object)->text_cache, g_free);
  G_OBJECT_CLASS (awra_text_area_parent_class)->finalize (object);
}

static void
awra_text_area_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  AwraTextArea *self = AWRA_TEXT_AREA (widget);
  if (self->overlay != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->overlay), snapshot);
}

static void
awra_text_area_get_property (GObject *object, guint id, GValue *value,
                             GParamSpec *pspec)
{
  AwraTextArea *self = AWRA_TEXT_AREA (object);
  switch (id) {
  case TEXT_PROP_TEXT: g_value_set_string (value, self->text_cache); break;
  case TEXT_PROP_PLACEHOLDER: g_value_set_string (value, gtk_label_get_text (self->placeholder)); break;
  case TEXT_PROP_MONOSPACE: g_value_set_boolean (value, gtk_text_view_get_monospace (self->delegate)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_text_area_set_property (GObject *object, guint id, const GValue *value,
                             GParamSpec *pspec)
{
  switch (id) {
  case TEXT_PROP_TEXT: awra_text_area_set_text (AWRA_TEXT_AREA (object), g_value_get_string (value)); break;
  case TEXT_PROP_PLACEHOLDER: awra_text_area_set_placeholder_text (AWRA_TEXT_AREA (object), g_value_get_string (value)); break;
  case TEXT_PROP_MONOSPACE: awra_text_area_set_monospace (AWRA_TEXT_AREA (object), g_value_get_boolean (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_text_area_class_init (AwraTextAreaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_text_area_dispose;
  object_class->finalize = awra_text_area_finalize;
  object_class->get_property = awra_text_area_get_property;
  object_class->set_property = awra_text_area_set_property;
  widget_class->snapshot = awra_text_area_snapshot;
  text_properties[TEXT_PROP_TEXT] = g_param_spec_string (
    "text", NULL, NULL, "",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  text_properties[TEXT_PROP_PLACEHOLDER] = g_param_spec_string (
    "placeholder-text", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  text_properties[TEXT_PROP_MONOSPACE] = g_param_spec_boolean (
    "monospace", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, TEXT_N_PROPS, text_properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awratextarea");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);
}

static void
awra_text_area_init (AwraTextArea *self)
{
  self->text_cache = g_strdup ("");
  self->overlay = GTK_OVERLAY (gtk_overlay_new ());
  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  self->delegate = GTK_TEXT_VIEW (gtk_text_view_new ());
  self->placeholder = GTK_LABEL (gtk_label_new (NULL));
  gtk_text_view_set_wrap_mode (self->delegate, GTK_WRAP_WORD_CHAR);
  /* The text sits inside the field's border, where the placeholder sits:
   * GtkTextView ignores CSS padding on its text node, so it's set here. */
  gtk_text_view_set_left_margin (self->delegate, 12);
  gtk_text_view_set_right_margin (self->delegate, 12);
  gtk_text_view_set_top_margin (self->delegate, 10);
  gtk_text_view_set_bottom_margin (self->delegate, 10);
  gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-text-area-delegate");
  gtk_scrolled_window_set_child (self->scroller, GTK_WIDGET (self->delegate));
  gtk_overlay_set_child (self->overlay, GTK_WIDGET (self->scroller));
  gtk_widget_add_css_class (GTK_WIDGET (self->placeholder), "awra-placeholder");
  gtk_widget_set_halign (GTK_WIDGET (self->placeholder), GTK_ALIGN_START);
  gtk_widget_set_valign (GTK_WIDGET (self->placeholder), GTK_ALIGN_START);
  gtk_widget_set_can_target (GTK_WIDGET (self->placeholder), FALSE);
  gtk_overlay_add_overlay (self->overlay, GTK_WIDGET (self->placeholder));
  gtk_widget_set_size_request (GTK_WIDGET (self), -1, 112);
  gtk_widget_set_parent (GTK_WIDGET (self->overlay), GTK_WIDGET (self));
  g_signal_connect (gtk_text_view_get_buffer (self->delegate), "changed",
                    G_CALLBACK (text_buffer_changed_cb), self);
}

GtkWidget *awra_text_area_new (void) { return g_object_new (AWRA_TYPE_TEXT_AREA, NULL); }
const char *awra_text_area_get_text (AwraTextArea *self) { g_return_val_if_fail (AWRA_IS_TEXT_AREA (self), NULL); return self->text_cache; }
void awra_text_area_set_text (AwraTextArea *self, const char *text) { g_return_if_fail (AWRA_IS_TEXT_AREA (self)); gtk_text_buffer_set_text (gtk_text_view_get_buffer (self->delegate), text != NULL ? text : "", -1); }
const char *awra_text_area_get_placeholder_text (AwraTextArea *self) { g_return_val_if_fail (AWRA_IS_TEXT_AREA (self), NULL); return gtk_label_get_text (self->placeholder); }
void awra_text_area_set_placeholder_text (AwraTextArea *self, const char *text) { g_return_if_fail (AWRA_IS_TEXT_AREA (self)); gtk_label_set_text (self->placeholder, text != NULL ? text : ""); g_object_notify_by_pspec (G_OBJECT (self), text_properties[TEXT_PROP_PLACEHOLDER]); }
gboolean awra_text_area_get_monospace (AwraTextArea *self) { g_return_val_if_fail (AWRA_IS_TEXT_AREA (self), FALSE); return gtk_text_view_get_monospace (self->delegate); }
void awra_text_area_set_monospace (AwraTextArea *self, gboolean monospace) { g_return_if_fail (AWRA_IS_TEXT_AREA (self)); monospace = !!monospace; if (gtk_text_view_get_monospace (self->delegate) == monospace) return; gtk_text_view_set_monospace (self->delegate, monospace); g_object_notify_by_pspec (G_OBJECT (self), text_properties[TEXT_PROP_MONOSPACE]); }
GtkTextView *awra_text_area_get_delegate (AwraTextArea *self) { g_return_val_if_fail (AWRA_IS_TEXT_AREA (self), NULL); return self->delegate; }
GtkTextBuffer *awra_text_area_get_buffer (AwraTextArea *self) { g_return_val_if_fail (AWRA_IS_TEXT_AREA (self), NULL); return gtk_text_view_get_buffer (self->delegate); }

/* AwraPasswordEntry */

struct _AwraPasswordEntry { GtkWidget parent_instance; GtkPasswordEntry *delegate; char *placeholder; gboolean activates_default; };
enum { PASSWORD_PROP_0, PASSWORD_PROP_TEXT, PASSWORD_PROP_PLACEHOLDER,
       PASSWORD_PROP_PEEK, PASSWORD_PROP_ACTIVATES_DEFAULT, PASSWORD_N_PROPS };
static GParamSpec *password_properties[PASSWORD_N_PROPS];
G_DEFINE_FINAL_TYPE (AwraPasswordEntry, awra_password_entry, GTK_TYPE_WIDGET)

static void password_notify_cb (GtkPasswordEntry *delegate, GParamSpec *pspec, AwraPasswordEntry *self) { (void) delegate; if (g_str_equal (pspec->name, "text")) g_object_notify_by_pspec (G_OBJECT (self), password_properties[PASSWORD_PROP_TEXT]); }
static void awra_password_entry_dispose (GObject *object) { AwraPasswordEntry *self = AWRA_PASSWORD_ENTRY (object); if (self->delegate != NULL) { gtk_widget_unparent (GTK_WIDGET (self->delegate)); self->delegate = NULL; } G_OBJECT_CLASS (awra_password_entry_parent_class)->dispose (object); }
static void awra_password_entry_finalize (GObject *object) { g_clear_pointer (&AWRA_PASSWORD_ENTRY (object)->placeholder, g_free); G_OBJECT_CLASS (awra_password_entry_parent_class)->finalize (object); }
static void awra_password_entry_snapshot (GtkWidget *widget, GtkSnapshot *snapshot) { gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_PASSWORD_ENTRY (widget)->delegate), snapshot); }
static void awra_password_entry_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) { AwraPasswordEntry *self = AWRA_PASSWORD_ENTRY (object); switch (id) { case PASSWORD_PROP_TEXT: g_value_set_string (value, awra_password_entry_get_text (self)); break; case PASSWORD_PROP_PLACEHOLDER: g_value_set_string (value, awra_password_entry_get_placeholder_text (self)); break; case PASSWORD_PROP_PEEK: g_value_set_boolean (value, awra_password_entry_get_show_peek_icon (self)); break; case PASSWORD_PROP_ACTIVATES_DEFAULT: g_value_set_boolean (value, awra_password_entry_get_activates_default (self)); break; default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); } }
static void awra_password_entry_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) { AwraPasswordEntry *self = AWRA_PASSWORD_ENTRY (object); switch (id) { case PASSWORD_PROP_TEXT: awra_password_entry_set_text (self, g_value_get_string (value)); break; case PASSWORD_PROP_PLACEHOLDER: awra_password_entry_set_placeholder_text (self, g_value_get_string (value)); break; case PASSWORD_PROP_PEEK: awra_password_entry_set_show_peek_icon (self, g_value_get_boolean (value)); break; case PASSWORD_PROP_ACTIVATES_DEFAULT: awra_password_entry_set_activates_default (self, g_value_get_boolean (value)); break; default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); } }
static gboolean awra_password_entry_grab_focus (GtkWidget *widget) { return gtk_widget_grab_focus (GTK_WIDGET (AWRA_PASSWORD_ENTRY (widget)->delegate)); }
static void awra_password_entry_class_init (AwraPasswordEntryClass *klass) { GObjectClass *oc = G_OBJECT_CLASS (klass); GtkWidgetClass *wc = GTK_WIDGET_CLASS (klass); oc->dispose = awra_password_entry_dispose; oc->finalize = awra_password_entry_finalize; oc->get_property = awra_password_entry_get_property; oc->set_property = awra_password_entry_set_property; wc->grab_focus = awra_password_entry_grab_focus; wc->snapshot = awra_password_entry_snapshot; password_properties[PASSWORD_PROP_TEXT] = g_param_spec_string ("text", NULL, NULL, "", G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS); password_properties[PASSWORD_PROP_PLACEHOLDER] = g_param_spec_string ("placeholder-text", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS); password_properties[PASSWORD_PROP_PEEK] = g_param_spec_boolean ("show-peek-icon", NULL, NULL, TRUE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS); password_properties[PASSWORD_PROP_ACTIVATES_DEFAULT] = g_param_spec_boolean ("activates-default", NULL, NULL, FALSE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS); g_object_class_install_properties (oc, PASSWORD_N_PROPS, password_properties); gtk_widget_class_set_layout_manager_type (wc, GTK_TYPE_BIN_LAYOUT); gtk_widget_class_set_css_name (wc, "awrapasswordentry"); gtk_widget_class_set_accessible_role (wc, GTK_ACCESSIBLE_ROLE_TEXT_BOX); }
static void awra_password_entry_init (AwraPasswordEntry *self) { self->delegate = GTK_PASSWORD_ENTRY (gtk_password_entry_new ()); gtk_password_entry_set_show_peek_icon (self->delegate, TRUE); gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-entry"); gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-password-entry"); gtk_widget_set_parent (GTK_WIDGET (self->delegate), GTK_WIDGET (self)); g_signal_connect (self->delegate, "notify::text", G_CALLBACK (password_notify_cb), self); }
GtkWidget *awra_password_entry_new (void) { return g_object_new (AWRA_TYPE_PASSWORD_ENTRY, NULL); }
const char *awra_password_entry_get_text (AwraPasswordEntry *self) { g_return_val_if_fail (AWRA_IS_PASSWORD_ENTRY (self), NULL); return gtk_editable_get_text (GTK_EDITABLE (self->delegate)); }
void awra_password_entry_set_text (AwraPasswordEntry *self, const char *text) { g_return_if_fail (AWRA_IS_PASSWORD_ENTRY (self)); gtk_editable_set_text (GTK_EDITABLE (self->delegate), text != NULL ? text : ""); }
const char *awra_password_entry_get_placeholder_text (AwraPasswordEntry *self) { g_return_val_if_fail (AWRA_IS_PASSWORD_ENTRY (self), NULL); return self->placeholder; }
void awra_password_entry_set_placeholder_text (AwraPasswordEntry *self, const char *text) { g_return_if_fail (AWRA_IS_PASSWORD_ENTRY (self)); if (g_strcmp0 (self->placeholder, text) == 0) return; g_free (self->placeholder); self->placeholder = g_strdup (text); g_object_set (self->delegate, "placeholder-text", text, NULL); g_object_notify_by_pspec (G_OBJECT (self), password_properties[PASSWORD_PROP_PLACEHOLDER]); }
gboolean awra_password_entry_get_show_peek_icon (AwraPasswordEntry *self) { g_return_val_if_fail (AWRA_IS_PASSWORD_ENTRY (self), FALSE); return gtk_password_entry_get_show_peek_icon (self->delegate); }
void awra_password_entry_set_show_peek_icon (AwraPasswordEntry *self, gboolean show) { g_return_if_fail (AWRA_IS_PASSWORD_ENTRY (self)); show = !!show; if (gtk_password_entry_get_show_peek_icon (self->delegate) == show) return; gtk_password_entry_set_show_peek_icon (self->delegate, show); g_object_notify_by_pspec (G_OBJECT (self), password_properties[PASSWORD_PROP_PEEK]); }
gboolean awra_password_entry_get_activates_default (AwraPasswordEntry *self) { g_return_val_if_fail (AWRA_IS_PASSWORD_ENTRY (self), FALSE); return self->activates_default; }
void awra_password_entry_set_activates_default (AwraPasswordEntry *self, gboolean activates) { g_return_if_fail (AWRA_IS_PASSWORD_ENTRY (self)); activates = !!activates; if (self->activates_default == activates) return; self->activates_default = activates; g_object_set (self->delegate, "activates-default", activates, NULL); g_object_notify_by_pspec (G_OBJECT (self), password_properties[PASSWORD_PROP_ACTIVATES_DEFAULT]); }
GtkPasswordEntry *awra_password_entry_get_delegate (AwraPasswordEntry *self) { g_return_val_if_fail (AWRA_IS_PASSWORD_ENTRY (self), NULL); return self->delegate; }

/* AwraTagEntry */

struct _AwraTagEntry { GtkWidget parent_instance; GtkBox *root; GtkFlowBox *flow; GtkEntry *delegate; GPtrArray *tags; gboolean editable; };
enum { TAG_PROP_0, TAG_PROP_EDITABLE, TAG_PROP_N_TAGS, TAG_N_PROPS };
enum { TAG_ADDED, TAG_REMOVED, TAG_N_SIGNALS };
static GParamSpec *tag_properties[TAG_N_PROPS];
static guint tag_signals[TAG_N_SIGNALS];
G_DEFINE_FINAL_TYPE (AwraTagEntry, awra_tag_entry, GTK_TYPE_WIDGET)

static void rebuild_tags (AwraTagEntry *self);
static void tag_button_clicked_cb (GtkButton *button, AwraTagEntry *self) { awra_tag_entry_remove (self, g_object_get_data (G_OBJECT (button), "awra-tag-value")); }
static void rebuild_tags (AwraTagEntry *self) { GtkWidget *child; while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->flow))) != NULL) gtk_flow_box_remove (self->flow, child); for (guint i = 0; i < self->tags->len; i++) { const char *tag = g_ptr_array_index (self->tags, i); g_autofree char *label = g_strdup_printf ("%s  ×", tag); GtkWidget *button = gtk_button_new_with_label (label); gtk_widget_add_css_class (button, "awra-tag"); gtk_widget_set_sensitive (button, self->editable); gtk_accessible_update_property (GTK_ACCESSIBLE (button), GTK_ACCESSIBLE_PROPERTY_LABEL, label, -1); g_object_set_data_full (G_OBJECT (button), "awra-tag-value", g_strdup (tag), g_free); g_signal_connect (button, "clicked", G_CALLBACK (tag_button_clicked_cb), self); gtk_flow_box_append (self->flow, button); } }
static void tag_entry_activate_cb (GtkEntry *entry, AwraTagEntry *self) { const char *text = gtk_editable_get_text (GTK_EDITABLE (entry)); if (awra_tag_entry_add (self, text)) gtk_editable_set_text (GTK_EDITABLE (entry), ""); }
static void awra_tag_entry_dispose (GObject *object) { AwraTagEntry *self = AWRA_TAG_ENTRY (object); if (self->root != NULL) { gtk_widget_unparent (GTK_WIDGET (self->root)); self->root = NULL; self->flow = NULL; self->delegate = NULL; } g_clear_pointer (&self->tags, g_ptr_array_unref); G_OBJECT_CLASS (awra_tag_entry_parent_class)->dispose (object); }
static void awra_tag_entry_snapshot (GtkWidget *widget, GtkSnapshot *snapshot) { gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_TAG_ENTRY (widget)->root), snapshot); }
static void awra_tag_entry_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) { AwraTagEntry *self = AWRA_TAG_ENTRY (object); if (id == TAG_PROP_EDITABLE) g_value_set_boolean (value, self->editable); else if (id == TAG_PROP_N_TAGS) g_value_set_uint (value, self->tags->len); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }
static void awra_tag_entry_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) { if (id == TAG_PROP_EDITABLE) awra_tag_entry_set_editable (AWRA_TAG_ENTRY (object), g_value_get_boolean (value)); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }
static void awra_tag_entry_class_init (AwraTagEntryClass *klass) { GObjectClass *oc = G_OBJECT_CLASS (klass); GtkWidgetClass *wc = GTK_WIDGET_CLASS (klass); oc->dispose = awra_tag_entry_dispose; oc->get_property = awra_tag_entry_get_property; oc->set_property = awra_tag_entry_set_property; wc->snapshot = awra_tag_entry_snapshot; tag_properties[TAG_PROP_EDITABLE] = g_param_spec_boolean ("editable", NULL, NULL, TRUE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS); tag_properties[TAG_PROP_N_TAGS] = g_param_spec_uint ("n-tags", NULL, NULL, 0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS); g_object_class_install_properties (oc, TAG_N_PROPS, tag_properties); tag_signals[TAG_ADDED] = g_signal_new ("tag-added", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING); tag_signals[TAG_REMOVED] = g_signal_new ("tag-removed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING); gtk_widget_class_set_layout_manager_type (wc, GTK_TYPE_BIN_LAYOUT); gtk_widget_class_set_css_name (wc, "awratagentry"); gtk_widget_class_set_accessible_role (wc, GTK_ACCESSIBLE_ROLE_GROUP); }
static void awra_tag_entry_init (AwraTagEntry *self) { self->tags = g_ptr_array_new_with_free_func (g_free); self->editable = TRUE; self->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 4)); self->flow = GTK_FLOW_BOX (gtk_flow_box_new ()); gtk_flow_box_set_selection_mode (self->flow, GTK_SELECTION_NONE); gtk_flow_box_set_column_spacing (self->flow, 6); gtk_flow_box_set_row_spacing (self->flow, 6); self->delegate = GTK_ENTRY (gtk_entry_new ()); gtk_entry_set_placeholder_text (self->delegate, _("Add a tag…")); gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-entry"); gtk_box_append (self->root, GTK_WIDGET (self->flow)); gtk_box_append (self->root, GTK_WIDGET (self->delegate)); gtk_widget_set_parent (GTK_WIDGET (self->root), GTK_WIDGET (self)); g_signal_connect (self->delegate, "activate", G_CALLBACK (tag_entry_activate_cb), self); }
GtkWidget *awra_tag_entry_new (void) { return g_object_new (AWRA_TYPE_TAG_ENTRY, NULL); }
gboolean awra_tag_entry_add (AwraTagEntry *self, const char *tag) { g_autofree char *normalized = NULL; g_return_val_if_fail (AWRA_IS_TAG_ENTRY (self), FALSE); if (tag == NULL) return FALSE; normalized = g_strdup (tag); g_strstrip (normalized); if (*normalized == '\0') return FALSE; for (guint i = 0; i < self->tags->len; i++) if (g_str_equal (g_ptr_array_index (self->tags, i), normalized)) return FALSE; g_ptr_array_add (self->tags, g_steal_pointer (&normalized)); rebuild_tags (self); g_object_notify_by_pspec (G_OBJECT (self), tag_properties[TAG_PROP_N_TAGS]); g_signal_emit (self, tag_signals[TAG_ADDED], 0, g_ptr_array_index (self->tags, self->tags->len - 1)); return TRUE; }
gboolean awra_tag_entry_remove (AwraTagEntry *self, const char *tag) { g_return_val_if_fail (AWRA_IS_TAG_ENTRY (self), FALSE); for (guint i = 0; i < self->tags->len; i++) if (g_strcmp0 (g_ptr_array_index (self->tags, i), tag) == 0) { g_autofree char *removed = g_strdup (tag); g_ptr_array_remove_index (self->tags, i); rebuild_tags (self); g_object_notify_by_pspec (G_OBJECT (self), tag_properties[TAG_PROP_N_TAGS]); g_signal_emit (self, tag_signals[TAG_REMOVED], 0, removed); return TRUE; } return FALSE; }
void awra_tag_entry_clear (AwraTagEntry *self) { g_return_if_fail (AWRA_IS_TAG_ENTRY (self)); while (self->tags->len > 0) awra_tag_entry_remove (self, g_ptr_array_index (self->tags, self->tags->len - 1)); }
guint awra_tag_entry_get_n_tags (AwraTagEntry *self) { g_return_val_if_fail (AWRA_IS_TAG_ENTRY (self), 0); return self->tags->len; }
const char *awra_tag_entry_get_tag (AwraTagEntry *self, guint position) { g_return_val_if_fail (AWRA_IS_TAG_ENTRY (self), NULL); return position < self->tags->len ? g_ptr_array_index (self->tags, position) : NULL; }
gboolean awra_tag_entry_get_editable (AwraTagEntry *self) { g_return_val_if_fail (AWRA_IS_TAG_ENTRY (self), FALSE); return self->editable; }
void awra_tag_entry_set_editable (AwraTagEntry *self, gboolean editable) { g_return_if_fail (AWRA_IS_TAG_ENTRY (self)); editable = !!editable; if (self->editable == editable) return; self->editable = editable; gtk_widget_set_visible (GTK_WIDGET (self->delegate), editable); rebuild_tags (self); g_object_notify_by_pspec (G_OBJECT (self), tag_properties[TAG_PROP_EDITABLE]); }
GtkEntry *awra_tag_entry_get_delegate (AwraTagEntry *self) { g_return_val_if_fail (AWRA_IS_TAG_ENTRY (self), NULL); return self->delegate; }
