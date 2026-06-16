#include "studio.h"
#include "pointer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── FINGER DRUMS ───────────────────────────────────────────────────────────
// A play-it-by-hand drum cart — the missing PERFORMANCE modality (every other
// drum cart in the cabinet is a step-sequencer; here you actually hit the kit).
// Two views, each matching its SOUND:
//
//   ACOUSTIC KIT  — a real drumset drawn in perspective (kick / snare / rack +
//                   floor toms / hats / crash + ride). Toms are the modeled
//                   INSTR_MEMBRANE (a tom IS a tuned drumhead); kick is a
//                   pitch-swept sine, snare + cymbals are filtered noise.
//   MPC PADS      — a 4×4 grid of an 808-style electronic kit (the tr808
//                   recipes: bridged-sine boom kick, noise snare, square
//                   cowbell, retriggered clap). The digital lineage.
//
//   TAB           switch view (= switch kit).
//
// VELOCITY without pressure: touch hardware gives us position, never force, so
// we read DYNAMICS from WHERE you hit — high on a drum/pad = soft, low = hard
// (8 levels, 1..7). A USB-MIDI pad controller plays it with REAL velocity for
// free (midi_get). The keyboard fallback hits mid-velocity; hold SHIFT to
// accent. Multitouch: every finger strikes independently (pointer.h pool).

// ── voice slots (5..31; 0..4 are the built-in waves) ───────────────────────
enum {
    // acoustic kit
    AK_KICK = 5, AK_SNR_N, AK_SNR_B, AK_TOM, AK_HHC, AK_HHO, AK_CRASH, AK_RIDE,
    // digital 808 kit
    DK_BD, DK_SD_B, DK_SD_N, DK_HHC, DK_HHO, DK_CLAP, DK_TOM, DK_COW, DK_CYM, DK_RIM,
};

// ── the distinct sounds each view fires (a view = a kit) ────────────────────
enum {
    SND_AK_KICK, SND_AK_SNARE, SND_AK_TOM, SND_AK_HHC, SND_AK_HHO, SND_AK_CRASH, SND_AK_RIDE,
    SND_DK_BD, SND_DK_SD, SND_DK_CLAP, SND_DK_HHC, SND_DK_HHO, SND_DK_COW, SND_DK_TOM, SND_DK_CYM, SND_DK_RIM,
};

// fire a sound at a given velocity (1..7). schedule_hit lets us pick the gate
// length per voice, so long tails (kick, crash) aren't clipped by note()'s 250ms.
static void fire(int snd, int midi, int vol) {
    if (vol < 1) vol = 1; if (vol > 7) vol = 7;
    switch (snd) {
    // — acoustic —
    case SND_AK_KICK:  schedule_hit(0, 33, AK_KICK, vol, 320); break;
    case SND_AK_SNARE: schedule_hit(0, 57, AK_SNR_B, vol, 90);          // drum tone
                       schedule_hit(0, 60, AK_SNR_N, vol, 120); break;  // wires/crack
    case SND_AK_TOM:   schedule_hit(0, midi, AK_TOM, vol, 300); break;  // tuned membrane
    case SND_AK_HHC:   schedule_hit(0, 70, AK_HHC, vol, 45); break;
    case SND_AK_HHO:   schedule_hit(0, 70, AK_HHO, vol, 240); break;
    case SND_AK_CRASH: schedule_hit(0, 70, AK_CRASH, vol, 800); break;
    case SND_AK_RIDE:  schedule_hit(0, 78, AK_RIDE, vol, 320); break;
    // — digital 808 —
    case SND_DK_BD:    schedule_hit(0, 31, DK_BD, vol, 460); break;
    case SND_DK_SD:    schedule_hit(0, 54, DK_SD_B, vol, 100);
                       schedule_hit(0, 60, DK_SD_N, vol, 140); break;
    case SND_DK_CLAP:  schedule_hit(0,  60, DK_CLAP, vol, 12);          // 3 fast retriggers
                       schedule_hit(10, 60, DK_CLAP, vol, 12);          // + a body = the
                       schedule_hit(20, 60, DK_CLAP, vol, 12);          // 808 handclap
                       schedule_hit(28, 60, DK_CLAP, vol > 1 ? vol - 1 : 1, 140); break;
    case SND_DK_HHC:   schedule_hit(0, 70, DK_HHC, vol, 45); break;
    case SND_DK_HHO:   schedule_hit(0, 70, DK_HHO, vol, 300); break;
    case SND_DK_COW:   schedule_hit(0, 73, DK_COW, vol, 200);           // square pair
                       schedule_hit(0, 79, DK_COW, vol, 200); break;
    case SND_DK_TOM:   schedule_hit(0, midi, DK_TOM, vol, 260); break;
    case SND_DK_CYM:   schedule_hit(0, midi ? midi : 79, DK_CYM, vol, 700); break;
    case SND_DK_RIM:   schedule_hit(0, midi ? midi : 92, DK_RIM, vol, 50); break;
    }
}

// ── playable zones ──────────────────────────────────────────────────────────
enum { Z_DRUM, Z_CYM, Z_PAD };
typedef struct {
    int cx, cy, rx, ry;     // center + half-extents (rx/ry; ellipse for drums/cymbals, rect for pads)
    int shape;              // Z_DRUM / Z_CYM / Z_PAD
    int snd;                // SND_*
    int midi;               // pitch for toms / cymbals (0 = use the fire() default)
    const char *label;
    int col;                // accent color
    int key;                // keyboard key (0 = none)
    // runtime
    float flash;            // 0..1, decays — strike brightness
    float vlast;            // last velocity 0..1 — glow size
    float squash;           // 0..1 squash-stretch amount, decays
} Zone;

// the acoustic kit, in DRAW order (back → front). Hit-test walks it in reverse
// so the front (kick) wins an overlap.
static Zone zk[] = {
    {  92,  60, 34, 11, Z_CYM,  SND_AK_CRASH, 0,  "CRASH", CLR_YELLOW, 'T' },
    { 242,  70, 40, 13, Z_CYM,  SND_AK_RIDE, 78, "RIDE",  CLR_ORANGE, 'Y' },
    {  40, 104, 22,  6, Z_CYM,  SND_AK_HHO,   0, "OPEN",  CLR_YELLOW, 'G' },
    {  40, 120, 22,  7, Z_CYM,  SND_AK_HHC,   0, "HAT",   CLR_YELLOW, 'F' },
    { 132, 116, 21, 17, Z_DRUM, SND_AK_TOM,  62, "TOM",   CLR_RED,    'I' },
    { 186, 112, 23, 19, Z_DRUM, SND_AK_TOM,  56, "TOM",   CLR_RED,    'K' },
    { 256, 166, 29, 26, Z_DRUM, SND_AK_TOM,  45, "FLOOR", CLR_RED,    'M' },
    {  96, 164, 25, 20, Z_DRUM, SND_AK_SNARE, 0, "SNARE", CLR_LIGHT_GREY, 'J' },
    { 162, 190, 50, 44, Z_DRUM, SND_AK_KICK,  0, "KICK",  CLR_RED, KEY_SPACE },
};
#define NZK ((int)(sizeof(zk) / sizeof(zk[0])))

// the digital 808 kit — a 4×4 pad grid (computed centers; row0 top → row3 bottom)
static Zone zp[] = {
    {  61,  55, 29, 21, Z_PAD, SND_DK_CYM,  79, "CRASH",  CLR_INDIGO,     '1' },
    { 127,  55, 29, 21, Z_PAD, SND_DK_CYM,  86, "RIDE",   CLR_INDIGO,     '2' },
    { 193,  55, 29, 21, Z_PAD, SND_DK_COW,   0, "COW",    CLR_LIME_GREEN, '3' },
    { 259,  55, 29, 21, Z_PAD, SND_DK_RIM,  92, "RIM",    CLR_PINK,       '4' },
    {  61, 105, 29, 21, Z_PAD, SND_DK_HHO,   0, "O-HAT",  CLR_YELLOW,     'Q' },
    { 127, 105, 29, 21, Z_PAD, SND_DK_TOM,  60, "TOM HI", CLR_BLUE,       'W' },
    { 193, 105, 29, 21, Z_PAD, SND_DK_TOM,  53, "TOM MID",CLR_BLUE,       'E' },
    { 259, 105, 29, 21, Z_PAD, SND_DK_TOM,  45, "TOM LO", CLR_TRUE_BLUE,  'R' },
    {  61, 155, 29, 21, Z_PAD, SND_DK_HHC,   0, "HAT",    CLR_YELLOW,     'A' },
    { 127, 155, 29, 21, Z_PAD, SND_DK_SD,    0, "SNARE",  CLR_ORANGE,     'S' },
    { 193, 155, 29, 21, Z_PAD, SND_DK_CLAP,  0, "CLAP",   CLR_DARK_ORANGE,'D' },
    { 259, 155, 29, 21, Z_PAD, SND_DK_RIM,  84, "PERC",   CLR_PINK,       'F' },
    {  61, 205, 29, 21, Z_PAD, SND_DK_BD,    0, "KICK",   CLR_RED,        'Z' },
    { 127, 205, 29, 21, Z_PAD, SND_DK_BD,    0, "KICK",   CLR_RED,        'X' },
    { 193, 205, 29, 21, Z_PAD, SND_DK_SD,    0, "SNARE",  CLR_ORANGE,     'C' },
    { 259, 205, 29, 21, Z_PAD, SND_DK_CLAP,  0, "CLAP",   CLR_DARK_ORANGE,'V' },
};
#define NZP ((int)(sizeof(zp) / sizeof(zp[0])))

static bool pads_view = false;   // false = acoustic kit, true = MPC pads
static Zone *zones(void)  { return pads_view ? zp : zk; }
static int   nzones(void) { return pads_view ? NZP : NZK; }

// ── strike ripples ──────────────────────────────────────────────────────────
#define NRIP 28
typedef struct { float x, y, age, life, r0, grow; int col; bool on; } Rip;
static Rip rip[NRIP];
static void spawn_rip(int x, int y, float r0, float grow, float life, int col) {
    for (int i = 0; i < NRIP; i++) if (!rip[i].on) {
        rip[i] = (Rip){ (float)x, (float)y, 0, life, r0, grow, col, true };
        return;
    }
}

// ── input plumbing ───────────────────────────────────────────────────────────
typedef struct { int id; } Ptr;     // first field MUST be id (pointer.h contract)
static Ptr ptrs[PTR_MAX];

#define FRAME (1.0f / 60.0f)
#define KEY_LSHIFT 340              // raylib KEY_LEFT_SHIFT
static bool show_help = false;
static int  last_vol = 0;          // for the readout

// velocity from vertical hit position inside a zone: top = soft, bottom = hard.
static int vol_from_y(const Zone *z, int hy) {
    float t = (float)(hy - (z->cy - z->ry)) / (float)(2 * z->ry);
    if (t < 0) t = 0; if (t > 1) t = 1;
    int v = 1 + (int)(t * 6.0f + 0.5f);
    return v < 1 ? 1 : v > 7 ? 7 : v;
}

static bool zone_contains(const Zone *z, int x, int y) {
    if (z->shape == Z_PAD) {
        return x >= z->cx - z->rx && x <= z->cx + z->rx &&
               y >= z->cy - z->ry && y <= z->cy + z->ry;
    }
    float dx = (float)(x - z->cx) / (float)z->rx;
    float dy = (float)(y - z->cy) / (float)(z->ry + (z->shape == Z_CYM ? 6 : 0));  // cymbals: forgiving vertical pad
    return dx * dx + dy * dy <= 1.0f;
}

// front-most zone under (x,y), or -1
static int zone_at(int x, int y) {
    Zone *z = zones();
    for (int i = nzones() - 1; i >= 0; i--)
        if (zone_contains(&z[i], x, y)) return i;
    return -1;
}

static void strike(int zi, int vol) {
    Zone *z = &zones()[zi];
    z->flash = 1.0f;
    z->vlast = vol / 7.0f;
    z->squash = (z->shape == Z_CYM) ? 0.0f : 0.5f + 0.5f * z->vlast;
    float r0 = (z->shape == Z_PAD) ? 4 : 6;
    spawn_rip(z->cx, z->cy, r0, (z->rx + z->ry) * (0.4f + 0.5f * z->vlast), 0.45f, z->col);
    if (z->snd == SND_AK_KICK || z->snd == SND_DK_BD) shake(1.0f + 2.0f * z->vlast);
    last_vol = vol;
    fire(z->snd, z->midi, vol);
}

// MIDI: a GM drum note → a sound in the CURRENT kit (so a pad controller plays it)
static void midi_play(int note, int vol) {
    int snd = -1, midi = 0;
    switch (note) {
    case 35: case 36: snd = pads_view ? SND_DK_BD : SND_AK_KICK; break;
    case 37: snd = pads_view ? SND_DK_RIM : SND_AK_SNARE; break;
    case 38: case 40: snd = pads_view ? SND_DK_SD : SND_AK_SNARE; break;
    case 39: snd = pads_view ? SND_DK_CLAP : SND_AK_SNARE; break;
    case 42: case 44: snd = pads_view ? SND_DK_HHC : SND_AK_HHC; break;
    case 46: snd = pads_view ? SND_DK_HHO : SND_AK_HHO; break;
    case 41: case 43: snd = pads_view ? SND_DK_TOM : SND_AK_TOM; midi = pads_view ? 45 : 45; break;
    case 45: case 47: snd = pads_view ? SND_DK_TOM : SND_AK_TOM; midi = pads_view ? 53 : 56; break;
    case 48: case 50: snd = pads_view ? SND_DK_TOM : SND_AK_TOM; midi = pads_view ? 60 : 62; break;
    case 49: case 57: snd = pads_view ? SND_DK_CYM : SND_AK_CRASH; midi = 79; break;
    case 51: case 59: snd = pads_view ? SND_DK_CYM : SND_AK_RIDE; midi = pads_view ? 86 : 78; break;
    case 56: snd = pads_view ? SND_DK_COW : SND_AK_HHC; break;
    default: return;
    }
    last_vol = vol;
    // flash a matching zone if this kit has one
    Zone *z = zones();
    for (int i = 0; i < nzones(); i++) if (z[i].snd == snd) {
        z[i].flash = 1.0f; z[i].vlast = vol / 7.0f;
        z[i].squash = (z[i].shape == Z_CYM) ? 0.0f : 0.4f + 0.5f * z[i].vlast;
        spawn_rip(z[i].cx, z[i].cy, 5, (z[i].rx + z[i].ry) * 0.5f, 0.45f, z[i].col);
        break;
    }
    fire(snd, midi, vol);
}

void init(void) {
    PTR_CLEAR(ptrs);
    // — acoustic voices —
    instrument(AK_KICK, INSTR_SINE, 0, 300, 0, 80);          // punchy kick
    instrument_filter(AK_KICK, FILTER_LOW, 220, 2);
    instrument_env(AK_KICK, 0, ENV_PITCH, 0, 55, 28.0f);     // the thump's pitch drop
    instrument_drive(AK_KICK, 0.15f);
    instrument(AK_SNR_B, INSTR_SINE, 0, 90, 0, 40);          // snare body
    instrument_filter(AK_SNR_B, FILTER_LOW, 1500, 1);
    instrument_env(AK_SNR_B, 0, ENV_PITCH, 0, 18, 4.0f);
    instrument(AK_SNR_N, INSTR_NOISE, 0, 120, 0, 60);        // snare wires/crack
    instrument_filter(AK_SNR_N, FILTER_BAND, 1900, 3);
    instrument(AK_TOM, INSTR_MEMBRANE, 1, 0, 7, 300);        // toms = tuned drumheads
    instrument_harmonics(AK_TOM, 0.28f);                     // tuned-ish head
    instrument_timbre(AK_TOM, 0.18f);                        // struck near center
    instrument_morph(AK_TOM, 0.12f);                         // a touch of bend
    instrument(AK_HHC, INSTR_NOISE, 0, 45, 0, 25);           // closed hat
    instrument_filter(AK_HHC, FILTER_HIGH, 8500, 2);
    instrument(AK_HHO, INSTR_NOISE, 0, 60, 2, 220);          // open hat — sustain>0 washes
    instrument_filter(AK_HHO, FILTER_HIGH, 8200, 2);
    instrument_choke(AK_HHC, AK_HHO);                        // closed kills open
    instrument(AK_CRASH, INSTR_NOISE, 0, 220, 0, 600);       // crash wash
    instrument_filter(AK_CRASH, FILTER_HIGH, 5000, 1);
    instrument(AK_RIDE, INSTR_NOISE, 0, 90, 0, 300);         // ride ping
    instrument_filter(AK_RIDE, FILTER_BAND, 7000, 3);
    // — digital 808 voices (tr808 recipes) —
    instrument(DK_BD, INSTR_SINE, 0, 480, 0, 60);
    instrument_filter(DK_BD, FILTER_LOW, 250, 3);
    instrument_env(DK_BD, 0, ENV_PITCH, 0, 50, 26.0f);
    instrument_drive(DK_BD, 0.28f);
    instrument(DK_SD_B, INSTR_SINE, 0, 100, 0, 30);
    instrument_filter(DK_SD_B, FILTER_LOW, 1200, 1);
    instrument_env(DK_SD_B, 0, ENV_PITCH, 0, 20, 3.0f);
    instrument(DK_SD_N, INSTR_NOISE, 0, 130, 0, 40);
    instrument_filter(DK_SD_N, FILTER_HIGH, 1800, 2);
    instrument(DK_HHC, INSTR_NOISE, 0, 45, 0, 25);
    instrument_filter(DK_HHC, FILTER_HIGH, 9000, 2);
    instrument(DK_HHO, INSTR_NOISE, 0, 60, 2, 300);
    instrument_filter(DK_HHO, FILTER_HIGH, 9000, 2);
    instrument_choke(DK_HHC, DK_HHO);
    instrument(DK_CLAP, INSTR_NOISE, 0, 110, 0, 50);
    instrument_filter(DK_CLAP, FILTER_BAND, 1100, 5);
    instrument(DK_TOM, INSTR_SINE, 0, 260, 0, 50);
    instrument_env(DK_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
    instrument(DK_COW, INSTR_SQUARE, 0, 200, 0, 30);
    instrument_filter(DK_COW, FILTER_BAND, 2640, 5);
    instrument(DK_CYM, INSTR_NOISE, 0, 300, 0, 700);
    instrument_filter(DK_CYM, FILTER_HIGH, 8000, 1);
    instrument(DK_RIM, INSTR_NOISE, 0, 40, 0, 30);
    instrument_filter(DK_RIM, FILTER_HIGH, 500, 3);
}

void update(void) {
    if (keyp(KEY_TAB)) { pads_view = !pads_view; }
    if (keyp('H')) show_help = !show_help;

    // — touch + mouse (mouse folds in as a synthetic touch) : fire once per finger-down —
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh = false;
        Ptr *p = PTR_ACQUIRE(ptrs, id, &fresh);
        if (!p) continue;
        if (fresh) {
            int zi = zone_at(tx, ty);
            if (zi >= 0) strike(zi, vol_from_y(&zones()[zi], ty));
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptrs, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    // — keyboard fallback: no position → mid velocity, SHIFT accents —
    int kvol = key(KEY_LSHIFT) ? 7 : 5;
    Zone *z = zones();
    for (int i = 0; i < nzones(); i++)
        if (z[i].key && keyp(z[i].key)) strike(i, kvol);

    // — MIDI: real hardware velocity for free —
    int note, vel;
    int r;
    while ((r = midi_get(&note, &vel)) != 0)
        if (r > 0) midi_play(note, 1 + (vel * 6) / 127);

    // — decay visuals —
    for (int i = 0; i < NZK; i++) { zk[i].flash -= FRAME * 6; if (zk[i].flash < 0) zk[i].flash = 0;
                                    zk[i].squash -= FRAME * 5; if (zk[i].squash < 0) zk[i].squash = 0; }
    for (int i = 0; i < NZP; i++) { zp[i].flash -= FRAME * 6; if (zp[i].flash < 0) zp[i].flash = 0;
                                    zp[i].squash -= FRAME * 5; if (zp[i].squash < 0) zp[i].squash = 0; }
    for (int i = 0; i < NRIP; i++) if (rip[i].on) { rip[i].age += FRAME; if (rip[i].age >= rip[i].life) rip[i].on = false; }
}

static void center(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// a struck ripple ring
static void draw_ripples(void) {
    for (int i = 0; i < NRIP; i++) if (rip[i].on) {
        float k = rip[i].age / rip[i].life;
        float R = rip[i].r0 + k * rip[i].grow;
        int col = k < 0.5f ? CLR_WHITE : rip[i].col;
        oval((int)rip[i].x, (int)rip[i].y, (int)R, (int)(R * 0.45f), col);
    }
}

// a cymbal: a thin gold ellipse on a stand, shimmering when struck
static void draw_cymbal(const Zone *z) {
    thickline(z->cx, z->cy, z->cx, 224, 2, CLR_DARK_GREY);     // stand
    int rx = z->rx, ry = z->ry;
    ovalfill(z->cx, z->cy, rx, ry, CLR_ORANGE);
    ovalfill(z->cx, z->cy, rx, ry > 2 ? ry - 2 : 1, z->col);
    oval(z->cx, z->cy, rx, ry, CLR_BROWN);
    pset(z->cx, z->cy, CLR_DARK_GREY);                          // bell
    if (z->flash > 0.05f) {
        int g = (int)(rx * (0.4f + 0.7f * z->flash));
        oval(z->cx, z->cy, g, (int)(g * 0.45f), CLR_WHITE);
        ovalfill(z->cx, z->cy, rx, ry, CLR_WHITE);
    }
}

// a drum: a cylinder (side shell + skin) seen slightly from above, squashing on a hit
static void draw_drum(const Zone *z) {
    int rx = z->rx;
    int ry = z->ry - (int)(z->ry * 0.18f * z->squash);         // squash on strike
    int side = z->ry;                                          // shell depth
    int shellD = CLR_BROWNISH_BLACK, shell = (z->snd == SND_AK_SNARE) ? CLR_MEDIUM_GREY : CLR_DARK_RED;
    ovalfill(z->cx, z->cy + side, rx, ry, shellD);             // bottom rim (shadow)
    rectfill(z->cx - rx, z->cy, 2 * rx, side, shell);          // side shell
    // chrome lugs down the shell
    for (int a = -1; a <= 1; a++) pset(z->cx + a * (rx - 3), z->cy + side / 2, CLR_LIGHT_GREY);
    int skin = z->flash > 0.05f ? CLR_WHITE : (z->snd == SND_AK_KICK ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY);
    ovalfill(z->cx, z->cy, rx, ry, skin);                      // top skin
    oval(z->cx, z->cy, rx, ry, CLR_WHITE);                     // hoop
    oval(z->cx, z->cy, rx - 2, ry - 2, CLR_MEDIUM_GREY);
    if (z->flash > 0.05f) {                                    // strike bloom
        int g = (int)(rx * z->flash);
        ovalfill(z->cx, z->cy, g, (int)(g * (float)ry / rx), CLR_WHITE);
    }
}

static void draw_kit(void) {
    vgradient(0, 16, SCREEN_W, SCREEN_H - 16, CLR_DARKER_BLUE, CLR_BROWNISH_BLACK);
    ovalfill(SCREEN_W / 2, 248, 200, 34, CLR_DARKER_GREY);     // floor riser
    Zone *z = zk;
    for (int i = 0; i < NZK; i++) {
        if (z[i].shape == Z_CYM) draw_cymbal(&z[i]); else draw_drum(&z[i]);
    }
    draw_ripples();
    font(FONT_SMALL);
    for (int i = 0; i < NZK; i++)
        center(z[i].label, z[i].cx, z[i].cy - (z[i].shape == Z_CYM ? 3 : 4), CLR_DARK_BLUE);
    font(FONT_NORMAL);
}

static void draw_pads(void) {
    cls(CLR_BROWNISH_BLACK);
    Zone *z = zp;
    for (int i = 0; i < NZP; i++) {
        int x = z[i].cx - z[i].rx, y = z[i].cy - z[i].ry, w = 2 * z[i].rx, h = 2 * z[i].ry;
        rrectfill(x, y, w, h, 5, CLR_DARKER_GREY);                 // pad base
        if (z[i].flash > 0.05f) {                                  // lit = the pad color, bright by velocity
            int c = z[i].flash > 0.6f ? CLR_WHITE : z[i].col;
            rrectfill(x + 1, y + 1, w - 2, h - 2, 4, c);
        } else {
            rrect(x, y, w, h, 5, z[i].col);                        // idle outline in its color
        }
        // velocity tick column on the right edge (where you hit = how hard)
        int bars = (int)(z[i].vlast * 6 + 0.5f);
        for (int b = 0; b < bars; b++)
            rectfill(x + w - 4, y + h - 4 - b * 3, 2, 2, CLR_LIME_GREEN);
    }
    draw_ripples();
    font(FONT_SMALL);
    for (int i = 0; i < NZP; i++) {
        int lit = z[i].flash > 0.6f;
        center(z[i].label, z[i].cx, z[i].cy - 3, lit ? CLR_BLACK : CLR_LIGHT_GREY);
        char k[2] = { (char)(z[i].key), 0 };
        if (z[i].key >= 32 && z[i].key < 127) center(k, z[i].cx, z[i].cy + 5, lit ? CLR_BLACK : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
}

static void draw_help(void) {
    rrectfill(28, 40, SCREEN_W - 56, SCREEN_H - 80, 6, CLR_DARKER_BLUE);
    rrect(28, 40, SCREEN_W - 56, SCREEN_H - 80, 6, CLR_BLUE);
    font(FONT_SMALL);
    int y = 50;
    center("FINGER DRUMS", SCREEN_W / 2, y, CLR_YELLOW); y += 12;
    print("TAP a drum/pad to play it. WHERE you hit", 38, y, CLR_LIGHT_GREY); y += 8;
    print("sets the VELOCITY: top = soft, low = hard.", 38, y, CLR_LIGHT_GREY); y += 8;
    print("Every finger plays at once (multitouch).", 38, y, CLR_LIGHT_GREY); y += 11;
    print("TAB   switch ACOUSTIC KIT <-> MPC PADS", 38, y, CLR_PEACH); y += 8;
    print("KEYS  kit: F G hat  T Y cym  J snare", 38, y, CLR_PEACH); y += 8;
    print("           I K M toms  SPACE kick", 38, y, CLR_PEACH); y += 8;
    print("      pads: 1-4 / QWER / ASDF / ZXCV", 38, y, CLR_PEACH); y += 8;
    print("SHIFT accents a keyboard hit (harder).", 38, y, CLR_LIGHT_GREY); y += 8;
    print("MIDI  a USB pad controller plays it with", 38, y, CLR_LIGHT_GREY); y += 8;
    print("      real velocity.", 38, y, CLR_LIGHT_GREY); y += 11;
    center("H  close", SCREEN_W / 2, y, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void) {
    if (pads_view) draw_pads(); else draw_kit();
    // top bar
    rectfill(0, 0, SCREEN_W, 14, CLR_BLACK);
    print(pads_view ? "MPC PADS" : "ACOUSTIC KIT", 4, 4, CLR_WHITE);
    print(str("VEL %d", last_vol), 150, 4, CLR_LIME_GREEN);
    print("TAB kit  H help", SCREEN_W - 110, 4, CLR_DARK_GREY);
    if (show_help) draw_help();
}
