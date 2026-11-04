import * as THREE from 'three';

/**
 * Procedural models. Everything the Silence made is obsidian (dark, mirror-like, reflecting the sky) with a
 * core of light; Volta is a prism of pure light. Glowing parts use colours above 1 so the bloom catches them.
 */

let env: THREE.Texture | null = null;

/** Geometries are shared between every model that asks for the same shape (no per-spawn GPU upload). */
const geoCache = new Map<string, THREE.BufferGeometry>();
export function cg<T extends THREE.BufferGeometry>(type: string, args: unknown[], make: () => T): T {
  const key = `${type}|${JSON.stringify(args)}`;
  let g = geoCache.get(key) as T | undefined;
  if (!g) {
    g = make();
    geoCache.set(key, g);
  }
  return g;
}

const edgeCache = new Map<string, THREE.EdgesGeometry>();
function edgeGeo(src: THREE.BufferGeometry): THREE.EdgesGeometry {
  let e = edgeCache.get(src.uuid);
  if (!e) {
    e = new THREE.EdgesGeometry(src);
    edgeCache.set(src.uuid, e);
  }
  return e;
}

/** Frees a model's materials (geometries are shared and kept). */
export function disposeModel(m: Model): void {
  m.root.traverse((o) => {
    const mat = (o as THREE.Mesh).material as THREE.Material | THREE.Material[] | undefined;
    if (Array.isArray(mat)) mat.forEach((x) => x.dispose());
    else mat?.dispose();
  });
}
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
      .replace('#include <emissivemap_fragment>', '#include <emissivemap_fragment>\n{ float fr = pow(clamp(1.0 - abs(dot(normalize(normal), normalize(vViewPosition))), 0.0, 1.0), 3.0); totalEmissiveRadiance += uRim * fr; }');
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
  tetra: cg('Tetrahedron', [1, 0], () => new THREE.TetrahedronGeometry(1, 0)),
  octa: cg('Octahedron', [1, 0], () => new THREE.OctahedronGeometry(1, 0)),
  ico: cg('Icosahedron', [1, 0], () => new THREE.IcosahedronGeometry(1, 0)),
  dodeca: cg('Dodecahedron', [1, 0], () => new THREE.DodecahedronGeometry(1, 0)),
  sphere: cg('Sphere', [1, 20, 14], () => new THREE.SphereGeometry(1, 20, 14)),
  box: cg('Box', [1, 1, 1], () => new THREE.BoxGeometry(1, 1, 1)),
};

function edges(geo: THREE.BufferGeometry, color: number, k = 3): THREE.LineSegments {
  return new THREE.LineSegments(edgeGeo(geo), new THREE.LineBasicMaterial({ color: new THREE.Color(color).multiplyScalar(k), toneMapped: false }));
}

/** Volta: a slender figure in black with neon seams, silver light for hair, a tuning-fork halo and a prism gauntlet. */
export function playerModel(): Model {
  const m = model();
  const body = new THREE.Group();
  const suit = glow(0x7fe9ff, 1.25);
  const dark = obsidian(0x05060c, 0.2, 0x2ef2ff, 2.2);
  const torso = new THREE.Mesh(cg('Octahedron', [1, 0], () => new THREE.OctahedronGeometry(1, 0)), suit);
  torso.scale.set(0.26, 0.62, 0.2);
  torso.position.y = 1.35;
  const hips = new THREE.Mesh(cg('Octahedron', [1, 0], () => new THREE.OctahedronGeometry(1, 0)), dark);
  hips.scale.set(0.2, 0.3, 0.16);
  hips.position.y = 0.82;
  for (const sx of [-1, 1]) {
    const leg = new THREE.Mesh(cg('Cone', [0.075, 0.75, 5], () => new THREE.ConeGeometry(0.075, 0.75, 5)), suit);
    leg.rotation.z = Math.PI;
    leg.position.set(sx * 0.09, 0.42, 0);
    body.add(leg);
  }
  const seam = new THREE.Mesh(cg('Box', [0.03, 1.0, 0.21], () => new THREE.BoxGeometry(0.03, 1.0, 0.21)), glow(0xffffff, 5));
  seam.position.y = 1.25;
  const head = new THREE.Mesh(cg('Sphere', [0.15, 16, 12], () => new THREE.SphereGeometry(0.15, 16, 12)), glow(0xe8f4ff, 3.2));
  head.position.y = 2.08;
  const hair = new THREE.Mesh(cg('Cone', [0.13, 0.42, 6], () => new THREE.ConeGeometry(0.13, 0.42, 6)), glow(0xffd6f4, 2.6));
  hair.position.set(-0.04, 2.05, -0.14);
  hair.rotation.x = -1.1;
  // Halo: a thin ring standing behind the shoulders.
  const halo = new THREE.Mesh(cg('Torus', [0.36, 0.014, 8, 48], () => new THREE.TorusGeometry(0.36, 0.014, 8, 48)), glow(0xffd890, 4));
  halo.position.set(0, 2.02, -0.2);
  m.parts.halo = halo;
  // The gauntlet follows the aim.
  const aim = new THREE.Group();
  const gaunt = new THREE.Mesh(cg('Octahedron', [1, 0], () => new THREE.OctahedronGeometry(1, 0)), glow(0xffffff, 6));
  gaunt.scale.set(0.16, 0.1, 0.1);
  gaunt.position.set(0.45, 1.4, 0);
  const gauntGlow = new THREE.Mesh(cg('Octahedron', [1, 0], () => new THREE.OctahedronGeometry(1, 0)), glow(0x2ef2ff, 3));
  gauntGlow.scale.set(0.26, 0.16, 0.16);
  gauntGlow.position.copy(gaunt.position);
  const blade = new THREE.Mesh(cg('Cone', [0.05, 0.7, 4], () => new THREE.ConeGeometry(0.05, 0.7, 4)), glow(0xbff8ff, 3));
  blade.rotation.z = -Math.PI / 2;
  blade.position.set(0.95, 1.4, 0);
  aim.add(gaunt, gauntGlow, blade);
  m.parts.aim = aim;
  const shards = new THREE.Group();
  for (let i = 0; i < 3; i++) {
    const sh = new THREE.Mesh(G.tetra, glow(i === 1 ? 0xff2fb9 : 0x2ef2ff, 3));
    sh.scale.setScalar(0.09);
    sh.position.set(Math.cos((i / 3) * Math.PI * 2) * 0.7, 1.5, Math.sin((i / 3) * Math.PI * 2) * 0.7);
    shards.add(sh);
  }
  m.parts.shards = shards;
  m.flash.push(head.material as THREE.MeshBasicMaterial);
  m.base.push((head.material as THREE.MeshBasicMaterial).color.clone());
  body.add(torso, hips, seam, head, hair, halo, shards, aim);
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
    const s = new THREE.Mesh(cg('Cone', [0.08, 0.35, 4], () => new THREE.ConeGeometry(0.08, 0.35, 4)), glow(0xffc34a, 4));
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
    const mouth = core(m, color, 4, cg('Torus', [0.42, 0.09, 8, 24], () => new THREE.TorusGeometry(0.42, 0.09, 8, 24)));
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
    const ringM = new THREE.Mesh(cg('Torus', [r, r * 0.28, 12, 48], () => new THREE.TorusGeometry(r, r * 0.28, 12, 48)), rimmed(m, color));
    ringM.rotation.x = Math.PI / 2;
    const inner = core(m, color, 4, cg('Torus', [r * 0.62, r * 0.07, 8, 48], () => new THREE.TorusGeometry(r * 0.62, r * 0.07, 8, 48)));
    inner.rotation.x = Math.PI / 2;
    for (let i = 0; i < 6; i++) {
      const sp = new THREE.Mesh(cg('Cone', [r * 0.16, r * 0.7, 4], () => new THREE.ConeGeometry(r * 0.16, r * 0.7, 4)), rimmed(m, color, 0x12121e));
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
    const tip = core(m, color, 4, cg('Cone', [r * 0.22, r * 1.1, 4], () => new THREE.ConeGeometry(r * 0.22, r * 1.1, 4)));
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
    const b = new THREE.Mesh(cg('Cylinder', [r * 0.75, r * 0.9, r * 1.6, 6], () => new THREE.CylinderGeometry(r * 0.75, r * 0.9, r * 1.6, 6)), rimmed(m, color));
    b.position.y = r * 0.8;
    const c = core(m, color, 3, cg('Cylinder', [r * 0.3, r * 0.3, r * 1.62, 6], () => new THREE.CylinderGeometry(r * 0.3, r * 0.3, r * 1.62, 6)));
    c.position.y = r * 0.8;
    // The shield: a curved wall in front, gold-edged.
    const sh = new THREE.Group();
    const wall = new THREE.Mesh(cg('Cylinder', [r * 1.45, r * 1.45, r * 1.5, 24, 1, true, -1.05, 2.1], () => new THREE.CylinderGeometry(r * 1.45, r * 1.45, r * 1.5, 24, 1, true, -1.05, 2.1)), rimmed(m, color, 0x161208, 0.1));
    (wall.material as THREE.MeshStandardMaterial).side = THREE.DoubleSide;
    wall.position.y = r * 0.8;
    const lip = new THREE.Mesh(cg('Torus', [r * 1.45, 0.05, 6, 24, 2.1], () => new THREE.TorusGeometry(r * 1.45, 0.05, 6, 24, 2.1)), glow(0xffc34a, 4));
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
    const bell = new THREE.Mesh(cg('Lathe', [pts, 24], () => new THREE.LatheGeometry(pts, 24)), rimmed(m, color));
    (bell.material as THREE.MeshStandardMaterial).side = THREE.DoubleSide;
    const mouth = core(m, color, 4, cg('Circle', [r * 1.05, 24], () => new THREE.CircleGeometry(r * 1.05, 24)));
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
    const iris = core(m, color, 5, cg('Torus', [r * 0.5, r * 0.08, 8, 32], () => new THREE.TorusGeometry(r * 0.5, r * 0.08, 8, 32)));
    iris.rotation.y = Math.PI / 2;
    iris.position.x = r * 0.86;
    const pupil = new THREE.Mesh(G.sphere, glow(0xffffff, 6));
    pupil.scale.setScalar(r * 0.22);
    pupil.position.x = r * 0.9;
    const ringO = new THREE.Mesh(cg('Torus', [r * 1.4, 0.03, 6, 48], () => new THREE.TorusGeometry(r * 1.4, 0.03, 6, 48)), glow(color, 2));
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
      const t = new THREE.Mesh(cg('Torus', [r * (0.9 - i * 0.2), 0.08, 8, 32], () => new THREE.TorusGeometry(r * (0.9 - i * 0.2), 0.08, 8, 32)), rimmed(m, color));
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

export function lookNames(): string[] {
  return Object.keys(BUILD);
}

export function registerLook(name: string, f: (r: number, color: number) => Model): void {
  BUILD[name] = f;
}

export { model as newModel, core as addCore, edges as edgeLines, G as GEO };
