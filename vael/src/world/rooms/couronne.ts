import type { RoomDef } from '../room';

// Zone 1 — La Couronne (screens x 8-16, y 4-7). Teaches each move in turn, ends with the Broken Warden.
// One screen = 48×27 tiles of 40 px. Hero: about 1×2 tiles; a full jump rises ~11 tiles, runs ~11 tiles across.

export const COURONNE: RoomDef[] = [
  {
    // The spark wakes in a cocoon of crystal. Walk, jump.
    id: 'c_reveil', zone: 'couronne', at: [8, 5], title: false,
    exits: { r: [[18, 21]] },
    solid: [[0, 22, 48, 5], [0, 0, 4, 27], [0, 0, 48, 3], [20, 19, 6, 3], [30, 17, 5, 5], [40, 0, 8, 12]],
    ents: [
      ['shrine', 10, 21], ['tablet', 16, 21, { lore: 'l16' }], ['hint', 22, 18, { text: 'move' }],
    ],
  },
  {
    // A long gallery: first foes, first pit of thorns, a cracked wall hiding a life shard.
    id: 'c_galerie', zone: 'couronne', at: [9, 5], size: [2, 1],
    exits: { l: [[18, 21]], r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 96, 4], [44, 16, 10, 6], [60, 12, 20, 4], [78, 16, 2, 6]],
    hole: [[30, 22, 8, 5]],
    spike: [[30, 26, 8, 1]],
    brk: [[60, 16, 2, 6, 'crack_c1']],
    ents: [
      ['hint', 6, 21, { text: 'attack' }], ['acarien', 20, 21], ['acarien', 50, 15], ['coquille', 88, 21],
      ['tablet', 40, 21, { lore: 'l3' }], ['crack', 61, 21, { flag: 'crack_c1', w: 2, h: 6 }], ['shard', 70, 21, { id: 'shard_c1' }],
    ],
  },
  {
    // The shaft: too high to jump. Throw the blade into the wall while rising, land on it, go higher.
    id: 'c_puits', zone: 'couronne', at: [11, 4], size: [1, 2],
    exits: { l: [[45, 48]], r: [[6, 9]] },
    solid: [[0, 49, 48, 5], [0, 0, 6, 45], [42, 10, 6, 44], [26, 37, 16, 3], [26, 40, 2, 9], [6, 28, 16, 3], [26, 19, 12, 3], [30, 10, 12, 3], [36, 0, 12, 6]],
    plat: [[18, 13, 5]],
    ents: [
      ['hint', 16, 48, { text: 'blade' }], ['lampion', 16, 20], ['vessel', 9, 27, { id: 'vessel_c1' }], ['acarien', 34, 36],
    ],
  },
  {
    // The great nave: the Cartographer, two frescoes, husk soldiers and lanterns.
    id: 'c_nef', zone: 'couronne', at: [12, 4], size: [2, 2],
    exits: { l: [[6, 9]], r: [[40, 43]], d: [[20, 24]] },
    solid: [[0, 10, 30, 3], [0, 0, 96, 3], [0, 44, 96, 10], [36, 10, 6, 12], [56, 18, 12, 3], [76, 26, 20, 3], [90, 3, 6, 23], [10, 30, 10, 3], [30, 36, 8, 8]],
    hole: [[20, 44, 5, 10]],
    plat: [[44, 22, 8], [22, 26, 6], [62, 34, 8], [80, 36, 6]],
    ents: [
      ['npc', 14, 9, { who: 'orsa' }], ['tablet', 26, 9, { lore: 'l1' }], ['tablet', 70, 43, { lore: 'l2' }],
      ['coquille', 50, 43], ['coquille', 84, 25], ['lampion', 60, 12], ['lampion', 30, 30], ['acarien', 14, 29],
      ['map', 4, 9, { value: 'couronne', id: 'map_couronne_free' }],
    ],
  },
  {
    // The crypt under the nave: a rest before the keeper, Sister Lunelle.
    id: 'c_crypte', zone: 'couronne', at: [12, 6],
    exits: { u: [[20, 24]], r: [[18, 21]] },
    solid: [[0, 22, 48, 5], [0, 0, 20, 4], [25, 0, 23, 4], [0, 0, 4, 22], [30, 14, 18, 3]],
    plat: [[16, 12, 6], [8, 17, 6]],
    ents: [['shrine', 12, 21], ['npc', 24, 21, { who: 'lunelle' }], ['tablet', 36, 21, { lore: 'l15' }]],
  },
  {
    // The watcher's tower: the Grey Warden on his ledge, a vial fragment for climbers.
    id: 'c_tour', zone: 'couronne', at: [13, 6],
    exits: { l: [[18, 21]], r: [[18, 21]] },
    solid: [[0, 22, 48, 5], [0, 0, 48, 3], [18, 9, 12, 3], [40, 3, 8, 9], [4, 3, 4, 12]],
    plat: [[10, 15, 5], [33, 15, 5]],
    ents: [['npc', 24, 8, { who: 'veilleur' }], ['coquille', 36, 21], ['shard', 44, 14, { id: 'shard_c2' }], ['chant', 27, 8, { value: 'couronne', id: 'chant_c' }]],
  },
  {
    // The choir: an iron gate; through the bars beside it, a star eye that opens it. Thorns, then the dash gap.
    id: 'c_choeur', zone: 'couronne', at: [14, 5], size: [2, 1],
    exits: { l: [[13, 16]], r: [[18, 21]], d: [[80, 85]] },
    solid: [[0, 17, 30, 10], [0, 0, 96, 3], [30, 22, 32, 5], [62, 22, 18, 5], [86, 22, 10, 5], [62, 3, 2, 7]],
    spike: [[36, 21, 7, 1], [47, 21, 6, 1]],
    grate: [[62, 10, 2, 4]],
    brk: [[62, 14, 2, 8, 'gate_c1']],
    plat: [[40, 16, 5], [54, 14, 5], [88, 15, 5]],
    ents: [
      ['eye', 68, 13, { flag: 'gate_c1' }], ['hint', 56, 13, { text: 'eye' }], ['lampion', 46, 8], ['coquille', 34, 21], ['acarien', 72, 21], ['coquille', 90, 21],
    ],
  },
  {
    // The keeper's hall.
    id: 'c_arene', zone: 'couronne', at: [14, 6], size: [2, 1],
    exits: { l: [[18, 21]], u: [[80, 85]], r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 76, 3], [86, 0, 10, 3], [0, 3, 3, 15], [93, 3, 3, 15]],
    plat: [[20, 15, 8], [68, 15, 8]],
    brk: [[0, 18, 2, 4, 'seal_gardien'], [94, 18, 2, 4, 'seal_gardien']],
    ents: [['arena', 56, 21, { boss: 'gardien', trigger: [12, 84], door: 'seal_gardien' }]],
  },
  {
    // The windows of the Crown: a thorn gap only the Wake can cross; the first art.
    id: 'c_vitraux', zone: 'couronne', at: [16, 5],
    exits: { l: [[18, 21]], u: [[20, 25]] },
    solid: [[0, 22, 12, 5], [28, 22, 20, 5], [0, 0, 20, 3], [26, 0, 22, 3], [44, 3, 4, 19], [12, 26, 16, 1]],
    slick: [[18, 0, 2, 3], [26, 0, 2, 3], [44, 3, 2, 19]],
    spike: [[12, 25, 16, 1]],
    ents: [['art', 38, 21, { value: 'comete', id: 'art_comete' }], ['tablet', 32, 21, { lore: 'l14' }], ['lampion', 20, 12], ['ring', 22, 15], ['ring', 22, 6], ['hint', 22, 21, { text: 'ring' }]],
  },
  {
    // The way down, past the keeper: towards the Gardens and the Forge.
    // A thorn chasm only the Wake can cross, then the long way down.
    id: 'c_sortie', zone: 'couronne', at: [16, 6], size: [1, 2],
    exits: { l: [[18, 21]], d: [[20, 25]] },
    solid: [[0, 22, 12, 5], [28, 22, 20, 5], [0, 0, 48, 3], [0, 49, 20, 5], [26, 49, 22, 5], [44, 3, 4, 19], [0, 27, 14, 22], [34, 27, 14, 22], [12, 26, 16, 1]],
    slick: [[0, 3, 2, 19], [12, 23, 16, 4]],
    hole: [[30, 22, 4, 5]],
    spike: [[12, 25, 16, 1]],
    plat: [[16, 32, 5], [24, 38, 5], [16, 44, 5]],
    ents: [['shrine', 36, 21], ['lampion', 24, 34], ['acarien', 30, 21]],
  },
];
