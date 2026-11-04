import { Enemy } from '../enemy';
import type { World } from '../world';

/**
 * A Conductor: an intro (invulnerable, two bars), three phases by health with a breath between them, attacks
 * scheduled on bars, and a long death. Subclasses implement `attack(w, phase, bar, step)`, called on every
 * sixteenth with the bar index of the current phase.
 */
export abstract class Boss extends Enemy {
  phase = 1;
  /** Beat at which the current phase started. */
  phaseBeat = 0;
  introBars = 2;
  dying = 0;
  /** Bars of rest between phases. */
  private breath = 0;
  abstract name: string;
  /** Health thresholds for phases 2 and 3. */
  thresholds: [number, number] = [0.66, 0.33];

  constructor(x: number, y: number, hp: number, r: number) {
    super(x, y);
    this.boss = true;
    this.heavy = true;
    this.hp = this.maxHp = hp;
    this.r = r;
    this.value = 20000;
    this.notes = 40;
    this.spawnT = 0;
    this.state = 'intro';
  }

  get introing(): boolean {
    return this.state === 'intro';
  }

  /** Position in the current phase, in bars (fractional). */
  bars(w: World): number {
    return (w.beat.beat - this.phaseBeat) / 4;
  }

  tick(w: World): void {
    if (this.dying > 0) {
      this.px = this.x;
      this.py = this.y;
      this.dying++;
      if (this.dying % 8 === 0 && this.dying < 150) {
        const a = w.rng.range(0, Math.PI * 2);
        const d = w.rng.range(0, this.r * 1.2);
        w.emit({ t: 'boom', x: this.x + Math.cos(a) * d, y: this.y + Math.sin(a) * d, r: 2 + w.rng.range(0, 2), color: this.color });
        w.emit({ t: 'shake', a: 0.25 });
        w.sfx('boom', 0.7 + w.rng.next() * 0.4);
      }
      if (this.dying === 160) {
        this.dead = true;
        w.killed(this);
        w.emit({ t: 'bossDown', x: this.x, y: this.y });
        w.emit({ t: 'kill', x: this.x, y: this.y, size: this.r * 2.5, color: this.color, look: this.look, heavy: true });
        w.emit({ t: 'boom', x: this.x, y: this.y, r: 10, color: 0xffffff });
        w.emit({ t: 'shake', a: 1 });
        w.emit({ t: 'flash', color: 0xffffff, a: 1 });
        w.sfx('bigBoom');
        w.hitStop(16);
      }
      return;
    }
    super.tick(w);
  }

  update(w: World, k: number): void {
    if (this.state === 'intro') {
      this.hurtable = false;
      this.move(w, k);
      if (this.st === 1) {
        w.emit({ t: 'boss', phase: 0, name: this.name });
        w.sfx('roar');
      }
      if (w.beat.crossed(1) && w.beat.index(1) % 4 === 0 && this.st > w.beat.spb * 60 * 4 * this.introBars - 30) {
        this.state = 'fight';
        this.hurtable = true;
        this.phaseBeat = Math.round(w.beat.beat);
        w.emit({ t: 'boss', phase: 1, name: this.name });
      }
      return;
    }
    if (this.breath > 0) {
      this.move(w, k);
      if (w.beat.crossed(1) && w.beat.index(1) % 4 === 0 && --this.breath === 0) {
        this.hurtable = true;
        this.phaseBeat = Math.round(w.beat.beat);
      }
      return;
    }
    const f = this.hp / this.maxHp;
    const next = f < this.thresholds[1] ? 3 : f < this.thresholds[0] ? 2 : 1;
    if (next > this.phase) {
      this.phase = next;
      this.hurtable = false;
      this.breath = 1;
      w.clearBullets(this.x, this.y, 80, true);
      for (const h of w.hazards) h.done = true;
      w.emit({ t: 'boss', phase: next, name: this.name });
      w.emit({ t: 'boom', x: this.x, y: this.y, r: this.r * 3, color: this.color });
      w.emit({ t: 'shake', a: 0.8 });
      w.emit({ t: 'chroma', a: 1 });
      w.sfx('roar');
      w.hitStop(10);
      this.onPhase(w, next);
      return;
    }
    this.move(w, k);
    if (w.beat.crossed(4)) {
      const b = this.bars(w);
      this.attack(w, this.phase, Math.floor(b), Math.floor((b * 16) % 16 + 1e-6));
    }
  }

  /** Movement every frame (intro and fight). */
  abstract move(w: World, k: number): void;
  abstract attack(w: World, phase: number, bar: number, step: number): void;
  onPhase(_w: World, _phase: number): void {}

  /** Bosses never die at once: a long collapse first. */
  die(w: World): void {
    if (this.dying > 0 || this.dead) return;
    this.dying = 1;
    this.hurtable = false;
    this.contact = false;
    w.clearBullets(this.x, this.y, 100, true);
    for (const h of w.hazards) h.done = true;
    w.emit({ t: 'flash', color: 0xffffff, a: 0.9 });
    w.emit({ t: 'shake', a: 1 });
    w.sfx('roar');
    w.hitStop(14);
    this.onDeath(w);
  }

  get active(): boolean {
    return !this.dead && this.dying === 0;
  }
}
