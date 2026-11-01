import { Rng, TAU } from '../engine/core';
import type { Spawn, Wave } from './stage';

/** Helpers to write waves compactly. Positions are in arena units (0,0 = centre). */
export const F = {
  ring(kind: string, n: number, r: number, at = 0, offset = 0, elite = false): Spawn[] {
    return Array.from({ length: n }, (_, i) => ({ at, kind, x: Math.cos(offset + (i / n) * TAU) * r, y: Math.sin(offset + (i / n) * TAU) * r, elite }));
  },
  line(kind: string, n: number, x1: number, y1: number, x2: number, y2: number, at = 0, step = 0): Spawn[] {
    return Array.from({ length: n }, (_, i) => {
      const t = n === 1 ? 0.5 : i / (n - 1);
      return { at: at + i * step, kind, x: x1 + (x2 - x1) * t, y: y1 + (y2 - y1) * t };
    });
  },
  at(kind: string, x: number, y: number, at = 0, elite = false): Spawn[] {
    return [{ at, kind, x, y, elite }];
  },
  /** n foes spread around the edge, staggered by `step` beats. */
  edge(kind: string, n: number, r: number, at = 0, step = 0.5, seed = 1, elite = false): Spawn[] {
    const rng = new Rng(seed);
    const a0 = rng.range(0, TAU);
    return Array.from({ length: n }, (_, i) => {
      const a = a0 + (i / n) * TAU + rng.range(-0.2, 0.2);
      return { at: at + i * step, kind, x: Math.cos(a) * r, y: Math.sin(a) * r, elite };
    });
  },
  /** A tight cluster. */
  pack(kind: string, n: number, x: number, y: number, spread = 2, at = 0, seed = 1): Spawn[] {
    const rng = new Rng(seed);
    return Array.from({ length: n }, () => ({ at, kind, x: x + rng.range(-spread, spread), y: y + rng.range(-spread, spread) }));
  },
};

export function wave(...parts: Spawn[][]): Wave {
  return parts.flat();
}
