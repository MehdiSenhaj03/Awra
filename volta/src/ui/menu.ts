import { sfx } from '../engine/sfx';
import { input } from '../engine/input';
import { el } from './dom';

export interface Item {
  el: HTMLElement;
  action?: () => void;
  adjust?: (d: number) => void;
  disabled?: boolean;
}

/** A focusable list driven by keyboard, gamepad and mouse. Vertical by default. */
export class Menu {
  items: Item[] = [];
  index = 0;
  constructor(public root: HTMLElement, public horizontal = false) {}

  add(html: string, action?: () => void, o: { cls?: string; adjust?: (d: number) => void; disabled?: boolean } = {}): HTMLElement {
    const e = el('button', `mi ${o.cls ?? ''}${o.disabled ? ' disabled' : ''}`, html);
    const item: Item = { el: e, action, adjust: o.adjust, disabled: o.disabled };
    const i = this.items.length;
    e.addEventListener('mouseenter', () => this.focus(i));
    e.addEventListener('click', (ev) => {
      ev.stopPropagation();
      this.focus(i);
      this.activate();
    });
    this.items.push(item);
    this.root.appendChild(e);
    if (this.items.length === 1) this.focus(0, true);
    return e;
  }

  focus(i: number, quiet = false): void {
    if (i === this.index && !quiet && this.items[i]?.el.classList.contains('on')) return;
    this.items[this.index]?.el.classList.remove('on');
    this.index = Math.max(0, Math.min(this.items.length - 1, i));
    this.items[this.index]?.el.classList.add('on');
    if (!quiet) sfx('ui', 1 + this.index * 0.04);
  }

  activate(): void {
    const it = this.items[this.index];
    if (!it || it.disabled) return;
    sfx('uiOk');
    it.action?.();
  }

  /** Reads this frame's menu actions. Returns true when something happened. */
  handle(): boolean {
    const prev = this.horizontal ? 'left' : 'up';
    const next = this.horizontal ? 'right' : 'down';
    if (input.pressed(prev)) {
      this.step(-1);
      return true;
    }
    if (input.pressed(next)) {
      this.step(1);
      return true;
    }
    const it = this.items[this.index];
    if (it?.adjust && !this.horizontal) {
      if (input.pressed('left')) {
        it.adjust(-1);
        sfx('ui', 0.9);
        return true;
      }
      if (input.pressed('right')) {
        it.adjust(1);
        sfx('ui', 1.1);
        return true;
      }
    }
    if (input.pressed('confirm') || input.pressed('fire')) {
      this.activate();
      return true;
    }
    return false;
  }

  private step(d: number): void {
    const n = this.items.length;
    let i = this.index;
    for (let k = 0; k < n; k++) {
      i = (i + d + n) % n;
      if (!this.items[i].disabled) break;
    }
    this.focus(i);
  }
}
