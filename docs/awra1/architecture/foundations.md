<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Awra foundation architecture

Awra 1.0 is a stable C17/GObject library. Its public ABI contains no
Wayland or compositor types. Applications select a semantic material; Awra
resolves it against the current token snapshot and effect capabilities.

The native path is:

```text
AwraWindow / AwraPopover
        -> GtkNative / GdkSurface
        -> private effect coordinator
        -> ext-background-effect-v1 or generic fallback
```

The Wayland backend is compiled from the pinned protocol XML in `protocols/`.
Registry discovery uses a short-lived private Wayland event queue and synchronous
roundtrips on GTK's main thread. It never starts another file-descriptor reader.
The bound manager is returned to the display's normal queue for capability
updates. Every effect region is surface-local and rounded corners are
approximated with integer scanline rectangles. GTK 4.22's public
`gdk_wayland_surface_force_next_commit()` schedules application of the
double-buffered region.

## Material invariants

- Low-level Solid, Translucent and Frosted primitives remain public.
- Semantic Canvas, Content, Chrome, Layer, Floating and Opaque presets resolve
  role-aware tint, alpha, radius and edge policy.
- Solid is always opaque.
- Translucent never requests compositor blur.
- Frosted requests blur only when the backend advertises it.
- Frosted becomes opaque when blur is unavailable or reduced transparency is
  enabled.
- Window inactivity never changes any resolved material field: fill tint,
  opacity, grain, border, highlight, shadow, geometry and blur request remain
  identical. Emphasis belongs to explicit component interaction states.

Nested surfaces do not request independent backdrop effects when they share a
native surface. The root native surface owns the blur; nested Chrome and Layer
presets provide compositing-aware local tint rather than restacking the root
opacity. Popovers implement `GtkNative`, so they register a
separate native effect for each mapping.

## Responsive and interaction layers

`AwraSplitView` derives compact, medium and expanded modes from its allocation;
collapsing the sidebar never mutates the root material or its blur request.
Controls reuse GTK behavior through derivation or composition. Menus are built
from `GMenuModel`/`GAction`, dropdowns and lists retain GTK model semantics,
and navigation maintains explicit push/pop/replace history. `AwraMenuButton`
and `AwraDropdown` both use public `GtkPopoverBin` anchoring, so repeated
activation and nested dismissal do not depend on a private GTK menu-button
popup. `AwraDropdown`
does not render `GtkDropDown`'s theme-owned private popover: a public
`GtkPopoverBin` anchors `AwraPopover`, whose surface always resolves the
global Floating recipe and participates in the native effect lifecycle.

Motion durations are semantic presets. Reduced motion resolves every preset to
zero, and navigation, tabs and toast revealers retarget their GTK transitions
when that preference changes. Toasts remain in-window while popovers and dialogs
continue through the native effect lifecycle.

The developer inspector holds its target weakly. It exposes dimensions,
accessible role, semantic material and surface state, and can draw the exact
rounded region Awra submits for the inspected native root.

Layout density follows the immutable token snapshot. The low-level spacing
scale remains public, while `AwraLayoutPreset` names the recurring page,
section, content, control, toolbar and overlay rhythms. Applying a recipe to a
GTK box or grid sets both its inset and gap from those tokens. Structural Awra
components consume the same contract: cards own their content inset and
sidebars start inset from the canvas unless explicitly docked.

## Known limits

The protocol does not control blur radius, saturation, or compositor algorithm.
The rounded region is quantized to logical integer coordinates, so fractional
scaling still requires visual validation. If protocol discovery, capability
negotiation, surface creation, or region update fails, Awra keeps the widget
functional and uses its opaque material fallback.
