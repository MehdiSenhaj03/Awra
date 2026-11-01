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

// ---- FUGUE (twin furnace golems linked by molten gold) ----------------------------------------------------------

registerLook('fugue', (r, color) => {
  const m = newModel();
  const g = new THREE.Group();
  const torso = new THREE.Mesh(new THREE.OctahedronGeometry(1, 0), rim(m, color));
  torso.scale.set(r * 0.75, r * 1.0, r * 0.6);
  torso.position.y = r * 1.6;
  const furnace = addCore(m, color, 5, new THREE.OctahedronGeometry(1, 0), 'heart');
  furnace.scale.set(r * 0.28, r * 0.36, r * 0.2);
  furnace.position.set(0, r * 1.6, r * 0.42);
  const head = new THREE.Mesh(new THREE.ConeGeometry(r * 0.35, r * 0.7, 5), rim(m, color));
  head.position.y = r * 2.75;
  for (let i = 0; i < 4; i++) {
    const c = new THREE.Mesh(new THREE.CylinderGeometry(r * 0.07, r * 0.09, r * 0.6, 6), rim(m, color, 0x100c08));
    c.position.set((i - 1.5) * r * 0.3, r * 2.5 + (i % 2) * r * 0.2, -r * 0.15);
    const f = new THREE.Mesh(GEO.octa, glow(0xff60c0, 5));
    f.scale.set(r * 0.06, r * 0.12, r * 0.06);
    f.position.copy(c.position).add(new THREE.Vector3(0, r * 0.38, 0));
    g.add(c, f);
  }
  const arms: THREE.Object3D[] = [];
  for (const s of [-1, 1]) {
    const arm = new THREE.Group();
    const a = new THREE.Mesh(new THREE.OctahedronGeometry(1, 0), rim(m, color));
    a.scale.set(r * 0.28, r * 0.75, r * 0.28);
    a.position.y = -r * 0.6;
    const fist = addCore(m, color, 3, GEO.sphere, `fist${s}`);
    fist.scale.setScalar(r * 0.22);
    fist.position.y = -r * 1.3;
    arm.add(a, fist);
    arm.position.set(s * r * 0.95, r * 2.0, 0);
    arms.push(arm);
    g.add(arm);
  }
  for (const s of [-1, 1]) {
    const leg = new THREE.Mesh(new THREE.OctahedronGeometry(1, 0), rim(m, color));
    leg.scale.set(r * 0.3, r * 0.65, r * 0.3);
    leg.position.set(s * r * 0.4, r * 0.6, 0);
    g.add(leg);
  }
  g.add(torso, furnace, head);
  (g as THREE.Group).userData.arms = arms;
  m.parts.body = g;
  m.parts.face = g;
  m.root.add(g);
  return m;
});

POSERS.fugue = (e, m, _dt, time) => {
  const arms = m.parts.body.userData.arms as THREE.Object3D[];
  arms.forEach((a, i) => (a.rotation.x = Math.sin(time * 3 + i * Math.PI) * 0.4));
  m.parts.body.position.y = Math.abs(Math.sin(time * 2.2)) * 0.15;
  m.parts.face.rotation.y = -e.rot + Math.PI / 2;
};

// ---- RUBATO (chrome serpent) -------------------------------------------------------------------------------------

function chrome(): THREE.MeshStandardMaterial {
  const mat = obsidian(0xc8ccd8, 0.04, 0x9b5cff, 0.8);
  mat.metalness = 1;
  mat.envMapIntensity = 2.4;
  return mat;
}

registerLook('rubato', (r, color) => {
  const m = newModel();
  const g = new THREE.Group();
  g.position.y = 1.3;
  const head = new THREE.Mesh(new THREE.SphereGeometry(r, 24, 16), chrome());
  head.scale.set(1.3, 0.85, 1);
  m.rims.push({ u: (head.material as THREE.MeshStandardMaterial).userData.rim, base: (head.material as THREE.MeshStandardMaterial).userData.rim.value.clone() });
  // The tuning-fork crest.
  for (const s of [-1, 1]) {
    const prong = new THREE.Mesh(new THREE.BoxGeometry(r * 1.6, r * 0.18, r * 0.18), chrome());
    prong.position.set(-r * 0.6, r * 0.55, s * r * 0.45);
    prong.rotation.z = 0.35;
    g.add(prong);
  }
  for (const s of [-1, 1]) {
    const eye = addCore(m, color, 6, GEO.sphere, `eye${s}`);
    eye.scale.setScalar(r * 0.16);
    eye.position.set(r * 0.85, r * 0.2, s * r * 0.4);
    g.add(eye);
  }
  g.add(head);
  m.parts.face = g;
  m.root.add(g);
  return m;
});

registerLook('segment', (r, color) => {
  const m = newModel();
  const g = new THREE.Group();
  g.position.y = 1.2;
  const ring = new THREE.Mesh(new THREE.SphereGeometry(r, 20, 14), chrome());
  ring.scale.set(0.9, 0.75, 0.9);
  m.rims.push({ u: (ring.material as THREE.MeshStandardMaterial).userData.rim, base: (ring.material as THREE.MeshStandardMaterial).userData.rim.value.clone() });
  const band = addCore(m, color, 3, new THREE.TorusGeometry(r * 0.92, r * 0.06, 6, 32), 'band');
  band.rotation.y = Math.PI / 2;
  g.add(ring, band);
  m.parts.face = g;
  m.root.add(g);
  return m;
});

POSERS.rubato = (e, m) => {
  m.root.traverse((o) => {
    const mat = (o as THREE.Mesh).material as THREE.Material | undefined;
    if (mat) {
      mat.transparent = e.vis < 0.99;
      mat.opacity = e.vis;
    }
  });
  m.root.position.y = (e.vis - 1) * 3;
};
POSERS.segment = (e, m) => {
  m.root.position.y = (e.vis - 1) * 3;
  m.root.visible = e.vis > 0.05;
};

// ---- STACCATO (tower-mech of dead skyscrapers, neon blades) -------------------------------------------------------

registerLook('staccato', (r, color) => {
  const m = newModel();
  const g = new THREE.Group();
  for (let i = 0; i < 4; i++) {
    const h = r * (1.4 - i * 0.22);
    const b = new THREE.Mesh(new THREE.BoxGeometry(r * (1.1 - i * 0.18), h, r * (0.9 - i * 0.15)), rim(m, color));
    b.position.y = r * 0.7 + i * r * 1.05;
    b.rotation.y = i * 0.35;
    g.add(b);
    // Lit windows.
    const w = new THREE.Mesh(new THREE.BoxGeometry(r * (1.12 - i * 0.18), h * 0.06, r * (0.92 - i * 0.15)), glow(color, 2.5));
    w.position.copy(b.position);
    w.rotation.y = b.rotation.y;
    g.add(w);
  }
  const core = addCore(m, color, 5, GEO.octa, 'heart');
  core.scale.set(r * 0.25, r * 0.4, r * 0.25);
  core.position.y = r * 4.6;
  const blades: THREE.Object3D[] = [];
  for (const s of [-1, 1]) {
    const blade = new THREE.Mesh(new THREE.BoxGeometry(r * 0.08, r * 3, r * 0.5), glow(color, 4));
    blade.position.set(s * r * 1.1, r * 1.8, 0);
    blade.rotation.z = s * 0.35;
    blades.push(blade);
    g.add(blade);
  }
  g.add(core);
  g.userData.blades = blades;
  m.parts.body = g;
  m.root.add(g);
  return m;
});

POSERS.staccato = (_e, m, _dt, time) => {
  const blades = m.parts.body.userData.blades as THREE.Object3D[] | undefined;
  blades?.forEach((b, i) => (b.rotation.z = (i ? 1 : -1) * (0.35 + Math.abs(Math.sin(time * 4)) * 0.25)));
  m.parts.body.rotation.y += 0.004;
};

// ---- FERMATA (black sun, broken golden arc) ----------------------------------------------------------------------

registerLook('fermata', (r) => {
  const m = newModel();
  const g = new THREE.Group();
  g.position.y = 3;
  const sun = new THREE.Mesh(new THREE.SphereGeometry(r, 48, 32), new THREE.MeshBasicMaterial({ color: 0x000000 }));
  const halo = new THREE.Mesh(new THREE.TorusGeometry(r * 1.04, r * 0.05, 12, 96), glow(0xffffff, 6));
  halo.rotation.x = Math.PI / 2.4;
  const corona = new THREE.Mesh(new THREE.TorusGeometry(r * 1.12, r * 0.14, 12, 96), glow(0xffd890, 1.4, { transparent: true, opacity: 0.6 }));
  corona.rotation.x = Math.PI / 2.4;
  m.flash.push(halo.material as THREE.MeshBasicMaterial);
  m.base.push((halo.material as THREE.MeshBasicMaterial).color.clone());
  // The fermata arc above, broken.
  const arc = new THREE.Mesh(new THREE.TorusGeometry(r * 1.9, r * 0.09, 8, 64, Math.PI * 0.85), rim(m, 0xffc34a, 0x14100a));
  arc.rotation.z = Math.PI * 0.075;
  arc.position.y = r * 0.2;
  const arcGlow = new THREE.Mesh(new THREE.TorusGeometry(r * 1.9, r * 0.025, 6, 64, Math.PI * 0.85), glow(0xffc34a, 5));
  arcGlow.rotation.copy(arc.rotation);
  arcGlow.position.copy(arc.position);
  const orbit = new THREE.Group();
  for (let i = 0; i < 40; i++) {
    const s = new THREE.Mesh(GEO.tetra, i % 3 ? rim(m, 0xffffff, 0x0a0a10) : glow(0xffe0b0, 3));
    const a = (i / 40) * Math.PI * 2;
    const d = r * (2.2 + (i % 5) * 0.25);
    s.position.set(Math.cos(a) * d, Math.sin(i * 1.7) * r * 0.4, Math.sin(a) * d);
    s.scale.setScalar(r * (0.05 + (i % 4) * 0.03));
    orbit.add(s);
  }
  g.add(sun, halo, corona, arc, arcGlow, orbit);
  m.parts.spin = orbit;
  m.parts.body = g;
  m.parts.arc = arc;
  m.root.add(g);
  return m;
});

POSERS.fermata = (_e, m, dt, time) => {
  m.parts.spin.rotation.y += dt * 0.25;
  m.parts.body.position.y = 3 + Math.sin(time * 0.8) * 0.2;
};
