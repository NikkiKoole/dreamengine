# junction + laneLink — the formal junction data model (OpenDRIVE ↔ our DSL, 2026-06-17)

> Part of the road-geometry effort — map & how-we-got-here: [`road-geometry-handoff.md`](road-geometry-handoff.md).
> Reconciles the standard against our DSL: [`interchange-dsl.md`](interchange-dsl.md) (Layer 1).
> The geometry it omits lives in: [`road-geometry-refs.md`](road-geometry-refs.md) + roadlab M1–M5.

This is the **decided next step** from the handoff: read OpenDRIVE's `junction`/`laneLink`, reconcile it
against our junction DSL (matches + gaps), and sketch the C data types roadlab/roadnet2 would use. It
costs ~no geometry code — it's a *schema* — and the point is to **validate the DSL against the standard**
and pin the representation roadnet2 serialises junctions into.

## The 30-second version
OpenDRIVE stores a junction as pure **connectivity** — *which road's lane feeds which road's lane* — and
puts the **shape** of every ramp in that ramp's own road geometry (`planView`). That is **exactly our
two-layer split**: Layer 1 (topology DSL) = `junction`/`connection`/`laneLink`; Layer 2 (the curves) =
roadlab's `LINE|ARC|CLOTHOID` ref-line + lanes + width + elevation (M1–M5). The standard confirms the
architecture. We adopt `connection`+`laneLink` close to verbatim, and keep **one field the standard
doesn't have** — the ramp *primitive* (`loop`/`flyover`/…), because that's our generative input that
*produces* the connecting-road geometry. OpenDRIVE is the lowered/baked form of what our DSL authors.

---

## 1. The OpenDRIVE model (firsthand, spec v1.8.1 §12)
Source: ASAM OpenDRIVE §12 Junctions —
<https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/12_junctions/12_01_introduction.html>
(§12.4 Connecting roads for `connection`/`laneLink`).

- **`<junction>`** — `id`, `name`, `type` ∈ {`common`/default (an *area* where roads meet; the normal
  case), `direct` (no separate connecting-road geometry — the ramp road links straight on; for simple
  highway on/off slips), `virtual` (spans overlapping roads; routing-only)}. A junction **groups
  connecting roads**; a connecting road is just a normal `<road>` whose `<link>` names this junction.
- **`<connection>`** (child of junction) — one path through the junction:
  - `id` — unique within the junction.
  - `incomingRoad` — the road id entering.
  - `connectingRoad` — the road id that *carries* you across (its `planView` **is the ramp shape**).
  - `contactPoint` ∈ {`start`, `end`} — which end of the connecting road touches the incoming road.
  - **Rule:** at most one `<connection>` per `(incomingRoad, connectingRoad)` pair.
- **`<laneLink>`** (child of connection) — the per-lane map:
  - `from` — lane id on the incoming road; `to` — lane id on the connecting road.
  - (`overlapZone`, metres — only `direct` junctions; skip.)
  - **Rules:** specify `laneLink` *only for lanes leading into* the junction; an unlinked lane = **no
    traversable path**; **several `laneLink`s in one connection ⇒ a lane change is allowed** across the
    junction; several single-link connections ⇒ no lane change.
- **Lane numbering** (§11, the context laneLink ids live in): per road, lane `0` = centre; **positive =
  left** of the reference line, **negative = right**, magnitude increasing outward. The sign is relative
  to the *reference-line direction*, **not** to the driving side — driving side is a separate fact.

## 2. The mapping to our DSL

| our DSL ([interchange-dsl](interchange-dsl.md)) | OpenDRIVE | note |
|---|---|---|
| **junction** = legs × {movement → primitive} | `<junction>` | the whole object |
| **leg** = `(bearing, class, terminates?)` | an **incoming road** (one per direction meeting the hub) | a 2-way leg = two incoming roads |
| **movement** `O → D` | a **`<connection>`** (`incomingRoad`=O, `connectingRoad` exits onto D) | road-level |
| **ramp primitive** (`loop`/`flyover`/`diverge`/…) | *(not stored)* — implicit in the connecting road's `planView` | **the gap; we keep it** |
| **port** = `leg.direction.lane` `(point, tangent, width)` | a **`<laneLink>`** endpoint `(road, lane id)` + `contactPoint` | port = roadlab's lane anchor |
| **port A is the ENTRY** (tangent points in; the roadlab gotcha) | `contactPoint` (`start`/`end`) | orientation of the connecting road |
| **completeness** (full vs partial: parclo, half-diamond) | omit the unserved `<connection>`s/`<laneLink>`s | partial = fewer connections, not "broken full" |
| **ring/circulate** (parked; British roundabout family) | `<junctionGroup type="roundabout">` | a *peer* construct, not a connection (matches our finding) |

## 3. Matches — what the standard validates
1. **The two-layer split is real and standard.** OpenDRIVE separates *connectivity* (`junction`) from
   *shape* (each road's `planView`). That is our Layer 1 (topology) vs Layer 2 (geometry) exactly. The
   architecture we reasoned to is the one the standard uses. ✅
2. **`laneLink` IS our movement layer — at finer grain.** Our "movement = leg→leg pair" is road-level;
   `laneLink` records lane→lane. Our claim that **"a port is a lane + its travel direction"** (roadlab's
   design) is precisely a `laneLink` endpoint. roadlab already anchors ports to lanes, so `laneLink` is
   just the *serialisation* of what roadlab ports already are. ✅
3. **The connecting road = our ramp; its primitive is its geometry.** A `loop` vs a `flyover` differ
   only in the connecting road's `planView` (+ elevation `z(s)`, our M5) — confirming the primitive
   belongs in the geometry layer, not the connectivity schema. ✅
4. **Partial junctions fall out for free.** A half-diamond/parclo is simply fewer `<connection>`s — no
   special "partial" flag — matching "a partial is a real type, a deliberate subset." ✅
5. **The ring family is a separate construct.** OpenDRIVE expresses roundabouts with
   `<junctionGroup type="roundabout">` grouping the junctions around a circulatory road — corroborating
   our parked conclusion that `ring`/`circulate` is a **peer** of the ramp grammar, not an assignment in
   it. When we build it, copy `junctionGroup`. ✅

## 4. Gaps / mismatches — where we differ on purpose
1. **Primitive type isn't in the standard — and we keep ours.** OpenDRIVE has no "this is a loop" tag;
   the shape is implicit in geometry. But the primitive is our *generative input*: `serve { O→D: loop }`
   is what *produces* the connecting road's `planView`. So our model sits **one layer above** OpenDRIVE:
   `DSL(primitive)` → *generate* → connecting-road geometry → *serialise* → OpenDRIVE. We retain a `prim`
   field; OpenDRIVE is the baked form that drops it. (Don't try to recover the primitive from geometry.)
2. **Handedness: signed lane id vs `DRIVE`.** OpenDRIVE lane ids are signed by *reference-line side*, not
   driving side. We collapse handedness into the `DRIVE` constant. Reconcile by storing the **signed lane
   id** in the link (so it's standard-faithful and serialisable) and letting `DRIVE` decide which sign is
   the driving side at *generation* time. Don't double-encode handedness (the chirality bug that already
   bit us — see handoff "What failed").
3. **`direct` junctions** — an optimisation for trivial slip ramps (no separate connecting road). We
   always use a connecting road (`common`), so we can ignore `direct` until a perf/serialisation reason
   appears.
4. **`virtual` junctions** — routing across overlapping roads; irrelevant to a 2D top-down drawer. Skip.
5. **Grain of our "movement."** Our DSL movement is leg→leg; the standard is road→connectingRoad with
   lane-level `laneLink`. **Recommendation: roadlab adopts `connection`+`laneLink` as-is** (road-level
   connection, lane-level links) — strictly more expressive than leg→leg, and it's the structure roadnet2
   will serialise and route on anyway.

## 5. Sketch — the C data types (roadlab → roadnet2)
The **topology layer** (interchange-dsl Layer 1), baked to plain C tables in the `JUNCS[]` spirit. The
*shape* of each connecting road lives in its `planView` (roadlab M1–M5), **not here**.

```c
// ── Junction connectivity (OpenDRIVE junction/laneLink, baked to C) ──
typedef enum { JUNC_COMMON, JUNC_DIRECT } JunctionType;       // skip `virtual` (routing-only)
typedef enum { CONTACT_START, CONTACT_END } ContactPoint;      // which END of the connecting road
                                                               //   touches the incoming road

// OUR field, not in OpenDRIVE: the generative primitive that PRODUCES the connecting
// road's planView. OpenDRIVE is the lowered form that drops it. (interchange-dsl Layer 1.)
typedef enum { RP_THROUGH, RP_DIVERGE, RP_MERGE, RP_LOOP, RP_FLYOVER, RP_DIRECT } RampPrim;

typedef struct {
    int from;   // lane id on the incoming road   — signed (0=centre, ±outward); DRIVE picks driving sign
    int to;     // lane id on the connecting road  — same convention
} LaneLink;     // == a roadlab PORT pair (lane anchor + travel dir)

typedef struct {
    int          id;
    int          incomingRoad;     // road id entering
    int          connectingRoad;   // road id carrying the movement across — its planView IS the ramp
    ContactPoint contactPoint;     // = roadlab's "port A is the ENTRY" orientation
    RampPrim     prim;             // OUR generative field (see RampPrim)
    LaneLink     links[8];
    int          nLinks;           // 1 link = no lane change; >1 = lane change allowed across
} Connection;

typedef struct {
    int          id;
    JunctionType type;
    Connection   conns[32];
    int          nConns;           // a PARTIAL junction is simply fewer conns — no flag
} Junction;
```

**How roadlab's current `Port`/`ramp()` maps in:** today a roadlab port is `(lane + travel dir)` at a leg
and a ramp is `ramp(portA → portB, type)`. In this model that becomes one `Connection`:
`incomingRoad = portA.road`, `connectingRoad = <the ramp road>`, `contactPoint` from portA's end,
`prim = type`, and one `LaneLink` per lane carried (`from`/`to` = the two ports' signed lane ids).

**The two directions of travel through the model:**
- **Author / generate:** DSL `serve { O→D : loop }` → `Connection{ prim=RP_LOOP, … }` → roadlab generates
  the connecting road's `planView` (M1 arc-spline → M2 clothoid → M3 lanes → M4 width → M5 z) → render.
- **Store / route:** roadnet2 persists `Junction[]` as the junction representation; `LaneLink.from→to`
  gives lane-level routing (which lane reaches which) **for free** — the thing the standard exists for.

## 6. What this unlocks / next
- **roadlab — DONE (M6, 2026-06-17).** The sketch above is now in `tools/carts/roadlab.c`:
  `RampPrim`/`LaneLink`/`Connection`/`Junction` types + a table-driven `draw_junction()` that strokes each
  connection with the M1–M5 splines, and a `j` toggle between the single-ramp sandbox and the whole
  junction. A `DEMO` connection table (a 4-way pinwheel of direct-curve slip turns over the 4 ports) drives
  it. This is the "re-express `JUNCS[]` as declarations" migration step (interchange-dsl §"Migration path"
  #4) made concrete with a standard-aligned schema. (`prim` is wired but only `RP_DIRECT` has a real drawer;
  `RP_LOOP`/`RP_FLYOVER` fall back to the spline — the natural next geometry work.)
- **roadnet2** gains the serialisation/routing target: store `Junction[]`, route on `laneLink`. The next
  port is to have worldgen *emit* `Junction[]` deterministically (from the seed) and call roadlab's
  `draw_junction()` as the drawer.
- Then the queued items (`ring`/`circulate` → `junctionGroup`; real `RP_LOOP`/`RP_FLYOVER` drawers) have a
  data model to attach to.
- **OpenDRIVE adoption inventory** ([`road-geometry-refs.md`](road-geometry-refs.md)) — `junction`+`laneLink`
  moved from "worth taking next" to **adopted (M6)**. ✅

## 7. Open reflections (post-M6) — what to weigh before the next move
Landing M6 (the schema + table-driven drawer) surfaced five things worth a deliberate decision, plus the
"which world consumes this" fork. Ranked by how much each shapes direction:

1. **`RP_LOOP` + `RP_FLYOVER` drawers — DONE (2026-06-17, §8.1).** `loop_spline()` draws the hard turn the
   long way (≈270°); `scurve_spline()` draws the flyover as an equal-tangent **biarc** (semi-direct reverse
   curve) that crosses the conflict on a raised deck. `l` cycles the sandbox primitive so any port pair
   draws as direct / loop / flyover. → spec **§8.1**.
2. **The generator — DONE (2026-06-17, §8.2).** `make_junction(type)` enumerates every movement, classifies
   the turn (through/right/left by relative bearing, DRIVE-folded), and assigns the primitive from a per-type
   `POLICY` (diamond/cloverleaf/stack differ only in the LEFT column). Pure function of `(ports, type)`. The
   junction view now shows this *generated* junction; `g` cycles the type. The hand-authored `DEMO` is gone.
   → spec **§8.2**.
3. **`Port` collapses road + lane.** roadlab's `Port` is a fixed point; the schema (and OpenDRIVE) have
   `incomingRoad`/`connectingRoad` as *roads* with `laneLink` mapping lane ids. roadnet2 legs are real graph
   edges (width, lane count, bearing), not hand-placed points. roadlab likely needs a thin **`Leg`/`Road`
   layer** (ports *derived* from a road + lane index via `portOf(leg, lane)`) before the port. Thinnest part
   of the abstraction today.
4. **`laneLink` has no teeth.** `nLinks` only sets the lane *count*; the `from`/`to` ids are inert (never
   `from ≠ to`). The case it doesn't exercise — a lane mapping to a *different* lane across the junction — is
   exactly the M4 diverge/merge **gore** and lane-change. M4's taper and `laneLink` are secretly the same
   thing; decide when to wire `from ≠ to` so the gore is data-driven, not a global `taper%`.
5. **Determinism hook.** The per-junction `Connection[]` must be derived from the **cell hash** of the
   crossing (like roadnet2's ranks/hubs), never from iteration order or accumulated float state. The
   hand-authored `DEMO` sidesteps this; the generator (§8.2) is where it becomes real. Cheap up front,
   painful to retrofit.

**The world fork (owner's open question): keep feeding roadnet2, or grow a new junction-first world?**
Take: roadlab stays the geometry lab; the determinism *substrate* (roadnet2's `seedZ` + `hash2` + perlin,
pure-function-of-cell) is worth copying verbatim either way. But **roadnet2 wasn't designed around
interchanges** — its worldgen is organic (POI-node β-skeleton graph, edges meeting at arbitrary
angles/counts = the *hard* case for a generator); junctions were "Part B," bolted on. A world built
**junction-first** (a road *hierarchy* where every crossing is, by construction, a typed junction with clean
leg bearings) is a cleaner substrate for *this study*, not just a smaller roadnet2. Suggested tier:
**roadlab** (one junction, tuned) → **a new seed-driven "junction field"** (sparse grid of crossings, each
generated from its cell hash — the rehearsal that proves the generator + drawer tile deterministically) →
*optionally* fold back into roadnet2 if you later want them in the organic world. Decider: if the goal is
"drop junctions into the existing organic world," retrofit roadnet2; if it's "study how a network grows from
a seed with junctions first-class," grow the new one (the read of this whole sandbox-driven thread).

## 8. Next-milestone specs

### 8.1 — `loop_spline()` + flyover (the hard-turn geometry)
> **✅ loop + flyover DONE (2026-06-17)** — both in `roadlab.c`. `loop_spline()` is exactly as specced below
> (the 2×2 stub/merge solve lands a fixed-R loop on B). **flyover** is `scurve_spline()`: an equal-tangent
> **biarc** (two circular arcs joined with a continuous tangent at a midpoint `J`) — the semi-direct reverse
> curve that crosses the conflict on a raised deck (forced `lift`). Both wired into `draw_connection()`
> (`RP_LOOP`/`RP_FLYOVER`) and the sandbox (`l` cycles direct/loop/flyover). The `Connection` gained
> `float R; int lift;`. The biarc determines its own radii from the geometry, so flyover ignores the `radius`
> slider. *Open polish:* on near-symmetric pairs the biarc is ~a single arc (correct, minimal curvature);
> the S inflection only shows on asymmetric pairs.

**Why the current splines can't.** `arc_spline` rounds the convex corner where A's and B's heading-lines
meet; for a hard (left-equivalent) turn that corner sits *behind* a port (`avail < 2` ⇒ it bails to a
straight stand-in — see roadlab.c). Loops and flyovers are the missing primitives.

**`loop_spline(Port a, Port b, float R, float* xs, float* ys)`** — closed-form (Tier 1), drive-side from
`DRIVE`. A loop serves a hard turn by going the **long way** (≈270°) so it never crosses opposing traffic:
- `adA = a.dir + 180` (entry heading into the junction).
- wanted deflection `Δ = norm(b.dir − adA)`; **loop sweep** `= Δ − 360·sgn(Δ)` (≈ ±270° for a 90° turn).
- **diverge:** a short tangent leaving A along `adA` (peel onto the loop).
- **loop arc:** centre `C = tangentStart + R·normal(adA)` on the side of the sweep; emit the radius-`R` arc
  over `loopSweep` (16–24 samples).
- **merge:** from the loop's exit (heading now `== b.dir`) a short straight / reverse-curve tangent onto B.
- `R_loop` is its **own** parameter (larger than a corner radius) → two nested loops are just **concentric
  offsets** (M3), no solver. Add `float R;` to `Connection` (0 ⇒ use the global radius).

**flyover (`RP_FLYOVER`).** Topologically a *semi-direct* left turn that **is allowed to cross** the conflict
point because it's grade-separated. Geometry = a **reverse-curve (S) direct connector** (not the
corner-rounder) **+ forced deck `liftPx > 0`** so it draws over (M5 draw-order-by-z). Cheapest first cut:
run the existing spline but (a) for this prim *don't* bail to straight when the corner is behind (allow the
S), and (b) take a non-zero lift from the `Connection`. So flyover mostly leans on M5 — the genuinely new
geometry is the loop. Add `int lift;` to `Connection` (0 ⇒ at grade).

**Drawer wiring:** `draw_connection()` switches on `prim` — `RP_LOOP → loop_spline`, `RP_FLYOVER → spline
(S allowed) + lift`, else the existing arc/clothoid spline.

### 8.2 — `make_junction()` (the generator: type → `Connection[]`, interchange-dsl Layer 1 in code)
> **✅ DONE (2026-06-17)** — `make_junction(nleg, type, &out)` is in `roadlab.c`, close to the sketch below:
> it enumerates every leg pair, `classify_turn()` sorts each by relative bearing (DRIVE-folded), and the
> `POLICY[]` table assigns the primitive. `g` cycles diamond/cloverleaf/stack; the junction view draws the
> generated table (the hand `DEMO` is gone). For roadlab the **`Leg` layer is implicit** — the 4 legs are
> encoded in the port layout (`leg_in(L)=L*2`, `leg_out(L)=L*2+1`), so `portOf` is just those two helpers;
> the explicit `Leg{bearing,lanes}` struct below is what the *roadnet2* port will need (ports derived from
> real graph edges). Generated loops get a compact per-connection `R`; `through` is emitted but not drawn (it
> *is* the road). The 4-way generates 12 movements (4 through + 4 right + 4 left).
>
> **Primitive count reconciled:** the DSL has 5 atoms (`through/diverge/merge/loop/flyover`); roadlab's enum
> is **4 drawers** (`RP_DIRECT/RP_LOOP/RP_FLYOVER/RP_THROUGH`) because **diverge + merge are the universal
> lead-in/run-out segments of *every* ramp** (the spline's first/last LINE), not standalone ramp kinds — so
> they stay implicit, exactly as the grammar's `diverge → [loop|flyover|direct] → merge` says.

A junction **type is a policy, not geometry**: which primitive serves each turn class, and which movements
are served (completeness).
```c
typedef struct { int bearing, lanes; int terminates; } Leg;   // a road meeting the hub
typedef enum { T_THROUGH, T_RIGHT, T_LEFT, T_UTURN } Turn;
typedef struct { RampPrim through, right, left; int serveUturn; } JuncPolicy;
static const JuncPolicy POLICY[] = {
  [JT_DIAMOND]    = { RP_THROUGH, RP_DIRECT, RP_DIRECT,  0 },  // left = at-grade signal
  [JT_CLOVERLEAF] = { RP_THROUGH, RP_DIRECT, RP_LOOP,    0 },
  [JT_STACK]      = { RP_THROUGH, RP_DIRECT, RP_FLYOVER, 0 },  // trumpet/parclo = subsets/mixes of these
};
// classify by RELATIVE bearing; DRIVE folds handedness so "right" is always the easy side
Turn classify_turn(int fromBearing, int toBearing){
  int rel = norm180(toBearing - fromBearing);          // (−180,180]
  if (abs(rel) > 150) return T_THROUGH;                // ~straight
  if (abs(rel) <  30) return T_UTURN;
  return (rel * DRIVE > 0) ? T_RIGHT : T_LEFT;
}
int make_junction(Leg legs[], int n, JuncType type, Connection out[]){
  JuncPolicy p = POLICY[type]; int m = 0;
  for (int i=0;i<n;i++) for (int j=0;j<n;j++){ if (i==j) continue;
    Turn t = classify_turn(legs[i].bearing, legs[j].bearing);
    if (t==T_UTURN && !p.serveUturn) continue;
    RampPrim prim = t==T_THROUGH ? p.through : t==T_RIGHT ? p.right : p.left;
    out[m++] = (Connection){ portOf(i, exitLane), portOf(j, entryLane), prim, /*laneLinks*/…, lanes };
  }
  return m;
}
```
- **`portOf(leg, lane)`** is the `Leg`/`Road` layer from reflection #3 — places a port on a leg's lane.
- **Determinism (reflection #5):** `legs` come from the cell hash in worldgen; `make_junction` is a **pure
  function of `(legs, type)`** ⇒ identical `Connection[]` for a given seed. No RNG, no order-dependence.
- **Lands in roadlab:** replace the hand-authored `DEMO` with `make_junction(legs, type, …)`; the `j` view
  picks a `type` (diamond / cloverleaf / stack) and shows the *generated* junction — the first end-to-end
  proof of type → table → drawer. This is also the function the new "junction field" world (or roadnet2)
  calls per crossing.

## 9. After the generator — the `(topology × policy)` plan (skew · trumpet/topology · ring)
The generator (§8.2) finished the **policy axis**: *how the hard turn is served* — direct / loop / flyover =
diamond / cloverleaf / stack. Mapping the full **roads.org.uk interchange catalogue**
(<https://www.roads.org.uk/interchanges> — our reference, re-read 2026-06-17) against what roadlab generates
shows the catalogue is organised by **leg count** — i.e. a *second* axis we haven't built: **topology**. A
junction recipe is then **`(topology × policy)`**, exactly how the catalogue is laid out.

| legs | type | turns served by | roadlab today |
|---|---|---|---|
| 2 / cross | diamond | direct ramps | ✅ generates |
| 2 / cross | cloverleaf | 4 loops | ✅ generates |
| 2 / cross | four-level stack | 4 flyovers | ✅ generates |
| 2 / cross | partial cloverleaf | mix loop + direct | ⚠️ needs per-movement policy |
| 3 / T | **trumpet** | one loop + one semi-direct + 2 direct | ❌ needs **topology** |
| 3 / T,Y | fork | pure diverge | ❌ needs topology |
| 3 / T,Y | triangle | 3 flyovers | ❌ needs topology |
| 2 / ring | dumbbell · roundabout · 3-level stacked roundabout · whirlpool | a circulating carriageway | ❌ the parked **ring** family |

So the 4-leg ramp family is done (it's just the policy axis); the entire **3-leg family is locked behind one
missing thing: topology.**

### The skew finding — the ramp layer is ALREADY angle-agnostic (verified 2026-06-17)
Nothing in the spline drawers assumes 90°: `arc_spline` rounds with the *actual* deflection `fa=|b.dir−a.dir|`
and `T=R·tan(fa/2)`; `loop_spline`/`scurve_spline` are relational; `classify_turn` uses *relative* bearing.
The **only** thing hardcoded to perpendicular is the **stage** — `setup()`'s fixed `+`-cross port placement and
`draw()`'s axis-aligned road rectangles/centrelines/arrows. So an angled crossing is *free for the ramps*;
the work is only rotating a leg's ports (position + dir) and drawing its slab tilted. (This is the exact case
worldgen imposes — graph edges meet at arbitrary bearings — so skew is the rehearsal for the world port.)
Rough edges at *extreme* skew: acute corners shrink tangent room (`arc_spline` clamps R / falls back to
straight — graceful); the through/turn threshold (±30°) may want widening for very oblique "straights."

### The unifying refactor — the `Leg` layer (reflection #3, made real)
**Skew and topology are the same change.** Describe a junction as a **list of legs, each `{bearing, present}`**,
and lay the 8 ports out from that (`rebuild_ports()`). Then: vary a leg's **bearing → skew**; vary **presence /
count → topology** (trumpet / fork / triangle). This is reflection #3 (the `Leg`/`Road` layer) — and the
abstraction the world port needs anyway (legs arrive as graph edges with bearings), so it's the keystone, not
a detour.

### Decided sequence (2026-06-17)
1. **`Leg` layer + skew — DONE (2026-06-17).** `roadlab.c` now has `Leg{bearing,present,in,out}` + a static
   `legs[4]`; `rebuild_ports()` lays the 8 ports out from the legs (reproducing the perpendicular cross at
   `skew=0`), and `draw_arm()` strokes each road from its bearing. A `skew` stepper angles the trunk (legs
   2,3); the ramps **and** the generated junction re-solve with **zero changes to the spline code** —
   confirming the ramp layer was already angle-agnostic. (`leg_in/leg_out` = `L*2`/`L*2+1`; `make_junction`
   now skips absent legs, ready for topology.)
2. **Topology** on the same machinery → **trumpet / fork / triangle** (the 3-leg column). Trumpet additionally
   needs an **asymmetric** hard-turn assignment — *one loop + one semi-direct flyover*, not the uniform
   per-class policy (the **loop+loop vs loop+flyover trumpet variant** parked in the handoff). Fork/triangle
   are uniform (pure diverge / all flyover).
3. **Ring / roundabout family LAST** — a deliberate *peer* construct (a circulating carriageway the legs tap
   on/off), not the `diverge → [...] → merge` grammar; needs the parked `ring`/`circulate` primitive (→
   OpenDRIVE `junctionGroup type=roundabout`), shares nothing with the Leg refactor. See
   [`interchange-dsl.md`](interchange-dsl.md) "the ring family."

Sources: ASAM OpenDRIVE Specification v1.8.1 §12 (Junctions), §11 (Lanes) —
<https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/12_junctions/12_01_introduction.html>,
§12.4 Connecting roads. · Interchange taxonomy: <https://www.roads.org.uk/interchanges>.
