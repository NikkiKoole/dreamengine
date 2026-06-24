// Probe (Fork 2, rotated strokes) — is a crisp (non-AA) rotated 1px line via `sline` actually
// uniform-width and connected at EVERY angle, and device-stable? The doc worried rotated strokes
// re-inherit the consistency/aliasing problem and might need a sub-pixel (Xiaolin-Wu) drawer. This
// quantifies the CRISP floor: sweep one line through 360°, measure per angle:
//   excess  = filled_px - (major_extent+1)   → 0 means perfectly 1-per-major-step (no fat spots)
//   comps   = connected components             → 1 means no gaps
//   churn   = pixels changed vs the previous 1° step → a proxy for rotation "shimmer"
// Build/run via tools/det-probes/run.sh, or: clang -O2 rotline.c -o rl && ./rl  | emcc ... && node
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define W 200
#define H 200
static unsigned char g[W*H], prev[W*H];

static void mark(int x,int y){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H) g[y*W+x]=1; }
static int round_minor(float v,float mid){
    float f=floorf(v); int fi=(int)f; float r=v-f;
    if(r!=0.5f) return (r<0.5f)?fi:fi+1;
    if(v==mid)  return fi;
    return (mid>v)?fi+1:fi;
}
static void sline(int x0,int y0,int x1,int y1){
    int dx=x1-x0,dy=y1-y0,adx=dx<0?-dx:dx,ady=dy<0?-dy:dy;
    if(adx==0&&ady==0){ mark(x0,y0); return; }
    if(adx>=ady){ int lo=x0<x1?x0:x1,hi=x0<x1?x1:x0; float ymid=(y0+y1)*0.5f;
        for(int x=lo;x<=hi;x++) mark(x, round_minor(y0+(float)(x-x0)*dy/(float)dx, ymid));
    } else { int lo=y0<y1?y0:y1,hi=y0<y1?y1:y0; float xmid=(x0+x1)*0.5f;
        for(int y=lo;y<=hi;y++) mark(round_minor(x0+(float)(y-y0)*dx/(float)dy, xmid), y); }
}
static int components(void){
    static int st[W*H]; static unsigned char seen[W*H]; int comps=0;
    for(int i=0;i<W*H;i++) seen[i]=0;
    for(int i=0;i<W*H;i++){ if(!g[i]||seen[i]) continue; comps++; int sp=0; st[sp++]=i; seen[i]=1;
        while(sp){ int p=st[--sp],x=p%W,y=p/W;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){ if(!dx&&!dy)continue;
                int nx=x+dx,ny=y+dy; if((unsigned)nx<W&&(unsigned)ny<H){int q=ny*W+nx;
                    if(g[q]&&!seen[q]){seen[q]=1;st[sp++]=q;} } } } }
    return comps;
}
int main(void){
    float cx=100,cy=100,R=80;
    int worst_excess=0, worst_comps=1, max_churn=0; uint64_t h=1469598103934665603ULL;
    for(int deg=0; deg<360; deg++){
        for(int i=0;i<W*H;i++) g[i]=0;
        float a=deg*3.14159265f/180.f;
        int x0=(int)lroundf(cx+R*cosf(a)), y0=(int)lroundf(cy+R*sinf(a));
        int x1=(int)lroundf(cx-R*cosf(a)), y1=(int)lroundf(cy-R*sinf(a));
        sline(x0,y0,x1,y1);
        int filled=0; for(int i=0;i<W*H;i++) filled+=g[i];
        int adx=x1-x0,ady=y1-y0; adx=adx<0?-adx:adx; ady=ady<0?-ady:ady;
        int major=adx>ady?adx:ady;
        int excess=filled-(major+1);
        int comps=components();
        int churn=0; if(deg>0){ for(int i=0;i<W*H;i++) if(g[i]!=prev[i]) churn++; }
        if(excess>worst_excess) worst_excess=excess;
        if(comps>worst_comps) worst_comps=comps;
        if(churn>max_churn) max_churn=churn;
        for(int i=0;i<W*H;i++){ h=(h^g[i])*1099511628211ULL; prev[i]=g[i]; }
    }
    printf("hash=%016llx | worst_excess=%d worst_components=%d max_churn_per_deg=%d\n",
           (unsigned long long)h, worst_excess, worst_comps, max_churn);
    return 0;
}
