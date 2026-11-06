//! Colors, keys and modifiers (the subset of GDK the applications use).

use std::collections::HashSet;
use std::sync::Mutex;

use crate::ffi;

/// A color with straight alpha, components 0..1.
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct RGBA {
    red: f32,
    green: f32,
    blue: f32,
    alpha: f32,
}

impl RGBA {
    pub const BLACK: RGBA = RGBA { red: 0.0, green: 0.0, blue: 0.0, alpha: 1.0 };
    pub const WHITE: RGBA = RGBA { red: 1.0, green: 1.0, blue: 1.0, alpha: 1.0 };
    pub const TRANSPARENT: RGBA = RGBA { red: 0.0, green: 0.0, blue: 0.0, alpha: 0.0 };

    pub const fn new(red: f32, green: f32, blue: f32, alpha: f32) -> RGBA {
        RGBA { red, green, blue, alpha }
    }
    pub fn red(&self) -> f32 {
        self.red
    }
    pub fn green(&self) -> f32 {
        self.green
    }
    pub fn blue(&self) -> f32 {
        self.blue
    }
    pub fn alpha(&self) -> f32 {
        self.alpha
    }
    pub fn set_red(&mut self, v: f32) {
        self.red = v
    }
    pub fn set_green(&mut self, v: f32) {
        self.green = v
    }
    pub fn set_blue(&mut self, v: f32) {
        self.blue = v
    }
    pub fn set_alpha(&mut self, v: f32) {
        self.alpha = v
    }
    pub fn with_alpha(mut self, alpha: f32) -> RGBA {
        self.alpha = alpha;
        self
    }
    /// "#rrggbb", "#rrggbbaa", "#rgb".
    pub fn parse(s: &str) -> Option<RGBA> {
        let h = s.trim().strip_prefix('#')?;
        let v = |i: usize, n: usize| u8::from_str_radix(&h[i..i + n], 16).ok();
        let (r, g, b, a) = match h.len() {
            3 => (v(0, 1)? * 17, v(1, 1)? * 17, v(2, 1)? * 17, 255),
            6 => (v(0, 2)?, v(2, 2)?, v(4, 2)?, 255),
            8 => (v(0, 2)?, v(2, 2)?, v(4, 2)?, v(6, 2)?),
            _ => return None,
        };
        Some(RGBA::new(r as f32 / 255.0, g as f32 / 255.0, b as f32 / 255.0, a as f32 / 255.0))
    }
    pub(crate) fn to_ffi(self) -> ffi::AwraRGBA {
        ffi::AwraRGBA { r: self.red, g: self.green, b: self.blue, a: self.alpha }
    }
    pub(crate) fn from_ffi(c: ffi::AwraRGBA) -> RGBA {
        RGBA::new(c.r, c.g, c.b, c.a)
    }
}

/// A key symbol by its X11 name ("Escape", "a", "Left").
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Key(&'static str);

static INTERNED: Mutex<Option<HashSet<&'static str>>> = Mutex::new(None);

#[allow(non_upper_case_globals)]
impl Key {
    pub const Escape: Key = Key("Escape");
    pub const Return: Key = Key("Return");
    pub const KP_Enter: Key = Key("KP_Enter");
    pub const Tab: Key = Key("Tab");
    pub const space: Key = Key("space");
    pub const BackSpace: Key = Key("BackSpace");
    pub const Delete: Key = Key("Delete");
    pub const Left: Key = Key("Left");
    pub const Right: Key = Key("Right");
    pub const Up: Key = Key("Up");
    pub const Down: Key = Key("Down");
    pub const Home: Key = Key("Home");
    pub const End: Key = Key("End");
    pub const Page_Up: Key = Key("Page_Up");
    pub const Page_Down: Key = Key("Page_Down");
    pub const F1: Key = Key("F1");
    pub const F2: Key = Key("F2");
    pub const F11: Key = Key("F11");

    /// The key named `name` (interned).
    pub fn from_name(name: &str) -> Key {
        let mut g = INTERNED.lock().unwrap();
        let set = g.get_or_insert_with(HashSet::new);
        if let Some(s) = set.get(name) {
            return Key(s);
        }
        let s: &'static str = Box::leak(name.to_owned().into_boxed_str());
        set.insert(s);
        Key(s)
    }
    pub fn name(&self) -> &'static str {
        self.0
    }
    /// The character for single-character key names.
    pub fn to_unicode(&self) -> Option<char> {
        let mut it = self.0.chars();
        match (it.next(), it.next()) {
            (Some(c), None) => Some(c),
            _ => None,
        }
    }
}

/// Modifier keys held.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct ModifierType(u32);

impl ModifierType {
    pub const SHIFT_MASK: ModifierType = ModifierType(1);
    pub const CONTROL_MASK: ModifierType = ModifierType(2);
    pub const ALT_MASK: ModifierType = ModifierType(4);
    pub const SUPER_MASK: ModifierType = ModifierType(64);

    pub const fn empty() -> ModifierType {
        ModifierType(0)
    }
    pub fn bits(&self) -> u32 {
        self.0
    }
    pub fn contains(&self, other: ModifierType) -> bool {
        self.0 & other.0 == other.0
    }
    pub fn intersects(&self, other: ModifierType) -> bool {
        self.0 & other.0 != 0
    }
    pub fn is_empty(&self) -> bool {
        self.0 == 0
    }
    pub(crate) fn from_awra(mods: i32) -> ModifierType {
        ModifierType(mods as u32 & (1 | 2 | 4 | 64))
    }
}

impl std::ops::BitOr for ModifierType {
    type Output = ModifierType;
    fn bitor(self, o: ModifierType) -> ModifierType {
        ModifierType(self.0 | o.0)
    }
}
impl std::ops::BitAnd for ModifierType {
    type Output = ModifierType;
    fn bitand(self, o: ModifierType) -> ModifierType {
        ModifierType(self.0 & o.0)
    }
}

pub const BUTTON_PRIMARY: u32 = 1;
pub const BUTTON_MIDDLE: u32 = 2;
pub const BUTTON_SECONDARY: u32 = 3;
