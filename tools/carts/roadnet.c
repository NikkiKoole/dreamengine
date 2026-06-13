#include "studio.h"
#include "ui.h"
#include <stdio.h>

// ============================================================================
// ROADNET  —  a SPLINE arterial network over an infinite heightmap.   (rung 1)
//
//   intro panel      drag the SLIDERS (world re-rolls live behind the panel),
//                    ROLL = fresh seed, EXPLORE / ENTER = dismiss the panel
//   ◄▲▼►  /  WASD   pan across the (infinite) world
//   mouse wheel      zoom in / out (a bit), pivoting on the screen centre
//   SPACE           deterministic jump to fresh scenery
//   R               new seed (a fresh, repeatable world)
//   M               re-open the setup panel
//   G               toggle the CELL-BORDER overlay  (the seam test)
//   H               hide HUD (clean screenshot)
//
// ── what this is ─────────────────────────────────────────────────────────────
// The thing procgen-places.md parked as "the wall": ARCED, non-axis-aligned roads
// in a deterministic, infinite world. The crack (see docs/design/roadnet.md): a
// NODE-LATTICE spline is local even though a *grown* spline isn't. Each coarse cell
// hashes to at most one POI node (worldgen's trick); a link between two nodes is a
// Catmull-Rom whose 4 control points are node(C-d), node(C), node(C+d), node(C+2d)
// — every one a PURE FUNCTION of cell coords. So any chunk recomputes the identical
// curve with no global pass: infinite + deterministic + seam-true.
//
// CONNECTED by HIERARCHY (rung 1.5): a flat node lattice fragments into clumps (a
// link needs BOTH adjacent cells to hold a node → a sub-percolation random graph).
// Real road networks are connected, so we split into two tiers — the "Main Rib":
//   • HIGHWAYS — hubs on a COARSE lattice (HUB_CS), each linked to its neighbour
//     hubs. A lattice graph is connected by construction → one spine spans the
//     world (broken only by water — honest; bridges are v2).
//   • MINOR ROADS — every town links to its nearest hub (a feeder onto the spine)
//     AND to its nearest other town (a local road). So clusters chain together and
//     a town far from a hub still connects via town→town→…→hub — not everything
//     needs a highway. Degree ~1 each → no criss-cross.
// All tiers are the same pure-function-of-cell-coords spline, so still seam-true.
//
// Borrowed VERBATIM from worldgen.c (the gtascii port): height_at / biome_col /
// passable / hash2 / ifloor. New here: the node tangents + the Hermite ribbon.
//
// Rung 1 milestone = THE SEAM TEST: pan across a cell border (toggle G to see the
// grid) and the same curve must draw from either side. Terrain reaction is the
// cheap honest version — a link whose straight LOS isn't passable is DROPPED.
// Valley-following / bridges / tunnels are v2 with seams left (roadnet.md).
// ============================================================================

#define TILE 4                                  // screen px per world tile at zoom 1
#define ZMIN 0.6f                                // mousewheel zoom range
#define ZMAX 2.5f
// colgrid is sized for the MOST zoomed-OUT case (most tiles on screen): SCREEN/(TILE·ZMIN)
#define MAXCOLS (SCREEN_W * 10 / (TILE * 6) + 3) // = SCREEN_W/(TILE*0.6)+3
#define MAXROWS (SCREEN_H * 10 / (TILE * 6) + 3)
#define SPAWN_X 312.0f
#define SPAWN_Y 8.0f

#define ROAD_R    2                  // road ribbon radius (px) at this zoom

// ── tweakable world params — all 0..1 (driven by the intro-panel sliders) ─────
// Getters map each to its real range; the NODE_CS / HUB_CS macros below call them
// so every existing call site stays unchanged. Defaults reproduce the tuned look.
static float P_hub_space  = 0.167f;  // hub lattice spacing  → 12..60 tiles
static float P_town_space = 0.167f;  // town lattice spacing → 6..30 tiles
static float P_hub_dens   = 0.75f;   // hub presence         → 40..100 %
static float P_town_dens  = 0.444f;  // town presence        → 0..90 %
static float P_jitter     = 0.6f;    // node wiggle off-grid → 0..~0.45·cell
static float P_diag       = 0.25f;   // diagonal shortcuts   → 0..100% of quads (no X)
static float P_sea        = 0.5f;    // sea level            → subtract 0.30..0.60

static int   hub_cs(void)   { return 12 + (int)(P_hub_space  * 48); }   // 12..60
static int   node_cs(void)  { return 6  + (int)(P_town_space * 24); }   // 6..30
static int   hub_pct(void)  { return 40 + (int)(P_hub_dens   * 60); }   // 40..100
static int   town_pct(void) { return (int)(P_town_dens * 90); }         // 0..90
static int   jit(int cs)    { int j = (int)(P_jitter * cs * 0.45f); return j < 1 ? 1 : j; }
static int   diag_pct(void) { return (int)(P_diag * 100); }             // 0..100
static float sea_sub(void)  { return 0.30f + P_sea * 0.30f; }           // 0.30..0.60
#define NODE_CS  node_cs()
#define HUB_CS   hub_cs()

static int mode = 0;                 // 0 = intro/setup panel, 1 = explore

static float camX, camY;             // top-left of view, in world-tile coords
static float seedZ;                  // noise z-slice = the world's "seed"
static float jumpN;                  // SPACE counter → deterministic far jumps
static int   show_grid = 0;          // cell-border overlay (seam test)
static int   show_hud  = 1;
static float zoom = 1.0f;            // mousewheel zoom (ZMIN..ZMAX)
static float P    = TILE;            // pixels per tile = TILE * zoom (set per frame)
static int   vcols, vrows;           // visible tile counts this frame (≤ MAXCOLS/ROWS)

static void view_metrics(void) {     // recompute P + visible tile span from zoom
    P = TILE * zoom;
    vcols = (int)(SCREEN_W / P) + 3; if (vcols > MAXCOLS) vcols = MAXCOLS;
    vrows = (int)(SCREEN_H / P) + 3; if (vrows > MAXROWS) vrows = MAXROWS;
}

// ── deterministic per-cell hash (worldgen's) ─────────────────────────────────
static unsigned hash2(int a, int b) {
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static int ifloor(float v) { int i = (int)v; return (v < 0 && (float)i != v) ? i - 1 : i; }

// ── HEIGHTMAP + RIVERS — worldgen's createZoomedinWorld(), verbatim (scalein 1) ─
static float height_at(float i, float j) {
    float x1 = noise3(i / 100.0f, j / 100.0f, seedZ);   // big continents
    float x2 = noise3(i / 10.0f,  j / 10.0f,  seedZ);   // fine roughness
    float x3 = noise3(i / 30.0f,  j / 30.0f,  seedZ);   // medium hills
    float x = 0.6f * x1 + 0.3f * x3 + 0.1f * x2;
    x = x - sea_sub();                                   // drop sea level (water slider)
    float xr = noise2(i / 40.0f, j / 40.0f); xr = xr * xr;
    if (xr > 0.3f && xr < 0.3f + (0.5f * 0.3f) * 0.5f && x > 0.0f) x = -0.5f;
    xr = noise2(i / 20.0f, j / 20.0f);
    if (xr > 0.3f && xr < 0.3f + (0.05f * 0.3f) * 0.5f && x > 0.0f) x = -0.5f;
    return x;
}
static int biome_col(float x) {
    if (x == -0.5f)  return CLR_DARK_BLUE;     // deep water / river
    if (x <  0.00f)  return CLR_BLUE;          // shallow water
    if (x <  0.01f)  return CLR_YELLOW;        // beach
    if (x <  0.05f)  return CLR_ORANGE;        // dunes
    if (x <  0.10f)  return CLR_PINK;          // scrub
    if (x <  0.20f)  return CLR_GREEN;         // grass
    if (x <  0.30f)  return CLR_DARK_GREEN;    // forest
    if (x <  0.40f)  return CLR_BROWN;         // hills
    if (x <  0.45f)  return CLR_DARK_GREY;     // rock
    return CLR_LIGHT_GREY;                      // peaks
}
static int passable(float x) { return x >= 0.0f && x < 0.4f; }   // road LOS test

// cached colour grid — only regenerated when the base tile / seed / zoom-span moves
static int colgrid[MAXROWS][MAXCOLS];
static int cacheX = 0x7fffffff, cacheY, cacheVC, cacheVR; static float cacheSeed, cacheSea;
static void regen_if_needed(void) {
    int bx = ifloor(camX), by = ifloor(camY);
    float sea = sea_sub();                               // water slider moves terrain
    if (bx == cacheX && by == cacheY && seedZ == cacheSeed && sea == cacheSea
        && vcols == cacheVC && vrows == cacheVR) return;
    cacheX = bx; cacheY = by; cacheSeed = seedZ; cacheSea = sea; cacheVC = vcols; cacheVR = vrows;
    for (int r = 0; r < vrows; r++)
        for (int c = 0; c < vcols; c++)
            colgrid[r][c] = biome_col(height_at((float)(bx + c), (float)(by + r)));
}

// ── POI NODES — at most one per coarse cell, hashed + jittered, on habitable land ─
// Pure function of (cx,cy): same node falls out wherever/whenever you visit. This is
// worldgen's city_at(), kept as the network's node placement.
static int get_node(int cx, int cy, float *wx, float *wy) {
    unsigned h = hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    if (h % 100u >= (unsigned)town_pct()) return 0;      // town-density slider
    int j = jit(NODE_CS);                                // wiggle slider × cell
    float jx = (float)((int)((h >> 7)  % (2*j+1)) - j);
    float jy = (float)((int)((h >> 15) % (2*j+1)) - j);
    float x = cx * NODE_CS + NODE_CS * 0.5f + jx;
    float y = cy * NODE_CS + NODE_CS * 0.5f + jy;
    if (!passable(height_at(x, y))) return 0;            // habitable land only
    *wx = x; *wy = y;
    return 1;
}

// ── HUBS — the highway backbone, on a coarse lattice. Independent hash layer from
// towns (different constants), denser (~85%) so the lattice graph stays connected. ─
static int get_hub(int cx, int cy, float *wx, float *wy) {
    unsigned h = hash2(cx * 374761393u + 5, cy * 668265263u + 3);
    if (h % 100u >= (unsigned)hub_pct()) return 0;       // hub-density slider
    int j = jit(HUB_CS);                                 // wiggle slider × cell
    float jx = (float)((int)((h >> 6)  % (2*j+1)) - j);
    float jy = (float)((int)((h >> 14) % (2*j+1)) - j);
    float x = cx * HUB_CS + HUB_CS * 0.5f + jx;
    float y = cy * HUB_CS + HUB_CS * 0.5f + jy;
    if (!passable(height_at(x, y))) return 0;            // habitable land only
    *wx = x; *wy = y;
    return 1;
}

// world tile → screen px (centre of tile) — P = TILE*zoom, set per frame
static int sxp(float wx) { return (int)((wx - camX) * P + P * 0.5f); }
static int syp(float wy) { return (int)((wy - camY) * P + P * 0.5f); }

static int road_clear(float ax, float ay, float bx, float by);   // defined below

// ── the SEAM: all link geometry comes out of here. v2 (valley-following, bridges)
// inserts/tags control points HERE so render + future road_at() can't disagree. ──
// Catmull-Rom through (p0,p1,p2,p3); p1->p2 is the drawn span. t in [0,1].
static void catmull(float p0,float p1,float p2,float p3,float t,float *out){
    float t2=t*t, t3=t2*t;
    *out = 0.5f*( (2*p1) + (-p0+p2)*t
                + (2*p0-5*p1+4*p2-p3)*t2
                + (-p0+3*p1-3*p2+p3)*t3 );
}

// The 4 forward link directions (the reverse 4 are owned by the neighbour cell, so
// each physical link is enumerated exactly once → no double-draw, no seam fight).
static const int DIR[4][2] = {{1,0},{0,1},{1,1},{1,-1}};

// Stroke one Catmull-Rom span (p1->p2, shaped by p0/p3) as overlapping circles.
static void stroke_catmull(float p0x, float p0y, float p1x, float p1y,
                           float p2x, float p2y, float p3x, float p3y, int r, int col) {
    float ddx = p2x - p1x, ddy = p2y - p1y;
    float len_px = fsqrt(ddx*ddx + ddy*ddy) * P;         // on-screen length (zoom-aware)
    int steps = (int)(len_px / (r > 0 ? r : 1)); if (steps < 6) steps = 6;
    for (int s = 0; s <= steps; s++) {
        float t = (float)s / steps, wx, wy;
        catmull(p0x, p1x, p2x, p3x, t, &wx);
        catmull(p0y, p1y, p2y, p3y, t, &wy);
        circfill(sxp(wx), syp(wy), r, col);
    }
}

// HIGHWAYS — the connected backbone. Hub lattice, forward-only links, Catmull-Rom
// shaped by the hubs beyond each span (reflect when absent). phase 0=casing, 1=centre.
static void draw_highways(int phase) {
    int col = phase ? CLR_LIGHT_GREY : CLR_DARKER_GREY;
    int r   = phase ? 2 : 3;
    int c0 = ifloor(camX / HUB_CS) - 2, c1 = ifloor((camX + vcols) / HUB_CS) + 2;
    int r0 = ifloor(camY / HUB_CS) - 2, r1 = ifloor((camY + vrows) / HUB_CS) + 2;
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1; cy++) {
            float p1x, p1y; if (!get_hub(cx, cy, &p1x, &p1y)) continue;
            for (int d = 0; d < 4; d++) {
                int dx = DIR[d][0], dy = DIR[d][1];
                if (d >= 2) {                                           // diagonal: AT MOST one per quad (no X)
                    int qx = cx, qy = (dy == 1) ? cy : cy - 1;          // the quad this diagonal crosses
                    unsigned qh = hash2(qx * 2246822519u + 7, qy * 3266489917u + 13);
                    if (qh % 100u >= (unsigned)diag_pct()) continue;    // this quad has no diagonal
                    if ((dy == 1) != (int)((qh >> 9) & 1)) continue;    // and it picked the OTHER diagonal
                }
                float p2x, p2y;
                if (!get_hub(cx + dx, cy + dy, &p2x, &p2y)) continue;
                if (!road_clear(p1x, p1y, p2x, p2y)) continue;          // water breaks the spine (v2 bridge)
                float p0x, p0y, p3x, p3y;
                if (!get_hub(cx - dx, cy - dy, &p0x, &p0y)) { p0x = 2*p1x - p2x; p0y = 2*p1y - p2y; }
                if (!get_hub(cx + 2*dx, cy + 2*dy, &p3x, &p3y)) { p3x = 2*p2x - p1x; p3y = 2*p2y - p1y; }
                stroke_catmull(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, r, col);
            }
        }
}

// nearest present hub to a town, searched over the 5x5 hub cells around it (pure fn).
// Wide search so a town near a hub-gap still finds the backbone instead of orphaning.
static int nearest_hub(float tx, float ty, float *hx, float *hy) {
    int hc = ifloor(tx / HUB_CS), hr = ifloor(ty / HUB_CS);
    float best = 1e18f; int found = 0;
    for (int a = hc - 2; a <= hc + 2; a++)
        for (int b = hr - 2; b <= hr + 2; b++) {
            float wx, wy; if (!get_hub(a, b, &wx, &wy)) continue;
            float dx = wx - tx, dy = wy - ty, dd = dx*dx + dy*dy;
            if (dd < best) { best = dd; *hx = wx; *hy = wy; found = 1; }
        }
    return found;
}

// nearest OTHER town, over the 5x5 town cells around it (pure fn, self excluded)
static int nearest_town(float tx, float ty, float *nx, float *ny) {
    int tc = ifloor(tx / NODE_CS), tr = ifloor(ty / NODE_CS);
    float best = 1e18f; int found = 0;
    for (int a = tc - 2; a <= tc + 2; a++)
        for (int b = tr - 2; b <= tr + 2; b++) {
            float wx, wy; if (!get_node(a, b, &wx, &wy)) continue;
            if (wx == tx && wy == ty) continue;          // self
            float dx = wx - tx, dy = wy - ty, dd = dx*dx + dy*dy;
            if (dd < best) { best = dd; *nx = wx; *ny = wy; found = 1; }
        }
    return found;
}

// MINOR ROADS — each town links to its nearest hub (feeder onto the backbone) AND to
// its nearest other town (a local road). The local roads chain clusters together so a
// town far from any hub still connects via town→town→…→hub — not everything needs a
// highway. Degree ~1 each, so no criss-cross. Owned by the town; mutual nearest pairs
// double-draw harmlessly (identical pixels).
static void draw_feeders(int phase) {
    int col = phase ? CLR_LIGHT_GREY : CLR_DARK_GREY;
    int r   = phase ? ROAD_R - 1 : ROAD_R;
    int m = 5;
    int c0 = ifloor(camX / NODE_CS) - m, c1 = ifloor((camX + vcols) / NODE_CS) + m;
    int r0 = ifloor(camY / NODE_CS) - m, r1 = ifloor((camY + vrows) / NODE_CS) + m;
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1; cy++) {
            float tx, ty; if (!get_node(cx, cy, &tx, &ty)) continue;
            float hx, hy;
            if (nearest_hub(tx, ty, &hx, &hy) && road_clear(tx, ty, hx, hy))
                stroke_catmull(2*tx - hx, 2*ty - hy, tx, ty, hx, hy, 2*hx - tx, 2*hy - ty, r, col);
            float nx, ny;
            if (nearest_town(tx, ty, &nx, &ny) && road_clear(tx, ty, nx, ny))
                stroke_catmull(2*tx - nx, 2*ty - ny, tx, ty, nx, ny, 2*nx - tx, 2*ny - ty, r, col);
        }
}

// straight LOS passability between two nodes (worldgen's road_clear)
static int road_clear(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float len = fsqrt(dx * dx + dy * dy);
    if (len < 0.5f) return 0;
    int steps = (int)len;
    for (int s = 1; s < steps; s++) {
        float t = (float)s / steps;
        if (!passable(height_at(ax + dx * t, ay + dy * t))) return 0;
    }
    return 1;
}

static void draw_nodes(void) {
    // towns (feeder tier) — small red/orange dots
    int c0 = ifloor(camX / NODE_CS) - 2, c1 = ifloor((camX + vcols) / NODE_CS) + 2;
    int r0 = ifloor(camY / NODE_CS) - 2, r1 = ifloor((camY + vrows) / NODE_CS) + 2;
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1; cy++) {
            float wx, wy; if (!get_node(cx, cy, &wx, &wy)) continue;
            int px = sxp(wx), py = syp(wy);
            circfill(px, py, 3, CLR_BLACK);
            circfill(px, py, 2, (hash2(cx, cy) & 1) ? CLR_RED : CLR_ORANGE);
        }
    // hubs (backbone tier) — bigger white markers, the cities on the spine
    int h0 = ifloor(camX / HUB_CS) - 2, h1 = ifloor((camX + vcols) / HUB_CS) + 2;
    int v0 = ifloor(camY / HUB_CS) - 2, v1 = ifloor((camY + vrows) / HUB_CS) + 2;
    for (int cx = h0; cx <= h1; cx++)
        for (int cy = v0; cy <= v1; cy++) {
            float wx, wy; if (!get_hub(cx, cy, &wx, &wy)) continue;
            int px = sxp(wx), py = syp(wy);
            circfill(px, py, 4, CLR_BLACK);
            circfill(px, py, 3, CLR_WHITE);
        }
}

static void draw_grid_overlay(void) {       // the seam test aid: NODE_CS cell borders
    for (int cx = ifloor(camX / NODE_CS); cx * NODE_CS < camX + vcols; cx++) {
        int x = (int)((cx * NODE_CS - camX) * P);
        line(x, 0, x, SCREEN_H, CLR_DARK_PURPLE);
    }
    for (int cy = ifloor(camY / NODE_CS); cy * NODE_CS < camY + vrows; cy++) {
        int y = (int)((cy * NODE_CS - camY) * P);
        line(0, y, SCREEN_W, y, CLR_DARK_PURPLE);
    }
}

static void hud(void) {
    char buf[64];
    float cx = camX + vcols / 2.0f, cy = camY + vrows / 2.0f;
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("ROADNET", 4, 2, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "x %d  y %d", (int)cx, (int)cy);
    print(buf, 80, 2, CLR_MEDIUM_GREY);
    if (show_grid) print("[grid]", 160, 2, CLR_DARK_PURPLE);
    print_centered("\x18\x19\x1a\x1b pan   wheel zoom   SPACE jump   R seed   M setup   G grid",
                   SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
}

// ── INTRO / SETUP PANEL — a glass strip of sliders over the LIVE world preview.
// The world is drawn full-screen behind this (draw() runs draw_world first), and
// every slider edits a P_* the generators read, so the world re-rolls live as you
// drag. No separate preview renderer — the whole right of the screen IS the preview.
static void draw_setup_panel(void) {
    char buf[48];
    int PW = 96;
    fillp(FILL_CHECKER, -1); rectfill(0, 0, PW, SCREEN_H, CLR_BLACK); fillp_reset();
    line(PW, 0, PW, SCREEN_H, CLR_DARK_GREY);
    print("ROADNET", 4, 4, CLR_LIGHT_GREY);

    font(FONT_SMALL);
    ui_begin();
    static const char *L[7] = { "hub gap", "town gap", "hub dens", "town dens",
                                "wiggle", "diag", "water" };
    float *pp[7] = { &P_hub_space, &P_town_space, &P_hub_dens, &P_town_dens,
                     &P_jitter, &P_diag, &P_sea };
    for (int i = 0; i < 7; i++) ui_slider(pp[i], 4, 16 + i * 12, 88, L[i]);
    int roll = ui_button(4, 104, 88, 14, "ROLL");
    int go   = ui_button(4, 122, 88, 16, "EXPLORE \x10");
    ui_end();

    snprintf(buf, sizeof buf, "seed #%u", (unsigned)(seedZ * 1000) % 100000u);
    print(buf, 4, 144, CLR_MEDIUM_GREY);
    print("drag = re-roll live", 4, SCREEN_H - 9, CLR_DARKER_GREY);
    font(FONT_NORMAL);

    if (roll) seedZ += 0.37f;                    // fresh world, same params
    if (go || keyp(KEY_ENTER)) mode = 1;
}

static void draw_world(void) {
    view_metrics();                                  // P + visible span from zoom
    regen_if_needed();
    int bx = ifloor(camX), by = ifloor(camY);
    int sz = (int)P + 1;                             // +1 so zoomed tiles never gap
    for (int r = 0; r < vrows; r++)
        for (int c = 0; c < vcols; c++) {
            int sx = (int)(((bx + c) - camX) * P), sy = (int)(((by + r) - camY) * P);
            rectfill(sx, sy, sz, sz, colgrid[r][c]);
        }
    if (show_grid) draw_grid_overlay();
    draw_highways(0); draw_feeders(0);    // all casings first...
    draw_highways(1); draw_feeders(1);    // ...then all centres (no cross-cover)
    draw_nodes();
}

void init(void) {
    zoom = 1.0f; view_metrics();
    camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f;
}

void update(void) {
    // mousewheel zoom, pivoting on the screen centre (clamped ZMIN..ZMAX)
    float w = mouse_wheel();
    if (w != 0) {
        float oldp = TILE * zoom;
        float ccx = camX + SCREEN_W * 0.5f / oldp, ccy = camY + SCREEN_H * 0.5f / oldp;
        zoom += (w > 0 ? 1 : -1) * 0.15f * zoom;
        if (zoom < ZMIN) zoom = ZMIN;
        if (zoom > ZMAX) zoom = ZMAX;
        float np = TILE * zoom;
        camX = ccx - SCREEN_W * 0.5f / np; camY = ccy - SCREEN_H * 0.5f / np;
    }

    float pan = 1.3f / zoom;                     // constant on-screen pan speed
    if (btn(0, BTN_LEFT)  || key('A')) camX -= pan;
    if (btn(0, BTN_RIGHT) || key('D')) camX += pan;
    if (btn(0, BTN_UP)    || key('W')) camY -= pan;
    if (btn(0, BTN_DOWN)  || key('S')) camY += pan;

    if (keyp(KEY_SPACE)) {                       // deterministic jump to fresh scenery
        jumpN += 1.0f;
        unsigned h = hash2((int)(jumpN * 911), 7);
        camX += (float)((int)(h % 4000u) - 2000) + 800.0f;
        camY += (float)((int)((h >> 11) % 4000u) - 2000) + 800.0f;
    }
    if (keyp('R')) { zoom = 1.0f; view_metrics(); camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f; seedZ += 0.37f; jumpN = 0; }
    if (keyp('G')) show_grid = !show_grid;
    if (keyp('H')) show_hud  = !show_hud;
    if (keyp('M')) mode = 0;                     // re-open the setup panel
}

void draw(void) {
    draw_world();                                // the live preview, always
    if (mode == 0) draw_setup_panel();
    else if (show_hud) hud();
}
