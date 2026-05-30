// config for textured3d.c — two 16×16 textures built as raw palette-index
// arrays (parseSprite accepts a number[] directly, not just ASCII art).
// palette indices: 4=brown 5=dark-grey 9=orange 15=peach 6=light-grey

function brick() {
  const N = 4, S = 5, O = 9, px = []
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const course = Math.floor(y / 4)
      const mortarRow = (y % 4 === 0)
      const offset = (course % 2) * 4
      const joint = ((x + offset) % 8 === 7)
      // a touch of orange highlight on the row just under each mortar line
      px.push(mortarRow || joint ? S : (y % 4 === 1 ? O : N))
    }
  }
  return px
}

function crate() {
  const N = 4, S = 5, O = 9, p = 15, px = []
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      let c = N
      if (x === 0 || y === 0 || x === 15 || y === 15) c = S          // metal frame
      else if (x === y || x === 15 - y) c = p                        // diagonal X brace
      if ((x === 1 || x === 14) && (y === 1 || y === 14)) c = O      // corner bolts
      px.push(c)
    }
  }
  return px
}

module.exports = {
  sprites: {
    0: brick(),
    1: crate(),
  },
}
