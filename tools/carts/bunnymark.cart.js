// config for bunnymark.c — the bunny, posterized into the pico32 palette and
// tinted 5 ways, all as ordinary palette-indexed sprites (no custom sheet).
//
// SHAPE is tools/carts/bunnymark_bunny.png (32×32 grayscale) posterised to 5
// brightness levels: 0 transparent · 1 outline · 2 dark · 3 mid · 4 light · 5
// brightest. Each tint is a level→pico32-index ramp (RAMPS). A 32×32 sprite is a
// 2×2 block of slots, so split() cuts each tinted grid into [TL,TR,BL,BR] and we
// drop them into a contiguous slot block; bunnymark.c draws the 32×32 region with
// sspr(). The tint index in the cart (0..4) = the row in RAMPS / SLOTS below.

const { split } = require('../sprite-draw.js')

// 32×32 posterised bunny — digit = brightness level (0 = transparent)
const SHAPE = [
  "00000000011111000011111000000000",
  "00000000134444100144444100000000",
  "00000000135554100135554100000000",
  "00000000135554100135554100000000",
  "00000000135554100135554100000000",
  "00000000135553100135554100000000",
  "00000000133533311333534100000000",
  "00000000133333333333334100000000",
  "00000000133313333331334100000000",
  "00000000133313333331334100000000",
  "00000000133333333333334100000000",
  "00000000133333111133334100000000",
  "00001111333333355333333411110000",
  "00012222333333355333333344441000",
  "00012222233333333333333333331000",
  "00001111223333444443333311110000",
  "00000000122334555554433100000000",
  "00000000122345555555433100000000",
  "00000000122345555555433100000000",
  "00000000122345555555433100000000",
  "00000000122345555555433100000000",
  "00000000122334555554333100000000",
  "00000000122333444443333100000000",
  "00000000122333333333333100000000",
  "00000000122333222222333100000000",
  "00000000122332222222233100000000",
  "00000000122311111111223100000000",
  "00000000122100000000122100000000",
  "00000000122100000000122100000000",
  "00000000122100000000122100000000",
  "00000000122100000000122100000000",
  "00000000011000000000011000000000",
]

// per-tint ramp: level 1..5 → pico32 palette index (level 0 stays 0 = transparent,
// keyed out via colorkey(0) in the cart). Index 1 of each row is the dark outline.
//                  L1  L2  L3  L4  L5
const RAMPS = [
  [0, 16,  5, 22,  6,  7],  // 0 grey / neutral  (the original bunny)
  [0, 16, 25,  9, 10, 23],  // 1 yellow
  [0, 16, 24, 14, 31, 31],  // 2 pink
  [0, 16,  3, 11, 26, 26],  // 3 green
  [0, 16,  4, 25,  9, 31],  // 4 orange
]

// each tint occupies a 2×2 slot block; pixel origins match BTX/BTY in bunnymark.c
// (0,0)(32,0)(64,0)(96,0)(0,32). split() returns [TL,TR,BL,BR] in that order.
const SLOTS = [[0,1,8,9], [2,3,10,11], [4,5,12,13], [6,7,14,15], [16,17,24,25]]

function tintGrid(ramp) {
  const g = []
  for (let y = 0; y < 32; y++) {
    const row = []
    for (let x = 0; x < 32; x++) { const lv = +SHAPE[y][x]; row.push(lv ? ramp[lv] : 0) }
    g.push(row)
  }
  return g
}

const sprites = {}
RAMPS.forEach((ramp, t) => {
  const quads = split(tintGrid(ramp), 16, 16)   // [TL, TR, BL, BR]
  SLOTS[t].forEach((slot, i) => { sprites[slot] = quads[i] })
})

module.exports = { sprites }
