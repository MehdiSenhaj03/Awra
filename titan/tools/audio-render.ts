/**
 * TITAN's music and sounds, rendered offline with VOLTA's synthesiser (OfflineAudioContext in headless Chromium).
 * Music in D hijaz (a desert mode): the title, the march (big slow drums, plucked oud, a cello drone), the fight
 * (war drums, brass), the Source (luminous, in D major); ambience loops; the titans' footfalls, roars, blows.
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

// ---- D hijaz, 16 bars of 4/4 ------------------------------------------------------------------------------------------
const CH: Record<string, number[]> = {
  Dm: [50, 57, 62, 65], D: [50, 57, 62, 66], Eb: [51, 58, 63, 67], C: [48, 55, 60, 64], Bb: [46, 53, 58, 62],
  Gm: [43, 50, 55, 58], G: [43, 50, 55, 59], A: [45, 52, 57, 61], Bm: [47, 54, 59, 62], Em: [52, 59, 64, 67],
};
const PROG = ['Dm', 'Dm', 'Eb', 'Dm', 'Gm', 'Dm', 'Eb', 'Dm', 'Gm', 'Bb', 'Gm', 'Eb', 'Bb', 'C', 'A', 'Dm'];
const PROG_S = ['D', 'D', 'G', 'D', 'Bm', 'G', 'A', 'A', 'D', 'G', 'Em', 'A', 'Bm', 'G', 'A', 'D'];
const MEL: number[][] = [
  [0, 0, 74, 2], [0, 2, 75, 1], [0, 3, 78, 1], [1, 0, 79, 3], [1, 3, 78, 1], [2, 0, 75, 2], [2, 2, 74, 2], [3, 0, 74, 4],
  [4, 0, 81, 2], [4, 2, 82, 1], [4, 3, 81, 1], [5, 0, 79, 2], [5, 2, 78, 2], [6, 0, 79, 1], [6, 1, 78, 1], [6, 2, 75, 2], [7, 0, 74, 4],
  [8, 0, 79, 2], [8, 2, 81, 2], [9, 0, 82, 3], [9, 3, 81, 1], [10, 0, 79, 2], [10, 2, 78, 2], [11, 0, 75, 4],
  [12, 0, 82, 2], [12, 2, 84, 2], [13, 0, 81, 2], [13, 2, 79, 2], [14, 0, 78, 2], [14, 2, 75, 2], [15, 0, 74, 4],
];
const MEL_S: number[][] = [
  [0, 0, 78, 2], [0, 2, 81, 2], [1, 0, 86, 4], [2, 0, 83, 2], [2, 2, 81, 2], [3, 0, 78, 4],
  [4, 0, 78, 2], [4, 2, 79, 2], [5, 0, 83, 4], [6, 0, 81, 3], [6, 3, 79, 1], [7, 0, 76, 4],
  [8, 0, 78, 2], [8, 2, 81, 2], [9, 0, 86, 3], [9, 3, 88, 1], [10, 0, 86, 2], [10, 2, 83, 2], [11, 0, 85, 4],
  [12, 0, 86, 2], [12, 2, 83, 2], [13, 0, 81, 2], [13, 2, 79, 2], [14, 0, 81, 2], [14, 2, 85, 2], [15, 0, 86, 4],
];
const BARS = 16;

function piece(kind: string, t0: number, bars: number, spb: number, o: Out): void {
  for (let b = 0; b < bars; b++) {
    const bar = b % BARS;
    const tb = t0 + b * 4 * spb;
    const c = kind === 'source' ? PROG_S[bar] : PROG[bar];
    const ch = CH[c];
    const root = ch[0] - 12;
    const mel = (kind === 'source' ? MEL_S : MEL).filter((m) => m[0] === bar);
    switch (kind) {
      case 'title':
        // Wind-swept and slow: a drone, plucked arpeggios like an oud far away, the melody on a reedy lead.
        V.pad(tb, [ch[0], ch[1], ch[2]], 4 * spb, { ...o, rev: 0.7 }, 0.12, 700);
        V.bass(tb, 38, spb * 4, { ...o, rev: 0.4 }, 0.28, 260, 0);
        for (let k = 0; k < 4; k++) V.pluck(tb + k * spb, [ch[0], ch[2], ch[1], ch[3]][k] + 12, { ...o, rev: 0.5, dly: 0.3 }, 0.1, 0.7, 'triangle', 1800);
        if (bar >= 4) for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n - 12, len * spb * 0.95, { ...o, rev: 0.7 }, 0.12, 1100);
        break;
      case 'march':
        // The walk: a great drum on the titan's steps, frame drums, a plucked ostinato, cello drone, the song.
        V.kick(tb, { ...o, rev: 0.4 }, 0.75); V.kick(tb + 2 * spb, { ...o, rev: 0.4 }, 0.6);
        V.snare(tb + 1.5 * spb, o, 0.12, 110); V.snare(tb + 3 * spb, o, 0.16, 120); V.snare(tb + 3.5 * spb, o, 0.1, 140);
        for (let k = 0; k < 8; k++) V.pluck(tb + k * spb * 0.5, [ch[0], ch[1], ch[2], ch[1], ch[0] + 12, ch[1], ch[2], ch[3]][k], { ...o, rev: 0.3 }, 0.12, 0.35, 'triangle', 2200);
        V.bass(tb, root, spb * 3.8, o, 0.45, 380, 0.05);
        V.pad(tb, [ch[1], ch[2]], 4 * spb, { ...o, rev: 0.5 }, 0.07, 900);
        for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n - 12, len * spb * 0.95, { ...o, rev: 0.45 }, 0.15, 1500);
        break;
      case 'fight':
        // War drums in triplets, brass stabs, a driving low string.
        for (let k = 0; k < 4; k++) { V.kick(tb + k * spb, o, k % 2 ? 0.55 : 0.85); V.snare(tb + k * spb + spb * 0.66, o, 0.14, 100); }
        V.snare(tb + 3.5 * spb, o, 0.3, 130);
        for (let k = 0; k < 8; k++) V.bass(tb + k * spb * 0.5, k % 4 === 3 ? root + 1 : root, spb * 0.45, o, 0.55, 700, 0.3);
        if (bar % 2 === 0) { V.lead(tb, ch[0] + 12, spb * 0.5, { ...o, rev: 0.3 }, 0.2, 2600); V.lead(tb, ch[1] + 12, spb * 0.5, { ...o, rev: 0.3 }, 0.15, 2600); }
        for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n, len * spb * 0.9, { ...o, rev: 0.3 }, 0.17, 3000);
        for (let k = 0; k < 16; k++) V.pluck(tb + k * spb * 0.25, ch[k % 2 ? 1 : 0] + 24, o, 0.04, 0.07, 'sawtooth', 1600);
        break;
      default:
        // The Source: bright, wide, bells.
        V.pad(tb, [ch[0] + 12, ch[1] + 12, ch[2] + 12], 4 * spb, { ...o, rev: 0.8 }, 0.14, 1600);
        V.bass(tb, root, spb * 4, { ...o, rev: 0.5 }, 0.3, 300, 0);
        for (let k = 0; k < 8; k++) V.pluck(tb + k * spb * 0.5, [ch[0], ch[1], ch[2], ch[3], ch[2] + 12, ch[3], ch[2], ch[1]][k] + 12, { ...o, rev: 0.6, dly: 0.3 }, 0.08, 0.9, 'sine', 3000);
        for (const [, beat, n, len] of mel) V.bell(tb + beat * spb, n, { ...o, rev: 0.7 }, 0.12, len * spb);
        for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n - 12, len * spb * 0.95, { ...o, rev: 0.7 }, 0.08, 1800);
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
  if (kind === 'wind') {
    const a = src('bandpass', 380, 1.0, 0.12);
    const b = src('bandpass', 800, 2.5, 0.04);
    for (let t = 0; t < L + X; t += 0.7) {
      a.fl.frequency.linearRampToValueAtTime(rnd(260, 520), t + 0.7);
      a.gn.gain.linearRampToValueAtTime(rnd(0.07, 0.14), t + 0.7);
      b.gn.gain.linearRampToValueAtTime(rnd(0.01, 0.06), t + 0.7);
    }
  } else if (kind === 'storm') {
    const a = src('bandpass', 500, 0.6, 0.22);
    const b = src('bandpass', 1300, 3.0, 0.08);
    src('lowpass', 160, 0.5, 0.25);
    for (let t = 0; t < L + X; t += 0.4) {
      a.fl.frequency.linearRampToValueAtTime(rnd(300, 900), t + 0.4);
      b.fl.frequency.linearRampToValueAtTime(rnd(900, 1800), t + 0.4);
      b.gn.gain.linearRampToValueAtTime(rnd(0.02, 0.12), t + 0.4);
    }
  } else if (kind === 'forge') {
    src('lowpass', 220, 0.7, 0.3);
    src('bandpass', 900, 0.8, 0.03);
    for (let t = 0; t < L + X; t += rnd(0.03, 0.25)) V.snare(t, { dest: bus }, rnd(0.02, 0.07), rnd(600, 1400));
  } else if (kind === 'fire') {
    src('bandpass', 1200, 0.6, 0.05);
    src('lowpass', 300, 0.6, 0.12);
    for (let t = 0; t < L + X; t += rnd(0.01, 0.08)) V.snare(t, { dest: bus }, rnd(0.03, 0.12), rnd(900, 2500));
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
  ui: [0.4, (o) => V.pluck(t, 74, o, 0.2, 0.08, 'triangle', 3000)],
  uiOk: [1.0, (o) => { V.pluck(t, 74, { ...o, rev: 0.3 }, 0.25, 0.4, 'triangle', 2600); V.pluck(t + 0.09, 81, { ...o, rev: 0.3 }, 0.2, 0.5, 'triangle', 2600); }],
  voice: [0.4, (o) => { V.lead(t, 69, 0.09, o, 0.12, 1600); V.lead(t + 0.1, 66, 0.12, o, 0.1, 1400); }],
  alert: [0.7, (o) => { V.pluck(t, 81, o, 0.22, 0.1, 'square', 3000); V.pluck(t + 0.14, 81, o, 0.22, 0.12, 'square', 3000); }],
  horn: [2.6, (o) => { V.lead(t, 45, 1.6, { ...o, rev: 0.7 }, 0.35, 700); V.lead(t, 52, 1.6, { ...o, rev: 0.7 }, 0.18, 800); V.sub(t, 55, 55, 1.6, o, 0.2); }],
  bell: [2.0, (o) => { V.bell(t, 86, { ...o, rev: 0.5 }, 0.3, 1.4); V.bell(t + 0.18, 81, { ...o, rev: 0.5 }, 0.2, 1.4); }],
  caw: [0.9, (o) => { V.sweep(t, 0.22, 2200, 900, o, 0.55, 7); V.sweep(t + 0.28, 0.25, 2000, 800, o, 0.45, 7); V.pluck(t, 64, o, 0.1, 0.2, 'sawtooth', 2500); }],
  peck: [0.5, (o) => { V.snare(t, o, 0.4, 600); V.pluck(t, 84, o, 0.15, 0.04, 'square', 5000); }],
  hit_bird: [1.0, (o) => { V.snare(t, o, 0.6, 260); V.sweep(t, 0.5, 2500, 600, o, 0.4, 5); V.kick(t, o, 0.3); }],
  shout: [0.8, (o) => { V.lead(t, 57, 0.35, o, 0.3, 1400); V.sweep(t, 0.35, 700, 1200, o, 0.15, 6); }],
  harpoon: [1.2, (o) => { V.pluck(t, 40, o, 0.6, 0.5, 'sawtooth', 1100); V.pluck(t, 52, o, 0.25, 0.3, 'sawtooth', 1600); V.sweep(t + 0.02, 0.6, 500, 3200, o, 0.28, 1.5); V.kick(t, o, 0.25); }],
  thud: [0.6, (o) => { V.kick(t, o, 0.6); V.snare(t, o, 0.18, 110); }],
  salvage: [1.6, (o) => { [74, 78, 81, 86].forEach((n, i) => V.bell(t + i * 0.08, n, { ...o, rev: 0.4 }, 0.2, 0.9)); V.sweep(t, 0.6, 300, 500, o, 0.15, 8); }],
  crack: [0.8, (o) => { V.snare(t, o, 0.8, 650); V.pluck(t, 90, o, 0.2, 0.03, 'square', 6000); V.kick(t, o, 0.3); }],
  wreck: [3.0, (o) => { V.kick(t, o, 1.0); V.snare(t, { ...o, rev: 0.5 }, 0.7, 120); V.snare(t + 0.2, o, 0.4, 90); V.sub(t, 110, 35, 1.2, o, 0.7); }],
  quake: [4.0, (o) => { V.sub(t, 55, 22, 3.0, { ...o, rev: 0.7 }, 1.0); V.snare(t, { ...o, rev: 0.6 }, 0.7, 60); V.sweep(t, 2.5, 220, 70, o, 0.6, 1); V.kick(t, o, 1.0); }],
  rockfall: [3.5, (o) => { for (let i = 0; i < 9; i++) { const d = i * 0.17 + (i % 3) * 0.05; V.kick(t + d, o, 0.5 + (i % 2) * 0.3); V.snare(t + d, o, 0.3, 90 + i * 10); } V.sub(t, 70, 30, 2.0, { ...o, rev: 0.5 }, 0.6); }],
  steam: [1.8, (o) => { V.sweep(t, 1.3, 5000, 3000, o, 0.45, 0.6); V.sweep(t, 1.0, 2500, 1800, o, 0.2, 0.8); }],
  burst: [3.0, (o) => { V.snare(t, { ...o, rev: 0.6 }, 1.0, 200); V.sweep(t, 1.8, 7000, 1500, o, 0.6, 0.6); V.sub(t, 90, 30, 1.0, o, 0.8); V.kick(t, o, 1.0); }],
  ignite: [1.2, (o) => { V.sweep(t, 0.7, 300, 2400, o, 0.5, 1); V.snare(t + 0.05, o, 0.3, 300); }],
  hurt: [0.8, (o) => { V.kick(t, o, 0.6); V.lead(t, 55, 0.25, o, 0.2, 1000); V.lead(t + 0.12, 50, 0.3, o, 0.15, 900); }],
  drum: [1.6, (o) => { V.kick(t, { ...o, rev: 0.5 }, 1.0); V.snare(t, o, 0.25, 80); }],
  step: [0.3, (o) => V.snare(t, o, 0.1, 400)],
  footfall: [2.2, (o) => { V.sub(t, 58, 26, 1.4, { ...o, rev: 0.5 }, 1.0); V.kick(t, o, 0.8); V.snare(t + 0.03, o, 0.2, 60); }],
  roar: [2.6, (o) => { V.sweep(t, 1.6, 420, 140, { ...o, rev: 0.6 }, 0.9, 4); V.sweep(t, 1.4, 900, 300, o, 0.35, 6); V.sub(t, 95, 45, 1.6, o, 0.7); V.lead(t, 31, 1.4, o, 0.3, 500); }],
  impact: [3.0, (o) => { V.kick(t, o, 1.2); V.snare(t, { ...o, rev: 0.6 }, 1.0, 90); V.sub(t, 85, 24, 2.0, { ...o, rev: 0.5 }, 1.0); V.sweep(t, 0.4, 5000, 300, o, 0.4, 1); }],
  clang: [2.5, (o) => { V.bell(t, 55, { ...o, rev: 0.6 }, 0.6, 1.6); V.bell(t, 62, { ...o, rev: 0.6 }, 0.4, 1.4); V.snare(t, o, 0.6, 320); V.kick(t, o, 0.6); }],
  grab: [1.8, (o) => { V.sweep(t, 1.2, 180, 520, o, 0.6, 9); V.kick(t, o, 0.7); V.sub(t, 70, 40, 1.0, o, 0.5); }],
  fall: [5.0, (o) => { V.kick(t, o, 1.3); V.snare(t, { ...o, rev: 0.8 }, 1.0, 70); V.sub(t, 70, 20, 4.0, { ...o, rev: 0.8 }, 1.0); V.sweep(t, 3.0, 300, 60, o, 0.6, 1); V.kick(t + 0.35, o, 0.8); }],
};

const api = {
  pieces: () => [['title', 66], ['march', 84], ['fight', 126], ['source', 64]],
  piece: (n: string, bpm: number) => renderPiece(n, bpm),
  loops: () => ['wind', 'storm', 'forge', 'fire'],
  loop: (n: string) => renderLoop(n),
  sfxNames: () => Object.keys(SFX),
  sfx: (n: string) => renderOne(SFX[n][0] + 2.0, SFX[n][1]),
};
(window as unknown as { temp: typeof api }).temp = api;
