# STRUDELFORCE v0.3.1 – Release Notes

**Release-Datum:** 29. Oktober 2026  
**Ziel-Plattformen:** Commodore Amiga 500 (OCS/ECS, 512KB Chip-RAM, Kickstart 1.3) & Windows PC (x64)  
**Floppy-Image:** `strudelforce.adf` (880 KB Standard OFS, Volume: `StrudelOS`)  
**Architektur:** C89/C99 Monolith mit 16.16 Fixed-Point Physik, ThePlayer 6.1a Paula DMA Music & Cyber-Paula Hub  
**Verifikation:** 44/44 Integrationstests PASS | Selftest 0 Failures | Deterministischer Score: 14700 (2500 Ticks)  

---

> [!NOTE] WICHTIGER HINWEIS ZUR ERSTEN AMIGA .ADF TESTVERSION
> `strudelforce.adf` ist die **erste bootfähige Amiga 500 Test- und Preview-Version** auf Diskette!
> Sie bootet zuverlässig auf unbeschleunigter Amiga-Hardware (A500, Kickstart 1.3, 512 KB Chip-RAM), WinUAE und RetroArch PUAE. Sie demonstriert das vollständige 3D-Wireframe-Cracktro, das Turrican-2-Menü, den AmigaShell-Ladebalken, den 3D-Story-Crawl sowie 5-Bitplane OCS-Grafik mit 4-Kanal Paula DMA Musik.
> **Status:** Die Floppy ist bootfähig und stabil; das Gameplay ist in dieser ersten Preview-Stufe als solides technisches Fundament ausgelegt und wird in den kommenden Releases spielerisch weiter ausbalanciert und verfeinert.

---

## 🔥 Highlights der Version 0.3.1

### 1. "THE WUG" – Echter 3D Star Wars Perspektiv-Crawl (Zero-Float Scanline-Raycasting)
- **Echte räumliche 3D-Perspektive:** Die Spiel-Lore scrollt auf einer nach hinten geneigten 3D-Ebene in den Weltraum-Fluchtpunkt. Zeichenbreite und -höhe verjüngen sich stufenlos ($Z = K / dy$), begleitet von kontinuierlicher Verjüngung, atmosphärischem Horizont-Fadeout und 2px-Drop-Shadow über dem driftenden Sternenfeld.
- **Interaktive Steuerung:**
  - `Pfeil Runter / S`: Schneller vorwärts scrollen (Warp-Speed)
  - `Pfeil Hoch / W`: Zurück scrollen / verlangsamen
  - `Leertaste / P`: Pause umschalten
  - `Feuer / Enter / ESC`: Direkt ins Spiel springen
- **Volle Amiga & PC Parität:** Sowohl in `amiga/main.c` (`AMI_STATE_STORY`) als auch in der PC-GUI `src/gui_main.c` (`GUI_STATE_STORY`) bit- und darstellungskonform implementiert.

### 2. Interaktiver Abspann mit vollständigem Captain's Log & Musik-Umschaltung
- **1500+ Zeilen Dokumentation & Chronik:** Nach den regulären Kapiteln I–IV (Chris Hülsbeck Tribute, Amiga Specs, Demoszene-Grüße) folgt nahtlos **Kapitel V: Das vollständige Captain's Log** (alle Iterationen von Iteration 1 bis v0.3.1) direkt im Spielabspann eingebettet.
- **Automatischer Musik- & Subsong-Wechsel:** Beim Übergang in das Captain's Log wechselt die Audio-Engine automatisch zu **Chris Hülsbecks Software Manager Subsong 21** (Amiga Heritage Thema).
- Über die Taste `M` bzw. Gamepad-Taste `Y` kann jederzeit durch weitere Subsongs geschaltet werden.

### 3. Verbose AmigaShell Ladebildschirm mit echtem Feedback
- **Authentischer Amiga CLI Terminal-Look:** Der Ladebalken ist keine statische Dummy-Animation mehr, sondern ein interaktives Shell-Fenster (`AmigaShell - StrudelOS v0.3.1 CLI`).
- **Echte Subsystem-Phasen:**
  1. *StrudelOS Chip-RAM & Hardware Init*
  2. *Copper 5 OCS Bitplanes & 32-Color Palette Setup*
  3. *Level-Engine: 197 Chunks & Biomes Parsed* (`level_load_campaign`)
  4. *Paula DMA: 4-Voice Replayer Sync @ 50Hz*
  5. *Core Sim: 16.16 Fixed-Point Zero-Float Pools* (`core_init`)
  6. *Bereit: Start der Kampagne und "THE WUG" Story!*
- **Laufende Lichtblenden:** Weich wandernde weiße Glanzlichter im Amiga-Block-Balken mit Prozentanzeige (0% bis 100%).

### 4. Erste bootfähige Amiga 500 Floppy Disk (`strudelforce.adf`) & Paula DMA Sound
- **Echte Amiga-Floppy (880 KB OFS):** Erstellt mit `amitools.tools.xdftool` und Kickstart 1.3 Bootblock (`boot1x`) sowie `s/startup-sequence`. Bootet direkt in das Spiel auf Amiga 500, WinUAE und RetroArch PUAE.
- **Paula Hardware DMA Audio:** ThePlayer 6.1a Replayer im beschreibbaren Datensegment (`.data`) für selbstmodifizierenden 68k-Code. Hardware-Takeover aktiviert Paula DMA (`AUD0..AUD3`) und CIA-B Timer-Timing für saubere 4-Kanal-Wiedergabe.
- **Volume-Label `StrudelOS`:** Authentische AmigaDOS-Diskette mit CLI-Boot-Banner.
- **Belegter Speicherplatz:** Nur ca. 320 KB Daten auf der Diskette – voller Spielgenuss innerhalb des 880 KB Limits.

### 4. Vollständige Amiga State Machine & Interaktive Credits
- **Amiga State Machine Parität:**
  1. *Cracktro-Intro:* 3D-Wireframe-Vektorwürfel in Bitplanes, Copper-Farbverläufe, Amiga Sinus-Scroller.
  2. *Turrican-2 Hauptmenü:* Großer 2-Ton Titel (Gold über Dunkelrot), Modus-Auswahl (Kampagne 5 Welten vs. Endlos Roguelike), Steuerung und Credits.
  3. *AmigaShell Ladebildschirm:* Authentischer Amiga-Block-Fortschrittsbalken mit Lichtblenden und Hardware-Statusmeldungen.
  4. *"THE WUG" Star Wars Story Crawl:* 3D-Perspektiv-Lore mit Fluchtpunkt und Starfield.
  5. *50Hz Gameplay:* 16.16 Fixed-Point Physik, Plattformen, Gegner, Projektile und HUD.
  6. *Credits & Info (`AMI_STATE_CREDITS`):* Flüssiger 50Hz-Vertikal-Scroll mit driftendem Sternenfeld, großen 2-Ton Überschriften und farbkodierten Tags.
- **Interaktive Steuerung im Abspann:**
  - `Pfeil Runter / S`: Schneller scrollen
  - `Pfeil Hoch / W`: Zurück scrollen
  - `Leertaste`: Pause umschalten
  - `Feuer / Enter / ESC`: Zurück zum Hauptmenü

### 5. De-Dimming & High-Contrast Visuals (Amiga & PC)
- **Keine störenden Hintergrundkästen:** Dunkle Rechtecke hinter Texten wurden sowohl auf dem Amiga als auch auf dem PC entfernt.
- **Schwebende Texte:** Menüs, Steuerungsanleitungen und Abspann-Texte schweben direkt über den animierten Hintergründen, dem Copper-Raster und dem Sternenfeld.
- **Vergrößerte 2x Überschriften & Farbkodierung:** Große 2-Ton Titel und dedizierte leuchtende Farben pro Kategorie (`[PROJEKT]`: Gold, `[AUDIO]`: Neonpink, `[CORE]`: Limettengrün, `[AI]`: Cyan, `[HERO]`: Gelb, `[TRIBUTE]`: Orange, `[SCENE]`: Violett).

### 6. Schlanker, kuratierter Soundtrack (~335 KB)
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
| **Story Crawl Tempo** | Pfeiltasten Oben/Unten (W/S) | Pfeiltasten Oben/Unten (W/S) / D-Pad |
| **Story Crawl Skip** | Feuerknopf / Space / Enter / ESC | Space / Enter / STRG / Gamepad A |
| **Pause / Menü** | ESC | ESC / Start-Taste |

---

## 📦 Release-Dateien

- `strudelforce.adf`: 880 KB Amiga 500 Floppy Disk Image (Bootfähig, Volume `StrudelOS`)
- `strudelforce-v0.3.1-amiga.zip`: ADF + `strudelforce.amiga` 68k Binary + Dokumentation
- `strudelforce-v0.3.1-windows-x64.zip`: `strudelforce.exe` (2.5D GUI) + `strudelforce_game.exe` (Terminal)
- `strudelforce-v0.3.1-source.zip`: Vollständiger Quellcode
- `SHA256SUMS.txt`: Kryptografische Prüfsummen aller Release-Archive
