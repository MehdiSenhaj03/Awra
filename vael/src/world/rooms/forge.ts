import type { RoomDef } from '../room';

// Zone 3 — La Forge Éteinte (screens x 17-25, y 8-12). Reached by the polished chimney of the Crossroads (claws).
// Pools of cooled star-metal still burn (hazard). The Hollow Smith gives the Starstep.

export const FORGE: RoomDef[] = [
  {
    id: 'f_entree', zone: 'forge', at: [17, 8], size: [2, 1],
    exits: { l: [[3, 6]], r: [[18, 21]] },
    solid: [[0, 7, 12, 20], [12, 22, 84, 5], [0, 0, 96, 3], [56, 14, 12, 3]],
    plat: [[14, 12, 5], [36, 16, 6]],
    ents: [['shrine', 22, 21], ['npc', 40, 21, { who: 'vey' }], ['souffle', 74, 21], ['hint', 8, 6, { text: 'forge' }]],
  },
  {
    // The great hall of anvils.
    id: 'f_halle', zone: 'forge', at: [19, 8], size: [2, 2],
    exits: { l: [[18, 21]], r: [[44, 47]] },
    solid: [[0, 22, 30, 5], [0, 0, 96, 3], [0, 48, 96, 6], [30, 30, 8, 18], [46, 24, 20, 3], [72, 34, 24, 3], [86, 37, 10, 6], [14, 36, 12, 3], [60, 12, 14, 3]],
    gloam: [[38, 47, 30, 1]],
    plat: [[32, 18, 6], [20, 30, 6], [40, 40, 6], [52, 42, 6], [66, 30, 6], [78, 20, 6]],
    ents: [
      ['tablet', 10, 21, { lore: 'l6' }], ['enclume', 54, 23], ['souffle', 20, 35], ['etincelle', 70, 8], ['etincelle', 40, 34],
      ['souffle', 80, 33], ['shard', 66, 11, { id: 'shard_f1' }],
    ],
  },
  {
    // The crucibles: burning pools and a forge-master's ledger.
    id: 'f_creusets', zone: 'forge', at: [21, 9], size: [2, 1],
    exits: { l: [[17, 20]], r: [[18, 21]] },
    solid: [[0, 21, 14, 6], [26, 22, 12, 5], [52, 22, 10, 5], [76, 22, 20, 5], [0, 0, 96, 3], [40, 8, 8, 3]],
    gloam: [[14, 25, 12, 2], [38, 25, 14, 2], [62, 25, 14, 2]],
    hole: [[14, 21, 12, 4], [38, 22, 14, 3], [62, 22, 14, 3]],
    plat: [[18, 17, 4], [44, 17, 4], [67, 17, 5]],
    ents: [['tablet', 30, 21, { lore: 'l7' }], ['etincelle', 44, 10], ['souffle', 56, 21], ['enclume', 86, 21], ['etincelle', 70, 8]],
  },
  {
    // The well of chains, down into the pit.
    id: 'f_puits', zone: 'forge', at: [23, 9], size: [1, 3],
    exits: { l: [[18, 21]], r: [[42, 45]], d: [[20, 25]] },
    solid: [[0, 22, 14, 59], [0, 0, 48, 3], [34, 3, 14, 39], [34, 46, 14, 35], [14, 78, 6, 3], [26, 78, 8, 3], [22, 34, 12, 3], [14, 52, 10, 3], [22, 66, 12, 3]],
    plat: [[16, 28, 5], [26, 44, 5], [16, 60, 5], [24, 72, 5]],
    ents: [['etincelle', 24, 30], ['souffle', 26, 33], ['etincelle', 20, 58], ['vessel', 30, 65, { id: 'vessel_f1' }]],
  },
  {
    // A sealed store: an eye behind bars opens it. The Ascension waits inside.
    id: 'f_reserve', zone: 'forge', at: [24, 10], size: [2, 1],
    exits: { l: [[15, 18]] },
    solid: [[0, 19, 30, 8], [30, 22, 66, 5], [0, 0, 96, 3], [93, 3, 3, 19], [36, 3, 2, 12], [60, 10, 20, 3]],
    grate: [[36, 15, 2, 7]],
    brk: [[46, 12, 2, 10, 'gate_f1']],
    plat: [[66, 16, 6]],
    ents: [
      ['eye', 40, 8, { flag: 'gate_f1' }], ['art', 84, 21, { value: 'ascension', id: 'art_ascension' }], ['ore', 70, 9, { id: 'ore_f' }],
      ['chant', 88, 21, { value: 'forge', id: 'chant_f' }], ['enclume', 74, 21],
    ],
  },
  {
    id: 'f_fosse', zone: 'forge', at: [22, 12], size: [2, 1],
    exits: { u: [[68, 73]], l: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 68, 3], [74, 0, 22, 3], [93, 3, 3, 19], [62, 12, 6, 10], [40, 16, 8, 6]],
    plat: [[70, 14, 6], [52, 11, 6]],
    ents: [['npc', 30, 21, { who: 'veilleur' }], ['shrine', 18, 21], ['enclume', 80, 21]],
  },
  {
    // The Hollow Smith's forge.
    id: 'f_arene', zone: 'forge', at: [20, 12], size: [2, 1],
    exits: { r: [[18, 21]] },
    solid: [[0, 22, 96, 5], [0, 0, 96, 3], [0, 3, 3, 19], [93, 3, 3, 15]],
    gloam: [[3, 21, 6, 1], [87, 21, 6, 1]],
    plat: [[18, 15, 8], [70, 15, 8], [44, 10, 8]],
    brk: [[94, 18, 2, 4, 'seal_forgeron']],
    ents: [['arena', 40, 21, { boss: 'forgeron', trigger: [14, 84], door: 'seal_forgeron' }]],
  },
];
