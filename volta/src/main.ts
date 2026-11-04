import '@fontsource/unbounded/500.css';
import '@fontsource/unbounded/700.css';
import '@fontsource/unbounded/800.css';
import '@fontsource/space-grotesk/400.css';
import '@fontsource/space-grotesk/600.css';
import '@fontsource/jetbrains-mono/500.css';
import '@fontsource/jetbrains-mono/700.css';
import './styles.css';
import './game/bosses/index';
import { App } from './ui/app';
import { input } from './engine/input';

const app = new App(document.getElementById('gl') as HTMLCanvasElement, document.getElementById('overlay')!, document.getElementById('ui')!);
void app.boot();
(window as unknown as { __volta: App }).__volta = app;
(window as unknown as { __voltaInput: typeof input }).__voltaInput = input;
