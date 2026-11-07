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
| Pango, HarfBuzz, FriBidi, fontconfig | HarfBuzz 14.6 and FriBidi built into BUI (the same on every platform), FreeType (fractional sizes, variable fonts), fontconfig loaded at run time; GTK's rules for weights, synthetic bold, hinted metrics at scale 1; Pango's itemizer, bidi, cursor model and line breaking |
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
  alignment), and lines break as PangoLayout breaks them: Pango 1.58's
  break.c ported (UAX #14 with Pango's tailorings: CJK between ideographs,
  kinsoku, numbers, quotes; UAX #29 graphemes and words; hyphens shown at
  soft hyphens and inside words broken by CHAR wrapping) over GLib's
  Line_Break data, Thai words from libthai when installed (loaded at run
  time; without it Thai breaks only at spaces, as Pango built without it),
  tailored item by item as PangoLayout tailors (items end where the font
  changes); process_item's choice of break (log widths from shaping the
  paragraph once, the last fitting break measured whole, a narrower one
  further on taken); WORD overflows a too-long word, WORD_CHAR breaks it;
  a wrapping label's minimum width is its layout at width 0;
- characters the font lacks fall back, character by character, through
  fontconfig's sorted list for the font (FcFontSort, by charset) on Linux
  and the system's font links (SystemLink, Segoe UI Emoji / Symbol) on
  Windows; spaces and default ignorables take the font of the run they are
  in, as Pango itemizes; a script the user's language doesn't write takes
  the fonts fontconfig sorts for the language Pango derives for it (Arabic:
  ar, Devanagari: hi...); a line is as tall as the fonts drawing it, as
  Pango's line extents are;
- shaping is HarfBuzz's (14.6, as GTK 4.22), item by item with the
  paragraph's text around it as context, as pango_hb_shape (a span inside
  an Arabic word keeps it joined; `BUI_text_set_context`), letter spacing
  added per grapheme with ligatures off and trimmed at line ends as Pango
  does, fallback fonts' baselines aligned to the first item's script
  baseline (apply_baseline_shift): Arabic joining and lam-alef, marks
  (harakat, Hebrew points), Indic, Thai, Hangul, kerning and ligatures;
  items split where Pango's itemizer splits them (bidi level, script with
  paired punctuation, emoji, font). Advances, carets and hit-testing match
  Pango's exactly at scale 1 and 2 (tests compare with Pango 1.58);
- bidi is FriBidi's (UAX #9) as Pango resolves it: a paragraph's direction
  is its first strong character's (else the previous paragraph's, else the
  text's), runs are reordered per line (L2), right-to-left runs mirror
  their brackets; right-to-left paragraphs align right where GTK aligns
  them (labels that wrap or ellipsize, text views; a label places its
  logical extents by xalign as GtkLabel does);
- right to left (`awra_widget_set_direction`, the default from the
  locale as gtk_get_locale_direction): containers mirror their children
  (start is right), text layouts take the widget's direction as Pango's
  base direction, labels and entries flip their xalign, scales and
  progress bars run from the right, switches are on at the left, menus,
  tabs, path bars, column headers and scrolled windows mirror, popovers
  swap start and end, icons are looked up with their -rtl names first, as
  GtkIconTheme does (docs/icons.md);
- editing is GTK's: the strong cursor (pango_layout_get_cursor_pos), Left
  and Right move visually (pango_layout_move_cursor_visually), clicks find
  graphemes as pango_layout_xy_to_index, Backspace deletes a grapheme or,
  where Pango says so (Arabic marks, Indic vowel signs), its last character
  after NFD, Delete a grapheme; selections are painted as each GTK widget
  paints them (labels and text views: each run's range and the line ends
  where the selection continues; entries: one box around a bidi range);
- tabs advance to Pango's default stops (shape_tab without a tab array):
  every eight spaces of the layout's font, the next stop at least a space
  past where the line has got to; each tab is a run of its own, in line
  breaking, measuring and drawing alike;
- ellipsizing is Pango's: the gap grows span by span from the mode's
  centre over the shaped clusters in logical order, the ellipsis takes the
  font at the gap and the lowest level it hides and fills the width, and
  the kept text is drawn from the original shaping (joined forms kept);
- emoji follow Pango: its emoji presentation scanner splits text into emoji
  and text runs, emoji runs take the "emoji" family (a colour font when a
  VS16 asks for one), and sequences (ZWJ, skin tones, flags, keycaps, tags)
  are shaped by HarfBuzz into one glyph. Colour glyphs are CBDT /
  sbix bitmaps (their PNGs decoded by the kit's Wuffs) scaled to the text
  size, or COLR layers blended by FreeType; they are cached like any glyph
  and drawn over the run's coverage mask with the text's alpha.

## Windows

Awra 1's win32 backend is reproduced: a borderless window (the native
frame's styles kept for snap and animations, `WM_NCCALCSIZE` taking the
frame away) whose client area is a sheet of glass; DWM draws the Acrylic
backdrop (Windows 11 22H2+) or the accent blur (Windows 10), rounds the
corners and draws the shadow; resizing goes through a native border band.
Physical keys come from scan codes (as evdev codes, the names KeyToMusic
binds), key labels from the layout, file drops through OLE (hover and drop).

## Popups

GTK 4 gives each popover, menu and tooltip a native surface of its own,
placed by GDK's popup layout; Awra 2 does the same. A popup class places
itself with a layout (`BKPopupLayout`: an anchor rectangle in its window's
coordinates, the gravity on it and on the popup, an offset, and whether it
may flip, slide or shrink — GtkPopover's `create_popup_layout`, GTK's
`gtk_tooltip_position` for tooltips), and the window host resolves it:

- Wayland: an `xdg_popup` whose positioner is built as GDK's (anchor in the
  parent's geometry, reactive), nested under the popup it opened from,
  grabbed with the last press's serial when it hides on outside clicks;
  the compositor places it and `popup_done` closes it;
- X11: an override-redirect ARGB window placed in root coordinates by
  GDK's layout helper against the work area of the anchor's monitor
  (`_GTK_WORKAREAS_Dn`, `_NET_WORKAREA`, RandR), moved with its window,
  pointer and keyboard grabbed with owner events;
- Windows: a `WS_POPUP` tool window owned by its window, never activated,
  placed against the monitor's work area; DWM rounds, shadows and blurs it
  as Awra 1's popovers there; it closes when its window is deactivated.

Without a compositor (an X11 visual without alpha) popups are opaque and
unshadowed, as GDK makes them. Hosts also tell the monitor a window is on
(`BKPopupHost.monitor`, `awra_widget_get_monitor_geometry`, as
gdk_display_get_monitor_at_surface): Awra 1's menus scroll only past its
height less 64 px. Tooltips belong to the deepest widget under the
pointer or its nearest ancestor with one, as GTK asks them.

Popups keep window coordinates, so hit testing, focus, menus' keys and the
accessibility tree are unchanged; each draws into its own buffer with
`BUI_raster_set_origin` and has its own damage (a hover in a menu repaints
that menu's row and nothing of the window). Hosts route a popup's input
back in window coordinates, and leaving the window for one of its popups
is no leave. Offscreen windows (tests, `awra-shot`) draw popups inside the
window, constrained by it; `BUI_POPUP_SURFACES=0` does it everywhere.

## Focus

The ring follows GtkWindow's focus-visible rules: a key press remembers
the focus (none while the ring shows) and its release shows the ring when
the focus is another; Alt alone shows it; it goes about three seconds
later (`awra_timeout_add_seconds`, g_timeout_add_seconds's timing). The
application's capture handlers and shortcuts run before this tracking, as
GTK's controller, added first, runs last: a press they take is not seen,
so its release shows the ring. Widgets that stand for many GTK children
(a collection's rows) say when a key kept the focus on the same child
(their key handler returns 2): a collection without a cursor has its first
row focused, as GtkListBase gives its first child the focus.

## Data transfer

The clipboard carries any type (BK_window_clipboard_*): text under every
name applications ask for it, images as PNG (encoded by Awra; on Windows
also CF_DIBV5 / CF_DIB), files as text/uri-list (GNOME's copied-files list
on Linux, CF_HDROP on Windows). Wayland serves the data source from memory
and reads offers through pipes; X11 owns the selections with a hidden window
(large data through INCR both ways); Windows uses the clipboard formats
directly. Drags out of the application are the system's: Wayland's
wl_data_source + start_drag with an icon surface, X11's XDND (source and
target, with an override-redirect image window), OLE's DoDragDrop with an
IDataObject and the shell's drag image helper. Inside the window a drag's
image (a widget snapshot, as GtkWidgetPaintable) is drawn above everything
and follows the pointer; the Rust DragSource hands a drag of text or files
to the system when the pointer leaves the window.

## Rust

`awra-sys` is generated from `include/awra/awra.h` by
`tools/gen_rust_sys.py`. `awra` wraps it with gtk-rs names so Awra 1
applications port by replacing `gtk::` with `awra::`: widget handles are
reference counted (`Clone` refs, `Drop` unrefs), weak references follow the
widget's life, closures connect to signals, `win.*` actions are activated by
buttons, menus, the command palette and accelerators, and futures run on the
UI thread (`glib::spawn_future_local`).

## Accessibility

GTK gave Awra 1 applications an accessibility tree; Awra 2 has its own,
built the same way (`src/a11y.c`, `include/awra/awra.h`, Rust
`awra::accessible`):

- every widget has a role from its class (GTK's and Awra 1's assignments),
  a name and a description computed with GTK's rules (an explicit label,
  labelled-by, the widget's own text, its children's for roles named from
  content, its tooltip), states (focused, checked, pressed, selected,
  expanded, disabled, busy…) and values; components describe themselves
  with `AwraClass.a11y`, containers their children with `a11y_child`, and
  widgets that draw items (tabs, crumbs, emoji) expose them as virtual
  children (`a11y_items`, `a11y_item`, `a11y_do`);
- applications refine it as with GtkAccessible:
  `awra_accessible_update_property / _state / _relation`, `set_role`,
  `announce` (`update_property(&[Property::Label(..)])` in Rust);
- Linux: AT-SPI2 over a small D-Bus client of Awra's own (no libdbus, no
  GLib), its socket watched by the window loop. Awra asks the session bus,
  without starting anything, whether an assistive technology runs
  (`org.a11y.Status`), and only then joins the accessibility bus;
- Windows: UI Automation providers answered from `WM_GETOBJECT`, with
  `UIAutomationCore.dll` loaded at the first request;
- while nothing listens, the hooks return at once and no widget holds
  accessibility data; when a client listens, widgets it has seen keep a
  snapshot and an idle pass tells it only what changed.

Focus rings follow GTK's focus-visible: they show after keyboard use, hide
on a pointer press and 3 s after the last key.

