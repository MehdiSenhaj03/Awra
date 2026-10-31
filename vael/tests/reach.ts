// World checker: explores the whole kingdom with the real hero physics and a set of input "macros"
// (runs, jumps, dashes, wall jumps, blade footholds, Starstep, ring pulls, strikes), and reports what is reachable.

import { TILE } from '../src/engine/core';
import { NO_CONTROLS, type Controls } from '../src/game/controls';
import { Arena, Crack, Eye, Interactive, Pickup, Ring } from '../src/game/props';
import { newSave } from '../src/game/save';
import { BOSS_REWARD, Scene } from '../src/game/scene';
import { spawnEntity } from '../src/game/spawn';
import { neighbour, roomDef, ROOMS } from '../src/world/world';

type Ctl = Partial<Controls>;
type Macro = (f: number, s: Scene) => Ctl | null;

const C = (o: Ctl): Controls => ({ ...NO_CONTROLS, ...o });

export interface ReachResult {
  rooms: Set<string>;
  flags: Set<string>;
  abilities: Set<string>;
  bosses: Set<string>;
  heart: boolean;
  log: string[];
  positions: string[];
}

function macros(s: Scene): { name: string; m: Macro; frames: number }[] {
  const list: { name: string; m: Macro; frames: number }[] = [];
  const has = (a: string) => s.has(a);
  for (const d of [-1, 1]) {
    for (const k of [8, 30, 90]) list.push({ name: `run${d}x${k}`, frames: k + 60, m: (f) => (f < k ? { x: d } : {}) });
    for (const hold of [4, 16]) {
      for (const drift of [10, 24, 60]) {
        list.push({ name: `jump${d}h${hold}d${drift}`, frames: 120, m: (f) => ({ x: f < drift ? d : 0, jumpPressed: f === 0, jumpHeld: f < hold }) });
      }
      list.push({ name: `upjump${d}h${hold}`, frames: 120, m: (f) => ({ x: f > 10 ? d : 0, jumpPressed: f === 0, jumpHeld: f < hold }) });
    }
    // Run-up then jump.
    list.push({ name: `runjump${d}`, frames: 140, m: (f) => ({ x: d, jumpPressed: f === 20, jumpHeld: f >= 20 && f < 36 }) });
    list.push({ name: `drop${d}`, frames: 80, m: (f) => ({ x: f < 6 ? d : 0, y: f < 3 ? 1 : 0, jumpPressed: f === 1 }) });
    if (has('dash')) {
      for (const at of [4, 12, 20]) list.push({ name: `jdash${d}@${at}`, frames: 140, m: (f) => ({ x: d, jumpPressed: f === 0, jumpHeld: f < 16, dashPressed: f === at }) });
      list.push({ name: `rundash${d}`, frames: 140, m: (f) => ({ x: d, jumpPressed: f === 14, jumpHeld: f >= 14 && f < 30, dashPressed: f === 26 }) });
      list.push({ name: `gdash${d}`, frames: 90, m: (f) => ({ x: d, dashPressed: f === 0 }) });
    }
    if (has('double')) {
      for (const at of [10, 18, 26]) {
        list.push({ name: `double${d}@${at}`, frames: 150, m: (f) => ({ x: d, jumpPressed: f === 0 || f === at, jumpHeld: f < 16 || (f >= at && f < at + 9) }) });
        list.push({ name: `doubleUp${d}@${at}`, frames: 150, m: (f) => ({ x: f > at + 6 ? d : 0, jumpPressed: f === 0 || f === at, jumpHeld: f < 16 || (f >= at && f < at + 9) }) });
      }
      if (has('dash')) list.push({ name: `ddash${d}`, frames: 160, m: (f) => ({ x: d, jumpPressed: f === 0 || f === 18, jumpHeld: f < 34, dashPressed: f === 30 }) });
    }
    if (has('walljump')) {
      // Climb between walls: keep jumping, alternate the held direction to face each wall.
      for (const period of [16, 22]) {
        list.push({
          name: `wallclimb${d}p${period}`, frames: 260,
          m: (f, sc) => {
            const p = sc.player;
            const toward = p.body.wall !== 0 ? p.body.wall : d * (Math.floor(f / period) % 2 === 0 ? 1 : -1);
            return { x: f > 220 ? d : toward, jumpPressed: f % period === 0, jumpHeld: f % period < period - 4 };
          },
        });
      }
      for (const cycles of [2, 3, 4, 6]) {
        list.push({
          name: `wallup${d}c${cycles}`, frames: 60 + cycles * 18 + 40,
          m: (f, sc) => {
            const p = sc.player;
            if (f < cycles * 18) return { x: p.body.wall !== 0 ? p.body.wall : d * (Math.floor(f / 18) % 2 === 0 ? 1 : -1), jumpPressed: f % 18 === 0, jumpHeld: f % 18 < 14 };
            return { x: d, jumpPressed: p.body.wall !== 0 && f % 6 === 0, jumpHeld: true };
          },
        });
      }
      list.push({ name: `wallexit${d}`, frames: 200, m: (f, sc) => ({ x: sc.player.body.wall !== 0 ? sc.player.body.wall : d, jumpPressed: f % 18 === 0, jumpHeld: true }) });
    }
    // Blade foothold: jump against a wall, throw while rising, land on the hilt, jump again.
    for (const at of [5, 9, 13]) {
      list.push({
        name: `bladeclimb${d}@${at}`, frames: 240,
        m: (f, sc) => {
          const p = sc.player;
          const second = f > 40 && p.body.onGround && sc.blade.state === 'stuck';
          return { x: f > 200 ? d : d, jumpPressed: f === 0 || second, jumpHeld: true, bladePressed: f === at };
        },
      });
    }
    // Strikes while walking (cracks, eyes).
    list.push({ name: `strike${d}`, frames: 90, m: (f) => ({ x: f < 30 ? d : 0, attackPressed: f % 8 === 0 }) });
    list.push({ name: `jstrike${d}`, frames: 90, m: (f) => ({ x: d, y: -1, jumpPressed: f === 0, jumpHeld: f < 16, attackPressed: f % 6 === 3 }) });
  }
  // Throws in eight directions (eyes behind bars), with Starstep when owned.
  const dirs: [number, number][] = [[1, 0], [-1, 0], [0, -1], [1, -1], [-1, -1], [0, 1], [1, 1], [-1, 1]];
  const eyes = s.entities.some((e) => e instanceof Eye && !(e as Eye).open);
  for (const [dx, dy] of has('blink') || eyes ? dirs : []) {
    for (const airborne of [false, true]) {
      list.push({
        name: `throw${dx},${dy}${airborne ? 'a' : ''}`, frames: 120,
        m: (f, sc) => {
          const t = airborne ? 8 : 1;
          const c: Ctl = { x: f === t ? dx : 0, y: f === t ? dy : 0, jumpPressed: airborne && f === 0, jumpHeld: airborne && f < 16, bladePressed: f === t };
          if (has('blink') && f === t + 24 && (sc.blade.state === 'stuck' || sc.blade.state === 'branded')) c.blinkPressed = true;
          if (f === 100 && sc.blade.out()) c.bladePressed = true;
          return c;
        },
      });
    }
  }
  // Rings: aim at each ring, pull, then chain to the next.
  if (has('ring') && has('blink')) {
    const rings = s.entities.filter((e) => e instanceof Ring);
    for (const r0 of rings) {
      list.push({
        name: `ring@${Math.round(r0.cx / TILE)}`, frames: 240,
        m: (f, sc) => {
          const p = sc.player;
          const b = sc.blade;
          if (b.held() && !p.zip) {
            // Aim at the nearest ring above or ahead that is not the one just used.
            const cands = sc.entities.filter((e) => e instanceof Ring && Math.hypot(e.cx - p.cx, e.cy - p.cy) < 640);
            const target = f < 4 ? r0 : cands.sort((a, c) => (a.cy - c.cy) || 0).find((e) => e.cy < p.cy - 40) ?? null;
            if (!target) return { x: 0 };
            const dx = target.cx - p.cx;
            const dy = target.cy - (p.cy - 8);
            const ax = Math.abs(dx) < 60 ? 0 : Math.sign(dx);
            const ay = dy < -60 ? -1 : dy > 60 ? 1 : 0;
            return { x: ax, y: ay, bladePressed: true, jumpPressed: p.body.onGround && ay > 0, jumpHeld: true };
          }
          if (b.ring) return { blinkPressed: true };
          if (b.state === 'stuck' && !b.ring) return { bladePressed: true };
          return {};
        },
      });
    }
  }
  return list;
}

/** Explores until nothing new is found. `ban` removes an ability from the world (it never gets granted). */
export function explore(opts: { ban?: string; maxRounds?: number; verbose?: boolean } = {}): ReachResult {
  const save = newSave(1);
  save.notches = 3;
  const res: ReachResult = { rooms: new Set(), flags: new Set(), abilities: new Set(), bosses: new Set(), heart: false, log: [], positions: [] };
  const scene = new Scene(save, { roomDef: (id) => roomDef(id), neighbour, spawn: spawnEntity });
  scene.headless = true;
  scene.god = true;
  const grant = () => {
    for (const a of save.abilities) res.abilities.add(a);
    for (const f of Object.keys(save.flags)) res.flags.add(f);
  };
  // Rewards of keepers are granted as soon as the arena is reached.
  scene.startBoss = (key: string) => {
    if (res.bosses.has(key)) return;
    res.bosses.add(key);
    scene.setFlag(`boss_${key}`);
    const r = BOSS_REWARD[key];
    if (r && !(opts.ban && r[1] === opts.ban)) {
      if (r[0] === 'ability') save.abilities.push(r[1]);
      else if (r[0] === 'art') save.arts.push(r[1]);
    }
    for (const [, , , , flag] of scene.room.def.brk ?? []) if (flag.startsWith('seal_')) scene.openBreakable(flag, true);
    res.log.push(`boss ${key} in ${scene.room.id}`);
  };
  scene.ui = (kind) => {
    if (kind === 'heart') res.heart = true;
  };
  const origGive = scene.give.bind(scene);
  scene.give = (kind, value, flag, x, y) => {
    if (kind === 'ability' && value === opts.ban) { scene.setFlag(flag); return; }
    origGive(kind, value, flag, x, y);
  };

  const enter = (id: string) => {
    scene.enter(id);
    // Foes do not matter for reachability.
    scene.entities = scene.entities.filter((e) => e.prop || e instanceof Interactive || e instanceof Pickup || e instanceof Crack || e instanceof Eye || e instanceof Ring || e instanceof Arena);
  };

  const place = (x: number, feet: number) => {
    const p = scene.player;
    p.body.x = x - p.body.w / 2;
    p.body.y = feet - p.body.h;
    p.body.vx = 0;
    p.body.vy = 0;
    p.body.onGround = true;
    p.dashT = 0; p.dashCd = 0; p.airDashUsed = false; p.doubleUsed = false; p.attack = null; p.zip = null; p.throwT = 0; p.artT = 0; p.artFree = false;
    p.jumpBuf = 0; p.attackBuf = 0; p.dashBuf = 0; p.bladeBuf = 0; p.wallLockT = 0; p.coyote = 6; p.hurtT = 0; p.dead = false; p.hp = p.maxHp;
    scene.blade.catchNow();
    scene.respawnT = 0;
    scene.freeze = 0;
  };

  const key = (room: string, x: number, feet: number) => `${room}:${Math.round(x / TILE / 3) * 3}:${Math.round(feet / TILE)}`;
  const perRoom = new Map<string, number>();
  const settle = (): boolean => {
    for (let i = 0; i < 90; i++) {
      if (scene.player.body.onGround && scene.blade.held()) return true;
      scene.update(C(i % 30 === 0 && scene.blade.out() ? { bladePressed: true } : {}));
      if (scene.pending) return false;
    }
    return scene.player.body.onGround;
  };

  for (let round = 0; round < (opts.maxRounds ?? 14); round++) {
    const before = `${save.abilities.length}|${Object.keys(save.flags).length}`;
    const seen = new Set<string>();
    perRoom.clear();
    const queue: [string, number, number][] = [];
    // Start at the first altar.
    enter('c_reveil');
    const start: [string, number, number] = ['c_reveil', 12 * TILE, 22 * TILE];
    queue.push(start);
    seen.add(key(...start));
    let steps = 0;
    while (queue.length) {
      const [room, x, feet] = queue.shift()!;
      res.rooms.add(room);
      if (scene.room?.id !== room || scene.tilesChanged < 0) enter(room);
      const ms = macros(scene);
      for (const mc of ms) {
        if (scene.room.id !== room) enter(room);
        place(x, feet);
        let left = false;
        let still = 0;
        for (let f = 0; f < mc.frames; f++) {
          const c = mc.m(f, scene);
          scene.update(C(c ?? {}));
          if (scene.pending) break;
          // Landed and at rest for a while: the rest of the macro would only stand there.
          const p0 = scene.player;
          if (f > 20 && p0.body.onGround && Math.abs(p0.body.vx) < 0.5 && scene.blade.held() && !p0.attack) {
            if (++still > 12 && !mc.name.startsWith('wall') && !mc.name.startsWith('blade') && !mc.name.startsWith('ring') && !mc.name.startsWith('strike')) break;
          } else still = 0;
          if (scene.player.hp < scene.player.maxHp) scene.player.hp = scene.player.maxHp;
          // Interact with everything in reach (frescoes, altars, the heart).
          for (const e of scene.entities) if (e instanceof Interactive && e.inRange() && e.prompt === 'touch') res.heart = true;
        }
        if (scene.pending) {
          const pd = scene.pending;
          scene.pending = null;
          enter(pd.def.id);
          const p = scene.player;
          p.body.x = pd.x;
          p.body.y = pd.y;
          p.body.vx = pd.vx;
          p.body.vy = Math.min(pd.vy, pd.vy < 0 ? -14 : pd.vy);
          // Keep going the same way a little, then settle.
          const dir = Math.sign(pd.vx);
          for (let i = 0; i < 40 && !scene.pending; i++) scene.update(C({ x: dir, jumpHeld: pd.vy < 0 }));
          left = true;
        }
        if (!left || !scene.pending) {
          if (settle() && !scene.pending) {
            const p = scene.player;
            const k = key(scene.room.id, p.cx, p.feet);
            const n = perRoom.get(scene.room.id) ?? 0;
            if (!seen.has(k) && n < 70) {
              seen.add(k);
              perRoom.set(scene.room.id, n + 1);
              queue.push([scene.room.id, p.cx, p.feet]);
            }
          }
        }
        if (scene.room.id !== room) enter(room);
        steps++;
      }
      grant();
    }
    grant();
    const after = `${save.abilities.length}|${Object.keys(save.flags).length}`;
    if (opts.verbose) console.log(`round ${round}: ${seen.size} positions, ${res.rooms.size} rooms, abilities ${[...save.abilities].join(',')}, ${steps} macros`);
    res.log.push(`round ${round}: ${seen.size} positions, rooms ${res.rooms.size}`);
    res.positions = [...seen];
    if (after === before) break;
  }
  return res;
}

/** Every pickup id and keeper in the world. */
export function inventory(): { pickups: string[]; bosses: string[] } {
  const pickups: string[] = [];
  const bosses: string[] = [];
  for (const r of ROOMS) for (const e of r.ents ?? []) {
    const o = (e[3] ?? {}) as Record<string, string>;
    if (['ability', 'art', 'shard', 'vessel', 'ore', 'notch', 'chant', 'seal', 'map', 'key'].includes(e[0])) pickups.push(o.id ?? `${r.id}:${e[0]}`);
    if (e[0] === 'arena') bosses.push(o.boss);
  }
  return { pickups, bosses };
}
