/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-rail-item.h>

#include <awra/awra-avatar.h>
#include <awra/awra-layout.h>
#include <awra/awra-spring-animation.h>
#include <awra/awra-token-set.h>
#include <math.h>

#include "awra-i18n-private.h"
#include "navigation/awra-rail-private.h"
#include "widgets/awra-widget-style-private.h"

#define TOOLTIP_DELAY 120
#define PRESSED_SCALE 0.94
#define LIFTED_SCALE 1.06

struct _AwraRailItem {
  GtkWidget parent_instance;
  char *label;
  GtkWidget *child;
  char *icon_name;
  GdkPaintable *icon;
  int icon_px;
  gboolean selected;
  gboolean unread;
  gboolean muted;
  guint mention_count;
  guint shown_count;
  PangoLayout *count_layout;
  gboolean hovered;
  gboolean pressed;
  gboolean group_header;
  gboolean lifted;
  gboolean drop_target;
  gboolean has_tint;
  GdkRGBA tint;
  /* Animated presentation: pill height in pixels, shape from round (0) to
   * rounded square (1), tile scale, badge scale. */
  double pill;
  double shape;
  double scale;
  double badge;
  AwraAnimation *pill_animation;
  AwraAnimation *shape_animation;
  AwraAnimation *scale_animation;
  AwraAnimation *badge_animation;
  GtkWidget *tooltip;
  guint tooltip_timeout;
  AwraStyleWatch watch;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_CHILD,
  PROP_ICON_NAME,
  PROP_SELECTED,
  PROP_UNREAD,
  PROP_MENTION_COUNT,
  PROP_MUTED,
  N_PROPS,
};

enum {
  SIGNAL_ACTIVATE,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraRailItem, awra_rail_item, GTK_TYPE_WIDGET)

/* Shared metrics */

double
awra_rail_tile_size (GtkWidget *widget)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);

  return awra_token_set_get_navigation_height (tokens) +
         awra_token_set_get_spacing (tokens, AWRA_SPACING_SM);
}

double
awra_rail_tile_inset (GtkWidget *widget)
{
  return awra_token_set_get_spacing (awra_widget_get_token_set (widget), AWRA_SPACING_MD);
}

double
awra_rail_entry_gap (GtkWidget *widget)
{
  return awra_token_set_get_spacing (awra_widget_get_token_set (widget), AWRA_SPACING_SM);
}

/* Animation */

#define DEFINE_SETTER(name, field) \
  static void \
  set_##name (double   value, \
              gpointer user_data) \
  { \
    AwraRailItem *self = user_data; \
    self->field = value; \
    gtk_widget_queue_draw (GTK_WIDGET (self)); \
  }

DEFINE_SETTER (pill, pill)
DEFINE_SETTER (shape, shape)
DEFINE_SETTER (scale, scale)

static void
set_badge (double   value,
           gpointer user_data)
{
  AwraRailItem *self = user_data;

  self->badge = MAX (value, 0.0);
  if (self->badge <= 0.0 && self->mention_count == 0)
    self->shown_count = 0;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
spring_to (AwraRailItem            *self,
           AwraAnimation          **animation,
           double                   current,
           double                   target,
           double                   damping,
           double                   stiffness,
           double                   epsilon,
           AwraAnimationTargetFunc  func)
{
  if (*animation == NULL) {
    if (current == target)
      return;
    *animation = awra_spring_animation_new (GTK_WIDGET (self), current, current, damping,
                                            stiffness,
                                            awra_callback_animation_target_new (func, self, NULL));
    awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (*animation), epsilon);
  }
  if (awra_spring_animation_get_value_to (AWRA_SPRING_ANIMATION (*animation)) == target &&
      awra_animation_get_state (*animation) == AWRA_ANIMATION_STATE_PLAYING)
    return;
  awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (*animation), target);
}

static double
pill_target (AwraRailItem *self)
{
  double tile = awra_rail_tile_size (GTK_WIDGET (self));

  if (self->lifted)
    return 0.0;
  if (self->selected)
    return round (tile * 0.84);
  if (self->hovered || self->drop_target)
    return round (tile * 0.42);
  if (self->unread && !self->muted)
    return round (tile / 5.0);
  return 0.0;
}

static void
update_presentation (AwraRailItem *self)
{
  double scale = self->lifted ? LIFTED_SCALE : self->pressed ? PRESSED_SCALE : 1.0;
  gboolean square = self->selected || self->hovered || self->drop_target || self->lifted;

  spring_to (self, &self->pill_animation, self->pill, pill_target (self),
             0.82, 420.0, 0.1, set_pill);
  spring_to (self, &self->shape_animation, self->shape, square ? 1.0 : 0.0,
             0.9, 380.0, 0.002, set_shape);
  spring_to (self, &self->scale_animation, self->scale, scale,
             self->pressed ? 1.0 : 0.55, 600.0, 0.001, set_scale);
}

/* Accessibility */

static void
update_accessible (AwraRailItem *self)
{
  GString *description = g_string_new (NULL);

  if (self->mention_count > 0)
    g_string_append_printf (description,
                            g_dngettext (GETTEXT_PACKAGE, "%u mention", "%u mentions",
                                         self->mention_count),
                            self->mention_count);
  else if (self->unread && !self->muted)
    g_string_append (description, _("Unread"));
  if (self->muted) {
    if (description->len > 0)
      g_string_append (description, ", ");
    g_string_append (description, _("Muted"));
  }
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  self->label != NULL ? self->label : "", -1);
  if (description->len > 0)
    gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description->str, -1);
  else
    gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION);
  if (!self->group_header)
    gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                 GTK_ACCESSIBLE_STATE_SELECTED, self->selected, -1);
  g_string_free (description, TRUE);
}

/* Tooltip */

static void
hide_tooltip (AwraRailItem *self)
{
  g_clear_handle_id (&self->tooltip_timeout, g_source_remove);
  if (self->tooltip != NULL)
    gtk_popover_popdown (GTK_POPOVER (self->tooltip));
}

static gboolean
show_tooltip_cb (gpointer user_data)
{
  AwraRailItem *self = user_data;
  GtkWidget *label;

  self->tooltip_timeout = 0;
  if (self->label == NULL || *self->label == '\0' || self->lifted ||
      !gtk_widget_get_mapped (GTK_WIDGET (self)))
    return G_SOURCE_REMOVE;
  if (self->tooltip == NULL) {
    self->tooltip = gtk_popover_new ();
    label = gtk_label_new (self->label);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 32);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_popover_set_child (GTK_POPOVER (self->tooltip), label);
    gtk_popover_set_has_arrow (GTK_POPOVER (self->tooltip), FALSE);
    gtk_popover_set_autohide (GTK_POPOVER (self->tooltip), FALSE);
    gtk_widget_set_can_target (self->tooltip, FALSE);
    gtk_widget_set_can_focus (self->tooltip, FALSE);
    gtk_widget_add_css_class (self->tooltip, "awra-rail-tooltip");
    gtk_accessible_update_state (GTK_ACCESSIBLE (self->tooltip),
                                 GTK_ACCESSIBLE_STATE_HIDDEN, TRUE, -1);
    gtk_widget_set_parent (self->tooltip, GTK_WIDGET (self));
  } else {
    gtk_label_set_text (GTK_LABEL (gtk_popover_get_child (GTK_POPOVER (self->tooltip))),
                        self->label);
  }
  gtk_popover_set_position (GTK_POPOVER (self->tooltip),
                            gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL
                              ? GTK_POS_LEFT : GTK_POS_RIGHT);
  gtk_popover_popup (GTK_POPOVER (self->tooltip));
  return G_SOURCE_REMOVE;
}

static void
schedule_tooltip (AwraRailItem *self)
{
  if (self->tooltip_timeout == 0)
    self->tooltip_timeout = g_timeout_add (TOOLTIP_DELAY, show_tooltip_cb, self);
}

/* Input */

static gboolean
rail_is_dragging (AwraRailItem *self)
{
  GtkWidget *rail = gtk_widget_get_ancestor (GTK_WIDGET (self), AWRA_TYPE_RAIL);

  return rail != NULL && awra_rail_is_dragging (AWRA_RAIL (rail));
}

static void
motion_enter_cb (GtkEventControllerMotion *controller,
                 double                    x,
                 double                    y,
                 AwraRailItem             *self)
{
  (void) controller;
  (void) x;
  (void) y;
  /* While another entry is being dragged, only the drop target reacts. */
  if (rail_is_dragging (self))
    return;
  self->hovered = TRUE;
  update_presentation (self);
  schedule_tooltip (self);
}

static void
motion_leave_cb (GtkEventControllerMotion *controller,
                 AwraRailItem             *self)
{
  (void) controller;
  self->hovered = FALSE;
  self->pressed = FALSE;
  update_presentation (self);
  hide_tooltip (self);
}

static void
click_pressed_cb (GtkGestureClick *gesture,
                  int              n_press,
                  double           x,
                  double           y,
                  AwraRailItem    *self)
{
  (void) gesture;
  (void) n_press;
  (void) x;
  (void) y;
  self->pressed = TRUE;
  update_presentation (self);
}

static void
click_released_cb (GtkGestureClick *gesture,
                   int              n_press,
                   double           x,
                   double           y,
                   AwraRailItem    *self)
{
  gboolean inside = gtk_widget_contains (GTK_WIDGET (self), x, y);

  (void) n_press;
  self->pressed = FALSE;
  update_presentation (self);
  if (inside && !self->lifted) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    g_signal_emit (self, signals[SIGNAL_ACTIVATE], 0);
  }
}

static void
click_stopped_cb (GtkGestureClick *gesture,
                  AwraRailItem    *self)
{
  (void) gesture;
  if (!self->pressed)
    return;
  self->pressed = FALSE;
  update_presentation (self);
}

static void
focus_enter_cb (GtkEventControllerFocus *controller,
                AwraRailItem            *self)
{
  (void) controller;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  if (gtk_widget_has_visible_focus (GTK_WIDGET (self)))
    schedule_tooltip (self);
}

static void
focus_leave_cb (GtkEventControllerFocus *controller,
                AwraRailItem            *self)
{
  (void) controller;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  if (!self->hovered)
    hide_tooltip (self);
}

static void
awra_rail_item_real_activate (AwraRailItem *self)
{
  GtkWidget *rail = gtk_widget_get_ancestor (GTK_WIDGET (self), AWRA_TYPE_RAIL);

  if (rail != NULL && !self->group_header)
    awra_rail_item_activated (AWRA_RAIL (rail), self);
}

/* Drawing */

static double
corner_radius (AwraRailItem *self,
               double        tile)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (GTK_WIDGET (self));
  double square = MIN (awra_token_set_get_radius_for_size (tokens, AWRA_RADIUS_LARGE), tile / 2);

  return tile / 2 + (square - tile / 2) * CLAMP (self->shape, 0.0, 1.2);
}

static GdkRGBA
mix (const GdkRGBA *from,
     const GdkRGBA *to,
     double         amount)
{
  amount = CLAMP (amount, 0.0, 1.0);
  return (GdkRGBA) {
    from->red + (to->red - from->red) * amount,
    from->green + (to->green - from->green) * amount,
    from->blue + (to->blue - from->blue) * amount,
    from->alpha + (to->alpha - from->alpha) * amount,
  };
}

static void
tile_colors (AwraRailItem *self,
             GdkRGBA      *background,
             GdkRGBA      *foreground)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (GTK_WIDGET (self));
  GdkRGBA rest = *awra_token_set_get_control_cap (tokens);
  GdkRGBA rest_fg = *awra_token_set_get_foreground (tokens);
  const GdkRGBA *active = self->selected ? awra_token_set_get_accent (tokens)
                                         : awra_token_set_get_accent_soft (tokens);
  const GdkRGBA *active_fg = self->selected ? awra_token_set_get_on_accent (tokens)
                                            : awra_token_set_get_accent_content (tokens);

  if (self->has_tint) {
    rest = awra_rgba_with_alpha (&self->tint, awra_token_set_get_dark (tokens) ? 0.36 : 0.28);
    *background = rest;
    *foreground = rest_fg;
    return;
  }
  if (self->group_header) {
    rest = *awra_token_set_get_control_well (tokens);
    *background = rest;
    *foreground = rest_fg;
    return;
  }
  *background = mix (&rest, active, self->shape);
  *foreground = mix (&rest_fg, active_fg, self->shape);
}

static PangoLayout *
ensure_count_layout (AwraRailItem *self,
                     double        height)
{
  g_autofree char *text = NULL;
  PangoFontDescription *font;

  if (self->count_layout != NULL)
    return self->count_layout;
  text = self->shown_count > 99 ? g_strdup ("99+") : g_strdup_printf ("%u", self->shown_count);
  self->count_layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), text);
  font = pango_font_description_copy (
    pango_context_get_font_description (pango_layout_get_context (self->count_layout)));
  pango_font_description_set_absolute_size (font, height * 0.66 * PANGO_SCALE);
  pango_font_description_set_weight (font, PANGO_WEIGHT_BOLD);
  pango_layout_set_font_description (self->count_layout, font);
  pango_font_description_free (font);
  return self->count_layout;
}

static void
badge_rect (AwraRailItem    *self,
            double           tile,
            graphene_rect_t *rect,
            PangoRectangle  *ink)
{
  double height = round (tile / 3.0);
  double width;
  double end;
  gboolean rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  pango_layout_get_pixel_extents (ensure_count_layout (self, height), ink, NULL);
  width = MAX (height, ink->width + height * 0.7);
  /* Hang off the corner a little, like a sticker. */
  end = tile + 2;
  graphene_rect_init (rect, rtl ? -2 : end - width, tile + 2 - height, width, height);
}

static void
append_rounded (GtkSnapshot           *snapshot,
                const graphene_rect_t *rect,
                double                 radius,
                const GdkRGBA         *color)
{
  GskRoundedRect shape;

  gsk_rounded_rect_init_from_rect (&shape, rect, radius);
  gtk_snapshot_push_rounded_clip (snapshot, &shape);
  gtk_snapshot_append_color (snapshot, color, rect);
  gtk_snapshot_pop (snapshot);
}

static void
append_ring (GtkSnapshot   *snapshot,
             double         tile,
             double         radius,
             double         offset,
             double         width,
             const GdkRGBA *color)
{
  GskRoundedRect ring;
  float widths[4] = { width, width, width, width };
  GdkRGBA colors[4] = { *color, *color, *color, *color };

  gsk_rounded_rect_init_from_rect (&ring,
                                   &GRAPHENE_RECT_INIT (-offset - width, -offset - width,
                                                        tile + 2 * (offset + width),
                                                        tile + 2 * (offset + width)),
                                   radius + offset + width);
  gtk_snapshot_append_border (snapshot, &ring, widths, colors);
}

static void
draw_pill (AwraRailItem *self,
           GtkSnapshot  *snapshot,
           float         width,
           float         height)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (GTK_WIDGET (self));
  double unread = round (awra_rail_tile_size (GTK_WIDGET (self)) / 5.0);
  float pill_width = 4.0f * (float) MIN (1.0, self->pill / unread);
  gboolean rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  float x = rtl ? width - pill_width : 0;

  if (self->pill < 0.5)
    return;
  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (x, 0, pill_width, height));
  append_rounded (snapshot,
                  &GRAPHENE_RECT_INIT (x - (rtl ? 0 : pill_width), (height - self->pill) / 2,
                                       pill_width * 2, self->pill),
                  pill_width, awra_token_set_get_foreground (tokens));
  gtk_snapshot_pop (snapshot);
}

static void
draw_tile (AwraRailItem *self,
           GtkSnapshot  *snapshot,
           double        tile)
{
  GskRoundedRect shape;
  GdkRGBA background;
  GdkRGBA foreground;

  tile_colors (self, &background, &foreground);
  gsk_rounded_rect_init_from_rect (&shape, &GRAPHENE_RECT_INIT (0, 0, tile, tile),
                                   corner_radius (self, tile));
  gtk_snapshot_push_rounded_clip (snapshot, &shape);
  gtk_snapshot_append_color (snapshot, &background, &shape.bounds);
  if (self->child != NULL) {
    gtk_widget_snapshot_child (GTK_WIDGET (self), self->child, snapshot);
  } else if (self->icon_name != NULL) {
    int px = (int) round (tile * 0.42);

    if (self->icon == NULL || self->icon_px != px) {
      g_clear_object (&self->icon);
      self->icon_px = px;
      self->icon = GDK_PAINTABLE (gtk_icon_theme_lookup_icon (
        gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (self))),
        self->icon_name, NULL, px, gtk_widget_get_scale_factor (GTK_WIDGET (self)),
        gtk_widget_get_direction (GTK_WIDGET (self)), 0));
    }
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (round ((tile - px) / 2),
                                                            round ((tile - px) / 2)));
    gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (self->icon), snapshot,
                                              px, px, &foreground, 1);
    gtk_snapshot_restore (snapshot);
  }
  gtk_snapshot_pop (snapshot);
}

static void
awra_rail_item_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (widget);
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);
  double tile = awra_rail_tile_size (widget);
  double radius = corner_radius (self, tile);
  gboolean badge = self->shown_count > 0 && self->badge > 0.0;
  graphene_rect_t badge_bounds;
  PangoRectangle ink;
  float origin_x = round ((width - tile) / 2);
  float origin_y = round ((height - tile) / 2);

  draw_pill (self, snapshot, width, height);
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (origin_x + tile / 2, origin_y + tile / 2));
  gtk_snapshot_scale (snapshot, self->scale, self->scale);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-tile / 2, -tile / 2));
  if (self->lifted) {
    GskRoundedRect shadow;

    gsk_rounded_rect_init_from_rect (&shadow, &GRAPHENE_RECT_INIT (0, 0, tile, tile), radius);
    gtk_snapshot_append_outset_shadow (snapshot, &shadow, &(GdkRGBA) { 0, 0, 0, 0.32 },
                                       0, 6, 0, 16);
  }
  if (badge) {
    badge_rect (self, tile, &badge_bounds, &ink);
    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_ALPHA);
    {
      graphene_point_t center;
      double ring = MAX (2.0, tile / 16.0);
      graphene_rect_t cut;

      graphene_rect_get_center (&badge_bounds, &center);
      graphene_rect_init (&cut,
                          center.x - (badge_bounds.size.width / 2 + ring) * self->badge,
                          center.y - (badge_bounds.size.height / 2 + ring) * self->badge,
                          (badge_bounds.size.width + 2 * ring) * self->badge,
                          (badge_bounds.size.height + 2 * ring) * self->badge);
      append_rounded (snapshot, &cut, cut.size.height / 2, &(GdkRGBA) { 0, 0, 0, 1 });
    }
    gtk_snapshot_pop (snapshot);
  }
  if (self->muted && !self->hovered && !self->selected)
    gtk_snapshot_push_opacity (snapshot, 0.48);
  draw_tile (self, snapshot, tile);
  if (self->muted && !self->hovered && !self->selected)
    gtk_snapshot_pop (snapshot);
  if (badge) {
    graphene_point_t center;
    GdkRGBA white = { 1, 1, 1, 1 };

    gtk_snapshot_pop (snapshot);
    graphene_rect_get_center (&badge_bounds, &center);
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot, &center);
    gtk_snapshot_scale (snapshot, self->badge, self->badge);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-center.x, -center.y));
    append_rounded (snapshot, &badge_bounds, badge_bounds.size.height / 2,
                    awra_token_set_get_danger (tokens));
    gtk_snapshot_translate (snapshot,
                            &GRAPHENE_POINT_INIT (
                              round (badge_bounds.origin.x +
                                     (badge_bounds.size.width - ink.width) / 2 - ink.x),
                              round (badge_bounds.origin.y +
                                     (badge_bounds.size.height - ink.height) / 2 - ink.y)));
    gtk_snapshot_append_layout (snapshot, self->count_layout, &white);
    gtk_snapshot_restore (snapshot);
  }
  if (self->drop_target)
    append_ring (snapshot, tile, radius, 2.0, 2.0, awra_token_set_get_accent (tokens));
  else if (gtk_widget_has_visible_focus (widget))
    append_ring (snapshot, tile, radius, 2.0, 2.0, awra_token_set_get_focus_ring (tokens));
  gtk_snapshot_restore (snapshot);
}

/* Layout */

static void
awra_rail_item_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  double tile = awra_rail_tile_size (widget);

  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = (int) (tile + 2 * awra_rail_tile_inset (widget));
  else
    *minimum = *natural = (int) tile;
}

static void
awra_rail_item_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (widget);
  double tile = awra_rail_tile_size (widget);

  (void) baseline;
  if (self->child != NULL) {
    /* The child is drawn by the tile at its origin. */
    gtk_widget_allocate (self->child, (int) tile, (int) tile, -1, NULL);
  }
  if (self->tooltip != NULL) {
    GdkRectangle pointing = { (int) ((width - tile) / 2), 0, (int) tile, height };

    gtk_popover_set_pointing_to (GTK_POPOVER (self->tooltip), &pointing);
    gtk_popover_present (GTK_POPOVER (self->tooltip));
  }
  awra_widget_present_popovers (widget);
}

static void
prepare_avatar (AwraRailItem *self)
{
  if (AWRA_IS_AVATAR (self->child)) {
    awra_avatar_set_size (AWRA_AVATAR (self->child), (int) awra_rail_tile_size (GTK_WIDGET (self)));
    awra_avatar_set_shape (AWRA_AVATAR (self->child), AWRA_AVATAR_SHAPE_ROUNDED);
  }
}

static void
style_changed (gpointer user_data)
{
  AwraRailItem *self = user_data;

  prepare_avatar (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
awra_rail_item_map (GtkWidget *widget)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (widget);

  GTK_WIDGET_CLASS (awra_rail_item_parent_class)->map (widget);
  awra_style_watch_start (&self->watch, widget, style_changed, self);
}

static void
awra_rail_item_unmap (GtkWidget *widget)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (widget);

  hide_tooltip (self);
  awra_style_watch_stop (&self->watch);
  self->hovered = FALSE;
  self->pressed = FALSE;
  GTK_WIDGET_CLASS (awra_rail_item_parent_class)->unmap (widget);
}

static void
clear_caches (AwraRailItem *self)
{
  g_clear_object (&self->icon);
  g_clear_object (&self->count_layout);
}

static void
awra_rail_item_system_setting_changed (GtkWidget        *widget,
                                       GtkSystemSetting  setting)
{
  clear_caches (AWRA_RAIL_ITEM (widget));
  GTK_WIDGET_CLASS (awra_rail_item_parent_class)->system_setting_changed (widget, setting);
}

static void
awra_rail_item_direction_changed (GtkWidget        *widget,
                                  GtkTextDirection  previous)
{
  clear_caches (AWRA_RAIL_ITEM (widget));
  GTK_WIDGET_CLASS (awra_rail_item_parent_class)->direction_changed (widget, previous);
}

static void
stop_animation (AwraAnimation **animation)
{
  if (*animation != NULL)
    awra_animation_skip (*animation);
  g_clear_object (animation);
}

static void
awra_rail_item_dispose (GObject *object)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (object);

  hide_tooltip (self);
  awra_style_watch_stop (&self->watch);
  stop_animation (&self->pill_animation);
  stop_animation (&self->shape_animation);
  stop_animation (&self->scale_animation);
  stop_animation (&self->badge_animation);
  g_clear_pointer (&self->tooltip, gtk_widget_unparent);
  g_clear_pointer (&self->child, gtk_widget_unparent);
  clear_caches (self);
  G_OBJECT_CLASS (awra_rail_item_parent_class)->dispose (object);
}

static void
awra_rail_item_finalize (GObject *object)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (object);

  g_free (self->label);
  g_free (self->icon_name);
  G_OBJECT_CLASS (awra_rail_item_parent_class)->finalize (object);
}

static void
awra_rail_item_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (object);

  switch (prop_id) {
    case PROP_LABEL: g_value_set_string (value, self->label); break;
    case PROP_CHILD: g_value_set_object (value, self->child); break;
    case PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
    case PROP_SELECTED: g_value_set_boolean (value, self->selected); break;
    case PROP_UNREAD: g_value_set_boolean (value, self->unread); break;
    case PROP_MENTION_COUNT: g_value_set_uint (value, self->mention_count); break;
    case PROP_MUTED: g_value_set_boolean (value, self->muted); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rail_item_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  AwraRailItem *self = AWRA_RAIL_ITEM (object);

  switch (prop_id) {
    case PROP_LABEL: awra_rail_item_set_label (self, g_value_get_string (value)); break;
    case PROP_CHILD: awra_rail_item_set_child (self, g_value_get_object (value)); break;
    case PROP_ICON_NAME: awra_rail_item_set_icon_name (self, g_value_get_string (value)); break;
    case PROP_SELECTED: awra_rail_item_set_selected (self, g_value_get_boolean (value)); break;
    case PROP_UNREAD: awra_rail_item_set_unread (self, g_value_get_boolean (value)); break;
    case PROP_MENTION_COUNT:
      awra_rail_item_set_mention_count (self, g_value_get_uint (value));
      break;
    case PROP_MUTED: awra_rail_item_set_muted (self, g_value_get_boolean (value)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rail_item_class_init (AwraRailItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->dispose = awra_rail_item_dispose;
  object_class->finalize = awra_rail_item_finalize;
  object_class->get_property = awra_rail_item_get_property;
  object_class->set_property = awra_rail_item_set_property;
  widget_class->snapshot = awra_rail_item_snapshot;
  widget_class->measure = awra_rail_item_measure;
  widget_class->size_allocate = awra_rail_item_size_allocate;
  widget_class->map = awra_rail_item_map;
  widget_class->unmap = awra_rail_item_unmap;
  widget_class->system_setting_changed = awra_rail_item_system_setting_changed;
  widget_class->direction_changed = awra_rail_item_direction_changed;

  /**
   * AwraRailItem:label:
   *
   * The name of the destination, shown in the tooltip.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL] = g_param_spec_string ("label", NULL, NULL, NULL, flags);
  /**
   * AwraRailItem:child:
   *
   * The content of the tile.
   *
   * Since: 1.1
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL, GTK_TYPE_WIDGET, flags);
  /**
   * AwraRailItem:icon-name:
   *
   * A symbolic icon shown on a plain tile when there is no child.
   *
   * Since: 1.1
   */
  properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL, NULL, flags);
  /**
   * AwraRailItem:selected:
   *
   * Whether the item is the current destination.
   *
   * Since: 1.1
   */
  properties[PROP_SELECTED] = g_param_spec_boolean ("selected", NULL, NULL, FALSE, flags);
  /**
   * AwraRailItem:unread:
   *
   * Whether the destination has unread activity.
   *
   * Since: 1.1
   */
  properties[PROP_UNREAD] = g_param_spec_boolean ("unread", NULL, NULL, FALSE, flags);
  /**
   * AwraRailItem:mention-count:
   *
   * The number of mentions waiting.
   *
   * Since: 1.1
   */
  properties[PROP_MENTION_COUNT] =
    g_param_spec_uint ("mention-count", NULL, NULL, 0, G_MAXUINT, 0, flags);
  /**
   * AwraRailItem:muted:
   *
   * Whether the destination is muted.
   *
   * Since: 1.1
   */
  properties[PROP_MUTED] = g_param_spec_boolean ("muted", NULL, NULL, FALSE, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraRailItem::activate:
   * @self: the item
   *
   * Emitted when the item is clicked, or when Enter or Space is pressed on
   * it. In an #AwraRail, the default handler selects the item.
   *
   * Since: 1.1
   */
  signals[SIGNAL_ACTIVATE] =
    g_signal_new_class_handler ("activate", G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (awra_rail_item_real_activate),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);
  gtk_widget_class_set_activate_signal (widget_class, signals[SIGNAL_ACTIVATE]);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0, "activate", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0, "activate", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0, "activate", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Space, 0, "activate", NULL);
  gtk_widget_class_set_css_name (widget_class, "awrarailitem");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TAB);
}

static void
awra_rail_item_init (AwraRailItem *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkEventController *motion = gtk_event_controller_motion_new ();
  GtkGesture *click = gtk_gesture_click_new ();
  GtkEventController *focus = gtk_event_controller_focus_new ();

  self->scale = 1.0;
  gtk_widget_set_focusable (widget, TRUE);
  gtk_widget_set_focus_on_click (widget, FALSE);
  gtk_widget_set_overflow (widget, GTK_OVERFLOW_VISIBLE);
  g_signal_connect (motion, "enter", G_CALLBACK (motion_enter_cb), self);
  g_signal_connect (motion, "leave", G_CALLBACK (motion_leave_cb), self);
  gtk_widget_add_controller (widget, motion);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_PRIMARY);
  g_signal_connect (click, "pressed", G_CALLBACK (click_pressed_cb), self);
  g_signal_connect (click, "released", G_CALLBACK (click_released_cb), self);
  g_signal_connect (click, "stopped", G_CALLBACK (click_stopped_cb), self);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (click));
  g_signal_connect (focus, "enter", G_CALLBACK (focus_enter_cb), self);
  g_signal_connect (focus, "leave", G_CALLBACK (focus_leave_cb), self);
  gtk_widget_add_controller (widget, focus);
  update_accessible (self);
}

GtkWidget *
awra_rail_item_new (const char *label)
{
  return g_object_new (AWRA_TYPE_RAIL_ITEM, "label", label, NULL);
}

const char *
awra_rail_item_get_label (AwraRailItem *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_ITEM (self), NULL);
  return self->label;
}

void
awra_rail_item_set_label (AwraRailItem *self,
                          const char   *label)
{
  g_return_if_fail (AWRA_IS_RAIL_ITEM (self));
  if (!g_set_str (&self->label, label))
    return;
  if (self->tooltip != NULL)
    gtk_label_set_text (GTK_LABEL (gtk_popover_get_child (GTK_POPOVER (self->tooltip))),
                        label != NULL ? label : "");
  update_accessible (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

GtkWidget *
awra_rail_item_get_child (AwraRailItem *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_ITEM (self), NULL);
  return self->child;
}

void
awra_rail_item_set_child (AwraRailItem *self,
                          GtkWidget    *child)
{
  g_return_if_fail (AWRA_IS_RAIL_ITEM (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));
  if (self->child == child)
    return;
  g_clear_pointer (&self->child, gtk_widget_unparent);
  self->child = child;
  if (child != NULL) {
    gtk_widget_set_parent (child, GTK_WIDGET (self));
    gtk_widget_set_can_target (child, FALSE);
    prepare_avatar (self);
  }
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

const char *
awra_rail_item_get_icon_name (AwraRailItem *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_ITEM (self), NULL);
  return self->icon_name;
}

void
awra_rail_item_set_icon_name (AwraRailItem *self,
                              const char   *icon_name)
{
  g_return_if_fail (AWRA_IS_RAIL_ITEM (self));
  if (!g_set_str (&self->icon_name, icon_name))
    return;
  g_clear_object (&self->icon);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

gboolean
awra_rail_item_get_selected (AwraRailItem *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_ITEM (self), FALSE);
  return self->selected;
}

void
awra_rail_item_set_selected (AwraRailItem *self,
                             gboolean      selected)
{
  GtkWidget *rail;

  g_return_if_fail (AWRA_IS_RAIL_ITEM (self));
  selected = !!selected;
  if (self->selected == selected)
    return;
  self->selected = selected;
  if (selected)
    gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED);
  update_presentation (self);
  update_accessible (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
  rail = gtk_widget_get_ancestor (GTK_WIDGET (self), AWRA_TYPE_RAIL);
  if (rail != NULL && !self->group_header)
    awra_rail_item_selection_changed (AWRA_RAIL (rail), self);
}

gboolean
awra_rail_item_get_unread (AwraRailItem *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_ITEM (self), FALSE);
  return self->unread;
}

void
awra_rail_item_set_unread (AwraRailItem *self,
                           gboolean      unread)
{
  g_return_if_fail (AWRA_IS_RAIL_ITEM (self));
  unread = !!unread;
  if (self->unread == unread)
    return;
  self->unread = unread;
  update_presentation (self);
  update_accessible (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNREAD]);
}

guint
awra_rail_item_get_mention_count (AwraRailItem *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_ITEM (self), 0);
  return self->mention_count;
}

void
awra_rail_item_set_mention_count (AwraRailItem *self,
                                  guint         count)
{
  AwraSpringAnimation *spring;

  g_return_if_fail (AWRA_IS_RAIL_ITEM (self));
  if (self->mention_count == count)
    return;
  self->mention_count = count;
  if (self->badge_animation == NULL)
    self->badge_animation =
      awra_spring_animation_new (GTK_WIDGET (self), self->badge, self->badge, 0.5, 520.0,
                                 awra_callback_animation_target_new (set_badge, self, NULL));
  spring = AWRA_SPRING_ANIMATION (self->badge_animation);
  awra_spring_animation_set_epsilon (spring, 0.002);
  if (count == 0) {
    /* Shrink away, still showing the last count. */
    awra_spring_animation_set_clamp (spring, TRUE);
    awra_spring_animation_retarget (spring, 0.0);
  } else {
    gboolean shown = self->shown_count > 0 && self->badge > 0.5;

    self->shown_count = count;
    g_clear_object (&self->count_layout);
    awra_spring_animation_set_clamp (spring, FALSE);
    if (shown) {
      /* A new count nods; the badge does not pop in again. */
      awra_spring_animation_set_value_from (spring, 1.22);
      awra_spring_animation_set_value_to (spring, 1.0);
      awra_spring_animation_set_initial_velocity (spring, 0.0);
      awra_animation_play (self->badge_animation);
    } else {
      awra_spring_animation_retarget (spring, 1.0);
    }
  }
  update_accessible (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENTION_COUNT]);
}

gboolean
awra_rail_item_get_muted (AwraRailItem *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL_ITEM (self), FALSE);
  return self->muted;
}

void
awra_rail_item_set_muted (AwraRailItem *self,
                          gboolean      muted)
{
  g_return_if_fail (AWRA_IS_RAIL_ITEM (self));
  muted = !!muted;
  if (self->muted == muted)
    return;
  self->muted = muted;
  update_presentation (self);
  update_accessible (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MUTED]);
}

/* Private */

void
awra_rail_item_set_group_header (AwraRailItem *self,
                                 gboolean      expanded)
{
  self->group_header = TRUE;
  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_EXPANDED, expanded, -1);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
awra_rail_item_set_tint (AwraRailItem  *self,
                         const GdkRGBA *tint)
{
  self->has_tint = tint != NULL;
  if (tint != NULL)
    self->tint = *tint;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
awra_rail_item_set_lifted (AwraRailItem *self,
                           gboolean      lifted)
{
  if (self->lifted == lifted)
    return;
  self->lifted = lifted;
  if (lifted) {
    hide_tooltip (self);
    self->pressed = FALSE;
  }
  update_presentation (self);
}

void
awra_rail_item_set_drop_target (AwraRailItem *self,
                                gboolean      target)
{
  if (self->drop_target == target)
    return;
  self->drop_target = target;
  update_presentation (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}
