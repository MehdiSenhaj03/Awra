/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-chip.h>

#include <awra/awra-spring-animation.h>
#include <awra/awra-timed-animation.h>
#include <awra/awra-token-set.h>
#include <math.h>
#include <string.h>

#include "widgets/awra-widget-style-private.h"

#define PRESSED_SCALE 0.9

struct _AwraChip {
  GtkWidget parent_instance;
  char *label;
  char *icon_name;
  GdkPaintable *paintable;
  char *emoji;
  int count;
  int previous_count;
  gboolean active;
  gboolean toggleable;
  AwraChipAppearance appearance;
  gboolean hovered;
  gboolean pressed;
  PangoLayout *label_layout;
  PangoLayout *emoji_layout;
  PangoLayout *count_layout;
  PangoLayout *previous_layout;
  GdkPaintable *icon;
  int icon_px;
  double scale;
  double active_mix;
  double hover_mix;
  double roll;
  AwraAnimation *scale_animation;
  AwraAnimation *active_animation;
  AwraAnimation *hover_animation;
  AwraAnimation *roll_animation;
  AwraStyleWatch watch;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ICON_NAME,
  PROP_PAINTABLE,
  PROP_EMOJI,
  PROP_COUNT,
  PROP_ACTIVE,
  PROP_TOGGLEABLE,
  PROP_APPEARANCE,
  N_PROPS,
};

enum {
  SIGNAL_ACTIVATE,
  SIGNAL_TOGGLED,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraChip, awra_chip, GTK_TYPE_WIDGET)

/* Metrics */

static double
chip_height (AwraChip *self)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (GTK_WIDGET (self));

  return round (awra_token_set_get_control_height (tokens) * 0.82);
}

static double
icon_size (AwraChip *self)
{
  return round (chip_height (self) * 0.64);
}

static double
inner_gap (AwraChip *self)
{
  return round (chip_height (self) * 0.2);
}

static double
side_padding (AwraChip *self)
{
  return round (chip_height (self) * 0.34);
}

static gboolean
has_leading (AwraChip *self)
{
  return self->paintable != NULL || (self->emoji != NULL && *self->emoji != '\0') ||
         self->icon_name != NULL;
}

static gboolean
has_label (AwraChip *self)
{
  return self->label != NULL && *self->label != '\0';
}

static PangoLayout *
count_layout_for (AwraChip *self,
                  int       count)
{
  g_autofree char *text = g_strdup_printf ("%d", count);
  PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), text);
  PangoAttrList *attributes = pango_attr_list_new ();

  /* Figures of equal width keep the chip still while digits roll. */
  pango_attr_list_insert (attributes, pango_attr_font_features_new ("tnum=1"));
  pango_attr_list_insert (attributes, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  pango_layout_set_attributes (layout, attributes);
  pango_attr_list_unref (attributes);
  return layout;
}

static PangoLayout *
ensure_label_layout (AwraChip *self)
{
  if (self->label_layout == NULL && has_label (self))
    self->label_layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), self->label);
  return self->label_layout;
}

static PangoLayout *
ensure_count_layout (AwraChip *self)
{
  if (self->count_layout == NULL && self->count >= 0)
    self->count_layout = count_layout_for (self, self->count);
  return self->count_layout;
}

static PangoLayout *
ensure_emoji_layout (AwraChip *self)
{
  if (self->emoji_layout == NULL && self->emoji != NULL && *self->emoji != '\0') {
    PangoFontDescription *font;

    self->emoji_layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), self->emoji);
    font = pango_font_description_copy (
      pango_context_get_font_description (pango_layout_get_context (self->emoji_layout)));
    pango_font_description_set_absolute_size (font, icon_size (self) * 0.82 * PANGO_SCALE);
    pango_layout_set_font_description (self->emoji_layout, font);
    pango_font_description_free (font);
  }
  return self->emoji_layout;
}

static int
layout_width (PangoLayout *layout)
{
  PangoRectangle logical;

  if (layout == NULL)
    return 0;
  pango_layout_get_pixel_extents (layout, NULL, &logical);
  return logical.width;
}

static double
content_width (AwraChip *self)
{
  double width = 0;
  int parts = 0;

  if (has_leading (self)) {
    width += icon_size (self);
    parts++;
  }
  if (has_label (self)) {
    width += layout_width (ensure_label_layout (self));
    parts++;
  }
  if (self->count >= 0) {
    width += layout_width (ensure_count_layout (self));
    parts++;
  }
  return width + MAX (parts - 1, 0) * inner_gap (self);
}

/* Animation */

#define DEFINE_SETTER(field) \
  static void \
  set_##field (double   value, \
               gpointer user_data) \
  { \
    AwraChip *self = user_data; \
    self->field = value; \
    gtk_widget_queue_draw (GTK_WIDGET (self)); \
  }

DEFINE_SETTER (scale)
DEFINE_SETTER (active_mix)
DEFINE_SETTER (hover_mix)

static void
set_roll (double   value,
          gpointer user_data)
{
  AwraChip *self = user_data;

  self->roll = value;
  if (value >= 1.0)
    g_clear_object (&self->previous_layout);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
spring_to (AwraChip                *self,
           AwraAnimation          **animation,
           double                   current,
           double                   target,
           double                   damping,
           double                   stiffness,
           gboolean                 clamp,
           AwraAnimationTargetFunc  func)
{
  if (*animation == NULL) {
    if (current == target)
      return;
    *animation = awra_spring_animation_new (GTK_WIDGET (self), current, current, damping,
                                            stiffness,
                                            awra_callback_animation_target_new (func, self, NULL));
    awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (*animation), 0.001);
    awra_spring_animation_set_clamp (AWRA_SPRING_ANIMATION (*animation), clamp);
  }
  awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (*animation), target);
}

static void
update_presentation (AwraChip *self)
{
  /* Pressing gives way quickly; letting go springs back past rest. */
  spring_to (self, &self->scale_animation, self->scale,
             self->pressed ? PRESSED_SCALE : 1.0, self->pressed ? 1.0 : 0.38,
             self->pressed ? 900.0 : 520.0, FALSE, set_scale);
  spring_to (self, &self->hover_animation, self->hover_mix, self->hovered ? 1.0 : 0.0,
             1.0, 600.0, TRUE, set_hover_mix);
}

static void
update_accessible (AwraChip *self)
{
  const char *name = has_label (self) ? self->label : self->emoji;
  g_autofree char *label = NULL;

  if (name != NULL && self->count >= 0)
    label = g_strdup_printf ("%s, %d", name, self->count);
  else if (name != NULL)
    label = g_strdup (name);
  else if (self->count >= 0)
    label = g_strdup_printf ("%d", self->count);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  label != NULL ? label : "", -1);
  gtk_accessible_update_state (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_STATE_PRESSED,
                               self->active ? GTK_ACCESSIBLE_TRISTATE_TRUE
                                            : GTK_ACCESSIBLE_TRISTATE_FALSE,
                               -1);
}

static void
set_active_internal (AwraChip *self,
                     gboolean  active)
{
  active = !!active;
  if (self->active == active)
    return;
  self->active = active;
  if (active)
    gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_CHECKED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_CHECKED);
  spring_to (self, &self->active_animation, self->active_mix, active ? 1.0 : 0.0,
             1.0, 520.0, TRUE, set_active_mix);
  update_accessible (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
}

/* Input */

static void
awra_chip_real_activate (AwraChip *self)
{
  if (!self->toggleable)
    return;
  set_active_internal (self, !self->active);
  if (!self->pressed) {
    if (self->scale_animation == NULL) {
      self->scale_animation =
        awra_spring_animation_new (GTK_WIDGET (self), PRESSED_SCALE, 1.0, 0.38, 520.0,
                                   awra_callback_animation_target_new (set_scale, self, NULL));
      awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (self->scale_animation), 0.001);
    }
    /* From the keyboard: the same nod as a click. */
    awra_spring_animation_set_value_from (AWRA_SPRING_ANIMATION (self->scale_animation),
                                          PRESSED_SCALE);
    awra_spring_animation_set_initial_velocity (AWRA_SPRING_ANIMATION (self->scale_animation),
                                                0.0);
    awra_spring_animation_set_value_to (AWRA_SPRING_ANIMATION (self->scale_animation), 1.0);
    awra_spring_animation_set_damping_ratio (AWRA_SPRING_ANIMATION (self->scale_animation), 0.38);
    awra_spring_animation_set_stiffness (AWRA_SPRING_ANIMATION (self->scale_animation), 520.0);
    awra_animation_play (self->scale_animation);
  }
  g_signal_emit (self, signals[SIGNAL_TOGGLED], 0);
}

static void
motion_enter_cb (GtkEventControllerMotion *controller,
                 double                    x,
                 double                    y,
                 AwraChip                 *self)
{
  (void) controller;
  (void) x;
  (void) y;
  self->hovered = TRUE;
  update_presentation (self);
}

static void
motion_leave_cb (GtkEventControllerMotion *controller,
                 AwraChip                 *self)
{
  (void) controller;
  self->hovered = FALSE;
  self->pressed = FALSE;
  update_presentation (self);
}

static void
click_pressed_cb (GtkGestureClick *gesture,
                  int              n_press,
                  double           x,
                  double           y,
                  AwraChip        *self)
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
                   AwraChip        *self)
{
  (void) n_press;
  self->pressed = FALSE;
  update_presentation (self);
  if (gtk_widget_contains (GTK_WIDGET (self), x, y)) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    g_signal_emit (self, signals[SIGNAL_ACTIVATE], 0);
  }
}

static void
click_stopped_cb (GtkGestureClick *gesture,
                  AwraChip        *self)
{
  (void) gesture;
  if (!self->pressed)
    return;
  self->pressed = FALSE;
  update_presentation (self);
}

/* Drawing */

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

typedef struct {
  GdkRGBA background;
  GdkRGBA border;
  GdkRGBA foreground;
  GdkRGBA count;
} ChipColors;

static ChipColors
chip_colors (AwraChip     *self,
             AwraTokenSet *tokens)
{
  ChipColors rest;
  ChipColors on;
  ChipColors result;
  gboolean dark = awra_token_set_get_dark (tokens);
  /* A raised plate on a dark canvas, a soft well on a light one. */
  GdkRGBA plate = dark ? *awra_token_set_get_control_cap (tokens)
                       : *awra_token_set_get_control_well (tokens);
  GdkRGBA hover_fill = dark ? *awra_token_set_get_control_cap_hover (tokens)
                            : awra_rgba_with_alpha (awra_token_set_get_control_well (tokens), 1.6);
  GdkRGBA clear = { 0, 0, 0, 0 };

  if (self->appearance == AWRA_CHIP_APPEARANCE_ACCENT) {
    rest.background = *awra_token_set_get_accent_soft (tokens);
    rest.border = clear;
    rest.foreground = *awra_token_set_get_accent_content (tokens);
    rest.count = rest.foreground;
    on.background = *awra_token_set_get_accent (tokens);
    on.border = *awra_token_set_get_accent (tokens);
    on.foreground = *awra_token_set_get_on_accent (tokens);
    on.count = on.foreground;
  } else {
    rest.background = plate;
    rest.border = clear;
    rest.foreground = *awra_token_set_get_foreground (tokens);
    rest.count = *awra_token_set_get_muted_foreground (tokens);
    on.background = *awra_token_set_get_accent_soft (tokens);
    on.border = awra_rgba_with_alpha (awra_token_set_get_accent (tokens), 0.9);
    on.foreground = *awra_token_set_get_accent_content (tokens);
    on.count = on.foreground;
  }
  if (awra_token_set_get_high_contrast (tokens))
    rest.border = *awra_token_set_get_outline (tokens);
  /* Hover lifts a resting chip a little; an active chip keeps its tint. */
  if (self->appearance == AWRA_CHIP_APPEARANCE_NEUTRAL)
    rest.background = mix (&rest.background, &hover_fill, self->hover_mix);
  rest.border = mix (&rest.border, awra_token_set_get_outline (tokens), self->hover_mix);
  result.background = mix (&rest.background, &on.background, self->active_mix);
  result.border = mix (&rest.border, &on.border, self->active_mix);
  result.foreground = mix (&rest.foreground, &on.foreground, self->active_mix);
  result.count = mix (&rest.count, &on.count, self->active_mix);
  return result;
}

static void
draw_layout_centered (GtkSnapshot   *snapshot,
                      PangoLayout   *layout,
                      double         x,
                      double         height,
                      double         offset_y,
                      const GdkRGBA *color)
{
  PangoRectangle logical;

  pango_layout_get_pixel_extents (layout, NULL, &logical);
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (round (x - logical.x),
                                                round ((height - logical.height) / 2.0 -
                                                       logical.y + offset_y)));
  gtk_snapshot_append_layout (snapshot, layout, color);
  gtk_snapshot_restore (snapshot);
}

static void
draw_leading (AwraChip      *self,
              GtkSnapshot   *snapshot,
              double         x,
              double         height,
              const GdkRGBA *color)
{
  double size = icon_size (self);
  double y = round ((height - size) / 2.0);

  if (self->paintable != NULL) {
    double ratio = gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);
    double width = size;
    double tall = size;

    /* Contain: the whole picture, centred in the icon square. */
    if (ratio > 1.0)
      tall = size / ratio;
    else if (ratio > 0.0)
      width = size * ratio;
    if (GDK_IS_TEXTURE (self->paintable)) {
      gtk_snapshot_append_scaled_texture (snapshot, GDK_TEXTURE (self->paintable),
                                          GSK_SCALING_FILTER_TRILINEAR,
                                          &GRAPHENE_RECT_INIT (x + (size - width) / 2,
                                                               y + (size - tall) / 2,
                                                               width, tall));
    } else {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + (size - width) / 2,
                                                              y + (size - tall) / 2));
      gdk_paintable_snapshot (self->paintable, snapshot, width, tall);
      gtk_snapshot_restore (snapshot);
    }
  } else if (ensure_emoji_layout (self) != NULL) {
    PangoRectangle ink;

    pango_layout_get_pixel_extents (self->emoji_layout, &ink, NULL);
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot,
                            &GRAPHENE_POINT_INIT (round (x + (size - ink.width) / 2.0 - ink.x),
                                                  round ((height - ink.height) / 2.0 - ink.y)));
    gtk_snapshot_append_layout (snapshot, self->emoji_layout, color);
    gtk_snapshot_restore (snapshot);
  } else if (self->icon_name != NULL) {
    int px = (int) round (size * 0.9);

    if (self->icon == NULL || self->icon_px != px) {
      g_clear_object (&self->icon);
      self->icon_px = px;
      self->icon = GDK_PAINTABLE (gtk_icon_theme_lookup_icon (
        gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (self))),
        self->icon_name, NULL, px, gtk_widget_get_scale_factor (GTK_WIDGET (self)),
        gtk_widget_get_direction (GTK_WIDGET (self)), 0));
    }
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (round (x + (size - px) / 2),
                                                            round ((height - px) / 2)));
    gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (self->icon), snapshot,
                                              px, px, color, 1);
    gtk_snapshot_restore (snapshot);
  }
}

/* The digits that differ roll; a shared leading part stays still. */
static void
draw_count (AwraChip      *self,
            GtkSnapshot   *snapshot,
            double         x,
            double         height,
            const GdkRGBA *color)
{
  PangoLayout *layout = ensure_count_layout (self);
  const char *now;
  const char *before;
  size_t prefix = 0;
  int direction;
  double travel = height * 0.7;
  double eased;

  if (self->previous_layout == NULL || self->roll >= 1.0) {
    draw_layout_centered (snapshot, layout, x, height, 0, color);
    return;
  }
  now = pango_layout_get_text (layout);
  before = pango_layout_get_text (self->previous_layout);
  if (strlen (now) == strlen (before)) {
    while (now[prefix] != '\0' && now[prefix] == before[prefix])
      prefix++;
  }
  direction = self->count > self->previous_count ? 1 : -1;
  eased = awra_easing_ease (AWRA_EASING_EASE_OUT_CUBIC, self->roll);
  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (x - 2, 0, layout_width (layout) + 4,
                                                         height));
  if (prefix > 0) {
    PangoRectangle position;
    GdkRGBA faded;

    /* Unchanged digits, drawn once, then the changing ones beside them. */
    pango_layout_index_to_pos (layout, (int) prefix, &position);
    gtk_snapshot_push_clip (snapshot,
                            &GRAPHENE_RECT_INIT (x - 2, 0, position.x / PANGO_SCALE + 2, height));
    draw_layout_centered (snapshot, layout, x, height, 0, color);
    gtk_snapshot_pop (snapshot);
    gtk_snapshot_push_clip (snapshot,
                            &GRAPHENE_RECT_INIT (x + position.x / PANGO_SCALE, 0,
                                                 layout_width (layout), height));
    faded = awra_rgba_with_alpha (color, 1.0 - eased);
    draw_layout_centered (snapshot, self->previous_layout, x, height, -direction * travel * eased,
                          &faded);
    faded = awra_rgba_with_alpha (color, eased);
    draw_layout_centered (snapshot, layout, x, height, direction * travel * (1.0 - eased),
                          &faded);
    gtk_snapshot_pop (snapshot);
  } else {
    GdkRGBA faded = awra_rgba_with_alpha (color, 1.0 - eased);

    draw_layout_centered (snapshot, self->previous_layout, x, height, -direction * travel * eased,
                          &faded);
    faded = awra_rgba_with_alpha (color, eased);
    draw_layout_centered (snapshot, layout, x, height, direction * travel * (1.0 - eased),
                          &faded);
  }
  gtk_snapshot_pop (snapshot);
}

static void
awra_chip_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  AwraChip *self = AWRA_CHIP (widget);
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);
  double chip = MIN (height, chip_height (self));
  double top = round ((height - chip) / 2.0);
  ChipColors colors = chip_colors (self, tokens);
  gboolean rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  double content = content_width (self);
  double x = rtl ? width - side_padding (self) : side_padding (self);
  GskRoundedRect pill;
  float widths[4] = { 1, 1, 1, 1 };
  GdkRGBA borders[4] = { colors.border, colors.border, colors.border, colors.border };

  (void) content;
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2, height / 2));
  gtk_snapshot_scale (snapshot, self->scale, self->scale);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-width / 2, -height / 2 + top));
  if (!gtk_widget_is_sensitive (widget))
    gtk_snapshot_push_opacity (snapshot, 0.5);
  gsk_rounded_rect_init_from_rect (&pill, &GRAPHENE_RECT_INIT (0, 0, width, chip), chip / 2);
  gtk_snapshot_push_rounded_clip (snapshot, &pill);
  gtk_snapshot_append_color (snapshot, &colors.background, &pill.bounds);
  gtk_snapshot_pop (snapshot);
  if (colors.border.alpha > 0)
    gtk_snapshot_append_border (snapshot, &pill, widths, borders);
  if (has_leading (self)) {
    double size = icon_size (self);

    draw_leading (self, snapshot, rtl ? x - size : x, chip, &colors.foreground);
    x += rtl ? -(size + inner_gap (self)) : size + inner_gap (self);
  }
  if (has_label (self)) {
    double label = layout_width (ensure_label_layout (self));

    draw_layout_centered (snapshot, self->label_layout, rtl ? x - label : x, chip, 0,
                          &colors.foreground);
    x += rtl ? -(label + inner_gap (self)) : label + inner_gap (self);
  }
  if (self->count >= 0) {
    double count = layout_width (ensure_count_layout (self));

    draw_count (self, snapshot, rtl ? x - count : x, chip, &colors.count);
  }
  if (gtk_widget_has_visible_focus (widget)) {
    GskRoundedRect ring;
    const GdkRGBA *focus = awra_token_set_get_focus_ring (tokens);
    float ring_widths[4] = { 2, 2, 2, 2 };
    GdkRGBA ring_colors[4] = { *focus, *focus, *focus, *focus };

    gsk_rounded_rect_init_from_rect (&ring, &GRAPHENE_RECT_INIT (-3, -3, width + 6, chip + 6),
                                     chip / 2 + 3);
    gtk_snapshot_append_border (snapshot, &ring, ring_widths, ring_colors);
  }
  if (!gtk_widget_is_sensitive (widget))
    gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
}

/* Layout */

static void
awra_chip_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  AwraChip *self = AWRA_CHIP (widget);
  double height = chip_height (self);

  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  if (orientation == GTK_ORIENTATION_VERTICAL) {
    *minimum = *natural = (int) height;
    return;
  }
  /* A chip with only a picture or a count stays at least round. */
  *minimum = *natural = (int) MAX (height, ceil (content_width (self) + 2 * side_padding (self)));
}

static void
clear_layouts (AwraChip *self)
{
  g_clear_object (&self->label_layout);
  g_clear_object (&self->emoji_layout);
  g_clear_object (&self->count_layout);
  g_clear_object (&self->previous_layout);
  g_clear_object (&self->icon);
}

static void
style_changed (gpointer user_data)
{
  AwraChip *self = user_data;

  clear_layouts (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
awra_chip_map (GtkWidget *widget)
{
  AwraChip *self = AWRA_CHIP (widget);

  GTK_WIDGET_CLASS (awra_chip_parent_class)->map (widget);
  awra_style_watch_start (&self->watch, widget, style_changed, self);
}

static void
awra_chip_unmap (GtkWidget *widget)
{
  AwraChip *self = AWRA_CHIP (widget);

  awra_style_watch_stop (&self->watch);
  self->hovered = FALSE;
  self->pressed = FALSE;
  GTK_WIDGET_CLASS (awra_chip_parent_class)->unmap (widget);
}

static void
awra_chip_system_setting_changed (GtkWidget        *widget,
                                  GtkSystemSetting  setting)
{
  clear_layouts (AWRA_CHIP (widget));
  gtk_widget_queue_resize (widget);
  GTK_WIDGET_CLASS (awra_chip_parent_class)->system_setting_changed (widget, setting);
}

static void
awra_chip_direction_changed (GtkWidget        *widget,
                             GtkTextDirection  previous)
{
  clear_layouts (AWRA_CHIP (widget));
  GTK_WIDGET_CLASS (awra_chip_parent_class)->direction_changed (widget, previous);
}

static void
paintable_invalidated_cb (AwraChip *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
stop_animation (AwraAnimation **animation)
{
  if (*animation != NULL)
    awra_animation_skip (*animation);
  g_clear_object (animation);
}

static void
awra_chip_dispose (GObject *object)
{
  AwraChip *self = AWRA_CHIP (object);

  awra_style_watch_stop (&self->watch);
  stop_animation (&self->scale_animation);
  stop_animation (&self->active_animation);
  stop_animation (&self->hover_animation);
  stop_animation (&self->roll_animation);
  if (self->paintable != NULL)
    g_signal_handlers_disconnect_by_func (self->paintable, paintable_invalidated_cb, self);
  g_clear_object (&self->paintable);
  clear_layouts (self);
  G_OBJECT_CLASS (awra_chip_parent_class)->dispose (object);
}

static void
awra_chip_finalize (GObject *object)
{
  AwraChip *self = AWRA_CHIP (object);

  g_free (self->label);
  g_free (self->icon_name);
  g_free (self->emoji);
  G_OBJECT_CLASS (awra_chip_parent_class)->finalize (object);
}

static void
awra_chip_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  AwraChip *self = AWRA_CHIP (object);

  switch (prop_id) {
    case PROP_LABEL: g_value_set_string (value, self->label); break;
    case PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
    case PROP_PAINTABLE: g_value_set_object (value, self->paintable); break;
    case PROP_EMOJI: g_value_set_string (value, self->emoji); break;
    case PROP_COUNT: g_value_set_int (value, self->count); break;
    case PROP_ACTIVE: g_value_set_boolean (value, self->active); break;
    case PROP_TOGGLEABLE: g_value_set_boolean (value, self->toggleable); break;
    case PROP_APPEARANCE: g_value_set_enum (value, self->appearance); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_chip_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  AwraChip *self = AWRA_CHIP (object);

  switch (prop_id) {
    case PROP_LABEL: awra_chip_set_label (self, g_value_get_string (value)); break;
    case PROP_ICON_NAME: awra_chip_set_icon_name (self, g_value_get_string (value)); break;
    case PROP_PAINTABLE: awra_chip_set_paintable (self, g_value_get_object (value)); break;
    case PROP_EMOJI: awra_chip_set_emoji (self, g_value_get_string (value)); break;
    case PROP_COUNT: awra_chip_set_count (self, g_value_get_int (value)); break;
    case PROP_ACTIVE: awra_chip_set_active (self, g_value_get_boolean (value)); break;
    case PROP_TOGGLEABLE: awra_chip_set_toggleable (self, g_value_get_boolean (value)); break;
    case PROP_APPEARANCE: awra_chip_set_appearance (self, g_value_get_enum (value)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_chip_class_init (AwraChipClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->dispose = awra_chip_dispose;
  object_class->finalize = awra_chip_finalize;
  object_class->get_property = awra_chip_get_property;
  object_class->set_property = awra_chip_set_property;
  widget_class->snapshot = awra_chip_snapshot;
  widget_class->measure = awra_chip_measure;
  widget_class->map = awra_chip_map;
  widget_class->unmap = awra_chip_unmap;
  widget_class->system_setting_changed = awra_chip_system_setting_changed;
  widget_class->direction_changed = awra_chip_direction_changed;

  /**
   * AwraChip:label:
   *
   * The text of the chip.
   *
   * Since: 1.1
   */
  properties[PROP_LABEL] = g_param_spec_string ("label", NULL, NULL, NULL, flags);
  /**
   * AwraChip:icon-name:
   *
   * A symbolic icon before the label.
   *
   * Since: 1.1
   */
  properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL, NULL, flags);
  /**
   * AwraChip:paintable:
   *
   * A picture before the label, such as a custom emoji.
   *
   * Since: 1.1
   */
  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL, GDK_TYPE_PAINTABLE, flags);
  /**
   * AwraChip:emoji:
   *
   * An emoji, or any short text, drawn at icon size before the label.
   *
   * Since: 1.1
   */
  properties[PROP_EMOJI] = g_param_spec_string ("emoji", NULL, NULL, NULL, flags);
  /**
   * AwraChip:count:
   *
   * The count, or -1 to show none.
   *
   * Since: 1.1
   */
  properties[PROP_COUNT] = g_param_spec_int ("count", NULL, NULL, -1, G_MAXINT, -1, flags);
  /**
   * AwraChip:active:
   *
   * Whether the chip is on.
   *
   * Since: 1.1
   */
  properties[PROP_ACTIVE] = g_param_spec_boolean ("active", NULL, NULL, FALSE, flags);
  /**
   * AwraChip:toggleable:
   *
   * Whether clicking the chip turns it on and off.
   *
   * Since: 1.1
   */
  properties[PROP_TOGGLEABLE] = g_param_spec_boolean ("toggleable", NULL, NULL, TRUE, flags);
  /**
   * AwraChip:appearance:
   *
   * The tone of the chip.
   *
   * Since: 1.1
   */
  properties[PROP_APPEARANCE] =
    g_param_spec_enum ("appearance", NULL, NULL, AWRA_TYPE_CHIP_APPEARANCE,
                       AWRA_CHIP_APPEARANCE_NEUTRAL, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraChip::activate:
   * @self: the chip
   *
   * Emitted when the chip is clicked, or when Enter or Space is pressed on
   * it. The default handler turns a toggleable chip on or off.
   *
   * Since: 1.1
   */
  signals[SIGNAL_ACTIVATE] =
    g_signal_new_class_handler ("activate", G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (awra_chip_real_activate),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);
  /**
   * AwraChip::toggled:
   * @self: the chip
   *
   * Emitted when the user turned the chip on or off. Read
   * #AwraChip:active for the new state.
   *
   * Since: 1.1
   */
  signals[SIGNAL_TOGGLED] =
    g_signal_new ("toggled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL, G_TYPE_NONE, 0);
  gtk_widget_class_set_activate_signal (widget_class, signals[SIGNAL_ACTIVATE]);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0, "activate", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0, "activate", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0, "activate", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Space, 0, "activate", NULL);
  gtk_widget_class_set_css_name (widget_class, "awrachip");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON);
}

static void
awra_chip_init (AwraChip *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkEventController *motion = gtk_event_controller_motion_new ();
  GtkGesture *click = gtk_gesture_click_new ();

  self->count = -1;
  self->toggleable = TRUE;
  self->scale = 1.0;
  self->roll = 1.0;
  gtk_widget_set_focusable (widget, TRUE);
  gtk_widget_set_focus_on_click (widget, FALSE);
  gtk_widget_set_cursor_from_name (widget, "pointer");
  g_signal_connect (motion, "enter", G_CALLBACK (motion_enter_cb), self);
  g_signal_connect (motion, "leave", G_CALLBACK (motion_leave_cb), self);
  gtk_widget_add_controller (widget, motion);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_PRIMARY);
  g_signal_connect (click, "pressed", G_CALLBACK (click_pressed_cb), self);
  g_signal_connect (click, "released", G_CALLBACK (click_released_cb), self);
  g_signal_connect (click, "stopped", G_CALLBACK (click_stopped_cb), self);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (click));
  update_accessible (self);
}

GtkWidget *
awra_chip_new (const char *label)
{
  return g_object_new (AWRA_TYPE_CHIP, "label", label, NULL);
}

const char *
awra_chip_get_label (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), NULL);
  return self->label;
}

void
awra_chip_set_label (AwraChip   *self,
                     const char *label)
{
  g_return_if_fail (AWRA_IS_CHIP (self));
  if (!g_set_str (&self->label, label))
    return;
  g_clear_object (&self->label_layout);
  update_accessible (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

const char *
awra_chip_get_icon_name (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), NULL);
  return self->icon_name;
}

void
awra_chip_set_icon_name (AwraChip   *self,
                         const char *icon_name)
{
  g_return_if_fail (AWRA_IS_CHIP (self));
  if (!g_set_str (&self->icon_name, icon_name))
    return;
  g_clear_object (&self->icon);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

GdkPaintable *
awra_chip_get_paintable (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), NULL);
  return self->paintable;
}

void
awra_chip_set_paintable (AwraChip     *self,
                         GdkPaintable *paintable)
{
  g_return_if_fail (AWRA_IS_CHIP (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));
  if (self->paintable == paintable)
    return;
  if (self->paintable != NULL)
    g_signal_handlers_disconnect_by_func (self->paintable, paintable_invalidated_cb, self);
  g_set_object (&self->paintable, paintable);
  if (paintable != NULL)
    g_signal_connect_swapped (paintable, "invalidate-contents",
                              G_CALLBACK (paintable_invalidated_cb), self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

const char *
awra_chip_get_emoji (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), NULL);
  return self->emoji;
}

void
awra_chip_set_emoji (AwraChip   *self,
                     const char *emoji)
{
  g_return_if_fail (AWRA_IS_CHIP (self));
  if (!g_set_str (&self->emoji, emoji))
    return;
  g_clear_object (&self->emoji_layout);
  update_accessible (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EMOJI]);
}

int
awra_chip_get_count (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), -1);
  return self->count;
}

void
awra_chip_set_count (AwraChip *self,
                     int       count)
{
  int previous;

  g_return_if_fail (AWRA_IS_CHIP (self));
  g_return_if_fail (count >= -1);
  if (self->count == count)
    return;
  previous = self->count;
  self->count = count;
  g_clear_object (&self->previous_layout);
  if (previous >= 0 && count >= 0 && gtk_widget_get_mapped (GTK_WIDGET (self))) {
    /* The number on screen rolls away; the new one rolls in. */
    self->previous_layout = g_steal_pointer (&self->count_layout);
    self->previous_count = previous;
    if (self->previous_layout == NULL)
      self->previous_layout = count_layout_for (self, previous);
    if (self->roll_animation == NULL)
      self->roll_animation =
        awra_timed_animation_new_for_preset (GTK_WIDGET (self), 0.0, 1.0,
                                             AWRA_MOTION_PRESET_NORMAL,
                                             awra_callback_animation_target_new (set_roll, self,
                                                                                 NULL));
    awra_timed_animation_set_easing (AWRA_TIMED_ANIMATION (self->roll_animation),
                                     AWRA_EASING_LINEAR);
    awra_animation_play (self->roll_animation);
  } else {
    g_clear_object (&self->count_layout);
  }
  update_accessible (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COUNT]);
}

gboolean
awra_chip_get_active (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), FALSE);
  return self->active;
}

void
awra_chip_set_active (AwraChip *self,
                      gboolean  active)
{
  g_return_if_fail (AWRA_IS_CHIP (self));
  set_active_internal (self, active);
}

gboolean
awra_chip_get_toggleable (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), TRUE);
  return self->toggleable;
}

void
awra_chip_set_toggleable (AwraChip *self,
                          gboolean  toggleable)
{
  g_return_if_fail (AWRA_IS_CHIP (self));
  toggleable = !!toggleable;
  if (self->toggleable == toggleable)
    return;
  self->toggleable = toggleable;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOGGLEABLE]);
}

AwraChipAppearance
awra_chip_get_appearance (AwraChip *self)
{
  g_return_val_if_fail (AWRA_IS_CHIP (self), AWRA_CHIP_APPEARANCE_NEUTRAL);
  return self->appearance;
}

void
awra_chip_set_appearance (AwraChip           *self,
                          AwraChipAppearance  appearance)
{
  g_return_if_fail (AWRA_IS_CHIP (self));
  g_return_if_fail (appearance == AWRA_CHIP_APPEARANCE_NEUTRAL ||
                    appearance == AWRA_CHIP_APPEARANCE_ACCENT);
  if (self->appearance == appearance)
    return;
  self->appearance = appearance;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPEARANCE]);
}
