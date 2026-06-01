# Rasterization consistency — one clean way to fill/outline a shape

> **Genre: design exploration (scratchpad) — OPEN, not solved.** This captures a
> recurring class of pixel bug, the point-fixes we've shipped so far, and the real goal
> we have **not** reached yet: *one* consistent way to rasterize a shape so its fill,
> outline, dither, solid and adjacent neighbours all agree pixel-for-pixel.
> - State of individual fixes → [`../STATUS.md`](../STATUS.md).
> - Per-pixel precedent for baked shapes → [`baked-rotation-atlas.md`](baked-rotation-atlas.md).
> - `pal()` sprite recolor (a related "one definition" win) → [`../decisions/0007-pal-recolors-sprites.md`](../decisions/0007-pal-recolors-sprites.md).

## The symptom (it kept coming back)

Across several carts the same kind of artifact showed up at shape **edges**:

- **arcs cart** — `arcfill`/`ring` (old triangle-fan) left 1px **seam cracks** between
  fan triangles, and the rim **overshot** so a stroked `arc`/`circ` outline didn't line up.
- **solid3d** — dithered faces showed **black hairlines** between adjacent triangles.
- **katamari** — a dithered circle's **outline didn't hug the fill** (the `circ` ring
  traced a different circle than the dithered `circfill`), and a "draw a smaller dither
  circle on top of a solid one" attempt left **rim artifacts** because the two circles
  had different edges.

Different carts, different primitives — **same root cause.**

## The root cause: the engine has *several* definitions of the same shape

A "circle of radius r centred at (x,y)" is currently rasterized **three different ways**,
and they do not produce the same set of pixels:

| Definition | Used by | Edge rule |
|---|---|---|
| **raylib GPU** | `circfill` (solid → `DrawCircle`), `circ` (`DrawCircleLines`), solid `trifill` (`DrawTriangle`) | GPU coverage (pixel-center, top-left fill rule) |
| **CPU scanline `*_pat`** | any fill while `fillp` is on — `ovalfill_pat`, `rectfill_pat`, `trifill_pat` | per-row span, integer rounding |
| **per-pixel** | `arcfill`/`ring`/`arc` (the new ones) — `sector_fill` | pixel-center distance test `(x+0.5-cx)²+… ≤ r²` |

When two of these meet at an edge they disagree by up to a pixel:
- **fill vs outline:** `circ` (GPU lines) over a dithered `circfill` (CPU scanline) → the
  ring sits 1px off the fill.
- **solid vs dither:** a GPU `trifill` next to a `trifill_pat` dither face → the CPU side
  truncates short of the shared edge → background shows (black crack).
- **fill vs fill:** two scanline triangles meeting on a diagonal → rounding gap.

So every "edge artifact" is really *"which rasterizer drew this pixel, and did the
neighbour use the same one?"*

## Point-fixes shipped so far (each forced agreement locally)

These all work, but each is a *local* patch, not the single rule:

1. **`arcfill`/`ring` → exact per-pixel**, measured from **pixel centres** (`x+0.5-cx`) so
   the disc matches the `circ`/`circfill` convention; **`arc` is a 1px ring of that same
   circle**, so fill + outline come from one definition. *(commit: arc rasterization fix.)*
2. **`trifill_pat` conservative span** — floor the left end, ceil the right, so adjacent
   dithered triangles **overlap by ≤1px instead of gapping**. Fixes solid3d's black lines.
   Only the `fillp` CPU path; solid GPU `trifill` untouched.
3. **katamari** — (a) one **single-shape** dithered fill (base colour on the 0-bits, a
   darker shade of itself on the 1-bits) instead of two stacked circles; (b) the "too big"
   marker is a slightly larger **solid backing** drawn *behind* the fill, so a solid `s+1`
   shape always contains the `s` fill (any rasterizer) and leaves a clean 1px ring.

Pattern across all three: **make the edge come from a single definition** — per-pixel,
or conservative-overlap, or "one shape, not two."

## The goal we haven't reached

> **One rasterization path that fill, outline, dither and solid all share**, so that:
> - an outline always exactly traces its fill (`circ` hugs `circfill`, for any radius),
> - adjacent shapes never crack and never overlap-bleed,
> - a dithered fill never specks past where the solid fill would be,
> - it behaves identically on desktop GL and web GL ES.

Today the per-pixel `sector_fill` (arcs) is the closest thing to a "true" definition, but
`circ`/`circfill`/`oval*`/`tri*` still go through raylib GPU (fast, but its own rule) and
`fillp` fills go through the CPU `*_pat` scanlines (a third rule).

## Candidate directions (to evaluate, not yet chosen)

1. **Unify on per-pixel CPU coverage** (extend the `sector_fill` idea to circle/oval/tri):
   one function decides "is pixel (x,y) inside this shape?", and **solid fill, dithered
   fill, and outline (the boundary ring) all read from it.** Outlines become "inside AND a
   4-neighbour is outside." Guarantees total agreement.
   - *Cost:* CPU per-pixel for every shape (today solids are GPU-cheap). Need to measure —
     fantasy-console shapes are small, but a cart spamming big fills could feel it.
2. **Keep raylib GPU as the truth; make the CPU/dither paths match it.** Tune `*_pat`
   rounding (and `pset`/pixel-center offsets) so a scanline fill covers exactly the GPU
   pixel set, and derive outlines from the GPU fill. *Cheaper, but "match raylib's exact
   coverage" is brittle and differs GL vs GL ES.*
3. **Two-layer convention, documented:** keep GPU solids + GPU outlines as a matched pair
   (they already mostly agree), and decree that **dithered fills must be drawn as a single
   shape with two palette colours** (never dither-then-outline, never stack two shapes).
   Least code; pushes the discipline onto cart authors (and this doc).

## Sub-problems any solution must nail

- **Pixel-centre vs corner.** The off-by-one on the half-dome was measuring distance from
  the pixel *corner* (`x-cx`) instead of its *centre* (`x+0.5-cx`). Whatever path wins must
  fix the centre convention once, everywhere.
- **Outline = boundary of the fill**, derived from the same coverage — not a separately
  rasterized stroke. (This is why `arc` became a ring of `sector_fill`.)
- **Adjacency / shared edges.** Either watertight (shared-vertex GPU) or conservative
  overlap (the `trifill_pat` floor/ceil) — never the truncate-both that gaps.
- **Thin shapes / silhouettes.** Conservative overlap adds ≤1px on outer silhouettes;
  decide whether that halo is acceptable (it is for solid3d) or needs trimming.
- **`fillp` lattice alignment** under `camera()` (screen- vs world-aligned) — keep it
  consistent so dither doesn't shimmer when scrolling.

## Shipped (2026-06-01, session 11)

`circ`/`circfill` and `oval`/`ovalfill` unified on a single pixel-center coverage test:
`disc_inside(x,y,cx,cy,r)` = `(x+0.5-cx)²+(y+0.5-cy)² ≤ r²` and the ellipse equivalent.
Outline = boundary ring of the fill (pixels inside with ≥1 outside 4-neighbour) — never
a pixel outside the fill. `plot_pat` handles both solid and dithered fills in one loop,
so dither and solid are the same path.

**Regression test:** `tools/carts/raster_test.c` — SPACE to freeze + analyse, Z/X to
toggle outline/dither. Harness: `tools/raster_test.script`. All meaningful states
(solid+outline, dithered+outline) report 0 mismatches.

## Shipped (2026-06-01, session 12)

`rrect`/`rrectfill` unified on a single pixel-center coverage test, `rrect_inside(px,py,
x,y,w,h,r)` = within `r` of the inner rect of corner centres (straight edges → distance 0
→ inside; corners reduce to `disc_inside` against the nearest corner centre). `rrectfill`
plots every inside pixel via `plot_pat` (solid + dither one path); `rrect` is the boundary
ring (inside with ≥1 outside 4-neighbour). This **removes the stacked-shape anti-pattern**
the old `rrectfill` used (3 rectfills + 4 circfills) and the old `rrect`'s lines-meet-arcs
seam — fill, outline and dither now come from one definition. Extended `raster_test.c` to
cover 6 rrects (varied radii); all four states report 0 mismatches.

**Harness fix (same session):** the freeze/analyse rework (carts d8c2d18/777a784) had left
`tools/raster_test.script` stale — it never pressed SPACE, so analysis never ran and the
trace was empty despite the script comment saying "read mismatches from the trace." The
cart now `watch()`es `mismatches` + `state` under `#ifdef DE_TRACE`, and the script drives
SPACE → Z → X → Z through all four states; counts land on frames 3/7/11/15.

> **Detector sensitivity (measured):** the marker test flags a fill-edge with *no adjacent
> outline* (yellow) or an outline with *no adjacent fill* (pink). A **1px** outline/fill
> misalignment keeps everything 4-adjacent and so reads as 0 mismatches; a **≥2px** gap is
> caught (verified: a +2px outline offset → 657 mismatches). So "0 mismatches" means
> "agree to within 1px," not "bit-identical." Good enough for the seam-crack class of bug;
> note it if a sub-pixel guarantee is ever needed.

## Shipped (2026-06-01, session 14)

The whole polygon family unified on **one even-odd point-in-polygon coverage** —
`poly_inside(fx,fy,xy,n)` measured from the pixel centre, same convention as the
disc/rrect tests. `poly_fill_cov` plots every inside pixel (`plot_pat` → solid + dither
one path); `poly_stroke_cov` is the boundary ring (inside with ≥1 outside 4-neighbour).
Rewritten onto it: `ngon`/`ngonfill`, `star`/`starfill`, `poly`/`polyfill`, **and
`tri`/`trifill`** (a triangle is just a 3-vertex polygon). Wins:
- outline is exactly the boundary of the fill for every one of them — no more GPU
  line-vs-fill drift (the "triangles differ slightly" bug);
- even-odd handles **concave** `poly` and the star's reflex points (the old fan filled
  them wrong); winding-independent, so 3D back/front faces both fill;
- `trifill_pat` (the CPU dither-scanline special case) deleted — `plot_pat` covers dither.

`trifill` is now CPU per-pixel (the 3D carts — `solid3d`/`cube3d`/`flyover` via `quadfill`
— go through it). Smoke-tested: `solid3d` renders filled+dithered with no face cracks,
no hang. **Perf vs the old GPU `DrawTriangle` is not yet measured** — deferred by choice
("get it correct first"); revisit if a triangle-spamming cart feels slow, and clamp the
fill bbox to the screen if huge off-screen tris ever bite.

**Detector rewritten (same session).** The old marker test was a local adjacency guess and
had two failure modes: it tolerated ≤1px drift, and once the pink rule was relaxed to stop
false-flagging sharp tips it went blind to connected offsets. Replaced with a **global
invariant**: reconstruct the fill region from the single render (`fill ∪ outline`) and
require the outline to be *exactly its boundary* — a FILL pixel that itself touches
background = an uncovered edge (yellow); an OUTLINE pixel with no background neighbour =
buried inside the fill (pink). This catches a 1px offset at any angle yet never flags a
correct tip (a tip pixel is on the boundary). Verified it now catches what it used to miss
(GPU triangles → 282 mismatches before conversion; 0 after).

> **Residual blind spot (known):** a single combined render still can't distinguish a
> uniformly-**1px-proud** outline (drawn just outside the fill but still touching it) from
> a correct boundary — both read as a boundary pixel with fill on one side, bg on the other.
> Closing that needs a **two-pass** test (render fill-only → set F, outline-only → set O,
> assert O == boundary(F)). Not needed today: every primitive is now coverage-based, so the
> outline *is* boundary(F) by construction and the bug class can't arise. The two-pass test
> is the upgrade if a future GPU/stroke primitive is reintroduced.

**Still open:** `thickline` (a filled stroke with no outline pair — different shape of
problem: verify its silhouette has no stray pixels and self-overlaps cleanly, rather than
outline=fill).

## When this advances

- Pick a direction above → prototype on `circ`/`circfill` first (the most-used pair), then
  `oval`/`tri`, measuring CPU cost. Write the result up here, and if it changes a public
  primitive's behaviour, add a **`decisions/` ADR**.
- Until then, the **cart-author rule of thumb** (direction 3): draw a dithered shape as
  **one fill with two palette colours**; if you need an outline, draw a **solid backing
  one pixel larger behind it**, never a stroked outline over a dithered fill.
