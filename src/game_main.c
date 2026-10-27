/*
 * game_main.c - STRUDELFORCE Terminal-Frontend (spielbar, ohne externe Abhaengigkeiten)
 *
 * Build:  make game && ./strudelforce_game
 * Demo:   ./strudelforce_game --demo 2500 [--seed 1234]   (deterministisch, headless)
 *
 * Steuerung: WASD/Pfeile bewegen, W/Leertaste springen, Z feuern,
 *            X Surround-Beam halten, C Waffenwechsel, P Pause,
 *            ESC Live-Coding Console, T Schatztruhe, Q beenden
 *
 * Reines C99 (Cross-Platform: Windows + POSIX). Lizenz: MIT (Clean Room)
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "core_sim.h"
#include "game_audio.h"
#include "livecode_ui.h"
#include "tfmx_player.h"
#include "amiga_intro.h"
#include "win_audio.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
static void sleep_ms(int ms) {
    Sleep((DWORD)ms);
}
static void enable_windows_vt(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        dwMode |= 0x0004; /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
        SetConsoleMode(hOut, dwMode);
    }
}
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

#ifndef FIX
#define FIX(x) fix16_from_int(x)
#endif

#define GRID_W 72
#define GRID_H 30
#define WORLD_TOP_ROW 1
#define WORLD_BOT_ROW 26
#define GROUND_ROW    27

/* Display-8-Richtungen (y nach oben = +1) */
static const int DX8[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const int DY8[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

static int yrow(fix16_t y) {
    return WORLD_BOT_ROW - fix16_to_int(y);
}

static void make_bar(char *buf, int filled, int width) {
    int i;
    for (i = 0; i < width; i++) buf[i] = (i < filled) ? '#' : '-';
    buf[width] = 0;
}

static const char *weapon_name(int w) {
    switch (w) {
        case WEAPON_LASER:  return "LASER";
        case WEAPON_BOUNCE: return "BOUNCE";
        default:            return "SPREAD";
    }
}

static void render(const GameState *s, const LiveCodeUI *lc, const TfmxPlayer *tfmx, char *grid) {
    int row, col, i;
    char line[GRID_W + 8];

    for (row = 0; row < GRID_H; row++)
        memset(&grid[row * GRID_W], ' ', GRID_W);

    /* Bodenlinie */
    for (col = 0; col < GRID_W; col++)
        grid[GROUND_ROW * GRID_W + col] = '-';

    /* Partikel */
    for (i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &s->particles[i];
        int r, c;
        if (!p->active) continue;
        r = yrow(p->y); c = fix16_to_int(p->x);
        if (r >= WORLD_TOP_ROW && r <= WORLD_BOT_ROW && c >= 0 && c < GRID_W)
            grid[r * GRID_W + c] = '.';
    }

    /* Laser (letzter Tick) */
    if (s->laser_len > 0) {
        int len = fix16_to_int(s->laser_len);
        int half = (int)s->laser_half;
        int base = yrow(s->py + FIX(1));
        int pc = fix16_to_int(s->px);
        int c2, h;
        if (half > 2) half = 2; /* Display-Kappung */
        for (c2 = 1; c2 <= len; c2++) {
            int cc = pc + s->laser_dir * c2;
            if (cc < 0 || cc >= GRID_W) break;
            for (h = -half; h <= half; h++) {
                int rr = base - h;
                if (rr >= WORLD_TOP_ROW && rr <= WORLD_BOT_ROW)
                    grid[rr * GRID_W + cc] = '=';
            }
        }
    }

    /* Projektile */
    for (i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &s->projectiles[i];
        int r, c;
        if (!p->active) continue;
        r = yrow(p->y); c = fix16_to_int(p->x);
        if (r >= WORLD_TOP_ROW && r <= WORLD_BOT_ROW && c >= 0 && c < GRID_W) {
            grid[r * GRID_W + c] = (p->kind == PROJ_BOUNCE) ? 'O'
                                 : (p->kind == PROJ_ENEMY) ? 'o' : '*';
        }
    }

    /* Surround-Beam */
    if (s->beam_active) {
        int d8 = (s->beam_angle / 32) & 7;
        int pc = fix16_to_int(s->px);
        int pr = yrow(s->py + FIX(1));
        int k;
        for (k = 1; k <= 5; k++) {
            int c2 = pc + DX8[d8] * k;
            int r2 = pr - DY8[d8] * k;
            if (c2 >= 0 && c2 < GRID_W && r2 >= WORLD_TOP_ROW && r2 <= WORLD_BOT_ROW)
                grid[r2 * GRID_W + c2] = 'O';
        }
    }

    /* Gegner */
    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s->enemies[i];
        int r, c;
        if (!e->active) continue;
        r = yrow(e->y + FIX(1)); c = fix16_to_int(e->x);
        if (r >= WORLD_TOP_ROW && r <= WORLD_BOT_ROW && c >= 0 && c < GRID_W) {
            grid[r * GRID_W + c] = (e->kind == ENEMY_SHOOTER) ? 'S'
                                 : (e->kind == ENEMY_TANK) ? 'T' : 'M';
        }
    }

    /* Spieler */
    {
        int r = yrow(s->py + FIX(1));
        int c = fix16_to_int(s->px);
        if (r >= WORLD_TOP_ROW && r <= WORLD_BOT_ROW && c >= 0 && c < GRID_W)
            grid[r * GRID_W + c] = s->morphing ? 'v' : '@';
    }

    /* HUD Zeile 0 */
    {
        char bar1[16], bar2[16];
        int hp10 = (int)(((uint32_t)s->hp * 10u) / (s->max_hp ? s->max_hp : 1u));
        if (hp10 > 10) hp10 = 10;
        make_bar(bar1, hp10, 10);
        make_bar(bar2, (int)(s->shield / 10), 10);
        snprintf(line, sizeof(line),
                 " HP[%s] %u/%u  SH[%s] %u  LV %u  XP %u/%u  WAVE %u",
                 bar1, s->hp, s->max_hp, bar2, s->shield,
                 s->level, s->xp, 50u * (uint32_t)s->level, s->wave);
        strncpy(&grid[0], line, GRID_W);
    }
    /* HUD Zeile GRID_H-2 */
    snprintf(line, sizeof(line),
             " WPN %s L%u   SCORE %u   KILLS %u   BNC %u   BIOME: %s",
             weapon_name(s->weapon), s->weapon_level, s->score,
             s->enemies_killed, s->bounces_total,
             lc ? livecode_biome_name(lc->biome) : "Standard");
    strncpy(&grid[(GRID_H - 2) * GRID_W], line, GRID_W);
    /* HUD Zeile GRID_H-1 */
    if (tfmx && tfmx->playing) {
        snprintf(line, sizeof(line),
                 " [WASD] [Z Feuer] [C Wpn] [ESC LiveCode] | TFMX: %.*s",
                 26, tfmx->title[0] ? tfmx->title : "Amiga 8-Voice DMA");
    } else {
        snprintf(line, sizeof(line),
                 " [WASD] [Z Feuer] [X Beam] [C Wpn] [ESC LiveCode] [T Truhe] [Q quit]");
    }
    strncpy(&grid[(GRID_H - 1) * GRID_W], line, GRID_W);

    /* Overlays */
    if (s->state == STATE_WAVE_INTRO) {
        snprintf(line, sizeof(line), "  WAVE %u INCOMING...", s->wave + 1);
        strncpy(&grid[13 * GRID_W + 20], line, GRID_W - 20);
    }
    if (s->state == STATE_GAMEOVER) {
        snprintf(line, sizeof(line), "  GAME OVER  -  Q zum Verlassen  -  SCORE %u", s->score);
        strncpy(&grid[13 * GRID_W + 10], line, GRID_W - 10);
    }

    /* LiveCode UI Overlay */
    if (lc && lc->editor_open) {
        int r, cc;
        char lbuf[GRID_W + 1];
        for (r = 8; r <= 18; r++) {
            memset(&grid[r * GRID_W + 3], ' ', GRID_W - 6);
            grid[r * GRID_W + 3] = '|';
            grid[r * GRID_W + GRID_W - 4] = '|';
        }
        for (cc = 3; cc < GRID_W - 3; cc++) {
            grid[8 * GRID_W + cc] = '-';
            grid[18 * GRID_W + cc] = '-';
        }
        grid[8 * GRID_W + 3] = '+'; grid[8 * GRID_W + GRID_W - 4] = '+';
        grid[18 * GRID_W + 3] = '+'; grid[18 * GRID_W + GRID_W - 4] = '+';

        snprintf(lbuf, sizeof(lbuf), "+-- [LIVE-CODE CONSOLE] %s | Cyc %u Stp %u/16 --+",
                 livecode_biome_name(lc->biome), lc->cycle_num, lc->step_in_cycle);
        memcpy(&grid[8 * GRID_W + 4], lbuf, strlen(lbuf) < (size_t)(GRID_W - 8) ? strlen(lbuf) : (size_t)(GRID_W - 8));

        snprintf(lbuf, sizeof(lbuf), "Strudel: %.*s", GRID_W - 18, lc->buffer);
        memcpy(&grid[10 * GRID_W + 5], lbuf, strlen(lbuf));

        {
            int cur_pos = lc->cursor;
            if (cur_pos > GRID_W - 20) cur_pos = GRID_W - 20;
            snprintf(lbuf, sizeof(lbuf), "Cursor:  %*s^", cur_pos, "");
            memcpy(&grid[11 * GRID_W + 5], lbuf, strlen(lbuf));
        }

        snprintf(lbuf, sizeof(lbuf), "Note: [%d]  Pending: %s",
                 livecode_current_note(lc), lc->have_pending ? "SWAP AM ZYKLUS-RAND" : "kein");
        memcpy(&grid[13 * GRID_W + 5], lbuf, strlen(lbuf));

        snprintf(lbuf, sizeof(lbuf), "Truhe: %s (\"The Great Bath\")",
                 lc->chest_opened ? "GEOEFFNET (Tribute-Pattern)" : "GESCHLOSSEN (T zum Oeffnen)");
        memcpy(&grid[14 * GRID_W + 5], lbuf, strlen(lbuf));

        snprintf(lbuf, sizeof(lbuf), "Tasten: [ENTER Swap] [F2 Save] [F3 Load] [T Truhe] [ESC Zurueck]");
        memcpy(&grid[16 * GRID_W + 5], lbuf, strlen(lbuf));
    }
}

/* Demo-Bot: deterministischer Spieler für Headless-Runs */
static uint16_t bot_input(const GameState *s, uint32_t tick) {
    uint16_t m = INPUT_FIRE;
    int i;
    int best = -1;
    fix16_t best_d = 0;
    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s->enemies[i];
        fix16_t dx;
        if (!e->active) continue;
        dx = e->x - s->px; if (dx < 0) dx = -dx;
        if (best < 0 || dx < best_d) { best_d = dx; best = i; }
    }
    if (best >= 0) {
        fix16_t dx = s->enemies[best].x - s->px;
        if (dx > fix16_from_int(2)) m |= INPUT_RIGHT;
        else if (dx < -fix16_from_int(2)) m |= INPUT_LEFT;
    }
    if ((tick % 97) == 0 && s->on_ground) m |= INPUT_UP;
    if ((tick % 300) < 60) m |= INPUT_SECOND;
    if ((tick % 250) == 0) m |= INPUT_WEAPSW;
    return m;
}

static void print_grid(const GameState *s, const LiveCodeUI *lc, const TfmxPlayer *tfmx) {
    static char grid[GRID_W * GRID_H];
    char line[GRID_W + 1];
    int i;
    render(s, lc, tfmx, grid);
    for (i = 0; i < GRID_H; i++) {
        memcpy(line, grid + i * GRID_W, GRID_W);
        line[GRID_W] = 0;
        puts(line);
    }
    fflush(stdout);
}

/* 44-Byte RIFF/PCM-Header (16-Bit Mono, 16574 Hz); 1. Aufruf = Platzhalter,
 * 2. Aufruf patcht die Groessenfelder nach dem Daten-Dump */
static void write_wav_header(FILE *f, uint32_t frames) {
    uint32_t data_bytes = frames * 2;
    uint32_t chunk_size = 36 + data_bytes;
    uint8_t hdr[44];
    int i;
    uint16_t u16;
    uint32_t u32;
    hdr[0] = 'R'; hdr[1] = 'I'; hdr[2] = 'F'; hdr[3] = 'F';
    u32 = chunk_size; for (i = 0; i < 4; i++) hdr[4 + i] = (uint8_t)(u32 >> (8 * i));
    hdr[8] = 'W'; hdr[9] = 'A'; hdr[10] = 'V'; hdr[11] = 'E';
    hdr[12] = 'f'; hdr[13] = 'm'; hdr[14] = 't'; hdr[15] = ' ';
    u32 = 16; for (i = 0; i < 4; i++) hdr[16 + i] = (uint8_t)(u32 >> (8 * i));
    u16 = 1;  hdr[20] = (uint8_t)u16; hdr[21] = (uint8_t)(u16 >> 8);
    u16 = 1;  hdr[22] = (uint8_t)u16; hdr[23] = (uint8_t)(u16 >> 8);
    u32 = GAME_AUDIO_RATE; for (i = 0; i < 4; i++) hdr[24 + i] = (uint8_t)(u32 >> (8 * i));
    u32 = (uint32_t)GAME_AUDIO_RATE * 2u; for (i = 0; i < 4; i++) hdr[28 + i] = (uint8_t)(u32 >> (8 * i));
    u16 = 2;  hdr[32] = (uint8_t)u16; hdr[33] = (uint8_t)(u16 >> 8);
    u16 = 16; hdr[34] = (uint8_t)u16; hdr[35] = (uint8_t)(u16 >> 8);
    hdr[36] = 'd'; hdr[37] = 'a'; hdr[38] = 't'; hdr[39] = 'a';
    u32 = data_bytes; for (i = 0; i < 4; i++) hdr[40 + i] = (uint8_t)(u32 >> (8 * i));
    fseek(f, 0, SEEK_SET);
    fwrite(hdr, 1, 44, f);
    fseek(f, 44 + data_bytes, SEEK_SET);
}

int main(int argc, char **argv) {
    GameState state;
    uint32_t seed = 1234;
    int demo = 0;
    uint32_t demo_ticks = 2500;
    const char *audio_path = NULL;
    const char *tfmx_path = NULL;
    int subsong = 0;
    int show_intro = 1;
    int intro_only = 0;
    int play_sound = 1;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--demo")) {
            demo = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') demo_ticks = (uint32_t)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = (uint32_t)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--audio") && i + 1 < argc) {
            audio_path = argv[++i];
        } else if (!strcmp(argv[i], "--tfmx") && i + 1 < argc) {
            tfmx_path = argv[++i];
        } else if (!strcmp(argv[i], "--subsong") && i + 1 < argc) {
            subsong = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--intro")) {
            intro_only = 1;
        } else if (!strcmp(argv[i], "--no-intro")) {
            show_intro = 0;
        } else if (!strcmp(argv[i], "--no-sound")) {
            play_sound = 0;
        } else if (!strcmp(argv[i], "--sound")) {
            play_sound = 1;
        }
    }

    if (intro_only) {
        amiga_intro_run(1, 0);
        return 0;
    }

    if (!demo && show_intro) {
        amiga_intro_run(1, 0);
    }

    core_init(&state, seed);

    if (demo) {
        GameAudio audio;
        LiveCodeUI livecode;
        TfmxPlayer tfmx;
        int tfmx_active = 0;
        FILE *wav = NULL;
        int16_t abuf[GAME_AUDIO_SAMPLES_PER_TICK];
        uint32_t t;
        uint32_t total_frames = demo_ticks * GAME_AUDIO_SAMPLES_PER_TICK;

        livecode_init(&livecode, "c2 [c2 eb2] [f2 g2] <bb2 [g2 f2]>");
        game_audio_init(&audio);

        if (tfmx_path) {
            if (tfmx_player_load(&tfmx, tfmx_path, NULL)) {
                tfmx_player_start(&tfmx, (uint8_t)subsong);
                tfmx_active = 1;
            } else {
                fprintf(stderr, "[TFMX] Warnung: Konnte %s nicht laden\n", tfmx_path);
            }
        }

        if (audio_path) {
            wav = fopen(audio_path, "wb");
            if (!wav) {
                fprintf(stderr, "FEHLER: kann %s nicht oeffnen\n", audio_path);
                return 1;
            }
            write_wav_header(wav, 0); /* Platzhalter, wird nach dem Dump gepatcht */
        }

        for (t = 0; t < demo_ticks; t++) {
            core_tick(&state, bot_input(&state, t));
            livecode_tick(&livecode);

            /* In Demo: bei Halbzeit (t = 1250) Great-Bath Truhe oeffnen fuer Audio-Tribute */
            if (t == 1250) {
                livecode_open_chest(&livecode);
            }

            if (tfmx_active) {
                tfmx_player_tick(&tfmx, &audio);
            } else {
                audio.bass_override_active = 1;
                audio.bass_override_note = livecode_current_note(&livecode);
                game_audio_update(&audio, &state);
            }

            if (wav) {
                game_audio_render(&audio, abuf, GAME_AUDIO_SAMPLES_PER_TICK);
                fwrite(abuf, sizeof(int16_t), GAME_AUDIO_SAMPLES_PER_TICK, wav);
            }
        }
        if (wav) {
            write_wav_header(wav, total_frames);
            fclose(wav);
            printf("AUDIO_DUMP %s (%u Samples, 16-Bit Mono, %d Hz)\n",
                   audio_path, total_frames, GAME_AUDIO_RATE);
        }
        print_grid(&state, &livecode, tfmx_active ? &tfmx : NULL);
        printf("DEMO_DONE tick=%u state=%d wave=%u score=%u kills=%u bounces=%u hp=%u biome=%s chest=%d\n",
               state.tick, state.state, state.wave, state.score,
               state.enemies_killed, state.bounces_total, state.hp,
               livecode_biome_name(livecode.biome), livecode.chest_opened);
        fflush(stdout);
        return 0;
    }

    /* --- Interaktiver Terminal-Modus (50Hz) --- */
    {
        LiveCodeUI livecode;
        GameAudio audio;
        TfmxPlayer tfmx;
        int tfmx_active = 0;
        uint16_t prev_mask = 0;
        int paused = 0;
        int running = 1;
        static char grid[GRID_W * GRID_H];
        static char out[GRID_W * GRID_H + 256];

        livecode_init(&livecode, "c2 [c2 eb2] [f2 g2] <bb2 [g2 f2]>");
        game_audio_init(&audio);

        if (tfmx_path) {
            if (tfmx_player_load(&tfmx, tfmx_path, NULL)) {
                tfmx_player_start(&tfmx, (uint8_t)subsong);
                tfmx_active = 1;
            }
        }

        if (play_sound) {
            win_audio_open(GAME_AUDIO_RATE);
        }

#ifdef _WIN32
        enable_windows_vt();
        printf("\x1b[2J\x1b[?25l");
        while (running) {
            uint16_t mask = 0;
            while (_kbhit()) {
                int c = _getch();
                if (livecode.editor_open) {
                    if (c == 27) {
                        livecode.editor_open = 0;
                    } else if (c == '\r' || c == '\n') {
                        livecode_compile_pending(&livecode);
                    } else if (c == 8) {
                        livecode_edit_backspace(&livecode);
                    } else if (c == 0 || c == 224) {
                        int ext = _getch();
                        if (ext == 75) livecode_edit_cursor(&livecode, -1);
                        else if (ext == 77) livecode_edit_cursor(&livecode, 1);
                        else if (ext == 60) livecode_save(&livecode, "user_music/unsaved.strudel");
                        else if (ext == 61) livecode_load(&livecode, "user_music/sector1_storm.strudel");
                    } else if (c >= 32 && c <= 126) {
                        livecode_edit_char(&livecode, (char)c);
                    }
                } else {
                    if (c == 27) {
                        livecode.editor_open = 1;
                    } else if (c == 'q' || c == 'Q') {
                        running = 0;
                    } else if (c == 'z' || c == 'Z') {
                        mask |= INPUT_FIRE;
                    } else if (c == 'x' || c == 'X') {
                        mask |= INPUT_SECOND;
                    } else if (c == 'c' || c == 'C') {
                        mask |= INPUT_WEAPSW;
                    } else if (c == 'p' || c == 'P') {
                        paused = !paused;
                    } else if (c == 't' || c == 'T') {
                        livecode_open_chest(&livecode);
                    } else if (c == 'a' || c == 'A') {
                        mask |= INPUT_LEFT;
                    } else if (c == 'd' || c == 'D') {
                        mask |= INPUT_RIGHT;
                    } else if (c == 'w' || c == 'W' || c == ' ') {
                        mask |= INPUT_UP;
                    } else if (c == 's' || c == 'S') {
                        mask |= INPUT_DOWN;
                    } else if (c == 0 || c == 224) {
                        int ext = _getch();
                        if (ext == 72) mask |= INPUT_UP;
                        else if (ext == 80) mask |= INPUT_DOWN;
                        else if (ext == 77) mask |= INPUT_RIGHT;
                        else if (ext == 75) mask |= INPUT_LEFT;
                        else if (ext == 60) livecode_save(&livecode, "user_music/unsaved.strudel");
                        else if (ext == 61) livecode_load(&livecode, "user_music/sector1_storm.strudel");
                    }
                }
            }
            if ((prev_mask & INPUT_WEAPSW) && (mask & INPUT_WEAPSW))
                mask &= (uint16_t)~INPUT_WEAPSW;

            if (!paused) {
                core_tick(&state, mask);
                livecode_tick(&livecode);
                if (tfmx_active) {
                    tfmx_player_tick(&tfmx, &audio);
                } else {
                    audio.bass_override_active = 1;
                    audio.bass_override_note = livecode_current_note(&livecode);
                    game_audio_update(&audio, &state);
                }
                if (play_sound) {
                    int16_t live_abuf[GAME_AUDIO_SAMPLES_PER_TICK];
                    game_audio_render(&audio, live_abuf, GAME_AUDIO_SAMPLES_PER_TICK);
                    win_audio_write(live_abuf, GAME_AUDIO_SAMPLES_PER_TICK);
                }
            }
            prev_mask = mask;

            render(&state, &livecode, tfmx_active ? &tfmx : NULL, grid);
            {
                int off = 0;
                memcpy(out, "\x1b[H", 3); off = 3;
                for (i = 0; i < GRID_H; i++) {
                    memcpy(out + off, grid + i * GRID_W, GRID_W); off += GRID_W;
                    memcpy(out + off, "\r\n", 2); off += 2;
                }
                fwrite(out, 1, off, stdout);
                fflush(stdout);
            }
            sleep_ms(20);
        }
        printf("\x1b[?25h\n");
        if (play_sound) {
            win_audio_close();
        }
#else
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        cfmakeraw(&newt);
        newt.c_cc[VMIN] = 0;
        newt.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
        printf("\x1b[2J\x1b[?25l");

        {
            struct timespec next_t;
            clock_gettime(CLOCK_MONOTONIC, &next_t);
            while (running) {
                uint16_t mask = 0;
                char buf[16];

                while (read(STDIN_FILENO, buf, 1) == 1) {
                    if (livecode.editor_open) {
                        if (buf[0] == 27) {
                            char seq[4];
                            if (read(STDIN_FILENO, seq, 1) == 1 && seq[0] == '[') {
                                char k;
                                if (read(STDIN_FILENO, &k, 1) == 1) {
                                    if (k == 'D') livecode_edit_cursor(&livecode, -1);
                                    else if (k == 'C') livecode_edit_cursor(&livecode, 1);
                                }
                            } else {
                                livecode.editor_open = 0;
                            }
                        } else if (buf[0] == '\r' || buf[0] == '\n') {
                            livecode_compile_pending(&livecode);
                        } else if (buf[0] == 127 || buf[0] == 8) {
                            livecode_edit_backspace(&livecode);
                        } else if (buf[0] >= 32 && buf[0] <= 126) {
                            livecode_edit_char(&livecode, buf[0]);
                        }
                    } else {
                        if (buf[0] == 27) {
                            char seq[2];
                            if (read(STDIN_FILENO, seq, 1) == 1 && seq[0] == '[') {
                                char k;
                                if (read(STDIN_FILENO, &k, 1) == 1) {
                                    if (k == 'A') mask |= INPUT_UP;
                                    else if (k == 'B') mask |= INPUT_DOWN;
                                    else if (k == 'C') mask |= INPUT_RIGHT;
                                    else if (k == 'D') mask |= INPUT_LEFT;
                                }
                            } else {
                                livecode.editor_open = 1;
                            }
                        } else if (buf[0] == 'q' || buf[0] == 'Q') running = 0;
                        else if (buf[0] == 'z' || buf[0] == 'Z') mask |= INPUT_FIRE;
                        else if (buf[0] == 'x' || buf[0] == 'X') mask |= INPUT_SECOND;
                        else if (buf[0] == 'c' || buf[0] == 'C') mask |= INPUT_WEAPSW;
                        else if (buf[0] == 'p' || buf[0] == 'P') paused = !paused;
                        else if (buf[0] == 't' || buf[0] == 'T') livecode_open_chest(&livecode);
                        else if (buf[0] == 'a' || buf[0] == 'A') mask |= INPUT_LEFT;
                        else if (buf[0] == 'd' || buf[0] == 'D') mask |= INPUT_RIGHT;
                        else if (buf[0] == 'w' || buf[0] == 'W' || buf[0] == ' ') mask |= INPUT_UP;
                        else if (buf[0] == 's' || buf[0] == 'S') mask |= INPUT_DOWN;
                    }
                }
                if ((prev_mask & INPUT_WEAPSW) && (mask & INPUT_WEAPSW))
                    mask &= (uint16_t)~INPUT_WEAPSW;

                if (!paused) {
                    core_tick(&state, mask);
                    livecode_tick(&livecode);
                    if (tfmx_active) {
                        tfmx_player_tick(&tfmx, &audio);
                    } else {
                        audio.bass_override_active = 1;
                        audio.bass_override_note = livecode_current_note(&livecode);
                        game_audio_update(&audio, &state);
                    }
                }
                prev_mask = mask;

                render(&state, &livecode, tfmx_active ? &tfmx : NULL, grid);
                {
                    int off = 0;
                    memcpy(out, "\x1b[H", 3); off = 3;
                    for (i = 0; i < GRID_H; i++) {
                        memcpy(out + off, grid + i * GRID_W, GRID_W); off += GRID_W;
                        memcpy(out + off, "\r\n", 2); off += 2;
                    }
                    write(STDOUT_FILENO, out, off);
                }

                /* 50Hz-Pacing (PAL) */
                next_t.tv_nsec += 20000000;
                if (next_t.tv_nsec >= 1000000000) {
                    next_t.tv_nsec -= 1000000000;
                    next_t.tv_sec += 1;
                }
                {
                    struct timespec now;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    if (now.tv_sec > next_t.tv_sec ||
                        (now.tv_sec == next_t.tv_sec && now.tv_nsec >= next_t.tv_nsec)) {
                        next_t = now;
                    } else {
                        struct timespec d;
                        d.tv_sec = next_t.tv_sec - now.tv_sec;
                        d.tv_nsec = next_t.tv_nsec - now.tv_nsec;
                        nanosleep(&d, NULL);
                    }
                }
            }
        }

        printf("\x1b[?25h");
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
        printf("Endstand: SCORE %u  WAVE %u  KILLS %u  BIOME %s\n",
               state.score, state.wave, state.enemies_killed, livecode_biome_name(livecode.biome));
    }
    return 0;
}
