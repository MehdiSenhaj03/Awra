<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Versioning, ABI and deprecation policy

Awra uses SemVer for its public C/GObject API and coordinates the GIR namespace
and Rust crate release with the C library release.

- The stable API namespace is `Awra-1.0`; its C include and pkg-config names
  remain `awra-1`.
- The shared-library ABI major is represented by the SONAME. A compatible
  minor release may add symbols, enum values where documented as extensible,
  properties and interfaces, but cannot remove or reinterpret existing ones.
- Source-breaking or ABI-breaking changes require the next major line and a
  parallel installable namespace when practical.
- A deprecated API remains available for at least one complete minor series.
  Its documentation names the replacement and the release that introduced the
  deprecation.
- Numeric visual calibration values are not ABI. Semantic meaning, ownership,
  focus behavior and accessibility are API contracts.
- Rust bindings use the matching Awra version and link the same C ABI. The GIR
  remains their generated source of truth; handwritten Rust is restricted to
  idiomatic projections that GIR cannot express.

Every release updates `CHANGELOG.md`, runs the public-symbol contract, clean
install consumers, sanitizers, visual matrix and a clean `meson dist`, and is
tagged `vMAJOR.MINOR.MICRO` on the release commit (`v0.1.0` and `v1.0.0` mark
the earlier ones).

## How a release is recorded

- `meson.build` carries the version, and `awra/awra-version.h` must state the
  same one: configuring fails otherwise. Between releases, the tree carries
  the version of the next one (1.1.0 after 1.0.0, while unreleased).
- API added after a release is documented `Since: MAJOR.MINOR` of the release
  that will ship it; the GIR records it as the element's `version`, and the
  Rust crate can gate it behind a matching feature.
- `AWRA_CHECK_VERSION (major, minor, micro)` tells code compiled against the
  headers whether that API is there.
- `tests/api/awra-api.json` is the current API, exactly; the `api-baseline`
  test lists any difference with the GIR. When an addition is reviewed, it is
  updated with `test-api-baseline.py GIR tests/api/awra-api.json --update`.
- `tests/api/awra-<release>-api.json` freezes a release's API for good; the
  `api-<release>-compatible` test checks that the current API still contains
  all of it unchanged (only additions, and new attributes such as a version).
- Exports: on ELF, the version script generated from the headers keeps every
  public symbol in its single `AWRA_0_1` node (renaming it would break
  binaries already linked); on Windows, a module-definition file generated
  from the same headers limits the DLL to the public API.
