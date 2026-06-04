#include "studio.h"
#include <math.h>

// Loopstation — a live-looper pedal: the first cart that RECORDS ITSELF.
// Four instruments share one 4-bar loop. Arm a track, play, and what you played
// comes back around — then layer the next instrument on top. A one-person band.
//
// What it demonstrates (see docs/design/input-recording-looper.md): recording at
// the AUDIO CONTROL EVENT level, MIDI's discovery —
//   * one-shots (drums, bass, lead) record as discrete NOTE events, quantized to
//     the nearest 16th so your sloppy timing comes back tight;
//   * the THEREMIN records as a CC STREAM — note_pitch/note_vol sampled only on
//     frames where your hand actually moved — and replays the gesture verbatim,
//     drawing a ghost crosshair where your hand was. Gestures don't quantize.
// The loop ring shows every event as a dot on its track's circle.
//
// CONTROLS: Z/X/C drums · A..K bass · Q..I lead · drag the pad = theremin ·
// 1-4 select track · SPACE arm/disarm record · M mute · BACKSPACE clear · N click.

#define LOOP_BEATS 16.0f     // 4 bars of 4/4
#define QSTEP      0.25f     // one-shots quantize to 16ths
#define TEMPO      110

#define NTRK   4
#define MAXEV  1024
enum { T_DRUM, T_BASS, T_LEAD, T_THER };
enum { EV_NOTE, EV_CC, EV_OFF };

// instrument slots
#define SL_KICK   5
#define SL_SNARE  6
#define SL_HAT    7
#define SL_BASS   8
#define SL_LEAD   9
#define SL_THER  10
#define SL_CLICK 11

// theremin pad geometry + pitch range
#define TPX 240
#define TPY 120
#define TPW 72
#define TPH 52
#define TH_LO 57.0f
#define TH_HI 81.0f

typedef struct { float pos; int kind, slot, vol, dur, born; float pitch; int aux; } Ev;
typedef struct { Ev ev[MAXEV]; int n; int armed, muted; } Track;

static Track trk[NTRK];
static int   sel;                    // selected track (1-4 keys)
static int   metro = 1;              // click track on/off

// transport — continuous beat time sliced into a loop position
static float songb, lp, prev_lp;
static int   loop_i;                 // which pass of the loop we're on

// theremin voices: one for the live hand, one for the recorded ghost
static int   vlive, vrep;
static int   ther_on;                // live gesture in progress
static float rec_p; static int rec_v;        // last recorded CC values (delta filter)
static float gho_p; static int gho_v, gho_on; // ghost (replay) state for drawing

// visual flash timers
static int padv[3], keyv[2][8], lamp;

static const char BK[8] = { 'A','S','D','F','G','H','J','K' };
static const char LK[8] = { 'Q','W','E','R','T','Y','U','I' };
static const int  TCOL[NTRK] = { CLR_ORANGE, CLR_BLUE, CLR_PINK, CLR_GREEN };
static const int  TRAD[NTRK] = { 16, 24, 32, 40 };
static const char *TNAME[NTRK] = { "DRUMS", "BASS", "LEAD", "THEREMIN" };

// ---- recording -------------------------------------------------------------

// append an event to track t's loop (only while that track is record-armed).
// NOTE events quantize to the 16th grid; `born` marks the loop pass whose
// crossing must be SKIPPED so a forward-quantized note doesn't double-fire
// right after you heard it live.
static void rec_ev(int t, int kind, int slot, float pitch, int vol, int dur, int aux) {
    Track *T = &trk[t];
    if (!T->armed || T->n >= MAXEV) return;
    float pos = lp; int born = -1;
    if (kind == EV_NOTE) {
        pos = roundf(lp / QSTEP) * QSTEP;
        if      (pos >= LOOP_BEATS) { pos -= LOOP_BEATS; born = loop_i + 1; }  // wrapped to the next pass's downbeat
        else if (pos > lp)            born = loop_i;                            // later this pass
    }
    T->ev[T->n++] = (Ev){ pos, kind, slot, vol, dur, born, pitch, aux };
}

// play a one-shot live AND record it (the looper hears what you play)
static void play_note(int t, int slot, int midi, int vol, int dur, int aux) {
    hit(midi, slot, vol, dur);
    rec_ev(t, EV_NOTE, slot, midi, vol, dur, aux);
}

// ---- playback --------------------------------------------------------------

static void fire_ev(int t, Ev *e) {
#ifdef DE_TRACE
    watch("fire", "t%d k%d pos=%.2f pitch=%.1f", t, e->kind, e->pos, e->pitch);
#endif
    if (e->kind == EV_NOTE) {
        hit((int)e->pitch, e->slot, e->vol, e->dur);
        if      (t == T_DRUM) padv[e->aux]    = 4;     // replay flashes the UI too
        else if (t == T_BASS) keyv[1][e->aux] = 4;
        else if (t == T_LEAD) keyv[0][e->aux] = 4;
    } else if (e->kind == EV_CC) {
        note_pitch(vrep, e->pitch); note_vol(vrep, e->vol);
        gho_p = e->pitch; gho_v = e->vol; gho_on = 1;
    } else {  // EV_OFF
        note_vol(vrep, 0); gho_on = 0;
    }
}

// fire every event the playhead crossed since last frame (wrap-aware)
static void fire_replay(void) {
    int wrap = lp < prev_lp;
    for (int t = 0; t < NTRK; t++) {
        Track *T = &trk[t];
        if (T->muted) continue;
        for (int i = 0; i < T->n; i++) {
            Ev *e = &T->ev[i];
            int hitnow = wrap ? (e->pos > prev_lp || e->pos <= lp)
                              : (e->pos > prev_lp && e->pos <= lp);
            if (!hitnow) continue;
            int it = (wrap && e->pos > prev_lp) ? loop_i - 1 : loop_i;  // which pass this crossing belongs to
            if (e->born == it) continue;                                // already heard live
            fire_ev(t, e);
        }
    }
}

static void silence_ther_replay(void) { note_vol(vrep, 0); gho_on = 0; }

// ---- setup -----------------------------------------------------------------

void init(void) {
    bpm(TEMPO);
    // drums
    instrument(SL_KICK, INSTR_SINE, 0, 130, 0, 60);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 90, 26);        // the punch: starts sharp, drops to the thump
    instrument(SL_SNARE, INSTR_NOISE, 0, 90, 0, 80);
    instrument_filter(SL_SNARE, FILTER_BAND, 1800, 4);
    instrument(SL_HAT, INSTR_NOISE, 0, 25, 0, 30);
    instrument_filter(SL_HAT, FILTER_HIGH, 6500, 6);
    // bass: saw pluck with a closing filter
    instrument(SL_BASS, INSTR_SAW, 2, 160, 4, 120);
    instrument_filter(SL_BASS, FILTER_LOW, 750, 7);
    instrument_env(SL_BASS, 0, ENV_CUTOFF, 0, 140, 1400);
    // lead: thin pulse with vibrato
    instrument(SL_LEAD, INSTR_SQUARE, 4, 120, 5, 160);
    instrument_duty(SL_LEAD, 0.25f);
    instrument_lfo(SL_LEAD, 0, LFO_PITCH, 5.5f, 0.25f);
    // theremin: sine swell — TWO persistent voices, your hand and its ghost
    instrument(SL_THER, INSTR_SINE, 40, 100, 7, 200);
    instrument_lfo(SL_THER, 0, LFO_PITCH, 5.5f, 0.3f);
    vlive = note_on(60, SL_THER, 0);  note_glide(vlive, 30);
    vrep  = note_on(60, SL_THER, 0);  note_glide(vrep, 30);
    // metronome click
    instrument(SL_CLICK, INSTR_TRI, 0, 30, 0, 20);
}

// ---- update ----------------------------------------------------------------

void update(void) {
    // transport: slice continuous beat time into loop position + pass counter
    songb  = beat() + beat_pos();
    lp     = fmodf(songb, LOOP_BEATS);
    loop_i = (int)(songb / LOOP_BEATS);

    fire_replay();

    // click track — accent on the bar
    if (metro && every(1)) {
        int acc = ((int)songb % 4) == 0;
        hit(acc ? 93 : 81, SL_CLICK, acc ? 4 : 2, 25);
        lamp = 4;
    }

    // ---- track controls ----
    for (int t = 0; t < NTRK; t++) if (keyp('1' + t)) sel = t;
    if (keyp(KEY_SPACE)) trk[sel].armed ^= 1;
    if (keyp('M')) {
        trk[sel].muted ^= 1;
        if (sel == T_THER && trk[sel].muted) silence_ther_replay();
    }
    if (keyp(KEY_BACKSPACE)) {
        trk[sel].n = 0;
        if (sel == T_THER) silence_ther_replay();
    }
    if (keyp('N')) metro ^= 1;

    // ---- drums ----
    if (keyp('Z')) { play_note(T_DRUM, SL_KICK,  36, 7, 120, 0); padv[0] = 5; }
    if (keyp('X')) { play_note(T_DRUM, SL_SNARE, 55, 6, 110, 1); padv[1] = 5; }
    if (keyp('C')) { play_note(T_DRUM, SL_HAT,   84, 4,  30, 2); padv[2] = 5; }

    // ---- bass + lead (minor pentatonic, two octaves apart) ----
    for (int i = 0; i < 8; i++) {
        if (keyp(BK[i])) { play_note(T_BASS, SL_BASS, degree(SCALE_PENTA_MIN, 2, i), 7, 260, i); keyv[1][i] = 5; }
        if (keyp(LK[i])) { play_note(T_LEAD, SL_LEAD, degree(SCALE_PENTA_MIN, 4, i), 6, 200, i); keyv[0][i] = 5; }
    }

    // ---- theremin: drag the pad. y = pitch, x = volume. Records as a CC
    // stream, but only on frames where the hand actually moved (delta filter).
    int mx = mouse_x(), my = mouse_y();
    int inpad = mx >= TPX && mx < TPX + TPW && my >= TPY && my < TPY + TPH;
    if (mouse_down(MOUSE_LEFT) && inpad) {
        float p = remap(my, TPY, TPY + TPH, TH_HI, TH_LO);          // top = high
        int   v = (int)clamp(remap(mx, TPX, TPX + TPW, 2.4f, 7.4f), 1, 7);
        note_pitch(vlive, p); note_vol(vlive, v);
        if (!ther_on || fabsf(p - rec_p) > 0.08f || v != rec_v) {
            rec_ev(T_THER, EV_CC, SL_THER, p, v, 0, 0);
            rec_p = p; rec_v = v;
        }
        ther_on = 1;
    } else if (ther_on) {
        note_vol(vlive, 0);
        rec_ev(T_THER, EV_OFF, SL_THER, 0, 0, 0, 0);
        ther_on = 0;
    }

    // decay flash timers
    for (int i = 0; i < 3; i++) if (padv[i] > 0) padv[i]--;
    for (int r = 0; r < 2; r++) for (int i = 0; i < 8; i++) if (keyv[r][i] > 0) keyv[r][i]--;
    if (lamp > 0) lamp--;

#ifdef DE_TRACE
    watch("lp", "%.2f pass=%d", lp, loop_i);
    watch("trk", "%d/%d/%d/%d", trk[0].n, trk[1].n, trk[2].n, trk[3].n);
#endif

    prev_lp = lp;
}

// ---- draw ------------------------------------------------------------------

void draw(void) {
    cls(CLR_BLACK);
    print("LOOPSTATION", 6, 6, CLR_LIGHT_PEACH);
    circfill(110, 9, 3, lamp ? CLR_ORANGE : (metro ? CLR_DARKER_GREY : CLR_BLACK));
    if (!metro) circ(110, 9, 3, CLR_DARKER_GREY);
    print_right(str("%d BPM  4 BARS", TEMPO), 314, 6, CLR_INDIGO);

    // ---- the loop ring: one circle per track, every event a dot ----
    int cx = 52, cy = 66;
    for (int i = 0; i < 16; i++) {                              // beat ticks (bars brighter)
        float a = i / 16.0f * 360 - 90;
        int big = (i % 4) == 0;
        line(cx + (int)dx(big ? 42 : 44, a), cy + (int)dy(big ? 42 : 44, a),
             cx + (int)dx(46, a),            cy + (int)dy(46, a),
             big ? CLR_LIGHT_GREY : CLR_DARKER_GREY);
    }
    for (int t = 0; t < NTRK; t++) {
        Track *T = &trk[t];
        circ(cx, cy, TRAD[t], T->muted ? CLR_DARKER_GREY : CLR_DARK_BLUE);
        for (int i = 0; i < T->n; i++) {
            Ev *e = &T->ev[i];
            float a = e->pos / LOOP_BEATS * 360 - 90;
            int x = cx + (int)dx(TRAD[t], a), y = cy + (int)dy(TRAD[t], a);
            int col = T->muted ? CLR_DARK_GREY : TCOL[t];
            if (e->kind == EV_NOTE) circfill(x, y, 1, col);
            else if (e->kind == EV_CC) pset(x, y, col);          // a gesture draws as an arc
        }
    }
    float ph = lp / LOOP_BEATS * 360 - 90;                       // playhead
    line(cx, cy, cx + (int)dx(44, ph), cy + (int)dy(44, ph), CLR_WHITE);
    int anyarmed = trk[0].armed || trk[1].armed || trk[2].armed || trk[3].armed;
    circfill(cx, cy, 2, (anyarmed && blink(15)) ? CLR_RED : CLR_LIGHT_GREY);

    // ---- track panel ----
    for (int t = 0; t < NTRK; t++) {
        Track *T = &trk[t];
        int ry = 24 + t * 21;
        if (t == sel) { rectfill(102, ry - 4, 212, 17, CLR_DARKER_BLUE); rect(102, ry - 4, 212, 17, CLR_DARK_BLUE); }
        print(str("%d", t + 1), 108, ry, CLR_DARK_GREY);
        print(TNAME[t], 122, ry, T->muted ? CLR_DARK_GREY : TCOL[t]);
        if (T->armed) {
            if (blink(16)) circfill(210, ry + 3, 3, CLR_RED);
            print("REC", 218, ry, CLR_RED);
        }
        if (T->muted) print("MUTE", 248, ry, CLR_DARK_GREY);
        font(FONT_SMALL);
        print_right(T->n >= MAXEV ? "FULL" : str("%d", T->n), 310, ry + 2,
                    T->n >= MAXEV ? CLR_RED : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }

    // ---- drum pads ----
    static const char *DN[3] = { "KICK", "SNARE", "HAT" };
    static const char  DK[3] = { 'Z', 'X', 'C' };
    for (int i = 0; i < 3; i++) {
        int x = 8 + i * 27;
        rectfill(x, 128, 24, 36, padv[i] ? CLR_ORANGE : CLR_DARKER_BLUE);
        rect(x, 128, 24, 36, CLR_DARK_BLUE);
        print(str("%c", DK[i]), x + 8, 138, padv[i] ? CLR_BLACK : CLR_ORANGE);
        font(FONT_SMALL); print(DN[i], x + 2, 152, CLR_MEDIUM_GREY); font(FONT_NORMAL);
    }

    // ---- melodic keys: lead letter on top, bass letter below ----
    for (int i = 0; i < 8; i++) {
        int x = 92 + i * 17;
        rectfill(x, 124, 15, 23, keyv[0][i] ? CLR_PINK : CLR_DARKER_BLUE);   // lead half
        rectfill(x, 148, 15, 23, keyv[1][i] ? CLR_BLUE : CLR_DARKER_BLUE);   // bass half
        rect(x, 124, 15, 47, CLR_DARK_BLUE);
        print(str("%c", LK[i]), x + 4, 132, keyv[0][i] ? CLR_BLACK : CLR_PINK);
        print(str("%c", BK[i]), x + 4, 156, keyv[1][i] ? CLR_BLACK : CLR_BLUE);
    }

    // ---- theremin pad: your hand solid, the recorded ghost hollow ----
    vgradient(TPX, TPY, TPW, TPH, CLR_DARKER_PURPLE, CLR_BLACK);
    rect(TPX, TPY, TPW, TPH, ther_on ? CLR_GREEN : CLR_DARK_BLUE);
    font(FONT_SMALL); print("THEREMIN", TPX + 4, TPY + 3, CLR_MAUVE); font(FONT_NORMAL);
    if (gho_on) {                                                // the ghost hand
        int gy = (int)remap(gho_p, TH_HI, TH_LO, TPY, TPY + TPH);
        int gx = (int)remap((float)gho_v, 2.4f, 7.4f, TPX, TPX + TPW);
        circ(mid(TPX + 2, gx, TPX + TPW - 3), mid(TPY + 2, gy, TPY + TPH - 3), 3, CLR_GREEN);
    }
    if (ther_on) {
        int mx = mouse_x(), my = mouse_y();
        line(mx - 4, my, mx + 4, my, CLR_WHITE);
        line(mx, my - 4, mx, my + 4, CLR_WHITE);
    }

    // ---- HUD ----
    font(FONT_SMALL);
    print("Z X C drums   A-K bass   Q-I lead   drag the pad: theremin", 6, 178, CLR_INDIGO);
    print("1-4 select   SPACE rec   M mute   BKSP clear   N click", 6, 188, CLR_DARKER_GREY);
    font(FONT_NORMAL);
}
