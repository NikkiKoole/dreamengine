// SMOOCH LOUNGE sprites.
// The dancer is a 16x32 figure split into a TOP slot (head + torso, slots 0-3)
// and a BOTTOM slot (hips + legs, slots 8-11). Slot p and slot p+8 sit in the
// same sheet column on consecutive rows, so a pose is one 16x32 region at
// (p*16, 0) — drawn with a single sspr_ex() (squash + sway). Poses:
//   0 idle   1 shimmy   2 fan-flutter   3 big-finish kick
// Plus: 4 heart note, 5 lips note, 6 kiss/lip-print stamp, 7 crowd silhouette.

// shared head + torso block (rows 0-9) for every pose top
const HEAD = [
    '.......O........',
    '......OkO.......',
    '......kkk.......',
    '.....PPPPP......',
    '....PPpppPP.....',
    '....PpppppP.....',
    '....PpBppBp.....',
    '.....ppppp......',
    '.....pRRRp......',
    '......ppp.......',
].join('\n')

// arm variants (rows 10-15)
const ARMS_IDLE = [
    '....WRRRRRW.....',
    '...WWRRRRRWW....',
    '...WWRRRRRWW....',
    '....WRRRRRW.....',
    '.....RRRRR......',
    '.....RRRRR......',
].join('\n')

const ARMS_SHIMMY = [
    '..W.WRRRRRW.....',
    '..WWWRRRRRWW....',
    '...WWRRRRRWW....',
    '....WRRRRRWW....',
    '.....RRRRR.W....',
    '.....RRRRR......',
].join('\n')

const ARMS_FAN = [
    '.W..RRRRR..W....',
    '.WW.RRRRR.WW....',
    '..WWRRRRRWW.....',
    '...WRRRRRW......',
    '.....RRRRR......',
    '.....RRRRR......',
].join('\n')

const ARMS_KICK = [
    'W....RRRRR....W.',
    'WW...RRRRR...WW.',
    '.WW..RRRRR..WW..',
    '..W..RRRRR..W...',
    '.....RRRRR......',
    '.....RRRRR......',
].join('\n')

const LEGS_IDLE = [
    '....RRRRRR......',
    '...RRRRRRRR.....',
    '...RRRRRRRR.....',
    '..RRRRRRRRRR....',
    '..RRRRRRRRRR....',
    '...RR....RR.....',
    '....pp..pp......',
    '....pp..pp......',
    '....pp..pp......',
    '....pp..pp......',
    '....pp..pp......',
    '....pp..pp......',
    '....pp..pp......',
    '....pp..pp......',
    '...RR....RR.....',
    '..RR......RR....',
].join('\n')

const LEGS_SHIMMY = [
    '...RRRRRR.......',
    '..RRRRRRRR......',
    '..RRRRRRRRR.....',
    '.RRRRRRRRRR.....',
    '..RRRRRRRRR.....',
    '...RR...RR......',
    '...pp...pp......',
    '...pp....pp.....',
    '...pp....pp.....',
    '..pp.....pp.....',
    '..pp......pp....',
    '..pp......pp....',
    '.pp.......pp....',
    '.pp.......pp....',
    'RR.........RR...',
    'RR..........RR..',
].join('\n')

const LEGS_FAN = [
    '....RRRRRR......',
    '...RRRRRRRR.....',
    '...RRRRRRRR.....',
    '..RRRRRRRRRR....',
    '..RRRRRRRRRR....',
    '...RR....RR.....',
    '...pp....pp.....',
    '...pp.....pp....',
    '...pp.....pp....',
    '...pp......pp...',
    '...pp......pp...',
    '..pp.......pp...',
    '..pp........pp..',
    '..pp........pp..',
    '.RR.........RR..',
    'RR...........RR.',
].join('\n')

const LEGS_KICK = [
    '....RRRRRR......',
    '...RRRRRRRR.....',
    '...RRRRRRRR..pp.',
    '..RRRRRRRR.pp...',
    '..RRRRRRppp.....',
    '...RRppp........',
    '...pp...........',
    '...pp...........',
    '...pp...........',
    '...pp...........',
    '...pp...........',
    '...pp...........',
    '...pp...........',
    '...pp...........',
    '..RR............',
    '.RR.............',
].join('\n')

const HEART = [
    '................',
    '...RR....RR.....',
    '..RRRR..RRRR....',
    '.RRRRRRRRRRRR...',
    '.RRRRRRRRRRRR...',
    '.RWRRRRRRRRRR...',
    '.RRRRRRRRRRRR...',
    '..RRRRRRRRRR....',
    '...RRRRRRRR.....',
    '....RRRRRR......',
    '.....RRRR.......',
    '......RR........',
    '................',
    '................',
    '................',
    '................',
].join('\n')

const LIPS = [
    '................',
    '................',
    '................',
    '................',
    '....RR....RR....',
    '...RRRR..RRRR...',
    '..RRRRRRRRRRRR..',
    '..RkRRRRRRRRkR..',
    '..RRRRRRRRRRRR..',
    '...RRPRRPRR.....',
    '....RRRRRR......',
    '................',
    '................',
    '................',
    '................',
    '................',
].join('\n')

const KISS = [
    '................',
    '................',
    '................',
    '.....kk..kk.....',
    '....kkkkkkkk....',
    '...kkkkkkkkkk...',
    '...kkkkkkkkkk...',
    '....kkkkkkkk....',
    '.....kkkkkk.....',
    '......k..k......',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
].join('\n')

const CROWD = [
    '................',
    '................',
    '.....MMMM.......',
    '....MMMMMM......',
    '....MMMMMM......',
    '....MMMMMM......',
    '.....MMMM.......',
    '.....MMMM.......',
    '....MMMMMM......',
    '...MMMMMMMM.....',
    '..MMMMMMMMMM....',
    '..MMMMMMMMMM....',
    '.MMMMMMMMMMMM...',
    '.MMMMMMMMMMMM...',
    'MMMMMMMMMMMMMM..',
    '................',
].join('\n')

const top = (arms) => HEAD + '\n' + arms

module.exports = {
    charMap: { 'M': 28 },   // crowd silhouette uses CLR_TRUE_BLUE as a pal() magic color
    sprites: {
        0: top(ARMS_IDLE),   8:  LEGS_IDLE,
        1: top(ARMS_SHIMMY), 9:  LEGS_SHIMMY,
        2: top(ARMS_FAN),    10: LEGS_FAN,
        3: top(ARMS_KICK),   11: LEGS_KICK,
        4: HEART,
        5: LIPS,
        6: KISS,
        7: CROWD,
    },
}
