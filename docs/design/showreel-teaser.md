# showreel / teaser — the big game's trailer as a forcing function

STATUS: BRAINSTORM / planning (2026-06-22). No code. The point of this doc: deciding what a
teaser would *show* enumerates what we'd have to *build* — so the shot list doubles as a
prioritized gap list (systems and API). Pairs with [`transitions.md`](transitions.md) (how
shots are cut) and [`cart-clips.md`](cart-clips.md) (how shots are baked).

## The game

A **lo-fi GTA1-but-better**: an infinite, deterministic, procedural world you drive around,
assembled from many small spec-locked sandbox carts rather than one monolith. The thesis
([`second-north-star.md`](second-north-star.md)): *author the honest rules, then let the
system judge you* — nothing faked. The original brief ([`roadnet-handoff.md`](roadnet-handoff.md)):
"GTA1-style + Cataclysm-DDA-flavoured driving/adventure — infinite, deterministic, good for
driving around a city and roaming."

## The pieces today (and how done)

| Cart | Tier | What it is | Status |
|---|---|---|---|
| **sloop** | street | Build a car from parts on a grid; how it drives is *computed* (mass/COM/inertia → accel/grip/drift). Collidable world (cones, houses, parked cars, chain reactions). BUILD↔DRIVE flip. | Rungs 1–2.7; drives, builds, collides |
| **streetlab** | at-grade | The grammar of ground-level streets — junctions, turn lanes, mini-roundabouts, sidewalks, crosswalks, typed cross-sections; 4 network patterns. Seam: `gen_network(pattern,seed)`. | **COMPLETE, spec-locked (60)** |
| **roadlab** | grade-sep | Interchange geometry — diamond/cloverleaf/loop/stack/trumpet/roundabout; arc-spline ramps, clothoids, elevation. Seam: `make_junction(legs,type)`. | **COMPLETE, spec-locked (25)** |
| **roadnet / roadnet2** | region | Infinite procedural roads over a deterministic heightmap; highways + towns; street-level lens with RCI zones + lots. Seams: `road_at(wx,wy)`, `building_at(wx,wy)`, `gnode[]`/`gedge[]`. | v1 rungs 1–3; **v2 vector rebuild not built** |
| **flank** | street | Top-down squad combat — comms, flanking, flow-field paths, cover, suppression, alertness, stealth. Explicitly the enemy brain for sloop's streets. | Building |
| **roadhouse** | audio | A radio station (modal psych-rock, procedural soloist). Not part of the world — but it's the **car radio** the teaser wants. | Shipped |

The architecture is **three tiers converging at a world level** that *doesn't exist yet*:
region (roadnet) → junctions (roadlab) + streets (streetlab) → street-level actors (sloop +
flank). The seams between them are designed and named; the composer is the frontier.

## The teaser — a shot list (= the gap list)

A ~30–60s trailer. Each shot: what it shows → what it draws on → can we record it **now**, or
what's the gap. Tagline candidate: *"Build a car. Drive an infinite city. Nothing is faked."*

| # | Shot | Draws on | Recordable now? / Gap |
|---|---|---|---|
| 1 | **Build a car** — bolt parts on the grid, COM crosshair + mass/top-speed readouts update live | sloop BUILD | ✅ **Now** (sloop) |
| 2 | **Drive off** — the rig pulls away, handling visibly from the build | sloop DRIVE | ✅ **Now** |
| 3 | **Drift** through a bend, grip breaking | sloop | ✅ **Now** |
| 4 | **Chaos** — chain-reaction collision through parked cars / cones | sloop collidable world | ✅ **Now** |
| 5 | **Road variety** — a cloverleaf, then a city grid with turn lanes | roadlab, streetlab | ⚠️ as *separate* carts now; one drivable view = integration gap |
| 6 | **Scale** — continental map zooms down to a single street | roadnet lens + `camera_ex` zoom | ⚠️ pieces exist; the smooth zoom in one cart = gap |
| 7 | **Combat taste** — enemies flank and take cover as you roll up | flank | ⚠️ runs as its own cart; wiring into the driving world = gap |
| 8 | **The radio** — a station plays as you cruise | roadhouse + spatial audio | ⚠️ audio-while-driving not wired |
| 9 | **Title card** — name + tagline | a card cart / font-bake | ✅ **Now** |

The reveal this table makes: **5 of 9 shots are recordable from existing carts today.** A teaser
is reachable in two horizons.

## Two horizons (this is the "what next" answer)

**v0 — the STITCHED teaser (reachable now, forces the clip pipeline).** Cut shots 1–4, 9 (and
rough versions of 5/7 from the standalone carts) into one reel. Requires no game integration —
it requires finishing the **clip/showreel pipeline** we already scoped:
1. Author good demo tracks per cart → `tools/clips/<cart>/NN-label.{script,beats,rec}`.
2. Bake them — `make-gif.js --all` (today only 1 of ~10 committed tracks is baked).
3. **Audio in clips** — mux `--wav` into the bake (clips are silent today; for sloop's engine,
   flank's gunfire, roadhouse's music this is non-negotiable). See [`cart-clips.md`](cart-clips.md) "Sound".
4. **Stitch** — `tools/compose-clips.js` (Layer B in [`transitions.md`](transitions.md)): glue
   the shots with [transitions](transitions.md) (the LUMA/iris/wipe work) into one video.

v0 is honest if it's a *vision* teaser ("here's the world we're building") and dishonest if it
implies the shots are one continuous game — a call for the maker (below).

**v1 — the INTEGRATED teaser (needs Phase 2; one continuous in-engine take).** The real game:
a single world cart where you drive sloop through roadnet's streets, junctions render via
roadlab/streetlab, flank spawns as combat, and the map↔street flip is a [transition](transitions.md).
This is the actual frontier:
1. **roadnet2** vector-native rebuild (replaces v1's field+graph mess).
2. **The world composer** — one cart calling `road_at()` / `make_junction()` / `gen_network()`.
3. **sloop wired to `road_at()`** — drive the procedural surface (roadnet rung 4).
4. **flank wired** as street-level combat in that world.
5. **map↔street mode flip** — exactly an in-cart [transition](transitions.md) (the showcase
   payoff of that whole thread).
6. **Audio wiring** — engine/collision SFX + the radio reacting to driving state.

## What's actually missing — systems, not API

The primitive API is **largely already there** (verified against `studio.h`): `lerp`, `clamp`,
`angle_to`, `camera_ex` (zoom+angle — the map↔street shot rides this), `boxes_touch`/collision,
`save`/`load`, spatial audio (`listener`/`note_pos`/`hit_at`), `rnd`, `ease_*`. So the gap is
**not** missing draw/math primitives — it's:

- **The composition layer** (the world cart that wires the seams). The single biggest piece.
- **Street-level rendering of the procedural world at speed** — systems work, but note roadnet
  is *seam-true* (`road_at(wx,wy)` is a pure function of world coords), so it's "render the
  query," not "fit a giant map array." `MAP_W/H` (128/64) is not the world.
- **The clip/showreel pipeline finish** (bake + audio mux + `compose-clips`) — for v0.
- **Audio integration** into gameplay (engine note, collision hits, reactive radio).

Genuinely-missing *API/engine* candidates (small, verify before building):
- **Analog gamepad steering** — only `BTN_A/B` map a pad today; a driving game wants an analog
  stick. (STATUS tracks gamepad.)
- **Events** (`broadcast`/`received`) — designed, listed open in STATUS; useful for
  spawns/triggers in the world. Not in `studio.h` yet.
- **`lengthdir_x/y`** (polar→cartesian) — `angle_to` exists, the inverse helper doesn't; minor.

## How the teaser gets made (ties the session's work together)

The teaser is the *reason* the clip/showreel pipeline matters, and it uses every piece we
touched: **demo tracks** (committed `.script`s) → **`make-gif.js`** bakes each shot → **audio
mux** gives them sound → **transitions** (LUMA/iris/wipe, and `compose-clips` for cross-shot
cuts) assemble the reel → it headlines the **curated showcase** ([ADR-0020](../decisions/0020-in-house-tool-curated-showcase.md)).

## Open questions (maker calls — needed to lock the teaser)

1. **The loop / fantasy.** Is the game primarily *driving + roaming* (GTA1 top-down), or
   equally *on-foot missions + combat* (flank)? This decides how much shot 7 (combat) leads.
2. **Honest-now vs. vision teaser.** Should the teaser only show what's integrated and
   continuous (wait for v1), or is a stitched vision-reel of standalone carts (v0) fair? Lean:
   v0 as an internal "does this excite us" cut; v1 as the real public teaser.
3. **Do we build v0 first?** It's reachable now and de-risks the pipeline + tells us if the
   *feel* sells before the integration spend — same logic as the attract-mode native-first call.
4. **Format / length / home** — a single hero clip on the gallery front page? Length?

## Related

- [`transitions.md`](transitions.md) — cutting shots (in-cart flips + the stitch layer).
- [`cart-clips.md`](cart-clips.md) — baking shots; the silent-audio gap.
- [`../decisions/0020-in-house-tool-curated-showcase.md`](../decisions/0020-in-house-tool-curated-showcase.md) — where the teaser lands.
- [`road-program-state.md`](road-program-state.md), [`roadnet2-plan.md`](roadnet2-plan.md),
  [`roadnet-handoff.md`](roadnet-handoff.md) — the road tiers + the integration frontier.
- [`second-north-star.md`](second-north-star.md) — the deep-sim-behind-lo-fi thesis.
