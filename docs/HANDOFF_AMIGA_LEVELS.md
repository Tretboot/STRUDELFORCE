# STRUDELFORCE v0.3.1 — Handoff Briefing: Amiga-Port Test & Level/Sprite Design

**Datum:** 2026-10-29  
**Version:** 0.3.1  
**Ziel:** Amiga 500 ADF-Testlauf (CI-basiert) und Vorbereitung der nächsten Phase (OCS Sprites & Level Design).

---

## 1. Amiga ADF Build ohne Release anstoßen

Im GitHub Repository existiert der Workflow `.github/workflows/amiga.yml`.
Er ist so konfiguriert, dass er **jederzeit völlig ohne GitHub Release** ausgeführt werden kann:

### Methode A: Manuell per GitHub UI (Empfohlen)
1. GitHub Repository im Browser öffnen.
2. Den Reiter **Actions** aufrufen.
3. In der linken Seitenleiste den Workflow **`amiga-adf`** auswählen.
4. Rechts auf den Button **"Run workflow"** klicken (Branch: `main`).
5. Nach ca. 2–3 Minuten ist der Build fertig. Unter **Artifacts** steht das ZIP **`strudelforce-amiga`** zum Download bereit. Es enthält:
   - `strudelforce.adf`: Die fertige bootfähige Amiga 500 Diskette mit dem echten C-Spiel und der Musik.
   - `strudelforce.amiga`: Das kompilierte 68k AmigaOS Executable.

### Methode B: Automatisch durch Push auf `main`
- Jeder Commit/Push auf den `main`-Branch, der Quelldateien (`src/*.c`, `src/*.h`, `Makefile`, etc.) ändert, triggert den Workflow automatisch.
- Ein **Release** wird nur dann publiziert, wenn ein Tag mit `v*` (z.B. `v0.3.1`) gepusht wird. Reguläre Workflow-Runs oder Branch-Pushes erstellen **keinen** Release, sondern nur die Test-Artefakte!

---

## 2. Warum war im lokalen ADF bisher nur der Stub?

- **Hintergrund:**
  Auf dem lokalen Windows-Entwicklerrechner ist kein nativer Motorola 68000 Cross-Compiler (`m68k-amigaos-gcc` oder `m68k-amiga-elf-gcc`) installiert.
  Daher hat der lokale Packer [`tools/build_amiga_adf.py`](file:///f:/AgentHome/STRUDELFORCE_Master/tools/build_amiga_adf.py) als Fallback einen gültigen 40-Byte AmigaOS Hunk-Stub eingesetzt, um die Diskettengeometrie, Bootblöcke und Musik-Dateien auf 880 KB zu prüfen.
- **In der CI:**
  Die GitHub Actions CI läuft auf `ubuntu-24.04`, nutzt die echte Toolchain `/opt/amiga/bin/m68k-amigaos-gcc` und ruft:
  ```bash
  make amiga_bin M68K_CC=/opt/amiga/bin/m68k-amigaos-gcc
  make amiga_adf M68K_CC=/opt/amiga/bin/m68k-amigaos-gcc
  ```
  Hierbei wird der **vollständige C-Code** (`AMIGA_SRCS = src/amiga_hw.c src/core_sim_sf.c src/voxel_render.c src/biomes.c src/tfmx_player.c src/amiga_music.c`) zu einem echten 68000-Maschinencode-Binary gebunden!

---

## 3. Aufgaben für den nachfolgenden Agenten

### Aufgabe 1: Amigaport Test (Emulator)
- Lade das von der CI gebaute `strudelforce.adf` herunter.
- Teste die ADF in **WinUAE**, **FS-UAE** oder per **Vamos**:
  - Konfiguration: Standard **Amiga 500 (Kickstart 1.3, OCS, 512 KB Chip-RAM)**.
  - Prüfe:
    1. Boot-Vorgang via `s/startup-sequence`.
    2. Titel-Bildschirm ("A TURRICAN TRIBUTE FOR THE AMIGA 500").
    3. Paula TFMX-Musik-Streaming (Voice 0, 16574 Hz) der 4 kuratierten Tracks (`M`-Taste wechselt Track).
    4. Start ins Gameplay via Fire-Button.
    5. Voxel-Rendering auf 6 Bitplanes und 50Hz VBlank-Timing.

### Aufgabe 2: Level- & Kampagnen-Design (Level Kit v2)
- Aus `KiwiReste/` wurden **197 Chunks und 37 validierte Levels** geborgen (`levels/levels_v2.json` und `levelkit/lv2.py`).
- Nutze das Tool `levelkit/lv2.py`, um neue Welten und Biome zu testen:
  ```bash
  python levelkit/lv2.py bot 0    # Testet Level 0 mit dem integrierten Physik-Bot
  python levelkit/lv2.py list     # Listet alle 37 Chunks/Levels auf
  ```
- Nächster Schritt: Die Kampagnen-Progression in `src/game_logic.c` mit den Levels 0 bis 36 feinschleifen.

### Aufgabe 3: Sprite-Design & OCS Hardware-Sprites
- In `src/sprite_data.h` und `ref/` liegen die aus den Turrican-2-Sheets extrahierten 16-Farben-Sprite-Daten.
- Erweitere die Bitplane-Render-Pipeline in `src/amiga_hw.c`, um OCS Hardware-Sprites für Spieler und Schüsse zu nutzen (spart Blitter-/CPU-Zeit beim Bitplane-Schreiben).

---

## 4. Status-Check & Befehlsübersicht

| Test / Task | Befehl |
|---|---|
| Windows MSVC Build | `build_msvc.bat` |
| Testsuite (44 Checks) | `test_suite.exe` |
| Core-Sim Determinismus | `python verify_core_sim.py` |
| Selftest & Oracles | `python tools/selftest.py` |
| Lokales ADF-Packen | `python tools/build_amiga_adf.py` |
| Level-Kit Runner | `python levelkit/lv2.py bot 0` |

