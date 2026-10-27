/*
 * main_pc.c - STRUDELFORCE Raylib 2.5D-Voxel-Frontend (AGENT 3, make pc_game)
 * 1280x720 @ 60 FPS, Core 50Hz (Accumulator), Render-Plan von voxel_render.c,
 * 32-Farb-Amiga-ECS-Palette, HUD, ESC-Live-Code-Overlay, Endlos-Biome-Wechsel.
 */
#include "sf_interfaces.h"

#include <raylib.h>
#include <raymath.h>

#include "cyber_paula.h"
#include "voxel_render.h"

#include <string.h>

#define WINDOW_W 1280
#define WINDOW_H 720
#define CORE_STEP 0.02f          /* 1/50 s */
#define MAX_PLAN_ITEMS 1024

static const int32_t palette[32][3] = {
    {    0,     0,     0}, {65536,     0,     0}, {    0, 65536,     0}, {65536, 65536,     0},
    {    0,     0, 65536}, {65536,     0, 65536}, {    0, 65536, 65536}, {65536, 65536, 65536},
    {32768,     0,     0}, {32768, 32768,     0}, {    0, 32768, 32768}, {32768,     0, 32768},
    {32768, 65536, 32768}, {65536, 32768,     0}, {32768, 65536, 65536}, {65536, 65536, 32768},
    {16384,     0,     0}, {16384, 16384,     0}, {    0, 16384, 16384}, {16384,     0, 16384},
    {16384, 32768, 16384}, {32768, 16384,     0}, {16384, 32768, 16384}, {32768, 32768, 16384},
    { 8192,     0,     0}, { 8192,  8192,     0}, {    0,  8192,  8192}, { 8192,     0,  8192},
    { 8192, 16384,  8192}, {16384,  8192,     0}, { 8192, 16384, 16384}, {16384, 16384,  8192}
};

static unsigned char u8_from_channel(int32_t v)
{
    if (v > 65535) v = 65535;
    return (unsigned char)(v >> 8);
}

static Color palette_color(int idx)
{
    if (idx < 0)  idx = 0;
    if (idx > 31) idx = 31;
    return (Color){ u8_from_channel(palette[idx][0]),
                    u8_from_channel(palette[idx][1]),
                    u8_from_channel(palette[idx][2]), 255 };
}

static uint16_t read_input_mask(void)
{
    uint16_t m = 0;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))   m |= SF_INPUT_LEFT;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))  m |= SF_INPUT_RIGHT;
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE)) m |= SF_INPUT_JUMP;
    if (IsKeyPressed(KEY_J))      m |= SF_INPUT_FIRE;
    if (IsKeyPressed(KEY_K))      m |= SF_INPUT_SECONDARY;
    if (IsKeyPressed(KEY_L))      m |= SF_INPUT_WPN_NEXT;
    if (IsKeyPressed(KEY_ESCAPE)) m |= SF_INPUT_ESCAPE;
    if (IsKeyPressed(KEY_M))      m |= SF_INPUT_MORPH;
    if (IsKeyPressed(KEY_ENTER))  m |= SF_INPUT_LEVELUP;
    return m;
}

static void render_frame(const GameState *state, const LiveCodeUI *lc)
{
    static VoxelRenderItem items[MAX_PLAN_ITEMS];
    uint16_t n = voxel_build_render_plan(state, items, MAX_PLAN_ITEMS);
    int cam_x = sf_unfix(state->player.x) - 640;
    int low_hp = (state->player.hp < 25) ? 1 : 0;
    int interlace_y = (low_hp && (state->tick & 1)) ? 1 : 0;
    int i;

    BeginDrawing();
    ClearBackground(BLACK);

    for (i = 0; i < (int)n; i++) {
        const VoxelRenderItem *it = &items[i];
        int x = sf_unfix(it->x);
        int y = sf_unfix(it->y);
        int w = sf_unfix(it->w);
        int h = sf_unfix(it->h);

        switch (it->layer) {
        case 3: /* Copper-Horizont: 1/20-Parallax */
            DrawRectangle(x + cam_x / 20, y, w, h, palette_color(it->pal));
            break;
        case 2: { /* Ruinen-Säulen: 3D-Extrusion + Lichteinfall */
            float t = 1.0f - (float)y / (float)WINDOW_H;
            float light = 0.55f + 0.45f * t;
            Color c = palette_color(it->pal);
            c.r = (unsigned char)(c.r * light);
            c.g = (unsigned char)(c.g * light);
            c.b = (unsigned char)(c.b * light);
            DrawRectangle(x, y, w, h, c);
            {
                Camera3D cam3d;
                cam3d.position   = (Vector3){ 0.0f, 0.0f, 1000.0f };
                cam3d.target     = (Vector3){ 0.0f, 0.0f, 0.0f };
                cam3d.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
                cam3d.fovy       = (float)WINDOW_H;
                cam3d.projection = CAMERA_ORTHOGRAPHIC;
                BeginMode3D(cam3d, 0);
                Draw3DRectangle((Vector3){ (float)x - 640.0f,
                                           360.0f - (float)(y + h), 0.0f },
                                (Vector3){ (float)w, (float)h, 24.0f }, c);
                EndMode3D();
            }
            break;
        }
        case 1: /* Gameplay: volles 1:1-Scroll */
            DrawRectangle(x - cam_x, y, w, h, palette_color(it->pal));
            break;
        case 0: /* Vordergrund: 1.5x-Scroll + Interlace-y */
            DrawRectangle(x - (int)(cam_x * 1.5f), y + interlace_y, w, h,
                          palette_color(it->pal));
            break;
        default:
            break;
        }
    }

    /* HUD */
    DrawText(TextFormat("HP: %d",     state->player.hp),     20,  20, 20, 1, WHITE);
    DrawText(TextFormat("SHIELD: %d", state->player.shield), 20,  44, 20, 1, SKY);
    DrawText(TextFormat("XP: %d",     state->player.xp),     20,  68, 20, 1, YELLOW);
    DrawText(TextFormat("WPN: %d",    state->player.weapon), 20,  92, 20, 1, YELLOW);
    DrawText(TextFormat("LVL: %d",    state->player.level),  20, 116, 20, 1, WHITE);

    /* ESC Live-Code-Overlay */
    if (lc != 0 && lc->open) {
        int gx, gy;
        const Color topaz = (Color){ 0, 200, 80, 255 };
        const Color grid  = (Color){ 0, 90, 45, 90 };
        DrawRectangle(0, 0, WINDOW_W, WINDOW_H, (Color){ 0, 0, 0, 170 });
        for (gx = 0; gx <= WINDOW_W; gx += 32) DrawLine(gx, 0, gx, WINDOW_H, grid);
        for (gy = 0; gy <= WINDOW_H; gy += 24) DrawLine(0, gy, WINDOW_W, gy, grid);
        DrawText(lc->source[0] ? lc->source : "", 40, 40, 20, 1, topaz);
        {
            int cx = 40 + ((int)lc->cursor % 48) * 12;
            int cy = 40 + ((int)lc->cursor / 48) * 24;
            DrawRectangle(cx, cy, 12, 22, (Color){ 0, 200, 80, 120 });
        }
        if (lc->error[0]) DrawText(lc->error, 40, 80, 20, 1, RED);
    }

    EndDrawing();
}

int main(void)
{
    GameState state;
    LiveCodeUI lc;
    float accumulator = 0.0f;

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(WINDOW_W, WINDOW_H, "STRUDELFORCE - 2.5D Voxel");
    SetTargetFPS(60);
    InitAudioDevice();
    cyber_paula_init();
    voxel_init();

    core_init(&state, SF_BIOME_CHROME_TEMPEST, 0x5EEDCAFEu);
    livecode_init(&lc, "c2 [eb2 g2]");

    while (!WindowShouldClose()) {
        accumulator += GetFrameTime();
        if (accumulator > 0.2f) accumulator = 0.2f;

        while (accumulator >= CORE_STEP) {
            core_tick(&state, read_input_mask());
            livecode_on_input(&lc, read_input_mask(), 0);

            if (lc.swap_pending) {
                cyber_paula_set_pending_pattern(&lc.compiled);
                lc.swap_pending = 0;
            }

            cyber_paula_update_from_game(&state);
            cyber_paula_tick_50hz(&state, 44100);

            accumulator -= CORE_STEP;

            if (state.tick % 1000 == 0) {
                int16_t nb = 0, co = 0;
                biome_next_chunk(&state.player.seed, &nb, &co);
                if (nb != state.biome) state.biome = nb;
            }
        }

        render_frame(&state, &lc);
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
