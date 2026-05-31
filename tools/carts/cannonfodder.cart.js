// config for cannonfodder.c — sprites are generated in JS (flat arrays) so we can
// reach palette indices 28/29, the "magic" uniform + helmet colors that the cart
// recolors per team with pal() (the crowd.c trick). The map is built at runtime in
// C with mset() — three missions from one tileset — so no map block is needed here.
//
// Slot layout (8-col sheet):
//   0,1   — soldier: stand + mid-stride (top-down, ~11px tall)
//   8     — ground/grass     9 — water        10 — tree
//   11    — hut (target)     12 — gun nest    13 — exit flag
//
// Terrain greens (3/11/27) are recolored per biome in the cart (jungle→desert→snow),
// so trees/grass/canopies shift together; trunks, water and roofs stay put.

const TRANS = 0
const UNI   = 28      // MAGIC uniform  → pal(28, teamColor)
const HEL   = 29      // MAGIC helmet   → pal(29, teamColor)
const SKIN  = 15      // light peach
const BOOT  = 4       // brown
const OUT   = 16      // brownish-black outline (never reads as transparent)

// grass palette (recolored per biome)
const G_DK = 3, G_MD = 27, G_LT = 11

// ── tiny pixel canvas helpers ────────────────────────────────
const blank = () => Array.from({ length: 16 }, () => new Array(16).fill(TRANS))
function box(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      if (x >= 0 && x < 16 && y >= 0 && y < 16) g[y][x] = c
}
function dot(g, x, y, c) { if (x >= 0 && x < 16 && y >= 0 && y < 16) g[y][x] = c }
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
// deterministic per-pixel "hash" for texturing tiles
const hash = (x, y, a, b, m) => (x * a + y * b) % m

// ── soldier (top-down) ───────────────────────────────────────
function makeSoldier(step) {
  const g = blank()
  // helmet
  box(g, 5, 3, 10, 5, HEL)
  dot(g, 4, 4, HEL); dot(g, 11, 4, HEL)
  // face brim peeking out the front
  box(g, 6, 6, 9, 6, SKIN)
  // torso + arms (uniform)
  box(g, 4, 7, 11, 11, UNI)
  box(g, 3, 8, 3, 10, UNI)
  box(g, 12, 8, 12, 10, UNI)
  // legs + boots, staggered for the stride
  if (step === 0) {
    box(g, 5, 12, 6, 13, UNI); box(g, 5, 14, 6, 14, BOOT)
    box(g, 9, 12, 10, 13, UNI); box(g, 9, 14, 10, 14, BOOT)
  } else {
    box(g, 5, 12, 6, 14, UNI); box(g, 5, 14, 6, 14, BOOT) // planted foot
    box(g, 9, 12, 10, 12, UNI); box(g, 9, 13, 10, 13, BOOT) // raised foot
  }
  return flat(outlined(g))
}

// ── tiles (all opaque — they include a ground base so nothing turns transparent) ──
function grass(g) {
  box(g, 0, 0, 15, 15, G_DK)
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = hash(x, y, 7, 13, 17)
      if (n === 0) g[y][x] = G_LT
      else if (n === 5) g[y][x] = G_MD
    }
}
function groundTile() { const g = blank(); grass(g); return flat(g) }

function waterTile() {
  const g = blank()
  box(g, 0, 0, 15, 15, 1)            // dark blue
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = hash(x, y, 5, 9, 11)
      if (n === 0) g[y][x] = 12       // bright blue ripple
      else if (n === 4) g[y][x] = 17  // darker deep
    }
  return flat(g)
}

function treeTile() {
  const g = blank(); grass(g)
  box(g, 7, 9, 8, 13, BOOT)          // trunk
  // canopy (grass greens so it recolors with the biome)
  box(g, 4, 2, 11, 7, G_DK)
  box(g, 5, 1, 10, 1, G_DK)
  box(g, 3, 4, 3, 6, G_DK); box(g, 12, 4, 12, 6, G_DK)
  for (let y = 1; y < 8; y++)
    for (let x = 3; x < 13; x++)
      if (g[y][x] === G_DK && hash(x, y, 7, 5, 6) === 0) g[y][x] = G_LT
  return flat(g)
}

function hutTile() {
  const g = blank(); grass(g)
  box(g, 3, 7, 12, 14, 4)            // walls (brown)
  box(g, 3, 7, 12, 7, 20)           // shadow line under roof
  box(g, 2, 3, 13, 7, 8)            // red roof
  box(g, 1, 6, 14, 7, 24)           // dark roof eave
  box(g, 6, 10, 9, 14, 20)          // doorway
  dot(g, 4, 9, 0); dot(g, 11, 9, 0) // windows
  return flat(g)
}

function nestTile() {
  const g = blank(); grass(g)
  // sandbag ring
  box(g, 2, 4, 13, 13, 22)
  box(g, 4, 6, 11, 11, 6)
  for (let y = 4; y <= 13; y++)
    for (let x = 2; x <= 13; x++)
      if (g[y][x] === 22 && hash(x, y, 3, 7, 4) === 0) g[y][x] = 20
  box(g, 6, 7, 9, 10, 5)            // dark emplacement
  box(g, 9, 8, 13, 9, 5)            // the gun barrel poking out
  return flat(g)
}

function exitTile() {
  const g = blank(); grass(g)
  box(g, 0, 0, 15, 15, G_DK)
  // a marked landing zone + flag
  box(g, 2, 2, 13, 13, G_MD)
  for (let i = 0; i < 16; i++) { dot(g, i, 0, 10); dot(g, i, 15, 10) } // edge stripe
  box(g, 7, 4, 7, 13, 6)           // flag pole
  box(g, 8, 4, 12, 7, 10)          // yellow flag
  dot(g, 8, 5, 9); dot(g, 11, 6, 9)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: {
    0:  makeSoldier(0),
    1:  makeSoldier(1),
    8:  groundTile(),
    9:  waterTile(),
    10: treeTile(),
    11: hutTile(),
    12: nestTile(),
    13: exitTile(),
  },
}
