import '../src/game/bosses/index';
import type { Controls } from '../src/game/controls';
import { StageRunner } from '../src/game/stage';
import { STAGES } from '../src/game/stages';
import { World } from '../src/game/world';

/**
 * A simple player: aims at the nearest foe, keeps its distance, sidesteps the closest bullets, dashes through
 * danger, pulses when bullets are about to land, taps fire on the beat when `rhythm` is set.
 */
export function botControls(w: World, opts: { rhythm?: boolean; dodge?: boolean } = {}): Controls {
  const p = w.player;
  const t = w.nearestEnemy(p.x, p.y, 80);
  let ax = 1;
  let ay = 0;
  let mx = 0;
  let my = 0;
  if (t) {
    ax = t.x - p.x;
    ay = t.y - p.y;
    const d = Math.hypot(ax, ay) || 1;
    ax /= d;
    ay /= d;
    const want = 8;
    mx = (d - want) * 0.25 * ax - ay * 0.6;
    my = (d - want) * 0.25 * ay + ax * 0.6;
  } else {
    mx = -p.x * 0.1;
    my = -p.y * 0.1;
  }
  let danger = 0;
  if (opts.dodge !== false) {
    const b = w.bullets;
    for (let i = 0; i < b.n; i++) {
      if (b.flags[i] === 2) continue;
      const dx = p.x - b.x[i];
      const dy = p.y - b.y[i];
      const d = Math.hypot(dx, dy);
      if (d < 3.5) {
        const k = (3.5 - d) / 3.5;
        mx += (dx / (d || 1)) * k * 3;
        my += (dy / (d || 1)) * k * 3;
        if (d < 1.4) danger++;
      }
    }
    for (const h of w.hazards) {
      if (h.kind === 'zone' && Math.hypot(p.x - h.x, p.y - h.y) < h.r + 1) {
        mx += (p.x - h.x) * 2;
        my += (p.y - h.y) * 2;
      }
    }
  }
  // Stay away from the edge.
  mx -= p.x * 0.02;
  my -= p.y * 0.02;
  const l = Math.hypot(mx, my);
  if (l > 1) {
    mx /= l;
    my /= l;
  }
  const onBeat = w.beat.judge() === 'perfect';
  return {
    mx,
    my,
    ax,
    ay,
    fire: opts.rhythm ? onBeat || w.frame % 3 !== 0 : true,
    firePressed: !!opts.rhythm && onBeat && Math.abs(w.beat.offsetSec()) < 0.017,
    dash: danger >= 2 && p.dashCharges > 0,
    pulse: danger >= 1 && p.pulseCd === 0 && onBeat,
    overdrive: w.gauge >= 100,
  };
}

export function playStage(id: string, o: { god?: boolean; maxFrames?: number; rhythm?: boolean; seed?: number; weapon?: string } = {}) {
  const def = STAGES[id];
  const w = new World(def.arena, def.bpm, o.seed ?? 3);
  w.god = o.god ?? true;
  if (o.weapon) w.player.weapon = o.weapon;
  w.player.y = 4;
  const run = new StageRunner(w, def);
  let maxPhase = 0;
  const max = o.maxFrames ?? 60 * 60 * 8;
  let f = 0;
  for (; f < max && !w.over; f++) {
    w.update(botControls(w, { rhythm: o.rhythm }));
    run.update();
    w.events.length = 0;
    for (const e of w.enemies) if (e.boss) maxPhase = Math.max(maxPhase, (e as unknown as { phase: number }).phase);
  }
  return { over: w.over, frames: f, seconds: Math.round(f / 60), score: w.score, stats: w.stats, maxPhase, hp: w.player.hp, wave: run.waveNumber, rank: w.rank };
}
