//! Textures over pixels they keep and read in place (GdkMemoryTexture over
//! a GBytes): a picture's RGBA or a video frame's planes, without a copy.

use std::ffi::c_void;

use crate::ffi;
use crate::Texture;

/// How the pixels given to [`Texture::for_data`] are laid out. Rows are
/// packed.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TextureData {
    /// Four bytes a pixel, R G B A, straight alpha (GDK_MEMORY_R8G8B8A8).
    Rgba,
    /// Native-endian 0xAARRGGBB words, premultiplied.
    ArgbPremultiplied,
    /// Video, 4:2:0, limited range: the luma plane, then Cb and Cr
    /// interleaved at half the width and height. Even width and height.
    Nv12 { bt709: bool },
    /// Video, 4:2:0, limited range: the luma plane, then the Cb plane and
    /// the Cr plane at half the width and height. Even width and height.
    I420 { bt709: bool },
}

impl TextureData {
    /// The bytes a `width` by `height` image takes.
    pub fn size(self, width: usize, height: usize) -> usize {
        match self {
            TextureData::Rgba | TextureData::ArgbPremultiplied => width * height * 4,
            TextureData::Nv12 { .. } | TextureData::I420 { .. } => width * height * 3 / 2,
        }
    }
}

unsafe extern "C" fn release<T>(data: *mut c_void) {
    drop(unsafe { Box::from_raw(data.cast::<T>()) });
}

impl Texture {
    /// A texture of `data`, which it keeps, reads where it is, and drops
    /// once its last reference goes (from whichever thread that is). Gives
    /// `data` back when it can't be shown so: too short, misaligned (pixels
    /// are read a word at a time), or 4:2:0 of an odd size.
    pub fn for_data<T: AsRef<[u8]> + Send + 'static>(
        width: i32,
        height: i32,
        format: TextureData,
        data: T,
    ) -> Result<Texture, T> {
        if width <= 0 || height <= 0 {
            return Err(data);
        }
        let boxed = Box::new(data);
        let bytes: &[u8] = (*boxed).as_ref();
        let (pointer, length) = (bytes.as_ptr(), bytes.len());
        if length < format.size(width as usize, height as usize) {
            return Err(*boxed);
        }
        let (code, bt709) = match format {
            TextureData::Rgba => (ffi::AWRA_TEXTURE_RGBA, false),
            TextureData::ArgbPremultiplied => (ffi::AWRA_TEXTURE_ARGB_PREMULTIPLIED, false),
            TextureData::Nv12 { bt709 } => (ffi::AWRA_TEXTURE_NV12, bt709),
            TextureData::I420 { bt709 } => (ffi::AWRA_TEXTURE_I420, bt709),
        };
        let user = Box::into_raw(boxed);
        // SAFETY: `pointer` is `*user`'s bytes, which stay where they are
        // until release drops the box.
        let texture = unsafe {
            ffi::awra_texture_new_for_data(
                pointer.cast(),
                width,
                height,
                code,
                bt709,
                Some(release::<T>),
                user.cast(),
            )
        };
        // SAFETY: on failure the texture didn't take the box.
        unsafe { Texture::from_raw(texture) }.ok_or_else(|| *unsafe { Box::from_raw(user) })
    }
}
