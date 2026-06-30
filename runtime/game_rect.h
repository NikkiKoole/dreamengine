// game_rect — where the SCREEN_W×SCREEN_H canvas sits inside the window, and the ONE
// window↔canvas coordinate transform built on it. This is the chokepoint from
// docs/design/touch-controls.md "De-risking principle": every touch/mouse read and the
// present blit go through here, so responsive placement (Phase 2) is just "set game_rect to
// a different value" — the silent, library-wide coordinate rewrite is retired before the
// feature that needs it. PURE math (no raylib types), so a det-probe can test the round-trip
// off-device (tools/det-probes/placetest.c).
//
// Phase 1.5 pins game_rect to the full window (origin 0,0; scale = the native SCALE), an
// identity transform: gr_win_to_canvas reduces to wx/SCALE, byte-identical to the old code.
#ifndef GAME_RECT_H
#define GAME_RECT_H

typedef enum { PLACE_OVERLAY = 0, PLACE_DECK, PLACE_RAILS } PlaceMode;

// the canvas's on-screen origin (window px) + px-per-canvas-px. With {0,0,SCALE} the game
// fills its scaled size at the window's top-left, exactly as before placement existed.
typedef struct { float x, y, scale; } GameRect;

typedef struct {
    PlaceMode mode;
    GameRect  game;                              // where the canvas blits
    int       band_x, band_y, band_w, band_h;    // control band (window px); full window in overlay
} Placement;

// window pixel → canvas pixel (inverse of the blit). int truncation matches the old (int)(w/SCALE).
static inline int gr_win_to_canvas_x(GameRect gr, float wx) { return (int)((wx - gr.x) / gr.scale); }
static inline int gr_win_to_canvas_y(GameRect gr, float wy) { return (int)((wy - gr.y) / gr.scale); }

// canvas pixel → window pixel (the blit's forward map).
static inline float gr_canvas_to_win_x(GameRect gr, int cx) { return gr.x + cx * gr.scale; }
static inline float gr_canvas_to_win_y(GameRect gr, int cy) { return gr.y + cy * gr.scale; }

// the placement decision: given the window + canvas sizes, where does the game sit and where do
// the controls go? PURE — renders nothing, so it's deterministically testable over a size matrix.
// Phase 1.5 STUB: always a full-window overlay at the native scale (identity), so wiring
// game_rect = gr_place(...).game changes nothing. Phase 2 fills in deck/rails from the letterbox.
static inline Placement gr_place(int win_w, int win_h, int screen_w, int screen_h, float native_scale) {
    (void)screen_w; (void)screen_h;
    Placement p;
    p.mode = PLACE_OVERLAY;
    p.game.x = 0.0f; p.game.y = 0.0f; p.game.scale = native_scale;
    p.band_x = 0; p.band_y = 0; p.band_w = win_w; p.band_h = win_h;
    return p;
}

#endif
