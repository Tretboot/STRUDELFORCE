/*
 * test_win_audio.c - Selftest fuer den Win-Audio-Transport v2 (Ring + Thread)
 *
 *   - Ring-Logik (Wrap / Overflow / Underrun-Hold) ohne Geraet
 *   - Optional: 2 s 440 Hz Sinus ueber das Geraet, falls eine Soundkarte da ist
 *
 * Build:  cl test_win_audio.c win_audio.c user32.lib gdi32.lib winmm.lib
 * Exit 0 = OK.  Lizenz: MIT (Clean Room)
 */
#include "win_audio.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(unsigned int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleep_ms(unsigned int ms) { usleep(ms * 1000); }
#endif

int main(void) {
    int r = win_audio_selftest();

    if (r != 0) {
        printf("WIN_AUDIO SELFTEST FAIL (fails=%d)\n", r);
        return 1;
    }
    printf("WIN_AUDIO ring selftest OK (Wrap / Overflow / Underrun-Hold)\n");

    if (win_audio_open(WIN_AUDIO_RATE)) {
        /* 2 s 440 Hz Sinus (6000/32767 Amplitude), Integer-Phasen-Akkumulator */
        static int16_t quarter[8] = { 0, 4242, 6000, 4242, 0, -4242, -6000, -4242 };
        static int16_t chunk[WIN_AUDIO_BUF_SAMPLES];
        int acc = 0;
        int chunks = WIN_AUDIO_RATE * 2 / WIN_AUDIO_BUF_SAMPLES;
        int c, j;

        for (c = 0; c < chunks; c++) {
            for (j = 0; j < WIN_AUDIO_BUF_SAMPLES; j++) {
                acc = (int)((uint32_t)acc + 1745u); /* 440 Hz / 16574 Hz * 65536 */
                chunk[j] = quarter[(acc >> 13) & 7];
            }
            win_audio_write(chunk, WIN_AUDIO_BUF_SAMPLES);
            sleep_ms((unsigned int)(WIN_AUDIO_BUF_SAMPLES * 1000 / WIN_AUDIO_RATE));
        }
        win_audio_close();
        printf("WIN_AUDIO device selftest OK (2 s Sinus abgespielt)\n");
    } else {
        printf("WIN_AUDIO device nicht verfuegbar (Headless) - OK\n");
    }

    return 0;
}