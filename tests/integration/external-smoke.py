#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Exercise Awra exclusively through the installed-style GI contract."""

import gc
import os
import sys
import tempfile

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
gi.require_version("Awra", "1.0")

from gi.repository import Awra, Gdk, Gio, Gtk  # noqa: E402


def main() -> int:
    Gtk.init()
    Awra.init()
    material = Awra.Material.new_for_preset(Awra.MaterialPreset.CANVAS)
    assert material.get_kind() == Awra.MaterialKind.FROSTED
    assert material.get_preset() == Awra.MaterialPreset.CANVAS

    surface = Awra.Surface.new_with_role(Awra.SurfaceRole.CARD)
    child = Gtk.Label(label="Python/GI consumer")
    surface.set_child(child)
    assert surface.get_child() is child
    surface.set_child(None)
    assert surface.get_child() is None
    source = Gtk.Picture()
    surface.set_backdrop_widget(source)
    assert surface.get_backdrop_widget() is source
    surface.set_backdrop_widget(None)
    header = Awra.Header.new()
    header.set_max_width(1000)
    assert header.get_max_width() == 1000
    assert isinstance(header.get_surface(), Awra.Surface)

    task = Awra.TaskRow.new("Export")
    task.set_subtitle("Preparing")
    task.set_show_progress(True)
    task.set_indeterminate(True)
    task.set_suffix(Awra.Button.new_with_label("Cancel"))
    task.set_indeterminate(False)
    task.set_fraction(0.5)
    assert task.get_fraction() == 0.5
    task.set_suffix(None)
    assert task.get_suffix() is None

    dropdown = Awra.Dropdown.new_from_strings(["One", "Two", "Three"])
    dropdown.set_selected(1)
    assert dropdown.get_selected() == 1
    dropdown.set_model(None)

    popover = Awra.Popover.new()
    popover.set_child(Gtk.Label(label="Floating"))
    popover.set_child(None)
    menu = Awra.MenuButton.new()
    menu.set_label("Actions")
    menu.set_popover(popover)
    assert menu.get_popover() is popover
    menu.set_popover(None)

    target = Gtk.Box()
    Awra.widget_set_context_menu_model(target, None)

    page = Awra.Page.new()
    header = Awra.PageHeader.new()
    header.set_title("Library")
    header.set_subtitle("Typed Python composition")
    section = Awra.Section.new()
    section.set_title("Recent work")
    section.set_child(Gtk.Label(label="No manual section spacing"))
    toolbar = Awra.Toolbar.new()
    toolbar.set_end_widget(Awra.Button.new_with_label("Create"))
    content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
    content.append(header)
    content.append(toolbar)
    content.append(section)
    page.set_child(content)
    assert page.get_child() is content
    page.set_child(None)

    form = Awra.Form.new()
    row = Awra.FormRow.new()
    row.set_label("Email")
    row.set_control(Awra.Entry.new())
    row.set_error_message("Enter a valid address")
    row.set_validation_state(Awra.ValidationState.ERROR)
    form.append(row)
    assert form.get_row(0) is row
    empty = Awra.EmptyState.new()
    empty.set_title("No results")
    empty.set_action(Awra.Button.new_with_label("Reset"))
    loading = Awra.LoadingState.new()
    loading.set_active(False)
    error = Awra.ErrorState.new()
    error.set_action(Awra.Button.new_with_label("Retry"))
    filters = Awra.FilterBar.new()
    filters.set_search(Awra.SearchEntry.new())
    filters.append_filter(Awra.Dropdown.new_from_strings(["All", "Open"]))
    assert filters.get_n_filters() == 1
    detail = Awra.DetailPane.new()
    detail.set_title("Workspace")
    detail_section = Awra.Section.new()
    detail_section.set_title("Overview")
    detail.append_section(detail_section)
    master_detail = Awra.MasterDetail.new()
    master_detail.set_master(filters)
    master_detail.set_detail(detail)
    master_detail.show_detail()
    assert master_detail.get_visible_pane() == Awra.MasterDetailPane.DETAIL
    data_view = Awra.DataView.new(None)
    data_column = Awra.DataColumn.new("Name", None)
    data_column.set_expand(True)
    data_view.append_column(data_column)
    data_view.set_state(Awra.DataViewState.EMPTY)
    selection_toolbar = Awra.SelectionToolbar.new(None)
    selection_toolbar.set_auto_hide(False)
    selection_toolbar.set_title("record")
    selection_toolbar.set_plural_title("records")
    assert data_view.get_n_columns() == 1
    text_area = Awra.TextArea.new()
    text_area.set_text("Notes")
    password = Awra.PasswordEntry.new()
    password.set_text("secret")
    tags = Awra.TagEntry.new()
    assert tags.add("GTK")
    choices = Awra.ChoiceGroup.new()
    choices.set_exclusive(True)
    choices.append("Daily")
    banner = Awra.StatusBanner.new()
    banner.set_appearance(Awra.StatusAppearance.INFO)
    responsive = Awra.ResponsiveBin.new()
    responsive.set_child(Gtk.Label(label="Adaptive"))
    assert responsive.get_child() is not None
    breadcrumb = Awra.BreadcrumbItem.new("Library", "library")
    breadcrumbs = Awra.BreadcrumbBar.new()
    assert breadcrumb.get_name() == "library"
    assert breadcrumbs.get_model() is None
    path_bar = Awra.PathBar.new()
    path_bar.set_text("/library")
    assert path_bar.get_text() == "/library" and not path_bar.get_editing()
    palette = Awra.CommandPalette.new(None)
    palette.set_title("Quick actions")
    palette.set_empty_text("Nothing found")
    assert palette.get_title() == "Quick actions"
    stat = Awra.StatBar.new("HP")
    stat.set_value(45)
    metadata = Awra.MetadataGroup.new()
    metadata.append("Height", "0.7 m")
    skeleton = Awra.Skeleton.new()
    skeleton.set_lines(4)
    keyboard = Awra.KeyboardView.new()
    keyboard.set_physical_layout(Awra.KeyboardPhysicalLayout.ISO)
    keyboard.set_selection_mode(Gtk.SelectionMode.MULTIPLE)
    keyboard.set_selected_codes(["KeyA", "KeyS"])
    assert keyboard.get_selected_codes() == ["KeyA", "KeyS"]
    assert "IntlBackslash" in keyboard.get_codes()
    cap = keyboard.get_key_cap("KeyA")
    cap.set_bound(True)
    cap.set_secondary_label("Thunder")
    # Hardware keycodes are evdev + 8 on Linux, virtual keys on Windows.
    key_a = Awra.keyboard_code_to_hardware_keycode("KeyA")
    control = Awra.keyboard_code_to_hardware_keycode("ControlLeft")
    assert Awra.keyboard_code_from_hardware_keycode(key_a) == "KeyA"
    if sys.platform != "win32":
        assert key_a == 38
        assert Awra.keyboard_code_to_hardware_keycode("Space") == 65
    capture = Awra.ShortcutCapture.new()
    capture.set_shortcut([control, key_a], Gdk.ModifierType.CONTROL_MASK)
    assert list(capture.get_keycodes()) == [control, key_a]
    assert capture.get_codes() == ["ControlLeft", "KeyA"]
    waveform = Awra.WaveformView.new()
    waveform.set_peaks([0.1, 0.5, 0.9])
    assert len(waveform.get_peaks()) == 3
    waveform.set_duration(10)
    waveform.set_marker_position(4)
    row = Awra.MediaRow.new("Rain")
    row.set_duration(90)
    row.set_suffix(Gtk.Label(label="OST"))
    volume = Awra.VolumeControl.new()
    volume.set_muted(True)
    assert volume.get_effective_volume() == 0
    timeline = Awra.Timeline.new()
    timeline.set_duration(120)
    timeline.set_step_increment(5)
    timeline.set_page_increment(30)
    assert timeline.get_step_increment() == 5
    assert timeline.get_page_increment() == 30
    timeline.add_interval(10, 15, "Cue")
    timeline.set_offset(-0.5)
    assert timeline.get_n_intervals() == 1
    assert timeline.get_offset() == -0.5
    drop = Awra.DropZone.new()
    drop.set_accepted_extensions(["mp3"])
    assert drop.accepts_file(Gio.File.new_for_path(os.path.join(tempfile.gettempdir(), "a.MP3")))
    toast = Awra.ToastOverlay.new()
    toast_id = toast.show_with_action("Removed", "Undo", None, 0)
    toast.dismiss(toast_id)
    source = Gdk.Paintable.new_empty(720, 576)
    aspect = Awra.AspectPaintable.new(source, 16 / 9)
    assert abs(aspect.get_intrinsic_aspect_ratio() - 16 / 9) < .001
    assert aspect.get_paintable() is source
    overlay = Awra.OverlayBar.new()
    overlay.set_content(Gtk.Label(label="Canvas"))
    overlay.set_bar(Awra.Button.new_with_label("Controls"))
    overlay.set_inset(Awra.Spacing.NONE)
    timeline.set_preview(Gtk.Label(label="Preview"))
    assert timeline.get_preview() is not None
    assert timeline.get_preview_popover() is not None
    timeline.set_preview(None)
    app = Gtk.Application(application_id="org.awra.GILifetime", flags=Gio.ApplicationFlags.NON_UNIQUE)
    app.register()
    window = Awra.Window.new(app)
    assert not window.get_keep_rounded_corners()
    window.set_keep_rounded_corners(True)
    assert window.props.keep_rounded_corners
    window.props.keep_rounded_corners = False
    assert not window.get_keep_rounded_corners()
    window.set_overlay_chrome(True)
    window.set_overlay_chrome(False)
    for _ in range(10):
        for create in (Awra.Dialog.new, Awra.CommandPalette.new):
            dialog = create(window)
            dialog.destroy()
            gc.collect()
            dialog.get_title()  # A retained binding still owns a valid native object.
            del dialog
        gc.collect()
    palette.destroy()
    window.destroy()
    gc.collect()
    window.get_title()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
