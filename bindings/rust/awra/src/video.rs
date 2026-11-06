//! Playback components: the video view (and its thread-safe frame sink),
//! the timeline, the overlay bar, the edge panel, and the chrome that
//! floats over video (frosted backdrops, overlay title bar, window
//! overlays).

use std::ffi::c_void;

use crate::awra_wrapper;
use crate::enums::*;
use crate::ffi;
use crate::gdk::RGBA;
use crate::object::{cstring, IntoOptWidget, IsWidget, Widget};
use crate::signal::{sig, SignalHandlerId};

macro_rules! ptr {
    ($s:expr) => {
        $s.0.as_ptr()
    };
}

/// Pixel layouts of [`VideoView`] frames.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum VideoFormat {
    /// Y plane, then interleaved CbCr at half resolution (limited range).
    Nv12 = 0,
    /// Premultiplied 0xAARRGGBB words.
    Bgra = 1,
    /// 0x..RRGGBB words, shown opaque.
    Bgrx = 2,
}

awra_wrapper!(
    /// Frames another thread produces (a decoder), written straight into
    /// buffers the view owns and shown fitted, with no per-frame copy or
    /// allocation. Only the view's rectangle is redrawn per frame.
    VideoView, ["AwraVideoView"]
);
impl VideoView {
    pub fn new() -> VideoView {
        unsafe { VideoView::from_full(ffi::awra_video_view_new()) }
    }
    /// A handle producer threads use. The view must outlive the producer.
    pub fn sink(&self) -> VideoSink {
        VideoSink(ptr!(self) as usize)
    }
    pub fn clear(&self) {
        unsafe { ffi::awra_video_view_clear(ptr!(self)) }
    }
    pub fn has_frame(&self) -> bool {
        unsafe { ffi::awra_video_view_has_frame(ptr!(self)) }
    }
    /// Pixels and display aspect ratio of the frame shown.
    pub fn frame_size(&self) -> (i32, i32, f64) {
        let (mut w, mut h, mut a) = (0, 0, 0.0);
        unsafe { ffi::awra_video_view_frame_size(ptr!(self), &mut w, &mut h, &mut a) };
        (w, h, a)
    }
    pub fn set_content_fit(&self, fit: ContentFit) {
        let f = match fit {
            ContentFit::Fill => 0,
            ContentFit::Cover => 2,
            _ => 1,
        };
        unsafe { ffi::awra_video_view_set_content_fit(ptr!(self), f) }
    }
    pub fn content_fit(&self) -> ContentFit {
        match unsafe { ffi::awra_video_view_content_fit(ptr!(self)) } {
            0 => ContentFit::Fill,
            2 => ContentFit::Cover,
            _ => ContentFit::Contain,
        }
    }
    pub fn set_background(&self, color: Option<&RGBA>) {
        let v = color.map(|c| c.to_ffi());
        unsafe { ffi::awra_video_view_set_background(ptr!(self), v.as_ref().map_or(std::ptr::null(), |c| c)) }
    }
    /// Writes the frame shown, at its display aspect, as a PNG.
    pub fn save_png(&self, path: impl AsRef<std::path::Path>) -> bool {
        let p = cstring(&path.as_ref().to_string_lossy());
        unsafe { ffi::awra_video_view_save_png(ptr!(self), p.as_ptr()) }
    }
    /// Frames presented, dropped (replaced before shown or no buffer), drawn.
    pub fn stats(&self) -> (u64, u64, u64) {
        let (mut p, mut d, mut r) = (0u64, 0u64, 0u64);
        unsafe { ffi::awra_video_view_stats(ptr!(self), &mut p, &mut d, &mut r) };
        (p, d, r)
    }
    /// Called on the UI thread when a new frame is shown.
    pub fn connect_frame<F: Fn(&VideoView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<VideoView, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("frame") {
                f(w)
            }
        })
    }
}
impl Default for VideoView {
    fn default() -> Self {
        Self::new()
    }
}

/// A frame being written (from [`VideoSink::acquire`]).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct VideoFrame {
    pub handle: *mut c_void,
    pub planes: [*mut u8; 2],
}
unsafe impl Send for VideoFrame {}

/// The producer side of a [`VideoView`]: usable from any thread while the
/// view lives.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct VideoSink(usize);
unsafe impl Send for VideoSink {}
unsafe impl Sync for VideoSink {}

impl VideoSink {
    fn p(&self) -> *mut ffi::AwraWidget {
        self.0 as *mut ffi::AwraWidget
    }
    /// (Re)allocates the frames; returns the planes' (row bytes, rows).
    pub fn configure(
        &self,
        format: VideoFormat,
        width: i32,
        height: i32,
        pixel_aspect: f64,
        buffers: i32,
    ) -> Option<([i32; 2], [i32; 2])> {
        let (mut strides, mut lines) = ([0i32; 2], [0i32; 2]);
        let ok = unsafe {
            ffi::awra_video_view_configure(
                self.p(),
                format as _,
                width,
                height,
                pixel_aspect,
                buffers,
                strides.as_mut_ptr(),
                lines.as_mut_ptr(),
            )
        };
        ok.then_some((strides, lines))
    }
    pub fn acquire(&self) -> Option<VideoFrame> {
        let mut planes = [std::ptr::null_mut::<c_void>(); 2];
        let h = unsafe { ffi::awra_video_view_acquire(self.p(), planes.as_mut_ptr()) };
        (!h.is_null()).then(|| VideoFrame { handle: h, planes: [planes[0] as *mut u8, planes[1] as *mut u8] })
    }
    pub fn present(&self, frame: *mut c_void) {
        unsafe { ffi::awra_video_view_present(self.p(), frame) }
    }
    pub fn release(&self, frame: *mut c_void) {
        unsafe { ffi::awra_video_view_release(self.p(), frame) }
    }
    /// The view, as a raw pointer (for C producers).
    pub fn as_raw(&self) -> *mut c_void {
        self.0 as *mut c_void
    }
}

// ===========================================================================

awra_wrapper!(
    /// Awra 1's AwraTimeline: playhead, intervals and markers, hovered time
    /// with a floating preview, seek requests; editable mode drags an offset.
    Timeline, ["AwraTimeline"]
);
impl Timeline {
    pub fn new() -> Timeline {
        unsafe { Timeline::from_full(ffi::awra_timeline_new()) }
    }
    pub fn set_duration(&self, s: f64) {
        unsafe { ffi::awra_timeline_set_duration(ptr!(self), s) }
    }
    pub fn duration(&self) -> f64 {
        unsafe { ffi::awra_timeline_duration(ptr!(self)) }
    }
    pub fn set_position(&self, s: f64) {
        unsafe { ffi::awra_timeline_set_position(ptr!(self), s) }
    }
    pub fn position(&self) -> f64 {
        unsafe { ffi::awra_timeline_position(ptr!(self)) }
    }
    pub fn set_view(&self, start: f64, duration: f64) {
        unsafe { ffi::awra_timeline_set_view(ptr!(self), start, duration) }
    }
    pub fn set_editable(&self, e: bool) {
        unsafe { ffi::awra_timeline_set_editable(ptr!(self), e) }
    }
    pub fn set_offset(&self, s: f64) {
        unsafe { ffi::awra_timeline_set_offset(ptr!(self), s) }
    }
    pub fn offset(&self) -> f64 {
        unsafe { ffi::awra_timeline_offset(ptr!(self)) }
    }
    pub fn set_increments(&self, step: f64, page: f64) {
        unsafe { ffi::awra_timeline_set_increments(ptr!(self), step, page) }
    }
    pub fn add_interval(&self, start: f64, end: f64, label: Option<&str>) {
        let l = label.map(cstring);
        unsafe {
            ffi::awra_timeline_add_interval(ptr!(self), start, end, l.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()))
        }
    }
    pub fn clear_intervals(&self) {
        unsafe { ffi::awra_timeline_clear_intervals(ptr!(self)) }
    }
    /// Negative when not hovered.
    pub fn hover_time(&self) -> f64 {
        unsafe { ffi::awra_timeline_hover_time(ptr!(self)) }
    }
    pub fn set_preview(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_timeline_set_preview(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn preview_surface(&self) -> Option<crate::Surface> {
        unsafe { Widget::from_none_opt(ffi::awra_timeline_preview_surface(ptr!(self))) }.and_then(|w| w.downcast().ok())
    }
    pub fn connect_seek_requested<F: Fn(&Timeline, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Timeline, _>(sig::SEEK, move |w, a| f(w, a.value()))
    }
    pub fn connect_hover_time_notify<F: Fn(&Timeline) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Timeline, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("hover-time") {
                f(w)
            }
        })
    }
    pub fn connect_offset_notify<F: Fn(&Timeline) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Timeline, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("offset") {
                f(w)
            }
        })
    }
    pub fn connect_offset_committed<F: Fn(&Timeline, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Timeline, _>(sig::VALUE_CHANGED, move |w, a| f(w, a.value()))
    }
}
impl Default for Timeline {
    fn default() -> Self {
        Self::new()
    }
}

impl crate::Surface {
    /// The blurred image of `source` (a [`VideoView`]) under the surface
    /// (Awra 1's backdrop-widget).
    pub fn set_backdrop_widget(&self, source: Option<&impl IsWidget>) {
        unsafe { ffi::awra_surface_set_backdrop(AsRef::<Widget>::as_ref(self).as_ptr(), source.map_or(std::ptr::null_mut(), |s| s.widget_ptr())) }
    }
}

impl crate::Header {
    pub fn set_backdrop_widget(&self, source: Option<&impl IsWidget>) {
        unsafe { ffi::awra_header_set_backdrop(AsRef::<Widget>::as_ref(self).as_ptr(), source.map_or(std::ptr::null_mut(), |s| s.widget_ptr())) }
    }
    /// The material painted when the header does not blend with the window.
    pub fn set_material(&self, m: Material) {
        unsafe { ffi::awra_header_set_material(AsRef::<Widget>::as_ref(self).as_ptr(), m as _) }
    }
    /// 0: none; otherwise the header spans this width, centered.
    pub fn set_max_width(&self, w: i32) {
        unsafe { ffi::awra_header_set_max_width(AsRef::<Widget>::as_ref(self).as_ptr(), w) }
    }
}

impl crate::Window {
    /// The title bar floats over the content (Awra 1's overlay-chrome).
    pub fn set_overlay_chrome(&self, overlay: bool) {
        unsafe { ffi::awra_window_set_overlay_titlebar(AsRef::<Widget>::as_ref(self).as_ptr(), overlay) }
    }
    pub fn overlay_chrome(&self) -> bool {
        unsafe { ffi::awra_window_overlay_titlebar(AsRef::<Widget>::as_ref(self).as_ptr()) }
    }
    pub fn add_overlay(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_window_add_overlay(AsRef::<Widget>::as_ref(self).as_ptr(), child.widget_ptr()) }
    }
    pub fn remove_overlay(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_window_remove_overlay(AsRef::<Widget>::as_ref(self).as_ptr(), child.widget_ptr()) }
    }
    pub fn place_overlay(&self, child: &impl IsWidget, x: f64, y: f64) {
        unsafe { ffi::awra_window_place_overlay(AsRef::<Widget>::as_ref(self).as_ptr(), child.widget_ptr(), x, y) }
    }
}

// ===========================================================================

awra_wrapper!(
    /// Awra 1's AwraOverlayBar: content at full size and a floating bar at
    /// its bottom that hides itself after a delay while auto-hide is on.
    OverlayBar, ["AwraOverlayBar"]
);
impl OverlayBar {
    pub fn new() -> OverlayBar {
        unsafe { OverlayBar::from_full(ffi::awra_overlay_bar_new()) }
    }
    pub fn set_content(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_overlay_bar_set_content(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_bar(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_overlay_bar_set_bar(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn surface(&self) -> crate::Surface {
        unsafe { Widget::from_none(ffi::awra_overlay_bar_surface(ptr!(self))) }.downcast().unwrap()
    }
    pub fn set_revealed(&self, r: bool) {
        unsafe { ffi::awra_overlay_bar_set_revealed(ptr!(self), r) }
    }
    pub fn is_revealed(&self) -> bool {
        unsafe { ffi::awra_overlay_bar_revealed(ptr!(self)) }
    }
    pub fn reveal(&self) {
        unsafe { ffi::awra_overlay_bar_reveal(ptr!(self)) }
    }
    pub fn set_auto_hide(&self, a: bool) {
        unsafe { ffi::awra_overlay_bar_set_auto_hide(ptr!(self), a) }
    }
    pub fn set_inhibit_auto_hide(&self, i: bool) {
        unsafe { ffi::awra_overlay_bar_set_inhibit_auto_hide(ptr!(self), i) }
    }
    pub fn set_hide_delay(&self, ms: u32) {
        unsafe { ffi::awra_overlay_bar_set_hide_delay(ptr!(self), ms) }
    }
    pub fn set_max_width(&self, w: i32) {
        unsafe { ffi::awra_overlay_bar_set_max_width(ptr!(self), w) }
    }
    pub fn set_inset(&self, s: Spacing) {
        unsafe { ffi::awra_overlay_bar_set_inset(ptr!(self), s as _) }
    }
    /// The inset in logical pixels.
    pub fn inset(&self) -> i32 {
        unsafe { ffi::awra_overlay_bar_inset_px(ptr!(self)) }
    }
    pub fn connect_revealed_notify<F: Fn(&OverlayBar) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<OverlayBar, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("revealed") {
                f(w)
            }
        })
    }
}
impl Default for OverlayBar {
    fn default() -> Self {
        Self::new()
    }
}

/// Edges of an [`EdgePanel`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PanelEdge {
    Start = 0,
    End = 1,
}

awra_wrapper!(
    /// Awra 1's AwraEdgePanel: content at full size and a floating panel
    /// sliding in from an edge.
    EdgePanel, ["AwraEdgePanel"]
);
impl EdgePanel {
    pub fn new() -> EdgePanel {
        unsafe { EdgePanel::from_full(ffi::awra_edge_panel_new()) }
    }
    pub fn set_content(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_edge_panel_set_content(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_panel(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_edge_panel_set_panel(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn surface(&self) -> crate::Surface {
        unsafe { Widget::from_none(ffi::awra_edge_panel_surface(ptr!(self))) }.downcast().unwrap()
    }
    pub fn set_edge(&self, e: PanelEdge) {
        unsafe { ffi::awra_edge_panel_set_edge(ptr!(self), e as i32) }
    }
    pub fn set_panel_width(&self, w: i32) {
        unsafe { ffi::awra_edge_panel_set_panel_width(ptr!(self), w) }
    }
    pub fn set_revealed(&self, r: bool) {
        unsafe { ffi::awra_edge_panel_set_revealed(ptr!(self), r) }
    }
    pub fn is_revealed(&self) -> bool {
        unsafe { ffi::awra_edge_panel_revealed(ptr!(self)) }
    }
    pub fn connect_revealed_notify<F: Fn(&EdgePanel) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<EdgePanel, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("revealed") {
                f(w)
            }
        })
    }
}
impl Default for EdgePanel {
    fn default() -> Self {
        Self::new()
    }
}
