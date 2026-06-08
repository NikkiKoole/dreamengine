// solo.h — the radio jam layer: a scale-locked solo strip, cart-land.
//
// The companion to radio.h (game-music.md → "the jam layer"). Every station
// already computes its key, scale, and current chord; this header turns that
// into a playable instrument you CAN'T play wrong: a horizontal strip of only
// the in-key notes, the current chord's tones lit, monophonic with portamento
// so drags slide like a stylophone / Omnichord. The station owns the VOICE (an
// I_SOLO instrument tuned to sit on top of the mix); solo.h owns the
// INTERACTION (capture, scale-lock, glide, optional beat-quantize, the strip).
//
// Built on ui.h's per-finger capture (the mouse is one synthetic finger, so
// it's mouse-and-touch for free) — so it MUST be called between ui_begin() and
// ui_end(), exactly like the rad_knob_* controls. radio.h already pulls ui.h
// in; just #include "solo.h" after it.
//
// Usage — the station fills a SoloCtx from data it already has, then calls the
// strip once in draw() (between ui_begin/ui_end):
//
//   int chord[4]; int nch = current_chord_pcs(chord);   // the cart's own chord
//   SoloCtx jc = { .root = sng.keyPc, .scale = PENT, .nscale = 5,
//                  .chordPcs = chord, .nchordPcs = nch,
//                  .instr = I_SOLO, .loMidi = 72, .hiMidi = 91,
//                  .quantize = false };
//   solo_strip(&jc, 28, 168, 264, 22, CLR_PINK);   // x,y,w,h, accent
//
// Closed, it draws a small "jam" tab (tap it, or press J) — so it works on a
// phone with no keyboard. Open, the strip replaces that corner. solo_midi()
// returns the note sounding right now (-1 = silent) for display or a trace.
//
// Everything static (gestures.h / improv.h / ui.h pattern).

#ifndef SOLO_H
#define SOLO_H

#include "studio.h"
#include "ui.h"
#include <math.h>

#ifndef SOLO_KEY
#define SOLO_KEY 'J'          // toggles the jam strip (override before include)
#endif
#ifndef SOLO_GLIDE_MS
#define SOLO_GLIDE_MS 55      // portamento between cells — the stylophone slide
#endif
#ifndef SOLO_VOL
#define SOLO_VOL 5            // the solo sits ABOVE the backing track
#endif

typedef struct {
    int  root;                // key root pitch class (0..11)
    const int *scale;         // in-key semitone offsets (0..11)…
    int  nscale;              // …and how many (5 pentatonic, 7 diatonic)
    const int *chordPcs;      // current chord's pitch classes to LIGHT (or NULL)
    int  nchordPcs;
    int  instr;               // the station's I_SOLO slot
    int  loMidi, hiMidi;      // the strip's playable register, inclusive
    bool quantize;            // snap each note-on to the next 16th
} SoloCtx;

static bool solo_open_f  = false;
static int  solo_handle  = -1;     // the held mono voice (-1 = silent)
static int  solo_curMidi = -1;     // what's sounding (display / trace)
static bool solo_pending = false;  // a press waiting for its 16th (quantize)
static int  solo_pendMidi = -1;
static long solo_pendStep = 0;

static bool solo_open(void) { return solo_open_f; }
static int  solo_midi(void) { return solo_curMidi; }   // -1 = silent

static void solo_kill(void) {
    if (solo_handle >= 0) note_off(solo_handle);
    solo_handle = -1; solo_curMidi = -1; solo_pending = false;
}

// build the ascending list of in-scale midis across [lo,hi]; returns count
static int solo_notes(const SoloCtx *cx, int *out, int max) {
    int n = 0;
    for (int m = cx->loMidi; m <= cx->hiMidi && n < max; m++) {
        int pc = ((m - cx->root) % 12 + 12) % 12;
        for (int s = 0; s < cx->nscale; s++)
            if (cx->scale[s] == pc) { out[n++] = m; break; }
    }
    return n;
}

static bool solo_is_chord_pc(const SoloCtx *cx, int midi) {
    if (!cx->chordPcs) return false;
    int pc = midi % 12;
    for (int i = 0; i < cx->nchordPcs; i++)
        if (cx->chordPcs[i] % 12 == pc) return true;
    return false;
}

// the whole layer: toggle tab when closed, the playable strip when open.
// call once per frame in draw(), between ui_begin() and ui_end().
static void solo_strip(const SoloCtx *cx, int x, int y, int w, int h, int accent) {
    if (keyp(SOLO_KEY)) { solo_open_f = !solo_open_f; if (!solo_open_f) solo_kill(); }

    // ── closed: a small finger-sized "jam" tab (tap or J to open) ──────────
    if (!solo_open_f) {
        int tw = 34, tx = x + w - tw, ty = y;
        static int tab_id;
        ui_reg(&tab_id, tx, ty, tw, h, 0);
        UiCap *tc = ui_cap_for(&tab_id);
        if (tc && tc->released &&
            tc->rx >= tx - UI_HIT_PAD && tc->rx < tx + tw + UI_HIT_PAD &&
            tc->ry >= ty - UI_HIT_PAD && tc->ry < ty + h + UI_HIT_PAD)
            solo_open_f = true;
        bool hot = tc != 0 || ui_hover(tx, ty, tw, h);
        rectfill(tx, ty, tw, h, hot ? accent : CLR_DARKER_GREY);
        rect(tx, ty, tw, h, accent);
        print("jam", tx + (tw - text_width("jam")) / 2, ty + (h - 6) / 2, hot ? CLR_BLACK : accent);
        return;
    }

    // ── open: the scale-locked strip ──────────────────────────────────────
    int notes[48];
    int nn = solo_notes(cx, notes, 48);
    if (nn == 0) { solo_open_f = false; solo_kill(); return; }
    int cw = w / nn;
    if (cw < 1) cw = 1;

    static int strip_id;
    ui_reg(&strip_id, x, y, w, h, 0);
    UiCap *c = ui_cap_for(&strip_id);

    int cell = -1, midi = -1;
    if (c) {
        int px = (c->released ? c->rx : c->cx) - x;
        cell = px / cw;
        cell = cell < 0 ? 0 : cell >= nn ? nn - 1 : cell;
        midi = notes[cell];
    }

    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;   // 16th-step clock

    // note lifecycle: grab → on (quantized or now); drag → glide; release → off
    if (ui_grabbed(&strip_id) && midi >= 0) {
        if (cx->quantize) { solo_pending = true; solo_pendMidi = midi; solo_pendStep = (long)pos; }
        else {
            solo_handle = note_on(midi, cx->instr, SOLO_VOL);
            note_glide(solo_handle, SOLO_GLIDE_MS);
            solo_curMidi = midi;
        }
    }
    if (solo_pending && (long)pos > solo_pendStep) {        // the quantized strike lands
        solo_handle = note_on(solo_pendMidi, cx->instr, SOLO_VOL);
        note_glide(solo_handle, SOLO_GLIDE_MS);
        solo_curMidi = solo_pendMidi;
        solo_pending = false;
    }
    if (c && !c->released && solo_handle >= 0 && midi >= 0 && midi != solo_curMidi) {
        note_pitch(solo_handle, midi);                      // slide to the new degree
        solo_curMidi = midi;
    }
    if (ui_released(&strip_id)) solo_kill();

    // ── draw the keybed ────────────────────────────────────────────────────
    rectfill(x, y, w, h, CLR_BLACK);
    for (int i = 0; i < nn; i++) {
        int col = (i == cell)                       ? CLR_WHITE
                : solo_is_chord_pc(cx, notes[i])    ? accent
                : (notes[i] % 12 == cx->root)       ? CLR_MEDIUM_GREY   // the tonic
                                                    : CLR_DARK_GREY;
        rectfill(x + i * cw + 1, y + 1, cw - 2, h - 2, col);
    }
    rect(x, y, w, h, accent);
    print("J", x + 2, y - 8, accent);                       // the close hint
}

#endif // SOLO_H
