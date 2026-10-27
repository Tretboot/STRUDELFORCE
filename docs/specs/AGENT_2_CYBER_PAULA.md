# AGENT 2 SPEC: Cyber-Paula Audio Engine (`src/cyber_paula.c`)

## Architektur der Sound-Engine
Unsere Audio-Engine mischt in einem einzigen 44.1kHz (PC) bzw. 28kHz (Amiga Paula) Stream drei Quellen zusammen:
1. **Strudel Clean-Room Engine (`src/strudel_cleanroom.h`):** Evaluiert Live-Coding-Patterns über rationale Zyklus-Zeit (`Fraction`).
2. **TFMX 50Hz Macro Engine (`src/tfmx_macro_synth.h`):** Führt pro Stimme 50-mal pro Sekunde Sound-Macros aus (PWM-Loop-Verschiebung, 50Hz Arpeggios, Pitch-Slides, Volume-Envelopes).
3. **Klassischer `.MOD` Loader (`pocketmod` Integration):** Spielt 4-Kanal Amiga ProTracker Module ab.

## Reaktive Gameplay-Patch-Matrix
Koppeln Sie in `cyber_paula_update_from_game(const GameState *state)` folgende Parameter:
- `active_weapon == WEAPON_LASER` -> Schaltet Bassline-Macro auf `MACRO_PWM_SAW`, erhöht Sub-Bass um +4 dB.
- `active_weapon == WEAPON_BOUNCE` -> Schaltet Bassline-Macro auf `MACRO_FM_PLUCK`, aktiviert 50Hz Oktav-Arpeggio.
- `shield_charging` (0..100%) -> Dreht Biquad Filter-Resonanz `Q` von `0.7` auf `8.5` hoch und beschleunigt die PWM-Modulation.
- `player_hp < 25%` -> Senkt globalen Low-Pass-Filter Cutoff auf `550 Hz`, aktiviert 4-Bit Quantisierungs-Verzerrung auf den Drums.
- `underwater_chest_opened` -> Startet das 7-Stimmen "Great Bath" Tribute-Pattern (Hall-Anteil 45%, weiche D-50 AnalogStrings).
