/*
 * win_audio.h - STRUDELFORCE Real-Time Low-Latency Audio Engine
 *
 * Echtes Windows WaveOut Audio-Backend (winmm) fuer 16574 Hz 16-Bit Mono PCM.
 * Spielt den Cyber-Paula / TFMX Sound in Echtzeit ueber PC-Lautsprecher/Kopfhoerer ab.
 * Null externe Abhaengigkeiten.
 *
 * Lizenz: MIT (Clean Room)
 */

#ifndef WIN_AUDIO_H
#define WIN_AUDIO_H

#include <stdint.h>

/* Win-Audio-Transport v2 (2026-10-27):
 * Ring-Puffer (~8 s) + dedizierter time-critical WaveOut-Thread.
 * Enthaelt das Knistern/Underrun-Artefakt aus v1 (1 Buffer/Frame, Blocking-Wait). */

#define WIN_AUDIO_RATE 16574
#define WIN_AUDIO_BUFFERS 8
#define WIN_AUDIO_BUF_SAMPLES 331 /* 16574 / 50Hz = ~331 Samples pro PAL-Tick */

int  win_audio_open(int sample_rate);
void win_audio_write(const int16_t *samples, int count);      /* Mono-Quell (L=R dupliziert) */
void win_audio_write_frames(const int16_t *frames, int n);    /* Stereo-Frames (L,R verflochten) */
void win_audio_close(void);
int  win_audio_is_active(void);
int  win_audio_selftest(void); /* 0 = OK; Ring-Logik (Wrap/Overflow/Underrun), kein Geraet */

#endif /* WIN_AUDIO_H */

