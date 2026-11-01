import { TAU } from '../../engine/core';
import { BK } from '../bullets';
import { Enemy } from '../enemy';
import { Hazard } from '../hazard';
import { C, fan, ring } from '../patterns';
import type { World } from '../world';
import { Boss } from './base';

/** A hollow copy of Staccato: shoots with it, breaks in a few hits, gives nothing to the real one. */
export class Decoy extends Enemy {
  kind = 'decoy';
  look = 'staccato';
  constructor(x: number, y: number, public master: Staccato) {
    super(x, y);
    this.r = 1.6;
    this.hp = this.maxHp = 30;
    this.color = C.magenta;
    this.value = 300;
    this.notes = 4;
    this.heavy = true;
  }
  update(w: World): void {
    this.vx *= 0.8;
    this.vy *= 0.8;
    if (this.master.dead || this.master.dying > 0) this.dead = true;
    if (w.beat.crossed(4) && [0, 1, 2].includes(w.beat.index(4) % 16)) {
      w.bullets.add(this.x, this.y, this.angleTo(w), 15, { color: C.magenta, kind: BK.Shard, r: 0.28 });
    }
  }
}

/**
 * STACCATO, Conductor of the Mute City: a tower of dead skyscrapers that cuts every sound short. It jumps
 * between anchors on the bar, fires in hard bursts and lays grids of light that burn on the downbeat.
 */
export class Staccato extends Boss {
  kind = 'staccato';
  look = 'staccato';
  name = 'staccato';
  private anchors = [
    [0, -10],
    [-12, -2],
    [12, -2],
    [0, 8],
  ];
  private at = 0;
  private tx = 0;
  private ty = -10;

  constructor(w: World) {
    super(0, -10, 2600, 1.6);
    this.color = C.magenta;
    void w;
  }

  move(w: World): void {
    this.vx = (this.tx - this.x) * 10;
    this.vy = (this.ty - this.y) * 10;
    this.rot = this.angleTo(w);
  }

  private jump(w: World): void {
    const p = w.player;
    let best = this.at;
    let bd = -1;
    this.anchors.forEach(([x, y], i) => {
      if (i === this.at) return;
      const d = Math.hypot(p.x - x, p.y - y);
      if (d > bd && w.rng.chance(0.75)) {
        bd = d;
        best = i;
      }
    });
    if (best === this.at) best = (this.at + 1) % this.anchors.length;
    this.at = best;
    w.emit({ t: 'spawn', x: this.x, y: this.y, color: C.magenta, size: 1.6 });
    [this.tx, this.ty] = this.anchors[best];
    w.emit({ t: 'boom', x: this.tx, y: this.ty, r: 3, color: C.magenta });
    w.sfx('phase', 0.8);
  }

  /** Parallel beams across the arena, warned for a bar. */
  private grid(w: World, vertical: boolean, n: number, offset: number, angle = 0): void {
    const spb = w.beat.spb * 60;
    for (let i = 0; i < n; i++) {
      const o = -18 + ((i + offset) / n) * 36;
      const c = Math.cos(angle);
      const s = Math.sin(angle);
      const [ax, ay, bx, by] = vertical ? [o, -24, o, 24] : [-24, o, 24, o];
      const h = new Hazard('beam', ax * c - ay * s, ax * s + ay * c, bx * c - by * s, bx * s + by * c, 0.45, Math.round(spb * 4), C.magenta, Math.round(spb));
      w.addHazard(h);
    }
    w.sfx('telegraph', 0.7, 0.6);
  }

  onPhase(w: World, phase: number): void {
    if (phase === 2) {
      for (const [x, y] of this.anchors.filter((_, i) => i !== this.at).slice(0, 2)) {
        const d = w.add(new Decoy(x, y, this));
        d.spawnT = 40;
      }
    }
  }

  attack(w: World, phase: number, bar: number, step: number): void {
    const x = this.x;
    const y = this.y;
    const aim = this.angleTo(w);
    if (step === 0 && bar % 2 === 1) this.jump(w);
    // Staccato bursts: three short notes, rest, three more.
    if ([0, 1, 2, 8, 9, 10].includes(step)) {
      w.bullets.add(x, y, aim + (step % 8 - 1) * 0.08, 17, { color: C.white, kind: BK.Shard, r: 0.3 });
      if (phase >= 2) fan(w, x, y, aim, 3, 0.9, 12, { color: C.magenta, kind: BK.Shard, r: 0.26 });
    }
    if (step === 4 && bar % 2 === 0) {
      if (phase === 1) this.grid(w, bar % 4 === 0, 4, (bar % 8) / 8);
      else if (phase === 2) {
        this.grid(w, true, 3, (bar % 6) / 6);
        this.grid(w, false, 3, ((bar + 3) % 6) / 6);
      } else this.grid(w, bar % 4 === 0, 5, (bar % 10) / 10, Math.PI / 4);
    }
    if (phase === 3 && step === 12) ring(w, x, y, 20, 8, bar * 0.2, { color: C.violet, r: 0.3 });
    if (phase === 3 && step % 4 === 2) w.bullets.add(x, y, (step / 16) * TAU + bar, 9, { color: C.magenta, r: 0.32 });
  }
}
