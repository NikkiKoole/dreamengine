// monstermix.cart.js — sprites for the Monster Mix Lab.
//
// THE sprite-draw.js showcase: nine hand-drawn PARTS (3 heads, 3 bodies,
// 3 legs) are stamp()-composited into all 27 combinations at bake time —
// the same workflow as the editor's stamp tool, but programmatic. Bodies
// use the magic pal() indices (28 body / 29 accent) so the cart recolors
// every combo into 4 color schemes at draw time: 108 distinct monsters
// from 9 drawn parts.
//
// Slot layout:
//   0–26   monster combos (slot = head*9 + body*3 + legs)
//   28–29  the mixer machine — one 32×16 canvas split() across two slots
//   30     prize star — concave polyfill, custom outline color
//   40/41  floor/wall map tiles — noise() speckle

const { blank, pixel, rectfill, rrectfill, circlefill, ovalfill,
        trifill, polyfill, ngonfill, noise, outlined, mirror, stamp,
        flat, split } = require('../sprite-draw.js')

const M = 28   // magic body color — cart swaps it with pal(28, …)
const A = 29   // magic accent color — pal(29, darker …)
const EYE = 7, PUP = 1   // white eyes, dark-blue pupils (never 0: colorkey would punch holes)

// ── heads (16×8) — draw the left side, mirror() makes it symmetric ──────────

function headRound() {           // cyclops with little horns
  const g = blank(16, 8)
  trifill(g, 2, 5, 4, 0, 6, 5, A)        // left horn
  mirror(g)                              // → right horn
  ovalfill(g, 8, 5, 5.5, 3, M)           // round noggin over the horn bases
  circlefill(g, 8, 5, 2.1, EYE)          // one big eye
  pixel(g, 7, 5, PUP); pixel(g, 8, 5, PUP)
  return g
}

function headBoxy() {            // square robot head with antenna
  const g = blank(16, 8)
  rectfill(g, 7, 0, 8, 1, A)             // antenna stalk
  pixel(g, 7, 0, 10); pixel(g, 8, 0, 10) // yellow bulb
  rrectfill(g, 3, 2, 10, 6, 2, M)        // the head box
  rectfill(g, 5, 4, 6, 5, EYE); pixel(g, 6, 5, PUP)
  rectfill(g, 9, 4, 10, 5, EYE); pixel(g, 9, 5, PUP)
  return g
}

function headSpiky() {           // hex head with a mohawk of spikes
  const g = blank(16, 8)
  trifill(g, 2, 5, 4, 1, 6, 5, A)        // left spike
  trifill(g, 6, 4, 8, 0, 10, 4, A)       // center spike (symmetric about x=8)
  mirror(g)                              // → right spike
  ngonfill(g, 8, 6, 4.8, 6, 0, M)        // hexagon face (bottom clipped; body covers it)
  rectfill(g, 5, 5, 6, 5, EYE); pixel(g, 5, 5, PUP)   // angry little eyes
  rectfill(g, 9, 5, 10, 5, EYE); pixel(g, 10, 5, PUP)
  return g
}

// ── bodies (16×7) ───────────────────────────────────────────────────────────

function bodyBlob() {            // round belly with sparse speckle
  const g = blank(16, 7)
  ovalfill(g, 8, 3.5, 6.5, 3.5, M)
  for (let y = 0; y < 7; y++)
    for (let x = 0; x < 16; x++)
      if (g[y][x] === M && noise(x, y, 6) === 0) g[y][x] = A
  return g
}

function bodyArmor() {           // riveted torso with shoulder pads + belt
  const g = blank(16, 7)
  rrectfill(g, 3, 0, 10, 7, 1, M)        // torso (symmetric about x=7.5)
  rectfill(g, 1, 1, 2, 3, A)             // left shoulder pad
  mirror(g)                              // → right pad
  rectfill(g, 4, 4, 11, 4, A)            // belt line
  pixel(g, 5, 1, EYE)                    // chest glint
  pixel(g, 4, 6, A); pixel(g, 11, 6, A)  // hip rivets
  return g
}

function bodyFuzzy() {           // shaggy ball — dense noise fur + tufts
  const g = blank(16, 7)
  ovalfill(g, 8, 3.5, 5.5, 3.2, M)
  pixel(g, 2, 1, M); pixel(g, 1, 3, M); pixel(g, 2, 5, M)  // tufts poking out
  mirror(g)
  for (let y = 0; y < 7; y++)
    for (let x = 0; x < 16; x++)
      if (g[y][x] === M && noise(x, y, 3) === 0) g[y][x] = A
  return g
}

// ── legs (16×5) ─────────────────────────────────────────────────────────────

function legsStubby() {
  const g = blank(16, 5)
  rectfill(g, 4, 0, 6, 2, M)             // left leg
  rectfill(g, 3, 3, 6, 4, A)             // left foot
  return mirror(g)
}

function legsLong() {
  const g = blank(16, 5)
  rectfill(g, 5, 0, 5, 3, M)             // thin left leg
  rectfill(g, 3, 4, 6, 4, A)             // flat clown foot
  return mirror(g)
}

function legsWheels() {
  const g = blank(16, 5)
  circlefill(g, 4, 2.4, 2.3, A)          // left wheel
  pixel(g, 4, 2, 6)                      // grey hub
  return mirror(g)
}

// ── compose all 27 combos — stamp() parts onto one canvas, outline, flatten ─

const HEADS  = [headRound(), headBoxy(), headSpiky()]
const BODIES = [bodyBlob(), bodyArmor(), bodyFuzzy()]
const LEGS   = [legsStubby(), legsLong(), legsWheels()]

function compose(h, b, l) {
  const g = blank(16, 16)
  stamp(g, LEGS[l], 0, 11)     // feet on the ground (y11–15)
  stamp(g, BODIES[b], 0, 6)    // body covers leg tops (y6–12)
  stamp(g, HEADS[h], 0, 0)     // head/chin in front of body top (y0–7)
  return flat(outlined(g))
}

const combos = {}
for (let h = 0; h < 3; h++)
  for (let b = 0; b < 3; b++)
    for (let l = 0; l < 3; l++)
      combos[h * 9 + b * 3 + l] = compose(h, b, l)

// ── mixer machine — one 32×16 drawing split() across slots 28+29 ────────────

function machineSheet() {
  const g = blank(32, 16)
  rrectfill(g, 0, 1, 32, 15, 3, 21)      // drop shadow
  rrectfill(g, 1, 0, 30, 14, 3, 5)       // housing
  rrectfill(g, 3, 2, 26, 10, 2, 6)       // face plate
  rectfill(g, 6, 4, 13, 9, 17)           // glass window
  rectfill(g, 7, 5, 8, 6, 12)            // glass shine
  pixel(g, 21, 4, 8); pixel(g, 23, 4, 10); pixel(g, 25, 4, 11)  // status lights
  rrectfill(g, 19, 6, 9, 4, 1, 22)       // dispenser slot
  rectfill(g, 20, 7, 26, 8, 16)          //   …its dark mouth
  for (let x = 4; x < 28; x += 3) pixel(g, x, 12, 22)  // rivet row
  return split(g)                        // → [left 16×16, right 16×16]
}
const [machineL, machineR] = machineSheet()

// ── prize star — concave 10-point polyfill, orange outline ──────────────────

function starSprite() {
  const g = blank()
  const pts = []
  for (let i = 0; i < 10; i++) {
    const a = -Math.PI / 2 + i * Math.PI / 5
    const r = i % 2 === 0 ? 7 : 3
    pts.push(8 + r * Math.cos(a), 8 + r * Math.sin(a))
  }
  polyfill(g, pts, 10)                   // yellow star
  pixel(g, 6, 6, EYE)                    // glint
  return flat(outlined(g, 9))            // orange outline instead of the default
}

// ── lab map tiles ───────────────────────────────────────────────────────────

function floorTile() {
  const g = blank(16, 16, 21)            // dark grout base (never 0 → no holes)
  rectfill(g, 1, 1, 15, 15, 5)           // tile face
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if (g[y][x] === 5 && noise(x, y, 17) === 0) g[y][x] = 22  // worn flecks
  return flat(g)
}

function wallTile() {
  const g = blank(16, 16, 17)            // deep blue panel
  rectfill(g, 0, 14, 15, 14, 1)          // panel seam
  rectfill(g, 0, 15, 15, 15, 21)         // skirting
  for (let y = 0; y < 13; y++)
    for (let x = 0; x < 16; x++)
      if (noise(x, y, 31) === 0) g[y][x] = 1   // faint texture
  return flat(g)
}

// ── map: wall strip behind the lab, floor below ─────────────────────────────

const MW = 20, MH = 13
const layout = []
for (let y = 0; y < MH; y++) layout.push((y < 4 ? '#' : '.').repeat(MW))

module.exports = {
  screenW: 320, screenH: 200, scale: 3,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: Object.assign({}, combos, {
    28: machineL, 29: machineR,
    30: starSprite(),
    40: floorTile(), 41: wallTile(),
  }),
  map: { layout, tiles: { '.': 40, '#': 41 } },
}
