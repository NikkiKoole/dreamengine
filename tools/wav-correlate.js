// wav-correlate.js — sample-level A/B: are two renders the SAME DSP, up to a constant gain and a
// small fixed delay? Normalizes each to unit RMS, cross-correlates a mid segment over a lag window,
// and reports the peak normalized correlation (1.000 = identical waveform shape). Robust to the
// engine's output-gain offset and a few-sample startup delay — the right tool for A/B-ing one of our
// effects against navkit's real DSP when the two render paths have different absolute levels.
//
//   node tools/wav-correlate.js A.wav B.wav
//
// Built for the navkit-port workflow (see docs/guides/debug-harness.md → "A/B a bus EFFECT").
// Pairs with tools/navkit-fx-render.c (renders navkit's genuine effect) + tools/wav-modrate.js.
const fs = require('fs');
function readWav(path) {
  const b = fs.readFileSync(path);
  let p = 12, sr = 44100, ch = 1, off = 44, len = 0;
  while (p + 8 <= b.length) {
    const id = b.toString('ascii', p, p + 4), sz = b.readUInt32LE(p + 4);
    if (id === 'fmt ') { ch = b.readUInt16LE(p + 10); sr = b.readUInt32LE(p + 12); }
    if (id === 'data') { off = p + 8; len = sz; break; }
    p += 8 + sz + (sz & 1);
  }
  const stride = 2 * ch, n = Math.floor(len / stride), x = new Float32Array(n);  // left channel
  for (let i = 0; i < n; i++) x[i] = b.readInt16LE(off + i * stride) / 32768;
  return { sr, x };
}
const [fa, fb] = process.argv.slice(2);
if (!fa || !fb) { console.error('usage: node tools/wav-correlate.js A.wav B.wav'); process.exit(1); }
const A = readWav(fa), B = readWav(fb);
const sr = A.sr;
const start = Math.round(sr * 1.0), N = Math.round(sr * 3.0);   // correlate the 1s..4s window
function seg(x, s, n) {
  const y = Float32Array.from(x.slice(s, s + n));
  let mean = 0; for (const v of y) mean += v; mean /= y.length;
  let e = 0; for (let i = 0; i < y.length; i++) { y[i] -= mean; e += y[i] * y[i]; }
  const norm = Math.sqrt(e) || 1; for (let i = 0; i < y.length; i++) y[i] /= norm;
  return y;
}
const a = seg(A.x, start, N);
let best = -Infinity, bestLag = 0;
for (let lag = -200; lag <= 200; lag++) {
  const b = seg(B.x, start + lag, N);
  let dot = 0; for (let i = 0; i < N; i++) dot += a[i] * b[i];
  if (dot > best) { best = dot; bestLag = lag; }
}
console.log(`peak normalized correlation: ${best.toFixed(5)}  (at lag ${bestLag} samples)`);
console.log(best > 0.999 ? 'MATCH — identical DSP (up to gain + delay)' :
            best > 0.99  ? 'close match' : 'MISMATCH');
process.exit(best > 0.999 ? 0 : 1);
