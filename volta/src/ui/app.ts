import * as THREE from 'three';
import { DT } from '../engine/core';
import { audio } from '../engine/audio';
import { input, loadLayout, type Action } from '../engine/input';
import { music } from '../engine/music';
import { sfx } from '../engine/sfx';
import type { Fx } from '../game/events';
import { GRADES, type Save, type Settings, buildMods, endlessUnlocked, grade, loadSave, loadSettings, newSave, overdriveUnlocked, pulseUnlocked, writeSave, writeSettings } from '../game/meta';
import { ORDER, STAGES } from '../game/stages';
import type { World } from '../game/world';
import { dialogue, setLang, t, tl } from '../i18n';
import { View, type SectorLook } from '../render/view';
import { Choice } from './choice';
import { Cinematic } from './cinematic';
import { Codex } from './codex';
import { Loadout } from './loadout';
import { MapScreen } from './map';
import { Options } from './options';
import { BossCard, Confirm, Dialogue, Pause, Unlock } from './overlays';
import { Results } from './results';
import type { Screen } from './screen';
import { Session } from './session';
import { Title } from './title';

export const LOOKS: Record<number, SectorLook> = {
  1: { accent: 0x2ef2ff, accent2: 0xff2fb9, fog: 0x02040c, painting: 'bg_seuil' },
  2: { accent: 0x40ffb0, accent2: 0x2ef2ff, fog: 0x020a08, painting: 'bg_jardin' },
  3: { accent: 0xffa040, accent2: 0xff2fb9, fog: 0x0a0402, painting: 'bg_fonderie' },
  4: { accent: 0xc8d4ff, accent2: 0x9b5cff, fog: 0x050510, painting: 'bg_ocean' },
  5: { accent: 0xff2fb9, accent2: 0x9b5cff, fog: 0x0a0210, painting: 'bg_ville' },
  6: { accent: 0xffffff, accent2: 0xffc34a, fog: 0x050505, painting: 'bg_coeur' },
};

const PAINTINGS = ['bg_seuil', 'bg_jardin', 'bg_fonderie', 'bg_ocean', 'bg_ville', 'bg_coeur'];
const ENEMY_HINTS: Record<string, string> = { bulwark: 'bulwark', mirror: 'mirror', phantom: 'phantom', choir: 'choir' };

/** The whole game: boot, screens, the play loop. */
export class App {
  view: View;
  session: Session | null = null;
  save: Save;
  settings: Settings;
  private screen: Screen | null = null;
  private overlays: Screen[] = [];
  private passive: BossCard[] = [];
  private acc = 0;
  private last = 0;
  private textures = new Map<string, THREE.Texture>();
  private awake = false;
  private hintsShown = new Set<string>();
  private stageId = '';
  test = new URLSearchParams(location.search).has('test');

  constructor(private canvas: HTMLCanvasElement, overlay: HTMLElement, public ui: HTMLElement) {
    this.view = new View(canvas, overlay);
    input.attach(canvas.parentElement!);
    this.save = loadSave();
    this.settings = loadSettings();
    if (this.settings.bindings) Object.assign(input.bindings, this.settings.bindings);
    setLang(this.settings.lang);
    if (this.view.software && !this.test) this.settings.quality = 0;
    this.view.setQuality(this.settings.quality);
    const resize = () => this.view.resize(window.innerWidth, window.innerHeight);
    window.addEventListener('resize', resize);
    resize();
  }

  async boot(): Promise<void> {
    void loadLayout();
    const bar = document.getElementById('bootbar');
    let done = 0;
    const total = PAINTINGS.length + 3;
    const tick = () => bar && (bar.style.width = `${(++done / total) * 100}%`);
    await Promise.all([
      ...PAINTINGS.map(
        (n) =>
          new Promise<void>((res) => {
            new THREE.TextureLoader().load(
              `img/${n}.webp`,
              (tx) => {
                tx.colorSpace = THREE.SRGBColorSpace;
                this.textures.set(n, tx);
                tick();
                res();
              },
              undefined,
              () => {
                tick();
                res();
              },
            );
          }),
      ),
      ...['bg_title', 'key_art', 'pt_volta'].map(
        (n) =>
          new Promise<void>((res) => {
            const i = new Image();
            i.onload = i.onerror = () => {
              tick();
              res();
            };
            i.src = `img/${n}.webp`;
          }),
      ),
    ]);
    const boot = document.getElementById('boot');
    if (boot) {
      boot.style.opacity = '0';
      setTimeout(() => boot.remove(), 700);
    }
    const wake = () => {
      audio.init();
      audio.setVolumes(this.settings.music, this.settings.sfx);
    };
    window.addEventListener('pointerdown', wake);
    window.addEventListener('keydown', wake);
    const q = new URLSearchParams(location.search);
    if (q.get('stage')) {
      this.awake = true;
      this.startStage(q.get('stage')!, 0, true);
    } else this.toTitle();
    if (!this.test) requestAnimationFrame((tm) => this.loop(tm));
  }

  // ---- Screens --------------------------------------------------------------------------------------------

  private setScreen(s: Screen | null): void {
    const old = this.screen;
    if (old) {
      old.root.classList.add('out');
      setTimeout(() => old.root.remove(), 350);
      old.dispose?.();
    }
    this.screen = s;
    if (s) this.ui.appendChild(s.root);
  }

  private push(o: Screen): void {
    this.overlays.push(o);
    this.ui.appendChild(o.root);
  }

  private pop(o?: Screen): void {
    const x = o ?? this.overlays[this.overlays.length - 1];
    if (!x) return;
    this.overlays = this.overlays.filter((y) => y !== x);
    x.root.remove();
    x.dispose?.();
  }

  private menuMusic(): void {
    if (music.theme?.id !== 'title') {
      music.stop(0.8);
      music.start('title');
    }
    music.intensity = 0;
    audio.muffle(false);
  }

  private endSession(): void {
    this.session?.dispose();
    this.session = null;
    this.canvas.style.visibility = 'hidden';
    for (const b of this.passive) b.root.remove();
    this.passive = [];
  }

  toTitle(): void {
    this.endSession();
    this.menuMusic();
    const s = this.save;
    const started = Object.keys(s.stages).length > 0;
    const items = [
      { label: started ? t('title.continue') : t('title.play'), action: () => this.play() },
      { label: endlessUnlocked(s) ? t('title.encore') : t('title.encoreLocked'), action: () => this.encore(), disabled: !endlessUnlocked(s) },
      { label: t('title.codex'), action: () => this.setScreen(new Codex(this.save, () => this.toTitle())) },
      { label: t('title.options'), action: () => this.options() },
    ];
    if (started) items.push({ label: t('title.newGame'), action: () => this.push(new Confirm(t('title.confirmNew'), () => { this.save = newSave(); writeSave(this.save); this.pop(); this.toTitle(); }, () => this.pop())) });
    if ((window as unknown as { voltaDesktop?: { quit(): void } }).voltaDesktop) items.push({ label: t('title.quit'), action: () => (window as unknown as { voltaDesktop: { quit(): void } }).voltaDesktop.quit() });
    if (this.view.software) setTimeout(() => this.screen instanceof Title && this.screen.root.appendChild(Object.assign(document.createElement('div'), { className: 'gpu-warn', textContent: t('options.software') })), 300);
    this.setScreen(new Title(items, () => { this.awake = true; audio.init(); audio.setVolumes(this.settings.music, this.settings.sfx); this.menuMusic(); }, this.awake));
  }

  private options(): void {
    this.push(new Options(this.settings, () => this.pop(), () => this.applySettings()));
  }

  private applySettings(): void {
    audio.setVolumes(this.settings.music, this.settings.sfx);
    this.view.setQuality(this.settings.quality);
    this.view.shakeScale = this.settings.shake;
    this.view.flashScale = this.settings.flashes;
    if (this.session) this.session.world.beat.latency = this.settings.latency;
    writeSettings(this.settings);
  }

  private play(): void {
    if (!this.save.seenIntro) {
      this.save.seenIntro = true;
      writeSave(this.save);
      const lines = tl('story.intro');
      const imgs = ['cine_symphony', 'cine_silence', 'cine_awaken'];
      this.setScreen(new Cinematic(lines.map((text, i) => ({ image: imgs[i], text })), () => this.toMap('s1_1')));
      music.stop(1.5);
      return;
    }
    this.toMap(null);
  }

  toMap(focus: string | null): void {
    this.endSession();
    this.menuMusic();
    this.setScreen(new MapScreen(this.save, focus, (id) => this.toLoadout(id), () => this.toTitle()));
  }

  private toLoadout(id: string): void {
    this.setScreen(new Loadout(this.save, id, () => this.startStage(id, 0), () => this.toMap(id)));
  }

  // ---- Playing --------------------------------------------------------------------------------------------

  startStage(id: string, checkpoint = 0, skipStory = false): void {
    this.endSession();
    this.setScreen(null);
    this.stageId = id;
    const def = STAGES[id];
    const look = LOOKS[def.sector];
    const s = this.save;
    const setup = (w: World) => {
      w.applyMods(buildMods(s));
      w.player.weapon = s.weapon;
      w.player.weaponLevel = s.weaponLevels[s.weapon] ?? 1;
      w.canPulse = pulseUnlocked(s) || def.sector > 1 || def.index > 0;
      w.canOverdrive = overdriveUnlocked(s);
      w.beat.latency = this.settings.latency;
      if (s.hard) w.player.maxHp = w.player.hp = Math.max(1, w.player.maxHp - 2);
    };
    music.stop(0.4);
    this.session = new Session(def, this.view, this.ui, { checkpoint, setup });
    this.canvas.style.visibility = 'visible';
    this.view.shakeScale = this.settings.shake;
    this.view.flashScale = this.settings.flashes;
    this.view.setStage(this.session.world, look, this.textures.get(look.painting ?? '') ?? null, null);
    this.session.onEnd = (win) => this.endStage(win);
    this.session.onEvents = (ev) => this.gameEvents(ev);
    this.session.hud.banner(t(`sector.${def.sector}.name`).toUpperCase(), t(`stage.${def.index}`).toUpperCase());
    const first = !s.stages[id]?.cleared;
    const lines = dialogue(`story.${id}`);
    if (!skipStory && first && checkpoint === 0 && lines.length) this.push(new Dialogue(lines, () => this.pop()));
    this.session.frame(1, DT);
    if (id === 's1_1' && first) this.hint('move', 0);
  }

  private hint(key: string, delayFrames: number): void {
    if (this.hintsShown.has(key)) return;
    this.hintsShown.add(key);
    const k = (a: Action) => input.keyName(a);
    const html = t(`hint.${key}`, { up: k('up'), down: k('down'), left: k('left'), right: k('right'), fire: k('fire'), dash: k('dash'), pulse: k('pulse'), overdrive: k('overdrive') });
    setTimeout(() => this.session?.hud.showHint(html, 420), delayFrames * (1000 / 60));
  }

  private gameEvents(events: Fx[]): void {
    const w = this.session!.world;
    for (const e of events) {
      if (e.t === 'boss' && e.phase === 0) {
        const b = new BossCard(e.name);
        this.passive.push(b);
        this.ui.appendChild(b.root);
      } else if (e.t === 'boss' && e.phase > 1 && e.phase < 9) {
        this.session!.hud.banner(t('boss.phase', { n: e.phase }));
      } else if (e.t === 'boss' && e.phase === 9) {
        this.session!.hud.banner(t('boss.solo'));
      } else if (e.t === 'wave') {
        if (this.stageId === 's1_1' && e.n === 2) this.hint('beat', 30);
        if (this.stageId === 's1_1' && e.n === 4) this.hint('dash', 30);
        if (this.stageId === 's1_2' && e.n === 1) this.hint('pulse', 60);
        if (this.stageId === 's1_2' && e.n === 4) this.hint('rank', 60);
      } else if (e.t === 'spawn') {
        for (const en of w.enemies) if (ENEMY_HINTS[en.kind]) this.hint(ENEMY_HINTS[en.kind], 60);
      } else if (e.t === 'fragment') {
        if (!this.save.fragments.includes(e.id)) {
          this.save.fragments.push(e.id);
          writeSave(this.save);
        }
        this.session!.hud.banner(t('map.fragment').toUpperCase(), t(`fragments.${e.id}.name`));
      }
    }
    if (w.canOverdrive && w.gauge >= 100) this.hint('overdrive', 0);
  }

  private endStage(win: boolean): void {
    const w = this.session!.world;
    const id = this.stageId;
    const s = this.save;
    s.playFrames += w.stats.frames;
    if (id === 'encore') {
      const wave = this.session!.runner.waveNumber;
      const record = w.score > s.endlessBest;
      s.endlessBest = Math.max(s.endlessBest, Math.round(w.score));
      s.endlessWave = Math.max(s.endlessWave, wave);
      s.notes += w.stats.notes;
      writeSave(s);
      this.push(new Results(w, false, 0, record, w.stats.notes, [
        { label: t('results.retry'), action: () => { this.pop(); this.startStage('encore', 0, true); } },
        { label: t('results.map'), action: () => { this.pop(); this.toTitle(); } },
      ]));
      return;
    }
    const g = win ? grade(id, w.score, w.stats.hits, w.stats.maxRank) : 0;
    const rec = s.stages[id] ?? { best: 0, rank: 0, cleared: false, fragment: false };
    const firstClear = win && !rec.cleared;
    const record = win && w.score > rec.best;
    let notes = w.stats.notes;
    if (win) notes += 40 * STAGES[id].sector + g * 30;
    s.notes += notes;
    if (win) {
      rec.cleared = true;
      rec.best = Math.max(rec.best, Math.round(w.score));
      rec.rank = Math.max(rec.rank, g);
    } else s.deaths++;
    const frag = STAGES[id].fragment?.id;
    if (frag && s.fragments.includes(frag)) rec.fragment = true;
    s.stages[id] = rec;
    writeSave(s);
    const next = ORDER[ORDER.indexOf(id) + 1];
    const actions = win
      ? [
          { label: t('results.next'), action: () => this.afterWin(id, firstClear, next) },
          { label: t('results.retry'), action: () => this.startStage(id) },
          { label: t('results.map'), action: () => this.toMap(id) },
        ]
      : [
          ...(this.session!.runner.movement > 0 ? [{ label: t('results.checkpoint'), action: () => this.startStage(id, this.session!.runner.movement, true) }] : []),
          { label: t('results.retry'), action: () => this.startStage(id, 0, true) },
          { label: t('results.map'), action: () => this.toMap(id) },
        ];
    music.intensity = 0;
    this.session!.hud.root.style.display = 'none';
    this.push(new Results(w, win, g, record, notes, actions.map((a) => ({ label: a.label, action: () => { this.pop(); a.action(); } }))));
  }

  /** Story beats and unlocks after a first clear, then onwards. */
  private afterWin(id: string, firstClear: boolean, next: string | undefined): void {
    this.endSession();
    music.start('title');
    music.intensity = 0;
    const queue: (() => void)[] = [];
    const run = () => {
      const f = queue.shift();
      if (f) f();
      else if (id === 's6_4' && firstClear) this.finale();
      else this.toMap(next ?? id);
    };
    const def = STAGES[id];
    const key = (a: Action) => input.keyName(a);
    if (firstClear) {
      if (def.boss) queue.push(() => this.push(new Unlock(t(`boss.${def.boss}.name`), '', t(`story.after.${def.boss}`), `boss_${def.boss}`, () => { this.pop(); run(); })));
      const after = dialogue(`story.${id}b`);
      if (after.length) queue.push(() => this.push(new Dialogue(after, () => { this.pop(); run(); })));
      const unlock = (k: string, n: string, d: string, icon: string | null) => queue.push(() => this.push(new Unlock(k, n, d, icon, () => { this.pop(); run(); })));
      if (id === 's1_1') unlock('', t('unlock.pulse.name'), t('unlock.pulse.desc', { key: key('pulse') }), 'ic_a6');
      if (id === 's1_4') {
        unlock('', t('unlock.overdrive.name'), t('unlock.overdrive.desc', { key: key('overdrive') }), 'ic_a7');
        unlock(t('unlock.weapon'), t('weapon.chord.name'), t('weapon.chord.desc'), 'ic_a1');
      }
      if (id === 's2_4') {
        unlock(t('unlock.weapon'), t('weapon.lance.name'), t('weapon.lance.desc'), 'ic_a2');
        unlock('', t('unlock.slot'), '', null);
      }
      if (id === 's3_4') {
        unlock(t('unlock.weapon'), t('weapon.arc.name'), t('weapon.arc.desc'), 'ic_a3');
        unlock('', t('unlock.encore'), t('encoreMode.desc'), null);
      }
      if (id === 's4_4') {
        unlock(t('unlock.weapon'), t('weapon.swarm.name'), t('weapon.swarm.desc'), 'ic_a4');
        unlock('', t('unlock.slot'), '', null);
      }
    }
    run();
  }

  private finale(): void {
    this.endSession();
    this.setScreen(new Choice(() => this.ending('coda'), () => this.ending('encore')));
  }

  private ending(kind: 'coda' | 'encore'): void {
    this.save.ending = kind;
    writeSave(this.save);
    music.stop(2);
    const lines = tl(`story.${kind}`);
    const h = Math.floor(this.save.playFrames / 216000);
    const m = Math.floor((this.save.playFrames / 3600) % 60);
    const shots = lines.map((text) => ({ image: kind === 'coda' ? 'cine_coda' : 'cine_encore', text }));
    shots.push({ image: 'cine_fermata', text: `${t('ending.thanks')} ${t('ending.stats', { time: `${h} h ${m} min`, deaths: this.save.deaths })}` });
    shots.push({ image: 'key_art', text: t('ending.credits') });
    this.setScreen(new Cinematic(shots, () => this.toTitle()));
    setTimeout(() => {
      music.start('title');
      music.intensity = 0;
    }, 1500);
  }

  private encore(): void {
    this.startStage('encore');
  }

  private pause(): void {
    audio.muffle(true, 700);
    sfx('ui', 0.8);
    const close = () => {
      this.pop();
      audio.muffle(false);
    };
    this.push(
      new Pause({
        resume: close,
        restart: () => {
          close();
          this.startStage(this.stageId, 0, true);
        },
        options: () => this.options(),
        quit: () => {
          close();
          this.toMap(this.stageId);
        },
      }),
    );
  }

  // ---- Loop -----------------------------------------------------------------------------------------------

  /** Frame-time watch: drops one quality level when the game stays under ~45 fps for three seconds. */
  private slow = 0;
  private watchT = 0;

  private watch(raw: number): void {
    if (!this.session || this.overlays.length || raw > 0.25) return;
    this.watchT += raw;
    if (raw > 1 / 45) this.slow += raw;
    if (this.watchT < 3) return;
    const ratio = this.slow / this.watchT;
    this.watchT = 0;
    this.slow = 0;
    if (ratio > 0.6 && this.settings.quality > 0) {
      this.settings.quality--;
      this.view.setQuality(this.settings.quality);
      writeSettings(this.settings);
      this.session.hud.showHint(t('options.autoQuality', { q: t(`options.q${this.settings.quality}`) }), 300);
    }
  }

  private loop(now: number): void {
    const raw = (now - (this.last || now)) / 1000;
    const dt = Math.min(0.1, raw);
    this.last = now;
    this.watch(raw);
    this.frame(dt);
    requestAnimationFrame((tm) => this.loop(tm));
  }

  private frame(dt: number): void {
    const s = this.session;
    const blocking = this.overlays.length > 0;
    if (s && !blocking && !s.ended) {
      this.acc += dt;
      let steps = 0;
      while (this.acc >= DT && steps < 5) {
        s.step();
        this.acc -= DT;
        steps++;
        if (input.pressed('pause') && !s.ended) {
          this.pause();
          break;
        }
      }
      if (steps === 5) this.acc = 0;
      s.frame(Math.min(1, this.acc / DT), dt);
    } else {
      input.poll();
      if (s) s.frame(1, blocking ? 0 : dt, this.test);
      else if (music.theme) music.update(performance.now() / 1000 * (music.theme.bpm / 60), music.theme.bpm);
      const top = this.overlays[this.overlays.length - 1] ?? this.screen;
      top?.update(dt);
    }
    for (const b of this.passive) {
      b.update(dt);
      if (b.done) b.root.remove();
    }
    this.passive = this.passive.filter((b) => !b.done);
  }

  /** Test hook: n frames with held actions and an aim, then one render. */
  step(n: number, keys: Action[] = [], aim?: { x: number; y: number } | 'auto'): void {
    input.script = new Set(keys);
    if (this.session) this.session.scriptAim = aim ?? null;
    for (let i = 0; i < n; i++) {
      if (this.session && !this.overlays.length) {
        this.session.step();
        if (i % 6 === 5 && i < n - 1) this.session.frame(1, DT * 6, true);
      } else this.frame(DT);
    }
    if (this.session) this.session.frame(1, DT);
    input.script = null;
  }

  /** Benchmark hook: n frames, each simulated and fully rendered, with timings and GPU counters. */
  bench(n: number, keys: Action[] = ['fire'], aim: { x: number; y: number } | 'auto' = 'auto'): Record<string, number> {
    const s = this.session!;
    input.script = new Set(keys);
    s.scriptAim = aim;
    const info = this.view.renderer.info;
    const mem = (performance as unknown as { memory?: { usedJSHeapSize: number } }).memory;
    const heap0 = mem?.usedJSHeapSize ?? 0;
    const programs0 = info.programs?.length ?? 0;
    let sim = 0;
    let draw = 0;
    let worst = 0;
    let calls = 0;
    info.autoReset = false;
    const prof = this.view.prof;
    Object.assign(prof, { actors: 0, fx: 0, misc: 0, gpu: 0, worstGpu: 0 });
    for (let i = 0; i < n; i++) {
      info.reset();
      const t0 = performance.now();
      s.step();
      const t1 = performance.now();
      s.frame(1, DT);
      const t2 = performance.now();
      sim += t1 - t0;
      draw += t2 - t1;
      worst = Math.max(worst, t2 - t0);
      calls += info.render.calls;
    }
    input.script = null;
    info.autoReset = true;
    return {
      actorsMs: prof.actors / n,
      fxMs: prof.fx / n,
      miscMs: prof.misc / n,
      gpuMs: prof.gpu / n,
      worstGpu: prof.worstGpu,
      sparks: (this.view as unknown as { sparks: { n: number } }).sparks.n,
      simMs: sim / n,
      drawMs: draw / n,
      worstMs: worst,
      calls: calls / n,
      programs: info.programs?.length ?? 0,
      newPrograms: (info.programs?.length ?? 0) - programs0,
      geometries: info.memory.geometries,
      textures: info.memory.textures,
      heapMB: ((mem?.usedJSHeapSize ?? 0) - heap0) / 1e6,
      enemies: s.world.enemies.length,
      bullets: s.world.bullets.n,
    };
  }

  /** Test hook: one menu frame. */
  ui1(): void {
    this.frame(DT);
  }
}

export { GRADES };
