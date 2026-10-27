# STRUDELFORCE — handoff.md (Technischer Ist-Zustand)

> Für jede neue Sitzung: Dieser Blick reicht, um sofort weiterzuarbeiten.

## 1. Projekt-Layout (STRUDELFORCE_Smith_Core/)
```
STRUDELFORCE_Smith_Core/
├── todo.md                  # Priorisierte Arbeitspakete (Steuerdatei)
├── handoff.md               # Technischer Ist-Zustand (Steuerdatei)
├── captains_log.md          # Chronologisches Forschungstagebuch (Steuerdatei)
├── Makefile                 # Dual-Target (PC gcc / Amiga m68k-amigaos-gcc); Target `headless_test`
├── README_KIWI_ORCHESTRATOR.md   # Altsystem-Orchestrator-Spec (NUR Referenz, Multi-Agent-Theater ignorieren)
├── specs/                   # 5 Agent-Specs (Referenz-Doku)
│   ├── AGENT_1_CORE_GAMEPLAY.md
│   ├── AGENT_2_CYBER_PAULA.md
│   ├── AGENT_3_VOXEL_RENDER.md
│   ├── AGENT_4_LIVECODE_BIOMES.md
│   └── AGENT_5_AMIGA_BACKPORT.md
├── src/
│   ├── strudel_cleanroom.h  # Clean-Room Strudel/TidalCycles Mini-Notation Parser (C99, header-only)
│   ├── tfmx_macro_synth.h   # 50Hz TFMX-Macro-Engine + Paula-Simulator (C99, header-only)
│   └── test_core_audio.c    # Headless-Selbsttest (main())
├── assets/samples/          # 3 generierte 8-Bit Amiga-Samples (.wav, 16574 Hz)
│   ├── st01_analog_string.wav
│   ├── tfmx_pwm_bass.wav
│   └── turrican_laser_zap.wav
└── user_music/              # Beispiel-Strudel-Tracks für das ESC-Menü
    ├── sector1_storm.strudel
    └── the_great_bath_chest.strudel
├── BUILD.md                 # Kompilier-Anweisungen (gcc/make, Dual-Target, Sandbox-Workaround)
├── verify_headless.py       # Faithful Python-3-Port des Headless-Tests (Sandbox-Verifikation)
└── test_output_python.txt   # Eingefangener, verifizierter Test-Run (Referenz-Trace: ALLE OK)
```

**AUSLIEFERUNG:** `/mnt/data/STRUDELFORCE_Smith_Core.zip` (21 Dateien, ~49 KB, CRC-OK) — enthält den kompletten Stand inkl. C-Quellen, Makefile, Assets und Steuerdateien.

## 2. Bindende Schnittstellen-Verträge (Header-Verträge)

### strudel_cleanroom.h
- `StrudelFrac { int32_t num, den; }` — rationale Zyklus-Zeit.
- `StrudelNodeType` = { S_NODE_ATOM, S_NODE_SEQ, S_NODE_ALT, S_NODE_EUCLID }.
- `StrudelNode { type, midi_note(-1=Pause), instrument_id, repeat_mult, euclid_k, euclid_n, num_children, children[16] }`.
- `StrudelPattern { nodes[128], node_count, root_idx, raw_source[512] }`.
- `strudel_parse_note_token(const char*) -> int16_t` (MIDI; -1 = Pause).
- `strudel_compile(StrudelPattern*, const char* code, uint8_t default_inst)`.
- `strudel_query_step(const StrudelPattern*, uint32_t cycle_num, uint32_t step_in_cycle, uint32_t total_steps) -> int16_t`
  - Return: `>=0` = MIDI-Note, `-1` = Rest (`~`), `-2` = Sustain (`.`).

### tfmx_macro_synth.h
- `PAULA_VOICES = 8`, `SAMPLE_RAM_SIZE = 65536`.
- `TfmxOpType` = { END, SET_WAVE, MOD_BEGIN, ARP3, PITCH_SLIDE, ENV_DECAY }.
- `TfmxMacroDef { wave_offset, wave_len, pwm_speed, arp_offsets[3], pitch_slide, vol_start, vol_decay }`.
- `CyberPaulaVoice { active, macro_id, base_note, tick_50hz, sample_pos_fp, sample_inc_fp, loop_start, loop_len, volume }`.
- `CyberPaulaEngine { sample_ram[65536], macros[16], voices[8], cutoff_hz, resonance_q, svf_low, svf_band }`.
- `cyber_paula_init_waves(CyberPaulaEngine*)` — init 8-Bit Sample-RAM (Bank0 Sägezahn/Puls, Bank1 FM-Slap, Bank2 D-50 String) + 3 Macros.
- `cyber_paula_tick_50hz(CyberPaulaEngine*, uint32_t sample_rate)` — 50Hz VBlank-Logik (PWM, Arp, Decay).

## 3. Build-Befehle
- **Headless-Test (PC, ohne Raylib):** `make headless_test && ./strudelforce_test`
  - Kompiliert `src/test_core_audio.c` mit `gcc -std=c99 -Wall -Wextra -O2 -Isrc -lm`.
- **Cleanup:** `make clean`.
- **Dual-Target:** PC via gcc (Raylib-Frontend separat); Amiga via `m68k-amigaos-gcc` (Backport, Phase 2+).

## 4. Kritisches Umgebungs-Gap (STOLPERSTEIN)
- **Diese Sandbox hat KEINE C-Toolchain:** kein `gcc`, `cc`, `make`, `tcc`, `clang`, keine Binutils, **kein Netzwerk** (kein apt/Download).
- → `make headless_test` / `make game` laufen hier NICHT. C-Build = Deliverable für echte Maschinen (s. BUILD.md).
- **Workaround:** Python-Oracle `verify_core_sim.py` (1:1-Port des Gameplay-Cores) + `verify_headless.py` (Audio-Parser) liefern das Headless-Test-Output inkl. Determinismus- + Invarianten-Checks.
- **Persistenz-Gap (Iteration 3 präzisiert, gemessene Matrix):**
  - persistiert: `.py .c .md .txt .zip .sh` · flüchtig: `.h Makefile .wav .strudel` (egal ob create_file oder bash-cp)
  - Subagent-Dateien erreichen die Captain-Sandbox **nie**; Subagenten sehen nur einen Snapshot (keine create_file-Outputs desselben Turns).
  - **Recovery-Protokoll (BINDEND):** `python3 /mnt/data/smith_bootstrap.py` regeneriert alle flüchtigen Dateien (core_sim.h, game_audio.h, Makefile, .strudel, .wav) + SHA256-Check + `--zip`. **Davor:** `python3 /mnt/data/gemini-code-1793055820039.py` + `cp STRUDELFORCE_Kit/src/*.h STRUDELFORCE_Smith_Core/src/` (Audio-Header). SHA-Baselines: core_sim.h `d0e16536`, core_sim.c `ecf32163`, game_main.c `4cb7c614`, game_audio.c `f38c8932` (Iteration 4).
  - **Regel:** Makefile-Änderungen immer in die `MAKEFILE`-Konstante von smith_bootstrap.py pflegen (nie nur in die Datei).
  - **Regel:** ZIP immer per `smith_bootstrap.py --zip` in derselben Session wie die Recovery (inkl. Audio-Header-cp).
  - **Regel:** Subagent-Datei-Output NICHT vertrauenswürdig; Abnahme immer mit eigenem `find`/`sha256sum`.

## 5. Exakter nächster Schritt (Iteration 5)
1. Recovery (s. §4) — inkl. Audio-Header-cp aus dem Original-Bootstrap.
2. **Meilenstein 4 (Wahl):** (a) Voxel-Renderer `src/voxel_render.c` (AGENT_3-Spec: 2D-Farb-Matrizen + z_depth → extrudierte Voxel, 4 Parallax-Layer, Voxel-Shatter) ODER (b) ESC-Live-Coding-Overlay `src/livecode_ui.c` (AGENT_4-Spec: Strudel-Editor, CTRL+ENTER Live-Compile, F2/F3 Save/Load).
3. Oracle-Erweiterung, ZIP neu packen (IMMER mit Essay), captains_log.md Iteration 5.

## 6. Referenz-Trace des Headless-Tests (für Verifikation)
Pattern: `c2 [eb2 g2] c3*2 <bb2 g2(3,8)>`
- AST-Knoten: **9**
- Cycle 0: `[36] . . . [39] . [43] . [48] . [48] . [46] . . .`
- Cycle 1: `[36] . . . [39] . [43] . [48] . [48] . [43] ~ ~ [43]`
- (`[N]`=Note, `~`=Rest, `.`=Sustain)
