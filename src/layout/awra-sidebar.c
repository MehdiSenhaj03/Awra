/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-sidebar.h>
#include <awra/awra-layout.h>

struct _AwraSidebar {
  GtkWidget parent_instance;
  AwraSurface *surface;
  guint inset;
};
enum { PROP_0, PROP_CHILD, PROP_SURFACE, PROP_INSET, N_PROPS };
static GParamSpec *properties[N_PROPS];
G_DEFINE_FINAL_TYPE (AwraSidebar, awra_sidebar, GTK_TYPE_WIDGET)

static void awra_sidebar_dispose (GObject *object) {
  AwraSidebar *self = AWRA_SIDEBAR (object);
  if (self->surface != NULL) { gtk_widget_unparent (GTK_WIDGET (self->surface)); self->surface = NULL; }
  G_OBJECT_CLASS (awra_sidebar_parent_class)->dispose (object);
}
static void awra_sidebar_snapshot (GtkWidget *widget, GtkSnapshot *snapshot) {
  gtk_widget_snapshot_child (widget, GTK_WIDGET (AWRA_SIDEBAR (widget)->surface), snapshot);
}
static void awra_sidebar_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) {
  AwraSidebar *self = AWRA_SIDEBAR (object);
  switch (id) {
  case PROP_CHILD: g_value_set_object (value, awra_sidebar_get_child (self)); break;
  case PROP_SURFACE: g_value_set_object (value, self->surface); break;
  case PROP_INSET: g_value_set_uint (value, self->inset); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}
static void awra_sidebar_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) {
  switch (id) {
  case PROP_CHILD:
    awra_sidebar_set_child (AWRA_SIDEBAR (object), g_value_get_object (value));
    break;
  case PROP_INSET:
    awra_sidebar_set_inset (AWRA_SIDEBAR (object), g_value_get_uint (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}
static void awra_sidebar_class_init (AwraSidebarClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->dispose = awra_sidebar_dispose;
  object_class->get_property = awra_sidebar_get_property;
  object_class->set_property = awra_sidebar_set_property;
  widget_class->snapshot = awra_sidebar_snapshot;
  properties[PROP_CHILD] = g_param_spec_object ("child", NULL, NULL, GTK_TYPE_WIDGET,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_SURFACE] = g_param_spec_object ("surface", NULL, NULL, AWRA_TYPE_SURFACE,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_INSET] = g_param_spec_uint (
    "inset", NULL, NULL, 0, 128, 12,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrasidebar");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_NAVIGATION);
}
static void awra_sidebar_init (AwraSidebar *self) {
  self->surface = AWRA_SURFACE (awra_surface_new_with_role (AWRA_SURFACE_ROLE_SIDEBAR));
  gtk_widget_set_parent (GTK_WIDGET (self->surface), GTK_WIDGET (self));
  awra_sidebar_set_inset_spacing (self, AWRA_SPACING_MD);
}
GtkWidget *awra_sidebar_new (void) { return g_object_new (AWRA_TYPE_SIDEBAR, NULL); }
GtkWidget *awra_sidebar_get_child (AwraSidebar *self) {
  g_return_val_if_fail (AWRA_IS_SIDEBAR (self), NULL); return awra_surface_get_child (self->surface);
}
void awra_sidebar_set_child (AwraSidebar *self, GtkWidget *child) {
  g_return_if_fail (AWRA_IS_SIDEBAR (self)); if (awra_sidebar_get_child (self) == child) return;
  awra_surface_set_child (self->surface, child); g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}
AwraSurface *awra_sidebar_get_surface (AwraSidebar *self) {
  g_return_val_if_fail (AWRA_IS_SIDEBAR (self), NULL); return self->surface;
}
guint awra_sidebar_get_inset (AwraSidebar *self) {
  g_return_val_if_fail (AWRA_IS_SIDEBAR (self), 0); return self->inset;
}
void awra_sidebar_set_inset (AwraSidebar *self, guint inset) {
  g_return_if_fail (AWRA_IS_SIDEBAR (self));
  g_return_if_fail (inset <= 128);
  if (self->inset == inset) return;
  self->inset = inset;
  gtk_widget_set_margin_top (GTK_WIDGET (self->surface), (int) inset);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self->surface), (int) inset);
  gtk_widget_set_margin_start (GTK_WIDGET (self->surface), (int) inset);
  gtk_widget_set_margin_end (GTK_WIDGET (self->surface), (int) inset);
  /* Docked flush, the sidebar is a straight band: the window's own rounded
   * frame clips its outer corners, and only the edge it shares with the
   * content is drawn, so no corner or rim meets a titlebar or the content
   * as a second contour. Inset, it shows its whole rounded silhouette. */
  awra_surface_set_corner_mask (self->surface, inset == 0 ? AWRA_SURFACE_CORNER_NONE
                                                          : AWRA_SURFACE_CORNER_ALL);
  awra_surface_set_edge_mask (self->surface, inset == 0 ? AWRA_SURFACE_EDGE_END
                                                        : AWRA_SURFACE_EDGE_ALL);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INSET]);
}

void
awra_sidebar_set_inset_spacing (AwraSidebar *self,
                                AwraSpacing  spacing)
{
  g_return_if_fail (AWRA_IS_SIDEBAR (self));
  awra_sidebar_set_inset (
    self, (guint) awra_spacing_resolve (GTK_WIDGET (self), spacing));
}
