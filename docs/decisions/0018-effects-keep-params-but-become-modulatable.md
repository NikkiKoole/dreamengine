# 0018 — Effects keep their own params (no instrument-style macros), but their params should be modulatable

Date: 2026-06-15 · Status: accepted

## Context

Instruments have the **three-macro core** ([0017](0017-three-macro-core-plus-engine-aux-channel.md)):
`harmonics`/`timbre`/`morph` — a fixed, normalized 0..1 knob set whose meaning each engine maps for
itself, and (the real power) which are **uniform modulation targets**: `LFO_TIMBRE`, `ENV_HARMONICS`,
`note_macro`, CV all work on *any* engine, and the macro is engine-smoothed so sweeping it is artifact-free.

After the boutique-effects arc (grains, univibe, dropout, shallow, amp-noise, gate, shimmer — all with
bespoke named params), the question came up: **should effects get the same 3-macro treatment?**

## Decision

**No instrument-style macro abstraction for effects. Keep each effect's bespoke, named params.** Three
reasons:

1. **Heterogeneous param counts.** Instruments genuinely converged on ~3 axes, so a fixed 3-macro shape
   *fit*. Effects don't: ring-mod has 2 params (freq/mix), phaser/leslie have 5. A forced 3-macro shape
   would either pad the small effects with invented knobs or hide controls on the big ones — i.e. **lose
   params we need or invent ones we don't.** That's the opposite of the macro discipline's goal.
2. **The "amount" macro already exists, and so does the preset layer.** Nearly every effect has a 0..1
   `mix` that *is* the universal more/less. And [`effects-recipes.md`](../guides/effects-recipes.md) is the
   effects' preset/"good starting values" layer — the recipe half of what instrument macros provide
   (consistent with [0015](0015-effects-are-recipes-not-primitives.md): effects are recipes, not a new
   primitive abstraction).
3. **SET-AND-HOLD.** The instrument macro's value is *smoothed modulation*. Effects are set-and-hold:
   reconfiguring buffer/coefficient DSP per frame (crush bit-depth, tape/chorus ring buffers) churns the
   audio thread → stutter. So a blanket "modulate any effect macro" layer can't exist safely — not all
   params are cheap to sweep.

## But the instinct is right — effects should be modulatable (the real want)

The macro idea points at a genuine gap: **you can't `LFO`/envelope/CV an effect param generically** the
way you can a voice macro. `modrack`'s "CV inlet into a module" is the hand-built version of exactly this,
and **we want effect params to be LFO-able in modrack.**

So the accepted *direction* (future work, not built here):
- Give the **cheaply-sweepable** effect params a normalized, **slewed modulation target** — filter cutoff,
  `mix`/sends, rate/depth LFOs — explicitly **not** the buffer-reconfiguring ones (bit-depth, ring-buffer
  lengths). The already-slewed `note_cutoff`/`note_reverb`/`note_vol` setters are the proven pattern.
- This is "a modulatable control + shared mappings," **not** "3 macros." Don't copy the macro *shape*;
  copy the *modulation plumbing*.
- Also DRY the per-cart `0..1 → param` mappings (`pedalboard`/`modrack`/`groovebox` each re-derive them) —
  document the canonical maps in `effects-recipes.md`.

## Consequences

- The effect roster stays as bespoke named functions + `mix` + recipes. No `fx_macro()` / 3-macro family.
- Future modrack work: expose the **sweep-safe** effect params as LFO/CV targets (per-param opt-in by
  "is this cheap to modulate"), so an effect can wobble under an LFO like an instrument macro can — without
  hitting the set-and-hold stutter.
- Cross-refs: [0015](0015-effects-are-recipes-not-primitives.md) (recipes not primitives),
  [0017](0017-three-macro-core-plus-engine-aux-channel.md) (the instrument macro core this contrasts with),
  and the effects modulation note in [`audio-notes.md`](../design/audio-notes.md) / the SET-AND-HOLD callout
  in [`effects-recipes.md`](../guides/effects-recipes.md).
