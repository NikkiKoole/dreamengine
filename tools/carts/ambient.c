#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── AMBIENT RADIO ─────────────────────────────────────────────────────────
// The night sibling of bossa radio (tools/carts/bossa.c): an endless AM
// station that composes beatless ambient drift. Same seed contract — every
// song IS a 32-bit number shown on the display — but a completely different
// engine underneath, exercising the half of the audio API bossa can't touch:
//
//   • HELD voices, not plucks — note_on() pads that never retrigger; chord
//     changes MORPH via note_glide() portamento (the fingers slide, the
//     sound never stops).
//   • per-note LFOs — note_lfo() gives each pad its own slow pitch wobble
//     (tape wow) and filter breathing. Set once, runs forever; no per-frame
//     driving needed.
//   • a different chord brain — no cadences, no ii-V pull. A weighted walk
//     over the degrees of ONE mode (lydian/dorian/mixolydian/aeolian);
//     neighbouring in-mode 7th chords share tones, so the harmony drifts
//     instead of resolving.
//
// Both carts are worked examples for docs/guides/game-music.md.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (layers)   UP/DOWN pace (chord-change rate)
//
// The seed pins the COMPOSITION (mode, key, chord walk, durations, title);
// the PERFORMANCE (each pad's detune, wow rate, glide time, the bell arps)
// re-rolls every playthrough — same dream, never the same night.

#define AMBIENT_SEED 0          // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_PAD  5   // filtered saw — the chord, 4 held voices
#define I_SUB  6   // sine sub bass — the root, 1 held voice
#define I_BELL 7   // long sine bell — sparse arps on chord changes
#define I_WIND 8   // band-passed noise — the weather, 1 held voice

// ── modes — the whole song lives inside one of these ─────────────────────
static const int MODESC[4][7] = {
    { 0, 2, 4, 6, 7, 9, 11 },   // lydian      — weightless, hopeful
    { 0, 2, 3, 5, 7, 9, 10 },   // dorian      — dusky, warm
    { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian  — amber, settled
    { 0, 2, 3, 5, 7, 8, 10 },   // aeolian     — cold, far away
};
static const char *MODENAME[4] = { "lydian", "dorian", "mixolyd", "aeolian" };
static const char *PCNAME[12]  = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// degree walk steps, weighted: mostly 4ths/3rds (shared tones), never static
static const int WALK[] = { 3, 3, 3, -3, -3, -3, 2, 2, -2, -2, 1, -1 };

#define NCHORD 16   // chords per song; each lasts 8–16 beats at 60 bpm

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc, mode;
    int  deg[NCHORD];       // scale degree of each chord in the walk
    int  durBeats[NCHORD];  // how long each chord holds
    char title[24];
    int  freq;              // fake AM dial position
    unsigned seed;
} Song;

static Song   sng;
static int    intensity = 2;     // 0 pads+sub · 1 +wind · 2 +bells · 3 brighter, busier
static float  pace      = 1.0f;  // chord-change rate multiplier (UP/DOWN)
static bool   radioOn   = true;
static int    chordIdx  = 0;
static double changeAt  = 0;     // beat position of the next chord change
static int    songCount = 0;
static char   nowChord[2][8];    // current / next chord names
static bool   showHelp  = false; // H or the ? button

// held-voice state
static int   padH[4] = { -1, -1, -1, -1 }, subH = -1, windH = -1;
static int   gv[4];              // pad voicing (midi), voice-led chord to chord
static bool  gvInit = false;
static float det[4];             // per-pad constant detune (performance, not seed)

static int iabs(int v) { return v < 0 ? -v : v; }

// composition PRNG (xorshift32) — same contract as bossa.c: everything that
// defines THE SONG draws from this; performance color keeps engine rnd().
static unsigned rngState = 1;
static unsigned srnd_u(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return rngState;
}
static int srnd(int n) { return (int)(srnd_u() % (unsigned)n); }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Aurora","Drift","Stillness","Vapor","Glacier","Fog",
    "Echo","Halcyon","Lumen","Tundra","Hollow","Moss","Nebula","Sleep","Polar","Ember" };
static const char *TW2[] = { "at Dawn","in Fog","Below","Adrift","at Sea","in Blue",
    "Asleep","Unmoored","in Winter","Above" };

static void new_song(double pos, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    rngState = sng.seed = seed;

    sng.keyPc = srnd(12);
    sng.mode  = srnd(4);
    sng.deg[0] = 0;                                  // open on the tonic chord
    for (int i = 1; i < NCHORD; i++) {
        int d = sng.deg[i - 1];
        int s;
        do { s = WALK[srnd(12)]; } while (((d + s) % 7 + 7) % 7 == d);
        sng.deg[i] = ((d + s) % 7 + 7) % 7;
        sng.durBeats[i - 1] = 8 + srnd(9);           // 8..16 beats each
    }
    sng.durBeats[NCHORD - 1] = 12 + srnd(5);         // linger on the last one

    if (srnd(100) < 35) snprintf(sng.title, sizeof sng.title, "%s", TW1[srnd(16)]);
    else snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(16)], TW2[srnd(10)]);
    sng.freq = 540 + srnd(107) * 10;

    chordIdx  = -1;                                  // apply_chord(0) on next tick
    changeAt  = pos;
    songCount++;
}

// session history — [ and ] walk back through everything the radio played
static unsigned hist[64];
static int histN = 0, histPos = -1;

static void fresh_song(double pos) {
    new_song(pos, 0);
    if (histN == 64) { for (int i = 1; i < 64; i++) hist[i - 1] = hist[i]; histN--; }
    hist[histN++] = sng.seed;
    histPos = histN - 1;
}

// ── harmony ───────────────────────────────────────────────────────────────
// in-mode 7th chord on degree d: pitch classes of degrees d, d+2, d+4, d+6
static void chord_pcs(int d, int out[4]) {
    const int *sc = MODESC[sng.mode];
    for (int k = 0; k < 4; k++)
        out[k] = (sng.keyPc + sc[(d + 2 * k) % 7]) % 12;
}

static void chord_label(char *out, int n, int d) {
    const int *sc = MODESC[sng.mode];
    int root = sc[d % 7];
    int third   = (sc[(d + 2) % 7] - root + 12) % 12;
    int fifth   = (sc[(d + 4) % 7] - root + 12) % 12;
    int seventh = (sc[(d + 6) % 7] - root + 12) % 12;
    const char *q = (third == 4) ? (seventh == 11 ? "maj7" : "7")
                  : (fifth == 6) ? "m7b5"
                  : (seventh == 11 ? "mM7" : "m7");
    snprintf(out, n, "%s%s", PCNAME[(sng.keyPc + root) % 12], q);
}

// same nearest-tone voice leading as bossa.c, four voices — the pads' fingers
// barely move, and note_glide turns each move into a slide
static void lead_voices(int d) {
    int pcs[4];
    chord_pcs(d, pcs);
    if (!gvInit) {
        for (int k = 0; k < 4; k++) {
            int target = 45 + k * 5;
            int dd = ((pcs[k] - target) % 12 + 18) % 12 - 6;
            gv[k] = target + dd;
        }
        gvInit = true;
    } else {
        bool used[4] = { false, false, false, false };
        for (int v = 0; v < 4; v++) {
            int bestJ = -1, bestC = 0, bestD = 99;
            for (int j = 0; j < 4; j++) {
                if (used[j]) continue;
                int dd = ((pcs[j] - gv[v]) % 12 + 18) % 12 - 6;
                if (iabs(dd) < bestD) { bestD = iabs(dd); bestJ = j; bestC = gv[v] + dd; }
            }
            used[bestJ] = true;
            gv[v] = bestC;
        }
    }
    for (int k = 0; k < 4; k++) {
        while (gv[k] < 40) gv[k] += 12;
        while (gv[k] > 67) gv[k] -= 12;
    }
}

// ── held-voice control ────────────────────────────────────────────────────
static void start_voices(void) {
    for (int i = 0; i < 4; i++) {
        padH[i] = note_on(gv[i], I_PAD, intensity >= 3 ? 5 : 4);
        det[i]  = rnd_float_between(-0.06f, 0.06f);            // performance color
        note_glide(padH[i], rnd_between(900, 2200));           // chord changes = slides
        note_lfo(padH[i], 0, LFO_PITCH,  rnd_float_between(0.05f, 0.16f),
                                          rnd_float_between(0.04f, 0.10f));  // tape wow
        note_lfo(padH[i], 1, LFO_CUTOFF, rnd_float_between(0.04f, 0.09f), 350);  // breathing
        note_pitch(padH[i], gv[i] + det[i]);
    }
    subH = note_on(36 + (sng.keyPc + MODESC[sng.mode][sng.deg[0]]) % 12, I_SUB, 5);
    note_glide(subH, 1600);
    if (intensity >= 1) {
        windH = note_on(70, I_WIND, 2);
        note_lfo(windH, 0, LFO_CUTOFF, 0.06f, 450);            // the weather rolls
    }
}

static void stop_voices(void) {
    note_off_all();
    for (int i = 0; i < 4; i++) padH[i] = -1;
    subH = windH = -1;
    gvInit = false;
}

static void apply_chord(int idx) {
    lead_voices(sng.deg[idx]);
    if (padH[0] < 0) start_voices();
    else {
        for (int i = 0; i < 4; i++) note_pitch(padH[i], gv[i] + det[i]);
        note_pitch(subH, 36 + (sng.keyPc + MODESC[sng.mode][sng.deg[idx]]) % 12);
    }
    // sparse bell arp over the new chord — pure performance, never seeded
    if (intensity >= 2 && chance(intensity >= 3 ? 80 : 55)) {
        int pcs[4];
        chord_pcs(sng.deg[idx], pcs);
        int n = 2 + (intensity >= 3 ? rnd(2) : 0);
        for (int k = 0; k < n; k++)
            schedule_hit(400 + k * rnd_between(380, 700), 84 + pcs[rnd(4)], I_BELL, 3, 2200);
    }
    chord_label(nowChord[0], 8, sng.deg[idx]);
    chord_label(nowChord[1], 8, sng.deg[(idx + 1) % NCHORD]);
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_PAD, INSTR_SAW, 1400, 600, 6, 2800);          // slow swell, long fade
    instrument_filter(I_PAD, FILTER_LOW, 620, 1);              // dark; LFO breathes it

    instrument(I_SUB, INSTR_SINE, 900, 400, 6, 2000);          // felt, not heard
    instrument_filter(I_SUB, FILTER_LOW, 300, 1);

    instrument(I_BELL, INSTR_SINE, 4, 1400, 0, 600);           // glassy, dies slowly
    instrument_filter(I_BELL, FILTER_LOW, 3400, 2);

    instrument(I_WIND, INSTR_NOISE, 2000, 800, 5, 2500);       // band-passed weather
    instrument_filter(I_WIND, FILTER_BAND, 700, 6);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() + (double)beat_pos();

    if (!booted) {
        bpm(60);
        setup_instruments();
        if (AMBIENT_SEED) { new_song(pos, AMBIENT_SEED); hist[histN++] = sng.seed; histPos = 0; }
        else fresh_song(pos);
        booted = true;
    }

    if (keyp(KEY_SPACE)) fresh_song(pos);
    if (keyp('R')) new_song(pos, sng.seed);                    // same dream, fresh night
    if (keyp('[') && histPos > 0)         new_song(pos, hist[--histPos]);
    if (keyp(']') && histPos < histN - 1) new_song(pos, hist[++histPos]);
    if (keyp(KEY_RIGHT) && intensity < 3) {
        intensity++;
        if (intensity == 1 && radioOn && windH < 0 && padH[0] >= 0) {
            windH = note_on(70, I_WIND, 2);
            note_lfo(windH, 0, LFO_CUTOFF, 0.06f, 450);
        }
        if (padH[0] >= 0) for (int i = 0; i < 4; i++) note_vol(padH[i], intensity >= 3 ? 5 : 4);
    }
    if (keyp(KEY_LEFT) && intensity > 0) {
        intensity--;
        if (intensity == 0 && windH >= 0) { note_off(windH); windH = -1; }
        if (padH[0] >= 0) for (int i = 0; i < 4; i++) note_vol(padH[i], 4);
    }
    if (keyp(KEY_UP)   && pace < 1.75f) pace += 0.15f;         // faster drift
    if (keyp(KEY_DOWN) && pace > 0.55f) pace -= 0.15f;
    if (keyp('M')) {
        radioOn = !radioOn;
        if (!radioOn) stop_voices();
        else { changeAt = pos; if (chordIdx >= 0) chordIdx--; }   // re-enter on a change
    }
    if (keyp('H')) showHelp = !showHelp;
    if (mouse_pressed(MOUSE_LEFT)) {       // the little ? button on the chassis
        int hx = mouse_x() - 288, hy = mouse_y() - 172;
        if (hx * hx + hy * hy < 81) showHelp = !showHelp;
    }

    if (radioOn && pos >= changeAt) {
        chordIdx++;
        if (chordIdx >= NCHORD) { fresh_song(pos); chordIdx = 0; }
        apply_chord(chordIdx);
        changeAt = pos + sng.durBeats[chordIdx] / pace;
    }

#ifdef DE_TRACE
    watch("song", "%d", songCount);
    watch("mode", "%s", MODENAME[sng.mode]);
    watch("chordIdx", "%d", chordIdx);
    watch("chord", "%s", nowChord[0]);
#endif
}

// ── draw — the radio at night ─────────────────────────────────────────────
static void knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARK_PURPLE);
    circ(x, y, r, CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_BLACK);
    float t = timer();

    // stars behind everything
    for (int k = 0; k < 40; k++) {
        int sx = (k * 53 + 7) % 320, sy = (k * 91 + 13) % 200;
        if ((k + frame() / 40) % 9) pset(sx, sy, (k % 3) ? CLR_DARK_GREY : CLR_LIGHT_GREY);
    }

    // body
    rectfill(20, 16, 280, 168, CLR_DARK_PURPLE);
    rectfill(24, 20, 272, 160, CLR_DARKER_PURPLE);

    // AM dial strip
    rectfill(32, 26, 256, 18, CLR_DARK_BLUE);
    rect(32, 26, 256, 18, CLR_DARK_PURPLE);
    for (int fq = 540; fq <= 1600; fq += 100) {
        int x = 36 + (fq - 540) * 248 / 1060;
        line(x, 38, x, 42, CLR_LIGHT_GREY);
        if ((fq - 540) % 300 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 8, 29, CLR_LIGHT_GREY);
        }
    }
    int nx = 36 + (sng.freq - 540) * 248 / 1060;
    line(nx, 27, nx, 43, CLR_RED);

    // the window — aurora over a night horizon where bossa has its grille
    rectfill(34, 52, 102, 116, CLR_DARKER_BLUE);
    rect(34, 52, 102, 116, CLR_DARK_PURPLE);
    circfill(118, 66, 8, CLR_LIGHT_YELLOW);                    // moon
    circfill(115, 63, 7, CLR_DARKER_BLUE);                     // waning
    static const int RIBBON[4][3] = {                          // colors per mode
        { CLR_MEDIUM_GREEN, CLR_LIME_GREEN, CLR_BLUE },  // lydian
        { CLR_ORANGE, CLR_PINK, CLR_DARK_PURPLE },             // dorian
        { CLR_YELLOW, CLR_ORANGE, CLR_DARK_RED },              // mixolydian
        { CLR_BLUE, CLR_INDIGO, CLR_TRUE_BLUE },              // aeolian
    };
    if (radioOn)
        for (int rb = 0; rb < 3; rb++)
            for (int x = 36; x < 134; x += 2) {
                float ph = rb * 2.1f + chordIdx * 0.7f;
                int cy = 84 + rb * 22 + (int)(sinf(x * 0.045f + t * (0.25f + rb * 0.1f) + ph) * 9.0f);
                int h  = 5 + (int)(sinf(x * 0.09f - t * 0.2f + ph) * 3.0f);
                line(x, cy - h, x, cy, RIBBON[sng.mode][rb]);
            }
    rectfill(34, 152, 102, 16, CLR_DARK_BLUE);                 // the dark sea below

    // display window
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_DARK_GREY);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_BLUE);
        char l2[32];
        snprintf(l2, 32, "AM %d  %s %s", sng.freq, PCNAME[sng.keyPc], MODENAME[sng.mode]);
        print(l2, 154, 70, CLR_BLUE);
        snprintf(l2, 32, "x%.1f  #%08X", pace, sng.seed);
        print(l2, 154, 82, CLR_BLUE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // chord readout: CURRENT drifting toward next
    if (radioOn) {
        int cw = text_width(nowChord[0]);
        rectfill(178 - cw / 2 - 3, 102, cw + 6, 13, CLR_INDIGO);
        print(nowChord[0], 178 - cw / 2, 105, CLR_DARK_PURPLE);
        print("->", 212, 106, CLR_DARK_GREY);
        print(nowChord[1], 232, 106, CLR_LIGHT_GREY);
        for (int i = 0; i < NCHORD; i++)                        // walk progress
            circfill(154 + i * 9, 126, 1, i <= chordIdx ? CLR_BLUE : CLR_DARK_GREY);
    }

    // knobs + slow-breathing power LED (no beat to blink to)
    static const char *FEEL[4] = { "void", "night", "dawn", "aurora" };
    knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_BLUE);
    knob(218, 148, 9, (pace - 0.55f) / 1.2f, "pace", CLR_BLUE);
    float breathe = 0.5f + 0.5f * sinf(t * 0.8f);
    knob(262, 148, 11, radioOn ? breathe : 0, "drift", CLR_RED);
    circfill(282, 28, 2, radioOn && breathe > 0.5f ? CLR_RED : CLR_DARK_RED);

    // help button + bottom hint
    circfill(288, 172, 6, CLR_DARK_PURPLE);
    circ(288, 172, 6, CLR_BLACK);
    print("?", 285, 169, CLR_LIGHT_GREY);
    print("SPACE next song   H help", 8, 190, CLR_DARK_GREY);

    if (showHelp) {
        rectfill(44, 40, 232, 122, CLR_BLACK);
        rect(44, 40, 232, 122, CLR_INDIGO);
        print("AMBIENT RADIO", 52, 46, CLR_BLUE);
        font(FONT_SMALL);
        static const char *HELP[7][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same dream again - a fresh night" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - void/night/dawn/aurora" },
            { "UP/DOWN",    "pace - how fast chords drift" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        for (int i = 0; i < 7; i++) {
            print(HELP[i][0], 52, 60 + i * 9, CLR_BLUE);
            print(HELP[i][1], 106, 60 + i * 9, CLR_LIGHT_GREY);
        }
        print("the #number on the display IS the song.", 52, 128, CLR_INDIGO);
        print("pin it for good: #define AMBIENT_SEED 0x...", 52, 137, CLR_INDIGO);
        print("seeded composition, plays fresh every night", 52, 146, CLR_INDIGO);
        font(FONT_NORMAL);
    }
}
