<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Compositor support matrix

Awra selects behavior from GDK session type and advertised protocol
capabilities. Application code must never branch on a compositor name.

| Environment | Automated coverage | Expected behavior |
| --- | --- | --- |
| Any GTK backend, forced generic | every CI run | opaque chromatically matched fallback |
| X11/Xvfb | regular CI | generic fallback, no native request |
| Wayland without `ext-background-effect-v1` | headless external CI | generic fallback, no warning |
| Wayland advertising the interface without BLUR | backend capability test | fallback, dynamically observable |
| KWin with built-in `blur` | manual release gate | native Canvas and popup requests applied |
| KWin with Better Blur DX | manual comparison gate | same protocol request; compositor controls result |
| KWin with blur disabled | manual release gate | immediate opaque fallback |

The manual `tests/manual/kwin-effect-proof.sh` snapshots the initial built-in
and Better Blur DX effect state, tests built-in-only and no-blur cases, and
restores the exact initial state through an EXIT trap. Fractional scaling is
reviewed at 125%, 150% and 175% because the protocol region is integer-valued.

The public diagnostics must agree with every run: interface announced,
capability, resolved request, applied count, region geometry and fallback
reason. A compositor can choose blur radius, diffusion and saturation; these
are intentionally absent from Awra's API and test expectations.
