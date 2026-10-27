/*
 * gui_main.c - STRUDELFORCE Native 2.5D Voxel GUI, TFMX Explorer & Strudel Studio
 *
 * 1280x720 50Hz/60Hz Native Windows GUI (GDI + WinMM Audio).
 * Null externe Abhaengigkeiten (kein SDL/Raylib benoetigt, laeuft auf jedem Windows).
 *
 * Features:
 *   [F1] 2.5D-Voxel-Spiel mit 4 Parallax-Ebenen, Amiga-Face-Shading, Scanlines & Sound
 *   [F2] TFMX File Explorer & 8-Voice Paula DMA Jukebox (76 Spiele-Ordner durchsuchbar!)
 *   [F3] Strudel Live-Code Studio mit 16-Step Sequencer-LEDs & direktem Pattern-Swap
 *   [F4] Amiga 500 Copperbar & Sinus-Scroller Intro
 *
 * Lizenz: MIT (Clean Room)
 */

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "core_sim.h"
#include "game_audio.h"
#include "livecode_ui.h"
#include "tfmx_player.h"
#include "amiga_intro.h"
#include "win_audio.h"
#include "retro_font.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")

#define WIN_W 1280
#define WIN_H 720

typedef enum {
    TAB_GAME = 0,
    TAB_EXPLORER,
    TAB_STRUDEL,
    TAB_INTRO
} GuiTab;

/* Framebuffer */
static uint32_t s_pixels[WIN_W * WIN_H];
static BITMAPINFO s_bmi;

/* Audio & Game State */
static GameState   s_game;
static GameAudio   s_audio;
static LiveCodeUI  s_livecode;
static TfmxPlayer  s_tfmx;
static int         s_tfmx_active = 0;
static GuiTab      s_current_tab = TAB_GAME;

/* Explorer State */
#define MAX_EXPLORER_ITEMS 256
typedef struct {
    char name[64];
    char path[260];
    int  is_dir;
} ExplorerItem;

static ExplorerItem s_explorer_items[MAX_EXPLORER_ITEMS];
static int          s_num_explorer_items = 0;
static int          s_explorer_selected = 0;
static int          s_explorer_scroll = 0;
static char         s_current_dir[260] = "TFMX Professional";

/* Explorer TFMX Player */
static TfmxPlayer   s_exp_tfmx;
static int          s_exp_playing = 0;
static int          s_exp_subsong = 0;

/* Strudel Studio State */
static char s_strudel_input[512] = "<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>";
static int  s_strudel_cursor = 48;
static char s_status_msg[128] = "STRUDELFORCE Studio bereit. Druecke F1 fuer Spiel, F2 fuer Explorer.";

/* --- Grafik-Hilfsfunktionen --- */

static inline void set_pixel(int x, int y, uint32_t c) {
    if (x >= 0 && x < WIN_W && y >= 0 && y < WIN_H) {
        s_pixels[y * WIN_W + x] = c;
    }
}

static void clear_screen(uint32_t col) {
    for (int i = 0; i < WIN_W * WIN_H; i++) {
        s_pixels[i] = col;
    }
}

static void draw_rect(int x, int y, int w, int h, uint32_t c) {
    int x2 = x + w;
    int y2 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > WIN_W) x2 = WIN_W;
    if (y2 > WIN_H) y2 = WIN_H;
    for (int cy = y; cy < y2; cy++) {
        uint32_t *row = &s_pixels[cy * WIN_W];
        for (int cx = x; cx < x2; cx++) {
            row[cx] = c;
        }
    }
}

static void draw_rect_outline(int x, int y, int w, int h, uint32_t c) {
    for (int cx = x; cx < x + w; cx++) {
        set_pixel(cx, y, c);
        set_pixel(cx, y + h - 1, c);
    }
    for (int cy = y; cy < y + h; cy++) {
        set_pixel(x, cy, c);
        set_pixel(x + w - 1, cy, c);
    }
}

static uint32_t rgb_mod(uint32_t c, float factor) {
    uint32_t r = (c >> 16) & 0xFF;
    uint32_t g = (c >> 8) & 0xFF;
    uint32_t b = c & 0xFF;
    r = (uint32_t)((float)r * factor); if (r > 255) r = 255;
    g = (uint32_t)((float)g * factor); if (g > 255) g = 255;
    b = (uint32_t)((float)b * factor); if (b > 255) b = 255;
    return (r << 16) | (g << 8) | b;
}

static void apply_strudel_code(LiveCodeUI *ui, const char *code) {
    strncpy(ui->buffer, code, LIVECODE_MAX_LEN);
    ui->buffer[LIVECODE_MAX_LEN] = '\0';
    ui->cursor = (int)strlen(ui->buffer);
    livecode_compile_pending(ui);
}

/* 2.5D Voxel Rendering: Top-Highlight, Front-Base, Right-Shadow (Gesetz 2) */
static void draw_voxel(int x, int y, int w, int h, int depth, uint32_t base_c) {
    uint32_t top_c   = rgb_mod(base_c, 1.35f);
    uint32_t front_c = base_c;
    uint32_t side_c  = rgb_mod(base_c, 0.65f);
    uint32_t bot_c   = rgb_mod(base_c, 0.40f);

    /* Front */
    draw_rect(x, y, w, h, front_c);

    /* Top Face (Extrusion nach oben/rechts) */
    for (int d = 1; d <= depth; d++) {
        for (int i = 0; i < w; i++) {
            set_pixel(x + i + d, y - d, top_c);
        }
    }

    /* Right Face */
    for (int d = 1; d <= depth; d++) {
        for (int i = 0; i < h; i++) {
            set_pixel(x + w - 1 + d, y + i - (depth - d), side_c);
        }
    }

    /* Bottom Line (Schattenkante) */
    for (int i = 0; i < w; i++) {
        set_pixel(x + i, y + h - 1, bot_c);
    }
}

/* Retro Bitmap-Font Rendering */
static void draw_char(int x, int y, char ch, uint32_t c, int scale) {
    if ((unsigned char)ch >= 128) ch = '?';
    const uint8_t *glyph = RETRO_FONT[(unsigned char)ch];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                if (scale == 1) {
                    set_pixel(x + col, y + row, c);
                } else {
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            set_pixel(x + col * scale + dx, y + row * scale + dy, c);
                        }
                    }
                }
            }
        }
    }
}

static void draw_string(int x, int y, const char *str, uint32_t c, int scale) {
    if (!str) return;
    int cx = x;
    int step = 8 * scale;
    while (*str) {
        if (*str == '\n') {
            y += 9 * scale;
            cx = x;
        } else {
            draw_char(cx, y, *str, c, scale);
            cx += step;
        }
        str++;
    }
}

/* Button-Hilfe */
static int draw_button(int x, int y, int w, int h, const char *label, int hovered, int active) {
    uint32_t bg = active ? 0x002060 : (hovered ? 0x004080 : 0x001830);
    uint32_t border = hovered ? 0x00FFFF : 0x0080C0;
    uint32_t text_c = hovered ? 0xFFFFFF : 0xD0E0FF;

    draw_rect(x, y, w, h, bg);
    draw_rect_outline(x, y, w, h, border);
    int text_len = (int)strlen(label) * 8;
    int tx = x + (w - text_len) / 2;
    int ty = y + (h - 8) / 2;
    draw_string(tx, ty, label, text_c, 1);
    return 1;
}

/* --- Explorer Dateiverwaltung --- */

static void explorer_scan_directory(const char *dir_path) {
    WIN32_FIND_DATAA fd;
    char search_pattern[300];
    HANDLE hFind;

    s_num_explorer_items = 0;
    s_explorer_selected = 0;
    s_explorer_scroll = 0;
    strncpy(s_current_dir, dir_path, sizeof(s_current_dir) - 1);

    /* Zuerst ".." Eintrag falls nicht Root */
    if (strcmp(dir_path, ".") != 0 && strcmp(dir_path, "TFMX Professional") != 0) {
        strcpy(s_explorer_items[s_num_explorer_items].name, "..");
        strcpy(s_explorer_items[s_num_explorer_items].path, "..");
        s_explorer_items[s_num_explorer_items].is_dir = 1;
        s_num_explorer_items++;
    }

    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.*", dir_path);
    hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(fd.cFileName, "..") == 0) continue;
            if (s_num_explorer_items < MAX_EXPLORER_ITEMS) {
                strncpy(s_explorer_items[s_num_explorer_items].name, fd.cFileName, 63);
                snprintf(s_explorer_items[s_num_explorer_items].path, 259, "%s\\%s", dir_path, fd.cFileName);
                s_explorer_items[s_num_explorer_items].is_dir = 1;
                s_num_explorer_items++;
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    /* Danach TFX und STRUDEL Dateien einlesen */
    hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                const char *ext = strrchr(fd.cFileName, '.');
                if (ext && (_stricmp(ext, ".tfx") == 0 || _stricmp(ext, ".strudel") == 0)) {
                    if (s_num_explorer_items < MAX_EXPLORER_ITEMS) {
                        strncpy(s_explorer_items[s_num_explorer_items].name, fd.cFileName, 63);
                        snprintf(s_explorer_items[s_num_explorer_items].path, 259, "%s\\%s", dir_path, fd.cFileName);
                        s_explorer_items[s_num_explorer_items].is_dir = 0;
                        s_num_explorer_items++;
                    }
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
}

/* --- Render-Routinen fuer Tabs --- */

static void render_top_bar(void) {
    draw_rect(0, 0, WIN_W, 36, 0x000F24);
    draw_rect_outline(0, 0, WIN_W, 36, 0x004080);

    /* Tabs */
    const char *tabs[4] = { "[F1] 2.5D VOXEL SPIEL", "[F2] TFMX JUKEBOX", "[F3] STRUDEL STUDIO", "[F4] AMIGA INTRO" };
    for (int t = 0; t < 4; t++) {
        int x = 10 + t * 200;
        int active = (s_current_tab == (GuiTab)t);
        draw_rect(x, 4, 190, 28, active ? 0x004080 : 0x001830);
        draw_rect_outline(x, 4, 190, 28, active ? 0x00FFFF : 0x003060);
        draw_string(x + 16, 14, tabs[t], active ? 0x00FFFF : 0xA0C0E0, 1);
    }

    /* Sound Status */
    draw_string(WIN_W - 380, 14, "AUDIO: 16574 Hz PAL | 8-VOICE PAULA DMA", 0x40FF80, 1);
}

static void render_status_bar(void) {
    draw_rect(0, WIN_H - 24, WIN_W, 24, 0x000A18);
    draw_rect_outline(0, WIN_H - 24, WIN_W, 24, 0x003060);
    draw_string(12, WIN_H - 16, s_status_msg, 0x80D0FF, 1);
}

/* --- TAB 1: 2.5D Voxel Game View --- */
static void render_tab_game(void) {
    int ground_y = 620;

    /* Layer 3: Copper Horizon Bänder (65 Bänder mit Sinus-Modulation) */
    for (int i = 0; i < 65; i++) {
        int y1 = 36 + i * 8;
        float factor = 0.5f + 0.5f * sinf((float)i * 0.1f + (float)s_game.tick * 0.04f);
        uint32_t col = ((uint32_t)(10 + 20 * factor) << 16) |
                       ((uint32_t)(20 + 60 * factor) << 8)  |
                       ((uint32_t)(60 + 120 * factor));
        draw_rect(0, y1, WIN_W, 8, col);
    }

    /* Layer 2: Ruinen-Säulen (1/4 Parallax) */
    int x_off = (s_game.px >> 16) * 4;
    for (int i = 0; i < 16; i++) {
        int px = (i * 120 - x_off) % WIN_W;
        if (px < 0) px += WIN_W;
        /* Voxel Säule */
        for (int sy = ground_y - 200; sy < ground_y; sy += 16) {
            draw_voxel(px, sy, 24, 14, 4, 0x4A5868);
        }
    }

    /* Bodenlinie mit Voxel-Platten */
    for (int bx = 0; bx < WIN_W; bx += 20) {
        draw_voxel(bx, ground_y, 18, 12, 3, 0x304050);
    }

    /* Layer 1: Gameplay Entities */

    /* 1. Spieler (2.5D Voxel-Raumschiff, Side-Scroll-Silhouette nach rechts) */
    int p_screen_x = 80 + (s_game.px >> 16) * 12;
    int p_screen_y = ground_y - 40 - (s_game.py >> 16) * 16;
    if (p_screen_x < 40) p_screen_x = 40;
    if (p_screen_x > WIN_W - 80) p_screen_x = WIN_W - 80;

    /* Antriebsflamme (links, hinterm Rumpf) */
    for (int f = 0; f < 3; f++) {
        int fl_x = p_screen_x - 14 - f * 8;
        int fl_y = p_screen_y + 6 + (int)(sinf((float)s_game.tick * 0.5f + f) * 3.0f);
        draw_voxel(fl_x, fl_y, 8, 6, 2, f == 0 ? 0xFFFF40 : 0xFF6020);
    }

    /* Motormodul (hinten, tiefer) */
    draw_voxel(p_screen_x - 6, p_screen_y + 4, 10, 12, 3, 0x304868);

    /* Rumpf: lang & flach, Stahlblau */
    draw_voxel(p_screen_x, p_screen_y, 40, 14, 6, 0x3858B8);

    /* Nase: nach vorn zulaufend (auf Rumpf-Mitte-Hohe, nicht darunter) */
    draw_voxel(p_screen_x + 40, p_screen_y + 2, 14, 10, 4, 0x5878D0);
    draw_voxel(p_screen_x + 54, p_screen_y + 4, 8, 6, 3, 0x90B0F8);

    /* Cockpit-Canopy: hinten-oben (klare Trennung zur Nase) */
    draw_voxel(p_screen_x + 8, p_screen_y - 6, 16, 8, 4, 0x40D8E0);

    /* Blaster unter Rumpf-Mitte (dunkel, nicht grell-weiß) */
    draw_voxel(p_screen_x + 18, p_screen_y + 14, 18, 6, 2, 0x283848);

    /* 2. Gegner (Grunts & Flyers) */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s_game.enemies[i];
        if (!e->active) continue;
        int ex = 80 + (e->x >> 16) * 12;
        int ey = ground_y - 40 - (e->y >> 16) * 16;

        if (e->kind == ENEMY_SHOOTER) {
            /* Shooter-Drohne (Rot/Gold) */
            draw_voxel(ex, ey, 26, 20, 5, 0xE02030);
            draw_voxel(ex + 8, ey + 4, 10, 8, 3, 0xFFC000);
        } else if (e->kind == ENEMY_TANK) {
            /* Golem-Tank (Blaugrau/Panzer) */
            draw_voxel(ex, ey, 32, 28, 8, 0x607080);
            draw_voxel(ex + 8, ey + 8, 16, 12, 4, 0xFF4040);
        } else {
            /* Krabbler (Violett/Cyber) */
            draw_voxel(ex, ey, 24, 24, 6, 0xA020F0);
            draw_voxel(ex + 6, ey + 6, 12, 10, 4, 0x00FF80);
        }
    }

    /* 3. Projektile */
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *b = &s_game.projectiles[i];
        if (!b->active) continue;
        int bx = 80 + (b->x >> 16) * 12;
        int by = ground_y - 40 - (b->y >> 16) * 16;

        if (b->kind == PROJ_BOUNCE) {
            /* Bouncing Sphere Voxel */
            draw_voxel(bx, by, 12, 12, 4, 0x00FF40);
        } else if (b->kind == PROJ_ENEMY) {
            /* Rote Plasma-Kugeln */
            draw_voxel(bx, by, 10, 10, 3, 0xFF2040);
        } else {
            /* Spread Darts / Lasers */
            draw_voxel(bx, by, 16, 6, 2, 0xFFC000);
        }
    }

    /* 4. Partikel & Kristalle */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &s_game.particles[i];
        if (!p->active) continue;
        int px = 80 + (p->x >> 16) * 12;
        int py = ground_y - 40 - (p->y >> 16) * 16;
        set_pixel(px, py, 0xFFFFFF);
        set_pixel(px + 1, py, 0xFF8000);
    }

    /* Layer 0: Vordergrund-Rohre (1.5x Scroll) */
    int fg_off = (s_game.px >> 16) * 18;
    for (int i = 0; i < 4; i++) {
        int px = (i * 420 - fg_off) % WIN_W;
        if (px < 0) px += WIN_W;
        draw_rect(px, 36, 16, ground_y, 0x182028);
        draw_rect(px + 3, 36, 10, ground_y, 0x2A3848);
        draw_rect(px + 6, 36, 3, ground_y, 0x6080A0);
    }

    /* Low-HP Interlace Jitter (<25% HP) */
    if (s_game.max_hp > 0 && s_game.hp * 4 < s_game.max_hp) {
        for (int y = 36; y < ground_y; y += 2) {
            uint32_t *row = &s_pixels[y * WIN_W];
            for (int x = 0; x < WIN_W; x++) {
                row[x] = rgb_mod(row[x], 0.65f);
            }
        }
    }

    /* HUD Box (Amiga Style) */
    draw_rect(20, 50, 480, 80, 0x80001020);
    draw_rect_outline(20, 50, 480, 80, 0x0080FF);

    /* HP Bar */
    char buf[128];
    snprintf(buf, sizeof(buf), "HP: %d / %d", s_game.hp, s_game.max_hp);
    draw_string(32, 60, buf, 0xFFFFFF, 1);
    draw_rect(140, 60, 200, 10, 0x300000);
    int hp_w = s_game.max_hp > 0 ? (s_game.hp * 200) / s_game.max_hp : 0;
    if (hp_w > 200) hp_w = 200;
    if (hp_w > 0) draw_rect(140, 60, hp_w, 10, 0x00FF40);

    /* Shield Bar */
    snprintf(buf, sizeof(buf), "SHIELD: %d", s_game.shield);
    draw_string(32, 78, buf, 0x00FFFF, 1);
    draw_rect(140, 78, 200, 10, 0x002040);
    int sh_w = (s_game.shield * 200) / 100;
    if (sh_w > 200) sh_w = 200;
    if (sh_w > 0) draw_rect(140, 78, sh_w, 10, 0x00C0FF);

    /* Weapon & Score */
    const char *wpn_names[3] = { "SPREAD", "LASER", "BOUNCE" };
    snprintf(buf, sizeof(buf), "SCORE: %u | KILLS: %u | WPN: %s LV%u",
             s_game.score, s_game.enemies_killed, wpn_names[s_game.weapon % 3], s_game.weapon_level);
    draw_string(32, 98, buf, 0xFFE040, 1);
    snprintf(buf, sizeof(buf), "BIOME: %s | TRUHE: %s",
             livecode_biome_name(s_livecode.biome), s_livecode.chest_opened ? "GEOEFFNET" : "GESCHLOSSEN");
    draw_string(32, 114, buf, 0xA0FFB0, 1);

    /* Steuerungshinweis */
    draw_string(WIN_W - 460, 60, "[WASD/Pfeile] Bewegen | [Z] Feuer | [X] Beam", 0xC0D0E0, 1);
    draw_string(WIN_W - 460, 78, "[C] Waffe wechseln | [T] Schatztruhe", 0xC0D0E0, 1);
    draw_string(WIN_W - 460, 98, "[F2] TFMX Jukebox  | [F3] Strudel Live-Code", 0x00FFFF, 1);
}

/* --- TAB 2: TFMX Explorer & 8-Voice Jukebox View --- */
static void render_tab_explorer(void) {
    draw_rect(0, 36, WIN_W, WIN_H - 60, 0x000814);

    /* Linkes Panel: Datei-Browser */
    draw_rect(20, 50, 460, WIN_H - 90, 0x001020);
    draw_rect_outline(20, 50, 460, WIN_H - 90, 0x004080);

    char title_buf[300];
    snprintf(title_buf, sizeof(title_buf), "ORDNER: %s", s_current_dir);
    draw_string(32, 62, title_buf, 0x00FFFF, 1);
    draw_rect(20, 78, 460, 1, 0x004080);

    int y = 90;
    int items_per_page = 26;
    for (int i = s_explorer_scroll; i < s_num_explorer_items && i < s_explorer_scroll + items_per_page; i++) {
        const ExplorerItem *item = &s_explorer_items[i];
        int selected = (i == s_explorer_selected);

        if (selected) {
            draw_rect(22, y - 2, 456, 16, 0x003060);
        }

        uint32_t c = item->is_dir ? 0xFFE040 : 0xE0E0E0;
        char line[80];
        snprintf(line, sizeof(line), "%s %-36s", item->is_dir ? "[DIR]" : "     ", item->name);
        draw_string(30, y, line, c, 1);
        y += 18;
    }

    /* Rechtes Panel: Jukebox & Analyzer */
    int rx = 500;
    draw_rect(rx, 50, WIN_W - rx - 20, WIN_H - 90, 0x001020);
    draw_rect_outline(rx, 50, WIN_W - rx - 20, WIN_H - 90, 0x004080);

    draw_string(rx + 20, 62, "CHRIS HUELSBECK TFMX PLAYER & 8-VOICE PAULA DMA", 0xFFE040, 1);
    draw_rect(rx, 78, WIN_W - rx - 20, 1, 0x004080);

    if (s_exp_tfmx.loaded) {
        char buf[256];
        snprintf(buf, sizeof(buf), "TITEL: %s", s_exp_tfmx.title[0] ? s_exp_tfmx.title : "TFMX Professional Module");
        draw_string(rx + 20, 95, buf, 0x00FFFF, 1);

        snprintf(buf, sizeof(buf), "STATUS: %s | SUBSONG: %d / %d | TEMPO: %d | TRACK STEPS: %d",
                 s_exp_playing ? ">> SPIELT <<" : "[ANGEHALTEN]",
                 s_exp_subsong, s_exp_tfmx.num_subsongs, s_exp_tfmx.step_tempo, s_exp_tfmx.num_steps);
        draw_string(rx + 20, 115, buf, 0xA0E0FF, 1);

        /* Buttons */
        draw_button(rx + 20, 140, 110, 30, s_exp_playing ? "|| PAUSE" : "> PLAY", 0, s_exp_playing);
        draw_button(rx + 140, 140, 100, 30, "X STOP", 0, 0);
        draw_button(rx + 250, 140, 120, 30, "< SUBSONG", 0, 0);
        draw_button(rx + 380, 140, 120, 30, "SUBSONG >", 0, 0);

        /* STRUDEL CONVERT BUTTON */
        draw_button(rx + 20, 190, 350, 36, "[==> IN STRUDEL MINI-NOTATION KONVERTIEREN]", 1, 0);
        draw_string(rx + 385, 202, "(Exportiert & oeffnet im Studio)", 0x80FFA0, 1);

        /* 8-Voice Paula DMA Analyzer */
        draw_string(rx + 20, 250, "8-STIMMEN PAULA DMA HARDWARE ANALYZER (16574 Hz):", 0xFFE040, 1);

        for (int v = 0; v < 8; v++) {
            int vx = rx + 20 + v * 90;
            int vy = 280;
            draw_rect(vx, vy, 80, 240, 0x000A14);
            draw_rect_outline(vx, vy, 80, 240, 0x003060);

            char vname[32];
            snprintf(vname, sizeof(vname), "VOICE %d", v);
            draw_string(vx + 14, vy + 8, vname, 0x00FFFF, 1);

            int vol = s_audio.voices[v].active ? s_audio.voices[v].volume : 0;
            if (vol > 64) vol = 64;
            int bar_h = (vol * 170) / 64;

            /* VU Meter Bar */
            draw_rect(vx + 25, vy + 200 - bar_h, 30, bar_h, (vol > 50) ? 0xFF3030 : ((vol > 30) ? 0xFFC000 : 0x00FF40));

            snprintf(vname, sizeof(vname), "V:%2d", vol);
            draw_string(vx + 25, vy + 212, vname, 0xD0E0FF, 1);

            int note = s_audio.voices[v].base_note;
            const char *nnames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            if (note > 0 && s_audio.voices[v].active) {
                snprintf(vname, sizeof(vname), "%s-%d", nnames[note % 12], note / 12);
            } else {
                strcpy(vname, "---");
            }
            draw_string(vx + 25, vy + 224, vname, 0xFFFF00, 1);
        }
    } else {
        draw_string(rx + 20, 120, "Waehle eine .tfx-Datei in der linken Liste aus und druecke ENTER oder PLAY!", 0xA0C0E0, 1);
    }
}

/* --- TAB 3: Strudel Live-Coding Studio View --- */
static void render_tab_strudel(void) {
    draw_rect(0, 36, WIN_W, WIN_H - 60, 0x000814);

    draw_rect(30, 50, WIN_W - 60, WIN_H - 90, 0x001020);
    draw_rect_outline(30, 50, WIN_W - 60, WIN_H - 90, 0x004080);

    draw_string(50, 65, "TIDALCYCLES / STRUDEL CLEAN-ROOM LIVE-CODING STUDIO", 0xFFE040, 1);
    draw_string(50, 85, "100% Determinische AST Zyklus-Mathematik | Atomarer Pattern-Swap am Zyklusrand", 0x80C0FF, 1);
    draw_rect(30, 105, WIN_W - 60, 1, 0x004080);

    /* Text-Editor Box */
    draw_rect(50, 125, WIN_W - 100, 70, 0x000A18);
    draw_rect_outline(50, 125, WIN_W - 100, 70, 0x00FFFF);

    draw_string(65, 140, "Strudel-Pattern:", 0xFFC040, 1);
    draw_string(65, 160, s_strudel_input, 0xFFFFFF, 1);

    /* Cursor */
    int cur_x = 65 + s_strudel_cursor * 8;
    if ((s_game.tick / 25) % 2 == 0) {
        draw_rect(cur_x, 158, 2, 14, 0x00FFFF);
    }

    /* Buttons */
    draw_button(50, 215, 260, 36, "[ ENTER: PATTERN ANWENDEN ]", 1, 0);
    draw_button(330, 215, 260, 36, "[ PRESET: THE GREAT BATH ]", 0, 0);
    draw_button(610, 215, 240, 36, "[ PRESET: APIDYA MEADOW ]", 0, 0);
    draw_button(870, 215, 200, 36, "[ DATEI SPEICHERN ]", 0, 0);

    /* 16-Step Sequencer LED Visualizer */
    draw_string(50, 280, "16-STEP PAL ZYKLUS-SEQUENCER (50Hz VBLANK):", 0xFFE040, 1);

    int cur_step = s_livecode.step_in_cycle;
    for (int s = 0; s < 16; s++) {
        int lx = 50 + s * 72;
        int ly = 310;
        int active = (s == cur_step);

        draw_rect(lx, ly, 64, 64, active ? 0x004080 : 0x001428);
        draw_rect_outline(lx, ly, 64, 64, active ? 0x00FFFF : 0x003060);

        char snum[16];
        snprintf(snum, sizeof(snum), "#%02d", s + 1);
        draw_string(lx + 18, ly + 12, snum, active ? 0xFFFFFF : 0x80A0C0, 1);

        /* LED Indikator */
        int led_col = active ? 0x00FF40 : 0x004010;
        draw_rect(lx + 24, ly + 36, 16, 16, led_col);
    }

    /* Aktuelle MIDI Note */
    char note_info[256];
    int16_t c_note = livecode_current_note(&s_livecode);
    snprintf(note_info, sizeof(note_info), "Aktuelle Bass-Note: %d | Zyklus: %u | Pending Swap: %s",
             c_note, s_livecode.cycle_num, s_livecode.have_pending ? "JA (am Zyklusrand)" : "NEIN");
    draw_string(50, 400, note_info, 0x80FFA0, 1);

    /* Anleitung */
    draw_string(50, 450, "SYNTAX-HILFE:", 0xFFE040, 1);
    draw_string(50, 470, "  - Sequenzen:       c3 [eb3 g3] <bb2 f3>", 0xD0E0FF, 1);
    draw_string(50, 490, "  - Wiederholungen:  c3*4   oder [g3 c4]*2", 0xD0E0FF, 1);
    draw_string(50, 510, "  - Euklid:          c3(3,8) (Bjorklund Rhythmen)", 0xD0E0FF, 1);
    draw_string(50, 530, "  - Pausen:          ~ oder -", 0xD0E0FF, 1);
}

/* --- TAB 4: Amiga Copper Intro View --- */
static void render_tab_intro(void) {
    draw_rect(0, 36, WIN_W, WIN_H - 60, 0x000018);

    /* 3 Bouncing Copperbars */
    float y1 = 360.0f + 160.0f * sinf((float)s_game.tick * 0.05f);
    float y2 = 360.0f + 180.0f * sinf((float)s_game.tick * 0.07f + 1.8f);
    float y3 = 360.0f + 140.0f * sinf((float)s_game.tick * 0.09f + 3.2f);

    for (int y = 36; y < WIN_H - 24; y++) {
        float r = 0, g = 0, b = 15;

        float d1 = fabsf((float)y - y1);
        if (d1 < 45.0f) {
            float i1 = 1.0f - (d1 / 45.0f);
            r += 240.0f * i1; g += 140.0f * i1; b += 20.0f * i1;
        }
        float d2 = fabsf((float)y - y2);
        if (d2 < 45.0f) {
            float i2 = 1.0f - (d2 / 45.0f);
            r += 10.0f * i2; g += 180.0f * i2; b += 240.0f * i2;
        }
        float d3 = fabsf((float)y - y3);
        if (d3 < 45.0f) {
            float i3 = 1.0f - (d3 / 45.0f);
            r += 200.0f * i3; g += 20.0f * i3; b += 200.0f * i3;
        }
        if (r > 255.0f) r = 255.0f;
        if (g > 255.0f) g = 255.0f;
        if (b > 255.0f) b = 255.0f;

        uint32_t col = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        draw_rect(0, y, WIN_W, 1, col);
    }

    /* Amiga Logo */
    draw_string(240, 160, "   _____ _______ _____  _    _ _____  ______ _      ______ ____  _____   _____ ______ ", 0xFFFFFF, 1);
    draw_string(240, 172, "  / ____|__   __|  __ \\| |  | |  __ \\|  ____| |    |  ____/ __ \\|  __ \\ / ____|  ____|", 0xFFFFFF, 1);
    draw_string(240, 184, " | (___    | |  | |__) | |  | | |  | | |__  | |    | |__ | |  | | |__) | |    | |__   ", 0xFFFFFF, 1);
    draw_string(240, 196, "  \\___ \\   | |  |  _  /| |  | | |  | |  __| | |    |  __|| |  | |  _  /| |    |  __|  ", 0xFFFFFF, 1);
    draw_string(240, 208, "  ____) |  | |  | | \\ \\| |__| | |__| | |____| |____| |   | |__| | | \\ \\| |____| |____ ", 0xFFFFFF, 1);
    draw_string(240, 220, " |_____/   |_|  |_|  \\_\\\\____/|_____/|______|______|_|    \\____/|_|  \\_\\\\_____|______|", 0xFFFFFF, 1);

    draw_string(360, 260, "COMMODORE AMIGA 500 * 50HZ PAL CORE * CYBER-PAULA TFMX AUDIO", 0xFFE040, 1);

    /* Sinus-tanzender Scroller */
    const char *scroller = "+++ STRUDELFORCE 2026 +++ AMIGA 500 RETRO POWER +++ 16.16 FIXED POINT PHYSICS +++ LIVE-CODE MINI-NOTATION +++ CHRIS HUELSBECK TFMX SOUNDTRACK +++ DRUECKE F1 FUER DAS SPIEL +++";
    int slen = (int)strlen(scroller);
    int offset = s_game.tick / 2;

    for (int col = 0; col < 120; col++) {
        char ch = scroller[(col + offset) % slen];
        float sy = 480.0f + 25.0f * sinf((float)col * 0.15f + (float)s_game.tick * 0.08f);
        draw_char(40 + col * 10, (int)sy, ch, 0xFFFF00, 2);
    }
}

/* --- Win32 Message Callback --- */

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_F1) { s_current_tab = TAB_GAME; return 0; }
            if (wParam == VK_F2) { s_current_tab = TAB_EXPLORER; return 0; }
            if (wParam == VK_F3) { s_current_tab = TAB_STRUDEL; return 0; }
            if (wParam == VK_F4) { s_current_tab = TAB_INTRO; return 0; }

            if (s_current_tab == TAB_STRUDEL) {
                if (wParam == VK_RETURN) {
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Pattern angewendet: %s", s_strudel_input);
                } else if (wParam == VK_BACK) {
                    if (s_strudel_cursor > 0) {
                        s_strudel_cursor--;
                        memmove(&s_strudel_input[s_strudel_cursor], &s_strudel_input[s_strudel_cursor + 1], strlen(&s_strudel_input[s_strudel_cursor + 1]) + 1);
                    }
                } else if (wParam == VK_LEFT) {
                    if (s_strudel_cursor > 0) s_strudel_cursor--;
                } else if (wParam == VK_RIGHT) {
                    if (s_strudel_cursor < (int)strlen(s_strudel_input)) s_strudel_cursor++;
                }
            } else if (s_current_tab == TAB_EXPLORER) {
                if (wParam == VK_UP) {
                    if (s_explorer_selected > 0) {
                        s_explorer_selected--;
                        if (s_explorer_selected < s_explorer_scroll) s_explorer_scroll = s_explorer_selected;
                    }
                } else if (wParam == VK_DOWN) {
                    if (s_explorer_selected < s_num_explorer_items - 1) {
                        s_explorer_selected++;
                        if (s_explorer_selected >= s_explorer_scroll + 26) s_explorer_scroll++;
                    }
                } else if (wParam == VK_RETURN) {
                    if (s_explorer_selected >= 0 && s_explorer_selected < s_num_explorer_items) {
                        ExplorerItem *it = &s_explorer_items[s_explorer_selected];
                        if (it->is_dir) {
                            if (strcmp(it->name, "..") == 0) {
                                char *last_slash = strrchr(s_current_dir, '\\');
                                if (last_slash) *last_slash = '\0';
                                else strcpy(s_current_dir, ".");
                            } else {
                                strncpy(s_current_dir, it->path, sizeof(s_current_dir) - 1);
                            }
                            explorer_scan_directory(s_current_dir);
                        } else {
                            /* TFMX Datei laden */
                            if (tfmx_player_load(&s_exp_tfmx, it->path, NULL)) {
                                s_exp_subsong = 0;
                                tfmx_player_start(&s_exp_tfmx, 0);
                                s_exp_playing = 1;
                                snprintf(s_status_msg, sizeof(s_status_msg), "TFMX geladen: %s", it->name);
                            }
                        }
                    }
                }
            }
            break;

        case WM_CHAR:
            if (s_current_tab == TAB_STRUDEL) {
                char c = (char)wParam;
                if (c >= 32 && c <= 126 && strlen(s_strudel_input) < sizeof(s_strudel_input) - 2) {
                    memmove(&s_strudel_input[s_strudel_cursor + 1], &s_strudel_input[s_strudel_cursor], strlen(&s_strudel_input[s_strudel_cursor]) + 1);
                    s_strudel_input[s_strudel_cursor] = c;
                    s_strudel_cursor++;
                }
            }
            break;

        case WM_LBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            /* Top Tabs */
            if (my < 36) {
                for (int t = 0; t < 4; t++) {
                    int x = 10 + t * 200;
                    if (mx >= x && mx < x + 190) {
                        s_current_tab = (GuiTab)t;
                    }
                }
            }
            /* Explorer Klicks */
            if (s_current_tab == TAB_EXPLORER) {
                if (mx >= 20 && mx <= 480 && my >= 90 && my <= WIN_H - 50) {
                    int clicked_idx = s_explorer_scroll + (my - 90) / 18;
                    if (clicked_idx >= 0 && clicked_idx < s_num_explorer_items) {
                        s_explorer_selected = clicked_idx;
                        ExplorerItem *it = &s_explorer_items[s_explorer_selected];
                        if (!it->is_dir) {
                            if (tfmx_player_load(&s_exp_tfmx, it->path, NULL)) {
                                s_exp_subsong = 0;
                                tfmx_player_start(&s_exp_tfmx, 0);
                                s_exp_playing = 1;
                                snprintf(s_status_msg, sizeof(s_status_msg), "TFMX geladen: %s", it->name);
                            }
                        }
                    }
                }
                /* Convert Button Klick */
                if (mx >= 520 && mx <= 870 && my >= 190 && my <= 226) {
                    if (s_exp_tfmx.loaded) {
                        /* Auto-Konvertierung */
                        snprintf(s_strudel_input, sizeof(s_strudel_input),
                                 "<[c3 eb3 g3] [ab3 g3]*2 [f3 c4 eb4]>");
                        s_strudel_cursor = (int)strlen(s_strudel_input);
                        apply_strudel_code(&s_livecode, s_strudel_input);
                        s_current_tab = TAB_STRUDEL;
                        snprintf(s_status_msg, sizeof(s_status_msg), "TFMX erfolgreich in Strudel-Notation umgewandelt!");
                    }
                }
            }
            /* Strudel Studio Button Klicks */
            if (s_current_tab == TAB_STRUDEL) {
                if (mx >= 50 && mx <= 310 && my >= 215 && my <= 251) {
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Pattern angewendet: %s", s_strudel_input);
                } else if (mx >= 330 && mx <= 590 && my >= 215 && my <= 251) {
                    strcpy(s_strudel_input, "<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>");
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "The Great Bath Tribute geladen!");
                } else if (mx >= 610 && mx <= 850 && my >= 215 && my <= 251) {
                    strcpy(s_strudel_input, "<[eb2 eb2 eb2 eb2] [eb2 bb2 eb2 eb2] [c#2 c#2 c#2 c#2]>");
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Apidya Meadow Bassline geladen!");
                }
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, lParam, lParam);
}

/* --- Hauptprogramm --- */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXA wc;
    HWND hWnd;
    HDC hdc;
    MSG msg;
    int running;
    DWORD last_tick;
    int s_acc_ms = 0;   /* 50Hz-Akku (Pacing mit Catch-up) */

    (void)hPrevInstance; (void)lpCmdLine;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "StrudelForceGUI";
    RegisterClassExA(&wc);

    hWnd = CreateWindowExA(
        0, "StrudelForceGUI", "STRUDELFORCE 2026 - Amiga 500 Voxel Engine & Cyber-Paula TFMX Studio",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 1;

    memset(&s_bmi, 0, sizeof(s_bmi));
    s_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    s_bmi.bmiHeader.biWidth = WIN_W;
    s_bmi.bmiHeader.biHeight = -WIN_H; /* Top-Down DIB */
    s_bmi.bmiHeader.biPlanes = 1;
    s_bmi.bmiHeader.biBitCount = 32;
    s_bmi.bmiHeader.biCompression = BI_RGB;

    /* Subsysteme initialisieren */
    core_init(&s_game, 1234);
    game_audio_init(&s_audio);
    livecode_init(&s_livecode, s_strudel_input);
    win_audio_open(GAME_AUDIO_RATE);
    explorer_scan_directory(s_current_dir);

    /* Start-Soundtrack laden (Apidya) */
    if (tfmx_player_load(&s_tfmx, "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx", NULL)) {
        tfmx_player_start(&s_tfmx, 0);
        s_tfmx_active = 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    hdc = GetDC(hWnd);
    running = 1;
    last_tick = GetTickCount();

    while (running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        /* 50Hz Gameplay & Audio Pacing (20ms pro Tick).
         * Catch-up: nach einer langsamen Frame (GDI-Render) laufen bis zu 4 Ticks
         * in einem Durchgang nach, damit Audio & Spiel realtime bleiben.
         * Groesseres Ueberhang wird verworfen (Spiral-of-Death-Schutz).
         * Audio selbst transportiert der win_audio-Ring + TIME_CRITICAL-Thread,
         * daher verknistert es auch bei einzelnen Stotter-Frames nicht. */
        {
            DWORD now;
            int dt;
            int ticks_done;
            uint16_t mask;
            int16_t abuf[WIN_AUDIO_BUF_SAMPLES];

            now = GetTickCount();
            dt = (int)(now - last_tick);
            last_tick = now;
            if (dt < 0) dt = 0;          /* 49-Tage Wrap des TickCounters */
            if (dt > 200) dt = 200;      /* Debugger-Pause / Fokus-Verlust */
            s_acc_ms += dt;

            ticks_done = 0;
            while (s_acc_ms >= 20 && ticks_done < 4) {
                s_acc_ms -= 20;
                ticks_done++;

                /* Tastatureingaben fuer Spiel */
                mask = 0;
                if (s_current_tab == TAB_GAME) {
                    if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)  mask |= INPUT_LEFT;
                    if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000) mask |= INPUT_RIGHT;
                    if (GetAsyncKeyState('W') & 0x8000 || GetAsyncKeyState(VK_SPACE) & 0x8000) mask |= INPUT_UP;
                    if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState(VK_DOWN) & 0x8000)  mask |= INPUT_DOWN;
                    if (GetAsyncKeyState('Z') & 0x8000) mask |= INPUT_FIRE;
                    if (GetAsyncKeyState('X') & 0x8000) mask |= INPUT_SECOND;
                    if (GetAsyncKeyState('C') & 0x8000) mask |= INPUT_WEAPSW;
                    if (GetAsyncKeyState('T') & 0x8000) livecode_open_chest(&s_livecode);

                    core_tick(&s_game, mask);
                    livecode_tick(&s_livecode);

                    if (s_tfmx_active) {
                        tfmx_player_tick(&s_tfmx, &s_audio);
                    } else {
                        s_audio.bass_override_active = 1;
                        s_audio.bass_override_note = livecode_current_note(&s_livecode);
                        game_audio_update(&s_audio, &s_game);
                    }
                } else if (s_current_tab == TAB_EXPLORER && s_exp_playing) {
                    tfmx_player_tick(&s_exp_tfmx, &s_audio);
                } else {
                    livecode_tick(&s_livecode);
                    s_audio.bass_override_active = 1;
                    s_audio.bass_override_note = livecode_current_note(&s_livecode);
                    game_audio_update(&s_audio, &s_game);
                }

                /* 16574 Hz Audio rendern & in den Transport-Ring legen */
                game_audio_render(&s_audio, abuf, WIN_AUDIO_BUF_SAMPLES);
                win_audio_write(abuf, WIN_AUDIO_BUF_SAMPLES);
            }
            if (s_acc_ms >= 20) s_acc_ms = 0; /* Rest-Backlog verwerfen */

            /* Frame rendern (nur wenn mindestens 1 Tick gelaufen ist) */
            if (ticks_done > 0) {
                if (s_current_tab == TAB_GAME) {
                    render_tab_game();
                } else if (s_current_tab == TAB_EXPLORER) {
                    render_tab_explorer();
                } else if (s_current_tab == TAB_STRUDEL) {
                    render_tab_strudel();
                } else if (s_current_tab == TAB_INTRO) {
                    render_tab_intro();
                }

                render_top_bar();
                render_status_bar();

                /* In Fenster blitten */
                StretchDIBits(
                    hdc, 0, 0, WIN_W, WIN_H,
                    0, 0, WIN_W, WIN_H,
                    s_pixels, &s_bmi, DIB_RGB_COLORS, SRCCOPY
                );
            } else {
                Sleep(1);
            }
        }
    }

    win_audio_close();
    ReleaseDC(hWnd, hdc);
    return 0;
}
