/**
 * TEMPÊTE's music and sounds, rendered offline with VOLTA's synthesiser (OfflineAudioContext in headless Chromium).
 * Music: wide-open prairie pieces in E (open fifths, plucked guitar-like arpeggios, slide lead) — the title, the chase
 * and its tension stem; loops for the engine, rain, hail and wind; thunder near and far, sirens, the shutter.
 * Bundled with esbuild; see audio-render.cjs.
 */
import { audio } from '../../volta/src/engine/audio';
import { V } from '../../volta/src/engine/synth';

const SR = 48000;
type Out = { dest: AudioNode; rev?: number; dly?: number };

function setup(ctx: OfflineAudioContext, delayTime = 0.25): GainNode {
  const out = ctx.createGain();
  out.connect(ctx.destination);
  const a = audio as unknown as Record<string, unknown>;
  a.ctx = ctx;
  const bus = ctx.createGain();
  bus.connect(out);
  a.music = bus;
  a.sfx = bus;
  const reverb = ctx.createConvolver();
  const len = Math.floor(SR * 2.2);
  const ir = ctx.createBuffer(2, len, SR);
  for (let c = 0; c < 2; c++) {
    const d = ir.getChannelData(c);
    for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 3.0);
  }
  reverb.buffer = ir;
  const rs = ctx.createGain();
  rs.gain.value = 0.9;
  rs.connect(reverb);
  const rv = ctx.createGain();
  rv.gain.value = 0.3;
  reverb.connect(rv);
  rv.connect(out);
  a.reverbSend = rs;
  const delay = ctx.createDelay(2);
  delay.delayTime.value = delayTime;
  const fb = ctx.createGain();
  fb.gain.value = 0.3;
  const dlp = ctx.createBiquadFilter();
  dlp.type = 'lowpass';
  dlp.frequency.value = 3000;
  const ds = ctx.createGain();
  ds.connect(delay);
  delay.connect(dlp);
  dlp.connect(fb);
  fb.connect(delay);
  const dv = ctx.createGain();
  dv.gain.value = 0.25;
  dlp.connect(dv);
  dv.connect(out);
  a.delaySend = ds;
  const noise = ctx.createBuffer(1, SR, SR);
  const nd = noise.getChannelData(0);
  for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;
  a.noise = noise;
  return bus;
}

function wav(buf: AudioBuffer, from = 0, to = buf.length): string {
  const n = to - from;
  const ch = buf.numberOfChannels;
  const bytes = new ArrayBuffer(44 + n * ch * 2);
  const v = new DataView(bytes);
  const w = (o: number, s: string) => [...s].forEach((c, i) => v.setUint8(o + i, c.charCodeAt(0)));
  w(0, 'RIFF'); v.setUint32(4, 36 + n * ch * 2, true); w(8, 'WAVE'); w(12, 'fmt ');
  v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, ch, true); v.setUint32(24, SR, true);
  v.setUint32(28, SR * ch * 2, true); v.setUint16(32, ch * 2, true); v.setUint16(34, 16, true); w(36, 'data');
  v.setUint32(40, n * ch * 2, true);
  const data = [...Array(ch)].map((_, c) => buf.getChannelData(c));
  let o = 44;
  for (let i = from; i < to; i++) for (let c = 0; c < ch; c++) {
    const s = Math.max(-1, Math.min(1, data[c][i]));
    v.setInt16(o, s < 0 ? s * 0x8000 : s * 0x7fff, true);
    o += 2;
  }
  let bin = '';
  const u8 = new Uint8Array(bytes);
  for (let i = 0; i < u8.length; i += 0x8000) bin += String.fromCharCode(...u8.subarray(i, i + 0x8000));
  return btoa(bin);
}




// ---- E Mixolydian / minor, 16 bars of 4/4 ------------------------------------------------------------------------------
const CH: Record<string, number[]> = { E: [52, 59, 64, 68], D: [50, 57, 62, 66], A: [45, 52, 57, 61], Cm: [48, 55, 60, 63], B: [47, 54, 59, 63], Em: [52, 59, 64, 67], C: [48, 55, 60, 64], G: [43, 50, 55, 59] };
const PROG = ['E', 'E', 'D', 'A', 'E', 'E', 'D', 'A', 'Cm', 'D', 'E', 'E', 'C', 'D', 'B', 'B'];
const PROG_T = ['Em', 'Em', 'C', 'D', 'Em', 'Em', 'C', 'B', 'Em', 'Em', 'C', 'D', 'C', 'D', 'B', 'B'];
const MEL: number[][] = [
  [0, 0, 76, 3], [0, 3, 74, 1], [1, 0, 71, 4], [2, 0, 74, 2], [2, 2, 78, 2], [3, 0, 76, 4],
  [4, 0, 76, 1], [4, 1, 79, 2], [4, 3, 81, 1], [5, 0, 83, 4], [6, 0, 81, 2], [6, 2, 78, 2], [7, 0, 76, 4],
  [8, 0, 75, 3], [8, 3, 74, 1], [9, 0, 74, 2], [9, 2, 78, 2], [10, 0, 80, 4], [11, 0, 76, 4],
  [12, 0, 79, 2], [12, 2, 76, 2], [13, 0, 78, 2], [13, 2, 81, 2], [14, 0, 78, 4], [15, 0, 75, 4],
];
const BARS = 16;

function piece(kind: string, t0: number, bars: number, spb: number, o: Out): void {
  for (let b = 0; b < bars; b++) {
    const bar = b % BARS;
    const tb = t0 + b * 4 * spb;
    const c = kind === 'title' || kind === 'chase' ? PROG[bar] : PROG_T[bar];
    const ch = CH[c];
    const root = ch[0] - 12;
    const mel = MEL.filter((m) => m[0] === bar);
    switch (kind) {
      case 'title':
        V.pad(tb, [ch[0], ch[1], ch[2]], 4 * spb, { ...o, rev: 0.6 }, 0.14, 800);
        for (let k = 0; k < 8; k++) V.pluck(tb + k * spb * 0.5, [ch[0], ch[1], ch[2], ch[3], ch[2] + 12, ch[3], ch[2], ch[1]][k] + 12, { ...o, rev: 0.4, dly: 0.25 }, 0.1, 0.9, 'triangle', 2400);
        if (bar % 2 === 0) for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n - 12, len * spb * 0.95, { ...o, rev: 0.6 }, 0.14, 1400);
        V.bass(tb, root, spb * 3.5, o, 0.4, 350, 0);
        break;
      case 'chase':
        V.kick(tb, o, 0.55); V.kick(tb + 2 * spb, o, 0.5); V.kick(tb + 2.5 * spb, o, 0.3);
        V.snare(tb + spb, o, 0.22, 180); V.snare(tb + 3 * spb, o, 0.25, 180);
        for (let k = 0; k < 8; k++) V.hat(tb + k * spb * 0.5, o, k % 2 ? 0.07 : 0.12, k === 7);
        for (let k = 0; k < 8; k++) V.bass(tb + k * spb * 0.5, k % 4 === 3 ? root + 7 : root, spb * 0.45, o, 0.6, 600, 0.15);
        for (let k = 0; k < 16; k++) V.pluck(tb + k * spb * 0.25, [ch[1], ch[2], ch[3], ch[2]][k % 4] + 12, o, 0.07, 0.15, 'triangle', 3000);
        V.pad(tb, [ch[0] + 12, ch[1] + 12], 4 * spb, { ...o, rev: 0.4 }, 0.08, 1200);
        for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n, len * spb * 0.95, { ...o, rev: 0.35 }, 0.18, 2200);
        break;
      default:
        // Tension: a pulse in eighth notes on the root, a low drone, rising string-like tremolo on the fifth.
        for (let k = 0; k < 8; k++) V.kick(tb + k * spb * 0.5, o, k % 2 ? 0.16 : 0.28);
        V.pad(tb, [root, root + 7, root + 12], 4 * spb, { ...o, rev: 0.4 }, 0.18, 500);
        for (let k = 0; k < 16; k++) V.pluck(tb + k * spb * 0.25, ch[k % 2 ? 1 : 0] + 24, o, 0.045, 0.07, 'sawtooth', 1800);
        break;
    }
  }
}

async function renderPiece(kind: string, bpm: number): Promise<string> {
  const spb = 60 / bpm;
  const loop = BARS * 4 * spb;
  const lead = 0.05;
  const ctx = new OfflineAudioContext(2, Math.ceil((loop * 2 + lead) * SR), SR);
  const bus = setup(ctx, spb * 0.75);
  piece(kind, lead, BARS * 2, spb, { dest: bus });
  const buf = await ctx.startRendering();
  const from = Math.round((lead + loop) * SR);
  return wav(buf, from, from + Math.round(loop * SR));
}

// Seamless ambience loops (render long, cross-fade the tail into the head).
async function renderLoop(kind: string): Promise<string> {
  const L = 8;
  const X = 1.5;
  const ctx = new OfflineAudioContext(2, Math.ceil((L + X) * SR), SR);
  const bus = setup(ctx);
  const noise = (audio as unknown as { noise: AudioBuffer }).noise;
  const rnd = (a: number, b: number) => a + Math.random() * (b - a);
  const src = (type: BiquadFilterType, f: number, q: number, g: number) => {
    const n = ctx.createBufferSource(); n.buffer = noise; n.loop = true;
    const fl = ctx.createBiquadFilter(); fl.type = type; fl.frequency.value = f; fl.Q.value = q;
    const gn = ctx.createGain(); gn.gain.value = g;
    n.connect(fl); fl.connect(gn); gn.connect(bus); n.start(0); n.stop(L + X);
    return { fl, gn };
  };
  if (kind === 'engine') {
    // A V8 idle-to-cruise rumble: low sawtooth harmonics with a little roughness.
    for (const [f, g] of [[42, 0.18], [84, 0.1], [126, 0.06], [168, 0.03]]) {
      const o = ctx.createOscillator(); o.type = 'sawtooth'; o.frequency.value = f;
      const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 500;
      const gn = ctx.createGain(); gn.gain.value = g;
      const lfo = ctx.createOscillator(); lfo.frequency.value = 7.5; const lg = ctx.createGain(); lg.gain.value = g * 0.4;
      lfo.connect(lg); lg.connect(gn.gain);
      o.connect(lp); lp.connect(gn); gn.connect(bus); o.start(0); o.stop(L + X); lfo.start(0); lfo.stop(L + X);
    }
    src('lowpass', 300, 0.5, 0.05);
  } else if (kind === 'rain') {
    src('highpass', 1800, 0.4, 0.12);
    src('bandpass', 700, 0.5, 0.08);
    for (let t = 0; t < L + X; t += rnd(0.004, 0.02)) V.pluck(t, Math.floor(rnd(96, 110)), { dest: bus }, rnd(0.004, 0.015), 0.01, 'sine', 9000);
  } else if (kind === 'hail') {
    src('bandpass', 2500, 0.6, 0.05);
    for (let t = 0; t < L + X; t += rnd(0.01, 0.05)) {
      V.pluck(t, Math.floor(rnd(84, 100)), { dest: bus }, rnd(0.03, 0.09), 0.02, 'square', 6000);
      if (Math.random() < 0.3) V.snare(t, { dest: bus }, rnd(0.02, 0.06), 300);
    }
  } else if (kind === 'wind') {
    const a = src('bandpass', 420, 1.2, 0.12);
    const b = src('bandpass', 900, 2.0, 0.05);
    for (let t = 0; t < L + X; t += 0.5) {
      a.fl.frequency.linearRampToValueAtTime(rnd(300, 650), t + 0.5);
      b.gn.gain.linearRampToValueAtTime(rnd(0.02, 0.08), t + 0.5);
    }
  }
  const buf = await ctx.startRendering();
  const n = Math.round(L * SR);
  const x = Math.round(X * SR);
  for (let c = 0; c < buf.numberOfChannels; c++) {
    const d = buf.getChannelData(c);
    for (let i = 0; i < x; i++) { const k = i / x; d[i] = d[i] * k + d[n + i] * (1 - k); }
  }
  return wav(buf, 0, n);
}

async function renderOne(len: number, play: (o: Out) => void): Promise<string> {
  const ctx = new OfflineAudioContext(2, Math.ceil(len * SR), SR);
  const bus = setup(ctx);
  play({ dest: bus });
  const buf = await ctx.startRendering();
  let end = buf.length;
  const a = buf.getChannelData(0);
  while (end > SR * 0.05 && Math.abs(a[end - 1]) < 2e-4) end--;
  return wav(buf, 0, end);
}

const t = 0.01;
const SFX: Record<string, [number, (o: Out) => void]> = {
  ui: [0.4, (o) => V.pluck(t, 76, o, 0.2, 0.06, 'triangle', 4000)],
  uiOk: [1.0, (o) => { V.pluck(t, 76, { ...o, rev: 0.3 }, 0.25, 0.4, 'triangle', 3000); V.pluck(t + 0.08, 83, { ...o, rev: 0.3 }, 0.2, 0.5, 'triangle', 3000); }],
  shutter: [0.5, (o) => { V.pluck(t, 100, o, 0.4, 0.015, 'square', 9000); V.snare(t + 0.02, o, 0.15, 400); V.pluck(t + 0.09, 96, o, 0.3, 0.015, 'square', 9000); V.sweep(t + 0.1, 0.25, 1500, 2500, o, 0.05, 2); }],
  probe: [0.8, (o) => { V.sub(t, 160, 60, 0.25, o, 0.6); V.snare(t, o, 0.25, 140); V.pluck(t + 0.3, 88, o, 0.15, 0.1, 'sine', 6000); V.pluck(t + 0.45, 88, o, 0.15, 0.1, 'sine', 6000); }],
  score: [1.6, (o) => { [76, 80, 83, 88].forEach((n, i) => V.bell(t + i * 0.07, n, { ...o, rev: 0.4 }, 0.25, 1.0)); }],
  siren: [5.0, (o) => { V.sweep(t, 2.2, 400, 900, { ...o, rev: 0.6 }, 0.25, 8); V.sweep(t + 2.2, 2.2, 900, 400, { ...o, rev: 0.6 }, 0.25, 8); }],
  alert: [0.6, (o) => { V.pluck(t, 88, o, 0.2, 0.08, 'square', 4000); V.pluck(t + 0.12, 88, o, 0.2, 0.08, 'square', 4000); }],
  radio: [0.4, (o) => { V.sweep(t, 0.12, 3000, 1200, o, 0.06, 1); V.pluck(t + 0.05, 84, o, 0.08, 0.05, 'sine', 5000); }],
  thunder: [6.0, (o) => { V.sub(t, 70, 30, 4.0, { ...o, rev: 0.9 }, 0.8); V.snare(t + 0.3, { ...o, rev: 0.9 }, 0.25, 70); V.snare(t + 1.1, { ...o, rev: 0.9 }, 0.2, 60); }],
  thunder_near: [5.0, (o) => { V.snare(t, { ...o, rev: 0.8 }, 1.0, 200); V.sweep(t, 0.12, 9000, 1500, o, 0.4, 1); V.sub(t + 0.05, 90, 30, 3.5, { ...o, rev: 0.8 }, 0.9); }],
  hit_hail: [0.4, (o) => { V.pluck(t, 96, o, 0.3, 0.02, 'square', 7000); V.pluck(t + 0.05, 91, o, 0.25, 0.02, 'square', 7000); }],
  hit_debris: [0.8, (o) => { V.kick(t, o, 0.9); V.snare(t, o, 0.4, 100); }],
  hit_lightning: [1.5, (o) => { V.snare(t, { ...o, rev: 0.5 }, 1.0, 250); V.sweep(t, 0.3, 8000, 200, o, 0.3, 2); }],
  wreck: [3.0, (o) => { V.kick(t, o, 1.2); V.snare(t, { ...o, rev: 0.5 }, 0.8, 90); V.snare(t + 0.15, o, 0.5, 70); V.sub(t, 120, 30, 1.2, o, 0.8); }],
  calib: [0.3, (o) => V.pluck(t, 96, o, 0.25, 0.02, 'square', 9000)],
};

const api = {
  pieces: () => [['title', 76], ['chase', 120], ['tension', 120]],
  piece: (n: string, bpm: number) => renderPiece(n, bpm),
  loops: () => ['engine', 'rain', 'hail', 'wind'],
  loop: (n: string) => renderLoop(n),
  sfxNames: () => Object.keys(SFX),
  sfx: (n: string) => renderOne(SFX[n][0] + 2.0, SFX[n][1]),
};
(window as unknown as { temp: typeof api }).temp = api;
