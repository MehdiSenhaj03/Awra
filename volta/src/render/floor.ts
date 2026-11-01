import * as THREE from 'three';
import { Reflector } from 'three/examples/jsm/objects/Reflector.js';
import type { Arena } from '../game/arena';

const MAX_RIPPLES = 16;

/**
 * The arena floor: black glass with a fine hexagonal lattice that breathes with the beat, light pools under the hero,
 * shock ripples from every explosion, and a burning rim. Outside the arena the floor is cut away (the abyss shows).
 */
export class Floor {
  mesh: THREE.Mesh;
  mat: THREE.ShaderMaterial;
  private ripples: THREE.Vector4[] = [];
  private next = 0;

  constructor(arena: Arena, accent: THREE.Color, accent2: THREE.Color, reflScale = 0.6) {
    const size = arena.extent * 2 + 8;
    const geo = new THREE.PlaneGeometry(size, size, 1, 1);
    for (let i = 0; i < MAX_RIPPLES; i++) this.ripples.push(new THREE.Vector4(0, 0, -100, 0));
    const shader = {
      name: 'VoltaFloor',
      uniforms: {
        color: { value: null },
        tDiffuse: { value: null },
        textureMatrix: { value: null },
        uTime: { value: 0 },
        uBeat: { value: 0 },
        uPulse: { value: 0 },
        uLevel: { value: 0 },
        uAccent: { value: accent },
        uAccent2: { value: accent2 },
        uPlayer: { value: new THREE.Vector2() },
        uShape: { value: arena.shape === 'circle' ? 0 : 1 },
        uSize: { value: new THREE.Vector3(arena.r, arena.w, arena.h) },
        uRipples: { value: this.ripples },
        uOverdrive: { value: 0 },
        uPillars: { value: Array.from({ length: 8 }, (_, i) => { const p = arena.pillars[i]; return p ? new THREE.Vector3(p.x, p.y, p.r) : new THREE.Vector3(0, 0, 0); }) },
      },
      vertexShader: /* glsl */ `
        uniform mat4 textureMatrix;
        varying vec2 vP;
        varying vec3 vW;
        varying vec4 vRefl;
        void main() {
          vec4 w = modelMatrix * vec4(position, 1.0);
          vW = w.xyz;
          vP = w.xz;
          vRefl = textureMatrix * vec4(position, 1.0);
          gl_Position = projectionMatrix * viewMatrix * w;
        }`,
      fragmentShader: /* glsl */ `
        uniform float uTime, uBeat, uPulse, uLevel, uShape, uOverdrive;
        uniform vec3 uAccent, uAccent2, uSize;
        uniform vec2 uPlayer;
        uniform vec4 uRipples[${MAX_RIPPLES}];
        uniform vec3 uPillars[8];
        uniform sampler2D tDiffuse;
        varying vec2 vP;
        varying vec3 vW;
        varying vec4 vRefl;

        // Signed distance to the arena edge (negative inside).
        float edgeDist(vec2 p) {
          if (uShape < 0.5) return length(p) - uSize.x;
          vec2 d = abs(p) - uSize.yz;
          return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
        }

        // Hexagonal lattice: distance to the nearest cell edge, and the cell id.
        vec4 hex(vec2 p) {
          const vec2 s = vec2(1.0, 1.7320508);
          vec4 hc = floor(vec4(p, p - vec2(0.5, 1.0)) / s.xyxy) + 0.5;
          vec4 h = vec4(p - hc.xy * s, p - (hc.zw + 0.5) * s);
          vec4 r = dot(h.xy, h.xy) < dot(h.zw, h.zw) ? vec4(h.xy, hc.xy) : vec4(h.zw, hc.zw + 9.73);
          vec2 a = abs(r.xy);
          float e = 0.5 - max(dot(a, s * 0.5), a.x);
          return vec4(e, r.zw, 0.0);
        }

        float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

        void main() {
          float ed = edgeDist(vP);
          if (ed > 0.35) discard;
          vec2 hp = vP / 1.6;
          vec4 h = hex(hp);
          float cell = hash(h.yz);
          // Base: black glass, slightly lifted near the hero.
          float dp = length(vP - uPlayer);
          vec3 col = vec3(0.002, 0.003, 0.006) + vec3(0.004, 0.005, 0.01) * cell;
          // Mirror: each glass tile is tilted a hair, so the reflection breaks along the lattice.
          vec2 tilt = (vec2(hash(h.yz + 3.1), hash(h.yz + 7.7)) - 0.5) * 0.012;
          vec4 rc = vRefl;
          rc.xy += tilt * rc.w;
          vec3 refl = texture2DProj(tDiffuse, rc).rgb;
          vec3 V = normalize(cameraPosition - vW);
          float fres = 0.07 + 0.45 * pow(1.0 - max(V.y, 0.0), 3.0);
          col += refl * fres * smoothstep(0.0, 0.08, h.x);
          col += uAccent * 0.025 * exp(-dp * 0.25);
          // Lattice lines: fine, mostly dark, lit by the hero and by waves on the beat.
          float line = smoothstep(0.016, 0.0, h.x);
          float wave = pow(0.5 + 0.5 * sin(length(vP) * 0.45 - uBeat * 6.2831), 6.0);
          float lit = 0.012 + 0.45 * uPulse * wave + 0.6 * exp(-dp * 0.5);
          col += mix(uAccent, uAccent2, cell) * line * lit;
          // A few cells light up on the beat, like a floor equaliser.
          float on = step(0.965 - uLevel * 0.05, fract(cell * 7.13 + floor(uBeat) * 0.618));
          col += mix(uAccent2, uAccent, cell) * on * 0.08 * pow(1.0 - fract(uBeat), 2.0) * smoothstep(0.0, 0.2, h.x);
          // Shock ripples.
          for (int i = 0; i < ${MAX_RIPPLES}; i++) {
            vec4 r = uRipples[i];
            float age = uTime - r.z;
            if (age < 0.0 || age > 1.0) continue;
            float d = length(vP - r.xy);
            float front = age * 18.0 * (0.6 + r.w * 0.5);
            float k = 1.0 - age;
            float ring = exp(-pow((d - front) * 2.4, 2.0)) * k * k;
            col += mix(uAccent, vec3(1.0), 0.3) * ring * r.w * (0.06 + line * 1.8);
          }
          // Pillar shadows and rims.
          for (int i = 0; i < 8; i++) {
            vec3 q = uPillars[i];
            if (q.z <= 0.0) continue;
            float d = length(vP - q.xy) - q.z;
            col *= mix(0.25, 1.0, smoothstep(0.0, 1.2, d));
            col += uAccent * 1.6 * exp(-abs(d) * 12.0);
          }
          // Overdrive: gold floods the lattice.
          col += vec3(1.0, 0.75, 0.3) * line * uOverdrive * 0.5 * wave;
          // The rim burns.
          float rim = exp(-abs(ed + 0.1) * 7.0);
          col += uAccent * rim * (2.2 + 2.5 * uPulse);
          col += vec3(1.0) * exp(-abs(ed + 0.05) * 30.0) * 1.2;
          float alpha = 1.0 - smoothstep(0.0, 0.35, ed);
          // Inner edge darkening (depth).
          col *= mix(1.0, 0.55, smoothstep(-3.0, 0.0, ed) * (1.0 - rim));
          gl_FragColor = vec4(col, alpha);
        }`,
    };
    const scale = Math.min(window.devicePixelRatio, 1.5) * reflScale;
    const refl = new Reflector(geo, { shader, textureWidth: Math.round(window.innerWidth * scale), textureHeight: Math.round(window.innerHeight * scale), clipBias: 0.003, multisample: 0 });
    refl.rotation.x = -Math.PI / 2;
    this.mat = refl.material as THREE.ShaderMaterial;
    this.mat.transparent = true;
    this.mat.toneMapped = false;
    this.mat.uniforms.uRipples.value = this.ripples;
    this.mat.uniforms.uAccent.value = accent;
    this.mat.uniforms.uAccent2.value = accent2;
    this.mesh = refl;
    this.mesh.renderOrder = -1;
  }

  ripple(x: number, y: number, strength: number, time: number): void {
    const r = this.ripples[this.next];
    r.set(x, y, time, strength);
    this.next = (this.next + 1) % MAX_RIPPLES;
  }

  update(time: number, beat: number, pulse: number, level: number, px: number, py: number, od: number): void {
    const u = this.mat.uniforms;
    u.uTime.value = time;
    u.uBeat.value = beat;
    u.uPulse.value = pulse;
    u.uLevel.value = level;
    u.uPlayer.value.set(px, py);
    u.uOverdrive.value = od;
  }
}
