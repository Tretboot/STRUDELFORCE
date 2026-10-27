/*
 * amiga_intro.c - STRUDELFORCE Amiga 500 Terminal Intro
 *
 * Simulierte Amiga-Copperbar-Hintergrundgrafik und Sinus-tanzender
 * Text-Scroller im ANSI/ASCII-Terminal-Modus (50Hz PAL).
 *
 * Lizenz: MIT (Clean Room)
 */

#define _CRT_SECURE_NO_WARNINGS
#include "amiga_intro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

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
    if (_kbhit()) {
        _getch();
        return 1;
    }
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
        max_frames = 25; /* Automatisch 25 Frames bei Pipe / Non-TTY */
    }
#else
    if (!isatty(STDIN_FILENO) && max_frames == 0) {
        max_frames = 25;
    }
#endif

#ifdef _WIN32
    /* Windows VT100 Modus aktivieren */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | 0x0004); /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
    }
#endif

    /* Cursor verstecken und Bildschirm leeren */
    printf("\x1b[?25l\x1b[2J\x1b[H");
    fflush(stdout);

    while (1) {
        char frame_buf[SCREEN_H][SCREEN_W + 1];
        uint8_t bg_r[SCREEN_H], bg_g[SCREEN_H], bg_b[SCREEN_H];
        int y, x;

        if (interactive && check_input()) {
            break;
        }

        if (max_frames > 0 && frame >= max_frames) {
            break;
        }

        /* 1. Copperbars Hintergrund berechnen */
        double y1 = 12.0 + 6.5 * sin(frame * 0.06);
        double y2 = 12.0 + 7.5 * sin(frame * 0.08 + 1.8);
        double y3 = 12.0 + 5.5 * sin(frame * 0.10 + 3.4);

        for (y = 0; y < SCREEN_H; y++) {
            double r = 0, g = 0, b = 0;

            /* Bar 1: Copper Gold / Orange */
            double d1 = fabs((double)y - y1);
            if (d1 < 2.5) {
                double i1 = 1.0 - (d1 / 2.5);
                r += 230.0 * i1;
                g += 120.0 * i1;
                b += 20.0  * i1;
            }

            /* Bar 2: Electric Amiga Cyan */
            double d2 = fabs((double)y - y2);
            if (d2 < 2.5) {
                double i2 = 1.0 - (d2 / 2.5);
                r += 10.0  * i2;
                g += 170.0 * i2;
                b += 240.0 * i2;
            }

            /* Bar 3: Amiga Purple / Magenta */
            double d3 = fabs((double)y - y3);
            if (d3 < 2.5) {
                double i3 = 1.0 - (d3 / 2.5);
                r += 190.0 * i3;
                g += 20.0  * i3;
                b += 190.0 * i3;
            }

            /* Tiefes Amiga-Blau als Basis ($000020) */
            b += 15.0;

            bg_r[y] = (uint8_t)(r > 255.0 ? 255 : r);
            bg_g[y] = (uint8_t)(g > 255.0 ? 255 : g);
            bg_b[y] = (uint8_t)(b > 255.0 ? 255 : b);

            /* Zeile mit Leerzeichen initialisieren */
            for (x = 0; x < SCREEN_W; x++) {
                frame_buf[y][x] = ' ';
            }
            frame_buf[y][SCREEN_W] = '\0';
        }

        /* 2. Amiga Header / ASCII Logo einsetzen */
        for (y = 0; y < 5; y++) {
            int len = (int)strlen(LOGO_ASCII[y]);
            int offset = (SCREEN_W - len) / 2;
            if (offset >= 0 && offset + len <= SCREEN_W) {
                memcpy(&frame_buf[2 + y][offset], LOGO_ASCII[y], len);
            }
        }

        /* Subheader */
        {
            const char *sub1 = "--- COMMODORE AMIGA 500 * 50HZ PAL * CYBER-PAULA TFMX ---";
            int len1 = (int)strlen(sub1);
            int off1 = (SCREEN_W - len1) / 2;
            if (off1 >= 0) memcpy(&frame_buf[8][off1], sub1, len1);

            const char *sub2 = ">>> STRUDEL CLEANROOM AST * 16.16 FIXED POINT SIMULATION <<<";
            int len2 = (int)strlen(sub2);
            int off2 = (SCREEN_W - len2) / 2;
            if (off2 >= 0) memcpy(&frame_buf[10][off2], sub2, len2);
        }

        /* 3. Sinus-tanzender Text-Scroller */
        int scroll_offset = frame;
        for (x = 0; x < SCREEN_W; x++) {
            int char_idx = (x + scroll_offset) % scroller_len;
            char c = SCROLLER_TEXT[char_idx];

            /* Vertikaler Sinus-Versatz */
            double sine_val = sin((double)x * 0.16 + (double)frame * 0.12);
            int sine_y = 17 + (int)floor(sine_val * 2.2 + 0.5);

            if (sine_y >= 0 && sine_y < SCREEN_H) {
                frame_buf[sine_y][x] = c;
            }
        }

        /* 4. Footer Info */
        {
            const char *hw_info = "[ PAL 50Hz | Chip RAM: 512KB | OCS 6 Planes | Paula: 8 Voices ]";
            int len = (int)strlen(hw_info);
            int off = (SCREEN_W - len) / 2;
            if (off >= 0) memcpy(&frame_buf[22][off], hw_info, len);
        }

        /* 5. Bild am Terminal ausgeben */
        printf("\x1b[H"); /* Cursor nach oben links */

        for (y = 0; y < SCREEN_H; y++) {
            /* Background RGB */
            printf("\x1b[48;2;%d;%d;%dm", bg_r[y], bg_g[y], bg_b[y]);

            /* Zeilentext ausgeben mit passender Textfarbe */
            if (y >= 2 && y <= 6) {
                /* Logo: Silber / Cyan Highlight */
                printf("\x1b[38;2;240;255;255m\x1b[1m%s", frame_buf[y]);
            } else if (y == 8 || y == 10) {
                /* Subheader: Gold */
                printf("\x1b[38;2;255;220;80m%s", frame_buf[y]);
            } else if (y >= 15 && y <= 20) {
                /* Scroller: Leuchtend Gelb/Weiss */
                printf("\x1b[38;2;255;255;200m\x1b[1m%s", frame_buf[y]);
            } else if (y == 22) {
                /* Statusleiste: Grün / Amber */
                printf("\x1b[38;2;140;255;160m%s", frame_buf[y]);
            } else {
                /* Standardtext */
                printf("\x1b[38;2;200;200;200m%s", frame_buf[y]);
            }

            printf("\x1b[0m\n");
        }

        fflush(stdout);
        sleep_ms(20); /* 50Hz = 20ms */
        frame++;
    }

    /* Reset und Cursor wieder einblenden */
    printf("\x1b[0m\x1b[?25h\x1b[2J\x1b[H");
    fflush(stdout);
    return 0;
}
