# STRUDELFORCE — PROJEKT-BIBEL (Single Source of Truth)

**Projekt-Status:** v0.2 Consolidated Monolith  
**Ziel-Plattform:** Commodore Amiga 500 (512 KB Chip-RAM, OCS, 68000 @ 7.09 MHz) & PC (Win32 / ANSI Terminal / GCC)  
**Letzte Aktualisierung:** 2026-10-28  

---

## 1. Executive Summary & Architektur-Vision

STRUDELFORCE ist ein 50Hz Fixed-Point Retro-Actionspiel (Turrican-Hommage) mit prozeduraler 2.5D-Voxel-Grafik und einer hybriden Sound-Engine (Chris Huelsbeck TFMX, ProTracker MOD und Cleanroom Strudel Live-Coding). 

Die Codebasis wurde von 27 fragmentierten Einzeldateien auf **3 monolithische Kernmodule** und eine saubere Frontend-Schicht konsolidiert (unter 15 C-Dateien):
- **`src/game_logic.c`**: 16.16 Fixed-Point Physik, 3-Leben-System, Waffen, Spawner, Endlos- & Kampagnen-Modus, Livecode-Puffer.
- **`src/soundengine_core.c`**: Universeller Hub für TFMX-Parsing, MOD-Playback, Strudel-Cleanroom AST, Game-SFX, Cyber-Paula Mixer, Soundgodizer und Win32 waveOut.
- **`src/engine_core.c`**: 2.5D Voxel-Szenen-Planer (4 Parallax-Ebenen, Directional Shading, Low-HP Jitter), Amiga OCS/AGA Hardware-Shims und Copper-Intro.
- **`src/strudelforce.h`**: Zentraler Master-Header für alle Subsysteme.

---

## 2. Chronologische Historie

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

---

## 3. Die 3 Eisernen Gesetze von STRUDELFORCE

Jede Codeänderung muss ausnahmslos diesen Gesetzen gehorchen:

### Gesetz 1: Amiga-First Core (`game_logic.c`)
- **Striktes C89/C99:** Standard-C ohne Compiler-Extensions.
- **16.16 Fixed-Point (`fix16_t` / `int32_t`):**
  - `1.0 == 65536` (`1 << 16`).
  - **Null `float` oder `double`** im Gameplay-, Physik- und Simulations-Loop!
- **Null `malloc` / `free`:** Keine dynamische Allokation im Hot-Path. Alle Entitäten (Spieler, Projektile, Gegner, Partikel, Kristalle, Pickups) leben in statisch vorallokierten Ringbuffern.
- **50Hz PAL-Tick:** Jeder Tick entspricht exakt 20 Millisekunden.
- Speicherbudget: Vollständig lauffähig in den **512 KB Chip-RAM** eines Amiga 500.

### Gesetz 2: Kein Minecraft-Look bei den Voxeln (`engine_core.c`)
- **Keine Pixeltexturen:** Voxel dürfen niemals texturiert werden.
- **Reine Amiga-Paletten:** Indizierte 16- bzw. 32-Farben-Paletten (OCS/ECS/AGA).
- **Plastizität durch Face-Shading:** Räumliche Tiefe entsteht durch Directional Face-Shading (abgedunkelte Kanten/Seiten) und Ambient Occlusion.
- **4 Parallax-Ebenen:**
  - Layer 3 (`z = -20`): Deep-Sky & Copper-Verläufe (Amiga Copper-Bänder).
  - Layer 2 (`z = -6`): Mid-Background Ruinensäulen (1/4 Parallax).
  - Layer 1 (`z = 0`): Gameplay-Ebene (Spieler, Gegner, Schüsse, Kristalle, Truhen).
  - Layer 0 (`z = +2`): Vordergrund-Rohre (1.5x Scroll).
- **Interlace-Jitter:** Bei kritischem Leben (`HP < 25%`) wird ein Zeilen-Interlace-Jitter auf ungeraden Scanlines aktiviert.

### Gesetz 3: Cyber-Paula Audio & Reaktivität (`soundengine_core.c`)
- **Synthese-Architektur:** Kombination aus Cleanroom Strudel-Parser, TFMX 50Hz-Makro-Synthese und 8-Stimmen Paula-Mixer (16574 Hz).
- **Hyper-Reaktive Gameplay-Kopplung:**
  - **Waffenwechsel:** Tauscht sofort das Bass-Makro (Spread = Standard Saw, Laser = PWM Sawtooth mit Sub-Bass-Boost, Bounce = FM Pluck mit 50Hz Oktav-Arpeggio).
  - **Schild:** Steuert PWM-Modulationsgeschwindigkeit und Filter-Resonanz ($Q \approx 0.7 \dots 8.5$).
  - **Kritischer Zustand (`HP < 25%`):** Schließt den 1-Pole Low-Pass Filter auf 550 Hz und aktiviert 4-Bit Quantisierung auf Drums/Bässen.
  - **The Great Bath Schatztruhe:** Öffnen der Truhe im Unterwasser-Biom schaltet die Bassline nahtlos auf das originale Tribute-Pattern um:
    `"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`
    und schaltet auf Amiga-Hardware den Hardware-Tiefpassfilter über CIA-A `$BFE001` Pin 1 ein.
- **Nahtloses Live-Coding:** Strudel-Pattern-Änderungen über die ESC-Konsole werden erst am 16-Step-Zyklus-Rand atomar gewechselt (glitchless).

---

## 4. Modul-Übersicht (Post-Defragmentierung)

| Modul | Haupt-Header | Quellcode | Aufgaben & Schnittstellen |
|---|---|---|---|
| Master-Schnittstelle | `strudelforce.h` | — | Alle Typen, Fixed-Point-Makros, Enums, GameState, RenderItems, SFX-IDs. |
| Gameplay-Logik | `strudelforce.h` | `src/game_logic.c` | 16.16 Physik, LCG-RNG, Kollision, Turrican-3-Leben, Spawner, Endlos/Kampagne, Biome-Chunks, Livecode-Puffer. |
| Universal Sound Engine | `strudelforce.h` | `src/soundengine_core.c` | Hub für TFMX-Player, MOD-Player, Strudel-Cleanroom AST, Game-SFX, Cyber-Paula Mixer, Soundgodizer, Win-Audio. |
| Engine Core | `strudelforce.h` | `src/engine_core.c` | 2.5D Voxel-Planer, 4 Parallax-Layer, Directional Face Shading, Amiga Copper/Bitplane-Shims, Copper-Intro. |
| GUI Frontend | `strudelforce.h` | `src/gui_main.c` | Win32/GDI 2.5D GUI (`strudelforce_v0.1.exe`), Menü Turrican-2-Stil, Loading-Screen, Jukebox, TFMX-Explorer. |
| Terminal Frontend | `strudelforce.h` | `src/game_main.c` | 50Hz ANSI/ASCII Terminal-Spiel (`strudelforce_game.exe`), Headless Bot (`--demo 2500`), WAV-Dumper. |
| PC Raylib Frontend | `strudelforce.h` | `src/main_pc.c` | 1280x720 Raylib 2.5D Renderer (optional). |
| Integrationstest-Suite | `strudelforce.h` | `src/test_suite.c` | Headless Test aller Module: 2500 Ticks LCG-Determinismus, Kampagne, Strudel-AST, Audio-Hub, Game-SFX, SfMixer. |

---

## 5. Audio-Direktive & Master Sample Bank (Für das Cloud-Kompositions-Modell)

Für das nächste KI-Modell, das den interaktiven OST im Chris-Huelsbeck-Stil komponiert:

1. **Master Sample Bank Extraktion:**
   - Erstelle eine einzige, kompakte Instrumenten-Datei (`master_sample_bank.sam`), die von allen Leveln gemeinsam genutzt wird.
   - Referenz-Dateien liegen in `assets/audio_reference/`:
     - Turrican 2 Load (`turrican2_load.*`)
     - Turrican 2 World 1 Dessert (`turrican2_world1.*`)
     - Turrican 2 World 2 Cave (`turrican2_world2.*`)
     - Wing Commander MODs (`ref1_wing_commander_patrol.mod`, `ref2_wing_commander_combat.mod`)
     - Switch Angel Strudel Tracks (`switch_angel_tracks.strudel`)

2. **Strikte Demoscene-Kompressionsregeln (.adf Budget: 200–400 KB):**
   - **Single-Cycle Waveforms für Bässe und Leads:** Nutze 32–256 Byte Loops (Sawtooth, PWM Pulse, FM Pluck). Modulation erfolgt rein zur Laufzeit über 50Hz-Makros.
   - **Keine vorgerenderten Effekte:** Kein Hall, Chorus oder Delay in den Samples! Nutze Tracker-Befehle, Arpeggio `0xy`, Paula-Portamento und CIA-A Tiefpass-Filterung.
   - **4-Bit ADPCM oder Delta-Kompression für Drums:** Minimale Transienten für Kicks/Snares/Hi-Hats.
   - **Sampleraten-Budgetierung:** Paula-PAL-Maximum ist 28 kHz. Standard-Instrumente laufen zwischen 11 kHz und 16.574 kHz.

3. **Biome & Stimmungen:**
   - **Chrome Tempest (Biom 0):** 125–136 BPM, Sci-Fi Metal/Synth-Pop (Turrican 2 World 1 Feeling).
   - **The Great Bath (Biom 1):** 110–120 BPM, Unterwasser-Kathedrale, D-50 Strings, FM Bells, Tribute-Akkord bei Truhenfund.
   - **Mechanical Hive (Biom 2):** 136–140 BPM, Alien-Maschinenwelt, Acid/Trance, detuned Supersaws (Switch Angel Ästhetik).

---

## 6. Gameplay, Biome & Steuerung

### Modi & Wellen
- **Endlos-Modus (`MODE_ENDLESS`):** Unendliche Wellen, kontinuierliche Level-Ups, deterministischer Highscore-Jagd.
- **Kampagnen-Modus (`MODE_CAMPAIGN`):** 5 Level à 3 Wellen. Feste Biome-Progression. Nach jeder 3. Welle Level-Bonus (`STATE_LEVEL_CLEAR`, 150 Ticks). Nach Level 5: Sieg (`STATE_VICTORY`).

### Leben & Pickups (Turrican-Stil)
- 3 Startleben (bis maximal 9 Leben).
- Respawn nach Treffer mit 100 Ticks Unverwundbarkeit (Blinken).
- 2 Extraleben-Pickups pro Welle an den Außenrändern der Welt (belohnen Exploration).

### Waffen-Arsenal
1. **Spread-Fächer:** 1 bis 7 Projektile in horizontalem Fächer (Pellets). Schnelle Kadenz.
2. **Laser-Strahl:** Sofortiger Durchschlag (Piercing ab Stufe 3). 2 bis 22 Voxel-Pixel Strahlbreite.
3. **Bounce 2.0 (Geometrisches Prisma):**
   - L1: 45° Reflexion.
   - L2: Geradeaus-Schuss zusätzlich.
   - L3: 90° Vertikal-Schüsse zusätzlich.
   - L4: Wandkontakt spaltet Projektil in zwei 90°-Sekundärwinkel.
   - L5: Smart-Ricochet (Wandkontakt lenkt direkt auf den nächsten Gegner).

### Steuerung
- **PC-Tastatur:** `WASD` / Pfeiltasten = Bewegen, `W` / Leertaste = Sprung, `S` = Morph-Ball, `STRG` / `Z` = Feuer, `ALT` / `X` = Haltestrahl, `C` = Waffe wechseln, `T` = Schatztruhe öffnen, `ESC` = Live-Code-Konsole / Pause.
- **Gamepad (XInput):** Stick / D-Pad = Bewegen, `A` = Feuer, `X` = Haltestrahl, `B` = Morph-Ball, `RS` = Waffe wechseln, `Start` = Menü.

---

## 7. Build- und Test-Kompendium

### Windows (MSVC)
```cmd
build_msvc.bat
```
Baut alle 3 Haupt-Executables:
- `strudelforce_game.exe` (50Hz ANSI/ASCII Terminal-Spiel)
- `strudelforce_v0.1.exe` (Win32 2.5D Voxel GUI)
- `test_suite.exe` (Headless Integrationstest)

### GCC / Linux / Make
```bash
make all      # Baut Game, GUI und Testsuite
make test     # Fuehrt test_suite aus
make c89_check# Strict C89 Syntaxprüfung
make verify   # Python-Oracles (tools/selftest.py & verify_core_sim.py)
```

### Python-Verifikation (Zero-GCC)
```bash
python tools/selftest.py
python verify_core_sim.py
```

---

## 8. Asset-, Lokalisierungs- & Story-Pipeline (Kiwi Export)

Aus dem erfolgreichen Kiwi-Cloud-Lauf (2026-10-28) wurden folgende Bausteine in die Root integriert:

### Lokalisierung & Text-Engine
- `assets/lang.json`: Strukturierte DE/EN-Texte für Menüs, HUD und Spiel-Status.
- `tools/build_lang.py`: Generiert zur Compile-Zeit `src/localization.h` mit statischen Text-Tabellen (O(1)-Lookup, Zero-Malloc).

### Balancing & Level-Spezifikation
- `assets/balancing.json`: Exakte Physik- und Level-Balancing-Werte pro Biom (Plattform-Abstände, Gravitation, Reibung, Gegner-Parameter).
- `assets/story.json`: 4-Kapitel-Story-Script ("The Cyber Odyssey") mit Dialogzeilen und Biom-Progression.
- `assets/level_01.json`: Chunks und Balancing für Level 01 Chrome Tempest.

### Soundtrack-Konzeptalben & TFMX-Spezifikationen
- `assets/chapter_1_awakening.tfmx.json` bis `chapter_4_fortress.tfmx.json`: Detaillierte Album- und Track-Architektur pro Kapitel.
- `assets/track_highscore.tfmx.json` & `assets/track_final_phase.tfmx.json`: Multi-Section TFMX-Spezifikationen (u.a. 5-Satz "Bohemian Rhapsody"-Struktur).

### Asset-Packing & VFS-Tooling
- `tools/asset_packer.py`: Packt alle JSON/Audio-Assets deterministisch in `assets.dat` und generiert `src/assets_index.h` (`AssetID`-Enum).
- `tools/github_exporter.py`: Export-Packer für saubere Zip-Archive.
- `KIWI_SURVIVAL_GUIDE.md`: Das Überlebens-Handbuch für Cloud-Sandboxen (Zero-Chat-Dump, Mega-Atomic-Calls, Zero-Malloc, C89/C99 Dual-Target).


