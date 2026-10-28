# 🚀 STRUDELFORCE v0.2.0 (Consolidated Monolith)

[![Amiga 500](https://img.shields.io/badge/Platform-Amiga%20500%20%2F%20PC-blue.svg)](https://en.wikipedia.org/wiki/Amiga_500)
[![Release: v0.2.0](https://img.shields.io/badge/Release-v0.2.0-brightgreen.svg)](https://github.com/Tretboot/STRUDELFORCE/releases/tag/v0.2.0)
[![PAL 50Hz](https://img.shields.io/badge/Timing-50Hz%20PAL%20VBlank-green.svg)](#)
[![Fixed Point](https://img.shields.io/badge/Math-16.16%20Fixed--Point%20Zero--Float-orange.svg)](#)
[![Audio](https://img.shields.io/badge/Sound-Cyber--Paula%208--Voice%20TFMX%2BMOD-magenta.svg)](#)
[![Live-Coding](https://img.shields.io/badge/Music-Strudel%20Cleanroom%20AST-yellow.svg)](#)
[![Tests](https://img.shields.io/badge/Tests-32%2F32%20PASS-brightgreen.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

> **"Amiga 500 trifft Cyber-Paula & 2.5D-Voxel-Action (Turrican-Hommage)."**  
> Ein kompromissloser Retro-Action-Shooter auf Amiga-Hardware-Restriktionen (512 KB Chip-RAM, OCS/AGA Bitplanes, Copper-Listen), Chris Hülsbecks legendärer **TFMX Professional Sound-Engine**, ProTracker MOD, TidalCycles/Strudel Live-Coding und richtungsbeschatteten 2.5D-Voxeln.

### 📦 Offizieller Release-Download: [v0.2.0](https://github.com/Tretboot/STRUDELFORCE/releases/tag/v0.2.0)
* **Windows x64 Binary-Paket:** [`strudelforce-v0.2.0-windows-x64.zip`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.2.0/strudelforce-v0.2.0-windows-x64.zip) (ca. 2.8 MB)
* **Prüfsummen:** [`SHA256SUMS.txt`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.2.0/SHA256SUMS.txt)
* **Sofort spielbar (Single-Binary):** Enthält die eigenständige **`strudelforce.exe`** (2.5D Voxel GUI, inklusive integrierter CLI-Optionen `-headless`, `--demo` und `--test`) sowie die originalen Turrican 2 und Apidya TFMX-Soundtracks. Keine verwirrenden Extra-Dateien mehr nötig!

```text
   _____ _______ _____  _    _ _____  ______ _      ______ ____  _____   _____ ______ 
  / ____|__   __|  __ \| |  | |  __ \|  ____| |    |  ____/ __ \|  __ \ / ____|  ____|
 | (___    | |  | |__) | |  | | |  | | |__  | |    | |__ | |  | | |__) | |    | |__   
  \___ \   | |  |  _  /| |  | | |  | |  __| | |    |  __|| |  | |  _  /| |    |  __|  
  ____) |  | |  | | \ \| |__| | |__| | |____| |____| |   | |__| | | \ \| |____| |____ 
 |_____/   |_|  |_|  \_\____/|_____/|______|______|_|    \____/|_|  \_\\_____|______|
                                                                          v0.2 MONOLITH
```

---

## ⚡ Highlights & Features

1. **Grafisches 2.5D-Voxel-Frontend (`strudelforce.exe`):**
   - 1280x720 HD mit nativer Win32/GDI-Pipeline (keine externen DLL-Abhängigkeiten nötig).
   - Echte **2.5D-Voxel-Projektion** mit direktionalem Face-Shading (Top-Highlight, Front-Base, Side-Shadow) – kein Texturen-Minecraft-Look.
   - **4 Parallax-Ebenen:** Deep-Sky-Copperverläufe, Ruinen-Säulen, Gameplay-Ebene, Vordergrund-Rohre.
   - **Multi-Tier 2.5D Plattform-System:** Erhöhte Industrie-Laufstege, Aquädukte und Tempel-Altäre (Turrican-2 Vertikalität).
   - **Dynamischer Chris Hülsbeck Kampagnen-Soundtrack:** Turrican 2 Level-Soundtracks (Desert, Cave, Factory, Flight, Aliens) und Jukebox-Shuffle (`M`/`N` oder Gamepad `Y`).
   - Amiga-Zeilen-Interlace-Jitter bei kritischer Energie (`HP < 25%`).
   - Turrican-2-inspiriertes Hauptmenü, Ladebildschirm, Sieg- und Game-Over-Overlays.
   - **F5 Soundgodizer:** Analog-Wärme, Bass-Boost, tanh-Soft-Drive und Haas-Stereo-Wide.

2. **Konsolidierte Monolithische Architektur:**
   - **`src/game_logic.c`**: 16.16 Fixed-Point Physik, 3-Leben-System, Respawn-Schutz, 3 Waffen (Spread, Laser, Bounce 2.0 mit Eck-Clamp-Fix), Gegner, Endlos-Roguelike & Kampagnen-Modus.
   - **`src/soundengine_core.c`**: Universeller Hub für TFMX 1.5/Pro Replay, ProTracker MOD (4/6/8 Kanäle), Strudel-Cleanroom AST, Game-SFX, Cyber-Paula 8-Stimmen Mixer, Soundgodizer und Win32 waveOut.
   - **`src/engine_core.c`**: 2.5D Voxel-Szenen-Planer, Amiga OCS/AGA Hardware-Shims, Zero-Float Sinus-Scroller & Copper-Intro.
   - **`src/strudelforce.h`**: Zentraler Master-Header für alle Module.

3. **Spielmodi:**
   - **Kampagne:** 5 Level à 3 Wellen mit fester Biom-Progression (*Chrome Tempest*, *The Great Bath*, *Mechanical Hive*), Level-Clear Boni und epischem Sieg-Screen.
   - **Endlos-Roguelike:** Fortlaufende Wellen, kontinuierliche Steigerung und deterministische Highscore-Jagd.

4. **TFMX Explorer & Jukebox ([F2]):**
   - Integrierter Dateibrowser mit freier Pfadeingabe für TFMX-Module und ProTracker MODs.
   - Live-Analyse aller Stimmen mit animierten Pegelanzeigen, Frequenz- und Notenanzeige.

5. **Strudel Live-Code Studio ([F3]):**
   - Interaktiver Mini-Notation Editor (`"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`).
   - Glitchless-Tausch am 16-Step-Zyklusrand.
   - Windows-Clipboard-Support (Strg+V / Rechtsklick), Preset-Speicherung (`user_pattern.strudel`).

6. **50Hz ANSI/ASCII Terminal-Spiel (`strudelforce_game.exe`):**
   - Vollständig autark im Terminal spielbar (inkl. Headless Bot-Modus `--demo 2500` und `--campaign`).

7. **Kiwi Cloud Asset & Lokalisierungs-Pipeline:**
   - `assets/lang.json` & `tools/build_lang.py`: Erzeugt `src/localization.h` mit Zero-Malloc $O(1)$-Text-Lookups (DE/EN).
   - `assets/balancing.json`: Exakte Physik- und Level-Balancing-Werte pro Biom.
   - `assets/story.json`: 4-Kapitel Cyber-Odyssey-Story mit Dialogen.
   - `tools/asset_packer.py`: Packt alle Assets zlib-komprimiert in `assets.dat` und erzeugt `src/assets_index.h`.

---

## 📜 Die 3 Eisernen Gesetze von STRUDELFORCE

Jede Zeile Code gehorcht ausnahmslos diesen Grundsätzen (siehe [PROJECT_BIBLE.md](PROJECT_BIBLE.md)):

### 1. Gesetz 1: Amiga-First Core (`game_logic.c` & `engine_core.c`)
- **Striktes Standard-C (C89/C99)**: Keine Compiler-spezifischen Dialekte im Kern.
- **16.16 Fixed-Point Arithmetik (`fix16_t` / `int32_t`)**: `1.0 == 65536`. **Null `float` oder `double`** im Gameplay- und Render-Loop!
- **Null `malloc` / `free`**: Keine dynamischen Allokationen im Hot-Path. Alle Entitäten leben in statischen Ringpuffern.
- **50Hz PAL-Tick**: Berechnet exakt 20 Millisekunden pro Simulationsschritt.
- Lauffähig im **512 KB Chip-RAM** eines Amiga 500.

### 2. Gesetz 2: Kein Minecraft-Look bei den Voxeln (`engine_core.c`)
- **Keine Texturen**: Voxel werden niemals mit Pixel-Texturen belegt.
- **Amiga-Paletten**: 16/32 indizierte ECS/OCS/AGA-Farben pro Biom.
- **Plastizität durch Face-Shading**: Räumlichkeit entsteht rein durch 3-seitige Richtungsabschattung und 2.5D Tiefenextrusion.
- **4 Parallax-Ebenen**:
  - Layer 3 (`z = -20`): Deep-Sky & Copper-Bänder.
  - Layer 2 (`z = -6`): Ruinen-Säulen (1/4 Parallax).
  - Layer 1 (`z = 0`): Gameplay-Ebene (Spieler, Gegner, Schüsse, Kristalle, Truhen).
  - Layer 0 (`z = +2`): Vordergrund-Rohre (1.5x Scroll).
- **Interlace-Effekt**: Bei `HP < 25%` zittert das Bild auf ungeraden Scanlines.

### 3. Gesetz 3: Cyber-Paula Audio & Reaktivität (`soundengine_core.c`)
- **Synthese-Architektur**: TFMX 50Hz-Makro-Synthese, ProTracker MOD-Player und Strudel-Cleanroom AST.
- **Gameplay-Kopplung**:
  - *Waffenwechsel*: Tauscht Bass-Makro (Spread = Saw, Laser = PWM Sawtooth, Bounce = FM Pluck).
  - *Schild*: Moduliert dynamisch PWM-Geschwindigkeit und Filter-Resonanz.
  - *Kritischer Zustand (`HP < 25%`)*: Schließt den Tiefpassfilter auf 550 Hz.
  - *The Great Bath Schatztruhe*: Aktiviert das Tribute-Pattern und schaltet auf Amiga-Hardware den Hardware-Filter via CIA-A `$BFE001` Pin 1.
- **Glitchless Live-Coding**: Tausch erfolgt atomar am Zyklusrand.

---

## 🎮 Steuerung

### Tastatur
| Taste | Aktion |
|---|---|
| **W, A, S, D** / **Pfeiltasten** | Raumschiff bewegen / Springen |
| **Leertaste** | Sprung |
| **STRG** / **Z** | Primärfeuer (Spread, Laser, Bounce) |
| **ALT** / **X** | 360° Halt-Strahl (Surround-Beam) |
| **C** | Waffe wechseln |
| **S** | Morph-Ball Modus |
| **T** | Schatztruhe öffnen (*The Great Bath*) |
| **M** / **N** | Nächster Musiktitel (Jukebox / TFMX / MOD) |
| **F1** / **ENTER** | Spiel starten / Zurück zum Spiel |
| **F2** | TFMX Jukebox & Explorer |
| **F3** | Strudel Live-Code Studio |
| **F4** | Copper-Intro |
| **F5** | Soundgodizer umschalten (Aus / Bass / Warm / Overdrive+Wide) |
| **ESC** | Hauptmenü / Pause |

### Gamepad (XInput / Xbox / Retro-Controller)
| Controller-Eingabe | Aktion |
|---|---|
| **Linker Stick / D-Pad** | Raumschiff steuern / Menü-Navigation |
| **A-Taste** | Feuer / Menü-Auswahl |
| **X-Taste** | Halt-Strahl |
| **B-Taste** | Morph-Ball |
| **Y-Taste** | Nächster Musiktitel (Jukebox / TFMX / MOD) |
| **Rechter Schulterbutton (RB/RS)** | Waffe wechseln |
| **Start-Taste** | Hauptmenü / Pause |

### 💻 Befehlszeilen-Optionen (Single-Binary CLI)
`strudelforce.exe` kann sowohl interaktiv mit vollständiger 2.5D GUI als auch headless im Terminal ausgeführt werden:

| Parameter | Funktion |
|---|---|
| `strudelforce.exe` | Startet das vollständige 2.5D GUI Spiel (Standard) |
| `strudelforce.exe -headless [ticks]` | 50Hz PAL Simulation im Terminal (Standard: 2500 Ticks) |
| `strudelforce.exe --demo [ticks]` | Autonomer Demo-Bot mit Live-Status |
| `strudelforce.exe --campaign` | Aktiviert den 5-Level Kampagnen-Modus |
| `strudelforce.exe --test` | Führt interne Integrationstests headless aus |
| `strudelforce.exe --dump [N] [basis]` | Erzeugt N Headless-PPM Frame Dumps |
| `strudelforce.exe --help` | Zeigt die Befehlszeilen-Hilfe an |

---

## 🛠️ Kompilieren & Ausführen

### Windows (MSVC)
Erfordert Visual Studio / Build Tools (automatische vcvars-Erkennung):
```cmd
build_msvc.bat
```
Baut alle drei Kern-Executables (v0.2.0):
- `strudelforce.exe` (bzw. `strudelforce_v0.2.exe`): Haupt-Spiel (2.5D Voxel GUI, Jukebox, Studio)
- `strudelforce_game.exe`: 50Hz ANSI/ASCII Terminal-Spiel (`--demo 2500`, `--campaign`)
- `test_suite.exe`: Konsolidierte Testsuite (32 automatische Checks)

### GCC / Linux / Make
```bash
make all        # Baut Spiel, GUI und Testsuite
make game       # Baut strudelforce_game
make gui        # Baut strudelforce (2.5D GUI)
make test       # Baut und führt die Testsuite aus
make c89_check  # Strict C89-Syntaxprüfung
make verify     # Führt Python-Oracles und Selftests aus
```

### Python-Verifikation & Oracles (Zero-GCC)
```bash
python tools/selftest.py          # 32-Check Selftest (Linter, MOD, TFMX, Parser, Level)
python verify_core_sim.py         # Bit-identischer LCG-Determinismus-Check
python verify_audio_matrix.py     # Reaktive Audio-Filter & SFX-Check
python verify_headless.py         # Strudel-AST & Macro-Engine Test
python verify_livecode.py         # Live-Coding & Truhen-Tribute Check
```

---

## 🗂️ Projektstruktur

```text
STRUDELFORCE_Master/
├── src/
│   ├── strudelforce.h          # Zentraler Master-Header für alle Subsysteme
│   ├── game_logic.c            # 16.16 Fixed-Point Physik, 3-Leben, Waffen, Kampagne/Endlos
│   ├── soundengine_core.c      # Universal Audio Hub: TFMX, MOD, Strudel AST, Paula, Win-Audio
│   ├── engine_core.c           # 2.5D Voxel-Planer, Zero-Float Amiga-Shims, Retro-Intro
│   ├── gui_main.c              # Win32/GDI 2.5D Voxel-GUI (strudelforce.exe / strudelforce_v0.2.exe)
│   ├── game_main.c             # 50Hz ANSI Terminal-Spiel Frontend (strudelforce_game.exe)
│   ├── main_pc.c               # Raylib 2.5D Frontend
│   ├── test_suite.c            # Konsolidierte Integrationstest-Suite (32 Checks)
│   ├── localization.h          # (generiert) Statische Mehrsprachen-Tabellen
│   ├── assets_index.h          # (generiert) Asset-IDs und Blob-Index
│   └── embedded_music.h        # Integrierte Soundtrack-Daten (Apidya Meadow u.a.)
├── assets/                     # Game-Assets & Spezifikationen
│   ├── story.json              # 4-Kapitel-Story-Script & Dialoge
│   ├── balancing.json          # Physik- & Level-Balancing pro Biom
│   ├── lang.json               # Lokalisierung (DE/EN)
│   ├── level_01.json           # Plattform- & Gegner-Layout Level 01
│   ├── audio_reference/        # Referenz-MODs, TFMX-Tracks und Strudel-Sketche
│   ├── strudel_tracks/         # Live-Coding Kompositionen
│   └── *.tfmx.json             # Soundtrack- und Album-Spezifikationen
├── tools/                      # Tooling, Generatoren & Packer
│   ├── build_lang.py           # Generiert src/localization.h aus assets/lang.json
│   ├── asset_packer.py         # Packt assets.dat & erzeugt src/assets_index.h
│   ├── github_exporter.py      # Erstellt saubere Release-Exporte
│   ├── selftest.py             # Konsolidierter Syntax- & Modultester
│   └── ...                     # Oracles & Hilfswerkzeuge
├── PROJECT_BIBLE.md            # Single Source of Truth für Architektur, Gesetze & APIs
├── KIWI_SURVIVAL_GUIDE.md      # Best Practices für Cloud-Sandboxen
├── WORKFLOW_ESSAY.md           # Chronologische Entwicklungs-Historie
└── AGENTS.md                   # Entwickler- und Agenten-Briefing
```

---

## ⚖️ Lizenz
MIT License (Clean Room Implementierung ohne GPL/AGPL-Abhängigkeiten), siehe [LICENSE](LICENSE).
TFMX-Referenz-Dateien unter ihren jeweiligen Original-Rechten.
