# streetlab — symmetric-corner mirror-blit fix (plan of attack)

> **Status: planned, not built (2026-06-23).** A handoff for whoever next has `streetlab.c`
> free. The diagnosis is confirmed and the verifier (`tools/mirror-diff.js`) and a petri-dish
> demo (`arcsym`) already exist. This doc is the build plan.
>
> Related: [`road-program-state.md`](road-program-state.md) ("Accepted floor"),
> `tools/carts/streetlab.c` (the `SEAM` note ~L85), [`../STATUS.md`](../STATUS.md) §43
> (the visual test-coverage blind spot), and the `arcsym` cart (the mechanism in isolation).

## The bug (confirmed)

On the default 4-way intersection (`cx=160.0`, an **even-width** boundary), the four kerb
fillets are mirror-symmetric in *real space* yet the **rendered kerb edges land ≤1px apart**.
Measured: `node tools/mirror-diff.js streetlab --band 20,110` → **131 mismatched mirror-pairs**;
the overlay shows the central octagon's left kerb edges lighting up (the rest is directional
markings + dashed-line phase, which *should* differ).

**Cause.** The corner loop at `streetlab.c:766–779` draws each corner independently:

```
edge_corner → curb_return → fill_corner (polyfill) / stroke_corner (line) / corner_bike
```

Each arc vertex is `ri()`-snapped to the grid and *then* `polyfill`/`line`-rasterised. As
`arcsym` proves, re-rasterising mirrored geometry **double-rounds** — `ri(mirror(v)) ≠
mirror(ri(v))` at half-pixel phases — so the edge drifts.

**Why the cheap fixes don't work** (both ruled out by the `arcsym` probes):
- Tweaking the rounding can't make every orientation agree — a flip about a pixel boundary
  maps pixel-centres to pixel-centres, so the residual is intrinsic to independent rasterisation.
- Even reflecting the *already-snapped integer vertices* and re-filling fails: the engine's
  `polyfill`/`line` **fill rule itself is not mirror-symmetric** (a half-open scan interval +
  per-scanline crossing rounding), so identical mirrored vertices still fill to off-by-one
  pixels (the probe showed a uniform 1px edge shift).

The only exact fix is to **reflect the rendered pixels**, exactly as the docs predicted:
*"compute one corner, blit it rotated/mirrored for non-skew junctions."*

## Scope / guard

Apply the fix only where the four corners *should* be pixel-mirror-images — the D4-symmetric
case:

```c
!roundabout && !isT && skew == 0      // 4 arms at 0/90/180/270, uniform HW & cornerR
```

`cornerR` and `HW` are single globals (`streetlab.c:173`, `cross_hw()`), so uniformity is
automatic. **Skew, T-junctions, and the roundabout keep the current per-corner path** — there
is no symmetry expectation there, so the accepted floor simply stays for those (note it in the
docs rather than chasing it). Marking toggles (turn lanes / median / bike / parking) are fine to
leave on: the kerb arcs stay 4-fold symmetric and the markings are drawn *after* the corners.

## The fix — "render one quadrant, mirror-blit the other three"

Insert **after** the corner loop and **before** the per-arm markings loop (`streetlab.c:783`).
This ordering is load-bearing: arrows / stop bars / turn arrows are directional (drive-on-right)
and must **not** be mirrored.

### Option A — `pget`/`pset` capture-and-mirror (recommended first)

~30–40 lines, no rasteriser duplication.

1. `enable_pget(true)` once in `init()` (the cart never enables read-back today).
2. Define a central box around `(cx,cy)`, radius ≈ `HW + cornerR + (peds?SW:0) + 2`.
3. For the three non-canonical quadrants, read the canonical quadrant's pixels with `pget` and
   write them mirrored with `pset`:
   - left/right: `x' = 2*round(cx) - 1 - x`
   - up/down:    `y' = 2*round(cy) - 1 - y`
   (the even-width bijection — no fixed pixel; the same reflection `mirror-diff.js` uses).

**Trade-off:** `pget` reads the *previous* frame, so there's a one-frame transient right after a
parameter change; in steady state the draw is deterministic, so the read is exact. Negligible for
a sandbox. The box is ~50×50, so the 3× pget/pset cost is trivial.

### Option B — software cell-buffer (the lag-free upgrade)

Rasterise one corner into an index buffer (reuse `arcsym`'s `fillpoly` + `bres`), then `pset`-blit
it ×4. Deterministic, no `pget` lag, and spec-able — but it reimplements the fill/stroke/bike
compositing. Do this only if A's one-frame lag or perf ever bites.

**Recommendation:** ship **A** (lowest risk, literally the docs' recipe); keep **B** documented as
the upgrade path.

## Verification — the golden-pixel-diff gate

`tools/mirror-diff.js` is the harness (promoted from the `pngdiff.js` investigation prototype). It
renders a cart headless, reflects the framebuffer about `cx`/`cy`, and counts mismatched
mirror-pairs — the framebuffer twin of `tune-check`/`level-check`, and the visual-regression gate
[`../STATUS.md`](../STATUS.md) §43 said was missing.

```bash
node tools/mirror-diff.js streetlab --band 20,110           # measure (now: 131)
node tools/mirror-diff.js streetlab --band 20,110 --overlay /tmp/x.png   # see WHERE
node tools/mirror-diff.js streetlab --xband 130,190 --band 40,70 --quiet # gate just the kerb band
```

- **Before:** ~131 mismatched pairs over the junction band (most is directional markings + dash
  phase; the kerb-band-restricted count is the real target).
- **After the fix:** 0 in the kerb band for the symmetric case.

To gate cleanly, restrict to a column/row band that excludes the dashed lane lines (or render with
markings off), so dash phase doesn't create false positives. `--quiet` exits 1 on any mismatch (CI).

## Build sequence (when streetlab frees up)

1. Add the guard predicate + `enable_pget(true)` + the central-box mirror-blit (Option A) in the
   non-roundabout branch, after `streetlab.c:779`.
2. Pin the gate: pick the kerb-band `--xband`/`--band`, confirm 131→0 for the symmetric default,
   and confirm skew/T are untouched (still use the per-corner path). Wire it into a check (or note
   the invocation in `debug-harness.md`).
3. Update the three "accepted floor" mentions — `road-program-state.md` "Accepted floor",
   `streetlab.c` `SEAM` note, `../STATUS.md` §43 — from *accepted* to *fixed for symmetric
   junctions via mirror-blit; skew/T still accept the floor*.

## Gotchas

- **Draw order**: blit after sidewalk/asphalt/corners, before markings. The earlier passes are
  already symmetric, so over-stamping them is a safe no-op.
- The 1px seam at the fixed centre column/row sits under the central asphalt overlap — hidden.
- `corner_bike` (the terracotta corner wrap) is inside the box → captured/mirrored for free.
- Keep the box clear of the toolbar/HUD; the junction is centred, the box is small, so it's fine.
