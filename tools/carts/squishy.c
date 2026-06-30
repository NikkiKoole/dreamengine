/* de:meta
{
  "title": "Squishy Lines",
  "status": "active",
  "created": "2026-06-30",
  "kind": [
    "tool"
  ],
  "teaches": [
    "velocity-brush"
  ],
  "description": {
    "summary": "Draw with a velocity-sensitive ink brush — lines swell when you go slow, thin out when you go fast, and taper to a point at each end. Pick a tool, thickness, and bevel in the top panel.",
    "detail": "Every stroke is stored as DATA (a path of points + the speed you drew each one at), not painted straight to pixels. A brush renders that path as a chain of overlapping round stamps whose width = a slow→fat / fast→thin speed curve × an end-taper × a little seeded wobble; each tool (ink / pencil / fineliner / marker) is just a different set of those numbers. The bevel toggle embosses each stroke into a faux-3D rim (light from the top-left). Storing strokes as data is what makes the planned 'boil' mode (re-render with fresh jitter → a living loop) almost free later. 2-tone ink-on-paper; boil + a richer palette come next.",
    "controls": "Top panel: pick a tool (ink/pen/fin/mrk), drag the thickness slider, toggle BVL (bevel), tap UNDO. Then drag on the canvas to draw. Keys still work: B bevel, U undo, C clear."
  },
  "todo": [
    "Swap the text tool-buttons for code-drawn glyph sprites (sprite-draw.js) per docs/design/squishy-lines.md.",
    "Add the opt-in boil mode (N cached frames, re-render with per-frame seed, cycle ~6-8fps).",
    "Cache finished strokes to a layer buffer instead of re-rendering every stroke every frame.",
    "spec(): same-seed determinism + jitter-bounds."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// ── SQUISHY LINES ─────────────────────────────────────────────────────────
// A velocity-sensitive drawing tool. The soul is one idea: a stroke is DATA (a
// path of points, each carrying the speed you drew it at), and rendering is a
// PURE FUNCTION of (that data + a seed). Draw slow → the line swells; flick fast
// → it thins; lift off → it tapers. Seeded per-stamp wobble keeps it hand-inked.
//
// Each TOOL is just a different set of brush numbers (width range, speed
// sensitivity, wobble, taper). The BEVEL toggle embosses strokes into faux-3D.
// Because strokes are data (not baked pixels), the planned "boil" mode is almost
// free. See docs/design/squishy-lines.md for the full plan.

// 2-tone: warm cream paper, soft inky brown-black (the tinyjam 1-bit target).
#define PAPER  CLR_WHITE           // #fff1e8
#define INK    CLR_BROWNISH_BLACK  // #291814
#define ACCENT CLR_RED             // selection / on-state marker in the panel

// bevel tones — light from the top-LEFT (DPaint convention): light rim on the
// upper-left edge, dark rim on the lower-right.
#define HILITE    CLR_LIGHT_PEACH  // the lit (top-left) rim
#define SHADOW    CLR_BLACK        // the shaded (bottom-right) rim, darker than ink
#define BEVEL_OFF 1.0f             // px the rims peek out past the ink body

#define PANEL_H   24               // top tool-bar height; the canvas is below it

#define MAX_STROKES 128
#define MAX_SAMPLES 1500           // a long continuous stroke (spiral fill, contour) fits

#define MIN_SPACING   1.6f         // px between captured path points
#define STAMP_SPACING 0.7f         // px between stamps when rendering a segment (dense = solid)

// a tool = a brush recipe. width swings between minw (full speed) and maxw
// (standstill); speedref = px/frame where width bottoms out; noise = per-stamp
// width wobble; taper = fraction of the stroke tapered at each end.
typedef struct { float minw, maxw, speedref, noise, taper; const char *name; } Brush;
static const Brush BRUSHES[] = {
    { 1.8f, 9.0f,  9.0f, 0.15f, 0.14f, "ink" },   // the squish — big swing, hand-inked
    { 1.0f, 2.6f,  6.0f, 0.35f, 0.10f, "pen" },   // pencil: thin, scratchy, grainy
    { 1.2f, 1.9f, 12.0f, 0.05f, 0.08f, "fin" },   // fineliner: thin, crisp, near-constant
    { 5.0f, 7.5f, 22.0f, 0.05f, 0.05f, "mrk" },   // marker: wide, even, flat
};
#define NTOOLS ((int)(sizeof(BRUSHES) / sizeof(BRUSHES[0])))

typedef struct { float x, y, speed; } Sample;
typedef struct {
    unsigned seed;          // per-stroke seed → deterministic wobble (boil reseeds this)
    int      tool;          // which brush this stroke was drawn with
    float    thick;         // thickness multiplier this stroke was drawn with
    int      n;
    Sample   pts[MAX_SAMPLES];
} Stroke;

static Stroke   strokes[MAX_STROKES];
static int      nstrokes = 0;
static Stroke   cur;                 // the stroke being drawn right now
static int      drawing = 0;
static int      bevel = 0;           // bevel mode: emboss each stroke into a faux-3D rim
static int      tool = 0;            // selected brush
static float    thick01 = 0.375f;    // thickness slider (0..1) → ~1.0× by default
static float    prevx, prevy;        // last frame's pointer (for per-frame speed)
static float    lastsx, lastsy;      // last *captured sample* (for min-spacing)
static float    ema = 0;             // smoothed pointer speed
static unsigned seedctr = 0x1234567u;

static float thickness(void) { return 0.4f + thick01 * 1.6f; }   // 0.4×..2.0×

// integer hash → [0,1); deterministic, no global rnd() state (boil-friendly).
static unsigned hashu(unsigned x){ x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static float    hashf(unsigned x){ return (hashu(x) & 0xFFFFFF) / (float)0x1000000; }

// width of the brush at sample i — the pure function: speed curve × taper × wobble.
static float sample_width(const Stroke *s, int i, unsigned fseed) {
    Brush b = BRUSHES[s->tool];
    float maxw = b.maxw * s->thick, minw = b.minw * s->thick;
    int n = s->n;
    float t = (n > 1) ? (float)i / (n - 1) : 0.5f;           // 0..1 along the stroke
    float taper = fminf(t / b.taper, (1.0f - t) / b.taper);
    if (taper > 1) taper = 1; if (taper < 0) taper = 0;
    taper = taper * taper * (3 - 2 * taper);                  // smoothstep the ends
    float sp = s->pts[i].speed / b.speedref;
    if (sp > 1) sp = 1; if (sp < 0) sp = 0;
    float wspeed = maxw - (maxw - minw) * sp;                 // slow = fat, fast = thin
    float wobble = 1.0f + (hashf(s->seed ^ fseed ^ (unsigned)(i * 2654435761u)) * 2 - 1) * b.noise;
    return wspeed * taper * wobble;
}

static void stamp(float x, float y, float w, int color) {
    if (w <= 0) return;
    int ix = (int)(x + 0.5f), iy = (int)(y + 0.5f);
    float r = w * 0.5f;
    if (r < 0.75f) pset(ix, iy, color);
    else circfill(ix, iy, (int)(r + 0.5f), color);
}

// render a stroke as a chain of overlapping round stamps, offset by (ox,oy) in
// `color` — the offset is what the bevel uses to peek a rim out from under the ink.
static void render_stroke(const Stroke *s, unsigned fseed, float ox, float oy, int color) {
    if (s->n == 0) return;
    if (s->n == 1) { stamp(s->pts[0].x + ox, s->pts[0].y + oy, sample_width(s, 0, fseed), color); return; }
    for (int i = 0; i < s->n - 1; i++) {
        float x0 = s->pts[i].x,   y0 = s->pts[i].y;
        float x1 = s->pts[i+1].x, y1 = s->pts[i+1].y;
        float w0 = sample_width(s, i, fseed), w1 = sample_width(s, i + 1, fseed);
        float dx = x1 - x0, dy = y1 - y0;
        float seg = sqrtf(dx * dx + dy * dy);
        int steps = (int)(seg / STAMP_SPACING);
        if (steps < 1) steps = 1;
        for (int k = 0; k <= steps; k++) {
            float u = (float)k / steps;
            stamp(x0 + dx * u + ox, y0 + dy * u + oy, w0 + (w1 - w0) * u, color);
        }
    }
}

// draw one stroke, with the bevel emboss if it's on: a SHADOW + HILITE copy
// offset under the ink body leave a light rim on the upper-left and a dark rim
// on the lower-right. Pure geometry, no canvas read-back → deterministic.
static void draw_one(const Stroke *s, unsigned fseed) {
    if (bevel) {
        render_stroke(s, fseed,  BEVEL_OFF,  BEVEL_OFF, SHADOW);
        render_stroke(s, fseed, -BEVEL_OFF, -BEVEL_OFF, HILITE);
    }
    render_stroke(s, fseed, 0, 0, INK);
}

static void do_undo(void) { if (nstrokes > 0) nstrokes--; }

// the top tool-bar: pick a tool, set thickness, toggle bevel, undo.
static void draw_panel(void) {
    rectfill(0, 0, SCREEN_W, PANEL_H, PAPER);
    line(0, PANEL_H - 1, SCREEN_W, PANEL_H - 1, INK);
    ui_begin();
    // tool buttons + a selection underline
    for (int i = 0; i < NTOOLS; i++) {
        int bx = 2 + i * 32;
        if (ui_button(bx, 3, 30, 16, BRUSHES[i].name)) tool = i;
        if (i == tool) rectfill(bx, 19, 30, 2, ACCENT);
    }
    ui_slider(&thick01, 134, 4, 56, "thk");        // thickness 0.4×..2.0×
    if (ui_button(196, 3, 30, 16, "bvl")) bevel = !bevel;
    if (bevel) rectfill(196, 19, 30, 2, ACCENT);
    if (ui_button(230, 3, 40, 16, "undo")) do_undo();
    ui_end();
}

void init(void) {
    mouse_hide();   // we draw our own brush-size ring as the cursor
}

void update(void) {
    float mx = mouse_x(), my = mouse_y();

    // per-frame pointer speed, EMA-smoothed so one jittery frame can't spike width
    float fdx = mx - prevx, fdy = my - prevy;
    float fspeed = sqrtf(fdx * fdx + fdy * fdy);
    ema += (fspeed - ema) * 0.4f;
    prevx = mx; prevy = my;

    // a press that lands in the panel drives the panel, never the canvas
    if (mouse_pressed(MOUSE_LEFT) && my >= PANEL_H) {
        drawing = 1;
        cur.n = 0;
        cur.tool = tool;
        cur.thick = thickness();
        seedctr = seedctr * 1103515245u + 12345u;
        cur.seed = seedctr;
        lastsx = mx; lastsy = my;
        cur.pts[cur.n++] = (Sample){ mx, my, ema };
    }
    if (drawing && mouse_down(MOUSE_LEFT)) {
        float dsx = mx - lastsx, dsy = my - lastsy;
        if (sqrtf(dsx * dsx + dsy * dsy) >= MIN_SPACING && cur.n < MAX_SAMPLES) {
            cur.pts[cur.n++] = (Sample){ mx, my, ema };
            lastsx = mx; lastsy = my;
        }
    }
    if (drawing && mouse_released(MOUSE_LEFT)) {
        drawing = 0;
        if (cur.n > 0 && nstrokes < MAX_STROKES) strokes[nstrokes++] = cur;
        cur.n = 0;
    }

    // NB: raylib letter keycodes are UPPERCASE — keyp('u') (lowercase, 117) never fires.
    if (keyp('U') || mouse_pressed(MOUSE_RIGHT)) do_undo();
    if (keyp('C')) nstrokes = 0;
    if (keyp('B')) bevel = !bevel;
}

void draw(void) {
    cls(PAPER);
    for (int i = 0; i < nstrokes; i++) draw_one(&strokes[i], strokes[i].seed);
    if (drawing) draw_one(&cur, cur.seed);

    // brush-size ring = the cursor; previews the live width (slow = big, fast = small)
    if (mouse_y() >= PANEL_H) {
        Brush b = BRUSHES[tool];
        float sp = ema / b.speedref; if (sp > 1) sp = 1;
        int r = (int)((b.maxw - (b.maxw - b.minw) * sp) * thickness() * 0.5f + 0.5f);
        if (r < 1) r = 1;
        circ(mouse_x(), mouse_y(), r, INK);
    }

    draw_panel();   // on top of the strokes
}
