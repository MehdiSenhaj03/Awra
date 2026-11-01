import { TAU } from '../../engine/core';
import { BK } from '../bullets';
import { Mote } from '../enemies';
import { Hazard } from '../hazard';
import { C, fan, ring } from '../patterns';
import type { World } from '../world';
import { Boss } from './base';

/**
 * OSTINATO, Conductor of the Threshold: a crown of rings around a prism heart. Everything it plays repeats —
 * learn the loop, find the gap. Its rings align every other bar: that is when the heart is open.
 */
export class Ostinato extends Boss {
  kind = 'ostinato';
  look = 'ostinato';
  name = 'ostinato';
  spiral = 0;
  /** 0..1: how aligned the rings are (render, and damage taken). */
  align = 0;
  private beams: Hazard[] = [];
  private gap = 0;

  constructor(w: World) {
    super(0, -6, 2000, 2.6);
    this.color = C.magenta;
    void w;
  }

  move(w: World, k: number): void {
    const t = w.beat.beat * (Math.PI / 16);
    const tx = Math.sin(t) * 5;
    const ty = -6 + Math.cos(t * 2) * 2;
    this.vx += (tx - this.x) * 0.02 * k;
    this.vy += (ty - this.y) * 0.02 * k;
    this.vx *= 0.9;
    this.vy *= 0.9;
    const open = Math.floor(w.beat.beat / 4) % 2 === 1 && this.state === 'fight';
    this.align += ((open ? 1 : 0) - this.align) * 0.1;
    this.armor = 0.5 + this.align * 0.9;
    this.spin += 0.01 + (1 - this.align) * 0.02;
    // Beams follow the crown.
    this.beams.forEach((h, i) => {
      const a = this.spin * 1.4 + (i / this.beams.length) * TAU;
      h.x = this.x + Math.cos(a) * (this.r + 0.5);
      h.y = this.y + Math.sin(a) * (this.r + 0.5);
      h.x2 = this.x + Math.cos(a) * 26;
      h.y2 = this.y + Math.sin(a) * 26;
    });
  }

  onPhase(w: World, phase: number): void {
    if (phase === 2) {
      for (let i = 0; i < 2; i++) {
        const h = new Hazard('beam', this.x, this.y, this.x, this.y, 0.3, Math.round(w.beat.spb * 60 * 4), C.magenta, 1e9);
        this.beams.push(h);
        w.addHazard(h);
      }
    }
    if (phase === 3) {
      this.beams = [];
    }
  }

  attack(w: World, phase: number, bar: number, step: number): void {
    const x = this.x;
    const y = this.y;
    const aim = this.angleTo(w);
    if (phase === 1) {
      if (step % 4 === 0) {
        ring(w, x, y, 14, 7, this.spiral, { color: C.magenta });
        this.spiral += 0.14;
        w.sfx('enemyShot', 0.7, 0.4);
      }
      if (bar % 2 === 1 && step === 8) fan(w, x, y, aim, 3, 0.5, 9.5, { color: C.white, kind: BK.Big, r: 0.5 });
      if (bar % 4 === 3 && step % 2 === 0) {
        for (let i = 0; i < 4; i++) {
          const a = (i / 4) * TAU + bar * 0.4 + step * 0.06;
          w.bullets.add(x + Math.cos(a) * this.r, y + Math.sin(a) * this.r, a, 12, { color: C.violet, kind: BK.Shard, r: 0.28 });
        }
      }
    } else if (phase === 2) {
      if (step % 4 === 0) {
        ring(w, x, y, 10, 6.5, this.spiral, { color: C.magenta });
        ring(w, x, y, 10, 6.5, -this.spiral * 1.3, { color: C.violet });
        this.spiral += 0.18;
      }
      if (step === 0 || step === 6 || step === 12) fan(w, x, y, aim, 5, 0.7, 10, { color: C.white, kind: BK.Big, r: 0.45 });
    } else {
      // The loop with a gap: rings on eighths, a hole that drifts — find it, stay in it.
      if (step % 2 === 0) {
        const n = 28;
        for (let i = 0; i < n; i++) {
          const a = (i / n) * TAU;
          const d = Math.abs(((a - this.gap + Math.PI * 3) % TAU) - Math.PI);
          if (d < 0.55) continue;
          w.bullets.add(x, y, a, 6, { color: i % 2 ? C.magenta : C.violet, r: 0.3 });
        }
        this.gap += 0.09 * (bar % 4 < 2 ? 1 : -1);
      }
      // The motif: the same rhythm every bar.
      if ([0, 3, 6, 10, 12].includes(step)) fan(w, x, y, aim, 2, 0.12, 13, { color: C.white, kind: BK.Shard, r: 0.3 });
      if (bar % 2 === 0 && step === 0) {
        for (let i = 0; i < 4; i++) {
          const e = w.arena.edge((i / 4) * TAU + bar, 3);
          const m = w.add(new Mote(e.x, e.y));
          m.spawnT = 40;
          w.emit({ t: 'spawn', x: e.x, y: e.y, color: m.color, size: m.r });
        }
      }
    }
  }
}
