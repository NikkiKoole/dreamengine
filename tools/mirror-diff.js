#!/usr/bin/env node
// mirror-diff.js — the GOLDEN-PIXEL-DIFF / visual-symmetry harness (STATUS.md §43).
//
// The framebuffer twin of tune-check / level-check: it can't be a spec() because the
// pixels are the output of the engine's polyfill/line scan-conversion, not a pure
// function of the cart's inputs (see docs/design/spec-harness.md). Instead it renders a
// cart HEADLESS, then asserts a SYMMETRY INVARIANT on the rendered pixels: a cart whose
// scene is mirror-symmetric (e.g. streetlab's default 4-way junction about x=160) should
// be pixel-symmetric too. It isn't — that's the ≤1px corner floor the road docs accept
// (docs/design/road-program-state.md "Accepted floor", streetlab.c §SEAM). This tool
// MEASURES that floor and, once the symmetric-corner mirror-blit fix lands
// (docs/design/streetlab-corner-symmetry-plan.md), GATES it back to zero.
//
// Promoted from the throwaway pngdiff.js prototype written during the arcsym
// investigation (the cart `arcsym` is the petri-dish demo of the same mechanism).
//
// Usage:
//   node tools/mirror-diff.js <cart>            render the cart headless, then diff
//   node tools/mirror-diff.js --png <file.png>  diff an existing PNG (e.g. a --dump frame)
// Options:
//   --axis h|v|both   mirror axis: h = left/right about cx (default), v = up/down, both
//   --cx <x> --cy <y> reflection centre (default: image centre W/2, H/2)
//   --band y0,y1      only compare rows in [y0,y1) (skip title/toolbar/dashed lanes)
//   --xband x0,x1     only compare columns in [x0,x1)
//   --overlay <file>  write a magnified PNG with mismatched pairs painted red
//   --frames <n>      frames to run before dumping (default 2)
//   --json            machine-readable output
//   --quiet           print nothing on success; exit 1 if any mismatch (CI gate)
//
// Zero-dependency: its own minimal PNG decode (zlib + the 5 PNG filters) and encode.

const fs = require('fs'), zlib = require('zlib'), path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');

// ── arg parsing ──────────────────────────────────────────────
const args = process.argv.slice(2);
function opt(name, def) { const i = args.indexOf(name); return i >= 0 ? args[i + 1] : def; }
function flag(name) { return args.includes(name); }
const QUIET = flag('--quiet'), JSON_OUT = flag('--json');
const axis = opt('--axis', 'h');
const overlayPath = opt('--overlay', null);
const frames = parseInt(opt('--frames', '2'), 10);
const pngArg = opt('--png', null);
const bandArg = opt('--band', null), xbandArg = opt('--xband', null);
const positional = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--') && args[i - 1] !== '--json' && args[i - 1] !== '--quiet'));
const cart = pngArg ? null : positional[0];

if (!pngArg && !cart) {
  console.error('usage: node tools/mirror-diff.js <cart> | --png <file>  [--axis h|v|both] [--band y0,y1] [--overlay out.png] [--json] [--quiet]');
  process.exit(2);
}

// ── PNG decode (RGBA/RGB/gray, all 5 filters) ────────────────
function decodePNG(file) {
  const b = fs.readFileSync(file);
  let off = 8, W, H, ct, idat = [];
  while (off < b.length) {
    const len = b.readUInt32BE(off), type = b.toString('latin1', off + 4, off + 8);
    const data = b.slice(off + 8, off + 8 + len);
    if (type === 'IHDR') { W = data.readUInt32BE(0); H = data.readUInt32BE(4); ct = data[9]; }
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    off += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const ch = ct === 6 ? 4 : ct === 2 ? 3 : ct === 0 ? 1 : 4;
  const stride = W * ch, px = Buffer.alloc(H * stride);
  const paeth = (a, bb, c) => { const p = a + bb - c, pa = Math.abs(p - a), pb = Math.abs(p - bb), pc = Math.abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? bb : c; };
  let p = 0, r = 0;
  for (let y = 0; y < H; y++) {
    const f = raw[p++];
    for (let x = 0; x < stride; x++) {
      const cur = raw[p++];
      const a = x >= ch ? px[r + x - ch] : 0;
      const up = y > 0 ? px[r - stride + x] : 0;
      const ul = (x >= ch && y > 0) ? px[r - stride + x - ch] : 0;
      let v;
      switch (f) { case 0: v = cur; break; case 1: v = cur + a; break; case 2: v = cur + up; break;
        case 3: v = cur + ((a + up) >> 1); break; case 4: v = cur + paeth(a, up, ul); break; default: v = cur; }
      px[r + x] = v & 255;
    }
    r += stride;
  }
  return { W, H, ch, px };
}

// ── PNG encode (RGB, filter 0) for the overlay ───────────────
function crc32(buf) { let c = ~0; for (let i = 0; i < buf.length; i++) { c ^= buf[i]; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1)); } return ~c >>> 0; }
function chunk(type, data) { const t = Buffer.from(type, 'latin1'), b = Buffer.concat([t, data]); const o = Buffer.alloc(12 + data.length); o.writeUInt32BE(data.length, 0); t.copy(o, 4); data.copy(o, 8); o.writeUInt32BE(crc32(b), 8 + data.length); return o; }
function encodePNG(W, H, rgb) {
  const stride = W * 3, raw = Buffer.alloc(H * (stride + 1));
  for (let y = 0; y < H; y++) { raw[y * (stride + 1)] = 0; rgb.copy(raw, y * (stride + 1) + 1, y * stride, y * stride + stride); }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
  const sig = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  return Buffer.concat([sig, chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

// ── render the cart headless if no --png given ───────────────
function renderCart(name) {
  const dump = path.join(ROOT, 'build', '.mirror-diff', name);
  fs.rmSync(dump, { recursive: true, force: true });
  fs.mkdirSync(dump, { recursive: true });
  execFileSync('node', ['tools/play.js', name, 'script', '/dev/null', '--headless',
    '--frames', String(frames), '--dump', dump, '--dump-every', '1'],
    { cwd: ROOT, stdio: 'ignore' });
  const fr = fs.readdirSync(dump).filter(f => f.endsWith('.png')).sort();
  if (!fr.length) throw new Error('no frame dumped for ' + name);
  return path.join(dump, fr[fr.length - 1]);
}

// ── run ──────────────────────────────────────────────────────
const file = pngArg || renderCart(cart);
const img = decodePNG(file);
const { W, H, ch, px } = img;
const cx = parseFloat(opt('--cx', String(W / 2)));
const cy = parseFloat(opt('--cy', String(H / 2)));
const [by0, by1] = bandArg ? bandArg.split(',').map(Number) : [0, H];
const [bx0, bx1] = xbandArg ? xbandArg.split(',').map(Number) : [0, W];
const at = (x, y) => { const i = (y * W + x) * ch; return (px[i] << 16) | (px[i + 1] << 8) | px[i + 2]; };

// reflect a coordinate about a real centre: c -> round(2*centre - 1 - c) keeps it a
// bijection on the integer grid (matches an even-width framebuffer's pixel mirror).
const mirX = x => Math.round(2 * cx - 1 - x);
const mirY = y => Math.round(2 * cy - 1 - y);

function diff(useH, useV) {
  let pairs = 0, mism = 0; const rowmis = {}, marks = [];
  for (let y = by0; y < by1; y++) {
    for (let x = bx0; x < bx1; x++) {
      const mx = useH ? mirX(x) : x, my = useV ? mirY(y) : y;
      if (mx < 0 || mx >= W || my < 0 || my >= H) continue;
      if (useH && mx <= x && !useV) continue;        // count each L/R pair once
      pairs++;
      if (at(x, y) !== at(mx, my)) { mism++; rowmis[y] = (rowmis[y] || 0) + 1; marks.push([x, y]); }
    }
  }
  return { pairs, mism, rowmis, marks };
}

const doH = axis === 'h' || axis === 'both';
const doV = axis === 'v' || axis === 'both';
const res = axis === 'both' ? diff(true, true) : doV ? diff(false, true) : diff(true, false);

if (overlayPath) {
  const S = 4, OW = W * S, OH = (by1 - by0) * S, out = Buffer.alloc(OW * OH * 3);
  const mset = new Set(res.marks.map(([x, y]) => y * W + x));
  for (let y = by0; y < by1; y++) for (let x = 0; x < W; x++) {
    const c = at(x, y); let r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    if (mset.has(y * W + x)) { r = 255; g = 0; b = 0; }
    for (let sy = 0; sy < S; sy++) for (let sx = 0; sx < S; sx++) {
      const i = (((y - by0) * S + sy) * OW + (x * S + sx)) * 3; out[i] = r; out[i + 1] = g; out[i + 2] = b;
    }
  }
  fs.writeFileSync(overlayPath, encodePNG(OW, OH, out));
}

if (JSON_OUT) {
  console.log(JSON.stringify({ file: path.relative(ROOT, file), W, H, axis, cx, cy,
    band: [by0, by1], xband: [bx0, bx1], pairs: res.pairs, mismatch: res.mism }));
} else if (!QUIET || res.mism > 0) {
  const tag = pngArg ? file : `${cart} (${path.relative(ROOT, file)})`;
  console.log(`${tag}: ${W}x${H}, axis=${axis}, centre=(${cx},${cy})`);
  console.log(`mirror mismatch: ${res.mism} of ${res.pairs} compared pixel-pairs`);
  const rows = Object.entries(res.rowmis).sort((a, b) => b[1] - a[1]).slice(0, 10);
  if (rows.length) console.log('worst rows (y:count):', rows.map(([y, n]) => `${y}:${n}`).join('  '));
  if (overlayPath) console.log('overlay:', overlayPath);
}

process.exit(res.mism > 0 && QUIET ? 1 : 0);
