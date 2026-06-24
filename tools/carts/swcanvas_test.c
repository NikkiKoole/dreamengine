// swcanvas_test — Phase-0 software-canvas probe cart. cls + pset + pset_rgb only (the v0a surface),
// in a deliberately ASYMMETRIC pattern so orientation bugs (Y-flip) are obvious. A/B:
//   node tools/play.js swcanvas_test run --headless --frames 1 --dump /tmp/gpu      (GPU path)
//   DE_SOFTWARE_CANVAS=on node tools/play.js swcanvas_test run --headless --frames 1 --dump /tmp/sw
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    for (int x = 4; x < 120; x++) pset(x, 8, CLR_WHITE);     // TOP bar (catches upside-down)
    for (int y = 8; y < 90; y++)  pset(6, y, CLR_YELLOW);    // LEFT bar (catches mirror)
    for (int i = 0; i < 80; i++)  pset(20 + i, 20 + i, CLR_RED);   // diagonal
    for (int x = 0; x < 64; x++) for (int y = 0; y < 16; y++)      // pset_rgb gradient block
        pset_rgb(140 + x, 30 + y, (x * 4) << 16 | (y * 12) << 8);
}
