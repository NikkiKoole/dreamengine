// placetest — correctness oracle for the window↔canvas transform (runtime/game_rect.h),
// the touch-controls Phase 1.5 chokepoint. PURE math, no raylib — runs anywhere.
//
// Asserts the property that retires the one hard-to-fix risk (silent, library-wide coordinate
// breakage when placement offsets/scales the game rect): the two transforms are exact mutual
// inverses, so a tap maps to the right game pixel at ANY placement. It covers not just the
// Phase-1.5 identity rect but offset+scaled rects (deck/rails-shaped), so it already guards
// Phase 2 — the bug would surface here, off-device, instead of on a phone.
//
//   ROUND TRIP   canvas -> window -> canvas == identity   (over a grid of sample points)
//   GR_PLACE     the Phase-1.5 stub returns a full-window identity placement
//
// Run: clang tools/det-probes/placetest.c -o /tmp/placetest && /tmp/placetest   (exit 0 = pass)
#include <stdio.h>

#define SCALE 4
#define SCREEN_W 320
#define SCREEN_H 200
#include "../../runtime/game_rect.h"

static int fails = 0;

// canvas pixel -> window -> canvas must return the same pixel for every sampled canvas point.
static void check_roundtrip(GameRect gr, const char *label) {
    int bad = 0;
    for (int cy = 0; cy <= SCREEN_H; cy += 7) {
        for (int cx = 0; cx <= SCREEN_W; cx += 11) {
            float wx = gr_canvas_to_win_x(gr, cx);
            float wy = gr_canvas_to_win_y(gr, cy);
            int rx = gr_win_to_canvas_x(gr, wx);
            int ry = gr_win_to_canvas_y(gr, wy);
            if (rx != cx || ry != cy) {
                if (bad < 4) printf("  [%s] MISMATCH canvas(%d,%d) -> win(%.2f,%.2f) -> canvas(%d,%d)\n",
                                    label, cx, cy, wx, wy, rx, ry);
                bad++;
            }
        }
    }
    if (bad) { printf("  [%s] %d round-trip mismatches\n", label, bad); fails++; }
    else     printf("  [%s] round-trip identity OK\n", label);
}

int main(void) {
    // 1) the Phase-1.5 stub: any window/screen size -> full-window overlay at native scale.
    Placement p = gr_place(1280, 800, SCREEN_W, SCREEN_H, (float)SCALE);
    if (p.mode != PLACE_OVERLAY || p.game.x != 0.0f || p.game.y != 0.0f || p.game.scale != (float)SCALE) {
        printf("  gr_place stub: expected full-window identity, got mode=%d game=(%.1f,%.1f,%.2f)\n",
               p.mode, p.game.x, p.game.y, p.game.scale);
        fails++;
    } else printf("  gr_place stub returns full-window identity OK\n");

    // 2) round-trip over a matrix of placements: the live identity rect, and offset+scaled rects
    //    shaped like the Phase-2 deck (game pushed down, smaller scale) and rails (game pushed right).
    check_roundtrip((GameRect){ 0.0f, 0.0f, (float)SCALE }, "identity x4");
    check_roundtrip((GameRect){ 0.0f, 0.0f, 2.0f },         "identity x2");
    check_roundtrip((GameRect){ 0.0f, 96.0f, 3.0f },        "deck (down, x3)");
    check_roundtrip((GameRect){ 140.0f, 0.0f, 4.0f },       "rails (right, x4)");
    check_roundtrip((GameRect){ 37.0f, 52.0f, 2.5f },       "offset frac (x2.5)");

    printf(fails ? "FAIL (%d)\n" : "PASS\n", fails);
    return fails ? 1 : 0;
}
