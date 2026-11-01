import { input } from '../engine/input';
import { sfx } from '../engine/sfx';
import { GRADES } from '../game/meta';
import { RANKS, type World } from '../game/world';
import { t } from '../i18n';
import { el } from './dom';
import { Menu } from './menu';
import type { Screen } from './screen';

const GRADE_COLOR = ['#7d8aa8', '#7fb8ff', '#2ef2ff', '#40ffb0', '#ffc34a', '#ff2fb9'];

/** End of a stage: the grade slams in, the numbers roll. */
export class Results implements Screen {
  root = el('div', 'screen results');
  private menu: Menu;

  constructor(w: World, win: boolean, grade: number, record: boolean, notes: number, actions: { label: string; action: () => void }[]) {
    const wrap = el('div', 'res-wrap');
    const g = el('div', 'res-grade');
    g.style.setProperty('--gc', win ? GRADE_COLOR[grade] : '#ff3050');
    g.innerHTML = `${win ? GRADES[grade] : '✕'}<small>${win ? t('results.grade') : ''}</small>`;
    const list = el('div', 'res-list');
    list.appendChild(el('div', 'h-title', win ? t('results.clear') : t('results.fail')));
    const s = w.stats;
    const sec = Math.round(s.frames / 60);
    const judged = s.perfects + s.greats + s.offs;
    const rows: [string, string, boolean?][] = [
      [t('results.score'), Math.round(w.score).toLocaleString('fr-FR'), true],
      [t('results.time'), `${Math.floor(sec / 60)}:${String(sec % 60).padStart(2, '0')}`],
      [t('results.perfects'), `${s.perfects}${judged ? ` (${Math.round((s.perfects / judged) * 100)} %)` : ''}`],
      [t('results.parries'), String(s.parries)],
      [t('results.grazes'), String(s.grazes)],
      [t('results.hits'), String(s.hits)],
      [t('results.maxRank'), `${RANKS[s.maxRank]} — ${t(`rank.${s.maxRank}`)}`],
      [t('results.notes'), `+${notes} ♪`],
    ];
    rows.forEach(([k, v, big], i) => {
      const r = el('div', `res-row${big ? ' big' : ''}`, `<span>${k}</span><b>${v}</b>`);
      r.style.animationDelay = `${0.15 + i * 0.09}s`;
      list.appendChild(r);
    });
    if (record) list.appendChild(el('div', 'record', t('results.record')));
    const m = el('div', 'menu res-menu');
    list.appendChild(m);
    this.menu = new Menu(m);
    for (const a of actions) this.menu.add(a.label, a.action);
    wrap.append(g, list);
    this.root.appendChild(wrap);
    if (win) setTimeout(() => sfx('rankUp', 1.2), 900);
  }

  update(): void {
    this.menu.handle();
    void input;
  }
}
