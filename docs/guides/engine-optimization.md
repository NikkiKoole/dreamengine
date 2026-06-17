# Engine optimization — the safe loop, and the ledger

How we make the engine faster **without changing what it draws or plays**. The whole
discipline is one rule:

> **An optimization must prove it's invisible.** Same pixels (byte-identical), same
> audio, same behaviour — *then* measure that it's faster. If you can't prove "same," it's
> not an optimization, it's a change.

This guide is the repeatable loop for that, the tooling that makes it cheap, and a
**ledger** of what we've done. Pair it with [profiler.md](profiler.md) (where the time
goes) and [debug-harness.md](debug-harness.md) (how to make a run deterministic).

---

## The loop

1. **Profile first — find the real hot path, don't guess.** `⏱ profile` (editor) gives
   the §2 call-graph (cost) + §3 draw counts (frequency). See [profiler.md](profiler.md).
   Optimize what's actually hot; a 90%-of-frame function is the only thing worth touching.
2. **Build a stress cart that pins it.** A deterministic cart that hammers the hot path so
   the profile is unambiguous *and* the output is byte-reproducible frame-to-frame. This is
   your correctness oracle (next step) and your profiling target. `polystress` (the polygon
   rasterizer) and `trifill_stress` (the off-screen scan-clamp) are the templates — both
   registered `tech-demo` carts, both pure functions of a frame counter (no time, no random).
3. **Keep the old code beside the new, switchable at runtime.** Don't delete the thing you're
   replacing — keep it as `*_legacy` behind a flag flipped by an **env var** (so any cart can
   be A/B'd headless or in the editor with no recompile). See the pattern below.
4. **Prove byte-identical.** Dump frames both ways and compare hashes:
   ```bash
   node tools/play.js <cart> script /dev/null --headless --frames 90 --dump /tmp/new --dump-every 15
   ENV=legacy … --dump /tmp/old …
   diff <(shasum /tmp/new/*.png|awk '{print $1}') <(shasum /tmp/old/*.png|awk '{print $1}')
   ```
   Identical = safe. A mismatch is a gift — it's a bug the oracle caught *before* it shipped.
   When one differs, **pixel-diff to the exact `(x,y)`** and reproduce it in a tiny standalone
   C program (matching the engine's `float` math); don't reason about rounding in your head
   (worked example below — two real bugs found this way).
5. **Run the engine's own regression guards.** Some invariants live in dedicated carts/tools:
   `raster_test` (fill==outline boundary, `mismatches:"0"`), `tune-check`, `level-check`,
   `fx-check`, `soak-check`, `web-audio-check`. Re-run the ones your change could touch
   (see CLAUDE.md "After touching …").
6. **A/B measure, same build.** Flip the flag and read `perf.json` (`workMsAvg`/`workMsMax`).
   Both runs must be the *same build* — comparing a `play.js` number to an editor `⏱` number
   is apples-to-oranges (different `-O` flags).
7. **Record it in the ledger** (bottom of this doc) with real before/after numbers.

---

## The keep-both A/B switch (the pattern)

Replacing a hot function? Keep the original verbatim as `<name>_legacy`, gate on a global
flipped by an env var at startup. Concretely (from `runtime/studio.c`):

```c
static bool poly_fill_fast = true;          // up with the other globals

// in main(), before anything draws:
{ const char *pf = getenv("DE_POLY_FILL");
  if (pf && strcmp(pf, "legacy") == 0) poly_fill_fast = false; }

static void poly_fill_cov_legacy(...) { /* the exact old code, untouched */ }
static void poly_fill_cov(...) {
    if (!poly_fill_fast) { poly_fill_cov_legacy(...); return; }
    /* the new path */
}
```

Why env var, not a `#define` or a key:
- **No recompile** between A and B — the same binary does both, so the comparison can't be
  contaminated by a stray flag.
- **Inherits everywhere.** `play.js` spawns the native binary, the editor spawns it too — both
  inherit the parent env. Headless A/B is per-run (`DE_POLY_FILL=legacy node tools/play.js …`);
  the editor is session-wide (`DE_POLY_FILL=legacy make`).
- **Both functions stay compiled and referenced**, so neither bit-rots and there's no
  `-Wunused` noise.

Keep the legacy path until the new one has soaked across many carts; then delete it + the
flag in a dedicated commit. Mark it `TODO` so it doesn't become permanent.

**A/B any cart, headless:**
```bash
ab() { rm -f build/perf.json; env "$1" node tools/play.js "$2" script /dev/null --headless --frames 240 >/dev/null 2>&1;
       node -e 'const p=require("./build/perf.json");console.log(`${p.workMsAvg.toFixed(3)}ms avg / ${p.workMsMax.toFixed(3)}ms max`)'; }
ab "DE_POLY_FILL=legacy" roadlab   # old
ab "" roadlab                      # new
```

---

## Worked example #1 — the polygon rasterizer span fill (2026-06)

**The cost.** `polyfill`/`trifill`/`ngonfill`/`starfill` all fill in software via
`poly_fill_cov`: scan the bounding box, test every pixel with the even-odd `poly_inside`,
and `pset` (→ `DrawPixel`) each inside one. The profile of `polystress` (159 fills/frame):
**~68%** of cart-active CPU was the per-pixel *write* (`DrawPixel` → one immediate-mode
`rlVertex3f` *per pixel*, plus `rlSetTexture` churn), **~26%** the per-pixel `poly_inside`.

**The change.** Rewrote `poly_fill_cov` as a **scanline span fill**: per row, compute the
sorted edge crossings (instead of testing each pixel), and paint each solid span as **one
`DrawRectangle`** (instead of N `DrawPixel`). The inside *decision* stays 100% on the CPU at
pixel-centre `x+0.5` — only the *plotting* of an already-decided run is batched — so it does
**not** revive the rejected "let the GPU decide coverage" approach
([rasterization-consistency.md](../design/rasterization-consistency.md) direction 2; an
integer height-1 rect paints exactly the same pixels on desktop GL and web GL ES, no
diagonal-edge ambiguity). The fast path is gated on an **axis-aligned camera** (rotation 0,
zoom 1) and a **solid** fill; a rotated/zoomed camera or active `fillp()` dither falls back to
per-pixel `plot_pat` (still skipping per-pixel `poly_inside`, so still faster), preserving the
documented rotated-fill staircase and the dither lattice exactly.

**The two bugs the byte-identical oracle caught** (both invisible to the eye, fatal to the
"invisible" claim — found by pixel-diff → standalone-C repro, not by squinting):
- **Notch bleed.** A trim that used the global `poly_inside` to find span ends latched onto a
  pixel that was inside via the *adjacent* span across a sub-pixel gap, filling a 1px star-notch
  that should've stayed open. Fix: bound each span by *its own* two crossings, not the global
  predicate.
- **Half-open off-by-one.** `poly_inside`'s strict `<` makes the inside interval `[cL, cR)`
  (left-closed, right-open). The naive `floor(cL)+1` left bound differs from the correct
  `ceil(cL-0.5)` exactly when a crossing lands on a half-integer — 83 mismatched pixels across
  the test frames until fixed. Verified the half-open rule exhaustively (reference vs formula,
  every pixel, every frame → 0 mismatches).

**Result** (same-build A/B, `perf.json` via `play.js`):

| cart | legacy | fast | speedup | max frame |
|---|---|---|---|---|
| `roadlab` (all-solid road strips — the real target) | 4.95ms | **1.83ms** | **2.7×** | 13.0 → 3.4ms |
| `polystress` (half its fills are dithered → fallback) | 8.47ms | **5.18ms** | 1.6× | 19.6 → 6.6ms |

(Editor `⏱ profile` build, the other yardstick: `polystress` 9.0 → 5.4ms typical, 54% → 32%
of budget.) `roadlab` gets the full win because it's all solid fills; `polystress` under-sells
it on purpose (its big dithered polys take the per-pixel fallback). The **max-frame** drop is
the bigger story — the per-pixel path spiked, the span path is steady.

**Validation:** `polystress` byte-identical (legacy == fast == pre-change original);
`raster_test` 20/20 `mismatches:"0"`. Commit `8f201c5`.

**Open follow-up:** the dither path is still per-pixel by design. Making it fast means routing
dithered spans through `rectfill_pat` — but session-14 deliberately unified dither onto
`plot_pat` for the consistency invariant, so that's a design discussion, not a free win.

---

## Ledger

| date | area | what | proof | result | flag / commit |
|---|---|---|---|---|---|
| 2026-06-02 | software polys | clamp scan box to on-screen region (off-screen bbox no longer scanned) | `raster_test` 0 | `trifill_stress` 46.7→2.7ms | (in rasterization-consistency.md) |
| 2026-06 | software polys | `poly_fill_cov` scanline span fill (solid → one `DrawRectangle`) | `polystress` byte-identical, `raster_test` 0 | `roadlab` 2.7×, `polystress` 1.6× | `DE_POLY_FILL=legacy` · `8f201c5` |

---

## See also
- [profiler.md](profiler.md) — `⏱ profile`: hot functions, call paths, budget, draw counts.
- [debug-harness.md](debug-harness.md) — deterministic `--dump`/`--trace`, live `perf.json` snapshot.
- [../design/rasterization-consistency.md](../design/rasterization-consistency.md) — why the fill
  family is CPU-per-pixel (the invariant any rasterizer optimization must preserve).
