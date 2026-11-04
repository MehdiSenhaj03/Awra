/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-token-set.h>

#include "tokens/awra-token-set-private.h"

struct _AwraTokenSet {
  GObject parent_instance;
  gboolean dark;
  GdkRGBA background;
  GdkRGBA foreground;
  GdkRGBA accent;
  GdkRGBA accent_content;
  GdkRGBA border;
  GdkRGBA muted_foreground;
  GdkRGBA control_background;
  GdkRGBA control_hover;
  GdkRGBA control_pressed;
  GdkRGBA control_well;
  GdkRGBA control_cap;
  GdkRGBA control_cap_hover;
  GdkRGBA control_thumb;
  GdkRGBA control_track;
  GdkRGBA selection;
  GdkRGBA focus_ring;
  GdkRGBA disabled_foreground;
  GdkRGBA on_accent;
  GdkRGBA danger;
  GdkRGBA success;
  GdkRGBA warning;
  GdkRGBA accent_soft;
  GdkRGBA surface_canvas;
  GdkRGBA surface_content;
  GdkRGBA surface_chrome;
  GdkRGBA surface_layer;
  GdkRGBA surface_floating;
  GdkRGBA surface_opaque;
  GdkRGBA outline;
  GdkRGBA separator;
  GdkRGBA rim_highlight;
  double window_opacity;
  double surface_opacity;
  double radius;
  double border_width;
  double spacing[6];
  double radii[5];
  double control_height;
  double navigation_height;
  double header_height;
  double grain_scale;
  GdkRGBA material_tints[AWRA_MATERIAL_PROFILE_COUNT];
  double material_radii[AWRA_MATERIAL_PROFILE_COUNT];
  double material_grain_scales[AWRA_MATERIAL_PROFILE_COUNT];
  double material_outline_strengths[AWRA_MATERIAL_PROFILE_COUNT];
  double material_highlight_strengths[AWRA_MATERIAL_PROFILE_COUNT];
  gboolean material_blur_enabled[AWRA_MATERIAL_PROFILE_COUNT];
  gboolean high_contrast;
};

enum {
  PROP_0,
  PROP_DARK,
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  PROP_ACCENT,
  PROP_BORDER,
  PROP_MUTED_FOREGROUND,
  PROP_CONTROL_BACKGROUND,
  PROP_CONTROL_HOVER,
  PROP_CONTROL_PRESSED,
  PROP_SELECTION,
  PROP_FOCUS_RING,
  PROP_DISABLED_FOREGROUND,
  PROP_WINDOW_OPACITY,
  PROP_SURFACE_OPACITY,
  PROP_RADIUS,
  PROP_BORDER_WIDTH,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraTokenSet, awra_token_set, G_TYPE_OBJECT)

static void
awra_token_set_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AwraTokenSet *self = AWRA_TOKEN_SET (object);

  switch (property_id) {
  case PROP_DARK:
    g_value_set_boolean (value, self->dark);
    break;
  case PROP_BACKGROUND:
    g_value_set_boxed (value, &self->background);
    break;
  case PROP_FOREGROUND:
    g_value_set_boxed (value, &self->foreground);
    break;
  case PROP_ACCENT:
    g_value_set_boxed (value, &self->accent);
    break;
  case PROP_BORDER:
    g_value_set_boxed (value, &self->border);
    break;
  case PROP_MUTED_FOREGROUND:
    g_value_set_boxed (value, &self->muted_foreground);
    break;
  case PROP_CONTROL_BACKGROUND:
    g_value_set_boxed (value, &self->control_background);
    break;
  case PROP_CONTROL_HOVER:
    g_value_set_boxed (value, &self->control_hover);
    break;
  case PROP_CONTROL_PRESSED:
    g_value_set_boxed (value, &self->control_pressed);
    break;
  case PROP_SELECTION:
    g_value_set_boxed (value, &self->selection);
    break;
  case PROP_FOCUS_RING:
    g_value_set_boxed (value, &self->focus_ring);
    break;
  case PROP_DISABLED_FOREGROUND:
    g_value_set_boxed (value, &self->disabled_foreground);
    break;
  case PROP_WINDOW_OPACITY:
    g_value_set_double (value, self->window_opacity);
    break;
  case PROP_SURFACE_OPACITY:
    g_value_set_double (value, self->surface_opacity);
    break;
  case PROP_RADIUS:
    g_value_set_double (value, self->radius);
    break;
  case PROP_BORDER_WIDTH:
    g_value_set_double (value, self->border_width);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
awra_token_set_class_init (AwraTokenSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = awra_token_set_get_property;

  properties[PROP_DARK] =
    g_param_spec_boolean ("dark", NULL, NULL, FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_BACKGROUND] =
    g_param_spec_boxed ("background", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_FOREGROUND] =
    g_param_spec_boxed ("foreground", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACCENT] =
    g_param_spec_boxed ("accent", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_BORDER] =
    g_param_spec_boxed ("border", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_MUTED_FOREGROUND] =
    g_param_spec_boxed ("muted-foreground", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_CONTROL_BACKGROUND] =
    g_param_spec_boxed ("control-background", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_CONTROL_HOVER] =
    g_param_spec_boxed ("control-hover", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_CONTROL_PRESSED] =
    g_param_spec_boxed ("control-pressed", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_SELECTION] =
    g_param_spec_boxed ("selection", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_FOCUS_RING] =
    g_param_spec_boxed ("focus-ring", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_DISABLED_FOREGROUND] =
    g_param_spec_boxed ("disabled-foreground", NULL, NULL, GDK_TYPE_RGBA,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_WINDOW_OPACITY] =
    g_param_spec_double ("window-opacity", NULL, NULL, 0.0, 1.0, 1.0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_SURFACE_OPACITY] =
    g_param_spec_double ("surface-opacity", NULL, NULL, 0.0, 1.0, 1.0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_RADIUS] =
    g_param_spec_double ("radius", NULL, NULL, 0.0, 128.0, 16.0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_BORDER_WIDTH] =
    g_param_spec_double ("border-width", NULL, NULL, 0.0, 8.0, 1.0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
awra_token_set_init (AwraTokenSet *self)
{
  (void) self;
}

AwraTokenSet *
awra_token_set_new (gboolean       dark,
                    const GdkRGBA *accent,
                    gboolean       high_contrast)
{
  return awra_token_set_new_with_tuning (dark, accent, high_contrast,
                                         NULL, AWRA_DEFAULT_GRAIN_SCALE);
}

AwraTokenSet *
awra_token_set_new_with_tuning (gboolean       dark,
                                const GdkRGBA *accent,
                                gboolean       high_contrast,
                                const GdkRGBA *canvas_tint,
                                double         grain_scale)
{
  static const GdkRGBA default_accent = { 0.49, 0.31, 0.95, 1.0 };
  AwraTokenSet *self = g_object_new (AWRA_TYPE_TOKEN_SET, NULL);

  self->dark = dark;
  self->high_contrast = high_contrast;
  self->accent = accent != NULL ? *accent : default_accent;
  self->spacing[AWRA_SPACING_XS] = 4.0;
  self->spacing[AWRA_SPACING_SM] = 8.0;
  self->spacing[AWRA_SPACING_MD] = 12.0;
  self->spacing[AWRA_SPACING_LG] = 16.0;
  self->spacing[AWRA_SPACING_XL] = 24.0;
  self->spacing[AWRA_SPACING_XXL] = 32.0;
  self->radii[AWRA_RADIUS_SMALL] = 7.0;
  self->radii[AWRA_RADIUS_MEDIUM] = 11.0;
  self->radii[AWRA_RADIUS_LARGE] = 17.0;
  self->radii[AWRA_RADIUS_WINDOW] = 24.0;
  self->radii[AWRA_RADIUS_PILL] = 999.0;
  self->radius = self->radii[AWRA_RADIUS_LARGE];
  self->border_width = high_contrast ? 2.0 : 1.0;
  self->window_opacity = dark ? 0.60 : 0.88;
  self->surface_opacity = dark ? 0.58 : 0.90;
  self->control_height = 34.0;
  self->navigation_height = 40.0;
  self->header_height = 58.0;
  self->grain_scale = CLAMP (grain_scale, 0.0, 4.0);

  if (dark) {
    self->background = (GdkRGBA) {
      35.0 / 255.0, 35.0 / 255.0, 35.0 / 255.0, 1.0
    };
    self->foreground = (GdkRGBA) { 0.945, 0.945, 0.958, 1.0 };
    self->muted_foreground = (GdkRGBA) { 0.68, 0.68, 0.71, 1.0 };
    self->surface_canvas = (GdkRGBA) {
      35.0 / 255.0, 35.0 / 255.0, 35.0 / 255.0, 0.60
    };
    self->surface_content = (GdkRGBA) { 0.14, 0.14, 0.15, 0.0 };
    self->surface_chrome = (GdkRGBA) { 0.035, 0.040, 0.050, 0.18 };
    self->surface_layer = (GdkRGBA) { 0.92, 0.92, 0.96, 0.045 };
    self->surface_floating = (GdkRGBA) {
      37.0 / 255.0, 37.0 / 255.0, 37.0 / 255.0, 0.60
    };
    self->surface_opaque = (GdkRGBA) {
      35.0 / 255.0, 35.0 / 255.0, 35.0 / 255.0, 1.0
    };
    self->outline = (GdkRGBA) { 0.96, 0.96, 0.99,
                                high_contrast ? 0.72 : 0.10 };
    self->separator = (GdkRGBA) { 0.96, 0.96, 0.99,
                                  high_contrast ? 0.62 : 0.055 };
    self->control_well = (GdkRGBA) { 0.0, 0.0, 0.0, 0.28 };
    self->control_cap = (GdkRGBA) { 1.0, 1.0, 1.0, 0.060 };
    self->control_cap_hover = (GdkRGBA) { 1.0, 1.0, 1.0, 0.10 };
    self->control_thumb = (GdkRGBA) { 0.92, 0.92, 0.94, 1.0 };
    self->control_track = (GdkRGBA) { 1.0, 1.0, 1.0, 0.115 };
    self->rim_highlight = (GdkRGBA) { 1.0, 1.0, 1.0,
                                      high_contrast ? 0.70 : 0.14 };
  } else {
    self->background = (GdkRGBA) { 0.95, 0.945, 0.96, 1.0 };
    self->foreground = (GdkRGBA) { 0.08, 0.07, 0.10, 1.0 };
    self->muted_foreground = (GdkRGBA) { 0.31, 0.29, 0.35, 1.0 };
    self->surface_canvas = (GdkRGBA) { 0.95, 0.945, 0.96, 0.89 };
    self->surface_content = (GdkRGBA) { 0.95, 0.945, 0.96, 0.0 };
    self->surface_chrome = (GdkRGBA) { 0.88, 0.87, 0.90, 0.22 };
    self->surface_layer = (GdkRGBA) { 1.0, 0.995, 1.0, 0.40 };
    self->surface_floating = (GdkRGBA) { 0.98, 0.975, 0.985, 0.94 };
    self->surface_opaque = (GdkRGBA) { 0.95, 0.945, 0.96, 1.0 };
    self->outline = (GdkRGBA) { 0.13, 0.10, 0.18,
                                high_contrast ? 0.82 : 0.18 };
    self->separator = (GdkRGBA) { 0.13, 0.10, 0.18,
                                  high_contrast ? 0.72 : 0.12 };
    self->control_well = (GdkRGBA) { 0.08, 0.065, 0.10, 0.105 };
    self->control_cap = (GdkRGBA) { 1.0, 1.0, 1.0, 0.72 };
    self->control_cap_hover = (GdkRGBA) { 1.0, 1.0, 1.0, 0.92 };
    self->control_thumb = (GdkRGBA) { 0.985, 0.98, 0.99, 1.0 };
    self->control_track = (GdkRGBA) { 0.10, 0.08, 0.14, 0.20 };
    self->rim_highlight = (GdkRGBA) { 1.0, 1.0, 1.0,
                                      high_contrast ? 0.95 : 0.68 };
  }
  if (canvas_tint != NULL) {
    self->surface_canvas = *canvas_tint;
    self->window_opacity = canvas_tint->alpha;
  }
  self->control_background = self->control_cap;
  self->control_hover = self->control_cap_hover;
  self->border = self->outline;
  self->control_pressed = dark
    ? (GdkRGBA) { 1.0, 1.0, 1.0, 0.135 }
    : (GdkRGBA) { 0.08, 0.06, 0.10, 0.14 };
  self->selection = (GdkRGBA) {
    self->accent.red, self->accent.green, self->accent.blue,
    dark ? 0.13 : 0.14
  };
  self->accent_soft = self->selection;
  self->focus_ring = (GdkRGBA) {
    self->accent.red, self->accent.green, self->accent.blue,
    high_contrast ? 1.0 : 0.88
  };
  self->disabled_foreground = self->foreground;
  self->disabled_foreground.alpha = high_contrast ? 0.68 : 0.56;
  self->accent_content = self->accent;
  if (dark) {
    self->accent_content.red += (1.0 - self->accent_content.red) * 0.20;
    self->accent_content.green += (1.0 - self->accent_content.green) * 0.20;
    self->accent_content.blue += (1.0 - self->accent_content.blue) * 0.20;
  } else {
    self->accent_content.red *= 0.72;
    self->accent_content.green *= 0.72;
    self->accent_content.blue *= 0.72;
  }
  self->on_accent = (0.2126 * self->accent.red +
                     0.7152 * self->accent.green +
                     0.0722 * self->accent.blue) > 0.60
                      ? (GdkRGBA) { 0.04, 0.035, 0.05, 1.0 }
                      : (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
  self->danger = (GdkRGBA) { 0.88, 0.22, 0.28, 1.0 };
  /* Strong enough to read as a small dot on either canvas. */
  self->success = dark ? (GdkRGBA) { 0.27, 0.76, 0.47, 1.0 }
                       : (GdkRGBA) { 0.12, 0.58, 0.31, 1.0 };
  self->warning = dark ? (GdkRGBA) { 0.96, 0.71, 0.26, 1.0 }
                       : (GdkRGBA) { 0.84, 0.54, 0.06, 1.0 };

  self->material_tints[AWRA_MATERIAL_PROFILE_SOLID] = self->background;
  self->material_tints[AWRA_MATERIAL_PROFILE_SOLID].alpha = 1.0;
  self->material_tints[AWRA_MATERIAL_PROFILE_TRANSLUCENT] = self->background;
  self->material_tints[AWRA_MATERIAL_PROFILE_TRANSLUCENT].alpha =
    self->surface_opacity;
  self->material_tints[AWRA_MATERIAL_PROFILE_FROSTED] = self->background;
  self->material_tints[AWRA_MATERIAL_PROFILE_FROSTED].alpha =
    self->surface_opacity;
  self->material_tints[AWRA_MATERIAL_PROFILE_CANVAS] = self->surface_canvas;
  self->material_tints[AWRA_MATERIAL_PROFILE_CONTENT] = self->surface_content;
  self->material_tints[AWRA_MATERIAL_PROFILE_CHROME] = self->surface_chrome;
  self->material_tints[AWRA_MATERIAL_PROFILE_LAYER] = self->surface_layer;
  self->material_tints[AWRA_MATERIAL_PROFILE_FLOATING] =
    self->surface_floating;
  self->material_tints[AWRA_MATERIAL_PROFILE_OPAQUE] = self->surface_opaque;
  for (guint i = 0; i < AWRA_MATERIAL_PROFILE_COUNT; i++) {
    self->material_radii[i] = self->radii[AWRA_RADIUS_LARGE];
    self->material_grain_scales[i] = 0.0;
    self->material_outline_strengths[i] = 1.0;
    self->material_highlight_strengths[i] = 1.0;
    self->material_blur_enabled[i] = FALSE;
  }
  self->material_radii[AWRA_MATERIAL_PROFILE_CANVAS] =
    self->radii[AWRA_RADIUS_WINDOW];
  self->material_radii[AWRA_MATERIAL_PROFILE_CONTENT] = 0.0;
  self->material_radii[AWRA_MATERIAL_PROFILE_CHROME] =
    self->radii[AWRA_RADIUS_LARGE];
  self->material_grain_scales[AWRA_MATERIAL_PROFILE_FROSTED] =
    self->grain_scale;
  self->material_grain_scales[AWRA_MATERIAL_PROFILE_CANVAS] =
    self->grain_scale;
  self->material_grain_scales[AWRA_MATERIAL_PROFILE_FLOATING] =
    self->grain_scale;
  self->material_blur_enabled[AWRA_MATERIAL_PROFILE_FROSTED] = TRUE;
  self->material_blur_enabled[AWRA_MATERIAL_PROFILE_CANVAS] = TRUE;
  self->material_blur_enabled[AWRA_MATERIAL_PROFILE_FLOATING] = TRUE;

  return self;
}

AwraMaterialProfileId
awra_material_profile_id_from_material (AwraMaterial *material)
{
  AwraMaterialPreset preset;

  g_return_val_if_fail (AWRA_IS_MATERIAL (material),
                        AWRA_MATERIAL_PROFILE_SOLID);
  preset = awra_material_get_preset (material);
  if (preset != AWRA_MATERIAL_PRESET_NONE)
    return (AwraMaterialProfileId) (AWRA_MATERIAL_PROFILE_CANVAS +
                                    preset - AWRA_MATERIAL_PRESET_CANVAS);
  switch (awra_material_get_kind (material)) {
  case AWRA_MATERIAL_KIND_TRANSLUCENT:
    return AWRA_MATERIAL_PROFILE_TRANSLUCENT;
  case AWRA_MATERIAL_KIND_FROSTED:
    return AWRA_MATERIAL_PROFILE_FROSTED;
  case AWRA_MATERIAL_KIND_SOLID:
  case AWRA_MATERIAL_KIND_LIQUID:
  default:
    return AWRA_MATERIAL_PROFILE_SOLID;
  }
}

static void
sync_semantic_tint (AwraTokenSet         *self,
                    AwraMaterialProfileId profile)
{
  switch (profile) {
  case AWRA_MATERIAL_PROFILE_CANVAS:
    self->surface_canvas = self->material_tints[profile];
    self->window_opacity = self->surface_canvas.alpha;
    break;
  case AWRA_MATERIAL_PROFILE_CONTENT:
    self->surface_content = self->material_tints[profile];
    break;
  case AWRA_MATERIAL_PROFILE_CHROME:
    self->surface_chrome = self->material_tints[profile];
    break;
  case AWRA_MATERIAL_PROFILE_LAYER:
    self->surface_layer = self->material_tints[profile];
    break;
  case AWRA_MATERIAL_PROFILE_FLOATING:
    self->surface_floating = self->material_tints[profile];
    break;
  case AWRA_MATERIAL_PROFILE_OPAQUE:
    self->surface_opaque = self->material_tints[profile];
    break;
  case AWRA_MATERIAL_PROFILE_SOLID:
  case AWRA_MATERIAL_PROFILE_TRANSLUCENT:
  case AWRA_MATERIAL_PROFILE_FROSTED:
  case AWRA_MATERIAL_PROFILE_COUNT:
  default:
    break;
  }
}

void
awra_token_set_apply_material_profile_override (
  AwraTokenSet                      *self,
  AwraMaterialProfileId              profile,
  const AwraMaterialProfileOverride *override)
{
  g_return_if_fail (AWRA_IS_TOKEN_SET (self));
  g_return_if_fail (profile >= AWRA_MATERIAL_PROFILE_SOLID &&
                    profile < AWRA_MATERIAL_PROFILE_COUNT);
  g_return_if_fail (override != NULL);
  if (override->tint_set) {
    self->material_tints[profile] = override->tint;
    sync_semantic_tint (self, profile);
  }
  if (override->radius_set)
    self->material_radii[profile] = override->radius;
  if (override->grain_scale_set)
    self->material_grain_scales[profile] = override->grain_scale;
  if (override->outline_strength_set)
    self->material_outline_strengths[profile] = override->outline_strength;
  if (override->highlight_strength_set)
    self->material_highlight_strengths[profile] =
      override->highlight_strength;
  if (override->blur_enabled_set)
    self->material_blur_enabled[profile] = override->blur_enabled;
}

const GdkRGBA *
awra_token_set_get_material_tint_internal (AwraTokenSet *self,
                                           AwraMaterial *material)
{
  g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), NULL);
  g_return_val_if_fail (AWRA_IS_MATERIAL (material), NULL);
  return &self->material_tints[
    awra_material_profile_id_from_material (material)];
}

#define DEFINE_MATERIAL_PROFILE_GETTER(name, field, fallback) \
  double \
  awra_token_set_get_material_##name##_internal (AwraTokenSet *self, \
                                                  AwraMaterial *material) \
  { \
    g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), fallback); \
    g_return_val_if_fail (AWRA_IS_MATERIAL (material), fallback); \
    return self->field[awra_material_profile_id_from_material (material)]; \
  }

DEFINE_MATERIAL_PROFILE_GETTER (radius, material_radii, 0.0)
DEFINE_MATERIAL_PROFILE_GETTER (grain_scale, material_grain_scales, 0.0)
DEFINE_MATERIAL_PROFILE_GETTER (outline_strength,
                                material_outline_strengths, 1.0)
DEFINE_MATERIAL_PROFILE_GETTER (highlight_strength,
                                material_highlight_strengths, 1.0)

gboolean
awra_token_set_get_material_blur_enabled_internal (AwraTokenSet *self,
                                                   AwraMaterial *material)
{
  g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), FALSE);
  g_return_val_if_fail (AWRA_IS_MATERIAL (material), FALSE);
  return self->material_blur_enabled[
    awra_material_profile_id_from_material (material)];
}

gboolean
awra_token_set_get_dark (AwraTokenSet *self)
{
  g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), FALSE);
  return self->dark;
}

#define DEFINE_COLOR_GETTER(name, field) \
  const GdkRGBA * \
  awra_token_set_get_##name (AwraTokenSet *self) \
  { \
    g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), NULL); \
    return &self->field; \
  }

DEFINE_COLOR_GETTER (background, background)
DEFINE_COLOR_GETTER (foreground, foreground)
DEFINE_COLOR_GETTER (accent, accent)
DEFINE_COLOR_GETTER (accent_content, accent_content)
DEFINE_COLOR_GETTER (border, border)
DEFINE_COLOR_GETTER (muted_foreground, muted_foreground)
DEFINE_COLOR_GETTER (control_background, control_background)
DEFINE_COLOR_GETTER (control_hover, control_hover)
DEFINE_COLOR_GETTER (control_pressed, control_pressed)
DEFINE_COLOR_GETTER (control_well, control_well)
DEFINE_COLOR_GETTER (control_cap, control_cap)
DEFINE_COLOR_GETTER (control_cap_hover, control_cap_hover)
DEFINE_COLOR_GETTER (control_thumb, control_thumb)
DEFINE_COLOR_GETTER (control_track, control_track)
DEFINE_COLOR_GETTER (selection, selection)
DEFINE_COLOR_GETTER (focus_ring, focus_ring)
DEFINE_COLOR_GETTER (disabled_foreground, disabled_foreground)
DEFINE_COLOR_GETTER (on_accent, on_accent)
DEFINE_COLOR_GETTER (danger, danger)
DEFINE_COLOR_GETTER (success, success)
DEFINE_COLOR_GETTER (warning, warning)
DEFINE_COLOR_GETTER (accent_soft, accent_soft)
DEFINE_COLOR_GETTER (surface_canvas, surface_canvas)
DEFINE_COLOR_GETTER (surface_content, surface_content)
DEFINE_COLOR_GETTER (surface_chrome, surface_chrome)
DEFINE_COLOR_GETTER (surface_layer, surface_layer)
DEFINE_COLOR_GETTER (surface_floating, surface_floating)
DEFINE_COLOR_GETTER (surface_opaque, surface_opaque)
DEFINE_COLOR_GETTER (outline, outline)
DEFINE_COLOR_GETTER (separator, separator)
DEFINE_COLOR_GETTER (rim_highlight, rim_highlight)

#define DEFINE_DOUBLE_GETTER(name, field) \
  double \
  awra_token_set_get_##name (AwraTokenSet *self) \
  { \
    g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), 0.0); \
    return self->field; \
  }

DEFINE_DOUBLE_GETTER (window_opacity, window_opacity)
DEFINE_DOUBLE_GETTER (surface_opacity, surface_opacity)
DEFINE_DOUBLE_GETTER (radius, radius)
DEFINE_DOUBLE_GETTER (border_width, border_width)

double
awra_token_set_get_spacing (AwraTokenSet *self,
                            AwraSpacing   spacing)
{
  g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), 0.0);
  if (spacing == AWRA_SPACING_NONE)
    return 0.0;
  g_return_val_if_fail (spacing >= AWRA_SPACING_XS &&
                        spacing <= AWRA_SPACING_XXL, 0.0);
  return self->spacing[spacing];
}

double
awra_token_set_get_radius_for_size (AwraTokenSet *self,
                                    AwraRadius    radius)
{
  g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), 0.0);
  g_return_val_if_fail (radius >= AWRA_RADIUS_SMALL &&
                        radius <= AWRA_RADIUS_PILL, 0.0);
  return self->radii[radius];
}

DEFINE_DOUBLE_GETTER (control_height, control_height)
DEFINE_DOUBLE_GETTER (navigation_height, navigation_height)
DEFINE_DOUBLE_GETTER (header_height, header_height)
DEFINE_DOUBLE_GETTER (grain_scale, grain_scale)

gboolean
awra_token_set_get_high_contrast (AwraTokenSet *self)
{
  g_return_val_if_fail (AWRA_IS_TOKEN_SET (self), FALSE);
  return self->high_contrast;
}
