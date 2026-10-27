# STRUDELFORCE – Master Workspace & Agent Briefing

Willkommen im Hauptverzeichnis `STRUDELFORCE_Master`. Dieses Dokument dient als zentrale Systemarchitektur- und Übergabedokumentation für alle zukünftigen Entwicklungs- und KI-Sitzungen.

---

## 1. Projektstruktur (post-Merge 2026-10-27: EIN Projekt)

Dieses Projekt entstand aus einem Multi-Agenten-Experiment mit zwei parallelen Stämmen
("Schwarm"/KIWI-Zweig A und "Smith Prime"/Zweig B) plus einem KIWI-Cloud-Run (v3-Audio-Stack).
**Am 2026-10-27 wurden beide Zweige in die Workspace-Root mergiert** (Single Source of Truth,
Dual-Core-Architektur — Details in `MERGE_NOTES.md`). Die Original-Zweig-Ordner liegen
unverändert in `archive/branches/`:

### Zweig A: `STRUDELFORCE/` (Der "Handoff Phase 1 / Schwarm"-Zweig)
- **Architektur & Module:** Beinhaltet die fünf Kernmodule nach Spezifikation:
  - `src/core_sim.c`: 50Hz Fixed-Point Physik & Gameplay-Simulation (Agent 1). Enthält den entscheidenden Phase-1.1 **SIN8/COS8-Bugfix** (Korrektur gespiegelter Diagonal-Geschwindigkeiten: `vx = speed * COS8`, `vy = speed * SIN8`), 4-Wege Bounce Stufe 3 (`{0°, 45°, 90°, 270°}`) sowie `int32_t` Score/XP gegen Wrap-Around.
  - `src/cyber_paula.c`: 16-Step-Sequencer + 8-Stimmen-TFMX-Mixer mit 64KB Sample-RAM (Agent 2).
  - `src/voxel_render.c`: 4-Layer-2.5D-Voxel-Render-Planer mit directionalem Face-Shading und Interlace-Jitter (Agent 3).
  - `src/livecode_ui.c`: ESC-Live-Coding-Editor (512 Byte Puffer, F2 Save, F3 Listing, Strudel Cleanroom AST) (Agent 4).
  - `src/biomes.c`: 3 Biome (Chrome Tempest, The Great Bath, Mechanical Hive) + deterministischer Chunk-Generator (Agent 4).
  - `src/amiga_hw.c`: Amiga OCS/AGA 68k-Hardware-Backport (6 Bitplanes, Copper-Listen, Paula 8-Voice DMA, CIA-A $BFE001 Tiefpass-Filter, PC-Stub) (Agent 5).
  - `src/main_pc.c`: Raylib-basiertes 2.5D PC-Frontend (1280x720 @ 60 FPS, 50Hz PAL Accumulator).
  - `src/test_phase1.c`: Headless 30-Check-Integrationstest aller Subsysteme.

### Zweig B: `STRUDELFORCE_Smith_Core/` (Der "Smith Prime / Meeseeks"-Zweig)
- **Fokus & Stärken:**
  1. **Sofort spielbares 50Hz-Terminal-Frontend (`src/game_main.c`):** Funktioniert vollkommen autark ohne externe Grafikbibliotheken sowohl interaktiv als auch im Headless-Bot-Modus (`--demo 2500`).
  2. **16-Bit WAV-Audio-Renderer (`src/game_audio.c`):** Echter PCM-Export (`--audio out.wav`) mit Paula-Emulation, TFMX-Macros und 1-Pole Low-Pass Filter.
  3. **Vollständige Python-Oracles & Dokumentation:** `verify_core_sim.py`, `verify_audio_matrix.py`, `verify_headless.py`, `WORKFLOW_ESSAY.md`, `captains_log.md`.
  4. **Meilenstein 4 / Iteration 5:** Anbindung der ESC-Live-Coding-Konsole (`livecode_ui.h/.c`) mit nahtlosem Pattern-Swap am Zyklus-Rand, Great-Bath-Truhen-Tribute und Bassline-Override (`bass_override_active`, `bass_override_note`).

### Root-Tools im Workspace:
- `kiwi_toolkit.py` & `kiwi_toolkit_multifolder.py`: Zero-GCC Syntax-Linter, Iteration-5 Rescue-Routinen, Atomic-Sync-Packer.
- `KIWI_SURVIVOR_PROMPT.md`: Historische Überlebens-Regeln aus der Cloud-Sandbox.
- `game_audio_update.c`: Referenz-Snippet für den reaktiven Bass-Override in `game_audio.c`.

---

## 2. Die 3 Eisernen Gesetze von STRUDELFORCE

Jede Codeänderung und Erweiterung im Projekt muss ausnahmslos diesen drei Gesetzen folgen:

### Gesetz 1: Amiga-First Core (`core_sim.c`)
- **Striktes C89/C99:** Reines Standard-C ohne Compiler-spezifische Erweiterungen oder POSIX-Zwang im Core.
- **16.16 Fixed-Point Arithmetik (`fix16_t` / `int32_t`):**
  - `1.0` entspricht `65536` (`1 << 16`).
  - **Null `float` oder `double`** im gesamten Gameplay- und Simulations-Loop!
- **Null `malloc` / `free`:** Keine dynamische Speicherallokation im Hot-Path. Alle Entitäten (Spieler, Projektile, Gegner, Partikel, Kristalle) leben in statisch vorallokierten Ringbuffern / Pools.
- **50Hz PAL-Tick:** Die Simulationsfunktion `core_tick()` berechnet exakt 20 Millisekunden (1/50 Sekunde).
- Das gesamte Kern-System muss ohne Änderungen direkt in **512 KB Chip-RAM** eines Amiga 500 ausführbar sein.

### Gesetz 2: Kein Minecraft-Look bei den Voxeln (`voxel_render.c`)
- **Keine Texturen:** Voxel dürfen niemals mit Pixel-Texturen belegt werden!
- **Reine Amiga-Farbpaletten:** Ausschließlich indizierte 16- bzw. 32-Farben-Paletten pro Biom (ECS/OCS/AGA).
- **Plastizität durch Face-Shading & Ambient Occlusion:** Räumliche Tiefe entsteht rein durch Richtungsabschattung (abgedunkelte Seiten-/Unterkanten der Voxel) und 2.5D Tiefen-Extrusion aus 2D-Sprite-Matrizen (`SpriteDef`).
- **4 Parallax-Ebenen:**
  - Layer 3 (`z = -20`): Deep-Sky & Copper-Farbverläufe (Amiga Copper-Bänder).
  - Layer 2 (`z = -6`): Mid-Background Ruinen-Säulen mit 1/4 Parallax.
  - Layer 1 (`z = 0`): Gameplay-Ebene (Spieler, Gegner, Schüsse, Kristalle, Truhen).
  - Layer 0 (`z = +2`): Vordergrund-Rohre / Strukturen mit 1.5x Scroll.
- **Interlace-Effekt:** Bei niedrigem Leben (`HP < 25%`) wird ein Amiga-Zeilen-Interlace-Jitter auf ungeraden Scanlines aktiviert.
- Alle visuellen Elemente müssen sich 1:1 auf Amiga-Bitplanes und Copper-Listen abbilden lassen.

### Gesetz 3: Cyber-Paula Audio & Reaktivität (`cyber_paula.c` / `game_audio.c`)
- **Synthese-Architektur:** Kombination aus Clean-Room Strudel-Parser (`strudel_cleanroom.h`), TFMX 50Hz-Makro-Synthese (`tfmx_macro_synth.h`) und 8-Stimmen Paula-Mixer.
- **Hyper-Reaktive Gameplay-Kopplung:**
  - **Waffenwechsel:** Tauscht sofort das Bass-Makro (Spread = Standard-Bass, Laser = PWM-Sawtooth mit Sub-Bass-Boost, Bounce 2.0 = FM-Pluck mit 50Hz Oktav-Arpeggio).
  - **Schild:** Steuert dynamisch die PWM-Modulationsgeschwindigkeit und Filter-Resonanz ($Q \approx 0.7 \dots 8.5$).
  - **Kritischer Zustand (`HP < 25%`):** Schließt den 1-Pole Low-Pass-Filter auf 550 Hz und aktiviert 4-Bit-Quantisierung auf Drums/Bässen.
  - **The Great Bath Schatztruhe:** Öffnen der Truhe im Unterwasser-Biom schaltet die Bassline nahtlos auf das originale Tribute-Pattern um:
    `"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`
    und aktiviert auf echter Amiga-Hardware den analogen 7kHz-Hardware-Tiefpassfilter über CIA-A `$BFE001` (Pin 1).
- **Nahtloses Live-Coding:** Pattern-Änderungen über die ESC-Konsole (`livecode_ui`) werden erst am Rand des 16-Step-Zyklus atomar umgeschaltet (`pending -> active`), um Audio-Knackser und Aussetzer zu verhindern.

---

## 3. Modul-Übersicht & Schnittstellenvertrag

| Modul | Header / Interfaces | Implementierung | Verantwortung |
|---|---|---|---|
| Gameplay-Simulation (Smith-Kern) | `core_sim.h` | `core_sim.c` | 16.16 Fixed-Point Physik, Waffen, Kollision, Spawner, Ringbuffer — für `game_main.c`/`gui_main.c` |
| Gameplay-Simulation (sf-Kern) | `sf_interfaces.h` | `core_sim_sf.c` | Schwarm-Variante (SIN8/COS8-Fix, 6-Waffen, Biome-Parameter) — für `main_pc.c`/`amiga_hw.c` (M68K)/`test_phase1.c` |
| Audio-Engine (Smith) | `game_audio.h` | `game_audio.c` | 8-Voice Mixer, 16-Bit WAV Export, reaktive Matrix, `ext_render`-Hook (v3) |
| Audio-Engine (Full) | `cyber_paula.h` | `cyber_paula.c` | TFMX Macros, Strudel Sequencer, Amiga Paula DMA Feed |
| TFMX-Player v2 | `tfmx_player.h` | `tfmx_player.c` | TFMX 1.5/Pro/7V, mdat./smpl./TFHD/TFMXPAK, C89, Mirror-Oracle `tools/tfmx_oracle.py` |
| MOD-Player | `mod_player.h` | `mod_player.c` | ProTracker MOD 4/6/8 Kanäle, C89, Mirror-Oracle `tools/mod_oracle.py` |
| Mixer | `sf_mixer.h` | `sf_mixer.c` | TFMX+MOD+Synth mit Gain-Ramps → `GameAudio.ext_render` |
| SID (Prep) | `sid_header.h` | `sid_header.c` | .sid-Header-Reader (No-Playback) |
| Level-Daten | `level_data.h` (generiert) | `levels/levels.json` + `tools/level_tool.py` | 12 Chunks, 5 Level, 4 Paletten, deterministischer Rogue-Generator |
| Live-Coding UI (Smith) | `livecode_ui.h` | `livecode_ui.c` | ESC-Editor, Zyklusrand-Swap, F2/F3 IO — für `game_main.c`/`gui_main.c` |
| Live-Coding UI (sf) | `sf_interfaces.h` | `livecode_ui_sf.c` | `livecode_on_input`/`compile_swap`/`list_user_music` — für `main_pc.c`/Amiga |
| Voxel Renderer | `voxel_render.h` | `voxel_render.c` | 2.5D Extrusion, Parallax-Planer, Palette Shading |
| Biome Generator | `sf_interfaces.h` | `biomes.c` | Biome-Parameter, Truhen-Platzierung, Chunk-RNG |
| Amiga Hardware | `sf_interfaces.h` | `amiga_hw.c` | OCS 6-Plane Bitplanes, Copper-List, $BFE001 Filter |
| Frontend (Terminal) | `core_sim.h`, `game_audio.h` | `game_main.c` | 50Hz ANSI/ASCII Terminal-Spiel, Demo-Bot, WAV-Dumper |
| Frontend (GUI) | `core_sim.h`, `game_audio.h` | `gui_main.c` | Win32/GDI 2.5D-Voxel-GUI, TFMX-Explorer, Strudel-Studio |
| Frontend (PC Raylib) | `sf_interfaces.h` | `main_pc.c` | 1280x720 Raylib-Renderer (optionales Target `pc_game`) |
| Amiga-Entry (M68K) | `sf_interfaces.h`, `tfmx_player.h`, `amiga_music.h` | M68K-Sektion von `amiga_hw.c` | **Einziger** 68k `main()`: 50Hz-VBlank-Loop, OCS-Render, TFMX-Paula-Streaming (Voice 0, 16574 Hz, 8-Bit), Track-Switch `M`, Exit `Q`/ESC. Musik via INCBIN (`amiga_music.c`), CIA-A-Tastatur, kein stdio |

> **⚠️ Dual-Core-Regel:** `core_sim.c` und `core_sim_sf.c` definieren beide
> `core_init`/`core_tick` + eigene `GameState`-Typen → **niemals in einem Binary linken**
> (gleiches gilt für `livecode_ui.c` vs. `livecode_ui_sf.c`). Target-Zuordnung steht in
> Makefile/`build_msvc.bat` und in `MERGE_NOTES.md` §2.

---

## 4. Richtlinien für zukünftige Entwickler & Agenten

1. **Determinismus bewahren:** Jeder Gameplay-Run mit demselben Seed muss bit-identische Zustände erzeugen. Nutze stets die projekteigenen LCG- bzw. Xorshift-RNGs.
2. **Compiler-Unabhängigkeit:** C-Quellen müssen sowohl unter GCC/Clang (Linux/macOS/Amiga) als auch unter MSVC (Windows) sauber und fehlerfrei kompilieren.
3. **Trace-First Testen:** Bei Audio- und Parser-Änderungen immer zuerst die Python-Oracles (`verify_headless.py`, `verify_core_sim.py`, `verify_audio_matrix.py`, `verify_livecode.py`) und `tools/selftest.py` ausführen; C-Änderungen an TFMX/MOD zusätzlich per `make c_vs_oracle` bit-genau gegen die Python-Referenzplayer prüfen.
4. **Dual-Core-Regel (post-Merge):** `core_sim.c` (Smith) und `core_sim_sf.c` (Schwarm) + deren Live-Code-Varianten nie in einem Binary kombinieren; bei neuen Targets die Kern-Zuordnung aus Makefile/`build_msvc.bat` übernehmen (s. `MERGE_NOTES.md` §2).
5. **Generatoren nicht handeditieren:** `Makefile`, `src/tfmx_player.h`, `src/mod_player.h`, `src/sf_mixer.h`, `src/sid_header.h`, `src/level_data.h`, `.vscode/tasks.json` und die v3-Strudel-Tracks werden von `tools/gen_sources.py` bzw. `tools/level_tool.py` erzeugt — Änderungen in den Generatoren vornehmen und regenerieren.
6. **Kiwi-Handoffs:** Nur ZIPs zählen. `python tools/kiwi_handoff.py pack . <out.zip>` (Manifest+Verify+Pack in einem Schritt), Empfänger prüft mit `verify`. Persistenz-Regeln: `docs/KIWI_PERSISTENCE_GUIDE.md`. C-Syntax ohne Compiler: `tools/c_parse_check.py` (pycparser; bekannte False Positives in `MERGE_NOTES.md` §5).
7. **M68K-Build ist stdio-/libc-frei (Dual-Toolchain-Regel):** Die Amiga-Quellen
   (`AMIGA_SRCS` + `src/amiga_music.c`) müssen mit **beiden** 68k-Toolchains
   linkbar bleiben: `m68k-amigaos-gcc` (CI/ADF) und `m68k-amiga-elf-gcc`
   (`-nostdlib`, VS Code Extension `BartmanAbyss.amiga-debug`, Haupt-Dev-Flow).
   → Kein `fopen`/`printf`/`malloc` im M68K-Pfad; Musik per INCBIN
   (`amiga_music.c`, Pfade relativ zum CWD — vorher `make stage_amiga_music`),
   Tastatur direkt CIA-A, `memset`/`memcmp` via `src/amiga_crt.h`,
   TFMX-Load via `tfmx_player_load_mem()`. PC-Builds bleiben unverändert (stdio OK).
   Extension-Integration (generiert): `Makefile`-Target `program`
   (wird vom `compile`-Task als `make -j4 program=<out>` gerufen) +
   `.vscode/{tasks,launch,settings,amiga,extensions}.json`. Kickstart-ROM-Pfad
   setzt der Nutzer in `.vscode/settings.json` (`amiga.rom-paths.A500`).

