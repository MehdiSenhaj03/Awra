//! What a file manager needs beyond the common components (ExplorerAwra):
//! a side panel that meets the content, ...
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi;
use crate::object::IsWidget;

macro_rules! ptr {
    ($w:expr) => {
        $w.widget_ptr()
    };
}

impl crate::Sidebar {
    /// The inset at the end edge only (logical px; -1: the inset), as Awra 1's
    /// `get_surface().set_margin_end()`: 0 lets the panel meet what follows.
    pub fn set_end_inset(&self, px: i32) {
        unsafe { ffi::awra_sidebar_set_end_inset(ptr!(self), px) }
    }
}

impl crate::CollectionView {
    /// List: a blank below each row (logical px; Awra 1's spaced-rows role),
    /// part of no item: a press there starts a rubberband.
    pub fn set_row_spacing(&self, spacing: i32) {
        unsafe { ffi::awra_collection_view_set_row_spacing(ptr!(self), spacing) }
    }
}

impl crate::Label {
    /// Blank pixels above each paragraph's first line and above its wrapped
    /// lines (logical; GtkTextView's pixels-above-lines, pixels-inside-wrap).
    pub fn set_line_spacing(&self, above_paragraph: i32, inside_wrap: i32) {
        unsafe { ffi::awra_label_set_line_spacing(ptr!(self), above_paragraph, inside_wrap) }
    }
}

impl crate::DropZone {
    /// Shows the zone as files over it would (an application's own drags).
    pub fn set_hover(&self, hover: bool) {
        unsafe { ffi::awra_drop_zone_set_hover(ptr!(self), hover) }
    }
}
