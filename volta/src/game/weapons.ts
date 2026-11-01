import { SK } from './bullets';
import type { Controls } from './controls';
import type { Player } from './player';
import type { World } from './world';

/**
 * Weapons fire on the music grid: holding the trigger plays a steady pattern (sixteenths, eighths, beats…),
 * and a press landed on the beat triggers that weapon's PERFECT volley. Tapping in time beats holding.
 */
export interface Weapon {
  id: string;
  update(w: World, p: Player, c: Controls): void;
}

const lvl = (p: Player) => [1, 1.25, 1.5][p.weaponLevel - 1] ?? 1;

/** A press on the beat, at most once per beat. Returns the judgement, or null. */
function tap(w: World, p: Player, c: Controls): 'perfect' | 'great' | null {
  if (!c.firePressed) return null;
  const j = w.beat.judge();
  const bi = Math.round(w.beat.beat);
  if (j === 'off' || bi === p.lastPerfectBeat) return null;
  p.lastPerfectBeat = bi;
  w.judged(j, 'shot', p.x, p.y);
  return j;
}

function muzzle(p: Player, d = 0.7): [number, number] {
  return [p.x + Math.cos(p.aim) * d, p.y + Math.sin(p.aim) * d];
}

const prism: Weapon = {
  id: 'prism',
  update(w, p, c) {
    p.firing = c.fire;
    const m = lvl(p) * w.mods.damage;
    const od = p.overdrive;
    const j = tap(w, p, c);
    if (j) {
      const [x, y] = muzzle(p);
      const n = j === 'perfect' ? 5 : 3;
      for (let i = 0; i < n; i++) {
        const a = p.aim + (i - (n - 1) / 2) * 0.07;
        w.shots.add(x, y, a, 62, (j === 'perfect' ? 3 : 2) * m, SK.Perfect, 0.8, od ? 3 : 1);
      }
      w.emit({ t: 'shot', x, y, a: p.aim, accent: true, weapon: 'prismPerfect' });
      w.recoil(0.25);
      return;
    }
    if (c.fire && w.beat.crossed(4)) {
      const accent = w.beat.crossed(1);
      const [x, y] = muzzle(p);
      const spread = (w.rng.next() - 0.5) * 0.035;
      const down = accent && w.mods.has.has('tempsfort') && w.beat.index(1) % 4 === 0 ? 2 : 1;
      w.shots.add(x, y, p.aim + spread, accent ? 56 : 50, (accent ? 2.5 * down : 1) * m, accent ? SK.Accent : SK.Bolt, 0.75, od ? 2 : accent ? 1 : 0);
      if (od) for (const s of [-0.12, 0.12]) w.shots.add(x, y, p.aim + s, 50, m, SK.Bolt, 0.6, 1);
      w.emit({ t: 'shot', x, y, a: p.aim, accent, weapon: 'prism' });
      if (accent) w.recoil(0.08);
    }
  },
};

const chord: Weapon = {
  id: 'chord',
  update(w, p, c) {
    p.firing = c.fire;
    const m = lvl(p) * w.mods.damage;
    const j = tap(w, p, c);
    const volley = (n: number, spread: number, dmg: number, kind: SK, speed: number) => {
      const [x, y] = muzzle(p);
      for (let i = 0; i < n; i++) {
        const a = p.aim + (i / (n - 1) - 0.5) * spread + (w.rng.next() - 0.5) * 0.05;
        w.shots.add(x, y, a, speed * (0.9 + w.rng.next() * 0.2), dmg, kind, 0.32, p.overdrive ? 1 : 0);
      }
    };
    if (j) {
      volley(j === 'perfect' ? 11 : 8, 0.75, 1.3 * m, SK.Perfect, 46);
      w.emit({ t: 'shot', x: p.x, y: p.y, a: p.aim, accent: true, weapon: 'chordPerfect' });
      w.recoil(0.5);
      p.vx -= Math.cos(p.aim) * 9;
      p.vy -= Math.sin(p.aim) * 9;
      return;
    }
    if (c.fire && w.beat.crossed(2)) {
      const accent = w.beat.crossed(1);
      volley(accent ? 7 : 5, accent ? 0.6 : 0.5, (accent ? 1 : 0.75) * m, accent ? SK.Accent : SK.Pellet, 42);
      w.emit({ t: 'shot', x: p.x, y: p.y, a: p.aim, accent, weapon: 'chord' });
      w.recoil(accent ? 0.2 : 0.1);
    }
  },
};

const lance: Weapon = {
  id: 'lance',
  update(w, p, c) {
    const m = lvl(p) * w.mods.damage;
    if (c.fire) {
      if (p.chargeStartBeat < 0) p.chargeStartBeat = w.beat.beat;
      const held = w.beat.beat - p.chargeStartBeat;
      const lv = Math.min(3, Math.floor(held + 0.15));
      if (lv > p.charge) {
        p.charge = lv;
        w.sfx('charge', 0.8 + lv * 0.15);
      }
      p.firing = true;
      return;
    }
    if (p.chargeStartBeat >= 0) {
      const lv = Math.max(1, p.charge);
      const j = p.charge > 0 ? w.beat.judge() : 'off';
      if (j !== 'off') w.judged(j, 'shot', p.x, p.y);
      const k = j === 'perfect' ? 1.8 : j === 'great' ? 1.35 : 1;
      const [x, y] = muzzle(p, 0.9);
      const dmg = [3, 9, 17, 28][lv] * k * m;
      w.shots.add(x, y, p.aim, 85, dmg, SK.Lance, 0.55, 99);
      w.emit({ t: 'shot', x, y, a: p.aim, accent: lv >= 2, weapon: j === 'perfect' ? 'lancePerfect' : 'lance' });
      w.recoil(0.15 + lv * 0.12);
      p.vx -= Math.cos(p.aim) * lv * 4;
      p.vy -= Math.sin(p.aim) * lv * 4;
    }
    p.chargeStartBeat = -1;
    p.charge = 0;
    p.firing = false;
  },
};

const arc: Weapon = {
  id: 'arc',
  update(w, p, c) {
    p.firing = c.fire;
    const m = lvl(p) * w.mods.damage;
    const j = tap(w, p, c);
    if (j) {
      w.chain(p.x, p.y, p.aim, j === 'perfect' ? 7 : 5, 2.6 * m, 11, true);
      w.emit({ t: 'shot', x: p.x, y: p.y, a: p.aim, accent: true, weapon: 'arcPerfect' });
      return;
    }
    if (c.fire && w.beat.crossed(2)) {
      const accent = w.beat.crossed(1);
      w.chain(p.x, p.y, p.aim, accent ? 4 : 3, (accent ? 2.2 : 1.5) * m, 9, false);
      w.emit({ t: 'shot', x: p.x, y: p.y, a: p.aim, accent, weapon: 'arc' });
    }
  },
};

const swarm: Weapon = {
  id: 'swarm',
  update(w, p, c) {
    p.firing = c.fire;
    const m = lvl(p) * w.mods.damage;
    const launch = (n: number, dmg: number) => {
      for (let i = 0; i < n; i++) {
        const a = p.aim + Math.PI + (i / Math.max(1, n - 1) - 0.5) * 2.4;
        const s = w.shots.add(p.x, p.y, a, 16, dmg, SK.Missile, 2.2, 0);
        if (s >= 0) w.shots.target[s] = w.nearestEnemy(p.x + Math.cos(p.aim) * 6, p.y + Math.sin(p.aim) * 6, 30)?.id ?? 0;
      }
    };
    const j = tap(w, p, c);
    if (j) {
      launch(j === 'perfect' ? 7 : 5, 2.6 * m);
      w.emit({ t: 'shot', x: p.x, y: p.y, a: p.aim, accent: true, weapon: 'swarmPerfect' });
      return;
    }
    if (c.fire && w.beat.crossed(1)) {
      launch(p.overdrive ? 4 : 2, 2.8 * m);
      w.emit({ t: 'shot', x: p.x, y: p.y, a: p.aim, accent: true, weapon: 'swarm' });
    }
  },
};

export const WEAPONS: Record<string, Weapon> = { prism, chord, lance, arc, swarm };
export const WEAPON_ORDER = ['prism', 'chord', 'lance', 'arc', 'swarm'];
