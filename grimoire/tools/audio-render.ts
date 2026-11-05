/**
 * GRIMOIRE's music and sounds, rendered offline with VOLTA's synthesiser (OfflineAudioContext in headless Chromium).
 * Music: modal (D Dorian) pieces for lute, harp, recorder, drones and tabor — the title, the duel, the Two Banks,
 * the Beast (Phrygian) — each with a tension stem of the same length; the rustle of pages and quills.
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



// ---- D Dorian, 16 bars of 4/4 ---------------------------------------------------------------------------------------
const CH: Record<string, number[]> = { Dm: [62, 65, 69], C: [60, 64, 67], G: [62, 67, 71], Am: [57, 60, 64], F: [60, 65, 69], Eb: [63, 67, 70], Bbm: [61, 65, 70] };
const ROOT: Record<string, number> = { Dm: 38, C: 36, G: 43, Am: 45, F: 41, Eb: 39, Bbm: 46 };
const PROG = ['Dm', 'Dm', 'C', 'Dm', 'Dm', 'G', 'C', 'Am', 'F', 'C', 'G', 'Dm', 'F', 'G', 'Am', 'Dm'];
const PROG_BOSS = ['Dm', 'Eb', 'Dm', 'Eb', 'Dm', 'C', 'Bbm', 'Dm', 'Dm', 'Eb', 'Dm', 'Eb', 'Bbm', 'C', 'Eb', 'Dm'];
// Recorder tune: [bar, beat, note, beats]
const MEL: number[][] = [
  [0, 0, 74, 1.5], [0, 1.5, 76, 0.5], [0, 2, 77, 1], [0, 3, 76, 1], [1, 0, 74, 2], [1, 2, 72, 1], [1, 3, 69, 1],
  [2, 0, 72, 1.5], [2, 1.5, 74, 0.5], [2, 2, 76, 2], [3, 0, 74, 3], [3, 3, 69, 1],
  [4, 0, 74, 1], [4, 1, 77, 1], [4, 2, 81, 2], [5, 0, 79, 1.5], [5, 1.5, 77, 0.5], [5, 2, 74, 2],
  [6, 0, 76, 1], [6, 1, 72, 1], [6, 2, 76, 1], [6, 3, 79, 1], [7, 0, 76, 3], [7, 3, 72, 1],
  [8, 0, 77, 2], [8, 2, 76, 1], [8, 3, 74, 1], [9, 0, 72, 2], [9, 2, 74, 1], [9, 3, 76, 1],
  [10, 0, 79, 1.5], [10, 1.5, 77, 0.5], [10, 2, 74, 2], [11, 0, 74, 3], [11, 3, 76, 1],
  [12, 0, 77, 1], [12, 1, 81, 1], [12, 2, 79, 1], [12, 3, 77, 1], [13, 0, 79, 2], [13, 2, 83, 2],
  [14, 0, 81, 1], [14, 1, 79, 1], [14, 2, 76, 2], [15, 0, 74, 4],
];
const BARS = 16;

function piece(kind: string, t0: number, bars: number, spb: number, o: Out): void {
  for (let b = 0; b < bars; b++) {
    const bar = b % BARS;
    const tb = t0 + b * 4 * spb;
    const c = (kind === 'boss' || kind === 'tension_boss') ? PROG_BOSS[bar] : PROG[bar];
    const ch = CH[c];
    const root = ROOT[c];
    const mel = MEL.filter((m) => m[0] === bar);
    switch (kind) {
      case 'title':
        V.pad(tb, [root + 12, root + 19], 4 * spb, { ...o, rev: 0.6 }, 0.14, 700);
        for (let k = 0; k < 8; k++) V.pluck(tb + k * spb * 0.5, [...ch, ch[0] + 12, ch[1] + 12][k % 5], { ...o, rev: 0.45 }, 0.11, 0.6, 'triangle', 2600);
        if (bar % 2 === 0) for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n, len * spb * 0.9, { ...o, rev: 0.5 }, 0.16, 1500);
        break;
      case 'duel':
        V.kick(tb, o, 0.5); V.kick(tb + 1.5 * spb, o, 0.32); V.kick(tb + 2.5 * spb, o, 0.3);
        V.snare(tb + 2 * spb, o, 0.22, 200);
        for (let k = 0; k < 8; k++) V.hat(tb + k * spb * 0.5, o, k % 2 ? 0.08 : 0.14, false);
        V.bass(tb, root, spb * 1.8, o, 0.7, 600, 0.1); V.bass(tb + 2 * spb, root + 7, spb * 1.8, o, 0.55, 600, 0.1);
        for (let k = 0; k < 8; k++) V.pluck(tb + k * spb * 0.5, [ch[0], ch[2], ch[1], ch[2]][k % 4], o, 0.12, 0.2, 'triangle', 3200);
        V.pad(tb, [root + 12, root + 19], 4 * spb, { ...o, rev: 0.3 }, 0.08, 900);
        for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n, len * spb * 0.92, { ...o, rev: 0.3 }, 0.24, 2600);
        break;
      case 'rives':
        V.kick(tb, o, 0.3); V.kick(tb + 2 * spb, o, 0.25);
        for (let k = 0; k < 4; k++) V.bass(tb + k * spb, k % 2 ? root + 7 : root, spb * 0.5, o, 0.55, 500, 0);
        for (let k = 0; k < 8; k++) V.pluck(tb + k * spb * 0.5, [...ch, ch[0] + 12][(k + bar) % 4] + 12, { ...o, rev: 0.4, dly: 0.2 }, 0.09, 0.4, 'sine', 3800);
        V.pad(tb, ch, 4 * spb, { ...o, rev: 0.5 }, 0.1, 1300);
        if (bar % 4 < 3) for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n - 12, len * spb * 0.9, { ...o, rev: 0.4 }, 0.2, 1900);
        break;
      case 'boss':
        V.kick(tb, o, 0.6); V.kick(tb + spb, o, 0.4); V.kick(tb + 2 * spb, o, 0.6); V.kick(tb + 3 * spb, o, 0.4);
        V.snare(tb + 1 * spb, o, 0.2, 160); V.snare(tb + 3 * spb, o, 0.25, 160);
        V.bass(tb, root - 12, spb * 3.8, o, 0.8, 400, 0.3);
        for (let k = 0; k < 8; k++) V.pluck(tb + k * spb * 0.5, ch[k % 3] - 12, o, 0.14, 0.15, 'sawtooth', 1500);
        V.pad(tb, [...ch, root + 24], 4 * spb, { ...o, rev: 0.5 }, 0.16, 1100);
        for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n - 12 + (c === 'Eb' ? 1 : 0), len * spb * 0.95, { ...o, rev: 0.4 }, 0.26, 1700);
        break;
      default:
        // Tension stems: a quickening heartbeat on the tabor, a low drone, a high tremolo on the fifth.
        V.kick(tb, o, 0.32); V.kick(tb + spb * 0.4, o, 0.2); V.kick(tb + 2 * spb, o, 0.32); V.kick(tb + 2.4 * spb, o, 0.2);
        V.pad(tb, [root, root + 7], 4 * spb, { ...o, rev: 0.3 }, 0.15, 450);
        for (let k = 0; k < 16; k++) V.pluck(tb + k * spb * 0.25, ch[k % 2 ? 2 : 0] + 12, o, 0.04, 0.07, 'sawtooth', 1700);
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

// Pages turning, a quill scratching, a low wind in the vaults: the room around the duel.
async function renderPages(): Promise<string> {
  const L = 16;
  const X = 2;
  const ctx = new OfflineAudioContext(2, Math.ceil((L + X) * SR), SR);
  const bus = setup(ctx);
  const rnd = (a: number, b: number) => a + Math.random() * (b - a);
  const noise = (audio as unknown as { noise: AudioBuffer }).noise;
  const wind = ctx.createBufferSource();
  wind.buffer = noise;
  wind.loop = true;
  const wf = ctx.createBiquadFilter();
  wf.type = 'bandpass';
  wf.frequency.value = 350;
  wf.Q.value = 0.7;
  const wg = ctx.createGain();
  wg.gain.value = 0.05;
  wind.connect(wf); wf.connect(wg); wg.connect(bus);
  wind.start(0); wind.stop(L + X);
  for (let t = 0.6; t < L + X; t += rnd(1.5, 4.0)) {
    // A page: a soft filtered noise sweep.
    const n = ctx.createBufferSource();
    n.buffer = noise;
    n.loop = true;
    const f = ctx.createBiquadFilter();
    f.type = 'bandpass';
    f.Q.value = 1.2;
    f.frequency.setValueAtTime(rnd(1500, 2500), t);
    f.frequency.linearRampToValueAtTime(rnd(3500, 6000), t + 0.35);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(rnd(0.03, 0.06), t + 0.08);
    g.gain.linearRampToValueAtTime(0.0001, t + 0.42);
    const p = ctx.createStereoPanner();
    p.pan.value = rnd(-0.8, 0.8);
    n.connect(f); f.connect(g); g.connect(p); p.connect(bus);
    n.start(t); n.stop(t + 0.5);
  }
  for (let t = 1.2; t < L + X; t += rnd(3, 6)) {
    // A quill: quick scratches.
    for (let k = 0; k < 6; k++) {
      const n = ctx.createBufferSource();
      n.buffer = noise;
      const f = ctx.createBiquadFilter();
      f.type = 'highpass';
      f.frequency.value = 4000;
      const g = ctx.createGain();
      const tt = t + k * rnd(0.07, 0.12);
      g.gain.setValueAtTime(0.0001, tt);
      g.gain.linearRampToValueAtTime(0.02, tt + 0.02);
      g.gain.linearRampToValueAtTime(0.0001, tt + 0.06);
      n.connect(f); f.connect(g); g.connect(bus);
      n.start(tt); n.stop(tt + 0.08);
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
  ui: [0.4, (o) => V.pluck(t, 74, o, 0.2, 0.08, 'triangle', 4000)],
  uiOk: [1.0, (o) => { V.pluck(t, 74, { ...o, rev: 0.3 }, 0.25, 0.4, 'triangle', 3000); V.pluck(t + 0.08, 81, { ...o, rev: 0.3 }, 0.2, 0.5, 'triangle', 3000); }],
  fizzle: [0.8, (o) => { V.sweep(t, 0.4, 2000, 300, o, 0.15, 2); V.pluck(t, 50, o, 0.2, 0.15, 'square', 700); }],
  g_fire: [1.0, (o) => { V.sweep(t, 0.45, 300, 3000, { ...o, rev: 0.2 }, 0.3, 1); V.sub(t, 160, 70, 0.3, o, 0.4); }],
  g_water: [1.2, (o) => { V.sweep(t, 0.7, 2500, 400, { ...o, rev: 0.4 }, 0.25, 4); V.pluck(t + 0.1, 69, { ...o, rev: 0.4 }, 0.15, 0.5, 'sine', 2000); }],
  g_wind: [1.0, (o) => { V.sweep(t, 0.6, 600, 2400, { ...o, rev: 0.4 }, 0.22, 6); V.sweep(t + 0.15, 0.5, 2400, 900, { ...o, rev: 0.4 }, 0.15, 6); }],
  g_bolt: [0.8, (o) => { V.sweep(t, 0.3, 200, 5000, o, 0.18, 8); V.pluck(t, 86, o, 0.15, 0.1, 'square', 9000); }],
  g_earth: [1.0, (o) => { V.sub(t, 120, 40, 0.5, o, 0.8); V.snare(t + 0.05, o, 0.3, 90); }],
  g_shield: [1.4, (o) => { V.bell(t, 74, { ...o, rev: 0.5 }, 0.3, 1.2); V.bell(t + 0.04, 81, { ...o, rev: 0.5 }, 0.2, 1.2); }],
  g_wall: [1.0, (o) => { V.sub(t, 90, 50, 0.3, o, 0.7); V.snare(t, o, 0.35, 110); V.snare(t + 0.1, o, 0.2, 100); }],
  g_star: [1.4, (o) => { [88, 91, 95].forEach((n, i) => V.bell(t + i * 0.06, n, { ...o, rev: 0.5 }, 0.2, 0.9)); }],
  g_heal: [1.8, (o) => { V.pad(t, [62, 66, 69, 74], 1.0, { ...o, rev: 0.6 }, 0.25, 2000); V.bell(t + 0.1, 86, { ...o, rev: 0.6 }, 0.15, 1.2); }],
  g_ice: [1.2, (o) => { [96, 100, 103].forEach((n, i) => V.pluck(t + i * 0.05, n, { ...o, rev: 0.5 }, 0.15, 0.3, 'sine', 9000)); V.sweep(t, 0.4, 6000, 2000, o, 0.08, 3); }],
  g_time: [1.8, (o) => { for (let i = 0; i < 4; i++) V.pluck(t + i * 0.18, i % 2 ? 84 : 79, { ...o, rev: 0.4 }, 0.2, 0.08, 'square', 4000); V.bell(t + 0.75, 67, { ...o, rev: 0.6 }, 0.25, 1.4); }],
  g_seal: [1.6, (o) => { V.bell(t, 81, { ...o, rev: 0.6 }, 0.25, 1.4); V.bell(t + 0.12, 80, { ...o, rev: 0.6 }, 0.2, 1.4); V.sub(t, 200, 60, 0.4, o, 0.3); }],
  combo: [2.2, (o) => { [62, 69, 74, 81].forEach((n, i) => V.bell(t + i * 0.07, n, { ...o, rev: 0.6 }, 0.3, 1.6)); V.sweep(t, 0.6, 400, 6000, { ...o, rev: 0.5 }, 0.15, 2); }],
  clash: [0.8, (o) => { V.snare(t, { ...o, rev: 0.3 }, 0.45, 150); V.sweep(t, 0.2, 4000, 800, o, 0.12, 2); }],
  hit: [0.7, (o) => { V.kick(t, o, 0.8); V.snare(t, o, 0.25, 120); }],
  block: [0.8, (o) => { V.bell(t, 69, { ...o, rev: 0.3 }, 0.3, 0.5); V.snare(t, o, 0.15, 220); }],
  thunder: [2.0, (o) => { V.snare(t, { ...o, rev: 0.8 }, 0.8, 120); V.sub(t, 90, 30, 1.2, { ...o, rev: 0.6 }, 0.7); V.sweep(t, 0.15, 9000, 1500, o, 0.3, 1); }],
  break: [1.0, (o) => { V.snare(t, o, 0.4, 90); V.snare(t + 0.08, o, 0.3, 80); V.sub(t, 100, 40, 0.4, o, 0.6); }],
  seal: [1.6, (o) => { V.bell(t, 86, { ...o, rev: 0.6 }, 0.3, 1.2); V.sweep(t, 0.5, 3000, 300, { ...o, rev: 0.5 }, 0.15, 4); }],
  step: [0.3, (o) => V.sweep(t, 0.12, 800, 2500, o, 0.08, 2)],
  round: [2.0, (o) => { V.bell(t, 62, { ...o, rev: 0.6 }, 0.4, 1.8); V.bell(t + 0.3, 69, { ...o, rev: 0.6 }, 0.35, 1.8); }],
  win: [3.4, (o) => { [62, 66, 69, 74, 78].forEach((n, i) => V.pluck(t + i * 0.1, n, { ...o, rev: 0.5 }, 0.3, 1.2, 'triangle', 3000)); V.pad(t + 0.45, [62, 66, 69, 74], 1.8, { ...o, rev: 0.5 }, 0.3, 2000); }],
  lose: [3.0, (o) => { V.pad(t, [50, 53, 57], 2.2, { ...o, rev: 0.6 }, 0.32, 800); V.pluck(t + 0.1, 62, { ...o, rev: 0.6 }, 0.25, 1.4, 'triangle', 1500); V.pluck(t + 0.5, 60, { ...o, rev: 0.6 }, 0.22, 1.6, 'triangle', 1500); }],
  ink_die: [1.0, (o) => { V.sweep(t, 0.5, 1200, 150, { ...o, rev: 0.4 }, 0.2, 3); V.sub(t, 80, 40, 0.3, o, 0.3); }],
  clear: [1.6, (o) => { [74, 78, 81].forEach((n, i) => V.bell(t + i * 0.08, n, { ...o, rev: 0.5 }, 0.25, 1.2)); }],
  beast: [3.0, (o) => { V.sub(t, 60, 28, 2.2, { ...o, rev: 0.7 }, 0.9); V.pad(t, [38, 39, 45], 2.2, { ...o, rev: 0.7 }, 0.3, 500); }],
  calib: [0.3, (o) => V.pluck(t, 96, o, 0.25, 0.02, 'square', 9000)],
};

const api = {
  pieces: () => [['title', 84], ['duel', 132], ['rives', 108], ['boss', 140], ['tension_duel', 132], ['tension_rives', 108], ['tension_boss', 140]],
  piece: (n: string, bpm: number) => renderPiece(n, bpm),
  pages: () => renderPages(),
  sfxNames: () => Object.keys(SFX),
  sfx: (n: string) => renderOne(SFX[n][0] + 2.0, SFX[n][1]),
};
(window as unknown as { grim: typeof api }).grim = api;
