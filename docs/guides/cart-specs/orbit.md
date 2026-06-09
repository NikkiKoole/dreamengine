# The game you're building: ORBIT (a gravity sandbox)

> Unlike most cart-specs (which are *forward* build plans), orbit is **shipped and
> playable** — `tools/carts/orbit.c`. This doc is a living spec: what's true today,
> what's reachable today, and the roadmap. The deep design rationale (the "why")
> lives in the cart header itself; this is the map above it. A flagship "legendary
> series" cart ([`VISION.md`](../../VISION.md),
> [`second-north-star.md`](../../design/second-north-star.md)) — it should have one.

## Premise
A **Kerbal-Space-Program-in-miniature**. Pick a rocket, launch from a planet, point it,
burn, and let orbital mechanics judge your design. Like KSP, the 95% where the rocket
betrays you *is* the loop — the rare success is the payoff. The chosen transcendent
moment: the first time the predicted path curves all the way around and **closes into a
loop clear of the ground** — "I'm in orbit."

## The one honest core (never fake it)
Point-mass inverse-square gravity, symplectic Euler. The **same integrator** (`grav_acc`
+ the step in `predict()`) runs the live ship *and* the dotted predicted path — so the
dots never lie: they show exactly where you'd coast if you cut the engine now. This
shared-core honesty is the whole design. Never give the prediction its own cheaper math,
or it starts lying. (Same DNA as [`coaster`](../../design/sloop.md) and `sloop`.)

Load-bearing tuning, learned the hard way (don't "fix" these):
- **Big, heavy planet** (`R_PLANET 120`, `G_SURF 8`). High escape velocity = room for
  bound orbits; low surface gravity = long, hand-flyable burns (`burn_time ∝ √(R/g)/TWR`).
  If orbits balloon or burns get twitchy, the lever is *world scale*, not the rocket.
- **Delta-v knife-edge.** With 1-button thrust (no throttle), the gap between "falls
  short" and "escapes" is narrow, so FALCON sits right around escape velocity. A clean
  orbit wants a deliberate **two-burn** (gravity-turn up, coast to apoapsis, small
  prograde nudge to lift periapsis off the surface).

## The achievement ladder (KSP's order of escalation)
Status: 🟢 reachable now · 🟡 wired but gated · 🔴 needs a new mechanic

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 1 | **ORBIT ACHIEVED** | 🟢 | Predicted path closes into a loop clear of the ground. The core beat. |
| 2 | **WELCOME HOME** (orbit → land back safely) | 🔴 | `try_contact()` *rewards* a soft planet landing after `orbit_done` — but see "ceiling" below: practically unreachable today. |
| 3 | **MUN ENCOUNTER** (reach a 2nd gravity well) | 🟢 | `grav_acc()` sums planet + a static Mun, so the same `predict()` bends the path around the Mun once your apoapsis reaches it. Banner + chime fire. |
| 4 | **MUN ORBIT** (capture into a loop *around* the Mun) | 🟡→🔴 | Detector **scaffolded** (a second swept-angle accumulator in `predict()`, `predMunOrbit` + a `MUN ORBIT` banner/chime) but it **cannot fire yet**: at the Mun's current distance no stable capture orbit physically exists, and FALCON arrives out of fuel. Blocked on physics + a throttle, not on detection. See "Mun orbit is blocked on physics" below. |
| 5 | **MUN LANDING** | 🟡 | Reward text already exists in `try_contact()`; unreachable until capture (4) exists. |
| 6 | **MUN LANDING + RETURN** | 🔴 | The full round trip. The hard, satisfying one. |
| 7 | Interplanetary (Minmus, Duna…) | 🔴 | Far future. |

## The current ceiling (don't grind — it's mechanics, not skill)
**Milestone 3 (encounter) is the real ceiling of the current build.** Both landings are
gated *by design*, not by piloting:

- **Mun landing** — you arrive at the Mun on a flyby/impact trajectory, fast. A soft
  touchdown needs `spd < LAND_SPEED (26)` and nose-up. There is **no way to brake into
  Mun orbit yet**, so you hit the Mun at encounter speed. The reward exists; the path to
  trigger it doesn't.
- **Landing back home** — the soft-landing gate is `spd < 26`, but orbital/impact speeds
  here are ~31–44, so you'd have to *suicide-burn* at the surface — and FALCON usually
  reaches orbit with little fuel left (the knife-edge). With no throttle and no
  atmosphere, threading it by hand is a coin flip.

The two levers that turn "impossible" into "hard-but-fair" are exactly the next-phase
items: **Mun-orbit capture** and **a throttle** (the header calls throttle "the real cure
for fiddliness").

## Mun orbit is blocked on physics, not detection (finding, 2026-06-09)
The **detector half is built** — `predict()` now carries a second swept-angle accumulator
around the Mun's centre (the Mun's version of the planet 358° check), gated so it only
counts a loop that stays bound to the Mun, plus `predMunOrbit`/`predMunPeri`, a `MUN
ORBIT` banner, a chime, and `munorbit`/`munperi` trace fields. Building it was the easy
part. Trying to *test* it surfaced the real blocker, in two layers:

1. **No fuel.** Flying FALCON to the Mun (gravity-turn east, coast to apoapsis) arrives on
   a fast flyby with **`twr` already 0** — both stages spent ~4 s in. There is nothing left
   for a capture burn. Confirms the "current ceiling" section: a throttle (or a higher-Δv
   design) is a hard prerequisite.
2. **No stable orbit to capture *into*.** The deeper problem. The Mun sits only ~312 px
   from the planet; the planet's tide matches the Mun's pull at ~95 px from the Mun while
   the Mun *surface* is at 42 px — a razor-thin band. In practice it's worse than thin:
   seeding a textbook circular velocity at 46–80 px from the Mun (even after relocating the
   Mun to ~600 px, where it dominates 6:1) produces a path that **crashes into the Mun or
   escapes back toward the planet within a single revolution** — never a closed loop. So
   the detector, correctly, never fires: there is no captured trajectory in the current
   world for it to recognise. (It *does* stay silent on a flyby — no false positive — which
   was the one thing testable today.)

**What milestone 4 actually needs, in order:**
1. **A capture regime that exists.** Move the Mun farther out and/or re-tune `MU_MUN` /
   `MUN_SOI` until a hand-reachable, *stable* orbital band exists around it — verify with a
   seeded circular orbit that the predicted path closes a full loop before the build of any
   piloting. Also suspect the **prediction timestep** (`PDT 0.045`): over `PRED_MAX` steps a
   coarse symplectic-Euler step can inject enough energy to unbind a tight orbit, so a finer
   `PDT` (or adaptive step near the Mun) may be part of the fix. Until a seeded orbit fires
   `predMunOrbit`, the detector's 2×SOI reset bound and 358° threshold are **unvalidated
   guesses** (so marked in the code).
2. **Throttle** — the capture burn needs partial thrust to be flyable (see below).
3. Only then is the detector worth trusting, and milestone 5 (Mun landing) falls out of it.

## Roadmap (in build order)
1. **Mun-orbit capture (do this next).** Detector is scaffolded; the work left is the
   *physics* above — a Mun placement/tuning that admits a stable capture orbit (validated by
   a seeded test), likely a finer prediction step, then the throttle for the braking burn.
   **Unlocks milestone 5 (Mun landing) for almost free** once it works — that's why it's first.
2. **Throttle.** Partial thrust (not just on/off) makes precise landing burns flyable —
   the cure for the delta-v knife-edge. Unlocks soft landings (2 and 5) as *reliable*.
3. **Mouse interaction — maneuver nodes.** The real KSP map-view verb: click your orbit
   to drop a node, drag prograde/retrograde, watch the predicted path change *before* you
   burn. **Gated on capture existing** — a node is a tool for planning the braking burn,
   so it's pointless before there's a burn worth planning. Mouse-wheel zoom (engine has
   `mouse_wheel()`) comes along for free here as a manual bias over the auto-camera. Until
   then, mouse does nothing on purpose.
4. **A *moving* Mun.** Deliberately deferred: a static Mun keeps `predict()` exact (it
   integrates the same two fixed wells the ship feels). A moving Mun forces `predict()` to
   propagate the Mun's future position across its lookahead — patched-conics / SOI
   territory.
5. **MVP-2: parts-bin builder.** Feed `DESIGNS[]` from parts you snap together instead of
   the three hardcoded test rockets (FALCON / PENCIL / BRICK).

## Controls
◄ / ► steer · **Z** (hold) thrust (on the pad only lifts if TWR > 1) · **SPACE** stage /
relaunch · **R** reset to pad · **1 / 2 / 3** swap rocket (balanced · finless tumbler ·
underpowered).

## Tuning levers (quick reference)
- World scale: `R_PLANET`, `G_SURF` (orbit size + burn flyability).
- Mun reachability: `MUN_X` / `MUN_Y` distance — **must sit inside the stock rocket's
  apoapsis reach** (~330 px for FALCON). Placed past ~340 px, no stock rocket can climb to
  it. `MUN_SOI` sets the encounter window.
- Rockets: `DESIGNS[]` (dry/fuel/thrust/burn/fins per stage).
</content>
</invoke>
