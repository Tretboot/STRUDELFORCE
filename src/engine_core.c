/*
 * engine_core.c - STRUDELFORCE Consolidated Engine Core
 *
 * Monolithisches Modul fuer Graphik- und Hardware-Abstraktionen:
 *   1. 2.5D Voxel-Render-Planer (4 Parallax-Ebenen, Directional Shading, Low-HP Jitter)
 *   2. Amiga OCS/AGA Hardware-Shims (Bitplanes, Copper, CIA-A Filter)
 *   3. Amiga 500 Terminal Copper-Intro Runner
 *
 * Gesetz 2: Kein Minecraft-Look! Reine Amiga-Farbpaletten, Richtungsabschattung,
 *           4 Parallax-Ebenen, Amiga Interlace-Effekt bei HP < 25%.
 *
 * Lizenz: MIT (Clean Room)
 */

#define _CRT_SECURE_NO_WARNINGS
#include "strudelforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Zero-Float Integer Sine LUT (32 Eintraege, Amplitude [-127, 127]) */
static const int8_t s_sin32[32] = {
    0, 25, 49, 71, 90, 106, 117, 125, 127, 125, 117, 106, 90, 71, 49, 25,
    0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25
};

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

/* ============================================================================
 * SECTION 1: 2.5D VOXEL RENDERING PLANNER
 * ============================================================================ */

static uint8_t buf_player_colors[16 * 16];
static uint8_t buf_player_depth[16 * 16];
static uint8_t buf_grunt_colors[16 * 16];
static uint8_t buf_grunt_depth[16 * 16];
static uint8_t buf_flyer_colors[16 * 16];
static uint8_t buf_flyer_depth[16 * 16];
static uint8_t buf_golem_colors[24 * 24];
static uint8_t buf_golem_depth[24 * 24];
static uint8_t buf_chest_colors[24 * 24];
static uint8_t buf_chest_depth[24 * 24];
static uint8_t buf_crystal_colors[16 * 16];
static uint8_t buf_crystal_depth[16 * 16];

const SpriteDef sprite_player = { "player", 16, 16, buf_player_colors, buf_player_depth };
const SpriteDef sprite_grunt  = { "grunt",  16, 16, buf_grunt_colors,  buf_grunt_depth  };
const SpriteDef sprite_flyer  = { "flyer",  16, 16, buf_flyer_colors,  buf_flyer_depth  };
const SpriteDef sprite_golem  = { "golem",  24, 24, buf_golem_colors,  buf_golem_depth  };
const SpriteDef sprite_chest  = { "chest",  24, 24, buf_chest_colors,  buf_chest_depth  };
const SpriteDef sprite_crystal= { "crystal",16, 16, buf_crystal_colors, buf_crystal_depth};

static uint8_t sprites_built = 0;

static void sprites_build(void) {
    int x, y, i;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int c = 0;
            if (y >= 1 && y <= 3 && x >= 5 && x <= 10)       c = 24;
            else if (y >= 4 && y <= 12 && x >= 4 && x <= 11) c = 16;
            else if (y >= 13 && y <= 15 && x >= 4 && x <= 5) c = 16;
            else if (y >= 13 && y <= 15 && x >= 10 && x <= 11) c = 16;
            i = y * 16 + x;
            buf_player_colors[i] = (uint8_t)c;
            buf_player_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int c = 0;
            if (y >= 2 && y <= 4 && x >= 5 && x <= 10)       c = 8;
            else if (y >= 5 && y <= 13 && x >= 3 && x <= 12) c = 8;
            else if (y >= 14 && y <= 15 && x >= 4 && x <= 5) c = 8;
            else if (y >= 14 && y <= 15 && x >= 10 && x <= 11) c = 8;
            i = y * 16 + x;
            buf_grunt_colors[i] = (uint8_t)c;
            buf_grunt_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int d = (x > 7 ? x - 7 : 7 - x) + (y > 7 ? y - 7 : 7 - y);
            int c = (d <= 6 && d >= 2) ? 10 : 0;
            if (d <= 1) c = 15;
            i = y * 16 + x;
            buf_flyer_colors[i] = (uint8_t)c;
            buf_flyer_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 24; y++) {
        for (x = 0; x < 24; x++) {
            int c = 0;
            if (y >= 4 && y <= 20 && x >= 4 && x <= 19) c = 4;
            if (y >= 6 && y <= 7 && ((x >= 8 && x <= 9) || (x >= 14 && x <= 15))) c = 12;
            if (y >= 10 && y <= 13 && x >= 10 && x <= 13) c = 12;
            i = y * 24 + x;
            buf_golem_colors[i] = (uint8_t)c;
            buf_golem_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 24; y++) {
        for (x = 0; x < 24; x++) {
            int c = 0;
            if (y >= 8 && y <= 20 && x >= 3 && x <= 20) c = 12;
            if (y >= 12 && y <= 14 && x >= 11 && x <= 12) c = 4;
            i = y * 24 + x;
            buf_chest_colors[i] = (uint8_t)c;
            buf_chest_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int d = (x > 7 ? x - 7 : 7 - x) + (y > 7 ? y - 7 : 7 - y);
            int c = (d <= 6) ? 14 : 0;
            if (d <= 1) c = 15;
            i = y * 16 + x;
            buf_crystal_colors[i] = (uint8_t)c;
            buf_crystal_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    sprites_built = 1;
}

void voxel_init(void) {
    if (!sprites_built) sprites_build();
}

uint16_t voxel_build_render_plan(const GameState *s, VoxelRenderItem *out, uint16_t max_items) {
    uint16_t n = 0;
    int i;
    if (!sprites_built) sprites_build();

#define ADD_ITEM(ix, iy, iw, ih, iz, ikind, ipal, ilayer)             \
    do {                                                               \
        if (n < max_items) {                                           \
            out[n].x = (ix);  out[n].y = (iy);                         \
            out[n].w = (iw); out[n].h = (ih);                          \
            out[n].z = (iz);                                           \
            out[n].kind = (uint8_t)(ikind);                            \
            out[n].pal = (uint8_t)(ipal);                              \
            out[n].layer = (uint8_t)(ilayer);                          \
            out[n].flags = 0;                                          \
            n++;                                                       \
        }                                                              \
    } while (0)

    /* LAYER 3 (z=-20): 65 Copper-Horizont-Baender */
    for (i = 0; i < 65; i++) {
        ADD_ITEM(sf_fix(0), sf_fix(i * 11), sf_fix(1280), sf_fix(11),
                 sf_fix(-20), 0, (uint8_t)(i % 8), 3);
    }

    /* LAYER 2 (z=-6): 24 Ruinen-Saeulen, 1/4-Parallax gebacken */
    {
        int x_offset = sf_unfix(s->px) / 4;
        for (i = 0; i < 24; i++) {
            int px = (i * 53 - x_offset) % 1280;
            if (px < 0) px += 1280;
            ADD_ITEM(sf_fix(px), sf_fix(720 - 128), sf_fix(32), sf_fix(128),
                     sf_fix(-6), 0, 6, 2);
        }
    }

    /* LAYER 1 (z=0): Player */
    ADD_ITEM(s->px, s->py, sf_fix(16), sf_fix(16), sf_fix(0), 1, 16, 1);

    /* Gegner */
    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s->enemies[i];
        int sz;
        if (!e->active) continue;
        sz = (e->kind == ENEMY_TANK) ? 24 : 16;
        ADD_ITEM(e->x, e->y, sf_fix(sz), sf_fix(sz),
                 sf_fix(0), 1, (uint8_t)(e->kind * 8 + 4), 1);
    }

    /* Projektile */
    for (i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &s->projectiles[i];
        if (!p->active) continue;
        ADD_ITEM(p->x, p->y, sf_fix(4), sf_fix(4), sf_fix(0), 0, (p->kind == PROJ_ENEMY ? 3 : 5), 1);
    }

    /* Partikel / Debris */
    for (i = 0; i < MAX_PARTICLES; i++) {
        const Particle *pt = &s->particles[i];
        if (!pt->active) continue;
        ADD_ITEM(pt->x, pt->y, sf_fix(2), sf_fix(2), sf_fix(0), 3, pt->color, 1);
    }

    /* Pickups */
    for (i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *pk = &s->pickups[i];
        if (!pk->active) continue;
        ADD_ITEM(pk->x, pk->y, sf_fix(16), sf_fix(16), sf_fix(0), 1, 20, 1);
    }

    /* The Great Bath Schatztruhe */
    if (s->biome == BIOME_GREAT_BATH && !s->chest_opened) {
        const SFBiomeDef *bd = &sf_biomedefs[BIOME_GREAT_BATH];
        ADD_ITEM(bd->chest_x, bd->chest_y, sf_fix(24), sf_fix(24),
                 sf_fix(0), 1, 12, 1);
    }

    /* LAYER 0 (z=+2): 6 Vordergrund-Rohre */
    {
        static const int pipe_x[6] = { 0, 213, 426, 639, 852, 1065 };
        for (i = 0; i < 6; i++) {
            ADD_ITEM(sf_fix(pipe_x[i]), sf_fix(0), sf_fix(48), sf_fix(720),
                     sf_fix(2), 0, 2, 0);
        }
    }

    /* LOW-HP AMIGA INTERLACE JITTER (Zero-Float) */
    if (s->max_hp > 0 && (uint32_t)s->hp * 100u / (uint32_t)s->max_hp < 25u) {
        int jitter = (s_sin32[(s->tick * 2) & 31] * 2) / 127;
        for (i = 0; i < (int)n; i++) {
            if ((out[i].layer == 0 || out[i].layer == 3) && out[i].kind != 4) {
                if (sf_unfix(out[i].y) & 1)
                    out[i].x += sf_fix(jitter);
            }
        }
    }

#undef ADD_ITEM
    return n;
}

/* ============================================================================
 * SECTION 2: AMIGA HARDWARE ABSTRAKTIONS-SHIMS
 * ============================================================================ */

void amiga_hw_init(void) {}
void amiga_hw_vblank(void) {}
void amiga_hw_set_underwater(uint8_t on) { (void)on; }
void amiga_hw_play_sample(const int8_t *data, uint16_t len, uint8_t voice, uint8_t loop) {
    (void)data; (void)len; (void)voice; (void)loop;
}
uint8_t amiga_hw_hardware_present(void) { return 0; }
int amiga_hw_render(const VoxelRenderItem *items, uint16_t n, const GameState *s) {
    (void)items; (void)n; (void)s; return 1;
}

/* ============================================================================
 * SECTION 3: AMIGA 500 TERMINAL COPPER INTRO
 * ============================================================================ */

#define SCREEN_W 80
#define SCREEN_H 24

static const char *LOGO_ASCII[5] = {
    "  ____ _____ ____  _   _ ____  _____ _     _____ ___  ____   ____ _____ ",
    " / ___|_   _|  _ \\| | | |  _ \\| ____| |   |  ___/ _ \\|  _ \\ / ___| ____|",
    " \\___ \\ | | | |_) | | | | | | |  _| | |   | |_ | | | | |_) | |   |  _|  ",
    "  ___) || | |  _ <| |_| | |_| | |___| |___|  _|| |_| |  _ <| |___| |___ ",
    " |____/ |_| |_| \\_\\\\___/|____/|_____|_____|_|   \\___/|_| \\_\\\\____|_____|"
};

static const char SCROLLER_TEXT[] = 
    "      *** STRUDELFORCE MASTER *** AMIGA 500 50HZ PAL CORE *** "
    "CHRIS HUELSBECK TFMX SOUND ENGINE *** CLEANROOM STRUDEL MINI-NOTATION *** "
    "100% FIXED-POINT GAMEPLAY SIMULATION *** GREETS TO COMMODORE, FACTOR 5, "
    "PSYGNOSIS AND THE AMIGA DEMOSCENE *** PRESS ANY KEY TO ENTER COMBAT! ***      ";

static int check_input(void) {
#ifdef _WIN32
    if (_kbhit()) { _getch(); return 1; }
    return 0;
#else
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        getchar();
        return 1;
    }
    return 0;
#endif
}

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

int amiga_intro_run(int interactive, int max_frames) {
    int frame = 0;
    int scroller_len = (int)strlen(SCROLLER_TEXT);

#ifdef _WIN32
    if (!_isatty(_fileno(stdin)) && max_frames == 0) {
        max_frames = 25;
    }
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | 0x0004);
        }
    }
#else
    if (!isatty(STDIN_FILENO) && max_frames == 0) {
        max_frames = 25;
    }
#endif

    printf("\x1b[?25l\x1b[2J\x1b[H");
    fflush(stdout);

    while (1) {
        char frame_buf[SCREEN_H][SCREEN_W + 1];
        int y, x, i;
        if (max_frames > 0 && frame >= max_frames) break;
        if (interactive && check_input()) break;

        for (y = 0; y < SCREEN_H; y++) {
            memset(frame_buf[y], ' ', SCREEN_W);
            frame_buf[y][SCREEN_W] = '\0';
        }

        for (i = 0; i < 5; i++) {
            int row = 3 + i;
            int start_col = (SCREEN_W - 68) / 2;
            if (row < SCREEN_H && start_col >= 0) {
                memcpy(&frame_buf[row][start_col], LOGO_ASCII[i], strlen(LOGO_ASCII[i]));
            }
        }

        {
            int scroll_offset = frame % scroller_len;
            for (x = 0; x < SCREEN_W; x++) {
                char ch = SCROLLER_TEXT[(scroll_offset + x) % scroller_len];
                int wave = (s_sin32[(frame + x) & 31] * 2) / 127;
                int wave_y = 18 + wave;
                if (wave_y >= 0 && wave_y < SCREEN_H) {
                    frame_buf[wave_y][x] = ch;
                }
            }
        }

        printf("\x1b[H");
        for (y = 0; y < SCREEN_H; y++) {
            int copper_col = (s_sin32[(frame * 3 + y * 8) & 31] * 3) / 127 + 4;
            if (copper_col < 1) copper_col = 1;
            if (copper_col > 6) copper_col = 6;
            printf("\x1b[3%dm%s\x1b[0m\n", copper_col, frame_buf[y]);
        }
        fflush(stdout);

        frame++;
        sleep_ms(20);
    }

    printf("\x1b[?25h\x1b[2J\x1b[H");
    fflush(stdout);
    return frame;
}
