import { input } from '../engine/input';
import { t } from '../i18n';
import { el } from './dom';
import { type Screen, img } from './screen';

/** Full-screen illustrations with captions; each shot lasts ~6 s, any key advances, long press skips. */
export class Cinematic implements Screen {
  root = el('div', 'screen cine');
  private i = -1;
  private t = 0;
  private imgs: HTMLElement[] = [];
  private txt = el('div', 'txt');
  private held = 0;

  constructor(private shots: { image: string; text: string }[], private onDone: () => void) {
    for (const s of shots) {
      const d = el('div', 'img');
      d.style.backgroundImage = `url(${img(s.image)})`;
      this.imgs.push(d);
      this.root.appendChild(d);
    }
    this.root.append(el('div', 'bars'), this.txt, el('div', 'skip', `${t('common.skip')} ▶▶`));
    this.root.addEventListener('click', () => this.next());
    this.next();
  }

  private next(): void {
    this.i++;
    if (this.i >= this.shots.length) {
      this.i = 999;
      this.onDone();
      return;
    }
    this.imgs.forEach((d, k) => d.classList.toggle('show', k === this.i));
    this.txt.classList.remove('show');
    this.txt.textContent = this.shots[this.i].text;
    setTimeout(() => this.txt.classList.add('show'), 600);
    this.t = 0;
  }

  update(dt: number): void {
    if (this.i >= this.shots.length) return;
    this.t += dt;
    const len = 3 + this.shots[this.i].text.length / 22;
    if (this.t > len) this.next();
    if (input.pressed('confirm') || input.pressed('fire') || input.pressed('dash')) this.next();
    this.held = input.isDown('back') || input.isDown('pause') ? this.held + dt : 0;
    if (this.held > 0.6) {
      this.i = 999;
      this.onDone();
    }
  }
}
