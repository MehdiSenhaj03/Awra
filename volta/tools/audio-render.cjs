// node tools/audio-render.cjs <outdir>: renders every stem, note and effect to WAV, then OGG via ffmpeg.
const { chromium } = require('playwright');
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');
(async () => {
  const out = path.resolve(process.argv[2]);
  fs.mkdirSync(path.join(out, 'music'), { recursive: true });
  fs.mkdirSync(path.join(out, 'notes'), { recursive: true });
  fs.mkdirSync(path.join(out, 'sfx'), { recursive: true });
  execFileSync('node_modules/.bin/esbuild', ['tools/audio-render.ts', '--bundle', '--format=iife', '--outfile=/tmp/volta-audio.js'], { stdio: 'inherit' });
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.setContent('<html><body></body></html>');
  await p.addScriptTag({ path: '/tmp/volta-audio.js' });
  const save = (dir, name, b64, q) => {
    const wav = path.join('/tmp', `${name}.wav`);
    fs.writeFileSync(wav, Buffer.from(b64, 'base64'));
    execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-i', wav, '-c:a', 'libvorbis', '-q:a', String(q), path.join(out, dir, `${name}.ogg`)]);
    fs.unlinkSync(wav);
  };
  const only = process.argv[3];
  if (!only || only === 'music') {
    for (const s of await p.evaluate(() => window.volta.stems())) {
      save('music', s.name, await p.evaluate((s) => window.volta.stem(s.theme, s.bpm, s.i, s.od), s), 5);
      process.stdout.write(`${s.name} `);
    }
  }
  if (!only || only === 'notes') {
    for (const k of await p.evaluate(() => window.volta.notes())) save('notes', k, await p.evaluate((k) => window.volta.note(k), k), 4);
    console.log('notes ok');
  }
  if (!only || only === 'sfx') {
    for (const k of await p.evaluate(() => window.volta.sfxNames())) save('sfx', k, await p.evaluate((k) => window.volta.sfx(k), k), 4);
    console.log('sfx ok');
  }
  await b.close();
})();
