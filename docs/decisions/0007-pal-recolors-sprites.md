# 0007 — `pal()` recolors sprites, via a palette-swap shader
Date: 2026-05-30 · Status: accepted

## Context
PICO-8's `pal(c0, c1)` remaps a color for *everything* drawn afterward, including
sprites — the foundation of the classic "magic color" trick (draw a character once
with placeholder colors for clothes, then `pal()`-swap them per-entity for a whole
varied crowd). dreamengine already had `pal()`, but it only touched the `palette[]`
LUT used by the *shape primitives* (`cls`, `rect`, `print`, …). Sprites bypass that
table entirely: `spr`/`sspr` blit the sheet texture with a flat `WHITE` tint via
`DrawTexturePro`, reading the texture's baked RGB. So `pal()` silently did nothing to
sprites — a surprising gap for a console whose docs lean on PICO-8 vocabulary.

Three ways to close it were on the table:
1. **Shader palette-swap** — a fragment shader that maps each texel back to its
   palette index and outputs the (possibly remapped) current color.
2. **`spr_tint()` + white masks** — a new tinted-sprite call; recolorable regions
   authored as separate white mask sprites, drawn tinted. GPU-cheap but eats sprite
   slots (each layer is its own sprite), so only ~2 characters fit in the 64-slot
   sheet.
3. **CPU re-blit per swap** — rebuild the texture like `colorkey()` does. Far too slow
   to do per-entity per-frame (`ImageCopy` + upload for every recolor).

## Decision
Make the existing `pal()` recolor sprites too, implemented as a palette-swap fragment
shader (`runtime/studio.c`). No new API — `pal()`/`pal_reset()` just do more now.

## Why
- It's the **PICO-8-correct** behavior the docs already imply, not a bespoke feature.
- **Slot-efficient**: one sprite set + per-entity `pal()` swaps, vs. option 2's
  mask-sprite-per-layer blowup. `the crowd` cart dresses ~28 people from 3 bodies.
- **GPU-cheap**: a 32-iteration nearest-match loop per fragment, and it's **gated on
  `pal_active`** — zero overhead until a swap is live, so existing carts are untouched.
- Sprite texels are *exact* palette RGBs (point-filtered, no blending), so
  "nearest base-palette color" is really "exact match" — robust and simple. Alpha is
  preserved, so `colorkey()` holes stay transparent and the two compose.

## Consequences
- `spr`, `sprf`, `sspr`, `spr_rot`, `sspr_ex` wrap their blit in the shader when a swap
  is active. `tritex` (rlgl immediate mode) is **not** wrapped — out of scope for now.
- The shader ships in two flavors: GLSL 330 (desktop) and GLSL ES 100 (web); the loop
  indexes the uniform arrays with the loop variable only, so it's ES-100-legal.
- Docs updated: `studio.h` + `studioDocs.js` `pal()` one-liners now say "shapes AND
  sprites"; the "magic color" technique is the headline of the `the crowd` cart.
- If `pal()`-on-sprites ever needs to be opt-out for perf, the gate is already there.
