// polystress — a deterministic torture test for the SOFTWARE polygon rasterizer
// (polyfill → poly_fill_cov → poly_inside → plot_pat). Built to (a) pin the CPU
// hard so a profile of the rasterizer is unambiguous, and (b) be byte-reproducible
// frame-by-frame so an optimization can be VALIDATED pixel-identical.
//
// Everything is a pure function of the frame counter F — no time, no random — so
// `play.js polystress --dump` produces the same PNGs on every run and on every build.
//   stress:    node tools/play.js polystress run                       (watch it churn)
//   profile:   editor ▶ run, then trigger profiler_request  (or play.js, headless)
//   validate:  node tools/play.js polystress script /dev/null --headless --frames 90 \
//                   --dump /tmp/poly_before --dump-every 15 ; shasum /tmp/poly_before/*.png
//   …optimize polyfill, rebuild, dump to /tmp/poly_after, diff the two shasum lists.
//
// Coverage — each block targets a distinct path through the rasterizer:
//   1 BIG CONCAVE stars  — large fill AREA × even-odd test: the per-pixel poly_inside cost
//   2 GRID of stars      — high polyfill COUNT: per-call setup (bbox, clamp) overhead
//   3 ROAD strips        — convex tapering quads, exactly roadlab's fill_strip case
//   4 DITHER poly        — fillp() active → plot_pat's patterned branch (not plain pset)
//   5 OFFSCREEN giant    — bbox runs way off-canvas → exercises poly_clamp_scan

#include "studio.h"
#include <math.h>
#include <stdio.h>

#define TAU 6.28318530718f

static int F = 0;
void update(void) { F++; }

// concave n-point star into xy[], returns vertex count (2*points)
static int mkstar(float cx, float cy, float rOut, float rIn, int points, float rot, int *xy) {
    int n = points * 2;
    for (int i = 0; i < n; i++) {
        float a = rot + (float)i / n * TAU;
        float r = (i & 1) ? rIn : rOut;
        xy[i*2]   = (int)(cx + cosf(a) * r);
        xy[i*2+1] = (int)(cy + sinf(a) * r);
    }
    return n;
}

// a roadlab-style strip: a centreline swept into convex quad segments, filled one
// polyfill per segment (this is fill_strip, the convex-trapezoid case, distilled)
static void strip(float x0, float y0, float x1, float y1, float halfw, int segs, int col) {
    float dx = x1 - x0, dy = y1 - y0;
    float L = sqrtf(dx*dx + dy*dy); if (L < 0.001f) L = 1;
    float nx = -dy / L * halfw, ny = dx / L * halfw;
    for (int i = 0; i < segs; i++) {
        float t0 = (float)i / segs, t1 = (float)(i+1) / segs;
        float ax = x0 + dx*t0, ay = y0 + dy*t0;
        float bx = x0 + dx*t1, by = y0 + dy*t1;
        int xy[8] = { (int)(ax+nx),(int)(ay+ny), (int)(bx+nx),(int)(by+ny),
                      (int)(bx-nx),(int)(by-ny), (int)(ax-nx),(int)(ay-ny) };
        polyfill(xy, 4, col);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    float ph = F * 0.04f;                       // global rotation phase, deterministic
    int xy[64];

    // ── 1 · big concave stars: maximum fill area through the even-odd test ──
    for (int s = 0; s < 3; s++) {
        float cx = SCREEN_W * (0.25f + 0.25f*s);
        float cy = SCREEN_H * 0.5f;
        float r  = 46 + 8 * sinf(ph + s);
        int n = mkstar(cx, cy, r, r*0.45f, 9, ph * (s+1) * 0.5f, xy);
        polyfill(xy, n, 8 + (s + F/20) % 8);
    }

    // ── 2 · a dense grid of small spinning stars: polyfill COUNT stress ──
    int GX = 10, GY = 7;
    for (int gy = 0; gy < GY; gy++)
        for (int gx = 0; gx < GX; gx++) {
            float cx = (gx + 0.5f) * SCREEN_W / GX;
            float cy = (gy + 0.5f) * SCREEN_H / GY;
            float rot = ph * 2 + (gx + gy);
            int n = mkstar(cx, cy, 11, 4.5f, 5, rot, xy);
            polyfill(xy, n, 16 + (gx + gy + F/10) % 16);
        }

    // ── 3 · sweeping road strips: convex quads, the roadlab fill_strip case ──
    for (int k = 0; k < 6; k++) {
        float a = ph * 0.7f + k * (TAU / 6);
        float cx = SCREEN_W * 0.5f, cy = SCREEN_H * 0.5f;
        strip(cx, cy, cx + cosf(a)*140, cy + sinf(a)*100, 6.f, 14, CLR_DARK_GREY);
    }

    // ── 4 · a dithered concave poly: drives plot_pat's fillp() patterned branch ──
    fillp(FILL_CHECKER, -1);
    int n = mkstar(SCREEN_W*0.5f, SCREEN_H*0.5f, 70, 26, 7, -ph, xy);
    polyfill(xy, n, CLR_WHITE);
    fillp_reset();

    // ── 5 · a giant mostly-offscreen poly: exercises poly_clamp_scan ──
    {
        float a = ph * 0.3f;
        int g[8] = { (int)(-200 + cosf(a)*40), -150,
                     (int)(SCREEN_W+200),        (int)(-80 + sinf(a)*40),
                     (int)(SCREEN_W+150),        SCREEN_H+200,
                     -180,                        SCREEN_H+120 };
        // draw it faint by dithering so it doesn't wipe the scene, but still fills
        fillp(FILL_DOTS, -1);
        polyfill(g, 4, CLR_DARK_BLUE);
        fillp_reset();
    }

    // readout
    print("polystress", 4, 4, CLR_WHITE);
    char buf[32];
    int polys = 3 + GX*GY + 6*14 + 1 + 1;       // fills issued this frame
    snprintf(buf, sizeof buf, "F=%d polyfills=%d", F, polys);
    print(buf, 4, 12, CLR_LIGHT_GREY);
#ifdef DE_TRACE
    watch("polyfills", "%d", polys);
#endif
}
