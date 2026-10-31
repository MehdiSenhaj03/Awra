import type { RoomDef } from '../room';

// Zone 5 — Le Bastion des Chaînes (screens x 3-8, y 13-17). The bridge needs the Second Breath: a polished block
// hangs too high over the thorns for a single jump. The Jailers give the Tether.

export const BASTION: RoomDef[] = [
  {
    id: 'b_pont', zone: 'bastion', at: [7, 13], size: [2, 1],
    exits: { r: [[18, 21]], l: [[8, 11]] },
    solid: [[0, 12, 18, 15], [50, 22, 46, 5], [0, 0, 96, 3], [28, 8, 14, 1], [18, 26, 32, 1]],
    slick: [[0, 0, 96, 3], [28, 8, 14, 1]],
    spike: [[18, 25, 32, 1]],
    ents: [['corbeau', 40, 18], ['geolier', 70, 21], ['hint', 60, 21, { text: 'double' }]],
  },
  {
    id: 'b_cellules', zone: 'bastion', at: [5, 13], size: [2, 2],
    exits: { r: [[8, 11]], l: [[18, 21]], d: [[20, 25]] },
    solid: [[0, 22, 96, 3], [66, 12, 30, 3], [0, 0, 96, 3], [0, 48, 20, 6], [26, 48, 70, 6], [40, 34, 56, 3], [0, 36, 30, 3], [56, 25, 4, 9], [20, 25, 4, 11]],
    hole: [[20, 22, 6, 3]],
    plat: [[44, 16, 6], [30, 30, 5], [70, 42, 6]],
    ents: [
      ['npc', 80, 11, { who: 'lunelle' }], ['tablet', 34, 21, { lore: 'l10' }], ['tablet', 70, 33, { lore: 'l11' }],
      ['geolier', 54, 21], ['corbeau', 70, 28], ['geolier', 50, 47], ['shrine', 88, 11],
    ],
  },
  {
    // The Grey Warden waits at the top of the keep, if you want to cross blades.
    id: 'b_sommet', zone: 'bastion', at: [3, 13], size: [2, 1],
    exits: { r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 96, 3], [0, 3, 3, 19]],
    plat: [[20, 15, 8], [62, 15, 8]],
    brk: [[94, 17, 2, 5, 'seal_veilleur']],
    ents: [['arena', 30, 21, { boss: 'veilleur', trigger: [10, 80], door: 'seal_veilleur' }]],
  },
  {
    id: 'b_cages', zone: 'bastion', at: [5, 15], size: [2, 2],
    exits: { u: [[20, 25]], l: [[17, 20], [44, 47]] },
    solid: [[0, 0, 20, 3], [26, 0, 70, 3], [0, 21, 30, 3], [0, 48, 96, 6], [93, 3, 3, 45], [40, 14, 30, 3], [60, 30, 33, 3], [10, 36, 30, 3]],
    gloam: [[30, 47, 30, 1]],
    plat: [[30, 26, 6], [76, 22, 6], [46, 40, 6]],
    ents: [['corbeau', 50, 8], ['corbeau', 20, 30], ['geolier', 76, 29], ['chevalier', 24, 35], ['vessel', 84, 47, { id: 'vessel_b1' }]],
  },
  {
    // The oubliettes, behind a cracked wall: forgotten prisoners, a little light.
    id: 'b_oubliettes', zone: 'bastion', at: [3, 14], size: [2, 2],
    exits: { r: [[44, 47]] },
    solid: [[0, 0, 96, 3], [0, 48, 96, 6], [0, 3, 3, 45], [84, 3, 12, 41], [10, 36, 30, 3], [50, 26, 30, 3], [10, 16, 30, 3]],
    brk: [[84, 44, 4, 4, 'crack_b1']],
    plat: [[44, 40, 5], [40, 30, 5], [44, 20, 5]],
    ents: [
      ['crack', 86, 47, { flag: 'crack_b1', w: 4, h: 4 }], ['art', 20, 15, { value: 'comete2', id: 'art_comete2' }], ['chant', 70, 25, { value: 'bastion', id: 'chant_b' }],
      ['npc', 20, 35, { who: 'pip' }], ['chevalier', 60, 47], ['corbeau', 30, 26],
    ],
  },
  {
    id: 'b_treuil', zone: 'bastion', at: [3, 16], size: [2, 1],
    exits: { r: [[17, 20]], d: [[10, 15]] },
    solid: [[0, 22, 10, 5], [16, 22, 80, 5], [0, 0, 96, 3], [0, 3, 3, 19], [40, 10, 6, 12], [60, 14, 20, 3]],
    plat: [[26, 15, 6], [50, 12, 6]],
    ents: [['shrine', 84, 20], ['geolier', 30, 21], ['corbeau', 66, 8], ['shard', 70, 13, { id: 'shard_b1' }]],
  },
  {
    // The Jailers' yard.
    id: 'b_arene', zone: 'bastion', at: [3, 17], size: [2, 1],
    exits: { u: [[10, 15]] },
    solid: [[0, 22, 96, 5], [0, 0, 10, 3], [16, 0, 80, 3], [0, 3, 3, 19], [93, 3, 3, 19]],
    plat: [[22, 15, 8], [66, 15, 8], [44, 10, 8]],
    brk: [[10, 0, 6, 2, 'seal_geoliers']],
    ents: [['arena', 50, 21, { boss: 'geoliers', trigger: [10, 86], door: 'seal_geoliers' }]],
  },
];
