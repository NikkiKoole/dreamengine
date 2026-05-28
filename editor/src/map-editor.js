// visual map editor — paints an MAP_W × MAP_H grid of sprite indices.
// dimensions are settings-driven and passed to the compile via -DMAP_W / -DMAP_H.
// see runtime/studio.h for the map() / mget() / mset() runtime API.

import { settings } from './settings.js'

let MAP_W = settings.mapW
let MAP_H = settings.mapH
const SPRITE_SIZE = 16
const PICKER_PX   = 32   // sprite picker tiles render at 32×32 (2× zoom)
const PICKER_COLS = 8    // spritesheet is 8×8 grid (64 sprites)

let CELL_PX = parseInt(localStorage.getItem('mapZoom') || '1', 10) * SPRITE_SIZE

let mapData = new Uint8Array(MAP_W * MAP_H)
let selectedSprite = 1

// tool state — mirrors the sprite editor: pixel | line | select | stamp
let currentTool = 'pixel'
let brushSize   = parseInt(localStorage.getItem('mapBrush') || '1', 10)

// in-progress drag state
let isMouseDown = false
let mouseButton = 0
let dragStart   = null   // { cx, cy } — anchor for line/select
let dragLast    = null   // last cell painted, for bresenham fill on drag

// selection result (also seeds the stamp buffer)
let selectionRect = null  // { x0, y0, x1, y1 } in cell space
let stampBuffer   = null  // { w, h, data: Uint8Array } from a finalized selection

let viewport, canvas, ctx
let overlayCanvas, overlayCtx
let pickerCanvas, pickerCtx
let statusEl
let tilemapCanvas

// ── undo / redo ─────────────────────────────────────────────────
const UNDO_MAX = 64
const undoStack = []
const redoStack = []

function pushUndo() {
  undoStack.push(new Uint8Array(mapData))
  if (undoStack.length > UNDO_MAX) undoStack.shift()
  redoStack.length = 0                       // any new action invalidates redo
}

function undo() {
  if (!undoStack.length) return
  redoStack.push(new Uint8Array(mapData))
  mapData.set(undoStack.pop())
  renderAll()
  saveToStorage()
}

function redo() {
  if (!redoStack.length) return
  undoStack.push(new Uint8Array(mapData))
  mapData.set(redoStack.pop())
  renderAll()
  saveToStorage()
}

// ── persistence ─────────────────────────────────────────────────
function loadFromStorage() {
  const s = localStorage.getItem('mapData')
  if (!s) return
  try {
    const bin = atob(s)
    const n = Math.min(bin.length, mapData.length)
    for (let i = 0; i < n; i++) mapData[i] = bin.charCodeAt(i)
  } catch {}
}

function saveToStorage() {
  let bin = ''
  for (let i = 0; i < mapData.length; i++) bin += String.fromCharCode(mapData[i])
  localStorage.setItem('mapData', btoa(bin))
}

// ── rendering ───────────────────────────────────────────────────
function renderCell(cx, cy) {
  if (!ctx) return
  const v = mapData[cy * MAP_W + cx]
  const dx = cx * CELL_PX, dy = cy * CELL_PX
  ctx.fillStyle = ((cx + cy) & 1) ? '#1c1d22' : '#16171a'
  ctx.fillRect(dx, dy, CELL_PX, CELL_PX)
  if (v > 0 && tilemapCanvas) {
    const sx = (v % PICKER_COLS) * SPRITE_SIZE
    const sy = Math.floor(v / PICKER_COLS) * SPRITE_SIZE
    ctx.drawImage(tilemapCanvas, sx, sy, SPRITE_SIZE, SPRITE_SIZE, dx, dy, CELL_PX, CELL_PX)
  }
}

function renderAll() {
  if (!ctx) return
  for (let cy = 0; cy < MAP_H; cy++) {
    for (let cx = 0; cx < MAP_W; cx++) renderCell(cx, cy)
  }
}

function renderPicker() {
  if (!pickerCtx || !tilemapCanvas) return
  pickerCtx.clearRect(0, 0, pickerCanvas.width, pickerCanvas.height)
  pickerCtx.drawImage(tilemapCanvas, 0, 0, pickerCanvas.width, pickerCanvas.height)
  pickerCtx.fillStyle = 'rgba(0,0,0,0.55)'
  pickerCtx.fillRect(0, 0, PICKER_PX, PICKER_PX)
  pickerCtx.strokeStyle = '#ff6e6e'
  pickerCtx.lineWidth   = 2
  pickerCtx.beginPath()
  pickerCtx.moveTo(2, 2);              pickerCtx.lineTo(PICKER_PX - 2, PICKER_PX - 2)
  pickerCtx.moveTo(PICKER_PX - 2, 2);  pickerCtx.lineTo(2, PICKER_PX - 2)
  pickerCtx.stroke()
  const sx = (selectedSprite % PICKER_COLS) * PICKER_PX
  const sy = Math.floor(selectedSprite / PICKER_COLS) * PICKER_PX
  pickerCtx.strokeStyle = '#c9a96e'
  pickerCtx.lineWidth   = 2
  pickerCtx.strokeRect(sx + 1, sy + 1, PICKER_PX - 2, PICKER_PX - 2)
}

// ── overlay (line preview, selection rect, cursor brush outline) ──
function clearOverlay() {
  if (!overlayCtx) return
  overlayCtx.clearRect(0, 0, overlayCanvas.width, overlayCanvas.height)
}

function strokeCells(rect, color) {
  overlayCtx.strokeStyle = color
  overlayCtx.lineWidth   = 2
  overlayCtx.strokeRect(
    rect.x0 * CELL_PX + 1, rect.y0 * CELL_PX + 1,
    (rect.x1 - rect.x0 + 1) * CELL_PX - 2,
    (rect.y1 - rect.y0 + 1) * CELL_PX - 2,
  )
}

function drawLinePreview(cx0, cy0, cx1, cy1) {
  overlayCtx.fillStyle = 'rgba(201, 169, 110, 0.4)'
  bresenhamCells(cx0, cy0, cx1, cy1).forEach(([cx, cy]) => {
    overlayCtx.fillRect(cx * CELL_PX, cy * CELL_PX, CELL_PX, CELL_PX)
  })
}

// ── input helpers ───────────────────────────────────────────────
function cellFromEvent(e) {
  const rect = canvas.getBoundingClientRect()
  return {
    cx: Math.floor((e.clientX - rect.left) / CELL_PX),
    cy: Math.floor((e.clientY - rect.top)  / CELL_PX),
  }
}

function inBounds(cx, cy) {
  return cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H
}

function bresenhamCells(x0, y0, x1, y1) {
  const out = []
  const dx = Math.abs(x1 - x0), dy = Math.abs(y1 - y0)
  const sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1
  let err = dx - dy, x = x0, y = y0
  while (true) {
    out.push([x, y])
    if (x === x1 && y === y1) break
    const e2 = 2 * err
    if (e2 > -dy) { err -= dy; x += sx }
    if (e2 <  dx) { err += dx; y += sy }
  }
  return out
}

function writeCell(cx, cy, v) {
  if (!inBounds(cx, cy)) return false
  if (mapData[cy * MAP_W + cx] === v) return false
  mapData[cy * MAP_W + cx] = v
  renderCell(cx, cy)
  return true
}

// paint a brush-sized square of cells centered on (cx, cy)
function paintBrush(cx, cy, erase) {
  const v = erase ? 0 : selectedSprite
  const r = (brushSize - 1) >> 1
  for (let dy = -r; dy <= r; dy++) {
    for (let dx = -r; dx <= r; dx++) {
      writeCell(cx + dx, cy + dy, v)
    }
  }
}

// 4-connected flood fill — replaces a connected region of the same value
function floodFill(cx, cy, erase) {
  if (!inBounds(cx, cy)) return
  const target = mapData[cy * MAP_W + cx]
  const v = erase ? 0 : selectedSprite
  if (target === v) return
  const stack = [[cx, cy]]
  while (stack.length) {
    const [x, y] = stack.pop()
    if (!inBounds(x, y)) continue
    if (mapData[y * MAP_W + x] !== target) continue
    mapData[y * MAP_W + x] = v
    renderCell(x, y)
    stack.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1])
  }
}

function eyedropCell(cx, cy) {
  if (!inBounds(cx, cy)) return
  const v = mapData[cy * MAP_W + cx]
  if (v > 0) {
    selectedSprite = v
    renderPicker()
    updateStatus(cx, cy)
  }
}

function updateStatus(cx, cy) {
  if (!statusEl) return
  const cellStr = (cx !== undefined && inBounds(cx, cy)) ? `cell (${cx}, ${cy})` : 'cell —'
  statusEl.textContent = `${cellStr}   sprite #${selectedSprite}   zoom ${CELL_PX / SPRITE_SIZE}×   ${currentTool}`
}

// ── tool dispatch ───────────────────────────────────────────────
function onMouseDown(e) {
  isMouseDown = true
  mouseButton = e.button
  const { cx, cy } = cellFromEvent(e)
  dragLast = { cx, cy }
  dragStart = { cx, cy }
  const erase = (e.button === 2) || e.altKey

  if (e.shiftKey) {
    eyedropCell(cx, cy)
  } else if (currentTool === 'pixel') {
    pushUndo()
    paintBrush(cx, cy, erase)
  } else if (currentTool === 'fill') {
    pushUndo()
    floodFill(cx, cy, erase)
    isMouseDown = false   // single-click fill
    saveToStorage()
  } else if (currentTool === 'line') {
    pushUndo()            // snapshot at gesture start; line commits on mouseup
  } else if (currentTool === 'stamp') {
    pushUndo()
    pasteStamp(cx, cy)
    isMouseDown = false   // single-click stamp
  }
  // select doesn't modify state, so no undo needed
  e.preventDefault()
}

function onMouseMove(e) {
  const { cx, cy } = cellFromEvent(e)
  updateStatus(cx, cy)

  // hover preview for stamp: show where the next paste will land
  if (currentTool === 'stamp' && !isMouseDown && stampBuffer) {
    clearOverlay()
    strokeCells(
      { x0: cx, y0: cy, x1: cx + stampBuffer.w - 1, y1: cy + stampBuffer.h - 1 },
      '#c9a96e',
    )
    return
  }

  if (!isMouseDown) return
  const erase = (mouseButton === 2) || e.altKey

  if (currentTool === 'pixel') {
    if (dragLast) {
      bresenhamCells(dragLast.cx, dragLast.cy, cx, cy).forEach(([x, y]) => paintBrush(x, y, erase))
    }
    dragLast = { cx, cy }
  } else if (currentTool === 'line' && dragStart) {
    clearOverlay()
    drawLinePreview(dragStart.cx, dragStart.cy, cx, cy)
  } else if (currentTool === 'select' && dragStart) {
    clearOverlay()
    const r = normalizeRect(dragStart.cx, dragStart.cy, cx, cy)
    strokeCells(r, '#c9a96e')
  }
}

function onMouseUp(e) {
  if (!isMouseDown) return
  const erase = (mouseButton === 2) || e.altKey
  let { cx, cy } = dragLast || {}
  if (e) ({ cx, cy } = cellFromEvent(e))

  if (currentTool === 'line' && dragStart) {
    bresenhamCells(dragStart.cx, dragStart.cy, cx, cy).forEach(([x, y]) => paintBrush(x, y, erase))
    clearOverlay()
  } else if (currentTool === 'select' && dragStart) {
    selectionRect = normalizeRect(dragStart.cx, dragStart.cy, cx, cy)
    captureStampFromSelection()             // stamp tool is ready when you switch to it
    clearOverlay()
    strokeCells(selectionRect, '#c9a96e')   // stay in select; keyboard ops act on this rect
  }

  isMouseDown = false
  dragStart = null
  dragLast  = null
  saveToStorage()
}

function normalizeRect(x0, y0, x1, y1) {
  return {
    x0: Math.max(0, Math.min(MAP_W - 1, Math.min(x0, x1))),
    y0: Math.max(0, Math.min(MAP_H - 1, Math.min(y0, y1))),
    x1: Math.max(0, Math.min(MAP_W - 1, Math.max(x0, x1))),
    y1: Math.max(0, Math.min(MAP_H - 1, Math.max(y0, y1))),
  }
}

function captureStampFromSelection() {
  if (!selectionRect) return
  const { x0, y0, x1, y1 } = selectionRect
  const w = x1 - x0 + 1, h = y1 - y0 + 1
  const data = new Uint8Array(w * h)
  for (let yy = 0; yy < h; yy++) {
    for (let xx = 0; xx < w; xx++) {
      data[yy * w + xx] = mapData[(y0 + yy) * MAP_W + (x0 + xx)]
    }
  }
  stampBuffer = { w, h, data }
}

// ── selection transforms (keyboard) ─────────────────────────────
// Each op pushes undo, mutates only cells inside selectionRect, and re-renders.
function selectionForEach(fn) {
  if (!selectionRect) return
  const { x0, y0, x1, y1 } = selectionRect
  const w = x1 - x0 + 1, h = y1 - y0 + 1
  // snapshot the region so transforms read from original
  const src = new Uint8Array(w * h)
  for (let yy = 0; yy < h; yy++)
    for (let xx = 0; xx < w; xx++)
      src[yy * w + xx] = mapData[(y0 + yy) * MAP_W + (x0 + xx)]
  for (let yy = 0; yy < h; yy++) {
    for (let xx = 0; xx < w; xx++) {
      const v = fn(xx, yy, w, h, src)
      writeCell(x0 + xx, y0 + yy, v)
    }
  }
}

function flipSelectionH() {
  if (!selectionRect) return
  pushUndo()
  selectionForEach((xx, yy, w, h, src) => src[yy * w + (w - 1 - xx)])
  saveToStorage()
  clearOverlay()
  strokeCells(selectionRect, '#c9a96e')
}

function flipSelectionV() {
  if (!selectionRect) return
  pushUndo()
  selectionForEach((xx, yy, w, h, src) => src[(h - 1 - yy) * w + xx])
  saveToStorage()
  clearOverlay()
  strokeCells(selectionRect, '#c9a96e')
}

function rotateSelection() {
  if (!selectionRect) return
  const { x0, y0, x1, y1 } = selectionRect
  if ((x1 - x0) !== (y1 - y0)) return  // only square selections can rotate in place
  pushUndo()
  selectionForEach((xx, yy, w, h, src) => src[(w - 1 - xx) * w + yy])
  saveToStorage()
  clearOverlay()
  strokeCells(selectionRect, '#c9a96e')
}

// arrow keys shift the selection by one cell, wrapping the contents at the edges
function shiftSelection(dx, dy) {
  if (!selectionRect) return
  const { x0, y0, x1, y1 } = selectionRect
  const w = x1 - x0 + 1, h = y1 - y0 + 1
  pushUndo()
  selectionForEach((xx, yy, w_, h_, src) => {
    const sx = ((xx - dx) % w + w) % w
    const sy = ((yy - dy) % h + h) % h
    return src[sy * w + sx]
  })
  saveToStorage()
  clearOverlay()
  strokeCells(selectionRect, '#c9a96e')
}

function pasteStamp(cx, cy) {
  if (!stampBuffer) return
  const { w, h, data } = stampBuffer
  for (let yy = 0; yy < h; yy++) {
    for (let xx = 0; xx < w; xx++) {
      writeCell(cx + xx, cy + yy, data[yy * w + xx])
    }
  }
  clearOverlay()
  saveToStorage()
}

// ── picker ──────────────────────────────────────────────────────
function onPickerClick(e) {
  const rect = pickerCanvas.getBoundingClientRect()
  const sx = Math.floor((e.clientX - rect.left) / PICKER_PX)
  const sy = Math.floor((e.clientY - rect.top)  / PICKER_PX)
  if (sx < 0 || sx >= PICKER_COLS || sy < 0) return
  selectedSprite = sy * PICKER_COLS + sx
  renderPicker()
  updateStatus()
}

// ── zoom / brush ────────────────────────────────────────────────
function setZoom(level) {
  level = Math.max(1, Math.min(4, level | 0))
  CELL_PX = level * SPRITE_SIZE
  localStorage.setItem('mapZoom', String(level))
  if (canvas) {
    canvas.width  = MAP_W * CELL_PX
    canvas.height = MAP_H * CELL_PX
    ctx.imageSmoothingEnabled = false
    overlayCanvas.width  = canvas.width
    overlayCanvas.height = canvas.height
    overlayCtx.imageSmoothingEnabled = false
    renderAll()
    clearOverlay()
  }
  updateStatus()
}

function setBrush(n) {
  brushSize = Math.max(1, Math.min(7, n | 0))
  if (brushSize % 2 === 0) brushSize++   // keep odd so the center cell exists
  localStorage.setItem('mapBrush', String(brushSize))
}

// resize the map; preserves cells that lie inside the overlap of old and new dims.
// undo/redo history is cleared because old snapshots are sized differently.
function setMapSize(newW, newH) {
  newW = Math.max(4, Math.min(512, newW | 0))
  newH = Math.max(4, Math.min(512, newH | 0))
  if (newW === MAP_W && newH === MAP_H) return
  const oldW = MAP_W
  const oldData = mapData
  const next = new Uint8Array(newW * newH)
  const cw = Math.min(MAP_W, newW)
  const ch = Math.min(MAP_H, newH)
  for (let cy = 0; cy < ch; cy++) {
    for (let cx = 0; cx < cw; cx++) next[cy * newW + cx] = oldData[cy * oldW + cx]
  }
  mapData = next
  MAP_W = newW
  MAP_H = newH
  settings.mapW = newW
  settings.mapH = newH
  localStorage.setItem('mapW', String(newW))
  localStorage.setItem('mapH', String(newH))
  undoStack.length = 0
  redoStack.length = 0
  if (canvas) {
    canvas.width  = MAP_W * CELL_PX
    canvas.height = MAP_H * CELL_PX
    overlayCanvas.width  = canvas.width
    overlayCanvas.height = canvas.height
    ctx.imageSmoothingEnabled        = false
    overlayCtx.imageSmoothingEnabled = false
    renderAll()
    clearOverlay()
  }
  saveToStorage()
  updateStatus()
}

// ── tool UI ─────────────────────────────────────────────────────
const TOOLS = ['pixel', 'fill', 'line', 'select', 'stamp']

function setActiveTool(name) {
  if (!TOOLS.includes(name)) return
  currentTool = name
  TOOLS.forEach(t => {
    const el = document.getElementById('map-tool-' + t)
    if (el) el.classList.toggle('active', t === name)
  })
  updateStatus()
}

// ── init ────────────────────────────────────────────────────────
function init() {
  const panel = document.getElementById('panel-map')
  if (!panel) return

  canvas        = document.getElementById('map-canvas')
  overlayCanvas = document.getElementById('map-canvas-overlay')
  pickerCanvas  = document.getElementById('map-picker-canvas')
  if (!canvas || !overlayCanvas || !pickerCanvas) return

  viewport = document.getElementById('map-viewport')
  statusEl = panel.querySelector('.map-status')

  canvas.width  = MAP_W * CELL_PX
  canvas.height = MAP_H * CELL_PX
  ctx = canvas.getContext('2d')
  ctx.imageSmoothingEnabled = false

  overlayCanvas.width  = canvas.width
  overlayCanvas.height = canvas.height
  overlayCtx = overlayCanvas.getContext('2d')
  overlayCtx.imageSmoothingEnabled = false

  pickerCanvas.width  = PICKER_COLS * PICKER_PX
  pickerCanvas.height = PICKER_COLS * PICKER_PX
  pickerCtx = pickerCanvas.getContext('2d')
  pickerCtx.imageSmoothingEnabled = false

  tilemapCanvas = document.querySelector('#tilemap-canvas')

  loadFromStorage()

  // overlay sits on top — listen there for mouse so cursor + previews work
  overlayCanvas.addEventListener('mousedown',  onMouseDown)
  overlayCanvas.addEventListener('mousemove',  onMouseMove)
  overlayCanvas.addEventListener('mouseleave', () => {
    if (currentTool === 'stamp' && !isMouseDown) clearOverlay()
  })
  overlayCanvas.addEventListener('contextmenu', e => e.preventDefault())
  window.addEventListener('mouseup',    onMouseUp)
  pickerCanvas.addEventListener('click', onPickerClick)

  // tool buttons — user-initiated switch also wipes any in-flight preview
  TOOLS.forEach(name => {
    const el = document.getElementById('map-tool-' + name)
    if (el) el.addEventListener('click', () => { clearOverlay(); setActiveTool(name) })
  })

  // undo / redo
  document.getElementById('map-undo-button')?.addEventListener('click', undo)
  document.getElementById('map-redo-button')?.addEventListener('click', redo)
  window.addEventListener('keydown', e => {
    if (!panel.classList.contains('active')) return     // only when map tab is open

    // undo/redo with modifier
    if (e.metaKey || e.ctrlKey) {
      const k = e.key.toLowerCase()
      if (k === 'z' && !e.shiftKey)                    { e.preventDefault(); undo(); return }
      if ((k === 'z' && e.shiftKey) || k === 'y')      { e.preventDefault(); redo(); return }
      return
    }

    // selection ops (no modifier) — same letters as sprite editor
    if (!selectionRect) return
    const k = e.key.toLowerCase()
    if      (k === 'h')          { e.preventDefault(); flipSelectionH() }
    else if (k === 'v')          { e.preventDefault(); flipSelectionV() }
    else if (k === 'r')          { e.preventDefault(); rotateSelection() }
    else if (e.key === 'ArrowLeft')  { e.preventDefault(); shiftSelection(-1,  0) }
    else if (e.key === 'ArrowRight') { e.preventDefault(); shiftSelection( 1,  0) }
    else if (e.key === 'ArrowUp')    { e.preventDefault(); shiftSelection( 0, -1) }
    else if (e.key === 'ArrowDown')  { e.preventDefault(); shiftSelection( 0,  1) }
  })

  // sliders + size inputs
  const zoomEl  = document.getElementById('mapZoomRange')
  const brushEl = document.getElementById('mapBrushRange')
  const widthEl  = document.getElementById('mapWInput')
  const heightEl = document.getElementById('mapHInput')
  if (zoomEl) {
    zoomEl.value = CELL_PX / SPRITE_SIZE
    zoomEl.addEventListener('input', e => setZoom(parseInt(e.target.value, 10)))
  }
  const brushInputEl = document.getElementById('mapBrushInput')
  if (brushEl) {
    brushEl.value = brushSize
    brushEl.addEventListener('input', e => {
      setBrush(parseInt(e.target.value, 10))
      if (brushInputEl) brushInputEl.value = brushSize
    })
  }
  if (brushInputEl) {
    brushInputEl.value = brushSize
    brushInputEl.addEventListener('change', e => {
      setBrush(parseInt(e.target.value, 10))
      brushInputEl.value = brushSize
      if (brushEl) brushEl.value = brushSize
    })
  }
  if (widthEl) {
    widthEl.value = MAP_W
    widthEl.addEventListener('change', e => {
      setMapSize(parseInt(e.target.value, 10), MAP_H)
      widthEl.value = MAP_W   // reflect clamping
    })
  }
  if (heightEl) {
    heightEl.value = MAP_H
    heightEl.addEventListener('change', e => {
      setMapSize(MAP_W, parseInt(e.target.value, 10))
      heightEl.value = MAP_H
    })
  }

  // refresh from spritesheet when entering the map tab — sprites may have changed
  document.querySelector('.tab[data-tab="map"]')?.addEventListener('click', () => {
    setTimeout(() => {
      tilemapCanvas = document.querySelector('#tilemap-canvas')
      renderAll()
      renderPicker()
    }, 0)
  })

  renderAll()
  renderPicker()
  updateStatus()
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init)
} else {
  init()
}

export function getMapBytes() {
  return mapData
}
