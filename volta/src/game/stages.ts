import { F, wave } from './formations';
import type { StageDef } from './stage';

/** All stages, by id. Filled per sector. */
export const STAGES: Record<string, StageDef> = {};

function def(s: StageDef): void {
  STAGES[s.id] = s;
}

// Sector 1 — Le Seuil (128 BPM). Teaches movement, rhythm firing, dash, pulse.
def({
  id: 's1_1', sector: 1, index: 0, bpm: 128, theme: 'seuil',
  arena: { shape: 'circle', r: 19 },
  par: [8000, 16000, 26000, 38000],
  movements: [
    [
      wave(F.ring('mote', 6, 12, 0)),
      wave(F.edge('mote', 10, 15, 0, 0.5, 2), F.at('spitter', 0, -12, 2)),
    ],
    [
      wave(F.at('spitter', -10, -6, 0), F.at('spitter', 10, -6, 0), F.edge('mote', 8, 15, 2, 0.25, 3)),
      wave(F.at('spinner', 0, -8, 0), F.ring('mote', 8, 14, 4)),
    ],
    [
      wave(F.line('lancer', 3, -10, -12, 10, -12, 0, 1), F.edge('mote', 12, 16, 3, 0.25, 5)),
      wave(F.at('splitter', -8, 0, 0), F.at('splitter', 8, 0, 0), F.at('spitter', 0, -13, 2, true)),
    ],
  ],
  fragment: { m: 1, w: 1, id: 'f1' },
});
