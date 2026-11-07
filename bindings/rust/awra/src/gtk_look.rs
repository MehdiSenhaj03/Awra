//! GTK widgets that Awra 1 left to GTK's own theme, drawn as GTK's default
//! theme drew them in an Awra 1 window.

use crate::ffi;
use crate::Entry;

impl Entry {
    /// A plain `gtk::Entry` (not an `awra::Entry`): GTK's default theme
    /// look (32 px inside a 1 px border, radius 5 px) instead of Awra's well.
    pub fn new_gtk() -> Entry {
        unsafe { Entry::from_full(ffi::awra_entry_new_gtk()) }
    }
}
