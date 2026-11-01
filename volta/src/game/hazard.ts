import { TAU } from '../engine/core';
import { BK } from './bullets';
import type { World } from './world';

/** Point to segment distance. */
export function segDist(px: number, py: number, x1: number, y1: number, x2: number, y2: number): number {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const l2 = dx * dx + dy * dy || 1;
  const t = Math.max(0, Math.min(1, ((px - x1) * dx + (py - y1) * dy) / l2));
  return Math.hypot(px - (x1 + dx * t), py - (y1 + dy * t));
}

/**
 * Telegraphs and area attacks the renderer draws: warning lines, laser beams, landing zones.
 * `warn` hazards only show; active ones hurt.
 */
export class Hazard {
  id = 0;
  t = 0;
  done = false;
  /** 0..1 for telegraph fill. */
  get k(): number {
    return Math.min(1, this.t / Math.max(1, this.delay));
  }

  constructor(
    public kind: 'line' | 'zone' | 'beam',
    public x: number,
    public y: number,
    public x2: number,
    public y2: number,
    public r: number,
    /** Frames before it resolves (zones) or expires (lines). */
    public delay: number,
    public color = 0xff2fb9,
    /** Zone: radial bullets on impact. Beam: lifetime after delay. */
    public n = 0,
  ) {}

  update(w: World): void {
    this.t += w.k;
    const p = w.player;
    if (this.kind === 'zone') {
      if (this.t >= this.delay) {
        this.done = true;
        w.emit({ t: 'boom', x: this.x, y: this.y, r: this.r, color: this.color });
        w.emit({ t: 'shake', a: 0.25 });
        w.sfx('mortar');
        if (Math.hypot(p.x - this.x, p.y - this.y) < this.r + 0.2) w.hurtPlayer(this.x, this.y);
        for (let i = 0; i < this.n; i++) w.bullets.add(this.x, this.y, (i / this.n) * TAU, 7, { color: this.color, r: 0.3 });
      }
    } else if (this.kind === 'line') {
      if (this.t >= this.delay) this.done = true;
    } else if (this.kind === 'beam') {
      // Warns during `delay`, then burns for n frames.
      if (this.t >= this.delay && segDist(p.x, p.y, this.x, this.y, this.x2, this.y2) < this.r + 0.3) w.hurtPlayer(this.x, this.y);
      if (this.t >= this.delay + this.n) this.done = true;
    }
  }

  get live(): boolean {
    return this.kind === 'beam' && this.t >= this.delay;
  }
}

export const ZONE_BULLET = BK.Orb;
