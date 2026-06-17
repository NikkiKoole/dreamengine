# the project history page (`docs/history.html`)

A generated, readable timeline of how dreamengine grew — a neo-brutalist "magazine"
view grouped **week by week**, opened from the editor's **Docs tab → ★ history**. It
exists so the whole arc (what API landed when, which carts it spawned, which subsystems
belong together, what mattered more than what) is legible at a glance instead of buried
in 1400+ commits.

It is **structure-first**: a small hand-authored file supplies the *judgment* (chapters,
groupings, importance, the marked days, an editorial seam), and a generator derives every
*fact* (dates, commit counts, ADRs, carts born, the per-era hero cart) from git +
`index.json`. So it stays honest with the repo and barely rots.

## The three pieces

| File | Role |
|---|---|
| [`docs/history-spine.json`](../history-spine.json) | **hand-authored structure** — weeks, eras, subsystems + their commit-matchers, importance tiers, marked days, milestones, and the (mostly empty) editorial seam. The only file you edit by hand. |
| [`tools/build-history.js`](../../tools/build-history.js) | the **generator** — reads the spine + git + `index.json`, emits `docs/history.html`. Plain node. |
| `docs/history.html` | the **generated page** (self-contained: inline CSS/JS, fonts from Google Fonts, hero thumbnails inlined as data URIs). Don't hand-edit — regenerate. |

Editor wiring (so it shows up): a `.html` content-type case in `editor/vite.config.js`,
and the **★ history** sidebar item + iframe + cart-loading + scroll-memory in
`editor/src/shell.js` / `shell.css`.

## Regenerate

```bash
node tools/build-history.js          # rewrites docs/history.html
```

Re-run after editing the spine (or to refresh derived data — new commits/carts/heroes).
The page is served fresh by the dev server; reopen **★ history** to reload the iframe.

## What's derived vs authored

- **Derived (never hand-write):** every date, the per-day and per-week commit counts, the
  growth bars, ADRs landed per era, carts born per era, and the **hero cart** (the cart
  with the most *source* `.c` commits in that era's window — sized bigger the more it was
  worked). Subsystem *tags* on each commit come from the matchers in the spine.
- **Authored (the spine):** the week boundaries + taglines, the era titles + date ranges +
  blurbs, the subsystem list + matchers + tiers, the marked-day callouts, and the
  milestone cards (each a title + tier + a `match` substring that the generator resolves to
  a real commit's date — a build warning prints if a matcher hits nothing).
- **The editorial seam:** each era has an `editorial` field, empty by default. Fill it with
  voiced prose (following [`voice.md`](voice.md)) when you want a chapter to read in
  Nikki's voice; leave empty and the neutral `blurb` carries it.

## Adding a new week

Once a week's worth of work has landed, extend the spine and regenerate:

1. **Widen the window** — `window.to` → the new end date; bump `window.label` (e.g.
   `"Weeks 1–4"`).
2. **Add the week** to `weeks[]` — `{ id, label, from, to, tagline }`. (Empty weeks are
   skipped, so you can pre-declare future weeks harmlessly.)
3. **Reconstruct its eras** from git — the quick way:
   ```bash
   git log --reverse --format="%ad %s" --date=format:"%m-%d %H:%M" \
     --since="<weekstart> 00:00" --until="<weekend+1> 00:00"
   ```
   Read the arc, carve it into ~4 eras, add them to `eras[]` (each `{ id, title, from,
   to, blurb }` — date ranges must tile the week with no gaps).
4. **Add milestones** for the prominent landings — `{ title, tier, era, subsystem?, match }`
   where `match` is a unique substring of the real commit subject. Foundational tier = the
   big filled cards.
5. **Add marked days** for the standout days (`markedDays[date] = "why it mattered"`); days
   over `denseDayThreshold` also auto-get a 🔥 badge.
6. **New subsystems?** Add to `subsystems[]` with matchers if the week introduced a new
   through-line (that's how `radio`, `effects`, `roads` got added).
7. `node tools/build-history.js` and check the console for any unmatched-milestone warnings.

> Heroes and growth stats compute themselves — you only describe the *shape*.

## Possible automation (not built)

The derived half could be refreshed automatically (a hook that re-runs the generator on
commit, or a scheduled "scaffold next week" that pre-adds an empty week entry when a new
week boundary passes). The **eras, milestones and blurbs still need human judgment**, so a
fully-automatic "add a week" can't write good chapters on its own — the realistic
automation is *keep the derived data fresh + scaffold the empty week for a human to fill*.
Not wired up yet; see the conversation that built this.
