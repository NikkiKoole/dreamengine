// eq — a 2-band shelving EQ that can BOOST as well as cut (the library's only boost; the
// filters only cut). A SAW bass + a bright square lead loop play; drag the LOW and HIGH faders
// (±12 dB each) and watch the response curve bend — up = boost, down = cut. Route the EQ over the
// WHOLE mix or just the BASS, or hit AMP to stack DRIVE_ASYM + a mid-forward EQ = a guitar-amp tone.
//   drag faders : low / high gain    A : route    B / tap AMP : guitar-amp preset    arrows : nudge
#include "studio.h"
#include "ui.h"
#include <stdio.h>
#include <math.h>

#define I_BASS 0
#define I_LEAD 1

// fader values are normalized 0..1; map to ±12 dB (0.5 = flat)
static float low_n  = 0.80f;   // opens with a visible boost (≈ +7 dB lows) so the curve shows the headline
static float high_n = 0.70f;   // feature — that EQ can BOOST, not just cut (≈ +5 dB highs)
static int   route  = 0;     // 0 = master (whole mix), 1 = bass only, 2 = off
static int   amp    = 0;     // guitar-amp preset engaged (DRIVE_ASYM + EQ)
static int   dirty  = 1;

static const char *ROUTE_NAME[3] = { "MASTER (whole mix)", "BASS ONLY (per-instr)", "OFF (flat)" };

static float db_of(float n) { return (n - 0.5f) * 24.0f; }   // 0..1 → −12..+12 dB

static void apply(void) {
    float lo = db_of(low_n), hi = db_of(high_n);
    // guitar-amp tone: asymmetric clipper on the lead + a mid-forward EQ around it (cut highs a touch)
    instrument_drive_mode(I_LEAD, DRIVE_ASYM);
    instrument_drive(I_LEAD, amp ? 0.55f : 0.0f);
    if (route == 0) { eq(lo, hi);                 instrument_eq(I_BASS, 0, 0); }   // whole mix
    else if (route == 1) { eq(0, 0);              instrument_eq(I_BASS, lo, hi); } // just the bass
    else { eq(0, 0);                              instrument_eq(I_BASS, 0, 0); }   // off (flat)
}

void init(void) {
    instrument(I_BASS, INSTR_SAW,    4, 200, 6, 240);
    instrument_filter(I_BASS, FILTER_LOW, 1100, 3);
    instrument(I_LEAD, INSTR_SQUARE, 2, 160, 4, 200);
    bpm(120);
    apply();
}

void update(void) {
    if (btnp(0, BTN_A)) { route = (route + 1) % 3; dirty = 1; }
    if (btnp(0, BTN_B)) { amp = !amp; dirty = 1; }
    // arrows nudge whichever band (up/down = low, left/right = high) for fine control
    if (btn(0, BTN_UP))    { low_n  += 0.01f; dirty = 1; }
    if (btn(0, BTN_DOWN))  { low_n  -= 0.01f; dirty = 1; }
    if (btn(0, BTN_RIGHT)) { high_n += 0.01f; dirty = 1; }
    if (btn(0, BTN_LEFT))  { high_n -= 0.01f; dirty = 1; }
    if (low_n  < 0) low_n  = 0; if (low_n  > 1) low_n  = 1;
    if (high_n < 0) high_n = 0; if (high_n > 1) high_n = 1;

    if (dirty) { apply(); dirty = 0; }

    static const int bass[8] = { 36, 36, 43, 36, 41, 36, 48, 43 };
    static const int lead[8] = { 72, 79, 76, 84, 77, 81, 79, 76 };
    if (every(1)) { note(bass[beat() & 7], I_BASS, 6); note(lead[beat() & 7], I_LEAD, 3); }
}

// the engine's shape: low band = LP@80Hz, top band = HP@6kHz, mid stays unity. Approximate the
// combined magnitude per frequency so the drawn curve matches what you hear (boost lifts, cut dips).
static float resp_db(float f, float lg, float hg) {
    float wl = 1.0f / (1.0f + (f / 80.0f) * (f / 80.0f));         // low-band weight (1 at DC → 0 high)
    float r  = (f / 6000.0f) * (f / 6000.0f);
    float wh = r / (1.0f + r);                                    // top-band weight (0 low → 1 high)
    float wm = 1.0f - wl - wh; if (wm < 0) wm = 0;                // mid (flat)
    float lin = wl * lg + wm + wh * hg;
    if (lin < 0.0001f) lin = 0.0001f;
    return 20.0f * log10f(lin);
}

#define CURVE_X 20
#define CURVE_Y 40
#define CURVE_W (SCREEN_W - 40)
#define CURVE_H 86

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print_centered("EQ", SCREEN_W / 2, 8, CLR_WHITE);
    print_centered("2-band shelving tone - boost or cut", SCREEN_W / 2, 22, CLR_MEDIUM_GREY);

    float lo = db_of(low_n), hi = db_of(high_n);
    float lg = powf(10.0f, lo / 20.0f), hg = powf(10.0f, hi / 20.0f);
    if (route == 2 && !amp) { lg = 1.0f; hg = 1.0f; }   // OFF: draw flat

    // ── response curve panel ── 0 dB line in the middle, ±12 dB top/bottom, log frequency axis
    rect(CURVE_X - 1, CURVE_Y - 1, CURVE_W + 2, CURVE_H + 2, CLR_DARKER_GREY);
    int midY = CURVE_Y + CURVE_H / 2;
    line(CURVE_X, midY, CURVE_X + CURVE_W, midY, CLR_DARK_GREY);             // 0 dB
    print("+12", CURVE_X + 1, CURVE_Y + 1, CLR_DARKER_GREY);
    print("-12", CURVE_X + 1, CURVE_Y + CURVE_H - 8, CLR_DARKER_GREY);
    // pivot markers: ~80 Hz (low shelf) and ~6 kHz (high shelf), placed on the 3-decade log axis
    int lpx = CURVE_X + (int)(log10f(80.0f   / 20.0f) / 3.0f * CURVE_W);
    int hpx = CURVE_X + (int)(log10f(6000.0f / 20.0f) / 3.0f * CURVE_W);
    line(lpx, CURVE_Y, lpx, CURVE_Y + CURVE_H, CLR_DARK_GREY);
    line(hpx, CURVE_Y, hpx, CURVE_Y + CURVE_H, CLR_DARK_GREY);
    print("80Hz",  lpx - 8, CURVE_Y + CURVE_H - 8, CLR_DARKER_GREY);
    print("6kHz",  hpx - 8, CURVE_Y + CURVE_H - 8, CLR_DARKER_GREY);
    // the curve itself
    int prev = midY;
    for (int px = 0; px < CURVE_W; px++) {
        float f = 20.0f * powf(10.0f, (px / (float)CURVE_W) * 3.0f);        // 20 Hz → 20 kHz
        float db = resp_db(f, lg, hg);
        int y = midY - (int)(db / 12.0f * (CURVE_H / 2.0f));
        if (y < CURVE_Y) y = CURVE_Y; if (y > CURVE_Y + CURVE_H) y = CURVE_Y + CURVE_H;
        if (px > 0) line(CURVE_X + px - 1, prev, CURVE_X + px, y, CLR_LIME_GREEN);
        prev = y;
    }

    // ── controls ──
    char buf[32];
    ui_begin();
    snprintf(buf, sizeof buf, "LOW  %+.0f dB", lo);
    if (ui_slider(&low_n,  20, 140, SCREEN_W - 40, buf)) dirty = 1;
    snprintf(buf, sizeof buf, "HIGH %+.0f dB", hi);
    if (ui_slider(&high_n, 20, 160, SCREEN_W - 40, buf)) dirty = 1;
    ui_end();

    // route banner (A) + amp toggle (B)
    int rc = route == 0 ? CLR_ORANGE : (route == 1 ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rectfill(20, 178, 180, 14, rc);
    print(ROUTE_NAME[route], 24, 180, CLR_BLACK);
    int ac = amp ? CLR_RED : CLR_DARKER_GREY;
    rectfill(SCREEN_W - 60, 178, 40, 14, ac);
    print_centered("AMP", SCREEN_W - 40, 180, amp ? CLR_WHITE : CLR_MEDIUM_GREY);
    if (tapp(SCREEN_W - 60, 178, 40, 14)) { amp = !amp; dirty = 1; }

    print_centered("drag faders   A route   B amp", SCREEN_W / 2, 194, CLR_MEDIUM_GREY);
}
