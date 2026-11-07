//! Colors, keys and modifiers (the subset of GDK the applications use).

use std::collections::HashSet;
use std::sync::Mutex;

use crate::ffi;

pub use crate::widgets::Rectangle;

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

pub use crate::compat::{MemoryFormat, Paintable, Texture};

/// GdkScrollUnit.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ScrollUnit {
    Wheel,
    Surface,
}

/// GdkMemoryTexture: straight or premultiplied pixels made a texture.
pub struct MemoryTexture;
impl MemoryTexture {
    #[allow(clippy::new_ret_no_self)]
    pub fn new(width: i32, height: i32, format: MemoryFormat, bytes: &crate::glib::Bytes, stride: usize) -> Texture {
        texture_from(width, height, format, bytes, stride).expect("a texture of these pixels")
    }
}

/// Converts pixels of `format` to a texture.
pub fn texture_from(width: i32, height: i32, format: MemoryFormat, px: &[u8], stride: usize) -> Option<Texture> {
    match format {
        MemoryFormat::R8g8b8a8 => Texture::from_rgba(width, height, px, stride),
        _ => {
            let mut rgba = vec![0u8; width as usize * height as usize * 4];
            for y in 0..height as usize {
                for x in 0..width as usize {
                    let o = y * width as usize * 4 + x * 4;
                    match format {
                        MemoryFormat::R8g8b8 => {
                            let i = y * stride + x * 3;
                            rgba[o..o + 3].copy_from_slice(&px[i..i + 3]);
                            rgba[o + 3] = 255;
                        }
                        _ => {
                            let i = y * stride + x * 4;
                            let (r, g, b, a) = match format {
                                MemoryFormat::B8g8r8a8 | MemoryFormat::B8g8r8a8Premultiplied => {
                                    (px[i + 2], px[i + 1], px[i], px[i + 3])
                                }
                                _ => (px[i], px[i + 1], px[i + 2], px[i + 3]),
                            };
                            let un = |c: u8| if a == 0 { 0 } else { ((c as u32 * 255 + a as u32 / 2) / a as u32).min(255) as u8 };
                            let premul = matches!(format, MemoryFormat::B8g8r8a8Premultiplied | MemoryFormat::R8g8b8a8Premultiplied);
                            rgba[o..o + 4].copy_from_slice(&if premul { [un(r), un(g), un(b), a] } else { [r, g, b, a] });
                        }
                    }
                }
            }
            Texture::from_rgba(width, height, &rgba, width as usize * 4)
        }
    }
}

/// GdkMemoryTextureBuilder.
#[derive(Default)]
pub struct MemoryTextureBuilder {
    width: i32,
    height: i32,
    stride: usize,
    format: Option<MemoryFormat>,
    bytes: Option<crate::glib::Bytes>,
}
impl MemoryTextureBuilder {
    pub fn new() -> MemoryTextureBuilder {
        MemoryTextureBuilder::default()
    }
    pub fn set_width(self, width: i32) -> Self {
        Self { width, ..self }
    }
    pub fn set_height(self, height: i32) -> Self {
        Self { height, ..self }
    }
    pub fn set_stride(self, stride: usize) -> Self {
        Self { stride, ..self }
    }
    pub fn set_format(self, format: MemoryFormat) -> Self {
        Self { format: Some(format), ..self }
    }
    pub fn set_bytes(self, bytes: Option<&crate::glib::Bytes>) -> Self {
        Self { bytes: bytes.cloned(), ..self }
    }
    pub fn build(self) -> Texture {
        let b = self.bytes.unwrap_or_else(|| crate::glib::Bytes::from_owned(Vec::new()));
        texture_from(self.width, self.height, self.format.unwrap_or(MemoryFormat::R8g8b8a8), &b, self.stride)
            .expect("a texture of these pixels")
    }
}

pub use crate::compat2::{DragAction, FileList, GdkDisplay as Display};
