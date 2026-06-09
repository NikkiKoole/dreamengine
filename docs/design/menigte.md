# menigte — 10,000 people, one living town (scale design seed)

**Status: experiment — v1+v1.5+v2 shipped (scale, walk-sim, road-following), plus a
simple drivable car (v3 roughed in); integration with the *real* procplaces field
deferred.** Captures a design
conversation (2026-06-09) about the SCALE problem behind the long-term goal: *drive
a car through a large world full of NPCs that are meaningful and have a life.* The
runnable testbed is [`tools/carts/menigte.c`](../../tools/carts/menigte.c).

It sits in a trio of companion testbeds, each de-risking one axis of that goal in
isolation:
- [`procgen-places.md`](procgen-places.md) / `procplaces.c` — **the world** (roads & cities).
- [`sloop.md`](sloop.md) — **the car** (driving; its v1 explicitly *cut* NPCs).
- **menigte** (this doc) — **the population** (can a world hold 10k lives cheaply?).

Prior art for the "life" half already exists: `galerijflat.c` gives its residents
`wake_h / sleep_h / leave_h / return_h` and a walker state machine that physically
realises those trips — a life engine **bounded to one building**. menigte is the same
idea let loose in open space, and pushed to a population that makes it *matter*.

## The forcing function — 10,000

The target isn't decoration. 10k is chosen because it kills the lazy approach: you
**cannot** store-and-tick 10,000 full AI agents per frame. Picking a number that
breaks the naive design forces the right one. (10k is also roughly a small Dutch
town — enough that "the town is alive" reads as true, not as a handful of extras.)

## The reframe — "having a life" is not "running a simulation"

The whole architecture turns on one split:

| | what it is | cost | who pays it |
|---|---|---|---|
| **State** | where a person is + what they're doing | a few compares + a `lerp` | **all 10k, every frame** |
| **Elaboration** | the *pretty* realisation: walk anim, steering, road-following | real CPU | **only the on-screen few** |

State is a **pure function of `(identity, time-of-day)`** — `person_at(p, hour)`:
home until morning, lerp toward work, at work all day, lerp home, asleep. No
pathfinding, no per-step integration; just closed-form evaluation. 10k of those is
~200k ops/frame — nothing (the v1 cart runs ~150–180 fps headless-uncapped with all
10k live). So **everyone is always live and correct, on-screen or not.**

This inverts the usual open-world-AI headache. The classic model *hydrates* NPCs near
the player (spin up full state) and *dehydrates* distant ones (collapse to a summary),
and the hard part is reconstructing a believable position on the way back without
teleporting. Here there is **no state to dehydrate** — state was always cheap and
always live. The only thing that spins up and down is the *rendering elaboration*, and
because an off-screen person's position was already being computed, walking them on
screen reconstructs nothing.

### LOD bands (rendering only — state is uniform across all of them)
- **HOT** — closed-form position is on screen. Gets the walk-sim: steering toward the
  schedule target, walk bob, eventually road-following.
- **WARM/visible** — within the (zoomed) view rect. Drawn as a single coloured dot at
  the closed-form position.
- **COLD** — everything else. State still ticks (it's free); simply not drawn. Cull is
  a rect test, not a lifecycle event.

## The one invariant to hold (the procplaces parallel)

procgen-places.md has its load-bearing rule: *draw and `road_at()` must read the same
geometry from shared helpers*, or the car drives through a building that visually
isn't there. menigte has the exact same shape one level up:

> **The walk-sim (HOT) position must track the closed-form (`person_at`) position.**

If the pretty walk-sim is allowed to drift from the schedule truth, people *pop* the
instant they cross the screen edge — the dot was at the closed-form spot, the sim
starts somewhere else. So the HOT layer is a **smoothed realisation** of the
closed-form target (it eases toward it, adds gait), never a free agent. Same class of
rule, same reason: two code paths must agree on a position at a boundary.

## Colour = activity (reading the day at a glance)

The v1 demo encodes the activity band as colour so the town's daily rhythm is legible
without any UI: morning **ORANGE** rush flowing toward downtown → midday **BLUE**
settled at work → evening **GREEN** tide flowing back to the suburbs → night dim at
home. The draw-count alone tells the story — it climbs as the crowd converges on the
centred downtown and thins as they disperse.

## Scope / rungs

- **v1 (shipped):** 10k people, closed-form home↔work↔home schedule with per-person
  jitter, identity as a pure function of `(index, seed)` (re-rolls identically — which
  is *why* 10k is free), observer camera (pan/zoom), accelerable clock, day/night
  tint, visible-only draw. Proves flat frame cost across a full day.
- **v1.5 — elaboration layer (shipped):** the HOT walk-sim. Past `HOT_ZOOM`, the
  on-screen few become little walking figures (head/body/legs + gait bob) that *ease*
  toward the closed-form target. A small slot pool (`NHOT`) with a free-list, claimed
  on entry (seeded at the closed-form spot → no edge-pop) and reclaimed when a person
  leaves view or drops below `HOT_ZOOM`. HUD shows `vis` / `hot` counts.
- **v2 — onto roads (shipped, self-contained):** the straight-line commute lerp is
  replaced by `route_point()` — a closed-form **L-shaped path that hugs a street grid**
  (snap onto the row nearest origin, run along it, turn down the column nearest the
  destination). Still a piecewise lerp, so all 10k stay cheap; commuters visibly travel
  the drawn grid. The grid mirrors procplaces' lattice (pitch 100, class by line index
  `%3/9/27`) but is menigte's own copy.
- **v2.5 — the *real* integration (deferred):** lift procplaces' actual road field into
  a shared `roads.h` both carts `#include`, so menigte routes on the *same* warped,
  zoned world procplaces draws. Deferred deliberately: (1) procplaces is under active
  construction (the procgen-places.md work) — refactoring its `.c` now risks colliding;
  (2) procplaces' **domain warp** makes "snap to nearest road line" non-trivial (roads
  aren't axis-aligned once warped), so closed-form routing needs more than `snap_road`.
  When it lands, `route_point` + any collision query must call the *one* shared helper
  (the procplaces shared-geometry rule, inherited).
- **v3 — drive in (roughed in):** a simple top-down arcade car (`carX/carY/carH/carV`,
  throttle + speed-scaled steering + drag), camera follows it, the daily tide flows
  around you; `F` detaches a free observer camera. No road collision yet — you can
  drive anywhere; snapping the car to the grid (and giving NPCs right-of-way) is next.
  Verified via the debug harness (scripted `down W` / `down D` → `carV`/`carX`/`carY`
  move correctly).
- **v3.5 — richer lives (shipped):** the day is no longer home↔work. Each person has an
  **archetype** (worker / student / night-shift / homebody, picked by hash) that builds
  a keyframe **itinerary** — home, commute, work *or* school, optional lunch out, an
  optional shop errand on the way home, sleep. `person_at` walks the keyframes (dwell =
  same place back-to-back, travel = `route_point` between different ones), so it's still
  one closed-form piecewise lerp — all 10k stay cheap. Colour now reads the *activity*
  (orange→work/school, green→home, yellow→café/shop, blue=at-work, pink=at-school).
  World shrunk to 3200×2300 with a tighter downtown so it feels populated to drive
  through (~210–290 visible downtown all day; dips at lunch). Still to do: zone-aware
  placement (homes in residential, work in commercial) once on the real procplaces field.
- **v4 — into sloop proper:** swap the toy car for sloop's real vehicle model, and
  have sloop query this population so NPCs become the traffic and pedestrians it shares
  roads with.

## Decisions so far
- ✓ **State is closed-form and uniform for all 10k**; only rendering is LOD'd. No
  hydrate/dehydrate of state.
- ✓ **Identity = pure function of `(index, seed)`** — lossless re-roll, no storage of
  transient state. This is what makes 10k (or 100k) cheap.
- ✓ **HOT-tracks-closed-form** is the non-negotiable invariant (no edge-pop).
- ⏸ **Persistent player-caused memory** (an NPC remembers you, dies, moves) is
  deliberately *out* of the scale experiment — it trades away the stateless re-roll
  that makes scale free, and belongs to the separate "meaningful encounters" axis.
- ✓ **Road-following is closed-form** (`route_point` L-path on a grid), never per-NPC
  pathfinding — that would forfeit the all-10k-cheap property the whole thing rests on.
- ⏸ **The real procplaces field + sloop integration** deferred (see v2.5/v3); the
  shared-helper rule is the only thing that must be respected meanwhile.
