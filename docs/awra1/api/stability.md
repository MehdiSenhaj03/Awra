<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# API stability and ownership

Awra 1.0 provides a stable source and binary compatibility contract. The full
non-documentation GIR surface of 1.0.0 is frozen in
`tests/api/awra-1.0-api.json`, and a test checks that the current API still
contains all of it unchanged; the current surface, additions included, is
`tests/api/awra-api.json`. Exported symbols must match the installed public
headers. Compatible 1.x releases may add API, but cannot remove or reinterpret
existing symbols, properties, enum values, ownership or nullability. API added
after 1.0 is documented with the release that ships it (`Since: 1.1`); see
`versioning.md`.

## Ownership rules

- `new` constructors for GObject descriptors return an owned reference.
- GTK widget constructors follow GTK's floating-reference convention; adding
  the widget to a parent consumes that floating reference.
- Every `get_child`, `get_surface`, `get_material`, `get_context`, `get_model`
  and `get_style_manager` accessor returns a borrowed reference unless its API
  documentation explicitly says otherwise.
- Container setters retain the supplied widget through GTK parenting. Passing
  `NULL` removes it when the parameter is annotated nullable.
- `AwraInspector:target` is weak. It can be cleared explicitly and becomes
  `NULL` when the target is destroyed.
- `AwraContext` is owned by its `GdkDisplay` and is returned borrowed. The
  context, style manager, diagnostics and native coordinator share the GTK main
  thread and are destroyed with that display.
- Applications own their `GMenuModel`, list model and expression references;
  Awra retains them only for as long as documented by the corresponding GTK or
  GObject property.

## Multi-language gate

The generated GIR is the contract. A machine-readable test guards nullability,
array shape and ownership for the foundational APIs. Permanent C, Vala,
Python/GI and experimental Rust consumers compile in the developer build. The
Rust consumer is intentionally early: it catches APIs that are legal C but
would otherwise project as raw pointers or ambiguous ownership.

The shared library exports only declarations found in installed public headers;
private effect coordinator, Wayland, Win32 and resolver symbols are hidden by
the ELF version map, and on Windows by the DLL's module-definition file.

## Deprecation policy

When a stable symbol must evolve, Awra first adds the replacement, documents
the migration, marks the old declaration deprecated, and keeps its behaviour
for at least one complete minor series. Silent semantic changes to Material,
Surface, Window or Overlay are not accepted.
