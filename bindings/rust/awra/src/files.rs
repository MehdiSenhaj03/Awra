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
    /// The rubberband's look: the accent (else the focus colour) at these
    /// alphas, corners of `radius` px; `fade`: it fades out when let go.
    pub fn set_band_style(&self, accent: bool, fill_alpha: f64, border_alpha: f64, radius: i32, fade: bool) {
        unsafe { ffi::awra_collection_view_set_band_style(ptr!(self), accent, fill_alpha, border_alpha, radius, fade) }
    }

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

crate::awra_wrapper!(
    /// Awra 1's AwraTransitionBin: its child changes in place and `play`
    /// shows the change as a short motion; the top `fixed_top` px stay still.
    TransitionBin, ["AwraTransitionBin"]
);

impl TransitionBin {
    pub fn new() -> TransitionBin {
        unsafe { TransitionBin::from_full(ffi::awra_transition_bin_new()) }
    }
    pub fn set_child(&self, child: impl crate::object::IntoOptWidget) {
        unsafe { ffi::awra_transition_bin_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn child(&self) -> Option<crate::Widget> {
        unsafe { crate::Widget::from_none_opt(ffi::awra_transition_bin_child(ptr!(self))) }
    }
    pub fn set_fixed_top(&self, height: i32) {
        unsafe { ffi::awra_transition_bin_set_fixed_top(ptr!(self), height) }
    }
    /// How long the next changes play (Normal by default).
    pub fn set_motion(&self, preset: crate::MotionPreset) {
        unsafe { ffi::awra_transition_bin_set_motion(ptr!(self), preset as i32) }
    }
    /// Call it right after the child's content changed, before the next frame.
    pub fn play(&self, dx: f64, dy: f64) {
        unsafe { ffi::awra_transition_bin_play(ptr!(self), dx, dy) }
    }
}

impl Default for TransitionBin {
    fn default() -> Self {
        Self::new()
    }
}

impl crate::Widget {
    /// gtk_widget_get_opacity.
    pub fn opacity(&self) -> f64 {
        unsafe { ffi::awra_widget_opacity(ptr!(self)) }
    }
}

crate::awra_wrapper!(
    /// Awra 1's AwraDragPreview: what a drag carries, as a pile of up to
    /// three items with a count badge; it fans out when it appears.
    DragPreview, ["AwraDragPreview"]
);

impl DragPreview {
    pub fn new() -> DragPreview {
        unsafe { DragPreview::from_full(ffi::awra_drag_preview_new()) }
    }
    pub fn set_icon_size(&self, size: i32) {
        unsafe { ffi::awra_drag_preview_set_icon_size(ptr!(self), size) }
    }
    pub fn set_count(&self, count: u32) {
        unsafe { ffi::awra_drag_preview_set_count(ptr!(self), count) }
    }
    /// Item `index` (0 in front, up to 2), an image; None: none.
    pub fn set_item(&self, index: u32, texture: Option<&crate::Texture>) {
        let t = texture.map_or(std::ptr::null(), |t| t.as_ptr() as *const _);
        unsafe { ffi::awra_drag_preview_set_item(ptr!(self), index as i32, t) }
    }
    /// Item `index` as a theme icon at the pile's size.
    pub fn set_item_icon(&self, index: u32, icon_name: &str) {
        let n = crate::object::cstring(icon_name);
        unsafe { ffi::awra_drag_preview_set_item_icon(ptr!(self), index as i32, n.as_ptr()) }
    }
    /// The image `window` shows under the pointer during a drag, its point
    /// (hot_x, hot_y) under it, kept up to date; None takes it away.
    pub fn set_drag_window(&self, window: Option<&crate::Window>, hot_x: i32, hot_y: i32) {
        let w = window.map_or(std::ptr::null_mut(), |w| w.widget_ptr());
        unsafe { ffi::awra_drag_preview_set_drag_window(ptr!(self), w, hot_x, hot_y) }
    }
    /// The pile as it is drawn now.
    pub fn texture(&self) -> Option<crate::Texture> {
        unsafe { crate::Texture::from_raw(ffi::awra_drag_preview_texture(ptr!(self))) }
    }
}

impl Default for DragPreview {
    fn default() -> Self {
        Self::new()
    }
}

impl crate::TabBar {
    /// Where tab `index` is in the bar (logical px: x, y, width, height).
    pub fn tab_bounds(&self, index: usize) -> Option<(f64, f64, f64, f64)> {
        let (mut x, mut y, mut w, mut h) = (0.0, 0.0, 0.0, 0.0);
        unsafe { ffi::awra_tab_bar_tab_bounds(ptr!(self), index as i32, &mut x, &mut y, &mut w, &mut h) }
            .then_some((x, y, w, h))
    }
}

impl crate::Texture {
    /// Part of the texture (pixels), copied.
    pub fn crop(&self, x: i32, y: i32, width: i32, height: i32) -> Option<crate::Texture> {
        let (tw, th) = (self.width(), self.height());
        let (x0, y0) = (x.clamp(0, tw), y.clamp(0, th));
        let (x1, y1) = ((x + width).clamp(0, tw), (y + height).clamp(0, th));
        if x1 <= x0 || y1 <= y0 {
            return None;
        }
        let mut all = vec![0u8; tw as usize * th as usize * 4];
        if !self.download(&mut all, tw as usize * 4) {
            return None;
        }
        let stride = tw as usize * 4;
        let start = y0 as usize * stride + x0 as usize * 4;
        crate::Texture::from_rgba(x1 - x0, y1 - y0, &all[start..], stride)
    }
}

impl crate::Popover {
    /// Opens it pointing at a point of `relative_to`, as GtkPopover's
    /// `set_pointing_to` with `PositionType::Bottom`: centred below it,
    /// above it without room (context menus).
    pub fn point_to(&self, relative_to: &impl IsWidget, x: f64, y: f64) {
        unsafe { ffi::awra_popover_point_to(ptr!(self), relative_to.widget_ptr(), x, y) }
    }
}

impl crate::Window {
    /// Stays above another application's window, as the desktop portal names
    /// it ("wayland:<handle>", "x11:<hex id>"); "" lets it go.
    pub fn set_transient_for_exported(&self, parent: &str) {
        let p = crate::object::cstring(parent);
        unsafe { ffi::awra_window_set_transient_for_exported(ptr!(self), p.as_ptr()) }
    }
}
