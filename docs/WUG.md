# WUG — Gegner-Design (Entwurf, Stand 2026-10-29)

Vorlage: `ref/wug-test.png` (Berko 1958, *Wug Test*; lokal, nicht im Repo). Ein kleiner, runder
Vogel auf zwei dünnen Beinen: rundlicher Körper, ein Punktauge, spitzer Schnabel, Schwanz-Zipfel.
Gemischt mit den watschelnden/stehenden Gegnern aus Turrican (Walker mit Telegraph-Phase).

## Verhalten (Zustandsautomat, 50 Hz, Fixed-Point, deterministisch)

| Zustand | Dauer (Ticks) | Verhalten |
|---|---|---|
| `WUG_WADDLE` | 120–240 (RNG) | Watschelt Richtung Spieler (0,5 Einh./Tick), Körper wippt (Sprite-Frame-Wechsel alle 8 Ticks) |
| `WUG_STOP` | 30 | Bleibt stehen, wendet sich dem Spieler zu, „schaut“ |
| `WUG_CHARGE` | 60 (Telegraph) | **Augen glühen rot**, Pulsieren (Palettenindex-Wechsel alle 4 Ticks), Spieler kann noch ausweichen/zerstören |
| `WUG_FIRE` | 40 | **Riesiger Laser** (horizontal, ~12 Einheiten lang, 3 Einh. dick), Schaden 25 + Knockback |
| `WUG_COOLDOWN` | 50 | Erschöpft, Augen dunkel; verwundbar (2× Schaden) → zurück zu `WAIT` |

Regel „wenn man nichts macht“: Wird der Wug in `WUG_CHARGE` getroffen (Treffer-Timer) → **Abbruch**
(Laser fällt aus, Stun 40 Ticks). Wer also aktiv schießt, entschärft ihn.

## Werte (Startwerte, per Playtest balancen)
- HP 4 (stirbt an 4 Pellets / 1 Beam-Sekunde), Score 150, XP 40.
- Erscheint ab Welle 2 (Kampagne: ab Level 2), nie mehr als 2 gleichzeitig.
- Spawn: deterministisch über `core_rng`, wie die anderen Gegner (Oracle-Spiegelung Pflicht).

## Umsetzung (Reihenfolge)
1. `src/strudelforce.h`: `ENEMY_WUG = 3`, `Enemy` um `state`/`state_timer` erweitern (Achtung: `GameState`-Größe, `memset`-Init, Python-Oracle-Layout).
2. `src/game_logic.c`: `wug_update()` in `enemy_update()`, Laser als eigene Gefahr-Box (`laser_*`-Felder existieren für den Spieler → eigene `enemy_beam`-Felder).
3. `verify_core_sim.py`: gleiche Logik spiegeln, Seed-Vergleich Endlos + Kampagne.
4. `test_suite.c`: Zustandsübergänge (WADDLE→STOP→CHARGE→FIRE), Abbruch durch Treffer.
5. Sprites (siehe TODO „Sprites“): 16×16, 4 Frames Watscheln, 1 Frame Stehen, 2 Frames Augen-Glühen, 1 Frame Erschöpft.
6. Audio: Charge = steigender PWM-Ton (Cyber-Paula-Makro), Fire = Laser-Sweep + Sub-Bass-Hit.

## Optik
- Amiga: 16×16 Bob, 4 Plane-Farben (Körper hell/dunkel, Schnabel, Augen) + rotes Glühen = Palettenindex, kein Textur-Look (Gesetz 2); Laser = Blitter-Linie + Copper-Farbflackern.
- PC: gleiche Sprite-Daten, zusätzlich Bloom/Glow der Augen und Laser-Partikel.

