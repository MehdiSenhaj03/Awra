<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# ADR-0005: Generated Rust bindings permit narrow projection fixups

Status: accepted.

The pinned official gtk-rs `gir` generator owns the Rust ABI and safe wrappers.
Handwritten code is limited to shapes GIR cannot project idiomatically. A
deterministic post-generation fixup is permitted when the GIR is correct but a
documented generator limitation loses an inherited interface; it must be
minimal and protected by a compile-time consumer assertion. The current case
restores `Gio::ActionMap` on `AwraWindow`.
