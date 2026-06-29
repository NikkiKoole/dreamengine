#ifndef DE_COLOR_H
#define DE_COLOR_H

// ============================================================================
// color.h — the engine's universal 32-bit RGBA color type.
//
// Phase A of the platform seam (docs/design/engine-portability.md): the engine
// core owns its color type instead of borrowing Raylib's, so non-Raylib backends
// (iOS, Switch) compile without Raylib's vocabulary.
//
//   - Raylib backends (desktop, web): DeColor IS Raylib's Color. Identical
//     {unsigned char r,g,b,a} layout → zero-cost alias, bit-identical output,
//     and DeColor values pass straight to Raylib draw calls with no conversion.
//   - DE_NO_RAYLIB backends (iOS/Switch): a standalone, layout-compatible struct.
//
// Set -DDE_NO_RAYLIB on a backend that has no Raylib (the software-canvas path).
// ============================================================================

#ifdef DE_NO_RAYLIB
typedef struct { unsigned char r, g, b, a; } DeColor;
#else
#include "raylib.h"
typedef Color DeColor;
#endif

#endif // DE_COLOR_H
