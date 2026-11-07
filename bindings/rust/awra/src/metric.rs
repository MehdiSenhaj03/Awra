//! AwraMetricRow: a label and its value (AwraMetadataGroup's rows).

use crate::awra_wrapper;
use crate::ffi;
use crate::object::{cstring, IsWidget, Widget};

awra_wrapper!(MetricRow, ["AwraBox"]);
impl MetricRow {
    pub fn new(label: &str, value: &str) -> MetricRow {
        let (l, v) = (cstring(label), cstring(value));
        unsafe { MetricRow::from_full(ffi::awra_metric_row_new(l.as_ptr(), v.as_ptr())) }
    }
    pub fn set_label(&self, label: &str) {
        let l = cstring(label);
        unsafe { ffi::awra_metric_row_set_label(self.widget_ptr(), l.as_ptr()) }
    }
    pub fn set_value(&self, value: &str) {
        let v = cstring(value);
        unsafe { ffi::awra_metric_row_set_value(self.widget_ptr(), v.as_ptr()) }
    }
    pub fn set_icon_name(&self, name: Option<&str>) {
        let n = name.map(cstring);
        unsafe { ffi::awra_metric_row_set_icon_name(self.widget_ptr(), n.as_ref().map_or(std::ptr::null(), |n| n.as_ptr())) }
    }
    pub(crate) fn from_row(w: Widget) -> MetricRow {
        MetricRow(w)
    }
}
