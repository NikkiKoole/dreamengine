#include "studio.h"
// verification cart for the LFO shapes (not registered). A held saw with sample-&-hold on PITCH
// (the random-step arp) — proves the stateful path runs AND is deterministic (--det byte-repro).
// Build with -DBASE to get the SINE control (same note, default shape).
#define SL 8
void init(void) {
    instrument(SL, INSTR_SAW, 200, 0, 8, 2000);
    int h = note_on(45, SL, 6);
    instrument_lfo(SL, 0, LFO_PITCH, 8.0f, 7.0f);   // 8 Hz, ±7 semitones
#ifndef BASE
    note_lfo_shape(h, 0, LFO_SHAPE_SH);             // sample & hold → stepped random pitch
#endif
}
void update(void) {}
void draw(void) { cls(0); }
