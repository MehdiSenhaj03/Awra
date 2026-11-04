<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Awra 1.0 API map

Awra exposes one C/GObject ABI and the `Awra-1.0` introspection namespace.
The umbrella header is `<awra/awra.h>`.

The stable 1.x compatibility and ownership rules are documented in
[stability.md](stability.md). They are enforced by public-symbol, GIR and
external-consumer tests.

## Runtime and appearance

- `AwraContext` is the lazy singleton associated with a `GdkDisplay`.
- `AwraStyleManager` owns appearance, accent, contrast, reduced motion and
  reduced transparency preferences. Accent sources are SYSTEM (best-effort,
  DEFAULT fallback), DEFAULT and CUSTOM.
  It also sets the display's font rendering: while any monitor is at scale 1,
  glyphs follow the pixel grid and the desktop's hinting; only a fully dense
  setup keeps GTK's automatic placement.
- The style manager also exposes reversible material-calibration overrides for
  Canvas tint/alpha, grain scale and native-blur activation. These rebuild the
  immutable token snapshot in real time; `reset_material_tuning()` restores the
  profile defaults. Native blur is a boolean request because the Wayland
  protocol exposes no blur-strength or algorithm parameter.
- Per-material calibration accepts either a low-level `AwraMaterial` descriptor
  or a semantic preset descriptor. Tint/alpha, radius, grain, outline,
  highlight and blur intent are global to that recipe: changing Floating is
  observed by every existing and future Floating surface. Solid and Opaque
  keep alpha locked to 1, and only Frosted, Canvas and Floating can request
  native blur. A per-material reset and the existing global reset are public.
- `AwraTokenSet` is the immutable semantic token snapshot.
- `AwraDiagnostics` reports backend capability and native effect state.
- `AwraMaterialResolution` is a read-only snapshot of the exact resolved fill,
  decoration, fallback and blur-request values used by a context. It exists so
  diagnostics can prove active/inactive invariants without private APIs.

## Rendering and layout

- `AwraWindow` uses a square outer frame without borders or shadow when
  maximized or fullscreen, filling the compositor's entire allocation. Set
  `keep-rounded-corners=true` to retain frame rounding in those states; the
  default is `false`. The flag can change while the window is visible and is
  available in GtkBuilder, C, Python and Rust. For example, Python uses
  `window.set_keep_rounded_corners(True)`, and Rust uses
  `window.set_keep_rounded_corners(true)`. Restoring the window preserves the
  configured radius and corner/edge masks. Nested surfaces and floating chrome
  keep their own geometry. Maximization respects the desktop work area;
  fullscreen covers the monitor, following ordinary GTK window behavior.

- `AwraFramePool` provides bounded, aligned RGB/YUV frame leases independent of
  media engines. `AwraFrame` publishes immutable textures without copying pixels.
  `AwraTextureStream` feeds live pictures without repeated layout or CPU mipmaps;
  `AwraAspectPaintable` supplies a display ratio for non-square source pixels.
  See [live-frames.md](../guides/live-frames.md).

- `AwraOverlayBar` places idle-aware floating controls over full-size content.
  `AwraWindow:overlay-chrome` composes headers without reserving canvas height.
  Enable that composition before first presentation; later mode changes preserve
  the native surface. Timeline-owned previews follow logical pointer coordinates,
  and `AwraToastOverlay:bottom-inset` keeps messages clear of overlay controls.
  See [timelines.md](../guides/timelines.md).

- `AwraMaterial` keeps low-level Solid, Translucent and Frosted intent and adds
  Canvas, Content, Chrome, Layer, Floating and Opaque semantic presets.
- `AwraSurface` renders the resolved material, clipping and elevation with GSK.
- `AwraCardAppearance` selects Plain, Tinted or Raised grouping without
  application CSS. `AwraElevation` names Flat, Raised, Floating and Modal depth.
- `AwraSpacing` exposes the low-level NONE/XS/SM/MD/LG/XL/XXL scale.
  `AwraLayoutPreset` combines those tokens into Compact, Control Group, Section,
  Content, Page, Toolbar and Overlay recipes. The public box/grid/margin helpers
  apply the recipes to ordinary GTK containers, keeping layouts introspectable
  and eliminating application-owned magic pixels.
- `AwraCard`, `AwraHeader`, `AwraSidebar` and `AwraSplitView` provide the
  responsive layout foundation. `AwraHeader` has independent start/center/end,
  vertically centers supplied widgets by default, gives expanding center
  controls a comfortable symmetric span before spending surplus width on the
  side clusters, and removes empty system-control groups from measurement.
  Drag and window-control choices remain independent; `AwraWindow:chrome`
  accepts it or another application-defined titlebar without imposing a
  sidebar composition.
  Cards own an LG content inset by default. `AwraSidebar` starts with an MD
  inset; NONE keeps the traditional docked composition.
  `blend-with-window` shares the root material and root surface role, while surface corner/edge masks
  distribute one continuous outer silhouette across header and content.
- `AwraPage` owns vertical scrolling, tokenized content insets and a centered
  readable width (960 logical pixels by default). `AwraPageHeader` and
  `AwraSection` provide semantic heading/action/content structure without
  painting another card. `AwraToolbar` exposes independent start/center/end
  groups and a compact density. All four are GtkBuilder/GIR types; their child
  properties are nullable and use GTK container ownership.
- `AwraForm` and adaptive `AwraFormRow` own accessible label/control/help/error
  relationships and validation state. `AwraEmptyState`, `AwraLoadingState` and
  `AwraErrorState` provide reusable page feedback.
- `AwraFilterBar` owns search/filter/summary/reset flow. `AwraMasterDetail`
  exposes observable Compact/Medium/Expanded layout and logical pane
  navigation; `AwraDetailPane` owns entity identity, metadata, actions and
  semantic sections without locking applications into one window chrome.
- `AwraEdgePanel` overlays a semantic Floating surface on either logical edge.
  It can remain visible, be controlled explicitly, or reveal from a narrow
  pointer/keyboard trigger and dismiss after a configurable delay or Escape.
  Its motion automatically follows reduced-motion preferences.
- `AwraMotionPreset` maps Fast, Normal and Slow motion to zero duration when
  reduced motion is enabled.

## Controls and navigation

The input widgets preserve GTK behavior either by subclassing a derivable GTK
control or by delegating to a final GTK control. This keeps actions, IME,
selection, keyboard navigation and accessibility semantics in GTK.

The extended set includes `AwraActionRow`, `AwraDropdown`, `AwraListView`,
`AwraSegmentedControl`, `AwraTabView`, `AwraNavigationView`,
`AwraNavigationItem`, `AwraBadge` and `AwraProgressBar`. A navigation item
can lead with any widget (`prefix`, such as an `AwraAvatar`), carry a quieter
`subtitle`, and mark a destination `emphasized` (something new) or `muted`;
its `label-color` takes a colour of the destination, such as a role colour,
made readable on the current appearance. An action row's `subtitle-lines`
lets its subtitle wrap. Buttons expose Primary,
Secondary, Ghost, Destructive and Toolbar appearances. An `AwraButtonContent`
child shows an icon before the label, and a `busy` button shows a spinner,
keeping its size, while the work it started runs.
Use Secondary for ordinary dismissive dialog actions: it remains identifiable
at rest and strengthens its material plane on hover. Ghost is intentionally
backgroundless at rest for quiet inline actions, but still exposes explicit
hover, pressed and keyboard-focus feedback. Primary hover/pressed states keep
the exact same rounded-rectangle geometry and vary only their restrained inner
rim/depth; the accent fill and its contrast stay stable, and state changes
never add an overlay shape.
Badges expose Neutral, Accent, Info, Success, Warning and Danger appearances,
so category and status colors remain accessible and isolated in Light/Dark.
For domain vocabularies with more categories, `AwraBadge:custom-color` draws a
clean, borderless and token-independent category fill with a stable white
content color; applications must therefore choose a sufficiently dark category
palette. This keeps the same vocabulary visually stable from one hue to the
next without manufacturing CSS classes for each data category.
`AwraBadge` owns its measured horizontal and vertical content insets, so its
breathing room is preserved by layout managers instead of being a purely
painted CSS decoration.
`AwraDropdown` exposes its Floating popup through
`awra_dropdown_get_popover()` plus explicit `popup()`/`popdown()` operations;
changing the global Floating material immediately updates every open dropdown.
Its list selection is provisional while the popup is being navigated: the
public `selected` value changes only on activation, so pointer hover or a
cancelled popup cannot apply a filter.
`AwraTabView` owns the complete dynamic-tab grammar: pinned and closable pages,
selection by widget, live title/tooltip changes and a vetoable `close-page`
signal. Pinned pages have no implicit Home identity; applications can assign
an optional themed icon or an application-provided `GtkImage`, and independently
hide the visual title while its accessible label remains available. Domain
images such as avatars, document thumbnails or sprites use
`awra_tab_view_set_page_icon()`; Awra fixes their footprint and spacing so
asynchronous image loading cannot change the rail geometry. A pinned icon-only
page uses a measured square control and is therefore a true circle rather than
a stretched pill; its fixed
separator remains visible through hover and selection. Contextual close
actions use the same square-allocation rule at their compact size. Non-pinned
tabs use bounded responsive widths. The bar is a single rounded well: tabs
shrink uniformly between bounded minimum and
maximum widths, then overflow into an internal horizontal viewport instead of
changing the window's minimum width. Pinned tabs stay fixed while wheel,
touchpad and horizontal gestures move the dynamic rail; selecting an offscreen
page reveals it automatically; `scroll-selects` makes scrolling move the
selection instead. A middle click closes a closable tab. Tabs ease into their slots, and a drag
can lift one out of the bar and show a placeholder where it would land. Close affordances appear only on selection or
hover, and the neighboring short separators fade without changing layout. The
minimum tab pitch deliberately favors calm labels and earlier scrolling over
packing one more cramped destination into the rail. Its content stack is
visible by default; `content-visible=false`
supports applications whose tabs navigate a shared external workspace while
retaining the same Awra-owned tab bar and interaction model.
`AwraDataView` retains GTK models and virtualization while owning the dense
table presentation, state views, scrolling and incremental-loading feedback.
`AwraDataColumn` wraps GTK factories/sorters with semantic sizing and
alignment. `AwraSelectionToolbar` binds contextual actions directly to a
selection model. See [data-views.md](../guides/data-views.md).
`AwraGridView` wraps `GtkGridView` with a scrollable viewport, rubberband
selection, keyboard navigation and Awra-owned hover/selection/focus styling.
It accepts ordinary GTK selection models and factories; its public delegate
provides activation, column limits and `scroll_to()`. Long breadcrumb trails
scroll horizontally and reveal the current segment without imposing their
full width on an application. `AwraPathBar` adds an in-place text field to the
same trail: the consumer supplies the committed text, answers `submit` and
optionally fills `suggestions` from `notify::edit-text`, so the bar works with
any hierarchical address.
`AwraListView` similarly owns its scrollable viewport and accepts an optional
`header` widget for compact row factories that intentionally remain a single
GTK list column. Overlay scrollbar visibility and header presentation stay
framework-owned.
`AwraMetricRow`, `AwraStatBar`, `AwraBadgeGroup` and `AwraMetadataGroup`
provide dense reusable detail composition without application spacing.
`awra_widget_set_typography()` applies the public Body, Muted, Eyebrow, Title,
Display or Monospace hierarchy to any `GtkWidget`; its implementation classes
remain private, while the system font family and scale remain authoritative.

The media and keyboard input family adds `AwraKeyCap`, `AwraKeyboardView`
(ANSI/ISO through `AwraKeyboardPhysicalLayout`, layout-aware legends,
`GtkSelectionMode` selection, roving focus and per-key file drops),
`AwraShortcutCapture`, `AwraWaveformView`, `AwraVolumeControl`,
`AwraMediaRow`, `AwraDropZone` and `AwraDragPreview`, which shows what a drag
carries as an image or a counted pile. `AwraTransitionBin` animates content
replaced in place from its last frame. `AwraTimedAnimation` and
`AwraSpringAnimation` animate any value on a widget's frame clock through an
`AwraAnimationTarget`, follow reduced motion, and a spring can be retargeted
in flight without a jump; see [motion](../guides/motion.md).

Messaging applications compose `AwraAvatar` (a picture, initials on a stable
colour, a presence dot and a speaking ring) and `AwraRail`, a vertical rail of
`AwraRailItem` destinations and `AwraRailGroup` folders with unread pills,
mention badges, keyboard navigation and drag reordering, and
`AwraTypingIndicator`, which keeps its height while it comes and goes, and
`AwraChip` for reactions, filters and tags with a rolling count; see
[messaging](../guides/messaging.md). `AwraTileView` divides a surface into
as many tiles as the user wants; see
[page composition](../guides/page-composition.md). `AwraConversationView` is
a bottom-anchored, virtualized timeline for chats, comment threads and logs,
`AwraComposer` the growing field where messages are written and
`AwraCompletionPopover` the suggestions shown at its caret, and `AwraDivider`
separates days or marks unread messages, and `AwraEmojiPicker` picks emoji
and custom images from `AwraEmojiItem` sections; see
[conversations](../guides/conversations.md). Physical keys use `KeyboardEvent.code`
identifiers; `awra_keyboard_code_from_hardware_keycode()`,
`awra_keyboard_code_to_hardware_keycode()` and
`awra_keyboard_code_get_label()` bridge them to GDK hardware keycodes and the
active layout. Signals carry typed values (`GdkModifierType`, `GdkFileList`,
seconds as doubles), and value collections are string vectors or
length-annotated arrays so every binding receives native containers. See
[forms-and-inputs.md](../guides/forms-and-inputs.md#media-and-keyboard-input).

`awra_widget_add_style_role()` and `awra_widget_remove_style_role()` opt an
ordinary GTK behavioral widget into Awra's data-application recipes. These
roles cover dense tables, filter toolbars, domain tags,
statistics and comparison charts without exposing CSS provider ownership to an
application. They are intentionally complementary to the higher-level Awra
widgets: GTK continues to own scrolling, selection, editing and activation,
while Awra owns the visual identity. Role identifiers use lowercase ASCII,
digits and hyphens and are ABI-stable strings so bindings can consume new
recipes without a new enum value.
The supported vocabulary, expected GTK widgets and typed replacements are
listed in [the role catalogue](../guides/style-roles.md).

## Native and in-window overlays

`AwraPopover` and `AwraDialog` own native surfaces and therefore participate in
the private effect coordinator. `AwraToastOverlay` stays inside its parent
window. `awra_toast_overlay_show_with_action()` adds one action button bound
to a detailed `GAction` name (for example an Undo) and returns an identifier
for `awra_toast_overlay_dismiss()`; `dismissed` reports every hidden toast,
while `show_message()` keeps its original behaviour. Menus consume
`GMenuModel` and `GAction`; applications never call a
Wayland or compositor API. `awra_widget_set_context_menu_model()` attaches the
same Awra menu presentation and secondary-click lifecycle to any widget, so
applications do not need to manually parent popovers or own input gestures.

`AwraDialog` keeps a calm default width but deliberately leaves its height
content-driven. Compose a short form or action group with
`AWRA_LAYOUT_PRESET_OVERLAY`; its token-based inset and gap avoid both cramped
headers and dead space below the final action. The shared `AwraHeader` centers
its title stack against end controls even when it contains only one title.
A response closes the dialog unless `awra_dialog_set_response_closes()` keeps
it open, for a form that waits on the network: mark the response busy while
the work runs, show an error in the body if it fails, and close with
`awra_dialog_close()`, which gives no response.

`AwraResponsiveBin` exposes observable size classes. `AwraBreadcrumbBar` and
`AwraCommandPalette` complete the typed navigation family; the latter consumes
ordinary `GMenuModel` commands and restores focus after dismissal. See
[responsive-navigation.md](../guides/responsive-navigation.md).

`AwraInspector` reports widget/surface state and can outline the native effect
region for developer diagnostics.

See [timelines and interval editing](../guides/timelines.md) for seeking, markers,
viewport control and backend-independent synchronization.
