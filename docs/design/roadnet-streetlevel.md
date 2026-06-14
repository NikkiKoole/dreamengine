# roadnet L3 — the on-the-street level (design seed, 2026-06-14)

**Status: design + harness only.** The deeper **"BLOCK" loupe** (a second, finer inset)
is in as the build harness — UI/placement provisional. The L3 *content* (residential
streets, footprints) is specified here but **not built yet**.

Related: [`roadnet.md`](roadnet.md) (L0–L2: the map + district street level),
[`roadnet-handoff.md`](roadnet-handoff.md) (where the work stands),
[`sloop.md`](sloop.md) (the car that will drive L3).

## The gap (why this level has to exist)

The district loupe (L2) draws arterials + the avenue/local grid + zones + Voronoi fields.
But inside a built-up block the lots are a **solid mass with no capillary streets** — the
interior houses have no road frontage; nobody could drive to them. A real network is
~five tiers:

> **highway → arterial → collector → local → access**

We have the **top three** (carved arterials/highways, avenues, core locals). The **access
tier** — the small residential streets and cul-de-sacs every house fronts onto — is
missing, so subdivision stops too early and houses clump.

And those finest streets + individual building footprints only *read* at a zoom deeper
than the district loupe. So this is simultaneously "**missing streets**" and "**missing a
zoom level**" — the same gap from two sides.

**Why now, before sloop:** L3 — a car on a residential street, beside individual building
footprints — is *exactly where sloop drives*. L3 isn't a detour before rung 4; it's the
level rung 4 needs. Wiring a car onto a district view where the houses have no streets
would be backwards.

## The LOD stack, made explicit

| level | view | shows | the query seam |
|---|---|---|---|
| **L0** | region map | terrain, cities, the arterial network | — |
| **L1/L2** | district loupe ("STREET") | arterials + avenue/local grid, zones, Voronoi fields, building lots | `road_at()` |
| **L3** | block loupe ("BLOCK") — *new* | **access streets** subdividing blocks, **individual footprints**, driveways, parking, the football field | `road_at()` (more tiers) + `building_at()` *(to build)* |

Generation is **zoom-independent** (every feature a pure function of world coords); only
**which tiers draw** is gated by zoom (LOD). So zoom changes what you *see*, never what
*exists* — determinism holds.

## L3 content (what fills the BLOCK loupe)

- **Access / residential streets** — a *finer lattice* (or organic tree of cul-de-sacs)
  that subdivides each L2 block, so every lot fronts a street. A new road class
  (`CL_ACCESS`) below `CL_STREET`.
- **Building footprints** — each lot → a real footprint rect (setback/yard + outline),
  fronting its street, with a driveway. Replaces the flat lot-colour square.
- **Parking, the football field, gardens** — block contents keyed to `(block, zone)`.

## The seams (the load-bearing rules — keep L3 local & deterministic)

This is the part that matters; get the seams right and L3 is an *extension*, not a rewrite.

1. **Recursive subdivision = pure-fn-of-block-coords AND connects to the parent.** An
   access street is defined *relative to its L2 block* — whose coords + orientation are
   already deterministic (`city_grid_coords`) — and must **meet the block's bounding
   local/avenue streets** (branch off a known parent edge / dead-end as a cul-de-sac),
   never float. Same locality contract as everything else: any chunk recomputes the
   identical streets from cell coords, no global pass.
2. **`road_at()` absorbs the access tier unchanged.** The finer streets just add lines to
   the grid query; `road_at` already returns `{on_road, cls, …}`. Add `CL_ACCESS`; sloop's
   call site doesn't change — it just sees more roads when it's at L3 scale.
3. **Footprints become their own collision seam — `building_at(wx,wy)`.** A pure function
   returning `{solid, lot id}`; the renderer draws the footprint, sloop reads the *same*
   rect for collision (the `road_at` pattern again — screen == collision). `road_at`'s
   existing `built` flag is the coarse precursor; `building_at` is the precise rect.
4. **Tier-by-zoom LOD is a DRAW gate, never a generation gate.** District loupe draws down
   to local; block loupe adds access + footprints. The functions that *produce* them run
   the same at any zoom; we just skip drawing sub-pixel tiers. (This is why the harness
   works: build the content, reveal it by zooming, with zero determinism risk.)

## The harness (how we build it — same playbook as L2)

The L2 street level was built by adding a **lens** first, then growing content into it.
Repeat: a second, deeper **BLOCK loupe** inset (centred on the same crosshair, zoomed a
few levels past the district loupe) is the canvas. Build the access streets, then
footprints, into that lens incrementally — bake, look, iterate. Eventually L3 stops being
a lens and becomes **sloop's camera** (a mode, not an inset) — but the lens is enough to
build against now, exactly as before.

*Current state:* the BLOCK loupe shows the **L2 content zoomed in** (lots become visible
as buildings-to-be) — the access streets + footprints are what we build into it next.

## Open decisions (for build time)

- **Street pattern:** clean grid subdivision (simplest, reads as suburb) vs an organic
  **cul-de-sac tree / loops-and-lollipops** (more real, more work). Lean grid for MVP.
- **Footprint shape:** rect MVP (inset + outline + driveway gap toward the fronting
  street); richer shapes later.
- **Irregular parent blocks** (near a curved arterial): how access streets terminate when
  the block edge isn't axis-aligned — probably clip to the parent road.
- **One deeper loupe vs the loupe gaining tiers as the mousewheel zooms it** — a UI
  question, deferred. (Today: a separate fixed BLOCK inset.)

## Next
1. Build the **access-street tier** into the BLOCK loupe (finer lattice, connects to parent).
2. **Footprints** + `building_at()` (the collision seam).
3. **Rung 4** — sloop at L3: the car drives the access streets, collides with footprints.
