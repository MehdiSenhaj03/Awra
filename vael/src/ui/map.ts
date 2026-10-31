import { Container, Graphics, type Text } from 'pixi.js';
import { audio } from '../engine/audio';
import { input } from '../engine/input';
import { CELL_H, CELL_W, TILE } from '../engine/core';
import type { Scene } from '../game/scene';
import { t } from '../i18n';
import { ROOMS, bounds } from '../world/world';
import { DIM, GOLD, PALE, star, style, text } from './draw';
import type { Modal } from './modals';

const ZONE_COLOR: Record<string, number> = {
  couronne: 0x6a7cc8, jardins: 0x4ab0a0, forge: 0xc87040, archives: 0x3a8aa8, bastion: 0x8a80a8, firmament: 0xc070b0, penombre: 0x7a40a0, coeur: 0xe8d8a0,
};

/** The kingdom map: rooms seen (and every room of a zone whose map was bought), shrines, the hero. */
export class MapScreen implements Modal {
  c = new Container();
  private g = new Graphics();
  private ox = 0;
  private oy = 0;
  private zoom = 0.9;
  private labels = new Container();
  private legend: Text;

  constructor(private scene: Scene) {
    this.c.addChild(new Graphics().rect(0, 0, 1920, 1080).fill({ color: 0x03040a, alpha: 0.94 }), this.g, this.labels);
    this.legend = text('', style('italic', DIM, 26, 'center'), 960, 1040, 0.5, 0.5);
    this.c.addChild(this.legend, text(t('menu.map').toUpperCase(), style('title', GOLD, 40, 'center'), 960, 60, 0.5, 0.5));
    // Centre on the current room.
    const b = bounds(scene.room.def);
    this.ox = (b.x + b.w / 2) / CELL_W;
    this.oy = (b.y + b.h / 2) / CELL_H;
    audio.play('ui');
  }

  update(): boolean {
    const sp = 0.12 / this.zoom;
    if (input.held('left')) this.ox -= sp;
    if (input.held('right')) this.ox += sp;
    if (input.held('up')) this.oy -= sp;
    if (input.held('down')) this.oy += sp;
    if (input.pressed('jump')) this.zoom = this.zoom > 1 ? 0.6 : this.zoom + 0.4;
    this.draw();
    return input.pressed('map') || input.pressed('back') || input.pressed('pause');
  }

  private draw(): void {
    const g = this.g;
    g.clear();
    for (const ch of this.labels.removeChildren()) ch.destroy();
    const s = this.scene.save;
    const U = 120 * this.zoom;
    const sx = (cx: number) => 960 + (cx - this.ox) * U;
    const sy = (cy: number) => 540 + (cy - this.oy) * U * (CELL_H / CELL_W) * 1.6;
    const shown = ROOMS.filter((r) => s.seen.includes(r.id) || s.flags[`map_${r.zone}`]);
    const zones = new Set<string>();
    for (const r of shown) {
      const b = bounds(r);
      const x = sx(b.x / CELL_W);
      const y = sy(b.y / CELL_H);
      const w = (b.w / CELL_W) * U;
      const h = sy((b.y + b.h) / CELL_H) - y;
      const seen = s.seen.includes(r.id);
      const col = ZONE_COLOR[r.zone] ?? 0x8090c0;
      g.roundRect(x + 3, y + 3, w - 6, h - 6, 6).fill({ color: col, alpha: seen ? 0.28 : 0.1 }).stroke({ color: col, width: 2, alpha: seen ? 0.9 : 0.4 });
      // Openings.
      for (const [side, spans] of Object.entries(r.exits ?? {})) {
        for (const [a, bb] of spans as [number, number][]) {
          const mid = (a + bb) / 2;
          let px = 0;
          let py = 0;
          if (side === 'l' || side === 'r') {
            px = side === 'l' ? x + 3 : x + w - 3;
            py = y + (mid / (b.h)) * h;
          } else {
            px = x + (mid / b.w) * w;
            py = side === 'u' ? y + 3 : y + h - 3;
          }
          g.circle(px, py, 5).fill({ color: PALE, alpha: 0.8 });
        }
      }
      for (const e of r.ents ?? []) {
        // The Watch seal reveals what is still hidden.
        const id = (e[3] as { id?: string } | undefined)?.id;
        if (this.scene.seal('guet') && id && !s.flags[id] && ['ability', 'art', 'shard', 'vessel', 'ore', 'notch', 'chant', 'seal'].includes(e[0])) {
          const px = x + (e[1] / b.w) * w;
          const py = y + (e[2] / b.h) * h;
          g.moveTo(px, py - 8).lineTo(px + 6, py).lineTo(px, py + 8).lineTo(px - 6, py).closePath().fill({ color: 0xffe0a0, alpha: 0.9 });
        }
        if (e[0] === 'shrine') star(g, x + (e[1] / b.w) * w, y + (e[2] / b.h) * h, 10, 0xffe0a0);
        if (e[0] === 'npc' && seen) g.circle(x + (e[1] / b.w) * w, y + (e[2] / b.h) * h, 6).fill({ color: 0x90e0ff });
      }
      zones.add(r.zone);
    }
    // The hero.
    {
      const b = bounds(this.scene.room.def);
      const p = this.scene.player;
      const px = sx((b.x + p.cx / TILE) / CELL_W);
      const py = sy((b.y + p.cy / TILE) / CELL_H);
      const pulse = 1 + Math.sin(performance.now() / 200) * 0.2;
      star(g, px, py, 14 * pulse, 0xffffff);
    }
    for (const z of zones) {
      const rs = shown.filter((r) => r.zone === z);
      const minX = Math.min(...rs.map((r) => bounds(r).x));
      const minY = Math.min(...rs.map((r) => bounds(r).y));
      const tx = text(t(`zones.${z}`), style('menu', ZONE_COLOR[z] ?? PALE, 24), sx(minX / CELL_W), sy(minY / CELL_H) - 30);
      this.labels.addChild(tx);
    }
    this.legend.text = `${input.keyName('left')}${input.keyName('right')}${input.keyName('up')}${input.keyName('down')} · [${input.keyName('jump')}] zoom`;
  }
}
