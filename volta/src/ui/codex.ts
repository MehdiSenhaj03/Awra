import { input } from '../engine/input';
import { type Save, cleared } from '../game/meta';
import { t } from '../i18n';
import { el } from './dom';
import { Menu } from './menu';
import { type Screen, img } from './screen';

const FRAGMENTS = [1, 2, 3, 4, 5, 6].flatMap((s) => [1, 2, 3].map((k) => `f${s}_${k}`));
const ENEMIES: [string, string][] = [
  ['mote', 's1_1'], ['spitter', 's1_1'], ['spinner', 's1_1'], ['lancer', 's1_1'], ['splitter', 's1_1'], ['bulwark', 's1_3'],
  ['mirror', 's2_1'], ['phantom', 's2_1'], ['mortar', 's3_1'], ['weaver', 's3_1'], ['sniper', 's4_1'], ['choir', 's4_1'], ['echo', 's1_1'],
];
const BOSSES = ['ostinato', 'crescendo', 'fugue', 'rubato', 'staccato', 'fermata'];

/** Memories, the Mutes and the Conductors. */
export class Codex implements Screen {
  root = el('div', 'screen');
  private tabs: Menu;
  private list!: Menu;
  private listEl = el('div', 'codex-list panel');
  private detail = el('div', 'codex-detail panel');
  private tab = 0;
  private focusTabs = true;

  constructor(private save: Save, private onBack: () => void) {
    const bg = el('div', 'bg');
    bg.style.backgroundImage = `url(${img('bg_title')})`;
    bg.style.filter = 'blur(16px) brightness(0.3)';
    const c = el('div', 'codex');
    c.appendChild(el('div', 'h-title', t('codex.title')));
    const tabsEl = el('div', 'tabs menu h');
    this.tabs = new Menu(tabsEl, true);
    ['fragments', 'bestiary', 'conductors'].forEach((k, i) => this.tabs.add(t(`codex.${k}`).toUpperCase(), () => this.show(i)));
    const body = el('div', 'codex-body');
    body.append(this.listEl, this.detail);
    c.append(tabsEl, body);
    this.root.append(bg, c);
    this.show(0);
  }

  private show(tab: number): void {
    this.tab = tab;
    this.listEl.innerHTML = '';
    const m = el('div', 'menu');
    this.listEl.appendChild(m);
    this.list = new Menu(m);
    const s = this.save;
    const entries: { label: string; html: () => string }[] = [];
    if (tab === 0) {
      for (const f of FRAGMENTS) {
        const got = s.fragments.includes(f);
        entries.push({ label: got ? t(`fragments.${f}.name`) : t('codex.unknown'), html: () => (got ? `<div class="t">${t(`fragments.${f}.name`)}</div><div class="d">${t(`fragments.${f}.text`)}</div>` : `<div class="t">${t('codex.unknown')}</div><div class="d">${t('codex.hidden')}</div>`) });
      }
    } else if (tab === 1) {
      for (const [e, st] of ENEMIES) {
        const seen = cleared(s, st);
        entries.push({ label: seen ? t(`enemy.${e}.name`) : t('codex.unknown'), html: () => (seen ? `<div class="t">${t(`enemy.${e}.name`)}</div><div class="d">${t(`enemy.${e}.desc`)}</div>` : `<div class="t">${t('codex.unknown')}</div>`) });
      }
    } else {
      BOSSES.forEach((b, i) => {
        const seen = cleared(s, `s${i + 1}_4`);
        entries.push({ label: seen ? t(`boss.${b}.name`) : t('codex.unknown'), html: () => (seen ? `<img src="${img(`boss_${b}`)}" alt=""><div class="t">${t(`boss.${b}.name`)}</div><div class="h-sub">${t(`boss.${b}.title`).toUpperCase()}</div><div class="d" style="margin-top:16px">${t(`boss.${b}.desc`)}</div><div class="d" style="margin-top:16px;color:#9fe">${t(`story.after.${b}`)}</div>` : `<div class="t">${t('codex.unknown')}</div>`) });
      });
    }
    entries.forEach((en) => {
      const e = this.list.add(en.label, () => (this.focusTabs = false));
      e.addEventListener('mouseenter', () => (this.detail.innerHTML = en.html()));
    });
    this.entries = entries;
    this.detail.innerHTML = entries[0]?.html() ?? '';
  }

  private entries: { label: string; html: () => string }[] = [];

  update(): void {
    if (input.pressed('back')) {
      if (!this.focusTabs) this.focusTabs = true;
      else this.onBack();
      return;
    }
    if (this.focusTabs) {
      if (this.tabs.handle()) this.show(this.tabs.index);
      if (input.pressed('down')) this.focusTabs = false;
    } else {
      const before = this.list.index;
      if (input.pressed('up') && this.list.index === 0) {
        this.focusTabs = true;
        return;
      }
      this.list.handle();
      if (this.list.index !== before) this.detail.innerHTML = this.entries[this.list.index].html();
    }
    void this.tab;
  }
}
