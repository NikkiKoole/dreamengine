// gta.cart.js — sprites + map for gta.c
// Sprite 2 = building/wall tile (brown brick)
// Map: 'W' → sprite 2 (wall), '.' → 0 (road/open)
//
// City layout (40×25 cells, 16px each = 640×400 world):
//   Cols  0-1   : border road
//   Cols  2-9   : block (8 wide)
//   Cols 10-13  : vertical road
//   Cols 14-23  : block (10 wide)
//   Cols 24-27  : vertical road
//   Cols 28-37  : block (10 wide)
//   Cols 38-39  : border road
//
//   Rows  0-1   : border road
//   Rows  2-9   : top building row (8 tall)
//   Rows 10-13  : horizontal road
//   Rows 14-22  : bottom building row (9 tall)
//   Rows 23-24  : border road

module.exports = {
  sprites: {

    // slot 2 — building/wall (brown brick, 16×16)
    2: `
NNNNNNNNNNNNNNNN
NN..NNNNNN..NNNN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
SSSSSSSSSSSSSSSS
NNNNNN..NNNNNN..
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
SSSSSSSSSSSSSSSS
NNNNNNNNNNNNNNNN
NN..NNNNNN..NNNN
NNNNNNNNNNNNNNNN
NNNNNNNNNNNNNNNN
SSSSSSSSSSSSSSSS
NNNNNN..NNNNNN..
`,

  },

  map: {
    tiles: { 'W': 2 },
    layout: [
      '........................................',  // row 0  border road
      '........................................',  // row 1
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 2  top blocks
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 3
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 4
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 5
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 6
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 7
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 8
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 9
      '........................................',  // row 10 horizontal road
      '........................................',  // row 11
      '........................................',  // row 12
      '........................................',  // row 13
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 14 bottom blocks
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 15
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 16
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 17
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 18
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 19
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 20
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 21
      '..WWWWWWWW....WWWWWWWWWW....WWWWWWWWWW..', // row 22
      '........................................',  // row 23 border road
      '........................................',  // row 24
    ],
    mapW: 128,
    mapH: 64,
  },
}
