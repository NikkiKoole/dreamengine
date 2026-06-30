#!/usr/bin/env node
// publish-all.js — publish every cart that NEEDS it (or all of them) in one deploy.
//
// The batch companion to publish-cart.sh: instead of naming carts by hand, it
// asks cart-status.js what's out of date, (re)builds exactly those for the web,
// and pushes them in a SINGLE commit → one GitHub Pages deploy.
//
//   node tools/publish-all.js               # smart: not-published + stale-published carts
//   node tools/publish-all.js --engine-stale # ALSO republish instrument carts that predate
//                                            #   the latest runtime/sound.h commit (advisory)
//   node tools/publish-all.js --all          # every cart (build-site mtime-skips fresh ones)
//   node tools/publish-all.js --all --force  #   …and force-rebuild even the fresh ones
//   node tools/publish-all.js --dry-run      # print the plan; build/commit/push NOTHING
//
// SMART SELECTION comes straight from `cart-status.js --json`: notPublished +
// stalePublished (+ engineStale with the flag). "Stale" is git-time based — a
// commit touched the cart's source/thumbnail after its last site/ build. The
// smart set is always force-rebuilt (we already know it's stale); under --all,
// build-site's own mtime check skips fresh carts unless you pass --force.
//
// RESILIENT: carts build via build-site.js, which keeps going past a cart that
// fails to compile. Only carts that actually produced a site/<name>/ build get
// published; failures are reported and skipped — they don't block the rest. The
// git leg reuses publish-cart.sh (--no-build), so the same stray-file guard
// protects against committing another agent's WIP.
//
// LONG RUN: web builds are emcc per cart — a full --all is minutes to tens of
// minutes; smart mode usually has little to do. If you just touched studio.h,
// run build-all.js first (catches API rot cheaply before the slow emcc loop).
//
// This PUSHES to the live site, exactly like publish-cart.sh. Use --dry-run to
// preview the plan first.

'use strict'

const fs = require('fs')
const path = require('path')
const { execFileSync, spawnSync } = require('child_process')

const ROOT = path.join(__dirname, '..')
const CARTS_DIR = path.join(ROOT, 'tools', 'carts')
const SITE_DIR = path.join(ROOT, 'site')

const args = process.argv.slice(2)
if (args.includes('-h') || args.includes('--help')) {
  const out = []
  for (const l of fs.readFileSync(__filename, 'utf8').split('\n')) {
    if (l.startsWith('//')) out.push(l.replace(/^\/\/ ?/, '')); else if (out.length) break
  }
  console.log(out.join('\n'))
  process.exit(0)
}
const DRY = args.includes('--dry-run')
const ALL = args.includes('--all')
const FORCE = args.includes('--force')
const ENGINE = args.includes('--engine-stale')

function allCartNames() {
  return fs.readdirSync(CARTS_DIR)
    .filter(f => f.endsWith('.c') && !f.startsWith('_'))   // skip throwaway _scratch carts
    .map(f => f.slice(0, -2))
    .sort()
}

// ── pick the publish list ─────────────────────────────────────────────────────
let names, plan, reason
if (ALL) {
  names = allCartNames()
  plan = `--all: ${names.length} cart(s) (${FORCE ? 'force-rebuild all' : 'rebuild only changed'})`
  reason = 'all'
} else {
  let status
  try {
    const raw = execFileSync('node', [path.join(__dirname, 'cart-status.js'), '--json'], { encoding: 'utf8' })
    status = JSON.parse(raw)
  } catch (e) {
    console.error('publish-all: could not read cart-status.js --json\n' + (e.stderr || e.message || e))
    process.exit(1)
  }
  const set = new Set([...status.notPublished, ...status.stalePublished])
  if (ENGINE) for (const n of status.engineStale) set.add(n)
  names = [...set].sort()
  plan = `smart: ${status.notPublished.length} not-published + ${status.stalePublished.length} stale-published` +
    (ENGINE
      ? ` + ${status.engineStale.length} engine-stale`
      : (status.engineStale.length ? ` (+ ${status.engineStale.length} engine-stale NOT included — pass --engine-stale to add)` : ''))
  reason = 'smart'
}

console.log(`plan — ${plan}`)
if (names.length === 0) { console.log('✓ nothing to publish — everything is up to date'); process.exit(0) }
console.log(names.map(n => '  ' + n).join('\n'))

if (DRY) {
  console.log(`\n(dry run — ${names.length} cart(s) would be built + published; nothing done)`)
  process.exit(0)
}

// ── build (resilient: build-site keeps going past a failing cart) ──────────────
const force = ALL ? FORCE : true   // smart-selected carts are known-stale → always rebuild
const buildArgs = [path.join(__dirname, 'build-site.js'), ...names]
if (force) buildArgs.push('--force')
console.log(`\nbuilding ${names.length} cart(s) for web…`)
spawnSync('node', buildArgs, { stdio: 'inherit' })   // exit code ignored — verify by outputs below

const built = names.filter(n => fs.existsSync(path.join(SITE_DIR, n, 'index.html')))
const failed = names.filter(n => !built.includes(n))
if (failed.length)
  console.error(`\n⚠ ${failed.length} cart(s) did not build (skipped, not published):\n${failed.map(n => '  ' + n).join('\n')}`)
if (built.length === 0) { console.error('\npublish-all: nothing built — aborting before commit'); process.exit(1) }

// ── publish (reuse publish-cart.sh's safe commit/push; already built) ──────────
const msg = `site: publish-all — ${built.length} cart(s) (${reason}${failed.length ? `, ${failed.length} skipped` : ''})`
console.log(`\npublishing ${built.length} cart(s)…`)
const pub = spawnSync(path.join(__dirname, 'publish-cart.sh'), ['--no-build', '-m', msg, ...built], { stdio: 'inherit' })
process.exit(pub.status || 0)
