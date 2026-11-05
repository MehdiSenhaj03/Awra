/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-color-palette.h>
#include <awra/awra-toggle-button.h>
#include <awra/awra-token-set.h>

#include "awra-i18n-private.h"
#include "widgets/awra-widget-style-private.h"

/* A palette is a flow of swatch buttons. Each swatch draws a dot of its
 * colour, with a quiet outline so pale colours keep their edge, and, once
 * chosen, a ring of the colour around it. The automatic choice leads, a
 * colour from outside the palette follows the palette's own, and the
 * button for any colour ends it. */

#define DOT 24

/* The dot inside a swatch button. */
#define AWRA_TYPE_SWATCH_DOT (awra_swatch_dot_get_type ())
G_DECLARE_FINAL_TYPE (AwraSwatchDot, awra_swatch_dot, AWRA, SWATCH_DOT, GtkWidget)

struct _AwraSwatchDot {
  GtkWidget parent_instance;
  GdkRGBA color;
  gboolean chosen;
};

G_DEFINE_FINAL_TYPE (AwraSwatchDot, awra_swatch_dot, GTK_TYPE_WIDGET)

static void
awra_swatch_dot_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  (void) widget;
  (void) orientation;
  (void) for_size;
  *minimum = *natural = DOT;
  *minimum_baseline = *natural_baseline = -1;
}

static void
append_circle (GtkSnapshot   *snapshot,
               float          inset,
               const GdkRGBA *color)
{
  GskRoundedRect circle;

  gsk_rounded_rect_init_from_rect (&circle, &GRAPHENE_RECT_INIT (inset, inset, DOT - 2 * inset, DOT - 2 * inset),
                                   DOT / 2.0f - inset);
  gtk_snapshot_push_rounded_clip (snapshot, &circle);
  gtk_snapshot_append_color (snapshot, color, &circle.bounds);
  gtk_snapshot_pop (snapshot);
}

static void
awra_swatch_dot_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  AwraSwatchDot *self = AWRA_SWATCH_DOT (widget);
  gboolean dark = awra_token_set_get_dark (awra_widget_get_token_set (widget));
  GdkRGBA foreground;
  GdkRGBA edge;
  GdkRGBA ring = awra_rgba_readable (&self->color, dark);
  GskRoundedRect circle;
  float width[4] = { 2, 2, 2, 2 };
  GdkRGBA rings[4] = { ring, ring, ring, ring };
  float inset = self->chosen ? 5.0f : 2.0f;

  gtk_widget_get_color (widget, &foreground);
  edge = awra_rgba_with_alpha (&foreground, 0.18);
  /* The outline, then the colour inside it. */
  append_circle (snapshot, inset - 1.0f, &edge);
  append_circle (snapshot, inset, &self->color);
  if (self->chosen) {
    gsk_rounded_rect_init_from_rect (&circle, &GRAPHENE_RECT_INIT (0, 0, DOT, DOT), DOT / 2.0f);
    gtk_snapshot_append_border (snapshot, &circle, width, rings);
  }
}

static void
awra_swatch_dot_class_init (AwraSwatchDotClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = awra_swatch_dot_measure;
  widget_class->snapshot = awra_swatch_dot_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awraswatchdot");
}

static void
awra_swatch_dot_init (AwraSwatchDot *self)
{
  gtk_widget_set_can_target (GTK_WIDGET (self), FALSE);
  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
}

struct _AwraColorPalette {
  GtkWidget parent_instance;
  GtkWidget *root;
  GtkWidget *flow;
  GtkWidget *automatic;
  GtkWidget *custom;
  GtkWidget *more;
  GPtrArray *swatches;
  GdkRGBA selected;
  gboolean has_selected;
  char *automatic_label;
  gboolean allows_custom;
  GCancellable *choosing;
};

enum {
  PROP_0,
  PROP_SELECTED,
  PROP_AUTOMATIC_LABEL,
  PROP_ALLOWS_CUSTOM,
  N_PROPS,
};

enum {
  SIGNAL_CHOSEN,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraColorPalette, awra_color_palette, GTK_TYPE_WIDGET)

static AwraSwatchDot *
dot_of (GtkWidget *swatch)
{
  return AWRA_SWATCH_DOT (gtk_button_get_child (GTK_BUTTON (swatch)));
}

static void
mark (GtkWidget *button,
      gboolean   chosen)
{
  gtk_accessible_update_state (GTK_ACCESSIBLE (button), GTK_ACCESSIBLE_STATE_CHECKED,
                               chosen ? GTK_ACCESSIBLE_TRISTATE_TRUE : GTK_ACCESSIBLE_TRISTATE_FALSE, -1);
  if (chosen)
    gtk_widget_add_css_class (button, "awra-selected");
  else
    gtk_widget_remove_css_class (button, "awra-selected");
}

/* Shows the selection on the swatches, the custom one when the chosen
 * colour isn't the palette's. */
static void
show_selection (AwraColorPalette *self)
{
  gboolean found = !self->has_selected;

  for (guint i = 0; i < self->swatches->len; i++) {
    GtkWidget *swatch = g_ptr_array_index (self->swatches, i);
    AwraSwatchDot *dot = dot_of (swatch);
    gboolean chosen = self->has_selected && gdk_rgba_equal (&dot->color, &self->selected);

    found |= chosen;
    if (dot->chosen != chosen) {
      dot->chosen = chosen;
      gtk_widget_queue_draw (GTK_WIDGET (dot));
    }
    mark (swatch, chosen);
  }
  gtk_widget_set_visible (gtk_widget_get_parent (self->custom), !found);
  if (!found) {
    AwraSwatchDot *dot = dot_of (self->custom);
    g_autofree char *hex = g_strdup_printf ("#%02x%02x%02x",
                                            (int) (self->selected.red * 255 + 0.5),
                                            (int) (self->selected.green * 255 + 0.5),
                                            (int) (self->selected.blue * 255 + 0.5));

    dot->color = self->selected;
    dot->chosen = TRUE;
    gtk_widget_queue_draw (GTK_WIDGET (dot));
    gtk_widget_set_tooltip_text (self->custom, hex);
    gtk_accessible_update_property (GTK_ACCESSIBLE (self->custom), GTK_ACCESSIBLE_PROPERTY_LABEL, hex, -1);
  }
  mark (self->custom, !found);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->automatic), !self->has_selected);
  mark (self->automatic, !self->has_selected);
}

static void
choose (AwraColorPalette *self,
        const GdkRGBA    *color)
{
  awra_color_palette_set_selected (self, color);
  g_signal_emit (self, signals[SIGNAL_CHOSEN], 0, color);
}

static void
swatch_clicked_cb (GtkButton        *button,
                   AwraColorPalette *self)
{
  GdkRGBA color = dot_of (GTK_WIDGET (button))->color;

  choose (self, &color);
}

static void
automatic_clicked_cb (GtkButton        *button,
                      AwraColorPalette *self)
{
  (void) button;
  /* A toggle button turns itself off when clicked while on: keep it on. */
  choose (self, NULL);
  show_selection (self);
}

static void
chosen_cb (GObject      *source,
           GAsyncResult *result,
           gpointer      user_data)
{
  g_autoptr (GdkRGBA) color = gtk_color_dialog_choose_rgba_finish (GTK_COLOR_DIALOG (source), result, NULL);
  AwraColorPalette *self = user_data;

  if (color != NULL) {
    color->alpha = 1.0;
    choose (self, color);
  }
  g_clear_object (&self->choosing);
  g_object_unref (self);
}

static void
more_clicked_cb (GtkButton        *button,
                 AwraColorPalette *self)
{
  g_autoptr (GtkColorDialog) dialog = gtk_color_dialog_new ();
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (button));

  if (self->choosing != NULL)
    return;
  gtk_color_dialog_set_with_alpha (dialog, FALSE);
  gtk_color_dialog_set_modal (dialog, TRUE);
  self->choosing = g_cancellable_new ();
  gtk_color_dialog_choose_rgba (dialog, GTK_IS_WINDOW (root) ? GTK_WINDOW (root) : NULL,
                                self->has_selected ? &self->selected : NULL,
                                self->choosing, chosen_cb, g_object_ref (self));
}

static GtkWidget *
swatch_new (const GdkRGBA *color)
{
  GtkWidget *button = g_object_new (GTK_TYPE_BUTTON, "accessible-role", GTK_ACCESSIBLE_ROLE_RADIO, NULL);
  AwraSwatchDot *dot = g_object_new (AWRA_TYPE_SWATCH_DOT, NULL);

  dot->color = *color;
  gtk_button_set_child (GTK_BUTTON (button), GTK_WIDGET (dot));
  gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
  gtk_widget_add_css_class (button, "awra-color-swatch");
  gtk_accessible_update_state (GTK_ACCESSIBLE (button), GTK_ACCESSIBLE_STATE_CHECKED,
                               GTK_ACCESSIBLE_TRISTATE_FALSE, -1);
  return button;
}

static void
awra_color_palette_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  AwraColorPalette *self = AWRA_COLOR_PALETTE (object);

  switch (property_id) {
  case PROP_SELECTED:
    g_value_set_boxed (value, awra_color_palette_get_selected (self));
    break;
  case PROP_AUTOMATIC_LABEL:
    g_value_set_string (value, self->automatic_label);
    break;
  case PROP_ALLOWS_CUSTOM:
    g_value_set_boolean (value, self->allows_custom);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_color_palette_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  AwraColorPalette *self = AWRA_COLOR_PALETTE (object);

  switch (property_id) {
  case PROP_SELECTED:
    awra_color_palette_set_selected (self, g_value_get_boxed (value));
    break;
  case PROP_AUTOMATIC_LABEL:
    awra_color_palette_set_automatic_label (self, g_value_get_string (value));
    break;
  case PROP_ALLOWS_CUSTOM:
    awra_color_palette_set_allows_custom (self, g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_color_palette_dispose (GObject *object)
{
  AwraColorPalette *self = AWRA_COLOR_PALETTE (object);

  if (self->choosing != NULL)
    g_cancellable_cancel (self->choosing);
  g_clear_pointer (&self->root, gtk_widget_unparent);
  self->flow = NULL;
  g_clear_pointer (&self->swatches, g_ptr_array_unref);
  G_OBJECT_CLASS (awra_color_palette_parent_class)->dispose (object);
}

static void
awra_color_palette_finalize (GObject *object)
{
  g_free (AWRA_COLOR_PALETTE (object)->automatic_label);
  G_OBJECT_CLASS (awra_color_palette_parent_class)->finalize (object);
}

static void
awra_color_palette_class_init (AwraColorPaletteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = awra_color_palette_get_property;
  object_class->set_property = awra_color_palette_set_property;
  object_class->dispose = awra_color_palette_dispose;
  object_class->finalize = awra_color_palette_finalize;
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awracolorpalette");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_RADIO_GROUP);

  /**
   * AwraColorPalette:selected:
   *
   * The chosen colour, or %NULL for the automatic choice.
   *
   * Since: 1.1
   */
  properties[PROP_SELECTED] = g_param_spec_boxed (
    "selected", NULL, NULL, GDK_TYPE_RGBA,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraColorPalette:automatic-label:
   *
   * The label of a leading choice that picks no colour, or %NULL for none.
   *
   * Since: 1.1
   */
  properties[PROP_AUTOMATIC_LABEL] = g_param_spec_string (
    "automatic-label", NULL, NULL, NULL,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraColorPalette:allows-custom:
   *
   * Whether a last swatch opens the system's colour chooser.
   *
   * Since: 1.1
   */
  properties[PROP_ALLOWS_CUSTOM] = g_param_spec_boolean (
    "allows-custom", NULL, NULL, FALSE,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraColorPalette::chosen:
   * @self: the colour palette
   * @color: (nullable): the colour the person chose, or %NULL for the
   *   automatic choice
   *
   * The person chose a colour. Unlike #GObject::notify on
   * #AwraColorPalette:selected, it isn't emitted when the application sets
   * the selection.
   *
   * Since: 1.1
   */
  signals[SIGNAL_CHOSEN] = g_signal_new (
    "chosen", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
    G_TYPE_NONE, 1, GDK_TYPE_RGBA | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
awra_color_palette_init (AwraColorPalette *self)
{
  GtkWidget *holder;

  self->swatches = g_ptr_array_new ();
  self->flow = gtk_flow_box_new ();
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->flow), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self->flow), FALSE);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->flow), 64);
  gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (self->flow), 2);
  gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (self->flow), 2);
  gtk_widget_set_hexpand (self->flow, TRUE);
  /* The automatic choice stands apart: in the flow box, its width would
   * set a column's and leave a gap in every row under it. */
  self->root = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_parent (self->root, GTK_WIDGET (self));

  self->automatic = awra_toggle_button_new_with_label ("");
  gtk_widget_add_css_class (self->automatic, "awra-color-automatic");
  gtk_widget_set_valign (self->automatic, GTK_ALIGN_START);
  g_signal_connect (self->automatic, "clicked", G_CALLBACK (automatic_clicked_cb), self);
  gtk_widget_set_visible (self->automatic, FALSE);
  gtk_box_append (GTK_BOX (self->root), self->automatic);
  gtk_box_append (GTK_BOX (self->root), self->flow);

  self->custom = swatch_new (&(GdkRGBA) { 0.5, 0.5, 0.5, 1.0 });
  gtk_widget_set_can_target (self->custom, FALSE);
  gtk_flow_box_append (GTK_FLOW_BOX (self->flow), self->custom);
  gtk_widget_set_visible (gtk_widget_get_parent (self->custom), FALSE);

  self->more = gtk_button_new_from_icon_name ("list-add-symbolic");
  gtk_button_set_has_frame (GTK_BUTTON (self->more), FALSE);
  gtk_widget_add_css_class (self->more, "awra-color-swatch");
  gtk_widget_add_css_class (self->more, "awra-color-more");
  gtk_widget_set_tooltip_text (self->more, _("Any colour…"));
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->more), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  _("Any colour…"), -1);
  g_signal_connect (self->more, "clicked", G_CALLBACK (more_clicked_cb), self);
  gtk_flow_box_append (GTK_FLOW_BOX (self->flow), self->more);
  holder = gtk_widget_get_parent (self->more);
  gtk_widget_set_visible (holder, FALSE);
  /* Flow box children take focus themselves; the buttons inside do it. */
  for (GtkWidget *child = gtk_widget_get_first_child (self->flow); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    gtk_widget_set_focusable (child, FALSE);
}

GtkWidget *
awra_color_palette_new (void)
{
  return g_object_new (AWRA_TYPE_COLOR_PALETTE, NULL);
}

void
awra_color_palette_add_color (AwraColorPalette *self,
                              const GdkRGBA    *color,
                              const char       *name)
{
  GtkWidget *swatch;
  g_autofree char *hex = NULL;
  GdkRGBA opaque;

  g_return_if_fail (AWRA_IS_COLOR_PALETTE (self));
  g_return_if_fail (color != NULL);
  opaque = *color;
  opaque.alpha = 1.0;
  swatch = swatch_new (&opaque);
  if (name == NULL)
    name = hex = g_strdup_printf ("#%02x%02x%02x", (int) (opaque.red * 255 + 0.5),
                                  (int) (opaque.green * 255 + 0.5), (int) (opaque.blue * 255 + 0.5));
  gtk_widget_set_tooltip_text (swatch, name);
  gtk_accessible_update_property (GTK_ACCESSIBLE (swatch), GTK_ACCESSIBLE_PROPERTY_LABEL, name, -1);
  g_signal_connect (swatch, "clicked", G_CALLBACK (swatch_clicked_cb), self);
  /* After the palette's colours so far. */
  gtk_flow_box_insert (GTK_FLOW_BOX (self->flow), swatch, (int) self->swatches->len);
  gtk_widget_set_focusable (gtk_widget_get_parent (swatch), FALSE);
  g_ptr_array_add (self->swatches, swatch);
  show_selection (self);
}

void
awra_color_palette_clear (AwraColorPalette *self)
{
  g_return_if_fail (AWRA_IS_COLOR_PALETTE (self));
  for (guint i = 0; i < self->swatches->len; i++)
    gtk_flow_box_remove (GTK_FLOW_BOX (self->flow), g_ptr_array_index (self->swatches, i));
  g_ptr_array_set_size (self->swatches, 0);
  show_selection (self);
}

const GdkRGBA *
awra_color_palette_get_selected (AwraColorPalette *self)
{
  g_return_val_if_fail (AWRA_IS_COLOR_PALETTE (self), NULL);
  return self->has_selected ? &self->selected : NULL;
}

void
awra_color_palette_set_selected (AwraColorPalette *self,
                                 const GdkRGBA    *color)
{
  GdkRGBA opaque;

  g_return_if_fail (AWRA_IS_COLOR_PALETTE (self));
  if (color != NULL) {
    opaque = *color;
    opaque.alpha = 1.0;
  }
  if (color == NULL ? !self->has_selected
                    : self->has_selected && gdk_rgba_equal (&opaque, &self->selected))
    return;
  self->has_selected = color != NULL;
  if (color != NULL)
    self->selected = opaque;
  show_selection (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
}

const char *
awra_color_palette_get_automatic_label (AwraColorPalette *self)
{
  g_return_val_if_fail (AWRA_IS_COLOR_PALETTE (self), NULL);
  return self->automatic_label;
}

void
awra_color_palette_set_automatic_label (AwraColorPalette *self,
                                        const char       *label)
{
  g_return_if_fail (AWRA_IS_COLOR_PALETTE (self));
  if (label != NULL && *label == '\0')
    label = NULL;
  if (!g_set_str (&self->automatic_label, label))
    return;
  gtk_button_set_label (GTK_BUTTON (self->automatic), label != NULL ? label : "");
  gtk_widget_set_visible (self->automatic, label != NULL);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTOMATIC_LABEL]);
}

gboolean
awra_color_palette_get_allows_custom (AwraColorPalette *self)
{
  g_return_val_if_fail (AWRA_IS_COLOR_PALETTE (self), FALSE);
  return self->allows_custom;
}

void
awra_color_palette_set_allows_custom (AwraColorPalette *self,
                                      gboolean          allows_custom)
{
  g_return_if_fail (AWRA_IS_COLOR_PALETTE (self));
  allows_custom = !!allows_custom;
  if (self->allows_custom == allows_custom)
    return;
  self->allows_custom = allows_custom;
  gtk_widget_set_visible (gtk_widget_get_parent (self->more), allows_custom);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALLOWS_CUSTOM]);
}
