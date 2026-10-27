/*
 * test_phase1.c — STRUDELFORCE Phase-1 Integrationstest (headless, kein Raylib)
 * Owner: ORCHESTRATOR. Start: make headless_test && ./strudelforce_test
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h> /* unlink() */
#include "sf_interfaces.h"

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  [PASS] %s\n", (msg)); } \
    else      { g_fail++; printf("  [FAIL] %s  (Zeile %d)\n", (msg), __LINE__); } \
} while (0)

static int count_active(const SFEntity *pool, int n) {
    int c = 0;
    for (int i = 0; i < n; i++) if (pool[i].active) c++;
    return c;
}

int main(void) {
    printf("=== STRUDELFORCE Phase-1 Integrationstest (headless) ===\n\n");

    printf("[1] Clean-Room Strudel-Parser (frozen)\n");
    StrudelPattern pat;
    strudel_compile(&pat, "c2 [eb2 g2] c3*2 <bb2 g2(3,8)>", 0);
    CHECK(pat.node_count > 0, "AST erzeugt Knoten");
    CHECK(strudel_query_step(&pat, 0, 0, 16) == 36,  "C0S0 = c2  (MIDI 36)");
    CHECK(strudel_query_step(&pat, 0, 4, 16) == 39,  "C0S4 = eb2 (MIDI 39)");
    CHECK(strudel_query_step(&pat, 0, 12, 16) == 46, "C0S12 = bb2 (MIDI 46)");
    CHECK(strudel_query_step(&pat, 1, 12, 16) == 43, "C1S12 = g2(3,8) Euklid-Treffer (MIDI 43)");

    printf("[2] Core Gameplay (Agent 1, core_sim.c)\n");
    CHECK(sf_laser_widths[0] == 2 && sf_laser_widths[4] == 22, "Laser-Breitentabelle {2,4,8,14,22}");
    CHECK(sf_spread_counts[0] == 1 && sf_spread_counts[4] == 7, "Spread-Zahltabelle {1,2,3,5,7}");

    GameState st;
    core_init(&st, SF_BIOME_CHROME_TEMPEST, 0xC0FFEE);
    CHECK(st.player.hp == 100, "Init: HP = 100");
    CHECK(st.player.wpn_lvl == 1, "Init: Waffen-Stufe 1");
    CHECK(st.player.y >= SF_GROUND_Y - sf_fix(1), "Init: Player an Ground-Line (fix16)");
    for (int t = 0; t < 50; t++) core_tick(&st, 0);
    CHECK(st.tick == 50, "50 Ticks -> tick == 50");

    for (int t = 0; t < 3; t++) core_tick(&st, SF_INPUT_FIRE);
    CHECK(count_active(st.bullets, SF_MAX_BULLETS) > 0, "FIRE (MULTIPLE) erzeugt Projektile");

    core_tick(&st, SF_INPUT_WPN_NEXT);
    core_tick(&st, SF_INPUT_FIRE);
    int laser = 0, lw_ok = 0;
    for (int i = 0; i < SF_MAX_BULLETS; i++)
        if (st.bullets[i].active && st.bullets[i].etype == SF_ETYPE_LASER) {
            laser++;
            if (st.bullets[i].w == sf_fix(2)) lw_ok++;
        }
    CHECK(laser > 0, "WPN_NEXT -> LASER feuert Balken-Entity");
    CHECK(lw_ok > 0, "LASER Stufe 1: Breite = 2 px (fix16)");

    core_tick(&st, SF_INPUT_WPN_NEXT);
    core_tick(&st, SF_INPUT_FIRE);
    int bounce = 0;
    for (int i = 0; i < SF_MAX_BULLETS; i++)
        if (st.bullets[i].active && st.bullets[i].wpn == SF_WPN_BOUNCE && st.bullets[i].bounces >= 1)
            bounce++;
    CHECK(bounce > 0, "Bounce 2.0: Projektile mit >= 1 Abpraller");

    core_tick(&st, SF_INPUT_WPN_NEXT);
    core_tick(&st, SF_INPUT_SECONDARY);
    int surround = 0;
    for (int i = 0; i < SF_MAX_BULLETS; i++)
        if (st.bullets[i].active && st.bullets[i].wpn == SF_WPN_SURROUND)
            surround++;
    CHECK(surround > 0, "SECONDARY -> 360-Grad Surround-Beam aktiv");

    core_tick(&st, SF_INPUT_WPN_NEXT);
    core_tick(&st, SF_INPUT_MORPH);
    CHECK(st.player.morph == 1, "MORPH schaltet 8x8-Hitbox-Modus");

    printf("[3] Cyber-Paula reaktive Patch-Matrix (Agent 2)\n");
    cyber_paula_init();
    st.player.weapon = SF_WPN_LASER;  st.player.wpn_lvl = 5;
    cyber_paula_update_from_game(&st);
    CHECK(cyber_paula_diag()->macro_bass == 0, "LASER -> Bass-Macro PWM (0)");
    CHECK(cyber_paula_diag()->sub_bass_up == 1, "LASER -> Sub-Bass +4 dB");
    st.player.weapon = SF_WPN_BOUNCE;
    cyber_paula_update_from_game(&st);
    CHECK(cyber_paula_diag()->macro_bass == 1, "BOUNCE -> Bass-Macro FM-Pluck (1)");
    st.player.shield = 100;
    cyber_paula_update_from_game(&st);
    CHECK(cyber_paula_diag()->q_fp >= sf_fix(8), "Schild 100% -> Resonanz Q >= 8 (von 0.7 auf ~8.5)");
    st.player.hp = 10; st.player.shield = 0;
    cyber_paula_update_from_game(&st);
    CHECK(cyber_paula_diag()->low_hp_mode == 1, "HP < 25% -> Low-HP-Kette aktiv");
    CHECK(cyber_paula_diag()->cutoff_fp <= sf_fix(600), "Low-HP -> Cutoff <= ~550 Hz (Toleranz 600)");
    CHECK(cyber_paula_diag()->quant_drums == 1, "Low-HP -> 4-Bit-Quantisierung Drums");
    st.chest_opened = 1; st.player.hp = 100;
    cyber_paula_update_from_game(&st);
    CHECK(cyber_paula_diag()->great_bath == 1, "Schatztruhe -> Great-Bath-Tribute-Modus");
    CHECK(cyber_paula_diag()->active_voices >= 7, "Great-Bath -> >= 7 aktive Stimmen");

    printf("[4] 2.5D-Voxel-Render-Plan (Agent 3)\n");
    voxel_init();
    VoxelRenderItem items[SF_VOXEL_MAX_ITEMS];
    uint16_t n = voxel_build_render_plan(&st, items, SF_VOXEL_MAX_ITEMS);
    int l0 = 0, l1 = 0, l2 = 0, l3 = 0, player_item = 0, z_ok = 0;
    for (uint16_t i = 0; i < n; i++) {
        switch (items[i].layer) { case 0: l0++; break; case 1: l1++; break; case 2: l2++; break; default: l3++; }
        if (items[i].layer == 1 && items[i].kind == 1) player_item++;
        if ((items[i].layer == 2 && items[i].z == sf_fix(-6)) ||
            (items[i].layer == 3 && items[i].z == sf_fix(-20))) z_ok++;
    }
    CHECK(n > 0, "Render-Plan nicht leer");
    CHECK(l0 > 0, "Layer 0 (Vordergrund, Z=+2) vorhanden");
    CHECK(l1 > 0 && player_item > 0, "Layer 1 (Gameplay, Z=0) enthaelt Player-Sprite");
    CHECK(l2 > 0, "Layer 2 (Mid-Background-Voxel, Z=-6) vorhanden");
    CHECK(l3 > 0, "Layer 3 (Deep-Sky/Copper, Z=-20) vorhanden");
    CHECK(z_ok > 0, "Layer-Z-Werte in fix16 korrekt (-6 / -20)");

    printf("[5] ESC Live-Coding & Biome-Generator (Agent 4)\n");
    LiveCodeUI ui;
    livecode_init(&ui, "c2 [eb2 g2]");
    CHECK(strncmp(livecode_get_source(&ui), "c2 [eb2 g2]", 10) == 0, "Init-Source im Editor");
    CHECK(livecode_compile_swap(&ui) == 1, "CTRL+ENTER: valides Pattern -> swap_pending");
    CHECK(livecode_save(&ui, "user_music/_phase1_test.strudel") == 1, "F2: Pattern als .strudel speichern");
    CHECK(livecode_load(&ui, "user_music/_phase1_test.strudel") == 1, "F3: BYO-Datei laden");
    unlink("user_music/_phase1_test.strudel");

    LiveCodeUI bad;
    livecode_init(&bad, "   ");
    CHECK(livecode_compile_swap(&bad) == 0, "Leer-Source -> KEIN Swap (Return 0)");

    char names[8][64];
    int cnt = 0;
    livecode_list_user_music(names, 8, &cnt);
    CHECK(cnt >= 2, "user_music/ Liste >= 2 Dateien (F3-Dialog)");

    CHECK(sf_biomedefs[SF_BIOME_GREAT_BATH].underwater == 1, "Great Bath = Unterwasser (HW-Tiefpass $BFE001)");
    CHECK(sf_biomedefs[0].music_file != 0, "Biome-Tabelle: Musik-Dateien referenziert");

    uint32_t s1 = 42, s2 = 42;
    int16_t b1, b2, c1, c2;
    biome_next_chunk(&s1, &b1, &c1);
    biome_next_chunk(&s2, &b2, &c2);
    CHECK(b1 == b2 && c1 == c2, "Endlos-Generator deterministisch (gleiche Seed)");
    CHECK(b1 >= 0 && b1 < SF_BIOME_COUNT, "Biome-ID gueltig");
    CHECK((b1 == SF_BIOME_GREAT_BATH) == (c1 != 0), "Schatztruhe nur im Great-Bath-Chunk");

    printf("\n=== ERGEBNIS: %d PASS / %d FAIL ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
