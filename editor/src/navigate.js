// navigate.js — read-only runtime-file preview.
//
// Cmd/ctrl-click the filename of an `#include "gestures.h"` and the file opens
// read-only INSIDE the code tab, overlaying the editor (VS Code preview style —
// no extra tab; the cart underneath is untouched and ▶ run still builds it).
// Served by the vite /runtime-src/ route, so it works in both Electron and the
// browser tab. Clicks keep working inside the preview: includes chain to other
// headers, documented symbols jump to the help tab. The outline sidebar stays
// visible and lists the PREVIEWED file (definitions + prototypes) while it's
// open. Close with the ✕ or Esc.

import { EditorState, Compartment } from '@codemirror/state'
import { EditorView, lineNumbers, highlightActiveLine } from '@codemirror/view'
import { cpp } from '@codemirror/lang-cpp'
import { oneDark } from '@codemirror/theme-one-dark'
import { dayTheme } from './dayTheme.js'
import { studioDocs } from './studioDocs.js'
import { flashField } from './main.js'
import { setOutlineOverride } from './outline.js'

let vview = null
const vTheme = new Compartment()

function themeExt() {
  return document.documentElement.classList.contains('day') ? dayTheme : oneDark
}

// follow the day/night toggle without touching shell.js: applyTheme() flips a
// class on <html>, which is all we need to observe
new MutationObserver(() => {
  if (vview) vview.dispatch({ effects: vTheme.reconfigure(themeExt()) })
}).observe(document.documentElement, { attributes: true, attributeFilter: ['class'] })

// cmd/ctrl-click inside the preview: includes chain, documented symbols → help
const viewerClicks = EditorView.domEventHandlers({
  click(event, v) {
    if (!event.metaKey && !event.ctrlKey) return false
    const pos = v.posAtCoords({ x: event.clientX, y: event.clientY })
    if (pos === null) return false

    const line = v.state.doc.lineAt(pos)
    const inc = /#include\s*"([^"]+)"/.exec(line.text)
    if (inc) {
      const from = line.from + inc.index + inc[0].indexOf('"')
      const to   = line.from + inc.index + inc[0].length
      if (pos >= from && pos <= to) {
        openFileViewer(inc[1])
        event.preventDefault()
        return true
      }
    }

    const word = v.state.wordAt(pos)
    if (!word) return false
    const name = v.state.sliceDoc(word.from, word.to)
    if (studioDocs[name]) {
      closeViewer()   // jumping to docs — don't leave the preview hanging behind
      window.dispatchEvent(new CustomEvent('help-jump', { detail: { key: name } }))
      event.preventDefault()
      return true
    }
    return false
  },
})

function makeViewerState(docText) {
  return EditorState.create({
    doc: docText,
    extensions: [
      lineNumbers(),
      highlightActiveLine(),
      cpp(),
      EditorState.readOnly.of(true),
      EditorView.editable.of(false),
      viewerClicks,
      flashField,        // outline clicks flash the jumped-to name, same as the cart
      vTheme.of(themeExt()),
    ],
  })
}

function ensureViewer() {
  if (!vview) {
    vview = new EditorView({
      state: makeViewerState(''),
      parent: document.getElementById('viewer-editor'),
    })
  }
  return vview
}

export function closeViewer() {
  document.getElementById('viewer-overlay')?.classList.remove('open')
  setOutlineOverride(null)   // outline goes back to listing the cart
}

function viewerOpen() {
  return document.getElementById('viewer-overlay')?.classList.contains('open')
}

document.getElementById('viewer-close')?.addEventListener('click', () => closeViewer())
window.addEventListener('keydown', e => {
  if (e.key === 'Escape' && viewerOpen()) {
    e.preventDefault()
    closeViewer()
  }
})

// Open `file` (e.g. 'gestures.h') read-only over the code editor.
// Quote-includes only — <math.h> and friends are system headers, not ours.
export async function openFileViewer(file) {
  let text
  try {
    const r = await fetch('/runtime-src/' + encodeURIComponent(file))
    if (!r.ok) throw new Error()
    text = await r.text()
  } catch {
    text = `// ${file} — not found in runtime/\n// (only the engine's own headers can be viewed here)`
  }
  // the preview lives inside the code tab — make sure that tab is showing
  const codeTab = document.querySelector('.tab[data-tab="code"]')
  if (codeTab && !codeTab.classList.contains('active')) codeTab.click()
  const title = document.getElementById('viewer-title')
  if (title) title.textContent = `runtime/${file} — read-only`
  const v = ensureViewer()
  v.setState(makeViewerState(text))
  document.getElementById('viewer-overlay')?.classList.add('open')
  // the outline sidebar stays visible beside the preview — point it at this file
  setOutlineOverride({ doc: () => v.state.doc.toString(), view: () => v })
}
