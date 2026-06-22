# Road program — current state (2026-06-22)

> One-screen snapshot of where the whole road/streets effort stands, tying the tier-specific
> maps together. Detail lives in [`road-geometry-handoff.md`](road-geometry-handoff.md) (the
> ★ interchange map) and [`road-hierarchy-notes.md`](road-hierarchy-notes.md) (the research +
> the layered hierarchy). **Update the date + the table when a tier moves.**

## The thesis

Roads sit on a **mobility ↔ access** hierarchy. We build proof-of-grammar **sandboxes** bottom-up
— each one pins a tier *deterministically* (pure generators, spec-locked) — before a seed-driven
**world** composes them. The research ([`road-hierarchy-notes.md`](road-hierarchy-notes.md)) mapped
the whole hierarchy and flagged what was unmodeled; this is the build status against that map.

## Where each tier stands

| Tier | Cart | Status | What it proves |
|---|---|---|---|
| **Freeway / interchange** (grade-separated) | `roadlab` | ✅ done | the whole roads.org.uk catalogue (diamond/cloverleaf/stack/trumpet/fork/triangle/roundabout) generates from `(legs, type)` at any skew/lane-count — arc-spline ramps, clothoid joints, lane tapers, flyover elevation |
| **The seam** (access control / continuity tenet) | — | understood, not code | the line between the ramp world (above) and the at-grade world (below) — §1/§3 of the research |
| **At-grade junction** (Facet A) | `streetlab` M1–M3 | ✅ done | curb-return fillets (tangent arc), the leg layer (skew + the T), turn lanes + raised median channelizing islands — all angle-agnostic |
| **Local/collector network** (Facet B) | `streetlab` M4 | ✅ done | a seed-driven graph in 4 canonical patterns — grid / organic / radial / cul-de-sac — that the SNDi metrics (mean degree, dead-ends) actually separate (§8.2) |
| **(prior) spline road-world** | `roadnet` / `roadnet2` | partial | the deterministic highway-tier spline network; roadnet2 already dabbles in street patterns + cul-de-sacs |

**Headline:** every tier the research flagged as unmodeled now has a working, spec-locked sandbox.
The *grammar* of roads — interchange → at-grade junction → street web — is built end to end.

## Built this cycle (2026-06-22)

- **`streetlab`** (new cart) — the at-grade sibling of roadlab. M1 curb returns · M2 skew + the T ·
  M3 turn lanes + channelizing islands · M4 the street web (grid/organic/radial/cul-de-sac) · M5 sidewalks +
  crosswalks · M6 the mini-roundabout (traversable island, give-way entries, splitters — the at-grade ring) ·
  M7 the typed cross-section (median/bike/parking lane types; HW = sum of lanes, so the junction re-solves).
- **`spec()` harness** (new infrastructure, [`spec-harness.md`](spec-harness.md)) — the gameplay twin
  of `tune-check`; `streetlab` is the first/reference cart (50 assertions). **`roadlab` is now spec-locked
  too** (25 assertions: the `classify_turn` chirality, the `make_junction` generator counts, the splines
  landing on their ports) — a regression lock and a **golden reference for the Phase-2 port**. Both road
  sandboxes are pinned before the world step.

## The frontier — Phase 2 (not started)

The grammar is done; **composing a world** is the open work, and it has two halves the research named:

1. **The §8.4 two-tier generator** — major arterials first, then fill local streets *within each region*
   (Parish-Müller L-system / Chen tensor fields). Today one pattern fills the whole view; the real thing
   emits a *different* `(pattern, region)` per place and stitches them at the boundaries.
2. **Integration** — where the three sandboxes converge into one seed-driven map: `streetlab` (at-grade)
   × `roadnet2` (network/highway) × `roadlab` (interchanges). Today they're separate benches.

This is the step the interchange handoff has always pointed at ("a world that EMITS `(legs, type)` per
crossing from the seed").

## Honest caveats

- Sandboxes prove the **grammar**, not production fidelity: `streetlab`'s network is toy-scale (54 nodes,
  a jitter/spanning-tree morph) — **not** the actual L-system or tensor-field generators the papers describe.
- **Unbuilt Facet A primitives** the research named: staggered junctions, free-right slip lanes,
  signalized-vs-priority control, pedestrian refuge islands. M3/M5 did the headline ones (turn bays, medians,
  sidewalks, crosswalks).
- **Open research questions** remain — the per-pattern numeric metric table, a superblock/fused-grid
  algorithm, and state-of-the-art beyond the 2001/2008 pillars (learned generative models). See
  [`road-hierarchy-notes.md`](road-hierarchy-notes.md) → "Open questions".

## Backlog — loose ends scattered through the research (ranked, addable now)

Pulled from `road-hierarchy-notes` §2 / §5 / §8 — named in the research, in-scope for the sandbox, unbuilt.
Cheap + high-value at the top. (✓ = shipped since.)

**At-grade junction (Facet A — §2/§5), in `streetlab`'s junction view:**
- ✓ **Sidewalks + crosswalks** (M5) — the *pedestrian layer*. 'k' toggles an SW-wide kerbside sidewalk ring
      (the footprint inflated by SW, wrapping the corners) + zebra crosswalks at each mouth. Composes with skew/T.
- ✓ **Degree-1/3/4 share metric** (network view) — §8.2's real discriminator ("mean degree ALONE is not
      enough"). A "node mix" readout under the metrics line: `cul(1) % · T(3) % · X(4+) %` (Marshall's node
      taxonomy), spec-locked to actually SEPARATE the patterns (grid = 0 cul-de-sacs + a big X share; cul-de-sac
      = a real degree-1 share + far fewer Xs). Completes the SNDi readout next to degree/dead-ends/sinuosity.
- ✓ **Real cross-section / lane types** (M7) — §5 OpenDRIVE lane types: a lane-section from the centre out,
      `[median] · driving×N · [bike] · [parking]`, toggled per element (m/b/;). The median is the same island
      primitive as M3's splitter / M6's central island, run continuously. Key trick: HW = `cross_hw()` = the
      SUM of the lanes present, and every junction primitive keys off HW ⇒ widening the section re-solves the
      whole junction (curb returns, mouth, roundabout, sidewalks) for free. Spec'd. (Sidewalk = the M5 layer.)
- ✓ **Mini-roundabout** (M6, at-grade, §2) — a junction "treatment" (`r`, mutually exclusive with turn lanes):
      a circulatory disc + a MINI, TRAVERSABLE (mountable/domed) central island, flush teardrop SPLITTER
      islands, GIVE-WAY (yield) entry lines + CCW circulating arrows. Leg-model based ⇒ composes with skew/T/
      lanes/sidewalks. Distinct from roadlab's grade-separated ring. Spec'd: island ⊂ inscribed circle, stays mini.
- [ ] **Corner free-right slip + triangular channelizing island** (§2) — the corner treatment deferred in M3.
- ✓ **Curb extensions / bulb-outs** (§2) — `draw_bulb()` fills the parking clear-zone at each mouth, shortening
      the crossing (auto with pavement+parking). Pedestrian refuge islands are already covered (the turn median +
      the roundabout splitter both serve as refuges).
- [ ] **TWLTL** (two-way centre left-turn lane) + **right-turn pockets** + **driveways as low-volume junctions** (§2).

**Network topology (Facet B — §8), in the network view:**
- [ ] **Fused-grid / superblock** pattern (§8.3) — perimeter arterial loop + calmed/discontinuous interior;
      the §8 open questions note *no algorithm exists in the 2001/2008 pillars*, so a small original bit.
- [ ] **Dendricity + circuity** metrics (§8.2) — the rest of the four-measure SNDi. Circuity needs shortest-path
      (heavier); defer until there's a reason.
- ✓ M4a–c: grid/organic/radial/cul-de-sac patterns + the curvature knob (winding, live sinuosity).

**Phase 2 — the world (§8.4), beyond a single screen:**
- [ ] **The two-tier major→minor generator** — major arterials first, then fill local streets per region
      (Parish-Müller / tensor fields). The bridge from "one pattern fills the screen" to a seed-driven world.
- [ ] **One-way streets** — directionality is a *network* property: a one-way is a DIRECTED edge (the graph is
      undirected today), and a junction inherits "this arm is entry-/exit-only" from it. Phase-2 territory, not a
      cross-section shape — but it breaks the symmetric two-way mirror (no centreline, single flow), so the M7
      lane-section models the HALF-section as the unit (mirror at the draw site) ⇒ one-way is later a flag, not a rewrite.
- [ ] **Integrate** `streetlab` × `roadnet2` × `roadlab` into one seed-driven map (the seams below).
- [ ] **Open research Qs** — per-pattern numeric metric table, Marshall route-structure mapping, learned
      generative models. See [`road-hierarchy-notes.md`](road-hierarchy-notes.md) → "Open questions".

## Roadmap — the ordered execution plan (set 2026-06-22)

**Guiding principle:** finish each tier's grammar IN ISOLATION (spec-locked) before composing tiers — the
sandboxes pay off most when each is complete. Three stages, bottom-up. Do NOT jump to the Phase-2 "bridge"
early; exhaust what can be built in isolation first.

**Stage 1 — finish the at-grade JUNCTION (Facet A).** Close out `streetlab`'s junction view, increasing effort:
1. ✓ **Bulb-outs / curb extensions** (done 2026-06-22) — `draw_bulb()` fills the parking clear-zone (#8) with a
   sidewalk-grey kerb extension + inboard kerb, auto when pavement+parking, and the zebra shortens to the
   driving lanes (sits between the bulbs). Bike off ⇒ a classic bulb to the kerb; bike on ⇒ a corner refuge
   island inboard of the wrapping bike lane (a protected-intersection corner). Reuses the existing lane offsets.
2. **Corner free-right slip + triangular channelizing island** *(medium)* — the corner treatment deferred in
   M3; reuses the island primitive (median/splitter/roundabout-island lineage). The one notable item left.
3. **Minor markings pass** *(low, batched)* — TWLTL (two-way centre left-turn) + right-turn pockets + driveways.
4. *Parked:* staggered junctions + signalised-vs-priority CONTROL (the latter is a signals/phasing layer, not
   at-grade geometry — its own thing). Revisit only on request.
   → Facet A complete + spec-locked; the junction cart is done.

**Stage 2 — finish the NETWORK sandbox (Facet B).**
5. **Fused-grid / superblock pattern** *(high value)* — perimeter arterial loop + calmed/discontinuous interior.
   The one original bit (no algorithm in the 2001/2008 pillars) AND a single-region prototype of the two-tier
   world, so it de-risks Stage 3 rather than being throwaway.
6. *Deferred:* dendricity + circuity metrics (circuity needs shortest-path) — add only if needed to separate
   the superblock from the other patterns numerically.
   → Network sandbox complete; the superblock teaches the region model.

**Stage 3 — Phase 2: compose the WORLD (the frontier; needs all sandboxes done).**
7. **Directed network → one-way streets** — directionality is a network property; the junction inherits
   entry/exit arms. The M7 half-section groundwork makes the one-way cross-section a flag.
8. **Two-tier major→minor generator** — major arterials first, fill local streets per region; the superblock
   (step 5) generalises straight into this.
9. **Network→junction zoom** — each network node renders in detail via the `streetlab` junction layer (the
   `gen_network` seam already marked in code).
10. **Integrate** `streetlab` × `roadnet2` × `roadlab` into one seed-driven map (grade-separated crossings call
    roadlab's `make_junction`).
11. *Ongoing/optional:* the open research questions (per-pattern metric table, learned generative models).
   → A seed-driven world that emits `(pattern, region)` per place and `(legs, type)` per crossing.

**Natural stopping points:** end of Stage 1 (junction done), end of Stage 2 (both sandboxes done — the clean
"stop and combine later" line), and each Stage-3 step is its own deliverable.

## Cross-section composition — known issues + the next-pass plan (2026-06-22)

M7 added typed lanes, but the EARLIER milestones (M3 turn lanes, M6 roundabout, M5 peds) still compute lane
offsets their own way — they predate the lane model. Collected issues:

1. **Turn-lane markings ignore the median offset** — the left-turn delineation + arrow are hardcoded at
   `LANEW` from the centre, not at the inner *driving* lane, so with a median they land on/in the median.
2. **Two medians stack** — the M3 turn splitter draws on top of the M7 continuous median. A turn bay should be
   the *gap in* the median, not a splitter laid over it.
3. **Stop bar spans the full HW** — including the bike + parking lanes (should stop at the driving lanes).
4. **Roundabout draws no lanes** — `cross_markings` runs only in the non-roundabout branch, so median/bike/
   parking widen the arms but paint nothing.
5. **Bike lane drops at the junction** — (5a) it should WRAP THE CORNER (a terracotta arc concentric with the
   curb return, one band inboard of the sidewalk ring — same construction as M5); (5b) the straight-through
   crossing (elephant's feet) is contextual → optional/deferred.
6. **Pavement bundled with crosswalks** under `k` — the sidewalk is a lane type; it wants its own toggle.
7/8. Cosmetic: markings start abruptly at the mouth; parking should end back from the junction (a clear zone) —
   the same cleanup the corner-wrap (5a) needs.

**The through-line: there's no single shared lane model.** The fix is one refactor + a follow-up:
- ✓ **Pass 1 — the lane-section as the single source of truth** (shipped 2026-06-22). `drive_inner()`/
  `drive_outer()` are the shared offsets; turn lanes (#1 delineation+arrow off the inner driving lane, #2 the
  splitter only when there's no median — the bay is the median's gap), the stop bar (#3 spans the driving lanes
  only), the roundabout approaches (#4 `cross_markings` now runs there) and pavement (#6 its own lane-type
  toggle on the cross-section row) all read from it. Half-section is the unit (one-way readiness). Spec'd (53).
- ✓ **Pass 2 — bike corner-wrap (#5a) + parking clear-zone (#8)** (shipped 2026-06-22). Reordered the section so
  the bike lane is OUTERMOST (kerb-side), parking inboard — the Dutch protected arrangement. The bike lane now
  WRAPS each corner: `corner_bike()` fills a terracotta annular band just inside the curb-return arc (radii
  R..R+BIKEW about the return centre), meeting each arm's kerb-side lane at the tangent points. Parking markings
  end `PCLEAR` back from the junction (a clear zone). Spec'd the lane order (58).
- ✓ **Pass 3 — the straight-through bike crossing, made OPTIONAL (#5b)** (shipped 2026-06-22). The bike toggle is
  now a 3-state cycle: off → on (lanes + the always-on corner-wrap) → +crossing. At level 2, `bike_thru()` draws
  elephant's-feet (a dashed row of terracotta squares) continuing each bike lane straight across the box — opt-in
  because it's contextual (signalised vs priority), exactly "some junctions want it, others don't". Spec'd the
  3-state cycle (60). (#7 transition-zone polish left as acceptable — the unmarked junction box is normal, and the
  bike crossing now optionally carries the lane through.)
- ✓ **Bike-lane composition follow-ups** (2026-06-22) — the bike lane now shows where the other treatments are:
  (a) with TURN LANES, `cross_markings` split into a kerb-side datum (bike/parking, at the corner clearance) and
  a centre datum (centreline/median/dividers, pushed past the turn bay) — a central turn bay no longer shoves the
  kerb-side bike lane off the arm; (b) at the ROUNDABOUT, the bike lane CIRCULATES — a terracotta ring just inside
  the kerb that meets the approach bike lanes at each mouth.

## Known deferrals (pick up when convenient)

- *(resolved 2026-06-22)* `streetlab`'s `index.json` gallery description is now current through M7 (committed in
  a quiet moment when the registry was clean). Left here as a note: when it lags again, update only the streetlab
  entry; if the file is dirty with foreign edits, prefer "just ship it" for a ref-safe description change.

## Shared code & seams

**Duplication today is minimal — and that's correct.** roadlab and streetlab share only 6 function names, and
the only real overlap is trivial (`ux`/`uy` trig wrappers, `step_btn`, and the generic spec helpers). All the
*geometry* is deliberately distinct (interchange ramps vs at-grade curb-returns/network) — a shared geometry
file now would be over-abstraction. **No `roadkit.h` yet.**

- **Done:** the generic spec helpers (`spec_near`, `spec_close`, `spec_tap`) now live in
  [`runtime/spec.h`](../../runtime/spec.h), so every spec cart shares them instead of redefining them.
- **Specs on an includeable:** a shared header can't define `spec()` (one per cart), but it can expose a
  `void <lib>_selfcheck(void)` of `expect()`s that the cart's `spec()` calls — the pattern a future `roadkit.h`
  would use to carry its own tests. (Documented at the bottom of `spec.h`.)
- **The seams (marked `// SEAM (Phase 2)` in code):**
  - `roadlab.c` → `make_junction(legs, type)` is the interchange drawer a world calls per grade-separated crossing.
  - `streetlab.c` → `gen_network(pattern, seed)` is the per-region local-street generator a two-tier world calls;
    each network **node is a crossing the junction layer (M1–M3) can render in detail** (the network→junction zoom).
  - A `roadkit.h` becomes worthwhile **only if** Phase 2 makes both carts share primitives — extract then, not now.

## Pointers

- **Carts:** `tools/carts/{roadlab,streetlab,roadnet,roadnet2,interchange,rampkit}.c`
- **★ interchange map (start here for the ramp work):** [`road-geometry-handoff.md`](road-geometry-handoff.md)
- **Research + layered hierarchy:** [`road-hierarchy-notes.md`](road-hierarchy-notes.md)
- **The DSL / OpenDRIVE recon / geometry refs:** [`interchange-dsl.md`](interchange-dsl.md) ·
  [`junction-lanelink.md`](junction-lanelink.md) · [`road-geometry-refs.md`](road-geometry-refs.md)
- **The working method (build loop + verify gates):** [`roadlab-method.md`](roadlab-method.md)
- **The test harness:** [`spec-harness.md`](spec-harness.md)
