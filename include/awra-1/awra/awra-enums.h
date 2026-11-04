/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
  AWRA_APPEARANCE_SYSTEM,
  AWRA_APPEARANCE_LIGHT,
  AWRA_APPEARANCE_DARK,
} AwraAppearance;

#define AWRA_TYPE_APPEARANCE (awra_appearance_get_type ())
GType awra_appearance_get_type (void) G_GNUC_CONST;

/**
 * AwraAccentSource:
 * @AWRA_ACCENT_SOURCE_SYSTEM: desktop accent when available, otherwise default
 * @AWRA_ACCENT_SOURCE_DEFAULT: Awra's restrained default accent
 * @AWRA_ACCENT_SOURCE_CUSTOM: the application-provided accent
 */
typedef enum {
  AWRA_ACCENT_SOURCE_SYSTEM,
  AWRA_ACCENT_SOURCE_DEFAULT,
  AWRA_ACCENT_SOURCE_CUSTOM,
} AwraAccentSource;

#define AWRA_TYPE_ACCENT_SOURCE (awra_accent_source_get_type ())
GType awra_accent_source_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_MATERIAL_KIND_SOLID,
  AWRA_MATERIAL_KIND_TRANSLUCENT,
  AWRA_MATERIAL_KIND_FROSTED,
  AWRA_MATERIAL_KIND_LIQUID,
} AwraMaterialKind;

#define AWRA_TYPE_MATERIAL_KIND (awra_material_kind_get_type ())
GType awra_material_kind_get_type (void) G_GNUC_CONST;

/**
 * AwraMaterialPreset:
 * @AWRA_MATERIAL_PRESET_NONE: a low-level material primitive
 * @AWRA_MATERIAL_PRESET_CANVAS: the native frosted window canvas
 * @AWRA_MATERIAL_PRESET_CONTENT: an unpainted content layer
 * @AWRA_MATERIAL_PRESET_CHROME: structural chrome such as sidebars and headers
 * @AWRA_MATERIAL_PRESET_LAYER: a subtly tinted content grouping
 * @AWRA_MATERIAL_PRESET_FLOATING: a strongly separated floating surface
 * @AWRA_MATERIAL_PRESET_OPAQUE: an explicitly opaque surface
 *
 * Semantic material recipes. Low-level material constructors remain available
 * and report %AWRA_MATERIAL_PRESET_NONE.
 */
typedef enum {
  AWRA_MATERIAL_PRESET_NONE,
  AWRA_MATERIAL_PRESET_CANVAS,
  AWRA_MATERIAL_PRESET_CONTENT,
  AWRA_MATERIAL_PRESET_CHROME,
  AWRA_MATERIAL_PRESET_LAYER,
  AWRA_MATERIAL_PRESET_FLOATING,
  AWRA_MATERIAL_PRESET_OPAQUE,
} AwraMaterialPreset;

#define AWRA_TYPE_MATERIAL_PRESET (awra_material_preset_get_type ())
GType awra_material_preset_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_SURFACE_ROLE_WINDOW,
  AWRA_SURFACE_ROLE_CONTENT,
  AWRA_SURFACE_ROLE_SIDEBAR,
  AWRA_SURFACE_ROLE_CARD,
  AWRA_SURFACE_ROLE_TOOLBAR,
  AWRA_SURFACE_ROLE_FLOATING,
  AWRA_SURFACE_ROLE_POPOVER,
  AWRA_SURFACE_ROLE_MENU,
  AWRA_SURFACE_ROLE_DIALOG,
  AWRA_SURFACE_ROLE_TOOLTIP,
  AWRA_SURFACE_ROLE_HUD,
} AwraSurfaceRole;

#define AWRA_TYPE_SURFACE_ROLE (awra_surface_role_get_type ())
GType awra_surface_role_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_SURFACE_EDGE_NONE   = 0,
  AWRA_SURFACE_EDGE_TOP    = 1 << 0,
  AWRA_SURFACE_EDGE_END    = 1 << 1,
  AWRA_SURFACE_EDGE_BOTTOM = 1 << 2,
  AWRA_SURFACE_EDGE_START  = 1 << 3,
  AWRA_SURFACE_EDGE_ALL    = (1 << 4) - 1,
} AwraSurfaceEdge;

#define AWRA_TYPE_SURFACE_EDGE (awra_surface_edge_get_type ())
GType awra_surface_edge_get_type (void) G_GNUC_CONST;

/**
 * AwraSurfaceCorner:
 * @AWRA_SURFACE_CORNER_NONE: no rounded corner
 * @AWRA_SURFACE_CORNER_TOP_START: leading corner on the top edge
 * @AWRA_SURFACE_CORNER_TOP_END: trailing corner on the top edge
 * @AWRA_SURFACE_CORNER_BOTTOM_END: trailing corner on the bottom edge
 * @AWRA_SURFACE_CORNER_BOTTOM_START: leading corner on the bottom edge
 * @AWRA_SURFACE_CORNER_ALL: all corners
 *
 * Logical corners used to compose multiple surfaces into one continuous
 * outer shape.
 */
typedef enum {
  AWRA_SURFACE_CORNER_NONE         = 0,
  AWRA_SURFACE_CORNER_TOP_START    = 1 << 0,
  AWRA_SURFACE_CORNER_TOP_END      = 1 << 1,
  AWRA_SURFACE_CORNER_BOTTOM_END   = 1 << 2,
  AWRA_SURFACE_CORNER_BOTTOM_START = 1 << 3,
  AWRA_SURFACE_CORNER_ALL          = (1 << 4) - 1,
} AwraSurfaceCorner;

#define AWRA_TYPE_SURFACE_CORNER (awra_surface_corner_get_type ())
GType awra_surface_corner_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_ELEVATION_FLAT,
  AWRA_ELEVATION_RAISED,
  AWRA_ELEVATION_FLOATING,
  AWRA_ELEVATION_MODAL,
} AwraElevation;

#define AWRA_TYPE_ELEVATION (awra_elevation_get_type ())
GType awra_elevation_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_BUTTON_APPEARANCE_PRIMARY,
  AWRA_BUTTON_APPEARANCE_SECONDARY,
  AWRA_BUTTON_APPEARANCE_GHOST,
  AWRA_BUTTON_APPEARANCE_DESTRUCTIVE,
  AWRA_BUTTON_APPEARANCE_TOOLBAR,
} AwraButtonAppearance;

#define AWRA_TYPE_BUTTON_APPEARANCE (awra_button_appearance_get_type ())
GType awra_button_appearance_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_CARD_APPEARANCE_PLAIN,
  AWRA_CARD_APPEARANCE_TINTED,
  AWRA_CARD_APPEARANCE_RAISED,
} AwraCardAppearance;

#define AWRA_TYPE_CARD_APPEARANCE (awra_card_appearance_get_type ())
GType awra_card_appearance_get_type (void) G_GNUC_CONST;

/**
 * AwraBadgeAppearance:
 * @AWRA_BADGE_APPEARANCE_NEUTRAL: quiet metadata without emphasis
 * @AWRA_BADGE_APPEARANCE_ACCENT: application accent emphasis
 * @AWRA_BADGE_APPEARANCE_INFO: informational metadata
 * @AWRA_BADGE_APPEARANCE_SUCCESS: positive or available state
 * @AWRA_BADGE_APPEARANCE_WARNING: state requiring attention
 * @AWRA_BADGE_APPEARANCE_DANGER: invalid or critical state
 *
 * Semantic badge tones whose accessible Light/Dark rendering is owned by
 * Awra rather than application CSS.
 */
typedef enum {
  AWRA_BADGE_APPEARANCE_NEUTRAL,
  AWRA_BADGE_APPEARANCE_ACCENT,
  AWRA_BADGE_APPEARANCE_INFO,
  AWRA_BADGE_APPEARANCE_SUCCESS,
  AWRA_BADGE_APPEARANCE_WARNING,
  AWRA_BADGE_APPEARANCE_DANGER,
} AwraBadgeAppearance;

#define AWRA_TYPE_BADGE_APPEARANCE (awra_badge_appearance_get_type ())
GType awra_badge_appearance_get_type (void) G_GNUC_CONST;

/**
 * AwraValidationState:
 * @AWRA_VALIDATION_STATE_NONE: no validation result is shown
 * @AWRA_VALIDATION_STATE_SUCCESS: the value has been accepted
 * @AWRA_VALIDATION_STATE_WARNING: the value needs attention but is accepted
 * @AWRA_VALIDATION_STATE_ERROR: the value is invalid
 */
typedef enum {
  AWRA_VALIDATION_STATE_NONE,
  AWRA_VALIDATION_STATE_SUCCESS,
  AWRA_VALIDATION_STATE_WARNING,
  AWRA_VALIDATION_STATE_ERROR,
} AwraValidationState;

#define AWRA_TYPE_VALIDATION_STATE (awra_validation_state_get_type ())
GType awra_validation_state_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_SPACING_XS,
  AWRA_SPACING_SM,
  AWRA_SPACING_MD,
  AWRA_SPACING_LG,
  AWRA_SPACING_XL,
  AWRA_SPACING_XXL,
  AWRA_SPACING_NONE,
} AwraSpacing;

#define AWRA_TYPE_SPACING (awra_spacing_get_type ())
GType awra_spacing_get_type (void) G_GNUC_CONST;

/**
 * AwraLayoutPreset:
 * @AWRA_LAYOUT_PRESET_COMPACT: dense utility content
 * @AWRA_LAYOUT_PRESET_CONTROL_GROUP: a related group of controls
 * @AWRA_LAYOUT_PRESET_SECTION: a title and its supporting copy
 * @AWRA_LAYOUT_PRESET_CONTENT: ordinary inset application content
 * @AWRA_LAYOUT_PRESET_PAGE: the primary rhythm of a scrollable page
 * @AWRA_LAYOUT_PRESET_TOOLBAR: compact horizontal application chrome
 * @AWRA_LAYOUT_PRESET_OVERLAY: floating, popover or dialog content
 *
 * Semantic combinations of inset and gap tokens. Applications should prefer
 * these recipes for common composition and use #AwraSpacing for deliberate
 * custom layouts.
 */
typedef enum {
  AWRA_LAYOUT_PRESET_COMPACT,
  AWRA_LAYOUT_PRESET_CONTROL_GROUP,
  AWRA_LAYOUT_PRESET_SECTION,
  AWRA_LAYOUT_PRESET_CONTENT,
  AWRA_LAYOUT_PRESET_PAGE,
  AWRA_LAYOUT_PRESET_TOOLBAR,
  AWRA_LAYOUT_PRESET_OVERLAY,
} AwraLayoutPreset;

#define AWRA_TYPE_LAYOUT_PRESET (awra_layout_preset_get_type ())
GType awra_layout_preset_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_RADIUS_SMALL,
  AWRA_RADIUS_MEDIUM,
  AWRA_RADIUS_LARGE,
  AWRA_RADIUS_WINDOW,
  AWRA_RADIUS_PILL,
} AwraRadius;

#define AWRA_TYPE_RADIUS (awra_radius_get_type ())
GType awra_radius_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_TYPOGRAPHY_BODY,
  AWRA_TYPOGRAPHY_MUTED,
  AWRA_TYPOGRAPHY_EYEBROW,
  AWRA_TYPOGRAPHY_TITLE_3,
  AWRA_TYPOGRAPHY_TITLE_2,
  AWRA_TYPOGRAPHY_TITLE_1,
  AWRA_TYPOGRAPHY_DISPLAY,
  AWRA_TYPOGRAPHY_MONOSPACE,
  AWRA_TYPOGRAPHY_LABEL,
} AwraTypography;

#define AWRA_TYPE_TYPOGRAPHY (awra_typography_get_type ())
GType awra_typography_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_EFFECT_CAPABILITY_NONE = 0,
  AWRA_EFFECT_CAPABILITY_BLUR = 1 << 0,
} AwraEffectCapability;

#define AWRA_TYPE_EFFECT_CAPABILITY (awra_effect_capability_get_type ())
GType awra_effect_capability_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_LAYOUT_MODE_COMPACT,
  AWRA_LAYOUT_MODE_MEDIUM,
  AWRA_LAYOUT_MODE_EXPANDED,
} AwraLayoutMode;

#define AWRA_TYPE_LAYOUT_MODE (awra_layout_mode_get_type ())
GType awra_layout_mode_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_MASTER_DETAIL_PANE_MASTER,
  AWRA_MASTER_DETAIL_PANE_DETAIL,
} AwraMasterDetailPane;

#define AWRA_TYPE_MASTER_DETAIL_PANE (awra_master_detail_pane_get_type ())
GType awra_master_detail_pane_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_DATA_VIEW_STATE_CONTENT,
  AWRA_DATA_VIEW_STATE_EMPTY,
  AWRA_DATA_VIEW_STATE_LOADING,
  AWRA_DATA_VIEW_STATE_ERROR,
} AwraDataViewState;

#define AWRA_TYPE_DATA_VIEW_STATE (awra_data_view_state_get_type ())
GType awra_data_view_state_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_STATUS_APPEARANCE_INFO,
  AWRA_STATUS_APPEARANCE_SUCCESS,
  AWRA_STATUS_APPEARANCE_WARNING,
  AWRA_STATUS_APPEARANCE_ERROR,
} AwraStatusAppearance;

#define AWRA_TYPE_STATUS_APPEARANCE (awra_status_appearance_get_type ())
GType awra_status_appearance_get_type (void) G_GNUC_CONST;

/**
 * AwraPanelEdge:
 * @AWRA_PANEL_EDGE_START: the logical leading edge
 * @AWRA_PANEL_EDGE_END: the logical trailing edge
 */
typedef enum {
  AWRA_PANEL_EDGE_START,
  AWRA_PANEL_EDGE_END,
} AwraPanelEdge;

#define AWRA_TYPE_PANEL_EDGE (awra_panel_edge_get_type ())
GType awra_panel_edge_get_type (void) G_GNUC_CONST;

typedef enum {
  AWRA_MOTION_PRESET_INSTANT,
  AWRA_MOTION_PRESET_FAST,
  AWRA_MOTION_PRESET_NORMAL,
  AWRA_MOTION_PRESET_SLOW,
} AwraMotionPreset;

#define AWRA_TYPE_MOTION_PRESET (awra_motion_preset_get_type ())
GType awra_motion_preset_get_type (void) G_GNUC_CONST;

/**
 * AwraKeyboardPhysicalLayout:
 * @AWRA_KEYBOARD_PHYSICAL_LAYOUT_ANSI: the ANSI key arrangement with a wide
 *   single-row Enter and a long left Shift
 * @AWRA_KEYBOARD_PHYSICAL_LAYOUT_ISO: the ISO arrangement with a tall Enter
 *   and an extra key beside the left Shift
 *
 * The physical key arrangement presented by #AwraKeyboardView. Key legends
 * follow the active logical layout independently of this choice.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_KEYBOARD_PHYSICAL_LAYOUT_ANSI,
  AWRA_KEYBOARD_PHYSICAL_LAYOUT_ISO,
} AwraKeyboardPhysicalLayout;

#define AWRA_TYPE_KEYBOARD_PHYSICAL_LAYOUT (awra_keyboard_physical_layout_get_type ())
/**
 * awra_keyboard_physical_layout_get_type:
 *
 * Since: 1.1
 */
GType awra_keyboard_physical_layout_get_type (void) G_GNUC_CONST;

/**
 * AwraAnimationState:
 * @AWRA_ANIMATION_STATE_IDLE: the animation has not started, or was reset
 * @AWRA_ANIMATION_STATE_PAUSED: the animation was paused and can resume
 * @AWRA_ANIMATION_STATE_PLAYING: the animation is running on the frame clock
 * @AWRA_ANIMATION_STATE_FINISHED: the animation reached its end or was skipped
 *
 * Where an #AwraAnimation is in its lifecycle.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_ANIMATION_STATE_IDLE,
  AWRA_ANIMATION_STATE_PAUSED,
  AWRA_ANIMATION_STATE_PLAYING,
  AWRA_ANIMATION_STATE_FINISHED,
} AwraAnimationState;

#define AWRA_TYPE_ANIMATION_STATE (awra_animation_state_get_type ())
/**
 * awra_animation_state_get_type:
 *
 * Since: 1.1
 */
GType awra_animation_state_get_type (void) G_GNUC_CONST;

/**
 * AwraEasing:
 * @AWRA_EASING_LINEAR: constant speed, for progress that maps to time
 * @AWRA_EASING_EASE_OUT_QUAD: a gentle deceleration
 * @AWRA_EASING_EASE_OUT_CUBIC: Awra's default deceleration for entering content
 * @AWRA_EASING_EASE_IN_OUT_CUBIC: accelerates then decelerates, for moves
 *   between two resting places
 * @AWRA_EASING_EASE_OUT_QUART: a quick start that settles softly
 * @AWRA_EASING_EASE_OUT_EXPO: a very fast start with a long, quiet tail
 * @AWRA_EASING_EASE_OUT_BACK: overshoots the end slightly, then settles
 * @AWRA_EASING_EASE_IN_OUT_SINE: the softest symmetric curve, for loops
 *
 * Curves an #AwraTimedAnimation can follow. Each maps progress in [0, 1] to
 * an eased progress that starts at 0 and ends at 1.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_EASING_LINEAR,
  AWRA_EASING_EASE_OUT_QUAD,
  AWRA_EASING_EASE_OUT_CUBIC,
  AWRA_EASING_EASE_IN_OUT_CUBIC,
  AWRA_EASING_EASE_OUT_QUART,
  AWRA_EASING_EASE_OUT_EXPO,
  AWRA_EASING_EASE_OUT_BACK,
  AWRA_EASING_EASE_IN_OUT_SINE,
} AwraEasing;

#define AWRA_TYPE_EASING (awra_easing_get_type ())
/**
 * awra_easing_get_type:
 *
 * Since: 1.1
 */
GType awra_easing_get_type (void) G_GNUC_CONST;

/**
 * AwraAvatarShape:
 * @AWRA_AVATAR_SHAPE_CIRCLE: a circle, for people
 * @AWRA_AVATAR_SHAPE_ROUNDED: a rounded square, for places, groups and
 *   workspaces
 *
 * The outline an #AwraAvatar clips its picture to.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_AVATAR_SHAPE_CIRCLE,
  AWRA_AVATAR_SHAPE_ROUNDED,
} AwraAvatarShape;

#define AWRA_TYPE_AVATAR_SHAPE (awra_avatar_shape_get_type ())
/**
 * awra_avatar_shape_get_type:
 *
 * Since: 1.1
 */
GType awra_avatar_shape_get_type (void) G_GNUC_CONST;

/**
 * AwraPresence:
 * @AWRA_PRESENCE_NONE: no presence is shown
 * @AWRA_PRESENCE_ONLINE: available
 * @AWRA_PRESENCE_IDLE: away from the keyboard
 * @AWRA_PRESENCE_BUSY: available but not to be disturbed
 * @AWRA_PRESENCE_OFFLINE: not connected
 *
 * Whether a person can be reached. Each state has its own shape as well as
 * its own colour, so it reads without colour vision.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_PRESENCE_NONE,
  AWRA_PRESENCE_ONLINE,
  AWRA_PRESENCE_IDLE,
  AWRA_PRESENCE_BUSY,
  AWRA_PRESENCE_OFFLINE,
} AwraPresence;

#define AWRA_TYPE_PRESENCE (awra_presence_get_type ())
/**
 * awra_presence_get_type:
 *
 * Since: 1.1
 */
GType awra_presence_get_type (void) G_GNUC_CONST;

/**
 * AwraChipAppearance:
 * @AWRA_CHIP_APPEARANCE_NEUTRAL: a quiet chip that takes the accent when
 *   active, as for reactions and filters
 * @AWRA_CHIP_APPEARANCE_ACCENT: a chip tinted with the accent, filled when
 *   active, as for a suggested choice
 *
 * The tone of an #AwraChip.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_CHIP_APPEARANCE_NEUTRAL,
  AWRA_CHIP_APPEARANCE_ACCENT,
} AwraChipAppearance;

#define AWRA_TYPE_CHIP_APPEARANCE (awra_chip_appearance_get_type ())
/**
 * awra_chip_appearance_get_type:
 *
 * Since: 1.1
 */
GType awra_chip_appearance_get_type (void) G_GNUC_CONST;

/**
 * AwraConversationAlign:
 * @AWRA_CONVERSATION_ALIGN_TOP: the item's top meets the top of the view
 * @AWRA_CONVERSATION_ALIGN_CENTER: the item is centred in the view
 * @AWRA_CONVERSATION_ALIGN_END: the item's bottom meets the bottom of the view
 *
 * Where awra_conversation_view_scroll_to_item() places an item.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_CONVERSATION_ALIGN_TOP,
  AWRA_CONVERSATION_ALIGN_CENTER,
  AWRA_CONVERSATION_ALIGN_END,
} AwraConversationAlign;

#define AWRA_TYPE_CONVERSATION_ALIGN (awra_conversation_align_get_type ())
/**
 * awra_conversation_align_get_type:
 *
 * Since: 1.1
 */
GType awra_conversation_align_get_type (void) G_GNUC_CONST;

/**
 * AwraDividerLabelPosition:
 * @AWRA_DIVIDER_LABEL_POSITION_CENTER: the label sits between two lines
 * @AWRA_DIVIDER_LABEL_POSITION_END: the line runs up to the label at the end
 *
 * Where an #AwraDivider puts its label.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_DIVIDER_LABEL_POSITION_CENTER,
  AWRA_DIVIDER_LABEL_POSITION_END,
} AwraDividerLabelPosition;

#define AWRA_TYPE_DIVIDER_LABEL_POSITION (awra_divider_label_position_get_type ())
/**
 * awra_divider_label_position_get_type:
 *
 * Since: 1.1
 */
GType awra_divider_label_position_get_type (void) G_GNUC_CONST;

/**
 * AwraDividerAppearance:
 * @AWRA_DIVIDER_APPEARANCE_NEUTRAL: a quiet line, such as between days
 * @AWRA_DIVIDER_APPEARANCE_ACCENT: a line in the accent colour
 * @AWRA_DIVIDER_APPEARANCE_DANGER: a line that demands attention, such as the
 *   start of unread messages, with its label in a small pill
 *
 * How an #AwraDivider looks.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_DIVIDER_APPEARANCE_NEUTRAL,
  AWRA_DIVIDER_APPEARANCE_ACCENT,
  AWRA_DIVIDER_APPEARANCE_DANGER,
} AwraDividerAppearance;

#define AWRA_TYPE_DIVIDER_APPEARANCE (awra_divider_appearance_get_type ())
/**
 * awra_divider_appearance_get_type:
 *
 * Since: 1.1
 */
GType awra_divider_appearance_get_type (void) G_GNUC_CONST;

/**
 * AwraEdgeStatus:
 * @AWRA_EDGE_STATUS_NONE: nothing is happening at the edge
 * @AWRA_EDGE_STATUS_LOADING: more items are on their way
 * @AWRA_EDGE_STATUS_FAILED: loading more failed; the edge offers to retry
 *
 * What is happening at an edge of a list that loads more as it is
 * scrolled, such as an #AwraConversationView.
 *
 * Since: 1.1
 */
typedef enum {
  AWRA_EDGE_STATUS_NONE,
  AWRA_EDGE_STATUS_LOADING,
  AWRA_EDGE_STATUS_FAILED,
} AwraEdgeStatus;

#define AWRA_TYPE_EDGE_STATUS (awra_edge_status_get_type ())
/**
 * awra_edge_status_get_type:
 *
 * Since: 1.1
 */
GType awra_edge_status_get_type (void) G_GNUC_CONST;


G_END_DECLS
