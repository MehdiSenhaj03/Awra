import { expect, it } from 'vitest';
import { NO_CONTROLS } from '../src/game/controls';
import { Ring } from '../src/game/props';
import { makeScene } from './helpers';
it('the Heart trial can be crossed while the gate stays open (240 frames, with 40 to hit the eye and recall)', () => {
  const s = makeScene(['dash', 'walljump', 'blink', 'double', 'ring', 'comet'], 'h_epreuve');
  s.god = true;
  s.openBreakable('gate_h1');
  s.entities = s.entities.filter((e) => e instanceof Ring);
  const p = s.player;
  p.body.x = 37 * 40 - 17; p.body.y = 17 * 40 - p.body.h;
  let landed = -1;
  for (let f = 0; f < 300; f++) {
    const c = { ...NO_CONTROLS };
    const b = s.blade;
    if (b.held() && !p.zip) {
      const cands = s.entities.filter((e) => e instanceof Ring && Math.hypot(e.cx - p.cx, e.cy - p.cy) < 640 && e.cx > p.cx + 60);
      const target = cands.sort((a, c2) => a.cx - c2.cx)[0];
      if (target) {
        const dx = target.cx - p.cx; const dy = target.cy - (p.cy - 8);
        const a = Math.round(Math.atan2(dy, dx) / (Math.PI / 4)) * (Math.PI / 4);
        c.x = Math.round(Math.cos(a)); c.y = Math.round(Math.sin(a)); c.bladePressed = true;
      } else c.x = 1;
    } else if (b.ring) c.blinkPressed = true;
    else if (b.state === 'stuck') c.bladePressed = true;
    s.update(c);
    if (landed < 0 && p.body.onGround && p.cx > 84 * 40) landed = f;
  }
  expect(landed).toBeGreaterThan(0);
  expect(landed + 40).toBeLessThan(240);
});
