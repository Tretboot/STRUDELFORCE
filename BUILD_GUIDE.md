# STRUDELFORCE Build & Cross-Compilation Guide

Dieser Leitfaden beschreibt, wie STRUDELFORCE für **Windows**, **Linux** und den **Commodore Amiga (m68k)** aus den Quelltexten gebaut wird.

---

## 1. Systemvoraussetzungen & Quelltext-Architektur

STRUDELFORCE basiert auf einer monolithischen, stark konsolidierten C89/C99-Architektur:
- **`src/game_logic.c`**: 16.16 Fixed-Point Physik, 3 Leben, Waffen, Plattform-Kollision, Endlos- & Kampagnen-Modus.
- **`src/soundengine_core.c`**: TFMX-Player, MOD-Replay, Cyber-Paula Mixer, Game-SFX, Cleanroom Strudel AST.
- **`src/engine_core.c`**: 2.5D Voxel-Szenen-Planer, Zero-Float Amiga OCS/AGA Hardware-Shims, Retro-Intro.
- **`src/gui_main.c`**: Win32/GDI High-Performance Voxel-Frontend mit RetroArch-Shader-Engine, Jukebox & Studio.
- **`src/game_main.c`**: 50Hz ANSI/ASCII Terminal-Spiel (vollständig portabel, Linux & Windows).
- **`src/test_suite.c`**: Konsolidierte Headless-Testsuite (38 automatisierte Checks).

---

## 2. Windows (MSVC Native Build)

### Voraussetzungen
- Windows 10 / 11 (x64)
- Microsoft Visual Studio 2022 (Community oder Build Tools) mit C++-Workload

### Bauen
Öffnen Sie eine Eingabeaufforderung (cmd.exe oder PowerShell) im Projektverzeichnis und führen Sie aus:
```cmd
build_msvc.bat
```
Das Skript findet automatisch Visual Studio, setzt die x64-Umgebung und erzeugt folgende Binaries:
- **`strudelforce.exe`** (und `strudelforce_0.3.exe`): Vollständige 2.5D Voxel GUI mit Shader-Studio, TFMX/MOD-Jukebox, Gamepad-Support (< 1 MB Budget!).
- **`strudelforce_game.exe`**: 50Hz Terminal-Frontend für Konsolen-Gameplay und Demo-Bots.
- **`test_suite.exe`**: 38-Check Headless-Integrationstest.

---

## 3. Linux (Ubuntu / Debian / Arch / Fedora)

Das Terminal-Spiel und die Testsuite sind 100% portables Standard-C99 ohne externe Bibliotheken.

### Voraussetzungen
```bash
sudo apt-get update
sudo apt-get install -y gcc make python3
```

### Bauen der Test-Suite & des Terminal-Frontends
```bash
# 1. Konsolidierte Test-Suite kompilieren & ausführen
gcc -std=c99 -O2 -Isrc src/test_suite.c src/game_logic.c src/engine_core.c src/soundengine_core.c -lm -o test_suite
./test_suite

# 2. 50Hz Terminal-Spiel bauen & starten
gcc -std=c99 -O2 -Isrc src/game_main.c src/game_logic.c src/engine_core.c src/soundengine_core.c -lm -o strudelforce_game
./strudelforce_game

# 3. Headless Bot-Demo mit WAV-Export ausführen
./strudelforce_game --demo 2500 --no-intro --audio output.wav
```

### Python-Oracles & Validierung
```bash
python3 verify_core_sim.py --seed 1234 --ticks 2500
python3 verify_core_sim.py --seed 1234 --campaign
python3 tools/selftest.py
```

---

## 4. Commodore Amiga 500 / 1200 (Motorola 68000)

STRUDELFORCE hält sich strikt an die **3 Eisernen Gesetze**:
- 100% 16.16 Fixed-Point Arithmetik (`fix16_t` / `int32_t`), **kein einziges Float**.
- Null dynamische Allokation (`malloc`/`free`) im Gameloop.
- Speicherbedarf für 512 KB Chip-RAM optimiert.

### Option A: VS Code Extension (BartmanAbyss amiga-debug)
1. Installieren Sie in Visual Studio Code die Extension `BartmanAbyss.amiga-debug`.
2. Die Toolchain (`m68k-amiga-elf-gcc`, `elf2hunk`, `gnumake`) befindet sich standardmäßig in:
   `~/.vscode/extensions/bartmanabyss.amiga-debug-*/bin/win32/opt/bin`
3. Konfigurieren Sie den Kickstart-1.3-ROM-Pfad in `.vscode/settings.json`:
   ```json
   "amiga.rom-paths.A500": "F:\\Emulators\\Amiga\\Kickstart1.3.rom"
   ```
4. Drücken Sie in VS Code `Strg + Umschalt + B` (Task: `compile`).

### Option B: Bebbo / jeffv03 amiga-gcc (Docker / Linux)
Mit einer installierten `m68k-amigaos-gcc`-Toolchain:
```bash
# C89-Syntax-Linter für den Amiga-Core
m68k-amigaos-gcc -m68000 -O2 -fomit-frame-pointer -Wall -Isrc -DM68K -DAMIGA -std=gnu89 -fsyntax-only \
    src/game_logic.c src/engine_core.c src/soundengine_core.c

# Amiga ADF Disk-Image bauen (benötigt amitools):
python3 -m pip install amitools
make amiga_adf M68K_CC=m68k-amigaos-gcc
```

### Option C: GitHub Actions CI
Jeder Git-Push mit einem Tag `v*` (z. B. `v0.3.0`) triggert automatisch den Workflow `.github/workflows/amiga.yml`, der eine frische GCC-68k-Toolchain instanziiert und `strudelforce.adf` als Build-Artefakt bereitstellt.

---

## 5. Roadmap v0.4.0 (Multi-Platform Meilensteine)

Für Version 0.4.0 sind folgende Erweiterungen geplant:
1. **Nativer Linux 2.5D GUI Client**: Portierung des Renderers von Win32/GDI auf **SDL2** (mit ALSA/PulseAudio-Soundbackend), um das grafische Fenster und alle RetroArch-Shader nativ unter Linux auszuführen.
2. **Direkter ADF/LHA-Release-Download**: Bereitstellung vorkompilierter `.adf`-Floppy-Disks und `.lha`-Pakete direkt im GitHub Release für Emulatoren (WinUAE, FS-UAE, Amiberry) und Original-Hardware.
3. **M68K Hardware Blitter & Copper Listen**: Native Amiga-Bitplane-Generierung direkt auf den OCS/AGA-Chipsatz ohne CPU-Software-Shims.
