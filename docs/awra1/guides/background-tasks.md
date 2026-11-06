<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Background tasks

`AwraTaskRow` presents background work without owning a scheduler, subprocess,
network client or domain state. It was introduced while building VodRecoveryAwra
and also fits exports, synchronization, indexing and imports.

```python
row = Awra.TaskRow.new("Exporting an archive")
row.set_subtitle("Original quality · 48 MB")
row.set_icon_name("document-save-symbolic")
row.set_show_progress(True)
row.set_fraction(0.42)
row.set_suffix(Awra.Button.new_with_label("Cancel"))
```

Use `indeterminate=True` while duration is unknown. The icon becomes a spinner;
with reduced motion enabled it becomes a static busy icon. Known progress uses
the existing Awra progress presentation. Hiding `show-progress` retains the
identity and actions for completed, cancelled or failed work. The application
owns the status vocabulary and action behavior.

Titles and subtitles are plain text, ellipsized inside a horizontally expanding
column. The title names the accessible group and progress control. Tokenized
spacing, GTK directionality and a nullable suffix preserve compact layouts and
RTL. Replacing the suffix releases the previous widget through GTK ownership.
Properties are available in GtkBuilder, C, Python, Vala and Rust (`v1_1`).

Always marshal worker updates onto the GUI main context. For continuous
animations or slow software rendering, deliver completion with
`GLib.idle_add(callback, priority=GLib.PRIORITY_DEFAULT)` so a default-idle
callback cannot starve behind ready frame sources. Network operations and
process waits belong outside that callback.
