#!/usr/bin/env node
// api-usage.js — cross-check studio.h API usage across all carts, and the
// "four places" doc coverage (studio.h ↔ studioDocs.js ↔ shell.js).
//
//   node tools/api-usage.js            # full table, least-used first
//   node tools/api-usage.js --unused   # only never/rarely used (≤1 cart)
//
// Counting is textual (word-boundary + open paren) per cart source in
// tools/carts/*.c — good enough because cart code shares one namespace with
// the API. Findings snapshot + interpretation: docs/design/api-usage-audit.md

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const onlyUnused = process.argv.includes('--unused');

// ── 1. API surface from studio.h ────────────────────────────────────────────
const hdr = fs.readFileSync(path.join(ROOT, 'runtime/studio.h'), 'utf8');
const names = new Set();
for (const m of hdr.matchAll(/^[A-Za-z_][\w \*]*?\b([a-z_][a-z0-9_]*)\s*\([^)]*\)\s*;/gm)) {
  names.add(m[1]);
}

// ── 2. usage counts across carts ────────────────────────────────────────────
const cartsDir = path.join(ROOT, 'tools/carts');
const carts = fs.readdirSync(cartsDir).filter((f) => f.endsWith('.c'));
const counts = {};
for (const n of names) counts[n] = { carts: 0, calls: 0 };
for (const f of carts) {
  const src = fs.readFileSync(path.join(cartsDir, f), 'utf8');
  for (const n of names) {
    const m = src.match(new RegExp('\\b' + n + '\\s*\\(', 'g'));
    if (m) { counts[n].carts++; counts[n].calls += m.length; }
  }
}

const sorted = Object.entries(counts).sort(
  (a, b) => a[1].carts - b[1].carts || a[1].calls - b[1].calls
);
const rows = onlyUnused ? sorted.filter(([, c]) => c.carts <= 1) : sorted;

console.log('API function'.padEnd(22) + 'carts'.padStart(6) + 'calls'.padStart(7));
for (const [n, c] of rows) {
  console.log(n.padEnd(22) + String(c.carts).padStart(6) + String(c.calls).padStart(7));
}
console.log(`\n${names.size} API functions, ${carts.length} carts scanned`);

// ── 3. four-places coverage: studioDocs.js + shell.js ───────────────────────
const docsSrc = fs.readFileSync(path.join(ROOT, 'editor/src/studioDocs.js'), 'utf8');
const docKeys = new Set();
for (const m of docsSrc.matchAll(/^\s{2}([A-Za-z_][A-Za-z0-9_]*)\s*:\s*\{/gm)) docKeys.add(m[1]);

const shellSrc = fs.readFileSync(path.join(ROOT, 'editor/src/shell.js'), 'utf8');
const shellKeys = new Set();
for (const m of shellSrc.matchAll(/keys:\s*\[([^\]]*)\]/g)) {
  for (const k of m[1].matchAll(/['"]([A-Za-z_][A-Za-z0-9_]*)['"]/g)) shellKeys.add(k[1]);
}

const missDocs = [...names].filter((n) => !docKeys.has(n));
const missShell = [...names].filter((n) => !shellKeys.has(n));
console.log('\nIn studio.h but missing from studioDocs.js: ' + (missDocs.join(', ') || '(none)'));
console.log('In studio.h but missing from shell.js keys: ' + (missShell.join(', ') || '(none)'));
