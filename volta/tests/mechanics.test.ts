import { describe, expect, it } from 'vitest';
import '../src/game/bosses/index';
import { BeatClock } from '../src/game/beat';
import { IDLE, type Controls } from '../src/game/controls';
import { Bulwark, Mirror } from '../src/game/enemies';
import { SK } from '../src/game/bullets';
import { GRADES, grade, newSave, slots, stageOpen } from '../src/game/meta';
import { World } from '../src/game/world';

const c = (o: Partial<Controls>): Controls => ({ ...IDLE, ...o });

/** Advances until the next beat is `frames` frames away. */
function toBeat(w: World, before = 0): void {
  for (let i = 0; i < 200; i++) {
    const per = w.beat.spb * 60;
    const framesLeft = (((Math.ceil(w.beat.beat - 1e-6) - w.beat.beat) * per) % per + per) % per;
    if (framesLeft <= before + 0.5 && framesLeft > before - 0.5) return;
    w.update(IDLE);
  }
}

describe('rhythm', () => {
  it('the beat clock keeps time and bends without jumps', () => {
    const b = new BeatClock(120);
    for (let i = 0; i < 60; i++) b.tick();
    expect(b.beat).toBeCloseTo(2, 5);
    b.setBpm(60);
    const before = b.beat;
    b.tick();
    expect(b.beat - before).toBeCloseTo(1 / 60, 5);
  });

  it('judges presses around the beat', () => {
    const b = new BeatClock(120);
    for (let i = 0; i < 60; i++) b.tick();
    expect(b.judge()).toBe('perfect');
    for (let i = 0; i < 5; i++) b.tick();
    expect(b.judge()).toBe('great');
    for (let i = 0; i < 10; i++) b.tick();
    expect(b.judge()).toBe('off');
  });

  it('a press on the beat fires a perfect volley; off the beat it does not', () => {
    const w = new World({ shape: 'circle', r: 20 }, 120);
    toBeat(w, 1);
    w.update(c({ fire: true, firePressed: true }));
    expect(w.stats.perfects).toBe(1);
    expect(w.shots.n).toBeGreaterThanOrEqual(5);
    const w2 = new World({ shape: 'circle', r: 20 }, 120);
    toBeat(w2, 15);
    w2.update(c({ fire: true, firePressed: true }));
    expect(w2.stats.perfects).toBe(0);
  });

  it('holding fire plays sixteenths with an accent on the beat', () => {
    const w = new World({ shape: 'circle', r: 20 }, 120);
    let shots = 0;
    for (let i = 0; i < 120; i++) {
      const before = w.shots.n;
      w.update(c({ fire: true }));
      if (w.shots.n > before) shots++;
    }
    // 2 seconds at 120 BPM: 16 sixteenths.
    expect(shots).toBeGreaterThanOrEqual(15);
    expect(shots).toBeLessThanOrEqual(17);
  });
});

describe('combat', () => {
  it('the pulse sends bullets back at foes', () => {
    const w = new World({ shape: 'circle', r: 20 }, 120);
    w.add(new Mirror(0, -12)).spawnT = 0;
    for (let i = 0; i < 6; i++) w.bullets.add(Math.cos(i) * 1.5, Math.sin(i) * 1.5, 0, 0.1);
    w.update(c({ pulse: true }));
    expect(w.stats.parries).toBe(6);
    for (let i = 0; i < w.bullets.n; i++) expect(w.bullets.flags[i]).toBe(2);
  });

  it('dashing makes the hero untouchable', () => {
    const w = new World({ shape: 'circle', r: 20 }, 120);
    w.update(c({ dash: true, mx: 1 }));
    w.bullets.add(w.player.x, w.player.y, 0, 0);
    w.update(IDLE);
    expect(w.player.hp).toBe(4);
  });

  it('a bulwark blocks from the front until accents crack its shield', () => {
    const w = new World({ shape: 'circle', r: 20 }, 120);
    const b = w.add(new Bulwark(0, -6));
    b.spawnT = 0;
    b.shieldA = Math.PI / 2;
    const hp = b.hp;
    expect(b.hit(w, 1, 0, 0, SK.Bolt)).toBe(false);
    expect(b.hp).toBe(hp);
    for (let i = 0; i < 6; i++) b.hit(w, 1, 0, 0, SK.Accent);
    expect(b.broken).toBeGreaterThan(0);
    expect(b.hit(w, 1, 0, 0, SK.Bolt)).toBe(true);
  });

  it('getting hit drops the style rank by two', () => {
    const w = new World({ shape: 'circle', r: 20 }, 120);
    w.gain(600, 0, 0);
    expect(w.rank).toBe(4);
    w.hurtPlayer(1, 0);
    expect(w.rank).toBe(2);
    expect(w.player.hp).toBe(3);
  });
});

describe('progression', () => {
  it('opens stages in order and grades results', () => {
    const s = newSave();
    expect(stageOpen(s, 's1_1')).toBe(true);
    expect(stageOpen(s, 's1_2')).toBe(false);
    s.stages.s1_1 = { best: 1, rank: 0, cleared: true, fragment: false };
    expect(stageOpen(s, 's1_2')).toBe(true);
    expect(stageOpen(s, 's2_1')).toBe(false);
    s.stages.s1_4 = { best: 1, rank: 0, cleared: true, fragment: false };
    expect(stageOpen(s, 's2_1')).toBe(true);
    expect(slots(s)).toBe(2);
    expect(GRADES[grade('s1_1', 1e9, 0, 6)]).toBe('V');
    expect(GRADES[grade('s1_1', 1e9, 1, 6)]).toBe('S');
    expect(GRADES[grade('s1_1', 0, 0, 0)]).toBe('D');
  });
});
