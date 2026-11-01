import type { ArenaDef } from './arena';
import { F, wave } from './formations';
import type { StageDef, Wave } from './stage';

/** All stages, by id, in campaign order. */
export const STAGES: Record<string, StageDef> = {};
export const ORDER: string[] = [];

export const SECTORS = [
  { id: 1, theme: 'seuil', bpm: 128, boss: 'ostinato' },
  { id: 2, theme: 'jardin', bpm: 136, boss: 'crescendo' },
  { id: 3, theme: 'fonderie', bpm: 140, boss: 'fugue' },
  { id: 4, theme: 'ocean', bpm: 118, boss: 'rubato' },
  { id: 5, theme: 'ville', bpm: 150, boss: 'staccato' },
  { id: 6, theme: 'coeur', bpm: 160, boss: 'fermata' },
];

function def(sector: number, index: number, arena: ArenaDef, movements: Wave[][], par: [number, number, number, number], fragment?: { m: number; w: number }): void {
  const s = SECTORS[sector - 1];
  const id = `s${sector}_${index + 1}`;
  STAGES[id] = { id, sector, index, bpm: s.bpm, theme: s.theme, arena, movements, par, fragment: fragment ? { ...fragment, id: `f${sector}_${index + 1}` } : undefined };
  ORDER.push(id);
}

function boss(sector: number, arena: ArenaDef, par: [number, number, number, number]): void {
  const s = SECTORS[sector - 1];
  const id = `s${sector}_4`;
  STAGES[id] = { id, sector, index: 3, bpm: s.bpm, theme: s.theme, arena, movements: [], boss: s.boss, par };
  ORDER.push(id);
}

const circle = (r: number, pillars: [number, number, number][] = []): ArenaDef => ({ shape: 'circle', r, pillars });
const rect = (w: number, h: number, pillars: [number, number, number][] = []): ArenaDef => ({ shape: 'rect', w, h, pillars });

// ---- 1. LE SEUIL — learn to move, fire on the beat, dash, parry ----------------------------------------------

def(1, 0, circle(19), [
  [wave(F.ring('mote', 6, 12)), wave(F.edge('mote', 10, 15, 0, 0.5, 2), F.at('spitter', 0, -12, 2))],
  [wave(F.at('spitter', -10, -6), F.at('spitter', 10, -6), F.edge('mote', 8, 15, 2, 0.25, 3)), wave(F.at('spinner', 0, -8), F.ring('mote', 8, 14, 4))],
  [wave(F.line('lancer', 3, -10, -12, 10, -12, 0, 1), F.edge('mote', 12, 16, 3, 0.25, 5)), wave(F.at('splitter', -8, 0), F.at('splitter', 8, 0), F.at('spitter', 0, -13, 2, true))],
], [9000, 18000, 30000, 45000], { m: 1, w: 1 });

def(1, 1, rect(21, 13, [[-9, 0, 1.6], [9, 0, 1.6]]), [
  [wave(F.line('spitter', 4, -15, -9, 15, -9), F.edge('mote', 8, 18, 2, 0.25, 7)), wave(F.at('spinner', -14, -6), F.at('spinner', 14, 6), F.pack('mote', 8, 0, -8, 3, 2, 3))],
  [wave(F.line('lancer', 4, -16, -10, 16, -10, 0, 0.5), F.line('lancer', 2, -16, 10, 16, 10, 4, 0.5)), wave(F.at('splitter', 0, -8), F.at('splitter', -12, 6), F.at('splitter', 12, 6), F.edge('mote', 10, 18, 4, 0.2, 9))],
  [wave(F.at('spinner', 0, -8, 0, true), F.line('spitter', 3, -14, 9, 14, 9, 2)), wave(F.edge('lancer', 5, 18, 0, 0.5, 4), F.edge('mote', 16, 18, 2, 0.125, 11), F.at('spitter', 0, 9, 4, true))],
], [14000, 28000, 46000, 68000], { m: 2, w: 0 });

def(1, 2, circle(20, [[-7, -6, 1.4], [7, -6, 1.4], [0, 8, 1.4]]), [
  [wave(F.ring('splitter', 3, 12), F.edge('mote', 8, 16, 3, 0.25, 5)), wave(F.at('bulwark', 0, -12), F.ring('mote', 10, 14, 2))],
  [wave(F.at('bulwark', -10, -8), F.at('bulwark', 10, -8), F.ring('spitter', 4, 14, 2, 0.4)), wave(F.ring('spinner', 2, 12, 0, 0.8), F.edge('lancer', 4, 16, 2, 0.5, 6))],
  [wave(F.ring('mote', 16, 16, 0, 0.2), F.ring('spitter', 4, 13, 2, 0.8, false), F.at('splitter', 0, -10, 4, true)), wave(F.at('bulwark', 0, -12, 0, true), F.ring('lancer', 4, 15, 2), F.ring('spinner', 2, 11, 6, 1.6))],
], [18000, 36000, 58000, 85000], { m: 1, w: 0 });

boss(1, circle(20), [30000, 60000, 95000, 140000]);

// ---- 2. LE JARDIN DE VERRE — mirrors (shoot on the beat) and phantoms (on the downbeat) ----------------------------

def(2, 0, circle(20, [[-10, 0, 1.5], [10, 0, 1.5]]), [
  [wave(F.at('mirror', 0, -10), F.edge('mote', 10, 16, 2, 0.25, 21)), wave(F.ring('mirror', 3, 12, 0, 0.5), F.at('spitter', 0, 12, 2))],
  [wave(F.ring('phantom', 3, 12), F.edge('mote', 8, 16, 2, 0.25, 22)), wave(F.at('spinner', 0, -10), F.ring('phantom', 2, 10, 2, 1), F.ring('mirror', 2, 13, 4, 0))],
  [wave(F.ring('splitter', 4, 13, 0, 0.4), F.at('mirror', 0, -12, 2, true)), wave(F.ring('phantom', 4, 13, 0, 0.8), F.ring('lancer', 4, 16, 2, 0), F.edge('mote', 12, 17, 4, 0.125, 23))],
], [22000, 44000, 70000, 100000], { m: 1, w: 1 });

def(2, 1, rect(22, 14, [[-8, -5, 1.4], [8, -5, 1.4], [-8, 5, 1.4], [8, 5, 1.4]]), [
  [wave(F.line('mirror', 3, -14, -10, 14, -10), F.line('spitter', 2, -16, 10, 16, 10, 2)), wave(F.at('spinner', -16, 0), F.at('spinner', 16, 0), F.pack('phantom', 3, 0, -10, 3, 2, 4))],
  [wave(F.edge('lancer', 6, 18, 0, 0.5, 24), F.at('mirror', 0, 0, 2, true)), wave(F.at('bulwark', 0, -10), F.ring('phantom', 4, 12, 2, 0.4), F.edge('mote', 12, 18, 4, 0.125, 25))],
  [wave(F.line('splitter', 3, -14, -9, 14, -9), F.line('mirror', 3, -14, 9, 14, 9, 2)), wave(F.at('spinner', 0, -10, 0, true), F.ring('phantom', 4, 13, 2, 0), F.edge('lancer', 4, 18, 4, 0.5, 26))],
], [26000, 52000, 82000, 118000], { m: 2, w: 1 });

def(2, 2, circle(21, [[0, 0, 2.2]]), [
  [wave(F.ring('mirror', 4, 13, 0, 0.4), F.ring('mote', 12, 17, 2)), wave(F.ring('phantom', 6, 14, 0, 0.2), F.at('spinner', 0, -15, 2))],
  [wave(F.ring('bulwark', 2, 13, 0, 0.8), F.ring('mirror', 2, 13, 2, 2.4), F.edge('mote', 10, 17, 4, 0.2, 27)), wave(F.ring('splitter', 4, 14, 0, 0.4, true), F.ring('lancer', 4, 17, 2, 1.2))],
  [wave(F.ring('spinner', 3, 13, 0, 0.5), F.ring('phantom', 4, 16, 2, 0)), wave(F.ring('mirror', 3, 14, 0, 0.2, true), F.ring('phantom', 3, 14, 2, 1.2, true), F.edge('mote', 16, 18, 4, 0.125, 28))],
], [30000, 60000, 92000, 132000], { m: 1, w: 0 });

boss(2, circle(21), [36000, 72000, 110000, 160000]);

// ---- 3. LA FONDERIE D'ÉCHOS — mortars, shields, sweeping beams -------------------------------------------------

def(3, 0, rect(22, 14, [[0, 0, 2]]), [
  [wave(F.line('mortar', 2, -15, -9, 15, -9), F.edge('mote', 10, 18, 2, 0.25, 31)), wave(F.at('bulwark', -12, 0), F.at('bulwark', 12, 0), F.line('spitter', 3, -12, -10, 12, -10, 2))],
  [wave(F.at('weaver', 0, -9), F.edge('mote', 12, 18, 2, 0.2, 32)), wave(F.line('mortar', 3, -14, 9, 14, 9), F.line('lancer', 4, -15, -10, 15, -10, 2, 0.5))],
  [wave(F.at('weaver', -12, -6), F.at('weaver', 12, 6), F.pack('mote', 10, 0, -9, 4, 2, 5)), wave(F.at('bulwark', 0, -10, 0, true), F.line('mortar', 2, -16, 0, 16, 0, 2), F.edge('spitter', 4, 18, 4, 0.5, 33))],
], [34000, 68000, 104000, 150000], { m: 1, w: 1 });

def(3, 1, circle(21, [[-8, -8, 1.5], [8, -8, 1.5], [-8, 8, 1.5], [8, 8, 1.5]]), [
  [wave(F.ring('mortar', 3, 15, 0, 0.5), F.ring('splitter', 2, 10, 2, 0)), wave(F.ring('bulwark', 3, 14, 0, 1.0), F.edge('mote', 12, 18, 2, 0.15, 34))],
  [wave(F.at('weaver', 0, 0), F.ring('lancer', 4, 17, 2, 0.8)), wave(F.ring('mirror', 2, 14, 0, 0), F.ring('mortar', 2, 15, 0, 1.6), F.ring('phantom', 3, 14, 2, 0.5))],
  [wave(F.ring('weaver', 2, 13, 0, 0.8), F.ring('spitter', 4, 16, 2, 0)), wave(F.at('bulwark', 0, -14, 0, true), F.at('mortar', 0, 14, 0, true), F.edge('lancer', 6, 18, 2, 0.5, 35), F.edge('mote', 16, 18, 4, 0.125, 36))],
], [38000, 76000, 116000, 165000], { m: 2, w: 0 });

def(3, 2, rect(24, 13, [[-12, 0, 1.6], [0, 0, 1.6], [12, 0, 1.6]]), [
  [wave(F.line('weaver', 2, -16, -8, 16, -8), F.line('mortar', 2, -16, 8, 16, 8, 2)), wave(F.line('bulwark', 3, -16, -9, 16, -9), F.edge('mote', 14, 20, 2, 0.15, 37))],
  [wave(F.line('spinner', 3, -16, 8, 16, 8, 0, 0), F.edge('lancer', 6, 20, 2, 0.5, 38)), wave(F.at('weaver', 0, -9, 0, true), F.line('mirror', 2, -18, 8, 18, 8, 2), F.line('phantom', 3, -14, -9, 14, -9, 4))],
  [wave(F.line('mortar', 4, -18, -10, 18, -10), F.line('splitter', 3, -14, 9, 14, 9, 2)), wave(F.at('bulwark', -16, 0, 0, true), F.at('bulwark', 16, 0, 0, true), F.at('weaver', 0, -10, 2, true), F.edge('mote', 16, 20, 4, 0.125, 39))],
], [42000, 84000, 128000, 180000], { m: 1, w: 1 });

boss(3, circle(21), [42000, 84000, 130000, 190000]);

// ---- 4. L'OCÉAN DE MERCURE — snipers and choirs: read the battlefield -------------------------------------------

def(4, 0, circle(21, [[0, -6, 1.6]]), [
  [wave(F.ring('sniper', 3, 17, 0, 0.5), F.edge('mote', 10, 18, 2, 0.25, 41)), wave(F.at('choir', 0, -14), F.ring('spitter', 4, 12, 2, 0.4))],
  [wave(F.ring('sniper', 2, 17, 0, 0), F.ring('phantom', 4, 12, 2, 0.8)), wave(F.at('choir', -12, -10), F.at('choir', 12, -10), F.ring('bulwark', 2, 10, 2, 0), F.edge('mote', 12, 18, 4, 0.15, 42))],
  [wave(F.ring('spinner', 2, 12, 0, 0), F.ring('sniper', 3, 18, 2, 1)), wave(F.at('choir', 0, -15, 0, true), F.ring('splitter', 3, 12, 2, 0.5), F.ring('lancer', 4, 17, 4, 0))],
], [46000, 92000, 140000, 200000], { m: 1, w: 1 });

def(4, 1, rect(23, 14, [[-6, 0, 1.4], [6, 0, 1.4]]), [
  [wave(F.line('sniper', 3, -18, -11, 18, -11), F.line('mote', 6, -18, 11, 18, 11, 2)), wave(F.at('choir', 0, -11), F.line('mirror', 2, -16, 0, 16, 0, 2), F.line('spitter', 3, -14, 10, 14, 10, 2))],
  [wave(F.at('weaver', 0, 0), F.line('sniper', 2, -18, 11, 18, 11, 2)), wave(F.line('choir', 2, -16, -10, 16, -10), F.line('bulwark', 2, -10, 0, 10, 0, 2), F.edge('lancer', 6, 20, 4, 0.5, 43))],
  [wave(F.line('phantom', 4, -16, -9, 16, -9), F.line('mortar', 2, -18, 10, 18, 10, 2)), wave(F.at('choir', 0, -11, 0, true), F.line('sniper', 4, -18, 11, 18, 11, 2), F.at('spinner', 0, 0, 4, true), F.edge('mote', 16, 20, 4, 0.125, 44))],
], [50000, 100000, 152000, 215000], { m: 2, w: 1 });

def(4, 2, circle(22, [[-9, 0, 1.4], [9, 0, 1.4], [0, -9, 1.4], [0, 9, 1.4]]), [
  [wave(F.ring('choir', 2, 16, 0, 0), F.ring('spinner', 2, 12, 2, 1.57), F.edge('mote', 12, 19, 4, 0.2, 45)), wave(F.ring('sniper', 4, 18, 0, 0.8), F.ring('phantom', 4, 13, 2, 0))],
  [wave(F.ring('weaver', 2, 14, 0, 0.8), F.ring('mirror', 2, 14, 2, 2.4)), wave(F.ring('bulwark', 3, 14, 0, 0.5), F.at('choir', 0, -17, 2, true), F.ring('lancer', 6, 18, 4, 0))],
  [wave(F.ring('mortar', 3, 16, 0, 0), F.ring('sniper', 3, 18, 2, 1)), wave(F.ring('choir', 2, 17, 0, 1.57, true), F.ring('phantom', 4, 13, 2, 0.8, true), F.ring('splitter', 2, 10, 4, 0, true), F.edge('mote', 18, 20, 4, 0.1, 46))],
], [54000, 108000, 164000, 230000], { m: 1, w: 0 });

boss(4, circle(22), [50000, 100000, 155000, 220000]);

// ---- 5. LA VILLE MUETTE — everything, faster, in tight streets ----------------------------------------------

def(5, 0, rect(22, 15, [[-11, -5, 1.4], [11, -5, 1.4], [-11, 5, 1.4], [11, 5, 1.4], [0, 0, 1.8]]), [
  [wave(F.line('spitter', 5, -18, -12, 18, -12), F.edge('mote', 14, 20, 2, 0.15, 51)), wave(F.line('lancer', 5, -18, 12, 18, 12, 0, 0.25), F.line('sniper', 2, -20, 0, 20, 0, 2))],
  [wave(F.line('mirror', 3, -16, -11, 16, -11), F.line('phantom', 3, -16, 11, 16, 11, 2), F.at('choir', 0, -13, 4)), wave(F.line('weaver', 2, -16, 0, 16, 0), F.edge('mote', 16, 20, 2, 0.125, 52))],
  [wave(F.line('bulwark', 3, -14, -11, 14, -11), F.line('mortar', 2, -18, 11, 18, 11, 2)), wave(F.at('spinner', -16, -10, 0, true), F.at('spinner', 16, 10, 0, true), F.edge('lancer', 6, 20, 2, 0.5, 53), F.at('choir', 0, 12, 4, true))],
], [58000, 116000, 176000, 250000], { m: 1, w: 1 });

def(5, 1, circle(22, [[-6, -6, 1.3], [6, -6, 1.3], [-6, 6, 1.3], [6, 6, 1.3]]), [
  [wave(F.ring('phantom', 6, 15, 0, 0), F.ring('sniper', 2, 19, 2, 1.57)), wave(F.ring('mirror', 4, 14, 0, 0.8), F.ring('spitter', 4, 17, 2, 0), F.edge('mote', 16, 20, 4, 0.125, 54))],
  [wave(F.ring('weaver', 3, 15, 0, 0), F.at('choir', 0, 0, 2)), wave(F.ring('bulwark', 4, 15, 0, 0.8), F.ring('mortar', 2, 18, 2, 0), F.ring('lancer', 6, 19, 4, 0.5))],
  [wave(F.ring('splitter', 4, 15, 0, 0.8, true), F.ring('phantom', 4, 12, 2, 0)), wave(F.ring('spinner', 3, 14, 0, 0, true), F.ring('choir', 2, 18, 2, 1.57), F.ring('sniper', 4, 19, 4, 0.8), F.edge('mote', 20, 20, 4, 0.1, 55))],
], [62000, 124000, 188000, 265000], { m: 2, w: 0 });

def(5, 2, rect(25, 13, [[-16, 0, 1.4], [-8, -5, 1.4], [0, 0, 1.4], [8, 5, 1.4], [16, 0, 1.4]]), [
  [wave(F.line('lancer', 8, -22, -10, 22, -10, 0, 0.25), F.line('lancer', 4, -22, 10, 22, 10, 4, 0.25)), wave(F.line('sniper', 3, -22, 0, 22, 0), F.line('phantom', 4, -18, -9, 18, 9, 2))],
  [wave(F.line('weaver', 2, -14, -8, 14, 8), F.line('mirror', 3, -20, 9, 20, 9, 2), F.edge('mote', 16, 22, 4, 0.125, 56)), wave(F.line('choir', 2, -20, -10, 20, -10), F.line('bulwark', 3, -16, 9, 16, 9), F.line('mortar', 2, -22, 0, 22, 0, 2))],
  [wave(F.line('spinner', 4, -18, -8, 18, 8, 0, 0, ), F.line('splitter', 3, -16, 9, 16, -9, 2)), wave(F.at('weaver', 0, 0, 0, true), F.at('choir', -20, 0, 0, true), F.at('choir', 20, 0, 0, true), F.line('sniper', 4, -22, -11, 22, 11, 2), F.edge('lancer', 8, 22, 4, 0.25, 57))],
], [66000, 132000, 200000, 280000], { m: 1, w: 1 });

boss(5, rect(22, 14), [60000, 120000, 185000, 260000]);

// ---- 6. LE CŒUR DU SILENCE — the last concert ------------------------------------------------------------------

def(6, 0, circle(22, [[0, 0, 2.4]]), [
  [wave(F.ring('mirror', 4, 15, 0, 0.4), F.ring('phantom', 4, 18, 2, 0)), wave(F.ring('spinner', 3, 14, 0, 0, true), F.edge('mote', 20, 20, 2, 0.1, 61))],
  [wave(F.ring('sniper', 4, 19, 0, 0.8), F.ring('choir', 2, 17, 2, 0), F.ring('bulwark', 2, 13, 2, 1.57)), wave(F.ring('weaver', 3, 15, 0, 0.5), F.ring('lancer', 8, 19, 2, 0))],
  [wave(F.ring('mortar', 3, 17, 0, 0, true), F.ring('splitter', 3, 13, 2, 1, true)), wave(F.ring('choir', 3, 18, 0, 0.5, true), F.ring('phantom', 6, 14, 2, 0, true), F.ring('mirror', 3, 12, 4, 1, true), F.edge('mote', 24, 20, 4, 0.08, 62))],
], [72000, 144000, 220000, 310000], { m: 1, w: 1 });

def(6, 1, rect(24, 15, [[-12, -6, 1.5], [12, -6, 1.5], [-12, 6, 1.5], [12, 6, 1.5]]), [
  [wave(F.line('weaver', 3, -16, 0, 16, 0), F.edge('lancer', 8, 22, 2, 0.25, 63)), wave(F.line('bulwark', 4, -18, -12, 18, -12, 0, 0), F.line('sniper', 3, -20, 12, 20, 12, 2), F.at('choir', 0, 0, 4, true))],
  [wave(F.line('spinner', 4, -18, -10, 18, -10, 0, 0), F.line('mirror', 4, -18, 10, 18, 10, 2)), wave(F.line('phantom', 6, -20, -11, 20, 11, 0), F.line('mortar', 3, -20, 0, 20, 0, 2), F.edge('mote', 24, 22, 4, 0.08, 64))],
  [wave(F.at('weaver', -16, 0, 0, true), F.at('weaver', 16, 0, 0, true), F.line('choir', 2, -8, -12, 8, -12, 2, 0)), wave(F.line('bulwark', 3, -16, -11, 16, -11, 0, 0), F.line('sniper', 4, -20, 12, 20, 12, 2), F.line('spinner', 2, -8, 0, 8, 0, 4, 0), F.edge('lancer', 10, 22, 4, 0.25, 65))],
], [78000, 156000, 236000, 330000], { m: 2, w: 1 });

def(6, 2, circle(23, [[-8, 0, 1.5], [8, 0, 1.5], [0, -8, 1.5], [0, 8, 1.5]]), [
  [wave(F.ring('choir', 3, 18, 0, 0.5), F.ring('mirror', 3, 13, 2, 1.5), F.ring('spinner', 2, 10, 4, 0)), wave(F.ring('sniper', 6, 20, 0, 0), F.ring('phantom', 6, 15, 2, 0.5))],
  [wave(F.ring('weaver', 3, 16, 0, 0.5, true), F.edge('mote', 24, 21, 2, 0.08, 66)), wave(F.ring('bulwark', 4, 16, 0, 0.8, true), F.ring('mortar', 3, 19, 2, 0), F.ring('lancer', 8, 20, 4, 0.4))],
  [wave(F.ring('splitter', 4, 15, 0, 0, true), F.ring('choir', 2, 19, 2, 1.57, true), F.ring('sniper', 4, 20, 4, 0.8, true)), wave(F.ring('spinner', 4, 14, 0, 0, true), F.ring('mirror', 4, 18, 2, 0.8, true), F.ring('phantom', 4, 11, 4, 0.4, true), F.edge('mote', 28, 21, 4, 0.06, 67))],
], [84000, 168000, 252000, 350000], { m: 1, w: 1 });

boss(6, circle(23), [80000, 160000, 245000, 340000]);
