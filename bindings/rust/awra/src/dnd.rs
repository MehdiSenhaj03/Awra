//! Drag and drop within the application: the destination state.

use crate::ffi;
use crate::object::Widget;

impl Widget {
    /// GTK's :drop(active): the destination a drag hovers, drawn with the
    /// drop colour's 1 px ring.
    pub fn set_drop_active(&self, active: bool) {
        unsafe { ffi::awra_widget_set_drop_active(self.as_ptr(), active) }
    }
    pub fn is_drop_active(&self) -> bool {
        unsafe { ffi::awra_widget_drop_active(self.as_ptr()) }
    }
}
