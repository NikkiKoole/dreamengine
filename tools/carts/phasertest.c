// phasertest — verification cart (NOT a showcase): a steady 600 Hz sine through the master phaser,
// so we can render it to a WAV and A/B against navkit's processPhaser. A phaser is allpass
// (magnitude-preserving), so the swept notches only show in the dry+wet sum — hence mix 0.5; the
// sine then amplitude-modulates as the notch sweeps past 600 Hz, the fingerprint we measure.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    phaser(0.5f, 0.7f, 0.3f, 0.5f, 4);   // navkit defaults — the values we verify
    note_on(74, I_S, 5);                 // D5 ≈ 587 Hz (close to the harness's 600 Hz probe)
}

void update(void) {}

void draw(void) {
    cls(0);
    print("phasertest: sine -> phaser(0.5, 0.7, 0.3, 0.5, 4)", 8, 8, 7);
}
