# 0013 — Cut `music()`: the pattern-bank model lost to code-first music
Date: 2026-06-04 · Status: accepted

## Context
`music(n)` was the PICO-8-style half of the original bank plan: 16 patterns × 4
channels of sfx indices, played on voices 0–3. The cart-authoring side of that plan
never shipped — the only pattern that ever existed was the hardcoded demo
(`music 0` = bass+hihat, fed by demo sfx slots 4/5), and the `sound.h` comment said
it outright: *"demo data (replaces eventually with cart-authored data)"*.

The API usage audit ([`design/api-usage-audit.md`](../design/api-usage-audit.md),
2026-06-04: 182 functions × 233 carts) found **zero carts calling `music()`** — it
could only play the one demo loop. Meanwhile the project had already chosen its
music direction twice over:

- **Per-event sound is code**: `note` in 146 carts, `hit` in 114
  ([decision 0003](0003-code-first-sound.md), code-first sound).
- **Soundtracks are generative**: the beat clock (`bpm/beat/every/euclid` +
  `schedule/strum/chord`) and [`guides/game-music.md`](../guides/game-music.md);
  dozens of carts sequence real music this way.
- **Authoring tools are carts that export code**, not engine banks: the
  sfx editor / sfx generator / wave editor carts emit paste-ready C
  ([decision 0006](0006-library-carts-not-engine.md) shape; STATUS 2026-06-04:
  "SFX authoring direction: editor cart, zero new engine API").

## Decision
Remove `music()` and all its machinery: the `Pattern` type, `music_bank`,
`music_current`, the `kind==1` request handler, and the `from_music` voice flag
(voice stealing simplifies to *free → non-held → voice 0*). Request kind 1 is
retired, **not renumbered** — kinds 2+ keep their values.

**`sfx()` stays.** Its real-world role is narrower than its name: of 11 cart call
sites, 8 are `sfx(-1)` — the "silence all ringing sounds" verb (held `note_on`
voices survive). The 6 built-in demo slots remain as first-contact sounds
(`sfx(0)` makes a coin blip on day one) and as the `soundcheck` self-test's
fixture. Slots 4/5 (the former music channels) are relabeled loop-flag demos.

## Why
A playback API for banks that can't be authored is a trap for learners: `music(0)`
plays *something*, suggesting patterns are a real feature, then dead-ends. Every
path the project actually invested in routes around it. Cutting now costs zero
carts; keeping it costs explanation forever.

## Consequences
- Removed from all four wiring points: `runtime/studio.h`, `runtime/sound.h`,
  `editor/src/studioDocs.js`, `editor/src/shell.js`. `studio_tcc_symbols.h`
  regenerated (179 symbols). Sound tripwire run: PASS (no `[sound]` warnings).
- The `sfx()` doc now tells the truth about `-1` and points music-makers at the
  beat clock.
- If pattern playback is ever wanted again, the shape is a **library cart /
  export-as-code tool** (like the sfx editor), not engine banks — the bar set by
  0003/0006.
