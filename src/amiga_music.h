/*
 * amiga_music.h - EMBEDdete TFMX-Tracks fuer den M68K-Build (v0.3.1)
 * ------------------------------------------------------------------
 * Der Amiga-Build hat keinen Dateizugriff (kein stdio/-nostdlib). Deshalb
 * werden die 4 kuratierten Tracks (TFMX + SAM) per INCBIN direkt ins Binary
 * eingebettet (siehe amiga_music.c):
 *   1. Turrican 3 - World 1 Factory Action (mit Subsongs & Level-Action)
 *   2. BiFi Adventure (legendäre Chris-Hülsbeck-Subtracks)
 *   3. Cybersty (Action-Synth)
 *   4. Mad TV Ongame 6 (Amiga-Klassiker)
 *
 * Voraussetzungen vor dem 68k-Build:
 *   make stage_amiga_music   (oder: python tools/stage_amiga_music.py)
 */
#ifndef AMIGA_MUSIC_H
#define AMIGA_MUSIC_H

#ifdef M68K
#include <stdint.h>

#define T2_TRACK_COUNT 4

extern const uint8_t * const t2_tfx[T2_TRACK_COUNT];
extern const uint32_t t2_tfx_len[T2_TRACK_COUNT];
extern const uint8_t * const t2_sam[T2_TRACK_COUNT];
extern const uint32_t t2_sam_len[T2_TRACK_COUNT];
extern const char * const t2_names[T2_TRACK_COUNT];

#endif /* M68K */
#endif /* AMIGA_MUSIC_H */
