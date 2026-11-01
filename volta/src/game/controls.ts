/** What the player asks for this frame (built from devices, or by tests). */
export interface Controls {
  mx: number;
  my: number;
  /** Aim direction (unit vector, world space). */
  ax: number;
  ay: number;
  fire: boolean;
  firePressed: boolean;
  dash: boolean;
  pulse: boolean;
  overdrive: boolean;
}

export const IDLE: Controls = { mx: 0, my: 0, ax: 1, ay: 0, fire: false, firePressed: false, dash: false, pulse: false, overdrive: false };
