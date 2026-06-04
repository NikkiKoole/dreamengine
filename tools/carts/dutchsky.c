#include "studio.h"
#include <math.h>

// dutchsky — day/night sky cycle; fly up through the atmosphere into space.
// ← → scrub time of day   ↑ ↓ altitude   A toggle auto-advance

#define NK  17
#define NS  90   // stars
#define GH  16   // ground strip height (px)

// p1..p4 are blend-zone boundaries (0=sky top, 1=sky bottom):
//   solid_top: 0→p1 | blend top→mid: p1→p2 | solid_mid: p2→p3
//   blend mid→bot: p3→p4 | solid_bot: p4→1
// Colours snap at the keyframe midpoint; p1..p4 lerp continuously —
// the dpaInt effect: blend zones grow/shrink rather than the whole sky snapping.
typedef struct {
    float t;
    int   top, mid, bot;
    float p1, p2, p3, p4;
} SkyKey;

static const SkyKey SK[NK] = {
  // time   top                  mid                 bot                   p1     p2     p3     p4
  { 0.00f, CLR_BROWNISH_BLACK, CLR_DARKER_BLUE,  CLR_DARKER_BLUE,   0.35f, 0.48f, 0.72f, 0.80f },// midnight
  { 0.09f, CLR_DARKER_BLUE,   CLR_DARKER_BLUE,  CLR_DARK_BLUE,     0.35f, 0.48f, 0.72f, 0.80f },// late night
  { 0.17f, CLR_DARKER_BLUE,   CLR_DARK_BLUE,    CLR_DARKER_PURPLE, 0.33f, 0.46f, 0.68f, 0.78f },// pre-dawn
  { 0.20f, CLR_DARKER_BLUE,   CLR_TRUE_BLUE,    CLR_DARK_PEACH,    0.26f, 0.42f, 0.52f, 0.76f },// dawn glow
  { 0.24f, CLR_DARKER_BLUE,   CLR_TRUE_BLUE,    CLR_DARK_ORANGE,   0.18f, 0.38f, 0.45f, 0.78f },// dawn
  { 0.28f, CLR_TRUE_BLUE,     CLR_BLUE,         CLR_ORANGE,        0.22f, 0.42f, 0.52f, 0.76f },// sunrise
  { 0.33f, CLR_TRUE_BLUE,     CLR_BLUE,         CLR_LIGHT_PEACH,   0.28f, 0.46f, 0.60f, 0.76f },// morning
  { 0.50f, CLR_TRUE_BLUE,     CLR_BLUE,         CLR_LIGHT_PEACH,   0.38f, 0.50f, 0.72f, 0.82f },// noon
  { 0.64f, CLR_TRUE_BLUE,     CLR_BLUE,         CLR_LIGHT_PEACH,   0.38f, 0.50f, 0.72f, 0.82f },// afternoon
  { 0.68f, CLR_TRUE_BLUE,     CLR_BLUE,         CLR_DARK_PEACH,    0.32f, 0.47f, 0.62f, 0.78f },// late afternoon
  { 0.71f, CLR_DARK_BLUE,     CLR_TRUE_BLUE,    CLR_DARK_ORANGE,   0.18f, 0.40f, 0.47f, 0.78f },// pre-golden
  { 0.75f, CLR_DARKER_BLUE,   CLR_DARK_ORANGE,  CLR_ORANGE,        0.06f, 0.38f, 0.44f, 0.82f },// golden hour
  { 0.78f, CLR_DARKER_BLUE,   CLR_MAUVE,        CLR_DARK_PEACH,    0.10f, 0.40f, 0.48f, 0.82f },// dusk
  { 0.81f, CLR_DARKER_BLUE,   CLR_DARK_PURPLE,  CLR_MAUVE,         0.18f, 0.44f, 0.56f, 0.80f },// deep dusk
  { 0.85f, CLR_DARKER_BLUE,   CLR_DARK_PURPLE,  CLR_DARKER_GREY,   0.26f, 0.47f, 0.64f, 0.78f },// twilight
  { 0.91f, CLR_BROWNISH_BLACK,CLR_DARKER_BLUE,  CLR_DARKER_PURPLE, 0.32f, 0.48f, 0.70f, 0.80f },// late twilight
  { 1.00f, CLR_BROWNISH_BLACK,CLR_DARKER_BLUE,  CLR_DARKER_BLUE,   0.35f, 0.48f, 0.72f, 0.80f },// midnight
};

static float tod   = 0.78f;   // time of day 0..1 (0 = midnight, 0.5 = noon)
static float alt   = 0.0f;    // altitude 0..1 (0 = ground, 1 = space)
static float planex = -22.0f;
static int   sx[NS], sy[NS];
static bool  sinit = false;
static bool  autot = true;
static int   fc    = 0;

// ── helpers ───────────────────────────────────────────────────────────────────

static float sky_dark(float t) {
    float d = fabsf(t - 0.5f) * 2.0f;
    return d > 0.55f ? (d - 0.55f) / 0.45f : 0.0f;
}

static void sky_find(float t, int *ai, int *bi, float *frac) {
    *ai = 0;
    for (int i = 0; i < NK - 2; i++) if (t >= SK[i+1].t) *ai = i + 1;
    *bi = *ai + 1;
    float span = SK[*bi].t - SK[*ai].t;
    *frac = span > 1e-5f ? (t - SK[*ai].t) / span : 0.0f;
    if (*frac < 0.0f) *frac = 0.0f;
    if (*frac > 1.0f) *frac = 1.0f;
}

// ── draw_atm ──────────────────────────────────────────────────────────────────
// Colours snap at the keyframe midpoint; blend-zone boundaries (p1..p4) lerp
// continuously. The zones grow/shrink like a dpaint gradient being dragged —
// the orange horizon at dawn literally expands upward into the blue sky.

static void draw_atm(int y0, int h, float t) {
    if (h <= 1) return;
    int ai, bi; float frac;
    sky_find(t, &ai, &bi, &frac);

    const SkyKey *k = frac < 0.5f ? &SK[ai] : &SK[bi];
    float p1 = lerp(SK[ai].p1, SK[bi].p1, frac);
    float p2 = lerp(SK[ai].p2, SK[bi].p2, frac);
    float p3 = lerp(SK[ai].p3, SK[bi].p3, frac);
    float p4 = lerp(SK[ai].p4, SK[bi].p4, frac);
    if (p2 < p1) p2 = p1;
    if (p3 < p2) p3 = p2;
    if (p4 < p3) p4 = p3;

    int y1 = y0 + (int)(p1 * h);
    int y2 = y0 + (int)(p2 * h);
    int y3 = y0 + (int)(p3 * h);
    int y4 = y0 + (int)(p4 * h);

    if (y1 > y0)    rectfill(0, y0, SCREEN_W, y1-y0,   k->top);
    if (y2 > y1)    vgradient(0, y1, SCREEN_W, y2-y1,  k->top, k->mid);
    if (y3 > y2)    rectfill(0, y2, SCREEN_W, y3-y2,   k->mid);
    if (y4 > y3)    vgradient(0, y3, SCREEN_W, y4-y3,  k->mid, k->bot);
    if (y0+h > y4)  rectfill(0, y4, SCREEN_W, y0+h-y4, k->bot);
}

// ── stars ─────────────────────────────────────────────────────────────────────

static void draw_stars(int y0, int h, float bright) {
    if (bright < 0.06f || h < 2) return;
    int n = (int)(bright * NS);
    if (n > NS) n = NS;
    for (int i = 0; i < n; i++) {
        int star_y = y0 + sy[i] * h / (SCREEN_H - GH);
        if (star_y < y0 || star_y >= y0+h) continue;
        if ((fc + i * 17) % 11 == 0) continue;   // twinkle: skip ~1/11 per frame
        int col = (i % 7 == 0) ? CLR_LIGHT_YELLOW
                : (i % 3 == 0) ? CLR_WHITE
                :                CLR_LIGHT_GREY;
        pset(sx[i], star_y, col);
    }
}

// ── moon ─────────────────────────────────────────────────────────────────────

static void draw_moon(float t, int atm_y, int atm_h) {
    if (sky_dark(t) < 0.25f || atm_h < 25) return;
    int mx = SCREEN_W - 40, my = atm_y + 18;
    circfill(mx, my, 7, CLR_LIGHT_YELLOW);
    // crescent: cover disc offset so a slim crescent remains on the right
    int ai, bi; float frac;
    sky_find(t, &ai, &bi, &frac);
    circfill(mx + 5, my - 2, 6, frac < 0.5f ? SK[ai].top : SK[bi].top);
}

// ── ground / horizon silhouette ───────────────────────────────────────────────

static void draw_ground(float alt_f) {
    if (alt_f >= 0.65f) return;
    int gy = SCREEN_H - GH;

    // flat city silhouette
    static const int BX[] = {  0, 18, 35, 52, 68, 80, 96,110,130,145,162,178,196,215,232,248,268,285,302 };
    static const int BW[] = { 18, 17, 17, 16, 12, 16, 14, 20, 15, 17, 16, 18, 19, 17, 16, 20, 17, 17, 18 };
    static const int BH[] = {  7,  5,  9,  6, 20,  8,  7,  6, 10,  7,  5,  8,  7,  6,  9,  7, 11,  5,  8 };
    for (int i = 0; i < 19; i++)
        rectfill(BX[i], gy - BH[i], BW[i], BH[i], CLR_BLACK);

    rectfill(0, gy,   SCREEN_W, 4,    CLR_DARK_GREEN);
    rectfill(0, gy+4, SCREEN_W, GH-4, CLR_BROWNISH_BLACK);
}

// ── plane ─────────────────────────────────────────────────────────────────────

static void draw_plane(int x, int y) {
    rectfill(x+4,  y+2, 13, 3, CLR_LIGHT_GREY);    // fuselage
    rectfill(x+17, y+2,  2, 2, CLR_WHITE);          // nose
    pset(x+19, y+2, CLR_MEDIUM_GREY);
    pset(x+19, y+3, CLR_MEDIUM_GREY);
    rectfill(x+8,  y,    6, 2, CLR_LIGHT_GREY);    // upper wing
    rectfill(x+8,  y+5,  6, 2, CLR_LIGHT_GREY);    // lower wing
    rectfill(x+3,  y,    3, 2, CLR_MEDIUM_GREY);   // upper tail fin
    rectfill(x+3,  y+5,  3, 2, CLR_MEDIUM_GREY);   // lower tail fin
    rectfill(x,    y+2,  4, 3, CLR_DARK_GREY);     // intake
    rectfill(x+12, y+1,  4, 2, CLR_DARKER_BLUE);   // cockpit
}

// ── HUD ──────────────────────────────────────────────────────────────────────

static void draw_hud(float t, float alt_f) {
    // time strip along the bottom
    int by = SCREEN_H - 3;
    line(6, by, SCREEN_W-6, by, CLR_BROWNISH_BLACK);
    int tx = 6 + (int)(t * (SCREEN_W - 12));
    circfill(tx, by, 2, sky_dark(t) > 0.4f ? CLR_LIGHT_GREY : CLR_YELLOW);

    // altitude bar on right edge
    if (alt_f > 0.02f) {
        int bx = SCREEN_W - 5;
        int bar_top = 4, bar_bot = SCREEN_H - GH - 4;
        line(bx, bar_top, bx, bar_bot, CLR_BROWNISH_BLACK);
        int ay = bar_bot - (int)(alt_f * (bar_bot - bar_top));
        circfill(bx, ay, 2, CLR_LIGHT_GREY);
    }
}

// ── main ─────────────────────────────────────────────────────────────────────

void draw(void) {
    if (!sinit) {
        for (int i = 0; i < NS; i++) {
            sx[i] = rnd_between(2, SCREEN_W - 2);
            sy[i] = rnd_between(2, SCREEN_H - GH - 4);
        }
        sinit = true;
    }
    fc++;

    // input
    if (btn(0, BTN_LEFT))  { tod -= 0.0025f; autot = false; }
    if (btn(0, BTN_RIGHT)) { tod += 0.0025f; autot = false; }
    if (btn(0, BTN_A))     autot = !autot;
    if (btn(0, BTN_UP))    alt = alt + 0.012f < 1.0f ? alt + 0.012f : 1.0f;
    if (btn(0, BTN_DOWN))  alt = alt - 0.009f > 0.0f ? alt - 0.009f : 0.0f;

    if (tod < 0.0f)  tod += 1.0f;
    if (tod >= 1.0f) tod -= 1.0f;
    if (autot) { tod += 0.00015f; if (tod >= 1.0f) tod -= 1.0f; }

    // plane drifts left-to-right
    planex += 0.55f;
    if (planex > SCREEN_W + 22) planex = -22.0f;

    // layout: space (top) + atmosphere + ground (bottom)
    int sky_h   = SCREEN_H - GH;
    int space_h = (int)(alt * (float)sky_h * 0.78f);
    int atm_y   = space_h;
    int atm_h   = sky_h - space_h;
    int plane_y = (int)(lerp((float)(SCREEN_H - GH - 40), 8.0f, alt));

    cls(CLR_BROWNISH_BLACK);   // base fill = space black
    draw_atm(atm_y, atm_h, tod);

    float dark       = sky_dark(tod);
    float atm_bright = dark + alt * 1.8f;
    if (atm_bright > 1.0f) atm_bright = 1.0f;

    if (space_h > 4)           draw_stars(0, space_h, 1.0f);
    if (atm_bright > 0.1f)     draw_stars(atm_y, atm_h / 2, atm_bright);
    draw_moon(tod, atm_y, atm_h);
    draw_ground(alt);
    draw_plane((int)planex, plane_y);
    draw_hud(tod, alt);
}
