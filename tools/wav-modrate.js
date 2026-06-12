// wav-modrate.js — extract a modulation effect's defining numbers from a WAV: the LFO RATE (Hz)
// and DEPTH (1 - trough/peak), independent of the carrier level. The right A/B for amplitude-modulating
// effects (tremolo, auto-pan, and the amplitude wobble a phaser/vibrato imparts on a steady tone) —
// it factors out each render's source gain, so our engine's ADSR/mix gain doesn't skew the comparison.
//
//   node tools/wav-modrate.js a.wav [b.wav ...]
//
// Feed a STEADY, flat-top tone (e.g. the tremtest.c / phasertest.c carts: INSTR_SINE, full sustain,
// one held note) — a busy/decaying source's envelope is dominated by note dynamics, not the effect.
// Pairs with tools/navkit-fx-render.c + tools/wav-correlate.js (see debug-harness.md → "A/B a bus EFFECT").
// (Distinct from tools/wav-envelope.js, which prints an amplitude+brightness envelope, not a mod rate.)
const fs = require('fs');
function readWav(path) {
  const b = fs.readFileSync(path);
  let p = 12, sr = 44100, ch = 1, dataOff = 44, dataLen = 0;
  while (p + 8 <= b.length) {
    const id = b.toString('ascii', p, p + 4), sz = b.readUInt32LE(p + 4);
    if (id === 'fmt ') { ch = b.readUInt16LE(p + 10); sr = b.readUInt32LE(p + 12); }
    if (id === 'data') { dataOff = p + 8; dataLen = sz; break; }
    p += 8 + sz + (sz & 1);
  }
  const stride = 2 * ch;                                            // stereo-aware: our --wav is stereo,
  const n = Math.floor(dataLen / stride), x = new Float32Array(n);  // navkit harness WAVs are mono —
  for (let i = 0; i < n; i++) x[i] = b.readInt16LE(dataOff + i * stride) / 32768;  // read the LEFT channel
  return { sr, x };
}
// envelope: sliding-window peak over ~1.5 carrier periods (assume ~440 Hz carrier ≈ 100 smp), step 50
function envelope(x, sr) {
  const win = Math.round(sr / 440 * 1.5), step = 50, env = [];
  for (let i = 0; i + win < x.length; i += step) {
    let m = 0; for (let j = i; j < i + win; j++) { const a = Math.abs(x[j]); if (a > m) m = a; }
    env.push(m);
  }
  return { env, hz: sr / step };
}
function analyze(path) {
  const { sr, x } = readWav(path);
  const { env, hz } = envelope(x, sr);
  const a = Math.round(0.3 * hz), z = env.length - Math.round(0.05 * hz);   // skip attack + tail
  const e = env.slice(a, z);
  const peak = Math.max(...e), trough = Math.min(...e);
  const mean = e.reduce((s, v) => s + v, 0) / e.length;
  const d = e.map(v => v - mean);
  let bestLag = 0, best = -Infinity;
  const loLag = Math.round(hz / 20), hiLag = Math.round(hz / 0.5);          // search 0.5..20 Hz
  for (let lag = loLag; lag < hiLag && lag < d.length; lag++) {
    let s = 0; for (let i = 0; i + lag < d.length; i++) s += d[i] * d[i + lag];
    if (s > best) { best = s; bestLag = lag; }
  }
  return { rateHz: hz / bestLag, depth: 1 - trough / peak, trough, peak };
}
const files = process.argv.slice(2);
if (!files.length) { console.error('usage: node tools/wav-modrate.js a.wav [b.wav ...]'); process.exit(1); }
for (const f of files) {
  const r = analyze(f);
  console.log(`${f}\n  LFO rate  ${r.rateHz.toFixed(3)} Hz\n  depth     ${r.depth.toFixed(3)} (trough/peak = ${(r.trough / r.peak).toFixed(3)})`);
}
