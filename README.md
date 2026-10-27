# 🚀 STRUDELFORCE Master (2026)

[![Amiga 500](https://img.shields.io/badge/Platform-Amiga%20500%20%2F%20PC-blue.svg)](https://en.wikipedia.org/wiki/Amiga_500)
[![PAL 50Hz](https://img.shields.io/badge/Timing-50Hz%20PAL%20VBlank-green.svg)](#)
[![Fixed Point](https://img.shields.io/badge/Math-16.16%20Fixed--Point%20(int32)-orange.svg)](#)
[![Audio](https://img.shields.io/badge/Sound-Cyber--Paula%208--Voice%20TFMX-magenta.svg)](#)
[![Live-Coding](https://img.shields.io/badge/Music-Strudel%20Cleanroom%20AST-yellow.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

> **"Amiga 500 trifft Cyber-Paula & 2.5D-Voxel-Shmup."**  
> Ein kompromissloser Retro-Shooter, gebaut auf Amiga-Hardware-Restriktionen (512 KB Chip-RAM, OCS/AGA Bitplanes, Copper-Listen), Chris Hülsbecks legendärer **TFMX Professional Sound-Engine**, TidalCycles/Strudel Live-Coding und richtungsbeschatteten 2.5D-Voxeln.

```text
   _____ _______ _____  _    _ _____  ______ _      ______ ____  _____   _____ ______ 
  / ____|__   __|  __ \| |  | |  __ \|  ____| |    |  ____/ __ \|  __ \ / ____|  ____|
 | (___    | |  | |__) | |  | | |  | | |__  | |    | |__ | |  | | |__) | |    | |__   
  \___ \   | |  |  _  /| |  | | |  | |  __| | |    |  __|| |  | |  _  /| |    |  __|  
  ____) |  | |  | | \ \| |__| | |__| | |____| |____| |   | |__| | | \ \| |____| |____ 
 |_____/   |_|  |_|  \_\____/|_____/|______|______|_|    \____/|_|  \_\\_____|______|
                                                                           MASTER 2026
```

---

## ⚡ Highlights & Features

1. **Grafisches 2.5D-Voxel-Frontend (`strudelforce_gui.exe`):**
   - 1280x720 HD mit nativer Win32/GDI-Pipeline (keine externen DLL-Abhängigkeiten wie SDL/Raylib nötig).
   - Echte **2.5D-Voxel-Projektion** mit direktionalem Face-Shading (Top-Highlight, Front-Base, Side-Shadow) statt flacher Pixel oder blockigem Minecraft-Texturen-Look.
   - **4 Parallax-Ebenen:** Deep-Sky-Copperverläufe, Ruinen-Säulen, Gameplay-Akteure, Vordergrund-Rohre.
   - Amiga-Zeilen-Interlace-Jitter bei kritischer Energie (`HP < 25%`).

2. **Echtzeit-Audio & Hardware-DMA (`winmm` / `game_audio`):**
   - Volle Paula 8-Stimmen-Wiedergabe mit 16.574 Hz direkt über die PC-Lautsprecher.
   - Reaktive Gameplay-Soundmatrix: Waffenwechsel tauscht Bass-Makros, Schilde modulieren PWM, niedrige HP schalten den Tiefpassfilter auf 550 Hz.

3. **TFMX Professional Explorer & Jukebox ([F2]):**
   - Integrierter Dateibrowser für **76 Spiele-Ordner** mit hunderten Original-Amiga-TFMX-Modulen (*Apidya, Battle Isle, Turrican, Apprentice, BC Kid, Gem X*, etc.).
   - Live-Analyse aller 8 Hardware-Stimmen mit animierten VU-Metern, Frequenz- und Notenanzeige.
   - **[Convert to Strudel] Button:** Konvertiert TFMX-Spuren per Mausklick in Strudel Mini-Notation!

4. **Strudel Live-Code Studio ([F3]):**
   - Interaktiver mini-notation Editor (`"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`).
   - 16-Step-PAL-LED-Sequencer in Echtzeit synchron zur 50Hz VBlank-Schleife.
   - Atomarer Swap am Zyklusrand verhindert Audio-Knackser.

5. **50Hz ANSI/ASCII Terminal-Spiel (`strudelforce_game.exe`):**
   - Vollständig autark im Windows-Terminal oder Linux/macOS spielbar.
   - Unterstützt Headless-Bot-Modus (`--demo 2500`), WAV-Export und das Amiga-Copper-Intro.

6. **Amiga 500 ADF-Target:**
   - 68k-Binary + 5 Turrican-2-TFMX-Tracks auf einer 880K-FFS-Disk (`strudelforce.adf`, `make amiga_adf`).
   - Testbar in WinUAE/VAMOS (A500 OCS) — `M` schaltet Tracks, Paula-Voice-0 streamt 16574 Hz mit 8-Bit-Quantisierung.

---

## 📜 Die 3 Eisernen Gesetze von STRUDELFORCE

Jede Zeile Code folgt diesen drei unveränderlichen Grundsätzen:

### 1. Gesetz 1: Amiga-First Core (`core_sim.c`)
- **Striktes Standard-C (C89/C99)**: Keine Compiler-spezifischen Dialekte im Kern.
- **16.16 Fixed-Point Arithmetik (`fix16_t` / `int32_t`)**: `1.0 == 65536`. **Null `float` oder `double`** im Gameplay-Loop!
- **Null `malloc` / `free`**: Keine dynamischen Allokationen im Hot-Path. Alle Entitäten leben in statischen Ringpuffern.
- **50Hz PAL-Tick**: Berechnet exakt 20 Millisekunden pro Simulationsschritt.
- Lauffähig im **512 KB Chip-RAM** eines Amiga 500.

### 2. Gesetz 2: Kein Minecraft-Look bei den Voxeln (`voxel_render.c`)
- **Keine Texturen**: Voxel werden niemals texturiert.
- **Amiga-Paletten**: 16/32 indizierte ECS/OCS/AGA-Farben.
- **Plastizität durch Face-Shading & Ambient Occlusion**: Räumlichkeit entsteht rein durch 3-seitige Richtungsbeleuchtung und 2.5D Tiefenextrusion.
- **4 Parallax-Ebenen**:
  - Layer 3 (`z = -20`): Deep-Sky & Copper-Bänder.
  - Layer 2 (`z = -6`): Ruinen-Säulen (1/4 Parallax).
  - Layer 1 (`z = 0`): Gameplay-Ebene (Spieler, Gegner, Blaster, Kristalle, Truhe).
  - Layer 0 (`z = +2`): Vordergrund-Rohre (1.5x Parallax).
- **Interlace-Effekt**: Bei `HP < 25%` zittert das Bild auf ungeraden Scanlines.

### 3. Gesetz 3: Cyber-Paula Audio & Live-Coding (`tfmx_player.c` / `game_audio.c`)
- **TFMX 50Hz-Makro-Synthese**: Authentische Makro-Befehle (PWM-Verschiebung, Arpeggios, Envelopes, Wave-Loops).
- **Gameplay-Kopplung**:
  - *Laser*: PWM-Sawtooth mit Sub-Bass.
  - *Bounce 2.0*: FM-Pluck mit 50Hz Oktav-Arpeggio.
  - *Schild*: Steuert PWM-Geschwindigkeit und Resonanz.
  - *The Great Bath Schatztruhe*: Aktiviert das Tribute-Pattern und schaltet auf Amiga-Hardware den 7kHz-Hardware-Filter via CIA-A `$BFE001` (Pin 1) an.

---

## 🎮 Steuerung

### Im Spiel ([F1])
| Taste | Funktion |
|---|---|
| **W, A, S, D** / **Pfeiltasten** | Raumschiff bewegen / Springen |
| **Z** | Primärfeuer (Spread, Laser, Bounce) |
| **X** | 360° Surround-Beam halten |
| **C** | Waffe wechseln |
| **T** | Schatztruhe öffnen ("The Great Bath") |
| **F1** | 2.5D Voxel-Spiel anzeigen |
| **F2** | TFMX Jukebox & Explorer öffnen |
| **F3** | Strudel Live-Code Studio öffnen |
| **F4** | Amiga Copperbar-Intro abspielen |

### Im TFMX Explorer ([F2])
| Taste / Maus | Funktion |
|---|---|
| **Pfeiltasten Hoch/Runter** | Datei / Ordner auswählen |
| **Enter** oder **Klick** | Ordner öffnen / TFMX-Song abspielen |
| **[PLAY] / [PAUSE] / [STOP]** | Audiowiedergabe steuern |
| **[< SUBSONG] / [SUBSONG >]** | Subsong wechseln |
| **[IN STRUDEL KONVERTIEREN]** | TFMX-Spur in Strudel-Notation übersetzen & im Studio öffnen |

### Im Strudel Live-Code Studio ([F3])
| Taste / Maus | Funktion |
|---|---|
| **Tippen** | Mini-Notation eingeben |
| **Enter** | Pattern kompilieren & atomar am Zyklusrand aktivieren |
| **Presets** | "The Great Bath", "Apidya Meadow" oder eigene Patterns laden |

---

## 🛠️ Kompilieren & Ausführen

### Unter Windows (MSVC)
Ein einziger Befehl baut **sechs** Executables ohne zusätzliche Abhängigkeiten:
```cmd
build_msvc.bat
```
Ausführen:
```cmd
.\strudelforce_gui.exe       # Startet das grafische 2.5D Voxel-Spiel & Studio
.\strudelforce_game.exe      # Startet das 50Hz Terminal-Spiel (--demo, --audio für WAV-Dump)
.\test_audio_mix.exe         # TFMX/MOD-Mix-Test (s. LOCAL_ONLY_TESTS.md)
.\strudelforce_test.exe      # Phase-1-Integrationstest (43 Checks)
.\strudelforce_headless.exe  # Headless Audio-Matrix-Dump (WAV/JSON)
.\win_audio_selftest.exe     # Audio-Transport v2 Selftest (Ring-Buffer + WaveOut-Thread)
```

### Unter Linux / macOS (GCC / Clang)
```bash
make game                   # Terminal-Spiel bauen
make gui                    # Win32-GUI (nur unter Windows lauffähig)
make test                   # Alle Tests & Demo-Runs ausführen
make strudel_export         # Alle TFMX-Module im Batch exportieren
make headless_test          # Headless Audio-Matrix-Dump
make c_vs_oracle            # C- vs. Python-Referenzplayer (bit-genauer Abgleich)
make amiga_lint             # 68k-Lint ohne Toolchain (m68k-amigaos-gcc optional)
```

### Amiga-Entwicklung: Haupt-Workflow (VS Code + Bartman-Extension)
**Primäre Dev-Umgebung** — kompilieren & debuggen auf Knopfdruck (F5):
1. VS Code Extension **`BartmanAbyss.amiga-debug`**
   („Amiga C/C++ Compile, Debug & Profile") installieren — sie bringt
   `m68k-amiga-elf-gcc` 15.2 (LTO, `-nostdlib`), GDB, WinUAE und `elf2hunk`
   self-contained mit.
2. In `.vscode/settings.json` den Pfad zur **Kickstart 1.3 ROM** setzen
   (`amiga.rom-paths.A500`) — aktuell dort ein Platzhalter.
3. **F5** → „Amiga 500 (STRUDELFORCE)": das generierte `compile`-Task ruft
   `make -j4 program=out/strudelforce.amiga` mit der gebündelten Toolchain,
   WinUAE startet mit dem Binary + GDB.

Das Projekt ist dafür vorbereitet (alles generiert via `tools/gen_sources.py`):
- `Makefile`: `program`-Target mit den Extension-Flags (LTO, `-nostdlib`,
  `--emit-relocs`, elf2hunk).
- `.vscode/tasks.json` (Task `compile`), `launch.json`, `settings.json`,
  `amiga.json`, `extensions.json`.
- **Kein stdio/libc im 68k-Build:** Die 5 Turrican-2-Tracks sind per INCBIN
  in `src/amiga_music.c`/`amiga_music.h` eingebettet (vor dem Build:
  `make stage_amiga_music` bzw. `python tools/stage_amiga_music.py`);
  Tastatur läuft direkt über CIA-A ($BFE001/$BFE000), `memset`/`memcmp`
  stehen in `src/amiga_crt.h`. `tfmx_player` lädt per
  `tfmx_player_load_mem()` aus den EMBED-Daten.
- Debug-Overlay: `debug_text()`/`KPrintF()` der Extension hängen bei Bedarf
  in `amiga_log()` (M68K-Sektion von `src/amiga_hw.c`) ein.

### Amiga 68k Cross-Compile & ADF-Floppy (CI/Release-Pfad)
Alternativ zur Extension (z.B. für die bootfähige ADF) via
`m68k-amigaos-gcc` — eine der beiden Wege:
1. **WSL2/Ubuntu (One-Shot-Skript):**
   ```bash
   wsl -d Ubuntu
   cd /mnt/<laufwerk>/.../STRUDELFORCE_Master
   bash tools/amiga_toolchain_wsl.sh   # baut Toolchain (~10 min) + Binary + ADF
   ```
2. **GitHub Actions:** `.github/workflows/amiga.yml` (manuell, Push auf main,
   oder Tag `v*` für Release-Builds mit Versions-Suffix) liefert
   `strudelforce.adf` als Artifact.

Manuell (mit vorhandener Toolchain, z.B. `/opt/amiga`):
```bash
make stage_amiga_music                                        # Tracks + startup-sequence
make amiga_bin M68K_CC=/opt/amiga/bin/m68k-amigaos-gcc
make amiga_adf M68K_CC=/opt/amiga/bin/m68k-amigaos-gcc        # FFS 880K via xdftool pack
```

**WinUAE/VAMOS-Test:** A500 (OCS, 512 KB Chip), Kickstart 1.3/2.0 ROM,
`strudelforce.adf` als DF0: einlegen. Boot-Disk-Modus startet das Spiel per
`s/startup-sequence`; alternativ als Daten-Disk: `df1:strudelforce.amiga`.
Steuerung: **M** = nächsten Track, **Q**/**ESC** = sauberer Exit.

---

## 🗂️ Verzeichnisstruktur

```text
STRUDELFORCE_Master/
├── src/
│   ├── gui_main.c          # 1280x720 Native 2.5D Voxel GUI, Explorer & Studio (Smith-Kern)
│   ├── game_main.c         # 50Hz ANSI/ASCII Terminal-Spiel Frontend (Smith-Kern)
│   ├── core_sim.c/.h       # 16.16 Fixed-Point Physik & Gameplay — Smith-Kern (kanonisch)
│   ├── core_sim_sf.c       # Schwarm-Variante des Kerns (sf_interfaces-API, SIN8/COS8-Fix)
│   ├── sf_interfaces.h     # Schnittstelle des sf-Kerns (PC/Amiga/Phase-1)
│   ├── game_audio.c/.h     # 8-Voice Paula Mixer, reaktive Matrix, ext_render-Hook
│   ├── tfmx_player.c/.h    # TFMX 1.5/Pro/7V 50Hz Modul-Player (v2, C89, Oracle-Mirror)
│   ├── mod_player.c/.h     # ProTracker-MOD Player (C89, Oracle-Mirror)
│   ├── sf_mixer.c/.h       # TFMX+MOD+Synth Mixer mit Gain-Ramps
│   ├── sid_header.c/.h     # .sid-Header-Reader (No-Playback, Vorbereitung)
│   ├── amiga_intro.c/.h    # Amiga Copperbars & Sinus-Scroller Intro
│   ├── livecode_ui.c/.h    # Strudel AST Parser & ESC Live-Coding Konsole (Smith)
│   ├── livecode_ui_sf.c    # Live-Code-UI für den sf-Kern (PC/Amiga)
│   ├── win_audio.c/.h      # WaveOut-Transport v2: statischer Ring + TIME_CRITICAL-Thread (kein GDI-Coupling)
│   ├── test_win_audio.c    # Ring-/Thread-Selftest des Audio-Transports (win_audio_selftest)
│   ├── retro_font.h        # Integrierter 8x8 Bitmap-Schriftfont
│   ├── amiga_hw.c          # OCS/AGA 6 Bitplanes, Copper-List, CIA-A — inkl. EINZIGEM M68K main() (Amiga-Entry)
│   ├── biomes.c            # Deterministischer Chunk- & Truhen-Generator
│   ├── voxel_render.c/.h   # 4-Layer 2.5D Voxel Render-Planer
│   ├── main_pc.c           # Raylib-Frontend (optionales Target pc_game, RAYLIB_DIR)
│   ├── level_data.h        # (generiert) 12 Chunks / 5 Level / 4 Paletten
│   ├── compat/             # Win32-Shims (dirent.h, unistd.h) für MSVC-SDK
│   ├── test_phase1.c       # 43-Check-Integrationstest (sf-Kern)
│   └── test_audio_mix.c    # Lokales TFMX/MOD-Mixer-Harness
├── tools/                    # Python-Tools: Oracles, Selftest, Importer, Kiwi-Handoff
│   ├── selftest.py          # 33-Check-Suite (Parser, MOD, TFMX, Level, Determinismus)
│   ├── gen_sources.py       # Single Source of Truth: Header, Makefile, tasks.json, Strudel
│   ├── level_tool.py        # levels.json → LevelData + validate + Rogue-Chunks
│   ├── c_vs_oracle.py       # C- vs. Python-Referenzplayer (bit-genau)
│   ├── tfmx_oracle.py       # Python TFMX-Referenzplayer + --scan über die TFMX-Bibliothek
│   ├── mod_oracle.py        # Python MOD-Referenzplayer
│   ├── kiwi_handoff.py      # manifest / verify / pack (sha256-ZIP-Handoffs)
│   ├── c_parse_check.py     # C-Syntax-Check ohne Compiler (pycparser)
│   ├── amiga_toolchain_wsl.sh # One-Shot 68k-Toolchain + ADF (WSL2/Ubuntu)
│   ├── stage_amiga_music.py # Windows-Spiegel von make stage_amiga_music
│   └── ...                   # tfmx_importer, tfmx2strudel, strudel2tfmx, headless_verify ...
├── verify_core_sim.py        # Python-Oracle: Determinismus & Invarianten des Kerns
├── verify_headless.py        # Python-Oracle: Strudel-AST & Macro-Engine
├── verify_audio_matrix.py    # Python-Oracle: Reaktive Audio-Status-Matrix
├── verify_livecode.py        # Python-Oracle: Live-Code-Compiler & Truhen-Tribute
├── levels/levels.json        # 12 Chunks, 5 Level, 4 Paletten (128x128, 16 Farben)
├── docs/                     # Agent-Spezifikationen, Handoffs, Essays, Bilder
├── tests/                    # Test-Daten (TFMX-Paare, sfsong, letzte Test-Run)
├── user_music/               # Strudel-Pattern (.strudel) & Presets
├── assets/samples/           # 8-Bit Amiga-Samples
├── TFMX Professional/        # 76 Original Chris-Hülsbeck Amiga-Soundtracks (lokal)
├── releases/                 # Fertige Distribution-ZIPs (Pre-Releases)
├── archive/                  # Zweig-Originals (branches/), Claude-Run, alte Artefakte
├── .github/workflows/        # amiga-adf CI: 68k-Build + ADF-Artefact (ubuntu-latest)
├── Makefile                  # (generiert) Multi-Target: PC, Amiga, Tests, Oracles
├── build_msvc.bat            # 6-Target 1-Klick Windows MSVC Build-Script
├── MERGE_NOTES.md            # Provenance, Merge-Entscheidungen & Verifikation (2026-10-27)
├── LOCAL_ONLY_TESTS.md       # Anleitung: C-Tests auf Windows/MSVC
└── AGENTS.md                 # Systemarchitektur & Briefing für Entwickler
```

---

## 📦 Pre-Releases & Downloads

Fertig gepackte ZIP-Archive finden sich im Ordner [`releases/`](file:///F:/AgentHome/STRUDELFORCE_Master/releases/):
- **`strudelforce_v0.5.0_win64.zip`**: Vollständig lauffähiges portable Paket (GUI, Terminal-Game, Soundtracks, Tools).
- **`strudelforce_v0.5.0_source.zip`**: Kompletter Quelltext mit Makefiles und Build-Skripten.

---

## ⚖️ Lizenz
MIT License (Clean Room Implementierung ohne GPL/AGPL-Abhängigkeiten), siehe [LICENSE](LICENSE).
Die TFMX-Soundtrack-Dateien (`TFMX Professional/`) sind **nicht** im Repository
enthalten (`.gitignore`) und bleiben lokale Referenz-Assets unter ihren
Original-Lizenzen — sie werden für `make stage_amiga_music` / den TFMX-Explorer
lokal im Workspace erwartet.

