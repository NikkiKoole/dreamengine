#include "studio.h"
#include <stdio.h>

// ============================================================================
// PROCEDURAL PLACES — a testbed for procedural-generation.
//
//   ◄ ► ▲ ▼ / WASD   pan the camera
//   Z / X            zoom in / out   (mouse wheel too)
//   R                new seed  (a fresh, repeatable world)
//   1 / 2            switch generator
//   TAB              field overlay (tint the world by its underlying field)
//
// ── what this is ─────────────────────────────────────────────────────────────
// One free-fly explorer shell that hosts PLUGGABLE generators. Each generator is
// a SECTION below exposing the same little vtable — draw / reseed / probe /
// field_col — so the shell never knows or cares what it's drawing. Ships with two
// structurally-opposite generators to prove the seam is real:
//
//   1. ROADS & CITIES — a STRUCTURED grid: a noise density field zones the world
//      into city/town/rural/hwy/super, and a nested road lattice is "promoted"
//      per zone. Cities cluster on the noise peaks; valleys get only an arterial.
//      Pan out and you see many cities, organic — not a bullseye.
//   2. TERRAIN        — a pure scalar FIELD: fbm elevation rendered as coloured
//      bands (sea → beach → grass → forest → rock → snow). No structure.
//
// Adding forest / dungeon / cave is just another section + a row in gens[].
//
// ── why one cart, not runtime/ headers (yet) ─────────────────────────────────
// These generators COULD be cart-land library headers (ADR-0006), but that ships
// per the second-customer rule — a real second cart using each. Until one exists
// (e.g. sloop consuming the road field for collision), they live here as in-file
// sections: cleaner to tweak, and honest about reuse. EXTRACT WHEN NEEDED — lift a
// section into runtime/<name>.h (make every fn `static`) the day a second cart
// includes it.
//
// ── seeded & repeatable ──────────────────────────────────────────────────────
// Everything is a pure function of world position + seed. The engine's noise is
// VALUE noise (not Perlin) and noise/noise2 take no seed — but noise2(x,y) ==
// noise3(x,y,0), so noise3(x, y, (float)seed) with an INTEGER seed is a fully
// independent, repeatable field (the z-slice hashes through noise_hash on its own;
// integer z = no interpolation bleed). Same seed → byte-identical world every run.
// ============================================================================

// ════════════════════════════════════════════════════════════════════════════
//  GENERATOR 1 — ROADS & CITIES   (structured grid over a density field)
// ════════════════════════════════════════════════════════════════════════════
// Two layers: a DENSITY FIELD d(x,y)∈0..1 from noise, and a NESTED ROAD LATTICE
// at one fine base pitch. Local density picks a ZONE; the zone "promotes" lattice
// lines — city promotes every line (dense grid), rural every 6th (long straights),
// super every 24th (arterials). Pitches are nested multiples, so a city's fine
// grid melts into the coarse arterials at its edge instead of tearing.
#define ROADS_BASE 100        // finest lattice pitch (px) = one city block
#define ROADS_FREQ 0.00045f   // density-field frequency (smaller = bigger cities)

enum { Z_CITY, Z_TOWN, Z_RURAL, Z_HWY, Z_SUPER, Z_N };   // finest → coarsest
static const int   ZONE_STEP[Z_N] = { 1, 2, 6, 12, 24 };           // pitch = BASE*step → 100/200/600/1200/2400
static const int   ZONE_LANE[Z_N] = { 16, 26, 40, 60, 104 };       // road width (px)
static const char *ZONE_NAME[Z_N] = { "CITY 30", "TOWN 50", "RURAL 80", "HWY 100", "SUPER 120" };
// min density for a level to appear (CITY rarest). Checked finest-first, so the
// finest level whose threshold the point clears is the local zone.
static const float ZONE_MIN[Z_N]  = { 0.74f, 0.60f, 0.46f, 0.30f, 0.0f };

static int roads_seed = 0;
static void roads_reseed(int seed) { roads_seed = seed; }

typedef struct { int zone; bool on_road; float density; } Place;

static float roads_density(float x, float y) {
    float z = (float)roads_seed;
    float a = noise3(x * ROADS_FREQ,        y * ROADS_FREQ,        z);          // landmass
    float b = noise3(x * ROADS_FREQ * 2.9f, y * ROADS_FREQ * 2.9f, z + 1.0f);   // detail octave
    return clamp(a * 0.7f + b * 0.3f, 0.0f, 1.0f);
}
static int roads_zone_of(float d) {
    for (int z = 0; z < Z_N; z++) if (d >= ZONE_MIN[z]) return z;
    return Z_SUPER;
}
static int roads_ifloordiv(int a, int b) {        // floor division (handles negatives)
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}
static unsigned roads_hash2(int a, int b) {       // per-cell pseudo-random (houses, fields)
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// fill one block's interior with packed houses — the detail that screams "city".
static void roads_houses(int wx, int wy, int span, int n) {
    int cw = span / n, ch = span / n;
    if (cw < 4 || ch < 4) return;
    int roof[4] = { CLR_BROWN, CLR_RED, CLR_DARK_PURPLE, CLR_DARK_GREY };
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            unsigned h = roads_hash2(wx + i * 137 + roads_seed * 911, wy + j * 137);
            if ((h & 7) == 0) continue;                          // empty lots / yards
            int hx = wx + i * cw, hy = wy + j * ch;
            rectfill(hx + 1, hy + 1, cw - 2, ch - 2, roof[h & 3]);
            rect(hx + 1, hy + 1, cw - 2, ch - 2, CLR_BROWNISH_BLACK);
        }
}

// cam/zoom match the shell's camera_ex. Enumerates the visible-world rect (wider
// than the screen when zoomed out) and draws at world coords; camera_ex scales.
static void roads_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int L = (int)(cx - hwv), R = (int)(cx + hwv), T = (int)(cy - hhv), B = (int)(cy + hhv);

    int B0 = ROADS_BASE;
    int kx0 = roads_ifloordiv(L, B0) - 1, kx1 = roads_ifloordiv(R, B0) + 1;
    int ky0 = roads_ifloordiv(T, B0) - 1, ky1 = roads_ifloordiv(B, B0) + 1;

    // 0. grass/ground backdrop over the whole visible rect
    rectfill(L - B0, T - B0, (R - L) + 2 * B0, (B - T) + 2 * B0, CLR_DARK_GREEN);

    // 1. per base-cell: zone from local density → fill interior + promote roads
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            int wx = kx * B0, wy = ky * B0;
            float d = roads_density(wx + B0 / 2.0f, wy + B0 / 2.0f);
            int z = roads_zone_of(d), step = ZONE_STEP[z], hw = ZONE_LANE[z] / 2;

            if (z == Z_CITY)      roads_houses(wx + hw + 2, wy + hw + 2, B0 - 2 * hw - 4, 4);
            else if (z == Z_TOWN) roads_houses(wx + hw + 2, wy + hw + 2, B0 - 2 * hw - 4, 3);
            else if (z == Z_RURAL && (roads_hash2(kx, ky) & 1)) {          // patchwork fields
                unsigned hf = roads_hash2(kx + roads_seed, ky);
                rectfill(wx + 4, wy + 4, B0 - 8, B0 - 8, (hf & 2) ? CLR_DARK_GREEN : CLR_BROWN);
            }
            // promoted tarmac: this column's left edge / this row's top edge
            if (kx % step == 0) rectfill(wx - hw, wy, hw * 2, B0, CLR_DARK_GREY);
            if (ky % step == 0) rectfill(wx, wy - hw, B0, hw * 2, CLR_DARK_GREY);
        }

    // 2. curbs + dashed centre lines, sampling zone per line midpoint
    for (int kx = kx0; kx <= kx1; kx++) {
        int wx = kx * B0;
        int z = roads_zone_of(roads_density((float)wx, cy)), step = ZONE_STEP[z], hw = ZONE_LANE[z] / 2;
        if (kx % step != 0) continue;
        line(wx - hw, T, wx - hw, B, CLR_LIGHT_GREY);
        line(wx + hw, T, wx + hw, B, CLR_LIGHT_GREY);
        for (int y = roads_ifloordiv(T, 24) * 24; y < B; y += 24) line(wx, y, wx, y + 11, CLR_YELLOW);
    }
    for (int ky = ky0; ky <= ky1; ky++) {
        int wy = ky * B0;
        int z = roads_zone_of(roads_density(cx, (float)wy)), step = ZONE_STEP[z], hw = ZONE_LANE[z] / 2;
        if (ky % step != 0) continue;
        line(L, wy - hw, R, wy - hw, CLR_LIGHT_GREY);
        line(L, wy + hw, R, wy + hw, CLR_LIGHT_GREY);
        for (int x = roads_ifloordiv(L, 24) * 24; x < R; x += 24) line(x, wy, x + 11, wy, CLR_YELLOW);
    }

    // 3. town roundabouts + city zebra crossings at promoted crossings
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            int wx = kx * B0, wy = ky * B0;
            int z = roads_zone_of(roads_density(wx + B0 / 2.0f, wy + B0 / 2.0f));
            int step = ZONE_STEP[z], hw = ZONE_LANE[z] / 2;
            if (kx % step != 0 || ky % step != 0) continue;
            if (z == Z_TOWN && (roads_hash2(kx, ky) & 3) == 0) {
                circfill(wx, wy, hw, CLR_DARK_GREEN);
                circ(wx, wy, hw, CLR_LIGHT_GREY);
            } else if (z == Z_CITY && (roads_hash2(kx, ky) & 3) == 0) {
                for (int s = -hw + 2; s < hw; s += 4) line(wx + s, wy - hw, wx + s, wy + hw, CLR_WHITE);
            }
        }
}

// query (this is what a consumer cart — sloop rung 3 — would call once extracted)
static Place road_at(float x, float y) {
    float d = roads_density(x, y);
    int z = roads_zone_of(d), step = ZONE_STEP[z], hw = ZONE_LANE[z] / 2;
    int pitch = ROADS_BASE * step;
    int ix = (int)x - roads_ifloordiv((int)x, pitch) * pitch;
    int iy = (int)y - roads_ifloordiv((int)y, pitch) * pitch;
    int dx = ix < pitch - ix ? ix : pitch - ix;
    int dy = iy < pitch - iy ? iy : pitch - iy;
    return (Place){ z, (dx <= hw) || (dy <= hw), d };
}
static int roads_field_col(float x, float y) {       // overlay tint: colour by zone
    static const int zc[Z_N] = { CLR_RED, CLR_ORANGE, CLR_LIME_GREEN, CLR_BLUE, CLR_DARK_BLUE };
    return zc[roads_zone_of(roads_density(x, y))];
}
static const char *roads_probe(float x, float y) {
    static char buf[40];
    Place p = road_at(x, y);
    snprintf(buf, sizeof buf, "%s %s d%.2f", ZONE_NAME[p.zone], p.on_road ? "ROAD" : "lot ", p.density);
    return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  GENERATOR 2 — TERRAIN   (pure fbm elevation field, no structure)
// ════════════════════════════════════════════════════════════════════════════
// Structurally the opposite of roads — proves the plug-in seam. fbm elevation
// thresholds into bands, drawn as filled cells so zooming out shows whole coasts.
#define TERRAIN_CELL 8        // world px per filled cell (smaller = finer coastline)
#define TERRAIN_FREQ 0.0016f  // base elevation frequency (smaller = bigger continents)

enum { TB_DEEP, TB_SEA, TB_BEACH, TB_GRASS, TB_FOREST, TB_ROCK, TB_SNOW, TB_N };
static const float TERRA_MAX[TB_N]  = { 0.34f, 0.42f, 0.46f, 0.58f, 0.72f, 0.86f, 1.01f }; // upper elev of each band
static const int   TERRA_COL[TB_N]  = { CLR_DARK_BLUE, CLR_BLUE, CLR_LIGHT_PEACH,
                                         CLR_LIME_GREEN, CLR_DARK_GREEN, CLR_MEDIUM_GREY, CLR_WHITE };
static const char *TERRA_NAME[TB_N] = { "DEEP SEA", "SEA", "BEACH", "GRASS", "FOREST", "ROCK", "SNOW" };

static int terrain_seed = 0;
static void terrain_reseed(int seed) { terrain_seed = seed; }

typedef struct { float elev; int band; } Terrain;

static float terrain_elev(float x, float y) {
    float z = (float)terrain_seed;
    float f = TERRAIN_FREQ, amp = 1.0f, sum = 0.0f, norm = 0.0f;
    for (int o = 0; o < 4; o++) {                       // fbm: 4 octaves
        sum  += amp * noise3(x * f, y * f, z + (float)o * 5.0f);
        norm += amp;
        f    *= 2.0f;
        amp  *= 0.5f;
    }
    return clamp(sum / norm, 0.0f, 1.0f);
}
static int terrain_band_of(float e) {
    for (int b = 0; b < TB_N; b++) if (e <= TERRA_MAX[b]) return b;
    return TB_SNOW;
}
static Terrain terrain_at(float x, float y) {
    float e = terrain_elev(x, y);
    return (Terrain){ e, terrain_band_of(e) };
}
static void terrain_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int C = TERRAIN_CELL;
    int L = ((int)(cx - hwv) / C - 1) * C, R = (int)(cx + hwv) + C;
    int T = ((int)(cy - hhv) / C - 1) * C, B = (int)(cy + hhv) + C;
    for (int wy = T; wy <= B; wy += C)
        for (int wx = L; wx <= R; wx += C)
            rectfill(wx, wy, C, C, TERRA_COL[terrain_band_of(terrain_elev(wx + C / 2.0f, wy + C / 2.0f))]);
}
static int terrain_field_col(float x, float y) {      // overlay tint: the band IS the field
    return TERRA_COL[terrain_band_of(terrain_elev(x, y))];
}
static const char *terrain_probe(float x, float y) {
    static char buf[32];
    Terrain t = terrain_at(x, y);
    snprintf(buf, sizeof buf, "%s e%.2f", TERRA_NAME[t.band], t.elev);
    return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  THE SHELL — free-fly explorer + generator registry
// ════════════════════════════════════════════════════════════════════════════
typedef struct {
    const char *name;
    void  (*draw)(float cam_x, float cam_y, float zoom);
    void  (*reseed)(int seed);
    const char *(*probe)(float wx, float wy);   // 1-line "what's under here"
    int   (*field_col)(float wx, float wy);     // palette colour of the field (overlay)
} Generator;

static Generator gens[] = {
    { "ROADS & CITIES", roads_draw,   roads_reseed,   roads_probe,   roads_field_col   },
    { "TERRAIN",        terrain_draw, terrain_reseed, terrain_probe, terrain_field_col },
};
#define N_GENS ((int)(sizeof gens / sizeof gens[0]))

static float cam_x = -160, cam_y = -100;   // world coord of screen top-left at zoom 1
static float zoom  = 1.0f;
static int   seed  = 1;
static int   cur   = 0;
static bool  overlay = false;

static void reseed_all(int s) {
    for (int i = 0; i < N_GENS; i++) gens[i].reseed(s);
}

void init(void) {
    reseed_all(seed);
}

void update(void) {
    float pan = 5.0f / zoom;                       // constant on-screen pan speed
    if (key(KEY_RIGHT) || key('D')) cam_x += pan;
    if (key(KEY_LEFT)  || key('A')) cam_x -= pan;
    if (key(KEY_DOWN)  || key('S')) cam_y += pan;
    if (key(KEY_UP)    || key('W')) cam_y -= pan;

    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom /= 1.04f;
    zoom += mouse_wheel() * 0.08f * zoom;
    zoom = clamp(zoom, 0.18f, 6.0f);

    if (keyp('R')) { seed++; reseed_all(seed); }
    if (keyp(KEY_TAB)) overlay = !overlay;
    for (int i = 0; i < N_GENS; i++) if (keyp('1' + i)) cur = i;
}

// fill the visible-world rect with the active field's colours (coarse, dithered)
static void draw_overlay(void) {
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int OV = 16;
    int L = ((int)(cx - hwv) / OV - 1) * OV, R = (int)(cx + hwv) + OV;
    int T = ((int)(cy - hhv) / OV - 1) * OV, B = (int)(cy + hhv) + OV;
    fillp(0xA5A5, -1);                       // checker → semi-transparent tint
    for (int wy = T; wy <= B; wy += OV)
        for (int wx = L; wx <= R; wx += OV)
            rectfill(wx, wy, OV, OV, gens[cur].field_col(wx + OV / 2.0f, wy + OV / 2.0f));
    fillp_reset();
}

void draw(void) {
    cls(CLR_BLACK);

    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    gens[cur].draw(cam_x, cam_y, zoom);
    if (overlay) draw_overlay();

    // HUD in screen space
    camera(0, 0);
    int mx = SCREEN_W / 2, my = SCREEN_H / 2;
    line(mx - 5, my, mx + 5, my, CLR_WHITE);          // centre crosshair = probe point
    line(mx, my - 5, mx, my + 5, CLR_WHITE);

    rectfill(0, 0, SCREEN_W, 10, CLR_BLACK);
    char buf[64];
    int x = print(gens[cur].name, 3, 2, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  seed %d  zoom %.2f", seed, (double)zoom);
    print(buf, x, 2, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 18, SCREEN_W, 18, CLR_BLACK);
    print(gens[cur].probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f),
          3, SCREEN_H - 16, CLR_WHITE);
    print("WASD pan  Z/X zoom  R seed  1/2 gen  TAB overlay",
          3, SCREEN_H - 8, CLR_MEDIUM_GREY);
}
