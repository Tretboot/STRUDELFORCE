# Amiga 500 Bare-Metal & ADF Floppy Pipeline

STRUDELFORCE ist von Grund auf so konstruiert, dass es nativ auf einem unbeschleunigten **Commodore Amiga 500 (Kickstart 1.3, 512 KB Chip-RAM, OCS)** von Diskette bootet und flüssig bei 50Hz PAL läuft.

---

## 1. Amiga 500 Hardware-Budget & Layout

- **CPU:** Motorola 68000 @ 7.09379 MHz (PAL).
- **Speicher:** 512 KB Chip-RAM (Gesamtbudget für Code, Bitplanes, Copper-Listen, Sprites und Audio-DMA).
- **Grafik (OCS Agnus & Denise):**
  - Auflösung: 320x256 Pixel (PAL Low-Res).
  - 5 Bitplanes (32 Farben gleichzeitig aus 4096 Farben).
  - Copper: Hardware-Koprozessor für rasterstrahlsynchrone Farbverläufe (Himmel-Bänder) und Split-Screens ohne CPU-Last.
  - Blitter: Hardware-Beschleunigung für schnelles Zeichnen von Bobs (Cookie-Cut Blits).
- **Audio (Paula 8364):**
  - 4 unabhängige Hardware-DMA-Kanäle mit 8-Bit-PCM.
  - Stereo-Panning fest verdrahtet (Kanäle 0 & 3 links, 1 & 2 rechts).
  - CIA-A `$BFE001` Pin 1 steuert den analogen 7-kHz-Hardware-Tiefpassfilter.
- **Eingabe (CIA-A & CIA-B):**
  - Port 2: Standard 9-Pin Atari/Amiga Joystick.
  - Tastatur: Serieller Empfang über CIA-A SDR (`$BFEC01`) mit Quittierungsimpuls über CRA (`$BFEE01`).

---

## 2. Die Amiga State Machine (`amiga/main.c`)

Die Bare-Metal-Architektur in `amiga/main.c` implementiert eine vollständige Amiga-State-Machine:

1. **`AMI_STATE_INTRO` (Cracktro):**
   - 3D-Wireframe-Vektorwürfel, der in Echtzeit rotiert.
   - 100% Fixed-Point Trigonometrie über Look-up-Tabellen (`s_sin32`/`cos_get`).
   - Copper-Horizont-Farbverlauf und sinusförmig schwingender Text-Scroller.
2. **`AMI_STATE_MENU` (Turrican-2 Menü):**
   - Großer 2-Ton-Titelbanner `STRUDELFORCE` (16x16 vergrößerte Pixel) mit dunkelrotem Schlagschatten.
   - Menü-Auswahl: Kampagne (5 Welten), Endlos Roguelike, Steuerung, Credits.
3. **`AMI_STATE_LOADING` (Amiga-Block-Ladebalken):**
   - Klassischer AmigaOS Ladebalken mit wandernden Lichtblenden und Hardware-Statusmeldungen.
4. **`AMI_STATE_STORY` (Star Wars 3D Story Crawl):**
   - Perspektivischer Text-Crawl vor pechschwarzem Weltraum-Sternenfeld.
5. **`AMI_STATE_GAME` (50Hz PAL Simulation):**
   - 16.16 Fixed-Point Physik, Plattformen, Ruinensäulen, Gegner, Waffen, Kristalle und HUD.
6. **`AMI_STATE_CREDITS`:**
   - Scrolling Starfield mit Demoszene-Grüßen und Danksagungen.

---

## 3. Paula DMA Audio (ThePlayer 6.1a)

- Der kompakte Amiga-Replayer `player610.6.no_cia.bin` und das Musikmodul `testmod.p61` sind direkt über `#embed` in das Binary eingebunden.
- Das Modul wird direkt im Chip-RAM initialisiert.
- `p61_music()` wird strikt synchron im VBlank-Interrupt (`sys_waitvbl()`) aufgerufen.
- Das Audiosystem erzeugt 0% CPU-Last während des Gameplays und nutzt alle 4 Paula-DMA-Hardwarekanäle.

---

## 4. Die ADF Floppy-Pipeline (`tools/build_amiga_adf.py`)

Das Skript `tools/build_amiga_adf.py` erzeugt ein vollständiges, bit-genaues 880 KB Amiga Floppy Disk Image:

- **Dateisystem:** Amiga Old File System (OFS, `DOS0:ofs`), zwingend erforderlich für Kickstart 1.3 Boot-Floppies.
- **Bootblock:** Kickstart-1.x-kompatibler Bootblock (`boot1x`), der das Diskettenlaufwerk übernimmt.
- **Volume-Name:** `StrudelOS`.
- **Startup-Sequence:** `s/startup-sequence` ruft direkt `strudelforce.amiga` auf.
- **Größe:** Exakt 901.120 Bytes (80 Spuren, 2 Seiten, 11 Sektoren à 512 Bytes).

### Ausführen & Testen mit WinUAE
Mit `tools/start_winuae.bat` wird der Amiga 500 Emulator mit authentischen mechanischen Laufwerksgeräuschen (Floppy Sound Plugin) gestartet:
```cmd
tools\start_winuae.bat
```

