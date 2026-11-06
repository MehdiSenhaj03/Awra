<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Visual isolation contract

Awra keeps GTK behavior and desktop accessibility settings, but owns the
identity properties of widgets carrying an `awra-*` class. The dynamic Awra
provider is installed once per `GdkDisplay` at
`GTK_STYLE_PROVIDER_PRIORITY_USER + 1`. It therefore wins over the GTK theme,
`GTK_THEME`, `$XDG_CONFIG_HOME/gtk-4.0/gtk.css`, and providers installed at
`GTK_STYLE_PROVIDER_PRIORITY_USER` for those scoped properties.

This deliberate exception to GTK's usual “user CSS wins” rule is limited to
Awra's shape and palette: foreground/background colors, borders, radii,
identity shadows, outlines, images, padding and control minimum sizes. Awra
does not protect the base font family or size, DPI/scaling, cursors, input
method, keyboard behavior, text selection behavior, or accessible roles. The
style manager also follows the GTK 4.22 system color-scheme, increased-contrast
and reduced-motion settings until an application explicitly overrides the
corresponding Awra preference. Reduced transparency remains an explicit Awra
preference because GTK exposes no matching setting.

## Cascade and CSS nodes by component

| Public widget | GTK behavior / CSS nodes retained | Awra-owned visual layer |
| --- | --- | --- |
| `AwraSurface`, `AwraCard`, sidebar and root window surface | Child fonts, scaling and accessibility | GSK fill/tint, grain, radius, border, highlight and elevation shadow; transparent GTK node |
| `AwraButton`, `AwraToggleButton` | GTK button actions, focus, keyboard and accessible role; `button` | `.awra-button` normal, hover, pressed/checked, disabled, focus-visible and backdrop |
| `AwraMenuButton` | GTK button action/focus plus public `GtkPopoverBin` anchoring | `AwraButton`, framework-drawn chevron and a true `AwraPopover`/Floating surface; no rendered private menu-button node |
| `AwraEntry`, `AwraSearchEntry`, `AwraSpinButton` | GTK editable, IME, caret and selection; `entry > text`, images and spin buttons | `.awra-entry`, `.awra-search-entry`, `.awra-spin-button` chrome, selection and state colors |
| `AwraSwitch` | GTK switching semantics; `switch > image/image/slider` | `.awra-switch` trough, hidden theme glyphs, thumb and all states |
| `AwraSlider` | GTK range semantics; `scale > trough > highlight/fill`, `slider` | `.awra-slider` track, fill, thumb, focus, hover and disabled states |
| `AwraCheckButton` and grouped radios | GTK grouping and accessible state; `checkbutton > check/radio` | `.awra-check` indicator geometry, marks, selected/focus/hover/disabled states |
| `AwraDropdown` | GTK model/expression and single-selection semantics; public `GtkPopoverBin` anchoring | `AwraButton` closed well plus a true `AwraPopover`/Floating surface, protected list rows and framework-drawn chevron |
| `AwraProgressBar`, segmented controls and tabs | GTK progress/toggle/stack behavior | `.awra-progress`, `.awra-segmented`, `.awra-segment`, `.awra-tabs`, `.awra-tab-bar` and descendants |
| `AwraListView` | GTK selection/model/factory; `listview > row` | `.awra-list` row normal, hover, selected and keyboard-focus states |
| `AwraBadge` | Application text plus a semantic appearance | Framework-owned Neutral/Accent/Info/Success/Warning/Danger contrast in Light and Dark |
| `AwraNavigationItem` | GTK toggle grouping, keyboard and tab semantics | Borderless row, icon/label, hover, selected accent layer and focus-visible ring |
| Public typography | Desktop font family, DPI and scale | Semantic Body/Muted/Eyebrow/Title/Display/Monospace color, weight and relative size via `awra_widget_set_typography()` |
| Public data-application roles | GTK table, list, flow, drawing-area and button behavior | Opt-in dense data recipes supplied by Awra through `awra_widget_add_style_role()`; applications own neither a CSS provider nor private class names |
| `AwraPopover`, menu and tooltip | GTK native popup lifecycle and dismissal | transparent popup shell plus Awra GSK material; `.awra-menu*` and protected tooltip chrome |
| Window controls | Public `GtkWindowControls` behavior, operations, symbols and desktop button layout | Awra titlebar metrics, button chrome and states; familiar platform symbols/layout remain intentional |

GTK does not expose a public API that enumerates every provider in a display's
cascade. The sources above are therefore verified by construction and by the
`style-isolation` multi-process test. That test renders the same fixed Light
scene under Breeze and Breeze Dark, then repeats it with hostile XDG user CSS
and a hostile runtime provider at USER priority. It requires identical rendered
RGBA pixels and allocations for closed controls, the mapped Dropdown popup and
the mapped menu popup in all four processes; testing only closed controls is
insufficient because native popup nodes have their own cascade. The fixture covers
semantic button appearances, the explicit Awra menu button, navigation item and
semantic typography in addition to the native controls. Fonts are deliberately
not attacked in this equality fixture because the isolation contract preserves
the desktop family and base scale.

Public style-role names are API vocabulary rather than GTK class names.
`awra_widget_add_style_role(widget, "sidebar-pane")`, for example, installs the
private class `awra-role-sidebar-pane`; it never exposes the generic
`.sidebar-pane` class to the desktop cascade. This distinction is required even
with Awra's higher provider priority: an external theme can define a property
that an Awra recipe did not otherwise need to override. The isolation fixture
therefore includes a real-world hostile sidebar rule with a 9000-pixel shadow
and proves identical active/backdrop pixels and allocations.
