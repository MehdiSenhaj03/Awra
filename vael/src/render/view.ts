import { AdjustmentFilter, AdvancedBloomFilter, KawaseBlurFilter, RGBSplitFilter, ShockwaveFilter } from 'pixi-filters';
import { type Application, Container, Graphics, RenderTexture, Sprite, Texture, TilingSprite } from 'pixi.js';
import { Rng, TILE, VIEW_H, VIEW_W, clamp, hash } from '../engine/core';
import { Camera } from '../game/camera';
import type { FxEvent } from '../game/events';
import type { Scene } from '../game/scene';
import { T, isSolid, tileAt } from '../world/room';
import { hasTex, tex } from './assets';
import { HeroRig } from './hero';
import { TerrainMesh } from './terrain';
import { fxTextures } from './textures';
import { Vfx } from './vfx';
import { type ZoneLook, zoneLook } from './zones';
import { EntityViews } from './entities';
import { FxDirector } from './director';

/** Everything drawn: layers, camera, lighting and post-processing. Reads the scene, never changes it. */
export class GameView {
  readonly root = new Container();
  /** Screen-space layers behind the world (paintings with parallax). */
  private far = new Container();
  private mid = new Container();
  /** Camera-space world. */
  readonly world = new Container();
  private back = new Container();
  private terrainLayer = new Container();
  private platLayer = new Container();
  private gates = new Container();
  private lastTiles = -1;
  readonly actors = new Container();
  readonly heroLayer = new Container();
  private lightOverlay: Sprite;
  readonly fxLayer = new Container();
  private fgLayer = new Container();
  private screenFx = new Container();
  /** Lights drawn into a low-resolution map multiplied over the world. */
  private lights = new Container();
  private lightRT: RenderTexture;
  private ambient = new Sprite();
  readonly camera = new Camera();
  readonly vfx = new Vfx();
  private bloom = new AdvancedBloomFilter({ threshold: 0.58, bloomScale: 0.85, brightness: 1.0, blur: 7, quality: 6 });
  readonly grade = new AdjustmentFilter({ saturation: 1.05, contrast: 1.06, gamma: 1 });
  private rgb = new RGBSplitFilter({ red: { x: 0, y: 0 }, green: { x: 0, y: 0 }, blue: { x: 0, y: 0 } });
  private shocks: { f: ShockwaveFilter; t: number; on: boolean }[] = [];
  private terrain: TerrainMesh | null = null;
  private look: ZoneLook = zoneLook('couronne');
  private farSprites: Sprite[] = [];
  private midSprites: { s: Sprite; px: number; py: number }[] = [];
  private fgSprites: { s: Sprite; px: number; py: number }[] = [];
  readonly hero: HeroRig;
  readonly ents: EntityViews;
  readonly director: FxDirector;
  private flashRect = new Graphics();
  private vignette: Sprite;
  private scaleFactor = 1;
  private roomId = '';
  private roomW = VIEW_W;
  private roomH = VIEW_H;
  /** Screen flash level and colour. */
  flash = 0;
  flashColor = 0xffffff;
  chroma = 0;
  slowmo = 0;
  private lightPool: Sprite[] = [];
  private lightUsed = 0;
  private motes: { s: Sprite; x: number; y: number; vx: number; vy: number; ph: number }[] = [];
  /** Lights fixed in the decor (lamps, candles, crystals). */
  private decorLights: { x: number; y: number; r: number; c: number; ph: number }[] = [];

  constructor(public app: Application) {
    const r = this.root;
    r.addChild(this.far, this.mid, this.world, this.screenFx);
    // Depth of field: the far painting and the mid props are softened.
    this.far.filters = [new KawaseBlurFilter({ strength: 1.2, quality: 2 })];
    this.mid.filters = [new KawaseBlurFilter({ strength: 1, quality: 2 })];
    this.world.addChild(this.back, this.terrainLayer, this.platLayer, this.gates, this.actors, this.heroLayer);
    this.lightRT = RenderTexture.create({ width: VIEW_W / 2, height: VIEW_H / 2 });
    this.lightOverlay = new Sprite(this.lightRT);
    this.lightOverlay.scale.set(2);
    this.lightOverlay.blendMode = 'multiply';
    r.addChild(this.lightOverlay);
    // Additive effects and the foreground draw above the lighting so they glow.
    const fxWorld = new Container();
    fxWorld.addChild(this.vfx.normal, this.fxLayer, this.vfx.add);
    this.fxWorld = fxWorld;
    r.addChild(fxWorld, this.fgLayer);
    this.ambient.texture = Texture.WHITE;
    this.lights.addChild(this.ambient);
    const ft = fxTextures();
    this.vignette = new Sprite(ft.vignette);
    this.vignette.width = VIEW_W;
    this.vignette.height = VIEW_H;
    this.vignette.alpha = 0.45;
    this.screenFx.addChild(this.vignette);
    r.addChild(this.screenFx);
    this.screenFx.addChild(this.flashRect);
    r.filters = [this.grade, this.bloom, this.rgb];
    this.hero = new HeroRig(this);
    this.ents = new EntityViews(this);
    this.director = new FxDirector(this);
    for (let i = 0; i < 3; i++) {
      const f = new ShockwaveFilter({ amplitude: 18, wavelength: 140, speed: 900, brightness: 1.15, radius: -1 });
      this.shocks.push({ f, t: 0, on: false });
    }
  }

  private fxWorld: Container;

  resize(): void {
    const w = this.app.screen.width;
    const h = this.app.screen.height;
    const s = Math.min(w / VIEW_W, h / VIEW_H);
    this.scaleFactor = s;
    this.root.scale.set(s);
    this.root.position.set(Math.round((w - VIEW_W * s) / 2), Math.round((h - VIEW_H * s) / 2));
    // Clip to the 16:9 view.
    const m = (this.root.mask as Graphics | null) ?? new Graphics();
    m.clear().rect(this.root.position.x, this.root.position.y, VIEW_W * s, VIEW_H * s).fill(0xffffff);
    if (!this.root.mask) {
      this.app.stage.addChild(m);
      this.root.mask = m;
    }
  }

  get pixelScale(): number {
    return this.scaleFactor;
  }

  /** Builds the visuals of the scene's current room. */
  buildRoom(scene: Scene): void {
    const room = scene.room;
    this.roomId = room.id;
    this.roomW = room.pxW;
    this.roomH = room.pxH;
    this.look = zoneLook(room.def.zone);
    const L = this.look;
    const rng = new Rng(hash(room.id));
    this.camera.boundsW = room.pxW;
    this.camera.boundsH = room.pxH;
    this.vfx.clear();
    this.ents.clear();
    for (const c of [this.far, this.mid, this.back, this.terrainLayer, this.platLayer, this.fgLayer]) {
      for (const ch of c.removeChildren()) ch.destroy();
    }
    this.terrain?.destroy();
    this.farSprites = [];
    this.midSprites = [];
    this.fgSprites = [];
    this.motes = [];
    this.decorLights = [];

    // Far painting.
    const bgName = room.def.bg && hasTex(room.def.bg) ? room.def.bg : L.bgs.filter(hasTex)[hash(room.id, 'bg') % Math.max(1, L.bgs.filter(hasTex).length)] ?? L.bgs[0];
    const far = new Sprite(tex(bgName));
    far.tint = L.bgTint;
    this.far.addChild(far);
    this.farSprites.push(far);
    const fog = new Sprite(fxTextures().shaft);
    fog.tint = L.fog;
    fog.alpha = 0.0;
    this.far.addChild(fog);

    // Mid layer: big props, darkened and hazed, with parallax 0.45.
    const midW = VIEW_W + (room.pxW - VIEW_W) * 0.45 + 400;
    const midN = Math.max(2, Math.round(midW / 520));
    const mids = L.mid.filter(hasTex);
    for (let i = 0; i < midN && mids.length; i++) {
      const s = new Sprite(tex(rng.pick(mids)));
      s.anchor.set(0.5, 1);
      const h = rng.range(520, 900);
      s.scale.set(h / s.texture.height * (rng.chance(0.5) ? 1 : -1), h / s.texture.height);
      s.tint = 0x2b3350;
      s.alpha = 0.85;
      this.mid.addChild(s);
      this.midSprites.push({ s, px: (i + rng.range(0.1, 0.9)) * (midW / midN) - 200, py: VIEW_H + Math.max(0, room.pxH - VIEW_H) * 0.45 + 60 + rng.range(0, 140) });
    }
    const haze = new Graphics().rect(0, 0, VIEW_W, VIEW_H).fill({ color: L.fog, alpha: 0.35 });
    this.mid.addChild(haze);

    // Terrain.
    const rock = tex(hasTex(L.tex) ? L.tex : 'tex_couronne').source;
    this.terrain = new TerrainMesh(room, { rock, rim: L.rim, deep: L.deep, top: L.top, rockScale: 300 });
    this.terrainLayer.addChild(this.terrain.mesh);
    this.decorate(scene, rng);

    // Floating motes of light.
    const ft = fxTextures();
    const n = Math.round((room.pxW * room.pxH) / (VIEW_W * VIEW_H) * 36);
    for (let i = 0; i < n; i++) {
      const s = new Sprite(ft.dot);
      s.anchor.set(0.5);
      s.blendMode = 'add';
      s.tint = L.motes;
      s.scale.set(rng.range(0.1, 0.32));
      this.fxLayer.addChild(s);
      this.motes.push({ s, x: rng.range(0, room.pxW), y: rng.range(0, room.pxH), vx: rng.range(-0.25, 0.25), vy: rng.range(-0.5, -0.1), ph: rng.range(0, 6.28) });
    }
    this.hero.reset();
    this.lastTiles = -1;
  }

  /** Places decor along the floors, edge strips on the top surfaces, platforms, spikes and foreground silhouettes. */
  private decorate(scene: Scene, rng: Rng): void {
    const room = scene.room;
    const L = this.look;
    const backs = L.back.filter(hasTex);
    // Top surfaces: runs of solid tiles with air above.
    const runs: { x0: number; x1: number; y: number }[] = [];
    for (let y = 1; y < room.h; y++) {
      let x0 = -1;
      for (let x = 0; x <= room.w; x++) {
        const top = x < room.w && isSolid(tileAt(room, x, y)) && !isSolid(tileAt(room, x, y - 1)) && tileAt(room, x, y - 1) !== T.Spike;
        if (top && x0 < 0) x0 = x;
        if (!top && x0 >= 0) {
          runs.push({ x0, x1: x, y });
          x0 = -1;
        }
      }
    }
    const names: string[] = [];
    const edgeTex = hasTex(L.edge) ? tex(L.edge) : null;
    for (const r of runs) {
      const w = (r.x1 - r.x0) * TILE;
      if (edgeTex) {
        const ts = new TilingSprite({ texture: edgeTex, width: w + 24, height: edgeTex.height * 0.42 });
        ts.tileScale.set(0.42);
        ts.tilePosition.x = rng.range(0, 400);
        ts.position.set(r.x0 * TILE - 12, r.y * TILE - edgeTex.height * 0.42 * 0.36);
        ts.tint = 0xb8c0e0;
        this.platLayer.addChild(ts);
      }
      // Props standing on long enough floors, behind the action.
      if (backs.length && w >= 240) {
        const count = Math.floor(w / 520) + (rng.chance(0.6) ? 1 : 0);
        for (let i = 0; i < count; i++) {
          names.push(rng.pick(backs));
          const s = new Sprite(tex(names[names.length - 1]));
          s.anchor.set(0.5, 0.97);
          const h = rng.range(130, 300);
          s.scale.set((h / s.texture.height) * (rng.chance(0.5) ? 1 : -1), h / s.texture.height);
          s.position.set(r.x0 * TILE + rng.range(60, w - 60), r.y * TILE + 6);
          const glows = (L.glow ?? []).includes(names[names.length - 1]);
          s.tint = glows ? 0xb8c0e0 : 0x5a6488;
          s.alpha = 0.92;
          this.back.addChild(s);
          if (glows) this.decorLights.push({ x: s.x, y: s.y - h * 0.5, r: h * 1.6, c: L.motes, ph: rng.range(0, 6) });
        }
      }
    }
    // Props hanging from ceilings: lanterns, banners, chains, roots.
    const hangs = (L.hang ?? []).filter(hasTex);
    if (hangs.length) {
      for (let y = 1; y < room.h - 1; y++) {
        let x0 = -1;
        for (let x = 0; x <= room.w; x++) {
          const ceil = x < room.w && isSolid(tileAt(room, x, y)) && !isSolid(tileAt(room, x, y + 1));
          if (ceil && x0 < 0) x0 = x;
          if (!ceil && x0 >= 0) {
            const w = (x - x0) * TILE;
            if (w >= 280 && rng.chance(0.75)) {
              const name = rng.pick(hangs);
              const s = new Sprite(tex(name));
              s.anchor.set(0.5, 0.02);
              const h = rng.range(160, 360) * (name.includes('chain') ? 1.6 : 1);
              s.scale.set(h / s.texture.height);
              s.position.set(x0 * TILE + rng.range(80, w - 80), (y + 1) * TILE - 4);
              const glows = (L.glow ?? []).includes(name);
              s.tint = glows ? 0xd0d4f0 : 0x6a7498;
              this.back.addChild(s);
              if (glows) this.decorLights.push({ x: s.x, y: s.y + h * 0.75, r: 420, c: 0xffe0a8, ph: rng.range(0, 6) });
            }
            x0 = -1;
          }
        }
      }
    }
    // One-way platforms.
    const plat = hasTex(L.plat) ? tex(L.plat) : null;
    for (const [x, y, w] of room.def.plat ?? []) {
      if (plat) {
        const s = new Sprite(plat);
        s.anchor.set(0.5, 0.18);
        s.width = w * TILE + 30;
        s.scale.y = Math.abs(s.scale.x);
        s.position.set((x + w / 2) * TILE, y * TILE);
        this.platLayer.addChild(s);
      } else {
        const g = new Graphics();
        g.roundRect(x * TILE - 6, y * TILE, w * TILE + 12, 18, 6).fill(0x1d2440).stroke({ color: 0xc8b48a, width: 2, alpha: 0.6 });
        this.platLayer.addChild(g);
      }
    }
    // Spikes: rows of crystal thorns.
    const crystal = hasTex('prop_crystals') ? tex('prop_crystals') : null;
    for (let ty = 0; ty < room.h; ty++) for (let tx = 0; tx < room.w; tx++) {
      if (tileAt(room, tx, ty) !== T.Spike) continue;
      if (crystal) {
        const s = new Sprite(crystal);
        s.anchor.set(0.5, 0.92);
        const up = isSolid(tileAt(room, tx, ty + 1)) || !isSolid(tileAt(room, tx, ty - 1));
        const h = rng.range(52, 74);
        s.scale.set((h / crystal.height) * (rng.chance(0.5) ? 1 : -1), (h / crystal.height) * (up ? 1 : -1));
        s.position.set(tx * TILE + TILE / 2 + rng.range(-6, 6), up ? (ty + 1) * TILE + 4 : ty * TILE - 4);
        s.tint = 0xd0d8ff;
        this.platLayer.addChild(s);
      }
    }
    // Still water.
    for (const [x, y, w, h] of room.def.water ?? []) {
      const g = new Graphics();
      g.rect(x * TILE, y * TILE + 10, w * TILE, h * TILE - 10).fill({ color: 0x0a3040, alpha: 0.55 });
      g.rect(x * TILE, y * TILE + 8, w * TILE, 3).fill({ color: 0x9ff0ff, alpha: 0.6 });
      this.platLayer.addChild(g);
    }
    // Polished rock: a glassy sheen so the hero learns the blade will not bite.
    for (const [x, y, w, h] of room.def.slick ?? []) {
      const g = new Graphics();
      const X = x * TILE;
      const Y = y * TILE;
      const W = w * TILE;
      const H = h * TILE;
      g.rect(X, Y, W, H).fill({ color: 0x9fb8ff, alpha: 0.07 });
      for (let i = -H; i < W; i += 46) {
        const x0 = Math.max(X, X + i);
        const y0 = Y + Math.max(0, -i);
        const len = Math.min(W - Math.max(0, i), H - Math.max(0, -i));
        if (len > 0) g.moveTo(x0, y0).lineTo(x0 + len, y0 + len).stroke({ color: 0xcfe0ff, width: 3, alpha: 0.18 });
      }
      g.rect(X, Y, W, H).stroke({ color: 0xcfe0ff, width: 2, alpha: 0.35 });
      this.platLayer.addChild(g);
    }
    // Gloam pools.
    for (const [x, y, w, h] of room.def.gloam ?? []) {
      const g = new Graphics();
      g.rect(x * TILE, y * TILE + 8, w * TILE, h * TILE - 8).fill({ color: L.pool, alpha: 0.95 });
      g.rect(x * TILE, y * TILE + 6, w * TILE, 5).fill({ color: L.poolGlow, alpha: 0.9 });
      g.rect(x * TILE, y * TILE - 10, w * TILE, 16).fill({ color: L.poolGlow, alpha: 0.12 });
      this.platLayer.addChild(g);
    }
    // Foreground silhouettes (parallax 1.25), sparse.
    const fgs = L.fg.filter(hasTex);
    const nfg = fgs.length ? Math.max(1, Math.round(room.pxW / 1600)) : 0;
    for (let i = 0; i < nfg; i++) {
      const s = new Sprite(tex(rng.pick(fgs)));
      s.anchor.set(0.5, 1);
      const sc = rng.range(0.8, 1.2);
      s.scale.set(sc * (rng.chance(0.5) ? 1 : -1), sc);
      s.tint = 0x0a0c18;
      this.fgLayer.addChild(s);
      this.fgSprites.push({ s, px: rng.range(0, room.pxW * 1.25), py: VIEW_H + Math.max(0, room.pxH - VIEW_H) * 1.25 + 60 });
    }
  }

  /** Iron gates, black crystal and boss seals that are currently closed. */
  private drawGates(scene: Scene): void {
    for (const ch of this.gates.removeChildren()) ch.destroy();
    const room = scene.room;
    const g = new Graphics();
    this.gates.addChild(g);
    for (const [x, y, w, h, flag] of room.def.brk ?? []) {
      if (flag.startsWith('crack_')) continue;
      if (tileAt(room, x, y) !== T.Breakable) continue;
      const X = x * TILE;
      const Y = y * TILE;
      const W = w * TILE;
      const H = h * TILE;
      if (flag.startsWith('gate_')) {
        g.rect(X, Y, W, H).fill({ color: 0x05060c, alpha: 0.35 });
        for (let i = 0; i < Math.max(2, Math.round(W / 22)); i++) {
          const bx = X + 8 + (i * (W - 16)) / Math.max(1, Math.round(W / 22) - 1);
          g.rect(bx - 4, Y, 8, H).fill(0x1c2236).stroke({ color: 0x8a96c0, width: 1.5, alpha: 0.6 });
        }
        g.rect(X - 6, Y, W + 12, 14).fill(0x262c44).stroke({ color: 0xc8b48a, width: 2, alpha: 0.7 });
        g.rect(X - 6, Y + H - 14, W + 12, 14).fill(0x262c44).stroke({ color: 0xc8b48a, width: 2, alpha: 0.7 });
      } else if (flag.startsWith('crys_')) {
        const cr = hasTex('prop_crystals') ? tex('prop_crystals') : null;
        g.rect(X, Y, W, H).fill({ color: 0x14081e, alpha: 0.9 }).stroke({ color: 0xb070ff, width: 3, alpha: 0.8 });
        if (cr) {
          for (let i = 0; i < Math.ceil(H / 60); i++) {
            const s = new Sprite(cr);
            s.anchor.set(0.5);
            s.scale.set(90 / cr.height);
            s.rotation = (i % 2 ? 1 : -1) * 0.4;
            s.position.set(X + W / 2, Y + 30 + i * 60);
            s.tint = 0x6a3aa0;
            this.gates.addChild(s);
          }
        }
      } else {
        // Boss seal: a wall of light.
        g.rect(X, Y, W, H).fill({ color: 0xffe0a0, alpha: 0.18 }).stroke({ color: 0xffe6b0, width: 3, alpha: 0.9 });
        for (let i = 0; i < 6; i++) g.moveTo(X + W / 2, Y + (H * i) / 6).lineTo(X + W / 2, Y + (H * (i + 0.6)) / 6).stroke({ color: 0xfff4dc, width: 4, alpha: 0.6 });
      }
    }
  }

  /** Adds a light for this frame (screen space computed from world coordinates). */
  light(x: number, y: number, radius: number, color: number, alpha = 1): void {
    let s = this.lightPool[this.lightUsed];
    if (!s) {
      s = new Sprite(fxTextures().light);
      s.anchor.set(0.5);
      s.blendMode = 'add';
      this.lightPool.push(s);
      this.lights.addChild(s);
    }
    this.lightUsed++;
    s.visible = true;
    const cam = this.camera;
    const z = cam.zoom;
    s.position.set(((x - cam.rx) * z) / 2, ((y - cam.ry) * z) / 2);
    s.scale.set((radius * z) / 256);
    s.tint = color;
    s.alpha = alpha;
  }

  shockwave(x: number, y: number, amp = 18): void {
    const sh = this.shocks.find((s) => !s.on) ?? this.shocks[0];
    sh.on = true;
    sh.t = 0;
    const cam = this.camera;
    const sx = (x - cam.rx) * cam.zoom * this.scaleFactor + this.root.position.x;
    const sy = (y - cam.ry) * cam.zoom * this.scaleFactor + this.root.position.y;
    sh.f.center = { x: sx, y: sy };
    sh.f.amplitude = amp * this.scaleFactor;
    sh.f.wavelength = 160 * this.scaleFactor;
    sh.f.speed = 1100 * this.scaleFactor;
    sh.f.time = 0;
  }

  events(evs: FxEvent[], scene: Scene): void {
    for (const e of evs) this.director.handle(e, scene);
  }

  /** Draws one frame. */
  render(scene: Scene, frameDt: number): void {
    if (scene.room.id !== this.roomId) this.buildRoom(scene);
    if (scene.tilesChanged !== this.lastTiles) {
      this.lastTiles = scene.tilesChanged;
      this.terrain?.refresh(scene.room);
      this.drawGates(scene);
    }
    const p = scene.player;
    const cam = this.camera;
    cam.focus = scene.cameraFocus;
    if (!scene.freeze) cam.update(p.cx, p.cy, p.facing, p.body.vy, p.body.onGround);
    const z = cam.zoom;
    this.world.scale.set(z);
    this.world.position.set(-cam.rx * z, -cam.ry * z);
    this.world.rotation = cam.angle;
    this.fxWorld.scale.set(z);
    this.fxWorld.position.copyFrom(this.world.position);
    this.fxWorld.rotation = cam.angle;

    // Parallax.
    const spanX = Math.max(1, this.roomW - VIEW_W);
    const spanY = Math.max(1, this.roomH - VIEW_H);
    const kx = clamp(cam.rx / spanX, 0, 1);
    const ky = clamp(cam.ry / spanY, 0, 1);
    for (const s of this.farSprites) {
      const needW = VIEW_W + spanX * 0.08 + 60;
      const needH = VIEW_H + spanY * 0.08 + 60;
      const sc = Math.max(needW / s.texture.width, needH / s.texture.height);
      s.scale.set(sc);
      s.position.set(-(s.texture.width * sc - VIEW_W) * kx, -(s.texture.height * sc - VIEW_H) * ky);
    }
    for (const m of this.midSprites) m.s.position.set(m.px - cam.rx * 0.45, m.py - cam.ry * 0.45);
    for (const f of this.fgSprites) f.s.position.set(f.px - cam.rx * 1.25, f.py - cam.ry * 1.25);

    // Actors.
    this.hero.update(scene, frameDt);
    this.ents.update(scene, frameDt);
    this.vfx.update(this.slowmo > 0 ? 0.4 : 1);
    this.director.update(scene);

    // Motes drift.
    const t = performance.now() / 1000;
    for (const m of this.motes) {
      m.x += m.vx + Math.sin(t * 0.7 + m.ph) * 0.3;
      m.y += m.vy;
      if (m.y < -20) m.y = this.roomH + 20;
      m.s.position.set(m.x, m.y);
      m.s.alpha = 0.35 + Math.sin(t * 2 + m.ph) * 0.3;
    }

    // Lighting.
    this.lightUsed = 0;
    this.ambient.width = VIEW_W / 2;
    this.ambient.height = VIEW_H / 2;
    this.ambient.tint = this.look.ambient;
    this.light(p.cx, p.cy - 20, 640, this.look.hero, 0.9);
    if (scene.blade.out()) this.light(scene.blade.x, scene.blade.y, 300, 0xffe0a0, 0.7);
    this.ents.lights(scene);
    const tt = performance.now() / 1000;
    for (const d of this.decorLights) this.light(d.x, d.y, d.r * (0.95 + Math.sin(tt * 2.3 + d.ph) * 0.05), d.c, 0.75);
    this.director.lights();
    for (let i = this.lightUsed; i < this.lightPool.length; i++) this.lightPool[i].visible = false;
    this.app.renderer.render({ container: this.lights, target: this.lightRT, clear: true });

    // Post.
    if (this.flash > 0) this.flash = Math.max(0, this.flash - 0.08);
    this.flashRect.clear();
    if (this.flash > 0) this.flashRect.rect(0, 0, VIEW_W, VIEW_H).fill({ color: this.flashColor, alpha: this.flash });
    this.chroma *= 0.85;
    const c = this.chroma * this.scaleFactor;
    this.rgb.red = { x: -c, y: 0 };
    this.rgb.blue = { x: c, y: 0 };
    const eclipse = scene.eclipse > 0 ? Math.min(1, scene.eclipse / 15) : 0;
    this.slowmo = scene.eclipse;
    this.grade.saturation = 1.05 - eclipse * 0.85;
    this.grade.contrast = 1.06 + eclipse * 0.12;
    const active = [this.grade, this.bloom] as (AdjustmentFilter | AdvancedBloomFilter | RGBSplitFilter | ShockwaveFilter)[];
    if (Math.abs(this.chroma) > 0.2) active.push(this.rgb);
    for (const sh of this.shocks) {
      if (!sh.on) continue;
      sh.t += frameDt / 60;
      sh.f.time = sh.t;
      if (sh.t > 0.6) sh.on = false;
      else active.push(sh.f);
    }
    this.root.filters = active;
  }
}
