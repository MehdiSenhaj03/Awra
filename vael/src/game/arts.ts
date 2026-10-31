import { type Box, TILE, overlap } from '../engine/core';
import { raycast } from '../engine/physics';
import { Entity } from './entity';
import { BASE_DAMAGE, type Scene } from './scene';

/** Base class for art effects: invisible to the hero's blows, hit foes once per "tick". */
abstract class ArtFx extends Entity {
  protected hitT = new Map<number, number>();

  constructor(scene: Scene, x: number, y: number, w: number, h: number) {
    super(scene, x, y + h / 2, w, h);
    this.hurtable = false;
    this.contact = 0;
    this.prop = true;
    this.brandable = false;
    this.body.noClip = true;
  }

  /** Strikes every foe overlapping the box, at most once every `every` frames each. */
  protected sweep(box: Box, dmg: number, every = 999): number {
    let n = 0;
    for (const e of this.scene.entities) {
      if (e === this || e.dead || !e.hurtable || e.prop) continue;
      const last = this.hitT.get(e.id);
      if (last !== undefined && this.t - last < every) continue;
      if (!overlap(box, e.box())) continue;
      this.hitT.set(e.id, this.t);
      if (this.scene.strike(e, dmg * this.scene.damageMul(), this.cx, 'art')) n++;
    }
    return n;
  }
}

/** Comète: a bolt of starlight that bursts on impact. */
export class Comet extends ArtFx {
  constructor(scene: Scene, x: number, y: number, public dir: number, public split = false, public small = false, vy = 0) {
    super(scene, x, y, small ? 30 : 46, small ? 30 : 46);
    this.body.vx = dir * (small ? 22 : 30);
    this.body.vy = vy;
    this.sprite = small ? 'cometSmall' : 'comet';
    this.facing = dir;
  }

  update(): void {
    const b = this.body;
    const nx = this.cx + b.vx;
    const ny = this.cy + b.vy;
    const hit = raycast(this.scene.room, this.cx, this.cy, nx, ny, true);
    b.x += b.vx;
    b.y += b.vy;
    if (this.t % 2 === 0) this.scene.emit({ t: 'spark', x: this.cx - b.vx, y: this.cy, n: 2, color: 0xffe6b0 });
    const struck = this.scene.entities.some((e) => e !== this && !e.dead && e.hurtable && !e.prop && overlap(this.box(), e.box()));
    if (hit || struck || this.t > 70) this.burst();
  }

  private burst(): void {
    this.dead = true;
    const r = this.small ? 90 : 150;
    this.sweep({ x: this.cx - r, y: this.cy - r, w: r * 2, h: r * 2 }, BASE_DAMAGE * (this.small ? 1.4 : 3));
    this.scene.emit({ t: 'boom', x: this.cx, y: this.cy, r: this.small ? 110 : 190, color: 0xffe0a0 });
    if (this.split) {
      for (const vy of [-9, 0, 9]) this.scene.add(new Comet(this.scene, this.cx - this.dir * 20, this.cy, -this.dir, false, true, vy));
    }
  }
}

/** Ascension: a whirl of blades lifting the hero. */
export class Ascension extends ArtFx {
  constructor(scene: Scene, public pillar: boolean) {
    super(scene, scene.player.cx, scene.player.cy, 10, 10);
    this.sprite = 'ascension';
  }

  update(): void {
    const p = this.scene.player;
    this.body.x = p.cx - 5;
    this.body.y = p.cy - 5;
    if (this.t <= 20) {
      p.body.vy = -19;
      p.body.vx *= 0.8;
      p.artT = Math.max(p.artT, 2);
      p.artFree = true;
      this.sweep({ x: p.cx - 140, y: p.cy - 160, w: 280, h: 300 }, BASE_DAMAGE * 0.9, 5);
      if (this.t % 3 === 0) this.scene.emit({ t: 'spin', x: p.cx, y: p.cy, big: this.t === 3 });
    } else {
      p.artFree = false;
      if (!this.pillar) this.dead = true;
      else {
        // Firmament: a pillar of light remains where the whirl rose.
        this.sweep({ x: this.startX - 70, y: this.startY - 520, w: 140, h: 600 }, BASE_DAMAGE * 0.6, 8);
        if (this.t > 70) this.dead = true;
      }
    }
  }

  startX = this.scene.player.cx;
  startY = this.scene.player.cy;
}

/** Chute d'Astre: a dive that ends in a shockwave. */
export class Starfall extends ArtFx {
  landed = false;

  constructor(scene: Scene, public meteor: boolean) {
    super(scene, scene.player.cx, scene.player.cy, 10, 10);
    this.sprite = 'starfall';
  }

  update(): void {
    const p = this.scene.player;
    if (!this.landed) {
      p.artT = 3;
      p.artFree = true;
      p.invuln = Math.max(p.invuln, 6);
      p.body.vx = 0;
      p.body.vy = 36;
      this.sweep({ x: p.cx - 60, y: p.body.y, w: 120, h: p.body.h + 40 }, BASE_DAMAGE * 1.2);
      this.scene.emit({ t: 'trail', x: p.cx, y: p.cy, vx: 0, vy: -1 });
      if (p.body.onGround || this.t > 70) {
        this.landed = true;
        p.artFree = false;
        p.artT = 8;
        const w = this.meteor ? 820 : 480;
        this.sweep({ x: p.cx - w / 2, y: p.feet - 130, w, h: 150 }, BASE_DAMAGE * 3.2);
        this.scene.emit({ t: 'boom', x: p.cx, y: p.feet - 20, r: this.meteor ? 260 : 190, color: 0xffd890 });
        this.scene.emit({ t: 'shockwave', x: p.cx, y: p.feet, r: w / 2, color: 0xffe6b0 });
        this.scene.freeze = 6;
        this.body.x = p.cx;
        this.body.y = p.feet;
      }
      return;
    }
    if (!this.meteor || this.t > 160) {
      this.dead = true;
      return;
    }
    // Meteor: the ground burns for a while.
    this.sweep({ x: this.body.x - 380, y: this.body.y - 60, w: 760, h: 70 }, BASE_DAMAGE * 0.4, 15);
    if (this.t % 3 === 0) this.scene.emit({ t: 'spark', x: this.body.x + (Math.random() - 0.5) * 700, y: this.body.y - 10, n: 2, color: 0xffb070 });
  }
}

/** Constellation: time stops, seven strikes cross every foe on screen. */
export class Constellation extends ArtFx {
  targets: Entity[] = [];

  constructor(scene: Scene) {
    super(scene, scene.player.cx, scene.player.cy, 10, 10);
    this.sprite = 'constellation';
    const p = scene.player;
    this.targets = scene.entities.filter((e) => !e.dead && e.hurtable && !e.prop && Math.abs(e.cx - p.cx) < 1100 && Math.abs(e.cy - p.cy) < 650);
    scene.timeStop = 100;
  }

  update(): void {
    const p = this.scene.player;
    p.artT = 3;
    p.artFree = true;
    p.body.vx = 0;
    p.body.vy = 0;
    p.invuln = Math.max(p.invuln, 30);
    if (this.t % 12 === 6 && this.t < 90) {
      const k = Math.floor(this.t / 12);
      for (const e of this.targets) {
        if (e.dead) continue;
        this.scene.strike(e, BASE_DAMAGE * 1.6 * this.scene.damageMul(), p.cx, 'art');
      }
      const a = (k * 2.4) % (Math.PI * 2);
      this.scene.emit({ t: 'blink', x0: p.cx + Math.cos(a) * 900, y0: p.cy + Math.sin(a) * 500, x1: p.cx - Math.cos(a) * 900, y1: p.cy - Math.sin(a) * 500, big: k === 6 });
    }
    if (this.t >= 96) {
      p.artFree = false;
      this.dead = true;
      this.scene.emit({ t: 'boom', x: p.cx, y: p.cy, r: 300, color: 0xfff0d0 });
    }
  }
}

export function castArt(scene: Scene, name: string): void {
  const p = scene.player;
  const has = (a: string) => scene.save.arts.includes(a);
  if (name === 'comete') scene.add(new Comet(scene, p.cx + p.facing * 40, p.cy - 4, p.facing, has('comete2')));
  else if (name === 'ascension') scene.add(new Ascension(scene, has('ascension2')));
  else if (name === 'chute') scene.add(new Starfall(scene, has('chute2')));
  else if (name === 'constellation') scene.add(new Constellation(scene));
}

export const ART_TILE = TILE;
