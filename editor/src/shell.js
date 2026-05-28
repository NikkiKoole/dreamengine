import './shell.css'
import { view, setEditorTheme } from './main.js'
import './sprite-editor.js'
import { getMapBytes } from './map-editor.js'
import { studioDocs } from './studioDocs.js'
import { settings, buildSettingsPanel } from './settings.js'

// ── tab switching ─────────────────────────────────────────────
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    if (btn.disabled) return
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'))
    document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'))
    btn.classList.add('active')
    document.getElementById('panel-' + btn.dataset.tab).classList.add('active')
  })
})

// ── help panel ───────────────────────────────────────────────
const sections = [
  { title: 'c basics',   titleNL: 'c basics',     keys: ['include', 'define', 'int', 'float', 'bool', 'void', 'static', 'if', 'for', 'return', 'logical', 'equality', 'comment', 'braces', 'semicolon', 'array'] },
  { title: 'callbacks',  titleNL: 'callbacks',    keys: ['update', 'draw'] },
  { title: 'graphics',   titleNL: 'tekenen',      keys: ['cls', 'spr', 'sprf', 'sspr', 'pget', 'pset', 'print', 'rect', 'rectfill', 'circ', 'circfill', 'line', 'camera', 'clip'] },
  { title: 'input',      titleNL: 'input',        keys: ['btn', 'btnp', 'BTN_UP', 'BTN_DOWN', 'BTN_LEFT', 'BTN_RIGHT', 'BTN_A', 'BTN_B'] },
  { title: 'touch',      titleNL: 'touch',        keys: ['stick_x', 'stick_y', 'touch_count', 'touch_x', 'touch_y', 'tap', 'touch_controls'] },
  { title: 'map',        titleNL: 'map',          keys: ['map', 'mget', 'mset', 'MAP_W', 'MAP_H'] },
  { title: 'sound',      titleNL: 'geluid',       keys: ['sfx', 'music', 'note', 'hit', 'tone', 'chord', 'strum', 'schedule', 'bpm', 'beat', 'beat_pos', 'every', 'euclid', 'chance', 'degree', 'INSTR_SQUARE', 'INSTR_SAW', 'INSTR_TRI', 'INSTR_NOISE', 'INSTR_SINE', 'SCALE_MAJOR', 'SCALE_MINOR', 'SCALE_PENTA', 'SCALE_PENTA_MIN', 'SCALE_BLUES', 'SCALE_CHROMATIC', 'CHORD_MAJ', 'CHORD_MIN', 'CHORD_DIM', 'CHORD_AUG', 'CHORD_MAJ7', 'CHORD_MIN7', 'CHORD_DOM7', 'CHORD_SUS4', 'CHORD_POWER'] },
  { title: 'utility',    titleNL: 'hulpmiddelen', keys: ['rnd', 'now', 'sgn', 'mid'] },
  { title: 'screen',     titleNL: 'scherm',       keys: ['SCREEN_W', 'SCREEN_H'] },
  { title: 'palette',    titleNL: 'palet',        keys: ['CLR_BLACK', 'CLR_DARK_BLUE', 'CLR_DARK_PURPLE', 'CLR_DARK_GREEN', 'CLR_BROWN', 'CLR_DARK_GREY', 'CLR_LIGHT_GREY', 'CLR_WHITE', 'CLR_RED', 'CLR_ORANGE', 'CLR_YELLOW', 'CLR_GREEN', 'CLR_BLUE', 'CLR_INDIGO', 'CLR_PINK', 'CLR_LIGHT_PEACH', 'CLR_BROWNISH_BLACK', 'CLR_DARKER_BLUE', 'CLR_DARKER_PURPLE', 'CLR_BLUE_GREEN', 'CLR_DARK_BROWN', 'CLR_DARKER_GREY', 'CLR_MEDIUM_GREY', 'CLR_LIGHT_YELLOW', 'CLR_DARK_RED', 'CLR_DARK_ORANGE', 'CLR_LIME_GREEN', 'CLR_MEDIUM_GREEN', 'CLR_TRUE_BLUE', 'CLR_MAUVE', 'CLR_DARK_PEACH', 'CLR_PEACH'] },
]

const introHTML = {
  en: `
  <h1>dreamengine</h1>
  <p>
    A fantasy console you program in C. Write code on the left, hit
    <span class="kbd">▶ run</span>, and a native game window pops open.
    The runtime gives you a 320×200 canvas, a 32-color palette, a sprite editor
    (next tab), a small synth, and the input + drawing API listed below.
    It's aimed at teens and hobbyists who want to learn real programming through
    making games — no malloc, no build system, no boilerplate.
  </p>
  <p>
    The goal is the smallest possible distance between an idea and a thing that
    moves on screen. Inspired by PICO-8, DIV Game Studio, and BlitzMax.
    Coming eventually: a DIV-style process model so each game object can be its
    own coroutine, an iPad runtime, and a sound tracker to match the sprite editor.
  </p>
  <p>
    <b>New to C?</b> A program is a list of instructions. The runtime calls your
    <span class="kbd">update()</span> 60 times a second, then your
    <span class="kbd">draw()</span>, and repeats. You declare variables like
    <span class="kbd">int x = 5;</span> to remember things between frames, and you
    use <span class="kbd">if</span>, <span class="kbd">for</span>, and function
    calls to make things happen. Hover any keyword in the editor for a quick
    explanation — the <b>c basics</b> section below has a short tour of the
    bits you'll meet first.
  </p>
`,
  nl: `
  <h1>dreamengine</h1>
  <p>
    Een fantasieconsole die je in C programmeert. Schrijf code aan de linkerkant, klik op
    <span class="kbd">▶ run</span>, en er verschijnt een echt spelvenster.
    De runtime geeft je een 320×200 canvas, een palet van 32 kleuren, een sprite-editor
    (volgende tab), een kleine synth, en de input- en teken-API hieronder.
    Bedoeld voor tieners en hobbyisten die echt willen leren programmeren door
    spellen te maken — geen malloc, geen build-systeem, geen boilerplate.
  </p>
  <p>
    Het doel is de kleinst mogelijke afstand tussen een idee en iets dat over het
    scherm beweegt. Geïnspireerd door PICO-8, DIV Game Studio en BlitzMax.
    Komt later nog: een DIV-achtig procesmodel zodat elk spelobject zijn eigen
    coroutine kan zijn, een iPad-runtime, en een sound-tracker die past bij de sprite-editor.
  </p>
  <p>
    <b>Nog niet bekend met C?</b> Een programma is een lijst met instructies.
    De runtime roept jouw <span class="kbd">update()</span> 60 keer per seconde aan,
    daarna jouw <span class="kbd">draw()</span>, en herhaalt dat. Je onthoudt dingen
    tussen frames met variabelen (<span class="kbd">int x = 5;</span>), en je laat
    dingen gebeuren met <span class="kbd">if</span>, <span class="kbd">for</span> en
    functie-aanroepen. Hover over willekeurig welk sleutelwoord in de editor voor
    een korte uitleg — het hoofdstuk <b>c basics</b> hieronder geeft een rondleiding
    langs de bouwstenen die je het eerst tegenkomt.
  </p>
`,
}

const helpPanel = document.getElementById('panel-help')
let helpLang = localStorage.getItem('helpLang') === 'nl' ? 'nl' : 'en'

function renderHelp() {
  helpPanel.innerHTML = ''

  // language toggle
  const toggle = document.createElement('div')
  toggle.className = 'help-lang-toggle'
  ;[['en', 'EN'], ['nl', 'NL']].forEach(([lang, label]) => {
    const btn = document.createElement('button')
    btn.textContent = label
    if (helpLang === lang) btn.classList.add('active')
    btn.addEventListener('click', () => {
      helpLang = lang
      localStorage.setItem('helpLang', lang)
      renderHelp()
    })
    toggle.appendChild(btn)
  })
  helpPanel.appendChild(toggle)

  const intro = document.createElement('div')
  intro.className = 'help-intro'
  intro.innerHTML = introHTML[helpLang]
  helpPanel.appendChild(intro)

  sections.forEach(({ title, titleNL, keys }) => {
    const section = document.createElement('div')
    section.className = 'help-section'
    section.innerHTML = `<div class="help-section-title">${helpLang === 'nl' ? titleNL : title}</div>`
    keys.forEach(key => {
      const entry = studioDocs[key]
      if (!entry) return
      const text = (helpLang === 'nl' && entry.docNL) ? entry.docNL : entry.doc
      const row = document.createElement('div')
      row.className = 'help-entry'
      row.id        = 'help-entry-' + key

      // for CLR_* entries, pull the hex out of the doc and prepend a swatch
      const hexMatch = key.startsWith('CLR_') ? entry.doc.match(/#[0-9a-fA-F]{6}/) : null
      const swatch = hexMatch ? `<span class="color-swatch" style="background:${hexMatch[0]}"></span>` : ''

      row.innerHTML = `
        <div class="help-sig">${swatch}${entry.sig}</div>
        <div class="help-doc">${text.replace(/\n/g, '<br>')}</div>
      `
      section.appendChild(row)
    })
    helpPanel.appendChild(section)
  })
}

renderHelp()

// jump-to-help from the code editor (cmd/ctrl-click on a documented word)
window.addEventListener('help-jump', (e) => {
  const key = e.detail.key
  const helpTab = document.querySelector('.tab[data-tab="help"]')
  if (helpTab) helpTab.click()
  // wait one frame for the panel to become visible before scrolling
  requestAnimationFrame(() => {
    const el = document.getElementById('help-entry-' + key)
    if (!el) return
    el.scrollIntoView({ behavior: 'instant', block: 'center' })
    el.classList.add('help-flash')
    setTimeout(() => el.classList.remove('help-flash'), 1200)
  })
})

// ── settings panel ───────────────────────────────────────────
buildSettingsPanel(document.getElementById('panel-settings'))

// ── day/night theme ──────────────────────────────────────────
const themeBtn = document.getElementById('theme-btn')
function applyTheme(mode) {
  document.documentElement.classList.toggle('day', mode === 'day')
  themeBtn.textContent = mode === 'day' ? '☀' : '☾'
  themeBtn.title = mode === 'day' ? 'switch to night' : 'switch to day'
  setEditorTheme(mode)
}
applyTheme(localStorage.getItem('theme') || 'night')
themeBtn.addEventListener('click', () => {
  const next = document.documentElement.classList.contains('day') ? 'night' : 'day'
  localStorage.setItem('theme', next)
  applyTheme(next)
})

// ── run button ────────────────────────────────────────────────
const runBtn  = document.getElementById('run-btn')
const buildLog = document.getElementById('build-log')

runBtn.addEventListener('click', async () => {
  if (!window.studio) {
    showLog({ ok: false, cmd: null, output: 'run requires the desktop app  (npm start)' })
    return
  }

  const code = view.state.doc.toString()
  runBtn.textContent = '⏳ compiling…'
  runBtn.disabled = true
  buildLog.style.display = 'none'

  // export sprite sheet from editor canvas before compiling
  const tilemapCanvas = document.querySelector('#tilemap-canvas')
  if (tilemapCanvas) {
    await window.studio.saveSprites(tilemapCanvas.toDataURL('image/png'))
  }

  // export the current map as raw bytes
  await window.studio.saveMap(getMapBytes())

  const result = await window.studio.run(code, settings)

  runBtn.textContent = '▶ run'
  runBtn.disabled = false

  showLog(result)
})

let hideTimer = null

function showLog(result) {
  clearTimeout(hideTimer)

  buildLog.style.display = 'block'
  buildLog.innerHTML = ''

  // close button
  const close = document.createElement('button')
  close.className = 'build-close'
  close.textContent = '×'
  close.addEventListener('click', () => {
    clearTimeout(hideTimer)
    buildLog.style.display = 'none'
  })
  buildLog.appendChild(close)

  if (result.cmd) {
    const cmdLine = document.createElement('div')
    cmdLine.className = 'build-cmd'
    cmdLine.textContent = '$ ' + result.cmd
    buildLog.appendChild(cmdLine)
  }

  if (result.ok) {
    const ok = document.createElement('div')
    ok.className = 'build-ok'
    ok.textContent = `✓ compiled → ${result.bin}`
    buildLog.appendChild(ok)

    const src = document.createElement('div')
    src.className = 'build-meta'
    src.textContent = `  source   → ${result.src}`
    buildLog.appendChild(src)

    // auto-hide after 3s on success
    hideTimer = setTimeout(() => { buildLog.style.display = 'none' }, 3000)
  }

  if (result.output) {
    const out = document.createElement('div')
    out.className = result.ok ? 'build-warn' : 'build-error'
    out.textContent = result.output
    buildLog.appendChild(out)
  }
}
