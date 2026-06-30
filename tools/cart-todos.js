#!/usr/bin/env node
// cart-todos.js — the navigable view over every cart's `de:meta.todo[]` polish punch-list.
// Each cart owns its own todo list inside its de:meta block (validated by lint-carts.js,
// NOT emitted to index.json); this tool gathers them into one report so the scattered
// per-cart notes read as a single backlog — the data form of docs/cart-polish-punchlist.md.
// A sibling of build-field-notes.js / cart-status.js: it only READS the carts.
//
//   node tools/cart-todos.js                 # list every cart with open todos (+ totals)
//   node tools/cart-todos.js <name>          # just that cart's todos
//   node tools/cart-todos.js --grep <term>   # only todos matching a substring (case-insensitive)
//   node tools/cart-todos.js --count         # one line: N todos across M carts
//
// collectCartTodos() is exported so the ★ todo board (build-design-board.js) can show the
// per-cart backlog beside the design docs — one place to see all pending work.
//
// To CLEAR an item: delete it from the cart's de:meta.todo[] (drop the whole field when empty).

const fs = require("fs");
const path = require("path");
const { readMeta, CARTS } = require("./build-cart-index.js");

// gather { name, title, todos[] } for every cart whose de:meta carries a non-empty todo[].
// opts.grep filters todos by substring; opts.only restricts to one cart.
function collectCartTodos({ grep = null, only = null } = {}) {
  const carts = [];
  for (const f of fs.readdirSync(CARTS).sort()) {
    if (!f.endsWith(".c")) continue;
    const name = f.slice(0, -2);
    if (only && name !== only) continue;
    let m;
    try { m = readMeta(fs.readFileSync(path.join(CARTS, f), "utf8"), name); }
    catch { continue; } // malformed de:meta is lint-carts' job to report, not ours
    if (!m || !Array.isArray(m.todo) || m.todo.length === 0) continue;
    let todos = m.todo;
    if (grep) todos = todos.filter((t) => t.toLowerCase().includes(grep.toLowerCase()));
    if (todos.length) carts.push({ name, title: m.title || name, todos });
  }
  return carts;
}

module.exports = { collectCartTodos };

// ---- CLI ----
if (require.main === module) {
  const args = process.argv.slice(2);
  const has = (f) => args.includes(f);
  const valAfter = (f) => { const i = args.indexOf(f); return i >= 0 ? args[i + 1] : null; };
  const grep = valAfter("--grep");
  const only = args.find((a) => !a.startsWith("--") && a !== grep) || null;

  const carts = collectCartTodos({ grep, only });
  const total = carts.reduce((n, c) => n + c.todos.length, 0);

  if (has("--count")) {
    console.log(`${total} open todo(s) across ${carts.length} cart(s)`);
    process.exit(0);
  }
  if (only && !carts.length) {
    console.log(`${only}: no open todos.`);
    process.exit(0);
  }
  for (const c of carts) {
    console.log(`\n${c.name}  (${c.title})`);
    for (const t of c.todos) console.log(`  • ${t}`);
  }
  console.log(`\n${total} open todo(s) across ${carts.length} cart(s)${grep ? ` matching "${grep}"` : ""}.`);
}
