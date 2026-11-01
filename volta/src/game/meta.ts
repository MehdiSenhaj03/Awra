import { STAGES } from './stages';
import { BASE_MODS, type Mods } from './world';

/** Overtones: passive perks bought with Notes and equipped in slots before a stage. */
export interface Overtone {
  id: string;
  cost: number;
  icon: string;
  apply(m: Mods): void;
}

export const OVERTONES: Overtone[] = [
  { id: 'sillage', cost: 400, icon: 'a8', apply: (m) => m.has.add('sillage') },
  { id: 'tenuto', cost: 500, icon: 'a9', apply: (m) => { m.styleKeep = 2; } },
  { id: 'forte', cost: 900, icon: 'a10', apply: (m) => { m.damage *= 1.2; } },
  { id: 'aimant', cost: 300, icon: 'a11', apply: (m) => { m.magnet = 2.2; } },
  { id: 'reprise', cost: 1200, icon: 'a12', apply: (m) => m.has.add('reprise') },
  { id: 'frolement', cost: 600, icon: 'a13', apply: (m) => m.has.add('frolement') },
  { id: 'trille', cost: 1400, icon: 'a14', apply: (m) => m.has.add('trille') },
  { id: 'ampleur', cost: 700, icon: 'a15', apply: (m) => { m.pulseRadius = 1.35; } },
  { id: 'orgue', cost: 800, icon: 'b0', apply: (m) => { m.overdrive = 1.5; } },
  { id: 'harmonie', cost: 1500, icon: 'b1', apply: (m) => m.has.add('harmonie') },
  { id: 'allegro', cost: 600, icon: 'b2', apply: (m) => { m.speed = 1.12; } },
  { id: 'contrepoint', cost: 1100, icon: 'b3', apply: (m) => m.has.add('contrepoint') },
  { id: 'tempsfort', cost: 1000, icon: 'b4', apply: (m) => m.has.add('tempsfort') },
  { id: 'sourdine', cost: 900, icon: 'b5', apply: (m) => m.has.add('sourdine') },
  { id: 'virtuose', cost: 2000, icon: 'b6', apply: (m) => m.has.add('virtuose') },
  { id: 'metronome', cost: 0, icon: 'b7', apply: (m) => m.has.add('metronome') },
];

export const WEAPON_UNLOCK: Record<string, string> = { prism: '', chord: 's1_4', lance: 's2_4', arc: 's3_4', swarm: 's4_4' };
export const WEAPON_UPGRADE = [0, 800, 2200];

export interface StageRecord {
  best: number;
  rank: number;
  cleared: boolean;
  fragment: boolean;
}

export interface Save {
  version: 1;
  stages: Record<string, StageRecord>;
  notes: number;
  weaponLevels: Record<string, number>;
  weapon: string;
  owned: string[];
  equipped: string[];
  fragments: string[];
  seenIntro: boolean;
  ending: '' | 'coda' | 'encore';
  endlessBest: number;
  endlessWave: number;
  hard: boolean;
  deaths: number;
  playFrames: number;
}

export const newSave = (): Save => ({
  version: 1,
  stages: {},
  notes: 0,
  weaponLevels: { prism: 1 },
  weapon: 'prism',
  owned: ['metronome'],
  equipped: [],
  fragments: [],
  seenIntro: false,
  ending: '',
  endlessBest: 0,
  endlessWave: 0,
  hard: false,
  deaths: 0,
  playFrames: 0,
});

/** Letter ranks of a stage result: D C B A S, and V (VOLTA) for an S with no hit and the top style rank. */
export const GRADES = ['D', 'C', 'B', 'A', 'S', 'V'];
export function grade(stageId: string, score: number, hits: number, maxRank: number): number {
  const par = STAGES[stageId].par;
  let g = 0;
  for (let i = 0; i < 4; i++) if (score >= par[i]) g = i + 1;
  if (g === 4 && hits === 0 && maxRank >= 6) g = 5;
  return g;
}

export const cleared = (s: Save, id: string) => !!s.stages[id]?.cleared;

/** A stage is open when the previous one in its sector is cleared (or it is the first stage of an open sector). */
export function stageOpen(s: Save, id: string): boolean {
  const def = STAGES[id];
  if (def.index > 0) return cleared(s, `s${def.sector}_${def.index}`);
  if (def.sector === 1) return true;
  return cleared(s, `s${def.sector - 1}_4`);
}

export const weaponUnlocked = (s: Save, w: string) => !WEAPON_UNLOCK[w] || cleared(s, WEAPON_UNLOCK[w]);
export const pulseUnlocked = (s: Save) => cleared(s, 's1_1');
export const overdriveUnlocked = (s: Save) => cleared(s, 's1_4');
export const slots = (s: Save) => 2 + (cleared(s, 's2_4') ? 1 : 0) + (cleared(s, 's4_4') ? 1 : 0);
export const endlessUnlocked = (s: Save) => cleared(s, 's3_4');

export function buildMods(s: Save): Mods {
  const m = BASE_MODS();
  for (const id of s.equipped) OVERTONES.find((o) => o.id === id)?.apply(m);
  return m;
}

const KEY = 'volta.save.v1';
export function loadSave(): Save {
  try {
    const raw = localStorage.getItem(KEY);
    if (raw) return { ...newSave(), ...JSON.parse(raw) };
  } catch {
    /* fresh save */
  }
  return newSave();
}

export function writeSave(s: Save): void {
  try {
    localStorage.setItem(KEY, JSON.stringify(s));
  } catch {
    /* storage unavailable */
  }
}

export interface Settings {
  music: number;
  sfx: number;
  shake: number;
  flashes: number;
  latency: number;
  lang: string;
  bindings: Record<string, string[]> | null;
  fullscreen: boolean;
}

const SKEY = 'volta.settings.v1';
export function loadSettings(): Settings {
  const d: Settings = { music: 0.7, sfx: 0.8, shake: 1, flashes: 1, latency: 0, lang: navigator.language?.startsWith('fr') ? 'fr' : 'en', bindings: null, fullscreen: false };
  try {
    const raw = localStorage.getItem(SKEY);
    if (raw) return { ...d, ...JSON.parse(raw) };
  } catch {
    /* defaults */
  }
  return d;
}

export function writeSettings(s: Settings): void {
  try {
    localStorage.setItem(SKEY, JSON.stringify(s));
  } catch {
    /* storage unavailable */
  }
}
