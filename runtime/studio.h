#ifndef STUDIO_H
#define STUDIO_H

#include <stdbool.h>

// ------------------------------------------------------------
// screen
// ------------------------------------------------------------

#ifndef SCREEN_W
#define SCREEN_W  320
#endif
#ifndef SCREEN_H
#define SCREEN_H  200
#endif

// ------------------------------------------------------------
// buttons
// ------------------------------------------------------------

#define BTN_UP     0
#define BTN_DOWN   1
#define BTN_LEFT   2
#define BTN_RIGHT  3
#define BTN_A      4   // z / gamepad a
#define BTN_B      5   // x / gamepad b

// ------------------------------------------------------------
// palette (32 fixed colors, 0-31) — the PICO-8 palette
// names per https://pico-8.fandom.com/wiki/Palette
// ------------------------------------------------------------

// standard 16 (0-15)
#define CLR_BLACK          0   // #000000
#define CLR_DARK_BLUE      1   // #1d2b53
#define CLR_DARK_PURPLE    2   // #7e2553
#define CLR_DARK_GREEN     3   // #008751
#define CLR_BROWN          4   // #ab5236
#define CLR_DARK_GREY      5   // #5f574f
#define CLR_LIGHT_GREY     6   // #c2c3c7
#define CLR_WHITE          7   // #fff1e8
#define CLR_RED            8   // #ff004d
#define CLR_ORANGE         9   // #ffa300
#define CLR_YELLOW         10  // #ffec27
#define CLR_GREEN          11  // #00e436
#define CLR_BLUE           12  // #29adff
#define CLR_INDIGO         13  // #83769c
#define CLR_PINK           14  // #ff77a8
#define CLR_LIGHT_PEACH    15  // #ffccaa

// extended 16 (16-31) — undocumented "secret" colors, named arbitrarily
#define CLR_BROWNISH_BLACK 16  // #291814
#define CLR_DARKER_BLUE    17  // #111d35
#define CLR_DARKER_PURPLE  18  // #422136
#define CLR_BLUE_GREEN     19  // #125359
#define CLR_DARK_BROWN     20  // #742f29
#define CLR_DARKER_GREY    21  // #49333b
#define CLR_MEDIUM_GREY    22  // #a28879
#define CLR_LIGHT_YELLOW   23  // #f3ef7d
#define CLR_DARK_RED       24  // #be1250
#define CLR_DARK_ORANGE    25  // #ff6c24
#define CLR_LIME_GREEN     26  // #a8e72e
#define CLR_MEDIUM_GREEN   27  // #00b543
#define CLR_TRUE_BLUE      28  // #065ab5
#define CLR_MAUVE          29  // #754665
#define CLR_DARK_PEACH     30  // #ff6e59
#define CLR_PEACH          31  // #ff9d81

// ------------------------------------------------------------
// callbacks — you implement these (update is optional)
// ------------------------------------------------------------

void draw(void);
void update(void);

// ------------------------------------------------------------
// api
// ------------------------------------------------------------

// input
bool btn(int player, int button);   // true while button is held
bool btnp(int player, int button);  // true only on the frame the button was pressed

// touch
int  touch_count(void);                  // active touches (0..10)
int  touch_x(int i);                     // canvas-space x, or -1 if i invalid
int  touch_y(int i);                     // canvas-space y, or -1
bool tap(int x, int y, int w, int h);    // any touch inside this canvas-space rect?
void touch_controls(bool on);            // show/hide on-screen stick + A/B (overrides default)

// analog stick (only nonzero while a finger is on the on-screen stick)
float stick_x(void);   // -1.0 .. 1.0
float stick_y(void);   // -1.0 .. 1.0  (negative = up)

// graphics
void cls(int color);
void spr(int index, int x, int y);
void sprf(int index, int x, int y, bool flip_x, bool flip_y);                  // sprite with flips
void sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);     // sub-rect → dest rect (stretched)
void print(const char *text, int x, int y, int color);
void line(int x1, int y1, int x2, int y2, int color);
void pset(int x, int y, int color);                     // set a single pixel (pairs with pget)
void rect(int x, int y, int w, int h, int color);       // rectangle border
void rectfill(int x, int y, int w, int h, int color);   // filled rectangle
void circ(int x, int y, int radius, int color);         // circle border
void circfill(int x, int y, int radius, int color);     // filled circle
int  pget(int x, int y);                                // palette index at (x,y), or 0 if no match
void camera(int x, int y);                              // shifts all subsequent drawing by (-x,-y). camera(0,0) resets
void clip(int x, int y, int w, int h);                  // scissor rect. clip(0,0,0,0) disables

// ------------------------------------------------------------
// map — 128 × 64 grid of sprite indices. cell value 0 = empty.
// ------------------------------------------------------------

#ifndef MAP_W
#define MAP_W  128
#endif
#ifndef MAP_H
#define MAP_H   64
#endif

int  mget(int cx, int cy);                              // sprite index at cell (cx,cy); 0 if out of bounds
void mset(int cx, int cy, int n);                       // write sprite index n into cell (cx,cy)
void map(int cx, int cy, int sx, int sy, int cw, int ch); // draw a cw×ch chunk of the map starting at cell (cx,cy) to screen (sx,sy). cells with value 0 are skipped. respects camera().

// ------------------------------------------------------------
// sound — PICO-8-style 4-channel synth
// ------------------------------------------------------------

#define INSTR_SQUARE  0
#define INSTR_SAW     1
#define INSTR_TRI     2
#define INSTR_NOISE   3
#define INSTR_SINE    4

void sfx(int n);                              // play sfx slot n; -1 stops all sfx
void music(int n);                            // play music pattern n; -1 stops music
void note(int midi, int instr, int vol);                  // one-shot note (250ms). vol 0..7
void hit(int midi, int instr, int vol, int dur_ms);       // note with custom duration — closed hihat ~30ms, open ~200ms

// musical scales (C root)
#define SCALE_MAJOR      0   // do re mi fa sol la ti
#define SCALE_MINOR      1   // natural minor
#define SCALE_PENTA      2   // major pentatonic — always pretty
#define SCALE_PENTA_MIN  3   // minor pentatonic — bluesy, moody
#define SCALE_BLUES      4   // minor penta + flat 5
#define SCALE_CHROMATIC  5   // all 12 notes

void tone(int scale, int octave, int instr, int vol);  // play a random note from scale

// chord types — pass to chord()
#define CHORD_MAJ    0   // major triad
#define CHORD_MIN    1   // minor triad
#define CHORD_DIM    2   // diminished
#define CHORD_AUG    3   // augmented
#define CHORD_MAJ7   4   // major 7th
#define CHORD_MIN7   5   // minor 7th
#define CHORD_DOM7   6   // dominant 7th
#define CHORD_SUS4   7   // suspended 4th
#define CHORD_POWER  8   // power chord (root + fifth)

void chord(int root, int type, int instr, int vol);   // play a chord stacked on root (MIDI note)
void strum(int root, int type, int instr, int vol, int delay_ms);  // chord with delay between notes; negative = down-strum

// musical timing
void  schedule(int delay_ms, int midi, int instr, int vol);  // play a note in the future
void  bpm(int rate);                                          // set tempo (default 120)
int   beat(void);                                             // current beat counter (advances based on bpm)
float beat_pos(void);                                         // fractional position within current beat: 0.0 → 1.0
bool  every(int n);                                           // true once per n beats — fire it on the beat
bool  euclid(int hits, int steps, int b);                    // Bjorklund: true if beat b is one of `hits` evenly across `steps`
bool  chance(int percent);                                    // true with the given probability (0–100)
int   degree(int scale, int octave, int n);                  // MIDI note for the nth degree of a scale (n can wrap octaves)

// ------------------------------------------------------------
// utility
// ------------------------------------------------------------

int   rnd(int n);          // random int in [0, n); rnd(6) → 0..5
float now(void);           // seconds since startup
int   sgn(int n);          // -1 if n<0, 0 if n==0, 1 if n>0
int   mid(int a, int b, int c);  // middle value of three. use for clamp: mid(lo, val, hi)

#endif // STUDIO_H
