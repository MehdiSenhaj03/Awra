import * as THREE from 'three';

/**
 * Procedural models. Everything the Silence made is obsidian (dark, mirror-like, reflecting the sky) with a
 * core of light; Volta is a prism of pure light. Glowing parts use colours above 1 so the bloom catches them.
 */

let env: THREE.Texture | null = null;
export function setEnv(t: THREE.Texture | null): void {
  env = t;
}

export function glow(color: number, k = 3, opts: Partial<THREE.MeshBasicMaterialParameters> = {}): THREE.MeshBasicMaterial {
  const c = new THREE.Color(color).multiplyScalar(k);
  return new THREE.MeshBasicMaterial({ color: c, toneMapped: false, ...opts });
}

/** Mirror-black stone with a neon fresnel rim (the silhouette glows). */
export function obsidian(tint = 0x0b0c14, rough = 0.16, rim = 0x000000, rimK = 1.6): THREE.MeshStandardMaterial {
  const m = new THREE.MeshStandardMaterial({ color: tint, metalness: 0.92, roughness: rough, envMap: env, envMapIntensity: 1.6 });
  const u = { value: new THREE.Color(rim).multiplyScalar(rimK) };
  m.userData.rim = u;
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uRim = u;
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nuniform vec3 uRim;')
      .replace('#include <emissivemap_fragment>', '#include <emissivemap_fragment>\n{ float fr = pow(1.0 - abs(dot(normalize(normal), normalize(vViewPosition))), 3.0); totalEmissiveRadiance += uRim * fr; }');
  };
  m.customProgramCacheKey = () => 'obsidianRim';
  return m;
}

export interface Model {
  root: THREE.Group;
  /** Materials that flash white when hit. */
  flash: THREE.MeshBasicMaterial[];
  /** Base colours of the flash materials. */
  base: THREE.Color[];
  /** Parts animated by the renderer. */
  parts: Record<string, THREE.Object3D>;
  /** Materials whose opacity follows visibility (phantoms). */
  fade: THREE.Material[];
  /** Fresnel rims (flash white when hit). */
  rims: { u: { value: THREE.Color }; base: THREE.Color }[];
}

function model(): Model {
  return { root: new THREE.Group(), flash: [], base: [], parts: {}, fade: [], rims: [] };
}

function core(m: Model, color: number, k: number, geo: THREE.BufferGeometry, name = 'core'): THREE.Mesh {
  const mat = glow(color, k);
  const mesh = new THREE.Mesh(geo, mat);
  m.flash.push(mat);
  m.base.push(mat.color.clone());
  m.parts[name] = mesh;
  return mesh;
}

const G = {
  tetra: new THREE.TetrahedronGeometry(1, 0),
  octa: new THREE.OctahedronGeometry(1, 0),
  ico: new THREE.IcosahedronGeometry(1, 0),
  dodeca: new THREE.DodecahedronGeometry(1, 0),
  sphere: new THREE.SphereGeometry(1, 20, 14),
  box: new THREE.BoxGeometry(1, 1, 1),
};

function edges(geo: THREE.BufferGeometry, color: number, k = 3): THREE.LineSegments {
  return new THREE.LineSegments(new THREE.EdgesGeometry(geo), new THREE.LineBasicMaterial({ color: new THREE.Color(color).multiplyScalar(k), toneMapped: false }));
}

/** Volta: a tall prism of light, a halo, orbiting shards. */
export function playerModel(): Model {
  const m = model();
  const body = new THREE.Group();
  const prism = core(m, 0xbff8ff, 2.6, new THREE.OctahedronGeometry(1, 0));
  prism.scale.set(0.34, 0.95, 0.34);
  prism.position.y = 1.15;
  const inner = new THREE.Mesh(new THREE.OctahedronGeometry(1, 0), glow(0xffffff, 6));
  inner.scale.set(0.14, 0.5, 0.14);
  inner.position.y = 1.15;
  const halo = new THREE.Mesh(new THREE.TorusGeometry(0.62, 0.035, 8, 48), glow(0x2ef2ff, 4));
  halo.position.y = 1.3;
  halo.rotation.x = Math.PI / 2;
  m.parts.halo = halo;
  const shards = new THREE.Group();
  for (let i = 0; i < 3; i++) {
    const s = new THREE.Mesh(G.tetra, glow(i === 1 ? 0xff2fb9 : 0x2ef2ff, 3));
    s.scale.setScalar(0.12);
    s.position.set(Math.cos((i / 3) * Math.PI * 2) * 0.75, 1.0, Math.sin((i / 3) * Math.PI * 2) * 0.75);
    shards.add(s);
  }
  m.parts.shards = shards;
  // Aim: a thin blade of light pointing forward.
  const aim = new THREE.Group();
  const blade = new THREE.Mesh(new THREE.ConeGeometry(0.09, 0.9, 4), glow(0xffffff, 3));
  blade.rotation.z = -Math.PI / 2;
  blade.position.set(0.95, 0.9, 0);
  aim.add(blade);
  m.parts.aim = aim;
  body.add(prism, inner, halo, shards, aim);
  m.parts.body = body;
  m.root.add(body);
  return m;
}

/** Obsidian whose rim glows in the foe's colour; registered for hit flashes. */
function rimmed(m: Model, color: number, tint = 0x0b0c14, rough = 0.16): THREE.MeshStandardMaterial {
  const mat = obsidian(tint, rough, color, 1.1);
  const u = mat.userData.rim as { value: THREE.Color };
  m.rims.push({ u, base: u.value.clone() });
  return mat;
}

/** Elite crown: five golden spikes. */
function crown(m: Model, r: number, y: number): void {
  const g = new THREE.Group();
  for (let i = 0; i < 5; i++) {
    const s = new THREE.Mesh(new THREE.ConeGeometry(0.08, 0.35, 4), glow(0xffc34a, 4));
    const a = (i / 5) * Math.PI * 2;
    s.position.set(Math.cos(a) * r * 0.55, y, Math.sin(a) * r * 0.55);
    g.add(s);
  }
  m.parts.crown = g;
  m.root.add(g);
}

const BUILD: Record<string, (r: number, color: number) => Model> = {
  mote(r, color) {
    const m = model();
    const b = new THREE.Mesh(G.tetra, rimmed(m, color));
    b.scale.setScalar(r * 1.15);
    const c = core(m, color, 4, G.octa);
    c.scale.setScalar(r * 0.42);
    const g = new THREE.Group();
    g.add(b, c, edges(G.tetra, color, 2).translateY(0));
    (g.children[2] as THREE.Object3D).scale.setScalar(r * 1.16);
    g.position.y = 0.7;
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
  spitter(r, color) {
    const m = model();
    const g = new THREE.Group();
    const b = new THREE.Mesh(G.octa, rimmed(m, color));
    b.scale.set(r, r * 0.8, r);
    const mouth = core(m, color, 4, new THREE.TorusGeometry(0.42, 0.09, 8, 24));
    mouth.scale.setScalar(r);
    mouth.rotation.y = Math.PI / 2;
    mouth.position.x = r * 0.75;
    for (let i = 0; i < 4; i++) {
      const fin = new THREE.Mesh(G.tetra, rimmed(m, color, 0x10111c));
      fin.scale.set(r * 0.5, r * 0.25, r * 0.12);
      const a = (i / 4) * Math.PI * 2 + Math.PI / 4;
      fin.position.set(-r * 0.3, Math.sin(a) * r * 0.8, Math.cos(a) * r * 0.8);
      g.add(fin);
    }
    g.add(b, mouth, edges(G.octa, color, 1.6));
    (g.children[g.children.length - 1] as THREE.Object3D).scale.set(r * 1.01, r * 0.81, r * 1.01);
    g.position.y = 0.9;
    m.parts.face = g;
    m.root.add(g);
    return m;
  },
  spinner(r, color) {
    const m = model();
    const g = new THREE.Group();
    const ringM = new THREE.Mesh(new THREE.TorusGeometry(r, r * 0.28, 12, 48), rimmed(m, color));
    ringM.rotation.x = Math.PI / 2;
    const inner = core(m, color, 4, new THREE.TorusGeometry(r * 0.62, r * 0.07, 8, 48));
    inner.rotation.x = Math.PI / 2;
    for (let i = 0; i < 6; i++) {
      const sp = new THREE.Mesh(new THREE.ConeGeometry(r * 0.16, r * 0.7, 4), rimmed(m, color, 0x12121e));
      const a = (i / 6) * Math.PI * 2;
      sp.position.set(Math.cos(a) * r * 1.3, 0, Math.sin(a) * r * 1.3);
      sp.rotation.z = -Math.PI / 2;
      sp.rotation.y = -a;
      g.add(sp);
    }
    const eye = core(m, 0xffffff, 3, G.sphere, 'eye');
    eye.scale.setScalar(r * 0.22);
    g.add(ringM, inner, eye);
    g.position.y = 0.8;
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
  lancer(r, color) {
    const m = model();
    const g = new THREE.Group();
    const b = new THREE.Mesh(G.octa, rimmed(m, color));
    b.scale.set(r * 2.1, r * 0.5, r * 0.65);
    const e = edges(G.octa, color, 3);
    e.scale.set(r * 2.12, r * 0.51, r * 0.66);
    const tip = core(m, color, 4, new THREE.ConeGeometry(r * 0.22, r * 1.1, 4));
    tip.rotation.z = -Math.PI / 2;
    tip.position.x = r * 1.7;
    g.add(b, e, tip);
    g.position.y = 0.8;
    m.parts.face = g;
    m.root.add(g);
    return m;
  },
  splitter(r, color) {
    const m = model();
    const g = new THREE.Group();
    const b = new THREE.Mesh(G.ico, rimmed(m, color, 0x0a1218, 0.1));
    b.scale.setScalar(r);
    const e = edges(G.ico, color, 3);
    e.scale.setScalar(r * 1.01);
    const c = core(m, color, 3.5, G.ico);
    c.scale.setScalar(r * 0.45);
    g.add(b, e, c);
    g.position.y = 0.9;
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
  bulwark(r, color) {
    const m = model();
    const g = new THREE.Group();
    const b = new THREE.Mesh(new THREE.CylinderGeometry(r * 0.75, r * 0.9, r * 1.6, 6), rimmed(m, color));
    b.position.y = r * 0.8;
    const c = core(m, color, 3, new THREE.CylinderGeometry(r * 0.3, r * 0.3, r * 1.62, 6));
    c.position.y = r * 0.8;
    // The shield: a curved wall in front, gold-edged.
    const sh = new THREE.Group();
    const wall = new THREE.Mesh(new THREE.CylinderGeometry(r * 1.45, r * 1.45, r * 1.5, 24, 1, true, -1.05, 2.1), rimmed(m, color, 0x161208, 0.1));
    (wall.material as THREE.MeshStandardMaterial).side = THREE.DoubleSide;
    wall.position.y = r * 0.8;
    const lip = new THREE.Mesh(new THREE.TorusGeometry(r * 1.45, 0.05, 6, 24, 2.1), glow(0xffc34a, 4));
    lip.rotation.x = Math.PI / 2;
    lip.rotation.z = -1.05;
    lip.position.y = r * 1.55;
    m.flash.push(lip.material as THREE.MeshBasicMaterial);
    m.base.push((lip.material as THREE.MeshBasicMaterial).color.clone());
    sh.add(wall, lip);
    sh.rotation.y = Math.PI / 2;
    m.parts.shield = sh;
    g.add(b, c, sh);
    m.parts.face = g;
    m.root.add(g);
    return m;
  },
  mortar(r, color) {
    const m = model();
    const pts: THREE.Vector2[] = [];
    for (let i = 0; i <= 12; i++) {
      const t = i / 12;
      pts.push(new THREE.Vector2(r * (0.35 + 0.75 * Math.pow(t, 2.2)), r * 1.6 * (1 - t)));
    }
    const bell = new THREE.Mesh(new THREE.LatheGeometry(pts, 24), rimmed(m, color));
    (bell.material as THREE.MeshStandardMaterial).side = THREE.DoubleSide;
    const mouth = core(m, color, 4, new THREE.CircleGeometry(r * 1.05, 24));
    mouth.rotation.x = -Math.PI / 2;
    mouth.position.y = 0.05;
    const top = core(m, color, 3, G.sphere, 'top');
    top.scale.setScalar(r * 0.3);
    top.position.y = r * 1.75;
    const g = new THREE.Group();
    g.add(bell, mouth, top);
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
  sniper(r, color) {
    const m = model();
    const g = new THREE.Group();
    const shell = new THREE.Mesh(G.sphere, rimmed(m, color));
    shell.scale.setScalar(r);
    const iris = core(m, color, 5, new THREE.TorusGeometry(r * 0.5, r * 0.08, 8, 32));
    iris.rotation.y = Math.PI / 2;
    iris.position.x = r * 0.86;
    const pupil = new THREE.Mesh(G.sphere, glow(0xffffff, 6));
    pupil.scale.setScalar(r * 0.22);
    pupil.position.x = r * 0.9;
    const ringO = new THREE.Mesh(new THREE.TorusGeometry(r * 1.4, 0.03, 6, 48), glow(color, 2));
    ringO.rotation.x = Math.PI / 2;
    m.parts.ring = ringO;
    g.add(shell, iris, pupil, ringO);
    g.position.y = 1.1;
    m.parts.face = g;
    m.root.add(g);
    return m;
  },
  mirror(r, color) {
    const m = model();
    const g = new THREE.Group();
    const mat = new THREE.MeshStandardMaterial({ color: 0xffffff, metalness: 1, roughness: 0.02, envMap: env, envMapIntensity: 2.2 });
    for (let i = 0; i < 2; i++) {
      const plate = new THREE.Mesh(G.octa, mat);
      plate.scale.set(r * 0.15, r * 1.2, r * 0.8);
      plate.position.z = (i - 0.5) * r * 0.5;
      plate.rotation.x = (i - 0.5) * 0.5;
      g.add(plate);
    }
    const c = core(m, color, 3, G.octa);
    c.scale.set(r * 0.12, r * 0.5, r * 0.3);
    c.position.x = r * 0.1;
    g.add(c);
    g.position.y = 1.1;
    m.parts.face = g;
    m.root.add(g);
    return m;
  },
  phantom(r, color) {
    const m = model();
    const g = new THREE.Group();
    for (let i = 0; i < 6; i++) {
      const mat = new THREE.MeshStandardMaterial({ color: 0x140a24, metalness: 0.8, roughness: 0.2, envMap: env, transparent: true, opacity: 0.9 });
      m.fade.push(mat);
      const s = new THREE.Mesh(G.tetra, mat);
      const a = (i / 6) * Math.PI * 2;
      s.position.set(Math.cos(a) * r * 0.6, Math.sin(i * 1.7) * r * 0.4, Math.sin(a) * r * 0.6);
      s.scale.setScalar(r * 0.45);
      s.rotation.set(i, i * 2, i * 3);
      g.add(s);
    }
    const c = core(m, color, 4, G.ico);
    c.scale.setScalar(r * 0.35);
    (c.material as THREE.MeshBasicMaterial).transparent = true;
    m.fade.push(c.material as THREE.Material);
    g.add(c);
    g.position.y = 1.0;
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
  choir(r, color) {
    const m = model();
    const g = new THREE.Group();
    for (let i = 0; i < 3; i++) {
      const t = new THREE.Mesh(new THREE.TorusGeometry(r * (0.9 - i * 0.2), 0.08, 8, 32), rimmed(m, color));
      t.rotation.x = Math.PI / 2;
      t.position.y = i * 0.4;
      g.add(t);
    }
    const c = core(m, color, 4, G.sphere);
    c.scale.setScalar(r * 0.35);
    c.position.y = 0.4;
    g.add(c);
    g.position.y = 0.6;
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
  weaver(r, color) {
    const m = model();
    const g = new THREE.Group();
    const b = new THREE.Mesh(G.dodeca, rimmed(m, color));
    b.scale.setScalar(r * 0.8);
    const c = core(m, color, 4, G.ico);
    c.scale.setScalar(r * 0.35);
    for (let i = 0; i < 4; i++) {
      const arm = new THREE.Mesh(G.box, rimmed(m, color, 0x101420));
      arm.scale.set(r * 1.6, 0.12, 0.22);
      arm.position.x = r * 0.9;
      const holder = new THREE.Group();
      holder.rotation.y = (i / 4) * Math.PI * 2;
      holder.add(arm);
      g.add(holder);
    }
    g.add(b, c);
    g.position.y = 0.9;
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
  echo(r, color) {
    const m = model();
    const g = new THREE.Group();
    const c = core(m, color, 5, G.dodeca);
    c.scale.setScalar(r * 0.7);
    const e = edges(G.dodeca, 0xffffff, 4);
    e.scale.setScalar(r * 0.95);
    g.add(c, e);
    g.position.y = 1.1;
    m.parts.spin = g;
    m.root.add(g);
    return m;
  },
};

export function enemyModel(look: string, r: number, color: number, elite: boolean): Model {
  const f = BUILD[look] ?? BUILD.mote;
  const m = f(r, color);
  if (elite) crown(m, r, r * 2 + 0.6);
  return m;
}

export function registerLook(name: string, f: (r: number, color: number) => Model): void {
  BUILD[name] = f;
}

export { model as newModel, core as addCore, edges as edgeLines, G as GEO };
