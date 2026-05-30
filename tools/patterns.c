#include "studio.h"

// FILL PATTERNS — rectfill_pat() tiles a 4×4 bitmask across a rect, so two
// colors make a texture. Stack different densities and you get a dither
// GRADIENT; use the named patterns for instant backgrounds and panels.
//
// A: change the color pair

// six densities, sparse → full, for a fake gradient
static const int DITH[6]  = { 0x0000, 0x8020, 0xA0A0, 0xA5A5, 0xFAFA, 0xFFFF };
static const int PATS[6]  = { FILL_CHECKER, FILL_DOTS, FILL_HLINES, FILL_VLINES, FILL_DIAG, FILL_GRID };
static const char *NAME[6] = { "CHECK", "DOTS", "HLINE", "VLINE", "DIAG", "GRID" };
static int pair;
static int randpat = 0xA5A5, lastSec = -1;

void update(void) {
    if (btnp(0, BTN_A) || btnp(1, BTN_A)) pair = (pair + 1) % 4;

    int s = (int)now();                       // re-roll a random 16-bit pattern once per second
    if (s != lastSec) { lastSec = s; randpat = rnd(0x10000); }
}

void draw(void) {
    // high-contrast dark→light pairs so the dither gradient really reads
    int lo[4] = { CLR_DARKER_BLUE, CLR_DARK_PURPLE, CLR_DARK_GREEN, CLR_BLACK };
    int hi[4] = { CLR_BLUE,        CLR_PINK,        CLR_LIME_GREEN, CLR_ORANGE };
    int a = lo[pair], b = hi[pair];

    // 1) dither gradient background: each band is denser than the one above it
    for (int i = 0; i < 6; i++)
        rectfill_pat(0, i * 34, SCREEN_W, 34, DITH[i], b, a);

    const char *t = "FILL PATTERNS";
    print_scaled(t, (SCREEN_W - text_width(t) * 2) / 2, 8, CLR_WHITE, 2);

    // a RANDOM pattern, re-rolled every second — proof that any 16-bit value works
    int bx = 70, by = 42, bw = 180, bh = 52;
    print_centered("random pattern, new every second:", by - 9, CLR_LIGHT_GREY);
    rectfill_pat(bx, by, bw, bh, randpat, b, a);
    rect(bx, by, bw, bh, CLR_WHITE);
    print_centered(str("0x%04X", randpat & 0xFFFF), by + bh + 3, CLR_YELLOW);

    // 2) a swatch of each named pattern
    for (int i = 0; i < 6; i++) {
        int x = 6 + i * 51, y = 130;
        rectfill_pat(x, y, 46, 28, PATS[i], b, a);
        rect(x, y, 46, 28, CLR_WHITE);
        print(NAME[i], x + 4, y + 30, CLR_LIGHT_GREY);
    }

    print_centered("A: change colors", SCREEN_H - 12, CLR_WHITE);
}
