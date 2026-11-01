import type { SK } from './bullets';
import type { World } from './world';

/** Base of every foe: spawning, stun, hit flash, knockback, contact damage, death. */
export abstract class Enemy {
  id = 0;
  abstract kind: string;
  /** Model id for the renderer. */
  look = '';
  x: number;
  y: number;
  px: number;
  py: number;
  vx = 0;
  vy = 0;
  r = 0.6;
  hp = 5;
  maxHp = 5;
  color = 0xff2fb9;
  value = 100;
  notes = 2;
  heavy = false;
  elite = false;
  boss = false;
  t = 0;
  st = 0;
  state = 'idle';
  spawnT = 45;
  dead = false;
  stun = 0;
  flash = 0;
  hurtable = true;
  targetable = true;
  contact = true;
  /** Facing angle (render). */
  rot = 0;
  /** Free animation channel (render). */
  spin = 0;
  /** Visible to the hero (phantoms fade). */
  vis = 1;
  /** Damage taken is multiplied by this (Choir shields). */
  armor = 1;
  shielded = 0;

  constructor(x: number, y: number) {
    this.x = this.px = x;
    this.y = this.py = y;
  }

  get active(): boolean {
    return !this.dead && this.spawnT <= 0;
  }

  /** Makes this an elite: tougher, bigger, crowned, may drop a heal. */
  makeElite(): this {
    this.elite = true;
    this.hp = this.maxHp = this.maxHp * 2.6;
    this.r *= 1.25;
    this.value *= 3;
    this.notes *= 3;
    return this;
  }

  to(s: string): void {
    this.state = s;
    this.st = 0;
  }

  angleTo(w: World): number {
    return Math.atan2(w.player.y - this.y, w.player.x - this.x);
  }

  distTo(w: World): number {
    return Math.hypot(w.player.x - this.x, w.player.y - this.y);
  }

  tick(w: World): void {
    this.px = this.x;
    this.py = this.y;
    if (this.dead) return;
    if (this.spawnT > 0) {
      this.spawnT--;
      return;
    }
    const k = w.k;
    this.t += k;
    this.st += k;
    if (this.flash > 0) this.flash = Math.max(0, this.flash - 0.12);
    if (this.shielded > 0) this.shielded--;
    if (this.stun > 0) {
      this.stun -= k;
      this.vx *= 0.86;
      this.vy *= 0.86;
    } else {
      this.update(w, k);
    }
    this.x += (this.vx * k) / 60;
    this.y += (this.vy * k) / 60;
    w.arena.confine(this, this.r);
    if (this.contact && this.vis > 0.5) {
      const p = w.player;
      if (Math.hypot(p.x - this.x, p.y - this.y) < this.r + 0.3) w.hurtPlayer(this.x, this.y);
    }
  }

  abstract update(w: World, k: number): void;

  /** Returns false when the hit was blocked. */
  hit(w: World, dmg: number, fromX: number, fromY: number, _kind?: SK): boolean {
    if (!this.hurtable || this.dead || this.spawnT > 0) return false;
    if (this.blocks(w, fromX, fromY, _kind)) {
      w.emit({ t: 'hit', x: this.x + (fromX - this.x) * 0.5, y: this.y + (fromY - this.y) * 0.5, color: 0xffffff, big: false });
      w.sfx('block', 1 + Math.random() * 0.2, 0.5);
      return false;
    }
    const d = dmg * (this.shielded > 0 ? 0.25 : this.armor);
    this.hp -= d;
    this.flash = 1;
    if (!this.heavy) {
      const a = Math.atan2(this.y - fromY, this.x - fromX);
      this.vx += Math.cos(a) * Math.min(8, d * 2.2);
      this.vy += Math.sin(a) * Math.min(8, d * 2.2);
    }
    w.emit({ t: 'hit', x: this.x, y: this.y, color: this.color, big: d >= 3 });
    if (this.hp <= 0) this.die(w);
    return true;
  }

  /** Shields and mirrors override this. */
  blocks(_w: World, _fx: number, _fy: number, _kind?: SK): boolean {
    return false;
  }

  die(w: World): void {
    if (this.dead) return;
    this.dead = true;
    w.killed(this);
    if (this.elite && w.rng.chance(0.4)) w.drop(this.x, this.y, 1, 'heal');
    this.onDeath(w);
  }

  onDeath(_w: World): void {}
}
