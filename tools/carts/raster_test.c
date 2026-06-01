#include "studio.h"

// Rasterization pixel-accuracy test cart.
//
// Z     = toggle outline on/off
// X     = toggle dithered fill on/off
// C     = cycle page (1 = curved: circ/oval/rrect, 2 = poly: tri/ngon/star/poly)
// SPACE = analyse (one-shot snapshot — press again to return to live)
//
// Live mode: clean shapes, no flicker
// After SPACE:
//   frame 0: draw clean shapes so pget captures them
//   frame 1: pget reads clean frame → run analysis → paint markers on canvas
//   frame 2+: canvas stays frozen (no cls) — markers remain visible
//             HUD redrawn each frame to stay crisp
//
// Yellow = fill edge pixel with no adjacent outline
// Pink   = outline pixel with no adjacent fill

#define BG      CLR_DARK_BLUE
#define FILL_A  CLR_WHITE
#define FILL_B  CLR_MEDIUM_GREY
#define OUT_C   CLR_RED
#define M_FILL  CLR_YELLOW
#define M_OUT   CLR_PINK
#define HUD_H   16
#define NPAGES  2

static bool show_outline = true;
static bool show_dither  = false;
static int  page         = 0;

// freeze state machine
#define FS_LIVE     0   // drawing live
#define FS_SETUP    1   // draw clean once so pget captures it
#define FS_ANALYSE  2   // run analysis, paint markers
#define FS_FROZEN   3   // stay still, just repaint HUD
static int  fs          = FS_LIVE;
static int  last_count  = 0;

static bool is_fill(int c) { return c == FILL_A || c == FILL_B; }
// the whole shape = fill ∪ outline ∪ already-painted markers (markers only ever
// replace pixels that were solid, so counting them keeps the region intact and
// stops the in-place painting from feeding back into later neighbour reads).
static bool is_solid(int c) { return is_fill(c) || c == OUT_C || c == M_FILL || c == M_OUT; }

// The invariant: the outline must be EXACTLY the boundary of the fill region.
// Reconstruct the region from the single render (fill ∪ outline) and check:
//   - any OUTLINE pixel with no background neighbour is buried inside the fill
//     (the outline strayed off the true edge)            → pink
//   - any FILL pixel that itself touches background was not covered by the
//     outline (a gap / the outline pulled away from it)  → yellow
// This is global, not a local guess, so it catches a 1px offset at any angle
// yet never false-flags a sharp tip (a correct tip pixel IS on the boundary).
static int n_pink = 0, n_yellow = 0;
static int analyse(void) {
    int n = 0; n_pink = 0; n_yellow = 0;
    for (int y = 0; y < SCREEN_H - HUD_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int c = pget(x, y);
            bool touches_bg = !is_solid(pget(x-1,y)) || !is_solid(pget(x+1,y)) ||
                              !is_solid(pget(x,y-1)) || !is_solid(pget(x,y+1));
            if (c == OUT_C) {
                if (!touches_bg) { pset(x, y, M_OUT); n++; n_pink++; }       // buried outline
            } else if (show_outline && is_fill(c)) {
                if (touches_bg) { pset(x, y, M_FILL); n++; n_yellow++; }     // uncovered fill edge
            }
        }
    }
    return n;
}

// page 1 — curved primitives (unified on pixel-center coverage)
static void draw_page1(void) {
    int cxs[] = {16, 40, 74, 118};
    int crs[] = {6, 10, 16, 22};
    for (int i = 0; i < 4; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        circfill(cxs[i], 50, crs[i], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) circ(cxs[i], 50, crs[i], OUT_C);
    }
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    ovalfill(20, 115, 14, 8,  FILL_A);
    ovalfill(70, 115, 20, 10, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) {
        oval(20, 115, 14, 8,  OUT_C);
        oval(70, 115, 20, 10, OUT_C);
    }
    // rounded rects — varied corner radii (small, big, near-stadium)
    int rx[] = {170, 235, 170, 250, 20, 115};
    int ry[] = { 28,  28,  90,  90, 140, 140};
    int rw[] = { 50,  70,  60,  55,  80,  90};
    int rh[] = { 40,  50,  34,  55,  36,  38};
    int rr[] = { 10,  18,   6,  24,  12,   4};
    for (int i = 0; i < 6; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        rrectfill(rx[i], ry[i], rw[i], rh[i], rr[i], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) rrect(rx[i], ry[i], rw[i], rh[i], rr[i], OUT_C);
    }
}

// page 2 — polygon primitives (still on the old GPU/fan paths — under test)
static void draw_page2(void) {
    // triangles — tri/trifill now share the polygon coverage path
    int tris[][6] = {
        { 15,28,  58,40,  30,76},
        { 72,76, 118,28, 128,72},
        {140,30, 195,34, 170,78},
    };
    for (int i = 0; i < 3; i++) {
        int *t = tris[i];
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        trifill(t[0],t[1], t[2],t[3], t[4],t[5], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) tri(t[0],t[1], t[2],t[3], t[4],t[5], OUT_C);
    }
    // regular polygons: pentagon, hexagon, octagon
    int   gx[]   = {32, 85, 142};
    int   gr[]   = {18, 20,  18};
    int   gs[]   = { 5,  6,   8};
    float grot[] = {-90, 0,  22};
    for (int i = 0; i < 3; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        ngonfill(gx[i], 122, gr[i], gs[i], grot[i], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) ngon(gx[i], 122, gr[i], gs[i], grot[i], OUT_C);
    }
    // stars: 5-point (top right), 6-point (mid)
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    starfill(250,  52, 24, 10, 5, -90, FILL_A);
    starfill(205, 122, 20,  9, 6,   0, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) {
        star(250,  52, 24, 10, 5, -90, OUT_C);
        star(205, 122, 20,  9, 6,   0, OUT_C);
    }
    // convex polygon (quad)
    int quad[] = {255,98, 305,108, 298,150, 258,142};
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    polyfill(quad, 4, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) poly(quad, 4, OUT_C);
}

static void draw_shapes(void) {
    cls(BG);
    if (page == 0) draw_page1();
    else           draw_page2();
}

static void draw_hud(void) {
    rectfill(0, SCREEN_H - HUD_H, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_TINY);
    print(str("Z out:%s  X dith:%s  C page %d/%d  SPACE:analyse",
              show_outline ? "ON " : "OFF",
              show_dither  ? "ON " : "OFF",
              page + 1, NPAGES),
          2, SCREEN_H - 14, CLR_LIGHT_GREY);
    if (fs == FS_FROZEN || fs == FS_ANALYSE) {
        int col = last_count == 0 ? CLR_LIME_GREEN : M_FILL;
        print(str("mismatches: %d", last_count), 2, SCREEN_H - 6, col);
        if (fs == FS_FROZEN)
            print("frozen - SPACE to resume", SCREEN_W - 110, SCREEN_H - 6, CLR_INDIGO);
    }
    font(FONT_NORMAL);
#ifdef DE_TRACE
    // make the harness legible: page + state + last analysis result, every frame
    watch("state", "pg=%d out=%d dith=%d fs=%d", page + 1, show_outline, show_dither, fs);
    watch("mismatches", "%d", last_count);
    watch("split", "pink=%d yellow=%d", n_pink, n_yellow);
#endif
}

void update(void) {
    if (btnp(0, BTN_A)) { show_outline = !show_outline; if (fs != FS_LIVE) fs = FS_SETUP; }
    if (btnp(0, BTN_B)) { show_dither  = !show_dither;  if (fs != FS_LIVE) fs = FS_SETUP; }
    if (keyp('C'))      { page = (page + 1) % NPAGES;   if (fs != FS_LIVE) fs = FS_SETUP; }
    if (keyp(KEY_SPACE)) fs = (fs == FS_LIVE) ? FS_SETUP : FS_LIVE;
}

void draw(void) {
    if (fs == FS_LIVE) {
        draw_shapes();
        draw_hud();
        return;
    }
    if (fs == FS_SETUP) {
        // draw clean — pget will capture this next frame
        draw_shapes();
        draw_hud();
        fs = FS_ANALYSE;
        return;
    }
    if (fs == FS_ANALYSE) {
        // pget reads the clean FS_SETUP frame — run analysis, paint markers
        draw_shapes();
        last_count = analyse();
        draw_hud();
        fs = FS_FROZEN;
        return;
    }
    // FS_FROZEN: canvas keeps the markers — just repaint HUD on top
    draw_hud();
}
