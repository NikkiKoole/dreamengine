#include "studio.h"

// PALETTE LAB — try on a new default palette without touching anything else.
// The Layer-1/1b probe from docs/design/palette-and-color.md: our 32 colors are
// lifted verbatim from PICO-8, and before blend tables (STATUS #18) bake that
// in deeper, candidates need to be SEEN against the corpus's worst cases.
//
// Built on the EXPERIMENTAL palette_hex(i, 0xRRGGBB) and the 64-slot palette
// (slots 32-63 mirror 0-31 by default, so nothing else changed): every scene is
// drawn with normal CLR_* indices and re-skins instantly when the palette
// changes. Sprite editor, cart format, docs: all untouched.
//
// Four candidates (keys 1-4):
//   1  PICO-8 (shipped)        — the baseline we're trying to replace
//   2  ENDESGA 32              — 32 role-mapped, upper half mirrors
//   3  RESURRECT 64 (full)     — 32 role-mapped + the other 32 in slots 32-63
//   4  E32 + 32 DERIVED        — the "blended in-betweens" idea: slots 32-63
//      are sRGB midpoints of ramp neighbours + hue bridges, computed here
//      (OKLab mixing would be better — judging that muddiness is the point)
//
// The BLEND scene is the 32-vs-64 verdict machine: a glass pane (AVG) and a
// glow (ADD) are blended per-pixel against a backdrop, blendlab-style — the
// mix is done in RGB then snapped to the NEAREST color of the active palette,
// so candidates with more/denser colors visibly band less. Same indices in,
// different fidelity out.
//
// Role-mapping notes: slot 8 must stay "the red", ramps must still ramp, or
// every existing cart scrambles. Weak fits are flagged below (E32 has no lime,
// no second dark plum, no near-black navy pair — three dup slots); the
// Resurrect cut filled every role. That asymmetry is already a finding.
//
// CAVEAT while a custom palette is active: pal(c0,c1) would inject the SHIPPED
// c1 color (base_palette stays the texel-matching key) — scenes avoid pal().
//
// CONTROLS: 1-4 palette · LEFT/RIGHT scene (swatches+ramps / sunset /
// night glow / portrait / blend test).

// ---- the shipped palette, for the blend table + completeness ------------
static const int PAL_PICO[32] = {
    0x000000, 0x1d2b53, 0x7e2553, 0x008751, 0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
    0xff004d, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xff77a8, 0xffccaa,
    0x291814, 0x111d35, 0x422136, 0x125359, 0x742f29, 0x49333b, 0xa28879, 0xf3ef7d,
    0xbe1250, 0xff6c24, 0xa8e72e, 0x00b543, 0x065ab5, 0x754665, 0xff6e59, 0xff9d81,
};

// ENDESGA 32 (by ENDESGA, lospec.com/palette-list/endesga-32), pico-role order.
// Weak fits: 17 dups 1, 18 dups 16, 26 dups 11.
static const int PAL_E32[32] = {
    0x181425, 0x262b44, 0x68386c, 0x265c42, 0xbe4a2f, 0x3a4466, 0xc0cbdc, 0xffffff,
    0xff0044, 0xf77622, 0xfee761, 0x63c74d, 0x0099db, 0x8b9bb4, 0xf6757a, 0xe8b796,
    0x3e2731, 0x262b44, 0x3e2731, 0x193c3e, 0x733e39, 0x5a6988, 0xc28569, 0xead4aa,
    0xa22633, 0xd77643, 0x63c74d, 0x3e8948, 0x124e89, 0xb55088, 0xe43b44, 0xe4a672,
};

// RESURRECT 64 (by Kerrie Lake, lospec.com/palette-list/resurrect-64):
// 32 role-mapped to the pico slots + the remaining 32 in the upper half.
static const int PAL_R32[32] = {
    0x2e222f, 0x484a77, 0x6b3e75, 0x165a4c, 0x9e4539, 0x3e3546, 0x9babb2, 0xffffff,
    0xe83b3b, 0xfb6b1d, 0xf9c22b, 0x1ebc73, 0x4d9be6, 0x7f708a, 0xed8099, 0xfdcbb0,
    0x45293f, 0x323353, 0x753c54, 0x0b5e65, 0x6e2727, 0x625565, 0xab947a, 0xfbff86,
    0xae2334, 0xcd683d, 0xd5e04b, 0x239063, 0x4d65b4, 0xa24b6f, 0xf68181, 0xfca790,
};
static const int PAL_R64X[32] = {   // the other 32 of Resurrect 64
    0x966c6c, 0x694f62, 0xc7dcd0, 0xb33831, 0xea4f36, 0xf57d4a, 0xf79617, 0x7a3045,
    0xe6904e, 0xfbb954, 0x4c3e24, 0x676633, 0xa2a947, 0x91db69, 0xcddf6c, 0x313638,
    0x374e4a, 0x547e64, 0x92a984, 0xb2ba90, 0x0b8a8f, 0x0eaf9b, 0x30e1b9, 0x8ff8e2,
    0x8fd3ff, 0x905ea9, 0xa884f3, 0xeaaded, 0xcf657f, 0x831c5d, 0xc32454, 0xf04f78,
};

// candidate 4: E32 + derived in-betweens — midpoint pairs (ramp neighbours +
// hue bridges so cross-ramp blends have somewhere to land)
static const int MIX_PAIRS[32][2] = {
    {0,1},{1,12},{12,6},{6,7},        // sky
    {16,4},{4,9},{9,10},{10,23},      // warm
    {19,3},{3,27},{27,11},{11,26},    // green
    {0,21},{21,5},{5,22},{22,15},     // grey -> warm light
    {16,20},{20,30},{30,15},{15,31},  // skin
    {8,24},{24,2},{2,29},{29,14},     // reds/purples
    {1,28},{28,12},{13,5},{14,15},    // blues + misc
    {8,12},{9,12},{11,12},{10,7},     // hue bridges (glow-over-water class)
};

#define NPAL 4
static int cur_pal;        // 0 pico, 1 e32, 2 r64 full, 3 e32+derived
static int pal_n;          // how many DISTINCT colors the candidate brings (32 or 64)
static int cur_hex[64];    // active palette as hexes — feeds the blend tables
static int scene;
#define NSCENES 5

static int mix_hex(int a, int b) {   // plain sRGB midpoint (OKLab would be kinder)
    return ((((a >> 16 & 255) + (b >> 16 & 255)) / 2) << 16) |
           ((((a >> 8  & 255) + (b >> 8  & 255)) / 2) << 8)  |
            (((a       & 255) + (b       & 255)) / 2);
}

static void apply_palette(int which) {
    cur_pal = which;
    const int *lo = which == 0 ? PAL_PICO : which == 2 ? PAL_R32 : PAL_E32;
    for (int i = 0; i < 32; i++) cur_hex[i] = lo[i];
    if (which == 2)      { for (int i = 0; i < 32; i++) cur_hex[32 + i] = PAL_R64X[i]; pal_n = 64; }
    else if (which == 3) { for (int i = 0; i < 32; i++) cur_hex[32 + i] = mix_hex(PAL_E32[MIX_PAIRS[i][0]], PAL_E32[MIX_PAIRS[i][1]]); pal_n = 64; }
    else                 { for (int i = 0; i < 32; i++) cur_hex[32 + i] = lo[i]; pal_n = 32; }
    for (int i = 0; i < 64; i++) palette_hex(i, cur_hex[i]);
}

void init(void) {
    apply_palette(2);    // boot on full Resurrect so the 64-slot experiment shows
    scene = 4;           // ...on the blend scene, the verdict machine
}

void update(void) {
    for (int i = 0; i < NPAL; i++) if (keyp('1' + i)) apply_palette(i);
    if (keyp(KEY_RIGHT)) scene = (scene + 1) % NSCENES;
    if (keyp(KEY_LEFT))  scene = (scene + NSCENES - 1) % NSCENES;
}

// ---- scene 0: swatches + dithered ramps --------------------------------
static void scene_swatches(void) {
    cls(CLR_BLACK);
    for (int i = 0; i < 64; i++) {
        int x = 8 + (i % 16) * 19, y = 20 + (i / 16) * 15;
        rectfill(x, y, 17, 12, i);
        if (i % 16 == 0) { font(FONT_TINY); print(str("%d", i), x - 7, y + 3, CLR_MEDIUM_GREY); font(FONT_NORMAL); }
    }
    static const int ramps[5][6] = {
        { 0, 1, 12, 6, 7, -1 }, { 16, 4, 9, 10, 23, -1 }, { 19, 3, 27, 11, 26, -1 },
        { 0, 21, 5, 22, 15, -1 }, { 16, 20, 30, 15, 31, -1 },
    };
    static const char *names[5] = { "SKY", "WARM", "GREEN", "GREY", "SKIN" };
    for (int r = 0; r < 5; r++) {
        int y = 88 + r * 20;
        font(FONT_SMALL); print(names[r], 8, y + 3, CLR_LIGHT_GREY); font(FONT_NORMAL);
        int x = 44;
        for (int i = 0; i < 5 && ramps[r][i + 1] >= 0; i++) { hgradient(x, y, 53, 12, ramps[r][i], ramps[r][i + 1]); x += 53; }
    }
}

// ---- scene 1: sunset — gradients + dither glow + fog -------------------
static void scene_sunset(void) {
    vgradient(0, 0, SCREEN_W, 60, CLR_DARK_BLUE, CLR_DARK_PURPLE);
    vgradient(0, 60, SCREEN_W, 50, CLR_DARK_PURPLE, CLR_ORANGE);
    vgradient(0, 110, SCREEN_W, 20, CLR_ORANGE, CLR_YELLOW);
    fillp(FILL_DOTS, -1);    circfill(240, 112, 26, CLR_YELLOW);       fillp_reset();
    fillp(FILL_CHECKER, -1); circfill(240, 112, 18, CLR_LIGHT_YELLOW); fillp_reset();
    circfill(240, 112, 11, CLR_WHITE);
    vgradient(0, 130, SCREEN_W, 50, CLR_BLUE, CLR_DARK_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 134, SCREEN_W, 30, CLR_LIGHT_YELLOW); fillp_reset();
    for (int x = 0; x < SCREEN_W; x += 4) {
        int h = 18 + (int)(noise(x * 0.02f) * 22);
        rectfill(x, 130 - h, 4, h, CLR_DARK_GREEN);
    }
    fillp(FILL_CHECKER, -1); rectfill(0, 104, SCREEN_W, 26, CLR_LIGHT_GREY); fillp_reset();
    vgradient(0, 180, SCREEN_W, 20, CLR_PEACH, CLR_BROWN);
}

// ---- scene 2: night glow — the galerijflat "lights gap" ----------------
static void scene_night(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK, CLR_DARKER_BLUE);
    rectfill(20, 60, 120, 130, CLR_DARKER_GREY);
    for (int wy = 0; wy < 4; wy++)
        for (int wx = 0; wx < 5; wx++) {
            int lit = (wx * 7 + wy * 3) % 3 != 0;
            rectfill(30 + wx * 22, 70 + wy * 30, 12, 16, lit ? CLR_LIGHT_YELLOW : CLR_DARKER_BLUE);
            if (lit) { fillp(FILL_CHECKER, -1); rect(29 + wx * 22, 69 + wy * 30, 14, 18, CLR_YELLOW); fillp_reset(); }
        }
    for (int i = 0; i < 3; i++) {
        int lx = 180 + i * 55;
        line(lx, 130, lx, 188, CLR_DARK_GREY);
        fillp(FILL_DOTS, -1);    circfill(lx, 128, 22, CLR_ORANGE);       fillp_reset();
        fillp(FILL_CHECKER, -1); circfill(lx, 128, 13, CLR_LIGHT_YELLOW); fillp_reset();
        circfill(lx, 128, 4, CLR_WHITE);
    }
    rectfill(0, 188, SCREEN_W, 12, CLR_DARKER_GREY);
    print_outline("NEON", 226, 60, CLR_PINK, CLR_DARK_PURPLE);
    print_outline("BAR",  238, 72, CLR_GREEN, CLR_BLUE_GREEN);
}

// ---- scene 3: portrait — skin tones are where palettes get cruel -------
static void scene_portrait(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_INDIGO, CLR_DARKER_PURPLE);
    int cx = 160, cy = 96;
    ovalfill(cx, cy + 78, 52, 36, CLR_TRUE_BLUE);
    rectfill(cx - 9, cy + 34, 18, 16, CLR_LIGHT_PEACH);
    ovalfill(cx, cy, 40, 46, CLR_LIGHT_PEACH);
    ovalfill(cx, cy - 28, 42, 24, CLR_BROWN);
    ovalfill(cx - 30, cy - 10, 12, 22, CLR_BROWN);
    ovalfill(cx + 30, cy - 10, 12, 22, CLR_BROWN);
    fillp(FILL_CHECKER, -1); ovalfill(cx - 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();
    fillp(FILL_CHECKER, -1); ovalfill(cx + 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();
    ovalfill(cx - 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx - 13, cy - 2, CLR_BROWNISH_BLACK);
    ovalfill(cx + 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx + 15, cy - 2, CLR_BROWNISH_BLACK);
    ovalfill(cx, cy + 26, 8, 3, CLR_DARK_PEACH);
    fillp(FILL_CHECKER, -1); ovalfill(cx + 18, cy + 8, 18, 30, CLR_PEACH); fillp_reset();
    static const int skin[6] = { 16, 20, 4, 30, 15, 31 };
    for (int i = 0; i < 6; i++) rectfill(8 + i * 16, 168, 14, 14, skin[i]);
    font(FONT_SMALL); print("SKIN FAMILY 16 20 4 30 15 31", 8, 184, CLR_LIGHT_GREY); font(FONT_NORMAL);
}

// ---- scene 4: blend test — the 32-vs-64 verdict machine ----------------
// blendlab's trick against the live candidate: mix in RGB, snap to NEAREST
// active color. More/denser colors -> the snap lands closer -> less banding.
static int nearest(int hex) {
    int br = hex >> 16 & 255, bg = hex >> 8 & 255, bb = hex & 255, best = 0, bd = 1 << 30;
    for (int i = 0; i < pal_n; i++) {
        int dr = (cur_hex[i] >> 16 & 255) - br, dg = (cur_hex[i] >> 8 & 255) - bg, db = (cur_hex[i] & 255) - bb;
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static int add_hex(int a, int b) {
    int r = (a >> 16 & 255) + (b >> 16 & 255), g = (a >> 8 & 255) + (b >> 8 & 255), bl = (a & 255) + (b & 255);
    return (min(r, 255) << 16) | (min(g, 255) << 8) | min(bl, 255);
}
static void scene_blend(void) {
    // backdrop: wide bands of a sky->warm run (each pixel's dst is knowable)
    static const int bands[8] = { 1, 12, 6, 7, 10, 9, 8, 2 };
    for (int b = 0; b < 8; b++) rectfill(b * 40, 12, 40, 180, bands[b]);
    // glass pane (AVG with white) and shadow pane (AVG with black), per-pixel
    for (int py = 30; py < 80; py++)
        for (int px = 24; px < 296; px++)
            pset(px, py, nearest(mix_hex(cur_hex[bands[px / 40]], 0xffffff)));
    for (int py = 132; py < 172; py++)
        for (int px = 24; px < 296; px++)
            pset(px, py, nearest(mix_hex(cur_hex[bands[px / 40]], 0x000000)));
    // additive glow disc (ADD orange), per-pixel, radius falloff in two steps
    for (int py = 76; py < 136; py++)
        for (int px = 130; px < 190; px++) {
            int dx2 = px - 160, dy2 = py - 106, d2 = dx2 * dx2 + dy2 * dy2;
            if (d2 > 30 * 30) continue;
            int glow = d2 < 15 * 15 ? 0xff8800 : 0x7f4400;
            pset(px, py, nearest(add_hex(cur_hex[bands[px / 40]], glow)));
        }
    font(FONT_SMALL);
    print("GLASS (avg white)", 26, 24, CLR_WHITE);
    print("SHADOW (avg black)", 26, 126, CLR_WHITE);
    print(str("ADD glow   snapped to %d colors", pal_n), 116, 174, CLR_WHITE);
    font(FONT_NORMAL);
}

void draw(void) {
    switch (scene) {
        case 0: scene_swatches(); break;
        case 1: scene_sunset();   break;
        case 2: scene_night();    break;
        case 3: scene_portrait(); break;
        case 4: scene_blend();    break;
    }
    static const char *pnames[NPAL] = { "1 PICO-8 (shipped)", "2 ENDESGA 32", "3 RESURRECT 64", "4 E32+32 DERIVED" };
    static const char *snames[NSCENES] = { "SWATCHES+RAMPS", "SUNSET", "NIGHT GLOW", "PORTRAIT", "BLEND TEST" };
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(pnames[cur_pal], 4, 2, CLR_WHITE);
    print_right(str("%s  %d/%d", snames[scene], scene + 1, NSCENES), 316, 2, CLR_LIGHT_GREY);
    font(FONT_SMALL);
    rectfill(0, 193, SCREEN_W, 7, CLR_BLACK);
    print("1-4 palette   LEFT/RIGHT scene   same indices, new clothes", 4, 194, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
