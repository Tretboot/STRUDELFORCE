# STRUDELFORCE — Phase 1/2 Contract (Orchestrator / Schwarmführer)

## Umgebung
- Sandbox: kein gcc/make, kein Raylib → C-Code wird hier NICHT kompiliert.
- Verifikation = (1) Code-Review-Agent, (2) Python-Twin des Audio-Kerns
  (tools/headless_verify.py → PHASE1_AUDIO_SELFTEST.txt),
  (3) Integrationstest src/test_phase1.c + make headless_test (Dev-Maschine).

## Dateieigentum (PARALLEL — kein Agent überschreibt fremde Dateien!)
| Agent | Darf erstellen |
|---|---|
| 1 core_sim | src/core_sim.c |
| 2 cyber_paula | src/cyber_paula.h, src/cyber_paula.c |
| 3 voxel_render | src/voxel_render.h, src/voxel_render.c, src/main_pc.c |
| 4 livecode/biomes | src/livecode_ui.h, src/livecode_ui.c, src/biomes.c |
| 5 amiga (Phase 2) | src/amiga_hw.c |
| Orchestrator | src/sf_interfaces.h, Makefile, src/test_phase1.c, tools/, specs/ |
FROZEN (read-only): strudel_cleanroom.h, tfmx_macro_synth.h, test_core_audio.c, assets/, user_music/.

## Regeln
1. C99 (-std=c99 -Wall -Wextra). core_sim.c: KEIN float/double, KEIN malloc.
2. voxel_render.c: raylib-frei (headless Render-Plan); Raylib nur in main_pc.c.
3. cyber_paula.c: Filter-Mathematik in fix16; 68k-sicher (int64 für Zwischensummen).
4. amiga_hw.c: #ifdef M68K echte HW-Register, #else Software-Stub mit gleicher API.
5. Alle Module beginnen mit #include "sf_interfaces.h"; API-Signaturen FIX.
6. 68k-Kompatibilität (Agents 1,2,4,5): keine VLAs, statische Pools, deterministisch.

## Phase-1.1 Review-Beschlüsse (dokumentiert)
- score & xp → int32_t (int16-Overflow)
- SIN8/COS8-Vertauschung im Velocity-Mapping behoben (vx←COS8, vy←SIN8)
- Bounce Stufe 3: {0°,45°,90°,270°} (Idx {0,1,2,6})
- wpn_lvl folgt dem Spieler-Level (max 5) bei Level-Up
- macro_bass-Reset im else-Branch (non-LASER/non-BOUNCE → PWM-Default)
