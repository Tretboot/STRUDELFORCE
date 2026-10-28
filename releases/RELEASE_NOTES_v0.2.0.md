# STRUDELFORCE v0.2.0 — Consolidated Monolith & Cyber-Paula Edition

> *"Amiga 500 meets Cyber-Paula, 2.5D Voxel Action and Chris Hülsbeck's Legendary Sound Engine."*

---

## 🚀 Highlights & New Features in v0.2.0

### 1. 🏗️ Consolidated Monolithic Architecture (Amiga-First)
* **Single Source of Truth:** Consolidated from 27 fragmented files into 3 high-performance monolithic core modules:
  * `src/game_logic.c`: 16.16 fixed-point physics, 3-lives system, respawn invulnerability, weapons, enemy spawning, and campaign progression.
  * `src/soundengine_core.c`: Universal audio hub combining TFMX 1.5/Pro replay, ProTracker MOD playback, TidalCycles/Strudel Cleanroom AST, and 8-voice Cyber-Paula mixer.
  * `src/engine_core.c`: 2.5D directional face-shading voxel scene planner, Amiga OCS/AGA hardware registers, and copper intro.
* **Strict The 3 Iron Laws:**
  * **Zero-Float & Zero-Double:** 100% 16.16 fixed-point arithmetic (`fix16_t`) across physics, renderer, and simulation loops.
  * **Zero-Malloc in Hot Path:** All entities live in statically allocated pools and ringbuffers.
  * **50Hz PAL Timing:** 20ms fixed timestep, fully compliant with 512 KB Chip-RAM constraints on real Amiga 500 hardware.

### 2. 🏛️ Multi-Tier 2.5D Platforms & Turrican-2 Verticality
* Added multi-elevation platform architectures tailored to each campaign biome:
  * **Chrome Tempest:** Staggered industrial catwalks at heights 5, 11, and 17.
  * **The Great Bath:** Ancient stepped aqueducts and a high temple altar at height 15.
  * **Mechanical Hive:** Biomechanical girders and elevated alien structures.
  * **Volcanic Skies & Cyber Fortress:** Magma basalt ledges and multi-tier battlements.
* Extruded 2.5D voxel slabs with support pillars and glowing neon energy edges (Cyan, Aquamarine, Magenta, Orange, Gold).
* **The Great Bath Shrine Treasure Chest:** Jump onto the altar platform to open the golden chest, triggering a sparkling particle fountain, CIA-A hardware filter, and the original Great Bath musical tribute!

### 3. 🎵 Dynamic Chris Hülsbeck TFMX & MOD Soundtracks
* **Official Turrican 2 Campaign Soundtracks:**
  * Level 1 (Chrome Tempest) → *Turrican 2 – World 1 – Dessert*
  * Level 2 (The Great Bath) → *Turrican 2 – World 2 – Cave*
  * Level 3 (Mechanical Hive) → *Turrican 2 – World 4 – Factory*
  * Level 4 (Volcanic Skies) → *Turrican 2 – World 3 – Flight*
  * Level 5 (Cyber Fortress) → *Turrican 2 – World 5 – Aliens*
* **In-Game Music Switcher:** Press **`M`** / **`N`** on your keyboard (or **`Y`** on your gamepad) anytime during gameplay to skip tracks with seamless 8-voice Paula playback.
* **Retro Music Toast & HUD:** Displays real-time song title and format (`♫ TRACK: <Name> [TFMX]`) in the HUD and top banner.

### 4. 🎛️ Soundgodizer (PC Audio Enhancement)
* Cycle presets anytime with **`F5`**:
  * `AUS` (Clean Amiga 16574 Hz Paula Output)
  * `BASS` (60 Hz Sub-Bass Punch)
  * `WARM` (Analog Low-Pass & Paula Filter Warmth)
  * `HQ OVERDRIVE + WIDE` (Analog Drive & Haas 128-Sample Stereo Expander)

### 5. 🎮 Dual Frontends: GUI & ANSI Terminal
* **`strudelforce.exe` (Win32 2.5D Voxel GUI):** Native 1280x720 @ 60 FPS (50Hz PAL pacing) with zero external DLL dependencies.
* **`strudelforce_game.exe` (50Hz ANSI/ASCII Terminal):** Play directly in Windows Terminal, Linux console, or run headless bot demos (`--demo 2500`).

### 6. 🥝 Kiwi Cloud Handoff & Automated Reconcile Pipeline
* **Lite Package (`tools/build_kiwi_handoff.py --lite`):** 4.27 MB package containing all code, assets, tools, and 53 top TFMX tracks.
* **Cloud Bootstrap (`tools/kiwi_bootstrap.py`):** Explicit directory creation and sha256 verification in cloud sandboxes.
* **Automated Reconcile Tool (`tools/kiwi_reconcile.py`):** Automated safety diff analysis, Zero-Float scanning, and timestamped backups for loss-free roundtrips.

---

## 🕹️ Controls

| Keyboard | Controller (XInput) | Action |
|---|---|---|
| **W, A, S, D** / **Arrows** | Left Stick / D-Pad | Move Ship / Menu Navigation |
| **Space** | Up / Jump | Jump / Platform Leap |
| **CTRL** | **A** Button | Primary Weapon (Spread, Laser, Bounce) |
| **ALT** | **X** Button | 360° Surround Holding Beam |
| **C** | Right Shoulder (RB) | Cycle Weapons |
| **S** | **B** Button | Morph-Ball Mode |
| **T** | Walk into Chest | Open Ancient Chest (*The Great Bath*) |
| **M** / **N** | **Y** Button | Next Soundtrack (TFMX / MOD Jukebox) |
| **F5** | — | Toggle Soundgodizer Presets |
| **ESC** | Start Button | Pause / Return to Main Menu |

---

## 🔬 Test Suite & Determinism
* **Consolidated Test Suite (`test_suite.exe`):** **32 PASS, 0 FAIL**
* **Integrated CLI Suite (`strudelforce.exe --test`):** **6 PASS, 0 FAIL**
* **Deterministic Simulation:** 2500-tick endless oracle test verified bit-identical:
  `Score=35150, Kills=191, Bounces=42047, Lives=9`
* **Single Standalone Executable:** The release package now contains exclusively **`strudelforce.exe`** (no extraneous test/alias binaries). It supports `-headless`, `--demo [ticks]`, and `--test` directly from the command line!

---

## 🔐 SHA-256 Checksums

```text
aa6378e4f240aa6007420089fcacb86c99b38b5b5f5f026a73edcde8975699be  strudelforce-v0.2.0-windows-x64.zip
52c93b56057d1a8f0f52af61eac2596a4056a1c98458e74f05dc150cb5d267a4  strudelforce.exe
```

---

## 💖 Acknowledgments & Dedication
Dedicated to **Chris Hülsbeck**, **Manfred Trenz**, **Rainbow Arts**, **Factor 5**, and the golden demoscene era of the **Commodore Amiga 500**.

