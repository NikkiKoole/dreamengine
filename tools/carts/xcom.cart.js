// xcom.cart.js — battlefield tiles + unit sprites for xcom.c
//
// Map slots are read directly by the gameplay (FLOOR=1, WALL=2, SAND=3, CRATE=4):
//   1 floor · 2 wall · 3 sandbags (half cover) · 4 crate (full cover)
//   8 soldier (armour painted in magic index 28 'm', recoloured per soldier)
//   9 sectoid · 10 muton
//
// Custom charMap reaches the 16–31 extended palette ('z','x','q','u','e','m','d').

const charMap = {
  '.': 0, 'S': 5, 'L': 6, 'W': 7, 'N': 4, 'O': 9, 'Y': 10, 'g': 11, 'R': 8,
  'b': 12, 'I': 13, 'G': 3, 'k': 14, 'p': 15,
  'z': 16, 'x': 21, 'q': 22, 'u': 20, 'e': 27, 'd': 25, 'm': 28,
}

module.exports = {
  charMap,
  sprites: {

    // 1 — floor: dark concrete, faint speckle, grid line on the bottom/right edge
    1: `
SSSSSSSSSSSSSSSz
SqSSSSSSSSSSSSSz
SSSSSSSSSSSqSSSz
SSSSSSSSSSSSSSSz
SSSSSSqSSSSSSSSz
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
SqSSSSSSSSSSSSSz
SSSSSSSSSSSqSSSz
SSSSSSSSSSSSSSSz
SSSSSqSSSSSSSSSz
SSSSSSSSSSSSSSSz
SSSSSSSSSSSqSSSz
SSSSSSSSSSSSSSSz
SqSSSSSSSSSSSSSz
zzzzzzzzzzzzzzzz
`,

    // 2 — wall: solid concrete block, light bevel
    2: `
LLLLLLLLLLLLLLLL
LxxxxxxxxxxxxxxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxSSSSSSSSSSSSxL
LxxxxxxxxxxxxxxL
LLLLLLLLLLLLLLLL
zzzzzzzzzzzzzzzz
`,

    // 3 — sandbags (half cover): floor base + a low stack of bags
    3: `
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
.NNNN.NNNN.NNN..
.NuuN.NuuN.NuN..
NNNNNNNNNNNNNN..
NuuuNNuuuNNuuN..
NuuuNNuuuNNuuN..
NNNNNNNNNNNNNN..
NuuuNNuuuNNuuNz.
NuuuNNuuuNNuuNz.
zzzzzzzzzzzzzzzz
`,

    // 4 — crate (full cover): floor base + a braced wooden box
    4: `
SSSSSSSSSSSSSSSz
SSSSSSSSSSSSSSSz
SSNNNNNNNNNNNNSz
SSNuuuuuuuuuuNSz
SSNuNuuuuuuNuNSz
SSNuuNuuuuNuuNSz
SSNuuuNuuNuuuNSz
SSNuuuuNNuuuuNSz
SSNuuuuNNuuuuNSz
SSNuuuNuuNuuuNSz
SSNuuNuuuuNuuNSz
SSNuNuuuuuuNuNSz
SSNuuuuuuuuuuNSz
SSNNNNNNNNNNNNSz
SSSSSSSSSSSSSSSz
zzzzzzzzzzzzzzzz
`,

    // 8 — soldier: helmet (dark grey), peach face, armour in magic index 28 ('m')
    8: `
................
......xxxx......
.....xppppx.....
.....pppppp.....
......pppp......
.....mmmmmm.....
....mmmmmmmm....
...Smmmmmmmm....
..SSSmmmmmm.....
....mmmmmmmm....
....mmm.mmm.....
....mm...mm.....
....mm...mm.....
...xxx...xxx....
...SSS...SSS....
................
`,

    // 9 — sectoid: grey body, big black eyes, spindly limbs
    9: `
................
......qqqq......
.....qqqqqq.....
....qqqqqqqq....
....qzqqqqzq....
....qzzqqzzq....
....qqqqqqqq....
.....qqqqqq.....
......qqqq......
......qSSq......
.....qSSSSq.....
.....SqSSqS.....
.....S.qq.S.....
....SS....SS....
....S......S....
................
`,

    // 10 — muton: bulky, green armour with dark-orange plating
    10: `
................
......eeee......
.....edddde.....
.....edddde.....
.....edddde.....
......eeee......
....eeeeeeee....
...eeeeeeeeee...
...eeeeeeeeee...
...edeeeeeede...
....eeeeeeee....
....eee.eee.....
....ee...ee.....
...ddd...ddd....
...SSS...SSS....
................
`,

  },
}
