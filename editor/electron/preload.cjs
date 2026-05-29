const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('studio', {
  run:          (code, cfg) => ipcRenderer.invoke('studio:run', code, cfg),
  saveSprites:  (dataUrl) => ipcRenderer.invoke('studio:save-sprites', dataUrl),
  saveMap:      (bytes)   => ipcRenderer.invoke('studio:save-map', bytes),
  onLog:        (cb) => ipcRenderer.on('cart:log',  (_, s)    => cb(s)),
  onExit:       (cb) => ipcRenderer.on('cart:exit', (_, info) => cb(info)),
})
