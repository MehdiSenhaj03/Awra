/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-enums.h>

GType
awra_appearance_get_type (void)
{
  static gsize type_id;
  static const GEnumValue values[] = {
    { AWRA_APPEARANCE_SYSTEM, "AWRA_APPEARANCE_SYSTEM", "system" },
    { AWRA_APPEARANCE_LIGHT, "AWRA_APPEARANCE_LIGHT", "light" },
    { AWRA_APPEARANCE_DARK, "AWRA_APPEARANCE_DARK", "dark" },
    { 0, NULL, NULL },
  };

  if (g_once_init_enter (&type_id)) {
    GType id = g_enum_register_static ("AwraAppearance", values);
    g_once_init_leave (&type_id, id);
  }

  return (GType) type_id;
}

GType
awra_accent_source_get_type (void)
{
  static gsize type_id;
  static const GEnumValue values[] = {
    { AWRA_ACCENT_SOURCE_SYSTEM, "AWRA_ACCENT_SOURCE_SYSTEM", "system" },
    { AWRA_ACCENT_SOURCE_DEFAULT, "AWRA_ACCENT_SOURCE_DEFAULT", "default" },
    { AWRA_ACCENT_SOURCE_CUSTOM, "AWRA_ACCENT_SOURCE_CUSTOM", "custom" },
    { 0, NULL, NULL },
  };

  if (g_once_init_enter (&type_id)) {
    GType id = g_enum_register_static ("AwraAccentSource", values);
    g_once_init_leave (&type_id, id);
  }
  return (GType) type_id;
}

GType
awra_material_kind_get_type (void)
{
  static gsize type_id;
  static const GEnumValue values[] = {
    { AWRA_MATERIAL_KIND_SOLID, "AWRA_MATERIAL_KIND_SOLID", "solid" },
    { AWRA_MATERIAL_KIND_TRANSLUCENT, "AWRA_MATERIAL_KIND_TRANSLUCENT", "translucent" },
    { AWRA_MATERIAL_KIND_FROSTED, "AWRA_MATERIAL_KIND_FROSTED", "frosted" },
    { AWRA_MATERIAL_KIND_LIQUID, "AWRA_MATERIAL_KIND_LIQUID", "liquid" },
    { 0, NULL, NULL },
  };

  if (g_once_init_enter (&type_id)) {
    GType id = g_enum_register_static ("AwraMaterialKind", values);
    g_once_init_leave (&type_id, id);
  }

  return (GType) type_id;
}

GType
awra_material_preset_get_type (void)
{
  static const GEnumValue values[] = {
    { AWRA_MATERIAL_PRESET_NONE, "AWRA_MATERIAL_PRESET_NONE", "none" },
    { AWRA_MATERIAL_PRESET_CANVAS, "AWRA_MATERIAL_PRESET_CANVAS", "canvas" },
    { AWRA_MATERIAL_PRESET_CONTENT, "AWRA_MATERIAL_PRESET_CONTENT", "content" },
    { AWRA_MATERIAL_PRESET_CHROME, "AWRA_MATERIAL_PRESET_CHROME", "chrome" },
    { AWRA_MATERIAL_PRESET_LAYER, "AWRA_MATERIAL_PRESET_LAYER", "layer" },
    { AWRA_MATERIAL_PRESET_FLOATING, "AWRA_MATERIAL_PRESET_FLOATING", "floating" },
    { AWRA_MATERIAL_PRESET_OPAQUE, "AWRA_MATERIAL_PRESET_OPAQUE", "opaque" },
    { 0, NULL, NULL },
  };
  static gsize type_id;

  if (g_once_init_enter (&type_id)) {
    GType registered = g_enum_register_static ("AwraMaterialPreset", values);
    g_once_init_leave (&type_id, registered);
  }
  return type_id;
}

GType
awra_surface_role_get_type (void)
{
  static gsize type_id;
  static const GEnumValue values[] = {
    { AWRA_SURFACE_ROLE_WINDOW, "AWRA_SURFACE_ROLE_WINDOW", "window" },
    { AWRA_SURFACE_ROLE_CONTENT, "AWRA_SURFACE_ROLE_CONTENT", "content" },
    { AWRA_SURFACE_ROLE_SIDEBAR, "AWRA_SURFACE_ROLE_SIDEBAR", "sidebar" },
    { AWRA_SURFACE_ROLE_CARD, "AWRA_SURFACE_ROLE_CARD", "card" },
    { AWRA_SURFACE_ROLE_TOOLBAR, "AWRA_SURFACE_ROLE_TOOLBAR", "toolbar" },
    { AWRA_SURFACE_ROLE_FLOATING, "AWRA_SURFACE_ROLE_FLOATING", "floating" },
    { AWRA_SURFACE_ROLE_POPOVER, "AWRA_SURFACE_ROLE_POPOVER", "popover" },
    { AWRA_SURFACE_ROLE_MENU, "AWRA_SURFACE_ROLE_MENU", "menu" },
    { AWRA_SURFACE_ROLE_DIALOG, "AWRA_SURFACE_ROLE_DIALOG", "dialog" },
    { AWRA_SURFACE_ROLE_TOOLTIP, "AWRA_SURFACE_ROLE_TOOLTIP", "tooltip" },
    { AWRA_SURFACE_ROLE_HUD, "AWRA_SURFACE_ROLE_HUD", "hud" },
    { 0, NULL, NULL },
  };

  if (g_once_init_enter (&type_id)) {
    GType id = g_enum_register_static ("AwraSurfaceRole", values);
    g_once_init_leave (&type_id, id);
  }

  return (GType) type_id;
}

GType
awra_surface_edge_get_type (void)
{
  static const GFlagsValue values[] = {
    { AWRA_SURFACE_EDGE_NONE, "AWRA_SURFACE_EDGE_NONE", "none" },
    { AWRA_SURFACE_EDGE_TOP, "AWRA_SURFACE_EDGE_TOP", "top" },
    { AWRA_SURFACE_EDGE_END, "AWRA_SURFACE_EDGE_END", "end" },
    { AWRA_SURFACE_EDGE_BOTTOM, "AWRA_SURFACE_EDGE_BOTTOM", "bottom" },
    { AWRA_SURFACE_EDGE_START, "AWRA_SURFACE_EDGE_START", "start" },
    { AWRA_SURFACE_EDGE_ALL, "AWRA_SURFACE_EDGE_ALL", "all" },
    { 0, NULL, NULL },
  };
  static gsize type_id;

  if (g_once_init_enter (&type_id)) {
    GType registered = g_flags_register_static ("AwraSurfaceEdge", values);
    g_once_init_leave (&type_id, registered);
  }
  return type_id;
}

GType
awra_surface_corner_get_type (void)
{
  static const GFlagsValue values[] = {
    { AWRA_SURFACE_CORNER_NONE, "AWRA_SURFACE_CORNER_NONE", "none" },
    { AWRA_SURFACE_CORNER_TOP_START, "AWRA_SURFACE_CORNER_TOP_START", "top-start" },
    { AWRA_SURFACE_CORNER_TOP_END, "AWRA_SURFACE_CORNER_TOP_END", "top-end" },
    { AWRA_SURFACE_CORNER_BOTTOM_END, "AWRA_SURFACE_CORNER_BOTTOM_END", "bottom-end" },
    { AWRA_SURFACE_CORNER_BOTTOM_START, "AWRA_SURFACE_CORNER_BOTTOM_START", "bottom-start" },
    { AWRA_SURFACE_CORNER_ALL, "AWRA_SURFACE_CORNER_ALL", "all" },
    { 0, NULL, NULL },
  };
  static gsize type_id;

  if (g_once_init_enter (&type_id)) {
    GType registered = g_flags_register_static ("AwraSurfaceCorner", values);
    g_once_init_leave (&type_id, registered);
  }
  return type_id;
}

#define DEFINE_ENUM_TYPE(func, type_name, ...) \
  GType func (void) \
  { \
    static const GEnumValue values[] = { __VA_ARGS__, { 0, NULL, NULL } }; \
    static gsize type_id; \
    if (g_once_init_enter (&type_id)) { \
      GType registered = g_enum_register_static (type_name, values); \
      g_once_init_leave (&type_id, registered); \
    } \
    return type_id; \
  }

DEFINE_ENUM_TYPE (awra_elevation_get_type, "AwraElevation",
  { AWRA_ELEVATION_FLAT, "AWRA_ELEVATION_FLAT", "flat" },
  { AWRA_ELEVATION_RAISED, "AWRA_ELEVATION_RAISED", "raised" },
  { AWRA_ELEVATION_FLOATING, "AWRA_ELEVATION_FLOATING", "floating" },
  { AWRA_ELEVATION_MODAL, "AWRA_ELEVATION_MODAL", "modal" })

DEFINE_ENUM_TYPE (awra_button_appearance_get_type, "AwraButtonAppearance",
  { AWRA_BUTTON_APPEARANCE_PRIMARY, "AWRA_BUTTON_APPEARANCE_PRIMARY", "primary" },
  { AWRA_BUTTON_APPEARANCE_SECONDARY, "AWRA_BUTTON_APPEARANCE_SECONDARY", "secondary" },
  { AWRA_BUTTON_APPEARANCE_GHOST, "AWRA_BUTTON_APPEARANCE_GHOST", "ghost" },
  { AWRA_BUTTON_APPEARANCE_DESTRUCTIVE, "AWRA_BUTTON_APPEARANCE_DESTRUCTIVE", "destructive" },
  { AWRA_BUTTON_APPEARANCE_TOOLBAR, "AWRA_BUTTON_APPEARANCE_TOOLBAR", "toolbar" })

DEFINE_ENUM_TYPE (awra_card_appearance_get_type, "AwraCardAppearance",
  { AWRA_CARD_APPEARANCE_PLAIN, "AWRA_CARD_APPEARANCE_PLAIN", "plain" },
  { AWRA_CARD_APPEARANCE_TINTED, "AWRA_CARD_APPEARANCE_TINTED", "tinted" },
  { AWRA_CARD_APPEARANCE_RAISED, "AWRA_CARD_APPEARANCE_RAISED", "raised" })

DEFINE_ENUM_TYPE (awra_badge_appearance_get_type, "AwraBadgeAppearance",
  { AWRA_BADGE_APPEARANCE_NEUTRAL, "AWRA_BADGE_APPEARANCE_NEUTRAL", "neutral" },
  { AWRA_BADGE_APPEARANCE_ACCENT, "AWRA_BADGE_APPEARANCE_ACCENT", "accent" },
  { AWRA_BADGE_APPEARANCE_INFO, "AWRA_BADGE_APPEARANCE_INFO", "info" },
  { AWRA_BADGE_APPEARANCE_SUCCESS, "AWRA_BADGE_APPEARANCE_SUCCESS", "success" },
  { AWRA_BADGE_APPEARANCE_WARNING, "AWRA_BADGE_APPEARANCE_WARNING", "warning" },
  { AWRA_BADGE_APPEARANCE_DANGER, "AWRA_BADGE_APPEARANCE_DANGER", "danger" })

DEFINE_ENUM_TYPE (awra_validation_state_get_type, "AwraValidationState",
  { AWRA_VALIDATION_STATE_NONE, "AWRA_VALIDATION_STATE_NONE", "none" },
  { AWRA_VALIDATION_STATE_SUCCESS, "AWRA_VALIDATION_STATE_SUCCESS", "success" },
  { AWRA_VALIDATION_STATE_WARNING, "AWRA_VALIDATION_STATE_WARNING", "warning" },
  { AWRA_VALIDATION_STATE_ERROR, "AWRA_VALIDATION_STATE_ERROR", "error" })

DEFINE_ENUM_TYPE (awra_spacing_get_type, "AwraSpacing",
  { AWRA_SPACING_XS, "AWRA_SPACING_XS", "xs" },
  { AWRA_SPACING_SM, "AWRA_SPACING_SM", "sm" },
  { AWRA_SPACING_MD, "AWRA_SPACING_MD", "md" },
  { AWRA_SPACING_LG, "AWRA_SPACING_LG", "lg" },
  { AWRA_SPACING_XL, "AWRA_SPACING_XL", "xl" },
  { AWRA_SPACING_XXL, "AWRA_SPACING_XXL", "xxl" },
  { AWRA_SPACING_NONE, "AWRA_SPACING_NONE", "none" })

DEFINE_ENUM_TYPE (awra_layout_preset_get_type, "AwraLayoutPreset",
  { AWRA_LAYOUT_PRESET_COMPACT, "AWRA_LAYOUT_PRESET_COMPACT", "compact" },
  { AWRA_LAYOUT_PRESET_CONTROL_GROUP, "AWRA_LAYOUT_PRESET_CONTROL_GROUP", "control-group" },
  { AWRA_LAYOUT_PRESET_SECTION, "AWRA_LAYOUT_PRESET_SECTION", "section" },
  { AWRA_LAYOUT_PRESET_CONTENT, "AWRA_LAYOUT_PRESET_CONTENT", "content" },
  { AWRA_LAYOUT_PRESET_PAGE, "AWRA_LAYOUT_PRESET_PAGE", "page" },
  { AWRA_LAYOUT_PRESET_TOOLBAR, "AWRA_LAYOUT_PRESET_TOOLBAR", "toolbar" },
  { AWRA_LAYOUT_PRESET_OVERLAY, "AWRA_LAYOUT_PRESET_OVERLAY", "overlay" })

DEFINE_ENUM_TYPE (awra_radius_get_type, "AwraRadius",
  { AWRA_RADIUS_SMALL, "AWRA_RADIUS_SMALL", "small" },
  { AWRA_RADIUS_MEDIUM, "AWRA_RADIUS_MEDIUM", "medium" },
  { AWRA_RADIUS_LARGE, "AWRA_RADIUS_LARGE", "large" },
  { AWRA_RADIUS_WINDOW, "AWRA_RADIUS_WINDOW", "window" },
  { AWRA_RADIUS_PILL, "AWRA_RADIUS_PILL", "pill" })

DEFINE_ENUM_TYPE (awra_typography_get_type, "AwraTypography",
  { AWRA_TYPOGRAPHY_BODY, "AWRA_TYPOGRAPHY_BODY", "body" },
  { AWRA_TYPOGRAPHY_MUTED, "AWRA_TYPOGRAPHY_MUTED", "muted" },
  { AWRA_TYPOGRAPHY_EYEBROW, "AWRA_TYPOGRAPHY_EYEBROW", "eyebrow" },
  { AWRA_TYPOGRAPHY_TITLE_3, "AWRA_TYPOGRAPHY_TITLE_3", "title-3" },
  { AWRA_TYPOGRAPHY_TITLE_2, "AWRA_TYPOGRAPHY_TITLE_2", "title-2" },
  { AWRA_TYPOGRAPHY_TITLE_1, "AWRA_TYPOGRAPHY_TITLE_1", "title-1" },
  { AWRA_TYPOGRAPHY_DISPLAY, "AWRA_TYPOGRAPHY_DISPLAY", "display" },
  { AWRA_TYPOGRAPHY_MONOSPACE, "AWRA_TYPOGRAPHY_MONOSPACE", "monospace" },
  { AWRA_TYPOGRAPHY_LABEL, "AWRA_TYPOGRAPHY_LABEL", "label" })

GType
awra_effect_capability_get_type (void)
{
  static gsize type_id;
  static const GFlagsValue values[] = {
    { AWRA_EFFECT_CAPABILITY_NONE, "AWRA_EFFECT_CAPABILITY_NONE", "none" },
    { AWRA_EFFECT_CAPABILITY_BLUR, "AWRA_EFFECT_CAPABILITY_BLUR", "blur" },
    { 0, NULL, NULL },
  };

  if (g_once_init_enter (&type_id)) {
    GType id = g_flags_register_static ("AwraEffectCapability", values);
    g_once_init_leave (&type_id, id);
  }

  return (GType) type_id;
}

GType
awra_layout_mode_get_type (void)
{
  static gsize type_id;
  static const GEnumValue values[] = {
    { AWRA_LAYOUT_MODE_COMPACT, "AWRA_LAYOUT_MODE_COMPACT", "compact" },
    { AWRA_LAYOUT_MODE_MEDIUM, "AWRA_LAYOUT_MODE_MEDIUM", "medium" },
    { AWRA_LAYOUT_MODE_EXPANDED, "AWRA_LAYOUT_MODE_EXPANDED", "expanded" },
    { 0, NULL, NULL },
  };

  if (g_once_init_enter (&type_id)) {
    GType id = g_enum_register_static ("AwraLayoutMode", values);
    g_once_init_leave (&type_id, id);
  }

  return (GType) type_id;
}

DEFINE_ENUM_TYPE (awra_master_detail_pane_get_type, "AwraMasterDetailPane",
  { AWRA_MASTER_DETAIL_PANE_MASTER, "AWRA_MASTER_DETAIL_PANE_MASTER", "master" },
  { AWRA_MASTER_DETAIL_PANE_DETAIL, "AWRA_MASTER_DETAIL_PANE_DETAIL", "detail" })

DEFINE_ENUM_TYPE (awra_data_view_state_get_type, "AwraDataViewState",
  { AWRA_DATA_VIEW_STATE_CONTENT, "AWRA_DATA_VIEW_STATE_CONTENT", "content" },
  { AWRA_DATA_VIEW_STATE_EMPTY, "AWRA_DATA_VIEW_STATE_EMPTY", "empty" },
  { AWRA_DATA_VIEW_STATE_LOADING, "AWRA_DATA_VIEW_STATE_LOADING", "loading" },
  { AWRA_DATA_VIEW_STATE_ERROR, "AWRA_DATA_VIEW_STATE_ERROR", "error" })

DEFINE_ENUM_TYPE (awra_status_appearance_get_type, "AwraStatusAppearance",
  { AWRA_STATUS_APPEARANCE_INFO, "AWRA_STATUS_APPEARANCE_INFO", "info" },
  { AWRA_STATUS_APPEARANCE_SUCCESS, "AWRA_STATUS_APPEARANCE_SUCCESS", "success" },
  { AWRA_STATUS_APPEARANCE_WARNING, "AWRA_STATUS_APPEARANCE_WARNING", "warning" },
  { AWRA_STATUS_APPEARANCE_ERROR, "AWRA_STATUS_APPEARANCE_ERROR", "error" })

DEFINE_ENUM_TYPE (awra_panel_edge_get_type, "AwraPanelEdge",
  { AWRA_PANEL_EDGE_START, "AWRA_PANEL_EDGE_START", "start" },
  { AWRA_PANEL_EDGE_END, "AWRA_PANEL_EDGE_END", "end" })

GType
awra_motion_preset_get_type (void)
{
  static gsize type_id;
  static const GEnumValue values[] = {
    { AWRA_MOTION_PRESET_INSTANT, "AWRA_MOTION_PRESET_INSTANT", "instant" },
    { AWRA_MOTION_PRESET_FAST, "AWRA_MOTION_PRESET_FAST", "fast" },
    { AWRA_MOTION_PRESET_NORMAL, "AWRA_MOTION_PRESET_NORMAL", "normal" },
    { AWRA_MOTION_PRESET_SLOW, "AWRA_MOTION_PRESET_SLOW", "slow" },
    { 0, NULL, NULL },
  };

  if (g_once_init_enter (&type_id)) {
    GType id = g_enum_register_static ("AwraMotionPreset", values);
    g_once_init_leave (&type_id, id);
  }

  return (GType) type_id;
}

DEFINE_ENUM_TYPE (awra_keyboard_physical_layout_get_type,
  "AwraKeyboardPhysicalLayout",
  { AWRA_KEYBOARD_PHYSICAL_LAYOUT_ANSI, "AWRA_KEYBOARD_PHYSICAL_LAYOUT_ANSI", "ansi" },
  { AWRA_KEYBOARD_PHYSICAL_LAYOUT_ISO, "AWRA_KEYBOARD_PHYSICAL_LAYOUT_ISO", "iso" })

DEFINE_ENUM_TYPE (awra_animation_state_get_type, "AwraAnimationState",
  { AWRA_ANIMATION_STATE_IDLE, "AWRA_ANIMATION_STATE_IDLE", "idle" },
  { AWRA_ANIMATION_STATE_PAUSED, "AWRA_ANIMATION_STATE_PAUSED", "paused" },
  { AWRA_ANIMATION_STATE_PLAYING, "AWRA_ANIMATION_STATE_PLAYING", "playing" },
  { AWRA_ANIMATION_STATE_FINISHED, "AWRA_ANIMATION_STATE_FINISHED", "finished" })

DEFINE_ENUM_TYPE (awra_easing_get_type, "AwraEasing",
  { AWRA_EASING_LINEAR, "AWRA_EASING_LINEAR", "linear" },
  { AWRA_EASING_EASE_OUT_QUAD, "AWRA_EASING_EASE_OUT_QUAD", "ease-out-quad" },
  { AWRA_EASING_EASE_OUT_CUBIC, "AWRA_EASING_EASE_OUT_CUBIC", "ease-out-cubic" },
  { AWRA_EASING_EASE_IN_OUT_CUBIC, "AWRA_EASING_EASE_IN_OUT_CUBIC", "ease-in-out-cubic" },
  { AWRA_EASING_EASE_OUT_QUART, "AWRA_EASING_EASE_OUT_QUART", "ease-out-quart" },
  { AWRA_EASING_EASE_OUT_EXPO, "AWRA_EASING_EASE_OUT_EXPO", "ease-out-expo" },
  { AWRA_EASING_EASE_OUT_BACK, "AWRA_EASING_EASE_OUT_BACK", "ease-out-back" },
  { AWRA_EASING_EASE_IN_OUT_SINE, "AWRA_EASING_EASE_IN_OUT_SINE", "ease-in-out-sine" })

DEFINE_ENUM_TYPE (awra_avatar_shape_get_type, "AwraAvatarShape",
  { AWRA_AVATAR_SHAPE_CIRCLE, "AWRA_AVATAR_SHAPE_CIRCLE", "circle" },
  { AWRA_AVATAR_SHAPE_ROUNDED, "AWRA_AVATAR_SHAPE_ROUNDED", "rounded" })

DEFINE_ENUM_TYPE (awra_presence_get_type, "AwraPresence",
  { AWRA_PRESENCE_NONE, "AWRA_PRESENCE_NONE", "none" },
  { AWRA_PRESENCE_ONLINE, "AWRA_PRESENCE_ONLINE", "online" },
  { AWRA_PRESENCE_IDLE, "AWRA_PRESENCE_IDLE", "idle" },
  { AWRA_PRESENCE_BUSY, "AWRA_PRESENCE_BUSY", "busy" },
  { AWRA_PRESENCE_OFFLINE, "AWRA_PRESENCE_OFFLINE", "offline" })

DEFINE_ENUM_TYPE (awra_chip_appearance_get_type, "AwraChipAppearance",
  { AWRA_CHIP_APPEARANCE_NEUTRAL, "AWRA_CHIP_APPEARANCE_NEUTRAL", "neutral" },
  { AWRA_CHIP_APPEARANCE_ACCENT, "AWRA_CHIP_APPEARANCE_ACCENT", "accent" })
DEFINE_ENUM_TYPE (awra_conversation_align_get_type, "AwraConversationAlign",
  { AWRA_CONVERSATION_ALIGN_TOP, "AWRA_CONVERSATION_ALIGN_TOP", "top" },
  { AWRA_CONVERSATION_ALIGN_CENTER, "AWRA_CONVERSATION_ALIGN_CENTER", "center" },
  { AWRA_CONVERSATION_ALIGN_END, "AWRA_CONVERSATION_ALIGN_END", "end" })

DEFINE_ENUM_TYPE (awra_divider_label_position_get_type, "AwraDividerLabelPosition",
  { AWRA_DIVIDER_LABEL_POSITION_CENTER, "AWRA_DIVIDER_LABEL_POSITION_CENTER", "center" },
  { AWRA_DIVIDER_LABEL_POSITION_END, "AWRA_DIVIDER_LABEL_POSITION_END", "end" })

DEFINE_ENUM_TYPE (awra_divider_appearance_get_type, "AwraDividerAppearance",
  { AWRA_DIVIDER_APPEARANCE_NEUTRAL, "AWRA_DIVIDER_APPEARANCE_NEUTRAL", "neutral" },
  { AWRA_DIVIDER_APPEARANCE_ACCENT, "AWRA_DIVIDER_APPEARANCE_ACCENT", "accent" },
  { AWRA_DIVIDER_APPEARANCE_DANGER, "AWRA_DIVIDER_APPEARANCE_DANGER", "danger" })
