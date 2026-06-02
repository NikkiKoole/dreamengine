#include "studio.h"

// Stylophone Deluxe — the pocket stylus-synth, built to reach the two corners of
// the audio engine the rest of the instrument cabinet never touches:
//
//   1. LIVE PULSE-WIDTH (note_duty). The stylophone's voice is a buzzy square; the
//      TONE fader morphs its pulse width by hand, thin/nasal → hollow/fat. No other
//      cart sweeps duty live.
//   2. THE TRANSPORT (bpm + beat + beat_pos). The famous "REITER" (reiteration)
//      switch retriggers the held note in time with the tempo — the cabinet's first
//      tempo-synced instrument. Crank RATE up and it machine-guns; flip ARP and the
//      retrigger walks a major arpeggio instead of repeating.
//
// Play it with the STYLUS (the mouse): touch the metal keyboard and slide — pitch
// steps pad-to-pad with the classic glissando. VIBRATO and SUB (sub-octave) are
// switches, just like the real deluxe panel.
//
// CONTROLS: hold the stylus on the keyboard to play · drag the faders · click the
// switches. TONE = pulse width · TEMPO/RATE drive the reiteration.

#define SLOT   5
#define BASE   53            // F3 — lowest pad
#define NPADS  15            // F3 .. G4, chromatic
#define KB_X   8
#define KB_Y   116
#define KB_W   304
#define KB_H   46
#define PAD_W  (KB_W / NPADS)

// fader geometry (shared x range, stacked) — leave room for the readout before the
// switch column at x=158
#define FX     46
#define FW     72

// --- panel state ---
static float duty  = 0.20f;  // pulse width  → note_duty
static float tempo = 120;    // bpm
static float rate  = 4;      // reiteration subdivisions per beat
static int   reiter, arp, vibrato, sub;     // switches
static int   ui_active = -1; // which fader is being dragged (-1 = none)

// --- voices / transport ---
static int   voice, sub_voice;
static int   active_pad = -1;
static int   last_slot = -99999, arp_idx, last_beat;
static int   tick_vis, pulse_vis;
static int   mx, my;

static int inbox(int x, int y, int w, int h) { return mx >= x && mx < x + w && my >= y && my < y + h; }

// a horizontal fader: returns the (possibly dragged) value. Drag logic only.
static float fader(int id, int fy, float val, float lo, float hi) {
    if (mouse_pressed(MOUSE_LEFT) && inbox(FX - 3, fy - 4, FW + 6, 12)) ui_active = id;
    if (ui_active == id) val = lo + clamp((float)(mx - FX) / FW, 0, 1) * (hi - lo);
    return val;
}

void init(void) {
    instrument(SLOT, INSTR_SQUARE, 6, 130, 6, 110);   // quick attack, buzzy pulse
    instrument_duty(SLOT, duty);
    instrument_filter(SLOT, FILTER_LOW, 2600, 3);     // tame the very top, stay nasal
    voice     = note_on(60, SLOT, 0);                 // sustained voice (reiter off)
    sub_voice = note_on(48, SLOT, 0);                 // its octave-down partner
}

void update(void) {
    mx = mouse_x(); my = mouse_y();
    int down = mouse_down(MOUSE_LEFT), pressed = mouse_pressed(MOUSE_LEFT);
    if (mouse_released(MOUSE_LEFT)) ui_active = -1;

    // ---- switches (toggle on click) ----
    if (pressed) {
        if      (inbox(158, 28, 72, 16)) reiter  = !reiter;
        else if (inbox(236, 28, 72, 16)) arp     = !arp;
        else if (inbox(158, 48, 72, 16)) vibrato = !vibrato;
        else if (inbox(236, 48, 72, 16)) sub     = !sub;
    }
    // ---- faders ----
    duty  = fader(0, 30, duty,  0.05f, 0.50f);
    tempo = fader(1, 46, tempo, 70,    180);
    rate  = fader(2, 62, rate,  1,     8);

    // ---- stylus on the keyboard (only if not dragging a fader) ----
    active_pad = -1;
    if (down && ui_active < 0 && inbox(KB_X, KB_Y, NPADS * PAD_W, KB_H)) {
        active_pad = (mx - KB_X) / PAD_W;
        if (active_pad >= NPADS) active_pad = NPADS - 1;
    }
    int playing = active_pad >= 0;
    int m = BASE + (playing ? active_pad : 0);

    // ---- audio ----
    bpm((int)tempo);
    instrument_duty(SLOT, duty);                      // for the reiteration hits
    float vdep = vibrato ? 0.6f : 0;
    instrument_lfo(SLOT, 0, LFO_PITCH, 6.0f, vdep);

    if (reiter) {
        // sustained voices off; retrigger in time with the transport instead
        note_vol(voice, 0); note_vol(sub_voice, 0);
        int subdiv = (int)(rate + 0.5f); if (subdiv < 1) subdiv = 1;
        int slot = (int)((beat() + beat_pos()) * subdiv);   // sub-beat tick index
        if (playing && slot != last_slot) {
            int step = 0;
            if (arp) { int pat[4] = { 0, 4, 7, 12 }; step = pat[arp_idx & 3]; arp_idx++; }
            hit(m + step, SLOT, 7, 90);
            if (sub) hit(m + step - 12, SLOT, 5, 90);
            tick_vis = 4;
        }
        last_slot = slot;
        if (!playing) arp_idx = 0;
    } else {
        arp_idx = 0;
        note_pitch(voice, m);  note_duty(voice, duty);
        note_lfo(voice, 0, LFO_PITCH, 6.0f, vdep);
        note_vol(voice, playing ? 7 : 0);
        note_pitch(sub_voice, m - 12);  note_duty(sub_voice, duty);
        note_vol(sub_voice, (playing && sub) ? 6 : 0);
    }

    // beat lamp + tick decay
    if (beat() != last_beat) { last_beat = beat(); pulse_vis = 6; }
    if (pulse_vis > 0) pulse_vis--;
    if (tick_vis  > 0) tick_vis--;
}

static void switch_btn(int x, int y, const char *label, int on, int col) {
    rectfill(x, y, 72, 16, on ? col : CLR_DARKER_BLUE);
    rect(x, y, 72, 16, inbox(x, y, 72, 16) ? CLR_WHITE : CLR_DARK_BLUE);
    print(label, x + 6, y + 5, on ? CLR_BLACK : CLR_MEDIUM_GREY);
}

static void draw_fader(int fy, const char *label, float val, float lo, float hi, const char *readout) {
    print(label, 8, fy + 1, CLR_LIGHT_PEACH);
    rectfill(FX, fy + 2, FW, 2, CLR_DARKER_GREY);
    int kx = FX + (int)((val - lo) / (hi - lo) * FW);
    rectfill(kx - 2, fy - 2, 4, 9, ui_active >= 0 ? CLR_WHITE : CLR_LIGHT_GREY);
    print(readout, FX + FW + 6, fy + 1, CLR_MEDIUM_GREY);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    // console face
    rectfill(4, 20, 312, 156, CLR_DARKER_BLUE);
    rect(4, 20, 312, 156, CLR_TRUE_BLUE);

    // ---- panel: faders + switches ----
    font(FONT_SMALL);
    char buf[16];
    int pct = (int)(duty * 100);
    buf[0]='P'; buf[1]='W'; buf[2]='M'; buf[3]=' ';
    buf[4]='0'+pct/10; buf[5]='0'+pct%10; buf[6]='%'; buf[7]=0;
    draw_fader(30, "TONE",  duty,  0.05f, 0.50f, buf);
    int bp = (int)tempo; char tb[5] = { (char)('0'+bp/100), (char)('0'+(bp/10)%10), (char)('0'+bp%10), 0, 0 };
    draw_fader(46, "TEMPO", tempo, 70, 180, tb);
    char rb[4] = { 'x', (char)('0' + (int)(rate + 0.5f)), 0, 0 };
    draw_fader(62, "RATE",  rate,  1, 8, rb);

    switch_btn(158, 28, "REITER",  reiter,  CLR_ORANGE);
    switch_btn(236, 28, "ARP",     arp,     CLR_YELLOW);
    switch_btn(158, 48, "VIBRATO", vibrato, CLR_GREEN);
    switch_btn(236, 48, "SUB",     sub,     CLR_PINK);

    // transport lamp — blinks on each beat, brighter on a reiteration tick
    int lampcol = tick_vis > 0 ? CLR_WHITE : pulse_vis > 0 ? CLR_ORANGE : CLR_DARKER_GREY;
    circfill(300, 78, 4, lampcol);
    print("BEAT", 266, 76, CLR_INDIGO);
    font(FONT_NORMAL);

    // ---- keyboard ----
    rect(KB_X - 2, KB_Y - 2, NPADS * PAD_W + 4, KB_H + 4, CLR_LIGHT_GREY);
    for (int i = 0; i < NPADS; i++) {
        int pc = (BASE + i) % 12;
        int sharp = (pc==1||pc==3||pc==6||pc==8||pc==10);
        int px = KB_X + i * PAD_W;
        int on = (i == active_pad);
        int col = on ? (tick_vis > 0 || !reiter ? CLR_YELLOW : CLR_ORANGE)
                     : sharp ? CLR_DARKER_GREY : CLR_MEDIUM_GREY;
        rectfill(px + 1, KB_Y, PAD_W - 2, KB_H, col);
        line(px, KB_Y, px, KB_Y + KB_H, CLR_DARK_BLUE);
    }

    // ---- the stylus (drawn over the keyboard while the pointer is on it) ----
    if (my >= KB_Y - 10 && my <= KB_Y + KB_H + 4 && mx >= KB_X && mx < KB_X + NPADS * PAD_W) {
        int tipy = my < KB_Y ? KB_Y : my;
        line(mx + 12, tipy - 20, mx, tipy, CLR_LIGHT_GREY);   // the pen body
        line(mx + 13, tipy - 20, mx + 1, tipy, CLR_DARK_GREY);
        circfill(mx, tipy, 2, active_pad >= 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
    }

    // ---- HUD ----
    print("STYLOPHONE DELUXE", 6, 6, CLR_LIGHT_PEACH);
    print("touch + slide the stylus to play", 6, 178, CLR_INDIGO);
    print("TONE=pulse width   REITER+RATE=rhythm", 6, 188, CLR_DARK_BLUE);
}
