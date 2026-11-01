import * as THREE from 'three';
import type { Enemy } from '../game/enemy';
import { GEO, type Model, addCore, glow, newModel, obsidian, registerLook } from './models';

/** Per-look animation of boss models (called every frame). */
export const POSERS: Record<string, (e: Enemy, m: Model, dt: number, time: number) => void> = {};

function rim(m: Model, color: number, tint = 0x0a0b12, rough = 0.14): THREE.MeshStandardMaterial {
  const mat = obsidian(tint, rough, color, 1.1);
  const u = mat.userData.rim as { value: THREE.Color };
  m.rims.push({ u, base: u.value.clone() });
  return mat;
}

// ---- OSTINATO --------------------------------------------------------------------------------------------------

registerLook('ostinato', (r, color) => {
  const m = newModel();
  const g = new THREE.Group();
  g.position.y = 2.2;
  const heart = addCore(m, color, 4, new THREE.OctahedronGeometry(1, 0), 'heart');
  heart.scale.set(r * 0.32, r * 0.55, r * 0.32);
  const inner = new THREE.Mesh(GEO.octa, glow(0xffffff, 6));
  inner.scale.set(r * 0.12, r * 0.25, r * 0.12);
  g.add(heart, inner);
  const rings: THREE.Object3D[] = [];
  for (let i = 0; i < 3; i++) {
    const ringG = new THREE.Group();
    const R = r * (0.8 + i * 0.32);
    const band = new THREE.Mesh(new THREE.TorusGeometry(R, r * 0.07, 10, 96), rim(m, color));
    const lip = new THREE.Mesh(new THREE.TorusGeometry(R, r * 0.012, 6, 96), glow(i === 1 ? 0xffc34a : color, 3));
    lip.position.z = r * 0.07;
    ringG.add(band, lip);
    // Organ pipes and candles on the outer ring.
    if (i === 2) {
      for (let k = 0; k < 12; k++) {
        const a = (k / 12) * Math.PI * 2;
        const h = r * (0.4 + (k % 3) * 0.2);
        const pipe = new THREE.Mesh(new THREE.CylinderGeometry(r * 0.05, r * 0.06, h, 6), rim(m, color, 0x0c0c14));
        pipe.position.set(Math.cos(a) * R, Math.sin(a) * R, 0);
        pipe.rotation.z = a - Math.PI / 2;
        pipe.translateY(h / 2);
        const flame = new THREE.Mesh(GEO.octa, glow(color, 5));
        flame.scale.set(r * 0.04, r * 0.09, r * 0.04);
        flame.position.copy(pipe.position).add(new THREE.Vector3(Math.cos(a), Math.sin(a), 0).multiplyScalar(h / 2 + r * 0.06));
        ringG.add(pipe, flame);
      }
    }
    rings.push(ringG);
    g.add(ringG);
  }
  m.parts.rings = new THREE.Group();
  (m.parts.rings as THREE.Group).userData.list = rings;
  m.parts.body = g;
  m.root.add(g);
  return m;
});

POSERS.ostinato = (e, m, dt, time) => {
  const al = (e as unknown as { align: number }).align ?? 0;
  const list = (m.parts.rings.userData.list as THREE.Object3D[]) ?? [];
  list.forEach((rg, i) => {
    const sp = (1 - al) * (0.6 + i * 0.35);
    rg.rotation.x = Math.PI / 2 + (1 - al) * Math.sin(time * sp + i * 2) * 1.1;
    rg.rotation.y = (1 - al) * Math.cos(time * sp * 0.8 + i) * 0.9;
    rg.rotation.z += dt * (0.2 + i * 0.15) * (i % 2 ? -1 : 1);
  });
  m.parts.heart.rotation.y += dt * 1.5;
  m.parts.heart.scale.setScalar(1);
  m.parts.heart.scale.set(0.8 + al * 0.5, 1.4 + al * 0.6, 0.8 + al * 0.5).multiplyScalar(e.r * 0.4);
  m.parts.body.position.y = 2.2 + Math.sin(time * 1.3) * 0.25;
};

// ---- CRESCENDO -------------------------------------------------------------------------------------------------

registerLook('crescendo', (r, color) => {
  const m = newModel();
  const g = new THREE.Group();
  const trunk = new THREE.Mesh(new THREE.CylinderGeometry(r * 0.35, r * 0.75, r * 2.2, 7), rim(m, color));
  trunk.position.y = r * 1.1;
  g.add(trunk);
  const crystalMat = (k: number) => glow(color, k, { transparent: true, opacity: 0.85 });
  const branches = new THREE.Group();
  for (let i = 0; i < 9; i++) {
    const a = (i / 9) * Math.PI * 2;
    const len = r * (1.1 + (i % 3) * 0.45);
    const b = new THREE.Mesh(new THREE.ConeGeometry(r * 0.13, len, 5), i % 2 ? rim(m, color, 0x0a140f) : crystalMat(2.2));
    b.position.set(Math.cos(a) * r * 0.5, r * 2.2 + len * 0.3, Math.sin(a) * r * 0.5);
    b.rotation.z = Math.cos(a) * -0.8;
    b.rotation.x = Math.sin(a) * 0.8;
    branches.add(b);
  }
  m.parts.spin = branches;
  const heart = addCore(m, color, 4, GEO.ico, 'heart');
  heart.scale.setScalar(r * 0.4);
  heart.position.y = r * 2.2;
  // Roots of glass on the floor.
  for (let i = 0; i < 7; i++) {
    const a = (i / 7) * Math.PI * 2 + 0.3;
    const root = new THREE.Mesh(new THREE.ConeGeometry(r * 0.12, r * 1.6, 4), crystalMat(1.4));
    root.position.set(Math.cos(a) * r * 0.9, r * 0.15, Math.sin(a) * r * 0.9);
    root.rotation.z = Math.PI / 2;
    root.rotation.y = -a;
    g.add(root);
  }
  g.add(branches, heart);
  m.parts.body = g;
  m.root.add(g);
  return m;
});

POSERS.crescendo = (_e, m, dt, time) => {
  m.parts.spin.rotation.y += dt * 0.2;
  m.parts.heart.rotation.y += dt;
  const s = 1 + Math.sin(time * 4) * 0.05;
  m.parts.heart.scale.setScalar(s * 1.0);
};

registerLook('crystal', (r, color) => {
  const m = newModel();
  const g = new THREE.Group();
  for (let i = 0; i < 4; i++) {
    const c = addCore(m, color, 2.4, GEO.octa, `c${i}`);
    c.scale.set(r * 0.28, r * (1.1 - i * 0.18), r * 0.28);
    c.position.set(Math.cos(i * 2.1) * r * 0.35, r * 0.5, Math.sin(i * 2.1) * r * 0.35);
    c.rotation.set(Math.cos(i) * 0.4, i, Math.sin(i) * 0.4);
    g.add(c);
  }
  m.parts.body = g;
  m.root.add(g);
  return m;
});

POSERS.crystal = (e, m) => {
  const grow = (e as unknown as { grow: number }).grow ?? 0;
  m.parts.body.scale.setScalar(0.5 + grow * 0.7);
};
