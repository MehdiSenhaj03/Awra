import { audio } from './audio';
import { V } from './synth';

/** Sound effects (synthesised). A minimum gap per sound keeps dense fights from turning into noise. */
const last: Record<string, number> = {};

export function sfx(name: string, pitch = 1, vol = 1): void {
  const ctx = audio.ctx;
  if (!ctx) return;
  const t = audio.now;
  const gap = name === 'enemyShot' || name === 'block' ? 0.05 : 0.025;
  if (last[name] && t - last[name] < gap) return;
  last[name] = t;
  const o = { dest: audio.sfx };
  const p = pitch;
  switch (name) {
    case 'dash':
      V.sweep(t, 0.22, 400 * p, 3500 * p, o, 0.5 * vol, 1.5);
      break;
    case 'pulse':
      V.sweep(t, 0.3, 4000 * p, 600, o, 0.45 * vol, 2);
      V.sub(t, 220 * p, 90, 0.18, o, 0.4 * vol);
      break;
    case 'hurt':
      V.sub(t, 160, 30, 0.5, o, 0.9 * vol);
      V.sweep(t, 0.5, 2500, 200, o, 0.7 * vol, 1);
      break;
    case 'die':
      V.sub(t, 120, 20, 1.6, o, 1 * vol);
      V.sweep(t, 1.6, 6000, 80, { dest: audio.sfx, rev: 0.6 }, 0.8 * vol, 0.8);
      break;
    case 'boom':
      V.sub(t, 110 * p, 35, 0.35, o, 0.7 * vol);
      V.sweep(t, 0.3, 3000, 300, o, 0.35 * vol, 0.7);
      break;
    case 'bigBoom':
      V.sub(t, 90, 25, 0.9, o, 1 * vol);
      V.sweep(t, 0.9, 5000, 120, { dest: audio.sfx, rev: 0.5 }, 0.7 * vol, 0.6);
      break;
    case 'rankUp':
      V.sweep(t, 0.4, 800 * p, 6000 * p, { dest: audio.sfx, rev: 0.4 }, 0.35 * vol, 4);
      break;
    case 'overdrive':
      V.sub(t, 60, 30, 1.2, o, 0.9 * vol);
      V.sweep(t, 1.2, 200, 9000, { dest: audio.sfx, rev: 0.6 }, 0.6 * vol, 2);
      break;
    case 'enemyShot':
      V.pluck(t, 64 + Math.random() * 3, o, 0.25 * vol * p, 0.05, 'sawtooth', 2200);
      break;
    case 'telegraph':
      V.pluck(t, 88 * p, { dest: audio.sfx, dly: 0.2 }, 0.2 * vol, 0.08, 'sine', 9000);
      break;
    case 'lunge':
      V.sweep(t, 0.25, 600, 4000, o, 0.4 * vol, 2);
      break;
    case 'mortarLaunch':
      V.sub(t, 140, 70, 0.2, o, 0.4 * vol);
      break;
    case 'mortar':
      V.sub(t, 90, 30, 0.4, o, 0.6 * vol);
      V.sweep(t, 0.35, 2500, 200, o, 0.3 * vol, 0.8);
      break;
    case 'sniper':
      V.sweep(t, 0.2, 9000, 1500, o, 0.5 * vol, 3);
      V.sub(t, 300, 60, 0.15, o, 0.4 * vol);
      break;
    case 'phase':
      V.bell(t, 76, { dest: audio.sfx, rev: 0.3 }, 0.25 * vol, 0.4);
      break;
    case 'block':
      V.pluck(t, 96, o, 0.15 * vol * p, 0.03, 'square', 9000);
      break;
    case 'charge':
      V.pluck(t, 60 + 7 * Math.round((p - 0.8) / 0.15), { dest: audio.sfx, dly: 0.2 }, 0.35 * vol, 0.15, 'sawtooth', 5000);
      break;
    case 'heal':
      V.bell(t, 84, { dest: audio.sfx, rev: 0.5 }, 0.5 * vol, 1.2);
      V.bell(t, 91, { dest: audio.sfx, rev: 0.5 }, 0.35 * vol, 1.2);
      break;
    case 'fragment':
      for (let i = 0; i < 4; i++) V.bell(t + i * 0.09, 79 + [0, 4, 7, 12][i], { dest: audio.sfx, rev: 0.6 }, 0.4 * vol, 1.5);
      break;
    case 'spawn':
      V.sweep(t, 0.4, 300, 2500, o, 0.12 * vol, 6);
      break;
    case 'roar':
      V.sub(t, 70, 28, 1.4, o, 0.9 * vol);
      V.sweep(t, 1.4, 300, 3000, { dest: audio.sfx, rev: 0.6 }, 0.5 * vol, 0.5);
      break;
    case 'ui':
      V.pluck(t, 79 * 1 + (p - 1) * 12, o, 0.25 * vol, 0.06, 'triangle', 6000);
      break;
    case 'uiOk':
      V.bell(t, 84, { dest: audio.sfx, rev: 0.3 }, 0.35 * vol, 0.6);
      break;
    case 'checkpoint':
      V.bell(t, 86, { dest: audio.sfx, rev: 0.5 }, 0.4 * vol, 1.4);
      V.bell(t + 0.12, 93, { dest: audio.sfx, rev: 0.5 }, 0.35 * vol, 1.4);
      break;
    default:
      break;
  }
}
