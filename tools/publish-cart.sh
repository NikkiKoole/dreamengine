#!/bin/sh
# publish-cart.sh — build cart(s) for the web, commit, push → live on
# https://nikkikoole.github.io/dreamengine/ a minute later.
#
#   tools/publish-cart.sh <name> [<name>...]
#
# Steps: node tools/build-site.js <names> (wasm + gallery into site/),
# then commit ONLY site/ and push. The pages.yml workflow does the deploy.
# Aborts if anything outside site/ is already staged (another agent's WIP —
# see CLAUDE.md "parallel-agent commit hazards").
set -e
cd "$(dirname "$0")/.."

# --no-build: site/<name>/ is already compiled (the editor's publish button
# builds straight into it) — skip the build, just finish the git leg.
NOBUILD=0
if [ "${1:-}" = "--no-build" ]; then NOBUILD=1; shift; fi

[ $# -ge 1 ] || { echo "usage: tools/publish-cart.sh [--no-build] <cart> [<cart>...]"; exit 1; }

[ $NOBUILD -eq 1 ] || node tools/build-site.js "$@"

git add site/
# the write-back keeps repo source and site in sync: stage the published carts'
# own sources too (and ONLY those — the guard below still protects everything else)
allow='^site/'
for n in "$@"; do
  for f in "tools/carts/$n.c" "tools/carts/$n.cart.js"; do
    [ -f "$f" ] && git add "$f"
  done
  allow="$allow|^tools/carts/$n\."
done
strays=$(git diff --cached --name-only | grep -Ev "$allow" || true)
if [ -n "$strays" ]; then
  echo "⚠ refusing to commit — unexpected files are staged (someone else's WIP?):"
  echo "$strays"
  exit 1
fi
if git diff --cached --quiet; then
  echo "nothing new to publish"
  exit 0
fi

git commit -m "site: publish $*"
git push
echo "✓ pushed — deploy: https://github.com/NikkiKoole/dreamengine/actions"
echo "  live in ~1 min: https://nikkikoole.github.io/dreamengine/"
