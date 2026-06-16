#include "studio.h"
#include <stdio.h>
#include <math.h>

// ============================================================================
// LOTFILL — the street-level CONTENT language, slice 1: the `scatter` atom.
//
//   ◄ ► ▲ ▼ / WASD   pan        Z / X (or wheel)  zoom
//   R                new seed   TAB  field overlay (tint by cover kind)
//
// ── what this is ─────────────────────────────────────────────────────────────
// roadnet/procplaces build the PARTITION (roads → blocks → lots, + the rural land
// between cities). This cart fills it. Design: docs/design/streetlevel-content.md.
// The thesis there: every street-level feature is a BOUNDED REGION + a SEEDED
// FILL-RULE, expressed as a pure fn of (region, hash, zone) that answers draw()
// AND the collision/query call from the SAME code.
//
// Slice 1 is the `scatter` ATOM — the CONTINUOUS-COVER fill-mode (no parcel, no
// identity): forest, meadow, scrub. We DON'T enumerate trees; cover_at(x,y) is a
// field, and the renderer scatters instances per visible JITTERED-GRID cell. Each
// instance is a pure fn of its own scatter cell + seed (the node-lattice lesson:
// lattice instances are local functions; grown sets tear at chunk borders). So the
// forest is infinite, seam-safe, and byte-identical for a given seed.
//
// Two field layers + two scatter layers make it "interesting" with no special cases:
//   • biome field (warped fbm) → cover KIND (bare/grass/meadow/forest)
//   • density field (finer fbm) → clearings in the forest, flower clumps in the meadow
//   • CANOPY scatter (coarse pitch) → trees, gated by forest density (clearings emerge)
//   • GROUND scatter (fine pitch)   → flowers / bushes / tufts (clumped by density)
//
// ── seeded & repeatable (same trick as procplaces) ───────────────────────────
// noise3(x, y, (float)seed) with an INTEGER seed is a fully independent repeatable
// field. Every instance is hash2(cell, seed) — no global pass, no accumulation.
// ============================================================================

#define BIO_FREQ   0.0016f   // biome frequency (smaller = bigger forests/meadows)
#define BIO_WFREQ  0.0011f   // domain-warp frequency (organic cover edges)
#define BIO_WAMP   140.0f    // domain-warp amplitude (px)
#define DENS_FREQ  0.0090f   // clearing/clump frequency (bigger = patchier)

#define CANOPY_P   16        // canopy scatter pitch (world px) — tree spacing
#define GROUND_P   6         // ground scatter pitch — flowers/bushes/tufts
#define CLUMP_P    44        // flower-clump pitch (one colour per patch)

enum { CV_BARE, CV_GRASS, CV_MEADOW, CV_FOREST, CV_N };
static const char *CV_NAME[CV_N] = { "bare", "grass", "meadow", "forest" };

static int lf_seed = 0;

static unsigned hash2(int a, int b) {                 // per-cell pseudo-random
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static int ifloordiv(int a, int b) {                  // floor division (handles negatives)
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}
static float frac01(unsigned h) { return (h & 4095u) / 4095.0f; }   // 12 bits → [0,1]

// ── the cover field: what grows here, and how densely ────────────────────────
typedef struct { int kind; float dens; } Cover;       // dens ∈ [0,1] within the kind

static void bio_warp(float x, float y, float *ox, float *oy) {
    float z = (float)lf_seed;
    *ox = x + (noise3(x * BIO_WFREQ, y * BIO_WFREQ, z + 11.0f) - 0.5f) * 2.0f * BIO_WAMP;
    *oy = y + (noise3(x * BIO_WFREQ, y * BIO_WFREQ, z + 19.0f) - 0.5f) * 2.0f * BIO_WAMP;
}
static float bio_raw(float x, float y) {              // fbm, 3 octaves
    float z = (float)lf_seed;
    float a = noise3(x * BIO_FREQ,        y * BIO_FREQ,        z);
    float b = noise3(x * BIO_FREQ * 2.3f, y * BIO_FREQ * 2.3f, z + 3.0f);
    float c = noise3(x * BIO_FREQ * 5.1f, y * BIO_FREQ * 5.1f, z + 7.0f);
    return clamp(a * 0.6f + b * 0.28f + c * 0.12f, 0.0f, 1.0f);
}
// THE shared sampler — every cover-query (draw + probe + future collision) goes here.
static Cover cover_at(float x, float y) {
    float wx, wy; bio_warp(x, y, &wx, &wy);
    float b = bio_raw(wx, wy);
    int kind = b < 0.34f ? CV_BARE : b < 0.46f ? CV_GRASS : b < 0.60f ? CV_MEADOW : CV_FOREST;
    float d = noise3(x * DENS_FREQ, y * DENS_FREQ, (float)lf_seed + 5.0f);
    return (Cover){ kind, clamp(d, 0.0f, 1.0f) };
}

// ── base ground tint (cheap filled cells, like terrain bands) ─────────────────
static int ground_col(int cx, int cy, const Cover *cv) {
    unsigned h = hash2(cx * 7 + lf_seed * 911, cy * 13);
    switch (cv->kind) {
        case CV_BARE:   return (h & 1) ? CLR_DARK_BROWN   : CLR_BROWN;
        case CV_GRASS:  return (h & 3) ? CLR_DARK_GREEN   : CLR_MEDIUM_GREEN;
        case CV_MEADOW: return (h & 1) ? CLR_MEDIUM_GREEN : CLR_LIME_GREEN;
        default: {                                                 // forest floor: dappled
            unsigned f = h % 9;
            return f == 0 ? CLR_BROWNISH_BLACK : f < 3 ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN;
        }
    }
}

// ── instance art (drawn in WORLD units; camera_ex scales) ─────────────────────
// tint trios {dark, mid, light}: most trees green, a few pine / autumn / dead.
static void draw_tree(int x, int y, unsigned h, int big) {
    static const int TR[][3] = {
        { CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN },   // broadleaf (common)
        { CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN },
        { CLR_BLUE_GREEN, CLR_DARK_GREEN,   CLR_MEDIUM_GREEN }, // pine
        { CLR_DARK_BROWN, CLR_ORANGE,       CLR_YELLOW },       // autumn (rare)
    };
    unsigned v = h % 8;                                            // broadleaf 5/8, pine 2/8, autumn 1/8
    const int *t = TR[v < 5 ? 0 : v < 7 ? 2 : 3];
    int r = (big ? 5 : 3) + (int)((h >> 6) & 1);                // canopy radius
    rectfill(x, y - 1, 1 + (big ? 1 : 0), 4, CLR_DARK_BROWN);   // trunk
    circfill(x, y - r,     r,     t[0]);                        // shadow base
    circfill(x - 1, y - r - 1, r - 1, t[1]);                    // body
    circfill(x - 2, y - r - 2, (r >= 4 ? 2 : 1), t[2]);         // highlight
}
static void draw_bush(int x, int y, unsigned h) {
    int c = (h & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
    circfill(x, y, 2, c);
    pset(x - 1, y - 1, CLR_LIME_GREEN);
}
static void draw_tuft(int x, int y, unsigned h) {              // grass blades
    int c = (h & 1) ? CLR_MEDIUM_GREEN : CLR_LIME_GREEN;
    line(x,     y, x,     y - 2, c);
    line(x - 1, y, x - 2, y - 2, c);
    line(x + 1, y, x + 2, y - 2, c);
}
static void draw_flower(int x, int y, int col) {
    pset(x, y, CLR_MEDIUM_GREEN);                               // stem dot
    pset(x, y - 2, col);                                       // bloom
}
static int flower_col(int cx, int cy) {                        // one colour per clump
    static const int FC[6] = { CLR_RED, CLR_YELLOW, CLR_WHITE, CLR_ORANGE, CLR_PINK, CLR_MAUVE };
    unsigned h = hash2(ifloordiv(cx * CANOPY_P, CLUMP_P) + lf_seed * 31,
                       ifloordiv(cy * CANOPY_P, CLUMP_P));
    return FC[h % 6];
}

// ════════════════════════════════════════════════════════════════════════════
//  THE WORKBENCH — shared inspection state every atom reads
// ════════════════════════════════════════════════════════════════════════════
// The reusability seam (docs/design/streetlevel-content.md → "the atom is a pure
// function, the driver is what changes"): an atom never touches cam/zoom/seed
// globals — it's handed the view rect and reads only these inspection flags. So the
// SAME atom fn runs here (one atom on stage, overlays on) and in the future world
// cart (composed over thousands of regions). What you tune here is what ships.
#define MAX_LAYERS 4
static bool layer_on[MAX_LAYERS] = { true, true, true, true };  // 1/2/3 peel each layer
static bool dbg = false;           // G: draw the lattice grid + cull (reject) markers
static int  ovl = 0;                // O: field overlay — 0 off / 1 cover-kind / 2 density
static int  n_tree, n_ground, n_reject;   // tallied per draw → HUD readout

static int dens_ramp(float d) {     // density field as a heat colour (low→high)
    return d < 0.25f ? CLR_DARK_BLUE : d < 0.5f ? CLR_INDIGO : d < 0.75f ? CLR_ORANGE : CLR_RED;
}
static const int KIND_COL[CV_N] = { CLR_DARK_BROWN, CLR_LIME_GREEN, CLR_PINK, CLR_DARK_GREEN };

// ── ATOM: scatter ─────────────────────────────────────────────────────────────
// Layers: 0 base tint · 1 ground (flowers/bushes/tufts) · 2 canopy (trees).
enum { SL_BASE, SL_GROUND, SL_CANOPY };

static void scat_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - 8, Rpx = (int)(cx + hwv) + 8;
    int Tpx = (int)(cy - hhv) - 8, Bpx = (int)(cy + hhv) + 8;
    n_tree = n_ground = n_reject = 0;

    // layer 0 — base ground tint (coarse cells that grow when zoomed out, >= ~3 px)
    if (layer_on[SL_BASE]) {
        int gc = 6, step = 1;
        while ((float)gc * step * zoom < 3.0f && step < 6) step++;
        int cellsz = gc * step;
        int g0x = ifloordiv(Lpx, cellsz), g1x = ifloordiv(Rpx, cellsz);
        int g0y = ifloordiv(Tpx, cellsz), g1y = ifloordiv(Bpx, cellsz);
        for (int gy = g0y; gy <= g1y; gy++)
            for (int gx = g0x; gx <= g1x; gx++) {
                int px = gx * cellsz, py = gy * cellsz;
                Cover cv = cover_at(px + cellsz / 2.0f, py + cellsz / 2.0f);
                rectfill(px, py, cellsz, cellsz, ground_col(gx, gy, &cv));
            }
    }

    // debug — draw the CANOPY lattice the candidates sit on (under the instances)
    if (dbg) {
        int c0x = ifloordiv(Lpx, CANOPY_P), c1x = ifloordiv(Rpx, CANOPY_P) + 1;
        int c0y = ifloordiv(Tpx, CANOPY_P), c1y = ifloordiv(Bpx, CANOPY_P) + 1;
        for (int cxi = c0x; cxi <= c1x; cxi++) line(cxi * CANOPY_P, Tpx, cxi * CANOPY_P, Bpx, CLR_DARKER_GREY);
        for (int cyi = c0y; cyi <= c1y; cyi++) line(Lpx, cyi * CANOPY_P, Rpx, cyi * CANOPY_P, CLR_DARKER_GREY);
    }

    bool zg = zoom >= 0.7f, zc = zoom >= 0.30f;   // LOD draw-gates (real behaviour kept)

    // layer 1 — GROUND scatter (flowers / bushes / tufts), one jittered item per cell
    if (layer_on[SL_GROUND] && zg) {
        int c0x = ifloordiv(Lpx, GROUND_P), c1x = ifloordiv(Rpx, GROUND_P);
        int c0y = ifloordiv(Tpx, GROUND_P), c1y = ifloordiv(Bpx, GROUND_P);
        for (int cyi = c0y; cyi <= c1y; cyi++)
            for (int cxi = c0x; cxi <= c1x; cxi++) {
                unsigned h = hash2(cxi + lf_seed * 101, cyi * 3 + 7);
                int wx = cxi * GROUND_P + (int)(frac01(h) * GROUND_P);
                int wy = cyi * GROUND_P + (int)(frac01(h >> 12) * GROUND_P);
                Cover cv = cover_at(wx, wy);
                float roll = frac01(h >> 20);
                bool hit = true;
                if      (cv.kind == CV_MEADOW && roll < 0.10f + cv.dens * 0.55f) draw_flower(wx, wy, flower_col(cxi, cyi));
                else if (cv.kind == CV_FOREST && roll < 0.12f + cv.dens * 0.25f) draw_bush(wx, wy, h);
                else if (cv.kind == CV_GRASS  && roll < 0.18f)                   draw_tuft(wx, wy, h);
                else hit = false;
                if (hit) n_ground++;
            }
    }

    // layer 2 — CANOPY scatter (trees). cy ascending = north→south painter's sort.
    if (layer_on[SL_CANOPY] && zc) {
        int c0x = ifloordiv(Lpx, CANOPY_P), c1x = ifloordiv(Rpx, CANOPY_P);
        int c0y = ifloordiv(Tpx, CANOPY_P), c1y = ifloordiv(Bpx, CANOPY_P);
        for (int cyi = c0y; cyi <= c1y; cyi++)
            for (int cxi = c0x; cxi <= c1x; cxi++) {
                unsigned h = hash2(cxi * 5 + lf_seed * 17, cyi + 3);
                int wx = cxi * CANOPY_P + (int)(frac01(h) * CANOPY_P);
                int wy = cyi * CANOPY_P + (int)(frac01(h >> 12) * CANOPY_P);
                Cover cv = cover_at(wx, wy);
                float roll = frac01(h >> 20);
                float prob = cv.kind == CV_FOREST ? 0.25f + cv.dens * 0.70f   // clearings emerge
                           : cv.kind == CV_MEADOW ? 0.04f                     // lone meadow trees
                           : 0.0f;
                if (roll < prob) { draw_tree(wx, wy, h, cv.kind == CV_FOREST && cv.dens > 0.55f); n_tree++; }
                else if (dbg && prob > 0.0f) { circ(wx, wy, 1, CLR_DARK_RED); n_reject++; }  // culled candidate
            }
    }
}

static const char *scat_probe(float x, float y) {
    static char buf[40];
    Cover cv = cover_at(x, y);
    snprintf(buf, sizeof buf, "%s  dens %.2f", CV_NAME[cv.kind], (double)cv.dens);
    return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  THE ATOM REGISTRY — each atom is a tab; `rows`/`footprint` drop in here next
// ════════════════════════════════════════════════════════════════════════════
typedef struct {
    const char *name;
    int n_layers;
    const char *layer[MAX_LAYERS];
    void (*draw)(float cam_x, float cam_y, float zoom);
    const char *(*probe)(float wx, float wy);
} Atom;

static Atom atoms[] = {
    { "SCATTER", 3, { "base", "ground", "canopy" }, scat_draw, scat_probe },
};
#define N_ATOMS ((int)(sizeof atoms / sizeof atoms[0]))

// ════════════════════════════════════════════════════════════════════════════
//  THE SHELL — free-fly explorer over the current atom
// ════════════════════════════════════════════════════════════════════════════
static float cam_x = 250, cam_y = 18;     // open zoomed on a forest↔meadow boundary (seed 1)
static float zoom  = 2.8f;
static int   seed  = 1;
static int   cur   = 0;                   // current atom

void init(void) { lf_seed = seed; }

void update(void) {
    float pan = 5.0f / zoom;
    if (key(KEY_RIGHT) || key('D')) cam_x += pan;
    if (key(KEY_LEFT)  || key('A')) cam_x -= pan;
    if (key(KEY_DOWN)  || key('S')) cam_y += pan;
    if (key(KEY_UP)    || key('W')) cam_y -= pan;
    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom /= 1.04f;
    zoom += mouse_wheel() * 0.08f * zoom;
    zoom = clamp(zoom, 0.18f, 6.0f);
    if (keyp('R')) { seed++; lf_seed = seed; }
    if (keyp(KEY_TAB)) { cur = (cur + 1) % N_ATOMS;             // next atom (resets layers)
                         for (int i = 0; i < MAX_LAYERS; i++) layer_on[i] = true; }
    for (int i = 0; i < atoms[cur].n_layers; i++)              // 1/2/3 peel layers
        if (keyp('1' + i)) layer_on[i] = !layer_on[i];
    if (keyp('G')) dbg = !dbg;
    if (keyp('O')) ovl = (ovl + 1) % 3;
}

// the field that drives the atom, drawn as a semi-transparent tint over the result
static void draw_overlay(void) {
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int OV = 12;
    int L = ((int)(cx - hwv) / OV - 1) * OV, R = (int)(cx + hwv) + OV;
    int T = ((int)(cy - hhv) / OV - 1) * OV, B = (int)(cy + hhv) + OV;
    fillp(0xA5A5, -1);
    for (int wy = T; wy <= B; wy += OV)
        for (int wx = L; wx <= R; wx += OV) {
            Cover cv = cover_at(wx + OV / 2.0f, wy + OV / 2.0f);
            rectfill(wx, wy, OV, OV, ovl == 1 ? KIND_COL[cv.kind] : dens_ramp(cv.dens));
        }
    fillp_reset();
}

void draw(void) {
    cls(CLR_BLACK);
    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    atoms[cur].draw(cam_x, cam_y, zoom);
    if (ovl) draw_overlay();

    camera(0, 0);
    int mx = SCREEN_W / 2, my = SCREEN_H / 2;
    line(mx - 5, my, mx + 5, my, CLR_WHITE);
    line(mx, my - 5, mx, my + 5, CLR_WHITE);

    // top bar — atom + seed/zoom/overlay
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[72];
    static const char *OVN[3] = { "off", "kind", "dens" };
    int x = print(atoms[cur].name, 3, 1, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  s%d  z%.1f  overlay:%s", seed, (double)zoom, OVN[ovl]);
    print(buf, x, 1, CLR_LIGHT_GREY);

    // layer chips + live counts (small font)
    rectfill(0, 9, SCREEN_W, 8, CLR_BLACK);
    font(FONT_SMALL);
    int lx = 3;
    for (int i = 0; i < atoms[cur].n_layers; i++) {
        snprintf(buf, sizeof buf, "%d:%s ", i + 1, atoms[cur].layer[i]);
        lx = print(buf, lx, 11, layer_on[i] ? CLR_GREEN : CLR_DARK_GREY);
    }
    if (dbg) snprintf(buf, sizeof buf, "   trees:%d grnd:%d culled:%d", n_tree, n_ground, n_reject);
    else     snprintf(buf, sizeof buf, "   trees:%d grnd:%d", n_tree, n_ground);
    print(buf, lx, 11, CLR_LIGHT_GREY);

    // bottom — probe + controls
    rectfill(0, SCREEN_H - 16, SCREEN_W, 16, CLR_BLACK);
    font(FONT_NORMAL);
    print(atoms[cur].probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f), 3, SCREEN_H - 16, CLR_WHITE);
    font(FONT_SMALL);
    print("WASD/ZX move  R seed  TAB atom  1-3 layers  G grid  O overlay",
          3, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
