#!/usr/bin/env node
// profile-fleet.js — batch CPU profile a set of carts and aggregate where the ENGINE
// spends its time across the fleet. Answers "which studio.c primitive is the hottest
// fleet-wide" → the next engine-level optimization target (see
// docs/guides/engine-optimization.md → "Fleet survey").
//
// Runs each cart headless under play.js (so perf.json's in-engine counters are written)
// and collects §1 (workMsAvg/max) + §3 (draw-call counts/frame). NOT §2 — the macOS
// `sample` call-graph needs the editor's ⏱ profile button (or a manual `sample` attach);
// see profiler.md. So this measures COST + call FREQUENCY, not per-function time. Counts
// UNDER-state large software fills (one circfill can be thousands of pixels), so treat a
// low-count/high-ms cart as a "pull §2 on this one" flag.
//
//   node tools/profile-fleet.js                 # the default graphics-heavy set
//   node tools/profile-fleet.js roadlab lotfill # a specific list
//   node tools/profile-fleet.js --frames 300 ...
//
// Software per-pixel primitives (the span-fill optimization candidates) are flagged *.
// trifill/polyfill/ngonfill/starfill already share the optimized poly_fill_cov.

const { execSync } = require('child_process');
const fs = require('fs');

const SOFT = new Set(['pset','circfill','ovalfill','rrectfill','arcfill','ring','thickline',
                      'trifill','polyfill','ngonfill','starfill']);

// a broad graphics-heavy default set (procedural worlds, 3D, paint, games, instruments)
const DEFAULT = ['lotfill','flyover','modrack','qbert','pet','dungeonkeeper','galerijflat',
  'digdug','oersoep','neonrain','dpaint','zoo','sloop','skystrike','pizzatycoon','shapes',
  'orbit','opwolf','loveparade','interiors','chess','bubblebobble','wintergames','trafficjam',
  'gnomeseek','roadlab','solid3d','doom','podracer','polystress','pinball','bones'];

const args = process.argv.slice(2);
let frames = 180;
const fi = args.indexOf('--frames');
if (fi >= 0) { frames = parseInt(args[fi+1], 10); args.splice(fi, 2); }
const carts = args.length ? args : DEFAULT;

const results = [];
for (const c of carts) {
  try { fs.rmSync('build/perf.json', { force: true }); } catch {}
  try {
    execSync(`node tools/play.js ${c} script /dev/null --headless --frames ${frames}`,
             { stdio: 'ignore', timeout: 90000 });
    const p = JSON.parse(fs.readFileSync('build/perf.json', 'utf8'));
    const calls = {}; (p.calls || []).forEach(k => calls[k.name] = k.total / p.frames);
    results.push({ c, avg: p.workMsAvg, max: p.workMsMax, calls });
    process.stderr.write(`ok ${c} ${p.workMsAvg.toFixed(2)}ms\n`);
  } catch (e) { process.stderr.write(`SKIP ${c} (${(e.message || '').slice(0, 40)})\n`); }
}

results.sort((a, b) => b.avg - a.avg);
console.log('\n=== PER-CART (sorted by CPU, * = software per-pixel) ===');
console.log('cart'.padEnd(16), 'avg'.padStart(7), 'max'.padStart(7), '  top draw calls/frame');
for (const r of results) {
  const top = Object.entries(r.calls).sort((a, b) => b[1] - a[1]).slice(0, 4)
    .map(([n, v]) => `${n}=${Math.round(v)}${SOFT.has(n) ? '*' : ''}`).join(' ');
  console.log(r.c.padEnd(16), r.avg.toFixed(2).padStart(7), r.max.toFixed(2).padStart(7), '  ' + top);
}

const agg = {}, used = {};
for (const r of results) for (const [n, v] of Object.entries(r.calls)) {
  agg[n] = (agg[n] || 0) + v; used[n] = (used[n] || 0) + 1;
}
console.log(`\n=== AGGREGATE primitive usage (sum calls/frame across ${results.length} carts) ===`);
console.log('primitive'.padEnd(14), 'sum/frame'.padStart(10), '#carts'.padStart(7), '  software?');
for (const [n, v] of Object.entries(agg).sort((a, b) => b[1] - a[1]))
  console.log(n.padEnd(14), Math.round(v).toString().padStart(10), String(used[n]).padStart(7),
              SOFT.has(n) ? '  ← per-pixel' : '');
