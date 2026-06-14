// univibetest — verification cart (NOT a showcase): a steady sine through univibe() at the same
// rate/depth as phasertest's phaser(), so we can render both and confirm the OPTICAL LFO produces a
// different (asymmetric) modulation than the sine phaser. Source-only, unregistered.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    univibe(0.5f, 0.7f, 0.5f);   // same rate/depth/mix as phasertest's phaser(0.5,0.7,_,0.5,4)
    note_on(74, I_S, 5);         // D5 ≈ 587 Hz probe — amplitude-modulates as the notch sweeps past
}

void update(void) {}

void draw(void) {
    cls(0);
    print("univibetest: sine -> univibe(0.5, 0.7, 0.5)", 8, 8, 7);
}
