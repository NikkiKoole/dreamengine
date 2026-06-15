// gatetest — verification cart (throwaway): a single pluck through a big reverb, then a GATE placed
// AFTER the reverb in the chain (gated reverb). With the gate on, the lush tail should chop off as it
// decays below threshold → much less energy in the back half than ungated. Source-only.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_PLUCK, 2, 0, 3, 200);
    reverb_insert(0.92f, 0.3f, 0.8f);              // a big wet tail
    gate(0.5f, 2, 50);                              // gate AFTER reverb -> chop the tail
    int chain[] = { FX_REVERB, FX_GATE };
    fx_order(0, chain, 2);
    note_on(60, I_S, 6);
}
void update(void) {}
void draw(void) { cls(0); print("gatetest: pluck -> reverb -> GATE (gated verb)", 8, 8, 7); }
