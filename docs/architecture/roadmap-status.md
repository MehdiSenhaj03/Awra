<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Roadmap execution status

This ledger records evidence for the gates defined by
`improvement-roadmap.md`. The roadmap remains the source of truth for scope and
ordering; this file records execution rather than replacing it.

## Phase A — complete (2026-08-18)

### Runtime and lifecycle

- `AwraContext` idempotence is exercised repeatedly for one display.
- The native stress fixture runs 100 map/popdown cycles by default and 200 in
  the sanitizer gate, plus nested popovers, context menus, two simultaneous
  windows, 25 successive mapped dialogs, pending toast timeouts and edge-panel
  motion followed by parent destruction.
- Native surface/effect counters return to zero after final destruction.
- Recoverable native-effect failures are diagnostic fallbacks, not fatal GLib
  warnings.
- The Wayland backend keeps registry observation on GDK's dispatch queue after
  initial discovery, handles removal/re-announcement of the effect global and
  never creates a second display-fd reader.

### Focus, materials and GTK isolation

- A full page composition now covers root Window/Canvas, inset Sidebar/Chrome,
  navigation rows, toolbar/filter roles, Card/Layer, controls and content.
- Its allocated pixels and geometry are identical under active/backdrop state.
- Canvas resolution proves identical fill, radius and blur request; sidebar
  collapse cannot alter the root effect count.
- Reduced transparency removes the native request and restoring the preference
  restores the same request.
- The existing theme/user-provider matrix continues to compare Breeze,
  Breeze-Dark, hostile user CSS and a runtime USER provider byte-for-byte.

### API, introspection and consumers

- ELF exports are generated from installed headers. Twenty-eight resolver,
  coordinator, backend and other implementation symbols that previously leaked
  are hidden.
- `tests/api/phase-a-contract.txt` protects the provisional Material, Surface,
  Window, Context and Overlay symbol set from accidental removal.
- GIR nullability and ownership are machine checked. The audit corrected
  `AwraDropdown.new_from_strings`, which previously projected as an opaque
  string pointer instead of a zero-terminated UTF-8 array.
- Permanent consumers cover C, Vala, Python/GI and experimental Rust. The Rust
  consumer checks the GIR shapes needed for future `Option<T>`, owned objects
  and string slices while using gtk-rs ownership traits against the C ABI.
- A clean staged installation validates pkg-config, typelib, VAPI, all three
  installed-style consumers and absence of libadwaita from `ldd`.

### Sanitizers and gate result

Clang ASan/UBSan passes the complete non-introspection suite with
`G_DEBUG=fatal-warnings`, 200 lifecycle iterations and leak detection disabled
only for process-global GTK/Pango/font caches. GIR and its consumers are tested
in the regular build because the local `g-ir-scanner` toolchain cannot link its
temporary executable against Clang's sanitizer runtime filenames.

The Phase A contracts are documented in `docs/api/stability.md`. Phase B can
therefore build typed page primitives on top of the frozen foundation.

## Phase B — complete (2026-08-18)

### Typed page foundation — validated

- `AwraPage`, `AwraPageHeader`, `AwraSection` and `AwraToolbar` are public,
  introspectable, GtkBuilder-constructible widgets with nullable child/action
  ownership documented in GIR.
- A mapped 1200-pixel fixture proves centered max-width layout, default token
  insets, accessibility roles, replacement/removal and destruction.
- The hostile-provider matrix covers a realistic nested typed page. It found
  and fixed both GTK-theme scrollbar leakage and inherited button-label
  backdrop styling; Breeze, Breeze Dark, user CSS and a runtime USER provider
  are again byte-identical.
- C, Vala, Python/GI and the permanent experimental Rust consumer exercise the
  new projection. Showcase now constructs every screen through `AwraPage` and
  `AwraPageHeader`, uses `AwraSection` headings, and its media pattern uses
  `AwraToolbar`, all exclusively through public API.
- The first mapped test also caught a consumed `GskTransform` being released a
  second time; the ownership bug was fixed before exposing the components to
  applications.

### Forms and application states — validated

- `AwraForm` and responsive `AwraFormRow` own tokenized row rhythm, accessible
  label/description/invalid relations, required and validation state.
- Typed empty/loading/error views centralize state hierarchy and actions;
  reduced motion disables loading animation without removing status semantics.
- GtkBuilder, mapped responsive behavior, destruction, hostile CSS and all
  four language consumers are covered. Showcase's Inputs page uses these
  primitives without local CSS.

### Filters and master/detail — validated

- `AwraFilterBar`, `AwraMasterDetail` and `AwraDetailPane` are public,
  introspectable and GtkBuilder-constructible. Compact/Medium/Expanded
  allocation is framework-owned and logical pane navigation remains explicit.
- Their pixels and geometry survive the complete hostile-provider matrix.
  Tests also compare FilterBar, MasterDetail and DetailPane separately across
  active/backdrop state.
- C, Vala, Python/GI and the early permanent Rust consumer exercise the API.
  Showcase's research workspace now uses the typed responsive composition.
- The public role catalogue and GTK/libadwaita migration guide distinguish
  temporary presentation roles from typed behavioral widgets. Blueprint now
  demonstrates Page, Section, FilterBar and Form construction.

### Reference-application migration — validated

- Showcase uses the typed page, section, toolbar, form, state and
  master/detail families through public API only.
- a reference application real catalogue filter composition now uses `AwraFilterBar` while
  retaining its GTK models and callbacks. This removes its manual
  `toolbar-card` composition from that page; the Rust port, public-boundary
  check and every page/detail/dialog smoke path pass.
- The migration is recorded in a reference application `AWRA_PORT_REPORT.md`; it confirms
  the new primitives can be adopted incrementally rather than forcing an
  application rewrite.

Phase B is complete. Phase C begins with the virtualized data-view family and
selection toolbar.

## Phase C — complete (2026-08-19)

### Virtualized data and selection — validated

- `AwraDataView` wraps `GtkColumnView` without replacing its selection model,
  factories, sorters or virtualization. It owns horizontal/vertical scrolling,
  Content/Empty/Loading/Error states and incremental-loading feedback.
- `AwraDataColumn` exposes title, factory, sorter, alignment, fixed width,
  expansion and resizing; `AwraSelectionToolbar` derives its count and clear
  behavior directly from `GtkSelectionModel`.
- GtkBuilder, selection mutation, column lifecycle, hostile GTK providers,
  Showcase and C/Vala/Python/Rust projections pass. The Showcase data
  workspace demonstrates multiple selection and contextual actions.

Phase C work item 2 now covers advanced inputs and reusable settings patterns.

### Advanced inputs and settings — validated

- `AwraTextArea`, `AwraPasswordEntry`, `AwraTagEntry`, `AwraChoiceGroup` and
  `AwraStatusBanner` preserve GTK editing/accessibility behavior while owning
  the complete Awra presentation.
- GtkBuilder, hostile CSS, C/Vala/Python/Rust projection and Showcase's real
  Inputs page cover the family. Date/time/color/file wrappers remain deferred
  under the roadmap's demonstrated-use-case rule.

### Media and keyboard input — validated

- `AwraKeyCap`, `AwraKeyboardView`, `AwraShortcutCapture`,
  `AwraWaveformView`, `AwraVolumeControl`, `AwraMediaRow`, `AwraDropZone` and
  toast actions were added for a reference application and kept domain-neutral.
- Widget behaviour/lifecycle, accessibility roles/Tab/RTL, hostile-provider
  isolation, GtkBuilder, GIR/API baselines, C/Python/Vala consumers, the
  generated Rust projection and the Showcase soundboard and media workspace
  cover the family. Native drag-and-drop remains a manual Showcase check
  because GTK offers no public drop synthesis.

### Responsive navigation and overlays — validated

- `AwraResponsiveBin` provides observable Compact/Medium/Expanded size
  classes without imposing a window/sidebar architecture. Existing
  `AwraSplitView`, inset `AwraSidebar` and `AwraEdgePanel` cover persistent,
  floating and edge-revealed compositions.
- Navigation history now remembers and restores page focus. Typed breadcrumb
  items consume `GListModel`; `AwraCommandPalette` consumes `GMenuModel`,
  filters actions and restores focus after Escape or activation.
- Showcase demonstrates breadcrumbs and the command palette, while widget,
  lifecycle, GIR and all four language consumers guard the contract.

### Dense data components — validated

- `AwraMetricRow`, `AwraStatBar`, `AwraBadgeGroup` and `AwraMetadataGroup`
  remove recurring manual compositions found in Showcase and a reference application
  detail views. They remain domain-neutral and GtkBuilder compatible.
- The deterministic visual suite already validates Light/Dark, custom blue
  accent and an automatically generated grayscale rendering with luminance
  variance and edge-structure thresholds. Light contrast tokens have explicit
  WCAG-oriented unit gates for text, states, disabled content and focus.

Phase C's public component set and Light/artistic gate are complete. Its final
cross-cutting work covers common loading/motion state, accessibility and RTL
audit, performance budgets, diagnostics export and CI consolidation.

### Loading, accessibility and RTL — validated

- `AwraSkeleton` provides cached, allocation-free snapshot feedback and obeys
  reduced motion; the existing loading state, progress bar and incremental
  data loading cover determinate and indeterminate application feedback.
- The accessible audit corrected navigation destinations to the `BUTTON`
  role, publishes meter values for `AwraStatBar`, verifies real editable focus
  and covers Tab navigation with long translated text.
- The live RTL test found and fixed `AwraEdgePanel` listening for the wrong
  direction notification. Logical START/END now mirrors after a runtime
  direction change, not only when initially constructed.
- `accessibility-and-localization.md` records the public contract and the
  manual Orca, large-font and localization release checklist.

### Performance and diagnostics — validated

- Grain is a single process-wide cached texture. A repeatable page benchmark
  records CPU snapshot time with strict reference and non-flaky CI ceilings.
- The native coordinator now deduplicates unchanged region geometry. Its
  monotonic update counter is regression-tested across token/accent changes.
- `AwraDiagnostics` exports backend-neutral region geometry and a plain-text
  report. Inspector reports allocation, scale, direction, accessibility,
  resolved surface/material/tokens and active/inactive invariants, with opt-in
  effect-region and overdraw visualizations.
- Showcase's Platform page consumes those public diagnostics directly.

### CI consolidation — validated

- Meson now gates the public-consumer boundary: no application-local CSS,
  private Awra include, direct Wayland/KWin symbol or libadwaita API may enter
  Showcase, the compiled guides or permanent consumers.
- The CI definition builds with fatal warnings, runs the complete suite under
  a virtual display, validates a staged non-standard-prefix installation,
  builds the distribution archive, and has a separate Clang ASan/UBSan job
  with the lifecycle stress count raised to 200.
- Deterministic visuals, hostile providers, external C/Vala/Python/Rust
  consumers and the performance ceiling are regular Meson tests rather than
  release-day manual commands.

Phase C is complete. Phase D begins with generated Rust bindings and their safe
gtk-rs layer.

## Phase D — complete (2026-08-19)

### Generated Rust bindings — validated

- `bindings/rust/awra-sys` is generated from `Awra-1.0.gir` by the official
  gtk-rs `gir` tool and covers the complete exported ABI; its generated C/Rust
  layout and constant tests pass.
- `bindings/rust/awra` projects the public objects, enums, flags, properties
  and builders as safe gtk-rs types. GTK widgets and models remain visible,
  rather than being hidden behind an application-specific façade.
- The one unsupported GIR shape (`GError` report export) has a small explicit
  safe extension using Rust `Path`/`io::Result`; the normal generated report
  getter remains the source of its data.
- A permanent executable constructs Page/PageHeader/Card/Inspector, verifies
  widget ownership and exports a report. Meson compiles and runs both the full
  binding workspace and this example offline.

### Packaging, documentation and compositor matrix — validated

- Installation now includes versioned architecture/API/guide documentation in
  addition to library, headers, pkg-config, GIR, typelib, VAPI and Showcase
  metadata. Desktop and AppStream validation are Meson gates.
- A least-privilege GNOME 50 Flatpak manifest is machine-checked and documents
  the native-effect fallback across the sandbox boundary.
- A real fallback dependency fixture builds Awra as a Meson subproject with
  tests, Showcase and introspection disabled. CI also stages `/opt/awra`, runs
  installed consumers and produces a clean source archive.
- The compositor matrix separates automated generic/X11/Wayland-fallback
  coverage from the state-restoring KWin built-in/Better Blur DX/manual
  fractional-scaling gate. It documents compositor-owned rendering honestly.
- Standalone C page, Python settings and Vala navigation applications are
  compiled and smoke-run alongside the generated Rust application and the
  Blueprint build fixture. All are installed as readable onboarding sources
  and pass the same public-only boundary gate.

Phase D is complete.

## Gate 1.0 — complete (2026-08-19)

### Stable contract and reference applications

- The complete non-documentation `Awra-1.0` GIR contract is frozen in
  `tests/api/awra-1.0-api.json` (later releases are checked against it and
  `tests/api/awra-api.json` holds the current surface); public ELF symbols, SONAME
  `libawra-1.so.1`, nullability, transfer, properties, signals and enum values
  are machine checked.
- The generated C, Python/GI, Vala and Rust consumers pass against the clean
  1.0 installation. The early Rust projection gate caught the missing
  `Gio.ActionMap` implementation on `AwraWindow` and initially unsupported GTK
  delegate/model types. The generator configuration now projects every such
  type safely, while a deterministic post-generation/test gate rejects both
  missing `ActionMap` support and any commented unsupported stub.
- Showcase and the complete a reference application port pass the public-consumer boundary
  with no local CSS, private Awra header, libadwaita, compositor API or direct
  Wayland call. A fresh a reference application build links `libawra-1.so.1` and smoke-runs all
  eight pages, four entity details, tab closure and both workspace dialogs.

### Visual, accessibility and lifecycle qualification

- Material paint is now entirely focus-invariant, including Floating border,
  highlight and shadow. The real-window probe is byte-identical across Breeze,
  Breeze Dark, hostile user CSS and a runtime USER provider; five repeated
  runs also eliminate transition-sampling flakiness.
- Light/Dark contrast, high contrast, reduced transparency/motion, custom blue
  accent and the derived grayscale scene pass their deterministic gates.
- GTK role/focus/RTL assertions pass. The external
  `tests/manual/atspi-smoke.py` audit exposes the mapped Showcase through the
  same AT-SPI bridge used by Orca, with frame, button, label, entry, list and
  form semantics plus named destinations. Human speech-output review remains
  in the documented release checklist because it cannot be represented by a
  truthful pixel or tree assertion.
- Clang ASan/UBSan passes 18/18 tests with fatal warnings and 200 native
  lifecycle iterations. The ordinary Wayland build passes 27/27; the
  Wayland-disabled build passes 18/18, including its Showcase fallback smoke.

### Distribution and compositor qualification

- A staged install validates version 1.0.0 through pkg-config, C, Python/GI
  and Vala, installs headers/GIR/typelib/VAPI/docs and has no libadwaita entry
  in `ldd`. A real Meson fallback subproject also builds and runs.
- `gi-docgen` generates and installs a structurally complete HTML reference;
  gettext extraction produces the `awra-1` catalogue. Desktop, AppStream and
  Flatpak contracts pass.
- A clean temporary Git tree produces `awra-1.0.0.tar.xz`. This gate found and
  fixed an overbroad ignore rule that had excluded the source `build-aux`
  directory from archives and excluded local Cargo artifacts. A dist hook
  normalizes copied mtimes to `SOURCE_DATE_EPOCH` (or the source commit), and
  CI requires two consecutive archives to be byte-identical.
- The state-restoring KWin transaction proves the standardized backend with
  built-in blur only (`interface=yes, blur=yes`), the opaque fallback with all
  blur effects disabled (`interface=yes, blur=no`), then restores the initial
  `better_blur_dx=1`, built-in `blur=0` state.

All roadmap phases and the stable 1.0 gate are complete. Future compatible 1.x
work follows `CONTRIBUTING.md`, the API baseline and the design-system ADRs.
