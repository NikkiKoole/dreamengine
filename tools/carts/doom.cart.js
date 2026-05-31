// Sprites for doom.c — all generated programmatically so we can reach the
// extended palette (indices 16–31) for dark outlines / dark-red gore, and build
// the 32×16 weapon view-models out of two adjacent 16×16 slots.
//
// Slot layout (8-col sheet):
//   0,1 imp walk · 2 imp attack · 3 imp gib
//   4,5 gunner walk · 6 gunner attack · 7 gunner gib
//   8 keycard · 9 medkit · 10 armor · 11 ammo
//   12 face ok · 13 face hurt
//   16,17 pistol idle · 18,19 pistol fire   (each gun = two slots, sspr'd as 32×16)
//   24,25 shotgun idle · 26,27 shotgun fire

const blank16 = () => Array.from({ length: 16 }, () => new Array(16).fill(0))
const blank32 = () => Array.from({ length: 16 }, () => new Array(32).fill(0))

function box(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      if (x >= 0 && x < g[0].length && y >= 0 && y < g.length) g[y][x] = c
}
const dot = (g, x, y, c) => { if (x >= 0 && x < g[0].length && y >= 0 && y < g.length) g[y][x] = c }
const flat = g => g.flat()

// 1-px dark outline (index 16) into transparent pixels touching the silhouette
function outlined(g) {
  const h = g.length, w = g[0].length, o = g.map(r => r.slice())
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      if (g[y][x] !== 0) continue
      let touch = false
      for (let dy = -1; dy <= 1 && !touch; dy++)
        for (let dx = -1; dx <= 1; dx++) {
          const ny = y + dy, nx = x + dx
          if (ny >= 0 && ny < h && nx >= 0 && nx < w && g[ny][nx] !== 0 && g[ny][nx] !== 16) { touch = true; break }
        }
      if (touch) o[y][x] = 16
    }
  return o
}
// split a 16-row × 32-col grid into two flat 256-int slots (left / right halves)
function split32(g) {
  const L = [], R = []
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) { L.push(g[y][x]); R.push(g[y][x + 16]) }
  return [L, R]
}

// ── imp (red horned demon) ──────────────────────────────────────────────────
function imp(leg) {
  const g = blank16(), lf = leg ? 13 : 15, rf = leg ? 15 : 13
  box(g, 5, 12, 6, lf, 24); box(g, 9, 12, 10, rf, 24)   // legs
  box(g, 4, 6, 11, 12, 8); box(g, 6, 8, 9, 11, 24)      // torso + belly
  box(g, 3, 7, 4, 11, 8); box(g, 11, 7, 12, 11, 8)      // arms
  dot(g, 3, 11, 15); dot(g, 12, 11, 15)                 // claws
  box(g, 5, 2, 10, 6, 8)                                // head
  dot(g, 4, 1, 15); dot(g, 11, 1, 15); dot(g, 4, 0, 15); dot(g, 11, 0, 15)  // horns
  dot(g, 6, 4, 10); dot(g, 9, 4, 10)                    // eyes
  box(g, 6, 5, 9, 5, 16); dot(g, 7, 6, 7); dot(g, 8, 6, 7)  // mouth + fangs
  return flat(outlined(g))
}
function impAtk() {
  const g = blank16()
  box(g, 5, 12, 6, 15, 24); box(g, 9, 12, 10, 15, 24)
  box(g, 4, 6, 11, 12, 8); box(g, 6, 8, 9, 11, 24)
  box(g, 3, 2, 4, 7, 8); box(g, 11, 2, 12, 7, 8)        // arms RAISED
  dot(g, 3, 1, 15); dot(g, 12, 1, 15)
  box(g, 5, 2, 10, 6, 8)
  dot(g, 4, 1, 15); dot(g, 11, 1, 15)
  dot(g, 6, 4, 10); dot(g, 9, 4, 10)
  box(g, 6, 5, 9, 7, 16); dot(g, 6, 5, 7); dot(g, 9, 5, 7)  // big open maw
  return flat(outlined(g))
}
function impDead() {
  const g = blank16()
  box(g, 3, 12, 12, 15, 24); box(g, 4, 13, 11, 15, 8)
  dot(g, 5, 12, 8); dot(g, 8, 12, 8); dot(g, 10, 13, 15); dot(g, 3, 14, 8); dot(g, 12, 14, 8)
  return flat(g)
}

// ── gunner (green armored soldier) ────────────────────────────────────────────
function gunner(leg) {
  const g = blank16(), lf = leg ? 13 : 15, rf = leg ? 15 : 13
  box(g, 5, 12, 6, lf, 3); box(g, 9, 12, 10, rf, 3)
  box(g, 4, 6, 11, 12, 11); box(g, 5, 8, 10, 11, 3)
  box(g, 3, 7, 4, 10, 11)
  box(g, 6, 2, 9, 6, 15); box(g, 5, 1, 10, 2, 3)        // head + helmet
  dot(g, 6, 4, 8); dot(g, 9, 4, 8)                      // eyes
  box(g, 10, 8, 13, 9, 5); dot(g, 13, 8, 16)            // gun, barrel toward you
  return flat(outlined(g))
}
function gunnerAtk() {
  const g = blank16()
  box(g, 5, 12, 6, 15, 3); box(g, 9, 12, 10, 15, 3)
  box(g, 4, 6, 11, 12, 11); box(g, 5, 8, 10, 11, 3)
  box(g, 3, 7, 4, 10, 11)
  box(g, 6, 2, 9, 6, 15); box(g, 5, 1, 10, 2, 3)
  dot(g, 6, 4, 8); dot(g, 9, 4, 8)
  box(g, 10, 8, 13, 9, 5)
  dot(g, 14, 8, 10); dot(g, 15, 8, 7); dot(g, 14, 7, 9); dot(g, 14, 9, 9); dot(g, 15, 9, 10)  // muzzle flash
  return flat(outlined(g))
}
function gunnerDead() {
  const g = blank16()
  box(g, 3, 12, 12, 15, 3); box(g, 4, 13, 11, 15, 11)
  dot(g, 5, 12, 11); dot(g, 9, 12, 11); dot(g, 4, 14, 8); dot(g, 11, 13, 5)
  return flat(g)
}

// ── pickups ───────────────────────────────────────────────────────────────────
function keycard() {
  const g = blank16()
  box(g, 4, 5, 11, 11, 8); box(g, 5, 6, 10, 7, 7); box(g, 5, 9, 10, 9, 24); dot(g, 11, 8, 16)
  return flat(outlined(g))
}
function medkit() {
  const g = blank16()
  box(g, 3, 4, 12, 12, 7); box(g, 3, 4, 12, 5, 6)
  box(g, 7, 6, 8, 11, 8); box(g, 5, 8, 10, 9, 8)        // red cross
  return flat(outlined(g))
}
function armorPack() {
  const g = blank16()
  box(g, 5, 4, 10, 11, 12); box(g, 6, 4, 9, 5, 7); box(g, 6, 7, 9, 9, 28)
  box(g, 4, 5, 4, 9, 12); box(g, 11, 5, 11, 9, 12); dot(g, 7, 12, 12); dot(g, 8, 12, 12)
  return flat(outlined(g))
}
function ammoBox() {
  const g = blank16()
  box(g, 3, 7, 12, 12, 4); box(g, 3, 7, 12, 7, 9)
  box(g, 4, 4, 5, 7, 9); dot(g, 4, 4, 10); dot(g, 5, 4, 10)     // shells
  box(g, 7, 3, 8, 7, 9); dot(g, 7, 3, 10); dot(g, 8, 3, 10)
  box(g, 10, 5, 11, 7, 9); dot(g, 10, 5, 10); dot(g, 11, 5, 10)
  return flat(outlined(g))
}

// ── HUD face ────────────────────────────────────────────────────────────────
function faceBase() {
  const g = blank16()
  box(g, 4, 3, 11, 13, 15)                              // face
  box(g, 4, 2, 11, 3, 4); box(g, 4, 2, 4, 6, 4); box(g, 11, 2, 11, 6, 4)  // hair
  box(g, 5, 6, 6, 7, 7); box(g, 9, 6, 10, 7, 7); dot(g, 6, 6, 1); dot(g, 9, 6, 1)  // eyes
  box(g, 5, 5, 6, 5, 4); box(g, 9, 5, 10, 5, 4)         // brow
  dot(g, 7, 8, 4); dot(g, 8, 8, 4)                      // nose
  return g
}
function faceOk() {
  const g = faceBase()
  box(g, 6, 11, 9, 11, 16); dot(g, 6, 10, 7); dot(g, 9, 10, 7)
  return flat(outlined(g))
}
function faceHurt() {
  const g = faceBase()
  dot(g, 5, 8, 8); dot(g, 5, 9, 8); dot(g, 10, 9, 8); dot(g, 6, 12, 8)   // blood
  box(g, 6, 11, 9, 12, 16); dot(g, 7, 11, 7); dot(g, 8, 11, 7)          // grimace
  dot(g, 6, 5, 16); dot(g, 9, 5, 16)
  return flat(outlined(g))
}

// ── weapon view-models (32×16) ─────────────────────────────────────────────────
function pistolGrid(fire) {
  const g = blank32()
  box(g, 13, 3, 19, 9, 5); box(g, 13, 3, 19, 4, 6)      // body + highlight
  box(g, 15, 0, 17, 3, 5); dot(g, 16, 0, 16)            // barrel up
  box(g, 13, 9, 18, 15, 4); box(g, 14, 9, 15, 15, 20)   // grip
  box(g, 12, 9, 13, 13, 15); box(g, 18, 9, 19, 13, 15)  // hands
  if (fire) { box(g, 14, 0, 18, 2, 10); box(g, 15, 0, 17, 1, 7); dot(g, 13, 2, 9); dot(g, 19, 2, 9) }
  return outlined(g)
}
function shotgunGrid(fire) {
  const g = blank32()
  box(g, 9, 4, 23, 8, 5); box(g, 9, 4, 23, 4, 6); box(g, 9, 6, 23, 6, 16)  // twin barrels
  box(g, 11, 8, 20, 10, 16)                             // pump
  box(g, 12, 10, 20, 15, 4); box(g, 13, 11, 15, 15, 20) // stock
  box(g, 10, 9, 11, 14, 15); box(g, 21, 9, 22, 14, 15)  // hands
  if (fire) {
    box(g, 7, 2, 13, 5, 10); box(g, 8, 3, 12, 4, 7)
    box(g, 19, 2, 25, 5, 10); box(g, 20, 3, 24, 4, 7); dot(g, 16, 1, 9)
  }
  return outlined(g)
}

const [pIdleL, pIdleR] = split32(pistolGrid(false))
const [pFireL, pFireR] = split32(pistolGrid(true))
const [sIdleL, sIdleR] = split32(shotgunGrid(false))
const [sFireL, sFireR] = split32(shotgunGrid(true))

module.exports = {
  screenW: 320, screenH: 200, scale: 3, cellW: 16, cellH: 16,
  sprites: {
    0: imp(false), 1: imp(true), 2: impAtk(), 3: impDead(),
    4: gunner(false), 5: gunner(true), 6: gunnerAtk(), 7: gunnerDead(),
    8: keycard(), 9: medkit(), 10: armorPack(), 11: ammoBox(),
    12: faceOk(), 13: faceHurt(),
    16: pIdleL, 17: pIdleR, 18: pFireL, 19: pFireR,
    24: sIdleL, 25: sIdleR, 26: sFireL, 27: sFireR,
  },
}
