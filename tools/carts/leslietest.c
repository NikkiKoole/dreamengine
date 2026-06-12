// leslietest — verification cart (NOT a showcase): a steady 220 Hz sine through the master Leslie,
// so we can render it to a WAV and A/B against navkit's processLeslie. The rotors spin up from rest
// (deterministic), so a sample-level correlate vs the navkit harness confirms the verbatim port.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    leslie(LESLIE_FAST, 0.0f, 0.5f, 0.7f, 1.0f);   // fast/tremolo, no drive, equal balance, navkit defaults
    note_on(57, I_S, 5);                            // A3 = 220 Hz (rich enough across the 800Hz crossover)
}

void update(void) {}

void draw(void) {
    cls(0);
    print("leslietest: sine -> leslie(FAST, 0, 0.5, 0.7, 1)", 8, 8, 7);
}
