#include "studio.h"

// ONDES MARTENOT — the 1928 ribbon synth (Radiohead, film scores, Messiaen's
// Turangalîla). Spiritual upgrade to the theremin: the same eerie continuous voice,
// but with the original's whole control room — a piano, a sliding RIBBON, a
// pressure lever that IS the dynamics, swappable timbres, and four loudspeakers.
//
// ONE sustained voice rings the whole time (note_on at start, never note_off'd).
// Everything else just shapes that one live voice:
//   • la TOUCHE d'intensité (left lever) → note_vol + note_cutoff   the soul: no
//     press, no sound; press harder, it swells AND brightens. THE expression.
//   • le RUBAN (ribbon strip)           → note_pitch   continuous glissando
//   • le CLAVIER (piano)                → note_pitch   tempered, snapped to keys
//   • the TIMBRE stops O/C/G/N          → wave_set     a single cycle you build by
//     adding the lit stops together (onde+gambe+nasillard…), the "drawbar" trick
//   • SOUFFLE (S)                       → a breath-noise layer that tracks intensity
//   • les DIFFUSEURS                    → reverb/chorus sends: the gong-speaker
//     (Métallique) and the lyre-of-strings (Palme) are the instrument's halo
//
// Two hands on a touchscreen: left thumb rides the touche, right finger the ribbon.
// One mouse on desktop: hold on ribbon/keys to sound (swells to the lever's level);
// the lever sets HOW loud. Drag the lever itself for live swells.
//
// EVERY on-screen control is tap/click (stops, diffuseurs, lever, ribbon, keys), so
// the computer keyboard is free to PLAY: the trusted GarageBand layout (A S D F… =
// whites, W E T Y… = blacks, 1.5 octaves), Z/X shift octave, UP/DOWN ride the touche.

#define VSLOT    5          // the main voice (a drawn wave, INSTR_USER0)
#define SSLOT    6          // souffle: a breath-noise bed
#define LO_MIDI  48         // C3 — bottom of ribbon & keyboard
#define SPAN     24         // two octaves up to C5
#define GLIDE_MS 28         // tiny portamento so pitch never zippers

// layout — left drawer (tiroir) of controls, right play surfaces
#define LVX 10
#define LVY 30
#define LVW 24
#define LVH 156            // touche lever travel
#define STX 42             // timbre stop column
#define STW 50
#define STH 16
#define STY 30
#define DFY 138            // diffuseur column
#define KX  102            // play area left
#define KW  274            // play area width (ribbon + keybed share it, aligned)
#define RBY 30
#define RBH 66             // ribbon lane
#define KBY 102
#define KBH 106            // keybed

// ---- timbre stops -------------------------------------------------------------
// O onde (sine) · C creux (hollow, soft-clipped) · G gambe (saw, string) ·
// N nasillard (narrow pulse, nasal). Combine freely — they sum into one cycle.
enum { ST_ONDE, ST_CREUX, ST_GAMBE, ST_NASAL, ST_SOUFFLE, NSTOP };
const char *STOP_LBL[NSTOP] = { "O onde", "C creux", "G gambe", "N nasal", "S souffle" };
bool stop_on[NSTOP] = { true, false, false, false, false };

const char *DIFF_LBL[4] = { "Principal", "Resonance", "Metallique", "Palme" };
int diffuseur = 1;          // start on the spring-reverb speaker — its signature glow

// computer keyboard — the trusted GarageBand musical-typing layout (same as moog/mt70):
// home row A S D F G H J K... = whites, QWERTY row W E T Y U... = blacks. Index = semitone.
// The on-screen stops & diffuseurs are tap/click ONLY (these letters are notes now).
#define NKBKEY 18
const char KBMAP[NKBKEY] = { 'A','W','S','E','D','F','T','G','Y','H','U','J','K','O','L','P',';','\'' };
int kb_oct = 0;             // Z/X octave shift, GarageBand-style (-1..+2)
int kb_semi = -1;           // semitone of the held computer key, or -1

float fracf(float x) { return x - (int)x; }

// Rebuild the single-cycle wave from whichever stops are lit, normalize, push it
// into INSTR_USER0. Called only when the timbre changes — not every frame.
void rebuild_wave(void) {
    static float buf[128];
    const int N = 128;
    float peak = 0.0001f;
    for (int i = 0; i < N; i++) {
        float t = (float)i / N, ph = t * 6.28318f, s = 0;
        if (stop_on[ST_ONDE])  s += sin_deg(t * 360.0f);
        if (stop_on[ST_CREUX]) s += clamp(sin_deg(t * 360.0f) * 1.8f, -0.7f, 0.7f);   // flat-topped = hollow
        if (stop_on[ST_GAMBE]) s += 2.0f * (t - 0.5f);                                // sawtooth
        if (stop_on[ST_NASAL]) s += (fracf(t) < 0.28f) ? 0.9f : -0.9f;                // narrow pulse
        (void)ph;
        buf[i] = s;
        float a = s < 0 ? -s : s;
        if (a > peak) peak = a;
    }
    if (!(stop_on[ST_ONDE] || stop_on[ST_CREUX] || stop_on[ST_GAMBE] || stop_on[ST_NASAL]))
        for (int i = 0; i < N; i++) buf[i] = sin_deg((float)i / N * 360.0f), peak = 1;  // never silent-by-stops
    for (int i = 0; i < N; i++) buf[i] /= peak;
    wave_set(0, buf, N);                                   // 0 → INSTR_USER0
}

// The diffuseurs are the loudspeakers: Principal (dry), Résonance (springs),
// Métallique (a gong as the cone → shimmer), Palme (a lyre of 12 strings → halo).
void apply_diffuseur(void) {
    float rv[4]  = { 0.0f, 0.42f, 0.30f, 0.66f };
    instrument_reverb(VSLOT, rv[diffuseur]);
    instrument_reverb(SSLOT, rv[diffuseur] * 0.6f);
    if (diffuseur == 2)      instrument_chorus(VSLOT, 1.4f, 0.55f, 0.55f);   // metallic shimmer
    else if (diffuseur == 3) instrument_chorus(VSLOT, 0.4f, 0.30f, 0.38f);   // palme glow
    else                     instrument_chorus(VSLOT, 1.0f, 0.0f, 0.0f);     // off
}

int   mainV = -1, souffleV = -1;
float lever = 0.55f;        // touche set-level / ceiling, 0..1 (a playable rest level)
float intens = 0.0f;        // current (slewed) intensity reaching the voice
float curMidi = 60.0f;      // pitch the voice is singing
int   fromRibbon = 0;       // was the last pitch set on the ribbon? (for the readout)

const int WHITE_SEMI[7] = { 0, 2, 4, 5, 7, 9, 11 };
const int HAS_BLACK[7]  = { 1, 1, 0, 1, 1, 1, 0 };   // black key sits after C D _ F G A _

float white_w(void) { return KW / 14.0f; }           // 14 white keys across 2 octaves

// midi → x on the shared ribbon/keyboard axis
float midi_x(float m) { return KX + (m - LO_MIDI) / SPAN * KW; }

// hit-test the piano: blacks (upper strip) first, then whites. -1 = miss.
int key_at(int x, int y) {
    float ww = white_w();
    if (y < KBY + (int)(KBH * 0.58f)) {
        for (int w = 0; w < 13; w++) {
            if (!HAS_BLACK[w % 7]) continue;
            float bx = KX + (w + 1) * ww - ww * 0.3f, bw = ww * 0.6f;
            if (x >= bx && x < bx + bw) {
                int oct = w / 7, deg = w % 7;
                return LO_MIDI + oct * 12 + WHITE_SEMI[deg] + 1;
            }
        }
    }
    int w = (int)((x - KX) / ww);
    if (w < 0 || w > 13) return -1;
    int oct = w / 7, deg = w % 7;
    return LO_MIDI + oct * 12 + WHITE_SEMI[deg];
}

void init(void) {
    rebuild_wave();
    // a holding voice: fast attack so the silent start is instant, long release.
    // we drive loudness live with note_vol/note_cutoff, so sustain sits high.
    instrument(VSLOT, INSTR_USER0, 12, 0, 7, 260);
    instrument_filter(VSLOT, FILTER_LOW, 1200, 4);
    mainV = note_on(60, VSLOT, 0);                 // starts silent, never released
    note_glide(mainV, GLIDE_MS);

    instrument(SSLOT, INSTR_NOISE, 30, 0, 7, 200);
    instrument_filter(SSLOT, FILTER_BAND, 2600, 5);
    souffleV = note_on(60, SSLOT, 0);

    apply_diffuseur();
}

// regions of the play half
enum { R_NONE, R_TOUCHE, R_RIBBON, R_KEYBED };
int region(int x, int y) {
    if (point_in_box(x, y, LVX, LVY, LVW, LVH))     return R_TOUCHE;
    if (point_in_box(x, y, KX, RBY, KW, RBH))       return R_RIBBON;
    if (point_in_box(x, y, KX, KBY, KW, KBH))       return R_KEYBED;
    return R_NONE;
}
float lever_from_y(int y) { return clamp(1.0f - (float)(y - LVY) / LVH, 0, 1); }

void set_pitch_from(int region_kind, int x) {
    if (region_kind == R_RIBBON) {
        curMidi = LO_MIDI + clamp((float)(x - KX) / KW, 0, 1) * SPAN;   // continuous
        fromRibbon = 1;
    } else {
        int m = key_at(x, KBY + KBH / 2);                               // snap to a key
        if (m >= 0) { curMidi = m; fromRibbon = 0; }
    }
}

void toggle_stop(int s) { stop_on[s] = !stop_on[s]; if (s != ST_SOUFFLE) rebuild_wave(); }

void update(void) {
    int touchePtr = 0;     // is some contact riding the touche lever right now?
    int pitchPtr  = 0;     // is some contact on a pitch surface right now?

    // ---- computer keyboard: GarageBand musical typing = the CLAVIER (tempered) ----
    // (stops & diffuseurs are tap/click buttons only — these letters play notes)
    if (keyp('Z') && kb_oct > -1) kb_oct--;
    if (keyp('X') && kb_oct <  2) kb_oct++;
    if (key(KEY_UP))   lever = clamp(lever + 0.025f, 0, 1);   // ride the touche from the desk
    if (key(KEY_DOWN)) lever = clamp(lever - 0.025f, 0, 1);
    for (int s = 0; s < NKBKEY; s++) if (keyp(KBMAP[s])) kb_semi = s;     // last-note priority
    if (kb_semi >= 0 && !key(KBMAP[kb_semi])) {              // active key lifted → highest still-held
        kb_semi = -1;
        for (int s = 0; s < NKBKEY; s++) if (key(KBMAP[s])) kb_semi = s;
    }
    if (kb_semi >= 0) { curMidi = LO_MIDI + kb_oct * 12 + kb_semi; fromRibbon = 0; pitchPtr = 1; }

    // ---- pointer input: touch AND mouse, ONE path ----
    // On desktop the mouse LMB is merged into the touch pool as a synthetic touch
    // (studio.c "mouse-as-touch"), so the touch loop + tapp() already cover the
    // mouse. Handling the mouse a SECOND time here would double-fire every edge —
    // and an even number of toggle_stop() calls = the button looks dead.
    for (int i = 0; i < touch_count(); i++) {
        int x = touch_x(i), y = touch_y(i);
        int r = region(x, y);
        if (r == R_TOUCHE) { lever = lever_from_y(y); touchePtr = 1; }
        else if (r == R_RIBBON || r == R_KEYBED) { set_pitch_from(r, x); pitchPtr = 1; }
    }
    // edge-triggered buttons (a drag must not re-toggle them)
    for (int s = 0; s < NSTOP; s++)
        if (tapp(STX, STY + s * (STH + 4), STW, STH)) toggle_stop(s);
    for (int d = 0; d < 4; d++)
        if (tapp(STX, DFY + d * (STH + 2), STW, STH)) { diffuseur = d; apply_diffuseur(); }

    // ---- intensity: the touche logic, unified for touch & mouse ----
    // riding the lever = live swell · else holding a pitch swells to the ceiling ·
    // else release to silence (the touche springs back).
    float target = touchePtr ? lever : (pitchPtr ? lever : 0.0f);
    float rate   = touchePtr ? 0.5f : (target > intens ? 0.22f : 0.14f);
    intens += (target - intens) * rate;
    if (intens < 0.002f) intens = 0;

    // drive the live voice
    note_pitch (mainV, curMidi);
    note_vol   (mainV, (int)(intens * 7 + 0.5f));
    note_cutoff(mainV, 320 + (int)(intens * intens * 4600));            // louder = brighter
    note_lfo   (mainV, 0, LFO_PITCH, 6.2f, 0.06f + 0.22f * intens);     // vibrato deepens with effort

    note_vol(souffleV, stop_on[ST_SOUFFLE] ? (int)(intens * 4 + 0.5f) : 0);
}

// ---- drawing ------------------------------------------------------------------
void panel(int x, int y, int w, int h, bool lit, int onc, int offc, const char *lbl) {
    rectfill(x, y, w, h, lit ? onc : offc);
    rect(x, y, w, h, CLR_BROWNISH_BLACK);
    print(lbl, x + 4, y + (h - 5) / 2, lit ? CLR_BLACK : CLR_MEDIUM_GREY);
}

const char *NOTE_NM[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    // wooden body
    rectfill(0, 0, SCREEN_W, 16, CLR_DARK_BROWN);
    rectfill(2, 18, 94, SCREEN_H - 20, CLR_BROWN);            // the tiroir (drawer)
    rect(2, 18, 94, SCREEN_H - 20, CLR_BROWNISH_BLACK);
    print("ONDES  MARTENOT", 8, 5, CLR_LIGHT_PEACH);
    print("A..' play   Z/X oct", 150, 5, CLR_MEDIUM_GREY);
    print_right("1928", SCREEN_W - 6, 5, CLR_PEACH);

    // touche d'intensité — the lever
    rectfill(LVX, LVY, LVW, LVH, CLR_DARKER_GREY);
    rect(LVX, LVY, LVW, LVH, CLR_BROWNISH_BLACK);
    int ky = LVY + (int)((1.0f - lever) * LVH);
    int fill = LVY + LVH - ky;
    rectfill(LVX + 1, ky, LVW - 2, fill, intens > 0.01f ? CLR_ORANGE : CLR_DARK_ORANGE);
    rectfill(LVX - 2, ky - 3, LVW + 4, 6, CLR_LIGHT_PEACH);   // the knob
    rect(LVX - 2, ky - 3, LVW + 4, 6, CLR_BROWNISH_BLACK);
    print("touche", LVX - 2, LVY + LVH + 3, CLR_LIGHT_PEACH);

    // timbre stops
    for (int s = 0; s < NSTOP; s++)
        panel(STX, STY + s * (STH + 4), STW, STH, stop_on[s], CLR_PINK, CLR_DARKER_PURPLE, STOP_LBL[s]);

    // diffuseur switches
    print("diffuseur", STX, DFY - 8, CLR_LIGHT_PEACH);
    for (int d = 0; d < 4; d++)
        panel(STX, DFY + d * (STH + 2), STW, STH, diffuseur == d, CLR_LIME_GREEN, CLR_BLUE_GREEN, DIFF_LBL[d]);

    // ribbon lane
    rectfill(KX, RBY, KW, RBH, CLR_DARKER_BLUE);
    rect(KX, RBY, KW, RBH, CLR_DARK_BLUE);
    for (int m = LO_MIDI; m <= LO_MIDI + SPAN; m++) {        // ticks: octaves bright
        int tx = (int)midi_x(m);
        bool oct = (m % 12) == 0;
        line(tx, RBY, tx, RBY + (oct ? RBH : 6), oct ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
    }
    print("ruban - glissando", KX + 4, RBY + 3, CLR_INDIGO);
    int rx = (int)clamp(midi_x(curMidi), KX, KX + KW);       // the sliding ring
    int glow = (int)(intens * 9);
    if (intens > 0.01f) circ(rx, RBY + RBH / 2, 7 + glow, CLR_BLUE);
    circfill(rx, RBY + RBH / 2, 5, intens > 0.01f ? CLR_LIGHT_PEACH : CLR_INDIGO);
    line(rx, RBY, rx, RBY + RBH, CLR_PINK);

    // keyboard — whites then blacks
    float ww = white_w();
    for (int w = 0; w < 14; w++) {
        int oct = w / 7, deg = w % 7, m = LO_MIDI + oct * 12 + WHITE_SEMI[deg];
        int x = KX + (int)(w * ww);
        bool lit = !fromRibbon && intens > 0.01f && (int)(curMidi + 0.5f) == m;
        rectfill(x, KBY, (int)ww - 1, KBH, lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
        rect(x, KBY, (int)ww - 1, KBH, CLR_DARK_BROWN);
    }
    int bh = (int)(KBH * 0.58f);
    for (int w = 0; w < 13; w++) {
        if (!HAS_BLACK[w % 7]) continue;
        int oct = w / 7, deg = w % 7, m = LO_MIDI + oct * 12 + WHITE_SEMI[deg] + 1;
        int bx = KX + (int)((w + 1) * ww - ww * 0.3f), bw = (int)(ww * 0.6f);
        bool lit = !fromRibbon && intens > 0.01f && (int)(curMidi + 0.5f) == m;
        rectfill(bx, KBY, bw, bh, lit ? CLR_DARK_ORANGE : CLR_BROWNISH_BLACK);
        rect(bx, KBY, bw, bh, CLR_BLACK);
    }

    // readout
    int mi = (int)(curMidi + 0.5f);
    print(str("%s%d  %s  oct%+d", NOTE_NM[mi % 12], mi / 12 - 1, fromRibbon ? "ruban" : "clavier", kb_oct),
          KX + 4, KBY - 9, CLR_LIGHT_PEACH);
    print_right(intens > 0.01f ? "sounding" : "touche it...", SCREEN_W - 6, KBY - 9,
                intens > 0.01f ? CLR_LIME_GREEN : CLR_DARK_GREY);
}
