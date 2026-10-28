/*
 * soundgodizer.c - STRUDELFORCE "Soundgodizer" Post-Processing-Kette
 *
 * Rechenlast: ~20 Ops/Sample @ 16574 Hz -> < 0.5 % einer Kern-CPU.
 * Alle Parameter 0..100, Presets werden in gui_main.c (Taste F5) gesetzt.
 *
 * Lizenz: MIT (Clean Room)
 */
#include "soundgodizer.h"
#include <math.h>
#include <string.h>

void god_init(Soundgodizer *g) {
    god_set_params(g, 0, 0, 0, 0);
    god_reset_state(g);
}

void god_set_params(Soundgodizer *g, int bass, int warm, int drive, int width) {
    if (bass  < 0) bass = 0;  if (bass  > 100) bass  = 100;
    if (warm  < 0) warm = 0;  if (warm  > 100) warm  = 100;
    if (drive < 0) drive = 0; if (drive > 100) drive = 100;
    if (width < 0) width = 0; if (width > 100) width = 100;
    g->bass = bass;
    g->warm = warm;
    g->drive = drive;
    g->width = width;
}

void god_reset_state(Soundgodizer *g) {
    g->lp_bass = 0.0f;
    g->lp_warm = 0.0f;
    g->dly_i = 0;
    memset(g->dly, 0, sizeof(g->dly));
}

/* --- Kern-Kette: Bass-Boost -> Warm-LP -> Soft-Drive -> Haas-Wide -------- */
void god_process(Soundgodizer *g, const int16_t *in, int n, int16_t *out_lr) {
    int i;
    float f_bass, f_warm, f_drv, f_wid;
    float lp_bass, lp_warm;

    if (!g || !in || !out_lr || n <= 0) return;

    if (!g->enabled) {
        /* Passthrough: Mono duplizieren */
        for (i = 0; i < n; i++) {
            out_lr[i * 2]     = in[i];
            out_lr[i * 2 + 1] = in[i];
        }
        return;
    }

    /* Koeffizienten (16574 Hz Basisrate):
     * Bass-LP:  fc ~ 170 Hz  -> a = 1-exp(-2*pi*170/16574) ~= 0.064
     * Warm-LP:  fc von 12kHz (warm=0) bis 3kHz (warm=100)
     * Drive:    A = 9000 (Sweetspot), f = 1 + drive*0.03 (1.0..4.0)
     * Wide:     R = y + w*(delay - y), w max 0.7 */
    f_bass = (float)g->bass  * 0.012f;             /* 0 .. 1.2x LP-Anteil  */
    f_warm = 1.0f - (float)g->warm * 0.0065f;      /* 1.0 .. 0.35 (LP-Alpha) */
    f_drv  = 1.0f + (float)g->drive * 0.03f;       /* 1.0 .. 4.0 Drive    */
    f_wid  = (float)g->width * 0.007f;             /* 0 .. 0.7 Haas-Mix   */

    lp_bass = g->lp_bass;
    lp_warm = g->lp_warm;

    for (i = 0; i < n; i++) {
        float x = (float)in[i];
        float y;
        int16_t delayed;

        /* 1) Bass-Boost: Anteil des 170-Hz-Lowpasses dazu */
        lp_bass += 0.064f * (x - lp_bass);
        y = x + f_bass * lp_bass;

        /* 2) Warm-LP: zaehmt Quadrat-Obertoene (Analog-Paula-Gefuehl) */
        lp_warm += f_warm * (y - lp_warm);
        y = lp_warm;

        /* 3) Soft-Drive: tanh-Saturation, Dry/Wet-Blend (unity bei Low-Level) */
        {
            float sat = 9000.0f * tanhf(y * f_drv * (1.0f / 9000.0f));
            y = (y * (100.0f - (float)g->drive) + sat * (float)g->drive) * 0.01f;
        }

        /* 4) Clamp + Haas-Delay-Line */
        if (y > 32767.0f) y = 32767.0f;
        if (y < -32767.0f) y = -32767.0f;
        {
            int16_t yc = (int16_t)y;
            delayed = g->dly[g->dly_i];
            g->dly[g->dly_i] = yc;
            g->dly_i = (g->dly_i + 1) & (GOD_DELAY_SAMPLES - 1);
            {
                int16_t L = yc;
                int16_t R = (int16_t)(yc + (int)(f_wid * ((int)delayed - (int)yc)));
                out_lr[i * 2]     = L;
                out_lr[i * 2 + 1] = R;
            }
        }
    }

    g->lp_bass = lp_bass;
    g->lp_warm = lp_warm;
}