import { describe, expect, it } from 'vitest';
import { explore, inventory } from './reach';

describe('the kingdom can be completed', () => {
  it('every room, pickup and keeper is reachable, and the heart', () => {
    const r = explore({ verbose: true });
    const inv = inventory();
    const missingP = inv.pickups.filter((p) => !r.flags.has(p));
    const missingB = inv.bosses.filter((b) => !r.bosses.has(b));
    console.log('rooms', r.rooms.size, 'abilities', [...r.abilities].join(','), 'missing pickups', missingP, 'missing keepers', missingB);
    expect(missingB).toEqual([]);
    expect(missingP).toEqual([]);
    expect(r.heart).toBe(true);
  }, 1800000);
});

// Each ability is needed: without it, the keeper behind its gate cannot be reached.
const GATES: [string, string][] = [
  ['dash', 'racine'], ['walljump', 'forgeron'], ['blink', 'bibliothecaire'], ['double', 'geoliers'], ['ring', 'oiseau'], ['comet', 'reine'],
];
describe('each ability is really needed', () => {
  for (const [ab, boss] of GATES) {
    it(`without ${ab}, ${boss} cannot be reached`, () => {
      const r = explore({ ban: ab, stopAt: boss });
      expect(r.bosses.has(boss)).toBe(false);
    }, 3600000);
  }
});
