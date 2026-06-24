#include "studio.h"
#include <math.h>

// CITYVIEW — pseudo-3D top-down city: GTA1 meets Zelda. A test bench for the
// "see the FRONTS, height goes straight UP the screen, no needless diagonals"
// look. Drive around a seeded city; press M to cycle FOUR view modes and feel
// the difference between them on the same streets:
//
//   1 NORTH-UP    camera locked to north; you always see the south-facing
//                 fronts. The purest Zelda look — zero diagonals, ever.
//   2 HEADING-UP  camera rotates to your heading; you see whichever 1-2 walls
//                 you're driving toward. Footprint edges go diagonal (fine — so
//                 does the road grid) but every wall's UP is still screen-up.
//   3 BILLBOARD   heading-up, but each building turns its front to face you
//                 (a flat card). Cheap; lovely for trees/props, odd for boxes.
//   4 LEAN-OUT    heading-up, roofs pushed AWAY from screen centre instead of
//                 straight up — the classic "diagonal walls everywhere" GTA
//                 extrusion, here for contrast.
//
// The whole renderer is flat quadfill: a lot footprint, 1-2 shaded walls, a
// roof cap. Modes 1/2/4 share ONE geometric extruder; only the roof-offset
// vector and the camera angle change. Mode 3 is the odd one (screen-aligned
// card). Drawn back-to-front (painter's) so near blocks overlap far ones.
//
// CONTROLS — arrows drive · M cycle view · [ ] zoom out/in · R reseed city
//
// VERDICT (same corner, same heading, headless A/B): mode 2 (HEADING-UP) is the
// keeper — real-reading blocks, walls straight up, diagonals only on footprints
// and the road grid. Mode 1 is the calm no-rotation map view; mode 3 is for
// props not boxes; mode 4 shows the melty diagonal-walls cost we're avoiding.
//
// WHERE THIS GOES NEXT (ideas, roughest → most load-bearing):
//   - THE HYBRID (the real test): mode 2 for buildings + mode 3 billboards for
//     trees / people / lamp-posts / signs in the SAME scene. Boxes get walls,
//     thin props face you as cards — that's the "Zelda town you can drive" look.
//   - NORTH-UP READABILITY: mode 1 reads flat. Taller/more-shaded front walls or
//     a thin roof-edge outline gives it pop without introducing any diagonals.
//   - BUILDING COLLISION: today the car drives onto/through buildings. Footprint
//     collision makes it a street, not a diorama (feeds the sloop drive feel).
//   - WIRE TO THE WORLD: this is big-game-backlog seam #2 (the cityscape
//     renderer). Swap the seeded grid for the composer's road_at()/lotfill atoms
//     so it draws the REAL world instead of a demo grid.
//   - DEPTH POLISH: per-building drop shadow on the ground, window rows on tall
//     walls (cheap dithered bands), night palette + lit windows, a horizon tint.
//   - Capture the "UP stays screen-up" rule + this 4-mode finding in a
//     docs/design/ note so it outlives this header.

#define PITCH   42          // block centre-to-centre, world units
#define ROADH   7.0f        // half road width
#define RANGE   7           // cells drawn each side of the car
#define HSCALE  1.0f        // how tall a height unit reads on screen
#define NMAT    6
#define TILE_W  6.0f        // world units per 16px texture tile (keeps bricks crisp)
#define QUARTER 0x7BDE      // ~25% black dither (1-bits transparent)
#define MAXREP  6           // cap texture repeats per wall (stress-test ceiling)

// material → wall texture slot (sprites in cityview.cart.js): 0 brick 1 block 2 glass 3 stucco
static const int MATSLOT[NMAT] = { 1, 0, 3, 2, 1, 3 };
static int clampi(int v, int lo, int hi) { return v<lo?lo:(v>hi?hi:v); }

typedef struct { int roof, lit, shade; } Mat;
static const Mat MAT[NMAT] = {
  { CLR_LIGHT_GREY,  CLR_MEDIUM_GREY, CLR_DARK_GREY    },  // concrete
  { CLR_LIGHT_PEACH, CLR_ORANGE,      CLR_BROWN        },  // brick
  { CLR_PINK,        CLR_DARK_RED,    CLR_DARK_PURPLE  },  // painted
  { CLR_BLUE,        CLR_TRUE_BLUE,   CLR_DARKER_BLUE  },  // glass
  { CLR_LIME_GREEN,  CLR_MEDIUM_GREEN,CLR_DARK_GREEN   },  // green roof
  { CLR_LIGHT_YELLOW,CLR_DARK_ORANGE, CLR_DARK_BROWN   },  // sand
};

// view-mode ids
enum { M_NORTH, M_HEADING, M_BILLBOARD, M_LEANOUT, M_COUNT };
static const char *MODE_NAME[M_COUNT] = {
  "1 NORTH-UP (zelda)", "2 HEADING-UP (fronts)", "3 BILLBOARD", "4 LEAN-OUT"
};

typedef struct {
  float px, py, ang, spd;     // car: pos, heading(deg), speed
  float camx, camy, rot;      // camera: eased pos + eased rotation(deg applied to world)
  float zoom;
  int   mode, seed;
  bool  started, tex;
} State;
static State S;

// deterministic per-cell hash → 0..1 floats
static unsigned hashu(int x, int y, int salt) {
  unsigned h = (unsigned)(x*374761393 + y*668265263 + salt*2246822519u + S.seed*3266489917u);
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
static float hf(int x, int y, int salt) { return (hashu(x,y,salt) & 0xffffff) / (float)0xffffff; }

// a building living in cell (gx,gy): footprint rect (world) + height + material
typedef struct { float x0,y0,x1,y1,h; int mat; bool exists; } Bldg;
static Bldg cell_bldg(int gx, int gy) {
  Bldg b; b.exists = false;
  float cx = gx*(float)PITCH, cy = gy*(float)PITCH;
  if (hf(gx,gy,9) < 0.12f) return b;            // ~12% of lots are empty (parks/plazas)
  float lot = PITCH*0.5f - ROADH;               // half-extent of the buildable lot
  float mx = lot * (0.55f + 0.40f*hf(gx,gy,1)); // building half-extents
  float my = lot * (0.55f + 0.40f*hf(gx,gy,2));
  float jx = (hf(gx,gy,3)-0.5f) * (lot-mx)*1.2f;// jitter inside the lot
  float jy = (hf(gx,gy,4)-0.5f) * (lot-my)*1.2f;
  b.x0 = cx+jx-mx; b.x1 = cx+jx+mx;
  b.y0 = cy+jy-my; b.y1 = cy+jy+my;
  float t = hf(gx,gy,5); b.h = 5 + t*t*46;       // skewed toward short, a few towers
  b.mat = hashu(gx,gy,6) % NMAT;
  b.exists = true;
  return b;
}

// project a world point at height h to screen, applying camera pos + rotation.
static void project(float wx, float wy, float h, float *sx, float *sy) {
  float dx = wx - S.camx, dy = wy - S.camy;
  float c = cos_deg(S.rot), s = sin_deg(S.rot);
  float rx = dx*c - dy*s, ry = dx*s + dy*c;     // rotate world by S.rot
  *sx = SCREEN_W*0.5f + rx*S.zoom;
  *sy = SCREEN_H*0.5f + ry*S.zoom - h*S.zoom*HSCALE;
}

// ── lifecycle ──────────────────────────────────────────────────────────────
static void reset(void) {
  S.px = 6; S.py = ROADH+2; S.ang = 0; S.spd = 0;
  S.camx = S.px; S.camy = S.py; S.rot = 0;
  S.zoom = 2.2f;
  if (!S.started) { S.mode = M_NORTH; S.zoom = 2.2f; S.tex = true; }
  S.started = true;
}

void init(void) { S.seed = 1; reset(); }

void update(void) {
  // mode + zoom + reseed
  if (keyp('M')) S.mode = (S.mode+1) % M_COUNT;
  if (keyp('1')) S.mode = M_NORTH;
  if (keyp('2')) S.mode = M_HEADING;
  if (keyp('3')) S.mode = M_BILLBOARD;
  if (keyp('4')) S.mode = M_LEANOUT;
  if (keyp('T')) S.tex = !S.tex;
  if (key('[')) S.zoom = fmaxf(0.9f, S.zoom*0.97f);
  if (key(']')) S.zoom = fminf(6.0f, S.zoom*1.03f);
  if (keyp('R')) { S.seed++; reset(); }

  // simple arcade driving
  float thr = (key(KEY_UP)?1:0) - (key(KEY_DOWN)?1:0);
  float steer = (key(KEY_RIGHT)?1:0) - (key(KEY_LEFT)?1:0);
  S.spd += thr*0.06f;
  S.spd *= 0.97f;                               // drag
  if (fabsf(S.spd) < 0.005f) S.spd = 0;
  S.ang += steer * (1.6f + fminf(fabsf(S.spd)*2.0f, 2.4f)) * (S.spd>=0?1:-1);
  S.px += cos_deg(S.ang)*S.spd;
  S.py += sin_deg(S.ang)*S.spd;

  // camera eases to the car
  S.camx += (S.px - S.camx)*0.18f;
  S.camy += (S.py - S.camy)*0.18f;

  // camera rotation: locked north, or eased toward heading-up
  float target = (S.mode == M_NORTH) ? 0.0f : (-90.0f - S.ang);
  // shortest-arc ease (handle wrap)
  float d = fmodf(target - S.rot + 540.0f, 360.0f) - 180.0f;
  S.rot += d * 0.16f;
}

// texture-map one wall quad: ground edge a→b, roof edge ca→cb (screen coords).
// the wall is subdivided into RX×RY tiles, each mapped to the FULL 16px texture
// slot — that REPEATS the bricks (no stretch) AND keeps each affine triangle
// small (no PS1 swim). a dither-shadow overlay darkens side-facing walls.
static void wall_tex(float ax,float ay, float bx,float by, float cax,float cay, float cbx,float cby,
                     int slot, int rxN, int ryN, float facing) {
  int scol = slot % 8, srow = slot / 8;
  float su = scol*16.0f, sv = srow*16.0f;
  float u0 = su+0.5f, u1 = su+15.5f, vtop = sv+0.5f, vbot = sv+15.5f;
  for (int j=0;j<ryN;j++) {
    float g0=(float)j/ryN, g1=(float)(j+1)/ryN;   // 0 = ground, 1 = roof
    for (int i=0;i<rxN;i++) {
      float f0=(float)i/rxN, f1=(float)(i+1)/rxN;  // along the wall
      #define PX(f,g) (int)(((ax+(bx-ax)*(f))*(1-(g))) + ((cax+(cbx-cax)*(f))*(g)))
      #define PY(f,g) (int)(((ay+(by-ay)*(f))*(1-(g))) + ((cay+(cby-cay)*(f))*(g)))
      int x00=PX(f0,g0),y00=PY(f0,g0), x10=PX(f1,g0),y10=PY(f1,g0);
      int x11=PX(f1,g1),y11=PY(f1,g1), x01=PX(f0,g1),y01=PY(f0,g1);
      #undef PX
      #undef PY
      tritex(x00,y00,u0,vbot, x10,y10,u1,vbot, x11,y11,u1,vtop);
      tritex(x00,y00,u0,vbot, x11,y11,u1,vtop, x01,y01,u0,vtop);
    }
  }
  // sparse patterns only — a 50% checker collapses into diagonal lines on a
  // sheared wall. front wall = clean, mid = dots, most side-on = ~25% dither.
  int pat = facing>0.66f ? 0 : (facing>0.40f ? FILL_DOTS : QUARTER);
  if (pat) {
    fillp(pat, -1);                               // black on 0-bits, holes transparent
    quadfill((int)ax,(int)ay,(int)bx,(int)by,(int)cbx,(int)cby,(int)cax,(int)cay, CLR_BLACK);
    fillp_reset();
  }
}

// draw one building's geometry (modes 1/2/4) — visible walls then roof cap.
static void draw_bldg_geo(Bldg *b, bool leanout) {
  // footprint corners CCW-ish
  float wx[4] = { b->x0, b->x1, b->x1, b->x0 };
  float wy[4] = { b->y0, b->y0, b->y1, b->y1 };
  float gx[4], gy[4], rx[4], ry[4];
  for (int i=0;i<4;i++) project(wx[i], wy[i], 0, &gx[i], &gy[i]);

  if (leanout) {
    // roof pushed AWAY from screen centre, scaled by height — the GTA extrusion
    for (int i=0;i<4;i++) {
      float dirx = gx[i]-SCREEN_W*0.5f, diry = gy[i]-SCREEN_H*0.5f;
      float k = b->h * 0.012f;
      rx[i] = gx[i] + dirx*k; ry[i] = gy[i] + diry*k;
    }
  } else {
    // roof straight UP the screen by height — the Zelda extrusion
    for (int i=0;i<4;i++) project(wx[i], wy[i], b->h, &rx[i], &ry[i]);
  }

  // visible walls: a wall is exposed iff its screen-space outward normal points
  // OPPOSITE the roof's shift direction (the side the roof slides off of).
  //   straight-up extrusion → shift = screen-up, so the down-facing (south) walls
  //   show;  lean-out → shift = radially away from screen centre, so the walls
  //   facing back TOWARD centre show. One test, both behaviours.
  // edges: 0:(0-1) 1:(1-2) 2:(2-3) 3:(3-0)
  float c = cos_deg(S.rot), s = sin_deg(S.rot);
  // world outward normals of each edge for an axis-aligned footprint:
  // edge0 top(-y): (0,-1) · edge1 right(+x): (1,0) · edge2 bottom(+y): (0,1) · edge3 left(-x): (-1,0)
  float nwx[4] = { 0,  1, 0, -1 };
  float nwy[4] = {-1,  0, 1,  0 };
  float sdx, sdy;                               // unit roof-shift direction (screen)
  if (leanout) {
    float bcx=(gx[0]+gx[1]+gx[2]+gx[3])*0.25f - SCREEN_W*0.5f;
    float bcy=(gy[0]+gy[1]+gy[2]+gy[3])*0.25f - SCREEN_H*0.5f;
    float L=sqrtf(bcx*bcx+bcy*bcy)+1e-4f; sdx=bcx/L; sdy=bcy/L;
  } else { sdx=0; sdy=-1; }
  int   order[4]; int n=0; float key_[4];
  for (int e=0;e<4;e++) {
    float nsx = nwx[e]*c - nwy[e]*s;            // rotate world normal into screen
    float nsy = nwx[e]*s + nwy[e]*c;
    float facing = -(nsx*sdx + nsy*sdy);        // >0 = exposed wall
    if (facing > 0.001f) { order[n]=e; key_[n]=facing; n++; }
  }
  // sort visible walls by facing (least-front first → painter's within the box)
  for (int i=0;i<n;i++) for (int j=i+1;j<n;j++)
    if (key_[j] < key_[i]) { float t=key_[i];key_[i]=key_[j];key_[j]=t; int o=order[i];order[i]=order[j];order[j]=o; }

  for (int k=0;k<n;k++) {
    int e = order[k]; int a=e, bb=(e+1)&3;
    if (S.tex) {
      float wlen = (e&1) ? (b->y1-b->y0) : (b->x1-b->x0);
      int rxN = clampi((int)(wlen/TILE_W + 0.5f), 1, MAXREP);
      int ryN = clampi((int)(b->h /TILE_W + 0.5f), 1, MAXREP);
      wall_tex(gx[a],gy[a], gx[bb],gy[bb], rx[a],ry[a], rx[bb],ry[bb],
               MATSLOT[b->mat], rxN, ryN, key_[k]);
    } else {
      // flat fill: brighter for the most front-facing wall, darker for the side
      int col = (key_[k] > 0.6f) ? MAT[b->mat].lit : MAT[b->mat].shade;
      quadfill((int)gx[a],(int)gy[a], (int)gx[bb],(int)gy[bb],
               (int)rx[bb],(int)ry[bb], (int)rx[a],(int)ry[a], col);
    }
  }
  // roof cap
  quadfill((int)rx[0],(int)ry[0],(int)rx[1],(int)ry[1],
           (int)rx[2],(int)ry[2],(int)rx[3],(int)ry[3], MAT[b->mat].roof);
}

// mode 3: screen-aligned billboard card always facing the camera
static void draw_bldg_billboard(Bldg *b) {
  float cxw = (b->x0+b->x1)*0.5f, cyw=(b->y0+b->y1)*0.5f;
  float sx, sy; project(cxw, cyw, 0, &sx, &sy);
  float w = (b->x1-b->x0)*S.zoom*0.5f;
  float ht = b->h*S.zoom*HSCALE;
  // body (front card) + a thin roof strip on top for read
  rectfill((int)(sx-w),(int)(sy-ht),(int)(w*2),(int)ht, MAT[b->mat].lit);
  rectfill((int)(sx-w),(int)(sy-ht),(int)(w*2),2, MAT[b->mat].roof);
}

// ── ground: asphalt everywhere, then a grass/pavement lot quad per cell ──────
static void draw_ground(int cgx, int cgy) {
  for (int gy=cgy-RANGE; gy<=cgy+RANGE; gy++)
  for (int gx=cgx-RANGE; gx<=cgx+RANGE; gx++) {
    float cx = gx*(float)PITCH, cy = gy*(float)PITCH;
    float h2 = PITCH*0.5f - ROADH;
    float lx0=cx-h2, ly0=cy-h2, lx1=cx+h2, ly1=cy+h2;
    int lot = (hf(gx,gy,9) < 0.12f) ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK;
    float ax,ay,bx,by,ccx,ccy,dx2,dy2;
    project(lx0,ly0,0,&ax,&ay); project(lx1,ly0,0,&bx,&by);
    project(lx1,ly1,0,&ccx,&ccy); project(lx0,ly1,0,&dx2,&dy2);
    quadfill((int)ax,(int)ay,(int)bx,(int)by,(int)ccx,(int)ccy,(int)dx2,(int)dy2, lot);
  }
}

void draw(void) {
  cls(CLR_DARK_GREY);                            // asphalt = road grid shows through
  int cgx = (int)floorf(S.px/PITCH + 0.5f);
  int cgy = (int)floorf(S.py/PITCH + 0.5f);

  draw_ground(cgx, cgy);

  // collect visible buildings, sort back-to-front by ground-centre screen-y
  static Bldg list[(2*RANGE+1)*(2*RANGE+1)];
  static float depth[(2*RANGE+1)*(2*RANGE+1)];
  int nb=0;
  for (int gy=cgy-RANGE; gy<=cgy+RANGE; gy++)
  for (int gx=cgx-RANGE; gx<=cgx+RANGE; gx++) {
    Bldg b = cell_bldg(gx,gy);
    if (!b.exists) continue;
    float sx,sy; project((b.x0+b.x1)*0.5f,(b.y0+b.y1)*0.5f,0,&sx,&sy);
    if (sx<-60||sx>SCREEN_W+60||sy<-80||sy>SCREEN_H+80) continue;
    list[nb]=b; depth[nb]=sy; nb++;
  }
  for (int i=0;i<nb;i++) for (int j=i+1;j<nb;j++)
    if (depth[j]<depth[i]) { float t=depth[i];depth[i]=depth[j];depth[j]=t; Bldg tb=list[i];list[i]=list[j];list[j]=tb; }

  for (int i=0;i<nb;i++) {
    if (S.mode==M_BILLBOARD) draw_bldg_billboard(&list[i]);
    else                     draw_bldg_geo(&list[i], S.mode==M_LEANOUT);
  }

  // the car at screen centre — long axis along its screen heading
  float screenAng = S.ang + S.rot;
  rectfill_rot(SCREEN_W/2, SCREEN_H/2, 10, 6, screenAng, CLR_WHITE);
  rectfill_rot(SCREEN_W/2 + (int)(cos_deg(screenAng)*3), SCREEN_H/2 + (int)(sin_deg(screenAng)*3), 4, 5, screenAng, CLR_RED);

  // HUD
  rectfill(0,0,SCREEN_W,9, CLR_BLACK);
  print(MODE_NAME[S.mode], 3, 2, CLR_YELLOW);
  print(S.tex?"M:view T:tex* [ ]:zoom":"M:view T:tex [ ]:zoom", SCREEN_W-150, 2, CLR_LIGHT_GREY);
}
