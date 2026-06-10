# Audio timing — frame-quantized triggers, and why web drifts

> EXPLORATION (researched 2026-06-10). Source: a real phone-play session — web
> playback "feels a bit drunk," with **occasional drift** on web that isn't audible
> on native. This note traces the cause through the code and ranks the fixes.
> The latency half of the web-audio question lives in
> [`product-notes.md`](product-notes.md); **this note owns timing jitter/drift**.

## Symptom

- **Native**: tight (any wobble is below the threshold of hearing).
- **Web**: a faint continuous wobble *plus* an **occasional** slip/stumble — the
  groove jumps and resettles. Tempo is right *on average* (no permanent speedup), so
  it's a **placement** problem, not a tempo problem.
- **Web on mobile (2026-06-10, `drift` on an iPhone):** even the **TRUTH** voice —
  the sample-accurate `schedule_hit` reference that *cannot* jitter from scheduling —
  is audibly wobbly. That rules scheduling out as the sole cause and indicts the
  **audio output clock itself** (see suspect #3, now confirmed).

## How timing works today

The musical clock is a main-thread accumulator, ticked once per frame from
`studio.c` **before** `update()`:

```c
// sound.h:469
static void sound_tick(float dt) {
    beat_accum += dt * (sound_bpm / 60.0f);   // advance by this frame's wall-clock delta
    int new_beat = (int)beat_accum;
    beat_just_advanced = (new_beat > beat_now);   // ← a BOOL (see "dropped beats" below)
    beat_now = new_beat;
    ...
}
```

`dt` comes from the loop. **This is where native and web diverge** — and the
divergence is the whole story:

```c
// studio.c — WEB path (was, before this note's fix):
frame_dt = GetFrameTime();          // raw rAF delta — jittery AND unbounded
sound_tick(frame_dt);

// studio.c — NATIVE path:
double tn = GetTime(); frame_dt = (float)(tn - last_time); last_time = tn;
if (frame_dt > 0.1f) frame_dt = 0.1f; if (frame_dt < 0) frame_dt = 0;   // hitch clamp
sound_tick(frame_dt);
```

Native gets a steadier clock **and** clamps hitches to 100 ms. Web had neither.

### How a note's onset is decided

Two paths, and they explain why the radios are tighter than tutorial drum loops:

1. **Frame-triggered (the drunk path).** A cart reads `every(n)` ("true once per n
   beats — fire it on the beat", `studio.h:387`) or `beat_just_advanced`, then calls
   `hit()` that frame. The onset is therefore *whenever the frame that detected the
   beat happened to run* — quantized to frame timing. Users: `06-sound`,
   `20-instruments`, `21-lfo`, `beatcrypt`, `calgames`, `cannonfodder`, `crowd`, …
2. **Schedule-ahead (the tight path).** `radio.h:130-133` runs a step counter against
   the beat clock and schedules **one step ahead** with `schedule_hit(dly, …)`, which
   carries a sample-accurate `delay_samples` the **audio thread** fires precisely
   (`sound.h:443/486`, the `delayed[]` pen). Its own comment: *"Never trigger on the
   frame (up to 16ms of jitter = a drunk drummer)."* `schedule_hit`'s doc brags
   *"sample-accurate future time… no frame-rate jitter"* (`studio.h:383`).

## Diagnosis (ranked)

### 1. PRIME (web-only, "occasional"): unbounded `frame_dt` lurches the clock

Most frames are only slightly jittery (→ the faint continuous wobble). But every
browser hitch — GC, scroll, tab/background work, a heavy render frame — arrives as
one large `dt`, and with no clamp the beat clock advances by the **entire gap at
once**: `beat_accum += dt`. The sequence lurches forward, then resettles. Tied to
hitches → **intermittent**, exactly "occasional drift." Native clamps the same hitch
to 100 ms, so it just holds steady — which is why native sounds clean.

**Second-order amplifier — dropped beats.** `beat_just_advanced` is a *bool*
(`sound.h:472`). If a hitch jumps the clock 4 → 7 in one frame, it fires **once** —
beats 5 and 6 never trigger. So a web hitch on a frame-triggered cart doesn't merely
slip, it can **drop notes** and resume out of place. (Schedule-ahead carts instead
get a negative look-ahead delay and fire a clump of late notes to catch up — also a
stumble.)

### 2. Continuous: frame-rate quantization on a jittery clock

Even with no hitch, onsets on the frame-triggered path are pinned to the frame grid
(~16.7 ms even at a steady 60 fps = ±8 ms), and web's rAF clock jitters that grid
frame to frame. Below most listeners' threshold for slow material; audible on fast
rhythmic steps. This is the "drunk" baseline; native's steadier clock keeps it under
the threshold.

### 3. CONFIRMED (web, worst on mobile): the audio output clock runs on the main thread

**This is the one that wobbles TRUTH**, and it's the deeper problem. The web build
ships **no AudioWorklet and no worker threads** — verified: neither `build-site.js`
nor the editor's `main.cjs` passes `-sAUDIO_WORKLET`/`-sWASM_WORKERS`, and there is no
worklet/ScriptProcessor code anywhere in the tree. With those flags absent, emscripten
+ raylib's miniaudio fall back to a **`ScriptProcessorNode`, whose `onaudioprocess`
runs on the main thread.**

So `sound_callback` (`sound.h:3279`) — which both *generates* the samples and *counts
down* `schedule_hit`'s `delay_samples` — is interleaved with rendering and GC on the
single main thread. On mobile (weaker CPU, busy compositor) the callback fires late and
in bursts, producing audio in catch-up chunks rather than a smooth stream. The **output
clock is therefore unsteady**, and "sample-accurate" loses its meaning: a note booked N
samples out lands whenever the main thread next services the audio node. That smears
*every* voice, **including the schedule-ahead TRUTH** — which is exactly the
on-device finding (symptom §3). The `frame_dt` clamp (suspect #1) is a layer above this
and cannot help it.

### Contributing: the 1024-sample buffer is small for a main-thread audio clock

The 1024-sample stream buffer (`sound.h:3277`, ≈23 ms) is fine when a dedicated audio
thread refills it on time. But when the refill happens on a contended **main** thread
(suspect #3), 23 ms is a thin cushion — a render frame longer than that starves it.
The buffer size isn't web-conditional. (On a steady clock it would be a pure *latency*
contributor — the `product-notes.md` topic — not jitter.)

## Fixes (ranked)

1. **DONE — clamp web `frame_dt`** (this note's change, `studio.c` web path): mirror
   native's `if (frame_dt > 0.1f) frame_dt = 0.1f`. One line. Converts a hitch from
   "lurch the music / drop beats" into "hold steady and resume in time." Targets
   suspect #1 — the *occasional drift you actually hear*. Does **not** fix the
   baseline frame-quantization wobble (#2).
2. **Engine cure — sample-clock beat.** Advance `beat_accum` from the **samples the
   audio callback actually produced**, not `frame_dt`. Then the beat position is
   rock-steady regardless of frame jitter, which de-jitters `every()`/`beat_just_advanced`
   *and* tightens the radios' look-ahead math. Bigger change; the real fix for #2.
3. **Cart-level — schedule ahead.** Carts on the frame-triggered path adopt the
   `radio.h` pattern (or a future shared helper) and trigger via
   `schedule`/`schedule_hit`/`strum` instead of `hit()` on the beat. Infrastructure
   already exists and is proven on the radios.
4. **Fix the bool** (cheap, complements #1/#2): make `beat_just_advanced` carry *how
   many* beats advanced (or have `every()` fire per crossed beat) so a multi-beat jump
   doesn't silently drop the in-between beats.

### Fixes for suspect #3 (the TRUTH-wobble — the deeper one)

5. **Cheap, reversible — bigger web audio buffer.** Raise
   `SetAudioStreamBufferSizeDefault` (`sound.h:3277`) from 1024 to 2048/4096 **on web
   only**. A deeper buffer rides over main-thread stalls (fewer underruns → steadier
   output clock), trading latency (~46/93 ms) for steadiness. For a non-interactive
   sequencer that's a pure win; for a live touch instrument it's a latency cost. The
   fastest thing to A/B on a phone — does TRUTH steady up? Run the soundcheck tripwire
   after (CLAUDE.md: any `sound.h` change).
6. **Less main-thread load** ([`frame-pacing.md`](frame-pacing.md)): reactive rendering
   / an `fps` cap frees main-thread time for the audio callback, so it starves less.
   Synergistic with #5 — fewer render frames competing with the ScriptProcessor.
7. **The real fix — AudioWorklet.** Build web audio on an `AudioWorklet`
   (`-sAUDIO_WORKLET=1 -sWASM_WORKERS=1` + a miniaudio/raylib that targets the
   emscripten worklet), so the callback runs on a **dedicated audio thread** with a
   steady clock, decoupled from rendering. This is what actually makes TRUTH tight on
   mobile. Caveat: depends on the vendored raylib/miniaudio version supporting
   emscripten audio worklets — **needs a feasibility check** (raylib bump or a custom
   audio backend) before it's a promise, not a one-liner.

## Verification plan

- **The `drift` cart is the purpose-built A/B rig** ([`../guides/instrument-carts.md`](../guides/instrument-carts.md)
  → Sequencers): it plays the same 16th pulse on all three clocks at once
  (`schedule_hit` / `hit`-on-frame / every-N-frames), panned center/left/right with a
  per-voice error scope, plus a **GLITCH** button that stalls a frame on demand. Solo
  TRUTH = metronome; add the others to hear the wobble/drift; tap GLITCH to see the
  clamp hold the groove through a hitch.
- **A/B the clamp**: publish `drift` (or any frame-triggered music cart), play on a
  phone, provoke hitches (scroll / background a tab / busy device / the GLITCH button),
  compare before/after the clamp. The clamp should remove the hitch-driven slips;
  residual wobble = suspect #2.
- **Quantify on-device**: an on-device audio capture (no native harness path exists
  for web) against a click track would separate jitter (variance) from any slow creep
  (suspect #3).
