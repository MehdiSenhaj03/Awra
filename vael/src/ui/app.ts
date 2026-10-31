import { type Application, Container, Graphics, type Text } from 'pixi.js';
import { audio } from '../engine/audio';
import { input } from '../engine/input';
import { NO_CONTROLS, readControls } from '../game/controls';
import type { Entity } from '../game/entity';
import { Interactive } from '../game/props';
import { type Save, type Settings, loadSettings, newSave, writeSave, writeSettings } from '../game/save';
import { Scene } from '../game/scene';
import { spawnEntity } from '../game/spawn';
import { setLang, t, tl } from '../i18n';
import { loadAssets } from '../render/assets';
import { sound } from '../render/director';
import { registerLooks } from '../render/looks';
import { GameView } from '../render/view';
import { zoneLook } from '../render/zones';
import { neighbour, roomDef } from '../world/world';
import { GOLD, style, text } from './draw';
import { Hud } from './hud';
import { ChoiceBox, type Modal } from './modals';
import { TitleScreen } from './title';
import { Cinematic } from './cinematic';
import { ControlsScreen } from './controls';
import { Inventory } from './inventory';
import { MapScreen } from './map';
import { GameHooks } from './gamehooks';

export const IMAGES = [
  'bg_couronne_a', 'bg_couronne_b', 'bg_couronne_c', 'tex_couronne', 'key_title', 'prop_arch', 'prop_crystals', 'prop_pillar', 'prop_statue', 'prop_chain',
  'prop_banner', 'prop_candles', 'prop_lamp', 'fg_couronne_a', 'fg_couronne_b', 'plat_couronne', 'edge_top', 'en_test',
  'fx_slash', 'fx_slash_wide', 'fx_slash_spin', 'fx_impact', 'fx_glyph', 'fx_explosion', 'fx_gloam', 'fx_tether', 'blade', 'shrine', 'fallen_star',
  'bg_jardins_a', 'bg_jardins_b', 'tex_jardins', 'prop_flower', 'prop_mushrooms', 'prop_roots', 'prop_greenhouse', 'prop_tree', 'fg_jardins_a', 'fg_jardins_b', 'plat_jardins',
  'bg_forge_a', 'bg_forge_b', 'tex_forge', 'fg_forge_a', 'plat_forge', 'edge_forge', 'prop_forge_0', 'prop_forge_1', 'prop_forge_2', 'prop_forge_3', 'prop_forge_4', 'bg_archives_a', 'bg_archives_b', 'tex_archives', 'fg_archives_a', 'plat_archives', 'edge_archives', 'prop_archives_0', 'prop_archives_1', 'prop_archives_2', 'prop_archives_3', 'prop_archives_4', 'bg_bastion_a', 'bg_bastion_b', 'tex_bastion', 'fg_bastion_a', 'plat_bastion', 'edge_bastion', 'prop_bastion_0', 'prop_bastion_1', 'prop_bastion_2', 'prop_bastion_3', 'prop_bastion_4', 'bg_firmament_a', 'bg_firmament_b', 'tex_firmament', 'fg_firmament_a', 'plat_firmament', 'edge_firmament', 'prop_firmament_0', 'prop_firmament_1', 'prop_firmament_2', 'prop_firmament_3', 'prop_firmament_4', 'bg_penombre_a', 'bg_penombre_b', 'tex_penombre', 'fg_penombre_a', 'plat_penombre', 'edge_penombre', 'prop_penombre_0', 'prop_penombre_1', 'prop_penombre_2', 'prop_penombre_3', 'prop_penombre_4', 'bg_coeur_a', 'bg_coeur_b', 'tex_coeur', 'fg_coeur_a', 'plat_coeur', 'edge_coeur', 'prop_coeur_0', 'prop_coeur_1', 'prop_coeur_2', 'prop_coeur_3', 'prop_coeur_4', 'tablet', 'npc_orsa', 'pt_orsa', 'npc_vey', 'pt_vey', 'npc_lunelle', 'pt_lunelle', 'npc_veilleur', 'pt_veilleur', 'npc_pip', 'npc_murmure', 'pt_murmure', 'cut_intro1', 'cut_intro2', 'cut_intro3', 'cut_intro4', 'fin_lien', 'fin_liberation', 'fin_aube', 'map_paper', 'ui_frame', 'ic_seal_0', 'ic_seal_1', 'ic_seal_2', 'ic_seal_3', 'ic_seal_4', 'ic_seal_5', 'ic_seal_6', 'ic_seal_7', 'ic_seal_8', 'ic_seal_9', 'ic_seal_10', 'ic_seal_11', 'ic_seal_12', 'ic_seal_13', 'ic_seal_14', 'ic_seal_15', 'ic_item_0', 'ic_item_1', 'ic_item_2', 'ic_item_3', 'ic_item_4', 'ic_item_5', 'ic_item_6', 'ic_item_7', 'ic_item_8', 'ic_item_9', 'ic_item_10', 'ic_item_11', 'ic_item_12', 'ic_item_13', 'ic_item_14', 'ic_item_15', 'ic_art_0', 'ic_art_1', 'ic_art_2', 'ic_art_3', 'ic_art_4', 'ic_art_5', 'ic_art_6', 'ic_art_7', 'ic_art_8', 'ic_art_9', 'ic_art_10', 'ic_art_11', 'ic_art_12', 'ic_art_13', 'ic_art_14', 'ic_art_15', 'ic_ab_blade', 'ic_ab_dash', 'ic_ab_walljump', 'ic_ab_blink', 'ic_ab_double', 'ic_ab_ring', 'ic_ab_comet', 'ic_art_comete', 'ic_art_comete2', 'ic_art_ascension', 'ic_art_ascension2', 'ic_art_chute', 'ic_art_chute2', 'ic_art_constellation',
];

export const ATLASES = [
  'hero_idle', 'hero_idle_free', 'hero_run', 'hero_run_free', 'hero_jump', 'hero_jump_free', 'hero_slash', 'hero_slash2', 'hero_slash3', 'hero_slash_up',
  'hero_slash_down', 'hero_dash_wall', 'hero_throw', 'hero_palm', 'hero_heal', 'hero_hurt_death', 'hero_art',
  'en_coquille', 'en_acarien', 'en_lampion', 'en_pretre', 'en_chevalier', 'en_ronce', 'en_spore', 'en_mante', 'boss_gardien_a', 'boss_gardien_b', 'boss_gardien_c',
  'en_forgeron_mineur', 'en_enclume', 'en_etincelle', 'en_noye', 'en_anguille', 'en_geolier', 'en_corbeau', 'en_astre', 'en_ombre', 'en_sentinelle', 'en_veilleur',
  ...['racine', 'forgeron', 'bibliothecaire', 'geoliers', 'oiseau', 'reine', 'enchaineur'].flatMap((b) => [`boss_${b}_a`, `boss_${b}_b`, `boss_${b}_c`]), 'boss_etoile',
];

type Mode = 'loading' | 'title' | 'game';

/** The application: title, game, overlays, transitions, saving. */
export class App {
  view!: GameView;
  scene!: Scene;
  hud = new Hud();
  /** Screen-space UI, above the post-processed world. */
  ui = new Container();
  private overlay = new Container();
  private fade = new Graphics();
  private mode: Mode = 'loading';
  private title: TitleScreen | null = null;
  modals: Modal[] = [];
  settings: Settings = loadSettings();
  /** Fade state for room changes and deaths. */
  private fadeK = 1;
  private fadeTarget = 0;
  private onBlack: (() => void) | null = null;
  private deathT = 0;
  private promptText: Text;
  private zoneText: Text;
  private zoneT = 0;
  private lastZone = '';
  hooks!: GameHooks;
  private saveT = 0;

  constructor(public app: Application) {
    this.promptText = text('', style('menu', GOLD, 24, 'center'), 0, 0, 0.5, 1);
    this.zoneText = text('', style('title', 0xfff4dc, 64, 'center'), 960, 300, 0.5, 0.5);
  }

  async start(): Promise<void> {
    setLang(this.settings.lang);
    audio.setVolumes(this.settings.music, this.settings.sfx);
    if (this.settings.bindings) Object.assign(input.bindings, this.settings.bindings);
    registerLooks();
    await Promise.all([document.fonts.load('40px Cinzel'), document.fonts.load('40px "Cormorant Garamond"'), document.fonts.load('italic 500 40px "Cormorant Garamond"')]).catch(() => {});
    await loadAssets(IMAGES, ATLASES, (k) => {
      const bar = document.getElementById('loadbar');
      if (bar) bar.style.width = `${Math.round(k * 100)}%`;
    });
    const ld = document.getElementById('loading');
    if (ld) {
      ld.style.opacity = '0';
      setTimeout(() => ld.remove(), 900);
    }
    sound.play = (name, o) => audio.play(name, o);
    this.view = new GameView(this.app);
    this.app.stage.addChild(this.view.root, this.ui);
    this.ui.addChild(this.hud.c, this.promptText, this.zoneText, this.overlay, this.fade);
    this.fade.rect(0, 0, 1920, 1080).fill(0x000000);
    this.resize();
    window.addEventListener('resize', () => this.resize());
    this.view.camera.shakeScale = this.settings.shake;
    const q = new URLSearchParams(location.search);
    if (q.has('room')) {
      // Direct start (development and tests).
      const save = newSave(9);
      save.abilities = (q.get('ab') ?? 'dash,walljump,double,blink,ring').split(',');
      save.arts = ['comete', 'ascension', 'chute'];
      this.startGame(save, q.get('room')!);
      this.scene.lumen = 99;
    } else this.showTitle();
  }

  private resize(): void {
    this.view.resize();
    this.ui.scale.copyFrom(this.view.root.scale);
    this.ui.position.copyFrom(this.view.root.position);
  }

  showTitle(): void {
    this.mode = 'title';
    this.title?.c.destroy({ children: true });
    this.title = new TitleScreen({
      play: (slot, save, hard) => {
        const s = save ?? newSave(slot);
        if (!save) s.hard = hard;
        this.fadeTo(() => {
          this.title?.c.destroy({ children: true });
          this.title = null;
          if (save) this.startGame(s);
          else {
            // A new game opens on the story of the fall.
            const lines = tl('intro');
            audio.playMusic('title');
            this.push(new Cinematic(lines.map((l, i) => ({ image: `cut_intro${i + 1}`, lines: [l] })), () => this.fadeTo(() => this.startGame(s))));
          }
        });
      },
      options: () => this.openOptions(),
      credits: () => this.push(new Cinematic([{ lines: tl('credits'), title: 'VAEL' }], () => {})),
      quit: () => (window as unknown as { carillonDesktop?: { quit(): void }; vaelDesktop?: { quit(): void } }).vaelDesktop?.quit(),
    });
    this.overlay.addChild(this.title.c);
    this.view.root.visible = false;
    this.hud.c.visible = false;
  }

  startGame(save: Save, roomOverride?: string): void {
    this.mode = 'game';
    this.view.root.visible = true;
    this.hud.c.visible = true;
    this.scene = new Scene(save, { roomDef, neighbour, spawn: spawnEntity });
    this.hooks = new GameHooks(this);
    this.hooks.attach(this.scene);
    const room = roomOverride ?? save.shrineRoom;
    this.scene.enter(room);
    this.hooks.placeAtShrine();
    if (roomOverride) {
      const p = this.scene.player;
      p.body.x = 6 * 40;
      p.body.y = 10 * 40;
      for (let i = 0; i < 60 && !p.body.onGround; i++) this.scene.update(NO_CONTROLS);
    }
    // A new game: the spark wakes in its cocoon of crystal.
    if (!save.flags.awake && !roomOverride) {
      save.flags.awake = true;
      const p = this.scene.player;
      p.body.x = 14 * 40;
      p.body.y = 22 * 40 - p.body.h;
      p.wakeT = 130;
      p.facing = 1;
      setTimeout(() => {
        this.scene.emit({ t: 'heal', x: p.cx, y: p.cy });
        this.scene.emit({ t: 'shockwave', x: p.cx, y: p.cy, r: 300, color: 0xffe6b0 });
      }, 1500);
    }
    this.view.buildRoom(this.scene);
    this.view.camera.snap(this.scene.player.cx, this.scene.player.cy);
    this.fadeK = 1;
    this.fadeTarget = 0;
    this.enteredRoom();
  }

  /** Fades to black, runs fn, fades back. */
  fadeTo(fn: () => void): void {
    this.fadeTarget = 1;
    this.onBlack = fn;
  }

  hideZoneTitle(): void {
    this.zoneT = 0;
  }

  push(m: Modal): void {
    this.modals.push(m);
    this.overlay.addChild(m.c);
  }

  private enteredRoom(): void {
    const s = this.scene;
    this.hooks.entered();
    const z = s.room.def.zone;
    audio.playMusic(s.boss ? 'boss' : zoneLook(z).music);
    if (z !== this.lastZone && s.room.def.title !== false) {
      this.lastZone = z;
      this.zoneText.text = t(`zones.${z}`);
      this.zoneT = 1;
    }
  }

  openOptions(): void {
    const st = this.settings;
    const pct = (v: number) => `${Math.round(v * 100)} %`;
    const box = new ChoiceBox(t('options.title'), [
      { label: () => `${t('options.music')} : ${pct(st.music)}`, adjust: (d) => { st.music = Math.max(0, Math.min(1, st.music + d * 0.1)); audio.setVolumes(st.music, st.sfx); } },
      { label: () => `${t('options.sfx')} : ${pct(st.sfx)}`, adjust: (d) => { st.sfx = Math.max(0, Math.min(1, st.sfx + d * 0.1)); audio.setVolumes(st.music, st.sfx); audio.play('hit'); } },
      { label: () => `${t('options.shake')} : ${pct(st.shake)}`, adjust: (d) => { st.shake = Math.max(0, Math.min(1, st.shake + d * 0.25)); this.view.camera.shakeScale = st.shake; } },
      { label: () => `${t('options.lang')} : ${st.lang === 'fr' ? 'Français' : 'English'}`, adjust: () => { st.lang = st.lang === 'fr' ? 'en' : 'fr'; setLang(st.lang); }, action: () => { st.lang = st.lang === 'fr' ? 'en' : 'fr'; setLang(st.lang); } },
      { label: () => t('options.keys'), action: () => this.push(new ControlsScreen(() => { st.bindings = { ...input.bindings }; writeSettings(st); })) },
      { label: () => `${t('options.fullscreen')} : ${document.fullscreenElement ? t('options.on') : t('options.off')}`, action: () => { if (document.fullscreenElement) void document.exitFullscreen(); else void document.documentElement.requestFullscreen?.(); } },
      { label: () => t('menu.back'), action: () => { box.closed = true; } },
    ]);
    this.push(box);
    const orig = box.update.bind(box);
    box.update = () => {
      const done = orig();
      if (done) writeSettings(st);
      return done;
    };
  }

  private pause(): void {
    const box: ChoiceBox = new ChoiceBox(t('menu.resume').toUpperCase(), [
      { label: () => t('menu.resume'), action: () => { box.closed = true; } },
      { label: () => t('menu.inventory'), action: () => { box.closed = true; this.push(new Inventory(this.scene)); } },
      { label: () => t('menu.map'), action: () => { box.closed = true; this.push(new MapScreen(this.scene)); } },
      { label: () => t('menu.options'), action: () => this.openOptions() },
      { label: () => t('menu.quitTitle'), action: () => { box.closed = true; this.saveNow(); this.fadeTo(() => { this.clearModals(); this.showTitle(); }); } },
    ]);
    this.push(box);
  }

  clearModals(): void {
    for (const m of this.modals) m.c.destroy({ children: true });
    this.modals = [];
  }

  saveNow(): void {
    if (this.scene) writeSave(this.scene.save);
  }

  update(): void {
    // Fades.
    if (this.fadeTarget === 1) {
      this.fadeK = Math.min(1, this.fadeK + 0.12);
      if (this.fadeK >= 1 && this.onBlack) {
        const fn = this.onBlack;
        this.onBlack = null;
        fn();
        this.fadeTarget = 0;
      }
    } else this.fadeK = Math.max(0, this.fadeK - 0.07);
    this.fade.alpha = this.fadeK;

    if (this.mode === 'title') {
      if (this.modals.length) this.updateModals();
      else this.title?.update();
      return;
    }
    if (this.mode !== 'game') return;
    const s = this.scene;
    s.save.playTime += 1 / 60;
    if (++this.saveT % 1800 === 0) this.saveNow();

    if (this.modals.length) {
      const top = this.modals[this.modals.length - 1];
      this.updateModals();
      if (!top.live) {
        this.view.events(s.events, s);
        s.events.length = 0;
        return;
      }
    }
    if (input.pressed('pause') && !this.modals.length && !s.player.dead) {
      this.pause();
      return;
    }
    if (input.pressed('map') && !this.modals.length && !s.player.dead) {
      this.push(new MapScreen(s));
      return;
    }
    if (input.pressed('inv') && !this.modals.length && !s.player.dead) {
      this.push(new Inventory(s));
      return;
    }
    const c = this.fadeTarget === 1 || this.deathT > 0 ? NO_CONTROLS : readControls();
    // Interact.
    const near = this.nearInteractive();
    if (near && c.interactPressed && s.player.body.onGround && !s.player.attack) {
      near.interact();
      c.interactPressed = false;
    }
    s.update(c);
    this.hooks.update();
    if (s.pending && this.fadeTarget === 0) {
      const p = s.pending;
      this.fadeTo(() => {
        s.enter(p.def, p.x, p.y);
        s.player.body.vx = p.vx;
        s.player.body.vy = Math.min(p.vy, p.vy < 0 ? -14 : p.vy);
        this.view.buildRoom(s);
        this.view.camera.snap(s.player.cx, s.player.cy);
        this.enteredRoom();
      });
    }
    // Death.
    if (s.player.dead) {
      this.deathT++;
      if (this.deathT === 90) this.fadeTo(() => this.hooks.respawn());
    } else this.deathT = 0;
    this.view.events(s.events, s);
    s.events.length = 0;
  }

  private updateModals(): void {
    const top = this.modals[this.modals.length - 1];
    if (top.update()) {
      const i = this.modals.indexOf(top);
      if (i >= 0) this.modals.splice(i, 1);
      top.c.destroy({ children: true });
    }
  }

  private nearInteractive(): (Entity & Interactive) | null {
    for (const e of this.scene.entities) if (e instanceof Interactive && e.inRange() && e.prompt) return e;
    return null;
  }

  /** Test hook: runs n simulation frames with the given held actions, then draws once. */
  step(n: number, keys: string[]): void {
    input.script = new Set(keys as never[]);
    for (let i = 0; i < n; i++) {
      input.poll();
      this.update();
      this.render(1);
    }
  }

  render(steps: number): void {
    if (this.mode === 'game') {
      this.view.render(this.scene, steps);
      this.hud.update(this.scene);
      // Interaction prompt above the object.
      const near = this.nearInteractive();
      if (near && !this.modals.length) {
        const cam = this.view.camera;
        this.promptText.text = `▲ ${t(`prompt.${near.prompt}`)}`;
        this.promptText.position.set((near.cx - cam.rx) * cam.zoom, (near.body.y - 20 - cam.ry) * cam.zoom);
        this.promptText.alpha = 0.9;
      } else this.promptText.alpha = 0;
      if (this.zoneT > 0) {
        this.zoneT += 1;
        const k = this.zoneT < 60 ? (this.zoneT - 1) / 60 : this.zoneT > 200 ? Math.max(0, 1 - (this.zoneT - 200) / 60) : 1;
        this.zoneText.alpha = k;
        if (this.zoneT > 260) this.zoneT = 0;
      } else this.zoneText.alpha = 0;
    }
  }
}
