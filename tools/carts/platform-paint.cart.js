// The level, painted as a 16×16 sprite in slot 0. Each char is one block:
//   N = brown solid   p = light-peach one-way   R = red hazard
//   Y = yellow coin    g = green spawn           . = empty air
// The cart reads these back at runtime with sget() and builds the world.
module.exports = {
  sprites: {
    0: [
      "NNNNNNNNNNNNNNNN",
      "N..............N",
      "N....YY........N",
      "N...NNNN.......N",
      "N..............N",
      "N.......pppp...N",
      "N..............N",
      "N..Y...........N",
      "NNNNN..........N",
      "N.......NNNN...N",
      "N..........Y...N",
      "N....g.........N",
      "N........ppp...N",
      "N..............N",
      "N...RR.....RR..N",
      "NNNNNNNNNNNNNNNN",
    ].join("\n"),
  },
};
