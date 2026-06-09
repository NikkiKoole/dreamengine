# The voice engine (INSTR_VOICE) — design notes

> **Status: in design (experimental).** Engine ported + playable; the public API is NOT
> locked. Lives behind `voice_param()`/`voice_consonant()`/`voice_coda()` — none are in
> `studioDocs.js`/`shell.js` yet. Probe carts: `voxlab` (fat 7-param prototype), `vox`
> (the 3-axis play cart + jam pad), and `say` (the PROSODY probe — §"Prosody" below).
> Probe ledger rows: [`probe-carts.md`](probe-carts.md).

## What it is

A port of navkit's **VoicForm** (`navkit/soundsystem/engines/`): a glottal pulse (Rosenberg
polynomial) through **4 parallel SVF formant filters** with a continuous vowel morph — the
same Chamberlin SVF dreamengine already uses for `filter()`. Vowels-only port: the 10 vowel
rows of navkit's phoneme table, plus a trimmed consonant set used for **articulation onsets/
codas** (not the full ARPABET text-to-speech path — that stays a separate future project).

This is the **formant SOURCE engine**, distinct from the formant *filter effect* (a vowel
filter you can put on any instrument). This one generates the voice from scratch.

## The control decomposition — the key idea

The voice's expressivity splits into **three KINDS of control**, and only one kind competes
for the engine's three macro slots:

| Kind | Examples | Lives where |
|---|---|---|
| **Continuous timbre** (held knobs) | vowel, size, effort | the 3 macros (`harmonics`/`timbre`/`morph`) |
| **Per-note articulation** (timed events) | consonant onset, consonant coda | `voice_consonant()` / `voice_coda()` at note-on/off — like velocity, NOT a macro |
| **Modulators** (shared infra) | vibrato | `LFO_PITCH` via `note_lfo()` — not a voice param at all |

The consonants are deliberately **not** axes: they're transient, timed events, so they belong
in the note-trigger API, not a held slider. Vibrato is an orthogonal modulator every engine
already has. So "how many macros does the voice need?" is really only about the *continuous*
dimensions.

## The 3 continuous axes (current proposal)

| # | Axis | Engine macro | What it does |
|---|---|---|---|
| 1 | **VOWEL** | `harmonics` | U→O→A→E→I morph — *what's said* |
| 2 | **SIZE** | `timbre` | formant shift = vocal-tract length, giant→child — *who says it* |
| 3 | **EFFORT** | `morph` | one macro of breath + glottal open-quotient + spectral tilt — soft/breathy/dark → hard/pressed/bright — *how it's said* |

**Why effort is a macro-of-three:** breath, open-quotient and tilt all point the same
perceptual direction (soft↔hard); the `voxlab` probe confirmed they read as one gesture.
(That probe also caught a bug — the original tilt mapping went transparent at both extremes
and did nothing; fixed to a monotonic bright→dark LP. Open-quotient is the subtlest of the
three solo, which is itself evidence it belongs folded in.)

## Per-note articulation (the consonant layer)

- `voice_consonant(handle, id)` — **onset**: the note STARTS on a consonant that morphs into
  the vowel ("bah", "mah", "sss-ah"). Fired right after `note_on`.
- `voice_coda(handle, id)` — **coda**: the note CLOSES on a consonant as it releases
  ("ahh-m", "oo-d"). The mirror; fired right before `note_off`. Voiced ids keep it sung.
- 8 consonants (`b d g m n l s sh`): plosives (noise burst), nasals/liquid (low-F1 voiced),
  fricatives (noise that voices in). `vox`'s jam uses a mellow no-`s`/`sh` pool.

## OPEN QUESTION — is three continuous macros enough?

The live design tension. Three generic macros (`harmonics`/`timbre`/`morph`) is the engine
convention, and the voice already *exceeds* a pure 3-macro engine by adding the consonant
calls + the LFO. The question: do the continuous dimensions fit in three, or does the voice —
the most expressive engine on the shelf — deserve a **dedicated richer API** that breaks the
uniform 3-macro mould?

Candidates for a 4th+ continuous axis (each would have to earn it by ear):

- **Breath split out of effort** — intimacy/airiness independent of brightness. The least
  redundant member of the effort macro; the most likely 4th axis.
- **Nasality** — open ↔ hummed-through-nose (morph toward the nasal formant config). A
  distinctive color no current axis reaches.
- **2D vowel** (openness + frontness) — the full IPA vowel plane instead of a 1D U→I path.
  Costs *two* axes, leaving only one for everything else. Best vowel control, worst budget.
- **Diphthong target** — a second vowel the note glides toward ("ai", "ou") within one note.

## Enrichment ideas (same style — playable, cart-land first)

Things that would deepen the instrument without necessarily adding macros:

- **Glide/portamento between syllables** — legato scat instead of re-triggered steps on the
  jam pad (within-drag pitch glide, re-randomise only on a fresh press).
- **Choir / unison-detune** — 2–3 voices a few cents apart for the "aah" pad / gospel stack.
- **Diphthongs** — vowel→vowel morph within a held note (needs a target-vowel set or the
  diphthong axis above).
- **Mid-note consonants / clusters** — stepping toward real words ("la-la" → "hello"); the
  bridge to the TTS project.
- **Scat-rate / auto-rhythm** on the jam pad — tempo-locked syllable retrigger.
- **Vowel on the pad's Y axis** (instead of effort) — sing words by gesture.
- **Per-note random size** — a ragged kids'-choir / crowd texture.

## Prosody — the fourth kind of control (toward SPEAKING)

The three kinds above (continuous timbre / per-note articulation / modulators) cover *singing
one note*. **Speaking** needs a fourth: **prosody** — pitch, timing and stress shaped over a
whole *phrase*. It's where "is this a question?", "which word is emphasized?", and the melody of
a spoken sentence live. Crucially it is mostly **not** an engine concern — it's *sequencing*, so
per the recipe-first instinct ([ADR-0006](../decisions/0006-library-carts-not-engine.md),
[decision-0015](../decisions/0015-effects-are-recipes-not-primitives.md)) it should prove out in
cart-land before any new API. The `say` probe does exactly that.

**What `say` established by ear (2026-06-09), using ONLY existing primitives:**

- **Connected speech works on ONE held note.** Re-firing `voice_consonant()` on an
  already-held note resets `vox_cons_t`, so it re-articulates the consonant *mid-note* — turning
  the "one syllable per note" voice into connected syllable chains on a single `note_on`, no
  retrigger. `say`'s RANDOM now builds language-like simlish from a mix of **V / CV / VC / CVC**
  shapes (codas wired through `voice_coda()` fired in each syllable's back third), not a
  monotonous CV string. **So a dedicated mid-note-consonant API is not *required* to prototype
  speech** — the open question is whether the re-fired onset/coda morph *reads* as a true
  inter-vocalic / syllable-closing stop or needs a purpose-built one.
- **Pitch contour = per-frame `note_pitch()`** along a per-syllable shape (flat/rise/fall/peak)
  plus phrase-level declination. Intonation falls out as data: statement = final fall, question =
  final rise, exclaim = hard fall; **emphasis** = a syllable gets a pitch *accent* (peak) + longer
  duration. This is the "pitch in a word / question / emphasize a word" the design was reaching for.
- **Character** (the old terminal `say()` "vary character" lever) = a preset bundle of
  size/breath/open-q/tilt + pitch-offset + **pitch-RANGE** + rate. robot = zero range + no glide →
  monotone; kid = small+high+wide; giant = long+low+slow; whisper = breath maxed.

**The engine gaps the probe surfaces** (candidate real API, each must earn it by ear):

1. **Creak / jitter in the glottal source** — without it robot can't sound truly sterile and
   there's no old-man rasp / vocal fry. The lever that most "varies character"; cheap to add to the
   Rosenberg pulse. *Most likely first engine addition.*
2. **A fuller consonant set** — only `b d g m n l s sh` today; speech wants `h w r f v` and the
   unvoiced stops `p t k`. Each is a formant/noise table row.
3. **Schwa (reduced vowel)** — unstressed syllables collapse to it; without it everything
   over-enunciates. A trivial vowel row.
4. **Diphthong / vowel-glide target within a note** — "I", "oh", "now"; also makes *singing* far
   more natural. **Now cart-prototyped in `say`** (a per-frame `note_pitch`-style lerp of the
   VOWEL param toward a second vowel across the syllable) — so, like breath in `voxlab`, it's
   auditionable by ear; the open question is whether the cart-land lerp is enough or the engine
   wants a first-class diphthong target.

The contour/timing/stress layer itself stays a **cart recipe**; only if it proves universal does a
`voice_phrase()` convenience earn a place.

## Decision log

- **2026-06-09** — Built the `say` prosody probe (see §"Prosody" above). Confirmed by render
  that **connected speech runs on a single held note** via re-fired `voice_consonant()`, that a
  per-frame `note_pitch()` contour gives statement/question/emphasis intonation, and that
  character presets cover the terminal-`say()` "vary character" lever — all with **zero new
  engine API**. Prosody stays a cart recipe. Next engine lever to audition: **creak/jitter** in
  the glottal source (the one gap that's clearly kernel-only and most changes perceived character).
- **2026-06-09** — On "is 3 macros enough?": **defer the API shape; audition first.** Decide
  whether breath / nasality / 2D-vowel each earn their own continuous knob *by ear* in
  `voxlab`/`vox` before committing to either the lean (3-macro + events) or the richer
  (dedicated `voice_*`) API. Evidence before structure.
  - Already auditionable: **breath** is its own slider in `voxlab` (param 2) — move it
    independently of tilt/open-q to feel whether it deserves to leave the effort bundle.
  - Not yet auditionable (would need an experimental slider added to `voxlab`):
    **nasality** (no param yet) and **2D vowel** (vowel is currently a 1D U→I path).

## Next step

The listen-by-ear pass (in `vox`) decides the axes. Once locked: map the chosen continuous
axes onto the macros (or a dedicated voice API if we go richer), wire the four places
(`studio.h` / `studio.c` / `studioDocs.js` / `shell.js`), retire `voice_param`, and ship the
demo cart. Until then everything stays experimental.
