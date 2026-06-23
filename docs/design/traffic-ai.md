# traffic-ai — NPC driving behaviours, prototyped as a TRAFFIC mode in trackgen

STATUS: BUILDING (2026-06-23). **Shipped:** the RACE/TRAFFIC toggle + lane-keeping, car-following
(IDM-style time-gap), OVERTAKING (blocked car pulls into a clear lane; a follower brakes to a stop,
never reverses), a crowd of TRAFFIC_CARS at density, speed-tinted cars (red=stopped→green=flowing),
a cycling TRAFFIC LIGHT (red = a stop-line all lanes queue behind), and a reaction lag (REACT_N)
that makes dense following unstable → PHANTOM JAMS emerge with no cause (the ring-road experiment).
Lanes are emergent from lateral position, so the player participates (stop in a lane → traffic
passes you). Collision is an ORIENTED box (long-along-heading, narrow-across), so adjacent-lane cars
never falsely touch. Spec (24 assertions) covers closing→brake, clear→accelerate, blocked→change-lane,
flow, no box-overlap, red-builds-a-queue / no-bolt / no-reverse, and stop-and-go spread. **Rough
edge:** on the tightest procedural corner a fast car can still clip the apex (localized, recovers).
**Next:** the cross-road intersection (#4), then speed zones / hazards / the more-ideas list below.

The racing rivals in `trackgen.c` proved the core:
one shared physics step (`step_car`) + a parameter-driven follow-controller (`drive_ai`) +
mass-weighted collisions. **Racing is one ruleset on that brain; traffic is another.** This
doc plans the **TRAFFIC mode** — same generated track, same cars, *not a race* — and the
behaviours worth testing there. It is the buildable home for two open backlog items:
sloop's **rung 4 "NPC-driven traffic"** and streetlab's `＋` gaps **traffic-signal *state*** and
**pedestrian-crossing *logic*** (see [`big-game-backlog.md`](big-game-backlog.md)).

## Decision: a MODE in trackgen, not a new cart

Racing and traffic share ~90% of the cart — the generated track (`cl[]`/`nl[]`), the `Car`
struct, `step_car` physics, the `drive_ai` follow-controller, and the collision resolver. A
separate cart would duplicate all of that (which `cart-dupes.js` exists to catch) or force a
header extraction now. So: add a **RACE / TRAFFIC** toggle to the setup panel; mode 1 stays the
race, a new mode runs free-flowing traffic. This *is* the cart's thesis — "same brain, different
soul" — lifted from the personality level to the mode level. `trafficjam.c` is a gag cart and
does **not** overlap.

**Escape hatch (the split trigger):** if TRAFFIC mode grows large (full IDM + lanes +
intersections + density UI), that is the signal to extract the shared core (`Car` + `step_car` +
`drive_ai`) into a header (`steer.h`-style) that *both* a race cart and a traffic cart include —
a clean split with zero duplication. Flag it when we cross that line; do not pre-build it.

## What makes trackgen a good traffic lab

- **The closed loop is the ring-road experiment.** Enough cars on a loop with reaction delay
  produce a stop-and-go wave from *nothing* — the canonical phantom-jam testbed.
- **The Figure-8 style crosses itself** — a free intersection for right-of-way prototyping.
- **Everything is deterministic + headless-specable** — flow, jams, and "no collision at the
  crossing" are all measurable in `spec()` (the cart already runs 24 assertions).

## The systems to test (rough dependency / leverage order)

| # | System | What it tests | Ports to | Spec angle |
|---|---|---|---|---|
| 1 | **Lane-keeping + car-following (IDM)** ✅ | hold a lane; adjust *throttle* to keep a safe time-gap to the leader **in your lane** (not just braking for corners) | every road in sloop | platoons form; gap never drops below a min |
| 2 | **Phantom traffic jam** ✅ | a stop-and-go wave emerges from density + reaction lag, no obstacle | highway flow realism | speed-variance wave travels *backward* round the ring |
| — | **Traffic light** ✅ (bonus) | a cycling red/green signal; a red is a stop-line all lanes queue behind — a controllable bottleneck | streetlab signal-state gap | a red builds a queue of stopped cars |
| 3 | **Overtaking (gap acceptance)** ✅ | change lanes to pass a slower leader *only* when the adjacent lane's gap is clear ahead+behind | multi-lane roads | no lane-change into an occupied gap |
| 4 | **Intersection / cross-road right-of-way** | a SECOND road (its own traffic stream) crosses the loop; cars on both must yield so they don't T-bone at the crossing. The figure-8 self-crossing is the cheap prototype; a dedicated cross-road through the world is the real thing (player's ask, 2026-06-23). Rules to try: priority road, give-way, 4-way-stop, or the cross-road's own light. | streetlab junctions; the world composer's road graph | zero collisions at the crossing over a long run |
| 5 | **Speed zones** | obey a per-section speed cap (corner / school zone) | road speed limits | cars under cap inside the zone |
| 6 | **Static hazard + merge** | route around a broken-down car and merge back | obstacles, parking, breakdowns | all cars clear the hazard, no pileup |
| 7 | **Pedestrians at crossings** | peds wait, then cross at a crosswalk; cars yield/stop for them | streetlab's open ped-logic `＋` gap | a ped crossing halts the cars; nobody is hit |
| 8 | **Two-way traffic** | an oncoming lane — some cars drive the loop the other way; head-on lane discipline | two-way roads | oncoming cars never stray into the wrong lane / collide head-on |
| 9 | **Coordinated lights ("green wave")** | multiple signals phased so a platoon catches greens in sequence | streetlab signal-state, timing | a car at the speed limit hits consecutive greens |
| 10 | **Emergency vehicle** | a siren vehicle others yield to (pull aside / stop) | game events, NPC reactions | regular cars clear a lane for it |
| — | **Light-bar polish** (small) | the signal reads subtly from far away; make it pop more / optional camera nudge to it | — | n/a (visual) |

**Foundation = #1.** Today `drive_ai` only brakes for *corners* and "lanes" are a static lateral
bias (`line_off`); real **longitudinal car-following** (the Intelligent Driver Model — match the
leader's speed, keep a time-gap) is the one primitive that unlocks #2 (jams), #3 (overtaking),
and realistic platooning. It is also the most portable piece to sloop and matches the
`car-following` teaches-tag the cart already claims.

## Sketch: the cross-road intersection (#4, the next build)

The vision (2026-06-23): instead of relying on the figure-8 self-crossing, lay **one large road
straight through the world** that crosses the loop, with **its own traffic** — and make cars on
both roads negotiate the crossing so they don't collide. The figure-8 self-crossing is the cheap
warm-up of the same problem (same path, two passes); a dedicated cross-road is two *independent*
streams meeting.

Likely shape in trackgen's model:
- A second path `cl2[]` (a straight or gentle spline across the map) intersecting the loop at one
  (or two) **crossing point(s)** — found by segment-intersection of the two centre lines.
- A pool of cars assigned to the cross-road, run by the **same `drive_ai_traffic`** (the brain
  doesn't care which line it follows — the whole portability thesis).
- **Conflict negotiation** at each crossing: a car approaching computes its time-to-crossing; if a
  car on the other road will be inside the intersection box within that window, it **yields**
  (treat the crossing as a temporary stop-line leader, reusing the red-light gap trick). Rule
  variants to try: a **priority road** (loop has right-of-way), **give-way**, **4-way stop**
  (first-come), or **a light** governing the cross-road.
- Spec: run long, assert **zero box-overlaps at the crossing** (the right-of-way held) while both
  roads keep flowing (not gridlocked).

This is the direct prototype of streetlab's junction priority and the world composer's road graph —
the first time two roads, not one loop, share the sim.

## How TRAFFIC mode differs from RACE mode

- No laps / standings / finish; cars cruise at their own desired speed indefinitely.
- Mixed desired speeds per car (the existing `spd_cap` already varies by personality — reuse it).
- Lanes instead of one racing line; cars hold a lane and only leave it to overtake (#3).
- Player is just *in* the traffic (free-drive, no win condition) — or a "spectate/observe"
  camera for watching jams form. Density (car count) becomes a setup lever.
- Optional two-way: some cars driving the loop in the opposite sense in an oncoming lane.

## Related

- [`big-game-backlog.md`](big-game-backlog.md) — sloop rung 4 (NPC traffic) + streetlab's
  signal-state / ped-logic `＋` gaps that this feeds.
- [`showreel-teaser.md`](showreel-teaser.md) — the larger lo-fi-GTA the traffic AI serves.
- `trackgen.c` header — the source-of-truth contract for `Car` / `step_car` / `drive_ai` /
  `resolve_collisions` (the shared brain this mode reuses).
- [`flank-tactical-ai.md`](flank-tactical-ai.md) — the sibling agent-AI subsystem (combat brains).
