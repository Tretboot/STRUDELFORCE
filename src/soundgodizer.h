/*
 * soundgodizer.h - STRUDELFORCE "Soundgodizer" Post-Processing-Kette (optional)
 *
 * Zuschaltbare HQ-Overdrive-Kette fuer die Windows-Version (GUI, Taste F5):
 *   1. Bass-Boost : 1-Pole-Lowpass-Anteil (Analog-Waerme im Unterton)
 *   2. Warm-LP    : 1-Pole-Tiefpass, zaehmt die harten TFMX-Quadrat-Obertoene
 *   3. Soft-Drive : tanh-Saturation mit Dry/Wet-Blend (fettet die Bassline)
 *   4. Haas-Wide  : Mono -> Stereo, rechtes Ohr mit ~7.7ms Delay-Line
 *
 * C89, integer-State (nur float-Koeffizienten), null malloc, null extern.
 * Nur fuer PC-Targets verlinken (Amiga-M68K bleibt Paula-pur).
 *
 * Lizenz: MIT (Clean Room)
 */
#ifndef SOUNDGODIZER_H
#define SOUNDGODIZER_H

#include <stdint.h>

#define GOD_DELAY_SAMPLES 128 /* 128 Samples ~ 7.7 ms @ 16574 Hz (Haas-Sweetspot) */

typedef struct {
    int     enabled;
    int     bass;    /* 0..100: Bass-Boost-Staerke       */
    int     warm;    /* 0..100: Warm-Tiefpass (0 = aus)  */
    int     drive;   /* 0..100: Soft-Saturation (Wet)    */
    int     width;   /* 0..100: Haas-Stereo-Wide         */
    float   lp_bass; /* Bass-Lowpass-Zustand             */
    float   lp_warm; /* Warm-Lowpass-Zustand             */
    int16_t dly[GOD_DELAY_SAMPLES]; /* Haas-Delay-Line (rechts) */
    int     dly_i;
} Soundgodizer;

void god_init(Soundgodizer *g);
void god_set_params(Soundgodizer *g, int bass, int warm, int drive, int width);
void god_reset_state(Soundgodizer *g); /* Filter-/Delay-State loeschen (kein Knackser) */

/* Mono 16-Bit In -> Stereo 16-Bit Out (L,R verflochten), n Samples.
 * Wenn !enabled: einfache Mono->Stereo-Duplizierung (identische API). */
void god_process(Soundgodizer *g, const int16_t *in, int n, int16_t *out_lr);

#endif /* SOUNDGODIZER_H */