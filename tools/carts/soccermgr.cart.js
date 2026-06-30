// soccermgr.cart.js — the baked intro title.
// "FOOTBALL" + "MANAGER" are rasterized from the Futura Condensed Extra Bold TTF
// (tools/font-bake.js) into the sprite sheet at build time — no runtime cost.
//   FOOTBALL → slots 0..15   (sheet region 0,0  → 128×32)
//   MANAGER  → slots 16..31  (sheet region 0,32 → 128×32)
// Drawn with two sspr() calls in soccermgr.c's draw_title(), each under a long
// layered drop shadow (the ink is pal()-recolored dark and stamped on a 6-step
// diagonal). soccermgr.c MUST colorkey(0) so the palette-0 padding is transparent.
// No baked outline/shadow here — the C side owns the shadow so it can be a big,
// cool extruded one rather than a flat 1px drop.

const { bakeBanner } = require('../font-bake.js')

const FONT = 'fonts/Futura-Condensed-Extra-Bold.ttf'

const sprites = {}
// white wordmark, light-grey AA edge
bakeBanner(FONT, 'FOOTBALL', 8, 2, { color: 7, aa: 6 })
  .forEach((tile, i) => { sprites[i] = tile })
// warm light-yellow wordmark, orange AA edge
bakeBanner(FONT, 'MANAGER', 8, 2, { color: 23, aa: 9 })
  .forEach((tile, i) => { sprites[16 + i] = tile })

module.exports = { sprites }
