const { app, BrowserWindow, ipcMain, Menu, session } = require('electron')
const { exec, execSync, spawn }        = require('child_process')
const path                             = require('path')
const fs                               = require('fs')

const RUNTIME_DIR = path.join(__dirname, '../../runtime')
const BUILD_DIR   = path.join(__dirname, '../../build')
const RAYLIB      = fs.existsSync('/opt/homebrew/opt/raylib') ? '/opt/homebrew/opt/raylib' : '/usr/local/opt/raylib'
const RAYLIB_WIN  = fs.existsSync('/opt/homebrew/opt/raylib-win64/raylib-5.5_win64_mingw-w64')
  ? '/opt/homebrew/opt/raylib-win64/raylib-5.5_win64_mingw-w64'
  : '/usr/local/opt/raylib-win64/raylib-5.5_win64_mingw-w64'
const MINGW       = 'x86_64-w64-mingw32-gcc'
const CART_SRC    = path.join(BUILD_DIR, 'cart.c')
const CART_BIN    = path.join(BUILD_DIR, 'cart')
const CART_EXE    = path.join(BUILD_DIR, 'cart.exe')

function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 800,
    titleBarStyle: 'hiddenInset',
    backgroundColor: '#1b1c1f',
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
    },
  })

  win.loadURL('http://localhost:5173')

  win.webContents.on('before-input-event', (event, input) => {
    if (!input.meta && !input.control) return
    switch (input.key.toLowerCase()) {
      case 'c': event.preventDefault(); win.webContents.copy();      break
      case 'v': event.preventDefault(); win.webContents.paste();     break
      case 'x': event.preventDefault(); win.webContents.cut();       break
      case 'a': event.preventDefault(); win.webContents.selectAll(); break
      // NOTE: Cmd/Ctrl+Z (and Shift/Y) are intentionally NOT intercepted here.
      // Letting them reach the renderer lets CodeMirror's own history handle
      // undo/redo on the code tab, and the sprite editor handle it on the
      // sprites tab. Intercepting natively here would block the sprite editor.
    }
  })
}

app.whenReady().then(() => {
  session.defaultSession.setPermissionRequestHandler((_wc, permission, callback) => {
    callback(permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write')
  })
  session.defaultSession.setPermissionCheckHandler((_wc, permission) => {
    return permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write'
  })
  Menu.setApplicationMenu(Menu.buildFromTemplate([
    {
      label: 'dreamengine',
      submenu: [{ role: 'quit' }]
    },
    {
      label: 'Edit',
      submenu: [
        // undo/redo are handled in the renderer (CodeMirror history on the code
        // tab, the sprite editor on the sprites tab). Registering the menu roles
        // here would claim the Cmd+Z accelerator and block them, so they're omitted.
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ]
    },
    {
      label: 'View',
      submenu: [
        { role: 'zoomIn' },
        { role: 'zoomOut' },
        { role: 'resetZoom' },
        { type: 'separator' },
        { role: 'togglefullscreen' },
        { role: 'toggleDevTools' },
      ]
    }
  ]))
  createWindow()
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})


// ── sprites handler ───────────────────────────────────────────
ipcMain.handle('studio:save-sprites', (_event, dataUrl) => {
  fs.mkdirSync(BUILD_DIR, { recursive: true })
  const base64 = dataUrl.replace(/^data:image\/png;base64,/, '')
  fs.writeFileSync(path.join(BUILD_DIR, 'sprites.png'), base64, 'base64')
})

// ── map handler ───────────────────────────────────────────────
// receives a Uint8Array (length MAP_W * MAP_H = 8192) of cell indices
ipcMain.handle('studio:save-map', (_event, bytes) => {
  fs.mkdirSync(BUILD_DIR, { recursive: true })
  fs.writeFileSync(path.join(BUILD_DIR, 'map.dat'), Buffer.from(bytes))
})

// ── run handler ───────────────────────────────────────────────
ipcMain.handle('studio:run', async (_event, code, cfg) => {
  const screenW       = cfg?.screenW || 320
  const screenH       = cfg?.screenH || 200
  const scale         = cfg?.scale   || 4
  const mapW          = cfg?.mapW    || 128
  const mapH          = cfg?.mapH    || 64
  const touchDefault  = cfg?.touchControls ? 1 : 0
  const studioC       = path.join(RUNTIME_DIR, 'studio.c')

  fs.mkdirSync(BUILD_DIR, { recursive: true })

  // remove any stale files from previous sessions
  const stale = ['ComicMono.ttf', 'ComicMono-Bold.ttf', 'PixelComicSans.ttf', 'Ac437_Acer_VGA_8x8.ttf']
  stale.forEach(f => { try { fs.unlinkSync(path.join(BUILD_DIR, f)) } catch {} })

  fs.writeFileSync(CART_SRC, code)

  // generate sprites_data.h from sprites.png via xxd (PNG is compressed — stays small)
  const spritesHeader = path.join(BUILD_DIR, 'sprites_data.h')
  const spritesPng    = path.join(BUILD_DIR, 'sprites.png')
  if (fs.existsSync(spritesPng)) {
    try {
      const xxd = execSync('xxd -i sprites.png', { cwd: BUILD_DIR }).toString()
      fs.writeFileSync(spritesHeader,
        xxd.replace(/unsigned char sprites_png\[\]/,  'static const unsigned char SPRITES_DATA[]')
           .replace(/unsigned int sprites_png_len/,   'static const unsigned int  SPRITES_DATA_LEN'))
    } catch {
      fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
    }
  } else {
    fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
  }

  // generate map_data.h from map.dat (raw 8192 bytes = MAP_W × MAP_H cell indices)
  const mapHeader = path.join(BUILD_DIR, 'map_data.h')
  const mapDat    = path.join(BUILD_DIR, 'map.dat')
  if (fs.existsSync(mapDat)) {
    try {
      const xxd = execSync('xxd -i map.dat', { cwd: BUILD_DIR }).toString()
      fs.writeFileSync(mapHeader,
        xxd.replace(/unsigned char map_dat\[\]/,  'static const unsigned char MAP_DATA[]')
           .replace(/unsigned int map_dat_len/,   'static const unsigned int  MAP_DATA_LEN'))
    } catch {
      fs.writeFileSync(mapHeader, 'static const unsigned char MAP_DATA[]={0};static const unsigned int MAP_DATA_LEN=0;\n')
    }
  } else {
    fs.writeFileSync(mapHeader, 'static const unsigned char MAP_DATA[]={0};static const unsigned int MAP_DATA_LEN=0;\n')
  }

  const args = [
    `"${CART_SRC}"`,
    `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`,
    `-I"${BUILD_DIR}"`,
    `-I"${RAYLIB}/include"`,
    `-DSCREEN_W=${screenW}`,
    `-DSCREEN_H=${screenH}`,
    `-DSCALE=${scale}`,
    `-DMAP_W=${mapW}`,
    `-DMAP_H=${mapH}`,
    `-DTOUCH_CONTROLS_DEFAULT=${touchDefault}`,
    '-Os',
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL',
    '-framework Cocoa',
    '-framework IOKit',
    '-framework CoreVideo',
    '-framework CoreFoundation',
    '-Wl,-dead_strip',
    `-o "${CART_BIN}"`,
  ]

  const cmd = `clang ${args.join(' ')}`

  return new Promise(resolve => {
    exec(cmd, (err, _stdout, stderr) => {
      // filter out the noisy macOS version warnings — they're never actionable
      const warnings = stderr
        .split('\n')
        .filter(l => !l.includes('was built for newer macOS version'))
        .join('\n')
        .trim()

      if (err) return resolve({ ok: false, cmd, output: warnings })

      const proc = spawn(CART_BIN, [], { detached: true, stdio: 'ignore', cwd: BUILD_DIR })
      proc.unref()

      // also cross-compile for Windows in the background
      if (fs.existsSync(MINGW) || fs.existsSync(`/usr/local/bin/${MINGW}`)) {
        const winArgs = [
          `"${CART_SRC}"`, `"${studioC}"`,
          `-I"${RUNTIME_DIR}"`, `-I"${BUILD_DIR}"`, `-I"${RAYLIB_WIN}/include"`,
          `-DSCREEN_W=${screenW}`,
          `-DSCREEN_H=${screenH}`,
          `-DSCALE=${scale}`,
          `-DTOUCH_CONTROLS_DEFAULT=${touchDefault}`,
          '-Os',
          `"${RAYLIB_WIN}/lib/libraylib.a"`,
          '-lopengl32', '-lgdi32', '-lwinmm', '-lcomdlg32',
          '-Wl,--gc-sections',
          `-o "${CART_EXE}"`,
        ]
        exec(`${MINGW} ${winArgs.join(' ')}`, () => {})
      }

      resolve({
        ok:     true,
        cmd,
        output: warnings || null,
        src:    CART_SRC,
        bin:    CART_BIN,
        exe:    CART_EXE,
      })
    })
  })
})
