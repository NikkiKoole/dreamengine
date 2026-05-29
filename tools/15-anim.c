#include "studio.h"

#define COUNT 7
#define SPEED 3.0f

void draw() {
    cls(CLR_DARK_BLUE);

    // ── top row: all in sync (phase 0) ──────────────────────
    print("phase 0 — all in sync", 4, 4, CLR_LIGHT_GREY);

    for (int i = 0; i < COUNT; i++) {
        int f  = anim(4, SPEED, 0.0f);
        int dy = f < 2 ? f * 8 : (4 - f) * 8;   // 0→8→16→8→0
        int x  = 16 + i * 44;
        circfill(x, 36 + dy, 12, CLR_RED);
    }

    // ── bottom row: staggered with phase ───────────────────
    print("phase = i/count — staggered", 4, 100, CLR_LIGHT_GREY);

    for (int i = 0; i < COUNT; i++) {
        float phase = (float)i / COUNT;
        int f  = anim(4, SPEED, phase);
        int dy = f < 2 ? f * 8 : (4 - f) * 8;
        int x  = 16 + i * 44;
        circfill(x, 130 + dy, 12, CLR_GREEN);
    }

    // ── code sample ─────────────────────────────────────────
    print("spr(base + anim(4, 10, (float)i/n),", 4, 174, CLR_YELLOW);
    print("    x, y);", 4, 184, CLR_YELLOW);
}
