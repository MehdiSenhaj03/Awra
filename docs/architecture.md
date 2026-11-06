<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Awra 2 architecture

Awra 2 reproduces Awra 1 (GTK 4.22 + Awra's CSS and widgets) pixel for pixel
without GTK. This document says what replaces what, and why the results
match.

## From GTK to BUI

| Awra 1 (GTK) | Awra 2 |
| --- | --- |
| GtkWidget tree, GObject | BKNode tree of BUI's kit; every Awra widget is a node whose `aux` holds Awra's state (refcount, margins, alignment, expand, measure cache, signal handlers, hover/press animations, style classes) |
| GtkLayoutManager, GtkBox… | `awra_measure` / `awra_allocate`: GTK's measure contract (min / natural for a given size, height-for-width), `gtk_widget_adjust_size_allocation` for alignment, `compute_expand` only where GTK computes it (boxes, grids, stacks, scrolled windows, buttons…), `gtk_distribute_natural_allocation` |
| GSK render nodes, GL | the kit's software raster (premultiplied ARGB) drawing only damaged regions, straight into the window's shared-memory buffer (Wayland) or DIB (Win32) |
| CSS (awra.css) | the token set and paint code: every rule of awra.css that Awra 1's components used is a C paint routine with the same numbers (`src/button.c`, `src/paint.c`…) |
| Pango, HarfBuzz, fontconfig | FreeType (fractional sizes, variable fonts), fontconfig loaded at run time; GDI on Windows; GTK's rules for weights, synthetic bold, hinted metrics at scale 1 and Pango's line breaking |
| GLib main loop | the kit's host loops with loop timers; `awra_invoke` from any thread |
| GDK backends | BUI's window hosts: Wayland, X11, Win32 |

## Rendering

A frame is: input → signal handlers → incremental layout (only nodes whose
size request changed are measured again; nodes that only move are not
repainted) → damage → the raster redraws the damaged rectangles in
z-order, clipping to them → the host attaches the buffer (Wayland: the
buffer that was already on screen is repaired from the previous frame's
damage, so a hover repaints a few hundred pixels, not the window).

Materials (Canvas, Chrome, Layer, Floating…) resolve as in Awra 1: tint
and alpha per appearance, grain when visible, an opaque fallback when the
compositor does not blur. The compositor blurs behind the window's rounded
shape (`ext-background-effect-v1` or KDE blur on Wayland, the KDE blur
property on X11, DWM's backdrop on Windows).

## Text

Text follows what GTK did for Awra 1:

- at scale 1, Awra 1 set GTK's manual font rendering with hinted metrics:
  whole-pixel advances and rounded glyph positions; on dense displays free
  placement;
- the desktop's font (gtk-font-name / kdeglobals on Linux, the message font
  on Windows), resolved through fontconfig (or GDI) with GTK's weight
  matching; bold is synthesized only where fontconfig's 90-synthetic.conf
  asks for it;
- labels measure as GtkLabel (approximate char width for width-chars,
  widest line at max-width-chars, width-for-height when allocated with an
  alignment), and lines break as Pango breaks them.

## Windows

Awra 1's win32 backend is reproduced: a borderless window (the native
frame's styles kept for snap and animations, `WM_NCCALCSIZE` taking the
frame away) whose client area is a sheet of glass; DWM draws the Acrylic
backdrop (Windows 11 22H2+) or the accent blur (Windows 10), rounds the
corners and draws the shadow; resizing goes through a native border band.
Physical keys come from scan codes (as evdev codes, the names KeyToMusic
binds), key labels from the layout, file drops through OLE (hover and drop).

## Rust

`awra-sys` is generated from `include/awra/awra.h` by
`tools/gen_rust_sys.py`. `awra` wraps it with gtk-rs names so Awra 1
applications port by replacing `gtk::` with `awra::`: widget handles are
reference counted (`Clone` refs, `Drop` unrefs), weak references follow the
widget's life, closures connect to signals, `win.*` actions are activated by
buttons, menus, the command palette and accelerators, and futures run on the
UI thread (`glib::spawn_future_local`).
