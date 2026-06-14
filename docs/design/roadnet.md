# roadnet — a spline arterial network over the heightmap (design)

**Status: rungs 1–3 + bridges + magnifier + L2 zones + L2 block grid/lots done (2026-06-14).** A fresh testbed cart
([`tools/carts/roadnet.c`](../../tools/carts/roadnet.c)) for the one thing
[`procgen-places.md`](procgen-places.md) explicitly parked as **"the wall"**:
**arced / non-axis-aligned roads in a deterministic, infinite world.** This doc is
that wall's design conversation (2026-06-13).

**The goal (2026-06-14):** roadnet grows into **one app** — a showcase of abstract,
infinite, deterministic road/world generation that is *also* a **drop-in procedural
world** other games can ram into (sloop is the first consumer). So rather than keep
roadnet and the tile-city cart (`procplaces`) as two carts that talk, we **fold** the
useful pieces *into* roadnet — `procplaces` is the proven L2 reference (its `road_at()`
is the collision==render seam), but the street level is being (re)built natively in
roadnet's own world-tile coords so the whole LOD stack lives in one place.

Related:
- [`roadnet-handoff.md`](roadnet-handoff.md) — **★ the orientation doc: what's built, the
  LOD stack, and the next-time plan. Start there when picking roadnet up.**
- [`roadnet-streetlevel.md`](roadnet-streetlevel.md) — **L3, the on-the-street level**
  (the missing access-street tier + footprints, where sloop will drive). Design + the
  deeper "BLOCK" loupe harness.
- [`procgen-places.md`](procgen-places.md) → "Scope / rungs" v2 — the wall this
  cart attacks. Its tile-grid city model is *kept*; roadnet sits **above** it (see
  "Two scales" below).
- [`sloop.md`](sloop.md) — the eventual consumer (its rig drives this world).
- worldgen (`tools/carts/worldgen.c`) — the gtascii port roadnet borrows its
  heightmap / rivers / hashed-node / passable-LOS helpers from, verbatim.
- **trackgen** (`tools/carts/trackgen.c`) — the **finite single-track cousin**. It
  generates *one bounded closed circuit* globally (scatter → convex hull → push
  apart → midpoint-displace → **drivability relax** → cardinal-spline), then drives
  it with the steer.c car model. roadnet is the opposite topology — an *open,
  infinite, streaming network* — so trackgen's generation can't be reused (it's a
  global pass). But two pieces transfer directly: its **cardinal-spline through
  points** is the same family as our Catmull-Rom, and its **corner-relax** (clamp a
  turn too sharp to drive) *is* roadnet's rung-2 drivability clamp — lift that idea
  into `link_path()` when we get there.

## The wall, and why it's softer than it looked

procgen-places parked curved roads with a real reason: *"splines aren't a local
function of position, so they fight the pure-function-per-cell model that makes the
whole thing seedable and zoomable."*

That's true for a **grown** spline — an L-system or drunk-driver agent, whose shape
depends on where the path came *from* (non-local history). It is **not** true for a
**node-lattice** spline:

- A coarse grid of cells; each cell hashes to **at most one node** (a POI), at a
  jittered position. Node presence + position are **pure functions of the cell's
  integer coords** — worldgen's `city_at()` already does exactly this.
- A road between node **N** and a neighbour **M** is a curve whose control data
  (the two endpoints and their tangents) is **recomputed from cell coords**, not
  remembered. The tangent at N is a pure function of N's neighbour nodes; M's of
  M's. So **any chunk can independently recompute the identical curve** for any
  link in its neighbourhood — no global pass, no cross-chunk communication.
- The only cost over a flat field is a **wider neighbourhood read**: Catmull-Rom /
  Hermite tangents need neighbours-of-neighbours, so the draw loop scans a ~2-cell
  margin. worldgen already scans a 2-cell margin for its cities — same cost.

**worldgen already proves the hard half** (deterministic node placement +
forward-only pairing so each link draws once). roadnet's new work is "swap the
straight `thick_line` for a sampled curve" — plus keeping it seam-true.

### The locality contract (the thing that must not break)

> A road's geometry must be a **pure function of the integer coords of the cells it
> touches** — nothing else. No accumulated state, no draw-order dependence, no
> "what chunk am I being viewed from." Hold this and infinite + deterministic +
> zoomable all come free; break it and roads tear at chunk borders.

## Two scales — the network stacks on top of the tile city

These two models stop competing and become a hierarchy (the "Main Rib Strategy"):

| layer | what | model | where |
|---|---|---|---|
| **arterial network** | long curvy roads *between* places | **roadnet** — spline over the POI node lattice, world-space | this cart |
| **local streets** | the dense grid *inside* a place | procplaces' two-field tile model | reused, scoped to a POI footprint |

A **POI is simultaneously a network node and a procplaces city site**: arterials are
the skeleton, the tile city is the flesh that fills a node where the arterials
arrive. The two carts converge at rung 4.

## The node → tangent → link scheme (rung 1)

Per node **N** at world pos `P_N`:

- **presence + position** — `node_at(cx,cy)`: `hash2(cx,cy)` gates presence (~a
  fraction of cells), jitters within the cell, rejects nodes off habitable land
  (height outside the passable band). Pure function of `(cx,cy)`.
- **tangent** `T_N` — sum of `(P_neighbour − P_N)` over the **present** neighbour
  nodes in the 8 surrounding cells, scaled by a handle factor. Symmetric and pure,
  so it's identical from either endpoint's chunk. (Imperfection accepted for rung
  1: the tangent counts neighbours even where the link is later dropped over water;
  it leans slightly toward a dead direction. Seam-safe, and invisible at overview
  zoom — revisit if it shows.)

Per link **N → M** (drawn **forward-only** — only when M's cell sorts after N's, so
each link draws once):

- `link_path(N, M, out[])` — samples a **cubic Hermite** from `P_N` (tangent `T_N`)
  to `P_M` (tangent `T_M`) into ~12 points; the renderer strokes a thick ribbon
  through them. **This one function is the seam** (see below).
- the link is **dropped** if the straight LOS between the nodes isn't passable
  (worldgen's `road_clear` — water / peak in the way). Cheap honest terrain
  reaction for now; routing *around* the obstacle is rung 2.

## Connected by hierarchy (rung 1.5 — done)

A **flat** node lattice fragments: a link needs *both* adjacent cells to hold a node
(0.4 × 0.4 ≈ 0.16) and then passable land between — a random graph below its
percolation threshold, so it reads as disconnected clumps, not a road *network*.
Real road networks are connected. The fix is the **road hierarchy** (the pasted
research's "Main Rib"), and it stays fully local/deterministic:

- **Highways** — `get_hub()`: hubs on a **coarse** lattice (`HUB_CS`), ~85% dense,
  each linked to its neighbour hubs (same forward-only Catmull-Rom). A lattice graph
  is **connected by construction**, so this spine spans the world — broken only
  where water/peak fails the passable-LOS (honest; **v2 bridges** reconnect it).
  **Diagonals are gated per-quad** (the `diag` slider): linking *both* diagonals of
  every 2×2 quad makes a "World-of-Goo" X through each square, which reads as noise.
  So each quad hashes its corner to pick *at most one* diagonal (or none), gated by
  the slider — clean orthogonal grid at 0, occasional shortcuts higher, never an X.
- **Minor roads** — every town (`get_node`) links to its **nearest hub** (a feeder
  onto the spine, `nearest_hub`, 5×5 hub-cell search) **and** to its **nearest other
  town** (`nearest_town`, a local road). Local roads chain clusters together so a
  town far from any hub still connects via town→town→…→hub — *not everything needs a
  highway*. Degree ~1 each → no criss-cross. Mutual-nearest pairs double-draw
  harmlessly (identical pixels). Some terrain-locked islands remain — honest, and
  what v2 bridges/ferries fix.
- The *promiscuous* town↔town lattice (link every forward neighbour) is gone — that
  was the original criss-cross; "nearest one" is the controlled replacement.

Both tiers are the same pure-function-of-cell-coords spline, so seam-safety holds.
Cell sizes are sized to the view (~`HUB_CS` 20 tiles, `NODE_CS` 10) so several hubs
fit the ~80-tile-wide screen — a coarser spine than the screen reads as empty.

## Intro / setup panel (done)

The cart opens on a `ui.h` setup panel: seven sliders (hub gap, town gap, hub/town
density, wiggle, diag, water) + ROLL + EXPLORE, over the **live full-screen world** — no
separate preview renderer, because terrain and roads are pure functions of the
params, so dragging a slider re-rolls the world behind the panel instantly. The
params are `P_*` floats in 0..1; getters map each to its real range and the
`NODE_CS`/`HUB_CS` macros call the getters, so every call site stayed unchanged.
Only the **water** slider touches the terrain cache (added to its key); cell-size /
density / wiggle only affect the per-frame road draw, so they're free to live-edit.

**Mousewheel zoom** (ZMIN 0.12 .. ZMAX 2.5, pivot = screen centre): tile-pixel size
is `P = TILE·zoom`; all screen transforms use `P`. Terrain is **sampled per screen
cell** — `height_at` at the world coord under each 2px block — *not* cached in a
per-tile buffer, so cost is fixed at any zoom and there's **no zoom-out ceiling**
(an earlier `colgrid` buffer capped it). **LOD:** below zoom 0.45 the town/feeder
tier is dropped (highway skeleton only) and hub markers shrink (3→1px), so a
continental view reads as a road network rather than a polka-dot field. `vcols/vrows`
(the visible world-tile span) is still recomputed per frame for the road-loop bounds.
**Left-drag pans** ("grab the map" — screen-px delta ÷ P → world tiles); in the setup
panel it's gated to `mouse_x > PANEL_W` so it doesn't fight the sliders.

## The magnifier — a window into the level below (the L2 harness)

**The map is L0; the game is L2.** roadnet draws the region map (highways, cities as
marks) but a GTA-style game is played at **street scale** — interior streets, zoned
blocks, parks, parking, the football field. That detail can't live at map resolution
(millions of streets, pointless to compute zoomed out), so it's a **separate, finer
generation anchored to the map data**: the big roads are the *same* data at a deeper
zoom (aligned by construction), and the street level is *new detail grown onto* that
skeleton — deterministic, so the streets always attach at the same bones.

To build that street level incrementally we don't need the full map↔street mode
transition yet — we need a **lens**. `draw_loupe()` (toggle **L**) is an inset that
re-renders the world at the screen-centre point a few zoom levels deeper (`LOUPE_ZOOM`)
*with the street level layered in*: it temporarily swaps the camera transform to the
deep zoom centred in the box, `clip()`s to the box, and composes
`render_terrain()` → `render_streetlevel()` → `render_roads(1)`, then restores the map
transform. So you see terrain + the **same highways** (aligned) + the interior
streets/zones the map is too coarse to show.

## L2 zone model (done — the RCI layer, anchored to roadnet)

`render_streetlevel()` now generates the real **zone field** — the SimCity RCI idea,
but with the intensity axis *anchored to roadnet's cities* instead of a free noise
(the convergence). Two axes, both pure functions of world coords:

- **Urbanity `U(p)` ∈ 0..1 — "how built-up".** A field of bumps:
  `U = max over nearby city nodes of weight(rank)·falloff(dist/radius(rank))`. Highest
  downtown, fading to countryside at the edge; metros are tall/wide bumps, hamlets
  small ones; overlapping cities merge into a conurbation (`max`). Nearby nodes are
  gathered once per loupe frame (`gather_cities`) so `urbanity()` is a short loop.
- **Land use from `U`, concentric + jittered** (`zone_of`): wild (terrain shows) →
  **FARM** (cultivated ring, `U_FARM..U_LIGHT`) → **RES** (suburb) → **COM** (downtown
  core, `U_COM`), with **IND** on the fringe (low-`U` band) gated by an industrial
  noise, becoming **HARBOR** where water is within ~2 tiles (`water_near`), and **PARK**
  pockets carved from any built-up zone by a high-freq noise. Built-up zones get an
  interior street grid (`STREET_SP`) cut in. Colours in `ZONE_COL`.

It's drawn in the *same world coords* as the map, so it aligns with the arterials
running through it (pan the crosshair across a city → downtown → suburb → farm in the
lens). To avoid a bullseye, `urbanity()` **domain-warps** the query point (every band
becomes an organic blob, not a circle) and farmland is **patchy** (a noise leaves
countryside gaps via `Z_NONE`) rather than a solid ring. **Decisions:** concentric
gradient (the patchy "mix" knob is a later add); industrial fringe + water harbours;
farm patches between suburb and wild.

**Live knobs:** the setup panel has a **CITY (loupe)** group of seven L2 sliders —
*city size* (`citysize_mul`, urbanity radius), *downtown* (`u_com`, core size), *farms*
(`farm_frac`, fraction of fields cultivated vs fallow), *blocks* (`block_sp`, block size),
*align* (`P_align`, artery-vs-axis district blend), *road w* (`road_w_mul`, arterial carve
width — so *blocks* + *road w* set the **highway-vs-block ratio**), and *field* (`field_sp`,
Voronoi farm-field size). All `P_*` floats read live, so the loupe re-renders as you drag.
(Lower-bang knobs — industry/park density, warp strength, the `U_CORE` gradient handoff —
left hardcoded.)

## L2 block contents — street grid + building lots (done — 2026-06-14)

The zone tint is now filled with **real content**. A built-up zone grows a **nested
street lattice** that carves it into **blocks**, and each block fills with **building
lots**. All in roadnet's own world-tile coords (a *sub-tile* pitch, so it reads as
blocks in the loupe), pure functions of world coords → seam-true, and the future
`road_at()` will read the identical geometry.

- **`street_axis(coord, U)` / `on_street(wx,wy,U)`** — line `k` sits at `k·block_sp()`
  world tiles; its class is a pure fn of `k` (`k%4==0` → **avenue**, else **local**),
  its road half-width follows the class. **Visibility is gated by urbanity**, so block
  density *emerges*: avenues appear wherever it's built (`U ≥ U_LIGHT`), locals only in
  the dense core (`U ≥ U_MED`) — downtown gets the full grid, the suburb only its
  avenues, the industrial fringe big open blocks. No flag; it falls out of `U`.
- **`lot_color(wx,wy,z)`** — a block interior is a sub-block hash grid of lots; each lot
  hashes to a roof colour from its zone's palette (RES warm roofs + 1-in-8 yards, COM
  concrete/glass, IND/HARBOR dark bodies).
- **`block_sp()`** — the **blocks** panel slider (`P_blocks`) now sets the block pitch in
  world tiles (0.7..2.3), live in the loupe.

### Aligned to the arterials (done — 2026-06-14)

The block grid no longer sits on a single global axis with arterials cutting across it
at random angles. Each city's grid is now **oriented + anchored to the road that enters
it**, so main street follows the highway and the grid radiates from downtown:

- **`city_dir(node)`** — the entering road's direction: a **town** points toward its
  nearest hub (its feeder, via `nearest_hub`); a **hub** toward its nearest neighbour
  hub (the highway). A unit vector, pure fn of position → deterministic + seam-true.
  Stored per gathered city (`gux/guy`).
- **`urbanity()` records the dominant city** (`dom_i` = the strongest bump at the query
  point). `render_streetlevel` transforms each built-up point into that city's **local
  rotated frame** (origin = node, axes = `city_dir`) before running the grid — so the
  lattice rotates to the road and a downtown crossing sits on the node.

Adjacent cities get different orientations (realistic); they only meet across
low-urbanity farmland, so there's no visible grid seam in practice. (A conurbation where
two high-`U` bumps overlap *can* show a watershed line where `dom_i` flips — acceptable
for now, revisit if it shows.)

**A city is a PATCHWORK, not one monolithic grid (the `align` slider).** Real cities
don't rotate *wholesale* to the one highway they hang off — some districts follow the
road in, others just grid north, and the bigger roads connect however. So orientation is
decided **per district**, not per city: `city_grid_coords()` cuts the city-local plane
into coarse `DISTRICT_BLK`-sized district cells, each hashing to "aligned" or "axis". The
**`align` slider** (`P_align`, the 5th CITY knob) sets the *proportion* that follow the
artery — **1** = whole city aligned, **0** = all world-axis, between = the realistic mix.
Both frames are node-anchored so it stays one coherent city; an aligned district abutting
an axis one shows a grid-shear line (the real thing, e.g. an old core meeting a newer
grid). Still a pure hash of the district cell → deterministic + seam-true.

### `road_at()` + arterials carved through (done — 2026-06-14)

The keystone: the street level is now **queryable**, and the arterials **carve through**
the blocks as real road surface instead of being painted on top. The renderer and the
query are *literally the same function* — `render_streetlevel` calls `road_at()` per cell
— so screen == collision by construction (the locality contract, now structural).

- **`gather_arterials()`** — once per loupe frame, enumerate the visible links (mirroring
  `draw_highways`/`draw_feeders`) and push each link's `link_path()` polyline + class into
  a **segment cache** (`sg*`/`nseg`). The geometry is the *same* `link_path()` the renderer
  strokes → the cache agrees with the drawn road by construction. *(The enumeration is
  duplicated from the draw loops — the one spot to keep in sync; the geometry is shared.)*
- **`arterial_at(wx,wy)`** — point-to-segment distance (`seg_dist2`) over the cache vs a
  per-class **world** half-width (`road_hw`: motorway 0.75 … dirt 0.22 tiles). Returns the
  widest (lowest-idx) class the point is inside.
- **`road_at(wx,wy) → {on_road, cls, zone, urb}`** — THE world query seam the drop-in
  world exposes (sloop calls this). Combines `arterial_at` + the local street grid
  (`on_street` in the district-aligned frame) + `zone_of`. Assumes `gather_cities()` +
  `gather_arterials()` ran for the region this frame (the renderer does it; a consumer
  gathers around its own position).

So arterials cut through the city as dark road surface, the local grid meets them, and a
highway shows as a road even crossing open farmland. Building lots only appear where
`road_at` reports no road.

**Class-aware at street scale (done — 2026-06-14).** In the loupe the **carve now owns
the road surface** (the map-style spline stroke is dropped there; only the city markers,
`draw_nodes`, draw on top), so roads read at their true street-scale width and can be
styled by class:
- **dirt = brown, paved = dark grey** (surface by `road_at`'s `cls`).
- **centre-line on motorway/highway/arterial** — `arterial_at` returns the distance to the
  road's centre-line (`coff`); a thin yellow line draws where `coff` is tiny. Local streets
  get none — that's the class read.
- **sidewalks** — `grid_at` now returns road / **sidewalk** / lot, so built-up local
  streets get a light kerb strip beside them.
(Bridges over water are the one gap: `road_at` doesn't pave water and the loupe no longer
strokes the spline, so an in-city bridge currently shows as a break — rare, fix later by
carving the bridge span.)

### Outskirts gradient + Voronoi fields (done — 2026-06-14)

The old city→country edge was a hard **blob**: RES filled uniformly to `U_LIGHT` then
became a flat patchy-yellow farm *ring*. Replaced with a gradient (the "middle path":
keep the concentric core, gradient only the outskirts):

- **Occupancy gradient** — `occupancy(U, road_dist)`: above `U_CORE` the suburb is fully
  built (the dense core, unchanged); below it, lot occupancy ramps down so the suburb
  **thins toward the edge**. `lot_built()` hashes each lot against `occupancy` → a building
  or open land. COM/IND stay dense (the core reads as a city); only RES/FARM thin.
- **Ribbon development** — `occupancy` gets a boost within `RIBBON_REACH` of an arterial
  (`road_dist` over the segment cache), so **houses string along the roads out of town**
  while the land behind them opens to fields. The single biggest "real town edge" cue.
- **Voronoi fields** — unbuilt cultivable land becomes discrete parcels. `field_at()` is a
  jittered-grid (Worley) partition: each field-cell hashes to one seed, the nearest seed's
  cell is the field (its hash → crop: wheat/pasture/plough/fallow), and where the two
  nearest seeds are ~equidistant we're on a **hedgerow** (`F2−F1` test). `field` slider =
  field size; `farms` slider (now `farm_frac`) = fraction cultivated vs fallow meadow.

So it now reads: downtown → dense suburb → thinning houses among fields, denser along the
roads → open countryside of hedged fields → wild. `road_at` gained a **`built`** flag (a
building stands here vs open field) — the seam sloop needs for building collision.
`U_CORE` (the concentric→gradient handoff) is hardcoded for now; easy to promote to a slider.

**Next:** park *contents* (the football field) as real block content (PARK still flat
tint); building *footprints* drawn as collidable rects (not just colour, using `built`);
then landmarks (gas/hospital/gun shop) as point POIs, and finally **sloop's car at street
scale (rung 4) — wire its driving into `road_at()`**, now that the seam exists.

## Rungs

1. **Spline seam + connectivity** *(done — see above)* — hub/town/feeder hierarchy,
   forward-only Catmull-Rom ribbons, passable-drop. Heightmap bands underneath
   (worldgen). Free-fly pan camera (testbed, not driving — sloop owns driving).
   **Milestone = the seam test:** pan across a cell boundary (toggle G for the
   border overlay) and the *same* curve draws from either side — held by
   construction (curve points are world-space, camera applied last). The wall is
   down.
2. **Terrain-aware routing** *(done)* — the real `link_path()` seam now exists: it
   samples the base Catmull-Rom plus a **perpendicular bow that bends the road around
   impassable terrain**, and **verifies every sample sits on passable land** before
   returning (so a road is never drawn over water/peak — strictly better than rung 1,
   which checked only the straight line then drew a curve that could clip water). The
   bend grows until the whole path clears. **Bridges (rung 2.5):** before bending,
   `link_path()` checks the blockage — a short **water** gap (≤ `MAX_BRIDGE` tiles, a
   river/strait, no peak in it) is **bridged** instead: the straight curve is kept and
   its over-water samples are tagged (`lp_br`) so the renderer draws a distinct deck;
   only a peak or *wide* water falls through to the land-detour bend, and only a fully
   boxed-in link drops. Priority straight → bridge → bend → drop. One function,
   world-space samples → render *and* future `road_at()` read the identical geometry.
   *(Drivability corner-clamp — trackgen's relax — folds into the same function when
   sloop starts driving; deferred till then, it's meaningless without a car.)*
3. **Rank + road class** *(done)* — each node hashes to a **rank** (`hub_rank` →
   city/metro, `town_rank` → town/hamlet; high ranks rare, so cities feel spaced out
   without a Poisson pass — rarity ≈ spacing). A link's **class** is then a pure
   `f(endpoint ranks)` — motorway / highway / arterial / street / dirt (`RS[]` style
   table: radius, casing+centre colour, centre-line flag). Both endpoints compute the
   same class → still seam-true. `draw_link()` strokes casing (phase 0) then centre +
   a dashed yellow **centre-line** on big roads (phase 1, only when zoomed in). Node
   markers size by rank (metro > city > town > hamlet). **Landmarks (gas / hospital /
   gun shop) deferred to rung 4** — they're gameplay destinations, not map character,
   so they want sloop's context.
   - **Spacing** *(done)* — **priority suppression**: each town candidate (density gate
     only, so it's pure-hash and cheap) is dropped if a higher-priority town candidate
     sits within R=1 cell; priority = rank-then-hash, so towns absorb adjacent hamlets
     and the survivors are blue-noise spaced. Metros de-cluster the same way but by
     **demotion** (a metro near a stronger metro becomes a city) so the backbone keeps
     all its nodes and stays connected. Both pure functions of cell coords → seam-true.
   - **Valley-following** *(done)* — a clear link bows toward the lower of its two sides
     at the midpoint (`VALLEY_*`), so roads sag into valleys / hug coasts instead of
     climbing; reverts to straight if the bias would run into water/peak. Lives in
     `link_path()`, world-space, so render and future `road_at()` agree.
4. **Fill the node** — drop procplaces' tile city into a POI footprint where the
   arterials enter. The two carts meet; `road_at()` becomes the shared query seam
   for sloop.
5. **(v2 terrain features — see seams below.)**

## v2 seams — documented, planned, NOT built (per this session's decision)

The pasted GTA-procgen research went deep on road↔terrain interaction. Most of it
is **parked** — some of it may never fit a 2D top-down pixel view — but rung 1
leaves the **hooks** so v2 is an extension, not a rewrite:

| v2 feature | the seam left for it | rule |
|---|---|---|
| **obstacle avoidance** | ✅ *done in rung 2* — `link_path()` bows the curve around impassable terrain and verifies passability | callers (render + future `road_at()`) never change — **one path function, both paths call it** (procgen-places' "one wrong turn to avoid") |
| **valley-following** (roads *prefer* low/flat ground, not just avoid water) | ✅ *done (rung 3)* — clear links bow toward the lower midpoint side (`VALLEY_*`) inside `link_path()` | extends the bend logic, doesn't change the seam |
| **bridges** | ✅ *done (rung 2.5)* — a short water gap (≤ `MAX_BRIDGE`) is bridged: over-water samples tagged in `lp_br`, drawn as a distinct deck | a link over a narrow river crosses instead of dropping; wide open water still drops (ferries/long bridges much later) |
| **tunnels / carving / embankments / Z-slope** | same per-sample height tag | **deferred hard** — likely doesn't map to top-down 2D; do not let it shape rung-1/2 structure |
| **sloop collision (`road_at`)** | sloop calls `link_path()` for the *same* geometry it sees | the locality contract guarantees screen == collision |

**The single rule that keeps all of this open:** never let a warp / reroute / drop
live only in the draw loop. Geometry comes out of `link_path()`; both render and
collision read it. Set up cheap now (rung 1), painful to retrofit later.

## Decisions
- ✓ **Fresh cart** (not an evolve of worldgen/procplaces) — worldgen stays a clean
  gtascii port; procplaces stays the tile-city model; roadnet is the network layer
  that will later *consume* both.
- ✓ **Node-lattice spline**, not grown/agent splines — preserves the locality
  contract (the whole reason the wall looked unclimbable).
- ✓ **Cheap terrain reaction first** (drop non-passable links); then rung 2 bends
  around terrain + bridges short water; valley-following / tunnels stay v2.
- ✓ **Rank → road class** (rung 3) — rarity for spacing, class = `f(endpoint ranks)`.
- ⏸ **landmarks, tile-city fill, sloop `road_at()`** — rung 4 (the drive-it leap).
