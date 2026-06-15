#include "studio.h"
#include <stdio.h>
#include <math.h>

// FLANK — top-down tactical AI: a squad that isn't dumb. (A testbed for a GTA1 /
// Hotline-Miami enemy brain.) Four ideas from the roguelike toolkit, applied to
// a real-time shooter:
//   * COMMUNICATION — a shared blackboard. The instant ANY enemy sees you it
//     calls your position to the whole squad; blind enemies converge on it.
//     Knowledge decays, so break contact and they go to your last-known spot.
//   * FLOW FIELD — a Dijkstra map flooded from your last-known cell; enemies
//     roll downhill to approach AROUND cover, not into walls.
//   * HEATMAP / FLANKING — your aim projects a DANGER CONE; enemies score
//     firing spots and AVOID the cone, so they peel off to hit you from the
//     sides and behind. Toggle H to see the heat.
//   * COVER + SPREAD — they favour cells beside walls and away from each other,
//     so they peek from cover and don't bunch into one kill-funnel.
//
// WASD move · mouse aim · click/SPACE shoot · H heat · L comms · TAB spectate · R reset

#define GW 40
#define GH 23
#define TILE 8
#define HUD_Y (GH * TILE)          // 184
#define NE 6
#define NB 96
#define INF 1e9f
#define DIAG 1.41421356f

enum { E_PATROL, E_HUNT, E_ENGAGE, E_SEARCH, E_DOWN };
static const int ECOL[5] = { CLR_DARK_GREY, CLR_ORANGE, CLR_RED, CLR_YELLOW, CLR_BLACK };

static unsigned char wall[GH][GW];
static float flow[GH][GW];          // Dijkstra from last-known player cell
static float heat[GH][GW];          // danger projected by the player's aim cone

typedef struct { float x, y, vx, vy, aim; int hp, mag, reload, shake, spectate; } Player;
typedef struct { float x, y, aim; int hp, alive, state, shootcd, mag, reload, callcd; } Enemy;
typedef struct { float x, y, vx, vy; int alive, foe; } Bullet;
static Player pl;
static Enemy en[NE];
static Bullet bul[NB];

// shared blackboard (the squad's collective knowledge of you)
static float kx, ky; static int known, kage, flow_cx = -1, flow_cy = -1;
static int show_heat = 1, show_comms = 1, kills, msg_t, tick; static char msg[40];

// ---- voices ----------------------------------------------------------------
typedef struct { int h, ttl; } Voice; static Voice voices[16];
static void play_pan(int midi, int instr, int vol, float pan, int ttl) {
    for (int i = 0; i < 16; i++) if (voices[i].ttl <= 0) { voices[i].h = note_on(midi,instr,vol); note_pan(voices[i].h,pan); voices[i].ttl = ttl; return; }
}
static void voices_tick(void) { for (int i = 0; i < 16; i++) if (voices[i].ttl > 0 && --voices[i].ttl == 0) note_off(voices[i].h); }
static float panx(float x) { return x / SCREEN_W * 2 - 1; }

// ---- geometry --------------------------------------------------------------
static bool wallpx(float x, float y) { int cx = (int)(x/TILE), cy = (int)(y/TILE); return cx<0||cx>=GW||cy<0||cy>=GH||wall[cy][cx]; }
static bool wcell(int x, int y) { return x<0||x>=GW||y<0||y>=GH||wall[y][x]; }
static bool los_px(float x0, float y0, float x1, float y1) {       // sample the segment for walls
    float dx = x1-x0, dy = y1-y0, d = fsqrt(dx*dx+dy*dy); int n = (int)(d/4)+1;
    for (int i = 1; i < n; i++) { float t = (float)i/n; if (wallpx(x0+dx*t, y0+dy*t)) return false; }
    return true;
}
static float angnorm(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

// ---- Dijkstra flow field to last-known position ----------------------------
static void relax(float f[GH][GW]) {
    bool ch = true; int g = 0;
    while (ch && g++ < GW*GH) { ch = false;
        for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
            if (wall[y][x]) continue; float b = f[y][x];
            for (int dy=-1; dy<=1; dy++) for (int dx=-1; dx<=1; dx++) {
                if (!dx&&!dy) continue; if (wcell(x+dx,y+dy)) continue;
                if (dx&&dy&&(wcell(x+dx,y)||wcell(x,y+dy))) continue;
                float v = f[y+dy][x+dx] + (dx&&dy?DIAG:1.0f); if (v < b) b = v;
            }
            if (b < f[y][x]) { f[y][x] = b; ch = true; }
        }
    }
}
static void reflow(int cx, int cy) {
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) flow[y][x] = INF;
    if (!wcell(cx,cy)) flow[cy][cx] = 0;
    relax(flow); flow_cx = cx; flow_cy = cy;
}

// ---- the danger heatmap: a cone in front of the player's aim ----------------
static void compute_heat(void) {
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) {
        float cxp = x*TILE+4, cyp = y*TILE+4, dx = cxp-pl.x, dy = cyp-pl.y, d = fsqrt(dx*dx+dy*dy);
        float h = 0;
        if (d < 150) {
            float a = angnorm(angle_to(pl.x,pl.y,cxp,cyp) - pl.aim);
            float cone = 1 - fabsf(a)/55.0f;                 // 55deg half-cone
            if (cone > 0) h = cone * (1 - d/150);
            if (d < 30) h = h < 0.5f ? 0.5f : h;             // point-blank ring
        }
        heat[y][x] = h;
    }
}
static float heat_at(float x, float y) { int cx=(int)(x/TILE), cy=(int)(y/TILE); return wcell(cx,cy)?1:heat[cy][cx]; }
static bool near_cover(int cx, int cy) {
    for (int k=0;k<4;k++) { int nx=cx+(k==0)-(k==1), ny=cy+(k==2)-(k==3); if (wcell(nx,ny)) return true; }
    return false;
}

// ---- bullets ---------------------------------------------------------------
static void fire(float x, float y, float aim, int foe, float spread) {
    for (int i=0;i<NB;i++) if (!bul[i].alive) {
        float a = aim + (rnd(1000)/1000.0f-0.5f)*spread;
        bul[i] = (Bullet){ x, y, dx(5.5f,a), dy(5.5f,a), 1, foe };
        play_pan(foe?38:50, INSTR_NOISE, foe?3:4, panx(x), 5);
        return;
    }
}

// ---- setup -----------------------------------------------------------------
static void reset(void) {
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) wall[y][x] = (x==0||y==0||x==GW-1||y==GH-1);
    // scattered cover blocks (the arena)
    int bx[]={8,8,18,18,28,30,14,24,33,6}, by[]={5,15,9,18,5,16,12,12,20,9}, bw[]={3,4,2,5,4,3,2,2,3,2}, bh[]={4,2,5,2,3,2,2,3,2,4};
    for (int b=0;b<10;b++) for (int y=by[b]; y<by[b]+bh[b] && y<GH-1; y++) for (int x=bx[b]; x<bx[b]+bw[b] && x<GW-1; x++) wall[y][x]=1;
    pl = (Player){ TILE*3, TILE*(GH/2), 0, 0, 0, 100, 12, 0, 0, pl.spectate };
    for (int i=0;i<NE;i++) {
        int x,y,t=0; do { x=GW/2+rnd(GW/2-2); y=2+rnd(GH-4); t++; } while (wcell(x,y) && t<80);
        en[i] = (Enemy){ x*TILE+4, y*TILE+4, 180, 30, 1, E_PATROL, 0, 8, 0, 0 };
    }
    for (int i=0;i<NB;i++) bul[i].alive = 0;
    known = 0; kage = 999; flow_cx = -1; kills = 0; msg_t = 0;
}
void init(void) { reverb(0.25f, 0.5f); reset(); }

static void setmsg(const char *s) { snprintf(msg,sizeof msg,"%s",s); msg_t=90; }

// ---- enemy brain -----------------------------------------------------------
static int enemy_at_px(float x, float y, int self) {
    for (int i=0;i<NE;i++) { if (i==self||!en[i].alive) continue; float dx=en[i].x-x, dy=en[i].y-y; if (dx*dx+dy*dy < 100) return i; }
    return -1;
}
static void move_enemy(Enemy *e, float ax, float ay, float spd) {
    float dx=ax-e->x, dy=ay-e->y, d=fsqrt(dx*dx+dy*dy);
    if (d < 0.5f) return;
    float nx = e->x + dx/d*spd, ny = e->y + dy/d*spd;
    if (!wallpx(nx, e->y) && enemy_at_px(nx, e->y, (int)(e-en)) < 0) e->x = nx;   // slide on walls/peers (no jam)
    if (!wallpx(e->x, ny) && enemy_at_px(e->x, ny, (int)(e-en)) < 0) e->y = ny;
}

static void enemy_update(int i) {
    Enemy *e = &en[i];
    if (!e->alive) return;
    if (e->shootcd > 0) e->shootcd--;
    if (e->callcd > 0) e->callcd--;
    bool see = los_px(e->x, e->y, pl.x, pl.y) && !pl.spectate ? los_px(e->x,e->y,pl.x,pl.y) : los_px(e->x,e->y,pl.x,pl.y);
    float pd = fsqrt((pl.x-e->x)*(pl.x-e->x) + (pl.y-e->y)*(pl.y-e->y));

    // --- sense + communicate ---
    if (see) {
        if (!known && e->callcd <= 0) { play_pan(72, INSTR_REED, 3, panx(e->x), 16); setmsg("enemy: \"contact!\""); e->callcd = 120; }
        known = 1; kx = pl.x; ky = pl.y; kage = 0;            // broadcast to the squad
        for (int j=0;j<NE;j++) if (en[j].alive && en[j].state == E_PATROL) en[j].state = E_HUNT;
        e->state = E_ENGAGE;
    } else if (known) {
        e->state = (pd < 24) ? E_ENGAGE : E_HUNT;
        if (kage > 200) { known = 0; e->state = E_SEARCH; }   // knowledge went stale
    } else if (e->state != E_SEARCH) e->state = E_PATROL;

    int kcx = (int)(kx/TILE), kcy = (int)(ky/TILE);
    if (known && (kcx != flow_cx || kcy != flow_cy)) reflow(kcx, kcy);

    // --- act per state ---
    if (e->state == E_ENGAGE) {
        // pick the best nearby firing stance: keep LOS, dodge the heat cone, hug
        // cover, hold range, spread from squadmates  -> emergent flanking
        float best = -1e9f, bx = e->x, by = e->y;
        for (int a=0;a<8;a++) {
            float ox = e->x + dx(7, a*45), oy = e->y + dy(7, a*45);
            if (wallpx(ox, oy) || enemy_at_px(ox, oy, i) >= 0) continue;
            int cx=(int)(ox/TILE), cy=(int)(oy/TILE);
            float od = fsqrt((pl.x-ox)*(pl.x-ox)+(pl.y-oy)*(pl.y-oy));
            float sc = 0;
            sc += los_px(ox,oy,pl.x,pl.y) ? 4 : -6;          // must be able to shoot
            sc -= heat_at(ox,oy) * 6;                         // AVOID the aim cone -> flank
            sc -= fabsf(od - 64) * 0.04f;                     // hold a firing range
            if (near_cover(cx,cy)) sc += 1.5f;                // peek from cover
            for (int j=0;j<NE;j++) if (j!=i && en[j].alive) { float dd=fsqrt((en[j].x-ox)*(en[j].x-ox)+(en[j].y-oy)*(en[j].y-oy)); if (dd<28) sc -= (28-dd)*0.1f; }
            if (sc > best) { best = sc; bx = ox; by = oy; }
        }
        move_enemy(e, bx, by, 0.9f);
        e->aim = angle_to(e->x, e->y, pl.x, pl.y);
        if (los_px(e->x,e->y,pl.x,pl.y) && e->shootcd <= 0 && e->mag > 0) {
            fire(e->x, e->y, e->aim, 1, 14); e->mag--; e->shootcd = 22;
            if (e->mag == 0) { e->reload = 70; }
        }
        if (e->mag == 0) { if (--e->reload <= 0) e->mag = 8; }   // reload window = vulnerable
    } else if (e->state == E_HUNT) {                          // approach around cover via the flow field
        float bestf = flow[(int)(e->y/TILE)][(int)(e->x/TILE)], tx = e->x, ty = e->y;
        for (int a=0;a<8;a++) { float ox=e->x+dx(7,a*45), oy=e->y+dy(7,a*45); int cx=(int)(ox/TILE),cy=(int)(oy/TILE);
            if (wcell(cx,cy)||enemy_at_px(ox,oy,i)>=0) continue; if (flow[cy][cx] < bestf) { bestf=flow[cy][cx]; tx=ox; ty=oy; } }
        move_enemy(e, tx, ty, 0.8f);
    } else if (e->state == E_SEARCH) {
        move_enemy(e, kx, ky, 0.5f);
        if (fsqrt((kx-e->x)*(kx-e->x)+(ky-e->y)*(ky-e->y)) < 10) e->state = E_PATROL;
    } else {                                                  // PATROL: small idle drift
        if (rnd(40)==0) e->aim = rnd(360);
        move_enemy(e, e->x+dx(8,e->aim), e->y+dy(8,e->aim), 0.25f);
    }
}

// ---- player ----------------------------------------------------------------
static void player_update(void) {
    if (pl.spectate) {                                        // autopilot: kite the nearest visible enemy
        int t=-1; float bd=1e9f;
        for (int i=0;i<NE;i++) if (en[i].alive && los_px(pl.x,pl.y,en[i].x,en[i].y)) { float d=(en[i].x-pl.x)*(en[i].x-pl.x)+(en[i].y-pl.y)*(en[i].y-pl.y); if (d<bd){bd=d;t=i;} }
        if (t>=0) { pl.aim = angle_to(pl.x,pl.y,en[t].x,en[t].y);
            float perp = pl.aim+90, mx = dx(1.4f,perp)+dx(0.6f,pl.aim+180), my = dy(1.4f,perp)+dy(0.6f,pl.aim+180);
            if (!wallpx(pl.x+mx,pl.y)) pl.x+=mx; if (!wallpx(pl.x,pl.y+my)) pl.y+=my;
            if (pl.mag>0 && (tick%9==0)) { fire(pl.x,pl.y,pl.aim,0,6); pl.mag--; if(pl.mag==0) pl.reload=40; } }
        if (pl.mag==0 && --pl.reload<=0) pl.mag=12;
        return;
    }
    float mx=0,my=0;
    if (key('A')||key(KEY_LEFT)) mx=-1.6f; else if (key('D')||key(KEY_RIGHT)) mx=1.6f;
    if (key('W')||key(KEY_UP)) my=-1.6f; else if (key('S')||key(KEY_DOWN)) my=1.6f;
    if (!wallpx(pl.x+mx, pl.y)) pl.x += mx; if (!wallpx(pl.x, pl.y+my)) pl.y += my;
    pl.aim = angle_to(pl.x, pl.y, mouse_x(), mouse_y());
    if ((mouse_down(0)||key(KEY_SPACE)) && pl.mag>0 && pl.reload<=0) { fire(pl.x,pl.y,pl.aim,0,5); pl.mag--; pl.reload=8; if(pl.mag==0) pl.reload=45; }
    if (pl.reload>0) pl.reload--;
    if (pl.mag==0 && pl.reload<=0) pl.mag=12;
}

void update(void) {
    voices_tick();
    tick++;
    if (msg_t>0) msg_t--;
    if (keyp('R')) { reset(); return; }
    if (keyp('H')) show_heat = !show_heat;
    if (keyp('L')) show_comms = !show_comms;
    if (keyp(KEY_TAB)) { pl.spectate = !pl.spectate; }
    if (pl.hp <= 0) return;

    if (known) kage++;
    compute_heat();
    player_update();
    for (int i=0;i<NE;i++) enemy_update(i);

    // bullets
    for (int i=0;i<NB;i++) {
        if (!bul[i].alive) continue;
        bul[i].x += bul[i].vx; bul[i].y += bul[i].vy;
        if (wallpx(bul[i].x, bul[i].y)) { bul[i].alive = 0; continue; }
        if (bul[i].foe) {
            if (fabsf(bul[i].x-pl.x)<4 && fabsf(bul[i].y-pl.y)<4) { pl.hp -= 8; pl.shake=6; bul[i].alive=0; play_pan(34,INSTR_NOISE,4,panx(pl.x),6);
                if (pl.hp<=0){ pl.hp=0; setmsg("you are down. R to retry."); } }
        } else for (int j=0;j<NE;j++) if (en[j].alive && fabsf(bul[i].x-en[j].x)<4 && fabsf(bul[i].y-en[j].y)<4) {
            en[j].hp -= 12; bul[i].alive=0; play_pan(60,INSTR_MEMBRANE,3,panx(en[j].x),5);
            if (en[j].hp<=0) { en[j].alive=0; en[j].state=E_DOWN; kills++; play_pan(40,INSTR_REED,3,panx(en[j].x),18); }
            break;
        }
    }
    if (pl.shake>0) pl.shake--;

#ifdef DE_TRACE
    int alive=0,eng=0; for(int i=0;i<NE;i++) if(en[i].alive){alive++; if(en[i].state==E_ENGAGE)eng++;}
    watch("hp","%d",pl.hp); watch("alive","%d",alive); watch("engaging","%d",eng);
    watch("known","%d",known); watch("kills","%d",kills);
#endif
}

// ---- draw ------------------------------------------------------------------
void draw(void) {
    cls(CLR_BLACK);
    int sh = pl.shake>0 ? rnd(3)-1 : 0;
    // heat overlay (danger cone) — debug viz
    if (show_heat) for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) if (heat[y][x] > 0.05f) {
        int h = heat[y][x] > 0.6f ? CLR_RED : heat[y][x] > 0.3f ? CLR_ORANGE : CLR_BROWN;
        rectfill(x*TILE+sh, y*TILE, TILE, TILE, h);
    }
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) if (wall[y][x]) rectfill(x*TILE+sh, y*TILE, TILE, TILE, CLR_DARK_GREY);

    // comms: a line from each seeing enemy to the shared known position
    if (show_comms && known) for (int i=0;i<NE;i++) if (en[i].alive && los_px(en[i].x,en[i].y,pl.x,pl.y))
        line((int)en[i].x+sh, (int)en[i].y, (int)kx+sh, (int)ky, CLR_DARK_GREEN);
    if (known) { pset((int)kx+sh,(int)ky,CLR_GREEN); circ((int)kx+sh,(int)ky,3,CLR_DARK_GREEN); }

    // bullets
    for (int i=0;i<NB;i++) if (bul[i].alive) {
        line((int)bul[i].x+sh,(int)bul[i].y,(int)(bul[i].x-bul[i].vx)+sh,(int)(bul[i].y-bul[i].vy), bul[i].foe?CLR_RED:CLR_YELLOW);
    }
    // enemies (coloured by state) + aim tick
    for (int i=0;i<NE;i++) { if (!en[i].alive) continue; int x=(int)en[i].x+sh, y=(int)en[i].y;
        circfill(x,y,3,ECOL[en[i].state]);
        line(x,y,(int)(en[i].x+dx(5,en[i].aim))+sh,(int)(en[i].y+dy(5,en[i].aim)),CLR_LIGHT_GREY);
    }
    // player
    int px=(int)pl.x+sh, py=(int)pl.y;
    circfill(px,py,3,pl.hp>0?CLR_WHITE:CLR_DARK_GREY);
    line(px,py,(int)(pl.x+dx(8,pl.aim))+sh,(int)(pl.y+dy(8,pl.aim)),CLR_YELLOW);

    // HUD
    rectfill(0,HUD_Y,SCREEN_W,SCREEN_H-HUD_Y,CLR_DARKER_BLUE);
    rect(4,HUD_Y+3,42,6,CLR_DARK_GREY); rectfill(5,HUD_Y+4,40*(pl.hp>0?pl.hp:0)/100,4,pl.hp>30?CLR_GREEN:CLR_RED);
    print(str("HP %d", pl.hp>0?pl.hp:0), 50, HUD_Y+2, CLR_WHITE);
    print(str("ammo %d", pl.mag), 96, HUD_Y+2, pl.mag?CLR_YELLOW:CLR_RED);
    print(str("down %d/%d", kills, NE), 150, HUD_Y+2, CLR_ORANGE);
    print_right(pl.spectate?"SPECTATE":(known?"SPOTTED":"hidden"), SCREEN_W-4, HUD_Y+2, known?CLR_RED:CLR_GREEN);
    font(FONT_SMALL);
    if (msg_t>0) print(msg, 4, HUD_Y+12, CLR_LIGHT_PEACH);
    else print("WASD move  mouse aim  click shoot   H heat  L comms  TAB spectate  R reset", 4, HUD_Y+12, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
