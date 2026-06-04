#include "studio.h"
#include <stdio.h>

// ── TR-808 RHYTHM COMPOSER ────────────────────────────────────────────────
// The big one. Roland's TR-808 (1980) — commercial flop, then the foundation
// of hip-hop, electro and house ("Planet Rock", "Sexual Healing", and the
// boom in every trunk since). Sibling cart to cr78.c (same editable grid,
// same swing knob); where the CR-78 is soft and round, the 808 is punchy and
// boomy. All-analog, and unlike the CR-78 it's been reverse-engineered to
// component level, so most voice values here are MEASURED, not guessed:
//
//   the metal bank — hats, cymbal and cowbell all share ONE bank of six
//     Schmitt-trigger square oscillators at 800 / 540 / 522.7 / 369.6 /
//     304.4 / 205.3 Hz. Hats = bank through a ~7kHz highpass (closed ~50ms,
//     open up to 600ms — same circuit, different decay). Cymbal = bank
//     through bandpasses at 7100/3440Hz, ringing up to 1.2s. Cowbell = just
//     oscillators 1+2 (540+800Hz) through a ~2.6kHz bandpass. We fire 2-3
//     squares per hit (full six would eat the 8-voice polyphony).
//   kick — bridged-T damped sine ~50Hz with the famous DECAY knob boom;
//     here: low sine, +26 semitone pitch drop over 50ms, ~500ms tail.
//   snare — two bridged-T modes at 180Hz + 330Hz under highpassed
//     "snappy" noise. Three layers, just like the schematic.
//   rimshot / claves — the same dual bridged-T circuit, switched: rimshot
//     rings 1667Hz + 455Hz; claves retunes it to 2500Hz (MIDI 99 exactly).
//   toms / congas — shared circuits too: toms = sine + a low noise thud,
//     congas = the same sine cleaner, higher and shorter (tunings are the
//     one estimated thing here — the service manual doesn't list them).
//   handclap — bandpassed noise hit by THREE fast retriggers ~10ms apart,
//     then a softer ~140ms "room" tail. The retrigger spacing IS the clap.
//   accent — like the CR-78 cart: flagged steps play louder (orange strip).
//   swing — the 808 never had it either (TR-909 got shuffle in 1983, after
//     the LM-1 proved it). Z/X = our anachronistic knob, 50..66%.
//
// Known infidelity: real 808 closed hats CHOKE the open hat; our one-shot
// notes can't cut each other, so an open hat rings through a closed hit.
//
//   Q W E R T Y U I O P A S D F G H   play each voice by hand
//   LEFT / RIGHT  preset      UP / DOWN  tempo      SPACE  start / stop
//   Z / X  swing down / up
//   MOUSE  click/drag cells to edit, click the strip above for accents,
//          click a label to audition

// voices — named indices, never raw numbers (house rule)
enum {
    V_BD, V_SD, V_LT, V_MT, V_HT, V_LC, V_MC, V_HC,
    V_RS, V_CLV, V_CP, V_MA, V_CB, V_CY, V_OH, V_CH, NV
};

// instrument slots (5..8 are user waves; 9..24 ours)
enum {
    SL_BD = 9, SL_SDB, SL_SDN, SL_TOM, SL_TOMN, SL_CON, SL_RS, SL_CLV,
    SL_CP, SL_MAR, SL_CB, SL_CYT, SL_HATO, SL_HATC
};

static const char *VNAME[NV] = {
    "BASS",  "SNARE", "LO TOM", "MD TOM", "HI TOM", "LO CGA", "MD CGA",
    "HI CGA", "RIM",  "CLAVES", "CLAP",  "MARACAS", "COWBELL", "CYMBAL",
    "OPEN HH", "CLSD HH"
};
static const char VKEY[NV] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', 'A', 'S', 'D', 'F', 'G', 'H'
};

#define STEPS 16

#define GX   88    // grid left edge
#define GY   39    // grid top edge
#define SX   13    // column stride
#define SY   9     // row stride

// the iconic step-button colors: steps 1-4 red, 5-8 orange, 9-12 yellow, 13-16 white
static const int QCLR[4] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_WHITE };

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars, 'x' = hit; NULL = silent row
    const char *accent;    // 16 chars, 'x' = +2 volume on that step
    int         swing;     // 50 = straight .. 66 = triplet (0 = straight)
} Pat;

static const Pat PRESET[] = {
    { "PLANET ROCK", 126, {        // the electro blueprint
        [V_BD]  = "x.......x..x....",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x...",
        [V_OH]  = "..............x.",
        [V_CLV] = "..x..x....x.....",
        [V_CB]  = "..........x.....",
      }, "x.......x......." },
    { "HEALING", 94, {             // slow-jam 808: rim, clap, congas, open hat
        [V_BD]  = "x.....x.x.......",
        [V_RS]  = "....x.......x...",
        [V_CP]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x.x.",
        [V_OH]  = "..x.............",
        [V_HC]  = "..........x..x..",
      }, "x.......x......." },
    { "HOUSE", 122, {              // four on the floor, hats off the beat
        [V_BD]  = "x...x...x...x...",
        [V_CP]  = "....x.......x...",
        [V_CH]  = "x...x...x...x...",
        [V_OH]  = "..x...x...x...x.",
        [V_MA]  = "xxxxxxxxxxxxxxxx",
      }, "x.......x......." },
    { "ELECTRO", 110, {
        [V_BD]  = "x..x......x.x...",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x.x.",
        [V_CB]  = "..x.......x.....",
        [V_HT]  = ".............x..",
        [V_MT]  = "..............x.",
        [V_LT]  = "...............x",
      }, "x.......x......." },
    { "BOOM BAP", 90, {            // long kick boom, lazy swing
        [V_BD]  = "x......x..x.....",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x.x.",
        [V_OH]  = "..............x.",
      }, "....x.......x...", 56 },
    { "COWBELL JAM", 115, {        // needs more
        [V_CB]  = "x..x..x.x..x..x.",
        [V_BD]  = "x.......x.......",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "..x...x...x...x.",
        [V_CLV] = "......x.......x.",
      }, "x.......x......." },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 0;      // start on PLANET ROCK
static int  tempo    = 126;
static bool running  = true;
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];
static bool grid[NV][STEPS];   // the live pattern — editable, loaded from preset
static bool gacc[STEPS];       // live accent row
static bool paint_val;         // what a click-drag writes (set on press)
static int  swing = 50;        // 50 = straight .. 66 = full triplet

static void load_preset(void) {
    const Pat *p = &PRESET[pre];
    for (int v = 0; v < NV; v++)
        for (int s = 0; s < STEPS; s++)
            grid[v][s] = p->row[v] && p->row[v][s] == 'x';
    for (int s = 0; s < STEPS; s++)
        gacc[s] = p->accent && p->accent[s] == 'x';
    tempo = p->tempo;
    swing = p->swing ? p->swing : 50;
    bpm(tempo);
}

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice `delay` ms from now. Layered hits follow the schematic:
// metal voices = members of the six-oscillator bank (MIDI 79=800Hz, 73=540,
// 72=522.7, 66=369.6, 63=304.4, 56=205.3), snare = 180+330Hz modes + noise.
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_BD:  // ~50Hz bridged-T with the decay-knob boom
        schedule_hit(delay, 31, SL_BD, vv(6, boost), 500);
        break;
    case V_SD:  // 180Hz + 330Hz modes under snappy noise
        schedule_hit(delay, 54, SL_SDB, vv(4, boost), 110);
        schedule_hit(delay, 64, SL_SDB, vv(3, boost), 110);
        schedule_hit(delay, 60, SL_SDN, vv(4, boost), 140);
        break;
    case V_LT: case V_MT: case V_HT: {  // sine drop + low noise thud
        int m = v == V_LT ? 40 : v == V_MT ? 45 : 52;
        schedule_hit(delay, m, SL_TOM, vv(4, boost), 280);
        schedule_hit(delay, 60, SL_TOMN, vv(2, boost), 30);
        break;
    }
    case V_LC: case V_MC: case V_HC: {  // same circuit, cleaner + shorter
        int m = v == V_LC ? 52 : v == V_MC ? 57 : 63;
        schedule_hit(delay, m, SL_CON, vv(4, boost), 160);
        break;
    }
    case V_RS:  // dual bridged-T: 1667Hz (midi 92) + 455Hz (midi 70)
        schedule_hit(delay, 92, SL_RS, vv(4, boost), 50);
        schedule_hit(delay, 70, SL_RS, vv(3, boost), 50);
        break;
    case V_CLV: // the rimshot circuit retuned to 2500Hz = midi 99 exactly
        schedule_hit(delay, 99, SL_CLV, vv(4, boost), 45);
        break;
    case V_CP:  // three retriggers ~10ms apart + a soft room tail
        schedule_hit(delay,      60, SL_CP, vv(4, boost), 12);
        schedule_hit(delay + 10, 60, SL_CP, vv(4, boost), 12);
        schedule_hit(delay + 20, 60, SL_CP, vv(4, boost), 12);
        schedule_hit(delay + 28, 60, SL_CP, vv(3, boost), 140);
        break;
    case V_MA:  schedule_hit(delay, 90, SL_MAR, vv(3, boost), 30); break;
    case V_CB:  // bank oscillators 1+2: 540Hz + 800Hz
        schedule_hit(delay, 73, SL_CB, vv(4, boost), 220);
        schedule_hit(delay, 79, SL_CB, vv(3, boost), 220);
        break;
    case V_CY:  // three bank members through the high bandpass, long ring
        schedule_hit(delay, 79, SL_CYT, vv(3, boost), 900);
        schedule_hit(delay, 72, SL_CYT, vv(2, boost), 900);
        schedule_hit(delay, 66, SL_CYT, vv(2, boost), 900);
        break;
    case V_OH:  // two bank members, highpassed, long decay
        schedule_hit(delay, 79, SL_HATO, vv(3, boost), 360);
        schedule_hit(delay, 72, SL_HATO, vv(2, boost), 360);
        break;
    case V_CH:  // same two, ~50ms
        schedule_hit(delay, 79, SL_HATC, vv(3, boost), 50);
        schedule_hit(delay, 72, SL_HATC, vv(2, boost), 50);
        break;
    }
}

void init(void) {
    // kick — the boom: low sine, lowpassed, +26st pitch drop over 50ms
    instrument(SL_BD, INSTR_SINE, 0, 480, 0, 60);
    instrument_filter(SL_BD, FILTER_LOW, 250, 3);
    instrument_env(SL_BD, 0, ENV_PITCH, 0, 50, 26.0f);

    // snare body (fired twice: 180Hz + 330Hz modes)
    instrument(SL_SDB, INSTR_SINE, 0, 100, 0, 30);
    instrument_filter(SL_SDB, FILTER_LOW, 1200, 1);
    instrument_env(SL_SDB, 0, ENV_PITCH, 0, 20, 3.0f);
    // snare "snappy" — highpassed noise
    instrument(SL_SDN, INSTR_NOISE, 0, 130, 0, 40);
    instrument_filter(SL_SDN, FILTER_HIGH, 1800, 2);

    // toms — sine with a pitch drop + a separate low noise thud
    instrument(SL_TOM, INSTR_SINE, 0, 260, 0, 50);
    instrument_env(SL_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
    instrument(SL_TOMN, INSTR_NOISE, 0, 28, 0, 12);
    instrument_filter(SL_TOMN, FILTER_BAND, 900, 2);

    // congas — the tom circuit without the noise, shorter
    instrument(SL_CON, INSTR_SINE, 0, 150, 0, 30);
    instrument_env(SL_CON, 0, ENV_PITCH, 0, 25, 4.0f);

    // rimshot — both bridged-T modes through a highpass (keeps 455 AND 1667)
    instrument(SL_RS, INSTR_TRI, 0, 45, 0, 15);
    instrument_filter(SL_RS, FILTER_HIGH, 500, 3);

    // claves — single 2500Hz ping
    instrument(SL_CLV, INSTR_TRI, 0, 40, 0, 14);
    instrument_filter(SL_CLV, FILTER_LOW, 4000, 5);

    // handclap — bandpassed noise; the retriggers in fire() make the clap
    instrument(SL_CP, INSTR_NOISE, 0, 110, 0, 50);
    instrument_filter(SL_CP, FILTER_BAND, 1100, 5);

    // maracas
    instrument(SL_MAR, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(SL_MAR, FILTER_HIGH, 6500, 2);

    // cowbell — square pair through the classic ~2.6kHz bandpass
    instrument(SL_CB, INSTR_SQUARE, 0, 250, 0, 60);
    instrument_filter(SL_CB, FILTER_BAND, 2640, 5);

    // cymbal — bank squares through the 3440Hz region, very long ring
    instrument(SL_CYT, INSTR_SQUARE, 0, 850, 0, 200);
    instrument_filter(SL_CYT, FILTER_HIGH, 3440, 3);

    // hats — bank squares through ~7kHz highpass; open vs closed = decay
    instrument(SL_HATO, INSTR_SQUARE, 0, 340, 0, 90);
    instrument_filter(SL_HATO, FILTER_HIGH, 7000, 3);
    instrument(SL_HATC, INSTR_SQUARE, 0, 42, 0, 16);
    instrument_filter(SL_HATC, FILTER_HIGH, 7000, 3);

    load_preset();
}

// hit-test the step grid: row -1 = accent strip, -2 = miss; col via *col
static int grid_row(int mx, int my, int *col) {
    int s = (mx - GX) / SX;
    if (mx < GX || s < 0 || s >= STEPS) return -2;
    if (my >= GY - 8 && my < GY - 1) { *col = s; return -1; }   // accent strip
    int v = (my - GY) / SY;
    if (my < GY || v < 0 || v >= NV) return -2;
    *col = s;
    return v;
}

void update(void) {
    for (int v = 0; v < NV; v++)
        if (keyp(VKEY[v])) { fire(v, 1, 0); flash[v] = 5; }

    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; }
    if (keyp(KEY_LEFT))  { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; }
    if (keyp(KEY_RIGHT)) { pre = (pre + 1) % NP;      load_preset(); last16 = -1; }
    if (keyp(KEY_UP))   { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }
    if (keyp(KEY_DOWN)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }
    if (keyp('Z')) { swing -= 2; if (swing < 50) swing = 50; }
    if (keyp('X')) { swing += 2; if (swing > 66) swing = 66; }

    // mouse: press toggles a cell (drag paints), label click auditions
    int mc, mr = grid_row(mouse_x(), mouse_y(), &mc);
    if (mouse_pressed(MOUSE_LEFT)) {
        if (mr >= 0) {
            paint_val = !grid[mr][mc];
            grid[mr][mc] = paint_val;
            if (paint_val) { fire(mr, 0, 0); flash[mr] = 5; }
        } else if (mr == -1) {
            paint_val = !gacc[mc];
            gacc[mc] = paint_val;
        } else if (mouse_x() < GX && mouse_y() >= GY && mouse_y() < GY + NV * SY) {
            int v = (mouse_y() - GY) / SY;
            fire(v, 1, 0); flash[v] = 5;
        }
    } else if (mouse_down(MOUSE_LEFT)) {
        if (mr >= 0)       grid[mr][mc] = paint_val;
        else if (mr == -1) gacc[mc]     = paint_val;
    }

    if (!running) return;

    // sixteenth clock off beat(); steps scheduled one ahead, sample-accurate
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (s16 != last16) {
        bool first = (last16 < 0);
        last16  = s16;
        playhead = s16 & 15;

        for (int v = 0; v < NV; v++)
            if (grid[v][playhead]) flash[v] = 5;

        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        int nx      = (s16 + 1) & 15;
        // swing: offbeat 8ths late (the 808 never had this knob either)
        if ((nx & 3) == 2) delay += (swing - 50) * 4 * step_ms / 100;
        int boost   = gacc[nx] ? 2 : 0;
        for (int v = 0; v < NV; v++)
            if (grid[v][nx]) fire(v, boost, delay);

        if (first) {   // fresh start: also sound the step we're already on
            int b0 = gacc[playhead] ? 2 : 0;
            for (int v = 0; v < NV; v++)
                if (grid[v][playhead]) fire(v, b0, 0);
        }
    }
}

void draw(void) {
    const Pat *p = &PRESET[pre];
    char buf[32];

    // black face, thin silver trim — the 808 look
    cls(CLR_BROWNISH_BLACK);
    rectfill(6, 8, 308, 188, CLR_BLACK);
    rect(6, 8, 308, 188, CLR_DARK_GREY);
    line(6, 22, 313, 22, CLR_DARKER_GREY);

    print("RHYTHM COMPOSER TR-808", 14, 13, CLR_LIGHT_GREY);
    sprintf(buf, "SW%2d", swing);
    print(buf, 204, 13, swing > 50 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 248, 13, CLR_LIGHT_PEACH);
    sprintf(buf, "< %s >", p->name);
    print(buf, 14, 24, CLR_ORANGE);
    print(running ? "PLAYING" : "STOPPED", 248, 24, running ? CLR_GREEN : CLR_RED);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 7, SX - 1, NV * SY + 7, CLR_DARKER_GREY);

    // accent strip (clickable)
    for (int s = 0; s < STEPS; s++)
        rectfill(GX + s * SX, GY - 6, SX - 4, 3,
                 gacc[s] ? CLR_ORANGE : CLR_DARKER_GREY);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y, flash[v] > 0 ? CLR_WHITE : CLR_YELLOW);
        print(VNAME[v], 26, y, flash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int s = 0; s < STEPS; s++) {
            int x = GX + s * SX;
            int q = QCLR[s >> 2];   // step-button color by quarter
            if (grid[v][s])
                rectfill(x, y, SX - 4, 7,
                         (flash[v] > 0 && s == playhead && running) ? CLR_WHITE : q);
            else
                rect(x, y, SX - 4, 7, (s & 3) == 0 ? q : CLR_DARKER_GREY);
        }
        if (flash[v] > 0) flash[v]--;
    }

    print("CLICK TO EDIT  <> PRESET  ^v BPM  Z/X SWING", 14, 186, CLR_MEDIUM_GREY);
}
