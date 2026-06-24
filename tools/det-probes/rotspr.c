// Probe (Fork 2, rotated sprites) — a rotated sprite = rotfill's INVERSE mapping + a texture
// sample. Two questions: (1) is it gap-free + device-stable like rotfill? (2) how much source
// detail does naive nearest-neighbour DROP (the thing RotSprite fixes)? Compares NEAREST (1 sample
// per dest pixel) vs SUPERSAMPLE (4x4 subsamples, majority vote — a crisp-preserving quality stand-
// in for RotSprite). Sweeps 360° for metrics + dumps a visual PPM at one angle (native run).
//   clang -O2 rotspr.c -o rs && ./rs   |   emcc -O2 rotspr.c -o rs.js && node rs.js
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define SS 16          // source sprite is SS x SS
#define DW 26          // dest canvas (room for rotated corners)
#define DH 26
static unsigned char src[SS*SS];
static unsigned char dN[DW*DH], dS[DW*DH];   // dest: nearest, supersampled
static unsigned char hit[SS*SS];              // which source texels got sampled (drop metric)

// source pattern: 1px frame (1), a diagonal (2), a single interior dot (3); 0 = transparent.
static void build_src(void){
    for(int y=0;y<SS;y++)for(int x=0;x<SS;x++){
        unsigned char c=0;
        if(x==0||y==0||x==SS-1||y==SS-1) c=1;       // frame
        if(x==y) c=2;                                // diagonal
        if(x==11&&y==4) c=3;                         // lone dot (the canary)
        src[y*SS+x]=c;
    }
}
static unsigned char samp(float sx,float sy,int markhit){
    int ix=(int)floorf(sx), iy=(int)floorf(sy);
    if((unsigned)ix>=SS||(unsigned)iy>=SS) return 0;
    if(markhit) hit[iy*SS+ix]=1;
    return src[iy*SS+ix];
}
static int components(unsigned char *g){
    static int st[DW*DH]; static unsigned char seen[DW*DH]; int comps=0;
    for(int i=0;i<DW*DH;i++) seen[i]=0;
    for(int i=0;i<DW*DH;i++){ if(!g[i]||seen[i]) continue; comps++; int sp=0; st[sp++]=i; seen[i]=1;
        while(sp){ int p=st[--sp],x=p%DW,y=p/DW;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){ if(!dx&&!dy)continue;
                int nx=x+dx,ny=y+dy; if((unsigned)nx<DW&&(unsigned)ny<DH){int q=ny*DW+nx;
                    if(g[q]&&!seen[q]){seen[q]=1;st[sp++]=q;} } } } }
    return comps;
}
// render the sprite rotated by `a` into dN (nearest) and dS (supersample). cx/cy = dest centre.
static void render(float a){
    float c=cosf(a), s=sinf(a), cx=DW*0.5f, cy=DH*0.5f, ox=SS*0.5f, oy=SS*0.5f;
    for(int i=0;i<DW*DH;i++){ dN[i]=0; dS[i]=0; }
    for(int i=0;i<SS*SS;i++) hit[i]=0;
    for(int py=0;py<DH;py++) for(int px=0;px<DW;px++){
        float dx=px+0.5f-cx, dy=py+0.5f-cy;
        // NEAREST: one inverse-mapped sample at the pixel centre.
        float lx= c*dx + s*dy, ly= -s*dx + c*dy;
        dN[py*DW+px] = samp(ox+lx, oy+ly, 1);
        // SUPERSAMPLE: 4x4 subsamples, majority vote (transparent counts as a candidate).
        int cnt[4]={0,0,0,0};
        for(int sj=0;sj<4;sj++) for(int si=0;si<4;si++){
            float ex=px+(si+0.5f)/4.f-cx, ey=py+(sj+0.5f)/4.f-cy;
            float qx= c*ex + s*ey, qy= -s*ex + c*ey;
            cnt[ samp(ox+qx, oy+qy, 0) ]++;
        }
        int best=0; for(int k=1;k<4;k++) if(cnt[k]>cnt[best]) best=k;
        dS[py*DW+px] = (unsigned char)best;
    }
}
int main(void){
    build_src();
    int src_solid=0; for(int i=0;i<SS*SS;i++) if(src[i]) src_solid++;
    int worst_dropN=0, worst_compsN=1, dotN=0, dotS=0; uint64_t h=1469598103934665603ULL;
    for(int deg=0; deg<360; deg++){
        render(deg*3.14159265f/180.f);
        int dropped=0; for(int i=0;i<SS*SS;i++) if(src[i] && !hit[i]) dropped++;   // nearest texels missed
        if(dropped>worst_dropN) worst_dropN=dropped;
        int comps=components(dN); if(comps>worst_compsN) worst_compsN=comps;        // frame fragmentation
        int hasN=0,hasS=0; for(int i=0;i<DW*DH;i++){ if(dN[i]==3)hasN=1; if(dS[i]==3)hasS=1; }
        dotN+=hasN; dotS+=hasS;                                                     // lone-dot survival / 360
        for(int i=0;i<DW*DH;i++){ h=(h^dN[i])*1099511628211ULL; h=(h^dS[i])*1099511628211ULL; }
    }
    // Honest verdict: footprint is gap-free + device-stable, but thin CONTENT degrades both ways —
    // nearest fragments 1px lines (high comps) & misses texels; supersample-majority erases the lone
    // dot. Neither is RotSprite (edge-aware) — which is why that's the documented quality path.
    printf("hash=%016llx | src_solid=%d | NEAREST: worst_dropped=%d frame_comps<=%d dot_survives=%d/360 "
           "| SUPERSAMPLE: dot_survives=%d/360\n",
           (unsigned long long)h, src_solid, worst_dropN, worst_compsN, dotN, dotS);

    // visual PPM at 32deg: nearest | supersample, magnified 8x, side by side.
    render(32.f*3.14159265f/180.f);
    int Z=8, GAP=8, PW=DW*Z*2+GAP, PH=DH*Z;
    FILE *f=fopen("rotspr.ppm","wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",PW,PH);
        for(int y=0;y<PH;y++) for(int x=0;x<PW;x++){
            unsigned char v=0; int panel0 = x < DW*Z;
            if(panel0){ v=dN[(y/Z)*DW + x/Z]; }
            else { int xx=x-DW*Z-GAP; if(xx>=0) v=dS[(y/Z)*DW + xx/Z]; else { fputc(24,f);fputc(24,f);fputc(24,f); continue; } }
            unsigned char r,g,b;
            if(v==1){r=200;g=200;b=210;}      // frame
            else if(v==2){r=90;g=170;b=255;}  // diagonal
            else if(v==3){r=255;g=80;b=80;}   // the lone dot
            else {r=24;g=24;b=28;}            // transparent/bg
            fputc(r,f);fputc(g,f);fputc(b,f);
        }
        fclose(f);
    }
    return 0;
}
