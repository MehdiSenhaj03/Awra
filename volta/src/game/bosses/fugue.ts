import { TAU } from '../../engine/core';
import { BK } from '../bullets';
import { Hazard } from '../hazard';
import { C, column, fan, ring } from '../patterns';
import type { World } from '../world';
import { Boss } from './base';

/**
 * FUGUE, the twin Conductors of the Foundry. The leader plays; one bar later its twin answers, mirrored.
 * Kill one and the survivor plays both voices, faster.
 */
export class Fugue extends Boss {
  kind = 'fugue';
  look = 'fugue';
  name = 'fugue';
  twin: Fugue | null = null;
  /** -1 left (leader), 1 right (answer). */
  side: number;
  private chain: Hazard | null = null;
  solo = false;

  constructor(w: World, side: number) {
    super(side * 8, -6, 1500, 1.9);
    this.side = side;
    this.color = side < 0 ? C.orange : C.gold;
    void w;
  }

  move(w: World, k: number): void {
    const b = w.beat.beat * (Math.PI / 8);
    let tx = this.side * 8 + Math.sin(b) * 3;
    let ty = -6 + Math.cos(b * 0.5) * 2;
    if (this.phase >= 2 || this.solo) {
      // The twins circle the arena, the chain between them sweeping the floor.
      const a = w.beat.beat * (Math.PI / 16) + (this.side < 0 ? 0 : Math.PI);
      tx = Math.cos(a) * 11;
      ty = Math.sin(a) * 9;
    }
    this.vx += (tx - this.x) * 0.03 * k;
    this.vy += (ty - this.y) * 0.03 * k;
    this.vx *= 0.88;
    this.vy *= 0.88;
    this.rot = this.angleTo(w);
    if (this.chain && this.twin && !this.twin.dead && this.twin.dying === 0) {
      this.chain.x = this.x;
      this.chain.y = this.y;
      this.chain.x2 = this.twin.x;
      this.chain.y2 = this.twin.y;
    } else if (this.chain) {
      this.chain.done = true;
      this.chain = null;
    }
  }

  onPhase(w: World, phase: number): void {
    if (phase === 2 && this.side < 0 && this.twin && !this.twin.dead) {
      this.chain = new Hazard('beam', this.x, this.y, this.x, this.y, 0.26, Math.round(w.beat.spb * 60 * 4), C.gold, 1e9);
      w.addHazard(this.chain);
    }
  }

  /** The leader plays bar n; the answer plays the leader's bar n-1, mirrored. */
  attack(w: World, phase: number, bar: number, step: number): void {
    if (this.side > 0 && !this.solo) {
      if (bar >= 1) this.voice(w, phase, bar - 1, step, true);
      return;
    }
    this.voice(w, phase, bar, step, false);
    if (this.solo && step % 8 === 4) this.voice(w, phase, bar, (step + 8) % 16, true);
  }

  private voice(w: World, phase: number, bar: number, step: number, mirror: boolean): void {
    const x = this.x;
    const y = this.y;
    const p = w.player;
    const aim = this.angleTo(w);
    const m = mirror ? -1 : 1;
    const motif = bar % 4;
    if (motif === 0 && step % 2 === 0) column(w, x, y, aim + m * (step / 16 - 0.5) * 1.2, 3, 8, 1.5, { color: this.color, r: 0.3 });
    if (motif === 1 && step === 0) {
      // The hammer: lands on the next bar.
      const tx = mirror ? -p.x : p.x;
      w.addHazard(new Hazard('zone', tx, p.y, x, y, 3, Math.round(w.beat.spb * 60 * 4), this.color, 12));
      w.sfx('mortarLaunch');
    }
    if (motif === 2 && step % 4 === 0) ring(w, x, y, 12 + phase * 2, 6.5, m * step * 0.13, { color: this.color, r: 0.3 });
    if (motif === 3 && (step === 0 || step === 8)) fan(w, x, y, aim, 7 + phase * 2, 1.4, 9, { color: C.white, kind: BK.Big, r: 0.42 });
    if (phase === 3 && step === 12) for (let i = 0; i < 6; i++) w.bullets.add(x, y, (i / 6) * TAU + bar, 5, { color: this.color, turn: m * 0.8, r: 0.32 });
  }

  onDeath(w: World): void {
    if (this.twin && !this.twin.dead && this.twin.dying === 0) {
      this.twin.solo = true;
      this.twin.hp = Math.min(this.twin.maxHp, this.twin.hp + this.twin.maxHp * 0.15);
      w.emit({ t: 'boss', phase: 9, name: 'fugueSolo' });
    }
  }
}

/** Both twins; the stage ends when both are gone. */
export function makeFugue(w: World): Fugue {
  const a = new Fugue(w, -1);
  const b = w.add(new Fugue(w, 1));
  a.twin = b;
  b.twin = a;
  return a;
}
