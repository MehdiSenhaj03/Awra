//! LevelMeter: a stereo (or mono) peak meter with peak hold, for audio
//! applications. Feed it levels (linear, 0..1+) as often as you like; it
//! draws with the theme's colours (green, warning, danger near 0 dBFS).
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use std::cell::Cell;
use std::rc::Rc;

use crate::gdk::RGBA;
use crate::{DrawingArea, Orientation, StyleManager};

#[derive(Default)]
struct Levels {
    l: Cell<f32>,
    r: Cell<f32>,
    hold_l: Cell<f32>,
    hold_r: Cell<f32>,
    hold_t: Cell<f64>,
    reduction: Cell<f32>,
}

/// dB shown from -60 to +3.
fn pos(level: f32) -> f64 {
    if level <= 1e-5 {
        return 0.0;
    }
    let db = 20.0 * level.log10();
    (((db + 60.0) / 63.0) as f64).clamp(0.0, 1.0)
}

#[derive(Clone)]
pub struct LevelMeter {
    area: DrawingArea,
    levels: Rc<Levels>,
}

impl AsRef<crate::Widget> for LevelMeter {
    fn as_ref(&self) -> &crate::Widget {
        self.area.as_ref()
    }
}

impl LevelMeter {
    pub fn new(orientation: Orientation) -> LevelMeter {
        let levels = Rc::new(Levels::default());
        let lv = levels.clone();
        let vertical = orientation == Orientation::Vertical;
        let area = DrawingArea::with_canvas(move |_, c, w, h| {
            let (w, h) = (w as f64, h as f64);
            let style = StyleManager::default();
            let track = style.color("control-track").unwrap_or(RGBA::new(0.5, 0.5, 0.5, 0.25));
            let ok = style.color("success").unwrap_or(RGBA::new(0.35, 0.8, 0.45, 1.0));
            let warn = style.color("warning").unwrap_or(RGBA::new(0.95, 0.7, 0.2, 1.0));
            let bad = style.color("danger").unwrap_or(RGBA::new(0.9, 0.3, 0.3, 1.0));
            let bars = [(lv.l.get(), lv.hold_l.get()), (lv.r.get(), lv.hold_r.get())];
            let gap = 2.0;
            for (i, (level, hold)) in bars.iter().enumerate() {
                let (x, y, bw, bh) = if vertical {
                    let bw = (w - gap) / 2.0;
                    (i as f64 * (bw + gap), 0.0, bw, h)
                } else {
                    let bh = (h - gap) / 2.0;
                    (0.0, i as f64 * (bh + gap), w, bh)
                };
                c.round_rect(x, y, bw, bh, 1.5, Some(&track), 0.0, None);
                let p = pos(*level);
                let color = if *level >= 0.99 { bad } else if *level >= 0.5 { warn } else { ok };
                if vertical {
                    let len = bh * p;
                    c.round_rect(x, y + bh - len, bw, len, 1.5, Some(&color), 0.0, None);
                    let hp = bh - bh * pos(*hold);
                    if *hold > 1e-4 {
                        c.fill_rect(x, hp, bw, 1.5, &color);
                    }
                } else {
                    c.round_rect(x, y, bw * p, bh, 1.5, Some(&color), 0.0, None);
                    if *hold > 1e-4 {
                        c.fill_rect(x + bw * pos(*hold) - 1.5, y, 1.5, bh, &color);
                    }
                }
            }
            let gr = lv.reduction.get();
            if gr > 0.1 && !vertical {
                let gw = w * (gr as f64 / 24.0).min(1.0);
                c.fill_rect(w - gw, h - 2.0, gw, 2.0, &warn);
            }
        });
        if vertical {
            area.set_content_size(10, 120);
        } else {
            area.set_content_size(120, 8);
        }
        LevelMeter { area, levels }
    }

    /// New levels (linear peak per channel since the last call).
    pub fn set_levels(&self, l: f32, r: f32) {
        let lv = &self.levels;
        // fall back smoothly, rise at once
        lv.l.set(l.max(lv.l.get() * 0.82));
        lv.r.set(r.max(lv.r.get() * 0.82));
        let now = crate::now_ms();
        if l >= lv.hold_l.get() || r >= lv.hold_r.get() || now - lv.hold_t.get() > 1500.0 {
            lv.hold_l.set(l.max(if now - lv.hold_t.get() > 1500.0 { 0.0 } else { lv.hold_l.get() }));
            lv.hold_r.set(r.max(if now - lv.hold_t.get() > 1500.0 { 0.0 } else { lv.hold_r.get() }));
            lv.hold_t.set(now);
        }
        self.area.queue_draw();
    }

    /// Gain reduction (dB) shown under a horizontal meter.
    pub fn set_reduction(&self, db: f32) {
        self.levels.reduction.set(db);
    }

    pub fn widget(&self) -> &DrawingArea {
        &self.area
    }
}
