#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Reject incomplete committed output from the safe Rust GIR projection."""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test-rust-projection.py PATH/TO/awra")

    root = Path(sys.argv[1])
    generated = root / "src" / "auto"
    forbidden = ("TODO: call ffi:", "Unsupported or ignored")
    failures: list[str] = []

    for path in sorted(generated.glob("*.rs")):
        source = path.read_text(encoding="utf-8")
        if any(marker in source for marker in forbidden):
            failures.append(path.name)

    window = (generated / "window.rs").read_text(encoding="utf-8")
    if "gio::ActionMap" not in window:
        failures.append("window.rs (missing Gio.ActionMap projection)")

    list_view = (generated / "list_view.rs").read_text(encoding="utf-8")
    if "pub fn set_header" not in list_view or "pub fn header" not in list_view:
        failures.append("list_view.rs (missing safe header projection)")

    navigation_item = (generated / "navigation_item.rs").read_text(encoding="utf-8")
    if "pub fn set_suffix" not in navigation_item or "pub fn suffix" not in navigation_item:
        failures.append("navigation_item.rs (missing safe suffix projection)")

    for module, needles in {
        "keyboard_view.rs": ("pub fn connect_key_activated", "pub fn selected_codes"),
        "shortcut_capture.rs": ("pub fn keycodes", "pub fn connect_captured"),
        "waveform_view.rs": ("pub fn set_peaks", "pub fn connect_marker_committed"),
        "drop_zone.rs": ("pub fn connect_files_dropped", "pub fn accepts_file"),
        "task_row.rs": ("pub fn set_fraction", "pub fn set_suffix", "pub fn set_indeterminate"),
        "toast_overlay.rs": ("pub fn show_with_action", "pub fn dismiss"),
    }.items():
        source = (generated / module).read_text(encoding="utf-8")
        if not all(needle in source for needle in needles):
            failures.append(f"{module} (missing typed media/input projection)")

    facade = (root / "src" / "lib.rs").read_text(encoding="utf-8")
    for helper in (
        "add_style_role",
        "apply_layout_preset",
        "set_typography",
        "motion_duration",
        "motion_ease_out_cubic",
        "motion_ease_in_out_cubic",
        "set_context_menu_model",
        "keyboard_code_from_hardware_keycode",
    ):
        if f"pub fn {helper}" not in facade:
            failures.append(f"lib.rs (missing safe {helper} helper)")

    if failures:
        print("incomplete safe Rust projection: " + ", ".join(failures), file=sys.stderr)
        return 1

    print("safe Rust projection: complete; no unsupported generated stubs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
