import { TAU } from '../../engine/core';
import { BK } from '../bullets';
import { Enemy } from '../enemy';
import { Hazard } from '../hazard';
import { C, fan, ring } from '../patterns';
import type { World } from '../world';
import { Boss } from './base';

/** One ring of the serpent's body. Hits are passed to the head at a discount. */
export class Segment extends Enemy {
  kind = 'segment';
  look = 'segment';
  constructor(public head: Rubato, public index: number) {
    super(head.x, head.y);
    this.r = 1.15 - index * 0.04;
    this.hp = this.maxHp = 1e9;
    this.color = C.violet;
    this.heavy = true;
    this.spawnT = 0;
    this.value = 0;
    this.notes = 0;
    this.contact = true;
  }
  update(): void {
    const h = this.head;
    const p = h.trail[Math.min(h.trail.length - 1, (this.index + 1) * 7)];
    if (p) {
      this.vx = (p.x - this.x) * 60;
      this.vy = (p.y - this.y) * 60;
      this.x = p.x - this.vx / 60;
      this.y = p.y - this.vy / 60;
    }
    this.vis = h.vis;
    this.hurtable = this.targetable = h.vis > 0.5 && h.hurtable;
    this.contact = h.vis > 0.5;
    this.dead = h.dead;
  }
  hit(w: World, dmg: number, fx: number, fy: number): boolean {
    if (!this.hurtable) return false;
    this.flash = 1;
    w.emit({ t: 'hit', x: this.x, y: this.y, color: this.color, big: false });
    return this.head.hit(w, dmg * 0.45, fx, fy);
  }
}

/**
 * RUBATO, Conductor of the Mercury Ocean: a serpent of liquid chrome that steals time. Each phase changes the
 * tempo (slower, then faster); it dives and resurfaces where its wake glows.
 */
export class Rubato extends Boss {
  kind = 'rubato';
  look = 'rubato';
  name = 'rubato';
  trail: { x: number; y: number }[] = [];
  segments: Segment[] = [];
  private path = 0;
  private dive = 0;
  private surface: Hazard | null = null;

  constructor(w: World) {
    super(0, -10, 2400, 1.5);
    this.color = C.violet;
    for (let i = 0; i < 14; i++) this.segments.push(w.add(new Segment(this, i)));
  }

  move(w: World, k: number): void {
    this.path += 0.011 * k * (w.beat.bpm / 118);
    const a = this.path;
    let tx = Math.sin(a) * 14;
    let ty = Math.sin(a * 2) * 8 - 2;
    if (this.dive > 0) {
      this.vis += (0 - this.vis) * 0.15;
      this.hurtable = this.targetable = false;
      if (this.surface) {
        tx = this.surface.x;
        ty = this.surface.y;
      }
    } else {
      this.vis += (1 - this.vis) * 0.15;
      if (this.state === 'fight') this.hurtable = this.targetable = true;
    }
    this.vx += (tx - this.x) * 0.06 * k;
    this.vy += (ty - this.y) * 0.06 * k;
    this.vx *= 0.85;
    this.vy *= 0.85;
    this.rot = Math.atan2(this.vy, this.vx);
    this.contact = this.vis > 0.5;
    this.trail.unshift({ x: this.x, y: this.y });
    if (this.trail.length > 120) this.trail.pop();
  }

  onPhase(w: World, phase: number): void {
    // Rubato: the tempo bends.
    w.beat.setBpm(phase === 2 ? 96 : 146);
  }

  attack(w: World, phase: number, bar: number, step: number): void {
    const x = this.x;
    const y = this.y;
    const aim = this.angleTo(w);
    if (this.dive > 0) {
      if (step === 0 && --this.dive === 0) {
        ring(w, x, y, 24, 7, 0, { color: C.violet, r: 0.32 });
        ring(w, x, y, 24, 5, TAU / 48, { color: C.white, r: 0.28 });
        w.emit({ t: 'boom', x, y, r: 4, color: C.violet });
        w.emit({ t: 'shake', a: 0.5 });
        w.sfx('bigBoom');
        if (this.surface) this.surface.done = true;
        this.surface = null;
      }
      return;
    }
    if (step % 4 === 0) fan(w, x, y, aim, phase === 3 ? 5 : 3, 0.4, 10, { color: C.white, kind: BK.Shard, r: 0.3 });
    // A ripple runs down the body every bar.
    if (step % 2 === 0) {
      const s = this.segments[(step / 2 + bar * 3) % this.segments.length];
      if (s) ring(w, s.x, s.y, phase === 1 ? 4 : 6, 4.5, step * 0.3, { color: C.violet, r: 0.28, turn: phase === 3 ? (step % 4 ? 0.6 : -0.6) : 0 });
    }
    if (phase >= 2 && bar % 4 === 3 && step === 0) {
      // Dive: resurfaces at the glowing spot a bar later.
      const p = w.player;
      this.surface = new Hazard('zone', p.x + p.vx * 0.5, p.y + p.vy * 0.5, x, y, 4, Math.round(w.beat.spb * 60 * 4), C.violet, 0);
      w.addHazard(this.surface);
      this.dive = 1;
      w.sfx('phase');
    }
    if (phase >= 2 && step === 8) for (const s of this.segments.filter((_, i) => i % 3 === bar % 3)) w.bullets.add(s.x, s.y, w.rng.range(0, TAU), 0.5, { color: C.violet, wait: 1.2, r: 0.36, life: 6 });
  }

  onDeath(w: World): void {
    w.beat.setBpm(118);
    for (const s of this.segments) w.emit({ t: 'boom', x: s.x, y: s.y, r: 2, color: C.violet });
  }
}
