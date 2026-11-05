/**
 * MASCARADE's music and sounds, rendered offline with VOLTA's synthesiser (OfflineAudioContext in headless Chromium).
 * Music: a D minor waltz in three arrangements (salon, glasshouse, opera), a tension stem in sync with them (a
 * heartbeat and a string drone that rise as the net tightens), a music-box title, the murmur of the crowd.
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


// ---- The waltz: D minor, 3/4, 16 bars ----------------------------------------------------------------------------
const CH: Record<string, number[]> = { Dm: [62, 65, 69], Gm: [62, 67, 70], A7: [61, 64, 67], Bb: [62, 65, 70], Em7b5: [62, 64, 67, 70] };
const PROG = ['Dm', 'Dm', 'Gm', 'Gm', 'A7', 'A7', 'Dm', 'Dm', 'Bb', 'Bb', 'Gm', 'Gm', 'Em7b5', 'A7', 'Dm', 'A7'];
const ROOT: Record<string, number> = { Dm: 38, Gm: 43, A7: 45, Bb: 46, Em7b5: 40 };
// [bar, beat, note, beats]
const MEL: number[][] = [
  [0, 0, 74, 2], [0, 2, 77, 1], [1, 0, 76, 1], [1, 1, 74, 1], [1, 2, 73, 1],
  [2, 0, 74, 2], [2, 2, 70, 1], [3, 0, 72, 1], [3, 1, 70, 1], [3, 2, 69, 1],
  [4, 0, 69, 2], [4, 2, 73, 1], [5, 0, 76, 1.5], [5, 1.5, 74, 0.5], [5, 2, 73, 1],
  [6, 0, 74, 3], [7, 0, 69, 1], [7, 1, 72, 1], [7, 2, 74, 1],
  [8, 0, 77, 2], [8, 2, 74, 1], [9, 0, 72, 1], [9, 1, 74, 1], [9, 2, 77, 1],
  [10, 0, 79, 2], [10, 2, 77, 1], [11, 0, 76, 1], [11, 1, 74, 1], [11, 2, 72, 1],
  [12, 0, 70, 2], [12, 2, 74, 1], [13, 0, 73, 1], [13, 1, 76, 1], [13, 2, 79, 1],
  [14, 0, 77, 2], [14, 2, 76, 1], [15, 0, 74, 1], [15, 1, 73, 2],
];
const BARS = 16;

function waltz(kind: string, t0: number, bars: number, spb: number, o: Out): void {
  for (let b = 0; b < bars; b++) {
    const bar = b % BARS;
    const tb = t0 + b * 3 * spb;
    const c = PROG[bar];
    const ch = CH[c];
    const root = ROOT[c];
    const mel = MEL.filter((m) => m[0] === bar);
    switch (kind) {
      case 'salon':
        V.bass(tb, root, spb * 0.9, o, 0.75, 650, 0);
        for (const k of [1, 2]) for (const n of ch) V.pluck(tb + k * spb, n, o, 0.09, 0.12, 'triangle', 2600);
        V.pad(tb, ch.map((n) => n - 12), 3 * spb, { ...o, rev: 0.4 }, 0.1, 1100);
        for (const [, beat, n, len] of mel) V.lead(tb + beat * spb, n, len * spb * 0.92, { ...o, rev: 0.35 }, 0.26, 2200);
        if (bar % 2 === 0) V.bell(tb, mel[0] ? mel[0][2] + 12 : ch[2] + 12, { ...o, rev: 0.4 }, 0.08, 1.4);
        break;
      case 'serre':
        V.bass(tb, root, spb * 0.8, o, 0.5, 500, 0);
        for (let k = 0; k < 6; k++) V.pluck(tb + k * spb * 0.5, [...ch, ch[0] + 12, ch[1] + 12, ch[2] + 12][(k + bar) % 6], { ...o, rev: 0.3 }, 0.1, 0.35, 'sine', 3800);
        V.pad(tb, ch, 3 * spb, { ...o, rev: 0.5 }, 0.12, 1700);
        for (const [, beat, n] of mel) V.bell(tb + beat * spb, n + 12, { ...o, rev: 0.45, dly: 0.2 }, 0.14, 1.3);
        break;
      case 'opera':
        V.bass(tb, root, spb * 0.95, o, 0.9, 800, 0.2);
        V.bass(tb, root - 12, spb * 0.95, o, 0.4, 400, 0);
        for (const k of [1, 2]) for (const n of ch) V.pluck(tb + k * spb, n, o, 0.12, 0.1, 'sawtooth', 2000);
        V.pad(tb, [...ch, ch[0] + 12], 3 * spb, { ...o, rev: 0.5 }, 0.2, 1600);
        for (const [, beat, n, len] of mel) {
          V.lead(tb + beat * spb, n, len * spb * 0.95, { ...o, rev: 0.4 }, 0.3, 2800);
          V.lead(tb + beat * spb, n - 12, len * spb * 0.95, { ...o, rev: 0.4 }, 0.16, 1600);
        }
        if (bar % 4 === 3) V.sweep(tb + 2 * spb, spb, 800, 6000, { ...o, rev: 0.5 }, 0.07, 1);
        if (bar % 4 === 0) V.sub(tb, 90, 45, 0.5, o, 0.45);
        break;
      case 'tension':
        // A heartbeat on each bar, a low drone, a high tremolo that leans on the fifth.
        V.kick(tb, o, 0.32);
        V.kick(tb + spb * 0.42, o, 0.2);
        V.pad(tb, [root, root + 7], 3 * spb, { ...o, rev: 0.3 }, 0.16, 500);
        for (let k = 0; k < 12; k++) V.pluck(tb + k * spb * 0.25, ch[k % 2 ? 2 : 0] + 12, o, 0.045, 0.08, 'sawtooth', 1800);
        break;
      case 'title':
        V.pad(tb, ch, 3 * spb, { ...o, rev: 0.6 }, 0.16, 900);
        V.bass(tb, root, spb * 2.6, o, 0.4, 420, 0);
        for (const [, beat, n] of mel) V.bell(tb + beat * spb, n + 12, { ...o, rev: 0.5 }, 0.16, 1.6);
        break;
    }
  }
}

function canal(ctx: OfflineAudioContext, dest: AudioNode, len: number): void {
  const n = ctx.createBufferSource();
  n.buffer = (audio as unknown as { noise: AudioBuffer }).noise;
  n.loop = true;
  const f = ctx.createBiquadFilter();
  f.type = 'lowpass';
  f.frequency.value = 500;
  const g = ctx.createGain();
  g.gain.value = 0.05;
  const lfo = ctx.createOscillator();
  lfo.frequency.value = 0.15;
  const lg = ctx.createGain();
  lg.gain.value = 0.03;
  lfo.connect(lg);
  lg.connect(g.gain);
  n.connect(f);
  f.connect(g);
  g.connect(dest);
  n.start(0);
  lfo.start(0);
  n.stop(len);
  lfo.stop(len);
}

async function renderWaltz(kind: string, bpm: number): Promise<string> {
  const spb = 60 / bpm;
  const loop = BARS * 3 * spb;
  const lead = 0.05;
  const ctx = new OfflineAudioContext(2, Math.ceil((loop * 2 + lead) * SR), SR);
  const bus = setup(ctx, spb * 0.75);
  waltz(kind, lead, BARS * 2, spb, { dest: bus });
  if (kind === 'title') canal(ctx, bus, loop * 2 + lead);
  const buf = await ctx.startRendering();
  const from = Math.round((lead + loop) * SR);
  return wav(buf, from, from + Math.round(loop * SR));
}

// The murmur of a hundred guests: voices as sawtooth sources through moving formant filters, chopped into
// syllables, panned around; glasses clinking now and then. Rendered long and cross-faded into a seamless loop.
async function renderCrowd(): Promise<string> {
  const L = 16;
  const X = 2;
  const ctx = new OfflineAudioContext(2, Math.ceil((L + X) * SR), SR);
  const bus = setup(ctx);
  const rnd = (a: number, b: number) => a + Math.random() * (b - a);
  for (let v = 0; v < 22; v++) {
    const osc = ctx.createOscillator();
    osc.type = 'sawtooth';
    const f0 = rnd(95, 240);
    osc.frequency.value = f0;
    const f1 = ctx.createBiquadFilter();
    f1.type = 'bandpass';
    f1.Q.value = 4;
    const f2 = ctx.createBiquadFilter();
    f2.type = 'bandpass';
    f2.Q.value = 6;
    const g = ctx.createGain();
    g.gain.value = 0;
    const pan = ctx.createStereoPanner();
    pan.pan.value = rnd(-0.9, 0.9);
    osc.connect(f1);
    osc.connect(f2);
    f1.connect(g);
    f2.connect(g);
    g.connect(pan);
    pan.connect(bus);
    let t = rnd(0, 1.5);
    while (t < L + X) {
      // A phrase of syllables, then a pause.
      const n = Math.floor(rnd(3, 12));
      for (let k = 0; k < n && t < L + X; k++) {
        const d = rnd(0.09, 0.22);
        f1.frequency.setValueAtTime(rnd(350, 900), t);
        f2.frequency.setValueAtTime(rnd(1100, 2600), t);
        osc.frequency.setValueAtTime(f0 * rnd(0.9, 1.15), t);
        g.gain.setValueAtTime(0.0001, t);
        g.gain.linearRampToValueAtTime(rnd(0.012, 0.03), t + d * 0.3);
        g.gain.linearRampToValueAtTime(0.0001, t + d);
        t += d + rnd(0.0, 0.05);
      }
      t += rnd(0.4, 2.2);
    }
    osc.start(0);
    osc.stop(L + X);
  }
  for (let t = 0.5; t < L + X; t += rnd(0.6, 2.4)) V.bell(t, Math.floor(rnd(96, 104)), { dest: bus, rev: 0.3 }, rnd(0.02, 0.05), 0.4);
  const hum = ctx.createBufferSource();
  hum.buffer = (audio as unknown as { noise: AudioBuffer }).noise;
  hum.loop = true;
  const hf = ctx.createBiquadFilter();
  hf.type = 'bandpass';
  hf.frequency.value = 600;
  hf.Q.value = 0.6;
  const hg = ctx.createGain();
  hg.gain.value = 0.025;
  hum.connect(hf);
  hf.connect(hg);
  hg.connect(bus);
  hum.start(0);
  hum.stop(L + X);
  const buf = await ctx.startRendering();
  // Cross-fade the tail into the head.
  const n = Math.round(L * SR);
  const x = Math.round(X * SR);
  for (let c = 0; c < buf.numberOfChannels; c++) {
    const d = buf.getChannelData(c);
    for (let i = 0; i < x; i++) {
      const k = i / x;
      d[i] = d[i] * k + d[n + i] * (1 - k);
    }
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
  ui: [0.4, (o) => V.pluck(t, 81, o, 0.2, 0.06, 'triangle', 5000)],
  uiOk: [1.0, (o) => { V.bell(t, 74, { ...o, rev: 0.3 }, 0.3, 0.7); V.bell(t + 0.07, 81, { ...o, rev: 0.3 }, 0.2, 0.7); }],
  mission: [2.4, (o) => { V.sweep(t, 0.5, 300, 2400, { ...o, rev: 0.5 }, 0.08, 2); V.bell(t + 0.08, 81, { ...o, rev: 0.6 }, 0.32, 1.8); V.bell(t + 0.2, 86, { ...o, rev: 0.6 }, 0.22, 1.8); }],
  shot: [3.0, (o) => {
    V.snare(t, { ...o, rev: 0.9 }, 1.0, 140);
    V.kick(t, { ...o, rev: 0.6 }, 1.2);
    V.sub(t, 120, 30, 0.6, { ...o, rev: 0.6 }, 0.9);
    V.sweep(t, 0.08, 9000, 2000, o, 0.5, 1);
  }],
  stab: [0.8, (o) => { V.sweep(t, 0.16, 600, 4200, o, 0.22, 3); V.sub(t + 0.12, 140, 50, 0.25, o, 0.7); V.snare(t + 0.12, o, 0.2, 110); }],
  scope: [0.4, (o) => { V.pluck(t, 64, o, 0.2, 0.03, 'square', 2500); V.pluck(t + 0.05, 71, o, 0.15, 0.03, 'square', 3500); }],
  win: [3.4, (o) => { [62, 65, 69, 74, 77].forEach((n, i) => V.bell(t + i * 0.1, n, { ...o, rev: 0.5 }, 0.4, 2.0)); V.pad(t + 0.45, [62, 65, 69, 74], 1.8, { ...o, rev: 0.5 }, 0.32, 2000); }],
  lose: [3.0, (o) => { V.pad(t, [50, 53, 56], 2.2, { ...o, rev: 0.6 }, 0.35, 900); V.bell(t + 0.1, 62, { ...o, rev: 0.6 }, 0.3, 2.2); V.bell(t + 0.5, 61, { ...o, rev: 0.6 }, 0.25, 2.2); }],
  midnight: [5.0, (o) => { for (let i = 0; i < 4; i++) V.bell(t + i * 0.9, i % 2 ? 57 : 50, { ...o, rev: 0.7 }, 0.45, 3.0); }],
  calib: [0.3, (o) => V.pluck(t, 96, o, 0.25, 0.02, 'square', 9000)],
};

const api = {
  waltzes: () => ['salon', 'serre', 'opera', 'tension'],
  waltz: (n: string) => renderWaltz(n, 168),
  title: () => renderWaltz('title', 120),
  crowd: () => renderCrowd(),
  sfxNames: () => Object.keys(SFX),
  sfx: (n: string) => renderOne(SFX[n][0] + 2.0, SFX[n][1]),
};
(window as unknown as { masc: typeof api }).masc = api;
