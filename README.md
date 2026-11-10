<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Awra 2

Awra is a frosted-glass desktop UI framework: semantic materials (Canvas,
Chrome, Layer, Floating…), the compositor's blur behind translucent windows,
a token set (spacing, radii, typography, motion) and the components built on
them. Awra 2 keeps Awra 1's look one to one and drops GTK: it is a small C
library on [BUI](bui/ARCHITECTURE.md), Awra's fork of Blender's interface
runtime.

- **No GTK, GLib, Pango, Cairo or GObject.** Widgets are nodes of BUI's
  retained kit; Awra lays them out with GTK's own size negotiation rules
  (height-for-width, expand propagation, alignment), draws them with the
  kit's damage-driven software raster (premultiplied ARGB, SDF rounded
  boxes, analytic shadows, FreeType text) straight into the compositor's
  shared-memory buffers, and only repaints what changed.
- **Native windows**: Wayland (xdg-shell, fractional scale, cursor-shape,
  `ext-background-effect-v1` / KDE blur), X11 (ARGB visual, KDE blur hint)
  and Win32 (DWM Acrylic on Windows 11 22H2+, accent blur on Windows 10,
  native rounding, shadow, snap and resize band), all client-side decorated
  with Awra's header.
- **Rust**: `bindings/rust/awra-sys` (generated FFI, builds the C library with
  CMake) and `bindings/rust/awra` (a gtk-rs-like safe API: typed widgets,
  closures on signals, `win.*` actions with accelerators, menus, timers and
  futures on the UI thread, native file dialogs).

## Awra Studio

[`studio/`](studio/README.md) is a music studio built on Awra (Rust): a
multitrack timeline, built-in effects and real-time autotune, VST3/CLAP
plugins, an agentic AI sound engineer, serverless peer-to-peer sessions,
and a workspace the user (or the AI) rearranges. It also drives Awra's
evolution: `awra::Dock`, `awra::LevelMeter` and canvas text came from it.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/awra-showcase
```

Linux needs `wayland-client`, `wayland-cursor` and `xkbcommon` (X11 and
fontconfig are loaded at run time when present). Windows builds with MinGW
(`-DCMAKE_TOOLCHAIN_FILE=<mingw toolchain>`) or MSVC; the MinGW runtime is
linked in.

`awra-shot <page> <png>` renders a showcase page offscreen (no display) and
prints the frame time; `AWRA_APPEARANCE=light|dark`,
`AWRA_REDUCED_MOTION=1` and `AWRA_FONT="Family 10"` override the desktop's
settings for reviews.

From Rust:

```toml
[dependencies]
awra = { path = "../Awra/bindings/rust/awra" }
```

```rust
let app = awra::Application::new("org.example.App");
let window = awra::Window::new(&app);
let button = awra::Button::with_label("Hello");
button.connect_clicked(|_| println!("clicked"));
window.set_child(Some(&button));
window.present();
app.run();
```

## Layout

| Path | What |
| --- | --- |
| `include/awra/awra.h` | the whole public C API |
| `src/` | Awra: core (tree, signals, layout), style and tokens, text, paint and materials, icons, windows, containers, components |
| `bui/` | the BUI fork: `core`, `gpu` (raster, text), `kit` (nodes, input, window hosts) |
| `bindings/rust/` | `awra-sys`, `awra` |
| `showcase/`, `tools/` | the showcase, `awra-shot`, generators (icons, Rust FFI) |
| `docs/architecture.md` | how Awra 2 works |
| `docs/icons.md` | icon themes and content types, as GTK and GIO have them |
| `docs/awra1/` | Awra 1's documentation (the visual language reference) |

## License

Awra 2 is GPL-3.0-or-later (`LICENSES/GPL-3.0-or-later.txt`): it is built on
BUI, which is derived from Blender and licensed GPL-2.0-or-later
(`bui/LICENSE`, `bui/THIRD_PARTY_NOTICES.md`). Applications that link Awra 2
are covered by the GPL when distributed. Awra 1 was LGPL-2.1-or-later. The built-in icons (`src/icons-data.c`) are
GTK's resource icons (LGPL-2.1-or-later) and Adwaita's symbolic icons
(CC-BY-SA-3.0 / LGPL-3.0-or-later).
