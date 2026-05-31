// config for hotline.c — a top-down neon murder-floor.
//
// One body sprite (slot 0) drawn from above, facing RIGHT (+x), so the cart can
// spr_rot() it to any aim angle. Its shirt is painted in the "magic" index 28 and
// recolored per entity with pal() — white player, red guards, orange thugs — the
// crowd.c trick. Tiles 1-5 are the floor / wall / closed door / open door / exit,
// generated as flat index arrays so we can reach the dark extended palette
// (16-31) for the moody neon look. The cart pal()-tints the whole tileset per
// floor (magenta → cyan → red).

const W = 16, H = 16

// palette indices
const TRANS  = 0
const OUTL   = 16   // brownish-black outline (reads near-black, never index 0)
const SKIN   = 31   // peach
const SHIRT  = 28   // MAGIC — recolored by pal(28, color)

const blank = () => Array.from({ length: H }, () => new Array(W).fill(TRANS))
function box(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      if (x >= 0 && x < W && y >= 0 && y < H) g[y][x] = c
}
function dot(g, x, y, c) { if (x >= 0 && x < W && y >= 0 && y < H) g[y][x] = c }
function disc(g, cx, cy, r, c) {
  for (let y = 0; y < H; y++)
    for (let x = 0; x < W; x++)
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) g[y][x] = c
}
// 1px outline into transparent pixels touching the silhouette
function outlined(g) {
  const o = g.map(r => r.slice())
  for (let y = 0; y < H; y++)
    for (let x = 0; x < W; x++) {
      if (g[y][x] !== TRANS) continue
      let touch = false
      for (let dy = -1; dy <= 1 && !touch; dy++)
        for (let dx = -1; dx <= 1; dx++) {
          const ny = y + dy, nx = x + dx
          if (ny >= 0 && ny < H && nx >= 0 && nx < W &&
              g[ny][nx] !== TRANS && g[ny][nx] !== OUTL) { touch = true; break }
        }
      if (touch) o[y][x] = OUTL
    }
  return o
}
const flat = g => g.flat()

// ── top-down body, facing right ──────────────────────────────────
function makeGuy() {
  const g = blank()
  disc(g, 7, 8, 5, SHIRT)        // torso / shoulders
  box(g, 11, 7, 14, 9, SHIRT)    // arm reaching forward (toward +x)
  disc(g, 8, 8, 3, SKIN)         // head (seen from above)
  dot(g, 14, 8, SKIN)            // forward hand
  return flat(outlined(g))
}

// ── tiles (16×16, fully opaque) ──────────────────────────────────
function floorTile() {
  const g = blank()
  box(g, 0, 0, 15, 15, 18)       // darker purple base
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = (x * 7 + y * 13) % 19
      if (n === 0) g[y][x] = 2    // dark purple speck
      else if (n === 6) g[y][x] = 17 // darker blue speck
    }
  // faint tile grid lines
  for (let i = 0; i < 16; i++) { if (i % 8 === 0) { g[0][i] = 2; g[i][0] = 2 } }
  return flat(g)
}
function wallTile() {
  const g = blank()
  box(g, 0, 0, 15, 15, 2)        // dark purple
  box(g, 0, 0, 15, 1, 14)        // hot pink neon strip (top)
  box(g, 0, 14, 15, 15, 18)      // shaded bottom
  box(g, 0, 0, 0, 15, 29)        // mauve left edge
  return flat(g)
}
function doorTile() {
  const g = blank()
  box(g, 0, 0, 15, 15, 24)       // dark red slab
  box(g, 1, 1, 14, 14, 8)        // red face
  box(g, 7, 1, 8, 14, 24)        // center seam
  box(g, 0, 0, 15, 0, 16)        // top frame
  box(g, 0, 15, 15, 15, 16)      // bottom frame
  dot(g, 11, 8, 10); dot(g, 11, 9, 10) // handle
  return flat(g)
}
function doorOpenTile() {
  const g = floorTile().reduce((a, v, i) => { a[Math.floor(i / 16)][i % 16] = v; return a }, blank())
  box(g, 0, 0, 1, 15, 24)        // left post
  box(g, 14, 0, 15, 15, 24)      // right post
  return flat(g)
}
function exitTile() {
  const g = blank()
  box(g, 0, 0, 15, 15, 3)        // dark green
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if ((x + y) % 2 === 0) g[y][x] = 11 // bright green checker
  // up-chevron "exit"
  box(g, 7, 4, 8, 12, 7)
  dot(g, 6, 6, 7); dot(g, 9, 6, 7); dot(g, 5, 8, 7); dot(g, 10, 8, 7)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: 30, mapH: 18,
  sprites: {
    0: makeGuy(),
    1: floorTile(),
    2: wallTile(),
    3: doorTile(),
    4: doorOpenTile(),
    5: exitTile(),
  },
  // the map is built procedurally in hotline.c (build_floor) via mset(); no layout here.
}
