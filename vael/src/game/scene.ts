import { type Box, Rng, TILE, clamp, overlap } from '../engine/core';
import { type Ledge, fits, move } from '../engine/physics';
import { type Room, type RoomDef, T, buildRoom, setTile, tileAt } from '../world/room';
import { Comet, castArt } from './arts';
import { Blade, segDist } from './blade';
import type { Controls } from './controls';
import { type DamageSource, Entity } from './entity';
import type { FxEvent } from './events';
import { type AttackDir, PHYS, Player, type PlayerWorld } from './player';
import type { Save } from './save';
import { Pickup, type PickupKind } from './props';
import { makeBoss } from './bosses';
import type { Boss } from './boss';
import { t as tr } from '../i18n';

export interface SceneHooks {
  roomDef(id: string): RoomDef | undefined;
  neighbour(room: RoomDef, side: 'l' | 'r' | 'u' | 'd', px: number, py: number): { def: RoomDef; x: number; y: number } | null;
  spawn(scene: Scene, kind: string, x: number, y: number, opts: Record<string, unknown>, uid: string): Entity | null;
}

/** What each keeper leaves behind. */
export const BOSS_REWARD: Record<string, [PickupKind, string]> = {
  gardien: ['ability', 'dash'],
  racine: ['ability', 'walljump'],
  forgeron: ['ability', 'blink'],
  bibliothecaire: ['ability', 'double'],
  geoliers: ['ability', 'ring'],
  oiseau: ['ability', 'comet'],
  reine: ['art', 'chute2'],
  veilleur: ['notch', 'veilleur'],
};

/** Base damage of one slash at forge level 0. Foes have HP in the same unit. */
export const BASE_DAMAGE = 10;
export const LUMEN_PER_HIT = 8;
export const HEAL_COST = 33;
export const ART_COST = 33;

/**
 * The running world: one room, the hero, the blade and the things living there. Pure simulation, no drawing,
 * so it runs headless in tests. Everything visible is reported through `events`.
 */
export class Scene implements PlayerWorld {
  room!: Room;
  player: Player;
  blade = new Blade();
  entities: Entity[] = [];
  events: FxEvent[] = [];
  rng = new Rng(7);
  /** Global freeze frames (hit-stop). */
  freeze = 0;
  /** Éclipse: foes run at a fraction of speed. */
  eclipse = 0;
  private enemyClock = 0;
  lumen = 0;
  /** Slain foes stay dead until the next rest. */
  killed = new Set<string>();
  /** Pending room change, handled by the game screen (fade). */
  pending: { def: RoomDef; x: number; y: number; vx: number; vy: number } | null = null;
  /** Set while respawning from a hazard. */
  respawnT = 0;
  god = false;
  headless = false;
  t = 0;
  private hitBy = new Map<number, number>();
  /** Fixed focus for the camera (boss arenas). */
  cameraFocus: { x: number; y: number } | null = null;
  boss: Entity | null = null;
  toast: { text: string; t: number } | null = null;
  /** Tutorial hint key shown this frame. */
  hint = '';

  constructor(public save: Save, public hooks: SceneHooks) {
    this.player = new Player(0, 0);
    this.player.maxHp = save.maxHp;
    this.player.hp = save.maxHp;
  }

  get flags(): Record<string, boolean | number> {
    return this.save.flags;
  }

  setFlag(k: string, v: boolean | number = true): void {
    this.save.flags[k] = v;
  }

  has(a: string): boolean {
    return this.save.abilities.includes(a);
  }

  maxLumen(): number {
    return 99 + this.save.vessels * 33;
  }

  /** A seal is worn. */
  seal(name: string): boolean {
    return this.save.sealsOn.includes(name);
  }

  damageMul(): number {
    let m = 1 + this.save.forge * 0.25;
    if (this.seal('fragile')) m *= 2;
    if (this.seal('lueur') && this.player.hp <= 1) m *= 1.5;
    return m;
  }

  emit(e: FxEvent): void {
    if (!this.headless) this.events.push(e);
  }

  sfx(name: string, x?: number, y?: number, vol?: number): void {
    this.emit({ t: 'sfx', name, x, y, vol });
  }

  // ---- Rooms -------------------------------------------------------------------------------------------

  enter(def: RoomDef | string, x?: number, y?: number): void {
    const d = typeof def === 'string' ? this.hooks.roomDef(def)! : def;
    this.room = buildRoom(d, this.save.flags);
    this.entities = [];
    this.boss = null;
    this.cameraFocus = null;
    this.blade.catchNow();
    if (!this.save.seen.includes(d.id)) this.save.seen.push(d.id);
    for (const [i, e] of (d.ents ?? []).entries()) {
      const uid = `${d.id}:${i}`;
      if (this.killed.has(uid)) continue;
      const ent = this.hooks.spawn(this, e[0], e[1] * TILE + TILE / 2, (e[2] + 1) * TILE, e[3] ?? {}, uid);
      if (ent) {
        ent.uid = uid;
        this.entities.push(ent);
      }
    }
    const p = this.player;
    if (x !== undefined && y !== undefined) {
      p.body.x = x;
      p.body.y = y;
    }
    p.safe = { x: p.body.x, y: p.body.y };
    this.pending = null;
  }

  add(e: Entity): void {
    this.entities.push(e);
  }

  // ---- PlayerWorld ---------------------------------------------------------------------------------------

  get ledges(): Ledge[] {
    const l = this.blade.ledge();
    return l ? [l] : [];
  }

  bladeHeld(): boolean {
    return this.blade.held();
  }

  bladeOut(): boolean {
    return this.blade.out();
  }

  throwBlade(dx: number, dy: number): void {
    const p = this.player;
    Blade.RANGE = this.seal('longfil') ? 960 : 640;
    // Aim assist: with the Tether, a ring close to the chosen direction pulls the throw onto it.
    if (this.has('ring')) {
      const a0 = Math.atan2(dy, dx);
      let best: { d: number; a: number } | null = null;
      for (const e of this.entities) {
        if (e.sprite !== 'ring') continue;
        const ex = e.cx - p.cx;
        const ey = e.cy - (p.cy - 8);
        const d = Math.hypot(ex, ey);
        if (d < 70 || d > Blade.RANGE - 20) continue;
        const a = Math.atan2(ey, ex);
        let diff = Math.abs(a - a0);
        if (diff > Math.PI) diff = Math.PI * 2 - diff;
        if (diff < 0.5 && (!best || d < best.d)) best = { d, a };
      }
      if (best) {
        dx = Math.cos(best.a);
        dy = Math.sin(best.a);
      }
    }
    this.blade.throw(p.cx + dx * 20, p.cy - 8 + dy * 10, dx, dy);
    if (this.seal('jumelle')) this.add(new Comet(this, p.cx + dx * 30, p.cy - 30, dx >= 0 ? 1 : -1, false, true, dy * 20));
    this.emit({ t: 'throw', x: p.cx, y: p.cy, dir: p.facing });
  }

  recallBlade(): void {
    const b = this.blade;
    // Ripping the blade out of a branded foe hurts it badly.
    if (b.state === 'branded' && b.target) {
      const e = b.target as unknown as Entity;
      this.strike(e, BASE_DAMAGE * 2.2 * this.damageMul(), b.x, 'rip');
    }
    b.recall();
    this.emit({ t: 'recall', x: b.x, y: b.y });
  }

  dashMul(): number {
    return this.has('comet') ? 1.4 : 1;
  }

  blinkTarget(): { x: number; y: number; execute: boolean; zip?: boolean } | null {
    const b = this.blade;
    // Only a planted blade can be joined.
    if (b.state !== 'stuck' && b.state !== 'branded') return null;
    if (b.ring) return { x: b.ring.x, y: b.ring.y + 40, execute: false, zip: true };
    if (b.state === 'branded' && b.target) {
      const e = b.target as unknown as Entity;
      // Land just beside the foe, on the hero's side.
      const side = Math.sign(this.player.cx - e.cx) || -1;
      return { x: e.cx + side * (e.body.w / 2 + 30), y: e.cy, execute: true };
    }
    // From a wall, stand just out of it; from a floor, just above; from a ceiling, just below.
    if (b.floorSide > 0) return { x: b.x, y: b.y - 60, execute: false };
    if (b.floorSide < 0) return { x: b.x, y: b.y + 60, execute: false };
    return { x: b.x - b.wallSide * 26, y: b.y + 20, execute: false };
  }

  afterBlink(execute: boolean): void {
    const b = this.blade;
    const p = this.player;
    const from = { x: p.trail[p.trail.length - 1]?.x ?? p.cx, y: (p.trail[p.trail.length - 1]?.y ?? p.feet) - 42 };
    if (execute && b.target) {
      const e = b.target as unknown as Entity;
      this.strike(e, BASE_DAMAGE * 3.5 * this.damageMul(), p.cx, 'execute');
      this.freeze = 10;
      this.gainLumen(20);
      this.emit({ t: 'shockwave', x: e.cx, y: e.cy, r: 260, color: 0xffe6a8 });
    }
    b.catchNow();
    this.emit({ t: 'blink', x0: from.x, y0: from.y, x1: p.cx, y1: p.cy, big: execute });
  }

  lumenValue(): number {
    return this.lumen;
  }

  spendLumen(n: number): boolean {
    if (this.lumen < n) return false;
    this.lumen -= n;
    return true;
  }

  gainLumen(n: number): void {
    this.lumen = clamp(this.lumen + n, 0, this.maxLumen());
  }

  canHeal(): boolean {
    return this.lumen >= HEAL_COST && this.player.hp < this.player.maxHp;
  }

  heal(): void {
    if (!this.spendLumen(HEAL_COST)) return;
    this.player.hp = Math.min(this.player.maxHp, this.player.hp + 1);
    this.emit({ t: 'heal', x: this.player.cx, y: this.player.cy });
  }

  castArt(dir: AttackDir | 'ultimate'): boolean {
    if (dir === 'ultimate') {
      if (!this.save.arts.includes('constellation') || this.lumen < this.maxLumen()) return false;
      this.lumen = 0;
      castArt(this, 'constellation');
      return true;
    }
    const name = dir === 'up' ? 'ascension' : dir === 'down' ? 'chute' : 'comete';
    if (!this.save.arts.includes(name)) return false;
    if (!this.spendLumen(ART_COST)) return false;
    castArt(this, name);
    return true;
  }

  /** Frames during which foes are frozen (Constellation). */
  timeStop = 0;

  // ---- Combat ---------------------------------------------------------------------------------------------

  /** Deals damage with all the feedback. Returns true when it landed. */
  strike(e: Entity, amount: number, fromX: number, source: DamageSource): boolean {
    if (this.seal('marque') && this.blade.state === 'branded' && this.blade.target === (e as unknown as typeof this.blade.target)) amount *= 1.3;
    if (!e.damage(amount, fromX, source)) return false;
    const p = this.player;
    const big = source === 'rip' || source === 'execute' || source === 'art' || e.dead;
    const dx = Math.sign(e.cx - fromX) || p.facing;
    this.emit({ t: 'hit', x: e.cx, y: e.cy, dx, dy: 0, big, kind: source });
    this.freeze = Math.max(this.freeze, e.dead ? 7 : big ? 6 : 3);
    const harvest = this.seal('moisson') ? 1.5 : 1;
    if (source === 'slash' || source === 'palm') this.gainLumen(LUMEN_PER_HIT * harvest);
    else if (source === 'blade') this.gainLumen(4 * harvest);
    return true;
  }

  killFx(e: Entity): void {
    this.emit({ t: 'kill', x: e.cx, y: e.cy, size: Math.max(e.body.w, e.body.h), color: 0x8a5cff, sprite: e.sprite, id: e.id });
    if (e.uid) this.killed.add(e.uid);
    if (e.dust > 0) {
      const n = Math.round(e.dust * (this.seal('avare') ? 1.5 : 1));
      this.save.dust += n;
      this.emit({ t: 'dust', x: e.cx, y: e.cy, n });
    }
  }

  hurtPlayer(amount: number, fromX: number, dodgeable = true): void {
    const p = this.player;
    if (p.dead || this.respawnT > 0 || this.god) return;
    // Perfect dodge: a dash that meets the blow in its first frames slips through it.
    if (dodgeable && p.dashT > PHYS.dashFrames - 8 && p.dashT > 0) {
      this.triggerEclipse();
      return;
    }
    if (p.invuln > 0) return;
    let dmg = this.save.hard ? amount * 2 : amount;
    if (this.seal('fragile')) dmg *= 2;
    p.hp -= dmg;
    p.hurt(fromX);
    if (this.seal('epines')) {
      for (const e of this.entities) {
        if (e.dead || !e.hurtable || e.prop) continue;
        if (Math.hypot(e.cx - p.cx, e.cy - p.cy) < 260) this.strike(e, BASE_DAMAGE * 1.5, p.cx, 'wave');
      }
      this.emit({ t: 'boom', x: p.cx, y: p.cy, r: 200, color: 0xa8c0ff });
    }
    this.freeze = 10;
    this.blade.state === 'held' || this.blade.recall();
    this.emit({ t: 'hurt', x: p.cx, y: p.cy });
    if (p.hp <= 0) this.die();
  }

  triggerEclipse(): void {
    if (this.eclipse > 0) return;
    this.eclipse = this.seal('eclipse') ? 180 : 90;
    this.player.invuln = Math.max(this.player.invuln, 40);
    this.gainLumen(25);
    this.emit({ t: 'dodge', x: this.player.cx, y: this.player.cy });
  }

  die(): void {
    const p = this.player;
    p.dead = true;
    p.hp = 0;
    this.save.deaths++;
  }

  private hazard(): void {
    const p = this.player;
    if (this.respawnT > 0 || p.dead) return;
    this.emit({ t: 'hurt', x: p.cx, y: p.cy });
    if (!this.god) p.hp -= this.save.hard ? 2 : 1;
    if (p.hp <= 0) {
      this.die();
      return;
    }
    this.respawnT = 40;
    p.invuln = 60;
    p.attack = null;
    p.dashT = 0;
    this.blade.catchNow();
  }

  // ---- Frame ----------------------------------------------------------------------------------------------

  update(c: Controls): void {
    this.t++;
    this.hint = '';
    if (this.toast && --this.toast.t <= 0) this.toast = null;
    if (this.freeze > 0) {
      this.freeze--;
      return;
    }
    const p = this.player;
    if (this.respawnT > 0) {
      this.respawnT--;
      if (this.respawnT === 20) {
        p.body.x = p.safe.x;
        p.body.y = p.safe.y;
        p.body.vx = 0;
        p.body.vy = 0;
      }
      return;
    }
    if (this.eclipse > 0) this.eclipse--;

    const evs = p.update(c, this);
    for (const e of evs) {
      if (e.kind === 'attack') {
        const a = e.attack;
        this.emit({ t: 'slash', x: p.cx, y: p.cy, dir: a.dir, facing: e.facing, combo: a.combo, palm: a.palm });
      } else if (e.kind === 'jump' || e.kind === 'doubleJump' || e.kind === 'land' || e.kind === 'footstep' || e.kind === 'wallJump') {
        this.emit({ t: e.kind, x: e.x ?? p.cx, y: e.y ?? p.feet, dir: e.dir, big: e.big });
      } else if (e.kind === 'dash') {
        this.emit({ t: 'dash', x: p.cx, y: p.cy, dir: e.dir ?? p.facing });
      } else if (e.kind === 'dashWall') {
        this.dashIntoWall(e.x ?? p.cx, e.y ?? p.cy);
      } else if (e.kind === 'zip') {
        this.emit({ t: 'dash', x: p.cx, y: p.cy, dir: p.facing });
      } else if (e.kind === 'zipEnd') {
        this.blade.catchNow();
        this.emit({ t: 'catch', x: p.cx, y: p.cy });
      } else if (e.kind === 'healStart') {
        this.emit({ t: 'healStart', x: p.cx, y: p.cy });
      } else if (e.kind === 'art') {
        this.emit({ t: 'art', x: p.cx, y: p.cy, art: e.art ?? 'side', dir: p.facing });
      }
    }

    this.updateBlade();
    this.playerAttacks();

    // Foes (slowed during the Éclipse, frozen by Constellation). Art effects always run.
    if (this.timeStop > 0) {
      this.timeStop--;
      for (const e of this.entities) if (!e.dead && e.prop && !e.hurtable) { e.tick(); e.update(); }
      this.entities = this.entities.filter((e) => !e.dead || (e as unknown as { keep?: boolean }).keep);
      if (!p.dead && this.touchesHazard()) this.hazard();
      return;
    }
    this.enemyClock += this.eclipse > 0 ? 0.3 : 1;
    while (this.enemyClock >= 1) {
      this.enemyClock -= 1;
      for (const e of this.entities) {
        if (e.dead) continue;
        e.tick();
        e.update();
      }
      this.enemyHits();
    }
    this.entities = this.entities.filter((e) => !e.dead || (e as unknown as { keep?: boolean }).keep);

    // Hazards.
    if (!p.dead && this.touchesHazard()) this.hazard();
    this.checkExit();
  }

  private touchesHazard(): boolean {
    const b = this.player.body;
    const x0 = Math.floor((b.x + 6) / TILE);
    const x1 = Math.floor((b.x + b.w - 6) / TILE);
    const y0 = Math.floor((b.y + 8) / TILE);
    const y1 = Math.floor((b.y + b.h - 2) / TILE);
    for (let ty = y0; ty <= y1; ty++) for (let tx = x0; tx <= x1; tx++) {
      const t = tileAt(this.room, tx, ty);
      if (t === T.Spike || t === T.Gloam) return true;
    }
    return false;
  }

  private updateBlade(): void {
    const b = this.blade;
    const p = this.player;
    if (b.state === 'held') return;
    const r = b.update(this.room, p.cx, p.cy - 8);
    if (r === 'caught') this.emit({ t: 'catch', x: p.cx, y: p.cy });
    if (r === 'bounced') this.emit({ t: 'clank', x: b.x, y: b.y });
    if (r === 'lodged') {
      this.emit({ t: 'lodge', x: b.x, y: b.y, angle: b.angle });
      // Lodged under the hero's feet would trap them inside the step; nothing to do, the step is one-way.
    }
    if (b.state === 'flying' && this.has('ring') && b.travelled > 60) {
      for (const e of this.entities) {
        if (e.sprite !== 'ring') continue;
        if (Math.hypot(e.cx - b.x, e.cy - b.y) < 46) {
          b.lodgeInRing(e.cx, e.cy);
          this.emit({ t: 'lodge', x: e.cx, y: e.cy, angle: b.angle });
          return;
        }
      }
    }
    if (b.state === 'flying' || b.state === 'returning') {
      const s = b.sweep();
      for (const e of this.entities) {
        if (e.dead || !e.hurtable || b.pierced.has(e.id)) continue;
        const reach = Math.max(e.body.w, e.body.h) / 2 + 16;
        if (segDist(e.cx, e.cy, s.x0, s.y0, s.x1, s.y1) > reach) continue;
        b.pierced.add(e.id);
        const dmg = BASE_DAMAGE * (b.state === 'returning' ? 1.2 * (this.seal('retour') ? 2 : 1) : 0.8) * this.damageMul();
        const landed = this.strike(e, dmg, b.x - b.vx, 'blade');
        if (b.state === 'flying' && landed && !e.dead && e.brandable) {
          b.brand(e);
          this.emit({ t: 'lodge', x: b.x, y: b.y, angle: b.angle });
          break;
        }
        if (b.state === 'flying' && !landed && !e.prop) {
          // Bounced off armour.
          this.emit({ t: 'clank', x: b.x, y: b.y });
          b.recall();
          break;
        }
      }
    }
  }

  private playerAttacks(): void {
    const p = this.player;
    const a = p.attack;
    const box = p.attackBox();
    if (!a || !box) return;
    for (const e of this.entities) {
      if (e.dead || !e.hurtable) continue;
      if (this.hitBy.get(e.id) === a.id) continue;
      if (!overlap(box, e.box())) continue;
      this.hitBy.set(e.id, a.id);
      const dmg = BASE_DAMAGE * a.mul * this.damageMul();
      const landed = this.strike(e, dmg, p.cx, a.palm ? 'palm' : 'slash');
      p.onAttackLanded(landed ? 'foe' : 'wall', true);
      if (p.pogoed && this.seal('rebond')) this.gainLumen(6);
      p.pogoed = false;
      if (!landed) this.emit({ t: 'clank', x: (box.x + box.w / 2 + e.cx) / 2, y: e.cy });
    }
    // Down-strikes bounce off spikes too.
    if (a.dir === 'down' && !a.landed && this.boxHasTile(box, T.Spike)) {
      p.onAttackLanded('spike', true);
      this.emit({ t: 'spark', x: p.cx, y: box.y + box.h / 2, n: 8, color: 0xffffff });
    }
  }

  boxHasTile(box: Box, t: T): boolean {
    const x0 = Math.floor(box.x / TILE);
    const x1 = Math.floor((box.x + box.w - 1) / TILE);
    const y0 = Math.floor(box.y / TILE);
    const y1 = Math.floor((box.y + box.h - 1) / TILE);
    for (let ty = y0; ty <= y1; ty++) for (let tx = x0; tx <= x1; tx++) if (tileAt(this.room, tx, ty) === t) return true;
    return false;
  }

  private enemyHits(): void {
    const p = this.player;
    if (p.dead) return;
    const pb = p.box();
    for (const e of this.entities) {
      if (e.dead || e.stun > 0) continue;
      if (e.contact > 0 && overlap(pb, e.box())) this.hurtPlayer(e.contact, e.cx, true);
      for (const h of e.hits()) if (overlap(pb, h.box)) this.hurtPlayer(h.damage, h.box.x + h.box.w / 2, h.dodgeable !== false);
    }
  }

  /** Opens a flag-gated wall and refreshes tiles (persist: remembered in the save). */
  openBreakable(flag: string, persist = true): void {
    if (persist) this.setFlag(flag);
    this.tempOpen.add(flag);
    for (const [x, y, w, h, f] of this.room.def.brk ?? []) {
      if (f !== flag) continue;
      for (let ty = y; ty < y + h; ty++) for (let tx = x; tx < x + w; tx++) setTile(this.room, tx, ty, T.Empty);
      this.emit({ t: 'break', x: x * TILE, y: y * TILE, w: w * TILE, h: h * TILE });
    }
    this.tilesChanged++;
  }

  /** Closes a timed gate again (never on top of the hero). */
  closeBreakable(flag: string): void {
    this.tempOpen.delete(flag);
    for (const [x, y, w, h, f] of this.room.def.brk ?? []) {
      if (f !== flag) continue;
      for (let ty = y; ty < y + h; ty++) for (let tx = x; tx < x + w; tx++) setTile(this.room, tx, ty, T.Breakable);
    }
    const b = this.player.body;
    if (!fits(this.room, b.x, b.y, b.w, b.h)) {
      // Pushed out sideways rather than trapped.
      for (let d = 10; d < 200; d += 10) {
        if (fits(this.room, b.x - d, b.y, b.w, b.h)) { b.x -= d; break; }
        if (fits(this.room, b.x + d, b.y, b.w, b.h)) { b.x += d; break; }
      }
    }
    this.tilesChanged++;
    this.sfx('door');
  }

  /** A dash into black crystal shatters it with Comet Wake. */
  private dashIntoWall(x: number, y: number): void {
    const tx = Math.floor(x / TILE);
    const ty = Math.floor(y / TILE);
    for (const [bx, by, bw, bh, flag] of this.room.def.brk ?? []) {
      if (!flag.startsWith('crys_')) continue;
      if (tx >= bx - 1 && tx <= bx + bw && ty >= by - 1 && ty <= by + bh) {
        if (this.has('comet')) {
          this.openBreakable(flag);
          this.emit({ t: 'boom', x, y, r: 220, color: 0xc890ff });
        } else {
          this.emit({ t: 'clank', x, y });
          this.say(tr('toast.needAbility'), 120);
        }
        return;
      }
    }
  }

  /** Bumped when tiles change, so the view rebuilds the terrain. */
  tilesChanged = 0;
  tempOpen = new Set<string>();

  text(key: string, vars?: Record<string, string | number>): string {
    return tr(key, vars);
  }

  /** UI requests (dialogues, revelations, lore) handled by the game screen. */
  ui: (kind: string, arg: string, extra?: unknown) => void = () => {};

  collect(p: Pickup): void {
    this.give(p.kind, p.value, p.flag, p.cx, p.cy);
  }

  /** Grants a reward (pickups, shops, keepers). */
  give(kind: PickupKind, value: string, flag: string, x = this.player.cx, y = this.player.cy): void {
    const s = this.save;
    const p = { kind, value };
    this.setFlag(flag);
    this.emit({ t: 'pickup', x, y, what: kind });
    this.freeze = 12;
    switch (p.kind) {
      case 'ability':
        if (!s.abilities.includes(p.value)) s.abilities.push(p.value);
        this.ui('reveal', p.value);
        break;
      case 'art':
        if (!s.arts.includes(p.value)) s.arts.push(p.value);
        this.ui('revealArt', p.value);
        break;
      case 'shard':
        s.shards++;
        if (s.shards >= 4) {
          s.shards = 0;
          s.maxHp++;
          this.player.maxHp = s.maxHp;
          this.player.hp = s.maxHp;
          this.ui('banner', 'heart');
        } else this.ui('banner', 'shard');
        break;
      case 'vessel':
        s.vesselFrags++;
        if (s.vesselFrags >= 3) {
          s.vesselFrags = 0;
          s.vessels++;
          this.lumen = this.maxLumen();
          this.ui('banner', 'vesselFull');
        } else this.ui('banner', 'vessel');
        break;
      case 'ore':
        s.ore++;
        this.ui('banner', 'ore');
        break;
      case 'notch':
        s.notches++;
        this.ui('banner', 'notch');
        break;
      case 'chant':
        this.setFlag(`chant_${p.value}`);
        this.ui('banner', 'chant');
        break;
      case 'seal':
        if (!s.seals.includes(p.value)) s.seals.push(p.value);
        this.ui('bannerSeal', p.value);
        break;
      case 'map':
        this.setFlag(`map_${p.value}`);
        this.ui('bannerMap', p.value);
        break;
      case 'key':
        this.setFlag(`key_${p.value}`);
        this.ui('banner', 'key');
        break;
    }
  }


  private checkExit(): void {
    const p = this.player;
    const b = p.body;
    const r = this.room;
    let side: 'l' | 'r' | 'u' | 'd' | null = null;
    if (b.x + b.w / 2 < 0) side = 'l';
    else if (b.x + b.w / 2 > r.pxW) side = 'r';
    else if (b.y + b.h / 2 < 0) side = 'u';
    else if (b.y > r.pxH) side = 'd';
    if (!side || this.pending) return;
    const n = this.hooks.neighbour(r.def, side, p.cx, p.cy);
    if (!n) return;
    this.pending = { def: n.def, x: n.x, y: n.y, vx: b.vx, vy: b.vy };
  }

  /** Moves a free body with the room's collisions (helpers for entities). */
  moveBody(e: Entity): ReturnType<typeof move> {
    return move(this.room, e.body, e.body.vx, e.body.vy);
  }

  fits(x: number, y: number, w: number, h: number): boolean {
    return fits(this.room, x, y, w, h);
  }

  /** Rest at an altar: heal, save point, foes return. */
  rest(key: string): void {
    this.save.shrine = key;
    this.save.shrineRoom = this.room.id;
    this.player.hp = this.player.maxHp;
    this.killed.clear();
    this.onRest(key);
  }

  onRest: (key: string) => void = () => {};

  /** Spawns a keeper and starts the fight. */
  startBoss(key: string, x: number, feet: number): void {
    const b = makeBoss(this, key, x, feet);
    if (!b) return;
    this.add(b);
    this.boss = b;
    this.ui('bossIntro', key);
  }

  /** A keeper fell: reward, doors, flags. */
  bossDefeated(b: Boss): void {
    this.setFlag(`boss_${b.bossKey}`);
    this.boss = null;
    for (const [, , , , flag] of this.room.def.brk ?? []) if (flag.startsWith('seal_')) this.openBreakable(flag, true);
    const reward = BOSS_REWARD[b.bossKey];
    if (reward) {
      const p = new Pickup(this, b.cx, b.body.y + b.body.h - 40, reward[0], reward[1], `got_${b.bossKey}`);
      this.add(p);
    }
    this.ui('bossDown', b.bossKey);
  }

  say(text: string, frames = 180): void {
    this.toast = { text, t: frames };
  }
}
