# STRUDELFORCE v0.3.0 – Release Notes

**Release-Datum:** 28. Oktober 2026  
**Binary-Größe:** `1,034,240 Bytes` (< 1.0 MB Budget eingehalten!)  
**Architektur:** C89/C99 Monolith mit 16.16 Fixed-Point Physik & Cyber-Paula TFMX/MOD Hub  
**Verifikation:** 38/38 Integrationstests PASS | Deterministischer Score: 35150 (2500 Ticks)  

---

## 🔥 Highlights der Version 0.3.0

### 1. Original Bren McGuire Sprites & Animationen
- **Amiga Turrican Bren McGuire Sprites:** 14 handgefertigte Frames @ 64x74 Pixel in authentischer 16-Farben-Palette.
- **Flüssiger 13-Frame Run-Cycle:** Getaktet auf 25 FPS (50Hz / 2) mit horizontaler Laufspiegelung.
- **Kultige Helden-Sprungpose:** Dynamische Cover-Art-Pose (`frame_idx = 0`) bei Sprüngen und Luftakrobatik.
- **Aufrechter Stand:** Stabile, geerdete Standpose (`frame_idx = 5`), bei der beide Stiefel fest am Boden bzw. auf Plattformen stehen.
- **Pixel-Perfect Platform Alignment:** Beseitigung des vorherigen 20-Pixel-Versatzes; Spieler, Plattformen, Kisten und Feinde teilen dieselbe Welt-zu-Bildschirm-Projektion.
- **Gyro-Rad (Morph-Ball):** Verwandlung in das Turrican-Kreiselrad mit rotierenden Voxel-Speichen, leuchtendem Fusionskern und Funkenflug am Boden.

### 2. Turrican 2 Steuerungs-Dynamik
- **Kontinuierlicher Dual-Laserstrahl:** Mächtiger Vorwärts-Laser mit Waffenstufen-Skalierung (bis zu 960 Pixel Reichweite).
- **360-Grad Surround Holding Beam:** Frei drehbarer Blitz-Laser (ALT-Taste bzw. Controller X-Taste).
- **Invertierte Vertikal-Achse:** Intuitive Turrican-2-Steuerung – Stick nach oben zielt nach oben, Stick nach unten zielt nach unten.
- **Multi-Device Gamepad-Unterstützung:** Gleichzeitige Abfrage aller 16 DirectInput-Slots (z. B. Sony PS4 DualShock 4 / PS5 DualSense) und XInput (Xbox Controller) mit automatischer Namenserkennung im Menü.
- **Alt+Enter & F11:** Nahtloser Wechsel zwischen 1280x720 Fenstermodus und 16:9 Vollbild mit Letterboxing.
- **Hauptmenü:** "BEENDEN (QUIT)" als Menüpunkt sowie sauberes Beenden per `ESC`.

### 3. Zerstörbare Kisten & Dynamisches Loot-System
- **Turrican / Mario Headbutt:** Kisten können von unten mit dem Kopf angestoßen werden; sie federn nach oben (`bounce_y`), leuchten weiß auf und nehmen Schaden.
- **Voxel-Debris:** Zerstörte Kisten und eliminierte Feinde explodieren in Amiga-Farbpartikel und Trümmerstücke.
- **Loot-Drops:**
  - **Turrican Diamanten (`[♦]`):** +1000 Punkte mit Kristall-Klang und schwebendem Score-Text.
  - **Powerup-Kapseln (`[W]` / `[S]`):** Sofortige Waffen-Aufwertung oder Schild-Aufladung.

### 4. RetroArch-Style Shader Studio (`TAB_GRAPHICS`)
- **Echtzeit-Shader-Profile:**
  1. *Bilinear Smooth* (Amiga Weichzeichner)
  2. *Sony Trinitron* (Aperture Grille Maske)
  3. *Commodore 1084S* (Phosphor-Glow & Farbglanz)
  4. *RetroArch CRT-Easymode* (Kombinierte Röhren-Simulation)
  5. *Arcade CRT Vignette* (Wölbung, Scanlines & chromatische Aberration)
- **Feinjustierung:** Scanline-Intensität (25%, 50%, 75%, 100%), Phosphor-Triaden, Neon-Bloom & 4 Farbmodi (OCS 32, AGA 256, Cyberpunk, Grünphosphor).
- **Hintergrund-Soundtrack:** *New Order – Blue Monday (Chiptune MOD)* läuft durchgehend im Hintergrund, während Einstellungen mit dezenten Soundeffekten bestätigt werden.

### 5. Umfassendes Amiga-In-EXE Musik- und Dokumentations-Archiv
- **Embedded Soundtracks (100% autark in der EXE):**
  - *Chris Huelsbeck – Apidya Meadow* (TFMX + Paula-Samples)
  - *Chris Huelsbeck – Turrican 2 Loader & Fanfare*
  - *Chris Huelsbeck – Turrican 2 Desert & Cave*
  - *Chris Huelsbeck – Software Manager* (22 Subsongs!)
  - *Lost Patrol – tunev1* (15-Sample Soundtracker MOD)
  - *New Order – Blue Monday* (Chiptune MOD)
  - *Wing Commander – Regular Combat* (Action MOD)
  - *Amiga Cracktros* (Fairlight, Skid Row, Paradox)
- **Interaktives Handbuch:** 5-seitiges illustriertes Piloten-Handbuch direkt im Spiel (`H`-Taste oder Menü).
- **Interaktiver Abspann:** Scrolling Starfield Credits mit mehrphasigem Soundtrack-Wechsel und Textdokumentation.

---

## 🛠️ Build & Verifikations-Übersicht

| Komponente | Status | Details |
|---|---|---|
| `strudelforce.exe` | **1,034,240 Bytes** | < 1 MB Budget (14 KB Puffer) |
| `strudelforce_game.exe` | **673,792 Bytes** | 50Hz ANSI/ASCII Terminal Game |
| `test_suite.exe` | **38 / 38 PASS** | Fixed-Point, Determinismus, TFMX, MOD, AST, SFX |
| `verify_core_sim.py` | **PASS (Score 35150)** | Bit-identischer Core-Sim-Verifikationslauf |
| `tools/selftest.py` | **0 FAILS** | Strudel, MOD, TFMX, Levels, Linter |

---

## 🚀 Multi-Platform & v0.4.0 Roadmap
Anleitungen zum Kompilieren auf Linux und Amiga finden sich in [BUILD_GUIDE.md](file:///f:/AgentHome/STRUDELFORCE_Master/BUILD_GUIDE.md).
Für Version 0.4.0 sind native Linux-GUI-Binaries via SDL2 und vorgefertigte Amiga-Floppy-Images (`.adf`) direkt in GitHub Releases eingeplant.
