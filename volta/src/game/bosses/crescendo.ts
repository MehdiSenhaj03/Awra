import { TAU } from '../../engine/core';
import { BK } from '../bullets';
import { Enemy } from '../enemy';
import { Hazard } from '../hazard';
import { C, fan, ring } from '../patterns';
import type { World } from '../world';
import { Boss } from './base';

/** A crystal grown by Crescendo: it rings louder every bar and shatters into a ring when it is full. */
export class Crystal extends Enemy {
  kind = 'crystal';
  look = 'crystal';
  grow = 0;
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.9;
    this.hp = this.maxHp = 8;
    this.value = 60;
    this.notes = 1;
    this.color = C.green;
    this.heavy = true;
    this.contact = true;
  }
  update(w: World): void {
    this.vx = 0;
    this.vy = 0;
    this.grow = Math.min(1, this.t / (w.beat.spb * 60 * 8));
    if (this.grow >= 1 && w.beat.crossed(1) && w.beat.index(1) % 4 === 0) {
      ring(w, this.x, this.y, 16, 3, w.rng.range(0, TAU), { color: C.green, acc: 5, r: 0.3 });
      w.emit({ t: 'boom', x: this.x, y: this.y, r: 2.2, color: C.green });
      w.sfx('boom', 1.3, 0.6);
      this.dead = true;
    }
  }
}

/**
 * CRESCENDO, Conductor of the Glass Garden: a tree of crystal that only knows how to get louder. Its volleys
 * grow with every bar of a phase; its crystals ring until they burst. Shatter them early.
 */
export class Crescendo extends Boss {
  kind = 'crescendo';
  look = 'crescendo';
  name = 'crescendo';
  private branches: Hazard[] = [];
  private curtain = 0;

  constructor(w: World) {
    super(0, -9, 2300, 2.4);
    this.color = C.green;
    void w;
  }

  move(w: World, k: number): void {
    // Rooted: it sways but never walks.
    this.vx = (Math.sin(w.beat.beat * 0.4) * 2 - (this.x - 0)) * 0.8 * k;
    this.vy = (-9 - this.y) * 0.8;
    this.spin += 0.004;
    const grow = this.phase === 2 && this.state === 'fight' ? Math.min(1, ((this.bars(w) % 4) / 4) * 1.2) : 0;
    this.branches.forEach((h, i) => {
      const a = Math.PI / 2 + (i - 1) * 0.65 + Math.sin(w.beat.beat * 0.25 + i) * 0.35;
      const len = 4 + grow * 20;
      h.x = this.x + Math.cos(a) * this.r;
      h.y = this.y + Math.sin(a) * this.r;
      h.x2 = this.x + Math.cos(a) * (this.r + len);
      h.y2 = this.y + Math.sin(a) * (this.r + len);
    });
  }

  onPhase(w: World, phase: number): void {
    if (phase === 2) {
      for (let i = 0; i < 3; i++) {
        const h = new Hazard('beam', this.x, this.y, this.x, this.y, 0.34, Math.round(w.beat.spb * 60 * 2), C.green, 1e9);
        this.branches.push(h);
        w.addHazard(h);
      }
    }
    if (phase === 3) this.branches = [];
  }

  attack(w: World, phase: number, bar: number, step: number): void {
    const x = this.x;
    const y = this.y;
    const aim = this.angleTo(w);
    const swell = bar % 8;
    if (step % 4 === 0) {
      // The swell: more bullets every bar, accelerating.
      const n = 3 + swell + (phase - 1) * 2;
      fan(w, x, y, aim, n, 0.35 + swell * 0.08, 3.5, { color: C.green, acc: 6 + phase * 2, r: 0.3 });
      w.sfx('enemyShot', 0.9 + swell * 0.05, 0.4);
    }
    if (step === 8 && bar % 2 === 0) {
      const count = phase === 1 ? 2 : 3;
      for (let i = 0; i < count; i++) {
        const a = w.rng.range(0, TAU);
        const d = w.rng.range(6, 13);
        const c = w.add(new Crystal(Math.cos(a) * d, Math.sin(a) * d * 0.8));
        c.spawnT = 30;
        w.emit({ t: 'spawn', x: c.x, y: c.y, color: C.green, size: 0.9 });
      }
    }
    if (phase >= 2 && step === 12) ring(w, x, y, 18 + swell * 2, 5, bar * 0.3, { color: C.cyan, r: 0.3, acc: 2 });
    if (phase === 3 && step % 2 === 0) {
      // A falling curtain with a drifting gap.
      const n = 18;
      for (let i = 0; i < n; i++) {
        const px = -18 + (i / (n - 1)) * 36;
        if (Math.abs(px - Math.sin(this.curtain) * 12) < 3.2) continue;
        if (step % 4 === 0) w.bullets.add(px, -19, Math.PI / 2, 7 + swell * 0.4, { color: C.green, kind: BK.Shard, r: 0.26 });
      }
      this.curtain += 0.12;
    }
  }
}
