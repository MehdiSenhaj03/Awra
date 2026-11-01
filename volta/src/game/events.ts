/** Things the simulation tells the renderer, the music and the interface. Purely descriptive. */
export type Judge = 'perfect' | 'great' | 'off';

export type Fx =
  | { t: 'shot'; x: number; y: number; a: number; accent: boolean; weapon: string }
  | { t: 'hit'; x: number; y: number; color: number; big: boolean }
  | { t: 'kill'; x: number; y: number; size: number; color: number; look: string; heavy: boolean }
  | { t: 'boom'; x: number; y: number; r: number; color: number }
  | { t: 'ring'; x: number; y: number; r: number; color: number; life?: number }
  | { t: 'spawn'; x: number; y: number; color: number; size: number }
  | { t: 'dash'; x: number; y: number; a: number; judge: Judge }
  | { t: 'pulse'; x: number; y: number; r: number; judge: Judge }
  | { t: 'parry'; x: number; y: number; n: number }
  | { t: 'graze'; x: number; y: number }
  | { t: 'hurt'; x: number; y: number; hp: number }
  | { t: 'die'; x: number; y: number }
  | { t: 'judge'; x: number; y: number; judge: Judge; what: string }
  | { t: 'score'; x: number; y: number; value: number }
  | { t: 'rank'; rank: number; up: boolean }
  | { t: 'overdrive'; on: boolean }
  | { t: 'note'; x: number; y: number }
  | { t: 'heal'; x: number; y: number }
  | { t: 'shake'; a: number }
  | { t: 'flash'; color: number; a: number }
  | { t: 'chroma'; a: number }
  | { t: 'boss'; phase: number; name: string }
  | { t: 'bossDown'; x: number; y: number }
  | { t: 'wave'; n: number; of: number }
  | { t: 'checkpoint' }
  | { t: 'clear' }
  | { t: 'fragment'; x: number; y: number; id: string }
  | { t: 'sfx'; name: string; x?: number; vol?: number; pitch?: number };
