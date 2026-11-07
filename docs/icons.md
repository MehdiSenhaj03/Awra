# Icons

Awra 2 shows the user's icon theme exactly as GTK 4.22 does: an icon name
given to any widget (image, button, menu item, row, sidebar item, entry,
header bar window controls, path bar, tab, chip...) is looked up in the
theme the way `GtkImage` looks it up, and drawn the way GTK draws the file
it found. A symbolic icon takes the colour of the text around it; a
full-colour icon (Papirus' blue folders, its red PDF) shows as it is.

## Which theme

`gtk-icon-theme-name`, read where GTK reads it (`src/icontheme.c`):

1. `/etc/gtk-4.0/settings.ini`, `$XDG_CONFIG_DIRS/gtk-4.0/settings.ini`,
   then `$XDG_CONFIG_HOME/gtk-4.0/settings.ini` (`[Settings]
   gtk-icon-theme-name=Papirus-Dark`); on Windows `<exe>/../etc/gtk-4.0`
   and `%LOCALAPPDATA%/gtk-4.0`;
2. over those, the desktop's own setting: on Wayland the settings
   portal's `org.gnome.desktop.interface icon-theme` (`"gnome"` when the
   portal answers without it, as GTK), on X11 the settings daemon's
   `Net/IconThemeName` (XSETTINGS);
3. else `Adwaita`.

Both desktop settings are followed live: when the theme changes, every
cached answer is dropped, the windows are drawn again and
`awra_icon_theme_connect_changed` handlers run.
`awra_icon_theme_set_theme_name` overrides the desktop's choice.

## How a name is looked up

As `gtk_icon_theme_lookup_by_gicon` with a `GThemedIcon` of the name, at
the size the widget draws (application px) and the window's integer scale:

- the names: the one given, then its symbolic / regular variant
  (`foo` -> `foo-symbolic`, `foo-symbolic` -> `foo`); each preceded by its
  `-ltr` or `-rtl` form (the widget's direction);
- the search path: `$XDG_DATA_HOME/icons`, `~/.icons`,
  `$XDG_DATA_DIRS/icons`, `$XDG_DATA_DIRS/pixmaps`
  (`awra_icon_theme_set_search_path` / `add_search_path`);
- the themes: the theme, its `Inherits` (depth first), then `hicolor`;
  for symbolic names every theme is searched for them before any other
  name, then each theme for every name in turn;
- in a theme: each `Directories`/`ScaledDirectories` entry of
  `index.theme` (Fixed, Scalable with MinSize/MaxSize, Threshold), the
  closest one by GTK's distance (exact first, then larger before smaller,
  then the window scale, then fixed before scalable), PNG before SVG in a
  directory; `icon-theme.cache` where it is not older than the theme
  (read by hash, 4 KiB blocks as needed), else the directory, listed the
  first time a lookup needs it;
- then the unthemed images directly in the search path, GTK's resource
  icons, Awra's (`resources/icons`) and the SVGs the application registered
  (`awra_icon_register_svg`, as `gtk_icon_theme_add_resource_path`);
- then `image-missing`.

GTK's resource icons, its hicolor `index.theme` and Adwaita's symbolic
icons are built in (`src/icons-data.c`, `tools/gen_icons.py`); the
Adwaita set is the `Adwaita` theme only where no Adwaita is installed
(Windows, minimal systems), so a system theme is never shadowed.

Nothing is read before the first lookup; loading Papirus-Dark (134
directories, a 2.8 MB cache) and hicolor takes about 2 ms. Every answer is
cached (2048 at most, least recently used dropped), so drawing never
touches the disk; as GTK, a lookup that misses the cache checks every 5 s
at most whether a theme directory changed.

## How a file is drawn

- symbolic SVGs (`-symbolic.svg`): read as GTK reads them into render
  nodes. Only `<path>`, `<rect>` and `<circle>` draw; `<g>`, `<defs>`,
  `<style>` and metadata are skipped; each shape is filled with its class's
  colour (none: the foreground; `success`, `warning`, `error`;
  `transparent-fill`: not filled), stroked only with a `*-stroke` class
  (width 2, round caps and joins unless set), whatever `fill`/`stroke` it
  says; `fill-opacity`, `opacity`, `visibility`, `fill-rule` apply. A file
  GTK cannot read that way (another element, a `transform`...) is read as
  GtkSvg reads symbolic icons (every shape filled with its class's colour,
  transforms and groups applied). The shapes are rasterized once at the
  device size, each class in its own channel, and tinted when drawn: the
  widget's colour, GTK's success (#33d17a light, #26ab62 dark), warning
  (#f57900) and error (#cc0000);
- other SVGs: drawn at the device size, fitted and centred (CSS classes of
  `<style>` and `currentColor` included: Papirus' `ColorScheme-Text`);
- PNG: decoded and scaled to the device size; `.symbolic.png` tinted as
  symbolic SVGs.

Rasters are cached per file and device size (8 MB at most, least recently
used dropped) and freed with the theme.

## Content types

`awra_content_type_*` (Rust: `awra::gio::content_type_*`) are GIO's fdo
backend over shared-mime-info's `mime.cache`: `guess` (file names by glob
weight, data by magic priority, GIO's way of combining both),
`for_file` (GIO's `standard::content-type`), aliases and parents
(`is_a`), `get_icon` / `get_symbolic_icon` (the names of GIO's
`GThemedIcon`: the `icons` entry, the type with `/` as `-`, the generic
icon), `get_description` in the user's languages.

To show a file's icon as GTK applications do:

```rust
let (ct, _) = awra::gio::content_type_guess(Some(&name), &[]);
// or: awra::gio::content_type_for_file(&path, false)
image.set_from_gicon(&awra::gio::content_type_get_icon(&ct));
```

Folders are `inode/directory` (Papirus: `inode-directory`, then
`folder`); special folders use their names (`folder-documents`,
`folder-download`, `user-home`, `user-desktop`, `user-trash`,
`user-trash-full`, `drive-harddisk`...), which the theme resolves like any
other.

## C API

`awra_icon_theme_get_theme_name`, `set_theme_name`, `get_search_path`,
`set_search_path`, `add_search_path`, `has_icon`, `has_names`,
`connect_changed`, `disconnect`, `lookup_icon`, `lookup_names`;
`AwraIconPaintable` (`new_for_file`, `get_file`, `get_icon_name`,
`is_symbolic`, `get_size`, `get_scale`, `render`, `ref`, `unref`);
`awra_themed_icon_names`; `awra_image_set_from_icon_names`;
`awra_content_type_*`. Rust: `awra::IconTheme`, `awra::IconPaintable`,
`awra::IconLookupFlags`, `awra::gio::ThemedIcon`,
`awra::Image::from_gicon` / `set_from_gicon`, `awra::gio::content_type_*`
(`bindings/rust/awra/src/icontheme.rs`).

## Checked against GTK

`tests/test_icon_theme.c` builds a theme of its own and checks GTK's
choices (each compared with GTK 4.22) and its recolouring pixel for
pixel. During development the lookups of 1464 names (Adwaita, hicolor,
Humanity, Papirus file names, Awra's) were compared with GTK 4.22's for
8 themes x 5 sizes x 3 scales, left to right and right to left, with and
without forced regular/symbolic icons: identical; content types were
compared with GIO 2.88 on every shared-mime-info glob (3420 names), 1800
files' data and 34 fixtures: identical.
