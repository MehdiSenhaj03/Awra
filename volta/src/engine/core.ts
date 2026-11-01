/** Shared constants and small helpers (no rendering, no DOM). */

export const HZ = 60;
export const DT = 1 / HZ;

export const clamp = (v: number, a: number, b: number) => (v < a ? a : v > b ? b : v);
export const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
export const smooth = (t: number) => t * t * (3 - 2 * t);
/** Frame-rate independent damping factor for x += (target - x) * damp(k). */
export const damp = (k: number, dt = DT) => 1 - Math.exp(-k * dt);
export const TAU = Math.PI * 2;
export const angleDiff = (a: number, b: number) => {
  let d = (b - a) % TAU;
  if (d > Math.PI) d -= TAU;
  if (d < -Math.PI) d += TAU;
  return d;
};

/** Small deterministic PRNG (mulberry32). */
export class Rng {
  constructor(public s = 1) {}
  next(): number {
    let t = (this.s = (this.s + 0x6d2b79f5) | 0);
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }
  range(a: number, b: number): number {
    return a + (b - a) * this.next();
  }
  int(a: number, b: number): number {
    return Math.floor(this.range(a, b + 1));
  }
  chance(p: number): boolean {
    return this.next() < p;
  }
  pick<T>(a: readonly T[]): T {
    return a[Math.floor(this.next() * a.length)];
  }
  sign(): number {
    return this.next() < 0.5 ? -1 : 1;
  }
}

/** A non-deterministic generator for purely visual randomness. */
export const fx = new Rng((Math.random() * 2 ** 31) | 0);

export interface V2 {
  x: number;
  y: number;
}
export const len = (x: number, y: number) => Math.sqrt(x * x + y * y);
export const dist = (a: V2, b: V2) => len(a.x - b.x, a.y - b.y);
