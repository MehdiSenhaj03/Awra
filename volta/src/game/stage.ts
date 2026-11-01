import { TAU } from '../engine/core';
import type { ArenaDef } from './arena';
import { BOSSES } from './bosses';
import { ENEMY_KINDS, GoldenEcho } from './enemies';
import type { Enemy } from './enemy';
import type { World } from './world';

/** One enemy entering the arena `at` beats after its wave started. */
export interface Spawn {
  at: number;
  kind: string;
  x: number;
  y: number;
  elite?: boolean;
}

export type Wave = Spawn[];

export interface StageDef {
  id: string;
  sector: number;
  /** 0..2 regular stages, 3 = the Conductor. */
  index: number;
  bpm: number;
  arena: ArenaDef;
  /** Music theme. */
  theme: string;
  /** Movements (checkpoints), each a list of waves. */
  movements: Wave[][];
  boss?: string;
  /** A golden echo appears during this wave (movement, wave). */
  fragment?: { m: number; w: number; id: string };
  /** Score thresholds for ranks C, B, A, S. */
  par: [number, number, number, number];
}

/** Runs a stage: waves start on bar lines once the previous wave is cleared. */
export class StageRunner {
  movement: number;
  wave = -1;
  private waveBeat = 0;
  private pending: Spawn[] = [];
  state: 'intro' | 'run' | 'clear' | 'fail' = 'intro';
  private t = 0;
  boss: Enemy | null = null;
  /** Waves per movement, for the interface. */
  total: number;
  echoSpawned = false;

  constructor(public w: World, public def: StageDef, startMovement = 0) {
    this.movement = startMovement;
    this.total = def.movements.reduce((n, m) => n + m.length, 0) + (def.boss ? 1 : 0);
  }

  get waveNumber(): number {
    let n = 0;
    for (let m = 0; m < this.movement; m++) n += this.def.movements[m]?.length ?? 0;
    return n + Math.max(0, this.wave) + 1;
  }

  update(): void {
    const w = this.w;
    this.t++;
    if (this.state === 'clear' || this.state === 'fail') return;
    if (w.player.dead) {
      if (w.player.deadT > 100) {
        this.state = 'fail';
        w.over = 'lose';
      }
      return;
    }
    if (this.state === 'intro') {
      // Start on the next bar after a short breath.
      if (this.t > 60 && w.beat.crossed(1) && w.beat.index(1) % 4 === 0) {
        this.state = 'run';
        this.nextWave();
      }
      return;
    }
    const b = w.beat.beat;
    for (let i = this.pending.length - 1; i >= 0; i--) {
      const s = this.pending[i];
      if (b >= this.waveBeat + s.at) {
        this.spawn(s);
        this.pending.splice(i, 1);
      }
    }
    if (this.boss) {
      if (!w.enemies.some((e) => e.boss && !e.dead)) {
        this.state = 'clear';
        w.over = 'win';
        w.emit({ t: 'clear' });
      }
      return;
    }
    const alive = w.enemies.some((e) => !e.dead && e.kind !== 'echo');
    if (this.pending.length === 0 && !alive && w.beat.crossed(1) && w.beat.index(1) % 4 === 0) this.nextWave();
  }

  private nextWave(): void {
    const w = this.w;
    const mv = this.def.movements;
    this.wave++;
    if (this.movement < mv.length && this.wave >= mv[this.movement].length) {
      this.movement++;
      this.wave = 0;
      if (this.movement < mv.length || this.def.boss) w.emit({ t: 'checkpoint' });
    }
    if (this.movement >= mv.length) {
      if (this.def.boss && !this.boss) {
        const B = BOSSES[this.def.boss];
        this.boss = w.add(B(w));
        w.emit({ t: 'wave', n: this.total, of: this.total });
        return;
      }
      this.state = 'clear';
      w.over = 'win';
      w.emit({ t: 'clear' });
      return;
    }
    this.waveBeat = w.beat.beat;
    this.pending = mv[this.movement][this.wave].slice();
    w.emit({ t: 'wave', n: this.waveNumber, of: this.total });
    if (w.mods.has.has('sourdine')) w.guard = true;
    const f = this.def.fragment;
    if (f && !this.echoSpawned && f.m === this.movement && f.w === this.wave) {
      this.echoSpawned = true;
      const e = w.arena.edge(w.rng.range(0, TAU), 4);
      this.pending.push({ at: 4, kind: `echo:${f.id}`, x: e.x, y: e.y });
    }
  }

  private spawn(s: Spawn): void {
    const w = this.w;
    let e: Enemy;
    if (s.kind.startsWith('boss:')) {
      const b = BOSSES[s.kind.slice(5)](w);
      if (!w.enemies.includes(b)) w.add(b);
      return;
    }
    if (s.kind.startsWith('echo:')) e = new GoldenEcho(s.x, s.y, s.kind.slice(5));
    else e = ENEMY_KINDS[s.kind](s.x, s.y);
    if (s.elite) e.makeElite();
    e.spawnT = Math.round(w.beat.spb * 60);
    // Never on top of the hero: push the entry point out to a fair distance.
    const hero = w.player;
    const dx = e.x - hero.x;
    const dy = e.y - hero.y;
    const d = Math.hypot(dx, dy);
    const safe = e.heavy ? 9 : 6;
    if (d < safe) {
      const a = d > 0.01 ? Math.atan2(dy, dx) : w.rng.range(0, Math.PI * 2);
      e.x = hero.x + Math.cos(a) * safe;
      e.y = hero.y + Math.sin(a) * safe;
    }
    const p = { x: e.x, y: e.y };
    w.arena.confine(p, e.r + 0.5);
    e.x = e.px = p.x;
    e.y = e.py = p.y;
    w.add(e);
    w.emit({ t: 'spawn', x: e.x, y: e.y, color: e.color, size: e.r });
  }
}
