import { type Box, approach, clamp } from '../engine/core';
import { type Body, type Ledge, type MoveResult, fits, inWater, makeBody, move, onPlatform, standing } from '../engine/physics';
import type { Room } from '../world/room';
import type { Controls } from './controls';

/** Movement tuning, in pixels per frame at 60 updates per second (hero hitbox 34×84). */
export const PHYS = {
  run: 9.4,
  accelGround: 2.6,
  accelAir: 1.9,
  decelGround: 3.4,
  gravity: 1.4,
  apexGravity: 0.75,
  maxFall: 25,
  jumpSpeed: 20,
  jumpHold: 15,
  doubleSpeed: 17.5,
  doubleHold: 8,
  coyote: 6,
  buffer: 8,
  dashSpeed: 26,
  dashFrames: 10,
  dashCooldown: 18,
  wallSlide: 6.5,
  wallJumpX: 12.5,
  wallJumpY: 18.5,
  wallLock: 9,
  pogo: 17.5,
  hurtFrames: 14,
  invuln: 75,
};

/** The three-hit combo: total frames, active window, window to chain, damage multiplier. */
export const SLASH = [
  { total: 17, from: 2, to: 6, chain: 7, cancel: 5, mul: 1, reach: 150, height: 104 },
  { total: 17, from: 2, to: 6, chain: 7, cancel: 5, mul: 1, reach: 150, height: 104 },
  { total: 24, from: 3, to: 8, chain: 99, cancel: 9, mul: 1.6, reach: 178, height: 128 },
];

export type Pose = 'wake' | 'idle' | 'run' | 'jump' | 'fall' | 'land' | 'dash' | 'wall' | 'slash' | 'slashUp' | 'slashDown' | 'throw' | 'heal' | 'hurt' | 'art' | 'dead' | 'blink';

export type AttackDir = 'side' | 'up' | 'down';

export interface Attack {
  id: number;
  dir: AttackDir;
  combo: number;
  t: number;
  total: number;
  from: number;
  to: number;
  chain: number;
  cancel: number;
  mul: number;
  reach: number;
  height: number;
  /** Bare-handed palm strike while the blade is away. */
  palm: boolean;
  /** Something was struck by this attack. */
  landed: boolean;
}

export type PlayerEvent =
  | { kind: 'jump' | 'doubleJump' | 'land' | 'wallJump' | 'dash' | 'throw' | 'recall' | 'blink' | 'healStart' | 'healed' | 'footstep' | 'pogo' | 'art' | 'dashWall' | 'zip' | 'zipEnd'; x?: number; y?: number; dir?: number; big?: boolean; art?: string }
  | { kind: 'attack'; attack: Attack; facing: number };

export interface PlayerWorld {
  room: Room;
  ledges: Ledge[];
  has(ability: string): boolean;
  /** The blade is in hand. */
  bladeHeld(): boolean;
  bladeOut(): boolean;
  throwBlade(dirX: number, dirY: number): void;
  recallBlade(): void;
  /** Teleport target when the blade can be reached, else null. */
  blinkTarget(): { x: number; y: number; execute: boolean; zip?: boolean } | null;
  /** Dash length multiplier (Comet Wake). */
  dashMul(): number;
  seal(name: string): boolean;
  gainLumen(n: number): void;
  afterBlink(execute: boolean): void;
  spendLumen(n: number): boolean;
  heal(): void;
  canHeal(): boolean;
  castArt(dir: AttackDir): boolean;
}

let attackSeq = 1;

export class Player {
  body: Body;
  facing = 1;
  pose: Pose = 'idle';
  poseT = 0;
  /** Frames left on each timer. */
  coyote = 0;
  jumpBuf = 0;
  attackBuf = 0;
  dashBuf = 0;
  bladeBuf = 0;
  jumpHoldT = 0;
  jumping = false;
  doubleUsed = false;
  airDashUsed = false;
  dashT = 0;
  dashCd = 0;
  dashDir = 1;
  wallLockT = 0;
  attack: Attack | null = null;
  combo = 0;
  comboT = 0;
  throwT = 0;
  healT = 0;
  hurtT = 0;
  invuln = 0;
  artT = 0;
  /** An art moves the body (no gravity from the player). */
  artFree = false;
  artHoldT = 0;
  landT = 0;
  recoilT = 0;
  recoilV = 0;
  hp = 5;
  maxHp = 5;
  dead = false;
  /** Frames since the last footstep event. */
  private stepT = 0;
  /** Frames left of the awakening at the start of a new game. */
  wakeT = 0;
  /** A down-strike bounced this frame (Rebound seal). */
  pogoed = false;
  /** Being pulled to an astral ring. */
  zip: { x: number; y: number; t: number } | null = null;
  /** Last safe ground, for hazard respawns. */
  safe = { x: 0, y: 0 };
  /** Position history for afterimages (render only reads it). */
  trail: { x: number; y: number; pose: Pose; facing: number }[] = [];

  constructor(cx: number, feet: number) {
    this.body = makeBody(cx, feet, 34, 84);
    this.safe = { x: this.body.x, y: this.body.y };
  }

  get cx(): number {
    return this.body.x + this.body.w / 2;
  }

  get cy(): number {
    return this.body.y + this.body.h / 2;
  }

  get feet(): number {
    return this.body.y + this.body.h;
  }

  box(): Box {
    return { x: this.body.x + 4, y: this.body.y + 6, w: this.body.w - 8, h: this.body.h - 8 };
  }

  /** The hitbox of the current attack while it is active. */
  attackBox(): Box | null {
    const a = this.attack;
    if (!a || a.t < a.from || a.t > a.to) return null;
    const reach = a.palm ? a.reach * 0.6 : a.reach;
    const b = this.body;
    if (a.dir === 'up') return { x: this.cx - a.height / 2, y: b.y - reach + 30, w: a.height, h: reach };
    if (a.dir === 'down') return { x: this.cx - a.height / 2, y: this.feet - 30, w: a.height, h: reach };
    const y = this.cy - a.height / 2 - 6;
    return this.facing > 0 ? { x: this.cx, y, w: reach, h: a.height } : { x: this.cx - reach, y, w: reach, h: a.height };
  }

  busy(): boolean {
    return this.dead || this.hurtT > 0 || this.healT > 0 || this.artT > 0;
  }

  setPose(p: Pose): void {
    if (this.pose !== p) {
      this.pose = p;
      this.poseT = 0;
    }
  }

  /** Called by the scene when an attack connects with something solid or alive. */
  onAttackLanded(target: 'foe' | 'spike' | 'wall', bounce: boolean): void {
    const a = this.attack;
    if (!a) return;
    a.landed = true;
    if (a.dir === 'down' && bounce) {
      this.body.vy = -PHYS.pogo;
      this.pogoed = true;
      this.jumping = false;
      this.doubleUsed = false;
      this.airDashUsed = false;
      this.dashCd = 0;
    } else if (a.dir === 'side' && target !== 'spike') {
      // Recoil: a short push back, so hits feel solid and spacing stays honest.
      this.recoilT = 5;
      this.recoilV = -this.facing * (target === 'wall' ? 5 : 6.5);
    }
  }

  hurt(fromX: number): void {
    this.hurtT = PHYS.hurtFrames;
    this.invuln = PHYS.invuln;
    this.attack = null;
    this.healT = 0;
    this.dashT = 0;
    this.artT = 0;
    const dir = Math.sign(this.cx - fromX) || -this.facing;
    this.body.vx = dir * 9;
    this.body.vy = -11;
    this.jumping = false;
    this.setPose('hurt');
  }

  update(c: Controls, w: PlayerWorld): PlayerEvent[] {
    const ev: PlayerEvent[] = [];
    const b = this.body;
    this.poseT++;
    if (this.invuln > 0) this.invuln--;
    if (this.dashCd > 0) this.dashCd--;
    if (this.landT > 0) this.landT--;
    if (this.comboT > 0) this.comboT--;
    else this.combo = 0;
    if (c.jumpPressed) this.jumpBuf = PHYS.buffer;
    else if (this.jumpBuf > 0) this.jumpBuf--;
    if (c.attackPressed) this.attackBuf = PHYS.buffer;
    else if (this.attackBuf > 0) this.attackBuf--;
    if (c.dashPressed) this.dashBuf = 6;
    else if (this.dashBuf > 0) this.dashBuf--;
    if (c.bladePressed) this.bladeBuf = 6;
    else if (this.bladeBuf > 0) this.bladeBuf--;

    if (this.wakeT > 0) {
      this.wakeT--;
      this.setPose('wake');
      b.vy = Math.min(b.vy + PHYS.gravity, PHYS.maxFall);
      move(w.room, b, 0, b.vy, w.ledges);
      return ev;
    }
    if (this.dead) {
      b.vx = approach(b.vx, 0, 1);
      b.vy = Math.min(b.vy + PHYS.gravity, PHYS.maxFall);
      move(w.room, b, b.vx, b.vy, w.ledges);
      this.setPose('dead');
      return ev;
    }

    if (b.onGround) {
      this.coyote = PHYS.coyote;
      this.doubleUsed = false;
      this.airDashUsed = false;
    } else if (this.coyote > 0) this.coyote--;

    // Hurt: knocked back, no control for a few frames.
    if (this.hurtT > 0) {
      this.hurtT--;
      b.vy = Math.min(b.vy + PHYS.gravity, PHYS.maxFall);
      b.vx = approach(b.vx, 0, 0.5);
      move(w.room, b, b.vx, b.vy, w.ledges);
      return ev;
    }

    // Healing: stand still and hold.
    if (this.healT > 0 || (c.healHeld && b.onGround && !this.attack && this.dashT === 0 && w.canHeal())) {
      if (c.healHeld && b.onGround && w.canHeal()) {
        if (this.healT === 0) ev.push({ kind: 'healStart' });
        this.healT++;
        b.vx = approach(b.vx, 0, PHYS.decelGround);
        move(w.room, b, b.vx, 0, w.ledges);
        this.setPose('heal');
        if (this.healT >= (w.seal('sang') ? 34 : 52)) {
          this.healT = 1;
          w.heal();
          ev.push({ kind: 'healed' });
        }
        return ev;
      }
      this.healT = 0;
    }

    // Arts.
    if (this.artT > 0 || this.artFree) {
      if (this.artT > 0) this.artT--;
      if (!this.artFree) {
        b.vx = approach(b.vx, 0, 1.2);
        b.vy = this.artT > 6 ? Math.min(b.vy + 0.3, 2) : Math.min(b.vy + PHYS.gravity, PHYS.maxFall);
      }
      move(w.room, b, b.vx, b.vy, w.ledges);
      this.setPose('art');
      return ev;
    }
    // Arts: up/down fire on press; the neutral one on release (a long hold with full Lumen: Constellation).
    if (c.artHeld) this.artHoldT++;
    const wantDir: AttackDir = c.y < 0 ? 'up' : c.y > 0 && !b.onGround ? 'down' : 'side';
    let cast: AttackDir | 'ultimate' | null = null;
    if (c.artPressed && wantDir !== 'side') cast = wantDir;
    if (!c.artHeld && this.artHoldT > 0) {
      if (this.artHoldT < 30 && wantDir === 'side') cast = 'side';
      this.artHoldT = 0;
    }
    if (c.artHeld && this.artHoldT === 45) cast = 'ultimate';
    if (cast && !this.attack && this.dashT === 0) {
      if (w.castArt(cast as AttackDir)) {
        if (cast === 'ultimate') this.artHoldT = 0;
        this.artT = cast === 'down' ? 4 : 22;
        this.setPose('art');
        ev.push({ kind: 'art', dir: this.facing, art: cast });
        return ev;
      }
    }

    // Pulled to a ring: fast straight flight, released with momentum.
    if (this.zip) {
      const z = this.zip;
      z.t++;
      const dx = z.x - this.cx;
      const dy = z.y - this.cy;
      const d = Math.hypot(dx, dy);
      const sp = Math.min(34, 12 + z.t * 3);
      b.vx = (dx / (d || 1)) * sp;
      b.vy = (dy / (d || 1)) * sp;
      const r = move(w.room, b, b.vx, b.vy, w.ledges);
      this.pushTrail();
      this.setPose('dash');
      if (d < sp + 30 || r.hitX || r.hitY || z.t > 50) {
        this.zip = null;
        b.vx *= 0.75;
        b.vy = Math.min(b.vy * 0.8, -12);
        this.doubleUsed = false;
        this.airDashUsed = false;
        ev.push({ kind: 'zipEnd', x: this.cx, y: this.cy });
      }
      return ev;
    }

    // Blink to the blade (or zip to a ring).
    if (c.blinkPressed && w.has('blink')) {
      const t = w.blinkTarget();
      if (t && t.zip) {
        this.zip = { x: t.x, y: t.y, t: 0 };
        this.attack = null;
        this.dashT = 0;
        ev.push({ kind: 'zip', x: this.cx, y: this.cy });
        return ev;
      }
      if (t) {
        const from = { x: this.cx, y: this.cy };
        const spot = findSpot(w.room, t.x - b.w / 2, t.y - b.h / 2, b.w, b.h);
        if (spot) {
          b.x = spot.x;
          b.y = spot.y;
          b.vx = 0;
          b.vy = t.execute ? -14 : -3;
          this.dashT = 0;
          this.attack = null;
          this.jumping = false;
          this.doubleUsed = false;
          this.airDashUsed = false;
          this.dashCd = 0;
          this.invuln = Math.max(this.invuln, 10);
          this.setPose('blink');
          ev.push({ kind: 'blink', x: from.x, y: from.y, big: t.execute });
          w.afterBlink(t.execute);
          return ev;
        }
      }
    }

    // Throw or recall the blade.
    if (this.bladeBuf > 0 && this.dashT === 0 && (!this.attack || this.attack.t >= this.attack.cancel)) {
      if (w.bladeHeld()) {
        this.bladeBuf = 0;
        let dx = this.facing;
        let dy = 0;
        if (c.y < 0) { dy = -1; dx = c.x !== 0 ? Math.sign(c.x) * 1 : 0; }
        else if (c.y > 0 && !b.onGround) { dy = 1; dx = c.x !== 0 ? Math.sign(c.x) : 0; }
        if (c.x !== 0) this.facing = Math.sign(c.x);
        if (dx !== 0 && dy !== 0) { dx *= Math.SQRT1_2; dy *= Math.SQRT1_2; }
        w.throwBlade(dx, dy);
        this.attack = null;
        this.throwT = 12;
        this.setPose('throw');
        ev.push({ kind: 'throw', dir: this.facing });
      } else if (w.bladeOut()) {
        this.bladeBuf = 0;
        w.recallBlade();
        ev.push({ kind: 'recall' });
      }
    }
    if (this.throwT > 0) this.throwT--;

    // Dash.
    const canDash = w.has('dash') && this.dashCd === 0 && this.dashT === 0 && (b.onGround || !this.airDashUsed);
    if (this.dashBuf > 0 && canDash && (!this.attack || this.attack.t >= this.attack.cancel)) {
      this.dashBuf = 0;
      if (c.x !== 0) this.facing = Math.sign(c.x);
      if (b.wall !== 0 && !b.onGround) this.facing = -b.wall;
      this.dashDir = this.facing;
      this.dashT = Math.round(PHYS.dashFrames * w.dashMul());
      this.dashCd = PHYS.dashCooldown;
      if (!b.onGround) this.airDashUsed = true;
      this.attack = null;
      this.jumping = false;
      ev.push({ kind: 'dash', dir: this.dashDir });
    }
    if (this.dashT > 0) {
      this.dashT--;
      b.vx = this.dashDir * PHYS.dashSpeed;
      b.vy = 0;
      const r = move(w.room, b, b.vx, 0, w.ledges);
      if (r.hitX) {
        this.dashT = 0;
        ev.push({ kind: 'dashWall', x: this.dashDir > 0 ? b.x + b.w + 6 : b.x - 6, y: this.cy, dir: this.dashDir });
      }
      if (this.dashT === 0) b.vx = this.dashDir * PHYS.run;
      this.setPose('dash');
      this.pushTrail();
      return ev;
    }

    // Attacks.
    if (this.attack) {
      const a = this.attack;
      a.t++;
      const quick = w.seal('braise') ? 3 : 0;
      if (a.t >= a.total - quick) this.attack = null;
      else if (a.t >= a.chain - quick && this.attackBuf > 0 && a.dir === 'side' && this.combo < 3) this.attack = null;
    }
    if (!this.attack && this.attackBuf > 0 && this.throwT === 0) {
      this.attackBuf = 0;
      const dir: AttackDir = c.y < 0 ? 'up' : c.y > 0 && !b.onGround ? 'down' : 'side';
      if (c.x !== 0 && dir === 'side') this.facing = Math.sign(c.x);
      let spec = SLASH[0];
      if (dir === 'side') {
        this.combo = this.comboT > 0 ? (this.combo % 3) + 1 : 1;
        spec = SLASH[this.combo - 1];
        this.comboT = spec.total + 14;
      } else {
        this.combo = 0;
        this.comboT = 0;
      }
      const palm = !w.bladeHeld();
      this.attack = {
        id: attackSeq++, dir, combo: dir === 'side' ? this.combo : 1, t: 0, total: dir === 'side' ? spec.total : 18,
        from: spec.from, to: spec.to + (dir === 'side' ? 0 : 1), chain: spec.chain, cancel: spec.cancel, mul: (palm ? 0.45 : 1) * (dir === 'side' ? spec.mul : 1),
        reach: dir === 'side' ? spec.reach : 150, height: dir === 'side' ? spec.height : 130, palm, landed: false,
      };
      ev.push({ kind: 'attack', attack: this.attack, facing: this.facing });
      // The finisher lunges forward a little.
      if (dir === 'side' && this.combo === 3 && b.onGround) b.vx = this.facing * 13;
    }

    // Horizontal movement.
    const wantX = this.wallLockT > 0 ? 0 : c.x;
    if (this.wallLockT > 0) this.wallLockT--;
    const attacking = !!this.attack && this.attack.dir === 'side' && b.onGround;
    const wet = inWater(w.room, b);
    const target = wantX * PHYS.run * (attacking ? 0.35 : 1) * (w.seal('coureur') ? 1.15 : 1) * (wet ? 0.7 : 1);
    if (this.wallLockT === 0) {
      const accel = b.onGround ? (wantX !== 0 ? PHYS.accelGround : PHYS.decelGround) : PHYS.accelAir;
      b.vx = approach(b.vx, target, Math.abs(b.vx) > PHYS.run + 0.5 ? 1.2 : accel);
    }
    if (wantX !== 0 && !this.attack) this.facing = Math.sign(wantX);
    if (this.recoilT > 0) {
      this.recoilT--;
      b.vx = this.recoilV;
    }

    // Wall slide.
    const hasWall = w.has('walljump');
    const onWall = hasWall && !b.onGround && b.wall !== 0 && c.x === b.wall && b.vy > 0;
    if (onWall) {
      b.vy = Math.min(b.vy, PHYS.wallSlide);
      this.doubleUsed = false;
      this.airDashUsed = false;
      this.facing = -b.wall;
    }

    // Jumps (buffered).
    if (this.jumpBuf > 0) {
      if (b.onGround || this.coyote > 0) {
        if (c.y > 0 && onPlatform(w.room, b)) {
          b.dropThrough = true;
          this.jumpBuf = 0;
        } else {
          this.jumpBuf = 0;
          this.coyote = 0;
          this.jumping = true;
          this.jumpHoldT = PHYS.jumpHold;
          b.vy = -PHYS.jumpSpeed;
          ev.push({ kind: 'jump', x: this.cx, y: this.feet });
        }
      } else if (hasWall && (b.wall !== 0 || nearWall(w.room, b) !== 0)) {
        const side = b.wall !== 0 ? b.wall : nearWall(w.room, b);
        this.jumpBuf = 0;
        b.vx = -side * PHYS.wallJumpX;
        b.vy = -PHYS.wallJumpY;
        this.facing = -side;
        this.wallLockT = PHYS.wallLock;
        this.jumping = true;
        this.jumpHoldT = 6;
        ev.push({ kind: 'wallJump', x: side > 0 ? b.x + b.w : b.x, y: this.cy, dir: -side });
      } else if (w.has('double') && !this.doubleUsed) {
        this.jumpBuf = 0;
        this.doubleUsed = true;
        this.jumping = true;
        this.jumpHoldT = PHYS.doubleHold;
        b.vy = -PHYS.doubleSpeed;
        ev.push({ kind: 'doubleJump', x: this.cx, y: this.feet });
      }
    }
    // Variable height: constant rise while held, then gravity; releasing early cuts the jump.
    if (this.jumping) {
      // A tap still gives a minimum hop: the first frames of the rise always play.
      const minRise = this.jumpHoldT > PHYS.jumpHold - 8 && !this.doubleUsed;
      if ((c.jumpHeld || minRise) && this.jumpHoldT > 0 && b.vy < 0) {
        this.jumpHoldT--;
        b.vy = Math.min(b.vy, -PHYS.jumpSpeed * (this.doubleUsed && this.jumpHoldT < PHYS.doubleHold ? 0.9 : 1));
      } else {
        if (!c.jumpHeld && b.vy < -7) b.vy = -7;
        this.jumping = false;
        this.jumpHoldT = 0;
      }
    }
    const apex = Math.abs(b.vy) < 3 && c.jumpHeld;
    if (!this.jumping) b.vy = Math.min(b.vy + (apex ? PHYS.apexGravity : PHYS.gravity) * (wet ? 0.6 : 1), onWall ? PHYS.wallSlide : wet ? 8 : PHYS.maxFall);

    const wasGround = b.onGround;
    const r: MoveResult = move(w.room, b, b.vx, b.vy, w.ledges);
    b.dropThrough = false;
    if (r.hitY && b.vy === 0 && !b.onGround) {
      this.jumping = false;
      this.jumpHoldT = 0;
    }
    if (b.onGround && !wasGround) {
      this.landT = 7;
      ev.push({ kind: 'land', x: this.cx, y: this.feet, big: r.landed });
    }
    if (b.onGround && standing(w.room, b) && fits(w.room, b.x, b.y, b.w, b.h)) {
      this.safe = { x: b.x, y: b.y };
    }

    // Pose.
    if (this.attack) this.setPose(this.attack.dir === 'up' ? 'slashUp' : this.attack.dir === 'down' ? 'slashDown' : 'slash');
    else if (this.throwT > 0) this.setPose('throw');
    else if (onWall) this.setPose('wall');
    else if (!b.onGround) this.setPose(b.vy < 0 ? 'jump' : 'fall');
    else if (Math.abs(b.vx) > 1.5) {
      this.setPose('run');
      if (++this.stepT >= 15) {
        this.stepT = 0;
        ev.push({ kind: 'footstep', x: this.cx, y: this.feet });
      }
    } else if (this.landT > 0) this.setPose('land');
    else this.setPose('idle');
    this.pushTrail();
    return ev;
  }

  private pushTrail(): void {
    this.trail.push({ x: this.cx, y: this.feet, pose: this.pose, facing: this.facing });
    if (this.trail.length > 12) this.trail.shift();
  }
}

function nearWall(room: Room, b: Body): number {
  if (!fits(room, b.x - 10, b.y + 4, b.w, b.h - 10)) return -1;
  if (!fits(room, b.x + 10, b.y + 4, b.w, b.h - 10)) return 1;
  return 0;
}

/** Nearest free spot for a box around (x, y), searching outward. */
export function findSpot(room: Room, x: number, y: number, w: number, h: number): { x: number; y: number } | null {
  if (fits(room, x, y, w, h)) return { x, y };
  for (let r = 6; r <= 72; r += 6) {
    for (const [dx, dy] of [[0, -r], [0, r], [-r, 0], [r, 0], [-r, -r], [r, -r], [-r, r], [r, r]]) {
      if (fits(room, x + dx, y + dy, w, h)) return { x: x + dx, y: y + dy };
    }
  }
  return null;
}

export const clampHp = (v: number, max: number) => clamp(v, 0, max);
