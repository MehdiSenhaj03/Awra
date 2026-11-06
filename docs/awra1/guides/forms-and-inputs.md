<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Forms, settings and advanced input

`AwraForm` and `AwraFormRow` are the default accessible label/control/help/
validation composition. A row switches to vertical layout when its allocation
cannot safely hold both columns; applications do not need to repeat breakpoint
logic.

Use `AwraTextArea` for multiline GTK text editing, `AwraPasswordEntry` for
secure input, `AwraTagEntry` for editable chips and `AwraChoiceGroup` for
checkbox or exclusive radio-like choices. They retain GTK IME, clipboard,
selection, reveal and keyboard behavior while Awra owns visible states.

`AwraActionRow` is the looser settings/list pattern. It accepts an icon and a
suffix control without imposing a libadwaita-style page hierarchy.
`AwraStatusBanner` provides non-modal information, success, warning and error
feedback. Empty, Loading and Error views cover whole-region state.

Awra intentionally does not yet wrap date, time, color or file pickers. The
roadmap requires two demonstrated generic scenarios before adding a wrapper;
ordinary GTK dialogs remain supported meanwhile.

## Media and keyboard input

`AwraKeyCap` presents one physical key. Free keys recede as wells and bound
keys sit on the plane as caps; `tint` carries a domain color (Awra picks a
light or dark legend for contrast), `edge-color` a thin category edge and
`badge` a tiny count. `selected`, `active` (a gentle accent pulse) and
`dimmed` are paint-only states, and `awra_key_cap_flash()` replays a press.
Every animation collapses to a static state under reduced motion.

`AwraKeyboardView` composes ANSI or ISO keyboards with optional function row,
navigation cluster and numpad. Keys are identified by `KeyboardEvent.code`
names (`KeyA`, `Digit1`, `Numpad4`, `Space`); legends follow the active
keyboard layout through GDK and can be overridden per key. Selection uses
`GtkSelectionMode`: Ctrl toggles, Shift extends in reading order, arrows move
a roving focus row by row and Tab treats the keyboard as a single stop. The
geometry is physical and is deliberately never mirrored in RTL. With
`accepts-file-drops`, `files-dropped` reports the key and a `GdkFileList`.
`awra_keyboard_code_from_hardware_keycode()` and its inverse translate
between codes and GDK hardware keycodes (Linux evdev + 8).

`AwraShortcutCapture` records a combination while focused: it collects every
held key, commits on release, allows chords with `allow-chords`, cancels on
Escape, a second click or focus loss, and never lets captured keys propagate.
Applications read keycodes, keyvals, codes and modifiers, and may present
their own vocabulary with `set_display_labels()`.

`AwraWaveformView` draws normalized peaks with an accent played region, a
draggable start marker, a dashed hint and a playback cursor. It is a slider
for assistive technologies; arrows move the marker by 1 % (Shift: 10 %), and
clicks request a seek when the marker is not editable or Ctrl/Shift is held.
Streamed analyses set `expected-peaks` so received bars keep their place.

`AwraMediaRow` is the transport row for lists and docks, `AwraVolumeControl`
the compact level control it embeds, and `AwraDropZone` makes any region
accept filtered files with a Floating hover hint or, without a child, a
dashed empty-state well with an optional browse action. Awra emits
`browse-requested` instead of owning a file chooser.
