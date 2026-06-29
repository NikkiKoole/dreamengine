// headless-nr.c — drive the DE_NO_RAYLIB engine with no OS, dump a frame.
//
// Proves the portable software path (studio.c + raylib_compat.c stubs + baked
// fonts) renders on desktop WITHOUT Raylib — so we can diff it against the
// Raylib build before the iOS shell exists. The iOS CanvasView does exactly
// what this does: de_init → de_frame(t) per tick → blit de_framebuffer().
//
//   clang -DDE_NO_RAYLIB -DSW_CANVAS_DEFAULT=1 -DSCREEN_W=.. -DSCREEN_H=.. \
//     tools/headless-nr.c runtime/studio.c runtime/raylib_compat.c <cart>.c \
//     -I runtime -I build -lm -o build/headless-nr
//   build/headless-nr <frames> <out.ppm>

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int frames = (argc > 1) ? atoi(argv[1]) : 1;
    const char *out = (argc > 2) ? argv[2] : "build/headless_nr.ppm";

    de_init(DE_RENDERER_SOFTWARE);
    for (int i = 0; i < frames; i++) de_frame((double)i / 60.0);

    const uint32_t *fb = de_framebuffer();
    int w = de_screen_w(), h = de_screen_h();
    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);              // binary PPM (RGB)
    // sw_cbuf is bottom-up (row 0 = bottom); the Raylib present flips it with a
    // negative source rect, and the iOS blit must too. Flip here for a normal image.
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++) {
            uint32_t px = fb[y * w + x];               // RGBA8888
            unsigned char rgb[3] = { (unsigned char)(px & 0xff),
                                     (unsigned char)((px >> 8) & 0xff),
                                     (unsigned char)((px >> 16) & 0xff) };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    fprintf(stderr, "headless-nr: %d frame(s), %dx%d -> %s\n", frames, w, h, out);
    return 0;
}
