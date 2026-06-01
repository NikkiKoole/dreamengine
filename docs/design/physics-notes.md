# Physics — design exploration (a tiny in-engine physics layer?)

> **Status: brainstorm / scratchpad, nothing decided.** Sparked by the ragdoll cart
> (`tools/carts/ragdoll.c`) feeling good and prompting the question: should dreamengine
> roll a tiny physics layer into the runtime — "a bit like a simple Box2D"? Captured here
> to think about and point back to later.

---

## The fork we've already navigated once: code-first vs black-box

[`0003-code-first-sound`](../decisions/0003-code-first-sound.md) said: *"Making music is
programming — which fits the learn-to-code mission."* We deliberately chose a code-driven
synth over a tracker UI. **Physics hits the identical fork.**

A `world_step()` that silently moves everything is the *tracker* of physics — convenient,
but it hides the thing worth learning. The ragdoll cart being hand-written verlet is, by
our own philosophy, a *feature*: the learner can read every line of why the little guy
falls over.

So the honest first question isn't "Box2D or not" — it's **"is physics something the
runtime does for you, or something you compose?"** Our track record leans **code-first**.
That doesn't kill the idea — it shapes it (see the middle path below).

Relevant open VISION question: *"How much to keep adding to the API vs. teaching learners
to write their own helpers"* — and the still-open *"whether to allow `#include` of other
files."* Physics is a strong test case for both.

---

## The technical fork: rigid bodies vs particles

There are genuinely two different worlds here, and they *feel* different.

### Rigid bodies (the Box2D family)
Objects have mass, velocity, **rotation**, and inertia. Collisions generate contact
manifolds, solved with sequential impulses. This is what you need for **stacking crates,
billiards, dominoes, a spinning crank**.

- **Catch:** rotation + inertia + stable stacking + friction + fast-object tunneling are
  exactly the hard, fiddly parts. Failure modes (jitter, sink-through, explode) live here.
- **API surface is big:** Box2D makes you think in World / Body / Fixture / Shape / Joint.
  Even **Box2D-Lite** (Erin Catto's teaching version, ~1500 lines, boxes only, sequential
  impulses) is a lot of machinery.

### Particles + constraints (Verlet / PBD) — what the ragdoll already is
Points have a position and an *implied* velocity (current − previous). Everything is:
integrate, then relax constraints a few times. No rotation math, no inertia tensor, no
manifolds. You get **ropes, cloth, ragdolls, blobs, chains, soft bodies** almost for free,
with only a handful of concepts.

- The "easy physics library with a few functions" half-memory is almost certainly **this**
  genre.
- Canonical reference: **Matthias Müller's "Ten Minute Physics"** (PBD / XPBD).
- Collision-only reference: **`cute_c2.h`** (Randy Gaul — single header, just
  `circle / AABB / capsule / poly` overlap tests that hand back a manifold; you write the
  response).

**Instinct for *this* engine: PBD, not rigid bodies.** ~80% of the toy-game joy for ~20%
of the engine complexity, and it's basically "promote the ragdoll's inner loop into a
shared place." Existing seeds already in the repo: `ragdoll.c`, `rope.c`, `pendulum.c`,
and the `tritex` work.

---

## How hard, exactly? The difficulty ladder

The difficulty cliff is **not** "shapes" in general — it's the three things that all
arrive together the moment a shape can be **oriented**:

1. **Rotation.** A circle has no orientation (roll it, it looks identical) → you never
   track an angle, never compute torque, never need a *moment of inertia*. A box/triangle
   has a facing, so an off-centre hit must spin it → now every body carries `angle` +
   `angular velocity` + inertia.
2. **"Where exactly did they touch?"** Two circles touching is the easiest collision in
   physics: centres closer than `r1+r2`, push direction = the line between centres (one
   subtraction). Two *polygons* need a real algorithm — **SAT** (Separating Axis Theorem)
   or **GJK** — just to find the contact point(s) and normal. That's the bulk of a real
   engine.
3. **Stable stacking** (the sneaky one). A box resting on the ground touches at **two**
   corners, not one — resolve only one and it rocks/jitters forever. So polygon contacts
   need *manifolds* (multiple contact points) solved together. Friction lives here too
   (a ball mostly rolls; a box needs friction or it slides off every ramp).

Easiest → hardest:

| Tier | What | Difficulty |
|---|---|---|
| Points | positions + constraints only | trivial |
| **Circles** (point + radius) | circle-circle, circle-vs-static-line | **easy — the sweet spot** |
| Static solids (ramps/walls) | only *immovable* boxes/segments, balls bounce off | still easy |
| Dynamic boxes/triangles | oriented, moving, colliding with each other, stacking | **big jump — full Box2D machinery** |

Two ways to cheat the cliff:

- **Static-only solids are cheap.** If triangles/boxes are *walls and ramps that don't
  move* and only balls move against them, you stay in easy territory (circle-vs-segment is
  a few lines). You only pay the full price for *dynamic* oriented shapes hitting *each
  other*.
- **A rigid box is free in the particle world:** 4 points + 6 sticks (rectangle + two
  cross-braces). It rotates, tumbles, and collides using *only* the point/stick code we
  already have — zero new concepts. A bit soft, stacks wobbily, but for a toy it reads as
  solid. The "have your cake" move: pseudo-solids without leaving the easy tier.

**Takeaway:** balls + static ramps/walls + stick-built pseudo-boxes gets surprisingly far
while staying entirely in the cheap, code-first tier. *Crisp* rigid polygons with proper
stacking is the part that turns it into a real project.

---

## The fantasy API (PBD flavour)

~9 calls. A kid could build a rope *or* a ragdoll *or* bouncy balls with the same
vocabulary:

```c
// physics — a tiny verlet world. you make points, link them, and step.
int   pt(float x, float y);            // a physics point; returns its id
void  pt_pin(int id);                  // nail it in place (anchor, wall)
void  pt_radius(int id, float r);      // give it size → points collide as circles
void  stick(int a, int b);             // rigid link, locked at current spacing
void  spring(int a, int b, float k);   // springy link (k 0..1)
void  world_gravity(float gx, float gy);
void  world_bounds(int x, int y, int w, int h);  // floor + walls everything bounces off
void  world_step(void);                // integrate + relax (call once in update)
float pt_x(int id);  float pt_y(int id);         // read back to draw
void  pt_kick(int id, float vx, float vy);       // throw / impulse
```

A whole rope:

```c
int prev = pt(160, 0); pt_pin(prev);
for (int i = 1; i < 20; i++) { int p = pt(160, i*6); stick(prev, p); prev = p; }
// in update(): world_step();  then draw lines between consecutive pts
```

If we later wanted the Box2D feel, the rigid-body version would add `rb_box`,
`rb_restitution`, `rb_friction`, `rb_angle` — but that's where the engine code (and the
"why does it jitter / explode" bug reports) balloons.

---

## The middle path actually pitched

Given code-first-sound and the open "API vs write-your-own-helpers" question, the first
move probably isn't baking `world_step()` into `studio.c`. Candidates, cheapest last to
most-committed first:

1. **A "physics starter" cart** (like a tutorial cart) — the ragdoll's guts generalized
   into a copy-me template. Cheapest, ships today, teaches the most, commits to nothing.
2. **Just the sharp primitives in the runtime** — vector helpers + collision tests
   (`circle_hit`, `seg_hit` returning a manifold, à la `cute_c2`) — and let carts write
   the response. Keeps "physics is programming," removes the tedious math.
3. **A readable `physics.h` mini-lib** the cart `#include`s. Learners can open it and see
   the verlet loop; black-box only if they want it to be. Would force a decision on the
   open `#include` question.

**Suggested order:** start with **#1** to feel out the API and let the function names stop
churning, then promote the winning shape into **#2 / #3** once a few carts have used it.

### No-regrets next step
Prototype a `physics.h` (option 3) with the ~9-function PBD API **plus** a demo cart — a
rope + a few bouncy balls + a draggable blob — so we can *feel* whether the vocabulary is
right before deciding if any of it earns a place in the runtime.
