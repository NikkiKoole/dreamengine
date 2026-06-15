// tremtest — a verification cart (NOT a showcase): pass a steady sine through the master tremolo
// so we can render it to a WAV and A/B against navkit's processTremolo formula. Plays one long
// A4 sine on INSTR_SINE (no decay tail to muddy the envelope) with tremolo(5.5, 0.7, LFO_SHAPE_SINE).
#include "studio.h"

#define I_S 0

void init(void) {
    // full-sustain sine: attack 2ms, no decay, sustain 1.0, so the raw voice is a flat-top sine —
    // any amplitude wobble in the render is the tremolo, nothing else.
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    tremolo(5.5f, 0.7f, LFO_SHAPE_SINE);   // 5.5 Hz, 70% depth, sine — the value we verify
    note_on(69, I_S, 5);              // A4 = 440 Hz
}

void update(void) {}

void draw(void) {
    cls(0);
    print("tremtest: sine -> tremolo(5.5, 0.7, SINE)", 8, 8, 7);
}
