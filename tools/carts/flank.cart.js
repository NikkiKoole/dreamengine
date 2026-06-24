// config for flank.c — programmatic pixel-art sprites drawn with sprite-draw.js.
// flank renders its WORLD procedurally (the little pixel-men, pickups, corpses — and
// each enemy's carried weapon is a thin barrel-line that varies by type). These
// sprites live in the CHROME instead — the HUD, the start menu, the win/lose screen
// — where there's room for art and it doesn't clutter the tactical read:
//   * WEAPON GLYPHS (slots 0..4) — the arsenal legend on the start menu, so a player
//     learns what each squad type carries. Slot == the W_* weapon enum.
//        0 SMG · 1 shotgun · 2 marksman · 3 knife · 4 brawl
//   * SLIDER CUES (slots 8..9) — medkit by the "healing" slider · clip by "ammo".
//   * END SCREEN (slots 16..17) — big heart on a clear · skull on a death.
//   * HUD (slots 18..20) — small heart (HP) · clip (ammo) · skull (kills), sized to
//     sit in the 16px status strip.
//
// Centred glyphs (0..9,16,17) sit around (8,8) in their 16×16 slot → C positions
// them spr(slot, x-8, y-8). HUD minis (18..20) are top-left so spr(slot, x, HUD_Y)
// lands them in the row. Transparent bg = index 0 (the colorkey); outlines use 16.
//
// pico32 indices used: 4 brown · 5 dk-grey · 6 lt-grey · 7 white · 8 red · 9 orange
// · 10 yellow · 11 green · 13 indigo · 14 pink · 16 outline · 18 darker-purple
// · 20 dk-brown · 22 med-grey · 23 lt-yellow · 24 dk-red · 25 dk-orange · 31 peach.
// Iterate: node tools/sprite-preview.js flank   (then Read the PNG).

const { blank, pixel, rectfill, rrectfill, line, circlefill, ovalfill, trifill,
        outlined, flat } = require('../sprite-draw.js')

// ── WEAPON GLYPHS (slots 0..4 == W_SMG, W_SHOTGUN, W_MARKSMAN, W_KNIFE, W_BRAWL) ──
// Side-view silhouettes pointing right, each carrying its signature colour (the
// same accent the man's cap uses), so the glyph echoes the figure it tags.
function w_smg() {                                       // compact submachine gun
  const g = blank()
  rectfill(g, 4, 7, 11, 9, 5)                            // receiver body
  rectfill(g, 2, 7, 4, 9, 5)                             // folding stock
  rectfill(g, 11, 7, 13, 8, 6)                           // short barrel
  rectfill(g, 6, 9, 8, 12, 22)                           // box magazine, hanging
  rectfill(g, 4, 6, 10, 6, 13)                           // indigo top rail (its cap colour)
  pixel(g, 5, 8, 6)                                      // glint
  return flat(outlined(g))
}
function w_shotgun() {                                   // fat pump shotgun
  const g = blank()
  rectfill(g, 2, 8, 6, 10, 4)                            // wooden stock
  rectfill(g, 6, 7, 14, 9, 5)                            // long heavy barrel
  rectfill(g, 6, 7, 14, 7, 6)                            // top rib highlight
  rectfill(g, 5, 9, 7, 11, 4)                            // pump grip
  rectfill(g, 2, 8, 3, 10, 14)                           // pink butt (its cap colour)
  pixel(g, 14, 8, 7)                                     // muzzle
  return flat(outlined(g))
}
function w_marksman() {                                  // long scoped rifle
  const g = blank()
  rectfill(g, 1, 9, 6, 10, 20)                           // stock
  rectfill(g, 6, 8, 15, 9, 5)                            // very long barrel
  rectfill(g, 8, 5, 11, 7, 6)                            // raised scope
  rectfill(g, 8, 6, 11, 6, 7)                            // white scope glint (its cap colour)
  line(g, 5, 10, 7, 13, 5)                               // trigger grip
  pixel(g, 15, 8, 7)                                     // muzzle
  return flat(outlined(g))
}
function w_knife() {                                     // dagger, blade up-right
  const g = blank()
  line(g, 5, 11, 11, 5, 6)                               // blade
  line(g, 6, 11, 12, 5, 7)                               // bright edge
  pixel(g, 12, 4, 7)                                     // tip glint
  line(g, 4, 10, 7, 13, 5)                               // crossguard
  rectfill(g, 3, 11, 5, 14, 24)                          // dark-red handle (its cap colour)
  return flat(outlined(g))
}
function w_brawl() {                                     // bare fist (melee bruiser)
  const g = blank()
  rrectfill(g, 4, 5, 7, 6, 2, 31)                        // peach fist
  pixel(g, 5, 5, 30); pixel(g, 7, 5, 30); pixel(g, 9, 5, 30)   // knuckle ridges
  rectfill(g, 4, 8, 5, 10, 31)                           // thumb
  rectfill(g, 4, 11, 11, 13, 3)                          // dark-green wristband (its cap colour)
  return flat(outlined(g))
}

// ── PICKUPS (slots 8..10) ─────────────────────────────────────────────────────
function ic_health() {                                   // medkit: white box, red cross
  const g = blank()
  rrectfill(g, 3, 4, 10, 10, 2, 7)                       // white box
  rectfill(g, 7, 6, 9, 11, 8)                            // red cross — vertical
  rectfill(g, 5, 8, 11, 9, 8)                            // red cross — horizontal
  pixel(g, 4, 5, 6)                                      // corner shade
  return flat(outlined(g))
}
function ic_ammo() {                                     // a clip of rounds
  const g = blank()
  rectfill(g, 5, 5, 10, 13, 10)                          // yellow magazine
  rectfill(g, 5, 5, 10, 6, 23)                           // lit lip
  pixel(g, 6, 4, 22); pixel(g, 8, 4, 22); pixel(g, 10, 4, 22)  // brass round-tips
  rectfill(g, 5, 8, 10, 8, 25)                           // band
  rectfill(g, 5, 12, 10, 13, 25)                         // dark base
  return flat(outlined(g))
}
// ── END-SCREEN GLYPHS (slots 16..17) — big, centred ───────────────────────────
function big_heart() {
  const g = blank()
  circlefill(g, 5, 6, 2, 8); circlefill(g, 10, 6, 2, 8)  // two humps
  trifill(g, 3, 7, 12, 7, 8, 12, 8)                      // point
  pixel(g, 5, 5, 14)                                     // pink glint
  return flat(outlined(g))
}
function big_skull() {
  const g = blank()
  circlefill(g, 8, 6, 4, 7)                              // cranium
  rectfill(g, 6, 9, 10, 12, 7)                           // jaw
  rectfill(g, 5, 6, 6, 7, 16); rectfill(g, 10, 6, 11, 7, 16)   // eye sockets
  pixel(g, 8, 8, 16)                                     // nose
  pixel(g, 7, 11, 16); pixel(g, 9, 11, 16)               // tooth gaps
  return flat(outlined(g))
}

// ── HUD MINIS (slots 18..20) — ~7px, top-left, for the 16px status strip ───────
function mini_heart() {
  const g = blank()
  rectfill(g, 1, 0, 2, 1, 8); rectfill(g, 4, 0, 5, 1, 8) // two humps
  rectfill(g, 0, 1, 6, 2, 8)                             // shoulders
  rectfill(g, 1, 3, 5, 3, 8)                             // taper
  rectfill(g, 2, 4, 4, 4, 8)
  pixel(g, 3, 5, 8)                                      // point
  pixel(g, 1, 0, 14)                                     // glint
  return flat(g)
}
function mini_clip() {
  const g = blank()
  rectfill(g, 1, 0, 4, 0, 22)                            // brass round-tips
  rectfill(g, 0, 1, 5, 1, 23)                            // lit lip
  rectfill(g, 0, 2, 5, 5, 10)                            // yellow body
  rectfill(g, 0, 4, 5, 4, 25)                            // band
  rectfill(g, 0, 6, 5, 6, 25)                            // dark base
  return flat(g)
}
function mini_skull() {
  const g = blank()
  rectfill(g, 1, 0, 5, 2, 7)                             // cranium
  pixel(g, 1, 1, 16); pixel(g, 5, 1, 16)                 // hollow sides
  rectfill(g, 0, 3, 6, 3, 7)                             // brow
  pixel(g, 1, 3, 16); pixel(g, 5, 3, 16)                 // eye sockets
  rectfill(g, 2, 4, 4, 5, 7)                             // jaw
  pixel(g, 3, 4, 16)                                     // nose
  pixel(g, 2, 6, 7); pixel(g, 4, 6, 7)                   // teeth
  return flat(g)
}

// ── SCENARIO / GAME-MODE ICONS (slots 24..28) — big, for the panel's mode tiles ──
function sc_fight() {                                    // open firefight — two crossed barrels + a clash spark
  const g = blank()
  line(g, 2, 13, 13, 2, 5); line(g, 2, 2, 13, 13, 5)    // two crossed barrels
  rectfill(g, 1, 12, 3, 14, 4); rectfill(g, 1, 1, 3, 3, 4)  // wooden stocks (the two grip ends, left)
  pixel(g, 13, 2, 6); pixel(g, 13, 13, 6)               // bright muzzle tips (right)
  circlefill(g, 8, 8, 1.6, 9); pixel(g, 8, 8, 10)       // muzzle-clash spark
  return flat(outlined(g))
}
function sc_sneak() {                                    // stealth — two stepping footprints
  const g = blank()
  rectfill(g, 4, 4, 6, 7, 6); pixel(g, 4, 3, 6); pixel(g, 5, 3, 6); pixel(g, 4, 8, 5)   // foot 1: sole + toes + heel
  rectfill(g, 9, 9, 11, 12, 6); pixel(g, 9, 8, 6); pixel(g, 10, 8, 6); pixel(g, 9, 13, 5)  // foot 2, stepped forward
  return flat(g)
}
function sc_alarm() {                                    // the squad's searching — an alarm bell
  const g = blank()
  rectfill(g, 5, 6, 11, 10, 10); rectfill(g, 6, 4, 10, 6, 10)  // body + dome
  pixel(g, 8, 3, 5)                                     // top knob
  rectfill(g, 4, 10, 12, 11, 10)                        // flared rim
  rectfill(g, 7, 12, 9, 13, 5)                          // clapper
  line(g, 2, 4, 3, 6, 7); line(g, 14, 4, 13, 6, 7)      // ring lines
  return flat(outlined(g))
}
function sc_ambush() {                                   // they know — arrows converge on you
  const g = blank()
  rectfill(g, 7, 7, 9, 9, 8)                            // you, centre
  trifill(g, 0, 5, 0, 11, 4, 8, 9)                      // ← arrows in from 4 sides
  trifill(g, 15, 5, 15, 11, 11, 8, 9)
  trifill(g, 5, 0, 11, 0, 8, 4, 9)
  trifill(g, 5, 15, 11, 15, 8, 11, 9)
  return flat(outlined(g))
}
function sc_brawl() {                                    // melee rush — two fists clash
  const g = blank()
  rrectfill(g, 1, 6, 5, 10, 1, 31); rrectfill(g, 10, 6, 14, 10, 1, 31)  // fists
  pixel(g, 2, 6, 30); pixel(g, 4, 6, 30); pixel(g, 11, 6, 30); pixel(g, 13, 6, 30)  // knuckles
  pixel(g, 8, 8, 10)                                    // impact star
  pixel(g, 7, 7, 9); pixel(g, 9, 7, 9); pixel(g, 7, 9, 9); pixel(g, 9, 9, 9)
  pixel(g, 8, 5, 9); pixel(g, 8, 11, 9)
  return flat(outlined(g))
}

// ── DIFFICULTY-SLIDER ICONS (slots 32..38) — small ~7px, top-left, one per slider ─
function sl_damage() {                                   // hit burst
  const g = blank()
  pixel(g, 3, 0, 9); pixel(g, 3, 6, 9); pixel(g, 0, 3, 9); pixel(g, 6, 3, 9)  // spikes
  pixel(g, 1, 1, 9); pixel(g, 5, 1, 9); pixel(g, 1, 5, 9); pixel(g, 5, 5, 9)  // diagonals
  rectfill(g, 2, 2, 4, 4, 8); pixel(g, 3, 3, 10)        // red core, hot centre
  return flat(g)
}
function sl_rate() {                                     // three rounds = rate of fire
  const g = blank()
  rectfill(g, 0, 1, 3, 1, 10); pixel(g, 4, 1, 9)
  rectfill(g, 0, 3, 3, 3, 10); pixel(g, 4, 3, 9)
  rectfill(g, 0, 5, 3, 5, 10); pixel(g, 4, 5, 9)
  return flat(g)
}
function sl_accuracy() {                                 // crosshair
  const g = blank()
  pixel(g, 3, 0, 6); pixel(g, 3, 1, 6); pixel(g, 3, 5, 6); pixel(g, 3, 6, 6)
  pixel(g, 0, 3, 6); pixel(g, 1, 3, 6); pixel(g, 5, 3, 6); pixel(g, 6, 3, 6)
  pixel(g, 3, 3, 8)                                     // red centre dot
  return flat(g)
}
function sl_sight() {                                    // an eye
  const g = blank()
  ovalfill(g, 3, 3, 3, 2, 7)                            // white of the eye
  pixel(g, 2, 3, 12); pixel(g, 3, 3, 12); pixel(g, 4, 3, 12)  // iris
  pixel(g, 3, 3, 1)                                     // pupil
  return flat(outlined(g))
}
function sl_supp() {                                     // pinned-down arrow
  const g = blank()
  rectfill(g, 2, 0, 4, 3, 9)                            // shaft
  trifill(g, 0, 3, 6, 3, 3, 6, 9)                       // head, pointing down
  return flat(g)
}
function sl_ammo() {                                     // a clip
  const g = blank()
  rectfill(g, 1, 0, 4, 0, 22); rectfill(g, 0, 1, 5, 1, 23)  // round-tips + lip
  rectfill(g, 0, 2, 5, 5, 10); rectfill(g, 0, 4, 5, 4, 25); rectfill(g, 0, 6, 5, 6, 25)
  return flat(g)
}
function sl_heal() {                                     // a red cross
  const g = blank()
  rectfill(g, 2, 0, 4, 6, 8); rectfill(g, 0, 2, 6, 4, 8)
  pixel(g, 3, 3, 7)                                     // white pip
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: {
    0: w_smg(), 1: w_shotgun(), 2: w_marksman(), 3: w_knife(), 4: w_brawl(),
    8: ic_health(), 9: ic_ammo(),
    16: big_heart(), 17: big_skull(),
    18: mini_heart(), 19: mini_clip(), 20: mini_skull(),
    24: sc_fight(), 25: sc_sneak(), 26: sc_alarm(), 27: sc_ambush(), 28: sc_brawl(),
    32: sl_damage(), 33: sl_rate(), 34: sl_accuracy(), 35: sl_sight(),
    36: sl_supp(), 37: sl_ammo(), 38: sl_heal(),
  },
}
