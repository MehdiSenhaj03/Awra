// node casse/tools/audio-render.cjs <outdir> — renders music stems and effects to OGG.
const { chromium } = require('playwright');
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');
(async () => {
  const out = path.resolve(process.argv[2]);
  fs.mkdirSync(path.join(out, 'music'), { recursive: true });
  fs.mkdirSync(path.join(out, 'sfx'), { recursive: true });
  execFileSync('/home/user/Awra/volta/node_modules/.bin/esbuild', [path.join(__dirname, 'audio-render.ts'), '--bundle', '--format=iife', '--outfile=/tmp/casse-audio.js'], { stdio: 'inherit' });
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.setContent('<html><body></body></html>');
  await p.addScriptTag({ path: '/tmp/casse-audio.js' });
  const save = (dir, name, b64) => {
    const wav = path.join('/tmp', `c_${name}.wav`);
    fs.writeFileSync(wav, Buffer.from(b64, 'base64'));
    execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-i', wav, '-c:a', 'libvorbis', '-q:a', '5', path.join(out, dir, `${name}.ogg`)]);
    fs.unlinkSync(wav);
  };
  for (const n of await p.evaluate(() => window.casse.layers())) {
    save('music', n, await p.evaluate((n) => window.casse.layer(n), n));
    process.stdout.write(n + ' ');
  }
  save('music', 'title', await p.evaluate(() => window.casse.title()));
  for (const n of await p.evaluate(() => window.casse.sfxNames())) save('sfx', n, await p.evaluate((n) => window.casse.sfx(n), n));
  console.log('done');
  await b.close();
})();
