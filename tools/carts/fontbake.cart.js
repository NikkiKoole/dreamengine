// fontbake.cart.js — title text baked from a real TTF (Google Font "Bungee")
// at build time via tools/font-bake.js. No runtime font code: the rasterized
// words land in the sprite sheet like any hand-drawn sprite, so the C side
// just sspr()s a fixed sheet region (and pal() still recolors it).
//
// Slot layout (8-col sheet) — each word is centered in a fixed slot-rect so
// the C cart can hardcode the sheet region regardless of the word's width:
//   slots 0–15  "DREAM"  → sheet region (0,  0) 128×32
//   slots 16–23 "ENGINE" → sheet region (0, 32) 128×16

const { blank, clone, stamp, split, outlined } = require('../sprite-draw.js')
const { bakeText, measure } = require('../font-bake.js')

const FONT = 'fonts/Bungee-Regular.ttf'

// Bake `text` centered into a slotCols×slotRows slot-rect, with the biggest
// px that fits. Treatments compose like on any sprite:
//   aa      darker edge color for partial-coverage pixels (-1 = off)
//   outline sprite-draw outlined() border color (-1 = off)
//   shadow  1px drop shadow color, stamped under the (outlined) text (-1 = off)
function bakeBanner(text, slotCols, slotRows, { color = 7, aa = -1, outline = -1, shadow = -1 } = {}) {
  const trim = 3 + (outline >= 0 ? 2 : 0) + (shadow >= 0 ? 1 : 0)
  const maxW = slotCols * 16 - trim, maxH = slotRows * 16 - trim
  let px = 64
  while (px > 6) {
    const m = measure(FONT, text, px)
    if (m.w <= maxW && m.h <= maxH) break
    px--
  }
  let g = bakeText(FONT, text, { px, color, aa, pad: outline >= 0 ? 2 : 1 })
  if (outline >= 0) g = outlined(g, outline)
  const c = blank(slotCols * 16, slotRows * 16)
  const ox = ((slotCols * 16 - g[0].length) / 2) | 0
  const oy = ((slotRows * 16 - g.length) / 2) | 0
  if (shadow >= 0) {
    const sh = clone(g)
    for (const row of sh) for (let i = 0; i < row.length; i++) if (row[i]) row[i] = shadow
    stamp(c, sh, ox + 1, oy + 1)
  }
  stamp(c, g, ox, oy)
  return split(c)
}

// title: yellow, orange AA edge, near-black outline, purple drop shadow
const title = bakeBanner('DREAM', 8, 2, { color: 10, aa: 9, outline: 16, shadow: 2 })
// subtitle: white with a dark-blue outline, no shadow — border alone
const sub = bakeBanner('ENGINE', 8, 1, { color: 7, outline: 17 })

const sprites = {}
title.forEach((t, i) => { sprites[((i / 8) | 0) * 8 + (i % 8)] = t })
sub.forEach((t, i) => { sprites[16 + i] = t })

module.exports = { sprites }
