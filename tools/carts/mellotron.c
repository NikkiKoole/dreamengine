#include "studio.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>

// ── MELLOTRON ────────────────────────────────────────────────────────────────
// The real thing IS tape: every key drags a strip of pre-recorded tape across a
// playback head — a real strings/choir/flute/brass section, captured once in 1963.
// We have NO sample playback (code-first synthesis, on purpose), so we FAKE the
// recording: each VOICE is a drawn single-cycle wave (a stack of harmonics via
// wave_set -> INSTR_USER0, the tapeloop trick), and we reconstruct the *cues* the
// ear uses to believe a recording —
//   * THE TAPE ENGAGE   a soft breath of noise + a pitch that settles UP to the note
//                        over ~90 ms (the capstan spinning to speed — not a click)
//   * ENSEMBLE shimmer   chorus() turns one wave into a whole section
//   * MOVEMENT           gentle vibrato + a warm lowpass so it isn't frozen
//   * TAPE + ROOM        tape() wow/flutter/saturation, then reverb()
// and the signature quirk that makes a Mellotron a Mellotron:
//   * THE ~8s TAPE LIMIT each key's tape is FINITE — hold a note and it runs out
//     and stops, even with the key still down. Lift + re-press rewinds the tape.
// LAYERING — a real Mellotron tape frame holds 3 tracks; blend them. Tap the VOICE
// buttons to stack timbres (we SUM the drawn waves into one cycle — additive, one
// voice per note), or hit a COMBI preset. (design: docs/design/recorded-timbres.md.)
//
// Play with the COMPUTER KEYBOARD (GarageBand musical-typing map): white keys on
// the home row A S D F G H J K L ; '  (A = C), black keys on the row above
// W E T Y U O P. Z / X shift the octave. Or use the mouse / multitouch keybed.

#define WOOD     12                       // the wooden end-cheeks of the cabinet
#define KX0      WOOD
#define NWHITE   14                        // two octaves of white keys
#define WKW      ((SCREEN_W - 2 * WOOD) / NWHITE)
#define KEY_TOP  72
#define BLACK_W  14
#define BLACK_H  76

#define SLOT   5                           // the "tape recording" — INSTR_USER0
#define CHIFF  6                           // the attack transient — INSTR_NOISE

static const int wsemi[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int bsemi[7] = { 1, 3, -1, 6, 8, 10, -1 };  // black key right of white i

// ── per-key tape state (two octaves = 24 semitones) ──
#define NSEMI    24
#define NOFINGER (-999)
#define KBD      (-2)                      // s_finger sentinel: held by a COMPUTER key, not a finger
static int   s_handle[NSEMI];
static int   s_finger[NSEMI];              // touch id holding this key, KBD, or NOFINGER
static float s_start [NSEMI];              // now() when this tape started rolling
static bool  s_spent [NSEMI];             // tape ran out — silent until the finger lifts

// the GarageBand musical-typing map (the house layout — docs/guides/cart-authoring.md):
// home row = white keys (A = C), the QWERTY row above = black keys.
static const char gb_wkey[11]  = "ASDFGHJKL;'";
static const int  gb_wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
static const char gb_bkey[7]   = { 'W', 'E', 'T', 'Y', 'U', 'O', 'P' };
static const int  gb_bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
static char  KC[18]; static int KS[18]; static int NK;   // flattened key->semitone map
static char  kbd_char[NSEMI];                            // reverse: semitone -> key label

static int octv = 0;                       // octave offset, Z/X (clamped -2..+2)
static int base_midi(void) { return 60 + octv * 12; }    // C4 at octave 0

// ── the four drawn "recordings" + the active LAYER blend ──
static const char *VNAME[4] = { "STRINGS", "CHOIR", "FLUTE", "BRASS" };
static const int   VCLR [4] = { CLR_BLUE, CLR_PINK, CLR_GREEN, CLR_ORANGE };
static float TBL[4][64];
static float ACT[64];
static int   layer = 1;                    // bitmask of active voices (default = STRINGS)

static const struct { const char *name; int mask; } COMBI[4] = {
    { "STR+CH", 1 | 2 }, { "STR+FL", 1 | 4 }, { "CHO+FL", 2 | 4 }, { "FULL", 1 | 2 | 4 | 8 },
};

static float k_tone = 0.55f;               // TONE knob -> lowpass cutoff
static float k_len  = 1.00f;               // TAPE knob -> loop length (default = 8s)

static float tape_len(void) { return 2.0f + k_len * 6.0f; }     // 2..8 seconds
static int   tone_hz (void) { return 700 + (int)(k_tone * 3300); }  // 700..4000 Hz
static int   first_voice(void) { for (int v = 0; v < 4; v++) if (layer & (1 << v)) return v; return 0; }

static void build_waves(void) {
    for (int i = 0; i < 64; i++) {
        float x = i / 64.0f * 6.2831853f;
        // STRINGS — dense bowed-ensemble harmonics, rolled-off top
        TBL[0][i] = 0.60f*sinf(x) + 0.42f*sinf(2*x) + 0.30f*sinf(3*x) + 0.20f*sinf(4*x)
                  + 0.14f*sinf(5*x) + 0.10f*sinf(6*x) + 0.06f*sinf(7*x);
        // CHOIR — vocal "aah": soft fundamental + a formant bump around the 3rd-4th harmonic
        TBL[1][i] = 0.45f*sinf(x) + 0.28f*sinf(2*x) + 0.40f*sinf(3*x) + 0.42f*sinf(4*x)
                  + 0.22f*sinf(5*x) + 0.10f*sinf(6*x);
        // FLUTE — nearly pure, a touch of 2nd/3rd (breath comes from the chiff + lowpass)
        TBL[2][i] = 0.88f*sinf(x) + 0.16f*sinf(2*x) + 0.07f*sinf(3*x);
        // BRASS — bright sawtooth-ish stack, a formant lift in the upper mids (the blare)
        TBL[3][i] = 0.50f*sinf(x) + 0.40f*sinf(2*x) + 0.34f*sinf(3*x) + 0.30f*sinf(4*x)
                  + 0.26f*sinf(5*x) + 0.20f*sinf(6*x) + 0.14f*sinf(7*x) + 0.10f*sinf(8*x);
    }
}

// blend the active voices into ONE drawn cycle (additive layering) and load it live
static void build_active_wave(void) {
    float peak = 0.0001f;
    for (int i = 0; i < 64; i++) {
        float s = 0;
        for (int v = 0; v < 4; v++) if (layer & (1 << v)) s += TBL[v][i];
        ACT[i] = s;
        float a = s < 0 ? -s : s;
        if (a > peak) peak = a;
    }
    float g = 0.95f / peak;                            // normalize so a blend isn't louder
    for (int i = 0; i < 64; i++) ACT[i] *= g;
    wave_set(0, ACT, 64);
}

void init(void) {
    build_waves();
    NK = 0;                                            // flatten the GB map + build labels
    for (int i = 0; i < 11; i++) { KC[NK] = gb_wkey[i]; KS[NK] = gb_wsemi[i]; kbd_char[gb_wsemi[i]] = gb_wkey[i]; NK++; }
    for (int i = 0; i < 7;  i++) { KC[NK] = gb_bkey[i]; KS[NK] = gb_bsemi[i]; kbd_char[gb_bsemi[i]] = gb_bkey[i]; NK++; }

    // the sustaining tape voice: a soft swelling pad (bloom in, long release out)
    instrument(SLOT, INSTR_USER0, 140, 0, 6, 600);
    instrument_filter(SLOT, FILTER_LOW, tone_hz(), 2);     // warm; TONE knob sweeps this
    instrument_lfo(SLOT, 0, LFO_PITCH, 5.2f, 0.10f);       // gentle player vibrato
    instrument_lfo(SLOT, 1, LFO_CUTOFF, 0.30f, 220.0f);    // slow drift so the timbre isn't frozen
    // the capstan spin-up: each note starts ~18 cents FLAT and settles up to pitch over
    // ~90 ms — the tape getting up to speed. The real Mellotron onset (not a click).
    instrument_env(SLOT, 2, ENV_PITCH, 0, 90, -0.18f);
    instrument_reverb(SLOT, 0.28f);                        // the room it was recorded in
    build_active_wave();

    // the attack "engage": a soft, airy breath of noise (the tape-head pressing on),
    // lowpassed dark so it reads as mechanism, not a percussive click.
    instrument(CHIFF, INSTR_NOISE, 4, 45, 0, 30);
    instrument_filter(CHIFF, FILTER_LOW, 1200, 1);

    chorus(0.55f, 0.50f, 0.45f);           // ENSEMBLE — one wave reads as a section
    reverb(0.55f, 0.45f);
    tape(0.65f, 0.45f, 0.50f);             // worn-tape wobble (wow / flutter / saturation) — pronounced
}

// which semitone (0..23) is under canvas point x,y? -1 = none
static int semi_at(int x, int y) {
    if (y < KEY_TOP || y >= SCREEN_H) return -1;
    if (x < KX0 || x >= KX0 + NWHITE * WKW) return -1;
    int wk = (x - KX0) / WKW;
    if (wk < 0) wk = 0;
    if (wk >= NWHITE) wk = NWHITE - 1;
    if (y < KEY_TOP + BLACK_H) {                   // black row: check both neighbours
        for (int k = wk - 1; k <= wk; k++) {
            if (k < 0 || k >= NWHITE) continue;
            int pos = k % 7;
            if (bsemi[pos] < 0) continue;
            int bx = KX0 + (k + 1) * WKW - BLACK_W / 2;
            if (x >= bx && x < bx + BLACK_W) return (k / 7) * 12 + bsemi[pos];
        }
    }
    return (wk / 7) * 12 + wsemi[wk % 7];
}

// start a key's tape rolling: the chiff onset + the sustaining blended voice
static void key_on(int k, int id) {
    int midi = base_midi() + k;
    hit(midi, CHIFF, 2, 36);                        // soft breath as the tape-head engages
    s_handle[k] = note_on(midi, SLOT, 6);
    s_finger[k] = id;
    s_start [k] = now();
    s_spent [k] = false;
}

static void key_off(int k) {
    note_off(s_handle[k]);
    s_finger[k] = NOFINGER;
    s_spent [k] = false;                            // lifting rewinds the tape
}

void update(void) {
    static bool booted = false;
    if (!booted) { for (int k = 0; k < NSEMI; k++) s_finger[k] = NOFINGER; booted = true; }

    // live fingers: claim keys / glissando (finger slid to a different key)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        int s  = semi_at(touch_x(i), touch_y(i));
        int cur = -1;
        for (int k = 0; k < NSEMI; k++) if (s_finger[k] == id) { cur = k; break; }
        if (s == cur) continue;                     // same key (or still off-keys)
        if (cur >= 0) key_off(cur);                 // slid off its old key
        if (s >= 0 && s_finger[s] == NOFINGER) key_on(s, id);
    }

    // lifted fingers: release exactly the keys those fingers held
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        for (int k = 0; k < NSEMI; k++) if (s_finger[k] == id) key_off(k);
    }

    // computer keyboard (GarageBand map): press starts a tape, release rewinds it
    // (KBD keys don't steal a finger-held key, and a finger-held key isn't released by keyr)
    for (int i = 0; i < NK; i++) {
        int s = KS[i];
        if (keyp(KC[i]) && s_finger[s] == NOFINGER) key_on(s, KBD);
        if (keyr(KC[i]) && s_finger[s] == KBD)       key_off(s);
    }
    if (keyp('Z') && octv > -2) octv--;             // octave down / up
    if (keyp('X') && octv <  2) octv++;

    // THE 8-SECOND TAPE LIMIT — held notes run out of tape and stop on their own
    float len = tape_len();
    for (int k = 0; k < NSEMI; k++)
        if (s_finger[k] != NOFINGER && !s_spent[k] && now() - s_start[k] >= len) {
            note_off(s_handle[k]);
            s_spent[k] = true;                      // silent, but the finger still holds it
        }
}

static void set_tone(void) {
    int hz = tone_hz();
    instrument_filter(SLOT, FILTER_LOW, hz, 2);
    for (int k = 0; k < NSEMI; k++)                 // sweep the ringing notes too
        if (s_finger[k] != NOFINGER && !s_spent[k]) note_cutoff(s_handle[k], hz);
}

// draw one key with its remaining-tape fill (drains from the top as tape is used)
static void draw_key(int x, int y, int w, int h, int k, bool black) {
    bool held = s_finger[k] != NOFINGER;
    int base = black ? CLR_DARK_GREY : CLR_WHITE;
    if (!held) {
        rectfill(x, y, w, h, base);
    } else if (s_spent[k]) {
        rectfill(x, y, w, h, CLR_BROWNISH_BLACK);   // tape ran out
    } else {
        float used = (now() - s_start[k]) / tape_len();
        if (used < 0) used = 0; if (used > 1) used = 1;
        rectfill(x, y, w, h, VCLR[first_voice()]);  // remaining tape = the (lead) voice colour
        rectfill(x, y, w, (int)(used * h), CLR_BROWNISH_BLACK);  // used tape spools away
    }
    rect(x, y, w, h, CLR_DARK_BROWN);
    if (kbd_char[k]) {                              // the computer key that plays this note
        char t[2] = { kbd_char[k], 0 };
        int col = held ? CLR_LIGHT_PEACH : (black ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
        print(t, x + (w - text_width(t)) / 2, y + h - (black ? 11 : 20), col);
    }
}

void draw(void) {
    cls(CLR_DARK_BROWN);                            // the wooden cabinet

    // ── wooden end-cheeks ──
    rectfill(0, 0, WOOD, SCREEN_H, CLR_BROWN);
    rectfill(SCREEN_W - WOOD, 0, WOOD, SCREEN_H, CLR_BROWN);
    line(WOOD - 1, 0, WOOD - 1, SCREEN_H, CLR_BROWNISH_BLACK);
    line(SCREEN_W - WOOD, 0, SCREEN_W - WOOD, SCREEN_H, CLR_BROWNISH_BLACK);

    // ── cream control panel ──
    rectfill(WOOD, 0, SCREEN_W - 2 * WOOD, KEY_TOP, CLR_LIGHT_PEACH);
    line(WOOD, KEY_TOP - 1, SCREEN_W - WOOD, KEY_TOP - 1, CLR_DARK_PEACH);

    // logo plate
    rectfill(16, 4, 92, 12, CLR_BROWNISH_BLACK);
    print("MELLOTRON", 20, 6, CLR_LIGHT_PEACH);

    ui_begin();
    // ── VOICE toggles (row 1) — tap to stack timbres into the blend ──
    static const int vx[4] = { 16, 72, 120, 168 }, vw[4] = { 52, 44, 44, 44 };
    for (int i = 0; i < 4; i++) {
        if (ui_button(vx[i], 18, vw[i], 14, VNAME[i])) {
            layer ^= (1 << i);
            if (layer == 0) layer = (1 << i);       // never empty
            build_active_wave();
        }
        if (layer & (1 << i)) rectfill(vx[i], 33, vw[i], 2, VCLR[i]);  // active marker
    }
    // ── COMBI presets (row 2) ──
    static const int cx[4] = { 16, 70, 124, 178 }, cw[4] = { 50, 50, 50, 40 };
    for (int i = 0; i < 4; i++)
        if (ui_button(cx[i], 38, cw[i], 13, COMBI[i].name)) { layer = COMBI[i].mask; build_active_wave(); }

    // ── TONE / TAPE knobs (top-right) ──
    font(FONT_SMALL);
    if (ui_knob(&k_tone, 256, 18, "TONE")) set_tone();
    ui_knob(&k_len, 292, 18, "TAPE");
    char buf[16];
    snprintf(buf, sizeof buf, "%.0fs", tape_len());
    print(buf, 292 - text_width(buf) / 2, 38, CLR_DARK_BROWN);
    print("A S D F.. = keys   Z X = octave", 16, 60, CLR_DARK_PEACH);
    font(FONT_NORMAL);
    ui_end();

    // ── keyboard ──
    for (int k = 0; k < NWHITE; k++) {              // white keys
        int s = (k / 7) * 12 + wsemi[k % 7];
        draw_key(KX0 + k * WKW + 1, KEY_TOP, WKW - 1, SCREEN_H - KEY_TOP, s, false);
        if (k % 7 == 0) {
            char lbl[5]; snprintf(lbl, sizeof lbl, "C%d", base_midi() / 12 - 1 + k / 7);
            print(lbl, KX0 + k * WKW + 4, SCREEN_H - 11, CLR_MEDIUM_GREY);
        }
    }
    for (int k = 0; k < NWHITE; k++) {              // black keys
        int pos = k % 7;
        if (bsemi[pos] < 0) continue;
        int s = (k / 7) * 12 + bsemi[pos];
        draw_key(KX0 + (k + 1) * WKW - BLACK_W / 2, KEY_TOP, BLACK_W, BLACK_H, s, true);
    }
}
