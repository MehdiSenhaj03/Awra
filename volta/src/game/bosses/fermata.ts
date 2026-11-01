import { TAU } from '../../engine/core';
import { BK } from '../bullets';
import { Hazard } from '../hazard';
import { C, column, fan, ring } from '../patterns';
import type { World } from '../world';
import { Boss } from './base';

/**
 * FERMATA, the Silence itself: a black sun that holds every note forever. It pulls the hero in, freezes its
 * bullets until the bar falls, takes the music away in its second phase (keep the beat yourself: your shots
 * are the only music left) and finally plays every Conductor's song at once.
 */
export class Fermata extends Boss {
  kind = 'fermata';
  look = 'fermata';
  name = 'fermata';
  /** 1 while the music is taken away. */
  silence = 0;
  private spiral = 0;

  constructor(w: World) {
    super(0, 0, 4200, 3.4);
    this.color = C.white;
    this.introBars = 3;
    void w;
  }

  move(w: World, k: number): void {
    this.vx = -this.x * 2;
    this.vy = -this.y * 2;
    this.spin += 0.004 * k;
    // Gravity: the black sun pulls.
    const p = w.player;
    if (!p.dead && !p.dashing && this.state === 'fight') {
      const dx = this.x - p.x;
      const dy = this.y - p.y;
      const d = Math.hypot(dx, dy) || 1;
      const pull = (this.phase === 3 ? 4.5 : 3) * Math.min(1, 10 / d);
      p.vx += (dx / d) * pull * 0.016 * 60 * 0.05;
      p.vy += (dy / d) * pull * 0.016 * 60 * 0.05;
    }
    this.silence += ((this.phase === 2 && this.state === 'fight' ? 1 : 0) - this.silence) * 0.02;
  }

  attack(w: World, phase: number, bar: number, step: number): void {
    const x = this.x;
    const y = this.y;
    const aim = this.angleTo(w);
    const spb = w.beat.spb;
    if (phase === 1) {
      // Held notes: rings appear and wait for the next downbeat.
      if (step === 8) {
        const wait = (spb * 8) / 4;
        ring(w, x, y, 20, 6, this.spiral, { color: C.white, wait, r: 0.32 });
        this.spiral += 0.16;
      }
      if (step % 4 === 0) for (let i = 0; i < 3; i++) w.bullets.add(x, y, this.spiral * 2 + (i / 3) * TAU, 6, { color: C.gold, turn: 0.7, r: 0.3 });
      if (bar % 2 === 1 && step === 0) fan(w, x, y, aim, 5, 0.6, 11, { color: C.white, kind: BK.Shard, r: 0.3 });
    } else if (phase === 2) {
      // The silence: only the beat ring remains. Patterns stay strictly on the beat.
      if (step % 4 === 0) {
        ring(w, x, y, 12, 6.5, this.spiral, { color: C.white, r: 0.3 });
        this.spiral += step === 0 ? 0.3 : 0.12;
      }
      if (step === 6 || step === 14) column(w, x, y, aim, 4, 9, 1.6, { color: C.gold, r: 0.3 });
    } else {
      // The final chord: every Conductor's song, a bar each.
      const song = bar % 5;
      if (song === 0 && step % 4 === 0) {
        ring(w, x, y, 16, 7, this.spiral, { color: C.magenta, r: 0.3 });
        this.spiral += 0.14;
      }
      if (song === 1 && step % 4 === 0) fan(w, x, y, aim, 5 + step / 2, 0.8, 3.5, { color: C.green, acc: 9, r: 0.3 });
      if (song === 2 && step === 0) {
        const p = w.player;
        w.addHazard(new Hazard('zone', p.x, p.y, x, y, 3.2, Math.round(spb * 60 * 4), C.gold, 14));
        w.addHazard(new Hazard('zone', -p.x, p.y, x, y, 3.2, Math.round(spb * 60 * 4), C.gold, 14));
      }
      if (song === 3 && step % 2 === 0) ring(w, x, y, 6, 5, step * 0.4, { color: C.violet, turn: step % 4 ? 0.7 : -0.7, r: 0.3 });
      if (song === 4 && step === 0) {
        for (let i = 0; i < 4; i++) {
          const o = -15 + i * 10;
          w.addHazard(new Hazard('beam', o, -24, o, 24, 0.45, Math.round(spb * 60 * 2), C.white, Math.round(spb * 60)));
        }
      }
      if (step === 12) fan(w, x, y, aim, 3, 0.25, 14, { color: C.white, kind: BK.Shard, r: 0.3 });
    }
  }
}
