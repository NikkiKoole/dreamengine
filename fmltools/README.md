# fmltools — Floorplanner `.fml` → playable top-down level

Project-specific tooling (not general cart tooling) that turns a Floorplanner
`.fml` export into the `floorwalker` cart: a walkable top-down level with real
walls, doorways, room floors, and furniture drawn as pixel-art sprites baked from
Floorplanner's CDN renders.

## Pipeline (run from repo root, in order)

```bash
F="/path/to/plan.fml"

# 1. geometry: .fml -> level data spliced into tools/carts/floorwalker.c
node fmltools/fml2cart.js "$F" --scale 8 --maxfurn 280

# 2. art: fetch each furniture/door/window top-down render -> palette pixel sprite
node fmltools/fml-assets.js "$F" --max 24 --outline --posterize 5 --saturate 2.2

# 3. embed the baked sprites into the cart (keyed to lv_refs order)
node fmltools/fml-sprites.js

# build + bake a thumbnail
node tools/make-cart.js tools/carts/floorwalker.c editor/public/carts/floorwalker.cart.png
node tools/make-cart.js --run editor/public/carts/floorwalker.cart.png
# play it
node tools/play.js floorwalker run
```

## Tools

- **fml2cart.js** — parses walls (door openings become walkable gaps, windows stay
  solid), room polygons (each a distinct colour, see `AREA_COLORS`), furniture and
  door/window openings as oriented sprites. Units are centimetres; `--scale` is
  cm/px, `--maxfurn` (cm) splits furniture from oversized surface planes. Splices a
  `FML_DATA` block into `floorwalker.c`. Flags: `--scale --maxfurn --floor --design
  --out --stdout`.
- **fml-assets.js** — for each refid, fetches `cdb/renders/<2>/<refid>_NN.top.x3.png`
  (probes NN), downscales (alpha-aware), `--saturate`, `--posterize`, `--outline`,
  then quantises to a **feedable palette** (`--palette`, default pico32 — note: the
  engine's 32 colours are fixed, so the palette only chooses *which* of them to use).
  Outputs `<refid>.png`, `manifest.json`, and a `preview.png` contact sheet under
  `build/.fml-assets/`. Raw downloads cached under `cache/`.
- **fml-sprites.js** — reads `manifest.json` + the cart's `lv_refs[]`, embeds each
  sprite's palette indices into the cart's `FML_SPRITES` block.

## Known gaps

- **Photo floor textures**: areas/surfaces reference textures by Roomstyler IDs
  (`rs-####`), which don't resolve through the render/texture CDN paths. Real tiled
  textures need an `rs-####` → texture-filename resolver. Until then `fml2cart.js`
  gives each room a distinct flat colour.
- Some refids have no published top-down render (reported as failures by `fml-assets.js`);
  those furniture items fall back to placeholder boxes (toggle with `T` in-cart).
