# STRUDELFORCE — PROJEKT-BIBEL (Single Source of Truth)

**Projekt-Status:** v0.3.1 Full Parity Monolith (Amiga 500 Bare-Metal & PC 2.5D GUI)  
**Ziel-Plattform:** Commodore Amiga 500 (512 KB Chip-RAM, OCS, 68000 @ 7.09 MHz) & PC (Win32 / ANSI Terminal / GCC / MSVC)  
**Letzte Aktualisierung:** 2026-10-29  

---

## 1. Executive Summary & Architektur-Vision

STRUDELFORCE ist ein 50Hz Fixed-Point Retro-Actionspiel (Turrican-Hommage) mit prozeduraler 2.5D-Voxel-Grafik und einer hybriden Sound-Engine (Chris Huelsbeck TFMX, ProTracker MOD und Cleanroom Strudel Live-Coding).

Das Projekt erreicht mit Version 0.3.1 volle funktionale Parität zwischen modernem PC (Win32 GUI / Terminal) und echtem Amiga 500 Bare-Metal (Floppy ADF Image):
- **`src/game_logic.c`**: 16.16 Fixed-Point Physik, 3-Leben-System, Waffen, Spawner, Endlos- & Kampagnen-Modus, Livecode-Puffer.
- **`src/soundengine_core.c`**: Universeller Hub für TFMX-Parsing, MOD-Playback, Strudel-Cleanroom AST, Game-SFX, Cyber-Paula Mixer, Soundgodizer und Win32 waveOut.
- **`src/engine_core.c`**: 2.5D Voxel-Szenen-Planer (4 Parallax-Ebenen, Directional Shading, Low-HP Jitter), Amiga OCS/AGA Hardware-Shims und Copper-Intro.
- **`src/strudelforce.h`**: Zentraler Master-Header für alle Subsysteme.
- **`amiga/main.c`**: Autarkes Amiga-500-Bare-Metal-Programm mit 3D-Wireframe-Cracktro, Turrican-2-Menü, AmigaShell-Ladebalken, 50Hz-Simulationsloop und 4-Kanal Paula DMA Musik.
- **`src/gui_main.c`**: Moderne 2.5D PC-Grafikoberfläche mit echtem 3D Star Wars Perspektiv-Crawl (Zero-Float Scanline-Trapezoid Raycasting) und interaktivem Abspann inklusive vollständigem Captain's Log.

---

## 2. Chronologische Historie & Meilensteine

### Phase 1: Die beiden Entwicklungsstämme (2026-10-25 bis 2026-10-26)
- **Zweig A ("Schwarm" / KIWI):** Spezifikationsgetreue Implementierung der Module `core_sim_sf.c`, `cyber_paula.c`, `voxel_render.c`, `biomes.c` und `amiga_hw.c`. Fokus auf SIN8/COS8 Diagonalsymmetrie, 6 Waffen und Amiga-Hardware-Register.
- **Zweig B ("Smith Prime" / Meeseeks):** Fokus auf ein sofort spielbares 50Hz ANSI/ASCII Terminal-Frontend (`game_main.c`), WAV-Export und Python-Oracles (`verify_core_sim.py`, `verify_audio_matrix.py`). Meilenstein 4 etablierte den ESC-Live-Coding-Editor mit nahtlosem Pattern-Swap am Zyklus-Rand.

### Phase 2: Der Große Merge & Dual-Core-Phase (2026-10-27)
- Zusammenführung beider Stämme in die Workspace-Root `STRUDELFORCE_Master`.
- Einführung der Dual-Core-Regel (Smith-Kern für Terminal/GUI, Schwarm-Kern für Amiga/Phase1-Tests).
- Entwicklung von `gui_main.c`: Win32/GDI 2.5D-GUI mit Hauptmenü im Turrican-2-Stil, Loading-Screen, Gameplay und Game-Over-/Sieg-Overlay.
- Integration von Turrican-inspirierten Mechaniken: 3 Startleben, Respawn-Unverwundbarkeit (100 Ticks) und deterministisch platzierte Extraleben-Pickups an den Welträndern.
- Korrektur des Bounce-Ecken-Bugs (Clamp beider Achsen vor dem Split).

### Phase 3: Monolithische Defragmentierung & Universal Hub (2026-10-28)
- Vollständige Defragmentierung des C-Codes: Zusammenfassung der verstreuten Dateien zu `game_logic.c`, `soundengine_core.c` und `engine_core.c`.
- Der `soundengine_core.c` dient nun als einziger, universeller Audio-Hub mit integriertem TFMX-, MOD-, Strudel- und Game-SFX-System.
- Reduktion auf 7 C-Dateien im `src/`-Ordner.
- Erstellung der Master-Dokumentation `PROJECT_BIBLE.md` und des selbstentpackenden `kiwi_injector.py` für nahtlose Cloud-Sandbox-Handoffs.

### Phase 4: Amiga 500 Parität, Star Wars 3D Crawl & Logbuch-Integration (2026-10-29)
- **Vollständiges Amiga-ADF-Image (`strudelforce.adf`):** `tools/build_amiga_adf.py` erzeugt ein 880 KB OFS Floppy-Image (`boot1x`, `s/startup-sequence`), bootfähig auf echtem Amiga 500 mit Kickstart 1.3.
- **Amiga State Machine (`amiga/main.c`):**
  1. `AMI_STATE_INTRO`: 3D-Wireframe-Vektorwürfel in Echtzeit (100% Integer Fixed-Point-Rotation), Copper-Gradient und Sinus-Scroller.
  2. `AMI_STATE_MENU`: 2-Ton-Titelbanner im Turrican-2-Stil mit Menüauswahl.
  3. `AMI_STATE_LOADING`: Amiga-Block-Ladebalken mit wandernden Blenden und Hardware-Statusmeldungen.
  4. `AMI_STATE_STORY`: Star Wars 3D Story Crawl.
  5. `AMI_STATE_GAME`: 50Hz Fixed-Point Simulation mit Paula DMA Musik.
  6. `AMI_STATE_CREDITS`: Demoszene- und Entwickler-Würdigung.
- **Paula Hardware Audio (ThePlayer 6.1a):** Hardware-Audio-DMA (`AUD0..AUD3`) läuft direkt im VBlank ohne messbare CPU-Last.
- **WinUAE Hardware-Emulation:** Realistische Diskettenkopf- und Motorgeräusche (`floppy0sound=1`, 100% Lautstärke) via `tools/start_winuae.bat`.
- **Echter Star Wars 3D Perspektiv-Crawl:** Zero-Float Scanline-Trapezoid Raycasting in `gui_main.c` mit kontinuierlicher Verjüngung zum Fluchtpunkt, atmosphärischem Horizont-Fade und Schlagschatten.
- **Abspann mit vollständigem Captain's Log:** 1446 Zeilen in `src/embedded_credits.h` mit automatischem Song- und Subsong-Wechsel (Chris Hülsbeck Software Manager Subsong 21) und visuellen Iterations-Headern.
- **Level Engine v2:** Geborgen und integriert (197 Chunks, 37 Level in `levels/levels_v2.json` und `levelkit/lv2.py`).

---

## 3. Die 3 Eisernen Gesetze von STRUDELFORCE

Jede Codeänderung muss ausnahmslos diesen drei Gesetzen gehorchen:

### Gesetz 1: Amiga-First Core (`game_logic.c` / `core_sim.c`)
- **Striktes C89/C99:** Reines Standard-C ohne Compiler-Extensions oder POSIX-Zwang im Core.
- **16.16 Fixed-Point (`fix16_t` / `int32_t`):**
  - `1.0 == 65536` (`1 << 16`).
  - **Null `float` oder `double`** im Gameplay-, Physik- und Simulations-Loop!
- **Null `malloc` / `free`:** Keine dynamische Allokation im Hot-Path. Alle Entitäten (Spieler, Projektile, Gegner, Partikel, Kristalle, Pickups) leben in statisch vorallokierten Ringbuffern.
- **50Hz PAL-Tick:** Jeder Tick entspricht exakt 20 Millisekunden (1/50 Sekunde).
- Speicherbudget: Vollständig lauffähig in den **512 KB Chip-RAM** eines Amiga 500.

### Gesetz 2: Kein Minecraft-Look bei den Voxeln (`engine_core.c` / `voxel_render.c`)
- **Keine Pixeltexturen:** Voxel dürfen niemals texturiert werden!
- **Reine Amiga-Paletten:** Indizierte 16- bzw. 32-Farben-Paletten (OCS/ECS/AGA).
- **Plastizität durch Face-Shading:** Räumliche Tiefe entsteht rein durch Directional Face-Shading (abgedunkelte Kanten/Seiten) und Ambient Occlusion.
- **4 Parallax-Ebenen:**
  - Layer 3 (`z = -20`): Deep-Sky & Copper-Verläufe (Amiga Copper-Bänder).
  - Layer 2 (`z = -6`): Mid-Background Ruinensäulen (1/4 Parallax).
  - Layer 1 (`z = 0`): Gameplay-Ebene (Spieler, Gegner, Schüsse, Kristalle, Truhen).
  - Layer 0 (`z = +2`): Vordergrund-Rohre (1.5x Scroll).
- **Interlace-Jitter:** Bei kritischem Leben (`HP < 25%`) wird ein Zeilen-Interlace-Jitter auf ungeraden Scanlines aktiviert.

### Gesetz 3: Cyber-Paula Audio & Reaktivität (`soundengine_core.c` / `amiga/main.c`)
- **Synthese-Architektur:** Kombination aus Cleanroom Strudel-Parser, TFMX 50Hz-Makro-Synthese und Paula-Mixer.
- **Hyper-Reaktive Gameplay-Kopplung:**
  - **Waffenwechsel:** Tauscht sofort das Bass-Makro (Spread = Standard Saw, Laser = PWM Sawtooth mit Sub-Bass-Boost, Bounce = FM Pluck mit 50Hz Oktav-Arpeggio).
  - **Schild:** Steuert PWM-Modulationsgeschwindigkeit und Filter-Resonanz ($Q \approx 0.7 \dots 8.5$).
  - **Kritischer Zustand (`HP < 25%`):** Schließt den 1-Pole Low-Pass Filter auf 550 Hz und aktiviert 4-Bit Quantisierung auf Drums/Bässen.
  - **The Great Bath Schatztruhe:** Öffnen der Truhe im Unterwasser-Biom schaltet die Bassline nahtlos auf das originale Tribute-Pattern um:
    `"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`
    und schaltet auf Amiga-Hardware den Hardware-Tiefpassfilter über CIA-A `$BFE001` Pin 1 ein.
- **Nahtloses Live-Coding:** Strudel-Pattern-Änderungen über die ESC-Konsole werden erst am 16-Step-Zyklus-Rand atomar gewechselt (glitchless).

---

## 4. Modul-Übersicht & Schnittstellenvertrag

| Modul | Haupt-Header | Quellcode | Aufgaben & Schnittstellen |
|---|---|---|---|
| Master-Schnittstelle | `strudelforce.h` | — | Alle Typen, Fixed-Point-Makros, Enums, GameState, RenderItems, SFX-IDs. |
| Gameplay-Logik | `strudelforce.h` | `src/game_logic.c` | 16.16 Physik, LCG-RNG, Kollision, Turrican-3-Leben, Spawner, Endlos/Kampagne, Biome-Chunks, Livecode-Puffer. |
| Universal Sound Engine | `strudelforce.h` | `src/soundengine_core.c` | Hub für TFMX-Player, MOD-Player, Strudel-Cleanroom AST, Game-SFX, Cyber-Paula Mixer, Soundgodizer, Win-Audio. |
| Engine Core | `strudelforce.h` | `src/engine_core.c` | 2.5D Voxel-Planer, 4 Parallax-Layer, Directional Face Shading, Amiga Copper/Bitplane-Shims, Copper-Intro. |
| GUI Frontend | `strudelforce.h` | `src/gui_main.c` | Win32/GDI 2.5D GUI (`strudelforce.exe`), Turrican-2 Menü, Star Wars 3D Crawl, Abspann mit komplettem Captain's Log. |
| Terminal Frontend | `strudelforce.h` | `src/game_main.c` | 50Hz ANSI/ASCII Terminal-Spiel (`strudelforce_game.exe`), Headless Bot (`--demo 2500`), WAV-Dumper. |
| Amiga Bare-Metal | — | `amiga/main.c` | Autarke 50Hz State Machine: 3D Cracktro, Turrican Menü, Ladebildschirm, 50Hz OCS Gameplay, ThePlayer 6.1a DMA Audio. |
| Level Engine v2 | `level_engine.h` | `src/level_engine.c` | 197 Chunks, 37 spielbare Levels, Musik-Zonen, deterministischer Rogue-Generator. |
| Integrationstest-Suite | `strudelforce.h` | `src/test_suite.c` | Headless Test aller Module: 44 Checks (2500 Ticks Determinismus, Kampagne, Strudel-AST, Audio-Hub, SFX, Level-Engine). |

---

## 5. Star Wars 3D Perspektiv-Crawl & Abspann-Architektur

### Star Wars 3D Crawl (Zero-Float Scanline-Trapezoid Raycaster)
In `gui_main.c` wird der Story-Crawl über ein reines Integer-/Fixed-Point Raycasting auf einer nach hinten geneigten 3D-Ebene berechnet:
- **Text-Strip Buffer:** 640x2400 Pixel (`s_crawl_plane`), in den die Story-Zeilen mit 16x16 Bitmap-Glyphen und 2px-Drop-Shadow gerastert werden.
- **Scanline-Projektion:** Für jede Scanline $y$ von Horizont $y_{vanish} = 145$ bis Basis $y_{base} = 672$:
  - Tiefe $Z = \frac{K}{y - y_{vanish}}$ ($K = 120000$)
  - Texturkoordinate $v = s\_story\_scroll - (Z - Z_{near})$
  - Trapez-Breite $W(y) = \frac{(y - y_{vanish}) \cdot W_{base}}{H_{area}}$
  - Horizontaler Schritt $step\_u = \frac{CRAWL\_TEX\_W \ll 16}{W(y)}$
  - Atmosphärischer Horizont-Fade: Text blendet weich in das Weltraum-Sternenfeld aus.
  - Steuerung: `W` / `UP` = Verlangsamen/Rückwärts, `S` / `DOWN` = Warp-Speed, `SPACE` = Pause, `ENTER` = Überspringen.

### Abspann (Credits & Vollständiges Captain's Log)
- Enthält die Kapitel I bis IV (Sound-Architektur, Voxel-Engine, Gameplay-Benchmark, Demoszene-Greetings).
- Nahtloser Übergang in **Kapitel V: Das vollständige Captain's Log** (1446 Zeilen in `src/embedded_credits.h`).
- Automatische Musik- und Subsong-Umschaltung: Beim Erreichen des Captain's Logs wechselt die Musik sofort zu Chris Hülsbecks Software Manager Subsong 21 (Amiga Heritage Thema).
- Über die Taste `M` bzw. Gamepad-Taste `Y` kann jederzeit durch weitere Subsongs geschaltet werden.

---

## 6. Build-, Toolchain- und Test-Kompendium

### Windows (MSVC)
```cmd
build_msvc.bat
```
Baut alle Haupt-Executables:
- `strudelforce.exe` (Win32 2.5D Voxel GUI mit Star Wars Crawl und Logbuch-Abspann)
- `strudelforce_game.exe` (50Hz ANSI/ASCII Terminal-Spiel)
- `test_suite.exe` (Konsolidierte Testsuite: 44 Checks)

### GCC / Linux / Make
```bash
make all      # Baut Game, GUI und Testsuite
make test     # Fuehrt test_suite aus
make c89_check# Strict C89 Syntaxprüfung
make verify   # Python-Oracles (tools/selftest.py & verify_core_sim.py)
```

### Amiga 500 Floppy Image (ADF) & Dual-Toolchain
```bash
# 1. Kompilieren des M68K-Binaries
make -C amiga
# 2. Erzeugen des bootfähigen 880 KB Floppy Images
python tools/build_amiga_adf.py
# 3. Starten in WinUAE (mit authentischen Laufwerksgeräuschen)
tools/start_winuae.bat
```

### Python-Verifikation (Zero-GCC)
```bash
python tools/selftest.py
python verify_core_sim.py
```

---

## 7. Arbeitsweise Mensch & KI-Cluster (Methodik & Gesetze)

Das Projekt STRUDELFORCE entstand und wächst durch die enge Zusammenarbeit zwischen dem menschlichen Projektleiter (`tretboot`) und einem Verbund autonomer KI-Agenten:

1. **Human-in-the-Loop Paarprogrammierung:** Der Mensch gibt Richtung, Ästhetik und Architekturvision vor. Die KI-Agenten implementieren, optimieren, verifizieren und testen strikt gegen Spezifikationen und Oracles.
2. **Trace-First Testen (Oracles):** Vor und nach jeder C-Änderung werden Python-Referenz-Oracles (`tools/selftest.py`, `verify_core_sim.py`, `tools/tfmx_oracle.py`, `tools/mod_oracle.py`) ausgeführt. C-Code und Python-Spiegel müssen bit-genau übereinstimmen.
3. **Sandbox-Isolation:** Um Dateiverlust in externen Cloud-Sandboxen zu vermeiden, werden Aufgaben strikt in kompakte, isolierte Arbeitspakete geschnitten (maximal 2-3 Dateien pro Transfer).
4. **Git- & Wiki-Synchronisation:** Das Wissen des Projekts wird redundant im Repository (`docs/`, `PROJECT_BIBLE.md`, `captains_log.md`) und im GitHub Wiki gepflegt.
