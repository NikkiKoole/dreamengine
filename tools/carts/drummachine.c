#include "studio.h"

// drum machine — a 16-step sequencer that shows off the sound API.
//
// The whole thing is one idea: a grid of on/off cells. Each ROW is an
// instrument, each of the 16 COLUMNS is a sixteenth-note in the bar. A
// playhead sweeps left-to-right forever; when it lands on a lit cell, that
// row's drum fires.
//
// The clock comes straight from the synth's own beat counter. beat() counts
// quarter-notes at the current bpm, beat_pos() is the fraction inside the
// beat — so beat()*4 + floor(beat_pos()*4) is a sixteenth-note counter. We
// trigger sounds the instant that number changes. No timers of our own.
//
//   WASD       move the cursor
//   Z          toggle the cell under the cursor (and audition it)
//   X          clear the whole grid
//   ← / →      tempo down / up (the arrow keys are "player 1")
//   ↑ / ↓      swing up / down — drags every 2nd 16th late, 0-60%

#define ROWS  6
#define STEPS 16

#define GX 46   // grid left edge
#define GY 28   // grid top edge
#define SX 17   // column stride (px)
#define SY 21   // row stride (px)
#define CW 15   // cell width
#define CH 18   // cell height

static const char *LABEL[ROWS] = { "KICK", "SNARE", "HIHAT", "OHAT", "CLAP", "BASS" };
static const int   LIT  [ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_PINK, CLR_BLUE };

// a starter groove so it sounds good the moment it loads
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // kick — four on the floor
    "....x.......x...",   // snare — backbeat
    "x.x.x.x.x.x.x.x.",   // closed hat — eighths
    "..............x.",   // open hat — lift before the loop
    "................",   // clap — empty, for you to fill in
    "x.....x.x.....x.",   // bass — a little bounce
};

static bool grid[ROWS][STEPS];
static int  curR = 0, curC = 0;       // cursor cell
static int  cur_step = 0;             // column the playhead is on
static int  last_16 = -1;             // last sixteenth-note we triggered on
static int  flash[ROWS];              // frame() when each row last fired
static int  tempo = 120;
static int  swing = 0;                // % of a 16th the off-beats drag (0-60, ↑/↓)

// fire one row's drum — each row plays its own instrument slot (defined in init)
static void play_row(int r) {
    switch (r) {
        case 0: hit(72, INSTR_NOISE, 2, 12);             // kick — click layer...
                hit(34, 5, 6, 250); break;               // ...over the sine boom (pitch env = punch)
        case 1: hit(58, 6, 5, 110);                      // snare — noise bark...
                hit(53, INSTR_TRI, 3, 45); break;        // ...over a tonal body
        case 2: hit(92, 7, 3, 24); break;                // closed hat — high-passed tick
        case 3: hit(92, 8, 2, 170); break;               // open hat — same sizzle, longer
        case 4: hit(64, 9, 4, 35);                       // clap — three bursts, like hands:
                schedule_hit(12, 64, 9, 3, 30);          // sample-accurate echoes of the slap
                schedule_hit(24, 64, 9, 4, 60); break;
        case 5: hit(40, 10, 4, 110); break;              // bass — rounded-off square
    }
}

void init() {
    // the kit lives in instrument slots 5-10, one per row. The kick is the "punch"
    // recipe: a LONG sine whose pitch env does the donk (909 boom), not a short tri
    // tick. The noise rows get per-row filters so the hats sizzle (highpass) and the
    // snare/clap bark (bandpass) instead of all four sharing the same white-noise splat.
    instrument(5, INSTR_SINE,  0, 280, 0, 60);           // kick body
    instrument_env(5, 1, ENV_PITCH, 0, 55, 30);          // starts +30 st, settles in 55ms
    instrument(6, INSTR_NOISE, 0, 130, 0, 50);           // snare rattle
    instrument_filter(6, FILTER_BAND, 1400, 3);
    instrument(7, INSTR_NOISE, 0, 25, 0, 15);            // closed hat
    instrument_filter(7, FILTER_HIGH, 6500, 2);
    instrument(8, INSTR_NOISE, 0, 180, 0, 80);           // open hat
    instrument_filter(8, FILTER_HIGH, 6000, 2);
    instrument(9, INSTR_NOISE, 0, 60, 0, 30);            // one clap burst (play_row fires 3)
    instrument_filter(9, FILTER_BAND, 1100, 5);
    instrument(10, INSTR_SQUARE, 1, 100, 3, 60);         // bass
    instrument_filter(10, FILTER_LOW, 900, 4);

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');
}

void update() {
    bpm(tempo);

    // ── transport: advance the playhead off the synth clock ──
    // swing drags every ODD 16th late by swing% of a step: the counter only ticks
    // over once beat_pos clears the swung onset. Even 16ths (the on-beats) stay straight.
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int   n     = (int)pos16;
    int sixteenth = (n % 2 == 1 && pos16 - n < swing / 100.0f) ? n - 1 : n;
    if (sixteenth != last_16) {
        last_16  = sixteenth;
        cur_step = sixteenth % STEPS;
        for (int r = 0; r < ROWS; r++)
            if (grid[r][cur_step]) { play_row(r); flash[r] = frame(); }
    }

    // ── edit: move cursor (WASD), toggle (Z), clear (X) ──
    if (btnp(0, BTN_UP))    curR = (curR + ROWS - 1) % ROWS;
    if (btnp(0, BTN_DOWN))  curR = (curR + 1) % ROWS;
    if (btnp(0, BTN_LEFT))  curC = (curC + STEPS - 1) % STEPS;
    if (btnp(0, BTN_RIGHT)) curC = (curC + 1) % STEPS;

    if (btnp(0, BTN_A)) {
        grid[curR][curC] = !grid[curR][curC];
        if (grid[curR][curC]) { play_row(curR); flash[curR] = frame(); }  // audition
    }
    if (btnp(0, BTN_B))
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < STEPS; c++) grid[r][c] = false;

    // ── tempo + swing: arrow keys are "player 1" ──
    if (btnp(1, BTN_LEFT))  tempo = max(60,  tempo - 5);
    if (btnp(1, BTN_RIGHT)) tempo = min(240, tempo + 5);
    if (btnp(1, BTN_UP))    swing = min(60, swing + 10);
    if (btnp(1, BTN_DOWN))  swing = max(0,  swing - 10);
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    print("DRUM MACHINE", 2, 4, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM  swing %d%%", tempo, swing), 318, 4, CLR_WHITE);

    // playhead marker above the active column
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, CLR_WHITE);

    for (int r = 0; r < ROWS; r++) {
        // row label, brightened to white for a few frames when it fires
        bool lit = (frame() - flash[r]) < 5;
        print(LABEL[r], 2, GY + r * SY + 6, lit ? CLR_WHITE : LIT[r]);

        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX, y = GY + r * SY;
            if (grid[r][c]) {
                // active cell — full color, white flash on the beat it plays
                rectfill(x, y, CW, CH, LIT[r]);
                if (c == cur_step) rect(x, y, CW, CH, CLR_WHITE);
            } else {
                // empty cell — playhead column lights up grey, downbeats tinted
                int bg = (c == cur_step) ? CLR_DARK_GREY
                       : (c % 4 == 0)    ? CLR_DARKER_BLUE
                                         : CLR_DARKER_GREY;
                rectfill(x, y, CW, CH, bg);
            }
        }
    }

    // cursor — a bright outline around the selected cell
    rect(GX + curC * SX - 1, GY + curR * SY - 1, CW + 2, CH + 2, CLR_GREEN);

    print("WASD move   Z toggle   X clear", 2, GY + ROWS * SY + 6, CLR_LIGHT_GREY);
    print("arrows: tempo + swing", 2, GY + ROWS * SY + 18, CLR_DARK_GREY);
}
