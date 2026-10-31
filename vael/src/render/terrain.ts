import { BufferImageSource, Geometry, Mesh, Shader, Texture, type TextureSource } from 'pixi.js';
import { TILE } from '../engine/core';
import { type Room, rockTile, tileAt } from '../world/room';

/** Margin of rock drawn around the room so the edges of the view never show emptiness. */
const MARGIN = 10;

export interface TerrainLook {
  rock: TextureSource;
  /** Lit rim, deep interior, light on top surfaces (0..1 RGB). */
  rim: [number, number, number];
  deep: [number, number, number];
  top: [number, number, number];
  /** Rock texture scale in pixels per repeat. */
  rockScale: number;
}

const vertex = `
in vec2 aPosition;
out vec2 vPos;
uniform mat3 uProjectionMatrix;
uniform mat3 uWorldTransformMatrix;
uniform mat3 uTransformMatrix;
void main() {
  mat3 mvp = uProjectionMatrix * uWorldTransformMatrix * uTransformMatrix;
  gl_Position = vec4((mvp * vec3(aPosition, 1.0)).xy, 0.0, 1.0);
  vPos = aPosition;
}`;

const fragment = `
in vec2 vPos;
out vec4 finalColor;
uniform sampler2D uDist;
uniform sampler2D uRock;
uniform vec2 uGrid;
uniform vec2 uOrigin;
uniform vec3 uRim;
uniform vec3 uDeep;
uniform vec3 uTop;
uniform float uRockScale;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
  vec2 i = floor(p); vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y) * 2.0 - 1.0;
}
float sdAt(vec2 p) {
  vec2 t = (p - uOrigin) / ${TILE.toFixed(1)};
  return texture(uDist, t / uGrid).r * 16.0 - 8.0;
}
void main() {
  float n = vnoise(vPos * 0.03) * 0.28 + vnoise(vPos * 0.09) * 0.1;
  float d = sdAt(vPos) + n;
  float px = d * ${TILE.toFixed(1)};
  float a = smoothstep(-1.2, 1.2, px);
  if (a <= 0.001) discard;
  float e = 5.0;
  vec2 g = vec2(sdAt(vPos + vec2(e, 0.0)) - sdAt(vPos - vec2(e, 0.0)), sdAt(vPos + vec2(0.0, e)) - sdAt(vPos - vec2(0.0, e)));
  vec2 nrm = -g / max(length(g), 1e-4);
  vec3 rock = texture(uRock, vPos / uRockScale).rgb;
  float depth = smoothstep(6.0, 170.0, px);
  vec3 col = mix(rock * (uRim * 0.8 + 0.3), uDeep, depth * 0.95);
  // Light catches the surfaces facing up, a cold rim elsewhere.
  float up = clamp(-nrm.y, 0.0, 1.0);
  float band = 1.0 - smoothstep(0.0, 26.0, px);
  col += uTop * up * band * 1.1;
  col += uRim * (1.0 - smoothstep(0.0, 4.0, px)) * 0.55;
  // Faint inner veins of starlight.
  float vein = smoothstep(0.82, 0.95, vnoise(vPos * 0.012 + vec2(vnoise(vPos * 0.03) * 1.5)) * 0.5 + 0.5);
  col += uTop * vein * 0.18 * (1.0 - depth * 0.7);
  finalColor = vec4(col * a, a);
}`;

/** Signed distance (in tiles) from each tile centre to the solid boundary: positive inside rock. */
function distanceField(room: Room): { data: Uint8Array; gw: number; gh: number } {
  const gw = room.w + MARGIN * 2;
  const gh = room.h + MARGIN * 2;
  const solid = new Uint8Array(gw * gh);
  for (let y = 0; y < gh; y++) for (let x = 0; x < gw; x++) solid[y * gw + x] = rockTile(tileAt(room, x - MARGIN, y - MARGIN)) ? 1 : 0;
  // Gates, black crystal and boss doors are drawn as objects, not rock.
  for (const [bx, by, bw, bh, flag] of room.def.brk ?? []) {
    if (flag.startsWith('crack_')) continue;
    for (let y = by; y < by + bh; y++) for (let x = bx; x < bx + bw; x++) solid[(y + MARGIN) * gw + x + MARGIN] = 0;
  }
  // Two-pass chamfer distance to the nearest tile of the other kind.
  const INF = 1e6;
  const dist = new Float32Array(gw * gh).fill(INF);
  for (let y = 0; y < gh; y++) {
    for (let x = 0; x < gw; x++) {
      const s = solid[y * gw + x];
      const edge =
        (x > 0 && solid[y * gw + x - 1] !== s) || (x < gw - 1 && solid[y * gw + x + 1] !== s) ||
        (y > 0 && solid[(y - 1) * gw + x] !== s) || (y < gh - 1 && solid[(y + 1) * gw + x] !== s);
      if (edge) dist[y * gw + x] = 0.5;
    }
  }
  const D = Math.SQRT2;
  const relax = (i: number, j: number, c: number) => {
    if (solid[i] === solid[j] && dist[j] + c < dist[i]) dist[i] = dist[j] + c;
  };
  for (let y = 0; y < gh; y++) for (let x = 0; x < gw; x++) {
    const i = y * gw + x;
    if (x > 0) relax(i, i - 1, 1);
    if (y > 0) relax(i, i - gw, 1);
    if (x > 0 && y > 0) relax(i, i - gw - 1, D);
    if (x < gw - 1 && y > 0) relax(i, i - gw + 1, D);
  }
  for (let y = gh - 1; y >= 0; y--) for (let x = gw - 1; x >= 0; x--) {
    const i = y * gw + x;
    if (x < gw - 1) relax(i, i + 1, 1);
    if (y < gh - 1) relax(i, i + gw, 1);
    if (x < gw - 1 && y < gh - 1) relax(i, i + gw + 1, D);
    if (x > 0 && y < gh - 1) relax(i, i + gw - 1, D);
  }
  const data = new Uint8Array(gw * gh * 4);
  for (let i = 0; i < gw * gh; i++) {
    const d = Math.min(dist[i], 8);
    const sd = solid[i] ? d : -d;
    const v = Math.round(Math.max(0, Math.min(1, (sd + 8) / 16)) * 255);
    data[i * 4] = v;
    data[i * 4 + 1] = v;
    data[i * 4 + 2] = v;
    data[i * 4 + 3] = 255;
  }
  return { data, gw, gh };
}

export class TerrainMesh {
  mesh: Mesh<Geometry, Shader>;
  private distTex: Texture;

  constructor(room: Room, look: TerrainLook) {
    const { data, gw, gh } = distanceField(room);
    const source = new BufferImageSource({ resource: data, width: gw, height: gh, format: 'rgba8unorm', alphaMode: 'no-premultiply-alpha' });
    source.style.scaleMode = 'linear';
    source.style.addressMode = 'clamp-to-edge';
    this.distTex = new Texture({ source });
    look.rock.style.addressMode = 'repeat';
    look.rock.style.scaleMode = 'linear';
    const x0 = -MARGIN * TILE;
    const y0 = -MARGIN * TILE;
    const x1 = x0 + gw * TILE;
    const y1 = y0 + gh * TILE;
    const geometry = new Geometry({
      attributes: { aPosition: [x0, y0, x1, y0, x1, y1, x0, y1] },
      indexBuffer: [0, 1, 2, 0, 2, 3],
    });
    const shader = Shader.from({
      gl: { vertex, fragment, name: 'vael-terrain' },
      resources: {
        uDist: source,
        uRock: look.rock,
        terrain: {
          uGrid: { value: [gw, gh], type: 'vec2<f32>' },
          uOrigin: { value: [x0, y0], type: 'vec2<f32>' },
          uRim: { value: look.rim, type: 'vec3<f32>' },
          uDeep: { value: look.deep, type: 'vec3<f32>' },
          uTop: { value: look.top, type: 'vec3<f32>' },
          uRockScale: { value: look.rockScale, type: 'f32' },
        },
      },
    });
    this.mesh = new Mesh({ geometry, shader });
  }

  /** Rebuild after a wall broke. */
  refresh(room: Room): void {
    const { data } = distanceField(room);
    const src = this.distTex.source as BufferImageSource;
    (src.resource as Uint8Array).set(data);
    src.update();
  }

  destroy(): void {
    this.mesh.destroy();
    this.distTex.destroy(true);
  }
}
