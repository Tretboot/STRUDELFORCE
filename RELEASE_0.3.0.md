# STRUDELFORCE v0.3.0 – Release Notes

**Release-Datum:** 29. Oktober 2026  
**Binary-Größe:** `963,584 Bytes` gemessen (Feinschliff-Build 2026-10-29); nach dem
Musik-Budget-Tausch (−64 KB Embedded) erwartet ≈ `899 KB` — strikt unter dem 1.0 MB-Budget!  
**Architektur:** C89/C99 Monolith mit 16.16 Fixed-Point Physik & Cyber-Paula TFMX/MOD Hub  
**Verifikation:** 38/38 Integrationstests PASS | Deterministischer Score: 14700 (2500 Ticks, v0.3.0-Physik)  

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
- **Embedded Soundtracks (100% autark in der EXE, 10 Tracks):**
  - *Chris Huelsbeck – Apidya Meadow* (TFMX + Paula-Samples)
  - *Chris Huelsbeck – Turrican 2 Desert & Cave* (Multi-Subsongs)
  - *Chris Huelsbeck – Software Manager Titel* (22 Subsongs!)
  - *Lost Patrol – tunev1* (15-Sample Soundtracker MOD)
  - *New Order – Blue Monday* (Chiptune MOD)
  - *Wing Commander – Commander Office* (Action MOD)
  - *Amiga Cracktros* (Fairlight, Skid Row, Paradox)
- **v0.3.0 Feinschliff (Musik-Budget):** Turrican 2 „Loader & Fanfare“ entfällt (die
  Endlevel-/Sieg-Fanfare übernimmt Wing Commander Commander Office), Software Manager
  „Msx“ durch „Titel“ ersetzt (sauberere Effekte, keine Demo-Störgeräusche) →
  **~64 KB gespart** (524.868 → 460.806 Bytes Embedded).
- **Interaktives Handbuch:** 5-seitiges illustriertes Piloten-Handbuch direkt im Spiel (`H`-Taste oder Menü).
- **Interaktiver Abspann:** Scrolling Starfield Credits mit mehrphasigem Soundtrack-Wechsel und Textdokumentation.

---

## 🛠️ Build & Verifikations-Übersicht

| Komponente | Status | Details |
|---|---|---|
| `strudelforce.exe` | **963,584 Bytes** (≈ 899 KB nach Musik-Swap) | < 1 MB Budget ✅ |
| `strudelforce_game.exe` | **266,240 Bytes** | 50Hz ANSI/ASCII Terminal Game |
| `test_suite.exe` | **38 / 38 PASS** | Fixed-Point, Determinismus, TFMX, MOD, AST, SFX |
| `verify_core_sim.py` | **PASS (Score 14700)** | Bit-identischer Core-Sim-Verifikationslauf (v0.3.0-Physik) |
| `tools/selftest.py` | **0 FAILS** | Strudel, MOD, TFMX, Levels, Linter |

## ✨ v0.3.0 Feinschliff (Final-Pass, 2026-10-29)

- **Entspannteres Gameplay-Tempo:** Beschleunigung/Geschwindigkeit −25%, flachere Sprünge,
  weicheres Ausrollen (Friction 0.80) — Oracle-geprüft (`verify_core_sim.py`).
- **Controller-Konfiguration (Menü-Punkt „CONTROLLER“):** 3 Belegungs-Profile
  (Modern / Competition Pro / Classic Platformer), Live-Belegungstabelle pro Gerätetyp,
  Live-Button-Testfeld, invertierbarer 360°-Laser — per Pad UND Tastatur steuerbar.
- **Top-Bar-Mini-Menü im Spiel** (Kampagne & Endlos) wiederhergestellt; F5-Soundgodizer-Status
  rechtsbündig verankert (kein Overlap mehr mit dem [F9]-Tab).
- **Credits:** Auto-Scroll mit 2–3 Song-Loops pro Track, klares blinkendes PAUSIERT-Banner,
  blaue Seiten-Streifen entfernt, LUT-basiertes Panel-Dimmen (Performance).
- **Performance:** LUT-Integer-Dimmen ersetzt Float-Pixel-Schleifen in
  Game-Over-/Sieg-Overlay & Credits-Panel (~4x schneller).
- **Musik-Budget:** −64 KB Embedded (T2 Load raus, Software Manager Titel statt Msx,
  Wing Commander Commander Office statt Regular Combat) — Details in Abschnitt 5.

---

## 🚀 Multi-Platform & v0.4.0 Roadmap
Anleitungen zum Kompilieren auf Linux und Amiga finden sich in [BUILD_GUIDE.md](file:///f:/AgentHome/STRUDELFORCE_Master/BUILD_GUIDE.md).
Für Version 0.4.0 sind native Linux-GUI-Binaries via SDL2 und vorgefertigte Amiga-Floppy-Images (`.adf`) direkt in GitHub Releases eingeplant.

