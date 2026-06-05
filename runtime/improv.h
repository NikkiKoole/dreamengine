// improv.h — THE IMPROVISER (melody brain #2): phrase-based solo generation.
//
// Born in roadhouse.c (the Doors station, 2026-06-05); graduated to a shared
// header when the cocktail trio became its second customer — the same
// extract-on-second-user rule as radio.h. Future customers: ethio-jazz,
// J-fusion, anything that needs a band member who can actually take a solo.
//
// The model: a solo is PHRASES; a phrase is the MOTIF, developed. At solo
// start a 3-4 note motif is invented; every 2-bar phrase renders it through
// a development op (stated / answered-inverted / sequenced up / answered
// low), with density and register driven by a TENSION ARC over the solo's
// length (rise to a peak at 0.7, then release), double-time bursts at the
// peak, a long resolving tone + a breath at every phrase end.
//
// ⚠ EVERYTHING here runs on engine rnd() — PERFORMANCE, never composition.
// A pinned seed replays the song; the solos are new every time. That also
// means extraction/refactor here can never break a pinned seed.
//
// Usage (see roadhouse.c / cocktail.c):
//   static Improv solo;
//   improv_begin(&solo, 67, 16, 1.0f);            // register, bars, density
//   // per 2-bar boundary inside the solo:
//   if (improv_due(&solo, barInSolo)) improv_render(&solo, barInSolo, mode7);
//   // per step (cs = 0..31 within the 2-bar window):
//   for (int i = 0; i < solo.n; i++)
//       if (solo.onset[i] == cs)
//           play(improv_midi(&solo, i, keyPc, mode7, bluePct), dur=solo.dur[i]);

#ifndef IMPROV_H
#define IMPROV_H

#include "studio.h"

typedef struct {
    int  motif[4], motifN;     // the solo's idea: small degree steps
    int  phraseNo;             // which 2-bar phrase of this solo
    int  onset[16], deg[16], dur[16], n;   // the rendered phrase (32 steps)
    int  regBase;              // register floor (the arc climbs from here)
    int  lenBars;              // solo length in bars (the arc's denominator)
    float dens;                // density multiplier (1 = lead; ~0.6 = a bass solo)
    long renderedAt;           // the 2-bar window this buffer belongs to
} Improv;

// the tension arc 0..1: rise to the peak at 0.7 of the solo, then release
static float improv_arc(const Improv *im, long barInSolo) {
    float t = (float)barInSolo / (float)(im->lenBars > 0 ? im->lenBars : 16);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t < 0.7f ? t / 0.7f : 1.0f - (t - 0.7f) / 0.3f * 0.55f;
}

// a fresh idea — call at the first step of a solo (pure performance rnd)
static void improv_begin(Improv *im, int regBase, int lenBars, float dens) {
    im->motifN = 3 + rnd(2);
    for (int i = 0; i < im->motifN; i++) im->motif[i] = rnd(5) - 2;   // -2..+2 degrees
    if (im->motif[0] == 0) im->motif[0] = 1;        // the idea has to GO somewhere
    im->phraseNo   = 0;
    im->regBase    = regBase;
    im->lenBars    = lenBars;
    im->dens       = dens;
    im->renderedAt = -1;
}

static bool improv_due(const Improv *im, long barInSolo) {
    return im->renderedAt != barInSolo;
}

// render one 2-bar phrase into the buffer. mode7 = the scale as 7 semitone
// offsets (any mode/scale — mixolydian, dorian, a jazz major, tizita...)
static void improv_render(Improv *im, long barInSolo, const int *mode7) {
    (void)mode7;
    float arc = improv_arc(im, barInSolo);
    int   op  = im->phraseNo % 4;                   // statement/answer/sequence/answer-low
    int   want = (int)((3.0f + arc * 7.0f + (op == 2 ? 1 : 0)) * im->dens);
    if (op == 3) want = want * 2 / 3;               // the answer breathes
    if (want > 11) want = 11;
    if (want < 1)  want = 1;

    // onsets: off-beat-leaning candidates; the peak doubles time
    static const int OC[14] = { 0, 2, 3, 5, 6, 8, 10, 11, 13, 16, 18, 21, 24, 26 };
    int used[14] = { 0 };
    im->n = 0;
    for (int k = 0; k < want && im->n < 14; k++) {
        int tries = 6;
        while (tries--) {
            int c = rnd(14);
            if (!used[c]) { used[c] = 1; break; }
        }
    }
    for (int c = 0; c < 14; c++) if (used[c]) im->onset[im->n++] = OC[c];
    if (arc > 0.8f && im->n < 13)                   // the PEAK: double-time bursts
        for (int c = 0; c < 14 && im->n < 13; c++)
            if (used[c] && OC[c] + 1 < 28 && chance(40)) im->onset[im->n++] = OC[c] + 1;
    for (int i = 1; i < im->n; i++)                 // onsets in order for the gap math
        for (int j = i; j > 0 && im->onset[j] < im->onset[j - 1]; j--)
            { int t = im->onset[j]; im->onset[j] = im->onset[j - 1]; im->onset[j - 1] = t; }

    // pitches: walk the mode by the motif; the op transposes/inverts the idea
    int d = (op == 2 ? 2 : 0) + (op == 3 ? -2 : 0) + rnd(3) - 1;
    for (int i = 0; i < im->n; i++) {
        int step = im->motif[i % im->motifN];
        if (op == 1) step = -step;                  // the ANSWER inverts the question
        d += step;
        if (d < -3) d = -3;
        if (d > 10) d = 10;
        im->deg[i] = d;
        im->dur[i] = (i + 1 < im->n ? im->onset[i + 1] - im->onset[i] : 32 - im->onset[i]);
    }
    if (im->n > 0) {                                // resolve: end long, on home
        im->deg[im->n - 1] = (im->deg[im->n - 1] > 4 ? 7 : 0) + (chance(40) ? 4 : 0);
        if (im->dur[im->n - 1] < 6) im->dur[im->n - 1] = 6;
    }
    im->phraseNo++;
    im->renderedAt = barInSolo;
}

// degree index -> midi, walking mode7 up/down from a center
static int improv_deg_to_midi(int degIdx, int center, const int *mode7) {
    int oct = 0;
    while (degIdx < 0)  { degIdx += 7; oct--; }
    while (degIdx >= 7) { degIdx -= 7; oct++; }
    return center + mode7[degIdx] + oct * 12;
}

// note i of the current phrase -> midi: mode walk in a climbing register
// window, with the blue intrusion (b3/b7/b5 over anything, bluePct percent)
static int improv_midi(Improv *im, int i, long barInSolo, int keyPc,
                       const int *mode7, int bluePct) {
    float arc = improv_arc(im, barInSolo);
    int m = improv_deg_to_midi(im->deg[i], 60 + keyPc, mode7);
    m += ((im->regBase - 60) / 12) * 12 + ((int)(arc * 10.0f) / 7) * 12;
    while (m < im->regBase - 5)  m += 12;
    while (m > im->regBase + 17) m -= 12;
    if (chance(bluePct)) {                          // the blue bend
        static const int BLUE[3] = { 3, 10, 6 };
        m = 60 + keyPc + BLUE[rnd(3)];
        while (m < im->regBase - 5) m += 12;
    }
    return m;
}

#endif // IMPROV_H
