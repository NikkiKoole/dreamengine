// Exposes the C engine seam + save API to Swift.
// Phase 2: the REAL dreamengine (engine.h → studio.c/raylib_compat.c). The spike stand-ins
// (canvas.{c,h}/audio.{c,h}) are no longer compiled by any target — the app AND the AUv3 both
// run the real engine now; the files are kept only as spike history.
#include "engine.h"
#include "save.h"
