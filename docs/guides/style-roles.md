<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Public style-role catalogue

Style roles let an application keep a GTK behavioral widget while Awra owns
its visual recipe. They are the right tool for data cells, charts and other
domain content; they are not a substitute for an Awra widget that owns
layout, focus, lifecycle or accessibility behavior.

Call `awra_widget_add_style_role(widget, role)`. Awra translates the public
identifier to a private `awra-role-*` CSS class, so an application must never
add the private class itself. Unknown well-formed identifiers are accepted for
forward compatibility but have no visual effect until Awra defines a recipe.

## Current roles

Prefer a typed widget for every reusable composition. Roles remain for domain
visualizations and GTK data rows whose model, activation and selection stay with
the application.

| Public role | Expected GTK widget | Purpose | Prefer a typed widget when… |
| --- | --- | --- | --- |
| `data-window` | `GtkWindow` | Data-application root metrics | always use `AwraWindow` for the real window |
| `data-root` | root container | Dense application content plane | `AwraPage` fits the page |
| `app-header` | `AwraHeader` | Compact data-application chrome; header children remain vertically centered | use on a custom start/center/end header composition |
| `app-search` | `GtkSearchEntry` | Header search field | use `AwraSearchEntry` |
| `sidebar-pane` | sidebar container | Structural chrome plane | `AwraSidebar` owns the panel |
| `navigation-sidebar` | list/container | Sidebar navigation rhythm | use `AwraNavigationItem` for destinations |
| `sidebar-section` | `GtkLabel` | Sidebar eyebrow heading | use public typography for new code |
| `nav-row`, `selected-nav` | row/button | Navigation state | use `AwraNavigationItem:selected` |
| `filter-dropdown` | dropdown | Dense filter sizing | use `AwraDropdown` in `AwraFilterBar` |
| `filter-toggle` | toggle button | Dense filter toggle | use `AwraToggleButton` in `AwraFilterBar` |
| `collection-view` | `GtkGridView` or `GtkListView` | Virtualized collections with a transparent plane, cell spacing and visible focus; GTK retains the model and selection | use when a card or row factory is application-owned |
| `data-list`, `data-row`, `selected-data-row` | list/row | Dense GTK data presentation | keep only when a GTK behavioral row is deliberate; prefer Awra list/data views |
| `category-text` | label | Domain category color | use `AwraBadge:custom-color` when possible |

## Rules

- Keep GTK models, factories, IME and selection behavior intact.
- Use a role only on the documented widget family.
- Combine at most the state role documented for a base role; do not build an
  application stylesheet out of role strings.
- Use `AwraMaterial`/`AwraSurface` for matter and depth, typed Awra widgets for
  reusable behavior, and public spacing/typography helpers for composition.
- Never inspect or modify private GTK children to make a role work.

## Desktop workspaces

The following opt-in recipes keep file, media and developer workspaces compact
without application-owned CSS. They use the same colors, contrast and motion
preferences as the rest of Awra.

| Role | Expected widget | Purpose |
|---|---|---|
| `workspace-canvas` | GtkBox | Quiet content plane and chrome separation |
| `workspace-tabs` | AwraTabView | Tabs embedded in compact window chrome |
| `compact-navigation` | AwraSidebar | 32 px navigation rows for long lists of places; selection stays neutral |
| `sidebar-heading` | GtkLabel, or an AwraButton that folds its section | Muted heading aligned with navigation items |
| `compact-table` | AwraDataView | Compact rows and column headers |
| `canvas-table` | AwraDataView | Transparent column headings on a shared Canvas |
| `pane-caption` / `active-pane-caption` | GtkLabel | Quiet caption and focus indication in a divided workspace |
| `drop-zone` | GtkWidget with GtkDropTarget | Destination highlight while GTK marks an accepted drop active |
| `focused-pane` | AwraToolbar | Subtle focus feedback for independent panes |
| `quiet-search` | AwraSearchEntry above a list outside a sidebar | The sidebar's quiet search: a light line until used |
| `inline-search` | AwraSearchEntry | Quiet filter in a pane footer |
| `location-search` | AwraSearchEntry | Search that takes over a location slot and keeps its proportions |
| `prose-metadata` | AwraMetadataGroup | Metadata read as text rather than compared as figures |
| `active-tabs` | AwraTabView | Marks the focused view when several views each have their tabs |
| `view-switcher` | GtkBox containing AwraToggleButton | Compact, observable view selection |
| `floating-selection` | AwraSelectionToolbar | Selection actions over content without moving it |
| `entry-hover` / `conversation-entry` | AwraConversationView / the entry inside its row | Rows that also carry a date or a heading tint only their entry on hover |
| `row-action` | GtkButton that is a whole row | Flat full-width row with navigation hover and press, theme-proof |
| `search-launcher` | GtkButton that opens search | Quiet pill drawn as the field it opens |
| `chosen-row` | Any row container | A row being acted on, such as a message being replied to |
| `flat-list` | GtkListView or GtkFlowBox whose items draw their own states | No background, padding, hover or selection from the desktop theme |
| `people-list` | A container of AwraNavigationItems listing people or things | Names at full contrast, a 12 px gap after the face, rows aligned on the column; muted items recede |
| `card-action` | GtkButton that is a whole card | A tinted card that lifts on hover, with its own 16 px inset |
| `quiet-destructive` | AwraButton (Ghost or Toolbar) | Quiet at rest, danger under the pointer, for hanging up or leaving among other controls |
| `danger-when-on` | AwraToggleButton | A danger tint while on, such as a muted microphone, short of a destructive action's red |
| `quality-good` / `quality-fair` / `quality-poor` | GtkImage | How well something is going, such as a call's connection, in the status colours; swap the role as it changes |
| `inline-edit` | AwraEntry replacing text in a row | Edit in place at the height of the text |
| `inline-action` | Any GtkButton inside a row or content | Quiet action, transparent at rest; Awra owns its look, so the desktop theme does not draw a button around it |
| `usage-meter` | AwraProgressBar under a row | Thin capacity gauge; `awra-usage-low` turns it red |
| `collection-tile` | GtkBox item factory content, or a GtkButton holding a tile | Compact icon/thumbnail and caption composition; on a button, the tile is the whole target |
| `inspector-pane` | GtkBox | Docked inspection pane, separated from content |
| `document-preview` | AwraTextArea | Readable document surface without input-field decoration |

`document-preview` changes presentation only. Set the public text-view delegate's
`editable` and `cursor-visible` properties for a read-only preview. Use
`AwraHeader:compact` for the matching compact titlebar. Its default remains the
existing spacious header. `AwraSelectionToolbar:clear-label` lets applications
localize or customize the dismiss-selection action alongside their own titles.

Set `AwraHeader:expand-center` when a navigation or address control should fill
the actual space between its start and end controls. The default remains the
centered, natural-width allocation. This opt-in avoids a second navigation bar
and wasted symmetric gutters when the side controls have different widths.
The header keeps its native window controls, drag behavior and Canvas material.

`drop-zone` is visual only: attach a `GtkDropTarget` and accept payloads according
to the application's domain. GTK owns the `:drop(active)` state; the role adds
destination feedback without changing its resting geometry.
