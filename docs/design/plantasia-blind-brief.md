# Plantasia — the blind band brief

STATUS: SHIPPED — the plantasia.c radio station is built (melody-forward Moog, growing houseplants).

> Phase-1 intent-first brief for a new radio station (`plantasia.c`), written from
> the music before reading `moog.c` or any cousin cart (the firewall). Companion to
> the runbook [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md)
> and the candidate entry in [`future-stations.md`](future-stations.md) (the
> melody-forward station — first where the LEAD is the protagonist — + the growing-
> houseplant face). Sibling brief: [`eno-blind-brief.md`](eno-blind-brief.md).

## The record, from the music

*Mother Earth's Plantasia* (Mort Garson, 1976) — "warm earth music for plants… and
the people who love them," sold with houseplants. Played **entirely on a Moog
modular** — no acoustic instrument anywhere. The feel is light, bouncy, whimsical,
pastoral, bright-major; library-music / exotica-adjacent but all-synth. Each track is
a little song *for a specific plant* ("Symphony for a Spider Plant," "Ode to an
African Violet," "Concerto for Philodendron & Pothos," "A Mellow Mood for Maranta").

The defining facts:

- **The melody is the star.** A mono Moog lead carries a clear, hummable, almost
  nursery-rhyme tune, with **portamento glide** between notes and **vibrato**. Unlike
  every station so far, the lead is not buried in the arrangement — it *is* the song.
- **A springy, plucky bass.** A bouncy Moog bassline (walking or arpeggiated), each
  note with that resonant filter-envelope "pluck" wow — the bounce is the pocket.
- **Burbling sequencer arps.** Percolating analog-sequencer patterns loop under the
  tune (the Moog modular's bubbling signature).
- **Glassy bell/celesta accents** dab counter-melodies and sparkles.
- **A soft warm pad/chord bed** under it (not on every track).
- **Light, optional percussion.** Many tracks float free or are lightly metered; some
  have a gentle synthetic woodblock/click/soft-kick lilt. Rhythm is never the driver.
- **Bright diatonic major harmony with vaudeville chromatic passing chords** — songlike
  (I–vi–ii–V, secondary dominants, chromatic approaches), the occasional whimsical
  key-lift. Never dark.

## The brains

- **MELODY = the headline (a NEW brain — melody brain #3).** Plantasia is the first
  station whose LEAD is the protagonist, so it needs a **seeded theme-and-variation
  generator**: a short singable motif with phrase structure (antecedent → consequent, a
  returning hook), *developed* across the form — stated, varied, sequenced, answered,
  recapped. **Why not just `improv.h`:** that brain is PERFORMANCE-only (runs on `rnd()`,
  a fresh motif every playthrough — "new every time") and can't be PINNED, which would
  break the seed-is-the-song contract. Plantasia's tune must be the song's *identity*:
  same hook on every replay of a seed. So the motif is invented on `rad_srnd` (the seed),
  and `improv.h`'s develop-ops (state / invert / sequence / answer-low) are reused but
  driven by that seeded motif instead of rnd. Performance dressing — portamento glide +
  vibrato on the mono Moog — stays on engine `rnd()`. **This brain is the cart's real
  contribution** (reusable by Reich, J-fusion, any tune-forward station).
- **CHORD = bright functional major + vaudeville color.** Diatonic changes that serve a
  tune (not a vamp): I–vi–ii–V, secondary dominants, chromatic passing/approach chords,
  a whimsical modulation up a step for the final return. Songlike, never modal-static.
- **FORM = songform.** A–A–B–A / theme → variation → bridge → return, short (2–3 min),
  with the final-section key-lift as a recurring whimsy. (The opposite of eno/ambient's
  endless drift — Plantasia tracks are little *songs* that end.)
- **TIME FEEL = light, rolled per song.** free/rubato · gentle waltz (3/4) · bouncy
  2-feel · soft bossa-ish lilt. The staccato bass bounce defines the pocket. Metered
  (so: the `radio.h` step clock, unlike beatless eno).

## The voices (palette mapping — to confirm in Phase 2)

| imagined voice | likely engine (confirm Phase 2) | notes |
|---|---|---|
| **lead Moog (protagonist)** | `INSTR_SAW` → resonant ladder LP + glide + vibrato — **your new Moog cart's signal path** | the star; mono, portamento, the hook. This is where your Moog lands. |
| **springy bass** | `INSTR_SAW`/pulse → resonant LP + per-note `ENV_CUTOFF` pluck | bouncy staccato; the filter-env "boing." |
| **burble / sequencer arp** | resonant `INSTR_SAW`/`INSTR_PD` | percolating loop under the tune. |
| **bell / celesta** | `INSTR_SINE`/`INSTR_FM`/`INSTR_MALLET` | glassy sparkle accents. |
| **warm pad bed** | `INSTR_SAW` string-machine | soft chord bed (not every track). |
| **light kit (optional)** | synth woodblock/click/soft-kick | per-song roll; rhythm never leads. |

Everything is **gloss-native** — our oscillators do Moog (saws + resonant filter)
natively, so this is one of the highest engine-fit stations possible. The whole band is
basically your Moog voiced four ways.

## NOT SAMEY — lean on the TRACK FEELS (the variety spine)

The variety lever is a set of **named feel templates** drawn from the actual album tracks —
the seed rolls one, and it bundles tempo + meter + groove + lead articulation + density +
chromaticism into one coherent character (the way air/jingle bundle song archetypes, but
for *feels* rather than cited chord-charts). Each is named for the track that anchors it:

| feel | anchor track | tempo / meter | character |
|---|---|---|---|
| **SPRIGHTLY** | "Symphony for a Spider Plant" | ~132, straight 2 | perky bouncing staccato bass, quick bright lead, busy arp |
| **SWING** | "Swingin' Spathiphyllums" / "Baby's Tears Blues" | ~120, shuffle | jazzy lilt, walking-ish bass, bluesy lead inflection |
| **WALTZ** | "A Mellow Mood for Maranta" | ~108, 3/4 | gentle lilting waltz, lighter kit, rounded lead |
| **RUBATO** | "Ode to an African Violet" | slow, free | expressive glide lead, sparse, no kit — the ballad |
| **GREEN** | "Rhapsody in Green" | ~100, bossa-lilt | soft clave groove, relaxed, warm pad bed |

On top of the rolled feel:
1. **Per-song Moog timbre rolls** — lead waveform, filter cutoff/res, glide amount, vibrato
   depth, bass pluck snap (the per-song timbre roll, eno/gamelan-style).
2. **Key + chromatic-color amount** — how much vaudeville chromaticism this song uses.
3. **The plant** — the seed also rolls which plant grows in the window (decoupled from the
   feel, or loosely tied: the rubato ballad tends toward the violet, etc.).
4. **Form + the final key-lift** — present or not, how far.

Within a song the seeded theme develops (variation, sequence, the bridge, the recap, the
lift) so it never just repeats — the songform itself is the anti-static engine.

## Player knobs (radio.h chrome)

- **tone** — Moog brightness (filter cutoff across the band).
- **tempo** (UP/DOWN) — the bounce rate.
- **feel/intensity** — arrangement layers (lead+bass → +arp → +bells/pad → +kit).
- **SPACE/tune** — new seed (new plant). Optional **B-panel**: pick the lead waveform / the plant.

## Window / face — the growing houseplant

The signature: a **generative houseplant in a pot that GROWS as the piece plays** —
sprouts on the intro, unfurls leaves through the body, opens a flower at the outro;
the species is the seed-rolled plant, so a fresh plant grows every song. The seed sits
on a little nursery price-tag / plant label. (Lineage: tango's bellows / carlos's
lattice / eno's loop-field — the station's identity made visible, here as the thing the
music is *for*.)

## What this station adds (why it earns the slot)

- **First melody-forward station** — the lead as protagonist; the theme-and-variation
  melody brain (promoting `improv.h` from soloist to frontman).
- **Your Moog, four ways** — the cleanest possible engine fit; the lead/bass/arp/pad are
  all one resonant-Moog voice re-voiced (palette shop confirms the exact signal path).
- **The growing-houseplant face** — a generative visual tied to song-time.
- **Phase-3 chassis hint** (not yet read): a metered, improviser-driven cousin
  (`cocktail`/`motorik` run `improv.h` on the `radio.h` step clock) — copy the *wiring*,
  promote the soloist to frontman, voice the band from this brief.
