import * as THREE from 'three';
import { fx } from '../engine/core';

/**
 * The abyss under the arena: a painted panorama far below (when the sector has one), a procedural nebula mixed in,
 * drifting stars and slow giant shards between the floor and the abyss for depth.
 */
export class Backdrop {
  group = new THREE.Group();
  private mat: THREE.ShaderMaterial;
  private debris: THREE.InstancedMesh;
  private dP: Float32Array;
  private dR: Float32Array;
  private m4 = new THREE.Matrix4();
  private q = new THREE.Quaternion();
  private e = new THREE.Euler();
  private v = new THREE.Vector3();
  private s = new THREE.Vector3();
  private count: number;

  constructor(painting: THREE.Texture | null, accent: THREE.Color, accent2: THREE.Color, env: THREE.Texture | null) {
    const geo = new THREE.PlaneGeometry(900, 600);
    geo.rotateX(-Math.PI / 2);
    this.mat = new THREE.ShaderMaterial({
      toneMapped: false,
      depthWrite: false,
      uniforms: {
        uTex: { value: painting },
        uHas: { value: painting ? 1 : 0 },
        uTime: { value: 0 },
        uA: { value: accent },
        uB: { value: accent2 },
        uLevel: { value: 0 },
      },
      vertexShader: /* glsl */ `
        varying vec2 vUv;
        void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
      fragmentShader: /* glsl */ `
        uniform sampler2D uTex;
        uniform float uHas, uTime, uLevel;
        uniform vec3 uA, uB;
        varying vec2 vUv;
        float h(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5); }
        float n(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
          return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y); }
        float fbm(vec2 p) { float s = 0.0, a = 0.5; for (int i = 0; i < 5; i++) { s += a * n(p); p *= 2.03; a *= 0.5; } return s; }
        void main() {
          vec2 p = vUv * vec2(3.0, 2.0);
          float t = uTime * 0.01;
          float neb = fbm(p * 1.5 + vec2(t, -t)) * fbm(p * 3.0 - vec2(t * 2.0, 0.0));
          vec3 col = mix(uA, uB, fbm(p * 0.8 + 3.0)) * pow(neb, 2.2) * 0.9;
          // Stars.
          vec2 sp = vUv * vec2(900.0, 600.0) * 0.5;
          float st = step(0.997, h(floor(sp))) * (0.5 + 0.5 * sin(uTime * 2.0 + h(floor(sp)) * 40.0));
          col += vec3(st) * 1.5;
          if (uHas > 0.5) {
            vec3 img = texture2D(uTex, vUv).rgb;
            col = img * (0.85 + uLevel * 0.25) + col * 0.35;
          }
          // Vignette towards the far edges.
          float v = smoothstep(0.75, 0.2, length(vUv - 0.5));
          gl_FragColor = vec4(col * v, 1.0);
        }`,
    });
    const plane = new THREE.Mesh(geo, this.mat);
    plane.position.y = -120;
    plane.renderOrder = -10;
    this.group.add(plane);
    // Giant obsidian shards drifting in the void.
    this.count = 70;
    this.debris = new THREE.InstancedMesh(new THREE.TetrahedronGeometry(1, 0), new THREE.MeshStandardMaterial({ color: 0x07080e, metalness: 0.95, roughness: 0.15, envMap: env, envMapIntensity: 1.2 }), this.count);
    this.dP = new Float32Array(this.count * 4);
    this.dR = new Float32Array(this.count * 6);
    for (let i = 0; i < this.count; i++) {
      const a = fx.range(0, Math.PI * 2);
      const d = fx.range(30, 140);
      this.dP[i * 4] = Math.cos(a) * d;
      this.dP[i * 4 + 1] = fx.range(-90, -12);
      this.dP[i * 4 + 2] = Math.sin(a) * d * 0.8;
      this.dP[i * 4 + 3] = fx.range(1, 7) * (1 + -this.dP[i * 4 + 1] / 60);
      for (let j = 0; j < 3; j++) {
        this.dR[i * 6 + j] = fx.range(0, 6.28);
        this.dR[i * 6 + 3 + j] = fx.range(-0.15, 0.15);
      }
    }
    this.debris.frustumCulled = false;
    this.group.add(this.debris);
  }

  update(time: number, dt: number, level: number): void {
    this.mat.uniforms.uTime.value = time;
    this.mat.uniforms.uLevel.value = level;
    for (let i = 0; i < this.count; i++) {
      for (let j = 0; j < 3; j++) this.dR[i * 6 + j] += this.dR[i * 6 + 3 + j] * dt;
      this.e.set(this.dR[i * 6], this.dR[i * 6 + 1], this.dR[i * 6 + 2]);
      this.q.setFromEuler(this.e);
      this.v.set(this.dP[i * 4], this.dP[i * 4 + 1] + Math.sin(time * 0.2 + i) * 0.8, this.dP[i * 4 + 2]);
      this.s.set(this.dP[i * 4 + 3], this.dP[i * 4 + 3] * 1.8, this.dP[i * 4 + 3]);
      this.m4.compose(this.v, this.q, this.s);
      this.debris.setMatrixAt(i, this.m4);
    }
    this.debris.instanceMatrix.needsUpdate = true;
  }
}
