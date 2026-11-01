/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-data-view.h>
#include <awra/awra-empty-state.h>
#include <awra/awra-error-state.h>
#include <awra/awra-layout.h>
#include <awra/awra-loading-state.h>
#include <awra/awra-typography.h>

#include "awra-i18n-private.h"

struct _AwraDataView {
  GtkWidget parent_instance;
  GtkBox *root;
  GtkStack *stack;
  GtkScrolledWindow *scroller;
  GtkColumnView *delegate;
  GtkBox *loading_more_box;
  GtkSpinner *loading_more_spinner;
  GtkWidget *empty_widget;
  GtkWidget *loading_widget;
  GtkWidget *error_widget;
  GPtrArray *columns;
  AwraDataViewState state;
  gboolean loading_more;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_STATE,
  PROP_EMPTY_WIDGET,
  PROP_LOADING_WIDGET,
  PROP_ERROR_WIDGET,
  PROP_LOADING_MORE,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];
static GtkBuildableIface *parent_buildable_iface;
static void awra_data_view_buildable_init (GtkBuildableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
  AwraDataView, awra_data_view, GTK_TYPE_WIDGET,
  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, awra_data_view_buildable_init))

static const char *
state_name (AwraDataViewState state)
{
  static const char *names[] = { "content", "empty", "loading", "error" };
  return names[state];
}

static GtkWidget **
state_slot (AwraDataView      *self,
            AwraDataViewState  state)
{
  if (state == AWRA_DATA_VIEW_STATE_EMPTY)
    return &self->empty_widget;
  if (state == AWRA_DATA_VIEW_STATE_LOADING)
    return &self->loading_widget;
  if (state == AWRA_DATA_VIEW_STATE_ERROR)
    return &self->error_widget;
  return NULL;
}

static void
replace_state_widget (AwraDataView      *self,
                      AwraDataViewState  state,
                      GtkWidget         *widget)
{
  GtkWidget **slot = state_slot (self, state);

  g_assert (slot != NULL);
  if (*slot == widget)
    return;
  if (*slot != NULL)
    gtk_stack_remove (self->stack, *slot);
  *slot = widget;
  if (widget != NULL)
    gtk_stack_add_named (self->stack, widget, state_name (state));
  if (self->state == state)
    gtk_stack_set_visible_child_name (
      self->stack, widget != NULL ? state_name (state) : "content");
}

static void
awra_data_view_dispose (GObject *object)
{
  AwraDataView *self = AWRA_DATA_VIEW (object);

  if (self->root != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->root));
    self->root = NULL;
    self->stack = NULL;
    self->scroller = NULL;
    self->delegate = NULL;
    self->loading_more_box = NULL;
    self->loading_more_spinner = NULL;
    self->empty_widget = NULL;
    self->loading_widget = NULL;
    self->error_widget = NULL;
  }
  g_clear_pointer (&self->columns, g_ptr_array_unref);
  G_OBJECT_CLASS (awra_data_view_parent_class)->dispose (object);
}

static void
awra_data_view_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  AwraDataView *self = AWRA_DATA_VIEW (widget);

  if (self->root != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->root), snapshot);
}

static void
awra_data_view_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AwraDataView *self = AWRA_DATA_VIEW (object);

  switch (property_id) {
  case PROP_MODEL:
    g_value_set_object (value, awra_data_view_get_model (self));
    break;
  case PROP_STATE:
    g_value_set_enum (value, self->state);
    break;
  case PROP_EMPTY_WIDGET:
    g_value_set_object (value, self->empty_widget);
    break;
  case PROP_LOADING_WIDGET:
    g_value_set_object (value, self->loading_widget);
    break;
  case PROP_ERROR_WIDGET:
    g_value_set_object (value, self->error_widget);
    break;
  case PROP_LOADING_MORE:
    g_value_set_boolean (value, self->loading_more);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_data_view_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  AwraDataView *self = AWRA_DATA_VIEW (object);

  switch (property_id) {
  case PROP_MODEL:
    awra_data_view_set_model (self, g_value_get_object (value));
    break;
  case PROP_STATE:
    awra_data_view_set_state (self, g_value_get_enum (value));
    break;
  case PROP_EMPTY_WIDGET:
    awra_data_view_set_empty_widget (self, g_value_get_object (value));
    break;
  case PROP_LOADING_WIDGET:
    awra_data_view_set_loading_widget (self, g_value_get_object (value));
    break;
  case PROP_ERROR_WIDGET:
    awra_data_view_set_error_widget (self, g_value_get_object (value));
    break;
  case PROP_LOADING_MORE:
    awra_data_view_set_loading_more (self, g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_data_view_class_init (AwraDataViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_data_view_dispose;
  object_class->get_property = awra_data_view_get_property;
  object_class->set_property = awra_data_view_set_property;
  widget_class->snapshot = awra_data_view_snapshot;
  properties[PROP_MODEL] = g_param_spec_object (
    "model", NULL, NULL, GTK_TYPE_SELECTION_MODEL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_STATE] = g_param_spec_enum (
    "state", NULL, NULL, AWRA_TYPE_DATA_VIEW_STATE,
    AWRA_DATA_VIEW_STATE_CONTENT,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_EMPTY_WIDGET] = g_param_spec_object (
    "empty-widget", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_LOADING_WIDGET] = g_param_spec_object (
    "loading-widget", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ERROR_WIDGET] = g_param_spec_object (
    "error-widget", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_LOADING_MORE] = g_param_spec_boolean (
    "loading-more", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awradataview");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_data_view_init (AwraDataView *self)
{
  GtkWidget *loading_more_label = gtk_label_new (_("Loading more…"));
  GtkWidget *empty = awra_empty_state_new ();
  GtkWidget *loading = awra_loading_state_new ();
  GtkWidget *error = awra_error_state_new ();

  self->columns = g_ptr_array_new_with_free_func (g_object_unref);
  self->state = AWRA_DATA_VIEW_STATE_CONTENT;
  self->root = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->stack = GTK_STACK (gtk_stack_new ());
  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  self->delegate = GTK_COLUMN_VIEW (gtk_column_view_new (NULL));
  self->loading_more_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->loading_more_spinner = GTK_SPINNER (gtk_spinner_new ());
  gtk_column_view_set_show_column_separators (self->delegate, FALSE);
  gtk_column_view_set_show_row_separators (self->delegate, FALSE);
  gtk_column_view_set_enable_rubberband (self->delegate, TRUE);
  gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-data-view-delegate");
  gtk_scrolled_window_set_child (self->scroller, GTK_WIDGET (self->delegate));
  gtk_widget_set_vexpand (GTK_WIDGET (self->scroller), TRUE);
  /* A list can be made small: pages that are not shown do not stand in its way. */
  gtk_stack_set_vhomogeneous (self->stack, FALSE);
  gtk_stack_add_named (self->stack, GTK_WIDGET (self->scroller), "content");
  awra_empty_state_set_title (AWRA_EMPTY_STATE (empty), _("No results"));
  awra_loading_state_set_title (AWRA_LOADING_STATE (loading), _("Loading"));
  awra_error_state_set_title (AWRA_ERROR_STATE (error),
                              _("Could not load data"));
  self->empty_widget = empty;
  self->loading_widget = loading;
  self->error_widget = error;
  gtk_stack_add_named (self->stack, empty, "empty");
  gtk_stack_add_named (self->stack, loading, "loading");
  gtk_stack_add_named (self->stack, error, "error");
  gtk_stack_set_visible_child_name (self->stack, "content");
  gtk_widget_set_vexpand (GTK_WIDGET (self->stack), TRUE);
  awra_widget_set_typography (loading_more_label, AWRA_TYPOGRAPHY_MUTED);
  awra_box_set_spacing (self->loading_more_box, AWRA_SPACING_SM);
  gtk_widget_set_halign (GTK_WIDGET (self->loading_more_box), GTK_ALIGN_CENTER);
  gtk_box_append (self->loading_more_box, GTK_WIDGET (self->loading_more_spinner));
  gtk_box_append (self->loading_more_box, loading_more_label);
  gtk_widget_set_visible (GTK_WIDGET (self->loading_more_box), FALSE);
  gtk_box_append (self->root, GTK_WIDGET (self->stack));
  gtk_box_append (self->root, GTK_WIDGET (self->loading_more_box));
  gtk_widget_set_parent (GTK_WIDGET (self->root), GTK_WIDGET (self));
}

static void
awra_data_view_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const char   *type)
{
  AwraDataView *self = AWRA_DATA_VIEW (buildable);

  if (g_strcmp0 (type, "column") == 0 && AWRA_IS_DATA_COLUMN (child))
    awra_data_view_append_column (self, AWRA_DATA_COLUMN (child));
  else if (g_strcmp0 (type, "empty") == 0 && GTK_IS_WIDGET (child))
    awra_data_view_set_empty_widget (self, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "loading") == 0 && GTK_IS_WIDGET (child))
    awra_data_view_set_loading_widget (self, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "error") == 0 && GTK_IS_WIDGET (child))
    awra_data_view_set_error_widget (self, GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
awra_data_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = awra_data_view_buildable_add_child;
}

GtkWidget *
awra_data_view_new (GtkSelectionModel *model)
{
  return g_object_new (AWRA_TYPE_DATA_VIEW, "model", model, NULL);
}

GtkSelectionModel *
awra_data_view_get_model (AwraDataView *self)
{
  g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), NULL);
  return gtk_column_view_get_model (self->delegate);
}

void
awra_data_view_set_model (AwraDataView      *self,
                          GtkSelectionModel *model)
{
  g_return_if_fail (AWRA_IS_DATA_VIEW (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));
  if (gtk_column_view_get_model (self->delegate) == model)
    return;
  gtk_column_view_set_model (self->delegate, model);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

void
awra_data_view_append_column (AwraDataView   *self,
                              AwraDataColumn *column)
{
  g_return_if_fail (AWRA_IS_DATA_VIEW (self));
  awra_data_view_insert_column (self, column, self->columns->len);
}

void
awra_data_view_insert_column (AwraDataView   *self,
                              AwraDataColumn *column,
                              guint           position)
{
  g_return_if_fail (AWRA_IS_DATA_VIEW (self));
  g_return_if_fail (AWRA_IS_DATA_COLUMN (column));
  g_return_if_fail (position <= self->columns->len);
  if (g_ptr_array_find (self->columns, column, NULL))
    return;
  gtk_column_view_insert_column (self->delegate, position,
                                 awra_data_column_get_delegate (column));
  g_ptr_array_insert (self->columns, position, g_object_ref (column));
}

void
awra_data_view_remove_column (AwraDataView   *self,
                              AwraDataColumn *column)
{
  guint position;

  g_return_if_fail (AWRA_IS_DATA_VIEW (self));
  g_return_if_fail (AWRA_IS_DATA_COLUMN (column));
  if (!g_ptr_array_find (self->columns, column, &position))
    return;
  gtk_column_view_remove_column (self->delegate,
                                 awra_data_column_get_delegate (column));
  g_ptr_array_remove_index (self->columns, position);
}

guint
awra_data_view_get_n_columns (AwraDataView *self)
{
  g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), 0);
  return self->columns->len;
}

AwraDataColumn *
awra_data_view_get_column (AwraDataView *self,
                           guint         position)
{
  g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), NULL);
  return position < self->columns->len
           ? g_ptr_array_index (self->columns, position) : NULL;
}

AwraDataViewState
awra_data_view_get_state (AwraDataView *self)
{
  g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), AWRA_DATA_VIEW_STATE_CONTENT);
  return self->state;
}

void
awra_data_view_set_state (AwraDataView      *self,
                          AwraDataViewState  state)
{
  GtkWidget **slot;

  g_return_if_fail (AWRA_IS_DATA_VIEW (self));
  g_return_if_fail (state >= AWRA_DATA_VIEW_STATE_CONTENT &&
                    state <= AWRA_DATA_VIEW_STATE_ERROR);
  if (self->state == state)
    return;
  self->state = state;
  slot = state_slot (self, state);
  gtk_stack_set_visible_child_name (
    self->stack, slot == NULL || *slot != NULL ? state_name (state) : "content");
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

GtkWidget *awra_data_view_get_empty_widget (AwraDataView *self) { g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), NULL); return self->empty_widget; }
void awra_data_view_set_empty_widget (AwraDataView *self, GtkWidget *widget) { g_return_if_fail (AWRA_IS_DATA_VIEW (self)); g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget)); replace_state_widget (self, AWRA_DATA_VIEW_STATE_EMPTY, widget); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EMPTY_WIDGET]); }
GtkWidget *awra_data_view_get_loading_widget (AwraDataView *self) { g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), NULL); return self->loading_widget; }
void awra_data_view_set_loading_widget (AwraDataView *self, GtkWidget *widget) { g_return_if_fail (AWRA_IS_DATA_VIEW (self)); g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget)); replace_state_widget (self, AWRA_DATA_VIEW_STATE_LOADING, widget); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING_WIDGET]); }
GtkWidget *awra_data_view_get_error_widget (AwraDataView *self) { g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), NULL); return self->error_widget; }
void awra_data_view_set_error_widget (AwraDataView *self, GtkWidget *widget) { g_return_if_fail (AWRA_IS_DATA_VIEW (self)); g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget)); replace_state_widget (self, AWRA_DATA_VIEW_STATE_ERROR, widget); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR_WIDGET]); }

gboolean
awra_data_view_get_loading_more (AwraDataView *self)
{
  g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), FALSE);
  return self->loading_more;
}

void
awra_data_view_set_loading_more (AwraDataView *self,
                                 gboolean      loading_more)
{
  g_return_if_fail (AWRA_IS_DATA_VIEW (self));
  loading_more = !!loading_more;
  if (self->loading_more == loading_more)
    return;
  self->loading_more = loading_more;
  gtk_widget_set_visible (GTK_WIDGET (self->loading_more_box), loading_more);
  if (loading_more)
    gtk_spinner_start (self->loading_more_spinner);
  else
    gtk_spinner_stop (self->loading_more_spinner);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING_MORE]);
}

GtkColumnView *
awra_data_view_get_delegate (AwraDataView *self)
{
  g_return_val_if_fail (AWRA_IS_DATA_VIEW (self), NULL);
  return self->delegate;
}
