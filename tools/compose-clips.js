#!/usr/bin/env node
// compose-clips.js — stitch baked cart clips into ONE reel with transitions.
//
// This is "Layer B" from docs/design/transitions.md: take already-baked .webm clips
// (made by make-gif.js, which now carry audio) and glue them with ffmpeg crossfades
// into a single showreel video — video via `xfade`, audio via `acrossfade`, so picture
// AND sound dissolve together at each cut.
//
//   node tools/compose-clips.js <reel>          # tools/reels/<reel>.reel → editor/public/reels/<reel>.webm
//   node tools/compose-clips.js --from x.reel --out y.webm
//
// A .reel manifest is a committed, reproducible recipe (the Layer-B sibling of a clip
// recipe). One clip per line; `# key value` comment lines carry defaults:
//
//   # the v0 teaser
//   # fps 30
//   # crf 28
//   # size 320x200            (optional; default = first clip's dimensions)
//   # xfade fade 0.5          (default transition type + seconds between cuts)
//   sloop/01-autodrive                      ← editor/public/clips/sloop/01-autodrive.webm
//   moog/01-fat        | wipeleft 0.6       ← transition INTO this clip overrides the default
//   easel/01-selfplay  | circleopen 0.5
//   path/to/any.webm                        ← a bare path also works
//
// A clip ref is `<cart>/<label>` (→ editor/public/clips/<cart>/<label>.webm) or a path.
// The `| <type> <secs>` on a line sets the transition used to bring THAT clip in (the cut
// before it); the first clip's is ignored. Transition types are ffmpeg xfade names:
// fade · dissolve · wipeleft/right/up/down · slideleft/… · circleopen · circleclose ·
// pixelize · radial · smoothleft/…  (our iris≈circleopen, wipe≈wipeleft, dissolve≈dissolve).
//
// Clips of different sizes are letterboxed (nearest-neighbour, pixels stay crisp) onto the
// target canvas. A reel is a standalone *watchable* file, so it's NEAREST-upscaled at encode
// time (default 3×) and written yuv444p — crisp in any video player (the native clips rely on
// the gallery's CSS image-rendering:pixelated, which a raw player doesn't apply). Clips bake
// with audio by default; a silent clip gets silence so the crossfade still lines up.
// Flags / `# meta`: --fps --crf --size WxH --scale N  (N = integer nearest upscale; `# scale N`).

const fs   = require('fs')
const path = require('path')
const { spawnSync } = require('child_process')
const mk = require('./make-cart.js')

const args = process.argv.slice(2)
const opt = (flag, def) => { const i = args.indexOf(flag); return i >= 0 && i + 1 < args.length ? args[i + 1] : def }

const REELS_IN  = path.join(mk.ROOT_DIR, 'tools', 'reels')
const REELS_OUT = path.join(mk.ROOT_DIR, 'editor', 'public', 'reels')
const CLIPS_OUT = path.join(mk.ROOT_DIR, 'editor', 'public', 'clips')

// ── resolve the manifest + output ─────────────────────────────
let manifest = opt('--from', null), out = opt('--out', null), reelName = null
if (!manifest) {
  reelName = args.find(a => !a.startsWith('--') && !args[args.indexOf(a) - 1]?.startsWith('--'))
  if (!reelName) { console.error('usage: node tools/compose-clips.js <reel> | --from <file> [--out <path>]'); process.exit(1) }
  manifest = path.join(REELS_IN, `${reelName}.reel`)
}
if (!fs.existsSync(manifest)) { console.error('no manifest at', manifest); process.exit(1) }
if (!out) out = reelName ? path.join(REELS_OUT, `${reelName}.webm`) : path.join(mk.ROOT_DIR, 'docs', 'media', 'reel.webm')
out = path.resolve(out)

// ── parse the manifest ────────────────────────────────────────
const meta = { fps: 30, crf: 28, size: null, scale: 3, xtype: 'fade', xdur: 0.5 }
const shots = []
for (const raw of fs.readFileSync(manifest, 'utf8').split('\n')) {
  const line = raw.trim()
  if (!line) continue
  let m
  if ((m = line.match(/^#\s*fps\s+(\d+)/)))        { meta.fps  = +m[1]; continue }
  if ((m = line.match(/^#\s*crf\s+(\d+)/)))        { meta.crf  = +m[1]; continue }
  if ((m = line.match(/^#\s*size\s+(\d+)x(\d+)/))) { meta.size = [+m[1], +m[2]]; continue }
  if ((m = line.match(/^#\s*scale\s+(\d+)/)))      { meta.scale = +m[1]; continue }
  if ((m = line.match(/^#\s*xfade\s+(\w+)\s+([\d.]+)/))) { meta.xtype = m[1]; meta.xdur = +m[2]; continue }
  if (line.startsWith('#')) continue
  const [refPart, xPart] = line.split('|').map(s => s.trim())
  const shot = { ref: refPart, xtype: meta.xtype, xdur: meta.xdur }
  if (xPart) { const xm = xPart.match(/^(\w+)\s+([\d.]+)/); if (xm) { shot.xtype = xm[1]; shot.xdur = +xm[2] } }
  // resolve ref → a webm path
  shot.file = /[\/.]/.test(refPart) && refPart.endsWith('.webm') ? path.resolve(refPart)
            : path.join(CLIPS_OUT, ...refPart.split('/')) + '.webm'
  if (!fs.existsSync(shot.file)) { console.error(`clip not found for "${refPart}": ${shot.file}\n  (bake it first: node tools/make-gif.js ${refPart.split('/')[0]} --recipe ${refPart.split('/')[1] || ''})`); process.exit(1) }
  shots.push(shot)
}
if (shots.length < 2) { console.error('a reel needs at least 2 clips'); process.exit(1) }
if (opt('--fps', null)) meta.fps = +opt('--fps')
if (opt('--crf', null)) meta.crf = +opt('--crf')
if (opt('--size', null)) { const s = opt('--size').match(/(\d+)x(\d+)/); if (s) meta.size = [+s[1], +s[2]] }
if (opt('--scale', null)) meta.scale = +opt('--scale')

// ── probe each clip: duration + size + has-audio ──────────────
const ffprobe = (a) => spawnSync('ffprobe', ['-v', 'error', ...a], { encoding: 'utf8' }).stdout.trim()
for (const s of shots) {
  s.dur = parseFloat(ffprobe(['-show_entries', 'format=duration', '-of', 'default=nk=1:nw=1', s.file])) || 0
  const wh = ffprobe(['-select_streams', 'v:0', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', s.file]).split(',')
  s.w = +wh[0]; s.h = +wh[1]
  s.hasAudio = ffprobe(['-select_streams', 'a', '-show_entries', 'stream=index', '-of', 'csv=p=0', s.file]).length > 0
  if (s.dur <= 0) { console.error('could not read duration of', s.file); process.exit(1) }
}
// canvas = base size × an integer NEAREST upscale, so the reel is crisp in any video
// player (unlike the native clips, which rely on the gallery's CSS image-rendering:pixelated).
const base = meta.size || [shots[0].w, shots[0].h]
const S = Math.max(1, meta.scale)
const W = base[0] * S, H = base[1] * S

// clamp each transition shorter than both neighbouring clips (xfade can't outlast a clip)
for (let i = 1; i < shots.length; i++) {
  const cap = Math.min(shots[i - 1].dur, shots[i].dur) - 0.05
  shots[i].xdur = Math.max(0.1, Math.min(shots[i].xdur, cap))
}

// ── build the ffmpeg filter graph ─────────────────────────────
// per clip: scale (nearest, keep aspect) → letterbox-pad to WxH → fps → yuv420p → sar 1
const inputs = []
const vparts = [], aparts = []
shots.forEach((s, i) => {
  inputs.push('-i', s.file)
  vparts.push(`[${i}:v]scale=${W}:${H}:flags=neighbor:force_original_aspect_ratio=decrease,` +
              `pad=${W}:${H}:(ow-iw)/2:(oh-ih)/2:color=black,fps=${meta.fps},format=yuv420p,setsar=1[v${i}]`)
  aparts.push(s.hasAudio
    ? `[${i}:a]aformat=sample_rates=44100:channel_layouts=stereo,asetpts=N/SR/TB[a${i}]`
    : `aevalsrc=0:d=${s.dur.toFixed(3)}:s=44100:c=stereo[a${i}]`)
})

// chain xfade (video) + acrossfade (audio); offset_i = (combined so far) - xdur_i
let vcur = 'v0', acur = 'a0', combined = shots[0].dur
const chain = []
for (let i = 1; i < shots.length; i++) {
  const d = shots[i].xdur, off = (combined - d).toFixed(3)
  const vo = `vx${i}`, ao = `ax${i}`
  chain.push(`[${vcur}][v${i}]xfade=transition=${shots[i].xtype}:duration=${d}:offset=${off}[${vo}]`)
  chain.push(`[${acur}][a${i}]acrossfade=d=${d}[${ao}]`)
  vcur = vo; acur = ao
  combined = combined + shots[i].dur - d
}
const filter = [...vparts, ...aparts, ...chain].join(';')

// ── encode ────────────────────────────────────────────────────
fs.mkdirSync(path.dirname(out), { recursive: true })
const ffArgs = ['-y', ...inputs, '-filter_complex', filter,
  '-map', `[${vcur}]`, '-map', `[${acur}]`,
  '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuv444p', '-crf', String(meta.crf), '-b:v', '0',
  '-c:a', 'libopus', '-b:a', '128k', out]

console.log(`composing ${shots.length} clips → ${path.relative(mk.ROOT_DIR, out)}  (${W}×${H} = ${S}× nearest @ ${meta.fps}fps, crf ${meta.crf})`)
shots.forEach((s, i) => console.log(`  ${i === 0 ? '▸' : `↳ ${shots[i].xtype} ${shots[i].xdur}s`}  ${s.ref}  (${s.dur.toFixed(1)}s${s.hasAudio ? '' : ', silent'})`))
const r = spawnSync('ffmpeg', ffArgs, { stdio: ['ignore', 'pipe', 'pipe'] })
if (r.status !== 0) { console.error(r.stderr?.toString() || 'ffmpeg failed'); process.exit(1) }
const total = (combined).toFixed(1)
console.log(`✓ ${path.relative(mk.ROOT_DIR, out)}  (${(fs.statSync(out).size / 1024).toFixed(0)} KB, ~${total}s)`)
