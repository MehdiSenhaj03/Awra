import { describe, expect, it } from 'vitest';
import { CELL_H, CELL_W } from '../src/engine/core';
import { ROOMS, bounds, roomAt } from '../src/world/world';

describe('world', () => {
  it('has unique ids and no overlapping rooms', () => {
    const ids = new Set<string>();
    for (const r of ROOMS) {
      expect(ids.has(r.id), r.id).toBe(false);
      ids.add(r.id);
    }
    for (const a of ROOMS) for (const b of ROOMS) {
      if (a === b) continue;
      const A = bounds(a);
      const B = bounds(b);
      const o = A.x < B.x + B.w && A.x + A.w > B.x && A.y < B.y + B.h && A.y + A.h > B.y;
      expect(o, `${a.id} overlaps ${b.id}`).toBe(false);
    }
  });

  it('every exit leads into a room with a matching opening', () => {
    const problems: string[] = [];
    for (const r of ROOMS) {
      const B = bounds(r);
      const check = (side: 'l' | 'r' | 'u' | 'd', a: number, b: number) => {
        for (let v = a; v <= b; v++) {
          const gx = side === 'l' ? B.x - 1 : side === 'r' ? B.x + B.w : B.x + v;
          const gy = side === 'u' ? B.y - 1 : side === 'd' ? B.y + B.h : B.y + v;
          const n = roomAt(gx, gy);
          if (!n) { problems.push(`${r.id} ${side}${v}: nothing beyond`); return; }
          const NB = bounds(n);
          const opp = { l: 'r', r: 'l', u: 'd', d: 'u' }[side] as 'l' | 'r' | 'u' | 'd';
          const local = side === 'l' || side === 'r' ? gy - NB.y : gx - NB.x;
          const spans = n.exits?.[opp] ?? [];
          if (!spans.some(([s0, s1]) => local >= s0 && local <= s1)) { problems.push(`${r.id} ${side}${v} -> ${n.id} has no ${opp} opening at ${local}`); return; }
        }
      };
      for (const side of ['l', 'r', 'u', 'd'] as const) for (const [a, b] of r.exits?.[side] ?? []) check(side, a, b);
    }
    expect(problems).toEqual([]);
  });

  it('rooms fit the grid', () => {
    for (const r of ROOMS) {
      const [w, h] = r.size ?? [1, 1];
      expect(w * CELL_W).toBeGreaterThan(0);
      expect(h * CELL_H).toBeGreaterThan(0);
    }
  });
});

import { buildRoom, isSolid, tileAt } from '../src/world/room';

describe('exits', () => {
  it('no exit is walled off from the inside', () => {
    const bad: string[] = [];
    for (const r of ROOMS) {
      const seals: Record<string, boolean> = {};
      for (const [, , , , f] of r.brk ?? []) if (f.startsWith('seal_')) seals[f] = true;
      const room = buildRoom(r, seals);
      for (const side of ['l', 'r', 'u', 'd'] as const) {
        for (const [a, b] of r.exits?.[side] ?? []) {
          // Two tiles in from the edge, at least 3 consecutive open tiles along the span.
          let run = 0;
          let best = 0;
          for (let v = a; v <= b; v++) {
            let open = true;
            for (let k = 0; k < 3; k++) {
              const tx = side === 'l' ? k : side === 'r' ? room.w - 1 - k : v;
              const ty = side === 'u' ? k : side === 'd' ? room.h - 1 - k : v;
              if (isSolid(tileAt(room, tx, ty))) open = false;
            }
            run = open ? run + 1 : 0;
            best = Math.max(best, run);
          }
          const need = side === 'l' || side === 'r' ? 3 : 2;
          if (best < Math.min(need, b - a + 1)) bad.push(`${r.id} ${side}[${a},${b}]`);
        }
      }
    }
    expect(bad).toEqual([]);
  });
});
