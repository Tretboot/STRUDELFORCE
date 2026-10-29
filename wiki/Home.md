# STRUDELFORCE — Offizielles Entwickler- & Agenten-Wiki

Willkommen im offiziellen Wiki des Projekts **STRUDELFORCE**!

STRUDELFORCE ist ein kompromissloser 50Hz Fixed-Point Retro-Action-Shooter (Turrican-Hommage) mit prozeduraler 2.5D-Voxel-Grafik und einer hybriden Amiga-Sound-Engine (Chris Hülsbeck TFMX, ProTracker MOD und Cleanroom Strudel Live-Coding).

Das Projekt läuft bit-genau und mit vollständiger funktionaler Parität auf zwei Hauptplattformen:
1. **Commodore Amiga 500:** Echtes Bare-Metal Floppy-Image (`strudelforce.adf`), 512 KB Chip-RAM, Motorola 68000 @ 7.09 MHz, OCS Custom Chipset (Copper, Bitplanes, Blitter, Paula 4-Kanal DMA Sound).
2. **PC (Windows / Linux):** Win32 2.5D-Voxel GUI (`strudelforce.exe`) mit echtem 3D Star Wars Perspektiv-Crawl, 50Hz ANSI/ASCII Terminal-Spiel (`strudelforce_game.exe`) und Headless-CI.

---

## 📚 Inhaltsverzeichnis & Wissensportal

| Wiki-Seite | Beschreibung |
|---|---|
| [[Project-Bible]] | Die **Single Source of Truth**: Die 3 Eisernen Gesetze, 16.16 Fixed-Point Arithmetik, Speicherlayout und Modul-Architektur. |
| [[Agent-Workflow-and-Cooperation]] | **Unsere Arbeitsweise:** Human-in-the-Loop Paarprogrammierung, Multi-Agent Swarm, Meeseeks-Zyklen, Kiwi-Sandbox-Isolation und Trace-First Oracles. |
| [[Amiga-Hardware-and-ADF-Pipeline]] | Amiga 500 Bare-Metal, 6 Bitplanes, Copper-Listen, Paula Audio-DMA, Floppy-ADF-Pipeline und WinUAE Hardware-Emulation. |
| [[Cyber-Paula-TFMX-and-MOD-Audio]] | Chris Hülsbeck TFMX 50Hz-Makro-Synthese, ProTracker 4V MOD Replay, Cleanroom Strudel Live-Coding und Soundgodizer DSP. |
| [[Level-Engine-and-Voxel-Pipeline]] | Level Engine v2 (197 Chunks, 37 Level), 2.5D Voxel-Szenen-Planer und der Star Wars 3D Perspektiv-Crawl. |
| [[Captains-Log-Chronik]] | Das vollständige chronologische Forschungstagebuch aller Meeseeks-Iterationen von Bootstrap bis v0.3.1. |

---

## ⚡ Schnellstart für Entwickler & KI-Agenten

### 1. Windows (MSVC)
```cmd
build_msvc.bat
```
Erzeugt in Sekunden:
- `strudelforce.exe`: Die spielbare 2.5D-Haupt-GUI (inkl. Star Wars Crawl & Logbuch-Abspann)
- `strudelforce_game.exe`: 50Hz ANSI Terminal-Spiel
- `test_suite.exe`: 44 automatisierte Konsistenz- und Integrationstests

### 2. Amiga 500 Floppy Image (ADF)
```bash
make -C amiga
python tools/build_amiga_adf.py
tools/start_winuae.bat
```
Baut das M68K-Binary, formatiert ein valides 880 KB OFS Floppy-Image (`strudelforce.adf`) und startet WinUAE mit realistischen Diskettenkopf- und Motorgeräuschen.

### 3. Trace-First Verifikation (Zero-GCC)
```bash
python tools/selftest.py
python verify_core_sim.py
```

