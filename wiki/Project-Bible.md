# Projekt-Bibel (Single Source of Truth)

**Projekt-Status:** v0.3.1 Full Parity Monolith  
**Ziel-Plattform:** Commodore Amiga 500 (512 KB Chip-RAM, OCS) & PC (Win32 GUI / ANSI Terminal / GCC / MSVC)  

---

## 1. Die 3 Eisernen Gesetze von STRUDELFORCE

Jede Codeänderung im Projekt muss ausnahmslos diesen drei Gesetzen folgen:

### Gesetz 1: Amiga-First Core (`game_logic.c` / `core_sim.c`)
- **Striktes C89/C99:** Reines Standard-C ohne Compiler-Extensions oder POSIX-Zwang im Core.
- **16.16 Fixed-Point Arithmetik (`fix16_t` / `int32_t`):**
  - `1.0` entspricht `65536` (`1 << 16`).
  - **Null `float` oder `double`** im gesamten Gameplay-, Physik- und Simulations-Loop!
- **Null `malloc` / `free`:** Keine dynamische Speicherallokation im Hot-Path. Alle Entitäten (Spieler, Projektile, Gegner, Partikel, Kristalle, Pickups) leben in statisch vorallokierten Ringbuffern.
- **50Hz PAL-Tick:** Die Simulationsfunktion berechnet exakt 20 Millisekunden (1/50 Sekunde).
- Das gesamte Kern-System muss ohne Änderungen direkt in **512 KB Chip-RAM** eines Amiga 500 ausführbar sein.

### Gesetz 2: Kein Minecraft-Look bei den Voxeln (`engine_core.c` / `voxel_render.c`)
- **Keine Pixeltexturen:** Voxel dürfen niemals texturiert werden!
- **Reine Amiga-Farbpaletten:** Ausschließlich indizierte 16- bzw. 32-Farben-Paletten pro Biom (OCS/ECS/AGA).
- **Plastizität durch Face-Shading:** Räumliche Tiefe entsteht rein durch Directional Face-Shading (abgedunkelte Kanten/Seiten) und Ambient Occlusion.
- **4 Parallax-Ebenen:**
  - Layer 3 (`z = -20`): Deep-Sky & Copper-Verläufe (Amiga Copper-Bänder).
  - Layer 2 (`z = -6`): Mid-Background Ruinensäulen (1/4 Parallax).
  - Layer 1 (`z = 0`): Gameplay-Ebene (Spieler, Gegner, Schüsse, Kristalle, Truhen).
  - Layer 0 (`z = +2`): Vordergrund-Rohre (1.5x Scroll).
- **Interlace-Jitter:** Bei niedrigem Leben (`HP < 25%`) wird ein Amiga-Zeilen-Interlace-Jitter auf ungeraden Scanlines aktiviert.

### Gesetz 3: Cyber-Paula Audio & Reaktivität (`soundengine_core.c` / `amiga/main.c`)
- **Synthese-Architektur:** Kombination aus Cleanroom Strudel-Parser, TFMX 50Hz-Makro-Synthese und Paula-Mixer.
- **Hyper-Reaktive Gameplay-Kopplung:**
  - **Waffenwechsel:** Tauscht sofort das Bass-Makro (Spread = Standard Saw, Laser = PWM Sawtooth mit Sub-Bass-Boost, Bounce = FM Pluck mit 50Hz Oktav-Arpeggio).
  - **Schild:** Steuert dynamisch die PWM-Modulationsgeschwindigkeit und Filter-Resonanz ($Q \approx 0.7 \dots 8.5$).
  - **Kritischer Zustand (`HP < 25%`):** Schließt den 1-Pole Low-Pass-Filter auf 550 Hz und aktiviert 4-Bit Quantisierung auf Drums/Bässen.
  - **The Great Bath Schatztruhe:** Öffnen der Truhe im Unterwasser-Biom schaltet die Bassline nahtlos auf das originale Tribute-Pattern um:
    `"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`
    und aktiviert auf echter Amiga-Hardware den Hardware-Tiefpassfilter über CIA-A `$BFE001` (Pin 1).
- **Nahtloses Live-Coding:** Pattern-Änderungen über die ESC-Konsole werden erst am 16-Step-Zyklus-Rand atomar gewechselt (glitchless).

---

## 2. Modul-Architektur

- **`src/game_logic.c`**: 16.16 Fixed-Point Physik, 3-Leben-System, Waffen (Spread, Laser, Bounce 2.0), Spawner, Endlos- & Kampagnen-Modus.
- **`src/soundengine_core.c`**: Universeller Hub für TFMX-Parsing, MOD-Playback, Strudel-Cleanroom AST, Game-SFX, Cyber-Paula Mixer, Soundgodizer und Win32 waveOut.
- **`src/engine_core.c`**: 2.5D Voxel-Szenen-Planer (Zero-Float), Amiga OCS/AGA Hardware-Shims und Copper-Intro.
- **`src/gui_main.c`**: PC Win32/GDI 2.5D GUI mit Star Wars 3D Crawl und Logbuch-Abspann.
- **`amiga/main.c`**: Standalone Amiga 500 Bare-Metal State Machine (Cracktro, Turrican Menü, Ladebalken, Paula DMA Musik).
- **`src/level_engine.c`**: Level Engine v2 mit 197 Chunks und 37 spielbaren Levels.
- **`src/test_suite.c`**: 44 automatisierte Konsolidierungs-Checks.

