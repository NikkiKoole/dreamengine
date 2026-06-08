// reed — INSTR_REED showcase: a blown single/double-reed instrument (the eighth modeled
// ENGINE), and the first SELF-OSCILLATING one. A bore delay line + a pressure-driven reed
// valve that sings for as long as you blow — so unlike the struck engines (pluck/mallet/
// membrane) it HOLDS, like ORGAN. One id covers clarinet / soprano-alto-tenor sax / oboe —
// the blown family the wavetables can't reach. Every engine answers the same three 0..1
// macros — the API never grows:
//   harmonics = BORE   (0 = cylindrical clarinet, hollow + odd-harmonics → 1 = conical sax, full + bright)
//   timbre    = EDGE   (0 = soft + dark reed → 1 = stiff + nasal/bright)
//   morph     = BREATH (0 = soft steady tone → 1 = a leaning, growling breath + deeper vibrato)
//
// Because it HOLDS, the macros are live expression: hold a note (press-and-hold a key) and
// sweep BREATH and you hear the player lean in — a swell no struck engine can make. The
// reed self-oscillates only inside a pressure window, so the macros are pinned to that window
// (push past it on a real reed and it chokes — see instrument-engines.md §8.8.7).
//
// The bore on the left morphs cylinder→cone with the harmonics knob; the breath meter rides
// the morph knob; the readout prints the instrument() call to copy into your cart.
//
// controls: A S D F G H J K  blow & HOLD (release to stop)   ·   Z / X octave down / up
//           1..5 presets: clarinet / soprano sax / alto sax / tenor sax / oboe
//           drag a slider (live on held notes), or LEFT/RIGHT pick + UP/DOWN turn it
//           SPACE auto-breath swell on the held notes   ·   M autoplay phrase on/off
//
// MULTITOUCH: every finger blows its own pad — hold a chord with several fingers, or hold a
// slider with one while a note drones. Desktop mouse = one pointer.

#include "studio.h"
#include <math.h>

#define I_REED 5
#define NPAD   8

static const char KEYS[NPAD] = { 'A','S','D','F','G','H','J','K' };

static const char *MNAME[3] = { "harmonics", "timbre", "morph"  };
static const char *MLO[3]   = { "clarinet",  "soft",   "still"  };
static const char *MHI[3]   = { "sax",       "nasal",  "growl"  };

// presets: macro positions for the five reeds the waveguide reaches (harmonica's free reed
// sits below the self-oscillation floor — out of scope, §8.8.7). STARTING GUESSES, tuned by ear.
typedef struct { const char *name; float h, t, m; } Preset;
static const Preset PRESET[5] = {
    { "clarinet",   0.00f, 0.30f, 0.40f },  // cylindrical, soft cane, dark hollow chalumeau
    { "sop sax",    0.92f, 0.70f, 0.55f },  // strongly conical, stiff, bright + cutting
    { "alto sax",   0.78f, 0.45f, 0.55f },  // conical, medium reed, the warm jazz horn
    { "tenor sax",  0.82f, 0.30f, 0.62f },  // conical, soft big reed, breathy + round
    { "oboe",       0.55f, 0.92f, 0.50f },  // narrow conical, very stiff, nasal + penetrating
};

#define NSLIDER 3
static int   midi_of[NPAD];
static int   handle_of[NPAD];      // held-note handle per pad, -1 = silent
static float glow[NPAD];
static float knob[NSLIDER];
static int   sel = 0;
static int   cur_preset = 0;
static int   oct = 0;
static bool  autoplay = true;
static bool  breath = false;       // SPACE: auto-breath swell on held notes
static float breath_lfo = 0.0f;
static int   apos = 0;
static float airflow = 0.0f;       // bore airflow animation phase

// per-finger pointer table — a finger blows a pad or drags a slider
#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_DRAG, PTR_BLOW };
typedef struct { int id, mode, k; } Ptr;
static Ptr ptr[NPTR];

static int km(int b) { return midi_of[b] + oct * 12; }

// layout
#define PAD_W    34
#define PAD_X(b) (10 + (b) * (PAD_W + 4))
#define PAD_Y    90
#define PAD_H    24
#define PRE_Y    122
#define MROW_Y   152
#define MROW_W   72
#define MROW_X(k) (10 + (k) * 102)
// bore viz
#define BORE_X   10
#define BORE_Y   46
#define BORE_W   86

static void apply_patch(void) {
    // attack 1, short decay, full sustain, release 4 — a held wind voice (like ORGAN/PD)
    instrument(I_REED, INSTR_REED, 1, 0, 4, 1200);
    instrument_harmonics(I_REED, knob[0]);
    instrument_timbre(I_REED, knob[1]);
    instrument_morph(I_REED, knob[2]);
}

// push the (possibly auto-breath-modulated) macros to every sounding voice — reed reads all
// three LIVE (it's a continuous voice), so a swell or a bore change reaches a droning note.
static void push_live(void) {
    float m = knob[2];
    if (breath) m = clamp(m * 0.5f + 0.5f * (0.5f + 0.5f * sinf(breath_lfo)), 0.0f, 1.0f);
    for (int b = 0; b < NPAD; b++)
        if (handle_of[b] >= 0) {
            note_harmonics(handle_of[b], knob[0]);
            note_timbre(handle_of[b], knob[1]);
            note_morph(handle_of[b], m);
        }
}

static void blow(int b) {
    if (handle_of[b] >= 0) return;           // already sounding
    handle_of[b] = note_on(km(b), I_REED, 6);
    glow[b] = 1.0f;
}
static void release(int b) {
    if (handle_of[b] < 0) return;
    note_off(handle_of[b]);
    handle_of[b] = -1;
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h; knob[1] = PRESET[p].t; knob[2] = PRESET[p].m;
    cur_preset = p;
    apply_patch();
    push_live();
}

void init(void) {
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    for (int b = 0; b < NPAD; b++) { midi_of[b] = degree(SCALE_MAJOR, 4, b); handle_of[b] = -1; }
    set_preset(2);   // alto sax — the headline sound
    bpm(96);
}

void update(void) {
    for (int b = 0; b < NPAD; b++) {
        if (keyp(KEYS[b])) blow(b);
        if (keyr(KEYS[b])) release(b);
    }
    for (int p = 0; p < 5; p++) if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + NSLIDER - 1) % NSLIDER;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % NSLIDER;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_patch();
    }
    if (keyp(KEY_SPACE)) breath = !breath;
    if (keyp('M')) autoplay = !autoplay;
    if (keyp('Z') && oct > -2) oct--;
    if (keyp('X') && oct <  2) oct++;

    // touch: blow a pad (held until the finger lifts) or drag a slider
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1 };
            if (point_in_box(tx, ty, SCREEN_W - 92, 2, 88, 12)) { autoplay = !autoplay; continue; }
            if (ty >= PRE_Y - 2 && ty < PRE_Y + 12)
                for (int q = 0; q < 5; q++) if (tx >= 12 + q * 60 && tx < 12 + q * 60 + 58) { set_preset(q); break; }
            for (int k = 0; k < NSLIDER; k++)
                if (point_in_box(tx, ty, MROW_X(k) - 2, MROW_Y - 8, MROW_W + 4, 20)) { p->mode = PTR_DRAG; p->k = sel = k; }
            if (p->mode == PTR_IDLE)
                for (int b = 0; b < NPAD; b++)
                    if (point_in_box(tx, ty, PAD_X(b), PAD_Y, PAD_W, PAD_H)) { p->mode = PTR_BLOW; p->k = b; blow(b); break; }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - MROW_X(p->k)) / (float)MROW_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_patch();
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) {
                if (ptr[j].mode == PTR_BLOW && ptr[j].k >= 0) release(ptr[j].k);
                ptr[j].id = NOID;
            }

    // autoplay: a slow legato phrase that lets each note bloom + the breath ride it
    if (autoplay && every(2)) {
        static const int seq[8] = { 0, 2, 4, 2, 5, 4, 2, 0 };
        int b = seq[apos % 8] % NPAD;
        hit(km(b), I_REED, 6, 360);     // a sustained blown note (self-oscillates through the gate)
        glow[b] = 1.0f;
        apos++;
    }

    breath_lfo += 0.10f;
    airflow += 0.04f + knob[2] * 0.05f;
    push_live();

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("breath", "%d", breath ? 1 : 0);
    watch("preset", "%d", cur_preset);
    int held = 0; for (int b = 0; b < NPAD; b++) if (handle_of[b] >= 0) held++;
    watch("held", "%d", held);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("REED", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("blown waveguide engine", 50, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M phrase: on" : "M phrase: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── the bore: cylinder (harmonics 0) → cone (harmonics 1), with a reed at the mouthpiece
    rectfill(BORE_X - 2, BORE_Y - 18, BORE_W + 8, 40, CLR_DARK_BROWN);
    float bore = knob[0];
    int mouthH = 5, bellH = (int)(5 + bore * 16);   // mouthpiece small; bell flares with conicity
    int cx0 = BORE_X + 6, cx1 = BORE_X + BORE_W - 4;
    int top0 = BORE_Y - mouthH, bot0 = BORE_Y + mouthH;
    int top1 = BORE_Y - bellH,  bot1 = BORE_Y + bellH;
    // bore body (two trapezoid edges)
    line(cx0, top0, cx1, top1, CLR_PEACH);
    line(cx0, bot0, cx1, bot1, CLR_PEACH);
    line(cx1, top1, cx1, bot1, CLR_LIGHT_PEACH);    // the bell rim
    // the reed/mouthpiece (timbre = edge: brighter = stiffer-looking)
    circfill(cx0, BORE_Y, 3, knob[1] > 0.6f ? CLR_LIGHT_YELLOW : CLR_DARK_RED);
    // airflow pulses travelling down the bore — speed/density rides breath (morph)
    int held = 0; for (int b = 0; b < NPAD; b++) if (handle_of[b] >= 0) held++;
    bool sounding = held > 0 || (autoplay);
    if (sounding)
        for (int s = 0; s < 5; s++) {
            float ph = airflow + s * 0.6f;
            float u = ph - floorf(ph);                 // 0..1 along the bore
            int x = cx0 + (int)(u * (cx1 - cx0));
            int h = (int)(mouthH + u * (bellH - mouthH));
            int yy = BORE_Y + (int)(sinf(ph * 6.28f) * h * 0.5f);
            pset(x, yy, u < 0.5f ? CLR_TRUE_BLUE : CLR_BLUE_GREEN);
        }
    font(FONT_TINY);
    print(bore < 0.33f ? "cylindrical" : bore < 0.66f ? "conical" : "wide conical", BORE_X, BORE_Y + 14, CLR_LIGHT_PEACH);
    font(FONT_NORMAL);

    // ── breath meter (morph) — the held-voice expression axis
    {
        int bx = BORE_X, by = BORE_Y + 26;
        float m = knob[2];
        if (breath) m = m * 0.5f + 0.5f * (0.5f + 0.5f * sinf(breath_lfo));
        font(FONT_TINY); print("breath", bx, by - 7, breath ? CLR_LIME_GREEN : CLR_DARK_GREY); font(FONT_NORMAL);
        bar(bx, by, BORE_W, 6, m, breath ? CLR_LIME_GREEN : CLR_ORANGE, CLR_DARK_BROWN);
    }

    // ── readout
    font(FONT_SMALL);
    const char *pname = (cur_preset >= 0) ? PRESET[cur_preset].name : "(custom)";
    print(str("voice: %s", pname), 108, 24, CLR_LIGHT_YELLOW);
    print(knob[0] < 0.5f ? "cylindrical bore - odd harmonics" : "conical bore - all harmonics", 108, 36, CLR_MEDIUM_GREY);
    print(held > 0 ? str("blowing %d note%s - holds while held", held, held == 1 ? "" : "s")
                   : "press & HOLD a key to blow", 108, 48, held > 0 ? CLR_PEACH : CLR_DARK_GREY);
    print(breath ? "SPACE auto-breath: swelling" : "SPACE auto-breath: off", 108, 60, breath ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_TINY);
    print(str("instrument(I, INSTR_REED, 1,0,4,1200)  h %.2f t %.2f m %.2f", knob[0], knob[1], knob[2]),
          108, 74, CLR_BLUE_GREEN);
    font(FONT_NORMAL);

    // ── pads (press & hold)
    for (int b = 0; b < NPAD; b++) {
        int x = PAD_X(b);
        bool on = handle_of[b] >= 0;
        if (!on) glow[b] *= 0.88f;
        int col = on ? CLR_LIGHT_YELLOW : glow[b] > 0.2f ? CLR_DARK_PEACH : CLR_DARK_BROWN;
        rectfill(x, PAD_Y, PAD_W, PAD_H, col);
        rect(x, PAD_Y, PAD_W, PAD_H, on ? CLR_WHITE : CLR_DARK_RED);
        print(str("%c", KEYS[b]), x + PAD_W / 2 - 3, PAD_Y + PAD_H - 10, on ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }

    // ── preset row
    font(FONT_SMALL);
    for (int p = 0; p < 5; p++)
        print(str("%d %s", p + 1, PRESET[p].name), 14 + p * 60, PRE_Y, p == cur_preset ? CLR_YELLOW : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── macro row (all three live on held notes)
    for (int k = 0; k < NSLIDER; k++) {
        int x = MROW_X(k), y = MROW_Y;
        bool on = (k == sel);
        font(FONT_SMALL); print(MNAME[k], x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY); font(FONT_NORMAL);
        bar(x, y, MROW_W, 7, knob[k], on ? CLR_ORANGE : CLR_DARK_PEACH, CLR_DARK_BROWN);
        font(FONT_TINY);
        print(MLO[k], x, y + 9, CLR_DARK_GREY);
        print_right(MHI[k], x + MROW_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    int rx = print("A..K hold to blow   Z/X octave   1..5 voices   ", 10, SCREEN_H - 8, CLR_DARK_GREY);
    int sx = print("SPACE breath swell", rx, SCREEN_H - 8, CLR_MEDIUM_GREY);
    print("   sliders: drag or arrows", sx, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
