import { CELL_H, CELL_W, TILE } from '../engine/core';

/** Tile kinds of the collision grid. Visuals are painted over it, never drawn tile by tile. */
export const enum T {
  Empty = 0,
  Solid = 1,
  /** One-way platform: solid from above only. */
  Platform = 2,
  /** Thorns of crystal: hurt and send back to the last safe ground. */
  Spike = 3,
  /** Cracked wall or floor, opened by a flag (struck, or by a power). */
  Breakable = 4,
  /** Pool of Gloam: the corrupted ink; same as spikes. */
  Gloam = 5,
  /** Still water: slows, no harm. */
  Water = 6,
  /** Iron grate: blocks bodies, lets the thrown blade through. */
  Grate = 7,
  /** Polished rock: solid, but the blade cannot bite into it (bounces off). */
  Slick = 8,
}

export const isSolid = (t: number) => t === T.Solid || t === T.Breakable || t === T.Grate || t === T.Slick;
/** Solid for the flying blade (grates let it through). */
export const bladeSolid = (t: number) => t === T.Solid || t === T.Breakable || t === T.Slick;
/** Drawn as rock by the terrain shader. */
export const rockTile = (t: number) => t === T.Solid || t === T.Breakable || t === T.Slick;
export const isHazard = (t: number) => t === T.Spike || t === T.Gloam;

export type Rect = [x: number, y: number, w: number, h: number];
export type Span = [from: number, to: number];
export type EntDef = [kind: string, x: number, y: number, opts?: Record<string, unknown>];

/** A room is authored in tiles; one screen is 48×27 tiles. */
export interface RoomDef {
  id: string;
  zone: string;
  /** Position on the world map, in screens. */
  at: [number, number];
  /** Size in screens (default 1×1). */
  size?: [number, number];
  /** Openings to the neighbouring rooms, in tiles along the edge. */
  exits?: { l?: Span[]; r?: Span[]; u?: Span[]; d?: Span[] };
  solid?: Rect[];
  /** Carved out of the solids. */
  hole?: Rect[];
  plat?: [x: number, y: number, w: number][];
  spike?: Rect[];
  gloam?: Rect[];
  water?: Rect[];
  /**
   * Walls that open with a flag: [x, y, w, h, flag]. The flag's prefix sets the look and how it opens:
   * crack_ (struck), gate_ (iron bars, a switch), crys_ (black crystal, Comet Wake), seal_ (boss door).
   */
  brk?: [number, number, number, number, string][];
  grate?: Rect[];
  /** Polished rock the blade bounces off (applied over solids). */
  slick?: Rect[];
  ents?: EntDef[];
  /** Background variant, decor seed, darkness. */
  bg?: string;
  dark?: boolean;
  title?: boolean;
  music?: string;
}

export interface Room {
  def: RoomDef;
  id: string;
  /** Size in tiles. */
  w: number;
  h: number;
  tiles: Uint8Array;
  pxW: number;
  pxH: number;
}

export function tileAt(room: Room, tx: number, ty: number): number {
  if (tx < 0 || tx >= room.w || ty < 0 || ty >= room.h) {
    // Outside: open where an exit is, otherwise polished rock (the blade never bites into a room's border).
    return outsideOpen(room, tx, ty) ? T.Empty : T.Slick;
  }
  return room.tiles[ty * room.w + tx];
}

function inSpans(spans: Span[] | undefined, v: number): boolean {
  return !!spans?.some(([a, b]) => v >= a && v <= b);
}

function outsideOpen(room: Room, tx: number, ty: number): boolean {
  const e = room.def.exits ?? {};
  if (tx < 0 && ty >= 0 && ty < room.h) return inSpans(e.l, ty);
  if (tx >= room.w && ty >= 0 && ty < room.h) return inSpans(e.r, ty);
  if (ty < 0 && tx >= 0 && tx < room.w) return inSpans(e.u, tx);
  if (ty >= room.h && tx >= 0 && tx < room.w) return inSpans(e.d, tx);
  return false;
}

export function setTile(room: Room, tx: number, ty: number, t: T): void {
  if (tx < 0 || tx >= room.w || ty < 0 || ty >= room.h) return;
  room.tiles[ty * room.w + tx] = t;
}

export function buildRoom(def: RoomDef, flags: Record<string, boolean | number>): Room {
  const [sw, sh] = def.size ?? [1, 1];
  const w = sw * CELL_W;
  const h = sh * CELL_H;
  const tiles = new Uint8Array(w * h);
  const room: Room = { def, id: def.id, w, h, tiles, pxW: w * TILE, pxH: h * TILE };
  const fill = (r: Rect, t: T) => {
    for (let y = r[1]; y < r[1] + r[3]; y++) for (let x = r[0]; x < r[0] + r[2]; x++) setTile(room, x, y, t);
  };
  for (const r of def.solid ?? []) fill(r, T.Solid);
  for (const r of def.hole ?? []) fill(r, T.Empty);
  // Exits are carved through anything placed on the border.
  const e = def.exits ?? {};
  for (const [a, b] of e.l ?? []) fill([0, a, 1, b - a + 1], T.Empty);
  for (const [a, b] of e.r ?? []) fill([w - 1, a, 1, b - a + 1], T.Empty);
  for (const [a, b] of e.u ?? []) fill([a, 0, b - a + 1, 1], T.Empty);
  for (const [a, b] of e.d ?? []) fill([a, h - 1, b - a + 1, 1], T.Empty);
  for (const [x, y, pw] of def.plat ?? []) fill([x, y, pw, 1], T.Platform);
  for (const r of def.spike ?? []) fill(r, T.Spike);
  for (const r of def.gloam ?? []) fill(r, T.Gloam);
  for (const r of def.water ?? []) fill(r, T.Water);
  for (const r of def.grate ?? []) fill(r, T.Grate);
  // Polish only what is already rock.
  for (const r of def.slick ?? []) {
    for (let y = r[1]; y < r[1] + r[3]; y++) for (let x = r[0]; x < r[0] + r[2]; x++) if (tileAt(room, x, y) === T.Solid && x >= 0 && y >= 0 && x < w && y < h) setTile(room, x, y, T.Slick);
  }
  for (const [x, y, bw, bh, flag] of def.brk ?? []) fill([x, y, bw, bh], flags[flag] ? T.Empty : T.Breakable);
  return room;
}

export const tileCenter = (t: number) => t * TILE + TILE / 2;
