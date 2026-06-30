# 002 — Context Assembly

> This note captures a discovery made during the evolution of DreamEngine.

**Status**
Incorporated

**Date**
2026-06-26

**Confidence**
High

---

## Observation

As DreamEngine grows, documentation should not merely be written. It should be assembled into task-specific context.

Rather than expecting agents to discover the correct documents, the system should construct the smallest relevant context automatically.

## Why this matters

Large context windows do not replace good context selection.

The quality of context depends more on relevance than on size.

## Implications

DreamEngine may eventually benefit from a context builder that assembles design documents, ADRs, carts, metadata and related notes into a task-specific briefing.

## Open questions

- How should context packs be generated?
- Which relationships are stronger than tags?
- How should stale documentation be detected?

## Related notes

- 001-protocol
- 003-curation
- 006-the-evolution-of-the-workflow
- 007-the-evolution-of-documentation

---

## Outcome (2026-06-30)

The "assemble the smallest relevant task-specific context automatically" idea is realized in tooling that cites this note: `tools/build-context.js` (one cart's de:meta + the prose docs that mention it), `tools/topic-brief.js` (the horizontal feature dossier), and `tools/orient.js` (the cold-start front door). The note's open questions about stale-doc detection and stronger-than-tag links are partly addressed by `tools/doc-status.js` + `tools/design-board.js` and `tools/lint-xrefs.js` (docs that should cross-link but don't).
