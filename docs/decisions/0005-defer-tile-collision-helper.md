# 0005 — Defer the `move_and_collide` tile-collision helper
Date: 2026-05-30 · Status: accepted

## Context
A `move_and_collide(&x, &y, w, h, vx, vy)` that moves an entity and resolves it out of
solid map tiles came up while polishing platformers. The question: ship it as engine API?

## Decision
**Not yet.** Don't add a built-in tile push-out resolver.

## Why
A cart survey (2026-05-30) found thin demand:
- Only `platform.c` does the full pattern (`mget` → 4-corner solid test → axis-separated
  move-then-push-out → `on_ground`).
- `zelda.c` / `gta.c` do a related 4-corner test, but top-down and against their *own*
  world data, not `mget` — a single map-based helper wouldn't serve them cleanly.
- alleycat / pitfall / burgertime / sokoban don't use map-collision at all.

The reusable nugget is the 4-corner "is this box solid?" predicate, not the full
move-resolve — and even that varies by data source.

## Consequences
- Carts keep hand-rolling tile collision (the manual physics is part of the lesson).
- Revisit if more tile-based platformers appear. `touching_map` (nonzero = solid) already
  covers the simplest case.
