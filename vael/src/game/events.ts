/** What happened during a simulation frame, for the renderer and the audio (never read back by the simulation). */
export type FxEvent =
  | { t: 'slash'; x: number; y: number; dir: 'side' | 'up' | 'down'; facing: number; combo: number; palm: boolean }
  | { t: 'hit'; x: number; y: number; dx: number; dy: number; big: boolean; kind: 'slash' | 'blade' | 'rip' | 'execute' | 'art' | 'wave' | 'palm' }
  | { t: 'clank'; x: number; y: number }
  | { t: 'kill'; x: number; y: number; size: number; color: number; sprite: string; id: number }
  | { t: 'hurt'; x: number; y: number }
  | { t: 'dodge'; x: number; y: number }
  | { t: 'jump' | 'doubleJump' | 'land' | 'footstep' | 'wallJump'; x: number; y: number; dir?: number; big?: boolean }
  | { t: 'dash'; x: number; y: number; dir: number }
  | { t: 'throw'; x: number; y: number; dir: number }
  | { t: 'recall'; x: number; y: number }
  | { t: 'lodge'; x: number; y: number; angle: number }
  | { t: 'catch'; x: number; y: number }
  | { t: 'blink'; x0: number; y0: number; x1: number; y1: number; big: boolean }
  | { t: 'heal'; x: number; y: number }
  | { t: 'healStart'; x: number; y: number }
  | { t: 'art'; x: number; y: number; art: string; dir: number }
  | { t: 'shockwave'; x: number; y: number; r: number; color: number }
  | { t: 'boom'; x: number; y: number; r: number; color: number }
  | { t: 'dust'; x: number; y: number; n: number }
  | { t: 'pickup'; x: number; y: number; what: string }
  | { t: 'break'; x: number; y: number; w: number; h: number }
  | { t: 'telegraph'; x: number; y: number; big?: boolean }
  | { t: 'sfx'; name: string; x?: number; y?: number; vol?: number }
  | { t: 'roar'; x: number; y: number }
  | { t: 'spark'; x: number; y: number; n: number; color: number }
  | { t: 'spin'; x: number; y: number; big: boolean }
  | { t: 'trail'; x: number; y: number; vx: number; vy: number };
