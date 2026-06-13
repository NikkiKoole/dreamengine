# procgen-places — varying the roads-&-cities generator (design seed)

**Status: v2 shipped — tile-based (2026-06-09).** The two-field matrix, road
classes, domain-warp, and pavements (v1) are now rendered **on a tile grid** (v2),
which dissolved the alignment/junction/seam bugs the vector renderer kept hitting.
v1.5 nice-to-haves (traffic lights, richer crossings, parking) and the arced-roads
wall remain open. See **"The tile-based rewrite"** below for the locked scheme.
Captures the design conversation that drove it (2026-06-09) about evolving the
`ROADS & CITIES` generator in
[`tools/carts/procplaces.c`](../../tools/carts/procplaces.c). The cart is the
procgen testbed: a free-fly explorer shell hosting pluggable generators
(`gens[]`), each exposing the same little vtable (draw / reseed / probe /
field_col). This doc concerns **generator 1 (roads & cities)** only; generator 2
(terrain) is fine as-is.

Related: [`sloop.md`](sloop.md) — the eventual consumer (its world is meant to
ride this road field; `road_at()` is the query seam).

## The problem

Today **one field does two jobs.** `roads_density(x,y)` returns a single 0..1
number, sliced into 5 tiers — CITY / TOWN / RURAL / HWY / SUPER. But those aren't
5 *kinds* of place; they're 5 *intensities* of the same place. A "city" is just a
"rural" with the knob turned up. That's why it reads samey, and why there's only
ever one road look (grey tarmac, varied only by width).

Two complaints, one root cause:
1. **Too few place types** — only an intensity ladder, no land-use variety.
2. **One road type** — all roads are `CLR_DARK_GREY`, differing only in lane width.
3. **Too grid-y** — a pure axis-aligned lattice; districts are blocky rectangles.

## The reframe — two orthogonal fields (SimCity 2000 model)

SimCity's trick: **what** a tile is (residential / commercial / industrial) is
independent of **how built-up** it is (light / medium / heavy). Two axes, not one
ladder. We get this almost for free — it's just a *second* noise lookup.
`noise3(x, y, seed + 50)` is a fully independent, repeatable field (the same
z-slice trick the cart's header comment already documents).

So: **land-use field × intensity field = a matrix of looks.**

### Axis 1 — land use (what). Four values: R / C / I + G.
| use | reads as | cheap visual signature |
|---|---|---|
| **Residential** | houses, yards | small roof rects, many empty lots (yards), warm roofs |
| **Commercial** | shops / downtown | bigger footprints, packed (no yards), grey/blue roofs, glass |
| **Industrial** | factories / depots | huge single footprints, few internal roads, dark + rust palette |
| **Green** | parks / farmland / wild | the current fields + trees; **this replaces the "nothing" tier honestly** |

(Water is deferred — it lives naturally in the *terrain* generator's elevation
idea; pulling it in here is scope creep. Start with 4.)

### Axis 2 — intensity (how much). Three values: light / medium / heavy.
This is the current density ladder **collapsed from 5 to 3**, and it now modulates
*block packing* rather than picking the whole identity:
- light → sparse, big lots, dirt / local roads only
- medium → filled blocks, avenues appear
- heavy → max packing, arterials cut through

The matrix then writes itself: *commercial × heavy* = downtown towers;
*residential × light* = leafy suburb; *industrial × medium* = a depot park. Same
two cheap noise reads, far more variety.

## Road types — decouple road look from zone

Today every road is grey tarmac; the only difference is width (`ZONE_LANE`). The
honest model: **a road's class is the lattice level it was promoted at, not the
zone it passes through.** A highway looks like a highway whether it cuts downtown
or countryside. The hierarchy already exists (nested pitch 1 / 2 / 6 / 12 / 24);
we just don't render the levels differently yet.

| class | promoted at | width | surface | markings |
|---|---|---|---|---|
| **dirt track / path** | finest, in green / light | 4px | brown | none |
| **local street** | finest, in built-up | thin | grey | none |
| **avenue** | mid pitch | medium | grey | white curbs |
| **arterial** | coarse | wide | grey | yellow dashes (current look) |
| **highway** | coarsest | very wide | dark | double line, no buildings front it, shoulders |

Same draw loop — branch on promotion level for color / width / markings instead of
one flat tarmac fill. Optional later: **rail** as a special line type (sleeper
ticks instead of dashes).

## "Less grid" — cheapest wins first

The perfect-nesting melt is what keeps the lattice seamless, so de-gridding needs
care. Ranked by payoff-per-effort:

1. **Domain-warp the field sampling** *(do first)* — sample density / land-use at
   `(x + noise·k, y + noise·k)` instead of raw `(x,y)`. Zone *boundaries* go wavy
   and organic; districts stop being blocky rectangles. Doesn't touch the road
   math, ~3 lines, huge payoff.
2. **Stochastically cull line segments** — hash each block, skip some promoted
   segments → cul-de-sacs, dead ends, irregular block sizes. Roads stop being
   infinite straight rulers.
3. **Jitter line positions** — offset each lattice line by a small per-line hash.
   Cheap, but risks breaking the nesting melt — apply only to the *finest* level
   so arterials stay aligned.
4. **Curved arterials / diagonal districts** — **don't** (see v2 below). Splines
   aren't a local function of position, so they fight the pure-function-per-cell
   model that makes the whole thing seedable and zoomable.

## The one wrong turn to avoid now

**Draw and `road_at()` must read the same geometry from shared helpers.** Today
both already call `roads_density()` / `roads_zone_of()` — good. The trap appears
the moment we de-grid: if a warp / cull / jitter lives only in the *draw* loop,
then collision (`road_at`, which sloop will call) disagrees with what's on screen —
the vehicle drives through a building that visually isn't there. **Rule:** any
warp / cull / jitter goes in a helper that *both* paths call. Set this up now
(cheap); retrofitting it later is not. With that rule held, all the sloop /
`road_at()` questions can be deferred safely — no other early decision locks us in.

## Scope / rungs

- **v1:** 2-field R / C / I + G matrix · road-class-by-hierarchy · domain-warp the
  field · **pavements** (free, and it sells the whole thing). Shared geometry
  helper so sloop stays safe.
- **v1.5:** parking, traffic lights, richer crossing rules (roundabout vs zebra
  vs lights), segment culling.
- **v2:** arced / diagonal / non-90° roads. The genuine structural wall — a cell
  can't know it's on a curve without non-local state (paths, splines, anchors),
  which breaks the field model. Probably a separate road-*network* pass laid over
  the field, not a tweak. Only if the field model proves insufficient.
  **→ being attacked in [`roadnet.md`](roadnet.md)** (2026-06-13): the wall is
  softer for a **node-lattice** spline (control points are pure functions of cell
  coords, so a curve stays local/seedable even though a *grown* spline wouldn't).
  roadnet is the separate network pass; this tile city becomes the *fill inside*
  its POI nodes.

### v1.5 detail — the "missing" list, triaged
Most of the wishlist is cheap and local (pure function of "what's at this cell"),
*not* v2:

| feature | difficulty | note |
|---|---|---|
| pavements / sidewalks | trivial | lighter strip inside the lane width; could even land in v1 |
| parking | easy | hash a commercial / industrial block → stripe rows |
| zebra crossings | done-ish | already drawn in CITY; extend the rule |
| traffic lights | easy | light boxes at promoted crossings, hash-gated; cycling = animation off the clock |
| roundabouts vs crossings | done-ish | already pick roundabout (TOWN) vs zebra (CITY); expand the decision |
| arced / non-90° roads | **hard** | the v2 wall above |

## The tile-based rewrite (shipped 2026-06-09)

The v1 vector renderer (rects at continuous world coords, promoted lattice *lines*)
kept producing alignment artefacts: per-block road segments that jogged across a
divider, pavement strips that rounded asymmetrically under fractional zoom, and a
colour seam where a dark highway met grey arterials so crossings looked
**unconnected**. These were whack-a-mole because the model fought the grid.

**Fix: render the roads generator on a fixed TILE GRID** (like generator 2 already
does — terrain draws 8px cells with zero alignment trouble). The valuable half — the
two-field zone logic (`roads_zone_at`, land use, intensity, domain-warp) — is reused
**verbatim**; only the *renderer* and *road model* changed. Each visible tile is
classified through the same helpers and drawn as one filled cell at tile-snapped
coords. Consequences, all good:

- **Crossings connect by construction** — a tile is road-or-not regardless of which
  axis claims it, and there's **one road surface colour** (no seam).
- **No floating strips** — sidewalks are whole tiles, centre lines fall on tile
  boundaries → the asymmetric-pavement bug can't recur.
- **Collision is trivial and identical to the render** — `road_at()` classifies the
  same tile through `roads_axis_v/h`, so sloop can never disagree with the screen.
  The wrong-turn rule above is now structural, not a discipline.
- **LOD built in** — zoomed out, draw bigger meta-cells; fine markings/edges only
  when truly close. That also keeps the frame cheap and parks the sub-pixel shimmer
  (STATUS open item 29) at the zoom where it'd show.

### The tile scheme — anchored to the car, and tunable

Anchored to sloop's rig so the two carts share one unit: **tile = sloop's `CELL` =
7px**, so "the car is 3–4 tiles wide" is literally true in this grid. A **lane = 5
tiles (35px)** — the widest planned car (4 tiles) fits with room to sit/drift. All
of it is `#define` knobs; off by a tile? change one line and roads, sidewalks,
centre lines, collision, and render all re-derive.

| unit | value | note |
|---|---|---|
| `TILE` | 7px | = sloop `CELL` |
| `LANE` | 5 tiles | widest car (4) + room |
| `SIDEWALK` | 1 tile | built-up, non-highway |
| `BLOCK_T` | 24 tiles | finest lattice pitch (local-street spacing) |
| `LOT_T` | 4 tiles | building-footprint grid |
| `ROAD_LANES[]` | 1 / 2 / 4 / 6 | local / avenue / arterial / highway |

**v1.5 polish shipped on the tile grid (2026-06-09):** centre-line dashes,
**zebra crossings** (white rungs on the approach tile just outside an avenue+
crossing), **traffic lights** (a lamp at each of the four corners of an avenue+
crossing, cycling red→amber→green on `now()` with a per-crossing phase), and
**parking lots** (some built-up COM/IND lots render as tarmac with bay lines
instead of a building). All are detail-gated (drawn only when zoomed in, where
they won't shimmer) and hash/class-gated for variety.

**Still open:** the arced-roads wall (unchanged — a separate network pass if
ever), and richer lane markings on the very wide arterials/highways.

## Decisions so far
- ✓ **Tile-based render**, tile = sloop `CELL` (7px), lane = 5 tiles — alignment,
  junctions, and collision all fall out of the grid; the field logic is reused.
- ✓ **Land use = R / C / I + G** (4 values; Green replaces the "nothing" tier).
- ✓ **One world**, not biome presets — panning suburb → downtown → industrial →
  countryside in one coherent field is the magic.
- ✓ **Road class = promotion level**, decoupled from zone.
- ⏸ **`road_at()` → sloop integration** deferred; the shared-helper rule is the
  only thing that must be respected meanwhile.
