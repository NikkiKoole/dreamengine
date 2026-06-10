# Afrobeat — the voices I wanted but can't have yet (blocked on the effects bus)

The blind-band brief for the **Afrobeat** station (Fela Kuti / Tony Allen) named an ideal
lineup *from the genre*, before reading any cousin cart — the intent-first discipline in
[`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md). Shopping that
brief against the [instrument palette](../guides/instrument-recipes.md) had a happy result —
the genre's signature **timbres** map almost perfectly onto modeled engines we already have
(`INSTR_GUITAR`, `INSTR_REED`, `INSTR_BRASS`, `INSTR_ORGAN`, `INSTR_MEMBRANE`). But several
voices came up short, and they're short for the *same single reason*: **we have no effects
bus yet.** The 70s Afrobeat sound isn't just instruments — it's instruments *in a room,
through an amp, onto tape, with the rhythm guitar through a wah pedal.* That layer is missing.

> **Genre: design exploration.** A wishlist tied to one station's blind brief, not a bug
> list. The shipped `afrobeat.c` works around every hole below (notes in each row); this doc
> records what the cart *would* be if the effects bus existed — and, more usefully, it's a
> fresh **demand witness** for that bus: Afrobeat wants nearly the entire rostered chain.
> Companion docs: the bus is **designed, not built** in
> [`instrument-engines.md`](instrument-engines.md) § 8.10, disciplined by
> [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md); the sequenced
> build-list (effect → showcase cart → stations it rescues) is in
> [`sound-next-steps.md`](sound-next-steps.md) § "Each effect → its showcase cart". The
> shipped-station gap ledger is [`radio-genre-fidelity.md`](radio-genre-fidelity.md); this is
> its forward-looking sibling for a station being built.

---

## The wants, each mapped to the effect that would unblock it

The right column is the **§8.10 / build-list effect** that delivers it — so this table is
also "which bus effect does Afrobeat vote for, and why." The build-list already names some of
these rescues for *other* stations; Afrobeat is a second (or third) vote for the same boxes.

| want (the voice the genre asks for) | why it's short today | unblocked by |
|---|---|---|
| **Wah / envelope-filter rhythm guitar** — the *talking*, quacking funk guitar (the "Gentleman" chop). The single most identifiable Afrobeat guitar move. | `INSTR_GUITAR` gives the string + body, but the **pedal** is an effect we can't send to. Faked in-cart with one swept `FILTER_BAND` peak (the mouthharp / epiano-clav stopgap) — a hint of quack, not the pedal. | **wah / auto-wah** (build-list already lists this rescuing *citypop's* funk guitar — Afrobeat is the second vote) |
| **Band-in-a-room glue (reverb)** — every 70s Afrobeat side is a live band in one room; the ambience *is* the sound. | No reverb at all. Nothing fakes a room convincingly; the cart plays dry. | **reverb** (§8.10 #1, bus-only — "the biggest") |
| **Tape saturation / analog warmth** — Afrika 70 on tape through a console: soft compression, harmonic warmth, the glue that makes the horns and guitars one band. | No tape/saturation stage. The mix is clean and digital where it wants to be warm and slightly squashed. | **tape (wow/flutter/sat)** (build-list → Frippertronics showcase) |
| **Horn-section width** — a real horn line is 3–5 players slightly apart in tune and time. We stack 2 modeled horns (`reed/tenor_sax` + `brass/trumpet`); they sound like 2 players, not a section. | No chorus / unison-width. Partly faked in-cart by **detuned + slightly-delayed layering**, but that's a stand-in for the real ensemble spread. | **chorus / unison width** (§8.10 #2; build-list → Juno) |
| **Organ that's actually there** — the combo organ/B3 comp is thin without its **Leslie**, tube grit, and room. *(Raised in review: "organ is not really there yet without Leslie.")* | The Leslie *rotor* is modeled (organ.c `leslie/slow`·`leslie/fast`) but only as a per-note layered recipe; the radio chassis voices a clean organ. Without the rotary swirl + amp drive + room it reads as a static drawbar, not a Hammond. | **leslie (rotary)** (build-list → "Hammond B3 + Leslie" showcase, also rescues roadhouse/yacht) **+ drive + reverb** |
| **Electric bass that sits in the pocket** — fingered electric through an amp, *compressed*, round and even. *(Raised in review: "electric bass ... is not really there yet.")* | A clean modeled/synth bass is thin and dynamically uneven; the genre's bass is an *amped, compressed* instrument. We have neither amp character nor compression. | **drive (amp)** + **compression** (compression is **not yet rostered** — see below) |

## The gap in the gap: compression isn't even on the bus roster yet

Two of the wants above (the even funk-guitar dynamics, the bass sitting in the pocket) and the
genre's overall *pumping glue* need **compression / sidechain** — and unlike wah/reverb/leslie,
compression is **not** in the §8.10 effect list or the build-list. The dance stations fake a
sidechain pump by re-aiming filters per frame (house's filter ride), but that's a per-voice
trick, not a dynamics processor. **Flagging it here as a roster candidate**: an Afrobeat (or any
live-band) station is the natural demand for it, and its showcase cart writes itself (a
**dbx/1176-style compressor pedal** with a visible gain-reduction meter — the same effect →
showcase-cart flywheel). Worth a line in `sound-next-steps.md`'s roster if it earns a vote
beyond this one.

## Not-effects (named so the doc isn't mistaken for complete)

Two more holes the brief found that are **not** the effects bus's job — recorded so a future
reader doesn't expect the bus to close them:

- **Call-and-response vocals** — Fela's lead call + the female chorus answer. `INSTR_VOICE`
  exists and is maturing (the `vox` carts), but *intelligible* sung words are beyond what the
  generative chassis does; this is a brain/lyric problem, not an effect. The cart leaves the
  vocal chair empty (or, at most, a wordless `INSTR_VOICE` shout as the response).
- **Tony Allen's broken-funk feel** — the human drummer's off-grid ghost notes and elastic
  swing. That's a **TIME/feel brain** (timing scatter + dynamic accenting), not an effect; it
  belongs with the generative-brain backlog in [`future-stations.md`](future-stations.md), not
  here.

---

## What this says about the effects bus

One station's blind brief just voted for **wah, reverb, tape, chorus, leslie, drive** — six of
the rostered bus effects — *plus* surfaced **compression** as a missing roster entry. That's the
strongest single-station demand signal for the bus so far, and it lands exactly where § 8.10
predicted the value is: the bus doesn't just add polish, it's the difference between "a stack of
modeled instruments" and "a band." When the bus ships, `afrobeat.c` is a ready acceptance test —
nearly every effect has a job to do in it.

*(Pairs with the cart `tools/carts/afrobeat.c` and its `radio-voices.md` chart once shipped.)*
