import { describe, expect, it } from 'vitest';
import { ORDER } from '../src/game/stages';
import { playStage } from './bot';

describe('every stage can be completed', () => {
  for (const id of ORDER) {
    it(id, () => {
      const r = playStage(id, { god: true });
      console.log(id, JSON.stringify({ over: r.over, s: r.seconds, score: r.score, phase: r.maxPhase, kills: r.stats.kills, perf: r.stats.perfects }));
      expect(r.over).toBe('win');
      if (id.endsWith('_4')) expect(r.maxPhase).toBe(3);
    });
  }
});
