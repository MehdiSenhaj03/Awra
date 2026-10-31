import { GlowFilter } from 'pixi-filters';
import { Container, Graphics, Sprite, type Texture } from 'pixi.js';
import { fx } from '../engine/core';
import type { Player } from '../game/player';
import { PHYS } from '../game/player';
import type { Scene } from '../game/scene';
import { AnimSprite, type Clip, range } from './anim';
import { hasTex, sheet, tex } from './assets';
import { fxTextures } from './textures';
import type { GameView } from './view';

export const HERO_SCALE = 1.12;

const C = (s: string, frames: number[], hold: number, loop = true): Clip => ({ sheet: s, frames, hold, loop });

interface Ghost {
  s: Sprite;
  life: number;
  max: number;
}

/** Picks the drawing for the hero's pose, with fallbacks while some sheets are missing. */
function pick(p: Player, scene: Scene): { clip: Clip; t: number; flip?: number } {
  const free = !scene.blade.held();
  const has = (n: string) => !!sheet(n);
  const idle = free && has('hero_idle_free') ? 'hero_idle_free' : has('hero_idle') ? 'hero_idle' : 'hero_run';
  const run = free && has('hero_run_free') ? 'hero_run_free' : 'hero_run';
  const jump = free && has('hero_jump_free') ? 'hero_jump_free' : has('hero_jump') ? 'hero_jump' : null;
  const b = p.body;
  const t = p.poseT;
  const idleFrames = sheet(idle)?.frames.length ?? 1;
  switch (p.pose) {
    case 'run':
      return { clip: C(run, range(0, 7), 4), t };
    case 'jump':
      if (!jump) return { clip: C('hero_run', [3], 1), t };
      return { clip: C(jump, b.vy < -6 ? [1, 2] : [3], 6, false), t };
    case 'fall':
      if (!jump) return { clip: C('hero_run', [7], 1), t };
      return { clip: C(jump, [4, 5], 7), t };
    case 'land':
      if (!jump) return { clip: C(idle, [0], 1), t };
      return { clip: C(jump, [6, 7], 4, false), t };
    case 'dash':
      if (!has('hero_dash_wall')) return { clip: C('hero_run', [3], 1), t };
      return { clip: C('hero_dash_wall', [0, 1, 2, 3], 3, false), t };
    case 'wall':
      if (!has('hero_dash_wall')) return { clip: C(idle, [0], 1), t };
      return { clip: C('hero_dash_wall', [4, 5], 8), t, flip: b.wall || p.facing };
    case 'slash':
    case 'slashUp':
    case 'slashDown': {
      const a = p.attack;
      if (!a) return { clip: C(idle, [0], 1), t };
      let name = 'hero_slash';
      if (a.palm) name = has('hero_palm') ? 'hero_palm' : 'hero_slash';
      else if (a.dir === 'up') name = has('hero_slash_up') ? 'hero_slash_up' : 'hero_slash';
      else if (a.dir === 'down') name = has('hero_slash_down') ? 'hero_slash_down' : 'hero_slash';
      else if (a.combo === 2) name = has('hero_slash2') ? 'hero_slash2' : 'hero_slash';
      else if (a.combo === 3) name = has('hero_slash3') ? 'hero_slash3' : 'hero_slash';
      // Wind-up drawings before the active frames, strike drawings while active, recovery after.
      let idx: number;
      if (a.t < a.from) idx = Math.min(1, Math.floor((a.t / a.from) * 2));
      else if (a.t <= a.to) idx = 2 + Math.min(2, Math.floor(((a.t - a.from) / (a.to - a.from + 1)) * 3));
      else idx = 5 + Math.min(2, Math.floor(((a.t - a.to) / (a.total - a.to)) * 3));
      if (a.palm) idx = (a.dir === 'up' ? 4 : 0) + Math.min(3, Math.floor((a.t / a.total) * 4));
      return { clip: C(name, [idx], 1), t: 0 };
    }
    case 'throw': {
      if (!has('hero_throw')) return { clip: C('hero_slash', [3], 1), t };
      const k = 12 - p.throwT;
      return { clip: C('hero_throw', [Math.min(4, Math.floor(k / 2.4))], 1), t: 0 };
    }
    case 'heal':
      if (!has('hero_heal')) return { clip: C(idle, [0], 1), t };
      return { clip: C('hero_heal', t < 10 ? [0, 1] : [2, 3, 4, 5], t < 10 ? 5 : 9, t >= 10), t: t < 10 ? t : t - 10 };
    case 'hurt':
      if (!has('hero_hurt_death')) return { clip: C(idle, [0], 1), t };
      return { clip: C('hero_hurt_death', [0, 1], 7, false), t };
    case 'wake':
      if (!has('hero_hurt_death')) return { clip: C(idle, [0], 1), t };
      return { clip: C('hero_hurt_death', [7, 7, 7, 7, 6, 5, 4, 3, 2], 14, false), t };
    case 'dead':
      if (!has('hero_hurt_death')) return { clip: C(idle, [0], 1), t };
      return { clip: C('hero_hurt_death', range(2, 7), 8, false), t };
    case 'art':
      if (!has('hero_art')) return { clip: C('hero_slash', [0], 1), t };
      return { clip: C('hero_art', p.attack ? range(4, 7) : range(0, 7), 3, false), t };
    default:
      return { clip: C(idle, range(0, idleFrames - 1), 8), t };
  }
}

export class HeroRig {
  readonly c = new Container();
  private body = new AnimSprite();
  private glow = new AnimSprite();
  private ghosts: Ghost[] = [];
  private bladeSprite = new Sprite();
  private tether = new Graphics();
  private trail: { x: number; y: number }[] = [];
  private ghostT = 0;
  private haloT = 0;

  constructor(private view: GameView) {
    this.glow.blendMode = 'add';
    this.glow.alpha = 0;
    this.bladeSprite.anchor.set(0.92, 0.5);
    this.tether.blendMode = 'add';
    view.heroLayer.addChild(this.c);
    this.c.addChild(this.body, this.glow);
    // A soft cold halo keeps the dark-cloaked hero readable against any background.
    this.body.filters = [new GlowFilter({ distance: 10, outerStrength: 1.4, innerStrength: 0, color: 0x9fb8ff, quality: 0.25, alpha: 0.55 })];
    view.fxLayer.addChild(this.tether, this.bladeSprite);
  }

  reset(): void {
    for (const g of this.ghosts) g.s.destroy();
    this.ghosts = [];
    this.trail = [];
    if (!this.view.fxLayer.children.includes(this.tether)) this.view.fxLayer.addChild(this.tether, this.bladeSprite);
  }

  /** A fading copy of the current drawing (dash, blink). */
  ghost(tint = 0x88b8ff, life = 16, alpha = 0.6): void {
    const s = new Sprite(this.body.texture as Texture);
    s.anchor.copyFrom(this.body.anchor);
    s.scale.copyFrom(this.body.scale);
    s.position.copyFrom(this.body.position);
    s.tint = tint;
    s.alpha = alpha;
    s.blendMode = 'add';
    this.view.heroLayer.addChildAt(s, 0);
    this.ghosts.push({ s, life, max: life });
  }

  update(scene: Scene, _dt: number): void {
    const p = scene.player;
    const { clip, t, flip } = pick(p, scene);
    const facing = flip ?? p.facing;
    const respawning = scene.respawnT > 0;
    this.body.visible = !respawning || scene.respawnT < 20;
    this.body.show(clip, t, facing, HERO_SCALE);
    this.body.position.set(p.cx, p.feet + 3);
    // Blink while invulnerable after a hit.
    this.body.alpha = p.invuln > 0 && p.hurtT === 0 && !p.dead && Math.floor(p.invuln / 4) % 2 === 0 ? 0.35 : 1;
    // Additive glow pass: strong on attacks, heal and Éclipse.
    this.glow.texture = this.body.texture;
    this.glow.anchor.copyFrom(this.body.anchor);
    this.glow.scale.copyFrom(this.body.scale);
    this.glow.position.copyFrom(this.body.position);
    let g = 0;
    if (p.pose === 'heal') g = 0.25 + Math.sin(p.poseT * 0.25) * 0.12;
    if (scene.eclipse > 0) g = Math.max(g, 0.35);
    if (p.hurtT > 0) g = 0.6;
    this.glow.alpha = g;
    this.glow.tint = p.hurtT > 0 ? 0xff6080 : p.pose === 'heal' ? 0xffe6a0 : 0x9fc4ff;

    // Dash afterimages.
    if (p.dashT > 0 || p.pose === 'blink') {
      if (++this.ghostT % 2 === 0) this.ghost(0x7fb0ff, 14, 0.55);
    }
    for (let i = this.ghosts.length - 1; i >= 0; i--) {
      const gh = this.ghosts[i];
      gh.life--;
      gh.s.alpha = (gh.life / gh.max) * 0.55;
      if (gh.life <= 0) {
        gh.s.destroy();
        this.ghosts.splice(i, 1);
      }
    }

    // Halo motes.
    if (++this.haloT % 9 === 0 && !p.dead) {
      this.view.vfx.spawn(fxTextures().dot, { x: p.cx - p.facing * 6 + fx.range(-6, 6), y: p.body.y - 18, vx: fx.range(-0.3, 0.3), vy: fx.range(-0.9, -0.4), life: 40, scale: 0.16, scaleEnd: 0, tint: 0xffe2a0 });
    }

    this.drawBlade(scene);
  }

  private drawBlade(scene: Scene): void {
    const b = scene.blade;
    const p = scene.player;
    const gfx = this.tether;
    gfx.clear();
    if (b.held()) {
      this.bladeSprite.visible = false;
      this.trail = [];
      return;
    }
    const bt = hasTex('blade') ? tex('blade') : null;
    this.bladeSprite.visible = true;
    if (bt) {
      this.bladeSprite.texture = bt;
      const sc = 118 / bt.width;
      this.bladeSprite.scale.set(sc);
    } else {
      this.bladeSprite.texture = fxTextures().streak;
      this.bladeSprite.scale.set(1, 0.6);
      this.bladeSprite.tint = 0xffe6b0;
      this.bladeSprite.blendMode = 'add';
    }
    let ang = b.angle;
    if (b.state === 'returning') ang = Math.atan2(b.vy, b.vx);
    this.bladeSprite.rotation = ang;
    this.bladeSprite.position.set(b.x, b.y);
    // Trail of light while flying.
    if (b.state === 'flying' || b.state === 'returning') {
      this.trail.push({ x: b.x, y: b.y });
      if (this.trail.length > 9) this.trail.shift();
    } else if (this.trail.length) this.trail.shift();
    for (let i = 1; i < this.trail.length; i++) {
      const k = i / this.trail.length;
      gfx.moveTo(this.trail[i - 1].x, this.trail[i - 1].y).lineTo(this.trail[i].x, this.trail[i].y).stroke({ color: 0xffe2a0, width: 14 * k, alpha: 0.35 * k });
    }
    // The tether: a thin wavering thread of light from the hand to the hilt.
    const hx = p.cx + p.facing * 8;
    const hy = p.cy - 6;
    const tx = b.x - Math.cos(ang) * 50;
    const ty = b.y - Math.sin(ang) * 50;
    const d = Math.hypot(tx - hx, ty - hy);
    const n = Math.max(4, Math.floor(d / 40));
    const now = performance.now() / 1000;
    const pts: [number, number][] = [];
    for (let i = 0; i <= n; i++) {
      const k = i / n;
      const sag = Math.sin(k * Math.PI) * Math.min(60, d * 0.08) * (b.state === 'stuck' || b.state === 'branded' ? 1 : 0.2);
      const wob = Math.sin(now * 9 + k * 12) * 3 * Math.sin(k * Math.PI);
      pts.push([hx + (tx - hx) * k, hy + (ty - hy) * k + sag + wob]);
    }
    for (const [w, a, col] of [[9, 0.12, 0xffd890], [4, 0.3, 0xffe8b0], [1.6, 0.9, 0xffffff]] as const) {
      gfx.moveTo(pts[0][0], pts[0][1]);
      for (let i = 1; i < pts.length; i++) gfx.lineTo(pts[i][0], pts[i][1]);
      gfx.stroke({ color: col, width: w, alpha: a });
    }
    void PHYS;
  }
}
