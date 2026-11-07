//! The tile grid: GtkGridView's layout for a set of children
//! (`awra_tile_grid_*`).

use crate::awra_wrapper;
use crate::ffi;
use crate::object::IsWidget;

macro_rules! ptr {
    ($s:expr) => {
        $s.0.as_ptr()
    };
}

awra_wrapper!(
    /// Children laid out as GtkGridView lays out its items: columns as wide
    /// as the widest child's minimum allows, rows as tall as their tallest
    /// child's minimum height (no virtualization).
    TileGrid, ["AwraTileGrid"]
);
impl TileGrid {
    pub fn new() -> TileGrid {
        unsafe { TileGrid::from_full(ffi::awra_tile_grid_new()) }
    }
    pub fn append(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_tile_grid_append(ptr!(self), child.widget_ptr()) }
    }
    pub fn insert(&self, child: &impl IsWidget, position: i32) {
        unsafe { ffi::awra_tile_grid_insert(ptr!(self), child.widget_ptr(), position) }
    }
    pub fn remove(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_remove(ptr!(self), child.widget_ptr()) }
    }
    pub fn remove_all(&self) {
        unsafe { ffi::awra_container_remove_all(ptr!(self)) }
    }
    /// GtkGridView's min-columns / max-columns (1 and 7 by default).
    pub fn set_columns(&self, min: u32, max: u32) {
        unsafe { ffi::awra_tile_grid_set_columns(ptr!(self), min as i32, max as i32) }
    }
    pub fn set_min_columns(&self, n: u32) {
        let max = self.0.rust_data::<u32>("tile-max").unwrap_or(7);
        self.0.set_rust_data("tile-min", n);
        self.set_columns(n, max)
    }
    pub fn set_max_columns(&self, n: u32) {
        let min = self.0.rust_data::<u32>("tile-min").unwrap_or(1);
        self.0.set_rust_data("tile-max", n);
        self.set_columns(min, n)
    }
    /// Logical px between rows and between columns.
    pub fn set_spacing(&self, row: i32, column: i32) {
        unsafe { ffi::awra_tile_grid_set_spacing(ptr!(self), row, column) }
    }
    /// Rows as tall as their tallest child's natural height
    /// (GTK_SCROLL_NATURAL) instead of its minimum.
    pub fn set_natural_rows(&self, natural: bool) {
        unsafe { ffi::awra_tile_grid_set_natural_rows(ptr!(self), natural) }
    }
    /// The columns for the grid's current width.
    pub fn columns(&self) -> u32 {
        unsafe { ffi::awra_tile_grid_columns(ptr!(self)) as u32 }
    }
}
impl Default for TileGrid {
    fn default() -> Self {
        Self::new()
    }
}
