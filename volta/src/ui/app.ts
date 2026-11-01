import * as THREE from 'three';
import { DT } from '../engine/core';
import { audio } from '../engine/audio';
import { input, type Action } from '../engine/input';
import { STAGES } from '../game/stages';
import type { World } from '../game/world';
import { View, type SectorLook } from '../render/view';
import { setLang } from '../i18n';
import { Session } from './session';

export const LOOKS: Record<number, SectorLook> = {
  1: { accent: 0x2ef2ff, accent2: 0xff2fb9, fog: 0x02040c, painting: 'bg_seuil' },
  2: { accent: 0x40ffb0, accent2: 0x2ef2ff, fog: 0x020a08, painting: 'bg_jardin' },
  3: { accent: 0xffa040, accent2: 0xff2fb9, fog: 0x0a0402, painting: 'bg_fonderie' },
  4: { accent: 0xc8d4ff, accent2: 0x9b5cff, fog: 0x050510, painting: 'bg_ocean' },
  5: { accent: 0xff2fb9, accent2: 0x9b5cff, fog: 0x0a0210, painting: 'bg_ville' },
  6: { accent: 0xffffff, accent2: 0xffc34a, fog: 0x050505, painting: 'bg_coeur' },
};

/** The whole game: boot, screens, the play loop. */
export class App {
  view: View;
  session: Session | null = null;
  private acc = 0;
  private last = 0;
  private textures = new Map<string, THREE.Texture>();
  private envs = new Map<string, THREE.Texture>();
  test = new URLSearchParams(location.search).has('test');

  constructor(canvas: HTMLCanvasElement, overlay: HTMLElement, public ui: HTMLElement) {
    this.view = new View(canvas, overlay);
    input.attach(canvas.parentElement!);
    const resize = () => this.view.resize(window.innerWidth, window.innerHeight);
    window.addEventListener('resize', resize);
    resize();
    setLang('fr');
  }

  async boot(): Promise<void> {
    const bar = document.getElementById('bootbar');
    const names = ['bg_seuil'];
    let done = 0;
    await Promise.all(
      names.map(
        (n) =>
          new Promise<void>((res) => {
            new THREE.TextureLoader().load(
              `img/${n}.webp`,
              (tx) => {
                tx.colorSpace = THREE.SRGBColorSpace;
                this.textures.set(n, tx);
                if (bar) bar.style.width = `${(++done / names.length) * 100}%`;
                res();
              },
              undefined,
              () => res(),
            );
          }),
      ),
    );
    const boot = document.getElementById('boot');
    if (boot) {
      boot.style.opacity = '0';
      setTimeout(() => boot.remove(), 700);
    }
    window.addEventListener('pointerdown', () => audio.init(), { once: false });
    window.addEventListener('keydown', () => audio.init(), { once: false });
    const q = new URLSearchParams(location.search);
    this.play(q.get('stage') ?? 's1_1');
    if (!this.test) requestAnimationFrame((t) => this.loop(t));
  }

  /** Environment map (reflections) from a sector painting. */
  private env(name: string): THREE.Texture | null {
    if (this.envs.has(name)) return this.envs.get(name)!;
    const tx = this.textures.get(name);
    if (!tx) return null;
    const pm = new THREE.PMREMGenerator(this.view.renderer);
    const src = tx.clone();
    src.mapping = THREE.EquirectangularReflectionMapping;
    src.needsUpdate = true;
    const env = pm.fromEquirectangular(src).texture;
    pm.dispose();
    this.envs.set(name, env);
    return env;
  }

  play(id: string, checkpoint = 0, setup?: (w: World) => void): void {
    const def = STAGES[id];
    this.session?.dispose();
    const look = LOOKS[def.sector];
    const painting = look.painting ? this.textures.get(look.painting) ?? null : null;
    this.session = new Session(def, this.view, this.ui, { checkpoint, setup });
    this.view.setStage(this.session.world, look, painting, look.painting ? this.env(look.painting) : null);
    this.session.onEnd = (win) => {
      if (!win) setTimeout(() => this.play(id, this.session?.runner.movement ?? 0), 1200);
    };
  }

  private loop(now: number): void {
    const dt = Math.min(0.1, (now - (this.last || now)) / 1000);
    this.last = now;
    this.acc += dt;
    let steps = 0;
    while (this.acc >= DT && steps < 5) {
      this.session?.step();
      this.acc -= DT;
      steps++;
    }
    if (steps === 5) this.acc = 0;
    this.session?.frame(this.acc / DT, dt);
    requestAnimationFrame((t) => this.loop(t));
  }

  /** Test hook: n frames with held actions and an aim, then one render. */
  step(n: number, keys: Action[] = [], aim?: { x: number; y: number } | 'auto'): void {
    input.script = new Set(keys);
    if (this.session) this.session.scriptAim = aim ?? null;
    // Render along the way (every few frames) so effects age as they would in real time.
    for (let i = 0; i < n; i++) {
      this.session?.step();
      if (i % 6 === 5 && i < n - 1) this.session?.frame(1, DT * 6, true);
    }
    this.session?.frame(1, DT);
    input.script = null;
  }
}
