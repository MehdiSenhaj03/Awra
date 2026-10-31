import { TILE, clamp } from '../engine/core';
import type { Hit } from './entity';
import { Foe } from './foes';
import type { Scene } from './scene';

/**
 * A keeper: three phases by health, readable wind-ups, staggers after a flurry of blows, a slow-motion death,
 * and a reward. Subclasses implement `think()` with a small state machine.
 */
export abstract class Boss extends Foe {
  phase = 1;
  bossKey: string;
  /** Hits taken recently, for staggers. */
  private flurry = 0;
  private flurryT = 0;
  staggerAt = 9;
  dying = 0;
  keep = true;
  intro = 60;
  /** Speed factor (phases make keepers faster). */
  pace = 1;
  /** Frames the hero has spent hugging the keeper; past a limit the keeper answers with a telegraphed repulse. */
  private hugT = 0;
  /** Distance under which the hero counts as hugging (from the keeper's edge). */
  hugRange = 170;
  /** Damage of the repulse burst. */
  repulseDmg = 1;

  constructor(scene: Scene, x: number, feet: number, w: number, h: number, key: string, hp: number) {
    super(scene, x, feet, w, h);
    this.bossKey = key;
    this.hp = this.maxHp = hp * 1.5 * (scene.save.hard ? 1.25 : 1);
    this.heavy = true;
    this.contact = 1;
    this.dust = 0;
    this.brandable = true;
  }

  /** Frames scaled by the current pace. */
  f(n: number): number {
    return Math.round(n / this.pace);
  }

  update(): void {
    this.st++;
    this.active = [];
    if (this.flurryT > 0 && --this.flurryT === 0) this.flurry = 0;
    if (this.dying > 0) {
      this.dying++;
      this.body.vx *= 0.9;
      this.fall();
      if (this.dying % 12 === 0 && this.dying < 100) {
        this.scene.emit({ t: 'boom', x: this.cx + (Math.random() - 0.5) * this.body.w * 1.4, y: this.cy + (Math.random() - 0.5) * this.body.h, r: 140, color: 0xffd890 });
      }
      if (this.dying === 120) this.finish();
      return;
    }
    if (this.intro > 0) {
      this.intro--;
      if (this.intro === 0) this.scene.emit({ t: 'roar', x: this.cx, y: this.cy });
      this.setAnim('idle');
      this.fall();
      return;
    }
    if (this.stun > 0) {
      this.setAnim('stagger');
      this.body.vx *= 0.85;
      this.fall();
      return;
    }
    const next = this.hp / this.maxHp < 0.33 ? 3 : this.hp / this.maxHp < 0.66 ? 2 : 1;
    if (next > this.phase) {
      this.phase = next;
      this.onPhase(next);
    }
    if (this.repulse()) return;
    this.think();
  }

  abstract think(): void;

  /**
   * Anti-hug: standing on a keeper and mashing is never safe. A flash, a short wind-up, then a burst of light
   * all around the keeper. Returns true while the repulse owns the keeper.
   */
  private repulse(): boolean {
    const p = this.scene.player;
    const gap = Math.abs(p.cx - this.cx) - this.body.w / 2;
    const near = gap < this.hugRange && Math.abs(p.cy - this.cy) < this.body.h * 0.6 + 80;
    if (this.state === 'repulseWind') {
      this.body.vx *= 0.8;
      this.setAnim('stagger');
      if (this.st % 6 === 0) this.telegraph(-0.1, true);
      if (this.st >= this.f(24)) {
        const w = this.body.w + 2 * (this.hugRange + 70);
        const h = this.body.h + 90;
        this.scene.add(new HitZone(this.scene, { box: { x: this.cx - w / 2, y: this.body.y + this.body.h - h, w, h }, damage: this.repulseDmg }, 8));
        this.scene.emit({ t: 'shockwave', x: this.cx, y: this.cy, r: w * 0.55, color: 0xfff0c0 });
        this.scene.emit({ t: 'boom', x: this.cx, y: this.cy + this.body.h * 0.3, r: 220, color: 0xffe0a0 });
        this.scene.sfx('break');
        this.to('repulse');
      }
      this.fall();
      return true;
    }
    if (this.state === 'repulse') {
      this.body.vx *= 0.8;
      this.setAnim('idle');
      if (this.st >= this.f(26)) {
        this.hugT = 0;
        this.to('walk');
      }
      this.fall();
      return true;
    }
    this.hugT = near ? this.hugT + 1 : Math.max(0, this.hugT - 2);
    const limit = this.phase === 1 ? 150 : this.phase === 2 ? 110 : 80;
    if (this.hugT > limit && (this.state === 'idle' || this.state === 'walk')) {
      this.hugT = 0;
      this.to('repulseWind');
      this.telegraph(-0.1, true);
      this.scene.sfx('focus');
      return true;
    }
    return false;
  }

  onPhase(n: number): void {
    this.pace = n === 2 ? 1.15 : 1.32;
    this.scene.emit({ t: 'roar', x: this.cx, y: this.cy });
    // A repulse already wound up still goes off.
    if (!this.state.startsWith('repulse')) this.to('roar');
  }

  damage(amount: number, fromX: number, source: Parameters<Foe['damage']>[2]): boolean {
    if (this.dying > 0 || this.intro > 0) return false;
    // A killing blow lands the keeper at the edge of death first, so the last phase always plays out.
    if (this.phase < 3 && this.hp - amount <= 0) amount = Math.max(0, this.hp - this.maxHp * 0.3);
    const ok = super.damage(amount, fromX, source);
    if (ok) {
      this.flurry++;
      this.flurryT = 90;
      if (this.flurry >= this.staggerAt && this.stun === 0) {
        this.flurry = 0;
        this.stun = 80;
        this.scene.emit({ t: 'shockwave', x: this.cx, y: this.cy, r: 200, color: 0xffffff });
        this.scene.sfx('break');
      }
    }
    return ok;
  }

  onDeath(): void {
    // Not dead yet: the death sequence plays first.
    this.dead = false;
    this.hp = 0;
    this.dying = 1;
    this.hurtable = false;
    this.contact = 0;
    this.setAnim('death');
    this.scene.eclipse = 100;
    this.scene.freeze = 20;
    this.scene.emit({ t: 'roar', x: this.cx, y: this.cy });
  }

  private finish(): void {
    this.dead = true;
    this.keep = false;
    this.scene.emit({ t: 'kill', x: this.cx, y: this.cy, size: 300, color: 0xffe0a0, sprite: this.sprite, id: this.id });
    this.scene.emit({ t: 'boom', x: this.cx, y: this.cy, r: 320, color: 0xfff0d0 });
    this.scene.bossDefeated(this);
  }

  /** A ground shockwave travelling along the floor. */
  wave(dir: number, speed = 11, h = 70): void {
    this.scene.add(new GroundWave(this.scene, this.cx + dir * this.body.w * 0.5, this.body.y + this.body.h, dir, speed, h));
  }

  /** Box in front, relative to the boss. */
  reachBox(reach: number, h: number, dy = 0, back = 0, dmg = 1): Hit {
    const y = this.body.y + this.body.h - h + dy;
    const x = this.facing > 0 ? this.cx - back : this.cx - reach;
    return { box: { x, y, w: reach + back, h }, damage: dmg };
  }

  clampToRoom(): void {
    this.body.x = clamp(this.body.x, TILE, this.scene.room.pxW - TILE - this.body.w);
  }
}

/** A wave of light running along the ground; jump over it. */
export class GroundWave extends Foe {
  life = 110;

  constructor(scene: Scene, x: number, feet: number, dir: number, speed: number, h: number) {
    super(scene, x, feet, 50, h);
    this.facing = dir;
    this.body.vx = dir * speed;
    this.hurtable = false;
    this.prop = true;
    this.brandable = false;
    this.sprite = 'wave';
    this.contact = 1;
  }

  update(): void {
    const r = this.scene.moveBody(this);
    this.body.vy = 0;
    if (r.hitX || --this.life <= 0 || !this.groundAhead(this.facing, 4)) {
      this.dead = true;
      this.scene.emit({ t: 'spark', x: this.cx, y: this.cy, n: 8, color: 0xffd890 });
    }
    if (this.t % 3 === 0) this.scene.emit({ t: 'spark', x: this.cx, y: this.body.y + this.body.h - 10, n: 2, color: 0xffd890 });
  }
}

/** Falling debris (crystal shards from the ceiling), announced by a shadow mark. */
export class Debris extends Foe {
  warn = 50;

  constructor(scene: Scene, x: number) {
    super(scene, x, 120, 40, 60);
    this.hurtable = false;
    this.prop = true;
    this.brandable = false;
    this.sprite = 'debris';
    this.contact = 1;
    this.body.noPlatforms = true;
    this.body.y = -60;
  }

  update(): void {
    if (this.warn > 0) {
      this.warn--;
      if (this.warn % 10 === 0) this.scene.emit({ t: 'spark', x: this.cx, y: 40, n: 3, color: 0xa8c0ff });
      return;
    }
    this.body.vy = Math.min(this.body.vy + 1.2, 24);
    const r = this.scene.moveBody(this);
    if (r.hitY) {
      this.dead = true;
      this.scene.emit({ t: 'spark', x: this.cx, y: this.body.y + this.body.h, n: 12, color: 0xa8c0ff });
      this.scene.sfx('clank');
    }
  }
}

/** A short-lived damaging zone (slams, sweeps) that the renderer does not draw. */
export class HitZone extends Foe {
  constructor(scene: Scene, public hb: Hit, public life: number) {
    super(scene, hb.box.x + hb.box.w / 2, hb.box.y + hb.box.h, hb.box.w, hb.box.h);
    this.hurtable = false;
    this.prop = true;
    this.brandable = false;
    this.contact = 0;
    this.body.noClip = true;
  }

  update(): void {
    this.active = [this.hb];
    if (--this.life <= 0) this.dead = true;
  }
}

export const BOSS_TILE = TILE;
