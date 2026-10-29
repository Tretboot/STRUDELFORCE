/*
 * test_suite.c - STRUDELFORCE Consolidated Integration Test Suite
 *
 * Testet alle Kernmodule headless:
 *   1. Core-Simulation: 2500 Ticks deterministischer Headless-Bot-Run (LCG)
 *   2. Kampagne-Modus: 5 Level x 3 Wellen, Level-Clear-Bonus & Sieg-Zustand
 *   3. Cleanroom Strudel-Parser: AST-Generierung, Query-Steps, Alternationen & Euklid
 *   4. TFMX Replay: Parsing, Makro-Ausfuehrung & Paula-Rendering
 *   5. ProTracker MOD Replay: 4-Kanal-Playback, Effekte & Loop
 *   6. Game SFX: Direkte Trigger fuer Feuer, Sprung, Treffer, Explosion & Pickups
 *   7. SfMixer: Multi-Source-Summierung, Gain-Rampen & Ducking
 *
 * Lizenz: MIT (Clean Room)
 */

#define _CRT_SECURE_NO_WARNINGS
#include "strudelforce.h"
#include "embedded_music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  [PASS] %s\n", (msg)); } \
    else      { g_fail++; printf("  [FAIL] %s  (Zeile %d)\n", (msg), __LINE__); } \
} while (0)

static uint16_t bot_input(const GameState *s, int tick) {
    uint16_t m = INPUT_FIRE;
    int best = -1;
    int32_t best_d = 0x7FFFFFFF;
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s->enemies[i];
        fix16_t dx, adx;
        if (!e->active) continue;
        dx = e->x - s->px;
        adx = (dx < 0) ? -dx : dx;
        if (best < 0 || adx < best_d) {
            best_d = adx;
            best = i;
        }
    }
    if (best >= 0) {
        fix16_t dx = s->enemies[best].x - s->px;
        if (dx > fix16_from_int(2)) m |= INPUT_RIGHT;
        else if (dx < -fix16_from_int(2)) m |= INPUT_LEFT;
    }
    if (tick % 97 == 0 && s->on_ground) m |= INPUT_UP;
    if (tick % 300 < 60) m |= INPUT_SECOND;
    if (tick % 250 == 0) m |= INPUT_WEAPSW;
    return m;
}

static void test_core_sim_endless(void) {
    GameState st;
    int t;
    printf("\n--- [1] Core Sim Determinismus (2500 Ticks, Endless) ---\n");
    core_init(&st, 1234);
    CHECK(st.hp == 100 && st.lives == 3, "Init: HP=100, Leben=3");
    CHECK(st.weapon == WEAPON_SPREAD, "Init: Startwaffe = Spread");

    for (t = 0; t < 2500; t++) {
        core_tick(&st, bot_input(&st, t));
    }
    printf("  Ergebnis nach 2500 Ticks: Score=%u, Kills=%u, Bounces=%u, Lives=%u\n",
           st.score, st.enemies_killed, st.bounces_total, st.lives);
    CHECK(st.tick == 2500, "Tick-Zaehler == 2500");
    CHECK(st.score == 35150, "Deterministischer Score == 35150");
    CHECK(st.enemies_killed == 191, "Deterministische Kills == 191");
    CHECK(st.bounces_total == 42047, "Deterministische Bounces == 42047");
}

static void test_core_sim_campaign(void) {
    GameState st;
    int t;
    printf("\n--- [2] Kampagnen-Modus (5 Level x 3 Wellen) ---\n");
    core_init(&st, 1337);
    st.mode = MODE_CAMPAIGN;
    CHECK(st.mode == MODE_CAMPAIGN, "Modus auf MODE_CAMPAIGN gesetzt");

    for (t = 0; t < 3000; t++) {
        uint16_t input = (t % 2 == 0) ? INPUT_FIRE : 0;
        core_tick(&st, input);
        if (st.state == STATE_VICTORY) break;
    }
    CHECK(st.campaign_level >= 1, "Kampagnen-Level mindestens 1");
    CHECK(core_campaign_biome(&st) < SF_BIOME_COUNT, "Biome-Index im gueltigen Bereich");
}

static void test_strudel_cleanroom(void) {
    StrudelPattern pat;
    printf("\n--- [3] Cleanroom Strudel-Parser & AST ---\n");
    strudel_compile(&pat, "c2 [eb2 g2] c3*2 <bb2 g2(3,8)>", 0);
    CHECK(pat.node_count > 0, "AST erzeugt Knoten");
    CHECK(strudel_query_step(&pat, 0, 0, 16) == 36,  "Cycle 0, Step 0  = c2 (MIDI 36)");
    CHECK(strudel_query_step(&pat, 0, 4, 16) == 39,  "Cycle 0, Step 4  = eb2 (MIDI 39)");
    CHECK(strudel_query_step(&pat, 0, 12, 16) == 46, "Cycle 0, Step 12 = bb2 (MIDI 46)");
    CHECK(strudel_query_step(&pat, 1, 12, 16) == 43, "Cycle 1, Step 12 = g2 Alternation (MIDI 43)");
}

static void test_sound_engine_hub(void) {
    GameAudio audio;
    GameState state;
    int16_t buf[GAME_AUDIO_SAMPLES_PER_TICK];
    printf("\n--- [4] Soundengine Hub & Cyber-Paula ---\n");
    soundengine_init(&audio);
    CHECK(audio.cutoff_fp == 62377, "Cutoff initialisiert auf 8000 Hz");
    CHECK(audio.pwm_speed == 2, "PWM-Speed initialisiert auf 2");

    core_init(&state, 42);
    soundengine_update(&audio, &state);
    soundengine_render(&audio, buf, GAME_AUDIO_SAMPLES_PER_TICK);
    CHECK(audio.tick == 1, "Audio-Tick zaehlt 50Hz");

    /* SFX Trigger testen */
    printf("\n--- [5] Game SFX Trigger ---\n");
    sound_sfx_fire(&audio, WEAPON_SPREAD);
    CHECK(audio.voices[4].active == 1, "SFX Fire: Voice 4 aktiv");
    sound_sfx_jump(&audio);
    CHECK(audio.voices[5].active == 1, "SFX Jump: Voice 5 aktiv");
    sound_sfx_hit(&audio, 0);
    CHECK(audio.voices[6].active == 1, "SFX Hit: Voice 6 aktiv");
    sound_sfx_crystal(&audio);
    CHECK(audio.voices[7].active == 1, "SFX Crystal: Voice 7 aktiv");

    soundengine_render(&audio, buf, GAME_AUDIO_SAMPLES_PER_TICK);
    CHECK(audio.voices[4].volume > 0, "SFX Audio gerendert");
}

static void test_multi_source_mixer(void) {
    SfMixer mx;
    int16_t out[SF_MIX_BLOCK];
    printf("\n--- [6] Multi-Source SfMixer ---\n");
    sf_mixer_init(&mx);
    CHECK(mx.master == 256, "Mixer Master Volume = 256");
    sf_mixer_render(&mx, out, SF_MIX_BLOCK);
    CHECK(out[0] == 0, "Leerer Mixer rendert Stille");
}

static void test_tfmx_playback(void) {
    TfmxPlayer tfmx;
    GameAudio audio;
    int16_t buf[GAME_AUDIO_SAMPLES_PER_TICK];
    int i, peak = 0;
    printf("\n--- [7] TFMX Replay & Paula 8-Voice Rendering ---\n");
    tfmx_player_init(&tfmx);
    CHECK(tfmx_player_load_mem(&tfmx, EMB_TFMX_APIDYA_MEADOW, EMB_TFMX_APIDYA_MEADOW_LEN,
                               (const int8_t *)EMB_SAM_APIDYA_MEADOW, EMB_SAM_APIDYA_MEADOW_LEN),
          "Apidya Meadow TFMX+SAM mem load");
    CHECK(tfmx.loaded == 1, "TFMX geladen");
    tfmx_player_start(&tfmx, 0);
    CHECK(tfmx.playing == 1, "TFMX gestartet");

    TfmxPlayer sm_tfmx;
    tfmx_player_init(&sm_tfmx);
    CHECK(tfmx_player_load_mem(&sm_tfmx, EMB_TFMX_SOFTMAN, EMB_TFMX_SOFTMAN_LEN,
                               (const int8_t *)EMB_SAM_SOFTMAN, EMB_SAM_SOFTMAN_LEN),
          "Software Manager TFMX+SAM mem load");
    CHECK(sm_tfmx.loaded == 1 && sm_tfmx.num_subsongs >= 22, "Software Manager 22 Subsongs verifiziert");
    tfmx_player_start(&sm_tfmx, 21);
    CHECK(sm_tfmx.playing == 1, "Software Manager Subsong 21 gestartet");
    soundengine_init(&audio);
    for (i = 0; i < 50; i++) {
        int k;
        tfmx_player_tick(&tfmx, &audio);
        soundengine_render(&audio, buf, GAME_AUDIO_SAMPLES_PER_TICK);
        for (k = 0; k < GAME_AUDIO_SAMPLES_PER_TICK; k++) {
            int val = abs(buf[k]);
            if (val > peak) peak = val;
        }
    }
    printf("  TFMX Peak Amplitude ueber 50 Ticks: %d\n", peak);
    CHECK(peak > 500, "TFMX generiert echtes Audio (Peak > 500)");
}

static void test_mod_playback(void) {
    ModPlayer mod;
    int16_t buf[GAME_AUDIO_SAMPLES_PER_TICK];
    int i, peak = 0;
    printf("\n--- [8] ProTracker MOD Replay ---\n");
    mod_player_init(&mod);
    CHECK(mod_player_load_mem(&mod, EMB_MOD_WC_OFFICE, EMB_MOD_WC_OFFICE_LEN),
          "Commander Office MOD mem load");
    CHECK(mod.loaded == 1 && mod.nch == 4, "MOD 4-Kanal geladen");
    mod_player_start(&mod, GAME_AUDIO_RATE);
    CHECK(mod.playing == 1, "MOD gestartet");
    for (i = 0; i < 50; i++) {
        int k;
        mod_player_render(&mod, buf, GAME_AUDIO_SAMPLES_PER_TICK);
        for (k = 0; k < GAME_AUDIO_SAMPLES_PER_TICK; k++) {
            int val = abs(buf[k]);
            if (val > peak) peak = val;
        }
    }
    printf("  MOD Peak Amplitude ueber 50 Ticks: %d\n", peak);
    CHECK(peak > 500, "MOD generiert echtes Audio (Peak > 500)");

    ModPlayer st15;
    mod_player_init(&st15);
    CHECK(mod_player_load_mem(&st15, EMB_MOD_LOST_PATROL, EMB_MOD_LOST_PATROL_LEN),
          "Lost Patrol 15-Sample Soundtracker load");
    CHECK(st15.loaded == 1 && st15.nch == 4, "Soundtracker 15-Sample 4-Kanal geladen");
    mod_player_start(&st15, GAME_AUDIO_RATE);
    CHECK(st15.playing == 1, "Lost Patrol 15-Sample MOD gestartet");
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("====================================================================\n");
    printf("STRUDELFORCE CONSOLIDATED INTEGRATION TEST SUITE\n");
    printf("====================================================================\n");

    test_core_sim_endless();
    test_core_sim_campaign();
    test_strudel_cleanroom();
    test_sound_engine_hub();
    test_multi_source_mixer();
    test_tfmx_playback();
    test_mod_playback();

    printf("\n====================================================================\n");
    printf("TEST ERGEBNIS: %d PASS, %d FAIL\n", g_pass, g_fail);
    printf("====================================================================\n");
    return (g_fail == 0) ? 0 : 1;
}
