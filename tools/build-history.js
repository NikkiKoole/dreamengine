#!/usr/bin/env node
// build-history.js — generate docs/history.html, a dedicated readable timeline
// of how the project grew. STRUCTURE comes from docs/history-spine.json (eras,
// subsystems, importance tiers, marked days, the editorial seam); every DATE,
// commit, ADR and cart-birth fact is derived from git + index.json so the page
// stays honest with the repo. Surfaced in the editor's Docs tab (a "history"
// sidebar item loads it in an iframe; doc links postMessage the parent to open
// the rendered markdown).
//
//   node tools/build-history.js            # regenerate docs/history.html
//
// v1 scope = the spine's window (week 1). Re-run after editing the spine.

const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')

const REPO = path.resolve(__dirname, '..')
const SPINE = path.join(REPO, 'docs', 'history-spine.json')
const INDEX = path.join(REPO, 'editor', 'public', 'carts', 'index.json')
const OUT = path.join(REPO, 'docs', 'history.html')

const git = (args) =>
  execFileSync('git', args, { cwd: REPO, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 })

// ---- load structure ----
const spine = JSON.parse(fs.readFileSync(SPINE, 'utf8'))
const carts = JSON.parse(fs.readFileSync(INDEX, 'utf8'))
const cartTitle = {}, cartDesc = {}
for (const c of carts) if (c.file) { cartTitle[c.file] = c.title || c.file; cartDesc[c.file] = c.description || '' }

const { from, to } = spine.window
// git --until is exclusive of the day boundary; bump one day past `to`
const untilExclusive = (() => {
  const d = new Date(to + 'T00:00:00Z'); d.setUTCDate(d.getUTCDate() + 1)
  return d.toISOString().slice(0, 10)
})()
const SINCE = `${from} 00:00`, UNTIL = `${untilExclusive} 00:00`

const dayOf = (iso) => iso.slice(0, 10)
const inWindow = (day) => day >= from && day <= to

// ---- gather commits in window (oldest first) ----
const SEP = '\x1f'
const rawCommits = git([
  'log', '--reverse', `--format=%H${SEP}%cI${SEP}%s`,
  '--since', SINCE, '--until', UNTIL,
]).split('\n').filter(Boolean).map((l) => {
  const [hash, iso, subject] = l.split(SEP)
  return { hash, iso, day: dayOf(iso), subject }
})

// ---- ADRs added in window ----
function addedInWindow(glob) {
  // newest-first; first sighting of each path is its most recent touch, but with
  // --diff-filter=A each path appears once at its add commit.
  const out = git(['log', '--diff-filter=A', '--format=C%cI', '--name-only', '--', glob])
  const res = []
  let day = null
  for (const line of out.split('\n')) {
    if (line.startsWith('C')) { day = dayOf(line.slice(1)); continue }
    const f = line.trim()
    if (!f || !day) continue
    if (inWindow(day)) res.push({ day, file: f })
  }
  return res
}
const adrs = addedInWindow('docs/decisions/*.md')
  .filter((a) => /\/\d{4}-/.test(a.file))
  .map((a) => ({
    day: a.day,
    rel: a.file.replace(/^docs\//, ''),
    name: path.basename(a.file),
    label: path.basename(a.file).replace(/\.md$/, '').replace(/-/g, ' '),
  }))
  .sort((x, y) => x.name.localeCompare(y.name))

// ---- carts born in window ----
const cartsBorn = addedInWindow('editor/public/carts/*.cart.png')
  .map((c) => {
    const file = path.basename(c.file)
    return { day: c.day, file, title: cartTitle[file] || file }
  })

// ---- design/guide docs born in window (for "docs written" per era) ----
// exclude the per-game cart-specs/ batch (the editor's docs sidebar hides them
// too) — they're build briefs, not design notes, and there are ~70 of them.
const docsBorn = [
  ...addedInWindow('docs/design/*.md'),
  ...addedInWindow('docs/guides/*.md'),
].filter((d) => !d.file.includes('cart-specs'))
  .map((d) => ({
    day: d.day,
    rel: d.file.replace(/^docs\//, ''),
    name: path.basename(d.file).replace(/\.md$/, ''),
  }))

// ---- per-day commit counts ----
const dayCount = {}
for (const c of rawCommits) dayCount[c.day] = (dayCount[c.day] || 0) + 1

// ---- the "hero" cart per era: whichever cart was committed-to MOST during the
// era's window (the thing being worked hardest right then) — its thumbnail goes
// in as a magazine pull-image, inlined as a data URI so the page is self-contained.
const CARTS_DIR = path.join(REPO, 'editor', 'public', 'carts')
function heroByEra() {
  // count SOURCE (.c) commits only — a cart's .cart.png gets bulk-rebaked/published
  // in sweeps that touch dozens at once, which would inflate the "worked on" signal.
  const out = git(['log', '--format=C%cI', '--name-only',
    '--since', SINCE, '--until', UNTIL, '--', 'tools/carts'])
  const tally = {}            // eraId -> { cartName -> commit count }
  let era = null, seen = null
  for (const line of out.split('\n')) {
    if (line.startsWith('C')) {
      const e = eraOf(dayOf(line.slice(1))); era = e ? e.id : null; seen = new Set(); continue
    }
    if (!era) continue
    const m = line.match(/tools\/carts\/([A-Za-z0-9_-]+)\.c$/)
    if (!m || seen.has(m[1])) continue   // count a cart once per commit
    seen.add(m[1])
    const t = (tally[era] ||= {}); t[m[1]] = (t[m[1]] || 0) + 1
  }
  const heroes = {}
  for (const eid of Object.keys(tally)) {
    let best = null
    for (const [name, n] of Object.entries(tally[eid])) {
      if (!fs.existsSync(path.join(CARTS_DIR, name + '.cart.png'))) continue
      if (!best || n > best.n) best = { name, n }
    }
    if (best && best.n >= 2) {
      const b64 = fs.readFileSync(path.join(CARTS_DIR, best.name + '.cart.png')).toString('base64')
      heroes[eid] = { name: best.name, commits: best.n,
        title: cartTitle[best.name + '.cart.png'] || best.name,
        dataUri: 'data:image/png;base64,' + b64 }
    }
  }
  return heroes
}
const heroes = heroByEra()

// ---- subsystem tagging ----
const subMatch = spine.subsystems.map((s) => ({
  id: s.id,
  needles: (s.match || []).map((m) => m.toLowerCase()),
}))
function subsystemsFor(subject) {
  const s = subject.toLowerCase()
  const hits = []
  for (const sm of subMatch) if (sm.needles.some((n) => s.includes(n))) hits.push(sm.id)
  return hits
}
for (const c of rawCommits) c.subs = subsystemsFor(c.subject)

// ---- assign commits to eras (by day) ----
function eraOf(day) { return spine.eras.find((e) => day >= e.from && day <= e.to) }
for (const c of rawCommits) { const e = eraOf(c.day); c.era = e ? e.id : null }

// ---- resolve milestones to dates (first matching commit) ----
const milestones = spine.milestones.map((m) => {
  const needle = m.match.toLowerCase()
  const hit = rawCommits.find((c) => c.subject.toLowerCase().includes(needle))
  return { ...m, day: hit ? hit.day : null, hash: hit ? hit.hash : null,
           matched: !!hit }
})
const missing = milestones.filter((m) => !m.matched)
if (missing.length) {
  console.warn('[build-history] WARNING — milestone matchers with no commit:')
  for (const m of missing) console.warn(`    "${m.title}"  (match: "${m.match}")`)
}

// ---- assemble per-era data ----
const eras = spine.eras.map((e) => {
  const days = []
  for (let d = new Date(e.from + 'T00:00:00Z'); dayOf(d.toISOString()) <= e.to; d.setUTCDate(d.getUTCDate() + 1)) {
    const day = dayOf(d.toISOString())
    days.push({
      day,
      count: dayCount[day] || 0,
      marked: spine.markedDays && spine.markedDays[day] ? spine.markedDays[day] : null,
      dense: (dayCount[day] || 0) >= (spine.denseDayThreshold || 9999),
      commits: rawCommits.filter((c) => c.day === day),
    })
  }
  return {
    ...e,
    days,
    commitCount: rawCommits.filter((c) => c.era === e.id).length,
    milestones: milestones.filter((m) => m.era === e.id),
    adrs: adrs.filter((a) => inWindow(a.day) && eraOf(a.day) && eraOf(a.day).id === e.id),
    docs: docsBorn.filter((d) => eraOf(d.day) && eraOf(d.day).id === e.id),
    cartsBorn: cartsBorn.filter((c) => eraOf(c.day) && eraOf(c.day).id === e.id),
    hero: heroes[e.id] || null,
  }
})

// title + description (from index.json) for every cart born in-window — feeds
// the hover card on the "carts born" lists
const cartMeta = {}
for (const c of cartsBorn) if (!cartMeta[c.file]) cartMeta[c.file] = { title: c.title, desc: cartDesc[c.file] || '' }

// group eras into weeks (by date) + per-week growth stats — the TOC reads off this
const weeks = (spine.weeks || []).map((w) => ({
  ...w,
  commits: rawCommits.filter((c) => c.day >= w.from && c.day <= w.to).length,
  cartsBorn: cartsBorn.filter((c) => c.day >= w.from && c.day <= w.to).length,
  eras: eras.filter((e) => e.from >= w.from && e.from <= w.to),
})).filter((w) => w.eras.length)

const data = {
  title: spine.title,
  subtitle: spine.subtitle,
  window: spine.window,
  generatedFrom: { commits: rawCommits.length, adrs: adrs.length, cartsBorn: cartsBorn.length },
  subsystems: spine.subsystems,
  cartMeta,
  weeks,
}

// ---- render HTML (CSS/JS consts are defined below; write at end of module) ----

// =====================================================================

function esc(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}

function renderHtml(d) {
  const json = JSON.stringify(d).replace(/</g, '\\u003c')
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(d.title)}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Archivo+Black&family=Space+Grotesk:wght@400;500;700&display=swap" rel="stylesheet">
<style>${CSS}</style>
</head>
<body>
<div id="root"></div>
<script>const DATA = ${json};</script>
<script>${JS}</script>
</body>
</html>
`
}

const CSS = `
/* neo-brutalism: thick black edges, hard offset shadows, flat bold colour,
   chunky display type — tuned to the editor's dark palette + pink accent,
   with orange + yellow carrying the importance tiers. */
:root{
  --bg:#1b1c1f; --panel:#22232a; --panel2:#2a2c34; --ink:#e7e9ee; --dim:#9094a2;
  --edge:#000; --pink:#ff6fb5; --orange:#ff7a2f; --yellow:#ffd23f;
  --on:#141414;                         /* text on bright fills */
  --sh:6px 6px 0 var(--edge);           /* the signature hard shadow */
  --sh-sm:4px 4px 0 var(--edge);
  --disp:"Archivo Black",Impact,system-ui,sans-serif;
  --body:"Space Grotesk",ui-sans-serif,system-ui,sans-serif;
  --mono:ui-monospace,"SF Mono",Menlo,monospace;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--body);
  font-size:14px;line-height:1.55;overflow-x:hidden;
  background-image:radial-gradient(var(--panel2) 1px,transparent 1px);
  background-size:22px 22px}
a{color:var(--ink);text-decoration:none}
#root{max-width:1000px;margin:0 auto;padding:0 24px 140px}

header.top{padding:44px 0 14px}
header.top h1{font-family:var(--disp);margin:0 0 12px;font-size:46px;line-height:.98;
  letter-spacing:-1px;text-transform:uppercase;color:var(--ink);
  text-shadow:4px 4px 0 var(--pink)}
header.top .sub{color:var(--ink);font-size:15px;max-width:680px;font-weight:500}
header.top .meta{display:inline-block;color:var(--on);background:var(--yellow);
  border:3px solid var(--edge);box-shadow:var(--sh-sm);font-size:12px;
  margin-top:16px;padding:6px 12px;font-weight:700;font-family:var(--mono);
  transform:rotate(var(--rot,0deg))}
header.top .meta b{font-family:var(--disp);font-weight:400}

.legend{display:flex;flex-wrap:wrap;gap:8px;margin:22px 0 4px;align-items:center}
.legend .lbl{color:var(--dim);font-size:12px;margin-right:2px;text-transform:uppercase;
  letter-spacing:.5px;font-weight:700}
.chip{display:inline-flex;align-items:center;gap:6px;background:var(--panel);
  border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);padding:3px 10px;
  font-size:12px;font-weight:600}
.chip .dot{width:9px;height:9px;border:2px solid var(--edge)}
.tier-foundational .dot,.dot.foundational{background:var(--pink)}
.tier-mid .dot,.dot.mid{background:var(--orange)}
.tier-small .dot,.dot.small{background:var(--yellow)}

nav.ribbon{position:sticky;top:0;z-index:5;background:var(--bg);
  border-bottom:3px solid var(--edge);
  display:flex;flex-wrap:wrap;gap:10px;padding:14px 0;margin:16px 0 8px}
nav.ribbon a{display:inline-flex;align-items:center;gap:8px;background:var(--panel);
  border:3px solid var(--edge);box-shadow:var(--sh-sm);padding:7px 13px;
  color:var(--ink);font-size:13px;font-weight:700;transition:transform .08s,box-shadow .08s}
nav.ribbon a:hover{transform:translate(-2px,-2px);box-shadow:6px 6px 0 var(--pink)}
nav.ribbon a:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--edge)}
nav.ribbon a .n{background:var(--pink);color:var(--on);border:2px solid var(--edge);
  padding:0 6px;font-family:var(--mono);font-size:11px;font-weight:700}

/* table of contents — the weeks listed up front, each with its growth bar +
   chapter chips (the chips reuse the ribbon look). The grouping IS the TOC. */
nav.toc{margin:18px 0 8px;border:3px solid var(--edge);box-shadow:var(--sh);background:var(--panel);padding:14px 16px 16px}
nav.toc .toc-title{font-family:var(--disp);text-transform:uppercase;font-size:13px;margin-bottom:12px;color:var(--ink)}
.toc-week{padding:14px 0;border-top:3px dashed var(--panel2)}
.toc-week:first-of-type{border-top:none;padding-top:2px}
.tw-head{display:inline-flex;align-items:baseline;gap:12px;flex-wrap:wrap;text-decoration:none}
.tw-label{font-family:var(--disp);font-size:20px;text-transform:uppercase;color:var(--on);
  border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);padding:2px 11px;line-height:1.15}
.tw-tag{font-style:italic;color:var(--ink);font-size:13.5px}
.tw-bar{margin:10px 0 6px;height:14px;background:var(--bg);border:2px solid var(--edge);max-width:520px}
.tw-bar span{display:block;height:100%;background:repeating-linear-gradient(45deg,var(--pink) 0 8px,var(--orange) 8px 16px)}
.tw-stats{font-family:var(--mono);font-size:11.5px;color:var(--dim);margin-bottom:10px}
.tw-stats b{color:var(--ink)}
.tw-eras{display:flex;flex-wrap:wrap;gap:9px}
.tw-eras a{display:inline-flex;align-items:center;gap:8px;background:var(--bg);border:3px solid var(--edge);
  box-shadow:var(--sh-sm);padding:6px 12px;color:var(--ink);font-size:13px;font-weight:700;
  transition:transform .08s,box-shadow .08s}
.tw-eras a:hover{transform:translate(-2px,-2px);box-shadow:6px 6px 0 var(--pink)}
.tw-eras a:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--edge)}
.tw-eras a .ec{background:var(--pink);color:var(--on);border:2px solid var(--edge);
  padding:0 6px;font-family:var(--mono);font-size:11px;font-weight:700}

/* week band — separates the weeks as you scroll the body */
section.week-band{margin:54px 0 0;scroll-margin-top:16px}
.wb-head{display:flex;align-items:baseline;gap:14px;flex-wrap:wrap}
.wb-head h2{font-family:var(--disp);font-size:40px;margin:0;text-transform:uppercase;color:var(--ink);letter-spacing:-1px}
.wb-range{font-family:var(--mono);color:var(--dim);font-size:13px;font-weight:700}
.wb-tag{font-style:italic;color:var(--ink);font-size:15px}
.wb-bar{margin:14px 0 8px;height:18px;background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh-sm);max-width:600px}
.wb-bar span{display:block;height:100%;background:repeating-linear-gradient(45deg,var(--pink) 0 10px,var(--orange) 10px 20px)}
.wb-stats{font-family:var(--mono);font-size:12px;color:var(--dim)}
.wb-stats b{color:var(--ink)}

section.era{padding:40px 0 8px;margin-top:24px;scroll-margin-top:78px}
.era-head{display:flex;align-items:center;gap:14px;flex-wrap:wrap}
.era-head h2{font-family:var(--disp);margin:0;font-size:27px;text-transform:uppercase;
  letter-spacing:-.5px;background:var(--pink);color:var(--on);
  border:3px solid var(--edge);box-shadow:var(--sh);padding:6px 14px;line-height:1.05}
.era-head .dates{font-family:var(--mono);color:var(--ink);font-size:13px;font-weight:700}
.era-head .cc{margin-left:auto;font-family:var(--mono);color:var(--on);
  background:var(--ink);padding:3px 9px;border:2px solid var(--edge);font-size:11px;font-weight:700}
.blurb{color:var(--ink);margin:16px 0 4px;max-width:780px;font-size:14.5px;font-weight:500}
.editorial{border:3px solid var(--edge);border-left-width:10px;border-left-color:var(--pink);
  box-shadow:var(--sh-sm);background:var(--panel);padding:12px 16px;margin:14px 0;
  color:var(--ink);font-style:italic;max-width:780px;transform:rotate(var(--rot,0deg))}

.era-intro{display:flex;gap:26px;align-items:flex-start;margin-top:16px;flex-wrap:wrap}
.intro-left{flex:1 1 320px;min-width:300px}
.era-intro .blurb{margin-top:0}
figure.hero{flex:0 0 224px;margin:0;background:var(--panel);border:3px solid var(--edge);
  box-shadow:var(--sh);padding:9px;transform:rotate(1.4deg);transition:transform .12s;cursor:pointer}
figure.hero:hover{transform:rotate(0deg) translate(-2px,-2px)}
figure.hero img{display:block;width:100%;image-rendering:pixelated;border:2px solid var(--edge);background:#000}
figure.hero figcaption{margin-top:9px;line-height:1.25}
figure.hero .hk{display:inline-block;background:var(--orange);color:var(--on);
  border:2px solid var(--edge);font-family:var(--mono);font-size:9.5px;font-weight:700;
  text-transform:uppercase;letter-spacing:.5px;padding:1px 6px;margin-bottom:5px}
figure.hero b{display:block;font-family:var(--disp);font-size:14px;text-transform:uppercase;letter-spacing:-.3px}
figure.hero .hc{font-family:var(--mono);color:var(--dim);font-size:10.5px}

.mstones{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:16px;margin:22px 0}
.ms{background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh);padding:13px 15px;
  transform:rotate(var(--rot,0deg));transition:transform .1s,box-shadow .1s}
.ms:hover{transform:rotate(0deg) translate(-2px,-2px);box-shadow:8px 8px 0 var(--edge)}
.ms.foundational{background:var(--pink);color:var(--on);grid-column:span 2}
.ms.mid{border-top:9px solid var(--orange)}
.ms.small{border-top:9px solid var(--yellow)}
.ms .date{font-family:var(--mono);font-size:11px;font-weight:700;opacity:.75;font-variant-numeric:tabular-nums}
.ms .t{font-weight:700;margin:4px 0 3px;font-size:15px;line-height:1.2}
.ms.foundational .t{font-family:var(--disp);font-weight:400;font-size:17px;text-transform:uppercase;letter-spacing:-.3px}
.ms .sx{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.4px;opacity:.7}
.ms.foundational .sx{opacity:.85}
.ms .nomatch{color:var(--orange);font-size:11px;font-weight:700}
@media(max-width:560px){.ms.foundational{grid-column:span 1}}

.row{margin:18px 0}
.row > .h{font-family:var(--disp);font-size:13px;text-transform:uppercase;letter-spacing:0;
  color:var(--ink);margin-bottom:9px}
.tags{display:flex;flex-wrap:wrap;gap:8px}
.tag{background:var(--panel);border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);
  padding:3px 10px;font-size:12px;font-weight:600;cursor:pointer;transition:transform .08s,box-shadow .08s}
.tag:hover{transform:translate(-1px,-1px);box-shadow:3px 3px 0 var(--yellow)}
.tag:active{transform:translate(2px,2px);box-shadow:0 0 0 var(--edge)}

details{background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh-sm);margin:16px 0;overflow:hidden;
  transform:rotate(var(--rot,0deg))}
details > summary{cursor:pointer;padding:11px 15px;list-style:none;font-size:13px;font-weight:700;
  display:flex;align-items:center;gap:10px;user-select:none;text-transform:uppercase;letter-spacing:.3px}
details > summary::-webkit-details-marker{display:none}
details > summary:hover{background:var(--panel2)}
details[open] > summary{border-bottom:3px solid var(--edge)}
details > summary .caret{transition:transform .12s;color:var(--pink)}
details[open] > summary .caret{transform:rotate(90deg)}
details > summary .grow{margin-left:auto;font-family:var(--mono);color:var(--on);
  background:var(--yellow);border:2px solid var(--edge);padding:1px 8px;font-size:11px;letter-spacing:0}
.detail-body{padding:6px 15px 15px}

.day{padding:12px 0;border-top:3px dashed var(--panel2)}
.day:first-child{border-top:none}
.day .dh{display:flex;align-items:center;gap:10px;font-size:13px}
.day .dd{font-family:var(--disp);font-size:15px}
.day .dc{font-family:var(--mono);color:var(--dim);font-size:12px}
.badge{font-size:11px;font-weight:700;padding:1px 8px;border:2px solid var(--edge);color:var(--on)}
.badge.dense{background:var(--orange)}
.badge.marked{background:var(--pink)}
.day .note{color:var(--ink);font-size:12.5px;font-style:italic;margin:5px 0 7px;
  border-left:6px solid var(--pink);padding-left:10px;font-weight:500}
ul.commits{margin:7px 0 0;padding:0;list-style:none}
ul.commits li{padding:2px 0 2px 18px;font-size:12.5px;color:var(--ink);position:relative}
ul.commits li:before{content:"▪";position:absolute;left:2px;color:var(--pink)}
ul.commits li .sx{color:var(--dim);font-size:11px;font-family:var(--mono)}

ul.carts{margin:7px 0 0;padding:0;list-style:none;columns:2;column-gap:28px}
ul.carts li{font-size:12.5px;padding:2px 0;break-inside:avoid}
ul.carts li[data-cart]{cursor:pointer}
ul.carts li[data-cart]:hover{color:var(--pink)}
ul.carts li .f{color:var(--dim);font-family:var(--mono);font-size:11px}
@media(max-width:560px){ul.carts{columns:1}}

.cart-preview{position:fixed;left:0;top:0;z-index:50;pointer-events:none;display:none;width:248px;
  background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh);padding:6px;transform:rotate(-1.6deg)}
.cart-preview.show{display:block}
.cart-preview img{display:block;width:100%;image-rendering:pixelated;border:2px solid var(--edge);background:#000}
.cart-preview img[src=""],.cart-preview img:not([src]){display:none}
.cart-preview .cp-meta{padding:7px 3px 2px}
.cart-preview .cp-t{display:block;font-family:var(--disp);font-size:13px;text-transform:uppercase;letter-spacing:-.2px;margin-bottom:4px}
.cart-preview .cp-d{display:block;font-size:11.5px;line-height:1.4;color:var(--dim);
  overflow:hidden;display:-webkit-box;-webkit-box-orient:vertical;-webkit-line-clamp:7}

.totop{position:fixed;right:18px;bottom:18px;z-index:60;display:none;width:44px;height:44px;
  background:var(--pink);color:var(--on);border:3px solid var(--edge);box-shadow:var(--sh-sm);
  font-family:var(--disp);font-size:20px;line-height:1;cursor:pointer;
  transition:transform .08s,box-shadow .08s}
.totop.show{display:block}
.totop:hover{transform:translate(-2px,-2px);box-shadow:6px 6px 0 var(--edge)}
.totop:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--edge)}
footer{color:var(--dim);font-size:12px;margin-top:60px;border-top:3px solid var(--edge);padding-top:18px}
footer a{color:var(--pink);font-weight:700}
`

const JS = `
const $ = (t, c, h) => { const e = document.createElement(t); if (c) e.className = c; if (h != null) e.innerHTML = h; return e }
const esc = s => String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
const subTitle = {}; DATA.subsystems.forEach(s => subTitle[s.id] = s.title)
const subTier  = {}; DATA.subsystems.forEach(s => subTier[s.id]  = s.tier)

// open a docs/ markdown file in the parent editor's Docs tab (postMessage),
// falling back to the raw route if we're not embedded.
function openDoc(rel){
  if (window.parent && window.parent !== window){
    window.parent.postMessage({ type:'open-doc', path: rel }, '*')
  } else {
    window.open('/docs/' + rel, '_blank')
  }
}

// ask the parent editor to load a cart (only works embedded in the editor)
function loadCart(file, title){
  if (window.parent && window.parent !== window){
    window.parent.postMessage({ type:'load-cart', file, title }, '*')
  } else {
    window.open('/carts/' + file, '_blank')
  }
}

function render(){
  const root = document.getElementById('root')

  const head = $('header','top')
  head.appendChild($('h1', null, esc(DATA.title)))
  head.appendChild($('div','sub', esc(DATA.subtitle)))
  const g = DATA.generatedFrom
  head.appendChild($('div','meta',
    '<b>'+DATA.window.label+'</b> · '+DATA.window.from+' → '+DATA.window.to+
    ' · derived from <b>'+g.commits+'</b> commits, <b>'+g.adrs+'</b> decisions, <b>'+g.cartsBorn+'</b> carts born'))
  root.appendChild(head)

  // subsystem legend
  const leg = $('div','legend')
  leg.appendChild($('span','lbl','subsystems —'))
  DATA.subsystems.forEach(s => {
    leg.appendChild($('span','chip tier-'+s.tier,
      '<span class="dot"></span>'+esc(s.title)))
  })
  root.appendChild(leg)
  const tl = $('div','legend')
  tl.appendChild($('span','lbl','tiers —'))
  ;[['foundational','foundational'],['mid','notable'],['small','everyday']].forEach(([k,lbl])=>
    tl.appendChild($('span','chip','<span class="dot '+k+'"></span>'+lbl)))
  root.appendChild(tl)

  // table of contents — one block per WEEK with its growth (commits/carts) + a
  // bar sized against the busiest week, then its chapters as jump links
  const maxC = Math.max(1, ...DATA.weeks.map(w => w.commits))
  const toc = $('nav','toc')
  toc.appendChild($('div','toc-title','Contents — week by week'))
  DATA.weeks.forEach((w, wi) => {
    const tw = $('div','toc-week')
    const head = $('a','tw-head'); head.href = '#'+w.id
    const lab = $('span','tw-label', esc(w.label)); lab.style.background = ERA_COLORS[wi % ERA_COLORS.length]
    head.appendChild(lab)
    if (w.tagline) head.appendChild($('span','tw-tag', esc(w.tagline)))
    tw.appendChild(head)
    const bar = $('div','tw-bar'); bar.innerHTML = '<span style="width:'+Math.round(w.commits / maxC * 100)+'%"></span>'
    tw.appendChild(bar)
    tw.appendChild($('div','tw-stats','<b>'+w.commits+'</b> commits · <b>'+w.cartsBorn+'</b> carts born · <b>'+w.eras.length+'</b> chapters'))
    const eras = $('div','tw-eras')
    w.eras.forEach(e => {
      const a = $('a', null, esc(e.title)+' <span class="ec">'+e.commitCount+'</span>')
      a.href = '#era-'+e.id
      eras.appendChild(a)
    })
    tw.appendChild(eras)
    toc.appendChild(tw)
  })
  root.appendChild(toc)

  // body — a week band, then that week's eras (era colours cycle continuously)
  let ei = 0
  DATA.weeks.forEach((w, wi) => {
    root.appendChild(renderWeekBand(w, maxC, wi))
    w.eras.forEach(e => root.appendChild(renderEra(e, ei++)))
  })

  const f = $('footer', null,
    'Generated by <code>tools/build-history.js</code> from <code>docs/history-spine.json</code> + git. '+
    'Structure is hand-authored; dates and lists are derived. Editorial prose follows '+
    '<a href="#" data-doc="guides/voice.md">guides/voice.md</a>.')
  root.appendChild(f)

  // doc links + click-a-cart-to-load
  document.addEventListener('click', ev => {
    const t = ev.target.closest('[data-doc]')
    if (t){ ev.preventDefault(); openDoc(t.getAttribute('data-doc')); return }
    const cart = ev.target.closest('[data-cart]')   // a carts-born entry OR a hero figure
    if (cart){ ev.preventDefault(); loadCart(cart.dataset.cart, cart.dataset.title || '') }
  })

  // give the panels a small, STABLE, hand-placed tilt (deterministic per index,
  // so the look doesn't churn between regenerations / reloads)
  let ti = 0
  document.querySelectorAll('.ms, details, .editorial, header.top .meta').forEach(el => {
    const r = Math.sin((ti + 1) * 99.13) * 1e4, f = r - Math.floor(r)  // 0..1
    el.style.setProperty('--rot', ((f - 0.5) * 2.2).toFixed(2) + 'deg') // ~±1.1°
    ti++
  })

  // hover a "carts born" entry → a card with its thumbnail (loaded on demand
  // from /carts/, one shared image so we never fetch 200 up front) + the cart's
  // description from index.json. Only resolves inside the editor dev server.
  const meta = DATA.cartMeta || {}
  const prev = $('div','cart-preview')
  prev.innerHTML = '<img alt=""><div class="cp-meta"><b class="cp-t"></b><span class="cp-d"></span></div>'
  document.body.appendChild(prev)
  const pimg = prev.querySelector('img'), pT = prev.querySelector('.cp-t'), pD = prev.querySelector('.cp-d')
  let curCart = ''
  document.addEventListener('mouseover', ev => {
    const li = ev.target.closest('li[data-cart]'); if (!li) return
    const f = li.dataset.cart
    if (f !== curCart){
      curCart = f; pimg.src = '/carts/' + f
      const m = meta[f] || {}
      pT.textContent = m.title || li.dataset.title || ''
      pD.textContent = m.desc || ''
      pD.style.display = m.desc ? '' : 'none'
    }
    prev.classList.add('show')
  })
  document.addEventListener('mouseout', ev => {
    const li = ev.target.closest('li[data-cart]'); if (!li) return
    const to = ev.relatedTarget && ev.relatedTarget.closest && ev.relatedTarget.closest('li[data-cart]')
    if (!to) prev.classList.remove('show')
  })
  document.addEventListener('mousemove', ev => {
    if (!prev.classList.contains('show')) return
    const pad = 18, w = prev.offsetWidth || 190, h = prev.offsetHeight || 140
    let x = ev.clientX + pad, y = ev.clientY + pad
    if (x + w > innerWidth)  x = ev.clientX - w - pad
    if (y + h > innerHeight) y = ev.clientY - h - pad
    prev.style.left = x + 'px'; prev.style.top = Math.max(8, y) + 'px'
  })

  // a tiny fixed "back to top" button, shown once you've scrolled down a bit
  const toTop = $('button','totop','↑'); toTop.title = 'back to top'
  document.body.appendChild(toTop)
  toTop.addEventListener('click', () => window.scrollTo({ top: 0, behavior: 'smooth' }))

  // persist + restore our own scroll. Two return paths, both handled here:
  //  · doc link → the iframe RELOADS → we restore on load (rAF + fonts.ready)
  //  · cart load → the iframe stays mounted but the host display:none's it
  //    (which zeroes scroll); the host posts 'restore-scroll' when the Docs tab
  //    is re-shown, and WE read back our own sessionStorage (the host can't —
  //    storage may be partitioned).
  const KEY = 'de-history-scroll'
  const getSaved = () => { try { return +sessionStorage.getItem(KEY) || 0 } catch (e) { return 0 } }
  const restoreScroll = () => { const y = getSaved(); if (y) requestAnimationFrame(() => requestAnimationFrame(() => window.scrollTo(0, y))) }
  restoreScroll()
  if (document.fonts && document.fonts.ready) document.fonts.ready.then(() => { const y = getSaved(); if (y) window.scrollTo(0, y) })
  window.addEventListener('message', e => { if (e.data && e.data.type === 'restore-scroll') restoreScroll() })
  let sraf = 0
  window.addEventListener('scroll', () => {
    toTop.classList.toggle('show', window.scrollY > 400)
    if (sraf) return
    sraf = requestAnimationFrame(() => { sraf = 0; try { sessionStorage.setItem(KEY, String(Math.round(window.scrollY))) } catch (e) {} })
  }, { passive: true })
}

const ERA_COLORS = ['var(--yellow)','var(--orange)','var(--pink)','#7ee0ff']

function renderWeekBand(w, maxC, wi){
  const s = $('section','week-band'); s.id = w.id
  const h = $('div','wb-head')
  const h2 = $('h2', null, esc(w.label)); h2.style.textShadow = '3px 3px 0 ' + ERA_COLORS[wi % ERA_COLORS.length]
  h.appendChild(h2)
  h.appendChild($('span','wb-range', w.from + ' → ' + w.to))
  if (w.tagline) h.appendChild($('span','wb-tag', esc(w.tagline)))
  s.appendChild(h)
  const bar = $('div','wb-bar'); bar.innerHTML = '<span style="width:' + Math.round(w.commits / maxC * 100) + '%"></span>'
  s.appendChild(bar)
  s.appendChild($('div','wb-stats', '<b>' + w.commits + '</b> commits · <b>' + w.cartsBorn + '</b> carts born · <b>' + w.eras.length + '</b> chapters'))
  return s
}

function renderEra(e, i){
  const s = $('section','era'); s.id = 'era-'+e.id
  const h = $('div','era-head')
  const h2 = $('h2', null, esc(e.title))
  h2.style.background = ERA_COLORS[i % ERA_COLORS.length]   // cycle colours across all weeks
  h.appendChild(h2)
  const dr = e.from === e.to ? e.from : e.from+' → '+e.to
  h.appendChild($('span','dates', dr))
  h.appendChild($('span','cc', e.commitCount+' commits'))
  s.appendChild(h)

  // intro: blurb + editorial on the left, the era's most worked-on cart as a
  // magazine pull-image on the right
  const intro = $('div','era-intro')
  const left = $('div','intro-left')
  if (e.blurb) left.appendChild($('div','blurb', esc(e.blurb)))
  if (e.editorial) left.appendChild($('div','editorial', esc(e.editorial)))
  intro.appendChild(left)
  if (e.hero){
    const fig = $('figure','hero')
    fig.dataset.cart = e.hero.name + '.cart.png'   // click → load this cart
    fig.dataset.title = e.hero.title
    fig.title = 'open ' + e.hero.title
    // size the thumb by how hard the cart was worked: ~165px → 320px, maxing
    // out around 40 commits in a stretch
    const W = Math.round(165 + Math.min(1, e.hero.commits / 40) * 155)
    fig.style.flexBasis = W + 'px'; fig.style.width = W + 'px'
    fig.innerHTML =
      '<img src="'+e.hero.dataUri+'" alt="'+esc(e.hero.title)+'">'+
      '<figcaption><span class="hk">most worked-on</span>'+
      '<b>'+esc(e.hero.title)+'</b>'+
      '<span class="hc">'+e.hero.commits+' commits this stretch</span></figcaption>'
    intro.appendChild(fig)
  }
  s.appendChild(intro)

  // milestones
  if (e.milestones.length){
    const grid = $('div','mstones')
    e.milestones.forEach(m => {
      const c = $('div','ms '+m.tier)
      c.appendChild($('div','date', m.day || ''))
      c.appendChild($('div','t', esc(m.title)))
      c.appendChild($('div','sx', m.subsystem ? esc(subTitle[m.subsystem]||m.subsystem) : ''))
      if (!m.matched) c.appendChild($('div','nomatch','⚠ no matching commit found'))
      grid.appendChild(c)
    })
    s.appendChild(grid)
  }

  // ADRs
  if (e.adrs.length){
    const r = $('div','row'); r.appendChild($('div','h','Decisions landed'))
    const tags = $('div','tags')
    e.adrs.forEach(a => {
      const t = $('span','tag', esc(a.label))
      t.setAttribute('data-doc', a.rel)
      tags.appendChild(t)
    })
    r.appendChild(tags); s.appendChild(r)
  }

  // design docs written
  if (e.docs.length){
    const r = $('div','row'); r.appendChild($('div','h','Design notes written ('+e.docs.length+')'))
    const tags = $('div','tags')
    e.docs.sort((a,b)=>a.name.localeCompare(b.name)).forEach(dd => {
      const t = $('span','tag', esc(dd.name))
      t.setAttribute('data-doc', dd.rel)
      tags.appendChild(t)
    })
    r.appendChild(tags); s.appendChild(r)
  }

  // carts born
  if (e.cartsBorn.length){
    const det = $('details')
    const sum = $('summary', null,
      '<span class="caret">▸</span> Carts born <span class="grow">'+e.cartsBorn.length+' cartridges</span>')
    det.appendChild(sum)
    const body = $('div','detail-body')
    const ul = $('ul','carts')
    e.cartsBorn.slice().sort((a,b)=>a.title.localeCompare(b.title)).forEach(c => {
      const li = $('li', null, esc(c.title)+' <span class="f">'+esc(c.file.replace(/\\.cart\\.png$/,''))+'</span>')
      li.dataset.cart = c.file    // hover → thumbnail; click → load the cart (both via /carts/)
      li.dataset.title = c.title
      ul.appendChild(li)
    })
    body.appendChild(ul); det.appendChild(body); s.appendChild(det)
  }

  // day-by-day beat (collapsible)
  const det = $('details')
  const markedN = e.days.filter(d=>d.marked).length
  const sum = $('summary', null,
    '<span class="caret">▸</span> The day-by-day beat'+
    '<span class="grow">'+e.days.length+' days'+(markedN?' · '+markedN+' marked':'')+'</span>')
  det.appendChild(sum)
  const body = $('div','detail-body')
  e.days.forEach(day => {
    const dv = $('div','day')
    const dh = $('div','dh')
    dh.appendChild($('span','dd', day.day))
    dh.appendChild($('span','dc', day.count+' commits'))
    if (day.marked) dh.appendChild($('span','badge marked','★ marked'))
    if (day.dense)  dh.appendChild($('span','badge dense','🔥 dense'))
    dv.appendChild(dh)
    if (day.marked) dv.appendChild($('div','note', esc(day.marked)))
    const ul = $('ul','commits')
    day.commits.forEach(c => {
      const sx = c.subs.length ? ' <span class="sx">['+c.subs.map(id=>esc(subTitle[id]||id)).join(', ')+']</span>' : ''
      ul.appendChild($('li', null, esc(c.subject)+sx))
    })
    dv.appendChild(ul)
    body.appendChild(dv)
  })
  det.appendChild(body); s.appendChild(det)

  return s
}

render()
`

// ---- write ----
fs.writeFileSync(OUT, renderHtml(data))
console.log(`[build-history] wrote ${path.relative(REPO, OUT)}`)
console.log(`  ${data.window.label}: ${data.weeks.length} weeks, ${data.weeks.reduce((n, w) => n + w.eras.length, 0)} eras, ${rawCommits.length} commits, ${adrs.length} ADRs, ${cartsBorn.length} carts born`)
