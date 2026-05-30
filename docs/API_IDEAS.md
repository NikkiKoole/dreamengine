# API Ideas

Notes from analysing the tutorial and game cart sources. Things to keep in mind as the cart collection grows.

> **Status (2026-05-30).** The juice/graphics primitives have landed (in working
> tree): `pal`/`fade`/`shake`, `print_scaled`, `text_width`, `spr_rot`/`spr_ex`,
> `oval`/`ovalfill`, `key`/`keyp`/`text_input`, `dt`, `save_bytes`/`load_bytes`
> (see API_RESEARCH §18–19). That clears the graphics gaps. The remaining
> **cart-pattern** helpers below — `hud()`, `game_over_screen()`, `explode()` — are
> now the top unmet items, since they recur across 11–12 carts each and aren't
> covered by anything that landed.

## What's already well-abstracted (don't touch)

`near()`, `boxes_touch()`, `touching_map()`, `angle_to()`, `dx()`, `dy()`, `lerp()`, `ease_*()`, `clamp()`, `anim()`, `follow()`, `camera()`, `rnd()`, `rnd_between()`, `note()`, `hit()`, `schedule()` — all solid, used consistently. No changes needed.

---

## Patterns worth addressing

### 1. HUD header bar — 11 files

Every game draws its own version of:
```c
rectfill(0, 0, SCREEN_W, 18, CLR_DARKER_BLUE);
print("GAME", 4, 5, CLR_LIGHT_GREY);
print(str("best: %d", hi), 240, 5, CLR_YELLOW);
```

Candidate API: `hud(label, score, best, lives)` — draws the standard header bar.

Files: 07-score, 12-hiscore, 14-hud, 18-invaders, 19-breakout, asteroids, frogger, lander, platform, pong, robotron.

---

### 2. Game over / press A to restart — 12 files

Everyone writes their own "darken screen, print GAME OVER, wait for btnp(A)" block. Varies slightly per game (some show score, some show best, some have a delay before accepting input).

Candidate API: `game_over_screen(score, best)` returning `true` when the player presses A. Probably too rigid given the variation — worth watching whether a pattern emerges as more carts are added before committing to an implementation.

Files: 09-enemies, 18-invaders, 19-breakout, asteroids, frogger, lander, platform, pong, robotron, snake + others.

---

### 3. Particle / explosion burst — lander, asteroids, robotron

Radial debris on death is reimplemented each time:
```c
for (int i = 0; i < 8; i++) {
    float a = i * 45.0f;
    pset((int)(x + dx(age * 0.6f, a)), (int)(y + dy(age * 0.6f, a)), CLR_ORANGE);
}
```

Candidate API: `explode(x, y, age, color)` — stateless radial burst driven by a caller-owned age value. Fits the engine's style (no hidden state).

---

### 4. Entity pool with `.on` flag — 6+ files

Every game reinvents the same pattern: array of structs, `bool on`, loop skips inactive slots. Hard to abstract cleanly in C without macros or generics.

Candidate: a `POOL_FOREACH(pool, n, var)` macro that expands to the skip-if-not-on loop. Low priority — the pattern is simple and readable as-is.

Files: 18-invaders, asteroids, frogger, platform, robotron + others.

---

### 5. Tile-collision push-out — surveyed, low demand

A `move_and_collide(&x, &y, w, h, vx, vy)` that moves an entity and resolves it out
of solid map tiles came up while polishing platformers. A cart survey (2026-05-30)
found the demand is thin:

- Only **`platform.c`** does the full pattern — `mget` → 4-corner `solid_at` test →
  axis-separated move-then-push-out → `on_ground`.
- `zelda.c` and `gta.c` do a related 4-corner solid test, but top-down (no gravity)
  and against their *own* world data, not `mget` — a single map-based helper
  wouldn't serve them cleanly.
- alleycat / pitfall / burgertime / sokoban don't use the map-collision API at all
  (hardcoded ledges, fixed floors, custom struct grids, or grid-snapped movement).

Verdict: **not worth a built-in yet.** If anything, the reusable nugget is the
4-corner "is this box solid?" predicate, not the full move-resolve — but even that
varies by data source. Revisit if more tile-based platformers appear.

---

## Priority order (when the time comes)

1. `explode()` — pure visual, stateless, easy to add, immediately useful in several games
2. `hud()` — affects almost every cart, high leverage
3. Pool macro — low priority, code is already readable
4. `game_over_screen()` — wait for more carts before deciding on the interface
