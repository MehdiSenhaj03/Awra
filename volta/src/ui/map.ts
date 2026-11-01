import { input } from '../engine/input';
import { sfx } from '../engine/sfx';
import { GRADES, type Save, stageOpen } from '../game/meta';
import { ORDER, STAGES } from '../game/stages';
import { t } from '../i18n';
import { el } from './dom';
import { SECTOR_BG, SECTOR_COLOR, type Screen, img } from './screen';

/** The Score: six sectors, four movements each, grades earned. */
export class MapScreen implements Screen {
  root = el('div', 'screen');
  private nodes: { id: string; el: HTMLElement; open: boolean; sector: HTMLElement }[] = [];
  private index = 0;
  private strip = el('div', 'sectors');

  constructor(save: Save, focus: string | null, private onPick: (id: string) => void, private onBack: () => void) {
    const bg = el('div', 'bg');
    bg.style.backgroundImage = `url(${img('bg_title')})`;
    bg.style.filter = 'blur(18px) brightness(0.35)';
    const map = el('div', 'map');
    const head = el('div', 'map-head');
    head.append(el('div', 'h-title', t('map.title')), el('div', 'h-sub', t('map.subtitle').toUpperCase()));
    const notes = el('div', 'map-notes', `<i>${t('common.notes').toUpperCase()}</i>${save.notes.toLocaleString('fr-FR')}`);
    for (let s = 1; s <= 6; s++) {
      const sec = el('div', 'sector');
      sec.style.setProperty('--sc', SECTOR_COLOR[s]);
      const art = el('div', 'art');
      art.style.backgroundImage = `url(${img(SECTOR_BG[s])})`;
      const info = el('div', 'info');
      const emb = el('img', 'emb');
      emb.src = img(`ic_b${7 + s}`);
      info.append(emb, el('div', 'num', `${String(s).padStart(2, '0')} / 06`), el('div', 'nm', t(`sector.${s}.name`)), el('div', 'ds', t(`sector.${s}.desc`)));
      const st = el('div', 'stages');
      let anyOpen = false;
      for (let k = 1; k <= 4; k++) {
        const id = `s${s}_${k}`;
        const rec = save.stages[id];
        const open = stageOpen(save, id);
        anyOpen ||= open;
        const g = rec?.cleared ? GRADES[rec.rank] : '·';
        const b = el('button', `stg${k === 4 ? ' boss' : ''}${open ? '' : ' locked'}`, `<div class="g${rec?.cleared ? ' got' : ''}">${g}</div><div class="l">${k === 4 ? t('map.boss').split(' ')[0] : `${s}.${k}`}</div>${rec?.fragment ? '<span class="frag">◆</span>' : ''}`);
        const ni = this.nodes.length;
        b.addEventListener('mouseenter', () => this.focus(ni));
        b.addEventListener('click', (e) => {
          e.stopPropagation();
          this.focus(ni);
          this.pick();
        });
        this.nodes.push({ id, el: b, open, sector: sec });
        st.appendChild(b);
      }
      info.appendChild(st);
      sec.append(art, info);
      if (!anyOpen) sec.classList.add('locked');
      this.strip.appendChild(sec);
    }
    const foot = el('div', 'map-foot keys', `<span><kbd>←</kbd><kbd>→</kbd>${t('map.subtitle')}</span><span><kbd>${t('keys.enter')}</kbd>${t('common.start')}</span><span><kbd>${t('keys.escape')}</kbd>${t('common.back')}</span>`);
    map.append(head, this.strip, foot);
    this.root.append(bg, map, notes);
    // Focus: the requested stage, else the furthest open one.
    let fi = this.nodes.findIndex((n) => n.id === focus);
    if (fi < 0) {
      fi = 0;
      for (let i = 0; i < this.nodes.length; i++) if (this.nodes[i].open && !save.stages[this.nodes[i].id]?.cleared) {
        fi = i;
        break;
      }
    }
    this.focus(fi, true);
    void ORDER;
  }

  private focus(i: number, quiet = false): void {
    this.nodes[this.index]?.el.classList.remove('on');
    this.nodes[this.index]?.sector.classList.remove('on');
    this.index = Math.max(0, Math.min(this.nodes.length - 1, i));
    const n = this.nodes[this.index];
    n.el.classList.add('on');
    n.sector.classList.add('on');
    const s = STAGES[n.id].sector;
    const w = (this.strip.firstElementChild as HTMLElement)?.offsetWidth ?? 400;
    this.strip.style.transform = `translateX(${-Math.max(0, s - 2) * (w + 18)}px)`;
    if (!quiet) sfx('ui', 1 + (this.index % 4) * 0.06);
  }

  private pick(): void {
    const n = this.nodes[this.index];
    if (!n.open) {
      sfx('block');
      return;
    }
    sfx('uiOk');
    this.onPick(n.id);
  }

  update(): void {
    if (input.pressed('left')) this.focus(this.index - 1);
    if (input.pressed('right')) this.focus(this.index + 1);
    if (input.pressed('up')) this.focus(this.index - 4);
    if (input.pressed('down')) this.focus(this.index + 4);
    if (input.pressed('confirm') || input.pressed('fire')) this.pick();
    if (input.pressed('back')) this.onBack();
  }
}
