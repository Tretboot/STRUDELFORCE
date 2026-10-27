/*
 * cyber_paula.c - STRUDELFORCE Cyber-Paula Audio Engine (AGENT 2)
 * 16-Step-Strudel-Sequencer + 8-Stimmen-TFMX-Mixer ueber 64KB 8-Bit-
 * Sample-RAM (frozen), reaktive Patch-Matrix (Waffe/Schild/HP/Truhe).
 * 68K-Regeln: kein malloc, keine VLAs, kein float im 50Hz-Pfad
 * (Frequenz-Mathematik: 1.16-Tabelle SF_CP_FREQ12 + Oktav-Shifts).
 *
 * Phase-1.1 (Code-Review-Runde): cyber_paula_voices() fuer den Amiga-
 * VBlank-Register-Write (Agent 5); macro_bass-Reset im else-Branch
 * (non-LASER/non-BOUNCE → PWM-Default 0, kein geerbtes Vorwert-Verhalten).
 */
#include "sf_interfaces.h"
#include "cyber_paula.h"
#include "strudel_cleanroom.h"

/* Name-Kollision: der frozen Header bringt ein STATIC cyber_paula_tick_50hz
 * (CyberPaulaEngine*, uint32_t). Public-API = externe Signatur (GameState*,
 * uint32_t). C99 6.2.2: ein Identifier darf nicht beide Linkagen tragen,
 * also wird der frozen static beim Include umbenannt (Sandwich-Makro). */
#define cyber_paula_tick_50hz tfmx_frozen_tick_50hz
#include "tfmx_macro_synth.h"
#undef cyber_paula_tick_50hz

#include <stdio.h>
#include <string.h>

static CyberPaulaEngine      frozen;
static StrudelPattern        pattern_a;
static StrudelPattern        pattern_b;
static uint8_t               use_pattern_b;
static uint32_t              cycle_num;
static uint32_t              step_in_cycle;
static CyberPaulaVoice       voices[SF_CP_VOICES];
static fix16_t               cutoff_fp;
static fix16_t               q_fp;      /* Q*10 in 16.16: 70=Q0.7, 850=Q8.5 */
static uint8_t               sub_bass_up;
static uint8_t               low_hp_mode;
static uint8_t               great_bath;
static uint8_t               quant_drums;
static uint8_t               macro_bass;
static CyberPaulaDiagnostics diag;

/* 1.16 Semiton-Faktoren 2^(n/12)*65536, n=0..11 */
static const uint32_t SF_CP_FREQ12[12] = {
    65536u, 69433u, 73562u, 77936u, 82570u, 87480u,
    92682u, 98193u, 104032u, 110218u, 116772u, 123715u
};

#define SF_CP_BASS_NOTE 36
static const int16_t SF_CP_GREAT_BATH_CHORD[7] = { 48, 51, 55, 60, 63, 67, 72 };

static void sf_cp_voice_set(int v, uint8_t macro_id, int16_t base_note, int16_t volume)
{
    const TfmxMacroDef *m = &frozen.macros[macro_id & 15];
    CyberPaulaVoice *vc = &voices[v];
    vc->active        = 1;
    vc->macro_id      = macro_id;
    vc->base_note     = base_note;
    vc->volume        = volume;
    vc->tick_50hz     = 0;
    vc->sample_pos_fp = 0;
    vc->loop_start    = m->wave_offset;
    vc->loop_len      = m->wave_len;
}

static void sf_cp_voice_off(int v) { voices[v].active = 0; }

/* 16.16 Sample-Inkrement: (440 * 2^((note-69)/12) * loop_len / sample_rate)
 * Reine Integer-Mathematik: 2^(x/12) = Oktav-Shift * SF_CP_FREQ12[rest]. */
static uint32_t sf_cp_sample_inc_fp(int32_t midi_note, uint32_t loop_len, uint32_t sample_rate)
{
    int32_t semi, oct, rem;
    int64_t freq_fp16, inc;

    if (sample_rate == 0) sample_rate = 44100;
    if (loop_len == 0)    loop_len = 1;

    semi = midi_note - 69;
    oct  = semi / 12;
    rem  = semi % 12;
    if (rem < 0) { rem += 12; oct -= 1; }   /* C99 % trunciert Richtung 0 */

    freq_fp16 = (int64_t)440 * (int64_t)SF_CP_FREQ12[rem];
    if (oct >= 0) freq_fp16 <<= oct;
    else          freq_fp16 >>= -oct;

    inc = (freq_fp16 * (int64_t)loop_len) / (int64_t)sample_rate;
    if (inc < 1) inc = 1;
    return (uint32_t)inc;
}

static void sf_cp_refresh_diag(void)
{
    int v;
    diag.cutoff_fp     = cutoff_fp;
    diag.q_fp          = q_fp;
    diag.macro_bass    = macro_bass;
    diag.sub_bass_up   = sub_bass_up;
    diag.low_hp_mode   = low_hp_mode;
    diag.great_bath    = great_bath;
    diag.quant_drums   = quant_drums;
    diag.active_voices = 0;
    for (v = 0; v < SF_CP_VOICES; v++) {
        if (voices[v].active) diag.active_voices++;
    }
}

void cyber_paula_init(void)
{
    memset(&pattern_a, 0, sizeof(pattern_a));
    memset(&pattern_b, 0, sizeof(pattern_b));
    memset(voices, 0, sizeof(voices));
    memset(&diag, 0, sizeof(diag));

    use_pattern_b = 0;
    cycle_num     = 0;
    step_in_cycle = 0;
    sub_bass_up   = 0;
    low_hp_mode   = 0;
    great_bath    = 0;
    quant_drums   = 0;
    macro_bass    = 0;
    cutoff_fp     = sf_fix(8000);
    q_fp          = sf_fix(12);      /* Q = 1.2 (Q*10 in 16.16) */

    strudel_compile(&pattern_a, "c2 [eb2 g2]", 0);

    /* FROZEN: fuellt 64KB Sample-RAM (Saw/FM-Slap/D-50-Pad) + 16 Macros.
     * double nur zur Init-Zeit, nie im 50Hz-Pfad (68k-Regel). */
    cyber_paula_init_waves(&frozen);

    /* Game-Spec Override: Bounce-Pluck (Macro 1) arpeggiert {0,12,0}. */
    frozen.macros[1].arp_offsets[0] = 0;
    frozen.macros[1].arp_offsets[1] = 12;
    frozen.macros[1].arp_offsets[2] = 0;

    /* Ein No-Op-Call in den frozen Referenz-Tick (alle frozen Voices
     * inaktiv): Funktion referenziert, Header beim Boot gesaenit. */
    tfmx_frozen_tick_50hz(&frozen, 44100);

    sf_cp_refresh_diag();
}

void cyber_paula_update_from_game(const GameState *s)
{
    int32_t shield;
    int v;

    /* 1. Waffe -> Bass-Makro + Sub-Bass-Boost.
     * Review-Fix: else-Branch setzt macro_bass=0 (PWM-Default),
     * sonst wuerde ein verlassener LASER/BOUNCE-Zustand geerbt. */
    if (s->player.weapon == SF_WPN_LASER) {
        macro_bass  = 0;   /* TFMX PWM-Saw-Bass */
        sub_bass_up = 1;   /* +4 dB Sub-Bass */
    } else if (s->player.weapon == SF_WPN_BOUNCE) {
        macro_bass  = 1;   /* FM-Pluck, 50Hz Oktav-Arpeggio {0,12,0} */
        sub_bass_up = 0;
    } else {
        macro_bass  = 0;
        sub_bass_up = 0;
    }

    /* 2. Schild (0..100) -> Resonanz-Q (0.7 .. 8.5, als Q*10 in 16.16) */
    shield = s->player.shield;
    if (shield < 0)   shield = 0;
    if (shield > 100) shield = 100;
    q_fp = sf_fix(70) + (fix16_t)((int64_t)(sf_fix(850) - sf_fix(70)) * shield / 100);

    /* 3. Low HP -> gedaempfter Cutoff + 4-Bit-Drum-Quantisierung */
    if (s->player.hp < 25) {
        low_hp_mode = 1;
        cutoff_fp   = sf_fix(550);
        quant_drums = 1;
    } else {
        low_hp_mode = 0;
        cutoff_fp   = sf_fix(8000);
        quant_drums = 0;
    }

    /* 4. Great-Bath (Unterwasser-Truhe) -> 7-stimmiges D-50-Pad-Tribute.
     * Stimmen-Layout: v0..v6 = Pad-Akkord, v7 frei (Bass weicht bewusst). */
    if (s->chest_opened == 1) {
        great_bath = 1;
        macro_bass = 2;
        for (v = 0; v < 7; v++) {
            if (!voices[v].active || voices[v].macro_id != 2) {
                sf_cp_voice_set(v, 2, SF_CP_GREAT_BATH_CHORD[v], 56);
            }
        }
        cutoff_fp = sf_fix(3000);
        q_fp      = sf_fix(30);   /* Q = 3.0 */
    } else {
        great_bath = 0;
        for (v = 2; v < 7; v++) {
            sf_cp_voice_off(v);
        }
        if (!voices[1].active || voices[1].macro_id != macro_bass) {
            sf_cp_voice_set(1, macro_bass, SF_CP_BASS_NOTE, 64);
        }
    }

    sf_cp_refresh_diag();
}

void cyber_paula_tick_50hz(GameState *s, uint32_t sample_rate)
{
    int16_t note;
    int v;

    (void)s; /* Reaktivitaet lebt in cyber_paula_update_from_game() */

    /* 1. 16-Step-Sequencer; nahtloser Live-Code-Swap am Zyklus-Boundary */
    step_in_cycle++;
    if (step_in_cycle >= SF_CP_STEPS_PER_CYCLE) {
        step_in_cycle = 0;
        cycle_num++;
        if (use_pattern_b) {
            memcpy(&pattern_a, &pattern_b, sizeof(StrudelPattern));
            use_pattern_b = 0;
        }
    }

    /* 2. Strudel-Query -> Lead-Stimme (voice 0).
     * query: >=0 Onset, -1 Rest, -2 Sustain (Stimme wird behalten). */
    if (!great_bath) {
        note = strudel_query_step(&pattern_a, cycle_num, step_in_cycle,
                                  SF_CP_STEPS_PER_CYCLE);
        if (note >= 0) {
            sf_cp_voice_set(0, 0, note, 64);   /* PWM-Lead, retrigger */
        } else if (note == -1) {
            sf_cp_voice_off(0);
        }
    }

    /* 3. Per-voice TFMX-Makro-Operationen (50Hz, pure Integer/16.16) */
    for (v = 0; v < SF_CP_VOICES; v++) {
        CyberPaulaVoice *vc = &voices[v];
        const TfmxMacroDef *m;
        int32_t pwm_shift;
        int8_t arp;

        if (!vc->active) continue;
        m = &frozen.macros[vc->macro_id & 15];

        pwm_shift = ((int32_t)vc->tick_50hz * (int32_t)m->pwm_speed) % 64;
        if (pwm_shift < 0) pwm_shift += 64;
        vc->loop_start = (uint32_t)((uint32_t)m->wave_offset + (uint32_t)pwm_shift)
                         & (SAMPLE_RAM_SIZE - 1);
        vc->loop_len   = m->wave_len;

        arp = m->arp_offsets[vc->tick_50hz % 3];
        vc->sample_inc_fp = sf_cp_sample_inc_fp(
                                (int32_t)vc->base_note + (int32_t)arp,
                                vc->loop_len, sample_rate);

        if (vc->tick_50hz > 0 && vc->volume > (int16_t)m->vol_decay) {
            vc->volume -= (int16_t)m->vol_decay;
        }
        vc->tick_50hz++;
    }

    sf_cp_refresh_diag();
}

void cyber_paula_set_pending_pattern(const StrudelPattern *p)
{
    if (!p) return;
    memcpy(&pattern_b, p, sizeof(StrudelPattern));
    use_pattern_b = 1;
}

int cyber_paula_load_strudel_file(const char *path)
{
    FILE *f;
    char buf[512];
    size_t n;
    int loaded = 0;

    f = fopen(path, "r");
    if (!f) return 0;

    if (fgets(buf, sizeof(buf), f)) {
        n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
            buf[--n] = '\0';
        }
        strudel_compile(&pattern_b, buf, 0);
        if (pattern_b.node_count > 1) {   /* Root allein = leer */
            use_pattern_b = 1;
            loaded = 1;
        }
    }
    fclose(f);
    return loaded;
}

const CyberPaulaDiagnostics *cyber_paula_diag(void)
{
    return &diag;
}

/* AGENT 5 (Amiga VBlank): Snapshot der 8 Mix-Voices im Kontrakt-Typ.
 * (frozen CyberPaulaVoice-Layout wird 1:1 in SF_AudioVoice kopiert) */
void cyber_paula_voices(SF_AudioVoice *out, int max)
{
    int v;
    if (out == 0 || max < 1) return;
    if (max > SF_CP_VOICES) max = SF_CP_VOICES;
    for (v = 0; v < max; v++) {
        const CyberPaulaVoice *vc = &voices[v];
        out[v].active        = vc->active;
        out[v].macro_id      = vc->macro_id;
        out[v].base_note     = vc->base_note;
        out[v].volume        = vc->volume;
        out[v].tick_50hz     = vc->tick_50hz;
        out[v].sample_inc_fp = vc->sample_inc_fp;
        out[v].loop_start    = vc->loop_start;
        out[v].loop_len      = vc->loop_len;
    }
}
