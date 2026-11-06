<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Live native frames

For cameras, media players and other native producers, use `AwraFramePool`
and `AwraTextureStream` rather than copying each frame through a language
binding and replacing a picture's paintable at video cadence.

Create a pool with visible dimensions, a supported `GdkMemoryFormat`, capacity
between 2 and 16, and a color state. Allocation is lazy and bounded. RGB
defaults to sRGB; planar YUV requires an explicit state describing its matrix,
transfer, primaries and range. Awra does not infer those from the byte format.
Use `get_n_planes()`, `get_stride()` and `get_lines()` to negotiate producer
buffers. Each plane, stride and padded height is aligned to 32 bytes.
Visible YUV 4:2:0 dimensions must be even, as required by GDK's format block size.

Acquire a frame on the producer thread. A null result means all slots are
still retained: skip publication or use a separate writable discard buffer
when the producer requires an address. Acquisition never waits for GTK.
`get_address(plane)` is a native interop address, not a checked memory view.
Write no more than `stride * lines` bytes for that plane. Each frame has a
single producer; do not mutate or publish the same lease concurrently.

After filling the planes, call `create_texture()` and release the producer's
frame reference. The immutable `GdkTexture` retains the lease without copying
pixels, including after the pool's external owner is released. Never write
through previously returned addresses after sealing or releasing a lease.
Addresses queried after sealing return zero. Rust projects the pool as
`Send + Sync`. Each frame stays with its producer; publish a thread-safe
`GdkTexture` to move the image to another thread. Dereferencing an address
needs unsafe code. Texture publication is independent of GTK widgets.

On the UI context, call `stream.set_frame(texture, display_aspect)` with a
display ratio or zero for square pixels, and assign the stream once to one
or more `GtkPicture` widgets. A new frame invalidates contents; only changed
geometry invalidates layout. Linear GPU sampling avoids recalculating CPU
mip levels for every large planar frame. The source texture remains at its
original dimensions. `set_frame(NULL, 0)` clears it.

For a small live view, `sealed_frame.copy_scaled(destination)` produces a
bilinear preview in another pool with the same format and color state. The
original texture remains intact for larger views and captures. This avoids
uploading a full 4K frame to a thumbnail-sized surface. Destination dimensions
must not exceed the source; publish the destination lease normally after the
copy. RGB, I420 and NV12 planes retain their encoding and sample layout.

Use `get_current_image()` when retaining a still image: it remains immutable
when subsequent frames arrive. `get_texture()` returns the source texture;
its pixel dimensions may differ from the display aspect for anamorphic media.
`AwraAspectPaintable` can express that ratio for other paintable sources too.

Validate actual frame-clock paints, latency and memory on the target driver,
not just decoder callbacks. Rendering, hardware decoding and codec selection
remain consumer/platform policies. GTK documents the supported planar formats
in [GdkMemoryFormat](https://docs.gtk.org/gdk4/enum.MemoryFormat.html).
