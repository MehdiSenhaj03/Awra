import { input } from '../engine/input';
import { t } from '../i18n';
import { el } from './dom';
import { Menu } from './menu';
import { type Screen, img } from './screen';

/** Title screen: the painted key art, the animated logo, then the menu. */
export class Title implements Screen {
  root = el('div', 'screen');
  private menu: Menu | null = null;
  private press = el('div', 'press', t('title.press'));
  private wrap = el('div', 'menu title-menu');

  constructor(private items: { label: string; sub?: string; action: () => void; disabled?: boolean }[], private onWake: () => void, awake: boolean) {
    const bg = el('div', 'bg');
    bg.style.backgroundImage = `url(${img('bg_title')}), url(${img('key_art')})`;
    const logo = el('div', 'logo');
    logo.innerHTML = `<h1 data-t="VOLTA">VOLTA</h1><div class="tag">${t('title.tagline').toUpperCase()}</div><div class="line"></div>`;
    this.root.append(bg, el('div', 'shade'), el('div', 'scan'), logo, this.press, this.wrap, el('div', 'corner', 'v1.0 · 2026'));
    this.root.addEventListener('click', () => this.wake());
    if (awake) this.wake();
  }

  private wake(): void {
    if (this.menu) return;
    this.onWake();
    this.press.remove();
    this.menu = new Menu(this.wrap);
    for (const it of this.items) this.menu.add(`${it.label}${it.sub ? `<small>${it.sub}</small>` : ''}`, it.action, { disabled: it.disabled });
  }

  update(): void {
    if (!this.menu) {
      if (input.pressed('confirm') || input.pressed('fire') || input.pressed('dash') || input.pressed('pause')) this.wake();
      return;
    }
    this.menu.handle();
  }
}
