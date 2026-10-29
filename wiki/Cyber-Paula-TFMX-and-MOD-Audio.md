# Cyber-Paula Audio, TFMX & MOD Engine

Die Sound-Engine von STRUDELFORCE verbindet das Beste aus der goldenen Amiga-Chiptune-Ära mit moderner Sound-Synthese.

---

## 1. Die drei Audio-Säulen

1. **Chris Hülsbeck TFMX (The Final Musicsystem eXtended):**
   - Unterstützt die Amiga-Container `mdat.` und `smpl.` in den Versionen 1.5, Pro und 7V.
   - Dynamische Macro-Abarbeitung bei 50Hz VBlank: Noten-Slides, Arpeggios, Portamento, Volume-Hüllkurven.
   - Enthält Original-Soundtracks von Chris Hülsbeck & Kaiko:
     - *Apidya Meadow* (Titel- & Menü-Intro)
     - *Turrican 2 Desert & Cave*
     - *Turrican 3 World 1 Factory* (inklusive des **Chris-Hülsbeck-Tempo-Fixes**: bei `tempo == 0` wird auf volles Arcade-Tempo `speed = 2` statt 5 geschaltet)
     - *Software Manager Titel* mit 22 Subsongs (u.a. Subsong 21 als Theme des Captain's Logs).
2. **ProTracker 4-Kanal MOD Player:**
   - Bit-genaues 4-Kanal Replay für Standard 31-Sample ProTracker V2.3 Module.
   - Unterstützt Tracker-Effekte: Arpeggio (`0xy`), Portamento (`1xx`/`2xx`/`3xx`), Vibrato (`4xy`), Volume Slide (`Axy`).
   - Taktgenaue Paula-Frequenzen basierend auf dem Amiga-Master-Takt (3.546895 MHz).
   - Klassiker der Demoszene: Fairlight, Skid Row, Razor 1911, Paradox, Airwolf 2 Chiptune, Ocean Loader und New Order "Blue Monday" (KGB Demo Edition).
3. **Cleanroom Strudel Live-Coding Synthesizer:**
   - Vollständiger Clean-Room Mini-Notation Parser nach Strudel.cc / TidalCycles Syntax.
   - Ermöglicht algorithmische Live-Komposition über die ESC-Konsole.
   - **Glitchless Zyklusrand-Swap:** Pattern-Änderungen werden erst an der 16-Step-Zyklusgrenze atomar aktiv (`pending -> active`), um Audio-Aussetzer zu verhindern.

---

## 2. Reaktive Gameplay-Kopplung

Die Audio-Engine reagiert in Echtzeit auf das Spielgeschehen:
- **Waffenwechsel:** Tauscht sofort das Bass-Makro (Spread = Standard Saw, Laser = PWM Saw mit Sub-Bass, Bounce = FM Pluck mit Oktav-Arpeggio).
- **Schild:** Steuert PWM-Modulationsgeschwindigkeit und Resonanz ($Q \approx 0.7 \dots 8.5$).
- **Kritischer Zustand (`HP < 25%`):** Schließt den Low-Pass-Filter auf 550 Hz und aktiviert 4-Bit Quantisierung auf Drums/Bässen.
- **The Great Bath Schatztruhe:** Öffnen der Truhe im Unterwasser-Biom schaltet die Bassline auf das originale Tribute-Pattern um:
  `"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`
  und aktiviert den analogen 7-kHz-Hardwarefilter via CIA-A `$BFE001`.

---

## 3. PC-DSP: Der Soundgodizer

Auf dem PC (Win32 GUI) steht über die Taste `F5` eine optionale DSP-Effektkette zur Verfügung:
1. **Bass-Boost:** Analoger Sub-Bass-Shelving-Filter.
2. **Warm-LP:** Emulation der analogen Amiga-Filter-Wärme.
3. **Tanh-Soft-Drive:** Sanfte Röhren-Sättigung ohne digitales Clipping.
4. **Haas-Wide:** Psychoakustische Stereoverbreiterung über ein 128-Sample Delay auf dem rechten Kanal.

