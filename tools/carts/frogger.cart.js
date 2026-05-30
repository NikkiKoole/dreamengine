// config for frogger.c
// Sprite slots:  1=grass  2=road  3=lily pad  4=frog  6=log chunk

module.exports = {
  sprites: {

    // slot 1 — grass tile (dark/bright green checkerboard)
    1: `
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
`,

    // slot 2 — road tile (dark grey)
    2: `
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
SSSSSSSSSSSSSSSS
`,

    // slot 3 — lily pad (green circle, transparent background)
    3: `
................
................
.....GGGGGG.....
....GggggggG....
...GggggggggG...
..GggggggggggG..
..GggggggggggG..
..GggggggggggG..
..GggggggggggG..
..GggggggggggG..
...GggggggggG...
....GggggggG....
.....GGGGGG.....
................
................
................
`,

    // slot 4 — frog (top-down view, facing up)
    // head at top (facing north), back feet at bottom
    4: `
................
..gg..gggg..gg..
..gg..gggg..gg..
...gggggggggg...
...ggWggggWgg...
....gggggggg....
...ggGGGGGGgg...
...ggGGGGGGgg...
...ggGGGGGGgg...
...gggggggggg...
....gggggggg....
...gg......gg...
..ggg......ggg..
.ggggg....ggggg.
................
................
`,

    // slot 6 — log chunk (brown with orange ring marks, repeatable)
    6: `
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
NNNNOONNNNNNOONN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
NNNNOONNNNNNOONN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
NNNNOONNNNNNOONN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
NNNNOONNNNNNOONN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
`,

  },

  // map: 20 cols × 11 rows, each cell = 16×16 px → 320×176 total
  // row 0 = home row, rows 1-4 = water, row 5 = median, rows 6-9 = road, row 10 = start
  map: {
    layout: [
      '.L...L...L...L...L..', // row 0: home (lily pads at cols 1,5,9,13,17)
      '                    ', // row 1: water
      '                    ', // row 2: water
      '                    ', // row 3: water
      '                    ', // row 4: water
      'GGGGGGGGGGGGGGGGGGGG', // row 5: safe median
      'RRRRRRRRRRRRRRRRRRRR', // row 6: road
      'RRRRRRRRRRRRRRRRRRRR', // row 7: road
      'RRRRRRRRRRRRRRRRRRRR', // row 8: road
      'RRRRRRRRRRRRRRRRRRRR', // row 9: road
      'GGGGGGGGGGGGGGGGGGGG', // row 10: start zone
    ],
    tiles: { 'G': 1, 'R': 2, 'L': 3 },
    mapW: 128,
    mapH: 64,
  },
}
