<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to Awra

Awra 2 is a C11 library whose public contract is `include/awra/awra.h`,
projected into Rust by `bindings/rust/awra-sys` (generated) and
`bindings/rust/awra` (hand-written, gtk-rs-like names).

## Decide where a feature belongs

Use a material or surface role for matter and depth, an Awra component for
reusable behaviour, focus and lifecycle, a style class for a variant of an
existing component, and application composition for domain concepts.

Any generic problem found in the showcase or an application belongs in
Awra. Applications do not reach into `src/awra-private.h`, BUI's kit or the
window hosts.

## Visual parity

Awra 1's look is the reference (`docs/awra1/architecture/visual-language.md`
and its CSS numbers). A component change keeps it: compare an offscreen
render (`awra-shot`) or an application screenshot with Awra 1 at the same
fonts, light and dark, scale 1 and 2.

## Definition of done for a component

- a public API in `awra.h`, its Rust projection (`tools/gen_rust_sys.py`,
  then the safe wrapper in `bindings/rust/awra`);
- GTK's sizing semantics (minimum / natural, height-for-width, expand);
- normal, hover, pressed, selected/checked, disabled, focus-visible states;
- keyboard semantics;
- Light, Dark and high-contrast;
- only the damaged area repainted on state changes.

## Build and checks

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/awra-shot gallery /tmp/gallery.png   # offscreen render, prints the frame time
(cd bindings/rust/awra && cargo run --release --example smoke)
```

Cross-compile for Windows with MinGW to check the Win32 host:

```sh
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=<mingw toolchain> -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
```
