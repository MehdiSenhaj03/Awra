<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# ADR-0001: C/GObject and GIR are the primary ABI

Status: accepted.

Awra exposes a versioned C/GObject ABI and treats `Awra-1.0.gir` as the
language-neutral contract. This keeps GTK inheritance, GtkBuilder and
accessibility native, and lets C, Vala, Python/GI and generated Rust share one
ownership model. Language bindings may add idiomatic safety, but may not hide
useful GTK types or invent behaviour absent from the C API.
