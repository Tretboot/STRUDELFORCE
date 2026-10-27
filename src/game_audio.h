/*
 * game_audio.h - STRUDELFORCE Cyber-Paula-Verbindung (Meilenstein 3)
 *
 * Reaktive Gameplay->Audio-Matrix (AGENT_2-Spec) + 8-Stimmen-Mixer mit
 * 1-Pole-Low-Pass und optionalem 4-Bit-Quantisizer. Kein malloc im Hot-Path.
 *
 * PC-Target: float/double NUR fuer die Note->Frequenz-Berechnung (konsistent
 * mit tfmx_macro_synth.h). Der Gameplay-Core bleibt strikt 16.16 Fixed-Point.
 * Amiga-Backport (Agent 5) wandelt diese Engine auf Fixed-Point um.
 *
 * Lizenz: MIT (Clean Room)
 */
#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <stdint.h>
#include "core_sim.h"

#define GAME_AUDIO_RATE 16574
#define GAME_AUDIO_VOICES 8
#define GAME_AUDIO_SAMPLES_PER_TICK 331  /* 16574 / 50 */

typedef struct GameAudio {
    uint8_t  active;
    uint8_t  macro_id;
    int16_t  base_note;
    uint16_t tick_50hz;
    uint32_t pos_fp;      /* 16.16 Position im Loop */
    uint32_t inc_fp;      /* 16.16 Schrittweite (Frequenz) */
    uint32_t loop_start;  /* Offset im Sample-RAM (unterstuetzt TFMX bis 256KB) */
    uint32_t loop_len;    /* Loop-Laenge */
    int16_t  volume;      /* 0..64 (Paula-Skala) */
} GameVoice;

typedef struct {
    int8_t   sample_ram[65536];
    GameVoice voices[GAME_AUDIO_VOICES];
    uint32_t tick;
    uint8_t  bass_macro;      /* 0 = PWM-Saw (Laser), 1 = FM-Pluck (Bounce) */
    uint8_t  arp_active;      /* 50Hz Oktav-Arpeggio (Bounce) */
    uint8_t  pwm_speed;       /* 2..6, gesteuert durch Schild */
    uint16_t cutoff_fp;       /* 16.16 Low-Pass-Koeffizient */
    uint8_t  quantize_4bit;   /* HP<25%: 4-Bit-Quantisierung */
    uint32_t last_shots;
    uint32_t last_kills;
    uint32_t last_bounces;
    uint8_t  bass_override_active; /* 1 = Strudel-Pattern steuert die Bassline */
    int16_t  bass_override_note;   /* -1 = Rest, sonst MIDI-Note */
    int32_t  lp_state;
    const int8_t *ext_sample_ram;  /* Optionaler externer TFMX 256KB Sample-Puffer */
    uint32_t ext_sample_size;
    void (*ext_render)(void *ctx, int16_t *out, int n); /* v2: TFMX/MOD/mixer hook */
    void *ext_ctx;
} GameAudio;

void game_audio_init(GameAudio *a);
void game_audio_update(GameAudio *a, const GameState *s); /* 50Hz */
void game_audio_render(GameAudio *a, int16_t *out, int n); /* 16574 Hz, honours ext_render */
void game_audio_render_synth(GameAudio *a, int16_t *out, int n); /* built-in synth only */

#endif /* GAME_AUDIO_H */
