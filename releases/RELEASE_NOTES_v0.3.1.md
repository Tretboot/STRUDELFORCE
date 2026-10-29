# 🚀 STRUDELFORCE v0.3.1 – Release Notes & Changelog

[![Amiga 500](https://img.shields.io/badge/Platform-Amiga%20500%20%2F%20PC%20%2F%20Linux-blue.svg)](https://en.wikipedia.org/wiki/Amiga_500)
[![Amiga ADF](https://img.shields.io/badge/ADF-Bootable%20880KB%20OFS-orange.svg)](#)
[![Stable: 0.3.1](https://img.shields.io/badge/Release-v0.3.1-brightgreen.svg)](https://github.com/Tretboot/STRUDELFORCE/releases/tag/v0.3.1)
[![PAL 50Hz](https://img.shields.io/badge/Timing-50Hz%20PAL%20VBlank-green.svg)](#)
[![Fixed Point](https://img.shields.io/badge/Math-16.16%20Fixed--Point%20Zero--Float-orange.svg)](#)
[![Audio](https://img.shields.io/badge/Sound-Cyber--Paula%208--Voice%20TFMX%2BMOD-magenta.svg)](#)
[![Tests](https://img.shields.io/badge/Tests-44%2F44%20PASS-brightgreen.svg)](#)

---

## 📦 Downloads & Pakete

> **Hinweis zur Paketierung:** Alle ZIP-Archive enthalten **ausschließlich die Binaries** (keine Textdateien, Lizenzen oder Dokumente im Archiv). Standalone-Binaries können auch direkt ohne Entpacken heruntergeladen werden.

| Datei | Plattform | Inhalt & Zweck |
|---|---|---|
| 🪟 [`strudelforce-v0.3.1-windows-x64.zip`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/strudelforce-v0.3.1-windows-x64.zip) | **Windows PC (x64)** | **NUR Binaries:** `strudelforce.exe` (2.5D GUI) und `strudelforce_game.exe` (Terminal) |
| 🐧 [`strudelforce-v0.3.1-linux-x64.zip`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/strudelforce-v0.3.1-linux-x64.zip) | **Linux (x64)** | **NUR Binary:** `strudelforce_game` (autarkes 50Hz C99 Terminal-Spiel) |
| 💾 [`strudelforce-v0.3.1-amiga.zip`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/strudelforce-v0.3.1-amiga.zip) | **Amiga 500 (OCS)** | **NUR Floppy-Image:** `strudelforce.adf` |
| 💾 [`strudelforce.adf`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/strudelforce.adf) | **Amiga 500 (OCS)** | Bootfähige 880 KB OFS Floppy Disk (Einzoldownload für WinUAE, FS-UAE, MiSTer, Gotek) |
| 🪟 [`strudelforce.exe`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/strudelforce.exe) | **Windows PC (x64)** | Standalone 2.5D Voxel GUI Executable |
| 🐧 [`strudelforce_game`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/strudelforce_game) | **Linux (x64)** | Standalone Linux x64 Terminal Executable (`chmod +x strudelforce_game`) |
| 📦 [`strudelforce-v0.3.1-source.zip`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/strudelforce-v0.3.1-source.zip) | **Quellcode** | Vollständiges Repository via `git archive` (ohne geschützte Test-Audiofiles) |
| 🔒 [`SHA256SUMS.txt`](https://github.com/Tretboot/STRUDELFORCE/releases/download/v0.3.1/SHA256SUMS.txt) | **Prüfsummen** | Offizielle SHA-256 Hashes aller Assets |

---

## 📋 Changelog (v0.3.0 → v0.3.1)

### ✨ Neue Features & Highlights
- **Echter 3D Star Wars Perspektiv-Crawl ("THE WUG"):**
  - Zero-Float Scanline-Trapezoid Raycaster ($Z = \frac{K}{y - y_{vanish}}$) mit stufenloser horizontaler und vertikaler Verjüngung.
  - Atmosphärischer Space-Horizont-Fade und 2px-Drop-Shadow über dem driftenden Sternenfeld.
  - Vollständig interaktive Steuerung (Warp-Vorlauf, Zurückspulen, Pause, Überspringen per Taste/Gamepad).
- **Interaktiver Abspann mit vollständiger Projekt-Chronik:**
  - 1500+ Zeilen Chronik: Nach den Kapiteln I–IV folgt Kapitel V mit dem **vollständigen Captain's Log** (alle Entwicklungsiterationen).
  - Automatischer Musikwechsel beim Eintritt in Kapitel V auf **Chris Hülsbecks Software Manager Subsong 21**.
  - Manueller Track-/Subsong-Wechsel jederzeit per Taste `M` bzw. Gamepad-Taste `Y`.
- **Erste bootfähige Amiga 500 Floppy Disk (`strudelforce.adf`):**
  - Valides 880 KB Standard OFS Floppy Disk Image mit Bootblock (`StrudelOS`) und `s/startup-sequence`.
  - Bootet auf Standard-Amiga-500 (Kickstart 1.3, 512 KB Chip-RAM), WinUAE, FS-UAE und RetroArch PUAE.
  - 6-Phasen State-Machine in `amiga/main.c`: 3D-Wireframe-Cracktro, Turrican-2 Menü, AmigaShell Ladebildschirm, 3D Story Crawl, 50Hz OCS Bitplanes, Credits.
  - ThePlayer 6.1a 4-Kanal Paula DMA Musikwiedergabe im beschreibbaren Datensegment.
- **🐧 Linux x64 Support & Native CI Pipeline:**
  - `strudelforce_game` kompiliert und läuft fehlerfrei auf Linux x64 (reines C99 ohne externe Abhängigkeiten).
  - Automatischer GCC-Build, Integrationstest, Selftest und Paketierung im GitHub Actions CI Workflow.

### 🎨 Visuals & Bedienung
- **De-Dimming & High-Contrast Visuals:**
  - Beseitigung aller dunklen Hintergrundboxen: Texte und Menüs schweben direkt über den Bitplanes und Sternenfeldern.
  - 2x skalierte 2-Ton Überschriften und einheitliche Farbkodierung für Kategorien (`[PROJEKT]`, `[AUDIO]`, `[CORE]`, `[AI]`, `[HERO]`, `[TRIBUTE]`, `[SCENE]`).
- **Verbose AmigaShell Ladebildschirm:**
  - Interaktives Shell-Terminal (`AmigaShell - StrudelOS v0.3.1 CLI`) mit realem Initialisierungsfortschritt und wandernden Lichtblenden im Ladebalken.

### 🎵 Audio-Engine & Sound
- **Chris Hülsbeck Turrican 3 Tempo-Fix:**
  - Fix für den bekannten Hülsbeck-Subsong-Bug (`tempo == 0`), der das Stück zuvor in Zeitlupe abspielte (`speed = 2` statt 5 für volles Arcade-Tempo).
- **Kuratierter Soundtrack (~335 KB):**
  - Turrican 3 World 1 (Factory), BiFi Adventure, Cybersty, Mad TV, Software Manager (22 Subsongs) und Chiptunes (Airwolf 2, Ocean Loader).

---

## 🎮 Plattform-Übersicht & Schnellstart

### 1. Windows PC (`strudelforce.exe`) – *Empfohlene Hauptversion*
- Entpacke `strudelforce-v0.3.1-windows-x64.zip` (oder starte direkt `strudelforce.exe`).
- Keine Installation und keine externen DLLs erforderlich.
- Native 1280x720 Win32/GDI 2.5D Voxel Engine mit Paula Audio Hub und Soundgodizer DSP (F5).

### 2. Linux x64 (`strudelforce_game`)
- Entpacke `strudelforce-v0.3.1-linux-x64.zip` (oder lade `strudelforce_game` herunter).
- Ausführbar machen und starten:
  ```bash
  chmod +x strudelforce_game
  ./strudelforce_game
  ```
- Unterstützt auch Headless-Modus: `./strudelforce_game --demo 2500 --no-intro --no-sound`

### 3. Commodore Amiga 500 (`strudelforce.adf`) – *Preview & Test-Version*
- **Echte Hardware:** ADF auf Floppy Disk schreiben oder Gotek Floppy-Emulator (FlashFloppy) nutzen.
- **WinUAE / FS-UAE:**
  - Modell: Standard Amiga 500 (OCS)
  - Kickstart: 1.3 (oder 2.04+)
  - Chip-RAM: 512 KB (oder 1 MB)
  - Floppy Drive DF0: `strudelforce.adf` einlegen und starten.
- *Hinweis:* Die Amiga-Version stellt das technische OCS-Fundament (VBlank, Bitplanes, Paula DMA) dar; das erweiterte Kampagnen-Balancing wird in v0.3.2 fortgeführt.

---

## 🕹️ Steuerung

| Aktion | Amiga Tastatur / Joystick | PC / Linux Tastatur | Gamepad (XInput / USB) |
|---|---|---|---|
| **Bewegen** | Joystick Port 2 / WASD / Pfeiltasten | WASD / Pfeiltasten | Linker Stick / D-Pad |
| **Springen** | Oben / W | Leertaste | Taste A |
| **Morph-Ball (Kreisel)** | Unten / S | Taste S | Taste B |
| **Feuer** | Feuerknopf / Space / Enter | STRG / Leertaste | Taste A |
| **Halt-Strahl (Holding Beam)** | 2. Taste / Alt / C | ALT / Taste C | Taste X |
| **Waffe wechseln** | Taste W | Taste C | Rechter Schulterbutton (RB) |
| **Story Crawl Tempo** | Pfeiltasten Oben / Unten (W / S) | Pfeiltasten Oben / Unten (W / S) | D-Pad Oben / Unten |
| **Story Crawl Pause** | Taste P | Taste P / Leertaste | Start-Taste |
| **Story Crawl Skip** | Feuerknopf / Space / Enter / ESC | Space / Enter / STRG / ESC | Taste A / Start-Taste |
| **Musik-Track / Subsong** | Taste M | Taste M | Taste Y |
| **Soundgodizer DSP (PC)** | — | Taste F5 | — |
| **Menü / Pause** | ESC | ESC | Start-Taste |
