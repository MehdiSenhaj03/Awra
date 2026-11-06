<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Accessibility, keyboard, localization and RTL contract

Awra preserves GTK's behavioural widgets instead of replacing their input
logic. Text editing therefore remains GTK editing: IME, selection, clipboard,
undo, password semantics and platform accessibility bridges are not
reimplemented by the framework.

## Public contract

- Interactive controls expose the role of the action they perform. In
  particular, `AwraNavigationItem` is a button (not a tab) because it is used
  for destinations, while `AwraTabView` owns tab semantics.
- Forms publish their label, description, required and invalid state.
  `AwraStatBar` exposes a meter with minimum, maximum, current and textual
  values. Dense metadata remains readable when visual layout disappears.
- Focus uses GTK focus navigation. Overlays and navigation history remember a
  weak reference to the previous focused widget and restore it only when the
  widget is still valid and sensitive.
- Public edge and corner concepts are logical START/END. Components must react
  through `GtkWidgetClass.direction_changed`; they may not cache physical
  left/right placement.
- Applications own their translated strings. Labels, descriptions,
  placeholders, empty-state messages and accessible names exposed by Awra are
  settable public properties. Awra never truncates the source string merely to
  preserve a preferred allocation.
- Framework-owned fallback labels use the `awra-1` gettext domain. Replaceable
  headings and empty messages, plus singular/plural selection nouns, remain
  public properties; localization never requires access to a private GTK
  child.
- Fonts and scale are inherited system preferences. Awra owns spacing and
  minimum hit targets, but layouts must reflow for enlarged fonts rather than
  overlap or clip essential content.

## Automated audit

`accessibility-rtl` maps a realistic page containing navigation, a labelled
form input, a meter, an action and an edge panel. It checks public accessible
roles, actual focus transfer into GTK's editable implementation, forward Tab
navigation, long translated text, bounded page allocation and physical
mirroring of logical START after switching the live widget tree to RTL.

The visual suite separately covers normal/high-contrast, Light/Dark, disabled
and focus states. Widget tests cover Escape dismissal and focus restoration
for transient surfaces.

`tests/manual/atspi-smoke.py build/showcase/awra-showcase awra-showcase`
starts the real application with the AT-SPI bridge enabled and inspects it out
of process, as a screen reader does. The 1.0 gate exposes the window frame,
buttons, labels and 44 named nodes to Orca 50.2; this complements the
in-process GTK assertions without pretending to replace a human speech-output
review.

## Manual Orca release checklist

Before a stable release, run Showcase and at least one external application
with Orca and verify:

1. Window, page and section landmarks are announced without duplicated names.
2. Sidebar destinations announce name, role and selected/current state.
3. Every form control is reached once by Tab and its label, help and validation
   message are announced in that order.
4. Data rows and column headings remain meaningful without relying on colour.
5. Opening a menu, dialog or command palette moves focus inside it; Escape and
   activation restore focus to the invoker.
6. Loading, warning and error feedback is announced without requiring motion.
7. The same paths work with an RTL locale and 200% text scaling.

Automated role checks are regression gates; they do not replace this assistive
technology pass because GTK does not expose the complete platform bridge as a
stable in-process assertion API.
