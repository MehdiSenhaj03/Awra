import * as THREE from 'three';
import { BloomEffect, ChromaticAberrationEffect, EffectComposer, EffectPass, HueSaturationEffect, NoiseEffect, RenderPass, ShockWaveEffect, ToneMappingEffect, ToneMappingMode, VignetteEffect, BlendFunction, BrightnessContrastEffect } from 'postprocessing';
import { clamp, damp, fx, TAU } from '../engine/core';
import { BK, SK } from '../game/bullets';
import type { Enemy } from '../game/enemy';
import type { Fx } from '../game/events';
import { Bulwark, Choir, Mirror } from '../game/enemies';
import type { World } from '../game/world';
import { Backdrop } from './backdrop';
import { Floor } from './floor';
import { GS, Glows } from './glows';
import { type Model, enemyModel, playerModel, setEnv } from './models';
import { Shards } from './shards';
import { Sparks } from './sparks';

export interface SectorLook {
  accent: number;
  accent2: number;
  fog: number;
  painting?: string;
}

const col = new THREE.Color();
const rgb = (c: number, k = 1): [number, number, number] => {
  col.set(c);
  return [col.r * k, col.g * k, col.b * k];
};

/** Text popups (score, PERFECT) drawn by the interface at projected positions. */
export interface Popup {
  x: number;
  y: number;
  z: number;
  text: string;
  cls: string;
  t: number;
}

/**
 * Renders a World: arena, actors, bullets and every effect. Reads the simulation, never changes it.
 */
export class View {
  renderer: THREE.WebGLRenderer;
  scene = new THREE.Scene();
  camera: THREE.PerspectiveCamera;
  composer: EffectComposer;
  private bloom: BloomEffect;
  private chroma: ChromaticAberrationEffect;
  private shocks: ShockWaveEffect[] = [];
  private shockI = 0;
  private hue: HueSaturationEffect;
  private bc: BrightnessContrastEffect;
  private floor: Floor | null = null;
  private backdrop: Backdrop | null = null;
  private glows = new Glows();
  private sparks = new Sparks();
  private shards = new Shards();
  private player: Model;
  private models = new Map<number, Model>();
  private lights: THREE.PointLight[] = [];
  private lightI = 0;
  private playerLight: THREE.PointLight;
  private key: THREE.DirectionalLight;
  private trail: { x: number; z: number; a: number }[] = [];
  private afterimages: { x: number; z: number; t: number; a: number }[] = [];
  private rings: { x: number; z: number; r: number; t: number; life: number; c: number; w: number }[] = [];
  private flashes: { x: number; z: number; r: number; t: number; life: number; c: number }[] = [];
  private arcs: { x1: number; z1: number; x2: number; z2: number; t: number }[] = [];
  popups: Popup[] = [];
  // Camera.
  private camX = 0;
  private camZ = 0;
  private trauma = 0;
  private kickX = 0;
  private kickZ = 0;
  private zoom = 1;
  zoomTarget = 1;
  private fovKick = 0;
  private chromaK = 0;
  private flashK = 0;
  private flashC = new THREE.Color();
  private odK = 0;
  private beatPulse = 0;
  time = 0;
  look: SectorLook = { accent: 0x2ef2ff, accent2: 0xff2fb9, fog: 0x02030a };
  shakeScale = 1;
  /** Music level 0..1 (from the analyser), for reactive visuals. */
  level = 0;
  private screenFlash: HTMLDivElement;
  private neonEnv: THREE.Texture;
  private stageGroup = new THREE.Group();

  /** A studio of neon strips, prefiltered: what obsidian reflects. */
  private buildEnv(): THREE.Texture {
    const s = new THREE.Scene();
    const dome = new THREE.Mesh(
      new THREE.SphereGeometry(50, 32, 16),
      new THREE.ShaderMaterial({
        side: THREE.BackSide,
        vertexShader: 'varying vec3 vP; void main(){ vP = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }',
        fragmentShader: 'varying vec3 vP; void main(){ float h = normalize(vP).y; vec3 c = mix(vec3(0.004,0.005,0.012), vec3(0.03,0.04,0.09), smoothstep(-0.2, 1.0, h)); gl_FragColor = vec4(c, 1.0); }',
      }),
    );
    s.add(dome);
    const strip = (color: number, k: number, x: number, y: number, z: number, w: number, h: number, ry = 0) => {
      const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ color: new THREE.Color(color).multiplyScalar(k), side: THREE.DoubleSide }));
      m.position.set(x, y, z);
      m.lookAt(0, 0, 0);
      m.rotateZ(ry);
      s.add(m);
    };
    strip(0x2ef2ff, 6, -30, 18, 10, 40, 1.6, 0.3);
    strip(0xff2fb9, 5, 30, 12, -15, 36, 1.4, -0.2);
    strip(0xffffff, 8, 0, 40, 0, 30, 6);
    strip(0xffc34a, 3, 10, 6, 35, 30, 1, 0.1);
    strip(0x9b5cff, 4, -20, 4, -35, 30, 1.2);
    strip(0x2ef2ff, 3, 35, 25, 20, 2, 30);
    const pm = new THREE.PMREMGenerator(this.renderer);
    const tex = pm.fromScene(s, 0.02).texture;
    pm.dispose();
    return tex;
  }

  constructor(canvas: HTMLCanvasElement, overlay: HTMLElement) {
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', stencil: false });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.5));
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.NoToneMapping;
    this.camera = new THREE.PerspectiveCamera(36, 16 / 9, 0.5, 600);
    this.scene.background = new THREE.Color(0x010104);
    this.scene.add(new THREE.AmbientLight(0x30365a, 0.55));
    this.key = new THREE.DirectionalLight(0xbcd4ff, 1.6);
    this.key.position.set(-20, 40, 10);
    this.scene.add(this.key);
    const rim = new THREE.DirectionalLight(0xff4fc8, 0.7);
    rim.position.set(25, 12, -30);
    this.scene.add(rim);
    this.playerLight = new THREE.PointLight(0x7ff6ff, 60, 16, 1.6);
    this.scene.add(this.playerLight);
    for (let i = 0; i < 6; i++) {
      const l = new THREE.PointLight(0xffffff, 0, 14, 1.8);
      this.lights.push(l);
      this.scene.add(l);
    }
    this.neonEnv = this.buildEnv();
    setEnv(this.neonEnv);
    this.player = playerModel();
    this.scene.add(this.player.root);
    this.scene.add(this.stageGroup);
    this.scene.add(this.glows.mesh, this.sparks.points, this.shards.dark, this.shards.lit);
    // Post-processing.
    this.composer = new EffectComposer(this.renderer, { frameBufferType: THREE.HalfFloatType, multisampling: 4 });
    this.composer.addPass(new RenderPass(this.scene, this.camera));
    this.bloom = new BloomEffect({ mipmapBlur: true, luminanceThreshold: 0.55, luminanceSmoothing: 0.25, intensity: 1.45, radius: 0.78, levels: 8 });
    for (let i = 0; i < 3; i++) {
      const s = new ShockWaveEffect(this.camera, new THREE.Vector3(), { speed: 1.6, maxRadius: 0.6, waveSize: 0.22, amplitude: 0.06 });
      this.shocks.push(s);
    }
    this.composer.addPass(new EffectPass(this.camera, ...this.shocks, this.bloom));
    this.chroma = new ChromaticAberrationEffect({ offset: new THREE.Vector2(0.0006, 0.0004), radialModulation: true, modulationOffset: 0.2 });
    this.composer.addPass(new EffectPass(this.camera, this.chroma));
    this.hue = new HueSaturationEffect({ saturation: 0.12, hue: 0 });
    this.bc = new BrightnessContrastEffect({ brightness: 0, contrast: 0.08 });
    const vignette = new VignetteEffect({ offset: 0.28, darkness: 0.72 });
    const noise = new NoiseEffect({ blendFunction: BlendFunction.OVERLAY, premultiply: false });
    noise.blendMode.opacity.value = 0.12;
    this.composer.addPass(new EffectPass(this.camera, this.hue, this.bc, vignette, noise, new ToneMappingEffect({ mode: ToneMappingMode.AGX })));
    this.screenFlash = document.createElement('div');
    this.screenFlash.className = 'screen-flash';
    overlay.appendChild(this.screenFlash);
  }

  resize(w: number, h: number): void {
    this.renderer.setSize(w, h, false);
    this.composer.setSize(w, h);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    this.sparks.setScale(h * 0.9);
  }

  /** Builds the arena of a new stage. */
  setStage(w: World, look: SectorLook, painting: THREE.Texture | null, env: THREE.Texture | null): void {
    this.look = look;
    void env;
    setEnv(this.neonEnv);
    this.stageGroup.clear();
    if (this.floor) this.scene.remove(this.floor.mesh);
    if (this.backdrop) this.scene.remove(this.backdrop.group);
    for (const m of this.models.values()) this.scene.remove(m.root);
    this.models.clear();
    this.scene.remove(this.player.root);
    this.player = playerModel();
    this.scene.add(this.player.root);
    const a = new THREE.Color(look.accent);
    const b = new THREE.Color(look.accent2);
    this.floor = new Floor(w.arena, a, b);
    this.scene.add(this.floor.mesh);
    this.backdrop = new Backdrop(painting, a.clone().multiplyScalar(0.5), b.clone().multiplyScalar(0.4), this.neonEnv);
    this.scene.add(this.backdrop.group);
    this.scene.fog = new THREE.Fog(look.fog, 60, 220);
    // Pillars: obsidian columns with glowing bands.
    for (const p of w.arena.pillars) {
      const g = new THREE.Group();
      const c = new THREE.Mesh(new THREE.CylinderGeometry(p.r, p.r * 1.1, 4, 8), new THREE.MeshStandardMaterial({ color: 0x07080d, metalness: 0.95, roughness: 0.14, envMap: this.neonEnv }));
      c.position.y = 2;
      const band = new THREE.Mesh(new THREE.CylinderGeometry(p.r * 1.02, p.r * 1.02, 0.12, 8, 1, true), new THREE.MeshBasicMaterial({ color: a.clone().multiplyScalar(4), toneMapped: false }));
      band.position.y = 3.2;
      g.add(c, band);
      g.position.set(p.x, 0, p.y);
      this.stageGroup.add(g);
    }
    this.shards.clear();
    this.sparks.clear();
    this.rings = [];
    this.flashes = [];
    this.afterimages = [];
    this.trail = [];
    this.popups = [];
    this.camX = w.player.x;
    this.camZ = w.player.y;
  }

  // ---- Events ---------------------------------------------------------------------------------------------

  private light(x: number, z: number, color: number, intensity: number): void {
    const l = this.lights[this.lightI];
    this.lightI = (this.lightI + 1) % this.lights.length;
    l.position.set(x, 1.6, z);
    l.color.set(color);
    l.intensity = intensity;
  }

  private shock(x: number, z: number, amp = 0.06, size = 0.22): void {
    const s = this.shocks[this.shockI];
    this.shockI = (this.shockI + 1) % this.shocks.length;
    s.position.set(x, 0.5, z);
    s.amplitude = amp;
    s.waveSize = size;
    s.explode();
  }

  private ring(x: number, z: number, r: number, c: number, life = 0.45, w = 0.12): void {
    this.rings.push({ x, z, r, t: 0, life, c, w });
  }

  private popup(x: number, z: number, text: string, cls: string): void {
    this.popups.push({ x, y: 1.5, z, text, cls, t: 0 });
    if (this.popups.length > 40) this.popups.shift();
  }

  handle(events: Fx[], w: World): void {
    for (const e of events) {
      switch (e.t) {
        case 'shot': {
          if (e.weapon.startsWith('arcLink:')) {
            const [x1, z1] = e.weapon.slice(8).split(',').map(Number);
            this.arcs.push({ x1, z1, x2: e.x, z2: e.y, t: 0 });
            this.sparks.emit(e.x, 1, e.y, 8, 0x9ff8ff, 8, { life: 0.3, size: 0.3 });
          } else if (e.weapon === 'arcMiss') {
            this.sparks.emit(e.x, 1, e.y, 4, 0x9ff8ff, 4, { life: 0.2 });
          } else {
            const big = e.accent;
            const c = e.weapon.includes('Perfect') ? 0xffffff : 0x9ff8ff;
            this.sparks.emit(e.x, 0.9, e.y, big ? 7 : 3, c, big ? 10 : 6, { dir: e.a, spread: 0.35, life: 0.18, size: 0.28 });
            this.flashes.push({ x: e.x, z: e.y, r: big ? 1.4 : 0.8, t: 0, life: 0.08, c: 0x9ff8ff });
            if (e.weapon.includes('Perfect')) {
              this.ring(e.x, e.y, 2.2, 0xffffff, 0.3, 0.15);
              this.light(e.x, e.y, 0xbff8ff, 50);
            }
          }
          break;
        }
        case 'hit':
          this.sparks.emit(e.x, 0.9, e.y, e.big ? 14 : 5, e.color, e.big ? 12 : 7, { life: 0.3, size: 0.3, up: 0.6 });
          if (e.big) this.flashes.push({ x: e.x, z: e.y, r: 1.4, t: 0, life: 0.1, c: e.color });
          break;
        case 'kill': {
          const s = e.size;
          const heavy = e.heavy;
          this.shards.burst(e.x, e.y, 0.9, Math.round(18 + s * 26), s * 1.6, 9 + s * 6, e.color, 0.45);
          this.sparks.emit(e.x, 0.9, e.y, Math.round(40 + s * 50), e.color, 14 + s * 8, { life: 0.7, size: 0.4, up: 0.8, drag: 2 });
          this.sparks.emit(e.x, 0.9, e.y, 16, 0xffffff, 20, { life: 0.25, size: 0.5 });
          this.ring(e.x, e.y, 2.5 + s * 2.5, e.color, 0.4, 0.1);
          this.ring(e.x, e.y, 1.2 + s, 0xffffff, 0.2, 0.25);
          this.flashes.push({ x: e.x, z: e.y, r: 2.5 + s * 2, t: 0, life: 0.18, c: e.color });
          this.light(e.x, e.y, e.color, heavy ? 160 : 70);
          this.floor?.ripple(e.x, e.y, heavy ? 1.6 : 0.8, this.time);
          this.trauma = Math.min(1, this.trauma + (heavy ? 0.35 : 0.12));
          if (heavy) {
            this.shock(e.x, e.y, 0.05, 0.25);
            this.chromaK = Math.max(this.chromaK, 0.5);
          }
          break;
        }
        case 'boom':
          this.sparks.emit(e.x, 0.6, e.y, Math.round(30 + e.r * 20), e.color, 6 + e.r * 6, { life: 0.5, size: 0.45 });
          this.ring(e.x, e.y, e.r, e.color, 0.35, 0.2);
          this.ring(e.x, e.y, e.r * 0.6, 0xffffff, 0.18, 0.4);
          this.flashes.push({ x: e.x, z: e.y, r: e.r * 1.3, t: 0, life: 0.2, c: e.color });
          this.light(e.x, e.y, e.color, 90);
          this.floor?.ripple(e.x, e.y, Math.min(2, e.r * 0.4), this.time);
          break;
        case 'ring':
          this.ring(e.x, e.y, e.r, e.color, (e.life ?? 20) / 60, 0.15);
          break;
        case 'spawn':
          this.ring(e.x, e.y, 2.4, e.color, w.beat.spb, 0.08);
          this.sparks.emit(e.x, 0.3, e.y, 20, e.color, 3, { life: 0.8, up: 2, size: 0.3, gravity: -4 });
          break;
        case 'dash': {
          const perf = e.judge === 'perfect';
          this.fovKick = perf ? 1 : 0.6;
          this.sparks.emit(e.x, 0.8, e.y, perf ? 30 : 14, perf ? 0xffffff : 0x2ef2ff, 10, { dir: e.a + Math.PI, spread: 0.6, life: 0.35 });
          this.ring(e.x, e.y, perf ? 2.4 : 1.5, perf ? 0xffffff : 0x2ef2ff, 0.25, 0.2);
          break;
        }
        case 'pulse': {
          const perf = e.judge === 'perfect';
          this.ring(e.x, e.y, e.r, perf ? 0xffffff : 0x2ef2ff, 0.25, 0.25);
          this.ring(e.x, e.y, e.r * 1.25, 0x2ef2ff, 0.4, 0.06);
          if (perf) this.shock(e.x, e.y, 0.05, 0.18);
          this.light(e.x, e.y, 0x7ff6ff, perf ? 120 : 50);
          break;
        }
        case 'parry':
          this.flashK = Math.max(this.flashK, 0.25);
          this.flashC.set(0x9ff8ff);
          this.sparks.emit(e.x, 1, e.y, 30 + e.n * 6, 0xffffff, 14, { life: 0.4 });
          this.popup(e.x, e.y, e.n > 1 ? `PARADE ×${e.n}` : 'PARADE', 'pp-parry');
          this.trauma = Math.min(1, this.trauma + 0.2);
          break;
        case 'graze':
          this.sparks.emit(e.x, 0.8, e.y, 5, 0xffffff, 5, { life: 0.2, size: 0.22 });
          break;
        case 'hurt':
          this.flashK = 0.6;
          this.flashC.set(0xff2040);
          this.chromaK = 1.4;
          this.ring(e.x, e.y, 4.5, 0xff3050, 0.4, 0.15);
          this.shock(e.x, e.y, 0.09, 0.3);
          this.sparks.emit(e.x, 1, e.y, 40, 0xff3050, 14, { life: 0.5 });
          break;
        case 'die':
          this.shards.burst(e.x, e.y, 1, 80, 1.6, 16, 0x2ef2ff, 0.8);
          this.sparks.emit(e.x, 1, e.y, 200, 0x9ff8ff, 22, { life: 1.2, size: 0.5 });
          this.flashK = 1;
          this.flashC.set(0xffffff);
          this.shock(e.x, e.y, 0.12, 0.4);
          break;
        case 'judge':
          if (e.judge === 'perfect') {
            this.popup(e.x, e.y, 'PARFAIT', 'pp-perfect');
            this.sparks.emit(e.x, 1.2, e.y, 12, 0xffd890, 6, { life: 0.4, up: 1.5, size: 0.3 });
          } else this.popup(e.x, e.y, 'BIEN', 'pp-great');
          break;
        case 'score':
          if (e.value >= 200) this.popup(e.x, e.y, `+${e.value}`, e.value >= 1000 ? 'pp-big' : 'pp-score');
          break;
        case 'note':
          this.sparks.emit(e.x, 0.8, e.y, 4, 0xffc34a, 3, { life: 0.25, size: 0.25 });
          break;
        case 'heal':
          this.ring(e.x, e.y, 2, 0x40ffb0, 0.4, 0.2);
          this.popup(e.x, e.y, '+1', 'pp-heal');
          break;
        case 'overdrive':
          if (e.on) {
            this.shock(w.player.x, w.player.y, 0.14, 0.5);
            this.ring(w.player.x, w.player.y, 12, 0xffd890, 0.7, 0.05);
            this.chromaK = 1;
          }
          break;
        case 'shake':
          this.trauma = Math.min(1, this.trauma + e.a);
          break;
        case 'flash':
          this.flashK = Math.max(this.flashK, e.a);
          this.flashC.set(e.color);
          break;
        case 'chroma':
          this.chromaK = Math.max(this.chromaK, e.a);
          break;
        case 'bossDown':
          this.shock(e.x, e.y, 0.15, 0.6);
          this.flashK = 1;
          this.flashC.set(0xffffff);
          break;
        default:
          break;
      }
    }
  }

  onBeat(strong: boolean): void {
    this.beatPulse = strong ? 1 : 0.6;
  }

  // ---- Frame ----------------------------------------------------------------------------------------------

  render(w: World, alpha: number, dt: number, skipDraw = false): void {
    this.time += dt;
    const p = w.player;
    const px = p.px + (p.x - p.px) * alpha;
    const pz = p.py + (p.y - p.py) * alpha;
    this.odK += ((p.overdrive ? 1 : 0) - this.odK) * damp(4, dt);
    this.beatPulse *= Math.exp(-dt * 5);
    // Camera: follows the hero with a slight lead towards the aim, frames bosses wider.
    const lead = 3.2;
    const tx = px + Math.cos(p.aim) * lead;
    const tz = pz + Math.sin(p.aim) * lead * 0.8;
    this.camX += (tx - this.camX) * damp(5, dt);
    this.camZ += (tz - this.camZ) * damp(5, dt);
    const ext = w.arena.extent;
    this.camX = clamp(this.camX, -ext + 8, ext - 8);
    this.camZ = clamp(this.camZ, -ext + 6, ext - 4);
    this.zoom += (this.zoomTarget - this.zoom) * damp(2, dt);
    this.trauma = Math.max(0, this.trauma - dt * 1.6);
    const sh = this.trauma * this.trauma * this.shakeScale;
    const t = this.time;
    const ox = (Math.sin(t * 61) + Math.sin(t * 37.3)) * 0.5 * sh * 0.9 + this.kickX;
    const oz = (Math.sin(t * 53.1) + Math.sin(t * 29.7)) * 0.5 * sh * 0.9 + this.kickZ;
    const height = 21 * this.zoom;
    const back = 12.5 * this.zoom;
    this.camera.position.set(this.camX + ox, height, this.camZ + back + oz);
    this.camera.lookAt(this.camX + ox * 0.5, 0, this.camZ + oz * 0.5);
    this.camera.rotation.z += (Math.sin(t * 47) * sh * 0.012);
    this.fovKick *= Math.exp(-dt * 6);
    this.camera.fov = 36 + this.fovKick * 3 + this.odK * 4;
    this.camera.updateProjectionMatrix();
    // Player model.
    this.updatePlayer(w, px, pz, dt);
    // Enemies.
    this.updateEnemies(w, alpha, dt);
    // Bullets, shots, hazards, rings.
    this.drawGlows(w, alpha, dt, px, pz);
    this.sparks.update(dt);
    this.shards.update(dt);
    for (const l of this.lights) l.intensity *= Math.exp(-dt * 9);
    // Floor and backdrop.
    this.floor?.update(t, w.beat.beat, this.beatPulse, this.level, px, pz, this.odK);
    this.backdrop?.update(t, dt, this.level);
    // Post.
    this.chromaK *= Math.exp(-dt * 4);
    const ca = 0.0006 + this.chromaK * 0.006 + this.odK * 0.0015;
    this.chroma.offset.set(ca, ca * 0.6);
    this.bloom.intensity = 1.35 + this.beatPulse * 0.25 + this.odK * 0.6;
    this.hue.saturation = 0.12 + this.odK * 0.15;
    this.hue.hue = this.odK * 0.08;
    this.bc.brightness = this.odK * 0.04;
    this.flashK *= Math.exp(-dt * 7);
    this.screenFlash.style.opacity = String(Math.min(0.85, this.flashK));
    this.screenFlash.style.background = `#${this.flashC.getHexString()}`;
    this.kickX *= Math.exp(-dt * 12);
    this.kickZ *= Math.exp(-dt * 12);
    if (!skipDraw) this.composer.render(dt);
  }

  private updatePlayer(w: World, px: number, pz: number, dt: number): void {
    const p = w.player;
    const m = this.player;
    m.root.position.set(px, 0, pz);
    m.root.scale.setScalar(1.25);
    const body = m.parts.body;
    const t = this.time;
    body.position.y = Math.sin(t * 3) * 0.06;
    m.parts.aim.rotation.y = -p.aim;
    m.parts.halo.rotation.z += dt * 2;
    m.parts.halo.scale.setScalar(1 + this.beatPulse * 0.25);
    m.parts.shards.rotation.y += dt * (2.5 + this.odK * 5);
    // Lean into motion.
    body.rotation.z = -p.vx * 0.012;
    body.rotation.x = p.vy * 0.012;
    // Invulnerability blink.
    m.root.visible = !p.dead && !(p.inv > 0 && Math.floor(p.inv / 4) % 2 === 0);
    this.playerLight.position.set(px, 2.2, pz);
    this.playerLight.intensity = 55 + this.beatPulse * 30 + this.odK * 60;
    this.playerLight.color.set(p.overdrive ? 0xffe0a0 : 0x7ff6ff);
    // Ribbon trail and dash afterimages.
    this.trail.unshift({ x: px, z: pz, a: p.aim });
    if (this.trail.length > 18) this.trail.pop();
    if (p.dashing && w.frame % 2 === 0) this.afterimages.push({ x: px, z: pz, t: 0, a: p.dashA });
    if (p.overdrive && w.frame % 3 === 0) this.sparks.emit(px, 1.2, pz, 2, 0xffd890, 3, { life: 0.6, up: 2, gravity: -3, size: 0.3 });
  }

  private updateEnemies(w: World, alpha: number, dt: number): void {
    const seen = new Set<number>();
    for (const e of w.enemies) {
      seen.add(e.id);
      let m = this.models.get(e.id);
      if (!m) {
        m = enemyModel(e.look, e.r * 1.35, e.color, e.elite);
        this.models.set(e.id, m);
        this.scene.add(m.root);
      }
      this.poseEnemy(e, m, alpha, dt, w);
    }
    for (const [id, m] of this.models) {
      if (!seen.has(id)) {
        this.scene.remove(m.root);
        this.models.delete(id);
      }
    }
  }

  private poseEnemy(e: Enemy, m: Model, alpha: number, dt: number, w: World): void {
    const x = e.px + (e.x - e.px) * alpha;
    const z = e.py + (e.y - e.py) * alpha;
    m.root.position.set(x, 0, z);
    // Spawning: grows out of a column of light.
    const spawnK = e.spawnT > 0 ? 1 - e.spawnT / Math.max(1, w.beat.spb * 60) : 1;
    m.root.scale.setScalar(e.spawnT > 0 ? spawnK * spawnK : 1);
    if (e.spawnT > 0 && w.frame % 3 === 0) this.sparks.emit(x, 0.2, z, 2, e.color, 2, { life: 0.5, up: 4, gravity: -6, size: 0.3 });
    const k = e.flash;
    m.flash.forEach((mat, i) => {
      mat.color.copy(m.base[i]);
      if (k > 0) mat.color.lerp(new THREE.Color(8, 8, 8), k);
      if (e.shielded > 0) mat.color.lerp(new THREE.Color(0.5, 4, 2), 0.4);
    });
    for (const r of m.rims) {
      r.u.value.copy(r.base);
      if (k > 0) r.u.value.lerp(new THREE.Color(10, 10, 10), k);
      if (e.shielded > 0) r.u.value.lerp(new THREE.Color(0.5, 4, 2), 0.5);
    }
    for (const f of m.fade) {
      f.transparent = true;
      f.opacity = e.vis;
    }
    if (m.parts.spin) {
      m.parts.spin.rotation.y += dt * (1.2 + e.spin * 0.5);
      if (e.look === 'mote') m.parts.spin.rotation.x += dt * 3;
    }
    if (m.parts.face) m.parts.face.rotation.y = -e.rot;
    if (e instanceof Bulwark && m.parts.shield) {
      m.parts.shield.parent!.rotation.y = -e.shieldA;
      m.parts.shield.rotation.y = 0;
    }
    if (e instanceof Mirror) {
      const open = e.spin > 0.5;
      m.flash.forEach((mat) => mat.color.multiplyScalar(open ? 2 : 0.4));
    }
    if (m.parts.crown) m.parts.crown.rotation.y += dt * 2;
    if (m.parts.ring) m.parts.ring.rotation.z += dt;
    if (e.stun > 0) m.root.rotation.y = Math.sin(this.time * 40) * 0.1;
    else m.root.rotation.y = 0;
    if (e instanceof Choir) {
      for (const id of e.links) {
        const o = w.enemyById(id);
        if (!o || !o.active) continue;
        const dx = o.x - x;
        const dz = o.y - z;
        const len = Math.hypot(dx, dz);
        const [r, g, b] = rgb(0x40ffb0, 1.6);
        this.glows.add(GS.Beam, x + dx / 2, z + dz / 2, 1.0, Math.atan2(dz, dx), len / 2, 0.18, r, g, b);
      }
    }
  }

  private drawGlows(w: World, alpha: number, dt: number, px: number, pz: number): void {
    const g = this.glows;
    g.begin();
    // Enemy bullets.
    const b = w.bullets;
    const ex = (1 - alpha) / 60;
    for (let i = 0; i < b.n; i++) {
      const x = b.x[i] - b.vx[i] * ex * w.k;
      const z = b.y[i] - b.vy[i] * ex * w.k;
      const [r, gg, bb] = rgb(b.color[i], b.flags[i] === 2 ? 2.5 : 2);
      const rad = b.r[i];
      const wait = b.wait[i] > 0;
      if (b.kind[i] === BK.Shard) {
        g.add(GS.Streak, x, z, 0.7, Math.atan2(b.vy[i], b.vx[i]), rad * 3, rad * 1.6, r, gg, bb);
      } else {
        const s = rad * (b.kind[i] === BK.Big ? 2.4 : 2.2) * (wait ? 0.8 + Math.sin(this.time * 20) * 0.1 : 1);
        g.add(GS.Orb, x, z, 0.7, 0, s, s, r, gg, bb);
      }
    }
    // Soft pools of light under the foes.
    for (const e of w.enemies) {
      if (e.dead) continue;
      const [r, gg, bb] = rgb(e.color, 0.35 * e.vis);
      g.add(GS.Disc, e.x, e.y, 0.05, 0, e.r * 2.6, e.r * 2.6, r, gg, bb);
    }
    // Player shots.
    const s = w.shots;
    for (let i = 0; i < s.n; i++) {
      const x = s.x[i] - s.vx[i] * ex;
      const z = s.y[i] - s.vy[i] * ex;
      const a = Math.atan2(s.vy[i], s.vx[i]);
      const k = s.kind[i];
      if (k === SK.Lance) g.add(GS.Beam, x, z, 0.9, a, 2.4, 0.5, 2.5, 4, 5);
      else if (k === SK.Perfect) g.add(GS.Streak, x, z, 0.9, a, 1.3, 0.36, 4, 4, 4);
      else if (k === SK.Accent) g.add(GS.Streak, x, z, 0.9, a, 1.2, 0.34, 1.2, 3.6, 4);
      else if (k === SK.Missile) g.add(GS.Orb, x, z, 0.9, 0, 0.5, 0.5, 4, 2.2, 0.6);
      else if (k === SK.Pellet) g.add(GS.Streak, x, z, 0.9, a, 0.5, 0.2, 0.8, 3, 3.6);
      else g.add(GS.Streak, x, z, 0.9, a, 0.85, 0.22, 0.6, 2.6, 3.2);
    }
    // Hazards.
    for (const h of w.hazards) {
      const [r, gg, bb] = rgb(h.color, 1);
      if (h.kind === 'zone') {
        g.add(GS.Zone, h.x, h.y, 0.08, 0, h.r, h.r, r * 2, gg * 2, bb * 2, h.k);
      } else {
        const dx = h.x2 - h.x;
        const dz = h.y2 - h.y;
        const len = Math.hypot(dx, dz) / 2;
        const a = Math.atan2(dz, dx);
        if (h.kind === 'beam' && h.live) g.add(GS.Beam, h.x + dx / 2, h.y + dz / 2, 0.9, a, len, h.r * 2.4, r * 3, gg * 3, bb * 3);
        else g.add(GS.Warn, h.x + dx / 2, h.y + dz / 2, 0.15, a, len, Math.max(0.2, h.r * 1.6), r * 2, gg * 2, bb * 2, h.k);
      }
    }
    // Pickups.
    for (const q of w.pickups) {
      const c = q.kind === 'heal' ? [0.6, 4, 2] : q.kind === 'fragment' ? [5, 4, 2] : [3, 2, 0.6];
      const sz = q.kind === 'note' ? 0.35 : 0.8;
      g.add(GS.Orb, q.x, q.y, 0.6 + Math.sin(this.time * 6 + q.t) * 0.1, 0, sz, sz, c[0], c[1], c[2]);
    }
    // Rings and flashes.
    for (let i = this.rings.length - 1; i >= 0; i--) {
      const r = this.rings[i];
      r.t += dt;
      const k = r.t / r.life;
      if (k >= 1) {
        this.rings.splice(i, 1);
        continue;
      }
      const rad = r.r * (0.3 + 0.7 * Math.sqrt(k));
      const [cr, cg, cb] = rgb(r.c, 3 * (1 - k));
      g.add(GS.Ring, r.x, r.z, 0.12, 0, rad, rad, cr, cg, cb, r.w);
    }
    for (let i = this.flashes.length - 1; i >= 0; i--) {
      const f = this.flashes[i];
      f.t += dt;
      const k = f.t / f.life;
      if (k >= 1) {
        this.flashes.splice(i, 1);
        continue;
      }
      const [cr, cg, cb] = rgb(f.c, 3 * (1 - k));
      g.add(GS.Disc, f.x, f.z, 0.5, 0, f.r, f.r, cr, cg, cb);
    }
    for (let i = this.arcs.length - 1; i >= 0; i--) {
      const a = this.arcs[i];
      a.t += dt;
      if (a.t > 0.12) {
        this.arcs.splice(i, 1);
        continue;
      }
      // Jagged lightning: three segments with random offsets.
      let x0 = a.x1;
      let z0 = a.z1;
      for (let k = 1; k <= 3; k++) {
        const tk = k / 3;
        const x1 = a.x1 + (a.x2 - a.x1) * tk + (k < 3 ? fx.range(-0.5, 0.5) : 0);
        const z1 = a.z1 + (a.z2 - a.z1) * tk + (k < 3 ? fx.range(-0.5, 0.5) : 0);
        const dx = x1 - x0;
        const dz = z1 - z0;
        g.add(GS.Beam, x0 + dx / 2, z0 + dz / 2, 1, Math.atan2(dz, dx), Math.hypot(dx, dz) / 2, 0.22, 1.5, 3.5, 5);
        x0 = x1;
        z0 = z1;
      }
    }
    // The hero: a ribbon of light behind, dash afterimages, pulse readiness, beat ring.
    const p = w.player;
    if (!p.dead) {
      for (let i = 1; i < this.trail.length; i++) {
        const a = this.trail[i - 1];
        const c = this.trail[i];
        const dx = c.x - a.x;
        const dz = c.z - a.z;
        const len = Math.hypot(dx, dz);
        if (len < 0.01) continue;
        const k = 1 - i / this.trail.length;
        g.add(GS.Streak, a.x + dx / 2, a.z + dz / 2, 1.1, Math.atan2(dz, dx), len / 2 + 0.2, 0.25 * k + 0.05, 0.4 * k, 2.4 * k, 3 * k);
      }
      for (let i = this.afterimages.length - 1; i >= 0; i--) {
        const a = this.afterimages[i];
        a.t += dt;
        if (a.t > 0.3) {
          this.afterimages.splice(i, 1);
          continue;
        }
        const k = 1 - a.t / 0.3;
        g.add(GS.Disc, a.x, a.z, 1, 0, 1.1, 1.1, 0.5 * k, 2.5 * k, 3 * k);
      }
      // The beat ring: closes in on the hero and flashes on every beat (the rhythm cue).
      const ph = w.beat.phase();
      const ringR = 1.15 + (1 - ph) * 1.6;
      const near = 1 - Math.min(1, Math.abs(w.beat.offsetSec()) / 0.12);
      g.add(GS.Ring, px, pz, 0.1, 0, ringR, ringR, 0.15 + near * 2, 0.5 + near * 2, 0.7 + near * 2, 0.04 + near * 0.06);
      // Dash charges: small pips under the hero.
      for (let i = 0; i < p.maxDash; i++) {
        const a = Math.PI / 2 + (i - (p.maxDash - 1) / 2) * 0.35;
        const on = i < p.dashCharges;
        g.add(GS.Orb, px + Math.cos(a) * 1.5, pz + Math.sin(a) * 1.5, 0.1, 0, 0.16, 0.16, on ? 1 : 0.15, on ? 3 : 0.3, on ? 3.5 : 0.4);
      }
      if (p.charge > 0) {
        const c = p.charge;
        g.add(GS.Ring, px, pz, 0.15, 0, 1.6 - c * 0.15, 1.6 - c * 0.15, 3, 3, 4, 0.08 + c * 0.03);
      }
      if (p.pulseT > 0) {
        g.add(GS.Ring, px, pz, 0.2, 0, p.pulseR, p.pulseR, 1, 3.5, 4, 0.12);
      }
      // Aim line.
      g.add(GS.Warn, px + Math.cos(p.aim) * 4, pz + Math.sin(p.aim) * 4, 0.12, p.aim, 2.6, 0.12, 0.3, 0.8, 1, 0.2);
    }
    g.end(this.time);
  }

  /** Projects popups to screen space for the interface. */
  project(x: number, y: number, z: number, w: number, h: number): { x: number; y: number } {
    const v = new THREE.Vector3(x, y, z).project(this.camera);
    return { x: (v.x * 0.5 + 0.5) * w, y: (-v.y * 0.5 + 0.5) * h };
  }

  /** World point under a screen position (on the floor plane). */
  pick(sx: number, sy: number, w: number, h: number): { x: number; y: number } | null {
    const ndc = new THREE.Vector2((sx / w) * 2 - 1, -(sy / h) * 2 + 1);
    const ray = new THREE.Raycaster();
    ray.setFromCamera(ndc, this.camera);
    const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), -0.9);
    const out = new THREE.Vector3();
    if (!ray.ray.intersectPlane(plane, out)) return null;
    return { x: out.x, y: out.z };
  }

  kick(x: number, z: number): void {
    this.kickX += x;
    this.kickZ += z;
  }
}

export { TAU };
