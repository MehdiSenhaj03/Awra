import { t } from '../i18n';
import { el } from './dom';
import { Menu } from './menu';
import type { Screen } from './screen';

/** The last bar: end the symphony, or play forever. */
export class Choice implements Screen {
  root = el('div', 'screen choice');
  private menu: Menu;
  constructor(onCoda: () => void, onEncore: () => void) {
    this.root.appendChild(el('div', 'h-title', t('story.choice.title')));
    const o = el('div', 'opts2 menu h');
    this.menu = new Menu(o, true);
    this.menu.add(`${t('story.choice.coda')}<small>${t('story.choice.codaDesc')}</small>`, onCoda);
    this.menu.add(`${t('story.choice.encore')}<small>${t('story.choice.encoreDesc')}</small>`, onEncore);
    this.root.appendChild(o);
  }
  update(): void {
    this.menu.handle();
  }
}
