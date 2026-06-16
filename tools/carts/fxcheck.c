#include "studio.h"
#include <stdio.h>

// FXCHECK — the engine's EFFECTS stability self-test cart. The twin of tunecheck.c
// (pitch) / the level sweep: it drives a loud sustained chord into the master bus and,
// one effect at a time, configures THAT effect at its DOCUMENTED EXTREME (max feedback /
// resonance / depth — the settings most likely to blow up), so `node tools/fx-check.js`
// can render it and assert the output stays FINITE and BOUNDED — no NaN-collapse, no DC
// runaway, no silent self-oscillation. This is a stability gate, not a character gate:
// whether an effect SOUNDS right is still by ear; this proves it doesn't break.
//
//   node tools/fx-check.js            # render this cart + report peak/rms/dc/clip per effect
//
// The SCHEDULE below is the source of truth. Each test window: at frame 0 reset ALL
// effects off (set-and-hold — once, never per-frame) then enable the one under test, and
// start a sustained chord; release it before a long gap so reverb/echo tails die before
// the next test. Every frame it watch()es the test index + a gate, so the analyzer reads
// ground truth from the trace. Test 0 is DRY (no effect) — the reference the others are
// compared against (an effect that doesn't move the signal off DRY is a dead/unwired fx).
//
// Pure timing harness — nothing to look at; run it headless via fx-check.js.

#define NF   56     // ~0.93s sounding @ 60fps (analyzer measures the stable middle)
#define GAP  28     // ~0.47s silence — long enough for a max-feedback reverb/echo tail to die
#define PER  (NF + GAP)
#define SLOT 5

// the master effects worth fuzzing at extremes. INSERTS process the whole mix (just call
// the config fn); SENDS (reverb/echo) also need a per-slot send level to be fed anything.
#define NTESTS 13   // 0=DRY, 1..12 = one effect each (keep in sync with fx-check.js FX_NAMES)

static int fnum = -1;
static int h0 = -1, h1 = -1, h2 = -1;

// reset EVERY effect to its off/neutral state (mix/depth/send = 0). Called once at the
// start of each test window so the effects never bleed into each other.
static void fx_all_off(void) {
    reverb(0, 0);                       instrument_reverb(SLOT, 0);
    echo(1, 0, 0);                      instrument_echo(SLOT, 0);
    chorus(1.5f, 0, 0);
    flanger(0.3f, 0, 0, 0);
    tape(0, 0, 0);
    wah_lfo(2, 0, 0);
    crush(16, 1, 0);
    eq(0, 0, 0);
    tremolo(4, 0, LFO_SHAPE_SINE);
    phaser(1, 0, 0, 0, 4);
    filter(FILTER_OFF, 1000, 0);
}

// enable exactly ONE effect at its documented EXTREME (the worst case for stability).
static void fx_enable(int t) {
    switch (t) {
        case 1:  reverb(1.0f, 0.0f); instrument_reverb(SLOT, 1.0f);   break; // endless bright hall (max tail)
        case 2:  echo(500, 1.1f, 1.0f); instrument_echo(SLOT, 1.0f);  break; // FEEDBACK > 1.0 — the runaway case
        case 3:  chorus(5.0f, 1.0f, 1.0f);                            break; // max rate/depth/wet
        case 4:  flanger(5.0f, 1.0f, 0.95f, 1.0f);                    break; // max + feedback (jet/metallic)
        case 5:  flanger(0.05f, 1.0f, -0.95f, 1.0f);                  break; // through-zero, max - feedback
        case 6:  tape(1.0f, 1.0f, 1.0f);                              break; // max wow/flutter/saturation
        case 7:  wah_lfo(10.0f, 1.0f, 1.0f);                          break; // max resonance quack
        case 8:  crush(1.0f, 0.05f, 1.0f);                            break; // 1-bit + heavy downsample, full wet
        case 9:  eq(15.0f, 15.0f, 15.0f);                             break; // +15 dB all bands (slams the limiter)
        case 10: tremolo(20.0f, 1.0f, LFO_SHAPE_SQUARE);              break; // max-rate hard chop
        case 11: phaser(10.0f, 1.0f, 0.95f, 1.0f, 8);                 break; // max feedback, 8 stages
        case 12: filter(FILTER_BAND, 300.0f, 0.99f);                  break; // near self-oscillation (resonant scream)
        default: break;                                                      // 0 = DRY (reference)
    }
}

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 40, 7, 200);   // bright/broadband + sustained = stresses every effect
}

void update(void) {
    fnum++;
    int idx   = fnum / PER;
    int local = fnum % PER;
    int gate  = (idx < NTESTS) && (local < NF);

    if (idx < NTESTS) {
        if (local == 0) {
            fx_all_off();          // isolate: clear everything, then turn on just this one
            fx_enable(idx);
            h0 = note_on(50, SLOT, 7);   // a loud sustained chord keeps the bus fed (worst case for feedback)
            h1 = note_on(57, SLOT, 6);
            h2 = note_on(62, SLOT, 6);
        } else if (local == NF - 1) {
            if (h0 >= 0) note_off(h0);
            if (h1 >= 0) note_off(h1);
            if (h2 >= 0) note_off(h2);
            h0 = h1 = h2 = -1;
        }
    }

#ifdef DE_TRACE
    watch("fx",   "%d", idx < NTESTS ? idx : -1);   // which effect test (0 = dry)
    watch("gate", "%d", gate);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("FXCHECK - run via tools/fx-check.js", 6, 6, CLR_WHITE);
    char buf[48];
    int idx = fnum < 0 ? 0 : fnum / PER;
    snprintf(buf, sizeof(buf), "test %d / %d", idx, NTESTS);
    print(buf, 6, 20, CLR_LIGHT_GREY);
}
