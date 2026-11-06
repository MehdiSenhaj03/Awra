<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# ADR-0002: Semantic materials above a private native backend

Status: accepted.

Applications choose low-level Solid/Translucent/Frosted materials or semantic
Canvas/Chrome/Content/Layer/Floating/Opaque presets. A private coordinator
translates resolved blur requests to a capability backend. Wayland objects,
compositor names and fictitious blur radii never enter public API. Failure or
reduced transparency resolves to an opaque, chromatically coherent fallback.
