/* de:meta
{
  "title": "6b. chords and rhythm",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "chord-voicing",
    "euclidean-rhythm",
    "scale-quantize"
  ],
  "lineage": "the harmony/rhythm sibling of 06-sound (single-note melody)",
  "description": {
    "summary": "Stack chords from a scale and gate a hi-hat with a Euclidean rhythm.",
    "detail": "degree(scale, octave, n) picks a chord ROOT from a scale degree; chord()/strum() stack a triad on it (strum staggers the notes into a tiny arpeggio). euclid(hits, steps, b) spreads hits as evenly as possible across steps — the same algorithm behind claves and tresillo — gating a hi-hat under a I-IV-V-vi progression.",
    "controls": "left/right arrows change the hi-hat's euclid density"
  }
}
de:meta */
#include "studio.h"

// 6B. CHORDS AND RHYTHM — the harmony/rhythm sibling of 06-sound.
//
// 06-sound played one melody note at a time. Here: degree() picks a chord ROOT
// from a scale, chord()/strum() stack a triad on that root, and euclid() spreads
// a hi-hat evenly across a bar — the same Bjorklund algorithm behind the world's
// clave and tresillo patterns, not just a drum-machine trick.

#define STEPS 16   // one bar, 16th notes

// a I-IV-V-vi progression in C major — degree indices into SCALE_MAJOR
static const int   ROOT_DEGREE[4] = { 0, 3, 4, 5 };
static const int   CHORD_KIND[4]  = { CHORD_MAJ, CHORD_MAJ, CHORD_MAJ, CHORD_MIN };
static const char *CHORD_NAME[4]  = { "I", "IV", "V", "vi" };
static const char *CHORD_NOTES[4] = { "C E G", "F A C", "G B D", "A C E" };

static int hits = 5;        // euclid density — tweak with left/right
static int last_chord = -1;
static int last_step  = -1;

void update() {
    bpm(96);

    int chord_i = (beat() / 2) % 4;                  // a new chord every 2 beats
    if (chord_i != last_chord) {
        last_chord = chord_i;
        int root = degree(SCALE_MAJOR, 3, ROOT_DEGREE[chord_i]);
        strum(root, CHORD_KIND[chord_i], INSTR_TRI, 5, 35);
    }

    int step = beat() * 4 + (int)(beat_pos() * 4);   // 16th-note counter
    if (step != last_step) {
        last_step = step;
        if (euclid(hits, STEPS, step)) note(72, INSTR_NOISE, 3);
    }

    if (keyp(KEY_RIGHT)) hits = min(hits + 1, STEPS);
    if (keyp(KEY_LEFT))  hits = max(hits - 1, 1);
}

void draw() {
    cls(CLR_DARKER_PURPLE);
    print("chords and rhythm.", 4, 4, CLR_WHITE);
    print("degree(scale, octave, n) -> a chord root", 4, 16, CLR_LIGHT_GREY);
    print("chord()/strum() stack a triad on it", 4, 26, CLR_LIGHT_GREY);
    print("euclid(hits, steps, b) spreads a hi-hat", 4, 36, CLR_LIGHT_GREY);

    // the progression, current chord highlighted
    int chord_i = (beat() / 2) % 4;
    for (int i = 0; i < 4; i++) {
        int col = (i == chord_i) ? CLR_YELLOW : CLR_DARK_GREY;
        int x = 20 + i * 72;
        rectfill(x, 56, 60, 50, col);
        print(CHORD_NAME[i],  x + 24, 64, CLR_BLACK);
        print(CHORD_NOTES[i], x + 6,  86, CLR_BLACK);
    }

    // euclid step row — playhead + hit dots
    int step = beat() * 4 + (int)(beat_pos() * 4);
    print(str("hi-hat density: %d/%d  (arrows to change)", hits, STEPS), 4, 128, CLR_LIGHT_GREY);
    for (int i = 0; i < STEPS; i++) {
        int x = 20 + i * 18;
        bool on  = euclid(hits, STEPS, i);
        bool now = (i == step % STEPS);
        int col = now ? CLR_WHITE : (on ? CLR_GREEN : CLR_DARK_GREY);
        circfill(x, 150, now ? 7 : 5, col);
    }

    print(str("beat %d", beat()), 4, 176, CLR_LIGHT_GREY);
}
