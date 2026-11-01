import { music } from '../engine/music';
import { sfx } from '../engine/sfx';
import { input } from '../engine/input';
import { audio } from '../engine/audio';
import type { Controls } from '../game/controls';
import type { Fx } from '../game/events';
import { StageRunner, type StageDef } from '../game/stage';
import { World } from '../game/world';
import type { View } from '../render/view';
import { Hud } from './hud';
import { el } from './dom';

/** One stage being played: the world, its runner, the HUD and the event plumbing to sound and screen. */
export class Session {
  world: World;
  runner: StageRunner;
  hud = new Hud();
  private popLayer = el('div', 'passive');
  private pops: { e: HTMLElement; t: number; x: number; y: number; z: number }[] = [];
  /** Scripted aim (tests); otherwise the mouse or the right stick. */
  scriptAim: { x: number; y: number } | 'auto' | null = null;
  private lastAim = { x: 1, y: 0 };
  onEnd: ((win: boolean) => void) | null = null;
  ended = false;

  constructor(public def: StageDef, public view: View, ui: HTMLElement, opts: { checkpoint?: number; seed?: number; setup?: (w: World) => void } = {}) {
    this.world = new World(def.arena, def.bpm, opts.seed ?? 7);
    opts.setup?.(this.world);
    this.runner = new StageRunner(this.world, def, opts.checkpoint ?? 0);
    this.world.player.y = 4;
    this.world.player.py = 4;
    ui.append(this.hud.root, this.popLayer);
    music.start(def.theme);
    music.intensity = 1;
  }

  dispose(): void {
    this.hud.root.remove();
    this.popLayer.remove();
  }

  private controls(): Controls {
    const m = input.move();
    const p = this.world.player;
    let ax = this.lastAim.x;
    let ay = this.lastAim.y;
    if (this.scriptAim === 'auto') {
      const t = this.world.nearestEnemy(p.x, p.y, 60);
      if (t) {
        ax = t.x - p.x;
        ay = t.y - p.y;
      }
    } else if (this.scriptAim) {
      ax = this.scriptAim.x;
      ay = this.scriptAim.y;
    } else if (input.usingPad) {
      const a = input.padAim;
      if (Math.hypot(a.x, a.y) > 0.3) {
        ax = a.x;
        ay = a.y;
      } else if (Math.hypot(m.x, m.y) > 0.3) {
        ax = m.x;
        ay = m.y;
      }
    } else {
      const c = this.view.renderer.domElement;
      const hit = this.view.pick(input.mouseX, input.mouseY, c.clientWidth, c.clientHeight);
      if (hit) {
        ax = hit.x - p.x;
        ay = hit.y - p.y;
      }
    }
    const l = Math.hypot(ax, ay) || 1;
    this.lastAim = { x: ax / l, y: ay / l };
    return {
      mx: m.x,
      my: m.y,
      ax: ax / l,
      ay: ay / l,
      fire: input.isDown('fire'),
      firePressed: input.pressed('fire'),
      dash: input.pressed('dash'),
      pulse: input.pressed('pulse'),
      overdrive: input.pressed('overdrive'),
    };
  }

  /** One simulation frame. */
  step(): void {
    input.poll();
    const w = this.world;
    const prevBeat = Math.floor(w.beat.beat);
    w.update(this.controls());
    this.runner.update();
    const events = w.events;
    w.events = [];
    this.view.handle(events, w);
    this.sound(events);
    if (Math.floor(w.beat.beat) !== prevBeat) this.view.onBeat(Math.floor(w.beat.beat) % 4 === 0);
    // Music intensity follows the fight.
    music.intensity = this.runner.state === 'intro' ? 1 : this.runner.boss ? 4 : w.rank >= 3 ? 3 : 2;
    music.overdrive = w.player.overdrive;
    if (!this.ended && w.over) {
      this.ended = true;
      const win = w.over === 'win';
      if (!win) audio.muffle(true, 500);
      setTimeout(() => this.onEnd?.(win), win ? 1800 : 600);
    }
  }

  private sound(events: Fx[]): void {
    const w = this.world;
    for (const e of events) {
      switch (e.t) {
        case 'shot':
          if (e.weapon.startsWith('arcLink') || e.weapon === 'arcMiss') break;
          music.play(e.weapon.includes('Perfect') ? 'perfect' : e.accent ? 'accent' : 'shot', w.beat.beat);
          break;
        case 'judge':
          if (e.what !== 'shot') music.play(e.judge === 'perfect' ? 'perfect' : 'great', w.beat.beat);
          break;
        case 'kill':
          music.play(e.heavy ? 'bigKill' : 'kill', w.beat.beat);
          sfx(e.heavy ? 'bigBoom' : 'boom', 1 + Math.random() * 0.2);
          break;
        case 'parry':
          music.play('parry', w.beat.beat);
          break;
        case 'graze':
          music.play('graze', w.beat.beat);
          break;
        case 'spawn':
          sfx('spawn');
          break;
        case 'checkpoint':
          sfx('checkpoint');
          this.hud.banner('CHECKPOINT');
          break;
        case 'boss':
          sfx('roar');
          break;
        case 'sfx':
          sfx(e.name, e.pitch, e.vol);
          break;
        default:
          break;
      }
    }
  }

  /** Every displayed frame. */
  frame(alpha: number, dt: number, skipDraw = false): void {
    const w = this.world;
    music.update(w.beat.beat, w.beat.bpm);
    this.view.level = audio.level();
    this.view.render(w, alpha, dt, skipDraw);
    this.hud.update(w, this.runner);
    this.updatePopups(dt);
  }

  private updatePopups(dt: number): void {
    const v = this.view;
    for (const p of v.popups) {
      const e = el('div', `pp ${p.cls}`, p.text);
      this.popLayer.appendChild(e);
      this.pops.push({ e, t: 0, x: p.x, y: p.y, z: p.z });
    }
    v.popups.length = 0;
    const c = v.renderer.domElement;
    for (let i = this.pops.length - 1; i >= 0; i--) {
      const p = this.pops[i];
      p.t += dt;
      if (p.t > 0.8) {
        p.e.remove();
        this.pops.splice(i, 1);
        continue;
      }
      const s = v.project(p.x, p.y + p.t * 1.6, p.z, c.clientWidth, c.clientHeight);
      const k = p.t < 0.08 ? 1.4 - p.t * 5 : 1;
      p.e.style.transform = `translate(${s.x}px, ${s.y}px) translate(-50%, -50%) scale(${k})`;
      p.e.style.left = '0';
      p.e.style.top = '0';
      p.e.style.opacity = String(p.t > 0.55 ? 1 - (p.t - 0.55) / 0.25 : 1);
    }
  }
}
