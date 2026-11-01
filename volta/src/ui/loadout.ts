import { input } from '../engine/input';
import { sfx } from '../engine/sfx';
import { OVERTONES, type Save, WEAPON_UPGRADE, slots, weaponUnlocked, writeSave } from '../game/meta';
import { STAGES } from '../game/stages';
import { WEAPON_ORDER } from '../game/weapons';
import { t } from '../i18n';
import { el } from './dom';
import { SECTOR_BG, type Screen, img } from './screen';

const WEAPON_ICON: Record<string, string> = { prism: 'ic_a0', chord: 'ic_a1', lance: 'ic_a2', arc: 'ic_a3', swarm: 'ic_a4' };

interface Node {
  el: HTMLElement;
  kind: 'weapon' | 'overtone' | 'play';
  id: string;
  row: number;
}

/** Before a stage: pick a weapon, equip Overtones, spend Notes. */
export class Loadout implements Screen {
  root = el('div', 'screen');
  private nodes: Node[] = [];
  private index = 0;
  private detail = el('div', 'detail panel');
  private notesEl = el('div', 'map-notes');
  private slotsEl = el('div', 'lbl');

  constructor(private save: Save, private stageId: string, private onPlay: () => void, private onBack: () => void) {
    const def = STAGES[stageId];
    const bg = el('div', 'bg');
    bg.style.backgroundImage = `url(${img(SECTOR_BG[def.sector])})`;
    bg.style.filter = 'blur(10px) brightness(0.3)';
    const lo = el('div', 'loadout');
    const left = el('div', 'lo-col');
    left.append(el('div', 'h-title', t('loadout.title')), el('div', 'h-sub', `${t(`sector.${def.sector}.name`).toUpperCase()} — ${t(`stage.${def.index}`).toUpperCase()}`));
    left.appendChild(el('div', 'lbl', t('loadout.weapon').toUpperCase()));
    const wc = el('div', 'cards');
    wc.style.gridTemplateColumns = 'repeat(5, 1fr)';
    for (const w of WEAPON_ORDER) this.addCard(wc, 'weapon', w, 0);
    left.appendChild(wc);
    left.append(this.slotsEl);
    const oc = el('div', 'cards');
    oc.style.gridTemplateColumns = 'repeat(4, 1fr)';
    OVERTONES.forEach((o, i) => this.addCard(oc, 'overtone', o.id, 1 + Math.floor(i / 4)));
    left.appendChild(oc);
    const right = el('div', 'lo-col');
    right.style.justifyContent = 'flex-end';
    const go = el('button', 'go', t('loadout.play').toUpperCase());
    const gi = this.nodes.length;
    go.addEventListener('mouseenter', () => this.focus(gi));
    go.addEventListener('click', () => this.activate(gi));
    this.nodes.push({ el: go, kind: 'play', id: 'play', row: 5 });
    right.append(this.detail, el('div', 'lbl', t('loadout.hint')), go);
    lo.append(left, right);
    this.root.append(bg, lo, this.notesEl);
    this.refresh();
    this.focus(this.nodes.length - 1, true);
  }

  private addCard(parent: HTMLElement, kind: 'weapon' | 'overtone', id: string, row: number): void {
    const b = el('button', 'card');
    const i = this.nodes.length;
    b.addEventListener('mouseenter', () => this.focus(i));
    b.addEventListener('click', () => this.activate(i));
    b.addEventListener('contextmenu', (e) => {
      e.preventDefault();
      this.focus(i);
      this.upgrade();
    });
    this.nodes.push({ el: b, kind, id, row });
    parent.appendChild(b);
  }

  private refresh(): void {
    const s = this.save;
    this.notesEl.innerHTML = `<i>${t('common.notes').toUpperCase()}</i>${s.notes.toLocaleString('fr-FR')}`;
    this.slotsEl.textContent = `${t('loadout.overtones').toUpperCase()} — ${t('loadout.slots', { n: s.equipped.length, max: slots(s) })}`;
    for (const n of this.nodes) {
      if (n.kind === 'weapon') {
        const open = weaponUnlocked(s, n.id);
        const lv = s.weaponLevels[n.id] ?? 1;
        n.el.className = `card${open ? '' : ' lock'}${s.weapon === n.id ? ' eq' : ''}${n === this.nodes[this.index] ? ' on' : ''}`;
        n.el.innerHTML = `<img src="${img(WEAPON_ICON[n.id])}" alt=""><div><div class="t">${open ? t(`weapon.${n.id}.name`) : '???'}</div><div class="p">${open ? `${t('common.level')} ${lv}` : ''}</div></div>`;
      } else if (n.kind === 'overtone') {
        const o = OVERTONES.find((x) => x.id === n.id)!;
        const owned = s.owned.includes(o.id);
        n.el.className = `card${owned ? '' : ' lock'}${s.equipped.includes(o.id) ? ' eq' : ''}${n === this.nodes[this.index] ? ' on' : ''}`;
        n.el.innerHTML = `<img src="${img(`ic_${o.icon}`)}" alt=""><div><div class="t">${t(`overtone.${o.id}.name`)}</div><div class="p">${owned ? '' : `${o.cost} ♪`}</div></div>`;
      }
    }
    this.showDetail();
  }

  private showDetail(): void {
    const n = this.nodes[this.index];
    const s = this.save;
    if (!n || n.kind === 'play') {
      this.detail.innerHTML = `<div class="t">${t(`sector.${STAGES[this.stageId].sector}.name`)}</div><div class="d">${t(`sector.${STAGES[this.stageId].sector}.desc`)}</div>`;
      return;
    }
    if (n.kind === 'weapon') {
      const open = weaponUnlocked(s, n.id);
      if (!open) {
        this.detail.innerHTML = `<div class="t">???</div><div class="d">${t('common.locked')}</div>`;
        return;
      }
      const lv = s.weaponLevels[n.id] ?? 1;
      const cost = WEAPON_UPGRADE[lv];
      this.detail.innerHTML = `<div class="t">${t(`weapon.${n.id}.name`)} · ${t('common.level')} ${lv}</div><div class="d">${t(`weapon.${n.id}.desc`)}</div><div class="p">${cost ? `${t('common.upgrade')} : ${cost} ♪ — [${input.keyName('pulse')}]` : t('common.max')}</div>`;
      return;
    }
    const o = OVERTONES.find((x) => x.id === n.id)!;
    const owned = s.owned.includes(o.id);
    this.detail.innerHTML = `<div class="t">${t(`overtone.${o.id}.name`)}</div><div class="d">${t(`overtone.${o.id}.desc`)}</div><div class="p">${owned ? (s.equipped.includes(o.id) ? t('common.unequip') : t('common.equip')) : `${t('common.buy')} : ${o.cost} ♪`}</div>`;
  }

  private focus(i: number, quiet = false): void {
    this.nodes[this.index]?.el.classList.remove('on');
    this.index = Math.max(0, Math.min(this.nodes.length - 1, i));
    this.nodes[this.index].el.classList.add('on');
    this.showDetail();
    if (!quiet) sfx('ui', 1 + (this.index % 5) * 0.05);
  }

  private activate(i: number): void {
    this.focus(i, true);
    const n = this.nodes[i];
    const s = this.save;
    if (n.kind === 'play') {
      sfx('uiOk');
      this.onPlay();
      return;
    }
    if (n.kind === 'weapon') {
      if (!weaponUnlocked(s, n.id)) return sfx('block');
      s.weapon = n.id;
      sfx('uiOk');
    } else {
      const o = OVERTONES.find((x) => x.id === n.id)!;
      if (!s.owned.includes(o.id)) {
        if (s.notes < o.cost) return sfx('block');
        s.notes -= o.cost;
        s.owned.push(o.id);
        sfx('fragment');
      } else if (s.equipped.includes(o.id)) {
        s.equipped = s.equipped.filter((x) => x !== o.id);
        sfx('ui', 0.8);
      } else {
        if (s.equipped.length >= slots(s)) return sfx('block');
        s.equipped.push(o.id);
        sfx('uiOk');
      }
    }
    writeSave(s);
    this.refresh();
  }

  private upgrade(): void {
    const n = this.nodes[this.index];
    if (n.kind !== 'weapon' || !weaponUnlocked(this.save, n.id)) return;
    const lv = this.save.weaponLevels[n.id] ?? 1;
    const cost = WEAPON_UPGRADE[lv];
    if (!cost || this.save.notes < cost) return sfx('block');
    this.save.notes -= cost;
    this.save.weaponLevels[n.id] = lv + 1;
    writeSave(this.save);
    sfx('fragment');
    this.refresh();
  }

  update(): void {
    const n = this.nodes[this.index];
    const sameRow = (d: number) => {
      const j = this.index + d;
      if (this.nodes[j] && this.nodes[j].row === n.row) this.focus(j);
    };
    const rowJump = (d: number) => {
      const target = n.row + d;
      const cands = this.nodes.map((x, i) => [x, i] as const).filter(([x]) => x.row === target);
      if (!cands.length) return;
      const col = this.nodes.filter((x) => x.row === n.row).indexOf(n);
      this.focus(cands[Math.min(col, cands.length - 1)][1]);
    };
    if (input.pressed('left')) sameRow(-1);
    if (input.pressed('right')) sameRow(1);
    if (input.pressed('up')) rowJump(-1);
    if (input.pressed('down')) rowJump(1);
    if (input.pressed('confirm') || input.pressed('fire')) this.activate(this.index);
    if (input.pressed('pulse')) this.upgrade();
    if (input.pressed('back')) this.onBack();
  }
}
