// Desktop shell for Steam / itch.io builds: loads the static Vite build from disk.
const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('node:path');

// Use the GPU even when Chromium has it on its blocklist (otherwise WebGL may fall back to slow software
// rendering), rasterise on the GPU and never throttle the game when the window loses focus.
app.commandLine.appendSwitch('ignore-gpu-blocklist');
app.commandLine.appendSwitch('enable-gpu-rasterization');
app.commandLine.appendSwitch('enable-zero-copy');
app.commandLine.appendSwitch('disable-renderer-backgrounding');
app.commandLine.appendSwitch('disable-background-timer-throttling');
app.commandLine.appendSwitch('force_high_performance_gpu');

function createWindow() {
  const win = new BrowserWindow({
    width: 1600,
    height: 900,
    minWidth: 960,
    minHeight: 540,
    backgroundColor: '#000000',
    autoHideMenuBar: true,
    fullscreen: !process.argv.includes('--windowed'),
    title: 'VOLTA',
    icon: path.join(__dirname, '..', 'dist', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
      sandbox: true,
      backgroundThrottling: false,
    },
  });
  win.setMenu(null);
  win.loadFile(path.join(__dirname, '..', 'dist', 'index.html'));
  // F11 toggles fullscreen like most PC games.
  win.webContents.on('before-input-event', (event, input) => {
    if (input.type === 'keyDown' && input.key === 'F11') {
      win.setFullScreen(!win.isFullScreen());
      event.preventDefault();
    }
  });
}

ipcMain.on('quit', () => app.quit());
app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());
