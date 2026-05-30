# 0001 — Cut the DIV-style process/coroutine model
Date: 2026-05-30 · Status: accepted

## Context
DIV Game Studio's signature feature is the **process**: each game object is a coroutine
with its own `loop … frame;` body. This was drafted in VISION as the "Level-2" learning
model and billed as the headline differentiator from PICO-8. Building it means C-side
coroutines (longjmp / ucontext / protothreads) plus a small parser/transformer for the
`process … loop … frame;` syntax — weeks of architectural work.

## Decision
Don't build it. Game objects stay plain C: a typed static array with an `on` flag and a
`for` loop that skips inactive slots.

## Why
The ~90 shipped carts are the counter-evidence — they all work cleanly with typed static
pools (`Enemy enemies[64]; bool on;`). The "objects talking to each other" need that the
process model would serve is covered by the proposed `broadcast`/`received` event bus,
with none of the coroutine machinery. It's weeks of work for a model the cart corpus
shows we don't need.

## Consequences
- VISION drops to two scaffolding levels (shielded loop, raw loop); the middle level is gone.
- Scratch's `wait N seconds` and `create clone` are skipped (they wanted coroutines).
- See [`../design/api-notes.md`](../design/api-notes.md) DIV section + 2026-05-30 review.
