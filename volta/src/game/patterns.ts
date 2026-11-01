import { TAU } from '../engine/core';
import { BK } from './bullets';
import type { World } from './world';

export interface BOpt {
  r?: number;
  color?: number;
  kind?: BK;
  life?: number;
  acc?: number;
  turn?: number;
  wait?: number;
}

/** n bullets spread over `spread` radians around angle a. */
export function fan(w: World, x: number, y: number, a: number, n: number, spread: number, speed: number, o: BOpt = {}): void {
  for (let i = 0; i < n; i++) {
    const t = n === 1 ? 0 : i / (n - 1) - 0.5;
    w.bullets.add(x, y, a + t * spread, speed, o);
  }
}

/** A full ring of n bullets. */
export function ring(w: World, x: number, y: number, n: number, speed: number, offset = 0, o: BOpt = {}): void {
  for (let i = 0; i < n; i++) w.bullets.add(x, y, offset + (i / n) * TAU, speed, o);
}

/** A line of bullets at increasing speeds (a "stream" that arrives as a column). */
export function column(w: World, x: number, y: number, a: number, n: number, speed: number, step: number, o: BOpt = {}): void {
  for (let i = 0; i < n; i++) w.bullets.add(x, y, a, speed + i * step, o);
}

export const C = {
  magenta: 0xff2fb9,
  cyan: 0x2ef2ff,
  gold: 0xffc34a,
  violet: 0x9b5cff,
  red: 0xff4060,
  white: 0xfff4e8,
  green: 0x40ffb0,
  orange: 0xff7a2f,
};

export { BK };
