// DELUXE PAINT — a pixel paint program. No sprites or map; it only needs a
// roomier canvas than the 320×200 default, so all this config does is bump the
// screen size (and keep a chunky 3× window). Everything is drawn at runtime
// with pset / primitives into an in-RAM canvas[][].
module.exports = {
  screenW: 384,
  screenH: 240,
  scale: 3,
};
