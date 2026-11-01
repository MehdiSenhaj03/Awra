import * as THREE from 'three';

export const enum GS {
  Orb = 0,
  Streak = 1,
  Ring = 2,
  Beam = 3,
  Warn = 4,
  Zone = 5,
  Disc = 6,
}

/**
 * Additive glowing quads lying on the floor plane, drawn in one call: bullets, shots, shock rings, lasers,
 * telegraphs. Each frame the owner calls begin(), add() for every glow, then end().
 */
export class Glows {
  mesh: THREE.InstancedMesh;
  private max: number;
  private n = 0;
  private aPos: THREE.InstancedBufferAttribute;
  private aDir: THREE.InstancedBufferAttribute;
  private aSize: THREE.InstancedBufferAttribute;
  private aColor: THREE.InstancedBufferAttribute;
  private aStyle: THREE.InstancedBufferAttribute;
  mat: THREE.ShaderMaterial;

  constructor(max = 6000) {
    this.max = max;
    const geo = new THREE.InstancedBufferGeometry();
    const base = new THREE.PlaneGeometry(2, 2);
    geo.index = base.index;
    geo.setAttribute('position', base.getAttribute('position'));
    geo.setAttribute('uv', base.getAttribute('uv'));
    this.aPos = new THREE.InstancedBufferAttribute(new Float32Array(max * 3), 3).setUsage(THREE.DynamicDrawUsage);
    this.aDir = new THREE.InstancedBufferAttribute(new Float32Array(max * 2), 2).setUsage(THREE.DynamicDrawUsage);
    this.aSize = new THREE.InstancedBufferAttribute(new Float32Array(max * 2), 2).setUsage(THREE.DynamicDrawUsage);
    this.aColor = new THREE.InstancedBufferAttribute(new Float32Array(max * 3), 3).setUsage(THREE.DynamicDrawUsage);
    this.aStyle = new THREE.InstancedBufferAttribute(new Float32Array(max * 2), 2).setUsage(THREE.DynamicDrawUsage);
    geo.setAttribute('iPos', this.aPos);
    geo.setAttribute('iDir', this.aDir);
    geo.setAttribute('iSize', this.aSize);
    geo.setAttribute('iColor', this.aColor);
    geo.setAttribute('iStyle', this.aStyle);
    this.mat = new THREE.ShaderMaterial({
      transparent: true,
      depthWrite: false,
      depthTest: true,
      side: THREE.DoubleSide,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
      uniforms: { uTime: { value: 0 } },
      vertexShader: /* glsl */ `
        attribute vec3 iPos;
        attribute vec2 iDir;
        attribute vec2 iSize;
        attribute vec3 iColor;
        attribute vec2 iStyle;
        varying vec2 vUv;
        varying vec3 vC;
        varying vec2 vS;
        varying vec2 vSize;
        void main() {
          vUv = position.xy;
          vC = iColor;
          vS = iStyle;
          vSize = iSize;
          vec2 p = position.xy * iSize;
          vec2 d = iDir;
          vec2 q = vec2(p.x * d.x - p.y * d.y, p.x * d.y + p.y * d.x);
          vec3 w = vec3(iPos.x + q.x, iPos.y, iPos.z + q.y);
          gl_Position = projectionMatrix * viewMatrix * vec4(w, 1.0);
        }`,
      fragmentShader: /* glsl */ `
        uniform float uTime;
        varying vec2 vUv;
        varying vec3 vC;
        varying vec2 vS;
        varying vec2 vSize;
        void main() {
          float st = vS.x;
          float prm = vS.y;
          vec3 c = vec3(0.0);
          if (st < 0.5) {
            // Orb: coloured halo, dark-edged ring, white-hot core.
            float r = length(vUv);
            float halo = exp(-r * r * 5.0) * 0.9;
            float rim = smoothstep(0.62, 0.5, r) * smoothstep(0.3, 0.45, r);
            float core = smoothstep(0.42, 0.2, r);
            c = vC * (halo + rim * 1.6) + vec3(1.0) * core * 2.2;
          } else if (st < 1.5) {
            // Streak along x.
            float ax = abs(vUv.x);
            float ay = abs(vUv.y);
            float body = exp(-ay * ay * 9.0) * smoothstep(1.0, 0.55, ax);
            float core = exp(-ay * ay * 60.0) * smoothstep(1.0, 0.3, ax);
            c = vC * body * 1.3 + vec3(1.0) * core * 2.0 * length(vC) * 0.5;
          } else if (st < 2.5) {
            // Ring: prm = thickness (0..1 of radius).
            float r = length(vUv);
            float t = max(prm, 0.02);
            float ring = exp(-pow((r - (1.0 - t)) / t, 2.0) * 2.5) * step(r, 1.0);
            c = vC * ring;
          } else if (st < 3.5) {
            // Burning beam along x.
            float ay = abs(vUv.y);
            float fl = 0.85 + 0.15 * sin(uTime * 60.0 + vUv.x * 20.0);
            float body = exp(-ay * ay * 4.0);
            float core = exp(-ay * ay * 40.0);
            c = (vC * body * 1.4 + vec3(1.0) * core * 2.5) * fl * smoothstep(1.0, 0.97, abs(vUv.x));
          } else if (st < 4.5) {
            // Warning line: thin dashed light, brightening as prm -> 1.
            float ay = abs(vUv.y);
            float dash = step(0.5, fract(vUv.x * vSize.x * 0.6 - uTime * 3.0));
            float line = exp(-ay * ay * 30.0);
            c = vC * line * (0.25 + 0.75 * prm) * (0.45 + 0.55 * dash) * 1.5;
          } else if (st < 5.5) {
            // Landing zone: ring at the edge and a disc filling from the centre.
            float r = length(vUv);
            float edge = exp(-pow((r - 0.96) * 20.0, 2.0)) * 1.4;
            float fill = step(r, prm) * 0.22 + exp(-pow((r - prm) * 18.0, 2.0)) * 0.9 * step(r, 1.0);
            float stripes = 0.6 + 0.4 * step(0.5, fract((vUv.x + vUv.y) * 4.0 - uTime * 1.5));
            c = vC * (edge + fill * stripes) * step(r, 1.02);
          } else {
            // Soft disc.
            float r = length(vUv);
            c = vC * exp(-r * r * 3.5) * step(r, 1.0);
          }
          gl_FragColor = vec4(c, 1.0);
        }`,
    });
    this.mesh = new THREE.InstancedMesh(geo as unknown as THREE.BufferGeometry, this.mat, max);
    this.mesh.frustumCulled = false;
    this.mesh.count = 0;
    this.mesh.renderOrder = 5;
  }

  begin(): void {
    this.n = 0;
  }

  add(style: GS, x: number, z: number, y: number, angle: number, length: number, width: number, r: number, g: number, b: number, param = 0): void {
    if (this.n >= this.max) return;
    const i = this.n++;
    this.aPos.setXYZ(i, x, y, z);
    this.aDir.setXY(i, Math.cos(angle), Math.sin(angle));
    this.aSize.setXY(i, length, width);
    this.aColor.setXYZ(i, r, g, b);
    this.aStyle.setXY(i, style, param);
  }

  end(time: number): void {
    this.mesh.count = this.n;
    this.aPos.needsUpdate = true;
    this.aDir.needsUpdate = true;
    this.aSize.needsUpdate = true;
    this.aColor.needsUpdate = true;
    this.aStyle.needsUpdate = true;
    this.mat.uniforms.uTime.value = time;
  }
}
