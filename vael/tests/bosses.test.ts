import { describe, expect, it } from 'vitest';
import { Rng } from '../src/engine/core';
import { NO_CONTROLS, type Controls } from '../src/game/controls';
import { makeScene } from './helpers';

/** A bot that walks to the keeper and keeps striking (the vulnerable variant never jumps nor dodges). An invulnerable bot that walks to the keeper and keeps striking: every keeper must fall and play its phases. */
function fight(room: string, key: string, maxFrames = 60 * 240, god = true) {
  const s = makeScene(['dash', 'walljump', 'double', 'blink', 'ring', 'comet'], room);
  s.god = god;
  let hits = 0;
  let lastHp = s.player.hp;
  s.rng = new Rng(4242);
  const p = s.player;
  // Start inside, on the floor, away from the door.
  const fromRight = ['a_arene', 'h_trone'].includes(room);
  p.body.x = (fromRight ? 88 : 6) * 40;
  p.body.y = 22 * 40 - p.body.h;
  if (room === 'b_arene' || room === 'f_arene') { p.body.x = 12 * 40; }
  // Walk in to trigger the arena.
  for (let f = 0; f < 600 && !s.boss; f++) s.update({ ...NO_CONTROLS, x: fromRight ? -1 : 1 });
  expect(s.boss, 'boss spawned').toBeTruthy();
  let maxPhase = 1;
  for (let f = 0; f < maxFrames; f++) {
    const b = s.boss as unknown as { cx: number; cy: number; phase: number; dead: boolean } | null;
    const c: Controls = { ...NO_CONTROLS };
    if (b) {
      maxPhase = Math.max(maxPhase, b.phase);
      const dx = b.cx - p.cx;
      const dy = b.cy - p.cy;
      c.x = Math.abs(dx) > 60 ? Math.sign(dx) : 0;
      c.y = dy < -120 ? -1 : 0;
      if (god && dy < -100 && p.body.onGround && f % 20 === 0) { c.jumpPressed = true; c.jumpHeld = true; }
      if (!p.body.onGround) c.jumpHeld = true;
      c.attackPressed = Math.abs(dx) < 220 && f % 4 === 0;
    }
    s.update(c);
    if (p.hp < lastHp) hits++;
    lastHp = p.hp;
    if (!god && (p.hp <= 0 || p.pose === 'dead')) return { won: false, frames: f, phases: maxPhase, hits, died: true };
    if (s.flags[`boss_${key}`]) return { won: true, frames: f, phases: maxPhase, hits, died: false };
  }
  return { won: false, frames: -1, phases: maxPhase, hits, died: false };
}

const ARENAS: [string, string][] = [
  ['c_arene', 'gardien'], ['j_arene', 'racine'], ['f_arene', 'forgeron'], ['a_arene', 'bibliothecaire'], ['b_arene', 'geoliers'],
  ['b_sommet', 'veilleur'], ['i_arene', 'oiseau'], ['p_arene', 'reine'], ['h_trone', 'enchaineur'],
];

describe('keepers', () => {
  for (const [room, key] of ARENAS) {
    it(`${key} can be beaten and plays its three phases`, () => {
      const r = fight(room, key);
      console.log(key, r);
      expect(r.won).toBe(true);
      expect(r.phases).toBe(3);
    });
  }
});

describe('keepers punish face-tanking', () => {
  for (const [room, key] of ARENAS) {
    it(`${key} kills a hero who never dodges`, () => {
      const r = fight(room, key, 60 * 240, false);
      console.log('no-dodge', key, r);
      expect(r.won).toBe(false);
    });
  }
});
