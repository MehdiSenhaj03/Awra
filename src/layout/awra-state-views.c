/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-empty-state.h>
#include <awra/awra-error-state.h>
#include <awra/awra-layout.h>
#include <awra/awra-loading-state.h>
#include <awra/awra-typography.h>

typedef struct {
  GtkBox *root;
  GtkImage *icon;
  GtkLabel *title;
  GtkLabel *description;
  GtkWidget *action;
} StateContent;

static void
state_content_init (GtkWidget    *owner,
                    StateContent *content,
                    const char   *icon_name)
{
  content->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  content->icon = GTK_IMAGE (gtk_image_new_from_icon_name (icon_name));
  content->title = GTK_LABEL (gtk_label_new (NULL));
  content->description = GTK_LABEL (gtk_label_new (NULL));
  awra_box_set_spacing (content->root, AWRA_SPACING_SM);
  awra_widget_set_margin (GTK_WIDGET (content->root), AWRA_SPACING_XL);
  awra_widget_set_typography (GTK_WIDGET (content->title),
                              AWRA_TYPOGRAPHY_TITLE_2);
  awra_widget_set_typography (GTK_WIDGET (content->description),
                              AWRA_TYPOGRAPHY_MUTED);
  gtk_widget_add_css_class (GTK_WIDGET (content->icon), "awra-state-icon");
  gtk_label_set_justify (content->title, GTK_JUSTIFY_CENTER);
  gtk_label_set_justify (content->description, GTK_JUSTIFY_CENTER);
  gtk_label_set_wrap (content->title, TRUE);
  gtk_label_set_wrap_mode (content->title, PANGO_WRAP_WORD_CHAR);
  gtk_label_set_wrap (content->description, TRUE);
  gtk_label_set_wrap_mode (content->description, PANGO_WRAP_WORD_CHAR);
  gtk_label_set_max_width_chars (content->description, 48);
  gtk_widget_set_halign (GTK_WIDGET (content->root), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (content->root), GTK_ALIGN_CENTER);
  gtk_widget_set_halign (GTK_WIDGET (content->icon), GTK_ALIGN_CENTER);
  gtk_widget_set_halign (GTK_WIDGET (content->title), GTK_ALIGN_CENTER);
  gtk_widget_set_halign (GTK_WIDGET (content->description), GTK_ALIGN_CENTER);
  gtk_box_append (content->root, GTK_WIDGET (content->icon));
  gtk_box_append (content->root, GTK_WIDGET (content->title));
  gtk_box_append (content->root, GTK_WIDGET (content->description));
  gtk_widget_set_parent (GTK_WIDGET (content->root), owner);
}

static void
state_content_dispose (StateContent *content)
{
  if (content->root != NULL)
    gtk_widget_unparent (GTK_WIDGET (content->root));
  content->root = NULL;
  content->icon = NULL;
  content->title = NULL;
  content->description = NULL;
  content->action = NULL;
}

static void
state_content_set_action (StateContent *content,
                          GtkWidget    *action)
{
  if (content->action == action)
    return;
  if (content->action != NULL)
    gtk_box_remove (content->root, content->action);
  content->action = action;
  if (action != NULL) {
    gtk_widget_set_halign (action, GTK_ALIGN_CENTER);
    gtk_box_append (content->root, action);
  }
}

/* Empty state */

struct _AwraEmptyState {
  GtkWidget parent_instance;
  StateContent content;
  char *icon_name;
  char *title;
  char *description;
};

enum {
  EMPTY_PROP_0,
  EMPTY_PROP_ICON_NAME,
  EMPTY_PROP_TITLE,
  EMPTY_PROP_DESCRIPTION,
  EMPTY_PROP_ACTION,
  EMPTY_N_PROPS
};

static GParamSpec *empty_properties[EMPTY_N_PROPS];

G_DEFINE_FINAL_TYPE (AwraEmptyState, awra_empty_state, GTK_TYPE_WIDGET)

static void
awra_empty_state_dispose (GObject *object)
{
  state_content_dispose (&AWRA_EMPTY_STATE (object)->content);
  G_OBJECT_CLASS (awra_empty_state_parent_class)->dispose (object);
}

static void
awra_empty_state_finalize (GObject *object)
{
  AwraEmptyState *self = AWRA_EMPTY_STATE (object);
  g_clear_pointer (&self->icon_name, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->description, g_free);
  G_OBJECT_CLASS (awra_empty_state_parent_class)->finalize (object);
}

static void
awra_empty_state_get_property (GObject *object, guint id, GValue *value,
                               GParamSpec *pspec)
{
  AwraEmptyState *self = AWRA_EMPTY_STATE (object);
  switch (id) {
  case EMPTY_PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
  case EMPTY_PROP_TITLE: g_value_set_string (value, self->title); break;
  case EMPTY_PROP_DESCRIPTION: g_value_set_string (value, self->description); break;
  case EMPTY_PROP_ACTION: g_value_set_object (value, self->content.action); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_empty_state_set_property (GObject *object, guint id, const GValue *value,
                               GParamSpec *pspec)
{
  AwraEmptyState *self = AWRA_EMPTY_STATE (object);
  switch (id) {
  case EMPTY_PROP_ICON_NAME: awra_empty_state_set_icon_name (self, g_value_get_string (value)); break;
  case EMPTY_PROP_TITLE: awra_empty_state_set_title (self, g_value_get_string (value)); break;
  case EMPTY_PROP_DESCRIPTION: awra_empty_state_set_description (self, g_value_get_string (value)); break;
  case EMPTY_PROP_ACTION: awra_empty_state_set_action (self, g_value_get_object (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_empty_state_class_init (AwraEmptyStateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_empty_state_dispose;
  object_class->finalize = awra_empty_state_finalize;
  object_class->get_property = awra_empty_state_get_property;
  object_class->set_property = awra_empty_state_set_property;
  empty_properties[EMPTY_PROP_ICON_NAME] = g_param_spec_string (
    "icon-name", NULL, NULL, "folder-open-symbolic",
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  empty_properties[EMPTY_PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  empty_properties[EMPTY_PROP_DESCRIPTION] = g_param_spec_string (
    "description", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  empty_properties[EMPTY_PROP_ACTION] = g_param_spec_object (
    "action", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, EMPTY_N_PROPS,
                                     empty_properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awraemptystate");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_empty_state_init (AwraEmptyState *self)
{
  self->icon_name = g_strdup ("folder-open-symbolic");
  state_content_init (GTK_WIDGET (self), &self->content, self->icon_name);
}

GtkWidget *awra_empty_state_new (void) { return g_object_new (AWRA_TYPE_EMPTY_STATE, NULL); }
const char *awra_empty_state_get_icon_name (AwraEmptyState *self) { g_return_val_if_fail (AWRA_IS_EMPTY_STATE (self), NULL); return self->icon_name; }
void awra_empty_state_set_icon_name (AwraEmptyState *self, const char *name) { const char *resolved; g_return_if_fail (AWRA_IS_EMPTY_STATE (self)); resolved = name != NULL && *name != '\0' ? name : "folder-open-symbolic"; if (g_strcmp0 (self->icon_name, resolved) == 0) return; g_free (self->icon_name); self->icon_name = g_strdup (resolved); gtk_image_set_from_icon_name (self->content.icon, resolved); g_object_notify_by_pspec (G_OBJECT (self), empty_properties[EMPTY_PROP_ICON_NAME]); }
const char *awra_empty_state_get_title (AwraEmptyState *self) { g_return_val_if_fail (AWRA_IS_EMPTY_STATE (self), NULL); return self->title; }
void awra_empty_state_set_title (AwraEmptyState *self, const char *title) { g_return_if_fail (AWRA_IS_EMPTY_STATE (self)); if (g_strcmp0 (self->title, title) == 0) return; g_free (self->title); self->title = g_strdup (title); gtk_label_set_text (self->content.title, title != NULL ? title : ""); gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL, title != NULL ? title : "", -1); g_object_notify_by_pspec (G_OBJECT (self), empty_properties[EMPTY_PROP_TITLE]); }
const char *awra_empty_state_get_description (AwraEmptyState *self) { g_return_val_if_fail (AWRA_IS_EMPTY_STATE (self), NULL); return self->description; }
void awra_empty_state_set_description (AwraEmptyState *self, const char *description) { g_return_if_fail (AWRA_IS_EMPTY_STATE (self)); if (g_strcmp0 (self->description, description) == 0) return; g_free (self->description); self->description = g_strdup (description); gtk_label_set_text (self->content.description, description != NULL ? description : ""); gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description != NULL ? description : "", -1); g_object_notify_by_pspec (G_OBJECT (self), empty_properties[EMPTY_PROP_DESCRIPTION]); }
GtkWidget *awra_empty_state_get_action (AwraEmptyState *self) { g_return_val_if_fail (AWRA_IS_EMPTY_STATE (self), NULL); return self->content.action; }
void awra_empty_state_set_action (AwraEmptyState *self, GtkWidget *action) { g_return_if_fail (AWRA_IS_EMPTY_STATE (self)); g_return_if_fail (action == NULL || GTK_IS_WIDGET (action)); if (self->content.action == action) return; state_content_set_action (&self->content, action); g_object_notify_by_pspec (G_OBJECT (self), empty_properties[EMPTY_PROP_ACTION]); }

/* Loading state */

struct _AwraLoadingState {
  GtkWidget parent_instance;
  StateContent content;
  GtkSpinner *spinner;
  char *title;
  char *description;
  gboolean active;
};

enum { LOADING_PROP_0, LOADING_PROP_TITLE, LOADING_PROP_DESCRIPTION,
       LOADING_PROP_ACTIVE, LOADING_N_PROPS };
static GParamSpec *loading_properties[LOADING_N_PROPS];

G_DEFINE_FINAL_TYPE (AwraLoadingState, awra_loading_state, GTK_TYPE_WIDGET)

static void
awra_loading_state_dispose (GObject *object)
{
  AwraLoadingState *self = AWRA_LOADING_STATE (object);
  state_content_dispose (&self->content);
  self->spinner = NULL;
  G_OBJECT_CLASS (awra_loading_state_parent_class)->dispose (object);
}

static void
awra_loading_state_finalize (GObject *object)
{
  AwraLoadingState *self = AWRA_LOADING_STATE (object);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->description, g_free);
  G_OBJECT_CLASS (awra_loading_state_parent_class)->finalize (object);
}

static void
awra_loading_state_get_property (GObject *object, guint id, GValue *value,
                                 GParamSpec *pspec)
{
  AwraLoadingState *self = AWRA_LOADING_STATE (object);
  switch (id) {
  case LOADING_PROP_TITLE: g_value_set_string (value, self->title); break;
  case LOADING_PROP_DESCRIPTION: g_value_set_string (value, self->description); break;
  case LOADING_PROP_ACTIVE: g_value_set_boolean (value, self->active); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_loading_state_set_property (GObject *object, guint id,
                                 const GValue *value, GParamSpec *pspec)
{
  AwraLoadingState *self = AWRA_LOADING_STATE (object);
  switch (id) {
  case LOADING_PROP_TITLE: awra_loading_state_set_title (self, g_value_get_string (value)); break;
  case LOADING_PROP_DESCRIPTION: awra_loading_state_set_description (self, g_value_get_string (value)); break;
  case LOADING_PROP_ACTIVE: awra_loading_state_set_active (self, g_value_get_boolean (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_loading_state_class_init (AwraLoadingStateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_loading_state_dispose;
  object_class->finalize = awra_loading_state_finalize;
  object_class->get_property = awra_loading_state_get_property;
  object_class->set_property = awra_loading_state_set_property;
  loading_properties[LOADING_PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  loading_properties[LOADING_PROP_DESCRIPTION] = g_param_spec_string (
    "description", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  loading_properties[LOADING_PROP_ACTIVE] = g_param_spec_boolean (
    "active", NULL, NULL, TRUE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, LOADING_N_PROPS,
                                     loading_properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awraloadingstate");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_PROGRESS_BAR);
}

static void
awra_loading_state_init (AwraLoadingState *self)
{
  state_content_init (GTK_WIDGET (self), &self->content,
                      "content-loading-symbolic");
  self->spinner = GTK_SPINNER (gtk_spinner_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->spinner), "awra-state-spinner");
  gtk_box_remove (self->content.root, GTK_WIDGET (self->content.icon));
  self->content.icon = NULL;
  gtk_box_prepend (self->content.root, GTK_WIDGET (self->spinner));
  self->active = TRUE;
  gtk_spinner_start (self->spinner);
}

GtkWidget *awra_loading_state_new (void) { return g_object_new (AWRA_TYPE_LOADING_STATE, NULL); }
const char *awra_loading_state_get_title (AwraLoadingState *self) { g_return_val_if_fail (AWRA_IS_LOADING_STATE (self), NULL); return self->title; }
void awra_loading_state_set_title (AwraLoadingState *self, const char *title) { g_return_if_fail (AWRA_IS_LOADING_STATE (self)); if (g_strcmp0 (self->title, title) == 0) return; g_free (self->title); self->title = g_strdup (title); gtk_label_set_text (self->content.title, title != NULL ? title : ""); gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL, title != NULL ? title : "", -1); g_object_notify_by_pspec (G_OBJECT (self), loading_properties[LOADING_PROP_TITLE]); }
const char *awra_loading_state_get_description (AwraLoadingState *self) { g_return_val_if_fail (AWRA_IS_LOADING_STATE (self), NULL); return self->description; }
void awra_loading_state_set_description (AwraLoadingState *self, const char *description) { g_return_if_fail (AWRA_IS_LOADING_STATE (self)); if (g_strcmp0 (self->description, description) == 0) return; g_free (self->description); self->description = g_strdup (description); gtk_label_set_text (self->content.description, description != NULL ? description : ""); gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description != NULL ? description : "", -1); g_object_notify_by_pspec (G_OBJECT (self), loading_properties[LOADING_PROP_DESCRIPTION]); }
gboolean awra_loading_state_get_active (AwraLoadingState *self) { g_return_val_if_fail (AWRA_IS_LOADING_STATE (self), FALSE); return self->active; }
void awra_loading_state_set_active (AwraLoadingState *self, gboolean active) { g_return_if_fail (AWRA_IS_LOADING_STATE (self)); active = !!active; if (self->active == active) return; self->active = active; if (active) gtk_spinner_start (self->spinner); else gtk_spinner_stop (self->spinner); g_object_notify_by_pspec (G_OBJECT (self), loading_properties[LOADING_PROP_ACTIVE]); }

/* Error state */

struct _AwraErrorState {
  GtkWidget parent_instance;
  StateContent content;
  char *title;
  char *description;
};

enum { ERROR_PROP_0, ERROR_PROP_TITLE, ERROR_PROP_DESCRIPTION,
       ERROR_PROP_ACTION, ERROR_N_PROPS };
static GParamSpec *error_properties[ERROR_N_PROPS];

G_DEFINE_FINAL_TYPE (AwraErrorState, awra_error_state, GTK_TYPE_WIDGET)

static void
awra_error_state_dispose (GObject *object)
{
  state_content_dispose (&AWRA_ERROR_STATE (object)->content);
  G_OBJECT_CLASS (awra_error_state_parent_class)->dispose (object);
}

static void
awra_error_state_finalize (GObject *object)
{
  AwraErrorState *self = AWRA_ERROR_STATE (object);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->description, g_free);
  G_OBJECT_CLASS (awra_error_state_parent_class)->finalize (object);
}

static void
awra_error_state_get_property (GObject *object, guint id, GValue *value,
                               GParamSpec *pspec)
{
  AwraErrorState *self = AWRA_ERROR_STATE (object);
  switch (id) {
  case ERROR_PROP_TITLE: g_value_set_string (value, self->title); break;
  case ERROR_PROP_DESCRIPTION: g_value_set_string (value, self->description); break;
  case ERROR_PROP_ACTION: g_value_set_object (value, self->content.action); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_error_state_set_property (GObject *object, guint id, const GValue *value,
                               GParamSpec *pspec)
{
  AwraErrorState *self = AWRA_ERROR_STATE (object);
  switch (id) {
  case ERROR_PROP_TITLE: awra_error_state_set_title (self, g_value_get_string (value)); break;
  case ERROR_PROP_DESCRIPTION: awra_error_state_set_description (self, g_value_get_string (value)); break;
  case ERROR_PROP_ACTION: awra_error_state_set_action (self, g_value_get_object (value)); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_error_state_class_init (AwraErrorStateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_error_state_dispose;
  object_class->finalize = awra_error_state_finalize;
  object_class->get_property = awra_error_state_get_property;
  object_class->set_property = awra_error_state_set_property;
  error_properties[ERROR_PROP_TITLE] = g_param_spec_string (
    "title", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  error_properties[ERROR_PROP_DESCRIPTION] = g_param_spec_string (
    "description", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  error_properties[ERROR_PROP_ACTION] = g_param_spec_object (
    "action", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, ERROR_N_PROPS,
                                     error_properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awraerrorstate");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_ALERT);
}

static void
awra_error_state_init (AwraErrorState *self)
{
  state_content_init (GTK_WIDGET (self), &self->content,
                      "awra-status-error-symbolic");
  gtk_widget_add_css_class (GTK_WIDGET (self->content.icon),
                            "awra-state-error-icon");
}

GtkWidget *awra_error_state_new (void) { return g_object_new (AWRA_TYPE_ERROR_STATE, NULL); }
const char *awra_error_state_get_title (AwraErrorState *self) { g_return_val_if_fail (AWRA_IS_ERROR_STATE (self), NULL); return self->title; }
void awra_error_state_set_title (AwraErrorState *self, const char *title) { g_return_if_fail (AWRA_IS_ERROR_STATE (self)); if (g_strcmp0 (self->title, title) == 0) return; g_free (self->title); self->title = g_strdup (title); gtk_label_set_text (self->content.title, title != NULL ? title : ""); gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL, title != NULL ? title : "", -1); g_object_notify_by_pspec (G_OBJECT (self), error_properties[ERROR_PROP_TITLE]); }
const char *awra_error_state_get_description (AwraErrorState *self) { g_return_val_if_fail (AWRA_IS_ERROR_STATE (self), NULL); return self->description; }
void awra_error_state_set_description (AwraErrorState *self, const char *description) { g_return_if_fail (AWRA_IS_ERROR_STATE (self)); if (g_strcmp0 (self->description, description) == 0) return; g_free (self->description); self->description = g_strdup (description); gtk_label_set_text (self->content.description, description != NULL ? description : ""); gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description != NULL ? description : "", -1); g_object_notify_by_pspec (G_OBJECT (self), error_properties[ERROR_PROP_DESCRIPTION]); }
GtkWidget *awra_error_state_get_action (AwraErrorState *self) { g_return_val_if_fail (AWRA_IS_ERROR_STATE (self), NULL); return self->content.action; }
void awra_error_state_set_action (AwraErrorState *self, GtkWidget *action) { g_return_if_fail (AWRA_IS_ERROR_STATE (self)); g_return_if_fail (action == NULL || GTK_IS_WIDGET (action)); if (self->content.action == action) return; state_content_set_action (&self->content, action); g_object_notify_by_pspec (G_OBJECT (self), error_properties[ERROR_PROP_ACTION]); }
