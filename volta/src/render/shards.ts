import * as THREE from 'three';
import { fx } from '../engine/core';
import { obsidian } from './models';

/**
 * Debris: obsidian chips and glowing splinters thrown by every kill. They tumble, bounce on the glass floor and
 * cool down. Two instanced meshes, a few thousand pieces, no allocation per frame.
 */
export class Shards {
  dark: THREE.InstancedMesh;
  lit: THREE.InstancedMesh;
  private max: number;
  private n = 0;
  private P: Float32Array; // x y z
  private V: Float32Array; // vx vy vz
  private R: Float32Array; // rot x y z
  private W: Float32Array; // ang vel
  private S: Float32Array; // scale
  private L: Float32Array; // life
  private glowing: Uint8Array;
  private C: Float32Array;
  private m = new THREE.Matrix4();
  private q = new THREE.Quaternion();
  private e = new THREE.Euler();
  private v = new THREE.Vector3();
  private s = new THREE.Vector3();
  private col = new THREE.Color();
  private nd = 0;
  private nl = 0;

  constructor(max = 2600) {
    this.max = max;
    const geo = new THREE.TetrahedronGeometry(1, 0);
    this.dark = new THREE.InstancedMesh(geo, obsidian(0x101018, 0.2), max);
    this.lit = new THREE.InstancedMesh(new THREE.OctahedronGeometry(1, 0), new THREE.MeshBasicMaterial({ toneMapped: false }), max);
    this.lit.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(max * 3), 3);
    for (const im of [this.dark, this.lit]) {
      im.frustumCulled = false;
      im.count = 0;
    }
    this.P = new Float32Array(max * 3);
    this.V = new Float32Array(max * 3);
    this.R = new Float32Array(max * 3);
    this.W = new Float32Array(max * 3);
    this.S = new Float32Array(max);
    this.L = new Float32Array(max);
    this.glowing = new Uint8Array(max);
    this.C = new Float32Array(max * 3);
  }

  /** A burst at (x, z) on the floor plane. */
  burst(x: number, z: number, y: number, count: number, size: number, speed: number, color: number, litRatio = 0.4): void {
    this.col.set(color);
    for (let k = 0; k < count; k++) {
      let i = this.n;
      if (i >= this.max) i = fx.int(0, this.max - 1);
      else this.n++;
      const a = fx.range(0, Math.PI * 2);
      const up = fx.range(0.2, 1);
      const sp = speed * fx.range(0.35, 1.1);
      this.P[i * 3] = x + fx.range(-0.2, 0.2) * size;
      this.P[i * 3 + 1] = y + fx.range(-0.3, 0.3) * size;
      this.P[i * 3 + 2] = z + fx.range(-0.2, 0.2) * size;
      this.V[i * 3] = Math.cos(a) * sp;
      this.V[i * 3 + 1] = up * sp * 0.9 + 2;
      this.V[i * 3 + 2] = Math.sin(a) * sp;
      for (let j = 0; j < 3; j++) {
        this.R[i * 3 + j] = fx.range(0, 6.28);
        this.W[i * 3 + j] = fx.range(-14, 14);
      }
      this.S[i] = size * fx.range(0.08, 0.32);
      this.L[i] = fx.range(1.2, 2.6);
      this.glowing[i] = fx.chance(litRatio) ? 1 : 0;
      const hot = fx.range(2, 5);
      this.C[i * 3] = Math.min(8, this.col.r * hot + 0.3);
      this.C[i * 3 + 1] = Math.min(8, this.col.g * hot + 0.3);
      this.C[i * 3 + 2] = Math.min(8, this.col.b * hot + 0.3);
    }
  }

  update(dt: number): void {
    this.nd = 0;
    this.nl = 0;
    for (let i = this.n - 1; i >= 0; i--) {
      this.L[i] -= dt;
      if (this.L[i] <= 0) {
        this.kill(i);
        continue;
      }
      const o = i * 3;
      this.V[o + 1] -= 26 * dt;
      this.P[o] += this.V[o] * dt;
      this.P[o + 1] += this.V[o + 1] * dt;
      this.P[o + 2] += this.V[o + 2] * dt;
      if (this.P[o + 1] < this.S[i] * 0.5) {
        this.P[o + 1] = this.S[i] * 0.5;
        this.V[o + 1] *= -0.35;
        this.V[o] *= 0.7;
        this.V[o + 2] *= 0.7;
        this.W[o] *= 0.6;
        this.W[o + 2] *= 0.6;
      }
      for (let j = 0; j < 3; j++) this.R[o + j] += this.W[o + j] * dt;
      const life = this.L[i];
      const sc = this.S[i] * Math.min(1, life * 1.5);
      this.e.set(this.R[o], this.R[o + 1], this.R[o + 2]);
      this.q.setFromEuler(this.e);
      this.v.set(this.P[o], this.P[o + 1], this.P[o + 2]);
      this.s.setScalar(sc);
      this.m.compose(this.v, this.q, this.s);
      if (this.glowing[i]) {
        const k = Math.min(1, life * 0.8);
        this.lit.setMatrixAt(this.nl, this.m);
        this.col.setRGB(this.C[o] * k, this.C[o + 1] * k, this.C[o + 2] * k);
        this.lit.setColorAt(this.nl, this.col);
        this.nl++;
      } else {
        this.dark.setMatrixAt(this.nd++, this.m);
      }
    }
    this.dark.count = this.nd;
    this.lit.count = this.nl;
    this.dark.instanceMatrix.needsUpdate = true;
    this.lit.instanceMatrix.needsUpdate = true;
    if (this.lit.instanceColor) this.lit.instanceColor.needsUpdate = true;
  }

  private kill(i: number): void {
    const j = --this.n;
    if (i === j) return;
    for (let k = 0; k < 3; k++) {
      this.P[i * 3 + k] = this.P[j * 3 + k];
      this.V[i * 3 + k] = this.V[j * 3 + k];
      this.R[i * 3 + k] = this.R[j * 3 + k];
      this.W[i * 3 + k] = this.W[j * 3 + k];
      this.C[i * 3 + k] = this.C[j * 3 + k];
    }
    this.S[i] = this.S[j];
    this.L[i] = this.L[j];
    this.glowing[i] = this.glowing[j];
  }

  clear(): void {
    this.n = 0;
  }
}
