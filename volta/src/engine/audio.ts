/**
 * Audio engine: one context, a limiter, music and effect buses, a shared reverb and a tempo delay.
 * Everything is synthesised (no samples).
 */
export class AudioEngine {
  ctx: AudioContext | null = null;
  master!: GainNode;
  music!: GainNode;
  sfx!: GainNode;
  reverb!: ConvolverNode;
  reverbSend!: GainNode;
  delay!: DelayNode;
  delaySend!: GainNode;
  delayFb!: GainNode;
  analyser!: AnalyserNode;
  noise!: AudioBuffer;
  musicFilter!: BiquadFilterNode;
  private levels = new Uint8Array(64);
  volMusic = 0.7;
  volSfx = 0.8;

  init(): void {
    if (this.ctx) {
      if (this.ctx.state === 'suspended') void this.ctx.resume();
      return;
    }
    const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    const ctx = new AC({ latencyHint: 'interactive' });
    this.ctx = ctx;
    this.master = ctx.createGain();
    this.master.gain.value = 0.9;
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -10;
    comp.knee.value = 6;
    comp.ratio.value = 8;
    comp.attack.value = 0.003;
    comp.release.value = 0.15;
    this.analyser = ctx.createAnalyser();
    this.analyser.fftSize = 128;
    this.master.connect(comp);
    comp.connect(this.analyser);
    this.analyser.connect(ctx.destination);
    this.musicFilter = ctx.createBiquadFilter();
    this.musicFilter.type = 'lowpass';
    this.musicFilter.frequency.value = 20000;
    this.music = ctx.createGain();
    this.music.gain.value = this.volMusic;
    this.music.connect(this.musicFilter);
    this.musicFilter.connect(this.master);
    this.sfx = ctx.createGain();
    this.sfx.gain.value = this.volSfx;
    this.sfx.connect(this.master);
    // Reverb: a generated hall.
    this.reverb = ctx.createConvolver();
    const len = Math.floor(ctx.sampleRate * 1.8);
    const ir = ctx.createBuffer(2, len, ctx.sampleRate);
    for (let c = 0; c < 2; c++) {
      const d = ir.getChannelData(c);
      for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 3.2);
    }
    this.reverb.buffer = ir;
    this.reverbSend = ctx.createGain();
    this.reverbSend.gain.value = 0.9;
    this.reverbSend.connect(this.reverb);
    const rv = ctx.createGain();
    rv.gain.value = 0.32;
    this.reverb.connect(rv);
    rv.connect(this.master);
    // Tempo delay.
    this.delay = ctx.createDelay(2);
    this.delay.delayTime.value = 0.35;
    this.delayFb = ctx.createGain();
    this.delayFb.gain.value = 0.38;
    const dlp = ctx.createBiquadFilter();
    dlp.type = 'lowpass';
    dlp.frequency.value = 3500;
    this.delaySend = ctx.createGain();
    this.delaySend.gain.value = 1;
    this.delaySend.connect(this.delay);
    this.delay.connect(dlp);
    dlp.connect(this.delayFb);
    this.delayFb.connect(this.delay);
    const dv = ctx.createGain();
    dv.gain.value = 0.3;
    dlp.connect(dv);
    dv.connect(this.master);
    // White noise.
    this.noise = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
    const nd = this.noise.getChannelData(0);
    for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;
  }

  get now(): number {
    return this.ctx?.currentTime ?? 0;
  }

  /** Latency between scheduling and hearing (seconds). */
  get latency(): number {
    const c = this.ctx as (AudioContext & { outputLatency?: number }) | null;
    return c ? (c.outputLatency ?? 0) + c.baseLatency : 0;
  }

  setVolumes(music: number, sfx: number): void {
    this.volMusic = music;
    this.volSfx = sfx;
    if (!this.ctx) return;
    this.music.gain.setTargetAtTime(music, this.now, 0.05);
    this.sfx.gain.setTargetAtTime(sfx, this.now, 0.05);
  }

  /** Overall loudness 0..1 of the last few milliseconds. */
  level(): number {
    if (!this.ctx) return 0;
    this.analyser.getByteFrequencyData(this.levels);
    let s = 0;
    for (let i = 1; i < 24; i++) s += this.levels[i];
    return s / (23 * 255);
  }

  /** Low-pass sweep on the music (pause, death, overdrive). */
  muffle(on: boolean, freq = 600): void {
    if (!this.ctx) return;
    this.musicFilter.frequency.setTargetAtTime(on ? freq : 20000, this.now, on ? 0.08 : 0.3);
  }
}

export const audio = new AudioEngine();
