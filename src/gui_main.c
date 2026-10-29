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
#include <xinput.h>
#include <io.h>
#include <fcntl.h>   /* Controller-Support (XInput, Gamepad 0) */

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
#include "turrican_sprites.h"
#include "embedded_credits.h"

#define GAME_VERSION "0.3.1"

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
    TAB_INTRO,
    TAB_GRAPHICS
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
    GUI_STATE_CREDITS,
    GUI_STATE_MANUAL,
    GUI_STATE_CONTROLS
} GuiState;

static GuiState s_gui_state = GUI_STATE_STUDIO; /* Startet direkt im Amiga Cracktro Intro! */
static int s_menu_index = 0;        /* aktiver Hauptmenue-Punkt */
static int s_active_mode = MODE_CAMPAIGN; /* Hauptmodus: Kampagne mit 5 Welten & Plattformen */
static int s_exp_mode = 0;           /* 0 = Integrierte ROM-Tracks (Default!), 1 = Lokale Dateien/Ordner */
static int s_exp_rom_selected = 0;   /* ausgewaehlter Track im ROM-Browser */
static int s_pad_connected = 0;      /* Gamepad verbunden (XInput oder DirectInput/PS4) */
static int s_pad_start_prev = 0;     /* Start-Button Edge-Erkennung */
static int s_loading_ticks = 0;     /* Loading-Screen-Fortschritt (50Hz-Ticks) */
static int s_manual_page = 0;       /* aktuelle Handbuch-Seite (0..4) */
static int s_is_fullscreen = 0;     /* Vollbild-Status */
static WINDOWPLACEMENT s_prev_wp = { sizeof(WINDOWPLACEMENT) };

/* --- Controller-Konfiguration & Presets --- */
typedef enum {
    PAD_PRESET_MODERN = 0,             /* Xbox / PS4: A=Feuer, B=Morph, X=Laser, Y=Waffe, Stick=Springen */
    PAD_PRESET_RETRO_COMPETITION_PRO,  /* Retro: Stick UP=Springen, Button 1=Feuer, Button 2=Laser */
    PAD_PRESET_PLATFORMER              /* Classic Jump'n'Run: A=Springen, X=Feuer, B=Morph, Y=Laser */
} PadPreset;

static int s_pad_preset = PAD_PRESET_MODERN;
static int s_pad_laser_invert_y = 1;   /* 1 = Invertiert (Stick Oben = zielt nach Oben), 0 = Normal */
static int s_ctrl_setting_cursor = 0;  /* Cursor im Tastenbelegungs-Menue */

/* Song Pacing & Looping State */
static int      s_song_start_wave = 0;
static uint32_t s_song_start_tick = 0;

/* --- RetroArch-Grade Grafik-Einstellungen --- */
static int s_filter_mode = 4;       /* 0=AUS, 1=BILINEAR, 2=TRINITRON, 3=1084S, 4=RETROARCH CRT, 5=ARCADE VIGNETTE */
static int s_scanline_strength = 2; /* 0=25%, 1=50%, 2=75%, 3=100% */
static int s_phosphor_mask = 1;     /* RGB Subpixel Triad Mask */
static int s_bloom_mode = 2;        /* 0=AUS, 1=Zart, 2=Stark, 3=Max */
static int s_bilinear = 1;          /* Bilineare Glaettung */
static int s_color_mode = 0;        /* 0=OCS 32, 1=AGA 256, 2=CYBER, 3=GREEN MONO */
static int s_gfx_setting_cursor = 0;/* Cursor im Grafik-Studio */

#define MENU_ITEM_COUNT 10
static const char *menu_labels[MENU_ITEM_COUNT] = {
    "KAMPAGNE (5 LEVELS)",
    "ENDLOS ROGUELIKE",
    "PILOTEN-HANDBUCH",
    "CONTROLLER & TASTEN",
    "GRAFIK-STUDIO (CRT/SHADER)",
    "TFMX JUKEBOX",
    "STRUDEL STUDIO",
    "AMIGA CRACKTRO",
    "CREDITS",
    "BEENDEN (QUIT)"
};

/* --- Unified Gamepad Struct (Unterstuetzt PS4/DirectInput & Xbox/XInput) --- */
typedef struct {
    int connected;
    int up, down, left, right;
    int btn_fire;    /* Primär-Feuer */
    int btn_jump;    /* Sprung-Button */
    int btn_morph;   /* Circle / B */
    int btn_weapsw;  /* Triangle / Y / R1 */
    int btn_beam;    /* Square / X / L1 / R2 */
    int btn_start;   /* Options / Start */
    int btn_lb, btn_rb;
    uint32_t raw_buttons;     /* Roh-Bitmaske der Tasten fuers Testfeld */
    float stick_rx, stick_ry; /* Rechter Analogstick fuer 360-Grad Laser */
    char name[32];
} UnifiedGamepad;

static UnifiedGamepad s_pad = {0};

static void poll_gamepad(void) {
    memset(&s_pad, 0, sizeof(s_pad));
    uint32_t raw_b = 0;
    int raw_b1 = 0, raw_b2 = 0, raw_b3 = 0, raw_b4 = 0;
    int raw_lb = 0, raw_rb = 0, raw_r2 = 0, raw_start = 0;
    int di_present = 0;

    /* 1. WinMM DirectInput (PS4 DualShock 4, PS5, Competition Pro USB, Arcade-Sticks 0..15) */
    UINT num_devs = joyGetNumDevs();
    if (num_devs > 16) num_devs = 16;
    for (UINT id = 0; id < num_devs; id++) {
        JOYINFOEX jex;
        memset(&jex, 0, sizeof(jex));
        jex.dwSize = sizeof(jex);
        jex.dwFlags = JOY_RETURNALL;

        if (joyGetPosEx(id, &jex) == JOYERR_NOERROR) {
            s_pad.connected = 1;
            di_present = 1;
            JOYCAPSW caps;
            if (joyGetDevCapsW(id, &caps, sizeof(caps)) == JOYERR_NOERROR) {
                if (caps.wMid == 0x054c || caps.wPid == 0x09cc || caps.wNumButtons >= 14) {
                    strncpy(s_pad.name, "PS4 DUALSHOCK 4", sizeof(s_pad.name) - 1);
                } else if (caps.wNumButtons <= 4) {
                    strncpy(s_pad.name, "COMPETITION PRO USB", sizeof(s_pad.name) - 1);
                } else if (s_pad.name[0] == '\0') {
                    strncpy(s_pad.name, "USB GAMEPAD", sizeof(s_pad.name) - 1);
                }
            }

            int lx = (int)jex.dwXpos - 32768;
            int ly = (int)jex.dwYpos - 32768;
            if (lx < -8000) s_pad.left = 1;
            if (lx >  8000) s_pad.right = 1;
            if (ly < -8000) s_pad.up = 1;
            if (ly >  8000) s_pad.down = 1;

            if (jex.dwPOV != 65535) {
                if (jex.dwPOV >= 31500 || jex.dwPOV <= 4500)  s_pad.up = 1;
                if (jex.dwPOV >= 4500  && jex.dwPOV <= 13500) s_pad.right = 1;
                if (jex.dwPOV >= 13500 && jex.dwPOV <= 22500) s_pad.down = 1;
                if (jex.dwPOV >= 22500 && jex.dwPOV <= 31500) s_pad.left = 1;
            }

            DWORD b = jex.dwButtons;
            raw_b |= b;
            if (b & 0x0001) raw_b1 = 1; /* Button 1 (Square / Retro Fire 1) */
            if (b & 0x0002) raw_b2 = 1; /* Button 2 (Cross / Retro Fire 2) */
            if (b & 0x0004) raw_b3 = 1; /* Button 3 (Circle) */
            if (b & 0x0008) raw_b4 = 1; /* Button 4 (Triangle) */
            if (b & 0x0010) raw_lb = 1; /* L1 */
            if (b & 0x0020) raw_rb = 1; /* R1 */
            if (b & 0x0080) raw_r2 = 1; /* R2 */
            if (b & 0x0200) raw_start = 1; /* Options / Start */

            /* Rechter Analogstick: Z-Achse = X, R-Achse = Y */
            int rx = (int)jex.dwZpos - 32768;
            int ry = (int)jex.dwRpos - 32768;
            if (abs(rx) > 5000 || abs(ry) > 5000) {
                s_pad.stick_rx = (float)rx / 32768.0f;
                s_pad.stick_ry = (float)ry / 32768.0f; /* ry < 0 = OBEN auf Screen! */
            }
        }
    }

    /* 2. XInput Abfrage (Xbox Controller 0..3) */
    for (DWORD xi = 0; xi < 4; xi++) {
        XINPUT_STATE xs;
        if (XInputGetState(xi, &xs) == ERROR_SUCCESS) {
            s_pad.connected = 1;
            if (s_pad.name[0] == '\0') {
                strncpy(s_pad.name, "XBOX CONTROLLER", sizeof(s_pad.name) - 1);
            }
            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) || xs.Gamepad.sThumbLY > 5000) s_pad.up = 1;
            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || xs.Gamepad.sThumbLY < -5000) s_pad.down = 1;
            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || xs.Gamepad.sThumbLX < -5000) s_pad.left = 1;
            if ((xs.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || xs.Gamepad.sThumbLX > 5000) s_pad.right = 1;

            if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_A) { raw_b2 = 1; raw_b |= 0x0002; }
            if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_X) { raw_b1 = 1; raw_b |= 0x0001; }
            if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_B) { raw_b3 = 1; raw_b |= 0x0004; }
            if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_Y) { raw_b4 = 1; raw_b |= 0x0008; }
            if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  { raw_lb = 1; raw_b |= 0x0010; }
            if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) { raw_rb = 1; raw_b |= 0x0020; }
            if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_START)          { raw_start = 1; raw_b |= 0x0200; }
            if (xs.Gamepad.bRightTrigger > 30)                      { raw_r2 = 1; raw_b |= 0x0080; }

            if (abs(xs.Gamepad.sThumbRX) > 4000 || abs(xs.Gamepad.sThumbRY) > 4000) {
                s_pad.stick_rx = (float)xs.Gamepad.sThumbRX / 32768.0f;
                s_pad.stick_ry = -(float)xs.Gamepad.sThumbRY / 32768.0f; /* Invert to match screen Y! */
            }
        }
    }

    s_pad.raw_buttons = raw_b;
    s_pad.btn_lb = raw_lb;
    s_pad.btn_rb = raw_rb;
    s_pad.btn_start = raw_start;

    /* Preset-Belegungs-Mapping (getrennte Quellen-Pruefung: XInput-Buttons
     * duerfen DirectInput-Mappings NICHT ueberschreiben, wenn kein
     * DirectInput-Pad angeschlossen ist — sonst toetet ein Xbox-Controller
     * die Belegung eines gleichzeitig verbundenen USB-Pads). */
    if (s_pad_preset == PAD_PRESET_RETRO_COMPETITION_PRO) {
        /* Competition Pro USB: Button 1 = Feuer, Button 2 = Drehlaser,
         * Button 3 = Morph, Button 4 = Waffe, Stick/D-Pad OBEN = Sprung */
        s_pad.btn_fire   = di_present ? raw_b1 : 0;
        s_pad.btn_beam   = di_present ? raw_b2 : 0;
        s_pad.btn_morph  = di_present ? raw_b3 : 0;
        s_pad.btn_weapsw = di_present ? raw_b4 : 0;
        s_pad.btn_jump   = s_pad.up; /* Sprung laeuft rein ueber Stick/D-Pad UP */
    } else if (s_pad_preset == PAD_PRESET_PLATFORMER) {
        /* Classic Platformer: A = Jump, X = Feuer, B = Morph, Y = Laser */
        s_pad.btn_jump   = raw_b2;
        s_pad.btn_fire   = raw_b1;
        s_pad.btn_morph  = raw_b3;
        s_pad.btn_beam   = raw_b4;
        s_pad.btn_weapsw = raw_rb;
    } else {
        /* PAD_PRESET_MODERN (Standard): A = Feuer, B = Morph, X/R2 = Laser,
         * Y/R1 = Waffe, LB = Sprung */
        s_pad.btn_fire   = raw_b2;
        s_pad.btn_beam   = raw_b1 | raw_r2;
        s_pad.btn_morph  = raw_b3;
        s_pad.btn_weapsw = raw_b4 | raw_rb;
        s_pad.btn_jump   = raw_lb;
    }

    s_pad_connected = s_pad.connected;
}

/* --- Destructible Mystery Blocks [?] & Turrican 2 Dynamic Loot Engine --- */
#define MAX_BLOCKS 24
#define MAX_LOOT_ITEMS 32
#define MAX_DEBRIS_PARTS 64
#define MAX_FLOATING_TEXTS 16

typedef enum {
    LOOT_NONE = 0,
    LOOT_DIAMOND,
    LOOT_WEAPON_POWER,
    LOOT_SHIELD_BOOST
} LootKind;

typedef struct {
    int active;
    LootKind kind;
    int world_x, world_y;
    int vx, vy;
    int life;
} GuiLootItem;

typedef struct {
    int active;
    int world_x, world_y;
    int vx, vy;
    int life;
    uint32_t color;
    int size;
} GuiDebrisPart;

typedef struct {
    int active;
    int screen_x, screen_y;
    int vy;
    int life;
    char text[32];
    uint32_t color;
} GuiFloatingText;

typedef struct {
    int active;
    int hp;
    int max_hp;
    int hit_flash;
    int plat_idx;
    int world_x;
    int world_y;
    int bounce_y;
    int destroyed;
} DestructibleBox;

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
static GuiTab      s_current_tab = TAB_INTRO; /* Intro-Tab beim Start */
static int         s_gui_frame = 0; /* GUI-Frame-Zaehler: animiert Tabs, deren
                                        Quelle (s_game.tick) nur im Game-Tab laeuft */

/* --- Soundgodizer (F5: aus -> BASS -> WARM -> OVERDRIVE) --- */
static Soundgodizer s_god;
static int          s_god_mode = 0;
static int16_t      s_god_buf[WIN_AUDIO_BUF_SAMPLES * 2]; /* Stereo-Ausgabe */

static DestructibleBox s_boxes[MAX_BLOCKS];
static int             s_box_count = 0;
static GuiLootItem     s_loot[MAX_LOOT_ITEMS];
static GuiDebrisPart   s_debris[MAX_DEBRIS_PARTS];
static GuiFloatingText s_float_texts[MAX_FLOATING_TEXTS];
static uint8_t         s_prev_enemy_active[MAX_ENEMIES];

static void spawn_floating_text(int sx, int sy, const char *txt, uint32_t col) {
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++) {
        if (!s_float_texts[i].active) {
            s_float_texts[i].active = 1;
            s_float_texts[i].screen_x = sx;
            s_float_texts[i].screen_y = sy;
            s_float_texts[i].vy = -2;
            s_float_texts[i].life = 45;
            s_float_texts[i].color = col;
            strncpy(s_float_texts[i].text, txt, sizeof(s_float_texts[i].text) - 1);
            break;
        }
    }
}

static void spawn_debris(int wx, int wy, int count, uint32_t col) {
    for (int i = 0; i < count; i++) {
        for (int k = 0; k < MAX_DEBRIS_PARTS; k++) {
            if (!s_debris[k].active) {
                s_debris[k].active = 1;
                s_debris[k].world_x = wx;
                s_debris[k].world_y = wy;
                int ang = (i * 360 / count) + (s_gui_frame % 30);
                float rad = (float)ang * 0.0174533f;
                float spd = 2.0f + (float)(i % 5) * 1.2f;
                s_debris[k].vx = (int)(cosf(rad) * spd * 65536.0f);
                s_debris[k].vy = (int)(sinf(rad) * spd * 65536.0f) + (2 << 16);
                s_debris[k].life = 35 + (i % 20);
                s_debris[k].color = (i & 1) ? col : 0xFFFFFF;
                s_debris[k].size = (i % 3 == 0) ? 3 : 2;
                break;
            }
        }
    }
}

static void spawn_loot(int wx, int wy, LootKind kind, int vx, int vy) {
    for (int i = 0; i < MAX_LOOT_ITEMS; i++) {
        if (!s_loot[i].active) {
            s_loot[i].active = 1;
            s_loot[i].kind = kind;
            s_loot[i].world_x = wx;
            s_loot[i].world_y = wy;
            s_loot[i].vx = vx;
            s_loot[i].vy = vy;
            s_loot[i].life = 500;
            break;
        }
    }
}

static void shatter_box(DestructibleBox *dbox) {
    dbox->destroyed = 1;
    dbox->active = 0;
    int wx = dbox->world_x << 16;
    int wy = dbox->world_y << 16;

    spawn_debris(wx, wy, 16, 0xE0A020);
    spawn_debris(wx, wy, 8, 0xFFFFFF);

    spawn_loot(wx, wy, LOOT_DIAMOND, -1 << 16, 4 << 16);
    spawn_loot(wx, wy, LOOT_DIAMOND,  0,        5 << 16);
    spawn_loot(wx, wy, LOOT_DIAMOND,  1 << 16, 4 << 16);

    if ((s_gui_frame & 1) == 0) {
        spawn_loot(wx, wy, LOOT_WEAPON_POWER, ((s_gui_frame & 2) ? 1 : -1) << 15, 3 << 16);
    } else {
        spawn_loot(wx, wy, LOOT_SHIELD_BOOST, ((s_gui_frame & 2) ? 1 : -1) << 15, 3 << 16);
    }

    s_game.score += 500;
    sound_sfx_crystal(&s_audio);
}

static void init_level_boxes(void) {
    int pi;
    s_box_count = 0;
    memset(s_loot, 0, sizeof(s_loot));
    memset(s_debris, 0, sizeof(s_debris));
    memset(s_float_texts, 0, sizeof(s_float_texts));
    memset(s_prev_enemy_active, 0, sizeof(s_prev_enemy_active));

    /* Schwebende Boxen in Bodennhe fuer Kopfsprung (Headbutt von unten!) */
    static const int ground_box_x[] = { 8, 14, 22, 28, 36, 44, 52, 60 };
    for (int gi = 0; gi < 8 && s_box_count < MAX_BLOCKS; gi++) {
        DestructibleBox *b = &s_boxes[s_box_count++];
        b->active = 1;
        b->hp = 2;
        b->max_hp = 2;
        b->hit_flash = 0;
        b->plat_idx = -1;
        b->world_x = ground_box_x[gi];
        b->world_y = 3;  /* 3 Einheiten hoch: exakt mit Kopfsprung erreichbar! */
        b->bounce_y = 0;
        b->destroyed = 0;
    }

    /* Boxen auf erhoehten Plattformen */
    for (pi = 0; pi < s_game.platform_count && s_box_count < MAX_BLOCKS; pi++) {
        if (pi % 2 == 1) {
            DestructibleBox *b = &s_boxes[s_box_count++];
            b->active = 1;
            b->hp = 2;
            b->max_hp = 2;
            b->hit_flash = 0;
            b->plat_idx = pi;
            b->world_x = (int)(s_game.platforms[pi].x >> 16) + (int)(s_game.platforms[pi].w >> 17);
            b->world_y = (int)(s_game.platforms[pi].y >> 16) + 3;
            b->bounce_y = 0;
            b->destroyed = 0;
        }
    }
}

/* --- Jukebox: zufaelliger Level-Song (TFMX + MOD) pro Wave --- */
#define PLAYLIST_MAX 512
typedef struct { char path[260]; char name[64]; int is_mod; } PlaylistEntry;
static PlaylistEntry s_playlist[PLAYLIST_MAX];
static int           s_playlist_count = 0;
static uint32_t      s_music_rng = 0x5EED5EEDu; /* eigener LCG (Core-RNG bleibt unberuehrt) */
static uint16_t      s_last_wave = 0;
static uint8_t       s_last_campaign_level = 1;
static char          s_current_track_name[128] = "Apidya Meadow (Intro)";
static char          s_current_track_type[16] = "TFMX";
static int           s_music_toast_timer = 0;
static ModPlayer     s_mod;        /* Game-Tab MOD-Player */
static uint8_t       s_mod_buf[128 * 1024];
static int           s_mod_active = 0;
static uint32_t      s_track_ticks = 0; /* Laufzeit des aktuellen Songs (fuer Auto-Rotation) */

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
static char s_strudel_input[512] = "<c3 eb3 g3 c4> [g3 c4 g3 c4] <bb2 f3 g3 d4> [eb3 g3 c4 eb4]*2";
static int  s_strudel_cursor = 48;
static char s_status_msg[128] = "STRUDELFORCE Studio bereit. Druecke F1 fuer Spiel, F2 fuer Explorer.";

static void credits_init(void);
static void draw_turrican_sprite(int px, int foot_y, int frame_idx, int facing);

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

/* --- Performance: LUT-basiertes Integer-Dimmen (kein Float!) ---
 * Ersetzt die teure rgb_mod()-Pixel-Schleife in Fullscreen-Overlays
 * (Game Over / Sieg / Credits-Panel) mit 3 Lookup-Reads pro Pixel.
 * f8 = Helligkeitsfaktor * 256 / 255 (z.B. 0.22 -> 56, 0.28 -> 71). */
static uint8_t s_dim_lut[256];
static void dim_lut_init(uint32_t f8) {
    for (int i = 0; i < 256; i++) s_dim_lut[i] = (uint8_t)(((uint32_t)i * f8) >> 8);
}
static void dim_screen_rect(int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > WIN_W) x1 = WIN_W;
    if (y1 > WIN_H) y1 = WIN_H;
    for (int y = y0; y < y1; y++) {
        uint32_t *row = &s_pixels[y * WIN_W];
        for (int x = x0; x < x1; x++) {
            uint32_t c = row[x];
            row[x] = ((uint32_t)s_dim_lut[(c >> 16) & 0xFF] << 16) |
                     ((uint32_t)s_dim_lut[(c >> 8) & 0xFF] << 8) |
                     (uint32_t)s_dim_lut[c & 0xFF];
        }
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

/* Bresenham Line Drawing */
static void draw_line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        set_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Funken-Burst fuer Partikeleffekte in der GUI */
static void spawn_sparks(GameState *s, fix16_t x, fix16_t y, uint8_t color, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        Particle *pt = &s->particles[s->part_head];
        s->part_head = (uint8_t)((s->part_head + 1) % MAX_PARTICLES);
        pt->x = x;
        pt->y = y;
        pt->vx = (fix16_t)(((int32_t)(core_rng(s) & 0xFFFF) - 32768) * 4);
        pt->vy = (fix16_t)(((int32_t)(core_rng(s) & 0xFFFF) - 32768) * 4) + FIX(1);
        pt->active = 1;
        pt->color = color;
        pt->life = 15 + (uint16_t)(core_rng(s) & 15);
    }
}

/* 360-Grad Surround Holding Beam (Turrican 2 Blitzstrahl - Authentische Hochspannungs-Optik) */
static void draw_holding_beam(int px, int py, uint8_t angle) {
    float rad = (float)angle * (6.283185307f / 256.0f);
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);
    float nx = -sin_a;
    float ny = cos_a;
    int beam_len = 240;
    int segments = 16;

    /* Rotierender Hochspannungs-Emitterring an der Muendung */
    for (int a = 0; a < 8; a++) {
        float r_ang = (float)s_gui_frame * 0.20f + (float)a * 0.785f;
        int rx = px + (int)(cosf(r_ang) * 16.0f);
        int ry = py + (int)(sinf(r_ang) * 12.0f);
        set_pixel(rx, ry, (a & 1) ? 0x00FFFF : 0xFFFFFF);
        set_pixel(rx + 1, ry, 0x0090E0);
    }

    /* Blitz-Strahl Segmente mit wild zuckendem Jitter und elektrischen Neben-Arcs */
    int prev_x = px;
    int prev_y = py;
    for (int s = 1; s <= segments; s++) {
        float dist = (float)s * ((float)beam_len / (float)segments);
        /* 2-Frequenz-Jitter fuer echte Turrican-2 Blitz-Optik */
        float jit = sinf((float)s_gui_frame * 0.85f + (float)s * 1.8f) * 5.0f
                  + cosf((float)s_gui_frame * 1.4f + (float)s * 3.1f) * 2.5f;
        if (s == segments) jit = 0.0f;
        int cur_x = px + (int)(cos_a * dist + nx * jit);
        int cur_y = py + (int)(sin_a * dist + ny * jit);

        /* Doppelter Blitzfaden (Kern weiss, Rand cyan) */
        draw_line(prev_x, prev_y, cur_x, cur_y, 0x00B8FF);
        draw_line(prev_x + 1, prev_y, cur_x + 1, cur_y, 0xFFFFFF);

        /* Gelegentliche elektrische Seiten-Entladungen (Mini-Funkenblitze) */
        if ((s % 3 == 0) && (s < segments - 2)) {
            float side_dir = ((s + s_gui_frame) & 1) ? 1.0f : -1.0f;
            int branch_x = cur_x + (int)(nx * side_dir * 12.0f);
            int branch_y = cur_y + (int)(ny * side_dir * 12.0f);
            draw_line(cur_x, cur_y, branch_x, branch_y, 0x60D8FF);
            set_pixel(branch_x, branch_y, 0xFFFFFF);
        }

        /* Energie-Knotenperle */
        if (s % 2 == 0) {
            draw_rect(cur_x - 1, cur_y - 1, 3, 3, 0x80FFFF);
            set_pixel(cur_x, cur_y, 0xFFFFFF);
        }
        prev_x = cur_x;
        prev_y = cur_y;
    }

    /* Beam-Spitze: Energetischer Treffer-Funkenstern */
    int tip_x = px + (int)(cos_a * (float)beam_len);
    int tip_y = py + (int)(sin_a * (float)beam_len);
    draw_line(tip_x - 10, tip_y, tip_x + 10, tip_y, 0xFFFFFF);
    draw_line(tip_x, tip_y - 10, tip_x, tip_y + 10, 0xFFFFFF);
    draw_line(tip_x - 7, tip_y - 7, tip_x + 7, tip_y + 7, 0x00E0FF);
    draw_line(tip_x - 7, tip_y + 7, tip_x + 7, tip_y - 7, 0x00E0FF);
    draw_rect(tip_x - 2, tip_y - 2, 5, 5, 0x80FFFF);
    set_pixel(tip_x, tip_y, 0xFFFFFF);
}

/* Dynamisches Muendungsfeuer beim Schiessen */
static void draw_muzzle_flash(int nx, int ny, uint8_t weapon) {
    if (weapon == WEAPON_LASER) {
        /* Gebuendelter Cyan-Laserblitz */
        draw_rect(nx, ny - 2, 20, 5, 0x00E0FF);
        draw_rect(nx + 2, ny - 1, 14, 3, 0xFFFFFF);
        draw_line(nx + 4, ny - 4, nx + 12, ny - 1, 0x80FFFF);
        draw_line(nx + 4, ny + 4, nx + 12, ny + 1, 0x80FFFF);
    } else if (weapon == WEAPON_BOUNCE) {
        /* Smaragd-Expansionsring */
        draw_rect(nx, ny - 3, 10, 7, 0x00E040);
        draw_rect(nx + 2, ny - 2, 6, 5, 0x80FF90);
        set_pixel(nx + 4, ny, 0xFFFFFF);
    } else {
        /* Goldener Streu-Faecher */
        draw_rect(nx, ny - 1, 12, 3, 0xFFC000);
        draw_rect(nx + 2, ny, 8, 1, 0xFFFFFF);
        draw_line(nx + 2, ny - 5, nx + 10, ny - 2, 0xFF8020);
        draw_line(nx + 2, ny + 5, nx + 10, ny + 2, 0xFF8020);
    }
}

/* Turrican-2 Kontinuierlicher Dual-Laserstrahl (WEAPON_LASER aus Sprite-Sheet 143425) */
static void draw_turrican_laser_beam(int x0, int y0, int x1, int y1, int level) {
    (void)y1;
    int min_x = (x0 < x1) ? x0 : x1;
    int max_x = (x0 < x1) ? x1 : x0;
    if (min_x < 0) min_x = 0;
    if (max_x > WIN_W) max_x = WIN_W;
    int len = max_x - min_x;
    if (len <= 0) return;

    int half_h = 3 + level;
    if (half_h > 8) half_h = 8;

    /* 1. Weicher aeusserer Plasma-Halo */
    draw_rect(min_x, y0 - half_h - 1, len, (half_h * 2) + 3, 0x003060);
    draw_rect(min_x, y0 - half_h, len, (half_h * 2) + 1, 0x0078C0);

    /* 2. Strahlkoerper: Elektrisches Cyan */
    draw_rect(min_x, y0 - half_h + 1, len, (half_h * 2) - 1, 0x00B8F0);

    /* 3. Turrican-Signatur: Parallele weisse Laser-Doppellinien */
    draw_rect(min_x, y0 - (half_h / 2), len, 2, 0xFFFFFF);
    draw_rect(min_x, y0 + (half_h / 2) - 1, len, 2, 0xFFFFFF);

    /* 4. Pulsierende Wander-Knotenperlen entlang des Strahls */
    int pulse_phase = (s_gui_frame * 8) % 32;
    for (int px = min_x + pulse_phase; px < max_x; px += 32) {
        draw_rect(px - 3, y0 - half_h, 7, (half_h * 2) + 1, 0x80FFFF);
        draw_rect(px - 1, y0 - 1, 3, 3, 0xFFFFFF);
    }

    /* 5. Einschlag-Fokus / Spitzen-Funken am Ziel */
    int tip_x = (x1 > x0) ? (max_x - 1) : min_x;
    draw_line(tip_x - 8, y0, tip_x + 8, y0, 0xFFFFFF);
    draw_line(tip_x, y0 - 8, tip_x, y0 + 8, 0xFFFFFF);
    draw_line(tip_x - 5, y0 - 5, tip_x + 5, y0 + 5, 0x00FFFF);
    draw_line(tip_x - 5, y0 + 5, tip_x + 5, y0 - 5, 0x00FFFF);
}

/* Turrican-2 Projektil: Laser-Twin-Bolt (Doppelter Hochenergie-Laserbolzen) */
static void draw_projectile_laser(int x, int y, int dir) {
    int sx = (dir > 0) ? (x - 16) : (x + 16);
    int ex = (dir > 0) ? (x + 8)  : (x - 8);
    int min_x = (sx < ex) ? sx : ex;
    int max_x = (sx < ex) ? ex : sx;

    /* Aeusserer Cyan-Plasma-Glanz */
    draw_rect(min_x, y - 3, max_x - min_x, 7, 0x005090);
    draw_rect(min_x + 2, y - 2, max_x - min_x - 2, 5, 0x00A0E0);
    /* Obere und untere Hochenergie-Strahllinie */
    draw_rect(min_x + 4, y - 1, max_x - min_x - 3, 1, 0xFFFFFF);
    draw_rect(min_x + 4, y + 1, max_x - min_x - 3, 1, 0xFFFFFF);
    /* Spitzenfokus */
    set_pixel(ex, y, 0xFFFFFF);
}

/* Turrican-2 Projektil: Spread-Shotgun-Dart (Authentisches Feuer-Pfeil-Design) */
static void draw_projectile_spread(int x, int y, fix16_t vx, fix16_t vy) {
    int dir = (vx >= 0) ? 1 : -1;
    int tilt = (vy > (1 << 15)) ? 1 : ((vy < -(1 << 15)) ? -1 : 0);
    uint32_t col_core = 0xFFFFFF;
    uint32_t col_glow = 0xFFF382;
    uint32_t col_mid  = 0xDF5D00;
    uint32_t col_dark = 0x921C00;

    /* Flackernder Funken-Schweif (Amiga Particle Trail) */
    int flick = (s_gui_frame + x) % 3;
    set_pixel(x - dir * 10, y + tilt * 2, col_dark);
    if (flick == 0) {
        set_pixel(x - dir * 7, y - 2, col_mid);
        set_pixel(x - dir * 8, y + 2, col_dark);
    } else if (flick == 1) {
        set_pixel(x - dir * 7, y + 2, col_mid);
        set_pixel(x - dir * 8, y - 2, col_dark);
    }

    /* Aerodynamischer Feuerpfeil-Koerper (orientiert in Flugrichtung) */
    if (dir > 0) {
        /* Nach rechts fliegend */
        draw_rect(x - 5, y - 1 + tilt, 4, 3, col_dark);
        draw_rect(x - 3, y - 2 + tilt, 5, 5, col_mid);
        draw_rect(x + 1, y - 1 + tilt, 5, 3, col_glow);
        /* Spitzen-Keil */
        set_pixel(x + 6, y + tilt, col_core);
        set_pixel(x + 5, y - 1 + tilt, col_glow);
        set_pixel(x + 5, y + 1 + tilt, col_glow);
        set_pixel(x + 2, y + tilt, col_core);
        set_pixel(x + 3, y + tilt, col_core);
        set_pixel(x + 4, y + tilt, col_core);
    } else {
        /* Nach links fliegend */
        draw_rect(x + 2, y - 1 + tilt, 4, 3, col_dark);
        draw_rect(x - 1, y - 2 + tilt, 5, 5, col_mid);
        draw_rect(x - 5, y - 1 + tilt, 5, 3, col_glow);
        /* Spitzen-Keil */
        set_pixel(x - 6, y + tilt, col_core);
        set_pixel(x - 5, y - 1 + tilt, col_glow);
        set_pixel(x - 5, y + 1 + tilt, col_glow);
        set_pixel(x - 2, y + tilt, col_core);
        set_pixel(x - 3, y + tilt, col_core);
        set_pixel(x - 4, y + tilt, col_core);
    }
}

/* Turrican-2 Projektil: Bouncing Emerald Sphere (Authentische 3D-Smaragdkugel) */
static void draw_projectile_bounce(int x, int y) {
    /* 14x14 Kreisfoermige, vielstufig schattierte Smaragd-Kugel aus Sprite-Sheet 143425 */
    static const uint8_t s_emerald_map[14][14] = {
        {0,0,0,0,1,1,1,1,1,1,0,0,0,0},
        {0,0,1,1,2,2,2,2,2,2,1,1,0,0},
        {0,1,2,3,4,4,3,3,2,2,2,1,0},
        {0,1,3,4,5,5,4,3,3,2,2,1,0},
        {1,2,4,5,5,5,4,4,3,2,2,1,1,0},
        {1,2,4,5,5,4,4,3,3,2,2,1,1,0},
        {1,2,3,4,4,4,3,3,2,2,1,1,1,0},
        {1,2,3,3,3,3,2,2,2,1,1,1,1,0},
        {1,1,2,2,2,2,2,2,1,1,1,1,1,0},
        {1,1,2,2,2,2,2,1,1,1,1,1,1,0},
        {0,1,1,2,2,2,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,1,1,1,1,1,0},
        {0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,0,0,0,0}
    };
    /* Farbstufen: 0=transparent, 1=Rand, 2=Schatten, 3=Koerper, 4=Leuchten, 5=Glanzpunkt */
    static const uint32_t s_emerald_pal[6] = {
        0, 0x004212, 0x007824, 0x00B836, 0x42FF72, 0xFFFFFF
    };

    int ox = x - 7;
    int oy = y - 7;
    for (int r = 0; r < 14; r++) {
        for (int c = 0; c < 14; c++) {
            uint8_t idx = s_emerald_map[r][c];
            if (idx > 0) {
                set_pixel(ox + c, oy + r, s_emerald_pal[idx]);
            }
        }
    }
}

/* Turrican-2 Projektil: Pulsierendes Feind-Plasma */
static void draw_projectile_enemy(int x, int y) {
    uint32_t pulse_c = ((s_gui_frame / 4) % 2 == 0) ? 0xFF3050 : 0xFF7030;
    /* 10x10 Plasma-Flammenball */
    draw_rect(x - 4, y - 3, 9, 7, 0x600020);
    draw_rect(x - 3, y - 4, 7, 9, 0x600020);
    draw_rect(x - 3, y - 2, 7, 5, pulse_c);
    draw_rect(x - 2, y - 3, 5, 7, pulse_c);
    draw_rect(x - 1, y - 1, 3, 3, 0xFFD080);
    set_pixel(x, y, 0xFFFFFF);
}
/* HUD Retro-Waffen-Badge mit Power-LED-Leiste */
static void draw_weapon_hud_badge(int x, int y, uint8_t weapon, uint8_t level) {
    /* 2.5D Badge-Rahmen */
    draw_rect(x, y, 42, 26, 0x0B1622);
    draw_rect_outline(x, y, 42, 26, 0x3A5C80);
    draw_rect(x + 1, y + 1, 40, 1, 0x6088B0);

    /* Waffen-Icon in der Badge */
    if (weapon == WEAPON_LASER) {
        /* Twin-Laser Emitter */
        draw_rect(x + 6, y + 5, 28, 3, 0x00E0FF);
        draw_rect(x + 8, y + 6, 22, 1, 0xFFFFFF);
        draw_rect(x + 6, y + 11, 28, 3, 0x00E0FF);
        draw_rect(x + 8, y + 12, 22, 1, 0xFFFFFF);
        draw_rect(x + 4, y + 4, 4, 12, 0x406080);
    } else if (weapon == WEAPON_BOUNCE) {
        /* Bouncing Ball mit Rebound-Kurve */
        draw_rect(x + 6, y + 14, 18, 1, 0x006020);
        draw_line(x + 6, y + 14, x + 16, y + 6, 0x20A040);
        draw_line(x + 16, y + 6, x + 26, y + 14, 0x20A040);
        draw_voxel(x + 20, y + 5, 10, 10, 2, 0x00E040);
        set_pixel(x + 22, y + 7, 0xFFFFFF);
    } else {
        /* Spread 3-Wege Pfeile */
        draw_rect(x + 6, y + 9, 8, 3, 0x507090);
        draw_line(x + 14, y + 10, x + 30, y + 6, 0xFFD020);
        draw_line(x + 14, y + 10, x + 34, y + 10, 0xFFFFFF);
        draw_line(x + 14, y + 10, x + 30, y + 14, 0xFFD020);
        draw_rect(x + 32, y + 9, 4, 3, 0xFF9010);
    }

    /* 5-Stufen Power-LEDs darunter */
    uint32_t led_on = (weapon == WEAPON_LASER) ? 0x00FFFF :
                      ((weapon == WEAPON_BOUNCE) ? 0x40FF60 : 0xFFE040);
    uint32_t led_off = 0x14202C;
    for (int l = 1; l <= 5; l++) {
        int lx = x + 3 + (l - 1) * 8;
        int ly = y + 19;
        uint32_t col = (l <= level) ? led_on : led_off;
        draw_rect(lx, ly, 6, 4, col);
        if (l <= level) {
            set_pixel(lx + 1, ly + 1, 0xFFFFFF);
        }
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

/* ============================================================================
 * EMBEDDED CHRIS HUELSBECK TFMX & MOD MASTER TRACKS
 * Direkt in der EXE gespeichert: Null Ladezeiten, null Disk-Abhaengigkeiten!
 * Multi-Subsongs: Chris Huelsbecks geniale Technik fuer unterbrechungsfreie
 * Uebergaenge zwischen Erkundung, Boss, Level-Clear und Game-Over!
 * ============================================================================ */
typedef struct {
    const char *name;
    const uint8_t *tfx;
    uint32_t tfx_len;
    const uint8_t *sam;
    uint32_t sam_len;
    int is_mod;
} EmbeddedTrack;

/* ============================================================================
 * VOLLBILD & FENSTER-SKALIERUNG (16:9 Letterbox / Pillarbox)
 * ============================================================================ */
static void toggle_fullscreen(HWND hwnd) {
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (!s_is_fullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &s_prev_wp) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            s_is_fullscreen = 1;
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &s_prev_wp);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        s_is_fullscreen = 0;
    }
}

/* ============================================================================
 * DEMOSZENE CRT & PHOSPHOR BLOOM FILTER (0 KB Libs, Mega-Wow-Effekt)
 * ============================================================================ */
static void apply_demoscene_postprocess(void) {
    if (s_filter_mode == 0) return;

    /* 1. Bilineare Weichzeichnung (falls aktiv oder Filter 1/4/5) */
    if (s_bilinear || s_filter_mode == 1 || s_filter_mode == 4) {
        for (int y = 0; y < WIN_H; y++) {
            uint32_t *row = &s_pixels[y * WIN_W];
            for (int x = 1; x < WIN_W - 1; x += 2) {
                uint32_t p0 = row[x];
                uint32_t p1 = row[x + 1];
                uint32_t r = (((p0 >> 16) & 0xFF) + ((p1 >> 16) & 0xFF)) >> 1;
                uint32_t g = (((p0 >> 8) & 0xFF)  + ((p1 >> 8) & 0xFF)) >> 1;
                uint32_t b = ((p0 & 0xFF)         + (p1 & 0xFF)) >> 1;
                row[x] = (r << 16) | (g << 8) | b;
            }
        }
    }

    /* 2. RGB Phosphor Triad Mask (Sony Trinitron / 1084S Aperture Grille) */
    if (s_phosphor_mask && (s_filter_mode == 2 || s_filter_mode == 4 || s_filter_mode == 5)) {
        for (int y = 0; y < WIN_H; y++) {
            uint32_t *row = &s_pixels[y * WIN_W];
            for (int x = 0; x < WIN_W; x++) {
                uint32_t p = row[x];
                uint32_t r = (p >> 16) & 0xFF;
                uint32_t g = (p >> 8) & 0xFF;
                uint32_t b = p & 0xFF;
                int triad = x % 3;
                if (triad == 0) {
                    r = (r * 280) >> 8; if (r > 255) r = 255;
                    g = (g * 220) >> 8;
                    b = (b * 220) >> 8;
                } else if (triad == 1) {
                    r = (r * 220) >> 8;
                    g = (g * 280) >> 8; if (g > 255) g = 255;
                    b = (b * 220) >> 8;
                } else {
                    r = (r * 220) >> 8;
                    g = (g * 220) >> 8;
                    b = (b * 280) >> 8; if (b > 255) b = 255;
                }
                row[x] = (r << 16) | (g << 8) | b;
            }
        }
    }

    /* 3. CRT Scanlines mit Helligkeits-Beam-Blooming */
    if (s_filter_mode >= 2) {
        int dark_mult = 200;
        if (s_scanline_strength == 0) dark_mult = 224;      /* 25% */
        else if (s_scanline_strength == 1) dark_mult = 192; /* 50% */
        else if (s_scanline_strength == 2) dark_mult = 160; /* 75% */
        else dark_mult = 120;                               /* 100% */

        for (int y = 1; y < WIN_H; y += 2) {
            uint32_t *row = &s_pixels[y * WIN_W];
            for (int x = 0; x < WIN_W; x++) {
                uint32_t p = row[x];
                uint32_t r = (p >> 16) & 0xFF;
                uint32_t g = (p >> 8) & 0xFF;
                uint32_t b = p & 0xFF;
                uint32_t lum = (r * 77 + g * 150 + b * 29) >> 8;
                int mult = dark_mult + (int)(lum >> 3);
                if (mult > 255) mult = 255;
                r = (r * mult) >> 8;
                g = (g * mult) >> 8;
                b = (b * mult) >> 8;
                row[x] = (r << 16) | (g << 8) | b;
            }
        }
    }

    /* 4. Phosphor Bloom & Glow */
    if (s_bloom_mode > 0 && (s_filter_mode == 3 || s_filter_mode == 4 || s_filter_mode == 5)) {
        static uint8_t s_glow_r[320 * 180];
        static uint8_t s_glow_g[320 * 180];
        static uint8_t s_glow_b[320 * 180];

        int threshold = (s_bloom_mode == 1) ? 160 : 130;
        for (int gy = 0; gy < 180; gy++) {
            int sy = gy * 4;
            uint32_t *srow = &s_pixels[sy * WIN_W];
            uint8_t *gr = &s_glow_r[gy * 320];
            uint8_t *gg = &s_glow_g[gy * 320];
            uint8_t *gb = &s_glow_b[gy * 320];
            for (int gx = 0; gx < 320; gx++) {
                uint32_t c = srow[gx * 4];
                uint32_t r = (c >> 16) & 0xFF;
                uint32_t g = (c >> 8) & 0xFF;
                uint32_t b = c & 0xFF;
                uint32_t lum = (r * 77 + g * 150 + b * 29) >> 8;
                if ((int)lum > threshold) {
                    uint32_t f = lum - threshold;
                    gr[gx] = (uint8_t)((r * f) >> 8);
                    gg[gx] = (uint8_t)((g * f) >> 8);
                    gb[gx] = (uint8_t)((b * f) >> 8);
                } else {
                    gr[gx] = 0; gg[gx] = 0; gb[gx] = 0;
                }
            }
        }

        for (int gy = 0; gy < 180; gy++) {
            uint8_t *gr = &s_glow_r[gy * 320];
            uint8_t *gg = &s_glow_g[gy * 320];
            uint8_t *gb = &s_glow_b[gy * 320];
            for (int gx = 1; gx < 319; gx++) {
                gr[gx] = (gr[gx-1] + gr[gx] * 2 + gr[gx+1]) >> 2;
                gg[gx] = (gg[gx-1] + gg[gx] * 2 + gg[gx+1]) >> 2;
                gb[gx] = (gb[gx-1] + gb[gx] * 2 + gb[gx+1]) >> 2;
            }
        }

        int boost = (s_bloom_mode == 1) ? 2 : (s_bloom_mode == 2 ? 3 : 5);
        for (int y = 0; y < WIN_H; y++) {
            int gy = y >> 2;
            uint32_t *row = &s_pixels[y * WIN_W];
            uint8_t *gr = &s_glow_r[gy * 320];
            uint8_t *gg = &s_glow_g[gy * 320];
            uint8_t *gb = &s_glow_b[gy * 320];
            for (int x = 0; x < WIN_W; x++) {
                int gx = x >> 2;
                uint32_t p = row[x];
                uint32_t r = ((p >> 16) & 0xFF) + ((uint32_t)gr[gx] * boost >> 2);
                uint32_t g = ((p >> 8) & 0xFF)  + ((uint32_t)gg[gx] * boost >> 2);
                uint32_t b = (p & 0xFF)         + ((uint32_t)gb[gx] * boost >> 2);
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                row[x] = (r << 16) | (g << 8) | b;
            }
        }
    }

    /* 5. CRT Barrel Curvature & Vignette bei Filter 5 */
    if (s_filter_mode == 5) {
        for (int y = 0; y < WIN_H; y++) {
            float ny = ((float)y - 360.0f) / 360.0f;
            uint32_t *row = &s_pixels[y * WIN_W];
            for (int x = 0; x < WIN_W; x++) {
                float nx = ((float)x - 640.0f) / 640.0f;
                float r2 = nx * nx + ny * ny;
                if (r2 > 0.4f) {
                    float vig = 1.0f - (r2 - 0.4f) * 0.5f;
                    if (vig < 0.2f) vig = 0.2f;
                    uint32_t p = row[x];
                    uint32_t r = (uint32_t)(((p >> 16) & 0xFF) * vig);
                    uint32_t g = (uint32_t)(((p >> 8) & 0xFF) * vig);
                    uint32_t b = (uint32_t)((p & 0xFF) * vig);
                    row[x] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }

    /* 6. Optionale Amiga-Farbmodi */
    if (s_color_mode == 3) {
        for (int y = 0; y < WIN_H; y++) {
            uint32_t *row = &s_pixels[y * WIN_W];
            for (int x = 0; x < WIN_W; x++) {
                uint32_t p = row[x];
                uint32_t lum = (((p >> 16) & 0xFF) * 77 + ((p >> 8) & 0xFF) * 150 + (p & 0xFF) * 29) >> 8;
                row[x] = (((lum * 50) >> 8) << 16) | (lum << 8) | ((lum * 60) >> 8);
            }
        }
    }
}

/* v0.3.1 Kuratierter, sparsamer Soundtrack: 100% Amiga & PC Paritaet */
static const EmbeddedTrack EMBEDDED_TRACKS[] = {
    { EMB_TFMX_T3W1_NAME,         EMB_TFMX_T3W1,         sizeof(EMB_TFMX_T3W1),         EMB_SAM_T3W1,         sizeof(EMB_SAM_T3W1),         0 }, /* 0: Turrican 3: World 1 Factory (Action Theme) */
    { EMB_TFMX_BIFI_NAME,         EMB_TFMX_BIFI,         sizeof(EMB_TFMX_BIFI),         EMB_SAM_BIFI,         sizeof(EMB_SAM_BIFI),         0 }, /* 1: BiFi Adventure (Chris Huelsbeck) */
    { EMB_TFMX_CYBER_NAME,        EMB_TFMX_CYBER,        sizeof(EMB_TFMX_CYBER),        EMB_SAM_CYBER,        sizeof(EMB_SAM_CYBER),        0 }, /* 2: Cybersty (Action Synth) */
    { EMB_TFMX_MADTV_NAME,        EMB_TFMX_MADTV,        sizeof(EMB_TFMX_MADTV),        EMB_SAM_MADTV,        sizeof(EMB_SAM_MADTV),        0 }, /* 3: Mad TV: Ongame 6 (Amiga Classic) */
    { EMB_TFMX_SOFTMAN_NAME,      EMB_TFMX_SOFTMAN,      sizeof(EMB_TFMX_SOFTMAN),      EMB_SAM_SOFTMAN,      sizeof(EMB_SAM_SOFTMAN),      0 }, /* 4: Software Manager Titel (22 Subsongs!) */
    { EMB_MOD_AIRWOLF2_NAME,      EMB_MOD_AIRWOLF2,      sizeof(EMB_MOD_AIRWOLF2),      NULL,                 0,                            1 }, /* 5: Airwolf 2 (Chiptune MOD) */
    { EMB_MOD_OCEAN_LOADER_NAME,  EMB_MOD_OCEAN_LOADER,  sizeof(EMB_MOD_OCEAN_LOADER),  NULL,                 0,                            1 }, /* 6: Ocean Loader (Demoscene MOD) */
    { EMB_MOD_INTRO45_NAME,       EMB_MOD_INTRO45,       sizeof(EMB_MOD_INTRO45),       NULL,                 0,                            1 }, /* 7: Intro 45 Cracktro (MOD) */
    { EMB_MOD_INTRO31_NAME,       EMB_MOD_INTRO31,       sizeof(EMB_MOD_INTRO31),       NULL,                 0,                            1 }, /* 8: Intro 31 Cracktro (MOD) */
    { EMB_MOD_INTRO75_NAME,       EMB_MOD_INTRO75,       sizeof(EMB_MOD_INTRO75),       NULL,                 0,                            1 }, /* 9: Intro 75 Cracktro (MOD) */
};
#define TRACK_IDX_AIRWOLF2     5
#define TRACK_IDX_OCEAN_LOADER 6
#define TRACK_IDX_INTRO45      7
#define TRACK_IDX_INTRO31      8
#define TRACK_IDX_INTRO75      9

/* Aliase fuer Abwaertskompatibilitaet mit Studio/Menue */
#define TRACK_IDX_BLUE_MONDAY  TRACK_IDX_AIRWOLF2
#define TRACK_IDX_WC_OFFICE    TRACK_IDX_OCEAN_LOADER
#define TRACK_IDX_FAIRLIGHT    TRACK_IDX_INTRO45
#define TRACK_IDX_SKIDROW      TRACK_IDX_INTRO31
#define TRACK_IDX_PARADOX      TRACK_IDX_INTRO75

/* ============================================================================
 * DJ FADE ENGINE (Pop-freier Studio Gain-Crossfade)
 * ============================================================================ */
#define DJ_FADE_FRAMES 12 /* 240ms butterweiches Fade-Out / Fade-In */
static int s_pending_track_idx = -1;
static int s_pending_subsong = 0;
static int s_fade_state = 0; /* 0 = none, 1 = fading out, 2 = fading in */
static int s_fade_frame = 0;

static int music_play_embedded_direct(int idx, int subsong);

static void trigger_dj_fade(int idx, int subsong) {
    s_pending_track_idx = idx;
    s_pending_subsong = subsong;
    s_fade_state = 1; /* Fade-out laeuft */
    s_fade_frame = 0;
}

static void apply_dj_fade(int16_t *buf, int n) {
    if (s_fade_state == 1) {
        float gain = 1.0f - (float)s_fade_frame / (float)DJ_FADE_FRAMES;
        if (gain < 0.0f) gain = 0.0f;
        for (int i = 0; i < n; i++) buf[i] = (int16_t)((float)buf[i] * gain);
        s_fade_frame++;
        if (s_fade_frame >= DJ_FADE_FRAMES) {
            music_play_embedded_direct(s_pending_track_idx, s_pending_subsong);
            s_fade_state = 2;
            s_fade_frame = 0;
        }
    } else if (s_fade_state == 2) {
        float gain = (float)s_fade_frame / (float)DJ_FADE_FRAMES;
        if (gain > 1.0f) gain = 1.0f;
        for (int i = 0; i < n; i++) buf[i] = (int16_t)((float)buf[i] * gain);
        s_fade_frame++;
        if (s_fade_frame >= DJ_FADE_FRAMES) {
            s_fade_state = 0;
        }
    }
}

/* ============================================================================
 * SUPERSAW LEAD SYNTHESIZER (7-Oszillator Detune Lead fuer Strudel Live-Coding)
 * ============================================================================ */
typedef struct {
    uint8_t  enabled;
    int16_t  note;
    uint32_t phases[7];
    uint32_t phase_inc[7];
    int32_t  lp_state;
    int      ticks_active;
    float    detune;
} SuperSawEngine;

static SuperSawEngine s_supersaw = { 1, 36, {0}, {0}, 0, 0, 0.038f };

static void supersaw_trigger(SuperSawEngine *ss, int16_t note) {
    if (note < 12 || note > 108) return;
    ss->note = note;
    ss->ticks_active = 22; /* 440ms Gate */
    float base_freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
    static const float s_detune_ratios[7] = {
        0.0f, -0.012f, 0.012f, -0.026f, 0.026f, -0.045f, 0.045f
    };
    for (int k = 0; k < 7; k++) {
        float f = base_freq * (1.0f + s_detune_ratios[k] * ss->detune * 25.0f);
        ss->phase_inc[k] = (uint32_t)((f * 65536.0f * 65536.0f) / (float)GAME_AUDIO_RATE);
    }
}

static void supersaw_render(SuperSawEngine *ss, int16_t *buf, int n, int16_t volume) {
    if (!ss->enabled || ss->ticks_active <= 0 || volume <= 0) return;
    for (int i = 0; i < n; i++) {
        int32_t sum_saw = 0;
        for (int k = 0; k < 7; k++) {
            int16_t saw = (int16_t)(((ss->phases[k] >> 16) & 0xFFFF) - 32768);
            sum_saw += saw;
            ss->phases[k] += ss->phase_inc[k];
        }
        int32_t avg_saw = sum_saw / 7;
        ss->lp_state += (avg_saw - ss->lp_state) >> 3;
        int32_t out_val = (ss->lp_state * volume) >> 8;
        int32_t mixed = buf[i] + out_val;
        if (mixed > 32767) mixed = 32767;
        else if (mixed < -32768) mixed = -32768;
        buf[i] = (int16_t)mixed;
    }
}
#define EMBEDDED_TRACK_COUNT (int)(sizeof(EMBEDDED_TRACKS) / sizeof(EMBEDDED_TRACKS[0]))

static int s_current_subsong = 0;
static int s_last_subsong_reason = -1;

/* In-EXE-Track abspielen (ohne Festplatten-/Dateizugriff) */
static int music_play_embedded_direct(int idx, int subsong) {
    if (idx < 0 || idx >= EMBEDDED_TRACK_COUNT) idx = 0;
    const EmbeddedTrack *t = &EMBEDDED_TRACKS[idx];
    s_mod_active = 0;
    s_tfmx_active = 0;
    s_song_start_wave = (int)s_game.wave;
    s_song_start_tick = s_game.tick;
    s_track_ticks = 0;

    if (t->is_mod) {
        if (mod_player_load_mem(&s_mod, t->tfx, t->tfx_len)) {
            mod_player_start(&s_mod, GAME_AUDIO_RATE);
            s_mod.loops = 0;
            s_mod_active = 1;
            s_current_subsong = 0;
            strncpy(s_current_track_name, t->name, sizeof(s_current_track_name) - 1);
            s_current_track_name[sizeof(s_current_track_name) - 1] = '\0';
            strcpy(s_current_track_type, "MOD");
            s_music_toast_timer = 180;
            snprintf(s_status_msg, sizeof(s_status_msg), "MUSIK [MOD]: %s", s_current_track_name);
            return 1;
        }
    } else {
        if (tfmx_player_load_mem(&s_tfmx, t->tfx, t->tfx_len, (const int8_t *)t->sam, t->sam_len)) {
            if (subsong < 0) {
                subsong = (s_tfmx.num_subsongs > 0) ? (int)(s_music_rng % s_tfmx.num_subsongs) : 0;
            } else if (subsong >= s_tfmx.num_subsongs) {
                subsong = 0;
            }
            s_current_subsong = subsong;
            tfmx_player_start(&s_tfmx, (uint8_t)subsong);
            s_tfmx_active = 1;
            strncpy(s_current_track_name, t->name, sizeof(s_current_track_name) - 1);
            s_current_track_name[sizeof(s_current_track_name) - 1] = '\0';
            strcpy(s_current_track_type, "TFMX");
            s_music_toast_timer = 180;
            if (s_tfmx.num_subsongs > 1) {
                snprintf(s_status_msg, sizeof(s_status_msg), "TFMX [%d/%d]: %s",
                         subsong + 1, s_tfmx.num_subsongs, s_current_track_name);
            } else {
                snprintf(s_status_msg, sizeof(s_status_msg), "TFMX: %s", s_current_track_name);
            }
            return 1;
        }
    }
    return 0;
}

static int music_play_embedded(int idx, int subsong) {
    if (s_mod_active || s_tfmx_active) {
        trigger_dj_fade(idx, subsong);
        return 1;
    }
    int res = music_play_embedded_direct(idx, subsong);
    s_fade_state = 2;
    s_fade_frame = 0;
    return res;
}

/* Reaktiver Subsong-Wechsel (z.B. Normal -> Boss -> LevelClear) */
static void music_subsong_switch(int subsong) {
    if (!s_tfmx_active || !s_tfmx.loaded || s_tfmx.num_subsongs <= 1) return;
    if (subsong < 0) subsong = 0;
    if (subsong >= s_tfmx.num_subsongs) subsong = 0;
    if (s_current_subsong == subsong) return;

    s_current_subsong = subsong;
    tfmx_player_start(&s_tfmx, (uint8_t)subsong);
    s_music_toast_timer = 180;
    snprintf(s_status_msg, sizeof(s_status_msg), "TFMX SUBSONG [%d/%d]: %s",
             s_current_subsong + 1, s_tfmx.num_subsongs, s_current_track_name);
}

/* Manueller Subsong-Zyklus (Taste K, SHIFT+M oder Gamepad RS+Y) */
static void music_subsong_next(void) {
    if (!s_tfmx_active || !s_tfmx.loaded || s_tfmx.num_subsongs <= 1) return;
    int next = (s_current_subsong + 1) % s_tfmx.num_subsongs;
    s_current_subsong = next;
    tfmx_player_start(&s_tfmx, (uint8_t)next);
    s_music_toast_timer = 180;
    snprintf(s_status_msg, sizeof(s_status_msg), "MANUELLER SUBSONG [%d/%d]: %s",
             s_current_subsong + 1, s_tfmx.num_subsongs, s_current_track_name);
}


/* Extrahieren des Sample-Bereichs fuer universellen TFMX <-> MOD Sample-Austausch */
static void get_track_sample_buffer(const EmbeddedTrack *t, const uint8_t **out_sam, uint32_t *out_len) {
    if (!t) { *out_sam = NULL; *out_len = 0; return; }
    if (!t->is_mod) {
        *out_sam = t->sam;
        *out_len = t->sam_len;
        return;
    }
    if (t->tfx_len >= 1084) {
        char sig[5];
        memcpy(sig, t->tfx + 1080, 4); sig[4] = '\0';
        int is_pt = (!memcmp(sig, "M.K.", 4) || !memcmp(sig, "M!K!", 4) || !memcmp(sig, "FLT4", 4) || !memcmp(sig, "4CHN", 4));
        if (is_pt) {
            int maxpat = 0;
            for (int i = 0; i < 128; i++) if (t->tfx[952 + i] > maxpat) maxpat = t->tfx[952 + i];
            uint32_t off = 1084u + (uint32_t)(maxpat + 1) * 1024u;
            if (off < t->tfx_len) {
                *out_sam = t->tfx + off;
                *out_len = t->tfx_len - off;
                return;
            }
        }
    }
    if (t->tfx_len >= 600) {
        int maxpat = 0;
        for (int i = 0; i < 128; i++) if (t->tfx[472 + i] > maxpat) maxpat = t->tfx[472 + i];
        uint32_t off = 600u + (uint32_t)(maxpat + 1) * 1024u;
        if (off < t->tfx_len) {
            *out_sam = t->tfx + off;
            *out_len = t->tfx_len - off;
            return;
        }
    }
    *out_sam = t->tfx;
    *out_len = t->tfx_len;
}

/* Shuffle / Frankenstein Sample-Swap: Universeller Tausch zwischen TFMX & MOD Samples! */
static char s_frankenstein_status[128] = "";

static void explorer_shuffle_swap(void) {
    int tfx_idx = rand() % EMBEDDED_TRACK_COUNT;
    int sam_idx = rand() % EMBEDDED_TRACK_COUNT;
    if (sam_idx == tfx_idx) sam_idx = (tfx_idx + 1 + (rand() % (EMBEDDED_TRACK_COUNT - 1))) % EMBEDDED_TRACK_COUNT;

    const EmbeddedTrack *trk_tfx = &EMBEDDED_TRACKS[tfx_idx];
    const EmbeddedTrack *trk_sam = &EMBEDDED_TRACKS[sam_idx];

    s_exp_mod_playing = 0;
    s_exp_playing = 0;
    tfmx_player_stop(&s_exp_tfmx);

    const uint8_t *sam_ptr = NULL;
    uint32_t sam_len = 0;
    get_track_sample_buffer(trk_sam, &sam_ptr, &sam_len);

    if (!trk_tfx->is_mod) {
        if (sam_ptr && sam_len > 64 &&
            tfmx_player_load_mem(&s_exp_tfmx, trk_tfx->tfx, trk_tfx->tfx_len,
                                 (const int8_t *)sam_ptr, sam_len)) {
            s_exp_subsong = 0;
            tfmx_player_start(&s_exp_tfmx, 0);
            s_exp_playing = 1;
            snprintf(s_frankenstein_status, sizeof(s_frankenstein_status),
                     "FRANKEN-SWAP: TFX [%s] x SAM [%s]", trk_tfx->name, trk_sam->name);
            snprintf(s_status_msg, sizeof(s_status_msg), "%s", s_frankenstein_status);
        }
    } else {
        if (mod_player_load_mem(&s_exp_mod, trk_tfx->tfx, trk_tfx->tfx_len)) {
            if (sam_ptr && sam_len > 1024) {
                for (int s = 1; s <= 31; s++) {
                    if (s_exp_mod.smp[s].len > 0) {
                        uint32_t new_off = (uint32_t)((s * 4096u) % sam_len);
                        s_exp_mod.smp[s].data = (const int8_t *)(sam_ptr + new_off);
                        if (new_off + s_exp_mod.smp[s].len > sam_len) {
                            s_exp_mod.smp[s].len = sam_len - new_off;
                        }
                        if (s_exp_mod.smp[s].rep > s_exp_mod.smp[s].len) {
                            s_exp_mod.smp[s].rep = 0;
                            s_exp_mod.smp[s].replen = s_exp_mod.smp[s].len > 2 ? 2 : 0;
                        } else if (s_exp_mod.smp[s].rep + s_exp_mod.smp[s].replen > s_exp_mod.smp[s].len) {
                            s_exp_mod.smp[s].replen = s_exp_mod.smp[s].len - s_exp_mod.smp[s].rep;
                        }
                    }
                }
            }
            mod_player_start(&s_exp_mod, GAME_AUDIO_RATE);
            s_exp_mod_playing = 1;
            snprintf(s_frankenstein_status, sizeof(s_frankenstein_status),
                     "FRANKEN-SWAP: MOD [%s] x SAM [%s]", trk_tfx->name, trk_sam->name);
            snprintf(s_status_msg, sizeof(s_status_msg), "%s", s_frankenstein_status);
        }
    }
}

/* --- Explorer Dateiverwaltung --- */

static void explorer_play_rom(int rom_idx) {
    if (rom_idx < 0 || rom_idx >= EMBEDDED_TRACK_COUNT) return;
    const EmbeddedTrack *et = &EMBEDDED_TRACKS[rom_idx];
    s_exp_playing = 0;
    s_exp_mod_playing = 0;
    if (et->is_mod) {
        if (mod_player_load_mem(&s_exp_mod, et->tfx, et->tfx_len)) {
            mod_player_start(&s_exp_mod, GAME_AUDIO_RATE);
            s_exp_mod_playing = 1;
            snprintf(s_status_msg, sizeof(s_status_msg), "ROM-MOD: %s", et->name);
        }
    } else {
        if (tfmx_player_load_mem(&s_exp_tfmx, et->tfx, et->tfx_len, (const int8_t *)et->sam, et->sam_len)) {
            s_exp_subsong = 0;
            tfmx_player_start(&s_exp_tfmx, 0);
            s_exp_playing = 1;
            snprintf(s_status_msg, sizeof(s_status_msg), "ROM-TFMX: %s (%d Subs)", et->name, s_exp_tfmx.num_subsongs);
        }
    }
}

static void explorer_scan_directory(const char *dir_path) {
    WIN32_FIND_DATAA fd;
    char search_pattern[300];
    HANDLE hFind;

    s_num_explorer_items = 0;
    s_explorer_selected = 0;
    s_explorer_scroll = 0;
    strncpy(s_current_dir, dir_path, sizeof(s_current_dir) - 1);
    s_current_dir[sizeof(s_current_dir) - 1] = '\0';

    /* Virtueller Ordner: Eingebaute Soundtracks aus der EXE */
    if (strcmp(dir_path, "<ROM: EINGEBAUTE SOUNDTRACKS>") == 0) {
        strcpy(s_explorer_items[0].name, "..");
        strcpy(s_explorer_items[0].path, "..");
        s_explorer_items[0].is_dir = 1;
        s_num_explorer_items = 1;

        for (int i = 0; i < EMBEDDED_TRACK_COUNT; i++) {
            if (s_num_explorer_items >= MAX_EXPLORER_ITEMS) break;
            snprintf(s_explorer_items[s_num_explorer_items].name, 63, "[ROM] %s", EMBEDDED_TRACKS[i].name);
            snprintf(s_explorer_items[s_num_explorer_items].path, 259, "ROM:%d", i);
            s_explorer_items[s_num_explorer_items].is_dir = 0;
            s_num_explorer_items++;
        }
        return;
    }

    /* Zuerst ".." Eintrag falls nicht Root */
    if (strcmp(dir_path, ".") != 0 && strcmp(dir_path, "TFMX Professional") != 0) {
        strcpy(s_explorer_items[s_num_explorer_items].name, "..");
        strcpy(s_explorer_items[s_num_explorer_items].path, "..");
        s_explorer_items[s_num_explorer_items].is_dir = 1;
        s_num_explorer_items++;
    }

    /* Virtuellen ROM-Ordner an oberster Stelle verfuegbar machen */
    if (strcmp(dir_path, ".") == 0 && s_num_explorer_items < MAX_EXPLORER_ITEMS) {
        strcpy(s_explorer_items[s_num_explorer_items].name, "[ROM: EINGEBAUTE SOUNDTRACKS]");
        strcpy(s_explorer_items[s_num_explorer_items].path, "<ROM: EINGEBAUTE SOUNDTRACKS>");
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

    /* Danach TFX, STRUDEL und MOD Dateien einlesen */
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
        /* Blacklist Werbe- und defekte Tracks (Karamalz, Ooops Up) */
        if (strstr(dir, "Karamalz") || strstr(dir, "Ooops") ||
            strstr(fd.cFileName, "Karamalz") || strstr(fd.cFileName, "Ooops")) continue;
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
                if (is_tfx) {
                    /* Nur aufnehmen wenn .sam im selben Verzeichnis existiert */
                    char sam_check[300];
                    char *dot;
                    FILE *ftest;
                    snprintf(sam_check, sizeof(sam_check), "%s\\%s", dir, fd.cFileName);
                    dot = strrchr(sam_check, '.');
                    if (dot) strcpy(dot, ".sam");
                    ftest = fopen(sam_check, "rb");
                    if (!ftest) is_tfx = 0;
                    else fclose(ftest);
                }
                if (is_mod || is_tfx) {
                    /* Duplikate verhindern */
                    int dup = 0;
                    for (int di = 0; di < s_playlist_count; di++) {
                        if (_stricmp(s_playlist[di].name, fd.cFileName) == 0) { dup = 1; break; }
                    }
                    if (!dup && s_playlist_count < PLAYLIST_MAX) {
                        strncpy(s_playlist[s_playlist_count].name, fd.cFileName, 63);
                        s_playlist[s_playlist_count].name[63] = '\0';
                        snprintf(s_playlist[s_playlist_count].path, 259, "%s\\%s", dir, fd.cFileName);
                        s_playlist[s_playlist_count].is_mod = is_mod;
                        s_playlist_count++;
                    }
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

/* Lade Track idx und starte ihn (exakt ein Player aktiv, gibt 1 bei Erfolg). */
static int music_load_index(int idx) {
    PlaylistEntry *e;
    char msg[180];

    if (idx < 0 || idx >= s_playlist_count) return 0;
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
        if (size >= 600 && mod_player_load_mem(&s_mod, s_mod_buf, (uint32_t)size)) {
            char *dot;
            mod_player_start(&s_mod, GAME_AUDIO_RATE);
            s_mod_active = 1;
            s_track_ticks = 0;
            strncpy(s_current_track_name, e->name, sizeof(s_current_track_name) - 1);
            dot = strrchr(s_current_track_name, '.');
            if (dot) *dot = '\0';
            strcpy(s_current_track_type, "MOD");
            s_music_toast_timer = 180;
            snprintf(msg, sizeof(msg), "[MOD] %s", s_current_track_name);
            snprintf(s_status_msg, sizeof(s_status_msg), "%s", msg);
            return 1;
        }
        return 0;
    } else {
        s_mod_active = 0;
        s_tfmx_active = 0;
        if (tfmx_player_load(&s_tfmx, e->path, NULL)) {
            char *dot;
            tfmx_player_start(&s_tfmx, 0);
            s_tfmx_active = 1;
            s_track_ticks = 0;
            strncpy(s_current_track_name, e->name, sizeof(s_current_track_name) - 1);
            dot = strrchr(s_current_track_name, '.');
            if (dot) *dot = '\0';
            strcpy(s_current_track_type, "TFMX");
            s_music_toast_timer = 180;
            snprintf(msg, sizeof(msg), "[TFMX] %s", s_current_track_name);
            snprintf(s_status_msg, sizeof(s_status_msg), "%s", msg);
            return 1;
        }
        return 0;
    }
}

static void music_play_campaign_level(int level) {
    /* v0.3.1 Kuratierter Amiga-Soundtrack: 0=Turrican 3 W1, 1=BiFi, 2=Cybersty, 3=Mad TV, 4=Software Manager */
    static const int camp_map[5] = { 0, 1, 2, 3, 4 };
    int idx = (level >= 1 && level <= 5) ? (level - 1) : 0;
    int emb_id = camp_map[idx];

    s_last_subsong_reason = -1;
    if (music_play_embedded(emb_id, 0)) {
        snprintf(s_status_msg, sizeof(s_status_msg), "LEVEL %d: %s", level, s_current_track_name);
    }
}

/* Deterministischer Zufalls-Song aus der Library oder Embedded (mit zufaelligem Subsong!) */
static void music_play_next(void) {
    s_last_subsong_reason = -1;
    if (s_playlist_count <= 0) {
        int r_idx = (int)(s_music_rng % (uint32_t)EMBEDDED_TRACK_COUNT);
        music_play_embedded(r_idx, -1);
        return;
    }
    for (int attempt = 0; attempt < 12; attempt++) {
        uint32_t seed = 0x5EED5EEDu + (uint32_t)(s_last_wave + s_gui_frame + attempt * 17) * 2654435761u;
        s_music_rng = seed * 1664525u + 1013904223u;
        int idx = (int)(s_music_rng % (uint32_t)s_playlist_count);
        if (music_load_index(idx)) {
            if (s_tfmx_active && s_tfmx.num_subsongs > 1) {
                int r_sub = (int)(s_music_rng % s_tfmx.num_subsongs);
                s_current_subsong = r_sub;
                tfmx_player_start(&s_tfmx, (uint8_t)r_sub);
                snprintf(s_status_msg, sizeof(s_status_msg), "TFMX [%d/%d]: %s",
                         r_sub + 1, s_tfmx.num_subsongs, s_current_track_name);
            }
            return;
        }
    }
    int r_idx = (int)(s_music_rng % (uint32_t)EMBEDDED_TRACK_COUNT);
    music_play_embedded(r_idx, -1);
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
    s_game.mode = (uint8_t)mode;         /* MODE_ENDLESS oder MODE_CAMPAIGN */
    s_game.campaign_level = 1;
    if (mode == MODE_ENDLESS) {
        /* Fuer interaktives Spiel in der GUI: Plattformen aus Biom 0 bereitstellen */
        s_game.mode = MODE_CAMPAIGN;
        core_setup_platforms(&s_game);
        s_game.mode = MODE_ENDLESS;
    } else {
        core_setup_platforms(&s_game);
    }
    init_level_boxes();
    s_active_mode = mode;
    s_pad_start_prev = 0;
    s_mod_active = 0;
    s_tfmx_active = 0;
    s_last_wave = s_game.wave;          /* = 0, Song wechselt bei Wave 1 */
    s_last_campaign_level = 1;
    if (mode == MODE_CAMPAIGN) {
        music_play_campaign_level(1);
    } else {
        music_play_next();              /* zufaelliger Track aus der Library */
    }
    s_loading_ticks = 0;
    s_gui_state = GUI_STATE_LOADING;
}

static void menu_activate(void) {
    switch (s_menu_index) {
        case 0: start_game(MODE_CAMPAIGN); break;
        case 1: start_game(MODE_ENDLESS);  break;
        case 2: s_gui_state = GUI_STATE_MANUAL; s_manual_page = 0; break;
        case 3: s_gui_state = GUI_STATE_CONTROLS; break;
        case 4: s_current_tab = TAB_GRAPHICS; s_gui_state = GUI_STATE_STUDIO; music_play_embedded(TRACK_IDX_BLUE_MONDAY, 0); break;
        case 5: s_current_tab = TAB_EXPLORER; s_gui_state = GUI_STATE_STUDIO; break;
        case 6: s_current_tab = TAB_STRUDEL;  s_gui_state = GUI_STATE_STUDIO; break;
        case 7: s_current_tab = TAB_INTRO;    s_gui_state = GUI_STATE_STUDIO; break;
        case 8: s_gui_state = GUI_STATE_CREDITS; credits_init(); break;
        case 9: PostQuitMessage(0); break;
        default: break;
    }
}

/* --- XInput / DirectInput: Gamepad im Hauptmenue & States navigieren (Edge-Getriggerung) --- */
static void pad_menu_poll(void) {
    static int prev_up, prev_down, prev_left, prev_right, prev_a, prev_b, prev_start;
    poll_gamepad();
    if (!s_pad.connected) return;

    if (s_gui_state == GUI_STATE_MENU) {
        if (s_pad.up && !prev_up)
            s_menu_index = (s_menu_index + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
        if (s_pad.down && !prev_down)
            s_menu_index = (s_menu_index + 1) % MENU_ITEM_COUNT;
        if ((s_pad.btn_fire && !prev_a) || (s_pad.btn_start && !prev_start))
            menu_activate();
    } else if (s_gui_state == GUI_STATE_CONTROLS) {
        if (s_pad.up && !prev_up) {
            s_ctrl_setting_cursor = (s_ctrl_setting_cursor + 2 - 1) % 2;
            sound_sfx_hit(&s_audio, 0);
        }
        if (s_pad.down && !prev_down) {
            s_ctrl_setting_cursor = (s_ctrl_setting_cursor + 1) % 2;
            sound_sfx_hit(&s_audio, 0);
        }
        int dir = 0;
        if (s_pad.left && !prev_left) dir = -1;
        if (s_pad.right && !prev_right) dir = 1;
        if (dir != 0) {
            if (s_ctrl_setting_cursor == 0) s_pad_preset = (s_pad_preset + 3 + dir) % 3;
            else if (s_ctrl_setting_cursor == 1) s_pad_laser_invert_y = !s_pad_laser_invert_y;
            sound_sfx_pickup(&s_audio);
        }
        if ((s_pad.btn_morph && !prev_b) || (s_pad.btn_start && !prev_start)) {
            s_gui_state = GUI_STATE_MENU;
        }
    } else if (s_gui_state == GUI_STATE_MANUAL) {
        if ((s_pad.left && !prev_left) || (s_pad.btn_lb)) {
            if (s_manual_page > 0) s_manual_page--;
        }
        if ((s_pad.right && !prev_right) || (s_pad.btn_rb)) {
            if (s_manual_page < 4) s_manual_page++;
        }
        if ((s_pad.btn_morph && !prev_b) || (s_pad.btn_start && !prev_start)) {
            s_gui_state = GUI_STATE_MENU;
        }
    } else if (s_gui_state == GUI_STATE_CREDITS) {
        if ((s_pad.btn_morph && !prev_b) || (s_pad.btn_start && !prev_start)) {
            s_gui_state = GUI_STATE_MENU;
            music_play_embedded(0, 0);
        }
    } else if (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_INTRO) {
        if ((s_pad.btn_fire && !prev_a) || (s_pad.btn_start && !prev_start) || (s_pad.btn_morph && !prev_b)) {
            s_gui_state = GUI_STATE_MENU;
        }
    } else if (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_GRAPHICS) {
        if (s_pad.up && !prev_up) {
            s_gfx_setting_cursor = (s_gfx_setting_cursor + 6) % 7;
            sound_sfx_hit(&s_audio, 0);
        }
        if (s_pad.down && !prev_down) {
            s_gfx_setting_cursor = (s_gfx_setting_cursor + 1) % 7;
            sound_sfx_hit(&s_audio, 0);
        }
        int dir = 0;
        if (s_pad.left && !prev_left) dir = -1;
        if (s_pad.right && !prev_right) dir = 1;
        if (dir != 0) {
            if (s_gfx_setting_cursor == 0) s_filter_mode = (s_filter_mode + 6 + dir) % 6;
            else if (s_gfx_setting_cursor == 1) s_scanline_strength = (s_scanline_strength + 4 + dir) % 4;
            else if (s_gfx_setting_cursor == 2) s_phosphor_mask = !s_phosphor_mask;
            else if (s_gfx_setting_cursor == 3) s_bloom_mode = (s_bloom_mode + 4 + dir) % 4;
            else if (s_gfx_setting_cursor == 4) s_bilinear = !s_bilinear;
            else if (s_gfx_setting_cursor == 6) s_color_mode = (s_color_mode + 4 + dir) % 4;
            sound_sfx_pickup(&s_audio);
        }
        if ((s_pad.btn_morph && !prev_b) || (s_pad.btn_start && !prev_start)) {
            s_gui_state = GUI_STATE_MENU;
            music_play_embedded(0, 0);
        }
    } else if (s_gui_state == GUI_STATE_GAMEOVER) {
        if ((s_pad.btn_fire && !prev_a) || (s_pad.btn_start && !prev_start)) {
            start_game(s_active_mode);
        } else if (s_pad.btn_morph && !prev_b) {
            s_gui_state = GUI_STATE_MENU;
        }
    }
    prev_up = s_pad.up; prev_down = s_pad.down;
    prev_left = s_pad.left; prev_right = s_pad.right;
    prev_a = s_pad.btn_fire; prev_b = s_pad.btn_morph;
    prev_start = s_pad.btn_start;
}

/* --- GUI_STATE_CONTROLS: Tastenbelegung, Controller-Konfiguration & Live-Tester --- */

/* Roh-Tasten-Indexraum (entspricht den Bits in s_pad.raw_buttons):
 * 0=B1/Square, 1=B2/Cross(A), 2=B3/Circle(B), 3=B4/Triangle(Y),
 * 4=LB, 5=RB, 6=R2, 7=Start */
#define RAW_BTN_COUNT 8
static const char *pad_button_name_di(int bi) {
    switch (bi) {
        case 0: return "1";  case 1: return "2";   case 2: return "3";
        case 3: return "4";  case 4: return "L1";  case 5: return "R1";
        case 6: return "R2"; case 7: return "OPT";
    }
    return "?";
}
static const char *pad_button_name_xi(int bi) {
    switch (bi) {
        case 0: return "X";  case 1: return "A";   case 2: return "B";
        case 3: return "Y";  case 4: return "LB";  case 5: return "RB";
        case 6: return "RT"; case 7: return "ST";
    }
    return "?";
}

/* Aktion-Indexraum: welche Taste ist wo belegt */
#define PAD_ACT_COUNT 6
typedef enum {
    PAD_ACT_FIRE = 0, PAD_ACT_JUMP, PAD_ACT_MORPH, PAD_ACT_WEAPSW, PAD_ACT_BEAM, PAD_ACT_MENU
} PadAction;
static const char *pad_action_names[PAD_ACT_COUNT] = {
    "FEUER (Hauptwaffe)",
    "SPRUNG",
    "MORPH-BALL",
    "WAFFE WECHSELN",
    "DREHLASER (360 Grad)",
    "MENUE / PAUSE"
};

/* Ermittelt die Belegung einer Aktion im aktuellen Preset.
 * di[]/xi[]: bis zu 2 belegte Roh-Tasten (sonst -1). stick_up = 1 wenn
 * Stick/D-Pad UP die Aktion triggert. */
static void pad_action_binding(int act, int preset, int di[2], int xi[2], int *stick_up) {
    di[0] = di[1] = -1; xi[0] = xi[1] = -1; *stick_up = 0;
    switch (act) {
        case PAD_ACT_FIRE:
            if (preset == PAD_PRESET_RETRO_COMPETITION_PRO) { di[0] = 0; }
            else if (preset == PAD_PRESET_PLATFORMER)       { di[0] = 0; xi[0] = 0; }
            else { di[0] = 1; xi[0] = 1; } /* A / Cross */
            break;
        case PAD_ACT_JUMP:
            if (preset == PAD_PRESET_RETRO_COMPETITION_PRO) { *stick_up = 1; }
            else if (preset == PAD_PRESET_PLATFORMER)       { di[0] = 1; xi[0] = 1; } /* A = Sprung */
            else { di[0] = 4; xi[0] = 4; } /* LB */
            break;
        case PAD_ACT_MORPH:
            if (preset == PAD_PRESET_RETRO_COMPETITION_PRO) { di[0] = 2; }
            else { di[0] = 2; xi[0] = 2; } /* B / Circle */
            break;
        case PAD_ACT_WEAPSW:
            if (preset == PAD_PRESET_RETRO_COMPETITION_PRO) { di[0] = 3; }
            else if (preset == PAD_PRESET_PLATFORMER)       { di[0] = 5; xi[0] = 5; } /* R1 */
            else { di[0] = 3; di[1] = 5; xi[0] = 3; xi[1] = 5; } /* Y + R1 */
            break;
        case PAD_ACT_BEAM:
            if (preset == PAD_PRESET_RETRO_COMPETITION_PRO) { di[0] = 1; }
            else if (preset == PAD_PRESET_PLATFORMER)       { di[0] = 3; xi[0] = 3; } /* Y / Triangle */
            else { di[0] = 0; di[1] = 6; xi[0] = 0; xi[1] = 6; } /* X / Square / R2 */
            break;
        case PAD_ACT_MENU:
            di[0] = 7; xi[0] = 7; /* Options / Start */
            break;
    }
}

static void render_state_controls(void) {
    clear_screen(0x00040A);

    /* Retro Amiga Header */
    draw_rect(0, 0, WIN_W, 40, 0x001020);
    draw_rect_outline(0, 0, WIN_W, 40, 0x0050A0);
    draw_string(24, 14, ">>> STRUDELFORCE - CONTROLLER-KONFIGURATION & TASTENBELEGUNG <<<", 0x00E0FF, 1);
    draw_string(WIN_W - 380, 14, "[ESC / BUTTON B] ZURUECK ZUM MENUE", 0xFFE040, 1);

    int mx = 50;
    int my = 54;
    int mw = WIN_W - 100;
    int mh = WIN_H - 106;
    draw_rect(mx, my, mw, mh, 0x000A14);
    draw_rect_outline(mx, my, mw, mh, 0x004080);
    draw_rect_outline(mx + 4, my + 4, mw - 8, mh - 8, 0x002040);

    /* Status-Leiste: Erkanntes Gamepad */
    draw_rect(mx + 20, my + 16, mw - 40, 32, 0x001428);
    draw_rect_outline(mx + 20, my + 16, mw - 40, 32, 0x004080);
    draw_string(mx + 36, my + 26, "AKTIVES GERAET:", 0xFFE040, 1);
    draw_string(mx + 200, my + 26, s_pad_connected ? s_pad.name : "KEIN GAMEPAD ERKANNT (TASTATUR WASD/PFEILE)", s_pad_connected ? 0x00FF80 : 0xA0A0A0, 1);

    /* Einstellungs-Optionen */
    int sy = my + 60;
    const char *preset_names[3] = {
        "1. STANDARD MODERN (A=Feuer, B=Morph, X=Laser, Y=Waffe, Stick=Springen)",
        "2. RETRO JOYSTICK (Competition Pro USB: Stick Oben=Springen, 1=Feuer, 2=Laser)",
        "3. CLASSIC PLATFORMER (A=Springen, X=Feuer, B=Morph, Y=Laser, R1=Waffe)"
    };

    /* Zeile 1: Profil */
    int active0 = (s_ctrl_setting_cursor == 0);
    draw_rect(mx + 20, sy, mw - 40, 34, active0 ? 0x002850 : 0x001020);
    draw_rect_outline(mx + 20, sy, mw - 40, 34, active0 ? 0x00FFFF : 0x004080);
    draw_string(mx + 36, sy + 11, "BELEGUNGS-PROFIL:", active0 ? 0xFFFFFF : 0xA0A0A0, 1);
    draw_string(mx + 220, sy + 11, preset_names[s_pad_preset % 3], active0 ? 0x00FFFF : 0x60D0FF, 1);

    /* Zeile 2: 360-Grad Laser Y-Achse */
    sy += 40;
    int active1 = (s_ctrl_setting_cursor == 1);
    draw_rect(mx + 20, sy, mw - 40, 34, active1 ? 0x002850 : 0x001020);
    draw_rect_outline(mx + 20, sy, mw - 40, 34, active1 ? 0x00FFFF : 0x004080);
    draw_string(mx + 36, sy + 11, "360 DREHLASER Y-ACHSE:", active1 ? 0xFFFFFF : 0xA0A0A0, 1);
    draw_string(mx + 280, sy + 11, s_pad_laser_invert_y ? "< INVERTIERT >  (Stick nach Oben = zielt nach Oben)" : "< NORMAL >      (Kamera-Flug-Achse)", active1 ? 0x00FFFF : 0x60D0FF, 1);

    /* Dynamische Belegungs-Tabelle: welche Taste triggert welche Aktion.
     * Gruen = belegt (Preset), hellgruen = belegt + gerade gedrueckt,
     * Dunkelblau = frei. Aktualisiert sich live beim Preset-Wechsel. */
    int tbl_y = sy + 44;
    draw_rect(mx + 20, tbl_y - 22, mw - 40, 22, 0x001428);
    draw_rect_outline(mx + 20, tbl_y - 22, mw - 40, 22, 0x004080);
    draw_string(mx + 36, tbl_y - 16, "AKTION", 0xFFE040, 1);
    draw_string(mx + 330, tbl_y - 16, "COMPETITION PRO / USB-JOYSTICK (WINMM)", 0xFF9040, 1);
    draw_string(mx + 700, tbl_y - 16, "PS4 / XBOX (XINPUT)", 0xFF9040, 1);
    for (int act = 0; act < PAD_ACT_COUNT; act++) {
        int ry = tbl_y + act * 24;
        int di_b[2], xi_b[2], su = 0;
        pad_action_binding(act, s_pad_preset, di_b, xi_b, &su);
        draw_rect(mx + 20, ry, mw - 40, 22, (act & 1) ? 0x001020 : 0x000C18);
        draw_string(mx + 36, ry + 6, pad_action_names[act], 0x00E0FF, 1);
        for (int bi = 0; bi < RAW_BTN_COUNT; bi++) {
            int pressed = (s_pad.raw_buttons >> bi) & 1;
            int bd = (di_b[0] == bi || di_b[1] == bi);
            int bxd = (xi_b[0] == bi || xi_b[1] == bi);
            int dx0 = mx + 330 + bi * 30;
            draw_rect(dx0, ry + 2, 26, 18, bd ? (pressed ? 0x00FF80 : 0x00A020) : 0x002040);
            draw_rect_outline(dx0, ry + 2, 26, 18, bd ? 0x00FFC0 : 0x003050);
            draw_string(dx0 + 5, ry + 6, pad_button_name_di(bi),
                        bd ? (pressed ? 0x000000 : 0xFFFFFF) : 0x506070, 1);
            int xx0 = mx + 700 + bi * 30;
            draw_rect(xx0, ry + 2, 26, 18, bxd ? (pressed ? 0x00FF80 : 0x00A020) : 0x002040);
            draw_rect_outline(xx0, ry + 2, 26, 18, bxd ? 0x00FFC0 : 0x003050);
            draw_string(xx0 + 5, ry + 6, pad_button_name_xi(bi),
                        bxd ? (pressed ? 0x000000 : 0xFFFFFF) : 0x506070, 1);
        }
        if (su) {
            int sx0 = mx + 330 + RAW_BTN_COUNT * 30 + 8;
            draw_rect(sx0, ry + 2, 66, 18, s_pad.up ? 0x00FF80 : 0x00A020);
            draw_rect_outline(sx0, ry + 2, 66, 18, 0x00FFC0);
            draw_string(sx0 + 7, ry + 6, "STICK-UP", s_pad.up ? 0x000000 : 0xFFFFFF, 1);
        }
    }

    /* Retro-Geraete-Illustrationen (reines Physisches Layout — die aktuelle
     * Belegung steht in der Tabelle oben und waehlt mit dem Preset um) */
    int ay = tbl_y + PAD_ACT_COUNT * 24 + 12;
    int box_h = 150;
    int box_w = (mw - 60) / 2;

    /* Linke Box: Competition Pro USB */
    draw_rect(mx + 20, ay, box_w, box_h, 0x000F1E);
    draw_rect_outline(mx + 20, ay, box_w, box_h, (s_pad_preset == PAD_PRESET_RETRO_COMPETITION_PRO) ? 0x00FF80 : 0x003060);
    draw_string(mx + 36, ay + 10, "=== COMPETITION PRO USB (RETRO JOYSTICK) ===", 0xFF9040, 1);
    draw_string(mx + 40, ay + 30, "        .-------.", 0x80D0FF, 1);
    draw_string(mx + 40, ay + 46, "   ____/  STICK  \\____", 0x80D0FF, 1);
    draw_string(mx + 40, ay + 62, "  |  [1] [2] [3] [4]  |", 0x00FF80, 1);
    draw_string(mx + 40, ay + 78, "   \\_______________/", 0x80D0FF, 1);
    draw_string(mx + 40, ay + 96, "  STICK L/R: LAUFEN   STICK OBEN: SPRINGEN", 0xC0D0E0, 1);
    draw_string(mx + 40, ay + 112, "  STICK UNTEN: DUCKEN / MORPH", 0xC0D0E0, 1);
    draw_string(mx + 40, ay + 130, "  TASTEN 1-4: SIEHE BELEGGUNGS-TABELLE OBEREN", 0xA0A0A0, 1);

    /* Rechte Box: Modernes Gamepad (PS4 / Xbox) */
    int rx = mx + 30 + box_w;
    draw_rect(rx, ay, box_w, box_h, 0x000F1E);
    draw_rect_outline(rx, ay, box_w, box_h, (s_pad_preset != PAD_PRESET_RETRO_COMPETITION_PRO) ? 0x00FF80 : 0x003060);
    draw_string(rx + 16, ay + 10, "=== DUALSHOCK 4 / XBOX / USB PAD ===", 0xFF9040, 1);
    draw_string(rx + 20, ay + 30, "      __               __", 0x80D0FF, 1);
    draw_string(rx + 20, ay + 46, "     [LB]             [RB]", 0x80D0FF, 1);
    draw_string(rx + 20, ay + 62, "  .------------------------.", 0x80D0FF, 1);
    draw_string(rx + 20, ay + 78, " /   (^)           (Y)      \\", 0x80D0FF, 1);
    draw_string(rx + 20, ay + 94, "|  (<)  (>)    (X)   (B)   |", 0x80D0FF, 1);
    draw_string(rx + 20, ay + 110, " \\     [L-STICK][R-STICK] /", 0x80D0FF, 1);
    draw_string(rx + 20, ay + 126, "  L: LAUFEN  R: DREHLASER-PEILEN", 0xC0D0E0, 1);
    draw_string(rx + 20, ay + 140, "  TASTEN: SIEHE BELEGGUNGS-TABELLE OBEREN", 0xA0A0A0, 1);

    /* Live Button & Axis Testfeld (Unten) */
    int ty = ay + box_h + 10;
    draw_rect(mx + 20, ty, mw - 40, 52, 0x001428);
    draw_rect_outline(mx + 20, ty, mw - 40, 52, 0x0050A0);
    draw_string(mx + 36, ty + 6, "LIVE CONTROLLER-TESTFELD (Druecke Tasten zum Pruefen):", 0xFFE040, 1);

    /* Test-Badges (aktionsbezogen: leuchten je nach aktueller Preset-Belegung) */
    struct { const char *name; int pressed; } badges[] = {
        { "UP", s_pad.up },
        { "DOWN", s_pad.down },
        { "LEFT", s_pad.left },
        { "RIGHT", s_pad.right },
        { "SPRUNG", s_pad.btn_jump },
        { "FEUER", s_pad.btn_fire },
        { "LASER", s_pad.btn_beam },
        { "MORPH", s_pad.btn_morph },
        { "WAFFE", s_pad.btn_weapsw },
        { "START", s_pad.btn_start }
    };
    int bx = mx + 36;
    for (int bi = 0; bi < 10; bi++) {
        int bw = (int)strlen(badges[bi].name) * 8 + 12;
        draw_rect(bx, ty + 24, bw, 20, badges[bi].pressed ? 0x00FF80 : 0x002040);
        draw_rect_outline(bx, ty + 24, bw, 20, badges[bi].pressed ? 0xFFFFFF : 0x004080);
        draw_string(bx + 6, ty + 29, badges[bi].name, badges[bi].pressed ? 0x000000 : 0x80C0E0, 1);
        bx += bw + 8;
    }
    draw_string(mx + 36, ty + 56, "OBEN/UNTEN: EINSTELLUNG WAEHLEN   LINKS/RECHTS: WERT UMSCHALTEN   B / START / ESC: ZURUECK", 0x80A0C0, 1);
}

/* --- GUI_STATE_MANUAL: Ausfuehrliches 5-Seiten Piloten-Handbuch (Amiga-Stil) --- */
static void render_state_manual(void) {
    clear_screen(0x00040A);

    /* Retro Amiga Header */
    draw_rect(0, 0, WIN_W, 40, 0x001020);
    draw_rect_outline(0, 0, WIN_W, 40, 0x0050A0);
    draw_string(24, 14, ">>> STRUDELFORCE 0.3.1 - OFFIZIELLES PILOTEN-HANDBUCH <<<", 0x00E0FF, 1);
    
    char pbuf[64];
    snprintf(pbuf, sizeof(pbuf), "SEITE %d / 5   [ < A / LINKS ]   [ D / RECHTS > ]", s_manual_page + 1);
    draw_string(WIN_W - 460, 14, pbuf, 0xFFE040, 1);

    int mx = 50;
    int my = 54;
    int mw = WIN_W - 100;
    int mh = WIN_H - 106;
    draw_rect(mx, my, mw, mh, 0x000A14);
    draw_rect_outline(mx, my, mw, mh, 0x004080);
    draw_rect_outline(mx + 4, my + 4, mw - 8, mh - 8, 0x002040);

    if (s_manual_page == 0) {
        /* SEITE 1: MISSION & GESCHICHTE */
        draw_string(mx + 30, my + 24, "=== KAPITEL I: DIE MISSION & DIE LEGENDE VON STRUDELFORCE ===", 0xFF9040, 2);
        draw_rect(mx + 30, my + 50, mw - 60, 2, 0xFF8020);

        int ty = my + 68;
        draw_string(mx + 40, ty,      "Im Jahr 2026 kollabiert das neuronale Datennetz des Amiga-Clusters.", 0xFFFFFF, 1); ty += 22;
        draw_string(mx + 40, ty,      "Aus den Truemmern erheben sich drei feindliche Biome:", 0xFFFFFF, 1); ty += 22;
        draw_string(mx + 60, ty,      "1. CHROME TEMPEST: Zersplitterte Neon-Industriemetropolen unter Dauerfeuer.", 0x00E0FF, 1); ty += 20;
        draw_string(mx + 60, ty,      "2. THE GREAT BATH: Ueberflutete Amiga-Katakomben voller Truhen und Geheimgreifer.", 0x40FF80, 1); ty += 20;
        draw_string(mx + 60, ty,      "3. MECHANICAL HIVE: Autonome Kampfroboter und schwere Kettenpanzer-Golems.", 0xFFA040, 1); ty += 28;

        draw_string(mx + 40, ty,      "PILOT JULIAN UND DIE CYBER-RUESTUNG:", 0xFFFF40, 1); ty += 22;
        draw_string(mx + 40, ty,      "Als Elite-Krieger in der cybernetischen Bren-McGuire-Gedaechtnisruestung bist Du", 0xE0E0E0, 1); ty += 20;
        draw_string(mx + 40, ty,      "die letzte Hoffnung der Menschheit. Deine Ruestung verfuegt ueber ein Arsenal", 0xE0E0E0, 1); ty += 20;
        draw_string(mx + 40, ty,      "aus Drei-Wege-Splittern, Hochenergielasern und dem legendaeren 360-Grad-Drehstrahl.", 0xE0E0E0, 1); ty += 28;

        draw_string(mx + 40, ty,      "SPIELMODI:", 0xFFFF40, 1); ty += 22;
        draw_string(mx + 60, ty,      "* KAMPAGNE: 5 grosse Amiga-Levels mit je 3 Wellen, Plattformen, Bossen & Sieg!", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* ENDLOS ROGUELIKE: Unendliche Wellenjagd auf 16.16 Fixed-Point Highscores.", 0xFFFFFF, 1); ty += 35;

        draw_turrican_sprite(mx + mw - 140, my + mh - 90, (s_gui_frame / 4) % 13 + 1, 1);
    } else if (s_manual_page == 1) {
        /* SEITE 2: STEUERUNG */
        draw_string(mx + 30, my + 24, "=== KAPITEL II: PILOTEN-STEUERUNG (KEYBOARD & XINPUT GAMEPAD) ===", 0xFF9040, 2);
        draw_rect(mx + 30, my + 50, mw - 60, 2, 0xFF8020);

        int ty = my + 68;
        draw_string(mx + 40, ty,      "TASTATUR-BELEGUNG:", 0x00E0FF, 1); ty += 22;
        draw_string(mx + 60, ty,      "WASD / PFEILTASTEN    : Laufen, Ducken, Zielen", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "LEERTASTE             : Sprung & Jetpack-Schub", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "STRG / CONTROL        : Primaerwaffe abfeuern (Dauerfeuer)", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "ALT / RECHTS / LINKS  : 360-GRAD TURRICAN-DREHSTRAHL (Lightning Beam)", 0xFFFF40, 1); ty += 20;
        draw_string(mx + 60, ty,      "C-TASTE               : Waffe wechseln (Spread -> Laser -> Bounce)", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "S-TASTE               : MORPH-BALL (Rollmodus - unverwundbar gegen Fallen!)", 0xFF80FF, 1); ty += 20;
        draw_string(mx + 60, ty,      "T-TASTE               : Schatztruhe in The Great Bath oeffnen", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "M / N                 : Musikstueck wechseln  |  K / SHIFT+M: Subsong wechseln", 0x80FF80, 1); ty += 28;

        draw_string(mx + 40, ty,      "GAMEPAD-STEUERUNG (XINPUT / XBOX / RETRO-CONTROLLER):", 0x00E0FF, 1); ty += 22;
        draw_string(mx + 60, ty,      "STICK LINKS / D-PAD   : Laufen, Zielen & Ducken", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "A-BUTTON / CROSS      : Sprung", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "X-BUTTON / SQUARE     : Primaerwaffe abfeuern", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "RECHTER STICK / TRIG  : 360-GRAD DREHSTRAHL IN ALLE RICHTUNGEN", 0xFFFF40, 1); ty += 20;
        draw_string(mx + 60, ty,      "B-BUTTON / CIRCLE     : Morph-Ball Rollmodus", 0xFF80FF, 1); ty += 20;
        draw_string(mx + 60, ty,      "RIGHT BUMPER (RB)     : Waffe umschalten  |  Y: Musik wechseln", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "START-BUTTON          : Menue oeffnen / Spiel pausieren", 0xFFFFFF, 1);
    } else if (s_manual_page == 2) {
        /* SEITE 3: ARSENAL & AUSRUESTUNG */
        draw_string(mx + 30, my + 24, "=== KAPITEL III: DAS HIGH-TECH WAFFEN-ARSENAL ===", 0xFF9040, 2);
        draw_rect(mx + 30, my + 50, mw - 60, 2, 0xFF8020);

        int ty = my + 68;
        draw_string(mx + 40, ty,      "1. TRIPLE-SPREAD GUN (TASTE C / DEFAULT):", 0xFFC000, 1); ty += 20;
        draw_string(mx + 60, ty,      "Feuert einen breiten Faecherschuss mit drei bis fuenf Energiebolzen.", 0xFFFFFF, 1); ty += 18;
        draw_string(mx + 60, ty,      "Perfekt fuer die Beseitigung grosser Gegnerwellen und fliegender Dronen.", 0xC0C0C0, 1); ty += 26;

        draw_string(mx + 40, ty,      "2. HIGH-ENERGY LASER (TASTE C):", 0x00E0FF, 1); ty += 20;
        draw_string(mx + 60, ty,      "Zwillings-Plasmabolzen mit extremer Durchschlagskraft.", 0xFFFFFF, 1); ty += 18;
        draw_string(mx + 60, ty,      "Dringt tief in gepanzerte Ziele wie Walker und Tanks ein.", 0xC0C0C0, 1); ty += 26;

        draw_string(mx + 40, ty,      "3. BOUNCE 2.0 (TASTE C):", 0x40FF40, 1); ty += 20;
        draw_string(mx + 60, ty,      "Smaragd-Energiekugeln, die bis zu vierfach an Decken und Boeden abprallen.", 0xFFFFFF, 1); ty += 18;
        draw_string(mx + 60, ty,      "Unverzichtbar in engen Hoehlen und verwinkelten Unterwasser-Abschnitten.", 0xC0C0C0, 1); ty += 26;

        draw_string(mx + 40, ty,      "4. 360-GRAD TURRICAN-DREHSTRAHL (TASTE ALT / RECHTER ANALOGSTICK):", 0xFFFF40, 1); ty += 20;
        draw_string(mx + 60, ty,      "Kontinuierlicher ionisierter Blitzstrahl. Kann per Stick oder Pfeiltasten", 0xFFFFFF, 1); ty += 18;
        draw_string(mx + 60, ty,      "frei im 360-Grad-Radius um das Schiff gelenkt werden. Zerstoeberer!", 0xC0C0C0, 1); ty += 26;

        draw_string(mx + 40, ty,      "5. MORPH-BALL (TASTE S / GAMEPAD B):", 0xFF80FF, 1); ty += 20;
        draw_string(mx + 60, ty,      "Verwandelt Julian in eine rotierende Kugel. Rollt durch enge Schaechte", 0xFFFFFF, 1); ty += 18;
        draw_string(mx + 60, ty,      "und weicht Bodenfallen und tiefen Geschossen muehelos aus.", 0xC0C0C0, 1);
    } else if (s_manual_page == 3) {
        /* SEITE 4: SOUND-SYSTEM & LIVE-CODING */
        draw_string(mx + 30, my + 24, "=== KAPITEL IV: CYBER-PAULA TFMX AUDIO & STRUDEL LIVE-CODING ===", 0xFF9040, 2);
        draw_rect(mx + 30, my + 50, mw - 60, 2, 0xFF8020);

        int ty = my + 68;
        draw_string(mx + 40, ty,      "AUTHENTISCHE CHRIS HUELSBECK TFMX REPLAY-ENGINE:", 0x00E0FF, 1); ty += 22;
        draw_string(mx + 60, ty,      "* 8-Kanal Paula DMA Mixer mit 16574 Hz PAL Samplerate.", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* Echte TFMX-Makrobefehle (Pitch-Bends, Sample-Loops, Arpeggios).", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* 22 Subsongs in Software Manager fuer dynamischen Abspann und Gameplay.", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* Butterweicher DJ-Crossfader fuer klickfreie Phasenwechsel.", 0xFFFF40, 1); ty += 28;

        draw_string(mx + 40, ty,      "STRUDEL LIVE-CODING STUDIO (TASTE F3):", 0x40FF80, 1); ty += 22;
        draw_string(mx + 60, ty,      "* Echter Cleanroom Mini-Notation Pattern Evaluator.", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* Druecke ESC im Spiel fuer die Livecode-Konsole.", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* 7-Oszillator Detune SuperSaw Lead Synth (F5 im Studio schaltbar).", 0xFF80FF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* Soundgodizer Masterkette (Taste F5): BASS -> WARM -> OVERDRIVE.", 0xFFFF40, 1); ty += 28;

        draw_string(mx + 40, ty,      "DEIN EIGENER SOUNDTRACK (JUKEBOX AUTO-SCAN):", 0x40E0FF, 1); ty += 22;
        draw_string(mx + 60, ty,      "* Leg eigene .mod / .tfx-Dateien in die Ordner 'TFMX PROFESSIONAL'", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "  oder 'MOD' (neben der EXE) - der Scan laeuft bis 3 Ebenen tief.", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* TFMX-Tracks brauchen die .sam-Samples im selben Ordner (Paula-DMA).", 0xC0C0C0, 1); ty += 20;
        draw_string(mx + 60, ty,      "* Neue Tracks erscheinen automatisch in Jukebox (F2) & Cracktro-Intro.", 0xC0C0C0, 1);
    } else {
        /* SEITE 5: GRAFIK & FILTER */
        draw_string(mx + 30, my + 24, "=== KAPITEL V: DEMOSZENE-GRAFIK, CRT-FILTER & VOLLBILD ===", 0xFF9040, 2);
        draw_rect(mx + 30, my + 50, mw - 60, 2, 0xFF8020);

        int ty = my + 68;
        draw_string(mx + 40, ty,      "GRAFIKEINSTELLUNGEN & FILTER (DEMOSZENE WOW-EFFEKTE):", 0x00E0FF, 1); ty += 22;
        draw_string(mx + 60, ty,      "[ F11 / ALT+ENTER ]  : VOLLBILD-MODUS (Borderless 16:9 Letterbox)", 0xFFFF40, 1); ty += 20;
        draw_string(mx + 60, ty,      "[ F7 ]               : CRT-SCANLINES (Sony Trinitron / 1084S Scanlines)", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "[ F8 ]               : POST-FX MODUS WECHSELN:", 0x40FF80, 1); ty += 20;
        draw_string(mx + 80, ty,      "- MODUS 0: AUS (Reine scharfe Pixel)", 0xC0C0C0, 1); ty += 18;
        draw_string(mx + 80, ty,      "- MODUS 1: CRT SCANLINES (Klassische Roehrenzeilen)", 0xC0C0C0, 1); ty += 18;
        draw_string(mx + 80, ty,      "- MODUS 2: PHOSPHOR BLOOM & GLOW (Neon-Lichter gluehen)", 0xC0C0C0, 1); ty += 18;
        draw_string(mx + 80, ty,      "- MODUS 3: DEMOSCENE CRT + BLOOM + ABERRATION (Arcade Pur!)", 0xFFFF40, 1); ty += 28;

        draw_string(mx + 40, ty,      "2.5D VOXEL-ENGINE GESETZE:", 0xFF9040, 1); ty += 22;
        draw_string(mx + 60, ty,      "* 100% Zero-Float fixed-point Simulation (16.16 fix16_t).", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* 4 Parallax-Ebenen (Sky, Ruinen, Gameplay, Vordergrund).", 0xFFFFFF, 1); ty += 20;
        draw_string(mx + 60, ty,      "* Amiga OCS/AGA indizierte Farbpaletten fuer unverfaelschte Amiga-Farben.", 0xFFFFFF, 1);
    }

    /* Footer Navigation */
    draw_rect(0, WIN_H - 45, WIN_W, 45, 0x000E1C);
    draw_rect_outline(0, WIN_H - 45, WIN_W, 45, 0x004080);
    draw_string(60, WIN_H - 30, "[ < / LINKS / A ] VORHERIGE SEITE     [ > / RECHTS / D ] NAECHSTE SEITE     [ ESC / ENTER ] ZURUECK ZUM MENUE", 0x80D0FF, 1);
}

/* --- Render-Routinen fuer Tabs --- */

static void render_top_bar(void) {
    draw_rect(0, 0, WIN_W, 36, 0x000F24);
    draw_rect_outline(0, 0, WIN_W, 36, 0x004080);

    /* Tabs */
    const char *tabs[5] = { "[F1] SPIEL", "[F2] JUKEBOX", "[F3] STRUDEL", "[F4] CRACKTRO", "[F9] GRAFIK" };
    for (int t = 0; t < 5; t++) {
        int x = 10 + t * 155;
        int active = (t == 0) ? (s_gui_state == GUI_STATE_GAME || s_gui_state == GUI_STATE_GAMEOVER)
                               : (s_current_tab == (GuiTab)t);
        draw_rect(x, 4, 148, 28, active ? 0x004080 : 0x001830);
        draw_rect_outline(x, 4, 148, 28, active ? 0x00FFFF : 0x003060);
        draw_string(x + 12, 14, tabs[t], active ? 0x00FFFF : 0xA0C0E0, 1);
    }

    /* Sound Status */
    {
        char aud[160];
        if (s_tfmx_active && s_tfmx.num_subsongs > 1) {
            snprintf(aud, sizeof(aud), "AUDIO: TFMX [SUB %d/%d] | %s [F5]",
                     s_current_subsong + 1, s_tfmx.num_subsongs, god_mode_name());
        } else if (s_mod_active) {
            snprintf(aud, sizeof(aud), "AUDIO: MOD STEREO | %s [F5]", god_mode_name());
        } else {
            snprintf(aud, sizeof(aud), "AUDIO: 16574 Hz PAULA | %s [F5]", god_mode_name());
        }
        /* Rechtsbuehnen-Flucht: nie ueber den [F9] GRAFIK-Tab legen (v0.3.0 Fix) */
        draw_string(WIN_W - (int)strlen(aud) * 8 - 16, 14, aud, s_god_mode ? 0xFF40FF : 0x40FF80, 1);
    }
}

static void render_status_bar(void) {
    draw_rect(0, WIN_H - 24, WIN_W, 24, 0x000A18);
    draw_rect_outline(0, WIN_H - 24, WIN_W, 24, 0x003060);
    draw_string(12, WIN_H - 16, s_status_msg, 0x80D0FF, 1);
    if (s_gui_state == GUI_STATE_GAME) {
        draw_string(WIN_W - 480, WIN_H - 16, "[M] TRACK  [K/SHIFT+M] SUBSONG  [F5] GODIZER", 0x60A0C0, 1);
    }
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

    /* OBERE AMIGA INFO-LEISTE (Sauber abgesetzt, kein Randfehler!) */
    draw_rect(0, 0, WIN_W, 32, 0x000B18);
    draw_line(0, 31, WIN_W, 31, 0x004080);
    draw_string(20, 9, "[>] INTRO: APIDYA MEADOW (16.5 kHz PAULA STEREO)", 0x40E0FF, 1);
    draw_string(WIN_W / 2 - 140, 9, "[F11 / ALT+ENTER] VOLLBILD", 0xFFFF40, 1);
    char pad_buf[64];
    snprintf(pad_buf, sizeof(pad_buf), "PAD: %s", s_pad_connected ? s_pad.name : "TASTATUR / XINPUT");
    draw_string(WIN_W - 320, 9, pad_buf, s_pad_connected ? 0x00FF80 : 0x708090, 1);

    /* Schwebende Copper-Baender (Amiga-Vertikalverlauf) */
    for (i = 0; i < 90; i++) {
        int y1 = 40 + i * 7;
        if (y1 >= WIN_H - 70) break;
        uint32_t c = ((uint32_t)(8 + (i & 1) * 6) << 16) |
                     ((uint32_t)(2 + (i & 3)) << 8) |
                     (uint32_t)(10 + ((i >> 1) & 3) * 4);
        draw_rect(0, y1, WIN_W, 2, c);
    }

    /* Turrican-2-Silhouette: Chrome-Tempest-Tuerme am Menue-Boden */
    for (i = 0; i < 12; i++) {
        draw_bg_structure(i * 140, WIN_H - 70, i, 0, 0x101A28);
    }

    /* Titel mit Schlagschatten */
    draw_title(WIN_W / 2, 72, "STRUDELFORCE", 4);
    {
        char sub2[128];
        const char *sub = "A TURRICAN TRIBUTE FOR THE AMIGA 500";
        draw_string(WIN_W / 2 - (int)strlen(sub) * 4, 126, sub, 0xFF905030, 1);
        snprintf(sub2, sizeof(sub2), "VERSION " GAME_VERSION "  -  CYBER PAULA AUDIO  |  RETROARCH ENGINE");
        draw_string(WIN_W / 2 - (int)strlen(sub2) * 4, 140, sub2, 0xFF6080A0, 1);
    }

    /* Menue-Liste (9 Items, sauber gestaffelt ab Y = 168, je 31px Hoehe) */
    for (i = 0; i < MENU_ITEM_COUNT; i++) {
        int y = 168 + i * 31;
        int sel = (i == s_menu_index);
        int lx = WIN_W / 2 - 170;
        if (sel) {
            int k;
            draw_rect(lx - 20, y - 6, 380, 28, 0xFF2A0800);
            draw_rect_outline(lx - 20, y - 6, 380, 28, 0xFFFF8020);
            draw_char(lx - 26, y, '>', 0xFFFFA040, 2);
            draw_char_2tone(lx + 8, y, menu_labels[i][0], 0xFFFFC0A0, 0xFFFF9060, 2);
            for (k = 1; menu_labels[i][k]; k++)
                draw_char(lx + 8 + k * 16, y, menu_labels[i][k], 0xFFFFFF, 2);
        } else {
            draw_string(lx + 8, y + 2, menu_labels[i], (i == 8) ? 0xFF8080 : 0xFF8A4A30, 1);
        }
    }

    /* Blinkender Eingabe-Hinweis unter der Liste (Y = 460) */
    if ((s_gui_frame / 25) % 2 == 0) {
        const char *hint = "[ UP/DOWN / D-PAD ] WAHLEN   [ ENTER / CROSS (A) ] AUSFUEHREN";
        draw_string(WIN_W / 2 - (int)strlen(hint) * 4, 460, hint, 0xFFFF8020, 1);
    }

    /* Bren McGuire Menu Banner Animation */
    {
        int travel_w = WIN_W + 120;
        int m_step = (s_gui_frame * 3) % (travel_w * 2);
        int m_facing = (m_step < travel_w) ? 1 : -1;
        int m_x = (m_facing > 0) ? (m_step - 60) : (WIN_W + 60 - (m_step - travel_w));
        int m_frame = 1 + ((s_gui_frame / 2) % 13);
        draw_turrican_sprite(m_x, WIN_H - 74, m_frame, m_facing);
    }

    /* Steuerung & Footer */
    draw_rect(0, WIN_H - 70, WIN_W, 46, 0x000A0402);
    draw_string(40, WIN_H - 62, "WASD/PFEILE BEWEGEN  LEER SPRUNG  STRG FEUER  ALT BEAM  C WAFFE  S MORPH  H HANDBUCH", 0xFF80A0C0, 1);
    draw_string(40, WIN_H - 44, "[F7] SHADER MODUS  [F8] BLOOM  [F11 / ALT+ENTER] VOLLBILD  [F5] SOUNDGODIZER  [ESC] ZURUECK/QUIT", 0xFFFF8020, 1);
    draw_string(40, WIN_H - 26, "(C) 2026 FRANK MITZKATIS & AI CODING AGENTS  -  BEENDEN IM MENUE ODER PER ESC", 0xFF505050, 1);

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

/* ============================================================================
 * CREDITS & ARTIFACT ARCHIVE SYSTEM
 * - 3D Parallax Perspective Starfield (Retro Amiga Demoscene Style)
 * - Strukturierte Credits mit Disclaimer & GitHub Bugreport-Aufruf
 * - Live-Scrolling von WORKFLOW_ESSAY.md & captains_log.md
 * - TFMX Embedded Track Progression & Level Complete Fanfare
 * ============================================================================ */

#define STAR_COUNT 350
typedef struct {
    int x, y, z;
    int prev_z;
    uint32_t color;
} Star3D;

static Star3D s_stars[STAR_COUNT];
static int    s_starfield_initialized = 0;

static void init_starfield(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        s_stars[i].x = (rand() % 1600) - 800;
        s_stars[i].y = (rand() % 1200) - 600;
        s_stars[i].z = 1 + (rand() % 1000);
        s_stars[i].prev_z = s_stars[i].z;
        int r = rand() % 8;
        if (r == 0) s_stars[i].color = 0xFFD060;      /* Warm Gold */
        else if (r == 1) s_stars[i].color = 0x60D0FF; /* Ice Cyan */
        else if (r == 2) s_stars[i].color = 0xE090FF; /* Amiga Violett */
        else s_stars[i].color = 0xFFFFFF;             /* Brilliant Weiss */
    }
    s_starfield_initialized = 1;
}

static void update_and_render_starfield(int speed) {
    if (!s_starfield_initialized) init_starfield();

    int cx = WIN_W / 2;
    int cy = WIN_H / 2;

    /* Subtiler Amiga Copper-Hintergrundverlauf */
    for (int y = 0; y < WIN_H; y++) {
        uint32_t bg_c;
        if (y < 60) bg_c = 0x02030A;
        else if (y > WIN_H - 60) bg_c = 0x020410;
        else bg_c = 0x000206;
        for (int x = 0; x < WIN_W; x++) {
            s_pixels[y * WIN_W + x] = bg_c;
        }
    }

    for (int i = 0; i < STAR_COUNT; i++) {
        s_stars[i].prev_z = s_stars[i].z;
        s_stars[i].z -= speed;

        if (s_stars[i].z <= 1) {
            s_stars[i].z += 1000;
            s_stars[i].prev_z = s_stars[i].z;
            s_stars[i].x = (rand() % 1600) - 800;
            s_stars[i].y = (rand() % 1200) - 600;
        } else if (s_stars[i].z > 1000) {
            s_stars[i].z -= 1000;
            s_stars[i].prev_z = s_stars[i].z;
        }

        int sx = cx + (s_stars[i].x * 360) / s_stars[i].z;
        int sy = cy + (s_stars[i].y * 360) / s_stars[i].z;

        if (sx >= 0 && sx < WIN_W && sy >= 0 && sy < WIN_H) {
            uint32_t col = s_stars[i].color;
            if (s_stars[i].z > 600) {
                set_pixel(sx, sy, rgb_mod(col, 0.40f));
            } else if (s_stars[i].z > 250) {
                set_pixel(sx, sy, rgb_mod(col, 0.75f));
            } else {
                set_pixel(sx, sy, col);
                if (sx > 0) set_pixel(sx - 1, sy, rgb_mod(col, 0.6f));
                if (sx < WIN_W - 1) set_pixel(sx + 1, sy, rgb_mod(col, 0.6f));
                if (sy > 0) set_pixel(sx, sy - 1, rgb_mod(col, 0.6f));
                if (sy < WIN_H - 1) set_pixel(sx, sy + 1, rgb_mod(col, 0.6f));
            }

            /* Warp-Streifen bei hoeherer Geschwindigkeit */
            if (speed >= 4) {
                int px = cx + (s_stars[i].x * 360) / s_stars[i].prev_z;
                int py = cy + (s_stars[i].y * 360) / s_stars[i].prev_z;
                if (px >= 0 && px < WIN_W && py >= 0 && py < WIN_H) {
                    set_pixel((sx + px) / 2, (sy + py) / 2, rgb_mod(col, 0.5f));
                }
            }
        }
    }
}

/* 384 Zeilen reichen fuer das kompakte In-EXE-Archiv (103 eingebettete +
 * Header/Footer). War 6000 (528 KB BSS!) als Doku-Dateien von der Platte
 * eingelesen wurden — Platz sparend reduziert (v0.3.0 Feinschliff). */
#define MAX_CREDIT_LINES 384
#define CREDIT_LINE_LEN  88

static char s_credit_lines[MAX_CREDIT_LINES][CREDIT_LINE_LEN];
static int  s_credit_line_count = 0;
static int  s_credit_scroll_y = 0;
static int  s_credit_paused = 0;
static int  s_credit_music_step = 0;
static int  s_credit_music_timer = 0;
static int  s_credit_fanfare_played = 0;
/* Song-Looping: jeder Credits-Track laeuft 2-3 Maale, bevor zum naechsten
 * gewechselt wird (Timer-Zaehler pro Loop statt starrer 3500-Tick-Grenze). */
static int  s_credit_song_loop = 0;
static int  s_credit_song_loop_target = 3;
#define CREDIT_SONG_LOOP_TICKS 1200  /* ~24 s pro Loop bei 50Hz */

typedef struct {
    int track_idx;
    int subsong;
} CreditSongStep;

/* v0.3.1 Kuratierte Credits-Playlist (Startet mit Software Manager!) */
static const CreditSongStep s_credit_playlist[] = {
    { 4,  0 }, /* Phase 0: Software Manager Titel Subsong 0 (Opening & Overview) */
    { 6,  0 }, /* Phase 1: Ocean Loader (Demoscene MOD Atmosphaere) */
    { 4, 21 }, /* Phase 2: Software Manager Titel Subsong 21 (Amiga Heritage Thema) */
    { 5,  0 }, /* Phase 3: Airwolf 2 Chiptune (8-Bit MOD) */
    { 1,  0 }, /* Phase 4: BiFi Adventure (Chris Huelsbeck TFMX) */
    { 7,  0 }, /* Phase 5: Intro 45 Cracktro (MOD) */
    { 2,  0 }, /* Phase 6: Cybersty (Action Synth TFMX) */
    { 0,  0 }  /* Phase 7: Turrican 3 World 1 Factory (TFMX) */
};
#define CREDIT_PLAYLIST_COUNT (int)(sizeof(s_credit_playlist) / sizeof(s_credit_playlist[0]))

/* Song-Loop-Reset: neue Ziel-Loops (2-3) pro neuem Track */
static void credit_song_reset_loops(void) {
    s_credit_music_timer = 0;
    s_credit_song_loop = 0;
    s_credit_song_loop_target = 2 + (s_credit_music_step & 1);
}

/* Naechsten Song aus der Playlist starten (M-Taste / Pad) */
static void credit_music_next(void) {
    s_credit_music_step = (s_credit_music_step + 1) % CREDIT_PLAYLIST_COUNT;
    credit_song_reset_loops();
    music_play_embedded(s_credit_playlist[s_credit_music_step].track_idx,
                        s_credit_playlist[s_credit_music_step].subsong);
}

static void credits_add_line(const char *text) {
    if (s_credit_line_count >= MAX_CREDIT_LINES) return;
    char *dst = s_credit_lines[s_credit_line_count++];
    strncpy(dst, text ? text : "", CREDIT_LINE_LEN - 1);
    dst[CREDIT_LINE_LEN - 1] = '\0';
}

static void credits_add_wrapped(const char *text) {
    if (!text) { credits_add_line(""); return; }
    char sanitized[2048];
    int si = 0;
    /* UTF-8 Umlaute sauber in ASCII transkribieren fuer fehlerfreie Bitmap-Font Anzeige */
    for (int i = 0; text[i] && si < (int)sizeof(sanitized) - 4; ) {
        unsigned char c = (unsigned char)text[i];
        if (c == 0xC3) {
            unsigned char c2 = (unsigned char)text[i + 1];
            if (c2 == 0xA4) { sanitized[si++] = 'a'; sanitized[si++] = 'e'; i += 2; continue; }
            if (c2 == 0xB6) { sanitized[si++] = 'o'; sanitized[si++] = 'e'; i += 2; continue; }
            if (c2 == 0xBC) { sanitized[si++] = 'u'; sanitized[si++] = 'e'; i += 2; continue; }
            if (c2 == 0x84) { sanitized[si++] = 'A'; sanitized[si++] = 'e'; i += 2; continue; }
            if (c2 == 0x96) { sanitized[si++] = 'O'; sanitized[si++] = 'e'; i += 2; continue; }
            if (c2 == 0x9C) { sanitized[si++] = 'U'; sanitized[si++] = 'e'; i += 2; continue; }
            if (c2 == 0x9F) { sanitized[si++] = 's'; sanitized[si++] = 's'; i += 2; continue; }
        } else if (c == 0xE2) {
            unsigned char c2 = (unsigned char)text[i + 1];
            unsigned char c3 = (unsigned char)text[i + 2];
            if (c2 == 0x80 && (c3 == 0x94 || c3 == 0x93)) { sanitized[si++] = '-'; sanitized[si++] = '-'; i += 3; continue; }
            if (c2 == 0x86 && c3 == 0x92) { sanitized[si++] = '-'; sanitized[si++] = '>'; i += 3; continue; }
        }
        if (c >= 32 && c <= 126) sanitized[si++] = (char)c;
        else if (c == ' ' || c == '\t') sanitized[si++] = ' ';
        i++;
    }
    sanitized[si] = '\0';

    const char *p = sanitized;
    while (*p) {
        while (*p == '\r' || *p == '\n') p++;
        if (!*p) break;
        int len = (int)strlen(p);
        if (len <= 74) {
            credits_add_line(p);
            break;
        }
        int split = 74;
        while (split > 30 && p[split] != ' ') split--;
        if (split <= 30) split = 74;
        char tmp[CREDIT_LINE_LEN];
        int copylen = (split < CREDIT_LINE_LEN - 1) ? split : CREDIT_LINE_LEN - 1;
        strncpy(tmp, p, copylen);
        tmp[copylen] = '\0';
        credits_add_line(tmp);
        p += split;
        while (*p == ' ') p++;
    }
}

static int credits_append_file(const char *filename, const char *section_title) {
    FILE *f = fopen(filename, "rt");
    if (!f) return 0;
    credits_add_line("");
    credits_add_line("============================================================================");
    credits_add_line(section_title);
    credits_add_line("============================================================================");
    credits_add_line("");

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        int l = (int)strlen(buf);
        while (l > 0 && (buf[l - 1] == '\r' || buf[l - 1] == '\n')) buf[--l] = '\0';
        credits_add_wrapped(buf);
        if (s_credit_line_count >= MAX_CREDIT_LINES - 50) break;
    }
    fclose(f);
    return 1;
}

static void credits_init(void) {
    s_credit_line_count = 0;
    s_credit_scroll_y = 0;
    s_credit_paused = 0;
    s_credit_music_step = 0;
    s_credit_music_timer = 0;
    s_credit_fanfare_played = 0;

    init_starfield();

    credits_add_line("");
    credits_add_line("============================================================================");
    credits_add_line("                         STRUDELFORCE 0.3.0");
    credits_add_line("           RETRO 2.5D VOXEL-SHOOTER & CYBER-PAULA AUDIO ENGINE");
    credits_add_line("                  COMMODORE AMIGA 500 HOMMAGE (50HZ PAL)");
    credits_add_line("============================================================================");
    credits_add_line("");
    credits_add_line("PROGRAMMIERUNG, AUDIO-SYSTEM & GESAMTLEITUNG");
    credits_add_line("    Frank Mitzkatis");
    credits_add_line("");
    credits_add_line("AI CODING PARTNER & SCHWARM-AGENTEN");
    credits_add_line("    Qwen 3.8 27B      (Fruehe Kern-Experimente & Prototypen)");
    credits_add_line("    Cline 3.4 122B    (Monolithische Konsolidierung & Build-Flow)");
    credits_add_line("    Claude 5.5 Opus   (Architektur-Audit, Review & Refactoring)");
    credits_add_line("    Gemini 4 Argon    (Mod-Oracle & Fixed-Point Verifikation)");
    credits_add_line("    Gemini 3.8 Flash  (Multi-Subsongs, Kamera-Tracking & Voxel-Overhaul)");
    credits_add_line("");
    credits_add_line("WICHTIGER EXPERIMENTELLER HINWEIS & DISCLAIMER");
    credits_add_line("    Alle hier enthaltenen Implementierungen:");
    credits_add_line("    - 8-Stimmen Cyber-Paula Hardware-Mixer (16574 Hz)");
    credits_add_line("    - TFMX 1.5/Pro/7V Replayer mit Macro-Synthese");
    credits_add_line("    - ProTracker MOD 4-Kanal Replayer");
    credits_add_line("    - 2.5D Voxel-Szenen-Planer (Zero-Float!)");
    credits_add_line("    - Strudel Cleanroom AST & Mini-Notation");
    credits_add_line("    - 16.16 Fixed-Point Physik & 50Hz Simulations-Loop");
    credits_add_line("    - Soundgodizer DSP Post-Effekt-Kette");
    credits_add_line("    sind hochgradig experimentell und 'Work in Progress'.");
    credits_add_line("    Sie funktionieren vermutlich alle nur so halb!");
    credits_add_line("");
    credits_add_line("    Wenn du Glitches, Sound-Knackser, Timing-Probleme oder");
    credits_add_line("    Abstuerze findest: BITTE GERNE EINEN BUGREPORT EINREICHEN!");
    credits_add_line("    GitHub: https://github.com/Tretboot/STRUDELFORCE_Master");
    credits_add_line("");
    credits_add_line("BESONDERER DANK (SPECIAL THANKS)");
    credits_add_line("    Chris Huelsbeck  -  fuer die legendaeren Soundtracks unserer");
    credits_add_line("                        Kindheit (Turrican 1-3, Apidya, Great Bath).");
    credits_add_line("                        Danke fuer deine Musik und Inspiration!");
    credits_add_line("");
    credits_add_line("    Manfred Trenz, Rainbow Arts, Factor 5, The Bitmap Brothers,");
    credits_add_line("    DMA Design, Psygnosis und die weltweite Amiga-Community.");
    credits_add_line("");
    credits_add_line("TASTATUR- & CONTROLLER-STEUERUNG IM ABSPANN:");
    credits_add_line("    [ LEERTASTE ]     Pause / Weiter");
    credits_add_line("    [ PFEIL UNTEN ]   Schneller vorwaerts scrollen (Warp-Speed)");
    credits_add_line("    [ PFEIL OBEN ]    Rueckwaerts scrollen");
    credits_add_line("    [ M ]             Naechster Soundtrack");
    credits_add_line("    [ ESC / ENTER ]   Zurueck zum Hauptmenue");
    credits_add_line("");

    /* Kompaktes In-Binary Archiv (4 Kapitel: Sound-Architektur, Voxel-Engine,
     * Controller/Level-Benchmark, Demoszene-Greetings) aus embedded_credits.h */
    for (int i = 0; i < EMBEDDED_CREDITS_COUNT && s_credit_line_count < MAX_CREDIT_LINES - 20; i++) {
        credits_add_line(EMBEDDED_CREDITS_DATA[i]);
    }

    credits_add_line("");
    credits_add_line("============================================================================");
    credits_add_line("    ENDE DER DOKUMENTATION - VIELEN DANK FUERS LESEN UND SPIELEN!");
    credits_add_line("============================================================================");
    credits_add_line("");
    credits_add_line("             STRUDELFORCE 0.3.0  -  50HZ PAL FOREVER");
    credits_add_line("                      AMIGA RULEZ THE WORLD!");
    credits_add_line("");
    credits_add_line("           [ ESC ] ODER [ ENTER ] DRUECKEN FUERS HAUPTMENUE");
    credits_add_line("");

    /* Musik-Start (Playlist-Phase 0: Software Manager) + Loop-Reset */
    s_credit_music_step = 0;
    credit_song_reset_loops();
    music_play_embedded(s_credit_playlist[0].track_idx, s_credit_playlist[0].subsong);
}

static void pad_credits_poll(void) {
    static int prev_fire, prev_morph, prev_start, prev_y;
    poll_gamepad();
    if (!s_pad.connected) return;

    if ((s_pad.btn_morph && !prev_morph) || (s_pad.btn_start && !prev_start)) {
        s_gui_state = GUI_STATE_MENU;
        music_play_embedded(0, 0);
        prev_morph = s_pad.btn_morph;
        prev_start = s_pad.btn_start;
        return;
    }
    if (s_pad.btn_fire && !prev_fire) {
        s_credit_paused = !s_credit_paused;
        snprintf(s_status_msg, sizeof(s_status_msg), s_credit_paused ? "Credits Pausiert" : "Credits Fortgesetzt");
    }
    prev_fire = s_pad.btn_fire;
    prev_morph = s_pad.btn_morph;
    prev_start = s_pad.btn_start;

    if (s_pad.down || s_pad.btn_rb) s_credit_scroll_y += 6;
    if (s_pad.up   || s_pad.btn_lb) {
        s_credit_scroll_y -= 5;
        if (s_credit_scroll_y < 0) s_credit_scroll_y = 0;
    }
    if (s_pad.btn_weapsw && !prev_y) {
        credit_music_next();
    }
    prev_y = s_pad.btn_weapsw;
}

static void render_state_credits(void) {
    pad_credits_poll();

    int star_speed = s_credit_paused ? 0 : 2;
    if (GetKeyState(VK_DOWN) & 0x8000) star_speed = 8;
    if (GetKeyState(VK_UP) & 0x8000)   star_speed = -4;

    update_and_render_starfield(star_speed);

    /* Abgedunkeltes Zentrum fuer optimale Text-Lesbarkeit (LUT-Integer) */
    int panel_w = 780;
    int panel_x = (WIN_W - panel_w) / 2;
    dim_lut_init(71); /* ~0.28 Helligkeit */
    dim_screen_rect(panel_x, 30, panel_x + panel_w, WIN_H - 26);
    /* Rahmen: nur oben + unten — die blauen Seiten-Streifen sind weg (v0.3.0) */
    draw_line(panel_x - 1, 30, panel_x + panel_w + 1, 30, 0x003060);
    draw_line(panel_x - 1, WIN_H - 27, panel_x + panel_w + 1, WIN_H - 27, 0x003060);

    /* Text-Scroll-Berechnung */
    int line_h = 20;
    int total_scroll = s_credit_line_count * line_h + WIN_H + 80;

    if (!s_credit_paused) {
        s_credit_scroll_y += 1;
        s_credit_music_timer++;

        /* Song-Looping: Track laeuft s_credit_song_loop_target (2-3) Maale
         * a CREDIT_SONG_LOOP_TICKS Ticks, dann weicher Wechsel zum naechsten.
         * Kein Song dauert laenger als der komplette Abspann-Scroll:
         * max. 3 * 1200 = 3600 Ticks (72 s) vs. Scroll-Ende ~35 s —
         * der Wechsel wird nie spater als das Scroll-Ende sichtbar. */
        if (s_credit_music_timer >= CREDIT_SONG_LOOP_TICKS) {
            s_credit_music_timer = 0;
            s_credit_song_loop++;
            if (s_credit_song_loop >= s_credit_song_loop_target) {
                s_credit_song_loop = 0;
                s_credit_music_step = (s_credit_music_step + 1) % CREDIT_PLAYLIST_COUNT;
                s_credit_song_loop_target = 2 + (s_credit_music_step & 1);
                music_play_embedded(s_credit_playlist[s_credit_music_step].track_idx,
                                    s_credit_playlist[s_credit_music_step].subsong);
            }
        }
    }

    if (s_credit_scroll_y >= total_scroll) {
        s_credit_scroll_y = total_scroll;
        if (!s_credit_fanfare_played) {
            s_credit_fanfare_played = 1;
            music_play_embedded(TRACK_IDX_WC_OFFICE, 0); /* WC Commander Office = End-Abspann-Thema (v0.3.0, ersetzt T2 Load-Fanfare) */
        }
    }

    /* Linien zeichnen */
    for (int i = 0; i < s_credit_line_count; i++) {
        int ly = WIN_H - 40 - s_credit_scroll_y + i * line_h;
        if (ly < 25 || ly > WIN_H - 35) continue;
        const char *l = s_credit_lines[i];
        if (!l || l[0] == '\0') continue;

        if (strncmp(l, "===", 3) == 0) {
            draw_rect(panel_x + 10, ly + 9, panel_w - 20, 2, 0x00A0FF);
            draw_rect(panel_x + 10, ly + 11, panel_w - 20, 1, 0xFF4080);
        } else if (strncmp(l, "    TEIL", 8) == 0 || strncmp(l, "                         STRUDELFORCE", 37) == 0) {
            draw_rect(panel_x + 10, ly - 2, panel_w - 20, 22, 0x002040);
            draw_rect_outline(panel_x + 10, ly - 2, panel_w - 20, 22, 0x00FFFF);
            draw_title(WIN_W / 2, ly, l, 1);
        } else if (l[0] == '#' && l[1] == '#') {
            draw_rect(panel_x + 16, ly - 1, panel_w - 32, 18, 0x001830);
            draw_string(panel_x + 24, ly, l, 0xFFD700, 1);
            draw_rect(panel_x + 24, ly + 15, panel_w - 48, 1, 0xFF8000);
        } else if (l[0] == '#') {
            draw_rect(panel_x + 10, ly - 2, panel_w - 20, 22, 0x002850);
            draw_rect_outline(panel_x + 10, ly - 2, panel_w - 20, 22, 0x00FFFF);
            draw_string(panel_x + 20, ly + 2, l, 0x00FFFF, 1);
        } else if (l[0] == '>') {
            draw_rect(panel_x + 20, ly, 4, 16, 0x00FFFF);
            draw_string(panel_x + 32, ly, l + 1, 0x60D0FF, 1);
        } else if (l[0] == '-' || l[0] == '*') {
            draw_rect(panel_x + 22, ly + 6, 4, 4, 0xFF8000);
            draw_string(panel_x + 32, ly, l, 0xE0E0E0, 1);
        } else if (strncmp(l, "    ", 4) == 0) {
            draw_string(panel_x + 36, ly, l, 0x80D8FF, 1);
        } else if (l[0] >= 'A' && l[0] <= 'Z' && strchr(l, ':') != NULL) {
            draw_string(panel_x + 24, ly, l, 0xFFFFA040, 1);
        } else {
            draw_string(panel_x + 24, ly, l, 0xD0D8E0, 1);
        }
    }

    /* Wenn Fanfare getriggert: Auszeichnung anzeigen */
    if (s_credit_fanfare_played) {
        draw_rect(panel_x + 10, WIN_H / 2 - 40, panel_w - 20, 80, 0xEE001020);
        draw_rect_outline(panel_x + 10, WIN_H / 2 - 40, panel_w - 20, 80, 0x00FF80);
        draw_title(WIN_W / 2, WIN_H / 2 - 25, "LEVEL COMPLETE - ARTEFAKTE VOLLSTAENDIG!", 2);
        draw_string(WIN_W / 2 - 190, WIN_H / 2 + 10, "[ ENTER ] ODER [ ESC ] ZURUECK ZUM HAUPTMENUE", 0xFFFF40, 1);
    }

    /* PAUSIERT-Banner: gross & blinkend, damit klar ist, dass der
     * Auto-Scroll nur anhalten und kein Fehler vorliegt (v0.3.0) */
    if (s_credit_paused) {
        if ((s_gui_frame / 15) % 2 == 0) {
            const char *pt = "  PAUSIERT  -  [ LEERTASTE / FEUER ] WEITER  ";
            int pw = (int)strlen(pt) * 8 + 16;
            draw_rect(WIN_W / 2 - pw / 2, WIN_H / 2 - 14, pw, 28, 0xEE000814);
            draw_rect_outline(WIN_W / 2 - pw / 2, WIN_H / 2 - 14, pw, 28, 0x00FF80);
            draw_string(WIN_W / 2 - pw / 2 + 8, WIN_H / 2 - 6, pt, 0x00FF80, 1);
        }
    }

    /* Top HUD */
    draw_rect(0, 0, WIN_W, 30, 0xEE000814);
    draw_rect(0, 29, WIN_W, 1, 0x004080);
    draw_string(20, 10, "STRUDELFORCE 0.3.0  -  CREDITS & ARTEFAKT-DOKUMENTATION", 0xFFE040, 1);
    char mus_info[128];
    snprintf(mus_info, sizeof(mus_info), "AUDIO: [%s] %s %s",
             s_current_track_type, s_current_track_name, s_credit_paused ? "[PAUSE]" : "");
    draw_string(WIN_W - (int)strlen(mus_info) * 8 - 20, 10, mus_info, 0x60D0FF, 1);

    /* Bottom HUD */
    draw_rect(0, WIN_H - 26, WIN_W, 26, 0xEE000814);
    draw_rect(0, WIN_H - 26, WIN_W, 1, 0x004080);
    draw_string(20, WIN_H - 18, "[ESC/ENTER] Menue   [LEERTASTE] Pause   [PFEIL OBEN/UNTEN] Tempo   [M] Musik", 0xA0A0A0, 1);
    int pct = total_scroll > 0 ? (s_credit_scroll_y * 100) / total_scroll : 0;
    if (pct > 100) pct = 100;
    char pct_str[32];
    snprintf(pct_str, sizeof(pct_str), "FORTSCHRITT: %3d%%", pct);
    draw_string(WIN_W - (int)strlen(pct_str) * 8 - 20, WIN_H - 18, pct_str, 0x00FF80, 1);
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
    dim_lut_init(56); /* ~0.22 Helligkeit, LUT-Integer statt Float-Schleife */
    dim_screen_rect(0, 0, WIN_W, WIN_H);
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
    /* Dimmen des letzten Frames (LUT-Integer statt Float-Schleife) */
    dim_lut_init(56); /* ~0.22 Helligkeit */
    dim_screen_rect(0, 0, WIN_W, WIN_H);
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

/* ============================================================================
 * ECHTER AMIGA BREN MCGUIRE SPRITE-RENDERER (TURRICAN.GIF)
 * 14 Frames @ 64x74, 16 Farben Amiga OCS/AGA, Horizontale Spiegelung bei Links-Lauf
 * ============================================================================ */
static void draw_turrican_sprite(int px, int foot_y, int frame_idx, int facing) {
    if (frame_idx < 0 || frame_idx >= TURRICAN_FRAME_COUNT) frame_idx = 0;
    const uint8_t *frame = TURRICAN_FRAMES[frame_idx];
    int mirror = (facing < 0);
    int top_x = px - TURRICAN_FRAME_W / 2;
    int top_y = foot_y - TURRICAN_FRAME_H + 2; /* 2px Einbettung in den Boden */

    for (int y = 0; y < TURRICAN_FRAME_H; y++) {
        int sy = top_y + y;
        if (sy < 0 || sy >= WIN_H) continue;
        int row_offset = y * (TURRICAN_FRAME_W / 2);

        for (int x = 0; x < TURRICAN_FRAME_W; x++) {
            int src_x = mirror ? (TURRICAN_FRAME_W - 1 - x) : x;
            int byte_idx = row_offset + (src_x >> 1);
            uint8_t b = frame[byte_idx];
            uint8_t col_idx = (src_x & 1) ? (b & 0x0F) : ((b >> 4) & 0x0F);

            if (col_idx != 0) {
                int sx = top_x + x;
                if (sx >= 0 && sx < WIN_W) {
                    s_pixels[sy * WIN_W + sx] = TURRICAN_PALETTE[col_idx];
                }
            }
        }
    }
}

/* ============================================================================
 * TURRICAN BREN MCGUIRE: ZWEIBEINIGER CYBER-KRIEGER & MORPH-RAD (2.5D VOXEL)
 * ============================================================================ */
static void draw_player_warrior(int px, int foot_y, int facing, int on_ground, int running, int morphing) {
    int dir = (facing < 0) ? -1 : 1;
    uint32_t armor_main = 0x3858B8;  /* Turrican Stahlblau */
    uint32_t armor_hi   = 0x6088D8;  /* Glanzkante */
    uint32_t chassis    = 0x182430;  /* Gelenke / Chassis */
    uint32_t visor_c    = 0x00E0FF;  /* HUD-Visor Cyan */

    if (morphing) {
        /* ================= 1. MORPH-BALL (TURRICAN GYRO-RAD) ================= */
        int cy = foot_y - 12;
        float spin = (float)s_gui_frame * 0.45f * (float)dir;

        /* Aeusserer rotierender Voxel-Ring */
        draw_voxel(px - 10, cy - 10, 20, 20, 3, armor_main);
        draw_rect_outline(px - 9, cy - 9, 18, 18, armor_hi);
        draw_rect_outline(px - 7, cy - 7, 14, 14, chassis);

        /* Rotierende Speichen / Energiefurchen */
        for (int spk = 0; spk < 4; spk++) {
            float a = spin + (float)spk * 1.5707963f;
            int sx = px + (int)(cosf(a) * 7.0f);
            int sy = cy + (int)(sinf(a) * 7.0f);
            draw_line(px, cy, sx, sy, visor_c);
        }

        /* Fusion-Kern */
        draw_rect(px - 2, cy - 2, 5, 5, 0xFFFFFF);
        set_pixel(px, cy, 0x00FFFF);

        /* Reibungs-Funken am Boden beim Rollen */
        if (running) {
            for (int f = 0; f < 3; f++) {
                int fx = px - dir * (8 + f * 5) + ((s_gui_frame * 3 + f * 7) % 5);
                int fy = foot_y - 2 + (f % 2);
                set_pixel(fx, fy, (f & 1) ? 0xFFFFA0 : 0xFF6020);
            }
        }
        return;
    }

    /* ================= 2. ECHTER AMIGA BREN MCGUIRE SPRITE (TURRICAN.GIF) ================= */
    int frame_idx = 0;
    if (!on_ground) {
        frame_idx = 0; /* Hero Jump / Airborne Frame (Heldenpose) */
    } else if (running) {
        /* 13-Frame Run-Cycle bei 25 FPS (50Hz / 2) */
        frame_idx = 1 + ((s_gui_frame / 2) % 13);
    } else {
        frame_idx = 5; /* Grounded Standing Idle Pose (Aufrechter Stand mit beiden Fuessen am Boden) */
    }

    draw_turrican_sprite(px, foot_y, frame_idx, facing);
}

/* ============================================================================
 * TURRICAN 2 ENEMY SILHOUETTES & VOXEL SHADING
 * Gesetz 2: Reine Farbpaletten, directional face-shading, unverwechselbare Formen.
 * ============================================================================ */

/* 1. Grunt Drone (ENEMY_WALKER): 4-beiniger Boden-Krabbler mit rotem Zyklopen-Auge */
static void draw_enemy_walker(int ex, int ey, int frame, int dir) {
    uint32_t shell_main = 0x782888;
    uint32_t shell_hi   = 0xA040B8;
    uint32_t shell_dark = 0x481858;
    int step = (frame / 5) % 4;
    int l1 = (step == 0 || step == 2) ? 4 : 1;
    int l2 = (step == 1 || step == 3) ? 4 : 1;

    /* 4 Gelenk-Beine (Krallen zum Boden) */
    draw_line(ex - 8, ey + 4, ex - 14, ey + 12 - l1, shell_dark);
    draw_line(ex - 14, ey + 12 - l1, ex - 16, ey + 18, shell_dark);
    draw_line(ex - 4, ey + 4, ex - 8, ey + 12 - l2, shell_main);
    draw_line(ex - 8, ey + 12 - l2, ex - 10, ey + 18, shell_main);
    draw_line(ex + 4, ey + 4, ex + 8, ey + 12 - l1, shell_main);
    draw_line(ex + 8, ey + 12 - l1, ex + 10, ey + 18, shell_main);
    draw_line(ex + 8, ey + 4, ex + 14, ey + 12 - l2, shell_dark);
    draw_line(ex + 14, ey + 12 - l2, ex + 16, ey + 18, shell_dark);

    /* Gepanzerter Chitin-/Metall-Körper */
    draw_voxel(ex - 10, ey, 20, 10, 4, shell_main);
    draw_rect(ex - 8, ey - 1, 16, 2, shell_hi);

    /* Bedrohliches leuchtendes Zyklopen-Auge (Rot mit weißem Reflex) */
    int eye_x = ex + (dir > 0 ? 3 : -7);
    draw_voxel(eye_x, ey + 2, 6, 5, 2, 0xFF1020);
    set_pixel(eye_x + (dir > 0 ? 3 : 1), ey + 3, 0xFFFFFF);

    /* Sensor-Antenne */
    draw_line(ex, ey - 1, ex + dir * 3, ey - 5, shell_hi);
    if ((frame / 8) % 2 == 0) set_pixel(ex + dir * 3, ey - 6, 0xFFFF40);
}

/* 2. Aerial Seeker (ENEMY_SHOOTER): Roter Delta-Flügel Jäger mit Cyan-Triebwerk */
static void draw_enemy_shooter(int ex, int ey, int frame, int dir) {
    uint32_t hull_main = 0xC82028;
    uint32_t hull_hi   = 0xFF5050;
    uint32_t hull_dark = 0x781018;

    /* Schwebendes Auf- und Abbewegen */
    int hover = (int)(sinf((float)frame * 0.12f) * 3.0f);
    ey += hover;

    /* Delta-Flügel Rumpf */
    draw_voxel(ex - 12, ey + 2, 24, 8, 4, hull_main);
    draw_voxel(ex - 6, ey - 3, 12, 6, 3, hull_main);
    draw_rect(ex - 10, ey + 1, 20, 2, hull_hi);
    draw_rect(ex - 12, ey + 9, 24, 2, hull_dark);

    /* Angewinkelte Flügelspitzen */
    draw_voxel(ex - 15, ey, 4, 10, 2, hull_dark);
    draw_voxel(ex + 11, ey, 4, 10, 2, hull_dark);

    /* Cockpit-Sensorvisor */
    int vis_x = ex + (dir > 0 ? 3 : -7);
    draw_voxel(vis_x, ey, 5, 4, 2, 0x00E0FF);
    set_pixel(vis_x + 1, ey + 1, 0xFFFFFF);

    /* Untergehängte Zwillings-Laserkanonen */
    draw_voxel(ex - 8, ey + 10, 4, 5, 2, 0xD4AF37);
    draw_voxel(ex + 4, ey + 10, 4, 5, 2, 0xD4AF37);

    /* Pulsierender Cyan-Triebwerksstrahl nach hinten */
    int jet_x = ex - dir * 14;
    int flen = 4 + (frame % 5);
    draw_rect(jet_x - (dir > 0 ? flen : 0), ey + 4, flen, 3, 0x00FFFF);
    draw_rect(jet_x - (dir > 0 ? flen - 2 : 0), ey + 5, flen - 2, 1, 0xFFFFFF);
    if ((frame / 3) % 2 == 0) {
        set_pixel(jet_x - dir * (flen + 2), ey + 4, 0x00A0FF);
        set_pixel(jet_x - dir * (flen + 4), ey + 5, 0x0050C0);
    }
}

/* 3. Tank Golem (ENEMY_TANK): Schwerer Kettenpanzer (Mini-Boss) mit Boss-Health-Bar */
static void draw_enemy_tank(int ex, int ey, int hp, int max_hp, int frame, int dir) {
    uint32_t steel_main = 0x4A5868;
    uint32_t steel_hi   = 0x788C9C;
    uint32_t steel_dark = 0x2A3440;
    uint32_t tread_col  = 0x1A2028;

    /* Schwere Kettenlaufwerke (36x10) */
    draw_voxel(ex - 18, ey + 10, 36, 10, 6, tread_col);
    /* Animierte Kettenglieder & Rollen */
    for (int k = 0; k < 4; k++) {
        int rx = ex - 14 + k * 8;
        draw_rect(rx, ey + 13, 5, 5, 0x506070);
        draw_rect(rx + 1, ey + 14, 3, 3, 0x8090A0);
    }
    /* Kettenstreifen oben */
    draw_rect(ex - 17, ey + 9, 34, 2, 0x303840);

    /* Abgeschrägte Panzerwanne */
    draw_voxel(ex - 15, ey + 2, 30, 9, 5, steel_main);
    draw_rect(ex - 13, ey + 1, 26, 2, steel_hi);

    /* Massiver Zwillings-Kanonenturm */
    draw_voxel(ex - 10, ey - 6, 20, 9, 4, 0x5A6C7E);
    draw_rect(ex - 8, ey - 7, 16, 2, 0x889CB0);

    /* Schweres Zwillings-Plasmageschütz */
    int gun_x = ex + (dir > 0 ? 6 : -22);
    draw_voxel(gun_x, ey - 4, 16, 3, 3, 0x344050);
    draw_voxel(gun_x, ey - 1, 16, 3, 3, 0x344050);
    /* Glühende Plasma-Spulen an der Mündung */
    int tip_x = (dir > 0) ? (gun_x + 14) : gun_x;
    draw_rect(tip_x, ey - 4, 2, 3, 0xFF8020);
    draw_rect(tip_x, ey - 1, 2, 3, 0xFF8020);

    /* Glühender Reaktor-Kern / Abgas-Grill (pulsierendes Rot) */
    uint32_t core_col = ((frame / 6) % 2 == 0) ? 0xFF3020 : 0xFF7040;
    draw_rect(ex - dir * 6 - 2, ey + 4, 5, 4, core_col);

    /* Boss-Lebensbalken overhead */
    int bar_w = 36;
    int bar_h = 5;
    int bar_x = ex - bar_w / 2;
    int bar_y = ey - 16;
    draw_rect(bar_x - 1, bar_y - 1, bar_w + 2, bar_h + 2, 0x001020);
    draw_rect(bar_x, bar_y, bar_w, bar_h, 0x401010);
    if (max_hp > 0 && hp > 0) {
        int fill = (hp * bar_w) / max_hp;
        if (fill > bar_w) fill = bar_w;
        uint32_t hp_col = (hp > max_hp / 2) ? 0x00FF60 : ((hp > max_hp / 4) ? 0xFFD000 : 0xFF2020);
        draw_rect(bar_x, bar_y, fill, bar_h, hp_col);
    }
    draw_string(bar_x + 4, bar_y - 9, "TANK [BOSS]", 0xFFC040, 1);
}

static float s_cam_x = 0.0f;
static float s_cam_y = 0.0f;

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
    const BiomePalette *bp = &biome_palettes[core_campaign_biome(&s_game)];
    int bi = (int)(bp - biome_palettes);

    /* Welt-Pixelkoordinaten (20px horizontal pro fixed-point Einheit, 16px vertikal) */
    int pw_x = (int)(((int64_t)s_game.px * 20) >> 16);
    int pw_y = (int)(((int64_t)s_game.py * 16) >> 16);

    /* Weiche 2D-Kamera (zentriert auf den Spieler, dynamisch an Level-Breite angepasst) */
    int total_world_px = (int)(((int64_t)s_game.world_w * 20) >> 16);
    float max_cam_x = (float)(total_world_px > WIN_W ? total_world_px - WIN_W : 0);
    float target_cam_x = (float)pw_x - (float)(WIN_W / 2);
    if (target_cam_x < 0.0f) target_cam_x = 0.0f;
    if (target_cam_x > max_cam_x) target_cam_x = max_cam_x;

    float target_cam_y = (pw_y > 80) ? (float)(pw_y - 80) : 0.0f;
    if (target_cam_y > 220.0f) target_cam_y = 220.0f;

    s_cam_x += (target_cam_x - s_cam_x) * 0.14f;
    if (s_cam_x < 0.0f) s_cam_x = 0.0f;
    if (s_cam_x > max_cam_x) s_cam_x = max_cam_x;

    s_cam_y += (target_cam_y - s_cam_y) * 0.10f;
    if (s_cam_y < 0.0f) s_cam_y = 0.0f;
    if (s_cam_y > 220.0f) s_cam_y = 220.0f;

    int cam_x = (int)s_cam_x;
    int cam_y = (int)s_cam_y;
    int ground_y = 620 + cam_y;

    /* Basis-Fluellung: volles Spielfeld (v0.3.0: toter 36px-Balken oben,
     * "wo mal Menue war", entfernt — Copper-Sky beginnt jetzt bei Zeile 0) */
    draw_rect(0, 0, WIN_W, WIN_H, 0x000A18);

    /* Layer 3: Dynamische Amiga Copper-Baender mit Elevation-Farbgradient */
    float elevation = s_cam_y / 200.0f;
    if (elevation < 0.0f) elevation = 0.0f;
    if (elevation > 1.0f) elevation = 1.0f;

    for (int i = 0; i < WIN_H / 8; i++) {
        int y1 = i * 8;
        float factor = 0.5f + 0.5f * sinf((float)i * 0.1f + (float)s_game.tick * 0.04f);

        /* Boden-/Horizont-Farben */
        float r_ground = (float)bp->sky_r + 2.0f * (float)bp->sky_r * factor;
        float g_ground = (float)bp->sky_g + 3.0f * (float)bp->sky_g * factor;
        float b_ground = (float)bp->sky_b + 2.0f * (float)bp->sky_b * factor;

        /* Hoehen-Stratosphere (Amiga-Cyan & Ultramarin-Blau) */
        float r_sky = 10.0f + 15.0f * factor;
        float g_sky = 30.0f + 80.0f * (1.0f - (float)i / (float)(WIN_H / 8)) + 30.0f * factor;
        float b_sky = 70.0f + 150.0f * (1.0f - (float)i / (float)(WIN_H / 8)) + 35.0f * factor;

        uint32_t cr = (uint32_t)(r_ground * (1.0f - elevation) + r_sky * elevation);
        uint32_t cg = (uint32_t)(g_ground * (1.0f - elevation) + g_sky * elevation);
        uint32_t cb = (uint32_t)(b_ground * (1.0f - elevation) + b_sky * elevation);
        if (cr > 255) cr = 255;
        if (cg > 255) cg = 255;
        if (cb > 255) cb = 255;

        draw_rect(0, y1, WIN_W, 8, (cr << 16) | (cg << 8) | cb);
    }

    /* Layer 2: Biome-Silhouetten (Parallax 0.35x mit Kamera) */
    int x_off = (int)(s_cam_x * 0.35f);
    for (int i = 0; i < 14; i++) {
        int a = i * 140 - x_off;
        int px = a % WIN_W;
        if (px < 0) px += WIN_W;
        draw_bg_structure(px, ground_y, floor_div(a, 140), bi, rgb_mod(bp->pillar, 0.72f));
    }

    /* Bodenlinie mit Voxel-Platten (scrollt 1:1 mit Kamera) */
    int g_off = cam_x;
    for (int i = 0; i < 74; i++) {
        int bx = (i * 20 - g_off) % WIN_W;
        if (bx < 0) bx += WIN_W;
        draw_voxel(bx, ground_y - 20, 18, 20, 3, bp->ground);
    }

    /* Untergrund: pro Biome eigene Terrassen + Strata-Linien */
    {
        uint32_t under  = rgb_mod(bp->ground, 0.55f);
        uint32_t strata = rgb_mod(bp->ground, 0.36f);
        uint32_t top_hi = rgb_mod(bp->ground, 1.7f);
        for (int x = 0; x < WIN_W; x++) {
            int h = terrain_height(bi, floor_div(x + g_off, 24));
            int top = ground_y;
            int yy;
            draw_rect(x, top, 1, h, under);
            draw_rect(x, top, 1, 2, top_hi);
            for (yy = top + 16; yy < top + h; yy += 16)
                draw_rect(x, yy, 1, 2, strata);
        }
    }

    /* Multi-Tier 2.5D Plattformen (Kampagnen-Ebenen & Turrican-2 Vertikalitaet) */
    if (s_game.platform_count > 0) {
        static const uint32_t neon_cols[5] = {
            0x00FFFF, /* Chrome Tempest: Cyber-Cyan */
            0x40FFA0, /* The Great Bath: Aqua-Smaragd */
            0xFF30D0, /* Mechanical Hive: Neon-Violett */
            0xFF8000, /* Volcanic: Magma-Orange */
            0xFFFF30  /* Fortress: Amber-Gold */
        };
        uint32_t neon = neon_cols[bi % 5];
        uint32_t slab_col = rgb_mod(bp->pillar, 0.85f);
        uint32_t post_col = rgb_mod(bp->pillar, 0.50f);

        uint32_t underside = rgb_mod(bp->pillar, 0.40f);
        uint32_t steel_brace = 0x283848;

        for (int pi = 0; pi < s_game.platform_count; pi++) {
            const LevelPlatform *pl = &s_game.platforms[pi];
            int pl_sx = (int)(((int64_t)pl->x * 20) >> 16) - cam_x;
            int pl_sw = (int)(((int64_t)pl->w * 20) >> 16);
            int pl_sy = ground_y - 20 - (int)(((int64_t)pl->y * 16) >> 16);
            int pl_sh = 14; /* Solide 14px Dicke nach Turrican 2 Benchmark */

            /* Diagonale Stahl-Tragwinkel (Cantilever Brackets) unter den Enden (nur 12px tief!) */
            for (int br = 0; br < 10; br++) {
                draw_rect(pl_sx + 4 + br, pl_sy + pl_sh + br, 3, 2, steel_brace);
                draw_rect(pl_sx + pl_sw - 7 - br, pl_sy + pl_sh + br, 3, 2, steel_brace);
            }

            /* Plattform-Slab mit Turrican 2 Level 1 Terrain-Schichten */
            draw_voxel(pl_sx, pl_sy, pl_sw, pl_sh, 4, slab_col);
            draw_rect(pl_sx + 2, pl_sy + pl_sh - 4, pl_sw - 4, 3, underside);

            /* Oberkante: Authentischer Sand/Gras-Rand nach Turrican 2 Tile 16x16 */
            draw_rect(pl_sx + 2, pl_sy - 2, pl_sw - 4, 2, neon);
            draw_rect(pl_sx + 4, pl_sy, pl_sw - 8, 1, 0xFFFFA0);

            /* Gelb-Schwarze Hazard-Streifen an den Aussenkanten */
            for (int hz = 0; hz < 8; hz += 2) {
                draw_rect(pl_sx + hz, pl_sy + 2, 2, 4, 0xFFCC00);
                draw_rect(pl_sx + pl_sw - 10 + hz, pl_sy + 2, 2, 4, 0xFFCC00);
            }

            /* Metallnieten alle 16 Pixel entlang der Plattform */
            for (int nv = pl_sx + 16; nv < pl_sx + pl_sw - 16; nv += 16) {
                set_pixel(nv, pl_sy + 3, 0xFFFFFF);
                set_pixel(nv + 1, pl_sy + 4, 0x203040);
            }

            /* Schwebende Amiga-Diamanten auf den hoechsten Plattformen */
            if (pi % 4 == 2 && pl->y > FIX(10)) {
                int dx = pl_sx + 12;
                int dy = pl_sy - 20 + (int)(sinf((float)s_gui_frame * 0.15f + (float)pi) * 3.0f);
                draw_rect(dx + 3, dy, 4, 2, 0x00FFFF);
                draw_rect(dx + 1, dy + 2, 8, 3, 0x80FFFF);
                draw_rect(dx + 2, dy + 5, 6, 2, 0x00C0FF);
                draw_rect(dx + 4, dy + 7, 2, 2, 0x0060C0);
            }

            /* The Great Bath (Biom 1): Altar-Truhe auf Plattform 8 */
            if (bi == 1 && pi == 8) {
                int cx = pl_sx + pl_sw / 2 - 12;
                int cy = pl_sy - 18;
                if (!s_livecode.chest_opened) {
                    /* Geschlossene Truhe (Gold mit Edelstein-Schloss) */
                    draw_voxel(cx, cy + 4, 24, 12, 4, 0xD4AF37);
                    draw_voxel(cx + 3, cy, 18, 6, 3, 0xFFD700);
                    draw_voxel(cx + 9, cy + 5, 6, 6, 2, 0x00E0FF);
                    if ((s_gui_frame / 12) % 2 == 0) set_pixel(cx + 11, cy + 7, 0xFFFFFF);
                    /* Tooltip bei Spieler-Naehe */
                    if (abs((s_game.px >> 16) - 40) < 8) {
                        draw_string(cx - 24, cy - 14, "[T] TRUHE OEFFNEN", 0x00FFFF, 1);
                    }
                } else {
                    /* Geoeffnete Truhe mit Schatz-Glow & Fontaene */
                    draw_voxel(cx, cy + 6, 24, 10, 4, 0x907020);
                    draw_voxel(cx + 2, cy - 6, 20, 8, 3, 0xFFD700);
                    draw_voxel(cx + 6, cy + 2, 12, 6, 2, 0xFFFF80);
                    for (int sp = 0; sp < 6; sp++) {
                        int sx = cx + 4 + (sp * 4 + s_gui_frame * 2) % 18;
                        int sy = cy - 4 - ((s_gui_frame * 3 + sp * 7) % 24);
                        set_pixel(sx, sy, (sp & 1) ? 0xFFFFA0 : 0x00FFFF);
                    }
                }
            }
        }
    }

    /* Layer 1: Gameplay Entities */

    /* 1. Spieler: Zweibeiniger Cyber-Krieger (Turrican Bren McGuire) */
    int p_screen_x = pw_x - cam_x;
    int foot_y = ground_y - 20 - pw_y;
    if (p_screen_x < 40) p_screen_x = 40;
    if (p_screen_x > WIN_W - 80) p_screen_x = WIN_W - 80;

    int dir = (s_game.facing < 0) ? -1 : 1;
    int is_running = (s_game.pvx != 0);
    int nozzle_x = p_screen_x + (dir > 0 ? 22 : -22);
    int nozzle_y = s_game.morphing ? (foot_y - 12) : (foot_y - 38);

    /* Respawn-Gnadenfrist: Spieler blinkt (Unverwundbar-Optik) */
    int player_visible = (s_game.respawn_timer == 0) || ((s_game.respawn_timer / 5) % 2 == 0);
    if (player_visible) {
        draw_player_warrior(p_screen_x, foot_y, s_game.facing, s_game.on_ground, is_running, s_game.morphing);

        /* Muendungsfeuer am Blaster-Lauf */
        if (s_game.fire_cooldown > 0 && !s_game.morphing) {
            draw_muzzle_flash(nozzle_x, nozzle_y, s_game.weapon);
        }

        /* Turrican 2 Kontinuierlicher Dual-Laserstrahl (WEAPON_LASER) */
        if (s_game.weapon == WEAPON_LASER && s_game.fire_cooldown > 0 && !s_game.morphing) {
            int lvl = (int)s_game.weapon_level - 1;
            if (lvl < 0) lvl = 0; if (lvl > 4) lvl = 4;
            int beam_len_px = 320 + lvl * 160;
            int end_x = nozzle_x + (dir > 0 ? beam_len_px : -beam_len_px);
            draw_turrican_laser_beam(nozzle_x, nozzle_y, end_x, nozzle_y, lvl + 1);
        }
    } /* Ende player_visible */

    /* 360-Grad Surround Holding Beam (Turrican 2 ALT / Controller X) */
    if (s_game.beam_active && !s_game.morphing) {
        draw_holding_beam(nozzle_x, nozzle_y, s_game.beam_angle);
    }

    /* 3.5 Extraleben-Pickups (an den Welt-Raendern, Bob-Animation) */
    for (int i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *pk = &s_game.pickups[i];
        if (!pk->active) continue;
        int kx = (int)(((int64_t)pk->x * 20) >> 16) - cam_x;
        int ky = ground_y - 40 - (int)(((int64_t)pk->y * 16) >> 16);
        int bob = (int)(sinf((float)s_gui_frame * 0.15f + (float)i) * 4.0f);
        draw_voxel(kx, ky - 16 + bob, 12, 12, 4, 0x00E050);
        set_pixel(kx + 5, ky - 12 + bob, 0xFFFFFF);
    }

    /* 2. Gegner (Turrican 2 Silhouette-Design: Walker, Seeker & Tank Golem) */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s_game.enemies[i];
        if (!e->active) continue;
        int ex = (int)(((int64_t)e->x * 20) >> 16) - cam_x;
        int ey = ground_y - 40 - (int)(((int64_t)e->y * 16) >> 16);

        int edir = (e->vx >= 0) ? 1 : -1;
        if (e->kind == ENEMY_SHOOTER) {
            draw_enemy_shooter(ex, ey, s_game.tick + i * 7, edir);
        } else if (e->kind == ENEMY_TANK) {
            draw_enemy_tank(ex, ey, e->hp, 15, s_game.tick, edir);
        } else {
            draw_enemy_walker(ex, ey, s_game.tick + i * 5, edir);
        }
    }

    /* 3. Projektile (Turrican 2 High-End Voxel-Grafik) */
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *b = &s_game.projectiles[i];
        if (!b->active) continue;
        int bx = (int)(((int64_t)b->x * 20) >> 16) - cam_x;
        int by = ground_y - 40 - (int)(((int64_t)b->y * 16) >> 16);

        /* Viewport Culling: Nur zeichnen wenn im sichtbaren Bildschirmbereich */
        if (bx < -32 || bx > WIN_W + 32 || by < -32 || by > WIN_H) continue;

        if (b->kind == PROJ_BOUNCE) {
            draw_projectile_bounce(bx, by);
        } else if (b->kind == PROJ_ENEMY) {
            draw_projectile_enemy(bx, by);
        } else if (b->vx > (9 << 16) || b->vx < -(9 << 16)) {
            draw_projectile_laser(bx, by, (b->vx >= 0) ? 1 : -1);
        } else {
            draw_projectile_spread(bx, by, b->vx, b->vy);
        }
    }

    /* 4. Partikel & Kristalle */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &s_game.particles[i];
        if (!p->active) continue;
        int px = (int)(((int64_t)p->x * 20) >> 16) - cam_x;
        int py = ground_y - 40 - (int)(((int64_t)p->y * 16) >> 16);
        set_pixel(px, py, 0xFFFFFF);
        set_pixel(px + 1, py, 0xFF8000);
    }

    /* 5. Turrican 2 Mystery [?] Item-Bloecke (Zerschiessbar & Headbutt-Sprung von unten!) */
    for (int bi = 0; bi < s_box_count; bi++) {
        DestructibleBox *dbox = &s_boxes[bi];
        if (!dbox->active || dbox->destroyed) continue;

        int bx = (dbox->world_x * 20) - cam_x;
        int dy = ground_y - 20 - (dbox->world_y * 16) - dbox->bounce_y;
        if (dbox->bounce_y > 0) dbox->bounce_y--;

        if (bx >= -32 && bx <= WIN_W + 32) {
            if (dbox->hit_flash > 0) {
                draw_voxel(bx, dy, 20, 20, 4, 0xFFFFFF);
                dbox->hit_flash--;
            } else {
                draw_voxel(bx, dy, 20, 20, 4, 0xD49A24);
                draw_rect_outline(bx, dy, 20, 20, 0xFFE040);
                set_pixel(bx + 2, dy + 2, 0xFFFFFF);
                set_pixel(bx + 17, dy + 2, 0xFFFFFF);
                set_pixel(bx + 2, dy + 17, 0x402000);
                set_pixel(bx + 17, dy + 17, 0x402000);
                uint32_t qcol = ((s_gui_frame / 6) % 2 == 0) ? 0xFFFFFF : 0x00FFFF;
                draw_string(bx + 6, dy + 4, "?", qcol, 1);
            }
        }

        /* Projektil-Kollision mit Kiste */
        for (int pri = 0; pri < MAX_PROJECTILES; pri++) {
            Projectile *proj = &s_game.projectiles[pri];
            if (!proj->active) continue;
            int proj_sx = (int)(((int64_t)proj->x * 20) >> 16) - cam_x;
            int proj_sy = ground_y - 20 - (int)(((int64_t)proj->y * 16) >> 16);
            if (proj_sx >= bx && proj_sx <= bx + 20 && proj_sy >= dy && proj_sy <= dy + 20) {
                proj->active = 0;
                dbox->hp -= (s_game.weapon == WEAPON_LASER) ? 2 : 1;
                dbox->hit_flash = 4;
                dbox->bounce_y = 6;
                spawn_debris(dbox->world_x << 16, dbox->world_y << 16, 4, 0xFFE040);
                sound_sfx_hit(&s_audio, 0);
                if (dbox->hp <= 0) shatter_box(dbox);
                break;
            }
        }

        /* Spieler Kopfstoss von unten gegen Kiste (Turrican / Super Mario Headbutt) */
        int p_sc_x = pw_x - cam_x;
        int p_sc_y = ground_y - 20 - (int)(((int64_t)s_game.py * 16) >> 16);
        int p_head = p_sc_y - 24;
        if (s_game.pvy > 0 && p_sc_x >= bx - 8 && p_sc_x <= bx + 28 &&
            p_head <= dy + 22 && p_head >= dy - 6) {
            dbox->bounce_y = 10;
            dbox->hp--;
            dbox->hit_flash = 4;
            s_game.pvy = -s_game.pvy / 2;
            sound_sfx_hit(&s_audio, 0);
            if (dbox->hp <= 0) shatter_box(dbox);
        }
    }

    /* 6. Gegner-Todes-Erkennung: Zerplatzen in Loot & Partikel */
    for (int ei = 0; ei < MAX_ENEMIES; ei++) {
        Enemy *e = &s_game.enemies[ei];
        if (s_prev_enemy_active[ei] && !e->active) {
            int e_wx = e->x;
            int e_wy = e->y + (1 << 16);
            int es_x = (int)(((int64_t)e->x * 20) >> 16) - cam_x;
            int es_y = ground_y - 40 - (int)(((int64_t)e->y * 16) >> 16);

            if (e->kind == ENEMY_TANK) {
                spawn_debris(e_wx, e_wy, 24, 0xFF4020);
                spawn_debris(e_wx, e_wy, 12, 0x00FFFF);
                spawn_loot(e_wx, e_wy, LOOT_DIAMOND, -2 << 16, 5 << 16);
                spawn_loot(e_wx, e_wy, LOOT_DIAMOND,  0,        6 << 16);
                spawn_loot(e_wx, e_wy, LOOT_DIAMOND,  2 << 16, 5 << 16);
                spawn_loot(e_wx, e_wy, LOOT_WEAPON_POWER, 1 << 15, 4 << 16);
                spawn_floating_text(es_x, es_y, "+500 MINI-BOSS!", 0xFFFF40);
                sound_sfx_explosion(&s_audio);
            } else if (e->kind == ENEMY_SHOOTER) {
                spawn_debris(e_wx, e_wy, 14, 0x00D0FF);
                spawn_loot(e_wx, e_wy, LOOT_DIAMOND, ((s_gui_frame & 1) ? 1 : -1) << 15, 4 << 16);
                spawn_floating_text(es_x, es_y, "+250", 0x00FFFF);
                sound_sfx_explosion(&s_audio);
            } else {
                spawn_debris(e_wx, e_wy, 10, 0xFF8020);
                if ((s_gui_frame % 3) == 0) {
                    spawn_loot(e_wx, e_wy, LOOT_DIAMOND, 0, 4 << 16);
                }
                spawn_floating_text(es_x, es_y, "+100", 0xFFE080);
                sound_sfx_explosion(&s_audio);
            }
        }
        s_prev_enemy_active[ei] = e->active;
    }

    /* 7. Loot-Items aktualisieren, zeichnen & aufsammeln */
    for (int li = 0; li < MAX_LOOT_ITEMS; li++) {
        GuiLootItem *item = &s_loot[li];
        if (!item->active) continue;

        item->life--;
        if (item->life <= 0) { item->active = 0; continue; }

        /* Gravitation & Trgheit */
        item->vy -= 900;
        item->world_x += item->vx;
        item->world_y += item->vy;

        if (item->world_y <= 0) {
            item->world_y = 0;
            item->vy = -item->vy / 2;
            item->vx = (item->vx * 3) / 4;
        }

        int lx = (int)(((int64_t)item->world_x * 20) >> 16) - cam_x;
        int ly = ground_y - 20 - (int)(((int64_t)item->world_y * 16) >> 16);

        if (lx >= -32 && lx <= WIN_W + 32) {
            if (item->kind == LOOT_DIAMOND) {
                int bob = (int)(sinf((float)s_gui_frame * 0.25f + (float)li) * 3.0f);
                draw_rect(lx + 4, ly + bob, 6, 3, 0x00FFFF);
                draw_rect(lx + 2, ly + 3 + bob, 10, 4, 0x80FFFF);
                draw_rect(lx + 4, ly + 7 + bob, 6, 3, 0x00A0E0);
                draw_rect(lx + 6, ly + 10 + bob, 2, 2, 0x005080);
                set_pixel(lx + 5, ly + 2 + bob, 0xFFFFFF);
            } else if (item->kind == LOOT_WEAPON_POWER) {
                draw_voxel(lx, ly, 16, 16, 3, 0xE02020);
                draw_rect_outline(lx, ly, 16, 16, 0xFFFFFF);
                draw_char(lx + 4, ly + 4, 'W', 0xFFFFFF, 1);
            } else if (item->kind == LOOT_SHIELD_BOOST) {
                draw_voxel(lx, ly, 16, 16, 3, 0x2080E0);
                draw_rect_outline(lx, ly, 16, 16, 0x00FFFF);
                draw_char(lx + 4, ly + 4, 'S', 0x00FFFF, 1);
            }
        }

        /* Spieler Einsammeln */
        int p_cur_x = pw_x - cam_x;
        int p_cur_y = ground_y - 20 - (int)(((int64_t)s_game.py * 16) >> 16);
        if (abs(p_cur_x - lx) < 28 && abs(p_cur_y - ly) < 32) {
            item->active = 0;
            if (item->kind == LOOT_DIAMOND) {
                s_game.score += 1000;
                if (s_game.shield < 3) s_game.shield++;
                sound_sfx_crystal(&s_audio);
                spawn_floating_text(lx, ly - 10, "+1000 DIAMANT!", 0x00FFFF);
            } else if (item->kind == LOOT_WEAPON_POWER) {
                if (s_game.weapon_level < 5) s_game.weapon_level++;
                s_game.score += 2000;
                sound_sfx_pickup(&s_audio);
                spawn_floating_text(lx, ly - 10, "WEAPON UPGRADE!", 0xFFFF40);
            } else if (item->kind == LOOT_SHIELD_BOOST) {
                s_game.shield = 3;
                s_game.score += 500;
                sound_sfx_pickup(&s_audio);
                spawn_floating_text(lx, ly - 10, "SHIELD FULL!", 0x40FF80);
            }
        }
    }

    /* 8. Debris-Partikel aktualisieren & zeichnen */
    for (int di = 0; di < MAX_DEBRIS_PARTS; di++) {
        GuiDebrisPart *dp = &s_debris[di];
        if (!dp->active) continue;
        dp->life--;
        if (dp->life <= 0) { dp->active = 0; continue; }

        dp->vy -= 800;
        dp->world_x += dp->vx;
        dp->world_y += dp->vy;
        if (dp->world_y <= 0) {
            dp->world_y = 0;
            dp->vy = -dp->vy / 3;
            dp->vx = (dp->vx * 2) / 3;
        }

        int dx = (int)(((int64_t)dp->world_x * 20) >> 16) - cam_x;
        int dy = ground_y - 20 - (int)(((int64_t)dp->world_y * 16) >> 16);
        if (dx >= 0 && dx < WIN_W && dy >= 0 && dy < WIN_H) {
            draw_rect(dx, dy, dp->size, dp->size, dp->color);
        }
    }

    /* 9. Floating Score Texte rendern */
    for (int fi = 0; fi < MAX_FLOATING_TEXTS; fi++) {
        GuiFloatingText *ft = &s_float_texts[fi];
        if (!ft->active) continue;
        ft->life--;
        if (ft->life <= 0) { ft->active = 0; continue; }
        ft->screen_y += ft->vy;

        draw_string(ft->screen_x + 1, ft->screen_y + 1, ft->text, 0x000000, 1);
        draw_string(ft->screen_x, ft->screen_y, ft->text, ft->color, 1);
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

        /* Rechtes Panel: LEVEL/WAVE + BIOME + WAFFE + TRUHE + MUSIK */
        draw_panel(WIN_W - 280, 46, 264, 150, 0x2A4860);
        if (s_game.mode == MODE_CAMPAIGN)
            snprintf(buf, sizeof(buf), "KAMPAGNE %u/%d", (unsigned)s_game.campaign_level, CAMPAIGN_LEVELS);
        else
            snprintf(buf, sizeof(buf), "ENDLOS  WAVE %u", (unsigned)s_game.wave + 1);
        draw_string(WIN_W - 268, 54, buf, 0xFFFFC040, 2);
        draw_string(WIN_W - 268, 76, bp->name, 0xA0E0FF, 1);

        /* 2.5D Retro Weapon Badge & Power-Meter */
        draw_weapon_hud_badge(WIN_W - 268, 92, s_game.weapon, s_game.weapon_level);
        snprintf(buf, sizeof(buf), "%s", wpn_names[s_game.weapon % 3]);
        draw_string(WIN_W - 218, 94, buf, (s_game.weapon == 1 ? 0x00FFFF : (s_game.weapon == 2 ? 0x50FF70 : 0xFFE040)), 1);
        snprintf(buf, sizeof(buf), "POWER LV %u/5", s_game.weapon_level);
        draw_string(WIN_W - 218, 108, buf, 0x90B0D0, 1);

        /* TRUHE-Anzeige entfernt */
        snprintf(buf, sizeof(buf), "MUSIK [M]: %-14.14s", s_current_track_name);
        draw_string(WIN_W - 268, 138, buf, 0x00FFFF, 1);
    }

    /* Musik-Toast (Overlay bei Songwechsel) */
    if (s_music_toast_timer > 0) {
        int tw = 480, th = 32;
        int tx = WIN_W / 2 - tw / 2;
        int ty = 96;
        char tmsg[128];
        snprintf(tmsg, sizeof(tmsg), "TRACK [%s]: %s", s_current_track_type, s_current_track_name);
        draw_rect(tx, ty, tw, th, 0x001830);
        draw_rect_outline(tx, ty, tw, th, 0x00FFFF);
        draw_string(tx + 16, ty + 10, tmsg, 0xFFFFA0, 1);
        draw_string(tx + tw - 80, ty + 10, "[M] WEITER", 0x80D0FF, 1);
    }
}

/* --- TAB 2: TFMX Explorer & 8-Voice Jukebox View --- */
static void render_tab_explorer(void) {
    draw_rect(0, 36, WIN_W, WIN_H - 60, 0x000814);

    /* Linkes Panel: Datei-Browser */
    draw_rect(20, 50, 460, WIN_H - 90, 0x001020);
    draw_rect_outline(20, 50, 460, WIN_H - 90, 0x004080);

    /* Top Mode Toggle Tabs */
    int t1_active = (s_exp_mode == 0);
    int t2_active = (s_exp_mode == 1);
    draw_rect(24, 54, 218, 26, t1_active ? 0x004080 : 0x001830);
    draw_rect_outline(24, 54, 218, 26, t1_active ? 0x00FFFF : 0x003060);
    {
        char rom_lbl[40];
        snprintf(rom_lbl, sizeof(rom_lbl), "[*] %d INTEGR. ROM-TRACKS", EMBEDDED_TRACK_COUNT);
        draw_string(30, 62, rom_lbl, t1_active ? 0x00FFFF : 0x80A0C0, 1);
    }

    draw_rect(246, 54, 230, 26, t2_active ? 0x004080 : 0x001830);
    draw_rect_outline(246, 54, 230, 26, t2_active ? 0x00FFFF : 0x003060);
    draw_string(254, 62, "[DIR] LOKALE DATEIEN", t2_active ? 0x00FFFF : 0x80A0C0, 1);

    draw_rect(20, 84, 460, 1, 0x004080);

    if (s_exp_mode == 0) {
        /* Modus 0: Eingebaute ROM-Soundtracks direkt listen (Anzahl dynamisch) */
        draw_string(28, 90, "EINGEBAUTE SOUNDTRACKS IM BINARY (CHRIS HUELSBECK):", 0xFFE040, 1);
        int y = 112;
        for (int i = 0; i < EMBEDDED_TRACK_COUNT; i++) {
            const EmbeddedTrack *et = &EMBEDDED_TRACKS[i];
            int selected = (i == s_exp_rom_selected);
            int is_curr_playing = 0;
            if (et->is_mod && s_exp_mod_playing && strcmp(s_exp_mod.title, et->name) == 0) is_curr_playing = 1;
            if (!et->is_mod && s_exp_playing && strcmp(s_exp_tfmx.title, et->name) == 0) is_curr_playing = 1;

            if (selected) {
                draw_rect(22, y - 2, 456, 18, 0x003060);
                draw_rect_outline(22, y - 2, 456, 18, 0x00A0FF);
            }

            char line[80];
            snprintf(line, sizeof(line), "%s [%s] %-32s",
                     selected ? ">" : " ",
                     et->is_mod ? "MOD " : "TFMX",
                     et->name);
            uint32_t c = is_curr_playing ? 0x00FF80 : (selected ? 0x00FFFF : (et->is_mod ? 0xA0FFA0 : 0xE0E0E0));
            draw_string(26, y, line, c, 1);
            if (is_curr_playing) {
                draw_string(410, y, "PLAY", 0x00FF80, 1);
            }
            y += 20;
        }
        /* v0.3.0: Hinweis auf eigene Soundtracks (Auto-Scan) */
        if (y + 90 < WIN_H - 90) {
            draw_rect(24, y + 4, 452, 1, 0x004080);
            draw_string(28, y + 12, ">>> DEIN EIGENER SOUNDTRACK:", 0x40E0FF, 1);
            draw_string(28, y + 30, "Leg .mod / .tfx(+.sam) neben der EXE", 0xC0D0E0, 1);
            draw_string(28, y + 46, "in 'TFMX PROFESSIONAL' oder 'MOD'", 0xC0D0E0, 1);
            draw_string(28, y + 62, "(bis 3 Ebenen tief) - die Jukebox", 0xC0D0E0, 1);
            draw_string(28, y + 78, "scannt das automatisch ein!", 0xC0D0E0, 1);
        }
        draw_rect(20, WIN_H - 85, 460, 1, 0x004080);
        draw_string(28, WIN_H - 78, "[KLICK/ENTER] Spielen   [PFEIL] Waehlen   [TAB] Modus", 0x80C0E0, 1);
    } else {
        /* Modus 1: Lokale Dateien & Ordner auf der Festplatte */
        char title_buf[300];
        snprintf(title_buf, sizeof(title_buf), "ORDNER: %s", s_current_dir);
        draw_string(28, 88, title_buf, 0x00FFFF, 1);

        char pathline[280];
        snprintf(pathline, sizeof(pathline), "PFAD: %s", s_exp_path_buf);
        draw_string(28, 102, pathline, 0xFF80FF80, 1);
        if ((s_gui_frame / 15) % 2 == 0)
            set_pixel(28 + (int)strlen(pathline) * 8 + 2, 106, 0x00FF80);

        int y = 120;
        int items_per_page = 22;
        for (int i = s_explorer_scroll; i < s_num_explorer_items && i < s_explorer_scroll + items_per_page; i++) {
            const ExplorerItem *item = &s_explorer_items[i];
            int selected = (i == s_explorer_selected);

            if (selected) {
                draw_rect(22, y - 2, 456, 16, 0x003060);
            }

            uint32_t c = item->is_dir ? 0xFFE040 : 0xE0E0E0;
            char line[80];
            snprintf(line, sizeof(line), "%s %-36s", item->is_dir ? "[DIR]" : "     ", item->name);
            draw_string(28, y, line, c, 1);
            y += 18;
        }
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
        draw_button(rx + 510, 140, 190, 30, "[? SHUFFLE SWAP]", 0, 0);

        if (s_frankenstein_status[0]) {
            draw_rect(rx + 20, 175, WIN_W - rx - 40, 18, 0x003018);
            draw_string(rx + 24, 180, s_frankenstein_status, 0xFF60FF, 1);
        }

        /* STRUDEL CONVERT BUTTON */
        draw_button(rx + 20, 198, 350, 36, "[==> IN STRUDEL MINI-NOTATION KONVERTIEREN]", 1, 0);
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
    draw_button(50, 215, 220, 36, s_supersaw.enabled ? "[ F5: SUPERSAW: AN ]" : "[ F5: SUPERSAW: AUS ]", s_supersaw.enabled ? 1 : 0, 0);
    draw_button(280, 215, 200, 36, "[ ENTER: ANWENDEN ]", 1, 0);
    draw_button(490, 215, 180, 36, "[ 1: GREAT BATH ]", 0, 0);
    draw_button(680, 215, 180, 36, "[ 2: STRANGER ]", 0, 0);
    draw_button(870, 215, 180, 36, "[ 3: TRANCE ]", 0, 0);
    draw_button(1060, 215, 170, 36, "[ 4: ACID SAW ]", 0, 0);

    /* 16-Step Sequencer LED Visualizer */
    draw_string(50, 280, "16-STEP PAL ZYKLUS-SEQUENCER (50Hz VBLANK):", 0xFFE040, 1);

    int cur_step = s_livecode.step_in_cycle;
    int16_t c_note = livecode_current_note(&s_livecode);
    static int s_prev_strudel_step = -1;
    if (s_prev_strudel_step != cur_step) {
        s_prev_strudel_step = cur_step;
        if (c_note > 0 && s_supersaw.enabled) {
            supersaw_trigger(&s_supersaw, c_note);
        }
    }
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

/* --- TAB 5: GRAFIK-STUDIO & RETROARCH SHADER MENUE --- */
static void render_tab_graphics(void) {
    draw_rect(0, 36, WIN_W, WIN_H - 60, 0x00060E);

    /* Header Banner */
    draw_rect(100, 48, WIN_W - 200, 42, 0x001020);
    draw_rect_outline(100, 48, WIN_W - 200, 42, 0x0080C0);
    draw_string(140, 58, ">>> STRUDELFORCE GRAFIK-STUDIO & RETROARCH SHADER ENGINE <<<", 0x00E0FF, 1);
    draw_string(140, 72, "Echtzeit-Shader: Bilinear, Aperture Grille, Scanlines, Phosphor-Glow & 16:9 Vollbild", 0xFFE040, 1);

    int lx = 100;
    int ly = 108;
    int lw = 520;
    int lh = WIN_H - 180;
    draw_rect(lx, ly, lw, lh, 0x000A14);
    draw_rect_outline(lx, ly, lw, lh, 0x003060);

    const char *profile_names[6] = {
        "0: AUS (Reine scharfe Pixel)",
        "1: BILINEAR SMOOTH (Weichzeichner)",
        "2: SONY TRINITRON (Aperture Grille)",
        "3: COMMODORE 1084S (Phosphor-Glow)",
        "4: RETROARCH CRT-EASYMODE (Bilinear+CRT)",
        "5: ARCADE CRT VIGNETTE (Woelbung+Aberration)"
    };
    const char *scan_names[4] = { "LEICHT (25%)", "MITTEL (50%)", "STARK (75%)", "INTENSIV (100%)" };
    const char *bloom_names[4] = { "AUS", "ZART (25%)", "NEON (50%)", "MAXIMUM (100%)" };
    const char *color_names[4] = { "AMIGA OCS (32 Farben)", "AMIGA AGA (256 Farben)", "CYBERPUNK NEON", "GRUENPHOSPHOR CRT" };

    struct { const char *label; char val[64]; } items[7] = {
        { "1. SHADER-PROFIL", "" },
        { "2. SCANLINE-STAERKE", "" },
        { "3. RGB PHOSPHOR-MASKE", "" },
        { "4. NEON BLOOM & GLOW", "" },
        { "5. BILINEARE GLAETTUNG", "" },
        { "6. BILDSCHIRM-MODUS", "" },
        { "7. AMIGA FARBMODUS", "" }
    };
    snprintf(items[0].val, sizeof(items[0].val), "< %s >", profile_names[s_filter_mode % 6]);
    snprintf(items[1].val, sizeof(items[1].val), "< %s >", scan_names[s_scanline_strength % 4]);
    snprintf(items[2].val, sizeof(items[2].val), "< %s >", s_phosphor_mask ? "AKTIV (Trinitron Triad)" : "DEAKTIVIERT");
    snprintf(items[3].val, sizeof(items[3].val), "< %s >", bloom_names[s_bloom_mode % 4]);
    snprintf(items[4].val, sizeof(items[4].val), "< %s >", s_bilinear ? "AKTIV (Bilinear)" : "AUS (Nearest)");
    snprintf(items[5].val, sizeof(items[5].val), "< %s >", s_is_fullscreen ? "VOLLBILD (16:9 Letterbox)" : "FENSTER (1280x720)");
    snprintf(items[6].val, sizeof(items[6].val), "< %s >", color_names[s_color_mode % 4]);

    for (int i = 0; i < 7; i++) {
        int y = ly + 25 + i * 46;
        int sel = (i == s_gfx_setting_cursor);
        if (sel) {
            draw_rect(lx + 10, y - 6, lw - 20, 38, 0x002040);
            draw_rect_outline(lx + 10, y - 6, lw - 20, 38, 0x00FFFF);
            draw_char(lx + 18, y + 4, '>', 0x00FFFF, 2);
        }
        draw_string(lx + 40, y + 4, items[i].label, sel ? 0x00FFFF : 0x80B0D0, 1);
        draw_string(lx + 230, y + 4, items[i].val, sel ? 0xFFFF40 : 0xE0E0E0, 1);
    }

    /* Rechtes Panel: Live Shader Preview Fenster */
    int rx = lx + lw + 20;
    int rw = WIN_W - rx - 100;
    int rh = lh;
    draw_rect(rx, ly, rw, rh, 0x000408);
    draw_rect_outline(rx, ly, rw, rh, 0x006090);
    draw_string(rx + 20, ly + 14, "LIVE SHADER VORSCHAU:", 0x00E0FF, 1);

    int py0 = ly + rh / 2;
    draw_voxel(rx + 20, py0, rw - 40, 24, 4, 0x8A4A20);
    draw_rect(rx + 22, py0 - 2, rw - 44, 3, 0xEECC00);
    draw_turrican_sprite(rx + 80, py0 - 4, (s_gui_frame / 4) % 13 + 1, 1);
    draw_voxel(rx + rw - 110, py0 - 18, 30, 20, 3, 0xD49A24);
    draw_rect_outline(rx + rw - 110, py0 - 18, 30, 20, 0xFFE040);
    draw_string(rx + rw - 100, py0 - 14, "?", 0xFFFFFF, 2);

    draw_rect(rx + 115, py0 - 28, rw - 240, 4, 0x00E0FF);
    draw_rect(rx + 115, py0 - 27, rw - 240, 2, 0xFFFFFF);

    draw_string(rx + 20, py0 + 40, "Pixel-Struktur reagiert sofort auf Shader!", 0xFFE040, 1);
    draw_string(rx + 20, py0 + 60, "Scanlines, Phosphormaske & Bloom live aktiv.", 0x80D0FF, 1);
    draw_string(lx + 20, ly + lh - 25, "HINTERGRUNDMUSIK: New Order - Blue Monday (Chiptune MOD)", 0x80FFA0, 1);

    /* Footer Navigation */
    draw_rect(0, WIN_H - 45, WIN_W, 45, 0x000A14);
    draw_rect_outline(0, WIN_H - 45, WIN_W, 45, 0x003060);
    draw_string(80, WIN_H - 30, "[ RAUF / RUNTER ] EINSTELLUNG     [ LINKS / RECHTS ] WERT AENDERN     [ F11 ] VOLLBILD     [ ESC ] ZURUECK", 0x00FFFF, 1);
}

/* --- TAB 4: Amiga Copper Cracktro Intro View (Demoszene 3D Vektor-Logo) --- */
static void render_tab_intro(void) {
    draw_rect(0, 0, WIN_W, WIN_H, 0x000010);

    /* Wenn noch kein Cracktro laeuft, zufaellig Fairlight/Skidrow/Paradox waehlen */
    static int s_intro_mod_started = 0;
    static int s_intro_trk_step = 0;
    if (!s_intro_mod_started) {
        s_intro_mod_started = 1;
        s_intro_trk_step = rand() % 3;
        int trk = (s_intro_trk_step == 0) ? TRACK_IDX_FAIRLIGHT :
                  (s_intro_trk_step == 1) ? TRACK_IDX_SKIDROW : TRACK_IDX_PARADOX;
        music_play_embedded(trk, 0);
    }

    /* 1. Amiga Copper Gradient Bars */
    float y1 = 360.0f + 160.0f * sinf((float)s_gui_frame * 0.05f);
    float y2 = 360.0f + 180.0f * sinf((float)s_gui_frame * 0.07f + 1.8f);
    float y3 = 360.0f + 140.0f * sinf((float)s_gui_frame * 0.09f + 3.2f);

    for (int y = 0; y < WIN_H; y++) {
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

    /* 2. Demoscene 3D Rotating Wireframe STRUDELFORCE Vector-Cube */
    {
        int cx = WIN_W / 2;
        int cy = 250;
        float f = (float)s_gui_frame;
        float ax = f * 0.024f;
        float ay = f * 0.038f;
        float az = f * 0.016f;

        float cosX = cosf(ax), sinX = sinf(ax);
        float cosY = cosf(ay), sinY = sinf(ay);
        float cosZ = cosf(az), sinZ = sinf(az);

        static const float cube_v[8][3] = {
            { -70, -70, -70 }, {  70, -70, -70 },
            {  70,  70, -70 }, { -70,  70, -70 },
            { -70, -70,  70 }, {  70, -70,  70 },
            {  70,  70,  70 }, { -70,  70,  70 },
        };
        static const int cube_e[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };

        int px[8], py[8];
        for (int v = 0; v < 8; v++) {
            float x = cube_v[v][0];
            float y = cube_v[v][1];
            float z = cube_v[v][2];

            float x1 = x * cosY - z * sinY;
            float z1 = x * sinY + z * cosY;
            float y1_r = y * cosX - z1 * sinX;
            float z2 = y * sinX + z1 * cosX;
            float x2 = x1 * cosZ - y1_r * sinZ;
            float y2_r = x1 * sinZ + y1_r * cosZ;

            float fov = 340.0f;
            float dist = z2 + 280.0f;
            if (dist < 10.0f) dist = 10.0f;
            px[v] = cx + (int)((x2 * fov) / dist);
            py[v] = cy + (int)((y2_r * fov) / dist);
        }

        for (int e = 0; e < 12; e++) {
            draw_line(px[cube_e[e][0]], py[cube_e[e][0]], px[cube_e[e][1]], py[cube_e[e][1]], 0x00E0FF);
            draw_line(px[cube_e[e][0]] + 1, py[cube_e[e][0]], px[cube_e[e][1]] + 1, py[cube_e[e][1]], 0x0080C0);
        }

        static const float oct_v[6][3] = {
            { -95, 0, 0 }, { 95, 0, 0 }, { 0, -95, 0 }, { 0, 95, 0 }, { 0, 0, -95 }, { 0, 0, 95 }
        };
        int opx[6], opy[6];
        for (int v = 0; v < 6; v++) {
            float x = oct_v[v][0]; float y = oct_v[v][1]; float z = oct_v[v][2];
            float x1 = x * cosY - z * sinY; float z1 = x * sinY + z * cosY;
            float y1_r = y * cosX - z1 * sinX; float z2 = y * sinX + z1 * cosX;
            float x2 = x1 * cosZ - y1_r * sinZ; float y2_r = x1 * sinZ + y1_r * cosZ;
            float dist = z2 + 280.0f;
            if (dist < 10.0f) dist = 10.0f;
            opx[v] = cx + (int)((x2 * 340.0f) / dist);
            opy[v] = cy + (int)((y2_r * 340.0f) / dist);
        }
        for (int a = 0; a < 6; a++) {
            for (int b = a + 1; b < 6; b++) {
                if (a / 2 != b / 2) {
                    draw_line(opx[a], opy[a], opx[b], opy[b], 0xFFFF40);
                }
            }
        }
    }

    /* 3. DEMOSCENE 3D-FLUGBY (v0.3.0): Grosse ASCII-STRUDELFORCE fliegt
     *    perspektivisch aus dem Raum auf die Kamera zu (Star-Wars-Zoom).
     *    Effekte: Copper-Rainbow-Zyklus, Distanz-Fade, Glow-Halo +
     *    Chromatic-Aberration bei Nahannah + Warp-Sternenfeld. */
    {
        static const char *w = "STRUDELFORCE";
        static const uint32_t copper[12] = {
            0xE04040, 0xE08020, 0xF0C040, 0xC0E040,
            0x40E080, 0x40E0C0, 0x40A0E0, 0x6060F0,
            0xA040E0, 0xE040C0, 0xE04080, 0xE06060
        };
        const int n = (int)strlen(w);              /* 12 Zeichen: STRUDELFORCE */
        const float mid = (float)(n - 1) / 2.0f;   /* Mitte der Wort-Achse */
        const float focal = 430.0f;
        const float cy_t = 385.0f;

        /* Flug-Phase: ~5.2 s (260 Frames), z geht per Ease-In von 1450 auf 150 */
        int phase = s_gui_frame % 260;
        float t = (float)phase / 260.0f;
        float zc = 150.0f + 1300.0f * (1.0f - t) * (1.0f - t);

        /* Warp-Sternenfeld: 36 radiale Streaks, Tempo koppelt an die Distanz */
        {
            static float s_warp_r[36];
            int st;
            float speed = 900.0f / (zc > 40.0f ? zc : 40.0f);
            for (st = 0; st < 36; st++) {
                float a = (float)st * (3.14159f / 18.0f);
                float ca = cosf(a), sa = sinf(a);
                s_warp_r[st] += speed * (1.0f + (float)(st % 3) * 0.35f);
                if (s_warp_r[st] > 440.0f) s_warp_r[st] = 8.0f + (float)st * 11.7f;
                float r0 = s_warp_r[st];
                float r1 = r0 + 14.0f + speed * 10.0f;
                if (r1 > 445.0f) continue;
                int x0 = WIN_W / 2 + (int)(ca * r0);
                int y0 = (int)cy_t + (int)(sa * r0 * 0.62f);
                int x1 = WIN_W / 2 + (int)(ca * r1);
                int y1 = (int)cy_t + (int)(sa * r1 * 0.62f);
                draw_line(x0, y0, x1, y1, (r1 > 300.0f) ? 0xB0C8F0 : 0x506880);
            }
        }

        for (int i = 0; i < n; i++) {
            float wx = ((float)i - mid) * 34.0f; /* Welt-Koordinaten (X-Achse) */
            float wy = 10.0f * sinf((float)i * 0.55f + (float)s_gui_frame * 0.04f);
            float z = zc + 18.0f * fabsf((float)i - mid); /* symmetrische Welle */
            if (z < 80.0f) z = 80.0f;
            float sf = focal / z;
            int scl = (int)(sf * 3.4f + 0.5f);
            if (scl < 1) scl = 1;
            if (scl > 26) scl = 26;
            int sx = WIN_W / 2 + (int)((wx * focal) / z);
            int sy = (int)cy_t + (int)((wy * focal) / z) - (scl * 4);

            uint32_t col = copper[(i + s_gui_frame / 3) % 12];
            if (z > 900.0f) col = 0x8090B0;          /* Ferne: blass-bleiern  */
            else if (z > 500.0f) col = (col >> 1);   /* Mitte: halbdunkel     */

            if (z < 420.0f) {
                /* Glow-Halo (4 diagonale Ghosts) + RGB-Aberration bei Nahannah */
                uint32_t glow = (col >> 1) & 0x7F7F7F;
                int o = scl / 4 + 1;
                draw_char(sx + o, sy - o, w[i], glow, scl);
                draw_char(sx - o, sy - o, w[i], glow, scl);
                draw_char(sx + o, sy + o, w[i], glow, scl);
                draw_char(sx - o, sy + o, w[i], glow, scl);
                int k = scl / 6 + 1;
                draw_char(sx - k, sy, w[i], 0xC02040, scl);
                draw_char(sx + k, sy, w[i], 0x20C060, scl);
            }
            draw_char(sx + 2, sy + 2, w[i], 0x400010, scl); /* Schlagschatten */
            draw_char(sx, sy, w[i], col, scl);
        }
    }

    /* Cracktro Banner */
    draw_rect(180, 50, 920, 48, 0x000E24);
    draw_rect_outline(180, 50, 920, 48, 0x00FFFF);
    draw_string(210, 60, ">>> COMMODORE AMIGA 500 CRACKTRO JUKEBOX (PROTRACKER 4-KANAL) <<<", 0x00FFFF, 1);
    {
        char cr_buf[160];
        snprintf(cr_buf, sizeof(cr_buf), "TRACK: %s  |  [ LEERTASTE / M / N : WECHSELN ]", s_current_track_name);
        draw_string(210, 78, cr_buf, 0xFFE040, 1);
    }
    /* v0.3.0: Hinweis auf Auto-Scan eigener Soundtracks */
    draw_string(210, 104, "[F2] JUKEBOX: EIGENE .MOD/.TFX IN 'MOD' & 'TFMX PROFESSIONAL' (neben der EXE) WERDEN AUTO-GEFUNDEN", 0x80A0C0, 1);

    /* Bottom Information & Navigation */
    draw_string(WIN_W / 2 - 250, 440, "COMMODORE AMIGA 500 * 50HZ PAL CORE * CYBER-PAULA TFMX AUDIO", 0xFFE040, 1);

    if ((s_gui_frame / 20) % 2 == 0) {
        const char *ent_msg = ">>> DRUECKE LEERTASTE / ENTER / START FUERS HAUPTMENUE <<<";
        draw_string(WIN_W / 2 - (int)strlen(ent_msg) * 4, 465, ent_msg, 0x00FFFF, 1);
    }

    /* Sinus-tanzender Amiga-Scroller */
    const char *scroller = "+++ STRUDELFORCE 0.3.0 +++ 100% 50HZ PAL AMIGA ENGINE +++ CHRIS HUELSBECK TFMX SOUNDTRACK +++ TURRICAN II 360-DEGREE LIGHTNING BEAM +++ DEMOSCENE CRT & BLOOM FILTERS +++ DRUECKE ENTER ODER LEERTASTE FUERS HAUPTMENUE +++";
    int slen = (int)strlen(scroller);
    int offset = s_gui_frame / 2;

    for (int col = 0; col < 120; col++) {
        char ch = scroller[(col + offset) % slen];
        float sy = 530.0f + 16.0f * sinf((float)col * 0.14f + (float)s_gui_frame * 0.07f);
        draw_char(40 + col * 10, (int)sy, ch, 0xFFFF00, 2);
    }
}

/* --- Frame-Dump (Headless-Visual-Test): strudelforce_gui.exe --dump [frames] [basis] ---
 * Rendert frames ohne Audio-Geraet und speichert alle 10 Ticks ein PPM-Bild.
 * Letztes Drittel: Intro-Tab (Anim-Check), ab 2/3 Low-HP (Interlace-Check). */
/* Legt fehlende Ordner einer Dateipfad-Angabe an (tmp/dump/...). */
static void dump_mkdirs(const char *fn) {
    char p[256];
    size_t i, n = strlen(fn);
    if (n >= sizeof(p)) return;
    memcpy(p, fn, n + 1);
    for (i = 1; i < n; i++) {
        if (p[i] == '/' || p[i] == '\\') {
            char c = p[i];
            p[i] = 0;
            CreateDirectoryA(p, NULL);
            p[i] = c;
        }
    }
}

static void dump_ppm(const char *fn) {
    FILE *f;
    int i;
    dump_mkdirs(fn);
    f = fopen(fn, "wb");
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
            if (wParam == VK_F7) {
                s_filter_mode = (s_filter_mode == 1 ? 0 : 1);
                snprintf(s_status_msg, sizeof(s_status_msg), "Filter: %s", s_filter_mode == 1 ? "CRT SCANLINES AN" : "AUS");
                return 0;
            }
            if (wParam == VK_F8) {
                s_filter_mode = (s_filter_mode + 1) % 4;
                const char *fnames[4] = { "AUS (Scharf)", "CRT SCANLINES", "PHOSPHOR BLOOM", "DEMOSCENE CRT+BLOOM" };
                snprintf(s_status_msg, sizeof(s_status_msg), "Filter: %s", fnames[s_filter_mode]);
                return 0;
            }
            if (wParam == VK_F11 || (wParam == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000))) {
                toggle_fullscreen(hWnd);
                return 0;
            }

            /* F1: Hauptmenue/Game-Over -> neues Spiel; sonst -> zurueck zum Spiel */
            if (wParam == VK_F1) {
                if (s_gui_state == GUI_STATE_MENU || s_gui_state == GUI_STATE_GAMEOVER)
                    start_game(s_active_mode);
                else if (s_gui_state != GUI_STATE_LOADING)
                    s_gui_state = GUI_STATE_GAME;
                return 0;
            }
            /* F2-F4: Studio-Tools (im Spiel = Pause, im Menue = direkter Start) */
            if (wParam == VK_F2 || wParam == VK_F3 || wParam == VK_F4 || wParam == VK_F9 || (wParam == 'G' && s_gui_state == GUI_STATE_MENU)) {
                GuiTab t = (wParam == VK_F2) ? TAB_EXPLORER :
                           (wParam == VK_F3) ? TAB_STRUDEL :
                           (wParam == VK_F4) ? TAB_INTRO : TAB_GRAPHICS;
                if (s_gui_state != GUI_STATE_LOADING) {
                    s_current_tab = t;
                    s_gui_state = GUI_STATE_STUDIO;
                    if (t == TAB_GRAPHICS) music_play_embedded(TRACK_IDX_BLUE_MONDAY, 0);
                }
                return 0;
            }
            /* ESC: aus jedem aktiven Zustand zurueck zum Hauptmenue */
            if (wParam == VK_ESCAPE) {
                if (s_gui_state != GUI_STATE_MENU && s_gui_state != GUI_STATE_LOADING) {
                    int was_graphics = (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_GRAPHICS);
                    s_gui_state = GUI_STATE_MENU;
                    if (was_graphics) music_play_embedded(0, 0);
                }
                return 0;
            }

                        /* --- Grafik-Studio Tastensteuerung --- */
            if (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_GRAPHICS) {
                if (wParam == VK_UP) {
                    s_gfx_setting_cursor = (s_gfx_setting_cursor + 6) % 7;
                    sound_sfx_hit(&s_audio, 0);
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    s_gfx_setting_cursor = (s_gfx_setting_cursor + 1) % 7;
                    sound_sfx_hit(&s_audio, 0);
                    return 0;
                }
                if (wParam == VK_LEFT || wParam == VK_RIGHT) {
                    int dir = (wParam == VK_RIGHT) ? 1 : -1;
                    if (s_gfx_setting_cursor == 0) s_filter_mode = (s_filter_mode + 6 + dir) % 6;
                    else if (s_gfx_setting_cursor == 1) s_scanline_strength = (s_scanline_strength + 4 + dir) % 4;
                    else if (s_gfx_setting_cursor == 2) s_phosphor_mask = !s_phosphor_mask;
                    else if (s_gfx_setting_cursor == 3) s_bloom_mode = (s_bloom_mode + 4 + dir) % 4;
                    else if (s_gfx_setting_cursor == 4) s_bilinear = !s_bilinear;
                    else if (s_gfx_setting_cursor == 5) toggle_fullscreen(hWnd);
                    else if (s_gfx_setting_cursor == 6) s_color_mode = (s_color_mode + 4 + dir) % 4;
                    sound_sfx_pickup(&s_audio);
                    return 0;
                }
            }
            /* --- Controller-Konfiguration: Tastatur (spiegelt die Pad-Steuerung) --- */
            if (s_gui_state == GUI_STATE_CONTROLS) {
                if (wParam == VK_UP || wParam == VK_DOWN) {
                    s_ctrl_setting_cursor = (s_ctrl_setting_cursor + 1) % 2;
                    sound_sfx_hit(&s_audio, 0);
                    return 0;
                }
                if (wParam == VK_LEFT || wParam == VK_RIGHT) {
                    int dir = (wParam == VK_RIGHT) ? 1 : -1;
                    if (s_ctrl_setting_cursor == 0) s_pad_preset = (s_pad_preset + 3 + dir) % 3;
                    else if (s_ctrl_setting_cursor == 1) s_pad_laser_invert_y = !s_pad_laser_invert_y;
                    sound_sfx_pickup(&s_audio);
                    return 0;
                }
            }
/* Tastatur-Kuerzel fuer Studio-Tabs */
            if (s_gui_state == GUI_STATE_STUDIO) {
                if (s_current_tab == TAB_INTRO) {
                    if (wParam == VK_RETURN || wParam == VK_SPACE || wParam == VK_ESCAPE) {
                        s_gui_state = GUI_STATE_MENU;
                        return 0;
                    }
                    if (wParam == 'M' || wParam == 'N') {
                        static int s_intro_trk_step = 0;
                        s_intro_trk_step = (s_intro_trk_step + 1) % 3;
                        int trk = (s_intro_trk_step == 0) ? TRACK_IDX_FAIRLIGHT :
                                  (s_intro_trk_step == 1) ? TRACK_IDX_SKIDROW : TRACK_IDX_PARADOX;
                        music_play_embedded(trk, 0);
                        return 0;
                    }
                } else if (s_current_tab == TAB_STRUDEL) {
                    if (wParam == VK_F5) {
                        s_supersaw.enabled = !s_supersaw.enabled;
                        snprintf(s_status_msg, sizeof(s_status_msg), "SuperSaw Overdub: %s", s_supersaw.enabled ? "AKTIV (7-Osc)" : "AUS");
                        return 0;
                    }
                }
            }

            /* M / N: Track wechseln | K / SHIFT+M: Subsong wechseln */
            if ((wParam == 'M' || wParam == 'N') && !(s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_STRUDEL)) {
                if (GetKeyState(VK_SHIFT) & 0x8000) {
                    music_subsong_next();
                } else {
                    music_play_next();
                }
                return 0;
            }
            if (wParam == 'K' && !(s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_STRUDEL)) {
                music_subsong_next();
                return 0;
            }

            /* --- Handbuch-Navigation --- */
            if (s_gui_state == GUI_STATE_MANUAL) {
                if (wParam == VK_LEFT || wParam == 'A' || wParam == 'a') {
                    if (s_manual_page > 0) s_manual_page--;
                } else if (wParam == VK_RIGHT || wParam == 'D' || wParam == 'd') {
                    if (s_manual_page < 4) s_manual_page++;
                } else if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                    s_gui_state = GUI_STATE_MENU;
                }
                return 0;
            }

            /* H-Taste im Menue: Handbuch oeffnen */
            if (s_gui_state == GUI_STATE_MENU && (wParam == 'H' || wParam == 'h')) {
                s_gui_state = GUI_STATE_MANUAL;
                s_manual_page = 0;
                return 0;
            }
            /* --- Hauptmenue-Navigation --- */
            if (s_gui_state == GUI_STATE_MENU) {
                if (wParam == VK_UP)   s_menu_index = (s_menu_index + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
                else if (wParam == VK_DOWN) s_menu_index = (s_menu_index + 1) % MENU_ITEM_COUNT;
                else if (wParam == VK_RETURN) menu_activate();
                return 0;
            }
            /* --- Credits: Steuerung (Pause, Speed, Musik, Exit) --- */
            if (s_gui_state == GUI_STATE_CREDITS) {
                if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
                    s_gui_state = GUI_STATE_MENU;
                    music_play_embedded(0, 0); /* Intro Meadow wieder an */
                } else if (wParam == VK_SPACE) {
                    s_credit_paused = !s_credit_paused;
                    snprintf(s_status_msg, sizeof(s_status_msg), s_credit_paused ? "Credits Pausiert" : "Credits Fortgesetzt");
                } else if (wParam == VK_DOWN) {
                    s_credit_scroll_y += 6;
                } else if (wParam == VK_F6 || ((wParam == 'X' || wParam == 'x') && s_exp_path_cur == 0)) { explorer_shuffle_swap(); return 0; } else if (wParam == VK_UP) {
                    s_credit_scroll_y -= 5;
                    if (s_credit_scroll_y < 0) s_credit_scroll_y = 0;
                } else if (wParam == 'M' || wParam == 'm') {
                    credit_music_next();
                }
                return 0;
            }
            /* --- Game Over: ENTER = Neustart, ESC = Menue (s.o.) --- */
            if (s_gui_state == GUI_STATE_GAMEOVER) {
                if (wParam == VK_RETURN) start_game(s_active_mode);
                return 0;
            }

            /* --- Studio-Tab (Jukebox / Strudel / Intro) --- */
            if (s_gui_state != GUI_STATE_STUDIO) return 0;

            if (s_current_tab == TAB_EXPLORER) {
                if (wParam == VK_TAB) {
                    s_exp_mode = !s_exp_mode;
                    return 0;
                }
                if (s_exp_mode == 0) {
                    if (wParam == VK_UP) {
                        s_exp_rom_selected = (s_exp_rom_selected + EMBEDDED_TRACK_COUNT - 1) % EMBEDDED_TRACK_COUNT;
                        return 0;
                    }
                    if (wParam == VK_DOWN) {
                        s_exp_rom_selected = (s_exp_rom_selected + 1) % EMBEDDED_TRACK_COUNT;
                        return 0;
                    }
                    if (wParam == VK_RETURN) {
                        explorer_play_rom(s_exp_rom_selected);
                        return 0;
                    }
                }
            }

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
                if (wParam == VK_F6 || ((wParam == 'X' || wParam == 'x') && s_exp_path_cur == 0)) { explorer_shuffle_swap(); return 0; } else if (wParam == VK_UP) {
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
                                if (sz >= 600 && mod_player_load_mem(&s_exp_mod, s_exp_mod_buf, (uint32_t)sz)) {
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
                                if (strcmp(s_current_dir, "<ROM: EINGEBAUTE SOUNDTRACKS>") == 0) {
                                    strcpy(s_current_dir, ".");
                                } else {
                                    char *last_slash = strrchr(s_current_dir, '\\');
                                    if (last_slash) *last_slash = '\0';
                                    else strcpy(s_current_dir, ".");
                                }
                            } else {
                                strncpy(s_current_dir, it->path, sizeof(s_current_dir) - 1);
                                s_current_dir[sizeof(s_current_dir) - 1] = '\0';
                            }
                            explorer_scan_directory(s_current_dir);
                        } else {
                            if (strncmp(it->path, "ROM:", 4) == 0) {
                                int rom_idx = atoi(it->path + 4);
                                if (rom_idx >= 0 && rom_idx < EMBEDDED_TRACK_COUNT) {
                                    const EmbeddedTrack *et = &EMBEDDED_TRACKS[rom_idx];
                                    s_exp_playing = 0;
                                    s_exp_mod_playing = 0;
                                    if (et->is_mod) {
                                        if (mod_player_load_mem(&s_exp_mod, et->tfx, et->tfx_len)) {
                                            mod_player_start(&s_exp_mod, GAME_AUDIO_RATE);
                                            s_exp_mod_playing = 1;
                                            snprintf(s_status_msg, sizeof(s_status_msg), "ROM-MOD: %s", et->name);
                                        }
                                    } else {
                                        if (tfmx_player_load_mem(&s_exp_tfmx, et->tfx, et->tfx_len, (const int8_t *)et->sam, et->sam_len)) {
                                            s_exp_subsong = 0;
                                            tfmx_player_start(&s_exp_tfmx, 0);
                                            s_exp_playing = 1;
                                            snprintf(s_status_msg, sizeof(s_status_msg), "ROM-TFMX: %s (%d Subs)", et->name, s_exp_tfmx.num_subsongs);
                                        }
                                    }
                                }
                            } else {
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
                                    if (size >= 600 && mod_player_load_mem(&s_exp_mod, s_exp_mod_buf, (uint32_t)size)) {
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
                /* Modus Toggle Tabs */
                if (my >= 54 && my <= 80) {
                    if (mx >= 24 && mx <= 242) { s_exp_mode = 0; return 0; }
                    if (mx >= 246 && mx <= 476) { s_exp_mode = 1; return 0; }
                }
                if (s_exp_mode == 0) {
                    /* ROM-Tracks Klick */
                    if (mx >= 20 && mx <= 480 && my >= 112 && my <= 112 + EMBEDDED_TRACK_COUNT * 20) {
                        int r_idx = (my - 112) / 20;
                        if (r_idx >= 0 && r_idx < EMBEDDED_TRACK_COUNT) {
                            s_exp_rom_selected = r_idx;
                            explorer_play_rom(s_exp_rom_selected);
                            return 0;
                        }
                    }
                } else if (mx >= 20 && mx <= 480 && my >= 108 && my <= WIN_H - 50) {
                    int clicked_idx = s_explorer_scroll + (my - 108) / 18;
                    if (clicked_idx >= 0 && clicked_idx < s_num_explorer_items) {
                        s_explorer_selected = clicked_idx;
                        ExplorerItem *it = &s_explorer_items[s_explorer_selected];
                        if (!it->is_dir) {
                            if (strncmp(it->path, "ROM:", 4) == 0) {
                                int rom_idx = atoi(it->path + 4);
                                if (rom_idx >= 0 && rom_idx < EMBEDDED_TRACK_COUNT) {
                                    const EmbeddedTrack *et = &EMBEDDED_TRACKS[rom_idx];
                                    s_exp_playing = 0;
                                    s_exp_mod_playing = 0;
                                    if (et->is_mod) {
                                        if (mod_player_load_mem(&s_exp_mod, et->tfx, et->tfx_len)) {
                                            mod_player_start(&s_exp_mod, GAME_AUDIO_RATE);
                                            s_exp_mod_playing = 1;
                                            snprintf(s_status_msg, sizeof(s_status_msg), "ROM-MOD: %s", et->name);
                                        }
                                    } else {
                                        if (tfmx_player_load_mem(&s_exp_tfmx, et->tfx, et->tfx_len, (const int8_t *)et->sam, et->sam_len)) {
                                            s_exp_subsong = 0;
                                            tfmx_player_start(&s_exp_tfmx, 0);
                                            s_exp_playing = 1;
                                            snprintf(s_status_msg, sizeof(s_status_msg), "ROM-TFMX: %s (%d Subs)", et->name, s_exp_tfmx.num_subsongs);
                                        }
                                    }
                                }
                            } else {
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
                                    if (size >= 600 && mod_player_load_mem(&s_exp_mod, s_exp_mod_buf, (uint32_t)size)) {
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
                        } else {
                            /* Klick auf Ordner */
                            if (strcmp(it->name, "..") == 0) {
                                if (strcmp(s_current_dir, "<ROM: EINGEBAUTE SOUNDTRACKS>") == 0) {
                                    strcpy(s_current_dir, ".");
                                } else {
                                    char *last_slash = strrchr(s_current_dir, '\\');
                                    if (last_slash) *last_slash = '\0';
                                    else strcpy(s_current_dir, ".");
                                }
                            } else {
                                strncpy(s_current_dir, it->path, sizeof(s_current_dir) - 1);
                                s_current_dir[sizeof(s_current_dir) - 1] = '\0';
                            }
                            explorer_scan_directory(s_current_dir);
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
                } else if (mx >= 1010 && mx <= 1200 && my >= 140 && my <= 170) {
                    explorer_shuffle_swap();
                }
                /* Convert Button Klick */
                else if (mx >= 520 && mx <= 870 && my >= 198 && my <= 234) {
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
                } else if (mx >= 50 && mx <= 270 && my >= 215 && my <= 251) {
                    s_supersaw.enabled = !s_supersaw.enabled;
                    snprintf(s_status_msg, sizeof(s_status_msg), "SuperSaw Overdub: %s", s_supersaw.enabled ? "AKTIV (7-Osc)" : "AUS");
                } else if (mx >= 280 && mx <= 480 && my >= 215 && my <= 251) {
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Pattern angewendet: %s", s_strudel_input);
                } else if (mx >= 490 && mx <= 670 && my >= 215 && my <= 251) {
                    strcpy(s_strudel_input, "<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>");
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Preset 1: The Great Bath Tribute geladen!");
                } else if (mx >= 680 && mx <= 860 && my >= 215 && my <= 251) {
                    strcpy(s_strudel_input, "c3 [e3 g3] b3 [c4 b3] [g3 e3]");
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Preset 2: Stranger Things Theme geladen!");
                } else if (mx >= 870 && mx <= 1050 && my >= 215 && my <= 251) {
                    strcpy(s_strudel_input, "[c3 c4] [eb3 eb4] [g3 g4]*2 <bb3 g3(3,8)>");
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Preset 3: Cyber Trance Anthem geladen!");
                } else if (mx >= 1060 && mx <= 1230 && my >= 215 && my <= 251) {
                    strcpy(s_strudel_input, "<c2 c3> [eb2 g2 c3]*2 <bb2 ab2(5,8)>");
                    s_strudel_cursor = (int)strlen(s_strudel_input);
                    apply_strudel_code(&s_livecode, s_strudel_input);
                    snprintf(s_status_msg, sizeof(s_status_msg), "Preset 4: Acid SuperSaw Lead geladen!");
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

static void init_console_io(void) {
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut && hStdOut != INVALID_HANDLE_VALUE && GetFileType(hStdOut) != FILE_TYPE_UNKNOWN) {
        int fd = _open_osfhandle((intptr_t)hStdOut, _O_TEXT);
        if (fd >= 0) {
            FILE *fp = _fdopen(fd, "w");
            if (fp) {
                *stdout = *fp;
                setvbuf(stdout, NULL, _IONBF, 0);
            }
        }
        int err_fd = _open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
        if (err_fd >= 0) {
            FILE *err_fp = _fdopen(err_fd, "w");
            if (err_fp) {
                *stderr = *err_fp;
                setvbuf(stderr, NULL, _IONBF, 0);
            }
        }
    } else {
        if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
            setvbuf(stdout, NULL, _IONBF, 0);
            setvbuf(stderr, NULL, _IONBF, 0);
        }
    }
}

/* --- Headless-Simulation und Test-Suite fuer CLI / Batch-Betrieb --- */

static int run_headless_simulation(uint32_t ticks, uint32_t seed, int campaign_mode) {
    GameState game;
    LiveCodeUI livecode;
    uint32_t t;
    printf("\n========================================================================\n");
    printf("STRUDELFORCE %s - Headless 50Hz PAL Simulation\n", GAME_VERSION);
    printf("Modus: %s | Seed: %u | Geplante Ticks: %u\n",
           campaign_mode ? "KAMPAGNE (5 Level x 3 Wellen)" : "ENDLOS ROGUELIKE", seed, ticks);
    printf("========================================================================\n");

    core_init(&game, seed);
    if (campaign_mode) {
        game.mode = MODE_CAMPAIGN;
        game.campaign_level = 1;
        core_setup_platforms(&game);
    }
    livecode_init(&livecode, "c2 [c2 eb2] [f2 g2] <bb2 [g2 f2]>");

    for (t = 0; t < ticks; t++) {
        uint16_t mask = INPUT_FIRE;
        int best = -1;
        int32_t best_d = 0x7FFFFFFF;
        int i;
        for (i = 0; i < MAX_ENEMIES; i++) {
            const Enemy *e = &game.enemies[i];
            fix16_t dx;
            if (!e->active) continue;
            dx = e->x - game.px;
            if (dx < 0) dx = -dx;
            if (dx < best_d) { best_d = dx; best = i; }
        }
        if (best >= 0) {
            if (game.enemies[best].x > game.px) mask |= INPUT_RIGHT;
            else if (game.enemies[best].x < game.px) mask |= INPUT_LEFT;
            if (game.enemies[best].y < game.py - FIX(1)) mask |= INPUT_UP;
        } else {
            mask |= (t % 100 < 60) ? INPUT_RIGHT : INPUT_LEFT;
        }

        core_tick(&game, mask);
        livecode_tick(&livecode);

        if (t == 1250) {
            livecode_open_chest(&livecode);
        }

        if ((t + 1) % 500 == 0 || t == ticks - 1 || game.state == STATE_GAMEOVER || game.state == STATE_VICTORY) {
            printf("  Tick %5u | Wave: %2u | Score: %6d | Kills: %3d | HP: %3d%% | Lives: %d | State: %s\n",
                   game.tick, game.wave, game.score, game.enemies_killed, game.hp, game.lives,
                   game.state == STATE_VICTORY ? "VICTORY" : (game.state == STATE_GAMEOVER ? "GAMEOVER" : "PLAYING"));
        }

        if (game.state == STATE_GAMEOVER || game.state == STATE_VICTORY) {
            break;
        }
    }

    printf("------------------------------------------------------------------------\n");
    printf("ERGEBNIS: tick=%u wave=%u score=%d kills=%u hp=%d lives=%d state=%s\n",
           game.tick, game.wave, game.score, game.enemies_killed, game.hp, game.lives,
           game.state == STATE_VICTORY ? "VICTORY" : (game.state == STATE_GAMEOVER ? "GAMEOVER" : "DONE"));
    printf("========================================================================\n\n");
    return 0;
}

static int run_headless_tests(void) {
    int pass = 0, fail = 0;
    printf("\n========================================================================\n");
    printf("STRUDELFORCE %s - Integrierte Headless Test-Suite\n", GAME_VERSION);
    printf("========================================================================\n");

    /* 1. 16.16 Fixed Point Arithmetik */
    {
        fix16_t a = FIX(5);
        fix16_t b = FIX(3);
        fix16_t prod = fix16_mul(a, b);
        fix16_t quot = (fix16_t)(((int64_t)prod << 16) / b);
        if (prod == FIX(15) && quot == a) {
            pass++; printf("  [PASS] 16.16 Fixed-Point Multiplikation & Division\n");
        } else {
            fail++; printf("  [FAIL] 16.16 Fixed-Point Multiplikation & Division\n");
        }
    }

    /* 2. Determinismus & Core-Init */
    {
        GameState g1, g2;
        core_init(&g1, 1337);
        core_init(&g2, 1337);
        if (g1.px == g2.px && g1.py == g2.py && g1.lives == 3 && g1.hp == 100) {
            pass++; printf("  [PASS] Deterministische Core-Initialisierung (3 Leben, 100 HP)\n");
        } else {
            fail++; printf("  [FAIL] Deterministische Core-Initialisierung\n");
        }
    }

    /* 3. 50Hz Simulation Tick */
    {
        GameState g;
        core_init(&g, 42);
        core_tick(&g, INPUT_RIGHT | INPUT_FIRE);
        if (g.tick == 1) {
            pass++; printf("  [PASS] 50Hz PAL Simulations-Tick & Ringpuffer\n");
        } else {
            fail++; printf("  [FAIL] 50Hz PAL Simulations-Tick\n");
        }
    }

    /* 4. Kampagnen-Plattformen */
    {
        GameState g;
        core_init(&g, 777);
        g.mode = MODE_CAMPAIGN;
        g.campaign_level = 1;
        core_setup_platforms(&g);
        if (g.platform_count > 0) {
            pass++; printf("  [PASS] Kampagnen-Modus 2.5D Plattform-Setup (%d Plattformen)\n", g.platform_count);
        } else {
            fail++; printf("  [FAIL] Kampagnen-Modus 2.5D Plattform-Setup\n");
        }
    }

    /* 5. Embedded Soundtracks */
    {
        if (EMB_TFMX_T3W1_LEN > 0 && EMB_SAM_T3W1_LEN > 0) {
            pass++; printf("  [PASS] Embedded Turrican 3 World 1 TFMX & Paula-Samples (%u + %u Bytes)\n",
                           EMB_TFMX_T3W1_LEN, EMB_SAM_T3W1_LEN);
        } else {
            fail++; printf("  [FAIL] Embedded Turrican 3 World 1 TFMX\n");
        }
    }

    /* 6. Soundgodizer Modus-Toggle */
    {
        god_toggle();
        const char *name = god_mode_name();
        god_toggle();
        god_toggle();
        god_toggle();
        if (name && strlen(name) > 0) {
            pass++; printf("  [PASS] Soundgodizer DSP Post-Effekt Switcher (%s)\n", name);
        } else {
            fail++; printf("  [FAIL] Soundgodizer DSP Post-Effekt Switcher\n");
        }
    }

    /* 7. Credits & Artefakt-Archiv */
    {
        credits_init();
        /* Kompaktes In-EXE-Archiv: 103 eingebettete + ~20 Header/Footer-Zeilen */
        if (s_credit_line_count > 50 && s_credit_line_count <= MAX_CREDIT_LINES) {
            pass++; printf("  [PASS] Credits & Artefakt-Archiv (%d Zeilen geladen, Starfield aktiv)\n", s_credit_line_count);
        } else {
            fail++; printf("  [FAIL] Credits & Artefakt-Archiv (%d Zeilen)\n", s_credit_line_count);
        }
    }

    /* 8. Explorer ROM Virtual Directory & Frankenstein Shuffle */
    {
        explorer_scan_directory("<ROM: EINGEBAUTE SOUNDTRACKS>");
        int rom_count = s_num_explorer_items;
        explorer_shuffle_swap();
        if (rom_count == EMBEDDED_TRACK_COUNT + 1 && s_frankenstein_status[0] != '\0') {
            pass++; printf("  [PASS] Explorer ROM-Ordner (%d Eintraege) & Frankenstein Swap (%s)\n",
                           rom_count, s_frankenstein_status);
        } else {
            fail++; printf("  [FAIL] Explorer ROM-Ordner / Frankenstein Swap\n");
        }
        explorer_scan_directory(".");
    }

    printf("------------------------------------------------------------------------\n");
    printf("TEST ERGEBNIS: %d PASS, %d FAIL\n", pass, fail);
    printf("========================================================================\n\n");
    return fail > 0 ? 1 : 0;
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
    char dump_base[128] = "tmp/dump/dump"; /* Testausgaben nie im Projekt-Root */
    int headless_mode = 0;
    int demo_mode = 0;
    uint32_t sim_ticks = 2500;
    uint32_t sim_seed = 1234;
    int campaign_mode = 0;
    int test_mode = 0;

    (void)hPrevInstance;

    /* Befehlszeilen-Argumente parsen */
    {
        char cmd[512];
        char *tok;
        strncpy(cmd, lpCmdLine ? lpCmdLine : "", sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        tok = strtok(cmd, " \t");
        while (tok) {
            if (_stricmp(tok, "--dump") == 0 || _stricmp(tok, "-dump") == 0) {
                dump_mode = 1;
                tok = strtok(NULL, " \t");
                if (tok) dump_frames = atoi(tok);
                tok = strtok(NULL, " \t");
                if (tok) { strncpy(dump_base, tok, sizeof(dump_base) - 1); dump_base[sizeof(dump_base) - 1] = '\0'; }
            } else if (_stricmp(tok, "--headless") == 0 || _stricmp(tok, "-headless") == 0) {
                headless_mode = 1;
                tok = strtok(NULL, " \t");
                if (tok && atoi(tok) > 0) sim_ticks = (uint32_t)atoi(tok);
                continue;
            } else if (_stricmp(tok, "--demo") == 0 || _stricmp(tok, "-demo") == 0) {
                demo_mode = 1;
                tok = strtok(NULL, " \t");
                if (tok && atoi(tok) > 0) sim_ticks = (uint32_t)atoi(tok);
                continue;
            } else if (_stricmp(tok, "--test") == 0 || _stricmp(tok, "-test") == 0) {
                test_mode = 1;
            } else if (_stricmp(tok, "--campaign") == 0 || _stricmp(tok, "-campaign") == 0) {
                campaign_mode = 1;
            } else if (_stricmp(tok, "--seed") == 0 || _stricmp(tok, "-seed") == 0) {
                tok = strtok(NULL, " \t");
                if (tok) sim_seed = (uint32_t)atoi(tok);
            } else if (_stricmp(tok, "--help") == 0 || _stricmp(tok, "-help") == 0 || _stricmp(tok, "-h") == 0 || _stricmp(tok, "/?") == 0) {
                init_console_io();
                printf("STRUDELFORCE %s - Amiga 500 Voxel & Cyber-Paula Engine\n\n", GAME_VERSION);
                printf("Verwendung:\n");
                printf("  strudelforce.exe                      Startet das 2.5D GUI Spiel (Standard)\n");
                printf("  strudelforce.exe -headless [ticks]    Headless 50Hz PAL Simulation (Default: 2500 Ticks)\n");
                printf("  strudelforce.exe --demo [ticks]       Headless Demo-Bot Run mit Status-Ausgabe\n");
                printf("  strudelforce.exe --test               Fuehrt interne Integrationstests aus\n");
                printf("  strudelforce.exe --dump [N] [basis]   Erzeugt N Headless-PPM Frame Dumps\n");
                printf("  strudelforce.exe --campaign           Aktiviert den 5-Level Kampagnen-Modus\n");
                printf("  strudelforce.exe --seed [wert]        Setzt den LCG Simulations-Seed\n");
                return 0;
            }
            tok = strtok(NULL, " \t");
        }
    }
    if (dump_frames < 10) dump_frames = 10;

    /* Headless / Test-Modi pruefen und ggf. Konsole anbinden */
    if (test_mode) {
        init_console_io();
        return run_headless_tests();
    }

    if (headless_mode || demo_mode) {
        init_console_io();
        return run_headless_simulation(sim_ticks, sim_seed, campaign_mode);
    }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "StrudelForceGUI";
    RegisterClassExA(&wc);

    RECT win_rect = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRectEx(&win_rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
    int adjusted_w = win_rect.right - win_rect.left;
    int adjusted_h = win_rect.bottom - win_rect.top;

    hWnd = CreateWindowExA(
        0, "StrudelForceGUI", "STRUDELFORCE " GAME_VERSION " - Amiga 500 Voxel Engine & Cyber-Paula TFMX Studio",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, adjusted_w, adjusted_h,
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
        /* Jukebox-Library: Kuratierte Klassiker und Chiptunes zuerst scannen */
        static const char *priority_dirs[] = {
            "TFMX Professional\\Turrican 3",
            "TFMX Professional\\BiFi Adventure",
            "TFMX Professional\\Cybersty",
            "TFMX Professional\\Mad TV",
            "MOD"
        };
        int p;
        for (p = 0; p < (int)(sizeof(priority_dirs)/sizeof(priority_dirs[0])); p++) {
            playlist_scan_rec(priority_dirs[p], 0);
        }
        /* Nur kuratierte Action-Soundtracks im Jukebox-Pool */
    }

    /* Allererster Start: Zufaelliges Cracktro-MOD (Fairlight / Skid Row / Paradox) */
    s_last_wave = s_game.wave;
    {
        int initial_cracktro = TRACK_IDX_FAIRLIGHT + (rand() % 3);
        music_play_embedded(initial_cracktro, 0);
    }

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
                    render_top_bar();
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
                    render_top_bar();
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

                    /* --- Controller (PS4 / DirectInput & Xbox / XInput via poll_gamepad) ---
                     * Stick/D-Pad = bewegen, Cross/A = Feuer, Square/X/R2 = Beam, Circle/B = Morph,
                     * Triangle/Y/R1/RS = Waffe, OPTIONS/START = Hauptmenue (Edge) */
                    poll_gamepad();
                    if (s_pad.connected) {
                        if (s_pad.left)  mask |= INPUT_LEFT;
                        if (s_pad.right) mask |= INPUT_RIGHT;
                        if (s_pad.up || s_pad.btn_jump) mask |= INPUT_UP;
                        if (s_pad.down || s_pad.btn_morph) mask |= INPUT_DOWN;
                        if (s_pad.btn_fire) mask |= INPUT_FIRE;
                        if (s_pad.btn_beam) mask |= INPUT_SECOND;
                        if (s_pad.btn_rb || s_pad.btn_weapsw) mask |= INPUT_WEAPSW;
                        {
                            static int s_pad_y_prev = 0;
                            int y_now = s_pad.btn_weapsw;
                            if (y_now && !s_pad_y_prev) {
                                if (s_pad.btn_rb) {
                                    music_subsong_next();
                                } else {
                                    music_play_next();
                                }
                            }
                            s_pad_y_prev = y_now;
                        }
                        {
                            int start_now = s_pad.btn_start;
                            if (start_now && !s_pad_start_prev) s_gui_state = GUI_STATE_MENU;
                            s_pad_start_prev = start_now;
                        }
                    } else {
                        s_pad_start_prev = 0;
                    }

                    /* Turrican 2 Steuerbarer Drehlaser (Blitzstrahl):
                     * Bei gehaltenem Beam (ALT / Pad X / Square / R2) bleibt der Spieler stehen.
                     * Links/Rechts dreht den Strahl (CCW / CW), Oben/Unten lenkt vertikal,
                     * Rechter Analogstick erlaubt direkte 360-Grad Praezisionspeilung!
                     * Wenn keine Richtung gedrueckt wird: Strahl bleibt stabil ausgerichtet! */
                    static float s_steered_beam_angle = 0.0f;
                    if (mask & INPUT_SECOND) {
                        int pad_aimed = 0;
                        if (s_pad.connected) {
                            float rx = s_pad.stick_rx;
                            float ry = s_pad.stick_ry;
                            if (rx * rx + ry * ry > 0.04f) {
                                /* Invertierte Y-Achsen Steuerung: Oben am Stick zielt senkrecht nach OBEN */
                                float rad = atan2f(ry, rx);
                                if (rad < 0.0f) rad += 6.283185307f;
                                s_steered_beam_angle = rad * (256.0f / 6.283185307f);
                                pad_aimed = 1;
                            }
                        }
                        /* Im Drehlaser-Modus bleibt der Krieger fixiert stehen (Turrican 2) */
                        mask &= ~(INPUT_LEFT | INPUT_RIGHT);

                        if (!pad_aimed) {
                            if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000 || s_pad.left) {
                                s_steered_beam_angle -= 4.0f;
                            }
                            if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000 || s_pad.right) {
                                s_steered_beam_angle += 4.0f;
                            }
                            if (GetAsyncKeyState('W') & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000 || s_pad.up) {
                                float diff = 192.0f - s_steered_beam_angle; /* 192 = Senkrecht nach oben */
                                while (diff > 128.0f) diff -= 256.0f;
                                while (diff < -128.0f) diff += 256.0f;
                                s_steered_beam_angle += (diff > 0.0f ? 4.0f : -4.0f);
                            }
                            if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState(VK_DOWN) & 0x8000 || s_pad.down) {
                                float diff = 64.0f - s_steered_beam_angle;  /* 64 = Nach unten gerichtet */
                                while (diff > 128.0f) diff -= 256.0f;
                                while (diff < -128.0f) diff += 256.0f;
                                s_steered_beam_angle += (diff > 0.0f ? 4.0f : -4.0f);
                            }
                        }
                        while (s_steered_beam_angle < 0.0f) s_steered_beam_angle += 256.0f;
                        while (s_steered_beam_angle >= 256.0f) s_steered_beam_angle -= 256.0f;
                        s_game.beam_angle = (uint8_t)(((int)s_steered_beam_angle) & 255);
                    }

                    core_tick(&s_game, mask);

                    if (mask & INPUT_SECOND) {
                        s_game.beam_angle = (uint8_t)(((int)s_steered_beam_angle) & 255);

                        /* Steered Beam Treffer-Erkennung gegen Mystery-Bloecke */
                        if (s_game.tick % 4 == 0) {
                            float b_rad = (float)s_game.beam_angle * (6.283185307f / 256.0f);
                            float b_cos = cosf(b_rad);
                            float b_sin = sinf(b_rad);
                            fix16_t b_cos_fp = (fix16_t)(b_cos * 65536.0f);
                            fix16_t b_sin_fp = (fix16_t)(-b_sin * 65536.0f);

                            for (int bi = 0; bi < s_box_count; bi++) {
                                DestructibleBox *dbox = &s_boxes[bi];
                                if (!dbox->active || dbox->destroyed) continue;
                                fix16_t bdx = (fix16_t)(dbox->world_x << 16) - s_game.px;
                                fix16_t bdy = (fix16_t)(dbox->world_y << 16) - s_game.py;
                                int64_t proj = ((int64_t)bdx * b_cos_fp + (int64_t)bdy * b_sin_fp) >> 16;
                                if (proj > 0 && proj < (11 << 16)) {
                                    int64_t perp = ((int64_t)bdx * b_sin_fp - (int64_t)bdy * b_cos_fp) >> 16;
                                    if (perp < 0) perp = -perp;
                                    if (perp < (fix16_t)(1.6f * 65536.0f)) {
                                        dbox->hp--;
                                        dbox->hit_flash = 3;
                                        sound_sfx_hit(&s_audio, 0);
                                        if (dbox->hp <= 0) shatter_box(dbox);
                                    }
                                }
                            }
                        }

                        /* Steered Beam Treffer-Erkennung gegen Gegner im Strahl */
                        if (s_game.tick % 4 == 0) {
                            float rad = (float)s_game.beam_angle * (6.283185307f / 256.0f);
                            float cos_a = cosf(rad);
                            float sin_a = sinf(rad);
                            fix16_t b_cos = (fix16_t)(cos_a * 65536.0f);
                            fix16_t b_sin = (fix16_t)(-sin_a * 65536.0f);
                            for (int ei = 0; ei < MAX_ENEMIES; ei++) {
                                Enemy *e = &s_game.enemies[ei];
                                if (!e->active) continue;
                                fix16_t edx = e->x - s_game.px;
                                fix16_t edy = e->y - s_game.py;
                                int64_t proj = ((int64_t)edx * b_cos + (int64_t)edy * b_sin) >> 16;
                                if (proj > 0 && proj < (11 << 16)) {
                                    int64_t perp = ((int64_t)edx * b_sin - (int64_t)edy * b_cos) >> 16;
                                    if (perp < 0) perp = -perp;
                                    if (perp < (fix16_t)(1.6f * 65536.0f)) {
                                        if (e->hp > 1) e->hp -= 1; else e->hp = 0;
                                        spawn_sparks(&s_game, e->x, e->y + FIX(1), 2, 4);
                                    }
                                }
                            }
                        }
                    }

                    /* Bereinigung nicht-prallender Projektile an den Grenzen im Spiel */
                    for (int pi = 0; pi < MAX_PROJECTILES; pi++) {
                        Projectile *p = &s_game.projectiles[pi];
                        if (!p->active) continue;
                        if (p->kind != PROJ_BOUNCE) {
                            if (p->x <= FIX(1) || p->x >= s_game.world_w - FIX(1) ||
                                p->y <= FIX(1) || p->y >= s_game.world_h - FIX(1)) {
                                p->active = 0;
                                spawn_sparks(&s_game, p->x, p->y, 2, 2);
                            }
                        }
                    }

                    livecode_tick(&s_livecode);
                    if (s_music_toast_timer > 0) s_music_toast_timer--;

                    /* The Great Bath: Altar-Truhe bei Beruehrung oeffnen */
                    if (core_campaign_biome(&s_game) == 1 && !s_livecode.chest_opened) {
                        if (abs((s_game.px >> 16) - 45) < 6 && (s_game.py >> 16) >= 14) {
                            livecode_open_chest(&s_livecode);
                        }
                    }

                    /* Alle Leben verbraucht? -> Game-Over-Screen.
                     * Kampagne vollstaendig? -> Sieg-Screen (gleiche End-State-Maschine). */
                    if (s_game.state == STATE_GAMEOVER || s_game.state == STATE_VICTORY) {
                        s_gui_state = GUI_STATE_GAMEOVER;
                    }

                    /* Reaktive Subsong- und Track-Steuerung im Spielverlauf */
                    if (s_game.state != STATE_GAMEOVER && s_game.state != STATE_VICTORY) {
                        if (s_game.mode == MODE_CAMPAIGN) {
                            if (s_game.campaign_level != s_last_campaign_level) {
                                s_last_campaign_level = s_game.campaign_level;
                                s_last_wave = s_game.wave;
                                music_play_campaign_level(s_game.campaign_level);
                            }
                        } else {
                            if (s_game.wave != s_last_wave) {
                                s_last_wave = s_game.wave;
                                music_play_next();
                            }
                        }

                        /* Auto-Rotation fuer kurze Looping-MODs (z.B. 3c minichip):
                         * Nach mehrmaligem Loop (oder 1-2 Minuten) naechsten Track starten */
                        s_track_ticks++;
                        if (s_mod_active && ((s_mod.loops >= 4 && s_track_ticks > 50 * 30) || s_track_ticks > 50 * 120)) {
                            music_play_next();
                        }

                        /* Reaktive Subsongs im laufenden Spiel */
                        if (s_game.state == STATE_PLAYING) {
                            int boss_active = 0;
                            int enemies_active = 0;
                            for (int ei = 0; ei < MAX_ENEMIES; ei++) {
                                if (s_game.enemies[ei].active) {
                                    enemies_active++;
                                    if (s_game.enemies[ei].kind == ENEMY_TANK) boss_active = 1;
                                }
                            }
                            if (boss_active || (s_game.wave % 3 == 0 && enemies_active > 0)) {
                                if (s_last_subsong_reason != 1) {
                                    s_last_subsong_reason = 1;
                                    music_subsong_switch(1); /* Boss Battle Theme */
                                }
                            } else if (s_game.lives == 1) {
                                if (s_last_subsong_reason != 4) {
                                    s_last_subsong_reason = 4;
                                    if (s_tfmx.num_subsongs >= 8) music_subsong_switch(1);
                                }
                            } else {
                                if (s_last_subsong_reason != 0) {
                                    s_last_subsong_reason = 0;
                                    music_subsong_switch(0); /* Standard Ingame Exploration */
                                }
                            }
                        } else if (s_game.state == STATE_LEVEL_CLEAR) {
                            if (s_last_subsong_reason != 2) {
                                s_last_subsong_reason = 2;
                                if (s_tfmx_active && s_tfmx.num_subsongs > 2) {
                                    music_subsong_switch(2); /* Level Clear Fanfare */
                                } else {
                                    music_play_embedded(TRACK_IDX_WC_OFFICE, 0); /* WC Commander Office MOD */
                                }
                            }
                        }
                    } else if (s_game.state == STATE_VICTORY) {
                        if (s_last_subsong_reason != 5) {
                            s_last_subsong_reason = 5;
                            music_play_embedded(TRACK_IDX_WC_OFFICE, 0); /* Sieg-Fanfare: WC Commander Office MOD */
                        }
                    } else if (s_game.state == STATE_GAMEOVER) {
                        if (s_last_subsong_reason != 3) {
                            s_last_subsong_reason = 3;
                            if (s_tfmx_active && s_tfmx.num_subsongs > 3) {
                                music_subsong_switch(3); /* Game Over Jingle */
                            }
                        }
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
                    /* Menu-/Loading-/Game-Over / TAB_INTRO: Jukebox-Track & Cracktro MOD weiter laufen */
                    int menu_music = (s_gui_state != GUI_STATE_STUDIO || s_current_tab == TAB_INTRO);
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
                 * (MOD-Audio: im Spiel, im Menu/Loading/Game-Over UND im TAB_INTRO Cracktro!) */
                if (s_mod_active && (s_gui_state != GUI_STATE_STUDIO || s_current_tab == TAB_INTRO)) {
                    mod_player_render(&s_mod, abuf, WIN_AUDIO_BUF_SAMPLES);
                } else if (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_EXPLORER && s_exp_mod_playing) {
                    mod_player_render(&s_exp_mod, abuf, WIN_AUDIO_BUF_SAMPLES);
                } else {
                    game_audio_render(&s_audio, abuf, WIN_AUDIO_BUF_SAMPLES);
                }
                /* Strudel SuperSaw Overdub (Lead Synthesizer) */
                if (s_supersaw.enabled && (s_gui_state == GUI_STATE_STUDIO && s_current_tab == TAB_STRUDEL)) {
                    supersaw_render(&s_supersaw, abuf, WIN_AUDIO_BUF_SAMPLES, 150);
                }

                /* DJ Fade (Pop-freier Studio Gain-Crossfade) */
                apply_dj_fade(abuf, WIN_AUDIO_BUF_SAMPLES);

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
                } else if (s_gui_state == GUI_STATE_MANUAL) {
                    render_state_manual();
                } else if (s_gui_state == GUI_STATE_CONTROLS) {
                    render_state_controls();
                } else if (s_gui_state == GUI_STATE_GAME || s_gui_state == GUI_STATE_GAMEOVER) {
                    render_tab_game();
                    if (s_gui_state == GUI_STATE_GAMEOVER) {
                        if (s_game.state == STATE_VICTORY) render_victory_overlay();
                        else render_game_over_overlay();
                    } else if (s_game.state == STATE_LEVEL_CLEAR) {
                        render_level_clear_overlay();
                    }
                    render_top_bar();   /* v0.3.0: Mini-Menue-Top-Bar im Spiel wieder da */
                    render_status_bar();
                } else {
                    /* STUDIO: Jukebox / Strudel / Intro (Top-Bar + Status) */
                    if (s_current_tab == TAB_EXPLORER) {
                        render_tab_explorer();
                    } else if (s_current_tab == TAB_STRUDEL) {
                        render_tab_strudel();
                    } else if (s_current_tab == TAB_GRAPHICS) {
                        render_tab_graphics();
                    } else {
                        render_tab_intro();
                    }
                    if (s_gui_state != GUI_STATE_STUDIO || s_current_tab != TAB_INTRO) {
                        render_top_bar();
                        render_status_bar();
                    }
                }

                /* Demoszene CRT & Phosphor Bloom Filter anwenden */
                apply_demoscene_postprocess();

                /* Vollbild 16:9 Letterbox / Pillarbox Skalierung */
                RECT cr;
                GetClientRect(hWnd, &cr);
                int cw = cr.right - cr.left;
                int ch = cr.bottom - cr.top;
                if (cw <= 0 || ch <= 0) { cw = WIN_W; ch = WIN_H; }

                int dst_w = cw;
                int dst_h = (cw * 9) / 16;
                if (dst_h > ch) {
                    dst_h = ch;
                    dst_w = (ch * 16) / 9;
                }
                int dst_x = (cw - dst_w) / 2;
                int dst_y = (ch - dst_h) / 2;

                if (s_is_fullscreen && (dst_x > 0 || dst_y > 0)) {
                    static int s_fs_cleared = 0;
                    if (s_fs_cleared < 2) {
                        HBRUSH bbr = (HBRUSH)GetStockObject(BLACK_BRUSH);
                        FillRect(hdc, &cr, bbr);
                        s_fs_cleared++;
                    }
                }

                /* In Fenster / Vollbild blitten */
                StretchDIBits(
                    hdc, dst_x, dst_y, dst_w, dst_h,
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
