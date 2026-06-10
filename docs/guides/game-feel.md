# Game feel — feedback

> The how-to for making player actions feel impactful. Extracted from `CLAUDE.md`
> (which now carries only the one-line rule + a pointer here) so it loads on demand
> when you're actually building game feel, not into every agent every session.
> Canonical carts: **`tools/carts/juice.c`** (tutorial 19 — every effect on a live
> toggle) and **`tools/carts/dinorun.c`** (squash/stretch + dust in a real game).

The goal is not to make a game flashy. The goal is to make every player action feel
**noticeable and impactful**. The player presses a button — did it register? Did
something happen? Can they feel the difference between a hit and a miss, between
taking damage and not? Good feedback answers those questions instantly, in the ears
and on the screen, without the player having to read a number.

"Game juice" is the popular term for this, but it's a misleading one — it implies
adding effects for their own sake. The real question to ask is always: **what does the
player need to feel right now, and am I communicating that clearly?**

## Think in events, not effects

Before reaching for a technique, identify the event and ask what the player needs to
feel:

| Event | What to communicate | Typical feedback |
|---|---|---|
| Jump takeoff | "I left the ground" | stretch body upward + dust puff + short blip sound |
| Landing | "I hit the ground hard" | squash body flat + dust puff + small shake |
| Shooting / firing | "I fired" | tiny camera kick backward + muzzle flash pixel + sharp sound |
| Hitting an enemy | "My attack connected" | hit-stop 2–4 frames + enemy flashes white + impact sound |
| Enemy knockback | "It felt that" | enemy slides 4–8px away from hit, then settles |
| Taking damage | "That hurt me" | screen flashes red/white + shake + HP bar visibly drains |
| Player knockback | "I was pushed" | player briefly loses control + stumble animation |
| Near miss | "That was close" | whoosh sound only — no visual; silence would feel like nothing happened |
| Blocking / parry | "I deflected it" | sparks burst at contact point + clank sound + hit-stop |
| Collecting a coin / item | "I got something" | item pops outward then vanishes + ascending blip + score floats up |
| Powerup collected | "I got stronger" | brief full-screen tint to the powerup's color + ascending chord |
| Combo building | "I'm on a roll" | combo counter scales up with each hit + each hit sound rises in pitch |
| Combo broken | "I lost my streak" | counter shrinks and greys out + low thud |
| Enemy death | "I killed it" | particle burst at position + body briefly flashes + death sound |
| Big enemy / boss hit | "This one matters" | longer hit-stop (5–8 frames) + louder sound + screen flash |
| Level complete | "I succeeded" | fanfare chord + screen brightens + results animate in one by one |
| Death / game over | "It's over" | large shake + white flash + heavy low sound + slow reveal of score |
| Score milestone | "I hit a target" | score display inverts briefly + musical strum |
| UI button press | "My input registered" | button scales up 1–2px then snaps back + short tick sound |
| Spawning / respawning | "I'm back" | brief invincibility blink (pal() alternates) + spawn pop sound |

The rule: **every effect must be tied to a specific event.** Adding shake or flash
"because it looks cool" adds noise, not feedback. If removing an effect wouldn't make
an action feel less clear, remove it.

## Particularly satisfying effects — notes on why they work

**Enemy flashing white on hit** (`pal(CLR_DARK_GREY, CLR_WHITE)` for 2 frames then `pal_reset()`) —
the player knows the hit registered even before they see the HP drop. Works because white is the
brightest possible contrast against any background.

**Floating damage numbers** — a small number (+10, -3) that floats upward from the hit point and
fades out over ~40 frames. Satisfying because it converts an abstract HP change into a spatial
event the player can see exactly where it happened.

**Pitch-climbing hit sounds** — each hit in a combo plays one semitone higher (`midi + combo_count`).
Pure audio feedback that lets the player count their combo without looking at any counter.

**Camera kick on shooting** — offset the camera 2–3px opposite the shot direction for 3 frames
then lerp back. Makes the gun feel like it has mass and recoil. Extremely cheap.

**Anticipation pause before big attacks** — freeze the attacker for 4–8 frames (wind-up) before
their hit lands. Counterintuitively this makes the impact feel *heavier*, not slower, because
the player has a moment to brace for it.

**Slow motion on near death** — when HP drops to 1, briefly set a `slow` flag that only runs
`update()` every 2nd frame. The world slowing down communicates danger without words.

**Item pop-scale on pickup** — draw the collected item at 2× scale for 3 frames before it
disappears. The brief enlargement makes the pickup feel physically satisfying.

**Screen edge vignette on damage** — flash the screen border red (4 `line()` calls) for 6 frames
when taking damage. Common in 3D shooters; works just as well in 2D to sell the "I was hit"
feeling without covering the whole screen.

## Techniques and how to wire them

**Sound** — the most immediate feedback channel. The player hears the hit before they
see any visual effect. A satisfying `hit(midi, INSTR_NOISE, vol, dur_ms)` on impact
does more work than almost any visual trick.

**Screen shake** — `shake(amount)` decays automatically; call it on impacts and deaths.
`shake(4–6)` for death/explosions, `shake(1–2)` for a landing or minor hit.

**White flash** — skip the normal draw for 2–6 frames. The simplest version:
```c
static int flash;
// on death: flash = 6;   on hard hit: flash = 3;
// in draw(): if (flash > 0) { cls(CLR_WHITE); flash--; return; }
```

**Hit-stop** — freeze the whole simulation for 2–4 frames on heavy impacts. Nothing
communicates weight like time briefly stopping:
```c
static int hitstop;
// in update(), before any movement: if (hitstop > 0) { hitstop--; return; }
// on hit: hitstop = 3;
```

**Squash & stretch** — deform the character to show acceleration and impact. Jump
takeoff = stretch tall (body elongates); landing = squash flat (body widens). Use a
`float squash` that you set on the event and decay with `lerp` every frame:
```c
static float squash;   // >0 = squash (land), <0 = stretch (takeoff)

// on jump takeoff: squash = -0.85f;
// on landing:      squash =  1.0f;
squash = lerp(squash, 0.0f, 0.22f);   // in update(), every frame

// in your draw function, use squash to shift head/extend body:
int sp  = (int)(squash * 3.0f);    // -3..3 px
int ext = (sp < 0) ? -sp : 0;      // extra height when stretched
```

**Particles** — a small pool of structs (position, velocity, lifetime) spawned at the
event location. A puff at the feet on jump/land, a burst on enemy death, sparks on a
sword hit. Copy the pattern from `juice.c` or `dinorun.c` (~25 lines):
```c
typedef struct { float x, y, vx, vy; int life, col; } Dust;
static Dust dust[24];

static void spawn_dust(float px, float py) {
    for (int k = 0; k < 6; k++)
        for (int i = 0; i < 24; i++)
            if (dust[i].life <= 0) {
                dust[i] = (Dust){ px + rnd_between(-4, 5), py,
                    rnd_float_between(-1.5f, 1.5f), rnd_float_between(-1.8f, -0.1f),
                    rnd_between(8, 15), (k & 1) ? CLR_MEDIUM_GREY : CLR_DARK_GREY };
                break;
            }
}
// tick each frame: x+=vx; y+=vy; vy+=0.12f; life--;
// draw each frame: if (life>0) pset((int)x, (int)y, col);
```

**Tint to white (soft flash)** — a `fillp` overlay fades the whole scene toward white without a hard cut. Draw it after your normal scene, before the HUD. Sequence the density over 3–4 frames for a smooth wash rather than a strobe:
```c
static int tint;   // frames remaining; set to e.g. 4 on hit

// in draw(), after scene but before HUD:
if (tint > 0) {
    // heavier pattern on the first frame, sparser as it fades
    int patterns[4] = { 0xFFFF, 0xA5A5, 0xAAAA, 0x8020 };
    int idx = 4 - tint;   // 0 = heaviest
    fillp(patterns[idx < 4 ? idx : 3], -1);  // -1 = transparent holes
    rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);
    fillp_reset();
    tint--;
}
```
The `-1` hole color means "show the background through"; only the 0-bits of the pattern become white. For an enemy tinting rather than the whole screen, use `pal()` instead — remap the entity's colors toward white for 2–3 frames then `pal_reset()`.

**Motion trail** — keep a short ring-buffer of recent positions and draw faded copies behind the object. Effective for fast projectiles, dashing characters, teleports:
```c
#define TRAIL_LEN 6
static float trail_x[TRAIL_LEN], trail_y[TRAIL_LEN];
static int trail_head;

// in update(), after moving:
trail_x[trail_head % TRAIL_LEN] = px;
trail_y[trail_head % TRAIL_LEN] = py;
trail_head++;

// in draw(), before drawing the main sprite:
for (int i = 0; i < TRAIL_LEN; i++) {
    int idx = (trail_head - 1 - i + TRAIL_LEN * 2) % TRAIL_LEN;
    int alpha = TRAIL_LEN - i;   // older = more faded
    if (alpha > 1) pset((int)trail_x[idx], (int)trail_y[idx],
                        alpha > 3 ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
}
```

**Easing curves** — never tween with raw linear interpolation. `ease_out(t)` for things arriving (they rush in and settle), `ease_in(t)` for things leaving (they accelerate away). For UI popups that overshoot their target and spring back, combine `ease_out` with a brief negative overshoot: lerp past the target by ~10%, pause 2 frames, lerp back.

**Randomised sound pitch** — the same sound repeated every frame becomes grating fast. Vary pitch by ±2–3 semitones on each play: `hit(base_midi + rnd_between(-2, 3), instr, vol, dur)`. The variation is subtle enough not to feel random but prevents the ear fatigue of a perfectly identical loop.

**Coyote time** — in platformers, let the player jump for 6–8 frames after they walk off a ledge. This makes controls feel responsive rather than punishing. Keep a `coyote` counter that starts counting when the player leaves the ground without jumping; allow a jump while `coyote > 0`. Invisible to the player, but they'll notice immediately if it's missing.

**Stack subtle effects on one event** — a single big effect (giant shake) feels cheap. The same event with five small effects stacked together (3-frame freeze + small shake + white flash + particle burst + impact sound) feels chunky and satisfying. Each effect alone is barely noticeable; together they're unmissable.

## Detecting the landing frame

The `was_grounded` pattern captures the exact frame a character touches down:
```c
bool was_gr = grounded;          // snapshot BEFORE physics runs
// ... physics that may flip grounded to true ...
if (!was_gr && grounded) {       // just landed THIS frame
    squash = 1.0f;
    shake(1.2f);
    spawn_dust(px, ground_y);
}
```
