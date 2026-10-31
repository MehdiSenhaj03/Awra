import type { RoomDef } from '../room';

// The Crossroads (under the Crown) and Zone 2 — Les Jardins Suspendus (screens x 2-13, y 8-12).
// Gate out: the Root Claws (wall jump) from the Root Mother open the Forge's chimney in the Crossroads.

export const JARDINS: RoomDef[] = [
  {
    // The Crossroads: west to the Gardens, a polished chimney east (claws), bars over the way down (Starstep).
    id: 'j_carrefour', zone: 'jardins', at: [14, 8], size: [3, 1], bg: 'bg_jardins_b',
    exits: { u: [[116, 121]], l: [[18, 21]], r: [[3, 6]], d: [[70, 75]] },
    solid: [
      [0, 22, 70, 5], [76, 22, 68, 5], [0, 0, 112, 3], [122, 0, 22, 1], [112, 0, 2, 2],
      [126, 7, 18, 15], [120, 5, 2, 17], [30, 15, 10, 3],
    ],
    slick: [[120, 5, 2, 17], [126, 7, 2, 15]],
    plat: [[100, 15, 6], [46, 12, 6]],
    ents: [
      ['shrine', 104, 21], ['npc', 92, 21, { who: 'orsa' }], ['tablet', 20, 21, { lore: 'l4' }],
      ['ronce', 40, 21], ['spore', 54, 10], ['acarien', 88, 21],
    ],
  },
  {
    id: 'j_lisiere', zone: 'jardins', at: [12, 8], size: [2, 1],
    exits: { l: [[18, 21]], r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 96, 3], [20, 16, 8, 6], [44, 11, 10, 3], [70, 18, 6, 4]],
    plat: [[34, 14, 6], [58, 12, 6], [80, 13, 6]],
    gloam: [[30, 21, 10, 1]],
    ents: [['ronce', 24, 15], ['spore', 40, 9], ['spore', 66, 8], ['mante', 82, 21], ['shard', 48, 10, { id: 'shard_j1' }]],
  },
  {
    id: 'j_serres', zone: 'jardins', at: [10, 8], size: [2, 1],
    exits: { l: [[18, 21]], r: [[18, 21]], d: [[40, 45]] },
    solid: [[0, 22, 40, 5], [46, 22, 50, 5], [0, 0, 96, 3], [14, 8, 3, 14], [60, 8, 3, 10], [76, 15, 12, 3]],
    plat: [[20, 15, 6], [30, 11, 6], [50, 14, 6], [66, 11, 6]],
    ents: [['tablet', 8, 21, { lore: 'l5' }], ['mante', 30, 21], ['ronce', 54, 21], ['spore', 70, 6], ['acarien', 84, 14]],
  },
  {
    // Under the greenhouses: tangled roots, a chant and star iron for those who look.
    id: 'j_racines', zone: 'jardins', at: [10, 9], size: [2, 2],
    exits: { u: [[40, 45]] },
    solid: [[0, 0, 40, 3], [46, 0, 50, 3], [0, 50, 96, 4], [0, 3, 3, 47], [93, 3, 3, 47], [20, 12, 26, 3], [56, 20, 30, 3], [10, 28, 30, 3], [50, 36, 36, 3], [40, 3, 6, 9]],
    plat: [[48, 14, 6], [30, 22, 6], [44, 30, 6], [20, 40, 6], [70, 44, 6]],
    spike: [[3, 49, 40, 1]],
    ents: [
      ['ronce', 30, 11], ['ronce', 70, 19], ['mante', 26, 27], ['spore', 66, 30], ['spore', 30, 44],
      ['ore', 84, 35, { id: 'ore_j' }], ['chant', 8, 27, { value: 'jardins', id: 'chant_j' }], ['vessel', 80, 49, { id: 'vessel_j1' }],
    ],
  },
  {
    id: 'j_saules', zone: 'jardins', at: [8, 8], size: [2, 1],
    exits: { l: [[18, 21]], r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 96, 3], [34, 14, 4, 8], [60, 10, 14, 3]],
    gloam: [[40, 21, 16, 1]],
    hole: [[40, 22, 16, 5]],
    plat: [[42, 16, 5], [50, 13, 5], [80, 15, 6]],
    ents: [['mante', 20, 21], ['spore', 46, 8], ['ronce', 70, 21], ['npc', 88, 21, { who: 'pip' }]],
  },
  {
    // The cascade: a long fall along luminous water, a rest at the bottom.
    id: 'j_cascade', zone: 'jardins', at: [7, 8], size: [1, 3],
    exits: { l: [[18, 21]], r: [[18, 21]], d: [[20, 27]] },
    solid: [[0, 0, 48, 3], [0, 22, 6, 59], [0, 3, 2, 15], [42, 22, 6, 59], [36, 22, 12, 3], [6, 34, 16, 3], [26, 46, 16, 3], [6, 58, 16, 3], [26, 70, 16, 3], [0, 78, 20, 3], [28, 78, 20, 3]],
    plat: [[24, 30, 6], [14, 42, 6], [30, 54, 6], [14, 66, 6]],
    ents: [['spore', 30, 40], ['spore', 16, 62], ['lampion', 34, 20], ['shrine', 12, 77], ['shard', 8, 33, { id: 'shard_j2' }]],
  },
  {
    id: 'j_bassin', zone: 'jardins', at: [6, 11], size: [2, 1],
    exits: { u: [[68, 75]], l: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 68, 3], [76, 0, 20, 3], [93, 3, 3, 19], [30, 12, 10, 3], [56, 15, 8, 7]],
    water: [[10, 19, 16, 3]],
    plat: [[18, 15, 6], [44, 11, 6]],
    ents: [['mante', 40, 21], ['ronce', 72, 21], ['spore', 20, 8], ['tablet', 84, 21, { lore: 'l12' }]],
  },
  {
    // The Root Mother's chamber.
    id: 'j_arene', zone: 'jardins', at: [4, 11], size: [2, 1],
    exits: { r: [[18, 21]], u: [[6, 10]] },
    solid: [[0, 22, 96, 5], [0, 0, 6, 3], [10, 0, 86, 3], [0, 3, 3, 19], [93, 3, 3, 15]],
    plat: [[20, 15, 8], [68, 15, 8], [44, 10, 8]],
    brk: [[94, 18, 2, 4, 'seal_racine'], [6, 0, 4, 2, 'seal_racine']],
    ents: [['arena', 30, 21, { boss: 'racine', trigger: [16, 80], door: 'seal_racine' }]],
  },
  {
    // A polished chimney back up to the willows: only claws climb it.
    id: 'j_cheminee', zone: 'jardins', at: [4, 8], size: [1, 3],
    exits: { d: [[6, 10]], r: [[18, 21]] },
    solid: [[0, 0, 48, 3], [0, 3, 4, 78], [14, 25, 34, 56], [36, 22, 12, 3], [4, 22, 2, 1]],
    slick: [[4, 25, 2, 56], [14, 25, 2, 56]],
    hole: [[6, 78, 4, 3]],
    ents: [['vessel', 24, 21, { id: 'vessel_j2' }], ['spore', 26, 10]],
  },
  {
    id: 'j_sentier', zone: 'jardins', at: [5, 8], size: [2, 1],
    exits: { l: [[18, 21]], r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 96, 3], [24, 12, 16, 3], [72, 16, 10, 6]],
    gloam: [[44, 21, 18, 1]],
    hole: [[44, 22, 18, 5]],
    plat: [[46, 16, 5], [55, 13, 5], [84, 14, 6]],
    ents: [['mante', 16, 21], ['ronce', 76, 15], ['spore', 52, 7], ['chevalier', 88, 21], ['seal', 30, 11, { value: 'rebond', id: 'seal_rebond' }]],
  },
];
