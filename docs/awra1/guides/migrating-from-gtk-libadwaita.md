<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Migrating a GTK or libadwaita application

Migration is best done outside-in. Replace the native window first, then the
page structure, then recurrent controls. Keep the application's models,
actions, factories and domain widgets.

| Existing composition | Awra equivalent |
| --- | --- |
| `GtkApplicationWindow` + custom transparent CSS | `AwraWindow` |
| titlebar/header bar | `AwraHeader`; its start/center/end slots remain flexible |
| scrolled page + hand-written margins | `AwraPage` |
| title/subtitle/actions box | `AwraPageHeader` |
| heading + arbitrary content | `AwraSection` |
| filter/search/count row | `AwraFilterBar` |
| label/control/help/error grid | `AwraForm` + `AwraFormRow` |
| list/detail split with manual breakpoints | `AwraMasterDetail` + `AwraDetailPane` |
| empty/loading/error labels | typed Awra state views |
| appearance CSS on a GTK data widget | documented Awra style role |
| local translucent card | `AwraSurface` with a semantic material preset |
| compositor-specific blur call | no application call; Awra resolves Frosted |

Do not migrate GTK behavior merely to change paint. `GtkListView`,
`GtkColumnView`, `GtkSelectionModel`, `GAction`, text editing and IME remain
valuable. Conversely, do not keep application CSS for a pattern Awra owns:
that bypasses theme isolation and active/inactive invariants.

The Awra window's Canvas fill and native blur request are independent of
focus. Apps must not listen to `is-active` to change their root opacity. Use
the public appearance, contrast, accent and reduced-transparency preferences;
SYSTEM accent is best-effort and falls back to DEFAULT.

For a staged port, use public style roles only as a bridge. Replace a role by
the corresponding typed widget once it owns reusable structure or behavior.
The permanent Rust projection consumer in Awra guards GIR shapes today; the
generated safe Rust bindings now exercise the same public contract.
