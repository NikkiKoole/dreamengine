// wav-envelope.js — print a fine-grained AMPLITUDE + BRIGHTNESS envelope of a mono 16-bit WAV.
//
// Companion to tools/wav-analyze.js (which gives peak/RMS/crest/clipping + a coarse per-SECOND
// RMS). This one shows the shape over time at 100ms resolution, with TWO curves:
//   • amplitude  — peak per window (the note's loudness contour)
//   • brightness — RMS of the first difference per window (a cheap high-frequency proxy)
// Plotting both is what cracked the Rhodes "ringy" bug and the funky-clav wah (2026-06-08):
// a Rhodes/clav should show brightness DROPPING FASTER than amplitude (the tine/filter dies
// while the body rings); a dull or wrong patch shows them moving in lockstep. See
// docs/design/sound-handoff.md.
//
//   node tools/wav-envelope.js <file.wav> [windowMs=100]
//
// CAVEAT: the brightness proxy conflates a resonant filter PEAK with perceptual brightness —
// a loud resonant peak reads as "bright" even when it isn't. Good for decay/transient shape;
// trust your ears for resonant-filter timbre. (Pairs with tools/navkit-render.c for A/Bs.)

const fs = require('fs');

function readWav(f) {
  const b = fs.readFileSync(f);
  let off = 12, data = null, sr = 44100;
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4), len = b.readUInt32LE(off + 4);
    if (id === 'fmt ') sr = b.readUInt32LE(off + 12);
    if (id === 'data') data = { off: off + 8, len };
    off += 8 + len + (len & 1);
  }
  if (!data) throw new Error(`${f}: no data chunk`);
  const n = (data.len / 2) | 0, s = new Float32Array(n);
  for (let i = 0; i < n; i++) s[i] = b.readInt16LE(data.off + i * 2) / 32768;
  return { sr, s };
}

const file = process.argv[2];
if (!file) { console.error('usage: node tools/wav-envelope.js <file.wav> [windowMs]'); process.exit(1); }
const winMs = process.argv[3] ? parseFloat(process.argv[3]) : 100;
const { sr, s } = readWav(file);
const W = Math.max(1, (sr * winMs / 1000) | 0);
let pmax = 0, bmax = 0; const pk = [], br = [];
for (let i = 0; i < s.length; i += W) {
  let p = 0, b2 = 0, nn = 0;
  for (let j = i; j < Math.min(i + W, s.length); j++) {
    p = Math.max(p, Math.abs(s[j]));
    if (j > 0) { const d = s[j] - s[j - 1]; b2 += d * d; nn++; }
  }
  const b = Math.sqrt(b2 / Math.max(1, nn));
  pk.push(p); br.push(b); pmax = Math.max(pmax, p); bmax = Math.max(bmax, b);
}
console.log(`${file}  (${winMs}ms windows, normalized)`);
console.log('   t    AMPLITUDE                 BRIGHTNESS(HF)');
for (let i = 0; i < pk.length; i++) {
  const a = pmax > 0 ? pk[i] / pmax : 0, b = bmax > 0 ? br[i] / bmax : 0;
  const t = (i * winMs / 1000).toFixed(2) + 's';
  console.log(t.padStart(6) + ' ' + a.toFixed(2) + ' ' + '#'.repeat(Math.round(a * 18)).padEnd(19) +
              ' ' + b.toFixed(2) + ' ' + '#'.repeat(Math.round(b * 18)));
}
