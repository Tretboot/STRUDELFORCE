# STRUDELFORCE — Handoff Phase 1 + 1.1 (Schwarmführer → nächster Operator)

**Stand:** Phase 1 (spielbarer 2.5D-Voxel-Prototyp) + Phase 1.1
(Code-Review-Runde, alle Fixes eingezogen) + **Agent 5 delivered**
(`src/amiga_hw.c`, Amiga-Backport, im PC-Build als Stub verlinkbar).
**Verifiziert:** Python-Twin-Selbsttest = PASS (PHASE1_AUDIO_SELFTEST.txt).
**Nicht verifiziert (Dev-Maschine):** `make headless_test` / `pc_game` /
`amiga_core` (Sandbox hat keinen gcc/m68k-amigaos-gcc).

## Schnellstart
    make headless_test && ./strudelforce_test   # Phase-1-Integrationstest (30 Checks)
    make audio_core   && ./strudelforce_audio   # Frorener Audio-Core-Selbsttest
    make pc_game      && ./strudelforce_pc      # Raylib-Frontend (braucht Raylib)
    make amiga_core   && ./strudelforce_amiga   # Amiga 68k (CC=m68k-amigaos-gcc)
    python3 tools/headless_verify.py            # Audio-Kern ohne gcc (Sandbox-Pfad)

## Dateibaum & Verantwortung
| Datei | Owner | Inhalt |
|---|---|---|
| src/sf_interfaces.h | ORCHESTRATOR | Vertrag v3: fix16, GameState (int32 score/xp), **SF_AudioVoice**, amiga_hw-API (READ-ONLY) |
| src/core_sim.c | AGENT 1 | 50Hz-Fixed-Point-Kern. **Phase-1.1:** SIN/COS-Vertauschung behoben, Bounce St.3={0,45,90,270}, wpn_lvl folgt Level (cap 5) |
| src/cyber_paula.{h,c} | AGENT 2 | 16-Step-Sequencer + 8-Stimmen-TFMX-Mixer. **Phase-1.1:** `cyber_paula_voices(SF_AudioVoice*)` für Agent 5, macro_bass-Reset |
| src/voxel_render.{h,c} | AGENT 3 | 4-Layer-3D-Parallax-Plan (Z=−20/−6/0/+2), 6 Sprite-Extrusionen, Interlace-Jitter |
| src/main_pc.c | AGENT 3 | Raylib 1280×720: 50Hz-Accumulator, Draw3D-Säulen, HUD, ESC-Overlay, Endlos-Biome |
| src/livecode_ui.{h,c} | AGENT 4 | ESC→SOUND/MUSIK: 512-B-Editor, CTRL+ENTER=Live-Swap, F2=Save, F3=BYO-Listing |
| src/biomes.c | AGENT 4 | 3 Biome + deterministischer Chunk-Generator (Great Bath: Truhe + underwater) |
| src/amiga_hw.c | **AGENT 5 (Phase 2)** | 6-Planes 320×256 (120KB Chip), Copper RGB10, Paula-8-Voices aus SF_AudioVoice (PER=5319227/inc), INTB-VBlank, Paddle-Input, **$BFE001 HW-Tiefpass**, Sample-Playback. `#ifdef M68K` real / `#else` PC-Stub. RAM ~226KB < 512KB |
| src/test_phase1.c | ORCHESTRATOR | 30-Check-Integrationstest (Target: headless_test) |
| src/strudel_cleanroom.h | FROZEN | Clean-Room-Strudel-Parser (1 Patch: `<stdio.h>`, dokumentiert) |
| src/tfmx_macro_synth.h | FROZEN | TFMX-50Hz-Makrosynthese, 64KB 8-Bit-Sample-RAM, 3 Wave-Bänke |
| Makefile | ORCHESTRATOR | headless_test / audio_core / pc_game / **amiga_core (-DM68K)** |
| tools/headless_verify.py | ORCHESTRATOR | Python-Twin des Audio-Selbsttests |
| specs/ | — | 5 Agent-Specs + PHASE1_CONTRACT.md (inkl. Phase-1.1-Beschlüsse) |
| user_music/ | BYO | 2 Beispiel-Strudel (F3-Pipeline) |
| assets/samples/ | — | 8-Bit Amiga-Beispiel-Samples (.wav) |

## Phase-1.1 Review-Beschlüsse (Code-Review-Agent, eingezogen)
1. **CRITICAL (Core):** `score`/`xp` int16→int32 (int16 wrapte nach ~219 Kills).
2. **CRITICAL (Core):** SIN8/COS8 im Velocity-Mapping vertauscht → vx←COS8,
   vy←SIN8 (Diagonalen waren gespiegelt).
3. **DESIGN (Core):** Bounce Stufe 3 = {0°,45°,90°,270°} (Idx {0,1,2,6});
   wpn_lvl steigt mit dem Spieler-Level (cap 5, Tabellen-Index-Schutz).
4. **DESIGN (Paula):** `macro_bass`-Reset im else-Branch.
5. **ARCH (Agent 5):** Stimmen-Access über Kontrakt-Typ `SF_AudioVoice`
   (Snapshot) — Agent 5 sieht nie den frozen tfmx-Typ, Sandwich-Rename
   in cyber_paula.c bleibt unangetastet.
6. **OK (akzeptiert):** CD-Reihenfolge, Bounce-Clamp, Kill-im-Loop,
   sample-inc Guards, Q*10-Semantik.

## Offene HW-Risiken (Agent 5, für Dev-Maschine / Phase 2a)
1. **Paula-RAM-Layout:** eigener 32KB-Sample-Buffer vs. 64KB-Sample-RAM der
   frozen Engine — Adresse/Kollision prüfen bei gleichzeitiger Aktivität.
2. **68k-Interrupt-Prolog:** gegen echten Bebbo-Toolchain-Codegen
   verifizieren; Busy-Wait → WaitTO produktiv.
3. **Paula-Periode:** Konstante 5319227 und PER=81 (44.1 kHz) auf echter
   HW gegen hörbare Tonhöhe checken.

## Nächste Schritte
1. **Compiler-Gate:** `make headless_test` auf der Dev-Maschine.
2. `make pc_game` mit Raylib (optional: TFMX-RAM per Python nach .wav).
3. **Phase 2a:** Amiga-Hardware-Verifikation (1–3) mit m68k-amigaos-gcc.
4. Optional: Agent-5-Review nach dem ersten echten 68k-Lauf.

## Sandbox-Instabilität (Wichtig für die nächste Session)
Die Entwicklungsumgebung löscht zwischen Turns wiederholt Dateien
(selektiv: `.h`-Header, `Makefile`, `tools/`, Verzeichnisse — `.c` und
`.md` überleben meist). Dieser Zip wurde in EINEM atomaren Lauf erzeugt,
verifiziert (Twin: PASS) und verpackt und ist die **Single Source of
Truth**. Recovery: `python3 gemini-code-1793055820039.py` regeneriert die
frozen Basis; die Phase-1.1-Dateien (alle .c, dieses HANDOFF, specs)
überleben in dieser Zip.

## Essay & Beschreibung
- `HANDOFF_DOCS/DESCRIPTION.md` — Projektüberblick (Features, Tech-Stack, Lizenz)
- `HANDOFF_DOCS/ESSAY.md` — Arbeitsvorgang & 9 Lehren (inkl. Phase-1.1:
  *Review ist der billigste Fehlerfinder — er fand den Winkelkonventions-
  Blindspot, den der gesamte Swarm geteilt hatte*)
