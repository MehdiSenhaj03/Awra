import { Container, Graphics, NineSliceSprite, Text, TextStyle } from 'pixi.js';
import { hasTex, tex } from '../render/assets';

export const GOLD = 0xf2d9a0;
export const PALE = 0xe8ecff;
export const DIM = 0x8a90b0;
export const INK = 0x05060c;

const styles = new Map<string, TextStyle>();

/** Text styles: display (Cinzel) and body (Cormorant Garamond). */
export function style(kind: 'display' | 'title' | 'body' | 'small' | 'italic' | 'menu', color = PALE, size?: number, align: 'left' | 'center' | 'right' = 'left', wrap = 0): TextStyle {
  const key = `${kind}|${color}|${size}|${align}|${wrap}`;
  let s = styles.get(key);
  if (s) return s;
  const base = {
    display: { fontFamily: 'Cinzel', fontSize: 120, fontWeight: '700', letterSpacing: 24 },
    title: { fontFamily: 'Cinzel', fontSize: 54, fontWeight: '400', letterSpacing: 8 },
    menu: { fontFamily: 'Cinzel', fontSize: 34, fontWeight: '400', letterSpacing: 4 },
    body: { fontFamily: 'Cormorant Garamond', fontSize: 34, fontWeight: '400' },
    italic: { fontFamily: 'Cormorant Garamond', fontSize: 34, fontStyle: 'italic', fontWeight: '500' },
    small: { fontFamily: 'Cormorant Garamond', fontSize: 26, fontWeight: '400' },
  }[kind] as Record<string, unknown>;
  s = new TextStyle({
    ...base,
    fontSize: size ?? (base.fontSize as number),
    fill: color,
    align,
    wordWrap: wrap > 0,
    wordWrapWidth: wrap || 100,
    lineHeight: Math.round((size ?? (base.fontSize as number)) * 1.3),
    dropShadow: { color: 0x000000, alpha: 0.7, blur: 6, distance: 0, angle: 0 },
  });
  styles.set(key, s);
  return s;
}

export function text(str: string, st: TextStyle, x = 0, y = 0, ax = 0, ay = 0): Text {
  const t = new Text({ text: str, style: st });
  t.anchor.set(ax, ay);
  t.position.set(x, y);
  return t;
}

/** A dark translucent panel with a thin gold rim and corner ornaments. */
export function panel(w: number, h: number, alpha = 0.82): Container {
  const c = new Container();
  const g = new Graphics();
  c.addChild(g);
  g.roundRect(0, 0, w, h, 6).fill({ color: 0x070914, alpha });
  if (hasTex('ui_frame')) {
    // The painted frame: its gold line sits 30 px inside the texture; corners keep their size (nine-slice).
    const k = Math.min(1, (Math.min(w, h) + 40) / 260);
    const f = new NineSliceSprite({ texture: tex('ui_frame'), leftWidth: 100, rightWidth: 100, topHeight: 100, bottomHeight: 100 });
    f.scale.set(k);
    f.width = (w + 60 * k) / k;
    f.height = (h + 54 * k) / k;
    f.position.set(-30 * k, -27 * k);
    c.addChild(f);
    return c;
  }
  g.roundRect(0, 0, w, h, 6).stroke({ color: GOLD, width: 1.5, alpha: 0.55 });
  g.roundRect(8, 8, w - 16, h - 16, 4).stroke({ color: GOLD, width: 1, alpha: 0.18 });
  for (const [x, y] of [[0, 0], [w, 0], [0, h], [w, h]]) {
    g.moveTo(x - 10, y).lineTo(x, y - 10).lineTo(x + 10, y).lineTo(x, y + 10).closePath().fill({ color: GOLD, alpha: 0.75 });
  }
  return c;
}

/** A thin ornamental divider. */
export function divider(w: number, color = GOLD, alpha = 0.6): Graphics {
  const g = new Graphics();
  g.moveTo(-w / 2, 0).lineTo(-14, 0).stroke({ color, width: 1.2, alpha });
  g.moveTo(14, 0).lineTo(w / 2, 0).stroke({ color, width: 1.2, alpha });
  g.moveTo(0, -7).lineTo(7, 0).lineTo(0, 7).lineTo(-7, 0).closePath().fill({ color, alpha });
  return g;
}

/** A four-pointed star (UI cursor, icons). */
export function star(g: Graphics, x: number, y: number, r: number, color: number, alpha = 1): Graphics {
  const k = r * 0.28;
  g.moveTo(x, y - r).quadraticCurveTo(x + k, y - k, x + r, y).quadraticCurveTo(x + k, y + k, x, y + r).quadraticCurveTo(x - k, y + k, x - r, y).quadraticCurveTo(x - k, y - k, x, y - r).closePath().fill({ color, alpha });
  return g;
}

export function clear(c: Container): void {
  for (const ch of c.removeChildren()) ch.destroy({ children: true });
}
