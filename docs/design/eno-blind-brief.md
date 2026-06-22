# Eno "Music for Airports" — the blind band brief

> Phase-1 intent-first brief for a new radio station (`eno.c`), written from the
> music before reading any cousin cart (the firewall). Companion to the runbook
> [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md) and the
> candidate entry in [`future-stations.md`](future-stations.md) (the ★★★★★ pick:
> first radio `INSTR_PIANO` **and** sustained `INSTR_VOICE` choir + a prime-length
> loops form brain).

## The record, from the music

*Ambient 1: Music for Airports* (Eno, 1978) is four pieces, and they are NOT one
texture — they are four different **setups**:

- **1/1** — acoustic **piano**, struck slowly and sparsely, notes left to ring and
  decay, over a faint synthetic bed. Electric-piano tints.
- **2/1** — the famous one: **voices only**. Sopranos each holding one sustained
  vowel ("aah"), recorded on tape loops *of different lengths*, layered so they
  drift in and out of phase. No two cycles align; the chords happen **by accident
  of phase**. This is the headline mechanism.
- **1/2** — **voices + piano** in interplay.
- **2/2** — **synthesizer only**: a slow, warm drone whose timbre morphs over minutes.

The genius is the **incommensurate (coprime) loop lengths**: each voice holds one
note and repeats on its own period (Eno used tape loops of ~17–30s, all different),
so the combined harmony never literally repeats. Eno's brief: *"as ignorable as it
is interesting."*

## The brains

- **FORM = PRIME-LENGTH LOOPS (the new mechanism).** Beatless. Each voice owns a
  loop period in *seconds*, the periods mutually coprime. A voice (re)sounds its
  held note when its own phase wraps. No step clock, no bar grid — the cart runs
  its own per-voice float timers off `now()`. This is the form brain the docs want.
- **HARMONY = EMERGENT, not a progression.** There is no chord table. A fixed
  **pitch set** (a lush mode — Lydian / Ionian / Dorian / Aeolian, never dark) is
  rolled per song; each loop holds one pitch from it. The harmony is whatever loops
  happen to coincide. *(Ties to the "prefer emergent behavior" house preference: the
  chords are a side-effect of phase, not authored.)*
- **NO drums, NO functional bass.** The lowest piano notes / a low pad drone are the
  bottom. Kin to `ambient` and `satie` in beatlessness, unlike either in mechanism.

## The voices (palette shopped — Phase 2)

| imagined voice | engine | sustain | notes / gap |
|---|---|---|---|
| acoustic piano (1/1) | **`INSTR_PIANO`** struck | strikes & decays *naturally* | **first radio consumer** of this shelf. Its "can't sustain" trait is *correct* here — Eno's piano IS struck-and-rings. `piano/grand h0.08 t0.50 m0.62` (mellow felt). |
| sung-vowel choir (2/1) | **`INSTR_VOICE`** | hand-rolled long swell | **new recipe**: `A2500 D0 S7 R1200`, harmonics→vowel (U/O/A/E/I), `voice_nasal` for breath. Existing voice presets are fast-attack audition patches — choir swell is the gap this fills. |
| warm synth pad / drone (2/2) | `INSTR_SAW` (S7) + LP, or `INSTR_PD` morph | holds | `saw/drone` LP600 bed; `pd/sweep-pad` for the slow timbral morph of 2/2. |
| tape warmth / wobble | `tape(wow,flutter,sat)` + `instrument_tune` micro-detune | — | the medium as a voice: gentle ombak beating between paired loops. |

Bed effects: `reverb(0.94, 0.32)` (cathedral bloom) + per-note `note_reverb` sends
high; `tape(0.22, 0.11, 0.34)` warm drift; `note_lfo(h, …, LFO_VOLUME, ~0.3Hz, …)`
for the slow breathing swell on each held voice.

## NOT SAMEY — the variety design (the headline ask)

Ambient generative music's failure mode is *every seed sounds identical and the
piece feels static*. The fix is layered, and the biggest lever is built into the
record itself:

1. **The SETUP roll (per seed) — the big lever.** Roll which of the four MfA
   textures you get:
   - **PIANO** (1/1): struck piano + faint pad, no voices.
   - **VOICES** (2/1): pure vowel choir, coprime drift, no piano — the purest phase piece.
   - **DUET** (1/2): voices + piano interplay.
   - **SYNTH** (2/2): morphing pad drone only, no piano/voice.
   One roll → four genuinely different instrumentations. This is what makes each
   "song" sound like a *different record*, not the same band re-keyed.
2. **Key + mode** (per seed) — Lydian/Ionian/Dorian/Aeolian × tonal centre → harmonic colour.
3. **Pitch set + register spread** — which 5–7 notes, and whether it's a high
   shimmer song or a low warm song.
4. **A fresh coprime loop-length set** per voice each seed → different phase relationships.
5. **Timbre macros** per engine — piano hammer/voicing, vowel + breathiness, pad
   waveform/filter, tape-wow depth (the per-song timbre roll, gamelan-style).
6. **Drift speed + density** — global loop-length scale (glacial ↔ flowing) and how
   many loops are live.

**Within a song:** slow whole-piece amplitude envelopes so voices *enter and recede*
over minutes (a voice arrives at ~2 min, fades at ~6); the phase drift itself; and in
SYNTH mode a slow filter/morph evolution.

## Player knobs (radio.h chrome)

- **DRIFT** — loop-length scale (glacial ↔ flowing).
- **TONE** — brightness (piano hammer, pad cutoff, vowel openness).
- **DENSITY** — how many loops live / how full.
- **SPACE** — re-roll seed (new setup + key). Optional explicit **MOVEMENT** selector.

## Window / face — the brain made visible

Propose: a calm horizontal-band field, **one band per live loop**, each with a marker
creeping across at its own period. Where markers align, a soft bloom — *you watch the
emergent chord happen*. (Same lineage as tango's bellows / carlos's counterpoint
lattice: visualize the generative mechanism.) Palette: pale dawn gradient, Eno's
aerial-map cover as the mood.

## What this station adds (why it earns the slot)

- **First radio `INSTR_PIANO`** (the last untapped original shelf with no station).
- **First sustained sung-vowel `INSTR_VOICE` choir** (air used the vocoder as a *lead*; this is held vowel pads — a new recipe).
- **The prime-length-loops form brain** — beatless coprime-period scheduling; reusable by a future Reich phasing station.
- **Emergent harmony** — first station with no chord brain at all; harmony is a phase side-effect.
