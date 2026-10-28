# STRUDELFORCE — Audio-Modell-Direktive & Master Sample Bank Spezifikation

Dieses Dokument ist die verbindliche Arbeitsanweisung für das nachfolgende KI-Modell in der Cloud-Sandbox, das den interaktiven Original-Soundtrack (OST) im Stil von Chris Huelsbeck (Turrican 2, Apidya) für STRUDELFORCE komponiert und arrangiert.

---

## 1. Hauptaufgabe: Master Sample Bank Extraktion

Erstelle eine einzige, kompakte und vereinheitlichte **Master Sample Bank** (`master_sample_bank.sam` bzw. `.bank`), die von ALLEN Modulen, Leveln und SFX gemeinsam genutzt wird:

1. **Extraktion aus TFMX-Referenzen:**
   - Analysiere die beiliegenden Referenz-Assets in `assets/audio_reference/`:
     - `turrican2_load.sam` & `.tfx` (Chris Huelsbeck Lade-Musik)
     - `turrican2_world1.sam` & `.tfx` (Turrican 2 Desert Theme)
     - `turrican2_world2.sam` & `.tfx` (Turrican 2 Cave Theme)
     - `ref1_wing_commander_patrol.mod` & `ref2_wing_commander_combat.mod`
     - `switch_angel_tracks.strudel` (Moderne Trance-/Bassline-Sketche)
   - Extrahiere die ikonischen Kern-Instrumente: Slap-Bass, DX21-Pluck, Analog-Strings, Supersaw-Cycle, Punchy Kick, Snare, Hi-Hats, Rimshot.

2. **Vereinheitlichung:**
   - Alle Level (Chrome Tempest, The Great Bath, Mechanical Hive) müssen auf dieselbe Sample-Bank im RAM zugreifen.
   - Die Bank darf im Amiga-Speicher maximal **128 KB bis 256 KB** belegen.

---

## 2. Strikte Demoscene-Kompressionsregeln (.adf Budget: 200–400 KB)

Das Gesamtziel für die finale Amiga-Disketten-Lieferung (.adf Disk-Image mit 880 KB Gesamtgröße) ist ein striktes Audio-Budget von **maximal 200 bis 400 KB** für den gesamten Soundtrack inklusive SFX:

### Regel 1: Single-Cycle Waveforms für Bässe und Leads
- **Niemals** lange Bässe oder Flächen absampeln!
- Nutze mathematisch perfekte Single-Cycle Loops (32, 64, 128 oder 256 Bytes Länge) für:
  - Sawtooth-Bass / Sub-Bass
  - PWM-Pulse Waveform
  - Square / Triangle / Sine Waveform
  - FM-Pluck Waveform
- Lebendigkeit und Modulation entstehen ausschließlich zur Laufzeit über:
  - 50Hz TFMX-Makros
  - Dynamische PWM-Verschiebung des Loop-Starts (`loop_start = base + (tick * speed) % 64`)
  - Tracker-Befehle (Arpeggio `0xy`, Pitch Slide `1xx` / `2xx`, Vibrato `4xy`)

### Regel 2: Keine vorgerenderten Effekte in den Samples
- **Verboten:** Vorgerenderter Hall (Reverb), Stereo-Chorus, Echo oder Delay direkt in den PCM-Daten!
- **Demoscene-Lösung:**
  - Nutze tracker-basiertes Noten-Echo (z.B. Note auf Kanal 1, leiseres Echo 3 Ticks später auf Kanal 2 oder in freien Zwischenräumen).
  - Nutze Paulas Hardware-Portamento und CIA-Timer für Vibrato/Flanger.
  - Nutze den Amiga-Hardware-Tiefpassfilter (CIA-A `$BFE001` Pin 1) für Unterwasser-Dämpfung in "The Great Bath".

### Regel 3: 4-Bit ADPCM oder Delta-Kompression für Drums
- Samples für Percussion (Kick, Snare, Hi-Hat, Crash) müssen radikal minimiert werden:
  - 4-Bit ADPCM oder 8-Bit Delta-Differenz-Kompression zur Speicherung.
  - Kicks: Kurzer 8-Bit Transienten-Klick (128 Bytes) gefolgt von gepitchtem Sinus-Decay.
  - Hi-Hats / Shaker: Sehr kurze Noise-Bursts (256–512 Bytes).

### Regel 4: Sampleraten-Budgetierung (Max. 28 kHz)
- Die Amiga Paula Hardware erreicht im PAL-Modus maximal 28867 Hz (Period 124).
- **Regeln für Sampleraten:**
  - Standard-Instrumente & Bässe: **11.025 Hz bis 16.574 Hz** (Standard STRUDELFORCE Paula Rate ist 16574 Hz).
  - Percussion (Snare, Cymbals): Maximal **22.050 Hz bis 28.000 Hz**.
  - Single-Cycle Synth-Waves: Samplerate ist irrelevant, da Paula die Frequenz rein über den DMA-Perioden-Teiler skaliert (`period = PAULA_CLOCK / freq`).

---

## 3. Kompositions-Struktur für die 3 Biome

1. **Biom 1: Chrome Tempest (Turrican 2 Feeling)**
   - Tempo: 125–136 BPM.
   - Stil: Treibender Synth-Pop / Sci-Fi Metal mit schneidendem PWM-Lead, schnellen Sechzehntel-Bässen und Arpeggios.
   - Referenz: *Turrican 2 - World 1 (The Desert Rocks)*.

2. **Biom 2: The Great Bath (Unterwasser-Kathedrale)**
   - Tempo: 110–120 BPM.
   - Stil: Verträumte, tief schwebende D-50 Strings, perlende FM-Glocken.
   - Tribute-Chord beim Öffnen der Schatztruhe:
     `"<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"`
   - Schaltung des analogen Amiga-Hardware-Filters.
   - Referenz: *The Great Bath (Turrican 2 Level 2)*.

3. **Biom 3: Mechanical Hive (Alien-Maschinenwelt)**
   - Tempo: 136–140 BPM.
   - Stil: Acid-Trance, treibender Industrial-Techno, detuned Supersaw-Bässe, Switch Angel Ästhetik.
   - Referenz: *Switch Angel - Extra Slippy / In the Beginning*.

