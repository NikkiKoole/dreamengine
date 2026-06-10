# 0017 — Keep the 3-macro core; a blessed per-engine aux channel for the exceptions (not a 4th macro)
Date: 2026-06-10 · Status: accepted

## Context
Every modeled engine exposes the same three live, normalized macros — `harmonics` / `timbre` /
`morph` — and nothing else ([instrument-engines.md §8.1.1](../design/instrument-engines.md), the
Plaits/MicroFreak discipline). The surface is `O(1)`: it never grows with the engine count.

But real engines keep wanting *something past three*, and each has improvised its own hatch:
- **GUITAR / PIANO** want fundamental-reinforcement weight + an attack click → `eng_tune(slot, idx, value)`, read **at note-on**.
- **BOWED** wants a pizzicato/arco **excitation** switch → also `eng_tune` (`eng_tune(slot,0,1)`), note-on.
- **VOICE** (`INSTR_VOICE`) wants **four-plus continuous timbre axes** (the `vox4` by-ear audition landed on vowel / size / effort / nasal) and a pile of probe params → it invented `voice_param(handle, idx, value)`, read **live on a held note**, plus `voice_consonant`/`voice_coda` for timed articulation.
- **BRASS** wants a mute / plunger wah.

So the recurring question — "should we relax 3 → 4, or add a generic engine hook?" — came to a
head with brass (the mute) and voice (genuinely four axes). The forcing observation: **we already
have two un-blessed aux channels** — `eng_tune` (note-on) and `voice_param` (live) — that are the
*same concept at two timing points*, mirroring how each macro has both `instrument_X(slot,…)`
(note-on) and `note_X(handle,…)` (live).

## Decision
**Keep the universal core at exactly three live macros. Do NOT add a universal 4th.** Route every
"past three" need into one of four lanes, by *what the thing actually is*:

| lane | what it is | timing | examples | mechanism |
|---|---|---|---|---|
| **3 macros** | universal **intrinsic timbre** | live | bore · brassiness · breath; vowel/size/effort | `instrument_X` / `note_X` (the fixed six) |
| **per-voice output controls** | shape the **output**, every engine | live | mute/wah, vibrato, slide, pan | `note_cutoff`/`note_res`/`note_pitch`/`note_glide`/`note_lfo`/`note_pan`/`note_drive`/`note_echo` |
| **per-engine aux params** | extras **past three**, indexed, per-engine | note-on **and** live | pizz (note-on) · voice's nasal/effort + raw probe params (live) | the blessed union of `eng_tune` (note-on) + `voice_param` (live) |
| **articulation events** | a **timed gesture** injected into a held note — not a knob | event | "bah", "ahh-m" | `voice_consonant` / `voice_coda` |

And **promote the per-engine aux channel from "EXPERIMENTAL, bake-to-constants later" to a
permanent, documented mechanism** — recognizing `eng_tune` and `voice_param` as its note-on and
live faces. It is explicitly *not* a continuous performance macro for everyone; it is an indexed,
per-engine channel for engines that genuinely exceed the three-macro timbre core.

**The rule of thumb** (the dividing line, so the next engine doesn't relitigate):
- Does it **shape the output**? → a universal per-voice control. *(mute = a swept filter on the
  output; it never touches the engine. Vibrato = `note_lfo`. The slide = `note_pitch`.)*
- Is it **continuous intrinsic timbre**? → one of the three macros. If an engine seems to need a
  *fourth* continuous timbre axis, first suspect it's really **two engines** (HARP folded into
  GUITAR; Rhodes/Wurli/Clav into EPIANO) — and only if it truly isn't, use the live aux channel.
- Does it **change how the sound is generated** (excitation / mode / structure)? → per-engine aux
  (`eng_tune`, note-on). *(You can't filter a bow into a pluck.)*
- Is it a **timed gesture** mid-note? → an articulation event, its own call.

There is a built-in tell for the lane: macros + per-voice controls are read **live**; aux *mode*
config is read **at note-on**. So anything that must move *while a note rings* (the plunger wah!)
literally cannot be the note-on hatch — which is why the mute belongs on `note_cutoff`, and why
voice needed the *live* `voice_param`, not `eng_tune`.

## Why
- **A universal 4th macro is paid everywhere, forever, to serve ~3 engines.** Every engine would
  have to define its 4th (PLUCK/SINE/etc. have no meaningful one → dead knobs), every preset grows
  a value, every showcase cart a slider, plus the four-places wiring and new `LFO_`/`ENV_`
  destinations. The 3-macro rule's whole value is that it's small and universal.
- **The evidence says the strain was never a missing 4th *knob*.** Where engines actually pushed
  past three: bowed wanted a *mode* (pizz), guitar/piano wanted *refinements*, brass wants
  *output filtering*. Only VOICE wants genuine extra continuous axes — and voice is exactly the
  engine that should use the live aux channel, not force a 4th macro on the other ten.
- **Engine ids and aux indices are cheap; clean macro semantics are expensive** (the same logic as
  [0016](0016-combo-organ-recipe-then-macro-or-engine.md): a meaning that shifts across the dial is
  the §8.1.1 sin). A per-engine indexed channel keeps each engine's three macros meaning exactly
  one thing.
- **Mutable themselves weren't dogmatic.** Plaits is 3 macros **+ a model selector**; **Rings is
  four** (Structure/Brightness/Damping/Position). "Three" was always "three *plus* model/config,"
  not a hard ceiling. `eng_tune`/`voice_param` is our version of that escape valve — we just hadn't
  named it as one mechanism.

## Consequences
- **VOICE has a sanctioned home.** `voice_param` stops being "EXPERIMENTAL, will-be-removed"; when
  the voice workstream locks its public axes (the `vox4`/`voxab`/`voxpad` probes), the eventual 3
  live macros land on the standard surface and the remaining axes (nasal/effort beyond the three,
  the probe levers) live on the live aux channel. **This ADR fixes the *mechanism*, not voice's
  specific axis mapping — that stays the voice workstream's call.**
- **BRASS mute is settled: not an engine concern at all** — it's `note_cutoff`+`note_res` in the
  cart (a swept resonant filter = the plunger wah), the same lane as the vibrato/slide controls
  already in `brass.c`. No 4th macro, no engine hook.
- **`eng_tune` doc status upgrades** from throwaway scaffolding to a documented per-engine channel.
  Open implementation question (gated — `sound.h` is mid-refactor and the unification touches it):
  whether `eng_tune` (note-on) and `voice_param` (live) literally merge into one named pair, or
  stay two entry points sharing the contract. The *principle* (this ADR) is decided regardless.
- **§8.1.1 gets a pointer here** so a future "should we add a 4th macro?" reads the considered
  answer (and the four-lane table) instead of re-deriving it.
- Does not supersede §8.1.1; it **extends** it — the three-macro core is unchanged; this records
  where everything *else* goes.
