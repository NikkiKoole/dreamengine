// config for turfwar.c — sprites + city map.
//
// Members are tiny top-down people. Their SHIRT pixels are drawn in the "magic"
// palette index 29; turfwar.c calls pal(29, gangColor) before drawing each gang
// so one sprite set yields four differently-colored crews (the crowd.c idiom).
// Two near-identical frames give a subtle walk bob.
//
// Slot layout (8-col sheet):
//   0,1 — member walk frames A/B (magic shirt = 29)
//   4   — city block / building tile (opaque, 16×16) used by the map
//
// Sprites are raw index arrays (generated here) so we can use palette index 29,
// which is past the ASCII char map's 0–15 range.

const TRANS = 0    // transparent (cart calls colorkey(0))
const OUT   = 16   // outline / dark, reads as near-black (never index 0)
const SKIN  = 15   // light peach
const HAIR  = 16
const SHOE  = 16
const SHIRT = 29   // MAGIC: recolored per gang via pal(29, ...)

const blank16 = () => Array.from({ length: 16 }, () => new Array(16).fill(TRANS))
function box(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      if (x >= 0 && x < 16 && y >= 0 && y < 16) g[y][x] = c
}
function dot(g, x, y, c) { if (x >= 0 && x < 16 && y >= 0 && y < 16) g[y][x] = c }

// 1px outline into transparent pixels touching the silhouette
function outlined(g) {
  const o = g.map(r => r.slice())
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      if (g[y][x] !== TRANS) continue
      let touch = false
      for (let dy = -1; dy <= 1 && !touch; dy++)
        for (let dx = -1; dx <= 1; dx++) {
          const ny = y + dy, nx = x + dx
          if (ny >= 0 && ny < 16 && nx >= 0 && nx < 16 &&
              g[ny][nx] !== TRANS && g[ny][nx] !== OUT) { touch = true; break }
        }
      if (touch) o[y][x] = OUT
    }
  return o
}
const flat = g => g.flat()

// top-down person; `sway` flips the arm/foot offsets for the walk frame
function person(sway) {
  const g = blank16()
  box(g, 4, 6, 11, 12, SHIRT)             // shoulders / torso
  dot(g, 4, 6, TRANS); dot(g, 11, 6, TRANS)   // round the top corners
  dot(g, 4, 12, TRANS); dot(g, 11, 12, TRANS) // round the bottom corners
  if (!sway) { box(g, 3, 8, 3, 10, SHIRT); box(g, 12, 9, 12, 11, SHIRT) }   // arms
  else       { box(g, 3, 9, 3, 11, SHIRT); box(g, 12, 8, 12, 10, SHIRT) }
  box(g, 6, 3, 9, 7, SKIN)                // head
  dot(g, 6, 3, TRANS); dot(g, 9, 3, TRANS)
  box(g, 6, 2, 9, 3, HAIR)                // hair cap
  if (!sway) { dot(g, 6, 13, SHOE); dot(g, 9, 14, SHOE) }   // feet
  else       { dot(g, 6, 14, SHOE); dot(g, 9, 13, SHOE) }
  return flat(outlined(g))
}

// top-down city block — dark roof, lit top/left edges, a couple of rooftop units
function building() {
  const g = blank16()
  box(g, 0, 0, 15, 15, 5)        // dark grey roof
  box(g, 0, 0, 15, 0, 6)         // lit north edge
  box(g, 0, 0, 0, 15, 6)         // lit west edge
  box(g, 0, 15, 15, 15, 16)      // shadow south edge
  box(g, 15, 0, 15, 15, 16)      // shadow east edge
  box(g, 4, 4, 7, 7, 22)         // rooftop unit
  box(g, 9, 9, 12, 12, 21)       // rooftop unit
  dot(g, 5, 5, 7)                // vent glints
  dot(g, 11, 11, 7)
  return flat(g)
}

// ── city map: REG_COLS×REG_ROWS districts, each RW×RH cells, building in the middle
const REG_COLS = 4, REG_ROWS = 3, RW = 7, RH = 5
const MW = REG_COLS * RW, MH = REG_ROWS * RH
const layout = []
for (let y = 0; y < MH; y++) {
  let row = ''
  for (let x = 0; x < MW; x++) {
    const lx = x % RW, ly = y % RH
    const wall = (lx >= 2 && lx <= 4 && ly >= 1 && ly <= 3)   // centered 3×3 block; open streets around
    row += wall ? 'B' : '.'
  }
  layout.push(row)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: {
    0: person(false),
    1: person(true),
    4: building(),
  },
  map: {
    layout,
    tiles: { B: 4 },
  },
}
