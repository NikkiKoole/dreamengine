---

title: Generated Cart Index

summary: >
The global cart index should become a generated artifact. Each cart owns its
own metadata, allowing multiple agents to work independently without creating
merge conflicts.

concepts:

* carts
* metadata
* indexing
* agents
* workflow

## status: incorporated

# Generated Cart Index

## Observation

As DreamEngine grows, more AI agents work on multiple carts simultaneously.

Today, many of those agents eventually need to modify the same file:

```text
editor/public/carts/index.json
```

This creates unnecessary merge conflicts and makes the global index a bottleneck.

The problem is not the index itself.

The problem is that many independent artifacts all depend on one manually maintained file.

## Principle

Each cart should own its own metadata.

No cart should require another cart to be modified simply because its metadata changed.

The cart becomes the source of truth.

The global index becomes a generated view.

## Proposed Direction

Instead of editing one shared file:

```text
cart A
cart B
cart C

        │
        ▼

editor/public/carts/index.json
```

each cart contains its own metadata:

```text
cart A
  ├── cart.c
  └── metadata

cart B
  ├── cart.c
  └── metadata

cart C
  ├── cart.c
  └── metadata
```

During publishing or repository builds, a tool scans every cart and generates:

```text
editor/public/carts/index.json
```

The generated index remains exactly what the editor and launcher need, but nobody edits it directly.

## Benefits

### Better multi-agent collaboration

Agents only modify the cart they are working on.

They no longer compete to edit a shared index.

### Single source of truth

Metadata lives with the cart rather than being duplicated elsewhere.

### Easier maintenance

Adding, removing or renaming a cart affects only that cart.

The index is rebuilt automatically.

### Better tooling

Future tools can inspect carts directly instead of relying on one manually maintained registry.

## Relationship to Self-Describing Artifacts

This is a direct application of the self-describing artifact principle.

The cart should describe itself.

Indexes, launchers, websites and documentation should all be generated from that information rather than becoming separate sources of truth.

## Open Question

The exact location of cart metadata remains an implementation detail.

Possible approaches include:

* embedded metadata inside `cart.c`
* a small `cart.json`
* another lightweight sidecar format

The important decision is architectural rather than technical:

**Cart metadata should be local to the cart.**

The global index should be generated.

## Summary

A generated `index.json` is not merely a convenience.

It removes a synchronization problem that becomes increasingly expensive as more humans and AI agents work on the repository simultaneously.

By allowing each cart to own its own metadata, DreamEngine becomes easier to scale, easier to automate and easier to evolve.

---

## Outcome (2026-06-30)

Fully shipped as the note proposed: `tools/build-cart-index.js` GENERATES `editor/public/carts/index.json` from each cart's embedded `de:meta` block (the "metadata in cart.c" option), with `--check` gating staleness; the cart is the sole source of truth and appears in the index iff it has a `de:meta`. The multi-agent merge conflict that motivated the note is explicitly retired (CLAUDE.md hazard #2), and `tools/lint-carts.js` asserts the index stays in sync. Schema in `docs/design/cart-metadata.md`.
