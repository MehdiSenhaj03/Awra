import type { RoomDef } from '../room';

// Zone 8 — Le Cœur (screens x 13-17, y 21-23). The heart of the star, where Aldrec still holds the chain.

export const COEUR: RoomDef[] = [
  {
    id: 'h_porte', zone: 'coeur', at: [15, 21],
    exits: { u: [[40, 45]], l: [[18, 21]], r: [[18, 21]] },
    solid: [[0, 22, 48, 5], [0, 0, 40, 3], [46, 0, 2, 3], [30, 12, 8, 3]],
    ents: [['shrine', 24, 21], ['tablet', 12, 21, { lore: 'l16' }]],
  },
  {
    // A trial of everything learned: rings over thorns, an eye that opens a gate for four seconds, the Constellation.
    id: 'h_epreuve', zone: 'coeur', at: [16, 21], size: [2, 1],
    exits: { l: [[18, 21]] },
    solid: [[0, 22, 10, 5], [0, 0, 96, 3], [84, 18, 12, 9], [93, 3, 3, 15]],
    slick: [[0, 0, 96, 3]],
    spike: [[10, 26, 74, 1]],
    plat: [[34, 17, 6]],
    brk: [[44, 3, 2, 23, 'gate_h1']],
    ents: [
      ['ring', 16, 15], ['ring', 30, 13], ['eye', 37, 8, { flag: 'gate_h1', timed: 240 }], ['ring', 50, 15], ['ring', 64, 12], ['ring', 78, 11],
      ['art', 90, 17, { value: 'constellation', id: 'art_constellation' }], ['astrelet', 56, 6], ['sentinelle', 88, 17], ['hint', 4, 21, { text: 'trial' }],
    ],
  },
  {
    id: 'h_veines', zone: 'coeur', at: [13, 21], size: [2, 1],
    exits: { r: [[18, 21]], d: [[20, 25]] },
    solid: [[0, 22, 20, 5], [26, 22, 70, 5], [0, 0, 96, 3], [0, 3, 3, 19], [40, 12, 16, 3], [70, 16, 8, 6]],
    plat: [[28, 15, 6], [60, 10, 6]],
    ents: [['sentinelle', 50, 21], ['sentinelle', 84, 21], ['astrelet', 48, 7], ['shard', 46, 11, { id: 'shard_h1' }]],
  },
  {
    id: 'h_chaines', zone: 'coeur', at: [13, 22], size: [2, 1],
    exits: { u: [[20, 25]], r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 20, 3], [26, 0, 70, 3], [0, 3, 3, 19], [30, 14, 6, 8], [60, 10, 6, 12]],
    plat: [[44, 14, 6], [76, 12, 6]],
    ents: [['sentinelle', 46, 21], ['ombre', 76, 21], ['astrelet', 70, 6], ['shrine', 88, 21], ['chant', 63, 9, { value: 'coeur', id: 'chant_h' }]],
  },
  {
    // The throne: Aldrec, the Chainer.
    id: 'h_trone', zone: 'coeur', at: [15, 22], size: [2, 1],
    exits: { l: [[18, 21]], d: [[40, 45]] },
    solid: [[0, 22, 40, 5], [46, 22, 50, 5], [0, 0, 96, 3], [93, 3, 3, 19]],
    brk: [[40, 22, 6, 5, 'seal_enchaineur_floor'], [0, 17, 2, 5, 'seal_enchaineur']],
    plat: [[16, 15, 8], [72, 15, 8], [42, 10, 10]],
    ents: [['arena', 60, 21, { boss: 'enchaineur', trigger: [12, 84], door: 'seal_enchaineur' }]],
  },
  {
    // The heart itself: the choice, and for those who sang the eight songs with Pip, the Dreaming Star.
    id: 'h_coeur', zone: 'coeur', at: [15, 23], size: [2, 1],
    exits: { u: [[40, 45]] },
    solid: [[0, 22, 96, 5], [0, 0, 40, 3], [46, 0, 50, 3], [0, 3, 3, 19], [93, 3, 3, 19]],
    plat: [[18, 14, 8], [70, 14, 8]],
    ents: [['heart', 48, 21]],
  },
];
