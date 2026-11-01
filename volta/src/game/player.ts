import { TAU, clamp, damp } from '../engine/core';
import type { Controls } from './controls';
import type { Judge } from './events';
import { WEAPONS } from './weapons';
import type { World } from './world';

export const PLAYER = {
  speed: 11.5,
  accel: 22,
  radius: 0.32,
  graze: 1.15,
  dashFrames: 9,
  dashDist: 5.4,
  dashRecharge: 50,
  pulseFrames: 8,
  pulseRadius: 3.1,
  pulseCooldown: 30,
  pulseMissCooldown: 48,
  hurtInv: 80,
};

export class Player {
  x = 0;
  y = 0;
  px = 0;
  py = 0;
  vx = 0;
  vy = 0;
  aim = 0;
  hp = 4;
  maxHp = 4;
  inv = 0;
  dead = false;
  deadT = 0;
  dashCharges = 2;
  maxDash = 2;
  dashRefill = 0;
  dashT = 0;
  dashA = 0;
  dashJudge: Judge = 'off';
  pulseT = 0;
  pulseCd = 0;
  pulseR = PLAYER.pulseRadius;
  pulseJudge: Judge = 'off';
  pulseHits = 0;
  /** Weapon state. */
  weapon = 'prism';
  weaponLevel = 1;
  charge = 0;
  chargeStartBeat = -1;
  lastPerfectBeat = -9;
  firing = false;
  /** Overdrive end, in beats (0 = off). */
  odEnd = 0;
  /** Afterimage spawn timer for renderer. */
  t = 0;

  get overdrive(): boolean {
    return this.odEnd > 0;
  }

  get dashing(): boolean {
    return this.dashT > 0;
  }

  get invulnerable(): boolean {
    return this.inv > 0 || this.dashT > 0 || this.dead;
  }

  update(w: World, c: Controls): void {
    this.t++;
    this.px = this.x;
    this.py = this.y;
    if (this.dead) {
      this.deadT++;
      this.vx *= 0.9;
      this.vy *= 0.9;
      return;
    }
    if (this.inv > 0) this.inv--;
    const mods = w.mods;
    // Aim.
    if (c.ax || c.ay) this.aim = Math.atan2(c.ay, c.ax);
    // Dash.
    if (this.dashCharges < this.maxDash) {
      if (++this.dashRefill >= PLAYER.dashRecharge * mods.dashRecharge) {
        this.dashRefill = 0;
        this.dashCharges++;
      }
    }
    if (c.dash && this.dashT === 0 && this.dashCharges > 0) {
      const ml = Math.hypot(c.mx, c.my);
      this.dashA = ml > 0.2 ? Math.atan2(c.my, c.mx) : this.aim;
      this.dashT = PLAYER.dashFrames;
      this.dashCharges--;
      this.dashJudge = w.beat.judge();
      w.judged(this.dashJudge, 'dash', this.x, this.y);
      if (this.dashJudge !== 'off') this.dashCharges = Math.min(this.maxDash, this.dashCharges + 1);
      w.emit({ t: 'dash', x: this.x, y: this.y, a: this.dashA, judge: this.dashJudge });
      w.sfx('dash', this.dashJudge === 'perfect' ? 1.15 : 1);
    }
    if (this.dashT > 0) {
      const sp = (PLAYER.dashDist * mods.dashDist) / PLAYER.dashFrames;
      this.vx = Math.cos(this.dashA) * sp * 60;
      this.vy = Math.sin(this.dashA) * sp * 60;
      this.dashT--;
      if (this.dashT === 0) {
        // Leaving the dash keeps some momentum.
        this.vx *= 0.35;
        this.vy *= 0.35;
        if (this.dashJudge === 'perfect') w.dashBurst(this.x, this.y);
      }
    } else {
      const sp = PLAYER.speed * mods.speed * (this.firing && this.weapon === 'lance' && this.charge > 0 ? 0.6 : 1);
      const k = damp(PLAYER.accel);
      this.vx += (c.mx * sp - this.vx) * k;
      this.vy += (c.my * sp - this.vy) * k;
    }
    this.x += this.vx / 60;
    this.y += this.vy / 60;
    w.arena.confine(this, PLAYER.radius);
    // Pulse (parry).
    if (this.pulseCd > 0) this.pulseCd--;
    if (c.pulse && this.pulseCd === 0 && this.pulseT === 0) {
      this.pulseJudge = w.beat.judge();
      this.pulseR = PLAYER.pulseRadius * mods.pulseRadius * (this.pulseJudge === 'perfect' ? 1.45 : this.pulseJudge === 'great' ? 1.2 : 1);
      this.pulseT = PLAYER.pulseFrames;
      this.pulseHits = 0;
      w.judged(this.pulseJudge, 'pulse', this.x, this.y);
      w.emit({ t: 'pulse', x: this.x, y: this.y, r: this.pulseR, judge: this.pulseJudge });
      w.sfx('pulse', this.pulseJudge === 'perfect' ? 1.2 : 1);
    }
    if (this.pulseT > 0) {
      this.pulseHits += w.reflect(this.x, this.y, this.pulseR, this.pulseJudge);
      if (--this.pulseT === 0) this.pulseCd = this.pulseHits > 0 ? PLAYER.pulseCooldown : PLAYER.pulseMissCooldown;
    }
    // Overdrive.
    if (this.odEnd > 0 && w.beat.beat >= this.odEnd) {
      this.odEnd = 0;
      w.emit({ t: 'overdrive', on: false });
    }
    if (c.overdrive && this.odEnd === 0 && w.gauge >= 100) {
      w.gauge = 0;
      this.odEnd = Math.ceil(w.beat.beat) + 16 * mods.overdrive;
      w.emit({ t: 'overdrive', on: true });
      w.emit({ t: 'flash', color: 0xffffff, a: 0.8 });
      w.emit({ t: 'shake', a: 0.6 });
      w.sfx('overdrive');
      w.clearBullets(this.x, this.y, 9, true);
    }
    // Weapon.
    WEAPONS[this.weapon].update(w, this, c);
    this.aim = ((this.aim % TAU) + TAU) % TAU;
    this.hp = clamp(this.hp, 0, this.maxHp);
  }
}
