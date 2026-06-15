#include "studio.h"
#include "ui.h"
#include <stdio.h>

// ============================================================================
// INTERCHANGE — a geometry SANDBOX for roadnet2's part-B (how roads cross/connect a
// highway). Screen-space, hand-placed: a horizontal HIGHWAY and a crossing road, with
// FAKE-3D grade separation (the crossing road is drawn OVER the highway → reads as an
// overpass) and a library of connection types built from RAMP curves. Grow the shapes
// here, eyeball them, then port the drawer into roadnet2. The eventual interface is:
// "given two crossing roads (+ a type) → draw the ramps."  See docs/design/roadnet2-plan.md.
//
//   T            cycle type: OVERPASS → DIAMOND → CLOVERLEAF → TRUMPET
//   ◄ ►          rotate the crossing road
//   ▲ ▼          ramp size
//   H            hide HUD
// ============================================================================

#define HW_HW   13          // highway carriageway half-width (px)
#define HW_AR    8          // crossing arterial half-width
static float fsq(float x){ if(x<=0)return 0; float g=x>1?x:1; for(int i=0;i<6;i++) g=0.5f*(g+x/g); return g; }
static float c_deg(float d){ return dx(1.0f, d); }   // engine dx/dy = cos/sin in degrees (0=right,90=down)
static float s_deg(float d){ return dy(1.0f, d); }

enum { T_OVERPASS, T_DIAMOND, T_CLOVERLEAF, T_TRUMPET, T_COUNT };
static const char *TNAME[T_COUNT] = { "OVERPASS", "DIAMOND", "CLOVERLEAF", "TRUMPET" };
static int   itype = T_DIAMOND;
static float ang   = 90.0f;     // crossing road angle (deg from highway)
static int   rampsz = 46;       // ramp reach (px)
static int   show_hud = 1;

// ── ribbon: stroke a polyline as a clean filled road (centre-line + polyfill quads + joint
//    circles), the same technique roadnet2 uses. casing = a darker outline drawn first/wider.
static void ribbon(const float *xs, const float *ys, int n, int hw, int col) {
    for (int i = 0; i + 1 < n; i++) {
        int x0=(int)xs[i], y0=(int)ys[i], x1=(int)xs[i+1], y1=(int)ys[i+1];
        line(x0, y0, x1, y1, col);
        float dx=x1-x0, dy=y1-y0, L=fsq(dx*dx+dy*dy);
        if (hw >= 2 && L >= 1.0f) {
            int px=(int)(-dy/L*hw), py=(int)(dx/L*hw);
            int xy[8]={ x0+px,y0+py, x1+px,y1+py, x1-px,y1-py, x0-px,y0-py };
            polyfill(xy, 4, col);
            circfill(x1, y1, hw, col);
        }
    }
}
// stroke with a casing (wider darker band under a narrower lighter centre) — a "real road" look
static void road(const float *xs, const float *ys, int n, int hw, int casing, int centre) {
    ribbon(xs, ys, n, hw,   casing);
    ribbon(xs, ys, n, hw-3, centre);
}

// ── cubic Bézier ramp from A (heading dir aA) to B (heading dir aB), sampled into xs/ys.
//    Control handles along the two tangents give a smooth merge curve.
#define RN 14
static void ramp_pts(float ax, float ay, float aA, float bx, float by, float aB,
                     float k, float *xs, float *ys) {
    float c1x=ax+c_deg(aA)*k, c1y=ay+s_deg(aA)*k;
    float c2x=bx-c_deg(aB)*k, c2y=by-s_deg(aB)*k;
    for (int i=0;i<=RN;i++){
        float t=(float)i/RN, u=1-t;
        float w0=u*u*u, w1=3*u*u*t, w2=3*u*t*t, w3=t*t*t;
        xs[i]=w0*ax + w1*c1x + w2*c2x + w3*bx;
        ys[i]=w0*ay + w1*c1y + w2*c2y + w3*by;
    }
}
static void draw_ramp(float ax,float ay,float aA, float bx,float by,float aB, float k) {
    float xs[RN+1], ys[RN+1];
    ramp_pts(ax,ay,aA, bx,by,aB, k, xs,ys);
    road(xs, ys, RN+1, 6, CLR_DARKER_GREY, CLR_DARK_GREY);
}
// a STRAIGHT road A→B, subdivided into short segments (polyfill fills short quads reliably; one
// screen-long quad came out empty). Optional `casing<0` → no casing (shadow pass uses one colour).
static void straight(float ax,float ay,float bx,float by,int hw,int casing,int centre){
    float xs[25], ys[25];
    for (int i=0;i<=24;i++){ float t=(float)i/24; xs[i]=ax+(bx-ax)*t; ys[i]=ay+(by-ay)*t; }
    if (casing >= 0) road(xs, ys, 25, hw, casing, centre);
    else             ribbon(xs, ys, 25, hw, centre);
}

void init(void) {}

void update(void) {
    if (keyp('T') || keyp(KEY_TAB)) itype = (itype + 1) % T_COUNT;
    if (key(KEY_LEFT))  ang  -= 1.5f;
    if (key(KEY_RIGHT)) ang  += 1.5f;
    if (ang < 25) ang = 25; if (ang > 155) ang = 155;
    if (key(KEY_UP))   rampsz += 1;
    if (key(KEY_DOWN)) rampsz -= 1;
    if (rampsz < 24) rampsz = 24; if (rampsz > 90) rampsz = 90;
    if (keyp('H')) show_hud = !show_hud;
}

void draw(void) {
    cls(CLR_DARK_GREEN);
    int CX = SCREEN_W/2, CY = SCREEN_H/2;
    float ux = c_deg(ang), uy = s_deg(ang);            // crossing-road unit dir
    float R  = (float)rampsz;

    // 1. HIGHWAY (horizontal), drawn UNDER everything
    straight(0, CY, SCREEN_W, CY, HW_HW, CLR_DARKER_GREY, CLR_LIGHT_GREY);

    // 2. RAMPS at highway level (so the overpass bridge occludes them where they pass under)
    if (itype == T_DIAMOND || itype == T_CLOVERLEAF) {
        // four quadrant ramps: highway point (CX±R,CY) ↔ crossing-road point (CX±R·u)
        for (int sx=-1; sx<=1; sx+=2) for (int sy=-1; sy<=1; sy+=2) {
            float hx = CX + sx*R*1.5f, hy = CY;                   // on the highway
            float ax = CX + ux*sy*R,   ay = CY + uy*sy*R;         // on the crossing road
            draw_ramp(hx,hy, (sx>0?0:180), ax,ay, (sy>0?ang:ang+180), R*0.9f);
        }
    }
    if (itype == T_CLOVERLEAF) {
        // loop ramps: a 270° curl in each quadrant (tight inner turn)
        for (int sx=-1; sx<=1; sx+=2) for (int sy=-1; sy<=1; sy+=2) {
            float cx = CX + sx*R*0.6f, cy = CY + sy*R*0.6f;
            float lp[28], lq[28]; int n=0;
            for (int i=0;i<=26;i++){ float a = (float)i/26*300 + (sx*sy>0?40:220);
                lp[n]=cx + c_deg(a)*R*0.55f; lq[n]=cy + s_deg(a)*R*0.55f; n++; }
            road(lp, lq, n, 5, CLR_DARKER_GREY, CLR_DARK_GREY);
        }
    }
    if (itype == T_TRUMPET) {
        // 3-way: the crossing road only goes ONE side; a loop + a direct ramp
        float ax = CX + ux*R, ay = CY + uy*R;
        draw_ramp(CX+R*1.5f,CY, 0,  ax,ay, ang, R);
        draw_ramp(CX-R*1.5f,CY, 180, ax,ay, ang+180, R);
        float lp[28], lq[28]; int n=0; float lcx=CX-R*0.5f, lcy=CY+uy*R*0.7f;
        for (int i=0;i<=26;i++){ float a=(float)i/26*300+30; lp[n]=lcx+c_deg(a)*R*0.5f; lq[n]=lcy+s_deg(a)*R*0.5f; n++; }
        road(lp,lq,n,5,CLR_DARKER_GREY,CLR_DARK_GREY);
    }

    // 3. the OVERPASS: crossing road drawn OVER the highway (fake-3D). A dark "underbridge"
    //    shadow band first, then the road on top → it visibly passes over.
    float far = (float)(SCREEN_W + SCREEN_H);
    float a0x = (itype==T_TRUMPET) ? CX : CX - ux*far, a0y = (itype==T_TRUMPET) ? CY : CY - uy*far;
    float a1x = CX + ux*far, a1y = CY + uy*far;
    straight(a0x,a0y, a1x,a1y, HW_AR+3, -1, CLR_BROWNISH_BLACK);   // underbridge shadow
    straight(a0x,a0y, a1x,a1y, HW_AR,   CLR_DARKER_GREY, CLR_MEDIUM_GREY);  // deck (over the highway)

    if (show_hud) {
        char buf[48];
        rectfill(0,0,SCREEN_W,11,CLR_BLACK);
        snprintf(buf,sizeof buf,"INTERCHANGE  %s", TNAME[itype]);
        print(buf, 4, 2, CLR_LIGHT_GREY);
        print_centered("T type   \x1a\x1b angle   \x18\x19 ramp size", SCREEN_W/2, SCREEN_H-9, CLR_DARK_GREY);
    }
}
