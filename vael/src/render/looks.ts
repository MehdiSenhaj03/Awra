import type { Graphics } from 'pixi.js';
import type { Entity } from '../game/entity';
import { defineLook } from './entities';

const starPath = (g: Graphics, x: number, y: number, r: number, color: number, alpha = 1) => {
  const k = r * 0.3;
  g.moveTo(x, y - r).quadraticCurveTo(x + k, y - k, x + r, y).quadraticCurveTo(x + k, y + k, x, y + r).quadraticCurveTo(x - k, y + k, x - r, y).quadraticCurveTo(x - k, y - k, x, y - r).closePath().fill({ color, alpha });
};

/** Visual definitions of every entity sprite key. */
export function registerLooks(): void {
  defineLook('coquille', { sheet: 'en_coquille', image: 'en_test', height: 150, facesLeft: true, light: { color: 0x9a60ff, radius: 200, dy: -30 } });
  defineLook('acarien', { sheet: 'en_acarien', image: 'prop_crystals', height: 58, facesLeft: true, light: { color: 0x80a8ff, radius: 140 } });
  defineLook('lampion', { sheet: 'en_lampion', image: 'prop_crystals', height: 96, facesLeft: true, bob: 6, light: { color: 0xb070ff, radius: 320 } });
  defineLook('pretre', { sheet: 'en_pretre', height: 150, facesLeft: true, bob: 4, light: { color: 0xa070ff, radius: 220 } });
  defineLook('chevalier', { sheet: 'en_chevalier', image: 'en_test', height: 150, facesLeft: true, light: { color: 0x9a60ff, radius: 200 } });
  defineLook('ronce', { sheet: 'en_ronce', height: 130, facesLeft: true, light: { color: 0x70ffd8, radius: 200 } });
  defineLook('spore', { sheet: 'en_spore', height: 110, facesLeft: true, bob: 8, light: { color: 0xff9ad8, radius: 260 } });
  defineLook('mante', { sheet: 'en_mante', height: 170, facesLeft: true, light: { color: 0x70ffd8, radius: 200 } });
  defineLook('souffle', { sheet: 'en_forgeron_mineur', height: 120, facesLeft: true, light: { color: 0xff9040, radius: 240 } });
  defineLook('enclume', { sheet: 'en_enclume', height: 210, facesLeft: true, light: { color: 0xff9040, radius: 260 } });
  defineLook('etincelle', { sheet: 'en_etincelle', height: 80, facesLeft: true, light: { color: 0xffa050, radius: 260 } });
  defineLook('lecteur', { sheet: 'en_noye', height: 150, facesLeft: true, bob: 4, light: { color: 0x70e0ff, radius: 220 } });
  defineLook('anguille', { sheet: 'en_anguille', height: 70, facesLeft: true, light: { color: 0x70e0ff, radius: 200 } });
  defineLook('geolier', { sheet: 'en_geolier', height: 190, facesLeft: true });
  defineLook('corbeau', { sheet: 'en_corbeau', height: 100, facesLeft: true, light: { color: 0xc8d0ff, radius: 160 } });
  defineLook('astrelet', { sheet: 'en_astre', height: 90, facesLeft: true, light: { color: 0xff9ad8, radius: 300 } });
  defineLook('ombre', { sheet: 'en_ombre', height: 140, facesLeft: true, light: { color: 0xff3060, radius: 200 } });
  defineLook('sentinelle', { sheet: 'en_sentinelle', height: 200, facesLeft: true, light: { color: 0xfff0c8, radius: 300 } });
  defineLook('cloud', {
    height: 1, noBody: true, customAdd: true,
    custom: (g, e, now) => {
      const c = e as unknown as { life: number; r: number; color: number };
      const k = Math.min(1, c.life / 20) * Math.min(1, (90 - c.life) / 10);
      for (let i = 0; i < 6; i++) g.circle(e.cx + Math.cos(now * 2 + i) * c.r * 0.4, e.cy + Math.sin(now * 1.7 + i * 2) * c.r * 0.3, c.r * 0.6).fill({ color: c.color, alpha: 0.12 * k });
    },
  });
  defineLook('glyph', {
    height: 1, noBody: true, customAdd: true,
    custom: (g, e, now) => {
      const gl = e as unknown as { life: number; color: number };
      const k = 1 - gl.life / 46;
      g.ellipse(e.cx, e.body.y + e.body.h, 75, 18).stroke({ color: gl.color, width: 3, alpha: 0.4 + k * 0.6 });
      g.ellipse(e.cx, e.body.y + e.body.h, 75 * k, 18 * k).fill({ color: gl.color, alpha: 0.25 });
      g.moveTo(e.cx, e.body.y + e.body.h).lineTo(e.cx, e.body.y + e.body.h - 160 * k).stroke({ color: gl.color, width: 2, alpha: 0.5 * Math.sin(now * 30) });
    },
  });
  defineLook('orb', {
    height: 1, noBody: true, customAdd: true, light: { color: 0xb070ff, radius: 220 },
    custom: (g, e, now) => {
      g.circle(e.cx, e.cy, 26 + Math.sin(now * 12) * 3).fill({ color: 0x8a40ff, alpha: 0.35 });
      g.circle(e.cx, e.cy, 15).fill({ color: 0xd8b0ff, alpha: 0.95 });
      g.circle(e.cx, e.cy, 7).fill({ color: 0xffffff, alpha: 1 });
    },
  });
  const pickup = (big: boolean) => (g: Graphics, e: Entity, now: number) => {
    const y = e.cy + Math.sin(now * 2.2) * 8;
    const r = big ? 46 : 30;
    g.circle(e.cx, y, r * 1.6).fill({ color: 0xffd890, alpha: 0.12 + Math.sin(now * 3) * 0.05 });
    g.circle(e.cx, y, r * 0.8).fill({ color: 0xfff0c8, alpha: 0.35 });
    starPath(g, e.cx, y, r, 0xffffff, 0.95);
    starPath(g, e.cx, y, r * 0.55, 0xffe6a8, 1);
    for (let i = 0; i < 4; i++) {
      const a = now * 1.5 + (i * Math.PI) / 2;
      g.circle(e.cx + Math.cos(a) * r * 1.3, y + Math.sin(a) * r * 0.5, 3).fill({ color: 0xfff0c8, alpha: 0.8 });
    }
  };
  defineLook('pickup', { height: 1, noBody: true, customAdd: true, custom: pickup(false), light: { color: 0xffe0a0, radius: 280 } });
  defineLook('pickupBig', { height: 1, noBody: true, customAdd: true, custom: pickup(true), light: { color: 0xffe0a0, radius: 460 } });
  defineLook('tablet', { image: 'tablet', height: 190, light: { color: 0xffe0a0, radius: 200, dy: -60 } });
  for (const who of ['orsa', 'vey', 'lunelle', 'veilleur', 'pip', 'murmure']) {
    defineLook(`npc_${who}`, { image: `npc_${who}`, height: who === 'pip' ? 70 : who === 'vey' ? 170 : 156, facesLeft: true, bob: who === 'pip' ? 6 : 0, light: who === 'pip' ? { color: 0xffe6a0, radius: 300 } : undefined });
  }
  defineLook('eye', {
    height: 1, noBody: true, light: { color: 0xffe0a0, radius: 260 },
    custom: (g, e, now) => {
      const open = (e as unknown as { open: boolean }).open;
      g.circle(e.cx, e.cy, 34).fill({ color: 0x14182c }).stroke({ color: 0xc8b48a, width: 4 });
      g.ellipse(e.cx, e.cy, 24, open ? 4 : 16).fill({ color: open ? 0x6a5a40 : 0xfff0c8 });
      if (!open) g.circle(e.cx, e.cy, 8 + Math.sin(now * 4) * 1.5).fill({ color: 0x1a1020 });
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2;
        g.moveTo(e.cx + Math.cos(a) * 38, e.cy + Math.sin(a) * 38).lineTo(e.cx + Math.cos(a) * 50, e.cy + Math.sin(a) * 50).stroke({ color: 0xc8b48a, width: 3, alpha: 0.8 });
      }
    },
  });
  defineLook('ring', {
    height: 1, noBody: true, customAdd: true, light: { color: 0x9fd0ff, radius: 260 },
    custom: (g, e, now) => {
      g.circle(e.cx, e.cy, 40).stroke({ color: 0x9fd0ff, width: 6, alpha: 0.9 });
      g.circle(e.cx, e.cy, 52 + Math.sin(now * 3) * 4).stroke({ color: 0x9fd0ff, width: 2, alpha: 0.35 });
      starPath(g, e.cx, e.cy - 40, 10, 0xffffff);
    },
  });
  defineLook('crack', {
    height: 1, noBody: true,
    custom: (g, e) => {
      const b = e.body;
      const k = 1 - e.hp / e.maxHp;
      for (let i = 0; i < 5 + k * 10; i++) {
        const x = b.x + ((i * 37) % Math.max(1, b.w));
        const y = b.y + ((i * 53) % Math.max(1, b.h));
        g.moveTo(x, y).lineTo(x + 18, y + 26).lineTo(x + 6, y + 44).stroke({ color: 0x9aa8d8, width: 2, alpha: 0.5 + k * 0.4 });
      }
    },
  });
  defineLook('wave', {
    height: 1, noBody: true, customAdd: true, light: { color: 0xffd890, radius: 200 },
    custom: (g, e, now) => {
      const b = e.body;
      for (let i = 0; i < 3; i++) g.ellipse(b.x + b.w / 2 - e.facing * i * 18, b.y + b.h - 10, 26 - i * 6, b.h * (0.9 - i * 0.2) / 2).fill({ color: 0xffd890, alpha: 0.5 - i * 0.12 + Math.sin(now * 20) * 0.05 });
    },
  });
  defineLook('debris', {
    image: 'prop_crystals', height: 70, light: { color: 0xa8c0ff, radius: 160 },
    custom: (g, e) => {
      const d = e as unknown as { warn: number };
      if (d.warn > 0) g.rect(e.cx - 20, 0, 40, 3000).fill({ color: 0xa8c0ff, alpha: 0.05 + (1 - d.warn / 50) * 0.12 });
    },
  });
  const cometLook = (r: number) => (g: Graphics, e: Entity) => {
    g.circle(e.cx, e.cy, r * 2).fill({ color: 0xffd890, alpha: 0.25 });
    g.ellipse(e.cx - e.facing * r, e.cy, r * 2.6, r * 0.7).fill({ color: 0xffe6b0, alpha: 0.5 });
    starPath(g, e.cx, e.cy, r * 1.3, 0xffffff);
  };
  defineLook('comet', { height: 1, noBody: true, customAdd: true, custom: cometLook(22), light: { color: 0xffe0a0, radius: 360 } });
  defineLook('cometSmall', { height: 1, noBody: true, customAdd: true, custom: cometLook(13), light: { color: 0xffe0a0, radius: 200 } });
  defineLook('ascension', { height: 1, noBody: true });
  defineLook('starfall', { height: 1, noBody: true });
  defineLook('constellation', {
    height: 1, noBody: true, customAdd: true,
    custom: (g, e) => {
      const c = e as unknown as { targets: Entity[] };
      g.rect(e.cx - 2000, e.cy - 1200, 4000, 2400).fill({ color: 0x0a0c30, alpha: 0.35 });
      const pts = c.targets.filter((x) => !x.dead);
      for (let i = 0; i < pts.length; i++) {
        starPath(g, pts[i].cx, pts[i].cy, 22, 0xfff0d0);
        if (i > 0) g.moveTo(pts[i - 1].cx, pts[i - 1].cy).lineTo(pts[i].cx, pts[i].cy).stroke({ color: 0xfff0d0, width: 2, alpha: 0.7 });
      }
    },
  });
  defineLook('shrine', { image: 'shrine', height: 170, sink: 0.02, light: { color: 0xffe0a0, radius: 520, dy: -40 } });
  defineLook('fallen', { image: 'fallen_star', height: 90, light: { color: 0xffe0a0, radius: 300 } });
  defineLook('gardien', { sheet: 'boss_gardien_a', height: 380, facesLeft: true, light: { color: 0xffd890, radius: 420, dy: -60 } });
  defineLook('racine', { sheet: 'boss_racine_a', height: 440, facesLeft: true, light: { color: 0x70ffd8, radius: 520, dy: -80 } });
  defineLook('forgeron', { sheet: 'boss_forgeron_a', height: 400, facesLeft: true, light: { color: 0xff9040, radius: 480, dy: -60 } });
  defineLook('bibliothecaire', { sheet: 'boss_bibliothecaire_a', height: 400, facesLeft: true, light: { color: 0x70e0ff, radius: 480 } });
  defineLook('geoliers', { sheet: 'boss_geoliers_a', height: 360, facesLeft: true, light: { color: 0xc8c0e0, radius: 380 } });
  defineLook('geolierTwin', { sheet: 'boss_geoliers_a', height: 360, facesLeft: true, tint: 0xb8c8ff });
  defineLook('oiseau', { sheet: 'boss_oiseau_a', height: 320, facesLeft: true, sink: -0.3, light: { color: 0xff9ad8, radius: 600 } });
  defineLook('reine', { sheet: 'boss_reine_a', height: 440, facesLeft: true, light: { color: 0xb070ff, radius: 500 } });
  defineLook('enchaineur', { sheet: 'boss_enchaineur_a', height: 380, facesLeft: true, light: { color: 0xfff0c8, radius: 460, dy: -60 } });
  defineLook('etoile', { sheet: 'boss_etoile', height: 480, light: { color: 0xfff4dc, radius: 900 } });
  defineLook('veilleurBoss', { sheet: 'en_veilleur', height: 180, facesLeft: true });
  defineLook('heart', {
    height: 1, noBody: true, customAdd: true, light: { color: 0xfff4dc, radius: 900, dy: -120 },
    custom: (g, e, now) => {
      const y = e.body.y - 120 + Math.sin(now * 1.3) * 10;
      const pulse = 1 + Math.sin(now * 2.4) * 0.08;
      g.circle(e.cx, y, 170 * pulse).fill({ color: 0xffe6a8, alpha: 0.12 });
      g.circle(e.cx, y, 110 * pulse).fill({ color: 0xfff0c8, alpha: 0.25 });
      starPath(g, e.cx, y, 90 * pulse, 0xffffff, 0.95);
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2 + now * 0.2;
        g.moveTo(e.cx + Math.cos(a) * 140, y + Math.sin(a) * 140).lineTo(e.cx + Math.cos(a) * 900, y + Math.sin(a) * 600).stroke({ color: 0xfff0c8, width: 10, alpha: 0.35 });
      }
    },
  });
  defineLook('spike', {
    height: 1, noBody: true, customAdd: true,
    custom: (g, e) => {
      const s = e as unknown as { warn: number; life: number; color: number; kind: string };
      const b = e.body;
      const foot = b.y + b.h;
      if (s.warn > 0) {
        g.ellipse(e.cx, foot, 40, 10).fill({ color: s.color, alpha: 0.25 + (s.warn % 8 < 4 ? 0.2 : 0) });
        g.rect(e.cx - 2, foot - 40, 4, 40).fill({ color: s.color, alpha: 0.5 });
        return;
      }
      const k = Math.min(1, (26 - s.life) / 5) * Math.min(1, s.life / 6);
      const h = b.h * k;
      g.moveTo(e.cx - 34, foot).lineTo(e.cx - 10, foot - h).lineTo(e.cx + 10, foot - h).lineTo(e.cx + 34, foot).closePath().fill({ color: s.color, alpha: 0.75 });
      g.moveTo(e.cx - 10, foot).lineTo(e.cx, foot - h * 1.04).lineTo(e.cx + 10, foot).closePath().fill({ color: 0xffffff, alpha: 0.6 });
    },
  });
}
