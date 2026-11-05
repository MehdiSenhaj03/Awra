<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Contributing to Awra

Awra is a C17/GObject framework whose public contract is its installed C API
and `Awra-1.0.gir`. Changes must remain usable from C, GtkBuilder, Vala,
Python/GI and the generated Rust bindings.

## Decide where a feature belongs

Use a material or surface role for matter and depth, a typed Awra widget for
reusable behaviour/focus/lifecycle, a public style role for presenting an
existing GTK behavioural widget, and application composition for domain
concepts. Do not add a widget solely to wrap an equivalent GTK type.

Any generic problem found in Showcase or a consumer belongs in Awra. Reference
applications may not add local CSS, use private Awra headers, inspect private
GTK children, call Wayland/KWin directly or depend on libadwaita.

## Definition of done for a component

A stable component includes:

- an introspectable public API with explicit nullability and ownership;
- GtkBuilder construction and a projection in the generated Rust bindings;
- default tokenized layout without application magic numbers;
- normal, hover, pressed, selected/checked, disabled, focus-visible and
  inactive behaviour where applicable;
- keyboard semantics, accessible role/name/state and RTL/long-text handling;
- Light, Dark, high-contrast and hostile-provider isolation evidence;
- reduced-motion/transparency handling where applicable;
- replacement/removal/destruction and lifecycle tests;
- a Showcase scenario and a minimal external-consumer exercise.

An incomplete experiment must remain explicitly experimental and cannot
silently expand the stable ABI.

## Design-system changes

New public tokens must express reusable semantic intent. New material presets
must represent a recurring depth/matter role, not a screenshot-specific
recipe. Numeric alpha, radius, spacing and timing values are calibration, not
ABI; semantic meaning, focus invariants and accessibility are contracts.

Every global visual change is checked in Dark and Light, with default and blue
accent, then as grayscale. Premium appearance must come from hierarchy,
matter, proportion and restraint—not decorative glow, arbitrary gradients,
border accumulation or duplicated translucent layers.

## Required validation

Before proposing a stable API change, run:

```sh
meson setup build -Ddeveloper=true
meson compile -C build
meson test -C build --print-errorlogs
./build-aux/validate-install.sh build
./build-aux/validate-subproject.sh
```

Release qualification additionally runs the sanitizer gate, generic backend,
public API/ABI baseline, generated docs, clean `meson dist`, compositor manual
matrix and the reference consumer smoke suite. Add an ADR under
`docs/architecture/decisions/` for a structural decision.
