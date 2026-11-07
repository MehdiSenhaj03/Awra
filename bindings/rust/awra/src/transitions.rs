//! Style transitions settled at once (a recycled cell's classes changed with
//! the item it shows, not for that item).

use crate::ffi;
use crate::object::IsWidget;

/// `awra_widget_finish_transitions`: the transitions in `widget` and its
/// children (the "suggested" role's icon glow) jump to what its classes
/// say now.
pub fn widget_finish_transitions(widget: &impl IsWidget) {
    unsafe { ffi::awra_widget_finish_transitions(widget.widget_ptr()) }
}
