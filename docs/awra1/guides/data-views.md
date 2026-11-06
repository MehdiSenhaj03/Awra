<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Virtualized data views

`AwraDataView` is a visual and state-management layer around
`GtkColumnView`. It deliberately keeps `GListModel`, `GtkSelectionModel`,
`GtkListItemFactory` and `GtkSorter` as public GTK contracts: Awra does not
copy application data or replace virtualization.

```c
GtkSelectionModel *selection = make_selection_model ();
GtkListItemFactory *name_factory = make_name_factory ();
AwraDataColumn *name = awra_data_column_new ("Name", name_factory);
GtkWidget *view = awra_data_view_new (selection);

awra_data_column_set_expand (name, TRUE);
awra_data_column_set_resizable (name, TRUE);
awra_data_view_append_column (AWRA_DATA_VIEW (view), name);
```

A column owns semantic title, factory, sorter, alignment, width, expansion and
resizability. Call `awra_data_column_apply_alignment()` from factory setup for
cells that should follow the declared alignment. The underlying GTK column and
view are exposed read-only for standard signals and advanced GTK operations;
applications must not inspect private children.

The view has Content, Empty, Loading and Error states with replaceable state
widgets. `loading-more` adds non-blocking infinite-loading feedback while
retaining the current rows. Horizontal and vertical scrolling remain GTK
owned.

`AwraSelectionToolbar` observes the same selection model, counts selected
items, optionally hides at zero and clears through `GtkSelectionModel`. Its
actions are ordinary Awra/GTK action widgets. This avoids duplicating selection
bookkeeping in an application.

## Dense data composition

Use `AwraMetricRow` for a name/value fact, `AwraStatBar` for a bounded metric,
`AwraBadgeGroup` for a wrapping set of semantic labels and
`AwraMetadataGroup` for a titled collection of facts. These components own
their spacing, typography and accessible grouping; they do not prescribe an
application data model.

```c
GtkWidget *metadata = awra_metadata_group_new ();
GtkWidget *hp = awra_stat_bar_new ("HP");
GtkWidget *types = awra_badge_group_new ();

awra_metadata_group_append (AWRA_METADATA_GROUP (metadata),
                            "Height", "0.7 m");
awra_stat_bar_set_maximum (AWRA_STAT_BAR (hp), 255.0);
awra_stat_bar_set_value (AWRA_STAT_BAR (hp), 45.0);
awra_badge_group_append (AWRA_BADGE_GROUP (types),
                         AWRA_BADGE (awra_badge_new ("GRASS")));
```

They replace repeated label/grid/progress compositions observed in both the
Showcase data workspace and catalogue detail pages. Domain-specific colors
remain badge configuration, never new framework widget types.

## Virtualized grids (since 1.1)

`AwraGridView` owns a `GtkGridView` inside an overlay-scrolling viewport.
Supply a `GtkSelectionModel` and `GtkListItemFactory` as for the list view;
retain the same selection when switching between a grid and a data table.
GTK owns cell reuse, keyboard navigation, activation and rubberband selection.
Awra owns neutral surfaces and reduced-motion-aware hover/focus/selection.

```python
grid = Awra.GridView.new(selection, factory)
grid.get_delegate().connect("activate", open_item)
grid.get_delegate().set_max_columns(12)
```

`model` and `factory` are nullable, observable GObject properties. Set them
to `None` to detach data without destroying the viewport. Constructors and
setters retain borrowed model/factory references; getters return borrowed
references. Connect ordinary GTK activation signals on the public delegate;
do not inspect the wrapper's private children. Item content, image loading
and domain commands belong to the consuming application.

State views use a preferred text width and wrap within narrow panes. Data
column headers keep padding on cells rather than the enclosing layout row,
so fixed column widths still work when horizontally scrolled.

## Content zoom (since 1.1)

`AwraZoomController` attaches to one content widget. Ctrl+wheel changes its
observable `scale`; ordinary scrolling stays with GTK. Its private, scoped
font scaling preserves the desktop font and leaves chrome outside the target
unchanged. The controller holds its target weakly and detaches its input and
style provider when disposed.

```python
zoom = Awra.ZoomController.new(content_view)
zoom.set_range(0.5, 3.0)
zoom.connect("notify::scale", update_domain_geometry)
zoom.zoom_in()
zoom.reset()
```

Keep one controller per target. `set_range()` clamps the current scale;
`handle_scroll(delta, modifiers)` is the public input adapter used by the GTK
controller. Only Ctrl-modified wheel input is consumed, including at a bound.
Applications resize domain content such as thumbnails or drawing canvases in
`notify::scale`. Keep the existing factories and models when changing size so
GTK retains virtualization, focus and selection.

`canvas-table` is an opt-in `AwraDataView` style role for column headings on the
continuous Canvas: no filled header strip or separator, while column controls
retain hover, keyboard and resize behavior.
