import { it } from 'vitest';
import { explore } from './reach';
it('dbg', () => {
  const start = (process.env.START ?? 'c_vitraux,38,22').split(',');
  const r = explore({ ban: process.env.BAN, maxRounds: 3, start: [start[0], +start[1], +start[2]], abilities: (process.env.AB ?? 'dash,walljump,blink,double,ring').split(',') });
  console.log('ROOMS', [...r.rooms].join(' '), 'BOSSES', [...r.bosses].join(' '), 'FLAGS', [...r.flags].filter((f) => !f.startsWith('read')).join(' '));
  for (const room of (process.env.ROOMS ?? '').split(',')) {
    const ys = new Map<number, number[]>();
    for (const p of r.positions.filter((p) => p.startsWith(room + ':'))) {
      const [, x, y] = p.split(':').map(Number);
      ys.set(y, [...(ys.get(y) ?? []), x]);
    }
    console.log(room, [...ys.entries()].sort((a, b) => a[0] - b[0]).map(([y, xs]) => `y${y}: ${Math.min(...xs)}-${Math.max(...xs)}`).join(' | '));
  }
}, 1800000);
