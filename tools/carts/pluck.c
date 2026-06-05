// pluck — INSTR_PLUCK showcase: eight strings + the three engine macros.
//
// The first modeled ENGINE (wave ids 16+): instead of reading a wavetable, every note
// excites a little string simulation (Karplus-Strong — a delay line + feedback), so what
// you hear is BEHAVIOR: the string decays and darkens on its own, like a real instrument.
// Every engine answers the same three 0..1 macro knobs — the API never grows:
//   harmonics = ring time        (0 dead slap   .. 1 near-endless sustain)
//   timbre    = pick brightness  (0 felt thud   .. 1 hard bright pick)
//   morph     = pick position    (0 by the bridge = full .. 1 mid-string = hollow)
//
// controls: A S D F G H J K  pluck a string   ·   click a string to pluck it
//           drag a slider with the mouse (auditions as you drag), or
//           LEFT/RIGHT pick a knob + UP/DOWN turn it
//           SPACE strum   ·   P autoplay on/off

#include "studio.h"
#include <math.h>

#define I_STR 5
#define NSTR  8

static const char STRKEY[NSTR] = { 'A','S','D','F','G','H','J','K' };
static const char *KNOB_NAME[3] = { "harmonics (ring)", "timbre (pick)", "morph (position)" };
static const char *KNOB_LO[3]   = { "slap",  "felt",   "bridge" };
static const char *KNOB_HI[3]   = { "drone", "bright", "hollow" };

static int   midi_of[NSTR];
static float amp[NSTR];        // visual string vibration, decays each frame
static float vib_ph[NSTR];
static int   pend[NSTR];       // frames until a scheduled strum note visually plucks
static float knob[3] = { 0.5f, 0.55f, 0.35f };
static int   sel = 0;
static int   drag_k = -1;      // slider currently grabbed by the mouse, -1 = none
static bool  autoplay = true;
static int   apos = 0;

// slider geometry — shared by draw() and the mouse hit-test
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 38)
#define KNOB_X(k) (14 + (k) * 102)

static void apply_knobs(void) {
    instrument_harmonics(I_STR, knob[0]);
    instrument_timbre(I_STR, knob[1]);
    instrument_morph(I_STR, knob[2]);
}

// gate scales with the ring knob — a slap doesn't hold a voice hostage, a drone isn't chopped
static int gate_ms(void) { return 800 + (int)(knob[0] * knob[0] * 15000.0f); }

static void pluck_str(int s, int vol) {
    hit(midi_of[s], I_STR, vol, gate_ms());   // long gate — the string decides its own decay
    amp[s]    = 1.0f;
    vib_ph[s] = 0.0f;
}

static void strum_all(void) {
    for (int s = 0; s < NSTR; s++) {
        schedule_hit(s * 45, midi_of[s], I_STR, 5, gate_ms());
        pend[s] = 1 + (s * 45 * 60) / 1000;   // matching visual pluck, frames
    }
}

void init(void) {
    // the engine id is just a waveform — wrap it in a slot like any wave
    instrument(I_STR, INSTR_PLUCK, 1, 0, 7, 80);
    apply_knobs();
    for (int s = 0; s < NSTR; s++) midi_of[s] = degree(SCALE_PENTA, 3, s + 2);
    bpm(96);
    amp[1] = 0.7f; amp[4] = 0.9f; amp[6] = 0.5f;   // a lively first frame
}

void update(void) {
    for (int s = 0; s < NSTR; s++) {
        if (keyp(STRKEY[s])) pluck_str(s, 6);
        if (pend[s] > 0 && --pend[s] == 0) { amp[s] = 1.0f; vib_ph[s] = 0.0f; }
    }

    // knobs: pick with LEFT/RIGHT, turn with UP/DOWN (held); audition every 12 frames
    if (keyp(KEY_LEFT))  sel = (sel + 2) % 3;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 3;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        apply_knobs();
        if (frame() % 12 == 0) pluck_str(4, 5);
    }

    if (keyp(KEY_SPACE)) strum_all();
    if (keyp('P')) autoplay = !autoplay;

    // mouse: grab a slider...
    if (mouse_pressed(MOUSE_LEFT)) {
        for (int k = 0; k < 3; k++)
            if (point_in_box(mouse_x(), mouse_y(), KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18)) {
                drag_k = sel = k;
            }
        // ...or click a string
        if (drag_k < 0)
            for (int s = 0; s < NSTR; s++)
                if (mouse_y() >= 42 + s * 13 - 5 && mouse_y() < 42 + s * 13 + 8) pluck_str(s, 6);
    }
    if (drag_k >= 0) {
        if (mouse_down(MOUSE_LEFT)) {
            knob[drag_k] = clamp((float)(mouse_x() - KNOB_X(drag_k)) / (float)KNOB_W, 0.0f, 1.0f);
            apply_knobs();
            if (frame() % 12 == 0) pluck_str(4, 5);   // audition while dragging
        } else drag_k = -1;
    }

    // autoplay: a wandering pentatonic noodle, strum at the top of every 16 beats
    if (autoplay && every(1)) {
        static const int seq[16] = { 4,2,5,4, 7,5,4,2, 0,2,4,5, 4,2,1,0 };
        if (beat() % 16 == 0) strum_all();
        else {
            pluck_str(seq[apos % 16], 5);
            if (chance(35)) schedule_hit(310, midi_of[(seq[apos % 16] + 2) % NSTR], I_STR, 3, 2000);
        }
        apos++;
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("PLUCK", 8, 6, CLR_PEACH);
    font(FONT_SMALL);
    print("karplus-strong string engine", 56, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "P autoplay: on" : "P autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the strings
    for (int s = 0; s < NSTR; s++) {
        int y = 42 + s * 13;
        amp[s] *= 0.94f;
        vib_ph[s] += 0.55f;
        int col = amp[s] > 0.5f ? CLR_LIGHT_YELLOW : amp[s] > 0.1f ? CLR_PEACH : CLR_DARK_BROWN;
        int x0 = 34, x1 = SCREEN_W - 16, px = x0, py = y;
        for (int x = x0 + 8; x <= x1; x += 8) {
            float t  = (float)(x - x0) / (float)(x1 - x0);
            int   wy = y + (int)(amp[s] * 5.0f * sinf(t * 9.42f + vib_ph[s]) * sinf(t * 3.14f));
            line(px, py, x, wy, col);
            px = x; py = wy;
        }
        print(str("%c", STRKEY[s]), 12, y - 3, amp[s] > 0.1f ? CLR_WHITE : CLR_MEDIUM_GREY);
        rect(8, y - 6, 14, 13, CLR_DARK_BROWN);
    }

    // the three macro knobs
    for (int k = 0; k < 3; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(KNOB_NAME[k], x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 7, knob[k], on ? CLR_ORANGE : CLR_BROWN, CLR_DARKER_GREY);
        font(FONT_TINY);
        print(KNOB_LO[k], x, y + 9, CLR_DARK_GREY);
        print_right(KNOB_HI[k], x + KNOB_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    print("A..K pluck   click a string   drag a slider (or arrows)   SPACE strum", 10, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
