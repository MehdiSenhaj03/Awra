<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Awra

Awra is a GTK 4.22+ desktop UI framework with semantic materials,
GSK-rendered surfaces, accessible GTK-derived controls, and progressive native
background effects. It does not depend on libadwaita.

```sh
meson setup build -Ddeveloper=true
meson compile -C build
meson test -C build
meson install -C build --destdir "$PWD/staging"
```

### Windows

Awra builds for Windows with MinGW (cross-compiled from Linux or with MSYS2)
against GTK 4.22. Windows has no compositor protocol to negotiate: the
`win32-dwm` backend asks DWM for the Acrylic backdrop on Windows 11 22H2 and
later, or the accent-policy blur on Windows 10, and falls back to the opaque
look when "Transparency effects" is off. Dark mode, accent colour, high
contrast and animation preferences come from Windows settings.

```sh
PKG_CONFIG_LIBDIR=<gtk-prefix>/lib/pkgconfig:<mingw-sysroot>/lib/pkgconfig \
  meson setup build-win --cross-file build-aux/windows/mingw64.meson \
  -Dwayland-effects=disabled -Dintrospection=disabled -Dshowcase=false \
  -Dtests=false -Dexamples=false
```

Run `build/showcase/awra-showcase` to browse eleven real routed pages. Alongside
the component gallery, the dedicated **Application Patterns** page demonstrates
a media workspace, launcher board and design canvas with permanent, floating
and edge-revealed panels. The complete demo also covers master/detail, tabs,
contextual actions, push/pop navigation, search, native and in-window overlays,
fallback, motion preferences, and deterministic material diagnostics.

New views can use public `AwraLayoutPreset` recipes on ordinary `GtkBox` and
`GtkGrid` containers. Page, section, content, toolbar and overlay composition
therefore inherit Awra's tokenized spacing without local CSS or copied pixels;
`AwraCard` and `AwraSidebar` also provide semantic content/outer insets by
default.

The **Platform & Blur** page includes a live material lab for Canvas tint,
opacity, grain strength and native-blur activation. Every control uses public
Awra APIs and updates the complete window immediately. Blur strength itself is
not exposed by `ext-background-effect-v1` and remains compositor-controlled.
Every card on **Surfaces & Materials** also exposes a profile editor for its
global tint, opacity, radius, grain, outline, highlight and blur intent. Editing
Floating, for example, immediately updates every Floating popover, dialog and
panel in the application.

For deterministic visual review, `AWRA_SHOWCASE_APPEARANCE=dark|light` selects
an explicit profile, `AWRA_SHOWCASE_ACCENT=system|default|blue|#rrggbb` selects
an accent through the public style manager, and
`AWRA_SHOWCASE_PAGE=overview|patterns|surfaces|controls|inputs|navigation|overlays|motion|layout|platform|diagnostics`
opens a routed page. These options only drive public Awra APIs.

The install includes `awra-1.pc`, `Awra-1.0.gir`, `Awra-1.0.typelib`, and,
when `vapigen` is available, `awra-1.vapi`.

The `Awra-1.0` API and `libawra-1.so.1` ABI are stable. Architecture and limitations are
documented in [`docs/architecture/foundations.md`](docs/architecture/foundations.md),
the visual language in [`docs/architecture/visual-language.md`](docs/architecture/visual-language.md),
the API families in [`docs/api/README.md`](docs/api/README.md), and validation
in [`docs/architecture/validation.md`](docs/architecture/validation.md). The
cross-cutting improvement plan derived from Showcase and reference applications is maintained
in [`docs/architecture/improvement-roadmap.md`](docs/architecture/improvement-roadmap.md),
with gate evidence in
[`docs/architecture/roadmap-status.md`](docs/architecture/roadmap-status.md).
The evolving observations from Showcase and reference applications are
kept in the [`usage journal`](docs/usage-journal/journal.md).

Blueprint validation is optional (`-Dblueprint=auto`) and is performed when
`blueprint-compiler` is installed. It is a build tool only; Awra has no
Blueprint or libadwaita runtime dependency.
