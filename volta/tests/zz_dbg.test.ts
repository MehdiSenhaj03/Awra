import { it } from 'vitest';
import { STAGES } from '../src/game/stages';
import { StageRunner } from '../src/game/stage';
import { World } from '../src/game/world';
import { botControls } from './bot';
it('dbg', () => {
  for (const id of (process.env.IDS ?? 's1_3').split(',')) {
    const def = STAGES[id];
    const w = new World(def.arena, def.bpm, 3);
    w.god = true;
    w.player.y = 4;
    const run = new StageRunner(w, def);
    for (let f = 0; f < 60 * 300 && !w.over; f++) {
      w.update(botControls(w));
      run.update();
      w.events.length = 0;
    }
    console.log(id, run.waveNumber, w.over, w.enemies.map((e) => `${e.kind}:${Math.round(e.hp)}@${e.x.toFixed(0)},${e.y.toFixed(0)}${e.spawnT > 0 ? 'S' : ''}`).join(' '));
  }
});
