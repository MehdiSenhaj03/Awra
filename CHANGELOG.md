<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Changelog

## 1.1.0 — unreleased

API added in this release is documented `Since: 1.1`; every 1.0.0 API is
unchanged (checked against the frozen 1.0.0 contract).

### Motion and messaging components

- `AwraTimedAnimation` and `AwraSpringAnimation` animate any value on a
  widget's frame clock, through an `AwraCallbackAnimationTarget` (a closure in
  Rust) or an `AwraPropertyAnimationTarget`. They follow reduced motion, jump
  to their end when the widget is not on screen, and keep themselves alive
  while they play. `awra_spring_animation_retarget()` sends a spring to a new
  destination in flight, keeping its value and velocity, so interrupted
  motion never jumps. `AwraEasing` names eight curves and
  `awra_easing_ease()` evaluates them. See [motion](docs/guides/motion.md).
- `AwraAvatar` shows a person or a place: a picture cropped to a circle or a
  rounded square, or initials on a colour derived from the name, so a name
  always looks the same. A presence dot is cut into its corner, readable on
  any background and by its shape as well as its colour; a ring breathes
  around someone speaking, and a new picture cross-fades.
- `AwraRail` is a vertical rail of destinations, such as workspaces or
  accounts, with a fixed header and footer around a scrolling column.
  `AwraRailItem` tiles are round at rest and become rounded squares on hover
  and selection; a pill on the start edge tells unread activity, hover and
  selection, and a mention badge pops in, all on springs that retarget, so
  quick pointer moves never jump. `AwraRailGroup` folds items into a folder
  that previews them, sums up their activity, and unfolds on a tinted column.
  A reorderable rail lets the user drag entries while the others make room,
  and reports a drop onto another entry so the application can make a folder.
- `AwraTypingIndicator` tells that someone is writing: three dots rise in a
  wave beside a line of text. It keeps its height whether shown or not, so the
  conversation never moves when it appears, and its dots stay still under
  reduced motion.
- `AwraChip` is a compact pill for reactions, filters and tags, with an icon,
  a picture or an emoji, a label and a count. A click turns it on with a small
  springy bounce, an active chip takes the accent, and when the count changes
  only the digits that differ roll to the new value.
- `AwraTokenSet` gains `success` and `warning` colours for states drawn by
  widgets themselves.

- `AwraNavigationItem` carries what a list of people or conversations
  shows: a `prefix` widget in place of the icon (an avatar, a swatch), a
  quieter `subtitle` line, `emphasized` for a destination with something
  new (full contrast and weight) and `muted` for a silenced one, which
  recedes until hovered or selected.
### Conversations

- `AwraConversationView` is a virtualized timeline for chats, comment threads,
  logs and consoles. It starts at the end and follows appended items, which
  glide into place and rise in; scrolling up stops following and a pill offers
  to jump back, counting unread items. Older history inserted above never moves
  what is on screen, `edge-reached` asks for more at either end, and
  `awra_conversation_view_scroll_to_item()` brings an item into view and can
  flash it.
- `AwraComposer` is the field where a message is written: one well with an
  optional banner ("Replying to…"), attachments, start and end widgets and a
  send button that grows in when there is something to send. The text grows
  to `max-lines` with a smooth change of height; Enter sends, Shift+Enter
  starts a new line, and an input method composing keeps Enter.
- `AwraCompletionPopover` shows suggestions at the caret of a text view or an
  entry (mentions, emoji shortcodes, commands) without taking the focus: the
  arrows move through them, Tab and Enter choose, Escape dismisses.
- `AwraDivider` separates content with a line and an optional label, centred
  or at the end, in a neutral, accent or danger appearance: day separators and
  the line where unread messages start.
- `AwraEmojiPicker` and `AwraEmojiItem` pick emoji, custom emoji and stickers
  from sections the application provides, with a search across sections,
  tabs that follow the scrolling, a virtualized grid with section titles,
  keyboard navigation, a large preview and a recent section.
- Rust bindings cover `AwraConversationView`, `AwraComposer`,
  `AwraCompletionPopover`, `AwraDivider`, `AwraEmojiPicker` and
  `AwraEmojiItem`.

### Desktop workspace composition

- `AwraFormRow` asks for its side-by-side width as natural width, so a container
  fitting it to a height can no longer report inconsistent sizes.
- Scrolling menus and their shortcut labels resist hostile themes, and theme
  slider margins can no longer give an Awra scrollbar a negative size.
- `AwraTileView` divides a surface into as many tiles as the user wants: tiles
  divide again in either direction, dividers resize their neighbours, and
  adding, removing or moving a tile animates without reparenting any child.
- `AwraDataView` no longer keeps the height of its hidden state pages, so a list
  can be made small.
- While a display at scale 1 is connected, the style manager keeps text on the
  pixel grid with the desktop's hinting, so small labels no longer lose the dot
  of an i or the top of their figures depending on where a window sits.
- Rust bindings cover `AwraPathBar`, `AwraDragPreview`, `AwraTransitionBin` and
  `AwraTileView`.
- `awra_path_bar_get_item_at()` and `awra_path_bar_set_drop_item()` let each
  breadcrumb receive a drop.
- Style roles `inline-edit`, `inline-action` and `usage-meter` cover editing in
  a row, actions inside a row and capacity gauges.
- Style role `spaced-rows` keeps a small gap between the rows of a table, so
  neighbours selected together still read as separate items.
- Style roles `about-to-open` and `refused` announce a place opening under a
  held drag and an input that was turned down.
- `AwraPopover` content unfolds like menus do, and follows reduced motion.
- `AwraTransitionBin` animates content replaced in place, such as a list that
  receives another model, from the last frame it showed.
- Rows, breadcrumbs, tabs and menus ease between states; presses give way,
  menus unfold and menu chevrons turn, within the reduced-motion preference.
- `AwraDragPreview` shows what a drag carries: one item as itself, several as
  a pile that fans out, with the total in a badge.
- Toolbar buttons and menu buttons acknowledge the pointer like ghost buttons.
- `AwraHeader:compact` provides compact titlebar proportions without changing
  the default header. `AwraSelectionToolbar:clear-label` customizes the
  localized clear-selection action.
- Public workspace roles cover embedded tabs, dense navigation and tables,
  pane focus, inline filtering, floating selection and docked document previews.
- Metadata values ellipsize with a full-value tooltip in narrow panes.
- Breadcrumb reveal runs after viewport allocation, keeping the visible path
  synchronized with the adjustment when a pane changes size.
- `AwraZoomController` adds scoped, bounded Ctrl+wheel content zoom. Text follows
  the desktop font, ordinary scroll input remains native, and `scale` notifications
  let consumers resize domain imagery without replacing their models.
- `canvas-table` provides transparent column guides for continuous-Canvas layouts.
- `AwraHeader:expand-center` fills the real gap between side controls for
  shared navigation/address bars, preserving the centered default.
- `drop-zone` follows GTK’s accepted-drop state for reusable destination feedback.
- `AwraPathBar` combines breadcrumbs with an in-place text field: click, Tab
  completion from consumer-supplied suggestions that insert without committing, refusal without losing the
  typed text, and a trailing widget slot that survives both modes.
- Tabs ease into their slots when reordered or closed. A drag can lift a tab out
  of its bar with `awra_tab_view_set_page_dragged()` and open an empty slot with
  `awra_tab_view_show_placeholder()`, positioned stably by
  `awra_tab_view_get_insertion_position()`.
- The floating selection rises into place, unless motion is reduced.
- A middle click closes a closable tab. `AwraTabView:scroll-selects` makes
  scrolling over the tab bar move the selection rather than the strip.
- Menus built from a `GMenuModel` now show the state of boolean and radio
  actions, and the shortcut registered for an action or set through the `accel`
  item attribute.
- `location-search` and `prose-metadata` roles cover a search field in a location
  slot and metadata read as text rather than compared as figures. Toolbar
  buttons in a compact header match its control height.

### Framework updates

- `AwraGridView` adds a public C/GI virtualized grid with shared GTK selection
  models, item factories, rubberband selection and semantic interaction styles.
- `AwraTextArea` applies its semantic foreground directly to GtkTextView so
  read-only previews retain contrast in both light and dark appearance.
- Breadcrumbs scroll horizontally, reveal the current segment, and respect RTL
  without making a long path the window minimum width.
- Empty/loading/error descriptions wrap in narrow panes; data table header
  padding no longer causes fixed-column row underallocation.
- `AwraSurface:backdrop-widget` samples an independent local widget behind a
  frosted material. Blur runs on bounded GSK nodes, follows accessibility
  fallback and does not copy pixels through consumers. `AwraHeader` exposes
  its surface and an optional centred maximum width with pointer pass-through
  outside the visual chrome. C/GI/Vala/Rust and GtkBuilder expose the additions.

- `AwraTimeline` exposes configurable `step-increment` and `page-increment`
  properties so local and global transport shortcuts can use consistent steps.
  Defaults and editable-mode scaling remain compatible; C/GI/Vala/Rust expose
  both properties.

- Responsive layout: `AwraMasterDetail`, `AwraFilterBar` and `AwraFormRow`
  are measured in the layout their width gives, so no frame is drawn with the
  previous layout's sizes after creation or a resize across a breakpoint.
  `AwraMasterDetail` takes the pane hidden in compact mode off screen without
  changing its visibility, and stays compact while both panes do not fit.
- `AwraSplitView` keeps a divider the user dragged until its mode changes.
- `AwraResponsiveBin` and `AwraSplitView` accept their compact and expanded
  widths in either order.
- `AwraNavigationView` slides forward on push and back on pop.
- Windows: Win+arrows snap and resize windows (Snap, FancyZones); system
  preferences follow Windows' change broadcasts instead of a poll, and no
  longer override an application's own GTK settings; the DLL exports only
  the public API; a Windows CI job builds and tests Awra.
- Rust bindings regenerated: the 1.1 API (dialogs, key caps, keyboard view,
  media widgets...) behind the default `v1_1` feature; both crates carry
  crates.io metadata and are ready to publish.
- `AWRA_CHECK_VERSION ()`; the version header is checked against the
  project's version.

- Make the outer window frame square and undecorated by default when maximized
  or fullscreen. Add `AwraWindow:keep-rounded-corners` (default false) to retain
  rounding per application, including live changes and fullscreen with a hidden
  native titlebar. Restoration preserves the configured surface geometry;
  floating controls and overlay chrome retain their own rounding.

- Add decoder-independent `AwraFramePool` / `AwraFrame` for bounded aligned
  RGB and planar YUV storage, immutable texture leases without pixel copies,
  safe producer-to-consumer lifetime, and alpha-correct bilinear previews in
  another bounded pool. `AwraTextureStream` updates multiple
  pictures without per-frame layout or CPU mipmap generation;
  `AwraAspectPaintable` handles non-square source pixels. C, GI and Rust
  consumers cover ownership, color conversion and threaded production.

- Add `AwraOverlayBar` with full-canvas allocation, floating controls, idle
  policy, hover/focus protection and reduced-motion support.
- Add opt-in overlay chrome to `AwraWindow`, timeline-owned pointer previews
  and toast bottom clearance. Dock/overlay transitions preserve native surfaces.
- Fix early window title bindings and full-transfer dialog/palette constructors
  so retained language bindings remain valid after destruction. Transient
  dialogs join their parent's application and support Escape dismissal.
- Forward focus through search, spin, switch and password composites.

- Add the introspectable `AwraTimeline` for backend-independent seeking,
  interval/point markers, viewports, hover previews and cancellable offset edits.
  Keyboard, RTL and `GtkAccessibleRange` semantics are included.
- Add the `collection-view` style role for virtualized GTK grid/list factories.
- Exercise the additions in Showcase, native tests and Python/Rust consumers.

- Windows support: a `win32-dwm` effect backend (Acrylic system backdrop on
  Windows 11 22H2+, accent-policy blur on Windows 10), Windows dark mode,
  accent colour, high contrast and animation preferences mirrored into GTK,
  native DWM shadow and corners for Awra windows, relocatable translations,
  and a MinGW cross file (`build-aux/windows/mingw64.meson`).
- Keyboard helpers map physical keys to the platform's hardware keycodes
  (virtual keys on Windows), so layout-aware legends work there too.
- The command palette shows icons, accelerators and section headings.

- Added `AwraKeyCap`, `AwraKeyboardView` (ANSI/ISO, layout-aware legends,
  selection, roving focus, per-key file drops) and `AwraShortcutCapture`,
  plus public `KeyboardEvent.code` ↔ hardware keycode helpers.
- Added `AwraWaveformView`, `AwraVolumeControl`, `AwraMediaRow` and
  `AwraDropZone` for media workspaces.
- Added toast actions to `AwraToastOverlay` (`show_with_action()`,
  `dismiss()`, `get_current_toast_id()`, `dismissed`); `show_message()` is
  unchanged.
- The safe Rust crate projects the new family and exposes motion duration and
  easing, context-menu and key-code helpers.
- Showcase gains a soundboard pattern and a media workspace built on
  `AwraMediaRow`.

## 1.0.0 — 2026-08-19

- Added typed page, section, toolbar, forms, application states, filters,
  master/detail and detail-pane composition.
- Added virtualized data views, selection toolbar, advanced inputs, responsive
  bins, breadcrumbs, command palette and dense data components.
- Added accessibility/RTL, hostile-theme, lifecycle, performance and public
  consumer gates.
- Added compositor-neutral diagnostic export and Inspector region/overdraw
  tooling.
- Added generated `awra-sys` and safe gtk-rs bindings with a permanent Rust
  example; all GTK delegate/model signatures are projected and a regression
  gate rejects unsupported generated stubs.
- Added staged packaging, documentation install, Flatpak manifest and CI.
- Added gettext-backed framework strings, installed `gi-docgen` reference and
  an external AT-SPI/Orca bridge audit.
- Made every resolved material field focus-invariant and stabilized hostile
  theme/popup captures at their final rendered state.
- Fixed source distribution so `build-aux` generation and validation scripts
  cannot be excluded by the build-directory ignore rule, while Cargo build
  artifacts remain excluded; normalized dist mtimes make repeated archives
  byte-identical.
- Stabilized the `Awra-1.0` GIR namespace and `libawra-1.so.1` ABI after the
  complete Showcase and reference-application gates.

## 0.1.0 — 2026-08-17

- Initial experimental C/GObject framework, semantic materials, AwraWindow,
  core controls, overlays and `ext-background-effect-v1` fallback backend.
