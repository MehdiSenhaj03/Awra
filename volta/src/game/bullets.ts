/**
 * Bullet pools stored as flat arrays (thousands of bullets at once, no garbage).
 * Enemy bullets can curve, accelerate, wait (frozen until a delay runs out) and be reflected by the Pulse.
 */
export const enum BK {
  Orb = 0, // round, glowing
  Shard = 1, // elongated, fast
  Big = 2, // large slow orb
  Note = 3, // musical "note" bullet (boss patterns)
}

export class EnemyBullets {
  readonly max: number;
  n = 0;
  x: Float32Array;
  y: Float32Array;
  vx: Float32Array;
  vy: Float32Array;
  r: Float32Array;
  life: Float32Array;
  acc: Float32Array;
  turn: Float32Array;
  wait: Float32Array;
  color: Uint32Array;
  kind: Uint8Array;
  /** 1 = already counted as a graze; 2 = reflected (now hurts enemies). */
  flags: Uint8Array;
  dmg: Float32Array;

  constructor(max = 4000) {
    this.max = max;
    this.x = new Float32Array(max);
    this.y = new Float32Array(max);
    this.vx = new Float32Array(max);
    this.vy = new Float32Array(max);
    this.r = new Float32Array(max);
    this.life = new Float32Array(max);
    this.acc = new Float32Array(max);
    this.turn = new Float32Array(max);
    this.wait = new Float32Array(max);
    this.color = new Uint32Array(max);
    this.kind = new Uint8Array(max);
    this.flags = new Uint8Array(max);
    this.dmg = new Float32Array(max);
  }

  /** Speeds are in units per second; life in seconds. */
  add(x: number, y: number, a: number, speed: number, o: { r?: number; color?: number; kind?: BK; life?: number; acc?: number; turn?: number; wait?: number } = {}): number {
    if (this.n >= this.max) return -1;
    const i = this.n++;
    this.x[i] = x;
    this.y[i] = y;
    this.vx[i] = Math.cos(a) * speed;
    this.vy[i] = Math.sin(a) * speed;
    this.r[i] = o.r ?? 0.32;
    this.life[i] = o.life ?? 8;
    this.acc[i] = o.acc ?? 0;
    this.turn[i] = o.turn ?? 0;
    this.wait[i] = o.wait ?? 0;
    this.color[i] = o.color ?? 0xff2fb9;
    this.kind[i] = o.kind ?? BK.Orb;
    this.flags[i] = 0;
    this.dmg[i] = 0;
    return i;
  }

  remove(i: number): void {
    const j = --this.n;
    if (i === j) return;
    this.x[i] = this.x[j];
    this.y[i] = this.y[j];
    this.vx[i] = this.vx[j];
    this.vy[i] = this.vy[j];
    this.r[i] = this.r[j];
    this.life[i] = this.life[j];
    this.acc[i] = this.acc[j];
    this.turn[i] = this.turn[j];
    this.wait[i] = this.wait[j];
    this.color[i] = this.color[j];
    this.kind[i] = this.kind[j];
    this.flags[i] = this.flags[j];
    this.dmg[i] = this.dmg[j];
  }

  clear(): void {
    this.n = 0;
  }
}

export const enum SK {
  Bolt = 0,
  Accent = 1,
  Perfect = 2,
  Pellet = 3,
  Lance = 4,
  Missile = 5,
  Arc = 6,
}

/** Player shots. */
export class Shots {
  readonly max: number;
  n = 0;
  x: Float32Array;
  y: Float32Array;
  vx: Float32Array;
  vy: Float32Array;
  dmg: Float32Array;
  life: Float32Array;
  pierce: Uint8Array;
  kind: Uint8Array;
  /** Homing target id (missiles), 0 = none. */
  target: Int32Array;
  /** Last enemy hit (pierce shots do not hit the same enemy twice in a row). */
  last: Int32Array;

  constructor(max = 1500) {
    this.max = max;
    this.x = new Float32Array(max);
    this.y = new Float32Array(max);
    this.vx = new Float32Array(max);
    this.vy = new Float32Array(max);
    this.dmg = new Float32Array(max);
    this.life = new Float32Array(max);
    this.pierce = new Uint8Array(max);
    this.kind = new Uint8Array(max);
    this.target = new Int32Array(max);
    this.last = new Int32Array(max);
  }

  add(x: number, y: number, a: number, speed: number, dmg: number, kind: SK, life = 0.9, pierce = 0): number {
    if (this.n >= this.max) return -1;
    const i = this.n++;
    this.x[i] = x;
    this.y[i] = y;
    this.vx[i] = Math.cos(a) * speed;
    this.vy[i] = Math.sin(a) * speed;
    this.dmg[i] = dmg;
    this.life[i] = life;
    this.pierce[i] = pierce;
    this.kind[i] = kind;
    this.target[i] = 0;
    this.last[i] = 0;
    return i;
  }

  remove(i: number): void {
    const j = --this.n;
    if (i === j) return;
    this.x[i] = this.x[j];
    this.y[i] = this.y[j];
    this.vx[i] = this.vx[j];
    this.vy[i] = this.vy[j];
    this.dmg[i] = this.dmg[j];
    this.life[i] = this.life[j];
    this.pierce[i] = this.pierce[j];
    this.kind[i] = this.kind[j];
    this.target[i] = this.target[j];
    this.last[i] = this.last[j];
  }

  clear(): void {
    this.n = 0;
  }
}
