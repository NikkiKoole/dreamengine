const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('studio', {
  run:          (code, cfg) => ipcRenderer.invoke('studio:run', code, cfg),
  saveSprites:  (b64, w, h) => ipcRenderer.invoke('studio:save-sprites', b64, w, h),
})
