import { BOSSES } from '../bosses';
import { Crescendo } from './crescendo';
import { Fermata } from './fermata';
import { makeFugue } from './fugue';
import { Ostinato } from './ostinato';
import { Rubato } from './rubato';
import { Staccato } from './staccato';

BOSSES.ostinato = (w) => new Ostinato(w);
BOSSES.crescendo = (w) => new Crescendo(w);
BOSSES.fugue = (w) => makeFugue(w);
BOSSES.rubato = (w) => new Rubato(w);
BOSSES.staccato = (w) => new Staccato(w);
BOSSES.fermata = (w) => new Fermata(w);
