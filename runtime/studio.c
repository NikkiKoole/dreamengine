#include "studio.h"
#include "raylib.h"
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "dos_8x8_font.h"
#include "sprites_data.h"
#include "map_data.h"
#include "sound.h"

// ------------------------------------------------------------
// internal state
// ------------------------------------------------------------

#define PALETTE_SIZE 32
#define SPRITE_SIZE  16
#define SPRITE_COUNT 64   // 8×8 grid of 16×16 sprites = 128×128 sheet
#ifndef SCALE
#define SCALE 4
#endif

static Texture2D       spritesheet;
static RenderTexture2D canvas;
static Font            game_font;
static bool            custom_font = false;
static Color           palette[PALETTE_SIZE];

#define BTN_COUNT 6
static bool            btn_curr[2][BTN_COUNT];
static bool            btn_prev[2][BTN_COUNT];

static uint8_t         map_data[MAP_W * MAP_H];

// ------------------------------------------------------------
// touch state (all coordinates in window pixels unless noted)
// ------------------------------------------------------------

#ifndef TOUCH_CONTROLS_DEFAULT
#define TOUCH_CONTROLS_DEFAULT 0
#endif

static bool show_touch_ui = TOUCH_CONTROLS_DEFAULT;

#define STICK_RADIUS    60.0f
#define STICK_DEADZONE  0.35f
#define BTN_RADIUS      44

static int   stick_touch_id = -1;
static float stick_base_x = 0, stick_base_y = 0;
static float stick_knob_x = 0, stick_knob_y = 0;

static int btn_a_cx, btn_a_cy;
static int btn_b_cx, btn_b_cy;

// virtual touch pool — merges raylib touches with mouse-as-touch on desktop.
// the mouse LMB is exposed as a synthetic touch with id MOUSE_TOUCH_ID.
#define MOUSE_TOUCH_ID  (-2)
#define VT_MAX           16
static int     vt_count = 0;
static Vector2 vt_pos[VT_MAX];
static int     vt_id[VT_MAX];

// camera + clip state
static int   cam_x = 0, cam_y = 0;
static bool  clip_active = false;

// pget snapshot — refreshed at the start of every frame, so pget reads
// the previous frame's canvas (consistent state, no mid-frame RT readback)
static Image pget_snapshot     = (Image){0};
static bool  pget_snapshot_valid = false;

static void init_touch_layout(void) {
    int W = SCREEN_W * SCALE, H = SCREEN_H * SCALE;
    btn_a_cx = W -  80;  btn_a_cy = H -  80;
    btn_b_cx = W - 180;  btn_b_cy = H - 120;
}

static bool point_in_circle(float px, float py, float cx, float cy, float r) {
    float dx = px - cx, dy = py - cy;
    return dx*dx + dy*dy <= r*r;
}

static void poll_virtual_touches(void) {
    int n = GetTouchPointCount();
    if (n > VT_MAX - 1) n = VT_MAX - 1;
    for (int i = 0; i < n; i++) {
        vt_pos[i] = GetTouchPosition(i);
        vt_id[i]  = GetTouchPointId(i);
    }
    vt_count = n;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && vt_count < VT_MAX) {
        vt_pos[vt_count] = GetMousePosition();
        vt_id[vt_count]  = MOUSE_TOUCH_ID;
        vt_count++;
    }
}

static bool any_touch_in_circle(int cx, int cy, int r) {
    for (int i = 0; i < vt_count; i++) {
        if (point_in_circle(vt_pos[i].x, vt_pos[i].y, cx, cy, r)) return true;
    }
    return false;
}

static void update_stick(void) {
    if (!show_touch_ui) { stick_touch_id = -1; return; }

    int W = SCREEN_W * SCALE;

    bool still_active = false;
    if (stick_touch_id != -1) {
        for (int i = 0; i < vt_count; i++) {
            if (vt_id[i] == stick_touch_id) {
                Vector2 p = vt_pos[i];
                float dx = p.x - stick_base_x, dy = p.y - stick_base_y;
                float len = sqrtf(dx*dx + dy*dy);
                if (len > STICK_RADIUS) { dx = dx/len*STICK_RADIUS; dy = dy/len*STICK_RADIUS; }
                stick_knob_x = stick_base_x + dx;
                stick_knob_y = stick_base_y + dy;
                still_active = true;
                break;
            }
        }
    }
    if (!still_active) stick_touch_id = -1;

    if (stick_touch_id == -1) {
        for (int i = 0; i < vt_count; i++) {
            Vector2 p = vt_pos[i];
            if (p.x > W * 0.55f) continue;
            if (point_in_circle(p.x, p.y, btn_a_cx, btn_a_cy, BTN_RADIUS)) continue;
            if (point_in_circle(p.x, p.y, btn_b_cx, btn_b_cy, BTN_RADIUS)) continue;
            stick_touch_id = vt_id[i];
            stick_base_x = stick_knob_x = p.x;
            stick_base_y = stick_knob_y = p.y;
            break;
        }
    }
}

static void draw_touch_overlay(void) {
    if (!show_touch_ui) return;

    Color ring  = (Color){ 255, 255, 255,  70 };
    Color knob  = (Color){ 255, 255, 255, 160 };
    Color press = (Color){ 255, 255, 255, 110 };

    if (stick_touch_id != -1) {
        DrawCircleLines((int)stick_base_x, (int)stick_base_y, STICK_RADIUS, ring);
        DrawCircleV((Vector2){ stick_knob_x, stick_knob_y }, STICK_RADIUS * 0.45f, knob);
    } else {
        // resting hint — mirror of A's position on the bottom-left
        int hx = 80;
        int hy = SCREEN_H * SCALE - 80;
        Color hint = (Color){ 255, 255, 255, 40 };
        DrawCircleLines(hx, hy, STICK_RADIUS, hint);
        DrawCircleV((Vector2){ (float)hx, (float)hy }, STICK_RADIUS * 0.45f, hint);
    }

    bool a_down = any_touch_in_circle(btn_a_cx, btn_a_cy, BTN_RADIUS);
    bool b_down = any_touch_in_circle(btn_b_cx, btn_b_cy, BTN_RADIUS);
    DrawCircle(btn_a_cx, btn_a_cy, BTN_RADIUS, a_down ? press : ring);
    DrawCircle(btn_b_cx, btn_b_cy, BTN_RADIUS, b_down ? press : ring);
    DrawCircleLines(btn_a_cx, btn_a_cy, BTN_RADIUS, knob);
    DrawCircleLines(btn_b_cx, btn_b_cy, BTN_RADIUS, knob);

    float fs = 4 * SCALE;
    DrawTextEx(game_font, "A", (Vector2){ btn_a_cx - fs/2, btn_a_cy - fs/2 }, fs, 0, WHITE);
    DrawTextEx(game_font, "B", (Vector2){ btn_b_cx - fs/2, btn_b_cy - fs/2 }, fs, 0, WHITE);
}

// ------------------------------------------------------------
// palette — the 32-color PICO-8 palette (matches the sprite editor's pico32)
// ------------------------------------------------------------

static void load_palette() {
    // standard 16 (0-15)
    palette[0]  = (Color){ 0,   0,   0,   255 }; // CLR_BLACK          #000000
    palette[1]  = (Color){ 29,  43,  83,  255 }; // CLR_DARK_BLUE      #1d2b53
    palette[2]  = (Color){ 126, 37,  83,  255 }; // CLR_DARK_PURPLE    #7e2553
    palette[3]  = (Color){ 0,   135, 81,  255 }; // CLR_DARK_GREEN     #008751
    palette[4]  = (Color){ 171, 82,  54,  255 }; // CLR_BROWN          #ab5236
    palette[5]  = (Color){ 95,  87,  79,  255 }; // CLR_DARK_GREY      #5f574f
    palette[6]  = (Color){ 194, 195, 199, 255 }; // CLR_LIGHT_GREY     #c2c3c7
    palette[7]  = (Color){ 255, 241, 232, 255 }; // CLR_WHITE          #fff1e8
    palette[8]  = (Color){ 255, 0,   77,  255 }; // CLR_RED            #ff004d
    palette[9]  = (Color){ 255, 163, 0,   255 }; // CLR_ORANGE         #ffa300
    palette[10] = (Color){ 255, 236, 39,  255 }; // CLR_YELLOW         #ffec27
    palette[11] = (Color){ 0,   228, 54,  255 }; // CLR_GREEN          #00e436
    palette[12] = (Color){ 41,  173, 255, 255 }; // CLR_BLUE           #29adff
    palette[13] = (Color){ 131, 118, 156, 255 }; // CLR_INDIGO         #83769c
    palette[14] = (Color){ 255, 119, 168, 255 }; // CLR_PINK           #ff77a8
    palette[15] = (Color){ 255, 204, 170, 255 }; // CLR_LIGHT_PEACH    #ffccaa
    // extended 16 (16-31) — undocumented "secret" colors
    palette[16] = (Color){ 41,  24,  20,  255 }; // CLR_BROWNISH_BLACK #291814
    palette[17] = (Color){ 17,  29,  53,  255 }; // CLR_DARKER_BLUE    #111d35
    palette[18] = (Color){ 66,  33,  54,  255 }; // CLR_DARKER_PURPLE  #422136
    palette[19] = (Color){ 18,  83,  89,  255 }; // CLR_BLUE_GREEN     #125359
    palette[20] = (Color){ 116, 47,  41,  255 }; // CLR_DARK_BROWN     #742f29
    palette[21] = (Color){ 73,  51,  59,  255 }; // CLR_DARKER_GREY    #49333b
    palette[22] = (Color){ 162, 136, 121, 255 }; // CLR_MEDIUM_GREY    #a28879
    palette[23] = (Color){ 243, 239, 125, 255 }; // CLR_LIGHT_YELLOW   #f3ef7d
    palette[24] = (Color){ 190, 18,  80,  255 }; // CLR_DARK_RED       #be1250
    palette[25] = (Color){ 255, 108, 36,  255 }; // CLR_DARK_ORANGE    #ff6c24
    palette[26] = (Color){ 168, 231, 46,  255 }; // CLR_LIME_GREEN     #a8e72e
    palette[27] = (Color){ 0,   181, 67,  255 }; // CLR_MEDIUM_GREEN   #00b543
    palette[28] = (Color){ 6,   90,  181, 255 }; // CLR_TRUE_BLUE      #065ab5
    palette[29] = (Color){ 117, 70,  101, 255 }; // CLR_MAUVE          #754665
    palette[30] = (Color){ 255, 110, 89,  255 }; // CLR_DARK_PEACH     #ff6e59
    palette[31] = (Color){ 255, 157, 129, 255 }; // CLR_PEACH          #ff9d81
}

// ------------------------------------------------------------
// weak stub — user can omit update() and it still compiles
// ------------------------------------------------------------

__attribute__((weak)) void update(void) {}

// ------------------------------------------------------------
// main + runtime loop
// ------------------------------------------------------------

int main(void) {
    InitWindow(SCREEN_W * SCALE, SCREEN_H * SCALE, "dreamengine");
    InitAudioDevice();
    sound_init();
    SetTargetFPS(60);

    load_palette();
    init_touch_layout();

    if (MAP_DATA_LEN >= sizeof(map_data)) {
        memcpy(map_data, MAP_DATA, sizeof(map_data));
    } else {
        memset(map_data, 0, sizeof(map_data));
    }

    // low-res canvas — all drawing goes here, then scaled up
    canvas = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    {
        Image fontImage = LoadImageFromMemory(".png", DOS_8X8_FONT, DOS_8X8_FONT_LEN);
        game_font = LoadFontFromImage(fontImage, (Color){ 255, 255, 0, 255 }, 0);
        SetTextureFilter(game_font.texture, TEXTURE_FILTER_POINT);
        UnloadImage(fontImage);
        custom_font = true;
    }

    if (SPRITES_DATA_LEN > 0) {
        Image img = LoadImageFromMemory(".png", SPRITES_DATA, SPRITES_DATA_LEN);
        if (img.width > 0) {
            spritesheet = LoadTextureFromImage(img);
            SetTextureFilter(spritesheet, TEXTURE_FILTER_POINT);
            UnloadImage(img);
        }
    }

    while (!WindowShouldClose()) {
        poll_virtual_touches();
        update_stick();
        sound_tick(GetFrameTime());

        // snapshot last frame's canvas so pget() has stable pixels to read
        if (pget_snapshot.data) UnloadImage(pget_snapshot);
        pget_snapshot       = LoadImageFromTexture(canvas.texture);
        pget_snapshot_valid = pget_snapshot.data != NULL;
        // snapshot input edges before update so btnp() works
        for (int p = 0; p < 2; p++)
            for (int b = 0; b < BTN_COUNT; b++) {
                btn_prev[p][b] = btn_curr[p][b];
                btn_curr[p][b] = btn(p, b);
            }
        update();

        // draw into the low-res canvas
        BeginTextureMode(canvas);
            draw();
        EndTextureMode();
        if (clip_active) { EndScissorMode(); clip_active = false; }

        // scale up to the window — RenderTexture is flipped in Y
        BeginDrawing();
            DrawTexturePro(
                canvas.texture,
                (Rectangle){ 0, 0,  SCREEN_W, -SCREEN_H },
                (Rectangle){ 0, 0,  SCREEN_W * SCALE, SCREEN_H * SCALE },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );
            draw_touch_overlay();
        EndDrawing();
    }

    if (custom_font) UnloadFont(game_font);
    if (spritesheet.width > 0) UnloadTexture(spritesheet);
    if (pget_snapshot.data) UnloadImage(pget_snapshot);
    UnloadRenderTexture(canvas);
    sound_shutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

// ------------------------------------------------------------
// api implementation
// ------------------------------------------------------------

bool btn(int player, int button) {
    if (player == 0) {
        if (show_touch_ui) {
            float sx = stick_x(), sy = stick_y();
            switch (button) {
                case BTN_UP:    if (sy < -STICK_DEADZONE) return true; break;
                case BTN_DOWN:  if (sy >  STICK_DEADZONE) return true; break;
                case BTN_LEFT:  if (sx < -STICK_DEADZONE) return true; break;
                case BTN_RIGHT: if (sx >  STICK_DEADZONE) return true; break;
                case BTN_A:     if (any_touch_in_circle(btn_a_cx, btn_a_cy, BTN_RADIUS)) return true; break;
                case BTN_B:     if (any_touch_in_circle(btn_b_cx, btn_b_cy, BTN_RADIUS)) return true; break;
            }
        }
        switch (button) {
            case BTN_UP:    return IsKeyDown(KEY_W);
            case BTN_DOWN:  return IsKeyDown(KEY_S);
            case BTN_LEFT:  return IsKeyDown(KEY_A);
            case BTN_RIGHT: return IsKeyDown(KEY_D);
            case BTN_A:     return IsKeyDown(KEY_Z);
            case BTN_B:     return IsKeyDown(KEY_X);
        }
    } else if (player == 1) {
        switch (button) {
            case BTN_UP:    return IsKeyDown(KEY_UP);
            case BTN_DOWN:  return IsKeyDown(KEY_DOWN);
            case BTN_LEFT:  return IsKeyDown(KEY_LEFT);
            case BTN_RIGHT: return IsKeyDown(KEY_RIGHT);
            case BTN_A:     return IsKeyDown(KEY_COMMA);
            case BTN_B:     return IsKeyDown(KEY_PERIOD);
        }
    }
    return false;
}

bool btnp(int player, int button) {
    if (player < 0 || player > 1) return false;
    if (button < 0 || button >= BTN_COUNT) return false;
    return btn_curr[player][button] && !btn_prev[player][button];
}

// ------------------------------------------------------------
// touch + stick api
// ------------------------------------------------------------

int touch_count(void) { return vt_count; }

int touch_x(int i) {
    if (i < 0 || i >= vt_count) return -1;
    return (int)(vt_pos[i].x / SCALE);
}
int touch_y(int i) {
    if (i < 0 || i >= vt_count) return -1;
    return (int)(vt_pos[i].y / SCALE);
}

bool tap(int x, int y, int w, int h) {
    for (int i = 0; i < vt_count; i++) {
        int cx = (int)(vt_pos[i].x / SCALE), cy = (int)(vt_pos[i].y / SCALE);
        if (cx >= x && cx < x + w && cy >= y && cy < y + h) return true;
    }
    return false;
}

void touch_controls(bool on) { show_touch_ui = on; }

float stick_x(void) {
    if (stick_touch_id < 0) return 0.0f;
    return (stick_knob_x - stick_base_x) / STICK_RADIUS;
}
float stick_y(void) {
    if (stick_touch_id < 0) return 0.0f;
    return (stick_knob_y - stick_base_y) / STICK_RADIUS;
}

void cls(int color) {
    ClearBackground(palette[color % PALETTE_SIZE]);
}

void spr(int index, int x, int y) {
    sprf(index, x, y, false, false);
}

void sprf(int index, int x, int y, bool flip_x, bool flip_y) {
    if (spritesheet.width == 0) return;
    int cols = spritesheet.width / SPRITE_SIZE;
    Rectangle src = {
        .x      = (index % cols) * SPRITE_SIZE,
        .y      = (index / cols) * SPRITE_SIZE,
        .width  = flip_x ? -SPRITE_SIZE : SPRITE_SIZE,
        .height = flip_y ? -SPRITE_SIZE : SPRITE_SIZE,
    };
    Rectangle dst = { x - cam_x, y - cam_y, SPRITE_SIZE, SPRITE_SIZE };
    DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (spritesheet.width == 0) return;
    Rectangle src = { sx, sy, sw, sh };
    Rectangle dst = { dx - cam_x, dy - cam_y, dw, dh };
    DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void print(const char *text, int x, int y, int color) {
    DrawTextEx(game_font, text, (Vector2){ x - cam_x, y - cam_y }, 8, 0, palette[color % PALETTE_SIZE]);
}

void rect(int x, int y, int w, int h, int color) {
    DrawRectangleLines(x - cam_x, y - cam_y, w, h, palette[color % PALETTE_SIZE]);
}

void rectfill(int x, int y, int w, int h, int color) {
    DrawRectangle(x - cam_x, y - cam_y, w, h, palette[color % PALETTE_SIZE]);
}

void circ(int x, int y, int radius, int color) {
    DrawCircleLines(x - cam_x, y - cam_y, (float)radius, palette[color % PALETTE_SIZE]);
}

void circfill(int x, int y, int radius, int color) {
    DrawCircle(x - cam_x, y - cam_y, radius, palette[color % PALETTE_SIZE]);
}

void line(int x1, int y1, int x2, int y2, int color) {
    DrawLine(x1 - cam_x, y1 - cam_y, x2 - cam_x, y2 - cam_y, palette[color % PALETTE_SIZE]);
}

void pset(int x, int y, int color) {
    DrawPixel(x - cam_x, y - cam_y, palette[color % PALETTE_SIZE]);
}

int pget(int x, int y) {
    if (!pget_snapshot_valid) return 0;
    // pget(x,y) reads the world-space coord the cart drew at, so apply camera
    int rx = x - cam_x;
    int ry = y - cam_y;
    if (rx < 0 || rx >= SCREEN_W || ry < 0 || ry >= SCREEN_H) return 0;
    // RT data is bottom-up in raylib; flip Y to match draw coords
    Color c = GetImageColor(pget_snapshot, rx, SCREEN_H - 1 - ry);
    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (palette[i].r == c.r && palette[i].g == c.g && palette[i].b == c.b) return i;
    }
    return 0;
}

void camera(int x, int y) {
    cam_x = x;
    cam_y = y;
}

void clip(int x, int y, int w, int h) {
    if (clip_active) { EndScissorMode(); clip_active = false; }
    if (w <= 0 || h <= 0) return;
    BeginScissorMode(x, y, w, h);
    clip_active = true;
}

int mget(int cx, int cy) {
    if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) return 0;
    return map_data[cy * MAP_W + cx];
}

void mset(int cx, int cy, int n) {
    if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) return;
    map_data[cy * MAP_W + cx] = (uint8_t)(n & 0xFF);
}

void map(int cx, int cy, int sx, int sy, int cw, int ch) {
    for (int yi = 0; yi < ch; yi++) {
        for (int xi = 0; xi < cw; xi++) {
            int v = mget(cx + xi, cy + yi);
            if (v == 0) continue;  // cell 0 = empty (skipped)
            spr(v, sx + xi * SPRITE_SIZE, sy + yi * SPRITE_SIZE);
        }
    }
}

int rnd(int n) {
    if (n <= 0) return 0;
    return GetRandomValue(0, n - 1);
}

float now(void) {
    return (float)GetTime();
}

int sgn(int n) {
    return (n > 0) - (n < 0);
}

int mid(int a, int b, int c) {
    int lo = a < b ? a : b;
    int hi = a > b ? a : b;
    return c < lo ? lo : (c > hi ? hi : c);
}
