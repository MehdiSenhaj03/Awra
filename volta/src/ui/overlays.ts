import { input } from '../engine/input';
import { sfx } from '../engine/sfx';
import { t } from '../i18n';
import { el } from './dom';
import { Menu } from './menu';
import { PORTRAIT, type Screen, img } from './screen';

/** Dialogue lines with portraits. Any key advances; the last one closes. */
export class Dialogue implements Screen {
  root = el('div', 'screen');
  private i = 0;
  private box = el('div', 'dlg');
  private pt = el('div', 'pt');
  private who = el('div', 'who');
  private txt = el('div', 'txt');
  private shown = 0;
  private full = '';
  done = false;

  constructor(private lines: [string, string][], private onDone: () => void) {
    const b = el('div', 'box panel');
    b.append(this.who, this.txt, el('div', 'nxt', '▶'));
    this.box.append(this.pt, b);
    this.root.appendChild(this.box);
    this.root.addEventListener('click', () => this.next());
    this.show();
  }

  private show(): void {
    const [who, text] = this.lines[this.i];
    this.who.textContent = t(`names.${who}`);
    this.pt.style.backgroundImage = PORTRAIT[who] ? `url(${img(PORTRAIT[who])})` : '';
    this.full = text;
    this.shown = 0;
    this.txt.textContent = '';
    this.box.style.animation = 'none';
    void this.box.offsetWidth;
    this.box.style.animation = '';
    sfx('ui', 0.8);
  }

  private next(): void {
    if (this.shown < this.full.length) {
      this.shown = this.full.length;
      this.txt.textContent = this.full;
      return;
    }
    this.i++;
    if (this.i >= this.lines.length) {
      this.done = true;
      this.onDone();
      return;
    }
    this.show();
  }

  update(dt: number): void {
    if (this.shown < this.full.length) {
      this.shown = Math.min(this.full.length, this.shown + dt * 70);
      this.txt.textContent = this.full.slice(0, Math.floor(this.shown));
    }
    if (input.pressed('confirm') || input.pressed('fire') || input.pressed('dash') || input.pressed('back')) this.next();
  }
}

/** A short announcement: new ability, weapon, slot. */
export class Unlock implements Screen {
  root = el('div', 'screen');
  private t = 0;
  constructor(kicker: string, name: string, desc: string, icon: string | null, private onDone: () => void) {
    const u = el('div', 'unlock panel');
    if (icon) {
      const i = el('img');
      i.src = img(icon);
      u.appendChild(i);
    }
    u.append(el('div', 'k', kicker), el('div', 'n', name), el('div', 'd', desc));
    this.root.appendChild(u);
    this.root.addEventListener('click', () => this.close());
    sfx('fragment');
  }
  private close(): void {
    if (this.t < 0.6) return;
    this.t = -99;
    this.onDone();
  }
  update(dt: number): void {
    this.t += dt;
    if (input.pressed('confirm') || input.pressed('fire') || input.pressed('back') || input.pressed('dash')) this.close();
  }
}

/** The Conductor's name across the screen (non-blocking). */
export class BossCard implements Screen {
  root = el('div', 'bosscard');
  private t = 0;
  done = false;
  constructor(id: string) {
    const art = el('div', 'art');
    art.style.backgroundImage = `url(${img(`boss_${id}`)})`;
    this.root.append(el('div', 'stripe'), art, el('div', 'nm', t(`boss.${id}.name`)), el('div', 'tt', t(`boss.${id}.title`)));
  }
  update(dt: number): void {
    this.t += dt;
    if (this.t > 3.5) this.done = true;
  }
}

/** Pause menu. */
export class Pause implements Screen {
  root = el('div', 'screen');
  private menu: Menu;
  constructor(actions: { resume: () => void; restart: () => void; options: () => void; quit: () => void }) {
    this.root.style.background = '#000000a8';
    const p = el('div', 'opts panel');
    p.appendChild(el('div', 'h-title', t('pause.title')));
    const m = el('div', 'menu');
    p.appendChild(m);
    this.menu = new Menu(m);
    this.menu.add(t('pause.resume'), actions.resume);
    this.menu.add(t('pause.restart'), actions.restart);
    this.menu.add(t('pause.options'), actions.options);
    this.menu.add(t('pause.quit'), actions.quit);
    this.root.appendChild(p);
    this.resume = actions.resume;
  }
  private resume: () => void;
  update(): void {
    if (input.pressed('pause') || input.pressed('back')) {
      this.resume();
      return;
    }
    this.menu.handle();
  }
}

/** A yes/no question. */
export class Confirm implements Screen {
  root = el('div', 'screen');
  private menu: Menu;
  constructor(question: string, yes: () => void, no: () => void) {
    this.root.style.background = '#000000b0';
    const p = el('div', 'opts panel');
    p.appendChild(el('div', 'detail', `<div class="d">${question}</div>`));
    const m = el('div', 'menu');
    p.appendChild(m);
    this.menu = new Menu(m);
    this.menu.add(t('common.no'), no);
    this.menu.add(t('common.yes'), yes);
    this.root.appendChild(p);
    this.no = no;
  }
  private no: () => void;
  update(): void {
    if (input.pressed('back')) {
      this.no();
      return;
    }
    this.menu.handle();
  }
}
