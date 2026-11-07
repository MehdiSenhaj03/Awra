/* Awra 2: a frosted-glass desktop UI framework on BUI.
 *
 * Awra 2 keeps Awra's visual identity (semantic materials, compositor blur,
 * tokens, components) and drops GTK: widgets are nodes of BUI's kit (Awra's
 * fork of Blender's interface runtime, in bui/), laid out by Awra's own
 * incremental layout, drawn by the kit's damage-driven software raster into
 * the window's shared-memory buffers, and hosted by BUI's native windows
 * (Wayland, X11, Win32).
 *
 * Conventions
 *  - Every widget is an AwraWidget (opaque). Constructors return a new
 *    reference; containers take their own reference on what they hold, so a
 *    widget added to a parent can be unreferenced by its creator
 *    (awra_widget_unref) and lives as long as it is in the tree, as a GTK
 *    widget does. awra_widget_destroy removes a widget from its parent.
 *  - Sizes are logical pixels: Awra multiplies them by the window scale.
 *  - Everything runs on the thread that called awra_init, except
 *    awra_invoke(), which any thread may call.
 *  - Callbacks receive the widget, an AwraArgs describing the signal and the
 *    user data; an optional destroy notify frees the data with the handler.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AWRA_VERSION_MAJOR 2
#define AWRA_VERSION_MINOR 0
#define AWRA_VERSION_MICRO 0
#define AWRA_VERSION_S "2.0.0"

typedef struct BKNode AwraWidget;
typedef void (*AwraDestroyNotify)(void *data);

/* ----------------------------------------------------------------------- */
/* Application loop */

/* Initializes Awra (idempotent). app_id names the application to the
 * desktop (Wayland app id, WM_CLASS). */
void awra_init(const char *app_id);
const char *awra_app_id(void);
/* Runs until awra_quit(), or until the last window closes when
 * awra_set_keep_alive(false) (the default). */
void awra_run(void);
void awra_quit(void);
void awra_set_keep_alive(bool keep_alive);
/* Handles pending events without blocking; returns the open window count. */
int awra_pump(void);
/* Calls fn(data) on the UI thread soon. Thread-safe. */
void awra_invoke(void (*fn)(void *data), void *data);
/* One-shot and repeating timers on the UI thread; ids > 0. A repeating
 * callback returns false to stop. */
unsigned awra_timeout_add(unsigned ms, bool (*fn)(void *data), void *data,
                          AwraDestroyNotify destroy);
unsigned awra_idle_add(bool (*fn)(void *data), void *data, AwraDestroyNotify destroy);
void awra_source_remove(unsigned id);
/* The window backend in use: "wayland", "x11", "win32". */
const char *awra_backend(void);
/* Milliseconds since an arbitrary start (monotonic). */
double awra_now_ms(void);

/* ----------------------------------------------------------------------- */
/* Enumerations (same names and values as Awra 1) */

typedef enum { AWRA_ORIENTATION_HORIZONTAL = 0, AWRA_ORIENTATION_VERTICAL = 1 } AwraOrientation;
typedef enum {
  AWRA_ALIGN_FILL = 0,
  AWRA_ALIGN_START,
  AWRA_ALIGN_END,
  AWRA_ALIGN_CENTER,
  AWRA_ALIGN_BASELINE
} AwraAlign;
typedef enum {
  AWRA_SPACING_NONE = -1,
  AWRA_SPACING_XS = 0,
  AWRA_SPACING_SM,
  AWRA_SPACING_MD,
  AWRA_SPACING_LG,
  AWRA_SPACING_XL,
  AWRA_SPACING_XXL
} AwraSpacing;
typedef enum {
  AWRA_RADIUS_SMALL = 0,
  AWRA_RADIUS_MEDIUM,
  AWRA_RADIUS_LARGE,
  AWRA_RADIUS_WINDOW,
  AWRA_RADIUS_PILL
} AwraRadius;
typedef enum {
  AWRA_TYPOGRAPHY_BODY = 0,
  AWRA_TYPOGRAPHY_MUTED,
  AWRA_TYPOGRAPHY_EYEBROW,
  AWRA_TYPOGRAPHY_TITLE_3,
  AWRA_TYPOGRAPHY_TITLE_2,
  AWRA_TYPOGRAPHY_TITLE_1,
  AWRA_TYPOGRAPHY_DISPLAY,
  AWRA_TYPOGRAPHY_MONOSPACE,
  AWRA_TYPOGRAPHY_LABEL,
  AWRA_TYPOGRAPHY_READING,
  AWRA_TYPOGRAPHY_TITLE = AWRA_TYPOGRAPHY_TITLE_2,
} AwraTypography;
typedef enum {
  AWRA_APPEARANCE_SYSTEM = 0,
  AWRA_APPEARANCE_LIGHT,
  AWRA_APPEARANCE_DARK
} AwraAppearance;
typedef enum { AWRA_ACCENT_DEFAULT = 0, AWRA_ACCENT_SYSTEM, AWRA_ACCENT_CUSTOM } AwraAccentSource;
typedef enum {
  AWRA_BUTTON_PRIMARY = 0,
  AWRA_BUTTON_SECONDARY,
  AWRA_BUTTON_GHOST,
  AWRA_BUTTON_DESTRUCTIVE,
  AWRA_BUTTON_TOOLBAR
} AwraButtonAppearance;
typedef enum { AWRA_CARD_PLAIN = 0, AWRA_CARD_TINTED, AWRA_CARD_RAISED } AwraCardAppearance;
typedef enum {
  AWRA_BADGE_NEUTRAL = 0,
  AWRA_BADGE_ACCENT,
  AWRA_BADGE_SUCCESS,
  AWRA_BADGE_WARNING,
  AWRA_BADGE_DANGER,
  AWRA_BADGE_INFO
} AwraBadgeAppearance;
typedef enum {
  AWRA_STATUS_INFO = 0,
  AWRA_STATUS_SUCCESS,
  AWRA_STATUS_WARNING,
  AWRA_STATUS_ERROR
} AwraStatusAppearance;
typedef enum {
  AWRA_MATERIAL_CANVAS = 0,
  AWRA_MATERIAL_CONTENT,
  AWRA_MATERIAL_CHROME,
  AWRA_MATERIAL_LAYER,
  AWRA_MATERIAL_FLOATING,
  AWRA_MATERIAL_OPAQUE,
  AWRA_MATERIAL_SOLID,
  AWRA_MATERIAL_TRANSLUCENT,
  AWRA_MATERIAL_FROSTED,
  AWRA_MATERIAL_COUNT
} AwraMaterial;
typedef enum {
  AWRA_ELEVATION_FLAT = 0,
  AWRA_ELEVATION_RAISED,
  AWRA_ELEVATION_FLOATING,
  AWRA_ELEVATION_MODAL
} AwraElevation;
typedef enum {
  AWRA_MOTION_INSTANT = 0,
  AWRA_MOTION_FAST,
  AWRA_MOTION_NORMAL,
  AWRA_MOTION_SLOW,
  AWRA_MOTION_EMPHASIZED
} AwraMotionPreset;
typedef enum { AWRA_POLICY_AUTOMATIC = 0, AWRA_POLICY_ALWAYS, AWRA_POLICY_NEVER, AWRA_POLICY_EXTERNAL } AwraPolicy;
typedef enum {
  AWRA_WRAP_NONE = 0,
  AWRA_WRAP_WORD,
  AWRA_WRAP_CHAR
} AwraWrap;
typedef enum {
  AWRA_ELLIPSIZE_NONE = 0,
  AWRA_ELLIPSIZE_START,
  AWRA_ELLIPSIZE_MIDDLE,
  AWRA_ELLIPSIZE_END
} AwraEllipsize;
typedef enum {
  AWRA_JUSTIFY_LEFT = 0,
  AWRA_JUSTIFY_CENTER,
  AWRA_JUSTIFY_RIGHT,
  AWRA_JUSTIFY_FILL
} AwraJustify;
typedef enum {
  AWRA_TRANSITION_NONE = 0,
  AWRA_TRANSITION_CROSSFADE,
  AWRA_TRANSITION_SLIDE_LEFT_RIGHT,
  AWRA_TRANSITION_SLIDE_UP_DOWN
} AwraTransition;
typedef enum {
  AWRA_SELECTION_NONE = 0,
  AWRA_SELECTION_SINGLE,
  AWRA_SELECTION_BROWSE,
  AWRA_SELECTION_MULTIPLE
} AwraSelectionMode;
typedef enum {
  AWRA_KEYBOARD_LAYOUT_ANSI = 0,
  AWRA_KEYBOARD_LAYOUT_ISO
} AwraKeyboardLayout;

/* Modifier bits in AwraArgs.mods. */
enum { AWRA_MOD_SHIFT = 1, AWRA_MOD_CTRL = 2, AWRA_MOD_ALT = 4, AWRA_MOD_SUPER = 64 };

/* ----------------------------------------------------------------------- */
/* Signals */

typedef enum {
  AWRA_SIGNAL_CLICKED = 1,    /* buttons, rows, cards, items */
  AWRA_SIGNAL_TOGGLED,        /* toggles, switches, checks: args->active */
  AWRA_SIGNAL_VALUE_CHANGED,  /* sliders, spin buttons, volume: args->value */
  AWRA_SIGNAL_SELECTED,       /* dropdowns, segmented, lists, stacks: args->index */
  AWRA_SIGNAL_CHANGED,        /* entries, text views: args->text */
  AWRA_SIGNAL_ACTIVATE,       /* entry Return, row activation: args->index */
  AWRA_SIGNAL_CLOSE_REQUEST,  /* windows, dialogs: return value through args->handled */
  AWRA_SIGNAL_DESTROY,        /* the widget is being freed */
  AWRA_SIGNAL_PRESSED,        /* raw pointer press: args->x, y, button, n_press */
  AWRA_SIGNAL_RELEASED,
  AWRA_SIGNAL_MOTION,
  AWRA_SIGNAL_ENTER,
  AWRA_SIGNAL_LEAVE,
  AWRA_SIGNAL_KEY_PRESSED,    /* args->keysym, text, mods; set handled to stop it */
  AWRA_SIGNAL_KEY_RELEASED,
  AWRA_SIGNAL_SCROLL,         /* args->dx, dy */
  AWRA_SIGNAL_MAP,            /* shown on screen for the first time / again */
  AWRA_SIGNAL_UNMAP,
  AWRA_SIGNAL_RESIZE,         /* allocation size changed: args->width, height */
  AWRA_SIGNAL_SHOW,           /* popovers, dialogs opened */
  AWRA_SIGNAL_HIDE,           /* popovers, dialogs closed */
  AWRA_SIGNAL_FILES_DROPPED,  /* drop zones / windows: args->text = paths, '\n'-separated */
  AWRA_SIGNAL_CAPTURED,       /* shortcut capture: args->text = accelerator */
  AWRA_SIGNAL_SEEK,           /* waveform / media: args->value in seconds or fraction */
  AWRA_SIGNAL_STATE,          /* window state changed: args->index = AWRA_WINDOW_STATE_* */
  AWRA_SIGNAL_SEARCH_CHANGED, /* search entries, after a short delay: args->text */
  AWRA_SIGNAL_STOP_SEARCH,    /* Escape in a search entry */
  AWRA_SIGNAL_NOTIFY,         /* a property changed: args->text = its name */
  AWRA_SIGNAL_MOVED,          /* keyboard views: a key dragged onto another, text = "from\tto" */
  AWRA_SIGNAL_PLAY_REQUESTED, /* media rows */
  AWRA_SIGNAL_STOP_REQUESTED,
  AWRA_SIGNAL_BROWSE_REQUESTED, /* drop zones */
  AWRA_SIGNAL_MARKER_MOVED,   /* waveforms: args->value seconds */
  AWRA_SIGNAL_MARKER_COMMITTED,
  AWRA_SIGNAL_DROP_HOVER,     /* a file drag over the widget: args->active, args->text = key code */
  /* Capture phase, on windows only, before anything else sees the event
   * (GTK's PropagationPhase::Capture): set handled to stop it. */
  AWRA_SIGNAL_KEY_CAPTURE,    /* key press: args->keysym, text, mods, code */
  AWRA_SIGNAL_PRESS_CAPTURE,  /* any button press: args->x, y (window), button */
  /* a link of a label (<a href>) clicked: args->text = its URI; set handled
   * to keep it from opening in the desktop's handler */
  AWRA_SIGNAL_ACTIVATE_LINK,
  /* conversation views: an end reached (args->index 0 top, 1 bottom), its
   * retry clicked, the jump pill clicked */
  AWRA_SIGNAL_EDGE_REACHED,
  AWRA_SIGNAL_RETRY,
  AWRA_SIGNAL_JUMP_ACTIVATED,
  AWRA_SIGNAL_COPY_CLIPBOARD, /* a selectable label's Ctrl+C; handled: no copy */
  AWRA_SIGNAL_COUNT
} AwraSignal;

typedef struct AwraArgs {
  AwraSignal signal;
  bool active;
  double value;
  int index;
  const char *text;
  double x, y; /* logical pixels relative to the widget */
  double dx, dy;
  int button, n_press, mods;
  const char *keysym;
  int width, height;
  bool *handled; /* set *handled = true to stop default handling */
  const char *code; /* key signals: the physical key ("KeyA", "Space"), or NULL */
} AwraArgs;

typedef void (*AwraHandler)(AwraWidget *widget, const AwraArgs *args, void *data);

/* Returns a handler id (> 0). Raw pointer / key signals make any widget
 * receive input (a GtkGestureClick / EventControllerKey equivalent). */
unsigned long awra_signal_connect(AwraWidget *widget, AwraSignal signal, AwraHandler handler,
                                  void *data, AwraDestroyNotify destroy);
void awra_signal_disconnect(AwraWidget *widget, unsigned long id);
void awra_signal_block(AwraWidget *widget, unsigned long id, bool blocked);
/* Emits a signal to its handlers (components emit theirs themselves);
 * true when a handler stopped it (args->handled, if set, is set too). */
bool awra_signal_emit(AwraWidget *widget, const AwraArgs *args);

/* ----------------------------------------------------------------------- */
/* Every widget */

AwraWidget *awra_widget_ref(AwraWidget *widget);
void awra_widget_unref(AwraWidget *widget);
/* Removes the widget from its parent (and closes it when it is a window). */
void awra_widget_destroy(AwraWidget *widget);
const char *awra_widget_type(const AwraWidget *widget);
bool awra_widget_is(const AwraWidget *widget, const char *type);
AwraWidget *awra_widget_parent(const AwraWidget *widget);
AwraWidget *awra_widget_root(const AwraWidget *widget); /* its window, or NULL */
int awra_widget_child_count(const AwraWidget *widget);
AwraWidget *awra_widget_child(const AwraWidget *widget, int index);

void awra_widget_set_visible(AwraWidget *widget, bool visible);
bool awra_widget_visible(const AwraWidget *widget);
bool awra_widget_mapped(const AwraWidget *widget); /* visible, and its ancestors too, in a window */
void awra_widget_set_sensitive(AwraWidget *widget, bool sensitive);
bool awra_widget_sensitive(const AwraWidget *widget);
void awra_widget_set_hexpand(AwraWidget *widget, bool expand);
void awra_widget_set_vexpand(AwraWidget *widget, bool expand);
bool awra_widget_hexpand(const AwraWidget *widget);
bool awra_widget_vexpand(const AwraWidget *widget);
void awra_widget_set_halign(AwraWidget *widget, AwraAlign align);
void awra_widget_set_valign(AwraWidget *widget, AwraAlign align);
void awra_widget_set_margins(AwraWidget *widget, int top, int end, int bottom, int start);
void awra_widget_set_margin_top(AwraWidget *widget, int margin);
void awra_widget_set_margin_bottom(AwraWidget *widget, int margin);
void awra_widget_set_margin_start(AwraWidget *widget, int margin);
void awra_widget_set_margin_end(AwraWidget *widget, int margin);
void awra_widget_set_margin(AwraWidget *widget, AwraSpacing spacing);
/* A margin: side 0 top, 1 end, 2 bottom, 3 start. */
int awra_widget_margin(AwraWidget *widget, int side);
/* The keyboard focus is the widget or inside it. */
bool awra_widget_focus_within(AwraWidget *widget);
void awra_widget_set_size_request(AwraWidget *widget, int width, int height);
void awra_widget_get_size_request(const AwraWidget *widget, int *width, int *height);
/* Minimum and natural size in an orientation (for_size: the other one, -1
 * none), logical pixels. */
void awra_widget_measure(AwraWidget *widget, AwraOrientation orientation, int for_size,
                         int *minimum, int *natural);
void awra_widget_set_tooltip(AwraWidget *widget, const char *text);
void awra_widget_set_typography(AwraWidget *widget, AwraTypography typography);
void awra_widget_set_opacity(AwraWidget *widget, double opacity);
void awra_widget_set_can_target(AwraWidget *widget, bool can_target);
void awra_widget_set_focusable(AwraWidget *widget, bool focusable);
void awra_widget_grab_focus(AwraWidget *widget);
bool awra_widget_has_focus(const AwraWidget *widget);
void awra_widget_set_cursor(AwraWidget *widget, const char *name); /* "pointer", "text", ... */
/* overflow: hidden — children are clipped to the widget's box. */
void awra_widget_set_overflow_hidden(AwraWidget *widget, bool hidden);
/* The pointer's place in the widget (logical px); false when it's elsewhere. */
bool awra_widget_pointer_position(AwraWidget *widget, double *x, double *y);
/* Style roles and classes (Awra 1's add_style_role / add_css_class names
 * that change a component's look: "accent", "muted", "flat", "circular",
 * "dim-label", "numeric", "success", "warning", "error", ...). */
void awra_widget_add_class(AwraWidget *widget, const char *name);
void awra_widget_remove_class(AwraWidget *widget, const char *name);
bool awra_widget_has_class(const AwraWidget *widget, const char *name);
/* Sizes after layout, in logical pixels (0 before the first layout). */
int awra_widget_width(const AwraWidget *widget);
int awra_widget_height(const AwraWidget *widget);
/* x, y of the widget in its window, logical pixels; false when unmapped. */
bool awra_widget_position(const AwraWidget *widget, double *x, double *y);
void awra_widget_queue_draw(AwraWidget *widget);
void awra_widget_queue_resize(AwraWidget *widget);
/* Arbitrary data by key (freed with destroy when replaced or the widget dies). */
void awra_widget_set_data(AwraWidget *widget, const char *key, void *data,
                          AwraDestroyNotify destroy);
void *awra_widget_get_data(const AwraWidget *widget, const char *key);
void awra_widget_set_name(AwraWidget *widget, const char *name);
const char *awra_widget_name(const AwraWidget *widget);
/* The widget under a point of its window (logical pixels), deepest first. */
AwraWidget *awra_widget_pick(AwraWidget *root, double x, double y);
/* Generic properties of components (strings and numbers by name), so
 * bindings and builders reach every component without a function each. */
bool awra_widget_set_property_string(AwraWidget *widget, const char *name, const char *value);
bool awra_widget_set_property_double(AwraWidget *widget, const char *name, double value);
const char *awra_widget_get_property_string(AwraWidget *widget, const char *name);
double awra_widget_get_property_double(AwraWidget *widget, const char *name);

/* ----------------------------------------------------------------------- */
/* Style: tokens, appearance, accent */

typedef struct AwraRGBA {
  float r, g, b, a;
} AwraRGBA;

void awra_style_set_appearance(AwraAppearance appearance);
AwraAppearance awra_style_appearance(void);
bool awra_style_is_dark(void);
void awra_style_set_accent_source(AwraAccentSource source);
void awra_style_set_accent(const AwraRGBA *custom); /* AWRA_ACCENT_CUSTOM */
AwraAccentSource awra_style_accent_source(void);
void awra_style_set_high_contrast(bool on);
void awra_style_set_reduced_motion(bool on);
bool awra_style_reduced_motion(void);
void awra_style_set_reduced_transparency(bool on);
void awra_style_set_native_blur(bool on);
bool awra_style_blur_active(void); /* the compositor blurs the main window now */
/* Material calibration (Awra 1's material lab): tint (NULL = default),
 * opacity in the tint's alpha, grain scale. */
void awra_style_set_material_tint(AwraMaterial material, const AwraRGBA *tint);
void awra_style_set_grain_scale(double scale);
void awra_style_reset(void);
/* Token values: spacing and radius in logical pixels, colors by name
 * ("foreground", "muted", "accent", "accent-content", "on-accent",
 * "danger", "success", "warning", "outline", "separator", "control-cap",
 * "control-hover", "control-pressed", "control-well", "control-thumb",
 * "control-track", "selection", "focus-ring", "canvas", "floating", ...). */
int awra_spacing(AwraSpacing spacing);
int awra_radius(AwraRadius radius);
bool awra_style_color(const char *name, AwraRGBA *r_color);
/* Called after the style (appearance, accent, tokens) changed. */
unsigned long awra_style_connect_changed(void (*fn)(void *data), void *data);
void awra_style_disconnect(unsigned long id);
unsigned awra_motion_duration(AwraMotionPreset preset); /* ms; 0 with reduced motion */
/* The UI font: family and size in points, as the desktop sets it (GTK's
 * gtk-font-name on Linux, the message font on Windows). */
void awra_style_set_font(const char *family, double points);
const char *awra_style_font_family(void);
double awra_style_font_size(void); /* the base size in logical pixels */

/* ----------------------------------------------------------------------- */
/* Windows */

typedef enum {
  AWRA_WINDOW_STATE_MAXIMIZED = 1 << 0,
  AWRA_WINDOW_STATE_FULLSCREEN = 1 << 1,
  AWRA_WINDOW_STATE_ACTIVE = 1 << 2,
  AWRA_WINDOW_STATE_TILED = 1 << 3,
  AWRA_WINDOW_STATE_BLUR = 1 << 4,
  AWRA_WINDOW_STATE_HIDDEN = 1 << 5,
} AwraWindowState;

AwraWidget *awra_window_new(const char *title, int width, int height);
void awra_window_set_title(AwraWidget *window, const char *title);
const char *awra_window_title(AwraWidget *window);
void awra_window_set_child(AwraWidget *window, AwraWidget *child);
AwraWidget *awra_window_child(AwraWidget *window);
/* The title bar: an AwraHeader (default: one with the title and window
 * controls) or any widget; NULL removes it. */
void awra_window_set_titlebar(AwraWidget *window, AwraWidget *titlebar);
AwraWidget *awra_window_titlebar(AwraWidget *window);
/* The title bar floats over the content, which takes the whole height
 * (Awra 1's overlay-chrome). */
void awra_window_set_overlay_chrome(AwraWidget *window, bool overlaid);
void awra_window_present(AwraWidget *window);
void awra_window_hide(AwraWidget *window);
void awra_window_close(AwraWidget *window); /* asks: close-request handlers may refuse */
void awra_window_set_default_size(AwraWidget *window, int width, int height);
void awra_window_get_size(AwraWidget *window, int *width, int *height);
void awra_window_set_min_size(AwraWidget *window, int width, int height);
void awra_window_set_resizable(AwraWidget *window, bool resizable);
void awra_window_maximize(AwraWidget *window);
void awra_window_unmaximize(AwraWidget *window);
void awra_window_minimize(AwraWidget *window);
void awra_window_fullscreen(AwraWidget *window, bool fullscreen);
int awra_window_state(AwraWidget *window); /* AwraWindowState bits */
bool awra_window_is_active(AwraWidget *window);
void awra_window_set_hide_on_close(AwraWidget *window, bool hide);
void awra_window_set_keep_rounded_corners(AwraWidget *window, bool keep);
/* The widget with the keyboard focus (NULL: none); set NULL to drop it. */
AwraWidget *awra_window_focus(AwraWidget *window);
void awra_window_set_focus(AwraWidget *window, AwraWidget *widget);
/* Keyboard shortcuts: "<Ctrl>k", "<Ctrl><Shift>z", "Escape", "F11". */
unsigned long awra_window_add_shortcut(AwraWidget *window, const char *accel, AwraHandler handler,
                                       void *data, AwraDestroyNotify destroy);
void awra_window_remove_shortcut(AwraWidget *window, unsigned long id);
/* Renders the window's current frame to a PNG (tests, screenshots). */
bool awra_window_snapshot_png(AwraWidget *window, const char *path);
/* Opens no native window: the window renders offscreen at width x height
 * (tests, benchmarks, CI); awra_window_render_offscreen draws a frame. */
AwraWidget *awra_window_new_offscreen(const char *title, int width, int height, double scale);
int awra_window_render_offscreen(AwraWidget *window, bool full);
/* Native handle: wl_surface *, X11 Window, HWND. */
void *awra_window_native_handle(AwraWidget *window);
/* Every window, for "present the existing one" logic. */
int awra_window_count(void);
AwraWidget *awra_window_nth(int index);
/* Raster statistics for benchmarks: frames drawn and redrawn pixel area. */
void awra_window_stats(AwraWidget *window, uint64_t *frames, uint64_t *pixels);

/* ----------------------------------------------------------------------- */
/* Layout containers */

AwraWidget *awra_box_new(AwraOrientation orientation, int spacing);
void awra_box_append(AwraWidget *box, AwraWidget *child);
void awra_box_prepend(AwraWidget *box, AwraWidget *child);
void awra_box_insert_after(AwraWidget *box, AwraWidget *child, AwraWidget *sibling);
void awra_box_remove(AwraWidget *box, AwraWidget *child);
void awra_box_remove_all(AwraWidget *box);
void awra_box_set_spacing(AwraWidget *box, int spacing);
void awra_box_set_homogeneous(AwraWidget *box, bool homogeneous);
void awra_box_set_orientation(AwraWidget *box, AwraOrientation orientation);
/* Generic container ops (box, grid, flow box, stack, overlay, ...). */
void awra_container_add(AwraWidget *container, AwraWidget *child);
void awra_container_remove(AwraWidget *container, AwraWidget *child);
void awra_container_remove_all(AwraWidget *container);

AwraWidget *awra_grid_new(void);
void awra_grid_attach(AwraWidget *grid, AwraWidget *child, int column, int row, int width,
                      int height);
void awra_grid_set_row_spacing(AwraWidget *grid, int spacing);
void awra_grid_set_column_spacing(AwraWidget *grid, int spacing);
void awra_grid_set_homogeneous(AwraWidget *grid, bool rows, bool columns);

AwraWidget *awra_flow_box_new(void);
void awra_flow_box_append(AwraWidget *flow, AwraWidget *child);
void awra_flow_box_insert(AwraWidget *flow, AwraWidget *child, int position);
void awra_flow_box_set_spacing(AwraWidget *flow, int row_spacing, int column_spacing);
void awra_flow_box_set_columns(AwraWidget *flow, int min_per_line, int max_per_line);
void awra_flow_box_set_homogeneous(AwraWidget *flow, bool homogeneous);
void awra_flow_box_set_selection_mode(AwraWidget *flow, AwraSelectionMode mode);
void awra_flow_box_select(AwraWidget *flow, AwraWidget *child);
AwraWidget *awra_flow_box_selected(AwraWidget *flow);
/* Show only children for which keep(child, data) is true. */
void awra_flow_box_set_filter(AwraWidget *flow, bool (*keep)(AwraWidget *child, void *data),
                              void *data, AwraDestroyNotify destroy);
void awra_flow_box_invalidate_filter(AwraWidget *flow);

AwraWidget *awra_stack_new(void);
void awra_stack_add_named(AwraWidget *stack, AwraWidget *child, const char *name);
void awra_stack_set_visible_child(AwraWidget *stack, AwraWidget *child);
void awra_stack_set_visible_child_name(AwraWidget *stack, const char *name);
AwraWidget *awra_stack_visible_child(AwraWidget *stack);
const char *awra_stack_visible_child_name(AwraWidget *stack);
AwraWidget *awra_stack_child_by_name(AwraWidget *stack, const char *name);
void awra_stack_set_transition(AwraWidget *stack, AwraTransition transition, unsigned ms);
void awra_stack_set_hhomogeneous(AwraWidget *stack, bool homogeneous);
void awra_stack_set_vhomogeneous(AwraWidget *stack, bool homogeneous);

AwraWidget *awra_overlay_new(void);
void awra_overlay_set_child(AwraWidget *overlay, AwraWidget *child);
void awra_overlay_add_overlay(AwraWidget *overlay, AwraWidget *child);
/* GtkOverlay::get-child-position: places an overlay child (logical px,
 * relative to the overlay; it may reach outside it); false: the default. */
typedef bool (*AwraOverlayPositionFunc)(AwraWidget *overlay, AwraWidget *child, int *x, int *y,
                                        int *width, int *height, void *data);
void awra_overlay_set_position_func(AwraWidget *overlay, AwraOverlayPositionFunc fn, void *data,
                                    AwraDestroyNotify destroy);

AwraWidget *awra_scrolled_new(void);
void awra_scrolled_set_child(AwraWidget *scrolled, AwraWidget *child);
AwraWidget *awra_scrolled_child(AwraWidget *scrolled);
void awra_scrolled_set_policy(AwraWidget *scrolled, AwraPolicy horizontal, AwraPolicy vertical);
void awra_scrolled_set_propagate_natural(AwraWidget *scrolled, bool width, bool height);
void awra_scrolled_set_max_content_height(AwraWidget *scrolled, int height);
void awra_scrolled_set_min_content_height(AwraWidget *scrolled, int height);
double awra_scrolled_vvalue(AwraWidget *scrolled);
void awra_scrolled_set_vvalue(AwraWidget *scrolled, double value);
double awra_scrolled_hvalue(AwraWidget *scrolled);
void awra_scrolled_set_hvalue(AwraWidget *scrolled, double value);
/* Scrolls so that `child` (a descendant) is visible. */
void awra_scrolled_scroll_to(AwraWidget *scrolled, AwraWidget *child);
/* The vertical range (GtkAdjustment's value, upper and page size), logical
 * px; value-changed is emitted when the value changes. */
void awra_scrolled_get_vadjustment(AwraWidget *scrolled, double *value, double *upper, double *page);

AwraWidget *awra_separator_new(AwraOrientation orientation);
AwraWidget *awra_spacer_new(void); /* an empty expanding widget */

/* ----------------------------------------------------------------------- */
/* Text and images */

AwraWidget *awra_label_new(const char *text);
void awra_label_set_text(AwraWidget *label, const char *text);
const char *awra_label_text(AwraWidget *label);
/* A subset of Pango markup: <b> <i> <u> <s> <tt> <small> <big> <span
 * weight= foreground= alpha= size= font_family=>, entities. */
void awra_label_set_markup(AwraWidget *label, const char *markup);
void awra_label_set_wrap(AwraWidget *label, AwraWrap wrap);
void awra_label_set_ellipsize(AwraWidget *label, AwraEllipsize ellipsize);
void awra_label_set_xalign(AwraWidget *label, float xalign);
void awra_label_set_yalign(AwraWidget *label, float yalign);
void awra_label_set_justify(AwraWidget *label, AwraJustify justify);
void awra_label_set_lines(AwraWidget *label, int lines);
void awra_label_set_max_width_chars(AwraWidget *label, int chars);
void awra_label_set_width_chars(AwraWidget *label, int chars);
void awra_label_set_selectable(AwraWidget *label, bool selectable);
void awra_label_set_color(AwraWidget *label, const AwraRGBA *color); /* NULL: typography's */
/* The link under the pointer (its href), or NULL. */
const char *awra_label_current_uri(AwraWidget *label);
void awra_label_set_weight(AwraWidget *label, int weight);           /* 0: typography's */
void awra_label_set_scale(AwraWidget *label, double scale);          /* font size factor */
void awra_label_set_monospace_digits(AwraWidget *label, bool tabular);
/* Selectable labels: the selection, in byte offsets (end -1: the end). */
bool awra_label_selection_bounds(AwraWidget *label, int *start, int *end);
void awra_label_select_region(AwraWidget *label, int start, int end);

AwraWidget *awra_image_new(void);
AwraWidget *awra_image_new_from_icon_name(const char *icon_name);
void awra_image_set_icon_name(AwraWidget *image, const char *icon_name);
void awra_image_set_pixel_size(AwraWidget *image, int size);
void awra_image_set_file(AwraWidget *image, const char *path);
/* Straight RGBA pixels (copied). */
void awra_image_set_rgba(AwraWidget *image, const uint8_t *rgba, int width, int height, int stride);
void awra_image_set_bytes(AwraWidget *image, const void *encoded, size_t length);
void awra_image_set_corner_radius(AwraWidget *image, double radius);
void awra_image_set_content_fit(AwraWidget *image, int fit); /* 0 fill 1 contain 2 cover */
void awra_image_set_color(AwraWidget *image, const AwraRGBA *color); /* icon tint; NULL: text color */
/* Registers an icon (SVG source) under a name; Awra ships the symbolic
 * icons its components and Adwaita's common names use. */
bool awra_icon_register_svg(const char *name, const char *svg, size_t length);
bool awra_icon_exists(const char *name);

/* ----------------------------------------------------------------------- */
/* Buttons and toggles */

AwraWidget *awra_button_new(void);
AwraWidget *awra_button_new_with_label(const char *label);
AwraWidget *awra_button_new_from_icon_name(const char *icon_name);
void awra_button_set_label(AwraWidget *button, const char *label);
const char *awra_button_label(AwraWidget *button);
void awra_button_set_icon_name(AwraWidget *button, const char *icon_name);
void awra_button_set_child(AwraWidget *button, AwraWidget *child);
void awra_button_set_appearance(AwraWidget *button, AwraButtonAppearance appearance);
AwraButtonAppearance awra_button_appearance(AwraWidget *button);
void awra_button_set_has_frame(AwraWidget *button, bool frame);

AwraWidget *awra_toggle_button_new(void);
AwraWidget *awra_toggle_button_new_with_label(const char *label);
void awra_toggle_button_set_active(AwraWidget *toggle, bool active);
bool awra_toggle_button_active(AwraWidget *toggle);
/* Toggles of one group: activating one deactivates the others. */
void awra_toggle_button_set_group(AwraWidget *toggle, AwraWidget *group_member);

AwraWidget *awra_check_button_new(void);
AwraWidget *awra_check_button_new_with_label(const char *label);
void awra_check_button_set_active(AwraWidget *check, bool active);
bool awra_check_button_active(AwraWidget *check);
void awra_check_button_set_label(AwraWidget *check, const char *label);
void awra_check_button_set_group(AwraWidget *check, AwraWidget *group_member); /* radios */
void awra_check_button_set_inconsistent(AwraWidget *check, bool inconsistent);

AwraWidget *awra_switch_new(void);
void awra_switch_set_active(AwraWidget *sw, bool active);
bool awra_switch_active(AwraWidget *sw);

AwraWidget *awra_segmented_control_new(void);
int awra_segmented_control_append(AwraWidget *control, const char *label, const char *icon_name);
void awra_segmented_control_set_selected(AwraWidget *control, int index);
int awra_segmented_control_selected(AwraWidget *control);
void awra_segmented_control_remove_all(AwraWidget *control);

AwraWidget *awra_menu_button_new(void);
void awra_menu_button_set_icon_name(AwraWidget *button, const char *icon_name);
void awra_menu_button_set_label(AwraWidget *button, const char *label);
void awra_menu_button_set_popover(AwraWidget *button, AwraWidget *popover);
AwraWidget *awra_menu_button_popover(AwraWidget *button);
void awra_menu_button_set_appearance(AwraWidget *button, AwraButtonAppearance appearance);
/* A menu built from items: label NULL = separator. */
typedef struct AwraMenuItem {
  const char *label;
  const char *icon_name;
  const char *accel;     /* shown on the right, e.g. "Ctrl+K" */
  int check;             /* -1 none, 0/1 a check mark, -2 a section heading, -3 a slot
                            for the application's widget, named `label` */
  bool sensitive;
  bool destructive;
  AwraHandler handler;   /* called with args->index = the item's index */
  void *data;
} AwraMenuItem;
void awra_menu_button_set_menu(AwraWidget *button, const AwraMenuItem *items, int n);

/* ----------------------------------------------------------------------- */
/* Inputs */

AwraWidget *awra_entry_new(void);
void awra_entry_set_text(AwraWidget *entry, const char *text);
const char *awra_entry_text(AwraWidget *entry);
void awra_entry_set_placeholder(AwraWidget *entry, const char *text);
void awra_entry_set_icon_name(AwraWidget *entry, const char *icon_name); /* leading icon */
void awra_entry_set_visibility(AwraWidget *entry, bool visible);           /* passwords */
void awra_entry_set_max_length(AwraWidget *entry, int chars);
void awra_entry_select_all(AwraWidget *entry);
void awra_entry_set_position(AwraWidget *entry, int position);
void awra_entry_set_accel_hint(AwraWidget *entry, const char *hint); /* "Ctrl K" at the end */
void awra_entry_set_editable(AwraWidget *entry, bool editable);
void awra_entry_set_quiet(AwraWidget *entry, bool quiet); /* sidebar search look */

AwraWidget *awra_search_entry_new(void);
void awra_search_entry_set_delay(AwraWidget *entry, unsigned ms);

AwraWidget *awra_text_view_new(void); /* multi-line */
void awra_text_view_set_text(AwraWidget *view, const char *text);
const char *awra_text_view_text(AwraWidget *view);
void awra_text_view_set_placeholder(AwraWidget *view, const char *text);
void awra_text_view_set_editable(AwraWidget *view, bool editable);
void awra_text_view_set_wrap(AwraWidget *view, AwraWrap wrap);
/* Editing entries and text views by character offsets (GtkEditable,
 * GtkTextBuffer); -1 means the end, or (insert) the cursor, replacing the
 * selection. notify carries "cursor-position" when the caret or the
 * selection moves. */
int awra_editable_length(AwraWidget *editable);
int awra_editable_cursor(AwraWidget *editable);
void awra_editable_set_cursor(AwraWidget *editable, int offset);
bool awra_editable_selection(AwraWidget *editable, int *start, int *end);
void awra_editable_select(AwraWidget *editable, int start, int end);
void awra_editable_insert(AwraWidget *editable, int offset, const char *text);
void awra_editable_delete(AwraWidget *editable, int start, int end);
/* The caret's top, left and height in the widget (logical px). */
void awra_editable_caret_rect(AwraWidget *editable, double *x, double *y, double *height);

AwraWidget *awra_slider_new(double min, double max, double step);
void awra_slider_set_value(AwraWidget *slider, double value);
double awra_slider_value(AwraWidget *slider);
void awra_slider_set_range(AwraWidget *slider, double min, double max);
void awra_slider_set_orientation(AwraWidget *slider, AwraOrientation orientation);
/* A tick under the trough at a value (GtkScale's marks; up to 8). */
void awra_slider_add_mark(AwraWidget *slider, double value);
void awra_slider_clear_marks(AwraWidget *slider);

AwraWidget *awra_spin_button_new(double min, double max, double step);
void awra_spin_button_set_value(AwraWidget *spin, double value);
double awra_spin_button_value(AwraWidget *spin);
void awra_spin_button_set_digits(AwraWidget *spin, int digits);
void awra_spin_button_set_range(AwraWidget *spin, double min, double max);

AwraWidget *awra_dropdown_new(const char *const *items, int n);
void awra_dropdown_set_items(AwraWidget *dropdown, const char *const *items, int n);
void awra_dropdown_set_selected(AwraWidget *dropdown, int index);
int awra_dropdown_selected(AwraWidget *dropdown);
const char *awra_dropdown_selected_text(AwraWidget *dropdown);
void awra_dropdown_set_enable_search(AwraWidget *dropdown, bool search);

AwraWidget *awra_volume_control_new(void);
void awra_volume_control_set_value(AwraWidget *volume, double value); /* 0..1 */
double awra_volume_control_value(AwraWidget *volume);
void awra_volume_control_set_muted(AwraWidget *volume, bool muted);
bool awra_volume_control_muted(AwraWidget *volume);

AwraWidget *awra_shortcut_capture_new(void);
void awra_shortcut_capture_set_accelerator(AwraWidget *capture, const char *accel);
const char *awra_shortcut_capture_accelerator(AwraWidget *capture);
void awra_shortcut_capture_set_placeholder(AwraWidget *capture, const char *text);
void awra_shortcut_capture_start(AwraWidget *capture);
bool awra_shortcut_capture_capturing(AwraWidget *capture);
void awra_shortcut_capture_set_allow_chords(AwraWidget *capture, bool allow);
void awra_shortcut_capture_clear(AwraWidget *capture);
/* The physical keys of the last capture ("ControlLeft", "KeyA"). */
int awra_shortcut_capture_codes(AwraWidget *capture, const char *const **r_codes);
void awra_shortcut_capture_set_display_labels(AwraWidget *capture, const char *const *labels, int n);
/* The key symbols the last capture's keys typed ("Control_L", "a"), one
 * per code; 0 when the codes were set rather than captured. */
int awra_shortcut_capture_keysyms(AwraWidget *capture, const char *const **r_keysyms);
/* Shows these physical keys as the shortcut (labels from the layout). */
void awra_shortcut_capture_set_codes(AwraWidget *capture, const char *const *codes, int n);

/* ----------------------------------------------------------------------- */
/* Composition */

AwraWidget *awra_card_new(void);
void awra_card_set_child(AwraWidget *card, AwraWidget *child);
void awra_card_set_appearance(AwraWidget *card, AwraCardAppearance appearance);
void awra_card_set_activatable(AwraWidget *card, bool activatable); /* clicked on press */
void awra_card_set_padding(AwraWidget *card, AwraSpacing spacing);

AwraWidget *awra_surface_new(AwraMaterial material);
void awra_surface_set_child(AwraWidget *surface, AwraWidget *child);
void awra_surface_set_material(AwraWidget *surface, AwraMaterial material);
void awra_surface_set_elevation(AwraWidget *surface, AwraElevation elevation);
void awra_surface_set_radius(AwraWidget *surface, double radius); /* < 0: the material's */
void awra_surface_set_corner_mask(AwraWidget *surface, int mask); /* bits: TL 1 TR 2 BR 4 BL 8 */
void awra_surface_set_padding(AwraWidget *surface, int top, int end, int bottom, int start);

AwraWidget *awra_page_new(void);
void awra_page_set_child(AwraWidget *page, AwraWidget *child);
void awra_page_set_header(AwraWidget *page, AwraWidget *header);
void awra_page_set_scrollable(AwraWidget *page, bool scrollable);
void awra_page_set_max_content_width(AwraWidget *page, int width); /* default 960 */
AwraWidget *awra_page_header_new(const char *title, const char *subtitle);
void awra_page_header_set_title(AwraWidget *header, const char *title);
void awra_page_header_set_subtitle(AwraWidget *header, const char *subtitle);
void awra_page_header_add_action(AwraWidget *header, AwraWidget *action);

AwraWidget *awra_section_new(const char *title);
void awra_section_set_title(AwraWidget *section, const char *title);
void awra_section_set_description(AwraWidget *section, const char *description);
void awra_section_append(AwraWidget *section, AwraWidget *child);
void awra_section_set_header_suffix(AwraWidget *section, AwraWidget *suffix);

AwraWidget *awra_form_new(void);
void awra_form_append(AwraWidget *form, AwraWidget *row);
AwraWidget *awra_form_row_new(const char *title, const char *subtitle);
void awra_form_row_set_control(AwraWidget *row, AwraWidget *control);
void awra_form_row_set_title(AwraWidget *row, const char *title);
void awra_form_row_set_subtitle(AwraWidget *row, const char *subtitle);
void awra_form_row_set_vertical(AwraWidget *row, bool vertical);
typedef enum {
  AWRA_VALIDATION_NONE,
  AWRA_VALIDATION_SUCCESS,
  AWRA_VALIDATION_WARNING,
  AWRA_VALIDATION_ERROR,
} AwraValidationState;
/* The message shown, in the danger colour, in place of the help text while
 * the state is an error; the state also colours the control's border. */
void awra_form_row_set_error_message(AwraWidget *row, const char *message);
void awra_form_row_set_validation_state(AwraWidget *row, AwraValidationState state);

AwraWidget *awra_action_row_new(const char *title, const char *subtitle);
void awra_action_row_set_title(AwraWidget *row, const char *title);
void awra_action_row_set_subtitle(AwraWidget *row, const char *subtitle);
void awra_action_row_set_icon_name(AwraWidget *row, const char *icon_name);
void awra_action_row_set_prefix(AwraWidget *row, AwraWidget *prefix);
void awra_action_row_add_suffix(AwraWidget *row, AwraWidget *suffix);
void awra_action_row_set_activatable(AwraWidget *row, bool activatable);

AwraWidget *awra_badge_new(const char *label, AwraBadgeAppearance appearance);
void awra_badge_set_label(AwraWidget *badge, const char *label);
void awra_badge_set_appearance(AwraWidget *badge, AwraBadgeAppearance appearance);
void awra_badge_set_icon_name(AwraWidget *badge, const char *icon_name);
void awra_badge_set_color(AwraWidget *badge, const AwraRGBA *color); /* NULL: the appearance's */

AwraWidget *awra_status_banner_new(const char *title, const char *description,
                                   AwraStatusAppearance appearance);
void awra_status_banner_set_title(AwraWidget *banner, const char *title);
void awra_status_banner_set_description(AwraWidget *banner, const char *description);
void awra_status_banner_set_appearance(AwraWidget *banner, AwraStatusAppearance appearance);
void awra_status_banner_add_action(AwraWidget *banner, AwraWidget *action);
void awra_status_banner_set_dismissible(AwraWidget *banner, bool dismissible);

AwraWidget *awra_empty_state_new(const char *icon_name, const char *title,
                                 const char *description);
void awra_empty_state_set_title(AwraWidget *state, const char *title);
void awra_empty_state_set_description(AwraWidget *state, const char *description);
void awra_empty_state_set_icon_name(AwraWidget *state, const char *icon_name);
void awra_empty_state_add_action(AwraWidget *state, AwraWidget *action);

AwraWidget *awra_loading_state_new(const char *title, const char *description);
void awra_loading_state_set_title(AwraWidget *state, const char *title);
void awra_loading_state_set_description(AwraWidget *state, const char *description);
AwraWidget *awra_spinner_new(void);
void awra_spinner_set_spinning(AwraWidget *spinner, bool spinning);

AwraWidget *awra_progress_bar_new(void);
void awra_progress_bar_set_fraction(AwraWidget *bar, double fraction);
double awra_progress_bar_fraction(AwraWidget *bar);
void awra_progress_bar_pulse(AwraWidget *bar);
void awra_progress_bar_set_text(AwraWidget *bar, const char *text);

/* ----------------------------------------------------------------------- */
/* Navigation and chrome */

AwraWidget *awra_header_new(void);
void awra_header_set_title(AwraWidget *header, const char *title);
void awra_header_set_subtitle(AwraWidget *header, const char *subtitle);
void awra_header_pack_start(AwraWidget *header, AwraWidget *child);
void awra_header_pack_end(AwraWidget *header, AwraWidget *child);
void awra_header_set_center(AwraWidget *header, AwraWidget *child);
void awra_header_set_show_controls(AwraWidget *header, bool show);
void awra_header_set_blend_with_window(AwraWidget *header, bool blend);
void awra_header_set_drag_enabled(AwraWidget *header, bool drag);
void awra_header_set_compact(AwraWidget *header, bool compact);
/* The material it paints when it doesn't blend with the window (Chrome). */
void awra_header_set_material(AwraWidget *header, AwraMaterial material);

AwraWidget *awra_sidebar_new(void);
void awra_sidebar_set_child(AwraWidget *sidebar, AwraWidget *child);
void awra_sidebar_append(AwraWidget *sidebar, AwraWidget *child);
void awra_sidebar_append_heading(AwraWidget *sidebar, const char *title);
void awra_sidebar_set_footer(AwraWidget *sidebar, AwraWidget *footer);
void awra_sidebar_set_inset(AwraWidget *sidebar, AwraSpacing inset);

AwraWidget *awra_navigation_item_new(const char *label, const char *icon_name);
void awra_navigation_item_set_label(AwraWidget *item, const char *label);
const char *awra_navigation_item_label(AwraWidget *item);
/* Custom content in place of the label and subtitle. */
void awra_navigation_item_set_child(AwraWidget *item, AwraWidget *child);
void awra_navigation_item_set_icon_name(AwraWidget *item, const char *icon_name);
void awra_navigation_item_set_selected(AwraWidget *item, bool selected);
bool awra_navigation_item_selected(AwraWidget *item);
void awra_navigation_item_set_badge(AwraWidget *item, const char *badge);
/* Items of one group: selecting one unselects the others. */
void awra_navigation_item_set_group(AwraWidget *item, AwraWidget *group_member);
/* Before the label (an avatar, a glyph), after it (a count, a button). */
void awra_navigation_item_set_prefix(AwraWidget *item, AwraWidget *prefix);
AwraWidget *awra_navigation_item_prefix(AwraWidget *item);
void awra_navigation_item_set_suffix(AwraWidget *item, AwraWidget *suffix);
AwraWidget *awra_navigation_item_suffix(AwraWidget *item);
void awra_navigation_item_set_subtitle(AwraWidget *item, const char *subtitle);
/* Silenced: it recedes until hovered or selected. */
void awra_navigation_item_set_muted(AwraWidget *item, bool muted);
/* Something new: the label at full contrast and weight. */
void awra_navigation_item_set_emphasized(AwraWidget *item, bool emphasized);
/* The label in a colour (a role's), made readable; NULL: the item's. */
void awra_navigation_item_set_label_color(AwraWidget *item, const AwraRGBA *color);
/* Items inside a widget with the "compact-navigation" class are 32 px rows;
 * inside "people-list", names at full contrast. */

AwraWidget *awra_split_view_new(void);
void awra_split_view_set_sidebar(AwraWidget *split, AwraWidget *sidebar);
void awra_split_view_set_content(AwraWidget *split, AwraWidget *content);
void awra_split_view_set_sidebar_width(AwraWidget *split, int width);
void awra_split_view_set_show_sidebar(AwraWidget *split, bool show);
void awra_split_view_set_end_pane(AwraWidget *split, AwraWidget *pane);
void awra_split_view_set_end_pane_width(AwraWidget *split, int width);
AwraWidget *awra_split_view_sidebar(AwraWidget *split);
AwraWidget *awra_split_view_content(AwraWidget *split);

/* ----------------------------------------------------------------------- */
/* Overlays */

/* A floating panel anchored to a widget (Floating material, blurred). */
AwraWidget *awra_popover_new(void);
void awra_popover_set_child(AwraWidget *popover, AwraWidget *child);
void awra_popover_set_anchor(AwraWidget *popover, AwraWidget *anchor);
void awra_popover_popup(AwraWidget *popover);
void awra_popover_popup_at(AwraWidget *popover, AwraWidget *relative_to, double x, double y);
void awra_popover_popdown(AwraWidget *popover);
bool awra_popover_visible(AwraWidget *popover);
void awra_popover_set_position(AwraWidget *popover, int side); /* 0 bottom 1 top 2 start 3 end */
/* A popover menu of items, at a point of `relative_to` (context menus). */
AwraWidget *awra_menu_popover_new(const AwraMenuItem *items, int n);
void awra_context_menu_attach(AwraWidget *widget, const AwraMenuItem *items, int n);

AwraWidget *awra_dialog_new(const char *title);
void awra_dialog_set_child(AwraWidget *dialog, AwraWidget *child);
void awra_dialog_set_body(AwraWidget *dialog, const char *text);
/* Buttons at the bottom; the handler gets args->index = the response. */
void awra_dialog_add_response(AwraWidget *dialog, int response, const char *label,
                              AwraButtonAppearance appearance);
void awra_dialog_present(AwraWidget *dialog, AwraWidget *parent_window);
void awra_dialog_close(AwraWidget *dialog);
void awra_dialog_set_content_width(AwraWidget *dialog, int width);
void awra_dialog_set_response_appearance(AwraWidget *dialog, int response,
                                         AwraButtonAppearance appearance);
void awra_dialog_set_response_enabled(AwraWidget *dialog, int response, bool enabled);
void awra_dialog_set_response_label(AwraWidget *dialog, int response, const char *label);
/* A response at work: a spinner in its button, which takes no clicks. */
void awra_dialog_set_response_busy(AwraWidget *dialog, int response, bool busy);
/* Return gives this response. */
void awra_dialog_set_default_response(AwraWidget *dialog, int response);

AwraWidget *awra_toast_overlay_new(void);
void awra_toast_overlay_set_child(AwraWidget *overlay, AwraWidget *child);
/* A toast; timeout 0 = until dismissed. Returns its id. */
unsigned awra_toast_overlay_show(AwraWidget *overlay, const char *title, unsigned timeout_ms);
unsigned awra_toast_overlay_show_with_action(AwraWidget *overlay, const char *title,
                                             const char *action_label, AwraHandler handler,
                                             void *data, unsigned timeout_ms);
void awra_toast_overlay_dismiss(AwraWidget *overlay, unsigned id);
/* Space kept clear under the toasts (logical px), say for a composer. */
void awra_toast_overlay_set_bottom_inset(AwraWidget *overlay, int inset);

typedef struct AwraCommand {
  const char *title;
  const char *subtitle;
  const char *icon_name;
  const char *accel;
  const char *section;
} AwraCommand;
/* activate: args->index = the command's index */
AwraWidget *awra_command_palette_new(void);
void awra_command_palette_set_commands(AwraWidget *palette, const AwraCommand *commands, int n);
void awra_command_palette_set_placeholder(AwraWidget *palette, const char *text);
void awra_command_palette_set_title(AwraWidget *palette, const char *title);
void awra_command_palette_set_empty_text(AwraWidget *palette, const char *text);
void awra_command_palette_present(AwraWidget *palette, AwraWidget *window);
void awra_command_palette_close(AwraWidget *palette);
void awra_command_palette_set_query(AwraWidget *palette, const char *query);

/* ----------------------------------------------------------------------- */
/* Media and specialised components */

/* A key cap (keyboards, shortcut chips). `code` names the physical key
 * ("KeyQ", "ControlLeft"; W3C KeyboardEvent.code names). */
AwraWidget *awra_key_cap_new(const char *code, const char *label);
const char *awra_key_cap_code(AwraWidget *cap);
void awra_key_cap_set_label(AwraWidget *cap, const char *label);
void awra_key_cap_set_secondary_label(AwraWidget *cap, const char *label);
void awra_key_cap_set_badge(AwraWidget *cap, const char *badge);
void awra_key_cap_set_bound(AwraWidget *cap, bool bound);
void awra_key_cap_set_active(AwraWidget *cap, bool active);   /* playing: glow */
void awra_key_cap_set_selected(AwraWidget *cap, bool selected);
void awra_key_cap_set_dimmed(AwraWidget *cap, bool dimmed);
void awra_key_cap_set_tint(AwraWidget *cap, const AwraRGBA *tint);
void awra_key_cap_set_edge_color(AwraWidget *cap, const AwraRGBA *color);
void awra_key_cap_set_width_units(AwraWidget *cap, double units);
void awra_key_cap_set_height_units(AwraWidget *cap, double units);
void awra_key_cap_set_drop_highlight(AwraWidget *cap, bool on);
void awra_key_cap_flash(AwraWidget *cap);
void awra_key_cap_set_compact(AwraWidget *cap, bool compact);

/* A physical keyboard of key caps; selected: args->index = count. */
AwraWidget *awra_keyboard_view_new(AwraKeyboardLayout layout);
void awra_keyboard_view_set_layout(AwraWidget *view, AwraKeyboardLayout layout);
void awra_keyboard_view_set_show_function_row(AwraWidget *view, bool show);
void awra_keyboard_view_set_show_navigation(AwraWidget *view, bool show);
void awra_keyboard_view_set_show_numpad(AwraWidget *view, bool show);
bool awra_keyboard_view_shows_navigation(AwraWidget *view);
void awra_keyboard_view_set_selection_mode(AwraWidget *view, AwraSelectionMode mode);
void awra_keyboard_view_set_keys_movable(AwraWidget *view, bool movable);
void awra_keyboard_view_set_accepts_file_drops(AwraWidget *view, bool accepts);
AwraWidget *awra_keyboard_view_key_cap(AwraWidget *view, const char *code);
/* The key codes shown, or selected; NULL-terminated, valid until the next change. */
const char *const *awra_keyboard_view_codes(AwraWidget *view);
const char *const *awra_keyboard_view_selected_codes(AwraWidget *view);
void awra_keyboard_view_set_selected_codes(AwraWidget *view, const char *const *codes, int n);
void awra_keyboard_view_unselect_all(AwraWidget *view);
const char *awra_keyboard_view_code_at(AwraWidget *view, double x, double y);

AwraWidget *awra_waveform_view_new(void);
void awra_waveform_view_set_peaks(AwraWidget *view, const float *peaks, int n);
void awra_waveform_view_set_expected_peaks(AwraWidget *view, int n);
void awra_waveform_view_set_duration(AwraWidget *view, double seconds);
void awra_waveform_view_set_position(AwraWidget *view, double seconds); /* < 0: none */
void awra_waveform_view_set_hint_position(AwraWidget *view, double seconds);
double awra_waveform_view_hint_position(AwraWidget *view);
void awra_waveform_view_set_marker_position(AwraWidget *view, double seconds);
void awra_waveform_view_set_marker_editable(AwraWidget *view, bool editable);
double awra_waveform_view_marker_position(AwraWidget *view);

AwraWidget *awra_media_row_new(const char *title);
void awra_media_row_set_title(AwraWidget *row, const char *title);
void awra_media_row_set_subtitle(AwraWidget *row, const char *subtitle);
void awra_media_row_set_icon_name(AwraWidget *row, const char *icon_name);
void awra_media_row_set_accent_color(AwraWidget *row, const AwraRGBA *color);
void awra_media_row_set_show_play_button(AwraWidget *row, bool show);
void awra_media_row_set_show_stop_button(AwraWidget *row, bool show);
void awra_media_row_set_show_volume(AwraWidget *row, bool show);
void awra_media_row_set_volume(AwraWidget *row, double volume);
void awra_media_row_set_playing(AwraWidget *row, bool playing);
void awra_media_row_set_duration(AwraWidget *row, double seconds);
void awra_media_row_set_position(AwraWidget *row, double seconds);
void awra_media_row_set_seekable(AwraWidget *row, bool seekable);
void awra_media_row_set_suffix(AwraWidget *row, AwraWidget *suffix);

AwraWidget *awra_drop_zone_new(void);
void awra_drop_zone_set_title(AwraWidget *zone, const char *title);
void awra_drop_zone_set_subtitle(AwraWidget *zone, const char *subtitle);
void awra_drop_zone_set_icon_name(AwraWidget *zone, const char *icon_name);
void awra_drop_zone_set_browse_label(AwraWidget *zone, const char *label); /* NULL: no button */
void awra_drop_zone_set_child(AwraWidget *zone, AwraWidget *child);

/* Any widget can take dropped files (files-dropped, args->text = paths,
 * '\n'-separated); drop-hover reports a drag over it. */
void awra_widget_set_accepts_file_drops(AwraWidget *widget, bool accepts);

/* A widget the application draws itself (Awra's GtkDrawingArea). */
typedef struct AwraCanvas AwraCanvas;
typedef void (*AwraDrawFunc)(AwraWidget *widget, AwraCanvas *canvas, int width, int height,
                             void *data);
AwraWidget *awra_drawing_area_new(AwraDrawFunc draw, void *data, AwraDestroyNotify destroy);
void awra_drawing_area_set_content_size(AwraWidget *area, int width, int height);
/* Canvas drawing in logical pixels of the widget. */
void awra_canvas_fill_rect(AwraCanvas *c, double x, double y, double w, double h, const AwraRGBA *rgba);
void awra_canvas_round_rect(AwraCanvas *c, double x, double y, double w, double h, double radius,
                            const AwraRGBA *fill, double border_width, const AwraRGBA *border);
void awra_canvas_line(AwraCanvas *c, double x0, double y0, double x1, double y1, double width,
                      const AwraRGBA *rgba);
void awra_canvas_circle(AwraCanvas *c, double cx, double cy, double r, const AwraRGBA *rgba);
void awra_canvas_text(AwraCanvas *c, double x, double y, const char *text, AwraTypography type,
                      const AwraRGBA *rgba);
void awra_canvas_icon(AwraCanvas *c, const char *icon, double x, double y, double size,
                      const AwraRGBA *rgba);

/* ----------------------------------------------------------------------- */
/* Native dialogs (xdg-desktop-portal on Linux through the app, common
 * dialogs on Windows): open files, a folder, or a file to save. The result
 * arrives on the UI thread: paths '\n'-separated, NULL when cancelled. */
typedef void (*AwraFileCallback)(const char *paths, void *data);
typedef struct AwraFileFilter {
  const char *name;     /* "Audio" */
  const char *patterns; /* "*.mp3;*.ogg;*.wav" */
} AwraFileFilter;
void awra_file_dialog_open(AwraWidget *parent, const char *title, bool multiple, bool folder,
                           const AwraFileFilter *filters, int n_filters, AwraFileCallback done,
                           void *data);
void awra_file_dialog_save(AwraWidget *parent, const char *title, const char *initial_name,
                           const AwraFileFilter *filters, int n_filters, AwraFileCallback done,
                           void *data);

/* Physical keys (W3C KeyboardEvent.code names, "KeyQ"): the label the
 * current layout prints on it ("A" on AZERTY), NULL when unknown. */
const char *awra_key_code_label(const char *code);
/* W3C code names and Linux evdev key numbers ("KeyA" = 30); NULL / -1
 * when unknown. */
const char *awra_key_code_name(int evdev);
int awra_key_code_evdev(const char *code);
/* Calls fn(data) after the keyboard layout changed (labels to refresh). */
void awra_keyboard_connect_layout_changed(void (*fn)(void *data), void *data);

/* The clipboard of the widget's window (malloc'd UTF-8, free with
 * awra_free; NULL when empty). */
char *awra_clipboard_get_text(AwraWidget *widget);
void awra_clipboard_set_text(AwraWidget *widget, const char *text);
void awra_free(void *p);

/* Opens a URI or file with the desktop's default handler. */
bool awra_show_uri(const char *uri);

/* ----------------------------------------------------------------------- */
/* Visibility the parent decides (gtk_widget_set_child_visible): a widget a
 * container hides stays visible to the application (awra_widget_visible). */

void awra_widget_set_child_visible(AwraWidget *widget, bool visible);
bool awra_widget_child_visible(const AwraWidget *widget);

/* ----------------------------------------------------------------------- */
/* Revealer (GtkRevealer): shows or hides its child with a transition; the
 * notify signal carries "child-revealed" when one ends. */

typedef enum {
  AWRA_REVEALER_NONE = 0,
  AWRA_REVEALER_CROSSFADE,
  AWRA_REVEALER_SLIDE_RIGHT,
  AWRA_REVEALER_SLIDE_LEFT,
  AWRA_REVEALER_SLIDE_UP,
  AWRA_REVEALER_SLIDE_DOWN,
  AWRA_REVEALER_SWING_RIGHT,
  AWRA_REVEALER_SWING_LEFT,
  AWRA_REVEALER_SWING_UP,
  AWRA_REVEALER_SWING_DOWN
} AwraRevealerTransition;

AwraWidget *awra_revealer_new(void);
void awra_revealer_set_child(AwraWidget *revealer, AwraWidget *child);
AwraWidget *awra_revealer_child(AwraWidget *revealer);
void awra_revealer_set_transition(AwraWidget *revealer, AwraRevealerTransition transition,
                                  unsigned ms);
void awra_revealer_set_reveal_child(AwraWidget *revealer, bool reveal);
bool awra_revealer_reveal_child(AwraWidget *revealer);
bool awra_revealer_child_revealed(AwraWidget *revealer);

/* ----------------------------------------------------------------------- */
/* Layouts the application defines (a GtkWidget subclass's measure and
 * size_allocate): children are added with awra_container_add; measure
 * reports logical pixels (for_size -1: none); allocate places each child
 * with awra_widget_allocate (logical pixels, relative to the layout).
 * NULL functions lay the children out on top of each other (GtkBinLayout). */

typedef void (*AwraLayoutMeasureFunc)(AwraWidget *layout, AwraOrientation orientation, int for_size,
                                      int *minimum, int *natural, void *data);
typedef void (*AwraLayoutAllocateFunc)(AwraWidget *layout, int width, int height, void *data);
AwraWidget *awra_layout_new(AwraLayoutMeasureFunc measure, AwraLayoutAllocateFunc allocate,
                            void *data, AwraDestroyNotify destroy);
/* Clips the children to the layout (overflow: hidden). */
void awra_layout_set_clip(AwraWidget *layout, bool clip);
void awra_widget_allocate(AwraWidget *child, int x, int y, int width, int height);

/* An aspect frame (GtkAspectFrame): its child at `ratio`, aligned. */
AwraWidget *awra_aspect_frame_new(float xalign, float yalign, float ratio);
void awra_aspect_frame_set_child(AwraWidget *frame, AwraWidget *child);
void awra_aspect_frame_set_ratio(AwraWidget *frame, float ratio);

/* A window handle (GtkWindowHandle): dragging it moves the window, a double
 * click maximizes or restores it. */
AwraWidget *awra_window_handle_new(void);
void awra_window_handle_set_child(AwraWidget *handle, AwraWidget *child);

/* ----------------------------------------------------------------------- */
/* Textures (GdkTexture): immutable pixels, shared by reference by every
 * widget showing them. */

typedef struct AwraTexture AwraTexture;
/* Straight RGBA rows (copied, premultiplied). */
AwraTexture *awra_texture_new_rgba(const uint8_t *rgba, int width, int height, int stride);
/* Premultiplied 0xAARRGGBB, rows of `width` (copied). */
AwraTexture *awra_texture_new_argb(const uint32_t *argb, int width, int height);
/* A video frame in NV12 (width * height luma bytes, then interleaved CbCr
 * at half resolution; limited range), copied. */
AwraTexture *awra_texture_new_nv12(const uint8_t *data, int width, int height, bool bt709);
/* Planar I420 (Y, then U, then V, each plane's rows packed); kept as NV12. */
AwraTexture *awra_texture_new_i420(const uint8_t *data, int width, int height, bool bt709);
/* A PNG, JPEG, GIF (first frame), WebP or SVG; NULL when it can't decode. */
AwraTexture *awra_texture_new_from_bytes(const void *encoded, size_t length);
AwraTexture *awra_texture_new_from_file(const char *path);
AwraTexture *awra_texture_ref(AwraTexture *texture);
void awra_texture_unref(AwraTexture *texture);
int awra_texture_width(const AwraTexture *texture);
int awra_texture_height(const AwraTexture *texture);
/* Straight RGBA rows; false for video frames. */
bool awra_texture_download(const AwraTexture *texture, uint8_t *rgba, int stride);
/* Images show a texture (as GtkPicture shows a paintable: natural size the
 * texture's, height for width by its aspect ratio, content fit). */
void awra_image_set_texture(AwraWidget *image, AwraTexture *texture);
AwraTexture *awra_image_texture(AwraWidget *image);
/* GtkPicture's can-shrink (default true): false keeps the minimum size at
 * the texture's own. */
void awra_image_set_can_shrink(AwraWidget *image, bool can_shrink);

/* ----------------------------------------------------------------------- */
/* Rich label (Awra 1's AwraRichLabel): a label whose U+FFFC characters are
 * squares as tall as its letters (or inline_size), a texture drawn over
 * each, in text order. The label inside does everything a label does. */

AwraWidget *awra_rich_label_new(void);
AwraWidget *awra_rich_label_label(AwraWidget *rich);
void awra_rich_label_clear_textures(AwraWidget *rich);
/* index -1 appends; NULL leaves the square empty */
void awra_rich_label_set_texture(AwraWidget *rich, int index, AwraTexture *texture);
void awra_rich_label_set_inline_size(AwraWidget *rich, int size);
/* The same on any label. */
void awra_label_set_inline_size(AwraWidget *label, int size);
void awra_label_clear_inline_textures(AwraWidget *label);
void awra_label_set_inline_texture(AwraWidget *label, int index, AwraTexture *texture);

/* ----------------------------------------------------------------------- */
/* List view (GtkListView with a GtkSignalListItemFactory): the application
 * keeps the items; the view makes item widgets for those near the visible
 * part of the scrolled window it is in, and reuses them. setup gives a new
 * item widget its contents (awra_list_item_set_child), bind fills it for
 * awra_list_item_position, unbind empties it, teardown forgets it.
 * activate: args->index = the position. */

typedef void (*AwraListItemFunc)(AwraWidget *list, AwraWidget *item, void *data);
AwraWidget *awra_list_view_new(void);
void awra_list_view_set_factory(AwraWidget *list, AwraListItemFunc setup, AwraListItemFunc bind,
                                AwraListItemFunc unbind, AwraListItemFunc teardown, void *data,
                                AwraDestroyNotify destroy);
/* The model changed: at `position`, `removed` items went and `added` came. */
void awra_list_view_items_changed(AwraWidget *list, unsigned position, unsigned removed,
                                  unsigned added);
unsigned awra_list_view_n_items(AwraWidget *list);
/* Binds the item at `position` again (unbind, bind) when it has a widget. */
void awra_list_view_rebind(AwraWidget *list, unsigned position);
/* The item widget bound to `position`, or NULL. */
AwraWidget *awra_list_view_item(AwraWidget *list, unsigned position);
int awra_list_view_bound_count(AwraWidget *list);
AwraWidget *awra_list_view_bound_item(AwraWidget *list, int index);
/* The first and last positions on screen; false when none. */
bool awra_list_view_visible_range(AwraWidget *list, unsigned *first, unsigned *last);
/* Scrolls to an item: align 0 just into view, 1 at the top, 2 centered,
 * 3 at the bottom. */
void awra_list_view_scroll_to(AwraWidget *list, unsigned position, int align);
void awra_list_view_set_single_click_activate(AwraWidget *list, bool single);
void awra_list_view_set_spacing(AwraWidget *list, int spacing);
/* Space above the first item and below the last (logical px). */
void awra_list_view_set_padding(AwraWidget *list, int top, int bottom);
/* Rows' look: 0 plain, 1 menu rows (the selected one tinted; hovering
 * selects). The selection: selected (args->index, -1 none). */
void awra_list_view_set_row_style(AwraWidget *list, int style);
void awra_list_view_set_selected(AwraWidget *list, unsigned position);
unsigned awra_list_view_selected(AwraWidget *list);
void awra_list_item_set_child(AwraWidget *item, AwraWidget *child);
AwraWidget *awra_list_item_child(AwraWidget *item);
/* UINT_MAX when it is not bound */
unsigned awra_list_item_position(AwraWidget *item);
void awra_list_item_set_activatable(AwraWidget *item, bool activatable);
void awra_list_item_set_selectable(AwraWidget *item, bool selectable);
void awra_list_item_set_focusable(AwraWidget *item, bool focusable);

/* ----------------------------------------------------------------------- */
/* Avatars (Awra 1's AwraAvatar, AwraAvatarGroup): a picture, or initials on
 * a plate of the name's hue; a presence dot cut into the bottom end corner,
 * a speaking ring. Groups overlap their avatars by a third. */

typedef enum {
  AWRA_PRESENCE_NONE = 0,
  AWRA_PRESENCE_ONLINE,
  AWRA_PRESENCE_IDLE,
  AWRA_PRESENCE_BUSY,
  AWRA_PRESENCE_OFFLINE
} AwraPresence;
typedef enum { AWRA_AVATAR_SHAPE_CIRCLE = 0, AWRA_AVATAR_SHAPE_ROUNDED } AwraAvatarShape;

AwraWidget *awra_avatar_new(int size, const char *text);
void awra_avatar_set_size(AwraWidget *avatar, int size);
int awra_avatar_size(AwraWidget *avatar);
void awra_avatar_set_text(AwraWidget *avatar, const char *text);
const char *awra_avatar_text(AwraWidget *avatar);
void awra_avatar_set_show_initials(AwraWidget *avatar, bool show);
/* The picture (NULL: initials or the icon). */
void awra_avatar_set_texture(AwraWidget *avatar, AwraTexture *texture);
AwraTexture *awra_avatar_texture(AwraWidget *avatar);
void awra_avatar_set_icon_name(AwraWidget *avatar, const char *icon_name);
void awra_avatar_set_shape(AwraWidget *avatar, int shape); /* AwraAvatarShape */
void awra_avatar_set_presence(AwraWidget *avatar, AwraPresence presence);
void awra_avatar_set_speaking(AwraWidget *avatar, bool speaking);
AwraWidget *awra_avatar_group_new(void);
void awra_avatar_group_append(AwraWidget *group, AwraWidget *avatar);
void awra_avatar_group_remove_all(AwraWidget *group);

/* ----------------------------------------------------------------------- */
/* Content pieces (Awra 1's AwraChip, AwraDivider, AwraSkeleton,
 * AwraTypingIndicator, AwraBadgeGroup, AwraMetadataGroup, AwraErrorState) */

typedef enum { AWRA_CHIP_NEUTRAL = 0, AWRA_CHIP_ACCENT } AwraChipAppearance;
/* A pill with a picture, an emoji or an icon, a label and a count (-1:
 * none); clicked when activated, toggled (args->active) when toggleable. */
AwraWidget *awra_chip_new(const char *label);
void awra_chip_set_label(AwraWidget *chip, const char *label);
const char *awra_chip_label(AwraWidget *chip);
void awra_chip_set_icon_name(AwraWidget *chip, const char *icon_name);
void awra_chip_set_texture(AwraWidget *chip, AwraTexture *texture);
void awra_chip_set_emoji(AwraWidget *chip, const char *emoji);
void awra_chip_set_count(AwraWidget *chip, int count);
int awra_chip_count(AwraWidget *chip);
void awra_chip_set_active(AwraWidget *chip, bool active);
bool awra_chip_active(AwraWidget *chip);
void awra_chip_set_toggleable(AwraWidget *chip, bool toggleable);
void awra_chip_set_appearance(AwraWidget *chip, int appearance); /* AwraChipAppearance */

typedef enum {
  AWRA_DIVIDER_LABEL_CENTER = 0,
  AWRA_DIVIDER_LABEL_END,
  AWRA_DIVIDER_LABEL_START
} AwraDividerLabelPosition;
typedef enum { AWRA_DIVIDER_NEUTRAL = 0, AWRA_DIVIDER_ACCENT, AWRA_DIVIDER_DANGER } AwraDividerAppearance;
AwraWidget *awra_divider_new(const char *label);
void awra_divider_set_label(AwraWidget *divider, const char *label);
void awra_divider_set_label_position(AwraWidget *divider, AwraDividerLabelPosition position);
void awra_divider_set_appearance(AwraWidget *divider, AwraDividerAppearance appearance);

AwraWidget *awra_skeleton_new(void);
void awra_skeleton_set_lines(AwraWidget *skeleton, int lines);
void awra_skeleton_set_animated(AwraWidget *skeleton, bool animated);

AwraWidget *awra_typing_indicator_new(void);
void awra_typing_indicator_set_label(AwraWidget *indicator, const char *label);
void awra_typing_indicator_set_active(AwraWidget *indicator, bool active);

AwraWidget *awra_badge_group_new(void);
void awra_badge_group_append(AwraWidget *group, AwraWidget *badge);
void awra_badge_group_remove_all(AwraWidget *group);
AwraWidget *awra_metadata_group_new(void);
void awra_metadata_group_set_title(AwraWidget *group, const char *title);
/* Appends a row of a label and a value; returns it (borrowed). */
AwraWidget *awra_metadata_group_append(AwraWidget *group, const char *label, const char *value);
AwraWidget *awra_metric_row_new(const char *label, const char *value);

/* An error state: the empty state's layout with the error icon. */
AwraWidget *awra_error_state_new(const char *title, const char *description);

/* ----------------------------------------------------------------------- */
/* Rail (Awra 1's AwraRail, AwraRailItem, AwraRailGroup): a column of 48 px
 * tiles with a pill at the edge (unread, hovered, selected), a mention
 * badge, folders (groups) that open into a column, separators, and a header
 * and footer that stay put while the entries scroll. Items emit activate
 * when clicked (the rail selects them, notify "selected-item"); a group's
 * tile toggles it (toggled). */

AwraWidget *awra_rail_new(void);
void awra_rail_append(AwraWidget *rail, AwraWidget *entry);
/* Inserts, or moves an entry already in the rail, at `position` (-1 end). */
void awra_rail_insert(AwraWidget *rail, AwraWidget *entry, int position);
void awra_rail_remove(AwraWidget *rail, AwraWidget *entry);
AwraWidget *awra_rail_add_separator(AwraWidget *rail);
int awra_rail_n_entries(AwraWidget *rail);
AwraWidget *awra_rail_entry(AwraWidget *rail, int position);
AwraWidget *awra_rail_selected_item(AwraWidget *rail);
void awra_rail_set_selected_item(AwraWidget *rail, AwraWidget *item);
void awra_rail_set_header(AwraWidget *rail, AwraWidget *header);
void awra_rail_set_footer(AwraWidget *rail, AwraWidget *footer);
void awra_rail_set_reorderable(AwraWidget *rail, bool reorderable);
bool awra_rail_reorderable(AwraWidget *rail);
AwraWidget *awra_rail_item_new(const char *label);
void awra_rail_item_set_label(AwraWidget *item, const char *label);
const char *awra_rail_item_label(AwraWidget *item);
/* The tile's face (an avatar takes the tile's size and shape). */
void awra_rail_item_set_child(AwraWidget *item, AwraWidget *child);
AwraWidget *awra_rail_item_child(AwraWidget *item);
void awra_rail_item_set_icon_name(AwraWidget *item, const char *icon_name);
void awra_rail_item_set_selected(AwraWidget *item, bool selected);
bool awra_rail_item_selected(AwraWidget *item);
void awra_rail_item_set_unread(AwraWidget *item, bool unread);
void awra_rail_item_set_mention_count(AwraWidget *item, unsigned count);
void awra_rail_item_set_muted(AwraWidget *item, bool muted);
AwraWidget *awra_rail_group_new(const char *label);
void awra_rail_group_set_label(AwraWidget *group, const char *label);
void awra_rail_group_set_icon_name(AwraWidget *group, const char *icon_name);
void awra_rail_group_set_color(AwraWidget *group, const AwraRGBA *color);
void awra_rail_group_set_expanded(AwraWidget *group, bool expanded);
bool awra_rail_group_expanded(AwraWidget *group);
void awra_rail_group_append(AwraWidget *group, AwraWidget *item);
void awra_rail_group_insert(AwraWidget *group, AwraWidget *item, int position);
void awra_rail_group_remove(AwraWidget *group, AwraWidget *item);
int awra_rail_group_n_items(AwraWidget *group);
AwraWidget *awra_rail_group_item(AwraWidget *group, int index);

/* ----------------------------------------------------------------------- */
/* Conversation view (Awra 1's AwraConversationView): a list view of
 * messages (awra_conversation_view_list gives its list view, whose factory
 * the application sets; items_changed goes through the view) that starts at
 * the newest, follows new ones while the newest shows, keeps what is read
 * in place while older ones load above, offers a pill back to the newest
 * (jump-activated), shows each end's status (edge-reached, retry), and
 * notifies "at-bottom". */

typedef enum { AWRA_EDGE_STATUS_NONE = 0, AWRA_EDGE_STATUS_LOADING, AWRA_EDGE_STATUS_FAILED } AwraEdgeStatus;
typedef enum {
  AWRA_CONVERSATION_ALIGN_TOP = 0,
  AWRA_CONVERSATION_ALIGN_CENTER,
  AWRA_CONVERSATION_ALIGN_END
} AwraConversationAlign;
AwraWidget *awra_conversation_view_new(void);
AwraWidget *awra_conversation_view_list(AwraWidget *view);
void awra_conversation_view_items_changed(AwraWidget *view, unsigned position, unsigned removed,
                                          unsigned added);
bool awra_conversation_view_at_bottom(AwraWidget *view);
void awra_conversation_view_set_follow(AwraWidget *view, bool follow);
bool awra_conversation_view_follow(AwraWidget *view);
void awra_conversation_view_set_unread_count(AwraWidget *view, unsigned count);
unsigned awra_conversation_view_unread_count(AwraWidget *view);
void awra_conversation_view_set_jump_label(AwraWidget *view, const char *label);
const char *awra_conversation_view_jump_label(AwraWidget *view);
/* How near an end counts as reaching it (logical px; -1: a page). */
void awra_conversation_view_set_edge_threshold(AwraWidget *view, int threshold);
void awra_conversation_view_scroll_to_end(AwraWidget *view, bool animate);
void awra_conversation_view_scroll_to_item(AwraWidget *view, unsigned position,
                                           AwraConversationAlign align, bool highlight);
/* edge: 0 top, 1 bottom */
void awra_conversation_view_set_edge_status(AwraWidget *view, int edge, AwraEdgeStatus status,
                                            const char *label);
bool awra_conversation_view_visible_range(AwraWidget *view, unsigned *first, unsigned *last);

/* ----------------------------------------------------------------------- */
/* Composer (Awra 1's AwraComposer): one well holding a banner and
 * attachments that slide out above a row of a start widget, the text (up
 * to max_lines), an end widget and the send button, shown once something
 * can be sent. activate: submit (Return, or the button); changed: the
 * text; stop-requested: the banner's close button. */

AwraWidget *awra_composer_new(void);
/* The text view (its editing: awra_editable_*). */
AwraWidget *awra_composer_view(AwraWidget *composer);
void awra_composer_set_text(AwraWidget *composer, const char *text);
const char *awra_composer_text(AwraWidget *composer);
void awra_composer_set_placeholder(AwraWidget *composer, const char *text);
void awra_composer_set_max_lines(AwraWidget *composer, int lines);
void awra_composer_set_can_send(AwraWidget *composer, bool can_send);
void awra_composer_set_send_on_enter(AwraWidget *composer, bool on);
void awra_composer_set_busy(AwraWidget *composer, bool busy);
void awra_composer_set_banner(AwraWidget *composer, AwraWidget *banner);
AwraWidget *awra_composer_banner(AwraWidget *composer);
void awra_composer_show_banner(AwraWidget *composer, const char *text, bool closable);
void awra_composer_set_attachments(AwraWidget *composer, AwraWidget *attachments);
AwraWidget *awra_composer_attachments(AwraWidget *composer);
void awra_composer_set_start_widget(AwraWidget *composer, AwraWidget *child);
void awra_composer_set_end_widget(AwraWidget *composer, AwraWidget *child);

/* Completion popover (Awra 1's AwraCompletionPopover): suggestions above
 * the caret of the text view it is attached to, in a list view
 * (awra_completion_popover_list: its factory and items are the
 * application's). Up/Down choose, Tab/Return take (activate: args->index),
 * Escape dismisses (stop-search); the text keeps the keyboard. */
AwraWidget *awra_completion_popover_new(void);
AwraWidget *awra_completion_popover_list(AwraWidget *completion);
void awra_completion_popover_attach(AwraWidget *completion, AwraWidget *text_view);
void awra_completion_popover_set_selected(AwraWidget *completion, unsigned position);
unsigned awra_completion_popover_selected(AwraWidget *completion);
bool awra_completion_popover_visible(AwraWidget *completion);
void awra_completion_popover_present_at_cursor(AwraWidget *completion);
void awra_completion_popover_popdown(AwraWidget *completion);

/* ----------------------------------------------------------------------- */
/* Data view (Awra 1's AwraDataView): column titles over a list view whose
 * rows hold a cell per column (each column's factory fills its cells, as
 * list items: awra_list_item_position); its list: awra_data_view_list. */

typedef enum {
  AWRA_DATA_VIEW_CONTENT = 0,
  AWRA_DATA_VIEW_EMPTY,
  AWRA_DATA_VIEW_LOADING,
  AWRA_DATA_VIEW_ERROR
} AwraDataViewState;
AwraWidget *awra_data_view_new(void);
AwraWidget *awra_data_view_list(AwraWidget *view);
int awra_data_view_add_column(AwraWidget *view, const char *title, AwraListItemFunc setup,
                              AwraListItemFunc bind, AwraListItemFunc unbind,
                              AwraListItemFunc teardown, void *data, AwraDestroyNotify destroy);
void awra_data_view_set_column_visible(AwraWidget *view, int column, bool visible);
void awra_data_view_set_state(AwraWidget *view, AwraDataViewState state);
void awra_data_view_set_empty_widget(AwraWidget *view, AwraWidget *child);
void awra_data_view_set_loading_widget(AwraWidget *view, AwraWidget *child);
void awra_data_view_set_error_widget(AwraWidget *view, AwraWidget *child);
/* A "Loading more…" row under the rows while the next page comes. */
void awra_data_view_set_loading_more(AwraWidget *view, bool loading);

/* ----------------------------------------------------------------------- */
/* Emoji picker (Awra 1's AwraEmojiPicker): search, section tabs, a grid of
 * emoji drawn without a widget each, a preview. activate: args->text = the
 * picked item's id. */

typedef struct AwraEmoji {
  const char *id;
  const char *name;
  const char *text;      /* the glyph, or NULL */
  AwraTexture *texture;  /* a picture (custom emoji), or NULL */
  const char *keywords;  /* space-separated, or NULL */
} AwraEmoji;
AwraWidget *awra_emoji_picker_new(void);
void awra_emoji_picker_clear(AwraWidget *picker);
void awra_emoji_picker_add_section(AwraWidget *picker, const char *id, const char *title,
                                   const char *icon_name, const AwraEmoji *items, int n);
void awra_emoji_picker_set_search_text(AwraWidget *picker, const char *text);
/* An item's picture arrived. */
void awra_emoji_picker_set_texture(AwraWidget *picker, const char *id, AwraTexture *texture);
AwraWidget *awra_emoji_picker_search_entry(AwraWidget *picker);

/* Colour palette (Awra 1's AwraColorPalette): an automatic swatch, colours
 * and a custom one; selected: args->index 0 automatic, 1 a colour
 * (args->text = #rrggbb). */
AwraWidget *awra_color_palette_new(void);
void awra_color_palette_add_color(AwraWidget *palette, const AwraRGBA *color, const char *name);
void awra_color_palette_clear(AwraWidget *palette);
void awra_color_palette_set_automatic_label(AwraWidget *palette, const char *label);
void awra_color_palette_set_allows_custom(AwraWidget *palette, bool allows);
void awra_color_palette_set_selected(AwraWidget *palette, const AwraRGBA *color);
bool awra_color_palette_selected(AwraWidget *palette, AwraRGBA *color);

/* ----------------------------------------------------------------------- */
/* Fields (Awra 1's AwraTextArea, AwraPasswordEntry, AwraTagEntry) */

/* A framed multi-line text view (its text: awra_text_view_*). */
AwraWidget *awra_text_area_new(void);
/* Hidden text, a peek button at the end; its entry: */
AwraWidget *awra_password_entry_new(void);
AwraWidget *awra_password_entry_entry(AwraWidget *password);
void awra_password_entry_set_show_peek_icon(AwraWidget *password, bool show);
/* Words as chips; Return in its entry adds one (activate: args->text), a
 * chip's click removes it (stop-requested: args->text). */
AwraWidget *awra_tag_entry_new(void);
AwraWidget *awra_tag_entry_entry(AwraWidget *tags);
bool awra_tag_entry_add(AwraWidget *tags, const char *tag);
void awra_tag_entry_clear(AwraWidget *tags);
int awra_tag_entry_n_tags(AwraWidget *tags);
const char *awra_tag_entry_tag(AwraWidget *tags, int index);

/* Overlay bar (Awra 1's AwraOverlayBar): controls floating at the foot of
 * media on the Floating material, shown when the pointer moves and hidden
 * after a rest (auto hide). */
AwraWidget *awra_overlay_bar_new(void);
void awra_overlay_bar_set_content(AwraWidget *bar, AwraWidget *content);
void awra_overlay_bar_set_bar(AwraWidget *bar, AwraWidget *controls);
void awra_overlay_bar_set_auto_hide(AwraWidget *bar, bool auto_hide);
void awra_overlay_bar_set_inhibit_auto_hide(AwraWidget *bar, bool inhibit);
void awra_overlay_bar_set_revealed(AwraWidget *bar, bool revealed);
void awra_overlay_bar_reveal(AwraWidget *bar);
void awra_overlay_bar_set_hide_delay(AwraWidget *bar, unsigned ms);
void awra_overlay_bar_set_max_width(AwraWidget *bar, int width);
void awra_overlay_bar_set_inset(AwraWidget *bar, AwraSpacing inset);
/* The floating surface holding the bar, and its inset (logical px). */
AwraWidget *awra_overlay_bar_surface(AwraWidget *bar);
int awra_overlay_bar_inset_px(AwraWidget *bar);

#ifdef __cplusplus
}
#endif
