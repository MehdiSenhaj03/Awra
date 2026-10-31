/** Visual identity of each zone: paintings, terrain colours, light, fog and decor. */
export interface ZoneLook {
  bgs: string[];
  tex: string;
  rim: [number, number, number];
  deep: [number, number, number];
  top: [number, number, number];
  ambient: number;
  fog: number;
  /** Tint applied to the far painting (darkens it behind the action). */
  bgTint: number;
  mid: string[];
  back: string[];
  /** Props hanging from ceilings. */
  hang?: string[];
  /** Props that glow (lights placed on them). */
  glow?: string[];
  fg: string[];
  plat: string;
  edge: string;
  motes: number;
  hero: number;
  music: string;
  /** Colour of the hazardous pools (ink, molten metal). */
  pool: number;
  poolGlow: number;
}

const C = (r: number, g: number, b: number): [number, number, number] => [r / 255, g / 255, b / 255];

export const ZONES: Record<string, ZoneLook> = {
  couronne: {
    bgs: ['bg_couronne_a', 'bg_couronne_b', 'bg_couronne_c'], tex: 'tex_couronne',
    rim: C(110, 135, 210), deep: C(6, 7, 16), top: C(255, 214, 140), ambient: 0xc4cae6, fog: 0x1a2140, bgTint: 0x8a94bc,
    mid: ['prop_pillar', 'prop_arch', 'prop_statue'], back: ['prop_arch', 'prop_crystals', 'prop_statue', 'prop_candles', 'prop_pillar'],
    hang: ['prop_lamp', 'prop_banner', 'prop_chain'], glow: ['prop_lamp', 'prop_candles', 'prop_crystals'],
    fg: ['fg_couronne_a', 'fg_couronne_b'], plat: 'plat_couronne', edge: 'edge_top', motes: 0xffe2a8, hero: 0xffd89a, music: 'couronne', pool: 0x1a0626, poolGlow: 0xa35cff,
  },
  jardins: {
    bgs: ['bg_jardins_a', 'bg_jardins_b'], tex: 'tex_jardins',
    rim: C(80, 200, 190), deep: C(3, 12, 12), top: C(150, 255, 220), ambient: 0xb8dcd8, fog: 0x0c2a2c, bgTint: 0x88b8b4,
    mid: ['prop_tree', 'prop_greenhouse'], back: ['prop_flower', 'prop_mushrooms', 'prop_tree', 'prop_greenhouse'],
    hang: ['prop_roots'], glow: ['prop_flower', 'prop_mushrooms'],
    fg: ['fg_jardins_a', 'fg_jardins_b'], plat: 'plat_jardins', edge: 'edge_top', motes: 0x9ff8e0, hero: 0xffd89a, music: 'jardins', pool: 0x1a0626, poolGlow: 0xa35cff,
  },
  forge: {
    bgs: ['bg_forge_a', 'bg_forge_b'], tex: 'tex_forge',
    rim: C(200, 120, 80), deep: C(10, 5, 4), top: C(255, 170, 90), ambient: 0xd8b8a8, fog: 0x2a1208, bgTint: 0xa88878,
    mid: ['prop_forge_3', 'prop_forge_4'], back: ['prop_forge_0', 'prop_forge_2', 'prop_forge_3', 'prop_forge_4'],
    hang: ['prop_forge_1'], glow: ['prop_forge_2'],
    fg: ['fg_forge_a'], plat: 'plat_forge', edge: 'edge_forge', motes: 0xffa060, hero: 0xffd89a, music: 'forge', pool: 0x2a0800, poolGlow: 0xff7a20,
  },
  archives: {
    bgs: ['bg_archives_a', 'bg_archives_b'], tex: 'tex_archives',
    rim: C(90, 170, 190), deep: C(3, 9, 12), top: C(255, 210, 150), ambient: 0xa8c8d0, fog: 0x08202a, bgTint: 0x80a8b0,
    mid: ['prop_archives_0', 'prop_archives_3'], back: ['prop_archives_0', 'prop_archives_1', 'prop_archives_2', 'prop_archives_3'],
    hang: ['prop_archives_4'], glow: ['prop_archives_1', 'prop_archives_4'],
    fg: ['fg_archives_a'], plat: 'plat_archives', edge: 'edge_archives', motes: 0xa0f0ff, hero: 0xffd89a, music: 'archives', pool: 0x10061e, poolGlow: 0x8a5cff,
  },
  bastion: {
    bgs: ['bg_bastion_a', 'bg_bastion_b'], tex: 'tex_bastion',
    rim: C(160, 150, 190), deep: C(6, 6, 10), top: C(230, 230, 255), ambient: 0xbcb8d0, fog: 0x1a1828, bgTint: 0x9894b0,
    mid: ['prop_bastion_1'], back: ['prop_bastion_2', 'prop_bastion_3', 'prop_bastion_4'],
    hang: ['prop_bastion_0'],
    fg: ['fg_bastion_a'], plat: 'plat_bastion', edge: 'edge_bastion', motes: 0xe0e0ff, hero: 0xffd89a, music: 'bastion', pool: 0x1a0626, poolGlow: 0xa35cff,
  },
  firmament: {
    bgs: ['bg_firmament_a', 'bg_firmament_b'], tex: 'tex_firmament',
    rim: C(220, 140, 220), deep: C(12, 6, 20), top: C(255, 200, 240), ambient: 0xd8c0e0, fog: 0x2a1238, bgTint: 0xb8a0c8,
    mid: ['prop_firmament_0', 'prop_firmament_1', 'prop_firmament_4'], back: ['prop_firmament_1', 'prop_firmament_2', 'prop_firmament_3', 'prop_firmament_4'],
    fg: ['fg_firmament_a'], plat: 'plat_firmament', edge: 'edge_firmament', motes: 0xffb0f0, hero: 0xffd89a, music: 'firmament', pool: 0x200838, poolGlow: 0xff70d0,
  },
  penombre: {
    bgs: ['bg_penombre_a', 'bg_penombre_b'], tex: 'tex_penombre',
    rim: C(150, 80, 200), deep: C(4, 2, 8), top: C(220, 120, 255), ambient: 0x9c88b4, fog: 0x14061e, bgTint: 0x80689c,
    mid: ['prop_penombre_2', 'prop_penombre_3'], back: ['prop_penombre_0', 'prop_penombre_1', 'prop_penombre_2', 'prop_penombre_4'],
    glow: ['prop_penombre_0'],
    fg: ['fg_penombre_a'], plat: 'plat_penombre', edge: 'edge_penombre', motes: 0xd080ff, hero: 0xffd89a, music: 'penombre', pool: 0x14001e, poolGlow: 0xc040ff,
  },
  coeur: {
    bgs: ['bg_coeur_a', 'bg_coeur_b'], tex: 'tex_coeur',
    rim: C(230, 210, 160), deep: C(14, 12, 10), top: C(255, 250, 220), ambient: 0xe8e0d0, fog: 0x2a2418, bgTint: 0xc8c0b0,
    mid: ['prop_coeur_1', 'prop_coeur_4'], back: ['prop_coeur_0', 'prop_coeur_2', 'prop_coeur_4'],
    hang: ['prop_coeur_3'], glow: ['prop_coeur_0', 'prop_coeur_3'],
    fg: ['fg_coeur_a'], plat: 'plat_coeur', edge: 'edge_coeur', motes: 0xfff4dc, hero: 0xffe8b0, music: 'coeur', pool: 0x1a0626, poolGlow: 0xa35cff,
  },
};

export function zoneLook(z: string): ZoneLook {
  return ZONES[z] ?? ZONES.couronne;
}
