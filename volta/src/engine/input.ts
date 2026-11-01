// Keyboard (rebindable, by physical key code), mouse and standard gamepads feed one per-frame state.

export type Action = 'up' | 'down' | 'left' | 'right' | 'fire' | 'dash' | 'pulse' | 'overdrive' | 'pause' | 'confirm' | 'back';
export const BINDABLE: Action[] = ['up', 'down', 'left', 'right', 'fire', 'dash', 'pulse', 'overdrive', 'pause'];

export const DEFAULT_KEYS: Record<Action, string[]> = {
  up: ['KeyW', 'ArrowUp'],
  down: ['KeyS', 'ArrowDown'],
  left: ['KeyA', 'ArrowLeft'],
  right: ['KeyD', 'ArrowRight'],
  fire: ['Mouse0', 'KeyJ'],
  dash: ['Space', 'ShiftLeft'],
  pulse: ['Mouse2', 'KeyK'],
  overdrive: ['KeyE', 'KeyQ'],
  pause: ['Escape', 'KeyP'],
  confirm: ['Enter', 'Space'],
  back: ['Escape', 'Backspace'],
};

const PAD: Partial<Record<Action, number[]>> = {
  fire: [7, 5],
  dash: [0, 4],
  pulse: [6, 2],
  overdrive: [3, 1],
  pause: [9],
  confirm: [0],
  back: [1],
  up: [12],
  down: [13],
  left: [14],
  right: [15],
};

class Input {
  bindings: Record<Action, string[]> = structuredClone(DEFAULT_KEYS);
  private down = new Set<string>();
  private held = new Set<Action>();
  private prev = new Set<Action>();
  private queued = new Set<Action>();
  /** Mouse position in CSS pixels (relative to the canvas). */
  mouseX = 0;
  mouseY = 0;
  mouseMoved = false;
  /** Gamepad sticks. */
  padMove = { x: 0, y: 0 };
  padAim = { x: 0, y: 0 };
  usingPad = false;
  /** Scripted held actions (tests and automation). */
  script: Set<Action> | null = null;
  /** Called on any key press while rebinding. */
  capture: ((code: string) => void) | null = null;

  attach(el: HTMLElement): void {
    window.addEventListener('keydown', (e) => {
      if (this.capture) {
        e.preventDefault();
        const c = this.capture;
        this.capture = null;
        c(e.code);
        return;
      }
      if (['Space', 'Tab', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.code)) e.preventDefault();
      if (!e.repeat) this.press(e.code);
      this.usingPad = false;
    });
    window.addEventListener('keyup', (e) => this.down.delete(e.code));
    el.addEventListener('mousedown', (e) => {
      if (this.capture) {
        const c = this.capture;
        this.capture = null;
        c(`Mouse${e.button}`);
        return;
      }
      this.press(`Mouse${e.button}`);
      this.usingPad = false;
    });
    window.addEventListener('mouseup', (e) => this.down.delete(`Mouse${e.button}`));
    el.addEventListener('contextmenu', (e) => e.preventDefault());
    window.addEventListener('mousemove', (e) => {
      const r = el.getBoundingClientRect();
      this.mouseX = e.clientX - r.left;
      this.mouseY = e.clientY - r.top;
      this.mouseMoved = true;
      this.usingPad = false;
    });
    window.addEventListener('blur', () => this.down.clear());
  }

  private press(code: string): void {
    this.down.add(code);
    for (const a of Object.keys(this.bindings) as Action[]) if (this.bindings[a].includes(code)) this.queued.add(a);
  }

  /** Samples devices once per simulation frame. */
  poll(): void {
    this.prev = this.held;
    this.held = new Set();
    if (this.script) {
      for (const a of this.script) this.held.add(a);
    } else {
      for (const a of Object.keys(this.bindings) as Action[]) if (this.bindings[a].some((c) => this.down.has(c))) this.held.add(a);
    }
    this.padMove = { x: 0, y: 0 };
    this.padAim = { x: 0, y: 0 };
    const pads = navigator.getGamepads?.() ?? [];
    for (const p of pads) {
      if (!p) continue;
      const dz = (v: number) => (Math.abs(v) < 0.18 ? 0 : v);
      const mx = dz(p.axes[0] ?? 0);
      const my = dz(p.axes[1] ?? 0);
      const ax = dz(p.axes[2] ?? 0);
      const ay = dz(p.axes[3] ?? 0);
      if (mx || my || ax || ay || p.buttons.some((b) => b.pressed)) this.usingPad = true;
      this.padMove = { x: mx, y: my };
      this.padAim = { x: ax, y: ay };
      for (const [a, idx] of Object.entries(PAD) as [Action, number[]][]) if (idx.some((i) => p.buttons[i]?.pressed)) this.held.add(a);
      if (Math.hypot(ax, ay) > 0.5) this.held.add('fire');
      break;
    }
    for (const a of this.queued) this.held.add(a);
    this.pressedNow = new Set([...this.queued].concat([...this.held].filter((a) => !this.prev.has(a))));
    this.queued.clear();
  }

  private pressedNow = new Set<Action>();

  isDown(a: Action): boolean {
    return this.held.has(a);
  }

  pressed(a: Action): boolean {
    return this.pressedNow.has(a);
  }

  /** Movement axis from keys and stick, length <= 1. */
  move(): { x: number; y: number } {
    let x = (this.isDown('right') ? 1 : 0) - (this.isDown('left') ? 1 : 0) + this.padMove.x;
    let y = (this.isDown('down') ? 1 : 0) - (this.isDown('up') ? 1 : 0) + this.padMove.y;
    const l = Math.hypot(x, y);
    if (l > 1) {
      x /= l;
      y /= l;
    }
    return { x, y };
  }

  keyName(a: Action): string {
    const c = this.bindings[a][0] ?? '?';
    return codeName(c);
  }
}

/** Real labels of the player's keyboard layout (AZERTY shows Z where QWERTY has W). */
let layout: Map<string, string> | null = null;
export async function loadLayout(): Promise<void> {
  try {
    const kb = (navigator as unknown as { keyboard?: { getLayoutMap(): Promise<Map<string, string>> } }).keyboard;
    if (kb) layout = await kb.getLayoutMap();
  } catch {
    layout = null;
  }
}

export function codeName(c: string): string {
  const l = layout?.get(c);
  if (l && l.trim() && c.startsWith('Key')) return l.toUpperCase();
  if (l && l.trim() && c.startsWith('Digit')) return l;
  if (c === 'Mouse0') return 'Clic G';
  if (c === 'Mouse2') return 'Clic D';
  if (c === 'Mouse1') return 'Clic M';
  if (c.startsWith('Key')) return c.slice(3);
  if (c.startsWith('Digit')) return c.slice(5);
  if (c.startsWith('Arrow')) return { ArrowUp: '↑', ArrowDown: '↓', ArrowLeft: '←', ArrowRight: '→' }[c] ?? c;
  if (c === 'Space') return 'Espace';
  if (c === 'ShiftLeft' || c === 'ShiftRight') return 'Maj';
  if (c === 'Escape') return 'Échap';
  return c;
}

export const input = new Input();
