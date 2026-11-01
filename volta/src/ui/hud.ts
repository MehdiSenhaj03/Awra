import type { StageRunner } from '../game/stage';
import { RANKS, RANK_AT, type World } from '../game/world';
import { t } from '../i18n';
import { el } from './dom';

const RANK_COLORS = ['#7d8aa8', '#7fb8ff', '#2ef2ff', '#40ffb0', '#ffc34a', '#ff7a2f', '#ff2fb9'];

/** In-game heads-up display (DOM). */
export class Hud {
  root = el('div', 'hud');
  private hp = el('div', 'hud-hp');
  private gauge = el('div', 'hud-gauge');
  private gaugeB = el('b');
  private gaugeL = el('div', 'hud-gauge-label');
  private score = el('div', 'hud-score');
  private mult = el('div', 'hud-mult');
  private rank = el('div', 'hud-rank');
  private rankR = el('div', 'r');
  private rankN = el('div', 'name');
  private rankBar = el('b');
  private wave = el('div', 'hud-wave');
  private boss = el('div', 'hud-boss');
  private bossName = el('div', 'bn');
  private bossB = el('b');
  private bossS = el('s');
  private center = el('div', 'hud-center');
  private hint = el('div', 'hud-hint');
  private hurt = el('div', 'vignette-hurt');
  private shownScore = 0;
  private lastRank = -1;
  private lastHp = -1;
  private hintT = 0;

  constructor() {
    const tl = el('div', 'hud-tl');
    this.gauge.appendChild(this.gaugeB);
    tl.append(this.hp, this.gauge, this.gaugeL);
    const tr = el('div', 'hud-tr');
    tr.append(this.score, this.mult);
    const bar = el('div', 'bar');
    bar.appendChild(this.rankBar);
    this.rank.append(this.rankR, this.rankN, bar);
    const bb = el('div', 'bb');
    bb.append(this.bossS, this.bossB);
    this.boss.append(this.bossName, bb);
    this.root.append(this.hurt, tl, tr, this.rank, this.wave, this.boss, this.center, this.hint);
  }

  banner(text: string, sub = ''): void {
    this.center.innerHTML = '';
    const b = el('div', 'banner', `${text}${sub ? `<small>${sub}</small>` : ''}`);
    this.center.appendChild(b);
  }

  showHint(html: string, frames = 360): void {
    this.hint.innerHTML = html;
    this.hintT = frames;
  }

  update(w: World, run: StageRunner | null): void {
    const p = w.player;
    if (p.hp !== this.lastHp || this.hp.children.length !== p.maxHp) {
      this.hp.innerHTML = '';
      for (let i = 0; i < p.maxHp; i++) this.hp.appendChild(el('i', i < p.hp ? '' : 'off'));
      this.lastHp = p.hp;
    }
    this.hurt.style.opacity = p.hp === 1 ? String(0.35 + Math.sin(performance.now() / 180) * 0.15) : '0';
    this.gaugeB.style.width = `${w.gauge}%`;
    const full = w.gauge >= 100;
    this.gauge.classList.toggle('full', full || p.overdrive);
    this.gaugeL.textContent = p.overdrive ? t('hud.overdriveOn') : full ? t('hud.overdriveReady') : t('hud.overdrive');
    this.gaugeL.classList.toggle('ready', full || p.overdrive);
    this.shownScore += (w.score - this.shownScore) * 0.2;
    if (Math.abs(w.score - this.shownScore) < 1) this.shownScore = w.score;
    this.score.textContent = Math.round(this.shownScore).toLocaleString('fr-FR');
    this.mult.textContent = `×${w.mult}`;
    const r = w.rank;
    if (r !== this.lastRank) {
      this.rank.style.setProperty('--rc', RANK_COLORS[r]);
      this.rankR.textContent = RANKS[r];
      this.rankN.textContent = t(`rank.${r}`);
      this.rank.classList.remove('pop');
      void this.rank.offsetWidth;
      if (r > this.lastRank && this.lastRank >= 0) this.rank.classList.add('pop');
      this.lastRank = r;
    }
    const lo = RANK_AT[r];
    const hi = RANK_AT[r + 1] ?? 1000;
    this.rankBar.style.width = `${Math.min(100, ((w.style - lo) / (hi - lo)) * 100)}%`;
    if (run) this.wave.textContent = run.boss ? '' : `${t('hud.wave')} ${run.waveNumber} / ${run.total - (run.def.boss ? 1 : 0)}`;
    const boss = run?.boss;
    this.boss.classList.toggle('on', !!boss && !boss.dead);
    if (boss) {
      this.bossName.textContent = t(`boss.${run!.def.boss}.name`);
      const k = Math.max(0, boss.hp / boss.maxHp) * 100;
      this.bossB.style.width = `${k}%`;
      this.bossS.style.width = `${k}%`;
    }
    if (this.hintT > 0 && --this.hintT === 0) this.hint.style.opacity = '0';
    else if (this.hintT > 0) this.hint.style.opacity = '1';
  }
}
