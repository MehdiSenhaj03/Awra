import { audio } from './audio';
import { V } from './synth';

/**
 * Generative music locked to the game's beat clock. A theme gives a key, a chord progression and patterns; the
 * intensity (0..4) adds layers. The hero's actions play notes from the current chord, quantised to the grid.
 */
export interface Theme {
  id: string;
  bpm: number;
  root: number;
  /** Chords, as semitone offsets from the root, one per bar. */
  prog: number[][];
  drums: 'four' | 'break' | 'half' | 'industrial' | 'none';
  /** 16 steps: 0 rest, 1 root, 2 octave, 3 fifth. */
  bass: number[];
  arp: 'up' | 'updown' | 'pulse' | 'random';
  /** Lead motif over two bars (32 steps): chord-tone index, -1 rest. */
  lead: number[];
  pad: number;
  drive?: number;
}

const four = [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0];
const L = (s: string) => s.split('').map((c) => (c === '.' ? -1 : parseInt(c, 36)));

export const THEMES: Record<string, Theme> = {
  title: { id: 'title', bpm: 96, root: 50, prog: [[0, 3, 7, 10], [-4, 0, 3, 7], [-7, -4, 0, 3], [-2, 2, 5, 9]], drums: 'none', bass: [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], arp: 'updown', lead: L('................................'), pad: 1600 },
  seuil: { id: 'seuil', bpm: 128, root: 50, prog: [[0, 3, 7], [-4, 0, 3], [3, 7, 10], [-2, 2, 5]], drums: 'four', bass: [1, 0, 2, 0, 1, 0, 2, 1, 1, 0, 2, 0, 1, 0, 3, 2], arp: 'up', lead: L('0..2..1.0...4...2..3..2.1...0...'), pad: 1800 },
  jardin: { id: 'jardin', bpm: 136, root: 52, prog: [[0, 3, 7], [5, 8, 12], [-4, 0, 3], [-5, -2, 2]], drums: 'half', bass: [1, 0, 0, 1, 0, 0, 2, 0, 1, 0, 0, 1, 0, 3, 0, 0], arp: 'updown', lead: L('2...1...0.1.2...4...3...2.1.0...'), pad: 2400 },
  fonderie: { id: 'fonderie', bpm: 140, root: 48, prog: [[0, 3, 7], [0, 3, 7], [-4, 0, 3], [-2, 2, 5]], drums: 'industrial', bass: [1, 1, 0, 1, 1, 0, 2, 0, 1, 1, 0, 1, 3, 0, 2, 0], arp: 'pulse', lead: L('0.0.2.....3.2...0.0.2.....4.3...'), pad: 1200, drive: 0.8 },
  ocean: { id: 'ocean', bpm: 118, root: 54, prog: [[0, 3, 7], [-4, 0, 3], [5, 8, 12], [7, 11, 14]], drums: 'half', bass: [1, 0, 0, 0, 0, 0, 3, 0, 2, 0, 0, 0, 1, 0, 0, 0], arp: 'random', lead: L('4.......3...2...1.......0...1...'), pad: 2000 },
  ville: { id: 'ville', bpm: 150, root: 45, prog: [[0, 3, 7], [3, 7, 10], [-2, 2, 5], [-4, 0, 3]], drums: 'break', bass: [1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 2, 0, 1, 0, 3, 0], arp: 'up', lead: L('0.2.3.4...3.2...0.2.3.6...4.3...'), pad: 2600, drive: 0.4 },
  coeur: { id: 'coeur', bpm: 160, root: 47, prog: [[0, 3, 7], [-4, 0, 3], [-7, -4, 0], [-5, -1, 2]], drums: 'break', bass: [1, 0, 2, 0, 1, 2, 0, 1, 1, 0, 2, 0, 1, 3, 2, 0], arp: 'updown', lead: L('4.3.2.0.2...3...4.6.4.3.2...0...'), pad: 3000, drive: 0.6 },
};

export class Music {
  theme: Theme | null = null;
  intensity = 2;
  boss = false;
  overdrive = false;
  /** Silence factor for the final fight (0 = normal, 1 = only the hero plays). */
  silence = 0;
  private next = 0;
  private offset = 0;
  private anchored = false;
  private noteI = 0;
  private bus: GainNode | null = null;

  start(theme: string): void {
    this.theme = THEMES[theme] ?? THEMES.seuil;
    this.anchored = false;
    this.next = -1;
    if (audio.ctx) {
      this.bus?.disconnect();
      this.bus = audio.ctx.createGain();
      this.bus.gain.value = 1;
      this.bus.connect(audio.music);
    }
  }

  stop(fade = 0.6): void {
    if (this.bus && audio.ctx) {
      this.bus.gain.setTargetAtTime(0.0001, audio.now, fade / 3);
      const b = this.bus;
      setTimeout(() => b.disconnect(), fade * 1000 + 200);
    }
    this.bus = null;
    this.theme = null;
  }

  private chord(step: number): number[] {
    const th = this.theme!;
    const bar = Math.floor(step / 16);
    return th.prog[((bar % th.prog.length) + th.prog.length) % th.prog.length].map((n) => n + th.root);
  }

  /** Audio time of a beat position. */
  private at(beat: number, spb: number): number {
    return this.offset + beat * spb;
  }

  /** Schedules everything up to a little ahead of the sim's beat. Call every frame. */
  update(simBeat: number, bpm: number): void {
    const ctx = audio.ctx;
    if (!ctx || !this.theme || !this.bus) return;
    const spb = 60 / bpm;
    const target = audio.now - audio.latency + 0.01 - simBeat * spb;
    if (!this.anchored || Math.abs(target - this.offset) > 0.06) {
      this.offset = target;
      this.anchored = true;
    } else this.offset += (target - this.offset) * 0.03;
    const horizon = audio.now + 0.14;
    if (this.next < 0) this.next = Math.ceil(simBeat * 4);
    while (this.at(this.next / 4, spb) < horizon) {
      const t = this.at(this.next / 4, spb);
      if (t >= audio.now - 0.005) this.step(this.next, Math.max(audio.now, t), spb);
      this.next++;
    }
  }

  private step(s: number, t: number, spb: number): void {
    const th = this.theme!;
    const o = { dest: this.bus! };
    const I = this.overdrive ? 4 : this.intensity;
    const i = ((s % 16) + 16) % 16;
    const bar = Math.floor(s / 16);
    const ch = this.chord(s);
    const quiet = 1 - this.silence;
    const step = spb / 4;
    // Pads.
    if (i === 0 && I >= 0) V.pad(t, ch.map((n) => n + 12), spb * 4, o, (I === 0 ? 1.2 : 0.8) * quiet, th.pad * (this.overdrive ? 2 : 1));
    if (quiet < 0.1) return;
    // Drums.
    if (I >= 1 && th.drums !== 'none') {
      const d = th.drums;
      let kick = false;
      let snare = false;
      if (d === 'four') {
        kick = four[i] === 1;
        snare = i === 4 || i === 12;
      } else if (d === 'half') {
        kick = i === 0 || i === 6 || (I >= 3 && i === 10);
        snare = i === 8;
      } else if (d === 'break') {
        kick = i === 0 || i === 10 || (I >= 3 && i === 3);
        snare = i === 4 || i === 12 || (I >= 4 && i === 15);
      } else if (d === 'industrial') {
        kick = four[i] === 1 || (I >= 3 && i === 14);
        snare = i === 4 || i === 12;
      }
      if (kick) V.kick(t, o, (I === 1 ? 0.7 : 1) * quiet);
      if (snare && I >= 2) V.snare(t, o, 0.8 * quiet, d === 'industrial' ? 160 : 200);
      if (I >= 2 && i % 4 === 2) V.hat(t, o, 0.9 * quiet, i === 14 && bar % 2 === 1);
      if (I >= 3 && i % 2 === 1) V.hat(t, o, 0.35 * quiet);
      if (I >= 4 && bar % 4 === 3 && i >= 12) V.snare(t, o, 0.25 + (i - 12) * 0.12, 220);
      if (i === 0 && bar % 8 === 0 && I >= 2) V.hat(t, { dest: this.bus!, rev: 0.6 }, 1.2, true);
    }
    // Bass.
    if (I >= 1) {
      const b = th.bass[i];
      if (b > 0) {
        const root = ch[0] - 24;
        const note = b === 2 ? root + 12 : b === 3 ? root + 7 : root;
        V.bass(t, note, step * (th.bass[(i + 1) % 16] === 0 ? 1.8 : 0.9), o, (I >= 2 ? 1 : 0.6) * quiet, I >= 3 ? 1300 : 800, th.drive ?? 0);
      }
    }
    // Arpeggio.
    if (I >= 3 || (I === 0 && th.drums === 'none')) {
      const every = I === 0 ? 2 : 1;
      if (i % every === 0) {
        const tones = [...ch, ...ch.map((n) => n + 12)].map((n) => n + 12);
        let n: number;
        if (th.arp === 'up') n = tones[i % tones.length];
        else if (th.arp === 'updown') {
          const k = i % (tones.length * 2 - 2);
          n = tones[k < tones.length ? k : tones.length * 2 - 2 - k];
        } else if (th.arp === 'pulse') n = tones[i % 2 === 0 ? 0 : (i >> 1) % tones.length];
        else n = tones[(i * 7 + bar * 3) % tones.length];
        V.pluck(t, n, { dest: this.bus!, dly: 0.35 }, (I === 0 ? 0.5 : 0.55) * quiet, 0.12, 'square', I >= 4 ? 7000 : 4000);
      }
    }
    // Lead.
    if (I >= 4) {
      const k = th.lead[(s % 32 + 32) % 32];
      if (k >= 0) {
        const tones = [...ch, ...ch.map((n) => n + 12), ...ch.map((n) => n + 24)];
        V.lead(t, tones[k % tones.length] + 12, step * 1.8, { dest: this.bus!, dly: 0.25, rev: 0.2 }, quiet, this.overdrive ? 6000 : 3500);
      }
    }
  }

  /** The hero's actions, as notes of the current chord. */
  play(kind: 'shot' | 'accent' | 'perfect' | 'kill' | 'bigKill' | 'parry' | 'great' | 'graze', simBeat: number): void {
    if (!audio.ctx || !this.theme) return;
    const t = audio.now;
    const s = Math.floor(simBeat * 4);
    const ch = this.chord(s);
    const tones = [...ch, ...ch.map((n) => n + 12), ...ch.map((n) => n + 24)];
    const o = { dest: audio.sfx, dly: 0.12 };
    switch (kind) {
      case 'shot':
        V.pluck(t, tones[(this.noteI++ % 3) + 3], o, 0.32, 0.06, 'triangle', 3500);
        break;
      case 'accent':
        V.pluck(t, tones[3 + (this.noteI++ % 4)] + 12, o, 0.5, 0.1, 'square', 6000);
        break;
      case 'perfect':
        V.bell(t, tones[6] + 12, o, 0.6, 0.9);
        V.pluck(t, tones[4] + 12, o, 0.4, 0.12, 'sawtooth', 8000);
        break;
      case 'great':
        V.pluck(t, tones[5] + 12, o, 0.4, 0.1, 'square', 6000);
        break;
      case 'kill':
        V.bell(t, tones[2 + (this.noteI++ % 5)] + 12, { dest: audio.sfx, rev: 0.2 }, 0.45, 0.6);
        break;
      case 'bigKill':
        V.bell(t, tones[0] + 12, { dest: audio.sfx, rev: 0.4 }, 0.7, 1.4);
        V.bell(t, tones[2] + 12, { dest: audio.sfx, rev: 0.4 }, 0.5, 1.4);
        break;
      case 'parry':
        V.bell(t, tones[4] + 24, { dest: audio.sfx, rev: 0.5 }, 0.6, 1.6);
        V.bell(t, tones[6] + 12, { dest: audio.sfx, rev: 0.5 }, 0.5, 1.6);
        break;
      case 'graze':
        V.pluck(t, tones[7] + 12, { dest: audio.sfx }, 0.15, 0.04, 'sine', 9000);
        break;
    }
  }
}

export const music = new Music();
