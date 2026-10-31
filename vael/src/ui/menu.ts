import { Container, Graphics, type Text } from 'pixi.js';
import { audio } from '../engine/audio';
import { input } from '../engine/input';
import { GOLD, PALE, star, style, text } from './draw';

export interface MenuItem {
  label: () => string;
  action?: () => void;
  /** Left/right adjusts (options). */
  adjust?: (dir: number) => void;
  disabled?: () => boolean;
  /** Small text under the menu when selected. */
  hint?: () => string;
}

/** A vertical list of choices with a star cursor, driven by up/down/confirm. */
export class Menu {
  readonly c = new Container();
  index = 0;
  private texts: Text[] = [];
  private cursor = new Graphics();
  private hintText: Text;
  private t = 0;

  constructor(public items: MenuItem[], public x: number, public y: number, public gap = 64, public align: 'left' | 'center' = 'center', size = 34) {
    this.c.addChild(this.cursor);
    for (const [i, it] of items.entries()) {
      const tx = text(it.label(), style('menu', PALE, size, align), x, y + i * gap, align === 'center' ? 0.5 : 0, 0.5);
      this.texts.push(tx);
      this.c.addChild(tx);
    }
    this.hintText = text('', style('italic', 0xa8aed0, 26, 'center', 1100), 960, y + items.length * gap + 20, 0.5, 0);
    this.c.addChild(this.hintText);
  }

  refresh(): void {
    for (const [i, it] of this.items.entries()) this.texts[i].text = it.label();
  }

  /** Returns true when an item was activated. */
  update(): boolean {
    this.t++;
    const n = this.items.length;
    if (input.pressed('down')) {
      this.index = (this.index + 1) % n;
      audio.play('uiMove');
    }
    if (input.pressed('up')) {
      this.index = (this.index + n - 1) % n;
      audio.play('uiMove');
    }
    const it = this.items[this.index];
    if (it.adjust && (input.pressed('left') || input.pressed('right'))) {
      it.adjust(input.pressed('left') ? -1 : 1);
      audio.play('uiMove');
      this.refresh();
    }
    if ((input.pressed('confirm') || input.pressed('jump') || input.pressed('attack')) && !(it.disabled?.() ?? false)) {
      audio.play('ui');
      it.action?.();
      // The action may have replaced this menu: draw only if still alive.
      if (!this.cursor.destroyed) {
        this.refresh();
        this.draw();
      }
      return true;
    }
    this.draw();
    return false;
  }

  draw(): void {
    const g = this.cursor;
    g.clear();
    for (const [i, tx] of this.texts.entries()) {
      const sel = i === this.index;
      const dis = this.items[i].disabled?.() ?? false;
      tx.alpha = dis ? 0.3 : sel ? 1 : 0.55;
      tx.tint = sel ? 0xfff4dc : 0xffffff;
      if (sel) {
        const w = tx.width;
        const x0 = this.align === 'center' ? tx.x - w / 2 : tx.x;
        const pulse = 1 + Math.sin(this.t * 0.12) * 0.15;
        star(g, x0 - 34, tx.y, 12 * pulse, GOLD, 0.95);
        if (this.align === 'center') star(g, x0 + w + 34, tx.y, 12 * pulse, GOLD, 0.95);
      }
    }
    this.hintText.text = this.items[this.index].hint?.() ?? '';
  }
}
