const { contextBridge, ipcRenderer } = require('electron');

// Lets the title screen offer a "Quit" button in the desktop build only.
contextBridge.exposeInMainWorld('voltaDesktop', {
  quit: () => ipcRenderer.send('quit'),
});
