<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Awra visual language

Awra's primary artistic reference is Dark. A native window is one continuous
frosted canvas; child regions establish hierarchy through local tint, spacing,
selected states and semantic depth rather than a bordered rectangle around
every group. Light preserves the same structure and accessibility contract but
remains open to a dedicated artistic calibration.

The current Dark calibration uses a neutral `#232323` Canvas tint at 60%
opacity and 20% of the Frosted grain reference. These remain visual calibration
points rather than frozen ABI constants.

## Surface grammar

`CANVAS` owns the native blur and outer edge. `CONTENT` paints nothing and is
the default reading plane. `CHROME` adds a light structural tint and only the
edge separating a sidebar or toolbar from content. `LAYER` is a quiet,
borderless grouping. `FLOATING` owns a complete outline and shadow; its Dark
calibration is `#252525` at 60% alpha, radius 17, grain scale 20%, with native
blur requested when available. `OPAQUE` is both an explicit recipe and the
accessibility fallback.
Chrome keeps a semantic corner radius instead of becoming a square sheet. A
docked sidebar may therefore meet the Canvas cleanly while retaining softened
inner corners; applications can recalibrate that radius globally without CSS.

The preset alpha is interpreted in compositing context. Chrome and Layer are
local overlays with much lower alpha than a native Canvas or Floating surface;
this prevents nested glass from becoming a stack of nearly opaque boxes. Grain
is restricted to Frosted surfaces and kept below the threshold where it reads
as decoration. The former full-surface white wash is replaced by a directional
rim: light describes the material edge instead of bleaching its content.

The optical primitives beneath components are `well`, `cap`, `cap-hover`,
`thumb`, `track` and `rim`. Wells recede, caps sit on the reading plane, and
thumbs stay pale and physical. These are public token values, not GTK theme
colors, and are shared by every control family.

## Components and states

Normal navigation rows and Ghost actions are transparent. Hover introduces a
neutral cap; selected navigation remains neutral and uses accent only on its
small icon. Pressed states deepen the local material rather than flooding the
control with accent. Keyboard focus is always a separate high-contrast ring.
Disabled content uses dedicated colors instead of reducing the opacity of an
entire widget subtree.

Buttons expose Primary, Secondary, Ghost, Destructive and Toolbar appearances.
Cards expose Plain, Tinted and Raised appearances. A card is appropriate only
when the grouped object has meaning; ordinary page sections use typography and
space directly on Content. Lists remain flat and highlight whole rows.

Typography is selected through `awra_widget_set_typography()` rather than
application CSS. Body, Reading, Muted, Eyebrow, three Title levels, Display and
Monospace form the semantic hierarchy while retaining the desktop font and scale. Reading is for text read at length:
a step larger, with open leading, so long text holds on glass.

Spacing follows a 4/8/12/16/24/32 scale. Current radii are small, medium, large,
window and pill tokens. These numerical values are calibration points, not a
promise that future Awra releases will preserve their exact pixels.

Common composition uses `AwraLayoutPreset` rather than copied pixel values.
Page, Section, Content, Toolbar, Overlay, Control Group and Compact recipes
combine an inset and a gap from the same scale. `awra_box_apply_layout_preset()`
and `awra_grid_apply_layout_preset()` keep ordinary GTK containers available
while giving new views an Awra rhythm by default. Granular token helpers remain
public for layouts whose intention does not match a preset.

`AwraCard` owns its content inset (`LG` by default), so card children should not
carry framework padding themselves. `AwraSidebar` starts with an `MD` outer
inset and rounded silhouette; applications can explicitly choose `NONE` for a
flush docked rail. These defaults make the first composition coherent while
remaining overrideable without CSS.

## Accent and chrome

Accent is a signal, not the palette. `DEFAULT` resolves to Awra's violet,
`CUSTOM` accepts any application color, and `SYSTEM` reads the desktop portal
or an equivalent GTK setting when available. SYSTEM is explicitly best-effort
and resolves to DEFAULT when no system color exists. Semantic `accent-content`
keeps small selected labels and icons legible in both profiles.

Window chrome is composable. `AwraWindow` accepts any titlebar widget and can
restore its default chrome. `AwraHeader` can independently expose start,
center and end content, optional GTK window controls and an optional public
`GtkWindowHandle` drag region. A header may therefore sit above the whole
window, align with content, or participate in an application-specific sidebar
pattern; Awra does not prescribe one window skeleton. With
`blend-with-window`, the window passes its exact root material to the header.
The header owns only the two top corners and outer top/side edges, while the
content canvas owns only the bottom corners and remaining outer edges. There
is consequently no second tint, separator, or rounded inner canvas beneath the
titlebar.

Canvas tint, its alpha and the Frosted grain scale can be overridden through
`AwraStyleManager` for live calibration and then reset atomically. Disabling
native blur resolves Canvas and Floating to opaque versions of their own tint,
so a capability or diagnostic change does not silently replace their color
identity with an unrelated background token. The compositor remains the sole
owner of blur radius and diffusion.

The same calibration contract now applies to every material profile. The
style manager rebuilds one immutable token snapshot after an edit, so existing
surfaces update together and newly created surfaces inherit the exact same
recipe. This avoids local Showcase-only overrides and makes the material lab a
faithful developer tool.

`AwraSurface:corner-mask` and `edge-mask` make this composition public and
reusable. Several adjacent surfaces can describe one external silhouette
without faking continuity through application CSS.

## Accessibility and window state

The framework retains GTK's system font, DPI, scaling, IME, input behavior and
accessible roles. High contrast strengthens necessary edges. Reduced
transparency resolves glass to opaque surfaces without changing layout.
Reduced motion resolves every timing preset and framework-owned state
transition to zero. Normal state transitions are short (Fast) and alter only
paint properties; they do not shift layout.

Inactive windows keep the complete resolved material bit-identical: fill,
grain, outline, highlight, shadow and native blur request. Interactive controls
may still communicate their own hover, press, selection and focus states; the
native window's activation never rewrites the material recipe. The public
material-resolution diagnostic exposes both states for proof.

## Composition patterns

Docked sidebars remain structural Chrome, while inspectors and tool palettes
use Floating material above one uninterrupted Content plane. `AwraEdgePanel`
supports both persistent floating tools and discoverable edge-reveal behavior;
it does not force either pattern into the window skeleton. `AwraActionRow`
provides a flat, full-row target for settings, queues and detail lists. Its
normal state is transparent, keeping visible surfaces for meaningful groups
rather than every item.

`AwraSidebar:inset` is the intermediate composition: the rail remains Chrome
and structural, but breathing room reveals its complete rounded silhouette and
subtle elevation. The semantic default is `MD`; applications can set
`AWRA_SPACING_NONE` through `awra_sidebar_set_inset_spacing()` or use the
low-level pixel property when the rail must dock flush to an edge.

Menus use the same Floating hierarchy with compact icon/label rows, section
separators only where they clarify grouping, and submenus for secondary
actions. `AwraMenuButton` uses explicit `GtkPopoverBin` anchoring rather than a
private `GtkMenuButton` popup lifecycle. Trigger state and nested dismissal are
therefore deterministic: a second activation closes, closing a parent closes
its descendants, and an action closes the complete menu chain. The public
context-menu helper ensures these rules apply equally to cards, list rows and
arbitrary application content.
