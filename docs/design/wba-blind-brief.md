# The Whitest Boy Alive — the blind band brief

> Phase-1 intent-first brief for an ARTIST radio station (`wba.c`), written from the music
> before reading any cousin cart (the firewall). Companion to the runbook
> [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md). This is an **artist
> station** — the dial plays recognizably different *songs of theirs*, via the stolen-playbook
> chord brain (#4) + song archetypes ([`../guides/game-music.md`](../guides/game-music.md)
> §"a stolen playbook" / §"Same song every night?"). Kin: `air`, `napoleon`, `polopan`.

## The record, from the music

The Whitest Boy Alive (Erlend Øye's band; *Dreams* 2006, *Rules* 2009). It *started* as Øye's
electronic project and became a live band — and that tension (machine precision + live warmth)
is the whole identity. Minimal, clean, groove-based indie. **RESTRAINT and SPACE are the point:
every instrument is dry and has room; nothing crowds.** Warm, major, a little melancholy.

The band (four players, each clearly audible — that's the aesthetic):
- **Clean electric guitar** (Erlend) — bright, *dry*, no distortion; palm-muted funk chops and
  single-note figures; the occasional slapback/spring on a lead. Telecaster-into-black-panel.
- **The bass is a CO-LEAD** (Marcin) — round, melodic, funky, fingered; it carries hooks, not
  just roots. Prominent in the mix. This is huge in WBA.
- **Tight, clean kit** (Sebastian) — minimal, precise, a steady disco / four-on-floor or
  hi-hat pulse; restrained, never busy.
- **Rhodes + a little analog synth** (Daniel) — warm EP comping, Juno-ish pad/organ stabs.
- **Soft tenor vocal** (Erlend) — understated, conversational, melancholic.

Harmony: warm major with soul/jazz colour (maj7, add9, ii–V-ish), simple loops — the *space*
is the sophistication, not the chords. Time: tight, ~108–125, the pocket is everything.

## The brains

- **STOLEN-PLAYBOOK CHORD BRAIN (#4) + SONG ARCHETYPES — the artist-station spine.** Encode
  4–6 cited tracks as template progressions, each bundled into a named ARCHETYPE that also fixes
  its groove / tempo / which instrument leads / form. The seed rolls the archetype; it varies
  key/pattern *within* it. Candidate archetypes:
  - **BURNING** — four-on-floor disco, driving, bright; guitar chops + the bass pumping. The hit.
  - **GOLDEN CAGE** — mid-tempo steady pulse, melancholic; the bass carries the hook, sparse guitar.
  - **1517** — bouncy, bright, up; interplay-forward.
  - **COURAGE** — slower, gentler, more space; Rhodes-forward.
  - **DONE WITH YOU / KEEP A SECRET** — funky, choppy palm-mute guitar + syncopated bass.
- **THE GUITAR↔BASS RESTRAINT DUET (the texture brain).** The signature move: clean palm-muted
  guitar and the melodic bass *interlock with deliberate gaps* — call-and-response, neither
  crowding. Fewer notes, dry, room. A "negative-space" arrangement rule (the opposite of a busy
  station): layers enter sparingly; the groove breathes.
- **THE TIGHT CLEAN POCKET.** A precise disco/motorik kit (machine-tight, ~0–2 ms), four-on-floor
  or steady-hat per archetype — the electronic-precision half of WBA.

## The voices (palette SHOPPED — Phase 2, incl. the pedalboard)

| imagined voice | engine + recipe (confirmed) | the pedalboard |
|---|---|---|
| **clean electric guitar** | `INSTR_GUITAR` `guitar/steel` (h0.55 t0.70 m0.20) | **the CLEAN amp voicing** via `ampcab.h` — `ampcab_apply(slot, 0, ~0.40, +2/-1/+2, 0.20)` (soft-clip, scooped-bright, light sag); **slapback** `instrument_echo(g, 0.12)`; **small warm room** `reverb(0.35,0.5)` + send 0.25; a touch of `instrument_chorus(g, 0.85, 0.25, 0.30)` |
| **melodic bass (co-lead)** | `INSTR_SAW`/`INSTR_SINE` round fingered, LP — prominent, the hook | dry; maybe light `glue` |
| **clean kit** | `INSTR_SINE` kick · `INSTR_NOISE` snare/hat — tight, minimal | dry, machine-tight |
| **Rhodes + synth** | `INSTR_EPIANO` (`epiano/rhodes`) + a Juno-ish `INSTR_SAW` pad/`INSTR_ORGAN` stab | `instrument_chorus` Rhodes width (0.7/0.30/0.28), gentle `tremolo` 5.4Hz |
| **soft vocal / topline** | `INSTR_VOICE` soft tenor, or a rounded synth lead | dry, close |
| **master glue** | — | `tape(0.10,0.07,0.22)` gentle analog cohesion + `glue(0, 0.20, 8, 150)` knits the band |

**The pedalboard, distilled (we have a deep one):** echo (slapback), reverb (small warm room),
chorus (subtle), tape (gentle glue), eq (presence), drive (`DRIVE_SOFT` barely-there) — and the
**amp/cab bundle** (`ampcab.h`, the `combo` cart's engine: CLEAN/CHIME/CRUNCH/HI-GAIN/LO-FI). WBA
wants **CLEAN only** — *no* distortion, *no* wah, *no* lo-fi. The restraint is the sound.
⚠ All these are **SET-AND-HOLD** — configure once / only on change (copy `groovebox`'s `apply_fx()`);
`filter()`/`varispeed()`/`note_*` are the only ride-live exceptions.

**No gap** — the engine + pedalboard cover WBA cleanly (clean electric is the steel recipe through
the CLEAN amp; the spring/room is `reverb` small-warm; slapback is the echo send).

## NOT SAMEY — the archetype roll + per-song timbre

1. **The ARCHETYPE (per seed) — the big lever.** Burning vs Golden Cage vs Courage are different
   *songs* (groove + tempo + key-mood + which instrument leads + form), not one texture re-keyed.
2. **Per-song timbre rolls** — guitar tone (a hair more/less bright via the amp gain + timbre),
   which instrument carries the hook, whether the Rhodes or guitar comps, kit four-on-floor vs hat.
3. **Key + the soul/jazz colour amount** (how many maj7/add9 leans).

## Player knobs (radio.h chrome)

- **feel** — arrangement density (the restraint dial: how much space).
- **tone** — overall brightness; **tempo**; SPACE re-rolls the archetype; **B** the band (guitar amp voicing, kit four-on-floor/hat).

## Window / face — Scandinavian minimal

WBA's aesthetic is stark and clean (Erlend's glasses, plain sleeves). Propose a **minimal stage**:
four dry instrument blocks (guitar / bass / drums / keys) lit only when each is playing, with lots
of black SPACE between them — the negative-space arrangement made visible. (Lineage: the mechanism
on screen; here the *restraint* is the picture.) A small glasses motif as the power LED.

## What this station adds (why it earns the slot)

- **First clean indie-BAND station** — the dry, spacious, groove-based texture nothing else has.
- **The guitar↔bass restraint duet** — a negative-space arrangement brain (interlock with gaps).
- **Exercises the clean `INSTR_GUITAR` + the amp/cab bundle + the tasteful pedalboard** (CLEAN amp,
  slapback, small-room verb, subtle chorus) — the first station to use the amp cab and a clean-guitar pedalboard.
- An **artist station** (stolen-playbook archetypes), kin to `air`/`napoleon`/`polopan`.

> **Phase-3 chassis hint** (not yet read): an artist station on the `radio.h` step clock — copy
> the *wiring* from `air`/`napoleon` (the archetype roll), `#include "ampcab.h"` for the guitar amp,
> and voice the band from this brief.
