import { audio } from './audio';

/** Synth voices scheduled at an exact audio time. Each creates its nodes and lets them go. */
export const mtof = (m: number) => 440 * Math.pow(2, (m - 69) / 12);

interface Out {
  dest: AudioNode;
  rev?: number;
  dly?: number;
}

function route(node: AudioNode, o: Out): void {
  node.connect(o.dest);
  if (o.rev) {
    const g = audio.ctx!.createGain();
    g.gain.value = o.rev;
    node.connect(g);
    g.connect(audio.reverbSend);
  }
  if (o.dly) {
    const g = audio.ctx!.createGain();
    g.gain.value = o.dly;
    node.connect(g);
    g.connect(audio.delaySend);
  }
}

function env(g: GainNode, t: number, a: number, peak: number, d: number, s: number, r: number, len: number): number {
  const p = g.gain;
  p.setValueAtTime(0.0001, t);
  p.linearRampToValueAtTime(peak, t + a);
  p.setTargetAtTime(peak * s, t + a, d / 3);
  p.setTargetAtTime(0.0001, t + len, r / 4);
  return t + len + r;
}

export const V = {
  kick(t: number, o: Out, vol = 1): void {
    const c = audio.ctx!;
    const osc = c.createOscillator();
    const g = c.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(160, t);
    osc.frequency.exponentialRampToValueAtTime(42, t + 0.12);
    g.gain.setValueAtTime(vol * 1.1, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + 0.42);
    osc.connect(g);
    route(g, o);
    osc.start(t);
    osc.stop(t + 0.45);
    // Click.
    const n = c.createBufferSource();
    n.buffer = audio.noise;
    const hp = c.createBiquadFilter();
    hp.type = 'highpass';
    hp.frequency.value = 2500;
    const ng = c.createGain();
    ng.gain.setValueAtTime(vol * 0.25, t);
    ng.gain.exponentialRampToValueAtTime(0.001, t + 0.015);
    n.connect(hp);
    hp.connect(ng);
    route(ng, o);
    n.start(t);
    n.stop(t + 0.03);
  },
  snare(t: number, o: Out, vol = 1, tone = 190): void {
    const c = audio.ctx!;
    const n = c.createBufferSource();
    n.buffer = audio.noise;
    const bp = c.createBiquadFilter();
    bp.type = 'bandpass';
    bp.frequency.value = 2200;
    bp.Q.value = 0.7;
    const g = c.createGain();
    g.gain.setValueAtTime(vol * 0.7, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + 0.22);
    n.connect(bp);
    bp.connect(g);
    route(g, { ...o, rev: (o.rev ?? 0) + 0.25 });
    n.start(t, Math.random() * 0.5);
    n.stop(t + 0.25);
    const osc = c.createOscillator();
    osc.type = 'triangle';
    osc.frequency.setValueAtTime(tone, t);
    osc.frequency.exponentialRampToValueAtTime(tone * 0.6, t + 0.08);
    const og = c.createGain();
    og.gain.setValueAtTime(vol * 0.5, t);
    og.gain.exponentialRampToValueAtTime(0.001, t + 0.1);
    osc.connect(og);
    route(og, o);
    osc.start(t);
    osc.stop(t + 0.12);
  },
  hat(t: number, o: Out, vol = 1, open = false): void {
    const c = audio.ctx!;
    const n = c.createBufferSource();
    n.buffer = audio.noise;
    const hp = c.createBiquadFilter();
    hp.type = 'highpass';
    hp.frequency.value = 7500;
    const g = c.createGain();
    const len = open ? 0.18 : 0.04;
    g.gain.setValueAtTime(vol * 0.28, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + len);
    n.connect(hp);
    hp.connect(g);
    route(g, o);
    n.start(t, Math.random() * 0.8);
    n.stop(t + len + 0.02);
  },
  /** Mono saw bass with a filter pluck. */
  bass(t: number, note: number, len: number, o: Out, vol = 1, cutoff = 900, drive = 0): void {
    const c = audio.ctx!;
    const g = c.createGain();
    const f = c.createBiquadFilter();
    f.type = 'lowpass';
    f.Q.value = 6;
    f.frequency.setValueAtTime(cutoff * 3, t);
    f.frequency.exponentialRampToValueAtTime(cutoff * 0.5, t + Math.min(len, 0.25));
    const fr = mtof(note);
    for (const [type, det, gv] of [['sawtooth', 0, 0.5], ['square', -7, 0.3], ['sine', -1200, 0.6]] as const) {
      const osc = c.createOscillator();
      osc.type = type;
      osc.frequency.value = fr;
      osc.detune.value = det;
      const og = c.createGain();
      og.gain.value = gv;
      osc.connect(og);
      og.connect(f);
      osc.start(t);
      osc.stop(t + len + 0.1);
    }
    let last: AudioNode = f;
    if (drive > 0) {
      const ws = c.createWaveShaper();
      const curve = new Float32Array(256);
      for (let i = 0; i < 256; i++) {
        const x = (i / 255) * 2 - 1;
        curve[i] = Math.tanh(x * (1 + drive * 4));
      }
      ws.curve = curve;
      f.connect(ws);
      last = ws;
    }
    last.connect(g);
    env(g, t, 0.004, vol * 0.42, 0.1, 0.7, 0.06, len);
    route(g, o);
  },
  /** Detuned-saw pad chord. */
  pad(t: number, notes: number[], len: number, o: Out, vol = 1, bright = 1200): void {
    const c = audio.ctx!;
    const g = c.createGain();
    const f = c.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.setValueAtTime(bright * 0.6, t);
    f.frequency.linearRampToValueAtTime(bright, t + len * 0.5);
    f.Q.value = 0.7;
    for (const n of notes) {
      for (const det of [-11, 0, 12]) {
        const osc = c.createOscillator();
        osc.type = 'sawtooth';
        osc.frequency.value = mtof(n);
        osc.detune.value = det;
        osc.connect(f);
        osc.start(t);
        osc.stop(t + len + 1.2);
      }
    }
    f.connect(g);
    env(g, t, Math.min(0.4, len * 0.3), (vol * 0.07) / Math.sqrt(notes.length), 0.5, 0.8, 0.9, len);
    route(g, { ...o, rev: (o.rev ?? 0) + 0.6 });
  },
  /** Bright pluck for arpeggios and the hero's notes. */
  pluck(t: number, note: number, o: Out, vol = 1, len = 0.18, type: OscillatorType = 'square', bright = 5000): void {
    const c = audio.ctx!;
    const osc = c.createOscillator();
    osc.type = type;
    osc.frequency.value = mtof(note);
    const osc2 = c.createOscillator();
    osc2.type = 'sawtooth';
    osc2.frequency.value = mtof(note);
    osc2.detune.value = 9;
    const f = c.createBiquadFilter();
    f.type = 'lowpass';
    f.Q.value = 4;
    f.frequency.setValueAtTime(bright, t);
    f.frequency.exponentialRampToValueAtTime(400, t + len);
    const g = c.createGain();
    osc.connect(f);
    osc2.connect(f);
    f.connect(g);
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(vol * 0.14, t + 0.003);
    g.gain.exponentialRampToValueAtTime(0.0008, t + len + 0.1);
    route(g, o);
    osc.start(t);
    osc2.start(t);
    osc.stop(t + len + 0.15);
    osc2.stop(t + len + 0.15);
  },
  /** FM bell. */
  bell(t: number, note: number, o: Out, vol = 1, len = 1.2): void {
    const c = audio.ctx!;
    const car = c.createOscillator();
    const mod = c.createOscillator();
    const mg = c.createGain();
    const fr = mtof(note);
    car.frequency.value = fr;
    mod.frequency.value = fr * 3.5;
    mg.gain.setValueAtTime(fr * 2.5, t);
    mg.gain.exponentialRampToValueAtTime(fr * 0.1, t + len);
    mod.connect(mg);
    mg.connect(car.frequency);
    const g = c.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(vol * 0.16, t + 0.004);
    g.gain.exponentialRampToValueAtTime(0.0008, t + len);
    car.connect(g);
    route(g, { ...o, rev: (o.rev ?? 0) + 0.4 });
    car.start(t);
    mod.start(t);
    car.stop(t + len + 0.1);
    mod.stop(t + len + 0.1);
  },
  /** Supersaw lead. */
  lead(t: number, note: number, len: number, o: Out, vol = 1, bright = 3500): void {
    const c = audio.ctx!;
    const f = c.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.setValueAtTime(bright, t);
    f.Q.value = 2;
    const g = c.createGain();
    for (const det of [-18, -7, 0, 7, 18]) {
      const osc = c.createOscillator();
      osc.type = 'sawtooth';
      osc.frequency.value = mtof(note);
      osc.detune.value = det;
      osc.connect(f);
      osc.start(t);
      osc.stop(t + len + 0.4);
    }
    f.connect(g);
    env(g, t, 0.01, vol * 0.05, 0.2, 0.75, 0.25, len);
    route(g, o);
  },
  /** Noise sweep (risers, impacts). */
  sweep(t: number, len: number, from: number, to: number, o: Out, vol = 1, q = 3): void {
    const c = audio.ctx!;
    const n = c.createBufferSource();
    n.buffer = audio.noise;
    n.loop = true;
    const f = c.createBiquadFilter();
    f.type = 'bandpass';
    f.Q.value = q;
    f.frequency.setValueAtTime(from, t);
    f.frequency.exponentialRampToValueAtTime(to, t + len);
    const g = c.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(vol * 0.5, t + len * 0.1);
    g.gain.exponentialRampToValueAtTime(0.001, t + len);
    n.connect(f);
    f.connect(g);
    route(g, o);
    n.start(t);
    n.stop(t + len + 0.05);
  },
  /** Sub drop (big impacts). */
  sub(t: number, from: number, to: number, len: number, o: Out, vol = 1): void {
    const c = audio.ctx!;
    const osc = c.createOscillator();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(from, t);
    osc.frequency.exponentialRampToValueAtTime(to, t + len);
    const g = c.createGain();
    g.gain.setValueAtTime(vol * 0.9, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + len);
    osc.connect(g);
    route(g, o);
    osc.start(t);
    osc.stop(t + len + 0.05);
  },
};
