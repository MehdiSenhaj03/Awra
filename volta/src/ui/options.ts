import { audio } from '../engine/audio';
import { codeName, input, BINDABLE, DEFAULT_KEYS, type Action } from '../engine/input';
import { sfx } from '../engine/sfx';
import { V } from '../engine/synth';
import { type Settings, writeSettings } from '../game/meta';
import { getLang, setLang, t } from '../i18n';
import { el } from './dom';
import { Menu } from './menu';
import type { Screen } from './screen';

/** Options: volumes, shake, flashes, rhythm calibration, language, controls, fullscreen. */
export class Options implements Screen {
  root = el('div', 'screen');
  private menu!: Menu;
  private panel = el('div', 'opts panel');
  private sub: 'main' | 'calib' | 'keys' = 'main';
  private calibT = 0;
  private calibNext = 0;
  private taps: number[] = [];
  private clicks: number[] = [];
  private dot: HTMLElement | null = null;

  constructor(private st: Settings, private onBack: () => void, private onChange: () => void) {
    this.root.style.background = '#000000b8';
    this.root.appendChild(this.panel);
    this.build();
  }

  private build(): void {
    this.panel.innerHTML = '';
    this.panel.appendChild(el('div', 'h-title', t('options.title')));
    const m = el('div', 'menu');
    this.panel.appendChild(m);
    this.menu = new Menu(m);
    const st = this.st;
    const pct = (v: number) => `${Math.round(v * 100)} %`;
    const row = (label: string, value: () => string, adjust?: (d: number) => void, action?: () => void) => {
      const e = this.menu.add(`<b>${label}</b><span>${value()}</span>`, action ?? (() => adjust?.(1)), { adjust });
      const upd = () => (e.querySelector('span')!.textContent = value());
      if (adjust) {
        const it = this.menu.items[this.menu.items.length - 1];
        it.adjust = (d) => {
          adjust(d);
          upd();
          this.save();
        };
        it.action = () => {
          adjust(1);
          upd();
          this.save();
        };
      }
      return e;
    };
    row(t('options.music'), () => pct(st.music), (d) => { st.music = Math.max(0, Math.min(1, st.music + d * 0.1)); audio.setVolumes(st.music, st.sfx); });
    row(t('options.sfx'), () => pct(st.sfx), (d) => { st.sfx = Math.max(0, Math.min(1, st.sfx + d * 0.1)); audio.setVolumes(st.music, st.sfx); sfx('ui'); });
    row(t('options.shake'), () => pct(st.shake), (d) => { st.shake = Math.max(0, Math.min(1, st.shake + d * 0.25)); });
    row(t('options.flashes'), () => pct(st.flashes), (d) => { st.flashes = Math.max(0, Math.min(1, st.flashes + d * 0.25)); });
    row(t('options.latency'), () => `${Math.round(st.latency * 1000)} ms`, (d) => { st.latency = Math.max(-0.2, Math.min(0.3, st.latency + d * 0.005)); });
    this.menu.add(`<b>${t('options.calibrate')}</b><span>♪</span>`, () => this.calibrate());
    row(t('options.lang'), () => (st.lang === 'fr' ? 'Français' : 'English'), () => { st.lang = st.lang === 'fr' ? 'en' : 'fr'; setLang(st.lang); setTimeout(() => this.build(), 0); });
    this.menu.add(`<b>${t('options.controls')}</b><span>⌨</span>`, () => this.keys());
    row(t('options.fullscreen'), () => (document.fullscreenElement ? t('options.on') : t('options.off')), () => {
      if (document.fullscreenElement) void document.exitFullscreen();
      else void document.documentElement.requestFullscreen?.();
      st.fullscreen = !document.fullscreenElement;
    });
    this.menu.add(`<b>${t('common.back')}</b>`, () => this.onBack());
    void getLang;
  }

  private save(): void {
    writeSettings(this.st);
    this.onChange();
  }

  /** Clicks on a steady 100 BPM; the player taps along; the mean offset becomes the latency setting. */
  private calibrate(): void {
    this.sub = 'calib';
    this.panel.innerHTML = '';
    this.panel.appendChild(el('div', 'h-title', t('options.latency')));
    const c = el('div', 'calib', `${t('options.calibHelp')}<span class="dot"></span>`);
    this.dot = c.querySelector('.dot');
    this.panel.appendChild(c);
    this.calibT = 0;
    this.calibNext = 0;
    this.taps = [];
    this.clicks = [];
  }

  private keys(): void {
    this.sub = 'keys';
    this.panel.innerHTML = '';
    this.panel.appendChild(el('div', 'h-title', t('options.controls')));
    const m = el('div', 'menu');
    this.panel.appendChild(m);
    this.menu = new Menu(m);
    for (const a of BINDABLE) {
      const e = this.menu.add(`<b>${t(`controls.${a}`)}</b><span>${input.bindings[a].map(codeName).join(' / ')}</span>`, () => {
        e.querySelector('span')!.textContent = t('options.press');
        input.capture = (code) => {
          input.bindings[a as Action] = [code, ...input.bindings[a as Action].filter((c) => c !== code)].slice(0, 2);
          this.st.bindings = { ...input.bindings };
          this.save();
          e.querySelector('span')!.textContent = input.bindings[a].map(codeName).join(' / ');
        };
      });
    }
    this.menu.add(`<b>${t('controls.reset')}</b>`, () => {
      input.bindings = structuredClone(DEFAULT_KEYS);
      this.st.bindings = null;
      this.save();
      this.keys();
    });
    this.menu.add(`<b>${t('common.back')}</b>`, () => {
      this.sub = 'main';
      this.build();
    });
  }

  update(dt: number): void {
    if (this.sub === 'calib') {
      this.calibT += dt;
      const now = audio.now;
      if (this.calibT > this.calibNext && this.clicks.length < 16) {
        const when = now + 0.05;
        V.pluck(when, 84, { dest: audio.sfx }, 0.6, 0.05, 'square', 8000);
        this.clicks.push(when + audio.latency);
        this.calibNext += 0.6;
        setTimeout(() => this.dot && (this.dot.style.transform = 'scale(1.6)'), 50);
        setTimeout(() => this.dot && (this.dot.style.transform = 'scale(1)'), 150);
      }
      if (input.pressed('dash') || input.pressed('confirm') || input.pressed('fire')) {
        const tap = audio.now;
        const near = this.clicks.reduce((b, c) => (Math.abs(c - tap) < Math.abs(b - tap) ? c : b), Infinity);
        if (Math.abs(near - tap) < 0.25) this.taps.push(tap - near);
      }
      if (this.clicks.length >= 16 && this.calibT > this.calibNext + 0.4) {
        if (this.taps.length >= 6) {
          const sorted = [...this.taps].sort((a, b) => a - b);
          this.st.latency = Math.round(sorted[Math.floor(sorted.length / 2)] * 1000) / 1000;
          this.save();
        }
        this.sub = 'main';
        this.build();
      }
      if (input.pressed('back')) {
        this.sub = 'main';
        this.build();
      }
      return;
    }
    if (input.capture) return;
    if (input.pressed('back')) {
      if (this.sub === 'keys') {
        this.sub = 'main';
        this.build();
      } else this.onBack();
      return;
    }
    this.menu.handle();
  }
}
