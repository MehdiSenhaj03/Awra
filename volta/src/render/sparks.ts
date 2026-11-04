import * as THREE from 'three';
import { fx } from '../engine/core';

/** Additive point sparks (thousands): hot white cores fading to their colour, with drag and optional gravity. */
export class Sparks {
  points: THREE.Points;
  private max: number;
  n = 0;
  private pos: Float32Array;
  private col: Float32Array;
  private size: Float32Array;
  private V: Float32Array;
  private L: Float32Array;
  private L0: Float32Array;
  private C: Float32Array;
  private S0: Float32Array;
  private G: Float32Array;
  private D: Float32Array;
  private tmp = new THREE.Color();

  constructor(max = 5000) {
    this.max = max;
    this.pos = new Float32Array(max * 3);
    this.col = new Float32Array(max * 3);
    this.size = new Float32Array(max);
    this.V = new Float32Array(max * 3);
    this.L = new Float32Array(max);
    this.L0 = new Float32Array(max);
    this.C = new Float32Array(max * 3);
    this.S0 = new Float32Array(max);
    this.G = new Float32Array(max);
    this.D = new Float32Array(max);
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(this.pos, 3).setUsage(THREE.DynamicDrawUsage));
    geo.setAttribute('color', new THREE.BufferAttribute(this.col, 3).setUsage(THREE.DynamicDrawUsage));
    geo.setAttribute('size', new THREE.BufferAttribute(this.size, 1).setUsage(THREE.DynamicDrawUsage));
    const mat = new THREE.ShaderMaterial({
      transparent: true,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
      uniforms: { uScale: { value: 600 } },
      vertexShader: /* glsl */ `
        attribute float size;
        attribute vec3 color;
        varying vec3 vC;
        uniform float uScale;
        void main() {
          vC = color;
          vec4 mv = modelViewMatrix * vec4(position, 1.0);
          gl_PointSize = size * uScale / -mv.z;
          gl_Position = projectionMatrix * mv;
        }`,
      fragmentShader: /* glsl */ `
        varying vec3 vC;
        void main() {
          vec2 d = gl_PointCoord - 0.5;
          float r = length(d) * 2.0;
          float a = exp(-r * r * 4.0);
          float core = exp(-r * r * 24.0);
          gl_FragColor = vec4(vC * a + vec3(core) * length(vC) * 0.35, 1.0);
        }`,
    });
    this.points = new THREE.Points(geo, mat);
    this.points.frustumCulled = false;
    geo.setDrawRange(0, 0);
  }

  setScale(px: number): void {
    (this.points.material as THREE.ShaderMaterial).uniforms.uScale.value = px;
  }

  emit(x: number, y: number, z: number, count: number, color: number, speed: number, o: { life?: number; size?: number; gravity?: number; drag?: number; up?: number; spread?: number; dir?: number; intensity?: number } = {}): void {
    this.tmp.set(color);
    const k = o.intensity ?? 3;
    for (let c = 0; c < count; c++) {
      let i = this.n;
      if (i >= this.max) i = fx.int(0, this.max - 1);
      else this.n++;
      const a = o.dir !== undefined ? o.dir + fx.range(-1, 1) * (o.spread ?? 0.5) : fx.range(0, Math.PI * 2);
      const sp = speed * fx.range(0.3, 1);
      const up = (o.up ?? 0.3) * fx.range(-0.2, 1);
      this.pos[i * 3] = x;
      this.pos[i * 3 + 1] = y;
      this.pos[i * 3 + 2] = z;
      this.V[i * 3] = Math.cos(a) * sp;
      this.V[i * 3 + 1] = up * sp;
      this.V[i * 3 + 2] = Math.sin(a) * sp;
      const l = (o.life ?? 0.6) * fx.range(0.5, 1.2);
      this.L[i] = this.L0[i] = l;
      this.C[i * 3] = this.tmp.r * k;
      this.C[i * 3 + 1] = this.tmp.g * k;
      this.C[i * 3 + 2] = this.tmp.b * k;
      this.S0[i] = (o.size ?? 0.35) * fx.range(0.6, 1.3);
      this.G[i] = o.gravity ?? 0;
      this.D[i] = o.drag ?? 2.5;
    }
  }

  update(dt: number): void {
    for (let i = this.n - 1; i >= 0; i--) {
      this.L[i] -= dt;
      if (this.L[i] <= 0) {
        this.kill(i);
        continue;
      }
      const o = i * 3;
      const dr = Math.exp(-this.D[i] * dt);
      this.V[o] *= dr;
      this.V[o + 1] = this.V[o + 1] * dr - this.G[i] * dt;
      this.V[o + 2] *= dr;
      this.pos[o] += this.V[o] * dt;
      this.pos[o + 1] = Math.max(0.05, this.pos[o + 1] + this.V[o + 1] * dt);
      this.pos[o + 2] += this.V[o + 2] * dt;
      const t = this.L[i] / this.L0[i];
      this.col[o] = this.C[o] * t;
      this.col[o + 1] = this.C[o + 1] * t;
      this.col[o + 2] = this.C[o + 2] * t;
      this.size[i] = this.S0[i] * (0.4 + 0.6 * t);
    }
    const g = this.points.geometry;
    g.setDrawRange(0, this.n);
    (g.attributes.position as THREE.BufferAttribute).needsUpdate = true;
    (g.attributes.color as THREE.BufferAttribute).needsUpdate = true;
    (g.attributes.size as THREE.BufferAttribute).needsUpdate = true;
  }

  private kill(i: number): void {
    const j = --this.n;
    if (i === j) return;
    for (let k = 0; k < 3; k++) {
      this.pos[i * 3 + k] = this.pos[j * 3 + k];
      this.V[i * 3 + k] = this.V[j * 3 + k];
      this.C[i * 3 + k] = this.C[j * 3 + k];
      this.col[i * 3 + k] = this.col[j * 3 + k];
    }
    this.L[i] = this.L[j];
    this.L0[i] = this.L0[j];
    this.S0[i] = this.S0[j];
    this.size[i] = this.size[j];
    this.G[i] = this.G[j];
    this.D[i] = this.D[j];
  }

  clear(): void {
    this.n = 0;
  }
}
