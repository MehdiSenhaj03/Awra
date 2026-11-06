<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Timelines and interval editing

`AwraTimeline` is a backend-independent time ruler for seeking and editing
interval offsets. It does not parse subtitles, decode media or store application
history. Media players, editors, annotation tools, audio
workstations and chapter browsers can use the same API.

```c
GtkWidget *timeline = awra_timeline_new ();
awra_timeline_set_duration (AWRA_TIMELINE (timeline), 120.0);
awra_timeline_add_interval (AWRA_TIMELINE (timeline), 10.0, 14.0, "A cue");
awra_timeline_add_interval (AWRA_TIMELINE (timeline), 45.0, 45.0, "Bookmark");
awra_timeline_set_position (AWRA_TIMELINE (timeline), 12.0);
```

Listen to `seek-requested` to drive a player; updating `position` never requests
a seek. The view can display the complete duration (`view-duration=0`) or a
moving viewport (`view-start` and `view-duration`). Point intervals draw markers.
Labels are plain text, ellipsized inside the clipped interval. Replacing a
collection uses `clear_intervals()` followed by `add_interval()`.

When a player accepts a seek, update `position` in the `seek-requested` handler
before returning. The drag preview ends with that handler; waiting for a later
playback poll would briefly restore the previous cursor. Subsequent playback
updates take over from the committed position. Rejected requests can leave
`position` unchanged. Playback confirmation and error handling stay with the
application, including any asynchronous backend state.

For synchronization, set `editable=TRUE`. Dragging translates all intervals
without changing their source times. `notify::offset` drives live preview and
`offset-committed` marks a finished edit. Cancelling a gesture restores its
original offset. Left/Right adjusts the offset by 100 ms (Shift: one second).
In seek mode, the same keys request one-second steps (Shift: ten seconds), and
Home/End request the start/end. Horizontal movement follows RTL direction.

Applications can set `step-increment` and `page-increment` to use the same
arrow-key steps inside the timeline as their global transport shortcuts. For
example, a player can choose five and thirty seconds. Changing either property
does not move the cursor or request a seek; defaults remain one and ten seconds.
The edit-mode displacement remains one tenth of the configured step.

An asynchronous player should retain its latest requested position separately
from its decoder clock. Keep the cursor at that target until the backend has
confirmed the transition, and accumulate repeated shortcuts from that target.
Network downloads, request coalescing and playback confirmation belong to the
player backend; the timeline does not own a playback timer or decoder.

`hover-time` is read-only, in seconds, and is -1 outside the view. Consumers can
use it to provide a decoder-backed preview without putting decoding into Awra.
The widget owns no timeout or playback thread. The framework's token snapshot
supplies its track, selected cue, foreground, focus ring and disabled colors.
It uses no animation, so reduced motion is inherently respected. It creates no
translucent surface and follows the parent material's accessibility fallback.

The slider accessible role implements `GtkAccessibleRange`: assistive
technology can request a seek or commit an offset. Bounds and value text track
the active mode. The C API, GtkBuilder properties, GIR, Python/GI and generated
Rust projection are exercised by the component and external consumer tests.

For virtualized media collections, use the public `collection-view` style role
on `GtkGridView` or `GtkListView`. GTK owns the model, factory, selection and
recycling; Awra supplies a neutral plane, cell spacing and focus states.

## Floating controls and hover previews

`AwraOverlayBar` allocates its content to the complete canvas. The floating bar
has its own material, token inset, maximum width, idle timer and motion policy.
Its vertical size never contributes to the canvas allocation. Its horizontal
minimum remains respected so controls cannot silently become unreachable.
Hovering controls, keyboard focus, editable fields and explicit inhibition keep
the bar available. Unmapping stops its timer. Changing `revealed` does not
resize the content. A bar can be moved between a dock and overlay using the
public nullable setters, retaining an owned reference during reparenting.

```python
overlay = Awra.OverlayBar.new()
overlay.set_content(canvas)
overlay.set_bar(controls)
overlay.set_auto_hide(True)
overlay.set_inhibit_auto_hide(inspector_open)
```

A timeline owns the preview popup and its pointer anchoring, including RTL,
dragging, cancellation and unmapping. Supply only backend-owned content with
`timeline.set_preview(widget)`, then respond to `notify::hover-time`. A negative
time means the pointer has left the ruler. Decode asynchronously and replace
stale frames immediately; do not start a process from each motion callback.
`get_hover_x()` provides logical pointer coordinates when needed. The preview
is noninteractive and never steals keyboard focus. `get_preview_popover()`
allows material/accessibility configuration through public APIs.

`AwraWindow.set_overlay_chrome(True)` must first be enabled before presentation.
After this opt-in, dock/overlay transitions preserve the native CSD surface.
`get_chrome()` remains the public visible header accessor. A full-screen media
canvas can therefore extend beneath both header and controls. Keep interactive
inspector content outside their measured regions, using scrollable panels.
`AwraToastOverlay.bottom-inset` reserves clearance for messages above a floating
bar without changing the content allocation (default 24 logical pixels).

## Frosted controls over application content

Native compositor blur samples behind the window. For an overlay over video,
images or another application subtree, set an explicit local source instead:

```python
controls.get_surface().set_backdrop_widget(video_picture)
header.get_surface().set_backdrop_widget(video_picture)
header.set_max_width(1200)  # optional; zero keeps the normal full width
```

The source must be an independent widget in the same native surface; neither
widget may contain the other. It is held weakly and contributes no size request.
For `GtkPicture`, Awra observes and samples the image paintable directly,
preserving content-fit and avoiding an extra cache of the entire video widget.
Other widgets supply their cached GTK render node without CPU readback. The blur
input is clipped to the overlay plus kernel padding, and rounded clipping keeps
corners clean. Dark glass attenuates luminous content to preserve text contrast.

The source's invalidations redraw the surface. A missing, unmapped or destroyed
source uses the opaque fallback. High contrast and reduced transparency also
use that fallback. Clear a header's backdrop when switching back to a docked
window canvas. A constrained header retains its content minimum and lets
pointer events outside its visible width pass through to the content.
