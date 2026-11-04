import { Rng, TAU, angleDiff } from '../engine/core';
import { Arena, type ArenaDef } from './arena';
import { BeatClock } from './beat';
import { EnemyBullets, SK, Shots } from './bullets';
import type { Controls } from './controls';
import type { Enemy } from './enemy';
import type { Fx, Judge } from './events';
import type { Hazard } from './hazard';
import { PLAYER, Player } from './player';

/** Style ranks: each one multiplies the score. */
export const RANKS = ['D', 'C', 'B', 'A', 'S', 'SS', 'V'];
export const RANK_AT = [0, 100, 230, 380, 550, 740, 920];
export const RANK_MULT = [1, 1.5, 2, 3, 4, 6, 8];
const STYLE_MAX = 1000;

/** Player modifiers from Overtones (perks). */
export interface Mods {
  damage: number;
  speed: number;
  dashDist: number;
  dashRecharge: number;
  pulseRadius: number;
  overdrive: number;
  gauge: number;
  magnet: number;
  styleKeep: number;
  /** Extra effects, by Overtone id. */
  has: Set<string>;
}

export const BASE_MODS = (): Mods => ({ damage: 1, speed: 1, dashDist: 1, dashRecharge: 1, pulseRadius: 1, overdrive: 1, gauge: 1, magnet: 1, styleKeep: 1, has: new Set() });

export interface Pickup {
  x: number;
  y: number;
  vx: number;
  vy: number;
  kind: 'note' | 'heal' | 'fragment';
  t: number;
  id?: string;
}

export interface Stats {
  kills: number;
  perfects: number;
  greats: number;
  offs: number;
  parries: number;
  grazes: number;
  hits: number;
  maxRank: number;
  notes: number;
  frames: number;
}

export class World {
  rng: Rng;
  beat: BeatClock;
  arena: Arena;
  player = new Player();
  enemies: Enemy[] = [];
  bullets = new EnemyBullets();
  shots = new Shots();
  pickups: Pickup[] = [];
  hazards: Hazard[] = [];
  /** Persistent hazards drawn by the renderer (lasers, telegraphs…), owned by enemies. */
  events: Fx[] = [];
  mods: Mods = BASE_MODS();
  frame = 0;
  /** Hit-stop frames (the beat keeps running). */
  freeze = 0;
  private lastStop = -999;

  /**
   * Requests a short hit-stop. Small ones are skipped when another one happened recently, so dense fights
   * never stutter; big moments (getting hit, a Conductor falling) always get theirs.
   */
  hitStop(frames: number): void {
    if (frames < 6 && this.frame - this.lastStop < 24) return;
    if (frames > this.freeze) this.freeze = frames;
    this.lastStop = this.frame;
  }
  score = 0;
  style = 0;
  rank = 0;
  private styleIdle = 0;
  gauge = 0;
  stats: Stats = { kills: 0, perfects: 0, greats: 0, offs: 0, parries: 0, grazes: 0, hits: 0, maxRank: 0, notes: 0, frames: 0 };
  nextId = 1;
  /** Set by the stage runner. */
  over: 'win' | 'lose' | null = null;
  /** Tests: the hero never takes damage. */
  god = false;
  /** Enemy time scale (overdrive slows the world). */
  k = 1;
  /** Abilities unlocked by the campaign. */
  canPulse = true;
  canOverdrive = true;
  /** Sourdine: one absorbed hit per wave. */
  guard = false;
  /** Reprise: one second chance per stage. */
  secondChance = false;
  private perfectsForHeal = 0;

  constructor(arena: ArenaDef, bpm = 128, seed = 1) {
    this.rng = new Rng(seed);
    this.beat = new BeatClock(bpm);
    this.arena = new Arena(arena);
    this.player.y = 0;
  }

  /** Applies Overtones that change the hero or the rules at the start of a stage. */
  applyMods(m: Mods): void {
    this.mods = m;
    const p = this.player;
    if (m.has.has('trille')) p.maxDash = p.dashCharges = 3;
    if (m.has.has('virtuose')) p.maxHp = p.hp = Math.max(1, p.maxHp - 1);
    if (m.has.has('metronome')) this.beat.perfectWindow = 0.1;
    this.secondChance = m.has.has('reprise');
    this.guard = m.has.has('sourdine');
  }

  get scoreMult(): number {
    return (this.mods.has.has('virtuose') ? 1.25 : 1) * (this.mods.has.has('metronome') ? 0.9 : 1);
  }

  get mult(): number {
    return RANK_MULT[this.rank] * (this.player.overdrive ? 2 : 1);
  }

  emit(e: Fx): void {
    this.events.push(e);
  }

  sfx(name: string, pitch = 1, vol = 1): void {
    this.events.push({ t: 'sfx', name, pitch, vol });
  }

  addHazard(h: Hazard): Hazard {
    h.id = this.nextId++;
    this.hazards.push(h);
    return h;
  }

  add<T extends Enemy>(e: T): T {
    e.id = this.nextId++;
    this.enemies.push(e);
    return e;
  }

  // ---- Style, score, gauge ------------------------------------------------------------------------------

  gain(style: number, gauge: number, score: number, x?: number, y?: number): void {
    this.style = Math.min(STYLE_MAX, this.style + style);
    this.styleIdle = 0;
    this.gauge = Math.min(100, this.gauge + gauge * this.mods.gauge);
    if (score > 0) {
      const v = Math.round(score * this.mult * this.scoreMult);
      this.score += v;
      if (x !== undefined && y !== undefined) this.emit({ t: 'score', x, y, value: v });
    }
    this.updateRank();
  }

  private updateRank(): void {
    let r = 0;
    for (let i = 0; i < RANK_AT.length; i++) if (this.style >= RANK_AT[i]) r = i;
    if (r !== this.rank) {
      this.emit({ t: 'rank', rank: r, up: r > this.rank });
      if (r > this.rank) this.sfx('rankUp', 1 + r * 0.06);
      this.rank = r;
      this.stats.maxRank = Math.max(this.stats.maxRank, r);
    }
  }

  /** A rhythm action was judged. */
  judged(j: Judge, what: string, x: number, y: number): void {
    if (j === 'perfect') {
      this.stats.perfects++;
      this.gain(26, 5, 60, x, y);
      if (this.mods.has.has('harmonie') && ++this.perfectsForHeal >= 30) {
        this.perfectsForHeal = 0;
        if (this.player.hp < this.player.maxHp) {
          this.player.hp++;
          this.emit({ t: 'heal', x, y });
          this.sfx('heal');
        }
      }
    } else if (j === 'great') {
      this.stats.greats++;
      this.gain(10, 2.5, 25);
    } else {
      this.stats.offs++;
      return;
    }
    this.emit({ t: 'judge', x, y, judge: j, what });
  }

  // ---- Player actions -----------------------------------------------------------------------------------

  recoil(a: number): void {
    this.emit({ t: 'shake', a: a * 0.35 });
  }

  /** A perfect dash leaves a burst of light behind. */
  dashBurst(x: number, y: number): void {
    this.emit({ t: 'boom', x, y, r: 2.6, color: 0x2ef2ff });
    for (const e of this.enemies) if (e.active && Math.hypot(e.x - x, e.y - y) < 2.6 + e.r) e.hit(this, 6 * this.mods.damage, x, y);
    this.clearBullets(x, y, 2.6, true);
  }

  /** Reflects enemy bullets inside the pulse. Returns how many. */
  reflect(x: number, y: number, r: number, j: Judge): number {
    const b = this.bullets;
    let n = 0;
    for (let i = 0; i < b.n; i++) {
      if (b.flags[i] === 2) continue;
      const dx = b.x[i] - x;
      const dy = b.y[i] - y;
      if (dx * dx + dy * dy > (r + b.r[i]) ** 2) continue;
      const t = this.nearestEnemy(b.x[i], b.y[i], 40);
      const a = t ? Math.atan2(t.y - b.y[i], t.x - b.x[i]) : Math.atan2(dy, dx);
      const sp = Math.max(26, Math.hypot(b.vx[i], b.vy[i]) * 1.8);
      b.vx[i] = Math.cos(a) * sp;
      b.vy[i] = Math.sin(a) * sp;
      b.flags[i] = 2;
      b.wait[i] = 0;
      b.acc[i] = 0;
      b.turn[i] = 0;
      b.life[i] = 2;
      b.color[i] = j === 'perfect' ? 0xffffff : 0x2ef2ff;
      b.dmg[i] = (j === 'perfect' ? 7 : j === 'great' ? 5 : 3.5) * this.mods.damage;
      n++;
      if (this.mods.has.has('contrepoint')) {
        for (const da of [-0.35, 0.35]) {
          const k = b.add(b.x[i], b.y[i], a + da, sp, { color: b.color[i], r: b.r[i], life: 2 });
          if (k >= 0) {
            b.flags[k] = 2;
            b.dmg[k] = b.dmg[i] * 0.6;
          }
        }
      }
    }
    if (n > 0) {
      this.stats.parries += n;
      this.gain(18 + n * 6, 6 + n * 2, 80 * n, x, y);
      this.emit({ t: 'parry', x, y, n });
      this.hitStop(j === 'perfect' ? 4 : 2);
      this.sfx('parry', j === 'perfect' ? 1.25 : 1);
    }
    // The pulse also shoves light foes.
    if (j !== 'off') {
      for (const e of this.enemies) {
        if (!e.active || e.heavy) continue;
        const d = Math.hypot(e.x - x, e.y - y);
        if (d < r + e.r) {
          e.vx += ((e.x - x) / (d || 1)) * 14;
          e.vy += ((e.y - y) / (d || 1)) * 14;
          e.stun = Math.max(e.stun, j === 'perfect' ? 40 : 20);
        }
      }
    }
    return n;
  }

  /** Removes bullets in a radius (optionally turning them into notes). */
  clearBullets(x: number, y: number, r: number, toNotes: boolean): void {
    const b = this.bullets;
    for (let i = b.n - 1; i >= 0; i--) {
      if (b.flags[i] === 2) continue;
      if ((b.x[i] - x) ** 2 + (b.y[i] - y) ** 2 < r * r) {
        if (toNotes && this.rng.chance(0.3)) this.drop(b.x[i], b.y[i], 1, 'note');
        this.emit({ t: 'hit', x: b.x[i], y: b.y[i], color: b.color[i], big: false });
        b.remove(i);
      }
    }
  }

  /** Chain lightning from the hero along the aim. */
  chain(x: number, y: number, aim: number, jumps: number, dmg: number, range: number, stun: boolean): void {
    let cx = x;
    let cy = y;
    const hit = new Set<number>();
    let first = true;
    for (let k = 0; k < jumps; k++) {
      let best: Enemy | null = null;
      let bd = first ? range : range * 0.7;
      for (const e of this.enemies) {
        if (!e.active || hit.has(e.id)) continue;
        const d = Math.hypot(e.x - cx, e.y - cy) - e.r;
        if (d > bd) continue;
        if (first && Math.abs(angleDiff(aim, Math.atan2(e.y - cy, e.x - cx))) > 0.75) continue;
        bd = d;
        best = e;
      }
      if (!best) {
        if (first) this.emit({ t: 'shot', x: cx + Math.cos(aim) * range * 0.6, y: cy + Math.sin(aim) * range * 0.6, a: aim, accent: false, weapon: 'arcMiss' });
        break;
      }
      this.emit({ t: 'shot', x: best.x, y: best.y, a: Math.atan2(cy - best.y, cx - best.x), accent: true, weapon: `arcLink:${cx.toFixed(2)},${cy.toFixed(2)}` });
      best.hit(this, dmg * (first ? 1 : 0.8 ** k), cx, cy);
      if (stun) best.stun = Math.max(best.stun, 30);
      hit.add(best.id);
      cx = best.x;
      cy = best.y;
      first = false;
    }
  }

  nearestEnemy(x: number, y: number, max: number): Enemy | null {
    let best: Enemy | null = null;
    let bd = max;
    for (const e of this.enemies) {
      if (!e.active || !e.targetable) continue;
      const d = Math.hypot(e.x - x, e.y - y);
      if (d < bd) {
        bd = d;
        best = e;
      }
    }
    return best;
  }

  enemyById(id: number): Enemy | undefined {
    return this.enemies.find((e) => e.id === id);
  }

  drop(x: number, y: number, n: number, kind: Pickup['kind'], id?: string): void {
    for (let i = 0; i < n; i++) {
      const a = this.rng.range(0, TAU);
      const s = this.rng.range(2, 8);
      this.pickups.push({ x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s, kind, t: 0, id });
    }
  }

  // ---- Damage -------------------------------------------------------------------------------------------

  hurtPlayer(fromX: number, fromY: number): void {
    const p = this.player;
    if (p.invulnerable || this.god) return;
    if (this.guard) {
      this.guard = false;
      p.inv = 50;
      this.clearBullets(p.x, p.y, 4, false);
      this.emit({ t: 'ring', x: p.x, y: p.y, r: 3, color: 0xffffff, life: 24 });
      this.emit({ t: 'judge', x: p.x, y: p.y, judge: 'great', what: 'guard' });
      this.sfx('parry', 0.7);
      return;
    }
    p.hp--;
    if (p.hp <= 0 && this.secondChance) {
      this.secondChance = false;
      p.hp = 1;
      this.emit({ t: 'heal', x: p.x, y: p.y });
    }
    p.inv = PLAYER.hurtInv;
    this.stats.hits++;
    // Losing two ranks hurts more than the hit itself.
    const r = Math.max(0, this.rank - 2);
    this.style = Math.min(this.style, RANK_AT[r]);
    this.updateRank();
    this.hitStop(6);
    const a = Math.atan2(p.y - fromY, p.x - fromX);
    p.vx = Math.cos(a) * 16;
    p.vy = Math.sin(a) * 16;
    this.clearBullets(p.x, p.y, 4.5, false);
    this.emit({ t: 'hurt', x: p.x, y: p.y, hp: p.hp });
    this.emit({ t: 'shake', a: 0.9 });
    this.emit({ t: 'chroma', a: 1 });
    this.sfx('hurt');
    if (p.hp <= 0) {
      p.dead = true;
      this.emit({ t: 'die', x: p.x, y: p.y });
      this.sfx('die');
      this.hitStop(18);
    }
  }

  /** Called by enemies when they die. */
  killed(e: Enemy): void {
    this.stats.kills++;
    this.gain(10 + e.value / 25, 2.5 + e.value / 60, e.value, e.x, e.y);
    this.emit({ t: 'kill', x: e.x, y: e.y, size: e.r, color: e.color, look: e.look, heavy: e.heavy });
    this.drop(e.x, e.y, e.notes, 'note');
    if (e.heavy) this.hitStop(3);
  }

  // ---- Frame --------------------------------------------------------------------------------------------

  update(c: Controls): void {
    this.beat.tick();
    this.frame++;
    if (this.freeze > 0) {
      this.freeze--;
      return;
    }
    if (this.over) return;
    this.stats.frames++;
    const p = this.player;
    this.k = p.overdrive ? 0.55 : 1;
    p.update(this, c);
    for (const e of this.enemies) e.tick(this);
    for (const h of this.hazards) h.update(this);
    let n = 0;
    for (const h of this.hazards) if (!h.done) this.hazards[n++] = h;
    this.hazards.length = n;
    this.separate();
    this.updateShots();
    this.updateBullets();
    this.updatePickups();
    for (let i = this.enemies.length - 1; i >= 0; i--) if (this.enemies[i].dead) this.enemies.splice(i, 1);
    // Style slowly drains when nothing happens.
    if (++this.styleIdle > 90 * this.mods.styleKeep) {
      this.style = Math.max(0, this.style - (1 + this.rank * 0.35));
      this.updateRank();
    }
  }

  /** Soft push between enemies so they never merge into one blob. */
  private separate(): void {
    const es = this.enemies;
    for (let i = 0; i < es.length; i++) {
      const a = es[i];
      if (!a.active || a.boss) continue;
      for (let j = i + 1; j < es.length; j++) {
        const b = es[j];
        if (!b.active || b.boss) continue;
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const m = a.r + b.r;
        const d2 = dx * dx + dy * dy;
        if (d2 >= m * m || d2 < 1e-6) continue;
        const d = Math.sqrt(d2);
        const push = (m - d) / d;
        // Heavy foes barely move for light ones.
        const wa = a.heavy === b.heavy ? 0.5 : a.heavy ? 0.05 : 0.95;
        const wb = 1 - wa;
        a.x -= dx * push * wa * 0.5;
        a.y -= dy * push * wa * 0.5;
        b.x += dx * push * wb * 0.5;
        b.y += dy * push * wb * 0.5;
      }
    }
  }

  private updateShots(): void {
    const s = this.shots;
    const dt = 1 / 60;
    for (let i = s.n - 1; i >= 0; i--) {
      if (s.kind[i] === SK.Missile) {
        // Homing: turn towards the target, accelerate.
        let t = s.target[i] ? this.enemyById(s.target[i]) : undefined;
        if (!t || !t.active) {
          t = this.nearestEnemy(s.x[i], s.y[i], 30) ?? undefined;
          s.target[i] = t?.id ?? 0;
        }
        const sp = Math.min(48, Math.hypot(s.vx[i], s.vy[i]) + 1.4);
        let a = Math.atan2(s.vy[i], s.vx[i]);
        if (t) a += Math.max(-0.16, Math.min(0.16, angleDiff(a, Math.atan2(t.y - s.y[i], t.x - s.x[i]))));
        s.vx[i] = Math.cos(a) * sp;
        s.vy[i] = Math.sin(a) * sp;
      }
      s.x[i] += s.vx[i] * dt;
      s.y[i] += s.vy[i] * dt;
      s.life[i] -= dt;
      if (s.life[i] <= 0 || !this.arena.inside(s.x[i], s.y[i], -1) || this.arena.blocked(s.x[i], s.y[i])) {
        if (s.life[i] > 0) this.emit({ t: 'hit', x: s.x[i], y: s.y[i], color: 0x9ff8ff, big: false });
        s.remove(i);
        continue;
      }
      for (const e of this.enemies) {
        if (!e.active || !e.hurtable) continue;
        if (s.last[i] === e.id) continue;
        const dx = s.x[i] - e.x;
        const dy = s.y[i] - e.y;
        const rr = e.r + (s.kind[i] === SK.Lance ? 0.6 : 0.15);
        if (dx * dx + dy * dy > rr * rr) continue;
        e.hit(this, s.dmg[i], s.x[i] - s.vx[i] * 0.02, s.y[i] - s.vy[i] * 0.02, s.kind[i]);
        s.last[i] = e.id;
        if (s.pierce[i] > 0) {
          s.pierce[i]--;
          continue;
        }
        s.life[i] = 0;
        break;
      }
      if (s.life[i] <= 0) s.remove(i);
    }
  }

  private updateBullets(): void {
    const b = this.bullets;
    const p = this.player;
    const k = this.k;
    const dt = (1 / 60) * k;
    for (let i = b.n - 1; i >= 0; i--) {
      const reflected = b.flags[i] === 2;
      const dtt = reflected ? 1 / 60 : dt;
      if (b.wait[i] > 0) {
        b.wait[i] -= dtt;
        continue;
      }
      if (b.turn[i] !== 0) {
        const c = Math.cos(b.turn[i] * dtt);
        const s = Math.sin(b.turn[i] * dtt);
        const vx = b.vx[i] * c - b.vy[i] * s;
        b.vy[i] = b.vx[i] * s + b.vy[i] * c;
        b.vx[i] = vx;
      }
      if (b.acc[i] !== 0) {
        const sp = Math.hypot(b.vx[i], b.vy[i]);
        const ns = Math.max(0.5, sp + b.acc[i] * dtt);
        b.vx[i] *= ns / sp;
        b.vy[i] *= ns / sp;
      }
      b.x[i] += b.vx[i] * dtt;
      b.y[i] += b.vy[i] * dtt;
      b.life[i] -= dtt;
      if (b.life[i] <= 0 || !this.arena.inside(b.x[i], b.y[i], -1.5) || this.arena.blocked(b.x[i], b.y[i])) {
        if (b.life[i] > 0) this.emit({ t: 'hit', x: b.x[i], y: b.y[i], color: b.color[i], big: false });
        b.remove(i);
        continue;
      }
      if (reflected) {
        for (const e of this.enemies) {
          if (!e.active || !e.hurtable) continue;
          if ((b.x[i] - e.x) ** 2 + (b.y[i] - e.y) ** 2 < (e.r + b.r[i]) ** 2) {
            e.hit(this, b.dmg[i], b.x[i], b.y[i], SK.Accent);
            this.emit({ t: 'boom', x: b.x[i], y: b.y[i], r: 1.2, color: 0x2ef2ff });
            b.life[i] = 0;
            break;
          }
        }
        if (b.life[i] <= 0) b.remove(i);
        continue;
      }
      if (p.dead) continue;
      const dx = b.x[i] - p.x;
      const dy = b.y[i] - p.y;
      const d2 = dx * dx + dy * dy;
      const hr = b.r[i] + PLAYER.radius;
      if (d2 < hr * hr && !p.invulnerable) {
        this.hurtPlayer(b.x[i], b.y[i]);
        b.remove(i);
        continue;
      }
      const gr = b.r[i] + PLAYER.graze;
      if (b.flags[i] === 0 && d2 < gr * gr) {
        b.flags[i] = 1;
        this.stats.grazes++;
        this.gain(5, this.mods.has.has('frolement') ? 4.4 : 2.2, 10);
        this.emit({ t: 'graze', x: b.x[i], y: b.y[i] });
      }
    }
  }

  private updatePickups(): void {
    const p = this.player;
    const reach = 3.2 * this.mods.magnet;
    for (let i = this.pickups.length - 1; i >= 0; i--) {
      const q = this.pickups[i];
      q.t++;
      const dx = p.x - q.x;
      const dy = p.y - q.y;
      const d = Math.hypot(dx, dy);
      if (q.t > 20 && (d < reach || q.t > 420 || p.overdrive) && !p.dead) {
        const sp = 30 + q.t * 0.1;
        q.vx += ((dx / d) * sp - q.vx) * 0.2;
        q.vy += ((dy / d) * sp - q.vy) * 0.2;
      } else {
        q.vx *= 0.92;
        q.vy *= 0.92;
      }
      q.x += q.vx / 60;
      q.y += q.vy / 60;
      this.arena.confine(q, 0.3);
      if (d < 0.7 && q.t > 20 && !p.dead) {
        if (q.kind === 'note') {
          this.stats.notes++;
          this.gain(0, 0.4, 5);
          this.emit({ t: 'note', x: q.x, y: q.y });
        } else if (q.kind === 'heal') {
          p.hp = Math.min(p.maxHp, p.hp + 1);
          this.emit({ t: 'heal', x: q.x, y: q.y });
          this.sfx('heal');
        } else if (q.kind === 'fragment') {
          this.emit({ t: 'fragment', x: q.x, y: q.y, id: q.id ?? '' });
          this.sfx('fragment');
        }
        this.pickups.splice(i, 1);
      }
    }
  }
}
