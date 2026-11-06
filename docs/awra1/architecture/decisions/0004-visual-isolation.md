<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# ADR-0004: Visual isolation preserves useful system preferences

Status: accepted.

Awra owns the rendered identity of Awra components even in the presence of
`GTK_THEME`, user `gtk.css` and runtime USER-priority providers. It neutralizes
appearance-sensitive inherited properties with private prefixed selectors and
GSK rendering. It deliberately preserves fonts, scaling, cursor behaviour,
IME, text selection, accessibility, system Light/Dark/high contrast and the
best-effort SYSTEM accent with DEFAULT fallback.
