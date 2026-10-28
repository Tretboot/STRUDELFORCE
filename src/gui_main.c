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
#include <xinput.h>   /* Controller-Support (XInput, Gamepad 0) */

#include "core_sim.h"
#include "game_audio.h"
#include "livecode_ui.h"
#include "tfmx_player.h"
#include "mod_player.h"
#include "amiga_intro.h"
#include "win_audio.h"
#include "soundgodizer.h"
#include "retro_font.h"
#include "embedded_music.h"

#define GAME_VERSION "0.1"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xinput.lib")

#define WIN_W 1280
#define WIN_H 720

typedef enum {
    TAB_GAME = 0,   /* nur noch als Referenz (GUI_STATE_GAME ersetzt den Tab) */
    TAB_EXPLORER,
    TAB_STRUDEL,
    TAB_INTRO
} GuiTab;

/* ================= GUI State Machine =================
 * MENU -> LOADING -> GAME -> (GAMEOVER -> LOADING -> GAME | MENU)
 * STUDIO = F1-F4-Tools (Jukebox/Strudel/Intro), pausiert das Spiel.
 * CREDITS = scrollender Abspann. ESC fuehrt immer zurueck zum MENU. */
typedef enum {
    GUI_STATE_MENU = 0,
    GUI_STATE_LOADING,
    GUI_STATE_GAME,
    GUI_STATE_GAMEOVER,
    GUI_STATE_STUDIO,
    GUI_STATE_CREDITS
} GuiState;

static GuiState s_gui_state = GUI_STATE_MENU;
static int s_menu_index = 0;        /* aktiver Hauptmenue-Punkt */
static int s_active_mode = MODE_ENDLESS; /* letzter gewaehlter Modus (F1/ENTER-Neustart) */
static int s_pad_connected = 0;      /* XInput Gamepad 0 verbunden (Footer-Anzeige) */
static int s_pad_start_prev = 0;     /* XInput Start-Button (Edge-Erkennung) */
static int s_loading_ticks = 0;     /* Loading-Screen-Fortschritt (50Hz-Ticks) */

#define MENU_ITEM_COUNT 6
static const char *menu_labels[MENU_ITEM_COUNT] = {
    "KAMPAGNE (5 LEVELS)",
    "ENDLOS ROGUELIKE",
    "TFMX JUKEBOX",
    "STRUDEL STUDIO",
    "AMIGA INTRO",
    "CREDITS"
};

#define GAME_SEED 1234u              /* fixer Seed: deterministische Läufe */
#define LOAD_DURATION 120            /* ~2.4 Sekunden Loading-Screen */

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
static int         s_gui_frame = 0; /* GUI-Frame-Zaehler: animiert Tabs, deren
                                        Quelle (s_game.tick) nur im Game-Tab laeuft */

/* --- Soundgodizer (F5: aus -> BASS -> WARM -> OVERDRIVE) --- */
static Soundgodizer s_god;
static int          s_god_mode = 0;
static int16_t      s_god_buf[WIN_AUDIO_BUF_SAMPLES * 2]; /* Stereo-Ausgabe */

/* --- Jukebox: zufaelliger Level-Song (TFMX + MOD) pro Wave --- */
#define PLAYLIST_MAX 512
typedef struct { char path[260]; char name[64]; int is_mod; } PlaylistEntry;
static PlaylistEntry s_playlist[PLAYLIST_MAX];
static int           s_playlist_count = 0;
static uint32_t      s_music_rng = 0x5EED5EEDu; /* eigener LCG (Core-RNG bleibt unberuehrt) */
static uint16_t      s_last_wave = 0;
static ModPlayer     s_mod;        /* Game-Tab MOD-Player */
static uint8_t       s_mod_buf[128 * 1024];
static int           s_mod_active = 0;

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
static char         s_exp_path_buf[260];   /* freier Pfadeingabe-Buffer (Explorer, TASTE+EINGABE) */
static int          s_exp_path_cur = 0;

/* Explorer TFMX Player */
static TfmxPlayer   s_exp_tfmx;
static int          s_exp_playing = 0;
static int          s_exp_subsong = 0;
/* Explorer MOD Player */
static ModPlayer    s_exp_mod;
static uint8_t      s_exp_mod_buf[128 * 1024];
static int          s_exp_mod_playing = 0;

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
                if (ext && (_stricmp(ext, ".tfx") == 0 || _stricmp(ext, ".strudel") == 0 || _stricmp(ext, ".mod") == 0)) {
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

/* --- Jukebox: Playlist (rekursiv) + Level-Song-Switching + Soundgodizer --- */

static void playlist_scan_rec(const char *dir, int depth) {
    WIN32_FIND_DATAA fd;
    char pattern[300];
    HANDLE hFind;

    if (depth > 3 || s_playlist_count >= PLAYLIST_MAX) return;
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char sub[300];
            if (depth < 3) {
                snprintf(sub, sizeof(sub), "%s\\%s", dir, fd.cFileName);
                playlist_scan_rec(sub, depth + 1);
            }
        } else if (s_playlist_count < PLAYLIST_MAX) {
            const char *ext = strrchr(fd.cFileName, '.');
            if (ext) {
                int is_mod = (_stricmp(ext, ".mod") == 0);
                int is_tfx = (_stricmp(ext, ".tfx") == 0);
                if (is_mod || is_tfx) {
                    strncpy(s_playlist[s_playlist_count].name, fd.cFileName, 63);
                    s_playlist[s_playlist_count].name[63] = '\0';
                    snprintf(s_playlist[s_playlist_count].path, 259, "%s\\%s", dir, fd.cFileName);
                    s_playlist[s_playlist_count].is_mod = is_mod;
                    s_playlist_count++;
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

/* Lade Track idx und starte ihn (exakt ein Player aktiv). */
static void music_load_index(int idx) {
    PlaylistEntry *e;
    char msg[180];

    if (idx < 0 || idx >= s_playlist_count) return;
    e = &s_playlist[idx];

    if (e->is_mod) {
        FILE *f = fopen(e->path, "rb");
        long size = 0;
        s_tfmx_active = 0;
        s_mod_active = 0;
        if (f) {
            fseek(f, 0, SEEK_END);
            size = ftell(f);
            if (size > (long)sizeof(s_mod_buf)) size = (long)sizeof(s_mod_buf);
            fseek(f, 0, SEEK_SET);
            (void)fread(s_mod_buf, 1, (size_t)size, f);
            fclose(f);
        }
        if (size > 1084 && mod_player_load_mem(&s_mod, s_mod_buf, (uint32_t)size)) {
            mod_player_start(&s_mod, GAME_AUDIO_RATE);
            s_mod_active = 1;
        }
        snprintf(msg, sizeof(msg), "WAVE %u: [MOD] %s", (unsigned)s_last_wave, e->name);
    } else {
        s_mod_active = 0;
        s_tfmx_active = 0;
        if (tfmx_player_load(&s_tfmx, e->path, NULL)) {
            tfmx_player_start(&s_tfmx, 0);
            s_tfmx_active = 1;
        }
        snprintf(msg, sizeof(msg), "WAVE %u: [TFMX] %s", (unsigned)s_last_wave, e->name);
    }
    snprintf(s_status_msg, sizeof(s_status_msg), "%s", msg);
}

/* In-EXE-Tracks (ohne Disk) — Intro + Jukebox-Fallback (src/embedded_music.h, generiert). */
static void music_play_embedded(int kind) {
    s_mod_active = 0;
    s_tfmx_active = 0;
    if (kind == 0) {
        /* Intro: Apidya Meadow — TFMX Professional mit den ORIGINALEN Amiga-Samples (.sam) */
        if (tfmx_player_load_mem(&s_tfmx, EMB_TFMX_APIDYA_MEADOW, EMB_TFMX_APIDYA_MEADOW_LEN,
                                 (const int8_t *)EMB_SAM_APIDYA_MEADOW, EMB_SAM_APIDYA_MEADOW_LEN)) {
            tfmx_player_start(&s_tfmx, 0);
            s_tfmx_active = 1;
            snprintf(s_status_msg, sizeof(s_status_msg), "INTRO: %s (in EXE, Original-Samples)", EMB_TFMX_APIDYA_MEADOW_NAME);
        }
    } else if (kind == 1) {
        if (mod_player_load_mem(&s_mod, EMB_MOD_WC_REGULARCOMBAT, EMB_MOD_WC_REGULARCOMBAT_LEN)) {
            mod_player_start(&s_mod, GAME_AUDIO_RATE);
            s_mod_active = 1;
            snprintf(s_status_msg, sizeof(s_status_msg), "MUSIK: %s (in EXE)", EMB_MOD_WC_REGULARCOMBAT_NAME);
        }
    } else if (kind == 2) {
        if (mod_player_load_mem(&s_mod, EMB_MOD_WC_OVERALLVICTORY, EMB_MOD_WC_OVERALLVICTORY_LEN)) {
            mod_player_start(&s_mod, GAME_AUDIO_RATE);
            s_mod_active = 1;
            snprintf(s_status_msg, sizeof(s_status_msg), "MUSIK: %s (in EXE)", EMB_MOD_WC_OVERALLVICTORY_NAME);
        }
    }
}

/* Deterministischer Zufalls-Song aus der Library (eigener LCG, kein Core-RNG). */
static void music_play_next(void) {
    uint32_t seed;
    int idx;

    if (s_playlist_count <= 0) {
        /* Fallback (leere Library): in-EXE-Tracks, deterministisch pro Wave rotierend */
        music_play_embedded((int)(s_last_wave % 3));
        return;
    }
    seed = 0x5EED5EEDu + (uint32_t)s_last_wave * 2654435761u;
    s_music_rng = seed * 1664525u + 1013904223u;
    s_music_rng = s_music_rng * 1664525u + 1013904223u; /* eine Runde waermen */
    idx = (int)(s_music_rng % (uint32_t)s_playlist_count);
    music_load_index(idx);
}

/* Soundgodizer-Presets (F5 schaltet durch: AUS -> BASS -> WARM -> OVERDRIVE) */
static void god_toggle(void) {
    char msg[128];

    s_god_mode = (s_god_mode + 1) % 4;
    switch (s_god_mode) {
        case 0: god_set_params(&s_god, 0, 0, 0, 0);   s_god.enabled = 0; break;
        case 1: god_set_params(&s_god, 60, 0, 0, 0);   s_god.enabled = 1; break;
        case 2: god_set_params(&s_god, 40, 55, 0, 0);  s_god.enabled = 1; break;
        default: god_set_params(&s_god, 50, 15, 45, 55); s_god.enabled = 1; break;
    }
    god_reset_state(&s_god);
    snprintf(msg, sizeof(msg), "SOUNDGODIZER: %s [F5 schaltet durch]",
             s_god_mode == 0 ? "AUS" :
             s_god_mode == 1 ? "BASS-BOOST" :
             s_god_mode == 2 ? "WARM (Analog-LP)" : "HQ OVERDRIVE + WIDE");
    snprintf(s_status_msg, sizeof(s_status_msg), "%s", msg);
}

static const char *god_mode_name(void) {
    if (s_god_mode == 1) return "BASS";
    if (s_god_mode == 2) return "WARM";
    if (s_god_mode == 3) return "ODRIVE";
    return "AUS";
}

/* --- Game-Flow (Menue -> Loading -> Spiel) --- */

static void start_game(int mode) {
    core_init(&s_game, GAME_SEED);
    s_game.mode = (uint8_t)mode;         /* MODE_ENDLESS (Default) oder MODE_CAMPAIGN */
    s_game.campaign_level = 1;
    s_active_mode = mode;
    s_pad_start_prev = 0;
    s_mod_active = 0;
    s_tfmx_active = 0;
    s_last_wave = s_game.wave;          /* = 0, Song wechselt bei Wave 1 */
    music_play_next();                  /* zufaelliger Track aus der Library */
    s_loading_ticks = 0;
    s_gui_state = GUI_STATE_LOADING;
}

static void menu_activate(void) {
    switch (s_menu_index) {
        case 0: start_game(MODE_CAMPAIGN); break;
        case 1: start_game(MODE_ENDLESS);  break;
        case 2: s_current_tab = TAB_EXPLORER; s_gui_state = GUI_STATE_STUDIO; break;
        case 3: s_current_tab = TAB_STRUDEL;  s_gui_state = GUI_STATE_STUDIO; break;
        case 4: s_current_tab = TAB_INTRO;    s_gui_state = GUI_STATE_STUDIO; break;
        default: s_gui_state = GUI_STATE_CREDITS; break;
    }
}

/* --- XInput: Gamepad 0 im Hauptmenue navigieren (Edge-Getriggerung) --- */
static void pad_menu_poll(void) {
    XINPUT_STATE xs;
    static int prev_up, prev_down, prev_a;
    int up, down, a;
    if (s_gui_state != GUI_STATE_MENU) return;
    if (XInputGetState(0, &xs) != ERROR_SUCCESS) { s_pad_connected = 0; return; }
    s_pad_connected = 1;
    up   = ((xs.Gamepad.wButtons & (XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_LEFT_SHOULDER)) != 0) || xs.Gamepad.sThumbLY > 4000;
    down = ((xs.Gamepad.wButtons & (XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_RIGHT_SHOULDER)) != 0) || xs.Gamepad.sThumbLY < -4000;
    a    = (xs.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
    if (up && !prev_up)
        s_menu_index = (s_menu_index + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
    if (down && !prev_down)
        s_menu_index = (s_menu_index + 1) % MENU_ITEM_COUNT;
    if (a && !prev_a)
        menu_activate();
    prev_up = up; prev_down = down; prev_a = a;
}

/* --- Render-Routinen fuer Tabs --- */

static void render_top_bar(void) {
    draw_rect(0, 0, WIN_W, 36, 0x000F24);
    draw_rect_outline(0, 0, WIN_W, 36, 0x004080);

    /* Tabs */
    const char *tabs[4] = { "[F1] ZURUECK / NEUES SPIEL", "[F2] TFMX JUKEBOX", "[F3] STRUDEL STUDIO", "[F4] AMIGA INTRO" };
    for (int t = 0; t < 4; t++) {
        int x = 10 + t * 200;
        int active = (t == 0) ? (s_gui_state == GUI_STATE_GAME || s_gui_state == GUI_STATE_GAMEOVER)
                              : (s_current_tab == (GuiTab)t);
        draw_rect(x, 4, 190, 28, active ? 0x004080 : 0x001830);
        draw_rect_outline(x, 4, 190, 28, active ? 0x00FFFF : 0x003060);
        draw_string(x + 16, 14, tabs[t], active ? 0x00FFFF : 0xA0C0E0, 1);
    }

    /* Sound Status */
    {
        char aud[160];
        snprintf(aud, sizeof(aud), "AUDIO: 16574 Hz STEREO | 8-VOICE PAULA | SOUNDGODIZER: %s [F5]",
                 god_mode_name());
        draw_string(WIN_W - 560, 14, aud, s_god_mode ? 0xFF40FF : 0x40FF80, 1);
    }
}

static void render_status_bar(void) {
    draw_rect(0, WIN_H - 24, WIN_W, 24, 0x000A18);
    draw_rect_outline(0, WIN_H - 24, WIN_W, 24, 0x003060);
    draw_string(12, WIN_H - 16, s_status_msg, 0x80D0FF, 1);
}

/* --- Turrican-2-Style UI: 2-Ton-Schrift mit Schlagschatten --- */

/* Glyph in zwei Farben: Zeilen 0..3 = obere Farbe, 4..7 = untere Farbe. */
static void draw_char_2tone(int x, int y, char ch, uint32_t top_c, uint32_t bot_c, int scale) {
    if ((unsigned char)ch >= 128) ch = '?';
    const uint8_t *glyph = RETRO_FONT[(unsigned char)ch];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        uint32_t c = (row < 4) ? top_c : bot_c;
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        set_pixel(x + col * scale + dx, y + row * scale + dy, c);
            }
        }
    }
}

/* Grosser Titel: pro Zeichen Sinus-Wanken, dunkelroter Schlagschatten,
 * zweifarbige Fuellung (Orange ueber Rot) — Retro-Turrican-Look. */
static void draw_title(int cx, int cy, const char *text, int scale) {
    int n = (int)strlen(text);
    int x0 = cx - (n * 8 * scale) / 2;
    int i;
    for (i = 0; i < n; i++) {
        float oy = sinf((float)i * 0.45f + (float)s_gui_frame * 0.07f) * 6.0f;
        int x = x0 + i * 8 * scale;
        int y = cy + (int)oy;
        draw_char(x + 2, y + 2, text[i], 0xFF2800, scale);              /* Schatten */
        draw_char_2tone(x, y, text[i], 0xFFFF7020, 0xFFD02010, scale);  /* 2-Ton  */
    }
}

/* Forward (Definition unten, vor biome_palettes) */
static void draw_bg_structure(int sx, int base_y, int slot, int biome, uint32_t col);

/* --- GUI_STATE_MENU: Hauptmenue (Turrican-2-Style) --- */
static void render_state_menu(void) {
    int i;
    clear_screen(0x000000);

    /* Dezent schwebende Copper-Baender (Amiga-Vertikalverlauf) */
    for (i = 0; i < 100; i++) {
        int y1 = 60 + i * 7;
        if (y1 >= WIN_H - 70) break;
        uint32_t c = ((uint32_t)(8 + (i & 1) * 6) << 16) |
                     ((uint32_t)(2 + (i & 3)) << 8) |
                     (uint32_t)(10 + ((i >> 1) & 3) * 4);
        draw_rect(0, y1, WIN_W, 2, c);
    }

    /* Turrican-2-Silhouette: Chrome-Tempest-Tuerme am Menue-Boden (dezent) */
    for (i = 0; i < 12; i++) {
        draw_bg_structure(i * 140, WIN_H - 70, i, 0, 0x101A28);
    }

    /* Titel mit Schlagschatten + 2 Tonen */
    draw_title(WIN_W / 2, 90, "STRUDELFORCE", 4);
    {
        char sub2[128];
        const char *sub = "A TURRICAN TRIBUTE FOR THE AMIGA 500";
        draw_string(WIN_W / 2 - (int)strlen(sub) * 4, 175, sub, 0xFF905030, 1);
        snprintf(sub2, sizeof(sub2), "VERSION " GAME_VERSION "  -  CYBER PAULA ENGINE   |   CONTROLLER: %s",
                 s_pad_connected ? "VERBUNDEN" : "KEINER ANGESCHLOSSEN");
        draw_string(WIN_W / 2 - (int)strlen(sub2) * 4, 190, sub2, 0xFF707070, 1);
    }

    /* Menue-Liste (Auswahl: Orange-Rot-Block, groessere Schrift, Pfeil) */
    for (i = 0; i < MENU_ITEM_COUNT; i++) {
        int y = 268 + i * 40;
        int sel = (i == s_menu_index);
        int lx = WIN_W / 2 - 170;
        if (sel) {
            int k;
            draw_rect(lx - 20, y - 8, 380, 36, 0xFF2A0800);
            draw_rect_outline(lx - 20, y - 8, 380, 36, 0xFFFF8020);
            draw_char(lx - 26, y, '>', 0xFFFFA040, 2);
            draw_char_2tone(lx + 8, y, menu_labels[i][0], 0xFFFFC0A0, 0xFFFF9060, 2);
            for (k = 1; menu_labels[i][k]; k++)
                draw_char(lx + 8 + k * 16, y, menu_labels[i][k], 0xFFFFFF, 2);
        } else {
            draw_string(lx + 8, y + 4, menu_labels[i], 0xFF8A4A30, 1);
        }
    }

    /* Blinkender Eingabe-Hinweis */
    if ((s_gui_frame / 25) % 2 == 0) {
        const char *hint = "[ UP/DOWN ] WAHLEN   [ ENTER ] AUSFUEHREN";
        draw_string(WIN_W / 2 - (int)strlen(hint) * 4, 505, hint, 0xFFFF8020, 1);
    }

    /* Steuerung & Footer */
    draw_rect(0, WIN_H - 70, WIN_W, 46, 0x000A0402);
    draw_string(40, WIN_H - 62, "WASD/PFEILE BEWEGEN  LEER SPRUNG  STRG FEUER  ALT BEAM  C WAFFE  S MORPH  ESC MENUE", 0xFF707070, 1);
    draw_string(40, WIN_H - 44, "CONTROLLER: STICK/D-PAD  A=FEUER  X=BEAM  B=MORPH  RS=WAFFE  START=MENUE  |  F2-F5 STUDIO", 0xFF707070, 1);
    draw_string(40, WIN_H - 26, "(C) 2026 FRANK MITZKATIS & AI CODING AGENTS  -  CREDITS FUER DIE VOLLE LISTE", 0xFF505050, 1);

    render_status_bar();
}

/* --- GUI_STATE_LOADING: Amiga-Style Boot-/Loading-Screen (Alien-Breed-Anlehnung) --- */
static void render_state_loading(void) {
    int i;
    int p = s_loading_ticks;
    if (p > LOAD_DURATION) p = LOAD_DURATION;
    clear_screen(0x000000);

    draw_title(WIN_W / 2, 120, "STRUDELFORCE", 4);
    {
        const char *d1 = "DISK 1 : STRUDELFORCE.ADF  (880K FFS, SINGLE DISC)";
        draw_string(WIN_W / 2 - (int)strlen(d1) * 4, 220, d1, 0xFFE0C060, 1);
    }

    /* Progress-Bar (Orange/Rot, Amiga-Block-Look mit Lauf-Blenden) */
    {
        int bx = 340, by = 300, bw = 600, bh = 26;
        int pct = (p * 100) / LOAD_DURATION;
        int fill = (bw * p) / LOAD_DURATION;
        draw_rect(bx - 4, by - 4, bw + 8, bh + 8, 0xFF5A1800);
        draw_rect(bx, by, bw, bh, 0xFF1A0600);
        if (fill > 0) {
            draw_rect(bx, by, fill, bh, 0xFFB03000);
            for (i = 0; i < 10; i++) {
                int stripe = ((i * 64 + s_gui_frame * 4) % (bw + 40)) - 20;
                if (stripe < fill) draw_rect(bx + stripe, by, 10, bh, 0xFFFF7020);
            }
        }
        draw_rect_outline(bx, by, bw, bh, 0xFFFF9040);
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%3d%%", pct);
            draw_string(bx + bw - 56, by + 7, buf, 0xFFFFFF, 2);
        }
    }

    /* Ladetext (Alien-Breed-Style Boot-Phasen) */
    {
        const char *step;
        if (p < 25)        step = "PRUEFE CHIP-RAM BANKEN ........ OK";
        else if (p < 50)   step = "INITIALISIERE PAULA DMA ....... OK";
        else if (p < 75)   step = "BILDE 2.5D VOXEL-SPRITETABELN . OK";
        else if (p < 100)  step = "MIXE CYBER-PAULA MATRIX ....... OK";
        else if (p < 115)  step = "LAEDE WELTCHUNKS (SEED 1234) ..";
        else               step = "BEREIT!";
        draw_string(340, 350, step, 0xFF80E0FF, 1);
    }

    if ((s_gui_frame / 20) % 2 == 0) {
        const char *ins = "PRESS FIRE TO CONTINUE";
        draw_string(WIN_W / 2 - (int)strlen(ins) * 4, 560, ins, 0xFFFFC040, 1);
    }

    render_status_bar();
}

/* --- GUI_STATE_CREDITS: scrollender Abspann --- */
static void render_state_credits(void) {
    static const char *lines[] = {
        "PROGRAMM, DESIGN & PRODUKTION",
        "    Frank Mitzkatis",
        "",
        "AI CODING AGENTS",
        "    Qwen 3.8 27B     (Fruehe Entwicklung)",
        "    Cline 3.4 122B   (Implementierung & Test)",
        "    Claude 5.5 Opus  (Architektur & Review)",
        "    Gemini 4 Argon   (Experiment & Analyse)",
        "    Gemini 3.8 Flash (Schnelle Iterationen)",
        "",
        "SPECIAL THANKS",
        "    Chris Huelbeck - fuer den besten Soundtrack",
        "    unserer Kindheit. Danke!",
        "",
        "    Turrican 2 / Alien Breed / Bitmap Brothers",
        "    Amiga-Community 1985-1993",
        "",
        "STRUDELFORCE 2026 - MADE ON THE AMIGA 500 (50HZ PAL)",
        "",
        "DRUECKE ESC ODER ENTER ZURUECK ZUM MENUE"
    };
    int n = (int)(sizeof(lines) / sizeof(lines[0]));
    int line_h = 22;
    int total = WIN_H + n * line_h + 100;
    int off = s_gui_frame / 2;                 /* 0.5 px pro Frame */
    int y0 = (total - off) % total - n * line_h - 100;
    int i;

    clear_screen(0x000000);
    for (i = 0; i < n; i++) {
        int ly = y0 + 160 + i * line_h;
        if (ly < 90 || ly > WIN_H - 30) continue;
        if (lines[i][0] == '\0') continue;
        if (lines[i][0] == ' ') {
            draw_string(240, ly, lines[i], 0xFFE09060, 1);      /* Namen: warm orange */
        } else {
            draw_title(WIN_W / 2, ly, lines[i], 1);             /* Rubriken: 2-Ton   */
        }
    }
    render_status_bar();
}

/* Kampagne-Biome-Paletten (vollstaendige Definition vor render_tab_game) */
typedef struct {
    const char *name;
    uint32_t sky_r, sky_g, sky_b;  /* Copper-Himmel-Basis (r, g, b) */
    uint32_t pillar;               /* Ruinen-Saeulen-Voxel */
    uint32_t ground;               /* Bodenplatten-Voxel */
} BiomePalette;
static const BiomePalette biome_palettes[5];

/* --- KAMPAGNE: Level-Clear-Intermezzo (2 Sekunden, ueber dem Spielbild) --- */
static void render_level_clear_overlay(void) {
    char buf[96];
    draw_rect(0, 220, WIN_W, 140, 0xC0000810);
    draw_title(WIN_W / 2, 240, "LEVEL GEKLAERT!", 3);
    snprintf(buf, sizeof(buf), "LEVEL %u ABGESCHLOSSEN   + %d BONUS-PUNKTE",
             (unsigned)s_game.campaign_level, 1000 * (int)s_game.campaign_level);
    draw_string(WIN_W / 2 - (int)strlen(buf) * 8, 320, buf, 0xFFE0C060, 2);
    if (s_game.campaign_level < CAMPAIGN_LEVELS) {
        snprintf(buf, sizeof(buf), "NAECHSTES LEVEL: %s", biome_palettes[s_game.campaign_level].name);
        draw_string(WIN_W / 2 - (int)strlen(buf) * 8, 348, buf, 0xFF60D0FF, 2);
    }
}

/* --- KAMPAGNE: Sieg-Screen (nach Level 5, wie Game-Over aufgebaut) --- */
static void render_victory_overlay(void) {
    int y;
    for (y = 0; y < WIN_H; y++) {
        uint32_t *row = &s_pixels[y * WIN_W];
        int x;
        for (x = 0; x < WIN_W; x++) row[x] = rgb_mod(row[x], 0.22f);
    }
    draw_title(WIN_W / 2, 150, "SIEG!", 5);
    {
        const char *a = "KAMPAGNE ABGESCHLOSSEN - DER STRUDELFORCE-KERN IST GEKLAERT";
        draw_string(WIN_W / 2 - (int)strlen(a) * 4, 240, a, 0xFF60FF60, 2);
    }
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "SCORE %u   |   KILLS %u   |   LEVEL 5/5",
                 s_game.score, s_game.enemies_killed);
        draw_string(WIN_W / 2 - (int)strlen(buf) * 8, 300, buf, 0xFFE0C060, 2);
    }
    if ((s_gui_frame / 20) % 2 == 0) {
        const char *b = "[ ENTER ] NOCHMAL   [ ESC ] HAUPTMENUE";
        draw_string(WIN_W / 2 - (int)strlen(b) * 8, 380, b, 0xFFFF8020, 2);
    }
}

/* --- GUI_STATE_GAMEOVER: Overlay ueber dem letzten Spielbild --- */
static void render_game_over_overlay(void) {
    int y;
    /* Dimmen des letzten Frames (Halbtransparent-Optik ohne Alpha) */
    for (y = 0; y < WIN_H; y++) {
        uint32_t *row = &s_pixels[y * WIN_W];
        int x;
        for (x = 0; x < WIN_W; x++) row[x] = rgb_mod(row[x], 0.22f);
    }
    draw_title(WIN_W / 2, 160, "GAME OVER", 5);
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "SCORE %u   |   WAVE %u   |   KILLS %u",
                 s_game.score, s_game.wave, s_game.enemies_killed);
        draw_string(WIN_W / 2 - (int)strlen(buf) * 8, 270, buf, 0xFFE0C060, 2);
    }
    if ((s_gui_frame / 20) % 2 == 0) {
        const char *a = "[ ENTER ] NOCHMAL";
        const char *b = "[ ESC ]   HAUPTMENUE";
        draw_string(WIN_W / 2 - (int)strlen(a) * 8, 360, a, 0xFFFF8020, 2);
        draw_string(WIN_W / 2 - (int)strlen(b) * 8, 410, b, 0xFF909090, 2);
    }
}

/* --- TAB 1: 2.5D Voxel Game View ---
 * Parallax-Regel (12x Welt-Zoom, s_game.px = Weltposition in fix16):
 *   Layer 3 (Himmel):  statisch,  1/16  -> keine Bewegung (Copper-Verlauf)
 *   Layer 2 (Saeulen): 1/4 Parallax  -> Scroll = Welt * 3   (12/4)
 *   Layer 1 (Gameplay): 1:1           -> Entities = Welt * 12 (Anker x=80)
 *   Layer 0 (Vgr-Rohre): 1.5x         -> Scroll = Welt * 18 (12*1.5)
 * Vorher waren die Faktoren (4 / 0 / 18) untereinander inkonsistent ->
 * das sah aus wie "schwimmende" uebereinanderliegende Ebenen. */

/* Kampagne-Biome-Paletten (Gesetz 2: reine Farbpaletten, keine Texturen).
 * Index = core_campaign_biome(): 0..4 (Endless: immer 0 = v0.0-Look).
 * Himmelsformel: Kanal = Basis + Multiplikator*Basis*(0.5+0.5*sin) — fuer
 * Biome 0 exakt die historische Formel (10+20f / 20+60f / 60+120f). */
/* ================================================================
 * Turrican-2-Visual-Upgrade (v0.1a): pro Biome eigene Welt-Geometrie.
 * REINES RENDERER-SEIT — das Core bleibt bit-identisch (Determinismus).
 * Layer 2 = Biome-Silhouetten (1/4 Parallax), Untergrund = Terrassen
 * mit Strata-Linien (1:1), Layer 0 = Vordergrund-Rohre (1.5x Scroll).
 * ================================================================ */

/* Deterministischer World-Hash (LCG + Xorshift, pro Biome + Slot + Salt) */
static int world_hash(int biome, int slot, int salt) {
    uint32_t s = (uint32_t)slot * 1664525u + (uint32_t)biome * 1013904223u + (uint32_t)salt * 7919u + 24635u;
    s = s * 1103515245u + 12345u;
    s ^= s >> 13;
    s = s * 22695477u + 1u;
    s ^= s >> 16;
    return (int)(s & 0x7FFFFFFF);
}

/* Ganzzahl-Division, abgerundet auf -unendlich (b > 0) */
static int floor_div(int a, int b) {
    return (a >= 0) ? a / b : (a - b + 1) / b;
}

/* Untergrund-Hoehe (px) pro 24px-Block — jedes Biome hat eigene Gelaeufe */
static int terrain_height(int biome, int block) {
    int h = world_hash(biome, block, 11) % 100;
    switch (biome) {
        case 1:  return 24 + (h % 2) * 10;            /* THE GREAT BATH: flache Wellen     */
        case 2:  return (h < 55) ? 96 : 28;           /* MECHANICAL HIVE: hohe Rohrblöcke  */
        case 3:  return 64 + (h % 4) * 22;            /* VOID REACH: tiefe Kerben          */
        case 4:  return 44 + (h % 5) * 18;            /* CORE FURNACE: Zacken              */
        default: return 28 + (h % 3) * 14;            /* CHROME TEMPEST: weite Stufen      */
    }
}

/* Layer 2: Biome-Silhouette (Standflaeche = base_y, z.B. ground_y) */
static void draw_bg_structure(int sx, int base_y, int slot, int biome, uint32_t col) {
    int h = world_hash(biome, slot, 21);
    int i;
    switch (biome) {
        case 1: { /* THE GREAT BATH: Kuppeln mit Krone */
            int dw = 44 + h % 30;
            int dh = 96 + h % 64;
            for (i = 0; i < 5; i++) {
                int t = i * i;
                draw_rect(sx + t / 2, base_y - dh + i * (dh / 5), dw - t, dh / 5 + 1, col);
            }
            draw_rect(sx + dw / 2 - 8, base_y - dh - 14, 16, 14, col);
            break;
        }
        case 2: { /* MECHANICAL HIVE: Fabrikblöcke + Schornstein + Fensterreihen */
            int dw = 58 + h % 40;
            int dh = 150 + h % 96;
            draw_rect(sx, base_y - dh, dw, dh, col);
            draw_rect(sx + dw - 14, base_y - dh - 34, 10, 34, col);
            for (i = 0; i < dh / 36; i++)
                draw_rect(sx + 6, base_y - dh + 12 + i * 36, dw - 12, 4, col);
            break;
        }
        case 3: { /* VOID REACH: Monolithe mit Kristallspitze */
            int mw = 18 + h % 14;
            int mh = 190 + h % 120;
            draw_rect(sx + 10, base_y - mh, mw, mh, col);
            draw_rect(sx + 12 + mw / 2, base_y - mh - 18, 4, 18, col);
            break;
        }
        case 4: { /* CORE FURNACE: Ofentürme mit flickernden Glut-Kronen */
            int dw = 46 + h % 26;
            int dh = 170 + h % 80;
            uint32_t glow = ((s_gui_frame / 8) % 2 == 0) ? 0xFF802000 : 0xFFB03800;
            draw_rect(sx + 8, base_y - dh, dw, dh, col);
            draw_rect(sx + 8, base_y - dh - 12, dw, 12, glow);
            draw_rect(sx + 8 + dw / 2 - 3, base_y - dh + 24, 6, 6, glow);
            break;
        }
        default: { /* CHROME TEMPEST: Antennen-Türme */
            int dh = 170 + h % 100;
            draw_rect(sx + 12, base_y - dh, 20, dh, col);
            draw_rect(sx + 18, base_y - dh - 30, 8, 30, col);
            draw_rect(sx + 20, base_y - dh - 44, 4, 14, col);
            break;
        }
    }
}

/* Layer 0: Vordergrund-Rohre (Turrican-2-Signatur) — 1.5x Scroll,
 * pro Slot eigene Hoehe/Breite, Flansche, Naht, Bohrungen, Biome-Farbton */
static void draw_fg_pipes(int x_off, int biome, uint32_t body_c, uint32_t seam_c) {
    int i;
    for (i = 0; i < 5; i++) {
        int a = i * 320 - x_off;
        int px = a % WIN_W;
        if (px < 0) px += WIN_W;
        int slot = floor_div(a, 320);
        int h = 100 + world_hash(biome, slot, 31) % 200;
        int w = 40 + world_hash(biome, slot, 32) % 24;
        int top = WIN_H - h;
        int j, yy;
        draw_rect(px, top, w, h, body_c);
        draw_rect(px + w / 2 - 2, top, 4, h, seam_c);
        for (yy = top + 20; yy < WIN_H - 30; yy += 48) {
            draw_rect(px - 4, yy, w + 8, 8, body_c);
            draw_rect(px - 4, yy, w + 8, 1, rgb_mod(body_c, 1.6f));
        }
        draw_rect(px, top, w, 3, rgb_mod(body_c, 1.7f));
        for (j = 0; j < w / 8; j++)
            draw_rect(px + 4 + j * 8, top + 5, 3, 3, rgb_mod(body_c, 0.7f));
    }
}

/* HUD-Panel (Amiga-Bevel: hell oben/links, dunkel unten/rechts + Rahmen) */
static void draw_panel(int x, int y, int w, int h, uint32_t border) {
    draw_rect(x, y, w, h, 0x0C141E);
    draw_rect(x, y, w, 1, 0x3E5C7C);
    draw_rect(x, y, 1, h, 0x3E5C7C);
    draw_rect(x, y + h - 1, w, 1, 0x02040A);
    draw_rect(x + w - 1, y, 1, h, 0x02040A);
    draw_rect_outline(x, y, w, h, border);
}

/* Segmentierte Status-Bar (24 Segmente, 1px Ritze, zweifarbige Fuellung) */
static void draw_stat_bar(int x, int y, int w, int h, int pct, uint32_t hi_c, uint32_t lo_c) {
    int segs = 24, fill, i;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    fill = (pct * segs) / 100;
    draw_rect(x, y, w, h, 0x05090E);
    for (i = 0; i < segs; i++) {
        int sw = w / segs;
        if (i < fill)
            draw_rect(x + i * sw, y, sw - 1, h, (i < fill / 2) ? lo_c : hi_c);
        else
            draw_rect(x + i * sw, y, sw - 1, h, 0x0A1218);
    }
    draw_rect_outline(x, y, w, h, 0x2A4860);
}

static const BiomePalette biome_palettes[5] = {
    { "CHROME TEMPEST",  10, 20,  60, 0x4A5868, 0x304050 }, /* blau   (v0.0) */
    { "THE GREAT BATH",   5, 40,  80, 0x2E6870, 0x205058 }, /* teal   (Unterwasser) */
    { "MECHANICAL HIVE", 60, 20,   5, 0x6E4A28, 0x503820 }, /* rost   (Industrie) */
    { "VOID REACH",      30,  5,  60, 0x403058, 0x382850 }, /* violet (Die Leere) */
    { "CORE FURNACE",    80, 15,   5, 0x703818, 0x582810 }  /* glut   (Endlevel) */
};

static void render_tab_game(void) {
    int ground_y = 620;
    const BiomePalette *bp = &biome_palettes[core_campaign_biome(&s_game)];

    /* Basis-Fluellung unter der Top-Bar (keine schwarzen Luecken mehr) */
    draw_rect(0, 36, WIN_W, WIN_H - 36, 0x000A18);

    /* Layer 3: Copper Horizon Bänder (80 Bänder = bis y=676, ueber den Boden) */
    for (int i = 0; i < 80; i++) {
        int y1 = 36 + i * 8;
        float factor = 0.5f + 0.5f * sinf((float)i * 0.1f + (float)s_game.tick * 0.04f);
        uint32_t col = ((uint32_t)(bp->sky_r + 2.0f * (float)bp->sky_r * factor) << 16) |
                       ((uint32_t)(bp->sky_g + 3.0f * (float)bp->sky_g * factor) << 8)  |
                       (uint32_t)(bp->sky_b + 2.0f * (float)bp->sky_b * factor);
        draw_rect(0, y1, WIN_W, 8, col);
    }

    int bi = (int)(bp - biome_palettes);

    /* Layer 2: Biome-Silhouetten (1/4 Parallax = 3x bei 12x Welt-Zoom) —
     * pro Biome eigene Strukturform (Turrican-2-Upgrade, deterministisch) */
    int x_off = (s_game.px >> 16) * 3;
    for (int i = 0; i < 12; i++) {
        int a = i * 140 - x_off;
        int px = a % WIN_W;
        if (px < 0) px += WIN_W;
        draw_bg_structure(px, ground_y, floor_div(a, 140), bi, rgb_mod(bp->pillar, 0.72f));
    }

    /* Bodenlinie mit Voxel-Platten (scrollt 1:1 mit der Welt = 12x) */
    int g_off = (s_game.px >> 16) * 12;
    for (int i = 0; i < 68; i++) {
        int bx = (i * 20 - g_off) % WIN_W;
        if (bx < 0) bx += WIN_W;
        draw_voxel(bx, ground_y, 18, 12, 3, bp->ground);
    }

    /* Untergrund: pro Biome eigene Terrassen (24px-Blöcke) + Strata-Linien */
    {
        uint32_t under  = rgb_mod(bp->ground, 0.55f);
        uint32_t strata = rgb_mod(bp->ground, 0.36f);
        uint32_t top_hi = rgb_mod(bp->ground, 1.7f);
        for (int x = 0; x < WIN_W; x++) {
            int h = terrain_height(bi, floor_div(x + g_off, 24));
            int top = ground_y + 12;
            int yy;
            draw_rect(x, top, 1, h, under);
            draw_rect(x, top, 1, 2, top_hi);
            for (yy = top + 16; yy < top + h; yy += 16)
                draw_rect(x, yy, 1, 2, strata);
        }
    }

    /* Layer 1: Gameplay Entities */

    /* 1. Spieler (2.5D Voxel-Raumschiff, Side-Scroll-Silhouette nach rechts) */
    int p_screen_x = 80 + (s_game.px >> 16) * 12;
    int p_screen_y = ground_y - 40 - (s_game.py >> 16) * 16;
    if (p_screen_x < 40) p_screen_x = 40;
    if (p_screen_x > WIN_W - 80) p_screen_x = WIN_W - 80;

    /* Respawn-Gnadenfrist: Spieler blinkt (Unverwundbar-Optik) */
    int player_visible = (s_game.respawn_timer == 0) || ((s_game.respawn_timer / 5) % 2 == 0);
    if (player_visible) {
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
    } /* Ende player_visible */

    /* 3.5 Extraleben-Pickups (an den Welt-Raendern, Bob-Animation) */
    for (int i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *pk = &s_game.pickups[i];
        if (!pk->active) continue;
        int kx = 80 + (pk->x >> 16) * 12;
        int ky = ground_y - 40 - (pk->y >> 16) * 16;
        int bob = (int)(sinf((float)s_gui_frame * 0.15f + (float)i) * 4.0f);
        draw_voxel(kx, ky - 16 + bob, 12, 12, 4, 0x00E050);
        set_pixel(kx + 5, ky - 12 + bob, 0xFFFFFF);
    }

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

    /* Layer 0: Vordergrund-Rohre (1.5x Scroll, Turrican-2-Signatur;
     * pro Biome + Slot eigene Hoehe/Breite/Farbton, deterministisch) */
    draw_fg_pipes((s_game.px >> 16) * 18, bi, rgb_mod(bp->pillar, 0.5f), rgb_mod(bp->pillar, 0.3f));

    /* Low-HP Interlace Jitter (<25% HP) */
    if (s_game.max_hp > 0 && s_game.hp * 4 < s_game.max_hp) {
        for (int y = 36; y < ground_y; y += 2) {
            uint32_t *row = &s_pixels[y * WIN_W];
            for (int x = 0; x < WIN_W; x++) {
                row[x] = rgb_mod(row[x], 0.65f);
            }
        }
    }

    /* Wave-/Level-Intro-Banner (mitte, ueber dem Gameplay) */
    if (s_game.state == STATE_WAVE_INTRO) {
        char banner[64];
        if (s_game.mode == MODE_CAMPAIGN && s_game.wave % CAMPAIGN_WAVES_PER_LEVEL == 0) {
            int nl = (int)(s_game.wave / CAMPAIGN_WAVES_PER_LEVEL) + 1;
            snprintf(banner, sizeof(banner), "LEVEL %d: %s", nl, biome_palettes[nl - 1].name);
            draw_title(WIN_W / 2, 250, banner, 2);
        } else {
            snprintf(banner, sizeof(banner), "WAVE %u", (unsigned)s_game.wave + 1);
            draw_title(WIN_W / 2, 250, banner, 2);
        }
    }

    /* ============ HUD v2: saubere Bevel-Panels (Turrican-2-Style) ============ */
    {
        char buf[96];
        const char *wpn_names[3] = { "SPREAD", "LASER", "BOUNCE" };
        int hp_pct = s_game.max_hp > 0 ? (s_game.hp * 100) / s_game.max_hp : 0;
        uint32_t hp_hi, hp_lo;
        if (hp_pct > 50)       { hp_hi = 0x40FF80; hp_lo = 0x18C040; }
        else if (hp_pct > 25)  { hp_hi = 0xFFC040; hp_lo = 0xD09018; }
        else                   { hp_hi = ((s_gui_frame / 6) % 2 == 0) ? 0xFF4020 : 0xFF8040; hp_lo = 0xC03018; }

        /* Linkes Panel: LEBEN + HP + SCHILD */
        draw_panel(16, 46, 264, 104, 0x2A4860);
        draw_string(28, 54, "LIVES", 0x7090B0, 1);
        for (int li = 0; li < (int)s_game.lives; li++)
            draw_voxel(96 + li * 22, 56, 16, 7, 2, 0x3858B8);
        draw_string(28, 78, "HP", 0x7090B0, 1);
        draw_stat_bar(64, 78, 200, 12, hp_pct, hp_hi, hp_lo);
        draw_string(28, 98, "SCHILD", 0x7090B0, 1);
        draw_stat_bar(64, 98, 200, 12, s_game.shield, 0x40D8FF, 0x1870C0);
        draw_string(28, 122, "EXTRA-LEBEN AN DEN WELT-RAENDERN", 0x405868, 1);

        /* Zentrales Score-Panel */
        draw_panel(WIN_W / 2 - 170, 46, 340, 44, 0x2A4860);
        draw_string(WIN_W / 2 - 156, 52, "SCORE", 0x7090B0, 1);
        snprintf(buf, sizeof(buf), "%08u", (unsigned)s_game.score);
        draw_string(WIN_W / 2 - 156, 66, buf, 0xFFFFFF, 2);
        snprintf(buf, sizeof(buf), "KILLS %u", (unsigned)s_game.enemies_killed);
        draw_string(WIN_W / 2 + 70, 52, buf, 0x90A8C0, 1);

        /* Rechtes Panel: LEVEL/WAVE + BIOME + WAFFE + TRUHE */
        draw_panel(WIN_W - 280, 46, 264, 104, 0x2A4860);
        if (s_game.mode == MODE_CAMPAIGN)
            snprintf(buf, sizeof(buf), "KAMPAGNE %u/%d", (unsigned)s_game.campaign_level, CAMPAIGN_LEVELS);
        else
            snprintf(buf, sizeof(buf), "ENDLOS  WAVE %u", (unsigned)s_game.wave + 1);
        draw_string(WIN_W - 268, 54, buf, 0xFFFFC040, 2);
        draw_string(WIN_W - 268, 78, bp->name, 0xA0E0FF, 1);
        snprintf(buf, sizeof(buf), "WPN: %s   LV %u", wpn_names[s_game.weapon % 3], s_game.weapon_level);
        draw_string(WIN_W - 268, 94, buf, 0xFFE040, 1);
        snprintf(buf, sizeof(buf), "TRUHE: %s", s_livecode.chest_opened ? "GEOEFFNET" : "GESCHLOSSEN");
        draw_string(WIN_W - 268, 110, buf, s_livecode.chest_opened ? 0x60FF90 : 0x405868, 1);
    }
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

    /* Freie Pfadeingabe: typen + EINGABE (auch absolute Pfaede z.B. D:\MUSIC) */
    {
        char pathline[280];
        snprintf(pathline, sizeof(pathline), "PFAD: %s", s_exp_path_buf);
        draw_string(30, 82, pathline, 0xFF80FF80, 1);
        if ((s_gui_frame / 15) % 2 == 0)
            set_pixel(30 + (int)strlen(pathline) * 8 + 2, 86, 0x00FF80); /* Blink-Cursor */
    }

    int y = 108;
    int items_per_page = 24;
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
    } else if (s_exp_mod.loaded) {
        char buf[256];
        snprintf(buf, sizeof(buf), "TITEL: %s  [MOD-PLAYER]", s_exp_mod.title[0] ? s_exp_mod.title : "ProTracker Module");
        draw_string(rx + 20, 95, buf, 0x00FF80, 1);
        snprintf(buf, sizeof(buf), "STATUS: %s | CHANNEL: %d | BPM: %u | SONGLEN: %u",
                 s_exp_mod_playing ? ">> SPIELT <<" : "[ANGEHALTEN]",
                 s_exp_mod.nch, s_exp_mod.bpm, s_exp_mod.songlen);
        draw_string(rx + 20, 115, buf, 0xA0E0FF, 1);
    }
    if (s_exp_tfmx.loaded || s_exp_mod.loaded) {

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

            int vol = 0;
            int note = -1;
            if (s_exp_playing && s_exp_tfmx.loaded) {
                vol = s_exp_tfmx.v[v].dma ? s_exp_tfmx.v[v].vol : 0;
                note = s_exp_tfmx.v[v].note;
            } else if (s_exp_mod_playing && s_exp_mod.loaded) {
                if (v < s_exp_mod.nch) {
                    vol = s_exp_mod.ch[v].active ? s_exp_mod.ch[v].vol : 0;
                    note = s_exp_mod.ch[v].note_per > 0 ? (int)(log(856.0 / (double)s_exp_mod.ch[v].note_per) / 0.05776 + 36.5) : -1;
                }
            } else {
                vol = s_audio.voices[v].active ? s_audio.voices[v].volume : 0;
                note = s_audio.voices[v].active ? s_audio.voices[v].base_note : -1;
            }
            if (vol > 64) vol = 64;
            int bar_h = (vol * 170) / 64;

            /* VU Meter Bar */
            draw_rect(vx + 25, vy + 200 - bar_h, 30, bar_h, (vol > 50) ? 0xFF3030 : ((vol > 30) ? 0xFFC000 : 0x00FF40));

            snprintf(vname, sizeof(vname), "V:%2d", vol);
            draw_string(vx + 25, vy + 212, vname, 0xD0E0FF, 1);

            const char *nnames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            if (note > 0) {
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

    draw_string(65, 140, "Strudel-Pattern (Klick / Tippen / CTRL+V):", 0xFFC040, 1);
    draw_string(65, 160, s_strudel_input, 0xFFFFFF, 1);

    /* Cursor */
    int cur_x = 65 + s_strudel_cursor * 8;
    if ((s_gui_frame / 25) % 2 == 0) {
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
    draw_string(50, 430, "TASTATUR / EDITOR:  CTRL+V = EINFUEGEN  CTRL+C = KOPIEREN  ENTF/BACK = LOESCHEN  ENTER = ANWENDEN", 0x00FFFF, 1);
    draw_string(50, 455, "SYNTAX-HILFE:", 0xFFE040, 1);
    draw_string(50, 470, "  - Sequenzen:       c3 [eb3 g3] <bb2 f3>", 0xD0E0FF, 1);
    draw_string(50, 490, "  - Wiederholungen:  c3*4   oder [g3 c4]*2", 0xD0E0FF, 1);
    draw_string(50, 510, "  - Euklid:          c3(3,8) (Bjorklund Rhythmen)", 0xD0E0FF, 1);
    draw_string(50, 530, "  - Pausen:          ~ oder -", 0xD0E0FF, 1);
}

/* --- TAB 4: Amiga Copper Intro View --- */
static void render_tab_intro(void) {
    draw_rect(0, 36, WIN_W, WIN_H - 60, 0x000018);

    /* 3 Bouncing Copperbars (GUI-Frame-Quelle: laeuft in ALLEN Tabs, nicht nur im Game) */
    float y1 = 360.0f + 160.0f * sinf((float)s_gui_frame * 0.05f);
    float y2 = 360.0f + 180.0f * sinf((float)s_gui_frame * 0.07f + 1.8f);
    float y3 = 360.0f + 140.0f * sinf((float)s_gui_frame * 0.09f + 3.2f);

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
    int offset = s_gui_frame / 2;

    for (int col = 0; col < 120; col++) {
        char ch = scroller[(col + offset) % slen];
        float sy = 480.0f + 25.0f * sinf((float)col * 0.15f + (float)s_gui_frame * 0.08f);
        draw_char(40 + col * 10, (int)sy, ch, 0xFFFF00, 2);
    }
}

/* --- Frame-Dump (Headless-Visual-Test): strudelforce_gui.exe --dump [frames] [basis] ---
 * Rendert frames ohne Audio-Geraet und speichert alle 10 Ticks ein PPM-Bild.
 * Letztes Drittel: Intro-Tab (Anim-Check), ab 2/3 Low-HP (Interlace-Check). */
static void dump_ppm(const char *fn) {
    FILE *f = fopen(fn, "wb");
    int i;
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", WIN_W, WIN_H);
    for (i = 0; i < WIN_W * WIN_H; i++) {
        uint32_t c = s_pixels[i];
        fputc((int)((c >> 16) & 0xFF), f);
        fputc((int)((c >> 8) & 0xFF), f);
        fputc((int)(c & 0xFF), f);
    }
    fclose(f);
}

/* --- Win32 Message Callback --- */

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            /* --- Globale F-Keys & ESC (State-Maschine) --- */
            if (wParam == VK_F5) { god_toggle(); return 0; }

            /* F1: Hauptmenue/Game-Over -> neues Spiel; sonst -> zurueck zum Spiel */
            if (wParam == VK_F1) {
                if (s_gui_state == GUI_STATE_MENU || s_gui_state == GUI_STATE_GAMEOVER)
                    start_game(s_active_mode);
                else if (s_gui_state != GUI_STATE_LOADING)
                    s_gui_state = GUI_STATE_GAME;
                return 0;
            }
            /* F2-F4: Studio-Tools (im Spiel = Pause, im Menue = direkter Start) */
            if (wParam == VK_F2 || wParam == VK_F3 || wParam == VK_F4) {
                GuiTab t = (wParam == VK_F2) ? TAB_EXPLORER :
                           (wParam == VK_F3) ? TAB_STRUDEL : TAB_INTRO;
                if (s_gui_state != GUI_STATE_LOADING) {
                    s_current_tab = t;
                    s_gui_state = GUI_STATE_STUDIO;
                }
                return 0;
            }
            /* ESC: aus jedem aktiven Zustand zurueck zum Hauptmenue */
            if (wParam == VK_ESCAPE) {
                if (s_gui_state != GUI_STATE_MENU && s_gui_state != GUI_STATE_LOADING)
                    s_gui_state = GUI_STATE_MENU;
                return 0;
            }

            /* --- Hauptmenue-Navigation --- */
            if (s_gui_state == GUI_STATE_MENU) {
                if (wParam == VK_UP)   s_menu_index = (s_menu_index + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
                else if (wParam == VK_DOWN) s_menu_index = (s_menu_index + 1) % MENU_ITEM_COUNT;
                else if (wParam == VK_RETURN) menu_activate();
                return 0;
            }
            /* --- Credits: beenden --- */
            if (s_gui_state == GUI_STATE_CREDITS) {
                if (wParam == VK_RETURN || wParam == VK_ESCAPE) s_gui_state = GUI_STATE_MENU;
                return 0;
            }
            /* --- Game Over: ENTER = Neustart, ESC = Menue (s.o.) --- */
            if (s_gui_state == GUI_STATE_GAMEOVER) {
                if (wParam == VK_RETURN) start_game(s_active_mode);
                return 0;
            }

            /* --- Studio-Tab (Jukebox / Strudel / Intro) --- */
            if (s_gui_state != GUI_STATE_STUDIO) return 0;

            if (s_current_tab == TAB_STRUDEL) {
                int ctrl = (GetKeyState(VK_CONTROL) & 0x8000);
                if (ctrl && (wParam == 'V' || wParam == 'v')) {
                    /* Aus Windows-Zwischenablage einfuegen */
                    if (OpenClipboard(hWnd)) {
                        HANDLE hData = GetClipboardData(CF_TEXT);
                        if (hData) {
                            const char *clip_text = (const char *)GlobalLock(hData);
                            if (clip_text) {
                                while (*clip_text) {
                                    char ch = *clip_text++;
                                    if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
                                    if ((unsigned char)ch >= 32 && (unsigned char)ch <= 126 && strlen(s_strudel_input) < sizeof(s_strudel_input) - 2) {
                                        memmove(&s_strudel_input[s_strudel_cursor + 1], &s_strudel_input[s_strudel_cursor], strlen(&s_strudel_input[s_strudel_cursor]) + 1);
                                        s_strudel_input[s_strudel_cursor] = ch;
                                        s_strudel_cursor++;
                                    }
                                }
                                GlobalUnlock(hData);
                                apply_strudel_code(&s_livecode, s_strudel_input);
                                snprintf(s_status_msg, sizeof(s_status_msg), "Aus Zwischenablage eingefuegt!");
                            }
                        }
                        CloseClipboard();
                    }
                } else if (ctrl && (wParam == 'C' || wParam == 'c')) {
                    /* In Windows-Zwischenablage kopieren */
                    if (OpenClipboard(hWnd)) {
                        EmptyClipboard();
                        size_t len = strlen(s_strudel_input);
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
                        if (hMem) {
                            char *dst = (char *)GlobalLock(hMem);
                            if (dst) {
                                memcpy(dst, s_strudel_input, len + 1);
                                GlobalUnlock(hMem);
                                SetClipboardData(CF_TEXT, hMem);
                                snprintf(s_status_msg, sizeof(s_status_msg), "In Zwischenablage kopiert (%d Zeichen)", (int)len);
                            }
                        }
                        CloseClipboard();
                    }
                } else if (ctrl && (wParam == 'A' || wParam == 'a')) {
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                } else if (wParam == VK_DELETE) {
                    if (s_strudel_cursor < (int)strlen(s_strudel_input)) {
                        memmove(&s_strudel_input[s_strudel_cursor], &s_strudel_input[s_strudel_cursor + 1], strlen(&s_strudel_input[s_strudel_cursor + 1]) + 1);
                    }
                } else if (wParam == VK_HOME) {
                    s_strudel_cursor = 0;
                } else if (wParam == VK_END) {
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                } else if (wParam == VK_RETURN) {
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
                        if (s_explorer_selected >= s_explorer_scroll + 24) s_explorer_scroll++;
                    }
                } else if (wParam == VK_BACK) {
                    if (s_exp_path_cur > 0) s_exp_path_buf[--s_exp_path_cur] = '\0';
                } else if (wParam == VK_RETURN) {
                    if (s_exp_path_buf[0] != '\0') {
                        /* Freier Pfad: Ordner wechseln oder Datei direkt laden (.tfx/.mod) */
                        DWORD attr = GetFileAttributesA(s_exp_path_buf);
                        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                            strncpy(s_current_dir, s_exp_path_buf, sizeof(s_current_dir) - 1);
                            s_current_dir[sizeof(s_current_dir) - 1] = '\0';
                            explorer_scan_directory(s_current_dir);
                            snprintf(s_status_msg, sizeof(s_status_msg), "PFAD: %s", s_current_dir);
                        } else {
                            const char *ext = strrchr(s_exp_path_buf, '.');
                            if (ext && _stricmp(ext, ".tfx") == 0) {
                                s_exp_mod_playing = 0;
                                if (tfmx_player_load(&s_exp_tfmx, s_exp_path_buf, NULL)) {
                                    s_exp_subsong = 0;
                                    tfmx_player_start(&s_exp_tfmx, 0);
                                    s_exp_playing = 1;
                                    snprintf(s_status_msg, sizeof(s_status_msg), "TFMX geladen: %s", s_exp_path_buf);
                                } else {
                                    snprintf(s_status_msg, sizeof(s_status_msg), "TFMX-Load fehlgeschlagen: %s", s_exp_path_buf);
                                }
                            } else if (ext && _stricmp(ext, ".mod") == 0) {
                                FILE *f = fopen(s_exp_path_buf, "rb");
                                long sz = 0;
                                s_exp_playing = 0;
                                if (f) {
                                    fseek(f, 0, SEEK_END);
                                    sz = ftell(f);
                                    if (sz > (long)sizeof(s_exp_mod_buf)) sz = (long)sizeof(s_exp_mod_buf);
                                    fseek(f, 0, SEEK_SET);
                                    (void)fread(s_exp_mod_buf, 1, (size_t)sz, f);
                                    fclose(f);
                                }
                                if (sz > 1084 && mod_player_load_mem(&s_exp_mod, s_exp_mod_buf, (uint32_t)sz)) {
                                    mod_player_start(&s_exp_mod, GAME_AUDIO_RATE);
                                    s_exp_mod_playing = 1;
                                    snprintf(s_status_msg, sizeof(s_status_msg), "MOD geladen: %s", s_exp_path_buf);
                                } else {
                                    snprintf(s_status_msg, sizeof(s_status_msg), "MOD-Load fehlgeschlagen: %s", s_exp_path_buf);
                                }
                            } else {
                                snprintf(s_status_msg, sizeof(s_status_msg), "Nur Ordner oder .tfx/.mod moeglich: %s", s_exp_path_buf);
                            }
                        }
                        s_exp_path_buf[0] = '\0';
                        s_exp_path_cur = 0;
                    } else if (s_explorer_selected >= 0 && s_explorer_selected < s_num_explorer_items) {
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
                            /* TFMX oder MOD Datei laden */
                            const char *ext = strrchr(it->name, '.');
                            if (ext && _stricmp(ext, ".mod") == 0) {
                                FILE *f = fopen(it->path, "rb");
                                long size = 0;
                                s_exp_playing = 0;
                                s_exp_mod_playing = 0;
                                if (f) {
                                    fseek(f, 0, SEEK_END);
                                    size = ftell(f);
                                    if (size > (long)sizeof(s_exp_mod_buf)) size = (long)sizeof(s_exp_mod_buf);
                                    fseek(f, 0, SEEK_SET);
                                    (void)fread(s_exp_mod_buf, 1, (size_t)size, f);
                                    fclose(f);
                                }
                                if (size > 1084 && mod_player_load_mem(&s_exp_mod, s_exp_mod_buf, (uint32_t)size)) {
                                    mod_player_start(&s_exp_mod, GAME_AUDIO_RATE);
                                    s_exp_mod_playing = 1;
                                    snprintf(s_status_msg, sizeof(s_status_msg), "MOD geladen: %s", it->name);
                                }
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
            }
            break;

        case WM_CHAR:
            if (s_current_tab == TAB_STRUDEL) {
                char c = (char)wParam;
                if ((unsigned char)c >= 32 && (unsigned char)c <= 126 && strlen(s_strudel_input) < sizeof(s_strudel_input) - 2) {
                    memmove(&s_strudel_input[s_strudel_cursor + 1], &s_strudel_input[s_strudel_cursor], strlen(&s_strudel_input[s_strudel_cursor]) + 1);
                    s_strudel_input[s_strudel_cursor] = c;
                    s_strudel_cursor++;
                }
            } else if (s_current_tab == TAB_EXPLORER) {
                /* Freier Pfad: beliebig tippen, EINGABE = Ordner wechseln / Datei laden */
                char c = (char)wParam;
                if (c >= 32 && c <= 126 && s_exp_path_cur < (int)sizeof(s_exp_path_buf) - 1) {
                    s_exp_path_buf[s_exp_path_cur++] = c;
                    s_exp_path_buf[s_exp_path_cur] = '\0';
                }
            }
            break;

        case WM_LBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            /* Top Tabs (nur im Studio-State sichtbar) */
            if (my < 36) {
                for (int t = 0; t < 4; t++) {
                    int x = 10 + t * 200;
                    if (mx >= x && mx < x + 190) {
                        if (t == 0) {
                            /* "Zurueck / Neues Spiel" Button (wie F1) */
                            if (s_gui_state == GUI_STATE_GAMEOVER || s_gui_state == GUI_STATE_MENU)
                                start_game(s_active_mode);
                            else if (s_gui_state != GUI_STATE_LOADING)
                                s_gui_state = GUI_STATE_GAME;
                        } else {
                            s_current_tab = (GuiTab)t;
                            s_gui_state = GUI_STATE_STUDIO;
                        }
                    }
                }
            }
            /* Explorer Klicks */
            if (s_current_tab == TAB_EXPLORER) {
                if (mx >= 20 && mx <= 480 && my >= 108 && my <= WIN_H - 50) {
                    int clicked_idx = s_explorer_scroll + (my - 108) / 18;
                    if (clicked_idx >= 0 && clicked_idx < s_num_explorer_items) {
                        s_explorer_selected = clicked_idx;
                        ExplorerItem *it = &s_explorer_items[s_explorer_selected];
                        if (!it->is_dir) {
                            const char *ext = strrchr(it->name, '.');
                            if (ext && _stricmp(ext, ".mod") == 0) {
                                FILE *f = fopen(it->path, "rb");
                                long size = 0;
                                s_exp_playing = 0;
                                s_exp_mod_playing = 0;
                                if (f) {
                                    fseek(f, 0, SEEK_END);
                                    size = ftell(f);
                                    if (size > (long)sizeof(s_exp_mod_buf)) size = (long)sizeof(s_exp_mod_buf);
                                    fseek(f, 0, SEEK_SET);
                                    (void)fread(s_exp_mod_buf, 1, (size_t)size, f);
                                    fclose(f);
                                }
                                if (size > 1084 && mod_player_load_mem(&s_exp_mod, s_exp_mod_buf, (uint32_t)size)) {
                                    mod_player_start(&s_exp_mod, GAME_AUDIO_RATE);
                                    s_exp_mod_playing = 1;
                                    snprintf(s_status_msg, sizeof(s_status_msg), "MOD geladen: %s", it->name);
                                }
                            } else {
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
                /* Explorer Transport & Subsong Buttons */
                if (mx >= 520 && mx <= 630 && my >= 140 && my <= 170) {
                    if (s_exp_tfmx.loaded) {
                        s_exp_playing = !s_exp_playing;
                        if (s_exp_playing) s_exp_mod_playing = 0;
                        snprintf(s_status_msg, sizeof(s_status_msg), s_exp_playing ? "TFMX Wiederaufnahme" : "TFMX Pausiert");
                    } else if (s_exp_mod.loaded) {
                        s_exp_mod_playing = !s_exp_mod_playing;
                        if (s_exp_mod_playing) s_exp_playing = 0;
                        snprintf(s_status_msg, sizeof(s_status_msg), s_exp_mod_playing ? "MOD Wiederaufnahme" : "MOD Pausiert");
                    }
                } else if (mx >= 640 && mx <= 740 && my >= 140 && my <= 170) {
                    s_exp_playing = 0;
                    s_exp_mod_playing = 0;
                    tfmx_player_stop(&s_exp_tfmx);
                    mod_player_stop(&s_exp_mod);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Wiedergabe angehalten");
                } else if (mx >= 750 && mx <= 870 && my >= 140 && my <= 170) {
                    if (s_exp_tfmx.loaded && s_exp_subsong > 0) {
                        s_exp_subsong--;
                        tfmx_player_start(&s_exp_tfmx, (uint8_t)s_exp_subsong);
                        s_exp_playing = 1;
                        snprintf(s_status_msg, sizeof(s_status_msg), "Subsong %d / %d", s_exp_subsong, s_exp_tfmx.num_subsongs);
                    }
                } else if (mx >= 880 && mx <= 1000 && my >= 140 && my <= 170) {
                    if (s_exp_tfmx.loaded && s_exp_subsong + 1 < s_exp_tfmx.num_subsongs) {
                        s_exp_subsong++;
                        tfmx_player_start(&s_exp_tfmx, (uint8_t)s_exp_subsong);
                        s_exp_playing = 1;
                        snprintf(s_status_msg, sizeof(s_status_msg), "Subsong %d / %d", s_exp_subsong, s_exp_tfmx.num_subsongs);
                    }
                }
                /* Convert Button Klick */
                else if (mx >= 520 && mx <= 870 && my >= 190 && my <= 226) {
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
            /* Strudel Studio Button & Klick-Events */
            if (s_current_tab == TAB_STRUDEL) {
                if (mx >= 50 && mx <= WIN_W - 50 && my >= 125 && my <= 195) {
                    /* Klick in Textbox -> Cursor setzen */
                    int idx = (mx - 65 + 4) / 8;
                    int len = (int)strlen(s_strudel_input);
                    if (idx < 0) idx = 0;
                    if (idx > len) idx = len;
                    s_strudel_cursor = idx;
                } else if (mx >= 50 && mx <= 310 && my >= 215 && my <= 251) {
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
                } else if (mx >= 870 && mx <= 1070 && my >= 215 && my <= 251) {
                    FILE *f = fopen("user_pattern.strudel", "w");
                    if (f) {
                        fputs(s_strudel_input, f);
                        fputc('\n', f);
                        fclose(f);
                        snprintf(s_status_msg, sizeof(s_status_msg), "Gespeichert in: user_pattern.strudel");
                    } else {
                        snprintf(s_status_msg, sizeof(s_status_msg), "Fehler beim Speichern!");
                    }
                }
            }
            break;
        }

        case WM_RBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            if (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_STRUDEL) {
                if (mx >= 50 && mx <= WIN_W - 50 && my >= 125 && my <= 195) {
                    /* Rechtsklick in Textbox -> Einfuegen aus Zwischenablage */
                    if (OpenClipboard(hWnd)) {
                        HANDLE hData = GetClipboardData(CF_TEXT);
                        if (hData) {
                            const char *clip_text = (const char *)GlobalLock(hData);
                            if (clip_text) {
                                while (*clip_text) {
                                    char ch = *clip_text++;
                                    if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
                                    if ((unsigned char)ch >= 32 && (unsigned char)ch <= 126 && strlen(s_strudel_input) < sizeof(s_strudel_input) - 2) {
                                        memmove(&s_strudel_input[s_strudel_cursor + 1], &s_strudel_input[s_strudel_cursor], strlen(&s_strudel_input[s_strudel_cursor]) + 1);
                                        s_strudel_input[s_strudel_cursor] = ch;
                                        s_strudel_cursor++;
                                    }
                                }
                                GlobalUnlock(hData);
                                apply_strudel_code(&s_livecode, s_strudel_input);
                                snprintf(s_status_msg, sizeof(s_status_msg), "Rechtsklick: Aus Zwischenablage eingefuegt!");
                            }
                        }
                        CloseClipboard();
                    }
                }
            }
            break;
        }

        case WM_CLOSE:
            /* X-Button / Alt+F4: sauberes Beenden (frueher nicht behandelt -> Fenster tot) */
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
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
    int dump_mode = 0;  /* --dump [frames] [basis] : Headless-Frame-Dump */
    int dump_frames = 150;
    char dump_base[128] = "dump";

    (void)hPrevInstance;

    /* --dump-Argumente parsen */
    {
        char cmd[512];
        char *tok;
        strncpy(cmd, lpCmdLine ? lpCmdLine : "", sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        tok = strtok(cmd, " \t");
        while (tok) {
            if (_stricmp(tok, "--dump") == 0) {
                dump_mode = 1;
                tok = strtok(NULL, " \t");
                if (tok) dump_frames = atoi(tok);
                tok = strtok(NULL, " \t");
                if (tok) { strncpy(dump_base, tok, sizeof(dump_base) - 1); dump_base[sizeof(dump_base) - 1] = '\0'; }
                break;
            }
            tok = strtok(NULL, " \t");
        }
    }
    if (dump_frames < 10) dump_frames = 10;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "StrudelForceGUI";
    RegisterClassExA(&wc);

    hWnd = CreateWindowExA(
        0, "StrudelForceGUI", "STRUDELFORCE " GAME_VERSION " - Amiga 500 Voxel Engine & Cyber-Paula TFMX Studio",
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
    god_init(&s_god);
    core_init(&s_game, 1234);
    game_audio_init(&s_audio);
    livecode_init(&s_livecode, s_strudel_input);
    if (!dump_mode) win_audio_open(GAME_AUDIO_RATE);
    explorer_scan_directory(s_current_dir);
    {
        /* Jukebox-Library: TFMX-Ordner + MOD-Ordner rekursiv */
        static const char *jukebox_roots[2] = { "TFMX Professional", "MOD" };
        int r;
        for (r = 0; r < 2; r++) playlist_scan_rec(jukebox_roots[r], 0);
    }

    /* Intro: Embedded Apidya Meadow (TFMX + Original-Amiga-Samples) — laeuft im Hauptmenue */
    s_last_wave = s_game.wave;
    music_play_embedded(0);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    /* --- Headless-Frame-Dump-Modus (GUI-Flow: Menue -> Loading -> Spiel -> Game Over) --- */
    if (dump_mode) {
        int i;
        int dumped = 0;
        for (i = 0; i < dump_frames; i++) {
            int seg = (i * 100) / dump_frames;
            s_gui_frame++;
            if (seg < 20) {
                /* Hauptmenue (Punkte durchwaelzen fuer Auswahl-Animation) */
                s_menu_index = (i / 10) % MENU_ITEM_COUNT;
                render_state_menu();
            } else if (seg < 40) {
                /* Loading-Screen (am Segment-Start neues Spiel initialisieren) */
                if (i == dump_frames * 2 / 10) {
                    start_game(MODE_ENDLESS);
                    s_loading_ticks = LOAD_DURATION - 10;  /* Loading laeuft im Segment aus -> Game-Frames */
                }
                s_loading_ticks++;
                if (s_loading_ticks >= LOAD_DURATION) s_gui_state = GUI_STATE_GAME;
                if (s_gui_state == GUI_STATE_LOADING) {
                    render_state_loading();
                } else {
                    render_tab_game();
                    render_status_bar();
                }
            } else {
                /* Spiel: Bot-Input (rechts laufen + feuern) */
                uint16_t mask2 = 0;
                if (s_gui_state != GUI_STATE_GAMEOVER) {
                    mask2 = INPUT_RIGHT;
                    if (((i / 15) % 2) == 0) mask2 |= INPUT_FIRE;
                }
                core_tick(&s_game, mask2);
                livecode_tick(&s_livecode);
                if (s_game.state == STATE_GAMEOVER) s_gui_state = GUI_STATE_GAMEOVER;
                if (seg == 75) s_game.hp = 20;              /* Low-HP-Interlace */
                if (seg == 90) {                            /* Game-Over-Overlay zeigen */
                    s_game.state = STATE_GAMEOVER;
                    s_gui_state = GUI_STATE_GAMEOVER;
                }
                if (s_gui_state == GUI_STATE_GAME || s_gui_state == GUI_STATE_GAMEOVER) {
                    render_tab_game();
                    if (s_gui_state == GUI_STATE_GAMEOVER) render_game_over_overlay();
                    render_status_bar();
                }
            }
            if ((i % 10) == 9 || i == dump_frames - 1) {
                char fn[200];
                snprintf(fn, sizeof(fn), "%s_%03d.ppm", dump_base, dumped++);
                dump_ppm(fn);
            }
        }
        DestroyWindow(hWnd);
        return 0;
    }

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

        /* Controller: Hauptmenue-Navigation (Edge-getriggert, einmal pro Frame) */
        pad_menu_poll();

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

                /* Tastatur- + Controller-Eingaben (nur im aktiven Spiel-State)
                 * Steuerung (v0.1): WASD/Pfeile = bewegen, Leertaste = Sprung,
                 * STRG = Feuer, ALT = Halt-Strahl, C = Waffe, T = Truhe, S = Morph */
                mask = 0;
                if (s_gui_state == GUI_STATE_GAME) {
                    if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)  mask |= INPUT_LEFT;
                    if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000) mask |= INPUT_RIGHT;
                    if (GetAsyncKeyState('W') & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000 || GetAsyncKeyState(VK_SPACE) & 0x8000) mask |= INPUT_UP;
                    if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState(VK_DOWN) & 0x8000)  mask |= INPUT_DOWN;
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mask |= INPUT_FIRE;    /* Strg: prim. Feuer */
                    if (GetAsyncKeyState(VK_MENU) & 0x8000)    mask |= INPUT_SECOND;  /* Alt:  Halt-Strahl */
                    if (GetAsyncKeyState('C') & 0x8000) mask |= INPUT_WEAPSW;
                    if (GetAsyncKeyState('T') & 0x8000) livecode_open_chest(&s_livecode);

                    /* --- Controller (XInput, Gamepad 0) ---
                     * Stick/D-Pad = bewegen, A = Feuer, X = Beam, B = Morph,
                     * RS = Waffe, START = Hauptmenue (Edge) */
                    {
                        XINPUT_STATE xs;
                        if (XInputGetState(0, &xs) == ERROR_SUCCESS) {
                            s_pad_connected = 1;
                            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0  || xs.Gamepad.sThumbLX < -4000) mask |= INPUT_LEFT;
                            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0 || xs.Gamepad.sThumbLX >  4000) mask |= INPUT_RIGHT;
                            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0    || xs.Gamepad.sThumbLY >  4000) mask |= INPUT_UP;
                            if ((xs.Gamepad.wButtons & (XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_B)) != 0 ||
                                xs.Gamepad.sThumbLY < -4000)                                                 mask |= INPUT_DOWN;
                            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0)                mask |= INPUT_FIRE;
                            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0)                mask |= INPUT_SECOND;
                            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0)   mask |= INPUT_WEAPSW;
                            {
                                int start_now = ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0);
                                if (start_now && !s_pad_start_prev) s_gui_state = GUI_STATE_MENU;
                                s_pad_start_prev = start_now;
                            }
                        } else {
                            s_pad_connected = 0;
                            s_pad_start_prev = 0;
                        }
                    }

                    core_tick(&s_game, mask);
                    livecode_tick(&s_livecode);

                    /* Alle Leben verbraucht? -> Game-Over-Screen.
                     * Kampagne vollstaendig? -> Sieg-Screen (gleiche End-State-Maschine). */
                    if (s_game.state == STATE_GAMEOVER || s_game.state == STATE_VICTORY) {
                        s_gui_state = GUI_STATE_GAMEOVER;
                    }

                    /* Neue Wave/Level -> zufaelliger Song aus der Library (TFMX + MOD) */
                    if (s_game.state != STATE_GAMEOVER && s_game.wave != s_last_wave) {
                        s_last_wave = s_game.wave;
                        music_play_next();
                    }

                    if (s_game.state != STATE_GAMEOVER && s_mod_active) {
                        s_audio.ext_render = NULL;
                    } else if (s_game.state != STATE_GAMEOVER && s_tfmx_active) {
                        tfmx_player_tick(&s_tfmx, &s_audio);
                    } else {
                        /* 8-Voice-Engine (im Game-Over/Menu-Ambient weiter laufen) */
                        s_audio.ext_render = NULL;
                        s_audio.bass_override_active = 1;
                        s_audio.bass_override_note = livecode_current_note(&s_livecode);
                        game_audio_update(&s_audio, &s_game);
                    }
                } else if (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_EXPLORER) {
                    if (s_exp_playing) {
                        tfmx_player_tick(&s_exp_tfmx, &s_audio);
                    } else if (s_exp_mod_playing) {
                        s_audio.ext_render = NULL;
                    } else {
                        s_audio.ext_render = NULL;
                    }
                } else {
                    /* Menu-/Loading-/Game-Over: Jukebox-Track (auch Embedded-Intro) weiter laufen */
                    int menu_music = (s_gui_state != GUI_STATE_STUDIO);
                    livecode_tick(&s_livecode);
                    if (menu_music && s_mod_active) {
                        s_audio.ext_render = NULL;
                    } else if (menu_music && s_tfmx_active) {
                        tfmx_player_tick(&s_tfmx, &s_audio);
                    } else {
                        /* 8-Voice-Engine (Studio-Ambient / Fallback) */
                        s_audio.ext_render = NULL;
                        s_audio.bass_override_active = 1;
                        s_audio.bass_override_note = livecode_current_note(&s_livecode);
                        game_audio_update(&s_audio, &s_game);
                    }
                }

                /* 16574 Hz Audio rendern & in den Transport-Ring legen
                 * (MOD-Jukebox: im Spiel UND in Menu/Loading/Game-Over, nicht im Studio) */
                if (s_mod_active && s_gui_state != GUI_STATE_STUDIO) {
                    mod_player_render(&s_mod, abuf, WIN_AUDIO_BUF_SAMPLES);
                } else if (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_EXPLORER && s_exp_mod_playing) {
                    mod_player_render(&s_exp_mod, abuf, WIN_AUDIO_BUF_SAMPLES);
                } else {
                    game_audio_render(&s_audio, abuf, WIN_AUDIO_BUF_SAMPLES);
                }
                /* Soundgodizer (Stereo) oder Mono-Duplizierung */
                if (s_god.enabled) {
                    god_process(&s_god, abuf, WIN_AUDIO_BUF_SAMPLES, s_god_buf);
                    win_audio_write_frames(s_god_buf, WIN_AUDIO_BUF_SAMPLES);
                } else {
                    win_audio_write(abuf, WIN_AUDIO_BUF_SAMPLES);
                }
            }
            if (s_acc_ms >= 20) s_acc_ms = 0; /* Rest-Backlog verwerfen */

            /* Frame rendern (nur wenn mindestens 1 Tick gelaufen ist) */
            if (ticks_done > 0) {
                s_gui_frame++;
                if (s_gui_state == GUI_STATE_MENU) {
                    render_state_menu();
                } else if (s_gui_state == GUI_STATE_LOADING) {
                    s_loading_ticks++;
                    if (s_loading_ticks >= LOAD_DURATION) s_gui_state = GUI_STATE_GAME;
                    render_state_loading();
                } else if (s_gui_state == GUI_STATE_CREDITS) {
                    render_state_credits();
                } else if (s_gui_state == GUI_STATE_GAME || s_gui_state == GUI_STATE_GAMEOVER) {
                    render_tab_game();
                    if (s_gui_state == GUI_STATE_GAMEOVER) {
                        if (s_game.state == STATE_VICTORY) render_victory_overlay();
                        else render_game_over_overlay();
                    } else if (s_game.state == STATE_LEVEL_CLEAR) {
                        render_level_clear_overlay();
                    }
                    render_status_bar();
                } else {
                    /* STUDIO: Jukebox / Strudel / Intro (Top-Bar + Status) */
                    if (s_current_tab == TAB_EXPLORER) {
                        render_tab_explorer();
                    } else if (s_current_tab == TAB_STRUDEL) {
                        render_tab_strudel();
                    } else {
                        render_tab_intro();
                    }
                    render_top_bar();
                    render_status_bar();
                }

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
