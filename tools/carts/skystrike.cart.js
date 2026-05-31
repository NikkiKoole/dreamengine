// SKY STRIKE — sprites + scrolling sea/island/airbase map.
// Sprites are generated as flat 16×16 index arrays (reaches the magic body index 28
// used for pal()-recolored enemy squadrons, like crowd.cart.js). Craft are drawn on
// the left half and mirrored so they stay symmetric.

// ---- 16×16 canvas helpers -------------------------------------------------
function G()  { return new Array(256).fill(0) }
function P(g, x, y, c) { if (x >= 0 && x < 16 && y >= 0 && y < 16) g[y * 16 + x] = c }
function R(g, x0, y0, x1, y1, c) { for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) P(g, x, y, c) }
function mirror(g) { for (let y = 0; y < 16; y++) for (let x = 0; x < 8; x++) g[y * 16 + (15 - x)] = g[y * 16 + x] }

const M = 28   // magic body index — swapped per enemy with pal()
const D = 16   // dark outline / plating (CLR_BROWNISH_BLACK)

// player jet (slot 0) — points UP, fixed grey/blue colors
function player() {
  const g = G()
  R(g, 6, 1, 7, 13, 6)      // fuselage
  R(g, 7, 0, 7, 1, 7)       // nose tip
  R(g, 6, 2, 6, 9, 7)       // highlight stripe
  R(g, 7, 3, 7, 5, 12)      // cockpit
  R(g, 1, 7, 7, 8, 6)       // main wing
  R(g, 0, 7, 1, 8, 5)       // wingtip
  R(g, 4, 11, 7, 12, 6)     // tail wing
  R(g, 4, 11, 4, 11, 5)
  R(g, 6, 13, 7, 13, 5)     // engine
  R(g, 6, 14, 7, 14, 9)     // flame
  R(g, 7, 15, 7, 15, 10)
  mirror(g); return g
}

// popcorn flyer (slot 3) — small, points DOWN, magic body
function popcorn() {
  const g = G()
  R(g, 6, 3, 7, 12, M)
  R(g, 7, 12, 7, 13, M)     // nose
  R(g, 7, 5, 7, 7, 12)      // cockpit
  R(g, 3, 5, 7, 7, M)       // wing
  R(g, 2, 6, 3, 7, D)       // wingtip
  R(g, 6, 2, 7, 3, D)       // tail
  mirror(g); return g
}

// popper (slot 4) — fires aimed shots, gun nubs
function popper() {
  const g = G()
  R(g, 5, 3, 7, 12, M)
  R(g, 6, 12, 7, 14, M)
  R(g, 7, 5, 7, 7, 12)
  R(g, 1, 7, 7, 9, M)       // wing
  R(g, 2, 9, 3, 12, 8)      // gun nub (red)
  R(g, 6, 2, 7, 3, D)
  mirror(g); return g
}

// gunship (slot 5) — heavy, spread fire
function gunship() {
  const g = G()
  R(g, 4, 2, 7, 13, M)
  R(g, 0, 6, 7, 10, M)      // wide wing
  R(g, 6, 4, 7, 7, 12)      // cockpit
  R(g, 2, 8, 3, 10, 8)      // weapon cores
  R(g, 6, 13, 7, 15, M)     // nose
  R(g, 5, 1, 7, 2, D)
  R(g, 0, 6, 1, 10, D)      // wing edge
  mirror(g); return g
}

// ground turret (slot 6) — rides the scroll, barrel points down
function turret() {
  const g = G()
  R(g, 3, 4, 7, 12, 5)      // base
  R(g, 4, 5, 7, 11, D)      // ring
  R(g, 5, 6, 7, 10, M)      // dome (recolored)
  R(g, 7, 7, 7, 9, 8)       // core
  R(g, 6, 11, 7, 15, 5)     // barrel
  mirror(g); return g
}

// ---- 32×32 boss across slots 8,9,10,11 -----------------------------------
function G32() { return new Array(1024).fill(0) }
function P32(g, x, y, c) { if (x >= 0 && x < 32 && y >= 0 && y < 32) g[y * 32 + x] = c }
function R32(g, x0, y0, x1, y1, c) { for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) P32(g, x, y, c) }
function mirror32(g) { for (let y = 0; y < 32; y++) for (let x = 0; x < 16; x++) g[y * 32 + (31 - x)] = g[y * 32 + x] }

function bossSheet() {
  const g = G32()
  R32(g, 4, 2, 15, 24, M)       // hull
  R32(g, 0, 8, 15, 15, M)       // wing
  R32(g, 0, 8, 1, 15, D)        // wingtip plating
  R32(g, 0, 11, 15, 11, D)      // plating lines
  R32(g, 4, 18, 15, 18, D)
  R32(g, 11, 4, 15, 9, 12)      // bridge / cockpit
  R32(g, 7, 13, 10, 17, 8)      // side weak core
  R32(g, 13, 19, 15, 23, 8)     // center core
  R32(g, 5, 24, 8, 29, 5)       // side cannon
  R32(g, 13, 24, 15, 30, 5)     // center cannon
  R32(g, 12, 0, 15, 1, 9)       // engine glow
  mirror32(g)
  // split into four 16×16 slots
  const slot = (cx, cy) => {
    const s = G()
    for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) s[y * 16 + x] = g[(cy + y) * 32 + (cx + x)]
    return s
  }
  return { 8: slot(0, 0), 9: slot(16, 0), 10: slot(0, 16), 11: slot(16, 16) }
}

// ---- map tiles ------------------------------------------------------------
function oceanA() {
  const g = G(); R(g, 0, 0, 15, 15, 1)         // deep blue base (never index 0 → no transparency)
  R(g, 2, 3, 5, 3, 12); R(g, 9, 7, 12, 7, 12); R(g, 4, 11, 7, 11, 12); R(g, 11, 13, 14, 13, 12)
  R(g, 1, 8, 3, 8, 17); R(g, 8, 2, 10, 2, 17)
  return g
}
function oceanB() {
  const g = oceanA()
  R(g, 5, 5, 8, 5, 6); R(g, 6, 5, 7, 5, 7); R(g, 2, 12, 4, 12, 6)   // whitecaps
  return g
}
function sand() {
  const g = G(); R(g, 0, 0, 15, 15, 15)
  R(g, 2, 2, 3, 3, 4); R(g, 9, 5, 10, 6, 4); R(g, 5, 11, 6, 12, 4); R(g, 12, 10, 13, 11, 4); R(g, 7, 8, 8, 8, 9)
  return g
}
function grass() {
  const g = G(); R(g, 0, 0, 15, 15, 3)
  R(g, 2, 3, 3, 3, 11); R(g, 8, 6, 9, 6, 11); R(g, 5, 11, 6, 11, 11); R(g, 11, 9, 12, 9, 4); R(g, 13, 2, 13, 2, 11)
  return g
}
function runway() {
  const g = G(); R(g, 0, 0, 15, 15, 5)
  R(g, 0, 0, 1, 15, 6); R(g, 14, 0, 15, 15, 6)                       // edges
  R(g, 7, 1, 8, 4, 10); R(g, 7, 7, 8, 10, 10); R(g, 7, 13, 8, 15, 10) // dashed center stripe
  return g
}

// ---- build the scrolling map (16 wide × 40 tall) --------------------------
const MW = 16, MH = 40
function buildLayout() {
  const grid = []
  for (let y = 0; y < MH; y++) grid.push(new Array(MW).fill('~'))
  const caps = [[3, 4], [11, 6], [14, 9], [6, 15], [13, 18], [2, 25], [9, 33], [5, 30]]
  for (const [x, y] of caps) grid[y][x] = 'w'
  const island = (cx, cy, rg, rs) => {
    for (let y = 0; y < MH; y++) for (let x = 0; x < MW; x++) {
      const d = Math.hypot(x - cx, y - cy)
      if (d <= rs) grid[y][x] = 's'
      if (d <= rg) grid[y][x] = 'g'
    }
  }
  island(4, 11, 1.7, 3.0)
  island(11, 24, 1.9, 3.2)
  for (let y = 29; y <= 35; y++) for (let x = 5; x <= 10; x++) grid[y][x] = 's'   // airbase apron
  for (let y = 30; y <= 34; y++) for (let x = 6; x <= 9; x++) grid[y][x] = 'r'    // runway
  return grid.map(r => r.join(''))
}

module.exports = {
  screenW: 240, screenH: 320, scale: 3,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: Object.assign(
    { 0: player(), 3: popcorn(), 4: popper(), 5: gunship(), 6: turret(),
      40: oceanA(), 41: oceanB(), 42: sand(), 43: grass(), 44: runway() },
    bossSheet()
  ),
  map: { layout: buildLayout(), tiles: { '~': 40, 'w': 41, 's': 42, 'g': 43, 'r': 44 } },
}
