/*
 * tfmx_macro_synth.h - Clean-Room 50Hz TFMX-Style Sound-Macro Engine & Paula Simulator
 * Inspiriert von Chris Hülsbecks TFMX (Turrican 2) + Reaktiver Gameplay-Biquad-Filter
 * Lizenz: MIT
 */

#ifndef TFMX_MACRO_SYNTH_H
#define TFMX_MACRO_SYNTH_H

#include <stdint.h>
#include <math.h>
#include <string.h>

#define PAULA_VOICES 8       /* 4 klassische Hardware-Stimmen + 4 aus dem 7-Voice-Mixer */
#define SAMPLE_RAM_SIZE 65536

/* TFMX Macro Befehle (50Hz Micro-OpCodes) */
typedef enum {
    TFMX_OP_END = 0,
    TFMX_OP_SET_WAVE,     /* Setzt Start-Offset und Länge im 8-Bit Sample-RAM */
    TFMX_OP_MOD_BEGIN,    /* Verschiebt Sample-Start (erzeugt den berühmten Hülsbeck PWM-Effekt!) */
    TFMX_OP_ARP3,         /* 50Hz 3-Noten Chiptune-Arpeggio (ideal für Bounce & Chords) */
    TFMX_OP_PITCH_SLIDE,  /* Portamento / Laser-Drop */
    TFMX_OP_ENV_DECAY     /* Senkt Lautstärke schrittweise ab */
} TfmxOpType;

typedef struct {
    uint32_t wave_offset;
    uint16_t wave_len;
    int16_t  pwm_speed;      /* Sample-Start-Modulation pro 50Hz Tick */
    int8_t   arp_offsets[3]; /* Halbtöne für 50Hz Arpeggio, z.B. {0, 4, 7} oder {0, 12, 0} */
    int16_t  pitch_slide;    /* Frequenzänderung pro Tick */
    uint8_t  vol_start;      /* 0..64 (Amiga Paula Skala) */
    uint8_t  vol_decay;      /* Abzug pro Tick */
} TfmxMacroDef;

typedef struct {
    uint8_t  active;
    uint8_t  macro_id;
    int16_t  base_note;
    uint16_t tick_50hz;
    uint32_t sample_pos_fp;  /* 16.16 Fixed-Point Position im Sample-RAM */
    uint32_t sample_inc_fp;  /* 16.16 Schrittweite (Frequenz) */
    uint32_t loop_start;
    uint16_t loop_len;
    int16_t  volume;         /* 0..64 */
} CyberPaulaVoice;

typedef struct {
    int8_t   sample_ram[SAMPLE_RAM_SIZE]; /* Echter 8-Bit Signed PCM Speicher (-128..127) */
    TfmxMacroDef macros[16];
    CyberPaulaVoice voices[PAULA_VOICES];
    /* Reaktiver Staatlich-Variabler Filter (Chamberlin SVF für Cutoff & Resonanz!) */
    float    cutoff_hz;      /* Gesteuert durch Spieler-HP & Unterwasser-Biom */
    float    resonance_q;    /* Gesteuert durch Schild-Aufladung & Laser-Breite */
    float    svf_low;
    float    svf_band;
} CyberPaulaEngine;

/* Initialisiert das 8-Bit Sample-RAM mit authentischen 1990er Wellenformen (ST-01 / Turrican Stil) */
static void cyber_paula_init_waves(CyberPaulaEngine *eng) {
    memset(eng, 0, sizeof(CyberPaulaEngine));
    eng->cutoff_hz = 8000.0f;
    eng->resonance_q = 1.2f;

    /* Bank 0 (0..255): Sägezahn + Puls-Hybrid für TFMX PWM-Modulation */
    for (int i = 0; i < 256; i++) {
        eng->sample_ram[i] = (int8_t)((i < 128 ? i * 2 : (i - 256) * 2) - 64);
    }
    /* Bank 1 (256..511): FM-Slap-Bass / Metallischer Bounce-Klang (DX21 Stil) */
    for (int i = 0; i < 256; i++) {
        double p = (2.0 * 3.14159265 * i) / 256.0;
        double val = sin(p + 2.2 * sin(p * 3.0));
        eng->sample_ram[256 + i] = (int8_t)(val * 120.0);
    }
    /* Bank 2 (512..1023): D-50 AnalogString / "The Great Bath" Pad */
    for (int i = 0; i < 512; i++) {
        double p = (2.0 * 3.14159265 * i) / 512.0;
        double val = 0.5 * sin(p) + 0.3 * sin(p * 2.01) + 0.2 * sin(p * 4.0);
        eng->sample_ram[512 + i] = (int8_t)(val * 115.0);
    }

    /* Macro 0: Turrican PWM Lead/Bass (Waffe: Laser) */
    eng->macros[0] = (TfmxMacroDef){ 0, 128, 2, {0, 0, 0}, 0, 64, 1 };
    /* Macro 1: Geometric Bounce Arpeggio Pluck (Waffe: Bounce 2.0) */
    eng->macros[1] = (TfmxMacroDef){ 256, 256, 0, {0, 12, 7}, 0, 64, 2 };
    /* Macro 2: The Great Bath Pad (Unterwasser Schatztruhe) */
    eng->macros[2] = (TfmxMacroDef){ 512, 512, 1, {0, 7, 12}, 0, 56, 0 };
}

/* 50Hz Interrupt-Routine (genau wie im Amiga VBlank) */
static void cyber_paula_tick_50hz(CyberPaulaEngine *eng, uint32_t sample_rate) {
    for (int v = 0; v < PAULA_VOICES; v++) {
        CyberPaulaVoice *vc = &eng->voices[v];
        if (!vc->active) continue;
        const TfmxMacroDef *m = &eng->macros[vc->macro_id & 15];

        /* 1. TFMX Echtzeit-PWM (Startpunkt im Sample-RAM verschieben) */
        vc->loop_start = (m->wave_offset + (vc->tick_50hz * m->pwm_speed) % 64) & (SAMPLE_RAM_SIZE - 1);
        vc->loop_len   = m->wave_len;

        /* 2. 50Hz Arpeggio berechnen */
        int8_t arp = m->arp_offsets[vc->tick_50hz % 3];
        double freq = 440.0 * pow(2.0, (vc->base_note + arp - 69) / 12.0);
        double cycles_per_sample = (freq * vc->loop_len) / (double)sample_rate;
        vc->sample_inc_fp = (uint32_t)(cycles_per_sample * 65536.0);

        /* 3. Volume Decay */
        if (vc->tick_50hz > 0 && vc->volume > m->vol_decay) {
            vc->volume -= m->vol_decay;
        }
        vc->tick_50hz++;
    }
}

#endif /* TFMX_MACRO_SYNTH_H */
