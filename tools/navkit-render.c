// navkit-render — render a navkit soundsystem preset to a WAV, for A/B-ing against dreamengine.
//
// WHY: dreamengine's synth borrows navkit's instrument *designs* (the engine-port program,
// docs/design/instrument-engines.md). When ours sounds different from navkit's, rendering
// navkit's ACTUAL output is the ground-truth comparison — far better than reading its source
// and guessing. This is how the "funky clav" was cracked (2026-06-08): rendering navkit's
// "Clav Funky" proved its funk is a fast per-note FILTER-ENVELOPE quack baked into the patch,
// NOT the bus wah (a bus effect, off by default) and NOT an envelope follower. See
// docs/design/sound-handoff.md → "Comparing against navkit".
//
// REQUIRES the navkit sibling repo at ~/Projects/navkit/soundsystem (NOT part of dreamengine —
// this tool is a no-op without it). Header-only C, self-contained, no real-time audio device.
//
// BUILD + RUN:
//   NK=~/Projects/navkit/soundsystem
//   clang -I "$NK" tools/navkit-render.c -lm -o /tmp/nkrender
//   /tmp/nkrender 180 130.81 /tmp/navkit_clav.wav   # preset 180 (Clav Funky), C3, ~2s, wah bus OFF
// then: node tools/wav-envelope.js /tmp/navkit_clav.wav   (amplitude + brightness envelope)
//
// Preset indices live in ~/Projects/navkit/soundsystem/engines/instrument_presets.h
// (e.g. 174 Rhodes Warm, 178 Wurli Buzz, 180 Clav Funky). The wah bus stays OFF so you hear
// what the patch itself does; flip fx.wahEnabled below if you want to audition the bus wah.

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "soundsystem.h"
#include "engines/instrument_presets.h"

static void wav16(const char *p, float *d, int n, int sr) {
    FILE *f = fopen(p, "wb"); if (!f) { printf("can't write %s\n", p); return; }
    uint16_t fmt = 1, ch = 1, bps = 16, ba = 2; uint32_t srr = sr, brate = sr * 2, dl = n * 2, sz16 = 16; int br = 36 + n * 2;
    fwrite("RIFF", 1, 4, f); fwrite(&br, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sz16, 4, 1, f);
    fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&srr, 4, 1, f); fwrite(&brate, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dl, 4, 1, f);
    for (int i = 0; i < n; i++) { float x = d[i]; if (x > 1) x = 1; if (x < -1) x = -1; int16_t s = (int16_t)(x * 32767); fwrite(&s, 2, 1, f); }
    fclose(f);
}

int main(int argc, char **argv) {
    int   preset = argc > 1 ? atoi(argv[1]) : 180;          // default Clav Funky
    float freq   = argc > 2 ? (float)atof(argv[2]) : 130.81f; // default C3
    const char *out = argc > 3 ? argv[3] : "/tmp/navkit.wav";
    initInstrumentPresets();
    initSynthContext(synthCtx);
    initEffectsContext(fxCtx);
    // fx.wahEnabled = true;  // <- uncomment to also hear navkit's BUS wah on top of the patch
    int vi = playNoteWithPatch(freq, &instrumentPresets[preset].patch);
    if (vi < 0) { printf("playNoteWithPatch failed (preset %d)\n", preset); return 1; }
    int N = 44100 * 2; static float buf[44100 * 2]; float dt = 1.0f / 44100.0f;
    for (int i = 0; i < N; i++) { float s = processVoice(&synthCtx->voices[vi], 44100.0f); buf[i] = processEffects(s, dt); }
    wav16(out, buf, N, 44100);
    printf("wrote %s — navkit preset %d (%s), %.2f Hz, 2s\n", out, preset, instrumentPresets[preset].name, freq);
    return 0;
}
