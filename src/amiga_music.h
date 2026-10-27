/*
 * amiga_music.h - EMBEDdete Turrican-2-TFMX-Tracks fuer den M68K-Build
 * ------------------------------------------------------------------
 * Der Amiga-Build hat keinen Dateizugriff (kein stdio/-nostdlib). Deshalb
 * werden die 5 Tracks (TFMX + SAM) per INCBIN direkt ins Binary eingebettet
 * (siehe amiga_music.c). Daten liegen im .rodata (fast RAM beim A500).
 *
 * Voraussetzungen vor dem 68k-Build:
 *   make stage_amiga_music   (oder: python tools/stage_amiga_music.py)
 * .incbin-Pfade sind relativ zum CWD des Compilers (= Projekt-Root).
 */
#ifndef AMIGA_MUSIC_H
#define AMIGA_MUSIC_H

#ifdef M68K
#include <stdint.h>

#define T2_TRACK_COUNT 5

extern const uint8_t * const t2_tfx[T2_TRACK_COUNT];
extern const uint32_t t2_tfx_len[T2_TRACK_COUNT];
extern const uint8_t * const t2_sam[T2_TRACK_COUNT];
extern const uint32_t t2_sam_len[T2_TRACK_COUNT];
extern const char * const t2_names[T2_TRACK_COUNT];

#endif /* M68K */
#endif /* AMIGA_MUSIC_H */