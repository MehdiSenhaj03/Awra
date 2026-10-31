import { Sprite } from 'pixi.js';
import { fx } from '../engine/core';
import type { FxEvent } from '../game/events';
import type { Scene } from '../game/scene';
import { hasTex, tex } from './assets';
import { fxTextures } from './textures';
import type { GameView } from './view';

/** Sound hook (set by the audio module). */
export const sound = { play: (_name: string, _opts?: { x?: number; vol?: number; pitch?: number }) => {} };

interface TempLight {
  x: number;
  y: number;
  r: number;
  color: number;
  life: number;
  max: number;
}

interface Homing {
  s: Sprite;
  x: number;
  y: number;
  vx: number;
  vy: number;
  t: number;
}

/**
 * Turns simulation events into spectacle: arcs of light, sparks, impact stars, shockwaves, flashes, camera kicks,
 * chromatic splits, temporary lights and sounds. All tuning of "game feel" lives here.
 */
export class FxDirector {
  private temp: TempLight[] = [];
  private homing: Homing[] = [];

  constructor(private v: GameView) {}

  private tlight(x: number, y: number, r: number, color: number, life = 10): void {
    this.temp.push({ x, y, r, color, life, max: life });
  }

  lights(): void {
    for (const l of this.temp) this.v.light(l.x, l.y, l.r * (0.6 + 0.4 * (l.life / l.max)), l.color, l.life / l.max);
  }

  update(scene: Scene): void {
    for (let i = this.temp.length - 1; i >= 0; i--) if (--this.temp[i].life <= 0) this.temp.splice(i, 1);
    const p = scene.player;
    for (let i = this.homing.length - 1; i >= 0; i--) {
      const h = this.homing[i];
      h.t++;
      const dx = p.cx - h.x;
      const dy = p.cy - h.y;
      const d = Math.hypot(dx, dy) || 1;
      const pull = Math.min(2.4, h.t * 0.08);
      h.vx = h.vx * 0.9 + (dx / d) * pull;
      h.vy = h.vy * 0.9 + (dy / d) * pull;
      h.x += h.vx;
      h.y += h.vy;
      h.s.position.set(h.x, h.y);
      if (d < 26 || h.t > 120) {
        h.s.destroy();
        this.homing.splice(i, 1);
        if (Math.random() < 0.5) sound.play('coin', { vol: 0.25, pitch: 1 + Math.random() * 0.3 });
      }
    }
  }

  private arc(name: string, x: number, y: number, rot: number, sx: number, sy: number, life: number, tint = 0xffffff, alpha = 1): void {
    const t = hasTex(name) ? tex(name) : fxTextures().streak;
    this.v.vfx.spawn(t, { x, y, life, scale: sy * 0.85, scaleEnd: sy * 1.08, scaleX: sx / sy, rot, alpha, tint, fadeIn: 0.08 });
  }

  handle(e: FxEvent, scene: Scene): void {
    const v = this.v;
    const ft = fxTextures();
    const cam = v.camera;
    switch (e.t) {
      case 'slash': {
        const f = e.facing;
        if (e.palm) {
          const x = e.x + (e.dir === 'side' ? f * 70 : 0);
          const y = e.y + (e.dir === 'up' ? -90 : e.dir === 'down' ? 90 : -4);
          v.vfx.spawn(ft.glow, { x, y, life: 12, scale: 0.6, scaleEnd: 1.4, tint: 0xbfd8ff });
          v.vfx.spawn(ft.ring, { x, y, life: 14, scale: 0.15, scaleEnd: 0.5, tint: 0xbfd8ff });
          sound.play('palm');
          break;
        }
        if (e.dir === 'side') {
          const big = e.combo === 3;
          const name = big && hasTex('fx_slash_wide') ? 'fx_slash_wide' : 'fx_slash';
          const flipY = e.combo === 2 ? -1 : 1;
          const s = big ? 0.66 : 0.5;
          this.arc(name, e.x + f * (big ? 90 : 72), e.y - 6, f > 0 ? 0.08 * flipY : -0.08 * flipY, s * f * -1, s * flipY, big ? 16 : 12);
          this.arc(name, e.x + f * (big ? 96 : 76), e.y - 6, 0, s * f * -0.9, s * flipY * 0.55, 8, 0xffffff, 0.8);
          // A bright cut line along the swing.
          v.vfx.spawn(ft.streak, { x: e.x + f * 80, y: e.y - 8 + flipY * 6, life: 7, scale: 0.5, scaleEnd: 0.2, scaleX: 3.2, rot: f > 0 ? -0.18 * flipY : 0.18 * flipY, tint: 0xffffff });
          for (let i = 0; i < (big ? 10 : 5); i++) {
            v.vfx.spawn(ft.dot, { x: e.x + f * fx.range(40, 150), y: e.y + fx.range(-60, 50), vx: f * fx.range(2, 6), vy: fx.range(-2, 1), life: fx.range(14, 28), scale: fx.range(0.1, 0.22), scaleEnd: 0, tint: 0xffe0a0, drag: 0.92 });
          }
          if (big) cam.kick(f * 6, 0);
          sound.play(big ? 'slashBig' : 'slash', { pitch: e.combo === 2 ? 1.08 : 1 });
        } else {
          const up = e.dir === 'up';
          this.arc('fx_slash', e.x + f * 6, e.y + (up ? -96 : 96), up ? -Math.PI / 2 : Math.PI / 2, 0.36, 0.36 * f, 12);
          sound.play('slash', { pitch: up ? 1.12 : 0.92 });
        }
        break;
      }
      case 'hit': {
        const big = e.big;
        const tint = e.kind === 'blade' || e.kind === 'rip' ? 0xffe6a8 : e.kind === 'execute' ? 0xfff2d0 : 0xffffff;
        const star = hasTex('fx_impact') ? tex('fx_impact') : ft.star;
        v.vfx.spawn(star, { x: e.x, y: e.y, life: big ? 14 : 9, scale: big ? 0.38 : 0.22, scaleEnd: big ? 0.6 : 0.34, rot: fx.range(0, 3.14), tint });
        v.vfx.spawn(ft.glow, { x: e.x, y: e.y, life: 10, scale: big ? 2.4 : 1.4, scaleEnd: 0.4, tint: 0xffd890, alpha: 0.7 });
        v.vfx.burst(ft.streak, e.x, e.y, big ? 18 : 9, { speed: big ? 22 : 15, life: big ? 18 : 12, scale: big ? 0.32 : 0.24, dir: e.dx > 0 ? 0 : Math.PI, spread: 1.0, stretch: 0.04, drag: 0.86, tint: 0xffe8b8 });
        v.vfx.spawn(ft.ring, { x: e.x, y: e.y, life: big ? 18 : 12, scale: 0.1, scaleEnd: big ? 0.9 : 0.5, tint: 0xfff0d0, alpha: 0.8 });
        this.tlight(e.x, e.y, big ? 420 : 260, 0xffd8a0, big ? 14 : 8);
        cam.kick(e.dx * (big ? 9 : 5), fx.range(-2, 2));
        if (big) {
          cam.shake(0.25);
          v.chroma = 4;
          if (e.kind === 'execute' || e.kind === 'rip') v.shockwave(e.x, e.y, 22);
        }
        sound.play(big ? 'hitBig' : e.kind === 'blade' ? 'hitBlade' : 'hit', { pitch: fx.range(0.94, 1.08) });
        break;
      }
      case 'clank':
        v.vfx.burst(ft.streak, e.x, e.y, 10, { speed: 14, life: 12, scale: 0.2, stretch: 0.05, tint: 0xd8e4ff });
        v.vfx.spawn(ft.star, { x: e.x, y: e.y, life: 8, scale: 0.4, scaleEnd: 0.7, tint: 0xd8e4ff });
        cam.kick(fx.range(-4, 4), -2);
        sound.play('clank');
        break;
      case 'kill': {
        const s = Math.max(0.5, e.size / 90);
        const ink = hasTex('fx_gloam') ? tex('fx_gloam') : ft.smoke;
        v.vfx.spawn(ink, { x: e.x, y: e.y, life: 26, scale: 0.25 * s, scaleEnd: 0.55 * s, rot: fx.range(0, 6.28), tint: 0xffffff, alpha: 0.95 });
        v.vfx.spawn(ft.glow, { x: e.x, y: e.y, life: 16, scale: 3 * s, scaleEnd: 0.5, tint: 0xb07cff });
        for (let i = 0; i < 14 * s; i++) {
          v.vfx.spawn(ft.smoke, { x: e.x + fx.range(-30, 30) * s, y: e.y + fx.range(-30, 30) * s, vx: fx.range(-2, 2), vy: fx.range(-3, -0.5), life: fx.range(30, 55), scale: fx.range(0.4, 0.8) * s, scaleEnd: 1.2 * s, tint: 0x2a1040, add: false, alpha: 0.7 });
        }
        v.vfx.burst(ft.dot, e.x, e.y, Math.round(16 * s), { speed: 9, life: 40, scale: 0.22, tint: 0xd0a8ff, drag: 0.93, gravity: -0.05 });
        this.tlight(e.x, e.y, 520 * s, 0xb488ff, 22);
        cam.shake(0.2 + 0.1 * s);
        v.shockwave(e.x, e.y, 10 + 6 * s);
        sound.play('kill', { pitch: 1.1 - Math.min(0.4, s * 0.1) });
        break;
      }
      case 'hurt':
        v.flash = 0.28;
        v.flashColor = 0xff2040;
        v.chroma = 9;
        cam.shake(0.55);
        v.vfx.burst(ft.streak, e.x, e.y, 14, { speed: 16, life: 16, scale: 0.3, stretch: 0.05, tint: 0xff8090 });
        v.vfx.spawn(ft.glow, { x: e.x, y: e.y, life: 16, scale: 3, scaleEnd: 0.4, tint: 0xff3050 });
        sound.play('hurt');
        break;
      case 'dodge': {
        v.flash = 0.35;
        v.flashColor = 0xbfd8ff;
        v.chroma = 7;
        v.shockwave(e.x, e.y, 26);
        const glyph = hasTex('fx_glyph') ? tex('fx_glyph') : ft.ring;
        v.vfx.spawn(glyph, { x: e.x, y: e.y, life: 60, scale: 0.15, scaleEnd: 0.55, spin: 0.02, tint: 0xbfe0ff, alpha: 0.9, fadeIn: 0.1 });
        v.vfx.spawn(ft.ring, { x: e.x, y: e.y, life: 30, scale: 0.2, scaleEnd: 3.2, tint: 0xcfe4ff });
        this.tlight(e.x, e.y, 700, 0xbfd8ff, 40);
        v.hero.ghost(0xbfe0ff, 30, 0.9);
        sound.play('eclipse');
        break;
      }
      case 'jump':
      case 'doubleJump':
      case 'wallJump': {
        const smoke = ft.smoke;
        const n = e.t === 'doubleJump' ? 1 : 6;
        for (let i = 0; i < n; i++) {
          v.vfx.spawn(smoke, { x: e.x + fx.range(-16, 16), y: e.y - 6, vx: fx.range(-2.5, 2.5), vy: fx.range(-1.2, 0), life: fx.range(18, 30), scale: fx.range(0.25, 0.45), scaleEnd: 0.7, tint: 0x9aa4c8, add: false, alpha: 0.5 });
        }
        if (e.t === 'doubleJump') {
          v.vfx.spawn(ft.ring, { x: e.x, y: e.y, life: 16, scale: 0.15, scaleEnd: 0.7, scaleX: 1.8, tint: 0xffe0a0 });
          v.vfx.burst(ft.dot, e.x, e.y, 12, { speed: 6, life: 24, scale: 0.18, tint: 0xffe0a0, dir: Math.PI / 2, spread: 1.2 });
        }
        sound.play(e.t === 'doubleJump' ? 'doubleJump' : 'jump', { vol: 0.5 });
        break;
      }
      case 'land':
        for (let i = 0; i < 6; i++) {
          const d = i < 3 ? -1 : 1;
          v.vfx.spawn(ft.smoke, { x: e.x + d * fx.range(6, 20), y: e.y - 4, vx: d * fx.range(1.5, 4), vy: fx.range(-1, 0), life: fx.range(16, 26), scale: fx.range(0.2, 0.4), scaleEnd: 0.6, tint: 0x9aa4c8, add: false, alpha: 0.45 });
        }
        sound.play('land', { vol: 0.5 });
        break;
      case 'footstep':
        v.vfx.spawn(ft.smoke, { x: e.x, y: e.y - 2, vx: fx.range(-1, 1), vy: -0.3, life: 14, scale: 0.15, scaleEnd: 0.3, tint: 0x8890b0, add: false, alpha: 0.3 });
        sound.play('step', { vol: 0.25, pitch: fx.range(0.9, 1.1) });
        break;
      case 'dash':
        for (let i = 0; i < 8; i++) {
          v.vfx.spawn(ft.streak, { x: e.x - e.dir * fx.range(0, 60), y: e.y + fx.range(-40, 40), vx: -e.dir * fx.range(6, 14), vy: 0, life: fx.range(8, 14), scale: fx.range(0.25, 0.5), stretch: 0.03, tint: 0xa8c8ff });
        }
        v.vfx.spawn(ft.ring, { x: e.x - e.dir * 20, y: e.y, life: 12, scale: 0.1, scaleEnd: 0.6, scaleX: 0.4, tint: 0xa8c8ff });
        sound.play('dash');
        break;
      case 'throw':
        v.vfx.spawn(ft.star, { x: e.x + e.dir * 30, y: e.y - 8, life: 8, scale: 0.4, scaleEnd: 0.1, tint: 0xffe0a0 });
        sound.play('throw');
        break;
      case 'lodge':
        v.vfx.burst(ft.streak, e.x, e.y, 10, { speed: 12, life: 12, scale: 0.22, stretch: 0.05, tint: 0xffe8b0, dir: e.angle + Math.PI, spread: 1.1 });
        v.vfx.spawn(ft.star, { x: e.x, y: e.y, life: 10, scale: 0.5, scaleEnd: 0.2, tint: 0xfff0c8 });
        this.tlight(e.x, e.y, 300, 0xffe0a0, 12);
        cam.kick(Math.cos(e.angle) * 3, Math.sin(e.angle) * 3);
        sound.play('lodge');
        break;
      case 'recall':
        v.vfx.spawn(ft.ring, { x: e.x, y: e.y, life: 12, scale: 0.4, scaleEnd: 0.05, tint: 0xffe0a0 });
        sound.play('recall');
        break;
      case 'catch':
        v.vfx.spawn(ft.glow, { x: e.x, y: e.y, life: 10, scale: 1.2, scaleEnd: 0.3, tint: 0xffe0a0 });
        sound.play('catch', { vol: 0.6 });
        break;
      case 'blink': {
        const n = Math.max(4, Math.floor(Math.hypot(e.x1 - e.x0, e.y1 - e.y0) / 26));
        for (let i = 0; i <= n; i++) {
          const k = i / n;
          v.vfx.spawn(ft.dot, { x: e.x0 + (e.x1 - e.x0) * k + fx.range(-6, 6), y: e.y0 + (e.y1 - e.y0) * k + fx.range(-6, 6), life: fx.range(14, 26), scale: fx.range(0.2, 0.4), scaleEnd: 0, tint: 0xd8e8ff });
        }
        const ang = Math.atan2(e.y1 - e.y0, e.x1 - e.x0);
        v.vfx.spawn(ft.streak, { x: (e.x0 + e.x1) / 2, y: (e.y0 + e.y1) / 2, life: 10, scale: 0.5, scaleX: Math.hypot(e.x1 - e.x0, e.y1 - e.y0) / 64, rot: ang, tint: 0xeaf2ff });
        v.vfx.spawn(ft.glow, { x: e.x1, y: e.y1, life: 12, scale: 2.2, scaleEnd: 0.3, tint: 0xd8e8ff });
        this.tlight(e.x1, e.y1, 420, 0xd8e8ff, 14);
        if (e.big) {
          v.flash = 0.25;
          v.flashColor = 0xfff4dc;
          v.chroma = 8;
          cam.shake(0.4);
        }
        sound.play(e.big ? 'execute' : 'blink');
        break;
      }
      case 'healStart':
        sound.play('focus', { vol: 0.6 });
        break;
      case 'heal': {
        const glyph = hasTex('fx_glyph') ? tex('fx_glyph') : ft.ring;
        v.vfx.spawn(glyph, { x: e.x, y: e.y + 30, life: 36, scale: 0.12, scaleEnd: 0.3, scaleX: 1, spin: 0.03, tint: 0xffe6b0, alpha: 0.9, fadeIn: 0.1 });
        v.vfx.burst(ft.dot, e.x, e.y + 20, 22, { speed: 5, life: 40, scale: 0.2, tint: 0xffe6b0, dir: -Math.PI / 2, spread: 0.8, drag: 0.95 });
        v.flash = 0.12;
        v.flashColor = 0xffe6b0;
        this.tlight(e.x, e.y, 500, 0xffe6b0, 24);
        sound.play('heal');
        break;
      }
      case 'shockwave':
        v.shockwave(e.x, e.y, 18);
        v.vfx.spawn(ft.ring, { x: e.x, y: e.y, life: 20, scale: 0.1, scaleEnd: e.r / 110, tint: e.color });
        break;
      case 'boom': {
        const boom = hasTex('fx_explosion') ? tex('fx_explosion') : ft.glow;
        v.vfx.spawn(boom, { x: e.x, y: e.y, life: 22, scale: e.r / 900, scaleEnd: e.r / 520, rot: fx.range(0, 6), tint: e.color });
        v.vfx.burst(ft.streak, e.x, e.y, 16, { speed: 18, life: 18, scale: 0.3, stretch: 0.04, tint: e.color });
        v.shockwave(e.x, e.y, 20);
        this.tlight(e.x, e.y, e.r * 2.4, e.color, 20);
        cam.shake(0.35);
        sound.play('boom');
        break;
      }
      case 'dust':
        for (let i = 0; i < Math.min(14, e.n); i++) {
          const h = new Sprite(ft.dot);
          h.anchor.set(0.5);
          h.blendMode = 'add';
          h.tint = 0xffd890;
          h.scale.set(0.22);
          v.fxLayer.addChild(h);
          const a = fx.range(0, Math.PI * 2);
          const sp = fx.range(4, 10);
          this.homing.push({ s: h, x: e.x, y: e.y, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp - 4, t: 0 });
        }
        break;
      case 'pickup':
        v.flash = 0.3;
        v.flashColor = 0xfff0d0;
        v.shockwave(e.x, e.y, 20);
        v.vfx.burst(ft.star, e.x, e.y, 10, { speed: 8, life: 30, scale: 0.3, tint: 0xfff0d0 });
        sound.play('pickup');
        break;
      case 'break':
        for (let i = 0; i < 26; i++) {
          v.vfx.spawn(ft.smoke, { x: e.x + fx.range(0, e.w), y: e.y + fx.range(0, e.h), vx: fx.range(-3, 3), vy: fx.range(-4, 0), life: fx.range(30, 60), scale: fx.range(0.4, 0.9), scaleEnd: 1.3, tint: 0x6a7090, add: false, alpha: 0.6 });
        }
        cam.shake(0.5);
        sound.play('break');
        break;
      case 'telegraph':
        v.vfx.spawn(ft.star, { x: e.x, y: e.y, life: 14, scale: e.big ? 0.9 : 0.5, scaleEnd: 0.1, rot: 0.4, tint: 0xffffff });
        v.vfx.spawn(ft.glow, { x: e.x, y: e.y, life: 12, scale: 1, scaleEnd: 0.2, tint: 0xff7090 });
        sound.play('glint', { vol: e.big ? 0.8 : 0.5 });
        break;
      case 'spark':
        v.vfx.burst(ft.streak, e.x, e.y, e.n, { speed: 12, life: 12, scale: 0.2, stretch: 0.05, tint: e.color });
        break;
      case 'roar':
        cam.shake(0.8);
        v.chroma = 6;
        v.shockwave(e.x, e.y, 30);
        sound.play('roar');
        break;
      case 'art':
        sound.play('art');
        v.vfx.spawn(ft.glow, { x: e.x, y: e.y, life: 14, scale: 2.4, scaleEnd: 0.3, tint: 0xffe6b0 });
        v.vfx.spawn(hasTex('fx_glyph') ? tex('fx_glyph') : ft.ring, { x: e.x, y: e.y, life: 26, scale: 0.08, scaleEnd: 0.32, spin: 0.05, tint: 0xffe6b0, alpha: 0.9 });
        v.chroma = 3;
        break;
      case 'spin': {
        const spin = hasTex('fx_slash_spin') ? tex('fx_slash_spin') : ft.ring;
        v.vfx.spawn(spin, { x: e.x, y: e.y, life: 12, scale: 0.36, scaleEnd: 0.46, rot: fx.range(0, 6.28), spin: 0.35, tint: 0xfff0d0 });
        v.vfx.burst(ft.streak, e.x, e.y, 4, { speed: 12, life: 14, scale: 0.3, stretch: 0.04, tint: 0xffe6b0, dir: Math.PI / 2, spread: 0.5 });
        this.tlight(e.x, e.y, 380, 0xffe0a0, 6);
        if (e.big) { cam.shake(0.2); sound.play('slashBig'); } else sound.play('slash', { vol: 0.5, pitch: 1.2 });
        break;
      }
      case 'trail':
        v.vfx.spawn(ft.streak, { x: e.x + fx.range(-10, 10), y: e.y - 60, vx: 0, vy: -30, life: 8, scale: 0.6, stretch: 0.05, tint: 0xfff0c8 });
        v.vfx.spawn(ft.glow, { x: e.x, y: e.y, life: 6, scale: 1.2, scaleEnd: 0.2, tint: 0xffd890 });
        this.tlight(e.x, e.y, 300, 0xffe0a0, 4);
        break;
      case 'sfx':
        sound.play(e.name, { vol: e.vol });
        break;
    }
    void scene;
  }
}
