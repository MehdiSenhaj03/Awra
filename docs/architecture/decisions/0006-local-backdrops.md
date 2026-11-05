# Local backdrop sources for frosted surfaces

Status: accepted, 2026-10-30.

Window compositor effects cannot blur application content drawn inside that
window. A transparent media overlay previously revealed sharp video, so nested
floating surfaces used an opaque fallback. Consumers need reusable local blur
without duplicating material painting or reading media into CPU pixels.

`AwraSurface` accepts an optional weak widget source. Pictures supply their
image paintable directly, with the same fit geometry; this avoids a second
widget-frame cache for live video. Other widgets use GTK's cached widget
paintable. The source is transformed into the target surface's coordinates. Sampling
is limited to independent subtrees in the same native surface, preventing
ancestry cycles. The blur input is clipped before filtering; only the local
region and kernel padding participate. Material tint, corner grammar and
accessibility fallbacks remain owned by Awra. Source replacement and destruction
release paintable resources and never affect layout measurement.

An explicit source that cannot currently render uses the opaque fallback,
rather than claiming compositor blur can sample it. Native popovers and window
canvas blur retain their existing capability path. No decoder, playback timer,
backend-specific handle or media format enters this API.

`AwraHeader.get_surface()` makes the same capability available to chrome.
Its optional maximum width centres the visible header and its hit region while
retaining the control minimum. Default unbounded headers remain compatible.

Validation covers rendered edge smoothing, live source invalidation, accessibility
fallbacks, weak lifetime, GTK construction, bindings, constrained hit testing and
RTL. A media player exercises the API with live frames and native 4K/60 qualification.
