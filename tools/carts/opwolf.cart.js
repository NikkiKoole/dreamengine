// opwolf.cart.js — sprites + scrolling world for opwolf.c (OPERATION WOLF)
//
// Magic uniform/helmet colors (28/29) on the soldier are swapped per enemy type
// with pal() in the cart (grunt=green, thrower=brown, armored=grey). The ground +
// treeline tiles are drawn in the dark/bright green ramp (3 / 11) and the cart
// pal()-remaps those two indices per stage (jungle -> camp -> airfield).
//
// Slots: 1 soldier stand · 2 soldier fire · 3 civilian · 4 ammo crate ·
//        5 rocket crate · 8 ground · 9 treeline · 10 structure

// build a 128-wide, periodic-every-8-columns world so the rail scroll wraps seamlessly
const W = 128
const sky   = ' '.repeat(W)
const huts  = Array.from({ length: W }, (_, c) => (c % 8 === 3 ? 'H' : ' ')).join('')
const tree  = 'F'.repeat(W)
const grnd  = 'g'.repeat(W)

module.exports = {
  // NOTE: make-cart's buildSpriteSheet REPLACES the default char map with this one
  // (it does not merge), so we list every char our sprites use, plus the two magic
  // placeholders 'u' (uniform 28) and 'm' (helmet 29) recolored per enemy in the cart.
  charMap: {
    '.': 0,
    'u': 28, 'm': 29,   // magic uniform / helmet
    'p': 15,            // peach skin
    'W': 7,             // white
    'S': 5,             // dark grey (gun, boots, crate frame)
    'G': 3,             // dark green  (map ramp — pal()-remapped per stage)
    'g': 11,            // bright green (map ramp — pal()-remapped per stage)
    'R': 8,             // red
    'Y': 10,            // yellow
    'N': 4,             // brown
    'L': 6,             // light grey
    'k': 14,            // pink (hut door)
  },

  sprites: {

    // 1 — soldier standing, rifle across the chest
    1: `
......mmmm......
.....mmmmmm.....
.....mppppm.....
.....pWppWp.....
......pppp......
....uuuuuuuu....
...uuuuuuuuuu...
...uuuSSSSuuu...
...uuSSSSSSuu...
...uuuuuuuuuu...
....uuuuuuuu....
....uu....uu....
....uu....uu....
....SS....SS....
................
................
`,

    // 2 — soldier firing, rifle raised toward you (the telegraph/shoot frame)
    2: `
......mmmm......
.....mmmmmm.....
.....mppppm.....
.....pWppWp....S
......pppp....SS
....uuuuuuuupSS.
...uuuuuuuuuSWS.
...uuuuuuuuuSS..
...uuuuuuuuuu...
....uuuuuuuu....
....uu....uu....
....uu....uu....
....SS....SS....
................
................
................
`,

    // 3 — civilian, hands up, white shirt / brown trousers (DON'T shoot)
    3: `
......pppp......
.....pppppp.....
.....pWppWp.....
......pppp......
.p..WWWWWWWW..p.
.pp.WWWWWWWW.pp.
....WWWWWWWW....
....WWWWWWWW....
....WWWWWWWW....
....NNNNNNNN....
....NNNNNNNN....
....NN....NN....
....NN....NN....
....SS....SS....
................
................
`,

    // 4 — ammo crate (olive box, yellow bands)
    4: `
................
.SSSSSSSSSSSSSS.
.SGGGGGGGGGGGGS.
.SGGGGGGGGGGGGS.
.SYYYYYYYYYYYYS.
.SYGGGGGGGGGGYS.
.SGGGGGGGGGGGGS.
.SGGGGGGGGGGGGS.
.SGGGGGGGGGGGGS.
.SYYYYYYYYYYYYS.
.SGGGGGGGGGGGGS.
.SSSSSSSSSSSSSS.
................
................
................
................
`,

    // 5 — rocket crate (red box, white rocket)
    5: `
................
.SSSSSSSSSSSSSS.
.SRRRRRRRRRRRRS.
.SRRRRRWWRRRRRS.
.SRRRRWWWWRRRRS.
.SRRRWWWWWWRRRS.
.SRRRRWWWWRRRRS.
.SRRRRRWWRRRRRS.
.SRRRRRWWRRRRRS.
.SRRRRRWWRRRRRS.
.SRRRRRRRRRRRRS.
.SSSSSSSSSSSSSS.
................
................
................
................
`,

    // 8 — ground (uniform dark green 3 with sparse bright green 11 speckle, no seams)
    8: `
GGGgGGGGGGgGGGGG
GGGGGGgGGGGGGGGG
GgGGGGGGGGGGGgGG
GGGGGGGGgGGGGGGG
GGGGgGGGGGGGGGGg
GGGGGGGGGGgGGGGG
GgGGGGGGGGGGGGGG
GGGGGGGgGGGGGGGG
GGGGGGGGGGGGGGgG
GGgGGGGGGGGGGGGG
GGGGGGGGgGGGGGGG
GGGGGGGGGGGGGGGG
GGGgGGGGGGGGGGgG
GGGGGGGGGGGGGGGG
GGGGgGGGGGGGGGGG
GGGGGGGGGGGGGGGG
`,

    // 9 — treeline (bumpy silhouette top so sky shows between the bush tops)
    9: `
..G..GG...GG..G.
.GGG.GGG.GGGG.GG
GGGGgGGGGGGGGgGG
GGGGGGGGGGGGGGGG
GGGgGGGGGGgGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGgGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
`,

    // 10 — structure / watch-hut (neutral browns + greys, same in every stage)
    10: `
.......W........
......WWW.......
.....NNNNN......
....NNNNNNN.....
...NNNNNNNNN....
..NNNNNNNNNNN...
..SLLLLLLLLLLS..
..SLLLLLLLLLLS..
..SLLLkkLLLLLS..
..SLLLkkLLLLLS..
..SLLLLLLLLLLS..
..SSSSSSSSSSSSS.
................
................
................
................
`,

  },

  map: {
    tiles: { 'g': 8, 'F': 9, 'H': 10 },
    layout: [ sky, sky, sky, sky, huts, tree, grnd, grnd, grnd, grnd, grnd, grnd, grnd ],
  },
}
