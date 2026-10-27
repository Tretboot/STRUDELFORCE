/*
 * game_audio.c - STRUDELFORCE Cyber-Paula-Verbindung (Meilenstein 3)
 *
 * Reaktive Gameplay->Audio-Matrix (AGENT_2-Spec: cyber_paula_update_from_game)
 * + 8-Stimmen-Mixer mit 1-Pole-Low-Pass und optionalem 4-Bit-Quantisizer.
 *
 * PC-Target: float/double NUR fuer die Note->Frequenz-Berechnung (konsistent
 * mit tfmx_macro_synth.h). Der Gameplay-Core bleibt strikt 16.16 Fixed-Point.
 * Amiga-Backport (Agent 5) wandelt diese Engine auf Fixed-Point um.
 * Kein malloc im Hot-Path. Lizenz: MIT (Clean Room)
 */
#include "game_audio.h"
#include <math.h>
#include <string.h>

/* 1-Pole-Low-Pass-Koeffizienten 16.16: 1 - exp(-2*pi*fc/fs), fs = 16574 */
#define CUTOFF_8000_FP 62377
#define CUTOFF_550_FP  12327

/* 50Hz Bassline: C-Moll (Biom 1 "Chrome Tempest") */
static const int16_t BASSLINE[8] = { 36, 36, 43, 36, 39, 36, 41, 39 };

typedef struct {
    uint16_t off;
    uint16_t len;
    int8_t   arp[3];
    uint8_t  decay;
} MacDef;

/* 0: PWM-Saw (Laser-Bass)  1: FM-Pluck (Bounce, Arp)  2: Zap  3: Boom  4: Tick */
static const MacDef MACROS[5] = {
    { 0,   128, { 0,  0,  0}, 1  },
    { 256, 256, { 0, 12,  7}, 2  },
    { 256, 256, { 0,  0,  0}, 8  },
    { 0,   128, { 0,  0,  0}, 12 },
    { 256, 256, { 0,  0,  0}, 16 }
};

static void voice_set(GameAudio *a, int v, uint8_t macro, int16_t note, int16_t vol) {
    GameVoice *vc = &a->voices[v];
    vc->active = 1;
    vc->macro_id = macro;
    vc->base_note = note;
    vc->tick_50hz = 0;
    vc->pos_fp = 0;
    vc->volume = vol;
}

void game_audio_init(GameAudio *a) {
    int i;
    memset(a, 0, sizeof(GameAudio));
    a->cutoff_fp = CUTOFF_8000_FP;
    a->pwm_speed = 2;
    a->bass_override_active = 0;
    a->bass_override_note = -1;
    a->ext_sample_ram = NULL;
    a->ext_sample_size = 0;
    a->ext_render = NULL;
    a->ext_ctx = NULL;
    /* Bank 0 (0..255): Saegezahn + Puls-Hybrid fuer TFMX PWM-Modulation */
    for (i = 0; i < 256; i++) {
        a->sample_ram[i] = (int8_t)((i < 128 ? i * 2 : (i - 256) * 2) - 64);
    }
    /* Bank 1 (256..511): FM-Slap/Pluck (DX21-Stil) */
    for (i = 0; i < 256; i++) {
        double p = (2.0 * 3.14159265358979 * i) / 256.0;
        a->sample_ram[256 + i] = (int8_t)(sin(p + 2.2 * sin(3.0 * p)) * 120.0);
    }
    /* Bank 2 (512..1023): D-50 AnalogString ("The Great Bath") */
    for (i = 0; i < 512; i++) {
        double p = (2.0 * 3.14159265358979 * i) / 512.0;
        a->sample_ram[512 + i] = (int8_t)((0.5 * sin(p) + 0.3 * sin(2.01 * p) + 0.2 * sin(4.0 * p)) * 115.0);
    }
}

void game_audio_update(GameAudio *a, const GameState *s) {
    int v;
    a->tick++;

    /* 1. Waffe -> Bass-Macro + Arpeggio (AGENT_2-Spec) */
    if (s->weapon == WEAPON_LASER) {
        a->bass_macro = 0;
        a->arp_active = 0;
    } else if (s->weapon == WEAPON_BOUNCE) {
        a->bass_macro = 1;
        a->arp_active = 1;
    } else {
        a->bass_macro = 0;
        a->arp_active = 0;
    }

    /* 2. Schild (0..100) -> PWM-Modulationsgeschwindigkeit (Spec: Q 0.7..8.5) */
    a->pwm_speed = (uint8_t)(2 + s->shield / 25);

    /* 3. HP < 25% -> Cutoff 550Hz + 4-Bit-Quantisierung */
    if (s->max_hp > 0 && (uint32_t)s->hp * 100u / (uint32_t)s->max_hp < 25u) {
        a->cutoff_fp = CUTOFF_550_FP;
        a->quantize_4bit = 1;
    } else {
        a->cutoff_fp = CUTOFF_8000_FP;
        a->quantize_4bit = 0;
    }

    /* 4. SFX-Transitions (read-only Diff der Core-Zaehler) */
    if (s->shots_fired > a->last_shots) {
        a->last_shots = s->shots_fired;
        voice_set(a, 2, 2,
                  (s->weapon == WEAPON_LASER) ? 60 : (s->weapon == WEAPON_BOUNCE ? 64 : 57), 48);
    }
    if (s->enemies_killed > a->last_kills) {
        a->last_kills = s->enemies_killed;
        voice_set(a, 3, 3, 24, 56);
    }
    if (s->bounces_total > a->last_bounces) {
        a->last_bounces = s->bounces_total;
        voice_set(a, 4, 4, 72, 40);
    }

    /* 5. Bass- + Arp-Sequencer (Schritt alle 5 Ticks = 10 Hz) */
    {
        int16_t bass_note;
        GameVoice *bass = &a->voices[0];
        GameVoice *arp = &a->voices[1];
        if (a->bass_override_active) {
            bass_note = a->bass_override_note;
        } else {
            bass_note = BASSLINE[(a->tick / 5) % 8];
        }
        if (bass_note < 0) {
            bass->active = 0;
            arp->active = 0;
        } else {
            if (!bass->active) {
                voice_set(a, 0, a->bass_macro, bass_note, 32);
            } else {
                bass->macro_id = a->bass_macro;
                bass->base_note = bass_note;
                if (bass->volume < 32) bass->volume = 32;
            }
            if (a->arp_active) {
                if (!arp->active) {
                    voice_set(a, 1, 1, bass_note + 12, 24);
                } else {
                    arp->base_note = bass_note + 12;
                    if (arp->volume < 24) arp->volume = 24;
                }
            } else {
                arp->active = 0;
            }
        }
    }

    /* 6. 50Hz-Macro-Tick (PWM, Arp, Decay, Frequenz) */
    for (v = 0; v < GAME_AUDIO_VOICES; v++) {
        GameVoice *vc = &a->voices[v];
        const MacDef *m;
        int16_t note;
        double freq;
        if (!vc->active) continue;
        m = &MACROS[vc->macro_id & 4];
        if (vc->tick_50hz > 0 && vc->volume > m->decay) {
            vc->volume -= m->decay;
        } else if (vc->volume <= m->decay) {
            vc->active = 0;
            continue;
        }
        vc->tick_50hz++;
        note = vc->base_note;
        if (vc->macro_id == 1) note += m->arp[vc->tick_50hz % 3];
        vc->loop_start = (uint16_t)((m->off + (vc->tick_50hz * a->pwm_speed) % 64) & 0xFFFF);
        vc->loop_len = m->len;
        freq = 440.0 * pow(2.0, (double)(note - 69) / 12.0);
        vc->inc_fp = (uint32_t)((freq * (double)m->len) / (double)GAME_AUDIO_RATE * 65536.0);
    }
}

void game_audio_render_synth(GameAudio *a, int16_t *out, int n) {
    int i, v;
    for (i = 0; i < n; i++) {
        int32_t sum = 0;
        for (v = 0; v < GAME_AUDIO_VOICES; v++) {
            GameVoice *vc = &a->voices[v];
            uint32_t idx;
            int8_t smp;
            if (!vc->active || vc->volume <= 0) continue;
            idx = (uint32_t)vc->loop_start + ((vc->pos_fp >> 16) % (uint32_t)vc->loop_len);
            if (a->ext_sample_ram && a->ext_sample_size > 0) {
                smp = a->ext_sample_ram[idx % a->ext_sample_size];
            } else {
                smp = a->sample_ram[idx & 0xFFFF];
            }
            sum += ((int32_t)smp * vc->volume) >> 6;
            vc->pos_fp += vc->inc_fp;
            if (vc->pos_fp >= (uint32_t)vc->loop_len << 16) {
                vc->pos_fp -= (uint32_t)vc->loop_len << 16;
            }
        }
        /* 1-Pole-Low-Pass (16.16) */
        a->lp_state += (int32_t)(((int64_t)a->cutoff_fp * (sum - a->lp_state)) >> 16);
        {
            int32_t y = a->lp_state;
            if (a->quantize_4bit) y = (y + 8) & ~15;
            if (y > 127) y = 127;
            else if (y < -128) y = -128;
            out[i] = (int16_t)(y << 8);
        }
    }
}

/* v2 wrapper: external source (TFMX player / SfMixer) replaces the synth when attached.
   To hear both, add game_audio_render_synth as an SfMixer source. */
void game_audio_render(GameAudio *a, int16_t *out, int n) {
    if (a->ext_render) { a->ext_render(a->ext_ctx, out, n); return; }
    game_audio_render_synth(a, out, n);
}
