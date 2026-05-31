// config for finalfight.c — a belt-scroll beat-'em-up "brawler engine".
//
// Characters are chunky 16×32 side-view fighters, each split into a TOP slot
// (head + torso + arms) and a BOTTOM slot (hips + legs) so the cart can mix &
// match poses (idle torso + walking legs, punch arm + lunge legs, …).
//
// All characters share ONE sprite set. Enemy variety comes entirely from pal()
// "magic colors" at runtime: every sprite paints SKIN in index 30, SHIRT in 28,
// PANTS in 29 and HAIR in 26 — placeholders the cart swaps per fighter so the
// hero, grunts, knife-thugs, the bruiser and the boss are all recolors of the
// same art (the crowd.c trick). The boss is the same art drawn scaled up.
//
// Slot layout (8-col sheet):
//   TOPS  (row 0):  0 idle   1 punch   2 hurt   3 grab   4 kick
//   BOTS  (row 1):  8 stand  9 walkA  10 walkB  11 lunge 12 kick-legs
//   TILES        : 16 road 17 sidewalk 18 brick 19 win-lit 20 win-dark
//                  21 door 22 metal-wall 23 vent 24 concrete 25 girder

const T = 0          // transparent (cart calls colorkey(0))
const OUT  = 16      // outline — brownish-black, reads near-black, never index 0
const SKIN = 30      // MAGIC: pal(30, skinColor)
const SHIRT= 28      // MAGIC: pal(28, shirtColor)
const PANTS= 29      // MAGIC: pal(29, pantsColor)
const HAIR = 26      // MAGIC: pal(26, hairColor)
const SHOE = 5       // fixed dark grey
const WHT  = 7

// ── tiny pixel-canvas helpers ─────────────────────────────────
const blank = () => Array.from({ length: 16 }, () => new Array(16).fill(T))
function box(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      if (x >= 0 && x < 16 && y >= 0 && y < 16) g[y][x] = c
}
function dot(g, x, y, c) { if (x >= 0 && x < 16 && y >= 0 && y < 16) g[y][x] = c }

// add a 1px OUT outline into transparent pixels touching the silhouette.
// run per-slot — torso is flush to the slot edge so the waist seam stays clean.
function outlined(g) {
  const o = g.map(r => r.slice())
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      if (g[y][x] !== T) continue
      let touch = false
      for (let dy = -1; dy <= 1 && !touch; dy++)
        for (let dx = -1; dx <= 1; dx++) {
          const ny = y + dy, nx = x + dx
          if (ny >= 0 && ny < 16 && nx >= 0 && nx < 16 &&
              g[ny][nx] !== T && g[ny][nx] !== OUT) { touch = true; break }
        }
      if (touch) o[y][x] = OUT
    }
  return o
}
const flat = g => outlined(g).flat()
const flatRaw = g => g.flat()

// ── character TOPS (head + torso + arms), facing RIGHT ───────────────
function headTorso(g) {
  box(g, 5, 1, 11, 2, HAIR)     // hair cap
  box(g, 5, 2, 6, 6, HAIR)      // back of head
  box(g, 7, 2, 11, 6, SKIN)     // face (front = right)
  dot(g, 10, 3, WHT); dot(g, 11, 4, OUT)   // eye + brow
  dot(g, 11, 5, OUT)            // mouth/jaw hint
  box(g, 5, 7, 11, 8, SHIRT)    // shoulders
  box(g, 6, 9, 11, 15, SHIRT)   // chest → waist (flush bottom row 15 = clean seam)
}
function topIdle() {
  const g = blank(); headTorso(g)
  box(g, 4, 8, 5, 12, SHIRT); dot(g, 4, 12, SKIN); dot(g, 4, 13, SKIN) // back arm
  box(g, 11, 9, 12, 13, SHIRT); box(g, 11, 13, 12, 14, SKIN)           // front arm
  return flat(g)
}
function topPunch() {
  const g = blank(); headTorso(g)
  box(g, 5, 9, 6, 11, SHIRT)                         // back arm cocked
  box(g, 11, 9, 14, 10, SHIRT); box(g, 13, 9, 15, 11, SKIN) // straight punch + big fist
  return flat(g)
}
function topHurt() {
  const g = blank()
  // head snapped back (shifted left/up)
  box(g, 5, 0, 10, 1, HAIR); box(g, 5, 1, 6, 5, HAIR)
  box(g, 6, 1, 10, 5, SKIN); dot(g, 9, 2, OUT)
  box(g, 5, 6, 11, 8, SHIRT); box(g, 6, 9, 11, 15, SHIRT)
  box(g, 3, 5, 5, 7, SHIRT); dot(g, 3, 7, SKIN)      // arms flung up
  box(g, 11, 5, 13, 7, SHIRT); dot(g, 13, 7, SKIN)
  return flat(g)
}
function topGrab() {
  const g = blank(); headTorso(g)
  box(g, 11, 8, 15, 9, SHIRT); box(g, 14, 8, 15, 9, SKIN)   // both arms forward
  box(g, 11, 11, 15, 12, SHIRT); box(g, 14, 11, 15, 12, SKIN)
  return flat(g)
}
function topKick() {
  const g = blank(); headTorso(g)
  box(g, 3, 7, 5, 9, SHIRT); dot(g, 3, 9, SKIN)      // back arm out for balance
  box(g, 10, 11, 11, 13, SHIRT); dot(g, 10, 13, SKIN)
  return flat(g)
}

// ── character BOTTOMS (hips + legs), local y 0..15 ───────────────────
function botStand() {
  const g = blank()
  box(g, 6, 0, 11, 3, PANTS)                 // pelvis (flush top)
  box(g, 6, 3, 8, 12, PANTS); box(g, 6, 13, 9, 15, SHOE)   // back leg + shoe
  box(g, 9, 3, 11, 12, PANTS); box(g, 9, 13, 12, 15, SHOE) // front leg + shoe
  return flat(g)
}
function botWalkA() {
  const g = blank()
  box(g, 6, 0, 11, 3, PANTS)
  box(g, 4, 4, 6, 12, PANTS); box(g, 3, 13, 6, 15, SHOE)   // back leg trailing
  box(g, 10, 4, 12, 11, PANTS); box(g, 11, 12, 14, 14, SHOE) // front leg leading
  return flat(g)
}
function botWalkB() {
  const g = blank()
  box(g, 6, 0, 11, 3, PANTS)
  box(g, 7, 3, 9, 13, PANTS); box(g, 6, 14, 9, 15, SHOE)   // legs passing center
  box(g, 9, 3, 11, 13, PANTS); box(g, 9, 14, 12, 15, SHOE)
  return flat(g)
}
function botLunge() {
  const g = blank()
  box(g, 5, 0, 10, 3, PANTS)
  box(g, 3, 4, 5, 12, PANTS); box(g, 2, 13, 5, 15, SHOE)   // back foot planted wide
  box(g, 10, 4, 13, 11, PANTS); box(g, 11, 12, 15, 14, SHOE) // front foot lunged
  return flat(g)
}
function botKick() {
  const g = blank()
  box(g, 4, 0, 9, 4, PANTS)
  box(g, 5, 4, 7, 13, PANTS); box(g, 4, 14, 7, 15, SHOE)   // support leg
  box(g, 8, 3, 14, 6, PANTS); box(g, 13, 3, 15, 6, SHOE)   // kicking leg out to the right
  return flat(g)
}

// ── tiles (16×16, opaque — avoid index 0 so nothing turns transparent) ──
function brick() {
  const g = blank(); box(g, 0, 0, 15, 15, 4)              // brown base
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      if (y % 5 === 0) g[y][x] = 20                        // mortar line (darker)
      else if (((Math.floor(y / 5) % 2) ? x + 4 : x) % 8 === 0) g[y][x] = 20
      else if ((x * 3 + y * 7) % 11 === 0) g[y][x] = 24    // fleck
    }
  return flatRaw(g)
}
function winLit() {
  const g = blank(); box(g, 0, 0, 15, 15, 4)
  box(g, 2, 2, 13, 13, 9); box(g, 3, 3, 12, 12, 10)        // glowing window
  box(g, 7, 2, 8, 13, 4); box(g, 2, 7, 13, 8, 4)           // frame cross
  return flatRaw(g)
}
function winDark() {
  const g = blank(); box(g, 0, 0, 15, 15, 4)
  box(g, 2, 2, 13, 13, 1); box(g, 3, 3, 12, 12, 17)
  box(g, 7, 2, 8, 13, 4); box(g, 2, 7, 13, 8, 4)
  return flatRaw(g)
}
function door() {
  const g = blank(); box(g, 0, 0, 15, 15, 4)
  box(g, 3, 1, 12, 15, 16); box(g, 4, 2, 11, 15, 1)        // dark doorway
  dot(g, 10, 8, 10)                                        // knob
  return flatRaw(g)
}
function road() {
  const g = blank(); box(g, 0, 0, 15, 15, 5)               // asphalt grey
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = (x * 7 + y * 13) % 17
      if (n === 0) g[y][x] = 21
      else if (n === 5) g[y][x] = 16
    }
  return flatRaw(g)
}
function sidewalk() {
  const g = blank(); box(g, 0, 0, 15, 15, 6)               // light grey kerb
  box(g, 0, 0, 15, 1, 22)                                  // top edge highlight
  for (let x = 0; x < 16; x += 8) box(g, x, 2, x, 15, 5)   // expansion joints
  return flatRaw(g)
}
function metalWall() {
  const g = blank(); box(g, 0, 0, 15, 15, 13)              // indigo steel
  for (let x = 0; x < 16; x += 3) box(g, x, 0, x, 15, 29)  // corrugation ribs
  box(g, 0, 0, 15, 0, 22)
  return flatRaw(g)
}
function vent() {
  const g = blank(); box(g, 0, 0, 15, 15, 13)
  box(g, 2, 3, 13, 12, 16); for (let y = 4; y < 12; y += 2) box(g, 3, y, 12, y, 5) // louvres
  return flatRaw(g)
}
function concrete() {
  const g = blank(); box(g, 0, 0, 15, 15, 21)              // dark warm grey
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if ((x * 5 + y * 11) % 13 === 0) g[y][x] = 16
  box(g, 0, 0, 15, 0, 5)
  return flatRaw(g)
}
function girder() {
  const g = blank(); box(g, 0, 0, 15, 15, 16)
  box(g, 0, 5, 15, 10, 9); box(g, 0, 6, 15, 9, 25)         // orange I-beam
  for (let x = 2; x < 16; x += 5) { dot(g, x, 7, 16); dot(g, x, 8, 16) } // rivets
  return flatRaw(g)
}

// ── build the long scrolling world (80 × 13 cells = 1280 px wide) ─────
const MW = 80, MH = 13
const layout = []
for (let y = 0; y < MH; y++) layout.push(new Array(MW).fill(' '))

function set(x, y, ch) { if (x >= 0 && x < MW && y >= 0 && y < MH) layout[y][x] = ch }

for (let x = 0; x < MW; x++) {
  const warehouse = x >= 42
  // facade rows 1..6
  for (let y = 1; y <= 6; y++) {
    if (warehouse) {
      set(x, y, y === 1 ? 'g' : (x % 6 === 3 && (y === 3 || y === 5) ? 'v' : 'm'))
    } else {
      let ch = 'b'
      if ((y === 3 || y === 5) && x % 3 === 0) ch = (x % 6 === 0) ? 'w' : 'd'
      if (y === 6 && x % 11 === 4) ch = 'D'               // street-level door
      set(x, y, ch)
    }
  }
  set(x, 7, warehouse ? 'm' : 'b')                         // facade base
  set(x, 8, warehouse ? 'c' : 's')                         // kerb / dock edge
  for (let y = 9; y < MH; y++) set(x, y, warehouse ? 'c' : 'r') // floor band
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: {
    0: topIdle(), 1: topPunch(), 2: topHurt(), 3: topGrab(), 4: topKick(),
    8: botStand(), 9: botWalkA(), 10: botWalkB(), 11: botLunge(), 12: botKick(),
    16: road(), 17: sidewalk(), 18: brick(), 19: winLit(), 20: winDark(),
    21: door(), 22: metalWall(), 23: vent(), 24: concrete(), 25: girder(),
  },
  map: {
    layout: layout.map(r => r.join('')),
    tiles: { b: 18, w: 19, d: 20, D: 21, s: 17, r: 16, m: 22, v: 23, c: 24, g: 25 },
  },
}
