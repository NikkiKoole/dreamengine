// config for gridcity.c — a sheet of tiny pixel-art BRUSH ICONS for the bottom
// toolbar (drawn with sprite-draw.js). gridcity renders its city as a 3px tile
// grid; the only sprites are these cute clickable brush buttons.
//
// Slot order = toolbar order (the C side draws spr(i) for button i):
//   0 erase  1 road  2 water  3 R-zone  4 C-zone  5 I-zone  6 police  7 park
//   8 school 9 hospital 10 power-plant 11 fire-station 12 water-pump 13 ignite
//   14 dense-toggle
//
// pico32 indices echo gridcity's own city_color() where it helps the icon read:
//   3 dk-green 11 green (R) · 12 blue 28 true-blue (C) · 10 yellow 25 dk-orange (I)
//   7 white · 8 red · 5 dk-grey 6 lt-grey · 16 brownish-black (outline)

const { blank, pixel, rectfill, line, circlefill, ovalfill, rrectfill, trifill, outlined, flat } =
  require('../sprite-draw.js')

function ic_erase() {
  const g = blank()
  rrectfill(g, 3, 6, 11, 7, 2, 14)                       // pink eraser body
  rectfill(g, 3, 6, 13, 7, 15)                           // light top bevel
  line(g, 6, 6, 4, 12, 5); line(g, 10, 6, 8, 12, 5)      // wear lines
  pixel(g, 2, 13, 6); pixel(g, 5, 14, 6); pixel(g, 8, 13, 6)  // crumbs
  return flat(outlined(g))
}
function ic_road() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 5)
  line(g, 1, 1, 14, 1, 21); line(g, 1, 14, 14, 14, 21)
  rectfill(g, 7, 2, 8, 4, 10); rectfill(g, 7, 7, 8, 9, 10); rectfill(g, 7, 12, 8, 13, 10)
  return flat(g)
}
function ic_water() {
  const g = blank(); rectfill(g, 1, 1, 14, 14, 12)
  line(g, 2, 4, 13, 4, 28); line(g, 2, 9, 13, 9, 28)
  pixel(g, 4, 6, 7); pixel(g, 9, 11, 7); pixel(g, 11, 3, 7)
  return flat(g)
}
function ic_house(roofHi, roofLo, body) {       // shared R-zone-ish little house
  const g = blank()
  trifill(g, 2, 8, 13, 8, 8, 2, roofHi)                  // roof
  line(g, 2, 8, 8, 2, roofLo); line(g, 13, 8, 8, 2, roofLo)
  rectfill(g, 4, 8, 11, 14, body)                        // walls
  rectfill(g, 6, 10, 7, 14, 16)                          // door
  pixel(g, 9, 10, 7); pixel(g, 10, 10, 7)                // window
  return flat(outlined(g))
}
function ic_R() { return ic_house(11, 3, 3) }            // green residential house
function ic_C() {                                        // blue commercial storefront
  const g = blank()
  rectfill(g, 2, 5, 13, 14, 28)                          // building
  rectfill(g, 2, 5, 13, 6, 12)                           // roof line
  for (let x = 2; x <= 13; x += 2) pixel(g, x, 8, 10)    // awning stripes
  rectfill(g, 2, 8, 13, 8, 9)
  rectfill(g, 4, 10, 6, 13, 7); rectfill(g, 9, 10, 11, 13, 7)  // big windows
  return flat(outlined(g))
}
function ic_I() {                                        // industrial factory + chimney
  const g = blank()
  rectfill(g, 2, 8, 13, 14, 25)                          // shed body
  trifill(g, 2, 8, 6, 8, 2, 5, 10); trifill(g, 6, 8, 10, 8, 6, 5, 10)  // sawtooth roof
  trifill(g, 10, 8, 14, 8, 10, 5, 10)
  rectfill(g, 10, 2, 12, 8, 5)                           // chimney
  circlefill(g, 11, 2, 2, 6)                             // smoke puff
  return flat(outlined(g))
}
function ic_police() {                                   // shield badge + a clean white star
  const g = blank()
  rrectfill(g, 3, 2, 10, 8, 2, 12)                       // blue shield top
  trifill(g, 3, 9, 12, 9, 8, 14, 12)                     // shield point
  // 5-point white star, centred on the shield
  pixel(g, 8, 4, 7); pixel(g, 8, 5, 7)
  rectfill(g, 5, 6, 11, 6, 7)                            // wide arms
  rectfill(g, 6, 7, 10, 7, 7)
  pixel(g, 6, 8, 7); pixel(g, 8, 8, 7); pixel(g, 10, 8, 7)  // legs
  return flat(outlined(g))
}
function ic_park() {                                     // a tree
  const g = blank()
  rectfill(g, 7, 9, 8, 14, 4)                            // trunk
  circlefill(g, 7, 6, 5, 3); circlefill(g, 7, 6, 4, 11)  // canopy
  pixel(g, 5, 5, 27); pixel(g, 9, 7, 27)
  return flat(outlined(g))
}
function ic_school() {                                   // graduation mortarboard
  const g = blank()
  rectfill(g, 5, 8, 10, 11, 1)                           // the cap on the head (trapezoid-ish)
  trifill(g, 8, 4, 1, 7, 15, 7, 1)                       // flat board, top half
  trifill(g, 1, 7, 15, 7, 8, 9, 1)                       // flat board, bottom half (a wide rhombus)
  pixel(g, 8, 7, 10)                                     // gold centre button
  line(g, 12, 7, 12, 12, 10); circlefill(g, 12, 13, 1, 10)  // tassel
  return flat(outlined(g))
}
function ic_hospital() {                                 // white block + red cross
  const g = blank()
  rrectfill(g, 2, 2, 12, 12, 2, 7)
  rectfill(g, 7, 4, 8, 11, 8); rectfill(g, 4, 7, 11, 8, 8)  // red cross
  return flat(outlined(g))
}
function ic_plant() {                                    // cooling tower + lightning bolt
  const g = blank()
  trifill(g, 2, 14, 6, 14, 4, 4, 6); rectfill(g, 2, 13, 6, 14, 5)  // tower
  trifill(g, 9, 14, 13, 14, 11, 4, 6); rectfill(g, 9, 13, 13, 14, 5)
  line(g, 9, 2, 6, 8, 10); line(g, 6, 8, 9, 8, 10); line(g, 9, 8, 6, 14, 10)  // bolt
  line(g, 8, 2, 5, 8, 10)
  return flat(outlined(g))
}
function ic_fire_stn() {                                 // red fire engine
  const g = blank()
  rectfill(g, 1, 6, 13, 11, 8)                           // body
  rectfill(g, 9, 4, 13, 7, 8)                            // cab
  rectfill(g, 10, 5, 12, 6, 12)                          // windscreen
  line(g, 2, 5, 9, 5, 6)                                 // ladder
  circlefill(g, 4, 12, 2, 5); circlefill(g, 11, 12, 2, 5)  // wheels
  pixel(g, 2, 7, 10)                                     // beacon
  return flat(outlined(g))
}
function ic_pump() {                                     // blue water tower on legs
  const g = blank()
  rrectfill(g, 3, 2, 10, 7, 2, 28)                       // tank
  pixel(g, 5, 4, 7)                                      // glint
  line(g, 4, 9, 2, 14, 5); line(g, 8, 9, 8, 14, 5); line(g, 11, 9, 13, 14, 5)  // legs
  rectfill(g, 3, 8, 12, 9, 5)
  return flat(outlined(g))
}
function ic_ignite() {                                   // a flame (matchstick)
  const g = blank()
  ovalfill(g, 8, 9, 3, 4, 8)                             // red flame body
  ovalfill(g, 8, 10, 2, 3, 9)                            // orange core
  pixel(g, 8, 4, 10); pixel(g, 8, 5, 10); pixel(g, 9, 6, 10)  // yellow tip
  rectfill(g, 7, 12, 8, 14, 4)                           // little stick
  return flat(outlined(g))
}
function ic_dense() {                                    // skyline: short→tall (zoning density)
  const g = blank()
  rectfill(g, 2, 10, 5, 14, 3)                           // short (green)
  rectfill(g, 6, 6, 9, 14, 11)                           // medium
  rectfill(g, 10, 2, 13, 14, 10)                         // tall
  pixel(g, 11, 4, 16); pixel(g, 11, 7, 16); pixel(g, 7, 8, 16)  // windows
  return flat(outlined(g))
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: {
    0: ic_erase(), 1: ic_road(), 2: ic_water(), 3: ic_R(), 4: ic_C(), 5: ic_I(),
    6: ic_police(), 7: ic_park(), 8: ic_school(), 9: ic_hospital(), 10: ic_plant(),
    11: ic_fire_stn(), 12: ic_pump(), 13: ic_ignite(), 14: ic_dense(),
  },
}
