# AGENT 1 SPEC: Amiga-First C89 Gameplay Core (`src/core_sim.c`)

## Strikte Regeln für Agent 1
1. **Kein `float` oder `double`!** Nutze ausschließlich `fix16_t` (`int32_t` im 16.16 Fixed-Point Format: `1 << 16` entspricht `1.0`).
2. **Keine dynamische Speicherallokation (`malloc`) während der Gameplay-Schleife!** Alle Entities, Schüsse und Partikel liegen in statischen Ringbuffern (damit der Code direkt in 512 KB Chip-RAM auf dem Amiga 500 passt).
3. **50Hz PAL-Tick:** Die Funktion `core_tick(GameState *state, uint16_t input_mask)` berechnet exakt 1/50 Sekunde.

## Waffen-Spezifikation (Turrican 1/2 + Bounce 2.0 + Survivors)
- **WEAPON_MULTIPLE (Spread):** 5 Upgrade-Stufen (1 bis 7 Projektile im Fächer).
- **WEAPON_LASER (Der Turrican-2-Balken):**
  - Stufe 1: Kurzer schmaler Impuls (`thickness = 2`, `length = 24`).
  - Stufe 2..5: Wächst bei jedem gefundenen *Laser*-Symbol kontinuierlich in der Breite (`thickness = 4, 8, 14, 22` Voxel) und schneidet ab Stufe 3 durch Wände und mehrere Gegner gleichzeitig!
- **WEAPON_BOUNCE_2_0 (Geometric Prism):**
  - Stufe 1: 45° Abprallwinkel.
  - Stufe 2: Feuert 0° (geradeaus) + 45° diagonal.
  - Stufe 3: Feuert zusätzlich 90° (senkrecht nach oben und unten für volle Raumkontrolle).
  - Stufe 4: Wandtreffer spalten das Projektil in 2 Sekundärwinkel.
  - Stufe 5: Smart-Ricochet (Winkel nach Wandkontakt zeigt Richtung nächster aktiver Gegner-Koordinate).
- **SURROUND_BEAM (360° Haltestrahl):** Wenn `FIRE_SECONDARY` gehalten wird, friert `player.vx` ein und `beam_angle` (0..255 für 360°) lässt sich frei im Kreis drehen.
- **MORPH_WHEEL & POWER_LINE:** Morph-Rad verkleinert die Hitbox auf `8x8` (unverwundbar gegen Standard-Schüsse).
- **Vampire-Survivors-Layer:** Kristalle erhöhen `player.xp`. Bei Level-Up Auswahl aus passiven Buffs (*Laser-Breite +20%*, *Schild-Resonanz*, *Auto-Fire Begleitdrohne*).
