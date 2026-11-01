import type { Enemy } from './enemy';
import type { World } from './world';

/** Conductors (bosses), registered by id. Filled by bosses/*.ts. */
export const BOSSES: Record<string, (w: World) => Enemy> = {};
