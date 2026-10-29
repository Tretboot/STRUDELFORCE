# STRUDELFORCE v0.3.1 – Release Notes

**Release-Datum:** 29. Oktober 2026  
**Ziel-Plattformen:** Commodore Amiga 500 (OCS/ECS, 512KB Chip-RAM, Kickstart 1.3) & Windows PC (x64)  
**Floppy-Image:** `strudelforce.adf` (880 KB Standard OFS, Volume: `StrudelOS`)  
**Architektur:** C89/C99 Monolith mit 16.16 Fixed-Point Physik, ThePlayer 6.1a Paula DMA Music & Cyber-Paula Hub  
**Verifikation:** 44/44 Integrationstests PASS | Selftest 0 Failures | Deterministischer Score: 14700 (2500 Ticks)  

---

## 🔥 Highlights der Version 0.3.1

### 1. Bootfähige Amiga 500 Floppy Disk (`strudelforce.adf`)
- **Echte Amiga-Floppy (880 KB OFS):** Erstellt mit `amitools.tools.xdftool` und Kickstart 1.3 Bootblock (`boot1x`) sowie `s/startup-sequence`. Bootet direkt in das Spiel auf Amiga 500, WinUAE und RetroArch PUAE.
- **Volume-Label `StrudelOS`:** Authentische AmigaDOS-Diskette mit CLI-Boot-Banner.
- **Belegter Speicherplatz:** Nur ca. 280 KB Daten auf der Diskette – Platz für Spiel und Module.

### 2. Amiga-Boot-Fix & Bare-Metal Stabilität
- **ExecBase & dos.library Fix:** Initialisierung von `SysBase` (Offset 4) vor allen Bibliotheksaufrufen behoben, wodurch die Kickstart-Fehlermeldung *"Software error - task held"* vollständig eliminiert wurde.
- **Sichere Chip-RAM Voraballokation:** Doppelpuffer (5 Bitplanes @ 320x256) und Copper-Liste werden vor dem System-Takeover regulär über AmigaOS `AllocMem(MEMF_CHIP)` allokiert und beim sauberen Beenden mit `ESC` per `FreeMem` freigegeben.
- **Paula Hardware DMA Audio:** ThePlayer 6.1a Replayer empfängt Custom-Chip Basis (`0xDFF000`) in Register `A6` und streamt 4-Kanal Paula Musik synchron zu den 50Hz PAL VBlanks.

### 3. Vollständige Amiga State Machine & Interaktive Credits
- **Amiga State Machine Parität:**
  1. *Cracktro-Intro:* 3D-Wireframe-Vektorwürfel in Bitplanes, Copper-Farbverläufe, Amiga Sinus-Scroller.
  2. *Turrican-2 Hauptmenü:* Großer 2-Ton Titel (Gold über Dunkelrot), Modus-Auswahl (Kampagne 5 Welten vs. Endlos Roguelike), Steuerung und Credits.
  3. *Ladebildschirm:* Authentischer Amiga-Block-Fortschrittsbalken mit Lichtblenden und mehrphasigen Hardware-Statusmeldungen.
  4. *50Hz Gameplay:* 16.16 Fixed-Point Physik, Plattformen, Gegner, Projektile und HUD.
  5. *Credits & Info (`AMI_STATE_CREDITS`):* Flüssiger 50Hz-Vertikal-Scroll mit driftendem Sternenfeld, großen 2-Ton Überschriften und farbkodierten Tags.
- **Interaktive Steuerung im Abspann:**
  - `Pfeil Runter / S`: Schneller scrollen
  - `Pfeil Hoch / W`: Zurück scrollen
  - `Leertaste`: Pause umschalten
  - `Feuer / Enter / ESC`: Zurück zum Hauptmenü

### 4. De-Dimming & High-Contrast Visuals (Amiga & PC)
- **Keine störenden Hintergrundkästen:** Dunkle Rechtecke hinter Texten wurden sowohl auf dem Amiga als auch auf dem PC entfernt.
- **Schwebende Texte:** Menüs, Steuerungsanleitungen und Abspann-Texte schweben direkt über den animierten Hintergründen, dem Copper-Raster und dem Sternenfeld.
- **Vergrößerte 2x Überschriften & Farbkodierung:** Große 2-Ton Titel und dedizierte leuchtende Farben pro Kategorie (`[PROJEKT]`: Gold, `[AUDIO]`: Neonpink, `[CORE]`: Limettengrün, `[AI]`: Cyan, `[HERO]`: Gelb, `[TRIBUTE]`: Orange, `[SCENE]`: Violett).

### 5. Schlanker, kuratierter Soundtrack (~335 KB)
- **Turrican 3 World 1 (Factory):** Tempo-Fix für Chris Hülsbecks `tempo == 0` Subsong-Bug (`speed = 2` statt 5) für volles Arcade-Tempo.
- **BiFi Adventure & Cybersty:** Legendäre Hülsbeck-Tracks und Kaiko-Sound-System-Chiptunes.
- **Software Manager:** Titel-Track mit allen 22 Subsongs.
- **Demoszene & Chiptunes:** *Airwolf 2* und *Ocean Loader*.

---

## 🕹️ Steuerung (Amiga & PC)

| Aktion | Amiga Tastatur / Joystick | PC Tastatur / Gamepad |
|---|---|---|
| **Bewegen** | Joystick Port 2 / WASD / Pfeiltasten | Stick / D-Pad / WASD / Pfeile |
| **Springen** | Oben / W | Leertaste / Gamepad A |
| **Morph-Ball (Kreisel)** | Unten / S | S / Gamepad B |
| **Feuer** | Feuerknopf / Space / Enter | STRG / Leertaste / Gamepad A |
| **Halt-Strahl (Holding Beam)** | 2. Taste / Alt / C | ALT / Gamepad X |
| **Pause / Menü** | ESC | ESC / Start-Taste |

---

## 📦 Release-Dateien

- `strudelforce.adf`: 880 KB Amiga 500 Floppy Disk Image (Bootfähig)
- `strudelforce-v0.3.1-amiga.zip`: ADF + `strudelforce.amiga` 68k Binary + Dokumentation
- `strudelforce-v0.3.1-windows-x64.zip`: `strudelforce.exe` (2.5D GUI) + `strudelforce_game.exe` (Terminal)
- `strudelforce-v0.3.1-source.zip`: Vollständiger Quellcode
- `SHA256SUMS.txt`: Kryptografische Prüfsummen aller Release-Archive
