import { TAU, angleDiff, clamp } from '../engine/core';
import { BK, SK } from './bullets';
import { Enemy } from './enemy';
import { Hazard, segDist } from './hazard';
import { C, fan, ring } from './patterns';
import type { World } from './world';

/** Steering towards a point with a maximum speed. */
function steer(e: Enemy, tx: number, ty: number, speed: number, accel = 0.08): void {
  const dx = tx - e.x;
  const dy = ty - e.y;
  const d = Math.hypot(dx, dy) || 1;
  e.vx += ((dx / d) * speed - e.vx) * accel;
  e.vy += ((dy / d) * speed - e.vy) * accel;
}

/** Grain: a small shard that hunts in swarms. */
export class Mote extends Enemy {
  kind = 'mote';
  look = 'mote';
  private wob: number;
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.45;
    this.hp = this.maxHp = 2;
    this.value = 40;
    this.notes = 1;
    this.wob = Math.random() * TAU;
  }
  update(w: World): void {
    const a = this.angleTo(w) + Math.sin(this.t * 0.05 + this.wob) * 0.6;
    const sp = 6.2 + Math.min(3, this.t / 120);
    steer(this, this.x + Math.cos(a) * 5, this.y + Math.sin(a) * 5, sp, 0.06);
    this.rot += 0.12;
  }
}

/** Bouche: keeps its distance and spits aimed triplets on the beat. */
export class Spitter extends Enemy {
  kind = 'spitter';
  look = 'spitter';
  private side = Math.random() < 0.5 ? 1 : -1;
  private phase = Math.floor(Math.random() * 2);
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.75;
    this.hp = this.maxHp = 7;
    this.value = 110;
    this.notes = 2;
  }
  update(w: World): void {
    const d = this.distTo(w);
    const a = this.angleTo(w);
    const want = d < 8 ? -1 : d > 13 ? 1 : 0;
    const tx = this.x + Math.cos(a) * want * 4 + Math.cos(a + Math.PI / 2) * this.side * 3;
    const ty = this.y + Math.sin(a) * want * 4 + Math.sin(a + Math.PI / 2) * this.side * 3;
    steer(this, tx, ty, 4.2, 0.05);
    this.rot = a;
    if (w.beat.crossed(1) && (w.beat.index(1) + this.phase) % 2 === 0 && this.t > 40) {
      const n = this.elite ? 5 : 3;
      fan(w, this.x, this.y, a, n, this.elite ? 0.7 : 0.42, 10.5, { color: C.magenta });
      w.emit({ t: 'ring', x: this.x, y: this.y, r: 1.2, color: C.magenta, life: 12 });
      w.sfx('enemyShot', 1, 0.5);
    }
    if (Math.random() < 0.004) this.side *= -1;
  }
}

/** Toupie: a spinning ring that sprays a double spiral on every eighth. */
export class Spinner extends Enemy {
  kind = 'spinner';
  look = 'spinner';
  private a = Math.random() * TAU;
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.95;
    this.hp = this.maxHp = 14;
    this.value = 190;
    this.notes = 3;
    this.heavy = true;
  }
  update(w: World): void {
    steer(this, w.player.x * 0.4, w.player.y * 0.4, 1.4, 0.02);
    this.spin += 0.05;
    if (w.beat.crossed(2) && this.t > 60) {
      const arms = this.elite ? 4 : 2;
      for (let i = 0; i < arms; i++) w.bullets.add(this.x, this.y, this.a + (i / arms) * TAU, 7.5, { color: C.violet, r: 0.3 });
      this.a += 0.42;
      if (w.beat.crossed(1)) w.sfx('enemyShot', 0.8, 0.35);
    }
  }
}

/** Lance: aims a warning line for one beat, then charges straight through it. */
export class Lancer extends Enemy {
  kind = 'lancer';
  look = 'lancer';
  private tx = 0;
  private ty = 0;
  private warn: Hazard | null = null;
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.65;
    this.hp = this.maxHp = 8;
    this.value = 150;
    this.notes = 2;
    this.state = 'roam';
  }
  update(w: World): void {
    const spb = w.beat.spb * 60;
    if (this.state === 'roam') {
      steer(this, w.player.x, w.player.y, 3, 0.03);
      this.rot = this.angleTo(w);
      if (this.st > spb * 2 && w.beat.crossed(1)) {
        const a = this.angleTo(w);
        this.tx = this.x + Math.cos(a) * 22;
        this.ty = this.y + Math.sin(a) * 22;
        this.rot = a;
        this.warn = new Hazard('line', this.x, this.y, this.tx, this.ty, 0.5, spb, C.red);
        w.addHazard(this.warn);
        this.to('aim');
        w.sfx('telegraph', 1.2, 0.5);
      }
    } else if (this.state === 'aim') {
      this.vx *= 0.8;
      this.vy *= 0.8;
      if (this.warn) {
        this.warn.x = this.x;
        this.warn.y = this.y;
      }
      if (this.st >= spb) {
        this.to('charge');
        w.sfx('lunge', 1, 0.6);
      }
    } else if (this.state === 'charge') {
      const a = this.rot;
      this.vx = Math.cos(a) * 34;
      this.vy = Math.sin(a) * 34;
      if (this.st > 26 || !w.arena.inside(this.x + Math.cos(a) * 1.2, this.y + Math.sin(a) * 1.2, this.r)) {
        this.to('recover');
        if (this.elite) fan(w, this.x, this.y, a + Math.PI, 7, 1.6, 8, { color: C.red });
      }
    } else if (this.state === 'recover') {
      this.vx *= 0.85;
      this.vy *= 0.85;
      if (this.st > spb) this.to('roam');
    }
  }
}

/** Prisme: splits into smaller copies twice. */
export class Splitter extends Enemy {
  kind = 'splitter';
  look = 'splitter';
  constructor(x: number, y: number, public gen = 0) {
    super(x, y);
    this.r = [1.0, 0.7, 0.48][gen];
    this.hp = this.maxHp = [10, 4, 2][gen];
    this.value = [90, 50, 30][gen];
    this.notes = 1;
    this.color = C.cyan;
    if (gen > 0) this.spawnT = 0;
  }
  update(w: World): void {
    steer(this, w.player.x, w.player.y, [2.6, 4.2, 5.8][this.gen], 0.04);
    this.rot += 0.03 + this.gen * 0.03;
    if (this.gen === 0 && w.beat.crossed(1) && w.beat.index(1) % 4 === 0 && this.t > 60) ring(w, this.x, this.y, 8, 6, this.rot, { color: C.cyan });
  }
  onDeath(w: World): void {
    if (this.gen >= 2) return;
    for (let i = 0; i < 3; i++) {
      const a = (i / 3) * TAU + this.rot;
      const s = w.add(new Splitter(this.x + Math.cos(a) * 0.6, this.y + Math.sin(a) * 0.6, this.gen + 1));
      s.vx = Math.cos(a) * 9;
      s.vy = Math.sin(a) * 9;
    }
  }
}

/**
 * Rempart: a shield bearer. Shots from the front are blocked — flank it, stun it with a Pulse, or crack the
 * shield with accented shots (on the beat): six cracks and it shatters for two bars.
 */
export class Bulwark extends Enemy {
  kind = 'bulwark';
  look = 'bulwark';
  shieldA = 0;
  guard = 6;
  /** Frames left with the shield broken. */
  broken = 0;
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 1.05;
    this.hp = this.maxHp = 26;
    this.value = 320;
    this.notes = 5;
    this.heavy = true;
    this.color = C.gold;
  }
  update(w: World): void {
    steer(this, w.player.x, w.player.y, 2.2, 0.03);
    // The shield turns slowly: flanking works.
    this.shieldA += clamp(angleDiff(this.shieldA, this.angleTo(w)), -0.022, 0.022);
    this.rot = this.shieldA;
    if (this.broken > 0 && --this.broken === 0) {
      this.guard = this.elite ? 9 : 6;
      w.emit({ t: 'ring', x: this.x, y: this.y, r: 2, color: C.gold, life: 20 });
    }
    if (w.beat.crossed(1) && w.beat.index(1) % 4 === 2 && this.t > 60) {
      fan(w, this.x, this.y, this.shieldA, this.elite ? 11 : 7, 1.3, 8.5, { color: C.gold, kind: BK.Big, r: 0.42 });
      w.sfx('enemyShot', 0.6, 0.6);
    }
  }
  blocks(w: World, fx: number, fy: number, kind?: SK): boolean {
    if (this.stun > 0 || this.broken > 0) return false;
    if (Math.abs(angleDiff(this.shieldA, Math.atan2(fy - this.y, fx - this.x))) >= 1.05) return false;
    if (kind === SK.Accent || kind === SK.Perfect || kind === SK.Lance) {
      this.guard -= kind === SK.Accent ? 1 : 2;
      w.emit({ t: 'hit', x: this.x + Math.cos(this.shieldA) * this.r, y: this.y + Math.sin(this.shieldA) * this.r, color: C.gold, big: true });
      if (this.guard <= 0) {
        this.broken = Math.round(w.beat.spb * 60 * 8);
        this.flash = 1;
        w.emit({ t: 'boom', x: this.x, y: this.y, r: 2.5, color: C.gold });
        w.emit({ t: 'judge', x: this.x, y: this.y, judge: 'perfect', what: 'break' });
        w.sfx('bigBoom', 1.4, 0.6);
        return false;
      }
    }
    return true;
  }
}

/** Cloche: lobs shells that land on the next bar, then burst. */
export class Mortar extends Enemy {
  kind = 'mortar';
  look = 'mortar';
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.9;
    this.hp = this.maxHp = 11;
    this.value = 200;
    this.notes = 3;
    this.heavy = true;
    this.color = C.orange;
  }
  update(w: World): void {
    steer(this, -w.player.x * 0.3, -w.player.y * 0.3, 1.2, 0.02);
    this.rot += 0.01;
    if (w.beat.crossed(1) && w.beat.index(1) % 8 === 0 && this.t > 30) {
      const n = this.elite ? 4 : 3;
      const p = w.player;
      for (let i = 0; i < n; i++) {
        const a = w.rng.range(0, TAU);
        const d = i === 0 ? 0 : w.rng.range(2.5, 5.5);
        const x = p.x + p.vx * 0.4 + Math.cos(a) * d;
        const y = p.y + p.vy * 0.4 + Math.sin(a) * d;
        w.addHazard(new Hazard('zone', x, y, this.x, this.y, 2.1, Math.round(w.beat.spb * 60 * 4), C.orange, 6));
      }
      w.emit({ t: 'ring', x: this.x, y: this.y, r: 1.6, color: C.orange, life: 16 });
      w.sfx('mortarLaunch');
    }
  }
}

/** Œil: blinks to the edge, tracks with a laser for three beats, locks, fires a piercing column on the bar. */
export class Sniper extends Enemy {
  kind = 'sniper';
  look = 'sniper';
  private beam: Hazard | null = null;
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.6;
    this.hp = this.maxHp = 6;
    this.value = 190;
    this.notes = 2;
    this.color = C.red;
    this.state = 'move';
  }
  update(w: World): void {
    const spb = w.beat.spb * 60;
    this.vx *= 0.9;
    this.vy *= 0.9;
    if (this.state === 'move') {
      if (this.st > spb * 2 && w.beat.crossed(1) && w.beat.index(1) % 4 === 0) {
        this.beam = new Hazard('line', this.x, this.y, 0, 0, 0.18, spb * 4, C.red);
        w.addHazard(this.beam);
        this.to('track');
        w.sfx('telegraph', 0.8, 0.5);
      }
    } else if (this.state === 'track') {
      const lock = this.st > spb * 3;
      if (!lock) this.rot = this.angleTo(w);
      if (this.beam) {
        this.beam.x = this.x;
        this.beam.y = this.y;
        this.beam.x2 = this.x + Math.cos(this.rot) * 60;
        this.beam.y2 = this.y + Math.sin(this.rot) * 60;
        this.beam.r = lock ? 0.5 : 0.12;
      }
      if (this.st >= spb * 4) {
        for (let i = 0; i < (this.elite ? 9 : 6); i++) w.bullets.add(this.x, this.y, this.rot, 30 + i * 2.2, { color: C.red, kind: BK.Shard, r: 0.34 });
        w.emit({ t: 'ring', x: this.x, y: this.y, r: 1.6, color: C.red, life: 10 });
        w.sfx('sniper');
        this.to('relocate');
      }
    } else if (this.state === 'relocate') {
      if (this.st > spb) {
        const e = w.arena.edge(w.rng.range(0, TAU), 3);
        w.emit({ t: 'spawn', x: this.x, y: this.y, color: C.red, size: 0.6 });
        this.x = e.x;
        this.y = e.y;
        w.emit({ t: 'spawn', x: this.x, y: this.y, color: C.red, size: 0.6 });
        this.to('move');
      }
    }
  }
}

/** Miroir: reflects shots, except on the beat when its face opens. */
export class Mirror extends Enemy {
  kind = 'mirror';
  look = 'mirror';
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.85;
    this.hp = this.maxHp = 10;
    this.value = 260;
    this.notes = 3;
    this.color = C.white;
  }
  get open(): boolean {
    return this.openPhase(this.lastBeat) || this.stun > 0;
  }
  private lastBeat = 0;
  private openPhase(b: number): boolean {
    const f = b - Math.floor(b);
    return f < 0.2 || f > 0.9;
  }
  update(w: World): void {
    this.lastBeat = w.beat.beat;
    const a = this.angleTo(w);
    steer(this, w.player.x - Math.cos(a) * 9, w.player.y - Math.sin(a) * 9, 3.5, 0.04);
    this.rot = a;
    this.spin = this.open ? 1 : 0;
  }
  blocks(w: World, fx: number, fy: number): boolean {
    this.lastBeat = w.beat.beat;
    if (this.open) return false;
    // Sends the shot back at the hero.
    const a = Math.atan2(w.player.y - this.y, w.player.x - this.x);
    w.bullets.add(this.x + Math.cos(a) * this.r, this.y + Math.sin(a) * this.r, a + (Math.random() - 0.5) * 0.2, 12, { color: C.white, kind: BK.Shard, r: 0.26 });
    void fx;
    void fy;
    return true;
  }
}

/** Écho: only solid on every other beat; bursts into a ring when it appears. */
export class Phantom extends Enemy {
  kind = 'phantom';
  look = 'phantom';
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.7;
    this.hp = this.maxHp = 7;
    this.value = 230;
    this.notes = 3;
    this.color = C.violet;
  }
  update(w: World): void {
    const b = w.beat.beat;
    const solid = Math.floor(b) % 2 === 0;
    this.vis += ((solid ? 1 : 0.08) - this.vis) * 0.3;
    this.hurtable = this.targetable = solid;
    this.contact = solid;
    if (!solid) {
      const a = this.angleTo(w) + Math.sin(this.t * 0.03) * 1.2;
      steer(this, w.player.x - Math.cos(a) * 6, w.player.y - Math.sin(a) * 6, 9, 0.08);
    } else {
      this.vx *= 0.85;
      this.vy *= 0.85;
    }
    if (w.beat.crossed(1) && solid && this.t > 40) {
      ring(w, this.x, this.y, this.elite ? 14 : 10, 6.5, w.rng.range(0, TAU), { color: C.violet });
      w.sfx('phase', 1, 0.5);
    }
  }
}

/** Chœur: binds up to three allies with beams of light that soak most of their damage. */
export class Choir extends Enemy {
  kind = 'choir';
  look = 'choir';
  links: number[] = [];
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 0.85;
    this.hp = this.maxHp = 16;
    this.value = 280;
    this.notes = 4;
    this.color = C.green;
  }
  update(w: World): void {
    // Hides behind its allies.
    const a = this.angleTo(w);
    steer(this, w.player.x - Math.cos(a) * 14, w.player.y - Math.sin(a) * 14, 3, 0.03);
    this.spin += 0.03;
    if (this.t % 30 < 1) {
      this.links = w.enemies
        .filter((e) => e !== this && e.active && e.kind !== 'choir' && !e.boss)
        .sort((p, q) => Math.hypot(p.x - this.x, p.y - this.y) - Math.hypot(q.x - this.x, q.y - this.y))
        .slice(0, this.elite ? 5 : 3)
        .map((e) => e.id);
    }
    for (const id of this.links) {
      const e = w.enemyById(id);
      if (e && e.active) e.shielded = 3;
    }
  }
}

/** Tisseuse: sweeps a beam of light around itself, one revolution every two bars. */
export class Weaver extends Enemy {
  kind = 'weaver';
  look = 'weaver';
  private beams: Hazard[] = [];
  private a = 0;
  constructor(x: number, y: number) {
    super(x, y);
    this.r = 1.0;
    this.hp = this.maxHp = 20;
    this.value = 340;
    this.notes = 5;
    this.heavy = true;
    this.color = C.cyan;
  }
  update(w: World): void {
    steer(this, w.player.x * 0.2, w.player.y * 0.2, 1, 0.01);
    if (this.beams.length === 0 && this.t > 30) {
      const n = this.elite ? 3 : 2;
      for (let i = 0; i < n; i++) {
        const h = new Hazard('beam', this.x, this.y, this.x, this.y, 0.28, Math.round(w.beat.spb * 60 * 2), C.cyan, 1e9);
        this.beams.push(h);
        w.addHazard(h);
      }
    }
    this.a += (TAU / (w.beat.spb * 60 * 8)) * (this.elite ? 1.2 : 1) * w.k;
    this.spin = this.a;
    const len = 9;
    this.beams.forEach((h, i) => {
      const a = this.a + (i / this.beams.length) * TAU;
      h.x = this.x + Math.cos(a) * this.r;
      h.y = this.y + Math.sin(a) * this.r;
      h.x2 = this.x + Math.cos(a) * (this.r + len);
      h.y2 = this.y + Math.sin(a) * (this.r + len);
    });
  }
  onDeath(): void {
    for (const h of this.beams) h.done = true;
  }
}

/** A golden echo carrying a memory: it flees, then vanishes. Kill it fast. */
export class GoldenEcho extends Enemy {
  kind = 'echo';
  look = 'echo';
  constructor(x: number, y: number, public fragment: string) {
    super(x, y);
    this.r = 0.55;
    this.hp = this.maxHp = 9;
    this.value = 500;
    this.notes = 6;
    this.color = C.gold;
    this.contact = false;
  }
  update(w: World): void {
    const a = this.angleTo(w) + Math.PI + Math.sin(this.t * 0.04) * 0.9;
    steer(this, this.x + Math.cos(a) * 5, this.y + Math.sin(a) * 5, 8, 0.08);
    this.rot += 0.1;
    if (this.t > w.beat.spb * 60 * 12) {
      this.vis -= 0.05;
      this.hurtable = this.vis > 0.3;
      if (this.vis <= 0) this.dead = true;
    }
  }
  onDeath(w: World): void {
    w.drop(this.x, this.y, 1, 'fragment', this.fragment);
  }
}

export const ENEMY_KINDS: Record<string, (x: number, y: number) => Enemy> = {
  mote: (x, y) => new Mote(x, y),
  spitter: (x, y) => new Spitter(x, y),
  spinner: (x, y) => new Spinner(x, y),
  lancer: (x, y) => new Lancer(x, y),
  splitter: (x, y) => new Splitter(x, y),
  bulwark: (x, y) => new Bulwark(x, y),
  mortar: (x, y) => new Mortar(x, y),
  sniper: (x, y) => new Sniper(x, y),
  mirror: (x, y) => new Mirror(x, y),
  phantom: (x, y) => new Phantom(x, y),
  choir: (x, y) => new Choir(x, y),
  weaver: (x, y) => new Weaver(x, y),
};

export { segDist, SK };
