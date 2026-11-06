<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Foundation validation record

Validated on 2026-08-18 with GTK 4.22.4, KWin/Wayland, GCC 16.1 and Clang
22.1. The regular and Wayland-disabled builds both pass the material, widget,
forced-fallback, and native lifecycle suites. The lifecycle stress test opens
and closes an `AwraPopover` one hundred times by default (two hundred in the
sanitizer gate) and verifies tracked/effective native
surface counts return to zero after destruction. It also exercises a nested
menu, second-click dismissal, parent dismissal while the submenu is mapped and
action-driven dismissal of the complete chain. Two simultaneous roots,
successive mapped dialogs, context menus, an edge panel and a pending toast
timeout cover destruction ordering.

The ASan/UBSan build passes all suites with Clang. Leak detection is disabled
for the GTK integration process because GTK, Pango, and Fontconfig retain
process-global caches; invalid access and undefined-behavior detection remain
enabled.

## KWin proof

The initial compositor state was `better_blur_dx=true`, built-in `blur=false`.
The transaction in `tests/manual/kwin-effect-proof.sh` produced:

```text
built-in blur only: interface=yes, blur=yes
no blur effect loaded: interface=yes, blur=no
restored: better_blur_dx=true, blur=false
```

This proves that Awra uses the standardized capability rather than naming or
depending on either KWin effect. The script records both initial values and
restores them through an `EXIT`, `INT`, and `TERM` trap.

Installed consumers were also compiled through `awra-1.pc`, imported through
`Awra-1.0.typelib` in Python, and compiled through the generated `awra-1.vapi`.
`ldd` contains GTK/GDK/Wayland dependencies and no libadwaita dependency.

## Extended library gate

The responsive layout, control, overlay, navigation, motion and display layers
are covered by the widget suite. This includes GTK Builder construction,
editable text, grouped selection, dropdown/list models, dialog transience,
menu models, toast state, navigation history, segmented selection, tabs,
progress, badges and weak inspector targets.
Public edge panels are covered in persistent and auto-hide configurations,
including logical edge, sizing, Floating material and reduced-motion wiring.
The public layout test resolves NONE/XS through XXL from the active token set,
checks Page, Section and Toolbar recipes on GTK boxes/grids, and verifies that
new Cards and Sidebars receive their LG and MD semantic insets without
application margins. Showcase composition contains no raw spacing pixels.
The suite also covers ActionRow semantics and attach/replace/remove lifecycle
for framework-owned context menus.
Per-material tuning tests modify Floating through one descriptor and verify a
second descriptor plus resolved surfaces receive the same tint, radius, grain,
decoration and blur policy. Native lifecycle coverage also disables Canvas
blur at profile level, verifies the compositor effect is removed, then resets
the profile and verifies it is restored. Chrome resolution is required to keep
a non-zero semantic corner radius.

The visual suite snapshots a fully allocated Frosted scene directly into GSK,
checks repeat snapshots serialize identically, and proves Light and Dark scenes
produce distinct render trees. It then resolves a custom blue accent, renders
the complete control/material scene, converts it to grayscale and asserts both
luminance variance and edge structure survive. With
`AWRA_VISUAL_OUTPUT_DIR`, it exports `awra-blue.png` and
`awra-blue-grayscale.png`. It captures only the Awra fixture and never the
desktop. The generated GIR is scanned with `--warn-all` and a Vala API is built
from it.

## Visual isolation and Light audit

The isolation probe runs four fresh GTK processes with explicit Awra Light:
Breeze, Breeze Dark, hostile `$XDG_CONFIG_HOME/gtk-4.0/gtk.css`, and a hostile
runtime provider installed at USER priority. It verifies identical allocations
and rendered RGBA hashes for Button, Toggle, Entry/Search/Spin, Switch, Slider,
Check, Dropdown, Progress and Segmented controls. The hostile rules attempt to
replace colors, gradients, borders, radii, outlines, shadows, opacity, padding
and minimum sizes. The Dropdown popup is opened and snapshotted as its own
native surface; its hash and allocation must also remain identical. The Awra
menu popup is opened and checked the same way. The fixture additionally covers
the explicit `AwraButton`/`GtkPopoverBin` menu trigger, navigation rows and the
public semantic typography API.

The actual local user stylesheet was also investigated. It previously changed
entry/spin metrics, switch/slider geometry, checkbox and dropdown assets, CSS
transition state, and titlebar control metrics. After the framework fixes,
every probed Awra component and the titlebar allocation have the same rendered
hash as clean Breeze, with GTK warnings fatal. `GtkWindowControls` still owns
the desktop operation layout and symbols; Awra now protects their safe metrics
and surrounding button chrome.

The a reference application validation also exposed a generic-class collision: the desktop
theme's `.sidebar-pane:first-child` rule painted an opaque 9000-pixel shadow
beside the sidebar only while the window was active. All public style roles are
now mapped to private `awra-role-*` classes. The regression fixture carries the
same hostile rule, and real active/inactive captures of a reference application Frosted
window compare with zero differing pixels while retaining the native blur
request.

`AwraDropdown` retains a non-rendered `GtkDropDown` only as its compatibility
model/expression delegate. Its visible button is an `AwraButton`, and its
mapped popup is `AwraPopover` with the Floating material. Native lifecycle
coverage verifies that Floating blur can be removed and restored while this
popup is mapped and that row activation commits selection before dismissal.

Light tokens are guarded by contrast tests: primary text/background and
text/control exceed 12:1, secondary text exceeds 4.5:1, pressed/selected text
exceeds 7:1, and disabled/focus affordances exceed 3:1. Light outlines remain
stronger than separators and both are strengthened in high contrast. The Light
canvas remains at least 0.85 opaque; its artistic calibration is intentionally
secondary to the Dark reference for this pass.

The semantic material matrix additionally verifies transparent Content,
edge-only Chrome, borderless Layer, native-blur Canvas/Floating and Opaque
fallback behavior. Active/inactive comparisons keep every resolved field
identical, including fill, grain, geometry, border, highlight, shadow and blur
request.

The Showcase smoke test constructs all eleven routed pages, visits each page,
resolves both appearance profiles, updates live diagnostics and displays a
toast with GTK warnings fatal. It complements manual interaction testing of
menus, popovers, dialog, context clicks, search, tabs, master/detail selection,
push/pop navigation, responsive panels, persistent floating panels and
edge-revealed inspectors. The Showcase header is installed as
public `AwraWindow` chrome; the same API is covered with default/custom chrome
restoration and embedded-header tests.

## Awra 1.0 release qualification

Validated on 2026-08-19 after the complete roadmap execution:

- regular Wayland build: 27/27 Meson tests, including the complete safe Rust
  projection guard;
- compile-time Wayland-disabled build: 18/18 tests, including Showcase
  fallback;
- Clang ASan/UBSan: 18/18 tests and 200 lifecycle iterations;
- clean staged install, Meson subproject consumer, gettext catalogue and
  structurally complete installed `gi-docgen` reference;
- clean Git-tree `meson dist`: `awra-1.0.0.tar.xz`;
- fresh a reference application Rust target linked to `libawra-1.so.1`, with tests, Clippy,
  public-only boundary and all page/detail/dialog smoke paths passing;
- external AT-SPI/Orca bridge audit of mapped Showcase semantics;
- KWin built-in-only and no-blur fallback transaction, with the initial
  Better Blur DX/built-in state restored exactly.
