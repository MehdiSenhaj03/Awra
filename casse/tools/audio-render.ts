/**
 * CASSE's music and sounds, rendered offline with VOLTA's synthesiser (OfflineAudioContext in headless Chromium).
 * Music: a noir jazz loop where every echo adds an instrument (clock, bass, brushes, Rhodes, vibes, brass, choir),
 * each instrument rendered as its own seamless 8-bar stem. Bundled with esbuild; see audio-render.cjs.
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

// ---- The tune: D minor, 8 bars --------------------------------------------------------------------------------
const VOICINGS = [[53, 57, 60, 64], [58, 62, 65, 69], [53, 57, 58, 62], [55, 58, 61, 64], [53, 57, 60, 64], [58, 62, 65, 69], [55, 58, 62, 64], [55, 57, 61, 64]];
const ROOTS = [38, 43, 46, 45, 38, 43, 40, 45];
// Brass motif: [bar, beat, note, beats].
const MELODY: number[][] = [
  [0, 0.5, 69, 0.5], [0, 1, 72, 0.5], [0, 1.5, 74, 1.5], [1, 0, 77, 1], [1, 1.5, 76, 0.5], [1, 2, 74, 1.5],
  [2, 1, 72, 0.5], [2, 1.5, 74, 0.5], [2, 2, 69, 2], [3, 0.5, 70, 0.5], [3, 1, 73, 1], [3, 2.5, 76, 1],
  [4, 0.5, 69, 0.5], [4, 1, 72, 0.5], [4, 1.5, 74, 1.5], [5, 0, 79, 1], [5, 1.5, 77, 0.5], [5, 2, 74, 1.5],
  [6, 1, 76, 0.5], [6, 1.5, 74, 0.5], [6, 2, 70, 1.5], [7, 0, 73, 1], [7, 1.5, 69, 2],
];

function layer(name: string, t0: number, bars: number, spb: number, o: Out): void {
  const sw = spb * 2 / 3; // swing
  for (let b = 0; b < bars; b++) {
    const bar = b % 8;
    const tb = t0 + b * 4 * spb;
    const ch = VOICINGS[bar];
    const root = ROOTS[bar];
    const next = ROOTS[(bar + 1) % 8];
    switch (name) {
      case 'clock':
        for (let k = 0; k < 4; k++) V.pluck(tb + k * spb, k % 2 ? 91 : 96, o, 0.22, 0.03, 'sine', 9000);
        V.sub(tb, 90, 50, 0.3, o, 0.25);
        V.pad(tb, ch.map((n) => n), 4 * spb, o, 0.18, 700);
        break;
      case 'bass': {
        const walk = [root, root + (bar % 2 ? 7 : 3), root + 7, next + (next > root ? -1 : 1)];
        for (let k = 0; k < 4; k++) V.bass(tb + k * spb, walk[k], spb * 0.9, o, 0.9, 700, 0);
        break;
      }
      case 'drums':
        for (let k = 0; k < 4; k++) {
          V.hat(tb + k * spb, o, 0.32, false);
          V.hat(tb + k * spb + sw, o, 0.14, false);
          if (k % 2 === 1) V.snare(tb + k * spb, o, 0.22, 170);
        }
        V.kick(tb, o, 0.45);
        if (bar % 2 === 1) V.kick(tb + 2.5 * spb, o, 0.3);
        break;
      case 'rhodes':
        for (const at of [0, 1.5 + (bar % 2) * 1]) for (const n of ch) V.bell(tb + at * spb, n, { ...o, rev: 0.25 }, 0.16, 1.1);
        break;
      case 'vibes':
        for (let k = 0; k < 8; k++) {
          const n = [...ch, ...ch.map((x) => x + 12)][(k + bar) % 8] + 12;
          V.pluck(tb + Math.floor(k / 2) * spb + (k % 2 ? sw : 0), n, { ...o, dly: 0.25 }, 0.2, 0.12, 'sine', 6000);
        }
        break;
      case 'brass':
        for (const [mb, beat, n, len] of MELODY) if (mb === bar) V.lead(tb + beat * spb, n, len * spb * 0.95, { ...o, rev: 0.2 }, 0.45, 2600);
        break;
      case 'choir':
        V.pad(tb, ch.map((n) => n + 12), 4 * spb, { ...o, rev: 0.6 }, 0.32, 1800);
        break;
      case 'title':
        V.pad(tb, ch.map((n) => n), 4 * spb, { ...o, rev: 0.5 }, 0.3, 1100);
        for (const n of ch) V.bell(tb + (bar % 2) * spb, n, { ...o, rev: 0.4 }, 0.12, 2.0);
        V.bass(tb, root, spb * 3.5, o, 0.6, 500, 0);
        if (bar % 2 === 0) V.lead(tb + 2 * spb, MELODY.find((m) => m[0] === bar)?.[2] ?? 69, spb * 1.8, { ...o, rev: 0.4 }, 0.18, 1500);
        V.pluck(tb + spb * 3.5, ch[3] + 12, { ...o, dly: 0.3 }, 0.12, 0.2, 'sine', 4000);
        break;
    }
  }
}

function rain(ctx: OfflineAudioContext, dest: AudioNode, len: number): void {
  const n = ctx.createBufferSource();
  n.buffer = (audio as unknown as { noise: AudioBuffer }).noise;
  n.loop = true;
  const f = ctx.createBiquadFilter();
  f.type = 'bandpass';
  f.frequency.value = 2400;
  f.Q.value = 0.4;
  const g = ctx.createGain();
  g.gain.value = 0.05;
  n.connect(f);
  f.connect(g);
  g.connect(dest);
  n.start(0);
  n.stop(len);
}

async function renderLayer(name: string, bpm: number): Promise<string> {
  const spb = 60 / bpm;
  const bars = 8;
  const loop = bars * 4 * spb;
  const lead = 0.05;
  const ctx = new OfflineAudioContext(2, Math.ceil((loop * 2 + lead) * SR), SR);
  const bus = setup(ctx, spb * 0.75);
  layer(name, lead, bars * 2, spb, { dest: bus });
  if (name === 'title') rain(ctx, bus, loop * 2 + lead);
  const buf = await ctx.startRendering();
  const from = Math.round((lead + loop) * SR);
  return wav(buf, from, from + Math.round(loop * SR));
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
  ui: [0.4, (o) => V.pluck(t, 84, o, 0.25, 0.05, 'sine', 6000)],
  uiOk: [1.0, (o) => { V.bell(t, 79, { ...o, rev: 0.3 }, 0.35, 0.6); V.bell(t + 0.06, 86, { ...o, rev: 0.3 }, 0.2, 0.6); }],
  block: [0.4, (o) => V.pluck(t, 40, o, 0.35, 0.08, 'square', 900)],
  door_open: [0.8, (o) => { V.sweep(t, 0.35, 300, 1400, o, 0.25, 2); V.sub(t, 110, 70, 0.25, o, 0.35); }],
  door_close: [0.6, (o) => { V.sub(t, 120, 45, 0.22, o, 0.6); V.snare(t, o, 0.2, 120); }],
  plate: [0.5, (o) => { V.sub(t, 160, 80, 0.12, o, 0.45); V.pluck(t, 55, o, 0.2, 0.05, 'triangle', 2000); }],
  click: [0.3, (o) => V.pluck(t, 96, o, 0.25, 0.02, 'square', 9000)],
  crack: [2.0, (o) => { V.sub(t, 80, 40, 0.3, o, 0.5); V.bell(t + 0.1, 67, { ...o, rev: 0.4 }, 0.5, 1.3); V.bell(t + 0.22, 74, { ...o, rev: 0.4 }, 0.4, 1.3); V.bell(t + 0.34, 79, { ...o, rev: 0.4 }, 0.35, 1.3); }],
  hack: [0.8, (o) => { [88, 93, 100].forEach((n, i) => V.pluck(t + i * 0.07, n, o, 0.22, 0.05, 'square', 7000)); }],
  pick: [1.0, (o) => { V.bell(t, 86, { ...o, rev: 0.3 }, 0.3, 0.7); V.bell(t + 0.05, 93, { ...o, rev: 0.3 }, 0.2, 0.7); }],
  takedown: [0.7, (o) => { V.sub(t, 110, 35, 0.28, o, 0.8); V.snare(t, o, 0.35, 110); }],
  coin: [1.2, (o) => { V.bell(t, 96, { ...o, rev: 0.4 }, 0.35, 0.5); V.bell(t + 0.11, 103, { ...o, rev: 0.4 }, 0.25, 0.5); V.bell(t + 0.19, 99, { ...o, rev: 0.4 }, 0.15, 0.4); }],
  throw: [0.4, (o) => V.sweep(t, 0.18, 1200, 5000, o, 0.12, 3)],
  spot: [0.6, (o) => { V.pluck(t, 76, o, 0.3, 0.08, 'triangle', 4000); V.pluck(t + 0.1, 81, o, 0.3, 0.12, 'triangle', 4000); }],
  alert: [0.6, (o) => { V.pluck(t, 72, o, 0.25, 0.08, 'triangle', 3000); V.pluck(t + 0.12, 70, o, 0.2, 0.12, 'triangle', 3000); }],
  alarm: [2.2, (o) => { for (let i = 0; i < 4; i++) V.lead(t + i * 0.42, i % 2 ? 72 : 79, 0.38, { ...o, rev: 0.3 }, 0.5, 3000); V.sub(t, 70, 40, 0.8, o, 0.6); }],
  win: [3.0, (o) => { [62, 66, 69, 74, 78].forEach((n, i) => V.bell(t + i * 0.09, n, { ...o, rev: 0.5 }, 0.45, 2.0)); V.pad(t + 0.4, [62, 66, 69, 74], 1.6, { ...o, rev: 0.5 }, 0.35, 2200); }],
  medal: [2.0, (o) => { V.bell(t, 86, { ...o, rev: 0.5 }, 0.45, 1.4); V.bell(t + 0.15, 91, { ...o, rev: 0.5 }, 0.35, 1.4); V.bell(t + 0.3, 98, { ...o, rev: 0.5 }, 0.3, 1.4); }],
  loop: [1.4, (o) => { V.sweep(t, 0.6, 5000, 200, { ...o, rev: 0.5 }, 0.25, 4); V.bell(t + 0.5, 62, { ...o, rev: 0.6 }, 0.35, 1.0); }],
  rewind: [0.5, (o) => V.sweep(t, 0.3, 3000, 400, o, 0.18, 6)],
};

const api = {
  layers: () => ['clock', 'bass', 'drums', 'rhodes', 'vibes', 'brass', 'choir'],
  layer: (n: string) => renderLayer(n, 120),
  title: () => renderLayer('title', 84),
  sfxNames: () => Object.keys(SFX),
  sfx: (n: string) => renderOne(SFX[n][0] + 2.0, SFX[n][1]),
};
(window as unknown as { casse: typeof api }).casse = api;
