// Textures for podracer.c — two 16×16 tiles built as raw palette-index arrays
// (parseSprite takes a number[] directly, so we can reach the extended palette
// 16–31 that ASCII art can't). tritex samples these per canyon quad.
//   slot 0 = rock-strata canyon wall   slot 1 = tech floor (dashed centre line)
// indices: 16 brownish-black 20 dark-brown 4 brown 5 dark-grey 30 dark-peach
//          9 orange  21 darker-grey 22 medium-grey 28 true-blue

function rock() {
  const band = [16, 20, 20, 4, 4, 30, 9, 30, 4, 4, 5, 20, 20, 16, 16, 16] // strata per row
  const px = []
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      let c = band[y]
      const off = (Math.floor(y / 4) % 2) * 4         // staggered vertical cracks
      if ((x + off) % 8 === 0) c = 16                 // dark fissure
      if (y % 4 === 2 && x % 5 === 2) c = 9           // a fleck of ore
      px.push(c)
    }
  }
  return px
}

function floor() {
  const px = []
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      let c = 21                                       // dark metal base
      if (x === 0 || x === 15 || y === 0 || y === 15) c = 16   // panel border
      if ((x === 4 || x === 11) && y % 4 !== 0) c = 5          // tread grooves
      if ((y === 4 || y === 11) && x % 4 !== 0) c = 5
      if ((x === 2 || x === 13) && (y === 2 || y === 13)) c = 22 // rivets
      if (x === 7 || x === 8) c = (y % 2 === 0) ? 28 : 21        // dashed centre line
      px.push(c)
    }
  }
  return px
}

module.exports = {
  sprites: {
    0: rock(),
    1: floor(),
  },
}
