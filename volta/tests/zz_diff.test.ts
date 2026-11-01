import { it } from 'vitest';
import { ORDER } from '../src/game/stages';
import { playStage } from './bot';
it('difficulty', () => {
  for (const id of ORDER) {
    const r = playStage(id, { god: false, maxFrames: 60 * 300 });
    console.log(id, r.over ?? 'timeout', 'wave', r.wave, 's', r.seconds, 'hits', r.stats.hits, 'grazes', r.stats.grazes, 'parries', r.stats.parries);
  }
});
