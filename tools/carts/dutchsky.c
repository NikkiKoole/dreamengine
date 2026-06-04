#include "studio.h"
#include <math.h>

// dutchsky — day/night sky cycle; fly up through the atmosphere into space.
// ← → scrub time of day   ↑ ↓ altitude   A toggle auto-advance

#define NK  10
#define NS  90   // stars
#define GH  16   // ground strip height (px)

typedef struct { float t; int top, mid, bot; } SkyKey;

static const SkyKey SK[NK] = {
    { 0.00f, CLR_BROWNISH_BLACK, CLR_DARKER_BLUE,  CLR_DARKER_BLUE   },  // midnight
    { 0.17f, CLR_DARKER_BLUE,   CLR_DARKER_BLUE,   CLR_DARKER_PURPLE },  // pre-dawn
    { 0.24f, CLR_DARKER_BLUE,   CLR_TRUE_BLUE,     CLR_DARK_ORANGE   },  // dawn
    { 0.32f, CLR_TRUE_BLUE,     CLR_BLUE,          CLR_LIGHT_PEACH   },  // morning
    { 0.50f, CLR_TRUE_BLUE,     CLR_BLUE,          CLR_BLUE          },  // noon
    { 0.66f, CLR_TRUE_BLUE,     CLR_BLUE,          CLR_LIGHT_PEACH   },  // afternoon
    { 0.73f, CLR_DARKER_BLUE,   CLR_DARK_ORANGE,   CLR_ORANGE        },  // golden hour
    { 0.80f, CLR_DARKER_BLUE,   CLR_MAUVE,         CLR_DARK_PEACH    },  // dusk
    { 0.87f, CLR_DARKER_BLUE,   CLR_DARK_PURPLE,   CLR_DARKER_GREY   },  // twilight
    { 1.00f, CLR_BROWNISH_BLACK, CLR_DARKER_BLUE,  CLR_DARKER_BLUE   },  // midnight
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

// ── draw_atm: A's smooth gradient + B overlaid via fillp ─────────────────────
//
// vgradient() overrides fillp internally per-row, so we can't blend two
// vgradients with an external fillp. Fix: draw A with vgradient (smooth),
// then overlay B as 3 solid rectfills (top/mid/bot zones) with fillp(-1) as
// the hole colour — the transparent holes show A's gradient underneath.
// 5 dither steps cover 12%→87% B coverage; frac≥0.97 switches to full B.

static void draw_atm(int y0, int h, float t) {
    if (h <= 1) return;
    int ai, bi; float frac;
    sky_find(t, &ai, &bi, &frac);
    const SkyKey *a = &SK[ai], *b = &SK[bi];

    int mid_y = y0 + h * 62 / 100;
    if (mid_y <= y0)   mid_y = y0 + 1;
    if (mid_y >= y0+h) mid_y = y0 + h - 1;

    if (frac >= 0.97f) {
        vgradient(0, y0, SCREEN_W, mid_y - y0,   b->top, b->mid);
        vgradient(0, mid_y, SCREEN_W, y0+h-mid_y, b->mid, b->bot);
        return;
    }

    // A's smooth gradient — always the base
    vgradient(0, y0, SCREEN_W, mid_y - y0,   a->top, a->mid);
    vgradient(0, mid_y, SCREEN_W, y0+h-mid_y, a->mid, a->bot);

    if (frac < 0.03f) return;

    // B overlay: 3 solid colour zones with fillp holes showing A through.
    // 0-bits = draw col_b, 1-bits = -1 = transparent (A gradient shows).
    // Steps:  ~12%     25%     50%     75%     87%  B coverage
    // 0x7DBE / 0x8241 are dispersed complements — one B pixel per row per col,
    // no vertical or horizontal stripes. 0xA5A5 = canonical PICO-8 checker.
    static const int BPAT[5] = { 0xF7F7, 0x7DBE, 0xA5A5, 0x8241, 0x0808 };
    int pi = (int)(frac * 5.0f);
    if (pi > 4) pi = 4;

    int z1 = y0 + h * 31 / 100;   // top→mid zone boundary
    int z2 = y0 + h * 75 / 100;   // mid→bot zone boundary
    if (z1 <= y0)   z1 = y0 + 1;
    if (z2 <= z1)   z2 = z1 + 1;
    if (z2 >= y0+h) z2 = y0 + h - 1;

    fillp(BPAT[pi], -1);
    rectfill(0, y0, SCREEN_W, z1 - y0,    b->top);
    rectfill(0, z1, SCREEN_W, z2 - z1,    b->mid);
    rectfill(0, z2, SCREEN_W, y0+h - z2,  b->bot);
    fillp_reset();
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
