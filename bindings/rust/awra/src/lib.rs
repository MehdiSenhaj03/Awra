// SPDX-License-Identifier: LGPL-2.1-or-later

#![allow(deprecated)]
#![allow(clippy::manual_c_str_literals)]

pub use awra_sys as ffi;
pub use gdk;
pub use gio;
pub use glib;
pub use gtk;

extern crate glib as gobject;

macro_rules! assert_initialized_main_thread {
    () => {
        assert!(
            gtk::is_initialized_main_thread(),
            "GTK must be initialized on the main thread before constructing Awra widgets"
        );
    };
}

macro_rules! skip_assert_initialized {
    () => {};
}

#[allow(clippy::derived_hash_with_manual_eq)]
#[allow(clippy::too_many_arguments)]
#[allow(clippy::type_complexity)]
#[allow(clippy::let_and_return)]
#[allow(unused_imports)]
mod auto;

pub use auto::*;

mod inspector;

pub mod builders {
    pub use crate::auto::builders::*;
}

pub mod prelude {
    pub use crate::auto::traits::*;
    pub use gtk::prelude::*;
}

/// Applies one of Awra's public, semantic style roles to a GTK widget.
///
/// Prefer a dedicated Awra widget whenever one exists. Style roles are the
/// supported escape hatch for domain-specific compositions, not a replacement
/// for framework components.
pub fn add_style_role(widget: &impl glib::object::IsA<gtk::Widget>, role: &str) -> bool {
    use glib::translate::{from_glib, ToGlibPtr};

    let role = std::ffi::CString::new(role).expect("Awra style roles cannot contain NUL bytes");
    unsafe {
        from_glib(ffi::awra_widget_add_style_role(
            widget.as_ref().to_glib_none().0,
            role.as_ptr(),
        ))
    }
}

/// Removes a public Awra style role previously applied with
/// [`add_style_role`].
pub fn remove_style_role(widget: &impl glib::object::IsA<gtk::Widget>, role: &str) -> bool {
    use glib::translate::{from_glib, ToGlibPtr};

    let role = std::ffi::CString::new(role).expect("Awra style roles cannot contain NUL bytes");
    unsafe {
        from_glib(ffi::awra_widget_remove_style_role(
            widget.as_ref().to_glib_none().0,
            role.as_ptr(),
        ))
    }
}

/// Applies Awra's spacing and inset recipe for a semantic layout.
pub fn apply_layout_preset(box_: &gtk::Box, preset: LayoutPreset) {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe {
        ffi::awra_box_apply_layout_preset(box_.to_glib_none().0, preset.into_glib());
    }
}

/// Sets every margin of `widget` to a semantic spacing token.
pub fn set_margin(widget: &impl glib::object::IsA<gtk::Widget>, spacing: Spacing) {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe {
        ffi::awra_widget_set_margin(widget.as_ref().to_glib_none().0, spacing.into_glib());
    }
}

/// Sets the start and end margins of `widget` to a semantic spacing token.
pub fn set_horizontal_margin(widget: &impl glib::object::IsA<gtk::Widget>, spacing: Spacing) {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe {
        ffi::awra_widget_set_horizontal_margin(
            widget.as_ref().to_glib_none().0,
            spacing.into_glib(),
        );
    }
}

/// Sets the top and bottom margins of `widget` to a semantic spacing token.
pub fn set_vertical_margin(widget: &impl glib::object::IsA<gtk::Widget>, spacing: Spacing) {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe {
        ffi::awra_widget_set_vertical_margin(widget.as_ref().to_glib_none().0, spacing.into_glib());
    }
}

/// Sets the gap between a box's children to a semantic spacing token.
pub fn set_spacing(box_: &impl glib::object::IsA<gtk::Box>, spacing: Spacing) {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe {
        ffi::awra_box_set_spacing(box_.as_ref().to_glib_none().0, spacing.into_glib());
    }
}

/// Sets a grid's row and column gaps to semantic spacing tokens.
pub fn set_grid_spacing(grid: &impl glib::object::IsA<gtk::Grid>, rows: Spacing, columns: Spacing) {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe {
        ffi::awra_grid_set_spacing(
            grid.as_ref().to_glib_none().0,
            rows.into_glib(),
            columns.into_glib(),
        );
    }
}

/// Applies an Awra typography token without exposing CSS classes to consumers.
pub fn set_typography(widget: &impl glib::object::IsA<gtk::Widget>, typography: Typography) {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe {
        ffi::awra_widget_set_typography(widget.as_ref().to_glib_none().0, typography.into_glib());
    }
}

/// Resolves a semantic motion preset to milliseconds for `manager`.
///
/// The result is `0` while reduced motion is enabled, so animations driven
/// by application code follow the same accessibility contract as Awra
/// widgets.
pub fn motion_duration(manager: &StyleManager, preset: MotionPreset) -> u32 {
    use glib::translate::{IntoGlib, ToGlibPtr};

    unsafe { ffi::awra_motion_get_duration(manager.to_glib_none().0, preset.into_glib()) }
}

/// Awra's ease-out cubic curve; `progress` is clamped to `0.0..=1.0`.
pub fn motion_ease_out_cubic(progress: f64) -> f64 {
    unsafe { ffi::awra_motion_ease_out_cubic(progress) }
}

/// Awra's ease-in-out cubic curve; `progress` is clamped to `0.0..=1.0`.
pub fn motion_ease_in_out_cubic(progress: f64) -> f64 {
    unsafe { ffi::awra_motion_ease_in_out_cubic(progress) }
}

/// The duration, in milliseconds, of an animation that never ends on its
/// own, such as a timed animation that repeats forever.
#[cfg(feature = "v1_1")]
pub const DURATION_INFINITE: u32 = ffi::AWRA_DURATION_INFINITE;

/// Attaches Awra's secondary-click context menu to any widget, or removes it
/// with `None`.
pub fn set_context_menu_model(
    widget: &impl glib::object::IsA<gtk::Widget>,
    model: Option<&impl glib::object::IsA<gio::MenuModel>>,
) {
    use glib::translate::ToGlibPtr;

    unsafe {
        ffi::awra_widget_set_context_menu_model(
            widget.as_ref().to_glib_none().0,
            model.map(|model| model.as_ref()).to_glib_none().0,
        );
    }
}

/// Returns the context menu model attached with [`set_context_menu_model`].
pub fn context_menu_model(widget: &impl glib::object::IsA<gtk::Widget>) -> Option<gio::MenuModel> {
    use glib::translate::{from_glib_none, ToGlibPtr};

    unsafe {
        from_glib_none(ffi::awra_widget_get_context_menu_model(
            widget.as_ref().to_glib_none().0,
        ))
    }
}

/// Maps a GDK hardware keycode (Linux evdev code + 8) to its
/// `KeyboardEvent.code` identifier, such as `"KeyA"`.
#[cfg(feature = "v1_1")]
#[cfg_attr(docsrs, doc(cfg(feature = "v1_1")))]
pub fn keyboard_code_from_hardware_keycode(keycode: u32) -> Option<&'static str> {
    unsafe {
        let code = ffi::awra_keyboard_code_from_hardware_keycode(keycode);
        if code.is_null() {
            None
        } else {
            std::ffi::CStr::from_ptr(code).to_str().ok()
        }
    }
}

/// Maps a `KeyboardEvent.code` identifier to its GDK hardware keycode, or `0`.
#[cfg(feature = "v1_1")]
#[cfg_attr(docsrs, doc(cfg(feature = "v1_1")))]
pub fn keyboard_code_to_hardware_keycode(code: &str) -> u32 {
    let Ok(code) = std::ffi::CString::new(code) else {
        return 0;
    };
    unsafe { ffi::awra_keyboard_code_to_hardware_keycode(code.as_ptr()) }
}

/// Returns the legend of a physical key in the active keyboard layout of
/// `display`, or a layout-independent legend when `display` is `None`.
#[cfg(feature = "v1_1")]
#[cfg_attr(docsrs, doc(cfg(feature = "v1_1")))]
pub fn keyboard_code_label(display: Option<&gdk::Display>, code: &str) -> Option<glib::GString> {
    use glib::translate::{from_glib_full, ToGlibPtr};

    let code = std::ffi::CString::new(code).ok()?;
    unsafe {
        from_glib_full(ffi::awra_keyboard_code_get_label(
            display.to_glib_none().0,
            code.as_ptr(),
        ))
    }
}

/// Registers Awra's resources and public types.
pub fn init() {
    assert_initialized_main_thread!();
    unsafe { ffi::awra_init() }
}
