/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-avatar.h>

#include <awra/awra-spring-animation.h>
#include <awra/awra-timed-animation.h>
#include <awra/awra-token-set.h>
#include <math.h>

#include "awra-i18n-private.h"
#include "widgets/awra-widget-style-private.h"

#define DEFAULT_SIZE 32
#define MIN_SIZE 8
#define DEFAULT_ICON "awra-avatar-default-symbolic"
/* Hues that stay pleasant at the soft lightness avatars use; muddy greens
 * and yellows are left out. */
static const double avatar_hues[] = {
  4, 22, 38, 145, 168, 188, 205, 222, 240, 262, 284, 318, 338,
};
#define PULSE_DURATION 1200

struct _AwraAvatar {
  GtkWidget parent_instance;
  int size;
  char *text;
  char *initials;
  guint hue;
  gboolean show_initials;
  GdkPaintable *image;
  GdkPaintable *previous_image;
  gboolean crossfading;
  char *icon_name;
  GdkPaintable *icon;
  int icon_px;
  AwraAvatarShape shape;
  AwraPresence presence;
  AwraPresence shown_presence;
  gboolean speaking;
  PangoLayout *layout;
  double crossfade;
  double presence_scale;
  double speaking_reveal;
  double pulse;
  AwraAnimation *crossfade_animation;
  AwraAnimation *presence_animation;
  AwraAnimation *speaking_animation;
  AwraAnimation *pulse_animation;
  AwraStyleWatch watch;
};

enum {
  PROP_0,
  PROP_SIZE,
  PROP_TEXT,
  PROP_SHOW_INITIALS,
  PROP_CUSTOM_IMAGE,
  PROP_ICON_NAME,
  PROP_SHAPE,
  PROP_PRESENCE,
  PROP_SPEAKING,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraAvatar, awra_avatar, GTK_TYPE_WIDGET)

/* Colour */

static GdkRGBA
hsl (double hue,
     double saturation,
     double lightness)
{
  double chroma = (1.0 - fabs (2.0 * lightness - 1.0)) * saturation;
  double sector = fmod (hue / 60.0, 6.0);
  double second = chroma * (1.0 - fabs (fmod (sector, 2.0) - 1.0));
  double base = lightness - chroma / 2.0;
  double r = 0, g = 0, b = 0;

  if (sector < 1) { r = chroma; g = second; }
  else if (sector < 2) { r = second; g = chroma; }
  else if (sector < 3) { g = chroma; b = second; }
  else if (sector < 4) { g = second; b = chroma; }
  else if (sector < 5) { r = second; b = chroma; }
  else { r = chroma; b = second; }
  return (GdkRGBA) { r + base, g + base, b + base, 1.0 };
}

static guint
hue_for_text (const char *text)
{
  /* FNV-1a: stable across runs and platforms, unlike g_str_hash. */
  guint32 hash = 2166136261u;

  for (const guchar *p = (const guchar *) text; *p != '\0'; p++) {
    hash ^= *p;
    hash *= 16777619u;
  }
  return hash % G_N_ELEMENTS (avatar_hues);
}

static void
face_colors (AwraAvatar *self,
             GdkRGBA    *background,
             GdkRGBA    *foreground)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (GTK_WIDGET (self));
  gboolean dark = awra_token_set_get_dark (tokens);
  gboolean high_contrast = awra_token_set_get_high_contrast (tokens);
  double hue;

  if (self->text == NULL || *self->text == '\0') {
    *background = awra_rgba_with_alpha (awra_token_set_get_foreground (tokens),
                                        dark ? 0.14 : 0.10);
    *foreground = *awra_token_set_get_muted_foreground (tokens);
    return;
  }
  hue = avatar_hues[self->hue];
  /* Soft tinted plates with text of the same hue: legible, and calm when
   * many avatars sit side by side. */
  if (dark) {
    *background = hsl (hue, 0.48, high_contrast ? 0.26 : 0.35);
    *foreground = hsl (hue, 0.85, high_contrast ? 0.94 : 0.87);
  } else {
    *background = hsl (hue, 0.70, high_contrast ? 0.90 : 0.86);
    *foreground = hsl (hue, 0.60, high_contrast ? 0.20 : 0.30);
  }
}

/* Initials: the first letter of the first and the last word. */
static char *
compute_initials (const char *text)
{
  GString *initials = g_string_new (NULL);
  const char *first = NULL;
  const char *last = NULL;
  gboolean at_word_start = TRUE;
  g_autofree char *upper = NULL;

  if (text == NULL)
    return NULL;
  for (const char *p = text; *p != '\0'; p = g_utf8_next_char (p)) {
    gunichar c = g_utf8_get_char (p);

    if (g_unichar_isalnum (c)) {
      if (at_word_start) {
        if (first == NULL)
          first = p;
        else
          last = p;
      }
      at_word_start = FALSE;
    } else if (g_unichar_isspace (c) || c == '-' || c == '_' || c == '.') {
      at_word_start = TRUE;
    }
  }
  if (first == NULL) {
    /* No letter at all, such as an emoji: show the first character. */
    if (*text == '\0' || g_unichar_isspace (g_utf8_get_char (text))) {
      g_string_free (initials, TRUE);
      return NULL;
    }
    first = text;
  }
  g_string_append_len (initials, first, g_utf8_next_char (first) - first);
  if (last != NULL)
    g_string_append_len (initials, last, g_utf8_next_char (last) - last);
  upper = g_utf8_strup (initials->str, -1);
  g_string_free (initials, TRUE);
  return g_steal_pointer (&upper);
}

/* Geometry */

static double
corner_radius (AwraAvatar *self)
{
  return self->shape == AWRA_AVATAR_SHAPE_CIRCLE ? self->size / 2.0 : self->size * 0.26;
}

static double
dot_diameter (AwraAvatar *self)
{
  return MAX (6.0, round (self->size * 0.3));
}

static double
cutout_width (AwraAvatar *self)
{
  return MAX (1.5, self->size * 0.07);
}

static graphene_point_t
dot_center (AwraAvatar *self)
{
  double radius = dot_diameter (self) / 2.0;
  double inset = self->shape == AWRA_AVATAR_SHAPE_CIRCLE ? radius : radius - 1.0;
  float x = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL
              ? inset : self->size - inset;

  return GRAPHENE_POINT_INIT (x, self->size - inset);
}

static void
fill_rounded (GtkSnapshot   *snapshot,
              float          x,
              float          y,
              float          width,
              float          height,
              float          radius,
              const GdkRGBA *color)
{
  GskRoundedRect shape;

  gsk_rounded_rect_init_from_rect (&shape, &GRAPHENE_RECT_INIT (x, y, width, height),
                                   MIN (radius, MIN (width, height) / 2.0f));
  gtk_snapshot_push_rounded_clip (snapshot, &shape);
  gtk_snapshot_append_color (snapshot, color, &shape.bounds);
  gtk_snapshot_pop (snapshot);
}

static void
fill_circle (GtkSnapshot   *snapshot,
             float          cx,
             float          cy,
             float          radius,
             const GdkRGBA *color)
{
  fill_rounded (snapshot, cx - radius, cy - radius, radius * 2, radius * 2, radius, color);
}

/* Animation */

static void
set_crossfade (double   value,
               gpointer user_data)
{
  AwraAvatar *self = user_data;

  self->crossfade = value;
  if (value >= 1.0) {
    self->crossfading = FALSE;
    g_clear_object (&self->previous_image);
  }
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_presence_scale (double   value,
                    gpointer user_data)
{
  AwraAvatar *self = user_data;

  self->presence_scale = MAX (value, 0.0);
  if (self->presence_scale <= 0.0)
    self->shown_presence = self->presence;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_speaking_reveal (double   value,
                     gpointer user_data)
{
  AwraAvatar *self = user_data;

  self->speaking_reveal = CLAMP (value, 0.0, 1.0);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_pulse (double   value,
           gpointer user_data)
{
  AwraAvatar *self = user_data;

  self->pulse = value;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static AwraAnimation *
new_spring (AwraAvatar             *self,
            double                  value,
            double                  damping,
            double                  stiffness,
            AwraAnimationTargetFunc func)
{
  AwraAnimation *animation =
    awra_spring_animation_new (GTK_WIDGET (self), value, value, damping, stiffness,
                               awra_callback_animation_target_new (func, self, NULL));

  awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (animation), 0.002);
  return animation;
}

static void
update_pulse (AwraAvatar *self)
{
  gboolean wanted = self->speaking && gtk_widget_get_mapped (GTK_WIDGET (self)) &&
                    !awra_widget_get_reduced_motion (GTK_WIDGET (self));

  if (wanted) {
    if (self->pulse_animation == NULL) {
      self->pulse_animation =
        awra_timed_animation_new (GTK_WIDGET (self), 0.0, 1.0, PULSE_DURATION,
                                  awra_callback_animation_target_new (set_pulse, self, NULL));
      awra_timed_animation_set_repeat_count (AWRA_TIMED_ANIMATION (self->pulse_animation), 0);
      awra_timed_animation_set_alternate (AWRA_TIMED_ANIMATION (self->pulse_animation), TRUE);
      awra_timed_animation_set_easing (AWRA_TIMED_ANIMATION (self->pulse_animation),
                                       AWRA_EASING_EASE_IN_OUT_SINE);
    }
    if (awra_animation_get_state (self->pulse_animation) != AWRA_ANIMATION_STATE_PLAYING)
      awra_animation_play (self->pulse_animation);
  } else if (self->pulse_animation != NULL) {
    awra_animation_reset (self->pulse_animation);
  }
}

static void
update_accessible (AwraAvatar *self)
{
  const char *presence = NULL;
  g_autofree char *description = NULL;

  switch (self->presence) {
    case AWRA_PRESENCE_ONLINE: presence = _("Online"); break;
    case AWRA_PRESENCE_IDLE: presence = _("Idle"); break;
    case AWRA_PRESENCE_BUSY: presence = _("Do not disturb"); break;
    case AWRA_PRESENCE_OFFLINE: presence = _("Offline"); break;
    case AWRA_PRESENCE_NONE:
    default: break;
  }
  if (presence != NULL && self->speaking)
    description = g_strdup_printf ("%s, %s", presence, _("Speaking"));
  else if (presence != NULL)
    description = g_strdup (presence);
  else if (self->speaking)
    description = g_strdup (_("Speaking"));
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  self->text != NULL ? self->text : _("Avatar"),
                                  -1);
  if (description != NULL)
    gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description, -1);
  else
    gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION);
}

/* Drawing */

static PangoLayout *
ensure_layout (AwraAvatar *self)
{
  PangoFontDescription *font;
  PangoContext *context;
  double scale;

  if (self->layout != NULL || self->initials == NULL)
    return self->layout;
  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), self->initials);
  context = pango_layout_get_context (self->layout);
  font = pango_font_description_copy (pango_context_get_font_description (context));
  scale = g_utf8_strlen (self->initials, -1) > 1 ? 0.38 : 0.44;
  pango_font_description_set_absolute_size (font, self->size * scale * PANGO_SCALE);
  pango_font_description_set_weight (font, PANGO_WEIGHT_SEMIBOLD);
  pango_layout_set_font_description (self->layout, font);
  pango_font_description_free (font);
  return self->layout;
}

static GdkPaintable *
ensure_icon (AwraAvatar *self)
{
  int px = MAX (8, (int) round (self->size * 0.56));

  if (self->icon != NULL && self->icon_px == px)
    return self->icon;
  g_clear_object (&self->icon);
  self->icon_px = px;
  self->icon = GDK_PAINTABLE (gtk_icon_theme_lookup_icon (
    gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (self))),
    self->icon_name != NULL ? self->icon_name : DEFAULT_ICON,
    NULL, px, gtk_widget_get_scale_factor (GTK_WIDGET (self)),
    gtk_widget_get_direction (GTK_WIDGET (self)), 0));
  return self->icon;
}

static void
draw_image (AwraAvatar   *self,
            GtkSnapshot  *snapshot,
            GdkPaintable *image)
{
  float size = self->size;
  double ratio = gdk_paintable_get_intrinsic_aspect_ratio (image);
  float width = size;
  float height = size;

  /* Cover: fill the square, crop the longer side around the centre. */
  if (ratio > 1.0)
    width = size * ratio;
  else if (ratio > 0.0 && ratio < 1.0)
    height = size / ratio;
  if (GDK_IS_TEXTURE (image)) {
    gtk_snapshot_append_scaled_texture (snapshot, GDK_TEXTURE (image),
                                        GSK_SCALING_FILTER_TRILINEAR,
                                        &GRAPHENE_RECT_INIT ((size - width) / 2,
                                                             (size - height) / 2,
                                                             width, height));
    return;
  }
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT ((size - width) / 2,
                                                          (size - height) / 2));
  gdk_paintable_snapshot (image, snapshot, width, height);
  gtk_snapshot_restore (snapshot);
}

static void
draw_face (AwraAvatar   *self,
           GtkSnapshot  *snapshot,
           GdkPaintable *image)
{
  float size = self->size;
  GdkRGBA background;
  GdkRGBA foreground;

  if (image != NULL) {
    draw_image (self, snapshot, image);
    return;
  }
  face_colors (self, &background, &foreground);
  gtk_snapshot_append_color (snapshot, &background, &GRAPHENE_RECT_INIT (0, 0, size, size));
  if (self->show_initials && ensure_layout (self) != NULL) {
    PangoRectangle ink;

    pango_layout_get_pixel_extents (self->layout, &ink, NULL);
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot,
                            &GRAPHENE_POINT_INIT (round ((size - ink.width) / 2.0 - ink.x),
                                                  round ((size - ink.height) / 2.0 - ink.y)));
    gtk_snapshot_append_layout (snapshot, self->layout, &foreground);
    gtk_snapshot_restore (snapshot);
  } else {
    GdkPaintable *icon = ensure_icon (self);
    float px = self->icon_px;

    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (round ((size - px) / 2),
                                                            round ((size - px) / 2)));
    gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (icon), snapshot,
                                              px, px, &foreground, 1);
    gtk_snapshot_restore (snapshot);
  }
}

static void
draw_speaking_ring (AwraAvatar  *self,
                    GtkSnapshot *snapshot)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (GTK_WIDGET (self));
  float size = self->size;
  float gap = MAX (1.5f, size * 0.05f);
  float width = MAX (2.0f, size * 0.055f);
  /* The ring emerges from the edge, then breathes by a pixel or so. */
  float spread = gap * self->speaking_reveal + width +
                 (float) (self->pulse * MAX (0.75f, size * 0.025f));
  float opacity = self->speaking_reveal * (0.72 + 0.28 * (1.0 - self->pulse));
  float radius = corner_radius (self);
  GdkRGBA color = awra_rgba_with_alpha (awra_token_set_get_accent (tokens), opacity);
  GskRoundedRect ring;
  float widths[4] = { width, width, width, width };
  GdkRGBA colors[4] = { color, color, color, color };

  if (self->speaking_reveal <= 0.0)
    return;
  gsk_rounded_rect_init_from_rect (&ring,
                                   &GRAPHENE_RECT_INIT (-spread, -spread,
                                                        size + 2 * spread, size + 2 * spread),
                                   radius + spread);
  gtk_snapshot_append_border (snapshot, &ring, widths, colors);
}

static void
draw_presence (AwraAvatar   *self,
               GtkSnapshot  *snapshot,
               AwraTokenSet *tokens)
{
  graphene_point_t center = dot_center (self);
  float radius = dot_diameter (self) / 2.0f * self->presence_scale;
  gboolean rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  const GdkRGBA *color;
  GdkRGBA black = { 0, 0, 0, 1 };

  switch (self->shown_presence) {
    case AWRA_PRESENCE_ONLINE:
      fill_circle (snapshot, center.x, center.y, radius, awra_token_set_get_success (tokens));
      return;
    case AWRA_PRESENCE_IDLE:
      color = awra_token_set_get_warning (tokens);
      break;
    case AWRA_PRESENCE_BUSY:
      color = awra_token_set_get_danger (tokens);
      break;
    case AWRA_PRESENCE_OFFLINE:
      color = awra_token_set_get_muted_foreground (tokens);
      break;
    case AWRA_PRESENCE_NONE:
    default:
      return;
  }
  /* Shapes besides colour: a crescent, a bar, a ring. */
  gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_ALPHA);
  switch (self->shown_presence) {
    case AWRA_PRESENCE_IDLE:
      fill_circle (snapshot, center.x + (rtl ? 0.42f : -0.42f) * radius,
                   center.y - 0.42f * radius, radius * 0.66f, &black);
      break;
    case AWRA_PRESENCE_BUSY:
      fill_rounded (snapshot, center.x - radius * 0.58f, center.y - radius * 0.2f,
                    radius * 1.16f, radius * 0.4f, radius * 0.2f, &black);
      break;
    case AWRA_PRESENCE_OFFLINE:
      fill_circle (snapshot, center.x, center.y, radius * 0.46f, &black);
      break;
    case AWRA_PRESENCE_NONE:
    case AWRA_PRESENCE_ONLINE:
    default:
      break;
  }
  gtk_snapshot_pop (snapshot);
  fill_circle (snapshot, center.x, center.y, radius, color);
  gtk_snapshot_pop (snapshot);
}

static void
awra_avatar_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  AwraAvatar *self = AWRA_AVATAR (widget);
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);
  float size = self->size;
  gboolean cutout = self->shown_presence != AWRA_PRESENCE_NONE && self->presence_scale > 0.0;
  GskRoundedRect face;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (round ((gtk_widget_get_width (widget) - size) / 2),
                                                round ((gtk_widget_get_height (widget) - size) / 2)));
  if (cutout) {
    graphene_point_t center = dot_center (self);
    float radius = (dot_diameter (self) / 2.0f + cutout_width (self)) * self->presence_scale;
    GdkRGBA black = { 0, 0, 0, 1 };

    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_ALPHA);
    fill_circle (snapshot, center.x, center.y, radius, &black);
    gtk_snapshot_pop (snapshot);
  }
  draw_speaking_ring (self, snapshot);
  gsk_rounded_rect_init_from_rect (&face, &GRAPHENE_RECT_INIT (0, 0, size, size),
                                   corner_radius (self));
  gtk_snapshot_push_rounded_clip (snapshot, &face);
  if (self->crossfading) {
    gtk_snapshot_push_cross_fade (snapshot, self->crossfade);
    draw_face (self, snapshot, self->previous_image);
    gtk_snapshot_pop (snapshot);
    draw_face (self, snapshot, self->image);
    gtk_snapshot_pop (snapshot);
  } else {
    draw_face (self, snapshot, self->image);
  }
  gtk_snapshot_pop (snapshot);
  if (self->image != NULL || awra_token_set_get_high_contrast (tokens)) {
    /* A hairline keeps a pale picture from dissolving into a pale canvas. */
    GdkRGBA line = awra_rgba_with_alpha (awra_token_set_get_foreground (tokens), 0.09);
    float widths[4] = { 1, 1, 1, 1 };
    GdkRGBA colors[4] = { line, line, line, line };

    gtk_snapshot_append_border (snapshot, &face, widths, colors);
  }
  if (cutout) {
    gtk_snapshot_pop (snapshot);
    draw_presence (self, snapshot, tokens);
  }
  gtk_snapshot_restore (snapshot);
}

static void
awra_avatar_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  (void) orientation;
  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  *minimum = *natural = AWRA_AVATAR (widget)->size;
}

static void
invalidate_text_caches (AwraAvatar *self)
{
  g_clear_object (&self->layout);
  g_clear_object (&self->icon);
}

static void
style_changed (gpointer user_data)
{
  AwraAvatar *self = user_data;

  update_pulse (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
awra_avatar_map (GtkWidget *widget)
{
  AwraAvatar *self = AWRA_AVATAR (widget);

  GTK_WIDGET_CLASS (awra_avatar_parent_class)->map (widget);
  awra_style_watch_start (&self->watch, widget, style_changed, self);
  update_pulse (self);
}

static void
awra_avatar_unmap (GtkWidget *widget)
{
  AwraAvatar *self = AWRA_AVATAR (widget);

  awra_style_watch_stop (&self->watch);
  GTK_WIDGET_CLASS (awra_avatar_parent_class)->unmap (widget);
  /* Animations jump to their end on unmap; the pulse rests. */
  if (self->pulse_animation != NULL)
    awra_animation_reset (self->pulse_animation);
}

static void
awra_avatar_system_setting_changed (GtkWidget        *widget,
                                    GtkSystemSetting  setting)
{
  invalidate_text_caches (AWRA_AVATAR (widget));
  GTK_WIDGET_CLASS (awra_avatar_parent_class)->system_setting_changed (widget, setting);
}

static void
awra_avatar_direction_changed (GtkWidget        *widget,
                               GtkTextDirection  previous)
{
  invalidate_text_caches (AWRA_AVATAR (widget));
  GTK_WIDGET_CLASS (awra_avatar_parent_class)->direction_changed (widget, previous);
}

static void
image_invalidated_cb (AwraAvatar *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
disconnect_image (AwraAvatar   *self,
                  GdkPaintable *image)
{
  if (image != NULL)
    g_signal_handlers_disconnect_by_func (image, image_invalidated_cb, self);
}

static void
clear_animation (AwraAnimation **animation)
{
  if (*animation == NULL)
    return;
  /* A playing animation outlives this reference; finish it now so its
   * target never reaches a disposed avatar. */
  awra_animation_skip (*animation);
  g_clear_object (animation);
}

static void
awra_avatar_dispose (GObject *object)
{
  AwraAvatar *self = AWRA_AVATAR (object);

  awra_style_watch_stop (&self->watch);
  clear_animation (&self->crossfade_animation);
  clear_animation (&self->presence_animation);
  clear_animation (&self->speaking_animation);
  if (self->pulse_animation != NULL)
    awra_animation_reset (self->pulse_animation);
  g_clear_object (&self->pulse_animation);
  disconnect_image (self, self->image);
  g_clear_object (&self->image);
  g_clear_object (&self->previous_image);
  invalidate_text_caches (self);
  G_OBJECT_CLASS (awra_avatar_parent_class)->dispose (object);
}

static void
awra_avatar_finalize (GObject *object)
{
  AwraAvatar *self = AWRA_AVATAR (object);

  g_free (self->text);
  g_free (self->initials);
  g_free (self->icon_name);
  G_OBJECT_CLASS (awra_avatar_parent_class)->finalize (object);
}

static void
awra_avatar_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  AwraAvatar *self = AWRA_AVATAR (object);

  switch (prop_id) {
    case PROP_SIZE: g_value_set_int (value, self->size); break;
    case PROP_TEXT: g_value_set_string (value, self->text); break;
    case PROP_SHOW_INITIALS: g_value_set_boolean (value, self->show_initials); break;
    case PROP_CUSTOM_IMAGE: g_value_set_object (value, self->image); break;
    case PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
    case PROP_SHAPE: g_value_set_enum (value, self->shape); break;
    case PROP_PRESENCE: g_value_set_enum (value, self->presence); break;
    case PROP_SPEAKING: g_value_set_boolean (value, self->speaking); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_avatar_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  AwraAvatar *self = AWRA_AVATAR (object);

  switch (prop_id) {
    case PROP_SIZE: awra_avatar_set_size (self, g_value_get_int (value)); break;
    case PROP_TEXT: awra_avatar_set_text (self, g_value_get_string (value)); break;
    case PROP_SHOW_INITIALS: awra_avatar_set_show_initials (self, g_value_get_boolean (value)); break;
    case PROP_CUSTOM_IMAGE: awra_avatar_set_custom_image (self, g_value_get_object (value)); break;
    case PROP_ICON_NAME: awra_avatar_set_icon_name (self, g_value_get_string (value)); break;
    case PROP_SHAPE: awra_avatar_set_shape (self, g_value_get_enum (value)); break;
    case PROP_PRESENCE: awra_avatar_set_presence (self, g_value_get_enum (value)); break;
    case PROP_SPEAKING: awra_avatar_set_speaking (self, g_value_get_boolean (value)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_avatar_class_init (AwraAvatarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->dispose = awra_avatar_dispose;
  object_class->finalize = awra_avatar_finalize;
  object_class->get_property = awra_avatar_get_property;
  object_class->set_property = awra_avatar_set_property;
  widget_class->snapshot = awra_avatar_snapshot;
  widget_class->measure = awra_avatar_measure;
  widget_class->map = awra_avatar_map;
  widget_class->unmap = awra_avatar_unmap;
  widget_class->system_setting_changed = awra_avatar_system_setting_changed;
  widget_class->direction_changed = awra_avatar_direction_changed;

  /**
   * AwraAvatar:size:
   *
   * The diameter, in logical pixels.
   *
   * Since: 1.1
   */
  properties[PROP_SIZE] =
    g_param_spec_int ("size", NULL, NULL, MIN_SIZE, G_MAXINT, DEFAULT_SIZE, flags);
  /**
   * AwraAvatar:text:
   *
   * The name of the person or place: initials, colour and accessible name.
   *
   * Since: 1.1
   */
  properties[PROP_TEXT] = g_param_spec_string ("text", NULL, NULL, NULL, flags);
  /**
   * AwraAvatar:show-initials:
   *
   * Whether initials are shown when there is no image.
   *
   * Since: 1.1
   */
  properties[PROP_SHOW_INITIALS] =
    g_param_spec_boolean ("show-initials", NULL, NULL, TRUE, flags);
  /**
   * AwraAvatar:custom-image:
   *
   * The picture, cropped to the shape of the avatar.
   *
   * Since: 1.1
   */
  properties[PROP_CUSTOM_IMAGE] =
    g_param_spec_object ("custom-image", NULL, NULL, GDK_TYPE_PAINTABLE, flags);
  /**
   * AwraAvatar:icon-name:
   *
   * The icon shown when there is neither image nor initials.
   *
   * Since: 1.1
   */
  properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL, NULL, flags);
  /**
   * AwraAvatar:shape:
   *
   * The outline of the avatar.
   *
   * Since: 1.1
   */
  properties[PROP_SHAPE] =
    g_param_spec_enum ("shape", NULL, NULL, AWRA_TYPE_AVATAR_SHAPE,
                       AWRA_AVATAR_SHAPE_CIRCLE, flags);
  /**
   * AwraAvatar:presence:
   *
   * The presence shown at the bottom end corner.
   *
   * Since: 1.1
   */
  properties[PROP_PRESENCE] =
    g_param_spec_enum ("presence", NULL, NULL, AWRA_TYPE_PRESENCE, AWRA_PRESENCE_NONE, flags);
  /**
   * AwraAvatar:speaking:
   *
   * Whether the speaking ring is shown.
   *
   * Since: 1.1
   */
  properties[PROP_SPEAKING] = g_param_spec_boolean ("speaking", NULL, NULL, FALSE, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "awraavatar");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_IMG);
}

static void
awra_avatar_init (AwraAvatar *self)
{
  self->size = DEFAULT_SIZE;
  self->show_initials = TRUE;
  self->crossfade = 1.0;
  g_signal_connect (self, "notify::scale-factor", G_CALLBACK (invalidate_text_caches), NULL);
  update_accessible (self);
}

GtkWidget *
awra_avatar_new (int         size,
                 const char *text)
{
  g_return_val_if_fail (size >= MIN_SIZE, NULL);
  return g_object_new (AWRA_TYPE_AVATAR, "size", size, "text", text, NULL);
}

int
awra_avatar_get_size (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), DEFAULT_SIZE);
  return self->size;
}

void
awra_avatar_set_size (AwraAvatar *self,
                      int         size)
{
  g_return_if_fail (AWRA_IS_AVATAR (self));
  g_return_if_fail (size >= MIN_SIZE);
  if (self->size == size)
    return;
  self->size = size;
  invalidate_text_caches (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIZE]);
}

const char *
awra_avatar_get_text (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), NULL);
  return self->text;
}

void
awra_avatar_set_text (AwraAvatar *self,
                      const char *text)
{
  g_return_if_fail (AWRA_IS_AVATAR (self));
  if (!g_set_str (&self->text, text))
    return;
  g_free (self->initials);
  self->initials = compute_initials (text);
  self->hue = text != NULL ? hue_for_text (text) : 0;
  g_clear_object (&self->layout);
  update_accessible (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

gboolean
awra_avatar_get_show_initials (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), TRUE);
  return self->show_initials;
}

void
awra_avatar_set_show_initials (AwraAvatar *self,
                               gboolean    show_initials)
{
  g_return_if_fail (AWRA_IS_AVATAR (self));
  show_initials = !!show_initials;
  if (self->show_initials == show_initials)
    return;
  self->show_initials = show_initials;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_INITIALS]);
}

GdkPaintable *
awra_avatar_get_custom_image (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), NULL);
  return self->image;
}

void
awra_avatar_set_custom_image (AwraAvatar   *self,
                              GdkPaintable *image)
{
  g_return_if_fail (AWRA_IS_AVATAR (self));
  g_return_if_fail (image == NULL || GDK_IS_PAINTABLE (image));
  if (self->image == image)
    return;
  /* The face on screen, mid-fade or not, fades out under the new one. */
  if (!self->crossfading || self->crossfade >= 0.5)
    g_set_object (&self->previous_image, self->image);
  disconnect_image (self, self->image);
  g_set_object (&self->image, image);
  if (image != NULL) {
    g_signal_connect_swapped (image, "invalidate-contents",
                              G_CALLBACK (image_invalidated_cb), self);
    g_signal_connect_swapped (image, "invalidate-size",
                              G_CALLBACK (image_invalidated_cb), self);
  }
  if (gtk_widget_get_mapped (GTK_WIDGET (self))) {
    if (self->crossfade_animation == NULL)
      self->crossfade_animation =
        awra_timed_animation_new_for_preset (GTK_WIDGET (self), 0.0, 1.0,
                                             AWRA_MOTION_PRESET_NORMAL,
                                             awra_callback_animation_target_new (set_crossfade,
                                                                                 self, NULL));
    self->crossfading = TRUE;
    awra_animation_play (self->crossfade_animation);
  } else {
    g_clear_object (&self->previous_image);
  }
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_IMAGE]);
}

const char *
awra_avatar_get_icon_name (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), NULL);
  return self->icon_name;
}

void
awra_avatar_set_icon_name (AwraAvatar *self,
                           const char *icon_name)
{
  g_return_if_fail (AWRA_IS_AVATAR (self));
  if (!g_set_str (&self->icon_name, icon_name))
    return;
  g_clear_object (&self->icon);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

AwraAvatarShape
awra_avatar_get_shape (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), AWRA_AVATAR_SHAPE_CIRCLE);
  return self->shape;
}

void
awra_avatar_set_shape (AwraAvatar      *self,
                       AwraAvatarShape  shape)
{
  g_return_if_fail (AWRA_IS_AVATAR (self));
  g_return_if_fail (shape == AWRA_AVATAR_SHAPE_CIRCLE || shape == AWRA_AVATAR_SHAPE_ROUNDED);
  if (self->shape == shape)
    return;
  self->shape = shape;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHAPE]);
}

AwraPresence
awra_avatar_get_presence (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), AWRA_PRESENCE_NONE);
  return self->presence;
}

void
awra_avatar_set_presence (AwraAvatar   *self,
                          AwraPresence  presence)
{
  AwraSpringAnimation *spring;

  g_return_if_fail (AWRA_IS_AVATAR (self));
  g_return_if_fail (presence >= AWRA_PRESENCE_NONE && presence <= AWRA_PRESENCE_OFFLINE);
  if (self->presence == presence)
    return;
  self->presence = presence;
  if (self->presence_animation == NULL)
    self->presence_animation = new_spring (self, self->presence_scale, 0.55, 520.0,
                                           set_presence_scale);
  spring = AWRA_SPRING_ANIMATION (self->presence_animation);
  if (presence == AWRA_PRESENCE_NONE) {
    /* Shrink away; the dot keeps its last shape while it goes. */
    awra_spring_animation_set_clamp (spring, TRUE);
    awra_spring_animation_retarget (spring, 0.0);
  } else {
    gboolean replacing = self->shown_presence != AWRA_PRESENCE_NONE &&
                         self->presence_scale > 0.0;

    self->shown_presence = presence;
    awra_spring_animation_set_clamp (spring, FALSE);
    if (replacing && awra_animation_get_state (self->presence_animation) !=
                       AWRA_ANIMATION_STATE_PLAYING) {
      /* A new state gives a small nod rather than a jump cut. */
      awra_spring_animation_set_value_from (spring, 0.72);
      awra_spring_animation_set_value_to (spring, 1.0);
      awra_spring_animation_set_initial_velocity (spring, 0.0);
      awra_animation_play (self->presence_animation);
    } else {
      awra_spring_animation_retarget (spring, 1.0);
    }
  }
  update_accessible (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRESENCE]);
}

gboolean
awra_avatar_get_speaking (AwraAvatar *self)
{
  g_return_val_if_fail (AWRA_IS_AVATAR (self), FALSE);
  return self->speaking;
}

void
awra_avatar_set_speaking (AwraAvatar *self,
                          gboolean    speaking)
{
  g_return_if_fail (AWRA_IS_AVATAR (self));
  speaking = !!speaking;
  if (self->speaking == speaking)
    return;
  self->speaking = speaking;
  if (self->speaking_animation == NULL) {
    self->speaking_animation = new_spring (self, self->speaking_reveal, 1.0, 340.0,
                                           set_speaking_reveal);
    awra_spring_animation_set_clamp (AWRA_SPRING_ANIMATION (self->speaking_animation), TRUE);
  }
  awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (self->speaking_animation),
                                  speaking ? 1.0 : 0.0);
  update_pulse (self);
  update_accessible (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SPEAKING]);
}
