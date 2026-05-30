# 0003 — Code-first sound; tracker UI deferred
Date: 2026-05-30 · Status: accepted

## Context
The original vision called for a 4-channel tracker/sequencer UI to match the sprite
editor. Sound needed *some* authoring path.

## Decision
Build a **code-first sound API** instead — an 8-voice synth driven entirely from cart
code (`note`/`hit`/`chord`/`strum`/`tone`, `bpm`/`beat`, `every`/`euclid`/`chance`/
`degree`, `schedule`). A visual tracker UI is **deferred, not rejected**.

## Why
Making music *is* programming — which fits the learn-to-code mission better than a
separate visual tool. Strudel-inspired primitives get a lot of musical range from a few
function calls. The code path was also far cheaper to ship than a tracker.

## Consequences
- The sound tab is disabled; SFX/music banks are hardcoded demo data (no cart-side
  bank authoring yet).
- Whether a tracker ever lands is a genuine open question — its prerequisites are the
  instrument bank + cart SFX authoring. Tracked in STATUS / [`../design/audio-notes.md`](../design/audio-notes.md).
