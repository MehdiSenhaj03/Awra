/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-breadcrumb-bar.h>
#include <awra/awra-button.h>
#include <awra/awra-layout.h>

struct _AwraBreadcrumbItem {
  GObject parent_instance;
  char *label;
  char *name;
};

enum { ITEM_PROP_0, ITEM_PROP_LABEL, ITEM_PROP_NAME, ITEM_N_PROPS };
static GParamSpec *item_properties[ITEM_N_PROPS];
G_DEFINE_FINAL_TYPE (AwraBreadcrumbItem, awra_breadcrumb_item, G_TYPE_OBJECT)

static void
awra_breadcrumb_item_finalize (GObject *object)
{
  AwraBreadcrumbItem *self = AWRA_BREADCRUMB_ITEM (object);
  g_free (self->label);
  g_free (self->name);
  G_OBJECT_CLASS (awra_breadcrumb_item_parent_class)->finalize (object);
}

static void
awra_breadcrumb_item_get_property (GObject *object, guint id, GValue *value,
                                   GParamSpec *pspec)
{
  AwraBreadcrumbItem *self = AWRA_BREADCRUMB_ITEM (object);
  switch (id) {
  case ITEM_PROP_LABEL: g_value_set_string (value, self->label); break;
  case ITEM_PROP_NAME: g_value_set_string (value, self->name); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_breadcrumb_item_set_property (GObject *object, guint id,
                                   const GValue *value, GParamSpec *pspec)
{
  switch (id) {
  case ITEM_PROP_LABEL:
    awra_breadcrumb_item_set_label (AWRA_BREADCRUMB_ITEM (object),
                                    g_value_get_string (value));
    break;
  case ITEM_PROP_NAME:
    awra_breadcrumb_item_set_name (AWRA_BREADCRUMB_ITEM (object),
                                   g_value_get_string (value));
    break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void
awra_breadcrumb_item_class_init (AwraBreadcrumbItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = awra_breadcrumb_item_finalize;
  object_class->get_property = awra_breadcrumb_item_get_property;
  object_class->set_property = awra_breadcrumb_item_set_property;
  item_properties[ITEM_PROP_LABEL] = g_param_spec_string (
    "label", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  item_properties[ITEM_PROP_NAME] = g_param_spec_string (
    "name", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, ITEM_N_PROPS,
                                     item_properties);
}

static void awra_breadcrumb_item_init (AwraBreadcrumbItem *self) { (void) self; }

AwraBreadcrumbItem *
awra_breadcrumb_item_new (const char *label, const char *name)
{
  return g_object_new (AWRA_TYPE_BREADCRUMB_ITEM,
                       "label", label, "name", name, NULL);
}

const char *awra_breadcrumb_item_get_label (AwraBreadcrumbItem *self) { g_return_val_if_fail (AWRA_IS_BREADCRUMB_ITEM (self), NULL); return self->label; }
void awra_breadcrumb_item_set_label (AwraBreadcrumbItem *self, const char *label) { g_return_if_fail (AWRA_IS_BREADCRUMB_ITEM (self)); if (g_strcmp0 (self->label, label) == 0) return; g_free (self->label); self->label = g_strdup (label); g_object_notify_by_pspec (G_OBJECT (self), item_properties[ITEM_PROP_LABEL]); }
const char *awra_breadcrumb_item_get_name (AwraBreadcrumbItem *self) { g_return_val_if_fail (AWRA_IS_BREADCRUMB_ITEM (self), NULL); return self->name; }
void awra_breadcrumb_item_set_name (AwraBreadcrumbItem *self, const char *name) { g_return_if_fail (AWRA_IS_BREADCRUMB_ITEM (self)); if (g_strcmp0 (self->name, name) == 0) return; g_free (self->name); self->name = g_strdup (name); g_object_notify_by_pspec (G_OBJECT (self), item_properties[ITEM_PROP_NAME]); }

struct _AwraBreadcrumbBar {
  GtkWidget parent_instance;
  GtkBox *box;
  GtkScrolledWindow *scroller;
  GListModel *model;
  gulong items_changed_handler;
  guint reveal_idle;
};

enum { BAR_PROP_0, BAR_PROP_MODEL, BAR_N_PROPS };
enum { ACTIVATE_ITEM, BAR_N_SIGNALS };
static GParamSpec *bar_properties[BAR_N_PROPS];
static guint bar_signals[BAR_N_SIGNALS];
G_DEFINE_FINAL_TYPE (AwraBreadcrumbBar, awra_breadcrumb_bar, GTK_TYPE_WIDGET)

static void
breadcrumb_clicked_cb (GtkButton *button, AwraBreadcrumbBar *self)
{
  AwraBreadcrumbItem *item = g_object_get_data (G_OBJECT (button),
                                                 "awra-breadcrumb-item");
  g_signal_emit (self, bar_signals[ACTIVATE_ITEM], 0, item);
}

static void
rebuild_bar (AwraBreadcrumbBar *self)
{
  GtkWidget *child;
  guint count = self->model != NULL ? g_list_model_get_n_items (self->model) : 0;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->box))) != NULL)
    gtk_box_remove (self->box, child);

  for (guint i = 0; i < count; i++) {
    g_autoptr (AwraBreadcrumbItem) item = g_list_model_get_item (self->model, i);
    GtkWidget *button;

    if (i > 0) {
      GtkWidget *separator = gtk_image_new_from_icon_name (
        "go-next-symbolic");
      gtk_widget_add_css_class (separator, "awra-breadcrumb-separator");
      gtk_accessible_update_state (GTK_ACCESSIBLE (separator),
                                   GTK_ACCESSIBLE_STATE_HIDDEN, TRUE, -1);
      gtk_box_append (self->box, separator);
    }
    button = awra_button_new_with_label (
      awra_breadcrumb_item_get_label (item));
    awra_button_set_appearance (AWRA_BUTTON (button),
                                AWRA_BUTTON_APPEARANCE_TOOLBAR);
    gtk_widget_add_css_class (button, "awra-breadcrumb-button");
    if (i + 1 == count)
      gtk_widget_add_css_class (button, "awra-breadcrumb-current");
    g_object_set_data_full (G_OBJECT (button), "awra-breadcrumb-item",
                            g_object_ref (item), g_object_unref);
    g_signal_connect (button, "clicked", G_CALLBACK (breadcrumb_clicked_cb),
                      self);
    gtk_box_append (self->box, button);
  }
}

static void
items_changed_cb (GListModel *model, guint position, guint removed,
                  guint added, AwraBreadcrumbBar *self)
{
  (void) model; (void) position; (void) removed; (void) added;
  rebuild_bar (self);
}

static void
awra_breadcrumb_bar_dispose (GObject *object)
{
  AwraBreadcrumbBar *self = AWRA_BREADCRUMB_BAR (object);
  if (self->reveal_idle != 0) {
    g_source_remove (self->reveal_idle);
    self->reveal_idle = 0;
  }
  if (self->items_changed_handler != 0) {
    g_signal_handler_disconnect (self->model, self->items_changed_handler);
    self->items_changed_handler = 0;
  }
  g_clear_object (&self->model);
  if (self->scroller != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->scroller));
    self->scroller = NULL;
    self->box = NULL;
  }
  G_OBJECT_CLASS (awra_breadcrumb_bar_parent_class)->dispose (object);
}

static void awra_breadcrumb_bar_snapshot (GtkWidget *widget, GtkSnapshot *snapshot) { gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_BREADCRUMB_BAR (widget)->scroller), snapshot); }
static void awra_breadcrumb_bar_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) { if (id == BAR_PROP_MODEL) g_value_set_object (value, AWRA_BREADCRUMB_BAR (object)->model); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }
static void awra_breadcrumb_bar_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) { if (id == BAR_PROP_MODEL) awra_breadcrumb_bar_set_model (AWRA_BREADCRUMB_BAR (object), g_value_get_object (value)); else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec); }

static void
awra_breadcrumb_bar_class_init (AwraBreadcrumbBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_breadcrumb_bar_dispose;
  object_class->get_property = awra_breadcrumb_bar_get_property;
  object_class->set_property = awra_breadcrumb_bar_set_property;
  widget_class->snapshot = awra_breadcrumb_bar_snapshot;
  bar_properties[BAR_PROP_MODEL] = g_param_spec_object (
    "model", NULL, NULL, G_TYPE_LIST_MODEL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, BAR_N_PROPS, bar_properties);
  bar_signals[ACTIVATE_ITEM] = g_signal_new (
    "activate-item", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
    NULL, NULL, NULL, G_TYPE_NONE, 1, AWRA_TYPE_BREADCRUMB_ITEM);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrabreadcrumbbar");
  gtk_widget_class_set_accessible_role (widget_class,
                                        GTK_ACCESSIBLE_ROLE_NAVIGATION);
}

static gboolean
reveal_current_idle (gpointer data)
{
  AwraBreadcrumbBar *self = data;
  GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (self->scroller);
  self->reveal_idle = 0;
  double value = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL
                   ? gtk_adjustment_get_lower (adjustment)
                   : gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment);
  gtk_adjustment_set_value (adjustment, value);
  return G_SOURCE_REMOVE;
}

static void
reveal_current_cb (GtkAdjustment *adjustment, AwraBreadcrumbBar *self)
{
  (void) adjustment;
  /* GtkViewport configures the range while allocating its child. Scrolling
   * synchronously there can leave a correct adjustment with an old viewport
   * transform. Reveal after allocation, once the viewport can consume it. */
  if (self->reveal_idle == 0)
    self->reveal_idle = g_idle_add (reveal_current_idle, self);
}

static void
awra_breadcrumb_bar_init (AwraBreadcrumbBar *self)
{
  self->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  awra_box_set_spacing (self->box, AWRA_SPACING_XS);
  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  gtk_scrolled_window_set_policy (self->scroller, GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
  gtk_scrolled_window_set_child (self->scroller, GTK_WIDGET (self->box));
  gtk_widget_set_parent (GTK_WIDGET (self->scroller), GTK_WIDGET (self));
  g_signal_connect (gtk_scrolled_window_get_hadjustment (self->scroller),
                    "changed", G_CALLBACK (reveal_current_cb), self);
}

GtkWidget *awra_breadcrumb_bar_new (void) { return g_object_new (AWRA_TYPE_BREADCRUMB_BAR, NULL); }
GListModel *awra_breadcrumb_bar_get_model (AwraBreadcrumbBar *self) { g_return_val_if_fail (AWRA_IS_BREADCRUMB_BAR (self), NULL); return self->model; }
void awra_breadcrumb_bar_set_model (AwraBreadcrumbBar *self, GListModel *model) { g_return_if_fail (AWRA_IS_BREADCRUMB_BAR (self)); g_return_if_fail (model == NULL || g_list_model_get_item_type (model) == AWRA_TYPE_BREADCRUMB_ITEM); if (self->model == model) return; if (self->items_changed_handler != 0) { g_signal_handler_disconnect (self->model, self->items_changed_handler); self->items_changed_handler = 0; } g_set_object (&self->model, model); if (model != NULL) self->items_changed_handler = g_signal_connect (model, "items-changed", G_CALLBACK (items_changed_cb), self); rebuild_bar (self); g_object_notify_by_pspec (G_OBJECT (self), bar_properties[BAR_PROP_MODEL]); }
