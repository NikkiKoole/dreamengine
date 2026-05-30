// config for zoo.c — sprites that get rotated with spr_rot().
// slot 0 = visitor (kid), slot 1 = balloon, slot 2 = pinwheel, slot 3 = sun.
// 1/2/3 are generated as raw 16x16 palette-index arrays so the geometry is exact
// (cart.js sprites accept raw index arrays, not just ASCII art).

function blank() { return new Array(256).fill(0) }
function px(a, x, y, c) { if (x >= 0 && x < 16 && y >= 0 && y < 16) a[y * 16 + x] = c }

function tri(a, x1, y1, x2, y2, x3, y3, c) {
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const cx = x + 0.5, cy = y + 0.5
    const w1 = (x2 - cx) * (y3 - cy) - (x3 - cx) * (y2 - cy)
    const w2 = (x3 - cx) * (y1 - cy) - (x1 - cx) * (y3 - cy)
    const w3 = (x1 - cx) * (y2 - cy) - (x2 - cx) * (y1 - cy)
    if ((w1 >= 0 && w2 >= 0 && w3 >= 0) || (w1 <= 0 && w2 <= 0 && w3 <= 0)) px(a, x, y, c)
  }
}
function disc(a, cx, cy, r, c) {
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const dx = x + 0.5 - cx, dy = y + 0.5 - cy
    if (dx * dx + dy * dy <= r * r) px(a, x, y, c)
  }
}

// slot 2 — pinwheel: four swept blades around a white hub (Y=10 O=9 R=8 g=11 W=7)
const pinwheel = blank()
tri(pinwheel, 8, 8, 2, 1, 9, 3, 10)   // yellow blade, up
tri(pinwheel, 8, 8, 15, 2, 13, 9, 9)  // orange blade, right
tri(pinwheel, 8, 8, 14, 15, 7, 13, 8) // red blade, down
tri(pinwheel, 8, 8, 1, 14, 3, 7, 11)  // green blade, left
disc(pinwheel, 8, 8, 1.6, 7)          // hub

// slot 3 — sun: orange rays radiating from a yellow core
const sun = blank()
for (let k = 0; k < 8; k++) {
  const a = k * Math.PI / 4
  const ex = 8 + Math.cos(a) * 7.5, ey = 8 + Math.sin(a) * 7.5
  const bx1 = 8 + Math.cos(a + 0.32) * 3.5, by1 = 8 + Math.sin(a + 0.32) * 3.5
  const bx2 = 8 + Math.cos(a - 0.32) * 3.5, by2 = 8 + Math.sin(a - 0.32) * 3.5
  tri(sun, bx1, by1, bx2, by2, ex, ey, 9)
}
disc(sun, 8, 8, 4.2, 10) // yellow core

// slots 1,4,5,6,7 — balloons in five colors (spr_rot ignores pal(), so we bake one
// sprite per color). white glint + pink knot on each.
function balloonOf(body) {
  const a = blank()
  disc(a, 8, 7, 6, body)
  disc(a, 6, 5, 1.4, 7)
  px(a, 7, 13, 14); px(a, 8, 13, 14); px(a, 7, 14, 14)
  return a
}
const balloonRed = balloonOf(8), balloonOrange = balloonOf(9),
      balloonGreen = balloonOf(11), balloonBlue = balloonOf(12), balloonPink = balloonOf(14)

module.exports = {
  sprites: {
    // slot 0 — visitor: brown hair, peach face, dark-blue eyes (so colorkey(black)
    // doesn't punch holes), green tee, blue shorts, brown shoes
    0: `
................
......NNNN......
.....NNNNNN.....
.....NppppN.....
.....pppppp.....
.....pBppBp.....
......pppp......
.....gggggg.....
....gggggggg....
...pgggggggg p..
....gggggggg....
.....bbbbbb.....
.....bb..bb.....
.....bb..bb.....
....NNN..NNN....
................
`,
    1: balloonRed,
    2: pinwheel,
    3: sun,
    4: balloonOrange,
    5: balloonGreen,
    6: balloonBlue,
    7: balloonPink,
  },
}
