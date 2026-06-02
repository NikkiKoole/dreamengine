#include "studio.h"

// MODRACK — a tiny modular synth, built in steps (see docs/design/modular-synth.md).
//
// STEP 1: the generative chain is HARDCODED in C — no cables, no editing yet. Wiring it
// by hand first is the point: you see exactly what each module does before any cable
// hides it. The classic Berlin-school patch:
//
//   CLOCK ──tick──► S&H ◄──samples── LFO
//                    └──value──► QUANT ──pitch──► VOICE
//   LFO ───────────────────────────────────────► VOICE filter cutoff (live, every frame)
//
// A slow LFO wanders; the S&H freezes it on each clock step; QUANT snaps that to a scale
// so it's always in key; VOICE plays it as a held note whose filter the same LFO sweeps
// live. Endless in-key melody, zero composition. Later steps draw real module strips and
// make the cables editable.

#define SLOT 5

const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

int   tempo    = 112;
int   scale    = SCALE_PENTA;   // major pentatonic — always pretty
int   root_oct = 4;
float lfo_phase = 0, lfo_rate = 0.37f;   // Hz — deliberately not clock-locked → it wanders
float lfo_out  = 0;             // LFO output 0..1
float sh       = 0;             // sample & hold: the LFO value frozen at the last tick
int   cur_midi = 60;            // QUANT output (MIDI note)
int   voice    = -1;            // VOICE: held-note handle
int   last_step = -1;           // for clock-edge detection
int   tick_flash = 99;          // frames since the last clock tick (LED)
float cutoff   = 600;

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 90, 3, 260);     // a plucky saw with some sustain
    instrument_filter(SLOT, FILTER_LOW, 600, 10);   // resonant lowpass for the LFO to sweep
    bpm(tempo);
}

void update(void) {
    bpm(tempo);

    // CLOCK — 8th-note steps (a tick = a rising gate edge)
    int step8 = beat() * 2 + (int)(beat_pos() * 2.0f);
    bool tick = step8 != last_step;
    if (tick) { last_step = step8; tick_flash = 0; } else tick_flash++;

    // LFO — a slow wandering 0..1
    lfo_phase += lfo_rate * dt();
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    lfo_out = (sin_deg(lfo_phase * 360.0f) + 1.0f) * 0.5f;

    // on each tick: S&H freezes the LFO → QUANT snaps to a scale → VOICE retriggers
    if (tick) {
        sh = lfo_out;                              // sample & hold
        int n = (int)(sh * 7.999f);                // 0..7
        cur_midi = degree(scale, root_oct, n);     // quantize to the scale
        if (voice >= 0) note_off(voice);
        voice = note_on(cur_midi, SLOT, 5);        // (re)trigger the held voice
    }

    // VOICE filter CV — the LFO sweeps the cutoff of the RINGING note, live every frame
    // (this is the held-note move; plain note() couldn't touch a sounding voice)
    cutoff = 300.0f + lfo_out * 2600.0f;
    if (voice >= 0) note_cutoff(voice, (int)cutoff);
}

// ── Step 2: draw the rack as real Eurorack-style strips (still no interaction) ──
// signal-type colours: 0 gate (green), 1 pitch (yellow), 2 CV (cyan-ish)
int sig_col(int t) { return t == 0 ? CLR_GREEN : t == 1 ? CLR_YELLOW : CLR_BLUE_GREEN; }

// proximity reveal: a label is dim at rest and brightens as the cursor nears it,
// so the cramped 46px strips stay legible without permanent clutter.
int near_col(int x, int y) {
    float d = distance(mouse_x(), mouse_y(), x, y);
    return d < 14 ? CLR_WHITE : d < 30 ? CLR_LIGHT_GREY : d < 54 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
}

// a knob: dark dial with a tick pointing at value v (0..1), sweeping 135°..405°
void knob(int cx, int cy, float v) {
    circfill(cx, cy, 6, CLR_DARKER_GREY);
    circ(cx, cy, 6, CLR_MEDIUM_GREY);
    float a = 135.0f + clamp(v, 0, 1) * 270.0f;
    line(cx, cy, cx + (int)(cos_deg(a) * 5), cy + (int)(sin_deg(a) * 5), CLR_WHITE);
}

// a patch jack: hollow ring = output, filled = input; coloured by signal type; lit pulses white
void jack(int cx, int cy, int type, bool out, bool lit) {
    int c = sig_col(type);
    if (out) { circ(cx, cy, 3, c); if (lit) circfill(cx, cy, 1, CLR_WHITE); }
    else       circfill(cx, cy, 3, lit ? CLR_WHITE : c);
}

// a labelled knob: knob + its name (proximity-dimmed) + value when the cursor is near
void knob_l(int cx, int cy, float v, const char *name, const char *val) {
    knob(cx, cy, v);
    print(name, cx - text_width(name) / 2, cy + 9, near_col(cx, cy));
    if (distance(mouse_x(), mouse_y(), cx, cy) < 16) print(val, cx - text_width(val) / 2, cy + 17, CLR_WHITE);
}

// a labelled jack: jack + its name (proximity-dimmed)
void jack_l(int cx, int cy, int type, bool out, bool lit, const char *name) {
    jack(cx, cy, type, out, lit);
    print(name, cx - text_width(name) / 2, cy + 6, near_col(cx, cy));
}

void meter(int x, int y, int w, int h, float v, int col) {   // vertical 0..1 output meter
    int fill = (int)(clamp(v, 0, 1) * h);
    rectfill(x, y, w, h, CLR_BLACK);
    rectfill(x, y + h - fill, w, fill, col);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("MODRACK", 6, 4, CLR_WHITE);
    print("step2: the rack", 84, 5, CLR_INDIGO);

    const char *nm[6] = { "CLOCK", "LFO", "S&H", "QUANT", "VOICE", "EUCLID" };
    int col[6]        = { CLR_ORANGE, CLR_PINK, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_DARK_GREY };
    int y = 18, h = 156;

    for (int i = 0; i < 6; i++) {
        int x = 6 + i * 52, w = 46, cx = x + w / 2;
        bool placeholder = (i == 5);                       // EUCLID lands in step 6
        bool gate_lit = tick_flash < 5;

        rectfill(x, y, w, h, placeholder ? CLR_BROWNISH_BLACK : CLR_DARKER_PURPLE);
        rect(x, y, w, h, col[i]);
        print(nm[i], x + 3, y + 3, col[i]);

        switch (i) {
            case 0:  // CLOCK — BPM knob, pulse LED, three gate outs (/1 /2 /4)
                circfill(cx, y + 16, 4, gate_lit ? CLR_WHITE : CLR_DARK_ORANGE);
                knob_l(cx, y + 42, (tempo - 60) / 180.0f, "bpm", str("%d", tempo));
                jack_l(x + 11, y + 132, 0, true, gate_lit, "1");
                jack_l(cx,     y + 132, 0, true, false,    "2");
                jack_l(x + 35, y + 132, 0, true, false,    "4");
                break;
            case 1:  // LFO — rate knob, live CV out
                knob_l(cx, y + 36, lfo_rate / 8.0f, "rate", str("%.1f", lfo_rate));
                meter(x + 8, y + 60, 6, 48, lfo_out, CLR_PINK);
                jack_l(cx, y + 132, 2, true, false, "cv");
                break;
            case 2:  // S&H — CV+clock in, held CV out
                jack_l(x + 13, y + 30, 2, false, false,    "in");
                jack_l(x + 33, y + 30, 0, false, gate_lit, "clk");
                meter(x + 8, y + 56, 6, 52, sh, CLR_YELLOW);
                jack_l(cx, y + 132, 2, true, false, "cv");
                break;
            case 3:  // QUANT — scale knob, CV in, pitch out, the note it picked
                jack_l(cx, y + 26, 2, false, false, "in");
                knob_l(cx, y + 50, scale / 5.0f, "scl", "penta");
                print(NOTES[cur_midi % 12], cx - text_width(NOTES[cur_midi % 12]) / 2, y + 84, CLR_WHITE);
                jack_l(cx, y + 132, 1, true, gate_lit, "pit");
                break;
            case 4:  // VOICE — cutoff knob, three CV ins, trigger LED, live cutoff meter
                jack_l(x + 9,  y + 26, 0, false, gate_lit, "g");
                jack_l(cx,     y + 26, 1, false, gate_lit, "p");
                jack_l(x + 37, y + 26, 2, false, false,    "f");
                knob_l(cx, y + 52, 0.45f, "cut", str("%d", (int)cutoff));
                meter(x + 8, y + 72, 6, 40, (cutoff - 300) / 2600.0f, CLR_BLUE);
                if (tick_flash < 8) circ(cx, y + 52, 9 - tick_flash, CLR_LIGHT_PEACH);
                break;
            case 5:  // EUCLID — placeholder until step 6
                knob_l(cx, y + 40, 0.4f, "hits", "—");
                jack_l(cx, y + 132, 0, true, false, "g");
                print("step6", x + 6, y + h - 12, CLR_DARK_GREY);
                break;
        }
    }

    print("rack is display-only - step3 adds knobs", 6, 180, CLR_DARKER_GREY);
    print("hover a knob to read its value", 6, 190, CLR_DARK_GREY);
}
