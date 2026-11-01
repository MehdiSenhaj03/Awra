/** A full screen or an overlay. */
export interface Screen {
  root: HTMLElement;
  /** Called every displayed frame with this frame's input already polled. */
  update(dt: number): void;
  dispose?(): void;
}

export const img = (name: string) => `img/${name}.webp`;

/** Portrait image for a speaker. */
export const PORTRAIT: Record<string, string> = {
  volta: 'pt_volta',
  corda: 'pt_corda',
  ostinato: 'boss_ostinato',
  crescendo: 'boss_crescendo',
  fugue: 'boss_fugue',
  rubato: 'boss_rubato',
  staccato: 'boss_staccato',
  fermata: 'boss_fermata',
};

export const SECTOR_COLOR: Record<number, string> = { 1: '#2ef2ff', 2: '#40ffb0', 3: '#ffa040', 4: '#c8b4ff', 5: '#ff2fb9', 6: '#ffe9b0' };
export const SECTOR_BG: Record<number, string> = { 1: 'bg_seuil', 2: 'bg_jardin', 3: 'bg_fonderie', 4: 'bg_ocean', 5: 'bg_ville', 6: 'bg_coeur' };
