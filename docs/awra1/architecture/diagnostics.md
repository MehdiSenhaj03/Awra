<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Diagnostics and Inspector

`AwraDiagnostics` is a read-only, compositor-neutral view of the effect
coordinator. It exposes session/backend names, announced capability, native
surface counts, applied effects, cumulative region submissions, resolved
geometry and fallback reason. `awra_diagnostics_dup_report()` returns the same
information as plain text without leaking `wl_surface`, protocol objects or a
KWin-specific type.

`AwraInspector` weakly targets any `GtkWidget`. Its report includes public and
resolved class information, allocation, scale, direction, accessible role,
native surface type, enclosing Awra surface role/material/elevation, token
preferences and side-by-side active/inactive fill and blur resolution. The
report can be copied or exported with `awra_inspector_export_report()`.

Two developer visualizations are opt-in:

- effect-region outlines show the rounded area Awra resolves for the target;
- overdraw tint marks the inspected semantic surface.

Neither mode changes the backend request. GTK does not expose an enumerable
list of all effective style providers, so Inspector does not invent one. Theme
isolation is instead proven by construction and the hostile-provider pixel and
allocation matrix.
