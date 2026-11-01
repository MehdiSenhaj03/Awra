import { DT } from '../engine/core';

/**
 * The beat clock. It advances every simulation frame, whatever happens in the game (hit-stop and slow motion
 * never bend it), so the music and the rhythm judgements always agree.
 */
export class BeatClock {
  frame = 0;
  bpm: number;
  /** Beat position at frame 0 of the current tempo. */
  private base = 0;
  private baseFrame = 0;
  private prevBeat = 0;
  /** Player calibration, in seconds (positive: the player hears/sees the beat later). */
  latency = 0;
  /** Half-width of the PERFECT window (seconds); the Metronome Overtone widens it. */
  perfectWindow = 0.07;

  constructor(bpm = 128) {
    this.bpm = bpm;
  }

  /** Beat position (fractional). */
  get beat(): number {
    return this.base + ((this.frame - this.baseFrame) * DT * this.bpm) / 60;
  }

  get spb(): number {
    return 60 / this.bpm;
  }

  tick(): void {
    this.prevBeat = this.beat;
    this.frame++;
  }

  /** Changes tempo without a jump in beat position. */
  setBpm(bpm: number): void {
    this.base = this.beat;
    this.baseFrame = this.frame;
    this.bpm = bpm;
  }

  /** True when this frame crossed a grid line of `div` subdivisions per beat (1 = beats, 2 = eighths, 4 = sixteenths). */
  crossed(div: number, offset = 0): boolean {
    return Math.floor((this.beat - offset) * div) !== Math.floor((this.prevBeat - offset) * div);
  }

  /** Index of the grid line crossed (valid when crossed() is true). */
  index(div: number): number {
    return Math.floor(this.beat * div);
  }

  /** Signed distance to the nearest beat (in seconds), from the player's point of view. */
  offsetSec(div = 1): number {
    const b = this.beat * div - (this.latency / this.spb) * div;
    const f = b - Math.round(b);
    return (f * this.spb) / div;
  }

  /** Rhythm judgement of an action pressed this frame. */
  judge(div = 1): 'perfect' | 'great' | 'off' {
    const o = Math.abs(this.offsetSec(div));
    return o <= this.perfectWindow ? 'perfect' : o <= this.perfectWindow + 0.05 ? 'great' : 'off';
  }

  /** 0 at a beat, rising to 1 just before the next one. */
  phase(div = 1): number {
    const b = this.beat * div;
    return b - Math.floor(b);
  }
}
