/**
 * Offline renderer of VOLTA's synthesised audio, for the Godot port: music stems (one seamless 8-bar loop per theme,
 * tempo and intensity), the hero's notes and every sound effect, rendered with the exact same voices and buses.
 * Bundled with esbuild and run in headless Chromium (OfflineAudioContext); see tools/audio-render.cjs.
 */
import { audio } from '../src/engine/audio';
import { THEMES, music } from '../src/engine/music';
import { sfx } from '../src/engine/sfx';
import { V } from '../src/engine/synth';

const SR = 48000;

function setup(ctx: OfflineAudioContext): GainNode {
  const out = ctx.createGain();
  out.connect(ctx.destination);
  const a = audio as unknown as Record<string, unknown>;
  a.ctx = ctx;
  const music = ctx.createGain();
  music.connect(out);
  const fx = ctx.createGain();
  fx.connect(out);
  a.music = music;
  a.sfx = fx;
  const reverb = ctx.createConvolver();
  const len = Math.floor(SR * 1.8);
  const ir = ctx.createBuffer(2, len, SR);
  for (let c = 0; c < 2; c++) {
    const d = ir.getChannelData(c);
    for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 3.2);
  }
  reverb.buffer = ir;
  const reverbSend = ctx.createGain();
  reverbSend.gain.value = 0.9;
  reverbSend.connect(reverb);
  const rv = ctx.createGain();
  rv.gain.value = 0.32;
  reverb.connect(rv);
  rv.connect(out);
  a.reverb = reverb;
  a.reverbSend = reverbSend;
  const delay = ctx.createDelay(2);
  delay.delayTime.value = 0.35;
  const fb = ctx.createGain();
  fb.gain.value = 0.38;
  const dlp = ctx.createBiquadFilter();
  dlp.type = 'lowpass';
  dlp.frequency.value = 3500;
  const delaySend = ctx.createGain();
  delaySend.connect(delay);
  delay.connect(dlp);
  dlp.connect(fb);
  fb.connect(delay);
  const dv = ctx.createGain();
  dv.gain.value = 0.3;
  dlp.connect(dv);
  dv.connect(out);
  a.delay = delay;
  a.delaySend = delaySend;
  const noise = ctx.createBuffer(1, SR, SR);
  const nd = noise.getChannelData(0);
  for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;
  a.noise = noise;
  return music;
}

function wav(buf: AudioBuffer, from = 0, to = buf.length): string {
  const n = to - from;
  const ch = buf.numberOfChannels;
  const bytes = new ArrayBuffer(44 + n * ch * 2);
  const v = new DataView(bytes);
  const w = (o: number, s: string) => [...s].forEach((c, i) => v.setUint8(o + i, c.charCodeAt(0)));
  w(0, 'RIFF');
  v.setUint32(4, 36 + n * ch * 2, true);
  w(8, 'WAVE');
  w(12, 'fmt ');
  v.setUint32(16, 16, true);
  v.setUint16(20, 1, true);
  v.setUint16(22, ch, true);
  v.setUint32(24, SR, true);
  v.setUint32(28, SR * ch * 2, true);
  v.setUint16(32, ch * 2, true);
  v.setUint16(34, 16, true);
  w(36, 'data');
  v.setUint32(40, n * ch * 2, true);
  const data = [...Array(ch)].map((_, c) => buf.getChannelData(c));
  let o = 44;
  for (let i = from; i < to; i++) {
    for (let c = 0; c < ch; c++) {
      const s = Math.max(-1, Math.min(1, data[c][i]));
      v.setInt16(o, s < 0 ? s * 0x8000 : s * 0x7fff, true);
      o += 2;
    }
  }
  let bin = '';
  const u8 = new Uint8Array(bytes);
  for (let i = 0; i < u8.length; i += 0x8000) bin += String.fromCharCode(...u8.subarray(i, i + 0x8000));
  return btoa(bin);
}

/** One seamless loop of 8 bars: 16 bars are rendered and the second half kept (it carries the first's tails). */
async function renderLoop(theme: string, bpm: number, intensity: number, overdrive: boolean): Promise<string> {
  const spb = 60 / bpm;
  const bars = 8;
  const loop = bars * 4 * spb;
  const lead = 0.05;
  const ctx = new OfflineAudioContext(2, Math.ceil((loop * 2 + lead) * SR), SR);
  const bus = setup(ctx);
  const m = music as unknown as Record<string, unknown> & { step(s: number, t: number, spb: number): void };
  m.theme = THEMES[theme];
  m.bus = bus;
  m.intensity = intensity;
  m.overdrive = overdrive;
  m.silence = 0;
  for (let s = 0; s < bars * 2 * 16; s++) m.step(s, lead + (s * spb) / 4, spb);
  const buf = await ctx.startRendering();
  const from = Math.round((lead + loop) * SR);
  return wav(buf, from, from + Math.round(loop * SR));
}

async function renderOne(len: number, play: () => void): Promise<string> {
  const ctx = new OfflineAudioContext(2, Math.ceil(len * SR), SR);
  setup(ctx);
  play();
  const buf = await ctx.startRendering();
  // Trim the silent tail.
  let end = buf.length;
  const a = buf.getChannelData(0);
  const b = buf.getChannelData(1);
  while (end > SR * 0.05 && Math.abs(a[end - 1]) < 2e-4 && Math.abs(b[end - 1]) < 2e-4) end--;
  return wav(buf, 0, end);
}

type Note = { kind: string; notes: number[] };
/** The hero's notes, as played by music.play(): each distinct (kind, pitches) gets its own sample. */
function noteSamples(): Map<string, Note> {
  const out = new Map<string, Note>();
  for (const th of Object.values(THEMES)) {
    if (th.id === 'title') continue;
    for (const p of th.prog) {
      const ch = p.map((n) => n + th.root);
      const tones = [...ch, ...ch.map((n) => n + 12), ...ch.map((n) => n + 24)];
      const add = (kind: string, notes: number[]) => out.set(`${kind}_${notes.join('_')}`, { kind, notes });
      for (let i = 0; i < 3; i++) add('shot', [tones[i + 3]]);
      for (let i = 0; i < 4; i++) add('accent', [tones[3 + i] + 12]);
      add('perfect', [tones[6] + 12, tones[4] + 12]);
      add('great', [tones[5] + 12]);
      for (let i = 0; i < 5; i++) add('kill', [tones[2 + i] + 12]);
      add('bigKill', [tones[0] + 12, tones[2] + 12]);
      add('parry', [tones[4] + 24, tones[6] + 12]);
      add('graze', [tones[7] + 12]);
    }
  }
  return out;
}

function playNote(n: Note): void {
  const t = 0.01;
  const a = audio as unknown as { sfx: AudioNode };
  const o = { dest: a.sfx, dly: 0.12 };
  const [x, y] = n.notes;
  switch (n.kind) {
    case 'shot': V.pluck(t, x, o, 0.32, 0.06, 'triangle', 3500); break;
    case 'accent': V.pluck(t, x, o, 0.5, 0.1, 'square', 6000); break;
    case 'perfect': V.bell(t, x, o, 0.6, 0.9); V.pluck(t, y, o, 0.4, 0.12, 'sawtooth', 8000); break;
    case 'great': V.pluck(t, x, o, 0.4, 0.1, 'square', 6000); break;
    case 'kill': V.bell(t, x, { dest: a.sfx, rev: 0.2 }, 0.45, 0.6); break;
    case 'bigKill': V.bell(t, x, { dest: a.sfx, rev: 0.4 }, 0.7, 1.4); V.bell(t, y, { dest: a.sfx, rev: 0.4 }, 0.5, 1.4); break;
    case 'parry': V.bell(t, x, { dest: a.sfx, rev: 0.5 }, 0.6, 1.6); V.bell(t, y, { dest: a.sfx, rev: 0.5 }, 0.5, 1.6); break;
    case 'graze': V.pluck(t, x, { dest: a.sfx }, 0.15, 0.04, 'sine', 9000); break;
  }
}

const SFX = ['dash', 'pulse', 'hurt', 'die', 'boom', 'bigBoom', 'rankUp', 'overdrive', 'enemyShot', 'telegraph', 'lunge', 'mortarLaunch', 'mortar', 'sniper', 'phase', 'block', 'charge', 'heal', 'fragment', 'spawn', 'roar', 'ui', 'uiOk', 'checkpoint', 'calib'];

const api = {
  stems(): { name: string; theme: string; bpm: number; i: number; od: boolean }[] {
    const list: { name: string; theme: string; bpm: number; i: number; od: boolean }[] = [];
    const tempos: Record<string, number[]> = { seuil: [128], jardin: [136], fonderie: [140], ocean: [118, 96, 146], ville: [150], coeur: [160] };
    for (const [th, bpms] of Object.entries(tempos)) {
      for (const bpm of bpms) {
        for (let i = 1; i <= 4; i++) list.push({ name: `${th}_${bpm}_${i}`, theme: th, bpm, i, od: false });
        list.push({ name: `${th}_${bpm}_od`, theme: th, bpm, i: 4, od: true });
      }
    }
    list.push({ name: 'title_96_0', theme: 'title', bpm: 96, i: 0, od: false });
    return list;
  },
  stem: (theme: string, bpm: number, i: number, od: boolean) => renderLoop(theme, bpm, i, od),
  notes: () => [...noteSamples().keys()],
  note: (key: string) => renderOne(2.6, () => playNote(noteSamples().get(key)!)),
  sfxNames: () => SFX,
  sfx: (name: string) => renderOne(name === 'die' || name === 'overdrive' || name === 'roar' || name === 'bigBoom' ? 3.5 : 2.4, () => {
    if (name === 'calib') V.pluck(0.01, 84, { dest: (audio as unknown as { sfx: AudioNode }).sfx }, 0.6, 0.05, 'square', 8000);
    else sfx(name, 1, 1);
  }),
};
(window as unknown as { volta: typeof api }).volta = api;
