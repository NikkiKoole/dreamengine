# 0004 — Degree-based trig (not radians or turns)
Date: 2026-05-29 · Status: accepted

## Context
Angle math needs a unit. The references disagree: PICO-8 uses **turns** (0..1), BlitzMax
uses **degrees**, C's `math.h` uses **radians**. This affects `angle_to`, `dx`/`dy`,
`sin_deg`/`cos_deg`, and every cart that moves at an angle.

## Decision
Expose **degrees** throughout. `angle_to` returns degrees (0 = right, 90 = down);
`dx`/`dy` and `sin_deg`/`cos_deg` take degrees. `dx`/`dy` return **float** (not the int
the first sketch had) so repeated `x += dx(...)` keeps sub-pixel motion.

## Why
Degrees are taught in school — the lowest-surprise unit for teens. They chain cleanly:
`angle_to` → `dx`/`dy` all speak the same unit. Radians are jargon; turns are clever but
opaque to a beginner.

## Consequences
- Carts hold position in floats when accumulating `dx`/`dy`.
- No radian-based trig in the public API; `math.h` stays internal.
