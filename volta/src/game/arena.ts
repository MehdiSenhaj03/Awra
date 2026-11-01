/** The playing field: a circle or a rounded rectangle, with optional pillars that stop bullets. */
export interface ArenaDef {
  shape: 'circle' | 'rect';
  /** Circle radius, or rectangle half-size. */
  r?: number;
  w?: number;
  h?: number;
  pillars?: [number, number, number][];
}

export class Arena {
  shape: 'circle' | 'rect';
  r: number;
  w: number;
  h: number;
  pillars: { x: number; y: number; r: number }[];

  constructor(d: ArenaDef) {
    this.shape = d.shape;
    this.r = d.r ?? 20;
    this.w = d.w ?? 22;
    this.h = d.h ?? 14;
    this.pillars = (d.pillars ?? []).map(([x, y, r]) => ({ x, y, r }));
  }

  /** Largest extent, for cameras and spawns. */
  get extent(): number {
    return this.shape === 'circle' ? this.r : Math.max(this.w, this.h);
  }

  inside(x: number, y: number, margin = 0): boolean {
    if (this.shape === 'circle') return x * x + y * y <= (this.r - margin) ** 2;
    return Math.abs(x) <= this.w - margin && Math.abs(y) <= this.h - margin;
  }

  /** Keeps a disc of radius rad inside the arena and out of pillars. */
  confine(p: { x: number; y: number }, rad: number): boolean {
    let hit = false;
    if (this.shape === 'circle') {
      const d = Math.hypot(p.x, p.y);
      const m = this.r - rad;
      if (d > m) {
        p.x *= m / d;
        p.y *= m / d;
        hit = true;
      }
    } else {
      const cx = Math.max(-this.w + rad, Math.min(this.w - rad, p.x));
      const cy = Math.max(-this.h + rad, Math.min(this.h - rad, p.y));
      hit = cx !== p.x || cy !== p.y;
      p.x = cx;
      p.y = cy;
    }
    for (const q of this.pillars) {
      const dx = p.x - q.x;
      const dy = p.y - q.y;
      const d = Math.hypot(dx, dy);
      const m = q.r + rad;
      if (d < m && d > 1e-6) {
        p.x = q.x + (dx / d) * m;
        p.y = q.y + (dy / d) * m;
        hit = true;
      }
    }
    return hit;
  }

  blocked(x: number, y: number): boolean {
    for (const q of this.pillars) if ((x - q.x) ** 2 + (y - q.y) ** 2 < q.r * q.r) return true;
    return false;
  }

  /** A point on the inner edge at angle a (spawn positions). */
  edge(a: number, inset = 2): { x: number; y: number } {
    if (this.shape === 'circle') return { x: Math.cos(a) * (this.r - inset), y: Math.sin(a) * (this.r - inset) };
    const c = Math.cos(a);
    const s = Math.sin(a);
    const k = Math.min((this.w - inset) / Math.max(1e-6, Math.abs(c)), (this.h - inset) / Math.max(1e-6, Math.abs(s)));
    return { x: c * k, y: s * k };
  }
}
