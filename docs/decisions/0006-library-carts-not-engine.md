# 0006 — Pathfinding/particles/flocking ship as library carts, not engine API
Date: 2026-05-30 · Status: accepted

## Context
Recurring asks for "big" capabilities — A* pathfinding, particle systems, flocking,
state machines — that other engines provide as built-ins. Where should they live?

## Decision
Ship them as **library/example carts**, not as engine surface in `studio.h`. Seeds
already exist: `astar.c` (102 lines), `boids.c` (85), `sims.c` (232, needs-driven Sims
that pathfinds around player-built walls).

## Why
In a learn-C console the algorithm *is* the content — burying A* or a particle loop in a
kernel deletes the lesson. These carts are teaching artifacts. Engine API should stay the
small, sharp, immediate-mode core; richness comes from readable cart code you can open.

## Consequences
- Particle systems and pathfinding stay out of `studio.h` (cf. STATUS "Decided-against").
- A future "library cart" collection can grow from the seeds above.
- Small, genuinely-cross-cutting helpers (`explode()`, `hud()`) are a *different* call —
  those are convenience primitives, not algorithms; they're tracked as open in STATUS.
