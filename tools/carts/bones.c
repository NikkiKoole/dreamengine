#include "studio.h"

// ============================================================
//  BONES — a pixel-man skeleton animator.
//
//  An 18-bone stick figure of 1px lines. You pose him by setting each bone's
//  ROTATION (1 of 16 directions, 22.5° apart) and author motion in a
//  SEQUENCER GRID: bones down the rows, 16 time frames across the columns.
//  Every cell holds one hex digit 0-f = that bone's rotation in that frame.
//  Hit play and a playhead sweeps the columns, animating the little guy in
//  the live preview on the right.
//
//  It's the drum machine's grid, but each cell is a rotation instead of
//  on/off — and the skeleton is forward-kinematic: a bone's stored angle is
//  RELATIVE to its parent, so turning the torso swings everything attached.
//
//    arrows  move the cell marker (↑↓ bone, ←→ frame)
//    Z / X   turn the selected bone CCW / CW (also , / .)
//    C       copy the previous frame into this one
//    SPACE   play / stop      [ ] fps      - = loop length
//    S / L   save / load (the whole animation persists via save_bytes)
//    mouse   click a cell · wheel turns it · DRAG a limb in the preview to
//            pose it directly (snaps to the nearest of 16)
// ============================================================

#define NB 18      // bones (grid rows)
#define NF 16      // frames (grid columns)
#define STEP 22.5f // degrees per rotation index

// ── the skeleton table ── parent (-1 = root), length px, rest local angle,
// region color, crossbar?, which end of the parent crossbar (0 none/1 L/2 R)
typedef struct { int parent, len; float rest; int region, cross, side; const char *tag; } Bone;
static const Bone BONE[NB] = {
    //  parent len  rest  reg cr sd  tag
    {  1, 10,   0.0f, 0, 0, 0, "hd"  },  // 0 head      (drawn as a circle on its tip)
    {  2,  9,   0.0f, 0, 0, 0, "nk"  },  // 1 neck
    { -1, 22, 270.0f, 0, 0, 0, "UT"  },  // 2 upper torso  (up from root)
    { -1, 20,  90.0f, 0, 0, 0, "LT"  },  // 3 lower torso  (down from root)
    {  3, 12, 270.0f, 0, 1, 0, "hip" },  // 4 hip crossbar (at lower-torso tip)
    {  2, 14,  90.0f, 0, 1, 0, "sho" },  // 5 shoulder crossbar (at upper-torso tip)
    {  5, 18,  90.0f, 1, 0, 1, "Lua" },  // 6 L upper arm  (off shoulder left end)
    {  6, 16,   0.0f, 1, 0, 0, "Lla" },  // 7 L lower arm
    {  7,  7,   0.0f, 1, 0, 0, "Lha" },  // 8 L hand
    {  5, 18,  90.0f, 2, 0, 2, "Rua" },  // 9 R upper arm  (off shoulder right end)
    {  9, 16,   0.0f, 2, 0, 0, "Rla" },  // 10 R lower arm
    { 10,  7,   0.0f, 2, 0, 0, "Rha" },  // 11 R hand
    {  4, 22,  90.0f, 3, 0, 1, "Lul" },  // 12 L upper leg (off hip left end)
    { 12, 20,   0.0f, 3, 0, 0, "Lll" },  // 13 L lower leg
    { 13,  9,  90.0f, 3, 0, 0, "Lft" },  // 14 L foot  (points outward, left)
    {  4, 22,  90.0f, 4, 0, 2, "Rul" },  // 15 R upper leg (off hip right end)
    { 15, 20,   0.0f, 4, 0, 0, "Rll" },  // 16 R lower leg
    { 16,  9, 270.0f, 4, 0, 0, "Rft" },  // 17 R foot  (points outward, right)
};
static const int REGCOL[5] = { CLR_LIGHT_GREY, CLR_BLUE, CLR_GREEN, CLR_ORANGE, CLR_PINK };
static const char *HEX = "0123456789abcdef";

// ── layout ──
#define GX 34      // grid left
#define GY 28      // grid top (first row)
#define CWp 11     // cell width
#define RS 11      // row stride
#define PVX 300    // preview root x
#define PVY 88     // preview root y

// ── animation state ──
static unsigned char rot[NB][NF];     // 0..15 per cell
static int   selBone, selFrame;       // the cell marker
static int   playFrame, fps = 8, loopLen = NF;
static bool  playing;
static float playAcc;
static float saveMsg; static const char *saveTxt = "";
static int   dragBone = -1;

// ── per-frame computed pose (filled by compute_pose, used to draw + hit-test) ──
static float wAng[NB], pWorld[NB];        // world angle, parent's world angle
static float stx[NB], sty[NB];            // bone start point
static float jx[NB], jy[NB];              // bone end ("tip") — drag handle
static float xL[NB], yL[NB], xR[NB], yR[NB]; // crossbar ends
static float dispLocal[NB];               // smoothed local angle (for easing)
static bool  posed;                       // snap on the very first pose

static int   mod16(int v) { return ((v % 16) + 16) % 16; }
static int   iround(float x) { return (int)(x + (x >= 0 ? 0.5f : -0.5f)); }
static float angwrap(float d) { while (d > 180) d -= 360; while (d < -180) d += 360; return d; }

// walk the tree (parents always precede children) and place every joint for
// `frame`. dispLocal eases toward the target so playback looks smooth even
// though the data is quantized to 16 steps.
static void compute_pose(int frame) {
    float k = clamp(dt() * 16.0f, 0.0f, 1.0f);
    for (int b = 0; b < NB; b++) {
        float target = BONE[b].rest + rot[b][frame] * STEP;
        dispLocal[b] = posed ? dispLocal[b] + angwrap(target - dispLocal[b]) * k : target;

        int p = BONE[b].parent;
        float px, py, pw;
        if (p < 0)                 { px = PVX;  py = PVY;  pw = 0; }
        else if (BONE[p].cross)    { pw = wAng[p];
                                     if (BONE[b].side == 1) { px = xL[p]; py = yL[p]; }
                                     else                   { px = xR[p]; py = yR[p]; } }
        else                       { px = jx[p]; py = jy[p]; pw = wAng[p]; }

        pWorld[b] = pw;
        stx[b] = px; sty[b] = py;
        float w = pw + dispLocal[b];
        wAng[b] = w;

        if (BONE[b].cross) {
            float h = BONE[b].len;
            xL[b] = px + dx(h, w + 180); yL[b] = py + dy(h, w + 180);
            xR[b] = px + dx(h, w);       yR[b] = py + dy(h, w);
            jx[b] = xR[b]; jy[b] = yR[b];
        } else {
            jx[b] = px + dx((float)BONE[b].len, w);
            jy[b] = py + dy((float)BONE[b].len, w);
        }
    }
    posed = true;
}

static void copy_prev(void) {
    int src = (selFrame - 1 + NF) % NF;
    for (int b = 0; b < NB; b++) rot[b][selFrame] = rot[b][src];
}

// ── save / load ──
typedef struct { int magic, fps, loopLen; unsigned char rot[NB][NF]; } Anim;
#define MAGIC 0x424F4E31  // "BON1"
static void save_anim(void) {
    Anim a = { MAGIC, fps, loopLen };
    for (int b = 0; b < NB; b++) for (int f = 0; f < NF; f++) a.rot[b][f] = rot[b][f];
    save_bytes(&a, sizeof a);
    saveMsg = 1.4f; saveTxt = "SAVED";
    note(72, INSTR_SINE, 4);
}
static bool load_anim(void) {
    Anim a;
    if (load_bytes(&a, sizeof a) != (int)sizeof a || a.magic != MAGIC) return false;
    fps = mid(1, a.fps, 30);
    loopLen = mid(2, a.loopLen, NF);
    for (int b = 0; b < NB; b++) for (int f = 0; f < NF; f++) rot[b][f] = a.rot[b][f] & 15;
    return true;
}

// a friendly default so the tool opens alive — a little 8-frame wave + idle
// bob — used only when nothing's been saved yet.
static void seed_default(void) {
    static const unsigned char arm []  = { 8, 9, 10, 9, 8, 9, 10, 9 };  // R upper arm: up, waving
    static const unsigned char hand[]  = { 0, 15, 14, 15, 0, 15, 14, 15 }; // R forearm flick
    loopLen = 8;
    for (int f = 0; f < NF; f++) {
        rot[9][f]  = arm [f % 8];               // R upper arm waving overhead
        rot[10][f] = hand[f % 8];               // R forearm flick
        rot[0][f]  = 1;                         // slight head tilt
        rot[12][f] = (f % 4 < 2) ? 1 : 15;      // legs bob a touch
        rot[15][f] = (f % 4 < 2) ? 15 : 1;
    }
}

void init(void) {
    selBone = 9; selFrame = 0;                  // start on the waving arm
    if (!load_anim()) seed_default();
}

void update(void) {
    int curFrame = playing ? playFrame : selFrame;

    // ── playback advances on its own clock (independent of bpm) ──
    if (playing) {
        playAcc += dt();
        float spf = 1.0f / fps;
        while (playAcc >= spf) { playAcc -= spf; playFrame = (playFrame + 1) % loopLen; }
    } else playAcc = 0;

    // ── keyboard: marker, rotate, transport, settings ──
    if (keyp(KEY_UP))    selBone  = (selBone - 1 + NB) % NB;
    if (keyp(KEY_DOWN))  selBone  = (selBone + 1) % NB;
    if (keyp(KEY_LEFT))  selFrame = (selFrame - 1 + NF) % NF;
    if (keyp(KEY_RIGHT)) selFrame = (selFrame + 1) % NF;

    if (keyp('X') || keyp('.')) rot[selBone][selFrame] = mod16(rot[selBone][selFrame] + 1);
    if (keyp('Z') || keyp(',')) rot[selBone][selFrame] = mod16(rot[selBone][selFrame] - 1);
    if (keyp('C')) copy_prev();

    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) { playFrame = selFrame; playAcc = 0; } }
    if (keyp('[')) fps = max(1, fps - 1);
    if (keyp(']')) fps = min(30, fps + 1);
    if (keyp('-')) loopLen = max(2, loopLen - 1);
    if (keyp('=')) loopLen = min(NF, loopLen + 1);
    if (keyp('S')) save_anim();
    if (keyp('L')) { if (load_anim()) { saveMsg = 1.4f; saveTxt = "LOADED"; } }

    // ── mouse on the grid: click selects a cell, wheel turns it ──
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT) && mx >= GX && mx < GX + NF * CWp && my >= GY && my < GY + NB * RS) {
        selFrame = (mx - GX) / CWp;
        selBone  = (my - GY) / RS;
    }
    float w = mouse_wheel();
    if (w != 0) rot[selBone][selFrame] = mod16(rot[selBone][selFrame] + (w > 0 ? 1 : -1));

    // ── drag-pose: grab a limb tip in the preview, snap to nearest of 16 ──
    if (mouse_pressed(MOUSE_LEFT) && mx >= 214 && posed) {
        float best = 14; dragBone = -1;
        for (int b = 0; b < NB; b++) {
            float d = distance((int)jx[b], (int)jy[b], mx, my);
            if (d < best) { best = d; dragBone = b; }
        }
        if (dragBone >= 0) selBone = dragBone;
    }
    if (!mouse_down(MOUSE_LEFT)) dragBone = -1;
    if (dragBone >= 0) {
        float want = angle_to((int)stx[dragBone], (int)sty[dragBone], mx, my);
        float local = want - pWorld[dragBone] - BONE[dragBone].rest;
        rot[dragBone][selFrame] = mod16(iround(local / STEP));
    }

    if (saveMsg > 0) saveMsg -= dt();
    (void)curFrame;
}

void draw(void) {
    int curFrame = playing ? playFrame : selFrame;
    compute_pose(curFrame);

    cls(CLR_BROWNISH_BLACK);

    // ── top bar ──
    rectfill(0, 0, SCREEN_W, 12, CLR_DARK_BLUE);
    print("BONES", 4, 2, CLR_LIGHT_PEACH);
    print_right(str("FRAME %d/%d   %d FPS   %s", curFrame + 1, loopLen, fps, playing ? "\x10PLAY" : "STOP"),
                SCREEN_W - 4, 2, playing ? CLR_GREEN : CLR_LIGHT_GREY);

    // ── frame-number header ──
    for (int f = 0; f < NF; f++) {
        bool on = (f == curFrame), down = (f % 4 == 0);
        print(str("%c", HEX[f]), GX + f * CWp + 2, GY - 9,
              on ? CLR_WHITE : down ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    }

    // ── the sequencer grid ──
    for (int b = 0; b < NB; b++) {
        print(BONE[b].tag, 2, GY + b * RS + 2, REGCOL[BONE[b].region]);
        for (int f = 0; f < NF; f++) {
            int x = GX + f * CWp, y = GY + b * RS;
            bool selCell = (b == selBone && f == selFrame);
            bool head = (f == curFrame), beatCol = (f % 4 == 0);
            int bg = selCell ? CLR_WHITE
                   : head    ? CLR_DARKER_BLUE
                   : beatCol ? CLR_DARKER_PURPLE
                             : CLR_BROWNISH_BLACK;
            rectfill(x, y, CWp - 1, RS - 1, bg);
            print(str("%c", HEX[rot[b][f]]), x + 2, y + 2,
                  selCell ? CLR_BLACK : REGCOL[BONE[b].region]);
        }
    }
    // marker outline + the bone/region key
    rect(GX + selFrame * CWp - 1, GY + selBone * RS - 1, CWp + 1, RS + 1, CLR_YELLOW);
    rect(GX - 2, GY - 1, NF * CWp + 1, NB * RS + 1, CLR_DARK_GREY);

    // ── divider + preview panel ──
    line(212, 14, 212, SCREEN_H - 2, CLR_DARK_BLUE);
    line(216, PVY + 66, SCREEN_W - 4, PVY + 66, CLR_DARKER_GREY);   // ground line

    // draw every bone; the selected one in white + doubled for weight
    for (int b = 0; b < NB; b++) {
        int col = (b == selBone) ? CLR_WHITE : REGCOL[BONE[b].region];
        if (BONE[b].cross) {
            line((int)xL[b], (int)yL[b], (int)xR[b], (int)yR[b], col);
        } else if (b == 0) {                                   // head: short neck-stub + skull
            line((int)stx[b], (int)sty[b], (int)jx[b], (int)jy[b], col);
            circfill((int)jx[b], (int)jy[b], 6, col);
            pset((int)jx[b] - 2, (int)jy[b] - 1, CLR_BLACK);   // a tiny eye
        } else {
            line((int)stx[b], (int)sty[b], (int)jx[b], (int)jy[b], col);
        }
        if (b == selBone && !BONE[b].cross && b != 0)
            line((int)stx[b] + 1, (int)sty[b], (int)jx[b] + 1, (int)jy[b], col);
        if (!BONE[b].cross) pset((int)stx[b], (int)sty[b], CLR_LIGHT_YELLOW);  // joint dot
    }
    print(str("> %s", BONE[selBone].tag), 216, 16, REGCOL[BONE[selBone].region]);

    // ── controls, down the right panel ──
    int hy = 184; int hc = CLR_DARK_GREY;
    print("arrows  move cell",  216, hy,       hc);
    print("Z / X   turn bone",  216, hy + 8,   hc);
    print("C  copy prev frame", 216, hy + 16,  hc);
    print("SPACE  play / stop", 216, hy + 24,  hc);
    print("[ ] fps   - = loop", 216, hy + 32,  hc);
    print("S / L  save / load", 216, hy + 40,  hc);
    print("wheel / drag = pose",216, hy + 48,  CLR_INDIGO);

    if (saveMsg > 0) print(saveTxt, 216, 26, CLR_GREEN);
}
