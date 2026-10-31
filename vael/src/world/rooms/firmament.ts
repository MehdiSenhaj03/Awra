import type { RoomDef } from '../room';

// Zone 6 — Le Firmament Inversé (screens x 16-23, y 1-4). A piece of sky folded under the earth, above the Crown.
// Only astral rings hang in the void: the Tether pulls the hero from ring to ring. The Comet Bird gives the Comet Wake.

export const FIRMAMENT: RoomDef[] = [
  {
    id: 'i_seuil', zone: 'firmament', at: [16, 3], size: [1, 2],
    exits: { d: [[20, 25]], r: [[6, 9]] },
    solid: [[0, 51, 20, 3], [26, 51, 22, 3], [0, 10, 4, 41], [44, 10, 4, 41], [34, 10, 14, 3], [0, 0, 48, 3], [8, 30, 8, 2]],
    slick: [[0, 10, 4, 41], [44, 10, 4, 41], [8, 30, 8, 2], [0, 0, 48, 3]],
    plat: [[17, 49, 12]],
    ents: [['ring', 23, 43], ['ring', 15, 34], ['ring', 25, 25], ['ring', 23, 17], ['ring', 34, 8], ['astrelet', 36, 30], ['hint', 10, 50, { text: 'ring' }]],
  },
  {
    id: 'i_iles', zone: 'firmament', at: [17, 2], size: [2, 2],
    exits: { l: [[33, 36]], r: [[10, 13]] },
    solid: [[0, 37, 14, 4], [24, 30, 12, 3], [44, 38, 10, 3], [62, 26, 10, 3], [78, 14, 18, 3], [30, 16, 10, 3], [0, 0, 96, 3]],
    slick: [[0, 0, 96, 3]],
    plat: [[52, 20, 5], [12, 24, 5]],
    ents: [
      ['ring', 20, 46], ['ring', 58, 46], ['ring', 72, 36], ['corbeau', 40, 24], ['astrelet', 66, 18], ['spore', 20, 30],
      ['shard', 34, 15, { id: 'shard_i1' }],
    ],
  },
  {
    id: 'i_temple', zone: 'firmament', at: [19, 2], size: [2, 1],
    exits: { l: [[10, 13]], r: [[18, 21]] },
    solid: [[0, 14, 30, 13], [30, 22, 66, 5], [0, 0, 96, 3], [56, 8, 10, 3]],
    plat: [[40, 16, 6], [74, 14, 6]],
    ents: [['shrine', 20, 13], ['art', 61, 7, { value: 'ascension2', id: 'art_ascension2' }], ['astrelet', 48, 10], ['corbeau', 80, 8], ['sentinelle', 84, 21]],
  },
  {
    id: 'i_aurore', zone: 'firmament', at: [21, 2], size: [2, 2],
    exits: { l: [[18, 21]], u: [[40, 45]], r: [[30, 33]] },
    solid: [[0, 22, 20, 3], [0, 0, 40, 3], [46, 0, 50, 3], [30, 34, 14, 3], [56, 28, 12, 3], [80, 34, 16, 3], [40, 12, 10, 3], [0, 50, 96, 4]],
    gloam: [[0, 49, 96, 1]],
    plat: [[66, 18, 6], [20, 40, 6]],
    ents: [['ring', 26, 30], ['ring', 50, 40], ['ring', 72, 22], ['ring', 44, 22], ['astrelet', 60, 36], ['corbeau', 30, 20], ['spore', 84, 24]],
  },
  {
    id: 'i_nuages', zone: 'firmament', at: [23, 2], size: [1, 2],
    exits: { l: [[30, 33]] },
    solid: [[0, 34, 12, 3], [0, 0, 48, 3], [30, 46, 18, 3], [28, 14, 16, 3]],
    gloam: [[0, 53, 48, 1]],
    ents: [['ring', 20, 40], ['ring', 26, 26], ['vessel', 38, 45, { id: 'vessel_i1' }], ['notch', 36, 13, { id: 'notch_i' }], ['astrelet', 34, 30]],
  },
  {
    id: 'i_obs', zone: 'firmament', at: [21, 1], size: [2, 1],
    exits: { d: [[40, 45]], l: [[18, 21]] },
    solid: [[0, 22, 40, 5], [46, 22, 50, 5], [0, 0, 96, 3], [93, 3, 3, 19], [60, 12, 20, 3]],
    plat: [[48, 16, 6], [84, 14, 6]],
    ents: [['chant', 70, 11, { value: 'firmament', id: 'chant_i' }], ['sentinelle', 30, 21], ['astrelet', 70, 6], ['tablet', 10, 21, { lore: 'l12' }]],
  },
  {
    // The Comet Bird's sky.
    id: 'i_arene', zone: 'firmament', at: [19, 1], size: [2, 1],
    exits: { r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 96, 3], [0, 3, 3, 19]],
    plat: [[14, 15, 8], [74, 15, 8], [40, 10, 16]],
    brk: [[94, 17, 2, 5, 'seal_oiseau']],
    ents: [['arena', 30, 21, { boss: 'oiseau', trigger: [10, 84], door: 'seal_oiseau' }]],
  },
];
