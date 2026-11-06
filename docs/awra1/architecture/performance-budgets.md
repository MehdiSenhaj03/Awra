<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Rendering and performance budgets

Awra's performance contract is expressed as measurable budgets, not as a
promise that every compositor uses the same blur algorithm.

## Current budgets

| Workload | Reference target | CI regression ceiling |
| --- | ---: | ---: |
| CPU construction of one 1080p page snapshot | 8.3 ms average | 100 ms |
| CPU construction of one 4K page snapshot | 16.7 ms average | 200 ms |
| Grain storage | one 96×96 RGBA texture/process | one texture/process |
| Native region submission with unchanged geometry | 0 updates | 0 updates |
| Skeleton with reduced motion | no tick callback | no tick callback |

The strict timing targets are enabled with `AWRA_PERFORMANCE_STRICT=1` on a
controlled runner. The regular CI ceiling is intentionally wide enough not to
turn virtual-machine scheduling noise into failures; it still catches
accidental orders-of-magnitude regressions. GPU time and compositor blur cost
must be captured externally because `ext-background-effect-v1` does not expose
the compositor's implementation.

## Architectural controls

- `AwraSurface` creates the deterministic grain texture through `g_once` and
  reuses it for every surface and frame.
- Material snapshots use stack values; no per-frame material object is
  created.
- The native coordinator caches width, height and effective radius. Token,
  accent and focus notifications may redraw pixels but cannot resubmit an
  unchanged blur region.
- `AwraDataView` retains GTK's list/column virtualization and selection
  models; Awra does not materialize the complete data set.
- Developer diagnostics expose cumulative effect-region updates, exact
  compositor-neutral region geometry and the number of tracked/applied native
  surfaces. Inspector overdraw tint is explicitly opt-in.

`render-budget` measures a realistic hierarchy of page, cards, rows,
typography and grain after warming caches. The native lifecycle test changes
tokens while blur is active and asserts that the update counter stays fixed.
