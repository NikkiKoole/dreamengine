# 0025 — The external-data hooks are committed API
Date: 2026-07-02 · Status: accepted

## Context
`de_data_path()` / `de_dropped_file()` / `de_open_path()` were introduced as an EXPERIMENT
([`external-data-carts.md`](../design/external-data-carts.md)): can a cart load a data blob at
**runtime** instead of baking it into its C source? The rule at the time: not committed API,
deliberately undocumented, delete cleanly if the idea doesn't earn its keep.

It earned its keep. The hooks now carry a load-bearing slice of the driving-world program:
`roadview` loads whole-city OSM road geometry from a `.rvb` at startup; `citydrive` renders the
same data in pseudo-3D; **`sloop` drives real Delft through them** — `--data`/`$DE_DATA` on
launch, drag-a-`.rvb`-onto-the-window to swap cities mid-session, `O` reveals the data folder
(P1 / Rung B of [`driving-world-program.md`](../design/driving-world-program.md)). The
2026-07-02 [API usage audit](../design/api-usage-audit.md) flagged the resulting inconsistency:
the only "four places" documentation gaps in the whole API were these three deliberately
undocumented hooks — "decide it, don't drift it."

## Decision
**Promote the three hooks to committed API** — the full four-places treatment: the
`studio.h` one-liners lose their "EXPERIMENTAL / may be removed" hedge, `studioDocs.js` gets
entries (autocomplete/hover/help), and `shell.js` gets an **external data** help-tab section.

The commitment is deliberately narrow — **the hooks traffic in paths, not formats**:
- `de_data_path()` → the `--data <file>` / `$DE_DATA` launch path, else NULL.
- `de_dropped_file()` → a file dropped on the window this frame, else NULL.
- `de_open_path(path)` → reveal a file/folder in the OS file manager.

What's *inside* the file stays cart-land policy (`.rvb`, JSON via `runtime/json.h`, anything) —
the engine never learns a data format, per [ADR-0006](0006-library-carts-not-engine.md)'s
capabilities-not-content line.

## Consequences
- The four-places cross-check (`node tools/api-usage.js`) reports zero gaps again.
- [`external-data-carts.md`](../design/external-data-carts.md) stays the design story
  (formats, pipeline, consumers) but no longer gates the hooks' existence — its STATUS
  records the graduation.
- Web/wasm builds: drag-drop and `--data` are native-shell affordances; a web cart simply
  sees NULL / no drops (same graceful degradation as before — nothing new to port).
