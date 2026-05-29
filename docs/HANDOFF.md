# Handoff / working notes

> Portable context for picking dreamengine up on another machine or in a fresh
> session. This is the stuff that isn't obvious from the code or git log. Keep it
> short; prune what goes stale.

_Last updated: 2026-05-29_

---

## Where we are right now

- **Debug tools shipped** (`printh` / `watch` / `watch_visible` / crash capture).
  Full design + as-built notes in [`docs/debug-printh-watch.md`](./debug-printh-watch.md). Status header there is the source of truth.
- A **second agent is expanding the API** in parallel (math / collision /
  animation / strings / timer, then the later passes in
  [`docs/API_RESEARCH.md`](./API_RESEARCH.md)). Their work and the debug work are
  **interleaved in the same files** — `runtime/studio.{c,h}`,
  `editor/src/shell.js`, `editor/src/studioDocs.js`. Commit `70210c5` bundles both
  (the user chose "commit everything together" because they couldn't be cleanly
  split here).

## Next on the todo list (from VISION.md "Biggest open items", by impact)

1. **Cartridge save/load** — `.cart` bundling code+sprites+map(+sound). VISION's #1
   ("show your friend" loop). Mostly `main.cjs` file I/O. Low overlap with API agent.
2. **Process/coroutine model** (Level-2 learning) — the big differentiator vs PICO-8.
   Weeks; architectural (C coroutines + a `process … loop … frame;` transformer).
3. **Tutorial carts** — dropdown of progressively-bigger example carts. Quick, low overlap.
4. **Inline error markers** — map clang `cart.c:line:col` errors to CodeMirror gutter
   marks. *Tightest follow-on to the debug tools* (reuses the compile pipe already
   wired in `main.cjs`). Touches `shell.js` → some overlap with the API agent.
5. Browser sharing target (emscripten / JS runtime) — large.
6. iPad runtime — touch already wired; needs a build path.

Also unresolved: **sprite-frame semantics** (VISION's open question; lean is "#4
animation strips") — pairs with the API agent's `anim()`.

**Was about to ask which of #1/#3/#4/sprite-frames to start; user had to run.** No
decision made yet — pick up there.

## Gotchas / environment facts (learned the hard way)

- **`main.cjs` / `preload.cjs` changes need an Electron restart** (`npm start`);
  Vite hot-reloads everything else.
- **`▶ run` only works in Electron** (it spawns clang); the browser tab edits but can't run.
- Use **Node 22** (`nvm use 22`) before any npm command.
- **Crashes can't be reproduced headlessly here** — `InitWindow` needs a display,
  so a cart run in a non-GUI shell just exits 0 immediately. Verify crash/overlay
  behavior by running the actual app.
- **Reliable test-crash:** a `volatile` null read/write —
  `volatile int *p = 0; int v = *p;`. A plain `*p = 42;` now also crashes because
  the build passes `-fno-delete-null-pointer-checks` (added this session). **arm64
  integer divide-by-zero does NOT trap** (returns 0) — SIGFPE won't fire on Apple Silicon.
- `studioDocs.js` is the single source of truth for autocomplete + hover + help.
  Adding an API fn = declare in `studio.h`, implement in `studio.c`, add a
  `studioDocs.js` entry, add the key to a `sections` group in `shell.js`.

## Working preferences observed this session

- **Respect day/night theming** — use the CSS vars (`--bg`, `--bg2`, `--fg`,
  `--fg-dim`, `--border`, `--font`), never hardcode panel colors.
- **Panels should auto-hide when there's nothing to show** (like the build log's
  3s timer) rather than lingering.
- **Optimize for beginner legibility** — making mistakes visible (crash banners,
  watch dumps, reliable null-deref faults) is a first-class goal, not polish.
- Commits go **direct to `master`** (solo repo; that's the established history).
